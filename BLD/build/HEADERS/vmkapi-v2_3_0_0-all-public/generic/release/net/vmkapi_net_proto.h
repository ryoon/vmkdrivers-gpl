/* **********************************************************
 * Copyright 2011 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * NetProto                                                       */ /**
 * \addtogroup Network
 *@{
 * \defgroup NetProto Network protocol header definitions
 *@{
 *
 ***********************************************************************
 */
#ifndef _VMKAPI_NET_PROTO_H_
#define _VMKAPI_NET_PROTO_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */


#define  VMK_ETH_ADDR_FMT_STR             "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx"
#define  VMK_ETH_ADDR_FMT_ARGS(addrPtr)   ((vmk_uint8 *) (addrPtr))[0],\
                                          ((vmk_uint8 *) (addrPtr))[1],\
                                          ((vmk_uint8 *) (addrPtr))[2],\
                                          ((vmk_uint8 *) (addrPtr))[3],\
                                          ((vmk_uint8 *) (addrPtr))[4],\
                                          ((vmk_uint8 *) (addrPtr))[5]
#define  VMK_ETH_PTR_ADDR_FMT_ARGS(addrPtr)  &((vmk_uint8 *) (addrPtr))[0],\
                                             &((vmk_uint8 *) (addrPtr))[1],\
                                             &((vmk_uint8 *) (addrPtr))[2],\
                                             &((vmk_uint8 *) (addrPtr))[3],\
                                             &((vmk_uint8 *) (addrPtr))[4],\
                                             &((vmk_uint8 *) (addrPtr))[5]

#define  VMK_IPV4_ADDR_FMT_STR            "%u.%u.%u.%u"
#define  VMK_IPV4_ADDR_FMT_STR_ARGS(val)  ((vmk_BE32ToCPU((vmk_uint32) (val)) >> 24) & 0xFF),\
                                          ((vmk_BE32ToCPU((vmk_uint32) (val)) >> 16) & 0xFF),\
                                          ((vmk_BE32ToCPU((vmk_uint32) (val)) >> 8) & 0xFF),\
                                          (vmk_BE32ToCPU((vmk_uint32) (val)) & 0xFF)

#define  VMK_IPV6_ADDR_FMT_STR               "%08x:%08x:%08x:%08x"
#define  VMK_IPV6_ADDR_FMT_STR_ARGS(addrPtr) vmk_BE32ToCPU(((vmk_uint32 *) (addrPtr))[0]),\
                                             vmk_BE32ToCPU(((vmk_uint32 *) (addrPtr))[1]),\
                                             vmk_BE32ToCPU(((vmk_uint32 *) (addrPtr))[2]),\
                                             vmk_BE32ToCPU(((vmk_uint32 *) (addrPtr))[3])


/**
 * \brief Ethernet type used in the ethernet header.
 */
typedef enum vmk_EthType {
   /** Internet protocol version 4. */
   VMK_ETH_TYPE_IPV4                = 0x0800,
   /** Address resolution protocol. */
   VMK_ETH_TYPE_ARP                 = 0x0806,
   /** Cisco discovery protocol. */
   VMK_ETH_TYPE_CDP                 = 0x2000,
   /** Transparent interconnection of lots of links. */
   VMK_ETH_TYPE_TRILL               = 0x22f3,
   /** Transparent Ethernet Bridging. */
   VMK_ETH_TYPE_TRANSPARENT_ETH     = 0x6558,
   /** Reverse address resolution protocol. */
   VMK_ETH_TYPE_RARP                = 0x8035,
   /** Apple talk. */
   VMK_ETH_TYPE_ATALK               = 0x809b,
   /** Apple address resolution protocol. */
   VMK_ETH_TYPE_AARP                = 0x80f3,
   /** Virtual LAN (IEEE 802.1q). */
   VMK_ETH_TYPE_VLAN                = 0x8100,
   /** Internetwork packet exchange (alt). */
   VMK_ETH_TYPE_IPX_ALT             = 0x8137,
   /** Internetwork packet exchange. */
   VMK_ETH_TYPE_IPX                 = 0x8138,
   /** SNMP over ethernet. */
   VMK_ETH_TYPE_SNMP                = 0x814c,
   /** Internet protocol version 6. */
   VMK_ETH_TYPE_IPV6                = 0x86dd,
   /** Slow protocols (IEEE 802.3). */
   VMK_ETH_TYPE_SLOW                = 0x8809,
   /** Multiprotocol label switching (unicast). */
   VMK_ETH_TYPE_MPLS_UNICAST        = 0x8847,
   /** Multiprotocol label switching (multicast). */
   VMK_ETH_TYPE_MPLS_MULTICAST      = 0x8847,
   /** PPP over ethernet (discovery). */
   VMK_ETH_TYPE_PPPOE_DISCOVERY     = 0x8863,
   /** PPP over ethernet (session). */
   VMK_ETH_TYPE_PPPOE_SESSION       = 0x8864,
   /** Jumbo frames. */
   VMK_ETH_TYPE_JUMBO               = 0x8870,
   /** Provider bridging (IEEE 802.3ad). */
   VMK_ETH_TYPE_PROVIDER_BRIDGING   = 0x88a8,
   /** Low level discovery protocol. */
   VMK_ETH_TYPE_LLDP                = 0x88cc,
   /** Akimbi frames, (VMware VMlab). */
   VMK_ETH_TYPE_AKIMBI              = 0x88de,
   /** Fiber channel over ethernet. */
   VMK_ETH_TYPE_FCOE                = 0x8906,
   /** Fiber channel over ethernet (init). */
   VMK_ETH_TYPE_FCOE_INIT           = 0x8914,
   /** VMware ESX beacon probing. */
   VMK_ETH_TYPE_VMWARE              = 0x8922,
   /** QinQ tagging (IEEE 802.1ad). */
   VMK_ETH_TYPE_QINQ                = 0x9100,
   /** DVfilter. */
   VMK_ETH_TYPE_DVFILTER            = 0xdfdf,
} vmk_EthType;


/**
 * \brief Ethernet type used in the ethernet header, in network byte order.
 */
typedef enum {
   /** Internet protocol version 4 (in net order). */
   VMK_ETH_TYPE_IPV4_NBO               = 0x0008,
   /** Address resolution protocol (in net order). */
   VMK_ETH_TYPE_ARP_NBO                = 0x0608,
   /** Cisco discovery protocol (in net order). */
   VMK_ETH_TYPE_CDP_NBO                = 0x0020,
   /** Transparent interconnection of lots of links (in net order). */
   VMK_ETH_TYPE_TRILL_NBO              = 0xf322,
   /** Transparent Ethernet Bridging (in net order). */
   VMK_ETH_TYPE_TRANSPARENT_ETH_NBO    = 0x5865,
   /** Reverse address resolution protocol (in net order). */
   VMK_ETH_TYPE_RARP_NBO               = 0x3580,
   /** Apple talk (in net order). */
   VMK_ETH_TYPE_ATALK_NBO              = 0x9b80,
   /** Apple address resolution protocol (in net order). */
   VMK_ETH_TYPE_AARP_NBO               = 0xf380,
   /** Virtual LAN (IEEE 802.1q) (in net order). */
   VMK_ETH_TYPE_VLAN_NBO               = 0x0081,
   /** Internetwork packet exchange (alt) (in net order). */
   VMK_ETH_TYPE_IPX_ALT_NBO            = 0x3781,
   /** Internetwork packet exchange (in net order). */
   VMK_ETH_TYPE_IPX_NBO                = 0x3881,
   /** SNMP over ethernet (in net order). */
   VMK_ETH_TYPE_SNMP_NBO               = 0x4c81,
   /** Internet protocol version 6 (in net order). */
   VMK_ETH_TYPE_IPV6_NBO               = 0xdd86,
   /** Slow protocols (IEEE 802.3) (in net order). */
   VMK_ETH_TYPE_SLOW_NBO               = 0x0988,
   /** Multiprotocol label switching (unicast) (in net order). */
   VMK_ETH_TYPE_MPLS_UNICAST_NBO       = 0x4788,
   /** Multiprotocol label switching (multicast) (in net order). */
   VMK_ETH_TYPE_MPLS_MULTICAST_NBO     = 0x4788,
   /** PPP over ethernet (discovery) (in net order). */
   VMK_ETH_TYPE_PPPOE_DISCOVERY_NBO    = 0x6388,
   /** PPP over ethernet (session) (in net order). */
   VMK_ETH_TYPE_PPPOE_SESSION_NBO      = 0x6488,
   /** Jumbo frames (in net order). */
   VMK_ETH_TYPE_JUMBO_NBO              = 0x7088,
   /** Provider bridging (IEEE 802.3ad) (in net order). */
   VMK_ETH_TYPE_PROVIDER_BRIDGING_NBO  = 0xa888,
   /** Low level discovery protocol (in net order). */
   VMK_ETH_TYPE_LLDP_NBO               = 0xcc88,
   /** Akimbi frames, (VMware VMlab) (in net order). */
   VMK_ETH_TYPE_AKIMBI_NBO             = 0xde88,
   /** Fiber channel over ethernet (in net order). */
   VMK_ETH_TYPE_FCOE_NBO               = 0x0689,
   /** Fiber channel over ethernet (init) (in net order). */
   VMK_ETH_TYPE_FCOE_INIT_NBO          = 0x1489,
   /** VMware ESX beacon probing (in net order). */
   VMK_ETH_TYPE_VMWARE_NBO             = 0x2289,
   /** QinQ tagging (IEEE 802.1ad) (in net order). */
   VMK_ETH_TYPE_QINQ_NBO               = 0x0091,
   /** DVfilter (in net order). */
   VMK_ETH_TYPE_DVFILTER_NBO           = 0xdfdf,
} vmk_EthTypeNBO;


/**
 * \brief IP protocol numbers, used in IPv4 and IPv6 headers.
 */
typedef enum {
   /** IPv6 hop-by-hop options. */
   VMK_IP_PROTO_IPV6_HOPOPT   = 0x00,
   /** Internet control message protocol version 4. */
   VMK_IP_PROTO_ICMPV4        = 0x01,
   /** Internet group management protocol. */
   VMK_IP_PROTO_IGMP          = 0x02,
   /** Internet protocol version 4. */
   VMK_IP_PROTO_IPV4          = 0x04,
   /** Transmission control protocol. */
   VMK_IP_PROTO_TCP           = 0x06,
   /** Exterior gateway protocol. */
   VMK_IP_PROTO_EGP           = 0x08,
   /** Interior gateway protocol. */
   VMK_IP_PROTO_IGP           = 0x09,
   /** User datagram protocol. */
   VMK_IP_PROTO_UDP           = 0x11,
   /** Internet protocol version 6. */
   VMK_IP_PROTO_IPV6          = 0x29,
   /** IPv6 routing option. */
   VMK_IP_PROTO_IPV6_ROUTE    = 0x2b,
   /** IPv6 fragment option. */
   VMK_IP_PROTO_IPV6_FRAG     = 0x2c,
   /** Generic routing encapsulation. */
   VMK_IP_PROTO_GRE           = 0x2f,
   /** Encapsulating security payload. */
   VMK_IP_PROTO_ESP           = 0x32,
   /** Authentication header. */
   VMK_IP_PROTO_AH            = 0x33,
   /** Internet control message protocol version 6. */
   VMK_IP_PROTO_ICMPV6        = 0x3a,
   /** IPv6 no next header. */
   VMK_IP_PROTO_IPV6_NONXT    = 0x3b,
   /** IPv6 destination options. */
   VMK_IP_PROTO_IPV6_DSTOPTS  = 0x3c,
   /** Enhanced interior gateway routing protocol. */
   VMK_IP_PROTO_EIGRP         = 0x58,
   /** Open shortest path first. */
   VMK_IP_PROTO_OSPF          = 0x59,
   /** IP over IP. */
   VMK_IP_PROTO_IPIP          = 0x5e,
   /** Protocol independant multicast. */
   VMK_IP_PROTO_PIM           = 0x67,
   /** Virtual router redundancy protocol. */
   VMK_IP_PROTO_VRRP          = 0x70,
   /** Layer 2 tunneling protocol. */
   VMK_IP_PROTO_L2TP          = 0x73,
   /** Mobility IP. */
   VMK_IP_PROTO_MOB           = 0x87,
   /** Host identity protocol. */
   VMK_IP_PROTO_HIP           = 0x8b,
} vmk_IPProto;


/**
 * \brief IGMP type numbers, used in IGMP headers.
 */
typedef enum {
   /* Constants from RFC 3376 */
   /** IGMP Host Membership Query. */
   VMK_IGMP_QUERY     = 0x11,
   /** IGMPv1 Host Membership Report. */
   VMK_IGMP_V1REPORT  = 0x12,
   /** IGMPv2 Host Membership Report. */
   VMK_IGMP_V2REPORT  = 0x16,
   /** IGMPv3 Host Membership Report. */
   VMK_IGMP_V3REPORT  = 0x22,
   /** IGMPv2 Leave. */
   VMK_IGMP_V2LEAVE   = 0x17,

   /* Constants from RFC 4286 for IPv4 */
   /** Multicast router discovery advertisement. */
   VMK_IGMP_MRDADV    = 0x30,
   /** Multicast router discovery solicitation. */
   VMK_IGMP_MRDSOL    = 0x31,
   /** Multicast router discovery termination. */
   VMK_IGMP_MRDTERM   = 0x32,
} VMK_IGMPType;


/**
 * \brief MLD type numbers, used in MLD headers.
 */
typedef enum {
   /* Constants from RFC 2710 and RFC 3810. */
   /** Multicast Listener Query. */
   VMK_MLD_QUERY      = 0x82,
   /** Multicast Listener V1 Report. */
   VMK_MLD_V1REPORT   = 0x83,
   /** Multicast Listener V2 Report. */
   VMK_MLD_V2REPORT   = 0x8F,
   /** Multicast Listener Done. */
   VMK_MLD_DONE       = 0x84,

   /* Constants from RFC 4286 for IPv6. */
   /** Multicast router discovery advertisement. */
   VMK_MLD_MRDADV     = 0x97,
   /** Multicast router discovery solicitation. */
   VMK_MLD_MRDSOL     = 0x98,
   /** Multicast router discovery termination. */
   VMK_MLD_MRDTERM    = 0x99,
} VMK_MLDType;


/**
 * \brief Ethernet header.
 */
typedef struct vmk_EthHdr {
   /** Destination MAC address. */
   vmk_uint8   daddr[6];
   /** Source MAC address. */
   vmk_uint8   saddr[6];
   /** Ethernet type of the payload. */
   vmk_uint16  type;
} VMK_ATTRIBUTE_PACKED vmk_EthHdr;


/**
 * \brief VLAN header, inserted after the type field of regular ethernet frames.
 */
typedef struct vmk_VLANHdr {
   /** High four bits of the VLAN ID. */
   vmk_uint8   vlanIDHigh:4;
   /** The frame is eligible to be dropped in the presence of congestion. */
   vmk_uint8   dropEligible:1;
   /** Priority tag. */
   vmk_uint8   priority:3;
   /** Low eight bits of the VLAN ID. */
   vmk_uint8   vlanIDLow;
   /** Ethernet type of the payload. */
   vmk_uint16  type;
} VMK_ATTRIBUTE_PACKED vmk_VLANHdr;

/**
 * \brief ARP header
 */
typedef struct vmk_ArpHdr {
   /** Hardware Type */
   vmk_uint16 htype;
   /** Protocol Type */
   vmk_uint16 ptype;
   /** Hardware Address Len */
   vmk_uint8 hlen;
   /** Protocol Address Len */
   vmk_uint8 plen;
   /** Operation */
   vmk_uint16 oper;
   /** Sender Hardware address. */
   vmk_uint8   sha[6];
   /** Sender Protocol address. */
   vmk_uint32   spa;
   /** Target Hardware address. */
   vmk_uint8   tha[6];
   /** Target Protocol address. */
   vmk_uint32   tpa;
} VMK_ATTRIBUTE_PACKED vmk_ArpHdr;

/** Helper macro to get the vlan ID in a vmk_VLANHdr. */
#define VMK_VLAN_HDR_GET_VID(hdr) \
   (((hdr)->vlanIDHigh << 8) | (hdr)->vlanIDLow)

/** Helper macro to set the vlan ID in a vmk_VLANHdr. */
#define VMK_VLAN_HDR_SET_VID(hdr, val) do {  \
   (hdr)->vlanIDHigh = (val) >> 8;           \
   (hdr)->vlanIDLow = (val);                 \
} while (0)


/**
 * \brief IPv4 header.
 */
typedef struct vmk_IPv4Hdr {
   /** Length of the IP header, including options in 32-bit words. */
   vmk_uint8   headerLength:4;
   /** Internet protocol version. */
   vmk_uint8   version:4;
   /** This field has two different interpretations. */
   union {
      /** The old interpretation, TOS. */
      struct {
         /** Type of service. */
         vmk_uint8   tos:5;
         /** Precendence. */
         vmk_uint8   precedence:3;
      };
      /** The new interpretation, DSCP. */
      struct {
         /** Explicit Congestion Notification. */
         vmk_uint8   ecn:2;
         /** Differentiated Services Code Point. */
         vmk_uint8   dscp:6;
      };
      /** Global access. */
      vmk_uint8 qosFlags;
   };
   /** Total length of the fragment. */
   vmk_uint16  totalLength;
   /** Identification of the fragment in the original packet. */
   vmk_uint16  identification;
   /** IP Fragment info. */
   union {
      /** Bitfield access. */
      struct {
         /** High five bits of this fragment's offset in the packet. */
         vmk_uint8   fragmentOffsetHigh:5;
         /** More fragments of this packets are available. */
         vmk_uint8   moreFragments:1;
         /** This packet must not be fragmented. */
         vmk_uint8   dontFragment:1;
         /** Reserved bit, must be zero. */
         vmk_uint8   zero:1;
         /** Low eight bits of the fragment offset. */
         vmk_uint8   fragmentOffsetLow;
      };
      /** Global access. */
      vmk_uint16  fragmentInfo;
   };
   /** Time to live. */
   vmk_uint8   ttl;
   /** Protocol number of the payload. */
   vmk_uint8   protocol;
   /** Checksum of the header. */
   vmk_uint16  checksum;
   /** Source address of the packet. */
   vmk_uint32  saddr;
   /** Destination address of the packet. */
   vmk_uint32  daddr;
} VMK_ATTRIBUTE_PACKED vmk_IPv4Hdr;

/** Helper macro to get the fragment offset in a vmk_IPv4Hdr. */
#define VMK_IPV4_HDR_GET_FRAGMENT_OFFSET(hdr)   \
   (((hdr)->fragmentOffsetHigh << 8) | (hdr)->fragmentOffsetLow)

/** Helper macro to set the fragment offset in a vmk_IPv4Hdr. */
#define VMK_IPV4_HDR_SET_FRAGMENT_OFFSET(hdr, val) do {  \
   (hdr)->fragmentOffsetHigh = (val) >> 8;               \
   (hdr)->fragmentOffsetLow = (val);                     \
} while (0)


/**
 * \brief IPv6 header.
 */
typedef struct vmk_IPv6Hdr {
   /** High four bits of the traffic class. */
   vmk_uint8   trafficClassHigh:4;
   /** Internet protocol version. */
   vmk_uint8   version:4;
   /** High four bits of the flow label. */
   vmk_uint8   flowLabelHigh:4;
   /** Low four bits of the traffic class. */
   vmk_uint8   trafficClassLow:4;
   /** High sixteen bits of the flow label. */
   vmk_uint16  flowLabelLow;
   /** Length of this packets payload. */
   vmk_uint16  payloadLength;
   /** Next header's protocol number. */
   vmk_uint8   nextHeader;
   /** Hop limit (similar to TTL in IPv4). */
   vmk_uint8   hopLimit;
   /** Source address of the packet. */
   vmk_uint8   saddr[16];
   /** Destination address of the packet. */
   vmk_uint8   daddr[16];
} VMK_ATTRIBUTE_PACKED vmk_IPv6Hdr;

/** Helper macro to get the traffic class in a vmk_IPv6Hdr. */
#define VMK_IPV6_HDR_GET_TRAFFIC_CLASS(hdr)  \
   (((hdr)->trafficClassHigh << 4) | (hdr)->trafficClassLow)

/** Helper macro to set the traffic class in a vmk_IPv6Hdr. */
#define VMK_IPV6_HDR_SET_TRAFFIC_CLASS(hdr, val) do { \
   (hdr)->trafficClassHigh = (val) >> 4;              \
   (hdr)->trafficClassLow = (val);                    \
} while (0)

/** Helper macro to get the flow label in a vmk_IPv6Hdr. */
#define VMK_IPV6_HDR_GET_FLOW_LABEL(hdr)  \
   (((hdr)->flowLabelHigh << 16) | vmk_Ntohs((hdr)->flowLabelLow))

/** Helper macro to set the flow label in a vmk_IPv6Hdr. */
#define VMK_IPV6_HDR_SET_FLOW_LABEL(hdr, val) do { \
   (hdr)->flowLabelHigh = (val) >> 16;             \
   (hdr)->flowLabelLow = vmk_Htons(val);           \
} while (0)


/**
 * \brief IPv6 extension header.
 */
typedef struct vmk_IPv6ExtHdr {
   /** Next header's protocol number. */
   vmk_uint8   nextHeader;
   /** Length of this header, in 64-bit words (not including the first one). */
   vmk_uint8   hdrExtLength;
   /** Option-specific area. */
   vmk_uint16  optPad1;
   /** Option-specific area. */
   vmk_uint32  optPad2;
} VMK_ATTRIBUTE_PACKED vmk_IPv6ExtHdr;


/**
 * \brief ICMP header.
 */
typedef struct vmk_ICMPHdr {
   /** Type of this ICMP message. */
   vmk_uint8   type;
   /** Code of this ICMP message. */
   vmk_uint8   code;
   /** Checksum of the entire ICMP message. */
   vmk_uint16  checksum;
} VMK_ATTRIBUTE_PACKED vmk_ICMPHdr;


/**
 * \brief IGMP header for IGMPv1/v2.
 */
typedef struct vmk_IGMPHdr {
   /** Type of this IGMP message. */
   vmk_uint8       type;
   /** Maximum time for an IGMP report corresponding to the IGMP query. */
   vmk_uint8       maxResponseTime;
   /** Checksum of the entire IGMP message. */
   vmk_uint16      checksum;
   /** Multicast group on which this IGMP message operates. */
   vmk_IPv4Address  groupAddr;
} VMK_ATTRIBUTE_PACKED vmk_IGMPHdr;


/**
 * \brief IGMPv3 query header.
 */
typedef struct vmk_IGMPv3QueryHdr {
   /** Type of this IGMP message. */
   vmk_uint8        type;
   /** Maximum time for an IGMP report corresponding to the IGMP query. */
   vmk_uint8        maxResponseCode;
   /** Checksum of the entire IGMP message. */
   vmk_uint16       checksum;
   /** Multicast group on which this IGMP message operates. */
   vmk_IPv4Address  groupAddr;
   /** Querier's Robustness Variable. */
   vmk_uint8        qrv:3;
   /** S Flag: Suppress Router-Side Processing. */
   vmk_uint8        sFlag:1;
   /** Reserved bits, must be 0. */
   vmk_uint8        reserved:4;
   /** Querier's Query Interval Code. */
   vmk_uint8        qqic;
   /** Number of the source IP addresses. */
   vmk_uint16       numSources;
   /** Source Address present in this multicast group. */
   vmk_IPv4Address  sourcesIPv4[];
} VMK_ATTRIBUTE_PACKED vmk_IGMPv3QueryHdr;


/**
 * \brief IGMPv3 group record.
 */
typedef struct vmk_IGMPv3GrpRecord {
   /** Type of this IGMPv3 group record. */
   vmk_uint8        type;
   /** Length of auxiliary data. */
   vmk_uint8        dataLength;
   /** Number of the source IP addresses. */
   vmk_uint16       numSources;
   /** Multicast group on which this IGMP message operates. */
   vmk_IPv4Address  groupIPv4Addr;
   /** Source Address allow|deny in this multicast group. */
   vmk_IPv4Address  sourcesIPv4[];
} vmk_IGMPv3GrpRecord;


/**
 * \brief IGMPv3 host membership report header.
 */
typedef struct vmk_IGMPv3ReportHdr {
   /** Type of this IGMPv3 report. */
   vmk_uint8           type;
   /** Reserved bytes, must be 0. */
   vmk_uint8           reserved1;
   /** Checksum of the entire IGMP message. */
   vmk_uint16          checksum;
   /** Reserved bytes, must be 0. */
   vmk_uint16          reserved2;
   /** Number of the group record(s). */
   vmk_uint16          numGroups;
   /** Group record(s) of this IGMPv3 report. */
   vmk_IGMPv3GrpRecord record[];
} vmk_IGMPv3ReportHdr;


/**
 * \brief MLD header for MLDv1.
 */
typedef struct vmk_MLDHdr {
   /** Type of this MLD message. */
   vmk_uint8        type;
   /** Code of this MLD message. */
   vmk_uint8        code;
   /** Checksum of the entire MLD message. */
   vmk_uint16       checksum;
   /** Maximum time for a MLD report corresponding to the MLD query. */
   vmk_uint16       maxResponseTime;
   /** Reserved bytes, must be 0. */
   vmk_uint16       reserved;
   /** Multicast group on which this MLD message operates. */
   vmk_IPv6Address  groupIPv6Addr;
} VMK_ATTRIBUTE_PACKED vmk_MLDHdr;


/**
 * \brief MLDv2 query header.
 */
typedef struct vmk_MLDv2QueryHdr {
   /** Type of this MLD message. */
   vmk_uint8        type;
   /** Code of this MLD message. */
   vmk_uint8        code;
   /** Checksum of the entire MLD message. */
   vmk_uint16       checksum;
   /** Maximum time for a MLD report corresponding to the MLD query. */
   vmk_uint16       maxResponseCode;
   /** Reserved bytes, must be 0. */
   vmk_uint16       reserved1;
   /** Multicast group on which this MLD message operates. */
   vmk_IPv6Address  groupIPv6Addr;
   /** Querier's Robustness Variable. */
   vmk_uint8        qrv:3;
   /** S Flag: Suppress Router-Side Processing. */
   vmk_uint8        sFlag:1;
   /** Reserved bits, must be 0. */
   vmk_uint8        reserved2:4;
   /** Querier's Query Interval Code. */
   vmk_uint8        qqic;
   /** Number of the source IP addresses. */
   vmk_uint16       numSources;
   /** Source Address present in this multicast group. */
   vmk_IPv6Address  sourcesIPv6[];
} VMK_ATTRIBUTE_PACKED vmk_MLDv2QueryHdr;


/**
 * \brief MLDv2 group record.
 */
typedef struct vmk_MLDv2GrpRecord {
   /** Type of this MLDv2 group record. */
   vmk_uint8        type;
   /** Length of auxiliary data. */
   vmk_uint8        dataLength;
   /** Number of the source IP addresses. */
   vmk_uint16       numSources;
   /** Multicast group on which this MLD message operates. */
   vmk_IPv6Address  groupIPv6Addr;
   /** Source Address allow|deny in this multicast group. */
   vmk_IPv6Address  sourcesIPv6[];
} vmk_MLDv2GrpRecord;


/**
 * \brief MLDv2 host membership report header.
 */
typedef struct vmk_MLDv2ReportHdr {
   /** Type of this MLDv2 report. */
   vmk_uint8           type;
   /** Reserved bytes, must be 0. */
   vmk_uint8           reserved1;
   /** Checksum of the entire MLD message. */
   vmk_uint16          checksum;
   /** Reserved bytes, must be 0. */
   vmk_uint16          reserved2;
   /** Number of the group record(s). */
   vmk_uint16          numGroups;
   /** Group record(s) of this MLDv2 report. */
   vmk_MLDv2GrpRecord  record[];
} vmk_MLDv2ReportHdr;


/**
 * \brief UDP header.
 */
typedef struct vmk_UDPHdr {
   /** Source port. */
   vmk_uint16  srcPort;
   /** Destination port. */
   vmk_uint16  dstPort;
   /** Length of the entire UDP message (header + data). */
   vmk_uint16  length;
   /** Checksum of the UDP message. */
   vmk_uint16  checksum;
} VMK_ATTRIBUTE_PACKED vmk_UDPHdr;


/**
 * \brief TCP header.
 */
typedef struct vmk_TCPHdr {
   /** Source port. */
   vmk_uint16  srcPort;
   /** Destination port. */
   vmk_uint16  dstPort;
   /** Sequence number. */
   vmk_uint32  seq;
   /** Acknowledgement number. */
   vmk_uint32  ackSeq;
   /** Reserved. */
   vmk_uint8   reserved:4;
   /** Length of the TCP header in 32-bit words. */
   vmk_uint8   dataOffset:4;
   /** TCP flags. */
   union {
      /** Bitfield access. */
      struct {
         /** FIN flag: no more data from sender. */
         vmk_uint8   fin:1;
         /** SYN flag: synchronize sequence numbers. */
         vmk_uint8   syn:1;
         /** RST flag: reset the connection. */
         vmk_uint8   rst:1;
         /** PSH flag: push data to application. */
         vmk_uint8   psh:1;
         /** ACK flag: acknowledgement field is significant. */
         vmk_uint8   ack:1;
         /** URG flag: urgent pointer field is significant. */
         vmk_uint8   urg:1;
         /** ECE flag: peer is ECN capable. */
         vmk_uint8   ece:1;
         /** CWR flag: congestion window has been reduced. */
         vmk_uint8   cwr:1;
      };
      /** Global access. */
      vmk_uint8 flags;
   };
   /** Size of the receive window. */
   vmk_uint16  window;
   /** Checksum of the TCP packet. */
   vmk_uint16  checksum;
   /** Last urgent data byte. */
   vmk_uint16  urgPtr;
} VMK_ATTRIBUTE_PACKED vmk_TCPHdr;


/**
 * \brief Fence header.
 */
typedef struct vmk_FenceHdr {
   /** Fence header version. */
   vmk_uint8   version:2;
   /** Original ethernet frame is fragmented. */
   vmk_uint8   frag:1;
   /** Fragment ID. */
   vmk_uint8   fragmentID:5;
   /** Fence ID. */
   vmk_uint32  fenceID:24;
} VMK_ATTRIBUTE_PACKED vmk_FenceHdr;


/**
 * \brief VXLAN header.
 */
typedef struct vmk_VXLANHdr {
   /** Flags. */
   vmk_uint8   flags1:2;
   /** Packet needs replication to multicast group (used for multicast proxy). */
   vmk_uint8   locallyReplicate:1;
   /** Instance ID flag, must be set to 1. */
   vmk_uint8   instanceID:1;
   /** Flags. */
   vmk_uint8   flags2:4;
   /** Reserved. */
   vmk_uint32  reserved1:24;
   /** VXLAN ID. */
   vmk_uint32  vxlanID:24;
   /** Reserved. */
   vmk_uint8   reserved2:8;
} VMK_ATTRIBUTE_PACKED vmk_VXLANHdr;

/** Helper macro to check if a VXLAN ID is valid. */
#define VMK_VXLAN_ID_IS_VALID(vxlanID) (0 < (vxlanID) && (vxlanID) <= 0xffffff)

/**
 * Protocol ID used by the UDP parser as vmk_PktHeaderEntry's nextHdrProto for
 * VXLAN.
 */
#define VMK_UDP_PROTO_VXLAN 8472

/**
 * \brief GRE header.
 */
typedef struct vmk_GREHdr {
   /** GRE flags. */
   union {
      /** Bitfield access. */
      struct {
         /** Recursion control. */
         vmk_uint8   reserved1:3;
         /** Strict source route. */
         vmk_uint8   strictSourceRoute:1;
         /** Sequence number field is present. */
         vmk_uint8   sequencePresent:1;
         /** Key field is present. */
         vmk_uint8   keyPresent:1;
         /** Routing field is present. */
         vmk_uint8   routingPresent:1;
         /** Checksum field is present. */
         vmk_uint8   checksumPresent:1;
         /** Version. */
         vmk_uint8   version:3;
         /** Flags. */
         vmk_uint8   reserved2:4;
         /** Acknowledgment sequence number present. */
         vmk_uint8   ackPresent:1;
      };
      /** Global access. */
      vmk_uint16  flags;
   };
   /** Protocol number of the inner packet. */
   vmk_uint16  protocol;
   /** Optionnal fields follow. */
   vmk_uint32  options[0];
} VMK_ATTRIBUTE_PACKED vmk_GREHdr;

/** IANA assigned UDP port for Geneve encapsulation format */
#define VMK_NET_PROTO_GENEVE_UDP_PORT 6081
#define VMK_NET_PROTO_GENEVE_UDP_PORT_NBO 0xC117

/**
 * \brief Geneve header.
 */
typedef struct vmk_GeneveHdr {
   /** Length of options (in 4 bytes multiple) */
   vmk_uint8   optionsLength:6;
   /** Geneve protocol version */
   vmk_uint8   version:2;
   /** Reserved bits */
   vmk_uint8   reserved1:6;
   /** Critical options present flag */
   vmk_uint8   criticalOptions:1;
   /** OAM frame flag */
   vmk_uint8   oamFrame:1;
   /** Protocol type of the following header using Ethernet type values */
   vmk_uint16  protocolType;
   /** Virtual network identifier */
   vmk_uint32  virtualNetworkId:24;
   /** Reserved bits */
   vmk_uint32  reserved2:8;
   /** Variable-length options */
   vmk_uint32  options[0];
} VMK_ATTRIBUTE_PACKED vmk_GeneveHdr;

/**
 * \brief Geneve option header.
 */
typedef struct vmk_GeneveOptionHdr {
   /** Namespace for the "Type" field. */
   vmk_uint16  optClass;
   /** Indicating the format of the data contained in this option block. */
   vmk_uint8   optType;
   /**
    * Length of this option block, expressed in four byte multiples excluding
    * the option header.
    */
   vmk_uint8   optLength:5;
   /**
    * Option control flags reserved for future use. They must be zero
    * on transmission and ignored on receipt.
    */
   vmk_uint8   reserved:3;
} VMK_ATTRIBUTE_PACKED vmk_GeneveOptionHdr;

/**
 * \brief PIM header.
 */
typedef struct vmk_PIMHdr {
   /** PIM message type. */
   vmk_uint8   type:4;
   /** PIM version number. */
   vmk_uint8   version:4;
   /** Reserved. */
   vmk_uint8   reserved;
   /** Checksum of the entire PIM message. */
   vmk_uint16  checksum;
} VMK_ATTRIBUTE_PACKED vmk_PIMHdr;


#endif /* _VMKAPI_NET_PROTO_H_ */
/** @} */
/** @} */
