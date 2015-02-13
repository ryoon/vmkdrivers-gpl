/* bnx2x_common.h: Broadcom Everest network driver.
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
 * UDP CSUM errata workaround by Arik Gendelman
 * Slowpath and fastpath rework by Vladislav Zolotarov
 * Statistics and Link management by Yitchak Gertner
 *
 */
#ifndef BNX2X_COMMON_H
#define BNX2X_COMMON_H

#include <linux/version.h>
#include <linux/module.h>
#if (LINUX_VERSION_CODE >= 0x020600) /* BNX2X_UPSTREAM */
#include <linux/moduleparam.h>
#endif
#include <linux/kernel.h>
#include <linux/ethtool.h>
#include <linux/pci.h>

extern struct iro e1_iro_arr[];
extern struct iro e1h_iro_arr[];
extern struct iro e2_iro_arr[];
/* This is used as a replacement for an MCP if it's not present */
extern int load_count[2][3]; /* per-path: 0-common, 1-port0, 2-port1 */

extern int int_mode;
extern int num_queues;

/************************ Macros ********************************/
#define BNX2X_FW_IP_HDR_ALIGN_PAD	2 /* FW places hdr with this padding */

#define IGU_U_SB_OFFSET(bp) ((bp)->igu_sb_cnt/2)

#if (LINUX_VERSION_CODE >= 0x020622) /* BNX2X_UPSTREAM */
#define BNX2X_PCI_FREE(x, y, size) \
	do { \
		if (x) { \
			dma_free_coherent(&bp->pdev->dev, size, (void*)x, y); \
			x = NULL; \
			y = 0; \
		} \
	} while (0)
#else
#define BNX2X_PCI_FREE(x, y, size) \
	do { \
		if (x) { \
			pci_free_consistent(bp->pdev, size, (void*)x, y); \
			x = NULL; \
			y = 0; \
		} \
	} while (0)
#endif

#define BNX2X_FREE(x) \
	do { \
		if (x) { \
			kfree((void*)x); \
			x = NULL; \
		} \
	} while (0)

#if (LINUX_VERSION_CODE >= 0x020622) /* BNX2X_UPSTREAM */
#define BNX2X_PCI_ALLOC(x, y, size) \
	do { \
		x = dma_alloc_coherent(&bp->pdev->dev, size, y, GFP_KERNEL); \
		if (x == NULL) \
			goto alloc_mem_err; \
		memset((void*)x, 0, size); \
	} while (0)
#else
#define BNX2X_PCI_ALLOC(x, y, size) \
	do { \
		x = pci_alloc_consistent(bp->pdev, size, y); \
		if (x == NULL) \
			goto alloc_mem_err; \
		memset((void*)x, 0, size); \
	} while (0)
#endif

#define BNX2X_ALLOC(x, size) \
	do { \
		x = kzalloc(size, GFP_KERNEL); \
		if (x == NULL) \
			goto alloc_mem_err; \
	} while (0)

/*********************** Interfaces ****************************
 *  Functions that need to be implemented by each driver version
 */
/* Init */

/**
 * Init HW blocks according to current initialization stage:
 * COMMON, PORT or FUNCTION.
 *
 * @param bp
 * @param load_code: COMMON, PORT or FUNCTION
 *
 * @return int
 */
int bnx2x_init_hw(struct bnx2x *bp, u32 load_code);

/**
 * Init driver internals:
 *  - rings
 *  - status blocks
 *  - etc.
 *
 * @param bp
 * @param load_code COMMON, PORT or FUNCTION
 */
void bnx2x_nic_init(struct bnx2x *bp, u32 load_code);

/**
 * Setup non-leading eth Client.
 *
 * @param bp
 * @param fp
 *
 * @return int
 */
int bnx2x_setup_client(struct bnx2x *bp, struct bnx2x_fastpath *fp,
		       int leading);

/**
 * Bring up a leading (the first) eth Client.
 *
 * @param bp
 *
 * @return int
 */
int bnx2x_setup_leading(struct bnx2x *bp);

/**
 * Init/halt function before/after sending
 * CLIENT_SETUP/CFC_DEL for the first/last client.
 *
 * @param bp
 *
 * @return int
 */
int bnx2x_func_start(struct bnx2x *bp);
int bnx2x_func_stop(struct bnx2x *bp);

/**
 * Bring down a non-leading eth Client.
 *
 * @param bp
 * @param p
 *
 * @return int
 */
int bnx2x_stop_fw_client(struct bnx2x *bp, struct bnx2x_client_ramrod_params *p);

/**
 * Prepare ILT configurations according to current driver
 * parameters.
 *
 * @param bp
 */
void bnx2x_ilt_set_info(struct bnx2x *bp);

/**
 * Configure eth MAC address in the HW according to the value in
 * netdev->dev_addr.
 *
 * @param bp
 * @param set
 */
void bnx2x_set_eth_mac(struct bnx2x *bp, bool set);


#ifdef BCM_CNIC
/**
 * Set/Clear FIP MAC(s) at the next enties in the CAM after the ETH
 * MAC(s). This function will wait until the ramdord completion
 * returns.
 *
 * @param bp driver handle
 * @param set set or clear the CAM entry
 *
 * @return 0 if cussess, -ENODEV if ramrod doesn't return.
 */
int bnx2x_set_fip_eth_mac_addr(struct bnx2x *bp, bool set);

/**
 * Set iSCSI MAC(s) at the next enties in the CAM after the ETH
 * MAC(s). This function will wait until the ramdord completion
 * returns.
 *
 * @param bp driver handle
 * @param set set or clear the CAM entry
 *
 * @return 0 if cussess, -ENODEV if ramrod doesn't return.
 */
int bnx2x_set_iscsi_eth_mac_addr(struct bnx2x *bp, bool set);

/**
 * Set/Clear ALL_ENODE mcast MAC.
 *
 * @param bp
 * @param set
 *
 * @return int
 */
int bnx2x_set_all_enode_macs(struct bnx2x *bp, bool set);
#endif


/**
 * Send the MCP a request, block until there is a reply.
 *
 * @param bp
 * @param command
 * @param param
 *
 * @return u32
 */
u32 bnx2x_fw_command(struct bnx2x *bp, u32 command, u32 param);

/**
 * Initialize link parameters structure variables.
 *
 * @param bp
 * @param load_mode
 *
 * @return u8
 */
u8 bnx2x_initial_phy_init(struct bnx2x *bp, int load_mode);

/**
 * Free driver's memories.
 *
 * @param bp
 */
void bnx2x_free_mem(struct bnx2x *bp);

/**
 * Allocate driver's memory.
 *
 * @param bp
 *
 * @return int
 */
int bnx2x_alloc_mem(struct bnx2x *bp);

/**
 * Acquire HW lock.
 *
 * @param bp
 * @param resource Resource bit which was locked
 *
 * @return int
 */
int bnx2x_acquire_hw_lock(struct bnx2x *bp, u32 resource);

/**
 * Release HW lock.
 *
 * @param bp
 * @param resource Resource bit which was locked
 *
 * @return int
 */
int bnx2x_release_hw_lock(struct bnx2x *bp, u32 resource);

/**
 * Configure MAC filtering rules in a FW according to bp->rx_mode.
 * If bp->state is OPEN,
 * should be called with netif_addr_lock_bh().
 *
 * @param bp
 * @param wait If true, wait for completion
 */
void bnx2x_set_storm_rx_mode(struct bnx2x *bp, bool wait);

/**
 * Disable interrupts. This function ensures that there are no
 * ISRs or SP DPCs (sp_task) are running after it returns.
 *
 * @param bp
 * @param disable_hw if true, disable HW interrupts.
 */
void bnx2x_int_disable_sync(struct bnx2x *bp, int disable_hw);

/**
 * Disable interrupts.
 * @param bp
 */
void bnx2x_int_disable(struct bnx2x *bp);

/**
 * Enable HW interrupts.
 *
 * @param bp
 */
void bnx2x_int_enable(struct bnx2x *bp);

/**
 * Write driver pulse to shmem: writes the value in
 * bp->fw_drv_pulse_wr_seq to drv_pulse mbox in the shmem.
 *
 * @param bp
 */
void bnx2x_drv_pulse(struct bnx2x *bp);

/**
 * Write an ack to IGU.
 *
 * @param bp
 * @param igu_sb_id
 * @param segment
 * @param index
 * @param op
 * @param update
 */
void bnx2x_igu_ack_sb(struct bnx2x *bp, u8 igu_sb_id, u8 segment,
		      u16 index, u8 op, u8 update);

/**
 *
 * @param bp
 */
void bnx2x_dcbx_init(struct bnx2x *bp);

/**
 * Set power state to the requested value. Currently only D0 and
 * D3hot are supported.
 *
 * @param bp
 * @param state D0 or D3hot
 *
 * @return int
 */
int bnx2x_set_power_state(struct bnx2x *bp, pci_power_t state);

#if (LINUX_VERSION_CODE < 0x020613) && (VMWARE_ESX_DDK_VERSION < 40000)
irqreturn_t bnx2x_msix_sp_int(int irq, void *dev_instance,
				     struct pt_regs *regs);
irqreturn_t bnx2x_interrupt(int irq, void *dev_instance,
				   struct pt_regs *regs);
#else /* BNX2X_UPSTREAM */
irqreturn_t bnx2x_msix_sp_int(int irq, void *dev_instance);
irqreturn_t bnx2x_interrupt(int irq, void *dev_instance);
#endif

/**
 * Cleanup chip internals:
 * - Cleanup MAC configuration.
 * - Close clients.
 * - etc.
 *
 * @param bp
 * @param unload_mode
 */
void bnx2x_chip_cleanup(struct bnx2x *bp, int unload_mode);

/**
 * Set MAC filtering configurations. If bp->state is OPEN,
 * should be called with netif_addr_lock_bh().
 *
 * @remarks called with netif_tx_lock from dev_mcast.c
 *
 * @param dev
 */
void bnx2x_set_rx_mode(struct net_device *dev);

/**
 * disables tx from stack point of view
 *
 * @param bp
 */

static inline void bnx2x_tx_disable(struct bnx2x *bp)
{
	netif_tx_disable(bp->dev);
	netif_carrier_off(bp->dev);
}

/** Configures rx_mode for a single client.
 *
 * @param bp
 * @param cl_id
 * @param rx_mode_flags
 * @param accept_flags
 * @param wait
 */
void bnx2x_set_cl_rx_mode(struct bnx2x *bp, u8 cl_id,
			  unsigned long rx_mode_flags,
			  unsigned long accept_flags,
			  unsigned long ramrod_flags);

/* Parity errors related */
void bnx2x_inc_load_cnt(struct bnx2x *bp);
u32 bnx2x_dec_load_cnt(struct bnx2x *bp);
bool bnx2x_chk_parity_attn(struct bnx2x *bp);
bool bnx2x_reset_is_done(struct bnx2x *bp);
void bnx2x_disable_close_the_gate(struct bnx2x *bp);

void bnx2x__link_status_update(struct bnx2x *bp);

/* Handle ramrods completion */
void bnx2x_sp_event(struct bnx2x_fastpath *fp,
		    union eth_rx_cqe *rr_cqe);

/* Statistics */
void bnx2x_stats_handle(struct bnx2x *bp, enum bnx2x_stats_event event);

#ifdef BCM_CNIC
int bnx2x_cnic_notify(struct bnx2x *bp, int cmd);
void bnx2x_setup_cnic_irq_info(struct bnx2x *bp);
#endif

/* Error handling */
void bnx2x_panic_dump(struct bnx2x *bp);
/********************************************************/

/**************** Common functions ***********************/

/* dev_close main block */
int bnx2x_nic_unload(struct bnx2x *bp, int unload_mode);

#if !defined(BNX2X_NEW_NAPI) && defined(USE_NAPI_GRO) /* ! BNX2X_UPSTREAM */
int  __bnx2x_poll(struct napi_struct *napi, int budget);
#endif

void bnx2x_set_num_queues(struct bnx2x *bp);

/* dev_open main block */
int bnx2x_nic_load(struct bnx2x *bp, int load_mode);

#if defined(BNX2X_SAFC) || defined(BCM_CNIC) /* ! BNX2X_UPSTREAM */
u16 bnx2x_select_queue(struct net_device *dev, struct sk_buff *skb);
#endif

/* hard_xmit callback */
int bnx2x_start_xmit(struct sk_buff *skb, struct net_device *dev);

int bnx2x_change_mac_addr(struct net_device *dev, void *p);

/* NAPI poll Rx part */
int bnx2x_rx_int(struct bnx2x_fastpath *fp, int budget);

void bnx2x_update_rx_prod(struct bnx2x *bp, struct bnx2x_fastpath *fp,
			u16 bd_prod, u16 rx_comp_prod, u16 rx_sge_prod);

/* NAPI poll Tx part */
int bnx2x_tx_int(struct bnx2x_fastpath *fp);

/* suspend/resume callbacks */
int bnx2x_suspend(struct pci_dev *pdev, pm_message_t state);
int bnx2x_resume(struct pci_dev *pdev);

/**
 * Request single ISR interrupt line (MSI or INT#x).
 *
 * @param bp
 *
 * @return int
 */
int bnx2x_req_irq(struct bnx2x *bp);

/**
 * Enable MSI.
 *
 * @param bp
 *
 * @return int
 */
int bnx2x_enable_msi(struct bnx2x *bp);

void bnx2x_free_fp_mem(struct bnx2x *bp);
int bnx2x_alloc_fp_mem(struct bnx2x *bp);
void bnx2x_napi_enable(struct bnx2x *bp);
void bnx2x_init_rx_rings(struct bnx2x *bp);
void bnx2x_free_skbs(struct bnx2x *bp);
void bnx2x_napi_disable(struct bnx2x *bp);
void bnx2x_netif_stop(struct bnx2x *bp, int disable_hw);
void bnx2x_netif_start(struct bnx2x *bp);

#if (LINUX_VERSION_CODE < 0x020613) && (VMWARE_ESX_DDK_VERSION < 40000)
irqreturn_t bnx2x_msix_fp_int(int irq, void *fp_cookie,
				     struct pt_regs *regs);
#ifdef BCM_CNIC
irqreturn_t bnx2x_msix_cnic_int(int irq, void *fp_cookie,
				     struct pt_regs *regs);
#endif
#else /* BNX2X_UPSTREAM */
/**
 * FP and SP MSI-X ISR.
 *
 * @param irq
 * @param fp_cookie
 *
 * @return irqreturn_t
 */
irqreturn_t bnx2x_msix_fp_int(int irq, void *fp_cookie);
#ifdef BCM_CNIC
irqreturn_t bnx2x_msix_cnic_int(int irq, void *fp_cookie);
#endif
#endif


void bnx2x_free_msix_irqs(struct bnx2x *bp, int num_queues);
int bnx2x_enable_msix(struct bnx2x *bp);
int bnx2x_req_msix_irqs(struct bnx2x *bp);
/**
 * Request IRQ vectors from OS.
 *
 * @param bp
 *
 * @return int
 */
int bnx2x_setup_irqs(struct bnx2x *bp);

/* Release IRQ vectors */
void bnx2x_free_irq(struct bnx2x *bp);

void bnx2x_link_report(struct bnx2x *bp);

/* ethtool callbacks */
int bnx2x_get_settings(struct net_device *dev, struct ethtool_cmd *cmd);
u32 bnx2x_get_msglevel(struct net_device *dev);
void bnx2x_set_msglevel(struct net_device *dev, u32 level);
int bnx2x_get_coalesce(struct net_device *dev,
			struct ethtool_coalesce *coal);
void bnx2x_get_ringparam(struct net_device *dev,
			struct ethtool_ringparam *ering);
int bnx2x_set_ringparam(struct net_device *dev,
			struct ethtool_ringparam *ering);
void bnx2x_get_pauseparam(struct net_device *dev,
			struct ethtool_pauseparam *epause);
#if defined(BNX2X_NEW_NAPI) /* BNX2X_UPSTREAM */
int bnx2x_poll(struct napi_struct *napi, int budget);
#else
int bnx2x_poll(struct net_device *dev, int *budget);
#endif

int __devinit bnx2x_alloc_mem_bp(struct bnx2x *bp);
void bnx2x_free_mem_bp(struct bnx2x *bp);

#if (LINUX_VERSION_CODE >= 0x02061a) /* BNX2X_UPSTREAM */
int bnx2x_set_flags(struct net_device *dev, u32 data);
#endif

u32 bnx2x_get_rx_csum(struct net_device *dev);
int bnx2x_set_rx_csum(struct net_device *dev, u32 data);
int bnx2x_set_tso(struct net_device *dev, u32 data);
int bnx2x_change_mtu(struct net_device *dev, int new_mtu);
void bnx2x_tx_timeout(struct net_device *dev);
#ifdef BCM_VLAN
/* called with rtnl_lock */
void bnx2x_vlan_rx_register(struct net_device *dev,
				   struct vlan_group *vlgrp);

void bnx2x_vlan_rx_kill_vid(struct net_device *dev, u16 vid);
void bnx2x_vlan_rx_add_vid(struct net_device *dev, u16 vid);
#endif

/*********************** Inlines **********************************/
/*********************** Fast path ********************************/
static inline int bnx2x_has_tx_work(struct bnx2x_fastpath *fp)
{
	u16 hw_cons;

	/* Tell compiler that status block fields can change */
	barrier();
	hw_cons = le16_to_cpu(*fp->tx_cons_sb);
	return hw_cons != fp->tx_pkt_cons;
}

static inline int bnx2x_has_rx_work(struct bnx2x_fastpath *fp)
{
	u16 rx_cons_sb;

	/* Tell compiler that status block fields can change */
	barrier();
	rx_cons_sb = le16_to_cpu(*fp->rx_cons_sb);
	if ((rx_cons_sb & MAX_RCQ_DESC_CNT) == MAX_RCQ_DESC_CNT)
		rx_cons_sb++;
	return (fp->rx_comp_cons != rx_cons_sb);
}

static inline void bnx2x_igu_ack_sb_gen(struct bnx2x *bp, u8 igu_sb_id,
					u8 segment, u16 index, u8 op,
					u8 update, u32 igu_addr)
{
	struct igu_regular cmd_data = {0};

	cmd_data.sb_id_and_flags =
			((index << IGU_REGULAR_SB_INDEX_SHIFT) |
			 (segment << IGU_REGULAR_SEGMENT_ACCESS_SHIFT) |
			 (update << IGU_REGULAR_BUPDATE_SHIFT) |
			 (op << IGU_REGULAR_ENABLE_INT_SHIFT));

	DP(NETIF_MSG_HW, "write 0x%08x to IGU addr 0x%x\n",
	   cmd_data.sb_id_and_flags, igu_addr);
	REG_WR(bp, igu_addr, cmd_data.sb_id_and_flags);

	/* Make sure that ACK is written */
	mmiowb();
	barrier();
}

static inline void bnx2x_igu_clear_sb_gen(struct bnx2x *bp, u8 func,
					  u8 idu_sb_id, bool is_Pf)
{
	u32 data, ctl, cnt = 100;
	u32 igu_addr_data = IGU_REG_COMMAND_REG_32LSB_DATA;
	u32 igu_addr_ctl = IGU_REG_COMMAND_REG_CTRL;
	u32 igu_addr_ack = IGU_REG_CSTORM_TYPE_0_SB_CLEANUP + (idu_sb_id/32)*4;
	u32 sb_bit =  1 << (idu_sb_id%32);
	u32 func_encode = func |
			((is_Pf == true ? 1 : 0) << IGU_FID_ENCODE_IS_PF_SHIFT);
	u32 addr_encode = IGU_CMD_E2_PROD_UPD_BASE + idu_sb_id;

	/* Not supported in BC mode */
	if (CHIP_INT_MODE_IS_BC(bp))
		return;

	data = (IGU_USE_REGISTER_cstorm_type_0_sb_cleanup
			<< IGU_REGULAR_CLEANUP_TYPE_SHIFT)	|
		IGU_REGULAR_CLEANUP_SET				|
		IGU_REGULAR_BCLEANUP;

	ctl = addr_encode << IGU_CTRL_REG_ADDRESS_SHIFT 	|
	      func_encode << IGU_CTRL_REG_FID_SHIFT 		|
	      IGU_CTRL_CMD_TYPE_WR << IGU_CTRL_REG_TYPE_SHIFT;

	DP(NETIF_MSG_HW, "write 0x%08x to IGU(via GRC) addr 0x%x\n",
			 data, igu_addr_data);
	REG_WR(bp, igu_addr_data, data);
	mmiowb();
	barrier();
	DP(NETIF_MSG_HW, "write 0x%08x to IGU(via GRC) addr 0x%x\n",
			  ctl, igu_addr_ctl);
	REG_WR(bp, igu_addr_ctl, ctl);
	mmiowb();
	barrier();

	/* wait for clean up to finish */
	while (!(REG_RD(bp, igu_addr_ack) & sb_bit) && --cnt)
		msleep(10);


	if (!(REG_RD(bp, igu_addr_ack) & sb_bit)) {
		DP(NETIF_MSG_HW, "Unable to finish IGU cleanup: "
			  "idu_sb_id %d offset %d bit %d (cnt %d)\n",
			  idu_sb_id, idu_sb_id/32, idu_sb_id%32, cnt);
	}
}

static inline void bnx2x_update_rx_prod_gen(struct bnx2x *bp,
			struct bnx2x_fastpath *fp, u16 bd_prod,
			u16 rx_comp_prod, u16 rx_sge_prod, u32 start)
{
	struct ustorm_eth_rx_producers rx_prods = {0};
	u32 i;

	/* Update producers */
	rx_prods.bd_prod = bd_prod;
	rx_prods.cqe_prod = rx_comp_prod;
	rx_prods.sge_prod = rx_sge_prod;

	/*
	 * Make sure that the BD and SGE data is updated before updating the
	 * producers since FW might read the BD/SGE right after the producer
	 * is updated.
	 * This is only applicable for weak-ordered memory model archs such
	 * as IA-64. The following barrier is also mandatory since FW will
	 * assumes BDs must have buffers.
	 */
	wmb();

	for (i = 0; i < sizeof(rx_prods)/4; i++)
		REG_WR(bp, start + i*4, ((u32 *)&rx_prods)[i]);

	mmiowb(); /* keep prod updates ordered */

	DP(NETIF_MSG_RX_STATUS,
	   "queue[%d]:  wrote  bd_prod %u  cqe_prod %u  sge_prod %u\n",
	   fp->index, bd_prod, rx_comp_prod, rx_sge_prod);
}

static inline void bnx2x_hc_ack_sb(struct bnx2x *bp, u8 sb_id,
				   u8 storm, u16 index, u8 op, u8 update)
{
	u32 hc_addr = (HC_REG_COMMAND_REG + BP_PORT(bp)*32 +
		       COMMAND_REG_INT_ACK);
	struct igu_ack_register igu_ack;

	igu_ack.status_block_index = index;
	igu_ack.sb_id_and_flags =
			((sb_id << IGU_ACK_REGISTER_STATUS_BLOCK_ID_SHIFT) |
			 (storm << IGU_ACK_REGISTER_STORM_ID_SHIFT) |
			 (update << IGU_ACK_REGISTER_UPDATE_INDEX_SHIFT) |
			 (op << IGU_ACK_REGISTER_INTERRUPT_MODE_SHIFT));

#if (LINUX_VERSION_CODE < 0x020600) /* ! BNX2X_UPSTREAM */
	/* x86's writel() in 2.4.x does not have barrier(). */
	barrier();
#endif
	DP(BNX2X_MSG_OFF, "write 0x%08x to HC addr 0x%x\n",
	   (*(u32 *)&igu_ack), hc_addr);
	REG_WR(bp, hc_addr, (*(u32 *)&igu_ack));

	/* Make sure that ACK is written */
	mmiowb();
	barrier();
}

static inline void bnx2x_ack_sb(struct bnx2x *bp, u8 igu_sb_id, u8 storm,
				u16 index, u8 op, u8 update)
{
	if (bp->common.int_block == INT_BLOCK_HC)
		bnx2x_hc_ack_sb(bp, igu_sb_id, storm, index, op, update);
	else {
		u8 segment;

		if (CHIP_INT_MODE_IS_BC(bp))
			segment = storm;
		else if (igu_sb_id != bp->igu_dsb_id)
			segment = IGU_SEG_ACCESS_DEF;
		else if (storm == ATTENTION_ID)
			segment = IGU_SEG_ACCESS_ATTN;
		else
			segment = IGU_SEG_ACCESS_DEF;
		bnx2x_igu_ack_sb(bp, igu_sb_id, segment, index, op, update);
	}
}

static inline void bnx2x_update_fpsb_idx(struct bnx2x_fastpath *fp)
{
	barrier(); /* status block is written to by the chip */
	fp->fp_hc_idx = fp->sb_running_index[SM_RX_ID];
}

static inline u16 bnx2x_hc_ack_int(struct bnx2x *bp)
{
	u32 hc_addr = (HC_REG_COMMAND_REG + BP_PORT(bp)*32 +
		       COMMAND_REG_SIMD_MASK);
	u32 result = REG_RD(bp, hc_addr);

	DP(BNX2X_MSG_OFF, "read 0x%08x from HC addr 0x%x\n",
	   result, hc_addr);

	barrier();
	return result;
}

static inline u16 bnx2x_igu_ack_int(struct bnx2x *bp)
{
	u32 igu_addr = (BAR_IGU_INTMEM + IGU_REG_SISR_MDPC_WMASK_LSB_UPPER*8);
	u32 result = REG_RD(bp, igu_addr);

	DP(NETIF_MSG_HW, "read 0x%08x from IGU addr 0x%x\n",
	   result, igu_addr);

	barrier();
	return result;
}

static inline u16 bnx2x_ack_int(struct bnx2x *bp)
{
	barrier();
	if (bp->common.int_block == INT_BLOCK_HC)
		return bnx2x_hc_ack_int(bp);
	else
		return bnx2x_igu_ack_int(bp);
}

static inline int bnx2x_alloc_rx_skb(struct bnx2x *bp,
				     struct bnx2x_fastpath *fp, u16 index)
{
	struct sk_buff *skb;
	struct sw_rx_bd *rx_buf = &fp->rx_buf_ring[index];
	struct eth_rx_bd *rx_bd = &fp->rx_desc_ring[index];
	dma_addr_t mapping;

	skb = netdev_alloc_skb(bp->dev, bp->rx_buf_size);
	if (unlikely(skb == NULL))
		return -ENOMEM;

#if (LINUX_VERSION_CODE >= 0x020622) && !defined(BNX2X_OLD_FCOE) /* BNX2X_UPSTREAM */
	mapping = dma_map_single(&bp->pdev->dev, skb->data, bp->rx_buf_size,
				 DMA_FROM_DEVICE);
#else
	mapping = pci_map_single(bp->pdev, skb->data, bp->rx_buf_size,
				 PCI_DMA_FROMDEVICE);
#endif
#if (LINUX_VERSION_CODE >= 0x02061b) /* BNX2X_UPSTREAM */
	if (unlikely(dma_mapping_error(&bp->pdev->dev, mapping))) {
#else
	if (unlikely(dma_mapping_error(mapping))) {
#endif
		dev_kfree_skb(skb);
		return -ENOMEM;
	}

	rx_buf->skb = skb;
#if (LINUX_VERSION_CODE >= 0x020622) && !defined(BNX2X_OLD_FCOE) /* BNX2X_UPSTREAM */
	dma_unmap_addr_set(rx_buf, mapping, mapping);
#else
	pci_unmap_addr_set(rx_buf, mapping, mapping);
#endif

	rx_bd->addr_hi = cpu_to_le32(U64_HI(mapping));
	rx_bd->addr_lo = cpu_to_le32(U64_LO(mapping));

	return 0;
}

/* note that we are not allocating a new skb,
 * we are just moving one from cons to prod
 * we are not creating a new mapping,
 * so there is no need to check for dma_mapping_error().
 */
static inline void bnx2x_reuse_rx_skb(struct bnx2x_fastpath *fp,
				      u16 cons, u16 prod)
{
	struct bnx2x *bp = fp->bp;
	struct sw_rx_bd *cons_rx_buf = &fp->rx_buf_ring[cons];
	struct sw_rx_bd *prod_rx_buf = &fp->rx_buf_ring[prod];
	struct eth_rx_bd *cons_bd = &fp->rx_desc_ring[cons];
	struct eth_rx_bd *prod_bd = &fp->rx_desc_ring[prod];

#if (LINUX_VERSION_CODE >= 0x020622) && !defined(BNX2X_OLD_FCOE) /* BNX2X_UPSTREAM */
	dma_sync_single_for_device(&bp->pdev->dev,
			       dma_unmap_addr(cons_rx_buf, mapping),
			       RX_COPY_THRESH, DMA_FROM_DEVICE);

	dma_unmap_addr_set(prod_rx_buf, mapping,
			   dma_unmap_addr(cons_rx_buf, mapping));
#else
	pci_dma_sync_single_for_device(bp->pdev,
				       pci_unmap_addr(cons_rx_buf, mapping),
				       RX_COPY_THRESH, PCI_DMA_FROMDEVICE);

	pci_unmap_addr_set(prod_rx_buf, mapping,
			   pci_unmap_addr(cons_rx_buf, mapping));
#endif
	prod_rx_buf->skb = cons_rx_buf->skb;
	*prod_bd = *cons_bd;
}

static inline int bnx2x_has_tx_work_unload(struct bnx2x_fastpath *fp)
{
	/* Tell compiler that consumer and producer can change */
	barrier();
	return (fp->tx_pkt_prod != fp->tx_pkt_cons);
}

static inline u16 bnx2x_tx_avail(struct bnx2x_fastpath *fp)
{
	s16 used;
	u16 prod;
	u16 cons;

	prod = fp->tx_bd_prod;
	cons = fp->tx_bd_cons;

	/* NUM_TX_RINGS = number of "next-page" entries
	   It will be used as a threshold */
	used = SUB_S16(prod, cons) + (s16)NUM_TX_RINGS;

#ifdef BNX2X_STOP_ON_ERROR
	WARN_ON(used < 0);
	WARN_ON(used > fp->bp->tx_ring_size);
	WARN_ON((fp->bp->tx_ring_size - used) > MAX_TX_AVAIL);
#endif

	return (s16)(fp->bp->tx_ring_size) - used;
}

static inline void bnx2x_clear_sge_mask_next_elems(struct bnx2x_fastpath *fp)
{
	int i, j;

	for (i = 1; i <= NUM_RX_SGE_PAGES; i++) {
		int idx = RX_SGE_CNT * i - 1;

		for (j = 0; j < 2; j++) {
			BIT_VEC64_CLEAR_BIT(fp->sge_mask, idx);
			idx--;
		}
	}
}

/************************* Init ******************************************/
static inline void bnx2x_add_all_napi(struct bnx2x *bp)
{
	int i;

	/* Add NAPI objects */
	for_each_napi_queue(bp, i)
#ifdef BNX2X_NEW_NAPI /* BNX2X_UPSTREAM */
#ifdef __VMKLNX__ /* ! BNX2X_UPSTREAM */
		if (!bnx2x_fp(bp, i, napi).net_poll)
#endif /* __VMKLNX__ */
		netif_napi_add(bp->dev, &bnx2x_fp(bp, i, napi),
			       bnx2x_poll, BNX2X_NAPI_WEIGHT);
#else /* ! BNX2X_UPSTREAM */
	{
		/* initialize net_device for each rx queue */
		struct net_device *dummy_netdev = &bnx2x_fp(bp, i, dummy_netdev);
#if defined(USE_NAPI_GRO)
		struct napi_struct *napi = &bnx2x_fp(bp, i, napi);
		napi->dev = bp->dev;
#endif
		dummy_netdev->priv = &bp->fp[i];
		dummy_netdev->poll = bnx2x_poll;
		dummy_netdev->weight = BNX2X_NAPI_WEIGHT;
		set_bit(__LINK_STATE_START, &dummy_netdev->state);
	}
#endif
}

static inline void bnx2x_del_all_napi(struct bnx2x *bp)
{
#if defined(BNX2X_NEW_NAPI) && (LINUX_VERSION_CODE >= 0x02061b) /* BNX2X_UPSTREAM */
	int i;

	for_each_napi_queue(bp, i)
		netif_napi_del(&bnx2x_fp(bp, i, napi));
#endif
}

static inline void bnx2x_disable_msi(struct bnx2x *bp)
{
	if (bp->flags & USING_MSIX_FLAG) {
		pci_disable_msix(bp->pdev);
		bp->flags &= ~USING_MSIX_FLAG;
	} else if (bp->flags & USING_MSI_FLAG) {
		pci_disable_msi(bp->pdev);
		bp->flags &= ~USING_MSI_FLAG;
	}
}

static inline int bnx2x_calc_num_queues(struct bnx2x *bp)
{
	return  (num_queues ?
		 min_t(int, num_queues, BNX2X_MAX_QUEUES(bp)) :
		 min_t(int, num_online_cpus(), BNX2X_MAX_QUEUES(bp)));
}

static inline void bnx2x_init_sge_ring_bit_mask(struct bnx2x_fastpath *fp)
{
	/* Set the mask to all 1-s: it's faster to compare to 0 than to 0xf-s */
	memset(fp->sge_mask, 0xff,
	       (NUM_RX_SGE >> BIT_VEC64_ELEM_SHIFT)*sizeof(u64));

	/* Clear the two last indices in the page to 1:
	   these are the indices that correspond to the "next" element,
	   hence will never be indicated and should be removed from
	   the calculations. */
	bnx2x_clear_sge_mask_next_elems(fp);
}

static inline int bnx2x_alloc_rx_sge(struct bnx2x *bp,
				     struct bnx2x_fastpath *fp, u16 index)
{
	struct page *page = alloc_pages(GFP_ATOMIC, PAGES_PER_SGE_SHIFT);
	struct sw_rx_page *sw_buf = &fp->rx_page_ring[index];
	struct eth_rx_sge *sge = &fp->rx_sge_ring[index];
	dma_addr_t mapping;

	if (unlikely(page == NULL))
		return -ENOMEM;

#if (LINUX_VERSION_CODE >= 0x020622)  && !defined(BNX2X_OLD_FCOE) /* BNX2X_UPSTREAM */
	mapping = dma_map_page(&bp->pdev->dev, page, 0, SGE_PAGE_SIZE*PAGES_PER_SGE,
			       DMA_FROM_DEVICE);
#else
	mapping = pci_map_page(bp->pdev, page, 0, SGE_PAGE_SIZE*PAGES_PER_SGE,
			       PCI_DMA_FROMDEVICE);
#endif
#if (LINUX_VERSION_CODE >= 0x02061b) /* BNX2X_UPSTREAM */
	if (unlikely(dma_mapping_error(&bp->pdev->dev, mapping))) {
#else
	if (unlikely(dma_mapping_error(mapping))) {
#endif
		__free_pages(page, PAGES_PER_SGE_SHIFT);
		return -ENOMEM;
	}

	sw_buf->page = page;
#if (LINUX_VERSION_CODE >= 0x020622) && !defined(BNX2X_OLD_FCOE) /* BNX2X_UPSTREAM */
	dma_unmap_addr_set(sw_buf, mapping, mapping);
#else
	pci_unmap_addr_set(sw_buf, mapping, mapping);
#endif

	sge->addr_hi = cpu_to_le32(U64_HI(mapping));
	sge->addr_lo = cpu_to_le32(U64_LO(mapping));

	return 0;
}

static inline void bnx2x_free_rx_sge(struct bnx2x *bp,
				     struct bnx2x_fastpath *fp, u16 index)
{
	struct sw_rx_page *sw_buf = &fp->rx_page_ring[index];
	struct page *page = sw_buf->page;
	struct eth_rx_sge *sge = &fp->rx_sge_ring[index];

	/* Skip "next page" elements */
	if (!page)
		return;

#if (LINUX_VERSION_CODE >= 0x020622)  && !defined(BNX2X_OLD_FCOE) /* BNX2X_UPSTREAM */
	dma_unmap_page(&bp->pdev->dev, dma_unmap_addr(sw_buf, mapping),
		       SGE_PAGE_SIZE*PAGES_PER_SGE, DMA_FROM_DEVICE);
#else
	pci_unmap_page(bp->pdev, pci_unmap_addr(sw_buf, mapping),
		       SGE_PAGE_SIZE*PAGES_PER_SGE, PCI_DMA_FROMDEVICE);
#endif
	__free_pages(page, PAGES_PER_SGE_SHIFT);

	sw_buf->page = NULL;
	sge->addr_hi = 0;
	sge->addr_lo = 0;
}


static inline void bnx2x_free_rx_sge_range(struct bnx2x *bp,
					   struct bnx2x_fastpath *fp, int last)
{
	int i;

	if (fp->disable_tpa)
		return;

	for (i = 0; i < last; i++)
		bnx2x_free_rx_sge(bp, fp, i);
}


static inline void bnx2x_free_tpa_pool(struct bnx2x *bp,
				       struct bnx2x_fastpath *fp, int last)
{
	int i;

	for (i = 0; i < last; i++) {
		struct sw_rx_bd *rx_buf = &(fp->tpa_pool[i]);
		struct sk_buff *skb = rx_buf->skb;

		if (skb == NULL) {
			DP(NETIF_MSG_IFDOWN, "tpa bin %d empty on free\n", i);
			continue;
		}

		if (fp->tpa_state[i] == BNX2X_TPA_START)
#if (LINUX_VERSION_CODE >= 0x020622) && !defined(BNX2X_OLD_FCOE) /* BNX2X_UPSTREAM */
			dma_unmap_single(&bp->pdev->dev,
					 dma_unmap_addr(rx_buf, mapping),
					 bp->rx_buf_size, DMA_FROM_DEVICE);
#else
			pci_unmap_single(bp->pdev,
					 pci_unmap_addr(rx_buf, mapping),
					 bp->rx_buf_size, PCI_DMA_FROMDEVICE);
#endif

		dev_kfree_skb(skb);
		rx_buf->skb = NULL;
	}
}

static inline void bnx2x_set_next_page_rx_bd(struct bnx2x_fastpath *fp)
{
	int i;

	for (i = 1; i <= NUM_RX_RINGS; i++) {
		struct eth_rx_bd *rx_bd;

		rx_bd = &fp->rx_desc_ring[RX_DESC_CNT * i - 2];
		rx_bd->addr_hi =
			cpu_to_le32(U64_HI(fp->rx_desc_mapping +
				    BCM_PAGE_SIZE*(i % NUM_RX_RINGS)));
		rx_bd->addr_lo =
			cpu_to_le32(U64_LO(fp->rx_desc_mapping +
				    BCM_PAGE_SIZE*(i % NUM_RX_RINGS)));
	}
}

static inline void bnx2x_set_next_page_sgl(struct bnx2x_fastpath *fp)
{
	int i;

	for (i = 1; i <= NUM_RX_SGE_PAGES; i++) {
		struct eth_rx_sge *sge;

		sge = &fp->rx_sge_ring[RX_SGE_CNT * i - 2];
		sge->addr_hi =
			cpu_to_le32(U64_HI(fp->rx_sge_mapping +
			BCM_PAGE_SIZE*(i % NUM_RX_SGE_PAGES)));

		sge->addr_lo =
			cpu_to_le32(U64_LO(fp->rx_sge_mapping +
			BCM_PAGE_SIZE*(i % NUM_RX_SGE_PAGES)));
	}
}

static inline void bnx2x_set_next_page_rx_cq(struct bnx2x_fastpath *fp)
{
	int i;
	for (i = 1; i <= NUM_RCQ_RINGS; i++) {
		struct eth_rx_cqe_next_page *nextpg;

		nextpg = (struct eth_rx_cqe_next_page *)
			&fp->rx_comp_ring[RCQ_DESC_CNT * i - 1];
		nextpg->addr_hi =
			cpu_to_le32(U64_HI(fp->rx_comp_mapping +
				   BCM_PAGE_SIZE*(i % NUM_RCQ_RINGS)));
		nextpg->addr_lo =
			cpu_to_le32(U64_LO(fp->rx_comp_mapping +
				   BCM_PAGE_SIZE*(i % NUM_RCQ_RINGS)));
	}
}

/* Returns the number of actually allocated BDs */
static inline int bnx2x_alloc_rx_bds(struct bnx2x_fastpath *fp,
				      int rx_ring_size)
{
	struct bnx2x *bp = fp->bp;
	u16 ring_prod, cqe_ring_prod;
	int i;

	fp->rx_comp_cons = 0;
	cqe_ring_prod = ring_prod = 0;

	/* This routine is called only during for init so
	 * fp->eth_q_stats.rx_skb_alloc_failed = 0
	 */
	for (i = 0; i < rx_ring_size; i++) {
		if (bnx2x_alloc_rx_skb(bp, fp, ring_prod) < 0) {
			fp->eth_q_stats.rx_skb_alloc_failed++;
			continue;
		}
		ring_prod = NEXT_RX_IDX(ring_prod);
		cqe_ring_prod = NEXT_RCQ_IDX(cqe_ring_prod);
		WARN_ON(ring_prod <= (i - fp->eth_q_stats.rx_skb_alloc_failed));
	}
	if (fp->eth_q_stats.rx_skb_alloc_failed)
		BNX2X_ERR("was only able to allocate "
			  "%d rx skbs on queue[%d]\n",
			  (i - fp->eth_q_stats.rx_skb_alloc_failed), fp->index);

	fp->rx_bd_prod = ring_prod;
	/* Limit the CQE producer by the CQE ring size */
	fp->rx_comp_prod = min_t(u16, NUM_RCQ_RINGS*RCQ_DESC_CNT,
			       cqe_ring_prod);
	fp->rx_pkt = fp->rx_calls = 0;

	return (i - fp->eth_q_stats.rx_skb_alloc_failed);
}

static inline void bnx2x_init_tx_ring_one(struct bnx2x_fastpath *fp)
{
	int i;

	for (i = 1; i <= NUM_TX_RINGS; i++) {
		struct eth_tx_next_bd *tx_next_bd =
			&fp->tx_desc_ring[TX_DESC_CNT * i - 1].next_bd;

		tx_next_bd->addr_hi =
			cpu_to_le32(U64_HI(fp->tx_desc_mapping +
				    BCM_PAGE_SIZE*(i % NUM_TX_RINGS)));
		tx_next_bd->addr_lo =
			cpu_to_le32(U64_LO(fp->tx_desc_mapping +
				    BCM_PAGE_SIZE*(i % NUM_TX_RINGS)));
	}

	SET_FLAG(fp->tx_db.data.header.header, DOORBELL_HDR_DB_TYPE, 1);
	fp->tx_db.data.zero_fill1 = 0;
	fp->tx_db.data.prod = 0;

	fp->tx_pkt_prod = 0;
	fp->tx_pkt_cons = 0;
	fp->tx_bd_prod = 0;
	fp->tx_bd_cons = 0;
	fp->tx_pkt = 0;
}

static inline void bnx2x_init_tx_rings(struct bnx2x *bp)
{
	int i;

	for_each_tx_queue(bp, i)
		bnx2x_init_tx_ring_one(&bp->fp[i]);

}

static inline void bnx2x_init_vlan_mac_fp_objs(struct bnx2x_fastpath *fp,
					       bnx2x_obj_type obj_type)
{
	struct bnx2x *bp = fp->bp;

	/* Configure classification DBs */
	bnx2x_init_mac_obj(bp, &fp->mac_obj, fp->cl_id, fp->cid,
			   BP_FUNC(bp), bnx2x_sp(bp, mac_rdata),
			   bnx2x_sp_mapping(bp, mac_rdata),
			   BNX2X_FILTER_MAC_PENDING,
			   &bp->sp_state, obj_type,
			   &bp->macs_pool);

	bnx2x_init_vlan_obj(bp, &fp->vlan_obj, fp->cl_id, fp->cid,
			    BP_FUNC(bp), bnx2x_sp(bp, vlan_rdata),
			    bnx2x_sp_mapping(bp, vlan_rdata),
			    BNX2X_FILTER_VLAN_PENDING,
			    &bp->sp_state, obj_type,
			    &bp->vlans_pool);
}

static inline void bnx2x_init_bp_objs(struct bnx2x *bp)
{
	/* RX_MODE controlling object */
	bnx2x_init_rx_mode_obj(bp, &bp->rx_mode_obj);

	/* multicast configuration controlling object */
	bnx2x_init_mcast_obj(bp, &bp->mcast_obj, bp->fp->cl_id, bp->fp->cid,
			     BP_FUNC(bp), bnx2x_sp(bp, mcast_rdata),
			     bnx2x_sp_mapping(bp, mcast_rdata),
			     BNX2X_FILTER_MCAST_PENDING, &bp->sp_state,
			     BNX2X_OBJ_TYPE_RX);

	/* Setup CAM credit pools */
	bnx2x_init_mac_credit_pool(bp, &bp->macs_pool);
	bnx2x_init_vlan_credit_pool(bp, &bp->vlans_pool);

	/* RSS configuration object */
	bnx2x_init_rss_config_obj(bp, &bp->rss_conf_obj, bp->fp->cl_id,
				  bp->fp->cid, BP_FUNC(bp),
				  bnx2x_sp(bp, rss_rdata),
				  bnx2x_sp_mapping(bp, rss_rdata),
				  BNX2X_FILTER_RSS_CONF_PENDING, &bp->sp_state,
				  BNX2X_OBJ_TYPE_RX);
}

static inline u8 bnx2x_fp_cl_qzone_id(struct bnx2x_fastpath *fp)
{
	if (CHIP_IS_E1x(fp->bp))
		return fp->cl_id + BP_PORT(fp->bp) * ETH_MAX_RX_CLIENTS_E1H;
	else
		return fp->cl_id;
}

static inline u32 bnx2x_rx_ustorm_prods_offset(struct bnx2x_fastpath *fp)
{
	struct bnx2x *bp = fp->bp;

	if (CHIP_IS_E2(bp))
		return USTORM_RX_PRODS_E2_OFFSET(fp->cl_qzone_id);
	else
		return USTORM_RX_PRODS_E1X_OFFSET(BP_PORT(bp), fp->cl_id);
}


#ifdef BCM_CNIC
/**
 * Updates Rx producers according to iSCSI OOO ETH ring spec.
 *
 * @param bp
 * @param fp
 * @param bd_prod
 * @param rx_comp_prod
 * @param rx_sge_prod
 */
static inline void bnx2x_update_ooo_prod(struct bnx2x *bp,
			struct bnx2x_fastpath *fp, u16 bd_prod,
			u16 rx_comp_prod, u16 rx_sge_prod)
{
	struct ustorm_eth_rx_producers rx_prods = {0};
	u32 i;
	u32 start = BAR_USTRORM_INTMEM + fp->ustorm_rx_prods_offset;

	/* Update producers */
	rx_prods.bd_prod = bd_prod + 0x4000;
	rx_prods.cqe_prod = rx_comp_prod + 0x4000;
	rx_prods.sge_prod = rx_sge_prod;

	/*
	 * Make sure that the BD and SGE data is updated before updating the
	 * producers since FW might read the BD/SGE right after the producer
	 * is updated.
	 * This is only applicable for weak-ordered memory model archs such
	 * as IA-64. The following barrier is also mandatory since FW will
	 * assumes BDs must have buffers.
	 */
	wmb();

	for (i = 0; i < sizeof(rx_prods)/4; i++)
		REG_WR(bp, start + i*4, ((u32 *)&rx_prods)[i]);

	mmiowb(); /* keep prod updates ordered */
	barrier();

	REG_WR16(bp, BAR_TSTRORM_INTMEM +
		 TSTORM_ISCSI_L2_ISCSI_OOO_PROD_OFFSET(BP_FUNC(bp)),
		 fp->rx_pkts_avail);

	mmiowb(); /* keep prod updates ordered */

	DP(NETIF_MSG_RX_STATUS,
	   "queue[%d]:  wrote  bd_prod %u  cqe_prod %u  sge_prod %u\n",
	   fp->index, bd_prod, rx_comp_prod, rx_sge_prod);
}

static inline int bnx2x_alloc_ooo_rx_bd_ring(struct bnx2x_fastpath *fp)
{
	struct bnx2x *bp = fp->bp;
	int rx_ring_size = bp->rx_ring_size, i;

	/* - OOO BD ring size should be less or equal to half FWD
	 *   Tx ring size.
	 * - OOO BD ring size must be less than than CQE ring size
	 *   minus maximum number of outstanding ramrods.
	 */
	/* Delete me! For integration only! */
	rx_ring_size = min_t(int, bp->tx_ring_size / 2, 500);

	/*rx_ring_size = min_t(int, bp->tx_ring_size / 2,
			     bp->rx_ring_size); */

	rx_ring_size = min_t(int, rx_ring_size, INIT_OOO_RING_SIZE);

	fp->rx_pkts_avail = bnx2x_alloc_rx_bds(fp, rx_ring_size);

	/* Add more CQEs for ramrods to ensure the demand above */
	for (i = 0; i < MAX_SPQ_PENDING; i++)
		fp->rx_comp_prod = NEXT_RCQ_IDX(fp->rx_comp_prod);

	return fp->rx_pkts_avail;
}

static inline u8 bnx2x_cnic_eth_cl_id(struct bnx2x *bp, u8 cl_idx)
{
	return bp->cnic_base_cl_id + cl_idx +
		BP_E1HVN(bp) * NONE_ETH_CONTEXT_USE;
}

static inline u8 bnx2x_cnic_fw_sb_id(struct bnx2x *bp) {

	/* the 'first' id is allocated for the cnic */
	if (CHIP_IS_E1x(bp))
		return bp->base_fw_ndsb + BP_L_ID(bp);
	else
		return bp->base_fw_ndsb;
}
static inline u8 bnx2x_cnic_igu_sb_id(struct bnx2x *bp) {
	return bp->igu_base_sb;
}



static inline void bnx2x_init_fcoe_fp(struct bnx2x *bp)
{
	struct bnx2x_fastpath *fp = bnx2x_fcoe_fp(bp);

	bnx2x_fcoe(bp, cl_id) = bnx2x_cnic_eth_cl_id(bp,
						     BNX2X_FCOE_ETH_CL_ID_IDX);
	bnx2x_fcoe(bp, cid) = BNX2X_FCOE_ETH_CID;
	bnx2x_fcoe(bp, fw_sb_id) = DEF_SB_ID;
	bnx2x_fcoe(bp, igu_sb_id) = bp->igu_dsb_id;
	bnx2x_fcoe(bp, bp) = bp;
	bnx2x_fcoe(bp, state) = BNX2X_FP_STATE_CLOSED;
	bnx2x_fcoe(bp, index) = FCOE_IDX;
	bnx2x_fcoe(bp, rx_cons_sb) = BNX2X_FCOE_L2_RX_INDEX;
	bnx2x_fcoe(bp, tx_cons_sb) = BNX2X_FCOE_L2_TX_INDEX;
	/* qZone id equals to FW (per path) client id */
	bnx2x_fcoe(bp, cl_qzone_id) = bnx2x_fp_cl_qzone_id(fp);
	/* init shortcut */
	bnx2x_fcoe(bp, ustorm_rx_prods_offset) = bnx2x_rx_ustorm_prods_offset(fp);

	/* Configure classification DBs */
	bnx2x_init_vlan_mac_fp_objs(fp, BNX2X_OBJ_TYPE_RX);
}

static inline void bnx2x_init_ooo_fp(struct bnx2x *bp)
{
	struct bnx2x_fastpath *fp = bnx2x_ooo_fp(bp);

	bnx2x_ooo(bp, cl_id) = bnx2x_cnic_eth_cl_id(bp,
						    BNX2X_OOO_ETH_CL_ID_IDX);
	bnx2x_ooo(bp, cid) = BNX2X_OOO_ETH_CID;
	bnx2x_ooo(bp, fw_sb_id) = bnx2x_cnic_fw_sb_id(bp);
	bnx2x_ooo(bp, igu_sb_id) = bnx2x_cnic_igu_sb_id(bp);
	bnx2x_ooo(bp, bp) = bp;
	bnx2x_ooo(bp, state) = BNX2X_FP_STATE_CLOSED;
	bnx2x_ooo(bp, index) = OOO_IDX;
	bnx2x_ooo(bp, rx_cons_sb) = BNX2X_RX_OOO_INDEX;
	bnx2x_ooo(bp, tx_cons_sb) = NULL; /* No Tx for OOO client */
	/* qZone id equals to FW (per path) client id */
	bnx2x_ooo(bp, cl_qzone_id) = bnx2x_fp_cl_qzone_id(fp);
	/* init shortcut */
	bnx2x_ooo(bp, ustorm_rx_prods_offset) =
		bnx2x_rx_ustorm_prods_offset(fp);

	/* Init OOO related internal memory */
	/* Set OOO ring CID */
	REG_WR(bp, BAR_TSTRORM_INTMEM +
	       TSTORM_ISCSI_L2_ISCSI_OOO_CID_TABLE_OFFSET(BP_FUNC(bp)),
		HW_CID(bp, bnx2x_ooo(bp, cid)));

	/* Set OOO ring Client ID */
	REG_WR8(bp, BAR_TSTRORM_INTMEM +
		TSTORM_ISCSI_L2_ISCSI_OOO_CLIENT_ID_TABLE_OFFSET(BP_FUNC(bp)),
		bnx2x_ooo(bp, cl_id));

	/* Configure classification DBs */
	bnx2x_init_vlan_mac_fp_objs(fp, BNX2X_OBJ_TYPE_RX);
}

static inline void bnx2x_init_fwd_fp(struct bnx2x *bp)
{
	struct bnx2x_fastpath *fp = bnx2x_fwd_fp(bp);

	bnx2x_fwd(bp, cl_id) = -1; /* No CL_ID for forwarding */
	bnx2x_fwd(bp, cid) = BNX2X_FWD_ETH_CID;
	bnx2x_fwd(bp, fw_sb_id) = bnx2x_cnic_fw_sb_id(bp);  /* Connect to CNIC SB */
	bnx2x_fwd(bp, igu_sb_id) = bnx2x_cnic_igu_sb_id(bp);
	bnx2x_fwd(bp, bp) = bp;
	bnx2x_fwd(bp, state) = BNX2X_FP_STATE_CLOSED;
	bnx2x_fwd(bp, index) = FWD_IDX;
	bnx2x_fwd(bp, rx_cons_sb) = NULL;
	bnx2x_fwd(bp, tx_cons_sb) = BNX2X_TX_FWD_INDEX;
}
#endif

static inline int bnx2x_clean_tx_queue(struct bnx2x *bp,
				       struct bnx2x_fastpath *fp)
{
	int cnt = 1000;

	while (bnx2x_has_tx_work_unload(fp)) {
		if (!cnt) {
			BNX2X_ERR("timeout waiting for queue[%d]: "
				 "fp->tx_pkt_prod(%d) != fp->tx_pkt_cons(%d)\n",
				  fp->index, fp->tx_pkt_prod, fp->tx_pkt_cons);
#ifdef BNX2X_STOP_ON_ERROR
			bnx2x_panic();
			return -EBUSY;
#else
			break;
#endif
		}
		cnt--;
		msleep(1);
	}

	return 0;
}

void bnx2x_acquire_phy_lock(struct bnx2x *bp);
void bnx2x_release_phy_lock(struct bnx2x *bp);
int bnx2x_get_link_cfg_idx(struct bnx2x *bp);

static inline void __storm_memset_struct(struct bnx2x *bp,
					 u32 addr, size_t size, u32 *data)
{
	int i;
	for (i = 0; i < size/4; i++)
		REG_WR(bp, addr + (i * 4), data[i]);
}

static inline void storm_memset_func_cfg(struct bnx2x *bp,
				struct tstorm_eth_function_common_config *tcfg,
				u16 abs_fid)
{
	size_t size = sizeof(struct tstorm_eth_function_common_config);

	u32 addr = BAR_TSTRORM_INTMEM +
			TSTORM_FUNCTION_COMMON_CONFIG_OFFSET(abs_fid);

	__storm_memset_struct(bp, addr, size, (u32*)tcfg);
}

static inline void storm_memset_cmng(struct bnx2x *bp,
				struct cmng_struct_per_port *cmng,
				u8 port)
{
	size_t size = sizeof(struct cmng_struct_per_port);

	u32 addr = BAR_XSTRORM_INTMEM +
			XSTORM_CMNG_PER_PORT_VARS_OFFSET(port);

	__storm_memset_struct(bp, addr, size, (u32*)cmng);
}

/* Waits for all outstanding SP commands to complete. */
static inline bool bnx2x_wait_sp_comp(struct bnx2x *bp)
{
	int tout = 5000; /* Wait for 5 secs tops */

	while (tout--) {
		smp_mb();
		if (!bp->sp_state)
			return true;

		msleep(1);
	}

	smp_mb();
	if (bp->sp_state) {
		BNX2X_ERR("Filtering completion timed out. sp_state 0x%lx\n",
			  bp->sp_state);
		return false;
	}

	return true;
}


#endif /* BNX2X_COMMON_H */
