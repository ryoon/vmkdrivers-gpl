/*
 * Copyright (C) 2003 - 2009 NetXen, Inc.
 * All rights reserved.
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA.
 * 
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.
 * 
 * Contact Information:
 * licensing@netxen.com
 * NetXen, Inc.
 * 18922 Forge Drive
 * Cupertino, CA 95014
 */
#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/types.h>
//#include <linux/kgdb-defs.h>
#include <linux/compiler.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/inetdevice.h>

#if defined(ESX_3X_COS)
#include <net/ip.h>
#include <net/tcp.h>
#else
#include <linux/ip.h>
#include <linux/tcp.h>
#endif

#if (!defined(ESX_3X) && !defined(ESX_3X_COS))
#include <linux/ipv6.h>
#endif

#include <linux/in.h>
#include <linux/skbuff.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/highmem.h>

#include "nx_errorcode.h"
#include "nxplat.h"
#include "nxhal_nic_interface.h"
#include "nxhal_nic_api.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,21)
#include <linux/ethtool.h>
#endif
#include <linux/mii.h>
#include <linux/interrupt.h>
#include <linux/timer.h>

#include <linux/mm.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 0)
#include <linux/wrapper.h>
#endif
#ifndef _LINUX_MODULE_PARAMS_H
#include <linux/moduleparam.h>
#endif
#include <asm/system.h>
#include <asm/io.h>
#include <asm/byteorder.h>
#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/checksum.h>
#include "kernel_compatibility.h"
#include "unm_nic_hw.h"
#include "unm_nic_config.h"
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
#include <net/net_namespace.h>
#endif



#undef SINGLE_DMA_BUF
#define UNM_NIC_HW_CSUM
// #define UNM_SKB_TASKLET

#define PCI_CAP_ID_GEN  0x10

#define UNM_RCV_PEG_DB_ID  2

#ifndef DMA_64BIT_MASK
#define DMA_64BIT_MASK			 0xffffffffffffffffULL
#endif

#ifndef DMA_39BIT_MASK
#define DMA_39BIT_MASK			 0x0000007fffffffffULL
#endif

#ifndef DMA_35BIT_MASK
#define DMA_35BIT_MASK                   0x00000007ffffffffULL
#endif

#ifndef DMA_32BIT_MASK
#define DMA_32BIT_MASK                   0x00000000ffffffffULL
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19)
#define	CHECKSUM_HW	CHECKSUM_PARTIAL
#endif /* KERNEL_VERSION(2,4,19) */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)
#define PCI_EXP_LNKSTA 18
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)
#define	PCI_D0		0
#define	PCI_D1		1
#define	PCI_D2		2
#define	PCI_D3hot	3
#define	PCI_D3cold	4
#endif

#if defined(__VMKLNX__) || LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
#define	TASK_PARAM		struct work_struct *
#define	NX_INIT_WORK(a, b, c)	INIT_WORK(a, b)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#define	TASK_PARAM		unsigned long
#define	NX_INIT_WORK(a, b, c)	INIT_WORK(a, (void (*)(void *))b, c)
#else
#define	TASK_PARAM		unsigned long
#define	NX_INIT_WORK(a, b, c)	INIT_TQUEUE(a, (void (*)(void *))b, c)
#endif

#include "unm_nic.h"

#ifdef NX_FCOE_SUPPORT1
#include "nx_fcoe.h"
#endif

#define DEFINE_GLOBAL_RECV_CRB
#include "nic_phan_reg.h"

#include "nic_cmn.h"
#include "nx_license.h"
#include "nxhal.h"
#include "nxhal_v34.h"
#include "unm_nic_config.h"
#include "unm_nic_lro.h"

#include "unm_nic_hw.h"
#include "unm_version.h"
#include "unm_brdcfg.h"
#include "nx_nic_linux_tnic_api.h"

#if defined (__VMKLNX__)
#define VC_1ST_BUFFER_18_BYTE_WORKAROUND 1
#define VC_1ST_BUFFER_REQUIRED_LENGTH   18
#endif /* __VMKLNX__ */

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESCRIPTION);
MODULE_LICENSE("GPL");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
MODULE_VERSION(UNM_NIC_LINUX_VERSIONID);
#endif

#ifndef ESX
static int nx_nic_inetaddr_event(struct notifier_block *this, unsigned long event,
			     void *ptr);
static int nx_nic_netdev_event(struct notifier_block *this, unsigned long event,
			   void *ptr);

struct notifier_block	nx_nic_inetaddr_cb = {
	notifier_call:	nx_nic_inetaddr_event,
};

struct notifier_block	nx_nic_netdev_cb = {
	notifier_call:	nx_nic_netdev_event,
};
#endif

static int use_msi = 1;
#if defined(ESX)
static int rss_enable = 0;
static int use_msi_x = 1;
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,8))
static int rss_enable = 1;
static int use_msi_x = 1;
#else
static int rss_enable = 0;
static int use_msi_x = 0;
#endif
static int tx_desc		= NX_MAX_CMD_DESCRIPTORS;
static int lro_desc		= MAX_LRO_RCV_DESCRIPTORS;
static int rdesc_1g		= NX_DEFAULT_RDS_SIZE_1G;
static int rdesc_10g		= NX_DEFAULT_RDS_SIZE;
static int jumbo_desc_1g	= NX_DEFAULT_JUMBO_RDS_SIZE_1G;
static int jumbo_desc		= NX_DEFAULT_JUMBO_RDS_SIZE;
static int rx_chained		= 0;

#if (defined(ESX)) 
#if (!defined(ESX_3X_COS))
static int multi_ctx		= 1;
#else
static int multi_ctx            = 0;
#endif
/*Initialize #of netqs with invalid value to know if user has configured or not */
static int num_rx_queues = INVALID_INIT_NETQ_RX_CONTEXTS;
#endif //#if (defined(ESX)

#if (defined(ESX) || defined(ESX_3X_COS))
static int lro                  = 0;
#else
static int lro                  = 1;
#endif

static int hw_vlan                     = 0;
static int tso                         = 1;

static int fw_load = LOAD_FW_WITH_COMPARISON;
module_param(fw_load, int, S_IRUGO);
MODULE_PARM_DESC(fw_load, "Load firmware from file system or flash");

static int port_mode = UNM_PORT_MODE_AUTO_NEG;	// Default to auto-neg. mode
module_param(port_mode, int, S_IRUGO);
MODULE_PARM_DESC(port_mode, "Ports operate in XG, 1G or Auto-Neg mode");

static int wol_port_mode         = 5; // Default to restricted 1G auto-neg. mode
module_param(wol_port_mode, int, S_IRUGO);
MODULE_PARM_DESC(wol_port_mode, "In wol mode, ports operate in XG, 1G or Auto-Neg");

module_param(use_msi, bool, S_IRUGO);
MODULE_PARM_DESC(use_msi, "Enable or Disable MSI");

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,8)
module_param(use_msi_x, bool, S_IRUGO);
MODULE_PARM_DESC(use_msi_x, "Enable or Disable MSI-X");

module_param(rss_enable, bool, S_IRUGO);
MODULE_PARM_DESC(rss_enable, "Enable or Disable RSS");
#endif

module_param(tx_desc, int, S_IRUGO);
MODULE_PARM_DESC(tx_desc, "Maximum Transmit Descriptors in Host");

module_param(jumbo_desc, int, S_IRUGO);
MODULE_PARM_DESC(jumbo_desc, "Maximum Jumbo Receive Descriptors");

module_param(rdesc_1g, int, S_IRUGO);
MODULE_PARM_DESC(rdesc_1g, "Maximum Receive Descriptors for 1G");

module_param(rdesc_10g, int, S_IRUGO);
MODULE_PARM_DESC(rdesc_10g, "Maximum Receive Descriptors for 10G");

module_param(jumbo_desc_1g, int, S_IRUGO);
MODULE_PARM_DESC(jumbo_desc_1g, "Maximum Jumbo Receive Descriptors for 1G");

#ifdef	ESX
module_param(multi_ctx, int, S_IRUGO);
MODULE_PARM_DESC(multi_ctx, "Enable/disable mutlple context support Default:1, Enable=1 ,Disable=0");
module_param(num_rx_queues, int, S_IRUGO);
MODULE_PARM_DESC(num_rx_queues, "Number of RX queues per PCI FN Default:15, Min:0, Max:15 ");
#endif

#if (defined(ESX_3X) || defined(ESX_3X_COS))
char unm_nic_driver_name[] = "unm_nic";
#else
char unm_nic_driver_name[] = DRIVER_NAME;
#endif

#if (defined (ESX) || defined(ESX_3X_COS))
module_param(lro, bool, S_IRUGO);
MODULE_PARM_DESC(lro, "Enable/disable LRO");
#endif

module_param(hw_vlan, bool, S_IRUGO);
MODULE_PARM_DESC(hw_vlan, "Enable/disable VLAN OFFLOAD TO HW. Default:0, Enable:1, Disable:0");

module_param(tso, bool, S_IRUGO);
MODULE_PARM_DESC(tso, "Enable/disable TSO. Default:1, Enable:1, Disable:0");

char unm_nic_driver_string[] = DRIVER_VERSION_STRING
    UNM_NIC_LINUX_VERSIONID
    "-" UNM_NIC_BUILD_NO " generated " UNM_NIC_TIMESTAMP;
uint8_t nx_nic_msglvl = NX_NIC_NOTICE;
char *nx_nic_kern_msgs[] = {
	KERN_EMERG,
	KERN_ALERT,
	KERN_CRIT,
	KERN_ERR,
	KERN_WARNING,
	KERN_NOTICE,
	KERN_INFO,
	KERN_DEBUG
};


struct loadregs {
    uint32_t function;
    uint32_t offset;
    uint32_t andmask;
    uint32_t ormask;
};

#define LOADREGCOUNT   8
struct loadregs driverloadregs_gen1[LOADREGCOUNT] = {
    { 0, 0xD8, 0x00000000, 0x000F1000  },
    { 1, 0xD8, 0x00000000, 0x000F1000  },
    { 2, 0xD8, 0x00000000, 0x000F1000  },
    { 3, 0xD8, 0x00000000, 0x000F1000  },
    { 4, 0xD8, 0x00000000, 0x000F1000  },
    { 5, 0xD8, 0x00000000, 0x000F1000  },
    { 6, 0xD8, 0x00000000, 0x000F1000  },
    { 7, 0xD8, 0x00000000, 0x000F1000  }
};

struct loadregs driverloadregs_gen2[LOADREGCOUNT] = {
    { 0, 0xC8, 0x00000000, 0x000F1000  },
    { 1, 0xC8, 0x00000000, 0x000F1000  },
    { 2, 0xC8, 0x00000000, 0x000F1000  },
    { 3, 0xC8, 0x00000000, 0x000F1000  },
    { 4, 0xC8, 0x00000000, 0x000F1000  },
    { 5, 0xC8, 0x00000000, 0x000F1000  },
    { 6, 0xC8, 0x00000000, 0x000F1000  },
    { 7, 0xC8, 0x00000000, 0x000F1000  }
};




static int adapter_count = 0;

static uint32_t msi_tgt_status[8] = {
	ISR_INT_TARGET_STATUS, ISR_INT_TARGET_STATUS_F1,
	ISR_INT_TARGET_STATUS_F2, ISR_INT_TARGET_STATUS_F3,
	ISR_INT_TARGET_STATUS_F4, ISR_INT_TARGET_STATUS_F5,
	ISR_INT_TARGET_STATUS_F6, ISR_INT_TARGET_STATUS_F7
};


static struct nx_legacy_intr_set legacy_intr[] = NX_LEGACY_INTR_CONFIG;

static uint32_t crb_cmd_producer[4] = {
	CRB_CMD_PRODUCER_OFFSET, CRB_CMD_PRODUCER_OFFSET_1,
	CRB_CMD_PRODUCER_OFFSET_2, CRB_CMD_PRODUCER_OFFSET_3
};

#ifndef ARCH_KMALLOC_MINALIGN
#define ARCH_KMALLOC_MINALIGN 0
#endif
#ifndef ARCH_KMALLOC_FLAGS
#define ARCH_KMALLOC_FLAGS SLAB_HWCACHE_ALIGN
#endif

#define UNM_NETDEV_WEIGHT	64

#define RCV_DESC_RINGSIZE(COUNT)    (sizeof(rcvDesc_t) * (COUNT))
#define STATUS_DESC_RINGSIZE(COUNT) (sizeof(statusDesc_t)* (COUNT))
#define TX_RINGSIZE(COUNT)          (sizeof(struct unm_cmd_buffer) * (COUNT))
#define RCV_BUFFSIZE(COUNT)         (sizeof(struct unm_rx_buffer) * (COUNT))

#define UNM_DB_MAPSIZE_BYTES    0x1000
#define UNM_CMD_PRODUCER_OFFSET                 0
#define UNM_RCV_STATUS_CONSUMER_OFFSET          0
#define UNM_RCV_PRODUCER_OFFSET                 0

#define MAX_RX_CTX 		1
#define MAX_TX_CTX 		1

#define NX_IS_LSA_REGISTERED(ADAPTER)				\
	((ADAPTER)->nx_sds_cb_tbl[NX_NIC_CB_LSA].registered)

/* Extern definition required for vmkernel module */

#ifdef ESX
extern int nx_handle_large_addr(struct unm_adapter_s *adapter,
		                struct unm_skb_frag *frag, dma_addr_t *phys,
				void *virt[], int len[], int tot_len);
extern  struct nx_cmd_struct *
nx_find_suitable_bounce_buf(struct vmk_bounce *bounce, int req_bufs);

extern int nx_setup_vlan_buffers(struct unm_adapter_s * adapter);

extern int nx_setup_tx_vmkbounce_buffers(struct unm_adapter_s * adapter);

extern void nx_free_vlan_buffers(struct unm_adapter_s *adapter);

extern void nx_free_tx_vmkbounce_buffers(struct unm_adapter_s *adapter);

extern void nx_free_frag_bounce_buf(struct unm_adapter_s *adapter,
		                struct unm_skb_frag *frag);
#endif

static inline int is_packet_tagged(struct sk_buff *skb);

/* Local functions to UNM NIC driver */
static int __devinit unm_nic_probe(struct pci_dev *pdev,
				   const struct pci_device_id *ent);
static void __devexit unm_nic_remove(struct pci_dev *pdev);
void *nx_alloc(struct unm_adapter_s *adapter, size_t sz,
	       dma_addr_t * ptr, struct pci_dev **used_dev);
static int unm_nic_open(struct net_device *netdev);
static int unm_nic_close(struct net_device *netdev);
static int unm_nic_set_mac(struct net_device *netdev, void *p);
static int nx_nic_fw34_change_mtu(struct net_device *netdev, int new_mtu);
static int nx_nic_fw40_change_mtu(struct net_device *netdev, int new_mtu);
static int receive_peg_ready(struct unm_adapter_s *adapter);
static int unm_nic_hw_resources(struct unm_adapter_s *adapter);
static void nx_nic_p2_set_multi(struct net_device *netdev);
static void nx_nic_p3_set_multi(struct net_device *netdev);
static void initialize_adapter_sw(struct unm_adapter_s *adapter,
				  nx_host_rx_ctx_t *nxhal_host_rx_ctx);
static int init_firmware(struct unm_adapter_s *adapter);
int unm_post_rx_buffers(struct unm_adapter_s *adapter,
			nx_host_rx_ctx_t *nxhal_host_rx_ctx, uint32_t type);
static int nx_alloc_rx_skb(struct unm_adapter_s *adapter,
			   nx_host_rds_ring_t * rcv_desc,
			   struct unm_rx_buffer *buffer);
static inline void unm_process_lro(struct unm_adapter_s *adapter,
				   nx_host_sds_ring_t *nxhal_sds_ring,
				   statusDesc_t *desc,
				   statusDesc_t *desc_list,
				   int num_desc);
static inline void unm_process_rcv(struct unm_adapter_s *adapter,
				   nx_host_sds_ring_t *nxhal_sds_ring,
				   statusDesc_t * desc);

static void nx_write_rcv_desc_db(struct unm_adapter_s *adapter,
				 nx_host_rx_ctx_t *nxhal_host_rx_ctx,
				 int ring);
static void unm_nic_v34_context_prepare(struct unm_adapter_s *adapter);

static int unm_nic_new_rx_context_prepare(struct unm_adapter_s *adapter, nx_host_rx_ctx_t *nxhal_rx_ctx);
static int unm_nic_new_tx_context_prepare(struct unm_adapter_s *adapter);
static int unm_nic_new_tx_context_destroy(struct unm_adapter_s *adapter, int ctx_destroy);
static int unm_nic_new_rx_context_destroy(struct unm_adapter_s *adapter, int ctx_destroy);
static void nx_nic_free_hw_rx_resources(struct unm_adapter_s *adapter, nx_host_rx_ctx_t *nxhal_rx_ctx);
static void nx_nic_free_host_rx_resources(struct unm_adapter_s *adapter, nx_host_rx_ctx_t *nxhal_rx_ctx);
static void nx_nic_free_host_sds_resources(struct unm_adapter_s *adapter,
                nx_host_rx_ctx_t *nxhal_host_rx_ctx);
int nx_nic_create_rx_ctx(struct net_device *netdev);

static void unm_tx_timeout(struct net_device *netdev);
static void unm_tx_timeout_task(TASK_PARAM adapid);
static int unm_process_cmd_ring(unsigned long data);
static void unm_nic_down(struct net_device *netdev);
static int initialize_adapter_hw(struct unm_adapter_s *adapter);
static void unm_watchdog_task(TASK_PARAM adapid);
static void unm_watchdog_task_fw_reset(TASK_PARAM adapid);
void unm_nic_enable_all_int(unm_adapter * adapter, nx_host_rx_ctx_t *nxhal_rx_ctx);
void unm_nic_enable_int(unm_adapter * adapter, nx_host_sds_ring_t *nxhal_sds_ring);
void unm_nic_disable_all_int(unm_adapter *adapter,
			     nx_host_rx_ctx_t *nxhal_rx_ctx);
static void unm_nic_disable_int(unm_adapter *adapter,
				nx_host_sds_ring_t *nxhal_sds_ring);
static int nx_status_msg_nic_response_handler(struct net_device *netdev, void *data,
					      unm_msg_t *msg, struct sk_buff *skb);
static int nx_status_msg_default_handler(struct net_device *netdev, void *data,
					 unm_msg_t *msg, struct sk_buff *skb);
static uint32_t unm_process_rcv_ring(struct unm_adapter_s *,
				     nx_host_sds_ring_t *nxhal_sds_ring, int);
static struct net_device_stats *unm_nic_get_stats(struct net_device *netdev);


static void unm_pci_release_regions(struct pci_dev *pdev);
#ifdef UNM_NIC_NAPI
#ifdef NEW_NAPI
static int nx_nic_poll_sts(struct napi_struct *napi, int work_to_do);
#else
static int nx_nic_poll_sts(struct net_device *netdev, int *budget);
static int unm_nic_poll(struct net_device *dev, int *budget);
#endif
#endif

#if !defined(__VMKLNX__)
#ifdef CONFIG_NET_POLL_CONTROLLER
static void unm_nic_poll_controller(struct net_device *netdev);
#endif
#endif /* !defined (__VMKLNX__) */

static inline void nx_nic_check_tso(struct unm_adapter_s *adapter, 
		cmdDescType0_t * desc, uint32_t tagged, struct sk_buff *skb);
static inline void nx_nic_tso_copy_headers(struct unm_adapter_s *adapter,
		struct sk_buff *skb, uint32_t *prod, uint32_t saved);

#ifdef OLD_KERNEL
static void nx_nic_legacy_intr(int irq, void *data, struct pt_regs *regs);
static void nx_nic_msi_intr(int irq, void *data, struct pt_regs *regs);
static void nx_nic_msix_intr(int irq, void *data, struct pt_regs *regs);
#elif !defined(__VMKLNX__) && LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
static irqreturn_t nx_nic_legacy_intr(int irq, void *data, struct pt_regs *regs);
static irqreturn_t nx_nic_msi_intr(int irq, void *data, struct pt_regs *regs);
static irqreturn_t nx_nic_msix_intr(int irq, void *data, struct pt_regs *regs);
#else
static irqreturn_t nx_nic_legacy_intr(int irq, void *data);
static irqreturn_t nx_nic_msi_intr(int irq, void *data);
static irqreturn_t nx_nic_msix_intr(int irq, void *data);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,21)
extern void set_ethtool_ops(struct net_device *netdev);
#endif

extern int unm_init_proc_drv_dir(void);
extern void unm_cleanup_proc_drv_entries(void);
extern void unm_init_proc_entries(struct unm_adapter_s *adapter);
extern void unm_cleanup_proc_entries(struct unm_adapter_s *adapter);

static void unm_init_pending_cmd_desc(unm_pending_cmd_descs_t * pending_cmds);
static void unm_destroy_pending_cmd_desc(unm_pending_cmd_descs_t *
					 pending_cmds);
static void unm_proc_pending_cmd_desc(unm_adapter * adapter);
static inline int unm_get_pending_cmd_desc_cnt(unm_pending_cmd_descs_t *
					       pending_cmds);

#if defined(XGB_DEBUG)
static void dump_skb(struct sk_buff *skb);
static int skb_is_sane(struct sk_buff *skb);
#endif

static int nx_config_rss(struct unm_adapter_s *adapter, int enable);

int nx_p3_set_vport_miss_mode(struct unm_adapter_s *adapter, int mode);

int unm_loopback_test(struct net_device *netdev, int fint, void *ptr,
		      unm_test_ctr_t * testCtx);
static void nx_free_sts_rings(struct unm_adapter_s *adapter, nx_host_rx_ctx_t *nxhal_rx_ctx);
static int nx_alloc_sts_rings(struct unm_adapter_s *adapter, nx_host_rx_ctx_t *nxhal_rx_ctx ,int cnt);
static int nx_register_irq(struct unm_adapter_s *adapter,nx_host_rx_ctx_t *nxhal_host_rx_ctx);
static int nx_unregister_irq(struct unm_adapter_s *adapter, nx_host_rx_ctx_t *nxhal_host_rx_ctx);

static int nx_init_status_msg_handler(struct unm_adapter_s *adapter);
static void nx_init_status_handler(struct unm_adapter_s *adapter);

int nx_nic_is_netxen_device(struct net_device *netdev);
int nx_nic_rx_register_msg_handler(struct net_device *netdev, uint8_t msgtype, void *data,
				   int (*nx_msg_handler) (struct net_device *netdev, void *data,
							  unm_msg_t *msg, struct sk_buff *skb));
void nx_nic_rx_unregister_msg_handler(struct net_device *netdev, uint8_t msgtype);
int nx_nic_rx_register_callback_handler(struct net_device *netdev, uint8_t interface_type,
					void *data);
void nx_nic_register_lsa_with_fw(struct net_device *netdev);
void nx_nic_unregister_lsa_from_fw(struct net_device *netdev);
void nx_nic_rx_unregister_callback_handler(struct net_device *netdev, uint8_t interface_type);
int nx_nic_get_adapter_revision_id(struct net_device *dev);
nx_tnic_adapter_t *nx_nic_get_lsa_adapter(struct net_device *netdev);
int nx_nic_get_device_port(struct net_device *netdev);
int nx_nic_get_device_ring_ctx(struct net_device *netdev);
struct pci_dev *nx_nic_get_pcidev(struct net_device *dev);
void nx_nic_get_lsa_version_number(struct net_device *netdev,
				    nic_version_t * version);
static void nx_nic_get_nic_version_number(struct net_device *netdev,
					  nic_version_t *version);
int nx_nic_send_msg_to_fw(struct net_device *dev,
				 pegnet_cmd_desc_t *cmd_desc_arr,
				 int nr_elements);
int nx_nic_send_msg_to_fw_pexq(struct net_device *dev,
				 pegnet_cmd_desc_t *cmd_desc_arr,
				 int nr_elements);
int nx_nic_cmp_adapter_id(struct net_device *dev1, struct net_device *dev2);
struct proc_dir_entry *nx_nic_get_base_procfs_dir(void);
nx_nic_api_t *nx_nic_get_api(void);
static uint64_t nx_nic_get_fw_capabilities(struct net_device *netdev);
static int nic_linkevent_request(struct unm_adapter_s *adapter,
			int async_enable, int linkevent_request);
static void nic_handle_linkevent_response(nx_dev_handle_t drv_handle,
				nic_response_t *resp);
int nx_nic_get_linkevent_cap(struct unm_adapter_s *adapter);
int nx_nic_get_pexq_cap(struct net_device *netdev);

nx_nic_api_t nx_nic_api_struct = {
        .api_ver                        = NX_NIC_API_VER,
        .is_netxen_device               = nx_nic_is_netxen_device,
        .register_msg_handler           = nx_nic_rx_register_msg_handler,
        .unregister_msg_handler         = nx_nic_rx_unregister_msg_handler,
        .register_callback_handler      = nx_nic_rx_register_callback_handler,
        .unregister_callback_handler    = nx_nic_rx_unregister_callback_handler,
        .get_adapter_rev_id             = nx_nic_get_adapter_revision_id,
        .get_lsa_adapter                = nx_nic_get_lsa_adapter,
        .get_device_port                = nx_nic_get_device_port,
        .get_device_ring_ctx            = nx_nic_get_device_ring_ctx,
        .get_pcidev                     = nx_nic_get_pcidev,
        .get_lsa_ver_num                = nx_nic_get_lsa_version_number,
        .get_nic_ver_num                = nx_nic_get_nic_version_number,
        .get_fw_capabilities		= nx_nic_get_fw_capabilities,
        .send_msg_to_fw                 = nx_nic_send_msg_to_fw,
        .send_msg_to_fw_pexq            = nx_nic_send_msg_to_fw_pexq,
        .cmp_adapter_id                 = nx_nic_cmp_adapter_id,
	.get_base_procfs_dir		= nx_nic_get_base_procfs_dir,
	.register_lsa_with_fw		= nx_nic_register_lsa_with_fw,
	.unregister_lsa_from_fw		= nx_nic_unregister_lsa_from_fw,
        .get_pexq_cap                   = nx_nic_get_pexq_cap,
};

static int nx_init_status_msg_handler(struct unm_adapter_s *adapter)
{
	int i = 0;

	for (i = 0; i < NX_MAX_SDS_OPCODE; i++) {
		adapter->nx_sds_msg_cb_tbl[i].msg_type = i;
		adapter->nx_sds_msg_cb_tbl[i].data = NULL;
		adapter->nx_sds_msg_cb_tbl[i].handler =
					nx_status_msg_default_handler;
		adapter->nx_sds_msg_cb_tbl[i].registered = 0;

		if (i == UNM_MSGTYPE_NIC_RESPONSE) {
			adapter->nx_sds_msg_cb_tbl[i].handler =
					nx_status_msg_nic_response_handler; 
			adapter->nx_sds_msg_cb_tbl[i].registered = 1;
		}
	}

	return 0;
}

static int nx_init_status_callback_handler(struct unm_adapter_s *adapter)
{
	int i = 0;

	for (i = 0; i < NX_NIC_CB_MAX; i++) {
		adapter->nx_sds_cb_tbl[i].interface_type = i;
		adapter->nx_sds_cb_tbl[i].data = NULL;
		adapter->nx_sds_cb_tbl[i].registered = 0;
		adapter->nx_sds_cb_tbl[i].refcnt = 0;
	} 

	return 0;
}

static void nx_init_status_handler(struct unm_adapter_s *adapter)
{
	int i = 0;

	for (i = 0; i < NX_NIC_CB_MAX; i++) {
		spin_lock_init(&adapter->nx_sds_cb_tbl[i].lock);
	}

	nx_init_status_msg_handler(adapter);
	nx_init_status_callback_handler(adapter);
}

static void unm_nic_v34_free_ring_context_in_fw(struct unm_adapter_s *adapter)
{
	int func_id;

	func_id = adapter->portnum;

	nx_fw_cmd_create_tx_ctx_free(adapter->nx_dev, 
			adapter->nx_dev->tx_ctxs[0]);

	read_lock(&adapter->adapter_lock);

	UNM_NIC_PCI_WRITE_32(UNM_CTX_RESET | func_id,
			     CRB_NORMALIZE(adapter,
					   CRB_CTX_SIGNATURE_REG(func_id)));

	read_unlock(&adapter->adapter_lock);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
/*
 * In unm_nic_down(), we must wait for any pending callback requests into
 * unm_watchdog_task() to complete; eg otherwise the watchdog_timer could be
 * reenabled right after it is deleted in unm_nic_down().
 * FLUSH_SCHEDULED_WORK() does this synchronization.
 *
 * Normally, schedule_work()/flush_scheduled_work() could have worked, but
 * unm_nic_close() is invoked with kernel rtnl lock held. netif_carrier_off()
 * call in unm_nic_close() triggers a schedule_work(&linkwatch_work), and a
 * subsequent call to flush_scheduled_work() in unm_nic_down() would cause
 * linkwatch_event() to be executed which also attempts to acquire the rtnl
 * lock thus causing a deadlock.
 */
#define	SCHEDULE_WORK(tp)	queue_work(unm_workq, tp)
#define	FLUSH_SCHEDULED_WORK()	flush_workqueue(unm_workq)
static struct workqueue_struct *unm_workq;
static void unm_watchdog(unsigned long);
static void unm_watchdog_fw_reset(unsigned long);
#else
#define SCHEDULE_WORK(tp) schedule_task(tp);
#if (defined (ESX_3X) || defined (ESX_3X_COS))
static void unm_watchdog(unsigned long);
static void unm_watchdog_fw_reset(unsigned long);
#endif
#define	FLUSH_SCHEDULED_WORK() flush_scheduled_tasks()
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0) */

#if defined(CONFIG_PCI_MSI)
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,8)) || defined(ESX_3X))
#define	NX_CONFIG_MSI_X
#endif
#endif

static char *
nx_errorcode2string(int rcode)
{
        switch (rcode) {
        case NX_RCODE_SUCCESS         : return "Success";
        case NX_RCODE_NO_HOST_MEM     : return "Error: No Host Memory";
        case NX_RCODE_NO_HOST_RESOURCE: return "Error: No Host Resources";
        case NX_RCODE_NO_CARD_CRB     : return "Error: No Card CRB";
        case NX_RCODE_NO_CARD_MEM     : return "Error: No Card Memory";
        case NX_RCODE_NO_CARD_RESOURCE: return "Error: No Card Resources";
        case NX_RCODE_INVALID_ARGS    : return "Error: Invalid Args";
        case NX_RCODE_INVALID_ACTION  : return "Error: Invalid Action";
        case NX_RCODE_INVALID_STATE   : return "Error: Invalid State";
        case NX_RCODE_NOT_SUPPORTED   : return "Error: Not Supported";
        case NX_RCODE_NOT_PERMITTED   : return "Error: Not Permitted";
        case NX_RCODE_NOT_READY       : return "Error: Not Ready";
        case NX_RCODE_DOES_NOT_EXIST  : return "Error: Does Not Exist";
        case NX_RCODE_ALREADY_EXISTS  : return "Error: Already Exists";
        case NX_RCODE_BAD_SIGNATURE   : return "Error: Bad Signature";
        case NX_RCODE_CMD_NOT_IMPL    : return "Error: Cmd Not Implemented";
        case NX_RCODE_CMD_INVALID     : return "Error: Cmd Invalid";
        case NX_RCODE_TIMEOUT         : return "Error: Timed Out";
        case NX_RCODE_CMD_FAILED      : return "Error: Cmd Failed";
        case NX_RCODE_MAX_EXCEEDED    : return "Error: Max Exceeded";
        case NX_RCODE_MAX:
        default:
                return "Error: Unknown code";
        }
}

/*
 * Allocate non-paged, non contiguous memory . Tag can be used for debug
 * purposes.
 */

U32 nx_os_alloc_mem(nx_dev_handle_t handle, void** addr,U32 len, U32 flags,
		    U32 dbg_tag)
{
	*addr = kmalloc(len, flags);
	if (*addr == NULL)
		return -ENOMEM;
	return 0;
}

/*
 * Free non-paged non contiguous memory
*/
U32 nx_os_free_mem(nx_dev_handle_t handle, void *addr, U32 len, U32 flags)
{
	kfree(addr);
	return 0;
}

void nx_os_nic_reg_read_w0(nx_dev_handle_t handle, U32 index, U32 * value)
{
	struct unm_adapter_s *adapter = (struct unm_adapter_s *)handle;
	*value = NXRD32(adapter, index);
}

void nx_os_nic_reg_write_w0(nx_dev_handle_t handle, U32 index, U32 value)
{
	struct unm_adapter_s *adapter = (struct unm_adapter_s *)handle;
	NXWR32(adapter, index, value);
}

void nx_os_nic_reg_read_w1(nx_dev_handle_t handle, U64 off, U32 * value)
{
	struct unm_adapter_s *adapter = (struct unm_adapter_s *)handle;
	*value = NXRD32(adapter, off);
}

void nx_os_nic_reg_write_w1(nx_dev_handle_t handle, U64 off, U32 val)
{
	struct unm_adapter_s *adapter = (struct unm_adapter_s *)handle;
	NXWR32(adapter, off, val);
}

/*
 * Allocate non-paged dma memory
 */
U32 nx_os_alloc_dma_mem(nx_dev_handle_t handle, void** vaddr,
			nx_dma_addr_t* paddr, U32 len, U32 flags)
{
	struct unm_adapter_s *adapter = (struct unm_adapter_s *)handle;
	*vaddr = pci_alloc_consistent(adapter->pdev, len, paddr);
	if ((unsigned long long)((*paddr) + len) < adapter->dma_mask) {
		return 0;
	}
	pci_free_consistent(adapter->pdev, len, *vaddr, *paddr);
	paddr = NULL;
	return NX_RCODE_NO_HOST_MEM;
}

void nx_os_free_dma_mem(nx_dev_handle_t handle, void *vaddr,
			nx_dma_addr_t paddr, U32 len, U32 flags)
{
	struct unm_adapter_s *adapter = (struct unm_adapter_s *)handle;
	pci_free_consistent(adapter->pdev, len, vaddr, paddr);
}

U32 nx_os_send_cmd_descs(nx_host_tx_ctx_t *ptx_ctx, nic_request_t *req,
			 U32 nr_elements)
{
	nx_dev_handle_t handle = ptx_ctx->nx_dev->nx_drv_handle;
	struct unm_adapter_s *adapter = (struct unm_adapter_s *)handle;
	U32 rv;

	rv = nx_nic_send_cmd_descs(adapter->netdev,
				   (cmdDescType0_t *) req, nr_elements);

	return rv;
}

nx_rcode_t nx_os_event_wait_setup(nx_dev_handle_t drv_handle,
				  nic_request_t *req, U64 *rsp_word,
				  nx_os_wait_event_t *wait)
{
	struct unm_adapter_s *adapter = (struct unm_adapter_s *)drv_handle;
	uint8_t  comp_id = 0;	
	uint8_t  index   = 0;
	uint64_t bit_map = 0;
	uint64_t i       = 0;

	init_waitqueue_head(&wait->wq);
	wait->active   = 1;
	wait->trigger  = 0;
	wait->rsp_word = rsp_word;

	for (i = 0; i < NX_MAX_COMP_ID; i++) {
		index   = (uint8_t)(i & 0xC0) >> 6;
		bit_map = (uint64_t)(1 << (i & 0x3F));

		if (!(bit_map & adapter->wait_bit_map[index])) {
			adapter->wait_bit_map[index] |= bit_map;
			comp_id = (uint8_t)i;			
			break;
		}
	}

	if (i >= NX_MAX_COMP_ID) {
		nx_nic_print6(adapter, "%s: completion index exceeds max of 255\n",
			      __FUNCTION__);
		return NX_RCODE_CMD_FAILED;
	}

	wait->comp_id = comp_id;
	req->body.cmn.req_hdr.comp_id = comp_id;

	list_add(&wait->list, &adapter->wait_list);

	return NX_RCODE_SUCCESS;
}

nx_rcode_t nx_os_event_wait(nx_dev_handle_t drv_handle,
			    nx_os_wait_event_t *wait,
			    I32 utimelimit)
{
	struct unm_adapter_s *adapter = (struct unm_adapter_s *)drv_handle;
	U32 rv = NX_RCODE_SUCCESS;
	uint8_t  index   = 0;
	uint64_t bit_map = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
	DEFINE_WAIT(wq_entry);
#else
	DECLARE_WAITQUEUE(wq_entry, current);
#endif

#ifdef ESX_3X
	init_waitqueue_entry(&wq_entry, current);
#endif

	while (wait->trigger == 0) {
		if (utimelimit <= 0) {
			nx_nic_print6(adapter, "%s: timelimit up\n", __FUNCTION__);
			rv = NX_RCODE_TIMEOUT;
			break;
		}
		PREPARE_TO_WAIT(&wait->wq, &wq_entry, TASK_INTERRUPTIBLE);
		/* schedule out for 100ms */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,7)
		msleep(100);
#else
		SCHEDULE_TIMEOUT(&wait->wq, (HZ / 10), NULL);
#endif
		utimelimit -= (100000);
	}

	index   = (wait->comp_id & 0xC0) >> 6;
	bit_map = (uint64_t)(1 << (((uint64_t)wait->comp_id) & 0x3F));
	adapter->wait_bit_map[index] &= ~bit_map;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
	finish_wait(&wait->wq, &wq_entry);
#else
	current->state = TASK_RUNNING;
	remove_wait_queue(&wait->wq, &wq_entry);
#endif
	list_del(&wait->list);
	
	return rv;
}

nx_rcode_t nx_os_event_wakeup_on_response(nx_dev_handle_t drv_handle,
					  nic_response_t *rsp)
{
	struct unm_adapter_s *adapter   = (struct unm_adapter_s *)drv_handle;
	nx_os_wait_event_t   *wait      = NULL;
	struct list_head     *ptr       = NULL;
	U8  compid   = rsp->rsp_hdr.nic.compid;
	U64 rsp_word = rsp->body.word;
	int found    = 0;

	if (rsp->rsp_hdr.nic.opcode == NX_NIC_C2H_OPCODE_LRO_DELETE_RESPONSE
		|| rsp->rsp_hdr.nic.opcode == NX_NIC_C2H_OPCODE_LRO_ADD_FAILURE_RESPONSE) {
		nx_handle_lro_response(drv_handle, rsp);
		return NX_RCODE_SUCCESS;
	}

	if (rsp->rsp_hdr.nic.opcode ==
			NX_NIC_C2H_OPCODE_GET_LINKEVENT_RESPONSE) {

		nic_handle_linkevent_response(drv_handle, rsp);
		return NX_RCODE_SUCCESS;
	}

	nx_nic_print6(adapter, "%s: 0x%x: %d 0x%llx\n",
		      __FUNCTION__, rsp->rsp_hdr.nic.opcode,
		      rsp->rsp_hdr.nic.compid, rsp_word);

	list_for_each(ptr, &adapter->wait_list) {
		wait = list_entry(ptr, nx_os_wait_event_t, list);
		if (wait->comp_id == compid) {
			found = 1;
			break;
		}
	}

	if (!found) {
		nx_nic_print4(adapter, "%s: entry with comp_id = %d not found\n",
			      __FUNCTION__, compid);
		return NX_RCODE_CMD_FAILED;
	}

	if (wait->active != 1) {
		nx_nic_print4(adapter, "%s: 0x%x: id %d not active\n",
			      __FUNCTION__, rsp->rsp_hdr.nic.opcode, compid);
		return NX_RCODE_CMD_FAILED;
	}

	if (wait->rsp_word != NULL) {
		*(wait->rsp_word) = rsp_word;
	}

	wait->trigger = 1;
	wait->active  = 0;

#ifdef ESX_3X
	vmk_thread_wakeup(&wait->wq);
#else
	wake_up_interruptible(&wait->wq);
#endif

	return NX_RCODE_SUCCESS;
}

#ifdef NEW_NAPI

void nx_init_napi (struct unm_adapter_s *adapter)
{

	int num_sds_rings = adapter->num_sds_rings;
	int ring;

	for(ring = 0; ring < num_sds_rings; ring++) {
		netif_napi_add(adapter->netdev,
				&adapter->host_sds_rings[ring].napi,
				nx_nic_poll_sts,
				UNM_NETDEV_WEIGHT);
	}
}
/*
 * Function to enable napi interface for all rss rings
 */

void nx_napi_enable(struct unm_adapter_s *adapter)
{

	int num_sds_rings = adapter->num_sds_rings;
	int ring;

	for(ring = 0; ring < num_sds_rings; ring++) {
		napi_enable( &adapter->host_sds_rings[ring].napi);
		adapter->host_sds_rings[ring].napi_enable = 1;
	}
}

/*
 * Function to disable napi interface for all rss rings
 */

void nx_napi_disable(struct unm_adapter_s *adapter)
{

	int num_sds_rings = adapter->num_sds_rings;
	int ring;

	for(ring = 0; ring < num_sds_rings; ring++) {
		if(adapter->host_sds_rings[ring].napi_enable == 1) {
			napi_disable(&adapter->host_sds_rings[ring].napi);
			adapter->host_sds_rings[ring].napi_enable = 0;
		}
	}
}
#elif defined(UNM_NIC_NAPI) 

void nx_init_napi (struct unm_adapter_s *adapter)
{
	return;
}

void nx_napi_disable(struct unm_adapter_s *adapter)
{
	int num_sds_rings = adapter->num_sds_rings;
	int ring;

	for(ring = 0; ring < num_sds_rings; ring++) {
		netif_poll_disable(adapter->host_sds_rings[ring].netdev);
	}

	return;
}

void nx_napi_enable(struct unm_adapter_s *adapter)
{
	int num_sds_rings = adapter->num_sds_rings;
	int ring;

	for(ring = 0; ring < num_sds_rings; ring++) {
		netif_poll_enable(adapter->host_sds_rings[ring].netdev);
	}
	return;
}
#else
void nx_init_napi (struct unm_adapter_s *adapter)
{
	return;
}

void nx_napi_disable(struct unm_adapter_s *adapter)
{
	int ring;
#ifdef ESX_3X
	if (adapter->flags & UNM_NIC_MSIX_ENABLED) {
		for(ring = 0; ring < adapter->num_sds_rings; ring++) {
			if(adapter->nx_dev->rx_ctxs[ring] != NULL)
				udelay(2000);
		}
	} else if (adapter->flags & UNM_NIC_MSI_ENABLED) {
		udelay(2500);
	} else {	// Legacy
		vmk_synchronize_irq(adapter->netdev->irq);
	}
#elif defined (ESX_3X_COS)
	if (adapter->flags & UNM_NIC_MSIX_ENABLED) {
		for(ring = 0; ring < adapter->num_sds_rings; ring++) {
			if(adapter->nx_dev->rx_ctxs[ring] != NULL)
				synchronize_irq();			
		}
	} else {
		synchronize_irq();
	}	
#endif
	return;
}

void nx_napi_enable(struct unm_adapter_s *adapter)
{
	return;
}

#endif


static int nx_alloc_adapter_sds_rings(struct unm_adapter_s *adapter) 
{
	sds_host_ring_t *host_ring;


#ifdef ESX

	if (adapter->multi_ctx) {
		adapter->num_sds_rings  = adapter->num_rx_queues + 1; 
	} else 
#endif
	{
		adapter->num_sds_rings = adapter->max_possible_rss_rings;
	}

	host_ring = kmalloc(sizeof(sds_host_ring_t) * adapter->num_sds_rings, 
			    GFP_KERNEL);

	if (host_ring == NULL) {
		nx_nic_print3(NULL, "Couldn't allocate memory for SDS ring\n");
		return -ENOMEM;
	}

	memset(host_ring, 0, sizeof(sds_host_ring_t) * adapter->num_sds_rings);

	adapter->host_sds_rings = host_ring;

	return 0;
}

inline void unm_nic_update_cmd_producer(struct unm_adapter_s *adapter,
					uint32_t crb_producer)
{

	if (adapter->crb_addr_cmd_producer)
        	NXWR32(adapter, adapter->crb_addr_cmd_producer, crb_producer);
	return;

}

inline void unm_nic_update_cmd_consumer(struct unm_adapter_s *adapter,
					uint32_t crb_consumer)
{
    int data = crb_consumer;

	switch (adapter->portnum) {
	case 0:
		NXWR32(adapter, CRB_CMD_CONSUMER_OFFSET, data);
        return;
	case 1:
		NXWR32(adapter, CRB_CMD_CONSUMER_OFFSET_1, data);
		return;
	case 2:
		NXWR32(adapter, CRB_CMD_CONSUMER_OFFSET_2, data);
		return;
	case 3:
		NXWR32(adapter, CRB_CMD_CONSUMER_OFFSET_3, data);
		return;
	default:
		nx_nic_print3(adapter,
				"Unable to update CRB_CMD_PRODUCER_OFFSET "
				"for invalid PCI function id %d\n",
				 adapter->portnum);
		return;
        }
}

/*
 * Checks the passed in module parameters for validity else it sets them to
 * default sane values.
 */
static void nx_verify_module_params(void)
{
	if (rdesc_1g < NX_MIN_DRIVER_RDS_SIZE ||
	    rdesc_1g > NX_MAX_SUPPORTED_RDS_SIZE ||
	    (rdesc_1g & (rdesc_1g - 1))) {
		nx_nic_print5(NULL, "Invalid module param rdesc_1g[%d]. "
			      "Setting it to %d\n",
			      rdesc_1g, NX_DEFAULT_RDS_SIZE_1G);
		rdesc_1g = NX_DEFAULT_RDS_SIZE_1G;
	}

	if (rdesc_10g < NX_MIN_DRIVER_RDS_SIZE ||
	    rdesc_10g > NX_MAX_SUPPORTED_RDS_SIZE ||
	    (rdesc_10g & (rdesc_10g - 1))) {
		nx_nic_print5(NULL, "Invalid module param rdesc_10g[%d]. "
			      "Setting it to %d\n",
			      rdesc_10g, NX_DEFAULT_RDS_SIZE);
		rdesc_10g = NX_DEFAULT_RDS_SIZE;
	}

	if (jumbo_desc < NX_MIN_DRIVER_RDS_SIZE ||
	    jumbo_desc > NX_MAX_JUMBO_RDS_SIZE ||
	    (jumbo_desc & (jumbo_desc - 1))) {
		nx_nic_print5(NULL, "Invalid module param jumbo_desc[%d]. "
			      "Setting it to %d\n",
			      jumbo_desc, NX_MAX_JUMBO_RDS_SIZE);
		jumbo_desc = NX_MAX_JUMBO_RDS_SIZE;
	}

	if (jumbo_desc_1g < NX_MIN_DRIVER_RDS_SIZE ||
	    jumbo_desc_1g > NX_MAX_JUMBO_RDS_SIZE ||
	    (jumbo_desc_1g & (jumbo_desc_1g - 1))) {
		nx_nic_print5(NULL, "Invalid module param jumbo_desc_1g[%d]. "
			      "Setting it to %d\n",
			      jumbo_desc_1g, NX_DEFAULT_JUMBO_RDS_SIZE_1G);
		jumbo_desc_1g = NX_DEFAULT_JUMBO_RDS_SIZE_1G;
	}

	if (port_mode != UNM_PORT_MODE_802_3_AP &&
	    port_mode != UNM_PORT_MODE_XG &&
	    port_mode != UNM_PORT_MODE_AUTO_NEG_1G &&
	    port_mode != UNM_PORT_MODE_AUTO_NEG_XG &&
	    port_mode != UNM_PORT_MODE_AUTO_NEG) {
		nx_nic_print4(NULL, "Warning: Invalid value for port_mode "
			      "Valid values are [3-6]. Resetting to Auto "
			      "Negotiation.\n");

		port_mode = UNM_PORT_MODE_AUTO_NEG;
	}
	nx_nic_print6(NULL, "port_mode is %d\n", port_mode);

	if ((wol_port_mode != UNM_PORT_MODE_802_3_AP) &&
	    (wol_port_mode != UNM_PORT_MODE_XG) &&
	    (wol_port_mode != UNM_PORT_MODE_AUTO_NEG_1G) &&
	    (wol_port_mode != UNM_PORT_MODE_AUTO_NEG_XG)) {
		wol_port_mode = UNM_PORT_MODE_AUTO_NEG;
	}
	nx_nic_print6(NULL, "wol_port_mode is %d\n", wol_port_mode);

	if (fw_load < 0 || fw_load > LOAD_FW_LAST_INVALID) {
		fw_load = LOAD_FW_WITH_COMPARISON;
		nx_nic_print4(NULL, "Invalid value for fw_load valid values "
			      "are[0-3]. Resetting to default (1 - Load after "
			      "flash compare).\n");
	}
#ifdef ESX
	if( num_rx_queues < INVALID_INIT_NETQ_RX_CONTEXTS || num_rx_queues > NETQ_MAX_RX_CONTEXTS) {
		num_rx_queues = INVALID_INIT_NETQ_RX_CONTEXTS;
		nx_nic_print4(NULL, "Invalid value for num_rx_queues, valid value is "
				"between 0 and %d. Number of netqueues are calculated at run-time.\n", NETQ_MAX_RX_CONTEXTS);
	}
#endif
}

/*
 *
 */
static void unm_check_options(unm_adapter *adapter)
{
	adapter->MaxJumboRxDescCount = jumbo_desc;
	adapter->MaxLroRxDescCount = lro_desc;

	switch (adapter->ahw.boardcfg.board_type) {
	case UNM_BRDTYPE_P3_XG_LOM:
	case UNM_BRDTYPE_P3_HMEZ:
	case UNM_BRDTYPE_P2_SB31_10G:
	case UNM_BRDTYPE_P2_SB31_10G_CX4:

	case UNM_BRDTYPE_P3_10G_CX4:
	case UNM_BRDTYPE_P3_10G_CX4_LP:
	case UNM_BRDTYPE_P3_IMEZ:
	case UNM_BRDTYPE_P3_10G_SFP_PLUS:
	case UNM_BRDTYPE_P3_10G_XFP:
	case UNM_BRDTYPE_P3_10000_BASE_T:
		adapter->msix_supported = 1;
		adapter->max_possible_rss_rings = CARD_SIZED_MAX_RSS_RINGS;
		adapter->MaxRxDescCount = rdesc_10g;
		break;

	case UNM_BRDTYPE_P2_SB31_10G_IMEZ:
	case UNM_BRDTYPE_P2_SB31_10G_HMEZ:
		adapter->msix_supported = 0;
		adapter->max_possible_rss_rings = 1;
		adapter->MaxRxDescCount = rdesc_10g;
		break;

	case UNM_BRDTYPE_P3_REF_QG:
	case UNM_BRDTYPE_P3_4_GB:
	case UNM_BRDTYPE_P3_4_GB_MM:
		adapter->msix_supported = 1;
		adapter->max_possible_rss_rings = 1;
                adapter->MaxRxDescCount = rdesc_1g;
                adapter->MaxJumboRxDescCount = jumbo_desc_1g;
		break;

	case UNM_BRDTYPE_P2_SB35_4G:
	case UNM_BRDTYPE_P2_SB31_2G:
		adapter->msix_supported = 0;
		adapter->max_possible_rss_rings = 1;
		adapter->MaxRxDescCount = rdesc_1g;
		break;
	case UNM_BRDTYPE_P3_10G_TP:
		if (adapter->portnum < 2) {
			adapter->msix_supported = 1;
			adapter->max_possible_rss_rings =
				CARD_SIZED_MAX_RSS_RINGS;
			adapter->MaxRxDescCount = rdesc_10g;
		} else {
			adapter->msix_supported = 1;
			adapter->max_possible_rss_rings = 1;
			adapter->MaxRxDescCount = rdesc_1g;
			adapter->MaxJumboRxDescCount = jumbo_desc_1g;
		}
		break;
	default:
		adapter->msix_supported = 0;
		adapter->max_possible_rss_rings = 1;
		adapter->MaxRxDescCount = rdesc_1g;

		nx_nic_print4(NULL, "Unknown board type(0x%x)\n",
			      adapter->ahw.boardcfg.board_type);
		break;
	}			// end of switch

	if (tx_desc >= 256 && tx_desc <= NX_MAX_CMD_DESCRIPTORS &&
	    !(tx_desc & (tx_desc - 1))) {
		adapter->MaxTxDescCount = tx_desc;
	} else {
		nx_nic_print5(NULL, "Ignoring module param tx_desc. "
			      "Setting it to %d\n",
			      NX_MAX_CMD_DESCRIPTORS);
		adapter->MaxTxDescCount = NX_MAX_CMD_DESCRIPTORS;
	}

	/* If RSS is not enabled set rings to 1 */
	if (!rss_enable) {
		adapter->max_possible_rss_rings = 1;
	}

	nx_nic_print6(NULL, "Maximum Rx Descriptor count: %d\n",
		      adapter->MaxRxDescCount);
	nx_nic_print6(NULL, "Maximum Tx Descriptor count: %d\n",
		      adapter->MaxTxDescCount);
	nx_nic_print6(NULL, "Maximum Jumbo Descriptor count: %d\n",
		      adapter->MaxJumboRxDescCount);
	nx_nic_print6(NULL, "Maximum LRO Descriptor count: %d\n",
		      adapter->MaxLroRxDescCount);
	return;
}

#ifdef NETIF_F_TSO
#if (defined(USE_GSO_SIZE) || defined(ESX_4X))
#define TSO_SIZE(x)   ((x)->gso_size)
#else
#define TSO_SIZE(x)   ((x)->tso_size)
#endif
#endif //#ifdef NETIF_F_TSO

/*  PCI Device ID Table  */
#define NETXEN_PCI_ID(device_id)   PCI_DEVICE(PCI_VENDOR_ID_NX, device_id)

static struct pci_device_id unm_pci_tbl[] __devinitdata = {
	{NETXEN_PCI_ID(PCI_DEVICE_ID_NX_QG)},
	{NETXEN_PCI_ID(PCI_DEVICE_ID_NX_XG)},
	{NETXEN_PCI_ID(PCI_DEVICE_ID_NX_CX4)},
	{NETXEN_PCI_ID(PCI_DEVICE_ID_NX_IMEZ)},
	{NETXEN_PCI_ID(PCI_DEVICE_ID_NX_HMEZ)},
	{NETXEN_PCI_ID(PCI_DEVICE_ID_NX_IMEZ_DUP)},
	{NETXEN_PCI_ID(PCI_DEVICE_ID_NX_HMEZ_DUP)},
	{NETXEN_PCI_ID(PCI_DEVICE_ID_NX_P3_XG)},
	{0,}
};

MODULE_DEVICE_TABLE(pci, unm_pci_tbl);

#define SUCCESS 0
static int unm_get_flash_block(struct unm_adapter_s *adapter, int base,
			       int size, uint32_t * buf)
{
	int i, addr;
	uint32_t *ptr32;

	addr = base;
	ptr32 = buf;
	for (i = 0; i < size / sizeof(uint32_t); i++) {
		if (rom_fast_read(adapter, addr, ptr32) == -1) {
			return -1;
		}
		ptr32++;
		addr += sizeof(uint32_t);
	}
	if ((char *)buf + size > (char *)ptr32) {
		uint32_t local;

		if (rom_fast_read(adapter, addr, &local) == -1) {
			return -1;
		}
		memcpy(ptr32, &local, (char *)buf + size - (char *)ptr32);
	}

	return 0;
}

static int get_flash_mac_addr(struct unm_adapter_s *adapter, uint64_t mac[])
{
	uint32_t *pmac = (uint32_t *) & mac[0];
         if (NX_IS_REVISION_P3(adapter->ahw.revision_id)) {
               uint32_t temp, crbaddr;
               uint16_t *pmac16 = (uint16_t *)pmac;

               // FOR P3, read from CAM RAM

               int pci_func= adapter->ahw.pci_func;
               pmac16 += (4*pci_func);
               crbaddr = CRB_MAC_BLOCK_START +
                               (4 * ((pci_func/2) * 3))+
                               (4 * (pci_func & 1));

               temp = NXRD32(adapter, crbaddr);
               if (pci_func & 1) {
                       *pmac16++ = (temp >> 16);
                       temp = NXRD32(adapter, crbaddr+4);
                       *pmac16++ = (temp & 0xffff);
                       *pmac16++ = (temp >> 16);
                       *pmac16=0;
               } else {
                       *pmac16++ = (temp & 0xffff);
                       *pmac16++ = (temp >> 16);
			temp = NXRD32(adapter, crbaddr+4);
                       *pmac16++ = (temp & 0xffff);
                       *pmac16=0;
               }
               return 0;
       }


	if (unm_get_flash_block(adapter,
				USER_START + offsetof(unm_user_info_t,
						      mac_addr),
				FLASH_NUM_PORTS * sizeof(U64), pmac) == -1) {
		return -1;
	}
	if (*mac == ~0ULL) {
		if (unm_get_flash_block(adapter,
					USER_START_OLD +
					offsetof(unm_old_user_info_t, mac_addr),
					FLASH_NUM_PORTS * sizeof(U64),
					pmac) == -1) {
			return -1;
		}
		if (*mac == ~0ULL) {
			return -1;
		}
	}

	return 0;
}

/*
 * Initialize buffers required for the adapter in pegnet_nic case.
 */
static int initialize_dummy_dma(unm_adapter *adapter)
{
        uint64_t        addr;
        uint32_t        hi;
        uint32_t        lo;


	/*
	 * Allocate Dummy DMA space.
	 */
        adapter->dummy_dma.addr = pci_alloc_consistent(adapter->ahw.pdev,
						UNM_HOST_DUMMY_DMA_SIZE,
						&adapter->dummy_dma.phys_addr);
        if (adapter->dummy_dma.addr == NULL) {
                nx_nic_print3(NULL, "ERROR: Could not allocate dummy "
			      "DMA memory\n");
                return (-ENOMEM);
        }

	addr = (uint64_t) adapter->dummy_dma.phys_addr;
	hi = (addr >> 32) & 0xffffffff;
	lo = addr & 0xffffffff;

	read_lock(&adapter->adapter_lock);
        NXWR32(adapter, CRB_HOST_DUMMY_BUF_ADDR_HI, hi);
        NXWR32(adapter, CRB_HOST_DUMMY_BUF_ADDR_LO, lo);

	if (NX_IS_REVISION_P3(adapter->ahw.revision_id)) {
		NXWR32(adapter, CRB_HOST_DUMMY_BUF, DUMMY_BUF_INIT);
	}

	read_unlock(&adapter->adapter_lock);

	return (0);
}

/*
 * Free buffers for the offload part from the adapter.
 */
static void destroy_dummy_dma(unm_adapter *adapter)
{
	if (adapter->dummy_dma.addr) {
		pci_free_consistent(adapter->ahw.pdev, UNM_HOST_DUMMY_DMA_SIZE,
				    adapter->dummy_dma.addr,
				    adapter->dummy_dma.phys_addr);
		adapter->dummy_dma.addr = NULL;
	}
}

#define addr_needs_mapping(adapter, phys) \
        ((phys) & (~adapter->dma_mask))

#ifdef CONFIG_XEN

static inline int
in_dma_range(struct unm_adapter_s *adapter, dma_addr_t addr, unsigned int len)
{
	dma_addr_t last = addr + len - 1;

	if ((addr & ~PAGE_MASK) + len > PAGE_SIZE) {
		return 0;
	}

	return !addr_needs_mapping(adapter, last);
}

#elif !defined(ESX_3X)

static inline int
in_dma_range(struct unm_adapter_s *adapter, dma_addr_t addr, unsigned int len)
{
	dma_addr_t last = addr + len - 1;

	return !addr_needs_mapping(adapter, last);
}

#endif

#ifdef ESX_3X

static inline int
try_map_skb_data(struct unm_adapter_s *adapter, struct sk_buff *skb,
		 size_t size, int direction, dma_addr_t * dma)
{
	*dma = skb->headMA;
	return 0;
}

static inline int
try_map_frag_page(struct unm_adapter_s *adapter,
		  struct page *page, unsigned long offset, size_t size,
		  int direction, dma_addr_t * dma)
{
	*dma = page_to_phys(page) + offset;
	return 0;
}

#else /* NATIVE LINUX */

static inline int
try_map_skb_data(struct unm_adapter_s *adapter, struct sk_buff *skb,
		 size_t size, int direction, dma_addr_t * dma)
{
	struct pci_dev *hwdev = adapter->pdev;
	dma_addr_t dma_temp;

	dma_temp = pci_map_single(hwdev, skb->data, size, direction);
	if (nx_pci_dma_mapping_error(hwdev, dma_temp))
		return -EIO;

	*dma = dma_temp;

	return 0;
}

static inline int
try_map_frag_page(struct unm_adapter_s *adapter,
		  struct page *page, unsigned long offset, size_t size,
		  int direction, dma_addr_t * dma)
{
	struct pci_dev *hwdev = adapter->pdev;
	dma_addr_t dma_temp;

	dma_temp = pci_map_page(hwdev, page, offset, size, direction);
	if (nx_pci_dma_mapping_error(hwdev, dma_temp))
		return -EIO;

	*dma = dma_temp;

	return 0;
}

#endif /* ESX */

#define	ADAPTER_LIST_SIZE	12
int unm_cards_found;

#if defined(CONFIG_PCI_MSI)
static void nx_reset_msix_bit(struct pci_dev *pdev)
{
#ifdef	UNM_HWBUG_9_WORKAROUND
	u32 control = 0x00000000;
	int pos;

	pos = pci_find_capability(pdev, PCI_CAP_ID_MSIX);
	pci_write_config_dword(pdev, pos, control);
#endif
}
#endif

static void cleanup_adapter(struct unm_adapter_s *adapter)
{
	struct pci_dev *pdev;
	struct net_device *netdev;

	if (!adapter) {
		return;
	}

	if (adapter->testCtx.tx_user_packet_data != NULL) {
		kfree(adapter->testCtx.tx_user_packet_data);
	}
	pdev = adapter->ahw.pdev;
	netdev = adapter->netdev;

	if (pdev == NULL || netdev == NULL) {
		return;
	}
#ifdef ESX
	if(adapter->multi_ctx) {
		if(adapter->nx_dev && adapter->nx_dev->saved_rules) 
			kfree(adapter->nx_dev->saved_rules);
	}
#endif
	unm_cleanup_lro(adapter);
	unm_cleanup_proc_entries(adapter);
#if defined(CONFIG_PCI_MSI)
	if ((adapter->flags & UNM_NIC_MSIX_ENABLED)) {

		pci_disable_msix(pdev);
		nx_reset_msix_bit(pdev);
	} else if ((adapter->flags & UNM_NIC_MSI_ENABLED)) {
		pci_disable_msi(pdev);
	}
#endif

        nx_os_dev_free(adapter->nx_dev);

	if (adapter->host_sds_rings != NULL) {
		kfree(adapter->host_sds_rings);
		adapter->host_sds_rings = NULL;
	}

	if (adapter->ahw.pci_base0) {
		iounmap((uint8_t *) adapter->ahw.pci_base0);
		adapter->ahw.pci_base0 = 0UL;
	}
	if (adapter->ahw.pci_base1) {
		iounmap((uint8_t *) adapter->ahw.pci_base1);
		adapter->ahw.pci_base1 = 0UL;
	}
	if (adapter->ahw.pci_base2) {
		iounmap((uint8_t *) adapter->ahw.pci_base2);
		adapter->ahw.pci_base2 = 0UL;
	}
	if (adapter->ahw.db_base) {
		iounmap((uint8_t *) adapter->ahw.db_base);
		adapter->ahw.db_base = 0UL;
	}

	unm_pci_release_regions(pdev);
	pci_disable_device(pdev);
}

/*
 *
 */
static void nx_p2_start_bootloader(struct unm_adapter_s *adapter)
{
	int timeout = 0;
	u32 val = 0;

	UNM_NIC_PCI_WRITE_32(UNM_BDINFO_MAGIC,
			     CRB_NORMALIZE(adapter, UNM_CAM_RAM(0x1fc)));
	UNM_NIC_PCI_WRITE_32(1, CRB_NORMALIZE(adapter,
					      UNM_ROMUSB_GLB_PEGTUNE_DONE));
	/*
	 * bootloader0 writes zero to UNM_CAM_RAM(0x1fc) before calling
	 * bootloader1
	 */
	if (fw_load != 0) {
		do {
			val = UNM_NIC_PCI_READ_32(CRB_NORMALIZE(adapter,
								UNM_CAM_RAM
								(0x1fc)));
			if (timeout > 10000) {
				nx_nic_print3(adapter, "The bootloader did not"
					      " increment the CAM_RAM(0x1fc)"
					      " register\n");
				break;
			}
			timeout++;
			nx_msleep(1);
		} while (val == UNM_BDINFO_MAGIC);
		/*Halt the bootloader now */
		UNM_NIC_PCI_WRITE_32(1, CRB_NORMALIZE(adapter,
						      UNM_ROMUSB_GLB_CAS_RST));
	}
}

/*
 *
 */
int nx_p2_check_hw_init(struct unm_adapter_s *adapter)
{
	u32 val = 0;
	int ret = 0;

	val = NXRD32(adapter, UNM_CAM_RAM(0x1fc));
	nx_nic_print7(adapter, "read 0x%08x for init reg.\n", val);
	if (val == 0x55555555) {

		/* This is the first boot after power up */
		val = NXRD32(adapter,
					UNM_ROMUSB_GLB_SW_RESET);
		nx_nic_print7(adapter, "read 0x%08x for reset reg.\n", val);
		if (val != 0x80000f) {
			nx_nic_print3(NULL, "ERROR: HW init sequence.\n");
			ret = -1;
		}

		nx_p2_start_bootloader(adapter);
	}
	return ret;
}

/* nx_set_dma_mask()
 * Set dma mask depending upon kernel type and device capability
 */
static int nx_set_dma_mask(struct unm_adapter_s *adapter, uint8_t revision_id)
{
	struct pci_dev *pdev = adapter->ahw.pdev;
	int err;
	uint64_t mask;

#ifndef CONFIG_IA64
	if (revision_id >= NX_P3_B0) {
		adapter->dma_mask = DMA_39BIT_MASK;
		mask = DMA_64BIT_MASK;
	} else if (revision_id == NX_P2_C1) {
		adapter->dma_mask = DMA_35BIT_MASK;
#ifdef ESX
                mask = DMA_64BIT_MASK; /* Workaround for 4.0 netdev dma mask assertion. */
#else
		mask = DMA_35BIT_MASK;
#endif
	} else {
		adapter->dma_mask = DMA_32BIT_MASK;
		mask = DMA_32BIT_MASK;
#ifdef ESX
		adapter->gfp_mask = GFP_DMA32;
		nx_nic_print5(adapter, "dma_mask = 0x%llx gfp_mask = 0x%x\n",
			      adapter->dma_mask,
		              adapter->gfp_mask);
#endif
		goto set_32_bit_mask;
	}

#ifdef ESX
	/*
	 * If the physical memory range is larger than the DMA range, we use the lower 4G memory
	 * for page allocation; otherwise, we are safe to use any available pages.
	 */
	adapter->gfp_mask = adapter->dma_mask < dma_get_required_mask(&pdev->dev) ? GFP_DMA32 : 0;
	nx_nic_print5(adapter, "dma_mask = 0x%llx gfp_mask = 0x%x\n",
		      adapter->dma_mask,
		      adapter->gfp_mask);
#endif


#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
	/*
	 * Consistent DMA mask is set to 32 bit because it cannot be set to
	 * 35 bits. For P3 also leave it at 32 bits for now. Only the rings
	 * come off this pool.
	 */
	if (pci_set_dma_mask(pdev, mask) == 0 &&
	    pci_set_consistent_dma_mask(pdev, DMA_32BIT_MASK) == 0)
#else
	if (pci_set_dma_mask(pdev, mask) == 0)
#endif
	{
#ifdef ESX
		if (revision_id == NX_P2_C0)
			adapter->pci_using_dac = 0;
		else
#endif
			adapter->pci_using_dac = 1;

		return (0);
	}
#else /* CONFIG_IA64 */
	adapter->dma_mask = DMA_32BIT_MASK;
#endif /* CONFIG_IA64 */

      set_32_bit_mask:
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
	if ((err = pci_set_dma_mask(pdev, PCI_DMA_32BIT)) ||
	    (err = pci_set_consistent_dma_mask(pdev, PCI_DMA_32BIT)))
#else
	if ((err = pci_set_dma_mask(pdev, PCI_DMA_32BIT)))
#endif
	{
		nx_nic_print3(adapter, "No usable DMA configuration, "
			      "aborting:%d\n", err);
                return err;
        }

	adapter->pci_using_dac = 0;
	return (0);
}

static void nx_set_num_rx_queues(unm_adapter *adapter)
{
	uint32_t pcie_setup_func1 = 0, pcie_setup_func2 = 0;
	uint32_t func_bitmap1 = 0, func_bitmap2 = 0;
	uint32_t func_count = 0, i;

	if(num_rx_queues == INVALID_INIT_NETQ_RX_CONTEXTS) {
		pcie_setup_func1 = NXRD32(adapter, UNM_PCIE_REG(PCIE_SETUP_FUNCTION));
		pcie_setup_func2 = NXRD32(adapter, UNM_PCIE_REG(PCIE_SETUP_FUNCTION2));

		/* Function Bitmap: Bit(0)=F0, Bit(2)=F1, Bit(4)=F2, Bit(6)=F3 */
		
		func_bitmap1 = pcie_setup_func1 & 0x55;	
		func_bitmap2 = pcie_setup_func2 & 0x55;	

		for(i=0; i<=6; i+=2) {
			if((func_bitmap1 >> i) & 0x1) 
				func_count++;
			if((func_bitmap2 >> i) & 0x1) 
				func_count++;
		}	
		if(func_count > 4) {
			adapter->num_rx_queues = NETQ_MAX_RX_CONTEXTS >> 2; 
		} else if(func_count > 2) {
			adapter->num_rx_queues = NETQ_MAX_RX_CONTEXTS >> 1; 
		} else {
			adapter->num_rx_queues = NETQ_MAX_RX_CONTEXTS;
		}
	} else {
		/* Honoring the user defined valid value for netqueue */
		adapter->num_rx_queues = num_rx_queues;
	}
	if(adapter->multi_ctx) {
		nx_nic_print4(NULL, "Number of RX queues per PCI FN are set to %d\n", adapter->num_rx_queues+1);
	}
}


static void nx_update_dma_mask(unm_adapter *adapter, struct pci_dev *pdev)
{
	int shift, change;
	uint64_t old_mask, mask;
#ifndef ESX
        int err;
#endif

#ifndef CONFIG_IA64
	change = 0;
	shift = NXRD32(adapter, CRB_DMA_SIZE);

	if (shift > 32)
		return;

	if (NX_IS_REVISION_P3(adapter->ahw.revision_id) && (shift > 9))
		change = 1;
	else if ((adapter->ahw.revision_id == NX_P2_C1) && (shift <= 4))
		change = 1;

	if (change) {
		old_mask = pdev->dma_mask;

		mask = (shift == 32) ? ~0ULL : ((1ULL << (32 + shift)) - 1);
#ifndef ESX
		/* ESX already sets actual mask to 64 bit */
		err = pci_set_dma_mask(pdev, mask);
		if (err) {
			err = pci_set_dma_mask(pdev, old_mask);
		} else
#endif
			adapter->dma_mask = mask;
	}
#endif /* CONFIG_IA64 */
}

static int
unm_nic_fill_adapter_macaddr_from_flash(struct unm_adapter_s *adapter)
{
	int i;
	unsigned char *p;
	uint64_t mac_addr[8 + 1];

	if (get_flash_mac_addr(adapter, mac_addr) != 0) {
		return -1;
	}

	if (NX_IS_REVISION_P3(adapter->ahw.revision_id)) {
		p = (unsigned char *)&mac_addr[adapter->ahw.pci_func];
	} else {
		p = (unsigned char *)&mac_addr[adapter->portnum];
	}

	for (i = 0; i < 6; ++i) {
		adapter->mac_addr[i] = p[5 - i];
	}

        if (!is_valid_ether_addr(adapter->mac_addr)) {
                nx_nic_print3(adapter, "Bad MAC address "
			      "%02x:%02x:%02x:%02x:%02x:%02x.\n",
			      adapter->mac_addr[0], adapter->mac_addr[1],
			      adapter->mac_addr[2], adapter->mac_addr[3],
			      adapter->mac_addr[4], adapter->mac_addr[5]);
                return -1;
        }

	return 0;
}

#if defined(CONFIG_PCI_MSI)
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,8)) || defined(ESX_3X))
/*
 * Initialize the msix entries.
 */
static void init_msix_entries(struct unm_adapter_s *adapter)
{
	int i;

	if(adapter->multi_ctx)
		adapter->num_msix = adapter->num_rx_queues + 1;
	else
		adapter->num_msix = 1;

	for (i = 0; i < adapter->num_msix; i++) {
		adapter->msix_entries[i].entry = i;
	}
}
#endif
#endif

static inline int unm_pci_region_offset(struct pci_dev *pdev, int region)
{
	unsigned long val;
	u32 control;

	switch (region) {
	case 0:
		val = 0;
		break;
	case 1:
		pci_read_config_dword(pdev, UNM_PCI_REG_MSIX_TBL, &control);
		val = control + UNM_MSIX_TBL_SPACE;
		break;
	}
	return val;
}

static inline int unm_pci_region_len(struct pci_dev *pdev, int region)
{
	unsigned long val;
	u32 control;
	switch (region) {
	case 0:
		pci_read_config_dword(pdev, UNM_PCI_REG_MSIX_TBL, &control);
		val = control;
		break;
	case 1:
		val = pci_resource_len(pdev, 0) -
		    unm_pci_region_offset(pdev, 1);
		break;
	}
	return val;
}

static int unm_pci_request_regions(struct pci_dev *pdev, char *res_name)
{
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,11)
	return pci_request_regions(pdev, res_name);
#else
	struct resource *res;
	unsigned int len;

	/*
	 * In P3 these memory regions might change and need to be fixed.
	 */
	len = pci_resource_len(pdev, 0);
	if (len <= NX_MSIX_MEM_REGION_THRESHOLD || !use_msi_x ||
	    unm_pci_region_len(pdev, 0) == 0 ||
	    unm_pci_region_len(pdev, 1) == 0) {
		res = request_mem_region(pci_resource_start(pdev, 0),
					 len, res_name);
		goto done;
	}

	/* In case of MSI-X  pci_request_regions() is not useful, because
	   pci_enable_msix() tries to reserve part of card's memory space for
	   MSI-X table entries and fails due to conflict, since nx_nic module
	   owns entire region.
	   soln : request region(s) leaving area needed for MSI-X alone */
	res = request_mem_region(pci_resource_start(pdev, 0) +
				 unm_pci_region_offset(pdev, 0),
				 unm_pci_region_len(pdev, 0), res_name);
	if (res == NULL) {
		goto done;
	}
	res = request_mem_region(pci_resource_start(pdev, 0) +
				 unm_pci_region_offset(pdev, 1),
				 unm_pci_region_len(pdev, 1), res_name);

	if (res == NULL) {
		release_mem_region(pci_resource_start(pdev, 0) +
				   unm_pci_region_offset(pdev, 0),
				   unm_pci_region_len(pdev, 0));
	}
      done:
	return (res == NULL);
#endif
}

static void unm_pci_release_regions(struct pci_dev *pdev)
{
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,11)
	pci_release_regions(pdev);
#else
	unsigned int len;

	len = pci_resource_len(pdev, 0);
	if (len <= NX_MSIX_MEM_REGION_THRESHOLD || !use_msi_x ||
	    unm_pci_region_len(pdev, 0) == 0 ||
	    unm_pci_region_len(pdev, 1) == 0) {
		release_mem_region(pci_resource_start(pdev, 0), len);
		return;
	}

	release_mem_region(pci_resource_start(pdev, 0) +
			   unm_pci_region_offset(pdev, 0),
			   unm_pci_region_len(pdev, 0));
	release_mem_region(pci_resource_start(pdev, 0) +
			   unm_pci_region_offset(pdev, 1),
			   unm_pci_region_len(pdev, 1));
#endif
}

static void nx_p3_do_chicken_settings(struct pci_dev *pdev,
				      struct unm_adapter_s *adapter)
{
        u32 c8c9value;
        u32 chicken;
        u32 pdevfuncsave;
	int pos;
        int data = 0;
	int ii;
        u32 control;

	chicken = NXRD32(adapter, UNM_PCIE_REG(PCIE_CHICKEN3));
	chicken &= 0xFCFFFFFF;	// clear chicken3.25:24
	// if gen1 and B0, set F1020 - if gen 2, do nothing
	// if gen2 set to F1000
	pos = pci_find_capability(pdev, PCI_CAP_ID_GEN);
	if (pos == 0xC0) {
		pci_read_config_dword(pdev, pos + 0x10, &data);
		if ((data & 0x000F0000) != 0x00020000) {
			chicken |= 0x01000000;  // set chicken3.24 if gen1
		}
		nx_nic_print4(NULL, "Gen2 strapping detected\n");
		c8c9value = 0xF1000;
	} else {
		chicken |= 0x01000000;  // set chicken3.24 if gen1
		nx_nic_print4(NULL, "Gen1 strapping detected\n");
		c8c9value = 0;
	}
	NXWR32(adapter, UNM_PCIE_REG(PCIE_CHICKEN3), chicken);

	pdevfuncsave = pdev->devfn;

	if ((pdevfuncsave & 0x07) != 0 || c8c9value == 0) {
		return;
	}

	for (ii = 0; ii < 8; ii++) {
		pci_read_config_dword(pdev, pos + 8, &control);
		pci_read_config_dword(pdev, pos + 8, &control);
		pci_write_config_dword(pdev, pos + 8, c8c9value);
		pdev->devfn++;
	}
	pdev->devfn = pdevfuncsave;
}

#if	(defined(NX_CONFIG_MSI_X) && defined(UNM_HWBUG_8_WORKAROUND))
static void nx_hwbug_8_workaround(struct pci_dev *pdev)
{
#define PCI_MSIX_FLAGS_ENABLE           (1 << 15)
	int		pos;
	__uint32_t	control;

	pos = pci_find_capability(pdev, PCI_CAP_ID_MSIX);
	if (pos) {
		pci_read_config_dword(pdev, pos, &control);
		control |= PCI_MSIX_FLAGS_ENABLE;
		pci_write_config_dword(pdev, pos, control);
	} else {
		/* XXX : What to do in this case ?? */
		nx_nic_print3(NULL, "Cannot set MSIX cap in "
			      "pci config space\n");
	}
}
#endif	/* (defined(NX_CONFIG_MSI_X) && defined(UNM_HWBUG_8_WORKAROUND)) */

static int nx_enable_msi_x(struct unm_adapter_s *adapter)
{
#ifndef	NX_CONFIG_MSI_X
	return (-1);
#else	/* NX_CONFIG_MSI_X */

	/* This fix is for some system showing msi-x on always. */
	nx_reset_msix_bit(adapter->pdev);
	if (adapter->msix_supported == 0) {
		return (-1);
	}

	if (!use_msi_x) {
		adapter->msix_supported = 0;
		return (-1);
	}

	if (NX_IS_REVISION_P2(adapter->ahw.revision_id) &&
	    NX_FW_VERSION(adapter->flash_ver.major,
			  adapter->flash_ver.minor,
			  adapter->flash_ver.sub) <
	    NX_FW_VERSION(NX_MSIX_SUPPORT_MAJOR,
			  NX_MSIX_SUPPORT_MINOR,
			  NX_MSIX_SUPPORT_SUBVERSION)) {

		nx_nic_print4(NULL, "Flashed firmware[%u.%u.%u] does not "
			      "support MSI-X, minimum firmware required is "
			      "[%u.%u.%u]\n",
			      adapter->flash_ver.major,
			      adapter->flash_ver.minor,
			      adapter->flash_ver.sub, NX_MSIX_SUPPORT_MAJOR,
			      NX_MSIX_SUPPORT_MINOR,
			      NX_MSIX_SUPPORT_SUBVERSION);
		adapter->msix_supported = 0;
		return (-1);
	}

	/* For now we can only run msi-x on functions with 128 MB of
	 * memory. */
        if ((pci_resource_len(adapter->pdev, 0) != UNM_PCI_128MB_SIZE) &&
            (pci_resource_len(adapter->pdev, 0) != UNM_PCI_2MB_SIZE)) {
		adapter->msix_supported = 0;
		return (-1);
	}

	init_msix_entries(adapter);
	/* XXX : This fix is for super-micro slot perpetually
	   showing msi-x on !! */
	nx_reset_msix_bit(adapter->pdev);

	if (pci_enable_msix(adapter->pdev, adapter->msix_entries,
			    adapter->num_msix)) {
		return (-1);
	}

	adapter->flags |= UNM_NIC_MSIX_ENABLED;

#ifdef	UNM_HWBUG_8_WORKAROUND
	nx_hwbug_8_workaround(adapter->pdev);
#endif
	adapter->netdev->irq = adapter->msix_entries[0].vector;
	return (0);
#endif	/* NX_CONFIG_MSI_X */
}

static int nx_enable_msi(struct unm_adapter_s *adapter)
{
	if (!use_msi) {
		return (-1);
	}
#if defined(CONFIG_PCI_MSI)
	if (!pci_enable_msi(adapter->pdev)) {
		adapter->flags |= UNM_NIC_MSI_ENABLED;
		return 0;
	}
	nx_nic_print3(NULL, "Unable to allocate MSI interrupt error\n");
#endif

	return (-1);
}

static int nx_read_flashed_versions(struct unm_adapter_s *adapter)
{
	int	rv;

	rv = nx_flash_read_version(adapter, FW_BIOS_VERSION_OFFSET,
				   &adapter->bios_ver);
	if (rv) {
		nx_nic_print4(NULL, "Flash bios version read FAILED: "
			      "offset[0x%x].\n", FW_BIOS_VERSION_OFFSET);
		return (-EIO);
	}

	rv = nx_flash_read_version(adapter, FW_VERSION_OFFSET,
				   &adapter->flash_ver);
	if (rv) {
		nx_nic_print4(NULL, "Flash FW version read FAILED: "
			      "offset[0x%x].\n", FW_VERSION_OFFSET);
		return (-EIO);
	}

	return (0);
}

static int nx_start_firmware(struct unm_adapter_s *adapter)
{
	int		tmp;
	int		pcie_cap;
	__uint16_t	lnk;
	/*
	 * In P3 the functions ID is directly mapped to the variable portnum.
	 * In P2 the function ID to portnum mapping depends on the board type.
	 * In either case the adapter with portnum == 0 brings the FW up.
	 */
	if (adapter->portnum != 0) {
		goto wait_for_rx_peg_sync;
	}

	/* scrub dma mask expansion register */
	NXWR32(adapter, CRB_DMA_SIZE, 0x55555555);

	if (adapter->ahw.boardcfg.board_type == UNM_BRDTYPE_P3_HMEZ ||
	    adapter->ahw.boardcfg.board_type == UNM_BRDTYPE_P3_XG_LOM) {
		NXWR32(adapter, UNM_PORT_MODE_ADDR, port_mode);

		NXWR32(adapter, UNM_WOL_PORT_MODE, wol_port_mode);
	}

	if (NX_IS_REVISION_P2(adapter->ahw.revision_id)) {
		if (nx_p2_check_hw_init(adapter) != 0) {
			return (-ENODEV);
		}
	}

	/* Overwrite stale initialization register values */
	NXWR32(adapter, CRB_CMDPEG_STATE, 0);
	NXWR32(adapter, CRB_RCVPEG_STATE, 0);

	load_fw(adapter, fw_load);

	if (NX_IS_REVISION_P3(adapter->ahw.revision_id)) {
		nx_p3_do_chicken_settings(adapter->pdev, adapter);
	}

	/*
	 * do this before waking up pegs so that we have valid dummy dma addr
	 */
	initialize_dummy_dma(adapter);

	/*
	 * Tell the hardware our version number.
	 */
	tmp = ((_UNM_NIC_LINUX_MAJOR << 16) |
	       ((_UNM_NIC_LINUX_MINOR << 8)) |
	       (_UNM_NIC_LINUX_SUBVERSION));
	NXWR32(adapter, CRB_DRIVER_VERSION, tmp);

	nx_nic_print7(NULL, "Bypassing PEGTUNE STUFF\n");
	//      UNM_NIC_PCI_WRITE_32(1, CRB_NORMALIZE(adapter,
	//                              UNM_ROMUSB_GLB_PEGTUNE_DONE));

	/* Handshake with the card before we register the devices. */
	nx_phantom_init(adapter, 0);

	nx_nic_print7(NULL, "State: 0x%0x\n",
		      NXRD32(adapter, CRB_CMDPEG_STATE));

  wait_for_rx_peg_sync:
	/* Negotiated Link width */
	pcie_cap = pci_find_capability(adapter->pdev, PCI_CAP_ID_EXP);
	pci_read_config_word(adapter->pdev, pcie_cap + PCI_EXP_LNKSTA, &lnk);
	adapter->link_width = (lnk >> 4) & 0x3f;

	/* Synchronize with Receive peg */
	return (receive_peg_ready(adapter));
}

static int __devinit get_adapter_attr(struct unm_adapter_s *adapter)
{
	struct nx_cardrsp_func_attrib	attr;

	if (NX_IS_REVISION_P2(adapter->ahw.revision_id))
		return 0;

	if (NX_FW_VERSION(adapter->version.major, adapter->version.minor,
	    adapter->version.sub) <= NX_FW_VERSION(4, 0, 401))
		return 0;

	if (nx_fw_cmd_func_attrib(adapter->nx_dev, adapter->portnum, &attr) !=
	    NX_RCODE_SUCCESS)
		return -1;

	adapter->interface_id = (int)attr.pvport[adapter->portnum];

#ifdef ESX
	if (adapter->ahw.fw_capabilities_1 & NX_FW_CAPABILITY_SWITCHING) {
		U64	freq = attr.freq;
		int	i, nfn = 0, mynumber = 0;

		for (i = 0; i < MAX_FUNC_ATTR; i++, freq >>= 1) {
			if (freq & 1) {
				nfn++;
				if (i < adapter->portnum)
					mynumber++;
			}
		}
		adapter->fmax = NUM_EPG_FILTERS / nfn;
		adapter->fstart = mynumber * adapter->fmax;
	}
#endif
	return 0;
}

#ifdef NX_VLAN_ACCEL
static void nx_vlan_rx_register(struct net_device *netdev,
				struct vlan_group *grp)
{
	struct unm_adapter_s *adapter = netdev_priv(netdev);

	adapter->vlgrp = grp;
}

static void nx_rx_kill_vid(struct net_device *netdev, unsigned short vid)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,21)
	struct unm_adapter_s *adapter = netdev_priv(netdev);

	if (adapter->vlgrp)
		adapter->vlgrp->vlan_devices[vid] = NULL;
#endif /* < 2.6.21 */
}
#endif /* NX_VLAN_ACCEL */

static void vlan_accel_setup(struct unm_adapter_s *adapter,
    struct net_device *netdev)
{
#ifdef NX_VLAN_ACCEL
	if (adapter->ahw.fw_capabilities_1 & NX_FW_CAPABILITY_FVLANTX) {
		netdev->features |= NETIF_F_HW_VLAN_RX;
		netdev->vlan_rx_register = nx_vlan_rx_register;
		netdev->vlan_rx_kill_vid = nx_rx_kill_vid;
		netdev->features |= NETIF_F_HW_VLAN_TX;
	}
#endif /* NX_VLAN_ACCEL */
}

static inline void check_8021q(struct net_device *netdev,
		struct sk_buff *skb, int *vidp)
{
#ifdef NX_VLAN_ACCEL
	struct ethhdr	*eth = (struct ethhdr *)(skb->data);
	u32		*src, *dst;

	if (!(netdev->features & NETIF_F_HW_VLAN_RX))
		return;

	if (eth->h_proto != htons(ETH_P_8021Q))
		return;

	*vidp = ntohs(*(u16 *)(eth + 1));

	/*
	 * Quick implementation of
	 * memmove(skb->data + VLAN_HLEN, skb->data, 12);
	 * to eliminate 8021q tag from data stream.
	 */
	dst = (u32 *)(skb->data + 14);
	src = (u32 *)(skb->data + 10);
	*((u16 *)dst) = *((u16 *)src);
	*(--dst) = *(--src);
	*(--dst) = *(--src);
	*(--dst) = *(--src);
	skb_pull(skb, VLAN_HLEN);
	SKB_ADJUST_PKT_MA(skb, VLAN_HLEN);
#endif /* NX_VLAN_ACCEL */
}

static inline int nx_rx_packet(struct unm_adapter_s *adapter,
    struct sk_buff *skb, int vid)
{
	int	ret;

#ifdef NX_VLAN_ACCEL
	if ((vid != -1) && adapter->vlgrp)
		ret = vlan_hwaccel_receive_skb(skb, adapter->vlgrp, (u16)vid);
	else
#endif /* NX_VLAN_ACCEL */
		ret = __NETIF_RX(skb);
	return(ret);
}

/*
 * Linux system will invoke this after identifying the vendor ID and device Id
 * in the pci_tbl where this module will search for UNM vendor and device ID
 * for quad port adapter.
 */
static int __devinit unm_nic_probe(struct pci_dev *pdev,
				   const struct pci_device_id *ent)
{
	struct net_device *netdev = NULL;
	struct unm_adapter_s *adapter = NULL;
	uint8_t *mem_ptr0 = NULL;
	uint8_t *mem_ptr1 = NULL;
	uint8_t *mem_ptr2 = NULL;
	uint8_t *db_ptr = NULL;
	unsigned long mem_base, mem_len, db_base, db_len, pci_len0;
	unsigned long first_page_group_start, first_page_group_end;
	int i = 0, err;
	int temp;
	int pci_func_id = PCI_FUNC(pdev->devfn);
	uint8_t revision_id;
	nx_host_nic_t *nx_dev = NULL;
	struct nx_legacy_intr_set *legacy_intrp;
        if (pdev->class != 0x020000) {
                nx_nic_print3(NULL, "function %d, class 0x%x will not be "
			      "enabled.\n", pci_func_id, pdev->class);
                return -ENODEV;
        }

        if ((err = pci_enable_device(pdev))) {
                nx_nic_print3(NULL, "Cannot enable PCI device. Error[%d]\n",
			      err);
                return err;
        }

        if (!(pci_resource_flags(pdev, 0) & IORESOURCE_MEM)) {
                nx_nic_print3(NULL, "Cannot find proper PCI device "
			      "base address, aborting. %p\n", pdev);
                err = -ENODEV;
                goto err_out_disable_pdev;
        }

        if ((err = unm_pci_request_regions(pdev, unm_nic_driver_name))) {
                nx_nic_print3(NULL, "Cannot find proper PCI resources. "
			      "Error[%d]\n", err);
                goto err_out_disable_pdev;
        }

	pci_set_master(pdev);

	pci_read_config_byte(pdev, PCI_REVISION_ID, &revision_id);

        nx_nic_print6(NULL, "Probe: revision ID = 0x%x\n", revision_id);

	netdev = alloc_etherdev(sizeof(struct unm_adapter_s));
	if (!netdev) {
		nx_nic_print3(NULL, "Failed to allocate memory for the "
			      "device block. Check system memory resource "
			      "usage.\n");
		err = -ENOMEM;
		unm_pci_release_regions(pdev);
	      err_out_disable_pdev:
		pci_disable_device(pdev);
		pci_set_drvdata(pdev, NULL);
		return err;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	SET_MODULE_OWNER(netdev);
#endif
	SET_NETDEV_DEV(netdev, &pdev->dev);

	adapter = netdev_priv(netdev);
	memset(adapter, 0, sizeof(struct unm_adapter_s));
	adapter->ahw.pdev = pdev;
	adapter->ahw.pci_func = pci_func_id;
	adapter->ahw.revision_id = revision_id;
	rwlock_init(&adapter->adapter_lock);
	spin_lock_init(&adapter->tx_lock);
	spin_lock_init(&adapter->lock);
	spin_lock_init(&adapter->buf_post_lock);
	spin_lock_init(&adapter->cb_lock);
#if defined(NEW_NAPI)
	spin_lock_init(&adapter->tx_cpu_excl);
#endif
	spin_lock_init(&adapter->ctx_state_lock);
	/* Invalidate the User defined speed/duplex/autoneg values*/
	adapter->cfg_speed = NX_INVALID_SPEED_CFG;
	adapter->cfg_duplex = NX_INVALID_DUPLEX_CFG;
	adapter->cfg_autoneg = NX_INVALID_AUTONEG_CFG;

	adapter->ahw.qdr_sn_window = -1;
	adapter->ahw.ddr_mn_window = -1;
	adapter->interface_id = -1;
	adapter->netdev = netdev;
	adapter->pdev = pdev;
	adapter->msglvl = NX_NIC_NOTICE;
#ifdef ESX
	adapter->multi_ctx = multi_ctx;
#endif

	INIT_LIST_HEAD(&adapter->wait_list);
	for (i = 0; i < NX_WAIT_BIT_MAP_SIZE; i++) {
		adapter->wait_bit_map[i] = 0;
	}

	nx_init_status_handler(adapter);

	if ((err = nx_set_dma_mask(adapter, revision_id))) {
		goto err_ret;
	}

	/* remap phys address */
	mem_base = pci_resource_start(pdev, 0);	/* 0 is for BAR 0 */
	mem_len = pci_resource_len(pdev, 0);

	/* 128 Meg of memory */
	nx_nic_print6(NULL, "ioremap from %lx a size of %lx\n", mem_base,
		      mem_len);

        if (NX_IS_REVISION_P2(revision_id) &&
	    (mem_len == UNM_PCI_128MB_SIZE || mem_len == UNM_PCI_32MB_SIZE)) {

		adapter->unm_nic_hw_write_wx = &unm_nic_hw_write_wx_128M;
		adapter->unm_nic_hw_write_ioctl =
			&unm_nic_hw_write_ioctl_128M;
		adapter->unm_nic_hw_read_wx =
			&unm_nic_hw_read_wx_128M;
		adapter->unm_nic_hw_read_ioctl =
			&unm_nic_hw_read_ioctl_128M;
		adapter->unm_nic_pci_set_window =
			&unm_nic_pci_set_window_128M;
		adapter->unm_nic_pci_mem_read =
			&unm_nic_pci_mem_read_128M;
		adapter->unm_nic_pci_mem_write =
			&unm_nic_pci_mem_write_128M;
		adapter->unm_nic_pci_write_immediate =
			&unm_nic_pci_write_immediate_128M;
		adapter->unm_nic_pci_read_immediate =
			&unm_nic_pci_read_immediate_128M;

		if (mem_len == UNM_PCI_128MB_SIZE) {
			mem_ptr0 = ioremap(mem_base, FIRST_PAGE_GROUP_SIZE);
			if (!mem_ptr0) {
				err = -EIO;
				goto err_ret;
			}
			pci_len0 = FIRST_PAGE_GROUP_SIZE;
			mem_ptr1 = ioremap(mem_base + SECOND_PAGE_GROUP_START,
					   SECOND_PAGE_GROUP_SIZE);
			if (!mem_ptr1) {
				err = -EIO;
				goto err_ret;
			}
			mem_ptr2 = ioremap(mem_base + THIRD_PAGE_GROUP_START,
					   THIRD_PAGE_GROUP_SIZE);
			if (!mem_ptr2) {
				err = -EIO;
				goto err_ret;
			}
			first_page_group_start = FIRST_PAGE_GROUP_START;
			first_page_group_end   = FIRST_PAGE_GROUP_END;
		} else {
			pci_len0 = 0;
			mem_ptr1 = ioremap(mem_base, SECOND_PAGE_GROUP_SIZE);
			if (!mem_ptr1) {
				err = -EIO;
				goto err_ret;
			}
			mem_ptr2 = ioremap(mem_base + THIRD_PAGE_GROUP_START -
					   SECOND_PAGE_GROUP_START,
					   THIRD_PAGE_GROUP_SIZE);
			if (!mem_ptr2) {
				err = -EIO;
				goto err_ret;
			}
			first_page_group_start = 0;
			first_page_group_end   = 0;
		}

        } else if (NX_IS_REVISION_P3(revision_id) &&
		   mem_len == UNM_PCI_2MB_SIZE) {

		adapter->unm_nic_hw_read_wx = &unm_nic_hw_read_wx_2M;
		adapter->unm_nic_hw_write_wx = &unm_nic_hw_write_wx_2M;
		adapter->unm_nic_hw_read_ioctl = &unm_nic_hw_read_ioctl_2M;
		adapter->unm_nic_hw_write_ioctl = &unm_nic_hw_write_ioctl_2M;
		adapter->unm_nic_pci_set_window =
			&unm_nic_pci_set_window_2M;
		adapter->unm_nic_pci_mem_read = &unm_nic_pci_mem_read_2M;
		adapter->unm_nic_pci_mem_write = &unm_nic_pci_mem_write_2M;
		adapter->unm_nic_pci_write_immediate =
			&unm_nic_pci_write_immediate_2M;
		adapter->unm_nic_pci_read_immediate =
			&unm_nic_pci_read_immediate_2M;

		mem_ptr0 = ioremap(mem_base, mem_len);
		if (!mem_ptr0) {
			err = -EIO;
			goto err_ret;
		}
		pci_len0 = mem_len;
		first_page_group_start = 0;
		first_page_group_end   = 0;

		adapter->ahw.ddr_mn_window = 0;
		adapter->ahw.qdr_sn_window = 0;

		adapter->ahw.mn_win_crb = (0x100000 +
					   PCIE_MN_WINDOW_REG(pci_func_id));

		adapter->ahw.ms_win_crb = (0x100000 +
					   PCIE_SN_WINDOW_REG(pci_func_id));

        } else {
		nx_nic_print3(NULL, "Invalid PCI memmap[0x%lx] for chip "
			      "revision[%u]\n", mem_len, revision_id);
		err = -EIO;
		goto err_ret;
        }
#ifdef ESX
	if(NX_IS_REVISION_P2(revision_id))
		adapter->multi_ctx = 0;
#endif
	nx_nic_print6(NULL, "ioremapped at 0 -> %p, 1 -> %p, 2 -> %p\n",
		      mem_ptr0, mem_ptr1, mem_ptr2);

	db_base = pci_resource_start(pdev, 4);	/* doorbell is on bar 4 */
	db_len = pci_resource_len(pdev, 4);
	if (db_len) {
		nx_nic_print6(NULL, "doorbell ioremap from %lx a size of %lx\n",
		      db_base, db_len);
	}

       /* Check used for Quad port cards where Bar4 is no more used */
       if (db_base) {

               db_ptr = ioremap(db_base, UNM_DB_MAPSIZE_BYTES);
               if (!db_ptr) {
                       err = -EIO;
                       goto err_ret;
               }
	}

	adapter->ahw.pci_base0 = (unsigned long)mem_ptr0;
	adapter->ahw.pci_len0 = pci_len0;
	adapter->ahw.first_page_group_start = first_page_group_start;
	adapter->ahw.first_page_group_end = first_page_group_end;
	adapter->ahw.pci_base1 = (unsigned long)mem_ptr1;
	adapter->ahw.pci_len1 = SECOND_PAGE_GROUP_SIZE;
	adapter->ahw.pci_base2 = (unsigned long)mem_ptr2;
	adapter->ahw.pci_len2 = THIRD_PAGE_GROUP_SIZE;
	adapter->ahw.crb_base =
	    PCI_OFFSET_SECOND_RANGE(adapter, UNM_PCI_CRBSPACE);
	adapter->ahw.db_base = (unsigned long)db_ptr;
	adapter->ahw.db_len = db_len;

	if (revision_id >= NX_P3_B0) {
		legacy_intrp = &legacy_intr[pci_func_id];
	} else {
		legacy_intrp = &legacy_intr[0];
	}

	adapter->legacy_intr.int_vec_bit = legacy_intrp->int_vec_bit;
	adapter->legacy_intr.tgt_status_reg = legacy_intrp->tgt_status_reg;
	adapter->legacy_intr.tgt_mask_reg = legacy_intrp->tgt_mask_reg;
	adapter->legacy_intr.pci_int_reg = legacy_intrp->pci_int_reg;
	/*
	 * Set the CRB window to invalid. If any register in window 0 is
	 * accessed it should set the window to 0 and then reset it to 1.
	 */
	adapter->curr_window = 255;


	if ((mem_len != UNM_PCI_2MB_SIZE) &&
	    (((mem_ptr0 == 0UL) && (mem_len == UNM_PCI_128MB_SIZE)) ||
	    (mem_ptr1 == 0UL) || (mem_ptr2 == 0UL))) {
		nx_nic_print3(NULL, "Cannot remap adapter memory aborting.:"
			      "0 -> %p, 1 -> %p, 2 -> %p\n",
			      mem_ptr0, mem_ptr1, mem_ptr2);
		err = -EIO;
		goto err_ret;
	}
	if (db_len == 0) {
		if(NX_IS_REVISION_P2(revision_id)) {
			nx_nic_print3(NULL, "doorbell is disabled\n");
			err = -EIO;
			goto err_ret;
		}
	}
	if (db_len && db_ptr == 0UL) {
		nx_nic_print3(NULL, "Failed to allocate doorbell map.\n");
		err = -EIO;
		goto err_ret;
	}
	if (db_len) {
		nx_nic_print6(NULL, "doorbell ioremapped at %p\n", db_ptr);
	}

	/* This will be reset for mezz cards  */
	adapter->portnum = pci_func_id;

	if (NX_IS_REVISION_P3(revision_id)) {
		adapter->max_mc_count = UNM_MC_COUNT;
	} else {
		adapter->max_mc_count = (adapter->portnum > 1) ? 4 : 16;
	}

#if defined(UNM_NIC_HW_CSUM)
	adapter->rx_csum = 1;
#endif

	netdev->open = unm_nic_open;
	netdev->stop = unm_nic_close;
	netdev->hard_start_xmit = unm_nic_xmit_frame;
	netdev->get_stats = unm_nic_get_stats;
	if (NX_IS_REVISION_P2(revision_id)) {
		netdev->set_multicast_list = nx_nic_p2_set_multi;
	} else {
		netdev->set_multicast_list = nx_nic_p3_set_multi;
	}
	netdev->set_mac_address = unm_nic_set_mac;
	netdev->do_ioctl = unm_nic_ioctl;
	netdev->tx_timeout = unm_tx_timeout;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,21)
	set_ethtool_ops(netdev);
	/* FIXME: maybe SET_ETHTOOL_OPS(netdev,&unm_nic_ethtool_ops); */
#endif
#if (defined(UNM_NIC_NAPI) && !defined(NEW_NAPI))
	netdev->poll = unm_nic_poll;
	netdev->weight = UNM_NETDEV_WEIGHT;
#endif
#if !defined(__VMKLNX__)
#ifdef CONFIG_NET_POLL_CONTROLLER
	netdev->poll_controller = unm_nic_poll_controller;
#endif
#endif /* !defined (__VMKLNX__) */
#ifdef UNM_NIC_HW_CSUM
	/* ScatterGather support */
	netdev->features = NETIF_F_SG;
	netdev->features |= NETIF_F_IP_CSUM;
#endif

#ifdef NETIF_F_TSO
	if(tso) {
		netdev->features |= NETIF_F_TSO;
#ifdef  NETIF_F_TSO6
		if (NX_IS_REVISION_P3(revision_id)) {
			netdev->features |= NETIF_F_TSO6;
		}
#endif
	}
#endif

	if (NX_IS_REVISION_P3(revision_id)) {
		netdev->features |= NETIF_F_HW_CSUM;
	}
#ifdef __VMKLNX__
#ifdef NETIF_F_RDONLYINETHDRS
        netdev->features |= NETIF_F_RDONLYINETHDRS;
#endif // NETIF_F_RDONLYINETHDRS
#endif // __VMKLNX__

	adapter->ahw.vendor_id = pdev->vendor;
	adapter->ahw.device_id = pdev->device;
	adapter->led_blink_rate = 1;

	/* Initialize default coalescing parameters */
	adapter->coal.normal.data.rx_packets =
	    NX_DEFAULT_INTR_COALESCE_RX_PACKETS;
	adapter->coal.normal.data.rx_time_us =
	    NX_DEFAULT_INTR_COALESCE_RX_TIME_US;
	adapter->coal.normal.data.tx_time_us =
	    NX_DEFAULT_INTR_COALESCE_TX_TIME_US;
	adapter->coal.normal.data.tx_packets =
	    NX_DEFAULT_INTR_COALESCE_TX_PACKETS;

	/*
	 * Initialize the pegnet_cmd_desc overflow data structure.
	 */
	unm_init_pending_cmd_desc(&adapter->pending_cmds);

	pci_read_config_word(pdev, PCI_COMMAND, &adapter->ahw.pci_cmd_word);


	if (adapter->pci_using_dac) {
		netdev->features |= NETIF_F_HIGHDMA;
	}

	NX_SET_VLAN_DEV_FEATURES(netdev, (struct net_device*)NULL);

	/*
	 * Initialize the HW so that we can get the board type first. Based on
	 * the board type the ring size is chosen.
	 */
	if (initialize_adapter_hw(adapter) != 0) {
		err = -EIO;
		goto err_ret;
	}

	/* Do not enable netq for all types of P3 quad gig cards.*/
	if (adapter->ahw.board_type == UNM_NIC_GBE) {
		adapter->multi_ctx = 0;
	}

	/* fill the adapter id field with the board serial num */
	unm_nic_get_serial_num(adapter);

	/* Mezz cards have PCI function 0,2,3 enabled */
	if (adapter->ahw.boardcfg.board_type == UNM_BRDTYPE_P2_SB31_10G_IMEZ ||
	    adapter->ahw.boardcfg.board_type == UNM_BRDTYPE_P2_SB31_10G_HMEZ) {
		if (pci_func_id >= 2) {
			adapter->portnum = pci_func_id - 2;
		}
	}

	NX_INIT_WORK(&adapter->tx_timeout_task, unm_tx_timeout_task, netdev);

	/* Initialize watchdog  */
	init_timer(&adapter->watchdog_timer);
	adapter->watchdog_timer.data = (unsigned long)adapter;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0) || defined(ESX_3X) || defined (ESX_3X_COS))
	adapter->watchdog_timer.function = &unm_watchdog;
	NX_INIT_WORK(&adapter->watchdog_task, unm_watchdog_task, adapter);
#else
	adapter->watchdog_timer.function = &unm_watchdog_task;
#endif

	/* Initialize Firmware watchdog  */	
	if(adapter->portnum == 0) {
		init_timer(&adapter->watchdog_timer_fw_reset);
		adapter->watchdog_timer_fw_reset.data = (unsigned long)adapter;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0) || defined(ESX_3X) || defined(ESX_3X_COS))
		adapter->watchdog_timer_fw_reset.function = &unm_watchdog_fw_reset;
		NX_INIT_WORK(&adapter->watchdog_task_fw_reset, unm_watchdog_task_fw_reset, adapter);
#else
		adapter->watchdog_timer_fw_reset.function = &unm_watchdog_task_fw_reset;
#endif
	}

	err = nx_read_flashed_versions(adapter);
	if (err) {
		goto err_ret;
	}

	unm_check_options(adapter);
	err = nx_start_firmware(adapter);
	if (err) {
		goto err_ret;
	}

    api_lock(adapter);
    NXWR32(adapter,UNM_FW_RESET, 0);
    api_unlock(adapter);

#if (defined(ESX))

#if (!defined(ESX_3X_COS))
	nx_set_num_rx_queues(adapter);
#else // !defined(ESX_3X_COS)
	adapter->num_rx_queues = 0;
#endif // !defined(ESX_3X_COS)

#else //defined(ESX)
	adapter->num_rx_queues = 0;
#endif //defined(ESX)

	nx_update_dma_mask(adapter, pdev);
	nx_nic_get_nic_version_number(adapter->netdev, &adapter->version);
	if (NX_FW_VERSION(adapter->version.major, adapter->version.minor,
			  adapter->version.sub) < NX_FW_VERSION(4, 0, 0)) {
		adapter->fw_v34 = 1;
		adapter->max_rds_rings = 3;
		adapter->msg_type_sync_offload =
					UNM_MSGTYPE_NIC_SYN_OFFLOAD_V34;
		adapter->msg_type_rx_desc = UNM_MSGTYPE_NIC_RXPKT_DESC_V34;
		netdev->change_mtu = nx_nic_fw34_change_mtu;
	} else {
		adapter->fw_v34 = 0;
		adapter->max_rds_rings = 2;
		adapter->msg_type_sync_offload = UNM_MSGTYPE_NIC_SYN_OFFLOAD;
		adapter->msg_type_rx_desc = UNM_MSGTYPE_NIC_RXPKT_DESC;
		netdev->change_mtu = nx_nic_fw40_change_mtu;
	}

	if (NX_FW_VERSION(adapter->version.major, adapter->version.minor,
			  adapter->version.sub) <= NX_FW_VERSION(4, 0, 230)) {
		adapter->MaxRxDescCount =
			 RDS_LIMIT_FW230(adapter->MaxRxDescCount);
		adapter->MaxJumboRxDescCount =
			 RDS_JUMBO_LIMIT_FW230(adapter->MaxJumboRxDescCount);
	}


	if (NX_IS_REVISION_P3(revision_id)) {
		temp = NXRD32(adapter, UNM_MIU_MN_CONTROL);
		adapter->ahw.cut_through = NX_IS_SYSTEM_CUT_THROUGH(temp);
		nx_nic_print5(NULL, "Running in %s mode\n",
			      adapter->ahw.cut_through ? "'Cut Through'" :
			      "'Legacy'");
	}


	if (nx_enable_msi_x(adapter)) {
		nx_enable_msi(adapter);
		/*
		 * Both Legacy & MSI case set the irq of the net device
		 */
		adapter->max_possible_rss_rings = 1;
		adapter->multi_ctx = 0;
		netdev->irq = pdev->irq;
	}

#ifdef ESX
	if (adapter->multi_ctx) {
		err = nx_os_dev_alloc(&nx_dev, adapter, adapter->portnum,
				adapter->num_rx_queues + 1, 1);
	} else
#endif
	{
		err = nx_os_dev_alloc(&nx_dev, adapter, adapter->portnum,
				MAX_RX_CTX, MAX_TX_CTX);
	}
	if (err) {
		nx_nic_print6(NULL, "Memory cannot be allocated");
		goto err_ret;
	}
	adapter->nx_dev = nx_dev;

	err = nx_alloc_adapter_sds_rings(adapter);
	if (err) {
		goto err_ret;
	}
	nx_init_napi(adapter);
	
#ifdef ESX
	if (adapter->multi_ctx) {
		adapter->nx_dev->saved_rules = kmalloc((sizeof(nx_rx_rule_t)*(adapter->num_rx_queues + 1))
				,GFP_KERNEL);
		if(adapter->nx_dev->saved_rules == NULL) {
			err = -ENOMEM;
			goto err_ret;
		}
		NX_SET_NETQ_OPS(netdev, nx_nic_netqueue_ops);
	}
#endif

	if (!unm_nic_fill_adapter_macaddr_from_flash(adapter)) {
		if (unm_nic_macaddr_set(adapter, adapter->mac_addr) != 0) {
			err = -EIO;
			goto err_ret;
		}
		memcpy(netdev->dev_addr, adapter->mac_addr, netdev->addr_len);
	}

	/* name the proc entries different than ethx, since that can change */
	sprintf(adapter->procname, "dev%d", adapter_count++);


	/* initialize lro hash table */
	if (!adapter->fw_v34) {
		if (NX_FW_VERSION(adapter->version.major,
				  adapter->version.minor,
				  adapter->version.sub) >=
		    NX_FW_VERSION(4, 0, 222)) {
			adapter->ahw.fw_capabilities_1 =
					NXRD32( adapter,
					UNM_FW_CAPABILITIES_1);
		} else {
			adapter->ahw.fw_capabilities_1 =
				(NX_FW_CAPABILITY_LRO | NX_FW_CAPABILITY_LSO |
				 NX_FW_CAPABILITY_TOE);
		}

		if (adapter->ahw.fw_capabilities_1 & NX_FW_CAPABILITY_LRO) {
			nx_nic_print6(adapter, "LRO capable device found\n");
			err = unm_init_lro(adapter);
			if (err != 0) {
				nx_nic_print3(NULL,
					      "LRO Initialization failed\n");
				goto err_ret;
			}
		}
                if(!lro) {
                        adapter->lro.enabled = 0;
                }
	}

	unm_init_proc_entries(adapter);
	pci_set_drvdata(pdev, netdev);

	switch (adapter->ahw.board_type) {
        case UNM_NIC_GBE:
                nx_nic_print5(NULL, "1 GbE port initialized\n");
                break;

        case UNM_NIC_XGBE:
                nx_nic_print5(NULL, "10 GbE port initialized\n");
                break;
        }

	netif_carrier_off(netdev);
	netif_stop_queue(netdev);

	if (get_adapter_attr(adapter)) {
                nx_nic_print5(NULL, "Could not get adapter attributes\n");
#ifdef ESX
		if (adapter->ahw.fw_capabilities_1 &
		    NX_FW_CAPABILITY_SWITCHING) {
			nx_nic_print3(NULL, "Can not proceed for card\n");
			goto err_ret;
		}
#endif
	}

        if (hw_vlan) {
		vlan_accel_setup(adapter, netdev);
        }

	if ((err = register_netdev(netdev))) {
		nx_nic_print3(NULL, "register_netdev failed\n");
		goto err_ret;
	}
	if(adapter->portnum == 0) {	
		adapter->auto_fw_reset = 1;
		NXWR32(adapter,UNM_FW_RESET, 0);
		adapter->fwload_failed_jiffies = jiffies;
		adapter->fwload_failed_count = 0 ;
		mod_timer(&adapter->watchdog_timer_fw_reset, jiffies + 10 * HZ);
	}

	return 0;

      err_ret:

	if (adapter->portnum == 0) 
		destroy_dummy_dma(adapter);

	nx_release_fw(&adapter->nx_fw);
	cleanup_adapter(adapter);
	free_netdev(netdev);
	return err;
}

static void initialize_adapter_sw(struct unm_adapter_s *adapter,
				  nx_host_rx_ctx_t *nxhal_host_rx_ctx)
{
	int ring;
	uint32_t i;
	uint32_t num_rx_bufs = 0;
	rds_host_ring_t *host_rds_ring = NULL;
	nx_host_rds_ring_t *nxhal_host_rds_ring = NULL;

        nx_nic_print7(NULL, "initializing some queues: %p\n", adapter);

	for (ring = 0; ring < adapter->max_rds_rings; ring++) {
		struct unm_rx_buffer *rxBuf;
		nxhal_host_rds_ring = &nxhal_host_rx_ctx->rds_rings[ring];
		host_rds_ring =
		    (rds_host_ring_t *) nxhal_host_rds_ring->os_data;

		host_rds_ring->producer = 0;
		/* Initialize the queues for both receive and
		   command buffers */
		rxBuf = host_rds_ring->rx_buf_arr;
		num_rx_bufs = nxhal_host_rds_ring->ring_size;
		host_rds_ring->begin_alloc = 0;
		/* Initialize the free queues */
		INIT_LIST_HEAD(&host_rds_ring->free_rxbufs.head);
		atomic_set(&host_rds_ring->alloc_failures, 0);
		/*
		 * Now go through all of them, set reference handles
		 * and put them in the queues.
		 */
		for (i = 0; i < num_rx_bufs; i++) {
			rxBuf->refHandle = i;
			rxBuf->state = UNM_BUFFER_FREE;
			rxBuf->skb = NULL;
			list_add_tail(&rxBuf->link, 
					&host_rds_ring->free_rxbufs.head);
			host_rds_ring->free_rxbufs.count++;
			nx_nic_print7(adapter, "Rx buf: i(%d) rxBuf: %p\n",
				      i, rxBuf);
			rxBuf++;
		}
	}

        nx_nic_print7(NULL, "initialized buffers for %s and %s\n",
		      "adapter->free_cmd_buf_list", "adapter->free_rxbuf");
        return;
}

static int initialize_adapter_hw(struct unm_adapter_s *adapter)
{
	uint32_t value = 0;
	unm_board_info_t *board_info = &(adapter->ahw.boardcfg);
	int ports = 0;

	if (unm_nic_get_board_info(adapter) != 0) {
            nx_nic_print3(NULL, "Error getting board config info.\n");
		return -1;
	}

	GET_BRD_PORTS_BY_TYPE(board_info->board_type, ports);
	if (ports == 0) {
                nx_nic_print3(NULL, "Unknown board type[0x%x]\n",
			      board_info->board_type);
	}
	adapter->ahw.max_ports = ports;

	return value;
}

static int init_firmware(struct unm_adapter_s *adapter)
{
	uint32_t state = 0, loops = 0, err = 0;
	uint32_t   tempout;

	/* Window 1 call */
	read_lock(&adapter->adapter_lock);
	state = NXRD32(adapter, CRB_CMDPEG_STATE);
	read_unlock(&adapter->adapter_lock);

	if (state == PHAN_INITIALIZE_ACK)
		return 0;

	while (state != PHAN_INITIALIZE_COMPLETE && loops < 200000) {
		udelay(100);
		schedule();
		/* Window 1 call */
		read_lock(&adapter->adapter_lock);
		state = NXRD32(adapter, CRB_CMDPEG_STATE);
		read_unlock(&adapter->adapter_lock);

		loops++;
	}
	if (loops >= 200000) {
                nx_nic_print3(adapter, "Cmd Peg initialization not "
			      "complete:%x.\n", state);
		err = -EIO;
		return err;
	}
	/* Window 1 call */
	read_lock(&adapter->adapter_lock);
        tempout = PHAN_INITIALIZE_ACK;
        NXWR32(adapter, CRB_CMDPEG_STATE, tempout);
	read_unlock(&adapter->adapter_lock);

	return err;
}

void nx_free_rx_resources(struct unm_adapter_s *adapter,
				 nx_host_rx_ctx_t *nxhal_rx_ctx)
{
	struct unm_rx_buffer *buffer;
	rds_host_ring_t *host_rds_ring = NULL;
	nx_host_rds_ring_t *nxhal_host_rds_ring = NULL;
	int i;
	int ring;
	struct pci_dev *pdev = adapter->pdev;

	for (ring = 0; ring < adapter->max_rds_rings; ring++) {
		nxhal_host_rds_ring = &nxhal_rx_ctx->rds_rings[ring];
		host_rds_ring =
		    (rds_host_ring_t *) nxhal_host_rds_ring->os_data;
		for (i = 0; i < nxhal_host_rds_ring->ring_size; ++i) {
			buffer = &(host_rds_ring->rx_buf_arr[i]);
			if (buffer->state == UNM_BUFFER_FREE) {
				continue;
			}
			pci_unmap_single(pdev, buffer->dma,
					 host_rds_ring->dma_size,
					 PCI_DMA_FROMDEVICE);
			if (buffer->skb != NULL) {
				dev_kfree_skb_any(buffer->skb);
			}
		}
	}
}

static void unm_nic_free_ring_context(struct unm_adapter_s *adapter)
{
	nx_host_rx_ctx_t *nxhal_rx_ctx = NULL;
	int index = 0;

	if (adapter->cmd_buf_arr != NULL) {
		vfree(adapter->cmd_buf_arr);
		adapter->cmd_buf_arr = NULL;
	}

	if (adapter->ahw.cmdDescHead != NULL) {
		pci_free_consistent(adapter->ahw.cmdDesc_pdev,
				    (sizeof(cmdDescType0_t) *
				     adapter->MaxTxDescCount)
				    + sizeof(uint32_t),
				    adapter->ahw.cmdDescHead,
				    adapter->ahw.cmdDesc_physAddr);
		adapter->ahw.cmdDescHead = NULL;
	}
	adapter->cmdProducer = 0;

        if (adapter->nx_lic_dma.addr != NULL) {
                pci_free_consistent(adapter->ahw.pdev,
                                    sizeof(nx_finger_print_t),
                                    adapter->nx_lic_dma.addr,
                                    adapter->nx_lic_dma.phys_addr);
                adapter->nx_lic_dma.addr = NULL;
        }
	adapter->cmdConsumer = NULL;
	adapter->crb_addr_cmd_producer = 0;
	adapter->crb_addr_cmd_consumer = 0;
	adapter->lastCmdConsumer = 0;

#ifdef UNM_NIC_SNMP
	if (adapter->snmp_stats_dma.addr != NULL) {
		pci_free_consistent(adapter->ahw.pdev,
				    sizeof(struct unm_nic_snmp_ether_stats),
				    adapter->snmp_stats_dma.addr,
				    adapter->snmp_stats_dma.phys_addr);
		adapter->snmp_stats_dma.addr = NULL;
	}
#endif

#ifdef EPG_WORKAROUND
	if (adapter->ahw.pauseAddr != NULL) {
		pci_free_consistent(adapter->ahw.pause_pdev, 512,
				    adapter->ahw.pauseAddr,
				    adapter->ahw.pause_physAddr);
		adapter->ahw.pauseAddr = NULL;
	}
#endif

#ifndef ESX
	if (adapter->nic_net_stats.data != NULL) {
		pci_free_consistent(adapter->pdev, sizeof (nx_ctx_stats_t),
				    (void *)adapter->nic_net_stats.data,
				    adapter->nic_net_stats.phys);	
		adapter->nic_net_stats.data = NULL;
	}
#endif

	for(index = 0; index < adapter->nx_dev->alloc_rx_ctxs ; index++) {
		if (adapter->nx_dev->rx_ctxs[index] != NULL) {
                       if(index != 0) {        
                               /* Only Synchronize for deletion of Netqueues. 
                                  Deletion of default queue is implicitly sychronized */
                               spin_lock(&adapter->ctx_state_lock);
                               if((adapter->nx_dev->rx_ctxs_state & (1 << index)) != 0) {
                                       adapter->nx_dev->rx_ctxs_state &= ~(1 << index);
                                       spin_unlock(&adapter->ctx_state_lock);
                               } else { 
                                       spin_unlock(&adapter->ctx_state_lock);
                                       continue;
                               }
                       } else {
                               adapter->nx_dev->rx_ctxs_state &= ~(1 << index);
                       }

			nxhal_rx_ctx = adapter->nx_dev->rx_ctxs[index];
			nx_free_sts_rings(adapter,nxhal_rx_ctx);
			nx_nic_free_host_sds_resources(adapter,nxhal_rx_ctx);
			nx_free_rx_resources(adapter,nxhal_rx_ctx);
			nx_nic_free_hw_rx_resources(adapter, nxhal_rx_ctx);
			nx_nic_free_host_rx_resources(adapter, nxhal_rx_ctx);
			nx_fw_cmd_create_rx_ctx_free(adapter->nx_dev,
					nxhal_rx_ctx);

		}
	}

	return;
}

static void unm_nic_free_ring_context_in_fw(struct unm_adapter_s *adapter, int ctx_destroy)
{
	if(!adapter->fw_v34) {
		unm_nic_new_rx_context_destroy(adapter, ctx_destroy);
		unm_nic_new_tx_context_destroy(adapter, ctx_destroy);
	} else {
		pci_free_consistent(adapter->pdev, adapter->ctx_add.size, adapter->ctx_add.ptr,
				adapter->ctx_add.phys);
		unm_nic_v34_free_ring_context_in_fw(adapter);
	}
}

void unm_nic_free_hw_resources(struct unm_adapter_s *adapter)
{
	if (adapter->is_up == ADAPTER_UP_MAGIC) {
		unm_nic_free_ring_context_in_fw(adapter, NX_DESTROY_CTX_RESET);
	}
	unm_nic_free_ring_context(adapter);
}

static void inline unm_nic_clear_stats(struct unm_adapter_s *adapter) 
{ 
	memset(&adapter->stats, 0, sizeof(adapter->stats)); 
	return; 
}

#ifndef ESX
/*
 * Send one of the four stats request to the card 
 * 1. Reset statistics
 * 2. One time stats DMA request response
 * 3. Periodic Stats DMA timer start request
 # 4. Periodic Stats DMA timer stop request
 */
static int nx_host_get_net_stats(struct unm_adapter_s *adapter,
                                       uint64_t dma_addr, uint32_t dma_size,
                                       int interval, int reset, int stop_dma)
{
	nic_request_t   req;
	
	if (adapter->fw_v34) {
		return 0; 
	}

	memset(&req, 0, sizeof(nic_request_t));
	req.opcode = NX_NIC_HOST_REQUEST;
	req.body.cmn.req_hdr.opcode = NX_NIC_H2C_OPCODE_GET_NET_STATS_REQUEST;
	req.body.cmn.req_hdr.ctxid =  adapter->nx_dev->rx_ctxs[0]->context_id;
	if (reset) {
		req.body.stats_req.stats_reset = 1;
		goto send_msg;
	}
	req.body.stats_req.stats_reset = 0;
	if (stop_dma) {
		req.body.stats_req.periodic_dma = NX_STATS_DMA_STOP_PERIODIC;
		goto send_msg;
	}
	req.body.stats_req.dma_addr = dma_addr;
	req.body.stats_req.dma_size = dma_size;
	if (interval) {
		req.body.stats_req.periodic_dma = NX_STATS_DMA_START_PERIODIC;
		req.body.cmn.req_hdr.comp_id = 0;
		req.body.stats_req.interval = interval;
	} else {
		req.body.stats_req.periodic_dma = NX_STATS_DMA_ONE_TIME;
		req.body.cmn.req_hdr.comp_id = 1;
	}
send_msg:
	return (nx_nic_send_cmd_descs(adapter->netdev,
                                      (cmdDescType0_t *)&req, 1));
}
#endif


static int unm_nic_attach(struct unm_adapter_s *adapter)
{
	int	rv;
	int	ring;
	int	tmp;

	if (adapter->driver_mismatch) {
		return -EIO;
	}
	if (adapter->is_up != ADAPTER_UP_MAGIC) {
		rv = init_firmware(adapter);
		if (rv != 0) {
			nx_nic_print3(adapter, "Failed to init firmware\n");
			return (-EIO);
		}
		unm_nic_flash_print(adapter);

		/* setup all the resources for the Phantom... */
		/* this include the descriptors for rcv, tx, and status */
		unm_nic_clear_stats(adapter);

		rv = unm_nic_hw_resources(adapter);
		if (rv) {
			nx_nic_print3(adapter, "Error in setting hw "
				      "resources: %d\n", rv);
			return (rv);
		}

		if ((nx_setup_vlan_buffers(adapter)) != 0) {
			nx_free_vlan_buffers(adapter);
			unm_nic_free_ring_context_in_fw(adapter, NX_DESTROY_CTX_RESET);
			unm_nic_free_ring_context(adapter);
			return (-ENOMEM);
		}

		if ((nx_setup_tx_vmkbounce_buffers(adapter)) != 0) {
			nx_free_tx_vmkbounce_buffers(adapter);
			nx_free_vlan_buffers(adapter);
			unm_nic_free_ring_context_in_fw(adapter, NX_DESTROY_CTX_RESET);
			unm_nic_free_ring_context(adapter);
			return (-ENOMEM);
		}

		for (ring = 0; ring < adapter->max_rds_rings; ring++) {
			if (adapter->nx_dev->rx_ctxs[0] != NULL) {
				if (unm_post_rx_buffers(adapter,
						adapter->nx_dev->rx_ctxs[0], ring) &&
			    	adapter->fw_v34) {
					nx_write_rcv_desc_db(adapter,
						adapter->nx_dev->rx_ctxs[0], ring);
				}
			}

		}

		rv = nx_register_irq(adapter,adapter->nx_dev->rx_ctxs[0]);
		if (rv) {
			nx_nic_print3(adapter, "Unable to register the "
				      "interrupt service routine\n");
			nx_free_tx_vmkbounce_buffers(adapter);
			nx_free_vlan_buffers(adapter);
			unm_nic_free_ring_context_in_fw(adapter, NX_DESTROY_CTX_RESET);
			unm_nic_free_ring_context(adapter);
			return (rv);
		}

		if (NX_IS_LSA_REGISTERED(adapter))
			nx_napi_enable(adapter);
		
#ifndef ESX
		nx_host_get_net_stats(adapter, adapter->nic_net_stats.phys,
					sizeof(nx_ctx_stats_t),
					NX_STATS_DMA_INTERVAL, 0, 0);
#endif

		adapter->is_up = ADAPTER_UP_MAGIC;

	}
	if (!NX_IS_LSA_REGISTERED(adapter))
		nx_napi_enable(adapter);

	/* Set up virtual-to-physical port mapping */
	if (NX_IS_REVISION_P2(adapter->ahw.revision_id)) {
		tmp = NXRD32(adapter, CRB_V2P(adapter->portnum));

		if (tmp != 0x55555555) {
			nx_nic_print6(adapter,
				      "PCI Function %d using Phy Port %d",
				      adapter->portnum, tmp);
			adapter->physical_port = tmp;
		}
	} else {
		adapter->physical_port = (adapter->nx_dev->rx_ctxs[0])->port;
	}

	read_lock(&adapter->adapter_lock);
	unm_nic_enable_all_int(adapter, adapter->nx_dev->rx_ctxs[0]);
	read_unlock(&adapter->adapter_lock);

	if (unm_nic_macaddr_set(adapter, adapter->mac_addr) != 0) {
                nx_nic_print3(adapter, "Cannot set Mac addr.\n");
		nx_free_tx_resources(adapter);
		return (-EIO);
	}
	memcpy(adapter->netdev->dev_addr, adapter->mac_addr,
	       adapter->netdev->addr_len);

	if (unm_nic_init_port(adapter) != 0) {
		nx_nic_print3(adapter, "Failed to initialize the port %d\n",
			      adapter->portnum);

		nx_free_tx_resources(adapter);
		return (-EIO);
	}

	/* Set the user configured speed, duplex, autoneg. 
	   Check if they are valid values */
	if (adapter->ahw.board_type == UNM_NIC_GBE 
		&&	(adapter->ahw.fw_capabilities_1 & NX_FW_CAPABILITY_GBE_LINK_CFG))
	{

		if (adapter->cfg_speed != NX_INVALID_SPEED_CFG &&
			adapter->cfg_duplex != NX_INVALID_DUPLEX_CFG &&
			adapter->cfg_autoneg != NX_INVALID_AUTONEG_CFG) {
		
			int ret_val;
			ret_val = nx_fw_cmd_set_gbe_port(adapter, adapter->physical_port,
					adapter->cfg_speed, adapter->cfg_duplex, adapter->cfg_autoneg);
			if (!ret_val) {
				adapter->link_speed = adapter->cfg_speed;
				adapter->link_duplex = adapter->cfg_duplex;
				adapter->link_autoneg = adapter->cfg_autoneg;
			} else {
				nx_nic_print3(adapter, "Error in re-setting "
					" user-defined speed(%d)/duplex(%d)/autoneg(%d) config\n",
					adapter->cfg_speed, adapter->cfg_duplex, adapter->cfg_autoneg);
			}
		}
	}

	adapter->netdev->set_multicast_list(adapter->netdev);

	if ((adapter->flags & UNM_NIC_MSIX_ENABLED) &&
	    adapter->max_possible_rss_rings > 1) {
		nx_nic_print6(adapter, "RSS being enabled\n");
		nx_config_rss(adapter, 1);
	}

	if (adapter->netdev->change_mtu) {
		adapter->netdev->change_mtu(adapter->netdev,
					    adapter->netdev->mtu);
	}

	adapter->ahw.linkup = 0;
	mod_timer(&adapter->watchdog_timer, jiffies);

	if (!adapter->fw_v34) {
		/* enable notifications of link events */
		nic_linkevent_request(adapter, 1, 1);
	}
	spin_lock(&adapter->ctx_state_lock);
	adapter->attach_flag = 1;
	spin_unlock(&adapter->ctx_state_lock);
	return (0);
}

static void nx_nic_p3_free_mac_list(struct unm_adapter_s *adapter)
{
	mac_list_t *cur, *next, *del_list;

	del_list = adapter->mac_list;
	adapter->mac_list = NULL;

	for (cur = del_list; cur;) {
		next = cur->next;
		kfree(cur);
		cur = next;
	}
}

static void unm_nic_detach(struct unm_adapter_s *adapter, int ctx_destroy)
{
	nx_os_wait_event_t   *wait      = NULL;
	struct list_head     *ptr, *tmp = NULL;
	int index = 0;

	spin_lock(&adapter->ctx_state_lock);
	adapter->attach_flag = 0;
	spin_unlock(&adapter->ctx_state_lock);
	if (adapter->is_up == ADAPTER_UP_MAGIC || adapter->is_up == FW_DEAD) {
		/*
		 * Step 3: Destroy the FW context to stop all RX traffic.
		 */
		if(adapter->is_up == FW_DEAD)
			ctx_destroy = NX_DESTROY_CTX_NO_FWCMD;
		unm_nic_free_ring_context_in_fw(adapter, ctx_destroy);

		/*
		 * Step 4: Disable the IRQ.
		 */
		for (index = 0; index < adapter->nx_dev->alloc_rx_ctxs;
		     index++) {
			if (adapter->nx_dev->rx_ctxs[index] != NULL) {
				spin_lock(&adapter->ctx_state_lock);
                if((adapter->nx_dev->rx_ctxs_state & (1 << index)) != 0) { 
					spin_unlock(&adapter->ctx_state_lock);
				} else {
					spin_unlock(&adapter->ctx_state_lock);
					continue;
				}
				nx_unregister_irq(adapter,
					adapter->nx_dev->rx_ctxs[index]);
			}
		}
	}

	/*
	 * Step 5: Destroy all the SW data structures.
	 */
	unm_destroy_pending_cmd_desc(&adapter->pending_cmds);

	if (adapter->portnum == 0) {
		destroy_dummy_dma(adapter);
	}

	nx_free_vlan_buffers(adapter);
	nx_free_tx_vmkbounce_buffers(adapter);

	if (!list_empty(&adapter->wait_list)) {
		list_for_each_safe(ptr, tmp, &adapter->wait_list) {
			wait = list_entry(ptr, nx_os_wait_event_t, list);

			wait->trigger = 1;
			wait->active  = 0;
#ifdef ESX_3X
			vmk_thread_wakeup(&wait->wq);
#else
			wake_up_interruptible(&wait->wq);
#endif
		}		
	}

	unm_nic_free_ring_context(adapter);

	nx_free_pexq_dbell(adapter);

	if (NX_IS_REVISION_P3(adapter->ahw.revision_id)) {
		nx_nic_p3_free_mac_list(adapter);
	}
	adapter->is_up = 0;
}


static void __devexit unm_nic_remove(struct pci_dev *pdev)
{
	struct unm_adapter_s *adapter;
	struct net_device *netdev;
	int index = 0;

	netdev = pci_get_drvdata(pdev);
	if (netdev == NULL) {
		return;
	}

	adapter = netdev_priv(netdev);
	if (adapter == NULL) {
		return;
	}

	spin_lock(&adapter->ctx_state_lock);
	adapter->attach_flag = 0;
	spin_unlock(&adapter->ctx_state_lock);

	adapter->removing_module = 1;

	if (NX_IS_LSA_REGISTERED(adapter)) {

		read_lock(&adapter->adapter_lock);
		for (index = 0; index < adapter->nx_dev->alloc_rx_ctxs;
		     index++) {
			if (adapter->nx_dev->rx_ctxs[index] != NULL) {
				unm_nic_disable_all_int(adapter,
					adapter->nx_dev->rx_ctxs[index]);
			}
		}
		read_unlock(&adapter->adapter_lock);

		if (adapter->is_up == ADAPTER_UP_MAGIC || adapter->is_up == FW_DEAD) 
			nx_napi_disable(adapter);
	}
	if(adapter->portnum == 0 ) {
		FLUSH_SCHEDULED_WORK();
       		del_timer_sync(&adapter->watchdog_timer_fw_reset);
	}
	unregister_netdev(netdev);
	unm_nic_detach(adapter, NX_DESTROY_CTX_RESET);

	nx_release_fw(&adapter->nx_fw);
	cleanup_adapter(adapter);

	pci_set_drvdata(pdev, NULL);

#ifdef OLD_KERNEL
	kfree(netdev);
#else
	free_netdev(netdev);
#endif
}

static int nx_config_rss(struct unm_adapter_s *adapter, int enable)
{
	nic_request_t req;
	rss_config_t *config;
	__uint64_t key[] = { 0xbeac01fa6a42b73bULL, 0x8030f20c77cb2da3ULL,
		0xae7b30b4d0ca2bcbULL, 0x43a38fb04167253dULL,
		0x255b0ec26d5a56daULL
	};
	int rv;

	req.opcode = NX_NIC_HOST_REQUEST;
	config = (rss_config_t *) & req.body;
	config->req_hdr.opcode = NX_NIC_H2C_OPCODE_CONFIG_RSS;
	config->req_hdr.comp_id = 1;
	config->req_hdr.ctxid = adapter->portnum;
	config->req_hdr.need_completion = 0;

	config->hash_type_v4 = RSS_HASHTYPE_IP_TCP;
	config->hash_type_v6 = RSS_HASHTYPE_IP_TCP;
	config->enable = enable ? 1 : 0;
	config->use_indir_tbl = 0;
	config->indir_tbl_mask = 7;
	config->secret_key[0] = key[0];
	config->secret_key[1] = key[1];
	config->secret_key[2] = key[2];
	config->secret_key[3] = key[3];
	config->secret_key[4] = key[4];

	rv = nx_nic_send_cmd_descs(adapter->netdev, (cmdDescType0_t *)&req, 1);
	if (rv) {
		nx_nic_print3(adapter, "Sending RSS config to FW failed %d\n",
			      rv);
	}

	return (rv);
}


int nx_nic_multictx_get_filter_count(struct net_device *netdev, int queue_type)
{
	struct unm_adapter_s *adapter = netdev_priv(netdev);
	nx_host_nic_t* nx_dev;
	U32 pci_func;
	U32 max;
	U32 rcode;

	if(adapter->is_up == FW_DEAD) { 
		return -1;
	}

	nx_dev = adapter->nx_dev;
	pci_func = adapter->nx_dev->pci_func;

	if (MULTICTX_IS_TX(queue_type)) {
		nx_nic_print3(adapter, "%s: TX filters not supported\n",
				__FUNCTION__);
		return -1;
	} else if(MULTICTX_IS_RX(queue_type)) {
		rcode = nx_fw_cmd_query_max_rules_per_ctx(nx_dev, pci_func,
					 &max) ;
	} else {
		nx_nic_print3(adapter, "%s: Invalid ctx type specified\n",
				__FUNCTION__);
		return -1;
	}

	if( rcode != NX_RCODE_SUCCESS){
		return -1;
	}

	return (max);
}
#ifdef ESX_4X
struct napi_struct * nx_nic_multictx_get_napi(struct net_device *netdev , int queue_id) 
{
	struct unm_adapter_s *adapter = netdev_priv(netdev);

		if(queue_id < 0 || queue_id > adapter->num_rx_queues)
			return NULL;
		return &(adapter->host_sds_rings[queue_id].napi);	
}
#endif
int nx_nic_multictx_get_ctx_count(struct net_device *netdev, int queue_type)
{
	U32 max = 0;

#if 0
	nx_host_nic_t* nx_dev;
	U32 pci_func ;
	U32 rcode;
	struct unm_adapter_s *adapter = netdev_priv(netdev);
	nx_dev = adapter->nx_dev;
	pci_func = adapter->nx_dev->pci_func;

	if (MULTICTX_IS_TX(queue_type)) {

		rcode = nx_fw_cmd_query_max_tx_ctx(nx_dev, pci_func, &max) ;

	} else if(MULTICTX_IS_RX(queue_type)) {

		rcode = nx_fw_cmd_query_max_rx_ctx(nx_dev, pci_func, &max) ;

	} else {
		nx_nic_print3(adapter, "%s: Invalid ctx type specified\n",__FUNCTION__);
		return -1;
	}

	if( rcode != NX_RCODE_SUCCESS){
		return -1;
	}
#endif
#ifdef ESX
	struct unm_adapter_s *adapter = netdev_priv(netdev);

	if(adapter->is_up == FW_DEAD) { 
		return -1;
	}

	max = adapter->num_rx_queues ;
#endif
	return (max);
}

int nx_nic_multictx_get_queue_vector(struct net_device *netdev, int qid)
{
#ifdef ESX
	struct unm_adapter_s *adapter = netdev_priv(netdev);

	if(qid < 0 || qid > adapter->num_rx_queues)
		return -1;
	return adapter->msix_entries[qid].vector ;
#else
	return 0;
#endif
	
}

int nx_nic_multictx_get_ctx_stats(struct net_device *netdev, int ctx_id,
		struct net_device_stats *stats)
{
	struct unm_adapter_s *adapter = netdev_priv(netdev);
	nx_host_rx_ctx_t *nxhal_host_rx_ctx = NULL;
	nx_host_rds_ring_t *nxhal_host_rds_ring = NULL;
	rds_host_ring_t *host_rds_ring = NULL;
	int ring;
#ifdef ESX
	if(adapter->is_up == FW_DEAD) { 
		return -1;
	}
	if(ctx_id < 0 || ctx_id > adapter->num_rx_queues)
		return -1;
#endif
	nxhal_host_rx_ctx = adapter->nx_dev->rx_ctxs[ctx_id];
	if(nxhal_host_rx_ctx == NULL)
		return -1;
	memset(stats, 0 ,sizeof(struct net_device_stats));
	for (ring = 0; ring < adapter->max_rds_rings; ring++) {
		nxhal_host_rds_ring = &nxhal_host_rx_ctx->rds_rings[ring];
		host_rds_ring =
			(rds_host_ring_t *) nxhal_host_rds_ring->os_data;
		stats->rx_packets += host_rds_ring->stats.no_rcv;
		stats->rx_bytes += host_rds_ring->stats.rxbytes;
		stats->rx_errors += host_rds_ring->stats.rcvdbadskb;
		stats->rx_dropped += host_rds_ring->stats.updropped;
	}
	return 0;

}

int nx_nic_multictx_get_default_rx_queue(struct net_device *netdev)
{
	return 0;
}


int nx_nic_multictx_alloc_tx_ctx(struct net_device *netdev)
{
	nx_nic_print6(NULL, "%s: Opertion not supported\n", __FUNCTION__);
	return -1;
}

static void nx_nic_free_hw_rx_resources(struct unm_adapter_s *adapter,
                nx_host_rx_ctx_t *nxhal_host_rx_ctx)

{
        int ring;
        nx_host_rds_ring_t *nxhal_host_rds_ring = NULL;
        rds_host_ring_t *host_rds_ring = NULL;

        for (ring = 0; ring < adapter->max_rds_rings; ring++) {
                nxhal_host_rds_ring = &nxhal_host_rx_ctx->rds_rings[ring];
                host_rds_ring =
                        (rds_host_ring_t *) nxhal_host_rds_ring->os_data;
                if (nxhal_host_rds_ring->host_addr != NULL) {
                        pci_free_consistent(host_rds_ring->phys_pdev,
                                        RCV_DESC_RINGSIZE
                                        (nxhal_host_rds_ring->ring_size),
                                        nxhal_host_rds_ring->host_addr,
                                        nxhal_host_rds_ring->host_phys);
                        nxhal_host_rds_ring->host_addr = NULL;
                }
        }
}

static void nx_nic_free_host_sds_resources(struct unm_adapter_s *adapter,
                nx_host_rx_ctx_t *nxhal_host_rx_ctx)
{

        int ring;

	for (ring = 0; ring < nxhal_host_rx_ctx->num_sds_rings; ring++) {
		if (nxhal_host_rx_ctx->sds_rings[ring].os_data) {
			nxhal_host_rx_ctx->sds_rings[ring].os_data = NULL;
                }
        }
}

static void nx_nic_free_host_rx_resources(struct unm_adapter_s *adapter,
					  nx_host_rx_ctx_t *nxhal_host_rx_ctx)
{

	int ring;
	rds_host_ring_t *host_rds_ring = NULL;

	for (ring = 0; ring < adapter->max_rds_rings; ring++) {
		if (nxhal_host_rx_ctx->rds_rings[ring].os_data) {
			host_rds_ring =
				nxhal_host_rx_ctx->rds_rings[ring].os_data;
			vfree(host_rds_ring->rx_buf_arr);
			host_rds_ring->rx_buf_arr = NULL;
			kfree(nxhal_host_rx_ctx->rds_rings[ring].os_data);
			nxhal_host_rx_ctx->rds_rings[ring].os_data = NULL;
		}
	}

}

static int nx_nic_alloc_hw_rx_resources(struct unm_adapter_s *adapter,
					nx_host_rx_ctx_t *nxhal_host_rx_ctx)
{

        nx_host_rds_ring_t *nxhal_host_rds_ring = NULL;
        rds_host_ring_t *host_rds_ring = NULL;
        int ring;
        int err = 0;
        void *addr;

        for (ring = 0; ring < adapter->max_rds_rings; ring++) {
                nxhal_host_rds_ring = &nxhal_host_rx_ctx->rds_rings[ring];
                host_rds_ring =
                        (rds_host_ring_t *) nxhal_host_rds_ring->os_data;
                addr = nx_alloc(adapter,
				RCV_DESC_RINGSIZE(nxhal_host_rds_ring->ring_size),
				(dma_addr_t *)&nxhal_host_rds_ring->host_phys,
				&host_rds_ring->phys_pdev);
                if (addr == NULL) {
                        nx_nic_print3(adapter, "bad return from nx_alloc\n");
                        err = -ENOMEM;
                        goto done;
                }
                nx_nic_print7(adapter, "rcv %d physAddr: 0x%llx\n",
			      ring, (U64)nxhal_host_rds_ring->host_phys);

                nxhal_host_rds_ring->host_addr = (I8 *) addr;
        }
        return 0;
  done:
        nx_nic_free_hw_rx_resources(adapter, nxhal_host_rx_ctx);
	return err;
}

static void nx_nic_alloc_host_sds_resources(struct unm_adapter_s *adapter,
		nx_host_rx_ctx_t *nxhal_host_rx_ctx)
{
	int ring = 0;
	int ctx_id = nxhal_host_rx_ctx->this_id;
	int num_sds_rings = nxhal_host_rx_ctx->num_sds_rings;
	sds_host_ring_t *host_ring;

	for(ring = 0; ring < num_sds_rings; ring ++) {
		host_ring =
			&adapter->host_sds_rings[ctx_id * num_sds_rings + ring];
		host_ring->ring = &nxhal_host_rx_ctx->sds_rings[ring];
		nxhal_host_rx_ctx->sds_rings[ring].os_data = host_ring;
	}
}

static int nx_nic_alloc_host_rx_resources(struct unm_adapter_s *adapter,
                nx_host_rx_ctx_t *nxhal_host_rx_ctx)
{

        nx_host_rds_ring_t *nxhal_host_rds_ring = NULL;
        rds_host_ring_t *host_rds_ring = NULL;
        int ring;
        int err = 0;
        struct unm_rx_buffer *rx_buf_arr = NULL;

        for (ring = 0; ring < adapter->max_rds_rings; ring++) {
                nxhal_host_rx_ctx->rds_rings[ring].os_data =
                        kmalloc(sizeof(rds_host_ring_t), GFP_KERNEL);
                if (nxhal_host_rx_ctx->rds_rings[ring].os_data == NULL) {
                        nx_nic_print3(adapter, "RX ring memory allocation "
				      "FAILED\n");
                        err = -ENOMEM;
                        goto err_ret;
                }
                memset(nxhal_host_rx_ctx->rds_rings[ring].os_data, 0,
		       sizeof(rds_host_ring_t));
        }

        for (ring = 0; ring < adapter->max_rds_rings; ring++) {
                nxhal_host_rds_ring = &nxhal_host_rx_ctx->rds_rings[ring];
                host_rds_ring =
                    (rds_host_ring_t *)nxhal_host_rds_ring->os_data;
		host_rds_ring->truesize_delta = 0;
                switch (RCV_DESC_TYPE(ring)) {
                case RCV_RING_STD:
                        nxhal_host_rds_ring->ring_size =
                            adapter->MaxRxDescCount;
                        nxhal_host_rds_ring->ring_kind = RCV_DESC_NORMAL;

			if (adapter->fw_v34) {
				host_rds_ring->dma_size =
					NX_RX_V34_NORMAL_BUF_MAX_LEN;
				nxhal_host_rds_ring->buff_size =
					(host_rds_ring->dma_size +
					 IP_ALIGNMENT_BYTES);
                        } else if (adapter->ahw.cut_through) {
                                nxhal_host_rds_ring->buff_size =
					NX_CT_DEFAULT_RX_BUF_LEN;
                                host_rds_ring->dma_size =
					nxhal_host_rds_ring->buff_size;
                        } else {
                                host_rds_ring->dma_size =
					NX_RX_NORMAL_BUF_MAX_LEN;
                                nxhal_host_rds_ring->buff_size =
					(host_rds_ring->dma_size +
					 IP_ALIGNMENT_BYTES);
                        }
			nx_nic_print6(adapter, "Buffer size = %d\n",
				      (int)nxhal_host_rds_ring->buff_size);
                        break;

                case RCV_RING_JUMBO:
                        nxhal_host_rds_ring->ring_size =
				adapter->MaxJumboRxDescCount;
                        nxhal_host_rds_ring->ring_kind = RCV_DESC_JUMBO;

                        if (NX_IS_REVISION_P2(adapter->ahw.revision_id)) {
                                host_rds_ring->dma_size =
					NX_P2_RX_JUMBO_BUF_MAX_LEN;
                        } else {
				host_rds_ring->dma_size =
					NX_P3_RX_JUMBO_BUF_MAX_LEN;
				if (!adapter->ahw.cut_through) {
					host_rds_ring->dma_size +=
						NX_NIC_JUMBO_EXTRA_BUFFER;
					host_rds_ring->truesize_delta =
						NX_NIC_JUMBO_EXTRA_BUFFER;
				}
                        }
                        nxhal_host_rds_ring->buff_size =
				(host_rds_ring->dma_size + IP_ALIGNMENT_BYTES);
                        break;

                case RCV_RING_LRO:
                        nxhal_host_rds_ring->ring_size =
				adapter->MaxLroRxDescCount;
                        nxhal_host_rds_ring->ring_kind = RCV_DESC_LRO;
                        host_rds_ring->dma_size = RX_LRO_DMA_MAP_LEN;
                        nxhal_host_rds_ring->buff_size =
				MAX_RX_LRO_BUFFER_LENGTH;
                        break;

                default:
                        nx_nic_print3(adapter,
				      "bad receive descriptor type %d\n",
				      RCV_DESC_TYPE(ring));
                        break;
                }
                rx_buf_arr =
                    vmalloc(RCV_BUFFSIZE(nxhal_host_rds_ring->ring_size));
                if (rx_buf_arr == NULL) {
                        nx_nic_print3(adapter, "Rx buffer alloc error."
				      "Check system memory resource usage.\n");
                        err = -ENOMEM;
                        goto err_ret;
                }
		memset(rx_buf_arr, 0,
                       RCV_BUFFSIZE(nxhal_host_rds_ring->ring_size));
                host_rds_ring->rx_buf_arr = rx_buf_arr;
        }

	initialize_adapter_sw(adapter, nxhal_host_rx_ctx);

        return err;

err_ret:
	nx_nic_free_host_rx_resources(adapter, nxhal_host_rx_ctx);
        return err;
}

int nx_nic_multictx_alloc_rx_ctx(struct net_device *netdev)
{
	int ctx_id = -1,ring = 0;
	int err = 0;
	nx_host_rx_ctx_t *nxhal_host_rx_ctx = NULL;
	struct unm_adapter_s *adapter = netdev_priv(netdev);

	if(adapter->is_up == FW_DEAD) { 
		return -1;
	}
	ctx_id = nx_nic_create_rx_ctx(netdev);
	if( ctx_id >= 0 ) {
		nxhal_host_rx_ctx = adapter->nx_dev->rx_ctxs[ctx_id];

		if (nx_fw_cmd_set_mtu(nxhal_host_rx_ctx, adapter->ahw.pci_func,
					netdev->mtu)) {
			nx_nic_print6(adapter,"Unable to set mtu for context: %d\n",ctx_id);
		}

		for (ring = 0; ring < adapter->max_rds_rings; ring++) {
			if (unm_post_rx_buffers(adapter,nxhal_host_rx_ctx,
						ring)) {
                               if (adapter->fw_v34)
                                       nx_write_rcv_desc_db(adapter,nxhal_host_rx_ctx, ring);
			}
		}
		err = nx_register_irq(adapter,nxhal_host_rx_ctx);
		if(err) {
			goto err_ret;
		}
		read_lock(&adapter->adapter_lock);
		unm_nic_enable_all_int(adapter, nxhal_host_rx_ctx);
		read_unlock(&adapter->adapter_lock);
		adapter->nx_dev->rx_ctxs_state |= (1 << ctx_id);	
		return ctx_id;
	}
err_ret:
	return -1;

}

int nx_nic_create_rx_ctx(struct net_device *netdev)
{
        int err = 0;
        nx_host_rx_ctx_t *nxhal_host_rx_ctx = NULL;
        struct unm_adapter_s *adapter = netdev_priv(netdev);
	int num_rules = 1;
        int num_rss_rings =   adapter->max_possible_rss_rings;
        err = nx_fw_cmd_create_rx_ctx_alloc(adapter->nx_dev,
					    adapter->max_rds_rings,
                                          num_rss_rings, num_rules,
                                          &nxhal_host_rx_ctx);
        if (err) {
                nx_nic_print3(adapter, "RX ctx memory allocation FAILED\n");
                goto err_ret;
        }
        if ((err = nx_nic_alloc_host_rx_resources(adapter,
						  nxhal_host_rx_ctx))) {
                nx_nic_print3(adapter, "RDS memory allocation FAILED\n");
                goto err_ret;
        }


        if ((err = nx_nic_alloc_hw_rx_resources(adapter, nxhal_host_rx_ctx))) {
                nx_nic_print3(adapter, "RDS Descriptor ring memory allocation "
			      "FAILED\n");
                goto err_ret;
        }

	nx_nic_alloc_host_sds_resources(adapter, nxhal_host_rx_ctx);

	err = nx_alloc_sts_rings(adapter, nxhal_host_rx_ctx,
				 nxhal_host_rx_ctx->num_sds_rings);
        if (err) {
		nx_nic_print3(adapter, "SDS memory allocation FAILED\n");
                goto err_ret;
        }

       	if ((err = unm_nic_new_rx_context_prepare(adapter,
						  nxhal_host_rx_ctx))) {
               	nx_nic_print3(adapter, "RX ctx allocation FAILED\n");
               	goto err_ret;
       	}

        return nxhal_host_rx_ctx->this_id;

  err_ret:
        if (nxhal_host_rx_ctx && adapter->nx_dev) {
		nx_free_sts_rings(adapter,nxhal_host_rx_ctx);
                nx_nic_free_host_sds_resources(adapter,nxhal_host_rx_ctx);
                nx_free_rx_resources(adapter,nxhal_host_rx_ctx);
                nx_nic_free_hw_rx_resources(adapter, nxhal_host_rx_ctx);
                nx_nic_free_host_rx_resources(adapter, nxhal_host_rx_ctx);
                nx_fw_cmd_create_rx_ctx_free(adapter->nx_dev,
					     nxhal_host_rx_ctx);
        }

        return (-1);
}

int nx_nic_multictx_free_tx_ctx(struct net_device *netdev, int ctx_id)
{
        struct unm_adapter_s *adapter = netdev_priv(netdev);

	nx_nic_print3(adapter, "%s: Operation not supported\n", __FUNCTION__);
	return -1;
}

int nx_nic_multictx_free_rx_ctx(struct net_device *netdev, int ctx_id)
{
        nx_host_rx_ctx_t *nxhal_host_rx_ctx = NULL;
        struct unm_adapter_s *adapter = netdev_priv(netdev);

	if(adapter->is_up == FW_DEAD) { 
		return -1;
	}
	if (ctx_id > adapter->nx_dev->alloc_rx_ctxs) {
		nx_nic_print4(adapter, "%s: Invalid context id\n",
			      __FUNCTION__);
		return -1;
	}

	if(adapter->attach_flag == 0) {
		return -1;
	}

	nxhal_host_rx_ctx = adapter->nx_dev->rx_ctxs[ctx_id];
	if (nxhal_host_rx_ctx != NULL) {
		spin_lock(&adapter->ctx_state_lock);
		if(((adapter->nx_dev->rx_ctxs_state & (1 << ctx_id)) != 0) && (adapter->attach_flag == 1)) {
			adapter->nx_dev->rx_ctxs_state &= ~(1 << ctx_id);
			spin_unlock(&adapter->ctx_state_lock);
		} else { 
			spin_unlock(&adapter->ctx_state_lock);
			return -1;
		}
		napi_synchronize(&adapter->host_sds_rings[ctx_id].napi);
		read_lock(&adapter->adapter_lock);
		unm_nic_disable_all_int(adapter, nxhal_host_rx_ctx);
		read_unlock(&adapter->adapter_lock);
		nx_unregister_irq(adapter,nxhal_host_rx_ctx);
		nx_fw_cmd_destroy_rx_ctx(nxhal_host_rx_ctx,
				NX_DESTROY_CTX_RESET);
		nx_lro_delete_ctx_lro(adapter, nxhal_host_rx_ctx->context_id);
		nx_free_sts_rings(adapter, nxhal_host_rx_ctx);
		nx_nic_free_host_sds_resources(adapter, nxhal_host_rx_ctx);
		nx_free_rx_resources(adapter, nxhal_host_rx_ctx);
		nx_nic_free_hw_rx_resources(adapter, nxhal_host_rx_ctx);
		nx_nic_free_host_rx_resources(adapter, nxhal_host_rx_ctx);
		nx_fw_cmd_create_rx_ctx_free(adapter->nx_dev,
                                nxhal_host_rx_ctx);
	} else {
		return (-1);
	}

	return 0;
}

int nx_nic_multictx_set_rx_rule(struct net_device *netdev, int ctx_id, char* mac_addr)
{
	int rv;
	int i;
        nx_host_rx_ctx_t *nxhal_host_rx_ctx = NULL;
	nx_rx_rule_t *rx_rule = NULL;
        struct unm_adapter_s *adapter = netdev_priv(netdev);

	if(adapter->is_up == FW_DEAD) { 
		return -1;
	}
	if (adapter->fw_v34) {
		nx_nic_print3(adapter, "%s: does not support in FW V3.4\n", __FUNCTION__);
		return -1;
	}

	if(ctx_id > adapter->nx_dev->alloc_rx_ctxs) {
		nx_nic_print3(adapter, "%s: Invalid context id\n",__FUNCTION__);
		return -1;
	}

	nxhal_host_rx_ctx = adapter->nx_dev->rx_ctxs[ctx_id];

	if(!nxhal_host_rx_ctx) {
		nx_nic_print3(adapter, "%s: Ctx not active\n", __FUNCTION__);
		return -1;
	}

	if(nxhal_host_rx_ctx->active_rx_rules >= nxhal_host_rx_ctx->num_rules) {
		nx_nic_print3(adapter, "%s: Rules counts exceeded\n",__FUNCTION__);
		return -1;
	}

	for (i = 0; i < nxhal_host_rx_ctx->num_rules; i++) {
		if(!nxhal_host_rx_ctx->rules[i].active) {
			rx_rule = &nxhal_host_rx_ctx->rules[i];
			rx_rule->id = i;
			break;
		}
	}

	if(!rx_rule) {
		nx_nic_print3(adapter, "%s: No rule available\n",__FUNCTION__);
		return -1;
	}

	rv = nx_os_pf_add_l2_mac(adapter->nx_dev, nxhal_host_rx_ctx, mac_addr);

	if (rv == 0) {
		rx_rule->active = 1;
		nx_os_copy_memory(&(rx_rule->arg.m.mac), mac_addr, 6) ;
		rx_rule->type = NX_RX_RULETYPE_MAC;
		nx_os_copy_memory(&adapter->nx_dev->saved_rules[ctx_id], rx_rule, sizeof(nx_rx_rule_t));
		nxhal_host_rx_ctx->active_rx_rules++;
	} else {
		nx_nic_print3(adapter, "%s: Failed to set mac addr\n", __FUNCTION__);
		return -1;
	}

	return rx_rule->id;
}


int nx_nic_multictx_remove_rx_rule(struct net_device *netdev, int ctx_id, int rule_id)
{
	int rv;
        nx_host_rx_ctx_t *nxhal_host_rx_ctx = NULL;
	nx_rx_rule_t *rx_rule = NULL;
        struct unm_adapter_s *adapter = netdev_priv(netdev);
	char mac_addr[6];

	if(adapter->is_up == FW_DEAD) { 
		return -1;
	}
	if (adapter->fw_v34) {
		nx_nic_print3(adapter, "%s: does not support in FW V3.4\n", __FUNCTION__);
		return -1;
	}

	if(ctx_id > adapter->nx_dev->alloc_rx_ctxs) {
		nx_nic_print3(adapter, "%s: Invalid context id\n",__FUNCTION__);
		return -1;
	}

	nxhal_host_rx_ctx = adapter->nx_dev->rx_ctxs[ctx_id];

	if(!nxhal_host_rx_ctx) {
		nx_nic_print3(adapter, "%s: Ctx not active\n", __FUNCTION__);
		return -1;
	}

	if(rule_id >= nxhal_host_rx_ctx->num_rules) {
		nx_nic_print3(adapter, "%s: Invalid rule id specified\n",__FUNCTION__);
		return -1;
	}

	rx_rule = &nxhal_host_rx_ctx->rules[rule_id];

	if(!rx_rule->active) {
		nx_nic_print3(adapter, "%s: Deleting an inactive rule \n",__FUNCTION__);
		return -1;
	}

	nx_os_copy_memory(mac_addr, &(rx_rule->arg.m.mac), 6);

	rv = nx_os_pf_remove_l2_mac(adapter->nx_dev, nxhal_host_rx_ctx, mac_addr);

	if (rv == 0) {
		rx_rule->active = 0;
		nxhal_host_rx_ctx->active_rx_rules--;
	} else {
		nx_nic_print3(adapter, "%s: Failed to delete mac addr\n", __FUNCTION__);
		return -1;
	}

	return rv;
}

/*
 * For V3.4 firmware
 * Write a doorbell msg to tell phanmon of change in receive ring producer
 */
static void nx_write_rcv_desc_db(struct unm_adapter_s *adapter,
				 nx_host_rx_ctx_t *nxhal_rx_ctx, int ring)
{
       nx_host_rds_ring_t *nxhal_rds_ring = &nxhal_rx_ctx->rds_rings[ring];
       ctx_msg msg = {0};

       msg.PegId = UNM_RCV_PEG_DB_ID;
       msg.privId = 1;
       msg.Count = ((nxhal_rds_ring->producer_index - 1) &
                    (nxhal_rds_ring->ring_size - 1));
       msg.CtxId = adapter->portnum;
       msg.Opcode = UNM_RCV_PRODUCER(ring);

       read_lock(&adapter->adapter_lock);
       UNM_NIC_PCI_WRITE_32(*((__uint32_t *)&msg),
                            DB_NORMALIZE(adapter, UNM_RCV_PRODUCER_OFFSET));
       read_unlock(&adapter->adapter_lock);
}

/*
 * Called when a network interface is made active
 * Returns 0 on success, negative value on failure
 */
static int unm_nic_open(struct net_device *netdev)
{
	struct unm_adapter_s *adapter = (struct unm_adapter_s *)netdev_priv(netdev);
	int err = 0;


	err = unm_nic_attach(adapter);
	if (err != 0) {
		nx_nic_print3(adapter, "Failed to Attach to device\n");
		return (err);
	}

	netif_start_queue(netdev);
	adapter->state = PORT_UP;

	return 0;
}

void *nx_alloc(struct unm_adapter_s *adapter, size_t sz,
	       dma_addr_t * ptr, struct pci_dev **used_dev)
{
	struct pci_dev *pdev;
	void *addr;

	pdev = adapter->ahw.pdev;

	addr = pci_alloc_consistent(pdev, sz, ptr);
	if ((unsigned long long)((*ptr) + sz) < adapter->dma_mask) {
		*used_dev = pdev;
		return addr;
	}
	pci_free_consistent(pdev, sz, addr, *ptr);
#ifdef ESX
	return NULL;
#endif
	addr = pci_alloc_consistent(NULL, sz, ptr);
	*used_dev = NULL;
	return addr;
}

static void nx_free_sts_rings(struct unm_adapter_s *adapter,
			      nx_host_rx_ctx_t *nxhal_rx_ctx)
{
	int i;
	int ctx_id;
	nx_host_sds_ring_t	*nxhal_sds_ring;
	sds_host_ring_t		*host_sds_ring;
	ctx_id = nxhal_rx_ctx->this_id;
	for (i = 0; i < nxhal_rx_ctx->num_sds_rings; i++) {
		nxhal_sds_ring = &nxhal_rx_ctx->sds_rings[i];
		host_sds_ring = nxhal_sds_ring->os_data;
#ifdef NEW_NAPI
		host_sds_ring->netdev = NULL;
#else
		if (((i != 0 && ctx_id == 0) || (i == 0 && ctx_id!= 0)) && host_sds_ring->netdev) {
			nx_nic_print7(adapter, "Freeing[%u] netdev\n", i);
			FREE_NETDEV(host_sds_ring->netdev);
			host_sds_ring->netdev = NULL;
		}
#endif
		if (nxhal_sds_ring->host_addr) {
			pci_free_consistent(host_sds_ring->pci_dev,
					    STATUS_DESC_RINGSIZE(adapter->
								 MaxRxDescCount),
					    nxhal_sds_ring->host_addr,
					    nxhal_sds_ring->host_phys);
			nxhal_sds_ring->host_addr = NULL;
		}
#ifdef NEW_NAPI
		host_sds_ring->ring = NULL;
		host_sds_ring->adapter = NULL;
#endif
	}
}

/*
 * Allocate the receive status rings for RSS.
 */
static int nx_alloc_sts_rings(struct unm_adapter_s *adapter,
			      nx_host_rx_ctx_t *nxhal_rx_ctx, int cnt)
{
#ifndef	NEW_NAPI
	struct net_device *netdev;
#endif
	int i;
	int j;
	int ctx_id ;
	void *addr;
	nx_host_sds_ring_t *nxhal_sds_ring = NULL;
	sds_host_ring_t	   *host_sds_ring = NULL;
	ctx_id = nxhal_rx_ctx->this_id;
	for (i = 0; i < cnt; i++) {
		nxhal_sds_ring = &nxhal_rx_ctx->sds_rings[i];
                host_sds_ring = (sds_host_ring_t *)nxhal_sds_ring->os_data;
		addr = nx_alloc(adapter,
				STATUS_DESC_RINGSIZE(adapter->MaxRxDescCount),
                                &nxhal_sds_ring->host_phys,
				&host_sds_ring->pci_dev);
		
		if (addr == NULL) {
			nx_nic_print3(adapter, "Status ring[%d] allocation "
				      "failed\n", i);
			goto error_done;
		}
		for (j = 0; j < adapter->max_rds_rings; j++) {
			INIT_LIST_HEAD(&host_sds_ring->free_rbufs[j].head);
		}

                nxhal_sds_ring->host_addr       = (I8 *)addr;
                host_sds_ring->adapter          = adapter;
                host_sds_ring->ring_idx         = i;
		host_sds_ring->consumer		= 0;
                nxhal_sds_ring->ring_size       = adapter->MaxRxDescCount;

#if defined(NEW_NAPI)
		host_sds_ring->netdev = adapter->netdev;
		SET_SDS_NETDEV_NAME(host_sds_ring,
				adapter->netdev->name, ctx_id, i);
#else
		if (i == 0 && ctx_id == 0) {
                        host_sds_ring->netdev = adapter->netdev;
		} else {
			netdev = alloc_netdev(0, adapter->netdev->name,
					      ether_setup);
			if (!netdev) {
				nx_nic_print3(adapter, "Netdev[%d] alloc "
					      "failed\n", i);
				goto error_done;
			}

                        netdev->priv = nxhal_sds_ring;
#ifdef UNM_NIC_NAPI
			netdev->weight = UNM_NETDEV_WEIGHT;
			netdev->poll = nx_nic_poll_sts;
#endif
			set_bit(__LINK_STATE_START, &netdev->state);

                        host_sds_ring->netdev = netdev;
			SET_SDS_NETDEV_NAME(host_sds_ring,
					adapter->netdev->name, ctx_id, i);
		}
#endif
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,8)) || defined(ESX_3X))
		if (adapter->flags & UNM_NIC_MSIX_ENABLED) {
			SET_SDS_NETDEV_IRQ(host_sds_ring,
					adapter->msix_entries[i + ctx_id].vector);

			nxhal_sds_ring->msi_index =
				adapter->msix_entries[i + ctx_id].entry;
		} else {
			SET_SDS_NETDEV_IRQ(host_sds_ring,
					   adapter->netdev->irq);
                }
#endif
	}
	return (0);

      error_done:
        nx_free_sts_rings(adapter,nxhal_rx_ctx);
	return (-ENOMEM);
	
}

/*
 *
 */
static int nx_register_irq(struct unm_adapter_s *adapter,
			   nx_host_rx_ctx_t *nxhal_host_rx_ctx)
{
	int i;
	int rv;
	int cnt;
        nx_host_sds_ring_t      *nxhal_sds_ring = NULL;
        sds_host_ring_t         *host_sds_ring = NULL;
	unsigned long flags = IRQF_SAMPLE_RANDOM;

	for (i = 0; i < nxhal_host_rx_ctx->num_sds_rings; i++) {
		nxhal_sds_ring = &nxhal_host_rx_ctx->sds_rings[i];
		host_sds_ring = (sds_host_ring_t *)nxhal_sds_ring->os_data;

		if (adapter->flags & UNM_NIC_MSIX_ENABLED) {
			adapter->intr_handler = nx_nic_msix_intr;
		} else if (adapter->flags & UNM_NIC_MSI_ENABLED)  {
			adapter->intr_handler = nx_nic_msi_intr;
		} else {
			flags |= IRQF_SHARED;
			adapter->intr_handler = nx_nic_legacy_intr;
		}

		rv = request_irq(GET_SDS_NETDEV_IRQ(host_sds_ring), 
				adapter->intr_handler, flags, 
				GET_SDS_NETDEV_NAME(host_sds_ring), 
				nxhal_sds_ring);

		if (rv) {
			nx_nic_print3(adapter, "%s Unable to register "
					"the interrupt service routine\n",
					GET_SDS_NETDEV_NAME(host_sds_ring));
			cnt = i;
			goto error_done;
		}
	}

	return (0);

error_done:
	for (i = 0; i < cnt; i++) {
                nxhal_sds_ring = &nxhal_host_rx_ctx->sds_rings[i];
                host_sds_ring = (sds_host_ring_t *)nxhal_sds_ring->os_data;
                free_irq(GET_SDS_NETDEV_IRQ(host_sds_ring) , nxhal_sds_ring);
	}

	return (rv);

}

/*
 *
 */
static int nx_unregister_irq(struct unm_adapter_s *adapter,
			     nx_host_rx_ctx_t *nxhal_host_rx_ctx)
{
	
	int i;
	nx_host_sds_ring_t      *nxhal_sds_ring = NULL;
	sds_host_ring_t         *host_sds_ring = NULL;


	for (i = 0; i < nxhal_host_rx_ctx->num_sds_rings; i++) {
		nxhal_sds_ring = &nxhal_host_rx_ctx->sds_rings[i];
		host_sds_ring = (sds_host_ring_t *)nxhal_sds_ring->os_data;
		if(host_sds_ring) {
			free_irq(GET_SDS_NETDEV_IRQ(host_sds_ring), nxhal_sds_ring);
		}
	}

	return (0);

}

/*
 * Utility to synchronize with receive peg.
 *  Returns   0 on sucess
 *         -EIO on error
 */
static int receive_peg_ready(struct unm_adapter_s *adapter)
{
	uint32_t state = 0;
	int loops = 0, err = 0;

	/* Window 1 call */
	read_lock(&adapter->adapter_lock);
	state = NXRD32(adapter, CRB_RCVPEG_STATE);
	read_unlock(&adapter->adapter_lock);

	while ((state != PHAN_PEG_RCV_INITIALIZED) && (loops < 20000)) {
		udelay(100);
		schedule();
		/* Window 1 call */

		read_lock(&adapter->adapter_lock);
		state = NXRD32(adapter, CRB_RCVPEG_STATE);
		read_unlock(&adapter->adapter_lock);

		loops++;
	}

	if (loops >= 20000) {
		nx_nic_print3(adapter, "Receive Peg initialization not "
			      "complete: 0x%x.\n", state);
		err = -EIO;
	}

	return err;
}

/*
 * check if the firmware has been downloaded and ready to run  and
 * setup the address for the descriptors in the adapter
 */
static int unm_nic_hw_resources(struct unm_adapter_s *adapter)
{
	struct _hardware_context *hw = &adapter->ahw;
	void *addr;
	int err = 0, ctx_id = -1;
#ifdef EPG_WORKAROUND
	void *pause_addr;
#endif

	/* Synchronize with Receive peg */
	err = receive_peg_ready(adapter);
	if (err) {
		return err;
	}

        nx_nic_print6(adapter, "Receive Peg ready too. starting stuff\n");

	/*
	 * Step 1: Allocate the TX software side of the ring.
	 */
	adapter->cmd_buf_arr = vmalloc(TX_RINGSIZE(adapter->MaxTxDescCount));

        if (adapter->cmd_buf_arr == NULL) {
		if (adapter->MaxTxDescCount > 1024) {
			nx_nic_print3(NULL, "Failed to allocate requested "
				      "memory for TX cmd buffer. "
				      "Setting MaxTxDescCount to 1024.\n");
			adapter->MaxTxDescCount = 1024;
			adapter->cmd_buf_arr =
				vmalloc(TX_RINGSIZE(adapter->MaxTxDescCount));
		}
		if (adapter->cmd_buf_arr == NULL) {
                	nx_nic_print3(NULL, "Failed to allocate memory for "
				      "the TX cmd buffer. "
				      "Check system memory resource usage.\n");
	                return (-ENOMEM);
		}
	}
	memset(adapter->cmd_buf_arr, 0, TX_RINGSIZE(adapter->MaxTxDescCount));

	/*
	 * Step 2: Allocate the TX HW descriptor ring.
	 */
	addr = nx_alloc(adapter,
			((sizeof(cmdDescType0_t) * adapter->MaxTxDescCount)
			 + sizeof(uint32_t)),
                        (dma_addr_t *)&hw->cmdDesc_physAddr,
			&adapter->ahw.cmdDesc_pdev);
        if (addr == NULL) {
                nx_nic_print3(adapter, "bad return from "
			      "pci_alloc_consistent\n");
                err = -ENOMEM;
                goto done;
        }

        adapter->cmdConsumer = (uint32_t *)(((char *) addr) +
					    (sizeof(cmdDescType0_t) *
					     adapter->MaxTxDescCount));
	/* changed right know */
	adapter->crb_addr_cmd_consumer =
		(((unsigned long)hw->cmdDesc_physAddr) +
		 (sizeof(cmdDescType0_t) * adapter->MaxTxDescCount));

        hw->cmdDescHead = (cmdDescType0_t *)addr;

        nx_nic_print6(adapter, "cmdDesc_physAddr: 0x%llx\n",
		      (U64)hw->cmdDesc_physAddr);

	/*
	 * Step 3: Allocate EPG workaround buffers.
	 */
#ifdef EPG_WORKAROUND
	pause_addr = nx_alloc(adapter, 512, (dma_addr_t *)&hw->pause_physAddr,
			      &hw->pause_pdev);

	if (pause_addr == NULL) {
                nx_nic_print3(adapter, "bad return from nx_alloc\n");
		err = -ENOMEM;
		goto done;
	}
	hw->pauseAddr = (char *)pause_addr;
	{
		uint64_t *ptr = (uint64_t *) pause_addr;
		*ptr++ = 0ULL;
		*ptr++ = 0ULL;
		*ptr++ = 0x200ULL;
		*ptr++ = 0x0ULL;
		*ptr++ = 0x2200010000c28001ULL;
		*ptr++ = 0x0100088866554433ULL;
	}
#endif

	/*
	 * Step 4: Allocate the license buffers.
	 */
        adapter->nx_lic_dma.addr =
                pci_alloc_consistent(adapter->ahw.pdev,
                                     sizeof(nx_finger_print_t),
                                     &adapter->nx_lic_dma.phys_addr);

        if (adapter->nx_lic_dma.addr == NULL) {
                nx_nic_print3(adapter, "NX_LIC_RD: bad return from "
                              "pci_alloc_consistent\n");
                err = -ENOMEM;
                goto done;
        }

	/*
	 * Step 5: Allocate the SNMP dma address.
	 */
#ifdef UNM_NIC_SNMP
	adapter->snmp_stats_dma.addr =
		pci_alloc_consistent(adapter->ahw.pdev,
				     sizeof(struct unm_nic_snmp_ether_stats),
				     &adapter->snmp_stats_dma.phys_addr);
	if (adapter->snmp_stats_dma.addr == NULL) {
		nx_nic_print3(adapter, "SNMP: bad return from "
			      "pci_alloc_consistent\n");
		err = -ENOMEM;
		goto done;
	}
#endif

#ifndef ESX
	/*
	 * Step 6: Now allocate space for network statistics.
	 */
	adapter->nic_net_stats.data = pci_alloc_consistent(adapter->pdev,
						sizeof (nx_ctx_stats_t),
						&adapter->nic_net_stats.phys);
	if (adapter->nic_net_stats.data == NULL) {
               	nx_nic_print3(NULL, "ERROR: Could not allocate space "
		      	      "for statistics\n");
               	return (-ENOMEM);
	}
#endif

	/*
	 * Need to check how many CPUs are available and use only that
	 * many rings.
	 */
        ctx_id = nx_nic_create_rx_ctx(adapter->netdev);
	if (ctx_id < 0) {
		err = -ENOMEM;
		goto done;
	}
        adapter->nx_dev->rx_ctxs_state |= (1 << ctx_id);
        /* TODO: Walk thru tear down sequence. */
	if (!adapter->fw_v34) {
                if (nx_nic_get_pexq_cap(adapter->netdev)) {
	                if ((err = nx_pexq_allocate_memory(adapter))) {
                                goto done;
                        }
                }
		if ((err = unm_nic_new_tx_context_prepare(adapter))) {
			unm_nic_new_rx_context_destroy(adapter, NX_DESTROY_CTX_RESET);
			goto done;
		}
                if (nx_nic_get_pexq_cap(adapter->netdev)) {
	                if ((err = nx_init_pexq_dbell(adapter))) {
                                goto done;
                        }
                }
	}
done:
	if (err) {
		unm_nic_free_ring_context(adapter);
	}
	return err;
}


static int unm_nic_new_rx_context_prepare(struct unm_adapter_s *adapter,
					  nx_host_rx_ctx_t *rx_ctx)
{
	nx_host_sds_ring_t *sds_ring = NULL;
	nx_host_rds_ring_t *rds_ring = NULL;
	int retval = 0;
	int i = 0;
	int nsds_rings = rx_ctx->num_sds_rings;
	int nrds_rings = adapter->max_rds_rings;
	struct nx_dma_alloc_s hostrq;
	struct nx_dma_alloc_s hostrsp;

	//rx_ctx->chaining_allowed = 1;

	if (nx_fw_cmd_create_rx_ctx_alloc_dma(adapter->nx_dev, nrds_rings,
					      nsds_rings, &hostrq,
					      &hostrsp) == NX_RCODE_SUCCESS) {

		rx_ctx->chaining_allowed = rx_chained;
#ifdef ESX
		rx_ctx->multi_context = adapter->multi_ctx;
#endif

		if (adapter->fw_v34) {
			unm_nic_v34_context_prepare(adapter);
			retval = nx_fw_cmd_v34_create_ctx(adapter->nx_dev->rx_ctxs[0],
					adapter->nx_dev->tx_ctxs[0], &hostrq);
			adapter->ctx_add.ptr    = hostrq.ptr;
			adapter->ctx_add.phys   = hostrq.phys;
			adapter->ctx_add.size   = hostrq.size;
			pci_free_consistent(adapter->pdev, hostrsp.size, hostrsp.ptr, hostrsp.phys);						
		} else {
			retval = nx_fw_cmd_create_rx_ctx(rx_ctx, &hostrq, &hostrsp);
			nx_fw_cmd_create_rx_ctx_free_dma(adapter->nx_dev,
					&hostrq, &hostrsp);
		}
	}

	if (retval != NX_RCODE_SUCCESS) {
		nx_nic_print3(adapter, "Unable to create the "
			      "rx context, code %d %s\n",
			      retval, nx_errorcode2string(retval));
		goto failure_rx_ctx;
	}

	if (!adapter->fw_v34) {
		rds_ring = rx_ctx->rds_rings;
		for (i = 0; i < nrds_rings; i++) {
			rds_ring[i].host_rx_producer =
		    		UNM_NIC_REG(rds_ring[i].host_rx_producer - 0x0200);
		}

		sds_ring = rx_ctx->sds_rings;
		for (i = 0; i < nsds_rings; i++) {
			uint32_t reg = 0;
			reg = UNM_NIC_REG(sds_ring[i].host_sds_consumer - 0x200);
                	sds_ring[i].host_sds_consumer = reg;
			reg = UNM_NIC_REG(sds_ring[i].interrupt_crb - 0x200);
                	sds_ring[i].interrupt_crb =  reg;
		}
	}

  failure_rx_ctx:
	return retval;
}

static void unm_nic_v34_context_prepare(struct unm_adapter_s *adapter)
{
	int retval = 0;
	int ncds_ring = 1;
	nx_host_tx_ctx_t *tx_ctx;

	UNM_NIC_PCI_WRITE_32(INTR_SCHEME_PERPORT,
			     CRB_NORMALIZE(adapter, CRB_NIC_CAPABILITIES_HOST));
	UNM_NIC_PCI_WRITE_32(MSI_MODE_MULTIFUNC,
			     CRB_NORMALIZE(adapter, CRB_NIC_MSI_MODE_HOST));

	retval = nx_fw_cmd_create_tx_ctx_alloc(adapter->nx_dev,
					       ncds_ring,
					       &tx_ctx);
	if (retval) {
		nx_nic_print4(adapter, "Could not allocate memory "
			      "for tx context\n");
		return;
	}

#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,8)) || defined(ESX_3X))
	tx_ctx->msi_index = 0;
#endif

	tx_ctx->cmd_cons_dma_addr = adapter->crb_addr_cmd_consumer;
	tx_ctx->dummy_dma_addr = adapter->dummy_dma.phys_addr;

	tx_ctx->cds_ring[0].card_tx_consumer = NULL;
	tx_ctx->cds_ring[0].host_addr = NULL;
	tx_ctx->cds_ring[0].host_phys = adapter->ahw.cmdDesc_physAddr;
	tx_ctx->cds_ring[0].ring_size = adapter->MaxTxDescCount;

	adapter->crb_addr_cmd_producer = crb_cmd_producer[adapter->portnum];
	unm_nic_update_cmd_producer(adapter, 0);
}


static int unm_nic_new_rx_context_destroy(struct unm_adapter_s *adapter, int ctx_destroy)
{
	nx_host_rx_ctx_t *rx_ctx;
	int retval = 0;
	int i;


	if (adapter->nx_dev == NULL) {
		nx_nic_print3(adapter, "nx_dev is NULL\n");
		return 1;
	}

        for (i = 0; i < adapter->nx_dev->alloc_rx_ctxs; i++) {
           if (adapter->nx_dev->rx_ctxs[i] != NULL) {

                       spin_lock(&adapter->ctx_state_lock);
                       if((adapter->nx_dev->rx_ctxs_state & (1 << i)) != 0) { 
                               spin_unlock(&adapter->ctx_state_lock);
                       } else {
                               spin_unlock(&adapter->ctx_state_lock);
                               continue;
                       }


                  rx_ctx = adapter->nx_dev->rx_ctxs[i];
                  retval = nx_fw_cmd_destroy_rx_ctx(rx_ctx, ctx_destroy);

                  if (retval) { 
                        nx_nic_print4(adapter, "Unable to destroy the "
                              "rx context, code %d %s\n", retval,
                              nx_errorcode2string(retval));
                  }
           }

	}

	return 0;
}

static int unm_nic_new_tx_context_prepare(struct unm_adapter_s *adapter)
{
	nx_host_tx_ctx_t *tx_ctx = NULL;
	int retval = 0;
	int ncds_ring = 1;
	struct nx_dma_alloc_s hostrq;
	struct nx_dma_alloc_s hostrsp;

	retval = nx_fw_cmd_create_tx_ctx_alloc(adapter->nx_dev,
					       ncds_ring,
					       &tx_ctx);
        if (retval) {
                nx_nic_print4(adapter, "Could not allocate memory "
			      "for tx context\n");
                goto failure_tx_ctx;
        }
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,8)) || defined(ESX_3X))
	tx_ctx->msi_index = 0;
#endif
#if 0
	tx_ctx->interrupt_ctl = 0;
	tx_ctx->msi_index = 0;
#endif
	tx_ctx->cmd_cons_dma_addr = adapter->crb_addr_cmd_consumer;
	tx_ctx->dummy_dma_addr = adapter->dummy_dma.phys_addr;

	tx_ctx->cds_ring[0].card_tx_consumer = NULL;
	tx_ctx->cds_ring[0].host_addr = NULL;
	tx_ctx->cds_ring[0].host_phys = adapter->ahw.cmdDesc_physAddr;
	tx_ctx->cds_ring[0].ring_size = adapter->MaxTxDescCount;

        if (nx_nic_get_pexq_cap(adapter->netdev)) {
                tx_ctx->pexq_req.host_pexq_ring_address = 
                    adapter->pexq.qbuf_paddr;
                tx_ctx->use_pexq = nx_nic_get_pexq_cap(adapter->netdev); 
        }
	
	if (nx_fw_cmd_create_tx_ctx_alloc_dma(adapter->nx_dev, ncds_ring,
					      &hostrq, &hostrsp) ==
	    NX_RCODE_SUCCESS) {

		retval = nx_fw_cmd_create_tx_ctx(tx_ctx, &hostrq, &hostrsp);

		nx_fw_cmd_create_tx_ctx_free_dma(adapter->nx_dev,
						 &hostrq, &hostrsp);
	}

	if (retval != NX_RCODE_SUCCESS) {
		nx_nic_print4(adapter, "Unable to create the tx "
			      "context, code %d %s\n", retval,
			      nx_errorcode2string(retval));
		nx_fw_cmd_create_tx_ctx_free(adapter->nx_dev, tx_ctx);
		goto failure_tx_ctx;
	}

        if (nx_nic_get_pexq_cap(adapter->netdev)) {
                adapter->pexq.db_number = tx_ctx->pexq_rsp.pexq_dbell_number; 
                adapter->pexq.xdma_hdr.word = tx_ctx->pexq_rsp.pexq_card_fc_q; 
                adapter->pexq.card_fc_array = 
                    tx_ctx->pexq_rsp.pexq_card_fc_address; 
                adapter->pexq.reflection_offset = 
                    tx_ctx->pexq_rsp.pexq_reflection_offset; 
        }
	adapter->crb_addr_cmd_producer =
		UNM_NIC_REG(tx_ctx->cds_ring->host_tx_producer - 0x200);
	
 failure_tx_ctx:
	return retval;
}

static int unm_nic_new_tx_context_destroy(struct unm_adapter_s *adapter, int ctx_destroy)
{
	nx_host_tx_ctx_t *tx_ctx;
	int retval = 0;
	int i;

	if (adapter->nx_dev == NULL) {
		nx_nic_print4(adapter, "nx_dev is NULL\n");
		return 1;
	}

	for (i = 0; i < adapter->nx_dev->alloc_tx_ctxs; i++) {

		tx_ctx = adapter->nx_dev->tx_ctxs[i];
		if (tx_ctx == NULL) {
			continue;
		}

		retval = nx_fw_cmd_destroy_tx_ctx(tx_ctx, ctx_destroy);

		if (retval) {
			nx_nic_print4(adapter, "Unable to destroy the "
				      "tx context, code %d %s\n", retval,
				      nx_errorcode2string(retval));
		}

		nx_fw_cmd_create_tx_ctx_free(adapter->nx_dev, tx_ctx);
	}

	return 0;
}

void nx_free_tx_resources(struct unm_adapter_s *adapter)
{
	int i, j;
	struct unm_cmd_buffer *cmd_buff;
	struct unm_skb_frag *buffrag;

	cmd_buff = adapter->cmd_buf_arr;
	if (cmd_buff == NULL) {
		nx_nic_print3(adapter, "cmd_buff == NULL\n");
		return;
	}
	for (i = 0; i < adapter->MaxTxDescCount; i++) {
		buffrag = cmd_buff->fragArray;
		if (buffrag->dma) {
			pci_unmap_single(adapter->pdev, buffrag->dma,
					 buffrag->length, PCI_DMA_TODEVICE);
			buffrag->dma = (uint64_t) 0;
		}
		for (j = 0; j < cmd_buff->fragCount; j++) {
			buffrag++;
			if (buffrag->dma) {
				pci_unmap_page(adapter->pdev, buffrag->dma,
						buffrag->length,
						PCI_DMA_TODEVICE);
				buffrag->dma = (uint64_t) 0;
			}
		}
		/* Free the skb we received in unm_nic_xmit_frame */
		if (cmd_buff->skb) {
			dev_kfree_skb_any(cmd_buff->skb);
			cmd_buff->skb = NULL;
		}
		cmd_buff++;
	}
}

/*
 * This will be called when all the ports of the adapter are removed.
 * This will cleanup and disable interrupts and irq.
 */
static void unm_nic_down(struct net_device *netdev)
{
	struct unm_adapter_s *adapter = netdev_priv(netdev);
	int index = 0;
	unsigned long flags;
	flags = 0;
	netif_carrier_off(netdev);
	netif_stop_queue(netdev);
	smp_mb();

	if (!NX_IS_LSA_REGISTERED(adapter)) {

		for (index = 0; index < adapter->nx_dev->alloc_rx_ctxs;
		     index++) {
			if (adapter->nx_dev->rx_ctxs[index] != NULL) {
				spin_lock(&adapter->ctx_state_lock);
				if((adapter->nx_dev->rx_ctxs_state & (1 << index)) == 0) {
					spin_unlock(&adapter->ctx_state_lock);
					continue;
				}
				spin_unlock(&adapter->ctx_state_lock);
				read_lock(&adapter->adapter_lock);
				unm_nic_disable_all_int(adapter,
					adapter->nx_dev->rx_ctxs[index]);
				read_unlock(&adapter->adapter_lock);
			}
		}

		if (adapter->is_up == ADAPTER_UP_MAGIC || adapter->is_up == FW_RESETTING)
			nx_napi_disable(adapter);
	}

	if (NX_IS_REVISION_P2(adapter->ahw.revision_id)) {
		nx_nic_p2_stop_port(adapter);
	}
	
	if(adapter->is_up == ADAPTER_UP_MAGIC)
		FLUSH_SCHEDULED_WORK();
	del_timer_sync(&adapter->watchdog_timer);

	TX_LOCK(&adapter->tx_lock, flags);
	nx_free_tx_resources(adapter);
	TX_UNLOCK(&adapter->tx_lock, flags);
}

/**
 * unm_nic_close - Disables a network interface entry point
 **/
static int unm_nic_close(struct net_device *netdev)
{
	struct unm_adapter_s *adapter = netdev_priv(netdev);

	/* wait here for poll to complete */
	//netif_poll_disable(netdev);
	unm_nic_down(netdev);

	adapter->state = PORT_DOWN;
 
	return 0;
}

static int unm_nic_set_mac(struct net_device *netdev, void *p)
{
	struct unm_adapter_s *adapter = netdev_priv(netdev);
	struct sockaddr *addr = p;
	int ret;

	if (netif_running(netdev))
		return -EBUSY;

	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;

	ret = unm_nic_macaddr_set(adapter, addr->sa_data);
	if (!ret) {
		memcpy(netdev->dev_addr, addr->sa_data, netdev->addr_len);
		memcpy(adapter->mac_addr, addr->sa_data, netdev->addr_len);
	}

	return ret;
}

static int nx_nic_p3_add_mac(struct unm_adapter_s *adapter, __u8 * addr,
			     mac_list_t ** add_list, mac_list_t ** del_list)
{
	mac_list_t *cur, *prev;

	/* if in del_list, move it to adapter->mac_list */
	for (cur = *del_list, prev = NULL; cur;) {
		if (memcmp(addr, cur->mac_addr, ETH_ALEN) == 0) {
			if (prev == NULL) {
				*del_list = cur->next;
			} else {
				prev->next = cur->next;
			}
			cur->next = adapter->mac_list;
			adapter->mac_list = cur;
			return 0;
		}
		prev = cur;
		cur = cur->next;
	}

	/* make sure to add each mac address only once */
	for (cur = adapter->mac_list; cur; cur = cur->next) {
		if (memcmp(addr, cur->mac_addr, ETH_ALEN) == 0) {
			return 0;
		}
	}
	/* not in del_list, create new entry and add to add_list */
	cur = kmalloc(sizeof(*cur), in_atomic()? GFP_ATOMIC : GFP_KERNEL);
	if (cur == NULL) {
		nx_nic_print3(adapter, "cannot allocate memory. MAC "
			      "filtering may not work properly from now.\n");
		return -1;
	}

	memcpy(cur->mac_addr, addr, ETH_ALEN);
	cur->next = *add_list;
	*add_list = cur;
	return 0;
}

static void nx_nic_p3_set_multi(struct net_device *netdev)
{
	struct unm_adapter_s *adapter = netdev_priv(netdev);
	mac_list_t *cur, *next, *del_list, *add_list = NULL;
	struct dev_mc_list *mc_ptr;
	__u8 bcast_addr[ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	__uint32_t mode = VPORT_MISS_MODE_DROP;

	del_list = adapter->mac_list;
	adapter->mac_list = NULL;

	/*
	 * Always add broadcast & unicast mac addresses.
	 */
	nx_nic_p3_add_mac(adapter, adapter->mac_addr, &add_list, &del_list);
	nx_nic_p3_add_mac(adapter, bcast_addr, &add_list, &del_list);

	/*
	 * Check and turn on Promiscous mode or ALL multicast mode.
	 */
	adapter->promisc = (netdev->flags & (IFF_PROMISC | IFF_ALLMULTI));

	if (netdev->flags & IFF_PROMISC) {
		mode = VPORT_MISS_MODE_ACCEPT_ALL;
		goto send_fw_cmd;
	}

	if ((netdev->flags & IFF_ALLMULTI) ||
	    netdev->mc_count > adapter->max_mc_count) {
		mode = VPORT_MISS_MODE_ACCEPT_MULTI;
		goto send_fw_cmd;
	}

	if (netdev->mc_count > 0) {
		for (mc_ptr = netdev->mc_list; mc_ptr; mc_ptr = mc_ptr->next) {

			nx_nic_p3_add_mac(adapter, mc_ptr->dmi_addr,
					  &add_list, &del_list);
		}
	}

  send_fw_cmd:
	nx_p3_set_vport_miss_mode(adapter, mode);

	for (cur = del_list; cur;) {
		nx_os_pf_remove_l2_mac(adapter->nx_dev,
				       adapter->nx_dev->rx_ctxs[0],
				       cur->mac_addr);
		next = cur->next;
		kfree(cur);
		cur = next;
	}
	for (cur = add_list; cur;) {
		nx_os_pf_add_l2_mac(adapter->nx_dev,
				    adapter->nx_dev->rx_ctxs[0],
				    cur->mac_addr);
		next = cur->next;
		cur->next = adapter->mac_list;
		adapter->mac_list = cur;
		cur = next;
	}
}

static void nx_nic_p2_set_multi(struct net_device *netdev)
{
	struct unm_adapter_s *adapter = netdev_priv(netdev);
	struct dev_mc_list *mc_ptr;
	__u8 null_addr[6] = { 0, 0, 0, 0, 0, 0 };
	int index = 0;

	if (netdev->flags & IFF_PROMISC ||
	    netdev->mc_count > adapter->max_mc_count) {

		unm_nic_set_promisc_mode(adapter);

		/* Full promiscuous mode */
		unm_nic_disable_mcast_filter(adapter);

		return;
	}

	if (netdev->mc_count == 0) {
		unm_nic_unset_promisc_mode(adapter);
		unm_nic_disable_mcast_filter(adapter);
		return;
	}

	unm_nic_set_promisc_mode(adapter);
	unm_nic_enable_mcast_filter(adapter);

	for (mc_ptr = netdev->mc_list; mc_ptr; mc_ptr = mc_ptr->next, index++)
		unm_nic_set_mcast_addr(adapter, index, mc_ptr->dmi_addr);

	if (index != netdev->mc_count) {
		nx_nic_print4(adapter, "Multicast address count mismatch\n");
	}

	/* Clear out remaining addresses */
	for (; index < adapter->max_mc_count; index++) {
		unm_nic_set_mcast_addr(adapter, index, null_addr);
	}
}

static inline int not_aligned(unsigned long addr)
{
	return (addr & 0x7);
}


static inline int is_packet_tagged(struct sk_buff *skb)
{
	struct vlan_ethhdr *veth = (struct vlan_ethhdr *)skb->data;

	if (veth->h_vlan_proto == __constant_htons(ETH_P_8021Q)) {
		return 1;
	}
	return 0;
}       

static inline void nx_nic_check_tso(struct unm_adapter_s *adapter,
		cmdDescType0_t * desc, uint32_t tagged, struct sk_buff *skb)
{
	if (desc->mss) {
		desc->totalHdrLength = sizeof(struct ethhdr) +
			(tagged * sizeof(struct vlan_hdr)) +
			NW_HDR_LEN(skb, tagged) +
			(TCP_HDR(skb)->doff * sizeof(u32));

		if (PROTO_IS_IP(skb, tagged)) {
			desc->opcode = TX_TCP_LSO;
		} else if (PROTO_IS_IPV6(skb, tagged)) {
			desc->opcode = TX_TCP_LSO6;
		}
	} else if (skb->ip_summed == CHECKSUM_HW) {
		if (PROTO_IS_IP(skb, tagged)) { /* IPv4  */
			if (PROTO_IS_TCP(skb, tagged)) {
				desc->opcode = TX_TCP_PKT;
			} else if (PROTO_IS_UDP(skb, tagged)) {
				desc->opcode = TX_UDP_PKT;
			} else {
				return;
			}
		} else if (PROTO_IS_IPV6(skb, tagged)) {        /* IPv6 */
			if (PROTO_IS_TCPV6(skb, tagged)) {
				desc->opcode = TX_TCPV6_PKT;
			} else if (PROTO_IS_UDPV6(skb, tagged)) {
				desc->opcode = TX_UDPV6_PKT;
			} else {
				return;
			}
		}
	}

	desc->tcpHdrOffset = TCP_HDR_OFFSET(skb);
	desc->ipHdrOffset = NW_HDR_OFFSET(skb, tagged); // tochange
}

static inline void nx_nic_tso_copy_headers(struct unm_adapter_s *adapter, 
		struct sk_buff *skb, uint32_t *prod, uint32_t saved)
{
	int hdr_len, first_hdr_len, more_hdr;
	cmdDescType0_t *hwdesc = &adapter->ahw.cmdDescHead[0];
	int vid = -1;

#ifdef NX_VLAN_ACCEL
	if (vlan_tx_tag_present(skb)) {
		u16	*ptr;
		ptr = (u16 *)(&hwdesc[saved].mcastAddr) + 3;
		hwdesc[saved].flags |= FLAGS_VLAN_OOB;
		vid = *ptr = vlan_tx_tag_get(skb);
	}
#endif /* NX_VLAN_ACCEL */

	if ((hwdesc[saved].opcode == TX_TCP_LSO) ||
			(hwdesc[saved].opcode == TX_TCP_LSO6)) {

		if (vid != -1) {
			hwdesc[saved].totalHdrLength += VLAN_HLEN;
			hwdesc[saved].tcpHdrOffset += VLAN_HLEN;
			hwdesc[saved].ipHdrOffset += VLAN_HLEN;
			hwdesc[saved].flags |= FLAGS_VLAN_TAGGED;
		}

		hdr_len = hwdesc[saved].totalHdrLength;

		if (hdr_len > (sizeof(cmdDescType0_t) - IP_ALIGNMENT_BYTES)) {
			first_hdr_len = sizeof(cmdDescType0_t) -
				IP_ALIGNMENT_BYTES;
			more_hdr = 1;
		} else {
			first_hdr_len = hdr_len;
			more_hdr = 0;
		}

		/* copy the first 64 bytes */
		if (vid != -1) {
			struct vlan_ethhdr *veth;
			u32 *dst, *src;

			/*
			 * Quick implementation of
			 * a. memcpy((void *)veth, skb->data, 12);
			 * b. 8012q tag update.
			 */
			dst = (u32 *)(&hwdesc[*prod]);
			veth = (struct vlan_ethhdr *)((char *)dst +
			    IP_ALIGNMENT_BYTES);
			src = (u32 *)(skb->data - IP_ALIGNMENT_BYTES);
			*dst++ = *src++;
			*dst++ = *src++;
			*dst++ = *src++;
			*dst = *src;
			veth->h_vlan_proto = htons(ETH_P_8021Q);
			veth->h_vlan_TCI = htons((u16)vid);
			memcpy((void *)((char *)veth + 16), skb->data + 12,
			    first_hdr_len - 12);
		} else {
			memcpy((void*)(&hwdesc[*prod]) + IP_ALIGNMENT_BYTES,
				skb->data, first_hdr_len);
		}

		*prod = get_next_index(*prod, adapter->MaxTxDescCount);

		if (more_hdr) {
			char    *skbsrc = skb->data + first_hdr_len;

			if (vid != -1)
				skbsrc -= VLAN_HLEN;

			/* copy the next 64 bytes - should be enough except
			 * for pathological case
			 */
			memcpy((void *)(&hwdesc[*prod]), (void *)skbsrc,
					hdr_len - first_hdr_len);

			*prod = get_next_index(*prod, adapter->MaxTxDescCount);
		}
	}
}

static void
netxen_clean_tx_dma_mapping(struct unm_adapter_s *adapter,
		struct unm_cmd_buffer *pbuf, int last)
{
	int k;
	struct unm_skb_frag *buffrag;
	struct pci_dev *pdev = adapter->pdev;

	buffrag = &pbuf->fragArray[0];
	pci_unmap_single(pdev, buffrag->dma,
			buffrag->length, PCI_DMA_TODEVICE);
	for (k = 1; k < last; k++) {
		buffrag = &pbuf->fragArray[k];
		pci_unmap_page(pdev, buffrag->dma,
				buffrag->length, PCI_DMA_TODEVICE);
	}
}

/* 
	This function will check the validity(Non corruptness) of the packet 
	to be transmitted.

	It will return -1 when we want the packet to be dropped.
	Else will return 0.
*/
#ifdef ESX
static int
validate_tso(struct net_device *netdev, struct sk_buff *skb, unsigned int tagged) 
{
	if (TSO_ENABLED(netdev)) {
		if (TSO_SIZE(skb_shinfo(skb)) > 0) {
			struct tcphdr *tmp_th = NULL;
			U64 tcpHdrOffset = TCP_HDR_OFFSET(skb) ;
			U64 tcp_opt_len = 0;
			U64 tcphdr_size  = 0;
			U64 tcp_offset  = 0;
			U64 totalHdrLength = 0;

			tmp_th = (struct tcphdr *) (SKB_MAC_HDR(skb) + tcpHdrOffset);

			/* Calculate the TCP header size */
			tcp_opt_len = ((tmp_th->doff - 5) * 4);
			tcphdr_size = sizeof(struct tcphdr) + tcp_opt_len;
			tcp_offset = tcpHdrOffset + tcphdr_size;

			totalHdrLength = sizeof(struct ethhdr) +
				(tagged * sizeof(struct vlan_hdr)) +
				 (NW_HDR_SIZE(skb)) +
				(TCP_HDR(skb)->doff * sizeof(u32));

			if(rarely(tmp_th->doff < 5)) {
				printk("%s: %s: Dropping packet, Illegal TCP header size in skb %d \n",
						unm_nic_driver_name, netdev->name, tmp_th->doff);
				return -1;
			}
			if (rarely(tcphdr_size > MAX_TCP_HDR)) {
				printk("%s: %s: Dropping packet, Too much TCP header %lld > %d\n",
						unm_nic_driver_name, netdev->name, tcphdr_size, MAX_TCP_HDR);
				return -1;
			}

			if (rarely(tcp_offset != totalHdrLength)) {
				printk("%s: %s: Dropping packet, Detected tcp_offset != totalHdrLength: %lld, %lld\n",
						unm_nic_driver_name, netdev->name, tcp_offset, totalHdrLength);
				return -1;
			}
		}
	}
	return 0;
}

static void esx_mcast_egress(struct unm_adapter_s *adapter,
    cmdDescType0_t *hwdesc, __uint64_t *uaddrp, struct sk_buff *skb)
{
	struct ethhdr   *phdr = (struct ethhdr *)(skb->data);
	int		i;

	memcpy(uaddrp, phdr->h_source, ETH_ALEN);
	memcpy(&hwdesc->mcastAddr, phdr->h_dest, ETH_ALEN);
	hwdesc->flags |= FLAGS_MCAST;

	/*
	 * Check whether we have seen this local address before.
	 * Update LRU count if so.
	 */
	for (i = 0; i < adapter->fnum; i++) {
		if (adapter->faddr[i] == *uaddrp) {
			adapter->ltime[i] = jiffies;
			*uaddrp = 0;
			break;
		}
	}
}

/*
 * Add extra Tx desc command to update card filter register, based on snooping
 * RARP packets. Extra descriptor was accounted for.
 */
static void esx_send_filter(struct unm_adapter_s *adapter, __uint64_t uaddr,
    uint32_t *pproducer)
{
	struct _hardware_context	*hw = &adapter->ahw;
	cmdDescType0_t			*hwdesc = &hw->cmdDescHead[*pproducer];
	int				i, k;

	memset(hwdesc, 0, sizeof(cmdDescType0_t));
	hwdesc->opcode = NX_FILTER;
	hwdesc->mcastAddr = uaddr;

	/*
	 * Assign free entry or replace LRU.
	 */
	if (adapter->fnum < adapter->fmax) {
		k = adapter->fnum;
		adapter->fnum++;
		hwdesc->buffer2Length = FILTER_ADD;
	} else {
		unsigned long	rtime = adapter->ltime[0];
		for (i = 0, k = 0; i < adapter->fmax; i++) {
			if (time_before(adapter->ltime[i], rtime)) {
				rtime = adapter->ltime[i];
				k = i;
			}
		}
		hwdesc->buffer2Length = FILTER_REPLACE;
	}
	adapter->ltime[k] = jiffies;
	adapter->faddr[k] = uaddr;
	hwdesc->buffer1Length = adapter->fstart + k;
	*pproducer = get_next_index(*pproducer, adapter->MaxTxDescCount);
}
#endif /* ESX */

int unm_nic_xmit_frame(struct sk_buff *skb, struct net_device *netdev)
{
	unsigned int nr_frags = 0, vlan_frag = 0;
	struct unm_adapter_s *adapter = netdev_priv(netdev);
	struct _hardware_context *hw = &adapter->ahw;
	unsigned int firstSegLen;
	struct unm_skb_frag *buffrag;
	unsigned int i;

	uint32_t producer = 0;
	uint32_t saved_producer = 0;
	cmdDescType0_t *hwdesc;
	int k;
	struct unm_cmd_buffer *pbuf = NULL;
	int fragCount;
	uint32_t MaxTxDescCount = 0;
	uint32_t lastCmdConsumer = 0;
	int no_of_desc;
#ifndef UNM_NIC_NAPI
	unsigned long flags = 0;
#endif
	unsigned int tagged = 0;
	struct nx_vlan_buffer *vlan_buf;
	unsigned int vlan_copy = 0;
#ifdef ESX
	__uint64_t	uaddr = 0;
#endif

	if (unlikely(skb->len <= 0)) {
		dev_kfree_skb_any(skb);
		adapter->stats.badskblen++;
		return 0;
	}

	nr_frags = skb_shinfo(skb)->nr_frags;
	fragCount = 1 + nr_frags;
#ifdef NX_VLAN_ACCEL
	if (!(netdev->features & NETIF_F_HW_VLAN_TX))
#endif /* NX_VLAN_ACCEL */
		tagged = is_packet_tagged(skb);

	if (fragCount > (NX_NIC_MAX_SG_PER_TX - tagged)) {

		int i;
		int delta = 0;
		struct skb_frag_struct *frag;

		for (i = 0; i < (fragCount - (NX_NIC_MAX_SG_PER_TX - tagged));
		     i++) {
			frag = &skb_shinfo(skb)->frags[i];
			delta += frag->size;
		}

		if (!__pskb_pull_tail(skb, delta)) {
			goto drop_packet;
		}

		fragCount = 1 + skb_shinfo(skb)->nr_frags;
	}
 	
#if defined (__VMKLNX__)
#if VC_1ST_BUFFER_18_BYTE_WORKAROUND
	if(adapter->ahw.boardcfg.board_type == UNM_BRDTYPE_P3_HMEZ &&
		(skb->len - skb->data_len < VC_1ST_BUFFER_REQUIRED_LENGTH)) {
		if(skb->len < VC_1ST_BUFFER_REQUIRED_LENGTH) {
			goto drop_packet;
		}

		if(!__pskb_pull_tail(skb, VC_1ST_BUFFER_REQUIRED_LENGTH - (skb->len - skb->data_len))) {
			goto drop_packet;
		}
		fragCount = 1 + skb_shinfo(skb)->nr_frags;
	}
#endif /* VC_1ST_BUFFER_18_BYTE_WORKAROUND */
#endif /* defined (__VMKLNX__) */

	firstSegLen = skb->len - skb->data_len;

#ifdef ESX
	if(NX_IS_REVISION_P2(adapter->ahw.revision_id) && tagged) {
		if (skb->ip_summed && not_aligned((unsigned long)(skb->data))) {
			if (firstSegLen > HDR_CP) {
				vlan_frag = 1;
				fragCount++;
			}
			vlan_copy = 1;
		}
	}
#endif

	/*
	 * Everything is set up. Now, we just need to transmit it out.
	 * Note that we have to copy the contents of buffer over to
	 * right place. Later on, this can be optimized out by de-coupling the
	 * producer index from the buffer index.
	 */
	TX_LOCK(&adapter->tx_lock, flags);
	if (!netif_carrier_ok(netdev))
		goto drop_packet;	
	producer = adapter->cmdProducer;
	/* There 4 fragments per descriptor */
	no_of_desc = (fragCount + 3) >> 2;

#ifdef ESX
	if (validate_tso(adapter->netdev, skb, tagged)) {
		goto drop_packet;
	}
#endif

	k = adapter->cmdProducer;
	MaxTxDescCount = adapter->MaxTxDescCount;
	smp_mb();
	lastCmdConsumer = adapter->lastCmdConsumer;
	if ((k + no_of_desc + 2) >=
	    ((lastCmdConsumer <= k) ? lastCmdConsumer + MaxTxDescCount :
	     lastCmdConsumer)) {
		goto requeue_packet;
	}

	/* Copy the descriptors into the hardware    */
	saved_producer = producer;
	hwdesc = &hw->cmdDescHead[producer];
	memset(hwdesc, 0, sizeof(cmdDescType0_t));

#ifdef ESX
	if ((adapter->fmax) && (*(unsigned char *)(skb->data) & 0x01))
		esx_mcast_egress(adapter, hwdesc, &uaddr, skb);
#endif /* ESX */

	/* Take skb->data itself */
	pbuf = &adapter->cmd_buf_arr[producer];

#ifdef NETIF_F_TSO 
	if ((TSO_SIZE(skb_shinfo(skb)) > 0) && TSO_ENABLED(netdev)) {
		pbuf->mss = TSO_SIZE(skb_shinfo(skb));
		hwdesc->mss = TSO_SIZE(skb_shinfo(skb));
	} else  
#endif
	{	pbuf->mss = 0;
		hwdesc->mss = 0;
	}

	pbuf->totalLength = skb->len;
	pbuf->skb = skb;
	pbuf->cmd = TX_ETHER_PKT;
	pbuf->fragCount = fragCount;
	pbuf->port = adapter->portnum;
	buffrag = &pbuf->fragArray[0];

	if (!vlan_copy) {
		if (try_map_skb_data(adapter, skb,
				firstSegLen, PCI_DMA_TODEVICE,
				(dma_addr_t *) &buffrag->dma)) {
			goto drop_packet;
		}
	} else {
		vlan_buf = &(pbuf->vlan_buf);
		if (vlan_frag) {
			memcpy(vlan_buf->data, skb->data, HDR_CP);
			buffrag->dma = vlan_buf->phys;

			hwdesc->buffer2Length = firstSegLen - HDR_CP;
			hwdesc->AddrBuffer2 = virt_to_phys(skb->data + HDR_CP);

			buffrag++;
			buffrag->length = hwdesc->buffer2Length;
			buffrag->dma = hwdesc->AddrBuffer2;

			if (adapter->bounce) {
				int len[MAX_PAGES_PER_FRAG] = {0};
				void *vaddr[MAX_PAGES_PER_FRAG] = {NULL};

				vaddr[0] = (void *) (skb->data + HDR_CP);
				len[0] = hwdesc->buffer2Length;

				if(nx_handle_large_addr(adapter, buffrag,
							&hwdesc->AddrBuffer2,
							vaddr, len, len[0])){

						adapter->stats.txOutOfBounceBufDropped++;
						goto drop_packet;
				}

			}
			firstSegLen = HDR_CP;
		} else {
			memcpy(vlan_buf->data, skb->data, firstSegLen);
			buffrag->dma = vlan_buf->phys;
		}
	}

	pbuf->fragArray[0].length = firstSegLen;
	hwdesc->totalLength = skb->len;
	hwdesc->numOfBuffers = fragCount;
	hwdesc->opcode = TX_ETHER_PKT;

        hwdesc->port = adapter->portnum; /* JAMBU: To be removed */

	hwdesc->ctx_id = adapter->portnum;
	hwdesc->buffer1Length = firstSegLen;
	hwdesc->AddrBuffer1 = pbuf->fragArray[0].dma;

#ifdef ESX
	if (adapter->bounce) {
		int len[MAX_PAGES_PER_FRAG] = {0};
		void *vaddr[MAX_PAGES_PER_FRAG] = {NULL};

		vaddr[0] = (void *) (skb->data);
		len[0] = hwdesc->buffer1Length;

		if (nx_handle_large_addr(adapter, &pbuf->fragArray[0],
					&hwdesc->AddrBuffer1,
					vaddr, len, len[0])) {

			if (vlan_frag) {
				nx_free_frag_bounce_buf(adapter, buffrag);
			}
			
			adapter->stats.txOutOfBounceBufDropped++;
			goto drop_packet;
		}
	}
#endif

	for (i = 1, k = (1 + vlan_frag); i < (fragCount - vlan_frag); i++, k++) {
		struct skb_frag_struct *frag;
		int len, temp_len;
		unsigned long offset;
		dma_addr_t temp_dma = 0;

		/* move to next desc. if there is a need */
		if ((k & 0x3) == 0) {
			k = 0;

			producer = get_next_index(producer,
						  adapter->MaxTxDescCount);
			hwdesc = &hw->cmdDescHead[producer];
			memset(hwdesc, 0, sizeof(cmdDescType0_t));
		}
		frag = &skb_shinfo(skb)->frags[i - 1];
		len = frag->size;
		offset = frag->page_offset;

		temp_len = len;

		if(try_map_frag_page(adapter, frag->page, offset, len,
					      PCI_DMA_TODEVICE, &temp_dma)) {
			netxen_clean_tx_dma_mapping(adapter, pbuf, i);
			goto drop_packet;
		}


		buffrag++;

#ifdef ESX
		temp_dma    = page_to_phys(frag->page) + offset;
		if (adapter->bounce) {

			int len[MAX_PAGES_PER_FRAG] = {0};
			void *vaddr[MAX_PAGES_PER_FRAG] = {NULL};
			int p_i = 0; // Page index;
			int len_rem = temp_len;

			if((temp_dma + temp_len)  >= adapter->dma_mask) {
				while(p_i < MAX_PAGES_PER_FRAG) {
					if (len_rem <= PAGE_SIZE) {
						vaddr[p_i] =
							ESX_PHYS_TO_KMAP(temp_dma + p_i*PAGE_SIZE,
									len_rem);
						len[p_i] = len_rem;
						break;
					}

					vaddr[p_i] =
						ESX_PHYS_TO_KMAP(temp_dma + p_i*PAGE_SIZE,
								PAGE_SIZE);
					len[p_i] = PAGE_SIZE;
					p_i++;
					len_rem -= PAGE_SIZE;
				}

				if((p_i >= MAX_PAGES_PER_FRAG) || 
						nx_handle_large_addr(adapter,
							buffrag, &temp_dma,
							vaddr, len, temp_len)) {
					int j, total;
					total = i + vlan_frag;

					for(j = 0; j < total; j++) {
						nx_free_frag_bounce_buf(adapter,
							&pbuf->fragArray[j]);
					}


					p_i = 0;
					while ( (p_i < MAX_PAGES_PER_FRAG) &&
							vaddr[p_i]) {
						ESX_PHYS_TO_KMAP_FREE(vaddr[p_i]);
						p_i++;
					}
					adapter->stats.txOutOfBounceBufDropped++;
					goto drop_packet;
				}

				p_i = 0;
				while ( (p_i < MAX_PAGES_PER_FRAG) && vaddr[p_i]) {
					ESX_PHYS_TO_KMAP_FREE(vaddr[p_i]);
					p_i++;
				}
			} else {
				buffrag->bounce_buf[0] = NULL;
			}
		}
#endif

		buffrag->dma = temp_dma;
		buffrag->length = temp_len;

                nx_nic_print7(adapter, "for loop. i = %d k = %d\n", i, k);
                switch (k) {
                    case 0:
                            hwdesc->buffer1Length = temp_len;
                            hwdesc->AddrBuffer1   = temp_dma ;
                            break;
                    case 1:
                            hwdesc->buffer2Length = temp_len;
                            hwdesc->AddrBuffer2   = temp_dma ;
                            break;
                    case 2:
                            hwdesc->buffer3Length = temp_len;
                            hwdesc->AddrBuffer3   = temp_dma ;
                            break;
                    case 3:
                            hwdesc->buffer4Length = temp_len;
                            hwdesc->AddrBuffer4   = temp_dma ;
                            break;
                }
                frag ++;
        }

        producer = get_next_index(producer, adapter->MaxTxDescCount);

	nx_nic_check_tso(adapter, &hw->cmdDescHead[saved_producer],
			tagged, skb);

	if (tagged) {
		hw->cmdDescHead[saved_producer].flags |= FLAGS_VLAN_TAGGED;
	}

	/* For LSO, we need to copy the MAC/IP/TCP headers into
	 * the descriptor ring
	 */

	nx_nic_tso_copy_headers(adapter, skb, &producer, saved_producer);

#ifdef ESX
	if (uaddr && (skb->protocol == __constant_htons(ETH_P_RARP)))
		esx_send_filter(adapter, uaddr, &producer);
#endif /* ESX */

	adapter->stats.txbytes += hw->cmdDescHead[saved_producer].totalLength;

	adapter->cmdProducer = producer;
	unm_nic_update_cmd_producer(adapter, producer);

	adapter->stats.xmitcalled++;
	netdev->trans_start = jiffies;

done:
	TX_UNLOCK(&adapter->tx_lock, flags);
	return NETDEV_TX_OK;

requeue_packet:
	netif_stop_queue(netdev);
	TX_UNLOCK(&adapter->tx_lock, flags);
	return NETDEV_TX_BUSY;

drop_packet:
	adapter->stats.txdropped++;
        dev_kfree_skb_any(skb);
	goto done;
}


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0) || defined (ESX_3X) || defined (ESX_3X_COS))
static void unm_watchdog_fw_reset(unsigned long v)
{
	struct unm_adapter_s *adapter = (struct unm_adapter_s *)v;
	if(!(adapter->removing_module)) {
		SCHEDULE_WORK(&adapter->watchdog_task_fw_reset);
	}
}
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0) || defined (ESX_3X) || defined (ESX_3X_COS))
static void unm_watchdog(unsigned long v)
{
	struct unm_adapter_s *adapter = (struct unm_adapter_s *)v;
	if(netif_running(adapter->netdev)) {
		SCHEDULE_WORK(&adapter->watchdog_task);
	}
}
#endif



static int unm_nic_check_temp(struct unm_adapter_s *adapter)
{
	uint32_t temp, temp_state, temp_val;
	int rv = 0;

	temp = NXRD32(adapter, CRB_TEMP_STATE);

	temp_state = nx_get_temp_state(temp);
	temp_val = nx_get_temp_val(temp);
	if (adapter->temp == 0) {
// initialize value to normal, else if already in warn temp range, no msg comes out
		adapter->temp = NX_TEMP_NORMAL;  	
	}

	if (temp_state == NX_TEMP_PANIC) {
                netdev_list_t *this;
                nx_nic_print1(adapter, "Device temperature %d degrees C "
			      "exceeds maximum allowed. Hardware has been "
			      "shut down.\n",
			      temp_val);
		for (this = adapter->netlist; this != NULL;
		     this = this->next) {
			netif_carrier_off(this->netdev);
			netif_stop_queue(this->netdev);
		}
#ifdef UNM_NIC_SNMP_TRAP
		unm_nic_send_snmp_trap(NX_TEMP_PANIC);
#endif
		rv = 1;
	} else if (temp_state == NX_TEMP_WARN) {
		if (adapter->temp == NX_TEMP_NORMAL) {
			nx_nic_print1(adapter, "Device temperature %d degrees "
				      "C.  Dynamic control starting.\n",
				      temp_val);
#ifdef UNM_NIC_SNMP_TRAP
			unm_nic_send_snmp_trap(NX_TEMP_WARN);
#endif
		}
	} else {
		if (adapter->temp == NX_TEMP_WARN) {
			nx_nic_print1(adapter, "Device temperature is now %d "
				      "degrees C in normal range.\n",
				      temp_val);
		}
	}
	adapter->temp = temp_state;
	return rv;
}

int nx_p3_set_vport_miss_mode(struct unm_adapter_s *adapter, int mode)
{
	nic_request_t req;

	req.opcode = NX_NIC_HOST_REQUEST;
	req.body.cmn.req_hdr.opcode =
				NX_NIC_H2C_OPCODE_PROXY_SET_VPORT_MISS_MODE;
	req.body.cmn.req_hdr.comp_id = 0;
	req.body.cmn.req_hdr.ctxid = adapter->portnum;
	req.body.vport_miss_mode.mode = mode;

	return (nx_nic_send_cmd_descs(adapter->netdev,
				      (cmdDescType0_t *) & req,
				      1));

}

static void unm_nic_handle_phy_intr(struct unm_adapter_s *adapter)
{
        uint32_t  val, val1, linkupval;
	struct net_device  *netdev;


		if (NX_IS_REVISION_P3(adapter->ahw.revision_id)) {
                	val = NXRD32(adapter, CRB_XG_STATE_P3);
			val1 = XG_LINK_STATE_P3(adapter->ahw.pci_func,val);
			linkupval = XG_LINK_UP_P3;
		} else {
                	val = NXRD32(adapter, CRB_XG_STATE);
                	val >>= (adapter->portnum * 8);
			val1 = val & 0xff;
			linkupval = XG_LINK_UP;
		}

		netdev = adapter->netdev;

		if (adapter->ahw.linkup && (val1 != linkupval)) {
			nx_nic_print3(adapter, "NIC Link is down\n");
			adapter->ahw.linkup = 0;
			netif_carrier_off(netdev);
			netif_stop_queue(netdev);
			if (adapter->ahw.board_type == UNM_NIC_GBE) {
				unm_nic_set_link_parameters(adapter);
			}



		} else if (!adapter->ahw.linkup && (val1 == linkupval)) {
			nx_nic_print3(adapter, "NIC Link is up\n");
			adapter->ahw.linkup = 1;
			netif_carrier_on(netdev);
			netif_wake_queue(netdev);
			if (adapter->ahw.board_type == UNM_NIC_GBE) {
				unm_nic_set_link_parameters(adapter);
			}
		}
}
#ifdef ESX_3X
int nx_reset_netq_rx_queues( struct net_device *netdev)
{
	struct unm_adapter_s *adapter;
	uint32_t rx_ctx_state,num_of_queues;
	int ctx_id;
	adapter = netdev_priv(netdev);
	if(adapter->multi_ctx) {
		rx_ctx_state = adapter->nx_dev->rx_ctxs_state;
		num_of_queues = adapter->nx_dev->rx_ctxs_state;
		adapter->nx_dev->rx_ctxs_state  = ~(adapter->nx_dev->rx_ctxs_state);
		while ( num_of_queues )
		{
			ctx_id = nx_nic_multictx_alloc_rx_ctx(netdev);
			if(ctx_id > 0)
				nx_nic_multictx_set_rx_rule(netdev, ctx_id, &adapter->nx_dev->saved_rules[ctx_id].arg.m.mac);
			num_of_queues = num_of_queues & (num_of_queues - 1);
		}
		adapter->nx_dev->rx_ctxs_state  = rx_ctx_state;
	}
	return 0;
}
#else
#ifdef ESX_4X
int nx_reset_netq_rx_queues( struct net_device *netdev) 
{
	struct unm_adapter_s *adapter;
	adapter = netdev_priv(netdev);
	if(adapter->multi_ctx)
		vmknetddi_queueops_invalidate_state(netdev);
	return 0;
}
#else
int nx_reset_netq_rx_queues( struct net_device *netdev)
{
	return 0;
}
#endif
#endif

int netxen_nic_attach_all_ports(struct unm_adapter_s *adapter)
{
	struct pci_dev *dev;
	int prev_lro_state;
	int rv = 0;
#ifdef ESX
	int bus_id = adapter->pdev->bus->number;
	list_for_each_entry(dev, &pci_devices, global_list) {
		if(dev->bus->number != bus_id)
			continue;
#else
	list_for_each_entry(dev,&adapter->pdev->bus->devices,bus_list)  {
#endif		
		struct net_device *curr_netdev;
		struct unm_adapter_s *curr_adapter;
		curr_netdev = pci_get_drvdata(dev);
		if(curr_netdev) {
			curr_adapter = netdev_priv(curr_netdev);
			prev_lro_state = curr_adapter->lro.enabled;
			if (curr_adapter->ahw.fw_capabilities_1 & NX_FW_CAPABILITY_LRO) {
				nx_nic_print6(adapter, "LRO capable device found\n");
				if(unm_init_lro(curr_adapter)){ 
					nx_nic_print3(curr_adapter,
							"LRO Initialization failed\n");
				}
			}
			curr_adapter->lro.enabled = prev_lro_state;
			if(curr_adapter->state == PORT_UP) {
				int err = unm_nic_attach(curr_adapter);
				if (err != 0) {
					nx_nic_print3(curr_adapter, "Failed to attach device\n");
					rv = err;
				}
			}
			nx_reset_netq_rx_queues(curr_netdev);
			netif_device_attach(curr_netdev);
		}
	}
	return rv;
}
int check_fw_reset_failure(struct unm_adapter_s *adapter) 
{

	if((jiffies - adapter->fwload_failed_jiffies ) < UNM_FW_RESET_INTERVAL) {
		adapter->fwload_failed_count ++;

	} else {
		adapter->fwload_failed_count = 0;
	}
	adapter->fwload_failed_jiffies = jiffies;

	if(adapter->fwload_failed_count >= UNM_FW_RESET_FAILED_THRESHOLD) {
		adapter->fwload_failed_count = 0;
		return 1;
	}

	return 0;

}
int netxen_nic_restart_fw(struct unm_adapter_s *adapter)
{
	int rv = 0;
	struct pci_dev *dev;
#ifdef ESX
	int bus_id = adapter->pdev->bus->number;
	list_for_each_entry(dev, &pci_devices, global_list) {
		if(dev->bus->number != bus_id)
			continue;
#else
		list_for_each_entry(dev,&adapter->pdev->bus->devices,bus_list)  {
#endif

		struct net_device *curr_netdev;
		struct unm_adapter_s *curr_adapter;
		curr_netdev = pci_get_drvdata(dev);
		if(curr_netdev) {
			curr_adapter = netdev_priv(curr_netdev);
			curr_adapter->curr_window = 255;
			if(curr_adapter->portnum == 0) {
				rv = nx_start_firmware(curr_adapter);
				if(rv) {
					nx_nic_print3(adapter, "\n Firmware load failed");
					return rv;
				}else
					return 0;

			}
		}
	}
	return -1;
}
int netxen_nic_reset_tx_timeout(struct unm_adapter_s  *adapter)
{
        struct pci_dev *dev;
#ifdef ESX
	int bus_id = adapter->pdev->bus->number;
	list_for_each_entry(dev, &pci_devices, global_list) {
		if(dev->bus->number != bus_id)
			continue;
#else
		list_for_each_entry(dev,&adapter->pdev->bus->devices,bus_list)  {
#endif

			struct net_device *curr_netdev;
			struct unm_adapter_s *curr_adapter;
			curr_netdev = pci_get_drvdata(dev);
			if(curr_netdev) {
				curr_adapter = netdev_priv(curr_netdev);
				curr_adapter->tx_timeout_count = 0;
			}
		}
		return 0;
	}


int netxen_nic_detach_all_ports(struct unm_adapter_s *adapter, int fw_health)
{
	struct pci_dev *dev;
	int adapter_state;
#ifdef ESX
	int bus_id = adapter->pdev->bus->number;
	list_for_each_entry(dev, &pci_devices, global_list) {
		if(dev->bus->number != bus_id)
			continue;
#else
	list_for_each_entry(dev,&adapter->pdev->bus->devices,bus_list)  {
#endif
		struct net_device *curr_netdev;
		struct unm_adapter_s *curr_adapter;
		curr_netdev = pci_get_drvdata(dev);
		if(curr_netdev) {
			curr_adapter = netdev_priv(curr_netdev);
			curr_adapter->promisc = 0;

			spin_lock(&curr_adapter->ctx_state_lock);
			curr_adapter->attach_flag = 0;
			spin_unlock(&curr_adapter->ctx_state_lock);

			if(!fw_health) {
				netif_device_detach(curr_netdev);
				adapter_state = curr_adapter->is_up;
				curr_adapter->is_up = FW_RESETTING;
				if(curr_adapter->state == PORT_UP) {
					unm_nic_down(curr_netdev);
				}
				curr_adapter->is_up = adapter_state;
				unm_nic_detach(curr_adapter, NX_DESTROY_CTX_NO_FWCMD);
				unm_cleanup_lro(curr_adapter);

			}
			else
			{
				adapter_state = curr_adapter->is_up;
				curr_adapter->is_up = FW_RESETTING;
				if(curr_adapter->state == PORT_UP) {
					unm_nic_down(curr_netdev);
				}
				curr_adapter->is_up = adapter_state;
				curr_adapter->is_up = FW_DEAD;
			}
		}
	}


	return 0;

}

static void nx_nic_halt_firmware(struct unm_adapter_s *adapter)
{
	NXWR32(adapter, UNM_CRB_PEG_NET_0 + 0x3c, 1);
	NXWR32(adapter, UNM_CRB_PEG_NET_1 + 0x3c, 1);
	NXWR32(adapter, UNM_CRB_PEG_NET_2 + 0x3c, 1);
	NXWR32(adapter, UNM_CRB_PEG_NET_3 + 0x3c, 1);
	NXWR32(adapter, UNM_CRB_PEG_NET_4 + 0x3c, 1);

	nx_nic_print2(adapter, "Firmwre is halted.\n");
}


static void unm_watchdog_task_fw_reset(TASK_PARAM adapid)
{

	u32     old_alive_counter, rv, fw_reset;
	u32     failure_type, return_address;
	int     err = 0;
#if defined(__VMKLNX__) || LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
	struct unm_adapter_s *adapter = container_of(adapid,
						     struct unm_adapter_s,
						     watchdog_task_fw_reset);
#else
	struct unm_adapter_s *adapter = (struct unm_adapter_s *)adapid;
#endif
#if defined (__VMKLNX__)
	if(!adapter || adapter->removing_module) {
		goto out2;
	}
#else /* !defined (__VMKLNX__) */
	if(!adapter) {
		goto out2;
	}
	if(adapter->removing_module) {
		goto out2;
	}
#endif /* defined (__VMKLNX__) */
	if (unm_nic_check_temp(adapter)) {
		/*We return without turning on the netdev queue as there
		 *was an overheated device
		 */
		/*We should call all the detach here*/
		netxen_nic_detach_all_ports(adapter, 0);
		nx_nic_print3(adapter,"\nPlease unload and load the driver.\n");
		goto out2;
	}

	if(!adapter->auto_fw_reset) {
		goto out;
	}
	/*
	 * Check if FW is still running.
	 */
	api_lock(adapter);
	fw_reset = NXRD32(adapter,UNM_FW_RESET);
	api_unlock(adapter);
	if(fw_reset == 1)
	{
		if (!try_module_get(THIS_MODULE)) {
                        goto out;
                }
                if(check_fw_reset_failure(adapter)) {
			nx_nic_print3(adapter,"\nFW reset failed.");
                        nx_nic_print3(adapter,"\nPlease unload and load the driver.\n");
						nx_nic_halt_firmware(adapter);
                        module_put(THIS_MODULE);
                        goto out2;
                }
		netxen_nic_detach_all_ports(adapter, 0);
		rv = netxen_nic_restart_fw(adapter);

		api_lock(adapter);
		NXWR32(adapter,UNM_FW_RESET, 0);
		api_unlock(adapter);

		if(!rv) {
			err = netxen_nic_attach_all_ports(adapter);
			if(err) {
				nx_nic_print3(adapter,"FW reset failed.\n");
				nx_nic_print3(adapter,"Please unload and load the driver.\n");
				netxen_nic_detach_all_ports(adapter, 1);
				nx_nic_halt_firmware(adapter);
				module_put(THIS_MODULE);
				goto out2;
			}
		}
		netxen_nic_reset_tx_timeout(adapter);
		module_put(THIS_MODULE);
		goto out;
	}

	if (NX_FW_VERSION(adapter->version.major, adapter->version.minor,
			  adapter->version.sub) < NX_FW_VERSION(4, 0, 222)) {
		goto out;

	}
	old_alive_counter = adapter->alive_counter;
	adapter->alive_counter = NXRD32(adapter,
					UNM_PEG_ALIVE_COUNTER);

	if (old_alive_counter == adapter->alive_counter) {
		if ((!(adapter->fw_alive_failures & 7)) && (adapter->fw_alive_failures != 0)) {
			nx_nic_print5(adapter, "Device is DOWN. Fail "
				      "count[%u]\n",
				      adapter->fw_alive_failures);
			if (!try_module_get(THIS_MODULE)) {
                                goto out;
                        }

			failure_type = NXRD32(adapter, UNM_PEG_HALT_STATUS1);
			return_address = NXRD32(adapter, UNM_PEG_HALT_STATUS2);
			nx_nic_print3(adapter,"Firmware hang detected. "
			 "Severity code=%0x Peg number=%0x "
			 "Error code=%0x Return address=%0x\n",
			 ((failure_type & 0xf0000000) >> 28),
			 NX_ERROR_PEGNUM(failure_type),
			 NX_ERROR_ERRCODE(failure_type),return_address);

                        if(check_fw_reset_failure(adapter)) {
				nx_nic_print3(adapter,"\nFW reset failed.");
                                nx_nic_print3(adapter,"\nPlease unload and load the driver.\n");
								nx_nic_halt_firmware(adapter);
                                module_put(THIS_MODULE);
                                goto out2;
                        }

			if(failure_type &  NX_RCODE_FATAL_ERROR) {
				/*Device is dead So halt the firmware and print debug msg*/
				netxen_nic_detach_all_ports(adapter, 1);
				nx_nic_print3(adapter,"\nFatal Error.");
                                nx_nic_print3(adapter,"\nPlease unload and load the driver.\n");
				nx_nic_halt_firmware(adapter);
				module_put(THIS_MODULE);
				goto out2;		
			}
			else {
				netxen_nic_detach_all_ports(adapter, 0);
				rv = netxen_nic_restart_fw(adapter);

				api_lock(adapter);
				NXWR32(adapter,UNM_FW_RESET, 0);
				api_unlock(adapter);

				if(!rv) { 
					err = netxen_nic_attach_all_ports(adapter);
					if(err) {
						nx_nic_print3(adapter,"FW reset failed.\n");
						nx_nic_print3(adapter,"Please unload and load the driver.\n");
						netxen_nic_detach_all_ports(adapter, 1);
						nx_nic_halt_firmware(adapter);
						module_put(THIS_MODULE);
						goto out2;
					}
				}
				module_put(THIS_MODULE);	
			}
		}
		adapter->fw_alive_failures++;

	} else if (adapter->fw_alive_failures) {
		adapter->fw_alive_failures = 0;
		nx_nic_print5(adapter, "Device is UP.\n");
	}

out:
	mod_timer(&adapter->watchdog_timer_fw_reset, jiffies + 5 * HZ);
out2:
	return;
}
static void unm_watchdog_task(TASK_PARAM adapid)
{
#if defined(__VMKLNX__) || LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
	struct unm_adapter_s *adapter = container_of(adapid,
						     struct unm_adapter_s,
						     watchdog_task);
#else
	struct unm_adapter_s *adapter = (struct unm_adapter_s *)adapid;
#endif

	if (!netif_running(adapter->netdev)) {
		return;
	}

	if ( ! nx_nic_get_linkevent_cap(adapter)) {
		unm_nic_handle_phy_intr(adapter);
	}
	
	mod_timer(&adapter->watchdog_timer, jiffies + 2 * HZ);
}

static void unm_tx_timeout(struct net_device *netdev)
{
	struct unm_adapter_s *adapter = (struct unm_adapter_s *)netdev_priv(netdev);
	SCHEDULE_WORK(&adapter->tx_timeout_task);
}

static void unm_tx_timeout_task(TASK_PARAM adapid)
{
#if defined(__VMKLNX__) || LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
	struct unm_adapter_s *adapter = container_of(adapid,
						     struct unm_adapter_s,
						     tx_timeout_task);
#else
	struct net_device *netdev = (struct net_device *)adapid;
	struct unm_adapter_s *adapter = (struct unm_adapter_s *)netdev_priv(netdev);
#endif
	unsigned long flags;
	int fw_reset_count;

	if(adapter->is_up == FW_DEAD) {
		return;
	}

	spin_lock_irqsave(&adapter->lock, flags);
	adapter->tx_timeout_count ++;
	if((adapter->tx_timeout_count > UNM_FW_RESET_THRESHOLD) || 
                                	(adapter->attach_flag == 0)) {
		api_lock(adapter);
		fw_reset_count = NXRD32(adapter,UNM_FW_RESET);
		if(fw_reset_count != 1) {
			NXWR32(adapter,UNM_FW_RESET,1);
		}
		api_unlock(adapter);
		goto out;

	}

	read_lock(&adapter->adapter_lock);
	if(adapter->nx_dev->rx_ctxs[0] != NULL) {
		unm_nic_disable_int(adapter, &adapter->nx_dev->rx_ctxs[0]->sds_rings[0]);
	}

	adapter->netdev->trans_start = jiffies;

	if(adapter->nx_dev->rx_ctxs[0] != NULL) {
		unm_nic_enable_int(adapter, &adapter->nx_dev->rx_ctxs[0]->sds_rings[0]);
	}
	read_unlock(&adapter->adapter_lock);
	netif_wake_queue(adapter->netdev);

out:
	spin_unlock_irqrestore(&adapter->lock, flags);
}



/*
 * unm_nic_get_stats - Get System Network Statistics
 * @netdev: network interface device structure
 */
static struct net_device_stats *unm_nic_get_stats(struct net_device *netdev)
{
	struct unm_adapter_s *adapter = netdev_priv(netdev);
	struct net_device_stats *stats = &adapter->net_stats;

	memset(stats, 0, sizeof(struct net_device_stats));

	/* total packets received   */
	stats->rx_packets += adapter->stats.no_rcv;
	/* total packets transmitted    */
	stats->tx_packets += adapter->stats.xmitedframes +
	    adapter->stats.xmitfinished;
	/* total bytes received     */
	stats->rx_bytes += adapter->stats.rxbytes;
	/* total bytes transmitted  */
	stats->tx_bytes += adapter->stats.txbytes;
	/* bad packets received     */
	stats->rx_errors = adapter->stats.rcvdbadskb;
	/* packet transmit problems */
	stats->tx_errors = adapter->stats.nocmddescriptor;
	/* no space in linux buffers    */
	stats->rx_dropped = adapter->stats.updropped;
	/* no space available in linux  */
	stats->tx_dropped = adapter->stats.txdropped;

	return stats;
}

/*
 * This is Firmware 3.4 version specific call. Also FW 3.4 is only supported
 * for P2 devices.
 *
 * Returns 0 on success, (-EINVAL) MTU is out of range.
 */
static int nx_nic_fw34_change_mtu(struct net_device *netdev, int new_mtu)
{
	struct unm_adapter_s *adapter = netdev_priv(netdev);

        if (new_mtu < 0 || new_mtu > P2_MAX_MTU) {
                nx_nic_print3(adapter, "MTU[%d] > %d is not supported\n",
			      new_mtu, P2_MAX_MTU);
                return -EINVAL;
        }

	nx_nic_p2_set_mtu(adapter, new_mtu);
	netdev->mtu = new_mtu;
	adapter->mtu = new_mtu;

	return 0;
}

/*
 * This is for Firmware version 4.0 and greater.
 *
 * Returns 0 on success, negative on failure
 */
static int nx_nic_fw40_change_mtu(struct net_device *netdev, int new_mtu)
{
	struct unm_adapter_s *adapter = netdev_priv(netdev);
	uint32_t max_mtu;
	int i;

	if (NX_FW_VERSION(adapter->version.major, adapter->version.minor,
			  adapter->version.sub) >= NX_FW_VERSION(4, 0, 222)) {
		if (nx_fw_cmd_query_max_mtu(adapter->nx_dev,
					    adapter->ahw.pci_func,
					    &max_mtu) != NX_RCODE_SUCCESS) {
			nx_nic_print4(adapter, "Failed in query of FW for "
				      "max MTU\n");
			return (-EIO);
		}
	} else {
		if (NX_IS_REVISION_P2(adapter->ahw.revision_id)) {
			max_mtu = P2_MAX_MTU;
		} else {
			max_mtu = P3_MAX_MTU;
		}
	}

	nx_nic_print6(adapter, "Max supported MTU is %u\n", max_mtu);

        if (new_mtu < 0 || new_mtu > max_mtu) {
                nx_nic_print3(adapter, "MTU[%d] > %d is not supported\n",
			      new_mtu, max_mtu);
                return -EINVAL;
        }

        for (i = 0; i < adapter->nx_dev->alloc_rx_ctxs; i++) {
                if ((adapter->nx_dev->rx_ctxs[i] != NULL) && 
                      ((adapter->nx_dev->rx_ctxs_state & (1 << i)) != 0)) {
			if (nx_fw_cmd_set_mtu(adapter->nx_dev->rx_ctxs[i],
						adapter->ahw.pci_func,
						new_mtu)) {
				nx_nic_print7(adapter, "Failed to set mtu to fw\n");
				return -EIO;
			}
		}
	}

        if (NX_IS_REVISION_P2(adapter->ahw.revision_id)) {
		nx_nic_p2_set_mtu(adapter, new_mtu);
	}
	netdev->mtu = new_mtu;
	adapter->mtu = new_mtu;

	return 0;
}

/*
 *
 */
static inline int nx_nic_clear_legacy_int(unm_adapter *adapter,
				    nx_host_sds_ring_t *nxhal_sds_ring)
{
	uint32_t	mask;
	uint32_t	temp;
	uint32_t	our_int;
	uint32_t	status;

	nx_nic_print7(adapter, "Entered ISR Disable \n");

	read_lock(&adapter->adapter_lock);

	/* check whether it's our interrupt */

	adapter->unm_nic_pci_read_immediate(adapter,
			ISR_INT_VECTOR, &status);

	if (!(status & adapter->legacy_intr.int_vec_bit)) {
		read_unlock(&adapter->adapter_lock);
		return (-1);
	}

	if (adapter->ahw.revision_id >= NX_P3_B1) {
		adapter->unm_nic_pci_read_immediate(adapter,
				ISR_INT_STATE_REG, &temp);
		if (!ISR_IS_LEGACY_INTR_TRIGGERED(temp)) {
			nx_nic_print7(adapter, "state = 0x%x\n",
					temp);
			read_unlock(&adapter->adapter_lock);
			return (-1);
		}
	} else if (NX_IS_REVISION_P2(adapter->ahw.revision_id)) {

		our_int = NXRD32(adapter, CRB_INT_VECTOR);

		/* FIXME: Assumes pci_func is same as ctx */
		if ((our_int & (0x80 << adapter->portnum)) == 0) {
			if (our_int != 0) {
				/* not our interrupt */
				read_unlock(&adapter->adapter_lock);
				return (-1);
			}
			nx_nic_print3(adapter, "P2 Legacy interrupt "
					"[bit 0x%x] without SW CRB[0x%x]"
					" bit being set\n",
					adapter->legacy_intr.int_vec_bit,
					our_int);
		}
		temp = our_int & ~((u32)(0x80 << adapter->portnum));
		NXWR32(adapter, CRB_INT_VECTOR, temp);
	}

	if(adapter->fw_v34)
		unm_nic_disable_int(adapter, nxhal_sds_ring);

	/* claim interrupt */
	temp = 0xffffffff;
	adapter->unm_nic_pci_write_immediate(adapter,
			adapter->legacy_intr.tgt_status_reg,
			&temp);

	adapter->unm_nic_pci_read_immediate(adapter, ISR_INT_VECTOR,
			&mask);

	/*
	 * Read again to make sure the legacy interrupt message got
	 * flushed out
	 */
	adapter->unm_nic_pci_read_immediate(adapter, ISR_INT_VECTOR,
			&mask);
	read_unlock(&adapter->adapter_lock);

	nx_nic_print7(adapter, "Done with Disable Int\n");
	return (0);
}

/*
 *
 */
static inline int nx_nic_clear_msi_int(unm_adapter *adapter,
				    nx_host_sds_ring_t *nxhal_sds_ring)
{
	uint32_t	temp;

	nx_nic_print7(adapter, "Entered ISR Disable \n");

	read_lock(&adapter->adapter_lock);

		/* clear interrupt */
		temp = 0xffffffff;
		adapter->unm_nic_pci_write_immediate(adapter,
					msi_tgt_status[adapter->ahw.pci_func],
					&temp);
	read_unlock(&adapter->adapter_lock);

	nx_nic_print7(adapter, "Done with Disable Int\n");
	return (0);
}


/*
 *
 */
static void unm_nic_disable_int(unm_adapter *adapter,
				nx_host_sds_ring_t *nxhal_sds_ring)
{
	NXWR32(adapter, nxhal_sds_ring->interrupt_crb,
				     0);
}

void unm_nic_disable_all_int(unm_adapter *adapter,
			     nx_host_rx_ctx_t *nxhal_host_rx_ctx)
{
	int	i;

	if (nxhal_host_rx_ctx == NULL ||
	    nxhal_host_rx_ctx->sds_rings == NULL) {
		return;
	}

	for (i = 0; i < nxhal_host_rx_ctx->num_sds_rings; i++) {
		unm_nic_disable_int(adapter, &nxhal_host_rx_ctx->sds_rings[i]);
	}
}

void unm_nic_enable_int(unm_adapter *adapter,
			nx_host_sds_ring_t *nxhal_sds_ring)
{
	u32 mask;
	u32 temp;

	nx_nic_print7(adapter, "Entered ISR Enable \n");

	temp = 1;
	NXWR32(adapter,
				     nxhal_sds_ring->interrupt_crb, temp);

	if (!UNM_IS_MSI_FAMILY(adapter)) {
		mask = 0xfbff;

		adapter->unm_nic_pci_write_immediate(adapter,
                                     adapter->legacy_intr.tgt_mask_reg,
                                     &mask);
	}

	nx_nic_print7(adapter, "Done with enable Int\n");
	return;
}

void unm_nic_enable_all_int(unm_adapter * adapter,
				nx_host_rx_ctx_t *nxhal_host_rx_ctx)
{
	int i;
        nx_host_sds_ring_t      *nxhal_sds_ring = NULL;
        sds_host_ring_t         *host_sds_ring  = NULL;


        for (i = 0; i < nxhal_host_rx_ctx->num_sds_rings; i++) {
                nxhal_sds_ring = &nxhal_host_rx_ctx->sds_rings[i];
                host_sds_ring = (sds_host_ring_t *)nxhal_sds_ring->os_data;
                unm_nic_enable_int(adapter, nxhal_sds_ring);
	}

}

#define find_diff_among(a,b,range) ((a)<=(b)?((b)-(a)):((b)+(range)-(a)))

#ifdef	OLD_KERNEL
#define IRQ_HANDLED
#define IRQ_NONE
#endif

static void inline nx_nic_handle_interrupt(struct unm_adapter_s *adapter,
		nx_host_sds_ring_t      *nxhal_sds_ring)
{
	sds_host_ring_t         *host_sds_ring  =
		(sds_host_ring_t *)nxhal_sds_ring->os_data;

#if !defined(UNM_NIC_NAPI)
	unm_process_rcv_ring(adapter, nxhal_sds_ring, MAX_STATUS_HANDLE);

	if (host_sds_ring->ring_idx == 0) {
		unm_process_cmd_ring((unsigned long)adapter);
	}

	read_lock(&adapter->adapter_lock);
	unm_nic_enable_int(adapter, nxhal_sds_ring);
	read_unlock(&adapter->adapter_lock);
#else
	NX_PREP_AND_SCHED_RX(host_sds_ring->netdev, host_sds_ring);
#endif
}

/**
 * nx_nic_legacy_intr - Legacy Interrupt Handler
 * @irq: interrupt number
 * data points to status ring
 **/
#ifdef OLD_KERNEL
void nx_nic_legacy_intr(int irq, void *data, struct pt_regs *regs)
#elif  !defined(__VMKLNX__) && LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
irqreturn_t nx_nic_legacy_intr(int irq, void *data, struct pt_regs * regs)
#else
irqreturn_t nx_nic_legacy_intr(int irq, void *data)
#endif
{
	nx_host_sds_ring_t      *nxhal_sds_ring = (nx_host_sds_ring_t *)data;
        sds_host_ring_t         *host_sds_ring  =
				     (sds_host_ring_t *)nxhal_sds_ring->os_data;
	struct unm_adapter_s *adapter = host_sds_ring->adapter;

	if (unlikely(!irq)) {
		return IRQ_NONE;	/* Not our interrupt */
	}

	if (nx_nic_clear_legacy_int(adapter, nxhal_sds_ring) == -1) {
		return IRQ_HANDLED;
	}

	/* process our status queue */
	if (!netif_running(adapter->netdev) && 
	    !adapter->nx_sds_cb_tbl[NX_NIC_CB_LSA].registered) {
		goto done;
	}

        nx_nic_print7(adapter, "Entered handle ISR\n");
	host_sds_ring->ints++;
	adapter->stats.ints++;

	nx_nic_handle_interrupt(adapter, nxhal_sds_ring);

done:
	return IRQ_HANDLED;
}

/**
 * nx_nic_msi_intr - MSI Interrupt Handler
 * @irq: interrupt number
 * data points to status ring
 **/
#ifdef OLD_KERNEL
void nx_nic_msi_intr(int irq, void *data, struct pt_regs *regs)
#elif  !defined(__VMKLNX__) && LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
irqreturn_t nx_nic_msi_intr(int irq, void *data, struct pt_regs * regs)
#else
irqreturn_t nx_nic_msi_intr(int irq, void *data)
#endif
{
	nx_host_sds_ring_t      *nxhal_sds_ring = (nx_host_sds_ring_t *)data;
        sds_host_ring_t         *host_sds_ring  =
				     (sds_host_ring_t *)nxhal_sds_ring->os_data;
	struct unm_adapter_s *adapter = host_sds_ring->adapter;

	if (unlikely(!irq)) {
		return IRQ_NONE;	/* Not our interrupt */
	}

	if (nx_nic_clear_msi_int(adapter, nxhal_sds_ring) == -1) {
		return IRQ_NONE;
	}

	/* process our status queue */
	if (!netif_running(adapter->netdev) && 
	    !adapter->nx_sds_cb_tbl[NX_NIC_CB_LSA].registered) {
		goto done;
	}

        nx_nic_print7(adapter, "Entered handle ISR\n");
	host_sds_ring->ints++;
	adapter->stats.ints++;

	nx_nic_handle_interrupt(adapter, nxhal_sds_ring);

done:
	return IRQ_HANDLED;
}

/**
 * nx_nic_msix_intr - MSI-X Interrupt Handler
 * @irq: interrupt vector
 * data points to status ring
 **/
#ifdef OLD_KERNEL
void nx_nic_msix_intr(int irq, void *data, struct pt_regs *regs)
#elif  !defined(__VMKLNX__) && LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
irqreturn_t nx_nic_msix_intr(int irq, void *data, struct pt_regs * regs)
#else
irqreturn_t nx_nic_msix_intr(int irq, void *data)
#endif
{
	nx_host_sds_ring_t      *nxhal_sds_ring = (nx_host_sds_ring_t *)data;
        sds_host_ring_t         *host_sds_ring  =
				     (sds_host_ring_t *)nxhal_sds_ring->os_data;
	struct unm_adapter_s *adapter = host_sds_ring->adapter;
	U32     ctx_id;

	if (unlikely(!irq)) {
		return IRQ_NONE;	/* Not our interrupt */
	}

	/* process our status queue */
	if (!netif_running(adapter->netdev) && 
	    !adapter->nx_sds_cb_tbl[NX_NIC_CB_LSA].registered) {
		goto done;
	}

        nx_nic_print7(adapter, "Entered handle ISR\n");
	host_sds_ring->ints++;
	adapter->stats.ints++;

	ctx_id = host_sds_ring->ring->parent_ctx->this_id;
	if(ctx_id && !(adapter->nx_dev->rx_ctxs_state & (1 << ctx_id))) {
		goto done;
	}

	nx_nic_handle_interrupt(adapter, nxhal_sds_ring);

done:
	return IRQ_HANDLED;
}


/*
 * Check if there are any command descriptors pending.
 */
static inline int unm_get_pending_cmd_desc_cnt(unm_pending_cmd_descs_t *
					       pending_cmds)
{
	return (pending_cmds->cnt);
}

#ifdef UNM_NIC_NAPI
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,21)
static inline void __netif_rx_complete(struct net_device *dev)
{
	if (!test_bit(__LINK_STATE_RX_SCHED, &dev->state))
		BUG();
	list_del(&dev->poll_list);
	smp_mb__before_clear_bit();
	clear_bit(__LINK_STATE_RX_SCHED, &dev->state);
}
#endif /* LINUX_VERSION_CODE < 2.4.23 */

/*
 * Per status ring processing.
 */
#ifdef NEW_NAPI
static int nx_nic_poll_sts(struct napi_struct *napi, int work_to_do)
{
	sds_host_ring_t *host_sds_ring  = container_of(napi, sds_host_ring_t, napi);
	nx_host_sds_ring_t *nxhal_sds_ring = (nx_host_sds_ring_t *)host_sds_ring->ring;
#else
static int nx_nic_poll_sts(struct net_device *netdev, int *budget)
{
	nx_host_sds_ring_t *nxhal_sds_ring = (nx_host_sds_ring_t *)netdev_priv(netdev);
	sds_host_ring_t    *host_sds_ring  =
		(sds_host_ring_t *)nxhal_sds_ring->os_data;
	int work_to_do = min(*budget, netdev->quota);
#endif
	struct unm_adapter_s    *adapter = host_sds_ring->adapter;
	int done = 1;
	int work_done;
	U32 ctx_id;

#ifdef NEW_NAPI
	if((nxhal_sds_ring == NULL) || (adapter == NULL)) {
		return 0;
	}
#endif

	ctx_id = host_sds_ring->ring->parent_ctx->this_id;

	if(!netif_running(adapter->netdev) || 
		(ctx_id && !(adapter->nx_dev->rx_ctxs_state & (1 << ctx_id)))){
		NETIF_RX_COMPLETE((host_sds_ring->netdev), napi);
		return 0;
	}

	work_done = unm_process_rcv_ring(adapter, nxhal_sds_ring, work_to_do);

#ifndef NEW_NAPI
	netdev->quota -= work_done;
	*budget -= work_done;
#endif

	if (work_done >= work_to_do) 
		done = 0;

#if defined(NEW_NAPI)
	/*
	 * Only one cpu must be processing Tx command ring.
	 */
	if(spin_trylock(&adapter->tx_cpu_excl)) {

		if (unm_process_cmd_ring((unsigned long)adapter) == 0) {
			done = 0;
		}
		spin_unlock(&adapter->tx_cpu_excl);
	}
#endif


        nx_nic_print7(adapter, "new work_done: %d work_to_do: %d\n",
		      work_done, work_to_do);
	if (done) {
		NETIF_RX_COMPLETE((host_sds_ring->netdev), napi);
		/*unm_nic_hw_write(adapter,(uint64_t)ISR_INT_VECTOR,
		  &reset_val,4); */
		read_lock(&adapter->adapter_lock);
		unm_nic_enable_int(adapter, nxhal_sds_ring);
		read_unlock(&adapter->adapter_lock);
	}
#if defined(NEW_NAPI)
	/*
	 * This comes after enabling the interrupt. The reason is that if done
	 * before the interrupt is turned and the following routine could fill
	 * up the cmd ring and the card could finish the processing and try
	 * interrupting the host even before the interrupt is turned on again.
	 * This could lead to missed interrupt and has an outside chance of
	 * deadlock.
	 */
	if(spin_trylock(&adapter->tx_cpu_excl)) {

		spin_lock(&adapter->tx_lock);
		if (unm_get_pending_cmd_desc_cnt(&adapter->pending_cmds)) {
			unm_proc_pending_cmd_desc(adapter);
		}
		spin_unlock(&adapter->tx_lock);
		spin_unlock(&adapter->tx_cpu_excl);
	}
	return work_done;
#else
	return (done ? 0 : 1);
#endif
}

#ifndef NEW_NAPI
static int unm_nic_poll(struct net_device *netdev, int *budget)
{
	struct unm_adapter_s *adapter = netdev_priv(netdev);
	int work_to_do = min(*budget, netdev->quota);
	int done = 1;
	int work_done = 0;
	nx_host_sds_ring_t *nxhal_sds_ring =
		&adapter->nx_dev->rx_ctxs[0]->sds_rings[0] ;

	adapter->stats.polled++;

	work_done = unm_process_rcv_ring(adapter,
			nxhal_sds_ring,
			work_to_do);

	netdev->quota -= work_done;
	*budget -= work_done;

	if (work_done >= work_to_do)
		done = 0;

	if (unm_process_cmd_ring((unsigned long)adapter) == 0) {
		done = 0;
	}

	nx_nic_print7(adapter, "new work_done: %d work_to_do: %d\n",
			work_done, work_to_do);
	if (done) {
		netif_rx_complete(netdev);
		/*unm_nic_hw_write(adapter,(uint64_t)ISR_INT_VECTOR,
		  &reset_val,4); */
		read_lock(&adapter->adapter_lock);
		unm_nic_enable_int(adapter,
				nxhal_sds_ring);
		read_unlock(&adapter->adapter_lock);
	}

	/*
	 * This comes after enabling the interrupt. The reason is that if done
	 * before the interrupt is turned and the following routine could fill
	 * up the cmd ring and the card could finish the processing and try
	 * interrupting the host even before the interrupt is turned on again.
	 * This could lead to missed interrupt and has an outside chance of
	 * deadlock.
	 */
	if (unm_get_pending_cmd_desc_cnt(&adapter->pending_cmds)) {
		spin_lock(&adapter->tx_lock);
		unm_proc_pending_cmd_desc(adapter);
		spin_unlock(&adapter->tx_lock);
	}

	return (done ? 0 : 1);
}
#endif
#endif

#if !defined(__VMKLNX__)
#ifdef CONFIG_NET_POLL_CONTROLLER
static void unm_nic_poll_controller(struct net_device *netdev)
{
	struct unm_adapter_s *adapter = netdev_priv(netdev);
	disable_irq(netdev->irq);
#if !defined(__VMKLNX__) && LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
	adapter->intr_handler(netdev->irq,  &adapter->nx_dev->rx_ctxs[0]->sds_rings[0], NULL);
#else
	adapter->intr_handler(netdev->irq, &adapter->nx_dev->rx_ctxs[0]->sds_rings[0]);
#endif
	enable_irq(netdev->irq);
}
#endif
#endif /* !defined (__VMKLNX__) */

static inline struct sk_buff *process_rxbuffer(struct unm_adapter_s *adapter,
					nx_host_sds_ring_t *nxhal_sds_ring,
					int desc_ctx, int totalLength,
					int csum_status, int index)
{

	struct net_device *netdev = adapter->netdev;
	struct pci_dev *pdev = adapter->pdev;
	struct sk_buff *skb = NULL;
	sds_host_ring_t  *host_sds_ring =
				 (sds_host_ring_t *)nxhal_sds_ring->os_data;
	rds_host_ring_t *host_rds_ring = NULL;
	nx_host_rds_ring_t *nxhal_rds_ring = NULL;
	nx_host_rx_ctx_t *nxhal_rx_ctx = nxhal_sds_ring->parent_ctx;
	struct unm_rx_buffer *buffer = NULL;


	nxhal_rds_ring = &nxhal_rx_ctx->rds_rings[desc_ctx];
	host_rds_ring = (rds_host_ring_t *) nxhal_rds_ring->os_data;
	buffer = &host_rds_ring->rx_buf_arr[index];

	pci_unmap_single(pdev, (buffer)->dma, host_rds_ring->dma_size,
			 PCI_DMA_FROMDEVICE);

	skb = (struct sk_buff *)(buffer)->skb;

	if (unlikely(skb == NULL)) {
		/*
		 * This should not happen and if it does, it is serious,
		 * catch it
		 */

		/* Window = 0 */
		NXWR32(adapter, UNM_CRB_PEG_NET_0 + 0x3c, 1);
		NXWR32(adapter, UNM_CRB_PEG_NET_1 + 0x3c, 1);
		NXWR32(adapter, UNM_CRB_PEG_NET_2 + 0x3c, 1);
		NXWR32(adapter, UNM_CRB_PEG_NET_3 + 0x3c, 1);
		NXWR32(adapter, UNM_CRB_NIU + 0x70000, 0);

                nx_nic_print2(adapter, "NULL skb for index %d desc_ctx 0x%x "
			      "of %s type\n",
			      index, desc_ctx, RCV_DESC_TYPE_NAME(desc_ctx));
                nx_nic_print2(adapter, "Halted the pegs and stopped the NIU "
			      "consumer\n");

		adapter->stats.rcvdbadskb++;
		host_rds_ring->stats.rcvdbadskb++;
		return NULL;
	}
	host_sds_ring->count++;
#if defined(XGB_DEBUG)
	if (!skb_is_sane(skb)) {
		dump_skb(skb);
                nx_nic_print3(adapter, "index:%d skb(%p) dma(%p) not sane\n",
			      index, skb, buffer->dma);
		/* Window = 0 */
		NXWR32(adapter, UNM_CRB_PEG_NET_0 + 0x3c, 1);
		NXWR32(adapter, UNM_CRB_PEG_NET_1 + 0x3c, 1);
		NXWR32(adapter, UNM_CRB_PEG_NET_2 + 0x3c, 1);
		NXWR32(adapter, UNM_CRB_PEG_NET_3 + 0x3c, 1);
		NXWR32(adapter, UNM_CRB_NIU + 0x70000, 0);

		return NULL;
	}
#endif

#if defined(UNM_NIC_HW_CSUM)
	if (likely((adapter->rx_csum) && (csum_status == STATUS_CKSUM_OK))) {
		adapter->stats.csummed++;
		skb->ip_summed = CHECKSUM_UNNECESSARY;
	} else {
		skb->ip_summed = CHECKSUM_NONE;
	}
#endif
	skb->dev = netdev;

	/*
	 * We just consumed one buffer so post a buffer.
	 * Buffers may come out of order in Rcv when LRO is turned ON.
	 */
	buffer->skb = NULL;
	buffer->state = UNM_BUFFER_FREE;
	buffer->lro_current_frags = 0;
	buffer->lro_expected_frags = 0;
	/* "process packet - allocate skb" sequence is having some performance
	 * implications. Until that is sorted out, don't allocate skbs here
	 */
	if (nx_alloc_rx_skb(adapter, nxhal_rds_ring, buffer)) {
		/*
		 * In case of failure buffer->skb is NULL and when trying to
		 * post the receive descriptor to the card that function tries
		 * allocating the buffer again.
		 */
		atomic_inc(&host_rds_ring->alloc_failures);
		adapter->stats.alloc_failures_imm++;
	}
	host_sds_ring->free_rbufs[desc_ctx].count++;
	list_add_tail(&buffer->link, &host_sds_ring->free_rbufs[desc_ctx].head);

	return skb;
}

/*
 * unm_process_rcv() send the received packet to the protocol stack.
 * and if the number of receives exceeds RX_BUFFERS_REFILL, then we
 * invoke the routine to send more rx buffers to the Phantom...
 */
static inline void unm_process_rcv(struct unm_adapter_s *adapter,
				   nx_host_sds_ring_t *nxhal_sds_ring,
				   statusDesc_t *desc)
{
	struct net_device *netdev = adapter->netdev;
	int index = desc->referenceHandle;
	struct unm_rx_buffer *buffer;
	struct sk_buff *skb;
	uint32_t length = desc->totalLength;
	uint32_t desc_ctx;
	int ret;
	sds_host_ring_t *host_sds_ring =
			 (sds_host_ring_t *)nxhal_sds_ring->os_data;
	nx_host_rx_ctx_t *nxhal_rx_ctx = nxhal_sds_ring->parent_ctx;
	nx_host_rds_ring_t *nxhal_host_rds_ring = NULL;
	rds_host_ring_t *host_rds_ring = NULL;
	int vid = -1;

	desc_ctx = desc->type;
	//nx_nic_print3(adapter, "type == %d\n", desc_ctx);
	if (unlikely(desc_ctx >= adapter->max_rds_rings)) {
                nx_nic_print3(adapter, "Bad Rcv descriptor ring\n");
		return;
	}

	nxhal_host_rds_ring = &nxhal_rx_ctx->rds_rings[desc_ctx];
	host_rds_ring = (rds_host_ring_t *) nxhal_host_rds_ring->os_data;
	if (unlikely(index > nxhal_host_rds_ring->ring_size)) {
                nx_nic_print3(adapter, "Got a buffer index:%x for %s desc "
			      "type. Max is %x\n",
			      index, RCV_DESC_TYPE_NAME(desc_ctx),
			      nxhal_host_rds_ring->ring_size);
		return;
	}
	buffer = &host_rds_ring->rx_buf_arr[index];

	/*
	 * Check if the system is running very low on buffers. If it is then
	 * don't process this packet and just repost it to the firmware. This
	 * avoids a condition where the fw has no buffers and does not
	 * interrupt the host because it has no packets to notify.
	 */
	if (rarely(nxhal_host_rds_ring->ring_size -
		   atomic_read(&host_rds_ring->alloc_failures) <
		   NX_NIC_RX_POST_THRES)) {

		buffer = &host_rds_ring->rx_buf_arr[index];
		buffer->state = UNM_BUFFER_BUSY;
		host_sds_ring->free_rbufs[desc_ctx].count++;
		list_add(&buffer->link, 
			&host_sds_ring->free_rbufs[desc_ctx].head);
		return;
	}

	skb = process_rxbuffer(adapter, nxhal_sds_ring, desc_ctx,
				 desc->totalLength,
				desc->status, index);
	if (!skb) {
		BUG();
		return;
	}

	skb_put(skb, (length < nxhal_host_rds_ring-> buff_size) ? 
			length : nxhal_host_rds_ring->buff_size);
	skb_pull(skb, desc->pkt_offset);
	SKB_ADJUST_PKT_MA(skb, desc->pkt_offset);

	check_8021q(netdev, skb, &vid);

	skb->protocol = eth_type_trans(skb, netdev);

	if (adapter->testCtx.capture_input) {
		if (adapter->testCtx.rx_user_packet_data != NULL &&
		    (adapter->testCtx.rx_user_pos + (skb->len)) <=
		    adapter->testCtx.rx_datalen) {
			memcpy(adapter->testCtx.rx_user_packet_data +
			       adapter->testCtx.rx_user_pos, skb->data,
			       skb->len);
			adapter->testCtx.rx_user_pos += (skb->len);
		}
	}

	if (!netif_running(netdev)) {
		kfree_skb(skb);
		goto packet_done_no_stats;
	}

#ifndef ESX
	/* At this point we have got a valid skb.
	 * Check whether this is a SYN that pegnet needs to handle
	 */
	if (desc->opcode == adapter->msg_type_sync_offload) {
		/* check whether pegnet wants this */
		spin_lock(&adapter->cb_lock);

		if (adapter->nx_sds_msg_cb_tbl[UNM_MSGTYPE_NIC_SYN_OFFLOAD].handler != 
		    nx_status_msg_default_handler) {
			ret = adapter->nx_sds_msg_cb_tbl[UNM_MSGTYPE_NIC_SYN_OFFLOAD].handler(netdev, 
				adapter->nx_sds_msg_cb_tbl[UNM_MSGTYPE_NIC_SYN_OFFLOAD].data, NULL, skb);

			/* ok, it does */
			if (ret == 0) {
				kfree_skb(skb);
				spin_unlock(&adapter->cb_lock);
				goto packet_done;
			}
		}
		spin_unlock(&adapter->cb_lock);
	} else if (desc->opcode == UNM_MSGTYPE_CAPTURE) {
		unm_msg_t msg;

		msg.hdr.word       = desc->body[0];
		msg.body.values[0] = desc->body[0];
		msg.body.values[1] = desc->body[1];

		if (adapter->nx_sds_msg_cb_tbl[UNM_MSGTYPE_CAPTURE].handler(netdev,
			NULL, &msg, skb)) {
		        nx_nic_print5(adapter, "%s: (parse error) dropping "
			              "captured offloaded skb %p\n", __FUNCTION__, skb);
		        kfree_skb(skb);
		        goto packet_done;
		}	
	}
	if ((desc->body[0] & NX_PROT_SHIFT_VAL(NX_PROT_MASK)) == 
	     	NX_PROT_SHIFT_VAL(NX_PROT_NIC_ICMP) && 
	    desc->opcode == UNM_MSGTYPE_NIC_RXPKT_DESC) {
		spin_lock(&adapter->cb_lock);
		ret = adapter->nx_sds_msg_cb_tbl[NX_PROT_NIC_ICMP].handler(netdev,
		adapter->nx_sds_msg_cb_tbl[NX_PROT_NIC_ICMP].data, NULL, skb);
		spin_unlock(&adapter->cb_lock);
	}

#endif

	nx_set_skb_queueid(skb, nxhal_rx_ctx);

	/* Check 1 in every NX_LRO_CHECK_INTVL packets for lro worthiness */
	if (rarely(((adapter->stats.uphappy & NX_LRO_CHECK_INTVL) == 0)  &&
		   adapter->lro.enabled != 0 && adapter->rx_csum)) {
		nx_try_initiate_lro(adapter, skb, desc->hashValue,
				    (__uint32_t)desc->port);
	}

	/* fallthrough for regular nic processing */
#ifdef UNM_XTRA_DEBUG
	nx_nic_print7(adapter, "reading from %p index %d, %d bytes\n",
			buffer->dma, index, length);
	for (i = 0; &skb->data[i] < skb->tail; i++) {
		printk("%02x%c", skb->data[i], (i + 1) % 0x10 ? ' ' : '\n');
	}
	printk("\n");
#endif

	NX_ADJUST_SMALL_PKT_LEN(skb);

	ret = nx_rx_packet(adapter, skb, vid);

	if (ret == NET_RX_SUCCESS) 
		adapter->stats.uphappy++;
	

	netdev->last_rx = jiffies;
#ifndef ESX
packet_done:
#endif
	adapter->stats.no_rcv++;
	adapter->stats.rxbytes += length;
	host_rds_ring->stats.no_rcv++;
	host_rds_ring->stats.rxbytes += length;

packet_done_no_stats:

	return;
}

static inline void lro2_adjust_skb(struct sk_buff *skb, statusDesc_t *desc)
{
	struct iphdr	*iph;
	struct tcphdr	*th;
	__uint16_t	length;

#ifdef ESX
	skb_pull(skb, ETH_HLEN);
#endif
	iph = (struct iphdr *)skb->data;
	skb_pull(skb, iph->ihl << 2);
	th = (struct tcphdr *)skb->data;
	skb_push(skb, iph->ihl << 2);

	length = (iph->ihl << 2) + (th->doff << 2) + desc->lro2.length;
	iph->tot_len = htons(length);
	iph->check = 0;
	iph->check = ip_fast_csum((unsigned char *)iph, iph->ihl);
	th->psh = desc->lro2.psh;
	th->seq = htonl(desc->lro2.seq_number);

#ifdef ESX
	skb_push(skb, ETH_HLEN);
#endif
//	nx_nic_print5(NULL, "Sequence Number [0x%x]\n", desc->lro2.seq_number);
}

static void unm_process_lro_contiguous(struct unm_adapter_s *adapter,
				       nx_host_sds_ring_t *nxhal_sds_ring,
				       statusDesc_t *desc)
{
	struct net_device	*netdev = adapter->netdev;
	struct sk_buff		*skb;
	uint32_t		length;
	uint32_t		data_offset;
	uint32_t		desc_ctx;
	sds_host_ring_t		*host_sds_ring;
	int			ret;
	nx_host_rx_ctx_t	*nxhal_rx_ctx = nxhal_sds_ring->parent_ctx;
	nx_host_rds_ring_t	*hal_rds_ring = NULL;
	rds_host_ring_t		*host_rds_ring = NULL;
	uint16_t		ref_handle;
	uint16_t		stats_idx;
	int			vid = -1;

	host_sds_ring = (sds_host_ring_t *)nxhal_sds_ring->os_data;

	desc_ctx = desc->lro2.type;
	if (unlikely(desc_ctx != 1)) {
		nx_nic_print3(adapter, "Bad Rcv descriptor ring\n");
		return;
	}

	hal_rds_ring = &nxhal_rx_ctx->rds_rings[desc_ctx];
	host_rds_ring = (rds_host_ring_t *)hal_rds_ring->os_data;

	adapter->lro.stats.contiguous_pkts++;
	stats_idx = desc->lro2.length >> 10;
	if (stats_idx >= NX_1K_PER_LRO) {
		stats_idx = (NX_1K_PER_LRO - 1);
	}
	adapter->lro.stats.bufsize[stats_idx]++;
	/*
	 * Check if the system is running very low on buffers. If it is then
	 * don't process this packet and just repost it to the firmware. This
	 * avoids a condition where the fw has no buffers and does not
	 * interrupt the host because it has no packets to notify.
	 */
	if (rarely(hal_rds_ring->ring_size -
		   atomic_read(&host_rds_ring->alloc_failures) <
						NX_NIC_RX_POST_THRES)) {
		struct unm_rx_buffer	*buffer;

		adapter->stats.rcv_buf_crunch++;

		ref_handle = desc->lro2.ref_handle;
		if (rarely(ref_handle >= hal_rds_ring->ring_size)) {
			nx_nic_print3(adapter, "Got a bad ref_handle[%u] for "
				      "%s desc type. Max[%u]\n",
				      ref_handle, RCV_DESC_TYPE_NAME(desc_ctx),
				      hal_rds_ring->ring_size);
			BUG();
			return;
		}
		buffer = &host_rds_ring->rx_buf_arr[ref_handle];
		buffer->state = UNM_BUFFER_BUSY;
		host_sds_ring->free_rbufs[desc_ctx].count++;
		list_add(&buffer->link, 
				&host_sds_ring->free_rbufs[desc_ctx].head);
		return;
	}

	ref_handle = desc->lro2.ref_handle;
	length = desc->lro2.length;

	skb = process_rxbuffer(adapter, nxhal_sds_ring, desc_ctx,
			       hal_rds_ring->buff_size, STATUS_CKSUM_OK,
			       ref_handle);

	if (!skb) {
		BUG();
		return;
	}

	data_offset = (desc->lro2.l4_hdr_offset +
		       (desc->lro2.timestamp ?
			TCP_TS_HDR_SIZE : TCP_HDR_SIZE));

	skb_put(skb, data_offset + length);

#if !defined(ESX_3X)

	/*
	 * This adjustment seems to keep the stack happy in cases where some
	 * sk's sometimes seem to get stuck in a zero-window condition even
	 * though their receive queues may be of zero length.
	 */
	skb->truesize = (skb->len + sizeof(struct sk_buff) +
			 ((unsigned long)skb->data -
			  (unsigned long)skb->head));

#endif

	skb_pull(skb, desc->lro2.l2_hdr_offset);
	SKB_ADJUST_PKT_MA(skb, desc->lro2.l2_hdr_offset);
	check_8021q(netdev, skb, &vid);
	skb->protocol = eth_type_trans(skb, netdev);

	lro2_adjust_skb(skb, desc);

	NX_ADJUST_SMALL_PKT_LEN(skb);

	length = skb->len;

	if (!netif_running(netdev)) {
		kfree_skb(skb);
		return;
	}

	nx_set_skb_queueid(skb, nxhal_rx_ctx);

	ret = nx_rx_packet(adapter, skb, vid);

	netdev->last_rx = jiffies;

	adapter->stats.no_rcv++;
	adapter->stats.rxbytes += length;
	host_rds_ring->stats.no_rcv++;
	host_rds_ring->stats.rxbytes += length;

	return;
}

static inline void lro_adjust_head_skb(struct sk_buff *head_skb,
				       struct sk_buff *tail_skb,
				       uint16_t incr_len,
				       nx_lro_hdr_desc_t *lro_hdr)
{
	struct iphdr	*iph;
	struct tcphdr	*head_th;
	struct tcphdr	*tail_th;
	__uint16_t	length;

	iph = (struct iphdr *)head_skb->data;
	length = ntohs(iph->tot_len) + incr_len;
	iph->tot_len = htons(length);
	iph->check = 0;
	iph->check = ip_fast_csum((unsigned char *)iph, iph->ihl);

	skb_pull(head_skb, iph->ihl << 2);
	head_th = (struct tcphdr *)head_skb->data;
	head_th->psh = lro_hdr->psh;
	/* tail skb data is pointing to payload */
	skb_push(tail_skb, head_th->doff << 2);
	tail_th = (struct tcphdr *)tail_skb->data;
	head_th->ack_seq = tail_th->ack_seq;
	if (head_th->doff > 5) {
		memcpy((head_skb->data + TCP_HDR_SIZE),
		       (tail_skb->data + TCP_HDR_SIZE),
		       TCP_TS_OPTION_SIZE);
	}
	skb_pull(tail_skb, head_th->doff << 2);
	skb_push(head_skb, iph->ihl << 2);
}

/*
 * unm_process_lro() send the lro packet to the protocol stack.
 * and if the number of receives exceeds RX_BUFFERS_REFILL, then we
 * invoke the routine to send more rx buffers to the Phantom...
 */
/* TODO: Large part of this function is common with unm_process_rcv()
 *  and should go to common fn */
static inline void unm_process_lro(struct unm_adapter_s *adapter,
				   nx_host_sds_ring_t *nxhal_sds_ring,
				   statusDesc_t *desc, statusDesc_t *desc_list,
				   int num_desc)
{
	struct net_device	*netdev = adapter->netdev;
	struct sk_buff		*skb;
	uint32_t		length;
	uint32_t		data_length;
	uint32_t		desc_ctx;
	sds_host_ring_t		*host_sds_ring;
	int			ret;
	int			ii;
	int			jj;
	struct sk_buff		*head_skb;
	struct sk_buff		*last_skb;
	int			nr_skbs;
	nx_lro_frags_desc_t	*lro_frags;
	nx_host_rx_ctx_t	*nxhal_rx_ctx = adapter->nx_dev->rx_ctxs[0];
	nx_host_rds_ring_t	*hal_rds_ring = NULL;
	rds_host_ring_t		*host_rds_ring = NULL;
	uint16_t		ref_handle;


	host_sds_ring = (sds_host_ring_t *)nxhal_sds_ring->os_data;

	nr_skbs = desc->lro_hdr.count;
	desc_ctx = desc->lro_hdr.type;
	if (unlikely(desc_ctx != 0)) {
		nx_nic_print3(adapter, "Bad Rcv descriptor ring\n");
		return;
	}
	hal_rds_ring = &nxhal_rx_ctx->rds_rings[desc_ctx];
	host_rds_ring = (rds_host_ring_t *)hal_rds_ring->os_data;

	adapter->lro.stats.chained_pkts++;
	if (nr_skbs <= NX_MAX_PKTS_PER_LRO) {
		adapter->lro.stats.accumulation[nr_skbs - 1]++;
	}
#if 0
	struct unm_rx_buffer	*buffer;

	/*
	 * Check if the system is running very low on buffers. If it is then
	 * don't process this packet and just repost it to the firmware. This
	 * avoids a condition where the fw has no buffers and does not
	 * interrupt the host because it has no packets to notify.
	 */
	if (rarely(hal_rds_ring->ring_size -
		   atomic_read(&host_rds_ring->alloc_failures) <
						NX_NIC_RX_POST_THRES)) {

		nx_nic_print3(adapter, "In rcv buffer crunch !!\n");

		ref_handle = desc->lro_hdr.ref_handle;
		if (rarely(ref_handle >= hal_rds_ring->ring_size)) {
			nx_nic_print3(adapter, "Got a bad ref_handle[%u] for "
				      "%s desc type. Max[%u]\n",
				      ref_handle, RCV_DESC_TYPE_NAME(desc_ctx),
				      hal_rds_ring->ring_size);
			BUG();
			return;
		}
		buffer = &host_rds_ring->rx_buf_arr[ref_handle];
		buffer->state = UNM_BUFFER_BUSY;
		host_sds_ring->free_rbufs[desc_ctx].count++;
		TAILQ_INSERT_TAIL(&host_sds_ring->free_rbufs[desc_ctx].head,
				  buffer, link);
		nr_skbs--;

		for (ii = 0; ii < num_desc; ii++) {
			lro_frags = &desc_list[ii].lro_frags;

			for (jj = 0;
			     jj < NX_LRO_PKTS_PER_STATUS_DESC && nr_skbs;
			     jj++, nr_skbs--) {

				ref_handle = lro_frags->pkts[jj].s.ref_handle;

				if (rarely(ref_handle >= hal_rds_ring->ring_size)) {
					nx_nic_print3(adapter, "Got a bad "
						      "ref_handle[%u] for %s "
						      "desc type. Max[%u]\n",
						      ref_handle,
						      RCV_DESC_TYPE_NAME(desc_ctx),
						      hal_rds_ring->ring_size);
					BUG();
					return;
				}
				buffer = &host_rds_ring->rx_buf_arr[ref_handle];
				buffer->state = UNM_BUFFER_BUSY;
				host_sds_ring->free_rbufs[desc_ctx].count++;
				TAILQ_INSERT_TAIL(&host_sds_ring->free_rbufs[desc_ctx].head,
						  buffer, link);
			}

		}
		return;
	}
#endif

	ref_handle = desc->lro_hdr.ref_handle;
	length = desc->lro_hdr.length;
	head_skb = process_rxbuffer(adapter, nxhal_sds_ring, desc_ctx,
				    hal_rds_ring->buff_size, STATUS_CKSUM_OK,
				    ref_handle);
	if (!head_skb) {
		BUG();
		return;
	}

	skb_put(head_skb, desc->lro_hdr.data_offset + length);
	skb_pull(head_skb, desc->lro_hdr.l2_hdr_offset);
	head_skb->protocol = eth_type_trans(head_skb, netdev);
	nr_skbs--;
//	netif_receive_skb(head_skb);

	last_skb = NULL;
	data_length = 0;
	for (ii = 0; ii < num_desc; ii++) {
		lro_frags = &desc_list[ii].lro_frags;

		for (jj = 0; jj < NX_LRO_PKTS_PER_STATUS_DESC && nr_skbs;
		     jj++, nr_skbs--) {

			ref_handle = lro_frags->pkts[jj].s.ref_handle;
			length = lro_frags->pkts[jj].s.length;

			if (unlikely(ref_handle >= hal_rds_ring->ring_size)) {
				nx_print(KERN_ERR, "Got a bad ref_handle[%u] for %s "
					 "desc type. Max[%u]\n",
					 ref_handle,
					 RCV_DESC_TYPE_NAME(desc_ctx),
					 hal_rds_ring->ring_size);
				nx_print(KERN_ERR, "ii[%u], jj[%u], nr_skbs[%u/%u], num_desc[%u]\n", ii, jj, nr_skbs, desc->lro_hdr.count, num_desc);
				printk("Head: %016llx %016llx\n", desc->body[0], desc->body[1]);
				for (ii = 0; ii < num_desc; ii++) {
					printk("%016llx %016llx\n", desc_list[ii].body[0], desc_list[ii].body[1]);
				}
				BUG();
				return;
			}

			skb = process_rxbuffer(adapter, nxhal_sds_ring,
					       desc_ctx,
					       hal_rds_ring->buff_size,
					       STATUS_CKSUM_OK, ref_handle);
			if (!skb) {
				BUG();
				return;
			}

			data_length += length;
			skb_put(skb, desc->lro_hdr.data_offset + length);
//	skb_pull(skb, desc->lro_hdr.hdr_offset);
//	skb->protocol = eth_type_trans(skb, netdev);
//	netif_receive_skb(skb);
//	continue;
			skb_pull(skb, desc->lro_hdr.data_offset);

			skb->next = NULL;
			if (skb_shinfo(head_skb)->frag_list == NULL) {
				skb_shinfo(head_skb)->frag_list = skb;
			}

			/* Point to payload */
			if (last_skb) {
				last_skb->next = skb;
			}
			last_skb = skb;
		}
	}

//	printk("Length = %u\n", data_length);
//	return;
	head_skb->data_len = data_length;
	head_skb->len += head_skb->data_len;
#ifndef ESX_3X
	head_skb->truesize += data_length;
#endif
	if (last_skb) {
		lro_adjust_head_skb(head_skb, last_skb, data_length,
				    &desc->lro_hdr);
	}
	
	NX_ADJUST_SMALL_PKT_LEN(head_skb);

	length = head_skb->len;

	if (!netif_running(netdev)) {
		kfree_skb(head_skb);
		return;
	}

	nx_set_skb_queueid(head_skb, nxhal_rx_ctx);

	ret = __NETIF_RX(head_skb);

	netdev->last_rx = jiffies;

	adapter->stats.no_rcv++;
	adapter->stats.rxbytes += length;

	return;
}

/*
 * nx_post_rx_descriptors puts buffer in the Phantom memory
 */
static int nx_post_rx_descriptors(struct unm_adapter_s *adapter,
				  nx_host_rds_ring_t *nxhal_rds_ring,
				  uint32_t ringid, nx_free_rbufs_t *free_list)
{
	uint producer;
	rcvDesc_t *pdesc;
	struct unm_rx_buffer *buffer;
	struct unm_rx_buffer *tmp_buffer;
	int count = 0;
	int rv;
	rds_host_ring_t *host_rds_ring = NULL;
	u32 	data;

	host_rds_ring = (rds_host_ring_t *) nxhal_rds_ring->os_data;
	producer = host_rds_ring->producer;
	/* We can start writing rx descriptors into the phantom memory. */
	list_for_each_entry_safe(buffer , tmp_buffer, &free_list->head, link) {

		if (buffer->skb == NULL) {
			/*If nx_alloc_rx_skb fails's, it should not modify buffer */
			rv = nx_alloc_rx_skb(adapter, nxhal_rds_ring, buffer);
			if (rv) {
				adapter->stats.alloc_failures_def++;
				continue;
			}

			if (atomic_read(&host_rds_ring->alloc_failures) > 0) {
				atomic_dec(&host_rds_ring->alloc_failures);
			}
		}

		list_del(&buffer->link);	

		/* make a rcv descriptor  */
		pdesc = ((rcvDesc_t *) (nxhal_rds_ring->host_addr)) + producer;
		pdesc->AddrBuffer = buffer->dma;
		pdesc->referenceHandle = buffer->refHandle;
		pdesc->bufferLength = host_rds_ring->dma_size;

                nx_nic_print7(adapter, "done writing descripter\n");
		producer = get_next_index(producer, nxhal_rds_ring->ring_size);

		count++;	/* now there should be no failure */
	}
	free_list->count -= count;
	if (!list_empty(&free_list->head)) {
		spin_lock_bh(&adapter->buf_post_lock);
		list_splice_tail(&free_list->head, 
					&host_rds_ring->free_rxbufs.head);
		host_rds_ring->free_rxbufs.count += free_list->count;
		spin_unlock_bh(&adapter->buf_post_lock);
	}

	host_rds_ring->producer = producer;

	/* if we did allocate buffers, then write the count to Phantom */
	if (count) {

		/* Window = 1 */
		read_lock(&adapter->adapter_lock);
		data = (producer - 1)& (nxhal_rds_ring->ring_size-1);
		NXWR32(adapter, nxhal_rds_ring->host_rx_producer, data);
		read_unlock(&adapter->adapter_lock);
	}

	host_rds_ring->posting = 0;
	return (count);
}

/*
 *
 */
static void nx_post_freed_rxbufs(struct unm_adapter_s *adapter,
				 nx_host_sds_ring_t *nxhal_sds_ring)
{
	int ring;
	//unm_rcv_desc_ctx_t    *rcv_desc;
	nx_free_rbufs_t *free_rbufs;
	nx_free_rbufs_t free_list;
	sds_host_ring_t         *host_sds_ring  = (sds_host_ring_t *)nxhal_sds_ring->os_data;
	rds_host_ring_t *host_rds_ring = NULL;
	nx_host_rds_ring_t *nxhal_rds_ring = NULL;
	nx_host_rx_ctx_t *nxhal_rx_ctx = nxhal_sds_ring->parent_ctx;

	for (ring = 0; ring < adapter->max_rds_rings; ring++) {
		nxhal_rds_ring = &nxhal_rx_ctx->rds_rings[ring];
		host_rds_ring = (rds_host_ring_t *) nxhal_rds_ring->os_data;
		free_rbufs = &host_sds_ring->free_rbufs[ring];

		if (!host_rds_ring->free_rxbufs.count && !free_rbufs->count) {
			continue;
		}

		INIT_LIST_HEAD(&free_list.head);
		spin_lock_bh(&adapter->buf_post_lock);
		if (!list_empty(&free_rbufs->head)) {
			list_splice_tail_init(&free_rbufs->head, 
					&host_rds_ring->free_rxbufs.head);
			host_rds_ring->free_rxbufs.count += free_rbufs->count;
			free_rbufs->count = 0;
		}
		if (!host_rds_ring->posting && host_rds_ring->free_rxbufs.count) {

			list_replace_init(&host_rds_ring->free_rxbufs.head, 
						&free_list.head);
			free_list.count = host_rds_ring->free_rxbufs.count;
			host_rds_ring->free_rxbufs.count = 0;

			host_rds_ring->posting = 1;
			spin_unlock_bh(&adapter->buf_post_lock);

			nx_post_rx_descriptors(adapter, nxhal_rds_ring, ring,
					       &free_list);
		} else {
			spin_unlock_bh(&adapter->buf_post_lock);
		}
	}
}

static int
nx_status_msg_nic_response_handler(struct net_device *netdev, void *data, unm_msg_t *msg, struct sk_buff *skb)
{
	struct unm_adapter_s *adapter = (struct unm_adapter_s *)netdev_priv(netdev);

	nx_os_handle_nic_response((nx_dev_handle_t)adapter, (nic_response_t *) &msg->body);

	return 0;
}

static int
nx_status_msg_default_handler(struct net_device *netdev, void *data, unm_msg_t *msg, struct sk_buff *skb)
{
#if 0
	struct unm_adapter_s *adapter = (struct unm_adapter_s *)netdev_priv(netdev);

	if (msg != NULL) {
		nx_nic_print3(adapter, "%s: Error: No status handler for this msg = 0x%x\n", __FUNCTION__, msg->hdr.type);
	} else {
		nx_nic_print3(adapter, "%s: Error: No status handler for this NULL msg\n", __FUNCTION__);
	}
#endif
	return 0;
}

/* Process Receive status ring */
static uint32_t
unm_process_rcv_ring(struct unm_adapter_s *adapter,
		     nx_host_sds_ring_t *nxhal_sds_ring, int max)
{
	statusDesc_t    *descHead       = (statusDesc_t *)nxhal_sds_ring->host_addr;
	sds_host_ring_t *host_sds_ring  = (sds_host_ring_t *)nxhal_sds_ring->os_data;
	statusDesc_t    *desc           = NULL;			/* used to read status desc here */
	uint32_t        consumer        = host_sds_ring->consumer;
	int             count           = 0;
	uint32_t        tmp_consumer    = 0;
	statusDesc_t    *last_desc      = NULL;
	u32 temp;

	host_sds_ring->polled++;

        nx_nic_print7(adapter, "processing receive\n");
	/*
	 * we assume in this case that there is only one port and that is
	 * port #1...changes need to be done in firmware to indicate port
	 * number as part of the descriptor. This way we will be able to get
	 * the netdev which is associated with that device.
	 */
	while (count < max) {
		desc = &descHead[consumer];
#if defined(DEBUG)
		if (desc->owner != STATUS_OWNER_HOST &&
		    desc->owner != STATUS_OWNER_PHANTOM) {
                    nx_nic_print7(adapter, "desc(%p)owner is %x consumer:%d\n",
				  desc, desc->owner, consumer);
			break;
		}
#endif
		if (!(desc->owner & STATUS_OWNER_HOST)) {
                    nx_nic_print7(adapter, "desc %p ownedby %s\n", desc,
				  STATUS_OWNER_NAME(desc));
			//printk("desc %p ownedby %s\n", desc,
			//       STATUS_OWNER_NAME(desc));
			break;
		}

		if (desc->opcode == adapter->msg_type_rx_desc ||
#if 0
		    desc->opcode == UNM_MSGTYPE_CAPTURE ||
#endif
		    desc->opcode == adapter->msg_type_sync_offload) {
			unm_process_rcv(adapter, nxhal_sds_ring, desc);
		} else if (desc->opcode == UNM_MSGTYPE_NIC_LRO_CONTIGUOUS) {
			unm_process_lro_contiguous(adapter, nxhal_sds_ring,
						   desc);
		} else if (desc->opcode == UNM_MSGTYPE_NIC_LRO_CHAINED) {

			int		desc_cnt, i;
			statusDesc_t	desc_list[5];

			desc_cnt = desc->descCnt - 1; /* First descriptor
							 already read so
							 decrement 1 */

			/* LRO packets */
			tmp_consumer = ((consumer + desc_cnt) &
					(adapter->MaxRxDescCount - 1));
			last_desc = &descHead[tmp_consumer];

			if (!(last_desc->owner & STATUS_OWNER_HOST)) {
				/*
				 * The whole queue message is not ready
				 * yet so break out of here.
				 */
				count = count ? 1 : 0;
				break;
			}
			desc->owner = STATUS_OWNER_PHANTOM;

			desc_cnt--;	/* Last desc is not very useful */
			for (i = 0; i < desc_cnt; i++) {
				consumer = ((consumer + 1) &
					    (adapter->MaxRxDescCount - 1));
				last_desc = &descHead[consumer];
				desc_list[i].body[0] = last_desc->body[0];
				desc_list[i].body[1] = last_desc->body[1];
				last_desc->owner = STATUS_OWNER_PHANTOM;
			}

			unm_process_lro(adapter, nxhal_sds_ring, desc,
					desc_list, desc_cnt);

			/*
			 * Place it at the last descriptor.
			 */
			consumer = ((consumer + 1) &
				    (adapter->MaxRxDescCount - 1));
			desc = &descHead[consumer];
		} else {
			unm_msg_t msg;
			int       cnt 	= desc->descCnt;
			int       index = 1;
			int       i     = 0;
			//cnt = desc->descCnt;

			/*
			 * Check if it is the extended queue message. If it is
			 * then read the rest of the descriptors also.
			 */
			if (cnt > 1) {

				*(uint64_t *) (&msg.hdr) = desc->body[0];
				msg.body.values[0] = desc->body[1];

				/*
				 *
				 */
				tmp_consumer = ((consumer + cnt - 1) &
						(adapter->MaxRxDescCount - 1));
				last_desc = &descHead[tmp_consumer];
				if (!(last_desc->owner & STATUS_OWNER_HOST)) {
					/*
					 * The whole queue message is not ready
					 * yet so break out of here.
					 */
					break;
				}
				
				desc->owner = STATUS_OWNER_PHANTOM;

				for (i = 0; i < (cnt - 2); i++) {
					consumer = ((consumer + 1) &
				    		(adapter->MaxRxDescCount - 1));
					desc = &descHead[consumer];
					msg.body.values[index++] = desc->body[0];
					msg.body.values[index++] = desc->body[1];
					desc->owner = STATUS_OWNER_PHANTOM;
				}

				consumer = ((consumer + 1) &
					    (adapter->MaxRxDescCount - 1));
				desc = &descHead[consumer];
			} else {
				/*
				 * These messages expect the type field in the
				 * queue message header to be set and nothing
				 * else in the header to be set. So we copy
				 * the desc->body[0] into the header.
				 * desc->body[0] has the opcode which is same
				 * as the queue message type.
				 */
				msg.hdr.word = desc->body[0];
				msg.body.values[0] = desc->body[0];
				msg.body.values[1] = desc->body[1];
			}

			/* Call back msg handler */
			spin_lock(&adapter->cb_lock);

			adapter->nx_sds_msg_cb_tbl[msg.hdr.type].handler(adapter->netdev, 
					adapter->nx_sds_msg_cb_tbl[msg.hdr.type].data, &msg, NULL);

			spin_unlock(&adapter->cb_lock);

		}
		desc->owner = STATUS_OWNER_PHANTOM;
		consumer = (consumer + 1) & (adapter->MaxRxDescCount - 1);
		count++;
	}

	nx_post_freed_rxbufs(adapter, nxhal_sds_ring);

	/* update the consumer index in phantom */
	if (count) {
		host_sds_ring->consumer = consumer;

		/* Window = 1 */
		read_lock(&adapter->adapter_lock);
		temp = consumer;
		NXWR32(adapter, nxhal_sds_ring->host_sds_consumer, temp);
		read_unlock(&adapter->adapter_lock);
	}

	return (count);
}


/*
 * Initialize the allocated resources in pending_cmd_descriptors.
 */
static void unm_init_pending_cmd_desc(unm_pending_cmd_descs_t *pending_cmds)
{
	INIT_LIST_HEAD(&pending_cmds->free_list);
	INIT_LIST_HEAD(&pending_cmds->cmd_list);
	pending_cmds->curr_block = 0;
	pending_cmds->cnt = 0;
}

/*
 * Free up the allocated resources in pending_cmd_descriptors.
 */
static void unm_destroy_pending_cmd_desc(unm_pending_cmd_descs_t *pending_cmds)
{
	unm_pending_cmd_desc_block_t *block;

	if (pending_cmds->curr_block) {
		kfree(pending_cmds->curr_block);
		pending_cmds->curr_block = 0;
	}

	while (!list_empty(&pending_cmds->free_list)) {

		block = list_entry(pending_cmds->free_list.next,
				   unm_pending_cmd_desc_block_t, link);
		list_del(&block->link);
		kfree(block);
	}

	while (!list_empty(&pending_cmds->cmd_list)) {

		block = list_entry(pending_cmds->cmd_list.next,
				   unm_pending_cmd_desc_block_t, link);
		list_del(&block->link);
		kfree(block);
	}
	INIT_LIST_HEAD(&pending_cmds->free_list);
	INIT_LIST_HEAD(&pending_cmds->cmd_list);
	pending_cmds->curr_block = 0;
	pending_cmds->cnt = 0;
}

/*
 *
 */
static unm_pending_cmd_desc_block_t
    *unm_dequeue_pending_cmd_desc(unm_pending_cmd_descs_t * pending_cmds,
				  uint32_t length)
{
	unm_pending_cmd_desc_block_t *block;

	if (!list_empty(&pending_cmds->cmd_list)) {
		if (length < MAX_PENDING_DESC_BLOCK_SIZE) {
			return (NULL);
		}
		block = list_entry(pending_cmds->cmd_list.next,
				   unm_pending_cmd_desc_block_t, link);
		list_del(&block->link);
	} else {
		block = pending_cmds->curr_block;
		if (block == NULL || length < block->cnt) {
			return (NULL);
		}
		pending_cmds->curr_block = NULL;
	}

	pending_cmds->cnt -= block->cnt;

	return (block);
}

/*
 *
 */
static inline unm_pending_cmd_desc_block_t *nx_alloc_pend_cmd_desc_block(void)
{
	return (kmalloc(sizeof(unm_pending_cmd_desc_block_t),
			in_atomic()? GFP_ATOMIC : GFP_KERNEL));
}

/*
 *
 */
static void unm_free_pend_cmd_desc_block(unm_pending_cmd_descs_t * pending_cmds,
					 unm_pending_cmd_desc_block_t * block)
{
/*         printk("%s: Inside\n", __FUNCTION__); */

	block->cnt = 0;
	list_add_tail(&block->link, &pending_cmds->free_list);
}

/*
 * This is to process the pending q msges for chimney.
 */
static void unm_proc_pending_cmd_desc(unm_adapter * adapter)
{
	uint32_t producer;
	uint32_t consumer;
	struct unm_cmd_buffer *pbuf;
	uint32_t length = 0;
	unm_pending_cmd_desc_block_t *block;
	int i;

	producer = adapter->cmdProducer;
	consumer = adapter->lastCmdConsumer;
	if (producer == consumer) {
		length = adapter->MaxTxDescCount - 1;
	} else if (producer > consumer) {
		length = adapter->MaxTxDescCount - producer + consumer - 1;
	} else {
		/* consumer > Producer */
		length = consumer - producer - 1;
	}

	while (length) {
		block = unm_dequeue_pending_cmd_desc(&adapter->pending_cmds,
						     length);
		if (block == NULL) {
			break;
		}
		length -= block->cnt;

		for (i = 0; i < block->cnt; i++) {
			pbuf = &adapter->cmd_buf_arr[producer];
			pbuf->mss = 0;
			pbuf->totalLength = 0;
			pbuf->skb = NULL;
			pbuf->cmd = PEGNET_REQUEST;
			pbuf->fragCount = 0;
			pbuf->port = 0;

			adapter->ahw.cmdDescHead[producer] = block->cmd[i];

			producer = get_next_index(producer,
						  adapter->MaxTxDescCount);
		}
		unm_free_pend_cmd_desc_block(&adapter->pending_cmds, block);
	}

	if (adapter->cmdProducer == producer) {
		return;
	}
	adapter->cmdProducer = producer;
	unm_nic_update_cmd_producer(adapter, producer);
}

/*
 * This is invoked when we are out of descriptor ring space for
 * pegnet_cmd_desc. It queues them up and is to be processed when the
 * descriptor space is freed.
 */
static void nx_queue_pend_cmd_desc(unm_pending_cmd_descs_t * pending_cmds,
				   cmdDescType0_t * cmd_desc, uint32_t length)
{
	unm_pending_cmd_desc_block_t *block;

	if (pending_cmds->curr_block) {
		block = pending_cmds->curr_block;

	} else if (!list_empty(&pending_cmds->free_list)) {

		block = list_entry(pending_cmds->free_list.next,
				   unm_pending_cmd_desc_block_t, link);
		list_del(&block->link);
		block->cnt = 0;
	} else {
		nx_nic_print2(NULL, "%s: *BUG** This should never happen\n",
			      __FUNCTION__);
		return;
	}

	memcpy(&block->cmd[block->cnt], cmd_desc, length);
	block->cnt++;

	if (block->cnt == MAX_PENDING_DESC_BLOCK_SIZE) {
		/*
		 * The array is full, put it in the tail of the cmd_list
		 */
		list_add_tail(&block->link, &pending_cmds->cmd_list);
		block = NULL;
	}

	pending_cmds->cnt++;
	pending_cmds->curr_block = block;
}

/*
 *
 */
static int nx_make_available_pend_cmd_desc(unm_pending_cmd_descs_t * pend,
					   int cnt)
{
	unm_pending_cmd_desc_block_t *block;

	block = pend->curr_block;
	if (block && block->cnt <= (MAX_PENDING_DESC_BLOCK_SIZE - cnt)) {
		return (0);
	}

	if (!list_empty(&pend->free_list)) {
		return (0);
	}

	block = nx_alloc_pend_cmd_desc_block();
	if (block != NULL) {
		/*
		 * Puts it into the free list.
		 */
		unm_free_pend_cmd_desc_block(pend, block);
		return (0);
	}

	return (-ENOMEM);
}

/*
 *
 */
static int nx_queue_pend_cmd_desc_list(unm_adapter * adapter,
				       cmdDescType0_t * cmd_desc_arr,
				       int no_of_desc)
{
	int i;
	int rv;

	rv = nx_make_available_pend_cmd_desc(&adapter->pending_cmds,
					     no_of_desc);
	if (rv) {
		return (rv);
	}

	i = 0;
	do {
		nx_queue_pend_cmd_desc(&adapter->pending_cmds,
				       &cmd_desc_arr[i],
				       sizeof(cmdDescType0_t));
		i++;
	} while (i != no_of_desc);

	return (0);
}

/*
 * Compare adapter ids for two NetXen devices.
 */
int nx_nic_cmp_adapter_id(struct net_device *dev1, struct net_device *dev2)
{
	unm_adapter *adapter1 = (unm_adapter *) netdev_priv(dev1);
	unm_adapter *adapter2 = (unm_adapter *) netdev_priv(dev2);
	return memcmp(adapter1->id, adapter2->id, sizeof(adapter1->id));
}

/*
 * Checks if the specified device is a NetXen device.
 */
int nx_nic_is_netxen_device(struct net_device *netdev)
{
	return (netdev->do_ioctl == unm_nic_ioctl);
}

/*
 * Return the registered TNIC adapter structure.
 *
 * Returns:
 *	The Tnic adapter if already registered else returns NULL.
 */
nx_tnic_adapter_t *nx_nic_get_lsa_adapter(struct net_device *netdev)
{
	struct unm_adapter_s *adapter = (unm_adapter *) netdev_priv(netdev);

	if (adapter) {
		return (adapter->nx_sds_cb_tbl[NX_NIC_CB_LSA].data);
	}

	return (NULL);
}

/*
 * Register msg handler with Nic driver.
 *
 * Return:
 *	0	- If registered successfully.
 *	1	- If already registered.
 *	-1	- If port or adapter is not set.
 */
int nx_nic_rx_register_msg_handler(struct net_device *netdev, uint8_t msgtype,
				   void *data,
				   int (*nx_msg_handler)(struct net_device *netdev, void *data,
						  	  unm_msg_t *msg, struct sk_buff *skb))
{
	struct unm_adapter_s *adapter = (struct unm_adapter_s *)netdev_priv(netdev);
	int rv = -1;

	if (adapter) {
		spin_lock_bh(&adapter->cb_lock);

		rv = 1;
		if (!adapter->nx_sds_msg_cb_tbl[msgtype].registered) {
			adapter->nx_sds_msg_cb_tbl[msgtype].msg_type = msgtype;
			adapter->nx_sds_msg_cb_tbl[msgtype].data = data; 
			adapter->nx_sds_msg_cb_tbl[msgtype].handler =
								nx_msg_handler;
			adapter->nx_sds_msg_cb_tbl[msgtype].registered = 1; 
			rv = 0;
		}
		spin_unlock_bh(&adapter->cb_lock);
	}

	return (rv);
}

/*
 * Unregisters a known msg handler
 */
void nx_nic_rx_unregister_msg_handler(struct net_device *netdev, uint8_t msgtype)
{
	struct unm_adapter_s *adapter = (struct unm_adapter_s *)netdev_priv(netdev);

	spin_lock_bh(&adapter->cb_lock);

	if (adapter->nx_sds_msg_cb_tbl[msgtype].registered) {
		adapter->nx_sds_msg_cb_tbl[msgtype].msg_type = msgtype;
		adapter->nx_sds_msg_cb_tbl[msgtype].data = NULL; 
		adapter->nx_sds_msg_cb_tbl[msgtype].handler =
						nx_status_msg_default_handler; 
		adapter->nx_sds_msg_cb_tbl[msgtype].registered = 0; 
	}

	spin_unlock_bh(&adapter->cb_lock);
}

/*
 * Register call back handlers (except msg handler) with Nic driver.
 *
 * Return:
 *	0	- If registered successfully.
 *	1	- If already registered.
 *	-1	- If port or adapter is not set.
 */
int nx_nic_rx_register_callback_handler(struct net_device *netdev,
					uint8_t interface_type, void *data)
{
	struct unm_adapter_s *adapter = (struct unm_adapter_s *)netdev_priv(netdev);
	int rv = -1;

	if (adapter) {
		spin_lock_bh(&adapter->nx_sds_cb_tbl[interface_type].lock);
		rv = 1;
		if (!adapter->nx_sds_cb_tbl[interface_type].registered) {
			adapter->nx_sds_cb_tbl[interface_type].interface_type =
								interface_type;
			adapter->nx_sds_cb_tbl[interface_type].data = data;
			adapter->nx_sds_cb_tbl[interface_type].registered = 1;
			adapter->nx_sds_cb_tbl[interface_type].refcnt = 0;
			rv = 0;
		}
		spin_unlock_bh(&adapter->nx_sds_cb_tbl[interface_type].lock);
	}

	return (rv);
}

/*
 * Unregisters callback handlers
 */
void nx_nic_rx_unregister_callback_handler(struct net_device *netdev,
					   uint8_t interface_type)
{
	struct unm_adapter_s	*adapter;

	adapter = (struct unm_adapter_s *)netdev_priv(netdev);

	spin_lock_bh(&adapter->nx_sds_cb_tbl[interface_type].lock);

	if (adapter->nx_sds_cb_tbl[interface_type].registered) {
		adapter->nx_sds_cb_tbl[interface_type].data = NULL;
		adapter->nx_sds_cb_tbl[interface_type].registered = 0;
		adapter->nx_sds_cb_tbl[interface_type].refcnt = 0;
	}

	spin_unlock_bh(&adapter->nx_sds_cb_tbl[interface_type].lock);
}

void nx_nic_register_lsa_with_fw(struct net_device *netdev)
{
	struct unm_adapter_s	*adapter;
	nx_host_nic_t		*nx_dev;

	adapter = (struct unm_adapter_s *)netdev_priv(netdev);
	nx_dev = adapter->nx_dev;

	nx_fw_cmd_configure_toe(nx_dev->rx_ctxs[0], nx_dev->pci_func, 
				NX_CDRP_TOE_INIT);
}

void nx_nic_unregister_lsa_from_fw(struct net_device *netdev)
{
	struct unm_adapter_s	*adapter;
	nx_host_nic_t		*nx_dev;

	adapter = (struct unm_adapter_s *)netdev_priv(netdev);
	nx_dev = adapter->nx_dev;

	nx_fw_cmd_configure_toe(nx_dev->rx_ctxs[0], nx_dev->pci_func, 
				NX_CDRP_TOE_UNINIT);
}


/*
 * Gets the port number of the device on a netxen adaptor if it is a netxen
 * device.
 *
 * Parameters:
 *	dev	- The device for which the port is requested.
 *
 * Returns:
 *	-1	- If not netxen device.
 *	port number on the adapter if it is netxen device.
 */
int nx_nic_get_device_port(struct net_device *netdev)
{
	struct unm_adapter_s *adapter = (unm_adapter *) netdev_priv(netdev);

	if (netdev->do_ioctl != unm_nic_ioctl) {
		return (-1);
	}

	if (NX_IS_REVISION_P2(adapter->ahw.revision_id))
		return ((int)adapter->physical_port);
	else
		return ((int)adapter->portnum);
}

/*
 * Gets the ring context used by the device to talk to the card.
 *
 * Parameters:
 *	dev	- The device for which the ring context is requested.
 *
 * Returns:
 *	-EINVAL	- If not netxen device.
 *	Ring context number if it is netxen device.
 */
int nx_nic_get_device_ring_ctx(struct net_device *netdev)
{
	struct unm_adapter_s *adapter;

	if (netdev->do_ioctl != unm_nic_ioctl) {
		return (-EINVAL);
	}

	adapter = netdev_priv(netdev);
	/*
	 * TODO: send the correct context.
	 */
	return (adapter->portnum);
}

/*
 * Gets the pointer to pci device if the device maps to a netxen device.
 */
struct pci_dev *nx_nic_get_pcidev(struct net_device *dev)
{
	if (netdev_priv(dev)) {
		return ((struct unm_adapter_s *)(netdev_priv(dev)))->pdev;
	}
	return (NULL);
}

/*
 * Returns the revision id of card.
 */
int nx_nic_get_adapter_revision_id(struct net_device *dev)
{
	if (netdev_priv(dev)) {
		return ((struct unm_adapter_s *)(netdev_priv(dev)))->ahw.revision_id;
	}
	return -1;
}

/*
 * Send a group of cmd descs to the card.
 * Used for sending tnic message or nic notification.
 */
int nx_nic_send_cmd_descs(struct net_device *dev,
			  cmdDescType0_t *cmd_desc_arr, int nr_elements)
{
	uint32_t producer;
	struct unm_cmd_buffer *pbuf;
	cmdDescType0_t *cmd_desc;
	int i;
	int rv;
	unm_adapter *adapter = NULL;

	if (dev == NULL) {
                nx_nic_print4(NULL, "%s: Device is NULL\n", __FUNCTION__);
		return (-1);
	}

	adapter = (unm_adapter *) netdev_priv(dev);

#if defined(DEBUG)
        if (adapter == NULL) {
                nx_nic_print4(adapter, "%s Adapter not initialized, cannot "
			      "send request to card\n", __FUNCTION__);
                return (-1);
        }
#endif /* DEBUG */
	if (adapter->cmd_buf_arr == NULL ) {
		nx_nic_print4(adapter, "Command Ring not Allocated");
		return -1;
	}
	if (nr_elements > MAX_PENDING_DESC_BLOCK_SIZE || nr_elements == 0) {
		nx_nic_print4(adapter, "%s: Too many command descriptors in a "
			      "request\n", __FUNCTION__);
		return (-EINVAL);
	}

	i = 0;

	spin_lock_bh(&adapter->tx_lock);

	/* check if space is available */
	if (unm_get_pending_cmd_desc_cnt(&adapter->pending_cmds) ||
	    ((adapter->cmdProducer + nr_elements) >=
	     ((adapter->lastCmdConsumer <= adapter->cmdProducer) ?
	      adapter->lastCmdConsumer + adapter->MaxTxDescCount :
	      adapter->lastCmdConsumer))) {

		rv = nx_queue_pend_cmd_desc_list(adapter, cmd_desc_arr,
						 nr_elements);
		spin_unlock_bh(&adapter->tx_lock);
		return (rv);
	}

/*         adapter->cmdProducer = get_index_range(adapter->cmdProducer, */
/* 					       MaxTxDescCount, nr_elements); */

	producer = adapter->cmdProducer;
	do {
		cmd_desc = &cmd_desc_arr[i];

		pbuf = &adapter->cmd_buf_arr[producer];
		pbuf->mss = 0;
		pbuf->totalLength = 0;
		pbuf->skb = NULL;
		pbuf->cmd = 0;
		pbuf->fragCount = 0;
		pbuf->port = 0;

		/* adapter->ahw.cmdDescHead[producer] = *cmd_desc; */
		adapter->ahw.cmdDescHead[producer].word0 = cmd_desc->word0;
		adapter->ahw.cmdDescHead[producer].word1 = cmd_desc->word1;
		adapter->ahw.cmdDescHead[producer].word2 = cmd_desc->word2;
		adapter->ahw.cmdDescHead[producer].word3 = cmd_desc->word3;
		adapter->ahw.cmdDescHead[producer].word4 = cmd_desc->word4;
		adapter->ahw.cmdDescHead[producer].word5 = cmd_desc->word5;
		adapter->ahw.cmdDescHead[producer].word6 = cmd_desc->word6;
		adapter->ahw.cmdDescHead[producer].unused = cmd_desc->unused;

		producer = get_next_index(producer, adapter->MaxTxDescCount);
		i++;

	} while (i != nr_elements);

	adapter->cmdProducer = producer;
	unm_nic_update_cmd_producer(adapter, producer);

	spin_unlock_bh(&adapter->tx_lock);

	return (0);
}

/*
 * Send a TNIC message to the card.
 */
int nx_nic_send_msg_to_fw(struct net_device *dev,
			 pegnet_cmd_desc_t *cmd_desc_arr,
			 int nr_elements)
{
	return nx_nic_send_cmd_descs(dev, (cmdDescType0_t *)cmd_desc_arr,
				     nr_elements);
}

/*
 * Send a TNIC message to the card using pexq.
 */
int nx_nic_send_msg_to_fw_pexq(struct net_device *dev,
			 pegnet_cmd_desc_t *cmd_desc_arr,
			 int nr_elements)
{
        /* TODO: We need to make sure that we have enough slots available. */
	struct unm_adapter_s *adapter = netdev_priv(dev);
        nx_pexq_dbell_t      *pexq = &adapter->pexq;
        unm_msg_t            *msg = (unm_msg_t *)cmd_desc_arr;
        int                  rv;

        rv = nx_schedule_pexqdb(pexq, msg, nr_elements);
        if (rv != NX_RCODE_SUCCESS) {
                nx_nic_print7(adapter, "%s: Return from "
                                        "nx_schedule_pexqdb() = %d\n", 
                                        __FUNCTION__, rv);
                return rv;
        } 
        return NX_RCODE_SUCCESS;
}

/* Process Command status ring */
static int unm_process_cmd_ring(unsigned long data)
{
	uint32_t lastConsumer;
	uint32_t consumer;
	unm_adapter *adapter = (unm_adapter *) data;
	int count = 0;
	struct unm_cmd_buffer *buffer;
	struct pci_dev *pdev;
	struct unm_skb_frag *frag;
	struct sk_buff *skb = NULL;
	int done;

	lastConsumer = adapter->lastCmdConsumer;
	consumer = *(adapter->cmdConsumer);
	while (lastConsumer != consumer) {
		buffer = &adapter->cmd_buf_arr[lastConsumer];
		pdev = adapter->pdev;
		frag = &buffer->fragArray[0];
		skb = buffer->skb;
		if (skb && (cmpxchg(&buffer->skb, skb, 0) == skb)) {
			uint32_t i;

			nx_free_frag_bounce_buf(adapter, frag);

			pci_unmap_single(pdev, frag->dma, frag->length,
					PCI_DMA_TODEVICE);
			for (i = 1; i < buffer->fragCount; i++) {
				nx_nic_print7(adapter, "get fragment no %d\n",
						i);
				frag++;	/* Get the next frag */
				nx_free_frag_bounce_buf(adapter, frag);
				pci_unmap_page(pdev, frag->dma, frag->length,
						PCI_DMA_TODEVICE);
			}
			adapter->stats.xmitfinished++;
			dev_kfree_skb_any(skb);
			skb = NULL;
		}

		lastConsumer = get_next_index(lastConsumer,
					      adapter->MaxTxDescCount);

		if (++count >= MAX_STATUS_HANDLE)
			break;
	}

	if (count) {
		adapter->lastCmdConsumer = lastConsumer;
		smp_mb();
		if(netif_queue_stopped(adapter->netdev)
			&& netif_running(adapter->netdev)) {
				spin_lock(&adapter->tx_lock);
				adapter->tx_timeout_count = 0;
				netif_wake_queue(adapter->netdev);
				spin_unlock(&adapter->tx_lock);
		}
	}

	/*
	 * If everything is freed up to consumer then check if the ring is full
	 * If the ring is full then check if more needs to be freed and
	 * schedule the call back again.
	 *
	 * This happens when there are 2 CPUs. One could be freeing and the
	 * other filling it. If the ring is full when we get out of here and
	 * the card has already interrupted the host then the host can miss the
	 * interrupt.
	 *
	 * There is still a possible race condition and the host could miss an
	 * interrupt. The card has to take care of this.
	 */
	consumer = *(adapter->cmdConsumer);
	done = (adapter->lastCmdConsumer == consumer);

	return (done);
}


/*
 *
 */
static int nx_alloc_rx_skb(struct unm_adapter_s *adapter,
			   nx_host_rds_ring_t *nxhal_rds_ring,
			   struct unm_rx_buffer *buffer)
{
	struct sk_buff *skb;
	dma_addr_t dma;
	rds_host_ring_t *host_rds_ring = NULL;

	host_rds_ring = (rds_host_ring_t *) nxhal_rds_ring->os_data;
	skb = ALLOC_SKB(adapter, nxhal_rds_ring->buff_size, GFP_ATOMIC);

	NX_NIC_HANDLE_HIGHDMA_OVERFLOW(adapter, skb);

	if (unlikely(!skb)) {
		/* the caller should update the correct counter so the
		   alloc failure is not lost */
		return (-ENOMEM);
	}
#if defined(XGB_DEBUG)
	*(unsigned long *)(skb->head) = 0xc0debabe;
	if (skb_is_nonlinear(skb)) {
		nx_nic_print3(adapter, "Allocated SKB %p is nonlinear\n", skb);
	}
#endif

	if (!adapter->ahw.cut_through) {
		skb_reserve(skb, IP_ALIGNMENT_BYTES);
	}

#ifndef ESX_3X
	/*
	 * Adjust the truesize by the delta. We allocate more in some cases
	 * like Jumbo.
	 */
	skb->truesize -= host_rds_ring->truesize_delta;
#endif

	/* This will be setup when we receive the
	 * buffer after it has been filled  FSL  TBD TBD
	 * skb->dev = netdev;
	 */
	if (try_map_skb_data(adapter, skb, host_rds_ring->dma_size,
				     PCI_DMA_FROMDEVICE, &dma)) {
		dev_kfree_skb_any(skb);
		return (-ENOMEM);
	}

	buffer->skb = skb;
	buffer->state = UNM_BUFFER_BUSY;
	buffer->dma = dma;

	return (0);
}

/**
 * unm_post_rx_buffers puts buffer in the Phantom memory
 **/
int unm_post_rx_buffers(struct unm_adapter_s *adapter,
			nx_host_rx_ctx_t *nxhal_rx_ctx ,uint32_t ringid)
{
	rds_host_ring_t *host_rds_ring = NULL;
	nx_host_rds_ring_t *nxhal_rds_ring = NULL;
	nx_free_rbufs_t free_list;


	INIT_LIST_HEAD(&free_list.head);
	nxhal_rds_ring = &nxhal_rx_ctx->rds_rings[ringid];
	host_rds_ring = (rds_host_ring_t *) nxhal_rds_ring->os_data;

	spin_lock_bh(&adapter->buf_post_lock);
	if (host_rds_ring->posting
	    || list_empty(&host_rds_ring->free_rxbufs.head)) {
		spin_unlock_bh(&adapter->buf_post_lock);
		return (0);
	}

	list_replace_init(&host_rds_ring->free_rxbufs.head, &free_list.head);
	free_list.count = host_rds_ring->free_rxbufs.count;
	host_rds_ring->free_rxbufs.count = 0;

	host_rds_ring->posting = 1;
	spin_unlock_bh(&adapter->buf_post_lock);

	return (nx_post_rx_descriptors(adapter, nxhal_rds_ring, ringid,
				       &free_list));
}

#if	(!defined(ESX) && defined(CONFIG_PM))
static inline void nx_pci_save_state(struct pci_dev *pdev,
				     struct unm_adapter_s *adapter)
{
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,9)
	pci_save_state(pdev, adapter->pm_state);
#else
	pci_save_state(pdev);
#endif
}

static inline void nx_pci_restore_state(struct pci_dev *pdev,
					struct unm_adapter_s *adapter)
{
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,9)
	pci_restore_state(pdev, adapter->pm_state);
#else
	pci_restore_state(pdev);
#endif
}

static inline int nx_pci_choose_state(struct pci_dev *pdev, PM_MESSAGE_T state)
{
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,10)
	return (PCI_D0);
#else
	return (pci_choose_state(pdev, state));
#endif
}

static int unm_nic_suspend(struct pci_dev *pdev, PM_MESSAGE_T state)
{
	struct net_device *netdev = pci_get_drvdata(pdev);
	struct unm_adapter_s *adapter = netdev_priv(netdev);
	struct ethtool_wolinfo wol;

//	nx_nic_print3(adapter, "Suspend called\n");
	netif_device_detach(netdev);

	if (netif_running(netdev)) {
		unm_nic_down(netdev);
//		adapter->state = PORT_SUSPEND;
	}

	unm_nic_detach(adapter, NX_DESTROY_CTX_RESET);

	nx_pci_save_state(pdev, adapter);

	/*
	 * Need to check if WOL is enabled first.
	 */
	unm_nic_get_wol(netdev, &wol);
	if (wol.wolopts & WAKE_MAGIC) {
		pci_enable_wake(pdev, PCI_D3hot, 1);
		pci_enable_wake(pdev, PCI_D3cold, 1);
	} else {
		pci_enable_wake(pdev, PCI_D3hot, 0);
		pci_enable_wake(pdev, PCI_D3cold, 0);
	}

	pci_disable_device(pdev);
        pci_set_power_state(pdev, nx_pci_choose_state(pdev, state));

	return 0;
}

static int unm_nic_resume(struct pci_dev *pdev)
{
	int	rv;
	struct net_device *netdev = pci_get_drvdata(pdev);
	struct unm_adapter_s *adapter = netdev_priv(netdev);

//	nx_nic_print3(adapter, "Resume called\n");
        pci_set_power_state(pdev, PCI_D0);
        nx_pci_restore_state(pdev, adapter);

	rv = pci_enable_device(pdev);
	if (rv) {
                nx_nic_print3(adapter, "Error[%d]: Cannot enable "
			      "PCI device.\n", rv);
		goto err_ret;
        }

	/*
	 * Set the CRB window to invalid. If any register in window 0 is
	 * accessed it should set the window to 0 and then reset it to 1.
	 */
	adapter->curr_window = 255;

	rv = nx_start_firmware(adapter);
	if (rv) {
                nx_nic_print3(adapter, "Error[%d]: FW start Failed\n", rv);
		goto err_ret;
	}

	if (adapter->state == PORT_UP) {
		rv = unm_nic_attach(adapter);
		if (rv) {
			nx_nic_print3(adapter, "Error[%d]: Attach FAILED\n",
				      rv);
			goto err_ret;
		}
		netif_device_attach(netdev);
	}

	return (0);

  err_ret:
//	unm_nic_down(netdev);
	adapter->state = PORT_DOWN;
	return rv;
}
#endif /* (!defined(ESX) && defined(CONFIG_PM)) */

static struct pci_driver unm_driver = {
	.name = unm_nic_driver_name,
	.id_table = unm_pci_tbl,
	.probe = unm_nic_probe,
	.remove = __devexit_p(unm_nic_remove),
#if	(!defined(ESX) && defined(CONFIG_PM))
	.suspend = unm_nic_suspend,
	.resume = unm_nic_resume
#endif
};

/* Driver Registration on UNM card    */
static int __init unm_init_module(void)
{
	int err;

	if (!VMK_SET_MODULE_VERSION(unm_nic_driver_string)) {
		return -ENODEV;
	}
	err = unm_init_proc_drv_dir();
	if (err) {
		return err;
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
	if ((unm_workq = create_singlethread_workqueue("unm")) == NULL) {
		unm_cleanup_proc_drv_entries();
		return (-ENOMEM);
	}
#endif

	nx_verify_module_params();

	err = PCI_MODULE_INIT(&unm_driver);
	
	if (err) {

		unm_cleanup_proc_drv_entries();
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
		destroy_workqueue(unm_workq);
#endif
		return err;
	}
#ifndef ESX
		register_inetaddr_notifier(&nx_nic_inetaddr_cb);
		register_netdevice_notifier(&nx_nic_netdev_cb);
#endif
	return err;
}

static void __exit unm_exit_module(void)
{
        /*
         * Wait for some time to allow the dma to drain, if any.
         */
        mdelay(5);

        pci_unregister_driver(&unm_driver);

#ifndef ESX
	unregister_inetaddr_notifier(&nx_nic_inetaddr_cb);
	unregister_netdevice_notifier(&nx_nic_netdev_cb);
#endif

	unm_cleanup_proc_drv_entries();
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
	destroy_workqueue(unm_workq);
#endif
}

void nx_nic_get_lsa_version_number(struct net_device *netdev,
				   nic_version_t *version)
{
	struct unm_adapter_s *adapter = (struct unm_adapter_s *)netdev_priv(netdev);

        version->major = NXRD32(adapter, UNM_TCP_FW_VERSION_MAJOR_ADDR);

        version->minor = NXRD32(adapter, UNM_TCP_FW_VERSION_MINOR_ADDR);

        version->sub = NXRD32(adapter, UNM_TCP_FW_VERSION_SUB_ADDR);
}

static void nx_nic_get_nic_version_number(struct net_device *netdev,
					  nic_version_t *version)
{
	struct unm_adapter_s *adapter = (struct unm_adapter_s *)netdev_priv(netdev);

        version->major = NXRD32(adapter, UNM_FW_VERSION_MAJOR);

        version->minor = NXRD32(adapter, UNM_FW_VERSION_MINOR);

        version->sub = NXRD32(adapter, UNM_FW_VERSION_SUB);

}

static uint64_t nx_nic_get_fw_capabilities(struct net_device *netdev)
{
	struct unm_adapter_s *adapter = (struct unm_adapter_s *)netdev_priv(netdev);

	return (adapter->ahw.fw_capabilities_1);
}

int nx_nic_get_linkevent_cap(struct unm_adapter_s *adapter)
{
	return (nx_nic_get_fw_capabilities(adapter->netdev) & 
			NX_FW_CAPABILITY_LINK_NOTIFICATION);
}


/* define to disable pexq. */
//#define DONT_USE_PEXQ

int nx_nic_get_pexq_cap(struct net_device *netdev)
{
#if ((!defined(writeq)) || defined(DONT_USE_PEXQ))
        return 0;
#else
        return ((nx_nic_get_fw_capabilities(netdev) & NX_FW_CAPABILITY_PEXQ)
                     ? 1 : 0);
#endif
}

/*
 * Return the NIC API structure.
 *
 */
nx_nic_api_t *nx_nic_get_api(void)
{
        return &nx_nic_api_struct;
}

EXPORT_SYMBOL(nx_nic_get_api);

#ifndef ESX

enum {
	NX_NETDEV_ADD,
	NX_NETDEV_REMOVE,
	NX_NETDEV_UP,
	NX_NETDEV_DOWN,
};


static int nx_config_ipaddr(struct unm_adapter_s *adapter, __uint32_t ip,
			    int cmd)
{
	nic_request_t req;

	req.opcode = NX_NIC_HOST_REQUEST;
	req.body.cmn.req_hdr.opcode = NX_NIC_H2C_OPCODE_CONFIG_IPADDR;
	req.body.cmn.req_hdr.comp_id = 0;
	req.body.cmn.req_hdr.ctxid = adapter->portnum;
	req.body.ipaddr_config.ipaddr.v4 = ip;
	req.body.ipaddr_config.cmd    = cmd;

	nx_nic_print6(adapter, "%s IP 0x%x \n",
		      (cmd == NX_NETDEV_UP) ? "Adding" : "Removing",
		      req.body.ipaddr_config.ipaddr.v4);

	return (nx_nic_send_cmd_descs(adapter->netdev,
				      (cmdDescType0_t *)&req, 1));

}

static int nx_nic_inetaddr_event(struct notifier_block *this,
				 unsigned long event, void *ptr)
{
	struct in_ifaddr        *ifa = (struct in_ifaddr *)ptr;
	struct net_device       *dev;
	struct net_device	*real_dev = NULL;
	int    port;
	struct unm_adapter_s 	*adapter;

	dev = ifa->ifa_dev ? ifa->ifa_dev->dev : NULL;
	if (dev == NULL) {
		goto done;
	}

	if (! netif_running(dev)) {
		goto done;
	}

	if (dev->priv_flags & IFF_802_1Q_VLAN) {
		real_dev = VLAN_DEV_INFO(dev)->real_dev;
	} else {
		real_dev = dev;
	}

	port = nx_nic_get_device_port(real_dev);
	if (port < 0) {
		goto done;
	}

	adapter = (struct unm_adapter_s *)netdev_priv(real_dev);
	if (!adapter) {
		goto done;
	}

	if(adapter->is_up != ADAPTER_UP_MAGIC || adapter->ahw.cut_through ||
	   NX_IS_REVISION_P2(adapter->ahw.revision_id)) {
		goto done;
	}

	switch(event) {

	case NETDEV_UP:
		nx_config_ipaddr(adapter, ifa->ifa_address, NX_NETDEV_UP);
		break;

	case NETDEV_DOWN:
		nx_config_ipaddr(adapter, ifa->ifa_address, NX_NETDEV_DOWN);
		break;

	default:
		break;
	}
done:
	return NOTIFY_DONE;
}

static int nx_nic_netdev_event(struct notifier_block *this,
			       unsigned long event, void *ptr)
{
	struct net_device       *dev = (struct net_device *)ptr;
	struct net_device	*real_dev = NULL;
	struct in_device * indev;
	struct unm_adapter_s *adapter;
	int port;

	if (dev == NULL) {
		goto done;
	}

	indev = dev->ip_ptr;
	if (!indev) {
		goto done;
	}

	if (dev->priv_flags & IFF_802_1Q_VLAN) {
		real_dev = VLAN_DEV_INFO(dev)->real_dev;
	} else {
		real_dev = dev;
	}

	port = nx_nic_get_device_port(real_dev);
	if (port < 0) {
		goto done;
	}

	NX_SET_VLAN_DEV_FEATURES(real_dev, dev);

	adapter = (struct unm_adapter_s *)netdev_priv(real_dev);
	if (!adapter) {
		goto done;
	}

	if(adapter->is_up != ADAPTER_UP_MAGIC || adapter->ahw.cut_through ||
	   NX_IS_REVISION_P2(adapter->ahw.revision_id)) {
		goto done;
	}

	for_ifa(indev) {
		switch (event) {
		case NETDEV_UP:
			nx_config_ipaddr(adapter, ifa->ifa_address,
					 NX_NETDEV_UP);
			break;
		case NETDEV_DOWN:
			nx_config_ipaddr(adapter, ifa->ifa_address,
					 NX_NETDEV_DOWN);
			break;
		default:
			break;
		}
	} endfor_ifa(indev);
done:
	return NOTIFY_DONE;
}
#endif

#if 0
/* Bridging perf mode enable/disable
 */
static int nic_config_bridging(struct unm_adapter_s *adapter,
			int enable)
{
	nic_request_t req;

	req.opcode = NX_NIC_HOST_REQUEST;
	req.body.cmn.req_hdr.opcode = NX_NIC_H2C_OPCODE_CONFIG_BRIDGING;
	req.body.cmn.req_hdr.comp_id = 0;
	req.body.cmn.req_hdr.ctxid = adapter->portnum;

	req.body.config_bridging.enable = enable;

	return (nx_nic_send_cmd_descs(adapter->netdev,
				      (cmdDescType0_t *)&req, 1));
}
#endif

/* async_enable - enable notifications of link event changes
 * linkevent_request - get link parameters
 */
static int nic_linkevent_request(struct unm_adapter_s *adapter,
			int async_enable, int linkevent_request)
{
	nic_request_t req;

	req.opcode = NX_NIC_HOST_REQUEST;
	req.body.cmn.req_hdr.opcode = NX_NIC_H2C_OPCODE_GET_LINKEVENT;
	req.body.cmn.req_hdr.comp_id = 0;
	req.body.cmn.req_hdr.ctxid = adapter->portnum;

	req.body.linkevent_request.notification_enable = async_enable;
	req.body.linkevent_request.get_linkevent = linkevent_request;

	return (nx_nic_send_cmd_descs(adapter->netdev,
				      (cmdDescType0_t *)&req, 1));
}

/* This is either a async notification or a response on a linkevent
 * request. valid_mask indicates fields that are populated.
 */
static void nic_handle_linkevent_response(nx_dev_handle_t drv_handle,
				nic_response_t *resp)
{
	struct unm_adapter_s *adapter = (struct unm_adapter_s*)drv_handle;
	struct net_device *netdev = adapter->netdev;
	nic_linkevent_response_t *linkevent;

	linkevent = &resp->body.linkevent_response;

	nx_nic_print6(adapter, "module %d speed %d status %d "
				"cable_len %d OUI 0x%x full_duplex %d\n",
					linkevent->module,
					linkevent->link_speed,
					linkevent->link_status,
					linkevent->cable_len,
					linkevent->cable_OUI,
					linkevent->full_duplex);

	if (linkevent->module == 
			LINKEVENT_MODULE_TWINAX_UNSUPPORTED_CABLE) {

		nx_nic_print2(adapter, "Unsupported cable, OUI 0x%x "
				"Length %d\n",
				linkevent->cable_OUI,
				linkevent->cable_len);

	} else if (linkevent->module == 
			LINKEVENT_MODULE_TWINAX_UNSUPPORTED_CABLELEN) {

		nx_nic_print2(adapter, "Unsupported cable length: %d, "
				"OUI 0x%x\n",
				linkevent->cable_len,
				linkevent->cable_OUI);
	}

	/* f/w cannot send anymore events */
	if ( ! nx_nic_get_linkevent_cap(adapter)) {
		return;
	}

	adapter->link_speed = linkevent->link_speed;
	if (linkevent->full_duplex == LINKEVENT_FULL_DUPLEX) {
		adapter->link_duplex = DUPLEX_FULL;
	} else {
		adapter->link_duplex = DUPLEX_HALF;
	}
	adapter->link_autoneg = linkevent->link_autoneg;
	adapter->link_module_type = linkevent->module;

	if (adapter->ahw.linkup != linkevent->link_status) {
		adapter->ahw.linkup = linkevent->link_status;
		if (adapter->ahw.linkup == 0) {
			nx_nic_print3(adapter, "NIC Link is down\n");
			netif_carrier_off(netdev);
			netif_stop_queue(netdev);

		} else {
			nx_nic_print3(adapter, "NIC Link is up\n");
			netif_carrier_on(netdev);
			netif_wake_queue(netdev);
		}
	}
}

#ifdef NX_FCOE_SUPPORT1

void nic_db_write(struct net_device *dev, u32 off, u64 val)
{
	struct unm_adapter_s *adapter =
		(struct unm_adapter_s *)netdev_priv(dev);
#if 0
	printk("writing 0x%llx to %p, off %d\n", val,
		(char *)(adapter->ahw.db_base + off), off);
#endif
	writeq(val, (char *)(adapter->ahw.db_base + off));
}

EXPORT_SYMBOL(nic_db_write);
#endif /* NX_FCOE_SUPPORT */

module_init(unm_init_module);
module_exit(unm_exit_module);
