/* bnx2x_plugin_defs.h: Broadcom bnx2x NPA plugin
 *
 * Copyright 2009-2011 Broadcom Corporation
 *
 * Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2, available
 * at http://www.gnu.org/licenses/old-licenses/gpl-2.0.html (the "GPL").
 *
 * Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a
 * license other than the GPL, without Broadcom's express prior written
 * consent.
 *
 * Written by: Shmulik Ravid
 *
 */
#ifndef _BNX2X_PLUGIN_DEFS_H
#define _BNX2X_PLUGIN_DEFS_H

/*
 * hsi types
 */
typedef int8                __s8;
typedef int16               __s16;
typedef int32               __s32;
typedef int64               __s64;
typedef uint8               __u8;
typedef uint16              __u16;
typedef uint32              __u32;
typedef uint64              __u64;
typedef __s8                s8;
typedef __s16               s16;
typedef __s32               s32;
typedef __s64               s64;
typedef __u8                u8;
typedef __u16               u16;
typedef __u32               u32;
typedef __u64               u64;
typedef __u16				__le16;
typedef __u16				__be16;
typedef __u32				__le32;
typedef __u32				__be32;
typedef __u64				__le64;
typedef __u64				__be64;
typedef __u16               __sum16;
typedef __u64			    dma_addr_t;

/* endianity */
#ifndef __LITTLE_ENDIAN
#define __LITTLE_ENDIAN 1234
#endif
#ifndef __LITTLE_ENDIAN_BITFIELD
#define __LITTLE_ENDIAN_BITFIELD
#endif

/* NULL */
#ifndef NULL
#define NULL    0
#endif


/*
 * Byte swapping macros not using architecture shortcuts
 */
#define ___constant_swab16(x) ((__u16)(                         \
	(((__u16)(x) & (__u16)0x00ffU) << 8) |                  \
	(((__u16)(x) & (__u16)0xff00U) >> 8)))

#define ___constant_swab32(x) ((__u32)(                         \
      (((__u32)(x) & (__u32)0x000000ffUL) << 24) |              \
      (((__u32)(x) & (__u32)0x0000ff00UL) <<  8) |              \
      (((__u32)(x) & (__u32)0x00ff0000UL) >>  8) |              \
      (((__u32)(x) & (__u32)0xff000000UL) >> 24)))

#define ___constant_swab64(x) ((__u64)(                         \
	(((__u64)(x) & (__u64)0x00000000000000ffULL) << 56) |   \
	(((__u64)(x) & (__u64)0x000000000000ff00ULL) << 40) |   \
	(((__u64)(x) & (__u64)0x0000000000ff0000ULL) << 24) |   \
	(((__u64)(x) & (__u64)0x00000000ff000000ULL) <<  8) |   \
	(((__u64)(x) & (__u64)0x000000ff00000000ULL) >>  8) |   \
	(((__u64)(x) & (__u64)0x0000ff0000000000ULL) >> 24) |   \
	(((__u64)(x) & (__u64)0x00ff000000000000ULL) >> 40) |   \
	(((__u64)(x) & (__u64)0xff00000000000000ULL) >> 56)))

#define ___constant_swahw32(x) ((__u32)(                        \
	(((__u32)(x) & (__u32)0x0000ffffUL) << 16) |            \
	(((__u32)(x) & (__u32)0xffff0000UL) >> 16)))

#define ___constant_swahb32(x) ((__u32)(                        \
	(((__u32)(x) & (__u32)0x00ff00ffUL) << 8) |             \
	(((__u32)(x) & (__u32)0xff00ff00UL) >> 8)))

# define __swab16 ___constant_swab16
# define __swab32 ___constant_swab32
# define __swab64 ___constant_swab64
# define __swahw32 ___constant_swahw32
# define __swahb32 ___constant_swahb32
# define swab16 __swab16
# define swab32 __swab32
# define swab64 __swab64
# define swahw32 __swahw32
# define swahb32 __swahb32

#define __constant_htonl(x) ((__be32)___constant_swab32((x)))
#define __constant_ntohl(x) ___constant_swab32((__be32)(x))
#define __constant_htons(x) ((__be16)___constant_swab16((x)))
#define __constant_ntohs(x) ___constant_swab16((__be16)(x))
#define __constant_cpu_to_le64(x) ((__le64)(__u64)(x))
#define __constant_le64_to_cpu(x) ((__u64)(__le64)(x))
#define __constant_cpu_to_le32(x) ((__le32)(__u32)(x))
#define __constant_le32_to_cpu(x) ((__u32)(__le32)(x))
#define __constant_cpu_to_le16(x) ((__le16)(__u16)(x))
#define __constant_le16_to_cpu(x) ((__u16)(__le16)(x))
#define __constant_cpu_to_be64(x) ((__be64)___constant_swab64((x)))
#define __constant_be64_to_cpu(x) ___constant_swab64((__u64)(__be64)(x))
#define __constant_cpu_to_be32(x) ((__be32)___constant_swab32((x)))
#define __constant_be32_to_cpu(x) ___constant_swab32((__u32)(__be32)(x))
#define __constant_cpu_to_be16(x) ((__be16)___constant_swab16((x)))
#define __constant_be16_to_cpu(x) ___constant_swab16((__u16)(__be16)(x))
#define __cpu_to_le64(x) ((__le64)(__u64)(x))
#define __le64_to_cpu(x) ((__u64)(__le64)(x))
#define __cpu_to_le32(x) ((__le32)(__u32)(x))
#define __le32_to_cpu(x) ((__u32)(__le32)(x))
#define __cpu_to_le16(x) ((__le16)(__u16)(x))
#define __le16_to_cpu(x) ((__u16)(__le16)(x))
#define __cpu_to_be64(x) ((__be64)__swab64((x)))
#define __be64_to_cpu(x) __swab64((__u64)(__be64)(x))
#define __cpu_to_be32(x) ((__be32)__swab32((x)))
#define __be32_to_cpu(x) __swab32((__u32)(__be32)(x))
#define __cpu_to_be16(x) ((__be16)__swab16((x)))
#define __be16_to_cpu(x) __swab16((__u16)(__be16)(x))

/* from byte-order generic */
#define cpu_to_le64 __cpu_to_le64
#define le64_to_cpu __le64_to_cpu
#define cpu_to_le32 __cpu_to_le32
#define le32_to_cpu __le32_to_cpu
#define cpu_to_le16 __cpu_to_le16
#define le16_to_cpu __le16_to_cpu
#define cpu_to_be64 __cpu_to_be64
#define be64_to_cpu __be64_to_cpu
#define cpu_to_be32 __cpu_to_be32
#define be32_to_cpu __be32_to_cpu
#define cpu_to_be16 __cpu_to_be16
#define be16_to_cpu __be16_to_cpu

/* networking macros */
#undef ntohl
#undef ntohs
#undef htonl
#undef htons

#define ___htonl(x) __cpu_to_be32(x)
#define ___htons(x) __cpu_to_be16(x)
#define ___ntohl(x) __be32_to_cpu(x)
#define ___ntohs(x) __be16_to_cpu(x)

#define htonl(x) ___htonl(x)
#define ntohl(x) ___ntohl(x)
#define htons(x) ___htons(x)
#define ntohs(x) ___ntohs(x)

/*
 * inet definitions
 */

/* ethernet */
#define BCMVF_ETH_ALEN        6			/* Octets in one ethernet addr  */
#define BCMVF_ETH_HLEN        14		/* Total octets in header.      */
#define BCMVF_ETH_DATA_LEN    1500		/* Max. octets in payload       */
#define BCMVF_ETH_JUMBO_TYPE  0x8870	/* Max. octets in payload       */

#include "vmware_pack_begin.h"
struct ethhdr {
	unsigned char   h_dest[BCMVF_ETH_ALEN];		/* destination eth addr */
	unsigned char   h_source[BCMVF_ETH_ALEN]; 	/* source ether addr    */
	__be16          h_proto;                	/* packet type ID field */
};
#include "vmware_pack_end.h"

/* ipv4 */
#include "vmware_pack_begin.h"
struct iphdr {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	u8	    ihl:4,
			version:4;
#elif defined (__BIG_ENDIAN_BITFIELD)
	u8	    version:4,
			ihl:4;
#else
#error	"Please fix <asm/byteorder.h>"
#endif
	u8	    tos;
	__be16	tot_len;
	__be16	id;
	__be16	frag_off;
	u8	    ttl;
	u8	    protocol;
	__sum16	check;
	__be32	saddr;
	__be32	daddr;
	/*The options start here. */
};
#include "vmware_pack_end.h"

/* ipv6 */
#include "vmware_pack_begin.h"
struct ipv6hdr {
#if defined(__LITTLE_ENDIAN_BITFIELD)
    u8      priority:4,
	    version:4;
#elif defined(__BIG_ENDIAN_BITFIELD)
    u8      version:4,
	    priority:4;
#else
#error  "Please fix <asm/byteorder.h>"
#endif
    u8      flow_lbl[3];
    __be16  payload_len;
    u8      nexthdr;
    u8      hop_limit;
    u8      saddr[16];
    u8      daddr[16];
};
#include "vmware_pack_end.h"

#define NEXTHDR_HOP             0       /* Hop-by-hop option header. */
#define NEXTHDR_TCP             6       /* TCP segment. */
#define NEXTHDR_UDP             17      /* UDP message. */
#define NEXTHDR_IPV6            41      /* IPv6 in IPv6 */
#define NEXTHDR_ROUTING         43      /* Routing header. */
#define NEXTHDR_FRAGMENT        44      /* Fragmentation/reassembly header. */
#define NEXTHDR_ESP             50      /* Encapsulating security payload. */
#define NEXTHDR_AUTH            51      /* Authentication header. */
#define NEXTHDR_ICMP            58      /* ICMP for IPv6. */
#define NEXTHDR_NONE            59      /* No next header */
#define NEXTHDR_DEST            60      /* Destination options header. */
#define NEXTHDR_MOBILITY        135     /* Mobility header. */
#define NEXTHDR_MAX             255

/* PAGE_SHIFT determines the page size */
#undef  PAGE_SIZE
#define PAGE_SHIFT              (12)
#define PAGE_SIZE               (1 << PAGE_SHIFT)
#define PAGE_MASK               (PAGE_SIZE-1)

/* Alignment */
#define __ALIGN_MASK(x,mask)    (((x)+(mask))&~(mask))
#define ALIGN(x,a)              __ALIGN_MASK(x,(typeof(x))(a)-1)
#define PAGE_ALIGN(addr)        ALIGN(addr, PAGE_SIZE)

#ifndef min
    #define min(x,y) ((x) < (y) ? x : y)
#endif

#ifndef max
    #define max(x,y) ((x) > (y) ? x : y)
#endif


#endif /* bnx2x_plugin_defs.h */
