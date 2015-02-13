/* ****************************************************************
 * Portions Copyright 2009-2010 VMware, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * ****************************************************************/
#ifndef _VMKLNX26_CNA_H
#define _VMKLNX26_CNA_H

#define	VMKLNX_FCOE_TEMPLATE_NAME		32

struct vmklnx_fcoe_template {
   struct list_head     entry;
   char                 name[VMKLNX_FCOE_TEMPLATE_NAME];
   vmk_ModuleID         module_id;

   /* Only software FCoE can set the PCI ID table to NULL */
   const struct         pci_device_id *id_table; 

   int                  (* fcoe_create) (struct net_device *);
   int                  (* fcoe_destroy) (struct  net_device *);
   int                  (* fcoe_wakeup) (struct  net_device *);
   int                  (* fcoe_recv) (struct sk_buff *);
   int                  (* fip_recv) (struct  sk_buff *);
   /*
    * Register this callback if the L2 driver needs to be notified 
    * upon the VLAN discovery completion.  This callback is optional.
    */
   int                  (* fcoe_vlan_disc_cmpl) (struct  net_device *);
};

int vmklnx_fcoe_attach_transport(struct module *this_module,
                                 struct vmklnx_fcoe_template *fcoe);
int vmklnx_fcoe_release_transport(struct vmklnx_fcoe_template *fcoe);

/**
 * fcoe_attach_transport - add given template to list of FCOE handlers
 * @fcoe: Pointer to fcoe template
 *
 * Adds the given fcoe template to the list of FCOE handlers.
 *
 * RETURN VALUE:
 * 0 upon success; non-zero otherwise
 *
 * ESX Deviation Notes:
 * This API is not present in Linux. This should be called by clients
 * during module load time.
 */
/* _VMKLNX_CODECHECK_: fcoe_attach_transport */
static inline int fcoe_attach_transport (struct vmklnx_fcoe_template *fcoe) {
	return vmklnx_fcoe_attach_transport(THIS_MODULE, fcoe);
}

/**
 * fcoe_release_transport -
 * @fcoe: Pointer to fcoe template
 *
 * Removes the given fcoe template from the list of FCOE handlers.
 *
 * ESX Deviation Notes:
 * This API is not present in Linux. This should be called during module
 * unload time.
 */
/* _VMKLNX_CODECHECK_: fcoe_release_transport */
static inline int fcoe_release_transport (struct vmklnx_fcoe_template *fcoe) {
	return vmklnx_fcoe_release_transport(fcoe);
}

void vmklnx_scsi_attach_cna(struct Scsi_Host *sh, struct net_device *netdev);
void vmklnx_scsi_remove_cna(struct Scsi_Host *sh, struct net_device *netdev);

int vmklnx_cna_set_macaddr(struct net_device *netdev, unsigned char *addr);
int vmklnx_cna_remove_macaddr(struct net_device *netdev, unsigned char *addr);

int vmklnx_cna_queue_xmit(struct sk_buff *skb);

unsigned short vmklnx_cna_get_vlan_tag(struct net_device *netdev);
int vmklnx_cna_set_vlan_tag(struct net_device *dev, unsigned short vid);

int vmklnx_init_fcoe_attribs(struct Scsi_Host *shost, 
                             char *netdevName,
                             unsigned short vid,
                             unsigned char *fcoeControllerMac,
                             unsigned char *vnportMac,
                             unsigned char *fcfMac);

struct vmklnx_cna * vmklnx_cna_lookup_by_name(char *vmnic_name);
vmk_Bool vmklnx_cna_is_dcb_hw_assist(char *vmnic_name);

int vmklnx_cna_cleanup_queues(struct net_device *netdev);
int vmklnx_cna_reinit_queues(struct net_device *netdev);

#endif /* _VMKLNX26_CNA_H */
