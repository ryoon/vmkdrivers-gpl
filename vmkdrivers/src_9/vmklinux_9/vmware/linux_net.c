/* ****************************************************************
 * Portions Copyright 2005, 2009-2011 VMware, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * ****************************************************************/

/******************************************************************
 *
 *  linux_net.c
 *
 * From linux-2.6.24-7/include/linux/netdevice.h:
 *
 * Authors:     Ross Biro
 *              Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *              Corey Minyard <wf-rch!minyard@relay.EU.net>
 *              Donald J. Becker, <becker@cesdis.gsfc.nasa.gov>
 *              Alan Cox, <Alan.Cox@linux.org>
 *              Bjorn Ekwall. <bj0rn@blox.se>
 *              Pekka Riikonen <priikone@poseidon.pspt.fi>
 *
 * From linux-2.6.27-rc9/net/core/dev.c:
 *
 * Authors:     Ross Biro
 *              Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *              Mark Evans, <evansmp@uhura.aston.ac.uk>
 *
 * Additional Authors:
 *              Florian la Roche <rzsfl@rz.uni-sb.de>
 *              Alan Cox <gw4pts@gw4pts.ampr.org>
 *              David Hinds <dahinds@users.sourceforge.net>
 *              Alexey Kuznetsov <kuznet@ms2.inr.ac.ru>
 *              Adam Sulmicki <adam@cfar.umd.edu>
 *              Pekka Riikonen <priikone@poesidon.pspt.fi>
 *
 * From linux-2.6.27-rc9/net/sched/sch_generic.c:
 *
 * Authors:     Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *              Jamal Hadi Salim, <hadi@cyberus.ca> 990601
 *
 ******************************************************************/

#define NET_DRIVER      // Special case for Net portion of VMKLINUX

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/if_vlan.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/ethtool.h>
#include <linux/rtnetlink.h> /* BUG_TRAP */
#include <linux/workqueue.h>
#include <linux/dma-mapping.h>
#include <asm/uaccess.h>
#include <asm/page.h> /* phys_to_page */

#include "vmkapi.h"
#include "linux_stubs.h"
#include "linux_pci.h"
#include "linux_stress.h"
#include "linux_task.h"
#include "linux_net.h"
#include "linux_cna.h"
#include "linux_dcb.h"

#include <vmkplexer_chardevs.h>

#define VMKLNX_LOG_HANDLE LinNet
#include "vmklinux_log.h"

/* default watchdog timeout value and timer period for device */
#define WATCHDOG_DEF_TIMEO 5 * HZ
#define WATCHDOG_DEF_TIMER 1000


enum {
   LIN_NET_HARD_QUEUE_XOFF = 0x0001,   /* hardware queue is stopped */
};

/*
 * NOTE: Try not to put any critical (data path) fields in LinNetDev.
 *       Instead, embed them in net_device, where they are next to
 *       their cache line brethrens.
 */
struct LinNetDev {
   unsigned int       napiNextId; /* Next unique id for napi context. */
   unsigned long      flags;      /* vmklinux private device flags */
   unsigned short     padded;     /* Padding added by alloc_netdev() */
   struct net_device  linNetDev __attribute__((aligned(NETDEV_ALIGN)));
   /*
    * WARNING: linNetDev must be last because it is assumed that
    * private data area follows immediately after.
    */
};

typedef struct LinNetDev LinNetDev;
typedef int (*PollHandler) (void* clientData, vmk_uint32 vector);

#define get_LinNetDev(net_device)                                \
   ((LinNetDev*)(((char*)net_device)-(offsetof(struct LinNetDev, linNetDev))))

static vmk_Timer  devWatchdogTimer;
static void link_state_work_cb(struct work_struct *work);
static void watchdog_work_cb(struct work_struct *work);
static struct delayed_work linkStateWork;
static struct delayed_work watchdogWork;
static unsigned      linkStateTimerPeriod;
static vmk_ConfigParamHandle linkStateTimerPeriodConfigHandle;
static vmk_ConfigParamHandle maxNetifTxQueueLenConfigHandle;
static unsigned blockTotalSleepMsec;
static vmk_ConfigParamHandle blockTotalSleepMsecHandle;
struct net_device    *dev_base = NULL;
EXPORT_SYMBOL(dev_base);
DEFINE_RWLOCK(dev_base_lock);
int                  netdev_max_backlog = 300;
static const unsigned eth_crc32_poly_le = 0xedb88320;
static unsigned      eth_crc32_poly_tbl_le[256];
static uint64_t max_phys_addr;

static vmk_ConfigParamHandle useHwIPv6CsumHandle;
static vmk_ConfigParamHandle useHwCsumForIPv6CsumHandle;
static vmk_ConfigParamHandle useHwTSO6Handle;
static vmk_ConfigParamHandle useHwTSOHandle;

/*
 * The global packet list for receiving packets when the system is in
 * the panic/debug status.
 */
static vmk_PktList debugPktList = NULL;

/* Stress option handles */
static vmk_StressOptionHandle stressNetGenTinyArpRarp;
static vmk_StressOptionHandle stressNetIfCorruptEthHdr;
static vmk_StressOptionHandle stressNetIfCorruptRxData;
static vmk_StressOptionHandle stressNetIfCorruptRxTcpUdp;
static vmk_StressOptionHandle stressNetIfCorruptTx;
static vmk_StressOptionHandle stressNetIfFailHardTx;
static vmk_StressOptionHandle stressNetIfFailRx;
static vmk_StressOptionHandle stressNetIfFailTxAndStopQueue;
static vmk_StressOptionHandle stressNetIfForceHighDMAOverflow;
static vmk_StressOptionHandle stressNetIfForceRxSWCsum;
static vmk_StressOptionHandle stressNetNapiForceBackupWorldlet;
static vmk_StressOptionHandle stressNetBlockDevIsSluggish;

/* LRO config option */
static vmk_ConfigParamHandle vmklnxLROEnabledConfigHandle;
static vmk_ConfigParamHandle vmklnxLROMaxAggrConfigHandle;
unsigned int vmklnxLROEnabled;
unsigned int vmklnxLROMaxAggr;

extern void LinStress_SetupStress(void);
extern void LinStress_CleanupStress(void);
extern void LinStress_CorruptSkbData(struct sk_buff*, unsigned int,
   unsigned int);
extern void LinStress_CorruptRxData(vmk_PktHandle*, struct sk_buff *);
extern void LinStress_CorruptEthHdr(struct sk_buff *skb);

static VMK_ReturnStatus map_pkt_to_skb(struct net_device *dev,
                                       struct netdev_queue *queue,
                                       vmk_PktHandle *pkt,
                                       struct sk_buff **pskb);
static void do_free_skb(struct sk_buff *skb);
static struct sk_buff *do_alloc_skb(kmem_cache_t *skb, gfp_t flags);
static VMK_ReturnStatus BlockNetDev(void *clientData);
static void SetNICLinkStatus(struct net_device *dev);
static VMK_ReturnStatus skb_gen_pkt_frags(struct sk_buff *skb);

static inline VMK_ReturnStatus
marshall_from_vmknetq_id(vmk_NetqueueQueueID vmkqid,
                         vmknetddi_queueops_queueid_t *qid);
static ATOMIC_NOTIFIER_HEAD(netdev_notifier_list);
static vmk_Bool napi_poll(void *ptr);

inline void vmklnx_set_skb_frags_owner_vmkernel(struct sk_buff *);

/*
 * Deal with the transition away from exposing vmk_Worldlet and
 * vmk_Uplink* directly through the vmklnx headers.
 */
VMK_ASSERT_LIST(VMKLNX_NET,
   VMK_ASSERT_ON_COMPILE(sizeof(vmk_Worldlet) == sizeof(void *));
   VMK_ASSERT_ON_COMPILE(sizeof(vmk_LinkState) ==
                         sizeof(vmklnx_uplink_link_state));
   VMK_ASSERT_ON_COMPILE(sizeof(vmk_UplinkPTOpFunc) ==
                         sizeof(void *));
   VMK_ASSERT_ON_COMPILE(sizeof(vmk_NetqueueQueueID) ==
                         sizeof(vmk_uint64));
   VMK_ASSERT_ON_COMPILE(VMKLNX_UPLINK_LINK_DOWN == VMK_LINK_STATE_DOWN);
   VMK_ASSERT_ON_COMPILE(VMKLNX_UPLINK_LINK_UP == VMK_LINK_STATE_UP);
   VMK_ASSERT_ON_COMPILE(sizeof(vmk_UplinkWatchdogPanicModState) == 
                         sizeof(vmklnx_uplink_watchdog_panic_mod_state));
   VMK_ASSERT_ON_COMPILE(sizeof(vmk_UplinkWatchdogPanicModState) == 
                         sizeof(vmklnx_uplink_watchdog_panic_mod_state));
   VMK_ASSERT_ON_COMPILE(VMKLNX_UPLINK_WATCHDOG_PANIC_MOD_DISABLE == 
                         VMK_UPLINK_WATCHDOG_PANIC_MOD_DISABLE);
   VMK_ASSERT_ON_COMPILE(VMKLNX_UPLINK_WATCHDOG_PANIC_MOD_ENABLE ==
                         VMK_UPLINK_WATCHDOG_PANIC_MOD_ENABLE);
   VMK_ASSERT_ON_COMPILE(sizeof(vmk_NetqueueQueueID) == sizeof(vmk_uint64));
   VMK_ASSERT_ON_COMPILE(VMKLNX_PKT_HEAP_MAX_SIZE == VMK_PKT_HEAP_MAX_SIZE);
)

/*
 * Section: Receive path
 */

/*
 *----------------------------------------------------------------------------
 *
 *  map_skb_to_pkt --
 *
 *    Converts sk_buff to PktHandle before handing packet to vmkernel.
 *
 *  Results:
 *    NET_RX_SUCCESS on success; NET_RX_DROP if the packet is dropped.
 *
 *  Side effects:
 *    Drops packet on the floor if unsuccessful.
 *
 *----------------------------------------------------------------------------
 */
static int
map_skb_to_pkt(struct sk_buff *skb)
{
   VMK_ReturnStatus status;
   vmk_PktHandle *pkt = NULL;
   struct net_device *dev = skb->dev;

   /* we need to ensure the blocked status */
   if (unlikely(test_bit(__LINK_STATE_BLOCKED, &dev->state))) {
      VMK_ASSERT(!(dev->features & NETIF_F_CNA));
      goto drop;
   }
   if (unlikely(skb->len == 0)) {
      static uint32_t logThrottleCounter = 0;
      VMKLNX_THROTTLED_INFO(logThrottleCounter,
                            "dropping zero length packet "
                            "(skb->len=%u, skb->data_len=%u)\n",
                            skb->len, skb->data_len);
      VMK_ASSERT(!(dev->features & NETIF_F_CNA));
      goto drop;
   }

   if (unlikely(skb_gen_pkt_frags(skb) != VMK_OK)) {
      VMK_ASSERT(!(dev->features & NETIF_F_CNA));
      goto drop;
   }
   pkt = skb->pkt;

   if (unlikely(vmk_PktFrameLenSet(pkt, skb->len) != VMK_OK)) {
      printk("unable to set skb->pkt %p frame length with skb->len = %u\n",
             pkt, skb->len);
      VMK_ASSERT(VMK_FALSE);
      goto drop;
   }

   if (skb_shinfo(skb)->gso_type != 0) {
      switch (skb_shinfo(skb)->gso_type) {
      case SKB_GSO_TCPV4:
         if (unlikely(skb_shinfo(skb)->gso_size == 0)) {
            printk("dropping LRO packet with zero gso_size\n");
            VMK_ASSERT(VMK_FALSE);
            goto drop;
         }

         status = vmk_PktSetLargeTcpPacket(pkt, skb_shinfo(skb)->gso_size);
         VMK_ASSERT(status == VMK_OK);
         break;
      default:
         printk("unable to process gso type 0x%x on the rx path\n",
                skb_shinfo(skb)->gso_type);
         VMK_ASSERT(VMK_FALSE);
         goto drop;
      }
   }

   /*
    * The following extracts vlan tag from skb.
    * The check just looks at a field of skb, so we
    * don't bother to check whether vlan is enabled.
    */
   if (vlan_rx_tag_present(skb)) {
      VMK_ASSERT(vmk_PktVlanIDGet(pkt) == 0);

      if  ((vlan_rx_tag_get(skb) & VLAN_VID_MASK) > VLAN_MAX_VALID_VID) {
         static uint32_t logThrottleCounter = 0;
         VMKLNX_THROTTLED_INFO(logThrottleCounter,
                               "invalid vlan tag: %d dropped",
                               vlan_rx_tag_get(skb) & VLAN_VID_MASK);
         VMK_ASSERT(!(dev->features & NETIF_F_CNA));
         goto drop;
      }
      status = vmk_PktVlanIDSet(pkt, vlan_rx_tag_get(skb) & VLAN_VID_MASK);
      VMK_ASSERT(status == VMK_OK);
      status = vmk_PktPrioritySet(pkt,
                                  (vlan_rx_tag_get(skb) & VLAN_1PTAG_MASK) >> VLAN_1PTAG_SHIFT);
      VMK_ASSERT(status == VMK_OK);
      VMKLNX_DEBUG(2, "%s: rx vlan tag %u present with priority %u",
                   dev->name, vmk_PktVlanIDGet(pkt), vmk_PktPriorityGet(pkt));

#ifdef VMX86_DEBUG
      {
         // generate arp/rarp frames that are < ETH_MIN_FRAME_LEN to
         // create test cases for PR 106153.
	 struct ethhdr *eh = (struct ethhdr *)vmk_PktFrameMappedPointerGet(pkt);

         if ((eh->h_proto == ntohs(ETH_P_ARP)
	      || eh->h_proto == ntohs(ETH_P_RARP))
	     && VMKLNX_STRESS_DEBUG_COUNTER(stressNetGenTinyArpRarp)) {
            int old_frameMappedLen;
            int target_len = (ETH_ZLEN - VLAN_HLEN);

	    old_frameMappedLen = vmk_PktFrameMappedLenGet(pkt);

            if (target_len <= old_frameMappedLen) {
               int old_len;
	       int len;

	       old_len = vmk_PktFrameLenGet(pkt);
               vmk_PktFrameLenSet(pkt, target_len);
	       len = vmk_PktFrameLenGet(pkt);
               VMKLNX_DEBUG(1, "shorten arp/rarp pkt to %d from %d",
                            len, old_len);
            }
         }
      }
#endif
   }

   if (skb->ip_summed != CHECKSUM_NONE &&
       !VMKLNX_STRESS_DEBUG_OPTION(stressNetIfForceRxSWCsum)) {
      status = vmk_PktSetCsumVfd(pkt);
      VMK_ASSERT(status == VMK_OK);
   }

   if (likely(!(dev->features & NETIF_F_CNA))) {
      if (VMKLNX_STRESS_DEBUG_COUNTER(stressNetIfCorruptRxTcpUdp)) {
         LinStress_CorruptSkbData(skb, 40, 14);
      }
      if (VMKLNX_STRESS_DEBUG_COUNTER(stressNetIfCorruptRxData)) {
         LinStress_CorruptRxData(pkt, skb);
      }
      if (VMKLNX_STRESS_DEBUG_COUNTER(stressNetIfCorruptEthHdr)) {
         LinStress_CorruptEthHdr(skb);
      }
   }

   dev->linnet_rx_packets++;

   if (!(dev->features & NETIF_F_CNA)) {
      do_free_skb(skb);
   } else {
      /*
       * Packets received for FCOE will be free'd by the OpenFCOE stack.
       */
      vmk_PktSetCompletionData(pkt, skb, dev->genCount, VMK_TRUE);
   }
   return NET_RX_SUCCESS;

 drop:
   dev_kfree_skb_any(skb);
   dev->linnet_rx_dropped++;
   VMK_ASSERT(!(dev->features & NETIF_F_CNA));
   return NET_RX_DROP;
}

/**
 * netif_rx - post buffer to the network code
 * @skb: buffer to post
 *
 * This function receives a packet from a device driver and queues it for
 * the upper (protocol) levels to process.  It always succeeds. The buffer
 * may be dropped during processing for congestion control or by the
 * protocol layers.
 *
 *  RETURN VALUE:
 *  NET_RX_SUCCESS (no congestion)
 *  NET_RX_DROP	(packet was dropped)
 *
 */
/* _VMKLNX_CODECHECK_: netif_rx */
int
netif_rx(struct sk_buff *skb)
{
   struct net_device *dev = skb->dev;
   vmk_PktHandle *pkt;
   int status;

   VMK_ASSERT(dev);

   VMKLNX_DEBUG(1, "Napi is not enabled for device %s\n", dev->name);

   pkt = skb->pkt;
   VMK_ASSERT(pkt);

   status = map_skb_to_pkt(skb);
   if (likely(status == NET_RX_SUCCESS)) {
      vmk_PktQueueForRxProcess(pkt, dev->uplinkDev);
   }

   return status;
}
EXPORT_SYMBOL(netif_rx);

/**
 * netif_receive_skb - process receive buffer from network
 * @skb: buffer to process
 *
 * netif_receive_skb() is the main receive data processing function.
 * It always succeeds. The buffer may be dropped during processing
 * for congestion control or by the protocol layers.
 *
 * ESX Deviation Notes:
 * This function may only be called from the napi poll callback routine.
 *
 *  RETURN VALUE:
 *  NET_RX_SUCCESS (no congestion)
 *  NET_RX_DROP	(packet was dropped)
 */
/* _VMKLNX_CODECHECK_: netif_receive_skb */
int
netif_receive_skb(struct sk_buff *skb)
{
   struct net_device *dev = skb->dev;
   vmk_NetPoll pollPriv;
   vmk_Worldlet wdt;
   struct napi_struct *napi = NULL;
   vmk_PktHandle *pkt;
   int status;

   VMK_ASSERT(dev);

   /*
    * When the system is not in the panic/debug status, put the arrived packets into
    * skb->napi->rxPktList.
    */
   if (skb->napi == NULL) {
      if (unlikely(vmk_WorldletGetCurrent(&wdt, (void **)&pollPriv) != VMK_OK)) {
         VMK_ASSERT(VMK_FALSE);
         dev_kfree_skb_any(skb);
         dev->linnet_rx_dropped++;
         status = NET_RX_DROP;
         goto done;
      } else {
         /*
          * When the system is in the panic/debug status, the current worldlet is the
          * debug worldlet rather than the napi_poll worldlet. In this case, put the
          * arrived packets into debugPktList. This list will be processed by
          * FlushRxBuffers, because netdump/netdebug will bypass the vswitch to read
          * the packets.
         */
         if  (vmk_NetPollGetCurrent(&pollPriv) == VMK_OK) { 
            napi = (struct napi_struct *)vmk_NetPollGetPrivate(pollPriv);
         }
         if (!napi || vmk_SystemCheckState(VMK_SYSTEM_STATE_PANIC)) {
            pkt = skb->pkt;
            status = map_skb_to_pkt(skb);
            if (likely(status == NET_RX_SUCCESS)) {
               if (debugPktList == NULL) {
                  if (vmk_PktListAlloc(&debugPktList) != VMK_OK) {
                     dev_kfree_skb_any(skb);
                     dev->linnet_rx_dropped++;
                     status = NET_RX_DROP;
                     goto done;
                  }
                  vmk_PktListInit(debugPktList);
               }
               VMK_ASSERT(pkt);
               vmk_PktListAppendPkt(debugPktList, pkt);
            }
            goto done;
         } else {
            VMK_ASSERT(pollPriv != NULL);
            skb->napi = napi;
         }
      }
   }

   VMK_ASSERT(skb->napi != NULL);
   VMK_ASSERT(skb->napi->dev == skb->dev);

   pkt = skb->pkt;
   napi = skb->napi;
   
   status = map_skb_to_pkt(skb);
   if (likely(status == NET_RX_SUCCESS)) {
      VMK_ASSERT(napi);
      VMK_ASSERT(pkt);
      vmk_NetPollQueueRxPkt(napi->net_poll, pkt);
   }

 done:
   return status;
}
EXPORT_SYMBOL(netif_receive_skb);



/*
 *----------------------------------------------------------------------------
 *
 *  napi_poll --
 *
 *    Callback registered with the net poll handler.
 *    This handler is responsible of polling the different napi context.
 *    
 *  Results:
 *    VMK_TRUE if we need to keep polling and VMK_FALSE otherwise.
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static vmk_Bool
napi_poll(void *ptr) 
{
   VMK_ReturnStatus status = VMK_OK;
   struct napi_struct *napi = (struct napi_struct *)ptr;

   /*
    * napi_schedule_prep()/napi_schedule() depend on accurately seeing whether
    * or not the worldlet is running and assume that the check for polling
    * executes only after the worldlet has been dispatched.  If the CPU
    * aggressively prefetches the test_bit() load here so that it occurs
    * prior to the worldlet being dispatched then __napi_schedule() could
    * avoid kicking the worldlet (seeing that it had not yet run), but at
    * the same time the aggressive prefetch would result in us seeing a
    * clear napi->state and returning VMK_WDT_SUSPEND from here.
    * Consequently an smp_mb() is required here; we need to ensure that none of
    * our loads here occur prior to any stores that may have occurred by the
    * caller of this function.
    */
   smp_mb();
 
   if (test_bit(NAPI_STATE_SCHED, &napi->state)) {
      VMKAPI_MODULE_CALL(napi->dev->module_id, status, napi->poll, napi,
                         napi->weight);
      if (vmklnxLROEnabled && !(napi->dev->features & NETIF_F_SW_LRO)) {
         /* Flush all the lro sessions as we are done polling the napi context */
         lro_flush_all(&napi->lro_mgr);
      }
   }

   if (test_bit(NAPI_STATE_SCHED, &napi->state)) {
      return VMK_TRUE;
   } else {
      return VMK_FALSE;
   }
}
 

/*
 *----------------------------------------------------------------------------
 *
 *  netdev_poll --
 *
 *    Callback registered for the devices that are unable to create their own
 *    poll. This handler is responsible of polling the different napi context.
 *
 *  Results:
 *    VMK_TRUE if we need to keep polling and VMK_FALSE otherwise.
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static vmk_Bool
netdev_poll(void *private)
{
   struct net_device *dev = private;
   vmk_Bool needWork;
   struct napi_struct *napi;
   VMK_ReturnStatus status = VMK_OK;

   needWork = VMK_FALSE;

   spin_lock(&dev->napi_lock);
   list_for_each_entry(napi, &dev->napi_list, dev_list) {
      if (napi->dev_poll &&
         (test_bit(NAPI_STATE_SCHED, &napi->state))) {
         needWork = VMK_TRUE;
         list_move_tail(&napi->dev_list, &dev->napi_list);
         break;
      }
   }
   spin_unlock(&dev->napi_lock);

   if (!needWork) {
      return VMK_FALSE;
   }

   VMKAPI_MODULE_CALL(napi->dev->module_id, status, napi->poll, napi,
                      napi->weight);
   if (vmklnxLROEnabled && !(napi->dev->features & NETIF_F_SW_LRO)) {
      /* Flush all the lro sessions as we are done polling the napi context */
      lro_flush_all(&napi->lro_mgr);
   }

   return VMK_TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 *  napi_poll_init --
 *
 *    Initialize a napi context . If the function is unable to create a unique
 *    net poll it will attach the napi context to the one provided by the
 *    device it belongs to.
 *
 *  Results:
 *    VMK_OK on success, VMK_NO_MEMORY if resources could not be allocated.
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
napi_poll_init(struct napi_struct *napi)
{
   VMK_ReturnStatus ret;
   vmk_ServiceAcctID serviceID;
   vmk_NetPollProperties pollInit;

   spin_lock(&napi->dev->napi_lock);
   napi->napi_id = get_LinNetDev(napi->dev)->napiNextId++;
   spin_unlock(&napi->dev->napi_lock);

   ret = vmk_ServiceGetID("netdev", &serviceID);
   VMK_ASSERT(ret == VMK_OK);

   napi->napi_wdt_priv.dev = napi->dev;
   napi->napi_wdt_priv.napi = napi;
   napi->dev_poll = VMK_FALSE;
   napi->vector = 0;

   pollInit.poll = napi_poll;
   pollInit.priv = napi;

   if (napi->dev->features & NETIF_F_CNA) {
      pollInit.deliveryCallback = LinuxCNA_Poll;
      pollInit.features = VMK_NETPOLL_CUSTOM_DELIVERY_CALLBACK;
   } else {
      pollInit.deliveryCallback = NULL;
      pollInit.features = VMK_NETPOLL_NONE;
   }
   ret = vmk_NetPollInit(&pollInit, serviceID, (vmk_NetPoll *)&napi->net_poll) ;
   if (ret != VMK_OK) {
      VMKLNX_WARN("Unable to create net poll for %s, using backup",
                  napi->dev->name);
      if (napi->dev->reg_state == NETREG_REGISTERED) {
         napi->net_poll = napi->dev->net_poll;
         napi->net_poll_type = NETPOLL_BACKUP;

         /*
          * Use device global net poll for polling this napi_struct,
          * if net poll creation fails
          */
         napi->dev_poll = VMK_TRUE;
      } else {
         napi->dev->reg_state = NETREG_EARLY_NAPI_ADD_FAILED;
         return VMK_FAILURE;
      }
   } else {
      napi->net_poll_type = NETPOLL_DEFAULT;
   }

   if (napi->dev->uplinkDev) {
      vmk_Name pollName;
      (void) vmk_NameFormat(&pollName, "-%d", napi->napi_id);
      vmk_NetPollRegisterUplink(napi->net_poll, napi->dev->uplinkDev, pollName, VMK_TRUE);
   }

   spin_lock(&napi->dev->napi_lock);
   list_add(&napi->dev_list, &napi->dev->napi_list);
   spin_unlock(&napi->dev->napi_lock);

   /*
    * Keep track of which poll is (most probably) driving the
    * default queue. For netqueue capable nics, we call
    * VMKNETDDI_QUEUEOPS_OP_GET_DEFAULT_QUEUE to figure out the
    * default poll. For non-netqueue nics, the first suceessful
    * netif_napi_add wins.
    */
   if (!napi->dev->default_net_poll && napi->net_poll) {
      napi->dev->default_net_poll = napi->net_poll;
   }

   return VMK_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 *  netdev_poll_init --
 *
 *    Initialize a device's backup net poll for the napi context not able to
 *    create their own.
 *
 *  Results:
 *    VMK_OK if everything is ok, VMK_* otherwise.
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
netdev_poll_init(struct net_device *dev)
{
   VMK_ReturnStatus ret;
   vmk_ServiceAcctID serviceID;
   vmk_NetPollProperties pollInit;

   VMK_ASSERT(dev);

   ret = vmk_ServiceGetID("netdev", &serviceID);
   VMK_ASSERT(ret == VMK_OK);

   dev->napi_wdt_priv.dev = dev;
   dev->napi_wdt_priv.napi = NULL;
 
   pollInit.poll = netdev_poll;
   pollInit.priv = dev;

   if (dev->features & NETIF_F_CNA) {
      pollInit.deliveryCallback = LinuxCNADev_Poll;
      pollInit.features = VMK_NETPOLL_CUSTOM_DELIVERY_CALLBACK;
   } else {
      pollInit.deliveryCallback = NULL;
      pollInit.features = VMK_NETPOLL_NONE;
   }

   ret = vmk_NetPollInit(&pollInit, serviceID, (vmk_NetPoll *)&dev->net_poll) ;
   return ret;
}

/*
 *----------------------------------------------------------------------------
 *
 *  napi_poll_cleanup --
 *
 *    Cleanup a napi structure.
 *
 *  Results:
 *    None.
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static void
napi_poll_cleanup(struct napi_struct *napi)
{
   VMK_ASSERT(napi);

   if (napi->net_poll == napi->dev->default_net_poll) {
      napi->dev->default_net_poll = NULL;
   }

   if (likely(!napi->dev_poll)) {
      if (napi->vector) {
         vmk_NetPollVectorUnSet(napi->net_poll);
         napi->vector = 0;
      }

      if (napi->net_poll) {
         vmk_NetPollCleanup(napi->net_poll);
         napi->net_poll = NULL;
      }
   }
   list_del_init(&napi->dev_list);
}

/*
 *----------------------------------------------------------------------------
 *
 *  netdev_poll_cleanup --
 *
 *    Cleanup all napi structures associated with the device.
 *
 *  Results:
 *    None.
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static void
netdev_poll_cleanup(struct net_device *dev)
{
   VMK_ASSERT(dev);
   struct list_head *ele, *next;
   struct napi_struct *napi;

   /*
    * Cleanup all napi structs
    */
   list_for_each_safe(ele, next, &dev->napi_list) {
      napi = list_entry(ele, struct napi_struct, dev_list);
      napi_poll_cleanup(napi);
   }

   if (dev->net_poll) {
      vmk_NetPollCleanup(dev->net_poll);
      dev->net_poll = NULL;
   }
}

/**
 * __napi_schedule - schedule for receive
 * @napi: entry to schedule
 *
 * The entry's receive function will be scheduled to run
 *
 * RETURN VALUE:
 * None
 */
/* _VMKLNX_CODECHECK_: __napi_schedule */
void
__napi_schedule(struct napi_struct *napi)
{
   vmk_uint32 myVector = 0;
   vmk_Bool inIntr = vmk_ContextIsInterruptHandler(&myVector);
   VMK_ASSERT(napi);

   if (unlikely(napi->vector != myVector)) {
      if (likely(inIntr)) {
         vmk_NetPollVectorSet(napi->net_poll, myVector);
         napi->vector = myVector;
      }
   }
 
   vmk_NetPollActivate(napi->net_poll);

}
EXPORT_SYMBOL(__napi_schedule);

/**
 *      napi_disable_timeout - prevent NAPI from scheduling
 *      @napi: napi context
 *
 * Stop NAPI from being scheduled on this context.
 * Waits till any outstanding processing completes
 *
 * RETURN VALUE:
 * None
 */
static vmk_Bool
napi_disable_timeout(struct napi_struct *napi, int timeout)
{
   VMK_ReturnStatus status;
   vmk_NetPollState state;
   vmk_Bool doTimeout=(timeout == -1)?VMK_FALSE:VMK_TRUE;
   vmk_Bool timedOut=VMK_TRUE;

   VMK_ASSERT(napi);

   while (timeout) {
      set_bit(NAPI_STATE_DISABLE, &napi->state);
      status = vmk_NetPollCheckState(napi->net_poll, &state);
      VMK_ASSERT(status == VMK_OK);
      /* If the poll isn't running/set to run, then we see if we can
       * disable it from running in the future by blocking off
       * NAPI_STATE_SCHED.
       */
      if (state == VMK_NETPOLL_DISABLED  &&
          !test_and_set_bit(NAPI_STATE_SCHED, &napi->state)) {
         vmk_NetPollCheckState(napi->net_poll, &state);
         VMK_ASSERT(state == VMK_NETPOLL_DISABLED);
         timedOut=VMK_FALSE;
         break;
      }
      /**
       * Give the flush a chance to run.
       */
      schedule_timeout_interruptible(1);
      if (doTimeout) {
         timeout--;
      }
   }
   if (!timedOut) {
      set_bit(NAPI_STATE_UNUSED, &napi->state);
   }

   if (napi->vector) {  
      vmk_NetPollVectorUnSet(napi->net_poll);
      napi->vector = 0;
   }

   clear_bit (NAPI_STATE_DISABLE, &napi->state);
   return timedOut;
}

/**
 *      napi_disable - prevent NAPI from scheduling
 *      @napi: napi context
 *
 * Stop NAPI from being scheduled on this context.
 * Waits till any outstanding processing completes.
 *
 * RETURN VALUE:
 * None
 */
/* _VMKLNX_CODECHECK_: napi_disable */
void
napi_disable(struct napi_struct *napi)
{
   napi_disable_timeout (napi, -1);
}
EXPORT_SYMBOL(napi_disable);

/**
 * netif_napi_add - initialize a napi context
 * @dev:  network device
 * @napi: napi context
 * @poll: polling function
 * @weight: default weight
 *
 * netif_napi_add() must be used to initialize a napi context prior to calling
 * *any* of the other napi related functions.
 *
 * RETURN VALUE:
 * None
 */
/* _VMKLNX_CODECHECK_: netif_napi_add */
void
netif_napi_add(struct net_device *dev,
               struct napi_struct *napi,
               int (*poll)(struct napi_struct *, int),
               int weight)
{
        struct net_lro_mgr *lro_mgr;

        napi->poll = poll;
        napi->weight = weight;
        napi->dev = dev;

        lro_mgr = &napi->lro_mgr;
        lro_mgr->dev = dev;
        lro_mgr->features = LRO_F_NAPI;
        lro_mgr->ip_summed = CHECKSUM_UNNECESSARY;
        lro_mgr->ip_summed_aggr = CHECKSUM_UNNECESSARY;
        lro_mgr->max_desc = LRO_DEFAULT_MAX_DESC;
        lro_mgr->lro_arr = napi->lro_desc;
        lro_mgr->get_skb_header = vmklnx_net_lro_get_skb_header;
        lro_mgr->get_frag_header = NULL;
        lro_mgr->max_aggr = vmklnxLROMaxAggr;
        lro_mgr->frag_align_pad = 0;

        napi_poll_init(napi);

        set_bit(NAPI_STATE_SCHED, &napi->state);
        set_bit(NAPI_STATE_UNUSED, &napi->state); 
}
EXPORT_SYMBOL(netif_napi_add);

/**
 *  netif_napi_del - remove a napi context
 *  @napi: napi context
 *
 *  netif_napi_del() removes a napi context from the network device napi list
 *
 * RETURN VALUE:
 * None
 */
/* _VMKLNX_CODECHECK_: netif_napi_del */
void
netif_napi_del(struct napi_struct *napi)
{
   napi_poll_cleanup(napi);
}
EXPORT_SYMBOL(netif_napi_del);

/**
 *      napi_enable - enable NAPI scheduling
 *      @n: napi context
 *
 * Resume NAPI from being scheduled on this context.
 * Must be paired with napi_disable.
 *
 * RETURN VALUE:
 * None
 */
/* _VMKLNX_CODECHECK_: napi_enable */
void
napi_enable(struct napi_struct *napi)
{
   struct net_lro_mgr *lro_mgr;
   int idx;

   BUG_ON(!test_bit(NAPI_STATE_SCHED, &napi->state));

   lro_mgr = &napi->lro_mgr;
   for (idx = 0; idx < lro_mgr->max_desc; idx++) {
      memset(&napi->lro_desc[idx], 0, sizeof(struct net_lro_desc));
   }

   smp_mb__before_clear_bit();
   clear_bit(NAPI_STATE_SCHED, &napi->state);
   clear_bit(NAPI_STATE_UNUSED, &napi->state);  
}
EXPORT_SYMBOL(napi_enable);


/*
 * Section: Skb helpers
 */

/*
 *----------------------------------------------------------------------------
 *
 *  skb_append_frags_to_pkt --
 *
 *    Append skb frags to the packet handle associated to it.
 *
 *  Results:
 *    VMK_OK on success; VMK_* otherwise.
 *
 *  Side effects:
 *    Drops packet on the floor if unsuccessful.
 *
 *----------------------------------------------------------------------------
 */
static inline VMK_ReturnStatus
skb_append_frags_to_pkt(struct sk_buff *skb)
{
   VMK_ReturnStatus status = VMK_OK;
   int i;

   for (i = 0; i < skb_shinfo(skb)->nr_frags; i++) {
      skb_frag_t *frag = &skb_shinfo(skb)->frags[i];

      status = vmk_PktAppendFrag(skb->pkt,
                                 page_to_phys(frag->page) + frag->page_offset,
                                 frag->size);
      if (unlikely(status != VMK_OK)) {
         return status;
      }

      /*
       * The frags should not be coalesced with the first sg entry (flat buffer).
       * If this happens let's just drop the packet instead of leaking.
       */
      if (unlikely(vmk_PktFragsNb(skb->pkt) <= 1)) {
         VMK_ASSERT(VMK_FALSE);
         return VMK_FAILURE;
      }
   }

   /*
    * Let vmkernel know it needs to release those frags explicitly.
    */
   vmk_PktSetPageFrags(skb->pkt);

   return status;
}

/*
 *----------------------------------------------------------------------------
 *
 *  skb_append_fraglist_to_pkt --
 *
 *    Append skb frag list to the packet handle associated to it.
 *
 *  Results:
 *    VMK_OK on success; VMK_* otherwise.
 *
 *  Side effects:
 *    Drops packet on the floor if unsuccessful.
 *
 *----------------------------------------------------------------------------
 */
static inline VMK_ReturnStatus
skb_append_fraglist_to_pkt(struct sk_buff *skb)
{
   VMK_ReturnStatus status = VMK_OK;
   struct sk_buff *frag_skb = skb_shinfo(skb)->frag_list;

   while (frag_skb) {
      /*
       * LRO might have pulled the all flat buffer if header split mode
       * is activated.
       */
      if (skb_headlen(frag_skb)) {
         status = vmk_PktAppend(skb->pkt, frag_skb->pkt,
                                skb_headroom(frag_skb), skb_headlen(frag_skb));
         if (unlikely(status != VMK_OK)) {
            return status;
         }
      }

      if (skb_shinfo(frag_skb)->nr_frags) {
         int i;

         for (i = 0; i < skb_shinfo(frag_skb)->nr_frags; i++) {
            skb_frag_t *frag = &skb_shinfo(frag_skb)->frags[i];

            status = vmk_PktAppendFrag(skb->pkt,
                                       page_to_phys(frag->page) + frag->page_offset,
                                       frag->size);
            if (unlikely(status != VMK_OK)) {
               return status;
            }
         }
      }

      frag_skb = frag_skb->next;
   }

   return status;
}

/*
 *----------------------------------------------------------------------------
 *
 *  skb_gen_pkt_frags --
 *
 *    Append the skb frags and frag list to the packet handle associated to it.
 *
 *  Results:
 *    VMK_OK on success; VMK_* otherwise.
 *
 *  Side effects:
 *    Drops packet on the floor if unsuccessful.
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
skb_gen_pkt_frags(struct sk_buff *skb)
{
   VMK_ReturnStatus status;

   status = vmk_PktAdjust(skb->pkt, skb_headroom(skb), skb_headlen(skb));
   VMK_ASSERT(status == VMK_OK);

   if (skb_shinfo(skb)->nr_frags) {
      status = skb_append_frags_to_pkt(skb);
      if (unlikely(status != VMK_OK)) {
         return status;
      }
   }

   /*
    * Since we removed packet completion in vmklinux, we
    * cannot support skb chaining anymore.
    */
   if (skb_shinfo(skb)->frag_list) {
      VMK_ASSERT(VMK_FALSE);
      return VMK_NOT_SUPPORTED;
   }

   return status;
}


/*
 *----------------------------------------------------------------------------
 *
 *  do_init_skb_bits --
 *
 *    Initialize a socket buffer.
 *
 *  Results:
 *    None.
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static inline void
do_init_skb_bits(struct sk_buff *skb, kmem_cache_t *cache)
{
   skb->qid = VMKNETDDI_QUEUEOPS_INVALID_QUEUEID;
   skb->next = NULL;
   skb->prev = NULL;
   skb->head = NULL;
   skb->data = NULL;
   skb->tail = NULL;
   skb->end = NULL;
   skb->dev = NULL;
   skb->pkt = NULL;
   atomic_set(&skb->users, 1);
   skb->cache = cache;
   skb->mhead = 0;
   skb->len = 0;
   skb->data_len = 0;
   skb->ip_summed = CHECKSUM_NONE;
   skb->csum = 0;
   skb->priority = 0;
   skb->protocol = 0;
   skb->truesize = 0;
   skb->mac.raw = NULL;
   skb->nh.raw = NULL;
   skb->h.raw = NULL;
   skb->napi = NULL;
   skb->lro_ready = 0;

   /* VLAN_RX_SKB_CB shares the same space so this is sufficient */
   VLAN_TX_SKB_CB(skb)->magic = 0;
   VLAN_TX_SKB_CB(skb)->vlan_tag = 0;

   atomic_set(&(skb_shinfo(skb)->dataref), 1);
   atomic_set(&(skb_shinfo(skb)->fragsref), 1);
   skb_shinfo(skb)->nr_frags = 0;
   skb_shinfo(skb)->frag_list = NULL;
   skb_shinfo(skb)->gso_size = 0;
   skb_shinfo(skb)->gso_segs = 0;
   skb_shinfo(skb)->gso_type = 0;
   skb_shinfo(skb)->ip6_frag_id = 0;

   get_LinSkb(skb)->flags = LIN_SKB_FLAGS_FRAGSOWNER_VMKLNX;
}


/*
 *----------------------------------------------------------------------------
 *
 *  do_bind_skb_to_pkt --
 *
 *    Bind a socket buffer to a packet handle.
 *
 *  Results:
 *    None.
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static inline void
do_bind_skb_to_pkt(struct sk_buff *skb, vmk_PktHandle *pkt, unsigned int size)
{
   skb->pkt = pkt;
   skb->head = (void *) vmk_PktFrameMappedPointerGet(pkt);
   skb->end = skb->head + size;
   skb->data = skb->head;
   skb->tail = skb->head;

#ifdef VMX86_DEBUG
   VMK_ASSERT(vmk_PktFrameMappedLenGet(pkt) >= size);

   /*
    * linux guarantees physical contiguity of the pages backing
    * skb's returned by this routine, so the drivers assume that.
    * we guarantee this by backing the buffers returned from
    * vmk_PktAlloc with a large-page, low-memory heap which is
    * guaranteed to be physically contiguous, so we just double
    * check it here.
    */
   {
      vmk_Bool isFlat;

      isFlat = vmk_PktIsFlatBuffer(pkt);
      VMK_ASSERT(isFlat);
   }
#endif // VMX86_DEBUG
}

/*
 *----------------------------------------------------------------------------
 *
 *  do_alloc_skb --
 *
 *    Allocate a socket buffer.
 *
 *  Results:
 *    A pointer to the allocated socket buffer.
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static struct sk_buff *
do_alloc_skb(kmem_cache_t *cache, gfp_t flags)
{
   struct LinSkb *linSkb;

   VMK_ASSERT(cache != NULL);

   if (!cache) {
      VMKLNX_WARN("No skb cache provided.");
      return NULL;
   }

   linSkb = vmklnx_kmem_cache_alloc(cache, flags);
   if (unlikely(linSkb == NULL)) {
      return NULL;
   }

   do_init_skb_bits(&linSkb->skb, cache);
   return &linSkb->skb;
}

/*
 *----------------------------------------------------------------------------
 *
 *  vmklnx_net_alloc_skb --
 *
 *    Allocate a socket buffer for a specified size and bind it to a packet handle.
 *
 *  Results:
 *    A pointer the allocated socket buffer.
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
struct sk_buff *
vmklnx_net_alloc_skb(struct kmem_cache_s *cache, unsigned int size, struct net_device *dev, gfp_t flags)
{
   vmk_PktHandle *pkt;
   struct sk_buff *skb;

   skb = do_alloc_skb(cache, flags);

   if (unlikely(skb == NULL)) {
      goto done;
   }

   if (dev && dev->uplinkDev) {
      /*
       * Do a packet allocation aimed at the specified device.
       * The packet will be allocated in memory that will be
       * easy to DMA map to.
       */
      vmk_PktAllocForUplink(size, dev->uplinkDev, &pkt);
   } else {
      /* Do a simple packet allocation. */
      vmk_PktAlloc(size, &pkt);
   }     

   if (unlikely(pkt == NULL)) {
      do_free_skb(skb);
      skb = NULL;
      goto done;
   }

   do_bind_skb_to_pkt(skb, pkt, size);

 done:
   return skb;
}
EXPORT_SYMBOL(vmklnx_net_alloc_skb);

/*
 *-----------------------------------------------------------------------------
 *
 * vmklnx_set_skb_frags_owner_vmkernel --
 *
 *      Toggle skb frag ownership for the given skb to VMkernel.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Sets the skb frag ownership to VMkernel for the given skb.
 *
 *-----------------------------------------------------------------------------
 */

inline void
vmklnx_set_skb_frags_owner_vmkernel(struct sk_buff *skb)
{
   get_LinSkb(skb)->flags &= ~LIN_SKB_FLAGS_FRAGSOWNER_VMKLNX;
   get_LinSkb(skb)->flags |= LIN_SKB_FLAGS_FRAGSOWNER_VMKERNEL;
   return;
}
EXPORT_SYMBOL(vmklnx_set_skb_frags_owner_vmkernel);

/*
 *-----------------------------------------------------------------------------
 *
 * vmklnx_is_skb_frags_owner --
 *
 *      Indicate if the skb frags belongs to vmklinux.
 *
 *      We do not always want to call put_page() on skb frags. For
 *      instance, in the TX path the frags belong to the guest
 *      OS. However, in the RX path with packet split and others we
 *      need to call put_page() since the frags belong to vmklinux.
 *
 * Results:
 *      1 if the frags belong to vmklinux, 0 otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
vmklnx_is_skb_frags_owner(struct sk_buff *skb)
{
   VMK_ASSERT(skb_shinfo(skb)->nr_frags);

   return (get_LinSkb(skb)->flags & LIN_SKB_FLAGS_FRAGSOWNER_VMKLNX);
}
EXPORT_SYMBOL(vmklnx_is_skb_frags_owner);

/*
 *----------------------------------------------------------------------------
 *
 *  skb_release_data --
 *
 *    Release data associated to a skb.
 *
 *  Results:
 *    None.
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
void
skb_release_data(struct sk_buff *skb)
{
   VMK_ASSERT((atomic_read(&skb_shinfo(skb)->dataref) & SKB_DATAREF_MASK) == 1);

   if (atomic_dec_and_test(&(skb_shinfo(skb)->dataref))) {
      if (unlikely(skb->mhead)) {
         skb->mhead = 0;
         vmklnx_kfree(vmklnxLowHeap, skb->head);
      }

      if (likely(atomic_dec_and_test(&(skb_shinfo(skb)->fragsref)))) {
         if (skb->pkt) {
            if ((in_irq() || irqs_disabled()) && !vmklnx_is_panic()) {
               vmk_PktReleaseIRQ(skb->pkt);
            } else {
               vmk_NetPoll pollPriv;
               struct napi_struct *napi;

               /*
                * Try to queue packets in NAPI's compPktList in order to
                * release them in batch, but first thoroughly check if we
                * got called from a napi context (PR #396873).
                */
               if (vmk_NetPollGetCurrent(&pollPriv) == VMK_OK && 
                   (napi = (struct napi_struct *) vmk_NetPollGetPrivate(pollPriv)) != NULL &&
                   napi->net_poll_type == NETPOLL_DEFAULT) {
                  vmk_NetPollQueueCompPkt(pollPriv, skb->pkt);
               } else {
                  vmk_PktRelease(skb->pkt);
               }
            }
         }

         if (skb_shinfo(skb)->nr_frags && vmklnx_is_skb_frags_owner(skb)) {
            int i;

            for (i = 0; i < skb_shinfo(skb)->nr_frags; i++) {
               put_page(skb_shinfo(skb)->frags[i].page);
            }
            skb_shinfo(skb)->nr_frags = 0;
         }

         if (skb_shinfo(skb)->frag_list) {
            struct sk_buff *frag_skb = skb_shinfo(skb)->frag_list;
            struct sk_buff *next_skb;

            while (frag_skb) {
               next_skb = frag_skb->next;
               kfree_skb(frag_skb);
               frag_skb = next_skb;
            }
            skb_shinfo(skb)->frag_list = NULL;
         }
      }
   }
}

/*
 *----------------------------------------------------------------------------
 *
 *  do_free_skb --
 *
 *    Release socket buffer.
 *
 *  Results:
 *    None.
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static void
do_free_skb(struct sk_buff *skb)
{
   vmklnx_kmem_cache_free(skb->cache, get_LinSkb(skb));
}

/**
 *  __kfree_skb - private function
 *  @skb: buffer
 *
 *  Free an sk_buff. Release anything attached to the buffer.
 *  Clean the state. This is an internal helper function. Users should
 *  always call kfree_skb
 *
 * RETURN VALUE:
 * None
 */
/* _VMKLNX_CODECHECK_: __kfree_skb */
void
__kfree_skb(struct sk_buff *skb)
{
   if (unlikely(!atomic_dec_and_test(&skb->users))) {
      return;
   }

   skb_release_data(skb);
   do_free_skb(skb);
}
EXPORT_SYMBOL(__kfree_skb);

/*
 *----------------------------------------------------------------------------
 *
 *  skb_debug_info --
 *      Debug function to print contents of a socket buffer.
 *
 *  Results:
 *    None.
 *
 *  Side effects:
 *    None.
 *----------------------------------------------------------------------------
 */
void
skb_debug_info(struct sk_buff *skb)
{
   int f;
   skb_frag_t *frag;

   printk(KERN_ERR "skb\n"
          "   head     <%p>\n"
          "   mhead    <%u>\n"
          "   data     <%p>\n"
          "   tail     <%p>\n"
          "   end      <%p>\n"
          "   data_len <%u>\n"
          "   nr_frags <%u>\n"
          "   dataref  <%u>\n"
          "   gso_size <%u>\n",
          skb->head, skb->mhead,
          skb->data, skb->tail, skb->end,
          skb->data_len,
          skb_shinfo(skb)->nr_frags,
          atomic_read(&(skb_shinfo(skb)->dataref)),
          skb_shinfo(skb)->gso_size);

   for (f = 0; f < skb_shinfo(skb)->nr_frags; f++) {
      frag = &skb_shinfo(skb)->frags[f];
      printk(KERN_ERR "skb frag %d\n"
             "   page         <0x%llx>\n"
             "   page_offset  <%u>\n"
             "   size         <%u>\n",
             f, page_to_phys(frag->page),
             frag->page_offset, frag->size);
   }
}


/*
 * Section: Transmit path
 */

/*
 *----------------------------------------------------------------------------
 *
 *  ipv6_set_hraw --
 *
 *    Parse an IPv6 skb to find the appropriate value for initializing
 *    skb->h.raw.  If skb->h.raw is initialized, also sets *protocol to
 *    the last nexthdr found.
 *
 *   Results:
 *    None
 *
 *   Side effects:
 *    None
 *
 *----------------------------------------------------------------------------
 */
static void
ipv6_set_hraw(struct sk_buff *skb, vmk_uint8 *protocol)
{
   vmk_uint8 nextHdr = skb->nh.ipv6h->nexthdr;
   vmk_uint8 *nextHdrPtr = (vmk_uint8 *) (skb->nh.ipv6h + 1);

   if (nextHdrPtr > skb->end) {
      // this happens if the source doesn't take care to map entire header
      return;

   }
   // take care of most common situation:
   if ((nextHdr == IPPROTO_TCP)
       || (nextHdr == IPPROTO_UDP)
       || (nextHdr == IPPROTO_ICMPV6)) {
      skb->h.raw = nextHdrPtr;
      (*protocol) = nextHdr;
      return;
   }

   /*
    * This will be the value if "end" not found within
    *   linear region.
    */
   VMK_ASSERT(skb->h.raw == NULL);
   do {
      switch (nextHdr) {
      case IPPROTO_ROUTING:
      case IPPROTO_HOPOPTS:
      case IPPROTO_DSTOPTS:
         // continue searching
         nextHdr = *nextHdrPtr;
         nextHdrPtr += nextHdrPtr[1] * 8 + 8;
         break;

      case IPPROTO_AH:
         // continue searching
         nextHdr = *nextHdrPtr;
         nextHdrPtr += nextHdrPtr[1] * 4 + 8;
         break;

      /*
       * We do NOT handle the IPPROTO_FRAGMENT case here. Thus,
       * if any packet has a IPv6 fragment header, this function
       * will return protocol == IPPROTO_FRAGMENT and *not*
       * find the L4 protocol. As the returned protocol is only
       * used for TSO and CSUM cases, and a fragment header is
       * not allowed in either case, this behavior is desirable,
       * as it allows handling this case in the caller.
       */

      default:
         // not recursing
         skb->h.raw = nextHdrPtr;
         (*protocol) = nextHdr;
         return;
         break;
      }
   } while (nextHdrPtr < skb->end);
}

/*
 *----------------------------------------------------------------------------
 *
 *  map_pkt_to_skb --
 *
 *    Converts PktHandle to sk_buff before handing packet to linux driver.
 *
 *   Results:
 *    Returns VMK_ReturnStatus
 *
 *   Side effects:
 *    This is ugly. Too much memory writes / packet. We should look at
 *    optimizing this. Maybe an skb cache or something, instead of
 *    having to touch 20+ variables for each packet.
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
map_pkt_to_skb(struct net_device *dev,
               struct netdev_queue *queue,
               vmk_PktHandle *pkt,
               struct sk_buff **pskb)
{
   VMK_ReturnStatus status;
   struct sk_buff *skb;
   int i;
   vmk_uint16 sgInspected;
   unsigned int headLen, bytesLeft;
   vmk_uint32 frameLen;
   VMK_ReturnStatus ret = VMK_OK;
   vmk_PktFrag frag;
   vmk_Bool must_vlantag, must_tso, must_csum, pkt_ipv4;
   vmk_uint8 protocol, ipVersion;
   vmk_uint32 ehLen;
   vmk_uint32 ipHdrLength;

   skb = do_alloc_skb(dev->skb_pool, GFP_ATOMIC);

   if (unlikely(skb == NULL)) {
      ret = VMK_NO_MEMORY;
      goto done;
   }

   skb->pkt = pkt;
   skb->queue_mapping = queue - dev->_tx;

   VMK_ASSERT(dev);
   VMK_ASSERT(pkt);
#ifdef VMX86_DEBUG
 {
    vmk_Bool consistent;

    consistent = vmk_PktCheckInternalConsistency(pkt);
    VMK_ASSERT(consistent);
 }
#endif

   VMK_ASSERT(vmk_PktFrameMappedLenGet(pkt) > 0);

   skb->head = (void *) vmk_PktFrameMappedPointerGet(pkt);

   frameLen = vmk_PktFrameLenGet(pkt);

   skb->len = frameLen;
   skb->dev = dev;
   skb->data = skb->head;

   headLen = min(vmk_PktFrameMappedLenGet(pkt), frameLen);
   skb->end = skb->tail = skb->head + headLen;
   skb->mac.raw = skb->data;

   must_csum = vmk_PktIsMustCsum(pkt);


   status = vmk_PktInetFrameLayoutGetComponents(pkt,
                                                &ehLen,
                                                &ipHdrLength,
                                                &ipVersion,
                                                &protocol);
   if (status == VMK_OK) {
      /*
       * Pkt has inet layout attributes associated, populate
       * the skb details from the layout components.
       */
      skb->nh.raw = skb->mac.raw + ehLen;
      skb->h.raw = skb->nh.raw + ipHdrLength;

      if (ipVersion == 4) {
         const int eth_p_ip_nbo = htons(ETH_P_IP);
         pkt_ipv4 = VMK_TRUE;
         skb->protocol = eth_p_ip_nbo;
      } else {
         pkt_ipv4 = VMK_FALSE;
         if (ipVersion == 6) {
            skb->protocol = ETH_P_IPV6_NBO;
         }
      }
      VMKLNX_DEBUG(3, "inet layout %d %d %d protocol %x ipVers %d proto %x "
                   "ipv4 %d csum %d tso %d",
                   ehLen,
                   ipHdrLength,
                   vmk_PktInetFrameLayoutGetL4HdrLength(pkt, VMK_FALSE),
                   skb->protocol,
                   protocol,
                   pkt_ipv4,
                   ipVersion,
                   must_csum,
                   vmk_PktIsLargeTcpPacket(pkt));
   } else {
      struct ethhdr *eh;

      eh = (struct ethhdr *) skb->head;
      ehLen = eth_header_len(eh);
      skb->nh.raw = skb->mac.raw + ehLen;
      skb->protocol = eth_header_frame_type(eh);
      sgInspected = 1;

      if (eth_header_is_ipv4(eh)) {
         if (headLen < ehLen + sizeof(*skb->nh.iph)) {
            ret = VMK_FAILURE;
            goto done;
         }
         skb->h.raw = skb->nh.raw + skb->nh.iph->ihl*4;
         pkt_ipv4 = VMK_TRUE;
         protocol = skb->nh.iph->protocol;
      } else {
         pkt_ipv4 = VMK_FALSE;
         protocol = 0xff;  // unused value.
         if (skb->protocol == ETH_P_IPV6_NBO) {
            ipv6_set_hraw(skb, &protocol);
            VMKLNX_DEBUG(3, "ipv6 %ld offset %ld %d",
                        skb->h.raw - (vmk_uint8 *)(skb->data),
                        skb->h.raw - (vmk_uint8 *)(skb->nh.ipv6h), protocol);
         }
      }
   }

   VMKLNX_DEBUG(10, "head: %u bytes at VA 0x%p", headLen, skb->head);

   /*
    * See if the packet requires VLAN tagging
    */
   must_vlantag = vmk_PktMustVlanTag(pkt);

   if (must_vlantag) {
      vmk_VlanID vlanID;
      vmk_VlanPriority priority;

      VMKLNX_DEBUG(2, "%s: tx vlan tag %u present with priority %u",
                   dev->name, vmk_PktVlanIDGet(pkt), vmk_PktPriorityGet(pkt));

      vlanID = vmk_PktVlanIDGet(pkt);
      priority = vmk_PktPriorityGet(pkt);

      vlan_put_tag(skb, vlanID | (priority << VLAN_1PTAG_SHIFT));
   }

   /*
    * See if the packet requires checksum offloading or TSO
    */

   must_tso = vmk_PktIsLargeTcpPacket(pkt);

   if (must_tso) {
      vmk_uint32 tsoMss = vmk_PktGetLargeTcpPacketMss(pkt);
      unsigned short inetHdrLen;

      /*
       * backends should check the tsoMss before setting MUST_TSO flag
       */
      VMK_ASSERT(tsoMss);

      if (!pkt_ipv4 &&
          (skb->protocol != ntohs(ETH_P_IPV6))) {
         static uint32_t throttle = 0;
         VMKLNX_THROTTLED_WARN(throttle,
                               "%s: non-ip packet with TSO (proto=0x%x)",
                               dev->name,
                               skb->protocol);
         ret = VMK_FAILURE;
         goto done;
      }

      if (!skb->h.raw || (protocol != IPPROTO_TCP)) {
         /*
          * This check will also trigger for IPv6 packets that
          * have a fragment header, as ipv6_set_hraw() sets protocol
          * to IPPROTO_FRAGMENT.
          */
         static uint32_t throttle = 0;
         VMKLNX_THROTTLED_WARN(throttle,
                               "%s: non-tcp packet with TSO (ip%s, proto=0x%x, hraw=%p)",
                               dev->name,
                               pkt_ipv4 ? "v4" : "v6",
                               protocol, skb->h.raw);
         ret = VMK_FAILURE;
         goto done;
      }

      /*
       * Perform some sanity checks on TSO frames, because buggy and/or
       * malicious guests might generate invalid packets which may wedge
       * the physical hardware if we let them through.
       */
      inetHdrLen = (skb->h.raw + tcp_hdrlen(skb)) - skb->nh.raw;

      // Reject if the frame doesn't require TSO in the first place
      if (unlikely(frameLen - ehLen - inetHdrLen <= tsoMss)) {
         static uint32_t throttle = 0;
         VMKLNX_THROTTLED_WARN(throttle,
                               "%s: runt TSO packet (tsoMss=%d, frameLen=%d)",
                               dev->name, tsoMss, frameLen);
         ret = VMK_FAILURE;
         goto done;
      }

      // Reject if segmented frame will exceed MTU
      if (unlikely(tsoMss + inetHdrLen > dev->mtu)) {
         static uint32_t logThrottleCounter = 0;
         VMKLNX_THROTTLED_WARN(logThrottleCounter,
                               "%s: oversized tsoMss: %d, mtu=%d",
                               dev->name, tsoMss, dev->mtu);
         ret = VMK_FAILURE;
         goto done;
      }

      skb_shinfo(skb)->gso_size = tsoMss;
      skb_shinfo(skb)->gso_segs = (skb->len + tsoMss - 1) / tsoMss;
      skb_shinfo(skb)->gso_type = pkt_ipv4 ? SKB_GSO_TCPV4 : SKB_GSO_TCPV6;

      /*
       * If congestion window has been reduced due to the
       *  previous TCP segment
       */
      if (unlikely(skb->h.th->cwr == 1)) {
         skb_shinfo(skb)->gso_type |= SKB_GSO_TCP_ECN;
      }
   } else {
      /*
       * We are dropping packets that are larger than the MTU of the NIC
       * since they could potentially wedge the NIC or PSOD in the driver.
       */
      if (unlikely(frameLen - ehLen > dev->mtu)) {
         static uint32_t linuxTxWarnCounter;
         VMKLNX_THROTTLED_WARN(linuxTxWarnCounter,
                               "%s: %d bytes packet couldn't be sent (mtu=%d)",
                               dev->name, frameLen, dev->mtu);
         ret = VMK_FAILURE;
         goto done;
      }
   }

   if (must_csum || must_tso) {

      switch (protocol) {

      case IPPROTO_TCP:
         skb->csum = 16;
         skb->ip_summed = CHECKSUM_HW;
         break;

      case IPPROTO_UDP:
         skb->csum = 6;
         skb->ip_summed = CHECKSUM_HW;
         break;

      /*
       * XXX add cases for other protos once we use NETIF_F_HW_CSUM
       * in some device.  I think the e1000 can do it, but the Intel
       * driver doesn't advertise so.
       */

      default:
         VMKLNX_DEBUG(0, "%s: guest driver requested xsum offload on "
                      "unsupported type %d", dev->name, protocol);
         ret = VMK_FAILURE;
         goto done;
      }

      VMK_ASSERT(skb->h.raw);
   } else {
      skb->ip_summed = CHECKSUM_NONE; // XXX: for now
   }

   bytesLeft = frameLen - headLen;
   for (i = sgInspected; bytesLeft > 0; i++) {
      skb_frag_t *skb_frag;

      if (unlikely(i - sgInspected >= MAX_SKB_FRAGS)) {
         static uint32_t fragsThrottleCounter = 0;
         VMKLNX_THROTTLED_INFO(fragsThrottleCounter,
		               "too many frags (> %u) bytesLeft %d",
                               MAX_SKB_FRAGS, bytesLeft);
#ifdef VMX86_DEBUG
	 VMK_ASSERT(VMK_FALSE);
#endif
    	 ret = VMK_FAILURE;
         goto done;
      }

      if (vmk_PktFragGet(pkt, &frag, i) != VMK_OK) {
    	 ret = VMK_FAILURE;
         goto done;
      }
      skb_frag = &skb_shinfo(skb)->frags[i - sgInspected];
      /* Going to use the frag->page to store page number and
         frag->page_offset for offset within that page */
      skb_frag->page = phys_to_page(frag.addr);
      skb_frag->page_offset = offset_in_page(frag.addr);
      skb_frag->size = min(frag.length, bytesLeft);
      VMKLNX_DEBUG(10, "frag: %u bytes at MA 0x%llx",
                   skb_frag->size, page_to_phys(skb_frag->page) + skb_frag->page_offset);
      skb->data_len += skb_frag->size;
      bytesLeft -= skb_frag->size;
      skb_shinfo(skb)->nr_frags++;
		
      vmk_MAAssertIOAbility(frag.addr, frag.length);
   }

   /*
    * Those frags are VMkernel's buffers.  Nothing special to do in the
    * Vmklinux layer for completion.
    */
   vmklnx_set_skb_frags_owner_vmkernel(skb);

   if (VMKLNX_STRESS_DEBUG_COUNTER(stressNetIfCorruptTx)) {
      LinStress_CorruptSkbData(skb, 60, 0);
   }

 done:

   if ((ret != VMK_OK) && (skb != NULL)) {
      do_free_skb(skb);
      skb = NULL;
   }

   *pskb = skb;

   return ret;
}

/*
 *----------------------------------------------------------------------------
 *
 * netdev_pick_tx_queue --
 *
 *    Pick device tx subqueue for transmission. The upper layers must ensure
 *    that all packets in pktList are destined for the same queue.
 *
 * Results:
 *    pointer to netdev_queue
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static inline struct netdev_queue *
netdev_pick_tx_queue(struct net_device *dev, vmk_NetqueueQueueID vmkqid)
{
   int queue_idx = 0;
   vmknetddi_queueops_queueid_t qid = VMKNETDDI_QUEUEOPS_INVALID_QUEUEID;
   VMK_ReturnStatus status;

   if (!vmkqid) {
      goto out;
   }

   status = marshall_from_vmknetq_id(vmkqid, &qid);
   VMK_ASSERT(status == VMK_OK);
   if (status == VMK_OK) {
      queue_idx = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(qid);
      if (unlikely(queue_idx >= dev->real_num_tx_queues ||
                   queue_idx >= dev->num_tx_queues)) {
         queue_idx = 0;
      }
   }

 out:
   VMK_ASSERT(queue_idx < dev->num_tx_queues);
   VMK_ASSERT(queue_idx >= 0);
   return &dev->_tx[queue_idx];
}

/*
 *----------------------------------------------------------------------------
 *
 * netdev_tx --
 *
 *    Transmit packets
 *
 * Results:
 *    VMK_ReturnStatus indicating the outcome.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
netdev_tx(struct net_device *dev,
          vmk_PktList pktList,
          vmk_NetqueueQueueID vmkqid)
{
   VMK_ReturnStatus ret = VMK_OK;
   VMK_PKTLIST_STACK_DEF_INIT(freeList);
   vmk_uint32 pktsCount;
   vmk_PktHandle *pkt;
   struct sk_buff *skb;
   struct netdev_queue *queue;

   queue = netdev_pick_tx_queue(dev, vmkqid);
   VMK_ASSERT(queue);

   if (VMKLNX_STRESS_DEBUG_COUNTER(stressNetIfFailTxAndStopQueue)) {
      netif_tx_stop_queue(queue);
   }

   if (unlikely(test_bit(__LINK_STATE_BLOCKED, &dev->state)) ||
       VMKLNX_STRESS_DEBUG_COUNTER(stressNetIfFailHardTx)) {
      vmk_PktListAppend(freeList, pktList);   
      goto out;
   }

   spin_lock(&queue->_xmit_lock);
   while (!vmk_PktListIsEmpty(pktList)) {
      int xmit_status = -1;
      VMK_ReturnStatus mapRet = VMK_OK;

      VMK_ASSERT(dev->flags & IFF_UP);

      /*
       * Queue state can change even before the device is opened!
       * Upper layers have no way of knowing about it until after
       * the device is opened. All we can do is check for a stopped
       * queue here and return the appropriate error.
       */
      if (unlikely(netif_tx_queue_stopped(queue))) {
         spin_unlock(&queue->_xmit_lock);
         ret = VMK_BUSY;
         goto out;
      }
      
      if (VMKLNX_STRESS_DEBUG_COUNTER(stressNetIfFailHardTx)) {
         pkt = vmk_PktListPopFirstPkt(pktList);
         VMK_ASSERT(pkt);
         VMKLNX_DEBUG(1, "Failing Hard Transmit. pkt = %p, device = %s\n", 
                      pkt, dev->name);
         vmk_PktListAppendPkt(freeList, pkt);
         continue;
      }

      pkt = vmk_PktListPopFirstPkt(pktList);
      VMK_ASSERT(pkt);

      mapRet = map_pkt_to_skb(dev, queue, pkt, &skb);
      if (unlikely(mapRet != VMK_OK)) {
#if defined(VMX86_LOG)           
         static uint32_t logThrottleCounter = 0;
#endif         
         VMKLNX_THROTTLED_DEBUG(logThrottleCounter, 0,
                                "%s: Unable to map packet to skb (%s). Dropping", 
                                dev->name, vmk_StatusToString(mapRet));
         vmk_PktListAppendPkt(freeList, pkt);
         continue;
      }

      VMKAPI_MODULE_CALL(dev->module_id, xmit_status, 
                         *dev->hard_start_xmit, skb, dev);

      if (unlikely(xmit_status != NETDEV_TX_OK)) {
         spin_unlock(&queue->_xmit_lock);
         VMKLNX_DEBUG(1, "hard_start_xmit failed (status %d; Q stopped %d. "
                      "Queuing packet. pkt=%p dev=%s\n", 
                      xmit_status, netif_tx_queue_stopped(queue),
                      skb->pkt, dev->name);

         /* destroy skb and its resources besides the packet handle itself. */
         atomic_inc(&(skb_shinfo(skb)->fragsref));
         dev_kfree_skb_any(skb);
         
         /*
          * sticking pkt back this way may cause tx re-ordering, 
          * but this should be very rare.
          */
         vmk_PktListAppendPkt(pktList, pkt);
         if (xmit_status == NETDEV_TX_BUSY) {
            ret = VMK_BUSY;
         } else {
            ret = VMK_FAILURE;
         }
         goto out;
      }

      dev->linnet_tx_packets++;
   }

   spin_unlock(&queue->_xmit_lock);

 out:
   /*
    * Free whatever could not be txed
    */
   pktsCount = vmk_PktListGetCount(freeList);
   if (unlikely(pktsCount)) {
      dev->linnet_tx_dropped += pktsCount;
      vmk_PktListReleaseAllPkts(freeList);
   }

   return ret;
}

/*
 * Section: Control operations and queue management
 */

void __netif_schedule(struct netdev_queue *queue)
{
   //XXX: does nothing. scheduling is done by the vmkernel now.
}
EXPORT_SYMBOL(__netif_schedule);

/*
 *----------------------------------------------------------------------------
 *
 * vmklnx_netif_start_tx_queue --
 *
 *    queue started
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
void
vmklnx_netif_start_tx_queue(struct netdev_queue *queue)
{
   struct net_device *dev = queue->dev;
   u16 qidx = queue - dev->_tx;
   VMK_ASSERT(qidx < dev->num_tx_queues);

   if (dev->uplinkDev) {
      struct tx_netqueue_info *txinfo = dev->tx_netqueue_info;
      VMK_ASSERT(txinfo);

      if (txinfo[qidx].valid) {
         VMK_ASSERT(txinfo[qidx].vmkqid != VMK_NETQUEUE_INVALID_QUEUEID);
         vmk_UplinkQueueStart(dev->uplinkDev, txinfo[qidx].vmkqid);
      }
   }
}
EXPORT_SYMBOL(vmklnx_netif_start_tx_queue);


/*
 *----------------------------------------------------------------------------
 *
 * vmklnx_netif_stop_tx_queue --
 *
 *    queue stopped
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
void
vmklnx_netif_stop_tx_queue(struct netdev_queue *queue)
{
   struct net_device *dev = queue->dev;
   u16 qidx = queue - dev->_tx;
   VMK_ASSERT(qidx < dev->num_tx_queues);

   if (dev->uplinkDev) {
      struct tx_netqueue_info *txinfo = dev->tx_netqueue_info;
      VMK_ASSERT(txinfo);

      if (txinfo[qidx].valid) {
         VMK_ASSERT(txinfo[qidx].vmkqid != VMK_NETQUEUE_INVALID_QUEUEID);
         vmk_UplinkQueueStop(dev->uplinkDev, txinfo[qidx].vmkqid);
      }
   }
}
EXPORT_SYMBOL(vmklnx_netif_stop_tx_queue);

/*
 *----------------------------------------------------------------------------
 *
 * vmklnx_netif_set_poll_cna --
 *
 *    Change net poll routine to do CNA processing.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
void
vmklnx_netif_set_poll_cna(struct napi_struct *napi)
{
   if (napi->net_poll) {
      vmk_NetPollProperties pollInit;
      if (napi->net_poll_type == NETPOLL_BACKUP) { 
         pollInit.poll = netdev_poll;
         pollInit.priv = napi->dev;
         pollInit.deliveryCallback = LinuxCNADev_Poll;
      } else {
         pollInit.poll = napi_poll;
         pollInit.priv = napi;
         pollInit.deliveryCallback = LinuxCNA_Poll;
      }
      pollInit.features = VMK_NETPOLL_CUSTOM_DELIVERY_CALLBACK;
      vmk_NetPollChangeCallback(napi->net_poll, &pollInit);
   }
}
EXPORT_SYMBOL(vmklnx_netif_set_poll_cna);

/**
 * dev_close - shutdown an interface.
 * @dev: device to shutdown
 *
 * This function moves an active device into down state. The device's
 * private close function is invoked.
 *
 * ESX Deviation Notes:
 *  netdev notifier chain is not called.
 *
 * RETURN VALUE:
 *  0
 */
/* _VMKLNX_CODECHECK_: dev_close */
int
dev_close(struct net_device *dev)
{
   unsigned int i;

   ASSERT_RTNL();

#ifdef VMX86_DEBUG
   {
      VMK_ASSERT(test_bit(__LINK_STATE_START, &dev->state));
      VMK_ASSERT(dev->flags & IFF_UP);
   }
#endif

   for (i = 0; i < dev->num_tx_queues; i++) {
      struct netdev_queue *queue = &dev->_tx[i];
      spin_unlock_wait(&queue->_xmit_lock);
   }

   clear_bit(__LINK_STATE_START, &dev->state);
   smp_mb__after_clear_bit(); /* Commit netif_running(). */

   if (dev->stop) {
      VMKLNX_DEBUG(0, "Calling device stop %p", dev->stop);
      VMKAPI_MODULE_CALL_VOID(dev->module_id, dev->stop, dev);
      VMKLNX_DEBUG(0, "Device stopped");
   }

   dev->flags &= ~IFF_UP;

   return 0;
}
EXPORT_SYMBOL(dev_close);

/*
 *----------------------------------------------------------------------------
 *
 * init_watchdog_timeo --
 *
 *    Init watchdog timeout
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static void
init_watchdog_timeo(struct net_device *dev)
{
   if (dev->tx_timeout) {
      if (dev->watchdog_timeo <= 0) {
         dev->watchdog_timeo = WATCHDOG_DEF_TIMEO;
      }
      dev->watchdog_timeohit_period_start = jiffies;
      dev->watchdog_timeohit_cnt = 0;
   }
}

/**
 *  dev_open	- prepare an interface for use.
 *  @dev:	device to open
 *
 *  Takes a device from down to up state. The device's private open
 *  function is invoked.
 *
 * ESX Deviation Notes:
 *  Device's notifier chain is not called.
 *  Device is put in promiscuous mode after it is opened unless it is
 *  a passthru device, in which case RX filters are pushed through the
 *  passthru APIs.
 *
 *  Calling this function on an active interface is a nop. On a failure
 *  a negative errno code is returned.
 *
 * RETURN VALUE:
 *  0 on success
 *  negative error code returned by the device on error
 *
 */
/* _VMKLNX_CODECHECK_: dev_open */
int
dev_open(struct net_device *dev)
{
   int ret = 0;

   ASSERT_RTNL();

   if (dev->flags & IFF_UP) {
      return 0;
   }

   set_bit(__LINK_STATE_START, &dev->state);
   if (dev->open) {
      VMKAPI_MODULE_CALL(dev->module_id, ret, dev->open, dev);
      if (ret == 0) {
         VMKLNX_DEBUG(0, "%s opened successfully\n", dev->name);

         dev->flags |= IFF_UP;
	 if (!(dev->features & NETIF_F_CNA)) {
            init_watchdog_timeo(dev);
            
            if (!dev->pt_ops) {
               /*
                * Regular uplinks are put in promiscuous mode.
                */
               dev->flags |= IFF_PROMISC;
            } else {
               /*
                * Passthru devices should not be in promiscuous mode:
                *
                *  UPT: device is used only for one vNIC, vf_set_mc,
                *  vf_set_rx_mode and vf_set_multicast are used to
                *  program filtering.
                *
                *  NPA: device has embedded l2 switching and adds filter
                *  for every unicast MAC addresses on the vSwitch.
                *  pf_add_mac_filter/pf_del_mac_filter and pf_mirror_all
                *  are used to program filtering.
                *
                * However, for NPA, device must be in all-multi mode.
                */
               if (!(dev->features & NETIF_F_UPT)) {
                  dev->flags |= IFF_ALLMULTI;
               }
            }

            VMKLNX_DEBUG(0, "%s set_multi %x %lx %p\n", dev->name, dev->flags, dev->features, dev->pt_ops);
            if (dev->set_multicast_list) {
               VMKAPI_MODULE_CALL_VOID(dev->module_id, 
                                       dev->set_multicast_list, 
                                       dev);
            }
	 } else {
            /* unblock the device */
            clear_bit(__LINK_STATE_BLOCKED, &dev->state);
         }
      } else {
         clear_bit(__LINK_STATE_START, &dev->state);
      }
   }

   return ret;
}
EXPORT_SYMBOL(dev_open);

/*
 *----------------------------------------------------------------------------
 *
 * vmklnx_free_netdev
 *
 *    Internal implementation of free_netdev, frees net_device and associated
 *    structures. Exposed verion of free_netdev is an inline because it
 *    touches driver private data structs.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
void
vmklnx_free_netdev(struct kmem_cache_s *pmCache, struct net_device *dev)
{
   LinNetDev *linDev = get_LinNetDev(dev);

   if (dev->skb_pool) {
      dev->skb_pool = NULL;
   }

   kfree(dev->tx_netqueue_info);
   kfree(dev->_tx);
   kfree((char *)linDev - linDev->padded);
}
EXPORT_SYMBOL(vmklnx_free_netdev);

static void
netdev_init_one_queue(struct net_device *dev,
                      struct netdev_queue *queue,
                      void *_unused)
{
   queue->dev = dev;
}

static void
netdev_init_queues(struct net_device *dev)
{
   netdev_for_each_tx_queue(dev, netdev_init_one_queue, NULL);
}

struct net_device *
vmklnx_alloc_netdev_mq(struct module *this_module,
                       int sizeof_priv,
                       const char *name,
                       void (*setup)(struct net_device *),
                       unsigned int queue_count)
{
   int i;
   LinNetDev *linDev;
   struct netdev_queue *tx;
   struct net_device *dev;
   int alloc_size;
   void *p;
   struct tx_netqueue_info *tx_netqueue_info;

   VMK_ASSERT(this_module->skb_cache);
   VMK_ASSERT(this_module->moduleID != 0 && this_module->moduleID != VMK_INVALID_MODULE_ID);

   BUG_ON(strlen(name) >= sizeof(dev->name));

   alloc_size = sizeof(struct LinNetDev);

   if (sizeof_priv) {
      /* ensure 32-byte alignment of private area */
      alloc_size = (alloc_size + NETDEV_ALIGN_CONST) & ~NETDEV_ALIGN_CONST;
      alloc_size += sizeof_priv;
   }

   /* ensure 32-byte alignment of whole construct */
   alloc_size += NETDEV_ALIGN_CONST;

   p = kzalloc(alloc_size, GFP_KERNEL);
   if (!p) {
      printk(KERN_ERR "alloc_netdev: Unable to allocate device.\n");
      return NULL;
   }

   linDev = (LinNetDev *)
      (((long)p + NETDEV_ALIGN_CONST) & ~NETDEV_ALIGN_CONST);
   linDev->padded = (char *)linDev - (char *)p;

   tx = kzalloc(sizeof(struct netdev_queue) * queue_count, GFP_KERNEL);
   if (!tx) {
      printk(KERN_ERR "alloc_netdev: Unable to allocate "
             "tx qdiscs.\n");
      kfree(p);
      return NULL;
   }

   alloc_size = sizeof (struct tx_netqueue_info) * queue_count;
   tx_netqueue_info = kzalloc(alloc_size, GFP_KERNEL);
   if (!tx_netqueue_info) {
      printk(KERN_ERR "alloc_netdev: Unable to allocate tx_netqueue_info.\n");
      kfree(tx);
      kfree(p);
      return NULL;
   }

   /* make default queue valid */
   tx_netqueue_info[0].valid = VMK_TRUE;
   tx_netqueue_info[0].vmkqid = VMK_NETQUEUE_DEFAULT_QUEUEID;

   for (i = 1; i < queue_count; i++) {
      tx_netqueue_info[i].valid = VMK_FALSE;
      tx_netqueue_info[i].vmkqid = VMK_NETQUEUE_INVALID_QUEUEID;
   }

   dev = &linDev->linNetDev;
   dev->skb_pool = this_module->skb_cache;
   dev->_tx = tx;
   dev->num_tx_queues = queue_count;
   dev->real_num_tx_queues = queue_count;
   dev->tx_netqueue_info = tx_netqueue_info;

   if (sizeof_priv) {
      dev->priv = ((char *)dev +
                   ((sizeof(struct net_device) + NETDEV_ALIGN_CONST)
                    & ~NETDEV_ALIGN_CONST));
   }

   netdev_init_queues(dev);

   dev->module_id = this_module->moduleID;
   INIT_LIST_HEAD(&dev->napi_list);
   spin_lock_init(&dev->napi_lock);
   set_bit(__NETQUEUE_STATE, (void*)&dev->netq_state);

   VMKAPI_MODULE_CALL_VOID(dev->module_id, setup, dev);
   strcpy(dev->name, name);

   return dev;
}
EXPORT_SYMBOL(vmklnx_alloc_netdev_mq);

#ifndef ARPHRD_ETHER
#define ARPHRD_ETHER  1       /* Ethernet 10Mbps.  */
#endif

/**
 *  ether_setup - setup the given Ethernet network device
 *  @dev: network device
 *
 *  Initializes fields of the given network device with Ethernet-generic
 *  values
 *
 *  ESX Deviation Notes:
 *  This function does not initialize any function pointers in the
 *  given net_device
 *
 *  RETURN VALUE:
 *  This function does not return a value
 */
/* _VMKLNX_CODECHECK_: ether_setup */
void
ether_setup(struct net_device *dev)
{
   dev->type		= ARPHRD_ETHER;
   dev->hard_header_len = ETH_HLEN; /* XXX should this include 802.1pq? */
   dev->mtu		= ETH_DATA_LEN; /* eth_mtu */
   dev->addr_len	= ETH_ALEN;
   /* XXX */
   dev->tx_queue_len	= 100;	/* Ethernet wants good queues */

   memset(dev->broadcast, 0xFF, ETH_ALEN);

   dev->flags		= IFF_BROADCAST|IFF_MULTICAST;
}
EXPORT_SYMBOL(ether_setup);


/**
 * netif_device_attach - mark device as attached
 * @dev: network device
 *
 * Mark device as attached from system and restart if needed.
 *
 * RETURN VALUE:
 *  None
 */
/* _VMKLNX_CODECHECK_: netif_device_attach */
void
netif_device_attach(struct net_device *dev)
{
	if (!test_and_set_bit(__LINK_STATE_PRESENT, &dev->state) &&
	    netif_running(dev)) {
                netif_tx_wake_all_queues(dev);
 		__netdev_watchdog_up(dev);
	}
}
EXPORT_SYMBOL(netif_device_attach);

/**
 * netif_device_detach - mark device as removed
 * @dev: network device
 *
 * Mark device as removed from system and therefore no longer available.
 *
 * RETURN VALUE:
 *  None
 */
/* _VMKLNX_CODECHECK_: netif_device_detach */
void
netif_device_detach(struct net_device *dev)
{
	if (test_and_clear_bit(__LINK_STATE_PRESENT, &dev->state) &&
	    netif_running(dev)) {
                netif_tx_stop_all_queues(dev);
	}
}
EXPORT_SYMBOL(netif_device_detach);

static void
__netdev_init_queue_locks_one(struct net_device *dev,
                              struct netdev_queue *queue,
                              void *_unused)
{
   VMK_ReturnStatus status;
   struct netdev_soft_queue *softq = &queue->softq;

   spin_lock_init(&queue->_xmit_lock);
   queue->xmit_lock_owner = -1;
   queue->processing_tx = 0;

   spin_lock_init(&softq->queue_lock);
   softq->state = 0;
   softq->outputList = (vmk_PktList) vmk_HeapAlloc(vmklnxLowHeap, 
                                                   vmk_PktListSizeInBytes);
   if (softq->outputList == NULL) {
      VMK_ASSERT(VMK_FALSE);
      return;
   }
   vmk_PktListInit(softq->outputList);
   status = vmk_ConfigParamGetUint(maxNetifTxQueueLenConfigHandle,
                                   &softq->outputListMaxSize);
   VMK_ASSERT(status == VMK_OK);
}

/*
 *----------------------------------------------------------------------------
 *
 * netdev_init_queue_locks --
 *
 *    Init device queues locks.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static void
netdev_init_queue_locks(struct net_device *dev)
{
   netdev_for_each_tx_queue(dev, __netdev_init_queue_locks_one, NULL);
}

static void
__netdev_destroy_queue_locks_one(struct net_device *dev,
                                 struct netdev_queue *queue,
                                 void *_unused)
{
   struct netdev_soft_queue *softq = &queue->softq;

   vmk_HeapFree(vmklnxLowHeap, softq->outputList);
}

/*
 *----------------------------------------------------------------------------
 *
 * netdev_destroy_queue_locks --
 *
 *    Destroy device queues locks.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static void
netdev_destroy_queue_locks(struct net_device *dev)
{
   netdev_for_each_tx_queue(dev, __netdev_destroy_queue_locks_one, NULL);
}

/*
 *----------------------------------------------------------------------------
 *
 *  netdev_ioctl --
 *    Process an ioctl request for a given device.
 *
 *  Results:
 *    VMK_ReturnStatus indicating the outcome.
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
netdev_ioctl(struct net_device *dev, uint32_t cmd, void *args, uint32_t *result,
             vmk_IoctlCallerSize callerSize, vmk_Bool callerHasRtnlLock)
{
   VMK_ReturnStatus ret = VMK_OK;

   VMK_ASSERT(dev);

   if (args && result) {
      if (cmd == SIOCGIFHWADDR) {
         struct ifreq *ifr = args;
         memcpy(ifr->ifr_hwaddr.sa_data, dev->dev_addr, 6);
         ifr->ifr_hwaddr.sa_family = dev->type;
         *result = 0;
         return VMK_OK;
      }

      if (cmd == SIOCETHTOOL) {
         struct ifreq *ifr = args;

         if (callerHasRtnlLock == VMK_FALSE) {
            rtnl_lock();
         }

         ret = vmklnx_ethtool_ioctl(dev, ifr, result, callerSize);

         /* Some drivers call dev_close() when ethtool ops like .set_ringparam failed.
          * The following check will update dev->gflags accordingly to avoid a second
          * dev_close() when CloseNetDev() is called.
          */
         if (ret && !(dev->flags & IFF_UP))
            dev->gflags &= ~IFF_DEV_IS_OPEN;

         if (callerHasRtnlLock == VMK_FALSE) {
            rtnl_unlock();
         }

         return ret;
      }

      if (dev->do_ioctl) {
         if (callerHasRtnlLock == VMK_FALSE) {
            rtnl_lock();
         }
         VMKAPI_MODULE_CALL(dev->module_id, *result, dev->do_ioctl, dev,
            args, cmd);
         if (callerHasRtnlLock == VMK_FALSE) {
            rtnl_unlock();
         }
         ret = VMK_OK;
      } else {
         ret = VMK_NOT_SUPPORTED;
      }
   } else {
      VMKLNX_DEBUG(0, "net_device: %p, cmd: 0x%x, args: %p, result: %p",
          dev, cmd, args, result);
      ret = VMK_FAILURE;
   }

   return ret;
}

/*
 *----------------------------------------------------------------------------
 *
 *  link_state_work_cb --
 *
 *    Periodic work function to check the status of various physical NICS.
 *
 *  Results:
 *    None.
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static void
link_state_work_cb(struct work_struct *work)
{
   struct net_device *cur;
   uint32_t result;
   unsigned speed = 0, duplex = 0, linkState = 0;
   VMK_ReturnStatus status;
   unsigned newLinkStateTimerPeriod;
   struct ethtool_cmd *cmd;

   cmd = compat_alloc_user_space(sizeof(*cmd));
   if (cmd == NULL) {
      VMKLNX_WARN("Aborting link state watchdog due to compat_alloc_user_space() failure.");
      goto reschedule_work;
   }

   /*
    * Since the ethtool ioctls require the rtnl_lock,
    * we should acquire the lock first before getting
    * dev_base_lock. This is the order used by other
    * code paths that require both locks.
    */
   rtnl_lock();
   write_lock(&dev_base_lock);

   cur = dev_base;
   while (cur) {
      struct ifreq ifr;

      vmk_Bool link_changed = VMK_FALSE;


      memset(&ifr, 0, sizeof(ifr));
      memcpy(ifr.ifr_name, cur->name, sizeof(ifr.ifr_name));

      /* get link speed and duplexity */
      put_user(ETHTOOL_GSET, &cmd->cmd);
      ifr.ifr_data = (void *) cmd;
      if (netdev_ioctl(cur, SIOCETHTOOL, &ifr, &result,
                       VMK_IOCTL_CALLER_64, VMK_TRUE) == VMK_OK) {
         get_user(speed, &cmd->speed);
         get_user(duplex, &cmd->duplex);
      }

      /* get link state */
      put_user(ETHTOOL_GLINK, &cmd->cmd);
      ifr.ifr_data = (void *) cmd;
      if (netdev_ioctl(cur, SIOCETHTOOL, &ifr, &result,
                       VMK_IOCTL_CALLER_64, VMK_TRUE) == VMK_OK) {
         struct ethtool_value value;
         copy_from_user(&value, cmd, sizeof(struct ethtool_value));
         linkState = value.data ?  VMKLNX_UPLINK_LINK_UP :
                     VMKLNX_UPLINK_LINK_DOWN;
      }

      /* set speed, duplexity and link state if changed */
      if (cur->link_state != linkState) {
         cur->link_state = linkState;
         link_changed = VMK_TRUE;
         if (linkState == VMKLNX_UPLINK_LINK_DOWN) {
            /* Tell people we are going down */
            call_netdevice_notifiers(NETDEV_GOING_DOWN, cur);
         } else {
            call_netdevice_notifiers(NETDEV_UP, cur);
         }
	 netif_toggled_clear(cur);
      } else if(netif_carrier_ok(cur)) {
         if (netif_toggled_test_and_clear(cur)) {
            /* Tell people we had a link flap */
            VMKLNX_DEBUG(0, "link flap on %s", cur->name);
            call_netdevice_notifiers(NETDEV_GOING_DOWN, cur);
            call_netdevice_notifiers(NETDEV_UP, cur);
         }
      }
      if (netif_carrier_ok(cur)) {
         if (cur->full_duplex != duplex) {
            cur->full_duplex = duplex;
            link_changed = VMK_TRUE;
         }
         if (cur->link_speed != speed) {
            cur->link_speed = speed;
            link_changed = VMK_TRUE;
         }
      }
      if (link_changed) {
         SetNICLinkStatus(cur);
      }

      cur = cur->next;
   }

   write_unlock(&dev_base_lock);
   rtnl_unlock();

reschedule_work:
   status = vmk_ConfigParamGetUint(linkStateTimerPeriodConfigHandle,
                                   &newLinkStateTimerPeriod);
   VMK_ASSERT(status == VMK_OK);
   if (linkStateTimerPeriod != newLinkStateTimerPeriod) {
      linkStateTimerPeriod = newLinkStateTimerPeriod;
   }
   schedule_delayed_work(&linkStateWork,
                         msecs_to_jiffies(linkStateTimerPeriod));

   /* Periodic update of the LRO config option */
   status = vmk_ConfigParamGetUint(vmklnxLROEnabledConfigHandle,
                                   &vmklnxLROEnabled);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_ConfigParamGetUint(vmklnxLROMaxAggrConfigHandle,
                                   &vmklnxLROMaxAggr);
   VMK_ASSERT(status == VMK_OK);
}

/*
 *----------------------------------------------------------------------------
 *
 *  netdev_watchdog --
 *
 *    Device watchdog
 *
 *  Results:
 *    None.
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static void
netdev_watchdog(struct net_device *dev)
{
   int some_queue_stopped = 0;

   netif_tx_lock(dev);
   if (netif_device_present(dev) &&
       /* don't bother if the device is being closed */
       netif_running(dev) &&
       /* only after the device is opened */
       (dev->flags & IFF_UP) &&
       netif_carrier_ok(dev)) {
      unsigned int i;

      for (i = 0; i < dev->real_num_tx_queues; i++) {
         struct netdev_queue *txq;

         txq = netdev_get_tx_queue(dev, i);
         if (netif_tx_queue_stopped(txq)) {
            some_queue_stopped = 1;
            break;
         }
      }

      if (some_queue_stopped &&
          time_after(jiffies, (dev->trans_start +
                               dev->watchdog_timeo))) {
         VMKLNX_WARN("NETDEV WATCHDOG: %s: transmit timed out", dev->name);

         dev->watchdog_timeohit_stats++;
         vmk_UplinkWatchdogTimeoutHit(dev->uplinkDev);

         /* call driver to reset the device */
         VMKAPI_MODULE_CALL_VOID(dev->module_id, dev->tx_timeout, dev);
         WARN_ON_ONCE(1);

#ifdef VMX86_DEBUG
         // PR 167776: Reset counter every hour or so. We'll panic
         // only if we go beyond a certain number of watchdog timouts
         // in an hour.
         if (time_after(jiffies,
                        dev->watchdog_timeohit_period_start + NETDEV_TICKS_PER_HOUR)) {
            dev->watchdog_timeohit_cnt = 0;
            dev->watchdog_timeohit_period_start = jiffies;
         }

         if (!VMKLNX_STRESS_DEBUG_OPTION(stressNetIfFailTxAndStopQueue)) {
            dev->watchdog_timeohit_cnt++;

            if (dev->watchdog_timeohit_cnt >= dev->watchdog_timeohit_cfg) {
               dev->watchdog_timeohit_cnt = 0;
               if (dev->watchdog_timeohit_panic == VMKLNX_UPLINK_WATCHDOG_PANIC_MOD_ENABLE) {
                  VMK_ASSERT_BUG(VMK_FALSE);
               }
            }
         }
#endif
      }
   }
   netif_tx_unlock(dev);
}


/*
 *----------------------------------------------------------------------------
 *
 *  watchdog_timer_cb --
 *
 *    Watchdog timer callback for all registered devices.
 *
 *  Results:
 *    None.
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static void
watchdog_work_cb(struct work_struct *work)
{
   struct net_device *dev = NULL;

   write_lock(&dev_base_lock);

   for (dev = dev_base; dev; dev = dev->next) {
      netdev_watchdog(dev);
   }

   write_unlock(&dev_base_lock);

   schedule_delayed_work(&watchdogWork,
                         msecs_to_jiffies(WATCHDOG_DEF_TIMER));
}

/**
 * __dev_get_by_name - find a device by its name
 * @name: name to find
 *
 * Find an interface by name. The returned handle does not have the
 * usage count incremented and the caller must be careful defore using
 * the handle.  %NULL is returned if no matching device is found.
 *
 * RETURN VALUE:
 *  Pointer to device structure on success
 *  %NULL is returned if no matching device is found
 */
/* _VMKLNX_CODECHECK_: __dev_get_by_name */
struct net_device *
__dev_get_by_name(const char *name)
{
   struct net_device *dev;

   read_lock(&dev_base_lock);

   dev = dev_base;
   while (dev) {
      if (!strncmp(dev->name, name, sizeof(dev->name))) {
         break;
      }
      dev = dev->next;
   }

   read_unlock(&dev_base_lock);

   return dev;
}
EXPORT_SYMBOL(__dev_get_by_name);

/**
 * dev_get_by_name - find a device by its name
 * @name: name to find
 *
 * Find an interface by name. The returned handle has the usage count
 * incremented and the caller must use dev_put() to release it when it
 * is no longer needed. %NULL is returned if no matching device is
 * found.
 *
 * RETURN VALUE:
 *  Pointer to device structure on success
 *  %NULL is returned if no matching device is found
 */
/* _VMKLNX_CODECHECK_: dev_get_by_name */
struct net_device *
dev_get_by_name(const char *name)
{
   struct net_device *dev;

   dev = __dev_get_by_name(name);
   if (dev) {
      dev_hold(dev);
   }
   return dev;
}
EXPORT_SYMBOL(dev_get_by_name);

/**
 * dev_alloc_name - allocate a name for a device
 * @dev: device
 * @name: name format string
 *
 * Passed a format string - eg "lt%d" it will try and find a suitable
 * id. It scans list of devices to build up a free map, then chooses
 * the first empty slot. Returns the number of the unit assigned or
 * a negative errno code.
 *
 * RETURN VALUE:
 *  Number of the unit assigned on success
 *  Negative errno code on error
 */
/* _VMKLNX_CODECHECK_: dev_alloc_name */
int
dev_alloc_name(struct net_device *dev, const char *name)
{
   int i;
   char buf[VMK_DEVICE_NAME_MAX_LENGTH];
   const int max_netdevices = 8*PAGE_SIZE;
   char *p;

   p = strnchr(name, VMK_DEVICE_NAME_MAX_LENGTH - 1, '%');
   if (p && (p[1] != 'd' || strchr(p+2, '%'))) {
      return -EINVAL;
   }

   for (i = 0; i < max_netdevices; i++) {
      snprintf(buf, sizeof(buf), name, i);

      if (vmk_UplinkIsNameAvailable(buf)) {
         strcpy(dev->name, buf);
         return i;
      }
   }
   return -ENFILE;
}
EXPORT_SYMBOL(dev_alloc_name);

/*
 *----------------------------------------------------------------------------
 *
 *  set_device_pci_name --
 *
 *    Set device's pci name
 *
 *  Results:
 *    None.
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static void
set_device_pci_name(struct net_device *dev, struct pci_dev *pdev)
{
   /* We normally have the pci device name, because
    * execfg-init or esxcfg-init-eesx generates the pci device names.
    *
    * We just override it with the one named by the driver.
    */
   VMK_ASSERT_ON_COMPILE(VMK_DEVICE_NAME_MAX_LENGTH >= IFNAMSIZ);
   if (LinuxPCI_IsValidPCIBusDev(pdev)) {
      LinuxPCIDevExt *pe = container_of(pdev, LinuxPCIDevExt, linuxDev);
      vmk_PCISetDeviceName(pe->vmkDev, dev->name);
      strncpy(pdev->name, dev->name,  sizeof(pdev->name));
   }
   if (strnlen(dev->name, VMK_DEVICE_NAME_MAX_LENGTH) > (IFNAMSIZ - 1)) {
      VMKLNX_WARN("Net device name length(%zd) exceeds IFNAMSIZ - 1(%d)",
                  strnlen(dev->name, VMK_DEVICE_NAME_MAX_LENGTH), IFNAMSIZ - 1);
   }
}

/**
 * register_netdevice	- register a network device
 * @dev: device to register
 *
 * Take a completed network device structure and add it to the kernel
 * interfaces. 0 is returned on success. A negative errno code is returned
 * on a failure to set up the device, or if the name is a duplicate.
 *
 * RETURN VALUE:
 *  0 on success
 *  negative errno code on error
 */
/* _VMKLNX_CODECHECK_: register_netdevice */
int
register_netdevice(struct net_device *dev)
{
   int ret = 0;

   /*
    * netif_napi_add can be called before register_netdev, unfortunately. 
    * fail register_netdev, if the prior napi_add had failed. it's most
    * likely a low memory condition and we'll fail somewhere further down
    * the line if we go on.
    */
   if (dev->reg_state == NETREG_EARLY_NAPI_ADD_FAILED) {
      VMKLNX_WARN("%s: early napi registration failed, bailing", dev->name);
      ret = -EIO;
      goto out;
   }
 
   netdev_init_queue_locks(dev);
   dev->iflink = -1;
   dev->vlan_group = NULL;

   /* Init, if this function is available */
   int rv = 0;
   if (dev->init != 0) {
      VMKAPI_MODULE_CALL(dev->module_id, rv, dev->init, dev);
      if (rv != 0) {
         ret = -EIO;
         goto out;
      }
   }
 
   if (netdev_poll_init(dev) != VMK_OK) {      
      ret = -ENOMEM;
      goto err_uninit;
   }

   set_bit(__LINK_STATE_PRESENT, &dev->state);

   write_lock(&dev_base_lock);
   
   /* CNA devices don't belong to the same uplink namespace. */
   if (dev->features & NETIF_F_CNA) {
      if (LinuxCNA_RegisterNetDev(dev) != VMK_OK) {
         ret = -EIO;
         write_unlock(&dev_base_lock);
         goto err_cna_reg;
      }
   } else {
      dev->next = dev_base;
      dev_base = dev;
   }
   
   
   write_unlock(&dev_base_lock);

   dev_hold(dev);
   dev->reg_state = NETREG_REGISTERED;

 out:
   return ret;

 err_cna_reg:
    netdev_poll_cleanup(dev);
 err_uninit:
   if (dev->uninit) {
      VMKAPI_MODULE_CALL_VOID(dev->module_id, dev->uninit, dev);
   }
   goto out;
}

/**
 * register_netdev	- register a network device
 * @dev: device to register
 *
 * Take a completed network device structure and add it to the kernel
 * interfaces. 0 is returned on success. A negative errno code is returned
 * on a failure to set up the device, or if the name is a duplicate.
 *
 * This is a wrapper around register_netdevice that expands the device name
 * if you passed a format string to alloc_netdev.
 *
 * RETURN VALUE:
 *  0 on success
 *  negative errno code on error
 */
/* _VMKLNX_CODECHECK_: register_netdev */
int
register_netdev(struct net_device *dev)
{
   int err = 0;

   rtnl_lock();

   if (strchr(dev->name, '%')) {
      err = dev_alloc_name(dev, dev->name);
   } else if (dev->name[0]==0 || dev->name[0]==' ') {
      err = dev_alloc_name(dev, "vmnic%d");
   }

   if (err >= 0) {
      struct pci_dev *pdev = dev->pdev;

      if (dev->useDriverNamingDevice) {
         /* net_device already named, we need update the PCI device name list */
         set_device_pci_name(dev, pdev);
      }
      err = register_netdevice(dev);
   }

   rtnl_unlock();

   if (dev->pdev == NULL) {
      /*
       * For pseudo network interfaces, we connect and open the
       * uplink at this point. For Real PCI NIC's, they do
       * this in pci_announce_device() and vmk_PCIPostInsert()
       * respectively.
       */
      if (LinNet_ConnectUplink(dev, NULL)
          || (vmk_UplinkOpen(dev->uplinkDev) != VMK_OK)) {
         err = -EIO;
      }
   }

   return err;
}
EXPORT_SYMBOL(register_netdev);

int
unregister_netdevice(struct net_device *dev)
{
   struct net_device **cur;

   VMK_ASSERT(atomic_read(&dev->refcnt) == 1);

   if (dev->nicMajor > 0) {
      vmkplxr_UnregisterChardev(dev->nicMajor, 0, dev->name);
   }

   if (dev->flags & IFF_UP) {
      dev_close(dev);
   }

   VMK_ASSERT(dev->reg_state == NETREG_REGISTERED);
   dev->reg_state = NETREG_UNREGISTERING;

   if (dev->uninit) {
      VMKAPI_MODULE_CALL_VOID(dev->module_id, dev->uninit, dev);
   }

   /* CNA devices don't belong to the same uplink namespace. */
   if (dev->features & NETIF_F_CNA) {
      LinuxCNA_UnRegisterNetDev(dev);
   } else {
      write_lock(&dev_base_lock);
      cur = &dev_base;
      while (*cur && *cur != dev) {
         cur = &(*cur)->next;
      }
      if (*cur) {
         *cur = (*cur)->next;
      }
      write_unlock(&dev_base_lock);
   }  

   dev->reg_state = NETREG_UNREGISTERED;

   netdev_poll_cleanup(dev);

   VMK_ASSERT(dev->vlan_group == NULL);
   if (dev->vlan_group) {
      vmk_HeapFree(VMK_MODULE_HEAP_ID, dev->vlan_group);
      dev->vlan_group = NULL;
   }

   netdev_destroy_queue_locks(dev);

   /*
    * Disassociate the pci_dev from this net device
    */
   if (dev->pdev != NULL) {
      dev->pdev->netdev = NULL;
      dev->pdev = NULL;
   }

   dev_put(dev);
   return 0;
}

/**
 * unregister_netdev - remove device from the kernel
 * @dev: device
 *
 * This function shuts down a device interface and removes it from the
 * kernel tables.
 *
 * This is just a wrapper for unregister_netdevice. In general you want
 * to use this and not unregister_netdevice.
 *
 * RETURN VALUE:
 *  None
 */
/* _VMKLNX_CODECHECK_: unregister_netdev */
void
unregister_netdev(struct net_device *dev)
{
   unsigned long warning_time;

   VMKLNX_DEBUG(0, "Unregistering %s", dev->name);

   if (dev->pdev == NULL) {
      /*
       * Close and disconnect the uplink here if
       * the device is a pseudo NIC. For real PCI
       * NIC, the uplink is closed and disconnected
       * via vmk_PCIDoPreRemove().
       */
      vmk_UplinkClose(dev->uplinkDev);
   }

   /*
    * Fixed PR366444 - Moved the 'refcnt' check here from within
    * unregister_netdevice()
    *
    * We will be stuck in the while loop below if someone forgot
    * to drop the reference count.
    */
   warning_time = jiffies;
   rtnl_lock();
   while (atomic_read(&dev->refcnt) > 1) {
      rtnl_unlock();

      if ((jiffies - warning_time) > 10*HZ) {
         VMKLNX_WARN("waiting for %s to become free. Usage count = %d",
                     dev->name, atomic_read(&dev->refcnt));
         warning_time = jiffies;
      }

      current->state = TASK_INTERRUPTIBLE;
      schedule_timeout(HZ/4);
      current->state = TASK_RUNNING;

      rtnl_lock();
   }

   unregister_netdevice(dev);
   rtnl_unlock();
   VMKLNX_DEBUG(0, "Done Unregistering %s", dev->name);
}
EXPORT_SYMBOL(unregister_netdev);

/*
 * register_netdevice_notifier - register a network notifier block
 * @nb: notifier
 *
 * Register a notifier to be called when network device events occur.
 * When registered, all registration and up events are replayed
 * to the new notifier to allow device to have a race free
 * view of the network device list.
 *
 * RETURN VALUE:
 *      0 on success, -1 on failure.
 */
/* _VMKLNX_CODECHECK_: register_netdevice_notifier */

int register_netdevice_notifier(struct notifier_block *nb)
{
   return atomic_notifier_chain_register(&netdev_notifier_list, nb);
}   
EXPORT_SYMBOL(register_netdevice_notifier);

/*
 * unregister_netdevice_notifier - unregister a network notifier block
 * @nb: notifier
 *
 * Unregister a previously regustered notifier block.
 *
 * RETURN VALUE:
 *      0 on success, -1 on failure.
 */
/* _VMKLNX_CODECHECK_: unregister_netdevice_notifier */
int unregister_netdevice_notifier(struct notifier_block *nb)
{
   return atomic_notifier_chain_unregister(&netdev_notifier_list, nb);
}   
EXPORT_SYMBOL(unregister_netdevice_notifier);

int call_netdevice_notifiers(unsigned long val, void *v)  
{
   return atomic_notifier_call_chain (&netdev_notifier_list, val, 
                                      (struct net_device *)v);
}

/*
 *-----------------------------------------------------------------------------
 *
 * create_dev_name --
 *
 * create a unique name for a network device.
 *
 * Results:
 *      none
 *
 * Side effects:
 *      pdev->name field is set to vmnic%d
 *
 *-----------------------------------------------------------------------------
 */
static void
create_dev_name(char *name, int length)
{
   /*
    * We use 32 as the starting number because we do not want to overlap with
    * the names used in the initprocess.  It is assumed that the first 32
    * devices (vmnic0 - vmnic31) may be used during boot.
    */
   #define NET_ANON_START_ID VMK_CONST64U(32)
   static vmk_atomic64 nameCounter = NET_ANON_START_ID;

   snprintf(name, length, "vmnic%"VMK_FMT64"u",
            vmk_AtomicReadInc64(&nameCounter));
}

/*
 *-----------------------------------------------------------------------------
 *
 * netdev_name_adapter --
 *
 *      Set the PCI adapter name, if not already set.  If the PCI adapter
 *      already has a name and the name is registered as an uplink then
 *      create a new name for a new uplink port.  Copy it to the net_device
 *      structure.
 *
 * Results:
 *      none
 *
 * Side effects:
 *      dev->name field is set.
 *
 *-----------------------------------------------------------------------------
 */
static void
netdev_name_adapter(struct net_device *dev, struct pci_dev *pdev)
{
   LinuxPCIDevExt *pe;
   char devName[VMK_DEVICE_NAME_MAX_LENGTH];
   char *name = NULL;

   if (pdev == NULL) {
      // Pseudo devices may handle their own naming.
      if (dev->name[0] != 0) {
         return;
      }
      create_dev_name(dev->name, sizeof dev->name);
      VMKLNX_INFO("Pseudo device %s", dev->name);
      return;
   }

   pe = container_of(pdev, LinuxPCIDevExt, linuxDev);

   /* Make sure a name exists */
   devName[0] = '\0';
   vmk_PCIGetDeviceName(pe->vmkDev, devName, sizeof devName);

   /*
    * If we do not have a name for the physical device then create one, else
    * if the uplink port has already been registered then we assume that the
    * we are called for a new port on the device and therefore create a new
    * which we are not passing on to the physical device.
    */
   if (devName[0] == '\0') {
      create_dev_name(pdev->name, sizeof pdev->name);
      vmk_PCISetDeviceName(pe->vmkDev, pdev->name);
      name = pdev->name;
      VMKLNX_INFO("%s at " PCI_DEVICE_BUS_ADDRESS, pdev->name,
                  pci_domain_nr(pdev->bus),
                  pdev->bus->number, 
                  PCI_SLOT(pdev->devfn), 
                  PCI_FUNC(pdev->devfn));
   } else {
      if (!vmk_UplinkIsNameAvailable(devName)) {
	 create_dev_name(pdev->name, sizeof pdev->name);
         name = pdev->name;
      } else {
         name = devName;
         /*
          * If we already have a name for the physical device in vmkernel, 
          * copy the name into pdev->name. 
          */
         snprintf(pdev->name, sizeof(pdev->name), "%s", name);
      }
   }

   /*
    * Give the PCI device name to net_device
    */
   snprintf(dev->name, sizeof (dev->name), "%s", name);

}

/*
 *----------------------------------------------------------------------------
 *
 *  netdev_query_capabilities --
 *
 *    Checks hardware device's capability and return the information in a
 *    32 bit "capability" value
 *
 *  Results:
 *    None.
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static vmk_UplinkCapabilities
netdev_query_capabilities(struct net_device *dev)
{
   vmk_UplinkCapabilities capability = 0;
   VMK_ReturnStatus status;
   unsigned int permitHwIPv6Csum = 0;
   unsigned int permitHwCsumForIPv6Csum = 0;
   unsigned int permitHwTSO6 = 0;
   unsigned int permitHwTSO = 0;
   vmk_MA maxPhysAddr = vmk_MachMemMaxAddr();

   status = vmk_ConfigParamGetUint(useHwIPv6CsumHandle, &permitHwIPv6Csum);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_ConfigParamGetUint(useHwCsumForIPv6CsumHandle, &permitHwCsumForIPv6Csum);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_ConfigParamGetUint(useHwTSOHandle, &permitHwTSO);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_ConfigParamGetUint(useHwTSO6Handle, &permitHwTSO6);
   VMK_ASSERT(status == VMK_OK);

   VMKLNX_DEBUG(0, "Checking device: %s's capabilities", dev->name);
   if (dev->features & NETIF_F_HW_VLAN_TX) {
      VMKLNX_DEBUG(0, "device: %s has hw_vlan_tx capability", dev->name);
      vmk_UplinkCapabilitySet(&capability, VMK_PORT_CLIENT_CAP_HW_TX_VLAN, VMK_TRUE);
   }
   if (dev->features & NETIF_F_HW_VLAN_RX) {
      VMKLNX_DEBUG(0, "device: %s has hw_vlan_rx capability", dev->name);
      vmk_UplinkCapabilitySet(&capability, VMK_PORT_CLIENT_CAP_HW_RX_VLAN, VMK_TRUE);
   }
   if (dev->features & NETIF_F_IP_CSUM) {
      VMKLNX_DEBUG(0, "device: %s has IP CSUM capability", dev->name);
      vmk_UplinkCapabilitySet(&capability, VMK_PORT_CLIENT_CAP_IP4_CSUM, VMK_TRUE);
   }
   if (permitHwIPv6Csum) {
      if (dev->features & NETIF_F_IPV6_CSUM) {
         VMKLNX_DEBUG(0, "device: %s has IPV6 CSUM capability", dev->name);
         vmk_UplinkCapabilitySet(&capability, VMK_PORT_CLIENT_CAP_IP6_CSUM, VMK_TRUE);
      } else {
         /*
          * When NETIF_F_IPV6_CSUM isn't available, then software
          * CSUM for IP6 headers will be done.  If software csum
          * is included, there's no reason to also examine the pktLists
          * for ip6 extension header offloads
          */
         if (!(dev->features & NETIF_F_HW_CSUM)) {
            vmk_UplinkCapabilitySet(&capability,
                                    VMK_PORT_CLIENT_CAP_IP6_CSUM_EXT_HDRS,
                                    VMK_TRUE);
         }
      }
   }
   if (dev->features & NETIF_F_HW_CSUM) {
      VMKLNX_DEBUG(0, "device: %s has HW CSUM capability", dev->name);
      // IP is the subset of HW we support.
      vmk_UplinkCapabilitySet(&capability, VMK_PORT_CLIENT_CAP_IP4_CSUM, VMK_TRUE);
      if (permitHwCsumForIPv6Csum) {
         VMKLNX_DEBUG(0, "device: %s has HW CSUM => IPv6 CSUM capability", dev->name);
         vmk_UplinkCapabilitySet(&capability, VMK_PORT_CLIENT_CAP_IP6_CSUM, VMK_TRUE);
      }
   }
   if ((dev->features & NETIF_F_SG) &&
       (MAX_SKB_FRAGS >= VMK_PKT_FRAGS_MAX_LENGTH)) {
      VMKLNX_DEBUG(0, "device: %s has SG capability", dev->name);
      vmk_UplinkCapabilitySet(&capability, VMK_PORT_CLIENT_CAP_SG, VMK_TRUE);
   }
   if (!(dev->features & NETIF_F_FRAG_CANT_SPAN_PAGES)) {
      VMKLNX_DEBUG(0, "device: %s has Frag Span Pages capability", dev->name);
      vmk_UplinkCapabilitySet(&capability, VMK_PORT_CLIENT_CAP_SG_SPAN_PAGES,
                              VMK_TRUE);
   }
   if ((dev->features & NETIF_F_HIGHDMA) ||
      ((dev->features & NETIF_F_DMA39) && maxPhysAddr <= DMA_BIT_MASK(39)) ||
      ((dev->features & NETIF_F_DMA40) && maxPhysAddr <= DMA_BIT_MASK(40)) ||
      ((dev->features & NETIF_F_DMA48) && maxPhysAddr <= DMA_BIT_MASK(48))) {
      VMKLNX_DEBUG(0, "device: %s has high dma capability", dev->name);
      vmk_UplinkCapabilitySet(&capability, VMK_PORT_CLIENT_CAP_HIGH_DMA, VMK_TRUE);
   }
   if (permitHwTSO && (dev->features & NETIF_F_TSO)) {
      VMKLNX_DEBUG(0, "device: %s has TSO capability", dev->name);
      vmk_UplinkCapabilitySet(&capability, VMK_PORT_CLIENT_CAP_TSO, VMK_TRUE);
   }

   if (permitHwTSO6) {
      if (dev->features & NETIF_F_TSO6) {
         VMKLNX_DEBUG(0, "device: %s has TSO6 capability", dev->name);
         vmk_UplinkCapabilitySet(&capability, VMK_PORT_CLIENT_CAP_TSO6, VMK_TRUE);
      } else {
         /*
          * When NETIF_F_TSO6 isn't available, then software TSO6
          * will be done, but when software TSO6 is enabled, there's
          * no reason to also review the pktLists for IP6 extension
          * headers.
          */
         vmk_UplinkCapabilitySet(&capability,
                                 VMK_PORT_CLIENT_CAP_TSO6_EXT_HDRS,
                                 VMK_TRUE);
      }
   }

   if (dev->features & NETIF_F_UPT) {
      VMKLNX_DEBUG(0, "device: %s has UPT capability", dev->name);
      vmk_UplinkCapabilitySet(&capability, VMK_PORT_CLIENT_CAP_UPT, VMK_TRUE);
   }

   if (dev->pt_ops && !(dev->features & NETIF_F_UPT)) {
      VMKLNX_DEBUG(0, "device: %s has NPA capability", dev->name);
      vmk_UplinkCapabilitySet(&capability, VMK_PORT_CLIENT_CAP_NPA, VMK_TRUE);
   }

   if (dev->dcbnl_ops) {
      VMKLNX_DEBUG(0, "device: %s has DCB capability", dev->name);
      vmk_UplinkCapabilitySet(&capability, VMK_PORT_CLIENT_CAP_DCB, VMK_TRUE);
   }

   /*
    * All devices have the RDONLY_INETHDRS capability.  Its a property
    * of a device driver, when VMK_TRUE, it means the device driver does
    * NOT modify the inet headers.  When VMK_FALSE, it means the device
    * driver DOES modify the inet headers, and that privte copies of
    * the pktHandles need to be make for the safety of the pktHandles
    * without priavate writable buffers.
    */
   if (dev->features & NETIF_F_RDONLYINETHDRS) {
      VMKLNX_DEBUG(0, "device: %s has RDONLY_INETHDRS capability", dev->name);
      vmk_UplinkCapabilitySet(&capability,
                              VMK_PORT_CLIENT_CAP_RDONLY_INETHDRS, VMK_TRUE);
   } else {
      VMKLNX_DEBUG(0, "device: %s does not have RDONLY_INETHDRS capability",
                   dev->name);
      vmk_UplinkCapabilitySet(&capability,
                              VMK_PORT_CLIENT_CAP_RDONLY_INETHDRS, VMK_FALSE);
   }

   /*
    * PR #324545: Artificially turn this feature on so that the VMkernel
    * doesn't activate any unnecessary & wasteful SW workaround.
    * The VMkernel shouldn't generate this kind of frames anyway.
    */
   if (VMK_TRUE) {
      VMKLNX_DEBUG(0, "device: %s has TSO256k capability", dev->name);
      vmk_UplinkCapabilitySet(&capability, VMK_PORT_CLIENT_CAP_TSO256k, VMK_TRUE);
   }

   if (dev->features & NETIF_F_TSO) {
      /*
       *  If a pNIC can do TSO, but not any of the following,
       *  our software path for any of these missing functions
       *  may end up trying to allocate very large buffers and
       *  not able to do it.  We'd like to know about such
       *  devices during development.  
       * NB: we already know that some e1000 devices,
       *      e.g. 82544EI (e1000 XT), can do TSO but not High_DMA.
       */
      VMK_ASSERT(dev->features & NETIF_F_SG);
      VMK_ASSERT(!(dev->features & NETIF_F_FRAG_CANT_SPAN_PAGES));

      if (!(dev->features & NETIF_F_SG) ||
          (dev->features & NETIF_F_FRAG_CANT_SPAN_PAGES)) {
         VMKLNX_WARN("%s: disabling hardware TSO because dev "
                     "has no hardware SG",
                     dev->name);
	 vmk_UplinkCapabilitySet(&capability, VMK_PORT_CLIENT_CAP_TSO, VMK_FALSE);
      }
   }

   /*
    * To support encapsulated offloads, the pNic must be able to
    * parameterize the location of the header, csum, etc.  Some
    * nics can parameterize, some can't.  Some nics use 8-bit
    * offsets, some use 16-bits.
    *
    */

   if (dev->features & NETIF_F_OFFLOAD_16OFFSET) {
      VMKLNX_DEBUG(0, "device: %s has TSO-CSUM offloads "
                   "with 16 bit offsets (8-bit also enabled)",
                   dev->name);

      vmk_UplinkCapabilitySet(&capability,
                              VMK_PORT_CLIENT_CAP_OFFLOAD_16OFFSET,
                              VMK_TRUE);
      if (!((dev->features & NETIF_F_OFFLOAD_8OFFSET))) {
         VMKLNX_DEBUG(0, "device: %s missing 8-bit offsets also enabled",
                      dev->name);
      }
      vmk_UplinkCapabilitySet(&capability,
                              VMK_PORT_CLIENT_CAP_OFFLOAD_8OFFSET,
                              VMK_TRUE);
   } else if (dev->features & NETIF_F_OFFLOAD_8OFFSET) {
      VMKLNX_DEBUG(0, "device: %s has TSO-CSUM with 8 bit offset capability",
                   dev->name);

      vmk_UplinkCapabilitySet(&capability,
                              VMK_PORT_CLIENT_CAP_OFFLOAD_8OFFSET,
                              VMK_TRUE);
   } else {
      VMKLNX_DEBUG(0, "device: %s no TSO-CSUM offset capability",
                   dev->name);
      /*
       * By enabling 16OFFSET, 8OFFSET is still disabled,and the
       * software version of the cap will be inserted.
       */
      vmk_UplinkCapabilitySet(&capability,
                              VMK_PORT_CLIENT_CAP_OFFLOAD_16OFFSET,
                              VMK_TRUE);
   }

   if (!(dev->features & NETIF_F_NO_SCHED)) {
      vmk_UplinkCapabilitySet(&capability,
                              VMK_PORT_CLIENT_CAP_SCHED,
                              VMK_TRUE);
      VMKLNX_DEBUG(0, "device: %s is network scheduling compliant",
                   dev->name);
   } else {
      VMKLNX_DEBUG(0, "device: %s is not network scheduling compliant",
                   dev->name);
   }

   VMKLNX_DEBUG(0, "device %s vmnet cap is 0x%"VMK_FMT64"x",
                dev->name, capability);

   return capability;
}

/*
 * Section: calltable functions, called through vmk_UplinkFunctions
 */

/*
 *----------------------------------------------------------------------------
 *
 *  IoctlNetDev --
 *
 *    Handle an ioctl request from the VMKernel for the given device name.
 *
 *  Results:
 *    VMK_ReturnStatus indicating the outcome.
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
IoctlNetDev(char *uplinkName, uint32_t cmd, void *args, uint32_t *result)
{
   VMK_ReturnStatus status;
   struct net_device *dev;

   dev = dev_get_by_name(uplinkName);
   if (!dev) {
      return VMK_NOT_FOUND;
   }

   status = netdev_ioctl(dev, cmd, args, result, VMK_IOCTL_CALLER_64, VMK_FALSE);
   dev_put(dev);
   return status;
}

/*
 *-----------------------------------------------------------------------------
 *
 * SetNICLinkStatus --
 *
 *      Push new link status up to the vmkernel.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      May cause teaming failover events to be scheduled.
 *
 *-----------------------------------------------------------------------------
 */
void
SetNICLinkStatus(struct net_device *dev)
{
   vmk_UplinkLinkInfo linkInfo;

   linkInfo.linkState  = dev->link_state;
   linkInfo.linkSpeed  = linkInfo.linkState ? dev->link_speed  : 0;
   linkInfo.fullDuplex = linkInfo.linkState ? dev->full_duplex : VMK_FALSE;

   /* Test if the uplink is connected (for a pseudo device) */
   if (dev->uplinkDev) {
      vmk_UplinkUpdateLinkState(dev->uplinkDev, &linkInfo);
   }
}

/*
 *----------------------------------------------------------------------------
 *
 * DevStartTxImmediate --
 *
 *    External entry point for transmitting packets. Packets are queued and
 *    then Tx-ed immediately.
 *
 *
 * Results:
 *    VMK_ReturnStatus indicating the outcome.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
DevStartTxImmediate(void *clientData, vmk_PktList pktList)
{
   struct net_device *dev = (struct net_device *)clientData;
   vmk_PktHandle *pkt = vmk_PktListGetFirstPkt(pktList);
   vmk_NetqueueQueueID vmkqid;

   VMK_ASSERT(pkt);
   vmkqid = vmk_PktQueueIDGet(pkt);
#ifdef VMX86_DEBUG
   {
      VMK_PKTLIST_ITER_STACK_DEF(iter);
      vmk_PktListIterStart(iter, pktList);
      while (!vmk_PktListIterIsAtEnd(iter)) {
         pkt = vmk_PktListIterGetPkt(iter);
         VMK_ASSERT(vmk_PktQueueIDGet(pkt) == vmkqid);
         vmk_PktListIterMove(iter);
      }
   }
#endif

   return netdev_tx(dev, pktList, vmkqid);
}  

/*
 *----------------------------------------------------------------------------
 *
 *  OpenNetDev --
 *
 *    Handler for calling the device's open function. If successful, the device
 *    state is changed to indicate that the device has been opened.
 *
 *  Results:
 *    Returns whatever the device's open function returns.
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
OpenNetDev(void *clientData)
{
   struct net_device *dev = (struct net_device *)clientData;
   int status = 0;

   if (dev->open == NULL) {
      VMKLNX_WARN("NULL open function for device %s", dev->name);
      return 1;
   }

   rtnl_lock();
   if ((dev->gflags & IFF_DEV_IS_OPEN) == 0) {
      status = dev_open(dev);
      if (status == 0) {
         dev->gflags |= IFF_DEV_IS_OPEN;
      }
   }
   rtnl_unlock();

   return status == 0 ? VMK_OK : VMK_FAILURE;
}

/*
 *----------------------------------------------------------------------------
 *
 *  CloseNetDev --
 *
 *    Handler for closing the device. If successful, the device state is
 *    modified to indicate that the device is now non-functional.
 *
 *  Results:
 *    Returns whatever the stop function of the module owning the device
 *    returns.
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
CloseNetDev(void *clientData)
{
   struct net_device *dev = (struct net_device *)clientData;
   int status = 0;

   VMK_ASSERT(dev->stop != NULL);
   VMKLNX_DEBUG(0, "Stopping device %s", dev->name);

   rtnl_lock();
   if (dev->gflags & IFF_DEV_IS_OPEN ) {
      status = dev_close(dev);
      if (status == 0) {
         dev->gflags &= ~IFF_DEV_IS_OPEN;
      }
   }
   rtnl_unlock();

   return status == 0 ? VMK_OK : VMK_FAILURE;
}


/*
 *----------------------------------------------------------------------------
 *
 *  BlockNetDev --
 *
 *    Handler for blocking the device. If successful, the device state is
 *    modified to indicate that the device is now blocked.
 *
 *  Results:
 *    VMK_OK always.
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
BlockNetDev(void *clientData)
{
   struct net_device *dev = (struct net_device *)clientData;
   struct napi_struct *napi;

   if (test_and_set_bit(__LINK_STATE_BLOCKED, &dev->state)) {
      VMKLNX_DEBUG(0, "%s is actually already blocked.", dev->name);
      return VMK_OK;
   }

   // Disable napi so as to give a chance for all packets in the middle of
   // rx processing to be handed off to the kernel
   spin_lock(&dev->napi_lock);
   list_for_each_entry(napi, &dev->napi_list, dev_list)
      if (!(test_bit(NAPI_STATE_UNUSED, &napi->state))) {
         while (1) {
            if (!napi_disable_timeout(napi, 50)) {
               // make sure we don't have packets stuck in the napi context
               VMKLNX_DEBUG(0, "Flushing napi context (%d) pending packets for %s",
                            napi->napi_id, dev->name);
               vmk_NetPollProcessRx(napi->net_poll);
               napi_enable (napi);
               break;
            }
            if (test_bit(NAPI_STATE_UNUSED, &napi->state)) {
               break;
            }
         }
      }
   spin_unlock(&dev->napi_lock);

   /* Emulate a case where it takes longer to complete the rx packets in flight */
   if (VMKLNX_STRESS_DEBUG_COUNTER(stressNetBlockDevIsSluggish)) {
      msleep(blockTotalSleepMsec);
   }

   VMKLNX_DEBUG(0, "%s is blocked.", dev->name);
   return VMK_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 *  UnblockNetDev --
 *
 *    Handler for unblocking the device. If successful, the device state is
 *    modified to indicate that the device is now unblocked.
 *
 *  Results:
 *    VMK_OK always.
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
UnblockNetDev(void *clientData)
{
   struct net_device *dev = (struct net_device *)clientData;

   if (!test_bit(__LINK_STATE_BLOCKED, &dev->state)) {
      VMKLNX_DEBUG(0, "%s is actually already unblocked.", dev->name);
      return VMK_OK;
   }

   smp_mb__before_clear_bit();
   clear_bit(__LINK_STATE_BLOCKED, &dev->state);

   VMKLNX_DEBUG(0, "%s is unblocked.", dev->name);
   return VMK_OK;
}


/*
 *-----------------------------------------------------------------------------
 *
 * LinNet_EnableHwVlan --
 *
 *      Enable HW vlan on the netdev
 *      If enable is FALSE, hardware vlan is expected to be enabled already.
 *
 * Results:
 *      Return VMK_OK if there is VLan HW tx/rx acceleration support;
 *      Return VMK_VLAN_NO_HW_ACCEL otherwise.
 *
 * Side effects:
 *      hw vlan register is updated.
 *
 *-----------------------------------------------------------------------------
 */
VMK_ReturnStatus
LinNet_EnableHwVlan(struct net_device *dev)
{
   struct vlan_group *grp = dev->vlan_group;

   /*
    * dev->vlan_group is only allocated after vlan_rx_register() has been
    * called successfully. If dev->vlan_group is not NULL, it means
    * vlan has already been enabled and no need to do it again
    */
   if(grp != NULL) {
      VMKLNX_DEBUG(1, "%s: HW VLAN already enabled", dev->name);
      return VMK_OK;
   }

   VMK_ASSERT(dev->features & NETIF_F_HW_VLAN_RX);

   /* call driver's vlan_rx_register handler to enable vlan */
   VMK_ASSERT(dev->vlan_rx_register);
   if (!dev->vlan_rx_register) {
      VMKLNX_DEBUG(0, "%s: no vlan_rx_register handler", dev->name);
      return VMK_VLAN_NO_HW_ACCEL;
   }

   grp = vmk_HeapAlloc(VMK_MODULE_HEAP_ID, sizeof (struct vlan_group));
   if (grp == NULL) {
      VMKLNX_DEBUG(0, "%s: failed to allocate vlan_group", dev->name);
      return VMK_NO_MEMORY;
   }
   vmk_Memset(grp, 0, sizeof (struct vlan_group));
   dev->vlan_group = grp;

   VMKLNX_DEBUG(0, "%s: enabling vlan", dev->name);
   VMKAPI_MODULE_CALL_VOID(dev->module_id, dev->vlan_rx_register, dev, grp);

   return VMK_OK;
}


/*
 *-----------------------------------------------------------------------------
 *
 * SetupVlanGroupDevice --
 *
 *      Enable HW vlan and add new vlan id's based on the bitmap.
 *      If enable is FALSE, hardware vlan is expected to be enabled
 *      already, If bitmap is null, just do enable.
 *
 * Results:
 *      Return VMK_OK if there is VLan HW tx/rx acceleration support;
 *      Return VMK_VLAN_NO_HW_ACCEL otherwise.
 *
 * Side effects:
 *      hw vlan register is updated.
 *
 *-----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
SetupVlanGroupDevice(void *clientData, vmk_Bool enable, void *bitmap)
{
   struct net_device *dev = (struct net_device *) clientData;
   struct vlan_group *grp = dev->vlan_group;
   VMK_ReturnStatus status;

   rtnl_lock();
   if (enable || grp == NULL) {
      status = LinNet_EnableHwVlan(dev);
      if (status != VMK_OK) {
         goto end;
      }
      grp = dev->vlan_group;
   }

   /* if hw doesn't support rx vlan filter, bail out here */
   if (!(dev->features & NETIF_F_HW_VLAN_FILTER)) {
      status = VMK_OK;
      goto end;
   }

   /* now compare bitmap with vlan_group and make up the difference */
   if (bitmap) {
      vmk_VlanID vid;
      VMK_ASSERT(dev->vlan_rx_add_vid);
      if (!dev->vlan_rx_add_vid) {
         VMKLNX_DEBUG(0, "%s: driver has no vlan_rx_add_vid handler",
                     dev->name);
         status = VMK_FAILURE;
         goto end;
      }

      for (vid = 0; vid < VLAN_GROUP_ARRAY_LEN; vid++) {
         if (test_bit(vid, bitmap) && grp->vlan_devices[vid] == NULL) {
            grp->vlan_devices[vid] = dev;
            VMKLNX_DEBUG(1, "%s: adding vlan id %d", dev->name, (int)vid);
            VMKAPI_MODULE_CALL_VOID(dev->module_id, dev->vlan_rx_add_vid, dev,
                                    vid);
         }
      }
   }
   status = VMK_OK;
  end:
   rtnl_unlock();
   return status;
}

/*
 *-----------------------------------------------------------------------------
 *
 * LinNet_RemoveVlanGroupDevice --
 *
 *      Delete vlan id's based on bitmap and disable hw vlan.
 *      Either bitmap or disable should be set, but not both.
 *      If neither is set, there is no work to do (illegal?).
 *
 * Results:
 *      VMK_OK if successfully added/deleted.
 *      VMK_FAILURE otherwise.
 *
 * Side effects:
 *      HW vlan table is updated. HW will may stop passing vlan.
 *
 *-----------------------------------------------------------------------------
 */
VMK_ReturnStatus
LinNet_RemoveVlanGroupDevice(void *clientData, vmk_Bool disable, void *bitmap)
{
   struct net_device *dev = (struct net_device *) clientData;
   struct vlan_group *grp = dev->vlan_group;
   VMK_ReturnStatus status;

   VMK_ASSERT(dev->features & NETIF_F_HW_VLAN_RX);

   rtnl_lock();
   /* Unregister vid's if hardware supports vlan filter */
   if (dev->features & NETIF_F_HW_VLAN_FILTER) {
      vmk_VlanID vid;
      VMK_ASSERT(dev->vlan_rx_kill_vid);
      if (!dev->vlan_rx_kill_vid) {
         VMKLNX_DEBUG(0, "%s: no vlan_rx_kill_vid handler", dev->name);
         status = VMK_FAILURE;
         goto end;
      }

      if (grp == NULL) { 
         VMKLNX_DEBUG(0, "%s: the vlan_group of this device is NULL", 
                      dev->name); 
         status = VMK_FAILURE;
         goto end; 
      } 

      for (vid = 0; vid < VLAN_GROUP_ARRAY_LEN; vid++) {
         if (grp->vlan_devices[vid] == NULL) {
            continue;
         }
         /* delete all if disable is true, else consult bitmap */
         if (disable || (bitmap && !test_bit(vid, bitmap))) {
            grp->vlan_devices[vid] = NULL;
            VMKLNX_DEBUG(1, "%s: deleting vlan id %d", dev->name, (int)vid);
            VMKAPI_MODULE_CALL_VOID(dev->module_id, dev->vlan_rx_kill_vid, dev,
                                    vid);
         }
      }
   }

   if (disable) {
      VMK_ASSERT(dev->vlan_rx_register);
      if (!dev->vlan_rx_register) {
         VMKLNX_DEBUG(0, "%s: no vlan_rx_register handler", dev->name);
         status = VMK_VLAN_NO_HW_ACCEL;
         goto end;
      }

      VMKLNX_DEBUG(0, "%s: disabling vlan", dev->name);
      VMKAPI_MODULE_CALL_VOID(dev->module_id, dev->vlan_rx_register, dev, NULL);

      VMK_ASSERT(grp);
      if (grp) {
         dev->vlan_group = NULL;
         vmk_HeapFree(VMK_MODULE_HEAP_ID, grp);
      }
   }
   status = VMK_OK;
  end:
   rtnl_unlock();
   return status;
}

/*
 *-----------------------------------------------------------------------------
 *
 * NICGetMTU --
 *
 *      Returns the MTU value for the given NIC
 *
 * Results:
 *      MTU for the given device.
 *
 * Side effects:
 *      none.
 *
 *-----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
NICGetMTU(void *device, vmk_uint32 *mtu)
{
   struct net_device *dev = (struct net_device *) device;

   *mtu = dev->mtu;

   return VMK_OK;
}

/*
 *-----------------------------------------------------------------------------
 *
 * NICSetMTU --
 *
 *      Set new MTU for the given NIC
 *
 * Results:
 *      VMK_OK if the new_mtu is accepted by the device.
 *      VMK_FAILURE or VMK_NOT_SUPPORTED otherwise.
 *
 * Side effects:
 *      The device queue is stopped. For most devices the entire ring is
 *      reallocated, and the device is reset.
 *
 *-----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
NICSetMTU(void *device, vmk_uint32 new_mtu)
{
   int ret = 0;
   struct net_device *dev = (struct net_device *) device;

   if (!dev->change_mtu) { // 3Com doesn't even register change_mtu!
      VMKLNX_DEBUG(0, "Changing MTU not supported by device.");
      return VMK_NOT_SUPPORTED;
   }
   /* PRs 478842, 478939	
    * Update trans_start here so that netdev_watchdog will not mistake
    * a stopped tx_queue as a sign of pNIc hang when change MTU is undergoing.
    */ 
   rtnl_lock();
   dev->trans_start = jiffies;
   VMKAPI_MODULE_CALL(dev->module_id, ret, dev->change_mtu, dev, new_mtu);

   /* Some drivers call dev_close() when change_mtu failed. The following check
    * will update dev->gflags accordingly to avoid a second dev_close()
    * when CloseNetDev() is called.
    */
   if (ret && !(dev->flags & IFF_UP))
      dev->gflags &= ~IFF_DEV_IS_OPEN;
   rtnl_unlock();

   if (ret == 0) {
      VMKLNX_DEBUG(0, "%s: MTU changed to %d", dev->name, new_mtu);
   } else {
      VMKLNX_DEBUG(0, "%s: Failed to change MTU to %d", dev->name, new_mtu);
      return VMK_FAILURE;
   }

   return VMK_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 *  NICSetLinkStateDown --
 *    Set NIC hardware to link down state to inform link peer.
 *
 *  Results:
 *    VMK_OK or failure code.
 *
 *  Side effects:
 *    Device is closed and settings may be lost.
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
NICSetLinkStateDown(struct net_device *dev)
{
   struct ethtool_ops *ops;

   if ((dev->gflags & IFF_DEV_IS_OPEN) == 0) {
      return VMK_OK;
   }

   /* disable wol so link is down */
   ops = dev->ethtool_ops;
   if (ops && ops->set_wol) {
      int error;
      struct ethtool_wolinfo wolInfo[1];
      vmk_LogMessage("Disable WOL on device %s", dev->name);
      memset(wolInfo, 0, sizeof (wolInfo));
      rtnl_lock();
      VMKAPI_MODULE_CALL(dev->module_id, error, ops->set_wol, dev, wolInfo);
      rtnl_unlock();
      if (error != 0) {
         vmk_LogMessage("Failed to disable wol on device %s", dev->name);
      }
   }

   /* now close the device to take the link down */
   return CloseNetDev((void *)dev);
}

/*
 *----------------------------------------------------------------------------
 *
 *  NICSetLinkStateUp --
 *    Set NIC hardware to link up state to inform link peer.
 *
 *  Results:
 *    VMK_OK or failure code.
 *
 *  Side effects:
 *    Device is opened.
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
NICSetLinkStateUp(struct net_device *dev)
{
   VMK_ReturnStatus status;

   if (dev->gflags & IFF_DEV_IS_OPEN) {
      return VMK_OK;      /* nothing to do */
   }

   status = OpenNetDev((void *)dev);
   if (status != VMK_OK) {
      return status;
   }

   /* Now the link is up, unblock device and restore wol state */
   if (UnblockNetDev((void *)dev) != VMK_OK) {
      vmk_LogMessage("Failed to unblock device %s", dev->name);
   }

   /* hostd will reenable wol when it processes link up */
   return VMK_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 *  NICSetLinkStatus --
 *    Set NIC hardware speed and duplex.
 *
 *  Results:
 *    VMK_OK or failure code.
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
NICSetLinkStatus(void *clientData, vmk_UplinkLinkInfo *linkInfo)
{
   struct net_device *dev = (struct net_device *)clientData;
   struct ethtool_cmd cmd;
   uint32_t result;
   VMK_ReturnStatus status;

   if (linkInfo->linkState == VMK_LINK_STATE_DOWN) {
      vmk_LogMessage("Taking down link on device %s", dev->name);
      return NICSetLinkStateDown(dev);
   }

   status = NICSetLinkStateUp(dev);
   if (status != VMK_OK) {
      vmk_LogMessage("Failed to bring link up on device %s", dev->name);
      return status;
   }

   /* get meaningful ethtool_cmd value first */
   if (!dev->ethtool_ops || !dev->ethtool_ops->get_settings) {
      return VMK_NOT_SUPPORTED;
   }

   memset(&cmd, 0, sizeof(struct ethtool_cmd));
   cmd.cmd = ETHTOOL_GSET;
   rtnl_lock();
   VMKAPI_MODULE_CALL(dev->module_id, result, dev->ethtool_ops->get_settings,
                      dev, &cmd);
   rtnl_unlock();

   if (result)
      return vmklnx_errno_to_vmk_return_status(result);

   /* set link speed and duplexity according to linkInfo */
   cmd.cmd = ETHTOOL_SSET;
   if (linkInfo->linkState == VMK_LINK_STATE_DOWN) {
      cmd.autoneg = 1;
      cmd.speed = ~0;
      cmd.duplex = ~0;
   } else {
      cmd.speed = linkInfo->linkSpeed;
      cmd.duplex = linkInfo->fullDuplex;
      if (cmd.speed != 0) {
         cmd.autoneg = 0;
      } else {
         cmd.autoneg = 1;
         cmd.advertising = cmd.supported &
               (ADVERTISED_100baseT_Full |
               ADVERTISED_100baseT_Half |
               ADVERTISED_10baseT_Full |
               ADVERTISED_10baseT_Half |
               ADVERTISED_1000baseT_Full |
               ADVERTISED_1000baseT_Half |
               ADVERTISED_Autoneg |
               ADVERTISED_2500baseX_Full |
               ADVERTISED_10000baseT_Full);
      }
   }

   /*
    * We call ethtool_ops directly to bypass copy_from_user(),
    * which doesn't handle in-kernel buffers (except for BH callers).
    *
    * See ethtool_set_settings()
    */
   if (!dev->ethtool_ops || !dev->ethtool_ops->set_settings) {
       return VMK_NOT_SUPPORTED;
   }

   rtnl_lock();
   VMKAPI_MODULE_CALL(dev->module_id, result, dev->ethtool_ops->set_settings,
                      dev, &cmd);
   rtnl_unlock();
   return vmklnx_errno_to_vmk_return_status(result);
}

/*
 *---------------------------------------------------------------------------->  *
 *  NICResetDev --
 *
 *    Handler for resetting the device. If successful, the device state is
 *    reset and the link state should go down and then up.
 *
 *  Results:
 *    VMK_OK always.
 *
 *  Side effects:
 *    Link state should bounce as seen from physical switch.
 *
 *---------------------------------------------------------------------------->  */

static VMK_ReturnStatus
NICResetDev(void *clientData)
{
   struct net_device *dev = (struct net_device *)clientData;

   netif_tx_lock(dev);
   VMK_ASSERT(dev->tx_timeout != NULL);
   VMKAPI_MODULE_CALL_VOID(dev->module_id, dev->tx_timeout, dev);
   netif_tx_unlock(dev);

   return VMK_OK;
}

static inline VMK_ReturnStatus
marshall_to_vmknetq_features(vmknetddi_queueops_features_t features,
                             vmk_NetqueueFeatures *vmkfeatures)
{
   if (features & VMKNETDDI_QUEUEOPS_FEATURE_RXQUEUES) {
      *vmkfeatures |= VMK_NETQUEUE_FEATURE_RXQUEUES;
   }
   if (features & VMKNETDDI_QUEUEOPS_FEATURE_TXQUEUES) {
      *vmkfeatures |= VMK_NETQUEUE_FEATURE_TXQUEUES;
   }

   return VMK_OK;
}

static inline VMK_ReturnStatus
marshall_from_vmknetq_type(vmk_NetqueueQueueType vmkqtype,
                           vmknetddi_queueops_queue_t *qtype)
{
   if (vmkqtype == VMK_NETQUEUE_QUEUE_TYPE_TX) {
      *qtype = VMKNETDDI_QUEUEOPS_QUEUE_TYPE_TX;
   } else if (vmkqtype == VMK_NETQUEUE_QUEUE_TYPE_RX) {
      *qtype = VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX;
   } else {
      VMKLNX_DEBUG(0, "invalid vmkqueue type 0x%x", (uint32_t)vmkqtype);
      return VMK_FAILURE;
   }

   return VMK_OK;
}

static inline VMK_ReturnStatus
marshall_to_vmknetq_id(vmknetddi_queueops_queueid_t qid,
                       vmk_NetqueueQueueID *vmkqid)
{
   if ( !VMKNETDDI_QUEUEOPS_IS_TX_QUEUEID(qid) &&
        !VMKNETDDI_QUEUEOPS_IS_RX_QUEUEID(qid) ) {
      VMKLNX_WARN("invalid queue id 0x%x", qid);
      return VMK_FAILURE;
   }

   vmk_NetqueueSetQueueIDUserVal(vmkqid, qid);
   return VMK_OK;
}

static inline VMK_ReturnStatus
marshall_from_vmknetq_id(vmk_NetqueueQueueID vmkqid,
                         vmknetddi_queueops_queueid_t *qid)
{
   VMK_DEBUG_ONLY(
   vmk_NetqueueQueueType qtype = vmk_NetqueueQueueIDType(vmkqid);

   if (unlikely((qtype != VMK_NETQUEUE_QUEUE_TYPE_TX) &&
                (qtype != VMK_NETQUEUE_QUEUE_TYPE_RX))) {
      VMKLNX_WARN("invalid vmk queue type 0x%"VMK_FMT64"x", vmkqid);
      return VMK_FAILURE;
   });

   *qid = vmk_NetqueueQueueIDUserVal(vmkqid);
   return VMK_OK;
}

static inline VMK_ReturnStatus
marshall_from_vmknetq_filter_type(vmk_NetqueueFilter *vmkfilter,
                                  vmknetddi_queueops_filter_t *filter)
{
   if (vmkfilter->class != VMK_NETQUEUE_FILTER_MACADDR &&
       vmkfilter->class != VMK_NETQUEUE_FILTER_VLAN &&
       vmkfilter->class != VMK_NETQUEUE_FILTER_VLANMACADDR) {
      VMKLNX_DEBUG(0, "unsupported vmk filter class");
      return VMK_NOT_SUPPORTED;
   }
   
   if (vmkfilter->class == VMK_NETQUEUE_FILTER_MACADDR) {
      filter->class = VMKNETDDI_QUEUEOPS_FILTER_MACADDR;
      memcpy(filter->u.macaddr, vmkfilter->u.macaddr, 6);
   }

   if (vmkfilter->class == VMK_NETQUEUE_FILTER_VLAN) {
      filter->class = VMKNETDDI_QUEUEOPS_FILTER_VLAN;
      filter->u.vlan_id = vmkfilter->u.vlan_id;
   }

   if (vmkfilter->class == VMK_NETQUEUE_FILTER_VLANMACADDR) {
      filter->class = VMKNETDDI_QUEUEOPS_FILTER_VLANMACADDR;
      memcpy(filter->u.vlanmac.macaddr, vmkfilter->u.vlanmac.macaddr, 6);
      filter->u.vlanmac.vlan_id = vmkfilter->u.vlanmac.vlan_id;
   }

   return VMK_OK;
}

static inline VMK_ReturnStatus
marshall_to_vmknetq_supported_filter_class(vmknetddi_queueops_filter_class_t class,
                                           vmk_NetqueueFilterClass *vmkclass)
{   
   *vmkclass = VMK_NETQUEUE_FILTER_NONE;
   
   if (class & VMKNETDDI_QUEUEOPS_FILTER_MACADDR) {
      *vmkclass |= VMK_NETQUEUE_FILTER_MACADDR;
   }
   
   if (class & VMKNETDDI_QUEUEOPS_FILTER_VLAN) {
      *vmkclass |= VMK_NETQUEUE_FILTER_VLAN;
   }

   if (class & VMKNETDDI_QUEUEOPS_FILTER_VLANMACADDR) {
      *vmkclass |= VMK_NETQUEUE_FILTER_VLANMACADDR;
   }

   return VMK_OK;
}

static inline VMK_ReturnStatus
marshall_to_vmknetq_filter_id(vmknetddi_queueops_filterid_t fid,
                              vmk_NetqueueFilterID *vmkfid)
{
   return vmk_NetqueueMkFilterID(vmkfid, VMKNETDDI_QUEUEOPS_FILTERID_VAL(fid));
}

static VMK_ReturnStatus
marshall_from_vmknetq_filter_id(vmk_NetqueueFilterID vmkfid,
                                vmknetddi_queueops_filterid_t *fid)
{
   *fid = VMKNETDDI_QUEUEOPS_MK_FILTERID(vmk_NetqueueFilterIDVal(vmkfid));
   return VMK_OK;
}

static VMK_ReturnStatus
marshall_from_vmknetq_pri(vmk_NetqueuePriority vmkpri,
                          vmknetddi_queueops_tx_priority_t *pri)
{
   *pri = (vmknetddi_queueops_tx_priority_t)vmkpri;
   return VMK_OK;
}

static inline VMK_ReturnStatus
marshall_to_vmknetq_queue_features(vmknetddi_queueops_queue_features_t features,
                                   vmk_NetqueueQueueFeatures *vmkfeatures)
{
   if (features & VMKNETDDI_QUEUEOPS_QUEUE_FEAT_LRO) {
      *vmkfeatures |= VMK_NETQUEUE_QUEUE_FEAT_LRO;
   }
   if (features & VMKNETDDI_QUEUEOPS_QUEUE_FEAT_PAIR) {
      *vmkfeatures |= VMK_NETQUEUE_QUEUE_FEAT_PAIR;
   }

   return VMK_OK;
}

static inline VMK_ReturnStatus
marshall_from_vmknetq_queue_features(vmk_NetqueueQueueFeatures vmkfeatures,
                                     vmknetddi_queueops_queue_features_t *features)
{
   if (vmkfeatures & VMK_NETQUEUE_QUEUE_FEAT_LRO) {
      *features |= VMKNETDDI_QUEUEOPS_QUEUE_FEAT_LRO;
   }
   if (vmkfeatures & VMK_NETQUEUE_QUEUE_FEAT_PAIR) {
      *features |= VMKNETDDI_QUEUEOPS_QUEUE_FEAT_PAIR;
   }

   return VMK_OK;
}

static VMK_ReturnStatus
marshall_from_vmknetq_attr(vmk_NetqueueQueueAttr *vmkattr,
                           u16 nattr,
                           vmknetddi_queueops_queueattr_t *attr)
{
   int i;

   for (i = 0; i < nattr; i++) {
      switch (vmkattr[i].type) {         
      case VMK_NETQUEUE_QUEUE_ATTR_PRIOR:
         attr[i].type = VMKNETDDI_QUEUEOPS_QUEUE_ATTR_PRIOR;
         marshall_from_vmknetq_pri(vmkattr[i].args.priority,
                                   &attr[i].args.priority);
         break;

      case VMK_NETQUEUE_QUEUE_ATTR_FEAT:
         attr[i].type = VMKNETDDI_QUEUEOPS_QUEUE_ATTR_FEAT;
         marshall_from_vmknetq_queue_features(vmkattr[i].args.features,
                                   &attr[i].args.features);
         break;
         
      default:
         return VMK_FAILURE;
      }
   }

   return VMK_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 *  netqueue_op_get_version --
 *
 *    Get driver Netqueue version
 *
 *  Results:
 *    VMK_OK on success. VMK_NOT_SUPPORTED, if operation is not supported by
 *    device. VMK_FAILURE, if operation fails.
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
netqueue_op_get_version(void *clientData,
                        void *opArgs)
{
   int result;
   VMK_ReturnStatus ret;
   vmknetddi_queueop_get_version_args_t args;

   vmk_NetqueueOpGetVersionArgs *vmkargs = (vmk_NetqueueOpGetVersionArgs *)opArgs;
   struct net_device *dev = (struct net_device *)clientData;
   VMK_ASSERT(dev);

   if (dev->netqueue_ops) {
      VMKAPI_MODULE_CALL(dev->module_id, result, dev->netqueue_ops,
                 VMKNETDDI_QUEUEOPS_OP_GET_VERSION, &args);
      if (result != 0) {
         ret = VMK_FAILURE;
      } else {
         vmkargs->major = args.major;
         vmkargs->minor = args.minor;
         ret = VMK_OK;
      }
   } else {
      VMKLNX_DEBUG(0, "!dev->netqueue_ops");
      ret = VMK_NOT_SUPPORTED;
   }

   return ret;
}

/*
 *----------------------------------------------------------------------------
 *
 *  netqueue_op_get_features --
 *
 *    Get driver Netqueue features
 *
 *  Results:
 *    VMK_OK on success, VMK_FAILURE on error, VMK_NOT_SUPPORTED if
 *    Netqueue ops are not supprted by the driver
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
netqueue_op_get_features(void *clientData,
                         void *opArgs)
{
   int result;
   VMK_ReturnStatus ret;
   vmknetddi_queueop_get_features_args_t args;

   vmk_NetqueueOpGetFeaturesArgs *vmkargs = (vmk_NetqueueOpGetFeaturesArgs *)opArgs;
   struct net_device *dev = (struct net_device *)clientData;
   VMK_ASSERT(dev);
   args.netdev = dev;
   args.features = VMKNETDDI_QUEUEOPS_FEATURE_NONE;

   if (dev->netqueue_ops) {
      VMKAPI_MODULE_CALL(dev->module_id, result, dev->netqueue_ops,
                 VMKNETDDI_QUEUEOPS_OP_GET_FEATURES, &args);
      if (result != 0) {
         ret = VMK_FAILURE;
      } else {
         ret = marshall_to_vmknetq_features(args.features, &vmkargs->features);
         VMK_ASSERT(ret == VMK_OK);
      }
   } else {
      VMKLNX_DEBUG(0, "!dev->netqueue_ops");
      ret = VMK_NOT_SUPPORTED;
   }

   return ret;
}

/*
 *----------------------------------------------------------------------------
 *
 *  netqueue_op_get_queue_count --
 *
 *    Get count of tx or rx queues supprted by the driver
 *
 *  Results:
 *    VMK_OK on success, VMK_FAILURE on error, VMK_NOT_SUPPORTED if
 *    not supported
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
netqueue_op_get_queue_count(void *clientData,
                            void *opArgs)
{
   int result;
   VMK_ReturnStatus ret;
   vmknetddi_queueop_get_queue_count_args_t args;

   vmk_NetqueueOpGetQueueCountArgs *vmkargs =
                                      (vmk_NetqueueOpGetQueueCountArgs *)opArgs;
   struct net_device *dev = (struct net_device *)clientData;
   VMK_ASSERT(dev);
   args.netdev = dev;

   if (marshall_from_vmknetq_type(vmkargs->qtype, &args.type) != VMK_OK) {
      return VMK_FAILURE;
   }

   if (dev->netqueue_ops) {
      VMKAPI_MODULE_CALL(dev->module_id, result, dev->netqueue_ops,
                         VMKNETDDI_QUEUEOPS_OP_GET_QUEUE_COUNT, &args);
      if (result != 0) {
         ret = VMK_FAILURE;
      } else {
         vmkargs->count = args.count;
         ret = VMK_OK;
      }
   } else {
      VMKLNX_DEBUG(0, "!dev->netqueue_ops");
      ret = VMK_NOT_SUPPORTED;
   }

   return ret;
}

/*
 *----------------------------------------------------------------------------
 *
 *  netqueue_op_get_filter_count --
 *
 *    Get number of rx filters supprted by the driver
 *
 *  Results:
 *    VMK_OK on success, VMK_FAILURE on error, VMK_NOT_SUPPORTED if
 *    not supported
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
netqueue_op_get_filter_count(void *clientData,
                             void *opArgs)
{
   int result;
   VMK_ReturnStatus ret;
   vmknetddi_queueop_get_filter_count_args_t args;

   vmk_NetqueueOpGetFilterCountArgs *vmkargs = (vmk_NetqueueOpGetFilterCountArgs *)opArgs;
   struct net_device *dev = (struct net_device *)clientData;
   VMK_ASSERT(dev);
   args.netdev = dev;

   if (marshall_from_vmknetq_type(vmkargs->qtype, &args.type) != VMK_OK) {
      return VMK_FAILURE;
   }

   if (dev->netqueue_ops) {
      VMKAPI_MODULE_CALL(dev->module_id, result, dev->netqueue_ops,
                 VMKNETDDI_QUEUEOPS_OP_GET_FILTER_COUNT, &args);
      if (result != 0) {
         ret = VMK_FAILURE;
      } else {
         vmkargs->count = args.count;
         ret = VMK_OK;
      }
   } else {
      VMKLNX_DEBUG(0, "!dev->netqueue_ops");
      ret = VMK_NOT_SUPPORTED;
   }

   return ret;
}

/*
 *----------------------------------------------------------------------------
 *
 *  dev_add_netqueue_qid --
 *
 *    Record new netqueue qid
 *
 *  Results:
 *    VMK_ReturnStatus
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
dev_add_netqueue_qid(struct net_device *dev,
                     u16 qidx,
                     vmk_NetqueueQueueID vmkqid)
{
   VMK_ReturnStatus ret = VMK_OK;
   struct tx_netqueue_info *txinfo = dev->tx_netqueue_info;
   VMK_ASSERT(txinfo);

   if (qidx < dev->num_tx_queues) {
      VMK_ASSERT(txinfo[qidx].valid == VMK_FALSE);
      txinfo[qidx].valid = VMK_TRUE;
      txinfo[qidx].vmkqid = vmkqid;
   } else {
      ret = VMK_FAILURE;
   }

   return ret;
}

/*
 *----------------------------------------------------------------------------
 *
 *  dev_remove_netqueue_qid --
 *
 *    Removed recorded netqueue qid
 *
 *  Results:
 *    None.
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static void
dev_remove_netqueue_qid(struct net_device *dev,
                        u32 qidx)
{
   struct tx_netqueue_info *txinfo = dev->tx_netqueue_info;
   VMK_ASSERT(txinfo);

   VMK_ASSERT(txinfo[qidx].valid == VMK_TRUE);
   txinfo[qidx].valid = VMK_FALSE;
   txinfo[qidx].vmkqid = VMK_NETQUEUE_INVALID_QUEUEID;
}

/*
 *----------------------------------------------------------------------------
 *
 *  netqueue_op_alloc_queue --
 *
 *    Call driver netqueue_op for allocating queue
 *
 *  Results:
 *    VMK_OK on success, VMK_FAILURE on error, VMK_NOT_SUPPORTED if
 *    not supported
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
netqueue_op_alloc_queue(void *clientData,
                        void *opArgs)
{
   int result;
   VMK_ReturnStatus ret;
   vmknetddi_queueop_alloc_queue_args_t args;
   vmknetddi_queueop_free_queue_args_t freeargs;
   vmk_NetqueueOpAllocQueueArgs *vmkargs = (vmk_NetqueueOpAllocQueueArgs *)opArgs;
   struct net_device *dev = (struct net_device *)clientData;
   vmk_NetqueueQueueType qtype = vmkargs->qtype;

   VMK_ASSERT(dev);

   args.netdev = dev;
   args.napi = NULL;
   args.queue_mapping = 0;

   if (!(qtype == VMK_NETQUEUE_QUEUE_TYPE_RX) &&
       !(qtype == VMK_NETQUEUE_QUEUE_TYPE_TX)) {
      VMKLNX_DEBUG(0, "invalid vmkqueue type 0x%x", qtype);
      return VMK_FAILURE;
   }

   ret = marshall_from_vmknetq_type(qtype, &args.type);
   VMK_ASSERT(ret == VMK_OK);
   if (ret != VMK_OK) {
      return VMK_FAILURE;
   }

   if (dev->netqueue_ops) {
      VMKAPI_MODULE_CALL(dev->module_id, result, dev->netqueue_ops,
                         VMKNETDDI_QUEUEOPS_OP_ALLOC_QUEUE, &args);
      if (result != 0) {
         ret = VMK_FAILURE;
      } else {
         if (qtype == VMK_NETQUEUE_QUEUE_TYPE_TX) {
            VMK_ASSERT(VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args.queueid) < dev->num_tx_queues);
            if (args.queue_mapping) {
               VMK_ASSERT(args.queue_mapping == 
                          VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args.queueid));
            }
         } else {
            VMK_ASSERT(qtype == VMK_NETQUEUE_QUEUE_TYPE_RX);
            if (args.napi != NULL) {
               vmkargs->net_poll = args.napi->net_poll;
            }
         }

         ret = marshall_to_vmknetq_id(args.queueid, &vmkargs->qid);
         VMK_ASSERT(ret == VMK_OK);
         if (unlikely(ret != VMK_OK)) {
            goto error_free;
         }

         if (qtype == VMK_NETQUEUE_QUEUE_TYPE_TX) {
            u16 qidx = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args.queueid);
            ret = dev_add_netqueue_qid(dev, qidx, vmkargs->qid);
            if (ret != VMK_OK) {
               VMKLNX_DEBUG(0, "%s: failed to add netqueue qidx=%d", dev->name, qidx);
               goto error_free;
            }
         }
      }
   } else {
      VMKLNX_DEBUG(0, "!dev->netqueue_ops");
      ret = VMK_NOT_SUPPORTED;
   }

 out:
   return ret;

 error_free:
   VMK_ASSERT(ret != VMK_OK);

   freeargs.netdev = dev;
   freeargs.queueid = args.queueid;
   VMKAPI_MODULE_CALL(dev->module_id, result, dev->netqueue_ops, 
                      VMKNETDDI_QUEUEOPS_OP_FREE_QUEUE, &freeargs);
   goto out;
}

/*
 *----------------------------------------------------------------------------
 *
 *  netqueue_op_alloc_queue_with_attr --
 *
 *    Call driver netqueue_op for allocating queue with attributes
 *
 *  Results:
 *    VMK_OK on success, VMK_FAILURE on error, VMK_NOT_SUPPORTED if
 *    not supported
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
netqueue_op_alloc_queue_with_attr(void *clientData, 
                                  void *opArgs)
{
   int result;
   VMK_ReturnStatus ret;
   vmknetddi_queueop_alloc_queue_with_attr_args_t args;   
   vmknetddi_queueop_free_queue_args_t freeargs;
   vmk_NetqueueOpAllocQueueArgs vmkallocargs; 
   vmk_NetqueueOpAllocQueueWithAttrArgs *vmkargs = 
      (vmk_NetqueueOpAllocQueueWithAttrArgs *)opArgs;
   struct net_device *dev = (struct net_device *)clientData;
   vmk_NetqueueQueueType qtype = vmkargs->qtype;
   vmknetddi_queueops_queueattr_t attr[VMKNETDDI_QUEUEOPS_QUEUE_ATTR_NUM];

   /* If alloc without attributes, just call normal alloc queue */
   if (vmkargs->nattr == 0) {
      memset(&vmkallocargs, 0, sizeof(vmkallocargs));
      vmkallocargs.net_poll = NULL; 
      vmkallocargs.qtype = qtype;
      vmkallocargs.qid = vmkargs->qid;
      ret = netqueue_op_alloc_queue(clientData, &vmkallocargs);
      if (ret == VMK_OK) {
         vmkargs->net_poll = vmkallocargs.net_poll; 
	 vmkargs->qid = vmkallocargs.qid;
	 return VMK_OK;
      } else {
	  return VMK_FAILURE;
      }
   }

   VMK_ASSERT(dev);
   args.netdev = dev;
   args.napi = NULL;
   args.queue_mapping = 0;
   
   if (vmkargs->nattr > VMKNETDDI_QUEUEOPS_QUEUE_ATTR_NUM) {
      VMK_ASSERT(VMK_FALSE);
      return VMK_LIMIT_EXCEEDED;
   }

   args.nattr = vmkargs->nattr;

   if (!(qtype == VMK_NETQUEUE_QUEUE_TYPE_RX) && 
       !(qtype == VMK_NETQUEUE_QUEUE_TYPE_TX)) {
      VMKLNX_DEBUG(0, "invalid vmkqueue type 0x%x", qtype);
      return VMK_FAILURE;
   }

   ret = marshall_from_vmknetq_type(qtype, &args.type);
   VMK_ASSERT(ret == VMK_OK);
   if (ret != VMK_OK) {
      return VMK_FAILURE;
   }

   memset(attr, 0, sizeof(attr));
   ret = marshall_from_vmknetq_attr(vmkargs->attr, vmkargs->nattr, attr);
   VMK_ASSERT(ret == VMK_OK);
   if (ret != VMK_OK) {
      return VMK_FAILURE;
   }
   args.attr = attr;

   if (dev->netqueue_ops) {
      VMKAPI_MODULE_CALL(dev->module_id, result, dev->netqueue_ops, 
                         VMKNETDDI_QUEUEOPS_OP_ALLOC_QUEUE_WITH_ATTR, &args);
      if (result != 0) {
         ret = VMK_FAILURE;
      } else {
         if (qtype == VMK_NETQUEUE_QUEUE_TYPE_TX) {
            VMK_ASSERT(VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args.queueid) < dev->num_tx_queues);
            if (args.queue_mapping) {
               VMK_ASSERT(args.queue_mapping == 
                          VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args.queueid));
            }
         } else {
            VMK_ASSERT(qtype == VMK_NETQUEUE_QUEUE_TYPE_RX);
            if (args.napi != NULL) {
               vmkargs->net_poll = args.napi->net_poll;
            }
         }

         ret = marshall_to_vmknetq_id(args.queueid, &vmkargs->qid);
         VMK_ASSERT(ret == VMK_OK);
         if (unlikely(ret != VMK_OK)) {
            VMKLNX_DEBUG(0, "invalid qid. freeing allocated queue");
            goto error_free;
         }

         if (qtype == VMK_NETQUEUE_QUEUE_TYPE_TX) {
            u16 qidx = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args.queueid);
            ret = dev_add_netqueue_qid(dev, qidx, vmkargs->qid);
            if (ret != VMK_OK) {
               VMKLNX_DEBUG(0, "%s: failed to add netqueue qidx=%d", dev->name, qidx);
               goto error_free;
            }
         }
      }
   } else {
      VMKLNX_DEBUG(0, "!dev->netqueue_ops");
      ret = VMK_NOT_SUPPORTED;
   }

 out:
   return ret;

 error_free:
   VMK_ASSERT(ret != VMK_OK);

   freeargs.netdev = dev;
   freeargs.queueid = args.queueid;
   VMKAPI_MODULE_CALL(dev->module_id, result, dev->netqueue_ops, 
                      VMKNETDDI_QUEUEOPS_OP_FREE_QUEUE, &freeargs);
   goto out;
}

/*
 *----------------------------------------------------------------------------
 *
 *  netqueue_op_free_queue --
 *
 *    Free queue
 *
 *  Results:
 *    VMK_OK on success, VMK_FAILURE on error, VMK_NOT_SUPPORTED if
 *    not supported
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
netqueue_op_free_queue(void *clientData,
                       void *opArgs)
{
   int result;
   VMK_ReturnStatus ret;
   vmknetddi_queueop_free_queue_args_t args;
   vmk_NetqueueOpFreeQueueArgs *vmkargs = (vmk_NetqueueOpFreeQueueArgs *)opArgs;
   vmk_NetqueueQueueID vmkqid = vmkargs->qid;
   vmk_NetqueueQueueType qtype = vmk_NetqueueQueueIDType(vmkqid);
   struct net_device *dev = (struct net_device *)clientData;

   VMK_ASSERT(dev);
   args.netdev = dev;

   ret = marshall_from_vmknetq_id(vmkqid, &args.queueid);
   VMK_ASSERT(ret == VMK_OK);
   if (ret != VMK_OK) {
      return ret;
   }

   if (qtype == VMK_NETQUEUE_QUEUE_TYPE_TX) {
      dev_remove_netqueue_qid(dev,
                              VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args.queueid));
   }

   if (dev->netqueue_ops) {
      VMKAPI_MODULE_CALL(dev->module_id, result, dev->netqueue_ops,
                 VMKNETDDI_QUEUEOPS_OP_FREE_QUEUE, &args);
      if (result != 0) {
         ret = VMK_FAILURE;
      } else {
         ret = VMK_OK;
      }
   } else {
      VMKLNX_DEBUG(0, "!dev->netqueue_ops");
      ret = VMK_NOT_SUPPORTED;
   }

   return ret;
}

/*
 *----------------------------------------------------------------------------
 *
 *  netqueue_op_get_queue_vector --
 *
 *    Get interrupt vector for the queue
 *
 *  Results:
 *    VMK_OK on success, VMK_FAILURE on error, VMK_NOT_SUPPORTED if
 *    not supported
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
netqueue_op_get_queue_vector(void *clientData,
                             void *opArgs)
{
   int result;
   VMK_ReturnStatus ret;
   vmknetddi_queueop_get_queue_vector_args_t args;

   vmk_NetqueueOpGetQueueVectorArgs *vmkargs = (vmk_NetqueueOpGetQueueVectorArgs *)opArgs;
   struct net_device *dev = (struct net_device *)clientData;
   VMK_ASSERT(dev);
   args.netdev = dev;

   ret = marshall_from_vmknetq_id(vmkargs->qid, &args.queueid);
   VMK_ASSERT(ret == VMK_OK);
   if (ret != VMK_OK) {
      return ret;
   }

   if (dev->netqueue_ops) {
      VMKAPI_MODULE_CALL(dev->module_id, result, dev->netqueue_ops,
                 VMKNETDDI_QUEUEOPS_OP_GET_QUEUE_VECTOR, &args);
      if (result != 0) {
         ret = VMK_FAILURE;
      } else {
         vmkargs->vector = args.vector;
         ret = VMK_OK;
      }
   } else {
      VMKLNX_DEBUG(0, "!dev->netqueue_ops");
      ret = VMK_NOT_SUPPORTED;
   }

   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 *  netqueue_op_get_default_queue --
 *
 *    Get default queue for tx/rx operations
 *
 *  Results:
 *    VMK_OK on success, VMK_FAILURE on error, VMK_NOT_SUPPORTED if
 *    not supported
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
netqueue_op_get_default_queue(void *clientData,
                              void *opArgs)
{
   int result;
   VMK_ReturnStatus ret;
   vmknetddi_queueop_get_default_queue_args_t args;
    vmk_NetqueueOpGetDefaultQueueArgs *vmkargs =
      (vmk_NetqueueOpGetDefaultQueueArgs *)opArgs;
   struct net_device *dev = (struct net_device *)clientData;
   vmk_NetqueueQueueType qtype = vmkargs->qtype;

   VMK_ASSERT(dev);
   args.netdev = dev;
   args.napi = NULL;
   args.queue_mapping  = 0;

   ret = marshall_from_vmknetq_type(qtype, &args.type);
   VMK_ASSERT(ret == VMK_OK);
   if (ret != VMK_OK) {
      return ret;
   }

   if (dev->netqueue_ops) {
      VMKAPI_MODULE_CALL(dev->module_id, result, dev->netqueue_ops,
                 VMKNETDDI_QUEUEOPS_OP_GET_DEFAULT_QUEUE, &args);
      if (result != 0) {
         ret = VMK_FAILURE;
      } else {
         if (qtype == VMK_NETQUEUE_QUEUE_TYPE_TX) {
            VMK_ASSERT(VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args.queueid) < dev->num_tx_queues);
         } else {
            VMK_ASSERT(qtype == VMK_NETQUEUE_QUEUE_TYPE_RX);
            if (args.napi != NULL) {
               vmkargs->net_poll = args.napi->net_poll;
            }
         }

         ret = marshall_to_vmknetq_id(args.queueid, &vmkargs->qid);
         VMK_ASSERT(ret == VMK_OK);
      }
   } else {
      VMKLNX_DEBUG(0, "!dev->netqueue_ops");
      ret = VMK_NOT_SUPPORTED;
   }

   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 *  netqueue_op_apply_rx_filter --
 *
 *    Apply rx filter on queue
 *
 *  Results:
 *    VMK_OK on success, VMK_FAILURE on error, VMK_NOT_SUPPORTED if
 *    not supported
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
netqueue_op_apply_rx_filter(void *clientData,
                            void *opArgs)
{
   int result;
   VMK_ReturnStatus ret;
   vmknetddi_queueop_apply_rx_filter_args_t args;

   vmk_NetqueueOpApplyRxFilterArgs *vmkargs =
      (vmk_NetqueueOpApplyRxFilterArgs *)opArgs;
   struct net_device *dev = (struct net_device *)clientData;
   VMK_ASSERT(dev);
   args.netdev = dev;

   ret = marshall_from_vmknetq_id(vmkargs->qid, &args.queueid);
   VMK_ASSERT(ret == VMK_OK);
   if (ret != VMK_OK) {
      return ret;
   }

   ret = marshall_from_vmknetq_filter_type(&vmkargs->filter, &args.filter);
   VMK_ASSERT(ret == VMK_OK);
   if (ret != VMK_OK) {
      return ret;
   }

   if (dev->netqueue_ops) {
      VMKAPI_MODULE_CALL(dev->module_id, result, dev->netqueue_ops,
                 VMKNETDDI_QUEUEOPS_OP_APPLY_RX_FILTER, &args);
      if (result != 0) {
         VMKLNX_DEBUG(0, "vmknetddi_queueops_apply_rx_filter returned %d", result);
         ret = VMK_FAILURE;
      } else {
         ret = marshall_to_vmknetq_filter_id(args.filterid, &vmkargs->fid);
	 vmkargs->pairhwqid = args.pairtxqid;
         VMK_ASSERT(ret == VMK_OK);
      }
   } else {
      VMKLNX_DEBUG(0, "!dev->netqueue_ops");
      ret = VMK_NOT_SUPPORTED;
   }

   return ret;
}

/*
 *----------------------------------------------------------------------------
 *
 *  netqueue_op_remove_rx_filter --
 *
 *    Remove rx filter from queue
 *
 *  Results:
 *    VMK_OK on success, VMK_FAILURE on error, VMK_NOT_SUPPORTED if
 *    not supported
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
netqueue_op_remove_rx_filter(void *clientData,
                             void *opArgs)
{
   int result;
   VMK_ReturnStatus ret;
   vmknetddi_queueop_remove_rx_filter_args_t args;

   vmk_NetqueueOpRemoveRxFilterArgs *vmkargs =
      (vmk_NetqueueOpRemoveRxFilterArgs *)opArgs;
   struct net_device *dev = (struct net_device *)clientData;
   VMK_ASSERT(dev);
   args.netdev = dev;

   ret = marshall_from_vmknetq_id(vmkargs->qid, &args.queueid);
   VMK_ASSERT(ret == VMK_OK);
   if (ret != VMK_OK) {
      return ret;
   }

   ret = marshall_from_vmknetq_filter_id(vmkargs->fid, &args.filterid);
   VMK_ASSERT(ret == VMK_OK);
   if (ret != VMK_OK) {
      return ret;
   }

   if (dev->netqueue_ops) {
      VMKAPI_MODULE_CALL(dev->module_id, result, dev->netqueue_ops,
                 VMKNETDDI_QUEUEOPS_OP_REMOVE_RX_FILTER, &args);
      if (result != 0) {
         VMKLNX_DEBUG(0, "vmknetddi_queueops_remove_rx_filter returned %d",
                      result);
         ret = VMK_FAILURE;
      } else {
         ret = VMK_OK;
      }
   } else {
      VMKLNX_DEBUG(0, "!dev->netqueue_ops");
      ret = VMK_NOT_SUPPORTED;
   }

   return ret;
}

/*
 *----------------------------------------------------------------------------
 *
 *  netqueue_op_get_queue_stats --
 *
 *    Get queue statistics
 *
 *  Results:
 *    VMK_OK on success, VMK_FAILURE on error, VMK_NOT_SUPPORTED if
 *    not supported
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
netqueue_op_get_queue_stats(void *clientData,
                            void *opArgs)
{
   return VMK_NOT_SUPPORTED;
}

/*
 *----------------------------------------------------------------------------
 *
 *  netqueue_op_set_tx_priority --
 *
 *    Set tx queue priority
 *
 *  Results:
 *    VMK_OK on success, VMK_FAILURE on error, VMK_NOT_SUPPORTED if
 *    not supported
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
netqueue_op_set_tx_priority(void *clientData,
                            void *opArgs)
{
   VMK_ReturnStatus ret;
   vmk_NetqueueOpSetTxPriorityArgs *vmkargs = opArgs;
   vmknetddi_queueop_set_tx_priority_args_t args;
   struct net_device *dev = (struct net_device *)clientData;

   args.netdev = dev;

   ret = marshall_from_vmknetq_id(vmkargs->qid, &args.queueid);
   VMK_ASSERT(ret == VMK_OK);
   if (ret != VMK_OK) {
      return ret;
   }

   ret = marshall_from_vmknetq_pri(vmkargs->priority, &args.priority);
   VMK_ASSERT(ret == VMK_OK);
   if (ret != VMK_OK) {
      return ret;
   }

   if (dev->netqueue_ops) {
      int result;

      VMKAPI_MODULE_CALL(dev->module_id, result, dev->netqueue_ops,
                         VMKNETDDI_QUEUEOPS_OP_SET_TX_PRIORITY, &args);
      if (result != 0) {
         return VMK_FAILURE;
      } else {
         return VMK_OK;
      }
   } else {
      VMKLNX_DEBUG(0, "!dev->netqueue_ops");
      return VMK_NOT_SUPPORTED;
   }
}

/*
 *----------------------------------------------------------------------------
 *
 *  netqueue_op_getset_state --
 *    Get and Set Netqueue Valid State
 *
 *  Results:
 *    The previous netqueue state
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
netqueue_op_getset_state(void *clientData,
                         void *opArgs)
{
   vmk_NetqueueOpGetSetQueueStateArgs *vmkargs =
      (vmk_NetqueueOpGetSetQueueStateArgs *)opArgs;
   struct net_device *dev = (struct net_device *)clientData;
   VMK_ASSERT(dev);

   if (dev->netqueue_ops) {
      vmkargs->oldState = vmknetddi_queueops_getset_state(dev, vmkargs->newState);
      return VMK_OK;
   } else {
      VMKLNX_DEBUG(0, "!dev->netqueue_ops");
      return VMK_NOT_SUPPORTED;
   }
}

/*
 *----------------------------------------------------------------------------
 *
 *  netqueue_op_enable_queue_feat --
 *    Enable queue's features
 *
 *  Results:
 *    VMK_ReturnStatus
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
netqueue_op_enable_queue_feat(void *clientData,
                              void *opArgs)
{
   VMK_ReturnStatus ret;
   vmk_NetqueueOpEnableQueueFeatArgs *vmkargs = opArgs;
   vmknetddi_queueop_enable_feat_args_t args;
   struct net_device *dev = (struct net_device *)clientData;

   args.netdev = dev;
   args.features = 0;

   ret = marshall_from_vmknetq_id(vmkargs->qid, &args.queueid);
   VMK_ASSERT(ret == VMK_OK);
   if (ret != VMK_OK) {
      return ret;
   }
  
   marshall_from_vmknetq_queue_features(vmkargs->features,
                                        &args.features);
   if (dev->netqueue_ops) {
      int result;

      VMKAPI_MODULE_CALL(dev->module_id, result, dev->netqueue_ops,
                         VMKNETDDI_QUEUEOPS_OP_ENABLE_FEAT, &args);
      if (result != 0) {
         return VMK_FAILURE;
      } else {
         return VMK_OK;
      }
   } else {
      VMKLNX_DEBUG(0, "!dev->netqueue_ops");
      return VMK_NOT_SUPPORTED;
   }
}

/*
 *----------------------------------------------------------------------------
 *
 *  netqueue_op_disable_queue_feat --
 *    Disable queue's features
 *
 *  Results:
 *    VMK_ReturnStatus
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
netqueue_op_disable_queue_feat(void *clientData,
                               void *opArgs)
{
   VMK_ReturnStatus ret;
   vmk_NetqueueOpDisableQueueFeatArgs *vmkargs = opArgs;
   vmknetddi_queueop_disable_feat_args_t args;
   struct net_device *dev = (struct net_device *)clientData;

   args.netdev = dev;
   args.features = 0;

   ret = marshall_from_vmknetq_id(vmkargs->qid, &args.queueid);
   VMK_ASSERT(ret == VMK_OK);
   if (ret != VMK_OK) {
      return ret;
   }

   marshall_from_vmknetq_queue_features(vmkargs->features,
                                        &args.features);
   if (dev->netqueue_ops) {
      int result;

      VMKAPI_MODULE_CALL(dev->module_id, result, dev->netqueue_ops,
                         VMKNETDDI_QUEUEOPS_OP_DISABLE_FEAT, &args);
      if (result != 0) {
         return VMK_FAILURE;
      } else {
         return VMK_OK;
      }
   } else {
      VMKLNX_DEBUG(0, "!dev->netqueue_ops");
      return VMK_NOT_SUPPORTED;
   }
}

/*
 *----------------------------------------------------------------------------
 *
 *  netqueue_op_get_queue_supported_feat --
 *    Get supported queues' features
 *
 *  Results:
 *    VMK_ReturnStatus
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
netqueue_op_get_queue_supported_feat(void *clientData,
                                     void *opArgs)
{
   VMK_ReturnStatus ret;
   vmk_NetqueueOpGetQueueSupFeatArgs *vmkargs = opArgs;
   vmknetddi_queueop_get_sup_feat_args_t args;
   struct net_device *dev = (struct net_device *)clientData;

   args.netdev = dev;

   ret = marshall_from_vmknetq_type(vmkargs->qtype, &args.type);
   VMK_ASSERT(ret == VMK_OK);
   if (ret != VMK_OK) {
      return VMK_FAILURE;
   }

   if (dev->netqueue_ops) {
      int result;

      VMKAPI_MODULE_CALL(dev->module_id, result, dev->netqueue_ops,
                         VMKNETDDI_QUEUEOPS_OP_GET_SUPPORTED_FEAT, &args);
      if (result != 0) {
         return VMK_FAILURE;
      } else {
         marshall_to_vmknetq_queue_features(args.features,
                                            &vmkargs->features);
         return VMK_OK;
      }
   } else {
      VMKLNX_DEBUG(0, "!dev->netqueue_ops");
      return VMK_NOT_SUPPORTED;
   }
}

/*
 *----------------------------------------------------------------------------
 *
 *  netqueue_op_get_queue_supported_filter_class --
 *    Get supported queues' filter class
 *
 *  Results:
 *    VMK_ReturnStatus
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
netqueue_op_get_queue_supported_filter_class(void *clientData,
                                             void *opArgs)
{
   VMK_ReturnStatus ret;
   vmk_NetqueueOpGetQueueSupFilterArgs *vmkargs = opArgs;
   vmknetddi_queueop_get_sup_filter_class_args_t args;
   struct net_device *dev = (struct net_device *)clientData;

   args.netdev = dev;

   ret = marshall_from_vmknetq_type(vmkargs->qtype, &args.type);
   VMK_ASSERT(ret == VMK_OK);
   if (ret != VMK_OK) {
      return VMK_FAILURE;
   }

   if (dev->netqueue_ops) {
      int result;

      VMKAPI_MODULE_CALL(dev->module_id, result, dev->netqueue_ops,
                         VMKNETDDI_QUEUEOPS_OP_GET_SUPPORTED_FILTER_CLASS,
                         &args);
      if (result != 0) {
         /* Assume by default only supports for mac address filters */
         vmkargs->class = VMK_NETQUEUE_FILTER_MACADDR;
      } else {
         marshall_to_vmknetq_supported_filter_class(args.class,
                                                    &vmkargs->class);
      }
      return VMK_OK;
   } else {
      VMKLNX_DEBUG(0, "!dev->netqueue_ops");
      return VMK_NOT_SUPPORTED;
   }
}

/*
 *----------------------------------------------------------------------------
 *
 *  LinNet_NetqueueSkbXmit --
 *
 *    Transmit a skb on a pre-allocated Tx queue for a specific device
 *
 *  Results:
 *    VMK_OK on success, VMK_FAILURE|VMK_BUSY on error 
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
VMK_ReturnStatus
LinNet_NetqueueSkbXmit(struct net_device *dev,
                       vmk_NetqueueQueueID vmkqid,
                       struct sk_buff *skb)
{
   VMK_ReturnStatus status = VMK_OK;
   struct netdev_queue *queue;   
   int xmit_status = -1;

   queue = netdev_pick_tx_queue(dev, vmkqid);
   VMK_ASSERT(queue != NULL);
   skb->queue_mapping = queue - dev->_tx;
   
   spin_lock(&queue->_xmit_lock);
   queue->processing_tx = 1;

   if (unlikely(netif_tx_queue_stopped(queue))) {
      status = VMK_BUSY;
      goto done;
   }

   VMKAPI_MODULE_CALL(dev->module_id, xmit_status, 
                      *dev->hard_start_xmit, skb, dev);

   /*
    * Map NETDEV_TX_OK and NETDEV_TX_BUSY to VMK_OK and VMK_BUSY. For others
    * that cannot be mapped directly, we should carry through these return
    * status.
    */
   if (xmit_status == NETDEV_TX_OK) {
      status = VMK_OK;
   } else if (xmit_status == NETDEV_TX_BUSY) {
      status = VMK_BUSY;
   } else if (xmit_status == NETDEV_TX_LOCKED) {
      status = VMK_BUSY;
   } else {
      VMKLNX_WARN("Unknown NETDEV_TX status %d, map to VMK_FAILURE\n",
                  xmit_status);
      status = VMK_FAILURE;
   }

 done:
   queue->processing_tx = 0;
   spin_unlock(&queue->_xmit_lock);
   
   return status;
}

/*
 *----------------------------------------------------------------------------
 *
 *  LinNetNetqueueOpFunc --
 *    Netqueue ops handler for vmklinux
 *
 *  Results:
 *    VMK_OK on success, VMK_FAILURE on error
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
LinNetNetqueueOpFunc(void *clientData,
                     vmk_NetqueueOp op,
                     void *opArgs)
{

   switch (op) {
   case VMK_NETQUEUE_OP_GET_VERSION:
      return netqueue_op_get_version(clientData, opArgs);

   case VMK_NETQUEUE_OP_GET_FEATURES:
      return netqueue_op_get_features(clientData, opArgs);

   case VMK_NETQUEUE_OP_QUEUE_COUNT:
      return netqueue_op_get_queue_count(clientData, opArgs);

   case VMK_NETQUEUE_OP_FILTER_COUNT:
      return netqueue_op_get_filter_count(clientData, opArgs);

   case VMK_NETQUEUE_OP_ALLOC_QUEUE:
      return netqueue_op_alloc_queue(clientData, opArgs);

   case VMK_NETQUEUE_OP_FREE_QUEUE:
      return netqueue_op_free_queue(clientData, opArgs);

   case VMK_NETQUEUE_OP_GET_QUEUE_VECTOR:
      return netqueue_op_get_queue_vector(clientData, opArgs);

   case VMK_NETQUEUE_OP_GET_DEFAULT_QUEUE:
      return netqueue_op_get_default_queue(clientData, opArgs);

   case VMK_NETQUEUE_OP_APPLY_RX_FILTER:
      return netqueue_op_apply_rx_filter(clientData, opArgs);

   case VMK_NETQUEUE_OP_REMOVE_RX_FILTER:
      return netqueue_op_remove_rx_filter(clientData, opArgs);

   case VMK_NETQUEUE_OP_GET_QUEUE_STATS:
      return netqueue_op_get_queue_stats(clientData, opArgs);

   case VMK_NETQUEUE_OP_SET_TX_PRIORITY:
      return netqueue_op_set_tx_priority(clientData, opArgs);

   case VMK_NETQUEUE_OP_GETSET_QUEUE_STATE:
      return netqueue_op_getset_state(clientData, opArgs);

   case VMK_NETQUEUE_OP_ALLOC_QUEUE_WITH_ATTR:
      return netqueue_op_alloc_queue_with_attr(clientData, opArgs);

   case VMK_NETQUEUE_OP_ENABLE_QUEUE_FEAT:
      return netqueue_op_enable_queue_feat(clientData, opArgs);
      
   case VMK_NETQUEUE_OP_DISABLE_QUEUE_FEAT:
      return netqueue_op_disable_queue_feat(clientData, opArgs);

   case VMK_NETQUEUE_OP_GET_QUEUE_SUPPORTED_FEAT:
      return netqueue_op_get_queue_supported_feat(clientData, opArgs);

   case VMK_NETQUEUE_OP_GET_QUEUE_SUPPORTED_FILTER_CLASS:
      return netqueue_op_get_queue_supported_filter_class(clientData, opArgs);

   default:
      return VMK_FAILURE;
   }
}

/*
 *----------------------------------------------------------------------------
 *
 *  LinNet_NetqueueOp --
 *    
 *    Submit a netqueue operation to a specific device
 *
 *  Results:
 *    VMK_OK on success, VMK_FAILURE on error 
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

VMK_ReturnStatus
LinNet_NetqueueOp(struct net_device *dev,
                  vmk_NetqueueOp op,
                  void *opArgs)
{
   return LinNetNetqueueOpFunc((void *) dev, op, opArgs);
}

/*
 *-----------------------------------------------------------------------------
 *
 * LinNetPTOpFunc --
 *
 *      This function dispatches the requested passthru control or
 *      eSwitch operation to the corresponding driver.
 *
 * Results:
 *      VMK_NOT_SUPPORTED if the uplink doesn't support PT/eSwitch or
 *      if the desired operation is not implemented. VMK_OK on
 *      success. Any other error code from the driver on failure.
 *
 * Side effects:
 *      Calls the uplink driver.
 *
 *-----------------------------------------------------------------------------
 */

static VMK_ReturnStatus
LinNetPTOpFunc(void *clientData, vmk_NetPTOP op, void *args)
{
   struct net_device *dev = (struct net_device *)clientData;
   VMK_ReturnStatus status;

   VMK_ASSERT(dev);

   if (!dev->pt_ops) {
      return VMK_NOT_SUPPORTED;
   }

   if (op == VMK_NETPTOP_IS_SUPPORTED) {
      return VMK_OK;
   }

   /*
    * If _attempting_ to get a VF, let's increment the refCount.
    */
   if (op == VMK_NETPTOP_VF_ACQUIRE) {
      vmk_ModuleIncUseCount(dev->module_id);
   }
   rtnl_lock();
   VMKAPI_MODULE_CALL(dev->module_id,
                      status,
                      (vmk_UplinkPTOpFunc) dev->pt_ops,
                      dev,
                      op,
                      args);
   rtnl_unlock();
   /*
    * If we succeeded to acquire a VF, then don't do anything. If we
    * failed, let's decrement the refCount. If we successfully
    * released a VF, decrement the refCount.
    */
   if ((op == VMK_NETPTOP_VF_ACQUIRE && status != VMK_OK) ||
       (op == VMK_NETPTOP_VF_RELEASE && status == VMK_OK)) {
      vmk_ModuleDecUseCount(dev->module_id);
   }

   return status;
}

/*
 *----------------------------------------------------------------------------
 *
 * GetMACAddr --
 *
 *    Return the MAC address of the NIC.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
GetMACAddr(void *clientData, vmk_uint8 *macAddr)
{
   struct net_device *dev = (struct net_device *)clientData;

   memcpy(macAddr, dev->dev_addr, 6);

   return VMK_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 * GetDeviceName --
 *
 *    Return the system name of corresponding device
 *
 * Results:
 *    None
 *
 * Side effects:
 *    When the dev->pdev is NULL, we return the dev->name (pseudo device name)
 *    instead
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
GetDeviceName(void *device,
              char *devName,
              vmk_ByteCount devNameLen)
{
   VMK_ReturnStatus status;
   struct net_device *dev = device;

   /* Check if the associated pdev is NULL (a pseudo device) */
   if (dev->pdev) {
      status = vmk_StringCopy(devName, dev->pdev->name, devNameLen);
   } else {
      status = vmk_StringCopy(devName, dev->name, devNameLen);
   }

   return status;
}


/*
 *----------------------------------------------------------------------------
 *
 * GetDeviceStats --
 *
 *    Return the stats of corresponding device.
 *
 *    There are two kinds of statistics :
 *
 *    - General statistics : retrieved by struct net_device_stats enclosed in
 *                           the struct net_device.
 *                           These stats are common to all device and stored in
 *                           stats array.
 *
 *    - Specific statistics : retrieved by ethtool functions provided by driver.
 *                            A global string is created in gtrings containing all
 *                            formatted statistics.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
GetDeviceStats(void *device, vmk_PortClientStats *stats)
{
   struct net_device *dev = device;
   struct net_device_stats *st = NULL;
   struct ethtool_ops *ops = dev->ethtool_ops;
   struct ethtool_stats stat;
   u64 *data;
   char *buf;
   char *pbuf;
   int idx = 0;
   int pidx = 0;

   if (dev->get_stats) {
      VMKAPI_MODULE_CALL(dev->module_id, st, dev->get_stats, dev);
   }

   if (!st) {
      return VMK_FAILURE;
   } else {
      VMK_ASSERT_ON_COMPILE(sizeof stats->rxPkt == sizeof st->rx_packets);   
      stats->rxPkt = st->rx_packets;
      stats->txPkt = st->tx_packets;
      stats->rxBytes = st->rx_bytes;
      stats->txBytes = st->tx_bytes;
      stats->rxErr = st->rx_errors;
      stats->txErr = st->tx_errors;
      stats->rxDrp = st->rx_dropped;
      stats->txDrp = st->tx_dropped;
      stats->mltCast = st->multicast;
      stats->col = st->collisions;
      stats->rxLgtErr = st->rx_length_errors;
      stats->rxOvErr = st->rx_over_errors;
      stats->rxCrcErr = st->rx_crc_errors;
      stats->rxFrmErr = st->rx_frame_errors;
      stats->rxFifoErr = st->rx_fifo_errors;
      stats->rxMissErr = st->rx_missed_errors;
      stats->txAbortErr = st->tx_aborted_errors;
      stats->txCarErr = st->tx_carrier_errors;
      stats->txFifoErr = st->tx_fifo_errors;
      stats->txHeartErr = st->tx_heartbeat_errors;
      stats->txWinErr = st->tx_window_errors;
      stats->intRxPkt = dev->linnet_rx_packets;
      stats->intTxPkt = dev->linnet_tx_packets;
      stats->intRxDrp = dev->linnet_rx_dropped;
      stats->intTxDrp = dev->linnet_tx_dropped;
   }

   if (!ops ||
       !ops->get_ethtool_stats ||
       (!ops->get_stats_count && !ops->get_sset_count) ||
       !ops->get_strings) {
      goto done;
   }

   rtnl_lock();
   if (ops->get_stats_count) {
      /* 2.6.18 network drivers method to retrieve the number of stats */
      VMKAPI_MODULE_CALL(dev->module_id, stat.n_stats, ops->get_stats_count, dev);
   } else {
      /* 2.6.18+ network drivers method to retrieve the number of stats */
      VMKAPI_MODULE_CALL(dev->module_id, stat.n_stats, ops->get_sset_count, dev, ETH_SS_STATS);
   }
   rtnl_unlock();
   
   data = kmalloc(stat.n_stats * sizeof(u64), GFP_ATOMIC);
   pbuf = buf = kmalloc(stat.n_stats * ETH_GSTRING_LEN, GFP_ATOMIC);

   if (!data) {
      goto done;
   }

   if (!buf) {
      kfree(data);
      goto done;
   }

   rtnl_lock();
   VMKAPI_MODULE_CALL_VOID(dev->module_id, ops->get_ethtool_stats, dev, &stat, data);
   VMKAPI_MODULE_CALL_VOID(dev->module_id, ops->get_strings, dev, ETH_SS_STATS, (vmk_uint8 *)buf);
   rtnl_unlock();

   stats->privateStats[pidx++] = '\n';
   for (; (pidx < sizeof stats->privateStats - 1) && (idx < stat.n_stats); idx++) {
      char tmp[128];

      snprintf(tmp, 128, "   %s : %lld\n", pbuf, data[idx]);
      memcpy(stats->privateStats + pidx, tmp,
             min(strlen(tmp), sizeof stats->privateStats - pidx - 1));
	
      pidx += min(strlen(tmp), sizeof stats->privateStats - pidx - 1);
      pbuf += ETH_GSTRING_LEN;
   }

   stats->privateStats[pidx] = '\0';

   kfree(data);
   kfree(buf);

 done:

   return VMK_OK;
}


/*
 *----------------------------------------------------------------------------
 *
 * GetDriverInfo --
 *
 *    Return informations of the corresponding device's driver.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
GetDriverInfo(void *device, vmk_UplinkDriverInfo *driverInfo)
{
   struct net_device *dev = device;
   struct ethtool_ops *ops = dev->ethtool_ops;
   struct ethtool_drvinfo drv;
   VMK_ReturnStatus status;

   snprintf(driverInfo->moduleInterface,
            sizeof driverInfo->moduleInterface,  "vmklinux");

   if (!ops || !ops->get_drvinfo) {
      snprintf(driverInfo->driver,
               sizeof driverInfo->driver, "(none)");
      snprintf(driverInfo->version,
               sizeof driverInfo->version, "(none)");
      snprintf(driverInfo->firmwareVersion,
               sizeof driverInfo->firmwareVersion, "(none)");
      status = VMK_FAILURE;
   } else {
      memset(&drv, 0, sizeof(struct ethtool_drvinfo));

      rtnl_lock();
      VMKAPI_MODULE_CALL_VOID(dev->module_id, ops->get_drvinfo, dev, &drv);
      rtnl_unlock();

      memset(driverInfo->driver, 0, sizeof driverInfo->driver);
      memset(driverInfo->version, 0, sizeof driverInfo->version);
      memset(driverInfo->firmwareVersion, 0, sizeof driverInfo->firmwareVersion);

      memcpy(driverInfo->driver, drv.driver,
             min((size_t)(sizeof driverInfo->driver - 1), sizeof drv.driver));
      memcpy(driverInfo->version, drv.version,
	     min((size_t)(sizeof driverInfo->version - 1), sizeof drv.version));
      memcpy(driverInfo->firmwareVersion, drv.fw_version,
	     min((size_t)(sizeof driverInfo->firmwareVersion - 1), sizeof(drv.fw_version)));

      status = VMK_OK;
   }

   return status;
}

/*
 *----------------------------------------------------------------------------
 *
 * wolLinuxCapsToVmkCaps --
 *
 *      translate from VMK wol caps to linux caps
 *
 * Results:
 *      vmk_wolCaps
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */
static vmk_UplinkWolCaps
wolLinuxCapsToVmkCaps(vmk_uint32 caps)
{
   vmk_UplinkWolCaps vmkCaps = 0;

   if (caps & WAKE_PHY) {
      vmkCaps |= VMK_UPLINK_WAKE_ON_PHY;
   }
   if (caps & WAKE_UCAST) {
      vmkCaps |= VMK_UPLINK_WAKE_ON_UCAST;
   }
   if (caps & WAKE_MCAST) {
      vmkCaps |= VMK_UPLINK_WAKE_ON_MCAST;
   }
   if (caps & WAKE_BCAST) {
      vmkCaps |= VMK_UPLINK_WAKE_ON_BCAST;
   }
   if (caps & WAKE_ARP) {
      vmkCaps |= VMK_UPLINK_WAKE_ON_ARP;
   }
   if (caps & WAKE_MAGIC) {
      vmkCaps |= VMK_UPLINK_WAKE_ON_MAGIC;
   }
   if (caps & WAKE_MAGICSECURE) {
      vmkCaps |= VMK_UPLINK_WAKE_ON_MAGICSECURE;
   }

   return vmkCaps;
}

/*
 *----------------------------------------------------------------------------
 *
 * GetWolState --
 *
 *      use the ethtool interface to populate a vmk_UplinkWolState
 *
 * Results:
 *      vmk_UplinkWolState
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
GetWolState(void *device, vmk_UplinkWolState *wolState)
{
   struct net_device *dev = device;
   struct ethtool_ops *ops = dev->ethtool_ops;

   if (!ops || !ops->get_wol) {
      return VMK_NOT_SUPPORTED;
   } else {
      struct ethtool_wolinfo wolInfo[1];
      VMK_ReturnStatus status = VMK_OK;

      memset(wolInfo, 0, sizeof(wolInfo));
      rtnl_lock();
      VMKAPI_MODULE_CALL_VOID(dev->module_id, ops->get_wol, dev, wolInfo);
      rtnl_unlock();

      wolState->supported = wolLinuxCapsToVmkCaps(wolInfo->supported);
      wolState->enabled = wolLinuxCapsToVmkCaps(wolInfo->wolopts);

      if (strlen((char *)wolInfo->sopass) > 0) {
         vmk_uint32 length = strlen((char *)wolInfo->sopass);

         memset(wolState->secureONPassword, 0,
                sizeof wolState->secureONPassword);

         length++;
         if (length > sizeof wolState->secureONPassword) {
            status = VMK_LIMIT_EXCEEDED; // truncated
            length = sizeof wolState->secureONPassword;
         }
         memcpy(wolState->secureONPassword, wolInfo->sopass, length);
      }
      return status;
   }
}

/*
 *----------------------------------------------------------------------------
 *
 * GetCoalesceParams --
 *
 *      use the ethtool interface to get device coalescing properties
 *
 * Results:
 *      VMK_ReturnStatus
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
GetCoalesceParams(void *device, 
                  vmk_UplinkCoalesceParams *coalesceParams)
{
   struct net_device *dev = device;
   struct ethtool_ops *ops = dev->ethtool_ops;
   struct ethtool_coalesce coalesce;
   VMK_ReturnStatus status;

   if (!ops || !ops->get_coalesce) {
      status = VMK_NOT_SUPPORTED;
   } else {
      int ret = -1;
      memset(&coalesce, 0, sizeof(struct ethtool_coalesce));

      rtnl_lock();
      coalesce.cmd = ETHTOOL_GCOALESCE;
      VMKAPI_MODULE_CALL(dev->module_id,
                         ret,
                         ops->get_coalesce, 
                         dev, 
                         &coalesce);
      rtnl_unlock();

      if (ret == 0) {
         if (coalesce.rx_coalesce_usecs) {
            coalesceParams->rxUsecs = coalesce.rx_coalesce_usecs;
         }

         if (coalesce.rx_max_coalesced_frames) {
            coalesceParams->rxMaxFrames = coalesce.rx_max_coalesced_frames;
         }

         if (coalesce.tx_coalesce_usecs) {
            coalesceParams->txUsecs = coalesce.tx_coalesce_usecs;
         }

         if (coalesce.tx_max_coalesced_frames) {
            coalesceParams->txMaxFrames = coalesce.tx_max_coalesced_frames;
         }

         status = VMK_OK;
      } else {
         status = VMK_FAILURE;
      }
   }

   return status;
}


/*
 *----------------------------------------------------------------------------
 *
 * SetCoalesceParams --
 *
 *      use the ethtool interface to set device coalescing properties
 *
 * Results:
 *      VMK_ReturnStatus
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
SetCoalesceParams(void *device, 
                  vmk_UplinkCoalesceParams *coalesceParams)
{
   struct net_device *dev = device;
   struct ethtool_ops *ops = dev->ethtool_ops;
   struct ethtool_coalesce coalesce;
   VMK_ReturnStatus status;

   if (!ops || !ops->set_coalesce) {
      status = VMK_NOT_SUPPORTED;
   } else {
      int ret = -1;
      memset(&coalesce, 0, sizeof(struct ethtool_coalesce));

      // get first, then set
      rtnl_lock();
      coalesce.cmd = ETHTOOL_GCOALESCE;
      VMKAPI_MODULE_CALL(dev->module_id,
                         ret,
                         ops->get_coalesce, 
                         dev, 
                         &coalesce);

      if (ret == 0) {
         if (coalesceParams->rxUsecs) {
            coalesce.rx_coalesce_usecs = coalesceParams->rxUsecs;
         }

         if (coalesceParams->rxMaxFrames) {
            coalesce.rx_max_coalesced_frames = coalesceParams->rxMaxFrames;
         }

         if (coalesceParams->txUsecs) {
            coalesce.tx_coalesce_usecs = coalesceParams->txUsecs;
         }

         if (coalesceParams->txMaxFrames) {
            coalesce.tx_max_coalesced_frames = coalesceParams->txMaxFrames;
         }

         coalesce.cmd = ETHTOOL_SCOALESCE;
         VMKAPI_MODULE_CALL(dev->module_id, 
                            ret, 
                            ops->set_coalesce, 
                            dev, 
                            &coalesce);
      }
      rtnl_unlock();

      if (ret == 0) {
         status = VMK_OK;
      } else {
         status = VMK_FAILURE;
      }
   }

   return status;
}


/*
 *----------------------------------------------------------------------------
 *
 * wolVmkCapsToLinuxCaps --
 *
 *      translate from VMK wol caps to linux caps
 *
 * Results:
 *      linux wol cap bits
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */
static vmk_uint32
wolVmkCapsToLinuxCaps(vmk_UplinkWolCaps vmkCaps)
{
   vmk_uint32 caps = 0;

   if (vmkCaps & VMK_UPLINK_WAKE_ON_PHY) {
      caps |= WAKE_PHY;
   }
   if (vmkCaps & VMK_UPLINK_WAKE_ON_UCAST) {
      caps |= WAKE_UCAST;
   }
   if (vmkCaps & VMK_UPLINK_WAKE_ON_MCAST) {
      caps |= WAKE_MCAST;
   }
   if (vmkCaps & VMK_UPLINK_WAKE_ON_BCAST) {
      caps |= WAKE_BCAST;
   }
   if (vmkCaps & VMK_UPLINK_WAKE_ON_ARP) {
      caps |= WAKE_ARP;
   }
   if (vmkCaps & VMK_UPLINK_WAKE_ON_MAGIC) {
      caps |= WAKE_MAGIC;
   }
   if (vmkCaps & VMK_UPLINK_WAKE_ON_MAGICSECURE) {
      caps |= WAKE_MAGICSECURE;
   }

   return caps;
}

/*
 *----------------------------------------------------------------------------
 *
 * SetWolState --
 *
 *      set wol state via ethtool from a vmk_UplinkWolState struct
 *
 * Results;
 *      VMK_OK, various other failues
 *
 * Side effects:
 *      can set state within the pNic
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
SetWolState(void *device, vmk_UplinkWolState *wolState)
{
   struct net_device *dev = device;
   struct ethtool_ops *ops = dev->ethtool_ops;
   VMK_ReturnStatus status = VMK_FAILURE;

   if (!ops || !ops->set_wol) {
      return VMK_NOT_SUPPORTED;
   } else {
      vmk_uint32 length;
      struct ethtool_wolinfo wolInfo[1];
      int error;

      wolInfo->supported = wolVmkCapsToLinuxCaps(wolState->supported);
      wolInfo->wolopts = wolVmkCapsToLinuxCaps(wolState->enabled);

      length = strlen(wolState->secureONPassword);
      if (length > 0) {
         if (length > sizeof(wolInfo->sopass)) {
            length = sizeof(wolInfo->sopass);
         }
         memcpy(wolInfo->sopass, wolState->secureONPassword, length);
      }
      rtnl_lock();
      VMKAPI_MODULE_CALL(dev->module_id, error, ops->set_wol, dev, wolInfo);
      rtnl_unlock();
      if (error == 0) {
         status = VMK_OK;
      }
   }

   return status;
}

/*
 *----------------------------------------------------------------------------
 *
 *  GetNICState --
 *    For the given NIC, return device resource information such as its
 *    irq, memory range, flags and so on.
 *
 *  Results:
 *    VMK_OK if successful. Other VMK_ReturnStatus codes returned on failure.
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
GetNICState(void *clientData, vmk_PortClientStates *states)
{
   if (clientData && states) {
      struct net_device *dev = (struct net_device *)clientData;

      if (test_bit(__LINK_STATE_PRESENT, &dev->state)) {
	 *states |= VMK_PORT_CLIENT_STATE_PRESENT;
      }

      if (!test_bit(__LINK_STATE_XOFF, &dev->state)) {
	 *states |= VMK_PORT_CLIENT_STATE_QUEUE_OK;
      }

      if (!test_bit(__LINK_STATE_NOCARRIER, &dev->state)) {
	 *states |= VMK_PORT_CLIENT_STATE_LINK_OK;
      }

      if (test_bit(__LINK_STATE_START, &dev->state)) {
	 *states |= VMK_PORT_CLIENT_STATE_RUNNING;
      }

      if (dev->flags & IFF_UP) {
	 *states |= VMK_PORT_CLIENT_STATE_READY;
      }

      if (dev->flags & IFF_PROMISC) {
	 *states |= VMK_PORT_CLIENT_STATE_PROMISC;
      }

      if (dev->flags & IFF_BROADCAST) {
	 *states |= VMK_PORT_CLIENT_STATE_BROADCAST;
      }

      if (dev->flags & IFF_MULTICAST) {
	 *states |= VMK_PORT_CLIENT_STATE_MULTICAST;
      }

      return VMK_OK;
   } else {
      VMKLNX_DEBUG(0, "clientData: %p, states %p", clientData, states);
      return VMK_FAILURE;
   }
}

static VMK_ReturnStatus
GetNICMemResources(void *clientData, vmk_UplinkMemResources *resources)
{
   if (clientData && resources) {
      struct net_device *dev = (struct net_device *) clientData;

      resources->baseAddr = (void *)dev->base_addr;
      resources->memStart = (void *)dev->mem_start;
      resources->memEnd = (void *)dev->mem_end;
      resources->dma = dev->dma;

      return VMK_OK;
   } else {
      VMKLNX_DEBUG(0, "clientData: %p, resources %p", clientData, resources);
      return VMK_FAILURE;
   }
}

static VMK_ReturnStatus
GetNICDeviceProperties(void *clientData, vmk_UplinkDeviceInfo *devInfo)
{
   VMK_ReturnStatus status;
   struct net_device *dev;
   struct pci_dev *pdev;
   vmk_PCIDevice vmkPciDev;

   if (clientData == NULL || devInfo == NULL) {
      VMKLNX_DEBUG(0, "clientData: %p, pciInfo %p", clientData, devInfo);
      return VMK_FAILURE;
   }

   dev = (struct net_device *)clientData;
   pdev = dev->pdev;

   if (dev->features & NETIF_F_PSEUDO_REG) {
      // If physical device but registered as a pseudo-device,
      // get the actual pdev from dev->pdev_pseudo (saved by the
      // NIC driver).
      VMK_ASSERT(pdev == NULL);
      pdev = (struct pci_dev *)dev->pdev_pseudo;
      VMKLNX_WARN("PCI device registered as pseudo-device %u:%u:%u.%u",
                  pci_domain_nr(pdev->bus), pdev->bus->number,
                  PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));
   }
   else if (pdev == NULL) {
      /*
       * Pseudo NICs don't have PCI properties
       */
      status = VMK_NOT_SUPPORTED;
      goto out;
   }
   
   /*
    * Get the device info and the DMA constraints for the device
    */
   status = vmk_PCIGetPCIDevice(pci_domain_nr(pdev->bus), pdev->bus->number,
                                PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn),
                                &vmkPciDev);
   if (status != VMK_OK) {
      VMK_ASSERT(status == VMK_OK);
      VMKLNX_WARN("Unable to find vmk_PCIDevice for PCI device %u:%u:%u.%u %s",
                  pci_domain_nr(pdev->bus), pdev->bus->number,
                  PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn),
                  vmk_StatusToString(status));
      status = VMK_FAILURE;
      goto out;
   }

   status = vmk_PCIGetGenDevice(vmkPciDev, &devInfo->device);
   if (status != VMK_OK) {
      VMK_ASSERT(status == VMK_OK);
      VMKLNX_WARN("Unable to get vmk_Device for PCI device %u:%u:%u.%u: %s",
                  pci_domain_nr(pdev->bus), pdev->bus->number,
                  PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn),
                  vmk_StatusToString(status));
      status = VMK_FAILURE;
      goto out;
   }
   
   // If it is a physical device being registered as a pseudo-device,
   // return here prior to other setup.
   if (dev->features & NETIF_F_PSEUDO_REG) {
      return VMK_OK;
   }

   /* Most constraints don't apply so set them to zero. */
   memset(&devInfo->constraints, 0, sizeof(devInfo->constraints));
   devInfo->constraints.addressMask = pdev->dma_mask;
   devInfo->constraints.sgMaxEntries = MAX_SKB_FRAGS + 1;

   return VMK_OK;

out:
   return status;
}


/*
 *----------------------------------------------------------------------------
 *
 *  GetNICPanicInfo --
 *    Fill in vmk_UplinkPanicInfo struct.
 *
 *  Results:
 *    VMK_OK if properties filled in. VMK_FAILURE otherwise.
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
GetNICPanicInfo(void *clientData,
                vmk_UplinkPanicInfo *intInfo)
{
   if (clientData && intInfo) {
      struct net_device* dev = (struct net_device*)clientData;

      if (dev->pdev == NULL) {
         /*
          * Pseudo NIC does not support remote
          * debugging.
          */
         intInfo->vector = 0;
         intInfo->clientData = NULL;
      } else {
         intInfo->vector = dev->pdev->irq;
         intInfo->clientData = dev;
      }

      return VMK_OK;
   } else {
      VMKLNX_DEBUG(0, "clientData: %p, intInfo %p", clientData, intInfo);
      return VMK_FAILURE;
   }
}

/*
 *----------------------------------------------------------------------------
 *
 * FlushRxBuffers --
 *
 *    Called by the net debugger
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
FlushRxBuffers(void* clientData)
{
   struct net_device* dev = (struct net_device*)clientData;
   struct napi_struct* napi = NULL;
   vmk_NetPoll pollPriv;

   VMKLNX_DEBUG(1, "client data, now net_device:%p", dev);

   list_for_each_entry(napi, &dev->napi_list, dev_list) {
      if (napi != NULL) {
         VMKLNX_DEBUG(1, "Calling Pkt List Rx Process on napi:%p", napi);
         VMK_ASSERT(napi->dev != NULL);

         /*
          * Bypass the vswitch to receive the packets when the system is in the
          * panic/debug mode.
          */
         if (vmk_NetPollGetCurrent(&pollPriv) != VMK_OK) { 
            if (debugPktList == NULL) {
               debugPktList = (vmk_PktList) vmk_HeapAlloc(vmklnxLowHeap,
                                                          vmk_PktListSizeInBytes);
               if (debugPktList == NULL) {
                  return VMK_NO_MEMORY;
               }
               vmk_PktListInit(debugPktList);
            }
            return vmk_NetDebugRxProcess(debugPktList);
         } else {
            vmk_NetPollProcessRx(napi->net_poll);
         }
      }
   }

   return VMK_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 *  PanicPoll --
 *    Poll for rx packets.
 *
 *  Results:
 *    result of napi->poll: the number of packets received and processed.
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
PanicPoll(void* clientData,
          vmk_uint32 budget,
          vmk_int32* workDone)
{
   struct net_device* dev = (struct net_device*)clientData;
   struct napi_struct* napi = NULL;
   vmk_int32 ret = 0;
   vmk_int32 modRet = 0;

   VMKLNX_DEBUG(1, "data:%p budget:%u", dev, budget);
   VMK_ASSERT(dev != NULL);

   if (dev->poll_controller) {
      // device supports NET_POLL interface
      VMKAPI_MODULE_CALL_VOID(dev->module_id, dev->poll_controller, dev);
      VMKLNX_DEBUG(1, "%s: poll_controller called\n", dev->name);
   } else {
      list_for_each_entry(napi, &dev->napi_list, dev_list) {
         if ((napi != NULL) && (napi->poll != NULL)) {
            set_bit(NAPI_STATE_SCHED, &napi->state);
            VMKAPI_MODULE_CALL(napi->dev->module_id, modRet, napi->poll, napi,
                               budget);
            ret += modRet;
            VMKLNX_DEBUG(1, "poll:%p napi:%p budget:%u poll returned:%d",
                         napi->poll, napi, budget, ret);
         }
      }
      if (workDone != NULL) {
         *workDone = ret;
      }
   }
   return VMK_OK;
}


static VMK_ReturnStatus
GetWatchdogTimeoHitCnt(void *device, vmk_int16 *hitcnt)
{
   struct net_device *dev = device;

   *hitcnt = dev->watchdog_timeohit_cfg;

   return VMK_OK;
}

static VMK_ReturnStatus
SetWatchdogTimeoHitCnt(void *device, vmk_int16 hitcnt)
{
   struct net_device *dev = device;

   dev->watchdog_timeohit_cfg = hitcnt;

   return VMK_OK;
}

static VMK_ReturnStatus
GetWatchdogTimeoStats(void *device, vmk_int16 *stats)
{
   struct net_device *dev = device;

   *stats = dev->watchdog_timeohit_stats;

   return VMK_OK;
}

static VMK_ReturnStatus
GetWatchdogTimeoPanicMod(void *device, vmk_UplinkWatchdogPanicModState *state)
{
   struct net_device *dev = device;

   *state = dev->watchdog_timeohit_panic;

   return VMK_OK;
}

static VMK_ReturnStatus
SetWatchdogTimeoPanicMod(void *device, vmk_UplinkWatchdogPanicModState state)
{
   struct net_device *dev = device;

   dev->watchdog_timeohit_panic = state;

   return VMK_OK;
}


#define NET_DEVICE_MAKE_PROPERTIES_FUNCTIONS     \
{                                                \
   getStates:          GetNICState,              \
   getMemResources:    GetNICMemResources,       \
   getDeviceProperties:GetNICDeviceProperties,   \
   getPanicInfo:       GetNICPanicInfo,          \
   getMACAddr:         GetMACAddr,               \
   getName:            GetDeviceName,            \
   getStats:           GetDeviceStats,           \
   getDriverInfo:      GetDriverInfo,            \
   getWolState:        GetWolState,              \
   setWolState:        SetWolState,              \
   getCoalesceParams:  GetCoalesceParams,        \
   setCoalesceParams:  SetCoalesceParams,        \
}

#define NET_DEVICE_MAKE_WATCHDOG_FUNCTIONS       \
{                                                \
   getHitCnt:          GetWatchdogTimeoHitCnt,   \
   setHitCnt:          SetWatchdogTimeoHitCnt,   \
   getStats:           GetWatchdogTimeoStats,    \
   getPanicMod:        GetWatchdogTimeoPanicMod, \
   setPanicMod:        SetWatchdogTimeoPanicMod  \
}

#define NET_DEVICE_MAKE_NETQUEUE_FUNCTIONS       \
{                                                \
   netqOpFunc:         LinNetNetqueueOpFunc,     \
   netqXmit:           NULL,                     \
}

#define NET_DEVICE_MAKE_PT_FUNCTIONS             \
{                                                \
   ptOpFunc:           LinNetPTOpFunc            \
}

#define NET_DEVICE_MAKE_VLAN_FUNCTIONS             \
{                                                  \
   setupVlan:         SetupVlanGroupDevice,        \
   removeVlan:        LinNet_RemoveVlanGroupDevice \
}

#define NET_DEVICE_MAKE_MTU_FUNCTIONS            \
{                                                \
   getMTU:            NICGetMTU,                 \
   setMTU:            NICSetMTU                  \
}

#define NET_DEVICE_MAKE_CORE_FUNCTIONS           \
{                                                \
   startTxImmediate:  DevStartTxImmediate,       \
   open:              OpenNetDev,                \
   close:             CloseNetDev,               \
   panicPoll:         PanicPoll,                 \
   flushRxBuffers:    FlushRxBuffers,            \
   ioctl:             IoctlNetDev,               \
   block:             BlockNetDev,               \
   unblock:           UnblockNetDev,             \
   setLinkStatus:     NICSetLinkStatus,          \
   reset:             NICResetDev                \
}

#define NET_DEVICE_MAKE_DCB_FUNCTIONS            \
{                                                \
   isDCBEnabled:      NICDCBIsEnabled,           \
   enableDCB:         NICDCBEnable,              \
   disableDCB:        NICDCBDisable,             \
   getNumTCs:         NICDCBGetNumTCs,           \
   getPG:             NICDCBGetPriorityGroup,    \
   setPG:             NICDCBSetPriorityGroup,    \
   getPFCCfg:         NICDCBGetPFCCfg,           \
   setPFCCfg:         NICDCBSetPFCCfg,           \
   isPFCEnabled:      NICDCBIsPFCEnabled,        \
   enablePFC:         NICDCBEnablePFC,           \
   disablePFC:        NICDCBDisablePFC,          \
   getApps:           NICDCBGetApplications,     \
   setApp:            NICDCBSetApplication,      \
   getCaps:           NICDCBGetCapabilities,     \
   applySettings:     NICDCBApplySettings,       \
   getSettings:       NICDCBGetSettings          \
}

vmk_UplinkFunctions linNetFunctions = {
   coreFns:           NET_DEVICE_MAKE_CORE_FUNCTIONS,
   mtuFns:            NET_DEVICE_MAKE_MTU_FUNCTIONS,
   vlanFns:           NET_DEVICE_MAKE_VLAN_FUNCTIONS,
   propFns:           NET_DEVICE_MAKE_PROPERTIES_FUNCTIONS,
   watchdogFns:       NET_DEVICE_MAKE_WATCHDOG_FUNCTIONS,
   netqueueFns:       NET_DEVICE_MAKE_NETQUEUE_FUNCTIONS,
   ptFns:             NET_DEVICE_MAKE_PT_FUNCTIONS,
   dcbFns:            NET_DEVICE_MAKE_DCB_FUNCTIONS,
};

static VMK_ReturnStatus
NicCharOpsIoctl(vmk_CharDevFdAttr *attr,
               unsigned int cmd,
               vmk_uintptr_t userData,
               vmk_IoctlCallerSize callerSize,
               vmk_int32 *result)
{
   struct net_device *dev;
   vmkplxr_ChardevHandles *handles;
   struct ifreq ifr;
   VMK_ReturnStatus status;

   if (copy_from_user(&ifr, (void *)userData, sizeof(ifr))) {
      return VMK_INVALID_ADDRESS;
   }

   handles = (vmkplxr_ChardevHandles *) attr->clientDeviceData.ptr;
   VMK_ASSERT(handles != NULL);

   dev = handles->vmklinuxInfo.ptr;
   VMK_ASSERT(dev != NULL);

   status = netdev_ioctl(dev, cmd, &ifr, (uint32_t *) result, callerSize, VMK_FALSE);
   if (status == VMK_OK) {
      if (copy_to_user((void *)userData, &ifr, sizeof(ifr))) {
         return VMK_INVALID_ADDRESS;
      }
   }

   return status;
}

static VMK_ReturnStatus
NicCharOpsOpen(vmk_CharDevFdAttr *attr)
{
   struct net_device *dev;
   vmkplxr_ChardevHandles *handles;

   handles = (vmkplxr_ChardevHandles *) attr->clientDeviceData.ptr;
   VMK_ASSERT(handles != NULL);

   dev = handles->vmklinuxInfo.ptr;
   VMK_ASSERT(dev != NULL);

   dev_hold(dev);

   return VMK_OK;
}

static VMK_ReturnStatus
NicCharOpsClose(vmk_CharDevFdAttr *attr)
{
   struct net_device *dev;
   vmkplxr_ChardevHandles *handles;

   handles = (vmkplxr_ChardevHandles *) attr->clientDeviceData.ptr;
   VMK_ASSERT(handles != NULL);

   dev = handles->vmklinuxInfo.ptr;
   VMK_ASSERT(dev != NULL);

   dev_put(dev);

   return VMK_OK;
}


static vmk_CharDevOps nicCharOps = {
   NicCharOpsOpen,
   NicCharOpsClose,
   NicCharOpsIoctl,
   NULL,
   NULL,
   NULL
};

static VMK_ReturnStatus
NicCharDataDestructor(vmk_AddrCookie charData)
{
  /* 
   * The device-private data is in fact the struct net_device,
   * which is destroyed separately from unregistration of the
   * character device.  So, do nothing here.
   */
   return VMK_OK;
}

static int
register_nic_chrdev(struct net_device *dev)
{
   VMK_ReturnStatus status;
   int major = VMKPLXR_DYNAMIC_MAJOR;
   int minor = 0;
   vmk_AddrCookie devCookie;

   if (dev->name) {
      devCookie.ptr = dev;
      status = vmkplxr_RegisterChardev(&major, &minor, dev->name,
                                        &nicCharOps, devCookie,
                                        NicCharDataDestructor,
                                        dev->module_id);
      if (status == VMK_OK) {
         dev->nicMajor = major;
         return 0;
      } else if (status == VMK_BUSY) {
         return -EBUSY;
      }
   } else {
      printk("Device has no name\n");
   }

   return -EINVAL;
}

/*
 *----------------------------------------------------------------------------
 *
 *  LinNet_ConnectUplink --
 *
 *    Register the device with the vmkernel. Initializes various device fields
 *    and sets up PCI hotplug notification handlers.
 *
 *  Results:
 *    0 if successful, non-zero on failure.
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
int
LinNet_ConnectUplink(struct net_device *dev, struct pci_dev *pdev)
{
   vmk_UplinkCapabilities capabilities = 0;
   vmk_Name pollName;
   
   vmk_ModuleID moduleID = VMK_INVALID_MODULE_ID;
   vmk_UplinkConnectInfo connectInfo;

   struct napi_struct *napi;

   /*
    * We should only make this call once per net_device
    */
   VMK_ASSERT(dev->uplinkDev == NULL);

   /*
    * Driver should have made the association with
    * the PCI device via the macro SET_NETDEV_DEV()
    */
   VMK_ASSERT(dev->pdev == pdev);

   /* CNA devices shouldn't go through this path. */
   VMK_ASSERT(!(dev->features & NETIF_F_CNA));

   /*
    * Driver naming device already has the device name in net_device.
    */
   if (!dev->useDriverNamingDevice) {
      netdev_name_adapter(dev, pdev);
   }

   capabilities = netdev_query_capabilities(dev);

   moduleID = dev->module_id;

   VMK_ASSERT(moduleID != VMK_INVALID_MODULE_ID);

   connectInfo.devName = dev->name;
   connectInfo.clientData = dev;
   connectInfo.moduleID = moduleID;
   connectInfo.functions = &linNetFunctions;
   connectInfo.cap = capabilities;

   if (dev->features & NETIF_F_HIDDEN_UPLINK) {
      connectInfo.flags = VMK_UPLINK_FLAG_HIDDEN;
   } else {
      connectInfo.flags = 0;
   }

   if (dev->features & NETIF_F_PSEUDO_REG) {
      connectInfo.flags |= VMK_UPLINK_FLAG_PSEUDO_REG;
   }

   if (vmk_UplinkRegister((vmk_Uplink *)&dev->uplinkDev, &connectInfo) != VMK_OK) {
      goto fail;
   }

   VMK_ASSERT(dev->net_poll);

   (void) vmk_NameFormat(&pollName, "-backup");
   vmk_NetPollRegisterUplink(dev->net_poll, dev->uplinkDev, pollName, VMK_FALSE);

   list_for_each_entry(napi, &dev->napi_list, dev_list) {
      vmk_Name pollName;
      (void) vmk_NameFormat(&pollName, "-%d", napi->napi_id);
      vmk_NetPollRegisterUplink(napi->net_poll, napi->dev->uplinkDev, pollName, VMK_TRUE);
   }


   dev->link_speed = -1;
   dev->full_duplex = 0;

   dev->link_state = VMKLNX_UPLINK_LINK_DOWN;
   dev->watchdog_timeohit_cnt = 0;
   dev->watchdog_timeohit_cfg = VMK_UPLINK_WATCHDOG_HIT_CNT_DEFAULT;
   dev->watchdog_timeohit_stats = 0;
   dev->watchdog_timeohit_panic = VMKLNX_UPLINK_WATCHDOG_PANIC_MOD_ENABLE;
   dev->watchdog_timeohit_period_start = jiffies;

   return register_nic_chrdev(dev);

 fail:
   return -1;
}

/*
 *----------------------------------------------------------------------------
 *
 *  vmklnx_netdev_high_dma_workaround --
 *    Make a copy of a skb buffer in low dma.
 *
 *  Results:
 *    If the copy succeeds then it releases the previous skb and
 *    returns the new one.
 *    If not it returns NULL.
 *
 *  Side effects:
 *    The skb buffer passed to the function might be released.
 *
 *----------------------------------------------------------------------------
 */
struct sk_buff *
vmklnx_netdev_high_dma_workaround(struct sk_buff *base)
{
   struct sk_buff *skb = skb_copy(base, GFP_ATOMIC);

   if (skb) {
      vmk_PktRelease(base->pkt);
   }

   return skb;
}

/*
 *----------------------------------------------------------------------------
 *
 *  vmklnx_netdev_high_dma_overflow --
 *    Check skb buffer's data are located beyond a specified dma limit.
 *
 *  Results:
 *    Returns TRUE if there is an overflow with the passed skb and FALSE
 *    otherwise.
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
#define GB      (1024LL * 1024 * 1024)
int
vmklnx_netdev_high_dma_overflow(struct sk_buff *skb,
                                short gb_limit)
{
   uint64_t dma_addr;
   uint64_t dma_addr_limit;
   int idx_frags;
   int nr_frags;
   skb_frag_t *skb_frag;
   vmk_PktFrag pkt_frag;

   if (VMKLNX_STRESS_DEBUG_COUNTER(stressNetIfForceHighDMAOverflow)) {
      return VMK_TRUE;
   }

   dma_addr_limit = (uint64_t) gb_limit * GB;
   if (dma_addr_limit > max_phys_addr) {
      return VMK_FALSE;
   }

   if (vmk_PktFragGet(skb->pkt, &pkt_frag, 0) != VMK_OK) {
      return VMK_FALSE;
   }

   dma_addr = pkt_frag.addr + (skb->end - skb->head);
   if (dma_addr >= dma_addr_limit) {
      return VMK_TRUE;
   }

   nr_frags = skb_shinfo(skb)->nr_frags;
   for (idx_frags = 0; idx_frags < nr_frags; idx_frags++) {
      skb_frag = &skb_shinfo(skb)->frags[idx_frags];
      dma_addr = page_to_phys(skb_frag->page) + skb_frag->page_offset + skb_frag->size;

      if (dma_addr >= dma_addr_limit) {
         return VMK_TRUE;
      }
   }

   return VMK_FALSE;
}
EXPORT_SYMBOL(vmklnx_netdev_high_dma_overflow);


/*
 *----------------------------------------------------------------------------
 *
 *  vmklnx_skb_real_size --
 *    This call is created to hide the size of "struct LinSkb" so that
 *    it won't be subject to binary compatibility. We can expand LinSkb
 *    in the future when the need comes and do not have to worry about
 *    binary compatibility.
 *
 *  Results:
 *    sizeof(struct LinSkb)
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

size_t
vmklnx_skb_real_size()
{
   return sizeof(struct LinSkb);
}
EXPORT_SYMBOL(vmklnx_skb_real_size);

static void
LinNetComputeEthCRCTableLE(void)
{
   unsigned i, crc, j;

   for (i = 0; i < 256; i++) {
      crc = i;
      for (j = 0; j < 8; j++) {
         crc = (crc >> 1) ^ ((crc & 0x1)? eth_crc32_poly_le : 0);
      }
      eth_crc32_poly_tbl_le[i] = crc;
   }
}

static uint32_t
LinNetComputeEthCRCLE(unsigned crc, const unsigned char *frame, uint32_t frameLen)
{
   int i, j;

   for (i = 0; i + 4 <= frameLen; i += 4) {
      crc ^= *(unsigned *)&frame[i];
      for (j = 0; j < 4; j++) {
         crc = eth_crc32_poly_tbl_le[crc & 0xff] ^ (crc >> 8);
      }
   }

   while (i < frameLen) {
      crc = eth_crc32_poly_tbl_le[(crc ^ frame[i++]) & 0xff] ^ (crc >> 8);
   }

   return crc;
}

/**
 *  crc32_le - Calculate bitwise little-endian Ethernet CRC
 *  @crc: seed value for computation
 *  @p: pointer to buffer over which CRC is run
 *  @len: length of buffer p
 *
 *  Calculates bitwise little-endian Ethernet CRC from an
 *  initial seed value that could be 0 or a previous value if
 *  computing incrementally.
 *
 *  RETURN VALUE:
 *  32-bit CRC value.
 *
 */
/* _VMKLNX_CODECHECK_: crc32_le */
uint32_t
crc32_le(uint32_t crc, unsigned char const *p, size_t len)
{
   return LinNetComputeEthCRCLE(crc, p, len);
}
EXPORT_SYMBOL(crc32_le);

/*
 *----------------------------------------------------------------------------
 *
 * LinNet_Init --
 *
 *    Initialize LinNet data structures.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
void
LinNet_Init(void)
{
   VMK_ReturnStatus status;

   VMKLNX_CREATE_LOG();

   LinStress_SetupStress();
   LinNetComputeEthCRCTableLE();

   /* set up link state timer */
   status = vmk_ConfigParamOpen("Net", "LinkStatePollTimeout",
                                &linkStateTimerPeriodConfigHandle);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_ConfigParamGetUint(linkStateTimerPeriodConfigHandle,
                                   &linkStateTimerPeriod);
   VMK_ASSERT(status == VMK_OK);

   status = vmk_ConfigParamOpen("Net", "VmklnxLROEnabled",
                                &vmklnxLROEnabledConfigHandle);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_ConfigParamGetUint(vmklnxLROEnabledConfigHandle,
                                   &vmklnxLROEnabled);
   VMK_ASSERT(status == VMK_OK);

   status = vmk_ConfigParamOpen("Net", "VmklnxLROMaxAggr",
                                &vmklnxLROMaxAggrConfigHandle);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_ConfigParamGetUint(vmklnxLROMaxAggrConfigHandle,
                                   &vmklnxLROMaxAggr);
   VMK_ASSERT(status == VMK_OK);

   INIT_DELAYED_WORK(&linkStateWork, link_state_work_cb);
   schedule_delayed_work(&linkStateWork,
                         msecs_to_jiffies(linkStateTimerPeriod));

   INIT_DELAYED_WORK(&watchdogWork, watchdog_work_cb);
   schedule_delayed_work(&watchdogWork,
                         msecs_to_jiffies(WATCHDOG_DEF_TIMER));

   status = vmk_ConfigParamOpen("Net", "PortDisableTimeout",
                                &blockTotalSleepMsecHandle);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_ConfigParamGetUint(blockTotalSleepMsecHandle, &blockTotalSleepMsec);
   VMK_ASSERT(status == VMK_OK);

   max_phys_addr = vmk_MachMemMaxAddr();

   status = vmk_ConfigParamOpen("Net", "MaxNetifTxQueueLen",
                                &maxNetifTxQueueLenConfigHandle);
   VMK_ASSERT(status == VMK_OK);

   status = vmk_ConfigParamOpen("Net", "UseHwIPv6Csum",
                                &useHwIPv6CsumHandle);
   VMK_ASSERT(status == VMK_OK);

   status = vmk_ConfigParamOpen("Net", "UseHwCsumForIPv6Csum",
                                &useHwCsumForIPv6CsumHandle);
   VMK_ASSERT(status == VMK_OK);

   status = vmk_ConfigParamOpen("Net", "UseHwTSO", &useHwTSOHandle);
   VMK_ASSERT(status == VMK_OK);

   status = vmk_ConfigParamOpen("Net", "UseHwTSO6", &useHwTSO6Handle);
   VMK_ASSERT(status == VMK_OK);

   status = vmk_StressOptionOpen(VMK_STRESS_OPT_NET_GEN_TINY_ARP_RARP,
                                 &stressNetGenTinyArpRarp);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_StressOptionOpen(VMK_STRESS_OPT_NET_IF_CORRUPT_ETHERNET_HDR,
                                 &stressNetIfCorruptEthHdr);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_StressOptionOpen(VMK_STRESS_OPT_NET_IF_CORRUPT_RX_DATA,
                                 &stressNetIfCorruptRxData);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_StressOptionOpen(VMK_STRESS_OPT_NET_IF_CORRUPT_RX_TCP_UDP,
                                 &stressNetIfCorruptRxTcpUdp);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_StressOptionOpen(VMK_STRESS_OPT_NET_IF_CORRUPT_TX,
                                 &stressNetIfCorruptTx);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_StressOptionOpen(VMK_STRESS_OPT_NET_IF_FAIL_HARD_TX,
                                 &stressNetIfFailHardTx);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_StressOptionOpen(VMK_STRESS_OPT_NET_IF_FAIL_RX,
                                 &stressNetIfFailRx);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_StressOptionOpen(VMK_STRESS_OPT_NET_IF_FAIL_TX_AND_STOP_QUEUE,
                                 &stressNetIfFailTxAndStopQueue);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_StressOptionOpen(VMK_STRESS_OPT_NET_IF_FORCE_HIGH_DMA_OVERFLOW,
                                 &stressNetIfForceHighDMAOverflow);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_StressOptionOpen(VMK_STRESS_OPT_NET_IF_FORCE_RX_SW_CSUM,
                                 &stressNetIfForceRxSWCsum);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_StressOptionOpen(VMK_STRESS_OPT_NET_NAPI_FORCE_BACKUP_WORLDLET,
                                 &stressNetNapiForceBackupWorldlet);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_StressOptionOpen(VMK_STRESS_OPT_NET_BLOCK_DEV_IS_SLUGGISH,
                                 &stressNetBlockDevIsSluggish);
   VMK_ASSERT(status == VMK_OK);
}

/*
 *----------------------------------------------------------------------------
 *
 * LinNet_Cleanup --
 *
 *    Cleanup function for linux_net. Release and cleanup all resources.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */
void LinNet_Cleanup(void)
{
   VMK_ReturnStatus status;

   LinStress_CleanupStress();
   cancel_delayed_work_sync(&linkStateWork);
   cancel_delayed_work_sync(&watchdogWork);
   vmk_TimerRemoveSync(devWatchdogTimer);

   status = vmk_ConfigParamClose(linkStateTimerPeriodConfigHandle);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_ConfigParamClose(maxNetifTxQueueLenConfigHandle);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_ConfigParamClose(useHwIPv6CsumHandle);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_ConfigParamClose(useHwCsumForIPv6CsumHandle);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_ConfigParamClose(useHwTSOHandle);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_ConfigParamClose(useHwTSO6Handle);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_ConfigParamClose(blockTotalSleepMsecHandle);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_ConfigParamClose(vmklnxLROEnabledConfigHandle);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_ConfigParamClose(vmklnxLROMaxAggrConfigHandle);
   VMK_ASSERT(status == VMK_OK);

   status = vmk_StressOptionClose(stressNetGenTinyArpRarp);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_StressOptionClose(stressNetIfCorruptEthHdr);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_StressOptionClose(stressNetIfCorruptRxData);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_StressOptionClose(stressNetIfCorruptRxTcpUdp);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_StressOptionClose(stressNetIfCorruptTx);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_StressOptionClose(stressNetIfFailHardTx);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_StressOptionClose(stressNetIfFailRx);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_StressOptionClose(stressNetIfFailTxAndStopQueue);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_StressOptionClose(stressNetIfForceHighDMAOverflow);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_StressOptionClose(stressNetIfForceRxSWCsum);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_StressOptionClose(stressNetNapiForceBackupWorldlet);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_StressOptionClose(stressNetBlockDevIsSluggish);
   VMK_ASSERT(status == VMK_OK);

   VMKLNX_DESTROY_LOG();
}
