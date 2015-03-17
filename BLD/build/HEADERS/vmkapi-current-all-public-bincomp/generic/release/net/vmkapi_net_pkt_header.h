/* **********************************************************
 * Copyright 2012 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * PktHeader                                                      */ /**
 * \addtogroup Network
 *@{
 * \defgroup PktHeader Packet Header Utilities
 *@{
 * \par Header Information
 *
 * VMKernel provides an API framework for accessing header information
 * for a network packet represented by a vmk_PktHandle. For each header,
 * information is exported in the form of a vmk_PktHeaderEntry, which
 * contains offset and type data. Headers are parsed on-demand by the
 * underlying backend implementation, significantly simplifying packet
 * processing for users of this API framework.\n
 *\n
 * Amongst the functionality provided are API's to search for headers
 * matching a given specification using vmk_PktHeaderFind(). Through
 * the use of masks, it is possible to search for any header matching
 * a certain layer, for example VMK_PKT_HEADER_L4_MASK can be used to
 * search for any layer 4 header in the packet. Submasks are also
 * provided, for example VMK_PKT_HEADER_L3_IPv6_EXT_HDR_MASK will match
 * any IPv6 extension header. Of course, an explicit header type can
 * be specified, such as VMK_PKT_HEADER_L4_TCP, which would search
 * for a TCP header within the packet. Convenience functions such as
 * vmk_PktHeaderL2Find() etc. are also provided.\n
 *\n
 * It is also possible to iterate through headers of a given packet
 * using vmk_PktHeaderEntryGet(). Information about header lengths
 * can be obtained using API's such as vmk_PktHeaderAllHeadersLenGet().
 *
 * \par Providing Layout Information
 *
 * API's are provided in order to manipulate the header cache
 * information associated with a particular packet. These API's can
 * be used by the following sites:
 *
 * - <b>Packet producers</b>
 *\n
 *     Any code that creates and injects packets into the VMKernel can
 *     optionally choose to pre-populate the header cache information
 *     for that packet, if the information is available. This helps
 *     optimize packet processing since the packet doesn't need to
 *     be parsed again. API's such as vmk_PktHeaderEntryInsert() or
 *     vmk_PktHeaderArrayGet() and vmk_PktHeaderArraySet() can be
 *     used for this purpose.\n
 *\n
 * - <b>Header modifications</b>
 *\n
 *     Any code that modifies headers, either by changing the size of
 *     an existing header or by adding or removing headers from a
 *     packet MUST adjust the header cache to ensure that it is
 *     coherent. This can be done by invalidating the affected
 *     portions of the cache through use of API's such as
 *     vmk_PktHeaderInvalidateAll() etc. but this method is not
 *     preferred since it will cause the packet to be parsed again
 *     when another consumer of this API requests a lookup. It is
 *     recommended to use API's such as vmk_PktHeaderIncOffsets(),
 *     vmk_PktHeaderPushForEncap() and vmk_PktHeaderEntryInsert()
 *     to adjust the header cache manually for best results.
 *
 * \par Encapsulated Packets
 *
 * The parsing framework supports encapsulated headers as well.
 * vmk_PktHeaderFind() accepts a hitCount argument which can be used to
 * find the second occurance of a given header, for example the second
 * L4 header etc. vmk_PktHeaderEncapFind() can be used to find an
 * encapsulation header within the packet, provided one exists. API's
 * such as vmk_PktHeaderInnerHeadersLenGet() and
 * vmk_PktHeaderOuterHeadersLenGet() return the length of the inner or
 * outer headers in the case of an encapsulated frame.
 *
 * \par Header Access
 *
 * This framework also provides the methods vmk_PktHeaderDataGet() and
 * vmk_PktHeaderDataRelease() which are recommended ways of accessing
 * header data. These functions take care of issues such as headers
 * not being in the frame mapped area (and thus requiring mapping) or
 * even situations such as a header spanning multiple SG entries, in
 * which case the header needs to be copied to be accessed directly
 * through casting to a header structure. The functions allow read
 * access and write access as long as header size is not modified.
 *
 ***********************************************************************
 */

#ifndef _VMKAPI_NET_PKT_HEADER_H_
#define _VMKAPI_NET_PKT_HEADER_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "net/vmkapi_net_pkt.h"

/**
 * \ingroup PktHeader
 * \enum vmk_PktHeaderType
 * \brief Packet header type used by the parsing infrastructure
 */
typedef enum vmk_PktHeaderType {
   /** Layer 2 mask - matches all layer 2 headers */
   VMK_PKT_HEADER_L2_MASK                   = 0x1000,
   /** Ethernet header mask - matches all ethernet headers */
   VMK_PKT_HEADER_L2_ETHERNET_MASK          = 0x1800,
   /** Simple DIX Ethernet header */
   VMK_PKT_HEADER_L2_ETHERNET               = 0x1801,
   /** 802.1pq Ethernet header */
   VMK_PKT_HEADER_L2_ETHERNET_802_1PQ       = 0x1802,
   /** 802.3 Ethernet header */
   VMK_PKT_HEADER_L2_ETHERNET_802_3         = 0x1803,
   /** 802.3 in 802.1pq Ethernet header */
   VMK_PKT_HEADER_L2_ETHERNET_802_1PQ_802_3 = 0x1804,
   /** Fenced ethernet header */
   VMK_PKT_HEADER_L2_ETHERNET_FENCED        = 0x1805,
   /** DVFilter ethernet header */
   VMK_PKT_HEADER_L2_ETHERNET_DVFILTER      = 0x1806,

   /** Layer 3 mask - matches all layer 3 headers */
   VMK_PKT_HEADER_L3_MASK                   = 0x2000,
   /** IP version 4 header */
   VMK_PKT_HEADER_L3_IPv4                   = 0x2001,
   /** IP version 6 header */
   VMK_PKT_HEADER_L3_IPv6                   = 0x2002,
   /** Mask matching all IPv6 extension headers */
   VMK_PKT_HEADER_L3_IPv6_EXT_HDR_MASK      = 0x2800,
   /** IPv6 extension header: Hop-by-hop options */
   VMK_PKT_HEADER_L3_IPv6_EXT_HDR_HOP       = 0x2801,
   /** IPv6 extension header: Destination options */
   VMK_PKT_HEADER_L3_IPv6_EXT_HDR_DST       = 0x2802,
   /** IPv6 extension header: Routing header */
   VMK_PKT_HEADER_L3_IPv6_EXT_HDR_ROUT      = 0x2803,
   /** IPv6 extension header: Fragment header */
   VMK_PKT_HEADER_L3_IPv6_EXT_HDR_FRAG      = 0x2804,
   /** IPv6 extension header: Authentication header */
   VMK_PKT_HEADER_L3_IPv6_EXT_HDR_AH        = 0x2805,
   /** IPv6 extension header: Encapsulation Security Payload header */
   VMK_PKT_HEADER_L3_IPv6_EXT_HDR_ESP       = 0x2806,
   /** IPv6 extension header: Mobility header */
   VMK_PKT_HEADER_L3_IPv6_EXT_HDR_MOB       = 0x2807,

   /** Layer 4 mask - matches all layer 4 headers */
   VMK_PKT_HEADER_L4_MASK                   = 0x4000,
   /** Transmission Control Protocol */
   VMK_PKT_HEADER_L4_TCP                    = 0x4001,
   /** User Datagram Protocol */
   VMK_PKT_HEADER_L4_UDP                    = 0x4002,
   /** Protocol Independent Multicast */
   VMK_PKT_HEADER_L4_PIM                    = 0x4003,
   /** Mask matching ICMP version 4 and version 6 */
   VMK_PKT_HEADER_L4_ICMP_MASK              = 0x4800,
   /** Internet Control Message Protocol v4 */
   VMK_PKT_HEADER_L4_ICMPV4                 = 0x4801,
   /** Internet Control Message Protocol v6 */
   VMK_PKT_HEADER_L4_ICMPV6                 = 0x4802,

   /** Encapsulation header mask - matches all encapsulation headers */
   VMK_PKT_HEADER_ENCAP_MASK                = 0x8000,
   /** VXLAN header */
   VMK_PKT_HEADER_ENCAP_VXLAN               = 0x8001,
   /** GRE header */
   VMK_PKT_HEADER_ENCAP_GRE                 = 0x8002,
   /** Geneve header */
   VMK_PKT_HEADER_ENCAP_GENEVE              = 0x8003,

   /** Miscelleneous header mask - matches all misc headers */
   VMK_PKT_HEADER_MISC_MASK                = 0x9000,
   /** ARP header */
   VMK_PKT_HEADER_MISC_ARP                 = 0x9001,
   /** RARP header */
   VMK_PKT_HEADER_MISC_RARP                = 0x9002,
} vmk_PktHeaderType;

/** Header bits used for type masks */
#define VMK_PKT_HEADER_TYPE_MASK_BITS (0xff00)

/** Given a header, return its type mask bits */
#define VMK_PKT_HEADER_GET_TYPE_MASK(type) \
    ((type) & VMK_PKT_HEADER_TYPE_MASK_BITS)

/** Check whether the given type matches a type mask */
#define VMK_PKT_HEADER_TYPE_CHECK(type, mask) \
    (VMK_PKT_HEADER_GET_TYPE_MASK(type) == (mask))

/** Header bits used for layer masks */
#define VMK_PKT_HEADER_LAYER_MASK_BITS (0xf000)

/** Given a header, return its layer mask bits */
#define VMK_PKT_HEADER_GET_LAYER_MASK(type) \
    ((type) & VMK_PKT_HEADER_LAYER_MASK_BITS)

/** Check whether the given type matches a layer mask */
#define VMK_PKT_HEADER_LAYER_CHECK(type, mask) \
    (VMK_PKT_HEADER_GET_LAYER_MASK(type) == (mask))

/** Macro to determine whether a mask is a layer mask vs. a type mask */
#define VMK_PKT_HEADER_MASK_IS_LAYER(mask) \
   (((mask) | VMK_PKT_HEADER_LAYER_MASK_BITS) == VMK_PKT_HEADER_LAYER_MASK_BITS)

/**
 *  Check whether the given type matches with a mask. Does a different
 *  check depending on whether the mask is a layer mask vs. a type mask
 */
#define VMK_PKT_HEADER_MASK_CHECK(type, mask) \
   (VMK_PKT_HEADER_MASK_IS_LAYER(mask) ?      \
    VMK_PKT_HEADER_LAYER_CHECK(type, mask) :  \
    VMK_PKT_HEADER_TYPE_CHECK(type, mask))

/** Macro to return whether a given header type is an L2 header type */
#define VMK_PKT_HEADER_IS_L2(type) \
   VMK_PKT_HEADER_LAYER_CHECK(type, VMK_PKT_HEADER_L2_MASK)

/** Macro to return whether a given header type is an L3 header type */
#define VMK_PKT_HEADER_IS_L3(type) \
   VMK_PKT_HEADER_LAYER_CHECK(type, VMK_PKT_HEADER_L3_MASK)

/** Macro to return whether a given header type is an L4 header type */
#define VMK_PKT_HEADER_IS_L4(type) \
   VMK_PKT_HEADER_LAYER_CHECK(type, VMK_PKT_HEADER_L4_MASK)

/** Macro to return whether a given header type is an encapsulation header type */
#define VMK_PKT_HEADER_IS_ENCAP(type) \
   (VMK_PKT_HEADER_LAYER_CHECK(type, VMK_PKT_HEADER_ENCAP_MASK) || \
    ((type) == VMK_PKT_HEADER_L2_ETHERNET_FENCED))

/** Protocol is a "toplevel" protocol */
#define VMK_PKT_PROTO_TOPLEVEL         0xFFFD
/** Protocol is unknown */
#define VMK_PKT_PROTO_UNKNOWN          0xFFFE
/** There are no more headers after this header */
#define VMK_PKT_PROTO_NO_MORE_HEADERS  0xFFFF

/**
 * \ingroup PktHeader
 * \struct vmk_PktHeaderEntry
 * \brief Entry structure describing one packet header
 */
typedef struct vmk_PktHeaderEntry {
   /** vmk_PktHeaderType of this header */
   vmk_uint16    type;
   /** Offset of this header from start of frame */
   vmk_uint16    offset;
   /** Parser-dependent output defining next header's protocol
    * VMK_PKT_PROTO_NO_MORE_HEADERS is a reserved value indicating there are no
    * more headers, VMK_PKT_PROTO_UNKNOWN is a reserved value indicating that
    * the provider of this entry did not specify the next protocol. This can
    * happen if the provider is a NIC driver or vNIC backend and it doesn't know
    * the next protocol. In this case the parsing infrastructure will re-parse
    * this header if necessary to determine the next header. */
   vmk_uint16    nextHdrProto;
   /** Offset of the next header from start of frame */
   vmk_uint16    nextHdrOffset;
} vmk_PktHeaderEntry;

/*
 ***********************************************************************
 * vmk_PktHeaderEntryGet --                                       */ /**
 *
 * \ingroup PktHeader
 * \brief Get the vmk_PktHeaderEntry for the header with the specified
 *        index.
 *
 * As a side effect this API may also parse the packet and caches the
 * result if the headers are not already parsed.
 *
 *
 * \param[in]  pkt       Packet to get header information from.
 * \param[in]  hdrIndex  Index of the header to get.
 * \param[out] hdrEntry  Pointer to the header entry for the requested
 *                       header.
 *
 * \retval     VMK_OK             Operation was successful.
 * \retval     VMK_LIMIT_EXCEEDED Header index out of bounds.
 * \retval     VMK_FAILURE        Parsing failure.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PktHeaderEntryGet(vmk_PktHandle *pkt,
                                       vmk_uint16 hdrIndex,
                                       vmk_PktHeaderEntry **hdrEntry);

/*
 ***********************************************************************
 * vmk_PktHeaderLength   --                                       */ /**
 *
 * \ingroup PktHeader
 * \brief Returns the length of the header represented by the given
 *        entry.
 *
 * \param[in]  hdrEntry  Entry to return header length of.
 *
 * \return               Length of the header, 0 if not available.
 *
 ***********************************************************************
 */
static inline vmk_uint16
vmk_PktHeaderLength(vmk_PktHeaderEntry *hdrEntry)
{
   VMK_ASSERT(hdrEntry != NULL);
   return hdrEntry->nextHdrOffset - hdrEntry->offset;
}

/*
 ***********************************************************************
 * vmk_PktHeaderNumParsedGet --                                   */ /**
 *
 * \ingroup PktHeader
 * \brief Return number of parsed headers in the given packet
 *
 * \param[in]  pkt       Packet to get header information from.
 *
 * \return               Number of parsed headers in the packet.
 *
 ***********************************************************************
 */
vmk_uint16 vmk_PktHeaderNumParsedGet(vmk_PktHandle *pkt);

/*
 ***********************************************************************
 * vmk_PktHeaderFind --                                           */ /**
 *
 * \ingroup PktHeader
 * \brief Search for a header matching the given mask and return it.
 *
 * As a side effect this API may also parse the packet and caches the
 * result if the headers are not already parsed.
 *
 * \param[in]  pkt        Packet to search through.
 * \param[in]  startIndex Index to start searching from (0 = first header)
 * \param[in]  searchMask Mask to match against. This could be a generic
 *                        mask such as VMK_PKT_HEADER_L3_MASK as well as
 *                        a specific header type like
 *                        VMK_PKT_HEADER_L4_TCP.
 * \param[in]  hitCount   1 = return first matching header, 2 = second
 *                        matching header etc.
 * \param[out] hdrEntry   Pointer to the header entry for the requested
 *                        header.
 * \param[out] hdrIndex   Pointer to the index of the header for the
 *                        requested header. This is optional, pass in
 *                        NULL if this is not required.
 *
 * \retval     VMK_OK              Operation was successful.
 * \retval     VMK_NOT_FOUND       No header found matching the given
 *                                 criteria.
 * \retval     VMK_FAILURE         Parsing failure due to corrupt header.
 * \retval     VMK_NOT_IMPLEMENTED Parsing failure due to missing parser.
 * \retval     VMK_LIMIT_EXCEEDED  Parsing failure due to truncated
 *                                 header.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PktHeaderFind(vmk_PktHandle *pkt,
                                   vmk_uint16 startIndex,
                                   vmk_PktHeaderType searchMask,
                                   vmk_uint16 hitCount,
                                   vmk_PktHeaderEntry **hdrEntry,
                                   vmk_uint16 *hdrIndex);

/*
 ***********************************************************************
 * vmk_PktHeaderL2Find --                                         */ /**
 *
 * \ingroup PktHeader
 * \brief Find the first layer 2 header in the given packet.
 *
 * As a side effect this API may also parse the packet and caches the
 * result if the headers are not already parsed.
 *
 * \param[in]  pkt      Packet to search through.
 * \param[out] hdrEntry Pointer to the header entry for the first L2
 *                      header.
 * \param[out] hdrIndex Pointer to the index of the header for the
 *                      requested header. This is optional, pass in
 *                      NULL if this is not required.
 *
 * \retval     VMK_OK              Operation was successful.
 * \retval     VMK_NOT_FOUND       No header found matching the given
 *                                 criteria.
 * \retval     VMK_FAILURE         Parsing failure due to corrupt header.
 * \retval     VMK_NOT_IMPLEMENTED Parsing failure due to missing parser.
 * \retval     VMK_LIMIT_EXCEEDED  Parsing failure due to truncated
 *                                 header.
 *
 ***********************************************************************
 */
static inline VMK_ReturnStatus
vmk_PktHeaderL2Find(vmk_PktHandle *pkt,
                    vmk_PktHeaderEntry **hdrEntry,
                    vmk_uint16 *hdrIndex)
{
   return vmk_PktHeaderFind(pkt, 0, VMK_PKT_HEADER_L2_MASK, 1, hdrEntry,
                            hdrIndex);
}

/*
 ***********************************************************************
 * vmk_PktHeaderL3Find --                                         */ /**
 *
 * \ingroup PktHeader
 * \brief Find the first layer 3 header in the given packet.
 *
 * As a side effect this API may also parse the packet and caches the
 * result if the headers are not already parsed.
 *
 * \param[in]  pkt      Packet to search through.
 * \param[out] hdrEntry Pointer to the header entry for the first L3
 *                      header.
 * \param[out] hdrIndex Pointer to the index of the header for the
 *                      requested header. This is optional, pass in
 *                      NULL if this is not required.
 *
 * \retval     VMK_OK              Operation was successful.
 * \retval     VMK_NOT_FOUND       No header found matching the given
 *                                 criteria.
 * \retval     VMK_FAILURE         Parsing failure due to corrupt header.
 * \retval     VMK_NOT_IMPLEMENTED Parsing failure due to missing parser.
 * \retval     VMK_LIMIT_EXCEEDED  Parsing failure due to truncated
 *                                 header.
 *
 ***********************************************************************
 */
static inline VMK_ReturnStatus
vmk_PktHeaderL3Find(vmk_PktHandle *pkt,
                    vmk_PktHeaderEntry **hdrEntry,
                    vmk_uint16 *hdrIndex)
{
   return vmk_PktHeaderFind(pkt, 1, VMK_PKT_HEADER_L3_MASK, 1, hdrEntry,
                            hdrIndex);
}

/*
 ***********************************************************************
 * vmk_PktHeaderL4Find --                                         */ /**
 *
 * \ingroup PktHeader
 * \brief Find the first layer 4 header in the given packet.
 *
 * As a side effect this API may also parse the packet and caches the
 * result if the headers are not already parsed.
 *
 * \param[in]  pkt      Packet to search through.
 * \param[out] hdrEntry Pointer to the header entry for the first L4
 *                      header.
 * \param[out] hdrIndex Pointer to the index of the header for the
 *                      requested header. This is optional, pass in
 *                      NULL if this is not required.
 *
 * \retval     VMK_OK              Operation was successful.
 * \retval     VMK_NOT_FOUND       No header found matching the given
 *                                 criteria.
 * \retval     VMK_FAILURE         Parsing failure due to corrupt header.
 * \retval     VMK_NOT_IMPLEMENTED Parsing failure due to missing parser.
 * \retval     VMK_LIMIT_EXCEEDED  Parsing failure due to truncated
 *                                 header.
 *
 ***********************************************************************
 */
static inline VMK_ReturnStatus
vmk_PktHeaderL4Find(vmk_PktHandle *pkt,
                    vmk_PktHeaderEntry **hdrEntry,
                    vmk_uint16 *hdrIndex)
{
   return vmk_PktHeaderFind(pkt, 2, VMK_PKT_HEADER_L4_MASK, 1, hdrEntry,
                            hdrIndex);
}

/*
 ***********************************************************************
 * vmk_PktHeaderAllHeadersLenGet --                               */ /**
 *
 * \ingroup PktHeader
 * \brief Returns the length of all headers of the given packet
 *
 * As a side effect this API may also parse the packet and caches the
 * result if the headers are not already parsed.
 *
 * \param[in]  pkt       Packet to return header length for.
 * \param[out] hdrLength Pointer for returning header length
 *
 * \retval     VMK_OK        Operation was successful.
 * \retval     VMK_FAILURE   Parsing failure.
 *
 ***********************************************************************
 */

VMK_ReturnStatus
vmk_PktHeaderAllHeadersLenGet(vmk_PktHandle *pkt,
                              vmk_uint32 *hdrLength);

/*
 ***********************************************************************
 * vmk_PktHeaderInnerHeadersLenGet --                             */ /**
 *
 * \ingroup PktHeader
 * \brief Returns the total header length of the inner frame in a given
 *        packet.
 *
 * As a side effect this API may also parse the packet and caches the
 * result if the headers are not already parsed.
 *
 * \note The inner frame is defined as a frame that is contained within
 *       another frame. Inner headers are all headers that come after
 *       an encapsulation header in the following list:
 *       - VMK_PKT_HEADER_L2_ETHERNET_FENCED
 *       - VMK_PKT_HEADER_ENCAP_VXLAN
 *       - VMK_PKT_HEADER_ENCAP_GRE
 *
 * \param[in]  pkt       Packet to return header length for.
 * \param[out] hdrLength Pointer for returning header length
 *
 * \retval     VMK_OK        Operation was successful.
 * \retval     VMK_FAILURE   Parsing failure.
 *
 ***********************************************************************
 */

VMK_ReturnStatus
vmk_PktHeaderInnerHeadersLenGet(vmk_PktHandle *pkt,
                                vmk_uint32 *hdrLength);

/*
 ***********************************************************************
 * vmk_PktHeaderOuterHeadersLenGet --                             */ /**
 *
 * \ingroup PktHeader
 * \brief Returns the total header length of the outer frame in a given
 *        packet.
 *
 * As a side effect this API may also parse the packet and caches the
 * result if the headers are not already parsed.
 *
 * \note The outer frame is defined as a frame that contains another
 *       frame within itself. Outer headers are all headers that come
 *       before an encapsulation header from the following list:
 *       - VMK_PKT_HEADER_L2_ETHERNET_FENCED
 *       - VMK_PKT_HEADER_ENCAP_VXLAN
 *       - VMK_PKT_HEADER_ENCAP_GRE
 *
 * \note The length of the encapsulation header is also part of the
 *       hdrLength returned.
 *
 * \param[in]  pkt       Packet to return header length for.
 * \param[out] hdrLength Pointer for returning header length
 *
 * \retval     VMK_OK        Operation was successful.
 * \retval     VMK_FAILURE   Parsing failure.
 *
 ***********************************************************************
 */

VMK_ReturnStatus
vmk_PktHeaderOuterHeadersLenGet(vmk_PktHandle *pkt,
                                vmk_uint32 *hdrLength);

/*
 ***********************************************************************
 * vmk_PktHeaderEncapFind --                                      */ /**
 *
 * \ingroup PktHeader
 * \brief Find the first encapsulation header in the given packet.
 *
 * As a side effect this API may also parse the packet and caches the
 * result if the headers are not already parsed.
 *
 * \note This function will also find fence headers designated with
 *       VMK_PKT_HEADER_L2_ETHERNET_FENCED
 *
 * \param[in]  pkt      Packet to search through.
 * \param[out] hdrEntry Pointer to the header entry for the first
 *                      encapsulation header.
 * \param[out] hdrIndex Pointer to the index of the header for the
 *                      requested header. This is optional, pass in
 *                      NULL if this is not required.
 *
 * \retval     VMK_OK              Operation was successful.
 * \retval     VMK_NOT_FOUND       No header found matching the given
 *                                 criteria.
 * \retval     VMK_FAILURE         Parsing failure due to corrupt header.
 * \retval     VMK_NOT_IMPLEMENTED Parsing failure due to missing parser.
 * \retval     VMK_LIMIT_EXCEEDED  Parsing failure due to truncated
 *                                 header.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_PktHeaderEncapFind(vmk_PktHandle *pkt,
                       vmk_PktHeaderEntry **hdrEntry,
                       vmk_uint16 *hdrIndex);

/*
 ***********************************************************************
 * vmk_PktHeaderEncapL2Find --                                    */ /**
 *
 * \ingroup PktHeader
 * \brief Find the first layer 2 header in the encapsulated (inner)
 *        frame of the given packet.
 *
 * As a side effect this API may also parse the packet and caches the
 * result if the headers are not already parsed.
 *
 * \param[in]  pkt      Packet to search through.
 * \param[out] hdrEntry Pointer to the header entry for the encapsulated
 *                      L2 header.
 * \param[out] hdrIndex Pointer to the index of the header for the
 *                      requested header. This is optional, pass in
 *                      NULL if this is not required.
 *
 * \retval     VMK_OK              Operation was successful.
 * \retval     VMK_NOT_FOUND       No header found matching the given
 *                                 criteria.
 * \retval     VMK_FAILURE         Parsing failure due to corrupt header.
 * \retval     VMK_NOT_IMPLEMENTED Parsing failure due to missing parser.
 * \retval     VMK_LIMIT_EXCEEDED  Parsing failure due to truncated
 *                                 header.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_PktHeaderEncapL2Find(vmk_PktHandle *pkt,
                         vmk_PktHeaderEntry **hdrEntry,
                         vmk_uint16 *hdrIndex);

/*
 ***********************************************************************
 * vmk_PktHeaderEncapL3Find --                                    */ /**
 *
 * \ingroup PktHeader
 * \brief Find the first layer 3 header in the encapsulated (inner)
 *        frame of the given packet.
 *
 * As a side effect this API may also parse the packet and caches the
 * result if the headers are not already parsed.
 *
 * \param[in]  pkt      Packet to search through.
 * \param[out] hdrEntry Pointer to the header entry for the encapsulated
 *                      L3 header.
 * \param[out] hdrIndex Pointer to the index of the header for the
 *                      requested header. This is optional, pass in
 *                      NULL if this is not required.
 *
 * \retval     VMK_OK              Operation was successful.
 * \retval     VMK_NOT_FOUND       No header found matching the given
 *                                 criteria.
 * \retval     VMK_FAILURE         Parsing failure due to corrupt header.
 * \retval     VMK_NOT_IMPLEMENTED Parsing failure due to missing parser.
 * \retval     VMK_LIMIT_EXCEEDED  Parsing failure due to truncated
 *                                 header.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_PktHeaderEncapL3Find(vmk_PktHandle *pkt,
                         vmk_PktHeaderEntry **hdrEntry,
                         vmk_uint16 *hdrIndex);

/*
 ***********************************************************************
 * vmk_PktHeaderEncapL4Find --                                    */ /**
 *
 * \ingroup PktHeader
 * \brief Find the first layer 4 header in the encapsulated (inner)
 *        frame of the given packet.
 *
 * As a side effect this API may also parse the packet and caches the
 * result if the headers are not already parsed.
 *
 * \param[in]  pkt      Packet to search through.
 * \param[out] hdrEntry Pointer to the header entry for the encapsulated
 *                      L4 header.
 * \param[out] hdrIndex Pointer to the index of the header for the
 *                      requested header. This is optional, pass in
 *                      NULL if this is not required.
 *
 * \retval     VMK_OK              Operation was successful.
 * \retval     VMK_NOT_FOUND       No header found matching the given
 *                                 criteria.
 * \retval     VMK_FAILURE         Parsing failure due to corrupt header.
 * \retval     VMK_NOT_IMPLEMENTED Parsing failure due to missing parser.
 * \retval     VMK_LIMIT_EXCEEDED  Parsing failure due to truncated
 *                                 header.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_PktHeaderEncapL4Find(vmk_PktHandle *pkt,
                         vmk_PktHeaderEntry **hdrEntry,
                         vmk_uint16 *hdrIndex);

/*
 ***********************************************************************
 * vmk_PktHeaderInvalidateIndex --                                */ /**
 *
 * \ingroup PktHeader
 * \brief Invalidate all parsed headers starting from a given index.
 *
 * This function (or a more general version) needs to be called by code
 * which modifies frame headers on a packet.
 *
 * \param[in]  pkt      Packet to invalidate headers of.
 * \param[in]  hdrIndex Index to start invalidating from (0 = all headers)
 *
 * \retval     VMK_BAD_PARAM hdrIndex out of bounds.
 * \retval     VMK_OK        Operation was successful.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PktHeaderInvalidateIndex(vmk_PktHandle *pkt,
                                              vmk_uint16 hdrIndex);

/*
 ***********************************************************************
 * vmk_PktHeaderInvalidateAll --                                  */ /**
 *
 * \ingroup PktHeader
 * \brief Invalidate all parsed headers on a packet.
 *
 * This function (or a more granular version) needs to be called by code
 * which modifies frame headers on a packet.
 *
 * \param[in]  pkt      Packet to invalidate headers of.
 *
 * \retval     VMK_OK   Operation was successful.
 *
 ***********************************************************************
 */
static inline VMK_ReturnStatus
vmk_PktHeaderInvalidateAll(vmk_PktHandle *pkt)
{
   return vmk_PktHeaderInvalidateIndex(pkt, 0);
}

/*
 ***********************************************************************
 * vmk_PktHeaderInvalidateOffset --                               */ /**
 *
 * \ingroup PktHeader
 * \brief Invalidate all parsed headers starting from a given byte
 *        offset.
 *
 * This function (or a more general version) needs to be called by code
 * which modifies frame headers on a packet.
 *
 * \param[in]  pkt      Packet to invalidate headers of.
 * \param[in]  offset   Byte offset to start invalidating from.
 *
 * \retval     VMK_OK   Operation was successful.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PktHeaderInvalidateOffset(vmk_PktHandle *pkt,
                                               vmk_uint16 offset);

/*
 ***********************************************************************
 * vmk_PktHeaderIncOffsets --                                     */ /**
 *
 * \ingroup PktHeader
 * \brief Increment all header offsets by the specified amount starting
 *        from the given header.
 *
 * Note that for the first header (ie. the one at hdrIndex) only the
 * nextHdrOffset is incremented by offset. For all subsequent headers,
 * both offset and nextHdrOffset are incremented.
 *
 * This function (or a more general version) needs to be called by code
 * which modifies frame headers on a packet.
 *
 * \param[in]  pkt      Packet to increment header offsets of.
 * \param[in]  hdrIndex Index of header to start incrementing offsets
 *                      from.
 * \param[in]  offset   Number of bytes to increment offsets by.
 *
 * \retval     VMK_OK        Operation was successful.
 * \retval     VMK_BAD_PARAM hdrIndex out of bounds.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PktHeaderIncOffsets(vmk_PktHandle *pkt,
                                         vmk_uint16 hdrIndex,
                                         vmk_uint16 offset);

/*
 ***********************************************************************
 * vmk_PktHeaderPushForEncap --                                  */ /**
 *
 * \ingroup PktHeader
 * \brief Push all the parsed headers in the packet by the specified
 *        amounts in preparation for prepending encapsulation headers.
 *
 * All headers which are already parsed are shifted by numHeaders slots
 * forward and their offsets are incremented by encapLen. If necessary
 * the header cache array may be reallocated to accomodate the new
 * headers.
 *
 * \param[in]  pkt        Packet to adjust header cache for.
 * \param[in]  numHeaders Number of encapsulation headers that will be
 *                        prepended to the packet.
 * \param[in]  encapLen   Total length of the encapsulation headers.
 *
 * \retval     VMK_OK        Operation was successful.
 * \retval     VMK_NO_MEMORY Couldn't grow packet header cache.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PktHeaderPushForEncap(vmk_PktHandle *pkt,
                                           vmk_uint16 numHeaders,
                                           vmk_uint16 encapLen);

/*
 ***********************************************************************
 * vmk_PktHeaderPullForDecap --                                  */ /**
 *
 * \ingroup PktHeader
 * \brief Pull all the parsed headers in the packet by the specified
 *        amounts after decapsulation of headers.
 *
 * All headers which are already parsed are shifted by numHeaders slots
 * back and their offsets are decremented by decapLen.
 *
 * \param[in]  pkt        Packet to adjust header cache for.
 * \param[in]  numHeaders Number of encapsulation headers that were
 *                        removed from the packet.
 * \param[in]  decapLen   Total length of the encapsulation headers.
 *
 * \retval     VMK_OK     Operation was successful.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PktHeaderPullForDecap(vmk_PktHandle *pkt,
                                           vmk_uint16 numHeaders,
                                           vmk_uint16 decapLen);

/*
 ***********************************************************************
 * vmk_PktHeaderDataGet --                                        */ /**
 *
 * \ingroup PktHeader
 * \brief Returns a pointer to the specified header, mapping or copying
 *        the header if needed.
 *
 * \note Any modifications of header data requires a corresponding
 *       vmk_PktHeaderInvalidate* call to invalidate header parsing
 *       results.
 *
 * \param[in]  pkt       Packet to get header data from.
 * \param[in]  entry     Header entry to get a pointer to.
 * \param[out] mappedPtr Mapped pointer to requested header data.
 *
 * \retval     VMK_OK        Operation was successful.
 * \retval     VMK_FAILURE   Mapping or copying failed.
 * \retval     VMK_NO_MEMORY Failed to allocate memory for header copy.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PktHeaderDataGet(vmk_PktHandle *pkt,
                                      vmk_PktHeaderEntry *entry,
                                      void **mappedPtr);

/*
 ***********************************************************************
 * vmk_PktHeaderDataRelease --                                    */ /**
 *
 * \ingroup PktHeader
 * \brief Releases a mapping obtained with vmk_PktHeaderDataGet()
 *
 * \param[in]  pkt       Packet mappedPtr refers to.
 * \param[in]  entry     Header entry mappedPtr refers to.
 * \param[in]  mappedPtr Mapped pointer to release.
 * \param[in]  modified  Whether the header was modified.
 *
 * \note In order to modify the header vmk_PktIsBufDescWritable() must
 *       be true for the packet. If a modification was performed then
 *       the modified parameter needs to be set to VMK_TRUE since the
 *       underlying infrastructure may need to copy header contents
 *       from a temporary buffer back into the original frame buffers.
 *
 * \note This API doesn't support modification of header length, so
 *       any header modifications must preserve the original header
 *       length since only the original header is guaranteed to be
 *       mapped.
 *
 * \retval     VMK_OK        Operation was successful.
 * \retval     VMK_READ_ONLY modified set to TRUE for a packet which has
 *                           vmk_PktIsBufDescWritable() as VMK_FALSE
 * \retval     VMK_FAILURE   Failed to copy modified header contents
 *                           back into frame buffers.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PktHeaderDataRelease(vmk_PktHandle *pkt,
                                          vmk_PktHeaderEntry *entry,
                                          void *mappedPtr,
                                          vmk_Bool modified);

/*
 ***********************************************************************
 * vmk_PktHeaderEntryInsert --                                    */ /**
 *
 * \ingroup PktHeader
 * \brief Insert a new header entry to the packet at the specified header
 *        index.
 *
 * This function is intended for use by NIC drivers or other packet
 * producing code paths that have pre-existing knowledge of the header
 * layout for the packet, and thus would like to fill the information
 * in for the benefit of the rest of the stack.
 *
 * \note All headers after hdrIndex are modified to reflect the offset
 *       changes due to insertion of an additional header.
 *
 * \note The caller must take care to provide correct values for ALL
 *       hdrEntry fields including nextHdrProto. Failure to do so may
 *       result in an unparsable packet.
 *
 * \param[in] pkt        Packet to add a header entry to.
 * \param[in] hdrEntry   Header entry to add.
 * \param[in] hdrIndex   Index to add the header entry at (0 = first).
 *
 * \retval    VMK_OK             Operation successful.
 * \retval    VMK_NO_MEMORY      Not enough memory to grow header array
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PktHeaderEntryInsert(vmk_PktHandle *pkt,
                                          vmk_PktHeaderEntry *hdrEntry,
                                          vmk_uint16 hdrIndex);

/*
 ***********************************************************************
 * vmk_PktHeaderEntryRemove --                                    */ /**
 *
 * \ingroup PktHeader
 * \brief Removes a header entry from the packet at the specified header
 *        index.
 *
 * This function is intended for use by NIC drivers or other packet
 * producing code paths that have pre-existing knowledge of the header
 * layout for the packet, and thus would like to fill the information
 * in for the benefit of the rest of the stack.
 *
 * \note All headers after hdrIndex are pulled back one slot and modified
 *       to reflect the offset changes due to removal of the header.
 *
 * \note If the header being removed is not at index 0, the caller has
 *       to take care of fixing the nextHdrProto field of the header
 *       at (hdrIndex - 1) to properly describe the protocol for the
 *       header at (hdrIndex + 1) before the header at hdrIndex was
 *       removed.
 *
 * \param[in] pkt        Packet to remove header entry from.
 * \param[in] hdrIndex   Index to remove the header entry from (0 = first).
 *
 * \retval    VMK_OK             Operation successful.
 * \retval    VMK_LIMIT_EXCEEDED hdrIndex out of bounds, only currently
 *                               parsed headers can be removed.
 *                               \see vmk_PktHeaderNumParsedGet()
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PktHeaderEntryRemove(vmk_PktHandle *pkt,
                                          vmk_uint16 hdrIndex);

#endif
/** @} */
/** @} */
