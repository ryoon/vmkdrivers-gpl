/***************************************************************************
 * Copyright 2007 - 2009 VMware, Inc.  All rights reserved.
 ***************************************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * VmkLink Sockets                                                */ /**
 * \addtogroup Socket
 * @{
 *
 * \defgroup SocketVmkLink VmkLink IPC Socket Interfaces 
 * @{
 *
 ***********************************************************************
 */

#ifndef _VMKAPI_SOCKET_VMKLINK_H_
#define _VMKAPI_SOCKET_VMKLINK_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "sockets/vmkapi_socket.h"

/*
 * VmkLink Protocol types
 */
#define VMK_SOCKET_VMKLINK_PROTO_DEFAULT   0

/*
 * VmkLink-level Socket Options
 */

/*
 ***********************************************************************
 * vmk_SocketVmkLinkCallback --                                  */ /**
 *
 * \ingroup SocketVmkLink
 * \brief Callback to call when data is available on a VmkLink socket
 *
 * \note This callback is called with a lock held. Therefore, it should
 *       perform a short action (such as waking up a world) and return
 *       as soon as possible.
 *
 * \param[in]  socket      Socket the data is available on.
 * \param[in]  len         Length of the data that's available.
 * \param[in]  private     Private data associated with the socket's
 *                         address.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_SocketVmkLinkCallback)(vmk_Socket socket,
                                                       vmk_ByteCount len,
                                                       void *private);

/** \brief The maximum length of a channel name */
#define VMK_SOCKET_VMKLINK_CHANNEL_NAME_MAX   31

                                 
/**
 * \brief
 * A VmkLink-style socket address
 */
typedef struct vmk_SocketVmkLinkAddress {
   /** \brief Should be sizeof(vmk_SocketVmkLinkAddress) */
   vmk_uint8 len;

   /** \brief Should be VMK_SOCKET_AF_VMKLINK */
   vmk_uint8 family;
   
   /** \brief String describing the channel name */
   char channelName[VMK_SOCKET_VMKLINK_CHANNEL_NAME_MAX+1];
   
   /**
    * \brief Callback invoked when data is available
    *
    * \note This callback cannot call vmk_SocketClose() on the
    *       socket which invoked it.
    */
   vmk_SocketVmkLinkCallback receiveCallback;
   
   /**
    * \brief Private data associatd with the socket and
    * passed to the callback
    */
   void *private;
} vmk_SocketVmkLinkAddress;

#endif /* _VMKAPI_SOCKET_VMKLINK_H_ */
/** @} */
/** @} */
