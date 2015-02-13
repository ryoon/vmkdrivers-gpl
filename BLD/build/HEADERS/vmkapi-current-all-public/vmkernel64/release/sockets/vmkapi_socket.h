/***************************************************************************
 * Copyright 2007 - 2009 VMware, Inc.  All rights reserved.
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

/**
 * \brief Opaque socket handle.
 */
typedef struct vmk_SocketInt *vmk_Socket;

/*
 * Address families
 */
/** \brief IPv4 */
#define VMK_SOCKET_AF_INET       2

/*
 * Socket types
 */
/** \brief Datagrams */
#define VMK_SOCKET_SOCK_DGRAM    2

/** \brief Raw datagrams */
#define VMK_SOCKET_SOCK_RAW      3

/*
 * Socket option levels
 */
/** \brief Operate on the socket itself */
#define VMK_SOCKET_SOL_SOCKET       0xffff

/*
 * Socket-level socket options
 */
/** \brief Bind socket to a vmknic */
#define VMK_SOCKET_SO_BINDTOVMK       0x1016

/**
 * \brief
 * Abstract socket network address
 *
 * A protocol-specific address is used in actual practice.
 */
typedef struct vmk_SocketAddress {
   vmk_uint8   sa_len;
   vmk_uint8   sa_family;
   vmk_uint8   sa_data[254];
} VMK_ATTRIBUTE_PACKED vmk_SocketAddress;

/*
 ***********************************************************************
 * vmk_SocketAddrToString --                                      */ /**
 *
 * \ingroup Socket
 * \brief Convert an address into a simple string for a particular
 *        address family.
 *
 * \note This function will not block.
 *
 * \param[in]  addr           Address to translate to a string.
 * \param[out] buffer         Buffer to place the converted string into.
 * \param[in]  bufferLength   Length of the buffer in bytes.
 *
 * \retval VMK_OK             Success. 
 * \retval VMK_BAD_PARAM      Bad input addr or buffer 
 * \retval VMK_NOT_SUPPORTED  Unknown address family. 
 *
 * \note This call does *not* do any sort of network lookup. It merely
 *       converts an address into a human-readable format. In most
 *       cases the converted string is simply a numeric string.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketAddrToString(const vmk_SocketAddress *addr,
                                        char *buffer,
                                        int bufferLength);

/*
 ***********************************************************************
 * vmk_SocketStringToAddr --                                      */ /**
 *
 * \ingroup Socket
 * \brief Convert an address into a simple string for a particular
 *        address family.
 *
 * \note This function will not block.
 *
 * \param[in]  addressFamily  Address family that the string address
 *                            will be converted into.
 * \param[in]  buffer         Buffer containing the string to convert.
 * \param[in]  bufferLength   Length of the buffer in bytes.
 * \param[out] addr           Address the string is converted into.
 *
 * \retval VMK_OK             Success. 
 * \retval VMK_BAD_PARAM      Bad input addr or buffer 
 * \retval VMK_NOT_SUPPORTED  Unknown address family. 

 * \note This call does *not* do any sort of network lookup. It merely
 *       converts a simple human-readable string into a address. In most
 *       cases, this is simply a conversion of a numeric string into
 *       a network address.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketStringToAddr(int addressFamily,
                                        const char *buffer,
                                        int bufferLength,
                                        vmk_SocketAddress *addr);

/*
 ***********************************************************************
 * vmk_SocketCreate --                                            */ /**
 *
 * \ingroup Socket
 * \brief Create a new unbound socket.
 *
 * \note This function will not block.
 *
 * \param[in]  domain      Protocol family for this socket.
 * \param[in]  type        Type of communication on this socket
 * \param[in]  protocol    Specific protocol to use from address family
 * \param[out] socket      Newly created socket
 *
 * \retval VMK_OK               Success. 
 * \retval VMK_BAD_PARAM        Bad input parameter. 
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
 * vmk_SocketClose --                                             */ /**
 *
 * \ingroup Socket
 * \brief Destroy an existing socket.
 *
 * \note This function may block if VMK_SOCKET_SO_LINGER has been set
 *       otherwise will not block.
 *
 * \param[in] socket Socket to close
 *
 * \retval VMK_OK             Success. 
 * \retval VMK_BAD_PARAM      Input socket is invalid. 
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
 * \note This function may block if flags VMK_SOCKET_MSG_DONTWAIT is not
 *       set or is not a nonblocking socket (default).
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
 * vmk_SocketGetSockName --                                       */ /**
 *
 * \ingroup Socket
 * \brief Get the socket's local endpoint network address information.
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
#endif /* _VMKAPI_SOCKET_H_ */
/** @} */
