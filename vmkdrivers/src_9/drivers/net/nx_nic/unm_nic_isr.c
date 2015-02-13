/*
 * Copyright (C) 2003 - 2007 NetXen, Inc.
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
 * MA  02111-1307, USA.
 * 
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.
 * 
 * Contact Information:
 *    licensing@netxen.com
 * NetXen, Inc.
 * 3965 Freedom Circle, Fourth floor,
 * Santa Clara, CA 95054
 */
#include <linux/netdevice.h>
#include <linux/delay.h>

#include "queue.h"
#include "unm_nic.h"
#include "unm_nic_hw.h"
#include "nic_cmn.h"
#include "nic_phan_reg.h"

long
unm_nic_enable_phy_interrupts (unm_adapter *adapter)
{
        switch (adapter->ahw.board_type) {
        case UNM_NIC_GBE:
                return unm_niu_gbe_enable_phy_interrupts (adapter);

        case UNM_NIC_XGBE:
                return unm_niu_xg_enable_phy_interrupts (adapter);

        default:
                printk(KERN_ERR"%s: Unknown board type\n", unm_nic_driver_name);
		return -1;
        }
}

long
unm_nic_disable_phy_interrupts (unm_adapter *adapter)
{
        switch (adapter->ahw.board_type) {
        case UNM_NIC_GBE:
                return unm_niu_gbe_disable_phy_interrupts (adapter);

        case UNM_NIC_XGBE:
                return unm_niu_xg_disable_phy_interrupts (adapter);

        default:
                printk(KERN_ERR"%s: Unknown board type\n", unm_nic_driver_name);
		return -1;
        }
}

long
unm_nic_clear_phy_interrupts (unm_adapter *adapter)
{
        switch (adapter->ahw.board_type) {
        case UNM_NIC_GBE:
                return unm_niu_gbe_clear_phy_interrupts (adapter);

        case UNM_NIC_XGBE:
                return unm_niu_xg_disable_phy_interrupts (adapter);

        default:
                printk(KERN_ERR"%s: Unknown board type\n", unm_nic_driver_name);
		return -1;
        }
}


void
unm_indicate_link_status(unm_adapter *adapter, u32 link)
{
        struct net_device        *netdev = adapter->netdev;

        if(link) {
                if (netdev->flags & IFF_UP) {
                        netif_carrier_on(netdev);
                        netif_wake_queue(netdev);
                }
        } else {
                netif_carrier_off(netdev);
                netif_stop_queue(netdev);
        }
}

void
unm_nic_isr_other(struct unm_adapter_s *adapter)
{
	u32 portno = adapter->portnum;
	u32 val, linkup, qg_linksup = adapter->ahw.linkup;

	read_lock(&adapter->adapter_lock);
    adapter->unm_nic_hw_read_wx(adapter, CRB_XG_STATE, &val, 4);
	read_unlock(&adapter->adapter_lock);

	linkup = 1 & (val >> adapter->physical_port);
	adapter->ahw.linkup = linkup;

	if (linkup != qg_linksup) {
		printk(KERN_INFO "%s: PORT %d link %s\n", unm_nic_driver_name,
				portno, ((linkup == 0) ? "down" : "up"));
		unm_indicate_link_status(adapter, linkup);
		if (linkup) {
			unm_nic_set_link_parameters(adapter);
		}

	}
	//adapter->stats.otherints++;
}

/*
 * return 1 if link is ok, 0 otherwise
 */
int
unm_link_ok(struct unm_adapter_s *adapter)
{
        switch (adapter->ahw.board_type) {
        case UNM_NIC_GBE:
        case UNM_NIC_XGBE:
                return (adapter->ahw.linkup);

        default:
                printk(KERN_ERR"%s: Function: %s, Unknown board type\n",
                        unm_nic_driver_name, __FUNCTION__);
                break;
        }

        return 0;
}
void
unm_nic_handle_phy_intr(struct unm_adapter_s *adapter)
{
        uint32_t  val, val1, linkupval;
	struct net_device  *netdev;

        switch (adapter->ahw.board_type) {
        case UNM_NIC_GBE:
		if (NX_IS_REVISION_P2(adapter->ahw.revision_id)) {
			unm_nic_isr_other(adapter);
			break;
		}
		/* Fall through for processing */
        case UNM_NIC_XGBE:

                /* WINDOW = 1 */
                //unm_nic_read_crb_w1(adapter, CRB_XG_STATE, &val1);
                read_lock(&adapter->adapter_lock);
		if (NX_IS_REVISION_P3(adapter->ahw.revision_id)) {
                	adapter->unm_nic_hw_read_wx(adapter, CRB_XG_STATE_P3,
								 &val, 4);
			val1 = XG_LINK_STATE_P3(adapter->ahw.pci_func,val);
			linkupval = XG_LINK_UP_P3;
		} else {
                	adapter->unm_nic_hw_read_wx(adapter, CRB_XG_STATE, 
								&val, 4);
                	val >>= (adapter->portnum * 8);
			val1 = val & 0xff;
			linkupval = XG_LINK_UP;
		}
		read_unlock(&adapter->adapter_lock);

		netdev = adapter->netdev;

		if (adapter->ahw.linkup && (val1 != linkupval)) {
			printk(KERN_ERR"%s: %s NIC Link is down\n",
					unm_nic_driver_name, netdev->name);
			unm_indicate_link_status(adapter, 0);
			adapter->ahw.linkup = 0;

		} else if (!adapter->ahw.linkup && (val1 == linkupval)) {
			printk(KERN_ERR"%s: %s NIC Link is up\n",
			       unm_nic_driver_name, netdev->name);
			unm_indicate_link_status(adapter, 1);
			adapter->ahw.linkup = 1;
		}
		if (adapter->ahw.board_type == UNM_NIC_GBE &&
		    adapter->ahw.linkup) {
			unm_nic_set_link_parameters(adapter);
		}

		break;

	default:
		printk(KERN_ERR"%s ISR: Unknown board type\n",
				unm_nic_driver_name);
	}

        return;
}
