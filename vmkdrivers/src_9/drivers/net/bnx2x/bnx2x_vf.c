/* bnx2x_vf.c: Broadcom Everest network driver.
 *
 * Copyright 2009-2011 Broadcom Corporation
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
 * Maintained by: Eilon Greenstein <eilong@broadcom.com>
 * Written by: Shmulik Ravid
 *
 */
#include <linux/version.h>
#include <linux/module.h>
#if (LINUX_VERSION_CODE >= 0x020600) /* BNX2X_UPSTREAM */
#include <linux/moduleparam.h>
#endif
#include <linux/kernel.h>
#if (LINUX_VERSION_CODE >= 0x020600) /* BNX2X_UPSTREAM */
#include <linux/device.h>  /* for dev_info() */
#endif
#include <linux/timer.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#if (LINUX_VERSION_CODE >= 0x020600) /* BNX2X_UPSTREAM */
#include <linux/dma-mapping.h>
#endif
#include <linux/bitops.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <asm/byteorder.h>
#include <linux/time.h>
#include <linux/ethtool.h>
#include <linux/if_vlan.h>
#if (LINUX_VERSION_CODE >= 0x020600) /* BNX2X_UPSTREAM */
#include <linux/workqueue.h>
#endif
#include <linux/prefetch.h>
#if (LINUX_VERSION_CODE >= 0x020618) /* BNX2X_UPSTREAM */
#include <linux/io.h>
#else
#include <asm/io.h>
#endif

#include "bnx2x.h"
#include "bnx2x_init.h"
#include "bnx2x_common.h"

#ifdef BCM_IOV
#ifdef __VMKLNX__ /* ! BNX2X_UPSTREAM */
#include "bnx2x_esx.h"
#endif

#include "bnx2x_vf.h"

#define BNX2X_VF_LOCAL_SWITCH

#ifdef VFPF_MBX /* BNX2X_UPSTREAM */

/** To get size of message of the type X(request or response)
 *  for the op_code Y of the version Z one should use
 *
 *  op_code_req_sz[Y][Z].req_sz/resp_sz
 ***************************************************************************/
static const msg_sz_t *op_codes_req_sz[] = {
	(const msg_sz_t*)acquire_req_sz,
	(const msg_sz_t*)init_vf_req_sz,
	(const msg_sz_t*)setup_q_req_sz,
	(const msg_sz_t*)set_q_filters_req_sz,
	(const msg_sz_t*)activate_q_req_sz,
	(const msg_sz_t*)deactivate_q_req_sz,
	(const msg_sz_t*)teardown_q_req_sz,
	(const msg_sz_t*)close_vf_req_sz,
	(const msg_sz_t*)release_vf_req_sz
};


static const int vfapi_to_pfvf_status_codes[] = {
	PFVF_STATUS_SUCCESS,		/* VF_API_SUCCESS */
	PFVF_STATUS_FAILURE,		/* VF_API_FAILURE */
	PFVF_STATUS_NO_RESOURCE		/* VF_API_NO_RESOURCE */
};

#endif /* VFPF_MBX */


#define VF_MAX_QUEUE_CNT(vf) 	(min_t(u8, BNX2X_VF_MAX_QUEUES, \
				 min_t(u8, (vf)->max_sb_count,  \
				 BNX2X_CIDS_PER_VF)))

/****************************************************************************
* General service functions
****************************************************************************/
static inline void storm_memset_vf_mbx_ack(struct bnx2x *bp, u16 abs_fid)
{
	u32 addr = BAR_CSTRORM_INTMEM +
			CSTORM_VF_PF_CHANNEL_STATE_OFFSET(abs_fid);

	REG_WR8(bp, addr, VF_PF_CHANNEL_STATE_READY);
}

static inline void storm_memset_vf_mbx_valid(struct bnx2x *bp, u16 abs_fid)
{
	u32 addr = BAR_CSTRORM_INTMEM +
			CSTORM_VF_PF_CHANNEL_VALID_OFFSET(abs_fid);

	REG_WR8(bp, addr, 1);
}

static inline void storm_memset_vf_to_pf(struct bnx2x *bp, u16 abs_fid,
					 u16 pf_id)
{
	REG_WR8(bp, BAR_XSTRORM_INTMEM + XSTORM_VF_TO_PF_OFFSET(abs_fid),
		pf_id);
	REG_WR8(bp, BAR_CSTRORM_INTMEM + CSTORM_VF_TO_PF_OFFSET(abs_fid),
		pf_id);
	REG_WR8(bp, BAR_TSTRORM_INTMEM + TSTORM_VF_TO_PF_OFFSET(abs_fid),
		pf_id);
	REG_WR8(bp, BAR_USTRORM_INTMEM + USTORM_VF_TO_PF_OFFSET(abs_fid),
		pf_id);
}


inline void
storm_memset_rcq_np(struct bnx2x *bp, dma_addr_t np_map, u8 cl_id)
{
	u32 addr = BAR_USTRORM_INTMEM +
		USTORM_CQE_PAGE_NEXT_VMWARE_TEMP_OFFSET(BP_PORT(bp), cl_id);

	REG_WR(bp,  addr, U64_LO(np_map));
	REG_WR(bp,  addr + 4, U64_HI(np_map));
}

static inline int bnx2x_vf_idx_by_abs_fid(struct bnx2x *bp, u16 abs_vfid)
{
	int idx;
	for_each_vf(bp, idx)
		if (bnx2x_vf(bp, idx, abs_vfid) == abs_vfid)
			break;
	return idx;
}

static inline
struct bnx2x_virtf *bnx2x_vf_by_abs_fid(struct bnx2x *bp, u16 abs_vfid)
{
	u16 idx =  (u16)bnx2x_vf_idx_by_abs_fid(bp, abs_vfid);
	return ( (idx < BNX2X_NR_VIRTFN(bp)) ? BP_VF(bp, idx) : NULL);
}


inline void
bnx2x_set_vf_mbxs_valid(struct bnx2x *bp)
{
	int i;
	for_each_vf(bp, i)
		storm_memset_vf_mbx_valid(bp, bnx2x_vf(bp, i, abs_vfid));

}

/*
 * VF enable primitives
 *
 * when pretend is required the caller is reponsible
 * for calling pretend prioir to calling these routines
 */

/*
 * called only on E1H or E2.
 * When pretending to be PF, the pretend value is the function number 0...7
 * When pretending t obe VF, the pretend val is the PF-num:VF-valid:ABS-VFID
 * combination
 */
int bnx2x_pretend_func(struct bnx2x *bp, u16 pretend_func_val)
{
	u32 pretend_reg;

	if (CHIP_IS_E1H(bp) && pretend_func_val > E1H_FUNC_MAX)
		return -1;

	/* get my own pretend register */
	pretend_reg = bnx2x_get_pretend_reg(bp);
	REG_WR(bp, pretend_reg, pretend_func_val);
	REG_RD(bp, pretend_reg);
	return 0;
}

/*
 * internal vf enable - until vf is enabled internally all transactions
 * are blocked. this routine should always be called last with pretend.
 */
static inline void bnx2x_vf_enable_internal(struct bnx2x *bp, u8 enable)
{
	REG_WR(bp, PGLUE_B_REG_INTERNAL_VFID_ENABLE, enable ? 1: 0);
}

/* called with pretend */
static inline void bnx2x_vf_enable_pbf(struct bnx2x *bp, u8 enable)
{
	REG_WR(bp, PBF_REG_DISABLE_VF, enable ? 0 : 1);
}


/* clears vf error in all semi blocks */
static inline void bnx2x_vf_semi_clear_err(struct bnx2x *bp, u8 abs_vfid)
{
	REG_WR(bp,TSEM_REG_VFPF_ERR_NUM, abs_vfid);
	REG_WR(bp,USEM_REG_VFPF_ERR_NUM, abs_vfid);
	REG_WR(bp,CSEM_REG_VFPF_ERR_NUM, abs_vfid);
	REG_WR(bp,XSEM_REG_VFPF_ERR_NUM, abs_vfid);
}

static void bnx2x_vf_pglue_clear_err(struct bnx2x *bp, u8 abs_vfid)
{
	u32 was_err_group = (2 * BP_PATH(bp) + abs_vfid) >> 5;
	u32 was_err_reg = 0;

	switch (was_err_group) {
	case 0:
	    was_err_reg = PGLUE_B_REG_WAS_ERROR_VF_31_0_CLR;
	    break;
	case 1:
	    was_err_reg = PGLUE_B_REG_WAS_ERROR_VF_63_32_CLR;
	    break;
	case 2:
	    was_err_reg = PGLUE_B_REG_WAS_ERROR_VF_95_64_CLR;
	    break;
	case 3:
	    was_err_reg = PGLUE_B_REG_WAS_ERROR_VF_127_96_CLR;
	    break;
	}
	REG_WR(bp, was_err_reg, 1 << (abs_vfid & 0x1f));
}

static void bnx2x_vf_igu_ack_sb(struct bnx2x *bp, struct bnx2x_virtf *vf,
				u8 igu_sb_id, u8 segment, u16 index, u8 op,
				u8 update)
{
	/* acking a VF sb through the PF - use the GRC */
	u32 ctl;
	u32 igu_addr_data = IGU_REG_COMMAND_REG_32LSB_DATA;
	u32 igu_addr_ctl = IGU_REG_COMMAND_REG_CTRL;
	u32 func_encode = vf->abs_vfid;
	u32 addr_encode = IGU_CMD_E2_PROD_UPD_BASE + igu_sb_id;
	struct igu_regular cmd_data = {0};

	cmd_data.sb_id_and_flags =
			((index << IGU_REGULAR_SB_INDEX_SHIFT) |
			 (segment << IGU_REGULAR_SEGMENT_ACCESS_SHIFT) |
			 (update << IGU_REGULAR_BUPDATE_SHIFT) |
			 (op << IGU_REGULAR_ENABLE_INT_SHIFT));


	ctl = addr_encode << IGU_CTRL_REG_ADDRESS_SHIFT 	|
	      func_encode << IGU_CTRL_REG_FID_SHIFT 		|
	      IGU_CTRL_CMD_TYPE_WR << IGU_CTRL_REG_TYPE_SHIFT;

	DP(NETIF_MSG_HW, "write 0x%08x to IGU(via GRC) addr 0x%x\n",
			 cmd_data.sb_id_and_flags, igu_addr_data);
	REG_WR(bp, igu_addr_data, cmd_data.sb_id_and_flags);
	mmiowb();
	barrier();

	DP(NETIF_MSG_HW, "write 0x%08x to IGU(via GRC) addr 0x%x\n",
			  ctl, igu_addr_ctl);
	REG_WR(bp, igu_addr_ctl, ctl);
	mmiowb();
	barrier();

}
static void bnx2x_vf_igu_reset(struct bnx2x *bp, struct bnx2x_virtf *vf)
{
	int i;
	u32 val;

	/* Set VF masks and configuration - pretend */
	bnx2x_pretend_func(bp, HW_VF_HANDLE(bp, vf->abs_vfid));

	REG_WR(bp, IGU_REG_SB_INT_BEFORE_MASK_LSB, 0);
	REG_WR(bp, IGU_REG_SB_INT_BEFORE_MASK_MSB, 0);
	REG_WR(bp, IGU_REG_SB_MASK_LSB, 0);
	REG_WR(bp, IGU_REG_SB_MASK_MSB, 0);
	REG_WR(bp, IGU_REG_PBA_STATUS_LSB, 0);
	REG_WR(bp, IGU_REG_PBA_STATUS_MSB, 0);

	val = REG_RD(bp, IGU_REG_VF_CONFIGURATION);
	val |= (IGU_VF_CONF_FUNC_EN | IGU_VF_CONF_MSI_MSIX_EN);
	if (vf->cfg_flags & VF_CFG_INT_SIMD)
	    val |= IGU_VF_CONF_SINGLE_ISR_EN;
	val &= ~IGU_VF_CONF_PARENT_MASK;
	val |= BP_FUNC(bp) << IGU_VF_CONF_PARENT_SHIFT;	/* parent PF */
	REG_WR(bp, IGU_REG_VF_CONFIGURATION, val);

	bnx2x_pretend_func(bp, BP_ABS_FUNC(bp));

	/* iterate ove all queues, clear sb consumer */
	for (i = 0; i < vf->max_sb_count; i++) {
		u8 igu_sb_id = __vf_igu_sb(vf,i);

		/* zero prod memory */
		REG_WR(bp, IGU_REG_PROD_CONS_MEMORY + igu_sb_id * 4, 0);

		/* clear sb state machine */
		bnx2x_igu_clear_sb_gen(bp, vf->abs_vfid, igu_sb_id,
				       false /* VF */);

		/* disable + update */
		bnx2x_vf_igu_ack_sb(bp, vf, igu_sb_id, USTORM_ID, 0,
				    IGU_INT_DISABLE, 1);
	}
}


static void bnx2x_vf_enable_access(struct bnx2x *bp, u8 abs_vfid)
{
	/* set the VF-PF association in the FW */
	storm_memset_vf_to_pf(bp, FW_VF_HANDLE(abs_vfid), BP_FUNC(bp));

	/* clear vf errors*/
	bnx2x_vf_semi_clear_err(bp, abs_vfid);
	bnx2x_vf_pglue_clear_err(bp, abs_vfid);

	/* internal vf-enable - pretend */
	bnx2x_pretend_func(bp, HW_VF_HANDLE(bp, abs_vfid));
	bnx2x_vf_enable_internal(bp, true);
	bnx2x_pretend_func(bp, BP_ABS_FUNC(bp));
}

static void bnx2x_vf_enable_traffic(struct bnx2x *bp, struct bnx2x_virtf *vf)
{
	/* Reset vf in IGU  interrupts are still disabled */
	bnx2x_vf_igu_reset(bp, vf);

	/* pretend to enable the vf with the PBF */
	bnx2x_pretend_func(bp, HW_VF_HANDLE(bp, vf->abs_vfid));
	bnx2x_vf_enable_pbf(bp, true);
	bnx2x_pretend_func(bp, BP_ABS_FUNC(bp));
}


static u8 bnx2x_vf_is_pcie_pending(struct bnx2x *bp, u8 abs_vfid)
{
#ifdef __VMKLNX__ /* ! BNX2X_UPSTREAM */

	vmk_PCIDevice vfDev;
	struct bnx2x_virtf *vf = bnx2x_vf_by_abs_fid(bp, abs_vfid);
	if (!vf)
		goto unknown_dev;

	bnx2x_vmk_vf_pci_dev(bp, vf->index, &vfDev);

	if (vfDev) {
		u16 status;
		vmk_uint16 pos;

		bnx2x_vmk_get_sriov_cap_pos(bp, &pos);
		bnx2x_vmk_pci_read_config_word(vfDev, pos + PCI_EXP_DEVSTA, &status);

		return (status & PCI_EXP_DEVSTA_TRPND);
	}
#else
	struct pci_dev *dev;
	struct bnx2x_virtf *vf = bnx2x_vf_by_abs_fid(bp, abs_vfid);
	if (!vf)
		goto unknown_dev;

	dev = pci_get_bus_and_slot(vf->bus, vf->devfn);
	if (dev)
		return (bnx2x_is_pcie_pending(dev));

#endif
unknown_dev:
	BNX2X_ERR("Unknown device\n");
	return false;
}

static int bnx2x_vf_flr_clnup_epilog(struct bnx2x *bp, u8 abs_vfid)
{
	/* Wait 100ms */
	msleep(100);

	/* Verify no pending pci transactions */
	if (bnx2x_vf_is_pcie_pending(bp, abs_vfid))
		BNX2X_ERR("PCIE Transactions still pending\n");

	return 0;
}


#ifdef VFPF_MBX
/* enable vf_pf mailbox (aka vf-pf-chanell) */
static void bnx2x_vf_enable_mbx(struct bnx2x *bp, u8 abs_vfid)
{
	bnx2x_vf_flr_clnup_epilog(bp, abs_vfid);

	/* enable the mailbox in the FW */
	storm_memset_vf_mbx_ack(bp, abs_vfid);
	storm_memset_vf_mbx_valid(bp, abs_vfid);

	/* eanble the VF access to the mailbox */
	bnx2x_vf_enable_access(bp, abs_vfid);
}
#endif

static int bnx2x_vf_disable(struct bnx2x *bp, u8 abs_vfid)
{
	bnx2x_pretend_func(bp, HW_VF_HANDLE(bp, abs_vfid));
	bnx2x_vf_enable_pbf(bp, false);
	bnx2x_vf_enable_internal(bp, false);
	bnx2x_pretend_func(bp, BP_ABS_FUNC(bp));
	return 0;
}

static vf_api_t
bnx2x_vfq_sw_cleanup(struct bnx2x *bp, struct bnx2x_virtf *vf,
		      struct bnx2x_vfq *rxq, bool drv_only)
{
	struct list_head *head;
	struct bnx2x_list_elem *first;
	unsigned long ramrod_flags = 0;

	/* remove classification rules */
	set_bit(RAMROD_COMP_WAIT, &ramrod_flags);
	if (drv_only)
		set_bit(RAMROD_DRV_CLR_ONLY, &ramrod_flags);

	/* macs */
	head = &rxq->mac_obj.head;
	while (!list_empty(head)) {
		first = list_first_entry(head, struct bnx2x_list_elem, link);
		bnx2x_vf_set_mac(bp, rxq, first->data.mac.mac, 0, ramrod_flags);
	}

	/* vlans TODO*/
	bnx2x_vf_clear_vlans(bp, rxq, false, 0, ramrod_flags);

	/* multicasts */
	if (VF_IS_LEADING_RXQ(rxq))
		/* 0-length list will cause the removal of all mcasts */
		bnx2x_vf_set_mcasts(bp, vf, NULL, 0, drv_only);

	/* Invalidate HW context */
	rxq->cxt->ustorm_ag_context.cdu_usage = 0;
	rxq->cxt->xstorm_ag_context.cdu_reserved = 0;

	return VF_API_SUCCESS;
}

int bnx2x_vf_queue_update_ramrod(struct bnx2x *bp, struct bnx2x_vfq *rxq,
				 dma_addr_t data_mapping, u8 block)
{
	int rc = VF_API_PENDING;

	rxq->update_pending = BNX2X_VF_UPDATE_PENDING;

	/* SETUP ramrod.
	 *
	 * bnx2x_sp_post() takes a spin_lock thus no other explict memory
	 * barrier except from mmiowb() is needed to impose a
	 * proper ordering of memory operations.
	 */
	mmiowb();


	bnx2x_sp_post(bp, RAMROD_CMD_ID_ETH_CLIENT_UPDATE, rxq->cid,
		      U64_HI(data_mapping), U64_LO(data_mapping), 0);

	/* Wait for completion if requested */
	if(block)
		rc = bnx2x_wait_ramrod(bp, BNX2X_VF_UPDATE_DONE, rxq->index,
				       &rxq->update_pending, 0);
	return rc;
}

static int bnx2x_vf_queue_ramrod(struct bnx2x *bp,
			  struct bnx2x_client_ramrod_params *p, int cmd,
			  u16 initial_state, u16 final_state, u8 block)
{
	union eth_specific_data ramrod_data = {{0}};
	int rc = VF_API_PENDING;

	ramrod_data.common_ramrod_data.client_id = p->cl_id;

	*(p->pstate) = initial_state;

	/* SETUP ramrod.
	 *
	 * bnx2x_sp_post() takes a spin_lock thus no other explict memory
	 * barrier except from mmiowb() is needed to impose a
	 * proper ordering of memory operations.
	 */
	mmiowb();
	bnx2x_sp_post(bp, cmd, p->cid,
		      ramrod_data.update_data_addr.hi,
		      ramrod_data.update_data_addr.lo, 0);

	/* Wait for completion if requested */
	if(block)
		rc = bnx2x_wait_ramrod(bp, final_state, p->index, p->pstate, 0);

	return rc;

}


/*
 *	FLR routines:
 */

static void bnx2x_vf_free_resc(struct bnx2x *bp ,struct bnx2x_virtf *vf)
{
#ifndef DYNAMIC_RSC
	/* static allocation: nothing to do */
#else
	int i;

	for_each_vf_sb(vf, i)
		SET_BIT(vf->sbi[i].hc_qid, BP_QID_MAP(bp));

	for (; i < vf->rxq_count; i++)
		SET_BIT(vf->rxq[i].hw_qid, BP_QID_MAP(bp));
#endif
}

int bnx2x_vf_flr_clnup(struct bnx2x *bp, struct bnx2x_virtf *vf)
{
	u32 poll_cnt = bnx2x_flr_clnup_poll_count(bp);
	int i;

	/*
	 * FLR cleanup is mostly asynchronous - Whenever a command is
	 * dispatched to the FW the function returns and restarted when the
	 * completion arrives.
	 */

	/* terminate all the queues */
	for_each_vf_rxq(vf, i) {
		struct bnx2x_vfq *rxq = VF_RXQ(vf, i);
		struct bnx2x_client_ramrod_params ramrod_params = {0};

		if (rxq->state == BNX2X_FP_STATE_CLOSED)
			continue;

		/* Send terminate ramrod */
		ramrod_params.cid = rxq->cid;
		ramrod_params.cl_id = vfq_cl_id(vf, rxq);
		ramrod_params.index = 0;
		ramrod_params.pstate = &rxq->state;

		bnx2x_vf_queue_ramrod(bp, &ramrod_params,
			RAMROD_CMD_ID_ETH_TERMINATE,
			BNX2X_FP_STATE_OPEN | BNX2X_FP_SUB_STATE_TERMINATING,
			BNX2X_FP_STATE_CLOSED, true); /* block */
	}

	/* clear allocated resources ~~ clear classification rules */
	for_each_vf_rxq(vf, i) {
		struct bnx2x_vfq *rxq = VF_RXQ(vf, i);
		bnx2x_vfq_sw_cleanup(bp, vf, rxq, true); /* drv-only*/
	}

	/* poll DQ usage counter */
	bnx2x_pretend_func(bp, HW_VF_HANDLE(bp, vf->abs_vfid));
	bnx2x_flr_clnup_poll_hw_counter(bp,
				DORQ_REG_VF_USAGE_CNT,
				"DQ VF usage counter timed out",
				poll_cnt);
	bnx2x_pretend_func(bp, BP_ABS_FUNC(bp));

	/* send the FW cleanup command - poll for the results */
	if (bnx2x_send_final_clnup(bp, (u8)FW_VF_HANDLE(vf->abs_vfid),
				   poll_cnt))
		return -EBUSY;

	/* ATC cleanup */

	/* verify TX hw is flushed */
	bnx2x_tx_hw_flushed(bp, poll_cnt);

	/* mark resources as released */
	bnx2x_vf_free_resc(bp, vf);

	return 0;
}

#if 0
void bnx2x_vf_handle_flr_self_notification(struct bnx2x* bp, u8 abs_vfid)
{
	int i;

	for_each_vf(bp, i) {
		struct bnx2x_virtf *vf = BP_VF(bp, i);

		if (vf->abs_vfid != abs_vfid)
			continue;


		vf->state = VF_RESET;
		DP(BNX2X_MSG_MCP,"Initiating Final cleanup for VF %d\n",
		   vf->abs_vfid);
		bnx2x_vf_flr_clnup(bp, vf);

		/* re-open the mailbox */
		bnx2x_vf_enable_mbx(bp, vf->abs_vfid);

		break;
	}
}
#endif


void bnx2x_vf_handle_flr_event(struct bnx2x* bp)
{
	#define FLRD_VFS_DWORDS (BNX2X_MAX_NUM_OF_VFS / 32)

	u32 flrd_vfs[FLRD_VFS_DWORDS];	/* bit vector */
	int i;

	/* Read FLR'd VFs */
	for (i = 0; i < FLRD_VFS_DWORDS; i++)
		flrd_vfs[i] = SHMEM2_RD(bp, mcp_vf_disabled[i]);

	DP(BNX2X_MSG_MCP,"DRV_STATUS_VF_DISABLED received for vfs 0x%x 0x%x\n",
	   flrd_vfs[0], flrd_vfs[1]);

	for_each_vf(bp, i) {
		struct bnx2x_virtf *vf = BP_VF(bp, i);
		u8 reset = 0;

		if (vf->abs_vfid < 32)
			reset = flrd_vfs[0] & (1 << vf->abs_vfid);
		else
			reset = flrd_vfs[1] & (1 << (vf->abs_vfid -32));

		if (reset) {
#ifdef __VMKLNX__ /* ! BNX2X_UPSTREAM */
			/*
			 * A work-around for the FLR issued between vf_acquire/vf_get_info
			 * and vf_init
			 */
			if (vf->state == VF_ACQUIRED)
				continue;
#endif

			vf->state = VF_RESET;
			DP(BNX2X_MSG_MCP,"Initiating Final cleanup for VF %d\n",
			   vf->abs_vfid);

			bnx2x_vf_flr_clnup(bp, vf);

#ifdef VFPF_MBX /* BNX2X_UPSTREAM */

			/* re-open the mailbox */
			bnx2x_vf_enable_mbx(bp, vf->abs_vfid);
#endif
		}
	}

	/* Acknoledge the VFs you handled  (per PF)*/
	for (i = 0; i < FLRD_VFS_DWORDS; i++)
		SHMEM2_WR(bp, drv_ack_vf_disabled[BP_FW_MB_IDX(bp)][i],
			flrd_vfs[i]);

	bnx2x_fw_command(bp, DRV_MSG_CODE_VF_DISABLED_DONE, 0);

	/*
	 * clear the acked bits - better yet if the MCP implemented
	 * write to clear semantics
	 */
	for (i = 0; i < FLRD_VFS_DWORDS; i++)
		SHMEM2_WR(bp, drv_ack_vf_disabled[BP_FW_MB_IDX(bp)][i],0);


}


/*
 *	IOV global initialization routines
 */
void bnx2x_iov_init_dq(struct bnx2x *bp)
{
	if (!IS_SRIOV(bp))
		return;

	/* Set the DQ such that the CID reflect the abs_vfid */
	REG_WR(bp, DORQ_REG_VF_NORM_VF_BASE, 0);
	REG_WR(bp, DORQ_REG_MAX_RVFID_SIZE, ilog2(BNX2X_MAX_NUM_OF_VFS));

	/*
	 * Set VFs starting CID. If its > 0 the preceding CIDs are belong to
	 * the PF L2 queues
	 */
	REG_WR(bp, DORQ_REG_VF_NORM_CID_BASE, BNX2X_FIRST_VF_CID);

	/* The VF window size is the log2 of the max number of CIDs per VF */
	REG_WR(bp, DORQ_REG_VF_NORM_CID_WND_SIZE, BNX2X_VF_CID_WND);

	/*
	 * The VF doorbell size  0 - *B, 4 - 128B. We set it here to match
	 * the Pf doorbell size although the 2 are independent.
	 */
	REG_WR(bp, DORQ_REG_VF_NORM_CID_OFST,
	       BNX2X_DB_SHIFT - BNX2X_DB_MIN_SHIFT);

	/*
	 * No security checks for now -
	 * configure single rule (out of 16) mask = 0x1, value = 0x0,
	 * CID range 0 - 0x1ffff
	 */
	REG_WR(bp, DORQ_REG_VF_TYPE_MASK_0, 1);
	REG_WR(bp, DORQ_REG_VF_TYPE_VALUE_0, 0);
	REG_WR(bp, DORQ_REG_VF_TYPE_MIN_MCID_0, 0);
	REG_WR(bp, DORQ_REG_VF_TYPE_MAX_MCID_0, 0x1ffff);

	/* set the number of VF alllowed doorbells to the full DQ range */
	REG_WR(bp, DORQ_REG_VF_NORM_MAX_CID_COUNT, 0x20000);

	/* set the VF doorbell threshold */
	REG_WR(bp, DORQ_REG_VF_USAGE_CT_LIMIT, 4);
}

void bnx2x_iov_init_dmae(struct bnx2x *bp)
{
	if (!IS_SRIOV(bp))
		return;

	REG_WR(bp, DMAE_REG_BACKWARD_COMP_EN, 0);
}


static int bnx2x_sriov_pci_cfg_info(struct bnx2x *bp, struct bnx2x_sriov *iov)
{
#ifdef __VMKLNX__ /* ! BNX2X_UPSTREAM */

	vmk_uint16 pos;
	vmk_PCIDevice dev;

	if (bnx2x_vmk_pci_dev(bp, &dev))
		return -ENODEV;

	bnx2x_vmk_get_sriov_cap_pos(bp, &pos);

	iov->pos = pos;
	bnx2x_vmk_pci_read_config_word(dev, pos + PCI_SRIOV_CTRL, &iov->ctrl);
	bnx2x_vmk_pci_read_config_word(dev, pos + PCI_SRIOV_TOTAL_VF, &iov->total);
	bnx2x_vmk_pci_read_config_word(dev, pos + PCI_SRIOV_INITIAL_VF, &iov->initial);
	bnx2x_vmk_pci_read_config_word(dev, pos + PCI_SRIOV_VF_OFFSET, &iov->offset);
	bnx2x_vmk_pci_read_config_word(dev, pos + PCI_SRIOV_VF_STRIDE, &iov->stride);
	bnx2x_vmk_pci_read_config_dword(dev, pos + PCI_SRIOV_SUP_PGSIZE, &iov->pgsz);
	bnx2x_vmk_pci_read_config_dword(dev, pos + PCI_SRIOV_CAP, &iov->cap);
	bnx2x_vmk_pci_read_config_byte(dev, pos + PCI_SRIOV_FUNC_LINK, &iov->link);
#else

	int pos;
	struct pci_dev *dev = bp->pdev;

	pos = pci_find_ext_capability(dev, PCI_EXT_CAP_ID_SRIOV);
	if (!pos)
		return -ENODEV;

	iov->pos = pos;
	DP(BNX2X_MSG_IOV,"sriov ext pos %d\n",pos);
	pci_read_config_word(dev, pos + PCI_SRIOV_CTRL, &iov->ctrl);
	pci_read_config_word(dev, pos + PCI_SRIOV_TOTAL_VF, &iov->total);
	pci_read_config_word(dev, pos + PCI_SRIOV_INITIAL_VF, &iov->initial);
	pci_read_config_word(dev, pos + PCI_SRIOV_VF_OFFSET, &iov->offset);
	pci_read_config_word(dev, pos + PCI_SRIOV_VF_STRIDE, &iov->stride);
	pci_read_config_dword(dev, pos + PCI_SRIOV_SUP_PGSIZE, &iov->pgsz);
	pci_read_config_dword(dev, pos + PCI_SRIOV_CAP, &iov->cap);
	pci_read_config_byte(dev, pos + PCI_SRIOV_FUNC_LINK, &iov->link);

#endif
	return 0;

}

static void bnx2x_sriov_nres(struct bnx2x *bp, int *nres)
{
#ifdef __VMKLNX__ /* ! BNX2X_UPSTREAM */

	/*
	 * The code below dpend on the SRIOV capability being initilaized on
	 * native Linux pci_dev structure. Since iov->nres is used only for
	 * debug, we skip thie code to To avoid compatibility issues.
	 */
#else
	int i;
	unsigned long flags;
	struct pci_dev *dev = bp->pdev;

	for (i = 0; i < PCI_SRIOV_NUM_BARS; i++) {
		flags = pci_resource_flags(dev, PCI_IOV_RESOURCES + i);
		if (!flags)
			continue;
		(*nres)++;
		if (flags & PCI_BASE_ADDRESS_MEM_TYPE_64)
			i++;
	}
#endif
}

static int bnx2x_sriov_info(struct bnx2x *bp, struct bnx2x_sriov *iov)
{
	u32 val;

	/*
	 * read the SRIOV capability structure
	 * The fields can be read via configuration read or
	 * directly from the device (starting at offset PCICFG_OFFSET)
	 */
	if (bnx2x_sriov_pci_cfg_info(bp, iov))
		return -ENODEV;

	/* get the number of SRIOV bars */
	iov->nres = 0;
	bnx2x_sriov_nres(bp, &iov->nres);

	/* read the first_vfid */
	val = REG_RD(bp, PCICFG_OFFSET + GRC_CONFIG_REG_PF_INIT_VF);
	iov->first_vf_in_pf = ((val & GRC_CR_PF_INIT_VF_PF_FIRST_VF_NUM_MASK)
			       * 8) - (BNX2X_MAX_NUM_OF_VFS * BP_PATH(bp));

	DP(BNX2X_MSG_IOV, "IOV info[%d]: "
	   "first vf %d, nres %d, cap 0x%x, ctrl 0x%x, total %d, initial %d, "
	   "num vfs %d, offset %d, stride %d, page size 0x%x\n",
	   BP_FUNC(bp),
	   iov->first_vf_in_pf, iov->nres, iov->cap, iov->ctrl, iov->total,
	   iov->initial, iov->nr_virtfn, iov->offset, iov->stride, iov->pgsz);

	return 0;
}

static inline int bnx2x_vf_bus(struct bnx2x *bp, int vfid)
{
#ifdef __VMKLNX__ /* ! BNX2X_UPSTREAM */
	return bnx2x_vmk_vf_bus(bp, vfid);
#else
	struct pci_dev *dev = bp->pdev;
	struct bnx2x_sriov *iov = &bp->vfdb->sriov;

	return dev->bus->number + ((dev->devfn + iov->offset +
				    iov->stride * vfid) >> 8);
#endif
}

static inline int bnx2x_vf_devfn(struct bnx2x *bp, int vfid)
{
#ifdef __VMKLNX__ /* ! BNX2X_UPSTREAM */
	return bnx2x_vmk_vf_devfn(bp, vfid);
#else
	struct pci_dev *dev = bp->pdev;
	struct bnx2x_sriov *iov = &bp->vfdb->sriov;

	return (dev->devfn + iov->offset + iov->stride * vfid) & 0xff;
#endif
}

static void bnx2x_vf_set_bars(struct bnx2x *bp, struct bnx2x_virtf *vf)
{
#ifdef __VMKLNX__ /* ! BNX2X_UPSTREAM */
	bnx2x_vmk_vf_set_bars(bp, vf);
#else
	int i, n;
	struct pci_dev *dev = bp->pdev;
	struct bnx2x_sriov *iov = &bp->vfdb->sriov;

	for (i = 0, n = 0; i < PCI_SRIOV_NUM_BARS; i+=2, n++) {
		u64 start = pci_resource_start(dev, PCI_IOV_RESOURCES + i);
		u32 size = pci_resource_len(dev, PCI_IOV_RESOURCES + i);
		do_div(size, iov->total);
		vf->bars[n].bar = start + size * vf->abs_vfid;
		vf->bars[n].size = size;
	}
#endif
}

static inline int bnx2x_ari_enabled(struct pci_dev *dev)
{
#ifdef __VMKLNX__ /* ! BNX2X_UPSTREAM */
	return 1;
#else
	return (dev->bus->self && dev->bus->self->ari_enabled);
#endif
}

static inline void __devinit
bnx2x_vf_set_igu_info(struct bnx2x *bp, u8 igu_sb_id, u8 abs_vfid)
{
	struct bnx2x_virtf *vf = bnx2x_vf_by_abs_fid(bp, abs_vfid);
	if (vf) {
		if (!vf->max_sb_count)
			vf->igu_base_id = igu_sb_id;
		vf->max_sb_count++;
	 }
}

static void __devinit
bnx2x_get_vf_igu_cam_info(struct bnx2x *bp)
{
	int sb_id;
	u32 val;
	u8 fid;

	/* IGU in normal mode - read CAM */
	for (sb_id = 0; sb_id < IGU_REG_MAPPING_MEMORY_SIZE; sb_id++) {
		val = REG_RD(bp, IGU_REG_MAPPING_MEMORY + sb_id * 4);
		if (!(val & IGU_REG_MAPPING_MEMORY_VALID))
			continue;
		fid = GET_FIELD((val), IGU_REG_MAPPING_MEMORY_FID);
		if (!(fid & IGU_FID_ENCODE_IS_PF))
			bnx2x_vf_set_igu_info(bp, sb_id,
					      (fid & IGU_FID_VF_NUM_MASK));

		DP(BNX2X_MSG_IOV, "%s[%d], igu_sb_id=%d, msix=%d\n",
		   ((fid & IGU_FID_ENCODE_IS_PF) ? "PF" : "VF"),
		   ((fid & IGU_FID_ENCODE_IS_PF) ? (fid & IGU_FID_PF_NUM_MASK) :
		   (fid & IGU_FID_VF_NUM_MASK)), sb_id,
		   GET_FIELD((val), IGU_REG_MAPPING_MEMORY_VECTOR));
	}
}

static void __bnx2x_iov_free_vfdb(struct bnx2x *bp)
{
	if (bp->vfdb) {
		if (bp->vfdb->vfq_stats)
			kfree(bp->vfdb->vfq_stats);
		if (bp->vfdb->rxqs)
			kfree(bp->vfdb->rxqs);
		if (bp->vfdb->txqs)
			kfree(bp->vfdb->txqs);
		if (bp->vfdb->vfs)
			kfree(bp->vfdb->vfs);
		kfree(bp->vfdb);
	}
	bp->vfdb = NULL;
}

/* must be called after PF bars are mapped */
int __devinit bnx2x_iov_init_one(struct bnx2x *bp, int int_mode_param,
				 int num_vfs_param)
{
	int err, i, queue_count;
	struct bnx2x_sriov *iov;


#ifdef __VMKLNX__ /* ! BNX2X_UPSTREAM */
	vmk_PCIDevice vmkDev;
#endif

	bp->vfdb = NULL;

	/* verify chip revision */
	if (CHIP_IS_E1x(bp))
		return 0;

	/* check if SRIOV support is turned off */
	if (!num_vfs_param)
		return 0;

	/* SRIOV can be enabled only with MSIX */
	if (int_mode_param == INT_MODE_MSI || int_mode_param == INT_MODE_INTx)
		BNX2X_ERR("Forced MSI/INTx mode is incompatible with SRIOV\n");

	err = -EIO;
	/* verify ari is enabled */
	if (!bnx2x_ari_enabled(bp->pdev)) {
		BNX2X_ERR("ARI not supported, SRIOV can not be enabled\n");
		return err;
	}

	/* verify igu is in normal mode */
	if (CHIP_INT_MODE_IS_BC(bp)) {
		BNX2X_ERR("IGU not normal mode,  SRIOV can not be enabled\n");
		return err;
	}

	/* allocate the vfs database */
	bp->vfdb = kzalloc(sizeof(*(bp->vfdb)), GFP_KERNEL);
	if (!bp->vfdb) {
		err = -ENOMEM;
		goto failed;
	}

	/*
	 * get the sriov info - Linux already collected all the pertinent
	 * information, however the sriov structure is for the private use
	 * of the pci module. Also we want this information regardless
	 *  of the hyper-visor.
	 */

	iov = &(bp->vfdb->sriov);
	err = bnx2x_sriov_info(bp, iov);
	if (err)
		goto failed;

	/* calcuate the actual number of VFs */
	iov->nr_virtfn = min_t(u16, iov->total, (u16)num_vfs_param);

	/* allcate the vf array */
	bp->vfdb->vfs = kzalloc(sizeof(struct bnx2x_virtf) *
				BNX2X_NR_VIRTFN(bp), GFP_KERNEL);
	if (!bp->vfdb->vfs) {
		err = -ENOMEM;
		goto failed;
	}

	/* Initial VF init - index and abs_vfid - nr_virtfn must be set */
	for_each_vf(bp, i) {
		bnx2x_vf(bp, i, index) = i;
		bnx2x_vf(bp, i, abs_vfid) = iov->first_vf_in_pf + i;
		bnx2x_vf(bp, i, state) = VF_FREE;
	}

	/* re-read the IGU CAM for VFs - index and abs_vfid must be set */
	bnx2x_get_vf_igu_cam_info(bp);

	/* get the total queue count and allocate the global queue arrays */
	queue_count = bnx2x_iov_get_max_queue_count(bp);

	/* allocate the queue arrays for all VFs */
	bp->vfdb->rxqs = kzalloc(sizeof(struct bnx2x_vfq) * queue_count,
				GFP_KERNEL);
	if (!bp->vfdb->rxqs) {
		err = -ENOMEM;
		goto failed;
	}
	bp->vfdb->txqs = kzalloc(sizeof(struct bnx2x_vfq) * queue_count,
				GFP_KERNEL);
	if (!bp->vfdb->txqs) {
		err = -ENOMEM;
		goto failed;
	}
	bp->vfdb->vfq_stats = kzalloc(sizeof(struct bnx2x_vfq_stats) *
				      queue_count , GFP_KERNEL);
	if (!bp->vfdb->vfq_stats) {
		err = -ENOMEM;
		goto failed;
	}

#ifdef __VMKLNX__ /* ! BNX2X_UPSTREAM */

	/* get the PCI device object */
	if (bnx2x_vmk_pci_dev(bp, &vmkDev)) {
		printk(KERN_ERR "Failed to get PCI device object");
		goto failed;
	}

	/* enable the VFs */
	if (vmk_PCIEnableVFs(vmkDev, &iov->nr_virtfn) != VMK_OK) {
		printk(KERN_ERR "Failed to enable VFs.\n");
		goto failed;
	}
	printk(KERN_ERR "VF num is %d\n", iov->nr_virtfn);
#else
	BNX2X_DEV_INFO("VF num is %d\n", iov->nr_virtfn);

	/* enable sriov */
	DP(BNX2X_MSG_IOV, "pci_enable_sriov(%d)\n", iov->nr_virtfn);
	err = pci_enable_sriov(bp->pdev, iov->nr_virtfn);
	if (err)
		goto failed;
#endif

	/* Final VF init */
	queue_count = 0;
	for_each_vf(bp, i) {
		struct bnx2x_virtf* vf = BP_VF(bp, i);

		/* fill in the BDF and bars */
		vf->bus = bnx2x_vf_bus(bp, i);
		vf->devfn = bnx2x_vf_devfn(bp, i);
		bnx2x_vf_set_bars(bp, vf);

		DP(BNX2X_MSG_IOV, "VF info[%d]: bus 0x%x, devfn 0x%x, "
		   "bar0 [0x%08x, %d], bar1 [0x%08x, %d], bar2 [0x%08x, %d]\n",
		   vf->abs_vfid, vf->bus, vf->devfn,
		   (unsigned)vf->bars[0].bar, vf->bars[0].size,
		   (unsigned)vf->bars[1].bar, vf->bars[1].size,
		   (unsigned)vf->bars[2].bar, vf->bars[2].size);

		/* set local queue arrays */
		vf->rxq = &bp->vfdb->rxqs[queue_count];
		vf->txq = &bp->vfdb->txqs[queue_count];
		vf->vfq_stats = &bp->vfdb->vfq_stats[queue_count];
		queue_count += vf->max_sb_count;
	}
	return 0;
failed:
	DP(BNX2X_MSG_IOV, "Failed err=%d\n", err);
	__bnx2x_iov_free_vfdb(bp);
	return err;
}

void __devexit bnx2x_iov_remove_one(struct bnx2x *bp)
{
#ifdef __VMKLNX__ /* ! BNX2X_UPSTREAM */
	vmk_PCIDevice vmkDev;
	VMK_ReturnStatus vmkRet;
#endif

	/* if SRIOV is not eanbled there's nothing to do */
	if (!IS_SRIOV(bp))
		return;

#ifdef __VMKLNX__ /* ! BNX2X_UPSTREAM */
	/* get the PCI device object */
	vmkRet = vmk_PCIGetPCIDevice(pci_domain_nr(bp->pdev->bus),
				     bp->pdev->bus->number,
				     PCI_SLOT(bp->pdev->devfn),
				     PCI_FUNC(bp->pdev->devfn),
				     &vmkDev);
	if (vmkRet != VMK_OK)
		printk(KERN_ERR "Failed to get PCI device object");
	else
		vmk_PCIDisableVFs(vmkDev);
#else
	/* disable SRIOV */
	pci_disable_sriov(bp->pdev);
#endif
	/* free vf database */
	__bnx2x_iov_free_vfdb(bp);
}

u8 bnx2x_iov_get_max_queue_count(struct bnx2x * bp)
{
	int i;
	u8 queue_count = 0;

	if (IS_SRIOV(bp))
		for_each_vf(bp, i)
			queue_count += bnx2x_vf(bp, i, max_sb_count);

	return queue_count;
}

void bnx2x_iov_free_mem(struct bnx2x* bp)
{
	int i;

	if (!IS_SRIOV(bp))
		return;

	/* free vfs hw contexts */
	for (i = 0; i < BNX2X_VF_CIDS/ILT_PAGE_CIDS ; i++) {
		struct hw_dma *cxt = &bp->vfdb->context[i];
		BNX2X_PCI_FREE(cxt->addr, cxt->mapping, cxt->size);
	}

	BNX2X_PCI_FREE(BP_VFDB(bp)->sp_dma.addr,
		       BP_VFDB(bp)->sp_dma.mapping,
		       BP_VFDB(bp)->sp_dma.size);

#ifdef VFPF_MBX /* BNX2X_UPSTREAM */
	BNX2X_PCI_FREE(BP_VF_MBX_DMA(bp)->addr,
		       BP_VF_MBX_DMA(bp)->mapping,
		       BP_VF_MBX_DMA(bp)->size);
#endif

}


int bnx2x_iov_alloc_mem(struct bnx2x* bp)
{
	size_t tot_size;
	int i, rc = 0;

	if (!IS_SRIOV(bp))
		return rc;

	/* allocate vfs hw contexts */
	tot_size = (BP_VFDB(bp)->sriov.first_vf_in_pf + BNX2X_NR_VIRTFN(bp)) *
		BNX2X_CIDS_PER_VF * sizeof(union cdu_context);

	for (i = 0; i < BNX2X_VF_CIDS/ILT_PAGE_CIDS ; i++) {
		struct hw_dma *cxt = BP_VF_CXT_PAGE(bp,i);
		cxt->size = min_t(size_t, tot_size, CDU_ILT_PAGE_SZ);

		if (cxt->size) {
			BNX2X_PCI_ALLOC(cxt->addr, &cxt->mapping, cxt->size);
		}
		else {
			cxt->addr = NULL;
			cxt->mapping = 0;
		}
		tot_size -= cxt->size;
	}

	/* allocate vfs ramrods dma memory - client_init and set_mac*/
	tot_size = BNX2X_NR_VIRTFN(bp) * sizeof(struct bnx2x_vf_sp);
	BNX2X_PCI_ALLOC(BP_VFDB(bp)->sp_dma.addr, &BP_VFDB(bp)->sp_dma.mapping,
			tot_size);
	BP_VFDB(bp)->sp_dma.size = tot_size;

#ifdef VFPF_MBX /* BNX2X_UPSTREAM */
	tot_size = BNX2X_NR_VIRTFN(bp) * MBX_MSG_ALIGNED_SIZE;
	BNX2X_PCI_ALLOC(BP_VF_MBX_DMA(bp)->addr, &BP_VF_MBX_DMA(bp)->mapping,
			tot_size);
	BP_VF_MBX_DMA(bp)->size = tot_size;

#endif
	return 0;

alloc_mem_err:
	return -ENOMEM;
}

/*
 * must be called after the number of PF queues and the number of VFs are
 * both known
 */
static void
bnx2x_iov_init_resc(struct bnx2x *bp, struct vf_pf_resc_request *resc)
{
	u16 vlan_count = 0;
	u8 max_queues = min_t(u8, BNX2X_VF_MAX_QUEUES , BNX2X_CIDS_PER_VF);

	resc->num_rxqs = resc->num_txqs = max_queues;

	/* no credit calculcis for macs (just yest) */
	resc->num_mac_filters = 1;

	/* divvy up vlan rules */
	vlan_count = bp->vlans_pool.check(&bp->vlans_pool);
	vlan_count = 1 << ilog2(vlan_count);
	resc->num_vlan_filters = vlan_count / BNX2X_NR_VIRTFN(bp);

	/* no real limitation */
	resc->num_mc_filters = 0;

	/* will be set per VF just before allocation */
	resc->num_sbs = 0;
}

/* called by bnx2x_nic_load */
int bnx2x_iov_nic_init(struct bnx2x *bp)
{
	int vfid;
	struct bnx2x_vfq *rxq;
	struct vf_pf_resc_request *resc;

	if (!IS_SRIOV(bp))
		return 0;

	/* initialize vf database */
	resc = &BP_VFDB(bp)->avail_resc;
	bnx2x_iov_init_resc(bp, resc);

	for_each_vf(bp, vfid) {
		int i;
		struct bnx2x_virtf* vf = BP_VF(bp, vfid);
		int base_vf_cid = (BP_VFDB(bp)->sriov.first_vf_in_pf + vfid) *
			BNX2X_CIDS_PER_VF;
		union cdu_context *base_cxt = (union cdu_context *)
			BP_VF_CXT_PAGE(bp, base_vf_cid/ILT_PAGE_CIDS)->addr +
			(base_vf_cid & (ILT_PAGE_CIDS-1));

		DP(BNX2X_MSG_IOV, "VF[%d] Max IGU SBs: %d, base vf cid 0x%x, "
				  "base cid 0x%x, base cxt %p\n",
		   vf->abs_vfid,
		   vf->max_sb_count,
		   base_vf_cid,
		   BNX2X_FIRST_VF_CID + base_vf_cid,
		   base_cxt);

		/* Initialize the queue contexts */
		vf->rxq_count = VF_MAX_QUEUE_CNT(vf);
		for_each_vf_rxq(vf, i) {
			rxq = VF_RXQ(vf, i);

			rxq->cxt = &((base_cxt + i)->eth);
			rxq->index = i;
			rxq->cid = BNX2X_FIRST_VF_CID + base_vf_cid + i;
			rxq->state =  BNX2X_FP_STATE_CLOSED;

			DP(BNX2X_MSG_IOV, "VF[%d] rxq[%d]: index %d, "
					  "cid 0x%x, cxt %p\n",
			   vf->abs_vfid, i,
			   rxq->index,
			   rxq->cid,
			   rxq->cxt);
		}

		vf->txq_count = VF_MAX_QUEUE_CNT(vf);
		for_each_vf_txq(vf, i) {
			struct bnx2x_vfq *txq = VF_TXQ(vf, i);
			txq->cxt = &((base_cxt + i)->eth);
			txq->index = i;
			txq->cid = BNX2X_FIRST_VF_CID + base_vf_cid + i;
		}

		/*
		 * init the classification objectsthe cl_id is bogus.
		 * It is allocated late when the VF is acquired
		 */

		/* mac, vlan  - per queue,
		 * but only the leading object is initialized
		 */
		rxq = VF_LEADING_RXQ(vf);

		bnx2x_init_mac_obj(bp, &rxq->mac_obj, 0xFF, rxq->cid,
				   FW_VF_HANDLE(vf->abs_vfid),
				   BP_VF_SP(bp, vf, mac_rdata),
				   BP_VF_SP_MAP(bp, vf, mac_rdata),
				   BNX2X_FILTER_MAC_PENDING,
				   &vf->filter_state,
				   BNX2X_OBJ_TYPE_RX_TX,
				   &bp->macs_pool);

		bnx2x_init_vlan_obj(bp, &rxq->vlan_obj, 0xFF, rxq->cid,
				    FW_VF_HANDLE(vf->abs_vfid),
				    BP_VF_SP(bp, vf, vlan_rdata),
				    BP_VF_SP_MAP(bp, vf, vlan_rdata),
				    BNX2X_FILTER_VLAN_PENDING,
				    &vf->filter_state,
				    BNX2X_OBJ_TYPE_RX_TX,
				    &bp->vlans_pool);

		/* mcast */
		bnx2x_init_mcast_obj(bp, &vf->mcast_obj, 0xFF, rxq->cid,
				     FW_VF_HANDLE(vf->abs_vfid),
				     BP_VF_SP(bp, vf, mcast_rdata),
				     BP_VF_SP_MAP(bp, vf, mcast_rdata),
				     BNX2X_FILTER_MCAST_PENDING,
				     &vf->filter_state,
				     BNX2X_OBJ_TYPE_RX_TX);

		/* PF TX switching mac object */
		bnx2x_init_mac_obj(bp, BP_TX_MAC_OBJ(bp), bnx2x_fp(bp, 0, cl_id),
				   bnx2x_fp(bp, 0, cid), BP_FUNC(bp),
				   bnx2x_sp(bp, tx_switch_mac_rdata),
				   bnx2x_sp_mapping(bp, tx_switch_mac_rdata),
				   BNX2X_FILTER_TX_SWITCH_MAC_PENDING,
				   &bp->sp_state, BNX2X_OBJ_TYPE_TX,
				   &bp->macs_pool);

		/* reserve the vf vlan credit */
		bp->vlans_pool.get(&bp->vlans_pool, resc->num_vlan_filters);
		vf->vlan_rules_count = resc->num_vlan_filters;

		vf->filter_state = 0;
		vf->leading_rss = -1;
		vf->sp_cl_id = bnx2x_fp(bp, 0, cl_id);

#ifdef VFPF_MBX /* BNX2X_UPSTREAM */
		/* set the mailbox message addresses */
		BP_VF_MBX(bp, vfid)->msg = (struct bnx2x_vf_mbx_msg*)
			((u8*)BP_VF_MBX_DMA(bp)->addr + vfid *
			MBX_MSG_ALIGNED_SIZE);

		BP_VF_MBX(bp, vfid)->msg_mapping = (dma_addr_t)
			((u8*)BP_VF_MBX_DMA(bp)->mapping + vfid *
			MBX_MSG_ALIGNED_SIZE);

		/* Enable vf mailbox */
		bnx2x_vf_enable_mbx(bp, vf->abs_vfid);
#endif
	}
	return 0;
}

/* called by bnx2x_chip_cleanup */
int bnx2x_iov_chip_cleanup(struct bnx2x *bp)
{
	struct list_head *head;
	struct bnx2x_list_elem *first;
	int i;

	if (!IS_SRIOV(bp))
		return 0;

	/* release all the VFs */
	for_each_vf(bp, i) {
		struct bnx2x_virtf *vf = BP_VF(bp, i);
		bnx2x_vf_release(bp, vf);
	}

	/* release iov TX mac rules */
	head = &BP_TX_MAC_OBJ(bp)->head;
	while (!list_empty(head)) {
		first = list_first_entry(head, struct bnx2x_list_elem, link);
		bnx2x_iov_set_tx_mac(bp, first->data.mac.mac, false);
	}
	return 0;
}


/* called by bnx2x_init_hw_func, returns the next ilt line */
int bnx2x_iov_init_ilt(struct bnx2x *bp, u16 line)
{
	int i;
	struct bnx2x_ilt* ilt = BP_ILT(bp);

	if (!IS_SRIOV(bp))
		return line;

	/* set vfs ilt lines */
	for (i = 0; i < BNX2X_VF_CIDS/ILT_PAGE_CIDS ; i++) {
		struct hw_dma *hw_cxt = BP_VF_CXT_PAGE(bp,i);
		ilt->lines[line+i].page = hw_cxt->addr;
		ilt->lines[line+i].page_mapping = hw_cxt->mapping;
		ilt->lines[line+i].size = hw_cxt->size; /* doesn't really matter */
	}
	return (line+i);
}



static inline u8 bnx2x_iov_is_vf_cid(struct bnx2x* bp, u16 cid)
{
	return ( (cid >= BNX2X_FIRST_VF_CID) &&
		 ((cid - BNX2X_FIRST_VF_CID) < BNX2X_VF_CIDS) );
}

static inline
void bnx2x_vf_handle_classification_eqe(struct bnx2x *bp,
					struct bnx2x_vfq *rxq,
					union event_ring_elem *elem)
{
	switch (elem->message.data.set_mac_event.echo >> BNX2X_SWCID_SHIFT) {
	case CLASSIFY_RULE_OPCODE_MAC:
		rxq->mac_obj.raw.clear_pending(&rxq->mac_obj.raw);
		break;
	case CLASSIFY_RULE_OPCODE_VLAN:
		rxq->vlan_obj.raw.clear_pending(&rxq->vlan_obj.raw);
		break;
	default:
		BNX2X_ERR("Unsupported classification command: %d\n",
			  elem->message.data.set_mac_event.echo);
		return;
	}
}

static inline
void bnx2x_vf_handle_mcast_eqe(struct bnx2x *bp,
			       struct bnx2x_virtf *vf)
{
	struct bnx2x_mcast_ramrod_params rparam = {0};
	int rc;

	rparam.mcast_obj = &vf->mcast_obj;

	vf->mcast_obj.raw.clear_pending(&vf->mcast_obj.raw);

	/* If there are pending mcast commands - send them */
	if (vf->mcast_obj.check_pending(&vf->mcast_obj)) {
		rc = bnx2x_config_mcast(bp, &rparam, true);
		if (rc)
			BNX2X_ERR("Failed to send pending mcast commands: %d\n",
				  rc);
	}
}

static inline
void bnx2x_vf_handle_filters_eqe(struct bnx2x *bp,
				 struct bnx2x_virtf *vf)
{
	smp_mb__before_clear_bit();
	clear_bit(BNX2X_FILTER_RX_MODE_PENDING, &vf->filter_state);
	smp_mb__after_clear_bit();
}


int bnx2x_iov_eq_sp_event(struct bnx2x* bp, union event_ring_elem *elem)
{
	struct bnx2x_virtf *vf;
	int qidx, abs_vfid;
	u8 opcode;
	u16 cid = 0xffff;

	if (!IS_SRIOV(bp))
		return 1;

	/* first get the cid - the only events we handle here are cfc-delete
	and set-mac completion */
	opcode = elem->message.opcode;

	switch (opcode) {
	case EVENT_RING_OPCODE_CFC_DEL:
		cid = SW_CID(elem->message.data.cfc_del_event.cid);
		break;
	case EVENT_RING_OPCODE_CLASSIFICATION_RULES:
	case EVENT_RING_OPCODE_MULTICAST_RULES:
	case EVENT_RING_OPCODE_FILTERS_RULES:
		cid = (elem->message.data.set_mac_event.echo &
		       BNX2X_SWCID_MASK);
		DP(BNX2X_MSG_IOV, "checking filtering comp cid=%d\n", cid);
		break;
	case EVENT_RING_OPCODE_VF_FLR:
		abs_vfid = elem->message.data.vf_flr_event.vf_id;
		goto get_vf;
	case EVENT_RING_OPCODE_MALICIOUS_VF:
		abs_vfid = elem->message.data.malicious_vf_event.vf_id;
		goto get_vf;
	default:
		return 1;
	}

	/* check if the cid is the VF range */
	if (!bnx2x_iov_is_vf_cid(bp, cid))
		return 1;
	/*
	 * extact vf and rxq index from vf_cid - relies on the following:
	 * 1. vfid on cid reflects the true abs_vfid
	 * 2. the max number of VFs (per path) is 64
	 */
	qidx = cid & ((1 << BNX2X_VF_CID_WND)-1);
	abs_vfid = (cid >> BNX2X_VF_CID_WND) & (BNX2X_MAX_NUM_OF_VFS-1);
get_vf:
	vf = bnx2x_vf_by_abs_fid(bp, abs_vfid);

	if (!vf) {
		BNX2X_ERR("EQ completion for unknown VF, cid %d, abs_vfid %d\n",
			  cid, abs_vfid);
		return 0;
	}

	switch (opcode) {
	case EVENT_RING_OPCODE_CFC_DEL:
		DP(BNX2X_MSG_IOV, "got VF [%d:%d] cfc del ramrod\n",
		   vf->abs_vfid, bnx2x_vf_rxq(vf, qidx, index));
		bnx2x_vf_rxq(vf, qidx, state) = BNX2X_FP_STATE_CLOSED;
		break;
	case EVENT_RING_OPCODE_CLASSIFICATION_RULES:
		DP(BNX2X_MSG_IOV, "got VF [%d:%d] set mac/vlan ramrod\n",
		   vf->abs_vfid, bnx2x_vf_rxq(vf, qidx, index));
		bnx2x_vf_handle_classification_eqe(bp, VF_RXQ(vf, qidx), elem);
		break;
	case EVENT_RING_OPCODE_MULTICAST_RULES:
		DP(BNX2X_MSG_IOV, "got VF [%d:%d] set mcast ramrod\n",
		   vf->abs_vfid, bnx2x_vf_rxq(vf, qidx, index));
		bnx2x_vf_handle_mcast_eqe(bp, vf);
		break;
	case EVENT_RING_OPCODE_FILTERS_RULES:
		DP(BNX2X_MSG_IOV, "got VF [%d:%d] set rx-mode ramrod\n",
		   vf->abs_vfid, bnx2x_vf_rxq(vf, qidx, index));
		bnx2x_vf_handle_filters_eqe(bp, vf);
		break;
	case EVENT_RING_OPCODE_VF_FLR:
		DP(BNX2X_MSG_IOV, "got VF [%d] FLR notification\n",
		   vf->abs_vfid);
		/* Do nothing */
		break;
	case EVENT_RING_OPCODE_MALICIOUS_VF:
		DP(BNX2X_MSG_IOV, "got VF [%d] MALICIOUS notification\n",
		   vf->abs_vfid);
		/* Do nothing */
		break;
	}
	return 0;
}

int bnx2x_iov_sp_event(struct bnx2x *bp, int vf_cid, int command)
{
	struct bnx2x_virtf *vf;
	struct bnx2x_vfq *rxq;
	int qidx, abs_vfid, rc;

	if (!IS_SRIOV(bp))
		return 1;

	/*
	 * extact vf and rxq index from vf_cid - relies on the following:
	 * 1. vfid on cid reflects the true abs_vfid
	 * 2. the max number of VFs (per path) is 64
	 */
	qidx = vf_cid & ((1 << BNX2X_VF_CID_WND)-1);
	abs_vfid = (vf_cid >> BNX2X_VF_CID_WND) & (BNX2X_MAX_NUM_OF_VFS-1);
	vf = bnx2x_vf_by_abs_fid(bp, abs_vfid);

	if (!vf) {
		BNX2X_ERR("Ramrod completion for unknown VF, cid %d\n",
			  vf_cid);
		return 0;
	}

	rc = 0;
	rxq = VF_RXQ(vf, qidx);

	/* switch on the command | state */
	switch (command | rxq->state) {
	case (RAMROD_CMD_ID_ETH_CLIENT_SETUP | BNX2X_FP_STATE_OPENING):
		DP(BNX2X_MSG_IOV, "got VF [%d:%d] setup ramrod\n",
		   vf->abs_vfid, rxq->index);
		rxq->state = BNX2X_FP_STATE_OPEN;
		goto get_out;

	case (RAMROD_CMD_ID_ETH_HALT | BNX2X_FP_STATE_HALTING):
		DP(BNX2X_MSG_IOV, "got VF [%d:%d] halt ramrod\n",
		   vf->abs_vfid, rxq->index);
		rxq->state = BNX2X_FP_STATE_HALTED;
		goto get_out;

	case (RAMROD_CMD_ID_ETH_TERMINATE | BNX2X_FP_STATE_TERMINATING):
		DP(BNX2X_MSG_IOV, "got VF [%d:%d] terminate ramrod (STOP)\n",
		   vf->abs_vfid, rxq->index);
		rxq->state = BNX2X_FP_STATE_TERMINATED;
		goto get_out;

	case (RAMROD_CMD_ID_ETH_TERMINATE |
	      (BNX2X_FP_STATE_OPEN | BNX2X_FP_SUB_STATE_TERMINATING)):
		DP(BNX2X_MSG_IOV, "got VF [%d:%d] terminate ramrod (FLR)\n",
		   vf->abs_vfid, rxq->index);
		rxq->state = BNX2X_FP_STATE_CLOSED;
		goto get_out;

	case (RAMROD_CMD_ID_ETH_CLIENT_UPDATE |
	      (BNX2X_FP_STATE_OPEN | BNX2X_FP_SUB_STATE_ACTIVATING)):
		DP(BNX2X_MSG_IOV, "got VF [%d:%d] activate ramrod\n",
		   vf->abs_vfid, rxq->index);
		rxq->state = (BNX2X_FP_STATE_OPEN |
			      BNX2X_FP_SUB_STATE_ACTIVATED);
		break;

	case (RAMROD_CMD_ID_ETH_CLIENT_UPDATE |
	      (BNX2X_FP_STATE_OPEN | BNX2X_FP_SUB_STATE_DEACTIVATING)):
		DP(BNX2X_MSG_IOV, "got VF [%d:%d] deactivate ramrod\n",
		   vf->abs_vfid, rxq->index);
		rxq->state = (BNX2X_FP_STATE_OPEN |
			      BNX2X_FP_SUB_STATE_DEACTIVATED);
		break;
	default:
		break;
	}

	/* switch on the command only */
	switch (command) {
	case (RAMROD_CMD_ID_ETH_CLIENT_UPDATE):
		rxq->update_pending = BNX2X_VF_UPDATE_DONE;
		break;
	default:
		rc = 1;
		break;
	}
get_out:
	return rc;
}


void bnx2x_iov_adjust_stats_req(struct bnx2x * bp)
{
	int i;
	dma_addr_t cur_data_offset;
	struct stats_query_entry *cur_query_entry;
	u8 stats_count = 0;

	if (!IS_SRIOV(bp))
		return;

	cur_data_offset = bp->fw_stats_data_mapping +
		offsetof(struct bnx2x_fw_stats_data, queue_stats) +
		BNX2X_NUM_ETH_QUEUES(bp) * sizeof(struct per_queue_stats);

	cur_query_entry = &bp->fw_stats_req->
		query[BNX2X_FIRST_QUEUE_QUERY_IDX + BNX2X_NUM_ETH_QUEUES(bp)];

	for_each_vf(bp, i) {
		int j;
		struct bnx2x_virtf *vf = BP_VF(bp, i);

		if (vf->state != VF_ENABLED)
			continue;

		for_each_vf_rxq(vf, j) {
			struct bnx2x_vfq *rxq = VF_RXQ(vf, j);

			if (rxq->state != (BNX2X_FP_STATE_OPEN |
					   BNX2X_FP_SUB_STATE_ACTIVATED))
				continue;

			cur_query_entry->kind = STATS_TYPE_QUEUE;
			cur_query_entry->index = vfq_cl_id(vf, rxq);
			cur_query_entry->funcID = cpu_to_le16(BP_FUNC(bp));
			cur_query_entry->address.hi =
				cpu_to_le32(U64_HI(cur_data_offset));
			cur_query_entry->address.lo =
				cpu_to_le32(U64_LO(cur_data_offset));

			cur_query_entry++;
			cur_data_offset += sizeof(struct per_queue_stats);
			stats_count++;
		}
	}
	bp->fw_stats_req->hdr.cmd_num = bp->fw_stats_num + stats_count;
}

/* statistics collection */
static void bnx2x_iov_qstats_update(struct bnx2x *bp,
				    struct bnx2x_vfq_stats *vfq_stats,
				    struct per_queue_stats *fw_qstats)
{
	struct tstorm_per_queue_stats *tclient =
		&fw_qstats->tstorm_queue_statistics;
	struct tstorm_per_queue_stats *old_tclient = &vfq_stats->old_tclient;

	struct ustorm_per_queue_stats *uclient =
		&fw_qstats->ustorm_queue_statistics;
	struct ustorm_per_queue_stats *old_uclient = &vfq_stats->old_uclient;

	struct xstorm_per_queue_stats *xclient =
		&fw_qstats->xstorm_queue_statistics;
	struct xstorm_per_queue_stats *old_xclient = &vfq_stats->old_xclient;

	struct bnx2x_vfq_fw_stats *qstats = &vfq_stats->qstats;
	u32 diff;

	/*
	DP(BNX2X_MSG_IOV, "ucast_sent 0x%x, bcast_sent 0x%x"
			  " mcast_sent 0x%x\n",
	   xclient->ucast_pkts_sent, xclient->bcast_pkts_sent,
	   xclient->mcast_pkts_sent);

	DP(BNX2X_MSG_IOV, "ucast_rcvd 0x%x ucast_dropped 0x%x\n",
	   tclient->rcv_ucast_pkts, uclient->ucast_no_buff_pkts);

	DP(BNX2X_MSG_IOV, "---------------\n");
	*/

	/* RX T-packets */
	UPDATE_EXTEND_TSTAT(rcv_ucast_pkts,
				total_unicast_packets_received);
	UPDATE_EXTEND_TSTAT(rcv_mcast_pkts,
				total_multicast_packets_received);
	UPDATE_EXTEND_TSTAT(rcv_bcast_pkts,
				total_broadcast_packets_received);
	UPDATE_EXTEND_TSTAT(pkts_too_big_discard,
				etherstatsoverrsizepkts);
	UPDATE_EXTEND_TSTAT(no_buff_discard, no_buff_discard);
	UPDATE_EXTEND_TSTAT(checksum_discard, error_discard);
	UPDATE_EXTEND_TSTAT(ttl0_discard, error_discard);

	/* RX T-bytes */
	qstats->total_broadcast_bytes_received_hi =
		le32_to_cpu(tclient->rcv_bcast_bytes.hi);
	qstats->total_broadcast_bytes_received_lo =
		le32_to_cpu(tclient->rcv_bcast_bytes.lo);

	qstats->total_multicast_bytes_received_hi =
		le32_to_cpu(tclient->rcv_mcast_bytes.hi);
	qstats->total_multicast_bytes_received_lo =
		le32_to_cpu(tclient->rcv_mcast_bytes.lo);

	qstats->total_unicast_bytes_received_hi =
		le32_to_cpu(tclient->rcv_ucast_bytes.hi);
	qstats->total_unicast_bytes_received_lo =
		le32_to_cpu(tclient->rcv_ucast_bytes.lo);

	/*
	DP(BNX2X_MSG_IOV, "0-ucast_rcvd_hi 0x%x ucast_rcvd_lo 0x%x\n",
	   qstats->total_unicast_packets_received_hi,
	   qstats->total_unicast_packets_received_lo);
	*/

	/* RX U-packets */
	SUB_EXTEND_USTAT(ucast_no_buff_pkts,
				total_unicast_packets_received);

	/*
	DP(BNX2X_MSG_IOV, "1-ucast_rcvd_hi 0x%x ucast_rcvd_lo 0x%x\n",
	   qstats->total_unicast_packets_received_hi,
	   qstats->total_unicast_packets_received_lo);

	DP(BNX2X_MSG_IOV, "+++++++++++++++++++\n");
	*/

	SUB_EXTEND_USTAT(mcast_no_buff_pkts,
				total_multicast_packets_received);
	SUB_EXTEND_USTAT(bcast_no_buff_pkts,
				total_broadcast_packets_received);
	UPDATE_EXTEND_USTAT(ucast_no_buff_pkts, no_buff_discard);
	UPDATE_EXTEND_USTAT(mcast_no_buff_pkts, no_buff_discard);
	UPDATE_EXTEND_USTAT(bcast_no_buff_pkts, no_buff_discard);

	/* RX U-bytes */
	SUB_64(qstats->total_broadcast_bytes_received_hi,
	       le32_to_cpu(uclient->bcast_no_buff_bytes.hi),
	       qstats->total_broadcast_bytes_received_lo,
	       le32_to_cpu(uclient->bcast_no_buff_bytes.lo));

	SUB_64(qstats->total_multicast_bytes_received_hi,
	       le32_to_cpu(uclient->mcast_no_buff_bytes.hi),
	       qstats->total_multicast_bytes_received_lo,
	       le32_to_cpu(uclient->mcast_no_buff_bytes.lo));

	SUB_64(qstats->total_unicast_bytes_received_hi,
	       le32_to_cpu(uclient->ucast_no_buff_bytes.hi),
	       qstats->total_unicast_bytes_received_lo,
	       le32_to_cpu(uclient->ucast_no_buff_bytes.lo));

	/* TX packets */
	UPDATE_EXTEND_XSTAT(ucast_pkts_sent,
				total_unicast_packets_sent);
	UPDATE_EXTEND_XSTAT(mcast_pkts_sent,
				total_multicast_packets_sent);
	UPDATE_EXTEND_XSTAT(bcast_pkts_sent,
				total_broadcast_packets_sent);
	UPDATE_EXTEND_XSTAT(error_drop_pkts, tx_error_packets);

	/* TX bytes */
	qstats->total_broadcast_bytes_sent_hi =
		le32_to_cpu(xclient->bcast_bytes_sent.hi);
	qstats->total_broadcast_bytes_sent_lo =
		le32_to_cpu(xclient->bcast_bytes_sent.lo);

	qstats->total_multicast_bytes_sent_hi =
		le32_to_cpu(xclient->mcast_bytes_sent.hi);
	qstats->total_multicast_bytes_sent_lo =
		le32_to_cpu(xclient->mcast_bytes_sent.lo);

	qstats->total_unicast_bytes_sent_hi =
		le32_to_cpu(xclient->ucast_bytes_sent.hi);
	qstats->total_unicast_bytes_sent_lo =
		le32_to_cpu(xclient->ucast_bytes_sent.lo);

}

static inline
struct bnx2x_virtf *__vf_from_stat_id(struct bnx2x *bp, u8 stat_id)
{
	int i;
	struct bnx2x_virtf *vf = NULL;

	for_each_vf(bp, i) {
		vf = BP_VF(bp, i);
		if (stat_id >= vf->igu_base_id &&
		    stat_id < vf->igu_base_id + vf->max_sb_count)
			break;
	}
	return vf;

}

void bnx2x_iov_storm_stats_update(struct bnx2x *bp)
{
	int i;
	u8 stats_count;
	struct stats_query_entry *cur_query_entry;
	struct per_queue_stats *cur_qstats;

	if (!IS_SRIOV(bp))
		return;

	stats_count =  bp->fw_stats_req->hdr.cmd_num - bp->fw_stats_num;
	cur_query_entry = &bp->fw_stats_req->query[bp->fw_stats_num];
	cur_qstats = &bp->fw_stats_data->queue_stats[BNX2X_NUM_ETH_QUEUES(bp)];

	/*DP(BNX2X_MSG_IOV, "iov stats count %d\n", stats_count);*/

	/* iterate on all vf queues in the current request and collect */
	for (i = 0; i < stats_count; i++, cur_query_entry++, cur_qstats++) {
		u8 idx;
		u8 stat_id = cur_query_entry->index;
		struct bnx2x_virtf *vf = __vf_from_stat_id(bp, stat_id);

		if (!vf) {
			BNX2X_ERR("Invalid statistics index %d on request\n",
				  stat_id);
			continue;
		}

		idx = cur_query_entry->index - vf->igu_base_id;

		/*DP(BNX2X_MSG_IOV, "VF[%d:%d] updating stats, local index %d\n",
		   vf->abs_vfid, stat_id, idx);*/

		bnx2x_iov_qstats_update(bp, &vf->vfq_stats[idx], cur_qstats);
	}
}


/*
 * VF API helpers
 */
static void bnx2x_vf_qtbl_set_q(struct bnx2x *bp, u8 abs_vfid, u8 qid,
				u8 enable)
{
	u32 reg = PXP_REG_HST_ZONE_PERMISSION_TABLE + qid * 4;
	u32 val = enable ? (abs_vfid | (1 << 6)) : 0;
	REG_WR(bp, reg, val);
}

static void bnx2x_vf_clr_qtbl(struct bnx2x *bp, struct bnx2x_virtf *vf)
{
	int i;

	for_each_vf_rxq(vf, i) {
		struct bnx2x_vfq *rxq = VF_RXQ(vf, i);
		bnx2x_vf_qtbl_set_q(bp, vf->abs_vfid, vfq_qzone_id(vf,rxq),
				    false);
	}
}

static inline void bnx2x_vf_igu_txq_enable(struct bnx2x *bp,
					   struct bnx2x_vfq *txq)
{
}

void bnx2x_vf_igu_disable(struct bnx2x *bp, struct bnx2x_virtf *vf)
{
	u32 val;

	/* clear the VF configuarion - pretend */
	bnx2x_pretend_func(bp, HW_VF_HANDLE(bp, vf->abs_vfid));
	val = REG_RD(bp, IGU_REG_VF_CONFIGURATION);
	val &= ~(IGU_VF_CONF_MSI_MSIX_EN | IGU_VF_CONF_SINGLE_ISR_EN |
		 IGU_VF_CONF_FUNC_EN | IGU_VF_CONF_PARENT_MASK);
	REG_WR(bp, IGU_REG_VF_CONFIGURATION, val);
	bnx2x_pretend_func(bp, BP_ABS_FUNC(bp));
}


static inline
int bnx2x_vf_chk_avail_resc(struct vf_pf_resc_request *req_resc,
			    struct vf_pf_resc_request *avail_resc)
{
	return ((req_resc->num_rxqs <= avail_resc->num_rxqs) &&
		(req_resc->num_txqs <= avail_resc->num_txqs) &&
		(req_resc->num_sbs <= avail_resc->num_sbs)   &&
		(req_resc->num_mac_filters <= avail_resc->num_mac_filters) &&
		(req_resc->num_vlan_filters <= avail_resc->num_vlan_filters));
}

/*
 * 	CORE VF API
 */
vf_api_t bnx2x_vf_acquire(struct bnx2x *bp, struct bnx2x_virtf *vf,
			  struct vf_pf_resc_request *resc)
{
	/*
	 * if state is 'acquired' the VF was not released or FLR'd, in
	 * this case the returned resources match the aquired already
	 * acquired resources. Verify that the requested numbers do
	 * not exceed the already acquired numbers.
	 */
	if (vf->state == VF_ACQUIRED) {
		DP(BNX2X_MSG_IOV, "VF[%d] Trying to re-acquire resources "
			"(VF was not released or FLR'd)\n", vf->abs_vfid);

		if (!bnx2x_vf_chk_avail_resc(resc, &vf->alloc_resc)) {
			BNX2X_ERR("VF[%d] When re-acquiring resources, "
				  "requested numbers must be <= then "
				  "previously acquired numbers\n",
				  vf->abs_vfid);
			return VF_API_FAILURE;
		}
		return VF_API_SUCCESS;
	}

	/* Otherwise vf state must be 'free' or 'reset' */
	if (vf->state != VF_FREE && vf->state != VF_RESET) {
		BNX2X_ERR("VF[%d] Can not acquire a VF with state %d\n",
			  vf->abs_vfid, vf->state);
		return VF_API_FAILURE;
	}

	/*
	 * static allocation:
	 * the global maximum number are fixed per VF.
	 * fail the request if requested number exceed these globals
	 */

	/* mac SBs is according to the igu CAM */
	BP_VFDB(bp)->avail_resc.num_sbs = vf->max_sb_count;

	if (!bnx2x_vf_chk_avail_resc(resc, &BP_VFDB(bp)->avail_resc)) {
		/* set the max resource in the vf */
		return VF_API_NO_RESOURCE;
	}

	vf->rxq_count = resc->num_rxqs;
	vf->txq_count = resc->num_txqs;
	vf->sb_count = resc->num_sbs;

	/* a request for 0 macs/vlans means you get the max available */
	if (resc->num_mac_filters)
		vf->mac_rules_count = resc->num_mac_filters;
	if (resc->num_vlan_filters)
		vf->vlan_rules_count = resc->num_vlan_filters;

	vf->state = VF_ACQUIRED;
	return VF_API_SUCCESS;

}

vf_api_t bnx2x_vf_init(struct bnx2x *bp, struct bnx2x_virtf *vf, u64 *sb_map)
{
	struct bnx2x_func_init_params func_init = {0};
	struct bnx2x_rss_params rss = {0};
	u16 flags = 0;
	int i;

	/*
	 * the sb resources are initializied at this point, do the
	 * FW/HW initializations
	 */
	for_each_vf_sb(vf, i)
		bnx2x_init_sb(bp, (dma_addr_t)sb_map[i], vf->abs_vfid, true,
			      __vf_fw_sb(vf,i), __vf_igu_sb(vf,i));

	/* Sanity checks */
	if (vf->state != VF_ACQUIRED) {
		DP(BNX2X_MSG_IOV, "VF[%d] is not in the acquired state\n",
		   vf->abs_vfid);
		return VF_API_FAILURE;
	}
#ifndef VFPF_MBX /* BNX2X_UPSTREAM */
	/* FLR cleanup epilogue */
	if (bnx2x_vf_flr_clnup_epilog(bp, vf->abs_vfid))
		return VF_API_FAILURE;;
#endif

	/* reset IGU VF statistics: MSIX */
	REG_WR(bp, IGU_REG_STATISTIC_NUM_MESSAGE_SENT + vf->abs_vfid * 4 , 0);

	/* vf init */
	if (vf->cfg_flags & VF_CFG_STATS)
		flags |= (FUNC_FLG_STATS | FUNC_FLG_SPQ);

	if (vf->cfg_flags & VF_CFG_TPA)
		flags |= FUNC_FLG_TPA;

	if (is_vf_multi(vf))
		flags |= FUNC_FLG_RSS;

	/* function setup */
	if (flags & FUNC_FLG_RSS) {
		rss.cap = (RSS_IPV4_CAP | RSS_IPV4_TCP_CAP |
			   RSS_IPV6_CAP | RSS_IPV6_TCP_CAP);
		rss.mode = ETH_RSS_MODE_REGULAR;
		rss.result_mask = MULTI_MASK;
		func_init.rss = &rss;
	}
	func_init.func_flgs = flags;
	func_init.pf_id = BP_FUNC(bp);
	func_init.func_id = FW_VF_HANDLE(vf->abs_vfid);
	func_init.fw_stat_map = vf->fw_stat_map;
	func_init.spq_map = vf->spq_map;
	func_init.spq_prod = 0;
	bnx2x_func_init(bp, &func_init);

	/* Configure RSS TBD */

	/* Enable the vf */
#ifndef VFPF_MBX /* BNX2X_UPSTREAM */
	bnx2x_vf_enable_access(bp, vf->abs_vfid);
#endif
	bnx2x_vf_enable_traffic(bp, vf);

	/* queue protection table */
	for_each_vf_rxq(vf, i) {
		struct bnx2x_vfq *rxq = VF_RXQ(vf, i);
		bnx2x_vf_qtbl_set_q(bp, vf->abs_vfid,
				    vfq_qzone_id(vf, rxq), true);
	}

	/* zero stats */
	for_each_vf_rxq(vf, i)
		memset(&vf->vfq_stats[i], 0 ,sizeof(struct bnx2x_vfq_stats));

	vf->state = VF_ENABLED;

	return VF_API_SUCCESS;
}

vf_api_t bnx2x_vf_txq_setup(struct bnx2x *bp, struct bnx2x_virtf *vf,
			    struct bnx2x_vfq *txq,
			    struct bnx2x_client_init_params *p)
{
	/*
	 * init queue hw and fw
	 * all the VF originated parameters are already set on the txq_init
	 * variable.
	 */
	p->txq_params.cxt = txq->cxt;
	p->txq_params.fw_sb_id = vfq_fw_sb_id(vf,txq);
	p->txq_params.cid = HW_CID(bp, txq->cid);

#ifdef BNX2X_VF_LOCAL_SWITCH
	/* always enable Tx-switching and security for VFs */
	set_bit(BNX2X_QUEUE_FLG_TX_SWITCH, &p->txq_params.flags);
	set_bit(BNX2X_QUEUE_FLG_TX_SEC, &p->txq_params.flags);
#endif

	/* enable interrupts */
	bnx2x_vf_igu_ack_sb(bp, vf, vfq_igu_sb_id(vf,txq), USTORM_ID, 0,
			    IGU_INT_ENABLE, 0);

	return VF_API_SUCCESS;
}

vf_api_t bnx2x_vf_rxq_setup(struct bnx2x *bp, struct bnx2x_virtf *vf,
			    struct bnx2x_vfq *rxq,
			    struct bnx2x_client_init_params *p,
			    u8 activate)
{
	vf_api_t rc = VF_API_SUCCESS;
	u8 cl_id = vfq_cl_id(vf, rxq);

	if (vf->cfg_flags & VF_CFG_FW_FC) {
		/* set pause params - no spport for now */
		BNX2X_ERR("No support for pause to VFs (abs_vfid - %d)\n",
			  vf->abs_vfid);
	}

	/*
	 * init queue (client) hw and fw
	 * all the VF originated parameters are already set on the rxq_init
	 * variable.
	 */

	/*
	 * note: the pause thresholds are set by default to 0 which
	 * effectively turns off the feature for this client. This is very
	 * important - we can not allow one client (VF) to influence another
	 * client (another VF)
	 */

	p->rxq_params.cid = HW_CID(bp, rxq->cid);
	p->rxq_params.cxt = rxq->cxt;
	p->rxq_params.fw_sb_id = vfq_fw_sb_id(vf, rxq);
	p->rxq_params.cl_qzone_id = vfq_qzone_id(vf, rxq);
	p->rxq_params.cl_id = cl_id;
	p->rxq_params.spcl_id = vf->sp_cl_id;

	/* Tell the FW to collect stats */
	set_bit(BNX2X_QUEUE_FLG_STATS, &p->rxq_params.flags);
	set_bit(BNX2X_QUEUE_FLG_ZERO_STATS, &p->rxq_params.flags);
	p->rxq_params.stat_id = vfq_stat_id(vf, rxq);

	/* Tell the FW to handle OV */
	if (IS_MF_SD(bp))
		set_bit(BNX2X_QUEUE_FLG_OV, &p->rxq_params.flags);


	if (test_bit(BNX2X_QUEUE_FLG_TPA, &p->rxq_params.flags))
		p->rxq_params.max_tpa_queues = BNX2X_VF_MAX_TPA_AGG_QUEUES;

	/* set values pertinent only for the 'leading' rxq:
	 * 1. tss leading client id and the
	 * 2. vf mcast object client id
	 * 3. rxq mac object client id
	 * 4. rxq vlan object client id
	 */
	if (VF_IS_LEADING_RXQ(rxq)) {
		set_bit(CLIENT_IS_LEADING_RSS, &p->ramrod_params.client_flags);
		set_bit(CLIENT_IS_MULTICAST, &p->ramrod_params.client_flags);
		vf->leading_rss = cl_id;
		vf->mcast_obj.raw.cl_id = cl_id;
		rxq->mac_obj.raw.cl_id = cl_id;
		rxq->vlan_obj.raw.cl_id = cl_id;
	}

	p->txq_params.tss_leading_cl_id = vf->leading_rss;

	/* dispatch the setup ramrod */
	p->ramrod_params.state = BNX2X_FP_STATE_OPEN;
	p->ramrod_params.pstate = &rxq->state;
	p->ramrod_params.cl_id = cl_id;
	p->ramrod_params.cid = rxq->cid;
	p->ramrod_params.index = rxq->index;

	if (bnx2x_setup_fw_client(bp, p, activate,
				  BP_VF_SP(bp, vf, client_init),
				  BP_VF_SP_MAP(bp, vf, client_init)))
		rc = VF_API_FAILURE;

	/* enable interrupts */
	bnx2x_vf_igu_ack_sb(bp, vf, vfq_igu_sb_id(vf, rxq), USTORM_ID, 0,
			    IGU_INT_ENABLE, 0);

	return rc;
}

vf_api_t bnx2x_vfq_teardown(struct bnx2x *bp, struct bnx2x_virtf *vf,
			       struct bnx2x_vfq *rxq)
{
	struct bnx2x_client_ramrod_params client_stop = {0};

	/* skip if unused */
	if ((rxq->state & ~BNX2X_FP_SUB_STATE_MASK) != BNX2X_FP_STATE_OPEN)
		return VF_API_FAILURE;

	/* rx mac filtering - set to drop all */
	bnx2x_vf_set_rxq_mode(bp, vf, rxq, 0);

	bnx2x_vfq_sw_cleanup(bp, vf, rxq, false);

	/* deactivate the queue  TODO */

	/* stop the queue - halt + delete cfc */
	client_stop.pstate = &rxq->state;
	client_stop.index = rxq->index;
	client_stop.cl_id = vfq_cl_id(vf, rxq);
	client_stop.cid = rxq->cid;
	bnx2x_stop_fw_client(bp, &client_stop);

	return VF_API_SUCCESS;
}

vf_api_t bnx2x_vf_close(struct bnx2x *bp ,struct bnx2x_virtf *vf)
{
	int i;

	/* tear down all queues (that are already setup) */
	for_each_vf_rxq(vf, i)
		bnx2x_vfq_teardown(bp, vf, VF_RXQ(vf, i));

	/* disable the interrupts */
	bnx2x_vf_igu_disable(bp, vf);

	/* disable the vf */
	bnx2x_vf_clr_qtbl(bp, vf);
	bnx2x_vf_disable(bp, vf->abs_vfid);

	vf->state = VF_ACQUIRED;

	return VF_API_SUCCESS;
}

/*
 * VF relaese can be called either:
 * 1. the VF was acquired but not loaded
 * 2. the vf was loaded or in the process of loading
 */
void bnx2x_vf_release(struct bnx2x *bp, struct bnx2x_virtf *vf)
{
	switch(vf->state) {
	case VF_ENABLED:
		bnx2x_vf_close(bp, vf);
	case VF_ACQUIRED:
		bnx2x_vf_free_resc(bp, vf);
		vf->state = VF_FREE;
		break;
	case VF_FREE:
	case VF_RESET:
		/* do nothing */
		break;
	default:
		BNX2X_ERR("Illegal state for VF[%d:%d]\n",
			  vf->index, vf->abs_vfid);
		break;
	}
}


vf_api_t bnx2x_vf_trigger_q(struct bnx2x *bp ,struct bnx2x_virtf *vf,
			    struct bnx2x_vfq *rxq, u8 activate)
{

	vf_api_t rc = VF_API_FAILURE;
	struct client_update_ramrod_data *client_update;

	/* activate/ deactivate only open queues */
	if ((rxq->state & ~BNX2X_FP_SUB_STATE_MASK) != BNX2X_FP_STATE_OPEN)
		return rc;

	rxq->state |= activate ? BNX2X_FP_SUB_STATE_ACTIVATING :
		BNX2X_FP_SUB_STATE_DEACTIVATING;

	client_update = BP_VF_SP(bp, vf, client_update);
	memset(client_update, 0, sizeof(*client_update));

	client_update->client_id = vfq_cl_id(vf, rxq);
	client_update->activate_flg = activate;
	client_update->activate_change_flg = 1;

	if (!bnx2x_vf_queue_update_ramrod(bp, rxq,
					  BP_VF_SP_MAP(bp, vf, client_update),
					  true)) /* block */
		rc = VF_API_SUCCESS;

	return rc;
}

vf_api_t bnx2x_vf_trigger(struct bnx2x *bp ,struct bnx2x_virtf *vf, u8 activate)
{
	int i;
	vf_api_t rc = VF_API_SUCCESS;

	for_each_vf_rxq(vf, i) {
		struct bnx2x_vfq * rxq = VF_RXQ(vf, i);
		rc = bnx2x_vf_trigger_q(bp, vf, rxq, activate);
		if (rc) {
			BNX2X_ERR("VF[%d] Error - Failed to %s queue %d\n",
				  vf->abs_vfid,
				  (activate ? "activate" : "quiesce"),
				  rxq->index);
			break;
		}
	}
	return rc;
}

int bnx2x_iov_set_tx_mac(struct bnx2x *bp, u8 *mac, bool add)
{
	unsigned long ramrod_flags = 0;
	set_bit(RAMROD_COMP_WAIT, &ramrod_flags);

	return bnx2x_set_mac_one(bp, mac, BP_TX_MAC_OBJ(bp), add,
				 BNX2X_ETH_MAC, ramrod_flags);
}

bool bnx2x_vf_check_vlan_op(struct bnx2x *bp, struct bnx2x_vfq *rxq,
			    u16 vtag, bool chk_add)
{
	struct bnx2x_vlan_mac_ramrod_params ramrod_param;

	memset(&ramrod_param, 0, sizeof(ramrod_param));

	ramrod_param.data.vlan.vlan = vtag;
	ramrod_param.vlan_mac_obj = &rxq->vlan_obj;

	if (chk_add)
		return rxq->vlan_obj.check_add(&ramrod_param);
	else
		return (rxq->vlan_obj.check_del(&ramrod_param) ? true : false);
}

vf_api_t bnx2x_vf_clear_vlans(struct bnx2x *bp, struct bnx2x_vfq *rxq,
			      bool skip, u16 vlan_to_skip,
			      unsigned long ramrod_flags)
{
	struct list_head *head;
	struct bnx2x_list_elem *first;
	u16 first_vlan;
	vf_api_t rc = VF_API_SUCCESS;

	/* release iov TX mac rules */
	head =  &rxq->vlan_obj.head;
	while ( !list_empty(head) && !rc) {
		first = list_first_entry(head, struct bnx2x_list_elem, link);
		first_vlan = first->data.vlan.vlan;

		/* skip */
		if (skip && vlan_to_skip == first_vlan) {
			if (list_is_singular(head))
				break;
			list_rotate_left(head);
			continue;
		}

		/* remove */
		rc = bnx2x_vf_set_vlan(bp, rxq, first_vlan, false,
				       ramrod_flags);
		if (first_vlan)
			atomic_dec(&rxq->vlan_count);
	}
	return rc;
}

vf_api_t bnx2x_vf_set_vlan(struct bnx2x *bp, struct bnx2x_vfq *rxq,
			   u16 vtag, bool add, unsigned long ramrod_flags)
{
	/*
	 * set the vlan do not consume credit -
	 * the VF reservs vlan credit in advance.
	 */
	int rc = bnx2x_set_vlan_one(bp, vtag, &rxq->vlan_obj, add,
				    ramrod_flags, false);
	if (rc)
		return VF_API_FAILURE;

	return VF_API_SUCCESS;
}

vf_api_t bnx2x_vf_set_mac(struct bnx2x *bp, struct bnx2x_vfq *rxq,
			  u8 *mac, bool add, unsigned long ramrod_flags)
{
	int rc = bnx2x_set_mac_one(bp, mac, &rxq->mac_obj, add, BNX2X_ETH_MAC,
				   ramrod_flags);
	if (rc)
		return VF_API_FAILURE;

	return VF_API_SUCCESS;
}

vf_api_t bnx2x_vf_set_mcasts(struct bnx2x *bp, struct bnx2x_virtf *vf,
			     bnx2x_mac_addr_t *mcasts, int mcast_num,
			     bool drv_only)
{
	int i, rc;
	struct bnx2x_mcast_list_elem *mc_mac = NULL;
	struct bnx2x_mcast_ramrod_params rparam = {0};

	/* allocate memory for mcast list */
	if (mcast_num) {
		mc_mac = kzalloc(sizeof(*mc_mac) * mcast_num, GFP_ATOMIC);
		if (!mc_mac) {
			BNX2X_ERR("Failed to create multicast MACs "
				  "list\n");
			return VF_API_NO_RESOURCE;
		}
	}

	rparam.mcast_obj = &vf->mcast_obj;
	if (drv_only)
		set_bit(RAMROD_DRV_CLR_ONLY, &rparam.ramrod_flags);

	/* clear existing mcasts */
	rc = bnx2x_config_mcast(bp, &rparam, false);
	if (rc) {
		BNX2X_ERR("Failed to clear multicast "
			  "configuration: %d\n", rc);
		return VF_API_FAILURE;
	}

	/* set mcasts */
	if (mcast_num) {
		/* fill in list */
		INIT_LIST_HEAD(&rparam.mcast_list);
		for (i = 0; i < mcast_num; i++) {
			mc_mac->mac = mcasts[i];
			list_add_tail(&mc_mac->link, &rparam.mcast_list);
			mc_mac++;
		}
		rparam.mcast_list_len = mcast_num;

		rc = bnx2x_config_mcast(bp, &rparam, true);
		if (rc) {
			BNX2X_ERR("Failed to set a new multicast "
				  "configuration: %d\n", rc);
			return VF_API_FAILURE;
		}

		/* free allocated list */
		mc_mac = list_first_entry(&rparam.mcast_list,
					  struct bnx2x_mcast_list_elem, link);
		kfree(mc_mac);
	}
	return VF_API_SUCCESS;
}

vf_api_t bnx2x_vf_set_rxq_mode(struct bnx2x* bp,struct bnx2x_virtf *vf,
			       struct bnx2x_vfq *rxq,
			       unsigned long accept_flags)
{
	struct bnx2x_rx_mode_ramrod_params ramrod_param;

	memset(&ramrod_param, 0, sizeof(ramrod_param));

	/* Prepare ramrod parameters */
	ramrod_param.cid = rxq->cid;
	ramrod_param.cl_id = vfq_cl_id(vf, rxq);
	ramrod_param.rx_mode_obj = &bp->rx_mode_obj;
	ramrod_param.func_id = FW_VF_HANDLE(vf->abs_vfid);

	ramrod_param.accept_flags = accept_flags;
	ramrod_param.pstate = &vf->filter_state;
	ramrod_param.state = BNX2X_FILTER_RX_MODE_PENDING;

	set_bit(BNX2X_FILTER_RX_MODE_PENDING, &vf->filter_state);
	set_bit(RAMROD_RX, &ramrod_param.ramrod_flags);
	set_bit(RAMROD_TX, &ramrod_param.ramrod_flags);
	set_bit(RAMROD_COMP_WAIT, &ramrod_param.ramrod_flags);

	ramrod_param.rdata = BP_VF_SP(bp, vf, rx_mode_rdata.e2);
	ramrod_param.rdata_mapping = BP_VF_SP_MAP(bp, vf, rx_mode_rdata.e2);

	if(bnx2x_config_rx_mode(bp, &ramrod_param)) {
		BNX2X_ERR("Set rx_mode %d failed\n", bp->rx_mode);
		return VF_API_FAILURE;
	}
	return VF_API_SUCCESS;
}

u8 inline bnx2x_vfid_valid(struct bnx2x* bp, int vfid)
{
	return (bp->vfdb && vfid <= BNX2X_NR_VIRTFN(bp));
}

void inline bnx2x_vf_get_sbdf(struct bnx2x *bp, struct bnx2x_virtf *vf, u32* sbdf)
{
	*sbdf = vf->devfn | (vf->bus << 8);
}

void inline bnx2x_vf_get_bars(struct bnx2x *bp, struct bnx2x_virtf *vf,
		       struct bnx2x_vf_bar_info *bar_info)
{

	int n;
	bar_info->nr_bars = bp->vfdb->sriov.nres;
	for (n = 0; n < bar_info->nr_bars; n++) {
		bar_info->bars[n] = vf->bars[n];
	}
}

#ifdef VFPF_MBX /* BNX2X_UPSTREAM */

/*
 * 	VF MBX (aka vf-pf channel)
 */

/* this works only on E2 */
int bnx2x_copy32_vf_dmae(struct bnx2x *bp, u8 from_vf, dma_addr_t pf_addr,
			 u8 vfid, u32 vf_addr_hi, u32 vf_addr_lo, u32 len32)
{
	struct dmae_command dmae;
	int rc = 0;

	if (CHIP_IS_E1x(bp)) {
		BNX2X_ERR("Chip revision does not support VFs\n");
		return DMAE_NOT_RDY;
	}

	if (!bp->dmae_ready) {
		BNX2X_ERR("DMAE is not ready, can not copy\n");
		return DMAE_NOT_RDY;
	}

	/* set opcode and fixed command fields */
	bnx2x_prep_dmae_with_comp(bp, &dmae, DMAE_SRC_PCI, DMAE_DST_PCI);

	if (from_vf) {
		dmae.opcode_iov = (vfid << DMAE_COMMAND_SRC_VFID_SHIFT) |
			(DMAE_SRC_VF << DMAE_COMMAND_SRC_VFPF_SHIFT) |
			(DMAE_DST_PF << DMAE_COMMAND_DST_VFPF_SHIFT);

		dmae.opcode |= (DMAE_C_DST << DMAE_COMMAND_C_FUNC_SHIFT);

		dmae.src_addr_lo = vf_addr_lo;
		dmae.src_addr_hi = vf_addr_hi;
		dmae.dst_addr_lo = U64_LO(pf_addr);
		dmae.dst_addr_hi = U64_HI(pf_addr);
	}
	else {
		dmae.opcode_iov = (vfid << DMAE_COMMAND_DST_VFID_SHIFT) |
			(DMAE_DST_VF << DMAE_COMMAND_DST_VFPF_SHIFT) |
			(DMAE_SRC_PF << DMAE_COMMAND_SRC_VFPF_SHIFT);

		dmae.opcode |= 	(DMAE_C_SRC << DMAE_COMMAND_C_FUNC_SHIFT);


		dmae.src_addr_lo = U64_LO(pf_addr);
		dmae.src_addr_hi = U64_HI(pf_addr);
		dmae.dst_addr_lo = vf_addr_lo;
		dmae.dst_addr_hi = vf_addr_hi;
	}
	dmae.len = len32;

	bnx2x_dp_dmae(bp, &dmae, BNX2X_MSG_DMAE);

	/* issue the command and wait for completion */
	rc = bnx2x_issue_dmae_with_comp(bp, &dmae);
	return rc;
}

static void bnx2x_vf_dp_mbx_raw(struct bnx2x *bp, int msglvl, char* name,
				u8* msg, int len)
{
	int i;

	DP(msglvl, "%s: [ ", name);
	for (i = 0; i < len; i++)
		DP_CONT(msglvl, "0x%02x ", msg[i]);
	DP_CONT(msglvl, "]\n");
}

static void bnx2x_vf_dp_resp_status(struct bnx2x *bp,
				    struct pf_vf_msg_hdr *hdr)
{
	DP(BNX2X_MSG_IOV, "Response header: status=%d, op=%d, op_ver=%d\n",
	   hdr->status,
	   hdr->opcode,
	   hdr->opcode_ver);
}
static void bnx2x_vf_mbx_resp(struct bnx2x *bp, struct bnx2x_vf_mbx *mbx,
			      u8 status)
{
	mbx->msg->resp.resp.hdr.opcode = mbx->hdr.opcode;
	mbx->msg->resp.resp.hdr.opcode_ver = mbx->hdr.opcode_ver;
	mbx->msg->resp.resp.hdr.status = status;
	bnx2x_vf_dp_resp_status(bp, &mbx->msg->resp.resp.hdr);
}

static void bnx2x_vf_mbx_acquire_resp(struct bnx2x *bp, struct bnx2x_virtf *vf,
				      struct bnx2x_vf_mbx *mbx, u8 status)
{
	int i;
	struct pf_vf_msg_acquire_resp *resp = (struct pf_vf_msg_acquire_resp*)
		&mbx->msg->resp.acquire_resp;
	struct pf_vf_resc *resc = &resp->resc;

	memset(resp, 0, sizeof(*resp));
	resp->hdr.opcode = mbx->hdr.opcode;
	resp->hdr.opcode_ver = mbx->hdr.opcode_ver;
	resp->hdr.status = status;

	bnx2x_vf_dp_resp_status(bp, &resp->hdr);

	/* fill in pfdev info */
	resp->pfdev_info.chip_num = bp->common.chip_id;
	resp->pfdev_info.db_size = (1 << BNX2X_DB_SHIFT);
	resp->pfdev_info.indices_per_sb = HC_SB_MAX_INDICES_E2;
	resp->pfdev_info.pf_cap = (PFVF_CAP_RSS | /*PFVF_CAP_DHC |*/ PFVF_CAP_TPA);

	if (status == PFVF_STATUS_NO_RESOURCE ||
		status == PFVF_STATUS_SUCCESS) {
		/*
		 * set resources numbers, if status equals NO_RESOURCE these
		 * are max possible numbers
		 */
		resc->num_rxqs = vf->rxq_count;
		resc->num_txqs = vf->txq_count;
		resc->num_sbs = vf->max_sb_count;
		resc->num_mac_filters = vf->mac_rules_count;
		resc->num_vlan_filters = vf->vlan_rules_count;
		resc->num_mc_filters = 0;

		if (status == PFVF_STATUS_SUCCESS) {
			/* fill in the allocated resources */
			for_each_vf_rxq(vf, i) {
				struct bnx2x_vfq *rxq = VF_RXQ(vf, i);
				resc->hw_qid[i] = vfq_qzone_id(vf, rxq);
			}
			for_each_vf_sb(vf, i) {
				resc->hw_sbs[i].hw_sb_id = __vf_igu_sb(vf,i);
				resc->hw_sbs[i].sb_qid = __vf_hc_qzone(vf,i);
			}
		}
	}

	DP(BNX2X_MSG_IOV, "VF[%d] ACQUIRE_RESPONSE: pfdev_info- chip_num=0x%x, "
			  "db_size=%d, idx_per_sb=%d, pf_cap=0x%x\n"
			  "resources- n_rxq-%d, n_txq-%d, n_sbs-%d, "
			  "n_macs-%d, n_vlans-%d, n_mcs-%d\n",
	   vf->abs_vfid,
	   resp->pfdev_info.chip_num,
	   resp->pfdev_info.db_size,
	   resp->pfdev_info.indices_per_sb,
	   resp->pfdev_info.pf_cap,
	   resc->num_rxqs,
	   resc->num_txqs,
	   resc->num_sbs,
	   resc->num_mac_filters,
	   resc->num_vlan_filters,
	   resc->num_mc_filters);

	DP_CONT(BNX2X_MSG_IOV, "hw_qids- [ ");
	for (i = 0; i < vf->rxq_count; i++)
		DP_CONT(BNX2X_MSG_IOV, "%d ", resc->hw_qid[i]);
	DP_CONT(BNX2X_MSG_IOV, "], sb_info- [ ");
	for (i = 0; i < vf->sb_count; i++)
		DP_CONT(BNX2X_MSG_IOV, "%d:%d ",
			resc->hw_sbs[i].hw_sb_id,
			resc->hw_sbs[i].sb_qid);
	DP_CONT(BNX2X_MSG_IOV, "]\n");
}

static void bnx2x_vf_mbx_acquire(struct bnx2x *bp, struct bnx2x_virtf *vf,
				 struct bnx2x_vf_mbx *mbx)
{
	vf_api_t status;
	struct vf_pf_msg_acquire *acquire = &mbx->msg->req.acquire;

	/* log vfdef info */
	DP(BNX2X_MSG_IOV, "VF[%d] ACQUIRE: "
			  "vfdev_info- vf_id %d, vf_os %d, vf driver ver %d\n"
			  "resources- n_rxq-%d, n_txq-%d, n_sbs-%d, "
			  "n_macs-%d, n_vlans-%d, n_mcs-%d\n",
	   vf->abs_vfid,
	   acquire->vfdev_info.vf_id,
	   acquire->vfdev_info.vf_os,
	   acquire->vfdev_info.vf_driver_version,
	   acquire->resc_request.num_rxqs,
	   acquire->resc_request.num_txqs,
	   acquire->resc_request.num_sbs,
	   acquire->resc_request.num_mac_filters,
	   acquire->resc_request.num_vlan_filters,
	   acquire->resc_request.num_mc_filters);

	/* acquire the resources */
	status = bnx2x_vf_acquire(bp, vf ,&acquire->resc_request);

	/* prepare the response */
	bnx2x_vf_mbx_acquire_resp(bp, vf ,mbx,
				  vfapi_to_pfvf_status_codes[status]);
}

static void bnx2x_vf_mbx_init_vf(struct bnx2x *bp, struct bnx2x_virtf *vf,
			      struct bnx2x_vf_mbx *mbx)
{
	int i;
	vf_api_t status;
	struct vf_pf_msg_init_vf *init_vf = &mbx->msg->req.init_vf;

	vf->spq_map = init_vf->spq_addr;
	vf->fw_stat_map = init_vf->stats_addr;

	/* debug */
	DP(BNX2X_MSG_IOV, "VF[%d] VF_INIT: sb_addr[ ", vf->abs_vfid);
	for_each_vf_sb(vf, i)
		DP_CONT(BNX2X_MSG_IOV, "0x%llx ", init_vf->sb_addr[i]);

	DP_CONT(BNX2X_MSG_IOV, "], spq_addr=0x%llx, fw_stat_addr=0x%llx\n",
		vf->spq_map, vf->fw_stat_map);

	status = bnx2x_vf_init(bp, vf, init_vf->sb_addr);
	bnx2x_vf_mbx_resp(bp, mbx, vfapi_to_pfvf_status_codes[status]);
}

static void bnx2x_vf_mbx_setup_q(struct bnx2x *bp, struct bnx2x_virtf *vf,
				 struct bnx2x_vf_mbx *mbx)
{
	vf_api_t status = VF_API_FAILURE;
	struct vf_pf_msg_setup_q *setup_q = &mbx->msg->req.setup_q;

	/* verify vf_qid */
	if (setup_q->vf_qid >= vf->rxq_count) {
		BNX2X_ERR("vf_qid %d invalid, max queue count is %d\n",
			  setup_q->vf_qid, vf->rxq_count);
		goto response;
	}

	/*
	 * tx queues must be setup along side rx queues thus if the rx queue
	 * is not marked as valid there's nothing to do.
	 */

	if (setup_q->param_valid & VFPF_RXQ_VALID) {
		struct bnx2x_vfq *rxq = VF_RXQ(vf, setup_q->vf_qid);
		struct bnx2x_client_init_params p = {{0}};

		/* verify the queue state */
		if (rxq->state != BNX2X_FP_STATE_CLOSED) {
			BNX2X_ERR("vf queue %d is already open\n",
				  setup_q->vf_qid);
			goto response;
		}

		/* tx queue first */
		if (setup_q->param_valid & VFPF_TXQ_VALID) {
			struct bnx2x_vfq *txq = VF_TXQ(vf, setup_q->vf_qid);


			/* save sb resource index */
			txq->sb_idx = setup_q->txq.vf_sb;

			/* prepare txq init parameters */
			p.txq_params.flags  = setup_q->txq.flags;
			p.txq_params.dscr_map = setup_q->txq.txq_addr;
			p.txq_params.sb_cq_index = setup_q->txq.sb_index;
			p.txq_params.hc_rate = setup_q->txq.hc_rate;
			p.txq_params.traffic_type = setup_q->txq.traffic_type;

			DP(BNX2X_MSG_IOV, "VF[%d] Q_SETUP: txq[%d]-- vfsb=%d, "
					  "sb-index=%d, hc-rate=%d, "
					  "flags=0x%lx, traffic-type=%d, "
					  "txq_addr=0x%llx\n",
			   vf->abs_vfid,
			   setup_q->vf_qid,
			   txq->sb_idx,
			   p.txq_params.sb_cq_index,
			   p.txq_params.hc_rate,
			   p.txq_params.flags,
			   p.txq_params.traffic_type,
			   p.txq_params.dscr_map);

			bnx2x_vf_txq_setup(bp, vf, txq, &p);
		}

		/* save sb resource index */
		rxq->sb_idx = setup_q->rxq.vf_sb;

		/* prepare rxq init parameters */
		p.rxq_params.flags  = setup_q->rxq.flags;
		p.rxq_params.drop_flags = setup_q->rxq.drop_flags;
		p.rxq_params.dscr_map = setup_q->rxq.rxq_addr;
		p.rxq_params.sge_map = setup_q->rxq.sge_addr;
		p.rxq_params.rcq_map = setup_q->rxq.rcq_addr;
		p.rxq_params.rcq_np_map = setup_q->rxq.rcq_np_addr;
		p.rxq_params.mtu = setup_q->rxq.mtu;
		p.rxq_params.buf_sz = setup_q->rxq.buf_sz; /* must be laready adjusted */
		p.rxq_params.tpa_agg_sz = setup_q->rxq.tpa_agg_sz;
		p.rxq_params.max_sges_pkt = setup_q->rxq.max_sge_pkt;
		p.rxq_params.sge_buf_sz = setup_q->rxq.sge_buf_sz;
		p.rxq_params.cache_line_log = setup_q->rxq.cache_line_log;
		p.rxq_params.sb_cq_index = setup_q->rxq.sb_index;
		p.rxq_params.stat_id = setup_q->rxq.stat_id;
		p.rxq_params.hc_rate = setup_q->rxq.hc_rate;

		DP(BNX2X_MSG_IOV, "VF[%d] Q_SETUP: rxq[%d]-- vfsb=%d, "
				  "sb-index=%d, hc-rate=%d, mtu=%d, "
				  "buf-size=%d\n"
				  "sge-size=%d, max_sge_pkt=%d, "
				  "tpa-agg-size=%d, flags=0x%lx, "
				  "drop-flags=0x%x, cache-log=%d, stat-id=%d\n"
				  "rcq_addr=0x%llx, rcq_np_addr=0x%llx, "
				  "rxq_addr=0x%llx, sge_addr=0x%llx\n",
		   vf->abs_vfid,
		   setup_q->vf_qid,
		   rxq->sb_idx,
		   p.rxq_params.sb_cq_index,
		   p.rxq_params.hc_rate,
		   p.rxq_params.mtu,
		   p.rxq_params.buf_sz,
		   p.rxq_params.sge_buf_sz,
		   p.rxq_params.max_sges_pkt,
		   p.rxq_params.tpa_agg_sz,
		   p.rxq_params.flags,
		   p.rxq_params.drop_flags,
		   p.rxq_params.cache_line_log,
		   p.rxq_params.stat_id,
		   p.rxq_params.rcq_map,
		   p.rxq_params.rcq_np_map,
		   p.rxq_params.dscr_map,
		   p.rxq_params.sge_map);

		status = bnx2x_vf_rxq_setup(bp, vf, rxq, &p, 1);
	}

response:
	bnx2x_vf_mbx_resp(bp, mbx, vfapi_to_pfvf_status_codes[status]);
}

static vf_api_t
bnx2x_vf_mbx_add_mac_vlan(struct bnx2x *bp, struct bnx2x_virtf *vf,
			  struct bnx2x_vfq *rxq,
			  struct vf_pf_q_mac_vlan_filter *filter)
{
	int rc;
	unsigned long ramrod_flags = 0;

	if (filter->flags & VFPF_Q_FILTER_DEST_MAC_PRESENT) {
		set_bit(RAMROD_COMP_WAIT, &ramrod_flags);
		rc = bnx2x_vf_set_mac(bp, rxq, filter->dest_mac, 1, ramrod_flags);
	}

	if (filter->flags & VFPF_Q_FILTER_VLAN_TAG_PRESENT)
		rc = -1; /* no support to do */

	if (rc) {
		BNX2X_ERR("Failed to add mac-vlan rule rc=%d\n", rc);
		return VF_API_FAILURE;
	}

	return VF_API_SUCCESS;
}

static void bnx2x_vf_mbx_set_q_filters(struct bnx2x *bp, struct bnx2x_virtf *vf,
				       struct bnx2x_vf_mbx *mbx)
{
	vf_api_t status = VF_API_SUCCESS;
	struct vf_pf_msg_set_q_filters *filters = &mbx->msg->req.set_q_filters;

	/* verify vf_qid */
	if (filters->vf_qid > vf->rxq_count)
		goto response;

	DP(BNX2X_MSG_IOV, "VF[%d] Q_FILTERS: queue[%d]\n",
	   vf->abs_vfid,
	   filters->vf_qid);

	/* MAC-VLAN */
	if (filters->flags & VFPF_SET_Q_FILTERS_MAC_VLAN_CHANGED) {
		int i;
		struct bnx2x_vfq *rxq = VF_RXQ(vf, filters->vf_qid);

		for (i = 0; i < filters->n_mac_vlan_filters; i++) {
			struct vf_pf_q_mac_vlan_filter *filter =
				&filters->filters[i];

			DP(BNX2X_MSG_IOV, "MAC-VLAN[%d] -- flags=0x%x",
			   i, filter->flags);
			if (filter->flags & VFPF_Q_FILTER_VLAN_TAG_PRESENT)
				DP_CONT(BNX2X_MSG_IOV, ", vlan=%d",
					 filter->vlan_tag);
			if (filter->flags & VFPF_Q_FILTER_DEST_MAC_PRESENT)
				DP_CONT(BNX2X_MSG_IOV,
					 ", MAC=%02x:%02x:%02x:%02x:%02x:%02x",
					 filter->dest_mac[0],
					 filter->dest_mac[1],
					 filter->dest_mac[2],
					 filter->dest_mac[3],
					 filter->dest_mac[4],
					 filter->dest_mac[5]);
			DP_CONT(BNX2X_MSG_IOV, "\n");

			status = bnx2x_vf_mbx_add_mac_vlan(bp, vf, rxq,
						       &filters->filters[i]);
			if (status != VF_API_SUCCESS)
				break;
		}
	}

	/* RX mask */
	if (status == VF_API_SUCCESS &&
	    filters->flags & VFPF_SET_Q_FILTERS_RX_MASK_CHANGED) {
		struct bnx2x_vfq *rxq = VF_RXQ(vf, filters->vf_qid);
		unsigned long accept_flags = 0;

		DP(BNX2X_MSG_IOV, "RX-MASK=0x%x\n", filters->rx_mask);

		/* covert VF-PF if mask to bnx2x accept flags */
		if (filters->rx_mask & VFPF_RX_MASK_ACCEPT_MATCHED_UNICAST)
			set_bit(BNX2X_ACCEPT_UNICAST, &accept_flags);
		if (filters->rx_mask & VFPF_RX_MASK_ACCEPT_MATCHED_MULTICAST)
			set_bit(BNX2X_ACCEPT_MULTICAST, &accept_flags);
		if (filters->rx_mask & VFPF_RX_MASK_ACCEPT_ALL_UNICAST)
			set_bit(BNX2X_ACCEPT_ALL_UNICAST, &accept_flags);
		if (filters->rx_mask & VFPF_RX_MASK_ACCEPT_ALL_MULTICAST)
			set_bit(BNX2X_ACCEPT_ALL_MULTICAST, &accept_flags);
		if (filters->rx_mask & VFPF_RX_MASK_ACCEPT_BROADCAST)
			set_bit(BNX2X_ACCEPT_BROADCAST, &accept_flags);

		status = bnx2x_vf_set_rxq_mode(bp, vf, rxq, filters->rx_mask);
	}

	/* multicasts */
	if(status == VF_API_SUCCESS &&
	   filters->flags & VFPF_SET_Q_FILTERS_MULTICAST_CHANGED) {
		BNX2X_ERR("VF[%d] No suppoert for VF MC just yet q[%d]\n",
			  vf->abs_vfid, filters->vf_qid);
		status = VF_API_FAILURE;
	}
response:
	bnx2x_vf_mbx_resp(bp, mbx, vfapi_to_pfvf_status_codes[status]);
}

static void bnx2x_vf_mbx_activate_q(struct bnx2x *bp, struct bnx2x_virtf *vf,
				    struct bnx2x_vf_mbx *mbx)
{
	struct bnx2x_vfq *rxq = VF_RXQ(vf, mbx->msg->req.q_op.vf_qid);
	vf_api_t status = bnx2x_vf_trigger_q(bp, vf, rxq, true);

	DP(BNX2X_MSG_IOV, "VF[%d] Q_ACTIVATE: vf_qid=%d\n", vf->abs_vfid,
	   mbx->msg->req.q_op.vf_qid);

	bnx2x_vf_mbx_resp(bp, mbx, vfapi_to_pfvf_status_codes[status]);
}

static void bnx2x_vf_mbx_deactivate_q(struct bnx2x *bp, struct bnx2x_virtf *vf,
				      struct bnx2x_vf_mbx *mbx)
{
	struct bnx2x_vfq *rxq = VF_RXQ(vf, mbx->msg->req.q_op.vf_qid);
	vf_api_t status = bnx2x_vf_trigger_q(bp, vf, rxq, false);

	DP(BNX2X_MSG_IOV, "VF[%d] Q_DEACTIVATE: vf_qid=%d\n", vf->abs_vfid,
	   mbx->msg->req.q_op.vf_qid);

	bnx2x_vf_mbx_resp(bp, mbx, vfapi_to_pfvf_status_codes[status]);
}

static void bnx2x_vf_mbx_teardown_q(struct bnx2x *bp, struct bnx2x_virtf *vf,
				    struct bnx2x_vf_mbx *mbx)
{
	struct bnx2x_vfq *rxq = VF_RXQ(vf, mbx->msg->req.q_op.vf_qid);
	vf_api_t status = bnx2x_vfq_teardown(bp, vf, rxq);

	DP(BNX2X_MSG_IOV, "VF[%d] Q_TEARDOWN: vf_qid=%d\n", vf->abs_vfid,
	   mbx->msg->req.q_op.vf_qid);

	bnx2x_vf_mbx_resp(bp, mbx, vfapi_to_pfvf_status_codes[status]);
}

static void bnx2x_vf_mbx_close_vf(struct bnx2x *bp, struct bnx2x_virtf *vf,
				  struct bnx2x_vf_mbx *mbx)
{
	vf_api_t status;

	/* TODO verfiy abs_vfid */

	DP(BNX2X_MSG_IOV, "VF[%d] VF_CLOSE\n", vf->abs_vfid);

	status = bnx2x_vf_close(bp, vf);
	bnx2x_vf_mbx_resp(bp, mbx, vfapi_to_pfvf_status_codes[status]);

	/* re-open the mailbox */
	bnx2x_vf_enable_mbx(bp, vf->abs_vfid);
}

static void bnx2x_vf_mbx_release_vf(struct bnx2x *bp, struct bnx2x_virtf *vf,
				    struct bnx2x_vf_mbx *mbx)
{
	/* TODO verfiy abs_vfid */

	DP(BNX2X_MSG_IOV, "VF[%d] VF_RELEASE\n", vf->abs_vfid);

	bnx2x_vf_release(bp, vf);
	bnx2x_vf_mbx_resp(bp, mbx, vfapi_to_pfvf_status_codes[VF_API_SUCCESS]);
}

/* dispatch request */
void bnx2x_vf_mbx_request(struct bnx2x *bp, struct bnx2x_virtf *vf,
			  struct bnx2x_vf_mbx *mbx)
{
	/* switch on the opcode */
	switch(mbx->hdr.opcode) {
	case PFVF_OP_ACQUIRE:
		bnx2x_vf_mbx_acquire(bp, vf, mbx);
		break;
	case PFVF_OP_INIT_VF:
		bnx2x_vf_mbx_init_vf(bp, vf, mbx);
		break;
	case PFVF_OP_SETUP_Q:
		bnx2x_vf_mbx_setup_q(bp, vf, mbx);
		break;
	case PFVF_OP_SET_Q_FILTERS:
		bnx2x_vf_mbx_set_q_filters(bp, vf, mbx);
		break;
	case PFVF_OP_ACTIVATE_Q:
		bnx2x_vf_mbx_activate_q(bp, vf, mbx);
		break;
	case PFVF_OP_DEACTIVATE_Q:
		bnx2x_vf_mbx_deactivate_q(bp, vf, mbx);
		break;
	case PFVF_OP_TEARDOWN_Q:
		bnx2x_vf_mbx_teardown_q(bp, vf, mbx);
		break;
	case PFVF_OP_CLOSE_VF:
		bnx2x_vf_mbx_close_vf(bp, vf, mbx);
		break;
	case PFVF_OP_RELEASE_VF:
		bnx2x_vf_mbx_release_vf(bp, vf, mbx);
		break;
	default:
		break;
	}
}

static inline int bnx2x_vf_mbx_req_len(struct vf_pf_msg_hdr *hdr)
{
	return op_codes_req_sz[hdr->opcode][hdr->opcode_ver].req_sz;
}
static inline int bnx2x_vf_mbx_resp_len(struct vf_pf_msg_hdr *hdr)
{
	return op_codes_req_sz[hdr->opcode][hdr->opcode_ver].resp_sz;
}

static int bnx2x_vf_mbx_hdr(struct bnx2x *bp, struct bnx2x_vf_mbx *mbx)
{
	/* save the message header */
	mbx->hdr = mbx->msg->req.hdr;

	if (mbx->hdr.if_ver != PFVF_IF_VERSION) {
		BNX2X_ERR("Version mismatch - VF ver %d\n", mbx->hdr.if_ver);
		bnx2x_vf_mbx_resp(bp, mbx, PFVF_STATUS_VERSION_MISMATCH);
		return 1;
	}
	if (mbx->hdr.opcode >= PFVF_OP_MAX) {
		BNX2X_ERR("opcdoe not supported %d\n", mbx->hdr.opcode);
		bnx2x_vf_mbx_resp(bp, mbx, PFVF_STATUS_NOT_SUPPORTED);
		return 1;
	}
	if (mbx->hdr.opcode_ver >
	    PFVF_OP_VER_MAX(op_codes_req_sz[mbx->hdr.opcode])) {
		BNX2X_ERR("opcdoe ver not supported %d\n", mbx->hdr.opcode_ver);
		bnx2x_vf_mbx_resp(bp, mbx, PFVF_STATUS_NOT_SUPPORTED);
		return 1;
	}
	return 0;
}

/* handle new vf-pf message */
void bnx2x_vf_mbx(struct bnx2x *bp, struct vf_pf_event_data *vfpf_event)
{
	struct bnx2x_virtf *vf;
	struct bnx2x_vf_mbx *mbx;
	u64 vf_addr;
	dma_addr_t pf_addr;
	u32 len;
	u8 vf_idx;
	int rc;

	/* Sanity checks consider removing later */

	/* check if the vf_id is valid */
	if (vfpf_event->vf_id > BNX2X_NR_VIRTFN(bp)) {
		BNX2X_ERR("Illegal vf_id %d\n", vfpf_event->vf_id);
		return;
	}
	vf_idx = bnx2x_vf_idx_by_abs_fid(bp, vfpf_event->vf_id);

	mbx = BP_VF_MBX(bp, vf_idx);

	/* verify an event is not currently being processed -
	   debug failsafe only */
	if (mbx->flags & VF_MSG_INPROCESS) {
		BNX2X_ERR("Previous message is still being processed, "
			  "vf_id %d\n", vfpf_event->vf_id);
		return;
	}

	vf = BP_VF(bp, vf_idx);

	/* save the VF message address */
	mbx->vf_addr_hi = le32_to_cpu(vfpf_event->msg_addr_hi);
	mbx->vf_addr_lo = le32_to_cpu(vfpf_event->msg_addr_lo);

	/* dmae to get the VF message (request) header */
	rc = bnx2x_copy32_vf_dmae(bp, true, mbx->msg_mapping, vf->abs_vfid,
				  mbx->vf_addr_hi, mbx->vf_addr_lo,
				  sizeof(struct vf_pf_msg_hdr)/4);
	if (rc) {
		BNX2X_ERR("Failed to copy msg header from VF %d\n", vf->abs_vfid);
		goto release_vf;
	}

	/* process the VF message header */
	rc = bnx2x_vf_mbx_hdr(bp, mbx);
	if (rc) {
		BNX2X_ERR("illegal msg header from VF %d\n", vf->abs_vfid);
		goto send_resp;
	}

	/* dmae to get the VF message (request) hdr + body */
	len = bnx2x_vf_mbx_req_len(&mbx->hdr);
	rc = bnx2x_copy32_vf_dmae(bp, true, mbx->msg_mapping, vf->abs_vfid,
				  mbx->vf_addr_hi, mbx->vf_addr_lo,
				  len/4);
	if (rc) {
		BNX2X_ERR("Failed to copy msg body from VF %d\n", vf->abs_vfid);
		goto release_vf;
	}

	bnx2x_vf_dp_mbx_raw(bp, BNX2X_MSG_DMAE, "Raw Request", (char *)mbx->msg,
			    len);

	/* dispatch the request (will prepare the response) */
	bnx2x_vf_mbx_request(bp, vf, mbx);

send_resp:
	vf_addr = HILO_U64(mbx->vf_addr_hi, mbx->vf_addr_lo) +
		mbx->hdr.resp_msg_offset;

	pf_addr = mbx->msg_mapping +
		offsetof(struct bnx2x_vf_mbx_msg, resp);

	if (mbx->msg->resp.resp.hdr.status == PFVF_STATUS_NOT_SUPPORTED ||
	    mbx->msg->resp.resp.hdr.status == PFVF_STATUS_VERSION_MISMATCH)
		len = sizeof(struct pf_vf_msg_hdr);
	else
		len = bnx2x_vf_mbx_resp_len(&mbx->hdr);


	bnx2x_vf_dp_mbx_raw(bp, BNX2X_MSG_DMAE, "Raw Response",
			    (char *)mbx->msg +
			    offsetof(struct bnx2x_vf_mbx_msg, resp), len);

	len -= sizeof(struct pf_vf_msg_hdr);

	/* copy the response body */
	if (len) {
		vf_addr += sizeof(struct pf_vf_msg_hdr);
		pf_addr += sizeof(struct pf_vf_msg_hdr);
		rc = bnx2x_copy32_vf_dmae(bp, false, pf_addr, vf->abs_vfid,
					  U64_HI(vf_addr),
					  U64_LO(vf_addr),
					  len/4);

		if (rc) {
			BNX2X_ERR("Failed to copy response body to VF %d\n",
				  vf->abs_vfid);
			goto release_vf;
		}
		vf_addr -= sizeof(struct pf_vf_msg_hdr);
		pf_addr -= sizeof(struct pf_vf_msg_hdr);
	}

	/* ack the FW */
	storm_memset_vf_mbx_ack(bp, vf->abs_vfid);
	mmiowb();

	/* initate dmae to send the reponse */
	mbx->flags &= ~VF_MSG_INPROCESS;

	/*
	 * copy the repsonse header including status-done field,
	 * must be last dmae, must be after FW is acked
	 */
	rc = bnx2x_copy32_vf_dmae(bp, false, pf_addr, vf->abs_vfid,
				  U64_HI(vf_addr),
				  U64_LO(vf_addr),
				  sizeof(struct pf_vf_msg_hdr)/4);
	if (rc) {
		BNX2X_ERR("Failed to copy response statis to VF %d\n",
			  vf->abs_vfid);
		goto release_vf;
	}

	return;

release_vf:
	bnx2x_vf_release(bp, vf);
	return;
}
#endif /* VFPF_MBX */

#else /* BCM_IOV */
#error "IOV source file compiles yet BCM_IOV is undefined"
#endif

