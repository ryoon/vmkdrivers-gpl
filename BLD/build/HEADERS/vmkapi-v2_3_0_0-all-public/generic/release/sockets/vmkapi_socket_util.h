/***************************************************************************
 * Copyright 2014 VMware, Inc.  All rights reserved.
 ***************************************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * vmkapi_socket_util.h                                           */ /**
 * \addtogroup Socket
 * @{
 * \defgroup SocketUtil Socket Utility Interfaces
 * @{
 *
 * Utility interfaces for sockets
 *
 ***********************************************************************
 */

#ifndef _VMKAPI_SOCKET_UTIL_H_
#define _VMKAPI_SOCKET_UTIL_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/**
 * Maximum buffer size needed to pass to vmk_SocketAddrToString() for
 * a vmk_SocketAddress of type VMK_SOCKET_AF_INET.
 */
#define VMK_SOCKET_AF_INET_STRLEN  16

/**
 * Maximum buffer size needed to pass to vmk_SocketAddrToString() for
 * a vmk_SocketAddress of type VMK_SOCKET_AF_INET6.
 */
#define VMK_SOCKET_AF_INET6_STRLEN 46

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
 * \brief Convert a string into an address.
 *
 * \note This call does \em not do any sort of network lookup. It merely
 *       converts an address into a human-readable format. In most
 *       cases the converted string is simply a numeric string.
 *
 * \note This function will not block.
 *
 * \param[in]  addr           Address to translate to a string.
 * \param[out] buffer         Buffer to place the converted string into.
 * \param[in]  bufferLength   Length of the buffer in bytes.
 *
 * \retval VMK_OK             Success.
 * \retval VMK_BAD_PARAM      Bad input address or buffer.
 * \retval VMK_NOT_SUPPORTED  Unknown address family.
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
 * \brief Convert an address into a simple string.
 *
 * \note This call does \em not do any sort of network lookup. It merely
 *       converts a simple human-readable string into a address. In most
 *       cases, this is simply a conversion of a numeric string into
 *       a network address.
 *
 * \note This function will not block.
 *
 * \param[in]     addressFamily  Address family that the string address
 *                               will be converted into.
 * \param[in]     buffer         Buffer containing the string to convert.
 * \param[in]     bufferLength   Length of the buffer in bytes.
 * \param[in,out] addr           Address the string is converted into.
 *
 * \retval VMK_OK             Success.
 * \retval VMK_BAD_PARAM      Bad input address or buffer.
 * \retval VMK_NOT_SUPPORTED  Unknown address family.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketStringToAddr(int addressFamily,
                                        const char *buffer,
                                        int bufferLength,
                                        vmk_SocketAddress *addr);

/** @} */
/** @} */
#endif /* _VMKAPI_SOCKET_UTIL_H_ */
