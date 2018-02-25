/*
 * Copyright (c) 2011-2013 Mellanox Technologies. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *	- Redistributions of source code must retain the above
 *	  copyright notice, this list of conditions and the following
 *	  disclaimer.
 *
 *	- Redistributions in binary form must reproduce the above
 *	  copyright notice, this list of conditions and the following
 *	  disclaimer in the documentation and/or other materials
 *	  provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#ifndef _MLNX_COMPAT_H
#define _MLNX_COMPAT_H

#if !defined(__VMKLNX__) || !defined(__LINUX_XHCI_HCD_H)

#include <linux/gfp.h>
#include <linux/workqueue.h>
#include <linux/if_vlan.h>
#include <linux/ethtool.h>
#include <linux/pci.h>
#include <linux/bitmap.h>
#include <linux/rbtree.h>
#include <linux/fs.h>


/************************************************************************/
#endif /* !defined(__VMKLNX__) || !defined(__LINUX_XHCI_HCD_H) */
#include "hash.h"
#if !defined(__VMKLNX__) || !defined(__LINUX_XHCI_HCD_H)


/************************************************************************/
/*
#define __VMKERNEL_BF_ENABLE__
#define __VMKERNEL_SYSFS_SUPPORT__
#define __VMKERNEL_LARGE_DMA_SEG_ENABLE__
#define __VMKERNEL_MLNX_SENSNE_ENABLE__
#define __VMKERNEL_HASH_EXPAND_ENABLE__
#define __VMKERNEL_NETLINK_SUPPORT__
#define __VMKERNEL_IPOIB_PACKET_DEBUG__
#define __VMKERNEL_MLX4_EN_SUPPORT__
#define __VMKERNEL_ETHTOOL_RSS_NUM_RINGS_SUPPORT__
#define __VMKERNEL_ETHTOOL_SELF_TEST_SUPPORT__
#define __VMKERNEL_TX_INLINE_WQE_ENABLE__
#define __VMKERNEL_NETDEV_SELECT_QUEUE_SUPPORT__
#define __VMKERNEL_NETDEV_ADVANCE_FEATURES_SUPPORT__
#define __VMKERNEL_NETDEV_GRO_FEATURE_SUPPORT__
#define __VMKERNEL_NETDEV_OPS_SET_FEATURES_SUPPORT__
#define __VMKERNEL_NETDEV_OPS_VALIDATE_ADDR_SUPPORT__
#define __MLNX_VMKERNEL_40G_SPEED_SUPPORT__
*/

/*
#define __MEMTRACK__
#define IPOIB_SHOW_ARP_DBG
*/

#ifndef CONFIG_MLX4_DEBUG
	#define CONFIG_MLX4_DEBUG
#endif

#define __VMKERNEL_MLX4_EN_TX_HASH__
#define __VMKERNEL_KOBJECT_FIX_BUG__

/*
 * Pseudo device defines -
 * In old DDKs NETIF_F_PSEUDO_REG not exists and
 * we try to workaround this.
 * In this case we also define __VMKERNEL_PSEUDO_OLD_API__
 * so we will know we are using old DDK
 */
#ifndef NETIF_F_PSEUDO_REG
	#define NETIF_F_PSEUDO_REG              0x10000000000
	#define __VMKERNEL_PSEUDO_OLD_API__
#endif  /* NETIF_F_PSEUDO_REG */

/*
 * Netqueue RSS for VXLAN defines
 * If vmkapi support RSS for VXLAN than we enable
 * this feature in netqueue code
 */
#if (VMKAPI_REVISION >= VMK_REVISION_FROM_NUMBERS(2, 1, 0, 0))
	#define __VMKERNEL_RSS_NETQ_SUPPORT__
#endif

#if (VMKAPI_REVISION >= VMK_REVISION_FROM_NUMBERS(2, 2, 0, 0))
	#define __MLNX_VMKERNEL_40G_SPEED_SUPPORT__
#endif


/************************************************************************/
/* Prints */
#ifndef KERN_ERR
#define KERN_ERR	""
#endif
#ifndef KERN_WARNING
#define KERN_WARNING	""
#endif
#ifndef KERN_CONT
#define KERN_CONT	""
#endif
#define pr_err(fmt, ...)	\
	printk(KERN_ERR pr_fmt(fmt), ##__VA_ARGS__)
#define pr_warning(fmt, ...)	\
	printk(KERN_WARNING pr_fmt(fmt), ##__VA_ARGS__)
#define pr_cont(fmt, ...)	\
	printk(KERN_CONT fmt, ##__VA_ARGS__)
#define printk_once(fmt, ...)			\
({						\
	static bool __print_once;		\
						\
	if (!__print_once) {			\
		__print_once = true;		\
		printk(fmt, ##__VA_ARGS__);	\
	}					\
})
#define pr_fmt(fmt) fmt


/************************************************************************/
/* Networking */

/* Netdev */
#define for_each_netdev_rcu(net, d)	\
		for ((d) = dev_base; (d);	\
			(d) = rcu_dereference(d)->next)

typedef int netdev_tx_t;

/* if_vlan.h */
#ifndef VLAN_N_VID
#define VLAN_N_VID	4096
#endif	/* VLAN_N_VID */

/* if_ether.h */
#ifndef ETH_FCS_LEN
#define ETH_FCS_LEN	4
#endif	/* NOT ETH_FCS_LEN */

/* skbuff.h */
static inline unsigned int skb_frag_size(const skb_frag_t *frag)
{
        return frag->size;
}

static inline void skb_frag_size_set(skb_frag_t *frag, unsigned int size)
{
	frag->size = size;
}

static inline void skb_frag_size_sub(skb_frag_t *frag, int delta)
{
	frag->size -= delta;
}

static inline struct page *skb_frag_page(const skb_frag_t *frag)
{
#ifdef __VMKERNEL_MODULE__
	return frag->page;
#else
	return frag->page.p;
#endif	/* __VMKERNEL_MODULE__ */
}

static inline void __skb_frag_set_page(skb_frag_t *frag, struct page *page)
{
#ifdef __VMKERNEL_MODULE__
	frag->page = page;
#else
	frag->page.p = page;
#endif	/* __VMKERNEL_MODULE__ */
}

static inline void skb_frag_set_page(struct sk_buff *skb, int f,
		struct page *page)
{
	__skb_frag_set_page(&skb_shinfo(skb)->frags[f], page);
}

static inline dma_addr_t skb_frag_dma_map(struct device *dev,
		const skb_frag_t *frag,
		size_t offset, size_t size,
		enum dma_data_direction dir)
{
	return dma_map_page(dev, skb_frag_page(frag),
			frag->page_offset + offset, size, dir);
}

static inline void __skb_frag_unref(skb_frag_t *frag)
{
	put_page(skb_frag_page(frag));
}

static inline void skb_frag_unref(struct sk_buff *skb, int f)
{
	__skb_frag_unref(&skb_shinfo(skb)->frags[f]);
}

static inline void skb_record_rx_queue(struct sk_buff *skb, u16 rx_queue)
{
	vmknetddi_queueops_set_skb_queueid(skb,
			VMKNETDDI_QUEUEOPS_MK_RX_QUEUEID(rx_queue));
}


/************************************************************************/
/* Some defines and types */
typedef unsigned long phys_addr_t;

#define num_possible_cpus() smp_num_cpus
#define __ALIGN_KERNEL_MASK(x, mask)    (((x) + (mask)) & ~(mask))
#define __ALIGN_MASK(x, mask)   __ALIGN_KERNEL_MASK((x), (mask))
#define for_each_set_bit(bit, addr, size)		\
	for ((bit) = find_first_bit((addr), (size));	\
	(bit) < (size); \
	(bit) = find_next_bit((addr), (size), (bit) + 1))

#define __packed __attribute__((packed))
#define uninitialized_var(x) x = x

#define iowrite32(v, addr)	writel((v), (addr))
#define iowrite32be(v, addr)	iowrite32(be32_to_cpu(v), (addr))


/************************************************************************/
/* VLANs */
#define vlan_dev_vlan_id(netdev) (VLAN_DEV_INFO(netdev)->vlan_id)
#define vlan_dev_real_dev(netdev) (VLAN_DEV_INFO(netdev)->real_dev)


/************************************************************************/
/* DMA */
/* In vmkernel we don't support dma attributes */
struct dma_attrs;

#define dma_map_single_attrs(dev, cpu_addr, size, dir, attrs)	\
	dma_map_single(dev, cpu_addr, size, dir)

#define dma_unmap_single_attrs(dev, dma_addr, size, dir, attrs)	\
	dma_unmap_single(dev, dma_addr, size, dir)

#define dma_map_sg_attrs(dev, sgl, nents, dir, attrs)	\
	dma_map_sg(dev, sgl, nents, dir)

#define dma_unmap_sg_attrs(dev, sgl, nents, dir, attrs)	\
	dma_unmap_sg(dev, sgl, nents, dir)


/************************************************************************/
/* PCI */
static inline int pci_pcie_cap(struct pci_dev *pdev)
{
	return pci_find_capability(pdev, PCI_CAP_ID_EXP);
}

/*
 * DEFINE_PCI_DEVICE_TABLE - macro used to describe a pci device table
 * @_table: device table name
 *
 * This macro is used to create a struct pci_device_id array (a device table)
 * in a generic manner.
 */
#define DEFINE_PCI_DEVICE_TABLE(_table)	\
	const struct pci_device_id _table[]

/*
 * PCI_VDEVICE - macro used to describe a specific pci device in short form
 * @vendor: the vendor name
 * @device: the 16 bit PCI Device ID
 *
 * This macro is used to create a struct pci_device_id that matches a
 * specific PCI device.  The subvendor, and subdevice fields will be set
 * to PCI_ANY_ID. The macro allows the next field to follow as the device
 * private data.
 */
#define PCI_VDEVICE(vendor, device)		\
	PCI_VENDOR_ID_##vendor, (device),	\
	PCI_ANY_ID, PCI_ANY_ID, 0, 0


/************************************************************************/
/* Bitmap */
#define BITMAP_FIRST_WORD_MASK(start) (~0UL << ((start) % BITS_PER_LONG))

unsigned long bitmap_find_next_zero_area(unsigned long *map,
		unsigned long size,
		unsigned long start,
		unsigned int nr,
		unsigned long align_mask);
void bitmap_set(unsigned long *map, int start, int nr);
void bitmap_clear(unsigned long *map, int start, int nr);


/************************************************************************/
#endif /* !defined(__VMKLNX__) || !defined(__LINUX_XHCI_HCD_H) */
/* Radix tree */
#define radix_tree_root hash_table
#define radix_tree_lookup mlnx_radix_tree_lookup
#define radix_tree_insert mlnx_radix_tree_insert
#define radix_tree_delete mlnx_radix_tree_delete

#ifdef INIT_RADIX_TREE
#undef INIT_RADIX_TREE
#endif

#define INIT_RADIX_TREE(p_tree, flags) do {	\
	memset((p_tree), 0, sizeof(struct hash_table));	\
} while (0);


static inline void * mlnx_radix_tree_lookup(struct radix_tree_root *p_tree,
		unsigned long index) {
	return hash_lookup(p_tree, index);
}

static inline int mlnx_radix_tree_insert(struct radix_tree_root *p_tree,
		unsigned long index,
		void *p_data) {
	return hash_insert(p_tree, index, p_data);
}

static inline void * mlnx_radix_tree_delete(struct radix_tree_root *p_tree,
		unsigned long index) {
	return hash_delete(p_tree, index);
}

#if !defined(__VMKLNX__) || !defined(__LINUX_XHCI_HCD_H)

/************************************************************************/
/* Ethtool */
int __ethtool_get_settings(struct net_device *dev, struct ethtool_cmd *p_cmd);
void ethtool_cmd_speed_set(struct ethtool_cmd *ep, __u32 speed);
__u32 ethtool_cmd_speed(const struct ethtool_cmd *ep);


/************************************************************************/
/* Hex dump */
int hex_to_bin(char ch);


/************************************************************************/
/* Pages */
unsigned long get_zeroed_page(gfp_t gfp_mask);


/************************************************************************/
/* Work Queues */
#define alloc_workqueue(name, flags, max_active)	\
		__create_workqueue((name), (max_active))

int __cancel_delayed_work(struct delayed_work *dwork);

static inline struct delayed_work *to_delayed_work(struct work_struct *work)
{
	return container_of(work, struct delayed_work, work);
}


/************************************************************************/
/* Mem */
/*
 * kmemdup - duplicate region of memory
 *
 * @src: memory region to duplicate
 * @len: memory region length
 * @gfp: GFP mask to use
 */
void *kmemdup(const void *src, size_t len, gfp_t gfp);


/************************************************************************/
/* File */
int nonseekable_open(struct inode *inode, struct file *filp);


#endif /* !defined(__VMKLNX__) || !defined(__LINUX_XHCI_HCD_H) */
#endif	/* _MLNX_COMPAT_H */
