/* **********************************************************
 * Copyright 2009 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 * vmkapi_net_vmknic.h --
 *
 *    VMkernel Network APIs for vmknic.
 *
 */

#ifndef _VMKAPI_NET_VMKNIC_H_
#define _VMKAPI_NET_VMKNIC_H_

#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif

#include "net/vmkapi_net_types.h"

/**
 * \brief
 * Abstract socket network address
 *
 * A protocol-specific address is used in actual practice.
 *
 * This is copied from sockets/vmkapi_socket.h since we can't 
 * include it here due to its scope limitation.
 */
struct vmk_SocketAddress;

/*
 * The maximum length of a vmknic name including the 
 * terminating nul. 
 */
#define VMK_VMKNIC_NAME_MAX           32

/*
 ***********************************************************************
 * vmk_VmknicIPAddrGet --                                         */ /**
 *
 * \ingroup vmknic 
 * \brief Retrieve the IP address of the given vmknic.
 *
 * \note Currently only IPv4 is supported, so ipAddr.sin_family has to 
 *       be set to VMK_SOCKET_AF_INET before calling this API. 
 *
 * \param[in] vmknic		VMkernel NIC interface (vmknic) name. 
 * \param[in, out] ipAddr	IP address of the vmknic.
 *
 * \retval VMK_OK 		Retrieve IP address succeeded.
 * \retval VMK_NOT_INITIALIZED	vmknic or ipAddr is not initialized.
 * \retval VMK_NOT_SUPPORTED	ipV6 is not supported. 
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_VmknicIPAddrGet(
   const char *vmknic, 
   struct vmk_SocketAddress *ipAddr); 

/*
 ***********************************************************************
 * vmk_VmknicMACAddrGet --                                        */ /**
 *
 * \ingroup vmknic 
 * \brief Retrieve the MAC address of the given vmknic.
 *
 * \param[in] vmknic	VMkernel NIC interface (vmknic) name. 
 * \param[out] macAddr	MAC address of the vmknic.
 *
 * \retval VMK_OK 		Retrieve MAC address succeeded.
 * \retval VMK_NOT_INITIALIZED	vmknic or macAddr is not initialized.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_VmknicMACAddrGet(
   const char *vmknic, 
   vmk_EthAddress macAddr); 

#endif // _VMKAPI_NET_VMKNIC_H_
