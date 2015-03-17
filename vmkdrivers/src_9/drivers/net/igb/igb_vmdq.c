/*******************************************************************************

  Intel(R) Gigabit Ethernet Linux driver
  Copyright(c) 2007-2013 Intel Corporation.

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

#include "igb.h"
#include "igb_vmdq.h"
#include <linux/if_vlan.h>

#ifdef CONFIG_IGB_VMDQ_NETDEV
int igb_vmdq_open(struct net_device *dev)
{
	struct igb_vmdq_adapter *vadapter = netdev_priv(dev);
	struct igb_adapter *adapter = vadapter->real_adapter;
	struct net_device *main_netdev = adapter->netdev;
	int hw_queue = vadapter->rx_ring->queue_index +
		       adapter->vfs_allocated_count;

	if (test_bit(__IGB_DOWN, &adapter->state)) {
		DPRINTK(DRV, WARNING,
			"Open %s before opening this device.\n",
			main_netdev->name);
		return -EAGAIN;
	}
	netif_carrier_off(dev);
	vadapter->tx_ring->vmdq_netdev = dev;
	vadapter->rx_ring->vmdq_netdev = dev;
	if (is_valid_ether_addr(dev->dev_addr)) {
		igb_del_mac_filter(adapter, dev->dev_addr, hw_queue);
		igb_add_mac_filter(adapter, dev->dev_addr, hw_queue);
	}
	netif_carrier_on(dev);
	return 0;
}

int igb_vmdq_close(struct net_device *dev)
{
	struct igb_vmdq_adapter *vadapter = netdev_priv(dev);
	struct igb_adapter *adapter = vadapter->real_adapter;
	int hw_queue = vadapter->rx_ring->queue_index +
		       adapter->vfs_allocated_count;

	netif_carrier_off(dev);
	igb_del_mac_filter(adapter, dev->dev_addr, hw_queue);

	vadapter->tx_ring->vmdq_netdev = NULL;
	vadapter->rx_ring->vmdq_netdev = NULL;
	return 0;
}

netdev_tx_t igb_vmdq_xmit_frame(struct sk_buff *skb, struct net_device *dev)
{
	struct igb_vmdq_adapter *vadapter = netdev_priv(dev);

	return igb_xmit_frame_ring(skb, vadapter->tx_ring);
}

struct net_device_stats *igb_vmdq_get_stats(struct net_device *dev)
{
	struct igb_vmdq_adapter *vadapter = netdev_priv(dev);
        struct igb_adapter *adapter = vadapter->real_adapter;
        struct e1000_hw *hw = &adapter->hw;
	int hw_queue = vadapter->rx_ring->queue_index +
		       adapter->vfs_allocated_count;

	vadapter->net_stats.rx_packets +=
			E1000_READ_REG(hw, E1000_PFVFGPRC(hw_queue));
	E1000_WRITE_REG(hw, E1000_PFVFGPRC(hw_queue), 0);
        vadapter->net_stats.tx_packets +=
			E1000_READ_REG(hw, E1000_PFVFGPTC(hw_queue));
        E1000_WRITE_REG(hw, E1000_PFVFGPTC(hw_queue), 0);
        vadapter->net_stats.rx_bytes +=
			E1000_READ_REG(hw, E1000_PFVFGORC(hw_queue));
        E1000_WRITE_REG(hw, E1000_PFVFGORC(hw_queue), 0);
        vadapter->net_stats.tx_bytes +=
			E1000_READ_REG(hw, E1000_PFVFGOTC(hw_queue));
        E1000_WRITE_REG(hw, E1000_PFVFGOTC(hw_queue), 0);
        vadapter->net_stats.multicast +=
			E1000_READ_REG(hw, E1000_PFVFMPRC(hw_queue));
        E1000_WRITE_REG(hw, E1000_PFVFMPRC(hw_queue), 0);
	/* only return the current stats */
	return &vadapter->net_stats;
}

/**
 * igb_write_vm_addr_list - write unicast addresses to RAR table
 * @netdev: network interface device structure
 *
 * Writes unicast address list to the RAR table.
 * Returns: -ENOMEM on failure/insufficient address space
 *                0 on no addresses written
 *                X on writing X addresses to the RAR table
 **/
static int igb_write_vm_addr_list(struct net_device *netdev)
{
	struct igb_vmdq_adapter *vadapter = netdev_priv(netdev);
        struct igb_adapter *adapter = vadapter->real_adapter;
	int count = 0;
	int hw_queue = vadapter->rx_ring->queue_index +
		       adapter->vfs_allocated_count;

	/* return ENOMEM indicating insufficient memory for addresses */
	if (netdev_uc_count(netdev) > igb_available_rars(adapter))
		return -ENOMEM;

	if (!netdev_uc_empty(netdev)) {
#ifdef NETDEV_HW_ADDR_T_UNICAST
		struct netdev_hw_addr *ha;
#else
		struct dev_mc_list *ha;
#endif
		netdev_for_each_uc_addr(ha, netdev) {
#ifdef NETDEV_HW_ADDR_T_UNICAST
			igb_del_mac_filter(adapter, ha->addr, hw_queue);
			igb_add_mac_filter(adapter, ha->addr, hw_queue);
#else
			igb_del_mac_filter(adapter, ha->da_addr, hw_queue);
			igb_add_mac_filter(adapter, ha->da_addr, hw_queue);
#endif
			count++;
		}
	}
	return count;
}


#define E1000_VMOLR_UPE		0x20000000 /* Unicast promiscuous mode */
void igb_vmdq_set_rx_mode(struct net_device *dev)
{
	struct igb_vmdq_adapter *vadapter = netdev_priv(dev);
        struct igb_adapter *adapter = vadapter->real_adapter;
        struct e1000_hw *hw = &adapter->hw;
	u32 vmolr, rctl;
	int hw_queue = vadapter->rx_ring->queue_index +
		       adapter->vfs_allocated_count;

	/* Check for Promiscuous and All Multicast modes */
	vmolr = E1000_READ_REG(hw, E1000_VMOLR(hw_queue));

	/* clear the affected bits */
	vmolr &= ~(E1000_VMOLR_UPE | E1000_VMOLR_MPME |
		   E1000_VMOLR_ROPE | E1000_VMOLR_ROMPE);

	if (dev->flags & IFF_PROMISC) {
		vmolr |= E1000_VMOLR_UPE;
		rctl = E1000_READ_REG(hw, E1000_RCTL);
		rctl |= E1000_RCTL_UPE;
		E1000_WRITE_REG(hw, E1000_RCTL, rctl);
	} else {
		rctl = E1000_READ_REG(hw, E1000_RCTL);
		rctl &= ~E1000_RCTL_UPE;
		E1000_WRITE_REG(hw, E1000_RCTL, rctl);
		if (dev->flags & IFF_ALLMULTI) {
			vmolr |= E1000_VMOLR_MPME;
		} else {
			/*
			 * Write addresses to the MTA, if the attempt fails
			 * then we should just turn on promiscous mode so
			 * that we can at least receive multicast traffic
			 */
			if (igb_write_mc_addr_list(adapter->netdev) != 0)
				vmolr |= E1000_VMOLR_ROMPE;
		}
#ifdef HAVE_SET_RX_MODE
		/*
		 * Write addresses to available RAR registers, if there is not
		 * sufficient space to store all the addresses then enable
		 * unicast promiscous mode
		 */
		if (igb_write_vm_addr_list(dev) < 0)
			vmolr |= E1000_VMOLR_UPE;
#endif
	}
	E1000_WRITE_REG(hw, E1000_VMOLR(hw_queue), vmolr);

	return;
}

int igb_vmdq_set_mac(struct net_device *dev, void *p)
{
	struct sockaddr *addr = p;
	struct igb_vmdq_adapter *vadapter = netdev_priv(dev);
        struct igb_adapter *adapter = vadapter->real_adapter;
	int hw_queue = vadapter->rx_ring->queue_index +
		       adapter->vfs_allocated_count;

	igb_del_mac_filter(adapter, dev->dev_addr, hw_queue);
	memcpy(dev->dev_addr, addr->sa_data, dev->addr_len);
	return igb_add_mac_filter(adapter, dev->dev_addr, hw_queue);
}

int igb_vmdq_change_mtu(struct net_device *dev, int new_mtu)
{
	struct igb_vmdq_adapter *vadapter = netdev_priv(dev);
	struct igb_adapter *adapter = vadapter->real_adapter;

	if (adapter->netdev->mtu < new_mtu) {
		DPRINTK(PROBE, INFO,
			"Set MTU on %s to >= %d "
			"before changing MTU on %s\n",
			adapter->netdev->name, new_mtu, dev->name);
		return -EINVAL;
	}
	dev->mtu = new_mtu;
	return 0;
}

void igb_vmdq_tx_timeout(struct net_device *dev)
{
	return;
}

void igb_vmdq_vlan_rx_register(struct net_device *dev, struct vlan_group *grp)
{
	struct igb_vmdq_adapter *vadapter = netdev_priv(dev);
	struct igb_adapter *adapter = vadapter->real_adapter;
	struct e1000_hw *hw = &adapter->hw;
	int hw_queue = vadapter->rx_ring->queue_index +
		       adapter->vfs_allocated_count;

	vadapter->vlgrp = grp;

	igb_enable_vlan_tags(adapter);
	E1000_WRITE_REG(hw, E1000_VMVIR(hw_queue), 0);

	return;
}
void igb_vmdq_vlan_rx_add_vid(struct net_device *dev, unsigned short vid)
{
	struct igb_vmdq_adapter *vadapter = netdev_priv(dev);
	struct igb_adapter *adapter = vadapter->real_adapter;
#ifndef HAVE_NETDEV_VLAN_FEATURES
	struct net_device *v_netdev;
#endif
	int hw_queue = vadapter->rx_ring->queue_index +
		       adapter->vfs_allocated_count;

	/* attempt to add filter to vlvf array */
	igb_vlvf_set(adapter, vid, TRUE, hw_queue);

#ifndef HAVE_NETDEV_VLAN_FEATURES

	/* Copy feature flags from netdev to the vlan netdev for this vid.
	 * This allows things like TSO to bubble down to our vlan device.
	 */
	v_netdev = vlan_group_get_device(vadapter->vlgrp, vid);
	v_netdev->features |= adapter->netdev->features;
	vlan_group_set_device(vadapter->vlgrp, vid, v_netdev);
#endif

	return;
}
void igb_vmdq_vlan_rx_kill_vid(struct net_device *dev, unsigned short vid)
{
	struct igb_vmdq_adapter *vadapter = netdev_priv(dev);
	struct igb_adapter *adapter = vadapter->real_adapter;
	int hw_queue = vadapter->rx_ring->queue_index +
		       adapter->vfs_allocated_count;

	vlan_group_set_device(vadapter->vlgrp, vid, NULL);
	/* remove vlan from VLVF table array */
	igb_vlvf_set(adapter, vid, FALSE, hw_queue);


	return;
}

static int igb_vmdq_get_settings(struct net_device *netdev,
				   struct ethtool_cmd *ecmd)
{
	struct igb_vmdq_adapter *vadapter = netdev_priv(netdev);
	struct igb_adapter *adapter = vadapter->real_adapter;
	struct e1000_hw *hw = &adapter->hw;
	u32 status;

	if (hw->phy.media_type == e1000_media_type_copper) {

		ecmd->supported = (SUPPORTED_10baseT_Half |
				   SUPPORTED_10baseT_Full |
				   SUPPORTED_100baseT_Half |
				   SUPPORTED_100baseT_Full |
				   SUPPORTED_1000baseT_Full|
				   SUPPORTED_Autoneg |
				   SUPPORTED_TP);
		ecmd->advertising = ADVERTISED_TP;

		if (hw->mac.autoneg == 1) {
			ecmd->advertising |= ADVERTISED_Autoneg;
			/* the e1000 autoneg seems to match ethtool nicely */
			ecmd->advertising |= hw->phy.autoneg_advertised;
		}

		ecmd->port = PORT_TP;
		ecmd->phy_address = hw->phy.addr;
	} else {
		ecmd->supported   = (SUPPORTED_1000baseT_Full |
				     SUPPORTED_FIBRE |
				     SUPPORTED_Autoneg);

		ecmd->advertising = (ADVERTISED_1000baseT_Full |
				     ADVERTISED_FIBRE |
				     ADVERTISED_Autoneg);

		ecmd->port = PORT_FIBRE;
	}

	ecmd->transceiver = XCVR_INTERNAL;

	status = E1000_READ_REG(hw, E1000_STATUS);

	if (status & E1000_STATUS_LU) {

		if ((status & E1000_STATUS_SPEED_1000) ||
		    hw->phy.media_type != e1000_media_type_copper)
			ecmd->speed = SPEED_1000;
		else if (status & E1000_STATUS_SPEED_100)
			ecmd->speed = SPEED_100;
		else
			ecmd->speed = SPEED_10;

		if ((status & E1000_STATUS_FD) ||
		    hw->phy.media_type != e1000_media_type_copper)
			ecmd->duplex = DUPLEX_FULL;
		else
			ecmd->duplex = DUPLEX_HALF;
	} else {
		ecmd->speed = -1;
		ecmd->duplex = -1;
	}

	ecmd->autoneg = hw->mac.autoneg ? AUTONEG_ENABLE : AUTONEG_DISABLE;
	return 0;
}


static u32 igb_vmdq_get_msglevel(struct net_device *netdev)
{
	struct igb_vmdq_adapter *vadapter = netdev_priv(netdev);
	struct igb_adapter *adapter = vadapter->real_adapter;
	return adapter->msg_enable;
}

static void igb_vmdq_get_drvinfo(struct net_device *netdev,
				   struct ethtool_drvinfo *drvinfo)
{
	struct igb_vmdq_adapter *vadapter = netdev_priv(netdev);
	struct igb_adapter *adapter = vadapter->real_adapter;
	struct net_device *main_netdev = adapter->netdev;

	strncpy(drvinfo->driver, igb_driver_name, 32);
	strncpy(drvinfo->version, igb_driver_version, 32);

	strncpy(drvinfo->fw_version, "N/A", 4);
	snprintf(drvinfo->bus_info, 32, "%s VMDQ %d", main_netdev->name,
		 vadapter->rx_ring->queue_index);
	drvinfo->n_stats = 0;
	drvinfo->testinfo_len = 0;
	drvinfo->regdump_len = 0;
}

static void igb_vmdq_get_ringparam(struct net_device *netdev,
				     struct ethtool_ringparam *ring)
{
	struct igb_vmdq_adapter *vadapter = netdev_priv(netdev);

	struct igb_ring *tx_ring = vadapter->tx_ring;
	struct igb_ring *rx_ring = vadapter->rx_ring;

	ring->rx_max_pending = IGB_MAX_RXD;
	ring->tx_max_pending = IGB_MAX_TXD;
	ring->rx_mini_max_pending = 0;
	ring->rx_jumbo_max_pending = 0;
	ring->rx_pending = rx_ring->count;
	ring->tx_pending = tx_ring->count;
	ring->rx_mini_pending = 0;
	ring->rx_jumbo_pending = 0;
}
static u32 igb_vmdq_get_rx_csum(struct net_device *netdev)
{
	struct igb_vmdq_adapter *vadapter = netdev_priv(netdev);
	struct igb_adapter *adapter = vadapter->real_adapter;

	return test_bit(IGB_RING_FLAG_RX_CSUM, &adapter->rx_ring[0]->flags);
}


static struct ethtool_ops igb_vmdq_ethtool_ops = {
	.get_settings           = igb_vmdq_get_settings,
	.get_drvinfo            = igb_vmdq_get_drvinfo,
	.get_link               = ethtool_op_get_link,
	.get_ringparam          = igb_vmdq_get_ringparam,
	.get_rx_csum            = igb_vmdq_get_rx_csum,
	.get_tx_csum            = ethtool_op_get_tx_csum,
	.get_sg                 = ethtool_op_get_sg,
	.set_sg                 = ethtool_op_set_sg,
	.get_msglevel           = igb_vmdq_get_msglevel,
#ifdef NETIF_F_TSO
	.get_tso                = ethtool_op_get_tso,
#endif
#ifdef HAVE_ETHTOOL_GET_PERM_ADDR
	.get_perm_addr          = ethtool_op_get_perm_addr,
#endif
};

void igb_vmdq_set_ethtool_ops(struct net_device *netdev)
{
	SET_ETHTOOL_OPS(netdev, &igb_vmdq_ethtool_ops);
}


#endif /* CONFIG_IGB_VMDQ_NETDEV */

#ifdef __VMKNETDDI_QUEUEOPS__
int igb_set_rxqueue_macfilter(struct net_device *netdev, int queue,
                              u8 *mac_addr)
{
	int err = 0;
	struct igb_adapter *adapter = netdev_priv(netdev);
	struct igb_ring *rx_ring = adapter->rx_ring[queue];

	if ((queue < 0) || (queue >= adapter->num_rx_queues)) {
		DPRINTK(DRV, ERR, "Invalid RX Queue %u specified\n", queue);
		return -EADDRNOTAVAIL;
	}


	/* Note: Broadcast address is used to disable the MAC filter*/
	if (!is_valid_ether_addr(mac_addr)) {

		/* Clear ring addr */
		DPRINTK(DRV, DEBUG,
			"disabling MAC filter on RX Queue[%d]\n", queue);
		igb_del_mac_filter(adapter, rx_ring->mac_addr, queue); 
		memset(rx_ring->mac_addr, 0xFF, NODE_ADDRESS_SIZE);

		return -EADDRNOTAVAIL;
	}

	DPRINTK(DRV, DEBUG,
		"enabling MAC filter [[0x%X:0x%X:0x%X:0x%X:0x%X:0x%X]] "
		"on RX Queue[%d]\n", mac_addr[0], mac_addr[1], mac_addr[2],
		mac_addr[3], mac_addr[4], mac_addr[5], queue);

	/* Store in ring */
	memcpy(rx_ring->mac_addr, mac_addr, NODE_ADDRESS_SIZE);

	igb_add_mac_filter(adapter, rx_ring->mac_addr, queue);

	return err;
}

static int igb_get_netqueue_features(vmknetddi_queueop_get_features_args_t *args)
{
	args->features = VMKNETDDI_QUEUEOPS_FEATURE_RXQUEUES |
	                 VMKNETDDI_QUEUEOPS_FEATURE_TXQUEUES;
	return VMKNETDDI_QUEUEOPS_OK;
}

static int igb_get_queue_count(vmknetddi_queueop_get_queue_count_args_t *args)
{
	struct net_device *netdev = args->netdev;
	struct igb_adapter *adapter = netdev_priv(netdev);

	if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_TX) {
		args->count = adapter->num_tx_queues - 1;
	} else if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX) {
		args->count = adapter->num_rx_queues - 1;
	} else {
		DPRINTK(DRV, ERR, "invalid queue type\n");
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	return VMKNETDDI_QUEUEOPS_OK;
}

static int igb_get_filter_count(vmknetddi_queueop_get_filter_count_args_t *args)
{
	args->count = 1;
	return VMKNETDDI_QUEUEOPS_OK;
}

static int igb_alloc_rx_queue(struct net_device *netdev,
                     vmknetddi_queueops_queueid_t *p_qid,
                     struct napi_struct **napi_p)
{
	struct igb_adapter *adapter = netdev_priv(netdev);

	if (adapter->n_rx_queues_allocated >= adapter->num_rx_queues) {
		DPRINTK(DRV, ERR, "igb_alloc_rx_queue: no free rx queues\n");
		return VMKNETDDI_QUEUEOPS_ERR;
        } else {
		int i;
		for (i = 1; i < adapter->num_rx_queues; i++) {
			struct igb_ring *ring = adapter->rx_ring[i];
			if (!ring->allocated) {
				ring->allocated = TRUE;
				*p_qid = VMKNETDDI_QUEUEOPS_MK_RX_QUEUEID(i);
				DPRINTK(DRV, DEBUG,
				        "allocated VMDQ rx queue=%d\n", i);
				*napi_p = &ring->q_vector->napi;
				adapter->n_rx_queues_allocated++;
				return VMKNETDDI_QUEUEOPS_OK;
			}
		}
		DPRINTK(DRV, ERR, "no free rx queues found!\n");
		return VMKNETDDI_QUEUEOPS_ERR;
	}
}

static int igb_alloc_tx_queue(struct net_device *netdev,
                      vmknetddi_queueops_queueid_t *p_qid,
		      u16 *queue_mapping)
{
	struct igb_adapter *adapter = netdev_priv(netdev);

	if (adapter->n_tx_queues_allocated >= adapter->num_tx_queues) {
		DPRINTK(DRV, ERR, "igb_alloc_tx_queue: no free tx queues\n");
		return VMKNETDDI_QUEUEOPS_ERR;
        } else {
		int i;
		for (i = 1; i < adapter->num_tx_queues; i++) {
			struct igb_ring *ring = adapter->tx_ring[i];
			if (!ring->allocated) {
				ring->allocated = TRUE;
				*p_qid = VMKNETDDI_QUEUEOPS_MK_TX_QUEUEID(i);
				*queue_mapping = i;
				DPRINTK(DRV, DEBUG,
					"allocated VMDQ tx queue=%d\n", i);
				adapter->n_tx_queues_allocated++;
				return VMKNETDDI_QUEUEOPS_OK;
			}
		}
		DPRINTK(DRV, ERR, "no free tx queues found!\n");
		return VMKNETDDI_QUEUEOPS_ERR;
	}
}

static int igb_alloc_queue(vmknetddi_queueop_alloc_queue_args_t *args)
{
	struct net_device *netdev = args->netdev;
	struct igb_adapter *adapter = netdev_priv(netdev);

	if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_TX) {
		return igb_alloc_tx_queue(args->netdev, &args->queueid,
		                          &args->queue_mapping);
	} else if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX) {
		return igb_alloc_rx_queue(args->netdev, &args->queueid,
		                          &args->napi);
	} else {
		DPRINTK(DRV, ERR, "invalid queue type\n");
		return VMKNETDDI_QUEUEOPS_ERR;
	}
}

static int
igb_free_rx_queue(struct net_device *netdev,
		     vmknetddi_queueops_queueid_t qid)
{
	struct igb_adapter *adapter = netdev_priv(netdev);
	u16 queue = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(qid);
	struct igb_ring *ring = adapter->rx_ring[queue];

	if (!ring->allocated) {
		DPRINTK(DRV, ERR, "rx queue %d not allocated\n", queue);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	DPRINTK(DRV, DEBUG, "freed VMDQ rx queue=%d\n", queue);
	ring->allocated = FALSE;
	adapter->n_rx_queues_allocated--;
	return VMKNETDDI_QUEUEOPS_OK;
}

static int
igb_free_tx_queue(struct net_device *netdev,
		     vmknetddi_queueops_queueid_t qid)
{
	struct igb_adapter *adapter = netdev_priv(netdev);
	u16 queue = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(qid);

	if (!adapter->tx_ring[queue]->allocated) {
		DPRINTK(DRV, ERR, "tx queue %d not allocated\n", queue);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	DPRINTK(DRV, DEBUG, "freed VMDQ tx queue=%d\n", queue);
	adapter->tx_ring[queue]->allocated = FALSE;
	adapter->n_tx_queues_allocated--;
	return VMKNETDDI_QUEUEOPS_OK;
}

static int
igb_free_queue(vmknetddi_queueop_free_queue_args_t *args)
{
	struct net_device *netdev = args->netdev;
	struct igb_adapter *adapter = netdev_priv(netdev);

	if (VMKNETDDI_QUEUEOPS_IS_TX_QUEUEID(args->queueid)) {
		return igb_free_tx_queue(netdev, args->queueid);
	} else if (VMKNETDDI_QUEUEOPS_IS_RX_QUEUEID(args->queueid)) {
		return igb_free_rx_queue(netdev, args->queueid);
	} else {
		DPRINTK(DRV, ERR, "invalid queue type\n");
		return VMKNETDDI_QUEUEOPS_ERR;
	}
}

static int
igb_get_queue_vector(vmknetddi_queueop_get_queue_vector_args_t *args)
{
	int qid;
	struct net_device *netdev = args->netdev;
	struct igb_adapter *adapter = netdev_priv(netdev);
	/* Assuming RX queue id's are received */
	qid = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args->queueid);
	args->vector = adapter->msix_entries[qid].vector;

	return VMKNETDDI_QUEUEOPS_OK;
}

static int
igb_get_default_queue(vmknetddi_queueop_get_default_queue_args_t *args)
{
	struct net_device *netdev = args->netdev;
	struct igb_adapter *adapter = netdev_priv(netdev);

	if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX) {
		args->napi = &adapter->rx_ring[0]->q_vector->napi;
		args->queueid = VMKNETDDI_QUEUEOPS_MK_RX_QUEUEID(0);
		return VMKNETDDI_QUEUEOPS_OK;
	} else if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_TX) {
		args->queueid = VMKNETDDI_QUEUEOPS_MK_TX_QUEUEID(0);
		return VMKNETDDI_QUEUEOPS_OK;
	} else {
		return VMKNETDDI_QUEUEOPS_ERR;
	}
}

static int
igb_apply_rx_filter(vmknetddi_queueop_apply_rx_filter_args_t *args)
{
	int rval;
	u8 *macaddr;
	u16 queue = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args->queueid);
	struct igb_adapter *adapter = netdev_priv(args->netdev);

	if (!VMKNETDDI_QUEUEOPS_IS_RX_QUEUEID(args->queueid)) {
		DPRINTK(DRV, ERR, "not an rx queue 0x%x\n",
			args->queueid);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	if (vmknetddi_queueops_get_filter_class(&args->filter)
					!= VMKNETDDI_QUEUEOPS_FILTER_MACADDR) {
		DPRINTK(DRV, ERR, "only mac filters supported\n");
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	if (!adapter->rx_ring[queue]->allocated) {
		DPRINTK(DRV, ERR, "queue not allocated\n");
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	if (adapter->rx_ring[queue]->active) {
		DPRINTK(DRV, ERR, "filter count exceeded\n");
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	macaddr = vmknetddi_queueops_get_filter_macaddr(&args->filter);

	rval = igb_set_rxqueue_macfilter(args->netdev, queue, macaddr);
	if (rval == 0) {
		adapter->rx_ring[queue]->active = TRUE;
		/* force to 0 since we only support one filter per queue */
		args->filterid = VMKNETDDI_QUEUEOPS_MK_FILTERID(0);
		return VMKNETDDI_QUEUEOPS_OK;
	} else {
		return VMKNETDDI_QUEUEOPS_ERR;
	}
}

static int
igb_remove_rx_filter(vmknetddi_queueop_remove_rx_filter_args_t *args)
{
	int rval;
	u16 cidx = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args->queueid);
	u16 fidx = VMKNETDDI_QUEUEOPS_FILTERID_VAL(args->filterid);
	struct igb_adapter *adapter = netdev_priv(args->netdev);
	u8 macaddr[ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

	DPRINTK(DRV, DEBUG, "removing filter on cidx=%d, fidx=%d\n",
		cidx, fidx);

	/* This will return an error because broadcast is not a valid
	 * Ethernet address, so ignore and carry on
	 */
	rval = igb_set_rxqueue_macfilter(args->netdev, cidx, macaddr);
	adapter->rx_ring[cidx]->active = FALSE;
	return rval;
}


static int
igb_get_queue_stats(vmknetddi_queueop_get_stats_args_t *args)
{
	return VMKNETDDI_QUEUEOPS_ERR;
}

static int
igb_get_netqueue_version(vmknetddi_queueop_get_version_args_t *args)
{
	return vmknetddi_queueops_version(args);
}
static int igb_set_tx_priority(vmknetddi_queueop_set_tx_priority_args_t *args)
{
	/* Not supported */
	return VMKNETDDI_QUEUEOPS_OK;
}
int
igb_netqueue_ops(vmknetddi_queueops_op_t op, void *args)
{
	switch (op) {
	case VMKNETDDI_QUEUEOPS_OP_GET_VERSION:
		return igb_get_netqueue_version(
			(vmknetddi_queueop_get_version_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_GET_FEATURES:
		return igb_get_netqueue_features(
			(vmknetddi_queueop_get_features_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_GET_QUEUE_COUNT:
		return igb_get_queue_count(
			(vmknetddi_queueop_get_queue_count_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_GET_FILTER_COUNT:
		return igb_get_filter_count(
			(vmknetddi_queueop_get_filter_count_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_ALLOC_QUEUE:
		return igb_alloc_queue(
			(vmknetddi_queueop_alloc_queue_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_FREE_QUEUE:
		return igb_free_queue(
			(vmknetddi_queueop_free_queue_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_GET_QUEUE_VECTOR:
		return igb_get_queue_vector(
			(vmknetddi_queueop_get_queue_vector_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_GET_DEFAULT_QUEUE:
		return igb_get_default_queue(
			(vmknetddi_queueop_get_default_queue_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_APPLY_RX_FILTER:
		return igb_apply_rx_filter(
			(vmknetddi_queueop_apply_rx_filter_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_REMOVE_RX_FILTER:
		return igb_remove_rx_filter(
			(vmknetddi_queueop_remove_rx_filter_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_GET_STATS:
		return igb_get_queue_stats(
			(vmknetddi_queueop_get_stats_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_SET_TX_PRIORITY:
		return igb_set_tx_priority(
			(vmknetddi_queueop_set_tx_priority_args_t *)args);
		break;

	default:
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	return VMKNETDDI_QUEUEOPS_ERR;
}

#endif /* __VMKNETDDI_QUEUEOPS__ */
