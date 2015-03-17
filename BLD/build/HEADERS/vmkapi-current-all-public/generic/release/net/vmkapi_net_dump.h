/* **********************************************************
 * Copyright 2009 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * CoreDump                                                          */ /**
 * \addtogroup Network
 *@{
 * \defgroup Dump Dump
 *@{
 *
 * \par Dump:
 *
 * Network coredump API's to support the coredump feature.
 * This includes interfaces such as receiving packets directly
 * from the network in exception.
 *
 ***********************************************************************
 */

#ifndef _VMKAPI_NET_NETDUMP_H_
#define _VMKAPI_NET_NETDUMP_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "net/vmkapi_net_types.h"

/**
 * \brief Structure containing Panic-time polling information of the device
 *        associated to an uplink.
 */

typedef struct vmk_UplinkPanicInfo {
   /** Polling data to be passed to the polling function */
   vmk_AddrCookie clientData;
} vmk_UplinkPanicInfo;


/*
 ***********************************************************************
 * vmk_UplinkNetDumpPanicTxCB --                                  */ /**
 *
 * \brief Handler used by vmkernel to send packets
 *
 * \note  This handler is called when vmkernel is in panic state. For
 *        TX completion, driver must call asynchronous vmkapi
 *        vmk_PktListRelease() or vmk_PktRelease inside
 *        vmk_UplinkNetDumpPanicPollCB().  It should not call
 *        vmk_NetPollQueueCompPkt() or request other asynchronous task
 *        to perform the completion.
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[in]   pktList      List of packets to transmit
 *
 * \retval      VMK_OK       If transmit succeed
 * \retval      Other        If transmit failed
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkNetDumpPanicTxCB)(vmk_AddrCookie driverData,
                                                       vmk_PktList pktList);

/*
 ***********************************************************************
 * vmk_UplinkNetDumpPanicPollCB --                                */ /**
 *
 * \brief Handler used by vmkernel to poll for packets received by
 *        the device associated to an uplink. Might be ignored.
 *
 * \note  This handler is called when vmkernel is in panic state. Driver
 *        should not call vmk_NetPollRxPktQueue to queue any RX packets.
 *        Instead, it must insert RX packets into pktList parameter
 *        and return to vmkernel.
 *
 * \note  Driver must perform all TX completion in this callback by
 *        calling vmk_PktListRelease or vmk_PktRelease, since this
 *        callback is guaranteed to be called more frequently than
 *        vmk_UplinkNetDumpPanicTxCB.
 *
 * \param[in]   clientData   Points to the internal module structure
 *                           returned by callback function
 *                           vmk_UplinkNetDumpPanicInfoGetCB(). It can be
 *                           different from the driver data specified in
 *                           vmk_UplinkRegData during device registration.
 * \param[out]  pktList      List of packets received
 *
 * \retval      VMK_OK       Always
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkNetDumpPanicPollCB)(vmk_AddrCookie clientData,
                                                         vmk_PktList pktList);

/*
 ***********************************************************************
 * vmk_UplinkNetDumpPanicInfoGetCB --                             */ /**
 *
 * \brief  Handler used by vmkernel to get panic-time polling properties
 *         of a device associated to an uplink.
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[out]  panicInfo    Panic-time polling properties of the device
 *
 * \retval      VMK_OK       If the information is properly stored
 * \retval      VMK_FAILURE  Otherwise
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkNetDumpPanicInfoGetCB)(vmk_AddrCookie driverData,
                                                            vmk_UplinkPanicInfo *panicInfo);

typedef struct vmk_UplinkNetDumpOps {
   /**
    * callback to transmit packet
    */
   vmk_UplinkNetDumpPanicTxCB           panicTx;

   /**
    * callback to dump panic poll
    */
   vmk_UplinkNetDumpPanicPollCB         panicPoll;

   /**
    * callback to Fump panic info get
    */
   vmk_UplinkNetDumpPanicInfoGetCB      panicInfoGet;
} vmk_UplinkNetDumpOps;

#endif /* _VMKAPI_NET_NETDUMP_H_ */
/** @} */
/** @} */
