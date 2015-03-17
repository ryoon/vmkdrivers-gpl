/***************************************************************************
 * Copyright 2007 - 2009 VMware, Inc.  All rights reserved.
 ***************************************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * vmkapi_socket_ip.h                                             */ /**
 * \addtogroup Socket
 *@{
 * \defgroup SocketIP IP Socket Interfaces
 *@{ 
 *
 * Data types and constants for IPv4 sockets
 *
 ***********************************************************************
 */

#ifndef _VMKAPI_SOCKET_IP_H_
#define _VMKAPI_SOCKET_IP_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/*
 * IP protocol types
 */
#define VMK_SOCKET_IPPROTO_IP    0
#define VMK_SOCKET_IPPROTO_ICMP  1
#define VMK_SOCKET_IPPROTO_TCP   6
#define VMK_SOCKET_IPPROTO_UDP   17
#define VMK_SOCKET_IPPROTO_GRE   47

/*
 * Special addresses
 */
#define VMK_SOCKET_IPINADDR_ANY       ((vmk_uint32)(0x0))
#define VMK_SOCKET_IPINADDR_BROADCAST ((vmk_uint32)(0xffffffff))

/*
 * Socket level
 */
#define VMK_SOCKET_SOL_IP    0

/*
 * Socket options
 */
#define VMK_SOCKET_OPT_IP_HDRINCL   2

/**
 * \brief Binary IPv4 address in network byte order.
 */
typedef struct vmk_SocketIPAddressAddr {
   vmk_uint32 s_addr;
} vmk_SocketIPAddressAddr;

/**
 * \brief An IPv4-style socket address.
 */
typedef struct vmk_SocketIPAddress {
   /** \brief Address length. Should be sizeof(vmk_SocketIPAddress) */
   vmk_uint8                 sin_len;

   /** \brief Address family. Should be VMK_SOCKET_AF_INET */
   vmk_uint8                 sin_family;

   /** \brief IP port in network byte order */
   vmk_uint16                sin_port;

   /** \brief Binary IP address in network byte order */
   vmk_SocketIPAddressAddr   sin_addr;

   /** \brief Padding. This area should be zeroed. */
   vmk_uint8                 sin_zero[8];
} VMK_ATTRIBUTE_PACKED vmk_SocketIPAddress;


/**
 * \brief Binary IPv6 address in network byte order.
 */
typedef struct vmk_SocketIPv6AddressAddr {
   union {
      vmk_uint8   __u6_addr8[16];
      vmk_uint16  __u6_addr16[8];
      vmk_uint32  __u6_addr32[4];
   } __u6_addr;
} VMK_ATTRIBUTE_PACKED vmk_SocketIPv6AddressAddr;


/**
 * \brief An IPv6-style socket address.
 */
typedef struct vmk_SocketIPv6Address {
   /** \brief Address length. Should be sizeof(vmk_SocketIPv6Address) */
   vmk_uint8                 sin6_len;

   /** \brief Address family. Should be VMK_SOCKET_AF_INET6 */
   vmk_uint8                 sin6_family;

   /** \brief IP port in network byte order */
   vmk_uint16                sin6_port;

   /** \brief Flow Info in network byte order */
   vmk_uint32                sin6_flowinfo;

   /** \brief Binary IP address in network byte order */
   vmk_SocketIPv6AddressAddr sin6_addr;

   /** \brief Scope ID in network byte order */
   vmk_uint32                sin6_scopeid;
} VMK_ATTRIBUTE_PACKED vmk_SocketIPv6Address;

/*
 ***********************************************************************
 * vmk_Ntohl --                                                   */ /**
 *
 * \ingroup SocketIP
 * \brief Convert a 32 bit unsigned integer in the network byte order
 *        to the host-native byte order.
 *
 * \note This function will not block.
 *
 * \param[in] x Value to reorder.
 *
 * \return Reordered value.
 *
 * \sa vmk_Htonl
 *
 ***********************************************************************
 */
static inline vmk_uint32 vmk_Ntohl(vmk_uint32 x) {
   return vmk_BE32ToCPU(x);
}

/*
 ***********************************************************************
 * vmk_Htonl --                                                   */ /**
 *
 * \ingroup SocketIP
 * \brief Convert a 32 bit unsigned integer in the host-native byte
 *        order to the network byte order.
 *
 * \note This function will not block.
 *
 * \param[in] x Value to reorder.
 *
 * \return Reordered value.
 *
 * \sa vmk_Ntohl
 *
 ***********************************************************************
 */
static inline vmk_uint32 vmk_Htonl(vmk_uint32 x) {
   return vmk_CPUToBE32(x);
}

/*
 ***********************************************************************
 * vmk_Ntohs --                                                   */ /**
 *
 * \ingroup SocketIP
 * \brief Convert a 16 bit unsigned integer in the network byte order
 *        to the host-native byte order.
 *
 * \note This function will not block.
 *
 * \param[in] x Value to reorder.
 *
 * \return Reordered value.
 *
 * \sa vmk_Htons
 *
 ***********************************************************************
 */
static inline vmk_uint16 vmk_Ntohs(vmk_uint16 x) {
   return vmk_BE16ToCPU(x);
}

/*
 ***********************************************************************
 * vmk_Htons --                                                   */ /**
 *
 * \ingroup SocketIP
 * \brief Convert a 16 bit unsigned integer in the host-native byte
 *        order to the network byte order.
 *
 * \note This function will not block.
 *
 * \param[in] x Value to reorder.
 *
 * \return Reordered value.
 *
 * \sa vmk_Ntohs
 *
 ***********************************************************************
 */
static inline vmk_uint16 vmk_Htons(vmk_uint16 x) {
   return vmk_CPUToBE16(x);
}

/** @} */
/** @} */
#endif /* _VMKAPI_SOCKET_IP_H_ */
