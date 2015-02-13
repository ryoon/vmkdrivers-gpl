/*
 * Portions Copyright 2010 VMware, Inc.
 */
/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  NET  is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Definitions for the Ethernet handlers.
 *
 * Version:	@(#)eth.h	1.0.4	05/13/93
 *
 * Authors:	Ross Biro
 *		Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *
 *		Relocated to include/linux where it belongs by Alan Cox 
 *							<gw4pts@gw4pts.ampr.org>
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 *	WARNING: This move may well be temporary. This file will get merged with others RSN.
 *
 */
#ifndef _LINUX_ETHERDEVICE_H
#define _LINUX_ETHERDEVICE_H

#include <linux/if_ether.h>
#include <linux/netdevice.h>
#include <linux/random.h>
#if defined(__VMKLNX__)
#include "vmklinux_dist.h"
#endif

#ifdef __KERNEL__
extern int		eth_header(struct sk_buff *skb, struct net_device *dev,
				   unsigned short type, void *daddr,
				   void *saddr, unsigned len);
extern int		eth_rebuild_header(struct sk_buff *skb);
#if !defined(__VMKLNX__)
extern __be16		eth_type_trans(struct sk_buff *skb, struct net_device *dev);
#endif /* !defined(__VMKLNX__) */

extern void		eth_header_cache_update(struct hh_cache *hh, struct net_device *dev,
						unsigned char * haddr);
extern int		eth_header_cache(struct neighbour *neigh,
					 struct hh_cache *hh);

#if !defined(__VMKLNX__)
extern struct net_device *alloc_etherdev(int sizeof_priv);
#else
static inline struct net_device *alloc_etherdev_mq(int sizeof_priv,
                                                   unsigned int queue_count)
{
        return vmklnx_alloc_netdev_mq(THIS_MODULE,
	                              sizeof_priv,
				      "vmnic%d",
				      ether_setup,
				      queue_count);
}

/**
 *  alloc_etherdev - Allocates and sets up an ethernet device
 *  @sizeof_priv: size of additional driver-private structure to be allocated 
 *  for this ethernet device
 *
 *  Fill in the fields of the device structure with ethernet-generic values. 
 *  Basically does everything except registering the device. Constructs a new 
 *  net device, complete with a private data area of size sizeof_priv. A 
 *  32-byte (not bit) alignment is enforced for this private data area.
 *
 *  SYNOPSIS:
 *  #define alloc_etherdev(sizeof_priv)
 *
 *  RETURN VALUE:
 *  The pointer to the newly allocated net_device struct if successful,  otherwise NULL.
 *
 */
/* _VMKLNX_CODECHECK_: alloc_etherdev */
#define alloc_etherdev(sizeof_priv) alloc_etherdev_mq(sizeof_priv, 1)
#endif

static inline void eth_copy_and_sum (struct sk_buff *dest, 
				     const unsigned char *src, 
				     int len, int base)
{
	memcpy (dest->data, src, len);
}

/**
 * is_zero_ether_addr - Determine if give Ethernet address is all zeros.
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Return true if the address is all zeroes.
 */
/* _VMKLNX_CODECHECK_: is_zero_ether_addr */
static inline int is_zero_ether_addr(const u8 *addr)
{
	return !(addr[0] | addr[1] | addr[2] | addr[3] | addr[4] | addr[5]);
}

/**
 * is_multicast_ether_addr - Determine if the Ethernet address is a multicast.
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Return true if the address is a multicast address.
 * By definition the broadcast address is also a multicast address.
 */
static inline int is_multicast_ether_addr(const u8 *addr)
{
	return (0x01 & addr[0]);
}

/**
 * is_broadcast_ether_addr - Determine if the Ethernet address is broadcast
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Return true if the address is the broadcast address.
 */
static inline int is_broadcast_ether_addr(const u8 *addr)
{
	return (addr[0] & addr[1] & addr[2] & addr[3] & addr[4] & addr[5]) == 0xff;
}

/**
 * is_valid_ether_addr - Determine if the given Ethernet address is valid
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Check that the Ethernet address (MAC) is not 00:00:00:00:00:00, is not
 * a multicast address, and is not FF:FF:FF:FF:FF:FF.
 *
 * RETURN VALUE:
 *    1 if the address is valid.
 *    0 if the address is not valid.
 */
/* _VMKLNX_CODECHECK_: is_valid_ether_addr */
static inline int is_valid_ether_addr(const u8 *addr)
{
	/* FF:FF:FF:FF:FF:FF is a multicast address so we don't need to
	 * explicitly check for it here. */
	return !is_multicast_ether_addr(addr) && !is_zero_ether_addr(addr);
}

/**
 * random_ether_addr - Generate software assigned random Ethernet address
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Generate a random Ethernet address (MAC) that is not multicast
 * and has the local assigned bit set.
 */
static inline void random_ether_addr(u8 *addr)
{
	get_random_bytes (addr, ETH_ALEN);
	addr [0] &= 0xfe;	/* clear multicast bit */
	addr [0] |= 0x02;	/* set local assignment bit (IEEE802) */
}

/**
 * compare_ether_addr - Compare two Ethernet addresses
 * @addr1: Pointer to a six-byte array containing the Ethernet address
 * @addr2: Pointer other six-byte array containing the Ethernet address
 *
 * Compare two ethernet addresses, returns 0 if equal
 */
/* _VMKLNX_CODECHECK_: compare_ether_addr */
static inline unsigned compare_ether_addr(const u8 *addr1, const u8 *addr2)
{
	const u16 *a = (const u16 *) addr1;
	const u16 *b = (const u16 *) addr2;

	BUILD_BUG_ON(ETH_ALEN != 6);
	return ((a[0] ^ b[0]) | (a[1] ^ b[1]) | (a[2] ^ b[2])) != 0;
}

#if defined(__VMKLNX__)

#include <linux/if_vlan.h>

static inline char eth_header_type(struct ethhdr *eh)
{
	unsigned short type = ntohs(eh->h_proto);

        /*
         * we use 1536 (IEEE 802.3-std mentions 1536, but iana indicates
         * type of 0-0x5dc are 802.3) instead of some #def symbol to prevent
         * inadvertant reuse of the same macro for buffer size decls.
         */

	if (type >= 1536) {
		if (type != ETH_P_8021Q) {
			
			/*
			 * typical case
			 */
			
			return ETH_HEADER_TYPE_DIX;
		} 

		/*
		 * some type of 802.1pq tagged frame
		 */
		
		type = ntohs(((struct vlan_ethhdr *)eh)->h_vlan_encapsulated_proto);
		
		if (type >= 1536) {
			
			/*
			 * vlan tagging with dix style type
			 */
			
			return ETH_HEADER_TYPE_802_1PQ;
		}
		
		/*
		 * vlan tagging with 802.3 header
		 */
		
		return ETH_HEADER_TYPE_802_1PQ_802_3;
	}
	
	/*
	 * assume 802.3
	 */
	return ETH_HEADER_TYPE_802_3;
}

/**
 *  eth_header_len - Determines the length of the ethernet header for the given ethernet packet
 *  @eh: pointer to ethernet header
 *
 *  Determines the type of the given ethernet header and returns the header length based on the
 *  type.
 *
 *
 *  RETURN VALUE:
 *  The length of the header if able to determine or 0
 *
 */
/* _VMKLNX_CODECHECK_: eth_header_len */
static inline unsigned short eth_header_len(struct ethhdr *eh)
{
	char type = eth_header_type(eh);

	switch (type) {
		
	case ETH_HEADER_TYPE_DIX :
                return sizeof(struct ethhdr);
		
	case ETH_HEADER_TYPE_802_1PQ :
                return sizeof(struct vlan_ethhdr);
		
	case ETH_HEADER_TYPE_802_3 :
                return sizeof(struct ethhdr_llc);
		
	case ETH_HEADER_TYPE_802_1PQ_802_3 :
                return sizeof(struct vlan_ethhdr_llc);
	}

        return 0;
}

static inline unsigned short eth_header_frame_type(struct ethhdr *eh)
{
	char type = eth_header_type(eh);

	switch (type) {
		
	case ETH_HEADER_TYPE_DIX :
                return eh->h_proto;
		
	case ETH_HEADER_TYPE_802_1PQ :
                return ((struct vlan_ethhdr *)eh)->h_vlan_encapsulated_proto;

	case ETH_HEADER_TYPE_802_3 :
                return ((struct ethhdr_llc *)eh)->h_llc_snap_type;
		
	case ETH_HEADER_TYPE_802_1PQ_802_3 :
                return ((struct vlan_ethhdr_llc *)eh)->h_llc_snap_type;
	}

	return 0;
}

static inline int eth_header_is_ipv4(struct ethhdr *eh)
{
	return (eth_header_frame_type(eh) == ntohs(ETH_P_IP));
}

/**                                          
 *  eth_type_trans - determines the given packet's protocol ID
 *  @skb: received socket data
 *  @dev: receiving network device
 *
 *  Returns the protocol ID for the given packet
 *                                           
 *  RETURN VALUE:
 *  The protocol ID for the given packet
 */                                          
/* _VMKLNX_CODECHECK_: eth_type_trans */
static inline unsigned short eth_type_trans(struct sk_buff *skb, struct net_device *dev)
{
        skb->dev = dev;
        skb->mac.raw = skb->data;
        return eth_header_frame_type((struct ethhdr *) skb->data);
}


#endif

#endif	/* __KERNEL__ */

#endif	/* _LINUX_ETHERDEVICE_H */
