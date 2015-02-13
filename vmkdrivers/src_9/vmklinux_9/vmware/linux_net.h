/* ****************************************************************
 * Portions Copyright 2008, 2010 VMware, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * ****************************************************************/

/*
 * linux_net.h --
 *
 *      Linux Network compatibility.
 */

#ifndef _LINUX_NET_H_
#define _LINUX_NET_H_

#include <linux/pci.h>
#include <linux/netdevice.h>

#include "vmkapi.h"

enum {
   LIN_SKB_FLAGS_FRAGSOWNER_VMKLNX   = 0x1,
   LIN_SKB_FLAGS_FRAGSOWNER_VMKERNEL = 0x2,
};

struct LinSkb {
   unsigned short   flags;
   struct sk_buff   skb;
   /*
    * WARNING: skb must be last because it is assumed that the sh_info
    * structure follows immadiately after.
    */
};

typedef struct LinSkb LinSkb;
#define get_LinSkb(skb)                                          \
   ((LinSkb*)(((char*)skb)-(offsetof(struct LinSkb, skb))))

void LinNet_Init(void);
void LinNet_Cleanup(void);
int LinNet_ConnectUplink(struct net_device *dev,
                         struct pci_dev *pdev);
VMK_ReturnStatus LinNet_EnableHwVlan(struct net_device *dev);
VMK_ReturnStatus LinNet_RemoveVlanGroupDevice(void *clientData,
                                              vmk_Bool disable,
                                              void *bitmap);
VMK_ReturnStatus LinNet_NetqueueOp(struct net_device *dev,
                                   vmk_NetqueueOp op,
                                   void *opArgs);
VMK_ReturnStatus LinNet_NetqueueSkbXmit(struct net_device *dev,
                                        vmk_NetqueueQueueID vmkqid,
                                        struct sk_buff *skb);

#endif /* _LINUX_NET_H_ */



