/* **********************************************************
 * Copyright 2012 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * PktCapture                                                          */ /**
 * \addtogroup Network
 * @{
 * \defgroup Pkt Packet Management
 * @{
 *
 * \par Packet capture
 *
 * Packet capture APIs to capture interesting packets and export to
 * user world.
 *
 ***********************************************************************
 */

#ifndef _VMKAPI_NET_PKT_CAPTURE_H_
#define _VMKAPI_NET_PKT_CAPTURE_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "net/vmkapi_net_pkt.h"


/**
 * \brief Reasons for a packet to be dropped.
 */
typedef enum {
   /** No Drop */
   VMK_PKTCAP_NO_DROP = 0,
   /** Enqueue Failed */
   VMK_PKTCAP_ENQUEUE_FAIL_DROP = 1,
   /** Uplink device stopped */
   VMK_PKTCAP_DEV_STOPPED_DROP = 2 ,
   /** Dropped during uplink TX */
   VMK_PKTCAP_DEV_TX_DROP = 3,
   /** Dropped during uplink RX */
   VMK_PKTCAP_DEV_RX_DROP = 4,
   /** Failed TSO */
   VMK_PKTCAP_TSO_FAIL_DROP = 5,
   /** Dropped by terminal IOChain */
   VMK_PKTCAP_TERMINAL_IOCHAIN_DROP = 6,
   /** Dropped by Shaper */
   VMK_PKTCAP_SHAPER_DROP = 7,
   /** Bad/Failed Checksum */
   VMK_PKTCAP_CSUM_FAIL_DROP = 8,
   /** Vlan mismatch */
   VMK_PKTCAP_VLAN_DROP = 9,
   /** Dropped by vxlan module */
   VMK_PKTCAP_VXLAN_DROP = 10,
   /** Paddign failed */
   VMK_PKTCAP_PADDING_FAIL_DROP = 11,
   /** MAC forgery drop */
   VMK_PKTCAP_MAC_FORGERY_DROP = 12,
   /** Dropped by Firewall */
   VMK_PKTCAP_FIREWALL_DROP = 13,
   /** Port blocked */
   VMK_PKTCAP_PORT_BLOCKED_DROP = 14,
   /** Generic Reason */
   VMK_PKTCAP_DEFAULT_DROP = 15,
   /** Dropped by TCPIP stack */
   VMK_PKTCAP_TCPIP_DROP = 16,
   /** Used to indicate the number
     * of drop reasons */
   VMK_PKTCAP_DROP_REASON_MAX = 17
} vmk_PktDropReason;

/**
 * \brief Packet capture point type.
 */
typedef enum {
   /** Invalid capture point */
   VMK_PKTCAP_POINT_INVALID = 0,
   /** Dynamic capture point: specify fn name */
   VMK_PKTCAP_POINT_DYNAMIC = 1,
   /** Uplink Rx capture point in driver */
   VMK_PKTCAP_POINT_UPLINK_DRIVER_RX = 2,
   /** Uplink Tx capture point in driver */
   VMK_PKTCAP_POINT_UPLINK_DRIVER_TX = 3,
   /** Vnic Tx capture point */
   VMK_PKTCAP_POINT_VNIC_TX = 4,
   /** Vnic Rx capture point */
   VMK_PKTCAP_POINT_VNIC_RX = 5,
   /** Port Input capture point */
   VMK_PKTCAP_POINT_PORT_INPUT = 6,
   /** IOChain capture point */
   VMK_PKTCAP_POINT_IOCHAIN = 7,
   /** Etherswitch Dispatch capture point */
   VMK_PKTCAP_POINT_SWITCH_DISPATCH = 8,
   /** Switch Output capture point */
   VMK_PKTCAP_POINT_SWITCH_OUTPUT = 9,
   /** Port Output capture point */
   VMK_PKTCAP_POINT_PORT_OUTPUT = 10,
   /** TCP/IP capture point */
   VMK_PKTCAP_POINT_TCPIP = 11,
   /** Just before DVFilter */
   VMK_PKTCAP_POINT_PRE_DVFILTER = 12,
   /** Just after DVFilter */
   VMK_PKTCAP_POINT_POST_DVFILTER = 13,
   /** Dropped packets capture point */
   VMK_PKTCAP_POINT_DROP = 14,
   /** VDR RX IOChain */
   VMK_PKTCAP_POINT_VDR_RX_FIRST = 15,
   /** VDR TX IOChain */
   VMK_PKTCAP_POINT_VDR_TX_FIRST = 16,
   /** VDR RX Terminal IOChain */
   VMK_PKTCAP_POINT_VDR_RX_TERMINAL = 17,
   /** VDR TX Terminal IOChain */
   VMK_PKTCAP_POINT_VDR_TX_TERMINAL = 18,
   /** Freed packets capture point */
   VMK_PKTCAP_POINT_PKTFREE = 19,
   /** TCP/IP Rx capture point */
   VMK_PKTCAP_POINT_TCPIP_RX = 20,
   /** TCP/IP Tx capture point */
   VMK_PKTCAP_POINT_TCPIP_TX = 21,
   /** Uplink Rx capture point in kernel */
   VMK_PKTCAP_POINT_UPLINK_KERNEL_RX = 22,
   /** Uplink Tx capture point in kernel */
   VMK_PKTCAP_POINT_UPLINK_KERNEL_TX = 23,
   /** Used to indicate max number of capture points */
   VMK_PKTCAP_POINT_MAX = 24,
} vmk_PktCapPoint;

/** Capture the packet list for export to user world. */
#define VMK_CAPTURE_PKTLIST(pktList, TYPE, DATA) {            \
   if (vmk_PktCapIsEnabled()) {                               \
      vmk_PktCapPktlist(pktList, TYPE, __func__, DATA);       \
   }                                                          \
}                                                             \


/** Capture the packet for export to user world. */
#define VMK_CAPTURE_PKT(pkt, TYPE, DATA) {                    \
   if (vmk_PktCapIsEnabled()) {                               \
      vmk_PktCapPkt(pkt, TYPE, __func__, DATA);               \
   }                                                          \
}                                                             \


/*
 ***********************************************************************
 * vmk_PktCapPktlist --                                           */ /**
 *
 * \brief Send the packet list to the Packet capture/trace framework
 *        for export to user world.
 *
 * \param[in]  pktList         Packetlist of interest
 * \param[in]  captureId       Identifier of capture point
 * \param[in]  funcPtr         Address of calling function
 * \param[in]  data            Custom data
 *
 * \retval     VMK_OK          Capture point is valid
 * \retval     VMK_BAD_PARAM   Packet list is NULL or invalid
 *                             capture point
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PktCapPktlist(vmk_PktList pktList,
                                   vmk_PktCapPoint captureId,
                                   const char *funcPtr,
                                   void *data);
/*
 ***********************************************************************
 * vmk_PktCapPkt --                                               */ /**
 *
 * \brief Send the packet to the Packet capture/trace framework
 *        for export to user world.
 *
 * \param[in]  pkt             Packet of interest
 * \param[in]  captureId       Identifier of capture point
 * \param[in]  funcPtr         Address of calling function
 * \param[in]  data            Custom data
 *
 * \retval     VMK_OK          Capture point is valid
 * \retval     VMK_BAD_PARAM   pakket is NULL or invalid capture point
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PktCapPkt(vmk_PktHandle *pkt,
                               vmk_PktCapPoint captureId,
                               const char *funcPtr,
                               void *data);

/*
 ***********************************************************************
 * vmk_PktCapIsEnabled --                                         */ /**
 *
 * \brief Check if the packet capture flag is enabled.
 *
 * \retval     None.
 *
 ***********************************************************************
 */
vmk_Bool vmk_PktCapIsEnabled(void);

#endif /* _VMKAPI_NET_PKTCAPTURE_H_ */
/** @} */
/** @} */
