/* bnx2x.h: Broadcom Everest network driver.
 *
 * Copyright (c) 2007-2011 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * Maintained by: Eilon Greenstein <eilong@broadcom.com>
 * Written by: Eliezer Tamir
 * Based on code from Michael Chan's bnx2 driver
 */

#ifndef BNX2X_H
#define BNX2X_H
#include <linux/pci.h>


/* compilation time flags */

/* define this to make the driver freeze on error to allow getting debug info
 * (you will need to reboot afterwards) */
/* #define BNX2X_STOP_ON_ERROR */

#if defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE)
#define BCM_VLAN			1
#endif


#if (LINUX_VERSION_CODE >= 0x020617) && \
    !defined(NETIF_F_MULTI_QUEUE) || \
    (defined(__VMKLNX__) && VMWARE_ESX_DDK_VERSION >= 40000) /* BNX2X_UPSTREAM */
#define BNX2X_MULTI_QUEUE
#endif

#if (LINUX_VERSION_CODE >= 0x020618) || defined(__VMKLNX__) /* BNX2X_UPSTREAM */
#define BNX2X_NEW_NAPI
#endif

#if !defined(BNX2X_NEW_NAPI) && defined(NAPI_GRO_CB) /* ! BNX2X_UPSTREAM */
#define USE_NAPI_GRO
#endif

#if defined(BNX2X_MULTI_QUEUE) && !defined(__VMKLNX__) /* ! BNX2X_UPSTREAM */
#define BNX2X_SAFC
#endif
#if defined(VMWARE_ESX_DDK_VERSION) /* ! BNX2X_UPSTREAM */
#if VMWARE_ESX_DDK_VERSION >= 40000
#define BNX2X_VMWARE_BMAPILNX
#else
#define __NO_TPA__		1
#endif
#endif

#if defined(CONFIG_PCI_IOV) && defined(BNX2X_IOV) /* ! BNX2X_UPSTREAM */
#define BCM_IOV
#endif

#ifndef BNX2X_UPSTREAM /* ! BNX2X_UPSTREAM */
#include "bnx2x_compat.h"
#endif
#if defined(CONFIG_CNIC) || defined(CONFIG_CNIC_MODULE)
/* DEBUG DEBUG */
#ifndef BNX2X_VF /* No CNIC for VF at the moment */
#define BCM_CNIC 1
#include "cnic_if.h"
#endif /* BNX2X_VF */
#endif

#if !defined(BCM_CNIC) && (LINUX_VERSION_CODE >= 0x020610) /* ! BNX2X_UPSTREAM */
/* DEBUG DEBUG */
#ifndef BNX2X_VF /* No CNIC for VF at the moment */
#define BCM_CNIC 1
#include "cnic_if.h"
#endif /* BNX2X_VF */
#endif

#ifdef BCM_CNIC
#define BNX2X_MIN_MSIX_VEC_CNT 3
#define BNX2X_MSIX_VEC_FP_START 2
#else
#define BNX2X_MIN_MSIX_VEC_CNT 2
#define BNX2X_MSIX_VEC_FP_START 1
#endif

#if (LINUX_VERSION_CODE > 0x02061e) /* BNX2X_UPSTREAM */
#include <linux/mdio.h>
#endif
#include "bnx2x_reg.h"
#include "bnx2x_fw_defs.h"
#include "bnx2x_hsi.h"
#include "bnx2x_link.h"
#include "bnx2x_sp_verbs.h"
#include "bnx2x_dcb.h"

#if defined(__VMKLNX__) && defined(__VMKNETDDI_QUEUEOPS__) /* ! BNX2X_UPSTREAM */
#define BNX2X_NETQ
#endif

#if defined(__VMKLNX__) && (VMWARE_ESX_DDK_VERSION > 50000)  /* ! BNX2X_UPSTREAM */
#define BNX2X_PASSTHRU
#endif

/* error/debug prints */

/* for messages that are currently off */
#define BNX2X_MSG_OFF			0x0
#define BNX2X_MSG_MCP			0x010000 /* was: NETIF_MSG_HW */
#define BNX2X_MSG_STATS			0x020000 /* was: NETIF_MSG_TIMER */
#define BNX2X_MSG_NVM			0x040000 /* was: NETIF_MSG_HW */
#define BNX2X_MSG_DMAE			0x080000 /* was: NETIF_MSG_HW */
#define BNX2X_MSG_SP			0x100000 /* was: NETIF_MSG_INTR */
#define BNX2X_MSG_FP			0x200000 /* was: NETIF_MSG_INTR */
#ifdef  BNX2X_NETQ /* ! BNX2X_UPSTREAM */
#define BNX2X_MSG_NETQ			0x400000
#endif
#define BNX2X_MSG_IOV			0x800000
#define BNX2X_MSG_CNIC			0x1000000

#define DP_LEVEL			KERN_NOTICE	/* was: KERN_DEBUG */

/* regular debug print */
#define DP(__mask, __fmt, __args...)				\
do {								\
	if (bp->msg_enable & (__mask))				\
		printk(DP_LEVEL "[%s:%d(%s)]" __fmt,		\
		       __func__, __LINE__,			\
		       bp->dev ? (bp->dev->name) : "?",		\
		       ##__args);				\
} while (0)

#define DP_CONT(__mask, __fmt, __args...)			\
do {								\
	if (bp->msg_enable & (__mask)) 				\
		pr_cont(__fmt, ##__args); 			\
} while (0)

/* errors debug print */
#define BNX2X_DBG_ERR(__fmt, __args...)				\
do {								\
	if (netif_msg_probe(bp))				\
		pr_err("[%s:%d(%s)]" __fmt,			\
		       __func__, __LINE__,			\
		       bp->dev ? (bp->dev->name) : "?",		\
		       ##__args);				\
} while (0)

/* for errors (never masked) */
#define BNX2X_ERR(__fmt, __args...)				\
do {								\
	pr_err("[%s:%d(%s)]" __fmt,				\
	       __func__, __LINE__,				\
	       bp->dev ? (bp->dev->name) : "?",			\
	       ##__args);					\
	} while (0)

#define BNX2X_ERROR(__fmt, __args...) do { \
	pr_err("[%s:%d]" __fmt, __func__, __LINE__, ##__args); \
	} while (0)


/* before we have a dev->name use dev_info() */
#define BNX2X_DEV_INFO(__fmt, __args...)			 \
do {								 \
	if (netif_msg_probe(bp))				 \
		dev_info(&bp->pdev->dev, __fmt, ##__args);	 \
} while (0)


#ifdef BNX2X_STOP_ON_ERROR
#define bnx2x_panic() do { \
		bp->panic = 1; \
		BNX2X_ERR("driver assert\n"); \
		bnx2x_int_disable(bp); \
		bnx2x_panic_dump(bp); \
	} while (0)
#else
#define bnx2x_panic() do { \
		bp->panic = 1; \
		BNX2X_ERR("driver assert\n"); \
		bnx2x_panic_dump(bp); \
	} while (0)
#endif

#ifdef BNX2X_UPSTREAM /* BNX2X_UPSTREAM */
#define bnx2x_mc_addr(ha)      ((ha)->addr)
#endif


#define U64_LO(x)			(u32)(((u64)(x)) & 0xffffffff)
#define U64_HI(x)			(u32)(((u64)(x)) >> 32)
#define HILO_U64(hi, lo)		((((u64)(hi)) << 32) + (lo))


#define REG_ADDR(bp, offset)		((bp->regview) + (offset))

#define REG_RD(bp, offset)		readl(REG_ADDR(bp, offset))
#define REG_RD8(bp, offset)		readb(REG_ADDR(bp, offset))
#define REG_RD16(bp, offset)		readw(REG_ADDR(bp, offset))

#define REG_WR(bp, offset, val)		writel((u32)val, REG_ADDR(bp, offset))
#define REG_WR8(bp, offset, val)	writeb((u8)val, REG_ADDR(bp, offset))
#define REG_WR16(bp, offset, val)	writew((u16)val, REG_ADDR(bp, offset))

#define REG_RD_IND(bp, offset)		bnx2x_reg_rd_ind(bp, offset)
#define REG_WR_IND(bp, offset, val)	bnx2x_reg_wr_ind(bp, offset, val)

#define REG_RD_DMAE(bp, offset, valp, len32) \
	do { \
		bnx2x_read_dmae(bp, offset, len32);\
		memcpy(valp, bnx2x_sp(bp, wb_data[0]), (len32) * 4); \
	} while (0)

#define REG_WR_DMAE(bp, offset, valp, len32) \
	do { \
		memcpy(bnx2x_sp(bp, wb_data[0]), valp, (len32) * 4); \
		bnx2x_write_dmae(bp, bnx2x_sp_mapping(bp, wb_data), \
				 offset, len32); \
	} while (0)

#define REG_WR_DMAE_LEN(bp, offset, valp, len32) \
	REG_WR_DMAE(bp, offset, valp, len32)

#define VIRT_WR_DMAE_LEN(bp, data, addr, len32, le32_swap) \
	do { \
		memcpy(GUNZIP_BUF(bp), data, (len32) * 4); \
		bnx2x_write_big_buf_wb(bp, addr, len32); \
	} while (0)

#define SHMEM_ADDR(bp, field)		(bp->common.shmem_base + \
					 offsetof(struct shmem_region, field))
#define SHMEM_RD(bp, field)		REG_RD(bp, SHMEM_ADDR(bp, field))
#define SHMEM_WR(bp, field, val)	REG_WR(bp, SHMEM_ADDR(bp, field), val)

#define SHMEM2_ADDR(bp, field)		(bp->common.shmem2_base + \
					 offsetof(struct shmem2_region, field))
#define SHMEM2_RD(bp, field)		REG_RD(bp, SHMEM2_ADDR(bp, field))
#define SHMEM2_WR(bp, field, val)	REG_WR(bp, SHMEM2_ADDR(bp, field), val)
#define MF_CFG_ADDR(bp, field)		(bp->common.mf_cfg_base + \
					 offsetof(struct mf_cfg, field))
#define MF2_CFG_ADDR(bp,field)		(bp->common.mf2_cfg_base + \
					 offsetof(struct mf2_cfg, field))

#define MF_CFG_RD(bp, field)		REG_RD(bp, MF_CFG_ADDR(bp, field))
#define MF_CFG_WR(bp, field, val)	REG_WR(bp, MF_CFG_ADDR(bp, field), (val))
#define MF2_CFG_RD(bp, field)		REG_RD(bp, MF2_CFG_ADDR(bp, field))

#define SHMEM2_HAS(bp, field)		((bp)->common.shmem2_base && 	\
					 (SHMEM2_RD((bp), size) >	\
					 offsetof(struct shmem2_region, field)))

#define EMAC_RD(bp, reg)		REG_RD(bp, emac_base + reg)
#define EMAC_WR(bp, reg, val)		REG_WR(bp, emac_base + reg, val)

/* SP SB indices */

/* General SP events - stats query, cfc delete, etc  */
/* HC_INDEX_DEF_C_ETH_SLOW_PATH */
#define HC_SP_INDEX_ETH_DEF_CONS		3

/* EQ completions */
/* HC_INDEX_DEF_C_EQ_CONS */
#define HC_SP_INDEX_EQ_CONS			7

/* FCoE L2 connection completions */
/* HC_INDEX_DEF_C_ETH_FCOE_CQ_CONS */
#define HC_SP_INDEX_ETH_FCOE_TX_CQ_CONS    	6
/* HC_INDEX_DEF_U_ETH_FCOE_RX_CQ_CONS */
#define HC_SP_INDEX_ETH_FCOE_RX_CQ_CONS 	4
/* iSCSI L2 */
/* HC_INDEX_DEF_C_ETH_ISCSI_CQ_CONS */
#define HC_SP_INDEX_ETH_ISCSI_CQ_CONS		5
/* HC_INDEX_DEF_U_ETH_ISCSI_RX_CQ_CONS */
#define HC_SP_INDEX_ETH_ISCSI_RX_CQ_CONS	1

/******************/

/* Special clients parameters */

/** SB indices */
/* FCoE L2 */
#define BNX2X_FCOE_L2_RX_INDEX \
	(&bp->def_status_blk->sp_sb.\
	index_values[HC_SP_INDEX_ETH_FCOE_RX_CQ_CONS])

#define BNX2X_FCOE_L2_TX_INDEX \
	(&bp->def_status_blk->sp_sb.\
	index_values[HC_SP_INDEX_ETH_FCOE_TX_CQ_CONS])

/* OOO connection */
#define HC_INDEX_OOO_RX_CQ_CONS		HC_INDEX_ETH_RX_CQ_CONS

#define BNX2X_RX_OOO_INDEX \
	(&fp->sb_index_values[HC_INDEX_ETH_RX_CQ_CONS])

/* FWD connection */
#define HC_INDEX_FWD_TX_CQ_CONS		C_SB_ETH_TX_CQ_INDEX

#define BNX2X_TX_FWD_INDEX \
	(&fp->sb_index_values[C_SB_ETH_TX_CQ_INDEX])

/** CIDs and CLIDs:
 *  CLIDs below is a CLID for func 0, then the CLID for other
 *  functions will be calculated by the formula:
 *
 *  FUNC_N_CLID_X = N * NUM_SPECIAL_CLIENTS + FUNC_0_CLID_X
 *
 */

/* iSCSI L2 */
#define BNX2X_ISCSI_ETH_CL_ID_IDX	1
#define BNX2X_ISCSI_ETH_CID		17

/* OOO */
#define BNX2X_OOO_ETH_CL_ID_IDX		0
#define BNX2X_OOO_ETH_CID		16

/* Forwarding: has no client, only CID */
#define BNX2X_FWD_ETH_CID		19

/* FCoE L2 */
#define BNX2X_FCOE_ETH_CL_ID_IDX	2
#define BNX2X_FCOE_ETH_CID		18

/** Additional rings budgeting */
#ifdef BCM_CNIC

#define CNIC_CONTEXT_USE		1
#define OOO_TX_CONTEXT_USE		1
#define OOO_RX_CONTEXT_USE		1
#define FCOE_CONTEXT_USE		1

#else

#define CNIC_CONTEXT_USE		0
#define OOO_TX_CONTEXT_USE		0
#define OOO_RX_CONTEXT_USE		0
#define FCOE_CONTEXT_USE		0

#endif /* BCM_CNIC */

#define OOO_CONTEXT_USE		(OOO_TX_CONTEXT_USE + OOO_RX_CONTEXT_USE)
#define NONE_ETH_CONTEXT_USE	(FCOE_CONTEXT_USE + OOO_CONTEXT_USE)


#define SM_RX_ID			0
#define SM_TX_ID			1

/* fast path */

struct sw_rx_bd {
	struct sk_buff	*skb;
#if (LINUX_VERSION_CODE >= 0x020622) && !defined(BNX2X_OLD_FCOE) /* BNX2X_UPSTREAM */
	DEFINE_DMA_UNMAP_ADDR(mapping);
#else
	DECLARE_PCI_UNMAP_ADDR(mapping)
#endif
};

struct sw_tx_bd {
	struct sk_buff	*skb;
	u16		first_bd;
	u8		flags;
/* Set on the first BD descriptor when there is a split BD */
#define BNX2X_TSO_SPLIT_BD		(1<<0)
};

struct sw_rx_page {
	struct page	*page;
#if (LINUX_VERSION_CODE >= 0x020622) && !defined(BNX2X_OLD_FCOE) /* BNX2X_UPSTREAM */
	DEFINE_DMA_UNMAP_ADDR(mapping);
#else
	DECLARE_PCI_UNMAP_ADDR(mapping)
#endif

};

union db_prod {
	struct doorbell_set_prod data;
	u32		raw;
};


/* MC hsi */
#define BCM_PAGE_SHIFT		12
#define BCM_PAGE_SIZE		(1 << BCM_PAGE_SHIFT)
#define BCM_PAGE_MASK		(~(BCM_PAGE_SIZE - 1))
#define BCM_PAGE_ALIGN(addr)	(((addr) + BCM_PAGE_SIZE - 1) & BCM_PAGE_MASK)

#define PAGES_PER_SGE_SHIFT	0
#define PAGES_PER_SGE		(1 << PAGES_PER_SGE_SHIFT)
#define SGE_PAGE_SIZE		PAGE_SIZE
#define SGE_PAGE_SHIFT		PAGE_SHIFT
#define SGE_PAGE_ALIGN(addr)	PAGE_ALIGN((typeof(PAGE_SIZE))(addr))

/* SGE ring related macros */
#define NUM_RX_SGE_PAGES	2
#define RX_SGE_CNT		(BCM_PAGE_SIZE / sizeof(struct eth_rx_sge))
#define MAX_RX_SGE_CNT		(RX_SGE_CNT - 2)
/* RX_SGE_CNT is promised to be a power of 2 */
#define RX_SGE_MASK		(RX_SGE_CNT - 1)
#define NUM_RX_SGE		(RX_SGE_CNT * NUM_RX_SGE_PAGES)
#define MAX_RX_SGE		(NUM_RX_SGE - 1)
#define NEXT_SGE_IDX(x)		((((x) & RX_SGE_MASK) == \
				  (MAX_RX_SGE_CNT - 1)) ? (x) + 3 : (x) + 1)
#define RX_SGE(x)		((x) & MAX_RX_SGE)

/* Manipulate a bit vector defined as an array of u64 */

/* Number of bits in one sge_mask array element */
#define BIT_VEC64_ELEM_SZ		64
#define BIT_VEC64_ELEM_SHIFT		6
#define BIT_VEC64_ELEM_MASK		((u64)BIT_VEC64_ELEM_SZ - 1)


#define __BIT_VEC64_SET_BIT(el, bit) \
	do { \
		el = ((el) | ((u64)0x1 << (bit))); \
	} while (0)

#define __BIT_VEC64_CLEAR_BIT(el, bit) \
	do { \
		el = ((el) & (~((u64)0x1 << (bit)))); \
	} while (0)


#define BIT_VEC64_SET_BIT(vec64, idx) \
	__BIT_VEC64_SET_BIT((vec64)[(idx) >> BIT_VEC64_ELEM_SHIFT], \
			   (idx) & BIT_VEC64_ELEM_MASK)

#define BIT_VEC64_CLEAR_BIT(vec64, idx) \
	__BIT_VEC64_CLEAR_BIT((vec64)[(idx) >> BIT_VEC64_ELEM_SHIFT], \
			     (idx) & BIT_VEC64_ELEM_MASK)

#define BIT_VEC64_TEST_BIT(vec64, idx) \
	(((vec64)[(idx) >> BIT_VEC64_ELEM_SHIFT] >> \
	((idx) & BIT_VEC64_ELEM_MASK)) & 0x1)

/* Creates a bitmask of all ones in less significant bits.
   idx - index of the most significant bit in the created mask */
#define BIT_VEC64_ONES_MASK(idx) \
		(((u64)0x1 << (((idx) & BIT_VEC64_ELEM_MASK) + 1)) - 1)
#define BIT_VEC64_ELEM_ONE_MASK	((u64)(~0))

/*******************************************************/



/* Number of u64 elements in SGE mask array */
#define RX_SGE_MASK_LEN			((NUM_RX_SGE_PAGES * RX_SGE_CNT) / \
					 BIT_VEC64_ELEM_SZ)
#define RX_SGE_MASK_LEN_MASK		(RX_SGE_MASK_LEN - 1)
#define NEXT_SGE_MASK_ELEM(el)		(((el) + 1) & RX_SGE_MASK_LEN_MASK)

union host_hc_status_block {
	/* pointer to fp status block e1x */
	volatile struct host_hc_status_block_e1x * e1x_sb;
	/* pointer to fp status block e2 */
	volatile struct host_hc_status_block_e2  * e2_sb;
};

struct bnx2x_eth_q_stats {
	u32 total_bytes_received_hi;
	u32 total_bytes_received_lo;
	u32 total_bytes_transmitted_hi;
	u32 total_bytes_transmitted_lo;
	u32 total_unicast_packets_received_hi;
	u32 total_unicast_packets_received_lo;
	u32 total_multicast_packets_received_hi;
	u32 total_multicast_packets_received_lo;
	u32 total_broadcast_packets_received_hi;
	u32 total_broadcast_packets_received_lo;
	u32 total_unicast_packets_transmitted_hi;
	u32 total_unicast_packets_transmitted_lo;
	u32 total_multicast_packets_transmitted_hi;
	u32 total_multicast_packets_transmitted_lo;
	u32 total_broadcast_packets_transmitted_hi;
	u32 total_broadcast_packets_transmitted_lo;
	u32 valid_bytes_received_hi;
	u32 valid_bytes_received_lo;

	u32 etherstatsoverrsizepkts_hi;
	u32 etherstatsoverrsizepkts_lo;
	u32 no_buff_discard_hi;
	u32 no_buff_discard_lo;

	u32 driver_xoff;
	u32 rx_err_discard_pkt;
	u32 rx_skb_alloc_failed;
	u32 hw_csum_err;
};

#define Q_STATS_OFFSET32(stat_name) \
			(offsetof(struct bnx2x_eth_q_stats, stat_name) / 4)

struct bnx2x_fastpath {
	struct bnx2x		*bp; /* parent */

#define BNX2X_NAPI_WEIGHT       128
#ifdef BNX2X_NEW_NAPI /* BNX2X_UPSTREAM */
	struct napi_struct	napi;
#else
#ifdef USE_NAPI_GRO
	struct napi_struct	napi;
#endif
	struct net_device	dummy_netdev;
#endif
	union host_hc_status_block	status_blk;
	/* chip independed shortcuts into sb structure */
	__le16 			*sb_index_values;
	__le16 			*sb_running_index;
	/* chip independed shortcut into rx_prods_offset memory */
	u32 			ustorm_rx_prods_offset;

	dma_addr_t		status_blk_mapping;

	struct sw_tx_bd		*tx_buf_ring;

	union eth_tx_bd_types	*tx_desc_ring;
	dma_addr_t		tx_desc_mapping;

	struct sw_rx_bd		*rx_buf_ring;	/* BDs mappings ring */
	struct sw_rx_page	*rx_page_ring;	/* SGE pages mappings ring */

	struct eth_rx_bd	*rx_desc_ring;
	dma_addr_t		rx_desc_mapping;

	union eth_rx_cqe	*rx_comp_ring;
	dma_addr_t		rx_comp_mapping;

	/* SGE ring */
	struct eth_rx_sge	*rx_sge_ring;
	dma_addr_t		rx_sge_mapping;

	u64			sge_mask[RX_SGE_MASK_LEN];

	u32			cid;
	u16			state;
#define BNX2X_FP_STATE_CLOSED		0
#define BNX2X_FP_STATE_IRQ		0x8000
#define BNX2X_FP_STATE_OPENING		0x9000
#define BNX2X_FP_STATE_OPEN		0xa000
#define BNX2X_FP_STATE_HALTING		0xb000
#define BNX2X_FP_STATE_HALTED		0xc000
#define BNX2X_FP_STATE_TERMINATING	0xd000
#define BNX2X_FP_STATE_TERMINATED	0xe000

	u8			index;		/* number in fp array */
	u8			cl_id;		/* eth client id */
	u8			cl_qzone_id;
	u8			fw_sb_id;	/* status block number in FW */
	u8			igu_sb_id;	/* status block number in HW */
#if defined(BNX2X_SAFC) || defined(__VMKLNX__) 	/* ! BNX2X_UPSTREAM */
	u8			cos;		/* unused for ESX -
						 * alignment only
						 */
#endif
	union db_prod		tx_db;

	u16			tx_pkt_prod;
	u16			tx_pkt_cons;
	u16			tx_bd_prod;
	u16			tx_bd_cons;
	__le16			*tx_cons_sb;

	__le16			fp_hc_idx;

#ifdef BCM_CNIC
	/* number of availiable buffers on the OOO Rx ring */
	u16			rx_pkts_avail;
#endif
	u16			rx_bd_prod;
	u16			rx_bd_cons;
	u16			rx_comp_prod;
	u16			rx_comp_cons;
	u16			rx_sge_prod;
	/* The last maximal completed SGE */
	u16			last_max_sge;
	__le16			*rx_cons_sb;

#ifdef BNX2X_NETQ /* ! BNX2X_UPSTREAM */
	u16			netq_filter_event;
	u16			netq_flags;
#define BNX2X_NETQ_RX_QUEUE_ALLOCATED	0x0001
#define BNX2X_NETQ_TX_QUEUE_ALLOCATED	0x0002
#define BNX2X_NETQ_RX_QUEUE_ACTIVE	0x0004
#define BNX2X_NETQ_TX_QUEUE_ACTIVE	0x0008

#define BNX2X_IS_NETQ_RX_QUEUE_ALLOCATED(fp) \
			((fp->netq_flags & BNX2X_NETQ_RX_QUEUE_ALLOCATED) == \
			  BNX2X_NETQ_RX_QUEUE_ALLOCATED)
#define BNX2X_IS_NETQ_TX_QUEUE_ALLOCATED(fp) \
			((fp->netq_flags & BNX2X_NETQ_TX_QUEUE_ALLOCATED) == \
			  BNX2X_NETQ_TX_QUEUE_ALLOCATED)
#define BNX2X_IS_NETQ_RX_QUEUE_ACTIVE(fp) \
			((fp->netq_flags & BNX2X_NETQ_RX_QUEUE_ACTIVE) == \
			  BNX2X_NETQ_RX_QUEUE_ACTIVE)
#define BNX2X_IS_NETQ_TX_QUEUE_ACTIVE(fp) \
			((fp->netq_flags & BNX2X_NETQ_TX_QUEUE_ACTIVE) == \
			  BNX2X_NETQ_TX_QUEUE_ACTIVE)

#if (VMWARE_ESX_DDK_VERSION >= 41000)

#define BNX2X_NETQ_FP_FEATURE_RESERVED_SHIFT_BIT	8
#define BNX2X_NETQ_FP_NONE_RESERVED	\
			(0 << BNX2X_NETQ_FP_FEATURE_RESERVED_SHIFT_BIT)

#define BNX2X_NETQ_FP_LRO_RESERVED	\
			(1 << BNX2X_NETQ_FP_FEATURE_RESERVED_SHIFT_BIT)

#define BNX2X_NETQ_FP_LRO_RESERVED_MAX_NUM(bp)	(!CHIP_IS_E1(bp) ? 2 : 1)
#define BNX2X_NETQ_FP_FEATURES_RESERVED        	1
#define BNX2X_NETQ_FP_FEATURES_RESERVED_MASK    \
		       (((1 << BNX2X_NETQ_FP_FEATURES_RESERVED) - 1) << \
			       BNX2X_NETQ_FP_FEATURE_RESERVED_SHIFT_BIT)

#define BNX2X_IS_NETQ_FP_FEAT_NONE_RESERVED(fp) \
	       (!(fp->netq_flags & BNX2X_NETQ_FP_FEATURES_RESERVED_MASK))
#define BNX2X_IS_NETQ_FP_FEAT_LRO_RESERVED(fp) \
	       ((fp->netq_flags & BNX2X_NETQ_FP_FEATURES_RESERVED_MASK) == \
		 BNX2X_NETQ_FP_LRO_RESERVED)
#endif
#endif

	unsigned long		tx_pkt,
				rx_pkt,
				rx_calls;

	/* TPA related */
	struct sw_rx_bd		tpa_pool[ETH_MAX_AGGREGATION_QUEUES_E1H];
	u8			tpa_state[ETH_MAX_AGGREGATION_QUEUES_E1H];
#define BNX2X_TPA_START			1
#define BNX2X_TPA_STOP			2
	u8			disable_tpa;
#ifdef BNX2X_STOP_ON_ERROR
	u64			tpa_queue_used;
#endif

	struct tstorm_per_queue_stats old_tclient;
	struct ustorm_per_queue_stats old_uclient;
	struct xstorm_per_queue_stats old_xclient;
	struct bnx2x_eth_q_stats eth_q_stats;

	/* The size is calculated using the following:
	     sizeof name field from netdev structure +
	     4 ('-Xx-' string) +
	     4 (for the digits and to make it DWORD aligned) */
#define FP_NAME_SIZE		(sizeof(((struct net_device *)0)->name) + 8)
	char			name[FP_NAME_SIZE];

	/* MACs object */
	struct bnx2x_vlan_mac_obj mac_obj;

	/* MACs object */
	struct bnx2x_vlan_mac_obj vlan_obj;

	/* MACs object */
	struct bnx2x_vlan_mac_obj vlan_mac_obj;
};

#define bnx2x_fp(bp, nr, var)		(bp->fp[nr].var)

/* FCoE L2 `fastpath' entry is right after the eth entries */
#define FCOE_IDX			BNX2X_NUM_ETH_QUEUES(bp)
#define bnx2x_fcoe_fp(bp)		(&bp->fp[FCOE_IDX])
#define bnx2x_fcoe(bp, var)		(bnx2x_fcoe_fp(bp)->var)

/* OOO Tx L2 (FWD) `fastpath' entry is after the FCoE L2 entry */
#define FWD_IDX				(BNX2X_NUM_ETH_QUEUES(bp) + \
						OOO_TX_CONTEXT_USE)
#define bnx2x_fwd_fp(bp)		(&bp->fp[FWD_IDX])
#define bnx2x_fwd(bp, var)		(bnx2x_fwd_fp(bp)->var)

/* OOO Rx L2 `fastpath' entry is the last entry */
#define OOO_IDX			(BNX2X_NUM_ETH_QUEUES(bp) + OOO_CONTEXT_USE)
#define bnx2x_ooo_fp(bp)		(&bp->fp[OOO_IDX])
#define bnx2x_ooo(bp, var)		(bnx2x_ooo_fp(bp)->var)

#ifdef BCM_CNIC
#define IS_FCOE_FP(fp)			(fp->index == FCOE_IDX)
#define IS_FCOE_IDX(idx)		((idx) == FCOE_IDX)
#define IS_FWD_FP(fp)			(fp->index == FWD_IDX)
#define IS_FWD_IDX(idx)			((idx) == FWD_IDX)
#define IS_OOO_FP(fp)			(fp->index == OOO_IDX)
#define IS_OOO_IDX(idx)			((idx) == OOO_IDX)
#else
#define IS_FCOE_FP(fp)		false
#define IS_FWD_FP(fp)		false
#define IS_OOO_FP(fp)		false
#define IS_FCOE_IDX(idx)	false
#define IS_FWD_IDX(idx)		false
#define IS_OOO_IDX(idx)		false
#endif

#if defined(BNX2X_VMWARE_BMAPILNX) /* ! BNX2X_UPSTREAM */

#define BNX2X_VMWARE_CIM_CMD_ENABLE_NIC		0x0001
#define BNX2X_VMWARE_CIM_CMD_DISABLE_NIC	0x0002
#define BNX2X_VMWARE_CIM_CMD_REG_READ		0x0003
#define BNX2X_VMWARE_CIM_CMD_REG_WRITE		0x0004
#define BNX2X_VMWARE_CIM_CMD_GET_NIC_PARAM	0x0005
#define BNX2X_VMWARE_CIM_CMD_GET_NIC_STATUS	0x0006

struct bnx2x_ioctl_reg_read_req {
	u32 reg_offset;
	u32 reg_value;
} __attribute__((packed));

struct bnx2x_ioctl_reg_write_req {
	u32 reg_offset;
	u32 reg_value;
} __attribute__((packed));

struct bnx2x_ioctl_get_nic_param_req {
	u32 version;
	u32 mtu;
	u8  current_mac_addr[8];
} __attribute__((packed));

struct bnx2x_ioctl_get_nic_status_req
{
	u32 nic_status; /*  1: Up, 0: Down */
} __attribute__((packed));

struct bnx2x_ioctl_req {
	u32 cmd;
	union {
		/* no struct for reset_nic command */
		struct bnx2x_ioctl_reg_read_req reg_read;
		struct bnx2x_ioctl_reg_write_req reg_write;
		struct bnx2x_ioctl_get_nic_param_req get_nic_param;
		struct bnx2x_ioctl_get_nic_status_req get_nic_status;
	} cmd_req;
} __attribute__((packed));

#endif

/* MC hsi */
#define MAX_FETCH_BD		13	/* HW max BDs per packet */
#define RX_COPY_THRESH		92

#define NUM_TX_RINGS		16
#define TX_DESC_CNT		(BCM_PAGE_SIZE / sizeof(union eth_tx_bd_types))
#define MAX_TX_DESC_CNT		(TX_DESC_CNT - 1)
#define NUM_TX_BD		(TX_DESC_CNT * NUM_TX_RINGS)
#define MAX_TX_BD		(NUM_TX_BD - 1)
#define MAX_TX_AVAIL		(MAX_TX_DESC_CNT * NUM_TX_RINGS - 2)
#define NEXT_TX_IDX(x)		((((x) & MAX_TX_DESC_CNT) == \
				  (MAX_TX_DESC_CNT - 1)) ? (x) + 2 : (x) + 1)
#define TX_BD(x)		((x) & MAX_TX_BD)
#define TX_BD_POFF(x)		((x) & MAX_TX_DESC_CNT)

/* The RX BD ring is special, each bd is 8 bytes but the last one is 16 */
#define NUM_RX_RINGS		8
#define RX_DESC_CNT		(BCM_PAGE_SIZE / sizeof(struct eth_rx_bd))
#define MAX_RX_DESC_CNT		(RX_DESC_CNT - 2)
#define RX_DESC_MASK		(RX_DESC_CNT - 1)
#define NUM_RX_BD		(RX_DESC_CNT * NUM_RX_RINGS)
#define MAX_RX_BD		(NUM_RX_BD - 1)
#define MAX_RX_AVAIL		(MAX_RX_DESC_CNT * NUM_RX_RINGS - 2)
#define MIN_RX_SIZE_OOO		40
#define MIN_RX_SIZE		128
#define MIN_RX_SIZE_TPA		(max_t(u32, 72, MIN_RX_SIZE))
#define MIN_RX_SIZE_NONTPA	(max_t(u32, 10, MIN_RX_SIZE))
#if defined(__VMKLNX__) /* ! BNX2X_UPSTREAM */
#define INIT_JUMBO_RX_RING_SIZE	(max_t(u32, (MAX_RX_AVAIL / 16), 256)) /* 256 */
#define INIT_RX_RING_SIZE	(MAX_RX_AVAIL / 4)
#endif
#define NEXT_RX_IDX(x)		((((x) & RX_DESC_MASK) == \
				  (MAX_RX_DESC_CNT - 1)) ? (x) + 3 : (x) + 1)
#define RX_BD(x)		((x) & MAX_RX_BD)

/* As long as CQE is 4 times bigger than BD entry we have to allocate
   4 times more pages for CQ ring in order to keep it balanced with
   BD ring */
#define NUM_RCQ_RINGS		(NUM_RX_RINGS * 4)
#define RCQ_DESC_CNT		(BCM_PAGE_SIZE / sizeof(union eth_rx_cqe))
#define MAX_RCQ_DESC_CNT	(RCQ_DESC_CNT - 1)
#define NUM_RCQ_BD		(RCQ_DESC_CNT * NUM_RCQ_RINGS)
#define MAX_RCQ_BD		(NUM_RCQ_BD - 1)
#define MAX_RCQ_AVAIL		(MAX_RCQ_DESC_CNT * NUM_RCQ_RINGS - 2)
#define NEXT_RCQ_IDX(x)		((((x) & MAX_RCQ_DESC_CNT) == \
				  (MAX_RCQ_DESC_CNT - 1)) ? (x) + 2 : (x) + 1)
#define RCQ_BD(x)		((x) & MAX_RCQ_BD)

#if !defined(__VMKLNX__) /* BNX2X_UPSTREAM */
#define INIT_OOO_RING_SIZE	(MAX_RCQ_DESC_CNT * NUM_RCQ_RINGS \
				 - MAX_SPQ_PENDING)
#else
/*  The minimium number of entires in the OOO ring is 16.  But having
 *  40 entries should be enough for ESX */
#define INIT_OOO_RING_SIZE	40
#endif



/* This is needed for determining of last_max */
#define SUB_S16(a, b)		(s16)((s16)(a) - (s16)(b))
#define SUB_S32(a, b)		(s32)((s32)(a) - (s32)(b))


#define BNX2X_SWCID_SHIFT	17
#define BNX2X_SWCID_MASK	((0x1 << BNX2X_SWCID_SHIFT) - 1)

/* used on a CID received from the HW */
#define SW_CID(x)			(le32_to_cpu(x) & BNX2X_SWCID_MASK)
#define CQE_CMD(x)			(le32_to_cpu(x) >> \
					COMMON_RAMROD_ETH_RX_CQE_CMD_ID_SHIFT)

#define BD_UNMAP_ADDR(bd)		HILO_U64(le32_to_cpu((bd)->addr_hi), \
						 le32_to_cpu((bd)->addr_lo))
#define BD_UNMAP_LEN(bd)		(le16_to_cpu((bd)->nbytes))

#define BNX2X_DB_MIN_SHIFT		3	/* 8 bytes */
#define BNX2X_DB_SHIFT			7	/* 128 bytes*/
#if (BNX2X_DB_SHIFT < BNX2X_DB_MIN_SHIFT)
#error "Min DB doorbell stride is 8"
#endif
#define DPM_TRIGER_TYPE			0x40
#define DOORBELL(bp, cid, val) \
	do { \
		writel((u32)(val), bp->doorbells + (bp->db_size * (cid)) + \
		       DPM_TRIGER_TYPE); \
	} while (0)


/* TX CSUM helpers */
#if (LINUX_VERSION_CODE >= 0x020616) && !defined(__VMKLNX__) /* BNX2X_UPSTREAM */
#define SKB_CS_OFF(skb)		(offsetof(struct tcphdr, check) - \
				 skb->csum_offset)
#define SKB_CS(skb)		(*(u16 *)(skb_transport_header(skb) + \
					  skb->csum_offset))
#else
#define SKB_CS_OFF(skb)		(offsetof(struct tcphdr, check) - skb->csum)
#define SKB_CS(skb)		(*(u16 *)(skb->h.raw + skb->csum))
#endif

#define pbd_tcp_flags(skb)	(ntohl(tcp_flag_word(tcp_hdr(skb)))>>16 & 0xff)

#define XMIT_PLAIN			0
#define XMIT_CSUM_V4			0x1
#define XMIT_CSUM_V6			0x2
#define XMIT_CSUM_TCP			0x4
#define XMIT_GSO_V4			0x8
#define XMIT_GSO_V6			0x10

#define XMIT_CSUM			(XMIT_CSUM_V4 | XMIT_CSUM_V6)
#define XMIT_GSO			(XMIT_GSO_V4 | XMIT_GSO_V6)


/* stuff added to make the code fit 80Col */

#define CQE_TYPE(cqe_fp_flags)	((cqe_fp_flags) & ETH_FAST_PATH_RX_CQE_TYPE)

#define TPA_TYPE_START			ETH_FAST_PATH_RX_CQE_START_FLG
#define TPA_TYPE_END			ETH_FAST_PATH_RX_CQE_END_FLG
#define TPA_TYPE(cqe_fp_flags)		((cqe_fp_flags) & \
					 (TPA_TYPE_START | TPA_TYPE_END))

#define ETH_RX_ERROR_FALGS		ETH_FAST_PATH_RX_CQE_PHY_DECODE_ERR_FLG

#define BNX2X_IP_CSUM_ERR(cqe) \
			(!((cqe)->fast_path_cqe.status_flags & \
			   ETH_FAST_PATH_RX_CQE_IP_XSUM_NO_VALIDATION_FLG) && \
			 ((cqe)->fast_path_cqe.type_error_flags & \
			  ETH_FAST_PATH_RX_CQE_IP_BAD_XSUM_FLG))

#define BNX2X_L4_CSUM_ERR(cqe) \
			(!((cqe)->fast_path_cqe.status_flags & \
			   ETH_FAST_PATH_RX_CQE_L4_XSUM_NO_VALIDATION_FLG) && \
			 ((cqe)->fast_path_cqe.type_error_flags & \
			  ETH_FAST_PATH_RX_CQE_L4_BAD_XSUM_FLG))

#define BNX2X_RX_CSUM_OK(cqe) \
			(!(BNX2X_L4_CSUM_ERR(cqe) || BNX2X_IP_CSUM_ERR(cqe)))

#define BNX2X_PRS_FLAG_OVERETH_IPV4(flags) \
				(((le16_to_cpu(flags) & \
				   PARSING_FLAGS_OVER_ETHERNET_PROTOCOL) >> \
				  PARSING_FLAGS_OVER_ETHERNET_PROTOCOL_SHIFT) \
				 == PRS_FLAG_OVERETH_IPV4)
#define BNX2X_RX_SUM_FIX(cqe) \
	BNX2X_PRS_FLAG_OVERETH_IPV4(cqe->fast_path_cqe.pars_flags.flags)


#define FP_USB_FUNC_OFF	\
			offsetof(struct cstorm_status_block_u, func)
#define FP_CSB_FUNC_OFF	\
			offsetof(struct cstorm_status_block_c, func)

#define HC_INDEX_TOE_RX_CQ_CONS		0 /* Formerly Ustorm TOE CQ index */
					  /* (HC_INDEX_U_TOE_RX_CQ_CONS)  */
#define HC_INDEX_ETH_RX_CQ_CONS		1 /* Formerly Ustorm ETH CQ index */
					  /* (HC_INDEX_U_ETH_RX_CQ_CONS)  */
#define HC_INDEX_ETH_RX_BD_CONS		2 /* Formerly Ustorm ETH BD index */
					  /* (HC_INDEX_U_ETH_RX_BD_CONS)  */

#define HC_INDEX_TOE_TX_CQ_CONS		4 /* Formerly Cstorm TOE CQ index   */
					  /* (HC_INDEX_C_TOE_TX_CQ_CONS)    */
#define HC_INDEX_ETH_TX_CQ_CONS		5 /* Formerly Cstorm ETH CQ index   */
					  /* (HC_INDEX_C_ETH_TX_CQ_CONS)    */

#define U_SB_ETH_RX_CQ_INDEX		HC_INDEX_ETH_RX_CQ_CONS
#define U_SB_ETH_RX_BD_INDEX		HC_INDEX_ETH_RX_BD_CONS
#define C_SB_ETH_TX_CQ_INDEX		HC_INDEX_ETH_TX_CQ_CONS

#define BNX2X_RX_SB_INDEX \
	(&fp->sb_index_values[HC_INDEX_ETH_RX_CQ_CONS])

#define BNX2X_TX_SB_INDEX \
	(&fp->sb_index_values[C_SB_ETH_TX_CQ_INDEX])


/* end of fast path */

/* common */

struct bnx2x_common {

	u32			chip_id;
/* chip num:16-31, rev:12-15, metal:4-11, bond_id:0-3 */
#define CHIP_ID(bp)			(bp->common.chip_id & 0xfffffff0)

#define CHIP_NUM(bp)			(bp->common.chip_id >> 16)
#define CHIP_NUM_57710			0x164e
#define CHIP_NUM_57711			0x164f
#define CHIP_NUM_57711E			0x1650
#define CHIP_NUM_57712			0x1662
#define CHIP_NUM_57712E			0x1663
#define CHIP_NUM_57712VF		0x166f
#define CHIP_NUM_57713			0x1651
#define CHIP_NUM_57713E			0x1652
#define CHIP_IS_E1(bp)			(CHIP_NUM(bp) == CHIP_NUM_57710)
#define CHIP_IS_57711(bp)		(CHIP_NUM(bp) == CHIP_NUM_57711)
#define CHIP_IS_57711E(bp)		(CHIP_NUM(bp) == CHIP_NUM_57711E)
#define CHIP_IS_57712(bp)		(CHIP_NUM(bp) == CHIP_NUM_57712)
#define CHIP_IS_57712VF(bp)		(CHIP_NUM(bp) == CHIP_NUM_57712VF)
#define CHIP_IS_57712E(bp)		(CHIP_NUM(bp) == CHIP_NUM_57712E)
#define CHIP_IS_57713(bp)		(CHIP_NUM(bp) == CHIP_NUM_57713)
#define CHIP_IS_57713E(bp)		(CHIP_NUM(bp) == CHIP_NUM_57713E)
#define CHIP_IS_E1H(bp)			(CHIP_IS_57711(bp) || \
					 CHIP_IS_57711E(bp))
#define CHIP_IS_E2(bp)			(CHIP_IS_57712(bp) || \
					 CHIP_IS_57712E(bp) || \
					 CHIP_IS_57712VF(bp) || \
					 CHIP_IS_57713(bp) || \
					 CHIP_IS_57713E(bp))
#define CHIP_IS_E1x(bp)			(CHIP_IS_E1((bp)) || CHIP_IS_E1H((bp)))


#define IS_VF(bp)			(CHIP_NUM(bp) == CHIP_NUM_57712VF)
#define HAS_DSB(bp)                     (!IS_VF(bp))
#define IS_E1H_OFFSET			(CHIP_IS_E1H(bp) || CHIP_IS_E2(bp))

#define CHIP_REV(bp)			(bp->common.chip_id & 0x0000f000)
#define CHIP_REV_Ax			0x00000000
/* assume maximum 5 revisions */
#define CHIP_REV_IS_SLOW(bp)		(CHIP_REV(bp) > 0x00005000)
/* Emul versions are A=>0xe, B=>0xc, C=>0xa, D=>8, E=>6 */
#define CHIP_REV_IS_EMUL(bp)		((CHIP_REV_IS_SLOW(bp)) && \
					 !(CHIP_REV(bp) & 0x00001000))
/* FPGA versions are A=>0xf, B=>0xd, C=>0xb, D=>9, E=>7 */
#define CHIP_REV_IS_FPGA(bp)		((CHIP_REV_IS_SLOW(bp)) && \
					 (CHIP_REV(bp) & 0x00001000))

#define CHIP_TIME(bp)			((CHIP_REV_IS_EMUL(bp)) ? 2000 : \
					((CHIP_REV_IS_FPGA(bp)) ? 200 : 1))

#define CHIP_METAL(bp)			(bp->common.chip_id & 0x00000ff0)
#define CHIP_BOND_ID(bp)		(bp->common.chip_id & 0x0000000f)

	int			flash_size;
#define BNX2X_NVRAM_1MB_SIZE			0x20000	/* 1M bit in bytes */
#define BNX2X_NVRAM_TIMEOUT_COUNT		30000
#define BNX2X_NVRAM_PAGE_SIZE			256

	u32			shmem_base;
	u32			shmem2_base;
	u32			mf_cfg_base;
	u32			mf2_cfg_base;

	u32			hw_config;

	u32			bc_ver;

	u8			int_block;
#define INT_BLOCK_HC			0
#define INT_BLOCK_IGU			1
#define INT_BLOCK_MODE_NORMAL		0
#define INT_BLOCK_MODE_BW_COMP		2
#define CHIP_INT_MODE_IS_NBC(bp) 		\
			(CHIP_IS_E2(bp) &&	\
			!((bp)->common.int_block & INT_BLOCK_MODE_BW_COMP))
#define CHIP_INT_MODE_IS_BC(bp) (!CHIP_INT_MODE_IS_NBC(bp))

	u8			chip_port_mode;
#define CHIP_4_PORT_MODE			0x0
#define CHIP_2_PORT_MODE			0x1
#define CHIP_PORT_MODE_NONE			0x2
#define CHIP_MODE(bp)			(bp->common.chip_port_mode)
#define CHIP_MODE_IS_4_PORT(bp) (CHIP_MODE(bp) == CHIP_4_PORT_MODE)
};

/* IGU MSIX STATISTICS on 57712: 64 for VFs; 4 for PFs; 4 for Attentions */
#define BNX2X_IGU_STAS_MSG_VF_CNT 64
#define BNX2X_IGU_STAS_MSG_PF_CNT 4

/* end of common */

/* port */

struct nig_stats {
	u32 brb_discard;
	u32 brb_packet;
	u32 brb_truncate;
	u32 flow_ctrl_discard;
	u32 flow_ctrl_octets;
	u32 flow_ctrl_packet;
	u32 mng_discard;
	u32 mng_octet_inp;
	u32 mng_octet_out;
	u32 mng_packet_inp;
	u32 mng_packet_out;
	u32 pbf_octets;
	u32 pbf_packet;
	u32 safc_inp;
	u32 egress_mac_pkt0_lo;
	u32 egress_mac_pkt0_hi;
	u32 egress_mac_pkt1_lo;
	u32 egress_mac_pkt1_hi;
};

struct bnx2x_port {
	u32			pmf;

	u32			link_config[LINK_CONFIG_SIZE];

	u32			supported[LINK_CONFIG_SIZE];
/* link settings - missing defines */
#define SUPPORTED_2500baseX_Full	(1 << 15)

	u32			advertising[LINK_CONFIG_SIZE];
/* link settings - missing defines */
#define ADVERTISED_2500baseX_Full	(1 << 15)

	u32			phy_addr;

	/* used to synchronize phy accesses */
	struct mutex		phy_mutex;
	int			need_hw_lock;

	u32			port_stx;

	struct nig_stats	old_nig_stats;
};

/* end of port */

/****************************************************************************
* Statistics Macros
****************************************************************************/

/* sum[hi:lo] += add[hi:lo] */
#define ADD_64(s_hi, a_hi, s_lo, a_lo) \
	do { \
		s_lo += a_lo; \
		s_hi += a_hi + ((s_lo < a_lo) ? 1 : 0); \
	} while (0)

/* difference = minuend - subtrahend */
#define DIFF_64(d_hi, m_hi, s_hi, d_lo, m_lo, s_lo) \
	do { \
		if (m_lo < s_lo) { \
			/* underflow */ \
			d_hi = m_hi - s_hi; \
			if (d_hi > 0) { \
				/* we can 'loan' 1 */ \
				d_hi--; \
				d_lo = m_lo + (UINT_MAX - s_lo) + 1; \
			} else { \
				/* m_hi <= s_hi */ \
				d_hi = 0; \
				d_lo = 0; \
			} \
		} else { \
			/* m_lo >= s_lo */ \
			if (m_hi < s_hi) { \
				d_hi = 0; \
				d_lo = 0; \
			} else { \
				/* m_hi >= s_hi */ \
				d_hi = m_hi - s_hi; \
				d_lo = m_lo - s_lo; \
			} \
		} \
	} while (0)

#define UPDATE_STAT64(s, t) \
	do { \
		DIFF_64(diff.hi, new->s##_hi, pstats->mac_stx[0].t##_hi, \
			diff.lo, new->s##_lo, pstats->mac_stx[0].t##_lo); \
		pstats->mac_stx[0].t##_hi = new->s##_hi; \
		pstats->mac_stx[0].t##_lo = new->s##_lo; \
		ADD_64(pstats->mac_stx[1].t##_hi, diff.hi, \
		       pstats->mac_stx[1].t##_lo, diff.lo); \
	} while (0)

#define UPDATE_STAT64_NIG(s, t) \
	do { \
		DIFF_64(diff.hi, new->s##_hi, old->s##_hi, \
			diff.lo, new->s##_lo, old->s##_lo); \
		ADD_64(estats->t##_hi, diff.hi, \
		       estats->t##_lo, diff.lo); \
	} while (0)

/* sum[hi:lo] += add */
#define ADD_EXTEND_64(s_hi, s_lo, a) \
	do { \
		s_lo += a; \
		s_hi += (s_lo < a) ? 1 : 0; \
	} while (0)

#define UPDATE_EXTEND_STAT(s) \
	do { \
		ADD_EXTEND_64(pstats->mac_stx[1].s##_hi, \
			      pstats->mac_stx[1].s##_lo, \
			      new->s); \
	} while (0)

#define UPDATE_EXTEND_TSTAT(s, t) \
	do { \
		diff = le32_to_cpu(tclient->s) - le32_to_cpu(old_tclient->s); \
		old_tclient->s = tclient->s; \
		ADD_EXTEND_64(qstats->t##_hi, qstats->t##_lo, diff); \
	} while (0)

#define UPDATE_EXTEND_USTAT(s, t) \
	do { \
		diff = le32_to_cpu(uclient->s) - le32_to_cpu(old_uclient->s); \
		old_uclient->s = uclient->s; \
		ADD_EXTEND_64(qstats->t##_hi, qstats->t##_lo, diff); \
	} while (0)

#define UPDATE_EXTEND_XSTAT(s, t) \
	do { \
		diff = le32_to_cpu(xclient->s) - le32_to_cpu(old_xclient->s); \
		old_xclient->s = xclient->s; \
		ADD_EXTEND_64(qstats->t##_hi, qstats->t##_lo, diff); \
	} while (0)

/* minuend -= subtrahend */
#define SUB_64(m_hi, s_hi, m_lo, s_lo) \
	do { \
		DIFF_64(m_hi, m_hi, s_hi, m_lo, m_lo, s_lo); \
	} while (0)

/* minuend[hi:lo] -= subtrahend */
#define SUB_EXTEND_64(m_hi, m_lo, s) \
	do { \
		SUB_64(m_hi, 0, m_lo, s); \
	} while (0)

#define SUB_EXTEND_USTAT(s, t) \
	do { \
		diff = le32_to_cpu(uclient->s) - le32_to_cpu(old_uclient->s); \
		SUB_EXTEND_64(qstats->t##_hi, qstats->t##_lo, diff); \
	} while (0)


enum bnx2x_stats_event {
	STATS_EVENT_PMF = 0,
	STATS_EVENT_LINK_UP,
	STATS_EVENT_UPDATE,
	STATS_EVENT_STOP,
	STATS_EVENT_MAX
};

enum bnx2x_stats_state {
	STATS_STATE_DISABLED = 0,
	STATS_STATE_ENABLED,
	STATS_STATE_MAX
};

struct bnx2x_eth_stats {
	u32 total_bytes_received_hi;
	u32 total_bytes_received_lo;
	u32 total_bytes_transmitted_hi;
	u32 total_bytes_transmitted_lo;
	u32 total_unicast_packets_received_hi;
	u32 total_unicast_packets_received_lo;
	u32 total_multicast_packets_received_hi;
	u32 total_multicast_packets_received_lo;
	u32 total_broadcast_packets_received_hi;
	u32 total_broadcast_packets_received_lo;
	u32 total_unicast_packets_transmitted_hi;
	u32 total_unicast_packets_transmitted_lo;
	u32 total_multicast_packets_transmitted_hi;
	u32 total_multicast_packets_transmitted_lo;
	u32 total_broadcast_packets_transmitted_hi;
	u32 total_broadcast_packets_transmitted_lo;
	u32 valid_bytes_received_hi;
	u32 valid_bytes_received_lo;

	u32 error_bytes_received_hi;
	u32 error_bytes_received_lo;
	u32 etherstatsoverrsizepkts_hi;
	u32 etherstatsoverrsizepkts_lo;
	u32 no_buff_discard_hi;
	u32 no_buff_discard_lo;

	u32 rx_stat_ifhcinbadoctets_hi;
	u32 rx_stat_ifhcinbadoctets_lo;
	u32 tx_stat_ifhcoutbadoctets_hi;
	u32 tx_stat_ifhcoutbadoctets_lo;
	u32 rx_stat_dot3statsfcserrors_hi;
	u32 rx_stat_dot3statsfcserrors_lo;
	u32 rx_stat_dot3statsalignmenterrors_hi;
	u32 rx_stat_dot3statsalignmenterrors_lo;
	u32 rx_stat_dot3statscarriersenseerrors_hi;
	u32 rx_stat_dot3statscarriersenseerrors_lo;
	u32 rx_stat_falsecarriererrors_hi;
	u32 rx_stat_falsecarriererrors_lo;
	u32 rx_stat_etherstatsundersizepkts_hi;
	u32 rx_stat_etherstatsundersizepkts_lo;
	u32 rx_stat_dot3statsframestoolong_hi;
	u32 rx_stat_dot3statsframestoolong_lo;
	u32 rx_stat_etherstatsfragments_hi;
	u32 rx_stat_etherstatsfragments_lo;
	u32 rx_stat_etherstatsjabbers_hi;
	u32 rx_stat_etherstatsjabbers_lo;
	u32 rx_stat_maccontrolframesreceived_hi;
	u32 rx_stat_maccontrolframesreceived_lo;
	u32 rx_stat_bmac_xpf_hi;
	u32 rx_stat_bmac_xpf_lo;
	u32 rx_stat_bmac_xcf_hi;
	u32 rx_stat_bmac_xcf_lo;
	u32 rx_stat_xoffstateentered_hi;
	u32 rx_stat_xoffstateentered_lo;
	u32 rx_stat_xonpauseframesreceived_hi;
	u32 rx_stat_xonpauseframesreceived_lo;
	u32 rx_stat_xoffpauseframesreceived_hi;
	u32 rx_stat_xoffpauseframesreceived_lo;
	u32 tx_stat_outxonsent_hi;
	u32 tx_stat_outxonsent_lo;
	u32 tx_stat_outxoffsent_hi;
	u32 tx_stat_outxoffsent_lo;
	u32 tx_stat_flowcontroldone_hi;
	u32 tx_stat_flowcontroldone_lo;
	u32 tx_stat_etherstatscollisions_hi;
	u32 tx_stat_etherstatscollisions_lo;
	u32 tx_stat_dot3statssinglecollisionframes_hi;
	u32 tx_stat_dot3statssinglecollisionframes_lo;
	u32 tx_stat_dot3statsmultiplecollisionframes_hi;
	u32 tx_stat_dot3statsmultiplecollisionframes_lo;
	u32 tx_stat_dot3statsdeferredtransmissions_hi;
	u32 tx_stat_dot3statsdeferredtransmissions_lo;
	u32 tx_stat_dot3statsexcessivecollisions_hi;
	u32 tx_stat_dot3statsexcessivecollisions_lo;
	u32 tx_stat_dot3statslatecollisions_hi;
	u32 tx_stat_dot3statslatecollisions_lo;
	u32 tx_stat_etherstatspkts64octets_hi;
	u32 tx_stat_etherstatspkts64octets_lo;
	u32 tx_stat_etherstatspkts65octetsto127octets_hi;
	u32 tx_stat_etherstatspkts65octetsto127octets_lo;
	u32 tx_stat_etherstatspkts128octetsto255octets_hi;
	u32 tx_stat_etherstatspkts128octetsto255octets_lo;
	u32 tx_stat_etherstatspkts256octetsto511octets_hi;
	u32 tx_stat_etherstatspkts256octetsto511octets_lo;
	u32 tx_stat_etherstatspkts512octetsto1023octets_hi;
	u32 tx_stat_etherstatspkts512octetsto1023octets_lo;
	u32 tx_stat_etherstatspkts1024octetsto1522octets_hi;
	u32 tx_stat_etherstatspkts1024octetsto1522octets_lo;
	u32 tx_stat_etherstatspktsover1522octets_hi;
	u32 tx_stat_etherstatspktsover1522octets_lo;
	u32 tx_stat_bmac_2047_hi;
	u32 tx_stat_bmac_2047_lo;
	u32 tx_stat_bmac_4095_hi;
	u32 tx_stat_bmac_4095_lo;
	u32 tx_stat_bmac_9216_hi;
	u32 tx_stat_bmac_9216_lo;
	u32 tx_stat_bmac_16383_hi;
	u32 tx_stat_bmac_16383_lo;
	u32 tx_stat_dot3statsinternalmactransmiterrors_hi;
	u32 tx_stat_dot3statsinternalmactransmiterrors_lo;
	u32 tx_stat_bmac_ufl_hi;
	u32 tx_stat_bmac_ufl_lo;

	u32 pause_frames_received_hi;
	u32 pause_frames_received_lo;
	u32 pause_frames_sent_hi;
	u32 pause_frames_sent_lo;

	u32 etherstatspkts1024octetsto1522octets_hi;
	u32 etherstatspkts1024octetsto1522octets_lo;
	u32 etherstatspktsover1522octets_hi;
	u32 etherstatspktsover1522octets_lo;

	u32 brb_drop_hi;
	u32 brb_drop_lo;
	u32 brb_truncate_hi;
	u32 brb_truncate_lo;

	u32 mac_filter_discard;
	u32 outer_vlan_discard;
	u32 brb_truncate_discard;
	u32 mac_discard;

	u32 driver_xoff;
	u32 rx_err_discard_pkt;
	u32 rx_skb_alloc_failed;
	u32 hw_csum_err;

	u32 nig_timer_max;
};

#define STATS_OFFSET32(stat_name) \
			(offsetof(struct bnx2x_eth_stats, stat_name) / 4)

/* slow path */

/* sriov */
#define BNX2X_MAX_NUM_OF_VFS	64
#define BNX2X_VF_CID_WND	0
#define BNX2X_CIDS_PER_VF	(1 << BNX2X_VF_CID_WND)
#define BNX2X_FIRST_VF_CID	64
#define BNX2X_VF_CIDS		(BNX2X_MAX_NUM_OF_VFS * BNX2X_CIDS_PER_VF)
#define BNX2X_VF_ID_INVALID	0xFF




/*
 * The total number of L2 queues, MSIX vectors and HW contexts (CIDs) is
 * control by the number of fast-path status blocks supported by the
 * device (HW/FW). Each fast-path status block (FP-SB) aka non-default
 * status block represents an independent interrupts context that can
 * serve a regular L2 networking queue. However special L2 queues such
 * as the FCoE queue do not require a FP-SB and other components like
 * the CNIC may consume FP-SB reducing the number of possible L2 queues
 *
 * If the maximum number of FP-SB available is X then:
 * a. If CNIC is supported it consumes 1 FP-SB thus the max number of
 *    regular L2 queues is Y=X-1
 * b. in MF mode the actual number of L2 queues is Y= (X-1/MF_factor)
 * c. If the FCoE L2 queue is supported the actual number of L2 queues
 *    is Y+1
 * d. The number of irqs (MSIX vectors) is either Y+1 (one extra for
 *    slow-path interrupts) or Y+2 if CNIC is supported (one additional
 *    FP interrupt context for the CNIC).
 * e. The number of HW context (CID count) is always X or X+1 if FCoE
 *    L2 queue is supported. the cid for the FCoE L2 queue is always X.
 */

/* fast-path interrupt contexts E1x */
#define FP_SB_MAX_E1x		16
/* fast-path interrupt contexts E2 */
#define FP_SB_MAX_E2		HC_SB_MAX_SB_E2

/*
 * cid_cnt paramter below refers to the value returned by
 * 'bnx2x_get_l2_cid_count()' routine
 */

/*
 * The number of FP context allocated by the driver == max number of regular
 * L2 queues + 1 for the FCoE L2 queue
 */
#define L2_FP_COUNT(cid_cnt)	((cid_cnt) - FCOE_CONTEXT_USE)

/*
 * The number of FP-SB allocated by the driver == max number of regular L2
 * queues + 1 for the CNIC which also consumes an FP-SB
 */
#define FP_SB_COUNT(cid_cnt)	((cid_cnt) - CNIC_CONTEXT_USE)
#define NUM_IGU_SB_REQUIRED(cid_cnt) \
				(FP_SB_COUNT(cid_cnt) - NONE_ETH_CONTEXT_USE)

union cdu_context {
	struct eth_context eth;
	char pad[1024];
};

/* CDU host DB constants */
#define CDU_ILT_PAGE_SZ_HW	3
#define CDU_ILT_PAGE_SZ		(4096 << CDU_ILT_PAGE_SZ_HW) /* 32K */
#define ILT_PAGE_CIDS		(CDU_ILT_PAGE_SZ / sizeof(union cdu_context))

#ifdef BCM_CNIC
#define CNIC_ISCSI_CID_MAX	256
#define CNIC_FCOE_CID_MAX	2048
#define CNIC_CID_MAX		(CNIC_ISCSI_CID_MAX + CNIC_FCOE_CID_MAX)
#define CNIC_ILT_LINES 		DIV_ROUND_UP(CNIC_CID_MAX, ILT_PAGE_CIDS)
#endif

#define QM_ILT_PAGE_SZ_HW	0
#define QM_ILT_PAGE_SZ		(4096 << QM_ILT_PAGE_SZ_HW) /* 4K */
#define QM_CID_ROUND		1024


#ifdef BCM_CNIC
/* TM (timers) host DB constants */
#define TM_ILT_PAGE_SZ_HW	0
#define TM_ILT_PAGE_SZ		(4096 << TM_ILT_PAGE_SZ_HW) /* 4K */
/* #define TM_CONN_NUM		(CNIC_STARTING_CID+CNIC_ISCSI_CXT_MAX) */
#define TM_CONN_NUM		1024
#define TM_ILT_SZ		(8 * TM_CONN_NUM)
#define TM_ILT_LINES 		DIV_ROUND_UP(TM_ILT_SZ, TM_ILT_PAGE_SZ)

/* SRC (Searcher) host DB constants */
#define SRC_ILT_PAGE_SZ_HW	0
#define SRC_ILT_PAGE_SZ		(4096 << SRC_ILT_PAGE_SZ_HW) /* 4K */
#define SRC_HASH_BITS		10
#define SRC_CONN_NUM		(1 << SRC_HASH_BITS) /* 1024 */
#define SRC_ILT_SZ		(sizeof(struct src_ent) * SRC_CONN_NUM)
#define SRC_T2_SZ		SRC_ILT_SZ
#define SRC_ILT_LINES 		DIV_ROUND_UP(SRC_ILT_SZ, SRC_ILT_PAGE_SZ)

#if (SRC_CONN_NUM < CNIC_ISCSI_CID_MAX)	/* ! BNX2X_UPSTREAM */
#error "Searcher tables are too small"
#endif
#endif

#define MAX_DMAE_C		8

/* DMA memory not used in fastpath */
struct bnx2x_slowpath {
	union {
		struct mac_configuration_cmd		e1x;
		struct eth_classify_rules_ramrod_data	e2;
	} mac_rdata;

	union {
		struct eth_classify_rules_ramrod_data	e2;
	} vlan_rdata;

	union {
		struct tstorm_eth_mac_filter_config	e1x;
		struct eth_filter_rules_ramrod_data	e2;
	} rx_mode_rdata;

	union {
		struct mac_configuration_cmd		e1;
		struct eth_multicast_rules_ramrod_data  e2;
	} mcast_rdata;

	union {
		struct eth_rss_update_ramrod_data_e1x	e1x;
		struct eth_rss_update_ramrod_data_e2	e2;
	} rss_rdata;

	/* Client State related ramrods are always sent under rtnl_lock */
	union {
		struct client_init_ramrod_data  init_data;
		struct client_update_ramrod_data update_data;
	} client_data;

	/* used for tx switching VM macs */
	struct eth_classify_rules_ramrod_data		tx_switch_mac_rdata;

	/* used by dmae command executer */
	struct dmae_command		dmae[MAX_DMAE_C];

	u32				stats_comp;
	union mac_stats			mac_stats;
	struct nig_stats		nig_stats;
	struct host_port_stats		port_stats;
	struct host_func_stats		func_stats;
	struct host_func_stats		func_stats_base;

	u32				wb_comp;
	u32				wb_data[4];
	/* pfc configuration for DCBX ramrod */
	struct flow_control_configuration pfc_config;
};

#define bnx2x_sp(bp, var)		(&bp->slowpath->var)
#define bnx2x_sp_mapping(bp, var) \
		(bp->slowpath_mapping + offsetof(struct bnx2x_slowpath, var))


/* attn group wiring */
#define MAX_DYNAMIC_ATTN_GRPS		8

struct attn_route {
	u32 sig[5];
};

struct iro {
	u32 base;
	u16 m1;
	u16 m2;
	u16 m3;
	u16 size;
};

struct hw_context {
	union cdu_context *vcxt;
	dma_addr_t cxt_mapping;
	size_t size;
#if defined (__VMKLNX__) /* ! BNX2X_UPSTREAM */
	u32 alignment_offset;
#endif
};

/* forward */
struct bnx2x_ilt;

#ifdef BCM_IOV /* ! BNX2X_UPSTREAM */
struct bnx2x_vfdb;
#endif

typedef enum {
	BNX2X_RECOVERY_DONE,
	BNX2X_RECOVERY_INIT,
	BNX2X_RECOVERY_WAIT,
} bnx2x_recovery_state_t;


/* Event queue (EQ or event ring) MC hsi
   NUM_EQ_PAGES and EQ_DESC_CNT_PAGE must powers of 2 */
#define NUM_EQ_PAGES		1
#define EQ_DESC_CNT_PAGE	(BCM_PAGE_SIZE / sizeof(union event_ring_elem))
#define EQ_DESC_MAX_PAGE	(EQ_DESC_CNT_PAGE - 1)
#define NUM_EQ_DESC		(EQ_DESC_CNT_PAGE * NUM_EQ_PAGES)
#define EQ_DESC_MASK		(NUM_EQ_DESC - 1)
#define MAX_EQ_AVAIL		(EQ_DESC_MAX_PAGE * NUM_EQ_PAGES - 2)

/* depends on EQ_DESC_CNT_PAGE being a power of 2 */
#define NEXT_EQ_IDX(x)		((((x) & EQ_DESC_MAX_PAGE) == \
				  (EQ_DESC_MAX_PAGE - 1)) ? (x) + 2 : (x) + 1)

/* depends on the above and on NUM_EQ_PAGES being a power of 2 */
#define EQ_DESC(x)		((x) & EQ_DESC_MASK)

#define BNX2X_EQ_INDEX \
	(&bp->def_status_blk->sp_sb.\
	index_values[HC_SP_INDEX_EQ_CONS])

/* This is a data that will be used to create a link report message.
 * We will keep the data used for the last link report in order
 * to prevent reporting the same link parameters twice.
 */
struct bnx2x_link_report_data {
	u16 line_speed;			/* Effective line speed */
	unsigned long link_report_flags;/* BNX2X_LINK_REPORT_XXX flags */
};

enum {
	BNX2X_LINK_REPORT_FD,		/* Full DUPLEX */
	BNX2X_LINK_REPORT_LINK_DOWN,
	BNX2X_LINK_REPORT_RX_FC_ON,
	BNX2X_LINK_REPORT_TX_FC_ON,
};

enum {
	BNX2X_PORT_QUERY_IDX,
	BNX2X_PF_QUERY_IDX,
	BNX2X_FIRST_QUEUE_QUERY_IDX,
};

struct bnx2x_fw_stats_req {
	struct stats_query_header hdr;
	struct stats_query_entry query[STATS_QUERY_CMD_COUNT];
};

struct bnx2x_fw_stats_data {
	struct stats_counter	storm_counters;
	struct per_port_stats	port;
	struct per_pf_stats	pf;
	struct per_queue_stats  queue_stats[1];
};

struct bnx2x {
	/* Fields used in the tx and intr/napi performance paths
	 * are grouped together in the beginning of the structure
	 */
	struct bnx2x_fastpath	*fp;
	void __iomem		*regview;
	void __iomem		*doorbells;
	u16                     db_size;

#define VF2PF_MBOX_SIZE         (sizeof(union vf_pf_msg) + \
				sizeof(union pf_vf_msg))
	/* General mail box plus aquire response buffer at the end */
	void                    *vf2pf_mbox;
	dma_addr_t              vf2pf_mbox_mapping;

	struct pf_vf_msg_acquire_resp *aquire_resp;

	struct net_device	*dev;
	struct pci_dev		*pdev;

	struct iro		*iro_arr;
#define IRO (bp->iro_arr)
	atomic_t		intr_sem;

	bnx2x_recovery_state_t	recovery_state;
	int 			is_leader;
	struct msix_entry	*msix_table;
#define INT_MODE_INTx			1
#define INT_MODE_MSI			2

	int			tx_ring_size;

#ifdef BCM_VLAN
	struct vlan_group	*vlgrp;
#endif

	u32			rx_csum;
	u32			rx_buf_size;
/* L2 header size + 2*VLANs (8 bytes) + LLC SNAP (8 bytes) */
#define ETH_OVREHEAD		(ETH_HLEN + 8 + 8)
#define ETH_MIN_PACKET_SIZE		60
#define ETH_MAX_PACKET_SIZE		1500
#define ETH_MAX_JUMBO_PACKET_SIZE	9600

	/* Max supported alignment is 256 (8 shift) */
#define BNX2X_RX_ALIGN_SHIFT		((L1_CACHE_SHIFT < 8) ? \
					 L1_CACHE_SHIFT : 8)
#define BNX2X_RX_ALIGN			(1 << BNX2X_RX_ALIGN_SHIFT)
#define BNX2X_PXP_DRAM_ALIGN		(BNX2X_RX_ALIGN_SHIFT - 5)

	struct host_sp_status_block *def_status_blk;
#define DEF_SB_IGU_ID			16
#define DEF_SB_ID			HC_SP_SB_ID
	__le16			def_idx;
	__le16			def_att_idx;
	u32			attn_state;
	struct attn_route	attn_group[MAX_DYNAMIC_ATTN_GRPS];

	/* slow path ring */
	struct eth_spe		*spq;
	dma_addr_t		spq_mapping;
	u16			spq_prod_idx;
	struct eth_spe		*spq_prod_bd;
	struct eth_spe		*spq_last_bd;
	__le16			*dsb_sp_prod;
	atomic_t		spq_left; /* serialize spq */
	/* used to synchronize spq accesses */
	spinlock_t		spq_lock;

	/* event queue */
	union event_ring_elem	*eq_ring;
	dma_addr_t		eq_mapping;
	u16			eq_prod;
	u16			eq_cons;
	__le16			*eq_cons_sb;


#ifdef BNX2X_NETQ /* ! BNX2X_UPSTREAM */
	/* n queues allocated */
	u16			n_rx_queues_allocated;
	u16			n_tx_queues_allocated;

	/* used to synchronize netq accesses */
	spinlock_t		netq_lock;
#endif

	/* Counter for marking that there is a STAT_QUERY ramrod pending */
	u16			stats_pending;
	/*  Counter for completed statistics ramrods */
	u16			stats_comp;

	/* End of fields used in the performance code paths */

	int			panic;
	int			msg_enable;

	u32			flags;
#define PCIX_FLAG			(1 << 0)
#define PCI_32BIT_FLAG			(1 << 1)
#define ONE_PORT_FLAG			(1 << 2)
#define NO_WOL_FLAG			(1 << 3)
#define USING_DAC_FLAG			(1 << 4)
#define USING_MSIX_FLAG			(1 << 5)
#define USING_MSI_FLAG			(1 << 6)
#define DISABLE_MSI_FLAG		(1 << 7)
#define TPA_ENABLE_FLAG			(1 << 8)
#define NO_MCP_FLAG		 	(1 << 9)

#define BP_NOMCP(bp)			(bp->flags & NO_MCP_FLAG)
#ifdef BNX2X_SAFC /* ! BNX2X_UPSTREAM */
#define SAFC_TX_FLAG			(1 << 10)
#endif
#define HW_VLAN_TX_FLAG			(1 << 11)
#define HW_VLAN_RX_FLAG			(1 << 12)
#define MF_FUNC_DIS			(1 << 13)
#define OWN_CNIC_IRQ			(1 << 14)
#define FCOE_MACS_SET			(1 << 15)
#define NO_ISCSI_OOO_FLAG	 	(1 << 16)
#define NO_FCOE_FLAG			(1 << 17)

#define NO_ISCSI_OOO(bp)	((bp)->flags & NO_ISCSI_OOO_FLAG)
#define NO_FCOE(bp)		((bp)->flags & NO_FCOE_FLAG)

	int			pf_num;	/* absolute PF number */
	int			pfid;	/* per-path PF number */
	int			base_fw_ndsb; /**/
#define BP_PATH(bp)			(!CHIP_IS_E2(bp) ? 0 : (bp->pf_num & 1))
#define BP_PORT(bp)			(bp->pfid & 1)
#define BP_FUNC(bp)			(bp->pfid)
#define BP_ABS_FUNC(bp)			(bp->pf_num)
#define BP_E1HVN(bp)			(bp->pfid >> 1)
#define BP_VN(bp)			(CHIP_MODE_IS_4_PORT(bp) ? 0:\
								   BP_E1HVN(bp))
#define BP_L_ID(bp)			(BP_E1HVN(bp) << 2)
#define BP_FW_MB_IDX(bp)		(BP_PORT(bp) +\
					 BP_VN(bp) * (CHIP_IS_E1x(bp) ? 2  : 1))

	int			pm_cap;
	int			pcie_cap;
	int			mrrs;

#if (LINUX_VERSION_CODE >= 0x020614) || (defined(__VMKLNX__)) /* BNX2X_UPSTREAM */
	struct delayed_work	sp_task;
#else
	struct work_struct	sp_task;
#endif
#if (LINUX_VERSION_CODE >= 0x020614) || (defined(__VMKLNX__)) /* BNX2X_UPSTREAM */
	struct delayed_work	reset_task;
#else
	struct work_struct	reset_task;
#endif
	struct timer_list	timer;
	int			current_interval;

	u16			fw_seq;
	u16			fw_drv_pulse_wr_seq;
	u32			func_stx;

	struct link_params	link_params;
	struct link_vars	link_vars;
	u32			link_cnt;
	struct bnx2x_link_report_data last_reported_link;

	struct mdio_if_info	mdio;

	struct bnx2x_common	common;
	struct bnx2x_port	port;

	struct cmng_struct_per_port cmng;
	u32			vn_weight_sum;
#ifdef BNX2X_SAFC /* ! BNX2X_UPSTREAM */
	u32			cos_weight_sum;
#endif
	u32			mf_config[E1HVN_MAX];
	u32			mf2_config[E2_FUNC_MAX];
	u16			mf_ov;
	u8			mf_mode;
#define IS_MF(bp)		(bp->mf_mode != 0)
#define IS_MF_SI(bp)		(bp->mf_mode == MULTI_FUNCTION_SI)
#define IS_MF_SD(bp)		(bp->mf_mode == MULTI_FUNCTION_SD)

	u8			wol;

	int			rx_ring_size;

	u16			tx_quick_cons_trip_int;
	u16			tx_quick_cons_trip;
	u16			tx_ticks_int;
	u16			tx_ticks;

	u16			rx_quick_cons_trip_int;
	u16			rx_quick_cons_trip;
	u16			rx_ticks_int;
	u16			rx_ticks;
/* Maximal coalescing timeout in us */
#define BNX2X_MAX_COALESCE_TOUT		(0xf0*12)

	u32			lin_cnt;

	u16			state;
#define BNX2X_STATE_CLOSED		0
#define BNX2X_STATE_OPENING_WAIT4_LOAD	0x1000
#define BNX2X_STATE_OPENING_WAIT4_PORT	0x2000
#define BNX2X_STATE_OPEN		0x3000
#define BNX2X_STATE_CLOSING_WAIT4_HALT	0x4000
#define BNX2X_STATE_CLOSING_WAIT4_DELETE 0x5000
#define BNX2X_STATE_CLOSING_WAIT4_UNLOAD 0x6000
#define BNX2X_STATE_FUNC_STARTED	0x7000

#define BNX2X_STATE_DIAG		0xe000
#define BNX2X_STATE_ERROR		0xf000

	int			multi_mode;
#ifdef BNX2X_SAFC /* ! BNX2X_UPSTREAM */
#define BNX2X_MAX_PRIORITY		8
#define BNX2X_MAX_ENTRIES_PER_PRI	16
#define BNX2X_MAX_COS			3
#define BNX2X_MAX_TX_COS		2
	/* priority to cos mapping */
	u8			pri_map[BNX2X_MAX_PRIORITY];
	/* number of queues per cos */
	u8			qs_per_cos[BNX2X_MAX_COS];
	/* min rate per cos */
	u16			cos_min_rate[BNX2X_MAX_COS];
	/* cos to queue mapping */
	u8			cos_map[BNX2X_MAX_COS];
#endif
	int			num_queues;
#ifdef BNX2X_SAFC /* ! BNX2X_UPSTREAM */
#define BNX2X_COS_QUEUES(cos)	((qs_per_cos & (0xff << cos*8)) >> cos*8)
#define BNX2X_COS_RATE(cos)	((cos_min_rate & (0xff << cos*8)) >> cos*8)
#endif

	u32			rx_mode;
#define BNX2X_RX_MODE_NONE		0
#define BNX2X_RX_MODE_NORMAL		1
#define BNX2X_RX_MODE_ALLMULTI		2
#define BNX2X_RX_MODE_PROMISC		3
#define BNX2X_MAX_MULTICAST		64
#define BNX2X_MAX_EMUL_MULTI		16

	u8			igu_dsb_id;
	u8			igu_base_sb;
	u8			igu_sb_cnt;
	dma_addr_t		def_status_blk_mapping;

	struct bnx2x_slowpath	*slowpath;
	dma_addr_t		slowpath_mapping;

	/* Total number of FW statistics requests */
	u8			fw_stats_num;

	/* This is a memory buffer that will contain both statistics
	 * ramrod request and data.
	 */
	void 			*fw_stats;
	dma_addr_t		fw_stats_mapping;

	/* FW statistics request shortcut (points at the
	 * beginning of fw_stats buffer).
	 */
	struct bnx2x_fw_stats_req	*fw_stats_req;
	dma_addr_t			fw_stats_req_mapping;
	int				fw_stats_req_sz;

	/* FW statistics data shortcut (points at the begining of
	 * fw_stats buffer + fw_stats_req_sz).
	 */
	struct bnx2x_fw_stats_data	*fw_stats_data;
	dma_addr_t			fw_stats_data_mapping;
	int				fw_stats_data_sz;

	struct hw_context	context;

	struct bnx2x_ilt 	*ilt;
#define BP_ILT(bp)		((bp)->ilt)
#define ILT_MAX_LINES		256

	int			l2_cid_count;
#define L2_ILT_LINES(bp) 	(DIV_ROUND_UP((bp)->l2_cid_count, \
				ILT_PAGE_CIDS))
#define BNX2X_DB_SIZE(bp)	((bp)->l2_cid_count * (1 << BNX2X_DB_SHIFT))

	int 			qm_cid_count;

	int			dropless_fc;

#ifdef BCM_CNIC
	u32			cnic_flags;
#define BNX2X_CNIC_FLAG_MAC_SET		1
	void			*t2;
	dma_addr_t		t2_mapping;
	struct cnic_ops		*cnic_ops;
	void			*cnic_data;
	u32			cnic_tag;
	struct cnic_eth_dev	cnic_eth_dev;
	union host_hc_status_block cnic_sb;
	dma_addr_t		cnic_sb_mapping;
	struct eth_spe		*cnic_kwq;
	struct eth_spe		*cnic_kwq_prod;
	struct eth_spe		*cnic_kwq_cons;
	struct eth_spe		*cnic_kwq_last;
	u16			cnic_kwq_pending;
	u16			cnic_spq_pending;
	u8			iscsi_mac[ETH_ALEN];
	u8			fip_mac[ETH_ALEN];
	struct mutex		cnic_mutex;
	struct bnx2x_vlan_mac_obj iscsi_l2_mac_obj;

	/* Start index of the "special" (CNIC related) L2 cleints */
	u8				cnic_base_cl_id;
#endif

	int			dmae_ready;
	/* used to synchronize dmae accesses */
	struct mutex		dmae_mutex;

	/* used to protect the FW mail box */
	struct mutex		fw_mb_mutex;

	/* used to synchronize stats collecting */
	int			stats_state;

	/* used for synchronization of concurrent threads statistics handling */
	spinlock_t		stats_lock;

	/* used by dmae command loader */
	struct dmae_command	stats_dmae;
	int			executer_idx;

	u16			stats_counter;
#if (LINUX_VERSION_CODE < 0x020618) && !defined(__VMKLNX__) /* ! BNX2X_UPSTREAM */
	struct net_device_stats	net_stats;
#endif
	struct bnx2x_eth_stats	eth_stats;

	struct z_stream_s	*strm;
	void			*gunzip_buf;
	dma_addr_t		gunzip_mapping;
	int			gunzip_outlen;
#define FW_BUF_SIZE			0x8000
#define GUNZIP_BUF(bp)			bp->gunzip_buf
#define GUNZIP_PHYS(bp)			bp->gunzip_mapping
#define GUNZIP_OUTLEN(bp)		bp->gunzip_outlen

#if (LINUX_VERSION_CODE < 0x02060b) /* ! BNX2X_UPSTREAM */
	u32			pci_state[16];
#endif
	struct raw_op		*init_ops;
	/* Init blocks offsets inside init_ops */
	u16			*init_ops_offsets;
	/* Data blob - has 32 bit granularity */
	u32			*init_data;
	/* Zipped PRAM blobs - raw data */
	const u8		*tsem_int_table_data;
	const u8		*tsem_pram_data;
	const u8		*usem_int_table_data;
	const u8		*usem_pram_data;
	const u8		*xsem_int_table_data;
	const u8		*xsem_pram_data;
	const u8		*csem_int_table_data;
	const u8		*csem_pram_data;
#define INIT_OPS(bp)			bp->init_ops
#define INIT_OPS_OFFSETS(bp)		bp->init_ops_offsets
#define INIT_DATA(bp)			bp->init_data
#define INIT_TSEM_INT_TABLE_DATA(bp)	bp->tsem_int_table_data
#define INIT_TSEM_PRAM_DATA(bp)		bp->tsem_pram_data
#define INIT_USEM_INT_TABLE_DATA(bp)	bp->usem_int_table_data
#define INIT_USEM_PRAM_DATA(bp)		bp->usem_pram_data
#define INIT_XSEM_INT_TABLE_DATA(bp)	bp->xsem_int_table_data
#define INIT_XSEM_PRAM_DATA(bp)		bp->xsem_pram_data
#define INIT_CSEM_INT_TABLE_DATA(bp)	bp->csem_int_table_data
#define INIT_CSEM_PRAM_DATA(bp)		bp->csem_pram_data

#define PHY_FW_VER_LEN			20
	char			fw_ver[32];
#if defined(BNX2X_UPSTREAM) && !defined(BNX2X_USE_INIT_VALUES) /* BNX2X_UPSTREAM */
	const struct firmware	*firmware;
#endif

#ifdef BCM_IOV /* ! BNX2X_UPSTREAM */
	struct bnx2x_vfdb	*vfdb;
#define IS_SRIOV(bp)		(bp)->vfdb
#endif
	/* LLDP params */
	struct bnx2x_config_lldp_params		lldp_config_params;

	/* DCBX params */
	struct bnx2x_config_dcbx_params		dcbx_config_params;

	struct bnx2x_dcbx_port_params	dcbx_port_params;
	int				dcb_version;

	/* CAM credit pools */
	struct bnx2x_credit_pool_obj	vlans_pool;
	struct bnx2x_credit_pool_obj	macs_pool;

	/* RX_MODE object */
	struct bnx2x_rx_mode_obj	rx_mode_obj;

	/* MCAST object */
	struct bnx2x_mcast_obj		mcast_obj;

	/* RSS configuration object */
	struct bnx2x_rss_config_obj     rss_conf_obj;

	unsigned long			sp_state;
};

/**
	Init queue/func interface
**/
/* queue init flags */
enum {
	BNX2X_QUEUE_FLG_TPA,
	BNX2X_QUEUE_FLG_STATS,
	BNX2X_QUEUE_FLG_ZERO_STATS,
	BNX2X_QUEUE_FLG_OV,
	BNX2X_QUEUE_FLG_VLAN,
	BNX2X_QUEUE_FLG_COS,
	BNX2X_QUEUE_FLG_HC,
	BNX2X_QUEUE_FLG_DHC,
	BNX2X_QUEUE_FLG_OOO,
	BNX2X_QUEUE_FLG_TX_SWITCH,
	BNX2X_QUEUE_FLG_TX_SEC,
};









#define QUEUE_DROP_IP_CS_ERR	TSTORM_ETH_CLIENT_CONFIG_DROP_IP_CS_ERR
#define QUEUE_DROP_TCP_CS_ERR	TSTORM_ETH_CLIENT_CONFIG_DROP_TCP_CS_ERR
#define QUEUE_DROP_TTL0		TSTORM_ETH_CLIENT_CONFIG_DROP_TTL0
#define QUEUE_DROP_UDP_CS_ERR	TSTORM_ETH_CLIENT_CONFIG_DROP_UDP_CS_ERR



/* rss capabilities */
#define RSS_IPV4_CAP    	0x0001
#define RSS_IPV4_TCP_CAP	0x0002
#define RSS_IPV6_CAP    	0x0004
#define RSS_IPV6_TCP_CAP	0x0008


/* Tx queues may be less or equal to Rx queues */
extern int num_queues;
#define BNX2X_NUM_QUEUES(bp)	 bp->num_queues
#define BNX2X_NUM_ETH_QUEUES(bp) (BNX2X_NUM_QUEUES(bp) - NONE_ETH_CONTEXT_USE)

/* ethtool statistics are displayed for all regualr ethernet queues and the
 * fcoe L2 queue if not disabled
 */
#define BNX2X_NUM_STAT_QUEUES(bp) (NO_FCOE(bp) ? BNX2X_NUM_ETH_QUEUES(bp) : \
				  (BNX2X_NUM_ETH_QUEUES(bp) + FCOE_CONTEXT_USE))
#define is_multi(bp)		  (BNX2X_NUM_QUEUES(bp) > 1)

#ifdef BNX2X_NETQ /* ! BNX2X_UPSTREAM */

/*
 * limit the total number of queues to 4 (default + 3 NetQueues) in SF and
 * 2 (default + 1 NetQueue) in MF. If num_queue is provided then we're in
 * engineering mode and the max is set the regular way.
 *
 * Workaround for PR347954 for inbox bnx2x driver.
 * Disable NetQueue when Flex10 module is connected for inbox bnx2x driver
 */
#if defined(BNX2X_INBOX)
#define BNX2X_MAX_QUEUES_E1HMF  1
#else
#define BNX2X_MAX_QUEUES_E1HMF  2
#endif
#define BNX2X_MAX_QUEUES_E1H    8
#define BNX2X_MAX_QUEUES_E1     4

#define BNX2X_MAX_ESX_QUEUES(bp) (IS_MF(bp) ? BNX2X_MAX_QUEUES_E1HMF : \
				 (CHIP_IS_E1(bp) ? BNX2X_MAX_QUEUES_E1 : \
				  BNX2X_MAX_QUEUES_E1H))
#define BNX2X_MAX_QUEUES(bp) \
	((num_queues == 0) ? \
	min_t(int, bp->igu_sb_cnt - CNIC_CONTEXT_USE, \
			BNX2X_MAX_ESX_QUEUES(bp)) : \
	(bp->igu_sb_cnt - CNIC_CONTEXT_USE))

/*
 *the total numver of net queues is the number of regualr L2 queues -1 for
 * the default.
 */
#define BNX2X_NUM_NETQUEUES(bp)	(BNX2X_NUM_ETH_QUEUES((bp)) - 1)

#else
#define BNX2X_MAX_QUEUES(bp)	(bp->igu_sb_cnt - CNIC_CONTEXT_USE)
#endif

#define RSS_IPV4_CAP_MASK						\
	TSTORM_ETH_FUNCTION_COMMON_CONFIG_RSS_IPV4_CAPABILITY

#define RSS_IPV4_TCP_CAP_MASK						\
	TSTORM_ETH_FUNCTION_COMMON_CONFIG_RSS_IPV4_TCP_CAPABILITY

#define RSS_IPV6_CAP_MASK						\
	TSTORM_ETH_FUNCTION_COMMON_CONFIG_RSS_IPV6_CAPABILITY

#define RSS_IPV6_TCP_CAP_MASK						\
	TSTORM_ETH_FUNCTION_COMMON_CONFIG_RSS_IPV6_TCP_CAPABILITY

/* func init flags */
#define FUNC_FLG_RSS		0x0001
#define FUNC_FLG_STATS		0x0002
/* removed  FUNC_FLG_UNMATCHED	0x0004 */
#define FUNC_FLG_TPA		0x0008
#define FUNC_FLG_SPQ		0x0010

struct rxq_pause_params {
	u16     	bd_th_lo;
	u16     	bd_th_hi;
	u16     	rcq_th_lo;
	u16     	rcq_th_hi;
	u16     	sge_th_lo; /* valid iff BNX2X_QUEUE_FLG_TPA */
	u16     	sge_th_hi; /* valid iff BNX2X_QUEUE_FLG_TPA */
	u16     	pri_map;
};

struct bnx2x_rxq_init_params {
	/* cxt*/
	struct eth_context *cxt;

	/* dma */
	dma_addr_t	dscr_map;
	dma_addr_t	sge_map;
	dma_addr_t	rcq_map;
	dma_addr_t	rcq_np_map;

	unsigned long	flags;
	u16		drop_flags;
	u16		mtu;
	u16		buf_sz;
	u16		fw_sb_id;
	u16		cl_id;
	u16		spcl_id;
	u16		cl_qzone_id;

	/* valid iff BNX2X_QUEUE_FLG_STATS */
	u16		stat_id;

	/* valid iff BNX2X_QUEUE_FLG_TPA */
	u16		tpa_agg_sz;
	u16		sge_buf_sz;
	u16		max_sges_pkt;
	u8		max_tpa_queues;

	u8		cache_line_log;

	u8		sb_cq_index;
	u32		cid;

	/* desired interrupts per sec. valid iff BNX2X_QUEUE_FLG_HC */
	u32		hc_rate;

	u8		rss_mode;
};

struct bnx2x_txq_init_params {
	/* cxt*/
	struct eth_context *cxt;

	/* dma */
	dma_addr_t	dscr_map;

	unsigned long	flags;
	u16		fw_sb_id;
	u8		sb_cq_index;
	u8		cos;		/* valid iff BNX2X_QUEUE_FLG_COS */
	u32		cid;
	u16		traffic_type;
	u16		hc_rate;	/* desired interrupts per sec.
					 * valid iff BNX2X_QUEUE_FLG_HC
					 */
	/* equals to the leading rss client id, used for TX classification*/
	u8		tss_leading_cl_id;
};

enum {
	CLIENT_IS_FCOE,
	CLIENT_IS_LEADING_RSS,
	CLIENT_IS_FWD,
	CLIENT_IS_OOO,
	CLIENT_IS_MULTICAST,
};

struct bnx2x_client_ramrod_params {
	u16 *pstate;
	u16 state;
	u16 index;
	u8 cl_id;
	u32 cid;
	unsigned long client_flags;
};

struct bnx2x_client_init_params {
	struct rxq_pause_params pause;
	struct bnx2x_rxq_init_params rxq_params;
	struct bnx2x_txq_init_params txq_params;
	struct bnx2x_client_ramrod_params ramrod_params;
};

/* TODO: Reconsider if we still need this struct */
struct bnx2x_rss_params {
	int 	mode;
	u16	cap;
	u16	result_mask;
};

struct bnx2x_func_init_params {

	/* rss */
	struct bnx2x_rss_params *rss;	/* valid iff FUNC_FLG_RSS */

	/* dma */
	dma_addr_t	fw_stat_map;	/* valid iff FUNC_FLG_STATS */
	dma_addr_t	spq_map;	/* valid iff FUNC_FLG_SPQ */

	u16		func_flgs;
	u16		func_id;	/* abs fid */
	u16		pf_id;
	u16		spq_prod;	/* valid iff FUNC_FLG_SPQ */
};

#define for_each_eth_queue(bp, var) \
			for (var = 0; var < BNX2X_NUM_ETH_QUEUES(bp); var++)

#define for_each_nondefault_eth_queue(bp, var) \
			for (var = 1; var < BNX2X_NUM_ETH_QUEUES(bp); var++)

#define for_each_napi_queue(bp, var) \
			for (var = 0; \
		var < BNX2X_NUM_ETH_QUEUES(bp) + FCOE_CONTEXT_USE; var++) \
		if (skip_queue(bp, var))	\
			continue;		\
		else

#define for_each_queue(bp, var) \
			for (var = 0; var < BNX2X_NUM_QUEUES(bp); var++) \
		if (skip_queue(bp, var))	\
			continue;		\
		else

/* Skip forwarding FP */
#define for_each_rx_queue(bp, var) \
			for (var = 0; var < BNX2X_NUM_QUEUES(bp); var++) \
		if (skip_rx_queue(bp, var))	\
			continue;		\
		else

/* Skip OOO FP */
#define for_each_tx_queue(bp, var) \
			for (var = 0; var < BNX2X_NUM_QUEUES(bp); var++) \
		if (skip_tx_queue(bp, var))	\
			continue;		\
		else

#define for_each_nondefault_queue(bp, var) \
			for (var = 1; var < BNX2X_NUM_QUEUES(bp); var++) \
		if (skip_queue(bp, var))	\
			continue;		\
		else

/* Skip rx queue if any of the following is TRUE:
 *   - iscsi-ooo support is disabled and this is the rx ooo queue OR
 *   - FCOE l2 support is diabled and this is the fcoe L2 queue OR
 *   - idx is an index of the Forwarding Client fp in the bp->fp[] array.
 */
#define skip_rx_queue(bp, idx)	((NO_ISCSI_OOO(bp) && IS_OOO_IDX(idx)) || \
				 (NO_FCOE(bp) &&  IS_FCOE_IDX(idx)) || \
				 IS_FWD_IDX(idx))

/* Skip tx queue if any of the following is TRUE:
 *   - iscsi-ooo support is disabled and this is the tx fwd queue OR
 *   - FCOE l2 support is diabled and this is the fcoe L2 queue OR
 *   - idx is OOO Rx L2 `fastpath' entry.
 */
#define skip_tx_queue(bp, idx)	((NO_ISCSI_OOO(bp) && IS_FWD_IDX(idx)) || \
				 (NO_FCOE(bp) &&  IS_FCOE_IDX(idx)) || \
				 IS_OOO_IDX(idx))

#define skip_queue(bp, idx) ((NO_FCOE(bp) &&  IS_FCOE_IDX(idx)) || \
		(NO_ISCSI_OOO(bp) && (IS_OOO_IDX(idx) || IS_FWD_IDX(idx))))


#ifdef BNX2X_NETQ /* ! BNX2X_UPSTREAM */
#define for_each_net_queue(bp, var) \
			for (var = 1; var <= BNX2X_NUM_NETQUEUES(bp); var++)
#endif

/**
	Init queue/func API
**/
int bnx2x_set_mac_one(struct bnx2x *bp, u8 *mac,
		      struct bnx2x_vlan_mac_obj *obj, bool set,
		      int mac_type, unsigned long ramrod_flags);

int bnx2x_set_vlan_one(struct bnx2x *bp, u16 vtag,
		       struct bnx2x_vlan_mac_obj *obj, bool set,
		       unsigned long ramrod_flags, bool consume_credit);

void bnx2x_func_init(struct bnx2x *bp, struct bnx2x_func_init_params *p);
int bnx2x_setup_fw_client(struct bnx2x *bp,
			  struct bnx2x_client_init_params *params,
			  u8 activate,
			  struct client_init_ramrod_data *data,
			  dma_addr_t data_mapping);
void bnx2x_init_sb(struct bnx2x *bp, dma_addr_t mapping, int vfid,
			  u8 vf_valid, int fw_sb_id, int igu_sb_id);

int bnx2x_wait_ramrod(struct bnx2x *bp, u16 state, int idx,
			     u16 *state_p, int poll);

u32 bnx2x_get_pretend_reg(struct bnx2x *bp);
int bnx2x_get_gpio(struct bnx2x *bp, int gpio_num, u8 port);
int bnx2x_set_gpio(struct bnx2x *bp, int gpio_num, u32 mode, u8 port);
int bnx2x_set_gpio_int(struct bnx2x *bp, int gpio_num, u32 mode, u8 port);
void bnx2x_read_mf_cfg(struct bnx2x *bp);
void bnx2x_reg_wr_ind(struct bnx2x *bp, u32 addr, u32 val);


/* dmae */
void bnx2x_read_dmae(struct bnx2x *bp, u32 src_addr, u32 len32);
void bnx2x_write_dmae(struct bnx2x *bp, dma_addr_t dma_addr, u32 dst_addr,
		      u32 len32);
void bnx2x_write_dmae_phys_len(struct bnx2x *bp, dma_addr_t phys_addr,
			       u32 addr, u32 len);
void bnx2x_dp_dmae(struct bnx2x *bp, struct dmae_command *dmae, int msglvl);
void bnx2x_prep_dmae_with_comp(struct bnx2x *bp, struct dmae_command *dmae,
			       u8 src_type, u8 dst_type);
int bnx2x_issue_dmae_with_comp(struct bnx2x *bp, struct dmae_command *dmae);


/* FLR related routines */
u32 bnx2x_flr_clnup_poll_count(struct bnx2x* bp);
void bnx2x_tx_hw_flushed(struct bnx2x *bp, u32 poll_count);
int bnx2x_send_final_clnup(struct bnx2x* bp, u8 clnup_func, u32 poll_cnt);
u8 bnx2x_is_pcie_pending(struct pci_dev *dev);
int bnx2x_flr_clnup_poll_hw_counter(struct bnx2x* bp, u32 reg,
				    char* msg, u32 poll_cnt);

int bnx2x_sp_post(struct bnx2x *bp, int command, int cid,
		  u32 data_hi, u32 data_lo, int common);

static inline u32 reg_poll(struct bnx2x *bp, u32 reg, u32 expected, int ms,
			   int wait)
{
	u32 val;

	do {
		val = REG_RD(bp, reg);
		if (val == expected)
			break;
		ms -= wait;
		msleep(wait);

	} while (ms > 0);

	return val;
}

#define BNX2X_ILT_ZALLOC(x, y, size) \
	do { \
		x = pci_alloc_consistent(bp->pdev, size, y); \
		if (x) \
			memset(x, 0, size); \
	} while (0)

#define BNX2X_ILT_FREE(x, y, size) \
	do { \
		if (x) { \
			pci_free_consistent(bp->pdev, size, x, y); \
			x = NULL; \
			y = 0; \
		} \
	} while (0)

#define ILOG2(x)	(ilog2((x)))

#define ILT_NUM_PAGE_ENTRIES	(3072)
/* In 57710/11 we use whole table since we have 8 func
 * In 57712 we have only 4 func, but use same size per func, then only half of
 * the table in use
 */
#define ILT_PER_FUNC		(ILT_NUM_PAGE_ENTRIES/8)

#define FUNC_ILT_BASE(func)	(func * ILT_PER_FUNC)
/*
 * the phys address is shifted right 12 bits and has an added
 * 1=valid bit added to the 53rd bit
 * then since this is a wide register(TM)
 * we split it into two 32 bit writes
 */
#define ONCHIP_ADDR1(x)		((u32)(((u64)x >> 12) & 0xFFFFFFFF))
#define ONCHIP_ADDR2(x)		((u32)((1 << 20) | ((u64)x >> 44)))

/* load/unload mode */
#define LOAD_NORMAL			0
#define LOAD_OPEN			1
#define LOAD_DIAG			2
#define UNLOAD_NORMAL			0
#define UNLOAD_CLOSE			1
#define UNLOAD_RECOVERY                 2


/* DMAE command defines */
#define DMAE_TIMEOUT			-1
#define DMAE_PCI_ERROR			-2	/* E2 and onward */
#define DMAE_NOT_RDY			-3
#define DMAE_PCI_ERR_FLAG		0x80000000

#define DMAE_SRC_PCI			0
#define DMAE_SRC_GRC			1

#define DMAE_DST_NONE			0
#define DMAE_DST_PCI			1
#define DMAE_DST_GRC			2

#define DMAE_COMP_PCI			0
#define DMAE_COMP_GRC			1

/* E2 and onward - PCI error handling in the
 completion */
#define DMAE_COMP_REGULAR		0
#define DMAE_COM_SET_ERR		1

#define DMAE_CMD_SRC_PCI		(DMAE_SRC_PCI << \
						DMAE_COMMAND_SRC_SHIFT)
#define DMAE_CMD_SRC_GRC		(DMAE_SRC_GRC << \
						DMAE_COMMAND_SRC_SHIFT)

#define DMAE_CMD_DST_PCI		(DMAE_DST_PCI << \
						DMAE_COMMAND_DST_SHIFT)
#define DMAE_CMD_DST_GRC		(DMAE_DST_GRC << \
						DMAE_COMMAND_DST_SHIFT)

#define DMAE_CMD_C_DST_PCI		(DMAE_COMP_PCI << \
						DMAE_COMMAND_C_DST_SHIFT)
#define DMAE_CMD_C_DST_GRC		(DMAE_COMP_GRC << \
						DMAE_COMMAND_C_DST_SHIFT)

#define DMAE_CMD_C_ENABLE		DMAE_COMMAND_C_TYPE_ENABLE

#define DMAE_CMD_PORT_0			0
#define DMAE_CMD_PORT_1			DMAE_COMMAND_PORT

#define DMAE_CMD_SRC_RESET		DMAE_COMMAND_SRC_RESET
#define DMAE_CMD_DST_RESET		DMAE_COMMAND_DST_RESET
#define DMAE_CMD_E1HVN_SHIFT		DMAE_COMMAND_E1HVN_SHIFT

#define DMAE_SRC_PF			0
#define DMAE_SRC_VF			1

#define DMAE_DST_PF			0
#define DMAE_DST_VF			1

#define DMAE_C_SRC			0
#define DMAE_C_DST			1

#define DMAE_LEN32_RD_MAX		0x80
#define DMAE_LEN32_WR_MAX(bp)		(CHIP_IS_E1(bp) ? 0x400 : 0x2000)

#define DMAE_COMP_VAL			0x60d0d0ae /* E2 and on - upper bit
							indicates eror */

#define MAX_DMAE_C_PER_PORT		8
#define INIT_DMAE_C(bp)			(BP_PORT(bp) * MAX_DMAE_C_PER_PORT + \
						BP_E1HVN(bp))
#define PMF_DMAE_C(bp)			(BP_PORT(bp) * MAX_DMAE_C_PER_PORT + \
						 E1HVN_MAX)

#define DMAE_CMD_ENDIANITY_NO_SWAP	(0 << DMAE_COMMAND_ENDIANITY_SHIFT)
#define DMAE_CMD_ENDIANITY_B_SWAP	(1 << DMAE_COMMAND_ENDIANITY_SHIFT)
#define DMAE_CMD_ENDIANITY_DW_SWAP	(2 << DMAE_COMMAND_ENDIANITY_SHIFT)
#define DMAE_CMD_ENDIANITY_B_DW_SWAP	(3 << DMAE_COMMAND_ENDIANITY_SHIFT)


/* PCIE link and speed */
#define PCICFG_LINK_WIDTH		0x1f00000
#define PCICFG_LINK_WIDTH_SHIFT		20
#define PCICFG_LINK_SPEED		0xf0000
#define PCICFG_LINK_SPEED_SHIFT		16


#define BNX2X_NUM_TESTS			7

#define BNX2X_PHY_LOOPBACK		0
#define BNX2X_MAC_LOOPBACK		1
#define BNX2X_PHY_LOOPBACK_FAILED	1
#define BNX2X_MAC_LOOPBACK_FAILED	2
#define BNX2X_LOOPBACK_FAILED		(BNX2X_MAC_LOOPBACK_FAILED | \
					 BNX2X_PHY_LOOPBACK_FAILED)


#define STROM_ASSERT_ARRAY_SIZE		50


/* must be used on a CID before placing it on a HW ring */
#define HW_CID(bp, x)			((BP_PORT(bp) << 23) | \
					 (BP_E1HVN(bp) << BNX2X_SWCID_SHIFT) | \
					 (x))

#define SP_DESC_CNT		(BCM_PAGE_SIZE / sizeof(struct eth_spe))
#define MAX_SP_DESC_CNT			(SP_DESC_CNT - 1)


#define BNX2X_BTR			4
#define MAX_SPQ_PENDING			8


/* CMNG constants
   derived from lab experiments, and not from system spec calculations !!! */
#define DEF_MIN_RATE			100
/* resolution of the rate shaping timer - 100 usec */
#define RS_PERIODIC_TIMEOUT_USEC	100
/* resolution of fairness algorithm in usecs -
   coefficient for calculating the actual t fair */
#define T_FAIR_COEF			10000000
/* number of bytes in single QM arbitration cycle -
   coefficient for calculating the fairness timer */
#define QM_ARB_BYTES			40000
#define FAIR_MEM			2


#define ATTN_NIG_FOR_FUNC		(1L << 8)
#define ATTN_SW_TIMER_4_FUNC		(1L << 9)
#define GPIO_2_FUNC			(1L << 10)
#define GPIO_3_FUNC			(1L << 11)
#define GPIO_4_FUNC			(1L << 12)
#define ATTN_GENERAL_ATTN_1		(1L << 13)
#define ATTN_GENERAL_ATTN_2		(1L << 14)
#define ATTN_GENERAL_ATTN_3		(1L << 15)
#define ATTN_GENERAL_ATTN_4		(1L << 13)
#define ATTN_GENERAL_ATTN_5		(1L << 14)
#define ATTN_GENERAL_ATTN_6		(1L << 15)

#define ATTN_HARD_WIRED_MASK		0xff00
#define ATTENTION_ID			4


/* stuff added to make the code fit 80Col */

#define BNX2X_PMF_LINK_ASSERT \
	GENERAL_ATTEN_OFFSET(LINK_SYNC_ATTENTION_BIT_FUNC_0 + BP_FUNC(bp))

#define BNX2X_MC_ASSERT_BITS \
	(GENERAL_ATTEN_OFFSET(TSTORM_FATAL_ASSERT_ATTENTION_BIT) | \
	 GENERAL_ATTEN_OFFSET(USTORM_FATAL_ASSERT_ATTENTION_BIT) | \
	 GENERAL_ATTEN_OFFSET(CSTORM_FATAL_ASSERT_ATTENTION_BIT) | \
	 GENERAL_ATTEN_OFFSET(XSTORM_FATAL_ASSERT_ATTENTION_BIT))

#define BNX2X_MCP_ASSERT \
	GENERAL_ATTEN_OFFSET(MCP_FATAL_ASSERT_ATTENTION_BIT)

#define BNX2X_GRC_TIMEOUT	GENERAL_ATTEN_OFFSET(LATCHED_ATTN_TIMEOUT_GRC)
#define BNX2X_GRC_RSV		(GENERAL_ATTEN_OFFSET(LATCHED_ATTN_RBCR) | \
				 GENERAL_ATTEN_OFFSET(LATCHED_ATTN_RBCT) | \
				 GENERAL_ATTEN_OFFSET(LATCHED_ATTN_RBCN) | \
				 GENERAL_ATTEN_OFFSET(LATCHED_ATTN_RBCU) | \
				 GENERAL_ATTEN_OFFSET(LATCHED_ATTN_RBCP) | \
				 GENERAL_ATTEN_OFFSET(LATCHED_ATTN_RSVD_GRC))

#define HW_INTERRUT_ASSERT_SET_0 \
				(AEU_INPUTS_ATTN_BITS_TSDM_HW_INTERRUPT | \
				 AEU_INPUTS_ATTN_BITS_TCM_HW_INTERRUPT | \
				 AEU_INPUTS_ATTN_BITS_TSEMI_HW_INTERRUPT | \
				 AEU_INPUTS_ATTN_BITS_PBF_HW_INTERRUPT)
#define HW_PRTY_ASSERT_SET_0	(AEU_INPUTS_ATTN_BITS_BRB_PARITY_ERROR | \
				 AEU_INPUTS_ATTN_BITS_PARSER_PARITY_ERROR | \
				 AEU_INPUTS_ATTN_BITS_TSDM_PARITY_ERROR | \
				 AEU_INPUTS_ATTN_BITS_SEARCHER_PARITY_ERROR |\
				 AEU_INPUTS_ATTN_BITS_TSEMI_PARITY_ERROR)
#define HW_INTERRUT_ASSERT_SET_1 \
				(AEU_INPUTS_ATTN_BITS_QM_HW_INTERRUPT | \
				 AEU_INPUTS_ATTN_BITS_TIMERS_HW_INTERRUPT | \
				 AEU_INPUTS_ATTN_BITS_XSDM_HW_INTERRUPT | \
				 AEU_INPUTS_ATTN_BITS_XCM_HW_INTERRUPT | \
				 AEU_INPUTS_ATTN_BITS_XSEMI_HW_INTERRUPT | \
				 AEU_INPUTS_ATTN_BITS_USDM_HW_INTERRUPT | \
				 AEU_INPUTS_ATTN_BITS_UCM_HW_INTERRUPT | \
				 AEU_INPUTS_ATTN_BITS_USEMI_HW_INTERRUPT | \
				 AEU_INPUTS_ATTN_BITS_UPB_HW_INTERRUPT | \
				 AEU_INPUTS_ATTN_BITS_CSDM_HW_INTERRUPT | \
				 AEU_INPUTS_ATTN_BITS_CCM_HW_INTERRUPT)
#define HW_PRTY_ASSERT_SET_1	(AEU_INPUTS_ATTN_BITS_PBCLIENT_PARITY_ERROR |\
				 AEU_INPUTS_ATTN_BITS_QM_PARITY_ERROR | \
				 AEU_INPUTS_ATTN_BITS_XSDM_PARITY_ERROR | \
				 AEU_INPUTS_ATTN_BITS_XSEMI_PARITY_ERROR | \
				 AEU_INPUTS_ATTN_BITS_DOORBELLQ_PARITY_ERROR |\
			     AEU_INPUTS_ATTN_BITS_VAUX_PCI_CORE_PARITY_ERROR |\
				 AEU_INPUTS_ATTN_BITS_DEBUG_PARITY_ERROR | \
				 AEU_INPUTS_ATTN_BITS_USDM_PARITY_ERROR | \
				 AEU_INPUTS_ATTN_BITS_USEMI_PARITY_ERROR | \
				 AEU_INPUTS_ATTN_BITS_UPB_PARITY_ERROR | \
				 AEU_INPUTS_ATTN_BITS_CSDM_PARITY_ERROR)
#define HW_INTERRUT_ASSERT_SET_2 \
				(AEU_INPUTS_ATTN_BITS_CSEMI_HW_INTERRUPT | \
				 AEU_INPUTS_ATTN_BITS_CDU_HW_INTERRUPT | \
				 AEU_INPUTS_ATTN_BITS_DMAE_HW_INTERRUPT | \
			AEU_INPUTS_ATTN_BITS_PXPPCICLOCKCLIENT_HW_INTERRUPT |\
				 AEU_INPUTS_ATTN_BITS_MISC_HW_INTERRUPT)
#define HW_PRTY_ASSERT_SET_2	(AEU_INPUTS_ATTN_BITS_CSEMI_PARITY_ERROR | \
				 AEU_INPUTS_ATTN_BITS_PXP_PARITY_ERROR | \
			AEU_INPUTS_ATTN_BITS_PXPPCICLOCKCLIENT_PARITY_ERROR |\
				 AEU_INPUTS_ATTN_BITS_CFC_PARITY_ERROR | \
				 AEU_INPUTS_ATTN_BITS_CDU_PARITY_ERROR | \
				 AEU_INPUTS_ATTN_BITS_IGU_PARITY_ERROR | \
				 AEU_INPUTS_ATTN_BITS_MISC_PARITY_ERROR)

#define HW_PRTY_ASSERT_SET_3 (AEU_INPUTS_ATTN_BITS_MCP_LATCHED_ROM_PARITY | \
		AEU_INPUTS_ATTN_BITS_MCP_LATCHED_UMP_RX_PARITY | \
		AEU_INPUTS_ATTN_BITS_MCP_LATCHED_UMP_TX_PARITY | \
		AEU_INPUTS_ATTN_BITS_MCP_LATCHED_SCPAD_PARITY)

#define RSS_FLAGS(bp) \
		(TSTORM_ETH_FUNCTION_COMMON_CONFIG_RSS_IPV4_CAPABILITY | \
		 TSTORM_ETH_FUNCTION_COMMON_CONFIG_RSS_IPV4_TCP_CAPABILITY | \
		 TSTORM_ETH_FUNCTION_COMMON_CONFIG_RSS_IPV6_CAPABILITY | \
		 TSTORM_ETH_FUNCTION_COMMON_CONFIG_RSS_IPV6_TCP_CAPABILITY | \
		 (bp->multi_mode << \
		  TSTORM_ETH_FUNCTION_COMMON_CONFIG_RSS_MODE_SHIFT))
#define MULTI_MASK			0x7f


#define DEF_USB_FUNC_OFF	offsetof(struct cstorm_def_status_block_u, func)
#define DEF_CSB_FUNC_OFF	offsetof(struct cstorm_def_status_block_c, func)
#define DEF_XSB_FUNC_OFF	offsetof(struct xstorm_def_status_block, func)
#define DEF_TSB_FUNC_OFF	offsetof(struct tstorm_def_status_block, func)

#define DEF_USB_IGU_INDEX_OFF \
			offsetof(struct cstorm_def_status_block_u, igu_index)
#define DEF_CSB_IGU_INDEX_OFF \
			offsetof(struct cstorm_def_status_block_c, igu_index)
#define DEF_XSB_IGU_INDEX_OFF \
			offsetof(struct xstorm_def_status_block, igu_index)
#define DEF_TSB_IGU_INDEX_OFF \
			offsetof(struct tstorm_def_status_block, igu_index)

#define DEF_USB_SEGMENT_OFF \
			offsetof(struct cstorm_def_status_block_u, segment)
#define DEF_CSB_SEGMENT_OFF \
			offsetof(struct cstorm_def_status_block_c, segment)
#define DEF_XSB_SEGMENT_OFF \
			offsetof(struct xstorm_def_status_block, segment)
#define DEF_TSB_SEGMENT_OFF \
			offsetof(struct tstorm_def_status_block, segment)

#define BNX2X_SP_DSB_INDEX \
		(&bp->def_status_blk->sp_sb.\
					index_values[HC_SP_INDEX_ETH_DEF_CONS])

#define SET_FLAG(value, mask, flag) \
	do {\
		(value) &= ~(mask);\
		(value) |= ((flag) << (mask##_SHIFT));\
	}while(0)

#define GET_FLAG(value, mask) \
	(((value) &= (mask)) >> (mask##_SHIFT))

#define GET_FIELD(value, fname) \
	(((value) & (fname##_MASK)) >> (fname##_SHIFT))

#define CAM_IS_INVALID(x) \
	(GET_FLAG(x.flags, \
	MAC_CONFIGURATION_ENTRY_ACTION_TYPE)== \
	(T_ETH_MAC_COMMAND_INVALIDATE))

/* Number of u32 elements in MC hash array */
#define MC_HASH_SIZE			8
#define MC_HASH_OFFSET(bp, i)		(BAR_TSTRORM_INTMEM + \
	TSTORM_APPROXIMATE_MATCH_MULTICAST_FILTERING_OFFSET(BP_FUNC(bp)) + i*4)


#ifndef PXP2_REG_PXP2_INT_STS
#define PXP2_REG_PXP2_INT_STS		PXP2_REG_PXP2_INT_STS_0
#endif

#ifndef NIG_REG_NIG_PRTY_STS
#define NIG_REG_NIG_PRTY_STS		0x103d0
#endif

#ifndef ETH_MAX_RX_CLIENTS_E2
#define ETH_MAX_RX_CLIENTS_E2 		ETH_MAX_RX_CLIENTS_E1H
#endif

#define BNX2X_VPD_LEN 			128
#define BNX2X_VPD_LRDT			0x80	/* Large Resource Data Type */
#define BNX2X_VPD_LRDT_ID(x)		(x | BNX2X_VPD_LRDT)

/* Large Resource Data Type Item Names */
#define BNX2X_VPD_LRDT_LIN_ID_STRING	0x02	/* Identifier String */
#define BNX2X_VPD_LRDT_LIN_RO_DATA	0x10	/* Read-Only Data */
#define BNX2X_VPD_LRDT_LIN_RW_DATA	0x11	/* Read-Write Data */

#define BNX2X_VPD_LRDT_ID_STRING \
	BNX2X_VPD_LRDT_ID(BNX2X_VPD_LRDT_LIN_ID_STRING)
#define BNX2X_VPD_LRDT_RO_DATA \
	BNX2X_VPD_LRDT_ID(BNX2X_VPD_LRDT_LIN_RO_DATA)
#define BNX2X_VPD_LRDT_RW_DATA \
	BNX2X_VPD_LRDT_ID(BNX2X_VPD_LRDT_LIN_RW_DATA)

/* Small Resource Data Type Item Names */
#define BNX2X_VPD_SRDT_SIN_END		0x78	/* End */

#define BNX2X_VPD_SRDT_END		BNX2X_VPD_SRDT_SIN_END

#define BNX2X_VPD_RO_KEYWORD_PARTNO	"PN"
#define BNX2X_VPD_RO_KEYWORD_VENDOR0	"V0"
#define BNX2X_VPD_RO_KEYWORD_MANUFACTURERNO	"MN"

#define BNX2X_VPD_SRDT_SIN_MASK		0x78
#define BNX2X_VPD_SRDT_LEN_MASK		0x07

#define BNX2X_VPD_LRDT_TAG_SIZE		3
#define BNX2X_VPD_SRDT_TAG_SIZE		1
#define BNX2X_VPD_INFO_FLD_HDR_SIZE	3
#define VENDOR_ID_LEN			4

#if defined(__VMKLNX__) /* ! BNX2X_UPSTREAM */
/***********************************************************/
/*                         Functions                       */
/***********************************************************/

#ifdef BNX2X_VMWARE_BMAPILNX /* ! BNX2X_UPSTREAM */
int bnx2x_open(struct net_device *dev);
int bnx2x_close(struct net_device *dev);
#endif
#endif
/* MISC_REG_RESET_REG - this is here for the hsi to work don't touch */


/* Congestion management fairness mode */
#define CMNG_FNS_NONE		0
#define CMNG_FNS_MINMAX		1
#ifdef BNX2X_SAFC /* ! BNX2X_UPSTREAM */
#define CMNG_FNS_COSWRR		2
#endif

#define HC_SEG_ACCESS_DEF		0   /*Driver decision 0-3*/
#define HC_SEG_ACCESS_ATTN		4
#define HC_SEG_ACCESS_NORM		0   /*Driver decision 0-1*/

#endif /* bnx2x.h */
