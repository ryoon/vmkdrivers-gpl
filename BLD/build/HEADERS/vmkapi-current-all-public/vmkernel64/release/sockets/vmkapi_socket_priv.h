/***************************************************************************
 * Copyright 2007 - 2009 VMware, Inc.  All rights reserved.
 ***************************************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * vmkapi_socket_priv.h                                           */ /**
 * \addtogroup Socket
 * @{
 * \defgroup SocketPrv Network Socket Interfaces (private portion)
 * @{
 *
 ***********************************************************************
 */

#ifndef _VMKAPI_SOCKET_PRIV_H_
#define _VMKAPI_SOCKET_PRIV_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/*
 * Address families
 */
/** \brief  Kernel/User IPC */
#define VMK_SOCKET_AF_VMKLINK    27
/** \brief  IPv6 */
#define VMK_SOCKET_AF_INET6      28

/*
 * Socket types
 */
/** \brief Streaming */
#define VMK_SOCKET_SOCK_STREAM   1

/*
 * Protocol families
 */
#define VMK_SOCKET_PF_VMKLINK    VMK_SOCKET_AF_VMKLINK
#define VMK_SOCKET_PF_INET6      VMK_SOCKET_AF_INET6

/* Max length of connection backlog */
#define VMK_SOCKET_SOMAXCONN     128

/*
 * Socket send/receive flags
 */
/** \brief Process out-of-band data */
#define	VMK_SOCKET_MSG_OOB          0x1
/** \brief Peek at incoming message */
#define	VMK_SOCKET_MSG_PEEK	    0x2
/** \brief Send without using routing tables */
#define	VMK_SOCKET_MSG_DONTROUTE    0x4
/** \brief Data completes record */
#define	VMK_SOCKET_MSG_EOR          0x8
/** \brief Data discarded before delivery */
#define	VMK_SOCKET_MSG_TRUNC	    0x10
/** \brief Control data lost before delivery */
#define	VMK_SOCKET_MSG_CTRUNC	    0x20
/** \brief Wait for full request or error */
#define	VMK_SOCKET_MSG_WAITALL	    0x40
/** \brief this message should be nonblocking */
#define	VMK_SOCKET_MSG_DONTWAIT	    0x80
/** \brief data completes connection */
#define	VMK_SOCKET_MSG_EOF          0x100
/** \brief FIONBIO mode, used by fifofs */
#define	VMK_SOCKET_MSG_NBIO         0x4000
/** \brief used in sendit() */
#define	VMK_SOCKET_MSG_COMPAT       0x8000

/*
 * Socket-level socket options
 */
/** \brief Turn on debugging info recording */
#define	VMK_SOCKET_SO_DEBUG         0x0001
/** \brief Socket has had listen() */
#define	VMK_SOCKET_SO_ACCEPTCON     0x0002
/** \brief Allow local address reuse */
#define	VMK_SOCKET_SO_REUSEADDR     0x0004
/** \brief Keep connections alive */
#define	VMK_SOCKET_SO_KEEPALIVE     0x0008
/** \brief Just use interface addresses */
#define	VMK_SOCKET_SO_DONTROUTE     0x0010
/** \brief Permit sending of broadcast msgs */
#define	VMK_SOCKET_SO_BROADCAST	    0x0020
/** \brief Bypass hardware when possible */
#define	VMK_SOCKET_SO_USELOOPBACK   0x0040
/** \brief Linger on close if data present */
#define	VMK_SOCKET_SO_LINGER	    0x0080
/** \brief Leave received OOB data in line */
#define	VMK_SOCKET_SO_OOBINLINE     0x0100
/** \brief Allow local address & port reuse */
#define	VMK_SOCKET_SO_REUSEPORT     0x0200
/** \brief Timestamp received dgram traffic */
#define	VMK_SOCKET_SO_TIMESTAMP     0x0400
/** \brief There is an accept filter */
#define	VMK_SOCKET_SO_ACCEPTFILTER  0x1000

/*
 * Values for the shutdown call's "how" argument.
 */
/** \brief Further receives will be disallowed */
#define VMK_SOCKET_SHUT_RD       0
/** \brief Further sends will be disallowed. */
#define VMK_SOCKET_SHUT_WR       1
/** \brief Further sends and receives will be disallowed. */
#define VMK_SOCKET_SHUT_RDWR     2


/*
 ***********************************************************************
 * vmk_SocketBind --                                              */ /**
 *
 * \ingroup SocketPrv
 * \brief Bind a socket to a network address endpoint.
 *
 * \note This function will not block.
 *
 * \param[in] socket          Socket to bind to the network address
 * \param[in] address         Information describing the network address
 *                            that the socket will be bound to.
 * \param[in] addressLength   Length in bytes of the network address
 *                            information.
 *
 * \retval VMK_BAD_PARAM       Socket was already bound
 * \retval VMK_NOT_SUPPORTED   Unknown socket type.
 * \retval VMK_EOPNOTSUPP      Maps to BSD error code EOPNOTSUPP.
 * \retval VMK_BAD_PARAM       Maps to BSD error code EINVAL.
 * \retval VMK_EADDRNOTAVAIL   Maps to BSD error code EADDRNOTAVAIL.
 * \retval VMK_ADDRFAM_UNSUPP  Maps to BSD error code EAFNOSUPPORT.
 * \retval VMK_EADDRINUSE      Maps to BSD error code EADDRINUSE.
 * \retval VMK_NO_ACCESS       Maps to BSD error code EACCESS.
 *
 * \note For BSD error code definitions see the FreeBSD 7 man page
 *       bind(2).
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketBind(vmk_Socket socket,
                                vmk_SocketAddress *address,
                                int addressLength);


/*
 ***********************************************************************
 * vmk_SocketRecvFrom --                                          */ /**
 *
 * \ingroup SocketPrv
 * \brief Receive data from a network address.
 *
 * \note This function may block if flags VMK_SOCKET_MSG_DONTWAIT is not
 *       set or is not a nonblocking socket (default).
 *
 * \param[in]     socket         Socket to receive the data through.
 * \param[in]     flags          Settings for this receive transaction.
 * \param[in]     address        The source address information the
 *                               messages should be received from,
 *                               or NULL if this is not necessary for
 *                               the socket's protocol or settings.
 * \param[in,out] addressLength  Length in bytes of the address
 *                               information.
 * \param[in]     data           Pointer to data buffer to receive to.
 * \param[in]     len            Length in bytes of the data buffer.
 * \param[out]    bytesReceived  Number of bytes actually received.
 *
 * \retval VMK_NOT_SUPPORTED     Receive on unbound VMKLINK socket is
 *                               not supported.
 * \retval VMK_EOPNOTSUPP        Maps to BSD error code EOPNOTSUPP.
 * \retval VMK_ENOTCONN          Maps to BSD error code ENOTCONN.
 * \retval VMK_MESSAGE_TOO_LONG  Maps to BSD error code EMSGSIZE.
 * \retval VMK_WOULD_BLOCK       Maps to BSD error code EAGAIN.
 * \retval VMK_ECONNRESET        Maps to BSD error code ECONNRESET.
 * \retval VMK_INVALID_ADDRESS   Maps to BSD error code EFAULT.
 *
 * \note For BSD error code definitions see the FreeBSD 7 man page
 *       recvfrom(2).
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketRecvFrom(vmk_Socket socket,
                                    int flags,
                                    vmk_SocketAddress *address,
                                    int *addressLength,
                                    void *data,
                                    int len,
                                    int *bytesReceived);

/*
 ***********************************************************************
 * vmk_SocketListen --                                            */ /**
 *
 * \ingroup SocketPrv
 * \brief Setup a socket to allow connections.
 *
 * \note This function will not block.
 *
 * \param[in] socket    Socket on which to allow connections.
 * \param[in] backlog   Max number of connections that are allowed to
 *                      wait to be accepted on a connection.
 *
 * \retval VMK_NOT_SUPPORTED   Unknown socket type.
 * \retval VMK_EOPNOTSUPP      Maps to BSD error code EOPNOTSUPP.
 * \retval VMK_ENOTCONN        Maps to BSD error code ENOTCONN.
 * \retval VMK_BAD_PARAM       Maps to BSD error code EINVAL.
 * \retval VMK_WOULD_BLOCK     Maps to BSD error code EAGAIN.
 * \retval VMK_EADDRNOTAVAIL   Maps to BSD error code EADDRNOTAVAIL.
 * \retval VMK_ADDRFAM_UNSUPP  Maps to BSD error code EAFNOSUPPORT.
 * \retval VMK_EADDRINUSE      Maps to BSD error code EADDRINUSE.
 * \retval VMK_NO_ACCESS       Maps to BSD error code EACCESS.
 *
 * \note For BSD error code definitions see the FreeBSD 7 man page
 *       listen(2).
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketListen(vmk_Socket socket,
                                  int backlog);

/*
 ***********************************************************************
 * vmk_SocketAccept --                                            */ /**
 *
 * \ingroup SocketPrv
 * \brief Setup a socket to allow connections.
 *
 * \note This function may block if not a nonblocking socket (default).
 *
 * \note This call may block.
 *
 * \param[in]  socket            Socket on which to allow connections.
 * \param[in]  canBlock          Should this call block?
 * \param[out] address           The network address info of the remote
 *                               connecting network entity.
 * \param[in,out] addressLength  The length of the address info.
 * \param[out] newSocket         A new socket to communicate with the
 *                               connecting network entity.
 * 
 * \retval VMK_NOT_SUPPORTED    Unknown socket type.
 * \retval VMK_BAD_PARAM        Socket not in listen.
 * \retval VMK_WOULD_BLOCK      Socket is nonblocking.
 * \retval VMK_EOPNOTSUPP       Maps to BSD error code EOPNOTSUPP.
 * \retval VMK_NO_MEMORY        Maps to BSD error code ENOMEM.
 * \retval VMK_ECONNABORTED     Maps to BSD error code ECONNABORTED.
 * \retval VMK_BAD_PARAM        Maps to BSD error code EINVAL.
 * \retval VMK_INVALID_ADDRESS  Maps to BSD error code EFAULT.
 *
 * \note For BSD error code definitions see the FreeBSD 7 man page
 *       accept(2).
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketAccept(vmk_Socket socket,
                                  vmk_Bool canBlock,
                                  vmk_SocketAddress *address,
                                  int *addressLength,
                                  vmk_Socket *newSocket);

/*
 ***********************************************************************
 * vmk_SocketConnect --                                           */ /**
 *
 * \ingroup SocketPrv
 * \brief Connect to a network address
 *
 * \note This function may block if not a nonblocking socket (default).
 *
 * \param[in] socket          Socket to connect through.
 * \param[in] address         The network address info for the address
 *                            to connect to.
 * \param[in] addressLength   The length of the address info.
 * 
 * \retval VMK_NOT_SUPPORTED      Unknown socket type.
 * \retval VMK_EALREADY           Socket already connected.
 * \retval VMK_EINPROGRESS        Socket is nonblocking and connection
 *                                is still in progress.
 * \retval VMK_EOPNOTSUPP         Maps to BSD error code EOPNOTSUPP.
 * \retval VMK_ALREADY_CONNECTED  Maps to BSD error code EISCONN.
 * \retval VMK_BAD_PARAM          Maps to BSD error code EINVAL.
 * \retval VMK_EADDRNOTAVAIL      Maps to BSD error code EADDRNOTAVAIL.
 * \retval VMK_ADDRFAM_UNSUPP     Maps to BSD error code EAFNOSUPPORT.
 * \retval VMK_EADDRINUSE         Maps to BSD error code EADDRINUSE.
 * \retval VMK_NO_ACCESS          Maps to BSD error code EACCESS.
 *
 * \note For BSD error code definitions see the FreeBSD 7 man page
 *       connect(2).
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketConnect(vmk_Socket socket,
                                   vmk_SocketAddress *address,
                                   int addressLength);

/*
 ***********************************************************************
 * vmk_SocketShutdown --                                          */ /**
 *
 * \ingroup SocketPrv
 * \brief Shutdown part or all of a connection on a socket
 *
 * \note This function may block if VMK_SOCKET_SO_LINGER has been set
 *       otherwise will not block.
 *
 * \param[in] socket     Socket to query.
 * \param[in] how        Data direction(s) to shutdown on the socket.
 * 
 * \retval VMK_NOT_SUPPORTED  Unknown socket type.
 * \retval VMK_EOPNOTSUPP     Maps to BSD error code EOPNOTSUPP.
 * \retval VMK_ECONNRESET     Maps to BSD error code ECONNRESET.
 * \retval VMK_ENOTCONN       Maps to BSD error code ENOTCONN.
 *
 * \note For BSD error code definitions see the FreeBSD 7 man page
 *       shutdown(2).
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketShutdown(vmk_Socket socket,
                                    int how);
#endif /* _VMKAPI_SOCKET_PRIV_H_ */
/** @} */
/** @} */
