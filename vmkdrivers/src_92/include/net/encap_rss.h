/*
 * Copyright 2012 VMware, Inc.  All rights reserved.
 * -- VMware Confidential
 *
 * Support for passing rss hash and type from driver to vmklinux and vice
 * versa.
 *
 */

#ifndef _ENCAP_RSS_H_
#define _ENCAP_RSS_H_

typedef enum vmklnx_rss_type {
   /** RSS hash type not set */
   VMKLNX_PKT_RSS_TYPE_NONE            = 0x00000000,

   /**
    * RSS hash based on IPv4 source/destination addresses and
    * TCP source/destination ports
    */
   VMKLNX_PKT_RSS_TYPE_IPV4_TCP        = 0x00000001,

   /** RSS hash based on IPv4 source/destination addresses */
   VMKLNX_PKT_RSS_TYPE_IPV4            = 0x00000002,

   /**
    * RSS hash based on IPv6 source/destination addresses and
    * TCP source/destination ports
    */
   VMKLNX_PKT_RSS_TYPE_IPV6_TCP        = 0x00000003,

   /** RSS hash based on IPv6 source/destination addresses and extensions */
   VMKLNX_PKT_RSS_TYPE_IPV6_EX         = 0x00000004,

   /** RSS hash based on IPv6 source/destination addresses */
   VMKLNX_PKT_RSS_TYPE_IPV6            = 0x00000005,

   /**
    * RSS hash based on IPv6 source/destination addresses, extensions
    * and TCP source/destination ports
    */
   VMKLNX_PKT_RSS_TYPE_IPV6_TCP_EX     = 0x00000006,

   /**
    * RSS hash based on IPv4 source/destination addresses and
    * UDP source/destination ports
    */
   VMKLNX_PKT_RSS_TYPE_IPV4_UDP        = 0x00000007,

   /**
    * RSS hash based on IPv6 source/destination addresses and
    * UDP source/destination ports
    */
   VMKLNX_PKT_RSS_TYPE_IPV6_UDP        = 0x00000008,

   /**
    * RSS hash based on IPv6 source/destination addresses, extensions
    * and UDP source/destination ports
    */
   VMKLNX_PKT_RSS_TYPE_IPV6_UDP_EX     = 0x00000009,
} vmklnx_rss_type;

typedef struct skb_rss_info {
   u32              rss_magic;
   u32              rss_hash;
   vmklnx_rss_type  rss_type;
} skb_rss_info;

#define SKB_RSS_INFO_MAGIC 0x725373 /* "rSs" in ascii. */
#define RSS_SKB_CB(__skb)	((skb_rss_info *)&((__skb)->cb[25]))
#define SKB_HAS_RSS_INFO(__skb)  \
   (RSS_SKB_CB(__skb)->rss_magic == SKB_RSS_INFO_MAGIC)


/**
 * rss_skb_put_info - RSS hash + type inserting
 *
 * @skb:          skbuff to set rss info
 * @hash:         rss hash value
 * @rss_type:     rss type
 *
 * Puts the RSS hash and type in @skb->cb[]
 */

static inline void
rss_skb_put_info(struct sk_buff *skb,
                 u32 hash,
                 vmklnx_rss_type rss_type)
{
   skb_rss_info *rss_info = RSS_SKB_CB(skb);

   rss_info->rss_magic = SKB_RSS_INFO_MAGIC;
   rss_info->rss_hash = hash;
   rss_info->rss_type = rss_type;
}


/**
 * rss_skb_get_info - RSS hash + type fetching
 *
 * @skb:          skbuff to set rss info
 * @hash:         rss hash value
 * @rss_type:     rss type
 *
 * Gets the RSS hash and type from @skb->cb[]
 */

static inline int
rss_skb_get_info(struct sk_buff *skb,
                 u32 *hash,
                 vmklnx_rss_type *rss_type)
{
   skb_rss_info *rss_info = RSS_SKB_CB(skb);

   if (rss_info->rss_magic == SKB_RSS_INFO_MAGIC) {
      *hash = rss_info->rss_hash;
      *rss_type = rss_info->rss_type;
      return 0;
   } else {
      return -EINVAL;
   }
}
#endif /* _ENCAP_RSS_H_ */
