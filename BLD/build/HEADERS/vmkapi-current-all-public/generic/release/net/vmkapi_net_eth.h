/* **********************************************************
 * Copyright 2008 - 2009 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Ethernet                                                      */ /**
 * \addtogroup Net
 * @{
 * \defgroup NetEth Ethernet APIs
 *
 * In vmkernel, portset is a generic framework capable of handling
 * arbitrary traffic. This group defines structures and interfaces
 * commonly used for handling ethernet packets.
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_NET_ETH_H_
#define _VMKAPI_NET_ETH_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "net/vmkapi_net_types.h"
#include "net/vmkapi_net_pkt.h"
#include "net/vmkapi_net_pktlist.h"

#define VMK_ETH_LADRF_LEN                 2
#define VMK_ETH_MAX_EXACT_MULTICAST_ADDRS 32

/**
 * \brief Type of Ethernet address filter.
 */
typedef enum vmk_EthFilterFlags {
   /** Pass unicast (directed) frames */
   VMK_ETH_FILTER_UNICAST   = 0x0001,

   /** Pass some multicast frames */
   VMK_ETH_FILTER_MULTICAST = 0x0002,

   /** Pass *all* multicast frames */
   VMK_ETH_FILTER_ALLMULTI  = 0x0004,

   /** Pass broadcast frames */
   VMK_ETH_FILTER_BROADCAST = 0x0008,

   /** Pass all frames (i.e. no filter) */
   VMK_ETH_FILTER_PROMISC   = 0x0010,

   /** Use LADRF for multicast filter */
   VMK_ETH_FILTER_USE_LADRF = 0x0020
} vmk_EthFilterFlags;

/**
 * \brief Ethernet address filter
 */
typedef struct {
   /** Type of filter(s) to apply */
   vmk_EthFilterFlags flags;

   /** Unicast addr to filter on      */
   vmk_EthAddress   unicastAddr;

   /** Number of exact multcast addrs */
   vmk_uint16       numMulticastAddrs;

   /** Multicast addresses to filter on */
   vmk_EthAddress   multicastAddrs[VMK_ETH_MAX_EXACT_MULTICAST_ADDRS];

   /** Lance style logical address filter */
   vmk_uint32       ladrf[VMK_ETH_LADRF_LEN];
} vmk_EthFilter;

/**
 * \brief Ethernet Frame routing policy
 */
typedef struct {
   /** Like the rx filter on a real NIC */
   vmk_EthFilter    requestedFilter;

   /** Restricted to a subset of above  */
   vmk_EthFilter    acceptedFilter;
} vmk_EthFRP;


/*
 ***********************************************************************
 * vmk_EthDestinationFilter --                                    */ /**
 *
 * \nativedriversdisallowed
 *
 * \brief Filter ethernet frames based on the destination address.
 *
 * \param[in]      filter      Ethernet address filter
 * \param[in,out]  pktListIn   Input packet list, to be filtered
 * \param[out]     pktListOut  List of packets filtered out
 *
 * \retval     VMK_OK
 ***********************************************************************
 */
VMK_ReturnStatus vmk_EthDestinationFilter(vmk_EthFilter *filter,
                                          vmk_PktList pktListIn,
                                          vmk_PktList pktListOut);

#endif
/** @} */
/** @} */
