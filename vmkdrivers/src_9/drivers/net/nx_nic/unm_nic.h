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
#ifndef _UNM_NIC_
#define _UNM_NIC_

#include <linux/module.h>
#include <linux/version.h>
#include<linux/netdevice.h>

#if defined(CONFIG_MODVERSIONS) && !defined(MODVERSIONS)
#define MODVERSIONS
#endif

#include <linux/skbuff.h>
#ifdef NETIF_F_HW_VLAN_TX
#include <linux/if_vlan.h>
#endif
#ifndef MODULE
#include <asm/i387.h>
#endif
#include <linux/pci.h>
#include "unm_nic_hw.h"
#include "nic_cmn.h"
#include "unm_inc.h"
#include "nx_hash_table.h"
#include "nx_mem_pool.h"
#include "nxhal_nic_api.h"
#include "unm_brdcfg.h"

#include "nx_nic_vmk.h"		// vmkernel specific defines
#include "nx_nic_minidump.h"

#include <linux/ethtool.h>

#define	NX_NIC_MAX_SG_PER_TX	(MAX_BUFFERS_PER_CMD - 2)

#define NX_FW_VERSION(a, b, c)    (((a) << 16) + ((b) << 8) + (c))

#define NX_NIC_VERSION_CODE(a, b, c)    (((a) << 24) + ((b) << 16) + (c))

#define HDR_CP 64

#ifndef in_atomic
#define in_atomic()     (1)
#endif

#ifndef IRQF_SAMPLE_RANDOM
#define IRQF_SAMPLE_RANDOM SA_SAMPLE_RANDOM
#endif

#ifndef IRQF_SHARED
#define IRQF_SHARED SA_SHIRQ 
#endif

#if !defined(__VMKLNX__) && LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
typedef irqreturn_t (*NX_IRQ_HANDLER)(int , void *, struct pt_regs *);
#else
typedef irqreturn_t (*NX_IRQ_HANDLER)(int , void *);
#endif

#define	__NETIF_RX(_SKB)	netif_receive_skb(_SKB);

#define TX_LOCK(__lock, flags) spin_lock_bh((__lock))
#define TX_UNLOCK(__lock, flags) spin_unlock_bh((__lock))

#define GET_SDS_NETDEV_IRQ(HOST_SDS_RING) \
	(HOST_SDS_RING)->netdev_irq

#define GET_SDS_NETDEV_NAME(HOST_SDS_RING) \
	(HOST_SDS_RING)->netdev_name 

#define SET_SDS_NETDEV_IRQ(HOST_SDS_RING, IRQ) \
	do {                                    \
		(HOST_SDS_RING)->netdev_irq = (IRQ);    \
	}while (0)

#define SET_SDS_NETDEV_NAME(HOST_SDS_RING, NAME, CTX_ID, NUM, ...) \
	do {    \
		if ((CTX_ID)) { \
			sprintf((HOST_SDS_RING)->netdev_name, \
					"%s:%d:%d", (NAME), \
					(CTX_ID), (NUM), ##__VA_ARGS__); \
		} else if ((NUM)){ \
			sprintf((HOST_SDS_RING)->netdev_name, \
					"%s:%d", (NAME), (NUM), ##__VA_ARGS__); \
		} else { \
			sprintf((HOST_SDS_RING)->netdev_name, \
					"%s", (NAME), ##__VA_ARGS__); \
		}\
	}       while(0)

#define NX_SDS_NAPI_ENABLE(HOST_SDS_RING) \
	napi_enable(&(HOST_SDS_RING)->napi)

#define NX_SDS_NAPI_DISABLE(HOST_SDS_RING) \
	napi_disable(&(HOST_SDS_RING)->napi)

#define NX_PREP_AND_SCHED_RX(NETDEV, SDS_RING)                              \
	do {                                                                \
		if (netif_rx_schedule_prep((NETDEV), &((SDS_RING)->napi))) {\
			__netif_rx_schedule(NETDEV, &((SDS_RING)->napi));   \
		}                                                           \
	}while (0)

#define	NETIF_RX_COMPLETE(_NETDEV, _NAPI) netif_rx_complete(_NETDEV, _NAPI)

#define NX_INVALID_SPEED_CFG 0xFFFF 
#define NX_INVALID_DUPLEX_CFG 0xFFFF 
#define NX_INVALID_AUTONEG_CFG 0xFFFF 

#define NX_PCI_SUBSYSTEM_VENDOR_ID 0x2c
#define NX_HP_VENDOR_ID 0x103c

/* XXX try to reduce this so struct sk_buff + data fits into
 * 2k pool
 */
#define NX_CT_DEFAULT_RX_BUF_LEN	2048

#define NX_MIN_DRIVER_RDS_SIZE		64

#define NX_DEFAULT_RDS_SIZE		1024
#define NX_DEFAULT_RDS_SIZE_1G		512
#define NX_DEFAULT_JUMBO_RDS_SIZE	128
#define NX_DEFAULT_JUMBO_RDS_SIZE_1G	128
#define MAX_LRO_RCV_DESCRIPTORS		32 /*  XXX */
#define RDS_MAX_FW230			1024
#define RDS_JUMBO_MAX_FW230		256
#define MAX_SUPPORTED_FILTERS		512

#define RDS_LIMIT_FW230(size) (((size) > RDS_MAX_FW230) ? \
				RDS_MAX_FW230 : (size))
#define RDS_JUMBO_LIMIT_FW230(size) (((size) > RDS_JUMBO_MAX_FW230) ? \
					RDS_JUMBO_MAX_FW230 : (size))

#define NX_SSID_NC375T 0x1740103c

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24))
#define netdev_priv(_DEV)       (_DEV)->priv
#endif

#define BAR_0            0
#define PCI_DMA_64BIT    0xffffffffffffffffULL
#define PCI_DMA_32BIT    0x00000000ffffffffULL

#define ADDR_IN_WINDOW0(off)    \
       ((off >= UNM_CRB_PCIX_HOST) && (off < UNM_CRB_PCIX_HOST2)) ? 1 : 0
#define ADDR_IN_WINDOW1(off)   \
       ((off > UNM_CRB_PCIX_HOST2) && (off < UNM_CRB_MAX)) ? 1 : 0

/*Do not use CRB_NORMALIZE if this is true*/
#define ADDR_IN_BOTH_WINDOWS(off)\
       ((off >= UNM_CRB_PCIX_HOST) && (off < UNM_CRB_DDR_NET)) ? 1 : 0

#if defined(CONFIG_X86_64) || defined(CONFIG_64BIT) || (CPU == x86-64)
typedef unsigned long uptr_t;
#else
typedef unsigned uptr_t;
#endif

#define FIRST_PAGE_GROUP_START	0
#define FIRST_PAGE_GROUP_END	0x100000

#define SECOND_PAGE_GROUP_START	0x6000000
#define SECOND_PAGE_GROUP_END	0x68BC000

#define THIRD_PAGE_GROUP_START	0x70E4000
#define THIRD_PAGE_GROUP_END	0x8000000

#define FIRST_PAGE_GROUP_SIZE	FIRST_PAGE_GROUP_END - FIRST_PAGE_GROUP_START
#define SECOND_PAGE_GROUP_SIZE	SECOND_PAGE_GROUP_END - SECOND_PAGE_GROUP_START
#define THIRD_PAGE_GROUP_SIZE	THIRD_PAGE_GROUP_END - THIRD_PAGE_GROUP_START

/* normalize a 64MB crb address to 32MB PCI window
 * To use CRB_NORMALIZE, window _must_ be set to 1
 */
#define CRB_NORMAL(reg)	\
	(reg) - UNM_CRB_PCIX_HOST2 + UNM_CRB_PCIX_HOST
#define CRB_NORMALIZE(adapter, reg) \
	(void *)(uptr_t)(pci_base_offset(adapter, CRB_NORMAL(reg)))

#define DB_NORMALIZE(adapter, off) \
        (void *)(uptr_t)(adapter->ahw.db_base + (off))

typedef struct {
        __uint32_t	major;
        __uint32_t	minor;
        __uint32_t	sub;
	__uint32_t	build;
} nic_version_t;

static const nic_version_t NX_SUPPORTED_FW_VERSIONS[] = {
  {.major = 3, .minor = 4},
  {.major = 4, .minor = 0},
  
};

#define NX_MAX_RDS_SZ_FW40	(8 * 1024)
#define NX_MAX_RDS_SZ_FW34	(32 * 1024)
#define NX_MAX_CMD_DESCRIPTORS	(1024)
#define NX_MAX_SUPPORTED_RDS_SZ	(NX_MAX_RDS_SZ_FW40)
#define NX_MAX_JUMBO_RDS_SIZE        (1024)

/* Phantom is multi-function.  For our purposes,  use function 0 */
//#define PHAN_FUNC_NUM         0

#define UNM_WRITE_LOCK(lock) \
        do { \
                write_lock((lock)); \
        } while (0)

#define UNM_WRITE_UNLOCK(lock) \
        do { \
                write_unlock((lock)); \
        } while (0)

#define UNM_READ_LOCK(lock) \
        do { \
                read_lock((lock)); \
        } while (0)

#define UNM_READ_UNLOCK(lock) \
        do { \
                read_unlock((lock)); \
        } while (0)

#define UNM_WRITE_LOCK_IRQS(lock, flags)\
        do { \
                write_lock_irqsave((lock), flags); \
        } while (0)

#define UNM_WRITE_UNLOCK_IRQR(lock, flags)\
        do { \
                write_unlock_irqrestore((lock), flags); \
        } while (0)



extern char	unm_nic_driver_name[];
extern uint8_t	nx_nic_msglvl;
extern char	*nx_nic_kern_msgs[];
#define	NX_NIC_EMERG	0
#define	NX_NIC_ALERT	1
#define	NX_NIC_CRIT	2
#define	NX_NIC_ERR	3
#define	NX_NIC_WARNING	4
#define	NX_NIC_NOTICE	5
#define	NX_NIC_INFO	6
#define	NX_NIC_DEBUG	7

#define	nx_nic_print(LVL, ADAPTER, FORMAT, ...)				\
	do {								\
		if (ADAPTER) {						\
			struct unm_adapter_s *TMP_ADAP;			\
			TMP_ADAP = (struct unm_adapter_s *)ADAPTER;	\
			if (LVL <= TMP_ADAP->msglvl) {			\
				printk("%s%s[%s]: " FORMAT,		\
				       nx_nic_kern_msgs[LVL],		\
				       unm_nic_driver_name,		\
				       ((TMP_ADAP->netdev &&		\
					 TMP_ADAP->netdev->name) ?	\
					TMP_ADAP->netdev->name : "eth?"), \
				       ##__VA_ARGS__);			\
			}						\
		} else if (LVL <= nx_nic_msglvl) {			\
			printk("%s%s: " FORMAT, nx_nic_kern_msgs[LVL],	\
			       unm_nic_driver_name, ##__VA_ARGS__);	\
		}							\
	} while (0)

#define	NX_NIC_DEBUG_LVL	6
#if	NX_NIC_DEBUG_LVL >= 7
#define	nx_nic_print7(ADAPTER, FORMAT, ...)		\
        nx_nic_print(7, ADAPTER, FORMAT, ##__VA_ARGS__)
#else
#define	nx_nic_print7(ADAPTER, FORMAT, ...)
#endif
#if	NX_NIC_DEBUG_LVL >= 6
#define	nx_nic_print6(ADAPTER, FORMAT, ...)		\
        nx_nic_print(6, ADAPTER, FORMAT, ##__VA_ARGS__)
#else
#define	nx_nic_print6(ADAPTER, FORMAT, ...)
#endif
#define	nx_nic_print5(ADAPTER, FORMAT, ...)		\
        nx_nic_print(5, ADAPTER, FORMAT, ##__VA_ARGS__)
#define	nx_nic_print4(ADAPTER, FORMAT, ...)		\
        nx_nic_print(4, ADAPTER, FORMAT, ##__VA_ARGS__)
#define	nx_nic_print3(ADAPTER, FORMAT, ...)		\
        nx_nic_print(3, ADAPTER, FORMAT, ##__VA_ARGS__)
#define	nx_nic_print2(ADAPTER, FORMAT, ...)		\
        nx_nic_print(2, ADAPTER, FORMAT, ##__VA_ARGS__)
#define	nx_nic_print1(ADAPTER, FORMAT, ...)		\
        nx_nic_print(1, ADAPTER, FORMAT, ##__VA_ARGS__)
#define	nx_nic_print0(ADAPTER, FORMAT, ...)		\
        nx_nic_print(0, ADAPTER, FORMAT, ##__VA_ARGS__)



#define	nx_print(LEVEL, FORMAT, ...)					\
        printk(LEVEL "%s: " FORMAT, unm_nic_driver_name, ##__VA_ARGS__)

#if	1			/* defined(NDEBUG) */
#define	nx_print1(LEVEL, FORMAT, ...)					\
        printk(LEVEL "%s: " FORMAT, unm_nic_driver_name, ##__VA_ARGS__)
#else
#define	nx_print1(format, ...)
#endif

/* Note: Make sure to not call this before adapter->port is valid */
#define DPRINTK(nlevel, klevel, fmt, args...)  do { \
    } while (0)


#define UNM_MAX_INTR        10

#define UNM_RXBUFFER        2048

/* Number of status descriptors to handle per interrupt */
#define MAX_STATUS_HANDLE  (256)

/* Number of status descriptors to process before posting buffers */
#define MAX_RX_THRESHOLD  (MAX_STATUS_HANDLE*2)

/* netif_wake_queue ?  count*/
#define UNM_TX_QUEUE_WAKE      16

/* After UNM_RX_BUFFER_WRITE have been processed, we will repopulate them */
#define UNM_RX_BUFFER_WRITE    64

/* only works for sizes that are powers of 2 */
#define UNM_ROUNDUP(i, size) ((i) = (((i) + (size) - 1) & ~((size) - 1)))

/*
 * If there are too many allocation failures and there are only 4 more skbs
 * left then don't send the packet up the stack but just repost it to the
 * firmware
 */
#define NX_NIC_RX_POST_THRES    10

/* For the MAC Address:experimental ref. pegnet_types.h */
#define UNM_MAC_OUI          "00:0e:1e:"
#define UNM_MAC_EXPERIMENTAL ((unsigned char)0x20)

#define MAX_VMK_BOUNCE 256
#define MAX_PAGES_PER_FRAG    24

enum {
	LOAD_FW_FROM_FLASH = 0,
	LOAD_FW_WITH_COMPARISON = 1,
	LOAD_FW_NO_COMPARISON = 2,
	LOAD_FW_CUT_THROUGH = 3,
	LOAD_FW_LAST_INVALID,
};

/*
 * unm_skb_frag{} is to contain mapping info for each SG list. This
 * has to be freed when DMA is complete. This is part of unm_tx_buffer{}.
 */
struct unm_skb_frag {
	uint64_t dma;
	uint32_t length;
};

/*    Following defines are for the state of the buffers    */
#define    UNM_BUFFER_FREE        0
#define    UNM_BUFFER_BUSY        1

/*
 * There will be one unm_buffer per skb packet.    These will be
 * used to save the dma info for pci_unmap_page()
 */
struct unm_cmd_buffer {
	struct sk_buff *skb;
	struct unm_skb_frag fragArray[MAX_BUFFERS_PER_CMD + 1];
	struct pci_dev *pdev;
	uint32_t totalLength;
	uint32_t mss;
	uint32_t port:16, cmd:8, fragCount:8;
	unsigned long time_stamp;
	uint32_t state;
};
#define NX_FILE_FW_READ 1
#define NX_FILE_FW_RELEASED 0
struct nx_firmware {
	uint64_t  *bootld;
	uint64_t  *fw_img;
	uint32_t size;
	uint32_t fw_version;
	uint32_t fw_bios_version;
	uint32_t fw_buildno;
	uint32_t file_fw_state;
};

/* In rx_buffer, we do not need multiple fragments as is a single buffer */
struct unm_rx_buffer {
	struct list_head link;

	struct sk_buff *skb;
	uint64_t dma;
	uint32_t refHandle:16, state:16;
	uint32_t lro_expected_frags;
	uint32_t lro_current_frags;
	uint32_t lro_length;
};

/* Board types */
#define  UNM_NIC_GBE     0x01
#define  UNM_NIC_XGBE    0x02

/*
 * One hardware_context{} per adapter
 * contains interrupt info as well shared hardware info.
 */
typedef struct _hardware_context {
	struct pci_dev *pdev;
	unsigned long pci_base0;
	unsigned long pci_len0;
	unsigned long pci_base1;
	unsigned long pci_len1;
	unsigned long pci_base2;
	unsigned long pci_len2;
	unsigned long first_page_group_end;
	unsigned long first_page_group_start;
	uint32_t fw_capabilities_1;
	uint16_t vendor_id;
	uint16_t device_id;
	uint8_t revision_id;
	uint8_t cut_through;
	uint16_t pci_cmd_word;
	uint16_t board_type;
	int pci_func;
	uint16_t max_ports;
	unm_board_info_t boardcfg;
	uint32_t linkup;
/* 	uint32_t qg_linksup; */

	struct unm_adapter *adapter;

	cmdDescType0_t *cmdDescHead;	/* Address of cmd ring in Phantom */

	uint32_t cmdConsumer;
	uint32_t rcvFlag;
	uint32_t crb_base;
	unsigned long db_base;	/* base of mapped db memory */
	unsigned long db_len;	/* length of mapped db memory */

	unsigned long LEDState;	//LED Test
	unsigned long LEDTestRet;	//LED test
	unsigned long LEDTestLast;	//LED test
	dma_addr_t cmdDesc_physAddr;
	struct pci_dev *cmdDesc_pdev;
	int		qdr_sn_window;
	int		ddr_mn_window;
    unsigned long mn_win_crb;
    unsigned long ms_win_crb;
} hardware_context, *phardware_context;

#define MAX_BUFFERS              2000
#define ADAPTER_UP_MAGIC		777
#define MINIMUM_ETHERNET_FRAME_SIZE  64	/* With FCS */
#define ETHERNET_FCS_SIZE             4

struct netq_stats {
	uint64_t rcvdbadskb;
	uint64_t rxbytes;
	uint64_t no_rcv;
	uint64_t updropped;
};

struct unm_adapter_stats {
	uint64_t rcvdbadskb;
	uint64_t xmitcalled;
	uint64_t xmitedframes;
	uint64_t xmitfinished;
	uint64_t badskblen;
	uint64_t nocmddescriptor;
	uint64_t polled;
	uint64_t uphappy;
	uint64_t updropped;
	uint64_t txdropped;
	uint64_t txOutOfBounceBufDropped;
	uint64_t csummed;
	uint64_t no_rcv;
	uint64_t rxbytes;
	uint64_t txbytes;
	uint64_t ints;
	uint64_t alloc_failures_imm; /* immediate, process_rxbuffer */
	uint64_t alloc_failures_def; /* deferred, nx_post_rx_descriptors */
	uint64_t rcv_buf_crunch;
};

/*
 * Ring which will hold the skbs which can be fed to receive ring
 */
#define UNM_SKB_ARRAY_SIZE 1024

typedef struct unm_recv_skb_ring_s {
	struct sk_buff **skb_array;
	uint32_t interrupt_index;
	uint32_t tasklet_index;
	uint32_t data_size;
} unm_recv_skb_ring_t;

/* descriptor types */
#define RCV_RING_STD       RCV_DESC_NORMAL
#define RCV_RING_JUMBO      RCV_DESC_JUMBO
#define RCV_RING_LRO            RCV_DESC_LRO

/*
 * The following declares a queue data structure for free rx descriptors.
 * This ends up with a 'struct nx_rxbuf_list'
 */

/*
 *
 */
typedef struct nx_free_rbufs {
	uint32_t count;
	struct list_head head;
} nx_free_rbufs_t;

/*
 * Jumbo buffer size is increased by the following size to improve LRO
 * performance.
 */
#if 0
#define	NX_NIC_JUMBO_EXTRA_BUFFER	2048
#else
#define	NX_NIC_JUMBO_EXTRA_BUFFER	0
#endif

/*
 * Rcv Descriptor Context. One such per Rcv Descriptor. There may
 * be one Rcv Descriptor for normal packets, one for jumbo,
 * one for LRO and may be expanded.
 */

typedef struct rds_host_ring_s {
	uint32_t producer;

	/* Num of bufs posted in phantom */

	struct pci_dev *phys_pdev;
	uint32_t dma_size;

	/* rx buffers for receive   */
	struct unm_rx_buffer *rx_buf_arr;
	/* free list */
	atomic_t alloc_failures;
	uint32_t posting;
	nx_free_rbufs_t free_rxbufs;
	struct netq_stats stats;
	uint32_t	truesize_delta;

	int begin_alloc;
} rds_host_ring_t;

/* In FW version 3.4.xyz the max rx rings is 3 and in FW version 4.0.xyz it is 2 */
#define MAX_RX_DESC_RINGS       3

typedef struct sds_host_ring_s {
	uint32_t                producer;
	uint32_t                consumer;
	uint64_t 		count;
	struct pci_dev          *pci_dev;
	struct unm_adapter_s    *adapter;
	struct net_device       *netdev;
	uint32_t                ring_idx;
	uint64_t                ints;
	uint64_t                polled;
	void                    *cons_reg_addr;
	void                    *intr_reg_addr;
	struct napi_struct      napi;
	unsigned int            netdev_irq;
	unsigned int            irq_allocated;
	char                    netdev_name[IFNAMSIZ];
	uint32_t                napi_enable;
	nx_host_sds_ring_t      *ring;
	nx_free_rbufs_t         free_rbufs[MAX_RX_DESC_RINGS];
} sds_host_ring_t;

/*
 * Defines the status ring data structure.
 */
#if 0
typedef struct {
	statusDesc_t *ring;
	dma_addr_t phys_addr;
	uint32_t producer;
	uint32_t consumer;
	struct pci_dev *pci_dev;
	struct unm_adapter_s *adapter;
	struct net_device *netdev;
	uint32_t msix_entry_idx;
	uint32_t ring_idx;
	uint64_t ints;
	uint64_t polled;
	void *cons_reg_addr;
	void *intr_reg_addr;
	nx_free_rbufs_t free_rbufs[MAX_RX_DESC_RINGS];
} nx_sts_ring_ctx_t;
#endif
#define	NX_NIC_MAX_HOST_RSS_RINGS	8

#define UNM_NIC_MSI_ENABLED	0x02
#define UNM_NIC_MSIX_ENABLED	0x04
#define UNM_IS_MSI_FAMILY(ADAPTER)	((ADAPTER)->flags &		\
					 (UNM_NIC_MSI_ENABLED |		\
					  UNM_NIC_MSIX_ENABLED))

#define	NX_USE_MSIX

/* msix defines */
#define MSIX_ENTRIES_PER_ADAPTER	8

#define UNM_MSIX_TBL_SPACE		8192
#define UNM_PCI_REG_MSIX_TBL		0x44

/*
 * Bug: word or char write on MSI-X capcabilities register (0x40) in PCI config
 * space has no effect on register values. Need to write dword.
 */
#define UNM_HWBUG_8_WORKAROUND

/*
 * Bug: Can not reset bit 32 (msix enable bit) on MSI-X capcabilities
 * register (0x40) independently.
 * Need to write 0x0 (zero) to MSI-X capcabilities register in order to reset
 * msix enable bit. On writing zero rest of the bits are not touched.
 */
#define UNM_HWBUG_9_WORKAROUND

#define UNM_MC_COUNT    38	/* == ((UNM_ADDR_L2LU_COUNT-1)/4) -2 */

typedef struct netdev_list_s netdev_list_t;
struct netdev_list_s {
	netdev_list_t *next;
	struct net_device *netdev;
};
/* The structure below is used for loopback test*/
typedef struct unm_test_ctr {
	uint8_t *tx_user_packet_data;
	uint8_t *rx_user_packet_data;
	uint32_t tx_user_packet_length;
	uint32_t rx_datalen;
	uint32_t rx_user_pos;
	uint32_t loopback_start;
	int capture_input;
	int tx_stop;
} unm_test_ctr_t;

typedef struct mac_list_s {
	struct mac_list_s *next;
	uint8_t mac_addr[MAX_ADDR_LEN];
} mac_list_t;

#define	NX_MAX_PKTS_PER_LRO	10
#define	NX_1K_PER_LRO		16
typedef struct {
	__uint64_t	accumulation[NX_MAX_PKTS_PER_LRO];
	__uint64_t	bufsize[NX_1K_PER_LRO];
	__uint64_t	chained_pkts;
	__uint64_t	contiguous_pkts;
} nx_lro_stats_t;

typedef struct {
	__uint8_t	initialized;
	__uint8_t	enabled;
	nx_hash_tbl_t	hash_tbl;
	nx_mem_pool_t	mem_pool;
	nx_lro_stats_t	stats;
} nx_nic_host_lro_t;

/* Status opcode handler */

#define NX_MAX_SDS_DESC                 8
#define NX_NIC_API_VER                  1					
#define NX_WAIT_BIT_MAP_SIZE            4					
#define NX_MAX_COMP_ID                  256					

#define	PORT_BITS	4
#define PORT_MASK	((1 << PORT_BITS) - 1)

#define NX_NIC_NUM_PEGS 5

#define NX_TRACE_ARR_SIZE	4096

#define NX_NIC_TRC_FN(A, F, V) 						\
{													\
	uint16_t i = 0;									\
	unsigned long   flags;							\
	spin_lock_irqsave(&(A)->trc_lock, flags);		\
	i = (A)->trc_idx;								\
	(A)->trc_buf[i].fn_ptr = (uint64_t)(F);			\
	(A)->trc_buf[i].ts     = jiffies;				\
	(A)->trc_buf[i].cpu    = smp_processor_id();	\
	(A)->trc_buf[i].line    = __LINE__;				\
	(A)->trc_buf[i].val    = (uint64_t)(V);			\
	(A)->trc_idx++;									\
	if ((A)->trc_idx == NX_TRACE_ARR_SIZE)			\
		(A)->trc_idx = 0;							\
	spin_unlock_irqrestore(&(A)->trc_lock, flags);	\
}

struct trace_s {
	uint64_t	fn_ptr;
	uint64_t	ts;
	uint32_t	cpu;
	uint32_t	line;
	uint64_t	val;
};

struct fw_dump {
	uint32_t enabled;
	char *pcon[NX_NIC_NUM_PEGS];
	char *phanstat;
	char *srestat;
	char *epgstat;
	char *nicregs;
	char *macstat[UNM_NIU_MAX_XG_PORTS];
	char *epgregs;
	char *sreregs;
};

struct nx_nic_minidump {
    u8  fw_supports_md;
    u8  dump_needed;
    u8  has_valid_dump;
    u8  md_capture_mask;
    u8  md_enabled;
    u8  disable_overwrite;

    u32 md_dump_size;
    u32 md_capture_size;

    u32 md_template_size;
    u32 md_template_ver;

    u64 md_timestamp;

    void* md_template;
    void* md_capture_buff;
};

/* Following structure is for specific port information */
typedef struct unm_adapter_s
{
	nx_host_nic_t *nx_dev;
	hardware_context ahw;
	uint8_t id[32];
	uint8_t procname[8];	/* name of this dev's proc entry */
	uint16_t portnum;	/* port number        */
	uint16_t physical_port;	/* port number        */
	uint16_t link_speed;
	uint16_t link_duplex;
	uint16_t state;		/* state of the port */
	uint16_t link_autoneg;
	uint16_t link_module_type;

	uint16_t attach_flag;

	 /* User configured values for 1Gbps adapter */
	uint16_t cfg_speed;
	uint16_t cfg_duplex;
	uint16_t cfg_autoneg;
		

	int rx_csum;
	int interface_id;	/* -1 indicates unknown */

	struct net_device *netdev;
	struct pci_dev *pdev;
	struct net_device_stats net_stats;
	struct unm_adapter_stats stats;
	nx_nic_host_lro_t	lro;
	unsigned char mac_addr[MAX_ADDR_LEN];
	mac_list_t *mac_list;
	int mtu;

	uint32_t promisc;
	int8_t mc_enabled;
	uint8_t max_mc_count;
	int link_width;
	spinlock_t tx_lock;

	sds_host_ring_t *host_sds_rings;
	int num_sds_rings;
	/* 
	 * Spinlock for exclusive Tx cmd ring processing 
	 * by CPUs. Only one CPU will be doing Tx 
	 * at any given time
	 */
	spinlock_t              tx_cpu_excl;
	rwlock_t adapter_lock;
	spinlock_t lock;
	spinlock_t buf_post_lock;
	spinlock_t ctx_state_lock;
	struct nx_legacy_intr_set	legacy_intr;
	struct work_struct watchdog_task;
	struct work_struct watchdog_task_fw_reset;
	struct work_struct tx_timeout_task;
	struct timer_list watchdog_timer;
	struct timer_list watchdog_timer_fw_reset;
	struct tasklet_struct tx_tasklet;

	uint32_t curr_window;
	uint32_t crb_win; 	/* should replace curr_window */

	uint32_t cmdProducer;
	uint32_t *cmdConsumer;

	uint32_t crb_addr_cmd_producer;
	dma_addr_t crb_addr_cmd_consumer;

	uint32_t lastCmdConsumer;
	/* Num of bufs posted in phantom */
	uint32_t MaxTxDescCount;
	uint32_t MaxRxDescCount;
	uint32_t MaxJumboRxDescCount;
	uint32_t MaxLroRxDescCount;
	/* Variables for send test and loopback are in this container */
	unm_test_ctr_t testCtx;

	int max_rds_rings;
	uint32_t flags;
	int driver_mismatch;
	uint32_t temp;		/* Temperature */
	int pci_using_dac;
	uint64_t dma_mask;
	int msg_type_sync_offload;
	int msg_type_rx_desc;

	u32 alive_counter;
	u32 fw_alive_failures;
	nic_version_t	version;
	nic_version_t	flash_ver;
	nic_version_t	bios_ver;
	const struct firmware *fw;
	__uint32_t fwtype;
	uint32_t bootld_size;

	struct unm_cmd_buffer *cmd_buf_arr;	/* Command buffers for xmit */

	uint8_t			msix_supported;
	uint8_t			max_possible_rss_rings;
	uint8_t			msglvl;
	/* msi-x entries array */
	struct msix_entry msix_entries[MSIX_ENTRIES_PER_ADAPTER];
	int num_msix;
	struct netdev_list_s *netlist;
	int is_up;
	uint32_t led_blink_rate;
	uint32_t led_blink_state;
	/* Coalescing parameters per context */
	nx_nic_intr_coalesce_t coal;
	struct proc_dir_entry *dev_dir;

#define UNM_HOST_DUMMY_DMA_SIZE         1024
	struct {
		void *addr;
		dma_addr_t phys_addr;
	} dummy_dma;
        struct {
                void *addr;
                dma_addr_t phys_addr;
        } nx_lic_dma;
	void *rdma_private;

	struct list_head wait_list;
	uint64_t wait_bit_map[NX_WAIT_BIT_MAP_SIZE];

	struct nx_nic_minidump mdump;
	spinlock_t trc_lock;
	uint16_t trc_idx;
	struct trace_s trc_buf[NX_TRACE_ARR_SIZE];

#ifdef	CONFIG_PM
	__uint32_t	pm_state[32];	/* Can't find a macro but the Linux
					 * code uses 16 dwords.
					 * Being cautious */
#endif
	int tx_timeout_count;
	int auto_fw_reset;
	int removing_module;
	struct nx_firmware nx_fw;
	int multi_ctx;
	int init_link_update_ctr;
	int false_linkup;
	uint32_t num_rx_queues;
	uint64_t fwload_failed_jiffies;
	uint64_t fwload_failed_count;

	struct vlan_group	*vlgrp;
	
	struct fw_dump fw_dmp;

	void (*unm_nic_pci_change_crbwindow)(struct unm_adapter_s *, uint32_t);
	int (*unm_nic_hw_write_wx)(struct unm_adapter_s *, u64, u32);
    int (*unm_nic_hw_indirect_write)(struct unm_adapter_s *, u64, u32);
    int (*unm_nic_hw_write_bar_reg)(struct unm_adapter_s *, u64, u32);
	int (*unm_nic_hw_write_ioctl)(struct unm_adapter_s *, u64,
				      void *, int);
	u32 (*unm_nic_hw_read_wx)(struct unm_adapter_s *, u64);
	u32 (*unm_nic_hw_indirect_read)(struct unm_adapter_s *, u64);
	u32 (*unm_nic_hw_read_bar_reg)(struct unm_adapter_s *, u64);
	int (*unm_nic_hw_read_ioctl)(struct unm_adapter_s *, u64, void *, int);
	int (*unm_nic_pci_mem_read)(struct unm_adapter_s *adapter, u64 off,
				    void *data, int size);

	int (*unm_nic_pci_mem_write)(struct unm_adapter_s *adapter, u64 off,
				     void *data, int size);
	int (*unm_nic_pci_write_immediate)(struct unm_adapter_s *adapter,
					   u64 off, u32 *data);
	int (*unm_nic_pci_read_immediate)(struct unm_adapter_s *adapter,
					  u64 off, u32 *data);
	unsigned long (*unm_nic_pci_set_window) (struct unm_adapter_s *adapter,
						 unsigned long long addr);
	NX_IRQ_HANDLER intr_handler;
} unm_adapter;			/* unm_adapter structure */

#define    PORT_DOWN            0
#define    PORT_UP              1

#define    MAX_HD_DESC_BUFFERS    4	/* number of buffers in 1st desc */

/*Max number of xmit producer threads that can run simultaneously*/
#define    MAX_XMIT_PRODUCERS   16

#define PROC_PARENT            NULL

void unm_nic_pci_change_crbwindow_128M(unm_adapter *adapter, uint32_t wndw);

#define PCI_OFFSET_FIRST_RANGE(adapter, off)	\
	((adapter)->ahw.pci_base0 + off)
#define PCI_OFFSET_SECOND_RANGE(adapter, off)	\
	((adapter)->ahw.pci_base1 + off - SECOND_PAGE_GROUP_START)
#define PCI_OFFSET_THIRD_RANGE(adapter, off)	\
	((adapter)->ahw.pci_base2 + off - THIRD_PAGE_GROUP_START)

#define pci_base_offset(adapter, off)	\
	((((off) < ((adapter)->ahw.first_page_group_end)) &&                                            \
          ((off) >= ((adapter)->ahw.first_page_group_start))) ?			                        \
		((adapter)->ahw.pci_base0 + (off)) :							\
		((((off) < SECOND_PAGE_GROUP_END) && ((off) >= SECOND_PAGE_GROUP_START)) ?		\
			((adapter)->ahw.pci_base1 + (off) - SECOND_PAGE_GROUP_START) :			\
			((((off) < THIRD_PAGE_GROUP_END) && ((off) >= THIRD_PAGE_GROUP_START)) ?	\
				((adapter)->ahw.pci_base2 + (off) - THIRD_PAGE_GROUP_START) :		\
				0)))

#define pci_base(adapter, off)	\
	((((off) < ((adapter)->ahw.first_page_group_end)) &&                                            \
          ((off) >= ((adapter)->ahw.first_page_group_start))) ?			                        \
		((adapter)->ahw.pci_base0) :								\
		((((off) < SECOND_PAGE_GROUP_END) && ((off) >= SECOND_PAGE_GROUP_START)) ?		\
			((adapter)->ahw.pci_base1) :							\
			((((off) < THIRD_PAGE_GROUP_END) && ((off) >= THIRD_PAGE_GROUP_START)) ?	\
				((adapter)->ahw.pci_base2) :						\
				0)))

#define NXRD32(adapter, off) \
	(adapter->unm_nic_hw_read_wx(adapter, off))
#define NXWR32(adapter, off, val) \
	(adapter->unm_nic_hw_write_wx(adapter, off, val))

/*
 * Functions from unm_nic_test.c
 */
int unm_read_blink_state(char *buf, char **start, off_t offset,
				int count, int *eof, void *data);
int unm_write_blink_state(struct file *file, const char *buffer,
				unsigned long count, void *data);
int unm_read_blink_rate(char *buf, char **start, off_t offset,
				int count, int *eof, void *data);
int unm_write_blink_rate(struct file *file, const char *buffer,
				unsigned long count, void *data);
int unm_nic_ioctl(struct net_device *netdev, struct ifreq *ifr, int cmd);
int unm_loopback_test(struct net_device *netdev, int fint, void *ptr,
			     unm_test_ctr_t * testCtx);
int unm_irq_test(unm_adapter * adapter);
int unm_link_test(unm_adapter * adapter);
int unm_led_test(unm_adapter * adapter);

/*
 * Functions from unm_nic_main.c
 */

int nx_nic_send_cmd_descs(struct net_device *dev,
			  cmdDescType0_t * cmd_desc_arr, int nr_elements);
int unm_nic_xmit_frame(struct sk_buff *skb, struct net_device *netdev);
int unm_niu_xg_get_tx_flow_ctl(struct unm_adapter_s *adapter, int *val);
int unm_niu_gbe_get_tx_flow_ctl(struct unm_adapter_s *adapter, int *val);
int unm_niu_gbe_get_rx_flow_ctl(struct unm_adapter_s *adapter, int *val);
int unm_niu_xg_set_tx_flow_ctl(struct unm_adapter_s *adapter, int enable);
int unm_niu_gbe_set_rx_flow_ctl(struct unm_adapter_s *adapter, int enable);
int unm_niu_gbe_set_tx_flow_ctl(struct unm_adapter_s *adapter, int enable);
long unm_niu_gbe_phy_read(struct unm_adapter_s *,
			  long reg, unm_crbword_t * readval);
long unm_niu_gbe_phy_write(struct unm_adapter_s *, long reg, unm_crbword_t val);
long unm_niu_xginit(void);
int xge_loopback(struct unm_adapter_s *, int on);
int xge_link_status(struct unm_adapter_s *, long *status);
int phy_lock(struct unm_adapter_s *adapter);
void phy_unlock(struct unm_adapter_s *adapter);
void nx_free_tx_resources(struct unm_adapter_s *adapter);

/* Functions available from unm_nic_hw.c */
int unm_nic_get_board_info(struct unm_adapter_s *adapter);
void _unm_nic_hw_block_read(struct unm_adapter_s *adapter,
			    u64 off, void *data, int num_words);
void unm_nic_hw_block_read(struct unm_adapter_s *adapter,
			   u64 off, void *data, int num_words);
void _unm_nic_hw_block_write(struct unm_adapter_s *adapter,
			     u64 off, void *data, int num_words);
void unm_nic_hw_block_write(struct unm_adapter_s *adapter,
			    u64 off, void *data, int num_words);
int  unm_nic_pci_mem_write_128M(struct unm_adapter_s *adapter,
			  u64 off, void *data, int size);
int  unm_nic_pci_mem_read_128M(struct unm_adapter_s *adapter,
			 u64 off, void *data, int size);
void unm_nic_mem_block_read(struct unm_adapter_s *adapter, u64 off,
			    void *data, int num_words);
void unm_nic_mem_block_write(struct unm_adapter_s *adapter, u64 off,
			     void *data, int num_words);

u32 unm_nic_hw_read_wx_128M(unm_adapter *adapter, u64 off);
int unm_nic_hw_write_wx_128M(unm_adapter *adapter, u64 off, u32 data);
u32 unm_nic_hw_read_wx_2M(unm_adapter *adapter, u64 off);
int unm_nic_hw_write_wx_2M(unm_adapter *adapter, u64 off, u32 data);
unsigned long unm_nic_pci_set_window_128M (struct unm_adapter_s *adapter, unsigned long long addr);
unsigned long unm_nic_pci_set_window_2M (struct unm_adapter_s *adapter, unsigned long long addr);
int unm_nic_hw_read_ioctl_128M(unm_adapter *adapter, u64 off, void *data, int len);
int unm_nic_hw_write_ioctl_128M(unm_adapter *adapter, u64 off, void *data, int len);
int unm_nic_hw_read_ioctl_2M(unm_adapter *adapter, u64 off, void *data, int len);
int unm_nic_hw_write_ioctl_2M(unm_adapter *adapter, u64 off, void *data, int len);

int unm_nic_pci_mem_read_2M(struct unm_adapter_s *adapter, u64 off,
                            void *data, int size);
int unm_nic_pci_mem_write_2M(struct unm_adapter_s *adapter, u64 off,
                             void *data, int size);

int nx_flash_read_version(struct unm_adapter_s *adapter, __uint32_t offset,
			  nic_version_t *version);


int nx_nic_rom_fast_read_words(unm_adapter *adapter, int addr,
                u8 *bytes, size_t size);

/*
 * Minidump specific
 */
int unm_nic_pci_mem_read_md(struct unm_adapter_s *adapter, u64 off, void *data,
    int size, int mem_type);
int unm_nic_hw_indirect_write_2M(unm_adapter *adapter, u64 off, u32 data);
u32 unm_nic_hw_indirect_read_2M(unm_adapter *adapter, u64 off);

int unm_nic_hw_write_bar_reg_2M(unm_adapter *adapter, u64 off, u32 data);
u32 unm_nic_hw_read_bar_reg_2M(unm_adapter *adapter, u64 off);



/*
 * Note : only 32-bit reads and writes !
 */
int unm_nic_pci_write_immediate_128M(unm_adapter *adapter, u64 off, u32 *data);
int unm_nic_pci_read_immediate_128M(unm_adapter *adapter, u64 off, u32 *data);

int unm_nic_pci_write_immediate_2M(unm_adapter *adapter, u64 off, u32 *data);
int unm_nic_pci_read_immediate_2M(unm_adapter *adapter, u64 off, u32 *data);

int unm_nic_pci_get_crb_addr_2M(unm_adapter *adapter, u64 *off, int len);

int unm_nic_macaddr_set(struct unm_adapter_s *adapter, __uint8_t * addr);
void unm_tcl_resetall(struct unm_adapter_s *adapter);
void unm_tcl_phaninit(struct unm_adapter_s *adapter);
void unm_tcl_postimage(struct unm_adapter_s *adapter);
//int update_core_clock(struct unm_port *port);
int nx_nic_p2_set_mtu(struct unm_adapter_s *adapter, int new_mtu);
long unm_nic_phy_read(unm_adapter * adapter, long reg, __uint32_t *);
long unm_nic_phy_write(unm_adapter * adapter, long reg, __uint32_t);
long unm_nic_init_port(struct unm_adapter_s *adapter);
int unm_crb_write_adapter(unsigned long off, void *data,
			  struct unm_adapter_s *adapter);

/* Functions from unm_nic_init.c */
int nx_phantom_init(struct unm_adapter_s *adapter, int first_time);
int load_from_flash(struct unm_adapter_s *adapter);
int pinit_from_rom(unm_adapter * adapter, int verbose);
int rom_fast_read(struct unm_adapter_s *adapter, int addr, int *valp);
int load_fw(struct unm_adapter_s *adapter, int cmp_versions);
int load_fused_fw(struct unm_adapter_s *adapter, int cmp_versions);
void nx_msleep(unsigned long msecs);
void unm_handle_port_int(unm_adapter * adapter, u32 enable);
int tap_crb_mbist_clear(unm_adapter * adapter);
int unm_nic_set_promisc_mode(struct unm_adapter_s *adapter);
int unm_nic_unset_promisc_mode(struct unm_adapter_s *adapter);
int unm_nic_enable_mcast_filter(struct unm_adapter_s *adapter);
int unm_nic_disable_mcast_filter(struct unm_adapter_s *adapter);
int unm_nic_set_mcast_addr(struct unm_adapter_s *adapter, int index,
			   __u8 * addr);
void nx_nic_p2_stop_port(struct unm_adapter_s *adapter);
int unm_nic_get_board_num(unm_adapter * adapter);

/* Functions from xge_mdio.c */
long unm_xge_mdio_rd(struct unm_adapter_s *adapter, long devaddr, long addr);
void unm_xge_mdio_wr(struct unm_adapter_s *adapter, long devaddr, long addr,
		     long data);
long unm_xge_mdio_rd_port(struct unm_adapter_s *adapter, long port, long devaddr, long addr);
void unm_xge_mdio_wr_port(struct unm_adapter_s *adapter, long port, long devaddr, long addr,
		     long data);

/* Functions from niu.c */

/* Set promiscuous mode for a GbE interface */
native_t unm_niu_set_promiscuous_mode(struct unm_adapter_s *,
				      unm_niu_prom_mode_t mode);
native_t unm_niu_xg_set_promiscuous_mode(struct unm_adapter_s *,
					 unm_niu_prom_mode_t mode);

/* XG versons */
int unm_niu_xg_macaddr_get(struct unm_adapter_s *,
			   unm_ethernet_macaddr_t * addr);
int unm_niu_xg_macaddr_set(struct unm_adapter_s *, unm_ethernet_macaddr_t addr);

/* Generic enable for GbE ports. Will detect the speed of the link. */
long unm_niu_gbe_init_port(long port);

native_t unm_niu_disable_xg_port(struct unm_adapter_s *);

/* get/set the MAC address for a given MAC */
int unm_niu_gbe_macaddr_get(struct unm_adapter_s *adapter,
			unsigned char *addr);
int unm_niu_gbe_macaddr_set(struct unm_adapter_s *adapter,
			unm_ethernet_macaddr_t addr);
/*Tool Functions*/
int unm_led_blink_state_set(unm_adapter *adapter, u32 testval);
int unm_led_blink_rate_set(unm_adapter *adapter, u32 testval);

/* LRO fundtions */
int unm_init_lro(struct unm_adapter_s *adapter); 
void unm_cleanup_lro(struct unm_adapter_s *adapter);
int nx_try_initiate_lro(struct unm_adapter_s *adapter, struct sk_buff *skb,
			__uint32_t rss_hash, __uint32_t ctx_id);
void nx_handle_lro_response(nx_dev_handle_t drv_handle, nic_response_t *rsp);
void nx_lro_delete_ctx_lro (struct unm_adapter_s *adapter, int ctx_id);

void unm_nic_get_wol(struct net_device *netdev, struct ethtool_wolinfo *wol);
void nx_release_fw( struct nx_firmware *nx_fw);

#define UNM_FW_RESET_THRESHOLD  2
#define UNM_FW_RESET_INTERVAL  (25 * HZ)
#define UNM_FW_RESET_FAILED_THRESHOLD  5
#define FW_DEAD 0xdead
#define FW_RESETTING 0x999
#define NX_STATS_DMA_INTERVAL   1000 /* Periodic DMA interval in ms */
#endif
