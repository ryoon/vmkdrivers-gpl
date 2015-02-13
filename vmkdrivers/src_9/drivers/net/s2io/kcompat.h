/*
 * Portions Copyright 2008 VMware, Inc.
 */
#ifndef _KCOMPAT_H_
#define _KCOMPAT_H_

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,16))
#include <linux/config.h>
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#include<linux/tqueue.h>
#else
#include <linux/moduleparam.h>
#include<linux/workqueue.h>
#endif

/* VENDOR and DEVICE ID of XENA. */
#ifndef PCI_VENDOR_ID_S2IO
#define PCI_VENDOR_ID_S2IO      0x17D5
#define PCI_DEVICE_ID_S2IO_WIN  0x5731
#define PCI_DEVICE_ID_S2IO_UNI  0x5831
#endif

#ifndef PCI_DEVICE_ID_HERC_WIN
#define PCI_DEVICE_ID_HERC_WIN  0x5732
#define PCI_DEVICE_ID_HERC_UNI  0x5832
#endif

#ifndef PCI_DEVICE_ID_TITAN
#define PCI_DEVICE_ID_TITAN_WIN  0x5733
#define PCI_DEVICE_ID_TITAN_UNI  0x5833
#endif

#ifndef PCI_DEVICE_ID_AMD_8132_BRIDGE
#define PCI_DEVICE_ID_AMD_8132_BRIDGE	0x7458
#endif

/* Ethtool related variables and Macros. */
#ifndef SET_ETHTOOL_OPS
static int s2io_ethtool(struct net_device *dev, struct ifreq *rq);
#define SPEED_10000 10000
#endif

#ifndef DMA_ERROR_CODE
#define DMA_ERROR_CODE          (~(dma_addr_t)0x0)
#endif

/*
 * Macros for msleep and msleep_interruptible for kernel versions in which
 * they are not defined.
 */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,8) )
#define msleep(x)       do { set_current_state(TASK_UNINTERRUPTIBLE); \
                                schedule_timeout((x * HZ)/1000 + 2); \
                        } while(0)
#endif

/* #define proc_net	init_net.proc_net */

#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,9) )
#define msleep_interruptible(x) do {set_current_state(TASK_INTERRUPTIBLE); \
                                        schedule_timeout((x * HZ)/1000); \
                                } while(0)
#endif

#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,5) )
#define pci_dma_sync_single_for_cpu(pdev, dma_addr, len, dir) \
	pci_dma_sync_single((pdev), (dma_addr), (len), (dir))

#define pci_dma_sync_single_for_device(pdev, dma_addr, len, dir) \
	pci_dma_sync_single((pdev), (dma_addr), (len), (dir))
#endif


/* pci_save_state */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,10))
#define pci_save_state(x,y)	pci_save_state(x)
#endif

/* pci_restore_state */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,10))
#define pci_restore_state(x,y)	pci_restore_state(x);
#endif

/* synchronize_irq */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,28))
#define s2io_synchronize_irq(x)		synchronize_irq()
#else
#define s2io_synchronize_irq(x)		synchronize_irq(x)
#endif

/* pci_dev_put */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
#define pci_dev_put(x)
#endif

/* Macros to ensure the code is backward compatible with 2.4.x kernels. */
#ifndef NET_IP_ALIGN
#define NET_IP_ALIGN 2
#endif

#ifndef SET_NETDEV_DEV
#define SET_NETDEV_DEV(a, b)    do {} while(0)
#endif

#ifndef HAVE_FREE_NETDEV
#define free_netdev(x) kfree(x)
#endif

#ifndef IRQ_NONE
typedef void irqreturn_t;
#define IRQ_NONE
#define IRQ_HANDLED
#define IRQ_RETVAL(x)
#endif

#ifdef INIT_TQUEUE
#define schedule_work schedule_task
#define flush_scheduled_work flush_scheduled_tasks
#define INIT_WORK INIT_TQUEUE
#endif

#ifndef __iomem
#define __iomem
#endif

#ifndef DMA_64BIT_MASK
#define DMA_64BIT_MASK  0xffffffffffffffffULL
#define DMA_32BIT_MASK  0x00000000ffffffffULL
#endif

#ifndef S2IO_ALIGN
#define S2IO_ALIGN(x,a) (((x)+(a)-1)&~((a)-1))
#endif

#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0) )
#define GET_SOCK_SLEEP_CALLBACK(sk)     sk->sleep
#define GET_SOCK_SOCKET_DESC(sk)        sk->socket
#else
#define GET_SOCK_SLEEP_CALLBACK(sk)     sk->sk_sleep
#define GET_SOCK_SOCKET_DESC(sk)        sk->sk_socket
#endif

#ifndef module_param
#define S2IO_PARM_INT(X, def_val) \
        static unsigned int X = def_val;\
	        MODULE_PARM(X, "i");
#endif

#ifndef SKB_GSO_TCPV4
#define SKB_GSO_TCPV4 	0x1
#endif

#ifndef SKB_GSO_UDP
#define SKB_GSO_UDP		0x2
#endif

#ifndef SKB_GSO_UDPV4
#define SKB_GSO_UDPV4	0x2
#endif

#ifndef SKB_GSO_TCPV6
#define SKB_GSO_TCPV6	0x10
#endif

#ifndef NETIF_F_GSO

#ifdef NETIF_F_TSO
#define s2io_tcp_mss(skb) skb_shinfo(skb)->tso_size
#else
#define s2io_tcp_mss(skb) 0
#endif

#ifdef  NETIF_F_UFO
#define s2io_udp_mss(skb) skb_shinfo(skb)->ufo_size
#else
#define s2io_udp_mss(skb) 0
#endif

inline int s2io_offload_type(struct sk_buff *skb)
{
#ifdef NETIF_F_TSO
	if (skb_shinfo(skb)->tso_size)
		return SKB_GSO_TCPV4;
#endif
#ifdef  NETIF_F_UFO
	else if(skb_shinfo(skb)->ufo_size)
		return SKB_GSO_UDP;
#endif
	return 0;
}
#endif
#ifndef NETIF_F_TSO6
#define s2io_ethtool_op_get_tso ethtool_op_get_tso
#define s2io_ethtool_op_set_tso ethtool_op_set_tso
#endif
#ifndef IRQF_SHARED
#define IRQF_SHARED SA_SHIRQ
#endif

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,15))
#undef CONFIG_PM
#endif

#ifndef CHECKSUM_PARTIAL
#define CHECKSUM_PARTIAL CHECKSUM_HW
#endif

/* Macros to ensure the code is backward compatible with 2.6.5 kernels. */
#ifndef  spin_trylock_irqsave
#define spin_trylock_irqsave(lock, flags) \
({ \
	local_irq_save(flags); \
	spin_trylock(lock) ? \
	1 : ({ local_irq_restore(flags); 0; }); \
})

#ifndef NETDEV_TX_LOCKED
#define NETDEV_TX_LOCKED -1	/* driver tx lock was already taken */
#endif
#endif

#ifndef NETDEV_TX_BUSY
#define NETDEV_TX_BUSY 1
#endif

#define S2IO_SKB_INIT_TAIL(frag_list, val) \
	frag_list->tail = (void *) (unsigned long)val
	/*skb_reset_tail_pointer(frag_list)*/

#define S2IO_PCI_FIND_DEVICE(_a, _b, _c) \
        pci_get_device(_a, _b, _c)

#define S2IO_PCI_PUT_DEVICE(_a) \
        pci_dev_put(_a)

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,9))
#ifndef __be32
#define __be32 u32
#endif
#endif

inline u16 s2io_get_tx_priority(struct sk_buff *skb, u16 vlan_tag)
{
	u16 ret = 0;
	ret = (skb->priority & 0x7);
	return ret;
}

#if defined(__VMKLNX__)
#define pci_channel_offline(a) 0
#endif // defined(__VMKLNX__)

#endif /* _KCOMPAT_H_ */
