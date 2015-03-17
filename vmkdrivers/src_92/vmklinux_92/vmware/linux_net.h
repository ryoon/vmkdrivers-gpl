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
#include "vmklinux_net.h"

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

/* distribued statistics */
struct linnet_qstats {
   struct {
      unsigned long rx_packets;
      unsigned long rx_dropped;
   } ____cacheline_aligned_in_smp;
   struct {
      unsigned long tx_packets;
     unsigned long tx_dropped;
   } ____cacheline_aligned_in_smp;
};

/*
 * NOTE: Try not to put any critical (data path) fields in LinNetDev.
 *       Instead, embed them in net_device, where they are next to
 *       their cache line brethrens.
 */
struct LinNetDev {
   unsigned int       napiNextId; /* Next unique id for napi context. */
   unsigned short     padded;     /* Padding added by alloc_netdev() */
   unsigned long      flags;      /* vmklinux private device flags */
#define NET_VMKLNX_IEEE_DCB 0x1000 /* Used to check driver's IEEE DCB support */
   unsigned long      traceSrcID; /* source id to use when tracing events */
   /* put shared data pointer here to keep net_device size binary compatible */
   vmk_UplinkSharedData      *sharedData;
   vmk_DMAEngine              dmaEngine;
   vmklnx_netdev_vxlan_port_update_callback   vxlanPortUpdateCallback;
   vmklnx_netdev_geneve_port_update_callback  genevePortUpdateCallback;
   unsigned int               geneve_inner_l7_offset_limit;
   unsigned int               geneve_offload_flags;
   struct linnet_qstats      *qstats;

   struct net_device  linNetDev __attribute__((aligned(NETDEV_ALIGN)));
   /*
    * WARNING: linNetDev must be last because it is assumed that
    * private data area follows immediately after.
    */
};

typedef struct LinNetDev LinNetDev;

#define get_LinNetDev(net_device)                                \
   ((LinNetDev*)(((char*)net_device)-(offsetof(struct LinNetDev, linNetDev))))

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

/*
 * Use 1 million as the base and 2 as the starting port number here.
 * The first port gets the pci name.
 * We have duplicated definitions in multiple header files.
 * We have PR 956487 to move these definitions to a common header.
 * Once PR 956487 is addressed, we need to remove all the duplicate
 * definitions.
 */
#define NET_LOGICAL_PORT_START_ID VMK_CONST64U(2)
#define NET_VMKLNX_LOGICAL_BASE        1000000
#define NET_VMKLNX_MAX_DEVS_PER_PF 100

#endif /* _LINUX_NET_H_ */
