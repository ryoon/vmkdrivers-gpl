/* **********************************************************
 * Copyright 2011 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * NetCksum                                                       */ /**
 * \addtogroup Network
 *@{
 * \defgroup NetCksum Network protocol checksum functions
 *@{
 *
 ***********************************************************************
 */
#ifndef _VMKAPI_NET_CKSUM_H_
#define _VMKAPI_NET_CKSUM_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/*
 ***********************************************************************
 * vmk_NetCsumFold --                                             */ /**
 *
 * \brief Fold a partial 32-bit checksum into a single 16-bit value.
 *
 * \param[in]   sum      Partial checksum to fold.
 *
 * \retval      The folded checksum.
 *
 ***********************************************************************
 */
static inline vmk_uint16 vmk_NetCsumFold(vmk_uint32 sum)
{
   __asm__("addl %1, %0; adcl $0xffff, %0"
           : "=r" (sum)
           : "r" (sum << 16), "0" (sum & 0xffff0000));

   return (~sum) >> 16;
}

/*
 ***********************************************************************
 * vmk_NetCsumDataPartial --                                      */ /**
 *
 * \brief Checksum a block of data.
 *
 * \param[in]   data       Data to checksum
 * \param[in]   len        Length of the data to checksum.
 * \param[in]   initialSum Initial checksum to add.
 *
 * \retval      The resulting unfolded checksum.
 *
 ***********************************************************************
 */
vmk_uint32 vmk_NetCsumDataPartial(void       *data,
                                  vmk_uint64  len,
                                  vmk_uint32  initialSum);

/*
 ***********************************************************************
 * vmk_NetCsumDataFinal --                                        */ /**
 *
 * \brief Checksum a block of data and fold the result.
 *
 * \param[in]   data       Data to checksum
 * \param[in]   len        Length of the data to checksum.
 * \param[in]   initialSum Initial checksum to add.
 *
 * \retval      The resulting checksum.
 *
 ***********************************************************************
 */
static inline vmk_uint16 vmk_NetCsumDataFinal(void       *data,
                                              vmk_uint64  len,
                                              vmk_uint32  initialSum)
{
   return vmk_NetCsumFold(vmk_NetCsumDataPartial(data, len, initialSum));
}

/*
 ***********************************************************************
 * vmk_NetCsumIPv4Pseudo --                                       */ /**
 *
 * \brief Checksum an IPv4 pseudo-header and fold the result.
 *
 * \note saddr, daddr, and proto are direct pointers to the IPv4
 *       header's content; saddr and daddr are expected to be in network
 *       byte order. On the other hand, totalLength is the total length
 *       of the upper layer(E.g: TCP or UDP) header + payload. It is
 *       expected to by in host byte order.
 *
 * \param[in]   saddr         Source address of the IPv4 packed.
 * \param[in]   daddr         Destination address of the IPv4 packed.
 * \param[in]   proto         Protocol number of the upper layer packet.
 * \param[in]   totalLength   Total length of the upper layer packet.
 * \param[in]   initialSum    Initial checksum to add.
 *
 * \retval      The resulting checksum.
 *
 ***********************************************************************
 */
vmk_uint16 vmk_NetCsumIPv4Pseudo(const vmk_uint32 *saddr,
                                 const vmk_uint32 *daddr,
                                 const vmk_uint8  *proto,
                                 vmk_uint16        totalLength,
                                 vmk_uint32        initialSum);

/*
 ***********************************************************************
 * vmk_NetCsumIPv6Pseudo --                                       */ /**
 *
 * \brief Checksum an IPv6 pseudo-header and fold the result.
 *
 * \note saddr, daddr, and proto are direct pointers to the IPv6
 *       header's content; saddr and daddr are expected to be in network
 *       byte order. On the other hand, totalLength is the total length
 *       of the upper layer(E.g: TCP or UDP) header + payload. It is
 *       expected to by in host byte order.
 *
 * \note daddr will usually point to the IPv6 header's destination
 *       address, but when a routing header is present, the last element
 *       of the routing header must be used in the IPv6 pseudo header.
 *
 * \param[in]   saddr         Source address of the IPv6 packed.
 * \param[in]   daddr         Destination address of the IPv6 packed.
 * \param[in]   proto         Protocol number of the upper layer packet.
 * \param[in]   totalLength   Total length of the upper layer packet.
 * \param[in]   initialSum    Initial checksum to add.
 *
 * \retval      The resulting checksum.
 *
 ***********************************************************************
 */
vmk_uint16 vmk_NetCsumIPv6Pseudo(const vmk_uint8  *saddr,
                                 const vmk_uint8  *daddr,
                                 const vmk_uint8  *proto,
                                 vmk_uint16        totalLength,
                                 vmk_uint32        initialSum);

#endif /* _VMKAPI_NET_CKSUM_H_ */
/** @} */
/** @} */
