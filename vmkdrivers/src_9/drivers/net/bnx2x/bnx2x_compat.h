/* bnx2x_compat.h: Broadcom Everest network driver.
 *
 * Copyright 2007-2011 Broadcom Corporation
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
 */
#ifndef __BNX2X_COMPAT_H__
#define __BNX2X_COMPAT_H__

#ifndef __VMKLNX__
#define VMWARE_ESX_DDK_VERSION		0
#define __COMPAT_LAYER_2_6_18_PLUS__	0
#define __USE_COMPAT_LAYER_2_6_18_PLUS__ 0
#endif

#include <linux/netdevice.h>
#include <linux/delay.h>

#if (LINUX_VERSION_CODE < 0x02061D)
#include <linux/pci.h> /* for vpd */
#endif

#include <linux/in.h>
#include <linux/in6.h>

#if (LINUX_VERSION_CODE < 0x02061e)
#define skb_record_rx_queue(skb, index)

#ifdef BNX2X_MULTI_QUEUE
#include <net/sock.h>
#if !defined(__VMKLNX__)
#include <linux/jhash.h>
#else
/**
 *  Taken from linux/jhash.h
 */

#define __jhash_mix(a, b, c) \
{ \
  a -= b; a -= c; a ^= (c>>13); \
  b -= c; b -= a; b ^= (a<<8); \
  c -= a; c -= b; c ^= (b>>13); \
  a -= b; a -= c; a ^= (c>>12);  \
  b -= c; b -= a; b ^= (a<<16); \
  c -= a; c -= b; c ^= (b>>5); \
  a -= b; a -= c; a ^= (c>>3);  \
  b -= c; b -= a; b ^= (a<<10); \
  c -= a; c -= b; c ^= (b>>15); \
}

/* The golden ration: an arbitrary value */
#define JHASH_GOLDEN_RATIO      0x9e3779b9

/* A special ultra-optimized versions that knows they are hashing exactly
 * 3, 2 or 1 word(s).
 *
 * NOTE: In partilar the "c += length; __jhash_mix(a,b,c);" normally
 *       done at the end is not done here.
 */
static inline u32 jhash_3words(u32 a, u32 b, u32 c, u32 initval)
{
	a += JHASH_GOLDEN_RATIO;
	b += JHASH_GOLDEN_RATIO;
	c += initval;

	__jhash_mix(a, b, c);

	return c;
}

static inline u32 jhash_1word(u32 a, u32 initval)
{
	return jhash_3words(a, 0, 0, initval);
}
#endif
#include <linux/random.h>
static u32 bnx2x_skb_tx_hashrnd;
static inline u16 skb_tx_hash(const struct net_device *dev,
			      const struct sk_buff *skb)
{
	u32 hash;

#if !defined(__VMKLNX__)
	if (skb->sk && skb->sk->sk_hash)
		hash = skb->sk->sk_hash;
	else
#endif
		hash = skb->protocol;

	hash = jhash_1word(hash, bnx2x_skb_tx_hashrnd);

	return (u16) (((u64) hash * dev->real_num_tx_queues) >> 32);
}
#endif
#endif

#if (LINUX_VERSION_CODE < 0x020618) && (VMWARE_ESX_DDK_VERSION < 40000) && !defined(NETIF_F_GRO)
#define napi_complete(napi)		netif_rx_complete(dev)
#endif
#if (LINUX_VERSION_CODE < 0x020618) && (VMWARE_ESX_DDK_VERSION < 40000)
#define napi_schedule(dev)		netif_rx_schedule(dev)
#endif

#if (!defined(BNX2X_NEW_NAPI)) && defined(NETIF_F_GRO)
#define napi_complete(napi)		do {				  \
						napi_gro_flush(&fp->napi);\
						netif_rx_complete(dev);	  \
					} while(0)
#endif

#ifndef NETIF_F_GRO
#define napi_gro_receive(napi, skb) netif_receive_skb(skb)
#define vlan_gro_receive(napi, vlgrp, vlan, skb) \
				vlan_hwaccel_receive_skb(skb, vlgrp, vlan)
#endif

#ifndef BNX2X_MULTI_QUEUE
#define netif_tx_wake_all_queues	netif_wake_queue
#define netif_tx_start_all_queues	netif_start_queue
#endif

#if (LINUX_VERSION_CODE < 0x020616)
#define skb_copy_from_linear_data_offset(skb, pad, new_skb_data, len) \
				memcpy(new_skb_data, skb->data + pad, len)

/* skb_buff accessors */
#define ip_hdr(skb)			(skb)->nh.iph
#define ipv6_hdr(skb)			(skb)->nh.ipv6h
#define ip_hdrlen(skb)			(ip_hdr(skb)->ihl * 4)
#define tcp_hdr(skb)			(skb)->h.th
#define tcp_hdrlen(skb)			(tcp_hdr(skb)->doff * 4)
#define udp_hdr(skb)			(skb)->h.uh
#define skb_mac_header(skb)		((skb)->mac.raw)
#define skb_network_header(skb)		((skb)->nh.raw)
#define skb_transport_header(skb)	((skb)->h.raw)
#endif


#ifndef CHECKSUM_PARTIAL
#define CHECKSUM_PARTIAL		CHECKSUM_HW
#endif


#if (LINUX_VERSION_CODE < 0x020600)
#define might_sleep()

#define num_online_cpus()		1

#define dev_info(dev, format, args...) \
				printk(KERN_INFO "bnx2x: " format, ##args)

#define dev_err(dev, format, args...) \
				printk(KERN_ERR "bnx2x: " format, ##args)

static inline int dma_mapping_error(dma_addr_t mapping)
{
	return 0;
}

#define synchronize_irq(X)		synchronize_irq()
#define flush_scheduled_work()
#endif


#ifndef SET_MODULE_OWNER
#define SET_MODULE_OWNER(dev)
#endif


#if (LINUX_VERSION_CODE < 0x020604)
#define MODULE_VERSION(version)
#endif


#if (LINUX_VERSION_CODE < 0x020605)
static inline void pci_dma_sync_single_for_device(struct pci_dev *dev,
						  dma_addr_t map, size_t size,
						  int dir)
{
}
#endif


#if (LINUX_VERSION_CODE < 0x020547)
#define pci_set_consistent_dma_mask(X, Y)	(0)
#endif


#if (LINUX_VERSION_CODE < 0x020607)
#define msleep(x) \
	do { \
		current->state = TASK_UNINTERRUPTIBLE; \
		schedule_timeout((HZ * (x)) / 1000); \
	} while (0)

#ifndef ADVERTISE_1000XPAUSE
static inline struct mii_ioctl_data *if_mii(struct ifreq *rq)
{
	return (struct mii_ioctl_data *)&rq->ifr_ifru;
}
#endif

#define pci_enable_msix(X, Y, Z)	(-1)
#endif


#if (LINUX_VERSION_CODE < 0x020609)
#define msleep_interruptible(x) \
	do{ \
		current->state = TASK_INTERRUPTIBLE; \
		schedule_timeout((HZ * (x)) / 1000); \
	} while (0)

#endif


#if (LINUX_VERSION_CODE < 0x02060b)
#define pm_message_t			u32
#define pci_power_t			u32
#define PCI_D0				0
#define PCI_D3hot			3
#define pci_choose_state(pdev, state)	state
#endif


#if (LINUX_VERSION_CODE < 0x02060e)
#define touch_softlockup_watchdog()
#endif


#if (LINUX_VERSION_CODE < 0x020612)
static inline struct sk_buff *netdev_alloc_skb(struct net_device *dev,
					       unsigned int length)
{
	struct sk_buff *skb = dev_alloc_skb(length);

	if (skb)
		skb->dev = dev;
	return skb;
}
#endif


#if (LINUX_VERSION_CODE < 0x020614)
#define PCI_VDEVICE(vendor, device)             \
	PCI_VENDOR_ID_##vendor, (device),       \
	PCI_ANY_ID, PCI_ANY_ID, 0, 0
#endif

#if (LINUX_VERSION_CODE < 0x020615)
#define vlan_group_set_device(vg, vlan_id, dev)	vg->vlan_devices[vlan_id] = dev
#endif


#ifndef IRQ_RETVAL
typedef void				irqreturn_t;
#define IRQ_HANDLED
#define IRQ_NONE
#endif


#ifndef IRQF_SHARED
#define IRQF_SHARED			SA_SHIRQ
#endif


#ifndef NETIF_F_GSO
static inline void netif_tx_lock(struct net_device *dev)
{
	spin_lock(&dev->xmit_lock);
	dev->xmit_lock_owner = smp_processor_id();
}

static inline void netif_tx_unlock(struct net_device *dev)
{
	dev->xmit_lock_owner = -1;
	spin_unlock(&dev->xmit_lock);
}

static inline void netif_tx_lock_bh(struct net_device *dev)
{
	spin_lock_bh(&dev->xmit_lock);
	dev->xmit_lock_owner = smp_processor_id();
}

static inline void netif_tx_unlock_bh(struct net_device *dev)
{
	dev->xmit_lock_owner = -1;
	spin_unlock_bh(&dev->xmit_lock);
}
#endif

#ifndef list_first_entry
/**
 * list_first_entry - get the first element from a list
 * @ptr:	the list head to take the element from.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_struct within the struct.
 *
 * Note, that list is expected to be not empty.
 */
#define list_first_entry(ptr, type, member) \
	list_entry((ptr)->next, type, member)
#endif


#ifndef skb_shinfo
#define skb_shinfo(SKB)	((struct skb_shared_info *)(skb_end_pointer(SKB)))
#endif


#ifdef NETIF_F_TSO
#ifndef NETIF_F_GSO

static inline int skb_is_gso(const struct sk_buff *skb)
{
	return skb_shinfo(skb)->tso_size;
}

#define gso_size			tso_size

#endif /* NETIF_F_GSO */

#ifndef NETIF_F_GSO_SOFTWARE
#define NETIF_F_GSO_SOFTWARE		(NETIF_F_TSO)
#endif

#endif /* NETIF_F_TSO */

#ifndef NETIF_F_TSO_ECN
#define NETIF_F_TSO_ECN			0
#endif

#ifndef NEXTHDR_IPV6
#define NEXTHDR_IPV6            41      /* IPv6 in IPv6 */
#endif /* NEXTHDR_IPV6 */

#if !defined(mmiowb)
#define mmiowb()
#endif

#if !defined(__iomem)
#define __iomem
#endif

#ifndef noinline
#define noinline
#endif

#if !defined(INIT_WORK)
#define INIT_WORK INIT_TQUEUE
#define schedule_work			schedule_task
#define work_struct			tq_struct
#endif

#if !defined(HAVE_NETDEV_PRIV) && (LINUX_VERSION_CODE != 0x020603) && (LINUX_VERSION_CODE != 0x020604) && (LINUX_VERSION_CODE != 0x020605)
#define netdev_priv(dev)		(dev)->priv
#endif

/* Missing defines */
#ifndef SPEED_2500
#define SPEED_2500			2500
#endif

#ifndef SUPPORTED_Pause
#define SUPPORTED_Pause			(1 << 13)
#endif
#ifndef SUPPORTED_Asym_Pause
#define SUPPORTED_Asym_Pause		(1 << 14)
#endif

#ifndef ADVERTISED_Pause
#define ADVERTISED_Pause		(1 << 13)
#endif

#ifndef ADVERTISED_Asym_Pause
#define ADVERTISED_Asym_Pause		(1 << 14)
#endif

#ifndef NETDEV_TX_OK
#define NETDEV_TX_OK			0 /* driver took care of packet */
#endif

#ifndef DMA_BIT_MASK
#define DMA_BIT_MASK(n)		(((n) == 64) ? ~0ULL : ((1ULL<<(n)) - 1))
#endif

#ifndef PCI_CAP_ID_EXP
#define PCI_CAP_ID_EXP			0x10
#endif

#ifndef PCI_EXP_DEVCTL
#define PCI_EXP_DEVCTL			8	/* Device Control */
#endif

#ifndef PCI_EXP_DEVCTL_PAYLOAD
#define PCI_EXP_DEVCTL_PAYLOAD		0x00e0	/* Max_Payload_Size */
#endif

#ifndef PCI_EXP_DEVCTL_READRQ
#define PCI_EXP_DEVCTL_READRQ		0x7000	/* Max_Read_Request_Size */
#endif

#ifndef ETH_P_FCOE
#define ETH_P_FCOE			0x8906
#endif

#ifndef ETH_P_FIP
#define ETH_P_FIP			0x8914
#endif

#if (LINUX_VERSION_CODE < 0x020618)

#ifndef NETIF_F_HW_CSUM
#define NETIF_F_HW_CSUM			8
#endif

static inline int bnx2x_set_tx_hw_csum(struct net_device *dev, u32 data)
{
	if (data)
		dev->features |= NETIF_F_HW_CSUM;
	else
		dev->features &= ~NETIF_F_HW_CSUM;
	return 0;
}
#endif


/* If mutex is not available, use semaphore */
#ifndef __LINUX_MUTEX_H
#define mutex				semaphore
#define mutex_lock(x)			down(x)
#define mutex_unlock(x)			up(x)
#define mutex_init(x)			sema_init(x,1)
#endif


#ifndef KERN_CONT
#define KERN_CONT			""
#endif


#if (LINUX_VERSION_CODE < 0x020619)
#define le16_add_cpu(var, val) *var = cpu_to_le16(le16_to_cpup(var) + val)
#define le32_add_cpu(var, val) *var = cpu_to_le32(le32_to_cpup(var) + val)
#endif

#if (LINUX_VERSION_CODE < 0x020620)
/* Driver transmit return codes */
#undef NETDEV_TX_OK
#undef NETDEV_TX_BUSY
#undef NETDEV_TX_LOCKED
enum netdev_tx {
	NETDEV_TX_OK = 0,	/* driver took care of packet */
	NETDEV_TX_BUSY,		/* driver tx path was busy*/
	NETDEV_TX_LOCKED = -1,	/* driver tx lock was already taken */
};
typedef enum netdev_tx netdev_tx_t;
#endif

#if (LINUX_VERSION_CODE < 0x02061b) || defined(BNX2X_DRIVER_DISK) || defined(__VMKLNX__)

/*
 * This is the CRC-32C table
 * Generated with:
 * width = 32 bits
 * poly = 0x1EDC6F41
 * reflect input bytes = true
 * reflect output bytes = true
 */

static u32 crc32c_table[256] = {
	0x00000000L, 0xF26B8303L, 0xE13B70F7L, 0x1350F3F4L,
	0xC79A971FL, 0x35F1141CL, 0x26A1E7E8L, 0xD4CA64EBL,
	0x8AD958CFL, 0x78B2DBCCL, 0x6BE22838L, 0x9989AB3BL,
	0x4D43CFD0L, 0xBF284CD3L, 0xAC78BF27L, 0x5E133C24L,
	0x105EC76FL, 0xE235446CL, 0xF165B798L, 0x030E349BL,
	0xD7C45070L, 0x25AFD373L, 0x36FF2087L, 0xC494A384L,
	0x9A879FA0L, 0x68EC1CA3L, 0x7BBCEF57L, 0x89D76C54L,
	0x5D1D08BFL, 0xAF768BBCL, 0xBC267848L, 0x4E4DFB4BL,
	0x20BD8EDEL, 0xD2D60DDDL, 0xC186FE29L, 0x33ED7D2AL,
	0xE72719C1L, 0x154C9AC2L, 0x061C6936L, 0xF477EA35L,
	0xAA64D611L, 0x580F5512L, 0x4B5FA6E6L, 0xB93425E5L,
	0x6DFE410EL, 0x9F95C20DL, 0x8CC531F9L, 0x7EAEB2FAL,
	0x30E349B1L, 0xC288CAB2L, 0xD1D83946L, 0x23B3BA45L,
	0xF779DEAEL, 0x05125DADL, 0x1642AE59L, 0xE4292D5AL,
	0xBA3A117EL, 0x4851927DL, 0x5B016189L, 0xA96AE28AL,
	0x7DA08661L, 0x8FCB0562L, 0x9C9BF696L, 0x6EF07595L,
	0x417B1DBCL, 0xB3109EBFL, 0xA0406D4BL, 0x522BEE48L,
	0x86E18AA3L, 0x748A09A0L, 0x67DAFA54L, 0x95B17957L,
	0xCBA24573L, 0x39C9C670L, 0x2A993584L, 0xD8F2B687L,
	0x0C38D26CL, 0xFE53516FL, 0xED03A29BL, 0x1F682198L,
	0x5125DAD3L, 0xA34E59D0L, 0xB01EAA24L, 0x42752927L,
	0x96BF4DCCL, 0x64D4CECFL, 0x77843D3BL, 0x85EFBE38L,
	0xDBFC821CL, 0x2997011FL, 0x3AC7F2EBL, 0xC8AC71E8L,
	0x1C661503L, 0xEE0D9600L, 0xFD5D65F4L, 0x0F36E6F7L,
	0x61C69362L, 0x93AD1061L, 0x80FDE395L, 0x72966096L,
	0xA65C047DL, 0x5437877EL, 0x4767748AL, 0xB50CF789L,
	0xEB1FCBADL, 0x197448AEL, 0x0A24BB5AL, 0xF84F3859L,
	0x2C855CB2L, 0xDEEEDFB1L, 0xCDBE2C45L, 0x3FD5AF46L,
	0x7198540DL, 0x83F3D70EL, 0x90A324FAL, 0x62C8A7F9L,
	0xB602C312L, 0x44694011L, 0x5739B3E5L, 0xA55230E6L,
	0xFB410CC2L, 0x092A8FC1L, 0x1A7A7C35L, 0xE811FF36L,
	0x3CDB9BDDL, 0xCEB018DEL, 0xDDE0EB2AL, 0x2F8B6829L,
	0x82F63B78L, 0x709DB87BL, 0x63CD4B8FL, 0x91A6C88CL,
	0x456CAC67L, 0xB7072F64L, 0xA457DC90L, 0x563C5F93L,
	0x082F63B7L, 0xFA44E0B4L, 0xE9141340L, 0x1B7F9043L,
	0xCFB5F4A8L, 0x3DDE77ABL, 0x2E8E845FL, 0xDCE5075CL,
	0x92A8FC17L, 0x60C37F14L, 0x73938CE0L, 0x81F80FE3L,
	0x55326B08L, 0xA759E80BL, 0xB4091BFFL, 0x466298FCL,
	0x1871A4D8L, 0xEA1A27DBL, 0xF94AD42FL, 0x0B21572CL,
	0xDFEB33C7L, 0x2D80B0C4L, 0x3ED04330L, 0xCCBBC033L,
	0xA24BB5A6L, 0x502036A5L, 0x4370C551L, 0xB11B4652L,
	0x65D122B9L, 0x97BAA1BAL, 0x84EA524EL, 0x7681D14DL,
	0x2892ED69L, 0xDAF96E6AL, 0xC9A99D9EL, 0x3BC21E9DL,
	0xEF087A76L, 0x1D63F975L, 0x0E330A81L, 0xFC588982L,
	0xB21572C9L, 0x407EF1CAL, 0x532E023EL, 0xA145813DL,
	0x758FE5D6L, 0x87E466D5L, 0x94B49521L, 0x66DF1622L,
	0x38CC2A06L, 0xCAA7A905L, 0xD9F75AF1L, 0x2B9CD9F2L,
	0xFF56BD19L, 0x0D3D3E1AL, 0x1E6DCDEEL, 0xEC064EEDL,
	0xC38D26C4L, 0x31E6A5C7L, 0x22B65633L, 0xD0DDD530L,
	0x0417B1DBL, 0xF67C32D8L, 0xE52CC12CL, 0x1747422FL,
	0x49547E0BL, 0xBB3FFD08L, 0xA86F0EFCL, 0x5A048DFFL,
	0x8ECEE914L, 0x7CA56A17L, 0x6FF599E3L, 0x9D9E1AE0L,
	0xD3D3E1ABL, 0x21B862A8L, 0x32E8915CL, 0xC083125FL,
	0x144976B4L, 0xE622F5B7L, 0xF5720643L, 0x07198540L,
	0x590AB964L, 0xAB613A67L, 0xB831C993L, 0x4A5A4A90L,
	0x9E902E7BL, 0x6CFBAD78L, 0x7FAB5E8CL, 0x8DC0DD8FL,
	0xE330A81AL, 0x115B2B19L, 0x020BD8EDL, 0xF0605BEEL,
	0x24AA3F05L, 0xD6C1BC06L, 0xC5914FF2L, 0x37FACCF1L,
	0x69E9F0D5L, 0x9B8273D6L, 0x88D28022L, 0x7AB90321L,
	0xAE7367CAL, 0x5C18E4C9L, 0x4F48173DL, 0xBD23943EL,
	0xF36E6F75L, 0x0105EC76L, 0x12551F82L, 0xE03E9C81L,
	0x34F4F86AL, 0xC69F7B69L, 0xD5CF889DL, 0x27A40B9EL,
	0x79B737BAL, 0x8BDCB4B9L, 0x988C474DL, 0x6AE7C44EL,
	0xBE2DA0A5L, 0x4C4623A6L, 0x5F16D052L, 0xAD7D5351L
};

/*
 * Steps through buffer one byte at at time, calculates reflected
 * crc using table.
 */

static inline u32 /*__attribute_pure__*/
crc32c_le(u32 seed, unsigned char const *data, size_t length)
{
	__le32 crc = __cpu_to_le32(seed);

	while (length--)
		crc = crc32c_table[(crc ^ *data++) & 0xFFL] ^ (crc >> 8);

	return __le32_to_cpu(crc);
}
#endif

/* Taken from drivers/net/mdio.c */
#if (LINUX_VERSION_CODE < 0x02061f)
#include <linux/mii.h>

/* MDIO Manageable Devices (MMDs). */
#define MDIO_MMD_AN		7	/* Auto-Negotiation */

/* Generic MDIO registers. */
#define MDIO_AN_ADVERTISE	16	/* AN advertising (base page) */
#define MDIO_AN_LPA		19	/* AN LP abilities (base page) */

/* Device present registers. */
#define MDIO_DEVS_PRESENT(devad)	(1 << (devad))
#define MDIO_DEVS_AN			MDIO_DEVS_PRESENT(MDIO_MMD_AN)

/**
 * struct mdio_if_info - Ethernet controller MDIO interface
 * @prtad: PRTAD of the PHY (%MDIO_PRTAD_NONE if not present/unknown)
 * @mmds: Mask of MMDs expected to be present in the PHY.  This must be
 *	non-zero unless @prtad = %MDIO_PRTAD_NONE.
 * @mode_support: MDIO modes supported.  If %MDIO_SUPPORTS_C22 is set then
 *	MII register access will be passed through with @devad =
 *	%MDIO_DEVAD_NONE.  If %MDIO_EMULATE_C22 is set then access to
 *	commonly used clause 22 registers will be translated into
 *	clause 45 registers.
 * @dev: Net device structure
 * @mdio_read: Register read function; returns value or negative error code
 * @mdio_write: Register write function; returns 0 or negative error code
 */
struct mdio_if_info {
	int prtad;
	u32 __bitwise mmds;
	unsigned mode_support;

	struct net_device *dev;
	int (*mdio_read)(struct net_device *dev, int prtad, int devad,
			 u16 addr);
	int (*mdio_write)(struct net_device *dev, int prtad, int devad,
			  u16 addr, u16 val);
};

#define MDIO_PRTAD_NONE			(-1)
#define MDIO_DEVAD_NONE			(-1)
#define MDIO_EMULATE_C22		4

/* Mapping between MDIO PRTAD/DEVAD and mii_ioctl_data::phy_id */

#define MDIO_PHY_ID_C45			0x8000
#define MDIO_PHY_ID_PRTAD		0x03e0
#define MDIO_PHY_ID_DEVAD		0x001f
#define MDIO_PHY_ID_C45_MASK						\
	(MDIO_PHY_ID_C45 | MDIO_PHY_ID_PRTAD | MDIO_PHY_ID_DEVAD)

static inline int mdio_phy_id_is_c45(int phy_id)
{
	return (phy_id & MDIO_PHY_ID_C45) && !(phy_id & ~MDIO_PHY_ID_C45_MASK);
}

static inline __u16 mdio_phy_id_prtad(int phy_id)
{
	return (phy_id & MDIO_PHY_ID_PRTAD) >> 5;
}

static inline __u16 mdio_phy_id_devad(int phy_id)
{
	return phy_id & MDIO_PHY_ID_DEVAD;
}

#define MDIO_SUPPORTS_C22		1
#define MDIO_SUPPORTS_C45		2

/**
 * mdio_mii_ioctl - MII ioctl interface for MDIO (clause 22 or 45) PHYs
 * @mdio: MDIO interface
 * @mii_data: MII ioctl data structure
 * @cmd: MII ioctl command
 *
 * Returns 0 on success, negative on error.
 */
static inline int mdio_mii_ioctl(const struct mdio_if_info *mdio,
				 struct mii_ioctl_data *mii_data, int cmd)
{
	int prtad, devad;
	u16 addr = mii_data->reg_num;

	/* Validate/convert cmd to one of SIOC{G,S}MIIREG */
	switch (cmd) {
	case SIOCGMIIPHY:
		if (mdio->prtad == MDIO_PRTAD_NONE)
			return -EOPNOTSUPP;
		mii_data->phy_id = mdio->prtad;
		cmd = SIOCGMIIREG;
		break;
	case SIOCGMIIREG:
		break;
	case SIOCSMIIREG:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		break;
	default:
		return -EOPNOTSUPP;
	}

	/* Validate/convert phy_id */
	if ((mdio->mode_support & MDIO_SUPPORTS_C45) &&
	    mdio_phy_id_is_c45(mii_data->phy_id)) {
		prtad = mdio_phy_id_prtad(mii_data->phy_id);
		devad = mdio_phy_id_devad(mii_data->phy_id);
	} else if ((mdio->mode_support & MDIO_SUPPORTS_C22) &&
		   mii_data->phy_id < 0x20) {
		prtad = mii_data->phy_id;
		devad = MDIO_DEVAD_NONE;
		addr &= 0x1f;
	} else if ((mdio->mode_support & MDIO_EMULATE_C22) &&
		   mdio->prtad != MDIO_PRTAD_NONE &&
		   mii_data->phy_id == mdio->prtad) {
		/* Remap commonly-used MII registers. */
		prtad = mdio->prtad;
		switch (addr) {
		case MII_BMCR:
		case MII_BMSR:
		case MII_PHYSID1:
		case MII_PHYSID2:
			devad = __ffs(mdio->mmds);
			break;
		case MII_ADVERTISE:
		case MII_LPA:
			if (!(mdio->mmds & MDIO_DEVS_AN))
				return -EINVAL;
			devad = MDIO_MMD_AN;
			if (addr == MII_ADVERTISE)
				addr = MDIO_AN_ADVERTISE;
			else
				addr = MDIO_AN_LPA;
			break;
		default:
			return -EINVAL;
		}
	} else {
		return -EINVAL;
	}

	if (cmd == SIOCGMIIREG) {
		int rc = mdio->mdio_read(mdio->dev, prtad, devad, addr);
		if (rc < 0)
			return rc;
		mii_data->val_out = rc;
		return 0;
	} else {
		return mdio->mdio_write(mdio->dev, prtad, devad, addr,
					mii_data->val_in);
	}
}
#endif

#if (LINUX_VERSION_CODE < 0x02061D)
static inline ssize_t
pci_read_vpd(struct pci_dev *dev, loff_t pos, size_t count, u8 *buf)
{
	int i, vpd_cap;

	vpd_cap = pci_find_capability(dev, PCI_CAP_ID_VPD);
	if (!vpd_cap)
		return -ENODEV;

	for (i = 0; i < count; i += 4) {
		u32 tmp, j = 0;
		__le32 v;
		u16 tmp16;

		pci_write_config_word(dev, vpd_cap + PCI_VPD_ADDR, i);
		while (j++ < 100) {
			pci_read_config_word(dev, vpd_cap +
					     PCI_VPD_ADDR, &tmp16);
			if (tmp16 & 0x8000)
				break;
			msleep(1);
		}
		if (!(tmp16 & 0x8000))
			break;

		pci_read_config_dword(dev, vpd_cap + PCI_VPD_DATA, &tmp);
		v = cpu_to_le32(tmp);
		memcpy(&buf[i], &v, sizeof(v));
	}

	return i;
}
#endif

#ifndef ____ilog2_NaN
#define ____ilog2_NaN(x) (-1)
#endif

#ifndef ilog2
static inline
int bnx2x_ilog2(int x)
{
	int log = 0;
	while (x >>=1 )
		log++;
	return log;
}
#define ilog2(x)	bnx2x_ilog2(x)
#endif


#ifndef bool
#define bool int
#endif

#ifndef false
#define false 0
#define true 1
#endif

#ifndef DIV_ROUND_UP
#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))
#endif

#if (LINUX_VERSION_CODE < 0x02060c)
#define is_multicast_ether_addr(addr) (0x01 & (addr)[0])
#endif

#if (LINUX_VERSION_CODE < 0x02060e)
#define is_broadcast_ether_addr(addr)\
	(((addr)[0] & (addr)[1] & (addr)[2] & (addr)[3] \
	& (addr)[4] & (addr)[5]) == 0xff)
#endif

#if (LINUX_VERSION_CODE < 0x020622)
static inline void list_rotate_left(struct list_head *head)
{
	struct list_head *first;

	if (!list_empty(head)) {
		first = head->next;
		list_move_tail(first, head);
	}
}
#endif

#ifndef roundup
#define roundup(x, y) ((((x) + ((y) - 1)) / (y)) * (y))
#endif

#ifndef DEFINE_PCI_DEVICE_TABLE
#define DEFINE_PCI_DEVICE_TABLE(tbl) const struct pci_device_id bnx2x_pci_tbl[]
#endif

#if (LINUX_VERSION_CODE < 0x020606)
#undef netdev_printk
#undef netdev_err
#undef netdev_info
#endif

#ifndef netdev_printk

#if (LINUX_VERSION_CODE < 0x020615)
#define NET_PARENT_DEV(netdev)  ((netdev)->class_dev.dev)
#else
#define NET_PARENT_DEV(netdev)  ((netdev)->dev.parent)
#endif

#if !defined(__VMKLNX__) /* BNX2X_UPSTREAM */
#define netdev_printk(level, netdev, format, args...)		\
	dev_printk(level, NET_PARENT_DEV(netdev),	\
		   "%s: " format,				\
		   netdev_name(netdev), ##args)

#else
#define netdev_printk(level, netdev, format, args...)           \
	printk(level \
	       "%s %s: %s: " format,                            \
	       DRV_MODULE_NAME, pci_name(netdev->pdev),         \
	       netdev_name(netdev), ##args)
#endif


static inline const char *netdev_name(const struct net_device *dev)
{
	if (dev->reg_state != NETREG_REGISTERED)
		return "(unregistered net_device)";
	return dev->name;
}


#endif

#ifndef netdev_err
#define netdev_err(dev, format, args...)			\
	netdev_printk(KERN_ERR, dev, format, ##args)
#endif

#ifndef netdev_dbg
#define netdev_dbg(dev, format, args...)			\
	netdev_printk(KERN_DEBUG, dev, format, ##args)
#endif

#ifndef pr_cont
#define pr_cont(fmt, ...) \
	printk(KERN_CONT fmt, ##__VA_ARGS__)
#endif

#ifndef netdev_info
#define netdev_info(dev, format, args...)			\
	netdev_printk(KERN_INFO, dev, format, ##args)
#endif

#ifndef pr_fmt
#define pr_fmt(fmt) fmt
#endif

#ifndef pr_alert
#define pr_alert(fmt, ...) \
	printk(KERN_ALERT pr_fmt(fmt), ##__VA_ARGS__)
#endif

#ifndef pr_err
#define pr_err(fmt, ...) \
	printk(KERN_ERR pr_fmt(fmt), ##__VA_ARGS__)
#endif

#ifndef netdev_mc_count
#define netdev_mc_count(dev) ((dev)->mc_count)
#endif

#ifndef dev_alert
#define dev_alert(dev, format, arg...)		\
	dev_printk(KERN_ALERT , dev , format , ## arg)
#endif

#ifndef netdev_for_each_mc_addr
#define netdev_for_each_mc_addr(mclist, dev) \
	for (mclist = (dev)->mc_list; mclist; mclist = mclist->next)
#endif

#ifndef netdev_hw_addr_list_for_each
#define bnx2x_mc_addr(ha)      ((ha)->dmi_addr)
#else
#ifdef BNX2X_OLD_FCOE
#define bnx2x_mc_addr(ha)      ((ha)->dmi_addr)
#else
#define bnx2x_mc_addr(ha)      ((ha)->addr)
#endif
#endif

#ifndef PCI_SRIOV_NUM_BARS
#define PCI_SRIOV_NUM_BARS	6
#endif

#ifndef PCI_IOV_RESOURCES
#define PCI_IOV_RESOURCES	7
#endif

#ifndef PCI_EXT_CAP_ID_SRIOV
#define PCI_EXT_CAP_ID_SRIOV	16
#endif

#ifndef PCI_SRIOV_CAP
#define PCI_SRIOV_CAP		0x04
#endif

#ifndef PCI_SRIOV_CTRL
#define PCI_SRIOV_CTRL		0x08
#endif

#ifndef PCI_SRIOV_INITIAL_VF
#define PCI_SRIOV_INITIAL_VF	0x0c
#endif

#ifndef PCI_SRIOV_TOTAL_VF
#define PCI_SRIOV_TOTAL_VF	0x0e
#endif

#ifndef PCI_SRIOV_FUNC_LINK
#define PCI_SRIOV_FUNC_LINK	0x12
#endif

#ifndef PCI_SRIOV_VF_OFFSET
#define PCI_SRIOV_VF_OFFSET	0x14
#endif

#ifndef PCI_SRIOV_VF_STRIDE
#define PCI_SRIOV_VF_STRIDE	0x16
#endif

#ifndef PCI_SRIOV_SUP_PGSIZE
#define PCI_SRIOV_SUP_PGSIZE	0x1c
#endif

#ifndef	PCI_EXP_DEVCTL2
#define PCI_EXP_DEVCTL2		40
#endif

#if (LINUX_VERSION_CODE < 0x02061b)
#define netif_addr_lock_bh(dev) netif_tx_lock_bh(dev)
#define netif_addr_unlock_bh(dev) netif_tx_unlock_bh(dev)
#endif

#if (LINUX_VERSION_CODE < 0x020623)
#define ETH_FLAG_RXHASH	(0x1 << 28)
#endif

#if (LINUX_VERSION_CODE < 0x020613)

/* The below code is similar to what is done random32() in
 * 2.6.19 but much simpler. ;)
 */

#define TAUSWORTHE(s,a,b,c,d) ((((s)&(c))<<(d)) ^ ((((s) <<(a)) ^ (s))>>(b)))
static inline u32 random32(void) {
	static u32 s1 = 4294967294UL;
	static u32 s2 = 4294967288UL;
	static u32 s3 = 4294967280UL;
	u32 cycles;

	/* This would be our seed for this step */
	cycles = get_cycles();

	s1 = TAUSWORTHE(s1 + cycles, 13, 19, 4294967294UL, 12);
	s2 = TAUSWORTHE(s2 + cycles, 2, 25, 4294967288UL, 4);
	s3 = TAUSWORTHE(s3 + cycles, 3, 11, 4294967280UL, 17);

	return (s1 ^ s2 ^ s3);
}
#endif


#ifndef PCI_MSIX_FLAGS_QSIZE
#define PCI_MSIX_FLAGS_QSIZE	0x7FF
#endif

#if (LINUX_VERSION_CODE < 0x02060f)
#define atomic_cmpxchg(p, old, new) cmpxchg((volatile int *)(p), old, new)
#endif


#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr)	(sizeof(arr) / sizeof((arr)[0]))
#endif

#endif /* __BNX2X_COMPAT_H__ */
