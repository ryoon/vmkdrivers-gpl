/*
 * Portions Copyright 2008 - 2010 VMware, Inc.
 */
/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Global definitions for the Ethernet IEEE 802.3 interface.
 *
 * Version:	@(#)if_ether.h	1.0.1a	02/08/94
 *
 * Author:	Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *		Donald Becker, <becker@super.org>
 *		Alan Cox, <alan@redhat.com>
 *		Steve Whitehouse, <gw7rrm@eeshack3.swan.ac.uk>
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */
 
#ifndef _LINUX_IF_ETHER_H
#define _LINUX_IF_ETHER_H

#include <linux/types.h>

/*
 *	IEEE 802.3 Ethernet magic constants.  The frame sizes omit the preamble
 *	and FCS/CRC (frame check sequence). 
 */

#define ETH_ALEN	6		/* Octets in one ethernet addr	 */
#define ETH_HLEN	14		/* Total octets in header.	 */
#define ETH_ZLEN	60		/* Min. octets in frame sans FCS */
#define ETH_DATA_LEN	1500		/* Max. octets in payload	 */
#define ETH_FRAME_LEN	1514		/* Max. octets in frame sans FCS */

#if defined(__VMKLNX__)

#define ETH_HEADER_TYPE_DIX               0
#define ETH_HEADER_TYPE_802_1PQ           1
#define ETH_HEADER_TYPE_802_3             2
#define ETH_HEADER_TYPE_802_1PQ_802_3     3

#endif

/*
 *	These are the defined Ethernet Protocol ID's.
 */

#define ETH_P_LOOP	0x0060		/* Ethernet Loopback packet	*/
#define ETH_P_PUP	0x0200		/* Xerox PUP packet		*/
#define ETH_P_PUPAT	0x0201		/* Xerox PUP Addr Trans packet	*/
#define ETH_P_IP	0x0800		/* Internet Protocol packet	*/
#define ETH_P_X25	0x0805		/* CCITT X.25			*/
#define ETH_P_ARP	0x0806		/* Address Resolution packet	*/
#define	ETH_P_BPQ	0x08FF		/* G8BPQ AX.25 Ethernet Packet	[ NOT AN OFFICIALLY REGISTERED ID ] */
#define ETH_P_IEEEPUP	0x0a00		/* Xerox IEEE802.3 PUP packet */
#define ETH_P_IEEEPUPAT	0x0a01		/* Xerox IEEE802.3 PUP Addr Trans packet */
#define ETH_P_DEC       0x6000          /* DEC Assigned proto           */
#define ETH_P_DNA_DL    0x6001          /* DEC DNA Dump/Load            */
#define ETH_P_DNA_RC    0x6002          /* DEC DNA Remote Console       */
#define ETH_P_DNA_RT    0x6003          /* DEC DNA Routing              */
#define ETH_P_LAT       0x6004          /* DEC LAT                      */
#define ETH_P_DIAG      0x6005          /* DEC Diagnostics              */
#define ETH_P_CUST      0x6006          /* DEC Customer use             */
#define ETH_P_SCA       0x6007          /* DEC Systems Comms Arch       */
#define ETH_P_RARP      0x8035		/* Reverse Addr Res packet	*/
#define ETH_P_ATALK	0x809B		/* Appletalk DDP		*/
#define ETH_P_AARP	0x80F3		/* Appletalk AARP		*/
#define ETH_P_8021Q	0x8100          /* 802.1Q VLAN Extended Header  */
#define ETH_P_IPX	0x8137		/* IPX over DIX			*/
#define ETH_P_IPV6	0x86DD		/* IPv6 over bluebook		*/
#define ETH_P_SLOW	0x8809		/* Slow Protocol. See 802.3ad 43B */
#define ETH_P_WCCP	0x883E		/* Web-cache coordination protocol
					 * defined in draft-wilson-wrec-wccp-v2-00.txt */
#define ETH_P_PPP_DISC	0x8863		/* PPPoE discovery messages     */
#define ETH_P_PPP_SES	0x8864		/* PPPoE session messages	*/
#define ETH_P_MPLS_UC	0x8847		/* MPLS Unicast traffic		*/
#define ETH_P_MPLS_MC	0x8848		/* MPLS Multicast traffic	*/
#define ETH_P_ATMMPOA	0x884c		/* MultiProtocol Over ATM	*/
#define ETH_P_ATMFATE	0x8884		/* Frame-based ATM Transport
					 * over Ethernet
					 */
#define ETH_P_AOE	0x88A2		/* ATA over Ethernet		*/
#define ETH_P_TIPC	0x88CA		/* TIPC 			*/

#define ETH_P_VMWARE    0x8922
#define ETH_P_AKIMBI    0x88DE
#define ETH_P_IPV6_NBO  0xDD86
#define ETH_P_AKIMBI_NBO 0xDE88

#if defined(__VMKLNX__)
#define ETH_P_FCOE      0x8906          /* FCOE ether type */
#define ETH_P_FIP       0x8914          /* FIP ether type */
#endif /* defined(__VMKLNX__) */


/*
 *	Non DIX types. Won't clash for 1500 types.
 */
 
#define ETH_P_802_3	0x0001		/* Dummy type for 802.3 frames  */
#define ETH_P_AX25	0x0002		/* Dummy protocol id for AX.25  */
#define ETH_P_ALL	0x0003		/* Every packet (be careful!!!) */
#define ETH_P_802_2	0x0004		/* 802.2 frames 		*/
#define ETH_P_SNAP	0x0005		/* Internal only		*/
#define ETH_P_DDCMP     0x0006          /* DEC DDCMP: Internal only     */
#define ETH_P_WAN_PPP   0x0007          /* Dummy type for WAN PPP frames*/
#define ETH_P_PPP_MP    0x0008          /* Dummy type for PPP MP frames */
#define ETH_P_LOCALTALK 0x0009		/* Localtalk pseudo type 	*/
#define ETH_P_PPPTALK	0x0010		/* Dummy type for Atalk over PPP*/
#define ETH_P_TR_802_2	0x0011		/* 802.2 frames 		*/
#define ETH_P_MOBITEX	0x0015		/* Mobitex (kaz@cafe.net)	*/
#define ETH_P_CONTROL	0x0016		/* Card specific control frames */
#define ETH_P_IRDA	0x0017		/* Linux-IrDA			*/
#define ETH_P_ECONET	0x0018		/* Acorn Econet			*/
#define ETH_P_HDLC	0x0019		/* HDLC frames			*/
#define ETH_P_ARCNET	0x001A		/* 1A for ArcNet :-)            */

/*
 *	This is an Ethernet frame header.
 */
 
#if defined(__VMKLNX__)
struct ethhdr_llc {
        unsigned char	h_dest[ETH_ALEN];
	unsigned char	h_source[ETH_ALEN];
        unsigned short  h_llc_len;
	unsigned char   h_llc_dsap;
	unsigned char   h_llc_ssap;
	unsigned char   h_llc_control;
	unsigned char   h_llc_snap_org[3];
	__be16          h_llc_snap_type;
} __attribute__((packed));
#endif

struct ethhdr {
	unsigned char              h_dest[ETH_ALEN];	/* destination eth addr	*/
	unsigned char              h_source[ETH_ALEN];	/* source ether addr	*/
	__be16                     h_proto;		/* packet type ID field	*/
} __attribute__((packed));

    
struct fence_header {
        unsigned int ver:2,
                     frag:1,
                     fragId:5,
                     fid:24; 
        unsigned char origDstMac[ETH_ALEN];
} __attribute__((packed));; 

#ifdef __KERNEL__
#include <linux/skbuff.h>

/**                                          
 *  eth_hdr - Return the ethernet structure
 *  @skb: Socket buffer
 *
 *  Typecasts the mac address in the socket buffer structure and returns it
 *  as an ethhdr structure. 
 *                                           
 *  RETURN VALUE:
 *  struct ethhdr *
 */                                          
/* _VMKLNX_CODECHECK_: eth_hdr */
static inline struct ethhdr *eth_hdr(const struct sk_buff *skb)
{
	return (struct ethhdr *)skb->mac.raw;
}

#ifdef CONFIG_SYSCTL
extern struct ctl_table ether_table[];
#endif

#if defined(__VMKLNX__)
/*
 *      Display a 6 byte device address (MAC) in a readable format.
 */
#define MAC_FMT "%02x:%02x:%02x:%02x:%02x:%02x"

/**                                          
 *  DECLARE_MAC_BUF - declarator for a MAC address
 *  @var: completion structure
 *
 *  SYNOPSIS:
 *      #define DECLARE_MAC_BUF(mac)
 *                                           
 *  Declares a 6 octet Media Access Control (MAC) Address named "var".
 *  "var" will contain space for an additional 12 octets (ordinarily
 *  unused).
 *                                           
 *  RETURN VALUE:
 *  Does not return any value
 */                                          
/* _VMKLNX_CODECHECK_: DECLARE_MAC_BUF */
#define DECLARE_MAC_BUF(var) char var[18] __maybe_unused

/**
 * print_mac - print an ethernet MAC address into a string
 * @buf: output string buffer
 * @addr: MAC address buffer
 *
 * Prints a 48-bit Media Access Control (MAC-48) address from
 * @addr to @buf.
 *
 * Starting from the first byte to the last byte in @addr,
 * each byte is printed in form of 2 hexadecimal digits,
 * separated by a colon (":"). The output format would look 
 * like this
 *
 * 	"@addr[0]:@addr[1]:@addr[2]:@addr[3]:@addr[4]:@addr[5]"
 *
 * RETURN VALUE:
 * @buf
 */
/* _VMKLNX_CODECHECK_: print_mac */
static inline char *print_mac(char *buf, const u8 *addr)
{
        sprintf(buf, MAC_FMT,
                addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
        return buf;
}
#endif /* defined(__VMKLNX__) */

#endif

#endif	/* _LINUX_IF_ETHER_H */
