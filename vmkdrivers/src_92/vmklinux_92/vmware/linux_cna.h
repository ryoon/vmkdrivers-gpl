/* ****************************************************************
 * Portions Copyright 2009-2013 VMware, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * ****************************************************************/

#ifndef _LINUX_CNA_H_
#define _LINUX_CNA_H_

#include "cna_ioctl.h"

#define CNA_MAX_QUEUE				01
#define CNA_MAX_FILTER_PER_QUEUE		02

/*
 * Provide information about rx filters
 */
struct rx_filter_info {
   vmk_NetqueueFilterID   id;
   int			  ref; /* Number of references on this mac */
   unsigned char 	  mac_addr[6]; 
};

/*
 * Per rx queue specific structure
 */
struct rx_queue {
   vmk_NetqueueQueueID 	  rx_queue_id;
   struct rx_filter_info  filter[CNA_MAX_FILTER_PER_QUEUE];
   vmk_Bool		  active;
};

/*
 * Per tx queue specific structure
 */
struct tx_queue {
   vmk_NetqueueQueueID 	  tx_queue_id;
   vmk_Bool		  active;
};

/*
 * CNA internal structure
 */
struct vmklnx_cna {
   struct list_head 	  entry;
   struct net_device 	  *netdev;

   /*
    * Lock to protect queue operation - Non perf path
    */
   spinlock_t             lock;

   /*
    * TX and RX queues are allocated during init and
    * are not touched upon during run time
    * However RX filters can be set dynamically
    */
   struct tx_queue        tqueue[CNA_MAX_QUEUE];
   struct rx_queue        rqueue[CNA_MAX_QUEUE];

   /*
    * VLAN ID to use for FCOE traffic.
    */
   vmk_VlanID             fcoe_vlan_id;

   /*
    * 802.1p priority number for FCOE application.
    */
   vmk_VlanPriority       fcoe_app_prio;

   /*
    * Place for generic receive handler for adapters that want to receive
    * frames on CNA and do not register with the FCoE template
    */
   int (*rx_handler)(struct sk_buff *skb, void *handle);
   void *handle;

#define CNA_FCOE_VN2VN    0x0001
   unsigned short         flags;
};

struct vmklnx_fcoe_contlr {
   struct vmklnx_cna           *cna;
   struct vmklnx_fcoe_template *fcoe;
   atomic_t                    vnports;
}; 

extern VMK_ReturnStatus LinuxCNA_RegisterNetDev(struct net_device *netdev);
extern VMK_ReturnStatus LinuxCNA_UnRegisterNetDev(struct net_device *netdev);

void LinuxCNA_Poll(vmk_PktList rxPktList, vmk_AddrCookie cookie);
void LinuxCNADev_Poll(vmk_PktList rxPktList, vmk_AddrCookie cookie);
extern void LinuxCNA_RxProcess(vmk_PktList pktList, 
				struct net_device *netdev);
extern void CNAProcessDCBRequests(struct fcoe_ioctl_pkt *fcoe_ioctl);
extern void LinuxDCB_Init(void);
extern void LinuxDCB_Cleanup(void);

#endif /*_LINUX_CNA_H_ */
