/*
 * Copyright 2011 - 2014 VMware, Inc.  All rights reserved.
 * -- VMware Confidential
 *
 * Support for adding HW offload control information for enapsulated packets -
 * For eg. vdl2 and Geneve packets, where offset information for outer and
 * inner headers need to be passed to the driver via the skb
 *
 */

#ifndef _ENCAP_OFFLOAD_H_
#define _ENCAP_OFFLOAD_H_

typedef struct skb_encap_layout {
	u32 magic;
	u16 innerIpHdrOff;
	u16 innerL4Off;
        u8  innerL4Proto;
} skb_encap_layout_t;

#define SKB_ENCAP_MAGIC	0x76646c32	/* "vdl2" in ascii. */
#define VMKLNX_SKB_ENCAP_CB(__skb)	((struct skb_encap_layout *)&((__skb)->cb[16]))
#define VMKLNX_SKB_DECAP_CB(__skb)	((__skb)->cb[16]=0)


#define vmklnx_skb_encap_present(__skb) \
		  	(VMKLNX_SKB_ENCAP_CB(__skb)->magic == SKB_ENCAP_MAGIC)

#define vmklnx_skb_encap_get_inner_ip_hdr_offset(__skb) \
			(VMKLNX_SKB_ENCAP_CB(__skb)->innerIpHdrOff)

#define vmklnx_skb_encap_get_inner_l4_hdr_offset(__skb) \
			(VMKLNX_SKB_ENCAP_CB(__skb)->innerL4Off)

#define vmklnx_skb_encap_get_inner_l4_proto(__skb) \
			(VMKLNX_SKB_ENCAP_CB(__skb)->innerL4Proto)


#define SKB_GENEVE_MAGIC   0x47454e45  /* GENE in ascii. */
#define VMKLNX_SKB_GENEVE_ENCAP_CB(__skb)  VMKLNX_SKB_ENCAP_CB(__skb)
#define VMKLNX_SKB_GENEVE_DECAP_CB(__skb)  VMKLNX_SKB_DECAP_CB(__skb)

#define vmklnx_skb_geneve_present(__skb) \
   (VMKLNX_SKB_GENEVE_ENCAP_CB(__skb)->magic == SKB_GENEVE_MAGIC)

#define vmklnx_skb_geneve_get_inner_ip_hdr_offset(__skb) \
   vmklnx_skb_encap_get_inner_ip_hdr_offset(__skb)

#define vmklnx_skb_geneve_get_inner_l4_hdr_offset(__skb) \
   vmklnx_skb_encap_get_inner_l4_hdr_offset(__skb)

#define vmklnx_skb_geneve_get_inner_l4_proto(__skb) \
   vmklnx_skb_encap_get_inner_l4_proto(__skb)

#endif /* !(_ENCAP_OFFLOAD_H_) */
