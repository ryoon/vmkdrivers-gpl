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


#include <linux/types.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/ipv6.h>
#ifdef __VMKLNX__ 
#include <linux/hash.h>
#endif

#include "ixgbe.h"

#include "ixgbe_sriov.h"

int ixgbe_set_vf_multicasts(struct ixgbe_adapter *adapter,
			    int entries, u16 *hash_list, u32 vf)
{
	struct vf_data_storage *vfinfo = &adapter->vfinfo[vf];
	struct ixgbe_hw *hw = &adapter->hw;
	int i;
	u32 vector_bit;
	u32 vector_reg;
	u32 mta_reg;
	u32 vmolr = IXGBE_READ_REG(hw, IXGBE_VMOLR(vf));

	/* only so many hash values supported */
	entries = min(entries, IXGBE_MAX_VF_MC_ENTRIES);

	/* salt away the number of multi cast addresses assigned
	 * to this VF for later use to restore when the PF multi cast
	 * list changes
	 */
	vfinfo->num_vf_mc_hashes = entries;

	/* VFs are limited to using the MTA hash table for their multicast
	 * addresses */
	for (i = 0; i < entries; i++) {
		vfinfo->vf_mc_hashes[i] = hash_list[i];;
	}

	for (i = 0; i < vfinfo->num_vf_mc_hashes; i++) {
		hw->addr_ctrl.mta_in_use++;
		vector_reg = (vfinfo->vf_mc_hashes[i] >> 5) & 0x7F;
		vector_bit = vfinfo->vf_mc_hashes[i] & 0x1F;
		mta_reg = IXGBE_READ_REG(hw, IXGBE_MTA(vector_reg));
		mta_reg |= (1 << vector_bit);
		IXGBE_WRITE_REG(hw, IXGBE_MTA(vector_reg), mta_reg);
	}
	vmolr |= IXGBE_VMOLR_ROMPE;
	IXGBE_WRITE_REG(hw, IXGBE_VMOLR(vf), vmolr);
	ixgbe_enable_mc(hw);

	return 0;
}

void ixgbe_restore_vf_multicasts(struct ixgbe_adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	struct vf_data_storage *vfinfo;
	int i, j;
	u32 vector_bit;
	u32 vector_reg;
	u32 mta_reg;

	for (i = 0; i < adapter->num_vfs; i++) {
		u32 vmolr = IXGBE_READ_REG(hw, IXGBE_VMOLR(i));
		vfinfo = &adapter->vfinfo[i];
		for (j = 0; j < vfinfo->num_vf_mc_hashes; j++) {
			hw->addr_ctrl.mta_in_use++;
			vector_reg = (vfinfo->vf_mc_hashes[j] >> 5) & 0x7F;
			vector_bit = vfinfo->vf_mc_hashes[j] & 0x1F;
			mta_reg = IXGBE_READ_REG(hw, IXGBE_MTA(vector_reg));
			mta_reg |= (1 << vector_bit);
			IXGBE_WRITE_REG(hw, IXGBE_MTA(vector_reg), mta_reg);
		}
		if (vfinfo->num_vf_mc_hashes)
			vmolr |= IXGBE_VMOLR_ROMPE;
		else
			vmolr &= ~IXGBE_VMOLR_ROMPE;
		IXGBE_WRITE_REG(hw, IXGBE_VMOLR(i), vmolr);
	}

	/* Restore any VF macvlans */
	ixgbe_full_sync_mac_table(adapter);
}

int ixgbe_set_vf_vlan(struct ixgbe_adapter *adapter, int add, int vid, u32 vf)
{
	return ixgbe_set_vfta(&adapter->hw, vid, vf, (bool)add);
}

#ifndef __VMKLNX__
void ixgbe_set_vf_lpe(struct ixgbe_adapter *adapter, u32 *msgbuf)
{
	struct ixgbe_hw *hw = &adapter->hw;
	int new_mtu = msgbuf[1];
	u32 max_frs;
	int max_frame = new_mtu + ETH_HLEN + ETH_FCS_LEN;

	/* Only X540 supports jumbo frames in IOV mode */
	if (adapter->hw.mac.type != ixgbe_mac_X540)
		return;

	/* MTU < 68 is an error and causes problems on some kernels */
	if ((new_mtu < 68) || (max_frame > IXGBE_MAX_JUMBO_FRAME_SIZE)) {
		e_err(drv, "VF mtu %d out of range\n", new_mtu);
		return;
	}

	max_frs = (IXGBE_READ_REG(hw, IXGBE_MAXFRS) &
		   IXGBE_MHADD_MFS_MASK) >> IXGBE_MHADD_MFS_SHIFT;
	if (max_frs < new_mtu) {
		max_frs = new_mtu << IXGBE_MHADD_MFS_SHIFT;
		IXGBE_WRITE_REG(hw, IXGBE_MAXFRS, max_frs);
	}

	e_info(drv, "VF requests change max MTU to %d\n", new_mtu);
}
#else /* __VMKLNX__ */
int ixgbe_set_vf_lpe(struct ixgbe_adapter *adapter, u32 vf, u32 new_mtu)
{
        struct ixgbe_hw *hw = &adapter->hw;
        u32 max_frs;
        int max_frame = new_mtu + ETH_HLEN + ETH_FCS_LEN;

        /* Only X540 supports jumbo frames in IOV mode */
        if (adapter->hw.mac.type != ixgbe_mac_X540)
                return -1;
 
        /* MTU < 68 is an error and causes problems on some kernels */
        if ((new_mtu < 68) || (max_frame > IXGBE_MAX_JUMBO_FRAME_SIZE)) {
                DPRINTK(PROBE, ERR, "VF mtu %d out of range\n", new_mtu);
                return -1;
        }
 
        max_frs = (IXGBE_READ_REG(hw, IXGBE_MAXFRS) &
                   IXGBE_MHADD_MFS_MASK) >> IXGBE_MHADD_MFS_SHIFT;
        if (max_frs < new_mtu) {
                max_frs = new_mtu << IXGBE_MHADD_MFS_SHIFT;
                IXGBE_WRITE_REG(hw, IXGBE_MAXFRS, max_frs);
        }
 
        DPRINTK(PROBE, ERR, "VF %d requested change of MTU to %d\n", vf, new_mtu);
	return 0;
}
#endif /* __VMKLNX__ */

void ixgbe_set_vmolr(struct ixgbe_hw *hw, u32 vf, bool aupe)
{
	u32 vmolr = IXGBE_READ_REG(hw, IXGBE_VMOLR(vf));
	vmolr |=  IXGBE_VMOLR_BAM;
	if (aupe)
		vmolr |= IXGBE_VMOLR_AUPE;
	else
		vmolr &= ~IXGBE_VMOLR_AUPE;
	IXGBE_WRITE_REG(hw, IXGBE_VMOLR(vf), vmolr);
}

static void ixgbe_set_vmvir(struct ixgbe_adapter *adapter, u32 vid, u32 vf)
{
	struct ixgbe_hw *hw = &adapter->hw;

	if (vid)
		IXGBE_WRITE_REG(hw, IXGBE_VMVIR(vf),
				(vid | IXGBE_VMVIR_VLANA_DEFAULT));
	else
		IXGBE_WRITE_REG(hw, IXGBE_VMVIR(vf), 0);
}

inline void ixgbe_vf_reset_event(struct ixgbe_adapter *adapter, u32 vf)
{
	struct ixgbe_hw *hw = &adapter->hw;

	/* reset offloads to defaults */
	if (adapter->vfinfo[vf].pf_vlan) {
		ixgbe_set_vf_vlan(adapter, true,
				  adapter->vfinfo[vf].pf_vlan, vf);
		ixgbe_set_vmvir(adapter,
				(adapter->vfinfo[vf].pf_vlan |
				 (adapter->vfinfo[vf].pf_qos << 
				  VLAN_PRIO_SHIFT)), vf);
		ixgbe_set_vmolr(hw, vf, false);
	} else {
		ixgbe_set_vmvir(adapter, 0, vf);
		ixgbe_set_vmolr(hw, vf, true);
	}


	/* reset multicast table array for vf */
	adapter->vfinfo[vf].num_vf_mc_hashes = 0;

	/* Flush and reset the mta with the new values */
	ixgbe_set_rx_mode(adapter->netdev);

	ixgbe_del_mac_filter(adapter, adapter->vfinfo[vf].vf_mac_addresses, vf);
}

int ixgbe_set_vf_mac(struct ixgbe_adapter *adapter,
                          int vf, unsigned char *mac_addr)
{
	s32 retval = 0;
	ixgbe_del_mac_filter(adapter, adapter->vfinfo[vf].vf_mac_addresses, vf);
	retval = ixgbe_add_mac_filter(adapter, mac_addr, vf);
	if (retval >= 0) {
		memcpy(adapter->vfinfo[vf].vf_mac_addresses, mac_addr, ETH_ALEN);
	} else if (retval != -EEXIST){
		memset(adapter->vfinfo[vf].vf_mac_addresses, 0, ETH_ALEN);
	}

	return retval;
}

static int ixgbe_set_vf_macvlan(struct ixgbe_adapter *adapter,
				int vf, int index, unsigned char *mac_addr)
{
	struct list_head *pos;
	struct vf_macvlans *entry;
	s32 retval = 0;

	if (index <= 1) {
		list_for_each(pos, &adapter->vf_mvs.l) {
			entry = list_entry(pos, struct vf_macvlans, l);
			if (entry->vf == vf) {
				entry->vf = -1;
				entry->free = true;
				entry->is_macvlan = false;
				ixgbe_del_mac_filter(adapter,
						     entry->vf_macvlan, vf);
			}
		}
	}

	/*
	 * If index was zero then we were asked to clear the uc list
	 * for the VF.  We're done.
	 */
	if (!index)
		return 0;

	entry = NULL;

	list_for_each(pos, &adapter->vf_mvs.l) {
		entry = list_entry(pos, struct vf_macvlans, l);
		if (entry->free)
			break;
	}

	/*
	 * If we traversed the entire list and didn't find a free entry
	 * then we're out of space on the RAR table.  Also entry may
	 * be NULL because the original memory allocation for the list
	 * failed, which is not fatal but does mean we can't support
	 * VF requests for MACVLAN because we couldn't allocate
	 * memory for the list management required.
	 */
	if (!entry || !entry->free)
		return -ENOSPC;

	retval = ixgbe_add_mac_filter(adapter, mac_addr, vf);
	if (retval >= 0) {
		entry->free = false;
        	entry->is_macvlan = true;
        	entry->vf = vf;
        	memcpy(entry->vf_macvlan, mac_addr, ETH_ALEN);
	}

	return retval;
}

int ixgbe_vf_configuration(struct pci_dev *pdev, unsigned int event_mask)
{
	unsigned char vf_mac_addr[6];
	struct ixgbe_adapter *adapter = pci_get_drvdata(pdev);
	unsigned int vfn = (event_mask & 0x3f);

	bool enable = ((event_mask & 0x10000000U) != 0);

	if (enable) {
#ifndef __VMKLNX__
		random_ether_addr(vf_mac_addr);
#else
		u32 addr;
		
		addr =
		   hash_long(*((unsigned long *)&adapter->netdev->dev_addr[2]),
			16) + adapter->netdev->dev_addr[5];
		
		vf_mac_addr[5] = vfn;
		vf_mac_addr[4] = (u8)(addr & 0xFF);
		vf_mac_addr[3] = (u8)((addr >> 8) & 0xFF);
		/* Use the OUI from the current MAC address */
		memcpy(vf_mac_addr, adapter->netdev->dev_addr, 3);

#endif /* __VMKLNX__ */
		e_info(probe, "IOV: VF %d is enabled "
		       "mac %02X:%02X:%02X:%02X:%02X:%02X\n",
		       vfn,
		       vf_mac_addr[0], vf_mac_addr[1], vf_mac_addr[2],
		       vf_mac_addr[3], vf_mac_addr[4], vf_mac_addr[5]);
		/* Store away the VF "permananet" MAC address, it will ask
		 * for it later.
		 */
		memcpy(adapter->vfinfo[vfn].vf_mac_addresses, vf_mac_addr, 6);
	}

	return 0;
}

inline void ixgbe_vf_reset_msg(struct ixgbe_adapter *adapter, u32 vf)
{
	struct ixgbe_hw *hw = &adapter->hw;
	u32 reg;
	u32 reg_offset, vf_shift;
	/* q_per_pool assumes that DCB is not enabled, hence in 64 pool mode */
	u32 q_per_pool = 2;
	int i;

	vf_shift = vf % 32;
	reg_offset = vf / 32;

	/* enable transmit and receive for vf */
	reg = IXGBE_READ_REG(hw, IXGBE_VFTE(reg_offset));
	reg |= (reg | (1 << vf_shift));
	IXGBE_WRITE_REG(hw, IXGBE_VFTE(reg_offset), reg);

	reg = IXGBE_READ_REG(hw, IXGBE_VFRE(reg_offset));
	reg |= (reg | (1 << vf_shift));
	IXGBE_WRITE_REG(hw, IXGBE_VFRE(reg_offset), reg);

	reg = IXGBE_READ_REG(hw, IXGBE_VMECM(reg_offset));
	reg |= (1 << vf_shift);
	IXGBE_WRITE_REG(hw, IXGBE_VMECM(reg_offset), reg);

	/* Reset the VFs TDWBAL and TDWBAH registers 
	 * which are not cleared by an FLR 
	 */
	for (i = 0; i < q_per_pool; i++) {
        	IXGBE_WRITE_REG(hw, IXGBE_PVFTDWBAHn(q_per_pool, vf, i), 0);
        	IXGBE_WRITE_REG(hw, IXGBE_PVFTDWBALn(q_per_pool, vf, i), 0);
	}

	ixgbe_vf_reset_event(adapter, vf);
}

static int ixgbe_rcv_msg_from_vf(struct ixgbe_adapter *adapter, u32 vf)
{
	u32 mbx_size = IXGBE_VFMAILBOX_SIZE;
	u32 msgbuf[mbx_size];
	struct ixgbe_hw *hw = &adapter->hw;
	s32 retval;
	int entries;
	u16 *hash_list;
	int add, vid, index;
	u8 *new_mac;
#ifdef __VMKLNX__
	struct vf_vlan guest_vlan;
	int new_mtu;
	int max_frame;
#endif
	retval = ixgbe_read_mbx(hw, msgbuf, mbx_size, vf);

	if (retval)
		printk(KERN_ERR "Error receiving message from VF\n");

	/* this is a message we already processed, do nothing */
	if (msgbuf[0] & (IXGBE_VT_MSGTYPE_ACK | IXGBE_VT_MSGTYPE_NACK))
		return retval;

	/*
	 * until the vf completes a virtual function reset it should not be
	 * allowed to start any configuration.
	 */

	if (msgbuf[0] == IXGBE_VF_RESET) {
		unsigned char *vf_mac = adapter->vfinfo[vf].vf_mac_addresses;
		new_mac = (u8 *)(&msgbuf[1]);
		adapter->vfinfo[vf].clear_to_send = false;
		ixgbe_vf_reset_msg(adapter, vf);
		adapter->vfinfo[vf].clear_to_send = true;

		DPRINTK(PROBE, ERR, "IXGBE_VF_RESET mailbox message received\n");
		ixgbe_set_vf_mac(adapter, vf, vf_mac);

		/* reply to reset with ack and vf mac address */
		msgbuf[0] = IXGBE_VF_RESET | IXGBE_VT_MSGTYPE_ACK;
		memcpy(new_mac, vf_mac, IXGBE_ETH_LENGTH_OF_ADDRESS);
 		/* Piggyback the multicast filter type so VF can compute the
 		 * correct vectors */
		msgbuf[3] = hw->mac.mc_filter_type;
		retval = ixgbe_write_mbx(hw, msgbuf, IXGBE_VF_PERMADDR_MSG_LEN, vf);

		return retval;
	}

	if (!adapter->vfinfo[vf].clear_to_send) {
		msgbuf[0] |= IXGBE_VT_MSGTYPE_NACK;
		ixgbe_write_mbx(hw, msgbuf, 1, vf);
		return retval;
	}

	switch ((msgbuf[0] & 0xFFFF)) {
	case IXGBE_VF_SET_MAC_ADDR:
		new_mac = ((u8 *)(&msgbuf[1]));
#ifdef __VMKLNX__
		if (is_valid_ether_addr(new_mac)) {
			if (memcmp(adapter->vfinfo[vf].vf_mac_addresses,
				new_mac, ETH_ALEN)) {
				DPRINTK(PROBE, ERR, "VF %d attempted to override "
					"administratively set MAC address\nReload "
					"the VF driver to resume operations\n", vf);
				retval = -1;
			}
			ixgbe_passthru_config(adapter, vf, VMK_CFG_MAC_CHANGED,
				(void*)new_mac);
			retval = 0;
		}
		else
			retval = -1;
#else /* __VMKLNX__ */
		if (is_valid_ether_addr(new_mac) &&
		    !adapter->vfinfo[vf].pf_set_mac) {
			e_info(probe, "Set MAC msg received from VF %d\n", vf);
			if (ixgbe_set_vf_mac(adapter, vf, new_mac) >= 0) {
				retval = 0 ;
			} else {
				retval = -1;
			} 
		} else if (memcmp(adapter->vfinfo[vf].vf_mac_addresses,
				  new_mac, ETH_ALEN)) {
			e_warn(drv, "VF %d attempted to override "
			       "administratively set MAC address\nReload "
			       "the VF driver to resume operations\n", vf);
			retval = -1;
		}
		break;
#endif /* __VMKLNX__ */
	case IXGBE_VF_SET_MULTICAST:
		entries = (msgbuf[0] & IXGBE_VT_MSGINFO_MASK)
					>> IXGBE_VT_MSGINFO_SHIFT;
		hash_list = (u16 *)&msgbuf[1];
		retval = ixgbe_set_vf_multicasts(adapter, entries,
						 hash_list, vf);
		break;
	case IXGBE_VF_SET_LPE:
		e_info(probe, "Set LPE msg received from vf %d\n", vf);
#ifdef __VMKLNX__
		new_mtu = msgbuf[1];
		max_frame = new_mtu + ETH_HLEN + ETH_FCS_LEN;

		DPRINTK(PROBE, ERR, "Set LPE msg received from vf %d\n", vf);

		/* Only X540 supports jumbo frames in IOV mode */
		if (adapter->hw.mac.type != ixgbe_mac_X540) {
			DPRINTK(PROBE, ERR, "Not Twinville. Rejecting MTU change request from guest\n");
			retval = -1;
			break;
		}

		/* MTU < 68 is an error and causes problems on some kernels */
		if ((new_mtu < 68) || (max_frame > IXGBE_MAX_JUMBO_FRAME_SIZE)) {
			DPRINTK(PROBE, ERR, "VF mtu %d out of range\n", new_mtu);
			retval = -1;
			break;
		}
		ixgbe_passthru_config(adapter, vf, VMK_CFG_MTU_CHANGED,
			(void*)&new_mtu);
#else
		ixgbe_set_vf_lpe(adapter, msgbuf);
#endif /* __VMKLNX__ */
		break;
	case IXGBE_VF_SET_VLAN:
		add = (msgbuf[0] & IXGBE_VT_MSGINFO_MASK)
					>> IXGBE_VT_MSGINFO_SHIFT;
		vid = (msgbuf[1] & IXGBE_VLVF_VLANID_MASK);
		DPRINTK(PROBE, ERR, "Mailbox msg received to %s vlan id %d to vf %d\n",
			(add > 0 ? "add" : "delete"), vid, vf);
#ifdef __VMKLNX__
		guest_vlan.add = (bool)add;
		guest_vlan.vid = vid;
		if ((guest_vlan.add == true) &&
			(guest_vlan.vid > 4095)) {
			e_warn(drv, "Invalid Guest VLAN ID configuration\n");
			retval = -1;
			break;
		}
		ixgbe_passthru_config(adapter, vf, add > 0 ? VMK_CFG_GUEST_VLAN_ADD
                                      : VMK_CFG_GUEST_VLAN_REMOVE, (void*)&guest_vlan);
#else /* __VMKLNX__ */
		if (adapter->vfinfo[vf].pf_vlan) {
			e_warn(drv, "VF %d attempted to override "
			       "administratively set VLAN configuration\n"
			       "Reload the VF driver to resume operations\n",
			       vf);
			retval = -1;
		} else {
			retval = ixgbe_set_vf_vlan(adapter, add, vid, vf);
		}
#endif /* __VMKLNX__ */
		break;
	case IXGBE_VF_SET_MACVLAN:
#ifdef __VMKLNX__
		e_warn(drv, "MACVLANs not supported\n");
		retval = -1;
#else /* __VMKLNX__ */
		index = (msgbuf[0] & IXGBE_VT_MSGINFO_MASK) >>
			IXGBE_VT_MSGINFO_SHIFT;
		/*
		 * If the VF is allowed to set MAC filters then turn off
		 * anti-spoofing to avoid false positives.  An index
		 * greater than 0 will indicate the VF is setting a
		 * macvlan MAC filter.
		 */
		if (index > 0 && adapter->antispoofing_enabled) {
			hw->mac.ops.set_mac_anti_spoofing(hw, false,
							  adapter->num_vfs);
			hw->mac.ops.set_vlan_anti_spoofing(hw, false, vf);
			adapter->antispoofing_enabled = false;
		}
		retval = ixgbe_set_vf_macvlan(adapter, vf, index,
					      (unsigned char *)(&msgbuf[1]));
#endif /* __VMKLNX__ */
		break;
	default:
		e_err(drv, "Unhandled Msg %8.8x\n", msgbuf[0]);
		retval = IXGBE_ERR_MBX;
		break;
	}

	/* notify the VF of the results of what it sent us */
	if (retval)
		msgbuf[0] |= IXGBE_VT_MSGTYPE_NACK;
	else
		msgbuf[0] |= IXGBE_VT_MSGTYPE_ACK;

	msgbuf[0] |= IXGBE_VT_MSGTYPE_CTS;

	ixgbe_write_mbx(hw, msgbuf, 1, vf);

	return retval;
}

static void ixgbe_rcv_ack_from_vf(struct ixgbe_adapter *adapter, u32 vf)
{
	struct ixgbe_hw *hw = &adapter->hw;
	u32 msg = IXGBE_VT_MSGTYPE_NACK;

	/* if device isn't clear to send it shouldn't be reading either */
	if (!adapter->vfinfo[vf].clear_to_send)
		ixgbe_write_mbx(hw, &msg, 1, vf);
}

void ixgbe_msg_task(struct ixgbe_adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	u32 vf;

	for (vf = 0; vf < adapter->num_vfs; vf++) {
		/* process any reset requests */
		if (!ixgbe_check_for_rst(hw, vf))
			ixgbe_vf_reset_event(adapter, vf);

		/* process any messages pending */
		if (!ixgbe_check_for_msg(hw, vf))
			ixgbe_rcv_msg_from_vf(adapter, vf);

		/* process any acks */
		if (!ixgbe_check_for_ack(hw, vf))
			ixgbe_rcv_ack_from_vf(adapter, vf);
	}
}

void ixgbe_disable_tx_rx(struct ixgbe_adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;

	/* disable transmit and receive for all vfs */
	IXGBE_WRITE_REG(hw, IXGBE_VFTE(0), 0);
	IXGBE_WRITE_REG(hw, IXGBE_VFTE(1), 0);

	IXGBE_WRITE_REG(hw, IXGBE_VFRE(0), 0);
	IXGBE_WRITE_REG(hw, IXGBE_VFRE(1), 0);
}

void ixgbe_ping_all_vfs(struct ixgbe_adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	u32 ping;
	int i;

	for (i = 0 ; i < adapter->num_vfs; i++) {
		ping = IXGBE_PF_CONTROL_MSG;
		if (adapter->vfinfo[i].clear_to_send)
			ping |= IXGBE_VT_MSGTYPE_CTS;
		ixgbe_write_mbx(hw, &ping, 1, i);
	}
}

#ifdef HAVE_IPLINK_VF_CONFIG
int ixgbe_ndo_set_vf_mac(struct net_device *netdev, int vf, u8 *mac)
{
	s32 retval = 0;
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	if (!is_valid_ether_addr(mac) || (vf >= adapter->num_vfs))
		return -EINVAL;
	dev_info(&adapter->pdev->dev, 
			"setting MAC %pM on VF %d\n", mac, vf);
	dev_info(&adapter->pdev->dev, 
			"Reload the VF driver to make this"
			" change effective.\n");
	retval = ixgbe_set_vf_mac(adapter, vf, mac);
	if (retval >= 0) {
#ifndef __VMKLNX__
		adapter->vfinfo[vf].pf_set_mac = true;
#endif
		if (test_bit(__IXGBE_DOWN, &adapter->state)) {
			dev_warn(&adapter->pdev->dev, 
				"The VF MAC address has been set,"
				" but the PF device is not up.\n");
			dev_warn(&adapter->pdev->dev, 
				"Bring the PF device up before"
				" attempting to use the VF device.\n");
		}
	} else {
		dev_warn(&adapter->pdev->dev,
		"The VF MAC address was NOT changed."
		"Attempt to configure with invalid" 
		" or duplicate MAC address rejected.\n");
	}
	return retval;
}

int ixgbe_ndo_set_vf_vlan(struct net_device *netdev, int vf, u16 vlan, u8 qos)
{
	int err = 0;
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	struct ixgbe_hw *hw = &adapter->hw;

	/* VLAN IDs accepted range 0-4094 */
	if ((vf >= adapter->num_vfs) || (vlan > VLAN_VID_MASK-1) || (qos > 7))
		return -EINVAL;
	if (vlan || qos) {
		err = ixgbe_set_vf_vlan(adapter, true, vlan, vf);
		if (err)
			goto out;
		ixgbe_set_vmvir(adapter, vlan | (qos << VLAN_PRIO_SHIFT), vf);
		ixgbe_set_vmolr(hw, vf, false);
		if (adapter->antispoofing_enabled)
			hw->mac.ops.set_vlan_anti_spoofing(hw, true, vf);
		adapter->vfinfo[vf].pf_vlan = vlan;
		adapter->vfinfo[vf].pf_qos = qos;
		dev_info(&adapter->pdev->dev,
			 "Setting VLAN %d, QOS 0x%x on VF %d\n", vlan, qos, vf);
		if (test_bit(__IXGBE_DOWN, &adapter->state)) {
			dev_warn(&adapter->pdev->dev,
				 "The VF VLAN has been set,"
				 " but the PF device is not up.\n");
			dev_warn(&adapter->pdev->dev,
				 "Bring the PF device up before"
				 " attempting to use the VF device.\n");
		}
	} else {
		err = ixgbe_set_vf_vlan(adapter, false,
					adapter->vfinfo[vf].pf_vlan, vf);
		ixgbe_set_vmvir(adapter, vlan, vf);
		ixgbe_set_vmolr(hw, vf, true);
		hw->mac.ops.set_vlan_anti_spoofing(hw, false, vf);
		adapter->vfinfo[vf].pf_vlan = 0;
		adapter->vfinfo[vf].pf_qos = 0;
       }
out:
       return err;
}

#ifdef __VMKLNX__
int ixgbe_vf_get_stats(struct net_device *netdev, vmk_VFID vf,
			uint8_t numTxQueues, uint8_t numRxQueues,
			vmk_NetVFTXQueueStats *tqStats,
			vmk_NetVFRXQueueStats *rqStats)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	int i;

	memset(tqStats, 0, numTxQueues * sizeof (vmk_NetVFTXQueueStats));
	memset(rqStats, 0, numRxQueues * sizeof (vmk_NetVFRXQueueStats));

	/* for now, no more than one rx/tx queue pair supported */
	numTxQueues = min((int)numTxQueues, 1);
	numRxQueues = min((int)numRxQueues, 1);

	for (i = 0; i < numTxQueues; i++) {
		tqStats[i].unicastPkts = adapter->vfinfo[vf].vfstats.gptc;
		tqStats[i].unicastBytes = adapter->vfinfo[vf].vfstats.gotc;
	}

	for (i = 0; i < numRxQueues; i++) {
		rqStats[i].unicastPkts = adapter->vfinfo[vf].vfstats.gprc;
		rqStats[i].unicastBytes = adapter->vfinfo[vf].vfstats.gorc;
		rqStats[i].multicastPkts = adapter->vfinfo[vf].vfstats.mprc;
	}
	return 0;
}
#endif /* __VMKLNX__ */


int ixgbe_ndo_set_vf_bw(struct net_device *netdev, int vf, int tx_rate)
{
	return -EOPNOTSUPP;
}

#ifndef __VMKLNX__
int ixgbe_ndo_get_vf_config(struct net_device *netdev,
			    int vf, struct ifla_vf_info *ivi)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	if (vf >= adapter->num_vfs)
		return -EINVAL;
	ivi->vf = vf;
	memcpy(&ivi->mac, adapter->vfinfo[vf].vf_mac_addresses, ETH_ALEN);
	ivi->tx_rate = 0;
	ivi->vlan = adapter->vfinfo[vf].pf_vlan;
	ivi->qos = adapter->vfinfo[vf].pf_qos;
	return 0;
}
#endif /* __VMKLNX__ */
#endif
