/***************************************************************************
 * Copyright 2007 - 2014 VMware, Inc.  All rights reserved.
 ***************************************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Sockets                                                        */ /**
 * \defgroup Socket Network Socket Interfaces
 * @{
 *
 ***********************************************************************
 */

#ifndef _VMKAPI_SOCKET_H_
#define _VMKAPI_SOCKET_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "sockets/vmkapi_socket_util.h"

/**
 * \brief Opaque socket handle.
 */
typedef struct vmk_SocketInt *vmk_Socket;

/**
 * \brief Definition for an invalid vmk_Socket.
 */
#define VMK_SOCKET_INVALID      ((vmk_Socket)NULL)

/*
 * Address families
 */
/** \brief IPv4 */
#define VMK_SOCKET_AF_INET       2

/** \brief IPv6 */
#define VMK_SOCKET_AF_INET6      28

/*
 * Socket types
 */
/** \brief Streaming */
#define VMK_SOCKET_SOCK_STREAM   1

/** \brief Datagrams */
#define VMK_SOCKET_SOCK_DGRAM    2

/** \brief Raw datagrams */
#define VMK_SOCKET_SOCK_RAW      3

/*
 * Flags for vmk_SocketSendTo() and vmk_SocketRecvFrom()
 */
/** \brief Send/receive of this message should not block */
#define VMK_SOCKET_MSG_DONTWAIT     0x80
/** \brief Receive from socket inside socket callback function */
#define VMK_SOCKET_MSG_SOCALLBCK    0x10000

/*
 * Socket option levels
 */
/** \brief Operate on the socket itself */
#define VMK_SOCKET_SOL_SOCKET       0xffff

/*
 * Socket-level socket options
 */
/** \brief Allow local address reuse */
#define VMK_SOCKET_SO_REUSEADDR     0x0004
/** \brief Keep connections alive */
#define VMK_SOCKET_SO_KEEPALIVE     0x0008
/** \brief Just use interface addresses */
#define VMK_SOCKET_SO_DONTROUTE     0x0010
/** \brief Linger on close if data present */
#define VMK_SOCKET_SO_LINGER        0x0080
/** \brief Allow local address and port reuse */
#define VMK_SOCKET_SO_REUSEPORT     0x0200
/** \brief Timestamp received dgram traffic */
#define VMK_SOCKET_SO_TIMESTAMP     0x0400
/** \brief Use non-blocking socket semantics */
#define VMK_SOCKET_SO_NONBLOCKING   0x1015

/** \brief Bind socket to a vmknic
 *  \note Note that the TCP/IP stack will only transmit the packet if the routing
 *        decision indicates that it can be sent out of the specified vmknic.
 */
#define VMK_SOCKET_SO_BINDTOVMK     0x1016

/*
 * Values for the vmk_SocketShutdown()'s "how" parameter
 */
/** \brief Further receives will be disallowed */
#define VMK_SOCKET_SHUT_RD       0
/** \brief Further sends will be disallowed. */
#define VMK_SOCKET_SHUT_WR       1
/** \brief Further sends and receives will be disallowed. */
#define VMK_SOCKET_SHUT_RDWR     2

/**
 * \brief
 * Data structure for setting the VMK_SOCKET_SO_LINGER option
 */
typedef struct vmk_SocketLingerData {
   /** \brief Whether linger is enabled or not */
   vmk_uint32   enabled;
   /** \brief Linger duration (in seconds) */
   vmk_uint32   duration;
} VMK_ATTRIBUTE_PACKED vmk_SocketLingerData;

/*
 ***********************************************************************
 * vmk_SocketCallbackFn --                                        */ /**
 *
 * \ingroup Socket
 * \brief Called when data is available or when data is sent out.
 *
 * \softwaredriversonly
 *
 * This callback is registered by vmk_SocketRegisterRecvBufferCallback
 * or vmk_SocketRegisterSendBufferCallback.
 *
 * \param[in]  socket      Socket the data is available on or sent out
 *                         from.
 * \param[in]  arg         Argument given when registering the callback.
 *
 ***********************************************************************
 */
typedef void (*vmk_SocketCallbackFn)(vmk_Socket socket, void *arg);

/*
 ***********************************************************************
 * vmk_SocketCreate --                                            */ /**
 *
 * \ingroup Socket
 * \brief Create a new unbound socket.
 *
 * \softwaredriversonly
 *
 * \note This function will not block.
 * \note The default behavior is to create a blocking socket. If
 *       nonblocking behavior is required then the
 *       VMK_SOCKET_SO_NONBLOCKING socket option must be set.
 *
 * \param[in]  domain      Protocol family for this socket.
 * \param[in]  type        Type of communication on this socket
 * \param[in]  protocol    Specific protocol to use from address family
 * \param[out] socket      Newly created socket
 *
 * \retval VMK_OK               Success. 
 * \retval VMK_BAD_PARAM        Bad input parameter. 
 * \retval VMK_NO_MODULE_HEAP   The module's heap is not set.
 * \retval VMK_NO_MEMORY        Unable to allocate memory for socket. 
 * \retval VMK_EPROTONOSUPPORT  Maps to BSD error code EPROTONOSUPPORT.
 * \retval VMK_BAD_PARAM_TYPE   Maps to BSD error code EPROTOTYPE.
 * \retval VMK_NO_BUFFERSPACE   Maps to BSD error code ENOBUFS.
 * \retval VMK_EOPNOTSUPP       Maps to BSD error code EOPNOTSUPP.
 * \retval VMK_NO_ACCESS        Maps to BSD error code EACCESS.
 *
 * \note For BSD error code definitions see the FreeBSD 7 man page
 *       socket(2).
 *
 * \note Specific domain (VMK_SOCKET_AF_*), type (VMK_SOCKET_SOCK_*),
 *       and protocol (VMK_SOCKET_*PROTO*) values are implementation
 *       dependent, an application can determine if a specific domain
 *       and type is supported by trying to create a socket with zero
 *       protocol value.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketCreate(int domain,
                                  int type,
                                  int protocol,
                                  vmk_Socket *socket);

/*
 ***********************************************************************
 * vmk_SocketBind --                                              */ /**
 *
 * \ingroup Socket
 * \brief Bind a socket to a network address endpoint.
 *
 * \nativedriversdisallowed
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
 * \retval VMK_NO_MODULE_HEAP  This module's heap is not set.
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
 * vmk_SocketConnect --                                           */ /**
 *
 * \ingroup Socket
 * \brief Connect to a network address
 *
 * \softwaredriversonly
 *
 * \note This function may block if the socket is blocking socket
 * (this is the default behavior). If nonblocking behavior is required
 * then the VMK_SOCKET_SO_NONBLOCKING socket option must be set.
 * \note On non-blocking sockets, the connection may not be established
 * by the time the function returns. In such a case, VMK_EINPROGRESS
 * will be returned. A subsequent invocation of the send socket callback
 * will indicate updates to the connection status. The send socket
 * callback can use \ref vmk_SocketIsConnected() to check the connection
 * status.
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
 * \ingroup Socket
 * \brief Shutdown part or all of a connection on a socket
 *
 * \softwaredriversonly
 *
 * \note This function may block if VMK_SOCKET_SO_LINGER has been set
 *       otherwise it will not block.
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

/*
 ***********************************************************************
 * vmk_SocketClose --                                             */ /**
 *
 * \ingroup Socket
 * \brief Destroy an existing socket.
 *
 * \softwaredriversonly
 *
 * \note This function may block if VMK_SOCKET_SO_LINGER has been set
 *       otherwise will not block.
 *
 * \param[in] socket Socket to close
 *
 * \retval VMK_OK             Success. 
 * \retval VMK_BAD_PARAM      Input socket is invalid. 
 * \retval VMK_NO_MODULE_HEAP The module's heap is not set.
 * \retval VMK_BUSY           Socket is already closing.
 * \retval VMK_NOT_SUPPORTED  Unknown socket type.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketClose(vmk_Socket socket);

/*
 ***********************************************************************
 * vmk_SocketGetSockOpt --                                        */ /**
 *
 * \ingroup Socket
 * \brief Get the option information from a socket
 *
 * \softwaredriversonly
 *
 * \note This function will not block.
 *
 * \param[in]  socket   Socket to get the option info from.
 * \param[in]  level    Level of communication infrastructure from which
 *                      to get the socket option.
 * \param[in]  option   The option to get the information about.
 * \param[out] optval   Data that is currently set on the option.
 * \param[out] optlen   The length of option data.
 *
 * \retval VMK_OK             Success. 
 * \retval VMK_BAD_PARAM      Input socket is invalid. 
 * \retval VMK_NOT_SUPPORTED  Unknown socket type.
 * \retval VMK_NOT_SUPPORTED  Maps to BSD error code ENOPROTOOPT.
 * \retval VMK_EOPNOTSUPP     Maps to BSD error code EOPNOTSUPP.
 * \retval VMK_BAD_PARAM      Maps to BSD error code EINVAL.
 *
 * \note For BSD error code definitions see the FreeBSD 7 man page
 *       getsockopt(2).
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketGetSockOpt(vmk_Socket socket,
                                      int level,
                                      int option,
                                      void *optval,
                                      int *optlen);

/*
 ***********************************************************************
 * vmk_SocketSetSockOpt --                                        */ /**
 *
 * \ingroup Socket
 * \brief Set the option information on a socket
 *
 * \softwaredriversonly
 *
 * \note This function will not block.
 *
 * \param[in] socket    Socket to set the option info on.
 * \param[in] level     Level of communication infrastructure from which
 *                      to set the socket option.
 * \param[in] option    The option to set the information about.
 * \param[in] optval    Data to set on the option.
 * \param[in] optlen    The length of the option data.
 *
 * \retval VMK_OK             Success. 
 * \retval VMK_BAD_PARAM      Input socket is invalid. 
 * \retval VMK_NOT_SUPPORTED  Unknown socket type.
 * \retval VMK_NOT_SUPPORTED  Maps to BSD error code ENOPROTOOPT.
 * \retval VMK_EOPNOTSUPP     Maps to BSD error code EOPNOTSUPP.
 * \retval VMK_BAD_PARAM      Maps to BSD error code EINVAL.
 *
 * \note For BSD error code definitions see the FreeBSD 7 man page
 *       setsockopt(2).
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketSetSockOpt(vmk_Socket socket,
                                      int level,
                                      int option,
                                      const void *optval,
                                      int optlen);

/*
 ***********************************************************************
 * vmk_SocketSendTo --                                            */ /**
 *
 * \ingroup Socket
 * \brief Send data to a network address.
 *
 * \softwaredriversonly
 *
 * \note This function may block if the VMK_SOCKET_MSG_DONTWAIT flag is
 *       not set or the socket is a blocking socket.
 *
 * \param[in]  socket         Socket to send the data through.
 * \param[in]  flags          Settings for this send transaction.
 * \param[in]  address        Address information describing the
 *                            data's destination.
 * \param[in]  data           Pointer to data buffer to send.
 * \param[in]  len            Length in bytes of the data buffer to send.
 * \param[out] bytesSent      Number of bytes actually sent.
 *
 * \retval VMK_OK                 Success.
 * \retval VMK_NOT_SUPPORTED      Unknown socket type.
 * \retval VMK_BAD_PARAM          Unsupported flags setting.
 * \retval VMK_EOPNOTSUPP         Maps to BSD error code EOPNOTSUPP.
 * \retval VMK_ENOTCONN           Maps to BSD error code ENOTCONN.
 * \retval VMK_EDESTADDRREQ       Maps to BSD error code EDESTADDRREQ.
 * \retval VMK_MESSAGE_TOO_LONG   Maps to BSD error code EMSGSIZE.
 * \retval VMK_WOULD_BLOCK        Maps to BSD error code EAGAIN.
 * \retval VMK_NO_BUFFERSPACE     Maps to BSD error code ENOBUFS.
 * \retval VMK_EHOSTUNREACH       Maps to BSD error code EHOSTUNREACH.
 * \retval VMK_ALREADY_CONNECTED  Maps to BSD error code EISCONN.
 * \retval VMK_ECONNREFUSED       Maps to BSD error code ECONNREFUSED.
 * \retval VMK_EHOSTDOWN          Maps to BSD error code EHOSTDOWN.
 * \retval VMK_ENETDOWN           Maps to BSD error code ENETDOWN.
 * \retval VMK_EADDRNOTAVAIL      Maps to BSD error code EADDRNOTAVAIL.
 * \retval VMK_BROKEN_PIPE        Maps to BSD error code EPIPE.
 *
 * \note For BSD error code definitions see the FreeBSD 7 man page
 *       sendto(2).
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketSendTo(vmk_Socket socket,
                                  int flags,
                                  vmk_SocketAddress *address,
                                  void *data,
                                  int len,
                                  int *bytesSent);

/*
 ***********************************************************************
 * vmk_SocketRecvFrom --                                          */ /**
 *
 * \ingroup Socket
 * \brief Receive data from a network address.
 *
 * \softwaredriversonly
 *
 * \note This function may block if the VMK_SOCKET_MSG_DONTWAIT flag is
 *       not set or the socket is a blocking socket.
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
 * vmk_SocketGetSockName --                                       */ /**
 *
 * \ingroup Socket
 * \brief Get the socket's local endpoint network address information.
 *
 * \softwaredriversonly
 *
 * \note This function will not block.
 *
 * \param[in] socket             Socket to query.
 * \param[out] address           The network address info for the socket
 *                               local endpoint.
 * \param[in,out] addressLength  The length of the address info.
 *
 * \retval VMK_OK             Success.
 * \retval VMK_BAD_PARAM      Bad input socket or address.
 * \retval VMK_NOT_SUPPORTED  Unknown socket family.
 * \retval VMK_EOPNOTSUPP     Maps to BSD error code EOPNOTSUPP.
 * \retval VMK_BAD_PARAM      Maps to BSD error code EINVAL.
 * \retval VMK_NO_MEMORY      Maps to BSD error code ENOMEM.
 *
 * \note For BSD error code definitions see the FreeBSD 7 man page
 *       getsockname(2).
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketGetSockName(vmk_Socket socket,
                                       vmk_SocketAddress *address,
                                       int *addressLength);

/*
 ***********************************************************************
 * vmk_SocketGetPeerName --                                       */ /**
 *
 * \ingroup Socket
 * \brief Get the socket's far endpoint network address information.
 *
 * \softwaredriversonly
 *
 * \note This function will not block.
 *
 * \param[in]  socket            Socket to query.
 * \param[out] address           The network address info for the
 *                               socket remote endpoint.
 * \param[in,out] addressLength  The length of the address info.
 *
 * \retval VMK_OK             Success.
 * \retval VMK_BAD_PARAM      Bad input socket or address.
 * \retval VMK_NOT_SUPPORTED  Unknown socket family.
 * \retval VMK_EOPNOTSUPP     Maps to BSD error code EOPNOTSUPP.
 * \retval VMK_NO_MEMORY      Maps to BSD error code ENOMEM.
 * \retval VMK_ENOTCONN       Maps to BSD error code ENOTCONN.
 *
 * \note For BSD error code definitions see the FreeBSD 7 man page
 *       getpeername(2).
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketGetPeerName(vmk_Socket socket,
                                       vmk_SocketAddress *address,
                                       int *addressLength);

/*
 ***********************************************************************
 * vmk_SocketRecvSgVirtArrayZeroCopy --                           */ /**
 *
 * \ingroup Socket
 * \brief Receive a scatter-gather VA array using zero copy.
 *
 * \softwaredriversonly
 *
 * Zero copy implies that the sgVirtArray returned as out argument by
 * the function (and the corresponding data buffers) are owned by the
 * underlying layers, and the ownership is being passed to the caller,
 * without making a copy of the buffers. The caller must treat the
 * data as read-only, and after consumption, the caller must transfer
 * the ownership back by calling vmk_SgVirtArrayFree.
 *
 * Example:
 *
 * 1. vmk_SocketRecvSgVirtArrayZeroCopy(): returns recvdSgVirtArray
 *
 * 2. Consume the data
 *       a. vmk_SgVirtArrayGetUsedElems: returns usedElems.
 *       b. For i from 0 to usedElems:
 *             i. vmk_SgVirtArrayGetElem().
 *             ii. vmk_SgVirtElemGetInfo().
 *             iii. Consume the data.
 *
 * 3. vmk_SgVirtArrayFree(): releases the ownership back to underlying
 * layer.
 *
 * \note The data buffers returned are read-only.
 * \note Will block for blocking socket, will return immediately for
 *       non-blocking sockets.
 * \note For blocking socket, if the peer socket closes connection, this
 *       call will return immediately. In such a case, VMK_OK will be
 *       returned and *recvdSgVirtArray will be set to NULL.
 * \note Only TCP sockets (VMK_SOCKET_IPPROTO_TCP) are currently
 *       supported.
 *
 * \param[in]     socket            Socket to receive from.
 * \param[in]     flags             Settings for this receive
 *                                  transaction.
 * \param[in,out] address           The source address information the
 *                                  messages should be received from,
 *                                  or NULL if this is not necessary for
 *                                  the socket's protocol or settings.
 * \param[in,out] addressLength     Length in bytes of the address
 *                                  information.
 * \param[in]     len               Length in bytes of the data buffer.
 * \param[out]    recvdSgVirtArray  SG VA array for the buffer received.
 *
 * \retval VMK_OK             Success.
 * \retval VMK_WOULD_BLOCK    No more bytes to read from receive
 *                            socket buffer.
 * \retval VMK_BAD_PARAM      Bad input socket or address.
 * \retval VMK_NOT_SUPPORTED  Unknown socket family or not a TCP socket.
 * \retval VMK_NO_BUFFERSPACE Unable to allocate memory for the
 *                            SG VA array.
 *
 * \sideeffect Upon any memory allocation failure (VMK_NO_BUFFERSPACE),
 *             the socket will be shutdown for read operations. No more
 *             data can be read from the socket after such an error.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_SocketRecvSgVirtArrayZeroCopy(vmk_Socket socket,
                                  int flags,
                                  vmk_SocketAddress *address,
                                  int *addressLength,
                                  int len,
                                  vmk_SgVirtArray *recvdSgVirtArray);

/*
 ***********************************************************************
 * vmk_SocketRegisterRecvBufferCallback --                        */ /**
 *
 * \ingroup Socket
 * \brief Registers a callback notifying of receive buffer changes.
 *
 * \softwaredriversonly
 *
 * The callback will be invoked when data is available to be read,
 * or on events like connection being closed.
 *
 * \note Previously registered callback, if any, is replaced.
 * \note Callbacks can be unregistered by passing NULL as the fn.
 * \note vmk_SocketSetRecvBufferLoWat() can be used to throttle the
 *       rate at which the callback is called.
 * \note The callback may issue reads (e.g. vmk_SocketRecvFrom()), or
 *       may choose to defer such action, e.g. to a system world. If
 *       reads are done from the callback, then they must specify
 *       VMK_SOCKET_MSG_SOCALLBCK in the flag argument.
 * \note The callback needs to handle spurious upcalls of any sort.
 * \note Registering this callback is optional.
 *
 * \param[in]  socket  Socket whose receive buffer changed.
 * \param[in]  fn      The callback.
 * \param[in]  arg     An argument passed back to the callback.
 *
 * \retval VMK_OK             Receive callback registered successfully.
 * \retval VMK_BAD_PARAM      Bad input socket or address.
 * \retval VMK_NOT_SUPPORTED  Registering callbacks not supported for
 *                            this stack instance.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_SocketRegisterRecvBufferCallback(vmk_Socket socket,
                                     vmk_SocketCallbackFn fn,
                                     void *arg);

/*
 ***********************************************************************
 * vmk_SocketSetRecvBufferLoWat
 *
 * \ingroup Socket
 * \brief Set the receive socket buffer low watermark.
 *
 * \softwaredriversonly
 *
 * If a receive socket buffer callback has been registered
 * (vmk_SocketRegisterRecvBufferCallback(), then the low watermark
 * is a hint to the TCP stack to not invoke the callback unless the
 * socket buffer is filled to at least the low watermark.
 *
 * \note The low watermark is a hint, not a binding guarantee of no
 *       callback invocations if there are less bytes than the
 *       watermark available.
 * \note May be called from within the receive callback.
 *
 * \param[in]  socket    Socket to set receive low watermark on.
 * \param[in]  bytes     Low watermark measured in bytes.
 *
 * \retval VMK_OK             Receive low watermark set successfully.
 * \retval VMK_BAD_PARAM      Bad input socket or address.
 * \retval VMK_NOT_SUPPORTED  Setting low watermark not supported for
 *                            this stack instance.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_SocketSetRecvBufferLoWat(vmk_Socket socket, vmk_ByteCountSmall bytes);

/*
 ***********************************************************************
 * vmk_SocketSendSgVirtArrayZeroCopy --                           */ /**
 *
 * \ingroup Socket
 * \brief Send a scatter-gather VA array using zero copy.
 *
 * \softwaredriversonly
 *
 * This function is asynchronous in nature, and the underlying layers
 * utilize the same data buffers described by the sgVirtArray parameter.
 * The following behavior is implied by this function:
 *
 * 1. With a call to this function, the ownership of sgVirtArray as
 *    well as the data buffers it describes, is passed to the
 *    underlying layer. Once the data buffers are successfully
 *    transmitted, the caller receives a call to vmk_SgVirtArrayOpsFree.
 *    This signals that the ownership of SgVirtArray and the data
 *    buffers it describes is passed back to the caller.
 *
 * 2. The caller should not be modifying sgVirtArray and the data
 *    buffers it describes, while sgVirtArray is owned by the
 *    underlying layer.
 *
 * Example:
 *
 * 1. vmk_SgVirtOpsHandleCreate: creates SG VA ops handle. Must
 *    register ops.alloc and ops.free. Usually performed once for
 *    a lifetime of a socket.
 *
 * 2. For every call to this function
 *       a. txBufList: list of buffers to send (list of VAs).
 *          txBufNumElems: number of buffers in the list.
 *       b. vmk_SgVirtArrayAllocWithInit(..., txBufNumElems, ...);
 *             invokes ops.alloc and later initializes the array.
 *       c. For every txBuf in the txBufList
 *             vmk_SgVirtElemInit().
 *
 * 3. vmk_SocketSendSgVirtArrayZeroCopy().
 *
 * 4. On successful transmission, ops.free will be invoked to signal
 *    that the ownership is passed back to the caller. ops.free
 *    must free the memory allocated for sgVirtArray.
 *
 * 5. vmk_SgVirtOpsHandleDestroy: Destroys SG VA ops handle.
 *    Must be called only after all the SG VA arrays for this handle
 *    are freed i.e. there are no outstanding zero copy transmits.
 *
 * \note For blocking socket, if the peer socket closes connection,
 *       this call will return immediately.
 * \note If memory allocation fails midway, VMK_NO_BUFFERSPACE is
 *       returned to the caller, but the ownership cannot be returned
 *       immediately. Instead, the allocated memory is freed, and
 *       then the ownership is returned to the caller using
 *       vmk_SgVirtArrayOpFree callback.
 * \note vmk_SgVirtArrayOpFree callback can occur before
 *       vmk_SocketSendSgVirtArrayZeroCopy returns.
 * \note Only TCP sockets (VMK_SOCKET_IPPROTO_TCP) are currently
 *       supported.
 *
 * \param[in]  socket         Socket to send on.
 * \param[in]  address        Destination to send the sgVirtArray to.
 * \param[in]  addressLength  The length of the address info.
 * \param[in]  sgVirtArray    SG VA array for the buffer to send.
 *
 * \retval VMK_OK              Success.
 * \retval VMK_BAD_PARAM       Bad input socket or address.
 * \retval VMK_NOT_SUPPORTED   Unknown socket family.
 * \retval VMK_NO_MODULE_HEAP  The module's heap is not set.
 * \retval VMK_NO_BUFFERSPACE  Unable to allocate memory to send
 *                             the data.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_SocketSendSgVirtArrayZeroCopy(vmk_Socket socket,
                                  const vmk_SocketAddress *address,
                                  int addressLength,
                                  vmk_SgVirtArray sgVirtArray);

/*
 ***********************************************************************
 * vmk_SocketRegisterSendBufferCallback                           */ /**
 *
 * \ingroup Socket
 * \brief Registers a callback notifying of send buffer changes.
 *
 * \softwaredriversonly
 *
 * This callback will be invoked when data is sent, and the send buffer
 * space is available for enqueuing more data to send.
 *
 * \note Callbacks can be unregistered by passing NULL as the fn
 * \note vmk_SocketSetSendBufferLoWat() can be used to throttle the
 *       rate at which the callback is called.
 * \note The callback is currently not expected to do sends in the
 *       same context.
 * \note The callback needs to handle spurious upcalls of any sort.
 * \note Registering this callback is optional.
 *
 * \param[in]  socket  Socket whose send buffer changed.
 * \param[in]  fn      The callback.
 * \param[in]  arg     An argument passed back to the callback.
 *
 * \retval VMK_OK             Send callback registered successfully.
 * \retval VMK_BAD_PARAM      Bad input socket or address.
 * \retval VMK_NOT_SUPPORTED  Registering callbacks not supported for
 *                            this stack instance.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_SocketRegisterSendBufferCallback(vmk_Socket socket,
                                     vmk_SocketCallbackFn fn,
                                     void *arg);

/*
 ***********************************************************************
 * vmk_SocketSetSendBufferLoWat                                   */ /**
 *
 * \ingroup Socket
 * \brief Set the send socket buffer low watermark.
 *
 * \softwaredriversonly
 *
 * If a send socket buffer callback has been registered
 * (vmk_SocketRegisterSendBufferCallback()), then the low watermark
 * is a hint to the TCP stack to not invoke the callback unless the
 * send socket buffer has at least the watermark worth of bytes
 * available.
 *
 * \note The low watermark is a hint, not a binding guarantee of no
 *       callback invocations if there are less bytes than the
 *       watermark available.
 * \note May be called from within the send callback.
 *
 * \param[in]  socket  Socket to set send low watermark on.
 * \param[in]  bytes   Low watermark measured in bytes.
 *
 * \retval VMK_OK             Send low watermark set successfully.
 * \retval VMK_BAD_PARAM      Bad input socket or address.
 * \retval VMK_NOT_SUPPORTED  Setting low watermark not supported for
 *                            this stack instance.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_SocketSetSendBufferLoWat(vmk_Socket socket, vmk_ByteCountSmall bytes);

/*
 ***********************************************************************
 * vmk_SocketIsConnected                                          */ /**
 *
 * \brief Check if the socket is connected.
 *
 * \softwaredriversonly
 *
 * \note This function will not block.
 *
 * \param[in]  socket       Socket to check the connection status of.
 * \param[out] isConnected  VMK_TRUE if the socket is connected,
 *                          VMK_FALSE otherwise. Set to a valid value
 *                          only when the return status is VMK_OK.
 *
 * \retval VMK_OK               Successful, socket is connected or
 *                              disconnected as indicated by
 *                              the out argument isConnected.
 * \retval VMK_BUSY             Socket is in the process of connecting.
 * \retval VMK_BAD_PARAM        Input socket is invalid.
 * \retval VMK_NOT_SUPPORTED    Not supported by this stack.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_SocketIsConnected(vmk_Socket socket, vmk_Bool *isConnected);

#endif /* _VMKAPI_SOCKET_H_ */
/** @} */
