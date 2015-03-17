/* bnx2x_esx.c: QLogic Everest network driver.
 *
 * Copyright 2008-2014 QLogic Corporation
 *
 * Portions Copyright (c) VMware, Inc. 2008-2014, All Rights Reserved.
 * Copyright (c) 2007-2014 QLogic Corporation
 *
 * Unless you and QLogic execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2, available
 * at http://www.gnu.org/licenses/old-licenses/gpl-2.0.html (the "GPL").
 *
 * Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other QLogic software provided under a
 * license other than the GPL, without QLogic's express prior written
 * consent.
 *
 * NetQueue code from VMware
 * IOCTL code by: Benjamin Li
 * PASSTHRU code by: Shmulik Ravid
 *
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/delay.h>
#include <linux/ethtool.h>
#ifdef __LINUX_MUTEX_H
#include <linux/mutex.h>
#endif
#include <linux/version.h>

#include "bnx2x.h"
#include "bnx2x_cmn.h"

#ifdef BNX2X_ESX_SRIOV
#include "bnx2x_sriov.h"
#endif /* BNX2X_ESX_SRIOV */

#include "hw_dump.h"
#include "bnx2x_esx.h"
#include "bnx2x_init.h"

#if defined(__VMKLNX__) && (VMWARE_ESX_DDK_VERSION >= 50000) /* ! BNX2X_UPSTREAM */
#include "cnic_register.h"

static int registered_cnic_adapter;
#endif /* defined(__VMKLNX__) && (VMWARE_ESX_DDK_VERSION >= 50000) */

/* empty implementations for thes kernel primitives because they seems to be compiled out */
inline void local_bh_enable(void) {}
inline void local_bh_disable(void) {}

#if defined(VMX86_DEBUG)
int psod_on_panic = 1;
#else
int psod_on_panic;
#endif /* VMX86_DEBUG */
module_param(psod_on_panic, int, 0);
MODULE_PARM_DESC(psod_on_panic, " PSOD on panic");

static int multi_rx_filters = -1;
module_param(multi_rx_filters, int, 0);
MODULE_PARM_DESC(multi_rx_filters, "Define the number of RX filters per "
				   "NetQueue: (allowed values: -1 to "
				   "Max # of RX filters per NetQueue, "
				   "-1: use the default number of RX filters; "
				   "0: Disable use of multiple RX filters; "
				   "1..Max # the number of RX filters "
				   "per NetQueue: will force the number "
				   "of RX filters to use for NetQueue");

#if (VMWARE_ESX_DDK_VERSION >= 50000)
int enable_default_queue_filters = -1;
module_param(enable_default_queue_filters, int, 0);
MODULE_PARM_DESC(enable_default_queue_filters, "Allow filters on the "
					       "default queue. "
					       "[Default is disabled "
					       "for non-NPAR mode, "
					       "enabled by default "
					       "on NPAR mode]");
#endif

int debug_unhide_nics = 0;
module_param(debug_unhide_nics, int, 0);
MODULE_PARM_DESC(debug_unhide_nics, "Force the exposure of the vmnic interface "
				    "for debugging purposes"
				    "[Default is to hide the nics]"
				    "1.  In SRIOV mode expose the PF");

#if (VMWARE_ESX_DDK_VERSION >= 55000)
#ifdef ENC_SUPPORTED
int enable_vxlan_ofld = 0;
module_param(enable_vxlan_ofld, int, 0);
MODULE_PARM_DESC(enable_vxlan_ofld, "Allow vxlan TSO/CSO offload support."
					       "[Default is disabled, "
					       "1: enable vxlan offload, "
					       "0: disable vxlan offload]");
#endif
int disable_fw_dmp;
module_param(disable_fw_dmp, int, 0);
MODULE_PARM_DESC(disable_fw_dmp, "For debug purposes, disable firmware dump "
			" feature when set to value of 1");
#endif

#if (VMWARE_ESX_DDK_VERSION >= 55000)
int disable_rss_dyn = 0;
module_param(disable_rss_dyn, int,	0);
MODULE_PARM_DESC(
	disable_rss_dyn,
	"For debug purposes, disable RSS_DYN feature when set to value of 1");
#endif

#ifdef BNX2X_ESX_DYNAMIC_NETQ
static int disable_feat_preemptible = 0;
module_param(disable_feat_preemptible, int, 0);
MODULE_PARM_DESC(
	disable_feat_preemptible,
	"For debug purposes, disable FEAT_PREEMPTIBLE when set to value of 1");
#endif

#ifdef BNX2X_ESX_SRIOV
#define BNX2X_MAX_VFS_OPTION_UNSET      0
#define BNX2X_MAX_NIC                   32

unsigned int max_vfs_count;
int max_vfs[BNX2X_MAX_NIC+1] = {
	[0 ... BNX2X_MAX_NIC] = BNX2X_MAX_VFS_OPTION_UNSET };
/*  Remove SR-IOV support */
module_param_array_named(max_vfs, max_vfs, int, &max_vfs_count, NULL);
/*  We will need to fix this module description as the Workbench 3.0 SRIOV
 *  test searches for the module description text below
 */
MODULE_PARM_DESC(max_vfs,
		 "Number of Virtual Functions: "
		 "0 = disable (default), 1-"
		 XSTRINGIFY(BNX2X_MAX_NUM_OF_VFS)
		 " = enable this many VFs");
#endif

static int bnx2x_get_filters_per_queue(struct bnx2x *bp);

#if (VMWARE_ESX_DDK_VERSION >= 50000)
static void bnx2x_esx_rss_config_common(struct bnx2x *bp,
			struct bnx2x_config_rss_params *params)
{
	__set_bit(RAMROD_COMP_WAIT, &params->ramrod_flags);
	if (bp->esx.rss_p_num) {
#if (VMWARE_ESX_DDK_VERSION < 55000)
		__set_bit(BNX2X_RSS_MODE_ESX51, &params->rss_flags);
#else
		__set_bit(BNX2X_RSS_MODE_REGULAR, &params->rss_flags);
#endif
		__set_bit(BNX2X_RSS_IPV4, &params->rss_flags);
		__set_bit(BNX2X_RSS_IPV4_TCP, &params->rss_flags);
		__set_bit(BNX2X_RSS_IPV4_UDP, &params->rss_flags);
		__set_bit(BNX2X_RSS_IPV6, &params->rss_flags);
		__set_bit(BNX2X_RSS_IPV6_TCP, &params->rss_flags);
		__set_bit(BNX2X_RSS_IPV6_UDP, &params->rss_flags);
		__set_bit(BNX2X_RSS_TUNNELING, &params->rss_flags);
		params->tunnel_mask = 0xffff;
		params->tunnel_value = 0x2118; /* Default VXLAN UDP port */
	}
	/* Hash bits */
	params->rss_result_mask = MULTI_MASK;
}

#if !defined(BNX2X_ESX_DYNAMIC_NETQ)
static int bnx2x_esx_rss(struct bnx2x *bp, struct bnx2x_rss_config_obj *rss_obj,
	      bool config_hash, bool enable)
{
	struct bnx2x_config_rss_params params = {NULL};

	/* Although RSS is meaningless when there is a single HW queue we
	 * still need it enabled in order to have HW Rx hash generated.
	 *
	 * if (!is_eth_multi(bp))
	 *      bp->multi_mode = ETH_RSS_MODE_DISABLED;
	 */

	params.rss_obj = rss_obj;
	bnx2x_esx_rss_config_common(bp, &params);
	memcpy(params.ind_table, rss_obj->ind_table, sizeof(params.ind_table));

	if (config_hash) {
		/* RSS keys */
		prandom_bytes(params.rss_key, T_ETH_RSS_KEY * 4);
		__set_bit(BNX2X_RSS_SET_SRCH, &params.rss_flags);
	}

	return bnx2x_config_rss(bp, &params);
}
#endif
#endif

#ifdef BNX2X_ESX_DYNAMIC_NETQ
inline int bnx2x_init_rx_rings(struct bnx2x *bp)
{
	return bnx2x_esx_init_rx_ring(bp, 0);
}

inline int bnx2x_drain_tx_queues(struct bnx2x *bp)
{
	return bnx2x_esx_drain_tx_queue(bp, 0);
}

#if (VMWARE_ESX_DDK_VERSION >= 50000)
static int bnx2x_esx_config_rss_pf(
	struct bnx2x *bp, const u8 *raw_ind_tbl,
	const u32 *rss_key_tbl)
{
	struct bnx2x_config_rss_params params = {NULL};
	int i, rc;

	/* Although RSS is meaningless when there is a single HW queue we
	 * still need it enabled in order to have HW Rx hash generated.
	 *
	 * if (!is_eth_multi(bp))
	 *      bp->multi_mode = ETH_RSS_MODE_DISABLED;
	 */
	if (raw_ind_tbl) {
		struct bnx2x_rss_config_obj *rss_obj = &bp->rss_conf_obj;
		int *rss_idx_tbl = bp->esx.rss_netq_idx_tbl;

		/* update RSS pool indirection table based on raw indirection
		 * table provided and the list of rss queues
		 */
		for (i = 0; i < T_ETH_INDIRECTION_TABLE_SIZE; i++) {
			u8 offset;
			offset = bp->esx.rss_raw_ind_tbl[i];
			/* offset should have a value between
			 * 0 to bp->esx.rss_p_size - 1
			 */
			if (offset >= bp->esx.rss_p_size) {
				BNX2X_ERR(
					"Invalid rss ind tbl entry[%d]:%d >= %d\n",
					i, offset, bp->esx.rss_p_size);
				return -1;
			}
			rss_obj->ind_table[i] =
				bp->fp[rss_idx_tbl[offset]].cl_id;
		}

		DP(BNX2X_MSG_NETQ, "updated ind tbl :%d %d %d %d %d %d %d %d\n",
		   rss_obj->ind_table[0], rss_obj->ind_table[1],
		   rss_obj->ind_table[2], rss_obj->ind_table[3],
		   rss_obj->ind_table[4], rss_obj->ind_table[5],
		   rss_obj->ind_table[6], rss_obj->ind_table[7]);

		params.rss_obj = rss_obj;
		bnx2x_esx_rss_config_common(bp, &params);
		memcpy(params.ind_table, rss_obj->ind_table,
		       sizeof(params.ind_table));
	}

	if (rss_key_tbl) {
		/* RSS keys */
		for (i = 0; i < T_ETH_RSS_KEY; i++)
			params.rss_key[i] = rss_key_tbl[i];

		__set_bit(BNX2X_RSS_SET_SRCH, &params.rss_flags);
	}

	rc = bnx2x_config_rss(bp, &params);
	if (rc)
		BNX2X_ERR("Failed to config rss");
	return rc;
}

static int bnx2x_netq_set_rss(struct bnx2x *bp)
{

	/* Set the RSS pool indirection table */
	if (bnx2x_esx_config_rss_pf
		(bp, bp->esx.rss_raw_ind_tbl, bp->esx.rss_key_tbl))
		return VMKNETDDI_QUEUEOPS_ERR;

	return VMKNETDDI_QUEUEOPS_OK;
}

static int bnx2x_netq_clear_rss(struct bnx2x *bp)
{
	struct bnx2x_config_rss_params params = {NULL};

	params.rss_obj = &bp->rss_conf_obj;

	__set_bit(RAMROD_COMP_WAIT, &params.ramrod_flags);

	/* Hash bits */
	params.rss_result_mask = MULTI_MASK;

	memset(params.ind_table, 0, sizeof(params.ind_table));

	return bnx2x_config_rss(bp, &params);
}

#endif /* VMWARE_ESX_DDK_VERSION >= 50000 */

static int bnx2x_esx_calc_rx_ring_size(struct bnx2x *bp)
{
	int rx_ring_size = 0;

	if (!bp->rx_ring_size &&
	    (IS_MF_STORAGE_SD(bp) || IS_MF_FCOE_AFEX(bp))) {
		rx_ring_size = MIN_RX_SIZE_NONTPA;
		bp->rx_ring_size = rx_ring_size;
	} else if (!bp->rx_ring_size) {
		rx_ring_size = MAX_RX_AVAIL/BNX2X_NUM_RX_QUEUES(bp);

		if (CHIP_IS_E3(bp)) {
			u32 cfg = SHMEM_RD(bp,
					   dev_info.port_hw_config[BP_PORT(bp)].
					   default_cfg);

			/* Decrease ring size for 1G functions */
			if ((cfg & PORT_HW_CFG_NET_SERDES_IF_MASK) ==
			    PORT_HW_CFG_NET_SERDES_IF_SGMII)
				rx_ring_size /= 10;
		}

		/* allocate at least number of buffers required by FW */
		rx_ring_size = max_t(int, bp->disable_tpa ? MIN_RX_SIZE_NONTPA :
				     MIN_RX_SIZE_TPA, rx_ring_size);

		bp->rx_ring_size = rx_ring_size;
	} else /* if rx_ring_size specified - use it */
		rx_ring_size = bp->rx_ring_size;

	DP(BNX2X_MSG_SP, "calculated rx_ring_size %d\n", rx_ring_size);

	return rx_ring_size;
}

static void bnx2x_reserve_netq_feature(struct bnx2x *bp)
{
	int i;

	if (bp->esx.rss_p_num && bp->esx.rss_p_size) {
#if (VMWARE_ESX_DDK_VERSION >= 50000)
		/* Validate we have enough queues to support RSS */
		if (BNX2X_NUM_ETH_QUEUES(bp) <= bp->esx.rss_p_size) {
			BNX2X_ERR("Not enough queues to support RSS. Disabling RSS\n");
			bp->esx.rss_p_num = 0;
			bp->esx.rss_p_size = 0;
			bp->esx.avail_rss_netq = 0;
		} else {
			/* Compute number of rss queues we can support */
			bp->esx.avail_rss_netq =
			    min((int)(bp->esx.rss_p_num * bp->esx.rss_p_size),
				 (int)(BNX2X_NUM_ETH_QUEUES(bp) - 1));

			/* Initialize default raw indirection table and
			 * RSS key table
			 */
			for (i = 0; i < T_ETH_INDIRECTION_TABLE_SIZE; i++) {
				/* raw ind tbl entry should have a value between
				 * 0 to bp->esx.rss_p_size - 1
				 */
				bp->esx.rss_raw_ind_tbl[i] =
					ethtool_rxfh_indir_default
						(i, bp->esx.rss_p_size);
			}
			/* Initialize RSS keys with random numbers */
			prandom_bytes
			   (bp->esx.rss_key_tbl, sizeof(bp->esx.rss_key_tbl));
		}
#else
		BNX2X_ERR("DDK does not support RSS\n");
		bp->esx.rss_p_num = 0;
		bp->esx.rss_p_size = 0;
		bp->esx.avail_rss_netq = 0;
#endif
	}

	/*  Disable tpa and rss for all queues initially
	 *  (including default queue)
	 */
	for_each_eth_queue(bp, i) {
		bp->fp[i].disable_tpa = 1;
		/* clear features flags */
		bp->fp[i].esx.netq_flags &=
			~BNX2X_NETQ_FP_FEATURES_RESERVED_MASK;
	}
	/* default queue is always allocated */
	bp->fp[0].esx.netq_flags |=
		(BNX2X_NETQ_TX_QUEUE_ALLOCATED | BNX2X_NETQ_RX_QUEUE_ALLOCATED);

	/* Set max lro queues available for allocation */
#if (VMWARE_ESX_DDK_VERSION < 41000)
	bp->esx.avail_lro_netq = 0;
#else
	bp->esx.avail_lro_netq = BNX2X_NETQ_FP_LRO_RESERVED_MAX_NUM(bp);
#endif
}

static int bnx2x_esx_calc_offset(struct bnx2x *bp)
{
	int offset = 0;

	/* VFs don't have a default SB */
	if (IS_PF(bp))
		offset++;

	if (CNIC_SUPPORT(bp))
		offset++;

	return offset;
}

static int bnx2x_esx_drain_tpa_queue(struct bnx2x *bp,
					  struct bnx2x_fastpath *fp)
{
	int cnt = 1000, i;

	if (fp->disable_tpa)
		return 0;

	for (i = 0; i < ETH_MAX_AGGREGATION_QUEUES_E1H_E2; i++) {
		while (fp->tpa_info[i].tpa_state == BNX2X_TPA_START) {
			if (!cnt) {
				BNX2X_ERR("timeout waiting for queue[%d] tpa_info[%d]\n",
					  fp->index, i);
				break;
			}

			cnt--;
			usleep_range(1000, 2000);
		}
	}
	return 0;
}

static int bnx2x_esx_stop_queue(struct bnx2x *bp, int index)
{
	struct netdev_queue *txq;
	struct bnx2x_fastpath *fp = &bp->fp[index];
	int offset, rc = 0;
	struct bnx2x_fp_txdata *txdata = &bp->bnx2x_txq[index];

	txq = netdev_get_tx_queue(bp->dev, index);

	/* set this flag to notify other threads not to wake this queue */
	fp->esx.dynamic_netq_stop = true;
	netif_tx_stop_queue(txq);

	/* make sure no more pending hard_start_xmit
	 * before draining the tx queue
	 */
	spin_unlock_wait(&txq->_xmit_lock);

	bp->dev->trans_start = jiffies; /* prevent tx timeout */

	bnx2x_esx_drain_tx_queue(bp, index);
	bnx2x_stop_queue(bp, index);

	offset = bnx2x_esx_calc_offset(bp);
	synchronize_irq(bp->msix_table[offset + index].vector);

	bnx2x_esx_drain_tpa_queue(bp, fp);
	bnx2x_ack_sb(bp, fp->igu_sb_id, USTORM_ID, 0, IGU_INT_DISABLE, 0);

	bnx2x_esx_napi_disable_queue(bp, index);
	free_irq(bp->msix_table[offset + index].vector, fp);
	bnx2x_free_tx_skbs_queue(fp);
	bnx2x_esx_free_rx_skbs_queue(bp, index);
	bnx2x_free_rx_sge_range(bp, fp, NUM_RX_SGE);

	fp->rx_comp_cons = 0;
	txdata->tx_pkt_prod = 0;
	txdata->tx_pkt_cons = 0;
	txdata->tx_bd_prod = 0;
	txdata->tx_bd_cons = 0;
	txdata->tx_pkt = 0;

	DP(BNX2X_MSG_NETQ, "queue %d stopped: [netq_flags: 0x%x]\n",
	   index, fp->esx.netq_flags);

	return rc;
}


static	void reset_queue_stats(struct bnx2x *bp, struct bnx2x_fastpath *fp)
{
	struct tstorm_per_queue_stats *old_tclient =
		&bnx2x_fp_stats(bp, fp)->old_tclient;
	struct ustorm_per_queue_stats *old_uclient =
		&bnx2x_fp_stats(bp, fp)->old_uclient;
	struct xstorm_per_queue_stats *old_xclient =
		&bnx2x_fp_stats(bp, fp)->old_xclient;
	struct bnx2x_eth_q_stats *qstats =
		&bnx2x_fp_stats(bp, fp)->eth_q_stats;
	struct bnx2x_eth_q_stats_old *qstats_old =
		&bnx2x_fp_stats(bp, fp)->eth_q_stats_old;

	/* save away queue statistics before restarting the queue*/
	UPDATE_QSTAT_OLD(total_unicast_bytes_received_hi);
	UPDATE_QSTAT_OLD(total_unicast_bytes_received_lo);
	UPDATE_QSTAT_OLD(total_broadcast_bytes_received_hi);
	UPDATE_QSTAT_OLD(total_broadcast_bytes_received_lo);
	UPDATE_QSTAT_OLD(total_multicast_bytes_received_hi);
	UPDATE_QSTAT_OLD(total_multicast_bytes_received_lo);
	UPDATE_QSTAT_OLD(total_unicast_bytes_transmitted_hi);
	UPDATE_QSTAT_OLD(total_unicast_bytes_transmitted_lo);
	UPDATE_QSTAT_OLD(total_broadcast_bytes_transmitted_hi);
	UPDATE_QSTAT_OLD(total_broadcast_bytes_transmitted_lo);
	UPDATE_QSTAT_OLD(total_multicast_bytes_transmitted_hi);
	UPDATE_QSTAT_OLD(total_multicast_bytes_transmitted_lo);
	UPDATE_QSTAT_OLD(total_tpa_bytes_hi);
	UPDATE_QSTAT_OLD(total_tpa_bytes_lo);

	/* restart stats update and clear the old stat counter  */
	memset(old_tclient, 0, sizeof(struct tstorm_per_queue_stats));
	memset(old_uclient, 0, sizeof(struct ustorm_per_queue_stats));
	memset(old_xclient, 0, sizeof(struct xstorm_per_queue_stats));
}

static int bnx2x_esx_setup_queue(struct bnx2x *bp, int index,
				 enum bnx2x_esx_rx_mode_start rx)
{
	int rc, offset, rx_ring_size, cos;
	struct bnx2x_fastpath *fp = &bp->fp[index];

	DP(BNX2X_MSG_NETQ, "Setting up queue[%d] as %cX\n", fp->index,
	   rx ? 'R' : 'T');

	for_each_cos_in_tx_queue(fp, cos) {
		struct bnx2x_fp_txdata *txdata = fp->txdata_ptr[cos];

		/* As this point, if we still has tx work need to be unload,
		 * mc_assert will occur
		 */
		if (bnx2x_has_tx_work_unload(txdata))
			BNX2X_ERR("fp%d: tx_pkt_prod:%x tx_pkt_cons:%x tx_bd_prod:%x "
				  "tx_bd_cons:%x *tx_cons_sb:%x last doorbell:%x\n",
				  index, txdata->tx_pkt_prod,
				  txdata->tx_pkt_cons, txdata->tx_bd_prod,
				  txdata->tx_bd_cons,
				  le16_to_cpu(*txdata->tx_cons_sb),
						txdata->tx_db.data.prod);
		txdata->tx_db.data.zero_fill1 = 0;
		txdata->tx_db.data.prod = 0;

		txdata->tx_pkt_prod = 0;
		txdata->tx_pkt_cons = 0;
		txdata->tx_bd_prod = 0;
		txdata->tx_bd_cons = 0;
		txdata->tx_pkt = 0;
	}

	if (rx == BNX2X_NETQ_START_RX_QUEUE) {
		int ring_size;

		rx_ring_size = bnx2x_esx_calc_rx_ring_size(bp);
		ring_size = bnx2x_alloc_rx_bds(fp, rx_ring_size);
		if (ring_size < rx_ring_size) {
			DP(BNX2X_MSG_NETQ, "Failed allocating the entire %d, only allocated: %d on queue: %d\n",
			   rx_ring_size, ring_size, index);
			rc = -ENOMEM;
			goto error_setup_queue_0;
		}

		/* ensure status block indices were read */
		rmb();
		rc = bnx2x_esx_init_rx_ring(bp, index);
		if (rc) {
			DP(BNX2X_MSG_NETQ, "Failed allocating TPA memory on queue: %d\n",
			   index);
			rc = -ENOMEM;
			goto error_setup_queue_0;
		}
	} else { /* reset rx prod/consumer index even as tx queue for SP msg */
		fp->rx_bd_prod = 0;
		fp->rx_bd_cons = 0;
		fp->rx_comp_prod = 0;
		fp->rx_comp_cons = 0;
	}

	bnx2x_init_sb(bp, fp->status_blk_mapping, BNX2X_VF_ID_INVALID, false,
		      fp->fw_sb_id, fp->igu_sb_id);

	bnx2x_esx_napi_enable_queue(bp, index);

	offset = bnx2x_esx_calc_offset(bp);

	snprintf(fp->name, sizeof(fp->name), "%s-fp-%d",
		 bp->dev->name, index);

	rc = request_irq(bp->msix_table[offset + index].vector,
			 bnx2x_msix_fp_int, 0, fp->name, fp);
	if (rc) {
		BNX2X_ERR("Queue %d failed IRQ request [0x%x]\n", index, rc);
		goto error_setup_queue_1;
	}

	rc = bnx2x_setup_queue(bp, fp, 0);
	if (rc) {
		BNX2X_ERR("Queue %d setup failed[0x%x]\n", index, rc);
		goto error_setup_queue_1;
	}

	/* flush all */
	mb();
	mmiowb();

	bnx2x_ack_sb(bp, fp->igu_sb_id, USTORM_ID, le16_to_cpu(fp->fp_hc_idx),
		     IGU_INT_ENABLE, 1);

	/* clear the stop flag since we are waking up the queue */
	fp->esx.dynamic_netq_stop = false;

	netif_tx_wake_queue(netdev_get_tx_queue(bp->dev, index));

	if (bp->vlgrp)
		bnx2x_esx_set_vlan_stripping(bp, true, index);

	/* Reset queue statistics since queue has been restart */
	reset_queue_stats(bp, fp);
	return 0;

error_setup_queue_1:
	offset = bnx2x_esx_calc_offset(bp);
	free_irq(bp->msix_table[offset + index].vector, fp);
	bnx2x_esx_napi_disable_queue(bp, index);
	rc = -EIO;


error_setup_queue_0:
	/* no need to counter effects of bnx2x_esx_init_rx_ring */
	if (rx == BNX2X_NETQ_START_RX_QUEUE) {
		/* counter effects of bnx2x_alloc_rx_bds */
		bnx2x_free_rx_bds(fp);
		bnx2x_esx_setup_queue(bp, index,
				      BNX2X_NETQ_IGNORE_START_RX_QUEUE);
	}
	return rc;
}

static void bnx2x_netq_free_rx_queue_single(struct bnx2x *bp,
					    struct bnx2x_fastpath *fp)
{
	int rc;

	/* Don't add non-leading RSS queues to free_netq_list */
	if (fp->esx.netq_flags & BNX2X_NETQ_FP_RSS_RESERVED &&
	    fp - bp->fp > BNX2X_NUM_RX_NETQUEUES(bp)) {
		goto restart_q;
	}

	/* put the queue back to the free_netq list if in use*/
	list_add((struct list_head *)(fp->esx.netq_in_use),
		 &bp->esx.free_netq_list);
	fp->esx.netq_in_use = NULL;
	if (bp->esx.n_rx_queues_allocated)
		bp->esx.n_rx_queues_allocated--;
  restart_q:
	/* stop the rx/tx queue */
	rc = bnx2x_esx_stop_queue(bp, fp->index);
	if (rc) {
		BNX2X_ERR("Could not stop queue:%d\n", fp->index);
		return;
	}

	/* Note that stop and setup rx queue uses configuration flags
	 * make sure configuration flags change is done between stop and setup
	 * (e.g. check disable_tpa to alloc/free tpa_pool)
	 */
	if (fp->esx.netq_flags & BNX2X_NETQ_FP_LRO_RESERVED) {
		fp->disable_tpa = 1;
		bp->esx.avail_lro_netq += 1;
	} else if (fp->esx.netq_flags & BNX2X_NETQ_FP_RSS_RESERVED) {
		bp->esx.avail_rss_netq += 1;
	}

	fp->esx.netq_flags &= ~(BNX2X_NETQ_RX_QUEUE_ALLOCATED |
				BNX2X_NETQ_FP_LRO_RESERVED |
				BNX2X_NETQ_FP_RSS_RESERVED);

	/* restart the rx/tx queue as tx only queue */
	rc = bnx2x_esx_setup_queue(bp, fp->index,
				   BNX2X_NETQ_IGNORE_START_RX_QUEUE);
	if (rc) {
		BNX2X_ERR("Could not restart queue:%d as Tx\n", fp->index);
		return;
	}
}

static void bnx2x_netq_free_rx_queue_rss(struct bnx2x *bp,
					    struct bnx2x_fastpath *fp)
{
	int i;

	if (!fp->esx.netq_in_use) {
		/* in case of RSS non-leading queues, they might have
		 * been freed already recursively while freeing
		 * RSS leading queue.
		 */
		DP(BNX2X_MSG_NETQ, "RSS[%d] not in use\n", fp->index);
		return;
	}

	for (i = bp->esx.rss_p_size - 1; i >=0 ; i--) {
		int rss_netq_idx = bp->esx.rss_netq_idx_tbl[i];
		if (!rss_netq_idx)
			break;
		if (fp->index == bp->esx.rss_netq_idx_tbl[0]) {
			/* Clear RSS pool indirection table for RSS queue */
			if (bnx2x_netq_clear_rss(bp))
				BNX2X_ERR("Failed to clear RSS ind tbl\n");
		}
		bnx2x_netq_free_rx_queue_single(bp, bp->fp + rss_netq_idx);
		bp->esx.rss_netq_idx_tbl[i] = 0;
	}
}

static void bnx2x_netq_free_rx_queue(struct bnx2x *bp,
					    struct bnx2x_fastpath *fp)
{
	if (fp->esx.netq_flags & BNX2X_NETQ_FP_RSS_RESERVED) {
		bnx2x_netq_free_rx_queue_rss(bp, fp);
	}  else {
		bnx2x_netq_free_rx_queue_single(bp, fp);
	}
}

static int bnx2x_dynamic_alloc_rx_queue_single(struct bnx2x *bp,
					   u32 feat, int rss_netq_idx)
{
	struct list_head *free_netq_elem;
	struct netq_list *free_netq;
	struct bnx2x_fastpath *fp;
	int rc = 0, non_leading_rss_q = 0;

	/*
	 * Leading RSS queue is known to VMKernel, and is taken
	 * from free_netq_list. Non-leading RSS queues are reserved
	 * for driver internal usage, and never get exposed to kernel.
	 */
	if (feat == BNX2X_NETQ_FP_RSS_RESERVED &&
	    rss_netq_idx != 0) {
		fp = &bp->fp[BNX2X_NUM_RX_NETQUEUES(bp) + rss_netq_idx];
		non_leading_rss_q = 1;
		goto restart_q;
	}

	if (list_empty(&bp->esx.free_netq_list)) {
		BNX2X_ERR("NetQ RX Queue %d >= BNX2X_NUM_RX_NETQUEUES(%d)\n",
			  bp->esx.n_rx_queues_allocated,
			  BNX2X_NUM_RX_NETQUEUES(bp));
		goto alloc_rxq_fail0;
	}

	/* use one of the unused queue if available */
	free_netq_elem = (struct list_head *)(&bp->esx.free_netq_list)->next;
	free_netq = (struct netq_list *)free_netq_elem;
	fp = free_netq->fp;
	BUG_ON(fp->esx.netq_in_use);
	fp->esx.netq_in_use = free_netq;
	/* remove from the free netq list  */
	list_del_init(free_netq_elem);

	fp->esx.netq_flags |= BNX2X_NETQ_RX_QUEUE_ALLOCATED;
	bp->esx.n_rx_queues_allocated++;

  restart_q:
	/* stop the queue */
	rc = bnx2x_esx_stop_queue(bp, fp->index);
	if (rc) {
		BNX2X_ERR("Could not stop queue:%d\n", fp->index);
		goto alloc_rxq_fail1;
	}

	/* Note that stop and setup rx queue uses configuration flags
	 * make sure configuration flags change is done between stop and setup
	 * (e.g. check disable_tpa to alloc/free tpa_pool)
	 */
	if (feat == BNX2X_NETQ_FP_LRO_RESERVED) {
		fp->disable_tpa = 0;
		fp->esx.netq_flags |= BNX2X_NETQ_FP_LRO_RESERVED;
		bp->esx.avail_lro_netq -= 1;
	} else if (feat == BNX2X_NETQ_FP_RSS_RESERVED) {
		bp->esx.rss_netq_idx_tbl[rss_netq_idx] = fp->index;
		fp->esx.netq_flags |= BNX2X_NETQ_FP_RSS_RESERVED;
		bp->esx.avail_rss_netq -= 1;
	}

	/* restart as tx/rx queue */
	rc = bnx2x_esx_setup_queue(bp, fp->index,
				   BNX2X_NETQ_START_RX_QUEUE);
	if (rc) {
		BNX2X_ERR("Could not start queue:%d\n", fp->index);
		goto alloc_rxq_fail2;
	}

	DP(BNX2X_MSG_NETQ,
	   "RX NetQ allocated on %d with feature 0x%x\n", fp->index, feat);
	return fp->index;

alloc_rxq_fail2:
	if (feat == BNX2X_NETQ_FP_LRO_RESERVED) {
		fp->disable_tpa = 1;
		fp->esx.netq_flags &= ~BNX2X_NETQ_FP_LRO_RESERVED;
		bp->esx.avail_lro_netq += 1;
	} else if (feat == BNX2X_NETQ_FP_RSS_RESERVED) {
		fp->esx.netq_flags &= ~BNX2X_NETQ_FP_RSS_RESERVED;
		bp->esx.avail_rss_netq += 1;
	}

alloc_rxq_fail1:
	if (!non_leading_rss_q) {
		list_add((struct list_head *)(fp->esx.netq_in_use),
			 &bp->esx.free_netq_list);
		fp->esx.netq_in_use = NULL;
		bp->esx.n_rx_queues_allocated--;

		fp->esx.netq_flags &= ~BNX2X_NETQ_RX_QUEUE_ALLOCATED;
	}
alloc_rxq_fail0:
	return -1;
}

static int bnx2x_dynamic_alloc_rx_queue_rss(struct bnx2x *bp, u32 feat)
{
	int i, rc = -1;

	for (i = bp->esx.rss_p_size - 1; i >= 0; i--) {
		rc = bnx2x_dynamic_alloc_rx_queue_single(bp, feat, i);

		if (rc < 0) {
			for (i++; i < bp->esx.rss_p_size; i++) {
				bnx2x_netq_free_rx_queue_single(bp, bp->fp + bp->esx.rss_netq_idx_tbl[i]);
			}
			return rc;
		}
	}

	/* Leading RSS queue gets allocated last.
	 * XXX: If not, RSS dispatching is not happening.
	 */
	return rc;
}

static int bnx2x_dynamic_alloc_rx_queue(struct bnx2x *bp, u32 feat)
{
	if (feat == BNX2X_NETQ_FP_RSS_RESERVED) {
		return bnx2x_dynamic_alloc_rx_queue_rss(bp, feat);
	} else {
		return bnx2x_dynamic_alloc_rx_queue_single(bp, feat, 0);
	}
}


static int bnx2x_alloc_rx_queue(struct net_device *netdev,
					   vmknetddi_queueops_queueid_t *p_qid,
					   struct napi_struct **napi_p,
					   u32 feat)
{
	struct bnx2x *bp;
	int netq_idx;

	if (feat && (!netdev || !p_qid || !napi_p)) {
		printk(KERN_ERR
			"Invalid parameters! netdev(%p) p_qid(%p) napi_p(%p)\n",
			netdev, p_qid, napi_p);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	bp = netdev_priv(netdev);

	switch (feat) {
	case 0:  /* no feature */
#if (VMWARE_ESX_DDK_VERSION >= 50000)
	case BNX2X_NETQ_FP_RSS_RESERVED:
#endif
		break;
#if (VMWARE_ESX_DDK_VERSION >= 41000)
	case BNX2X_NETQ_FP_LRO_RESERVED:
		if (bp->esx.avail_lro_netq < 1 ||
		    bp->disable_tpa ||
		    (bp->flags & TPA_ENABLE_FLAG) == 0) {
			DP(BNX2X_MSG_NETQ,
			   "no free rx queues with feature LRO found!\n");
			return VMKNETDDI_QUEUEOPS_ERR;
		}
		break;
#endif
	default:
		BNX2X_ERR("%s: No free NetQ with unknown feature\n",
			  bp->dev->name);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	/* Need to stop and restart statistics when restarting queue(s)
     * (i.e. restarting tx only queues as tx/rx only)
	 */
	bnx2x_stats_handle(bp, STATS_EVENT_STOP);
	netq_idx = bnx2x_dynamic_alloc_rx_queue(bp, feat);
	bnx2x_stats_handle(bp, STATS_EVENT_LINK_UP);

	if (netq_idx <= 0 || netq_idx > BNX2X_NUM_RX_NETQUEUES(bp))
		return VMKNETDDI_QUEUEOPS_ERR;


	/* set up indirection table in case of RSS */
	if (feat == BNX2X_NETQ_FP_RSS_RESERVED) {
		/* Set RSS pool indirection table for RSS queue is enabled */
		if (bnx2x_netq_set_rss(bp)) {
			BNX2X_ERR("Failed to set RSS ind tbl\n");
			bnx2x_netq_free_rx_queue(bp, &(bp->fp[netq_idx]));
			return VMKNETDDI_QUEUEOPS_ERR;
		}
	}
	*p_qid = VMKNETDDI_QUEUEOPS_MK_RX_QUEUEID(netq_idx);
	*napi_p = &(bp->fp[netq_idx].napi);

	DP(BNX2X_MSG_NETQ,
	   "RX NetQ allocated on %d with feature 0x%x\n", netq_idx, feat);
	return VMKNETDDI_QUEUEOPS_OK;
}

int bnx2x_esx_init_netqs(struct bnx2x *bp)
{
	int rc = 0;
	int i;
	spin_lock(&bp->esx.netq_lock);
	for_each_tx_net_queue(bp, i) {
		rc = bnx2x_esx_setup_queue(bp, i,
				  BNX2X_NETQ_IGNORE_START_RX_QUEUE);
		if (rc) {
			int j;
			BNX2X_ERR("Could not start tx netq[%d]:%d\n", rc, i);
			/* free the ones we have set up so far */
			for (j = 1; j < i; j++)
				bnx2x_esx_stop_queue(bp, j);
			break;
		}
	}
	/* If BNX2X_ESX_DYNAMIC_NETQ, this will be the first time
	 * bp->esx.number_of_mac_filters being initialized.
	 */
	bp->esx.number_of_mac_filters =
		bnx2x_get_filters_per_queue(bp);

	spin_unlock(&bp->esx.netq_lock);
	return rc;
}

#endif /* BNX2X_ESX_DYNAMIC_NETQ */

int bnx2x_esx_get_num_vfs(struct bnx2x *bp)
{
#ifdef BNX2X_ESX_SRIOV
	int pos;
	int req_vfs = 0;
	int param_vfs = BNX2X_GET_MAX_VFS(bp->esx.index);

	if ((!IS_MF(bp) && (param_vfs > BNX2X_MAX_NUM_OF_VFS)) ||
	    (IS_MF_SD(bp) && (param_vfs > (BNX2X_MAX_NUM_OF_VFS))) ||
	    (IS_MF_SI(bp) && (param_vfs > (BNX2X_MAX_NUM_OF_VFS/4))) ||
	    (param_vfs < 0)) {
		BNX2X_ERR("Invalid max_vfs value for index %d: %d\n",
			  bp->esx.index, param_vfs);

		return 0;
	}

	pos = bnx2x_vmk_pci_find_ext_capability(bp->pdev, PCI_EXT_CAP_ID_SRIOV);
	if (!pos || param_vfs == 0) {
		return 0;
	} else {
		u16 t_vf = 0;

		/** Limit the number of requested VFs by the totoal number
		 *  of VFs appearing in the configuration space
		 */
		pci_read_config_word(bp->pdev, pos + PCI_SRIOV_TOTAL_VF, &t_vf);
		req_vfs = min_t(int, BNX2X_MAX_NUM_OF_VFS, (int)t_vf);
	}

	if (param_vfs)
		req_vfs = min_t(int, req_vfs, param_vfs);

	return req_vfs;
#else
	return 0;
#endif
}

void
bnx2x_vmk_set_netdev_features(struct bnx2x *bp)
{
	static int index;

	bp->esx.index = index;
	index++;
#if (VMWARE_ESX_DDK_VERSION >= 55000) /* ! BNX2X_UPSTREAM */
	if (index < BNX2X_MAX_NIC) {
		bnx2x_fwdmp_bp[index].bp = bp;
		bnx2x_fwdmp_bp[index].disable_fwdmp = 0;
	}
#endif

#if (VMWARE_ESX_DDK_VERSION < 55000)
	if ((IS_MF_FCOE_AFEX(bp) || IS_MF_FCOE_SD(bp)) && !debug_unhide_nics)
		bp->dev->features |= NETIF_F_HIDDEN_UPLINK;
#endif

#if (VMWARE_ESX_DDK_VERSION >= 50000)
	if (((!IS_MF_SI(bp) && enable_default_queue_filters == 1)) ||
	    (IS_MF_SI(bp) && !(enable_default_queue_filters == 0))) {
		bp->dev->features |= NETIF_F_DEFQ_L2_FLTR;
		DP(BNX2X_MSG_NETQ, "Enabled Default L2 filters\n");
	}
#endif
#ifdef BNX2X_ESX_SRIOV
	if (bnx2x_vmk_pci_find_ext_capability(bp->pdev, PCI_EXT_CAP_ID_SRIOV) &&
	    IS_PF(bp) && bnx2x_esx_get_num_vfs(bp)) {
#if (VMWARE_ESX_DDK_VERSION < 55000)
		if (!debug_unhide_nics)
			bp->dev->features |= NETIF_F_HIDDEN_UPLINK;
#endif

		/*  Disable iSCSI for the PF if in SR-IOV mode */
		bp->flags |= NO_ISCSI_FLAG | NO_ISCSI_OOO_FLAG;

#if (VMWARE_ESX_DDK_VERSION < 55000)
		bp->flags |= NO_FCOE_FLAG;
#endif

		/*  Fsetor PF to VF commmunicate to occur the RX queue
		 *  must be setup with TX-switching enabled and
		 *  the MAC address must be pushed to the HW
		 */
		bp->flags |= TX_SWITCHING;
		bp->dev->features |= NETIF_F_DEFQ_L2_FLTR;
	}
#endif
#if (VMWARE_ESX_DDK_VERSION >= 55000)
#ifdef ENC_SUPPORTED
	if (!CHIP_IS_E1x(bp) && enable_vxlan_ofld) {
		bp->dev->features |= NETIF_F_ENCAP;
		DP(BNX2X_MSG_NETQ, "Enabled VXLAN TSO/CSO offload\n");
	}
#endif
#endif
}

#ifdef BNX2X_VMWARE_BMAPILNX
int
bnx2x_ioctl_cim(struct net_device *dev, struct ifreq *ifr)
{
	struct bnx2x *bp = netdev_priv(dev);
	void __user *useraddr = ifr->ifr_data;
	struct bnx2x_ioctl_req req;
	int rc = 0;
	u32 val;

	/* We assume all userworld apps are 32 bits ==> ifr->ifr_data is
	   a 32 bit pointer so we truncate useraddr */
	unsigned long uaddr32 = (unsigned long)useraddr & 0xffffffff;

	if (copy_from_user(&req, (void __user *)uaddr32, sizeof(req))) {
		BNX2X_ERR("Could not copy from user\n");
		return -EFAULT;
	}

	DP(NETIF_MSG_LINK, "CIM cmd: 0x%x\n", req.cmd);

	switch(req.cmd) {
	case BNX2X_VMWARE_CIM_CMD_ENABLE_NIC:
		DP(NETIF_MSG_LINK, "enable NIC\n");

		rc = bnx2x_open(bp->dev);
		break;
	case BNX2X_VMWARE_CIM_CMD_DISABLE_NIC:
		DP(NETIF_MSG_LINK, "disable NIC\n");

		rc = bnx2x_close(bp->dev);
		break;
	case BNX2X_VMWARE_CIM_CMD_REG_READ: {
		u32 mem_len;

		mem_len = pci_resource_len(bp->pdev, 0);
		if (mem_len < req.cmd_req.reg_read.reg_offset) {
			BNX2X_ERR("reg read: "
				  "out of range: max reg: 0x%x "
				  "req reg: 0x%x\n",
				  mem_len, req.cmd_req.reg_read.reg_offset);
			rc = -EINVAL;
			break;
		}

		val = REG_RD(bp, req.cmd_req.reg_read.reg_offset);
		req.cmd_req.reg_read.reg_value = val;

		DP(NETIF_MSG_LINK, "reg read: reg: 0x%x value:0x%x\n",
				   req.cmd_req.reg_read.reg_offset,
				   req.cmd_req.reg_read.reg_value);

		break;
	} case BNX2X_VMWARE_CIM_CMD_REG_WRITE: {
		u32 mem_len;

		mem_len = pci_resource_len(bp->pdev, 0);
		if (mem_len < req.cmd_req.reg_write.reg_offset) {
			BNX2X_ERR("reg write: "
				  "out of range: max reg: 0x%x "
				  "req reg: 0x%x\n",
				  mem_len, req.cmd_req.reg_write.reg_offset);
			rc = -EINVAL;
			break;
		}
		DP(NETIF_MSG_LINK, "reg write: reg: 0x%x value:0x%x\n",
				   req.cmd_req.reg_write.reg_offset,
				   req.cmd_req.reg_write.reg_value);

		REG_WR(bp, req.cmd_req.reg_write.reg_offset,
			   req.cmd_req.reg_write.reg_value);

		break;
	} case BNX2X_VMWARE_CIM_CMD_GET_NIC_PARAM:
		DP(NETIF_MSG_LINK, "get NIC param\n");

		req.cmd_req.get_nic_param.mtu = dev->mtu;
		memcpy(req.cmd_req.get_nic_param.current_mac_addr,
		       dev->dev_addr,
		       sizeof(req.cmd_req.get_nic_param.current_mac_addr));
		break;
	case BNX2X_VMWARE_CIM_CMD_GET_NIC_STATUS:
		DP(NETIF_MSG_LINK, "get NIC status\n");

		req.cmd_req.get_nic_status.nic_status = netif_running(dev);
		break;

	case BNX2X_VMWARE_CIM_CMD_GET_PCI_CFG_READ:
		if (req.cmd_req.reg_read.reg_offset > 4096) {
			BNX2X_ERR("read PCI CFG: "
				  "out of range: max config space read == 4096 "
				  "req reg: 0x%x\n",
				  req.cmd_req.reg_read.reg_offset);
			rc = -EINVAL;
			break;
		}

		pci_read_config_dword(bp->pdev,
				      req.cmd_req.reg_read.reg_offset,
				      &val);
		req.cmd_req.reg_read.reg_value = val;

		DP(NETIF_MSG_LINK, "PCI CFG read: reg: 0x%x value:0x%x\n",
				   req.cmd_req.reg_read.reg_offset,
				   req.cmd_req.reg_read.reg_value);

		break;
	default:
		BNX2X_ERR("unknown req.cmd: 0x%x\n", req.cmd);
		rc = -EINVAL;
	}

	if (rc == 0 &&
	    copy_to_user((void __user *)uaddr32, &req, sizeof(req))) {
		BNX2X_ERR("couldn't copy result back to user ");
		return -EFAULT;
	}

	return rc;
}
#endif

#if (VMWARE_ESX_DDK_VERSION >= 50000)
/* including the default queue - qid 0*/
static inline int bnx2x_netq_valid_rx_qid(struct bnx2x *bp, u16 qid)
{
	return (qid <= BNX2X_NUM_RX_NETQUEUES(bp));
}
#else
/* not including the default queue - qid 0*/
static inline int bnx2x_netq_valid_rx_qid(struct bnx2x *bp, u16 qid)
{
	return ((qid > 0) && (qid <= BNX2X_NUM_RX_NETQUEUES(bp)));
}
#endif
static inline int bnx2x_netq_valid_tx_qid(struct bnx2x *bp, u16 qid)
{
	return ((qid > 0) && (qid <= BNX2X_NUM_TX_NETQUEUES(bp)));
}

static int bnx2x_num_active_rx_mac_filters(struct bnx2x_fastpath *fp)

{
	struct bnx2x *bp = fp->bp;
	int i, count = 0;

	for (i = 0; i < bp->esx.number_of_mac_filters; i++) {
		if (BNX2X_IS_NETQ_RX_FILTER_ACTIVE(fp, i))
			count++;
	}

	return count;
}

static int bnx2x_is_last_rx_mac_filter(struct bnx2x_fastpath *fp)
{
	return (bnx2x_num_active_rx_mac_filters(fp) == 1 ? 1 : 0);
}

static void bnx2x_add_rx_mac_entry(struct netq_mac_filter *filter,
				  u8 *mac)
{
	memcpy(filter->mac, mac, ETH_ALEN);
	filter->flags |= BNX2X_NETQ_RX_FILTER_ACTIVE;
}

static void bnx2x_clear_rx_mac_entry(struct netq_mac_filter *filter)
{
	memset(filter->mac, 0, ETH_ALEN);
	filter->flags &= ~BNX2X_NETQ_RX_FILTER_ACTIVE;
}

static int bnx2x_find_rx_mac_filter_add(struct bnx2x *bp,
					struct bnx2x_fastpath *fp, u8 *mac)
{
	int i;

	if (fp->esx.mac_filters == NULL) {
		if (bp->esx.number_of_mac_filters == 0) {
			bp->esx.number_of_mac_filters =
					bnx2x_get_filters_per_queue(bp);

			if (unlikely(bp->esx.number_of_mac_filters == 0)) {
				DP(BNX2X_MSG_NETQ,
				   "number_of_mac_filters == 0\n");
				return -ENODEV;
			} else {
				DP(BNX2X_MSG_NETQ,
				   "NetQueue assigned per filters: %d\n",
				   bp->esx.number_of_mac_filters);
			}
		}

		fp->esx.mac_filters = kzalloc(sizeof(struct netq_mac_filter) *
					  bp->esx.number_of_mac_filters,
					  GFP_ATOMIC);
		if (fp->esx.mac_filters == NULL) {
			DP(BNX2X_MSG_NETQ, "Failed to allocate "
					   "RX MAC filter table\n");
			return -ENOMEM;
		}
		DP(BNX2X_MSG_NETQ, "Allocated RX MAC filter table with %d "
				   "entries on RX queue %d\n",
				   bp->esx.number_of_mac_filters, fp->index);

		bnx2x_add_rx_mac_entry(&fp->esx.mac_filters[0], mac);
		return 0;
	}

	for (i = 0; i < bp->esx.number_of_mac_filters; i++) {
		if (!BNX2X_IS_NETQ_RX_FILTER_ACTIVE(fp, i)) {
			bnx2x_add_rx_mac_entry(&fp->esx.mac_filters[i], mac);
			return i;
		}
	}

	DP(BNX2X_MSG_NETQ, "RX filters on NetQ RX Queue %d exhausted\n",
	   fp->index);
	return -ENODEV;
}

static void bnx2x_remove_rx_mac_filter(struct bnx2x *bp,
				       struct bnx2x_fastpath *fp, u16 fid,
				       int is_last_mac_filter)
{
	fp->esx.mac_filters[fid].flags &= ~BNX2X_NETQ_RX_FILTER_ACTIVE;

	if (is_last_mac_filter) {
		fp->esx.netq_flags &= ~BNX2X_NETQ_RX_QUEUE_ACTIVE;
		kfree(fp->esx.mac_filters);
		fp->esx.mac_filters = NULL;

		DP(BNX2X_MSG_NETQ, "Freed RX MAC filter table\n");
	}
}

#if !defined(BNX2X_ESX_DYNAMIC_NETQ)
static void bnx2x_reserve_netq_feature(struct bnx2x *bp)
{
	int i;

#if (VMWARE_ESX_DDK_VERSION >= 41000)
	int num_lro_reserved = 0;
#endif

	if (bp->esx.rss_p_num && bp->esx.rss_p_size) {
#if (VMWARE_ESX_DDK_VERSION >= 50000)
		/* Validate we have enough queues to support RSS */
		if (BNX2X_NUM_RX_NETQUEUES(bp) <= 0) {
			BNX2X_ERR("Not enough queues to support RSS. Disabling RSS\n");
			bp->esx.rss_p_num = 0;
			bp->esx.rss_p_size = 0;
		}
		/* Reserve last queues for RSS (if needed) */
		for (i = BNX2X_NUM_RX_NETQUEUES(bp) - bp->esx.rss_p_num + 1;
		     i <= BNX2X_NUM_RX_NETQUEUES(bp); i++) {
			struct bnx2x_fastpath *fp = &bp->fp[i];
			fp->esx.netq_flags |= BNX2X_NETQ_FP_RSS_RESERVED;
			fp->esx.rss_p_lead_idx = i - (BNX2X_NUM_RX_NETQUEUES(bp)
						  - bp->esx.rss_p_num);
			DP(BNX2X_MSG_NETQ, "Queue[%d] is reserved for RSS\n", i);
		}
#else
		BNX2X_ERR("DDK does not support RSS\n");
		bp->esx.rss_p_num = 0;
		bp->esx.rss_p_size = 0;
#endif
	}

	/*  Set the proper options for the default queue */
	bp->fp[0].disable_tpa = 1;
	bp->fp[0].esx.netq_flags |= BNX2X_NETQ_RX_QUEUE_ALLOCATED;
	bp->fp[0].esx.netq_flags &= ~BNX2X_NETQ_FP_FEATURES_RESERVED_MASK;

	/* Set features for non-RSS queues */
	for (i = 1; i <= BNX2X_NUM_RX_NETQUEUES(bp) - bp->esx.rss_p_num; i++) {
		struct bnx2x_fastpath *fp = &bp->fp[i];
	#if (VMWARE_ESX_DDK_VERSION < 41000)
		fp->disable_tpa = 1;
	#else
		/* clear features flags */
		fp->esx.netq_flags &= ~BNX2X_NETQ_FP_FEATURES_RESERVED_MASK;

		/* Always start the LRO queue from queue #2 because
		 * in the MF case there is 1 default queue and
		 * 1 NetQueue.  This NetQueue has to be a non-LRO queue
		 * otherwise the ESX load balancer will loop trying for
		 * a LRO queue/fail and ultimately no NetQueue
		 * will be used. */
		if (((i % 2) == 0) &&
		    (num_lro_reserved <
		    BNX2X_NETQ_FP_LRO_RESERVED_MAX_NUM(bp)) &&
		    (bp->flags & TPA_ENABLE_FLAG)) {
			fp->esx.netq_flags |= BNX2X_NETQ_FP_LRO_RESERVED;
			DP(BNX2X_MSG_NETQ, "Queue[%d] is reserved "
					   "for LRO\n", i);
			num_lro_reserved++;
		} else {
			/* either TPA is glabally disabled or we
			 * already reserved the max number of LRO queues
			 */
			DP(BNX2X_MSG_NETQ, "Queue[%d] is non-LRO\n", i);
			fp->disable_tpa = 1;
		}
	#endif
	}

	/* In RSS mode, last queues after BNX2X_NUM_RX_NETQUEUES
	 * cannot support tpa
	 */
	for (i = BNX2X_NUM_RX_NETQUEUES(bp)+1;
	     i < BNX2X_NUM_ETH_QUEUES(bp); i++) {
		struct bnx2x_fastpath *fp = &bp->fp[i];
		fp->disable_tpa = 1;
	}
}
#endif /* !BNX2X_ESX_DYNAMIC_NETQ */

static int bnx2x_get_netqueue_features(vmknetddi_queueop_get_features_args_t *args)
{
	args->features = VMKNETDDI_QUEUEOPS_FEATURE_NONE;
	args->features |= VMKNETDDI_QUEUEOPS_FEATURE_RXQUEUES;
	args->features |= VMKNETDDI_QUEUEOPS_FEATURE_TXQUEUES;
	return VMKNETDDI_QUEUEOPS_OK;
}

static int bnx2x_get_queue_count(vmknetddi_queueop_get_queue_count_args_t *args)
{
	struct bnx2x *bp = netdev_priv(args->netdev);

	if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX) {
		args->count = max_t(u16, BNX2X_NUM_RX_NETQUEUES(bp), 0);
		return VMKNETDDI_QUEUEOPS_OK;

	} else if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_TX) {
		args->count = max_t(u16, BNX2X_NUM_TX_NETQUEUES(bp), 0);
		return VMKNETDDI_QUEUEOPS_OK;

	} else {
		DP(BNX2X_MSG_NETQ, "invalid queue type: %x\n", args->type);
		return VMKNETDDI_QUEUEOPS_ERR;
	}
}

static int bnx2x_get_total_filters(struct bnx2x *bp)
{
	int total_filters = 0;

	/*  Calculate the number of credits left */
	total_filters = bp->macs_pool.check(&bp->macs_pool);
	if (total_filters > 3)
		total_filters = total_filters - 3;
	else
		total_filters = 0;

	return total_filters;
}

static int bnx2x_get_filters_per_queue(struct bnx2x *bp)
{
	int total_filters = 0, filter_cnt;
	int reported_netq_count = BNX2X_NUM_RX_NETQUEUES(bp);

	if ((BNX2X_NUM_RX_NETQUEUES(bp) == 0 && !IS_MF_FCOE_AFEX(bp)) ||
	    (BNX2X_NUM_RX_NETQUEUES(bp) == 0 && IS_MF_FCOE_AFEX(bp) &&
	     !BNX2X_IS_MF_SD_PROTOCOL_FCOE(bp))) {
		return 0;
	}

	if (multi_rx_filters == 0) {
		DP(BNX2X_MSG_NETQ, "Multiple RX filters disabled forced to "
				   "use only 1 filter.\n");
		return 1;
	}

	/*  Calculate the number of credits left */
	total_filters = bnx2x_get_total_filters(bp);

	DP(BNX2X_MSG_NETQ, "Total Avaiable filters: %d\n", total_filters);
	DP(BNX2X_MSG_NETQ, "# NetQueue: %d\n", reported_netq_count);
	filter_cnt = (total_filters / reported_netq_count);

	if (multi_rx_filters > 0) {
		if (filter_cnt < multi_rx_filters)
			DP(BNX2X_MSG_NETQ, "Forced RX filter setting %d higher "
					   "then checked value %d\n",
					   multi_rx_filters,
					   filter_cnt);
		filter_cnt = min_t(int, filter_cnt, multi_rx_filters);
	}
	return filter_cnt;
}

static int bnx2x_get_filter_count
	(vmknetddi_queueop_get_filter_count_args_t *args)
{
	struct bnx2x *bp;
	int rc;

	bp = netdev_priv(args->netdev);


#if defined(BNX2X_ESX_CNA)
	if (args->netdev->features & NETIF_F_CNA) {
		args->count = 2;
		rc = VMKNETDDI_QUEUEOPS_OK;
		goto done;
	}
#endif
	/* If !BNX2X_ESX_DYNAMIC_NETQ, this will be the first time
	 * bp->esx.number_of_mac_filters being initialized.
	 */
	bp->esx.number_of_mac_filters =
		bnx2x_get_filters_per_queue(bp);
	args->count = bp->esx.number_of_mac_filters;

	DP(BNX2X_MSG_NETQ, "NetQueue assigned per filters: %d\n",
	   args->count);

	rc = VMKNETDDI_QUEUEOPS_OK;

done:
	BNX2X_ESX_PRINT_END_TIMESTAMP();
	return rc;
}

#if !defined(BNX2X_ESX_DYNAMIC_NETQ)
static int bnx2x_alloc_rx_queue(struct net_device *netdev,
				vmknetddi_queueops_queueid_t *p_qid,
				struct napi_struct **napi_p)
{
	struct bnx2x *bp = netdev_priv(netdev);
	int i, rc;

	if (bp->esx.n_rx_queues_allocated >= BNX2X_NUM_RX_NETQUEUES(bp)) {
		BNX2X_ERR("NetQ RX Queue %d >= BNX2X_NUM_RX_NETQUEUES(%d)\n",
			  bp->esx.n_rx_queues_allocated,
			  BNX2X_NUM_RX_NETQUEUES(bp));
		rc = VMKNETDDI_QUEUEOPS_ERR;
		goto done;
	}

	for_each_rx_net_queue(bp, i) {
		struct bnx2x_fastpath *fp = &bp->fp[i];

		if ((!BNX2X_IS_NETQ_RX_QUEUE_ALLOCATED(fp)) &&
		    BNX2X_IS_NETQ_FP_FEAT_NONE_RESERVED(fp)) {
			fp->esx.netq_flags |= BNX2X_NETQ_RX_QUEUE_ALLOCATED;
			bp->esx.n_rx_queues_allocated++;
			*p_qid = VMKNETDDI_QUEUEOPS_MK_RX_QUEUEID(fp->index);
			*napi_p = &fp->napi;

			DP(BNX2X_MSG_NETQ, "RX NetQ allocated on %d\n", i);
			return VMKNETDDI_QUEUEOPS_OK;
		}
	}
	DP(BNX2X_MSG_NETQ, "No free rx queues found!\n");
	rc = VMKNETDDI_QUEUEOPS_ERR;

done:
	BNX2X_ESX_PRINT_END_TIMESTAMP();
	return rc;
}
#endif /*!BNX2X_ESX_DYNAMIC_NETQ*/

static int bnx2x_alloc_tx_queue(struct net_device *netdev,
				vmknetddi_queueops_queueid_t *p_qid,
				u16 *queue_mapping)
{
	struct bnx2x *bp = netdev_priv(netdev);
	int i, rc;

	if (bp->esx.n_tx_queues_allocated >= BNX2X_NUM_TX_NETQUEUES(bp)) {
		BNX2X_ERR("NetQ TX Queue %d >= BNX2X_NUM_TX_NETQUEUES(%d)\n",
			  bp->esx.n_tx_queues_allocated,
			  BNX2X_NUM_TX_NETQUEUES(bp));
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	for_each_tx_net_queue(bp, i) {
		struct bnx2x_fastpath *fp = &bp->fp[i];

		if (!BNX2X_IS_NETQ_TX_QUEUE_ALLOCATED(fp)) {
			fp->esx.netq_flags |= BNX2X_NETQ_TX_QUEUE_ALLOCATED;
			bp->esx.n_tx_queues_allocated++;
			*p_qid = VMKNETDDI_QUEUEOPS_MK_TX_QUEUEID(fp->index);
			*queue_mapping = fp->index;

			DP(BNX2X_MSG_NETQ, "TX NetQ allocated on %d\n", i);
			rc = VMKNETDDI_QUEUEOPS_OK;
			goto done;
		}
	}
	DP(BNX2X_MSG_NETQ, "No free tx queues found!\n");
	rc = VMKNETDDI_QUEUEOPS_ERR;
done:
	BNX2X_ESX_PRINT_END_TIMESTAMP();
	return rc;
}

static int bnx2x_alloc_queue(vmknetddi_queueop_alloc_queue_args_t *args)
{

	if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_TX)
		return bnx2x_alloc_tx_queue(args->netdev, &args->queueid,
					    &args->queue_mapping);

	else if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX)
#ifdef BNX2X_ESX_DYNAMIC_NETQ
		return bnx2x_alloc_rx_queue(args->netdev, &args->queueid,
					    &args->napi, 0);
#else
		return bnx2x_alloc_rx_queue(args->netdev, &args->queueid,
					    &args->napi);
#endif /* BNX2X_ESX_DYNAMIC_NETQ */
	else {
		struct bnx2x *bp = netdev_priv(args->netdev);

		DP(BNX2X_MSG_NETQ, "invalid queue type: %x\n", args->queueid);
		return VMKNETDDI_QUEUEOPS_ERR;
	}
}

#if (VMWARE_ESX_DDK_VERSION >= 41000) && !defined(BNX2X_ESX_DYNAMIC_NETQ)

static int bnx2x_alloc_rx_queue_with_feat(struct net_device *netdev,
					   vmknetddi_queueops_queueid_t *p_qid,
					   struct napi_struct **napi_p,
					   u32 feat)
{
	int i, rc;
	struct bnx2x *bp;

	if (!netdev || !p_qid || !napi_p) {
		printk(KERN_ERR "bnx2x_alloc_rx_queue_with_feat: "
			"Invalid parameters! netdev(%p) p_qid(%p) napi_p(%p)\n",
			netdev, p_qid, napi_p);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	bp = netdev_priv(netdev);
	if (bp->esx.n_rx_queues_allocated >= BNX2X_NUM_RX_NETQUEUES(bp)) {
		BNX2X_ERR("NetQ RX Queue %d >= BNX2X_NUM_RX_NETQUEUES(%d)\n",
			  bp->esx.n_rx_queues_allocated,
			  BNX2X_NUM_RX_NETQUEUES(bp));
		rc = VMKNETDDI_QUEUEOPS_ERR;
		goto done;
	}

	for_each_rx_net_queue(bp, i) {
		struct bnx2x_fastpath *fp = &bp->fp[i];
		/* if this Rx queue is not used */
		if (!BNX2X_IS_NETQ_RX_QUEUE_ALLOCATED(fp) &&
		    BNX2X_IS_NETQ_FP_FEAT(fp, feat)) {
			fp->esx.netq_flags |= BNX2X_NETQ_RX_QUEUE_ALLOCATED;
			bp->esx.n_rx_queues_allocated++;
			*p_qid = VMKNETDDI_QUEUEOPS_MK_RX_QUEUEID(fp->index);
			*napi_p = &fp->napi; /* FIXME - what about RSS? */
			switch(feat) {
			case BNX2X_NETQ_FP_LRO_RESERVED:
				DP(BNX2X_MSG_NETQ,
				   "RX NetQ allocated on %d with LRO feature\n",
				   i);
				break;
#if (VMWARE_ESX_DDK_VERSION >= 50000)
			case BNX2X_NETQ_FP_RSS_RESERVED:
				DP(BNX2X_MSG_NETQ,
				   "RX NetQ allocated on %d with RSS feature\n",
				   i);
				break;
#endif
			default:
				printk(KERN_WARNING "%s: RX NetQ allocated on %d with unknown feature\n",
				       bp->dev->name, i);
			}

			rc = VMKNETDDI_QUEUEOPS_OK;
			goto done;
		}
	}
	DP(BNX2X_MSG_NETQ, "no free rx queues with feature 0x%08x found!\n",
	   feat);
	rc = VMKNETDDI_QUEUEOPS_ERR;
done:
	BNX2X_ESX_PRINT_END_TIMESTAMP();
	return rc;
}
#endif /* (VMWARE_ESX_DDK_VERSION >= 41000) && !defined(BNX2X_ESX_DYNAMIC_NETQ) */

#if (VMWARE_ESX_DDK_VERSION >= 55000)
#if defined(BNX2X_ESX_DYNAMIC_NETQ)
#define BNX2X_NETQ_SUPPORTED_FEATURES(bp) \
	(VMKNETDDI_QUEUEOPS_QUEUE_FEAT_PAIR | \
	 (((bp)->esx.rss_p_num && (bp)->esx.rss_p_size) ? \
		(disable_rss_dyn ? \
			VMKNETDDI_QUEUEOPS_QUEUE_FEAT_RSS : \
			VMKNETDDI_QUEUEOPS_QUEUE_FEAT_RSS_DYN) : 0) | \
	 (((bp)->esx.number_of_mac_filters > 1) ? \
		VMKNETDDI_QUEUEOPS_QUEUE_FEAT_DYNAMIC : 0) | \
	 (disable_feat_preemptible ? \
	       0 : VMKNETDDI_QUEUEOPS_QUEUE_FEAT_PREEMPTIBLE) | \
	(((bp)->flags & TPA_ENABLE_FLAG) ? \
		VMKNETDDI_QUEUEOPS_QUEUE_FEAT_LRO : 0))
#else
#define BNX2X_NETQ_SUPPORTED_FEATURES(bp) \
	(VMKNETDDI_QUEUEOPS_QUEUE_FEAT_PAIR | \
	 (((bp)->esx.rss_p_num && (bp)->esx.rss_p_size) ? \
		(disable_rss_dyn ? \
			VMKNETDDI_QUEUEOPS_QUEUE_FEAT_RSS : \
			VMKNETDDI_QUEUEOPS_QUEUE_FEAT_RSS_DYN) : 0) | \
	 ((((bp)->flags & TPA_ENABLE_FLAG) && \
	   (bnx2x_get_lro_netqueue_count(bp) > 0)) ? \
		VMKNETDDI_QUEUEOPS_QUEUE_FEAT_LRO : 0))
#endif
#elif (VMWARE_ESX_DDK_VERSION >= 50000)
#define BNX2X_NETQ_SUPPORTED_FEATURES(bp) \
	(VMKNETDDI_QUEUEOPS_QUEUE_FEAT_PAIR | \
	 (((bp)->esx.rss_p_num && (bp)->esx.rss_p_size) ? \
		VMKNETDDI_QUEUEOPS_QUEUE_FEAT_RSS : 0) | \
	 ((((bp)->flags & TPA_ENABLE_FLAG) && \
	   (bnx2x_get_lro_netqueue_count(bp) > 0)) ? \
		VMKNETDDI_QUEUEOPS_QUEUE_FEAT_LRO : 0))
#elif (VMWARE_ESX_DDK_VERSION >= 41000)
#define BNX2X_NETQ_SUPPORTED_FEATURES(bp) \
	 ((((bp)->flags & TPA_ENABLE_FLAG) && \
	   (bnx2x_get_lro_netqueue_count(bp) > 0)) ? \
		VMKNETDDI_QUEUEOPS_QUEUE_FEAT_LRO : 0)
#else
#define BNX2X_NETQ_SUPPORTED_FEATURES(bp) \
	(VMKNETDDI_QUEUEOPS_QUEUE_FEAT_NONE)
#endif

/*
 * bnx2x_alloc_queue_with_attr - Alloc queue with NETQ features.
 *
 */
static int
bnx2x_alloc_queue_with_attr(
			vmknetddi_queueop_alloc_queue_with_attr_args_t *args)
{
	int i, rc;
	struct bnx2x *bp = netdev_priv(args->netdev);
	vmknetddi_queueops_queue_features_t feat;
	u32 req_feat;

	if (!args->attr || !args->nattr) {
		BNX2X_ERR("Attributes are invalid! attr(%p), nattr(%d).\n",
			args->attr, args->nattr);
		rc = VMKNETDDI_QUEUEOPS_ERR;
		goto done;
	}

	DP(BNX2X_MSG_NETQ, "Attributes number: %d\n", args->nattr);
	for (i = 0; i < args->nattr; i++) {
		DP(BNX2X_MSG_NETQ, "Attribute[%d] type: 0x%x\n",
				i, args->attr[i].type);
		switch (args->attr[i].type) {
		case VMKNETDDI_QUEUEOPS_QUEUE_ATTR_PRIOR:
			/* Nothing to do */
			BNX2X_ERR("VMKNETDDI_QUEUEOPS_QUEUE_ATTR_PRIOR "
				"isn't supported now.\n");
			break;
		case VMKNETDDI_QUEUEOPS_QUEUE_ATTR_FEAT:
			feat = args->attr[i].args.features;
			DP(BNX2X_MSG_NETQ, "Features 0x%x needed.\n", feat);

			/* Unsupported features */
			if (feat & ~BNX2X_NETQ_SUPPORTED_FEATURES(bp)) {
				BNX2X_ERR("Failed... "
					"unsupported feature 0x%x\n",
					feat & ~BNX2X_NETQ_SUPPORTED_FEATURES(bp));
				return VMKNETDDI_QUEUEOPS_ERR;
			}

#if (VMWARE_ESX_DDK_VERSION >= 50000)
#if (VMWARE_ESX_DDK_VERSION >= 55000)
			if (feat & (VMKNETDDI_QUEUEOPS_QUEUE_FEAT_LRO |
				    VMKNETDDI_QUEUEOPS_QUEUE_FEAT_RSS_DYN |
				    VMKNETDDI_QUEUEOPS_QUEUE_FEAT_RSS)) {
#else
			if (feat & (VMKNETDDI_QUEUEOPS_QUEUE_FEAT_LRO |
				    VMKNETDDI_QUEUEOPS_QUEUE_FEAT_RSS)) {
#endif  /* VMWARE_ESX_DDK_VERSION >= 55000 */
				if (feat & VMKNETDDI_QUEUEOPS_QUEUE_FEAT_LRO)
					req_feat = BNX2X_NETQ_FP_LRO_RESERVED;
				else { /* RSS */
					req_feat = BNX2X_NETQ_FP_RSS_RESERVED;
				}
				if (args->type !=
					    VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX) {
					BNX2X_ERR("Invalid queue type, feature is only for RX queue\n");
					break;
				}
#ifdef BNX2X_ESX_DYNAMIC_NETQ
				return bnx2x_alloc_rx_queue(
						args->netdev, &args->queueid,
						&args->napi, req_feat);
#else
				return bnx2x_alloc_rx_queue_with_feat(
						args->netdev, &args->queueid,
						&args->napi, req_feat);
#endif  /* BNX2X_ESX_DYNAMIC_NETQ */
			}
#else
			if (feat & (VMKNETDDI_QUEUEOPS_QUEUE_FEAT_LRO)) {
				if (feat & VMKNETDDI_QUEUEOPS_QUEUE_FEAT_LRO)
					req_feat = BNX2X_NETQ_FP_LRO_RESERVED;
				if (args->type !=
					    VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX) {
					BNX2X_ERR("Invalid queue type, feature is only for RX queue\n");
					break;
				}
				return bnx2x_alloc_rx_queue_with_feat(
					args->netdev, &args->queueid,
					&args->napi, req_feat);
			}
#endif  /* VMWARE_ESX_DDK_VERSION >= 50000 */
			/* No feature isn't allowed */
			if (!feat)
				BNX2X_ERR("Invalid feature: features is NONE!\n");
			break;
		default:
			BNX2X_ERR("Invalid attribute type\n");
			break;
		}
	}
	BNX2X_ERR("No queue is allocated.\n");
	rc = VMKNETDDI_QUEUEOPS_ERR;
done:
	BNX2X_ESX_PRINT_END_TIMESTAMP();
	return rc;
}

#if !defined(BNX2X_ESX_DYNAMIC_NETQ)
int
bnx2x_get_lro_netqueue_count(struct bnx2x *bp)
{
	int i, count = 0;

	for_each_rx_net_queue(bp, i) {
		struct bnx2x_fastpath *fp = &bp->fp[i];

		if (fp->esx.netq_flags & BNX2X_NETQ_FP_LRO_RESERVED)
			count++;
	}

	return count;
}
#endif /* !BNX2X_ESX_DYNAMIC_NETQ */

static int
bnx2x_get_supported_feature(vmknetddi_queueop_get_sup_feat_args_t *args)
{
	struct bnx2x *bp = netdev_priv(args->netdev);

	args->features = BNX2X_NETQ_SUPPORTED_FEATURES(bp);
#if (VMWARE_ESX_DDK_VERSION >= 55000)
	DP(BNX2X_MSG_NETQ, "Netq features supported: %s%s%s%s%s%s%s\n",
	   (args->features & VMKNETDDI_QUEUEOPS_QUEUE_FEAT_LRO) ? "LRO " : "",
	   (args->features & VMKNETDDI_QUEUEOPS_QUEUE_FEAT_PAIR) ? "PAIR " : "",
	   (args->features & VMKNETDDI_QUEUEOPS_QUEUE_FEAT_RSS) ? "RSS " : "",
	   (args->features & VMKNETDDI_QUEUEOPS_QUEUE_FEAT_DYNAMIC) ?
	     "DYNAMIC " :  "",
	   (args->features &	VMKNETDDI_QUEUEOPS_QUEUE_FEAT_RSS_DYN) ?
	     "RSS_DYN " : "",
	   (args->features & VMKNETDDI_QUEUEOPS_QUEUE_FEAT_PREEMPTIBLE) ?
	     "PREEMPTIBLE " :  "",
	   (args->features) ? "" : "NONE");
#elif (VMWARE_ESX_DDK_VERSION >= 50000)
	DP(BNX2X_MSG_NETQ, "Netq features supported: %s%s%s%s\n",
	   (args->features & VMKNETDDI_QUEUEOPS_QUEUE_FEAT_LRO) ? "LRO " : "",
	   (args->features & VMKNETDDI_QUEUEOPS_QUEUE_FEAT_PAIR) ? "PAIR " : "",
	   (args->features & VMKNETDDI_QUEUEOPS_QUEUE_FEAT_RSS) ? "RSS " : "",
	   (args->features) ? "" : "NONE");
#else
	DP(BNX2X_MSG_NETQ, "Netq features supported: %s%s\n",
	   (args->features & VMKNETDDI_QUEUEOPS_QUEUE_FEAT_LRO) ? "LRO " : "",
	   (args->features) ? "" : "NONE");
#endif

	return VMKNETDDI_QUEUEOPS_OK;
}

#if (VMWARE_ESX_DDK_VERSION >= 50000)
static int
bnx2x_get_supported_filter_class(vmknetddi_queueop_get_sup_filter_class_args_t *args)
{
	args->class = VMKNETDDI_QUEUEOPS_FILTER_MACADDR;

	return VMKNETDDI_QUEUEOPS_OK;
}
#endif

static int bnx2x_free_tx_queue(struct net_device *netdev,
			       vmknetddi_queueops_queueid_t qid)
{
	struct bnx2x *bp = netdev_priv(netdev);
	u16 index = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(qid);
	struct bnx2x_fastpath *fp;
	int rc;

	if (!bnx2x_netq_valid_tx_qid(bp, index)) {
		DP(BNX2X_MSG_NETQ, "%d not a valid TX QID\n", index);
		rc = VMKNETDDI_QUEUEOPS_ERR;
		goto done;
	}

	fp = &bp->fp[index];

	if (!BNX2X_IS_NETQ_TX_QUEUE_ALLOCATED(fp)) {
		DP(BNX2X_MSG_NETQ, "NetQ TX Queue %d is not allocated\n",
		   index);
		rc = VMKNETDDI_QUEUEOPS_ERR;
		goto done;
	}

	fp->esx.netq_flags &= ~BNX2X_NETQ_TX_QUEUE_ALLOCATED;

	if (bp->esx.n_tx_queues_allocated)
		bp->esx.n_tx_queues_allocated--;

	DP(BNX2X_MSG_NETQ, "Free NetQ TX Queue: %x netq_flags: 0%x\n",
	   index, fp->esx.netq_flags);
	DP(BNX2X_MSG_NETQ, "Free NetQ TX Queue: %x\n", index);
	rc = VMKNETDDI_QUEUEOPS_OK;

done:
	BNX2X_ESX_PRINT_END_TIMESTAMP();
	return rc;
}

#if !defined(BNX2X_ESX_DYNAMIC_NETQ)
static inline void bnx2x_netq_free_rx_queue(struct bnx2x *bp,
					    struct bnx2x_fastpath *fp)
{
	fp->esx.netq_flags &= ~BNX2X_NETQ_RX_QUEUE_ALLOCATED;

	if (bp->esx.n_rx_queues_allocated)
		bp->esx.n_rx_queues_allocated--;
}
#endif /* !BNX2X_ESX_DYNAMIC_NETQ */

static int bnx2x_free_rx_queue(struct net_device *netdev,
			       vmknetddi_queueops_queueid_t qid)
{
	struct bnx2x *bp = netdev_priv(netdev);
	u16 index = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(qid);
	struct bnx2x_fastpath *fp;
	int rc;

	if (!bnx2x_netq_valid_rx_qid(bp, index)) {
		DP(BNX2X_MSG_NETQ, "NetQ RX Queue %d is invalid\n", index);
		rc = VMKNETDDI_QUEUEOPS_ERR;
		goto done;
	}

	fp = &bp->fp[index];

	if (!BNX2X_IS_NETQ_RX_QUEUE_ALLOCATED(fp)) {
		DP(BNX2X_MSG_NETQ, "NetQ RX Queue %d is not allocated\n",
		   index);
		rc = VMKNETDDI_QUEUEOPS_ERR;
		goto done;
	}

	/* Need to stop and restart statistics when freeing rx queue(s)
	 * (i.e. restarting tx/rx queues as tx only)
	 */
	bnx2x_stats_handle(bp, STATS_EVENT_STOP);
	bnx2x_netq_free_rx_queue(bp, fp);
	bnx2x_stats_handle(bp, STATS_EVENT_LINK_UP);

	DP(BNX2X_MSG_NETQ, "Free NetQ RX Queue: %x, netq_flags: 0%x\n",
	   index, fp->esx.netq_flags);
	rc = VMKNETDDI_QUEUEOPS_OK;
done:
	BNX2X_ESX_PRINT_END_TIMESTAMP();
	return rc;
}

static int bnx2x_free_queue(vmknetddi_queueop_free_queue_args_t *args)
{
	if (VMKNETDDI_QUEUEOPS_IS_TX_QUEUEID(args->queueid))
		return bnx2x_free_tx_queue(args->netdev, args->queueid);

	else if (VMKNETDDI_QUEUEOPS_IS_RX_QUEUEID(args->queueid))
		return bnx2x_free_rx_queue(args->netdev, args->queueid);

	else {
		struct net_device *netdev = args->netdev;
		struct bnx2x *bp = netdev_priv(netdev);

		DP(BNX2X_MSG_NETQ, "invalid queue type: %x\n", args->queueid);
		return VMKNETDDI_QUEUEOPS_ERR;
	}
}

static int bnx2x_get_queue_vector(vmknetddi_queueop_get_queue_vector_args_t *args)
{
	struct net_device *netdev = args->netdev;
	struct bnx2x *bp = netdev_priv(netdev);
	int qid;

	qid = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args->queueid);

	/* may be invoked also for the default queue */
	/* Assuming can be called for Rx and Tx both; use Max limit*/
	if (qid > max_t(int, BNX2X_NUM_TX_NETQUEUES(bp),
			BNX2X_NUM_RX_NETQUEUES(bp))) {
		DP(BNX2X_MSG_NETQ, "NetQ Queue %d is invalid\n", qid);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	/* msix_table indices:
	 * 0 - default SB (slow-path operations)
	 * 1 - CNIC fast-path operations (if compiled in)
	 * 2 - Max NetQs - Net-queues starting form the default queue
	 */
	qid += (1 + CNIC_SUPPORT(bp));

	args->vector = bp->msix_table[qid].vector;
	return VMKNETDDI_QUEUEOPS_OK;
}

static int bnx2x_get_default_queue(vmknetddi_queueop_get_default_queue_args_t *args)
{
	struct net_device *netdev = args->netdev;
	struct bnx2x *bp = netdev_priv(netdev);

	if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX) {
		args->queueid = VMKNETDDI_QUEUEOPS_MK_RX_QUEUEID(0);
		args->napi = &bp->fp[0].napi;
		return VMKNETDDI_QUEUEOPS_OK;

	} else if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_TX) {
		args->queueid = VMKNETDDI_QUEUEOPS_MK_TX_QUEUEID(0);
		args->queue_mapping = 0;
		return VMKNETDDI_QUEUEOPS_OK;

	} else
		return VMKNETDDI_QUEUEOPS_ERR;
}

#if (VMWARE_ESX_DDK_VERSION >= 50000 && !defined(BNX2X_ESX_DYNAMIC_NETQ))
static int bnx2x_netq_set_rss(struct bnx2x *bp, struct bnx2x_fastpath *fp)
{
	int i, offset;

	/* Set the RSS pool indirection table */
	for (i = 0; i < sizeof(bp->rss_conf_obj.ind_table); i++) {
		bp->rss_conf_obj.ind_table[i] = fp->cl_id;
		offset = ethtool_rxfh_indir_default(i, bp->esx.rss_p_size);
		/* If more than 1 RSS pool exists, the 'leaded' queues will not
		 * be consecutive to the leading queue, but rather will reside
		 * after leading queues of all RSS pools (in order).
		 */
		if (offset) {
			offset += (bp->esx.rss_p_num - fp->esx.rss_p_lead_idx) +
				  ((fp->esx.rss_p_lead_idx - 1) *
				   (bp->esx.rss_p_size - 1));
			bp->rss_conf_obj.ind_table[i] += offset;
		}
	}

	if (bnx2x_esx_rss(bp, &bp->rss_conf_obj, true, true))
		return VMKNETDDI_QUEUEOPS_ERR;

	return VMKNETDDI_QUEUEOPS_OK;
}

int bnx2x_netq_init_rss(struct bnx2x *bp)
{
	/* Validate that indeed RSS can be supported */
	if (!bp->esx.rss_p_num || !bp->esx.rss_p_size)
		return VMKNETDDI_QUEUEOPS_OK;

	if (bp->esx.rss_p_num != 1 || bp->esx.rss_p_size < 2 ||
	    bp->esx.rss_p_size > MAX_RSS_P_SIZE) {
		BNX2X_ERR("Cannot configure %d[%d] RSS pools\n",
			  bp->esx.rss_p_num, bp->esx.rss_p_size);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	if (BNX2X_NUM_RX_NETQUEUES(bp) <= 0) {
		BNX2X_ERR("Not enough queues to support RSS\n");
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	/* Configure RSS - notice that once more than 1 pool will be required
	 * things will have to change, since every bp contains a single conf_obj
	 * based on the assumption it will only need a single RSS engine.
	 */
	return bnx2x_netq_set_rss(bp, &bp->fp[BNX2X_NUM_RX_NETQUEUES(bp)]);
}
#endif /* VMWARE_ESX_DDK_VERSION >= 50000 && !BNX2X_ESX_DYNAMIC_NETQ */

static inline int bnx2x_netq_set_mac_one(u8 *mac, struct bnx2x *bp,
					 struct bnx2x_fastpath *fp, bool add)
{
	unsigned long ramrod_flags = 0;

	set_bit(RAMROD_COMP_WAIT, &ramrod_flags);

	if (fp->index == 0 && IS_SRIOV(bp))
		bnx2x_set_mac_one(bp, mac, &bnx2x_sp_obj(bp, fp).mac_obj, add,
				  BNX2X_ETH_MAC, &ramrod_flags);

	return bnx2x_set_mac_one(bp, mac, &bnx2x_sp_obj(bp, fp).mac_obj, add,
				 BNX2X_NETQ_ETH_MAC, &ramrod_flags);
}

static int bnx2x_netq_remove_rx_filter(struct bnx2x *bp,
				       struct bnx2x_fastpath *fp, u16 fid)
{
	unsigned long ramrod_flags = 0;
	struct bnx2x_queue_state_params qstate = {NULL};
	u8 *macaddr;
	int is_last_mac_filter, rc;
	DECLARE_MAC_BUF(mac);

	if (fid >= bp->esx.number_of_mac_filters) {
		DP(BNX2X_MSG_NETQ, "Couldn't remove invalid RX filter %d "
		   "on NetQ RX Queue %d\n", fid, fp->index);
		return VMKNETDDI_QUEUEOPS_ERR;
	} else {
		if (fp->esx.mac_filters == NULL) {
			DP(BNX2X_MSG_NETQ, "Error Freeing RX filter "
					   "with empty RX MAC filter table "
					   "on RX queue: %d fid: %d\n",
					   fp->index, fid);
			return VMKNETDDI_QUEUEOPS_OK;
		}

		macaddr = fp->esx.mac_filters[fid].mac;

		DP(BNX2X_MSG_NETQ, "NetQ remove RX filter: queue:%d mac:%s "
		   "filter id:%d]\n",
		   fp->index, print_mac(mac, macaddr), fid);
	}

	is_last_mac_filter = bnx2x_is_last_rx_mac_filter(fp);

	/* clear MAC */

	/* set to drop-all*/
	if (is_last_mac_filter) {
		set_bit(RAMROD_RX, &ramrod_flags);
		set_bit(RAMROD_TX, &ramrod_flags);
		set_bit(RAMROD_COMP_WAIT, &ramrod_flags);
		rc = bnx2x_set_q_rx_mode(bp, fp->cl_id, 0, 0, 0, ramrod_flags);

		if (rc) {
			BNX2X_ERR("NetQ could not remove RX filter, "
				  "rx mode failed: queue:%d mac:%s "
				  "filter id:%d]\n",
				  fp->index, print_mac(mac, macaddr), fid);
			return VMKNETDDI_QUEUEOPS_ERR;
		}
	}

	/* delete MAC */
	rc = bnx2x_netq_set_mac_one(macaddr, bp, fp, 0);
	if (rc) {
		BNX2X_ERR("NetQ could not remove RX filter: queue:%d mac:%s "
			  "filter id:%d]\n",
			  fp->index, print_mac(mac, macaddr), fid);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	if (fp->index != 0) {
		/* send empty-ramrod to flush packets lurking in the HW */
		qstate.q_obj = &bnx2x_sp_obj(bp, fp).q_obj;
		ramrod_flags = 0;
		set_bit(RAMROD_COMP_WAIT, &ramrod_flags);
		qstate.ramrod_flags = ramrod_flags; /* wait for completion */
		qstate.cmd = BNX2X_Q_CMD_EMPTY;
		if (bnx2x_queue_state_change(bp, &qstate)) {
			BNX2X_ERR("RX %d queue state not changed for fid: %d\n",
				  fp->index, fid);
			return VMKNETDDI_QUEUEOPS_ERR;
		}
	}
	bnx2x_remove_rx_mac_filter(bp, fp, fid, is_last_mac_filter);

	return VMKNETDDI_QUEUEOPS_OK;
}

static int bnx2x_remove_rx_filter(vmknetddi_queueop_remove_rx_filter_args_t *args)
{
	struct bnx2x *bp = netdev_priv(args->netdev);
	u16 qid = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args->queueid);
	u16 fid = VMKNETDDI_QUEUEOPS_FILTERID_VAL(args->filterid);
	struct bnx2x_fastpath *fp = &bp->fp[qid];
	int rc;

	/*  For ESX5.x the defalut queue type is not correctly set.
	 *  Only check the queue type for non-default queue
	 */
	if (qid != 0 &&
	    !VMKNETDDI_QUEUEOPS_IS_RX_QUEUEID(args->queueid)) {
		BNX2X_ERR("Queue ID %d is not RX queue\n", args->queueid);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	/* not invoked for the default queue before ESX 5.0*/
	if (!bnx2x_netq_valid_rx_qid(bp, qid)) {
		DP(BNX2X_MSG_NETQ, "NetQ RX Queue %d is invalid\n", qid);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	/* Verify the queue is allocated and has an active filter */
	if (!BNX2X_IS_NETQ_RX_QUEUE_ACTIVE(fp) ||
	    !BNX2X_IS_NETQ_RX_QUEUE_ALLOCATED(fp)) {
		DP(BNX2X_MSG_NETQ,
		   "NetQ RX Queue %d is not allocated/active 0x%x\n",
		   qid, fp->esx.netq_flags);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	/* Do the work */
	rc = bnx2x_netq_remove_rx_filter(bp, fp, fid);
	DP(BNX2X_MSG_NETQ, "NetQ %d remove RX filter %d, rc: %d\n",
	   qid, fid, rc);

	return rc;
}


static int bnx2x_apply_rx_filter(vmknetddi_queueop_apply_rx_filter_args_t *args)
{
	u8 *macaddr;
	struct bnx2x *bp = netdev_priv(args->netdev);
	struct bnx2x_fastpath *fp;
	u16 qid = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args->queueid);
	u16 vlan_id;
	unsigned long accept_flags = 0, ramrod_flags = 0;
	int filter_id, rc;

	vmknetddi_queueops_filter_class_t filter;
	DECLARE_MAC_BUF(mac);

	if (qid != 0 &&
	    !VMKNETDDI_QUEUEOPS_IS_RX_QUEUEID(args->queueid)) {
		BNX2X_ERR("Queue ID %d is not RX queue\n", args->queueid);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	filter = vmknetddi_queueops_get_filter_class(&args->filter);
	if (filter != VMKNETDDI_QUEUEOPS_FILTER_MACADDR) {
		BNX2X_ERR("Queue filter %x not MACADDR filter\n", filter);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	/* not invoked for the default queue before ESX 5.0*/
	if (!bnx2x_netq_valid_rx_qid(bp, qid)) {
		DP(BNX2X_MSG_NETQ, "NetQ RX Queue %d is invalid\n", qid);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	fp = &bp->fp[qid];

	if (!BNX2X_IS_NETQ_RX_QUEUE_ALLOCATED(fp)) {
		BNX2X_ERR("Trying to apply filter on non allocated Queue %d\n",
			  qid);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	macaddr = (void *)vmknetddi_queueops_get_filter_macaddr(&args->filter);
	vlan_id = vmknetddi_queueops_get_filter_vlanid(&args->filter);

	filter_id = bnx2x_find_rx_mac_filter_add(bp, fp, macaddr);
	if (filter_id < 0) {
		BNX2X_ERR("NetQ could not add RX filter, no filters "
			  "avaliable: queue:%d mac:%s "
			  "vlan id:%d]\n",
			   qid, print_mac(mac, macaddr), vlan_id);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	if (bnx2x_num_active_rx_mac_filters(fp) == 1) {
		/* set to recv-unicast */
		set_bit(BNX2X_ACCEPT_UNICAST, &accept_flags);
		set_bit(BNX2X_ACCEPT_ANY_VLAN, &accept_flags);
		set_bit(RAMROD_RX, &ramrod_flags);
		set_bit(RAMROD_TX, &ramrod_flags);
		set_bit(RAMROD_COMP_WAIT, &ramrod_flags);

#if (VMWARE_ESX_DDK_VERSION >= 50000)
		if (qid == 0) {
			set_bit(BNX2X_ACCEPT_UNMATCHED, &accept_flags);
			set_bit(BNX2X_ACCEPT_ALL_MULTICAST, &accept_flags);
			set_bit(BNX2X_ACCEPT_BROADCAST, &accept_flags);
		}
#endif

		rc = bnx2x_set_q_rx_mode(bp, fp->cl_id, 0, accept_flags,
					 accept_flags, ramrod_flags);

		if (rc) {
			BNX2X_ERR("NetQ could not add RX filter, "
				  " rx mode failed: queue:%d mac:%s "
				  "vlan id:%d filter id:%d]\n",
				   qid, print_mac(mac, macaddr), vlan_id,
				  filter_id);
			return VMKNETDDI_QUEUEOPS_ERR;
		}
	}

	/* add MAC */
	rc = bnx2x_netq_set_mac_one(macaddr, bp, fp, 1);
	if (rc) {
		BNX2X_ERR("NetQ could not add RX filter: queue:%d mac:%s "
			  "vlan id:%d filter id:%d]\n",
			   qid, print_mac(mac, macaddr), vlan_id, filter_id);
		bnx2x_clear_rx_mac_entry(&fp->esx.mac_filters[filter_id]);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	fp->esx.netq_flags |= BNX2X_NETQ_RX_QUEUE_ACTIVE;
	args->filterid = VMKNETDDI_QUEUEOPS_MK_FILTERID(filter_id);
#if (VMWARE_ESX_DDK_VERSION >= 50000)
	/* Need by feature: VMKNETDDI_QUEUEOPS_FEATURE_PAIRQUEUE  */
	args->pairtxqid = qid;
#endif
	DP(BNX2X_MSG_NETQ, "NetQ set RX filter: queue:%d mac:%s "
			   "vlan id:%d filter id:%d]\n",
	   qid, print_mac(mac, macaddr), vlan_id, filter_id);

	return VMKNETDDI_QUEUEOPS_OK;
}

static int bnx2x_get_queue_stats(vmknetddi_queueop_get_stats_args_t *args)
{
	return VMKNETDDI_QUEUEOPS_ERR;
}

static int bnx2x_get_netqueue_version(vmknetddi_queueop_get_version_args_t *args)
{
	return vmknetddi_queueops_version(args);
}


void bnx2x_netq_clear_tx_rx_queues(struct bnx2x *bp)
{
	int i, j;
	struct bnx2x_fastpath *fp = &bp->fp[0];

	spin_lock(&bp->esx.netq_lock);

	if (fp->esx.mac_filters) {
		for (j = 0; j < bp->esx.number_of_mac_filters; j++) {
			if (fp->esx.mac_filters != NULL &&
			    BNX2X_IS_NETQ_RX_FILTER_ACTIVE(fp, j))
				bnx2x_netq_remove_rx_filter(bp, fp, j);
		}
	}

	for_each_rx_net_queue(bp, i) {
		fp = &bp->fp[i];

		if (BNX2X_IS_NETQ_RX_QUEUE_ACTIVE(fp)) {
			for (j = 0; j < bp->esx.number_of_mac_filters; j++)
				if (fp->esx.mac_filters != NULL &&
				    BNX2X_IS_NETQ_RX_FILTER_ACTIVE(fp, j))
					bnx2x_netq_remove_rx_filter(bp, fp, j);
		}

		if (BNX2X_IS_NETQ_RX_QUEUE_ALLOCATED(fp))
			bnx2x_netq_free_rx_queue(bp, fp);
	}

#ifdef BNX2X_ESX_DYNAMIC_NETQ
	for_each_tx_net_queue(bp, i)
		bnx2x_esx_stop_queue(bp, i);

#endif /* BNX2X_ESX_DYNAMIC_NETQ */
	spin_unlock(&bp->esx.netq_lock);
}

#if (VMWARE_ESX_DDK_VERSION >= 55000)
static int bnx2x_esx_rss_get_params(
		vmknetddi_queue_rssop_get_params_args_t *args)
{
	struct bnx2x *bp = netdev_priv(args->netdev);

	/*
	 * It is a bug that RSS netq operations get called if
	 * there is no RSS capability in the driver.
	 */
	BUG_ON(!bp->esx.rss_p_num || !RSS || !bp->esx.rss_p_size);

	args->num_rss_pools = bp->esx.rss_p_num;
	args->num_rss_queues_per_pool = bp->esx.rss_p_size;
	/* following fields are in bytes (not entries) */
	args->rss_hash_key_size = sizeof(bp->esx.rss_key_tbl);
	args->rss_ind_table_size = T_ETH_INDIRECTION_TABLE_SIZE;
	DP(BNX2X_MSG_NETQ,
		"VMKNETDDI_QUEUEOPS_RSS_OP_GET_PARAMS (%x, %x, %x, %x)\n",
		args->num_rss_pools,
		args->num_rss_queues_per_pool,
		args->rss_hash_key_size,
		args->rss_ind_table_size);

	return VMKNETDDI_QUEUEOPS_OK;
}
#if !defined(BNX2X_ESX_DYNAMIC_NETQ)
static int bnx2x_esx_rss_init_state(
	vmknetddi_queue_rssop_init_state_args_t *args)
{
	struct bnx2x *bp = netdev_priv(args->netdev);
	vmknetddi_queue_rssop_hash_key_t *rss_key = args->rss_key;
	vmknetddi_queue_rssop_ind_table_t *ind_table = args->rss_ind_table;
	struct bnx2x_config_rss_params params = {NULL};
	struct bnx2x_fastpath *fp = &bp->fp[BNX2X_NUM_RX_NETQUEUES(bp)];
	int i;
	u32 *key;
	u32 max_rss_qidx = fp->cl_id + bp->esx.rss_p_size;
	u8 tmp_ind_table[T_ETH_INDIRECTION_TABLE_SIZE];

	if (ind_table->table_size > VMKNETDDI_NETQUEUE_MAX_RSS_IND_TABLE_SIZE) {
		BNX2X_ERR("indirection table size > %x\n",
			VMKNETDDI_NETQUEUE_MAX_RSS_IND_TABLE_SIZE);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	/* Set the RSS pool indirection table */
	for (i = 0; i < T_ETH_INDIRECTION_TABLE_SIZE; i++) {
		tmp_ind_table[i] =
			(fp->cl_id +
			 ind_table->table[i % ind_table->table_size]) %
			 max_rss_qidx;
	}
	params.rss_obj = &bp->rss_conf_obj;
	bnx2x_esx_rss_config_common(bp, &params);

	memcpy(params.ind_table, tmp_ind_table, T_ETH_INDIRECTION_TABLE_SIZE);

	/* RSS keys */
	key = (u32 *)&rss_key->key;
	if (rss_key->key_size != sizeof(params.rss_key))
		return VMKNETDDI_QUEUEOPS_ERR;

	/* Program RSS keys (size in bytes) */
	memcpy(params.rss_key, key,  rss_key->key_size);

	__set_bit(BNX2X_RSS_SET_SRCH, &params.rss_flags);

	if (bnx2x_config_rss(bp, &params)) {
		BNX2X_ERR("Failed to configure RSS parameters!\n");
		return VMKNETDDI_QUEUEOPS_ERR;
	}
	memcpy(bp->rss_conf_obj.ind_table,
			tmp_ind_table, T_ETH_INDIRECTION_TABLE_SIZE);

	DP(BNX2X_MSG_NETQ, "VMKNETDDI_QUEUEOPS_RSS_OP_INIT_STATE (0x%x, 0x%x)\n",
	   rss_key->key_size, ind_table->table_size);

	return  VMKNETDDI_QUEUEOPS_OK;
}

static int bnx2x_esx_rss_ind_table_set(
	struct net_device *netdev, vmknetddi_queue_rssop_ind_table_t *ind_table)
{
	struct bnx2x *bp = netdev_priv(netdev);
	struct bnx2x_fastpath *fp = &bp->fp[BNX2X_NUM_RX_NETQUEUES(bp)];
	int i;
	u32 max_rss_qidx = fp->cl_id + bp->esx.rss_p_size;
	u8 tmp_ind_table[T_ETH_INDIRECTION_TABLE_SIZE];

	if (ind_table->table_size > VMKNETDDI_NETQUEUE_MAX_RSS_IND_TABLE_SIZE) {
		BNX2X_ERR("indirection table size > %x\n",
			VMKNETDDI_NETQUEUE_MAX_RSS_IND_TABLE_SIZE);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	/* Set the RSS pool indirection table */
	for (i = 0; i < T_ETH_INDIRECTION_TABLE_SIZE; i++)
		tmp_ind_table[i] =
			(fp->cl_id +
			 ind_table->table[i % ind_table->table_size]) %
			 max_rss_qidx;

	if (bnx2x_esx_rss(bp, &bp->rss_conf_obj, false, true)) {
		BNX2X_ERR("Failed to update RSS table!\n");
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	memcpy(bp->rss_conf_obj.ind_table,
			tmp_ind_table, T_ETH_INDIRECTION_TABLE_SIZE);

	DP(BNX2X_MSG_NETQ, "VMKNETDDI_QUEUEOPS_RSS_OP_UPDATE_IND_TABLE (%x)\n",
		ind_table->table_size);

	return VMKNETDDI_QUEUEOPS_OK;
}

static int bnx2x_esx_rss_ind_table_get(
	struct net_device *netdev, vmknetddi_queue_rssop_ind_table_t *ind_table)
{
	struct bnx2x *bp = netdev_priv(netdev);
	struct bnx2x_fastpath *fp = &bp->fp[BNX2X_NUM_RX_NETQUEUES(bp)];
	int i;

	if (ind_table->table_size > VMKNETDDI_NETQUEUE_MAX_RSS_IND_TABLE_SIZE) {
		BNX2X_ERR("indirection table size > %x\n",
			VMKNETDDI_NETQUEUE_MAX_RSS_IND_TABLE_SIZE);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	for (i = 0; i < sizeof(bp->rss_conf_obj.ind_table); i++)
		ind_table->table[i % ind_table->table_size] =
			bp->rss_conf_obj.ind_table[i] - fp->cl_id;

	DP(BNX2X_MSG_NETQ, "VMKNETDDI_QUEUEOPS_RSS_OP_GET_IND_TABLE (%x)\n",
		ind_table->table_size);
	return VMKNETDDI_QUEUEOPS_OK;
}
#else /* BNX2X_ESX_DYNAMIC_NETQ */

static int bnx2x_esx_rss_init_state(
	vmknetddi_queue_rssop_init_state_args_t *args)
{
	struct bnx2x *bp = netdev_priv(args->netdev);
	vmknetddi_queue_rssop_hash_key_t *rss_key = args->rss_key;
	vmknetddi_queue_rssop_ind_table_t *ind_table = args->rss_ind_table;
	int i;

	if (ind_table->table_size > VMKNETDDI_NETQUEUE_MAX_RSS_IND_TABLE_SIZE ||
	    rss_key->key_size != sizeof(bp->esx.rss_key_tbl)) {
		BNX2X_ERR("ind tbl size %d > %d || key_size %d != %d\n",
			  ind_table->table_size,
			  VMKNETDDI_NETQUEUE_MAX_RSS_IND_TABLE_SIZE,
			  rss_key->key_size,
			  (int)sizeof(bp->esx.rss_key_tbl));
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	/* Overwrite the RAW RSS pool indirection table provided by host*/
	for (i = 0; i < ind_table->table_size; i++) {
		bp->esx.rss_raw_ind_tbl[i] =
			(u8)ind_table->table[i % ind_table->table_size];
		if (bp->esx.rss_raw_ind_tbl[i] >= bp->esx.rss_p_size) {
			BNX2X_ERR("Invalid rss ind tbl entry[%d]:%d >= %d\n",
				  i, bp->esx.rss_raw_ind_tbl[i],
				  bp->esx.rss_p_size);
			return VMKNETDDI_QUEUEOPS_ERR;
		}
	}

	/* Save away RSS keys for later (size in bytes) */
	memcpy(bp->esx.rss_key_tbl, rss_key->key,  rss_key->key_size);

	DP(BNX2X_MSG_NETQ,
	   "VMKNETDDI_QUEUEOPS_RSS_OP_INIT_STATE "
	   "(%d %d %d %d %d %d %d %d - %d)"
	   "(%d %d %d %d %d %d %d %d - %d)\n",
	   rss_key->key[0], rss_key->key[1],
	   rss_key->key[2], rss_key->key[3],
	   rss_key->key[4], rss_key->key[5],
	   rss_key->key[6], rss_key->key[7],
	   rss_key->key_size,
	   ind_table->table[0], ind_table->table[1],
	   ind_table->table[2], ind_table->table[3],
	   ind_table->table[4], ind_table->table[5],
	   ind_table->table[6], ind_table->table[7],
	   ind_table->table_size);

	return  VMKNETDDI_QUEUEOPS_OK;
}

static int bnx2x_esx_rss_ind_table_set(
	struct net_device *netdev, vmknetddi_queue_rssop_ind_table_t *ind_table)
{
	int i;
	struct bnx2x *bp = netdev_priv(netdev);
	if (ind_table->table_size !=
	    VMKNETDDI_NETQUEUE_MAX_RSS_IND_TABLE_SIZE) {
		BNX2X_ERR("ind tbl size %d != %d\n",
			  ind_table->table_size,
			  VMKNETDDI_NETQUEUE_MAX_RSS_IND_TABLE_SIZE);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	/* Overwrite the RAW RSS pool indirection table provided by host*/
	for (i = 0; i < ind_table->table_size; i++) {
		bp->esx.rss_raw_ind_tbl[i] = (u8)ind_table->table[i];
		if (bp->esx.rss_raw_ind_tbl[i] >= bp->esx.rss_p_size) {
			BNX2X_ERR("Invalid rss ind tbl entry[%d]:%d >= %d\n",
				  i, bp->esx.rss_raw_ind_tbl[i],
				  bp->esx.rss_p_size);
			return VMKNETDDI_QUEUEOPS_ERR;
		}
	}

	if (bnx2x_esx_config_rss_pf(bp, bp->esx.rss_raw_ind_tbl, NULL))
		return VMKNETDDI_QUEUEOPS_ERR;

	DP(BNX2X_MSG_NETQ,
	   "VMKNETDDI_QUEUEOPS_RSS_OP_UPDATE_IND_TABLE: "
	   "(%d %d %d %d %d %d %d %d - %d)\n",
	   ind_table->table[0], ind_table->table[1],
	   ind_table->table[2], ind_table->table[3],
	   ind_table->table[4], ind_table->table[5],
	   ind_table->table[6], ind_table->table[7],
	   ind_table->table_size);

	return VMKNETDDI_QUEUEOPS_OK;
}

static int bnx2x_esx_rss_ind_table_get(
	struct net_device *netdev, vmknetddi_queue_rssop_ind_table_t *ind_table)
{
	struct bnx2x *bp = netdev_priv(netdev);
	int i;

	if (ind_table->table_size !=
	    VMKNETDDI_NETQUEUE_MAX_RSS_IND_TABLE_SIZE) {
		BNX2X_ERR("indirection table size != %d\n",
			  VMKNETDDI_NETQUEUE_MAX_RSS_IND_TABLE_SIZE);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	for (i = 0; i < ind_table->table_size; i++) {
		ind_table->table[i] =  bp->esx.rss_raw_ind_tbl[i];
	}

	DP(BNX2X_MSG_NETQ,
	   "VMKNETDDI_QUEUEOPS_RSS_OP_GET_IND_TABLE:"
	   "(%d %d %d %d %d %d %d %d - %d)\n",
	   ind_table->table[0], ind_table->table[1],
	   ind_table->table[2], ind_table->table[3],
	   ind_table->table[4], ind_table->table[5],
	   ind_table->table[6], ind_table->table[7],
	   ind_table->table_size);
	return VMKNETDDI_QUEUEOPS_OK;
}

/*
 * bnx2x_realloc_queue_with_attr - Alloc queue with NETQ features.
 *
 */
static int
bnx2x_realloc_queue_with_attr(
			vmknetddi_queueop_realloc_queue_with_attr_args_t *args)
{
	struct bnx2x *bp = netdev_priv(args->alloc_args->netdev);
	int rc;

	BUG_ON(disable_feat_preemptible);
	bnx2x_remove_rx_filter(args->rm_filters_args);

	bnx2x_free_rx_queue(
		args->alloc_args->netdev, args->alloc_args->queueid);

	rc =  bnx2x_alloc_queue_with_attr(args->alloc_args);

	bnx2x_apply_rx_filter(args->apply_rx_filter_args);

	DP(BNX2X_MSG_NETQ, "VMKNETDDI_QUEUEOPS_OP_REALLOC_QUEUE_WITH_ATTR: %d\n",
	   args->alloc_args->queueid);

	return rc;
}

/* bnx2x_get_filter_count_of_device - Get count of filter per device. */
static int bnx2x_get_filter_count_of_device(
		vmknetddi_queueop_get_filter_count_of_device_args_t *args)
{
	struct bnx2x *bp;
	int total_filters = 0;

	bp = netdev_priv(args->netdev);

	/*  Calculate the number of credits left */
	total_filters = bnx2x_get_total_filters(bp);

	args->filters_of_device_count = bnx2x_get_total_filters(bp);
	args->filters_per_queue_count = bp->esx.number_of_mac_filters;
	DP(BNX2X_MSG_NETQ,
	   "VMKNETDDI_QUEUEOPS_OP_GET_FILTER_COUNT_OF_DEVICE: (%d %d)\n",
	   args->filters_of_device_count, args->filters_per_queue_count);
	return VMKNETDDI_QUEUEOPS_OK;
}

#endif /* BNX2X_ESX_DYNAMIC_NETQ */

static int bnx2x_netqueue_rss_ops(vmknetddi_queueop_config_rss_args_t *args)
{
	struct net_device *netdev =
	((vmknetddi_queue_rssop_get_params_args_t *)(args->op_args))->netdev;
	struct bnx2x *bp = netdev_priv(netdev);
	int rc = VMKNETDDI_QUEUEOPS_ERR;

	spin_lock(&bp->esx.netq_lock);
	switch (args->op_type) {

	case VMKNETDDI_QUEUEOPS_RSS_OP_GET_PARAMS:
		rc = bnx2x_esx_rss_get_params(
		(vmknetddi_queue_rssop_get_params_args_t *)args->op_args);
		bp->esx.vmnic_rss_op_stats.get_params++;
		break;

	case VMKNETDDI_QUEUEOPS_RSS_OP_INIT_STATE:
		rc = bnx2x_esx_rss_init_state(
		(vmknetddi_queue_rssop_init_state_args_t *)args->op_args);
		bp->esx.vmnic_rss_op_stats.init_state++;
		break;

	case VMKNETDDI_QUEUEOPS_RSS_OP_UPDATE_IND_TABLE: {
		vmknetddi_queue_rssop_ind_table_args_t *targs =
			(vmknetddi_queue_rssop_ind_table_args_t *)args->op_args;
		rc = bnx2x_esx_rss_ind_table_set(targs->netdev,
						targs->rss_ind_table);
		bp->esx.vmnic_rss_op_stats.update_ind_table++;
		break;
	}

	case VMKNETDDI_QUEUEOPS_RSS_OP_GET_IND_TABLE: {
		vmknetddi_queue_rssop_ind_table_args_t *targs =
			(vmknetddi_queue_rssop_ind_table_args_t *)args->op_args;
		rc = bnx2x_esx_rss_ind_table_get(targs->netdev,
						targs->rss_ind_table);
		bp->esx.vmnic_rss_op_stats.get_ind_table++;
		break;
	}

	default:
		BNX2X_ERR("Unhandled RSS OP %d\n", args->op_type);
	}
	spin_unlock(&bp->esx.netq_lock);
	return rc;
}
#endif  /* VMWARE_ESX_DDK_VERSION >= 55000 */

int bnx2x_netqueue_ops(vmknetddi_queueops_op_t op, void *args)
{
	vmknetddi_queueop_get_queue_count_args_t *p;
	struct bnx2x *bp;
	int rc = VMKNETDDI_QUEUEOPS_ERR;

	if (op == VMKNETDDI_QUEUEOPS_OP_GET_VERSION)
		return bnx2x_get_netqueue_version(
			(vmknetddi_queueop_get_version_args_t *)args);
	else if(op == VMKNETDDI_QUEUEOPS_OP_GET_FEATURES)
		return bnx2x_get_netqueue_features(
			(vmknetddi_queueop_get_features_args_t *)args);
#if (VMWARE_ESX_DDK_VERSION >= 55000)
	else if (op == VMKNETDDI_QUEUEOPS_OP_CONFIG_RSS)
		return bnx2x_netqueue_rss_ops(
			(vmknetddi_queueop_config_rss_args_t *)args);
#endif
	p = (vmknetddi_queueop_get_queue_count_args_t *)args;
	bp = netdev_priv(p->netdev);

	BNX2X_ESX_PRINT_START_TIMESTAMP();

	spin_lock(&bp->esx.netq_lock);

	if (bp->state != BNX2X_STATE_OPEN) {
		DP(BNX2X_MSG_NETQ,
		   "Device not ready for NetQueue Ops: state: 0x%x op: 0x%x\n",
		   bp->state, op);
		spin_unlock(&bp->esx.netq_lock);
		return rc;
	}

	switch (op) {
	case VMKNETDDI_QUEUEOPS_OP_GET_QUEUE_COUNT:
		rc = bnx2x_get_queue_count(
			(vmknetddi_queueop_get_queue_count_args_t *)args);
		bp->esx.vmnic_netq_op_stats.get_queue_count++;
		break;

	case VMKNETDDI_QUEUEOPS_OP_GET_FILTER_COUNT:
		rc = bnx2x_get_filter_count(
			(vmknetddi_queueop_get_filter_count_args_t *)args);
		bp->esx.vmnic_netq_op_stats.get_filter_count++;
		break;

	case VMKNETDDI_QUEUEOPS_OP_ALLOC_QUEUE:
		rc = bnx2x_alloc_queue(
			(vmknetddi_queueop_alloc_queue_args_t *)args);
		bp->esx.vmnic_netq_op_stats.alloc_queue++;
		break;

	case VMKNETDDI_QUEUEOPS_OP_FREE_QUEUE:
		rc = bnx2x_free_queue(
			(vmknetddi_queueop_free_queue_args_t *)args);
		bp->esx.vmnic_netq_op_stats.free_queue++;
		break;

	case VMKNETDDI_QUEUEOPS_OP_GET_QUEUE_VECTOR:
		rc = bnx2x_get_queue_vector(
			(vmknetddi_queueop_get_queue_vector_args_t *)args);
		bp->esx.vmnic_netq_op_stats.get_queue_vector++;
		break;

	case VMKNETDDI_QUEUEOPS_OP_GET_DEFAULT_QUEUE:
		rc = bnx2x_get_default_queue(
			(vmknetddi_queueop_get_default_queue_args_t *)args);
		bp->esx.vmnic_netq_op_stats.get_default_queue++;
		break;

	case VMKNETDDI_QUEUEOPS_OP_APPLY_RX_FILTER:
		rc = bnx2x_apply_rx_filter(
			(vmknetddi_queueop_apply_rx_filter_args_t *)args);
		bp->esx.vmnic_netq_op_stats.apply_rx_filter++;
		break;

	case VMKNETDDI_QUEUEOPS_OP_REMOVE_RX_FILTER:
		rc = bnx2x_remove_rx_filter(
			(vmknetddi_queueop_remove_rx_filter_args_t *)args);
		bp->esx.vmnic_netq_op_stats.remove_rx_filter++;
		break;

	case VMKNETDDI_QUEUEOPS_OP_GET_STATS:
		rc = bnx2x_get_queue_stats(
			(vmknetddi_queueop_get_stats_args_t *)args);
		bp->esx.vmnic_netq_op_stats.get_stats++;
		break;

#if (VMWARE_ESX_DDK_VERSION >= 41000)
	case VMKNETDDI_QUEUEOPS_OP_ALLOC_QUEUE_WITH_ATTR:
		rc = bnx2x_alloc_queue_with_attr(
			(vmknetddi_queueop_alloc_queue_with_attr_args_t *)args);
		bp->esx.vmnic_netq_op_stats.alloc_queue_with_attr++;
		break;

	case VMKNETDDI_QUEUEOPS_OP_GET_SUPPORTED_FEAT:
		rc = bnx2x_get_supported_feature(
			(vmknetddi_queueop_get_sup_feat_args_t *)args);
		bp->esx.vmnic_netq_op_stats.get_supported_feat++;
		break;
#if (VMWARE_ESX_DDK_VERSION >= 50000)
	case VMKNETDDI_QUEUEOPS_OP_GET_SUPPORTED_FILTER_CLASS:
		rc = bnx2x_get_supported_filter_class(
			(vmknetddi_queueop_get_sup_filter_class_args_t *)args);
		bp->esx.vmnic_netq_op_stats.get_supported_filter_class++;
		break;
#ifdef BNX2X_ESX_DYNAMIC_NETQ
	case VMKNETDDI_QUEUEOPS_OP_REALLOC_QUEUE_WITH_ATTR:
		return bnx2x_realloc_queue_with_attr(
		    (vmknetddi_queueop_realloc_queue_with_attr_args_t *)args);
		bp->esx.vmnic_netq_op_stats.realloc_queue_with_attr++;
		break;
	case VMKNETDDI_QUEUEOPS_OP_GET_FILTER_COUNT_OF_DEVICE:
		rc = bnx2x_get_filter_count_of_device(
		 (vmknetddi_queueop_get_filter_count_of_device_args_t *)args);
		bp->esx.vmnic_netq_op_stats.get_filter_count_of_device++;
		break;
#endif
#endif
#endif

	default:
		BNX2X_ERR("Unhandled NETQUEUE OP 0x%x\n", op);
		rc = VMKNETDDI_QUEUEOPS_ERR;
		bp->esx.vmnic_netq_op_stats.unhandled++;
	}

	spin_unlock(&bp->esx.netq_lock);

	return rc;
}

#if defined(BNX2X_ESX_CNA) /* ! BNX2X_UPSTREAM */

static int bnx2x_cna_get_queue_count(
	vmknetddi_queueop_get_queue_count_args_t *args)
{
	struct net_device *netdev = args->netdev;
	struct bnx2x *bp = netdev->priv;

	if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX) {
		args->count = 2;
		return VMKNETDDI_QUEUEOPS_OK;

	} else if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_TX) {
		args->count = 2;
		return VMKNETDDI_QUEUEOPS_OK;

	} else {
		DP(BNX2X_MSG_NETQ, "invalid queue type: %x\n", args->type);
		return VMKNETDDI_QUEUEOPS_ERR;
	}
}
static int bnx2x_cna_alloc_rx_queue(struct net_device *netdev,
				    vmknetddi_queueops_queueid_t *p_qid,
				    struct napi_struct **napi_p)
{
	struct bnx2x *bp = netdev->priv;
	struct bnx2x_fastpath *fp = bnx2x_fcoe_fp(bp);

	if (!BNX2X_IS_NETQ_RX_QUEUE_ALLOCATED(fp)) {
		napi_synchronize(&bnx2x_fcoe(bp, napi));

		fp->esx.netq_flags |= BNX2X_NETQ_RX_QUEUE_ALLOCATED;
		*p_qid = VMKNETDDI_QUEUEOPS_MK_RX_QUEUEID(1);
		*napi_p = &fp->napi;

		DP(BNX2X_MSG_NETQ, "RX CNA NetQ allocated on %d\n", 1);
		return VMKNETDDI_QUEUEOPS_OK;
	}

	DP(BNX2X_MSG_NETQ, "No free CNA rx queues found!\n");
	return VMKNETDDI_QUEUEOPS_ERR;
}

static int bnx2x_cna_get_filter_count(vmknetddi_queueop_get_filter_count_args_t *args)
{
	args->count = 2;
	return VMKNETDDI_QUEUEOPS_OK;
}

static int bnx2x_cna_alloc_tx_queue(struct net_device *netdev,
				    vmknetddi_queueops_queueid_t *p_qid,
				    u16 *queue_mapping)
{
	struct bnx2x *bp = netdev->priv;
	struct bnx2x_fastpath *fp = bnx2x_fcoe_fp(bp);

	if (!BNX2X_IS_NETQ_TX_QUEUE_ALLOCATED(fp)) {
		fp->esx.netq_flags |= BNX2X_NETQ_TX_QUEUE_ALLOCATED;

		/*  TODO determine remapping */
		*p_qid = VMKNETDDI_QUEUEOPS_MK_TX_QUEUEID(1);
		*queue_mapping = 1;

		/* Start Tx */
		netif_tx_start_all_queues(netdev);

		DP(BNX2X_MSG_NETQ, "CNA NetQ TX Queue ID %d Mapping: %d\n",
			*p_qid, *queue_mapping);

		return VMKNETDDI_QUEUEOPS_OK;
	}

	DP(BNX2X_MSG_NETQ, "No free tx queues for CNA device found!\n");
	return VMKNETDDI_QUEUEOPS_ERR;
}

static int bnx2x_cna_alloc_queue(vmknetddi_queueop_alloc_queue_args_t *args)
{
	struct bnx2x *bp;

	if (args->netdev->features & NETIF_F_CNA)
		bp = args->netdev->priv;
	else
		bp = netdev_priv(args->netdev);

	if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_TX) {
		return bnx2x_cna_alloc_tx_queue(args->netdev, &args->queueid,
						&args->queue_mapping);

	} else if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX) {
		return bnx2x_cna_alloc_rx_queue(args->netdev, &args->queueid,
						&args->napi);
	} else {
		DP(BNX2X_MSG_NETQ, "invalid queue type: %x\n", args->queueid);
		return VMKNETDDI_QUEUEOPS_ERR;
	}
}

static int bnx2x_cna_free_tx_queue(struct net_device *netdev,
			       vmknetddi_queueops_queueid_t qid)
{
	struct bnx2x *bp = netdev->priv;
	struct bnx2x_fastpath *fp = bnx2x_fcoe_fp(bp);

	if (!BNX2X_IS_NETQ_TX_QUEUE_ALLOCATED(fp)) {
		DP(BNX2X_MSG_NETQ, "CNA NetQ TX Queue is not allocated\n");
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	fp->esx.netq_flags &= ~BNX2X_NETQ_TX_QUEUE_ALLOCATED;

	/* Stop TX */
	netif_tx_disable(netdev);
	netif_carrier_off(netdev);

	DP(BNX2X_MSG_NETQ, "Free CNA NetQ TX Queue\n");

	return VMKNETDDI_QUEUEOPS_OK;
}

static int bnx2x_cna_free_rx_queue(struct net_device *netdev,
				   vmknetddi_queueops_queueid_t qid)
{
	struct bnx2x *bp = netdev->priv;
	u16 index = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(qid);
	struct bnx2x_fastpath *fp = bnx2x_fcoe_fp(bp);

	if (!BNX2X_IS_NETQ_RX_QUEUE_ALLOCATED(fp)) {
		DP(BNX2X_MSG_NETQ, "CNA NetQ RX Queue is not allocated\n");
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	fp->esx.netq_flags &= ~BNX2X_NETQ_RX_QUEUE_ALLOCATED;

	DP(BNX2X_MSG_NETQ, "Free CNA NetQ RX Queue: %x\n", index);

	return VMKNETDDI_QUEUEOPS_OK;
}

static int bnx2x_cna_free_queue(vmknetddi_queueop_free_queue_args_t *args)
{
	if (VMKNETDDI_QUEUEOPS_IS_TX_QUEUEID(args->queueid))
		return bnx2x_cna_free_tx_queue(args->netdev, args->queueid);

	else if (VMKNETDDI_QUEUEOPS_IS_RX_QUEUEID(args->queueid))
		return bnx2x_cna_free_rx_queue(args->netdev, args->queueid);

	else {
		struct net_device *netdev = args->netdev;
		struct bnx2x *bp = netdev_priv(netdev);

		DP(BNX2X_MSG_NETQ, "invalid queue type: %x\n", args->queueid);
		return VMKNETDDI_QUEUEOPS_ERR;
	}
}

static int bnx2x_cna_get_queue_vector(vmknetddi_queueop_get_queue_vector_args_t *args)
{
	struct net_device *netdev = args->netdev;
	struct bnx2x *bp = netdev->priv;
	int qid;

	qid = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args->queueid);

	/* may be invoked also for the default queue */
	if (qid > max_t(int, BNX2X_NUM_RX_NETQUEUES(bp),
			BNX2X_NUM_TX_NETQUEUES(bp))) {
		DP(BNX2X_MSG_NETQ, "NetQ Queue %d is invalid\n", qid);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	/* msix_table indices:
	 * 0 - default SB (slow-path operations)
	 * 1 - CNIC fast-path operations (if compiled in)
	 * 2 - Max NetQs - Net-queues starting form the default queue
	 */
	qid += (1 + CNIC_SUPPORT(bp));

	args->vector = bp->msix_table[qid].vector;
	return VMKNETDDI_QUEUEOPS_OK;
}

static int bnx2x_cna_get_default_queue(vmknetddi_queueop_get_default_queue_args_t *args)
{
	struct bnx2x *bp = args->netdev->priv;

	if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX) {
		args->queueid = VMKNETDDI_QUEUEOPS_MK_RX_QUEUEID(0);
		args->napi = &bp->fp[0].napi;
		return VMKNETDDI_QUEUEOPS_OK;

	} else if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_TX) {
		args->queueid = VMKNETDDI_QUEUEOPS_MK_TX_QUEUEID(0);
		args->queue_mapping = 0;
		return VMKNETDDI_QUEUEOPS_OK;

	} else
		return VMKNETDDI_QUEUEOPS_ERR;
}

static int bnx2x_cna_remove_rx_filter(
	vmknetddi_queueop_remove_rx_filter_args_t *args)
{
	struct bnx2x *bp = args->netdev->priv;
	u16 qid = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args->queueid);
	struct bnx2x_fastpath *fp = bnx2x_fcoe_fp(bp);
	struct bnx2x_queue_state_params qstate = {0};
	unsigned long sp_bits = 0;

	if (!VMKNETDDI_QUEUEOPS_IS_RX_QUEUEID(args->queueid)) {
		BNX2X_ERR("Queue ID %d is not RX queue\n",
			  args->queueid);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	/* Verfiy the queue is allocated and has an active filter */
	if ((!BNX2X_IS_NETQ_RX_QUEUE_ACTIVE(fp) ||
	     !BNX2X_IS_NETQ_RX_QUEUE_ALLOCATED(fp))) {
		DP(BNX2X_MSG_NETQ,
		   "NetQ RX Queue %d is not allocated/active 0x%x\n",
		   qid, fp->esx.netq_flags);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	/* Stop receiving */
	netif_addr_lock_bh(bp->dev);
	bnx2x_set_fcoe_eth_rx_mode(bp, false);
	netif_addr_unlock_bh(bp->dev);

	/* bits to wait on */
	set_bit(BNX2X_FILTER_RX_MODE_PENDING, &sp_bits);
	set_bit(BNX2X_FILTER_FCOE_ETH_STOP_SCHED, &sp_bits);

	if (!bnx2x_wait_sp_comp(bp, sp_bits)) {
		BNX2X_ERR("rx_mode completion timed out!\n");
		return VMKNETDDI_QUEUEOPS_ERR;
	} else {
		/* send empty-ramrod to flush packets lurking in the HW */
		qstate.q_obj = &bnx2x_sp_obj(bp, fp).q_obj;
		qstate.cmd = BNX2X_Q_CMD_EMPTY;
		/* wait for completion */
		set_bit(RAMROD_COMP_WAIT, &qstate.ramrod_flags);
		if (bnx2x_queue_state_change(bp, &qstate))
			return VMKNETDDI_QUEUEOPS_ERR;
	}

	DP(BNX2X_MSG_NETQ, "Free CNA NetQ RX Queue: %x\n", qid);
	return VMKNETDDI_QUEUEOPS_OK;
}

static int bnx2x_cna_apply_rx_filter(
	vmknetddi_queueop_apply_rx_filter_args_t *args)
{
	struct bnx2x *bp = args->netdev->priv;
	struct bnx2x_fastpath *fp;
	u16 qid = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args->queueid);
	unsigned long sp_bits = 0;

	fp = bnx2x_fcoe_fp(bp);

	fp->esx.netq_flags |= BNX2X_NETQ_RX_QUEUE_ACTIVE;
	args->filterid = VMKNETDDI_QUEUEOPS_MK_FILTERID(0);

	/* Need by feature: VMKNETDDI_QUEUEOPS_FEATURE_PAIRQUEUE  */
	args->pairtxqid = qid;

	/* Start receiving */
	netif_addr_lock_bh(bp->dev);
	bnx2x_set_fcoe_eth_rx_mode(bp, true);
	netif_addr_unlock_bh(bp->dev);

	/* bits to wait on */
	set_bit(BNX2X_FILTER_RX_MODE_PENDING, &sp_bits);
	set_bit(BNX2X_FILTER_FCOE_ETH_START_SCHED, &sp_bits);

	if (!bnx2x_wait_sp_comp(bp, sp_bits))
		BNX2X_ERR("rx_mode completion timed out!\n");

	DP(BNX2X_MSG_NETQ, "NetQ set CNA RX filter\n");

	return VMKNETDDI_QUEUEOPS_OK;
}

int bnx2x_cna_netqueue_ops(vmknetddi_queueops_op_t op, void *args)
{
	vmknetddi_queueop_get_queue_count_args_t *p;
	struct bnx2x *bp;
	int rc = VMKNETDDI_QUEUEOPS_ERR;

	if (op == VMKNETDDI_QUEUEOPS_OP_GET_VERSION)
		return bnx2x_get_netqueue_version(
			(vmknetddi_queueop_get_version_args_t *)args);
	else if (op == VMKNETDDI_QUEUEOPS_OP_GET_FEATURES)
		return bnx2x_get_netqueue_features(
			(vmknetddi_queueop_get_features_args_t *)args);

	p = (vmknetddi_queueop_get_queue_count_args_t *)args;

	bp = p->netdev->priv;

	spin_lock(&bp->esx.netq_lock);

	switch (op) {
	case VMKNETDDI_QUEUEOPS_OP_GET_QUEUE_COUNT:
		rc = bnx2x_cna_get_queue_count(
			(vmknetddi_queueop_get_queue_count_args_t *)args);
		bp->esx.cna_netq_op_stats.get_queue_count++;
		break;

	case VMKNETDDI_QUEUEOPS_OP_GET_FILTER_COUNT:
		rc = bnx2x_cna_get_filter_count(
			(vmknetddi_queueop_get_filter_count_args_t *)args);
		bp->esx.cna_netq_op_stats.get_filter_count++;
		break;

	case VMKNETDDI_QUEUEOPS_OP_ALLOC_QUEUE:
		rc = bnx2x_cna_alloc_queue(
			(vmknetddi_queueop_alloc_queue_args_t *)args);
		bp->esx.cna_netq_op_stats.alloc_queue++;
		break;

	case VMKNETDDI_QUEUEOPS_OP_FREE_QUEUE:
		rc = bnx2x_cna_free_queue(
			(vmknetddi_queueop_free_queue_args_t *)args);
		bp->esx.cna_netq_op_stats.free_queue++;
		break;

	case VMKNETDDI_QUEUEOPS_OP_GET_QUEUE_VECTOR:
		rc = bnx2x_cna_get_queue_vector(
			(vmknetddi_queueop_get_queue_vector_args_t *)args);
		bp->esx.cna_netq_op_stats.get_queue_vector++;
		break;

	case VMKNETDDI_QUEUEOPS_OP_GET_DEFAULT_QUEUE:
		rc = bnx2x_cna_get_default_queue(
			(vmknetddi_queueop_get_default_queue_args_t *)args);
		bp->esx.cna_netq_op_stats.get_default_queue++;
		break;

	case VMKNETDDI_QUEUEOPS_OP_APPLY_RX_FILTER:
		rc = bnx2x_cna_apply_rx_filter(
			(vmknetddi_queueop_apply_rx_filter_args_t *)args);
		bp->esx.cna_netq_op_stats.apply_rx_filter++;
		break;

	case VMKNETDDI_QUEUEOPS_OP_REMOVE_RX_FILTER:
		rc = bnx2x_cna_remove_rx_filter(
			(vmknetddi_queueop_remove_rx_filter_args_t *)args);
		bp->esx.cna_netq_op_stats.remove_rx_filter++;
		break;

	case VMKNETDDI_QUEUEOPS_OP_GET_STATS:
		rc = bnx2x_get_queue_stats(
			(vmknetddi_queueop_get_stats_args_t *)args);
		bp->esx.cna_netq_op_stats.get_stats++;
		break;

#if (VMWARE_ESX_DDK_VERSION >= 41000)
	case VMKNETDDI_QUEUEOPS_OP_ALLOC_QUEUE_WITH_ATTR:
		rc = VMKNETDDI_QUEUEOPS_ERR;
		bp->esx.cna_netq_op_stats.alloc_queue_with_attr++;
		break;

	case VMKNETDDI_QUEUEOPS_OP_GET_SUPPORTED_FEAT:
		rc = bnx2x_get_supported_feature(
			(vmknetddi_queueop_get_sup_feat_args_t *)args);
		bp->esx.cna_netq_op_stats.get_supported_feat++;
		break;
#endif

	default:
		BNX2X_ERR("Unhandled NETQUEUE OP 0x%x\n", op);
		rc = VMKNETDDI_QUEUEOPS_ERR;
		bp->esx.cna_netq_op_stats.unhandled++;
	}

	spin_unlock(&bp->esx.netq_lock);

	return rc;
}

static int bnx2x_cna_set_vlan_stripping(struct bnx2x *bp, bool set)
{
	struct bnx2x_queue_state_params q_params = {0};
	struct bnx2x_queue_update_params *update_params =
		&q_params.params.update;
	int rc;
	struct bnx2x_fastpath *fp = bnx2x_fcoe_fp(bp);

	/* We want to wait for completion in this context */
	set_bit(RAMROD_COMP_WAIT, &q_params.ramrod_flags);

	/* Set the command */
	q_params.cmd = BNX2X_Q_CMD_UPDATE;

	/* Enable VLAN stripping if requested */
	if (set)
		set_bit(BNX2X_Q_UPDATE_IN_VLAN_REM,
			&update_params->update_flags);

	/* Indicate that VLAN stripping configuration has changed */
	set_bit(BNX2X_Q_UPDATE_IN_VLAN_REM_CHNG, &update_params->update_flags);

	/* Set the appropriate Queue object */
	q_params.q_obj = &bnx2x_sp_obj(bp, fp).q_obj;

	/* Update the Queue state */
	rc = bnx2x_queue_state_change(bp, &q_params);
	if (rc) {
		BNX2X_ERR("Failed to configure VLAN stripping "
			  "for CNA FCoE ring\n");
		return rc;
	}

	return 0;
}

/* CNA related */
static int bnx2x_cna_open(struct net_device *cnadev)
{
	struct bnx2x *bp = cnadev->priv;

	if (!CNIC_LOADED(bp)) {
		int rc;

		rc = bnx2x_load_cnic(bp);
		if (rc) {
			BNX2X_ERR("CNIC load failed: %d\n", rc);
			return rc;
		}
	}

	strcpy(cnadev->name, bp->dev->name);

	bnx2x_cna_set_vlan_stripping(bp, true);

	netif_set_poll_cna(&bnx2x_fcoe(bp, napi));

	vmknetddi_queueops_invalidate_state(cnadev);

	if (BNX2X_IS_NETQ_TX_QUEUE_ALLOCATED(bnx2x_fcoe_fp(bp)))
		netif_tx_start_all_queues(cnadev);

	DP(NETIF_MSG_PROBE, "CNA pseudo device opened %s\n", cnadev->name);
	return 0;
}

static int bnx2x_cna_close(struct net_device *cnadev)
{
	struct bnx2x *bp = cnadev->priv;

	netif_tx_disable(cnadev);
	netif_carrier_off(cnadev);

	DP(NETIF_MSG_PROBE, "CNA pseudo device closed %s\n", cnadev->name);
	return 0;
}

static int bnx2x_cna_change_mtu(struct net_device *cnadev, int new_mtu)
{
	struct bnx2x *bp = cnadev->priv;

	if (cnadev->mtu == new_mtu)
		return 0;

	if ((new_mtu > ETH_MAX_JUMBO_PACKET_SIZE) ||
		((new_mtu + ETH_HLEN) < ETH_MIN_PACKET_SIZE))
		return -EINVAL;



	DP(BNX2X_MSG_SP,  "changing MTU from %d to %d\n",
		cnadev->mtu, new_mtu);

	/* must set new MTU before calling down or up */
	cnadev->mtu = new_mtu;

	return dev_close(cnadev) || dev_open(cnadev);
}

/* called with rtnl_lock */
static void bnx2x_cna_vlan_rx_register(struct net_device *dev,
				       struct vlan_group *vlgrp)
{
	struct bnx2x *bp = dev->priv;
	int rc;

	/* Configure VLAN stripping if NIC is up.
	 * Otherwise just set the bp->vlgrp and stripping will be
	 * configured in bnx2x_nic_load().
	 */
	if (bp->state == BNX2X_STATE_OPEN) {
		if (vlgrp != NULL) {
			rc = bnx2x_cna_set_vlan_stripping(bp, true);

			/* If we failed to configure VLAN stripping we don't
			 * want to use HW accelerated flow in bnx2x_rx_int().
			 * Thus we will leave bp->vlgrp to be equal to NULL to
			 * disable it.
			 */
			if (rc) {
				netdev_err(dev, "Failed to set HW "
						"VLAN stripping for a CNA "
						"device\n");
				bnx2x_cna_set_vlan_stripping(bp, false);
			} else
				bp->cna_vlgrp = vlgrp;
		} else {
			rc = bnx2x_cna_set_vlan_stripping(bp, false);

			if (rc)
				netdev_err(dev, "Failed to clear HW "
						"VLAN strippingfor a CNA "
						"device\n");

			bp->cna_vlgrp = NULL;
		}
	} else
		bp->cna_vlgrp = vlgrp;
}

static void bnx2x_cna_vlan_rx_add_vid(struct net_device *dev, uint16_t vid)
{
	struct bnx2x *bp = dev->priv;

	if (bp->cna_vlgrp)
		vlan_group_set_device(bp->cna_vlgrp, vid, dev);
}

static void bnx2x_cna_vlan_rx_kill_vid(struct net_device *dev, uint16_t vid)
{
	struct bnx2x *bp = dev->priv;

	if (bp->cna_vlgrp)
		vlan_group_set_device(bp->cna_vlgrp, vid, NULL);
}

/* TODO: think of a better name */
int bnx2x_cna_enable(struct bnx2x *bp, int tx_count, int rx_count)
{
	struct net_device *cnadev;
	struct net_device *netdev;
	int err;

	//u16 device_caps;
	if (NO_FCOE(bp))
		return -EINVAL;

	bp->flags |= CNA_ENABLED;

	netdev = bp->dev;

	/* Oppositely to regular net device, CNA device doesn't have
	 * a private allocated region as we don't want to duplicate
	 * bnx2x information. Though, the CNA device still need
	 * to access the bnx2x if FP. Thereby, cnadev->priv needs to
	 * point to netdev->priv.
	 */
	cnadev = alloc_etherdev_mqs(0, tx_count, rx_count);
	if (!cnadev) {
		err = -ENOMEM;
		goto err_alloc_etherdev;
	}
	bp->cnadev = cnadev;

	cnadev->priv = bp;

	cnadev->open             = &bnx2x_cna_open;
	cnadev->stop             = &bnx2x_cna_close;
	cnadev->change_mtu       = &bnx2x_cna_change_mtu;
	cnadev->do_ioctl         = netdev->do_ioctl;
	cnadev->hard_start_xmit  = netdev->hard_start_xmit;
#ifdef NETIF_F_HW_VLAN_TX
	cnadev->vlan_rx_register = bnx2x_cna_vlan_rx_register;
	cnadev->vlan_rx_add_vid  = bnx2x_cna_vlan_rx_add_vid;
	cnadev->vlan_rx_kill_vid = bnx2x_cna_vlan_rx_kill_vid;
#endif
	bnx2x_set_ethtool_ops(bp, cnadev);

#ifdef CONFIG_DCB
	cnadev->dcbnl_ops = netdev->dcbnl_ops;
#endif

	cnadev->mtu = netdev->mtu;
	cnadev->pdev = netdev->pdev;
	cnadev->gso_max_size = GSO_MAX_SIZE;
	cnadev->features = netdev->features | NETIF_F_HWDCB | NETIF_F_CNA |
			   NETIF_F_HW_VLAN_FILTER;

	/* set the MAC address to SAN mac address */
	memcpy(cnadev->dev_addr, bp->fip_mac, ETH_ALEN);

	netif_carrier_off(cnadev);
	netif_tx_stop_all_queues(cnadev);

	VMKNETDDI_REGISTER_QUEUEOPS(cnadev, bnx2x_cna_netqueue_ops);

	netif_napi_add(bp->cnadev, &bnx2x_fp(bp, FCOE_IDX(bp), napi),
		       bnx2x_poll, NAPI_POLL_WEIGHT);

	err = register_netdev(cnadev);
	if (err)
		goto err_register;

	DP(NETIF_MSG_PROBE, "CNA pseudo device registered %s\n", netdev->name);

	return err;

err_register:
	netif_napi_del(&bnx2x_fp(bp, FCOE_IDX(bp), napi));
	DP(NETIF_MSG_PROBE, "CNA pseudo device cannot be registered %s\n",
		netdev->name);
	free_netdev(cnadev);
err_alloc_etherdev:
	DP(NETIF_MSG_PROBE, "CNA cannot be enabled on %s\n", netdev->name);
	bp->flags &= ~CNA_ENABLED;
	return err;
}

void bnx2x_cna_disable(struct bnx2x *bp, bool remove_netdev)
{
	struct net_device *cnadev = bp->cnadev;

	if (!remove_netdev)
		return;

	if (bp->flags & CNA_ENABLED) {
		bp->flags &= ~CNA_ENABLED;
		netif_napi_del(&bnx2x_fp(bp, FCOE_IDX(bp), napi));
		unregister_netdev(cnadev);
		DP(NETIF_MSG_PROBE, "CNA pseudo device unregistered %s\n",
			cnadev->name);

		free_netdev(cnadev);
		bp->cnadev = NULL;
	}
}
#endif
/*******************/

int bnx2x_filters_validate_mac(struct bnx2x *bp,
			       struct bnx2x_virtf *vf,
			       struct vfpf_set_q_filters_tlv *filters)
{
	return 0;
}

void pci_wake_from_d3(struct pci_dev *dev, bool enable)
{
	u16 pmcsr;
	int pmcap;

	pmcap = pci_find_capability(dev, PCI_CAP_ID_PM);
	if (!pmcap)
		return;

	pci_read_config_word(dev, pmcap + PCI_PM_CTRL, &pmcsr);
	pmcsr |= PCI_PM_CTRL_PME_STATUS | PCI_PM_CTRL_PME_ENABLE;
	if (!enable)
		pmcsr &= PCI_PM_CTRL_PME_ENABLE;
	pci_write_config_word(dev, pmcap + PCI_PM_CTRL, pmcsr);
}

#ifdef BNX2X_ESX_SRIOV
/* Must be called only after VF-Enable*/
inline int
bnx2x_vmk_vf_bus(struct bnx2x *bp, int vfid)
{
	vmk_uint32 sbdf;
	struct pci_dev *vf;

	vf = vmklnx_get_vf(bp->pdev, vfid, &sbdf);
	if (vf == NULL)
		return -EINVAL;

	return vf->bus->number;
}

/* Must be called only after VF-Enable*/
inline int
bnx2x_vmk_vf_devfn(struct bnx2x *bp, int vfid)
{
	vmk_uint32 sbdf;
	struct pci_dev *vf;

	vf = vmklnx_get_vf(bp->pdev, vfid, &sbdf);
	if (vf == NULL)
		return -EINVAL;

	return vf->devfn;
}

static int bnx2x_vmk_pci_cfg_space_size_ext(struct pci_dev *dev)
{
	u32 status;
	int pos = PCI_CFG_SPACE_SIZE;

	if (pci_read_config_dword(dev, pos, &status) != PCIBIOS_SUCCESSFUL)
		goto fail;
	if (status == 0xffffffff)
		goto fail;

	return PCI_CFG_SPACE_EXP_SIZE;

 fail:
	return PCI_CFG_SPACE_SIZE;
}

static int bnx2x_vmk_pci_cfg_space_size(struct pci_dev *dev)
{
	int pos;
	u32 status;
	u16 class;

	class = dev->class >> 8;
	if (class == PCI_CLASS_BRIDGE_HOST)
		return bnx2x_vmk_pci_cfg_space_size_ext(dev);

	pos = pci_pcie_cap(dev);
	if (!pos) {
		pos = pci_find_capability(dev, PCI_CAP_ID_PCIX);
		if (!pos)
			goto fail;

		pci_read_config_dword(dev, pos + PCI_X_STATUS, &status);
		if (!(status & (PCI_X_STATUS_266MHZ | PCI_X_STATUS_533MHZ)))
			goto fail;
	}

	return bnx2x_vmk_pci_cfg_space_size_ext(dev);

 fail:
	return PCI_CFG_SPACE_SIZE;
}

int bnx2x_vmk_pci_find_ext_capability(struct pci_dev *dev, int cap)
{
	u32 header;
	int ttl;
	int pos = PCI_CFG_SPACE_SIZE;
	int cfg_size;

	/* minimum 8 bytes per capability */
	ttl = (PCI_CFG_SPACE_EXP_SIZE - PCI_CFG_SPACE_SIZE) / 8;

	cfg_size = bnx2x_vmk_pci_cfg_space_size(dev);
	if (cfg_size <= PCI_CFG_SPACE_SIZE) {
		printk(KERN_ERR "dev->cfg_size: %d <= PCI_CFG_SPACE_SIZE: %d\n", cfg_size, PCI_CFG_SPACE_SIZE);
		return 0;
	}

	if (pci_read_config_dword(dev, pos, &header) != PCIBIOS_SUCCESSFUL) {
		printk(KERN_ERR "Could not read first header\n");
		return 0;
	}

	/*
	 * If we have no capabilities, this is indicated by cap ID,
	 * cap version and next pointer all being 0.
	 */
	if (header == 0)
		return 0;
	while (ttl-- > 0) {
		if (PCI_EXT_CAP_ID(header) == cap)
			return pos;

		pos = PCI_EXT_CAP_NEXT(header);
		if (pos < PCI_CFG_SPACE_SIZE)
			break;

		if (pci_read_config_dword(dev, pos, &header) != PCIBIOS_SUCCESSFUL)
			break;
	}

	return 0;
}

u8 bnx2x_vmk_vf_is_pcie_pending(struct bnx2x *bp, u8 abs_vfid)
{
	u16 idx =  (u16)bnx2x_vf_idx_by_abs_fid(bp, abs_vfid);
	struct bnx2x_virtf *vf;

	 if (bp->esx.flags & BNX2X_ESX_SRIOV_ENABLED)
		return false;

	vf = ((idx < BNX2X_NR_VIRTFN(bp)) ? BP_VF(bp, idx) : NULL);
	if (!vf) {
		BNX2X_ERR("Unknown device: index %d invalid [abs_vfid: %d]\n",
			  idx, abs_vfid);
		return false;
	} else {
		struct pci_dev *vf_pdev;
		u16 status;
		vmk_uint16 pos;

		vf_pdev = vmklnx_get_vf(bp->pdev, abs_vfid, NULL);
		if (!vf_pdev) {
			BNX2X_ERR("Unknown device: abs_vfid: %d\n", abs_vfid);
			return false;
		}

		pos = bnx2x_pci_find_ext_capability(vf_pdev,
						    PCI_EXT_CAP_ID_SRIOV);
		pci_read_config_word(vf_pdev, pos + PCI_EXP_DEVSTA, &status);

		return (status & PCI_EXP_DEVSTA_TRPND);
	}
}

int __devinit bnx2x_vmk_iov_init_one(struct bnx2x *bp)
{
	if (!bp->vfdb)
		return 0;

	bp->esx.flags |= BNX2X_ESX_SRIOV_ENABLED;
	bp->flags |= NO_FCOE_FLAG;

	return 0;
}

int bnx2x_esx_active_vf_count(struct bnx2x *bp)
{
	int i;
	int active_vfs = 0;

	if (!IS_SRIOV(bp))
		return 0;

	for_each_vf(bp, i) {
		struct bnx2x_virtf *vf = BP_VF(bp, i);

		if (vf->state != VF_FREE)
			active_vfs++;
	}

	return active_vfs;
}

int bnx2x_esx_check_active_vf_count(struct bnx2x *bp, char *text)
{
	int active_vf_count = bnx2x_esx_active_vf_count(bp);
	if (active_vf_count > 0) {
		BNX2X_ERR(
			  "Preventing %s because %d "
			  "VF's are active\n", text, active_vf_count);
	}

	return active_vf_count;
}

static int
bnx2x_pt_vf_set_mac(struct net_device *netdev, vmk_VFID vmkVf,
		    vmk_EthAddress mac)
{
	int rc;
	struct bnx2x *bp = netdev_priv(netdev);
	struct bnx2x_virtf *vf = BP_VF(bp, vmkVf);
	DECLARE_MAC_BUF(mac_buf);

	if (is_zero_ether_addr(mac)) {
		struct bnx2x_vlan_mac_obj *mac_obj = &bnx2x_vfq(vf, 0, mac_obj);

		if (vf == NULL || vf->state != VF_ENABLED)
			return VMK_OK;

		/* must lock vfpf channel to protect against vf flows */
		bnx2x_lock_vf_pf_channel(bp, vf, CHANNEL_TLV_PF_SET_MAC);

		/* remove existing eth macs */
		rc = bnx2x_del_all_macs(bp, mac_obj, BNX2X_ETH_MAC, true);
		if (rc) {
			BNX2X_ERR("failed to delete eth macs\n");
			rc = VMK_FAILURE;
			goto error;
		}

		/* remove existing uc list macs */
		rc = bnx2x_del_all_macs(bp, mac_obj, BNX2X_UC_LIST_MAC, true);
		if (rc) {
			BNX2X_ERR("failed to delete uc_list macs\n");
			rc = VMK_FAILURE;
			goto error;
		}

		rc = VMK_OK;
error:
		bnx2x_unlock_vf_pf_channel(bp, vf, CHANNEL_TLV_PF_SET_MAC);
		return rc;
	}

	rc = bnx2x_set_vf_mac(netdev, vmkVf, mac);

	if (!rc) {
		DP(BNX2X_MSG_IOV, "Using mac:'%s' for VF: %d\n",
		   print_mac(mac_buf, mac), vf->index);
		return VMK_OK;
	} else {
		DP(BNX2X_MSG_IOV, "Failed to set mac:'%s' for VF: %d\n",
		   print_mac(mac_buf, mac), vf->index);

		/*  Silently fail the setting of the MAC */
		return VMK_OK;
	}
}

static int
bnx2x_pt_vf_set_default_vlan(struct net_device *netdev,
			    vmk_NetPTOPVFSetDefaultVlanArgs *args)
{
	struct bnx2x *bp = netdev_priv(netdev);
	int rc;

	DP(BNX2X_MSG_IOV,
	   "Using default VLAN for VF: %d: "
	   "enabled: %d vid:%d priority:%d\n",
	   args->vf, args->enable, args->vid, args->prio);

	rc = bnx2x_set_vf_vlan(netdev, args->vf, args->enable ? args->vid : 0,
			       args->prio);
	if (rc == 0)
		return VMK_OK;
	else
		return VMK_FAILURE;
}

int
bnx2x_esx_save_statistics(struct bnx2x *bp, u8 vfID)
{
	int i;

	if (!IS_SRIOV(bp))
		return 0;

	for (i = 0; i < BNX2X_VF_MAX_QUEUES; i++) {
		struct per_queue_stats *fw_stats = (struct per_queue_stats *)
			   ((u8 *)bp->esx.vf_fw_stats +
			    (vfID * BNX2X_VF_MAX_QUEUES *
			     PAGE_ALIGN(sizeof(struct per_queue_stats))) +
			    (i *
			     PAGE_ALIGN(sizeof(struct per_queue_stats))));
		struct per_queue_stats *old_fw_stats =
			   (struct per_queue_stats *)
			   ((u8 *)bp->esx.old_vf_fw_stats +
			    (vfID * BNX2X_VF_MAX_QUEUES *
			     PAGE_ALIGN(sizeof(struct per_queue_stats))) +
			    (i *
			     PAGE_ALIGN(sizeof(struct per_queue_stats))));

		struct xstorm_per_queue_stats *xstorm =
			&fw_stats->xstorm_queue_statistics;
		struct xstorm_per_queue_stats *old_xstorm =
			&old_fw_stats->xstorm_queue_statistics;

		struct tstorm_per_queue_stats *tstorm =
			&fw_stats->tstorm_queue_statistics;
		struct tstorm_per_queue_stats *old_tstorm =
			&old_fw_stats->tstorm_queue_statistics;

		struct ustorm_per_queue_stats *ustorm =
			&fw_stats->ustorm_queue_statistics;
		struct ustorm_per_queue_stats *old_ustorm =
			&old_fw_stats->ustorm_queue_statistics;

		ADD_64(old_tstorm->rcv_ucast_bytes.hi,
		       le32_to_cpu(tstorm->rcv_ucast_bytes.hi),
		       old_tstorm->rcv_ucast_bytes.lo,
		       le32_to_cpu(tstorm->rcv_ucast_bytes.lo));
		old_tstorm->rcv_ucast_pkts +=
			le32_to_cpu(tstorm->rcv_ucast_pkts);
		old_tstorm->checksum_discard +=
			le32_to_cpu(tstorm->checksum_discard);
		ADD_64(old_tstorm->rcv_bcast_bytes.hi,
		       le32_to_cpu(tstorm->rcv_bcast_bytes.hi),
		       old_tstorm->rcv_bcast_bytes.lo,
		       le32_to_cpu(tstorm->rcv_bcast_bytes.lo));
		old_tstorm->rcv_bcast_pkts +=
			le32_to_cpu(tstorm->rcv_bcast_pkts);
		old_tstorm->pkts_too_big_discard +=
			le32_to_cpu(tstorm->pkts_too_big_discard);
		ADD_64(old_tstorm->rcv_mcast_bytes.hi,
		       le32_to_cpu(tstorm->rcv_mcast_bytes.hi),
		       old_tstorm->rcv_mcast_bytes.lo,
		       le32_to_cpu(tstorm->rcv_mcast_bytes.lo));
		old_tstorm->rcv_mcast_pkts +=
			le32_to_cpu(tstorm->rcv_mcast_pkts);
		old_tstorm->ttl0_discard +=
			le32_to_cpu(tstorm->ttl0_discard);
		old_tstorm->no_buff_discard +=
			le32_to_cpu(tstorm->no_buff_discard);

		ADD_64(old_ustorm->ucast_no_buff_bytes.hi,
		       le32_to_cpu(ustorm->ucast_no_buff_bytes.hi),
		       old_ustorm->ucast_no_buff_bytes.lo,
		       le32_to_cpu(ustorm->ucast_no_buff_bytes.lo));
		ADD_64(old_ustorm->mcast_no_buff_bytes.hi,
		       le32_to_cpu(ustorm->mcast_no_buff_bytes.hi),
		       old_ustorm->mcast_no_buff_bytes.lo,
		       le32_to_cpu(ustorm->mcast_no_buff_bytes.lo));
		ADD_64(old_ustorm->bcast_no_buff_bytes.hi,
		       le32_to_cpu(ustorm->bcast_no_buff_bytes.hi),
		       old_ustorm->bcast_no_buff_bytes.lo,
		       le32_to_cpu(ustorm->bcast_no_buff_bytes.lo));
		old_ustorm->ucast_no_buff_pkts +=
			le32_to_cpu(ustorm->ucast_no_buff_pkts);
		old_ustorm->mcast_no_buff_pkts +=
			le32_to_cpu(ustorm->mcast_no_buff_pkts);
		old_ustorm->bcast_no_buff_pkts +=
			le32_to_cpu(ustorm->bcast_no_buff_pkts);
		old_ustorm->coalesced_pkts +=
			le32_to_cpu(ustorm->coalesced_pkts);
		ADD_64(old_ustorm->coalesced_bytes.hi,
		       le32_to_cpu(ustorm->coalesced_bytes.hi),
		       old_ustorm->coalesced_bytes.lo,
		       le32_to_cpu(ustorm->coalesced_bytes.lo));
		old_ustorm->coalesced_events +=
			le32_to_cpu(ustorm->coalesced_events);
		old_ustorm->coalesced_aborts +=
			le32_to_cpu(ustorm->coalesced_aborts);

		ADD_64(old_xstorm->ucast_bytes_sent.hi,
		       le32_to_cpu(xstorm->ucast_bytes_sent.hi),
		       old_xstorm->ucast_bytes_sent.lo,
		       le32_to_cpu(xstorm->ucast_bytes_sent.lo));
		ADD_64(old_xstorm->mcast_bytes_sent.hi,
		       le32_to_cpu(xstorm->mcast_bytes_sent.hi),
		       old_xstorm->mcast_bytes_sent.lo,
		       le32_to_cpu(xstorm->mcast_bytes_sent.lo));
		ADD_64(old_xstorm->bcast_bytes_sent.hi,
		       le32_to_cpu(xstorm->bcast_bytes_sent.hi),
		       old_xstorm->bcast_bytes_sent.lo,
		       le32_to_cpu(xstorm->bcast_bytes_sent.lo));
		old_xstorm->ucast_pkts_sent +=
			le32_to_cpu(xstorm->ucast_pkts_sent);
		old_xstorm->mcast_pkts_sent +=
			le32_to_cpu(xstorm->mcast_pkts_sent);
		old_xstorm->bcast_pkts_sent +=
			le32_to_cpu(xstorm->bcast_pkts_sent);
		old_xstorm->error_drop_pkts +=
			le32_to_cpu(xstorm->error_drop_pkts);
	}

	return 0;
}

void
bnx2x_esx_vf_close(struct bnx2x *bp, struct bnx2x_virtf *vf)
{
	bnx2x_esx_save_statistics(bp, vf->index);
}

void
bnx2x_esx_vf_release(struct bnx2x *bp, struct bnx2x_virtf *vf)
{
	int i;

	memset(bp->esx.old_vf_fw_stats, 0, bp->esx.vf_fw_stats_size);
	memset(bp->esx.vf_fw_stats, 0, bp->esx.vf_fw_stats_size);

	for (i = 0; i < BP_VFDB(bp)->sriov.nr_virtfn; i++) {
		struct bnx2x_esx_vf *esx_vf = &bp->esx.vf[i];

		esx_vf->old_mtu = BNX2X_ESX_PASSTHRU_MTU_UNINITIALIZED;
	}
}

int
bnx2x_esx_iov_adjust_stats_req_to_pf(struct bnx2x *bp,
				     struct bnx2x_virtf *vf,
				     int vfq_idx,
				     struct stats_query_entry **cur_query_entry,
				     u8 *stats_count)
{
	struct bnx2x_vf_queue *rxq = vfq_get(vf, vfq_idx);
	dma_addr_t q_stats_addr;

	/* collect stats for active queues only */
	if (bnx2x_get_q_logical_state(bp, &rxq->sp_obj) ==
	    BNX2X_Q_LOGICAL_STATE_STOPPED)
		return 0;

	q_stats_addr = bp->esx.vf_fw_stats_mapping +
			(vf->index *  BNX2X_VF_MAX_QUEUES *
			 PAGE_ALIGN(sizeof(struct per_queue_stats))) +
			(vfq_idx *
			 PAGE_ALIGN(sizeof(struct per_queue_stats)));

	/* create stats query entry for this queue */
	(*cur_query_entry)->kind = STATS_TYPE_QUEUE;
	(*cur_query_entry)->index = vfq_stat_id(vf, rxq);
	(*cur_query_entry)->funcID = cpu_to_le16(BP_FUNC(bp));
	(*cur_query_entry)->address.hi = cpu_to_le32(U64_HI(q_stats_addr));
	(*cur_query_entry)->address.lo = cpu_to_le32(U64_LO(q_stats_addr));
	DP(BNX2X_MSG_IOV,
	   "To PF: added address %x %x for vf %d funcID %d queue %d "
	   "q->index %d client %d\n",
	   (*cur_query_entry)->address.hi,
	   (*cur_query_entry)->address.lo,
	   vf->index,
	   (*cur_query_entry)->funcID,
	   vfq_idx, rxq->index, (*cur_query_entry)->index);
	*cur_query_entry = *cur_query_entry + 1;
	*stats_count = *stats_count + 1;

	/* all stats are coalesced to the leading queue */
	if (vf->cfg_flags & VF_CFG_STATS_COALESCE)
		return 0;

	return 0;
}

static void bnx2x_esx_get_tx_vf_stats(struct bnx2x *bp,
				      vmk_VFID vfID,
				      uint8_t numTxQueues,
				      vmk_NetVFTXQueueStats *tqStats)
{
	int i, j;
	struct bnx2x_virtf *vf = BP_VF(bp, vfID);

	for (i = 0; i < vf_txq_count(vf); i++) {
		struct per_queue_stats *fw_stats = (struct per_queue_stats *)
			   ((u8 *)bp->esx.vf_fw_stats +
			    (vfID * BNX2X_VF_MAX_QUEUES *
			     PAGE_ALIGN(sizeof(struct per_queue_stats))) +
			    (i *
			     PAGE_ALIGN(sizeof(struct per_queue_stats))));
		struct xstorm_per_queue_stats *xstorm =
			&fw_stats->xstorm_queue_statistics;

		struct per_queue_stats *old_fw_stats =
			   (struct per_queue_stats *)
			   ((u8 *)bp->esx.old_vf_fw_stats +
			    (vfID * BNX2X_VF_MAX_QUEUES *
			     PAGE_ALIGN(sizeof(struct per_queue_stats))) +
			    (i *
			     PAGE_ALIGN(sizeof(struct per_queue_stats))));
		struct xstorm_per_queue_stats *old_xstorm =
			&old_fw_stats->xstorm_queue_statistics;

		j = (numTxQueues != vf_txq_count(vf)) ? 0 : i;

		tqStats[j].unicastPkts += BNX2X_ESX_GET_STORM_STAT_32(
							xstorm,
							ucast_pkts_sent);
		tqStats[j].unicastBytes += BNX2X_ESX_GET_STORM_STAT_64(
							xstorm,
							ucast_bytes_sent);
		tqStats[j].multicastPkts += BNX2X_ESX_GET_STORM_STAT_32(
							xstorm,
							mcast_pkts_sent);
		tqStats[j].multicastBytes += BNX2X_ESX_GET_STORM_STAT_64(
							xstorm,
							mcast_bytes_sent);
		tqStats[j].broadcastPkts += BNX2X_ESX_GET_STORM_STAT_32(
							xstorm,
							bcast_pkts_sent);
		tqStats[j].broadcastBytes += BNX2X_ESX_GET_STORM_STAT_64(
							xstorm,
							bcast_bytes_sent);
		tqStats[j].discards = 0;
		tqStats[j].TSOPkts = 0;
		tqStats[j].TSOBytes = 0;
	}
}

static void bnx2x_esx_get_rx_vf_stats(struct bnx2x *bp,
				      vmk_VFID vfID,
				      uint8_t numRxQueues,
				      vmk_NetVFRXQueueStats *rqStats)
{
	int i, j;
	struct bnx2x_virtf *vf = BP_VF(bp, vfID);

	for (i = 0; i < vf_rxq_count(vf); i++) {
		struct per_queue_stats *fw_stats = (struct per_queue_stats *)
			   ((u8 *)bp->esx.vf_fw_stats +
			    (vfID * BNX2X_VF_MAX_QUEUES *
			     PAGE_ALIGN(sizeof(struct per_queue_stats))) +
			    (i *
			     PAGE_ALIGN(
				sizeof(struct per_queue_stats))));
		struct tstorm_per_queue_stats *tstorm =
			&fw_stats->tstorm_queue_statistics;
		struct ustorm_per_queue_stats *ustorm =
			&fw_stats->ustorm_queue_statistics;

		struct per_queue_stats *old_fw_stats =
			   (struct per_queue_stats *)
			   ((u8 *)bp->esx.old_vf_fw_stats +
			    (vfID * BNX2X_VF_MAX_QUEUES *
			     PAGE_ALIGN(sizeof(struct per_queue_stats))) +
			    (i *
			     PAGE_ALIGN(sizeof(struct per_queue_stats))));
		struct tstorm_per_queue_stats *old_tstorm =
			&old_fw_stats->tstorm_queue_statistics;
		struct ustorm_per_queue_stats *old_ustorm =
			&old_fw_stats->ustorm_queue_statistics;

		j = (numRxQueues != vf_rxq_count(vf)) ? 0 : i;

		rqStats[j].unicastPkts += BNX2X_ESX_GET_STORM_STAT_32(
							tstorm,
							rcv_ucast_pkts);
		rqStats[j].unicastBytes += BNX2X_ESX_GET_STORM_STAT_64(
							tstorm,
							rcv_ucast_bytes);
		rqStats[j].multicastPkts += BNX2X_ESX_GET_STORM_STAT_32(
							tstorm,
							rcv_mcast_pkts);
		rqStats[j].multicastBytes += BNX2X_ESX_GET_STORM_STAT_64(
							tstorm,
							rcv_mcast_bytes);
		rqStats[j].broadcastPkts += BNX2X_ESX_GET_STORM_STAT_32(
							tstorm,
							rcv_bcast_pkts);
		rqStats[j].broadcastBytes += BNX2X_ESX_GET_STORM_STAT_64(
							tstorm,
							rcv_bcast_bytes);
		rqStats[j].outOfBufferDrops += BNX2X_ESX_GET_STORM_STAT_32(
							tstorm,
							no_buff_discard);
		rqStats[j].errorDrops += BNX2X_ESX_GET_STORM_STAT_32(
							tstorm,
							checksum_discard) +
					 BNX2X_ESX_GET_STORM_STAT_32(
							tstorm,
							pkts_too_big_discard) +
					 BNX2X_ESX_GET_STORM_STAT_32(
							tstorm,
							ttl0_discard);
		rqStats[j].LROPkts += BNX2X_ESX_GET_STORM_STAT_32(
							ustorm,
							coalesced_pkts);
		rqStats[j].LROBytes += BNX2X_ESX_GET_STORM_STAT_64(
							ustorm,
							coalesced_bytes);
	}
}

VMK_ReturnStatus
bnx2x_esx_vf_get_stats(struct net_device *netdev, vmk_VFID vfID,
		       uint8_t numTxQueues, uint8_t numRxQueues,
		       vmk_NetVFTXQueueStats *tqStats,
		       vmk_NetVFRXQueueStats *rqStats)
{
	struct bnx2x *bp = netdev_priv(netdev);
	struct bnx2x_virtf *vf = BP_VF(bp, vfID);

	DP(BNX2X_MSG_IOV,
	   "Returning VF stats VF: %d, TX queues: %d/%d, RX queues %d/%d\n",
	   vfID, numTxQueues, vf_txq_count(vf), numRxQueues, vf_rxq_count(vf));

	if (unlikely(tqStats == NULL)) {
		BNX2X_ERR("tqStats == NULL\n");
		return VMK_FAILURE;
	}

	if (unlikely(rqStats == NULL)) {
		BNX2X_ERR("rqStats == NULL\n");
		return VMK_FAILURE;
	}

	if (unlikely(vfID >= BNX2X_NR_VIRTFN(bp))) {
		BNX2X_ERR("Requesting VF: %d but only configured %d VF's\n",
			  vfID,  BNX2X_NR_VIRTFN(bp));
		return VMK_FAILURE;
	}

	memset(tqStats, 0, numTxQueues * sizeof(vmk_NetVFTXQueueStats));
	memset(rqStats, 0, numRxQueues * sizeof(vmk_NetVFRXQueueStats));

	if (vf->state != VF_ENABLED) {
		DP(BNX2X_MSG_IOV,
		   "vf %d not enabled so no stats for it\n",  vfID);
		return VMK_OK;
	}

	bnx2x_esx_get_tx_vf_stats(bp, vfID, numTxQueues, tqStats);
	bnx2x_esx_get_rx_vf_stats(bp, vfID, numRxQueues, rqStats);

	return 0;
}

static void
bnx2x_esx_wake_up_passthru_config(struct bnx2x *bp, u16 vf_idx)
{
	wake_up(&bp->esx.vf[vf_idx].passthru_wait_comp);
}

void
bnx2x_esx_passthru_config_mtu_finalize(struct bnx2x *bp, u16 vf_idx,
				       int rc, int state, u16 mtu)
{
	struct bnx2x_esx_vf *esx_vf = &bp->esx.vf[vf_idx];

	BNX2X_ESX_SET_PASSTHRU_RC_STATE(bp, vf_idx, rc, state);
	if (esx_vf->old_mtu != BNX2X_ESX_PASSTHRU_MTU_UNINITIALIZED)
		bnx2x_esx_wake_up_passthru_config(bp, vf_idx);
	esx_vf->old_mtu = mtu;
}

void
bnx2x_esx_passthru_config_setup_filters_finalize(struct bnx2x *bp, u16 vf_idx,
						 int rc, int state)
{
	BNX2X_ESX_SET_PASSTHRU_RC_STATE(bp, vf_idx, rc, state);
	if (rc == 0)
		bnx2x_esx_wake_up_passthru_config(bp, vf_idx);
}


static VMK_ReturnStatus
bnx2x_esx_config_wait(struct bnx2x *bp,
		      struct bnx2x_virtf *vf, struct bnx2x_esx_vf *esx_vf,
		      u32 next_state)
{
	int rc;

	esx_vf->passthru_state = next_state;
	wake_up(&esx_vf->passthru_wait_config);

	rc = wait_event_timeout(esx_vf->passthru_wait_comp,
				esx_vf->passthru_state == 0,
				BNX2X_PASSTHRU_WAIT_EVENT_TIMEOUT);
	if (rc == 0) {
		BNX2X_ERR(
			  "Timeout waiting for completion: "
			  "VF: %d passthru_state: 0x%x\n",
			  vf->index, esx_vf->passthru_state);
		rc = VMK_FAILURE;
	} else {
		if (esx_vf->passthru_rc != 0) {
			DP(BNX2X_MSG_IOV,
			   "Failure during PT OP VF: %d rc: %d\n",
			   vf->index, esx_vf->passthru_rc);
			rc = VMK_FAILURE;
		} else {
			DP(BNX2X_MSG_IOV, "Completed PT OP VF: %d\n",
			   vf->index);
			rc = VMK_OK;
		}
	}

	esx_vf->passthru_state = 0;
	return rc;
}

VMK_ReturnStatus
bnx2x_pt_passthru_ops(struct net_device *netdev, vmk_NetPTOP op, void *pargs)
{
	struct bnx2x *bp;

	if (unlikely(netdev == NULL)) {
		BNX2X_ERR("netdev == NULL\n");
		return VMK_FAILURE;
	}

	bp = netdev_priv(netdev);

	if (unlikely(pargs == NULL)) {
		BNX2X_ERR("pargs == NULL\n");
		return VMK_FAILURE;
	}

	if (!bp->esx.vf) {
		BNX2X_ERR("Failing Passthru OP %d."
				" PF may not be up.\n", op);
		return VMK_FAILURE;
	}

	switch (op) {
	case VMK_NETPTOP_VF_SET_MAC:
	{
		vmk_NetPTOPVFSetMacArgs *args = pargs;
		struct bnx2x_virtf *vf = BP_VF(bp, args->vf);
		struct bnx2x_esx_vf *esx_vf = &bp->esx.vf[vf->index];
		DECLARE_MAC_BUF(mac_buf);
		int rc;

		if (esx_vf->passthru_state ==
		    BNX2X_ESX_PASSTHRU_SET_MAC_MSG_FROM_VF &&
		    memcmp(esx_vf->mac_from_config, args->mac, ETH_ALEN) == 0) {
			DP(BNX2X_MSG_IOV,
			   "Calling Passthru OP to change MAC Address via "
			   "VF: %d, MAC: %s\n",
			   args->vf, print_mac(mac_buf, args->mac));

			rc = bnx2x_esx_config_wait(bp, vf, esx_vf,
				BNX2X_ESX_PASSTHRU_SET_MAC_COMPLETE_OP);
		} else {
			if (memcmp(esx_vf->last_mac,
				   args->mac, ETH_ALEN) != 0) {

				esx_vf->passthru_state = 0;
				DP(BNX2X_MSG_IOV,
				   "Calling Passthru OP to change "
				   "MAC Address via "
				   "HV: VF: %d, MAC: %s\n",
				   args->vf, print_mac(mac_buf, args->mac));
				rc = bnx2x_pt_vf_set_mac(netdev,
							 args->vf, args->mac);
			} else {
				DP(BNX2X_MSG_IOV,
				   "Calling Passthru OP to change "
				   "MAC Address via "
				   "HV: VF: %d, MAC: %s "
				   "but unchaged HV assigning same MAC\n",
				   args->vf, print_mac(mac_buf, args->mac));
				rc = 0;
			}
		}

		if (rc == VMK_OK)
			memcpy(esx_vf->last_mac, args->mac, ETH_ALEN);

		return rc;
	}
	case VMK_NETPTOP_VF_SET_DEFAULT_VLAN:
	{
		vmk_NetPTOPVFSetDefaultVlanArgs *args = pargs;

		return bnx2x_pt_vf_set_default_vlan(netdev, args);
	}
	case VMK_NETPTOP_VF_GET_QUEUE_STATS:
	{
		vmk_NetPTOPVFGetQueueStatsArgs *args = pargs;
		return bnx2x_esx_vf_get_stats(netdev, args->vf,
					      args->numTxQueues,
					      args->numRxQueues,
					      args->tqStats,
					      args->rqStats);
	}
	case VMK_NETPTOP_VF_SET_MTU:
	{
		vmk_NetPTOPVFSetMtuArgs *args = pargs;
		struct bnx2x_virtf *vf = BP_VF(bp, args->vf);
		struct bnx2x_esx_vf *esx_vf = &bp->esx.vf[vf->index];
		int rc;

		if (esx_vf->passthru_state ==
		    BNX2X_ESX_PASSTHRU_SET_MTU_MSG_FROM_VF &&
		    esx_vf->mtu_from_config == args->mtu) {
			DP(BNX2X_MSG_IOV,
			   "Calling Passthru OP to change MTU Address via "
			   "VF: %d, MTU: %d\n",
			   args->vf, args->mtu);

			rc = bnx2x_esx_config_wait(bp, vf, esx_vf,
				BNX2X_ESX_PASSTHRU_SET_MTU_COMPLETE_OP);
		} else {
			esx_vf->passthru_state = 0;
			DP(BNX2X_MSG_IOV,
			   "Calling Passthru OP to change MTU via "
			   "HV: VF: %d, MTU: %d\n",
			   args->vf, args->mtu);

			esx_vf->forced_mtu = args->mtu;
			esx_vf->flags |= BNX2X_ESX_PASSTHRU_FORCE_MTU;
			rc = VMK_OK;
		}

		return rc;
	}
	default:
		DP(BNX2X_MSG_IOV, "Unhandled OP 0x%x\n", op);
		return VMK_FAILURE;
	}
}


VMK_ReturnStatus
bnx2x_passthru_config(struct bnx2x *bp, u32 vfIdx, int change, void *data)
{
	vmk_NetVFCfgInfo cfginfo;
	u32 *new_mtu = 0;
	DECLARE_MAC_BUF(mac);

	memset(&cfginfo, 0, sizeof(cfginfo));

	switch (change) {
	case VMK_CFG_MAC_CHANGED:
		DP(BNX2X_MSG_IOV, "MAC Address changed\n");
		if (ETH_ALEN != sizeof(cfginfo.macAddr)) {
			BNX2X_ERR("Invalid MAC address: %s\n",
				  print_mac(mac, cfginfo.macAddr));
			return VMK_FAILURE;
		}
		cfginfo.cfgChanged = VMK_CFG_MAC_CHANGED;
		memcpy(cfginfo.macAddr, ((u8 *)data), ETH_ALEN);
		DP(BNX2X_MSG_IOV,
		   "Guest OS requesting MAC addr %s for VF %d\n",
		   print_mac(mac, cfginfo.macAddr), vfIdx);
		break;
	case VMK_CFG_MTU_CHANGED:
		new_mtu = (u32 *)data;
		cfginfo.cfgChanged = VMK_CFG_MTU_CHANGED;
		DP(BNX2X_MSG_IOV, "Guest OS requesting MTU change to %d\n",
			*new_mtu);
		memcpy(&cfginfo.mtu, new_mtu, sizeof(cfginfo.mtu));
		break;
	default:
		DP(BNX2X_MSG_IOV, "Invalid VF configuration change request.\n");
		return VMK_FAILURE;
	}

	return vmklnx_configure_net_vf(bp->pdev, (void *)&cfginfo, vfIdx);
}

int
bnx2x_esx_set_mac_passthru_config(struct bnx2x *bp, struct bnx2x_virtf *vf,
				  struct bnx2x_vfop_filters *fl)
{
	struct bnx2x_vfop_filter *entry;
	int rc = 0;
	struct bnx2x_esx_vf *esx_vf = &bp->esx.vf[vf->index];
	DECLARE_MAC_BUF(mac);

	list_for_each_entry(entry, &fl->head, link) {
		memcpy(esx_vf->mac_from_config, entry->mac, ETH_ALEN);

		esx_vf->passthru_state = BNX2X_ESX_PASSTHRU_SET_MAC_MSG_FROM_VF;
		bnx2x_passthru_config(bp, vf->index, VMK_CFG_MAC_CHANGED,
				      entry->mac);

		rc = wait_event_timeout(esx_vf->passthru_wait_config,
					esx_vf->passthru_state ==
					BNX2X_ESX_PASSTHRU_SET_MAC_COMPLETE_OP,
					BNX2X_PASSTHRU_WAIT_EVENT_TIMEOUT);

		if (rc == 0) {
			if (memcmp(esx_vf->last_mac,
				   entry->mac, ETH_ALEN) == 0) {
				BNX2X_ERR(
					  "MAC unchanged keeping: "
					  "VF: %d, MAC: %s\n",
					  vf->index, print_mac(mac, entry->mac));
				rc = 0;
			} else {
				DECLARE_MAC_BUF(last_mac);

				BNX2X_ERR(
					  "Timeout waiting for MAC "
					  "validation: "
					  "VF: %d, MAC: %s last MAC: %s\n",
					  vf->index,
					  print_mac(mac, entry->mac),
					  print_mac(last_mac,
						    esx_vf->last_mac));

				rc = -EIO;
				break;
			}
		} else {
			DP(BNX2X_MSG_IOV,
			   "VF: %d MAC %s allowed to configure\n",
			   vf->index, print_mac(mac, entry->mac));
			rc = 0;
		}

	}
	esx_vf->passthru_state = 0;

	return rc;
}

int
bnx2x_esx_set_mtu_passthru_config(struct bnx2x *bp, struct bnx2x_virtf *vf)
{
	int rc = 0;
	struct bnx2x_queue_setup_params *setup_p =
		&vf->op_params.qctor.prep_qsetup;
	u32 mtu = setup_p->gen_params.mtu;
	struct bnx2x_esx_vf *esx_vf = &bp->esx.vf[vf->index];

	if (esx_vf->flags & BNX2X_ESX_PASSTHRU_FORCE_MTU) {
		DP(BNX2X_MSG_IOV, "HV forced VF: %d to MTU %d\n",
		   vf->index, esx_vf->forced_mtu);

		setup_p->gen_params.mtu = esx_vf->forced_mtu;
		esx_vf->flags &= ~BNX2X_ESX_PASSTHRU_FORCE_MTU;
		goto done;
	}

	if (esx_vf->old_mtu == BNX2X_ESX_PASSTHRU_MTU_UNINITIALIZED) {
		DP(BNX2X_MSG_IOV, "VF: %d: Initial MTU value set to %d\n",
		   vf->index, setup_p->gen_params.mtu);
		goto done;
	}

	esx_vf->mtu_from_config = mtu;
	esx_vf->passthru_state = BNX2X_ESX_PASSTHRU_SET_MTU_MSG_FROM_VF;
	bnx2x_passthru_config(bp, vf->index, VMK_CFG_MTU_CHANGED, &mtu);

	rc = wait_event_timeout(esx_vf->passthru_wait_config,
				esx_vf->passthru_state ==
				BNX2X_ESX_PASSTHRU_SET_MTU_COMPLETE_OP,
				BNX2X_PASSTHRU_WAIT_EVENT_TIMEOUT);

	if (rc == 0) {
		u16 fallback_mtu = esx_vf->old_mtu ==
					BNX2X_ESX_PASSTHRU_MTU_UNINITIALIZED ?
					ETH_MAX_PACKET_SIZE : esx_vf->old_mtu;

		if (mtu == fallback_mtu)
			DP(BNX2X_MSG_IOV,
			   "MTU didn't change VF: %d keeping MTU: %d\n",
			   vf->index, fallback_mtu);
		else
			BNX2X_ERR(
				  "Timeout waiting for MTU validation: "
				  "VF: %d, passthru_state: 0x%x "
				  "tried to set MTU: %d falling back to MTU: %d\n",
				  vf->index, esx_vf->passthru_state,
				  mtu, fallback_mtu);
		setup_p->gen_params.mtu = fallback_mtu;
	} else {
		DP(BNX2X_MSG_IOV,
		   "VF: %d old MTU: %d to MTU %d allowed to configure\n",
		   vf->index, esx_vf->old_mtu, mtu);
	}

	rc = 0;
done:
	esx_vf->passthru_state = 0;

	return rc;
}

#else
int bnx2x_esx_active_vf_count(struct bnx2x *bp)
{
	return 0;
}

int bnx2x_vmk_pci_find_ext_capability(struct pci_dev *dev, int cap)
{
	return 0;
}

int __devinit bnx2x_vmk_iov_init_one(struct bnx2x *bp)
{
	return 0;
}

int bnx2x_esx_check_active_vf_count(struct bnx2x *bp, char *text)
{
	return 0;
}

int
bnx2x_esx_iov_adjust_stats_req_to_pf(struct bnx2x *bp,
				     struct bnx2x_virtf *vf,
				     int vfq_idx,
				     struct stats_query_entry **cur_query_entry,
				     u8 *stats_count)
{
	return 0;
}

void
bnx2x_esx_vf_close(struct bnx2x *bp, struct bnx2x_virtf *vf)
{
}

void
bnx2x_esx_vf_release(struct bnx2x *bp, struct bnx2x_virtf *vf)
{
}

void
bnx2x_esx_passthru_config_mtu_finalize(struct bnx2x *bp, u16 vf_idx,
				       int rc, int state, u16 mtu)
{
}

void
bnx2x_esx_passthru_config_setup_filters_finalize(struct bnx2x *bp, u16 vf_idx,
						 int rc, int state)
{
}

int
bnx2x_esx_set_mac_passthru_config(struct bnx2x *bp, struct bnx2x_virtf *vf,
				  struct bnx2x_vfop_filters *fl)
{
	return 0;
}

int
bnx2x_esx_set_mtu_passthru_config(struct bnx2x *bp, struct bnx2x_virtf *vf)
{
	return 0;
}
#endif

int bnx2x_vmk_open_epilog(struct bnx2x *bp)
{
	int rc;

	if (IS_SRIOV(bp)) {
		rc = bnx2x_sriov_configure(bp->pdev,
					   (BP_VFDB(bp)->sriov.nr_virtfn));
		if (rc != (BP_VFDB(bp)->sriov.nr_virtfn))
			BNX2X_ERR("Failed to configure VFs. [0x%x]\n", rc);
	}

#if defined(BNX2X_ESX_CNA)
	if (bp->flags & CNA_ENABLED)
		return dev_open(bp->cnadev);
#endif
	return 0;
}

#if (VMWARE_ESX_DDK_VERSION >= 55000)
#define MISC_REG_AEU_MASK_ATTN_MCP 0xa068
#define NIG_REG_BMAC1_PAUSE_OUT_EN 0x10114
#define NIG_REG_EMAC1_PAUSE_OUT_EN 0x1011c

static struct dmp_config dmpcfg;
vmklnx_DumpFileHandle bnx2x_fwdmp_dh;
void *bnx2x_fwdmp_va;
struct bnx2x_fwdmp_info bnx2x_fwdmp_bp[BNX2X_MAX_NIC+1];

static void esx_reg_wr_ind(struct bnx2x *bp, u32 addr, u32 val)
{
	pci_write_config_dword(bp->pdev, PCICFG_GRC_ADDRESS, addr);
	pci_write_config_dword(bp->pdev, PCICFG_GRC_DATA, val);
	pci_write_config_dword(bp->pdev, PCICFG_GRC_ADDRESS,
			       PCICFG_VENDOR_ID_OFFSET);
}

static u32 esx_reg_rd_ind(struct bnx2x *bp, u32 addr)
{
	u32 val;

	pci_write_config_dword(bp->pdev, PCICFG_GRC_ADDRESS, addr);
	pci_read_config_dword(bp->pdev, PCICFG_GRC_DATA, &val);
	pci_write_config_dword(bp->pdev, PCICFG_GRC_ADDRESS,
			       PCICFG_VENDOR_ID_OFFSET);

	return val;
}

static u32 *
read_idle_chk(struct bnx2x *bp, u32 *dst, struct chip_core_dmp *dmp)
{
	u32 i, j;

	/* Read the idle chk registers */
	for (i = 0; i < IDLE_REGS_COUNT; i++)
		if (dmpcfg.mode == (idle_addrs[i].info & dmpcfg.mode))
			for (j = 0; j < idle_addrs[i].size; j++) {
				if ((dmp->fw_hdr.dmp_size + 4) >
				    DRV_DUMP_CRASH_DMP_BUF_SIZE_E3B0) {
					SET_FLAGS(dmp->fw_hdr.flags,
						  FWDMP_FLAGS_SPACE_NEEDED);
					break;
				}
				*dst++ = RD_IND(bp,
						idle_addrs[i].addr + j*4);
				dmp->fw_hdr.dmp_size += 4;
			}
	return dst;
}

static u32 *
read_mcp_traces(struct bnx2x *bp, u32 *dst, struct chip_core_dmp *dmp)
{
	u32 trace_shmem_base;
	u32 addr;
	u32 i;

	if (dmp->fw_hdr.dmp_size + DBG_DMP_TRACE_BUFFER_SIZE >
		DRV_DUMP_CRASH_DMP_BUF_SIZE_E3B0) {
		SET_FLAGS(dmp->fw_hdr.flags, FWDMP_FLAGS_SPACE_NEEDED);
		return dst;
	}

	if (BP_PATH(bp) == 0)
		trace_shmem_base = bp->common.shmem_base;
	else
		trace_shmem_base = SHMEM2_RD(bp, other_shmem_base_addr);

	addr = trace_shmem_base - DBG_DMP_TRACE_BUFFER_SIZE;
	for (i = 0; i < DBG_DMP_TRACE_BUFFER_SIZE;) {
		if ((dmp->fw_hdr.dmp_size + 4)
			> DRV_DUMP_CRASH_DMP_BUF_SIZE_E3B0) {
			SET_FLAGS(dmp->fw_hdr.flags, FWDMP_FLAGS_SPACE_NEEDED);
			break;
		}
		*dst++ = RD_IND(bp, addr + i);
		i += 4;
		dmp->fw_hdr.dmp_size += 4;
	}
	return dst;
}

static u32 *
read_regular_regs(struct bnx2x *bp, u32 *dst, struct chip_core_dmp *dmp)
{
	u32 i, j;

	/* Read the regular address registers */
	for (i = 0; i < REGS_COUNT; i++)
		if (dmpcfg.mode == (reg_addrs[i].info & dmpcfg.mode))
			for (j = 0; j < reg_addrs[i].size; j++) {
				if ((dmp->fw_hdr.dmp_size + 4) >
				    DRV_DUMP_CRASH_DMP_BUF_SIZE_E3B0) {
					SET_FLAGS(dmp->fw_hdr.flags,
						  FWDMP_FLAGS_SPACE_NEEDED);
					break;
				}
				*dst++ = RD_IND(bp, reg_addrs[i].addr + j*4);
				dmp->fw_hdr.dmp_size += 4;
			}
	return dst;
}

static u32 *
read_wregs(struct bnx2x *bp, u32 *dst, struct chip_core_dmp *dmp)
{
	u32 i, j, k;
	u32 reg_add_read = 0;
	const struct wreg_addr *pwreg_addrs = dmpcfg.pwreg_addrs;

	for (i = 0; i < dmpcfg.wregs_count; i++) {
		if (dmpcfg.mode == (pwreg_addrs[i].info & dmpcfg.mode)) {
			reg_add_read = pwreg_addrs[i].addr;
			for (j = 0; j < pwreg_addrs[i].size; j++) {
				if ((dmp->fw_hdr.dmp_size + 4) >
				    DRV_DUMP_CRASH_DMP_BUF_SIZE_E3B0) {
					SET_FLAGS(dmp->fw_hdr.flags,
						  FWDMP_FLAGS_SPACE_NEEDED);
					break;
				}
				*dst++ = RD_IND(bp, reg_add_read);
				reg_add_read += 4;
				dmp->fw_hdr.dmp_size += 4;
				for (k = 0;
				     k < pwreg_addrs[i].read_regs_count; k++) {
					if ((dmp->fw_hdr.dmp_size + 4) >
					    DRV_DUMP_CRASH_DMP_BUF_SIZE_E3B0) {
						SET_FLAGS(dmp->fw_hdr.flags,
						      FWDMP_FLAGS_SPACE_NEEDED);
						break;
					}
					*dst++ = RD_IND(bp,
						   pwreg_addrs[i].read_regs[k]);
					dmp->fw_hdr.dmp_size += 4;
				}
			}
		}
	}
	return dst;
}

static u32 *
read_page_mode(struct bnx2x *bp, u32 *dst, struct chip_core_dmp *dmp)
{
	u32 index_page_vals       = 0;
	u32 index_page_write_regs = 0;
	const struct reg_addr *page_read_regs = dmpcfg.page_read_regs;
	u32 i, j;

	/* If one of the array size is zero, this means that
	   page mode is disabled. */
	if ((0 == dmpcfg.page_mode_values_count) ||
	    (0 == dmpcfg.page_write_regs_count) ||
	    (0 == dmpcfg.page_read_regs_count))
		return dst;

	for (index_page_vals = 0;
	     index_page_vals < dmpcfg.page_mode_values_count;
	     index_page_vals++) {
		for (index_page_write_regs = 0;
		     index_page_write_regs < dmpcfg.page_write_regs_count;
		     index_page_write_regs++) {
			WR_IND(bp,
			       dmpcfg.page_write_regs[index_page_write_regs],
			       dmpcfg.page_vals[index_page_vals]);
		}
		for (i = 0; i < dmpcfg.page_read_regs_count; i++) {
			if (dmpcfg.mode ==
				(page_read_regs[i].info & dmpcfg.mode)) {
				for (j = 0; j < page_read_regs[i].size; j++) {
					if ((dmp->fw_hdr.dmp_size + 4) >
					    DRV_DUMP_CRASH_DMP_BUF_SIZE_E3B0) {
						SET_FLAGS(dmp->fw_hdr.flags,
						   FWDMP_FLAGS_SPACE_NEEDED);
						break;
					}
					*dst++ = RD_IND(bp,
						  page_read_regs[i].addr + j*4);
					dmp->fw_hdr.dmp_size += 4;
				}
			}
		}
	}
	return dst;
}

static void
get_vfc_info(struct bnx2x *bp,
	     const struct vfc_general **xvfc_info,
	     const struct vfc_general **tvfc_info)
{
	if (CHIP_IS_E1(bp)) {
		*xvfc_info = &vfc_general_x_e1;
		*tvfc_info = &vfc_general_t_e1;
	} else if (CHIP_IS_E1H(bp)) {
		*xvfc_info = &vfc_general_x_e1h;
		*tvfc_info = &vfc_general_t_e1h;
	} else if (CHIP_IS_E2(bp)) {
		*xvfc_info = &vfc_general_x_e2;
		*tvfc_info = &vfc_general_t_e2;
	} else if (CHIP_IS_E3(bp)) {
		*xvfc_info = &vfc_general_x_e3;
		*tvfc_info = &vfc_general_t_e3;
	} else {
		*xvfc_info = NULL;
		*tvfc_info = NULL;
	}
}

static void get_vfc_ops(struct bnx2x *bp,
		   const struct vfc_read_task **xvfc_ops,
		   const struct vfc_read_task **tvfc_ops)
{
	if (CHIP_IS_E2(bp)) {
		*xvfc_ops = &vfc_task_x_e2;
		*tvfc_ops = &vfc_task_t_e2;
	} else if (CHIP_IS_E3(bp)) {
		*xvfc_ops = &vfc_task_x_e3;
		*tvfc_ops = &vfc_task_t_e3;
	} else {
		*xvfc_ops = NULL;
		*tvfc_ops = NULL;
	}
}

static void
init_extension_header(u32 data_type,
		      u32 data_source,
		      struct extension_hdr *header)
{
	header->hdr_signature = HDR_SIGNATURE;
	header->hdr_size = (sizeof(*header) -
			    sizeof(header->hdr_size)) / sizeof(u32);
	header->data_type = data_type;
	header->data_source = data_source;
}

static u8
wait_for_reg_value_equals(struct bnx2x *bp,
	u32 offset,
	u32 mask,
	u32 expected_value,
	u32 timeout_us)
{
	u32 reg_value = 0;
	u32 wait_cnt = 0;
	u32 wait_cnt_limit = timeout_us/DEFAULT_WAIT_INTERVAL_MICSEC;

	reg_value = RD_IND(bp, offset);
	while (((reg_value & mask) != expected_value) &&
		(wait_cnt++ != wait_cnt_limit)) {
		udelay(DEFAULT_WAIT_INTERVAL_MICSEC);
		reg_value = RD_IND(bp, offset);
	}
	return ((reg_value & mask) == expected_value);
}

static u32 *
read_vfc_block(struct bnx2x *bp,
		const struct vfc_general *vfc_info,
		const struct vfc_read_task *vfc_ops,
		struct extension_hdr *header,
		u32 *dst,
		struct chip_core_dmp *dmp,
		u8 *rc)
{
	u32 cur_op_idx, i;
	const struct vfc_read_write_vector *current_entry = NULL;
	u32 *dst_start = dst;

	if (!vfc_info->valid || !vfc_info->valid) {
		*rc = false;
		return dst;
	}
	if (!wait_for_reg_value_equals(bp, vfc_info->vfc_status,
				0xFFFFFFFF, 0, 1000)) {
		header->error = true;
		*rc = false;
		goto exit;
	}
	if (dmp->fw_hdr.dmp_size + sizeof(*header) >
		DRV_DUMP_CRASH_DMP_BUF_SIZE_E3B0) {
		SET_FLAGS(dmp->fw_hdr.flags, FWDMP_FLAGS_SPACE_NEEDED);
		*rc = false;
		return dst;
	}
	dst += sizeof(*header)/sizeof(u32);
	dmp->fw_hdr.dmp_size += sizeof(*header);
	for (cur_op_idx = 0; cur_op_idx < vfc_ops->array_size; ++cur_op_idx) {
		current_entry = &vfc_ops->read_write_vectors[cur_op_idx];
		for (i = 0; i < current_entry->write_value_num_valid; ++i) {
			WR_IND(bp,
				vfc_info->vfc_data_write,
				current_entry->write_value[i]);
		}
		WR_IND(bp,
			vfc_info->vfc_address, current_entry->address_value);
		for (i = 0; i < current_entry->read_size; ++i) {
			if (!wait_for_reg_value_equals(bp, vfc_info->vfc_status,
				RI_VFC_IS_READY, RI_VFC_IS_READY, 1000)) {
				u32 reg_value;

				reg_value =
					RD_IND(bp, vfc_info->vfc_status);
				header->error = true;
				*rc = false;
				goto exit;
			}
			if ((dmp->fw_hdr.dmp_size + 4) >
			    DRV_DUMP_CRASH_DMP_BUF_SIZE_E3B0) {
				SET_FLAGS(dmp->fw_hdr.flags,
					  FWDMP_FLAGS_SPACE_NEEDED);
				*rc = false;
				break;
			}
			*dst++ = RD_IND(bp, vfc_info->vfc_data_read);
			header->data_size++;
			dmp->fw_hdr.dmp_size += 4;
		}
	}
exit:
	memcpy((u8 *)dst_start, header, sizeof(*header));
	return dst;
}

static u32 *
read_vfc(struct bnx2x *bp, u32 *dst, struct chip_core_dmp *dmp, u8 *status)
{
	u8 rc = false;
	struct extension_hdr xvfc_header = {0};
	struct extension_hdr tvfc_header = {0};

	const struct vfc_general *xvfc_info = NULL;
	const struct vfc_general *tvfc_info = NULL;
	const struct vfc_read_task *xvfc_ops = NULL;
	const struct vfc_read_task *tvfc_ops = NULL;

	get_vfc_info(bp, &xvfc_info, &tvfc_info);
	get_vfc_ops(bp, &xvfc_ops, &tvfc_ops);
	if ((xvfc_info == NULL) || (tvfc_info == NULL)) {
		*status = false;
		return dst;
	}
	init_extension_header(RI_TYPE_VFC, RI_SRC_XSTORM, &xvfc_header);
	init_extension_header(RI_TYPE_VFC, RI_SRC_TSTORM, &tvfc_header);
	dst = read_vfc_block(bp,
		xvfc_info, xvfc_ops, &xvfc_header, dst, dmp, &rc);
	if (!rc) {
		*status = rc;
		return dst;
	}
	dst = read_vfc_block(bp,
		tvfc_info, tvfc_ops, &tvfc_header, dst, dmp, &rc);
	*status = rc;
	return dst;
}

static const struct igu_data *get_igu_info(struct bnx2x *bp)
{
	if (CHIP_IS_E1(bp))
		return &igu_address_e1;
	else if (CHIP_IS_E1H(bp))
		return &igu_address_e1h;
	else if (CHIP_IS_E2(bp))
		return &igu_address_e2;
	else if (CHIP_IS_E3(bp))
		return &igu_address_e3;
	else
		return NULL;
}

static u32 *
read_igu(struct bnx2x *bp, u32 *dst, struct chip_core_dmp *dmp, u8 *status)
{
	struct extension_hdr igu_header = {0};
	const struct igu_data *igu_info = get_igu_info(bp);
	u32 iter_cnt = 0;
	u32 more_data = 0;
	u32 *dst_start = dst;

	init_extension_header(RI_TYPE_IGU, RI_OTHER_BLOCK, &igu_header);
	if (!igu_info) {
		*status = false;
		return dst;
	}
	if (!igu_info->valid) {
		*status = true;
		return dst;
	}
	more_data = RD_IND(bp, igu_info->is_data_valid);
	if (!more_data) {
		*status = true;
		return dst;
	}
	if (dmp->fw_hdr.dmp_size + sizeof(igu_header) >
		DRV_DUMP_CRASH_DMP_BUF_SIZE_E3B0) {
		SET_FLAGS(dmp->fw_hdr.flags, FWDMP_FLAGS_SPACE_NEEDED);
		*status = false;
		return dst;
	}
	dst += sizeof(igu_header)/sizeof(u32);
	dmp->fw_hdr.dmp_size += sizeof(igu_header);
	igu_header.additional_data = RD_IND(bp, igu_info->is_last_commands);
	do {
		if ((dmp->fw_hdr.dmp_size + 4) >
		    DRV_DUMP_CRASH_DMP_BUF_SIZE_E3B0) {
			SET_FLAGS(dmp->fw_hdr.flags,
				  FWDMP_FLAGS_SPACE_NEEDED);
			igu_header.error = true;
			*status = false;
			break;
		}
		*dst++ = RD_IND(bp, igu_info->data[0]);
		++igu_header.data_size;
		dmp->fw_hdr.dmp_size += 4;
		if ((dmp->fw_hdr.dmp_size + 4) >
		    DRV_DUMP_CRASH_DMP_BUF_SIZE_E3B0) {
			SET_FLAGS(dmp->fw_hdr.flags,
				  FWDMP_FLAGS_SPACE_NEEDED);
			igu_header.error = true;
			*status = false;
			break;
		}
		*dst++ = RD_IND(bp, igu_info->data[1]);
		++igu_header.data_size;
		dmp->fw_hdr.dmp_size += 4;
		more_data = RD_IND(bp, igu_info->is_data_valid);
		++iter_cnt;
	} while (more_data && (iter_cnt < igu_info->max_size));

	memcpy((u8 *)dst_start, &igu_header, sizeof(igu_header));
	return dst;
}

static u32 *
read_additional_blocks(struct bnx2x *bp, u32 *dst, struct chip_core_dmp *dmp)
{
	u8 dmp_status = false;

	dst = read_vfc(bp, dst, dmp, &dmp_status);
	if (dmp_status)
		dst = read_igu(bp, dst, dmp, &dmp_status);
	return dst;
}

static u32 *
dmp_stop_timer(struct bnx2x *bp, u32 *dst, struct chip_core_dmp *dmp)
{
	u32 i, j;
	u32 *timer_scan_reg;

	if ((dmp->fw_hdr.dmp_size + (2 * dmpcfg.regs_timer_count)) >=
	    DRV_DUMP_CRASH_DMP_BUF_SIZE_E3B0) {
		SET_FLAGS(dmp->fw_hdr.flags,
			  FWDMP_FLAGS_SPACE_NEEDED);
		return dst;
	}
	if (dmp->fw_hdr.flags & FWDMP_FLAGS_LIVE_DUMP) {
		/* driver shouldn't read the timer in online
		   since it could cause attention. However,
		   it should fills the buffer and move on */
		for (i = 0;
		     i < 2 * dmpcfg.regs_timer_count;
		     i++) {
			*dst++ = 0;
			dmp->fw_hdr.dmp_size += 4;
		}
		return dst;
	}
	for (i = 0; i < dmpcfg.regs_timer_count; i++) {
		*dst = RD_IND(bp, dmpcfg.regs_timer_status_addrs[i]);
		timer_scan_reg = dst + dmpcfg.regs_timer_count;
		if (*dst == 1) {
			WR_IND(bp, dmpcfg.regs_timer_status_addrs[i], 0);
			for (j = 0; j < DRV_DUMP_MAX_TIMER_PENDING; j++) {
				*timer_scan_reg = RD_IND(bp,
					 dmpcfg.regs_timer_scan_addrs[i]);
				if (*timer_scan_reg == 0)
					break;
			}
		} else {
			*timer_scan_reg = DRV_DUMP_TIMER_SCAN_DONT_CARE;
		}
		dst++;
		dmp->fw_hdr.dmp_size += 4;
	}
	dst += dmpcfg.regs_timer_count;
	dmp->fw_hdr.dmp_size += 4 * dmpcfg.regs_timer_count;
	return dst;
}

static void
dmp_rollback_timer(struct bnx2x *bp,
	struct chip_core_dmp *dmp,
	u32 *tmr_status)
{
	u32 i;

	if (dmp->fw_hdr.flags & FWDMP_FLAGS_LIVE_DUMP)
		return;
	for (i = 0; i < dmpcfg.regs_timer_count; i++) {
		if (*tmr_status == 1)
			WR_IND(bp, dmpcfg.regs_timer_status_addrs[i], 1);
		tmr_status++;
	}
}

static int
init_dump_header(struct bnx2x *bp,
	struct dump_hdr *dmp_hdr,
	struct dmp_config *dmpcfg,
	struct chip_core_dmp *dmp)
{
	dmp_hdr->hdr_size = (sizeof(struct dump_hdr)/4) - 1;
	dmp_hdr->hd_param_all = hd_param_all;
	dmp_hdr->idle_chk = 1;
	dmp_hdr->x_storm_wait_p_status =
		RD_IND(bp, DRV_DUMP_XSTORM_WAITP_ADDRESS);
	dmp_hdr->t_storm_wait_p_status =
		RD_IND(bp, DRV_DUMP_TSTORM_WAITP_ADDRESS);
	dmp_hdr->u_storm_wait_p_status =
		RD_IND(bp, DRV_DUMP_USTORM_WAITP_ADDRESS);
	dmp_hdr->c_storm_wait_p_status =
		RD_IND(bp, DRV_DUMP_CSTORM_WAITP_ADDRESS);

	if (CHIP_IS_E1(bp)) {
		if (dmp->fw_hdr.flags & FWDMP_FLAGS_LIVE_DUMP)
			dmp_hdr->info = RI_E1_ONLINE;
		else
			dmp_hdr->info = RI_E1_OFFLINE;
		dmpcfg->wregs_count = WREGS_COUNT_E1;
		dmpcfg->pwreg_addrs = wreg_addrs_e1;
		dmpcfg->regs_timer_count = TIMER_REGS_COUNT_E1;
		dmpcfg->regs_timer_status_addrs = timer_status_regs_e1;
		dmpcfg->regs_timer_scan_addrs = timer_scan_regs_e1;
		dmpcfg->page_mode_values_count =
				PAGE_MODE_VALUES_E1;
		dmpcfg->page_vals = page_vals_e1;
		dmpcfg->page_write_regs_count = PAGE_WRITE_REGS_E1;
		dmpcfg->page_write_regs = page_write_regs_e1;
		dmpcfg->page_read_regs_count = PAGE_READ_REGS_E1;
		dmpcfg->page_read_regs = page_read_regs_e1;
	} else if (CHIP_IS_E1H(bp)) {
		if (dmp->fw_hdr.flags & FWDMP_FLAGS_LIVE_DUMP)
			dmp_hdr->info = RI_E1H_ONLINE;
		else
			dmp_hdr->info = RI_E1H_OFFLINE;
		dmpcfg->wregs_count = WREGS_COUNT_E1H;
		dmpcfg->pwreg_addrs = wreg_addrs_e1h;
		dmpcfg->regs_timer_count = TIMER_REGS_COUNT_E1H;
		dmpcfg->regs_timer_status_addrs = timer_status_regs_e1h;
		dmpcfg->regs_timer_scan_addrs = timer_scan_regs_e1h;
		dmpcfg->page_mode_values_count =
				PAGE_MODE_VALUES_E1H;
		dmpcfg->page_vals = page_vals_e1h;
		dmpcfg->page_write_regs_count = PAGE_WRITE_REGS_E1H;
		dmpcfg->page_write_regs = page_write_regs_e1h;
		dmpcfg->page_read_regs_count = PAGE_READ_REGS_E1H;
		dmpcfg->page_read_regs = page_read_regs_e1h;
	} else if (CHIP_IS_E2(bp)) {
		if (dmp->fw_hdr.flags & FWDMP_FLAGS_LIVE_DUMP)
			dmp_hdr->info = RI_E2_ONLINE;
		else
			dmp_hdr->info = RI_E2_OFFLINE;
		dmpcfg->wregs_count = WREGS_COUNT_E2;
		dmpcfg->pwreg_addrs = wreg_addrs_e2;
		dmpcfg->regs_timer_count = TIMER_REGS_COUNT_E2;
		dmpcfg->regs_timer_status_addrs = timer_status_regs_e2;
		dmpcfg->regs_timer_scan_addrs = timer_scan_regs_e2;
		dmpcfg->page_mode_values_count =
				PAGE_MODE_VALUES_E2;
		dmpcfg->page_vals = page_vals_e2;
		dmpcfg->page_write_regs_count = PAGE_WRITE_REGS_E2;
		dmpcfg->page_write_regs = page_write_regs_e2;
		dmpcfg->page_read_regs_count = PAGE_READ_REGS_E2;
		dmpcfg->page_read_regs = page_read_regs_e2;
	} else if (CHIP_IS_E3A0(bp)) {
		if (dmp->fw_hdr.flags & FWDMP_FLAGS_LIVE_DUMP)
			dmp_hdr->info = RI_E3_ONLINE;
		else
			dmp_hdr->info = RI_E3_OFFLINE;
		dmpcfg->wregs_count = WREGS_COUNT_E3;
		dmpcfg->pwreg_addrs = wreg_addrs_e3;
		dmpcfg->regs_timer_count = TIMER_REGS_COUNT_E3;
		dmpcfg->regs_timer_status_addrs = timer_status_regs_e3;
		dmpcfg->regs_timer_scan_addrs = timer_scan_regs_e3;
		dmpcfg->page_mode_values_count =
				PAGE_MODE_VALUES_E3;
		dmpcfg->page_vals = page_vals_e3;
		dmpcfg->page_write_regs_count = PAGE_WRITE_REGS_E3;
		dmpcfg->page_write_regs = page_write_regs_e3;
		dmpcfg->page_read_regs_count = PAGE_READ_REGS_E3;
		dmpcfg->page_read_regs = page_read_regs_e3;
	} else if (CHIP_IS_E3B0(bp)) {
		if (dmp->fw_hdr.flags & FWDMP_FLAGS_LIVE_DUMP)
			dmp_hdr->info = RI_E3B0_ONLINE;
		else
			dmp_hdr->info = RI_E3B0_OFFLINE;
		dmpcfg->wregs_count = WREGS_COUNT_E3B0;
		dmpcfg->pwreg_addrs = wreg_addrs_e3b0;
		dmpcfg->regs_timer_count = TIMER_REGS_COUNT_E3B0;
		dmpcfg->regs_timer_status_addrs = timer_status_regs_e3b0;
		dmpcfg->regs_timer_scan_addrs = timer_scan_regs_e3b0;
		dmpcfg->page_mode_values_count =
				PAGE_MODE_VALUES_E3;
		dmpcfg->page_vals = page_vals_e3;
		dmpcfg->page_write_regs_count = PAGE_WRITE_REGS_E3;
		dmpcfg->page_write_regs = page_write_regs_e3;
		dmpcfg->page_read_regs_count = PAGE_READ_REGS_E3;
		dmpcfg->page_read_regs = page_read_regs_e3;
	} else {
		BNX2X_ERR("firmware dump chip not supported\n");
		return 1;
	}
	switch (BP_PATH(bp)) {
	case 0:
		dmp_hdr->info |= RI_PATH0_DUMP;
		break;
	case 1:
		dmp_hdr->info |= RI_PATH1_DUMP;
		break;
	default:
		BNX2X_ERR("unknown path ID (%x)\n", BP_PATH(bp));
	}
	dmpcfg->mode = dmp_hdr->info & ~(RI_PATH1_DUMP |  RI_PATH0_DUMP);
	dmp_hdr->reserved = 0;
	return 0;
}

static void disable_pause(struct bnx2x *bp)
{
	if (CHIP_IS_E1x(bp) || CHIP_IS_E2(bp)) {
		WR_IND(bp, NIG_REG_BMAC0_PAUSE_OUT_EN, 0);
		WR_IND(bp, NIG_REG_BMAC1_PAUSE_OUT_EN, 0);
		WR_IND(bp, NIG_REG_EMAC0_PAUSE_OUT_EN, 0);
		WR_IND(bp, NIG_REG_EMAC1_PAUSE_OUT_EN, 0);
	} else {
		WR_IND(bp, NIG_REG_P0_MAC_PAUSE_OUT_EN, 0);
		WR_IND(bp, NIG_REG_P1_MAC_PAUSE_OUT_EN, 0);
	}
}

VMK_ReturnStatus bnx2x_fwdmp_callback(void *cookie, vmk_Bool liveDump)
{
	VMK_ReturnStatus status = VMK_OK;
	u32 idx;
	u32 *dst, *tmr_status;
	struct bnx2x *bp;
	struct dump_hdr dmp_hdr = {0};
	struct chip_core_dmp *dmp;

	for (idx = 0; idx < BNX2X_MAX_NIC; idx++) {
		if (bnx2x_fwdmp_va && bnx2x_fwdmp_bp[idx].bp) {
			if (bnx2x_fwdmp_bp[idx].disable_fwdmp)
				continue;
			bp = bnx2x_fwdmp_bp[idx].bp;
			dst = bnx2x_fwdmp_va;
			/* build the fw dump header */
			dmp = (struct chip_core_dmp *)dst;
			snprintf(dmp->fw_hdr.name, sizeof(dmp->fw_hdr.name),
				"%s", bp->dev->name);
			dmp->fw_hdr.bp = (void *)bp;
			dmp->fw_hdr.chip_id = bp->common.chip_id;
			dmp->fw_hdr.len = sizeof(struct fw_dmp_hdr);
			dmp->fw_hdr.ver = BNX2X_ESX_FW_DMP_VER;
			dmp->fw_hdr.dmp_size = dmp->fw_hdr.len;
			dmp->fw_hdr.flags = 0;
			if (liveDump)
				SET_FLAGS(dmp->fw_hdr.flags, FWDMP_FLAGS_LIVE_DUMP);
			memset(&dmpcfg, 0, sizeof(struct dmp_config));
			memset(&dmp_hdr, 0, sizeof(struct dump_hdr));
			dst = dmp->fw_dmp_buf;
			bnx2x_disable_blocks_parity(bp);
			/* build the GRC dump header */
			if (init_dump_header(bp, &dmp_hdr, &dmpcfg, dmp))
				continue;
			memcpy(dst, &dmp_hdr, sizeof(struct dump_hdr));
			dst += dmp_hdr.hdr_size + 1;
			dmp->fw_hdr.dmp_size += (dmp_hdr.hdr_size + 1) * 4;
			/* stop the timers before idle check. */
			tmr_status = dst;
			dst = dmp_stop_timer(bp, dst, dmp);
			if (dmp->fw_hdr.flags & FWDMP_FLAGS_SPACE_NEEDED)
				goto write_file;
			/* dump 1st idle check */
			dst = read_idle_chk(bp, dst, dmp);
			if (dmp->fw_hdr.flags & FWDMP_FLAGS_SPACE_NEEDED)
				goto write_file;
			/* dump 2nd idle check */
			dst = read_idle_chk(bp, dst, dmp);
			/* Enable the timers after idle check. */
			dmp_rollback_timer(bp, dmp, tmr_status);
			if (dmp->fw_hdr.flags & FWDMP_FLAGS_SPACE_NEEDED)
				goto write_file;
			/* dump mcp traces */
			dst = read_mcp_traces(bp, dst, dmp);
			if (dmp->fw_hdr.flags & FWDMP_FLAGS_SPACE_NEEDED)
				goto write_file;
			/* dump regular address registers */
			dst = read_regular_regs(bp, dst, dmp);
			if (dmp->fw_hdr.flags & FWDMP_FLAGS_SPACE_NEEDED)
				goto write_file;
			/* dump wide bus registers */
			dst = read_wregs(bp, dst, dmp);
			if (dmp->fw_hdr.flags & FWDMP_FLAGS_SPACE_NEEDED)
				goto write_file;
			/* dump read page mode registers */
			dst = read_page_mode(bp, dst, dmp);
			if (dmp->fw_hdr.flags & FWDMP_FLAGS_SPACE_NEEDED)
				goto write_file;
			/* dump additional blocks */
			dst = read_additional_blocks(bp, dst, dmp);
			if (((dmp->fw_hdr.dmp_size + 4) <=
			      DRV_DUMP_CRASH_DMP_BUF_SIZE_E3B0)) {
				*dst++ = BNX2X_FWDMP_MARKER_END;
				dmp->fw_hdr.dmp_size += 4;
			}
write_file:
			status = vmklnx_dump_range(bnx2x_fwdmp_dh,
					bnx2x_fwdmp_va, dmp->fw_hdr.dmp_size);
			if (status != VMK_OK) {
				BNX2X_ERR("failed to dump firmware/chip "
					  "data %x %d!\n", status, idx);
				break;
			}
			/* Re-enable parity attentions */
			bnx2x_clear_blocks_parity(bp);
			bnx2x_enable_blocks_parity(bp);
			if (!(dmp->fw_hdr.flags & FWDMP_FLAGS_LIVE_DUMP))
				disable_pause(bp);
		}
	}
	/* restore firmware dump on disabled adapters */
	for (idx = 0; idx < BNX2X_MAX_NIC; idx++) {
		if (bnx2x_fwdmp_bp[idx].bp && bnx2x_fwdmp_bp[idx].disable_fwdmp)
			bnx2x_fwdmp_bp[idx].disable_fwdmp = 0;
	}
	return status;
}

/*
 * Disable firmware dump on netdump worker NIC (bp) such that, the
 * grcDump, which is very intrusive, won't interrupt netdump
 * traffic. Besides the netdump worker NIC, we also need to disable
 * grcDump on other functions that shared the same device as worker
 * NIC function.
 */
void bnx2x_disable_esx_fwdmp(struct bnx2x *bp)
{
	u32 i, j;
	for (i = 0; i < BNX2X_MAX_NIC; i++) {
		if (bnx2x_fwdmp_bp[i].bp == bp) {
			struct bnx2x *fw_bp;

			bnx2x_fwdmp_bp[i].disable_fwdmp = 1;
			netdev_info(bp->dev,
			   "Firmware dump disabled on netdump worker "
			   "(bp=%p, %d).\n",
			   bp, i);
			for (j = 0; j < BNX2X_MAX_NIC; j++) {
				fw_bp = bnx2x_fwdmp_bp[j].bp;
				/* disable fw dmp on the functions that
				   shared the same device as well */
				if (fw_bp && fw_bp != bp &&
				    fw_bp->pdev->bus == bp->pdev->bus) {
					bnx2x_fwdmp_bp[j].disable_fwdmp = 1;
					netdev_info(fw_bp->dev,
					   "Firmware dump disabled on function "
					   "sharing the same device (bp=%p, %d).\n",
					   fw_bp, j);
					fw_bp->esx.poll_disable_fwdmp = 1;
				}
			}
			bp->esx.poll_disable_fwdmp = 1;
			break;
		}
	}
}

static void bnx2x_esx_register_fwdmp(void)
{
	VMK_ReturnStatus status;

	if (!disable_fw_dmp) {

		bnx2x_fwdmp_va = kzalloc(DRV_DUMP_CRASH_DMP_BUF_SIZE_E3B0,
					 GFP_KERNEL);
		if (!bnx2x_fwdmp_va)
			pr_info("bnx2x: can't alloc mem for dump handler!\n");
		else {
			status = vmklnx_dump_add_callback(BNX2X_DUMPNAME,
					bnx2x_fwdmp_callback,
					NULL,
					BNX2X_DUMPNAME,
					&bnx2x_fwdmp_dh);
			if (status != VMK_OK)
				pr_info("bnx2x: can't add dump handler (rc = 0x%x!)\n",
					status);
		}
	}
}

static void bnx2x_esx_unregister_fwdmp(void)
{
	if (bnx2x_fwdmp_dh) {
		VMK_ReturnStatus status =
			vmklnx_dump_delete_callback(bnx2x_fwdmp_dh);
		if (status != VMK_OK) {
			VMK_ASSERT(0);
		} else {
			pr_info("bnx2x: dump handler (%p) unregistered!\n",
				bnx2x_fwdmp_dh);
		}
	}
	kfree(bnx2x_fwdmp_va);
	bnx2x_fwdmp_va = NULL;
}
#endif

int bnx2x_esx_mod_init(void)
{
#if (VMWARE_ESX_DDK_VERSION >= 55000)
	bnx2x_esx_register_fwdmp();
#endif
#if (VMWARE_ESX_DDK_VERSION >= 50000)
	if (cnic_register_adapter("bnx2x", bnx2x_cnic_probe) == 0)
		registered_cnic_adapter = 1;
	else
		pr_err("Unable to register with CNIC adapter\n");
#endif
	return 0;
}

void bnx2x_esx_mod_cleanup(void)
{
#if (VMWARE_ESX_DDK_VERSION >= 55000)
	bnx2x_esx_unregister_fwdmp();
#endif
#if (VMWARE_ESX_DDK_VERSION >= 50000)
	if (registered_cnic_adapter) {
		cnic_register_cancel("bnx2x");
		registered_cnic_adapter = 0;
	}
#endif
}

#ifdef BNX2X_ESX_SRIOV
int bnx2x_esx_reset_bp_vf(struct bnx2x *bp)
{
	int i;

	for (i = 0; i < BP_VFDB(bp)->sriov.nr_virtfn; i++) {
		struct bnx2x_esx_vf *esx_vf = &bp->esx.vf[i];

		esx_vf->old_mtu = BNX2X_ESX_PASSTHRU_MTU_UNINITIALIZED;
		init_waitqueue_head(&esx_vf->passthru_wait_config);
		init_waitqueue_head(&esx_vf->passthru_wait_comp);
	}

	return 0;
}
#else
int bnx2x_esx_reset_bp_vf(struct bnx2x *bp)
{
	return 0;
}
#endif

static void bnx2x_esx_free_mem_bp(struct bnx2x *bp)
{
	if (IS_SRIOV(bp)) {
		BNX2X_FREE(bp->esx.old_vf_fw_stats);

		BNX2X_PCI_FREE(bp->esx.vf_fw_stats,
			       bp->esx.vf_fw_stats_mapping,
			       bp->esx.vf_fw_stats_size);

		BNX2X_FREE(bp->esx.vf);
	}
	if (bp->esx.free_netq_pool)
		BNX2X_FREE(bp->esx.free_netq_pool);
}

int bnx2x_esx_init_bp(struct bnx2x *bp)
{
	int index, num_queue;
#ifdef BNX2X_ESX_SRIOV
	if (IS_SRIOV(bp)) {
		BNX2X_ALLOC(bp->esx.vf,
			    BP_VFDB(bp)->sriov.nr_virtfn *
				sizeof(struct bnx2x_esx_vf));

		bnx2x_esx_reset_bp_vf(bp);
	}
#endif


	/* fp data structure has been zeroed out, counters need to be cleared */
	bp->esx.n_rx_queues_allocated = 0;
	bp->esx.n_tx_queues_allocated = 0;
	/*
	 * The following routine iterates over all the net-queues allocating
	 * the queues that will use LRO. It sets their internal state
	 * including the 'disable_tpa' field. This must be done prior to
	 * setting up the queue below.
	 */
	bnx2x_reserve_netq_feature(bp);

	num_queue = BNX2X_NUM_RX_NETQUEUES(bp);
	BNX2X_ALLOC(bp->esx.free_netq_pool,
		    num_queue * sizeof(struct netq_list));
	INIT_LIST_HEAD(&bp->esx.free_netq_list);
	for (index = 0; index < num_queue; index++) {
		/* Initialize to all non-default queues */
		bp->esx.free_netq_pool[index].fp = &bp->fp[index+1];
		list_add((struct list_head *)(bp->esx.free_netq_pool+index),
			 &bp->esx.free_netq_list);
	}

	if (IS_SRIOV(bp)) {
		bp->esx.vf_fw_stats_size =
				BP_VFDB(bp)->sriov.nr_virtfn *
				BNX2X_VF_MAX_QUEUES *
				PAGE_ALIGN(sizeof(struct per_queue_stats));

		BNX2X_PCI_ALLOC(bp->esx.vf_fw_stats,
				&bp->esx.vf_fw_stats_mapping,
				bp->esx.vf_fw_stats_size);

		BNX2X_ALLOC(bp->esx.old_vf_fw_stats, bp->esx.vf_fw_stats_size);
	}

	return 0;

alloc_mem_err:
	bnx2x_esx_free_mem_bp(bp);

	BNX2X_ERR("Failed to allocate ESX bp memory\n");
	return -ENOMEM;
}

void bnx2x_esx_cleanup_bp(struct bnx2x *bp)
{
	bnx2x_esx_free_mem_bp(bp);
}

int bnx2x_esx_is_BCM57800_1G(struct bnx2x *bp)
{
	return ((SHMEM_RD(bp,
			 dev_info.port_hw_config[BP_PORT(bp)].default_cfg) &
		 PORT_HW_CFG_NET_SERDES_IF_MASK) ==  
		PORT_HW_CFG_NET_SERDES_IF_SGMII);
}

/*******************/
