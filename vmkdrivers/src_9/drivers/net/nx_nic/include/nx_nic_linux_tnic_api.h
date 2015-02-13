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
/*
 * The Linux API exported by the NetXen NIC driver to the LSA driver.
 */
#ifndef NX_NIC_LINUX_TNIC_API_H
#define NX_NIC_LINUX_TNIC_API_H

#include <linux/skbuff.h>
#include "nic_cmn.h"
#include "message.h"
#include "unm_pstats.h"

typedef struct nx_tnic_adapter_s		nx_tnic_adapter_t;

/* 
 * Gets the tnic version number from the card. 
 */
typedef struct {
        __uint32_t	major;
        __uint32_t	minor;
        __uint32_t	sub;
	__uint32_t	build;
} nic_version_t;

struct sk_buff;

#define NX_NIC_CB_LSA           0
#define NX_NIC_CB_SCSI          1
	
#define NX_NIC_CB_MAX           2	
#define NX_MAX_SDS_OPCODE       64


typedef struct nx_nic_api {
	int		api_ver;
	int		(*is_netxen_device) (struct net_device *netdev);
	int		(*register_msg_handler) (struct net_device *netdev, 
						 uint8_t msgtype,
						 void *data,
						 int (*nx_msg_handler) (struct net_device *netdev,
									void *data,
									unm_msg_t *msg,
									struct sk_buff *skb));
	void		(*unregister_msg_handler) (struct net_device *netdev,
						   uint8_t msgtype);
	int		(*register_callback_handler) (struct net_device *netdev,
						      uint8_t interface_type,	                                               
						      void *data);
	void		(*unregister_callback_handler) (struct net_device *netdev,
							uint8_t interface_type);
	int		(*get_adapter_rev_id) (struct net_device *netdev);
	nx_tnic_adapter_t	*(*get_lsa_adapter) (struct net_device *netdev);
	int		(*get_device_port) (struct net_device *netdev);
	int		(*get_device_ring_ctx) (struct net_device *netdev);
	struct pci_dev	*(*get_pcidev) (struct net_device *netdev);
	void		(*get_lsa_ver_num) (struct net_device *netdev,
					    nic_version_t * version);
	void		(*get_nic_ver_num) (struct net_device *netdev,
					    nic_version_t * version);
	uint64_t	(*get_fw_capabilities) (struct net_device *netdev);
	int		(*send_msg_to_fw) (struct net_device *netdev, 
					   pegnet_cmd_desc_t *cmd_desc_arr,
					   int nr_elements);
	int		(*send_msg_to_fw_pexq) (struct net_device *netdev, 
					   pegnet_cmd_desc_t *cmd_desc_arr,
					   int nr_elements);
	int		(*cmp_adapter_id) (struct net_device *dev1, 
					   struct net_device *dev2);
	struct proc_dir_entry *(*get_base_procfs_dir)(void);
	void		(*register_lsa_with_fw) (struct net_device *dev);
	void		(*unregister_lsa_from_fw) (struct net_device *dev);
	int		(*get_pexq_cap) (struct net_device *dev);
} nx_nic_api_t;


/*
 * Return NIC API struct pointer  
 */
extern nx_nic_api_t * __attribute__((weak)) nx_nic_get_api(void);


/*
 * Gets the proc file directory where the procfs files are created.
 *
 * Parameters:
 *	None
 *
 * Returns:
 *	NULL - If the file system is not created.
 *	The directory that was created.
 */

#endif	/* NX_NIC_LINUX_TNIC_API_H */
