/*
 * Copyright (C) 2003 - 2009 NetXen, Inc.
 * Copyright (C) 2009 - QLogic Corporation.
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
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/ipv6.h>
#include <linux/in.h>
#include <linux/skbuff.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/highmem.h>

#include "nx_errorcode.h"
#include "nxplat.h"
#include "nxhal_nic_interface.h"
#include "nxhal_nic_api.h"

#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/interrupt.h>
#include <linux/timer.h>

#include <linux/mm.h>

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

#include "addrmap.h"

#undef SINGLE_DMA_BUF
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

#if defined(__VMKLNX__) || LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
#define	TASK_PARAM		struct work_struct *
#define	NX_INIT_WORK(a, b, c)	INIT_WORK(a, b)
#else
#define	TASK_PARAM		unsigned long
#define	NX_INIT_WORK(a, b, c)	INIT_WORK(a, (void (*)(void *))b, c)
#endif

#include "unm_nic.h"

#define DEFINE_GLOBAL_RECV_CRB
#define COLLECT_FW_LOGS

#include "nic_phan_reg.h"

#include "nic_cmn.h"
#include "nx_license.h"
#include "nxhal.h"
#include "unm_nic_config.h"
#include "unm_nic_lro.h"

#include "unm_nic_hw.h"
#include "unm_version.h"
#include "unm_brdcfg.h"

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESCRIPTION);
MODULE_LICENSE("GPL");
MODULE_VERSION(UNM_NIC_LINUX_VERSIONID);

static int use_msi = 1;
static int rss_enable = 0;
static int use_msi_x = 1;
static int tx_desc		= NX_MAX_CMD_DESCRIPTORS;
static int lro_desc		= MAX_LRO_RCV_DESCRIPTORS;
static int rdesc_1g		= NX_DEFAULT_RDS_SIZE_1G;
static int rdesc_10g		= NX_DEFAULT_RDS_SIZE;
static int jumbo_desc_1g	= NX_DEFAULT_JUMBO_RDS_SIZE_1G;
static int jumbo_desc		= NX_DEFAULT_JUMBO_RDS_SIZE;
static int rx_chained		= 0;

static int multi_ctx		= 1;
/*Initialize #of netqs with invalid value to know if user has configured or not */
static int num_rx_queues = INVALID_INIT_NETQ_RX_CONTEXTS;
static int md_capture_mask     = NX_NIC_MD_DEFAULT_CAPTURE_MASK;
static int md_enable           = -1;

static int lro                  = 0;

static int hw_vlan                  = 0;
static int tso                  	= 1;

static int collect_fw_logs = 1;
module_param(collect_fw_logs, bool, S_IRUGO);
MODULE_PARM_DESC(collect_fw_logs, "Collect debug logs after a adapter hang. Enable=1 ,Disable=0, Default=1");

static int fw_load = LOAD_FW_WITH_COMPARISON;
module_param(fw_load, int, S_IRUGO);
MODULE_PARM_DESC(fw_load, "DEBUG ONLY - Load firmware from file system or flash");

static int port_mode = UNM_PORT_MODE_AUTO_NEG;	// Default to auto-neg. mode
module_param(port_mode, int, S_IRUGO);
MODULE_PARM_DESC(port_mode, "Ports operate in XG, 1G or Auto-Neg mode");

static int wol_port_mode         = 5; // Default to restricted 1G auto-neg. mode
module_param(wol_port_mode, int, S_IRUGO);
MODULE_PARM_DESC(wol_port_mode, "In wol mode, ports operate in XG, 1G or Auto-Neg");

module_param(use_msi, bool, S_IRUGO);
MODULE_PARM_DESC(use_msi, "Enable or Disable MSI");

module_param(use_msi_x, bool, S_IRUGO);
MODULE_PARM_DESC(use_msi_x, "Enable or Disable MSI-X");

module_param(rss_enable, bool, S_IRUGO);
MODULE_PARM_DESC(rss_enable, "Enable or Disable RSS");

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

module_param(multi_ctx, int, S_IRUGO);
MODULE_PARM_DESC(multi_ctx, "Enable/disable mutlple context support Default:1, Enable=1 ,Disable=0");
module_param(num_rx_queues, int, S_IRUGO);
MODULE_PARM_DESC(num_rx_queues, "Number of RX queues per PCI FN Default:15, Min:0, Max:15 ");

char unm_nic_driver_name[] = DRIVER_NAME;

#if 0
module_param(lro, bool, S_IRUGO);
MODULE_PARM_DESC(lro, "Enable/disable LRO");
#endif

module_param(hw_vlan, bool, S_IRUGO);
MODULE_PARM_DESC(hw_vlan, "Enable/disable VLAN OFFLOAD TO HW. Default:0, Enable:1, Disable=0");

module_param(tso, bool, S_IRUGO);
MODULE_PARM_DESC(tso, "Enable/disable TSO. Default:1, Enable:1, Disable=0");

module_param(md_capture_mask, int, S_IRUGO);
MODULE_PARM_DESC(md_capture_mask, "Capture mask for collection of firmware dump,"
       "Default:0x7F, Min:0x03, Max:0xFF(Valid values : 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F, 0xFF");

module_param(md_enable, bool, S_IRUGO);
MODULE_PARM_DESC(md_enable, "Enable/disable firmware minidump "
        "support. Default:1, Enable=1 ,Disable=0");

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

#define NX_WATCHDOG_LINK_THRESHOLD 2
/* Extern definition required for vmkernel module */

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
static int nx_nic_fw40_change_mtu(struct net_device *netdev, int new_mtu);
static int receive_peg_ready(struct unm_adapter_s *adapter);
static int unm_nic_hw_resources(struct unm_adapter_s *adapter);
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

static int unm_nic_new_rx_context_prepare(struct unm_adapter_s *adapter, nx_host_rx_ctx_t *nxhal_rx_ctx);
static int unm_nic_new_tx_context_prepare(struct unm_adapter_s *adapter);
static int unm_nic_new_tx_context_destroy(struct unm_adapter_s *adapter, int ctx_destroy);
static int unm_nic_new_rx_context_destroy(struct unm_adapter_s *adapter, int ctx_destroy);
static void nx_nic_free_hw_rx_resources(struct unm_adapter_s *adapter, nx_host_rx_ctx_t *nxhal_rx_ctx);
static void nx_nic_free_host_rx_resources(struct unm_adapter_s *adapter, nx_host_rx_ctx_t *nxhal_rx_ctx);
static void nx_nic_free_host_sds_resources(struct unm_adapter_s *adapter,
                nx_host_rx_ctx_t *nxhal_host_rx_ctx);
int nx_nic_create_rx_ctx(struct net_device *netdev);
int nx_nic_multictx_free_rx_ctx(struct net_device *netdev, int ctx_id);

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
static uint32_t unm_process_rcv_ring(struct unm_adapter_s *,
				     nx_host_sds_ring_t *nxhal_sds_ring, int);
static struct net_device_stats *unm_nic_get_stats(struct net_device *netdev);

static int nx_nic_poll_sts(struct napi_struct *napi, int work_to_do);

#ifdef CONFIG_NET_POLL_CONTROLLER
static void unm_nic_poll_controller(struct net_device *netdev);
#endif

static inline void nx_nic_check_tso(struct unm_adapter_s *adapter, 
		cmdDescType0_t * desc, uint32_t tagged, struct sk_buff *skb);
static inline void nx_nic_tso_copy_headers(struct unm_adapter_s *adapter,
		struct sk_buff *skb, uint32_t *prod, uint32_t saved);

#if !defined(__VMKLNX__) && LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
static irqreturn_t nx_nic_legacy_intr(int irq, void *data, struct pt_regs *regs);
static irqreturn_t nx_nic_msi_intr(int irq, void *data, struct pt_regs *regs);
static irqreturn_t nx_nic_msix_intr(int irq, void *data, struct pt_regs *regs);
#else
static irqreturn_t nx_nic_legacy_intr(int irq, void *data);
static irqreturn_t nx_nic_msi_intr(int irq, void *data);
static irqreturn_t nx_nic_msix_intr(int irq, void *data);
#endif

extern void set_ethtool_ops(struct net_device *netdev);

extern int unm_init_proc_drv_dir(void);
extern void unm_cleanup_proc_drv_entries(void);
extern void unm_init_proc_entries(struct unm_adapter_s *adapter);
extern void unm_cleanup_proc_entries(struct unm_adapter_s *adapter);

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
static void nx_synchronize_irq(struct unm_adapter_s *adapter, nx_host_rx_ctx_t *nxhal_host_rx_ctx);

int nx_nic_is_netxen_device(struct net_device *netdev);
static void nx_nic_get_nic_version_number(struct net_device *netdev,
					  nic_version_t *version);
struct proc_dir_entry *nx_nic_get_base_procfs_dir(void);
static int nic_linkevent_request(struct unm_adapter_s *adapter,
			int async_enable, int linkevent_request);
static void nic_handle_linkevent_response(nx_dev_handle_t drv_handle,
				nic_response_t *resp);
int nx_nic_get_linkevent_cap(struct unm_adapter_s *adapter);
void nx_nic_halt_firmware(struct unm_adapter_s *adapter);
int nx_nic_minidump(struct unm_adapter_s *adapter);

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

	if ((*vaddr == NULL)) {
		return NX_RCODE_NO_HOST_MEM;
	}

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
#if 0
struct unm_adapter_s *adapter
adapter = 
(struct unm_adapter_s *)
drv_handle

	uint64_t cur_fn = (uint64_t) ;

#endif


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

	DEFINE_WAIT(wq_entry);

	while (wait->trigger == 0) {
		if (utimelimit <= 0) {
			nx_nic_print6(adapter, "%s: timelimit up\n", __FUNCTION__);
			rv = NX_RCODE_TIMEOUT;
			break;
		}
		PREPARE_TO_WAIT(&wait->wq, &wq_entry, TASK_INTERRUPTIBLE);
		/* schedule out for 100ms */
		msleep(100);
		utimelimit -= (100000);
	}

	index   = (wait->comp_id & 0xC0) >> 6;
	bit_map = (uint64_t)(1 << (((uint64_t)wait->comp_id) & 0x3F));
	adapter->wait_bit_map[index] &= ~bit_map;

	finish_wait(&wait->wq, &wq_entry);
	list_del(&wait->list);
	
	return rv;
}

nx_rcode_t nx_nic_handle_fw_response(nx_dev_handle_t drv_handle,
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

	wake_up_interruptible(&wait->wq);

	return NX_RCODE_SUCCESS;
}

void nx_init_napi (struct unm_adapter_s *adapter)
{

	int num_sds_rings = adapter->num_sds_rings;
	int ring;

	for(ring = 0; ring < num_sds_rings; ring++) {
		netif_napi_add(adapter->netdev,
				&adapter->host_sds_rings[ring].napi,
				nx_nic_poll_sts,
				adapter->MaxRxDescCount >> 2);
	}
}
/*
 * Function to enable napi interface for all rss rings
 */

void nx_napi_enable(struct unm_adapter_s *adapter)
{

	int num_sds_rings = adapter->num_sds_rings;
	int ring;
	uint64_t cur_fn = (uint64_t) nx_napi_enable;

	NX_NIC_TRC_FN(adapter, cur_fn, 0);

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

static int nx_alloc_adapter_sds_rings(struct unm_adapter_s *adapter) 
{
	sds_host_ring_t *host_ring;
	uint64_t cur_fn = (uint64_t) nx_alloc_adapter_sds_rings;

	NX_NIC_TRC_FN(adapter, cur_fn, 0);

	if (adapter->multi_ctx) {
		adapter->num_sds_rings  = adapter->num_rx_queues + 1; 
	} else {
		adapter->num_sds_rings = adapter->max_possible_rss_rings;
	}
	NX_NIC_TRC_FN(adapter, cur_fn, adapter->num_sds_rings);

	host_ring = kmalloc(sizeof(sds_host_ring_t) * adapter->num_sds_rings, 
			    GFP_KERNEL);

	if (host_ring == NULL) {
		nx_nic_print3(NULL, "Couldn't allocate memory for SDS ring\n");
		NX_NIC_TRC_FN(adapter, cur_fn, ENOMEM);
		return -ENOMEM;
	}

	memset(host_ring, 0, sizeof(sds_host_ring_t) * adapter->num_sds_rings);

	adapter->host_sds_rings = host_ring;

	NX_NIC_TRC_FN(adapter, cur_fn, 0);
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

	if( num_rx_queues < INVALID_INIT_NETQ_RX_CONTEXTS || num_rx_queues > NETQ_MAX_RX_CONTEXTS) {
		num_rx_queues = INVALID_INIT_NETQ_RX_CONTEXTS;
		nx_nic_print4(NULL, "Invalid value for num_rx_queues, valid value is "
				"between 0 and %d. Number of netqueues are calculated at run-time.\n", NETQ_MAX_RX_CONTEXTS);
	}

	if(md_enable < -1 || md_enable > 1) {
		md_enable = -1;
		nx_nic_print4(NULL, "Invalid value for md_enable, valid value is either 0 or 1\n ");
	}

}

/*
 *
 */
static void unm_check_options(unm_adapter *adapter)
{
	uint64_t cur_fn = (uint64_t) unm_check_options;
	uint16_t subsys_vendor_id = 0;
	adapter->MaxJumboRxDescCount = jumbo_desc;
	adapter->MaxLroRxDescCount = lro_desc;

	switch (adapter->ahw.boardcfg.board_type) {
	case UNM_BRDTYPE_P3_XG_LOM:
	case UNM_BRDTYPE_P3_HMEZ:

	case UNM_BRDTYPE_P3_10G_CX4:
	case UNM_BRDTYPE_P3_10G_CX4_LP:
	case UNM_BRDTYPE_P3_IMEZ:
	case UNM_BRDTYPE_P3_10G_SFP_PLUS:
	case UNM_BRDTYPE_P3_10G_XFP:
	case UNM_BRDTYPE_P3_10000_BASE_T:
		adapter->msix_supported = 1;
		adapter->max_possible_rss_rings = CARD_SIZED_MAX_RSS_RINGS;
		adapter->MaxRxDescCount = rdesc_10g;
		NX_NIC_TRC_FN(adapter, cur_fn, 1);
		break;

	case UNM_BRDTYPE_P3_REF_QG:
	case UNM_BRDTYPE_P3_4_GB:
	case UNM_BRDTYPE_P3_4_GB_MM:
		adapter->msix_supported = 1;
		adapter->max_possible_rss_rings = 1;
                adapter->MaxRxDescCount = rdesc_1g;
                adapter->MaxJumboRxDescCount = jumbo_desc_1g;
		NX_NIC_TRC_FN(adapter, cur_fn, 1);
		break;

	case UNM_BRDTYPE_P3_10G_TP:
		if (adapter->portnum < 2) {
			adapter->msix_supported = 1;
			adapter->max_possible_rss_rings =
				CARD_SIZED_MAX_RSS_RINGS;
			adapter->MaxRxDescCount = rdesc_10g;
			NX_NIC_TRC_FN(adapter, cur_fn, rdesc_10g);
		} else {
			adapter->msix_supported = 1;
			adapter->max_possible_rss_rings = 1;
			adapter->MaxRxDescCount = rdesc_1g;
			adapter->MaxJumboRxDescCount = jumbo_desc_1g;
			NX_NIC_TRC_FN(adapter, cur_fn, adapter->MaxRxDescCount);
		}
		break;
	default:
		adapter->msix_supported = 0;
		adapter->max_possible_rss_rings = 1;
		adapter->MaxRxDescCount = rdesc_1g;
		NX_NIC_TRC_FN(adapter, cur_fn, adapter->msix_supported);

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
		NX_NIC_TRC_FN(adapter, cur_fn, adapter->MaxTxDescCount);
	}

	/* If RSS is not enabled set rings to 1 */
	if (!rss_enable) {
		adapter->max_possible_rss_rings = 1;
		NX_NIC_TRC_FN(adapter, cur_fn, 1);
	}

	nx_nic_print6(NULL, "Maximum Rx Descriptor count: %d\n",
		      adapter->MaxRxDescCount);
	nx_nic_print6(NULL, "Maximum Tx Descriptor count: %d\n",
		      adapter->MaxTxDescCount);
	nx_nic_print6(NULL, "Maximum Jumbo Descriptor count: %d\n",
		      adapter->MaxJumboRxDescCount);
	nx_nic_print6(NULL, "Maximum LRO Descriptor count: %d\n",
		      adapter->MaxLroRxDescCount);

	if (!(md_capture_mask == 0x03 || md_capture_mask == 0x07 || md_capture_mask == 0x0F ||
                md_capture_mask == 0x1F || md_capture_mask == 0x3F || md_capture_mask == 0x7F ||
                md_capture_mask == 0xFF)) {

        md_capture_mask = NX_NIC_MD_DEFAULT_CAPTURE_MASK;
        dev_err(&adapter->pdev->dev, "Invalid vlaue for \"md_capture_mask\" module param, valid"
                "values are : 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F and 0xFF, setting to default value of 0x%x\n",
                NX_NIC_MD_DEFAULT_CAPTURE_MASK);
	}

    	adapter->mdump.md_capture_mask = md_capture_mask;

	if(md_enable == -1) {
		adapter->mdump.md_enabled = 1;

		pci_read_config_word(adapter->pdev, NX_PCI_SUBSYSTEM_VENDOR_ID, &subsys_vendor_id);
		/* Disable minidump feature for HP branded adapters */
		if(subsys_vendor_id == NX_HP_VENDOR_ID) {
			adapter->mdump.md_enabled = 0;
		}
	} else {
		adapter->mdump.md_enabled = md_enable;
	}

	if(adapter->portnum == 0) {
		adapter->fw_dmp.enabled = collect_fw_logs;
	}

	return;
}

#ifdef NETIF_F_TSO
#define TSO_SIZE(x)   ((x)->gso_size)
#endif //#ifdef NETIF_F_TSO

/*  PCI Device ID Table  */
#define NETXEN_PCI_ID(device_id)   PCI_DEVICE(PCI_VENDOR_ID_NX, device_id)

static struct pci_device_id unm_pci_tbl[] __devinitdata = {
	{NETXEN_PCI_ID(PCI_DEVICE_ID_NX_P3_XG)},
	{0,}
};

MODULE_DEVICE_TABLE(pci, unm_pci_tbl);

#define SUCCESS 0
static int get_flash_mac_addr(struct unm_adapter_s *adapter, uint64_t mac[])
{
	uint32_t *pmac = (uint32_t *) & mac[0];
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

/*
 * Initialize buffers required for the adapter in pegnet_nic case.
 */
static int initialize_dummy_dma(unm_adapter *adapter)
{
	uint64_t        addr;
	uint32_t        hi;
	uint32_t        lo;
	uint64_t cur_fn = (uint64_t) initialize_dummy_dma;

	/*
	 * Allocate Dummy DMA space.
	 */
	adapter->dummy_dma.addr = pci_alloc_consistent(adapter->ahw.pdev,
			UNM_HOST_DUMMY_DMA_SIZE,
			&adapter->dummy_dma.phys_addr);
	if (adapter->dummy_dma.addr == NULL) {
		nx_nic_print3(NULL, "ERROR: Could not allocate dummy "
				"DMA memory\n");
		NX_NIC_TRC_FN(adapter, cur_fn, -ENOMEM);
		return (-ENOMEM);
	}

	addr = (uint64_t) adapter->dummy_dma.phys_addr;
	hi = (addr >> 32) & 0xffffffff;
	lo = addr & 0xffffffff;

	NX_NIC_TRC_FN(adapter, cur_fn, addr);

	read_lock(&adapter->adapter_lock);
	NXWR32(adapter, CRB_HOST_DUMMY_BUF_ADDR_HI, hi);
	NXWR32(adapter, CRB_HOST_DUMMY_BUF_ADDR_LO, lo);

	NXWR32(adapter, CRB_HOST_DUMMY_BUF, DUMMY_BUF_INIT);

	read_unlock(&adapter->adapter_lock);

	NX_NIC_TRC_FN(adapter, cur_fn, 0);
	return (0);
}

/*
 * Free buffers for the offload part from the adapter.
 */
static void destroy_dummy_dma(unm_adapter *adapter)
{
	uint64_t cur_fn = (uint64_t) destroy_dummy_dma;
	NX_NIC_TRC_FN(adapter, cur_fn, 0);
	if (adapter->dummy_dma.addr) {
		NX_NIC_TRC_FN(adapter, cur_fn, adapter->dummy_dma.addr);
		pci_free_consistent(adapter->ahw.pdev, UNM_HOST_DUMMY_DMA_SIZE,
				    adapter->dummy_dma.addr,
				    adapter->dummy_dma.phys_addr);
		adapter->dummy_dma.addr = NULL;
	}
}

#define addr_needs_mapping(adapter, phys) \
        ((phys) & (~adapter->dma_mask))

static inline int
in_dma_range(struct unm_adapter_s *adapter, dma_addr_t addr, unsigned int len)
{
	dma_addr_t last = addr + len - 1;

	return !addr_needs_mapping(adapter, last);
}

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

static int nx_nic_setup_minidump(struct unm_adapter_s *adapter)
{
    int err = 0;
    u32 capture_size = 0; 
    ql_minidump_template_hdr_t *hdr; 
	uint64_t cur_fn = (uint64_t)nx_nic_setup_minidump;

        if (!adapter->mdump.md_enabled) {
			NX_NIC_TRC_FN(adapter, cur_fn, err);
                return err;
        }

    if ((err = unm_nic_get_minidump_template_size(adapter))) {
        if (err == NX_RCODE_CMD_NOT_IMPL) {
            adapter->mdump.fw_supports_md = 0;
			NX_NIC_TRC_FN(adapter, cur_fn, err);
            return 0;
        } else {
			NX_NIC_TRC_FN(adapter, cur_fn, err);
            return err;
        }
    }

    if (!adapter->mdump.md_template_size) {
        nx_nic_print3(NULL, "Error : Invalid minidump template size, should be nonzero.\n");
		NX_NIC_TRC_FN(adapter, cur_fn, -EIO);
        return -EIO;
    }

    adapter->mdump.md_template = kmalloc(adapter->mdump.md_template_size, GFP_KERNEL);
    if (!adapter->mdump.md_template) {
        nx_nic_print3(NULL, "Unable to allocate memory for minidump template.\n");
		NX_NIC_TRC_FN(adapter, cur_fn, -ENOMEM);
        return -ENOMEM;
    }

    if ((err = unm_nic_get_minidump_template(adapter))) {
        if (err == NX_RCODE_CMD_NOT_IMPL) {
            adapter->mdump.fw_supports_md = 0;
            err = 0;
        }

        goto free_template;
    }

    adapter->mdump.fw_supports_md = 1;

    hdr = (ql_minidump_template_hdr_t *) adapter->mdump.md_template;

    if (nx_nic_check_template_checksum(adapter)) {
        nx_nic_print3(NULL, "The minidump template checksum isn't matching.\n");
        err = -EIO;
		NX_NIC_TRC_FN(adapter, cur_fn, err);
        goto free_template;
    }

    capture_size = nx_nic_get_capture_size(adapter);

    if (!capture_size) {
        nx_nic_print3(NULL, "Invalid capture sizes in minidump template for "
            "capture_mask : 0x%x\n", adapter->mdump.md_capture_mask);
        err = -EINVAL;
		NX_NIC_TRC_FN(adapter, cur_fn, err);
        goto free_template;
    }

    adapter->mdump.md_capture_size = capture_size;
        /*
        Need to allocate memory including template size, since the final dump should
                have template at the start.
    */
    adapter->mdump.md_dump_size = adapter->mdump.md_template_size + capture_size;

        /*
                Memory for capture buff will be allocated before collecting the dump.
        */
        adapter->mdump.md_capture_buff = NULL;
        adapter->mdump.dump_needed = 1;

    return 0;

free_template:
    if (adapter->mdump.md_template) {
        kfree(adapter->mdump.md_template);
        adapter->mdump.md_template = NULL;
    }

	NX_NIC_TRC_FN(adapter, cur_fn, err);
    return err;
}


static void nx_nic_cleanup_minidump(struct unm_adapter_s *adapter)
{
	uint64_t cur_fn = (uint64_t)nx_nic_cleanup_minidump;

    if (adapter->mdump.md_template) {
        kfree(adapter->mdump.md_template);
        adapter->mdump.md_template = NULL;
    }

    if (adapter->mdump.md_capture_buff) {
        vfree(adapter->mdump.md_capture_buff);
        adapter->mdump.md_capture_buff = NULL;
    }
	NX_NIC_TRC_FN(adapter, cur_fn, 0);
}

#ifdef COLLECT_FW_LOGS 
static void nx_nic_logs_free (struct unm_adapter_s *adapter)
{
    struct fw_dump *fw_dmp = &adapter->fw_dmp;
    int i = 0;

    for (i = 0; i < NX_NIC_NUM_PEGS; i++) {
        if (fw_dmp->pcon[i])
            vfree(fw_dmp->pcon[i]);

        fw_dmp->pcon[i] = NULL;
    }

    if (fw_dmp->phanstat)
        vfree(fw_dmp->phanstat);

    fw_dmp->phanstat = NULL;

    if (fw_dmp->srestat)
        vfree(fw_dmp->srestat);

    fw_dmp->srestat = NULL;

    if (fw_dmp->epgstat)
        vfree(fw_dmp->epgstat);

    fw_dmp->epgstat = NULL;

    if (fw_dmp->nicregs)
        vfree(fw_dmp->nicregs);

    fw_dmp->nicregs = NULL;

    for (i = 0; i < UNM_NIU_MAX_XG_PORTS; i++) {
        if (fw_dmp->macstat[i])
            vfree(fw_dmp->macstat[i]);

        fw_dmp->macstat[i] = NULL;
    }

    if (fw_dmp->epgregs)
        vfree(fw_dmp->epgregs);

    fw_dmp->epgregs = NULL;

    if (fw_dmp->sreregs)
        vfree(fw_dmp->sreregs);

    fw_dmp->sreregs = NULL;
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

#ifdef COLLECT_FW_LOGS 
	nx_nic_logs_free(adapter);
#endif
	nx_nic_cleanup_minidump(adapter);
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

	pci_release_regions(pdev);
	pci_disable_device(pdev);
}

/* nx_set_dma_mask()
 * Set dma mask depending upon kernel type and device capability
 */
static int nx_set_dma_mask(struct unm_adapter_s *adapter, uint8_t revision_id)
{
	struct pci_dev *pdev = adapter->ahw.pdev;
	uint64_t mask;
	uint64_t cur_fn = (uint64_t)nx_set_dma_mask;

	adapter->dma_mask = DMA_64BIT_MASK;
	mask = DMA_64BIT_MASK;
	NX_NIC_TRC_FN(adapter, cur_fn, mask);

	if (pci_set_dma_mask(pdev, mask) == 0 &&
		pci_set_consistent_dma_mask(pdev, mask) == 0) {

		adapter->pci_using_dac = 1;
		NX_NIC_TRC_FN(adapter, cur_fn, 0);

		return (0);
	} else {
		nx_nic_print3(adapter, "No usable DMA configuration, "
				"aborting\n");
		return -1;
	}
}

static void nx_set_num_rx_queues(unm_adapter *adapter)
{
	uint32_t pcie_setup_func1 = 0, pcie_setup_func2 = 0;
	uint32_t func_bitmap1 = 0, func_bitmap2 = 0;
	uint32_t func_count = 0, i;
	uint64_t cur_fn = (uint64_t) nx_set_num_rx_queues;

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

		NX_NIC_TRC_FN(adapter, cur_fn, func_count);

		if(func_count > 4) {
			adapter->num_rx_queues = NETQ_MAX_RX_CONTEXTS >> 2; 
		} else if(func_count > 2) {
			adapter->num_rx_queues = NETQ_MAX_RX_CONTEXTS >> 1; 
		} else {
			adapter->num_rx_queues = NETQ_MAX_RX_CONTEXTS;
		}
		if(adapter->ahw.cut_through == 1) {
			adapter->num_rx_queues = adapter->num_rx_queues >> 1;
			NX_NIC_TRC_FN(adapter, cur_fn, adapter->num_rx_queues);
		}
	} else {
		/* Honoring the user defined valid value for netqueue */
		adapter->num_rx_queues = num_rx_queues;
		NX_NIC_TRC_FN(adapter, cur_fn, adapter->num_rx_queues);
	}
	if(adapter->multi_ctx) {
		nx_nic_print4(NULL, "Number of RX queues per PCI FN are set to %d\n", adapter->num_rx_queues+1);
	}
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

	p = (unsigned char *)&mac_addr[adapter->ahw.pci_func];

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
	uint64_t cur_fn = (uint64_t) nx_p3_do_chicken_settings;

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
		NX_NIC_TRC_FN(adapter, cur_fn, c8c9value);
	} else {
		chicken |= 0x01000000;  // set chicken3.24 if gen1
		nx_nic_print4(NULL, "Gen1 strapping detected\n");
		c8c9value = 0;
		NX_NIC_TRC_FN(adapter, cur_fn, c8c9value);
	}
	NXWR32(adapter, UNM_PCIE_REG(PCIE_CHICKEN3), chicken);

	pdevfuncsave = pdev->devfn;

	if ((pdevfuncsave & 0x07) != 0 || c8c9value == 0) {
		NX_NIC_TRC_FN(adapter, cur_fn, pdevfuncsave);
		NX_NIC_TRC_FN(adapter, cur_fn, c8c9value);
		return;
	}

	for (ii = 0; ii < 8; ii++) {
		pci_read_config_dword(pdev, pos + 8, &control);
		pci_read_config_dword(pdev, pos + 8, &control);
		pci_write_config_dword(pdev, pos + 8, c8c9value);
		pdev->devfn++;
	}
	pdev->devfn = pdevfuncsave;
	NX_NIC_TRC_FN(adapter, cur_fn, pdev->devfn);
}

#if	defined(UNM_HWBUG_8_WORKAROUND)
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
#endif	/* defined(UNM_HWBUG_8_WORKAROUND) */

static int nx_enable_msi_x(struct unm_adapter_s *adapter)
{
	uint64_t cur_fn = (uint64_t) nx_enable_msi_x;
	/* This fix is for some system showing msi-x on always. */
	nx_reset_msix_bit(adapter->pdev);
	if (adapter->msix_supported == 0) {
		NX_NIC_TRC_FN(adapter, cur_fn, -1);
		return (-1);
	}

	if (!use_msi_x) {
		adapter->msix_supported = 0;
		NX_NIC_TRC_FN(adapter, cur_fn, -1);
		return (-1);
	}

	/* For now we can only run msi-x on functions with 128 MB of
	 * memory. */
	if ((pci_resource_len(adapter->pdev, 0) != UNM_PCI_128MB_SIZE) &&
			(pci_resource_len(adapter->pdev, 0) != UNM_PCI_2MB_SIZE)) {
		adapter->msix_supported = 0;
		NX_NIC_TRC_FN(adapter, cur_fn, -1);
		return (-1);
	}

	init_msix_entries(adapter);
	/* XXX : This fix is for super-micro slot perpetually
	   showing msi-x on !! */
	nx_reset_msix_bit(adapter->pdev);

	if (pci_enable_msix(adapter->pdev, adapter->msix_entries,
			    adapter->num_msix)) {
		NX_NIC_TRC_FN(adapter, cur_fn, -1);
		return (-1);
	}

	adapter->flags |= UNM_NIC_MSIX_ENABLED;

#ifdef	UNM_HWBUG_8_WORKAROUND
	nx_hwbug_8_workaround(adapter->pdev);
#endif
	adapter->netdev->irq = adapter->msix_entries[0].vector;
	NX_NIC_TRC_FN(adapter, cur_fn, 0);
	return (0);
}

static int nx_enable_msi(struct unm_adapter_s *adapter)
{
	if (!use_msi) {
		NX_NIC_TRC_FN(adapter, nx_enable_msi, -1);
		return (-1);
	}
#if defined(CONFIG_PCI_MSI)
	if (!pci_enable_msi(adapter->pdev)) {
		adapter->flags |= UNM_NIC_MSI_ENABLED;
		NX_NIC_TRC_FN(adapter, nx_enable_msi, 0);
		return 0;
	}
	nx_nic_print3(NULL, "Unable to allocate MSI interrupt error\n");
#endif

	NX_NIC_TRC_FN(adapter, nx_enable_msi, -1);
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
	uint64_t cur_fn = (uint64_t) nx_start_firmware;
	int err = 0;
	
	/*
	 * The adapter with portnum == 0 brings the FW up.
	 */
	NX_NIC_TRC_FN(adapter, cur_fn, 0);
	if (adapter->portnum != 0) {
		NX_NIC_TRC_FN(adapter, cur_fn, adapter->portnum);
		goto wait_for_rx_peg_sync;
	}

	NXWR32(adapter, CRB_DMA_SIZE, 0x55555555);

	if (adapter->ahw.boardcfg.board_type == UNM_BRDTYPE_P3_HMEZ ||
			adapter->ahw.boardcfg.board_type == UNM_BRDTYPE_P3_XG_LOM) {
		NXWR32(adapter, UNM_PORT_MODE_ADDR, port_mode);

		NXWR32(adapter, UNM_WOL_PORT_MODE, wol_port_mode);
		NX_NIC_TRC_FN(adapter, cur_fn, adapter->ahw.boardcfg.board_type);
	}

	/* Overwrite stale initialization register values */
	NXWR32(adapter, CRB_CMDPEG_STATE, 0);
	NXWR32(adapter, CRB_RCVPEG_STATE, 0);

	err = load_fw(adapter, fw_load);
	if(err != 0) {
		nx_nic_print3(NULL, "Error in loading firmware\n");
		return err;
	}

	nx_p3_do_chicken_settings(adapter->pdev, adapter);

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
	NX_NIC_TRC_FN(adapter, cur_fn, 0);
	return (receive_peg_ready(adapter));
}

static int __devinit get_adapter_attr(struct unm_adapter_s *adapter)
{
	struct nx_cardrsp_func_attrib	attr;

	if (NX_FW_VERSION(adapter->version.major, adapter->version.minor,
	    adapter->version.sub) <= NX_FW_VERSION(4, 0, 401))
		return 0;

	if (nx_fw_cmd_func_attrib(adapter->nx_dev, adapter->portnum, &attr) !=
	    NX_RCODE_SUCCESS)
		return -1;

	adapter->interface_id = (int)attr.pvport[adapter->portnum];

	return 0;
}

static void nx_vlan_rx_register(struct net_device *netdev,
				struct vlan_group *grp)
{
	struct unm_adapter_s *adapter = netdev_priv(netdev);
	uint64_t cur_fn = (uint64_t) nx_vlan_rx_register;

	adapter->vlgrp = grp;
	NX_NIC_TRC_FN(adapter, cur_fn, grp);
}

static void nx_rx_kill_vid(struct net_device *netdev, unsigned short vid)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,21)
	struct unm_adapter_s *adapter = netdev_priv(netdev);

	if (adapter->vlgrp)
		adapter->vlgrp->vlan_devices[vid] = NULL;
#endif /* < 2.6.21 */
}

static void vlan_accel_setup(struct unm_adapter_s *adapter,
    struct net_device *netdev)
{
	if (adapter->ahw.fw_capabilities_1 & NX_FW_CAPABILITY_FVLANTX) {
		netdev->features |= NETIF_F_HW_VLAN_RX;
		netdev->vlan_rx_register = nx_vlan_rx_register;
		netdev->vlan_rx_kill_vid = nx_rx_kill_vid;
		netdev->features |= NETIF_F_HW_VLAN_TX;
		NX_NIC_TRC_FN(adapter, vlan_accel_setup, NX_FW_CAPABILITY_FVLANTX);
	}
}

static inline void check_8021q(struct net_device *netdev,
		struct sk_buff *skb, int *vidp)
{
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
}

static inline int nx_rx_packet(struct unm_adapter_s *adapter,
    struct sk_buff *skb, int vid)
{
	int	ret;

	if ((vid != -1) && adapter->vlgrp)
		ret = vlan_hwaccel_receive_skb(skb, adapter->vlgrp, (u16)vid);
	else
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
	uint64_t cur_fn = (uint64_t)unm_nic_probe;

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

	if ((err = pci_request_regions(pdev, unm_nic_driver_name))) {
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
		pci_release_regions(pdev);
err_out_disable_pdev:
		pci_disable_device(pdev);
		pci_set_drvdata(pdev, NULL);
		return err;
	}

	SET_MODULE_OWNER(netdev);
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
	spin_lock_init(&adapter->tx_cpu_excl);
	spin_lock_init(&adapter->ctx_state_lock);
	spin_lock_init(&adapter->trc_lock);

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
	adapter->multi_ctx = multi_ctx;

	INIT_LIST_HEAD(&adapter->wait_list);
	for (i = 0; i < NX_WAIT_BIT_MAP_SIZE; i++) {
		adapter->wait_bit_map[i] = 0;
	}

	if ((err = nx_set_dma_mask(adapter, revision_id))) {
		goto err_ret;
	}

	/* remap phys address */
	mem_base = pci_resource_start(pdev, 0);	/* 0 is for BAR 0 */
	mem_len = pci_resource_len(pdev, 0);

	nx_nic_print6(NULL, "ioremap from %lx a size of %lx\n", mem_base,
		      mem_len);

	/* Functions for minidump tools*/
	adapter->unm_nic_hw_write_bar_reg = &unm_nic_hw_write_bar_reg_2M;
	adapter->unm_nic_hw_read_bar_reg = &unm_nic_hw_read_bar_reg_2M;
	adapter->unm_nic_hw_indirect_read = &unm_nic_hw_indirect_read_2M;
	adapter->unm_nic_hw_indirect_write = &unm_nic_hw_indirect_write_2M;

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
		NX_NIC_TRC_FN(adapter, cur_fn, adapter->ahw.pci_func);
		NX_NIC_TRC_FN(adapter, cur_fn, pdev);
	} else {
		NX_NIC_TRC_FN(adapter, cur_fn, adapter->ahw.pci_func);
		NX_NIC_TRC_FN(adapter, cur_fn, pdev);
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

	legacy_intrp = &legacy_intr[pci_func_id];

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

	adapter->max_mc_count = UNM_MC_COUNT;

	adapter->rx_csum = 1;

	netdev->open = unm_nic_open;
	netdev->stop = unm_nic_close;
	netdev->hard_start_xmit = unm_nic_xmit_frame;
	netdev->get_stats = unm_nic_get_stats;
	netdev->set_multicast_list = nx_nic_p3_set_multi;
	netdev->set_mac_address = unm_nic_set_mac;
	netdev->do_ioctl = unm_nic_ioctl;
	netdev->tx_timeout = unm_tx_timeout;
	set_ethtool_ops(netdev);
#ifdef CONFIG_NET_POLL_CONTROLLER
	netdev->poll_controller = unm_nic_poll_controller;
#endif

	/* ScatterGather support */
	netdev->features = NETIF_F_SG;
	netdev->features |= NETIF_F_IP_CSUM;

#ifdef NETIF_F_TSO
	if(tso) {
		netdev->features |= NETIF_F_TSO;
#ifdef  NETIF_F_TSO6
		netdev->features |= NETIF_F_TSO6;
#endif
	}
#endif

	netdev->features |= NETIF_F_HW_CSUM;

	netdev->features |= NETIF_F_DEFQ_L2_FLTR;

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
		NX_NIC_TRC_FN(adapter, cur_fn, 0);
	} else {
		NX_NIC_TRC_FN(adapter, cur_fn, adapter->multi_ctx);
	}

	/* fill the adapter id field with the board serial num */
	unm_nic_get_serial_num(adapter);

	NX_INIT_WORK(&adapter->tx_timeout_task, unm_tx_timeout_task, netdev);

	/* Initialize watchdog  */
	init_timer(&adapter->watchdog_timer);
	adapter->watchdog_timer.data = (unsigned long)adapter;
	adapter->watchdog_timer.function = &unm_watchdog;
	NX_INIT_WORK(&adapter->watchdog_task, unm_watchdog_task, adapter);

	/* Initialize Firmware watchdog  */	
	if(adapter->portnum == 0) {
		init_timer(&adapter->watchdog_timer_fw_reset);
		adapter->watchdog_timer_fw_reset.data = (unsigned long)adapter;
		adapter->watchdog_timer_fw_reset.function = &unm_watchdog_fw_reset;
		NX_INIT_WORK(&adapter->watchdog_task_fw_reset, unm_watchdog_task_fw_reset, adapter);
		NX_NIC_TRC_FN(adapter, cur_fn, 0);
	}

	err = nx_read_flashed_versions(adapter);
	if (err) {
		goto err_ret;
	}
	/* Check flash fw compatiblity. Minimum fw required is 4.0.505 */
	#define MIN_FLASH_FW_SUPP NX_FW_VERSION(4, 0, 505)	
	if (NX_FW_VERSION(adapter->flash_ver.major, adapter->flash_ver.minor,
	    adapter->flash_ver.sub) < MIN_FLASH_FW_SUPP) {
		nx_nic_print3(NULL, "Minimum fw version supported is 4.0.505."
			      "Please update firmware on flash.\n");
		err = -EIO;
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


	nx_nic_get_nic_version_number(adapter->netdev, &adapter->version);

	adapter->max_rds_rings = 2;
	adapter->msg_type_sync_offload = UNM_MSGTYPE_NIC_SYN_OFFLOAD;
	adapter->msg_type_rx_desc = UNM_MSGTYPE_NIC_RXPKT_DESC;
	netdev->change_mtu = nx_nic_fw40_change_mtu;

	temp = NXRD32(adapter, UNM_SRE_MISC);
	adapter->ahw.cut_through = NX_IS_SYSTEM_CUT_THROUGH(temp);
	nx_nic_print5(NULL, "Running in %s mode\n",
			adapter->ahw.cut_through ? "'Cut Through'" :
			"'Legacy'");

	if(adapter->portnum == 0) {
		if ((err = nx_nic_setup_minidump(adapter))) {
			nx_nic_print3(NULL,
				"%s: Error in setting up minidump.\n", __FUNCTION__);
			err = 0;
   		}
	}

	nx_set_num_rx_queues(adapter);

	if (nx_enable_msi_x(adapter)) {
		nx_enable_msi(adapter);
		/*
		 * Both Legacy & MSI case set the irq of the net device
		 */
		adapter->max_possible_rss_rings = 1;
		adapter->multi_ctx = 0;
		netdev->irq = pdev->irq;
		NX_NIC_TRC_FN(adapter, cur_fn, adapter->multi_ctx);
		NX_NIC_TRC_FN(adapter, cur_fn, netdev->irq);
	}

	if (adapter->multi_ctx) {
		err = nx_os_dev_alloc(&nx_dev, adapter, adapter->portnum,
				adapter->num_rx_queues + 1, 1);
		NX_NIC_TRC_FN(adapter, cur_fn, adapter->multi_ctx);
	} else {
		err = nx_os_dev_alloc(&nx_dev, adapter, adapter->portnum,
				MAX_RX_CTX, MAX_TX_CTX);
		NX_NIC_TRC_FN(adapter, cur_fn, adapter->multi_ctx);
	}

	if (err) {
		nx_nic_print6(NULL, "Memory cannot be allocated");
		err = -ENOMEM;
		goto err_ret;
	}

	adapter->nx_dev = nx_dev;

	err = nx_alloc_adapter_sds_rings(adapter);
	if (err) {
		goto err_ret;
	}
	nx_init_napi(adapter);
	
	if (adapter->multi_ctx)
		NX_SET_NETQ_OPS(netdev, nx_nic_netqueue_ops);

	if (!unm_nic_fill_adapter_macaddr_from_flash(adapter)) {
		memcpy(netdev->dev_addr, adapter->mac_addr, netdev->addr_len);
	}

	/* name the proc entries different than ethx, since that can change */
	sprintf(adapter->procname, "dev%d", adapter_count++);


	adapter->ahw.fw_capabilities_1 = NXRD32( adapter, UNM_FW_CAPABILITIES_1);

	if (adapter->ahw.fw_capabilities_1 & NX_FW_CAPABILITY_LRO) {
		nx_nic_print6(adapter, "LRO capable device found\n");
		err = unm_init_lro(adapter);
		if (err != 0) {
			nx_nic_print3(NULL,
					"LRO Initialization failed\n");
			goto err_ret;
		}
		NX_NIC_TRC_FN(adapter, cur_fn, NX_FW_CAPABILITY_LRO);
	}
	if(!lro) {
		adapter->lro.enabled = 0;
		NX_NIC_TRC_FN(adapter, cur_fn, adapter->lro.enabled);
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

	if (get_adapter_attr(adapter))
                nx_nic_print5(NULL, "Could not get adapter attributes\n");

	if(hw_vlan) {
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

	NX_NIC_TRC_FN(adapter, cur_fn, 0);
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
	uint64_t cur_fn = (uint64_t)initialize_adapter_hw;

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
	NX_NIC_TRC_FN(adapter, cur_fn, ports);

	return value;
}

static int init_firmware(struct unm_adapter_s *adapter)
{
	uint32_t state = 0, loops = 0, err = 0;
	uint32_t   tempout;
	uint64_t cur_fn = (uint64_t) init_firmware;

	NX_NIC_TRC_FN(adapter, cur_fn, 0);
	/* Window 1 call */
	read_lock(&adapter->adapter_lock);
	state = NXRD32(adapter, CRB_CMDPEG_STATE);
	read_unlock(&adapter->adapter_lock);

	if (state == PHAN_INITIALIZE_ACK) {
		NX_NIC_TRC_FN(adapter, cur_fn, 0);
		return 0;
	}

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
		NX_NIC_TRC_FN(adapter, cur_fn, err);
		return err;
	}
	/* Window 1 call */
	read_lock(&adapter->adapter_lock);
        tempout = PHAN_INITIALIZE_ACK;
        NXWR32(adapter, CRB_CMDPEG_STATE, tempout);
	read_unlock(&adapter->adapter_lock);

	NX_NIC_TRC_FN(adapter, cur_fn, err);
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
	uint64_t cur_fn = (uint64_t) nx_free_rx_resources;

	NX_NIC_TRC_FN(adapter, cur_fn, 0);

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
			buffer->dma = (uint64_t) 0;
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
	uint64_t cur_fn = (uint64_t) unm_nic_free_ring_context;

	if (adapter->cmd_buf_arr != NULL) {
		NX_NIC_TRC_FN(adapter, cur_fn, adapter->cmd_buf_arr);
		vfree(adapter->cmd_buf_arr);
		adapter->cmd_buf_arr = NULL;
	}

	if (adapter->ahw.cmdDescHead != NULL) {
		NX_NIC_TRC_FN(adapter, cur_fn, adapter->ahw.cmdDescHead);
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

#ifdef EPG_WORKAROUND
	if (adapter->ahw.pauseAddr != NULL) {
		pci_free_consistent(adapter->ahw.pause_pdev, 512,
				adapter->ahw.pauseAddr,
				adapter->ahw.pause_physAddr);
		adapter->ahw.pauseAddr = NULL;
	}
#endif

	for(index = 0; index < adapter->nx_dev->alloc_rx_ctxs ; index++) {
		if (adapter->nx_dev->rx_ctxs[index] != NULL) {
			if(index != 0) {	
				NX_NIC_TRC_FN(adapter, cur_fn, index);
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
				NX_NIC_TRC_FN(adapter, cur_fn, index);
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
	unm_nic_new_rx_context_destroy(adapter, ctx_destroy);
	unm_nic_new_tx_context_destroy(adapter, ctx_destroy);
}

void unm_nic_free_hw_resources(struct unm_adapter_s *adapter)
{
	NX_NIC_TRC_FN(adapter, unm_nic_free_hw_resources, 0);
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

static int unm_nic_attach(struct unm_adapter_s *adapter)
{
	int	rv;
	int	ring;
	uint64_t cur_fn = (uint64_t) unm_nic_attach;

	NX_NIC_TRC_FN(adapter, cur_fn, 0);

	if (adapter->driver_mismatch) {
		NX_NIC_TRC_FN(adapter, cur_fn, EIO);
		return -EIO;
	}
	if (adapter->is_up != ADAPTER_UP_MAGIC) {
		NX_NIC_TRC_FN(adapter, cur_fn, adapter->is_up);
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
			NX_NIC_TRC_FN(adapter, cur_fn, rv);
			return (rv);
		}

		for (ring = 0; ring < adapter->max_rds_rings; ring++) {
			if(adapter->nx_dev->rx_ctxs[0] != NULL) {
				unm_post_rx_buffers(adapter, adapter->nx_dev->rx_ctxs[0], ring);
			}
		}

		rv = nx_register_irq(adapter,adapter->nx_dev->rx_ctxs[0]);
		if (rv) {
			nx_nic_print3(adapter, "Unable to register the "
				      "interrupt service routine\n");
			unm_nic_free_ring_context_in_fw(adapter, NX_DESTROY_CTX_RESET);
			unm_nic_free_ring_context(adapter);
			NX_NIC_TRC_FN(adapter, cur_fn, rv);
			return (rv);
		}

		adapter->is_up = ADAPTER_UP_MAGIC;

	}

	nx_napi_enable(adapter);

	/* Set up virtual-to-physical port mapping */
	adapter->physical_port = (adapter->nx_dev->rx_ctxs[0])->port;

	read_lock(&adapter->adapter_lock);
	unm_nic_enable_all_int(adapter, adapter->nx_dev->rx_ctxs[0]);
	read_unlock(&adapter->adapter_lock);

	memcpy(adapter->netdev->dev_addr, adapter->mac_addr,
	       adapter->netdev->addr_len);

	if (unm_nic_init_port(adapter) != 0) {
		nx_nic_print3(adapter, "Failed to initialize the port %d\n",
			      adapter->portnum);

		nx_free_tx_resources(adapter);
		NX_NIC_TRC_FN(adapter, cur_fn, EIO);
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
		NX_NIC_TRC_FN(adapter, cur_fn, adapter->max_possible_rss_rings);
	}

	if (adapter->netdev->change_mtu) {
		adapter->netdev->change_mtu(adapter->netdev,
					    adapter->netdev->mtu);
	}

	adapter->ahw.linkup = 0;
    adapter->false_linkup = 0;
    adapter->init_link_update_ctr = 0;
    netif_carrier_off(adapter->netdev);

	mod_timer(&adapter->watchdog_timer, jiffies);

	spin_lock(&adapter->ctx_state_lock);
	adapter->attach_flag = 1;
	spin_unlock(&adapter->ctx_state_lock);

	NX_NIC_TRC_FN(adapter, cur_fn, 0);
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
		for (index = 0; index < adapter->nx_dev->alloc_rx_ctxs; index++) {
			if (adapter->nx_dev->rx_ctxs[index] != NULL) {
				spin_lock(&adapter->ctx_state_lock);
				if((adapter->nx_dev->rx_ctxs_state & (1 << index)) != 0) { 
					spin_unlock(&adapter->ctx_state_lock);
				} else {
					spin_unlock(&adapter->ctx_state_lock);
					continue;
				}
				nx_unregister_irq(adapter, adapter->nx_dev->rx_ctxs[index]);
			}
		}
	}

	/*
	 * Step 5: Destroy all the SW data structures.
	 */
	if (adapter->portnum == 0) {
		destroy_dummy_dma(adapter);
	}

	if (!list_empty(&adapter->wait_list)) {
		list_for_each_safe(ptr, tmp, &adapter->wait_list) {
			wait = list_entry(ptr, nx_os_wait_event_t, list);

			wait->trigger = 1;
			wait->active  = 0;
			wake_up_interruptible(&wait->wq);
		}		
	}

	unm_nic_free_ring_context(adapter);

	nx_nic_p3_free_mac_list(adapter);

	adapter->is_up = 0;
}


static void __devexit unm_nic_remove(struct pci_dev *pdev)
{
	struct unm_adapter_s *adapter;
	struct net_device *netdev;

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

	if(adapter->portnum == 0 ) {
		FLUSH_SCHEDULED_WORK();
       		del_timer_sync(&adapter->watchdog_timer_fw_reset);
	}
	unregister_netdev(netdev);
	unm_nic_detach(adapter, NX_DESTROY_CTX_RESET);

	nx_release_fw(&adapter->nx_fw);
	cleanup_adapter(adapter);

	pci_set_drvdata(pdev, NULL);

	free_netdev(netdev);
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
#ifndef __VMKLNX__
	nx_host_nic_t* nx_dev;
	U32 pci_func;
	U32 rcode;
#endif
	U32 max;
	uint64_t cur_fn = (uint64_t) nx_nic_multictx_get_filter_count;

	NX_NIC_TRC_FN(adapter, cur_fn, queue_type);

	if(adapter->is_up == FW_DEAD) {
		return -1;
	}

#ifndef __VMKLNX__
	nx_dev = adapter->nx_dev;
	pci_func = adapter->nx_dev->pci_func;
#endif
	if (MULTICTX_IS_TX(queue_type)) {
		nx_nic_print3(adapter, "%s: TX filters not supported\n",
				__FUNCTION__);
		return -1;
	} else if(MULTICTX_IS_RX(queue_type)) {
#ifndef __VMKLNX__
		rcode = nx_fw_cmd_query_max_rules_per_ctx(nx_dev, pci_func,
					 &max);
#else
		/* Hard code filter count to 1 to avoid firmware would report
		 * 0 on filter numbers sometimes due to firmware defects.
		 * See PR 926909. */
		max = 1;
#endif
	} else {
		nx_nic_print3(adapter, "%s: Invalid ctx type specified\n",
				__FUNCTION__);
		return -1;
	}

#ifndef __VMKLNX__
	if( rcode != NX_RCODE_SUCCESS){
		NX_NIC_TRC_FN(adapter, cur_fn, -1);
		return -1;
	}
#endif
	NX_NIC_TRC_FN(adapter, cur_fn, max);
	return (max);
}

struct napi_struct * nx_nic_multictx_get_napi(struct net_device *netdev , int queue_id)
{
	struct unm_adapter_s *adapter = netdev_priv(netdev);
	uint64_t cur_fn = (uint64_t) nx_nic_multictx_get_napi;

	if(queue_id < 0 || queue_id > adapter->num_rx_queues) {
		NX_NIC_TRC_FN(adapter, cur_fn, queue_id);
		return NULL;
	}
	NX_NIC_TRC_FN(adapter, cur_fn, &(adapter->host_sds_rings[queue_id].napi));
	return &(adapter->host_sds_rings[queue_id].napi);	
}

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
	struct unm_adapter_s *adapter = netdev_priv(netdev);
	uint64_t cur_fn = (uint64_t) nx_nic_multictx_get_ctx_count;

    if(adapter->is_up == FW_DEAD) {
		NX_NIC_TRC_FN(adapter, cur_fn, 0);
        return -1;
    }

	max = adapter->num_rx_queues ;
	NX_NIC_TRC_FN(adapter, cur_fn, max);
	return (max);
}

int nx_nic_multictx_get_queue_vector(struct net_device *netdev, int qid)
{
	struct unm_adapter_s *adapter = netdev_priv(netdev);
	uint64_t cur_fn = (uint64_t) nx_nic_multictx_get_queue_vector;

	if(qid < 0 || qid > adapter->num_rx_queues) {
		NX_NIC_TRC_FN(adapter, cur_fn, qid);
		NX_NIC_TRC_FN(adapter, cur_fn, -1);
		return -1;
	}

	NX_NIC_TRC_FN(adapter, cur_fn, adapter->msix_entries[qid].vector);
	return adapter->msix_entries[qid].vector ;
}

int nx_nic_multictx_get_ctx_stats(struct net_device *netdev, int ctx_id,
		struct net_device_stats *stats)
{
	struct unm_adapter_s *adapter = netdev_priv(netdev);
	nx_host_rx_ctx_t *nxhal_host_rx_ctx = NULL;
	nx_host_rds_ring_t *nxhal_host_rds_ring = NULL;
	rds_host_ring_t *host_rds_ring = NULL;
	int ring;
	uint64_t cur_fn = (uint64_t) nx_nic_multictx_get_ctx_stats;

    if(adapter->is_up == FW_DEAD) {
		NX_NIC_TRC_FN(adapter, cur_fn, -1);
        return -1;
    }

	if(ctx_id < 0 || ctx_id > adapter->num_rx_queues) {
		NX_NIC_TRC_FN(adapter, cur_fn, ctx_id);
		NX_NIC_TRC_FN(adapter, cur_fn, -1);
		return -1;
	}

	nxhal_host_rx_ctx = adapter->nx_dev->rx_ctxs[ctx_id];

	if(nxhal_host_rx_ctx == NULL) {
		NX_NIC_TRC_FN(adapter, cur_fn, -1);
		return -1;
	}

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
	NX_NIC_TRC_FN(adapter, cur_fn, 0);
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
	uint64_t cur_fn = (uint64_t) nx_nic_free_hw_rx_resources;

	NX_NIC_TRC_FN(adapter, cur_fn, 0);

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
	uint64_t cur_fn = (uint64_t) nx_nic_free_host_sds_resources;

	for (ring = 0; ring < nxhal_host_rx_ctx->num_sds_rings; ring++) {
		if (nxhal_host_rx_ctx->sds_rings[ring].os_data) {
			NX_NIC_TRC_FN(adapter, cur_fn, nxhal_host_rx_ctx->sds_rings[ring].os_data);
			nxhal_host_rx_ctx->sds_rings[ring].os_data = NULL;
		}
	}
}

static void nx_nic_free_host_rx_resources(struct unm_adapter_s *adapter,
					  nx_host_rx_ctx_t *nxhal_host_rx_ctx)
{
	int ring;
	rds_host_ring_t *host_rds_ring = NULL;
	uint64_t cur_fn = (uint64_t) nx_nic_free_host_rx_resources;

	NX_NIC_TRC_FN(adapter, cur_fn, 0);

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
	uint64_t cur_fn = (uint64_t) nx_nic_alloc_hw_rx_resources;
	NX_NIC_TRC_FN(adapter, cur_fn, err);

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
			NX_NIC_TRC_FN(adapter, cur_fn, err);
			goto done;
		}
		nx_nic_print7(adapter, "rcv %d physAddr: 0x%llx\n",
				ring, (U64)nxhal_host_rds_ring->host_phys);

		nxhal_host_rds_ring->host_addr = (I8 *) addr;
	}
	NX_NIC_TRC_FN(adapter, cur_fn, 0);
	return 0;
done:
	nx_nic_free_hw_rx_resources(adapter, nxhal_host_rx_ctx);
	NX_NIC_TRC_FN(adapter, cur_fn, err);
	return err;
}

static void nx_nic_alloc_host_sds_resources(struct unm_adapter_s *adapter,
		nx_host_rx_ctx_t *nxhal_host_rx_ctx)
{
	int ring = 0;
	int ctx_id = nxhal_host_rx_ctx->this_id;
	int num_sds_rings = nxhal_host_rx_ctx->num_sds_rings;
	sds_host_ring_t *host_ring;
	uint64_t cur_fn = (uint64_t) nx_nic_alloc_host_sds_resources;

	NX_NIC_TRC_FN(adapter, cur_fn, 0);

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
	uint64_t cur_fn = (uint64_t) nx_nic_alloc_host_rx_resources;

	NX_NIC_TRC_FN(adapter, cur_fn, 0);

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

				if (adapter->ahw.cut_through) {
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

				host_rds_ring->dma_size =
					NX_P3_RX_JUMBO_BUF_MAX_LEN;
				if (!adapter->ahw.cut_through) {
					host_rds_ring->dma_size +=
						NX_NIC_JUMBO_EXTRA_BUFFER;
					host_rds_ring->truesize_delta =
						NX_NIC_JUMBO_EXTRA_BUFFER;
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
			NX_NIC_TRC_FN(adapter, cur_fn, err);
			goto err_ret;
		}
		memset(rx_buf_arr, 0,
				RCV_BUFFSIZE(nxhal_host_rds_ring->ring_size));
		host_rds_ring->rx_buf_arr = rx_buf_arr;
	}

	initialize_adapter_sw(adapter, nxhal_host_rx_ctx);

	NX_NIC_TRC_FN(adapter, cur_fn, err);
	return err;

err_ret:
	nx_nic_free_host_rx_resources(adapter, nxhal_host_rx_ctx);
	NX_NIC_TRC_FN(adapter, cur_fn, err);
	return err;
}

int nx_nic_multictx_alloc_rx_ctx(struct net_device *netdev)
{
	int ctx_id = -1,ring = 0;
	int err = 0;
	nx_host_rx_ctx_t *nxhal_host_rx_ctx = NULL;
	struct unm_adapter_s *adapter = netdev_priv(netdev);
	uint64_t cur_fn = (uint64_t) nx_nic_multictx_alloc_rx_ctx;

	if(adapter->is_up == FW_DEAD) { 
		NX_NIC_TRC_FN(adapter, cur_fn, -1);
		return -1;
	}
	ctx_id = nx_nic_create_rx_ctx(netdev);

	NX_NIC_TRC_FN(adapter, cur_fn, ctx_id);

	if( ctx_id >= 0 ) {
		nxhal_host_rx_ctx = adapter->nx_dev->rx_ctxs[ctx_id];

		if (nx_fw_cmd_set_mtu(nxhal_host_rx_ctx, adapter->ahw.pci_func,
					netdev->mtu)) {
			nx_nic_print6(adapter,"Unable to set mtu for context: %d\n",ctx_id);
			NX_NIC_TRC_FN(adapter, cur_fn, ctx_id);
		}

		for (ring = 0; ring < adapter->max_rds_rings; ring++) {
			if(unm_post_rx_buffers(adapter,nxhal_host_rx_ctx, ring)) {
				NX_NIC_TRC_FN(adapter, cur_fn, ring);
				goto err_ret;
			}
		}
		err = nx_register_irq(adapter,nxhal_host_rx_ctx);
		if(err) {
			NX_NIC_TRC_FN(adapter, cur_fn, err);
			goto err_ret;
		}
#ifdef __VMKLNX__
		/* enable NAPI after allocating an IRQ */
		if (!adapter->host_sds_rings[ctx_id].napi_enable) {
			napi_enable(&adapter->host_sds_rings[ctx_id].napi);
			adapter->host_sds_rings[ctx_id].napi_enable = 1;
		}
#endif
		read_lock(&adapter->adapter_lock);
		unm_nic_enable_all_int(adapter, nxhal_host_rx_ctx);
		read_unlock(&adapter->adapter_lock);
		adapter->nx_dev->rx_ctxs_state |= (1 << ctx_id);
		NX_NIC_TRC_FN(adapter, cur_fn, ctx_id);
		return ctx_id;
	}
err_ret:
	if (nxhal_host_rx_ctx && adapter->nx_dev) {
		nx_fw_cmd_destroy_rx_ctx(nxhal_host_rx_ctx,
				NX_DESTROY_CTX_RESET);
		nx_free_sts_rings(adapter, nxhal_host_rx_ctx);
		nx_nic_free_host_sds_resources(adapter, nxhal_host_rx_ctx);
		nx_free_rx_resources(adapter, nxhal_host_rx_ctx);
		nx_nic_free_hw_rx_resources(adapter, nxhal_host_rx_ctx);
		nx_nic_free_host_rx_resources(adapter, nxhal_host_rx_ctx);
		nx_fw_cmd_create_rx_ctx_free(adapter->nx_dev,
				nxhal_host_rx_ctx);
	}

	NX_NIC_TRC_FN(adapter, cur_fn, -1);
	return -1;

}

int nx_nic_create_rx_ctx(struct net_device *netdev)
{
	int err = 0;
	nx_host_rx_ctx_t *nxhal_host_rx_ctx = NULL;
	struct unm_adapter_s *adapter = netdev_priv(netdev);
	int num_rules = 1;
	int num_rss_rings =   adapter->max_possible_rss_rings;
	uint64_t cur_fn = (uint64_t) nx_nic_create_rx_ctx;

	NX_NIC_TRC_FN(adapter, cur_fn, 0);

	err = nx_fw_cmd_create_rx_ctx_alloc(adapter->nx_dev,
			adapter->max_rds_rings,
			num_rss_rings, num_rules,
			&nxhal_host_rx_ctx);
	if (err) {
		nx_nic_print3(adapter, "RX ctx memory allocation FAILED\n");
		NX_NIC_TRC_FN(adapter, cur_fn, err);
		goto err_ret;
	}
	if ((err = nx_nic_alloc_host_rx_resources(adapter,
					nxhal_host_rx_ctx))) {
		nx_nic_print3(adapter, "RDS memory allocation FAILED\n");
		NX_NIC_TRC_FN(adapter, cur_fn, err);
		goto err_ret;
	}

	if ((err = nx_nic_alloc_hw_rx_resources(adapter, nxhal_host_rx_ctx))) {
		nx_nic_print3(adapter, "RDS Descriptor ring memory allocation "
				"FAILED\n");
		NX_NIC_TRC_FN(adapter, cur_fn, err);
		goto err_ret;
	}

	nx_nic_alloc_host_sds_resources(adapter, nxhal_host_rx_ctx);

	err = nx_alloc_sts_rings(adapter, nxhal_host_rx_ctx,
			nxhal_host_rx_ctx->num_sds_rings);
	if (err) {
		nx_nic_print3(adapter, "SDS memory allocation FAILED\n");
		NX_NIC_TRC_FN(adapter, cur_fn, err);
		goto err_ret;
	}

	if ((err = unm_nic_new_rx_context_prepare(adapter,
					nxhal_host_rx_ctx))) {
		nx_nic_print3(adapter, "RX ctx allocation FAILED\n");
		NX_NIC_TRC_FN(adapter, cur_fn, err);
		goto err_ret;
	}

	NX_NIC_TRC_FN(adapter, cur_fn, nxhal_host_rx_ctx->this_id);
	return nxhal_host_rx_ctx->this_id;

err_ret:
	if (nxhal_host_rx_ctx && adapter->nx_dev) {
		NX_NIC_TRC_FN(adapter, cur_fn, 0);
		nx_free_sts_rings(adapter,nxhal_host_rx_ctx);
		nx_nic_free_host_sds_resources(adapter,nxhal_host_rx_ctx);
		nx_free_rx_resources(adapter,nxhal_host_rx_ctx);
		nx_nic_free_hw_rx_resources(adapter, nxhal_host_rx_ctx);
		nx_nic_free_host_rx_resources(adapter, nxhal_host_rx_ctx);
		nx_fw_cmd_create_rx_ctx_free(adapter->nx_dev,
				nxhal_host_rx_ctx);
	}

	NX_NIC_TRC_FN(adapter, cur_fn, -1);
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
	uint64_t cur_fn = (uint64_t) nx_nic_multictx_free_rx_ctx;

	NX_NIC_TRC_FN(adapter, cur_fn, ctx_id);

	if(adapter->is_up == FW_DEAD) { 
		NX_NIC_TRC_FN(adapter, cur_fn, -1);
		return -1;
	}
	if (ctx_id > adapter->nx_dev->alloc_rx_ctxs) {
		nx_nic_print4(adapter, "%s: Invalid context id\n",
			      __FUNCTION__);
		NX_NIC_TRC_FN(adapter, cur_fn, -1);
		return -1;
	}

	if(adapter->attach_flag == 0) {
		NX_NIC_TRC_FN(adapter, cur_fn, -1);
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
#ifdef __VMKLNX__
		if (!test_bit(NAPI_STATE_UNUSED, &adapter->host_sds_rings[ctx_id].napi.state))
#endif
		napi_synchronize(&adapter->host_sds_rings[ctx_id].napi);
#ifdef __VMKLNX__
		/* need to disable NAPI when freeing an IRQ */
		if (adapter->host_sds_rings[ctx_id].napi_enable) {
			napi_disable(&adapter->host_sds_rings[ctx_id].napi);
			adapter->host_sds_rings[ctx_id].napi_enable = 0;
		}
#endif
		read_lock(&adapter->adapter_lock);
		unm_nic_disable_all_int(adapter, nxhal_host_rx_ctx);
		read_unlock(&adapter->adapter_lock);
		nx_synchronize_irq(adapter, nxhal_host_rx_ctx);
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
		NX_NIC_TRC_FN(adapter, cur_fn, -1);
		return (-1);
	}

	NX_NIC_TRC_FN(adapter, cur_fn, 0);
	return 0;
}

int nx_nic_multictx_set_rx_rule(struct net_device *netdev, int ctx_id, char* mac_addr)
{
	int rv;
	int i;
	nx_host_rx_ctx_t *nxhal_host_rx_ctx = NULL;
	nx_rx_rule_t *rx_rule = NULL;
	struct unm_adapter_s *adapter = netdev_priv(netdev);
	uint64_t cur_fn = (uint64_t) nx_nic_multictx_set_rx_rule;

	NX_NIC_TRC_FN(adapter, cur_fn, ctx_id);

	if(adapter->is_up == FW_DEAD) { 
		NX_NIC_TRC_FN(adapter, cur_fn, -1);
		return -1;
	}

	if(ctx_id > adapter->nx_dev->alloc_rx_ctxs) {
		nx_nic_print3(adapter, "%s: Invalid context id\n",__FUNCTION__);
		NX_NIC_TRC_FN(adapter, cur_fn, -1);
		return -1;
	}

	nxhal_host_rx_ctx = adapter->nx_dev->rx_ctxs[ctx_id];

	if(!nxhal_host_rx_ctx) {
		nx_nic_print3(adapter, "%s: Ctx not active\n", __FUNCTION__);
		NX_NIC_TRC_FN(adapter, cur_fn, -1);
		return -1;
	}

	if(nxhal_host_rx_ctx->active_rx_rules >= nxhal_host_rx_ctx->num_rules) {
		nx_nic_print3(adapter, "%s: Rules counts exceeded\n",__FUNCTION__);
		NX_NIC_TRC_FN(adapter, cur_fn, -1);
		NX_NIC_TRC_FN(adapter, cur_fn, nxhal_host_rx_ctx->active_rx_rules);
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
		NX_NIC_TRC_FN(adapter, cur_fn, -1);
		return -1;
	}

	rv = nx_os_pf_add_l2_mac(adapter->nx_dev, nxhal_host_rx_ctx, mac_addr);

	if (rv == 0) {
		rx_rule->active = 1;
		nx_os_copy_memory(&(rx_rule->arg.m.mac), mac_addr, 6) ;
		rx_rule->type = NX_RX_RULETYPE_MAC;
		nxhal_host_rx_ctx->active_rx_rules++;
		NX_NIC_TRC_FN(adapter, cur_fn, nxhal_host_rx_ctx->active_rx_rules);
	} else {
		nx_nic_print3(adapter, "%s: Failed to set mac addr\n", __FUNCTION__);
		NX_NIC_TRC_FN(adapter, cur_fn, -1);
		return -1;
	}

	NX_NIC_TRC_FN(adapter, cur_fn, rx_rule->id);
	return rx_rule->id;
}


int nx_nic_multictx_remove_rx_rule(struct net_device *netdev, int ctx_id, int rule_id)
{
	int rv;
	nx_host_rx_ctx_t *nxhal_host_rx_ctx = NULL;
	nx_rx_rule_t *rx_rule = NULL;
	struct unm_adapter_s *adapter = netdev_priv(netdev);
	char mac_addr[6];
	uint64_t cur_fn = (uint64_t) nx_nic_multictx_remove_rx_rule;

	NX_NIC_TRC_FN(adapter, cur_fn, ctx_id);
	NX_NIC_TRC_FN(adapter, cur_fn, rule_id);

	if(adapter->is_up == FW_DEAD) { 
		NX_NIC_TRC_FN(adapter, cur_fn, -1);
		return -1;
	}

	if(ctx_id > adapter->nx_dev->alloc_rx_ctxs) {
		nx_nic_print3(adapter, "%s: Invalid context id\n",__FUNCTION__);
		NX_NIC_TRC_FN(adapter, cur_fn, -1);
		return -1;
	}

	nxhal_host_rx_ctx = adapter->nx_dev->rx_ctxs[ctx_id];

	if(!nxhal_host_rx_ctx) {
		nx_nic_print3(adapter, "%s: Ctx not active\n", __FUNCTION__);
		NX_NIC_TRC_FN(adapter, cur_fn, -1);
		return -1;
	}

	if(rule_id >= nxhal_host_rx_ctx->num_rules) {
		nx_nic_print3(adapter, "%s: Invalid rule id specified\n",__FUNCTION__);
		NX_NIC_TRC_FN(adapter, cur_fn, -1);
		return -1;
	}

	rx_rule = &nxhal_host_rx_ctx->rules[rule_id];

	if(!rx_rule->active) {
		nx_nic_print3(adapter, "%s: Deleting an inactive rule \n",__FUNCTION__);
		NX_NIC_TRC_FN(adapter, cur_fn, -1);
		return -1;
	}

	nx_os_copy_memory(mac_addr, &(rx_rule->arg.m.mac), 6);

	rv = nx_os_pf_remove_l2_mac(adapter->nx_dev, nxhal_host_rx_ctx, mac_addr);

	if (rv == 0) {
		rx_rule->active = 0;
		nxhal_host_rx_ctx->active_rx_rules--;
		NX_NIC_TRC_FN(adapter, cur_fn, nxhal_host_rx_ctx->active_rx_rules);
	} else {
		nx_nic_print3(adapter, "%s: Failed to delete mac addr\n", __FUNCTION__);
		NX_NIC_TRC_FN(adapter, cur_fn, -1);
		return -1;
	}

	NX_NIC_TRC_FN(adapter, cur_fn, rv);
	return rv;
}

#ifdef COLLECT_FW_LOGS 
int nx_pqm_queuelen (struct unm_adapter_s *adapter, int qnum)
{
    uint32_t val;

    NXWR32(adapter, NX_PQM_Q_STATUS_MUX, qnum);
    val = NXRD32(adapter, NX_PQM_Q_STATUS_VAL);
    return val;
}

uint32_t nx_qm_queuelen(struct unm_adapter_s *adapter, int pqnum,   /* primary queue number */
                int remote,  /* 0 = primary, 1 = remote */
                int sqnum)   /* minor number (only valid for remote) */
{
    uint32_t len;

#if 0
    /*
     * Check for out-of-range on our arguments and for wrong queue type.
     */
    if (((side < 0) || (side >= UNM_QM_MAX_SIDE)) ||
            (pqnum < 0) || (pqnum > UNM_PQM_MAX_QUEUES)) {
        return -1;
    }
#endif


    if (remote == 0) {
        len = (nx_pqm_queuelen(adapter, pqnum) & 0xFFF);
    } else {

        /* remote queue - pqnum must be 0,4,8,12 */
        if (((pqnum & ~0xc) != 0))
            return -1;

        if (pqnum == NX_QM_TYPE1_MAJQ) {
            /* This is a type1 queue . Type1 queues store their state in
             * memory rather than CRB.  To avoid cache destruction, use
             * the CRB backdoor to get to memory.
             */
            int grp;
            __uint64_t addr;
            uint64_t qstate;
    
            grp = sqnum >> 16;
            addr = (sqnum & 0xFFFF) * sizeof(qstate);
            addr += UNM_ADDR_SQMSTAT_BASE_TABLE(grp);
            adapter->unm_nic_pci_mem_read(adapter,
                    addr, &qstate, sizeof(qstate));
            len = ((qstate >> 48) & 0x7F);
        } else {
            int grp;

            grp = (pqnum >> 2) & 0x3;
            len = NXRD32(adapter, NX_SQM_CURR_LEN(grp, sqnum));
        }
    }
    return len;
}

#define TESTMUX_LOCK_TIMEOUT   100000

int nx_testmux_lock(struct unm_adapter_s *adapter)
{
	int timeout = 0;

	while (1) {
		/* acquire semaphore4 from PCI HW block */
		if(NXRD32(adapter, UNM_PCIE_REG(PCIE_SEM4_LOCK)))
			break;
		if (timeout >= TESTMUX_LOCK_TIMEOUT) {
			return -1;
		}
		timeout++;
	}
	return 0;
}

int nx_testmux_unlock(struct unm_adapter_s *adapter)
{

    NXRD32(adapter, UNM_PCIE_REG(PCIE_SEM4_UNLOCK));
    return 0;
}

static void nx_clear_all_tm(struct unm_adapter_s *adapter)
{
    uint32_t temp;

    NXWR32(adapter, NX_EPG_TM, 0);
    NXWR32(adapter, NX_MN_TM, 0);
    NXWR32(adapter, NX_MS_TM, 0);
    NXWR32(adapter, NX_NIU_TM, 0);
    NXWR32(adapter, NX_SN_TM, 0);
    NXWR32(adapter, NX_SRE_TM, 0);
    NXWR32(adapter, NX_SQN0F_TM, 0);
    NXWR32(adapter, NX_SQN0_TM, 0);
    NXWR32(adapter, NX_SQN2F_TM, 0);
    NXWR32(adapter, NX_SQN2_TM, 0);
    NXWR32(adapter, NX_SQN3F_TM, 0);
    NXWR32(adapter, NX_SQN3_TM, 0);
    NXWR32(adapter, NX_SQN1F_TM, 0);
    NXWR32(adapter, NX_SQN1_TM, 0);
    NXWR32(adapter, NX_XDMA_TM, 0);
    temp = NXRD32(adapter, NX_CAM_TM);
    NXWR32(adapter, NX_CAM_TM, temp & 0xc0ffffff);
    NXWR32(adapter, NX_OCM0_TM, 0);
}

static uint32_t nx_testmux(struct unm_adapter_s *adapter,
        uint32_t which, uint32_t select)
{
	uint32_t clkgate;
	uint32_t sre_tm;
	uint32_t ret;
	uint32_t temp;

	clkgate = NXRD32(adapter, UNM_ROMUSB_GLB_CHIP_CLK_CTRL);
	sre_tm = NXRD32(adapter, NX_SRE_TM);

	NXWR32(adapter, UNM_ROMUSB_GLB_CHIP_CLK_CTRL, 0x7ffff);

	nx_clear_all_tm(adapter);
	nx_clear_all_tm(adapter);

	if(nx_testmux_lock(adapter)) {
		printk("FAIL: could not get testmux lock\n");
		return 0;
	}

	switch (which) {
		case NX_EPG_TM:
		case NX_PEX_TM:
		case NX_MN_TM:
		case NX_MS_TM:
		case NX_NIU_TM:
		case NX_SN_TM:
		case NX_XDMA_TM:
		case NX_SQN0F_TM:
		case NX_SQN0_TM:
		case NX_SQN1F_TM:
		case NX_SQN1_TM:
		case NX_SQN2F_TM:
		case NX_SQN2_TM:
		case NX_SQN3F_TM:
		case NX_SQN3_TM:
		case NX_CAS_TM:
		case NX_TMR_TM:
			NXWR32(adapter, which, select);
			break;

		case NX_SRE_TM:
			NXWR32(adapter, which, select | sre_tm );
			break;

		case NX_CAM_TM:
			temp = ((NXRD32(adapter, which) & 0xc0ffffff) | (select << 24));
			NXWR32(adapter, which, temp);
			break;

		case NX_PEG0_TM:
			temp = ((NXRD32(adapter, which) & 0xffffffe7) | (select << 3));
			NXWR32(adapter, which, temp);
			break;
	}

	ret = NXRD32(adapter, UNM_CRB_OCM0 + 0x700);

	NXWR32(adapter, UNM_ROMUSB_GLB_CHIP_CLK_CTRL, clkgate);
	NXWR32(adapter, NX_SRE_TM, sre_tm);

	nx_testmux_unlock(adapter);
	return ret;
}

static void nx_sre_status (struct unm_adapter_s *adapter)
{
	uint32_t status;
	uint32_t ok = 1;
	uint32_t hdr;
	int qmaj, qsub, qmin;
	int qlen = 0;
	int len;
	struct fw_dump *fw_dmp = &adapter->fw_dmp;
	char *phanstat = NULL;

	if(fw_dmp->phanstat) {
		phanstat = fw_dmp->phanstat;
	} else {
		return;
	}

	len = strlen(phanstat);

	len += sprintf(phanstat + len,"\nSre Status: ");

	ok = 1;
	status = NXRD32(adapter, UNM_SRE_PBI_ACTIVE_STATUS);
	if((status & 0x1) == 0) {
		len += sprintf(phanstat + len,"\nSRE PBI currently halted. Empty buffer list?");
		ok = 0;
	}

	status = NXRD32(adapter, UNM_SRE_L1RE_CTL);
	if((status & 0x20000000) != 0) {
		len += sprintf(phanstat + len,"\nSRE currently paused due to L1 IPQ discard failure.");
		ok = 0;
	}

	status = NXRD32(adapter, UNM_SRE_L2RE_CTL);
	if((status & 0x20000000) != 0) {
		len += sprintf(phanstat + len,"\nSRE currently paused due to L2 IFQ discard failure.");
		ok = 0;
	}

	hdr = NXRD32(adapter, NX_SRE_FBQ_MSGHDR_LOW);
	qmaj = (hdr >> 19) & 0xF;
	qsub = (hdr >> 18) & 0x1;
	qmin = hdr & 0x3FFFF;
	qlen = nx_qm_queuelen(adapter, qmaj, qsub, qmin);
	if(qlen == 0) {
		len += sprintf(phanstat + len,"\nSRE Free Buffer list is currently empty.");
	}

	hdr = NXRD32(adapter, NX_SRE_L1RE_IPQ_LOW);
	qmaj = (hdr >> 19) & 0xF;
	qsub = (hdr >> 18) & 0x1;
	qmin = hdr & 0x3FFFF;
	qlen = nx_qm_queuelen(adapter, qmaj, qsub, qmin);
	if(qlen > 0) {
		len += sprintf(phanstat + len,"\nIPQ is currently non-empty.");
	}

	status = NXRD32(adapter, UNM_SRE_INT_STATUS);
	if((status & 0xc0ff) != 0) {
		ok = 0;
		len += sprintf(phanstat + len,"\nSRE INT STATUS = 0x%08x\n", status);

		if(status & 0x1)
			len += sprintf(phanstat + len,"\nIPQ write pause previously detected.");

		if(status & 0x2)
			len += sprintf(phanstat + len,"\nIPQ write full previously detected.");

		if(status & 0x4)
			len += sprintf(phanstat + len,"\nIFQ write pause previously detected.");

		if(status & 0x8)
			len += sprintf(phanstat + len,"\nIFQ write full previously detected.");

		if(status & 0x10)
			len += sprintf(phanstat + len,"\nMemory Backpressure Timeout previously detected.");

		if(status & 0x20)
			len += sprintf(phanstat + len,"\nDownstream Backpressure Timeout previously detected.");

		if(status & 0x40)
			len += sprintf(phanstat + len,"\nFree Buffer Pool Low Watermark previously detected.");

		if(status & 0x80)
			len += sprintf(phanstat + len,"\nPacket Buffer Error previously detected.");

		if(status & 0x4000)
			len += sprintf(phanstat + len,"\nFM Message Header Error previously detected.");

		if(status & 0x8000)
			len += sprintf(phanstat + len,"\nFM Message Error previously detected.");
	}

	if(ok)
		len += sprintf(phanstat + len,"OK\n");
}

static void nx_epg_status (struct unm_adapter_s *adapter)
{
	uint32_t status;
	uint32_t ok = 1;
	uint32_t hdr;
	int qmaj, qsub, qmin;
	int qlen = 0;
	int len;
	struct fw_dump *fw_dmp = &adapter->fw_dmp;
	char *phanstat = NULL;

	if(fw_dmp->phanstat) {
		phanstat = fw_dmp->phanstat;
	} else {
		return;
	}

	len = strlen(phanstat);

	len += sprintf(phanstat + len,"\nEPG Status: ");

	hdr = NXRD32(adapter, NX_EPG_Q0_RCV_MSG_HDR);
	qmaj = (hdr >> 19) & 0xF;
	qsub = (hdr >> 18) & 0x1;
	qmin = hdr & 0x3FFFF;
	qlen += nx_qm_queuelen(adapter, qmaj, qsub, qmin);

	hdr = NXRD32(adapter, NX_EPG_Q1_RCV_MSG_HDR);
	qmaj = (hdr >> 19) & 0xF;
	qsub = (hdr >> 18) & 0x1;
	qmin = hdr & 0x3FFFF;
	qlen += nx_qm_queuelen(adapter, qmaj, qsub, qmin);

	hdr = NXRD32(adapter, NX_EPG_Q2_RCV_MSG_HDR);
	qmaj = (hdr >> 19) & 0xF;
	qsub = (hdr >> 18) & 0x1;
	qmin = hdr & 0x3FFFF;
	qlen += nx_qm_queuelen(adapter, qmaj, qsub, qmin);

	hdr = NXRD32(adapter, NX_EPG_Q3_RCV_MSG_HDR);
	qmaj = (hdr >> 19) & 0xF;
	qsub = (hdr >> 18) & 0x1;
	qmin = hdr & 0x3FFFF;
	qlen += nx_qm_queuelen(adapter, qmaj, qsub, qmin);

	if(qlen) {

		ok = 0;
		len += sprintf(phanstat + len,"\nOne or more EPG queues are backed up. EPG could be wedged.");
		status = nx_testmux(adapter, NX_EPG_TM, 6);
		if(status & 0x1) {
			len += sprintf(phanstat + len,"\nEPG->NIU interface 0 is backed up.");
		}
		if(status & 0x2) {
			len += sprintf(phanstat + len,"\nEPG->NIU interface 1 is backed up.");
		}
		if(status & 0x4) {
			len += sprintf(phanstat + len,"\nEPG->NIU interface 2 is backed up.");
		}
		if(status & 0x8) {
			len += sprintf(phanstat + len,"\nEPG->NIU interface 3 is backed up.");
		}

		status = nx_testmux(adapter, NX_EPG_TM, 0x41);
		if(status == 0x1000) {
			len += sprintf(phanstat + len,"\nEPG Overfetch Hang.");
		}

		status = nx_testmux(adapter, NX_EPG_TM, 0x46);
		if(status == 0x1000) {
			len += sprintf(phanstat + len,"\nEPG Overfetch Hang.");
		}

		status = nx_testmux(adapter, NX_EPG_TM, 0x40);
		if((status & 0x3F) == 0x15) {
			len += sprintf(phanstat + len,"\nBad fetch from PCIE (bad host address?).");
		}

		status = nx_testmux(adapter, NX_EPG_TM, 0x45);
		if((status & 0x3F) == 0x15) {
			len += sprintf(phanstat + len,"\nBad fetch from PCIE (bad host address?).");
		}
		status = nx_testmux(adapter, NX_EPG_TM, 0x7);
		if((status & 0xFFFF) > 0x2000) {
			len += sprintf(phanstat + len,"\n MSG0 descriptor length too long 0x%x", status & 0xFFFF);
		}

		status = nx_testmux(adapter, NX_EPG_TM, 0x8);
		if((status & 0xFFFF) > 0x2000) {
			len += sprintf(phanstat + len,"\n MSG1 descriptor length too long 0x%x", status & 0xFFFF);
		}

		status = NXRD32(adapter, NX_EPG_MSG_ERR_CNT);
		if(status != 0) {
			len += sprintf(phanstat + len,"EPG received %d zero-length q msgs\n", status);
		}

		status = NXRD32(adapter, NX_EPG_SZ_PARAM_MSG_CNT);
		if(status != 0) {
			len += sprintf(phanstat + len,"EPG received %d zero-length descriptors\n", status);
		}
	}

	status = NXRD32(adapter, UNM_SRE_INT_STATUS);
	if((status & 0x3f00)) {
		ok = 0;
		if(status & 0x100)
			len += sprintf(phanstat + len,"\n EPG Message Buffer Error.");
		if(status & 0x200)
			len += sprintf(phanstat + len,"\n EPG Queue Read Timeout.");
		if(status & 0x400)
			len += sprintf(phanstat + len,"\n EPG Queue Write Timeout.");
		if(status & 0x800)
			len += sprintf(phanstat + len,"\n EPG Completion Queue Write Full.");
		if(status & 0x1000)
			len += sprintf(phanstat + len,"\n EPG Message Checksum Error.");
		if(status & 0x2000)
			len += sprintf(phanstat + len,"\n EPG MTL Queue Fetch Timeout.");
	}

	if(ok)
		len += sprintf(phanstat + len,"OK.\n");
}

static void nx_peg_status(struct unm_adapter_s *adapter)
{
#if 0
	uint32_t pc0;
	uint32_t pc1;
	uint32_t pc2;
	uint32_t pc3;
	uint32_t pc4;

	pc0 = NXRD32(adapter, NX_PEG0_STATUS);
	pc1 = NXRD32(adapter, NX_PEG1_STATUS);
	pc2 = NXRD32(adapter, NX_PEG2_STATUS);
	pc3 = NXRD32(adapter, NX_PEG3_STATUS);
	pc4 = NXRD32(adapter, NX_PEG4_STATUS);

#endif
}

static void nx_phanstat(struct unm_adapter_s *adapter)
{
	uint32_t status;
	uint32_t ok = 1;
	struct fw_dump *fw_dmp = &adapter->fw_dmp;
	char *phanstat = NULL;
	int len = 0;
	uint32_t phanstat_size = 4096 * 2;

	if(fw_dmp->phanstat) {
		phanstat = fw_dmp->phanstat;
	} else {
		phanstat = vmalloc(phanstat_size + 1);
		if(phanstat == NULL) 
			return;
		fw_dmp->phanstat = phanstat;
	}	

	memset(phanstat, 0, phanstat_size);

	len += sprintf(phanstat + len,"Phantom HW Status:\n");
	len += sprintf(phanstat + len,"PCIE Status: ");
	status = NXRD32(adapter, NX_PCIE_UNCE_STATUS);

	if(status == 0) {
		len += sprintf(phanstat + len,"OK\n");
	} else {
		uint32_t mask;
		uint32_t sev;

		if(status & 0xFFE4AFEF)
			len += sprintf(phanstat + len,"\nUndefined error(s) detected.");
		len += sprintf(phanstat + len,"\nErrors detected on PCIE: 0x00100104=0x%08x", status);
		mask = NXRD32(adapter, NX_PCIE_UNCE_MASK);
		len += sprintf(phanstat + len,"\n0x00100108=0x%08x",mask);
		sev = NXRD32(adapter, NX_PCIE_UNCE_SEV);
		len += sprintf(phanstat + len,"\n0x0010010C=0x%08x",sev);
	}

	len += sprintf(phanstat + len,"\nMN Status: ");
	status = NXRD32(adapter, UNM_CRB_DDR_NET);
	if((status & 0x10) == 0x10) {
		uint32_t cnt;
		uint32_t fatal_err;
		cnt = NXRD32(adapter, UNM_CRB_DDR_NET + 0x80);
		fatal_err = NXRD32(adapter, UNM_CRB_DDR_NET + 0x14);
		if(cnt != 0) {
			len += sprintf(phanstat + len,"\n%d ECC Errors detected in MN", cnt);
			ok = 0;
		}
		if((fatal_err & 0x02) != 0) {
			len += sprintf(phanstat + len,"\nUncorrectable ECC error detected in MN");
			ok = 0;
		}
	} else {
		ok = 1;
	}

	if(ok)
		len += sprintf(phanstat + len,"OK\n");

	ok = 1;
	len += sprintf(phanstat + len,"\nSN Status: ");
	status = NXRD32(adapter, UNM_CRB_QDR_NET);
	if((status & 0x10) == 0x10) {
		uint32_t cnt;
		uint32_t fatal_err;
		cnt = NXRD32(adapter, UNM_CRB_QDR_NET + 0x80);
		fatal_err = NXRD32(adapter, UNM_CRB_QDR_NET + 0x14);
		if(cnt != 0) {
			len += sprintf(phanstat + len,"\n%d ECC Errors detected in SN", cnt);
			ok = 0;
		}
		if((fatal_err & 0x02) != 0) {
			len += sprintf(phanstat + len,"\nUncorrectable ECC error detected in SN");
			ok = 0;
		}
	} else {
		ok = 1;
	}

	if(ok)
		len += sprintf(phanstat + len,"OK\n");

	ok = 1;
	len += sprintf(phanstat + len,"\nDMA Status: ");
	status = NXRD32(adapter, UNM_DMA_COMMAND(0));
	if((status & 0x03) == 0x03) {
		len += sprintf(phanstat + len,"\n DMA0 is busy.");
		ok = 0;
	}

	status = NXRD32(adapter, UNM_DMA_COMMAND(1));
	if((status & 0x03) == 0x03) {
		len += sprintf(phanstat + len,"\n DMA0 is busy.");
		ok = 0;
	}

	status = NXRD32(adapter, UNM_DMA_COMMAND(2));
	if((status & 0x03) == 0x03) {
		len += sprintf(phanstat + len,"\n DMA0 is busy.");
		ok = 0;
	}

	status = NXRD32(adapter, UNM_DMA_COMMAND(3));
	if((status & 0x03) == 0x03) {
		len += sprintf(phanstat + len,"\n DMA0 is busy.");
		ok = 0;
	}

	if(ok == 1)
		len += sprintf(phanstat + len,"OK\n");

	nx_sre_status(adapter);

	nx_epg_status(adapter);

	nx_peg_status(adapter);

	phanstat[phanstat_size] = '\0';
}

static void nx_pegconsole(struct unm_adapter_s *adapter)
{
    uint32_t enabled;
    uint32_t peg;
    uint32_t head;
    uint32_t limit;
    uint32_t tail;
    uint64_t data;
    uint32_t shift;
    uint32_t ascii_char;
	struct fw_dump *fw_dmp = &adapter->fw_dmp;

    for (peg = 0; peg < NX_NIC_NUM_PEGS; peg++) {
		char *pcon = NULL;
		uint32_t pcon_len = 0;
		int len = 0;

		if (fw_dmp->pcon[peg]) {
			vfree(fw_dmp->pcon[peg]);
			fw_dmp->pcon[peg] = NULL;
		}

		enabled = NXRD32(adapter, NX_PCONS_ENABLE(peg));
        if ((enabled & 0xFFFFFFFF) != 0xCAFEBABE) {

			pcon = vmalloc(64);
			if(pcon == NULL)
				continue;

			memset(pcon, 0, 64);

			fw_dmp->pcon[peg] = pcon;

            sprintf(pcon, "Output not enabled by peg%x\n", peg);
            continue;
        }

		pcon_len = NXRD32(adapter, NX_PCONS_LEN(peg));

		pcon = vmalloc(pcon_len + 1);
		if(pcon == NULL)
			continue;
		fw_dmp->pcon[peg] = pcon;

		memset(pcon, 0, pcon_len + 1);

        head = NXRD32(adapter, NX_PCONS_HEAD(peg));
        limit = head + pcon_len;
        tail = NXRD32(adapter, NX_PCONS_TAIL(peg));
        len = sprintf(pcon, "Peg %d ::\n", peg);

        while ((head != tail) && (head < limit)) {
            adapter->unm_nic_pci_mem_read(adapter, head & 0xFFFFFFF8,
                    &data, sizeof(data));
            shift = 8 * (head & 0x7);
            ascii_char = (data >> shift) & 0xFF;
            len += sprintf(pcon + len, "%c", ascii_char);
            head++;
        }
    }
}

static void nx_sre_statistics (struct unm_adapter_s *adapter)
{
	char *srestat;
	struct fw_dump *fw_dmp = &adapter->fw_dmp;
	uint32_t count;
	int len = 0;
	uint32_t srestats_size = 4096;

	if (fw_dmp->srestat) {
		srestat = fw_dmp->srestat;
	} else {
		srestat = vmalloc(srestats_size + 1);
		if(srestat == NULL)
			return;
		fw_dmp->srestat = srestat;
	}

	memset(srestat, 0, srestats_size);

	len += sprintf(srestat + len,"SRE Packet Statistics:\n");
	count = NXRD32(adapter, NX_SRE_IN_GOOD);
	len += sprintf(srestat + len,"Inbound Good Packet Count: %u\n", count);
	count = NXRD32(adapter, NX_SRE_IN_BAD);
	len += sprintf(srestat + len,"Inbound Bad Packet Count : %u\n", count);
	count = NXRD32(adapter, NX_SRE_IN_CSUM);
	len += sprintf(srestat + len,"Inbound Checksum Errors  : %u\n", count);
	count = NXRD32(adapter, NX_SRE_RE1_IPQ);
	len += sprintf(srestat + len,"RE1->IPQ Count           : %u\n", count);
	count = NXRD32(adapter, NX_SRE_RE1_ERR);
	len += sprintf(srestat + len,"RE1 Error Pkt Count      : %u\n", count);
	count = NXRD32(adapter, NX_SRE_RE1_ARP);
	len += sprintf(srestat + len,"RE1->ARP Count           : %u\n", count);
	count = NXRD32(adapter, NX_SRE_RE1_ICMP);
	len += sprintf(srestat + len,"RE1->ICMP Count          : %u\n", count);
	count = NXRD32(adapter, NX_SRE_RE1_FRAG);
	len += sprintf(srestat + len,"RE1->Frag Count          : %u\n", count);
	count = NXRD32(adapter, NX_SRE_RE1_ALIEN);
	len += sprintf(srestat + len,"RE1 Alien Pkt Count      : %u\n", count);
	count = NXRD32(adapter, NX_SRE_RE1_L2_MAP);
	len += sprintf(srestat + len,"RE1 L2 mapping Pkt Count : %u\n", count);
	count = NXRD32(adapter, NX_SRE_RE1_RDMA_GOOD);
	len += sprintf(srestat + len,"RE1 Rdma Good Pkt Count  : %u\n", count);
	count = NXRD32(adapter, NX_SRE_RE1_RDMA_BAD);
	len += sprintf(srestat + len,"RE1 Rdma Bad Pkt Count   : %u\n", count);
	count = NXRD32(adapter, NX_SRE_RE1_IPSEC);
	len += sprintf(srestat + len,"RE1 Ipsec Pkt Count      : %u\n", count);
	count = NXRD32(adapter, NX_SRE_RE1_SYN);
	len += sprintf(srestat + len,"RE1 SYN Pkt Count        : %u\n", count);
	count = NXRD32(adapter, NX_SRE_RE1_IPQ_FD);
	len += sprintf(srestat + len,"RE1 Ipq Full Discard Cnt : %u\n", count);
	count = NXRD32(adapter, NX_SRE_RE2_IFQ_MAP);
	len += sprintf(srestat + len,"RE2->IFQ (mapped) Count  : %u\n", count);
	count = NXRD32(adapter, NX_SRE_RE2_FBQ);
	len += sprintf(srestat + len,"RE2->FBQ (unmapped)Count : %u\n", count);
	count = NXRD32(adapter, NX_SRE_RE2_IFDQ);
	len += sprintf(srestat + len,"RE2->IFDQ (full discard) : %u\n", count);
	count = NXRD32(adapter, NX_SRE_IPV6);
	len += sprintf(srestat + len,"IPv6 packets             : %u\n", count);
	count = NXRD32(adapter, NX_SRE_IPV6_EXECP);
	sprintf(srestat + len,"IPv6 exception packets   : %u\n", count);
	srestat[srestats_size] = '\0';
}

static void nx_epg_statistics (struct unm_adapter_s *adapter)
{
	char *epgstat;
	struct fw_dump *fw_dmp = &adapter->fw_dmp;
	uint32_t count;
	int len = 0;
	uint32_t epgstats_size = 1024;

	if (fw_dmp->epgstat) {
		epgstat = fw_dmp->epgstat;
	} else {
		epgstat = vmalloc(epgstats_size + 1);
		if(epgstat == NULL)
			return;
		fw_dmp->epgstat = epgstat;
	}

	memset(epgstat, 0, epgstats_size);

	count = NXRD32(adapter, NX_EPG_TOTAL_FRAME_COUNT);
	len += sprintf(epgstat + len,"Total Sent Packets (EPG)	: %u\n", count);
	count = NXRD32(adapter, NX_EPG_IF0_FRAME_COUNT);
	len += sprintf(epgstat + len,"Port 0 Sent Packets		: %u\n", count);
	count = NXRD32(adapter, NX_EPG_IF1_FRAME_COUNT);
	len += sprintf(epgstat + len,"Port 1 Sent Packets		: %u\n", count);
	count = NXRD32(adapter, NX_EPG_IF2_FRAME_COUNT);
	len += sprintf(epgstat + len,"Port 2 Sent Packets		: %u\n", count);
	count = NXRD32(adapter, NX_EPG_IF3_FRAME_COUNT);
	len += sprintf(epgstat + len,"Port 3 Sent Packets		: %u\n", count);
	epgstat[epgstats_size] = '\0';
}

static void nx_nic_regs (struct unm_adapter_s *adapter)
{
	char *nicregs;
	struct fw_dump *fw_dmp = &adapter->fw_dmp;
	int len = 0;
	uint64_t rx_count;
	uint64_t tx_count;
	uint64_t port_count;
	uint32_t base;
	uint32_t i;
	uint32_t nicregs_size = 2 * 4096;

	if (fw_dmp->nicregs) {
		nicregs = fw_dmp->nicregs;
	} else {
		nicregs = vmalloc(nicregs_size + 1);
		if(nicregs == NULL)
			return;
		fw_dmp->nicregs = nicregs;
	}

	memset(nicregs, 0, nicregs_size);

	base = NXRD32(adapter, NX_NIC_BASE_REG);

	adapter->unm_nic_pci_mem_read(adapter,
			base + 8, &rx_count, sizeof(rx_count));
	rx_count &= 0xffffffff;
	
	adapter->unm_nic_pci_mem_read(adapter,
			base + 20, &tx_count, sizeof(tx_count));
	tx_count &= 0xffffffff;

	adapter->unm_nic_pci_mem_read(adapter,
			base + 32, &port_count, sizeof(port_count));
	port_count &= 0xffffffff;

	for(i = 0; i < rx_count; i++) {
		uint64_t rx_off;
		uint64_t rx_size;
		uint32_t rx_base;
		uint64_t is_active;
		uint64_t rx_addr;
		uint64_t temp;
		uint32_t rds_crb;
		uint32_t rds_val;
		uint64_t rds_addr;
		uint32_t sds_crb;
		uint32_t sds_val;
		uint64_t sds_addr;
		uint32_t int_crb;
		uint32_t int_val;
		uint32_t peg_id;
		int j;

		if (len > nicregs_size - 1024)
			break;

		adapter->unm_nic_pci_mem_read(adapter,
				base, &rx_off, sizeof(rx_off));
		rx_off &= 0xffffffff;

		adapter->unm_nic_pci_mem_read(adapter,
				base + 4, &rx_size, sizeof(rx_size));
		rx_size &= 0xffffffff;

		rx_base = base + rx_off + i * rx_size;

		adapter->unm_nic_pci_mem_read(adapter,
				rx_base, &is_active, sizeof(is_active));
		is_active &= 0xffffffff;

		if(is_active == 0)
			continue;

		adapter->unm_nic_pci_mem_read(adapter,
				rx_base + 4, &rx_addr, sizeof(rx_addr));
		rx_addr &= 0xffffffff;

		peg_id = (i & 0x1) ? 1 : 0;

		len += sprintf(nicregs + len,"Rx Context %u Peg %u\n", i, peg_id);
		len += sprintf(nicregs + len,"Info Base 		: 0x%08x\n", rx_base);
		len += sprintf(nicregs + len,"Card RxCtx Addr 	: 0x%08llx\n", rx_addr);

		for(j = 0; j < 2; j++) {
			adapter->unm_nic_pci_mem_read(adapter,
					rx_base + 8 + 4 * j, &temp, sizeof(temp));
			temp &= 0xffffffff;

			rds_crb = (temp & 0xfff) + UNM_CAM_RAM_BASE;
			if(rds_crb == UNM_CAM_RAM_BASE)
				continue;

			len += sprintf(nicregs + len, "RDS Ring %d:%d\n",i,j);
			rds_val = NXRD32(adapter, rds_crb);
			
			adapter->unm_nic_pci_mem_read(adapter,
					rx_base + 88 + 4 * j, &rds_addr, sizeof(rds_addr));
			rds_addr &= 0xffffffff;

			len += sprintf(nicregs + len, "RDS CRB %d : 0x%08x VAL 0x%08x\n", j, rds_crb, rds_val);
			len += sprintf(nicregs + len, "Card RDS Addr 	: 0x%08llx\n", rds_addr);
		}

		for(j = 0; j < 8; j++) {
			adapter->unm_nic_pci_mem_read(adapter,
					rx_base + 24 + 4 * j, &temp, sizeof(temp));
			temp &= 0xffffffff;

			sds_crb = (temp & 0xfff) + UNM_CAM_RAM_BASE;

			if(sds_crb == UNM_CAM_RAM_BASE)
				continue;

			adapter->unm_nic_pci_mem_read(adapter,
					rx_base + 56 + 4 * j, &temp, sizeof(temp));
			temp &= 0xffffffff;

			int_crb = (temp & 0xfff) + UNM_CAM_RAM_BASE;

			len += sprintf(nicregs + len, "SDS Ring %d:%d\n",i,j);
			sds_val = NXRD32(adapter, sds_crb);
			int_val = NXRD32(adapter, int_crb);

			adapter->unm_nic_pci_mem_read(adapter,
					rx_base + 104 + 4 * j, &sds_addr, sizeof(sds_addr));
			sds_addr &= 0xffffffff;

			len += sprintf(nicregs + len, "SDS CRB %d : 0x%08x VAL 0x%08x\n", j, sds_crb, sds_val);
			len += sprintf(nicregs + len, "INT CRB %d : 0x%08x VAL 0x%08x\n", j, int_crb, int_val);
			len += sprintf(nicregs + len, "Card RDS Addr 	: 0x%08llx\n", sds_addr);
		}
	}

	for(i = 0; i < tx_count; i++) {
		uint64_t tx_off;
		uint64_t tx_size;
		uint32_t tx_base;
		uint64_t is_active;
		uint64_t tx_addr;
		uint64_t temp;
		uint32_t cds_crb;
		uint32_t cds_val;
		uint32_t peg_id;

		if (len > nicregs_size - 1024)
			break;

		adapter->unm_nic_pci_mem_read(adapter,
				base + 12, &tx_off, sizeof(tx_off));
		tx_off &= 0xffffffff;

		adapter->unm_nic_pci_mem_read(adapter,
				base + 16, &tx_size, sizeof(tx_size));
		tx_size &= 0xffffffff;

		tx_base = base + tx_off + i * tx_size;

		adapter->unm_nic_pci_mem_read(adapter,
				tx_base, &is_active, sizeof(is_active));
		is_active &= 0xffffffff;

		if(is_active == 0)
			continue;

		adapter->unm_nic_pci_mem_read(adapter,
				tx_base + 4, &tx_addr, sizeof(tx_addr));
		tx_addr &= 0xffffffff;

		peg_id = (i & 0x1) ? 3 : 2;
	
		len += sprintf(nicregs + len,"Tx Context %u Peg %u\n", i, peg_id);
		len += sprintf(nicregs + len,"Info Base 		: 0x%08x\n", tx_base);
		len += sprintf(nicregs + len,"Card TxCtx Addr 	: 0x%08llx\n", tx_addr);

		adapter->unm_nic_pci_mem_read(adapter,
				tx_base + 8, &temp, sizeof(temp));
		temp &= 0xffffffff;
		
		cds_crb = (temp & 0xfff) + UNM_CAM_RAM_BASE;
		if(cds_crb == UNM_CAM_RAM_BASE)
			continue;
		cds_val = NXRD32(adapter, cds_crb);
		len += sprintf(nicregs + len, "CDS CRB: 0x%08x, 0x%08x \n", cds_crb, cds_val);
	}

	for(i = 0; i < port_count; i++) {
		uint64_t port_off;
		uint64_t port_size;
		uint32_t port_base;
		uint64_t port_addr;
		uint32_t peg_id;

		if (len > nicregs_size - 1024)
			break;

		adapter->unm_nic_pci_mem_read(adapter,
				base + 24, &port_off, sizeof(port_off));
		port_off &= 0xffffffff;

		adapter->unm_nic_pci_mem_read(adapter,
				base + 28, &port_size, sizeof(port_size));
		port_size &= 0xffffffff;

		port_base = base + port_off + i * port_size;

		adapter->unm_nic_pci_mem_read(adapter,
				port_base, &port_addr, sizeof(port_addr));
		port_addr &= 0xffffffff;

		peg_id = (i & 0x1) ? 1 : 3;

		len += sprintf(nicregs + len, "Port %i Peg %i\n", i, peg_id);
		len += sprintf(nicregs + len, "Card Port Addr : %llx\n", port_addr);
	}
	nicregs[nicregs_size] = '\0';
}

static void nx_mac_statistics (struct unm_adapter_s *adapter)
{
	char *macstat;
	struct fw_dump *fw_dmp = &adapter->fw_dmp;
	uint32_t port;
	uint32_t pm_8023ap = 0;
	uint32_t count;
	uint32_t macstats_size = 4096;

	for(port  = 0; port < UNM_NIU_MAX_XG_PORTS; port++) {
		int len = 0;
		if (fw_dmp->macstat[port]) {
			macstat = fw_dmp->macstat[port];
		} else {
			macstat = vmalloc(macstats_size + 1);
			if(macstat == NULL)
				return;
			fw_dmp->macstat[port] = macstat;
		}

		memset(macstat, 0, macstats_size);

		if(adapter->ahw.board_type == UNM_NIC_GBE) {
			int first_port = 0; //Will change when we consider tropper, later;
			int port_count = 4;
			int j;
			
			len += sprintf(macstat + len, "Quad Gig Card\n");
			for(j = first_port; j < port_count; j++) {
				count = NXRD32(adapter, NX_NIC_QG_RCV_PACKETS + j * 4);	
				len += sprintf(macstat + len, "RX Pkt Count Port %d : %u\n", j, count); 
			}

			for(j = first_port; j < port_count; j++) {
				NXWR32(adapter, NX_NIC_QG_DRP_PACKETS_W, j);
				count = NXRD32(adapter, NX_NIC_QG_DRP_PACKETS_R);	
				len += sprintf(macstat + len, "RX Dropped Count Port %d : %u\n", j, count); 
			}

			macstat[macstats_size] = '\0';
			return;
		}

		if(pm_8023ap) {
			//Implement 8023ap later
			macstat[macstats_size] = '\0';
			return;
		}
	
		len += sprintf(macstat + len, "Port %d\n", port);
			
		count = NXRD32(adapter, NX_NIU_XG_TX_BYTES(port));
		len += sprintf(macstat + len, "TX Byte Count : %u\n", count);

		count = NXRD32(adapter, NX_NIU_XG_TX_FRAMES(port));
		len += sprintf(macstat + len, "TX Frame Count : %u\n", count);

		count = NXRD32(adapter, NX_NIU_XG_RX_BYTES(port));
		len += sprintf(macstat + len, "RX Byte Count : %u\n", count);

		count = NXRD32(adapter, NX_NIU_XG_RX_FRAMES(port));
		len += sprintf(macstat + len, "RX Frame Count : %u\n", count);

		count = NXRD32(adapter, NX_NIU_XG_TOT_ERR(port));
		len += sprintf(macstat + len, "Total Error  Count : %u\n", count);

		count = NXRD32(adapter, NX_NIU_XG_MCAST(port) );
		len += sprintf(macstat + len, "Multicast  Frame Count : %u\n", count);

		count = NXRD32(adapter, NX_NIU_XG_UNICAST(port));
		len += sprintf(macstat + len, "Unicast Frame Count : %u\n", count);

		count = NXRD32(adapter, NX_NIU_XG_CRC_ERR(port));
		len += sprintf(macstat + len, "CRC Error Count : %u\n", count);

		count = NXRD32(adapter, NX_NIU_XG_RX_OSZ_FRM(port));
		len += sprintf(macstat + len, "RX Over sized Frame Count : %u\n", count);

		count = NXRD32(adapter, NX_NIU_XG_RX_USZ_FRM(port));
		len += sprintf(macstat + len, "RX Under sized Frame  Count : %u\n", count);

		count = NXRD32(adapter, NX_NIU_XG_LOCAL_ERR(port));
		len += sprintf(macstat + len, "Local Error Count : %u\n", count);

		count = NXRD32(adapter, NX_NIU_XG_REMOTE_ERR(port));
		len += sprintf(macstat + len, "Remote Error Count : %u\n", count);

		count = NXRD32(adapter, NX_NIU_XG_RX_CTL_CHAR(port));
		len += sprintf(macstat + len, "RX Control Char Count : %u\n", count);

		count = NXRD32(adapter, NX_NIU_XG_RX_PAUSE(port));
		len += sprintf(macstat + len, "RX Pause Frame Count : %u\n", count);

		count = NXRD32(adapter, NX_NIU_XG_STATUS(port));
		len += sprintf(macstat + len, "XGE status Reg : %u\n", count);

		NXWR32(adapter, NX_NIC_XG_DRP_PACKETS_W, 0);
		count = NXRD32(adapter, NX_NIC_XG_DRP_PACKETS_R);
		len += sprintf(macstat + len, "FIFO_XG RX Dropped : %u\n", count);

		count = NXRD32(adapter, NX_NIU_XG_OVERFLOW(port));
		len += sprintf(macstat + len, "NIU Overflow Dropped : %u\n", count);
		macstat[macstats_size] = '\0';
	}
}
#endif //COLLECT_FW_LOGS

int nx_nic_minidump(struct unm_adapter_s *adapter)
{
    uint64_t cur_fn = (uint64_t)nx_nic_minidump;

        if (adapter->mdump.fw_supports_md && adapter->mdump.dump_needed) {
            if (!adapter->mdump.md_capture_buff) {
                adapter->mdump.md_capture_buff = vmalloc(adapter->mdump.md_dump_size);
                if (!adapter->mdump.md_capture_buff) {
                    nx_nic_print3(adapter, "Unable to allocate memory for minidump "
                            "capture_buffer(%d bytes).\n", adapter->mdump.md_dump_size);
					NX_NIC_TRC_FN(adapter, cur_fn, -ENOMEM);
                    return -ENOMEM;
                }
            } else {
                nx_nic_print4(adapter, "Overwriting previously collected firmware minidump.\n");
			}

            memset(adapter->mdump.md_capture_buff, 0, adapter->mdump.md_dump_size);
            if (nx_nic_collect_minidump(adapter)) {
                adapter->mdump.has_valid_dump = 0;
                nx_nic_print3(adapter, "Error in collecting firmware minidump.\n");
				return -EFAULT;
            } else {
                adapter->mdump.md_timestamp = jiffies;
                adapter->mdump.has_valid_dump = 1;
                nx_nic_print4(adapter, "Successfully collected firmware minidump.\n");
				return 0;
            }
    } else {
		nx_nic_print3(adapter, "Minidump support is not available.\n");
	}
	return 0;
}

#ifdef COLLECT_FW_LOGS 
static void nx_nic_collect_fw_logs (struct unm_adapter_s *adapter)
{
	if(adapter->fw_dmp.enabled == 0)
		return;

    nx_pegconsole(adapter);
    nx_phanstat(adapter);
	nx_sre_statistics(adapter);
	nx_epg_statistics(adapter);
	nx_nic_regs(adapter);
	nx_mac_statistics(adapter);
	nx_nic_print4(adapter, "Collected firmware logs.\n");
}
#endif

/*
 * Called when a network interface is made active
 * Returns 0 on success, negative value on failure
 */
static int unm_nic_open(struct net_device *netdev)
{
	struct unm_adapter_s *adapter = (struct unm_adapter_s *)netdev_priv(netdev);
	int err = 0;
	uint64_t cur_fn = (uint64_t) unm_nic_open;

	NX_NIC_TRC_FN(adapter, cur_fn, 0);

	err = unm_nic_attach(adapter);
	if (err != 0) {
		nx_nic_print3(adapter, "Failed to Attach to device\n");
		NX_NIC_TRC_FN(adapter, cur_fn, err);
		return (err);
	}

	if (netif_queue_stopped(netdev)) {
		netif_start_queue(netdev);
	}

	adapter->state = PORT_UP;

	NX_NIC_TRC_FN(adapter, cur_fn, 0);
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
	return NULL;
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
	uint64_t cur_fn = (uint64_t) nx_free_sts_rings;

	NX_NIC_TRC_FN(adapter, cur_fn, 0);
	ctx_id = nxhal_rx_ctx->this_id;
	for (i = 0; i < nxhal_rx_ctx->num_sds_rings; i++) {
		nxhal_sds_ring = &nxhal_rx_ctx->sds_rings[i];
		host_sds_ring = nxhal_sds_ring->os_data;

		if(host_sds_ring == NULL) {
			continue;
		}

		host_sds_ring->netdev = NULL;

		if (nxhal_sds_ring->host_addr) {
			NX_NIC_TRC_FN(adapter, cur_fn, nxhal_sds_ring->host_addr);
			pci_free_consistent(host_sds_ring->pci_dev,
					STATUS_DESC_RINGSIZE(adapter->
						MaxRxDescCount),
					nxhal_sds_ring->host_addr,
					nxhal_sds_ring->host_phys);
			nxhal_sds_ring->host_addr = NULL;
		}
		
		host_sds_ring->ring = NULL;
		host_sds_ring->adapter = NULL;	
	}
}

/*
 * Allocate the receive status rings for RSS.
 */
static int nx_alloc_sts_rings(struct unm_adapter_s *adapter,
			      nx_host_rx_ctx_t *nxhal_rx_ctx, int cnt)
{
	int i;
	int j;
	int ctx_id ;
	void *addr;
	nx_host_sds_ring_t *nxhal_sds_ring = NULL;
	sds_host_ring_t	   *host_sds_ring = NULL;
	ctx_id = nxhal_rx_ctx->this_id;
	uint64_t cur_fn = (uint64_t) nx_alloc_sts_rings;

	NX_NIC_TRC_FN(adapter, cur_fn, ctx_id);
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
			NX_NIC_TRC_FN(adapter, cur_fn, ctx_id);
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

		host_sds_ring->netdev = adapter->netdev;
		SET_SDS_NETDEV_NAME(host_sds_ring,
				adapter->netdev->name, ctx_id, i);

		if (adapter->flags & UNM_NIC_MSIX_ENABLED) {
			SET_SDS_NETDEV_IRQ(host_sds_ring,
					adapter->msix_entries[i + ctx_id].vector);

			nxhal_sds_ring->msi_index =
				adapter->msix_entries[i + ctx_id].entry;
			NX_NIC_TRC_FN(adapter, cur_fn, adapter->msix_entries[i + ctx_id].vector);
		} else {
			SET_SDS_NETDEV_IRQ(host_sds_ring,
					adapter->netdev->irq);
			NX_NIC_TRC_FN(adapter, cur_fn, adapter->netdev->irq);
		}
	}
	NX_NIC_TRC_FN(adapter, cur_fn, 0);
	return (0);

error_done:
	nx_free_sts_rings(adapter,nxhal_rx_ctx);
	NX_NIC_TRC_FN(adapter, cur_fn, ENOMEM);
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
	uint64_t cur_fn = (uint64_t) nx_register_irq;

	NX_NIC_TRC_FN(adapter, cur_fn, 0);

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
		NX_NIC_TRC_FN(adapter, cur_fn, adapter->intr_handler);

		rv = request_irq(GET_SDS_NETDEV_IRQ(host_sds_ring), 
				adapter->intr_handler, flags, 
				GET_SDS_NETDEV_NAME(host_sds_ring), 
				nxhal_sds_ring);

		if (rv) {
			nx_nic_print3(adapter, "%s Unable to register "
					"the interrupt service routine\n",
					GET_SDS_NETDEV_NAME(host_sds_ring));
			cnt = i;
			NX_NIC_TRC_FN(adapter, cur_fn, rv);
			goto error_done;
		}
		host_sds_ring->irq_allocated = 1;
		NX_NIC_TRC_FN(adapter, cur_fn, GET_SDS_NETDEV_IRQ(host_sds_ring));
	}

	NX_NIC_TRC_FN(adapter, cur_fn, 0);
	return (0);

error_done:
	for (i = 0; i < cnt; i++) {
		nxhal_sds_ring = &nxhal_host_rx_ctx->sds_rings[i];
		host_sds_ring = (sds_host_ring_t *)nxhal_sds_ring->os_data;
		if(host_sds_ring->irq_allocated) {
			host_sds_ring->irq_allocated = 0;
			NX_NIC_TRC_FN(adapter, cur_fn, GET_SDS_NETDEV_IRQ(host_sds_ring));
			free_irq(GET_SDS_NETDEV_IRQ(host_sds_ring) , nxhal_sds_ring);
		}
	}

	NX_NIC_TRC_FN(adapter, cur_fn, rv);
	return (rv);
}

static void nx_synchronize_irq(struct unm_adapter_s *adapter,
					nx_host_rx_ctx_t *nxhal_host_rx_ctx)
{
	int i;
	nx_host_sds_ring_t      *nxhal_sds_ring = NULL;
	sds_host_ring_t         *host_sds_ring = NULL;
	uint64_t cur_fn = (uint64_t) nx_synchronize_irq;
	
	for (i = 0; i < nxhal_host_rx_ctx->num_sds_rings; i++) {
		nxhal_sds_ring = &nxhal_host_rx_ctx->sds_rings[i];
		host_sds_ring = (sds_host_ring_t *)nxhal_sds_ring->os_data;
		if(host_sds_ring && host_sds_ring->irq_allocated) {
			NX_NIC_TRC_FN(adapter, cur_fn, GET_SDS_NETDEV_IRQ(host_sds_ring));
			synchronize_irq(GET_SDS_NETDEV_IRQ(host_sds_ring));
		}
	}
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
	uint64_t cur_fn = (uint64_t) nx_unregister_irq;

	NX_NIC_TRC_FN(adapter, cur_fn, 0);

	for (i = 0; i < nxhal_host_rx_ctx->num_sds_rings; i++) {
		nxhal_sds_ring = &nxhal_host_rx_ctx->sds_rings[i];
		host_sds_ring = (sds_host_ring_t *)nxhal_sds_ring->os_data;
		if(host_sds_ring && host_sds_ring->irq_allocated) {
			host_sds_ring->irq_allocated = 0;
			NX_NIC_TRC_FN(adapter, cur_fn, GET_SDS_NETDEV_IRQ(host_sds_ring));
			free_irq(GET_SDS_NETDEV_IRQ(host_sds_ring), nxhal_sds_ring);
		}
	}

	NX_NIC_TRC_FN(adapter, cur_fn, 0);
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
	uint64_t cur_fn = (uint64_t) receive_peg_ready;

	NX_NIC_TRC_FN(adapter, cur_fn, 0);
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

	NX_NIC_TRC_FN(adapter, cur_fn, err);
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
	uint64_t cur_fn = (uint64_t) unm_nic_hw_resources;
#ifdef EPG_WORKAROUND
	void *pause_addr;
#endif

	/* Synchronize with Receive peg */
	err = receive_peg_ready(adapter);
	if (err) {
		NX_NIC_TRC_FN(adapter, cur_fn, err);
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

	NX_NIC_TRC_FN(adapter, cur_fn, adapter->cmd_buf_arr);

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

	NX_NIC_TRC_FN(adapter, cur_fn, hw->cmdDesc_physAddr);

	adapter->cmdConsumer = (uint32_t *)(((char *) addr) +
			(sizeof(cmdDescType0_t) *
			 adapter->MaxTxDescCount));
	NX_NIC_TRC_FN(adapter, cur_fn, adapter->cmdConsumer);
	/* changed right know */
	adapter->crb_addr_cmd_consumer =
		(((unsigned long)hw->cmdDesc_physAddr) +
		 (sizeof(cmdDescType0_t) * adapter->MaxTxDescCount));

	NX_NIC_TRC_FN(adapter, cur_fn, adapter->crb_addr_cmd_consumer);

	hw->cmdDescHead = (cmdDescType0_t *)addr;
	NX_NIC_TRC_FN(adapter, cur_fn, hw->cmdDescHead);

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
	NX_NIC_TRC_FN(adapter, cur_fn, hw->pauseAddr);
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
		NX_NIC_TRC_FN(adapter, cur_fn, err);
		goto done;
	}

	/*
	 * Need to check how many CPUs are available and use only that
	 * many rings.
	 */
	ctx_id = nx_nic_create_rx_ctx(adapter->netdev);
	if (ctx_id < 0) {
		err = -ENOMEM;
		NX_NIC_TRC_FN(adapter, cur_fn, err);
		goto done;
	}
	adapter->nx_dev->rx_ctxs_state |= (1 << ctx_id);
	/* TODO: Walk thru tear down sequence. */
	if ((err = unm_nic_new_tx_context_prepare(adapter)))
		unm_nic_new_rx_context_destroy(adapter, NX_DESTROY_CTX_RESET);

done:
	if (err) {
		unm_nic_free_ring_context(adapter);
	}
	NX_NIC_TRC_FN(adapter, cur_fn, err);
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
	uint64_t cur_fn = (uint64_t) unm_nic_new_rx_context_prepare;

	//rx_ctx->chaining_allowed = 1;

	NX_NIC_TRC_FN(adapter, cur_fn, 0);
	if (nx_fw_cmd_create_rx_ctx_alloc_dma(adapter->nx_dev, nrds_rings,
				nsds_rings, &hostrq,
				&hostrsp) == NX_RCODE_SUCCESS) {

		rx_ctx->chaining_allowed = rx_chained;
		rx_ctx->multi_context = adapter->multi_ctx;

		retval = nx_fw_cmd_create_rx_ctx(rx_ctx, &hostrq, &hostrsp);
		nx_fw_cmd_create_rx_ctx_free_dma(adapter->nx_dev,
				&hostrq, &hostrsp);
	}

	if (retval != NX_RCODE_SUCCESS) {
		nx_nic_print3(adapter, "Unable to create the "
				"rx context, code %d %s\n",
				retval, nx_errorcode2string(retval));
		NX_NIC_TRC_FN(adapter, cur_fn, retval);
		goto failure_rx_ctx;
	}

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

failure_rx_ctx:
	NX_NIC_TRC_FN(adapter, cur_fn, retval);
	return retval;
}

static int unm_nic_new_rx_context_destroy(struct unm_adapter_s *adapter, int ctx_destroy)
{
	nx_host_rx_ctx_t *rx_ctx;
	int retval = 0;
	int i;
	uint64_t cur_fn = (uint64_t) unm_nic_new_rx_context_destroy;

	NX_NIC_TRC_FN(adapter, cur_fn, 0);

	if (adapter->nx_dev == NULL) {
		nx_nic_print3(adapter, "nx_dev is NULL\n");
		NX_NIC_TRC_FN(adapter, cur_fn, 1);
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
				NX_NIC_TRC_FN(adapter, cur_fn, retval);
			}
		}
	}

	NX_NIC_TRC_FN(adapter, cur_fn, 0);
	return 0;
}

static int unm_nic_new_tx_context_prepare(struct unm_adapter_s *adapter)
{
	nx_host_tx_ctx_t *tx_ctx = NULL;
	int retval = 0;
	int ncds_ring = 1;
	struct nx_dma_alloc_s hostrq;
	struct nx_dma_alloc_s hostrsp;
	uint64_t cur_fn = (uint64_t) unm_nic_new_tx_context_prepare;

	NX_NIC_TRC_FN(adapter, cur_fn, 0);
	retval = nx_fw_cmd_create_tx_ctx_alloc(adapter->nx_dev,
			ncds_ring,
			&tx_ctx);
	if (retval) {
		nx_nic_print4(adapter, "Could not allocate memory "
				"for tx context\n");
		NX_NIC_TRC_FN(adapter, cur_fn, retval);
		goto failure_tx_ctx;
	}
	tx_ctx->msi_index = adapter->msix_entries[adapter->portnum].entry;
	tx_ctx->cmd_cons_dma_addr = adapter->crb_addr_cmd_consumer;
	tx_ctx->dummy_dma_addr = adapter->dummy_dma.phys_addr;

	tx_ctx->cds_ring[0].card_tx_consumer = NULL;
	tx_ctx->cds_ring[0].host_addr = NULL;
	tx_ctx->cds_ring[0].host_phys = adapter->ahw.cmdDesc_physAddr;
	tx_ctx->cds_ring[0].ring_size = adapter->MaxTxDescCount;

	if (nx_fw_cmd_create_tx_ctx_alloc_dma(adapter->nx_dev, ncds_ring,
				&hostrq, &hostrsp) == NX_RCODE_SUCCESS) {

		retval = nx_fw_cmd_create_tx_ctx(tx_ctx, &hostrq, &hostrsp);

		nx_fw_cmd_create_tx_ctx_free_dma(adapter->nx_dev,
				&hostrq, &hostrsp);
	}

	if (retval != NX_RCODE_SUCCESS) {
		nx_nic_print4(adapter, "Unable to create the tx "
				"context, code %d %s\n", retval,
				nx_errorcode2string(retval));
		nx_fw_cmd_create_tx_ctx_free(adapter->nx_dev, tx_ctx);
		NX_NIC_TRC_FN(adapter, cur_fn, retval);
		goto failure_tx_ctx;
	}

	adapter->crb_addr_cmd_producer =
		UNM_NIC_REG(tx_ctx->cds_ring->host_tx_producer - 0x200);

	NX_NIC_TRC_FN(adapter, cur_fn, adapter->crb_addr_cmd_producer);
failure_tx_ctx:
	return retval;
}

static int unm_nic_new_tx_context_destroy(struct unm_adapter_s *adapter, int ctx_destroy)
{
	nx_host_tx_ctx_t *tx_ctx;
	int retval = 0;
	int i;
	uint64_t cur_fn = (uint64_t) unm_nic_new_tx_context_destroy;

	NX_NIC_TRC_FN(adapter, cur_fn, 0);

	if (adapter->nx_dev == NULL) {
		nx_nic_print4(adapter, "nx_dev is NULL\n");
		NX_NIC_TRC_FN(adapter, cur_fn, 1);
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
			NX_NIC_TRC_FN(adapter, cur_fn, retval);
		}

		nx_fw_cmd_create_tx_ctx_free(adapter->nx_dev, tx_ctx);
	}

	NX_NIC_TRC_FN(adapter, cur_fn, 0);
	return 0;
}

void nx_free_tx_resources(struct unm_adapter_s *adapter)
{
	int i, j;
	struct unm_cmd_buffer *cmd_buff;
	struct unm_skb_frag *buffrag;
	uint64_t cur_fn = (uint64_t) nx_free_tx_resources;

	NX_NIC_TRC_FN(adapter, cur_fn, 0);

	cmd_buff = adapter->cmd_buf_arr;
	if (cmd_buff == NULL) {
		nx_nic_print3(adapter, "cmd_buff == NULL\n");
		NX_NIC_TRC_FN(adapter, cur_fn, 0);
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

	for (index = 0; index < adapter->nx_dev->alloc_rx_ctxs; index++) {
		if (adapter->nx_dev->rx_ctxs[index] != NULL) {
			spin_lock(&adapter->ctx_state_lock);
			if((adapter->nx_dev->rx_ctxs_state & (1 << index)) == 0) {
				spin_unlock(&adapter->ctx_state_lock);
				continue;
			}
			spin_unlock(&adapter->ctx_state_lock);
			read_lock(&adapter->adapter_lock);
			unm_nic_disable_all_int(adapter, adapter->nx_dev->rx_ctxs[index]);
			read_unlock(&adapter->adapter_lock);
			nx_synchronize_irq(adapter, adapter->nx_dev->rx_ctxs[index]);
		}
	}

	if (adapter->is_up == ADAPTER_UP_MAGIC || adapter->is_up == FW_RESETTING)
		nx_napi_disable(adapter);

	if(adapter->is_up == ADAPTER_UP_MAGIC)
		FLUSH_SCHEDULED_WORK();
	del_timer_sync(&adapter->watchdog_timer);

	spin_lock_bh(&adapter->tx_lock);
	nx_free_tx_resources(adapter);
	spin_unlock_bh(&adapter->tx_lock);
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
	int ret = 0;
	uint64_t cur_fn = (uint64_t) unm_nic_set_mac;

	NX_NIC_TRC_FN(adapter, cur_fn, 0);
	if (netif_running(netdev)) {
		NX_NIC_TRC_FN(adapter, cur_fn, EBUSY);
		return -EBUSY;
	}

	if (!is_valid_ether_addr(addr->sa_data)) {
		NX_NIC_TRC_FN(adapter, cur_fn, EADDRNOTAVAIL);
		return -EADDRNOTAVAIL;
	}

	memcpy(netdev->dev_addr, addr->sa_data, netdev->addr_len);
	memcpy(adapter->mac_addr, addr->sa_data, netdev->addr_len);

	NX_NIC_TRC_FN(adapter, cur_fn, ret);
	return ret;
}

static int nx_nic_p3_add_mac(struct unm_adapter_s *adapter, __u8 * addr,
			     mac_list_t ** add_list, mac_list_t ** del_list)
{
	mac_list_t *cur, *prev;
	uint64_t cur_fn = (uint64_t) nx_nic_p3_add_mac;

	NX_NIC_TRC_FN(adapter, cur_fn, 0);

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
			NX_NIC_TRC_FN(adapter, cur_fn, 0);
			return 0;
		}
		prev = cur;
		cur = cur->next;
	}

	/* make sure to add each mac address only once */
	for (cur = adapter->mac_list; cur; cur = cur->next) {
		if (memcmp(addr, cur->mac_addr, ETH_ALEN) == 0) {
			NX_NIC_TRC_FN(adapter, cur_fn, 0);
			return 0;
		}
	}
	/* not in del_list, create new entry and add to add_list */
	cur = kmalloc(sizeof(*cur), in_atomic()? GFP_ATOMIC : GFP_KERNEL);
	if (cur == NULL) {
		nx_nic_print3(adapter, "cannot allocate memory. MAC "
			      "filtering may not work properly from now.\n");
		NX_NIC_TRC_FN(adapter, cur_fn, -1);
		return -1;
	}

	memcpy(cur->mac_addr, addr, ETH_ALEN);
	cur->next = *add_list;
	*add_list = cur;
	NX_NIC_TRC_FN(adapter, cur_fn, 0);
	return 0;
}

static void nx_nic_p3_set_multi(struct net_device *netdev)
{
	struct unm_adapter_s *adapter = netdev_priv(netdev);
	mac_list_t *cur, *next, *del_list, *add_list = NULL;
	struct dev_mc_list *mc_ptr;
	__u8 bcast_addr[ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	__uint32_t mode = VPORT_MISS_MODE_DROP;
	uint64_t cur_fn = (uint64_t) nx_nic_p3_set_multi;

	NX_NIC_TRC_FN(adapter, cur_fn, 0);

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
		NX_NIC_TRC_FN(adapter, cur_fn, mode);
		goto send_fw_cmd;
	}

	if ((netdev->flags & IFF_ALLMULTI) ||
	    netdev->mc_count > adapter->max_mc_count) {
		mode = VPORT_MISS_MODE_ACCEPT_MULTI;
		NX_NIC_TRC_FN(adapter, cur_fn, mode);
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

	if (vlan_tx_tag_present(skb)) {
		u16	*ptr;
		ptr = (u16 *)(&hwdesc[saved].mcastAddr) + 3;
		hwdesc[saved].flags |= FLAGS_VLAN_OOB;
		vid = *ptr = vlan_tx_tag_get(skb);
	}

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
			    first_hdr_len - 16);
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
	if (buffrag->dma) {
		pci_unmap_single(pdev, buffrag->dma,
				buffrag->length, PCI_DMA_TODEVICE);
		buffrag->dma = (uint64_t) 0;
	}
	for (k = 1; k < last; k++) {
		buffrag = &pbuf->fragArray[k];
		if (buffrag->dma) {
			pci_unmap_page(pdev, buffrag->dma,
					buffrag->length, PCI_DMA_TODEVICE);
			buffrag->dma = (uint64_t) 0;
		}
	}
}

/* 
	This function will check the validity(Non corruptness) of the packet 
	to be transmitted.

	It will return -1 when we want the packet to be dropped.
	Else will return 0.
*/
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

int unm_nic_xmit_frame(struct sk_buff *skb, struct net_device *netdev)
{
	unsigned int nr_frags = 0;
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
	unsigned int tagged = 0;

	if (unlikely(!adapter->ahw.linkup)) {
		adapter->stats.txdropped++;
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	if (unlikely(skb->len <= 0)) {
		dev_kfree_skb_any(skb);
		adapter->stats.badskblen++;
		return 0;
	}

	nr_frags = skb_shinfo(skb)->nr_frags;
	fragCount = 1 + nr_frags;

	if (!(netdev->features & NETIF_F_HW_VLAN_TX))
		tagged = is_packet_tagged(skb);

	if (fragCount > NX_NIC_MAX_SG_PER_TX ) {

		int i;
		int delta = 0;
		struct skb_frag_struct *frag;

		for (i = 0; i < (fragCount - NX_NIC_MAX_SG_PER_TX);
		     i++) {
			frag = &skb_shinfo(skb)->frags[i];
			delta += frag->size;
		}

		if (!__pskb_pull_tail(skb, delta)) {
			goto drop_packet;
		}

		fragCount = 1 + skb_shinfo(skb)->nr_frags;
	}

#define VC_1ST_BUFFER_18_BYTE_WORKAROUND 1
#define VC_1ST_BUFFER_REQUIRED_LENGTH   18

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
#endif

	firstSegLen = skb->len - skb->data_len;

	/*
	 * Everything is set up. Now, we just need to transmit it out.
	 * Note that we have to copy the contents of buffer over to
	 * right place. Later on, this can be optimized out by de-coupling the
	 * producer index from the buffer index.
	 */
	spin_lock_bh(&adapter->tx_lock);
	if (!netif_carrier_ok(netdev))
		goto drop_packet;	
	producer = adapter->cmdProducer;
	/* There 4 fragments per descriptor */
	no_of_desc = (fragCount + 3) >> 2;

	if (validate_tso(adapter->netdev, skb, tagged)) {
		goto drop_packet;
	}

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

	if (try_map_skb_data(adapter, skb,
				firstSegLen, PCI_DMA_TODEVICE,
				(dma_addr_t *) &buffrag->dma)) {
		buffrag->dma = (uint64_t) 0;
		goto drop_packet;
	}

	pbuf->fragArray[0].length = firstSegLen;
	hwdesc->totalLength = skb->len;
	hwdesc->numOfBuffers = fragCount;
	hwdesc->opcode = TX_ETHER_PKT;

	hwdesc->port = adapter->portnum; /* JAMBU: To be removed */

	hwdesc->ctx_id = adapter->portnum;
	hwdesc->buffer1Length = firstSegLen;
	hwdesc->AddrBuffer1 = pbuf->fragArray[0].dma;

	for (i = 1, k = 1; i < fragCount; i++, k++) {
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

		temp_dma    = page_to_phys(frag->page) + offset;
	
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

	adapter->stats.txbytes += hw->cmdDescHead[saved_producer].totalLength;

	adapter->cmdProducer = producer;
	unm_nic_update_cmd_producer(adapter, producer);

	adapter->stats.xmitcalled++;
	netdev->trans_start = jiffies;

done:
	spin_unlock_bh(&adapter->tx_lock);
	return NETDEV_TX_OK;

requeue_packet:
	netif_stop_queue(netdev);
	spin_unlock_bh(&adapter->tx_lock);
	return NETDEV_TX_BUSY;

drop_packet:
	adapter->stats.txdropped++;
        dev_kfree_skb_any(skb);
	goto done;
}


static void unm_watchdog_fw_reset(unsigned long v)
{
	struct unm_adapter_s *adapter = (struct unm_adapter_s *)v;

	if(!(adapter->removing_module)) {
		SCHEDULE_WORK(&adapter->watchdog_task_fw_reset);
	}
}

static void unm_watchdog(unsigned long v)
{
	struct unm_adapter_s *adapter = (struct unm_adapter_s *)v;
	if(netif_running(adapter->netdev)) {
		SCHEDULE_WORK(&adapter->watchdog_task);
	}
}

static int unm_nic_check_temp(struct unm_adapter_s *adapter)
{
	uint32_t temp, temp_state, temp_val;
	int rv = 0;
	uint64_t cur_fn = (uint64_t) unm_nic_check_temp;

	temp = NXRD32(adapter, CRB_TEMP_STATE);

	temp_state = nx_get_temp_state(temp);
	temp_val = nx_get_temp_val(temp);
	if (adapter->temp == 0) {
		// initialize value to normal, else if already in warn temp range, no msg comes out
		adapter->temp = NX_TEMP_NORMAL;  	
	}

	if(temp_val == 255) {
		return rv;
	}

	if (temp_state == NX_TEMP_PANIC) {
		netdev_list_t *this;
		nx_nic_print1(adapter, "Device temperature %d degrees C "
				"exceeds maximum allowed. Hardware has been "
				"shut down.\n",
				temp_val);

		NX_NIC_TRC_FN(adapter, cur_fn, temp_state);
		for (this = adapter->netlist; this != NULL;
				this = this->next) {
			netif_carrier_off(this->netdev);
			netif_stop_queue(this->netdev);
		}
		rv = 1;
	} else if (temp_state == NX_TEMP_WARN) {
		if (adapter->temp == NX_TEMP_NORMAL) {
			nx_nic_print1(adapter, "Device temperature %d degrees "
					"C.  Dynamic control starting.\n",
					temp_val);
			NX_NIC_TRC_FN(adapter, cur_fn, temp_state);
		}
	} else {
		if (adapter->temp == NX_TEMP_WARN) {
			nx_nic_print1(adapter, "Device temperature is now %d "
					"degrees C in normal range.\n",
					temp_val);
			NX_NIC_TRC_FN(adapter, cur_fn, temp_state);
		}
	}
	adapter->temp = temp_state;
	return rv;
}

int nx_p3_set_vport_miss_mode(struct unm_adapter_s *adapter, int mode)
{
	nic_request_t req;
	uint64_t cur_fn = (uint64_t) nx_p3_set_vport_miss_mode;

	NX_NIC_TRC_FN(adapter, cur_fn, mode);

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

	if (adapter->init_link_update_ctr < NX_WATCHDOG_LINK_THRESHOLD) {
		if (adapter->init_link_update_ctr == 0) {
			netif_carrier_on(adapter->netdev);
			if (netif_queue_stopped(adapter->netdev)) {
				netif_start_queue(adapter->netdev);
			}
			adapter->false_linkup = 1;
		} else if (adapter->init_link_update_ctr == 1) {
				/* enable notifications of link events */
				nic_linkevent_request(adapter, 1, 1);
		}
		adapter->init_link_update_ctr++;
	} else {

		val = NXRD32(adapter, CRB_XG_STATE_P3);
		val1 = XG_LINK_STATE_P3(adapter->ahw.pci_func,val);
		linkupval = XG_LINK_UP_P3;

		netdev = adapter->netdev;

		if ((adapter->ahw.linkup || adapter->false_linkup)
				&& (val1 != linkupval)) {
			nx_nic_print3(adapter, "NIC Link is down\n");
			adapter->ahw.linkup = 0;
			adapter->false_linkup = 0;
			netif_carrier_off(netdev);
			netif_stop_queue(netdev);
			if (adapter->ahw.board_type == UNM_NIC_GBE) {
				unm_nic_set_link_parameters(adapter);
			}
		} else if (!adapter->ahw.linkup && (val1 == linkupval)) {
			nx_nic_print3(adapter, "NIC Link is up\n");
			adapter->ahw.linkup = 1;
			adapter->false_linkup = 0;
			netif_carrier_on(netdev);
			netif_wake_queue(netdev);
			if (adapter->ahw.board_type == UNM_NIC_GBE) {
				unm_nic_set_link_parameters(adapter);
			}
		}
	}
}

int nx_reset_netq_rx_queues( struct net_device *netdev) 
{
	struct unm_adapter_s *adapter;
	adapter = netdev_priv(netdev);
	if(adapter->multi_ctx)
		vmknetddi_queueops_invalidate_state(netdev);
	return 0;
}

int netxen_nic_attach_all_ports(struct unm_adapter_s *adapter)
{
	struct pci_dev *dev;
	int prev_lro_state;
	int rv = 0;
	int bus_id = adapter->pdev->bus->number;

	list_for_each_entry(dev, &pci_devices, global_list) {
		if(dev->bus->number != bus_id)
			continue;

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
	int bus_id = adapter->pdev->bus->number;

	list_for_each_entry(dev, &pci_devices, global_list) {
		if(dev->bus->number != bus_id)
			continue;

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
	int bus_id = adapter->pdev->bus->number;

	list_for_each_entry(dev, &pci_devices, global_list) {
		if(dev->bus->number != bus_id)
			continue;

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
	int bus_id = adapter->pdev->bus->number;

	list_for_each_entry(dev, &pci_devices, global_list) {
		if(dev->bus->number != bus_id)
			continue;

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

void nx_nic_halt_firmware(struct unm_adapter_s *adapter)
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
	if(!adapter) {
		goto out2;
	}
	if(adapter->removing_module) {
		goto out2;
	}

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
	if(fw_reset == 1) {
		if (!try_module_get(THIS_MODULE)) {
			goto out;
		}
#ifdef COLLECT_FW_LOGS
		nx_nic_collect_fw_logs(adapter);
#endif
		if(adapter->mdump.disable_overwrite == 0) {
			nx_nic_minidump(adapter);
		}
		adapter->mdump.disable_overwrite = 0;

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
		if (adapter->fw_alive_failures > 1) {
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

			if((failure_type & 0x1fffff00) == 0x00006700) {
				nx_nic_print3(adapter,"Firmware aborted with error code 0x00006700."
					" Device is being reset.\n");
			}


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
			} else {
#ifdef COLLECT_FW_LOGS 
				nx_nic_collect_fw_logs(adapter);
#endif
				if(adapter->mdump.disable_overwrite == 0) {
					nx_nic_minidump(adapter);
				}
				adapter->mdump.disable_overwrite = 0;
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
	mod_timer(&adapter->watchdog_timer_fw_reset, jiffies + 2 * HZ);
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

	/* Handle link events from phy until vmkernel is done with getting info */
	if ( (!nx_nic_get_linkevent_cap(adapter)) ||
			(adapter->init_link_update_ctr < NX_WATCHDOG_LINK_THRESHOLD)) {
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
 * This is for Firmware version 4.0 and greater.
 *
 * Returns 0 on success, negative on failure
 */
static int nx_nic_fw40_change_mtu(struct net_device *netdev, int new_mtu)
{
	struct unm_adapter_s *adapter = netdev_priv(netdev);
	uint32_t max_mtu;
	int i;
	uint64_t cur_fn = (uint64_t) nx_nic_fw40_change_mtu;

	NX_NIC_TRC_FN(adapter, cur_fn, new_mtu);

	max_mtu = P3_MAX_MTU;

	nx_nic_print6(adapter, "Max supported MTU is %u\n", max_mtu);

	if (new_mtu < 0 || new_mtu > max_mtu) {
		nx_nic_print3(adapter, "MTU[%d] > %d is not supported\n",
				new_mtu, max_mtu);
		NX_NIC_TRC_FN(adapter, cur_fn, max_mtu);
		NX_NIC_TRC_FN(adapter, cur_fn, EINVAL);
		return -EINVAL;
	}

	for (i = 0; i < adapter->nx_dev->alloc_rx_ctxs; i++) {
		if ((adapter->nx_dev->rx_ctxs[i] != NULL) && 
				((adapter->nx_dev->rx_ctxs_state & (1 << i)) != 0)) {
			if (nx_fw_cmd_set_mtu(adapter->nx_dev->rx_ctxs[i],
						adapter->ahw.pci_func,
						new_mtu)) {

				NX_NIC_TRC_FN(adapter, cur_fn, EIO);
				return -EIO;
			}
		}
	}

	netdev->mtu = new_mtu;
	adapter->mtu = new_mtu;

	NX_NIC_TRC_FN(adapter, cur_fn, 0);
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
	}

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
	uint64_t cur_fn = (uint64_t) unm_nic_disable_all_int;

	NX_NIC_TRC_FN(adapter, cur_fn, 0);

	if (nxhal_host_rx_ctx == NULL ||
	    nxhal_host_rx_ctx->sds_rings == NULL) {
		NX_NIC_TRC_FN(adapter, cur_fn, nxhal_host_rx_ctx);
		return;
	}

	for (i = 0; i < nxhal_host_rx_ctx->num_sds_rings; i++) {
		unm_nic_disable_int(adapter, &nxhal_host_rx_ctx->sds_rings[i]);
	}
	NX_NIC_TRC_FN(adapter, cur_fn, 0);
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
	uint64_t cur_fn = (uint64_t) unm_nic_enable_all_int;

	NX_NIC_TRC_FN(adapter, cur_fn, 0);

	for (i = 0; i < nxhal_host_rx_ctx->num_sds_rings; i++) {
		nxhal_sds_ring = &nxhal_host_rx_ctx->sds_rings[i];
		host_sds_ring = (sds_host_ring_t *)nxhal_sds_ring->os_data;
		unm_nic_enable_int(adapter, nxhal_sds_ring);
	}

}

#define find_diff_among(a,b,range) ((a)<=(b)?((b)-(a)):((b)+(range)-(a)))

static void inline nx_nic_handle_interrupt(struct unm_adapter_s *adapter,
		nx_host_sds_ring_t      *nxhal_sds_ring)
{
	sds_host_ring_t         *host_sds_ring  =
		(sds_host_ring_t *)nxhal_sds_ring->os_data;

	NX_PREP_AND_SCHED_RX(host_sds_ring->netdev, host_sds_ring);
}

/**
 * nx_nic_legacy_intr - Legacy Interrupt Handler
 * @irq: interrupt number
 * data points to status ring
 **/
#if !defined(__VMKLNX__) &&  LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
irqreturn_t nx_nic_legacy_intr(int irq, void *data, struct pt_regs * regs)
#else
irqreturn_t nx_nic_legacy_intr(int irq, void *data)
#endif
{
	nx_host_sds_ring_t      *nxhal_sds_ring = (nx_host_sds_ring_t *)data;
        sds_host_ring_t         *host_sds_ring  =
				     (sds_host_ring_t *)nxhal_sds_ring->os_data;
	struct unm_adapter_s *adapter = host_sds_ring->adapter;
	uint64_t cur_fn = (uint64_t) nx_nic_legacy_intr;

	if (unlikely(!irq)) {
		NX_NIC_TRC_FN(adapter, cur_fn, 0);
		return IRQ_NONE;	/* Not our interrupt */
	}

	if (nx_nic_clear_legacy_int(adapter, nxhal_sds_ring) == -1) {
		return IRQ_HANDLED;
	}

	/* process our status queue */
	if (!netif_running(adapter->netdev)) {
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
#if !defined(__VMKLNX__) &&  LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
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
	if (!netif_running(adapter->netdev)) {
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
#if !defined(__VMKLNX__) &&  LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
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
	uint64_t cur_fn = (uint64_t) nx_nic_msix_intr;

	if (unlikely(!irq)) {
		NX_NIC_TRC_FN(adapter, cur_fn, 0);
		return IRQ_NONE;	/* Not our interrupt */
	}

	/* process our status queue */
	if (!netif_running(adapter->netdev)) {
		goto done;
	}

	nx_nic_print7(adapter, "Entered handle ISR\n");
	host_sds_ring->ints++;
	adapter->stats.ints++;

	ctx_id = host_sds_ring->ring->parent_ctx->this_id;
	if(ctx_id && !(adapter->nx_dev->rx_ctxs_state & (1 << ctx_id))) {
		NX_NIC_TRC_FN(adapter, cur_fn, ctx_id);
		goto done;
	}

	nx_nic_handle_interrupt(adapter, nxhal_sds_ring);

done:
	return IRQ_HANDLED;
}


/*
 * Per status ring processing.
 */
static int nx_nic_poll_sts(struct napi_struct *napi, int work_to_do)
{
	sds_host_ring_t *host_sds_ring  = container_of(napi, sds_host_ring_t, napi);
	nx_host_sds_ring_t *nxhal_sds_ring = (nx_host_sds_ring_t *)host_sds_ring->ring;
	struct unm_adapter_s    *adapter = host_sds_ring->adapter;
	int done = 1;
	int work_done;
	U32 ctx_id;

    if((nxhal_sds_ring == NULL) || (adapter == NULL)) {
        return 0;
    }

	ctx_id = host_sds_ring->ring->parent_ctx->this_id;

	if(!netif_running(adapter->netdev) || 
		(ctx_id && !(adapter->nx_dev->rx_ctxs_state & (1 << ctx_id)))){
		NETIF_RX_COMPLETE((host_sds_ring->netdev), napi);
		return 0;
	}

	work_done = unm_process_rcv_ring(adapter, nxhal_sds_ring, work_to_do);

	if (work_done >= work_to_do) 
		done = 0;

	/*
	 * Only one cpu must be processing Tx command ring.
	 */
	if(spin_trylock(&adapter->tx_cpu_excl)) {

		if (unm_process_cmd_ring((unsigned long)adapter) == 0) {
			done = 0;
		}
		spin_unlock(&adapter->tx_cpu_excl);
	}

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

	return work_done;
}

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
	uint64_t cur_fn = (uint64_t) process_rxbuffer;

	nxhal_rds_ring = &nxhal_rx_ctx->rds_rings[desc_ctx];
	host_rds_ring = (rds_host_ring_t *) nxhal_rds_ring->os_data;
	buffer = &host_rds_ring->rx_buf_arr[index];

	pci_unmap_single(pdev, (buffer)->dma, host_rds_ring->dma_size,
			 PCI_DMA_FROMDEVICE);
	buffer->dma = (uint64_t) 0;

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
		NX_NIC_TRC_FN(adapter, cur_fn, 0);
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

	if (likely((adapter->rx_csum) && (csum_status == STATUS_CKSUM_OK))) {
		adapter->stats.csummed++;
		skb->ip_summed = CHECKSUM_UNNECESSARY;
	} else {
		skb->ip_summed = CHECKSUM_NONE;
	}
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
	uint64_t cur_fn = (uint64_t) unm_process_rcv;

	desc_ctx = desc->type;
	//nx_nic_print3(adapter, "type == %d\n", desc_ctx);
	if (unlikely(desc_ctx >= adapter->max_rds_rings)) {
                nx_nic_print3(adapter, "Bad Rcv descriptor ring\n");
		NX_NIC_TRC_FN(adapter, cur_fn, desc_ctx);
		return;
	}

	nxhal_host_rds_ring = &nxhal_rx_ctx->rds_rings[desc_ctx];
	host_rds_ring = (rds_host_ring_t *) nxhal_host_rds_ring->os_data;
	if (unlikely(index > nxhal_host_rds_ring->ring_size)) {
                nx_nic_print3(adapter, "Got a buffer index:%x for %s desc "
			      "type. Max is %x\n",
			      index, RCV_DESC_TYPE_NAME(desc_ctx),
			      nxhal_host_rds_ring->ring_size);
		NX_NIC_TRC_FN(adapter, cur_fn, index);
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
		NX_NIC_TRC_FN(adapter, cur_fn, 0);
		BUG();
		return;
	}

	skb_put(skb, (length < nxhal_host_rds_ring-> buff_size) ? 
			length : nxhal_host_rds_ring->buff_size);
	skb_pull(skb, desc->pkt_offset);

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
		NX_NIC_TRC_FN(adapter, cur_fn, 0);
		goto packet_done_no_stats;
	}

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

	skb_pull(skb, ETH_HLEN);
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

	skb_push(skb, ETH_HLEN);
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
	uint64_t cur_fn = (uint64_t) unm_process_lro_contiguous;

	host_sds_ring = (sds_host_ring_t *)nxhal_sds_ring->os_data;

	desc_ctx = desc->lro2.type;
	if (unlikely(desc_ctx != 1)) {
		nx_nic_print3(adapter, "Bad Rcv descriptor ring\n");
		NX_NIC_TRC_FN(adapter, cur_fn, desc_ctx);
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
			NX_NIC_TRC_FN(adapter, cur_fn, ref_handle);
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
		NX_NIC_TRC_FN(adapter, cur_fn, 0);
		BUG();
		return;
	}

	data_offset = (desc->lro2.l4_hdr_offset +
		       (desc->lro2.timestamp ?
			TCP_TS_HDR_SIZE : TCP_HDR_SIZE));

	skb_put(skb, data_offset + length);

	/*
	 * This adjustment seems to keep the stack happy in cases where some
	 * sk's sometimes seem to get stuck in a zero-window condition even
	 * though their receive queues may be of zero length.
	 */
	skb->truesize = (skb->len + sizeof(struct sk_buff) +
			 ((unsigned long)skb->data -
			  (unsigned long)skb->head));

	skb_pull(skb, desc->lro2.l2_hdr_offset);
	check_8021q(netdev, skb, &vid);
	skb->protocol = eth_type_trans(skb, netdev);

	lro2_adjust_skb(skb, desc);

	NX_ADJUST_SMALL_PKT_LEN(skb);

	length = skb->len;

	if (!netif_running(netdev)) {
		kfree_skb(skb);
		NX_NIC_TRC_FN(adapter, cur_fn, 0);
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
	head_skb->truesize += data_length;

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
	int skb_alloc_failed = 0;

	host_rds_ring = (rds_host_ring_t *) nxhal_rds_ring->os_data;
	producer = host_rds_ring->producer;
	/* We can start writing rx descriptors into the phantom memory. */
	list_for_each_entry_safe(buffer , tmp_buffer, &free_list->head, link) {

		if (buffer->skb == NULL) {
			/*If nx_alloc_rx_skb fails's, it should not modify buffer */
			rv = nx_alloc_rx_skb(adapter, nxhal_rds_ring, buffer);
			if (rv) {
				skb_alloc_failed = 1;
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
	return skb_alloc_failed;
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
			break;
		}

		if (desc->opcode == adapter->msg_type_rx_desc) {
			unm_process_rcv(adapter, nxhal_sds_ring, desc);
		} else if (desc->opcode == UNM_MSGTYPE_NIC_LRO_CONTIGUOUS) {
			unm_process_lro_contiguous(adapter, nxhal_sds_ring,
					desc);
		} else if (desc->opcode == UNM_MSGTYPE_NIC_RESPONSE) {
			unm_msg_t msg;
			int       cnt 	= desc->descCnt;
			int       index = 1;
			int       i     = 0;

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

			nx_nic_handle_fw_response((nx_dev_handle_t)adapter,
					(nic_response_t *) &msg.body);
			
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
 * Checks if the specified device is a NetXen device.
 */
int nx_nic_is_netxen_device(struct net_device *netdev)
{
	return (netdev->do_ioctl == unm_nic_ioctl);
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

	return ((int)adapter->portnum);
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
	unm_adapter *adapter = NULL;
	uint64_t cur_fn = (uint64_t) nx_nic_send_cmd_descs;

	if (dev == NULL) {
                nx_nic_print4(NULL, "%s: Device is NULL\n", __FUNCTION__);
		return (-1);
	}

	adapter = (unm_adapter *) netdev_priv(dev);

	NX_NIC_TRC_FN(adapter, cur_fn, 0);

#if defined(DEBUG)
        if (adapter == NULL) {
                nx_nic_print4(adapter, "%s Adapter not initialized, cannot "
			      "send request to card\n", __FUNCTION__);
                return (-1);
        }
#endif /* DEBUG */
	if (adapter->cmd_buf_arr == NULL ) {
		nx_nic_print4(adapter, "Command Ring not Allocated");
		NX_NIC_TRC_FN(adapter, cur_fn, -1);
		return -1;
	}

	i = 0;

	spin_lock_bh(&adapter->tx_lock);

	/* check if space is available */
	if ((adapter->cmdProducer + nr_elements) >=
	     ((adapter->lastCmdConsumer <= adapter->cmdProducer) ?
	      adapter->lastCmdConsumer + adapter->MaxTxDescCount :
	      adapter->lastCmdConsumer)) {

		netif_stop_queue(adapter->netdev);
		spin_unlock_bh(&adapter->tx_lock);
		NX_NIC_TRC_FN(adapter, cur_fn, EBUSY);
		return -EBUSY;
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

	NX_NIC_TRC_FN(adapter, cur_fn, 0);
	return (0);
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
	uint64_t cur_fn = (uint64_t) unm_process_cmd_ring;

	lastConsumer = adapter->lastCmdConsumer;
	consumer = *(adapter->cmdConsumer);
	while (lastConsumer != consumer) {
		buffer = &adapter->cmd_buf_arr[lastConsumer];
		pdev = adapter->pdev;
		frag = &buffer->fragArray[0];
		skb = buffer->skb;
		if (skb && (cmpxchg(&buffer->skb, skb, 0) == skb)) {
			uint32_t i;

			pci_unmap_single(pdev, frag->dma, frag->length,
					PCI_DMA_TODEVICE);
			frag->dma = (uint64_t) 0;
			for (i = 1; i < buffer->fragCount; i++) {
				nx_nic_print7(adapter, "get fragment no %d\n",
						i);
				frag++;	/* Get the next frag */
				pci_unmap_page(pdev, frag->dma, frag->length,
						PCI_DMA_TODEVICE);
				frag->dma = (uint64_t) 0;
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
				NX_NIC_TRC_FN(adapter, cur_fn, 0);
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

	/*
	 * Adjust the truesize by the delta. We allocate more in some cases
	 * like Jumbo.
	 */
	skb->truesize -= host_rds_ring->truesize_delta;

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
	uint64_t cur_fn = (uint64_t) unm_post_rx_buffers;

	NX_NIC_TRC_FN(adapter, cur_fn, ringid);

	INIT_LIST_HEAD(&free_list.head);
	nxhal_rds_ring = &nxhal_rx_ctx->rds_rings[ringid];
	host_rds_ring = (rds_host_ring_t *) nxhal_rds_ring->os_data;

	spin_lock_bh(&adapter->buf_post_lock);
	if (host_rds_ring->posting
	    || list_empty(&host_rds_ring->free_rxbufs.head)) {
		spin_unlock_bh(&adapter->buf_post_lock);
		NX_NIC_TRC_FN(adapter, cur_fn, 0);
		return (0);
	}

	list_replace_init(&host_rds_ring->free_rxbufs.head, &free_list.head);
	free_list.count = host_rds_ring->free_rxbufs.count;
	host_rds_ring->free_rxbufs.count = 0;

	host_rds_ring->posting = 1;
	spin_unlock_bh(&adapter->buf_post_lock);

	NX_NIC_TRC_FN(adapter, cur_fn, nx_post_rx_descriptors);
	return (nx_post_rx_descriptors(adapter, nxhal_rds_ring, ringid,
				       &free_list));
}

#if	(!defined(ESX) && defined(CONFIG_PM))
static inline void nx_pci_save_state(struct pci_dev *pdev,
				     struct unm_adapter_s *adapter)
{
	pci_save_state(pdev);
}

static inline void nx_pci_restore_state(struct pci_dev *pdev,
					struct unm_adapter_s *adapter)
{
	pci_restore_state(pdev);
}

static inline int nx_pci_choose_state(struct pci_dev *pdev, PM_MESSAGE_T state)
{
	return (pci_choose_state(pdev, state));
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

	err = unm_init_proc_drv_dir();
	if (err) {
		return err;
	}
	if ((unm_workq = create_singlethread_workqueue("unm")) == NULL) {
		unm_cleanup_proc_drv_entries();
		return (-ENOMEM);
	}

	nx_verify_module_params();

	err = PCI_MODULE_INIT(&unm_driver);
	
	if (err) {

		unm_cleanup_proc_drv_entries();
		destroy_workqueue(unm_workq);
		return err;
	}
	return err;
}

static void __exit unm_exit_module(void)
{
	/*
	 * Wait for some time to allow the dma to drain, if any.
	 */
	mdelay(5);

	pci_unregister_driver(&unm_driver);

	unm_cleanup_proc_drv_entries();
	destroy_workqueue(unm_workq);
}

static void nx_nic_get_nic_version_number(struct net_device *netdev,
					  nic_version_t *version)
{
	struct unm_adapter_s *adapter = (struct unm_adapter_s *)netdev_priv(netdev);

        version->major = NXRD32(adapter, UNM_FW_VERSION_MAJOR);

        version->minor = NXRD32(adapter, UNM_FW_VERSION_MINOR);

        version->sub = NXRD32(adapter, UNM_FW_VERSION_SUB);

}

int nx_nic_get_linkevent_cap(struct unm_adapter_s *adapter)
{
	return (adapter->ahw.fw_capabilities_1 & 
			NX_FW_CAPABILITY_LINK_NOTIFICATION);
}


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
	uint64_t cur_fn = (uint64_t) nic_linkevent_request;

	NX_NIC_TRC_FN(adapter, cur_fn, NX_NIC_H2C_OPCODE_GET_LINKEVENT);

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
	uint64_t cur_fn = (uint64_t) nic_handle_linkevent_response;

	NX_NIC_TRC_FN(adapter, cur_fn, 0);
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

	NX_NIC_TRC_FN(adapter, cur_fn, linkevent->module);

	/* f/w cannot send anymore events */
	if ( ! nx_nic_get_linkevent_cap(adapter)) {
		NX_NIC_TRC_FN(adapter, cur_fn, 0);
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

	if ((adapter->ahw.linkup != linkevent->link_status) ||
			(adapter->false_linkup)) {
		adapter->ahw.linkup = linkevent->link_status;
		adapter->false_linkup = 0;
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
	NX_NIC_TRC_FN(adapter, cur_fn, 0);
}


module_init(unm_init_module);
module_exit(unm_exit_module);
