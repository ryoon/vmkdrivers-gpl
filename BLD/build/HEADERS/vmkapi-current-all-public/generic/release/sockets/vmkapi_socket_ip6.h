/***************************************************************************
 * Copyright 2008 - 2009 VMware, Inc.  All rights reserved.
 ***************************************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * IPv6 Socket Interfaces                                         */ /**
 * \addtogroup Socket
 * @{
 *
 * \defgroup SocketIP6 IPv6 Socket Interfaces
 * @{ 
 *
 * Data types and constants for IPv6 sockets
 *
 ***********************************************************************
 */

#ifndef _VMKAPI_SOCKET_IP6_H_
#define _VMKAPI_SOCKET_IP6_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "sockets/vmkapi_socket.h"
#include "sockets/vmkapi_socket_ip.h"

/**
 * \brief Binary IPv6 address in network byte order.
 */
typedef union vmk_SocketIP6AddressAddr {
   vmk_uint8   vs6_addr[16];
   vmk_uint8   vs6_addr8[16];
   vmk_uint16  vs6_addr16[8];
   vmk_uint32  vs6_addr32[4];
} vmk_SocketIP6AddressAddr;

/*
 * Special addresses
 */
/** \brief Any IPv6 address */
extern const vmk_SocketIP6AddressAddr vmk_SocketIP6In6AddrAny;

/** \brief Loopback IPv6 address */
extern const vmk_SocketIP6AddressAddr vmk_SocketIP6In6AddrLoopback;

/**
 * \brief
 * An IPv4-style socket address
 */
typedef struct vmk_SocketIP6Address {
   /** \brief Address length. Should be sizeof(vmk_SocketIP6Address) */
   vmk_uint8                  sin6_len;
   
   /** \brief Address family. Should be VMK_SOCKET_AF_INET6 */
   vmk_uint8                  sin6_family;
   
   /** \brief IPv6 port in network byte order */
   vmk_uint16                 sin6_port;
   
   /** \brief IPv6 flow information */
   vmk_uint32                 sin6_flowinfo;
   
   /** \brief Binary IPv6 address in network byte order */
   vmk_SocketIP6AddressAddr   sin6_addr;

   /** \brief Binary IP address in network byte order */
   vmk_uint32                 sin6_scope_id;
} VMK_ATTRIBUTE_PACKED vmk_SocketIP6Address;

/*
 ***********************************************************************
 * vmk_SocketIP6ToIP4 --                                          */ /**
 *
 * \ingroup SocketIP6
 * \brief Convert an IPv6 address into an IPv4 address, if possible.
 *
 * \note This function will not block.
 *
 * \param[in]  src      IPv6 address to convert.
 * \param[out] dst      Converted IPv4 address.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketIP6ToIP4(const vmk_SocketIP6Address *src,
                                    vmk_SocketIPAddress *dst);

/*
 ***********************************************************************
 * vmk_SocketIP6ToIP4 --                                          */ /**
 *
 * \ingroup SocketIP6
 * \brief Convert an IPv4 address into a IPv4 mapped IPv6 address.
 *
 * \note This function will not block.
 *
 * \param[in]  src      IPv4 address to convert.
 * \param[out] dst      Converted IPv6 address.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketIP4ToIP6(const vmk_SocketIPAddress *src,
                                    vmk_SocketIP6Address *dst);

/*
 ***********************************************************************
 * vmk_SocketIP6IsAddrUnspecified --                              */ /**
 *
 * \ingroup SocketIP6
 * \brief Determine if an address is unspecified.
 *
 * \note This function will not block.
 *
 * \param[in]  addr     IPv6 Address to check.
 * \param[out] result   VMK_TRUE if address is unspecified.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketIP6IsAddrUnspecified(const vmk_SocketIP6AddressAddr
                                                *addr, vmk_Bool *result);

/*
 ***********************************************************************
 * vmk_SocketIP6IsAddrLoopback --                                 */ /**
 *
 * \ingroup SocketIP6
 * \brief Determine if an address is a loopback address.
 *
 * \note This function will not block.
 *
 * \param[in]  addr     IPv6 Address to check.
 * \param[out] result   VMK_TRUE if address is a loopback address.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketIP6IsAddrLoopback(const vmk_SocketIP6AddressAddr
                                             *addr, vmk_Bool *result);

/*
 ***********************************************************************
 * vmk_SocketIP6IsAddrV4Mapped --                                 */ /**
 *
 * \ingroup SocketIP6
 * \brief Determine if an address a mapped IPv4 Address.
 *
 * \note This function will not block.
 *
 * \param[in]  addr     IPv6 Address to check.
 * \param[out] result   VMK_TRUE if address is IPv4 compatible.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketIP6IsAddrV4Mapped(const vmk_SocketIP6AddressAddr
                                             *addr, vmk_Bool *result);

/*
 ***********************************************************************
 * vmk_SocketIP6IsAddrV4Compat --                                 */ /**
 *
 * \ingroup SocketIP6
 * \brief Determine if an address is IPv4 compatible.
 *
 * \note This function will not block.
 *
 * \param[in]  addr     IPv6 Address to check.
 * \param[out] result   VMK_TRUE if address is IPv4 compatible.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketIP6IsAddrV4Compat(const vmk_SocketIP6AddressAddr
                                             *addr, vmk_Bool *result);

/*
 ***********************************************************************
 * vmk_SocketIP6IsAddrLinkLocal --                                */ /**
 *
 * \ingroup SocketIP6
 * \brief Determine if an address is local to the link.
 *
 * \note This function will not block.
 *
 * \param[in]  addr     IPv6 Address to check.
 * \param[out] result   VMK_TRUE if address is local to the link.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketIP6IsAddrLinkLocal(const vmk_SocketIP6AddressAddr
                                              *addr, vmk_Bool *result);

/*
 ***********************************************************************
 * vmk_SocketIP6IsAddrSiteLocal --                                */ /**
 *
 * \ingroup SocketIP6
 * \brief Determine if an address is local to the site.
 *
 * \note This function will not block.
 *
 * \param[in]  addr     IPv6 Address to check.
 * \param[out] result   VMK_TRUE if address is local to the link.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketIP6IsAddrSiteLocal(const vmk_SocketIP6AddressAddr
                                              *addr, vmk_Bool *result);

/*
 ***********************************************************************
 * vmk_SocketIP6IsAddrMulticast --                                */ /**
 *
 * \ingroup SocketIP6
 * \brief Determine if an address is a multicast address.
 *
 * \note This function will not block.
 *
 * \param[in]  addr     IPv6 Address to check.
 * \param[out] result   VMK_TRUE if address is a multicast address
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketIP6IsAddrMulticast(const vmk_SocketIP6AddressAddr
                                              *addr, vmk_Bool *result);

/*
 ***********************************************************************
 * vmk_SocketIP6IsAddrMcastNodeLocal --                           */ /**
 *
 * \ingroup SocketIP6
 * \brief Determine if an address is a node-local multicast address.
 *
 * \note This function will not block.
 *
 * \param[in]  addr     IPv6 Address to check.
 * \param[out] result   VMK_TRUE if address is a node-local multicast
 *                      address.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketIP6IsAddrMcastNodeLocal(const
                                                   vmk_SocketIP6AddressAddr
                                                   *addr, vmk_Bool *result);

/*
 ***********************************************************************
 * vmk_SocketIP6IsAddrMcastIfaceLocal --                          */ /**
 *
 * \ingroup SocketIP6
 * \brief Determine if an address is an interface-local multicast
 *        address.
 *
 * \note This function will not block.
 *
 * \param[in]  addr     IPv6 Address to check.
 * \param[out] result   VMK_TRUE if address is a interface-local multicast
 *                      address.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketIP6IsAddrMcastIfaceLocal(const
                                                    vmk_SocketIP6AddressAddr
                                                    *addr, vmk_Bool *result);

/*
 ***********************************************************************
 * vmk_SocketIP6IsAddrMcastLinkLocal --                           */ /**
 *
 * \ingroup SocketIP6
 * \brief Determine if an address is a link-local multicast
 *        address.
 *
 * \note This function will not block.
 *
 * \param[in]  addr     IPv6 Address to check.
 * \param[out] result   VMK_TRUE if address is a link-local multicast
 *                      address.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketIP6IsAddrMcastLinkLocal(const
                                                   vmk_SocketIP6AddressAddr
                                                   *addr, vmk_Bool *result);

/*
 ***********************************************************************
 * vmk_SocketIP6IsAddrMcastSiteLocal --                           */ /**
 *
 * \ingroup SocketIP6
 * \brief Determine if an address is a site-local multicast
 *        address.
 *
 * \note This function will not block.
 *
 * \param[in]  addr     IPv6 Address to check.
 * \param[out] result   VMK_TRUE if address is a site-local multicast
 *                      address.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketIP6IsAddrMcastSiteLocal(const
                                                   vmk_SocketIP6AddressAddr
                                                   *addr, vmk_Bool *result);

/*
 ***********************************************************************
 * vmk_SocketIP6IsAddrMcastOrgLocal --                            */ /**
 *
 * \ingroup SocketIP6
 * \brief Determine if an address is an organization-local multicast
 *        address.
 *
 * \note This function will not block.
 *
 * \param[in]  addr     IPv6 Address to check.
 * \param[out] result   VMK_TRUE if address is an organziation-local
 *                      multicast address.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketIP6IsAddrMcastOrgLocal(const
                                                  vmk_SocketIP6AddressAddr
                                                  *addr, vmk_Bool *result);

/*
 ***********************************************************************
 * vmk_SocketIP6IsAddrMcastGlobal --                              */ /**
 *
 * \ingroup SocketIP6
 * \brief Determine if an address is a global multicast address.
 *
 * \note This function will not block.
 *
 * \param[in]  addr     IPv6 Address to check.
 * \param[out] result   VMK_TRUE if address is a global multicast
 *                      address.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SocketIP6IsAddrMcastGlobal(const
                                                vmk_SocketIP6AddressAddr
                                                *addr, vmk_Bool *result);

#endif /* _VMKAPI_SOCKET_IP6_H_ */
/** @} */
/** @} */

