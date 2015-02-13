/* **********************************************************
 * Copyright 2009 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Debug                                                          */ /**
 * \addtogroup Network
 *@{
 * \defgroup Debug Debug
 *@{
 *
 * \par Debug:
 *
 * Networking Debug API's to support debugging the vmkernel.
 * This includes interfaces such as receiving packets directly
 * from the network in exception.
 *
 ***********************************************************************
 */

#ifndef _VMKAPI_NET_NETDEBUG_H_
#define _VMKAPI_NET_NETDEBUG_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "net/vmkapi_net_types.h"

/*
 ***********************************************************************
 * vmk_NetDebugRxProcess --                                      */ /**
 *
 * \brief  Deliver Rx packets directly to the network netdebug handler.
 *
 * \param[in]  pktList            Set of packets to process.
 *
 * \retval     VMK_OK             If the network netdebug handles
 *                                the packets successfully.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_NetDebugRxProcess(vmk_PktList pktList);

#endif /* _VMKAPI_NET_NETDEBUG_H_ */
/** @} */
/** @} */
