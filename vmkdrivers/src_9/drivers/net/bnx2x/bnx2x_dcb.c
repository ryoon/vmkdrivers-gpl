/* bnx2x_dcb.c: Broadcom Everest network driver.
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
 * Written by: Dmitry Kravkov
 *
 */
#include <linux/version.h>
#include <linux/errno.h>

#include "bnx2x.h"
#include "bnx2x_common.h"
#include "bnx2x_dcb.h"


/* forward declarations of dcbx related functions */
static void bnx2x_dcbx_stop_hw_tx(struct bnx2x *bp);
static void bnx2x_pfc_set_pfc(struct bnx2x *bp);
static void bnx2x_dcbx_update_ets_params(struct bnx2x *bp);
static void bnx2x_dcbx_resume_hw_tx(struct bnx2x *bp);
static void bnx2x_dcbx_get_ets_pri_pg_tbl(struct bnx2x*, u32* ,u8* );
static void bnx2x_dcbx_get_num_pg_traf_type(struct bnx2x *bp,
					    u32 *pg_pri_orginal_spread,
					    struct pg_help_data *help_data);
static void bnx2x_dcbx_fill_cos_params(struct bnx2x *bp,
				       struct pg_help_data *help_data,
				       struct dcbx_ets_feature *ets,
				       u32 *pg_pri_orginal_spread);
static void bnx2x_dcbx_separate_pauseable_from_non(struct bnx2x *bp,
				struct cos_help_data *cos_data,
				u32 *pg_pri_orginal_spread,
				struct dcbx_ets_feature *ets);
static void bnx2x_pfc_fw_struct_e2(struct bnx2x *bp);


static void bnx2x_pfc_set(struct bnx2x *bp)
{
	struct bnx2x_nig_brb_pfc_port_params pfc_params = {0};
	u32 pri_bit, val = 0;
	u8 pri;

	/* Tx COS configuration */
	if (bp->dcbx_port_params.ets.cos_params[0].pauseable)
		pfc_params.rx_cos0_priority_mask =
			bp->dcbx_port_params.ets.cos_params[0].pri_bitmask;
	if (bp->dcbx_port_params.ets.cos_params[1].pauseable)
		pfc_params.rx_cos1_priority_mask =
			bp->dcbx_port_params.ets.cos_params[1].pri_bitmask;


	/**
	 * Rx COS configuration
	 * Changing PFC RX configuration .
	 * In RX COS0 will always be configured to lossy and COS1 to lossless
	 */
	for (pri = 0 ; pri < MAX_PFC_PRIORITIES ; pri++) {
		pri_bit = 1 << pri;

		if (pri_bit & DCBX_PFC_PRI_PAUSE_MASK(bp))
			val |= 1 << (pri * 4);
	}

	pfc_params.pkt_priority_to_cos = val;

	/* RX COS0 */
	pfc_params.llfc_low_priority_classes = 0;
	/* RX COS1 */
	pfc_params.llfc_high_priority_classes = DCBX_PFC_PRI_PAUSE_MASK(bp);

	/* BRB configuration */
	pfc_params.cos0_pauseable = false;
	pfc_params.cos1_pauseable = true;

	bnx2x_acquire_phy_lock(bp);
	bp->link_params.feature_config_flags |= FEATURE_CONFIG_PFC_ENABLED;
	bnx2x_update_pfc(&bp->link_params, &bp->link_vars, &pfc_params);
	bnx2x_release_phy_lock(bp);
}

static void bnx2x_pfc_clear(struct bnx2x *bp)
{
	struct bnx2x_nig_brb_pfc_port_params nig_params = {0};
	nig_params.pause_enable = 1;
#ifdef BNX2X_SAFC
	if (bp->flags & SAFC_TX_FLAG) {
		u32 high = 0, low = 0;
		int i;

		for (i = 0; i < BNX2X_MAX_PRIORITY; i++) {
			if (bp->pri_map[i] == 1)
				high |= (1 << i);
			if (bp->pri_map[i] == 0)
				low |= (1 << i);
		}

		nig_params.llfc_low_priority_classes = high;
		nig_params.llfc_low_priority_classes = low;

		nig_params.pause_enable = 0;
		nig_params.llfc_enable = 1;
		nig_params.llfc_out_en = 1;
	}
#endif /* BNX2X_SAFC */
	bnx2x_acquire_phy_lock(bp);
	bp->link_params.feature_config_flags &= ~FEATURE_CONFIG_PFC_ENABLED;
	bnx2x_update_pfc(&bp->link_params, &bp->link_vars, &nig_params);
	bnx2x_release_phy_lock(bp);
}

static int bnx2x_is_valid_lldp_params(struct bnx2x *bp) {
	return 1;
}

static int bnx2x_is_valid_dcbx_params(struct bnx2x *bp) {
	return 1;
}

static void  bnx2x_dump_dcbx_drv_param(struct bnx2x *bp,
				       struct lldp_local_mib *local_mib) {
#ifdef BNX2X_DUMP_DCBX
	u8 i = 0;
	BNX2X_ERR("local_mib.error %x\n",local_mib->error);

	/* PG */
	BNX2X_ERR("local_mib.features.ets.enabled %x\n",
		  local_mib->features.ets.enabled);
	for( i = 0; i < DCBX_MAX_NUM_PG; i++)
		BNX2X_ERR("local_mib.features.ets.pg_bw_tbl[%x] %x\n", i,
					local_mib->features.ets.pg_bw_tbl[i]);
	for( i = 0; i < DCBX_MAX_NUM_PRI_PG_ENTRIES; i++)
		BNX2X_ERR("local_mib.features.ets.pri_pg_tbl[%x] %x\n", i,
					local_mib->features.ets.pri_pg_tbl[i]);

	/* pfc */
	BNX2X_ERR("local_mib.features.pfc.pri_en_bitmap %x\n",
					local_mib->features.pfc.pri_en_bitmap);
	BNX2X_ERR("local_mib.features.pfc.pfc_caps %x\n",
					local_mib->features.pfc.pfc_caps);
	BNX2X_ERR("local_mib.features.pfc.enabled %x\n",
					local_mib->features.pfc.enabled);

	BNX2X_ERR("local_mib.features.app.default_pri %x\n",
					local_mib->features.app.default_pri);
	BNX2X_ERR("local_mib.features.app.tc_supported %x\n",
					local_mib->features.app.tc_supported);
	BNX2X_ERR("local_mib.features.app.enabled %x\n",
					local_mib->features.app.enabled);
	for( i = 0; i < DCBX_MAX_APP_PROTOCOL; i++) {
		BNX2X_ERR("local_mib.features.app.app_pri_tbl[%x].app_id[0] %x"
		    "\n", i, local_mib->features.app.app_pri_tbl[i].app_id[0]);
		BNX2X_ERR("local_mib.features.app.app_pri_tbl[%x].app_id[1] %x"
		    "\n", i, local_mib->features.app.app_pri_tbl[i].app_id[1]);
		BNX2X_ERR("local_mib.features.app.app_pri_tbl[%x].pri_bitmap %x"
		    "\n", i, local_mib->features.app.app_pri_tbl[i].pri_bitmap);
		BNX2X_ERR("local_mib.features.app.app_pri_tbl[%x].appBitfield "
		   "%x\n",i,local_mib->features.app.app_pri_tbl[i].appBitfield);
	}
#endif /* BNX2X_DUMP_DCBX */
}

static void bnx2x_dcbx_get_ap_priority(struct bnx2x *bp,
				       u8 pri_bitmap,
				       u8 llfc_traf_type)
{
	u32 pri = MAX_PFC_PRIORITIES;
	u32 index = MAX_PFC_PRIORITIES -1;
	u32 pri_mask;
	u32 *ttp = bp->dcbx_port_params.app.traffic_type_priority;

	/* Choose the highest priority */
	while((MAX_PFC_PRIORITIES == pri ) && (0 != index)) {
		pri_mask = 1 << index;
		if(GET_FLAGS(pri_bitmap , pri_mask))
			pri = index ;
		index--;
	}

	if(pri < MAX_PFC_PRIORITIES)
		ttp[llfc_traf_type] = max_t(u32, ttp[llfc_traf_type], pri);
}

static void bnx2x_dcbx_get_ap_feature(struct bnx2x *bp,
				   struct dcbx_app_priority_feature *app,
				   u32 error) {
	u8 index;
	u16 app_protocol_id;
	u32 *ttp = bp->dcbx_port_params.app.traffic_type_priority;

	if (GET_FLAGS(error,DCBX_LOCAL_APP_ERROR))
		DP(NETIF_MSG_LINK, "DCBX_LOCAL_APP_ERROR\n");

	if( app->enabled && !GET_FLAGS(error,DCBX_LOCAL_APP_ERROR)) {

		bp->dcbx_port_params.app.enabled = true;

		for( index=0 ; index < LLFC_DRIVER_TRAFFIC_TYPE_MAX; index++)
			ttp[index] = 0;

		if(app->default_pri < MAX_PFC_PRIORITIES)
			ttp[LLFC_TRAFFIC_TYPE_NW] = app->default_pri;

		for(index=0 ; index < DCBX_MAX_APP_PROTOCOL; index++) {
			struct dcbx_app_priority_entry *entry =
							app->app_pri_tbl;

			app_protocol_id = entry[index].app_id[0] |
					  entry[index].app_id[1] << 8;

			/* Arrays that there cell are less than 32 bit are still
			 * in big endian mode.*/
			app_protocol_id = be16_to_cpu(app_protocol_id);
			if(GET_FLAGS(entry[index].appBitfield,
				     DCBX_APP_SF_ETH_TYPE) &&
			   ETH_TYPE_FCOE == app_protocol_id)
				bnx2x_dcbx_get_ap_priority(bp,
						entry[index].pri_bitmap,
						LLFC_TRAFFIC_TYPE_FCOE);

			if(GET_FLAGS(entry[index].appBitfield,
				     DCBX_APP_SF_PORT) &&
			   TCP_PORT_ISCSI == app_protocol_id)
				bnx2x_dcbx_get_ap_priority(bp,
						entry[index].pri_bitmap,
						LLFC_TRAFFIC_TYPE_ISCSI);
		}
	} else {
		DP(NETIF_MSG_LINK, "DCBX_LOCAL_APP_DISABLED\n");
		bp->dcbx_port_params.app.enabled = false;
		for( index=0 ; index < LLFC_DRIVER_TRAFFIC_TYPE_MAX; index++)
			ttp[index] = INVALID_TRAFFIC_TYPE_PRIORITY;
	}
}

static void bnx2x_dcbx_get_ets_feature(struct bnx2x *bp,
				       struct dcbx_ets_feature *ets,
				       u32 error) {
	int i = 0;
	u32 *buff;
	u32 pg_pri_orginal_spread[DCBX_MAX_NUM_PG] = {0};
	struct pg_help_data pg_help_data;
	struct bnx2x_dcbx_cos_params *cos_params =
			bp->dcbx_port_params.ets.cos_params;

	memset(&pg_help_data, 0, sizeof(struct pg_help_data));


	if (GET_FLAGS(error,DCBX_LOCAL_ETS_ERROR))
		DP(NETIF_MSG_LINK, "DCBX_LOCAL_ETS_ERROR\n");


	buff = (u32*)ets->pg_bw_tbl;
	for ( i = 0; i < sizeof(ets->pg_bw_tbl)/sizeof(u32); i++)
		/* Arrays that there cell are less than 32 bit are still
		 * in big endian mode. */
		buff[i] =  be32_to_cpu(buff[i]);

	/* Clean up old settings of ets on COS */
	for(i = 0; i < E2_NUM_OF_COS ; i++) {

		cos_params[i].pauseable = false;
		cos_params[i].strict = BNX2X_DCBX_COS_NOT_STRICT;
		cos_params[i].bw_tbl = DCBX_INVALID_COS_BW;
		cos_params[i].pri_bitmask = DCBX_PFC_PRI_GET_NON_PAUSE(bp,0);
	}

	if(bp->dcbx_port_params.app.enabled &&
	   !GET_FLAGS(error,DCBX_LOCAL_ETS_ERROR) &&
	   ets->enabled) {
		DP(NETIF_MSG_LINK, "DCBX_LOCAL_ETS_ENABLE\n");
		bp->dcbx_port_params.ets.enabled = true;

		bnx2x_dcbx_get_ets_pri_pg_tbl(bp,
					      pg_pri_orginal_spread,
					      ets->pri_pg_tbl);

		bnx2x_dcbx_get_num_pg_traf_type(bp,
						pg_pri_orginal_spread,
						&pg_help_data);

		bnx2x_dcbx_fill_cos_params(bp, &pg_help_data,
					   ets, pg_pri_orginal_spread);

	} else {
		DP(NETIF_MSG_LINK, "DCBX_LOCAL_ETS_DISABLED\n");
		bp->dcbx_port_params.ets.enabled = false;
		memset(ets->pri_pg_tbl, 0, sizeof(ets->pri_pg_tbl));
		for(i = 0; i < DCBX_MAX_NUM_PRI_PG_ENTRIES ; i++)
			ets->pg_bw_tbl[i] = 1;
	}
}

static void  bnx2x_dcbx_get_pfc_feature(struct bnx2x *bp,
					struct dcbx_pfc_feature *pfc, u32 error)
{

	if (GET_FLAGS(error,DCBX_LOCAL_PFC_ERROR))
		DP(NETIF_MSG_LINK, "DCBX_LOCAL_PFC_ERROR\n");

	if(bp->dcbx_port_params.app.enabled &&
	   !GET_FLAGS(error,DCBX_LOCAL_PFC_ERROR) &&
	   pfc->enabled) {
		bp->dcbx_port_params.pfc.enabled = true;
		bp->dcbx_port_params.pfc.priority_non_pauseable_mask =
			~(pfc->pri_en_bitmap);
		BNX2X_ERR("priority_non_pauseable_mask 0x%x\n",
			  bp->dcbx_port_params.pfc.priority_non_pauseable_mask);
	} else {
		DP(NETIF_MSG_LINK, "DCBX_LOCAL_PFC_DISABLED\n");
		bp->dcbx_port_params.pfc.enabled = false;
		bp->dcbx_port_params.pfc.priority_non_pauseable_mask = 0;
	}
}

static void bnx2x_get_dcbx_drv_param(struct bnx2x *bp,
			struct bnx2x_dcbx_port_params *dcbx_port_params,
			struct lldp_local_mib *local_mib)
{
	bnx2x_dcbx_get_ap_feature(bp,
				  &local_mib->features.app,
				  local_mib->error);

	bnx2x_dcbx_get_pfc_feature(bp,
				  &local_mib->features.pfc,
				  local_mib->error);

	bnx2x_dcbx_get_ets_feature(bp,
				  &local_mib->features.ets,
				  local_mib->error);
}

#define DCBX_LOCAL_MIB_MAX_TRY_READ		(100)
static int bnx2x_dcbx_read_mib(struct bnx2x *bp,
			       u32 *base_mib_addr,
			       u32 offset,
			       bnx2x_dcbx_read_mib_type read_mib_type)
{
	int max_try_read = 0, i;
	u32 *buff, mib_size, prefix_seq_num, suffix_seq_num;
	struct lldp_remote_mib *remote_mib ;
	struct lldp_local_mib  *local_mib;


	switch (read_mib_type) {
	case DCBX_READ_LOCAL_MIB:
		mib_size = sizeof(struct lldp_local_mib);
		break;
	case DCBX_READ_REMOTE_MIB:
		mib_size = sizeof(struct lldp_remote_mib);
		break;
	default:
		return 1; /*error*/
	}

	offset += BP_PORT(bp) * mib_size;

	do {
		buff = base_mib_addr;
		for(i = 0 ;i < mib_size; i += 4, buff++)
		*buff = REG_RD(bp, offset + i);

		max_try_read++;

		switch (read_mib_type) {
		case DCBX_READ_LOCAL_MIB:
			local_mib  = (struct lldp_local_mib*) base_mib_addr;
			prefix_seq_num = local_mib->prefix_seq_num;
			suffix_seq_num = local_mib->suffix_seq_num;
			break;
		case DCBX_READ_REMOTE_MIB:
			remote_mib   = (struct lldp_remote_mib*) base_mib_addr;
			prefix_seq_num = remote_mib->prefix_seq_num;
			suffix_seq_num = remote_mib->suffix_seq_num;
			break;
		default:
			return 1; /*error*/
		}
	}while((prefix_seq_num != suffix_seq_num)&&
	       (max_try_read < DCBX_LOCAL_MIB_MAX_TRY_READ));

	if(max_try_read >= DCBX_LOCAL_MIB_MAX_TRY_READ) {
		BNX2X_ERR("MIB could not be read\n");
		return 1;
	}

	return 0;
}


static void bnx2x_pfc_set_pfc(struct bnx2x *bp) {
	if (CHIP_IS_E2(bp)) {
		if(BP_PORT(bp)){
			BNX2X_ERR("4 port mode is not supported");
			return;
		}

		if(bp->dcbx_port_params.pfc.enabled)

			/* 1. Fills up common PFC structures if required.*/
			/* 2. Configure NIG, MAC and BRB via the elink:
			 *    elink must first check if BMAC is not in reset
			 *    and only then configures the BMAC
			 *    Or, configure EMAC.
			 */
			bnx2x_pfc_set(bp);

		else
			bnx2x_pfc_clear(bp);
	}
}

static void bnx2x_dcbx_stop_hw_tx(struct bnx2x *bp)
{
	DP(NETIF_MSG_LINK, "sending STOP TRAFFIC\n");
	bnx2x_sp_post(bp, RAMROD_CMD_ID_COMMON_STOP_TRAFFIC,
		      0 /* connectionless */,
		      0 /* dataHi is zero */,
		      0 /* dataLo is zero */,
		      1 /* common */);
}

static void bnx2x_dcbx_resume_hw_tx(struct bnx2x *bp)
{
	bnx2x_pfc_fw_struct_e2(bp);
	DP(NETIF_MSG_LINK, "sending START TRAFFIC\n");
	bnx2x_sp_post(bp, RAMROD_CMD_ID_COMMON_START_TRAFFIC,
		      0, /* connectionless */
		      U64_HI(bnx2x_sp_mapping(bp, pfc_config)),
		      U64_LO(bnx2x_sp_mapping(bp, pfc_config)),
		      1  /* commmon */);
}

static void bnx2x_dcbx_update_ets_params(struct bnx2x *bp)
{
	struct bnx2x_dcbx_pg_params *ets = &(bp->dcbx_port_params.ets);
	u8	status = 0;

	bnx2x_ets_disabled(&bp->link_params);

	if (!ets->enabled)
		return;

	if ((ets->num_of_cos == 0 ) || (ets->num_of_cos > E2_NUM_OF_COS)) {
		BNX2X_ERR("illegal num of cos= %x",ets->num_of_cos);
		return;
	}

	/* valid COS entries */
	if (ets->num_of_cos == 1)   /* no ETS */
		return;

	/* sanity */
	if (((BNX2X_DCBX_COS_NOT_STRICT == ets->cos_params[0].strict)&&
	     (DCBX_INVALID_COS_BW == ets->cos_params[0].bw_tbl)) ||
	    ((BNX2X_DCBX_COS_NOT_STRICT == ets->cos_params[1].strict)&&
	     (DCBX_INVALID_COS_BW == ets->cos_params[1].bw_tbl))) {
		BNX2X_ERR("all COS should have at least bw_limit or strict"
			    "ets->cos_params[0].strict= %x"
			    "ets->cos_params[0].bw_tbl= %x"
			    "ets->cos_params[1].strict= %x"
			    "ets->cos_params[1].bw_tbl= %x",
			  ets->cos_params[0].strict,
			  ets->cos_params[0].bw_tbl,
			  ets->cos_params[1].strict,
			  ets->cos_params[1].bw_tbl);
		return;
	}
	/* If we join a group and there is bw_tbl and strict then bw rules */
	if ((DCBX_INVALID_COS_BW != ets->cos_params[0].bw_tbl) &&
	    (DCBX_INVALID_COS_BW != ets->cos_params[1].bw_tbl)) {
		u32 bw_tbl_0 = ets->cos_params[0].bw_tbl;
		u32 bw_tbl_1 = ets->cos_params[1].bw_tbl;
		/* Do not allow 0-100 configuration
		 * since PBF does not support it
		 * force 1-99 instead
		 */
		if (bw_tbl_0 == 0) {
			bw_tbl_0 = 1;
			bw_tbl_1 = 99;
		} else if (bw_tbl_1 == 0) {
			bw_tbl_1 = 1;
			bw_tbl_0 = 99;
		}

		bnx2x_ets_bw_limit(&bp->link_params, bw_tbl_0, bw_tbl_1);
	} else {
		if (ets->cos_params[0].strict == BNX2X_DCBX_COS_HIGH_STRICT)
			status = bnx2x_ets_strict(&bp->link_params, 0);
		else if (ets->cos_params[1].strict
						== BNX2X_DCBX_COS_HIGH_STRICT)
			status = bnx2x_ets_strict(&bp->link_params, 1);

		if (status)
			BNX2X_ERR("update_ets_params failed\n");
	}
}

void bnx2x_dcbx_set_params(struct bnx2x *bp, u32 state) {
	u32 dcbx_neg_res_offset;

	dcbx_neg_res_offset = SHMEM2_RD(bp,dcbx_neg_res_offset);

	DP(NETIF_MSG_LINK, "dcbx_neg_res_offset 0x%x\n", dcbx_neg_res_offset);

	if (SHMEM_DCBX_NEG_RES_NONE == dcbx_neg_res_offset) {
		BNX2X_ERR("FW doesn't support dcbx_neg_res_offset\n");
		return;
	}

	switch (state) {
	case BNX2X_DCBX_STATE_NEG_RECEIVED:
		{
			struct lldp_local_mib local_mib = {0};
			DP(NETIF_MSG_LINK, "BNX2X_DCBX_STATE_NEG_RECEIVED\n");
			if(bnx2x_dcbx_read_mib(bp, (u32*)&local_mib, dcbx_neg_res_offset,
					       DCBX_READ_LOCAL_MIB))
				return;


			bnx2x_dump_dcbx_drv_param(bp, &local_mib);

			bnx2x_get_dcbx_drv_param(bp, &bp->dcbx_port_params, &local_mib);
			BNX2X_ERR( "dcbx_port_params.ets.enabled %d\n", bp->dcbx_port_params.ets.enabled);

			if (bp->state != BNX2X_STATE_OPENING_WAIT4_LOAD) {
				bnx2x_dcbx_stop_hw_tx(bp);
				return;
			}
			/* fall through */
		}
	case BNX2X_DCBX_STATE_TX_PAUSED:
		DP(NETIF_MSG_LINK, "BNX2X_DCBX_STATE_TX_PAUSED\n");
		BNX2X_ERR( "dcbx_port_params.ets.enabled %d\n", bp->dcbx_port_params.ets.enabled);
		bnx2x_pfc_set_pfc(bp);
		BNX2X_ERR( "dcbx_port_params.ets.enabled %d\n", bp->dcbx_port_params.ets.enabled);

		bnx2x_dcbx_update_ets_params(bp);
		if (bp->state != BNX2X_STATE_OPENING_WAIT4_LOAD){
			bnx2x_dcbx_resume_hw_tx(bp);
			return;
		}
		/* fall through */
	case BNX2X_DCBX_STATE_TX_RELEASED:
		DP(NETIF_MSG_LINK, "BNX2X_DCBX_STATE_TX_RELEASED\n");
		if (bp->state != BNX2X_STATE_OPENING_WAIT4_LOAD)
			bnx2x_fw_command(bp, DRV_MSG_CODE_DCBX_PMF_DRV_OK, 0);

		return;
	default:
		BNX2X_ERR("Unknown DCBX_STATE\n");
	}
}


#define LLDP_STATS_OFFSET(bp) 	 	(BP_PORT(bp)*\
					 sizeof(struct lldp_dcbx_stat))

static void bnx2x_dcbx_lldp_updated_stat(struct bnx2x *bp,
					   u32 dcbx_stat_offset)
{

}

/* calculate struct offset in array according to chip information */
#define LLDP_PARAMS_OFFSET(bp) 	 	(BP_PORT(bp)*sizeof(struct lldp_params))

#define LLDP_ADMIN_MIB_OFFSET(bp)	(PORT_MAX*sizeof(struct lldp_params) + \
				      BP_PORT(bp)*sizeof(struct lldp_admin_mib))

static void bnx2x_dcbx_lldp_updated_params(struct bnx2x *bp,
					   u32 dcbx_lldp_params_offset)
{
	struct lldp_params lldp_params = {0};
	u32 i = 0, *buff = NULL;
	u32 offset = dcbx_lldp_params_offset + LLDP_PARAMS_OFFSET(bp);

	DP(NETIF_MSG_LINK, "lldp_offset 0x%x\n", offset);

	if(bnx2x_is_valid_lldp_params(bp) &&
	   (bp->lldp_config_params.overwrite_settings ==
					BNX2X_DCBX_OVERWRITE_SETTINGS_ENABLE)) {
		/* Read the data first */
		buff = (u32*)&lldp_params;
		for(i = 0; i < sizeof(struct lldp_params); i += 4,  buff++)
			*buff = REG_RD(bp, (offset+ i));

		lldp_params.msg_tx_hold =
			(u8)bp->lldp_config_params.msg_tx_hold;
		lldp_params.msg_fast_tx_interval =
			(u8)bp->lldp_config_params.msg_fast_tx;
		lldp_params.tx_crd_max =
			(u8)bp->lldp_config_params.tx_credit_max;
		lldp_params.msg_tx_interval =
			(u8)bp->lldp_config_params.msg_tx_interval;
		lldp_params.tx_fast =
			(u8)bp->lldp_config_params.tx_fast;

		/* Write the data.*/
		buff = (u32*)&lldp_params;
		for( i = 0; i < sizeof(struct lldp_params); i += 4, buff++)
			REG_WR(bp, (offset+ i) , *buff);


	} else if(BNX2X_DCBX_OVERWRITE_SETTINGS_ENABLE ==
				bp->lldp_config_params.overwrite_settings)
		bp->lldp_config_params.overwrite_settings =
				BNX2X_DCBX_OVERWRITE_SETTINGS_INVALID;
}

static void bnx2x_dcbx_admin_mib_updated_params(struct bnx2x *bp,
				u32 dcbx_lldp_params_offset)
{
	struct lldp_admin_mib admin_mib;
	u32 i, other_traf_type = PREDEFINED_APP_IDX_MAX, traf_type = 0;
	u32 *buff;
	u16 *buff16;
	u32 offset = dcbx_lldp_params_offset + LLDP_ADMIN_MIB_OFFSET(bp);

	/*shortcuts*/
	struct dcbx_features *af = &admin_mib.features;
	struct bnx2x_config_dcbx_params *dp = &bp->dcbx_config_params;

	memset(&admin_mib, 0, sizeof(struct lldp_admin_mib));
	buff = (u32*)&admin_mib;
	/* Read the data first */
	for( i = 0; i < sizeof(struct lldp_admin_mib); i += 4, buff++)
		*buff = REG_RD(bp, (offset+ i));


	if(BNX2X_DCBX_CONFIG_INV_VALUE == dp->admin_dcbx_enable) {
		if(dp->admin_dcbx_enable)
			SET_FLAGS(admin_mib.ver_cfg_flags, DCBX_DCBX_ENABLED);
		else
			RESET_FLAGS(admin_mib.ver_cfg_flags, DCBX_DCBX_ENABLED);
	}

	if(bnx2x_is_valid_dcbx_params(bp) &&
			(BNX2X_DCBX_OVERWRITE_SETTINGS_ENABLE ==
				dp->overwrite_settings)) {
		RESET_FLAGS(admin_mib.ver_cfg_flags,DCBX_CEE_VERSION_MASK);
		admin_mib.ver_cfg_flags |=
			(dp->admin_dcbx_version << DCBX_CEE_VERSION_SHIFT) &
			 DCBX_CEE_VERSION_MASK;

		af->ets.enabled = (u8)dp->admin_ets_enable;

		af->pfc.enabled =(u8)dp->admin_pfc_enable;

		/* FOR IEEE dp->admin_tc_supported_tx_enable */
		if(dp->admin_ets_configuration_tx_enable)
			SET_FLAGS(admin_mib.ver_cfg_flags,
				  DCBX_ETS_CONFIG_TX_ENABLED);
		else
			RESET_FLAGS(admin_mib.ver_cfg_flags,
				    DCBX_ETS_CONFIG_TX_ENABLED);
		/* For IEEE admin_ets_recommendation_tx_enable */
		if(dp->admin_pfc_tx_enable)
			SET_FLAGS(admin_mib.ver_cfg_flags,
				  DCBX_PFC_CONFIG_TX_ENABLED);
		else
			RESET_FLAGS(admin_mib.ver_cfg_flags,
				  DCBX_PFC_CONFIG_TX_ENABLED);

		if(dp->admin_application_priority_tx_enable)
			SET_FLAGS(admin_mib.ver_cfg_flags,
				  DCBX_APP_CONFIG_TX_ENABLED);
		else
			RESET_FLAGS(admin_mib.ver_cfg_flags,
				  DCBX_APP_CONFIG_TX_ENABLED);

		if(dp->admin_ets_willing)
			SET_FLAGS(admin_mib.ver_cfg_flags, DCBX_ETS_WILLING);
		else
			RESET_FLAGS(admin_mib.ver_cfg_flags, DCBX_ETS_WILLING);
		/* For IEEE admin_ets_reco_valid */
		if(dp->admin_pfc_willing)
			SET_FLAGS(admin_mib.ver_cfg_flags, DCBX_PFC_WILLING);
		else
			RESET_FLAGS(admin_mib.ver_cfg_flags, DCBX_PFC_WILLING);

		if(dp->admin_app_priority_willing)
			SET_FLAGS(admin_mib.ver_cfg_flags,DCBX_APP_WILLING);
		else
			RESET_FLAGS(admin_mib.ver_cfg_flags,DCBX_APP_WILLING);

		/*
		 * Arrays that there cell are less than 32 bit are still
		 * in big endian mode.
		 */
		buff = (u32*)&af->ets.pg_bw_tbl[0];
		buff[0] = be32_to_cpu(buff[0]);
		buff[1] = be32_to_cpu(buff[1]);

		for(i = 0 ; i < DCBX_MAX_NUM_PG; i++) {
			af->ets.pg_bw_tbl[i] = (u8)
			dp->admin_configuration_bw_precentage[i];
			DP(NETIF_MSG_LINK, "pg_bw_tbl[%d] = %02x\n",
					    i, af->ets.pg_bw_tbl[i]);
		}

		buff[0] = cpu_to_be32(buff[0]);
		buff[1] = cpu_to_be32(buff[1]);

		/* Arrays that there cell are less than 32 bit are still
		 * in big endian mode.*/
		buff = (u32 *)&af->ets.pri_pg_tbl[0];
		buff[0] = be32_to_cpu(buff[0]);

		for( i = 0; i < DCBX_MAX_NUM_PRI_PG_ENTRIES; i++) {
			u8 val_0, val_1;
			val_0 = (u8)dp->admin_configuration_ets_pg[i*2] & 0xF;
			val_1 = (u8)(dp->admin_configuration_ets_pg[i*2 +1] &
				       0xF) << 4;
			af->ets.pri_pg_tbl[i] = (u8) (val_0 | val_1);
			DP(NETIF_MSG_LINK, "pri_pg_tbl[%d] = %02x\n",
					    i, af->ets.pri_pg_tbl[i]);
		}

		buff[0] = cpu_to_be32(buff[0]);

		/*For IEEE admin_recommendation_bw_precentage
		 *For IEEE admin_recommendation_ets_pg */
		af->pfc.pri_en_bitmap = (u8)dp->admin_pfc_bitmap;
		for( i = 0; i < 4; i++) {
			if(dp->admin_priority_app_table[i].valid) {
				struct bnx2x_admin_priority_app_table *table =
					dp->admin_priority_app_table;
				if((ETH_TYPE_FCOE == table[i].app_id) &&
				   (TRAFFIC_TYPE_ETH == table[i].traffic_type))
					traf_type = FCOE_APP_IDX;
				else if((TCP_PORT_ISCSI == table[i].app_id) &&
				   (TRAFFIC_TYPE_PORT == table[i].traffic_type))
					traf_type = ISCSI_APP_IDX;
				else
					traf_type = other_traf_type++;


				af->app.app_pri_tbl[traf_type].app_id[0] =
					table[i].app_id & 0xff;
				af->app.app_pri_tbl[traf_type].app_id[1] =
					(table[i].app_id >> 8) & 0xff;

				buff16 = (u16*)
				    &af->app.app_pri_tbl[traf_type].app_id[0];
				buff16[0] = cpu_to_be16(buff16[0]);

				af->app.app_pri_tbl[traf_type].pri_bitmap =
					(u8)(1 << table[i].priority);

				af->app.app_pri_tbl[traf_type].appBitfield =
				    (DCBX_APP_ENTRY_VALID);

				af->app.app_pri_tbl[traf_type].appBitfield |=
				   (TRAFFIC_TYPE_ETH == table[i].traffic_type) ?
					DCBX_APP_SF_ETH_TYPE : DCBX_APP_SF_PORT;
			}
		}

		af->app.default_pri = (u8)dp->admin_default_priority;

	} else if(BNX2X_DCBX_OVERWRITE_SETTINGS_ENABLE ==
						dp->overwrite_settings)
		dp->overwrite_settings = BNX2X_DCBX_OVERWRITE_SETTINGS_INVALID;

	/* Write the data. */
	buff = (u32 *)&admin_mib;
	for( i = 0; i < sizeof(struct lldp_admin_mib); i+=4,buff++)
		REG_WR(bp, (offset+ i) , *buff);
}

void bnx2x_dcbx_init_params(struct bnx2x *bp) {
	bp->dcbx_config_params.admin_dcbx_version = 0x0; /* 0 - CEE; 1 - IEEE */
	bp->dcbx_config_params.dcb_enable = 1;
	bp->dcbx_config_params.admin_dcbx_enable = 2;
	bp->dcbx_config_params.admin_ets_willing = 1;
	bp->dcbx_config_params.admin_pfc_willing = 1;
	bp->dcbx_config_params.overwrite_settings = 1;
	bp->dcbx_config_params.admin_ets_enable = 1;
	bp->dcbx_config_params.admin_pfc_enable = 1;
	bp->dcbx_config_params.admin_tc_supported_tx_enable = 1;
	bp->dcbx_config_params.admin_ets_configuration_tx_enable = 1;
	bp->dcbx_config_params.admin_pfc_tx_enable = 1;
	bp->dcbx_config_params.admin_application_priority_tx_enable = 1;
	bp->dcbx_config_params.admin_ets_reco_valid = 1;
	bp->dcbx_config_params.admin_app_priority_willing = 1;
	bp->dcbx_config_params.admin_configuration_bw_precentage[0] = 00;
	bp->dcbx_config_params.admin_configuration_bw_precentage[1] = 50;
	bp->dcbx_config_params.admin_configuration_bw_precentage[2] = 50;
	bp->dcbx_config_params.admin_configuration_bw_precentage[3] = 0;
	bp->dcbx_config_params.admin_configuration_bw_precentage[4] = 0;
	bp->dcbx_config_params.admin_configuration_bw_precentage[5] = 0;
	bp->dcbx_config_params.admin_configuration_bw_precentage[6] = 0;
	bp->dcbx_config_params.admin_configuration_bw_precentage[7] = 0;
	bp->dcbx_config_params.admin_configuration_ets_pg[0] = 1;
	bp->dcbx_config_params.admin_configuration_ets_pg[1] = 0;
	bp->dcbx_config_params.admin_configuration_ets_pg[2] = 0;
	bp->dcbx_config_params.admin_configuration_ets_pg[3] = 2;
	bp->dcbx_config_params.admin_configuration_ets_pg[4] = 0;
	bp->dcbx_config_params.admin_configuration_ets_pg[5] = 0;
	bp->dcbx_config_params.admin_configuration_ets_pg[6] = 0;
	bp->dcbx_config_params.admin_configuration_ets_pg[7] = 0;
	bp->dcbx_config_params.admin_recommendation_bw_precentage[0] = 0;
	bp->dcbx_config_params.admin_recommendation_bw_precentage[1] = 1;
	bp->dcbx_config_params.admin_recommendation_bw_precentage[2] = 2;
	bp->dcbx_config_params.admin_recommendation_bw_precentage[3] = 0;
	bp->dcbx_config_params.admin_recommendation_bw_precentage[4] = 7;
	bp->dcbx_config_params.admin_recommendation_bw_precentage[5] = 5;
	bp->dcbx_config_params.admin_recommendation_bw_precentage[6] = 6;
	bp->dcbx_config_params.admin_recommendation_bw_precentage[7] = 7;
	bp->dcbx_config_params.admin_recommendation_ets_pg[0] = 0;
	bp->dcbx_config_params.admin_recommendation_ets_pg[1] = 1;
	bp->dcbx_config_params.admin_recommendation_ets_pg[2] = 2;
	bp->dcbx_config_params.admin_recommendation_ets_pg[3] = 3;
	bp->dcbx_config_params.admin_recommendation_ets_pg[4] = 4;
	bp->dcbx_config_params.admin_recommendation_ets_pg[5] = 5;
	bp->dcbx_config_params.admin_recommendation_ets_pg[6] = 6;
	bp->dcbx_config_params.admin_recommendation_ets_pg[7] = 7;
	bp->dcbx_config_params.admin_pfc_bitmap = 0x8; /* FCoE(3) enable */
	bp->dcbx_config_params.admin_priority_app_table[0].valid = 1;
	bp->dcbx_config_params.admin_priority_app_table[1].valid = 1;
	bp->dcbx_config_params.admin_priority_app_table[2].valid = 0;
	bp->dcbx_config_params.admin_priority_app_table[3].valid = 0;
	bp->dcbx_config_params.admin_priority_app_table[0].priority = 3;
	bp->dcbx_config_params.admin_priority_app_table[1].priority = 0;
	bp->dcbx_config_params.admin_priority_app_table[2].priority = 0;
	bp->dcbx_config_params.admin_priority_app_table[3].priority = 0;
	bp->dcbx_config_params.admin_priority_app_table[0].traffic_type = 0;
	bp->dcbx_config_params.admin_priority_app_table[1].traffic_type = 1;
	bp->dcbx_config_params.admin_priority_app_table[2].traffic_type = 0;
	bp->dcbx_config_params.admin_priority_app_table[3].traffic_type = 0;
	bp->dcbx_config_params.admin_priority_app_table[0].app_id = 0x8906;
	bp->dcbx_config_params.admin_priority_app_table[1].app_id = 3260;
	bp->dcbx_config_params.admin_priority_app_table[2].app_id = 0;
	bp->dcbx_config_params.admin_priority_app_table[3].app_id = 0;
	bp->dcbx_config_params.admin_default_priority =
		bp->dcbx_config_params.admin_priority_app_table[1].priority;
}

void bnx2x_dcbx_init(struct bnx2x *bp) {
	u32 dcbx_lldp_params_offset = SHMEM_LLDP_DCBX_PARAMS_NONE;
	/* validate:
	 * chip of good for dcbx version,
	 * dcb wanted
	 * the function is pmf
	 * shmem2 contains DCBX support fields
	 */
	DP(NETIF_MSG_LINK, "dcb_enable %d bp->port.pmf %d\n",
	   bp->dcbx_config_params.dcb_enable, bp->port.pmf );

	if((CHIP_IS_E1H(bp) || (CHIP_IS_E2(bp) && !CHIP_MODE_IS_4_PORT(bp))) &&
	   bp->dcbx_config_params.dcb_enable &&
	   bp->port.pmf &&
	   SHMEM2_HAS(bp, dcbx_lldp_params_offset)) {
		dcbx_lldp_params_offset = SHMEM2_RD(bp,dcbx_lldp_params_offset);
		DP(NETIF_MSG_LINK, "dcbx_lldp_params_offset 0x%x\n",
		   dcbx_lldp_params_offset);
		if (SHMEM_LLDP_DCBX_PARAMS_NONE != dcbx_lldp_params_offset) {
			u32 dcbx_stat_offset;
			bnx2x_dcbx_lldp_updated_params(bp,
						       dcbx_lldp_params_offset);
			if(SHMEM2_HAS(bp, dcbx_lldp_dcbx_stat_offset) &&
			   (dcbx_stat_offset =
			    SHMEM2_RD(bp,dcbx_lldp_dcbx_stat_offset)) !=
						SHMEM_LLDP_DCBX_STAT_NONE)
				bnx2x_dcbx_lldp_updated_stat(bp,
							     dcbx_stat_offset);



			bnx2x_dcbx_admin_mib_updated_params(bp,
				dcbx_lldp_params_offset);

			/* set default configuration BC has */
			bnx2x_dcbx_set_params(bp,
					      BNX2X_DCBX_STATE_NEG_RECEIVED);

			bnx2x_fw_command(bp, DRV_MSG_CODE_DCBX_ADMIN_PMF_MSG,0);
		}
	}
}

void bnx2x_dcb_init_intmem_pfc(struct bnx2x *bp)
{
	struct priority_cos pricos[MAX_PFC_TRAFFIC_TYPES];
	u32 i = 0, addr;
	memset(pricos, 0, sizeof(pricos));
	/* Default initialization */
	for (i = 0; i < MAX_PFC_TRAFFIC_TYPES; i++)
		pricos[i].priority = LLFC_TRAFFIC_TYPE_TO_PRIORITY_UNMAPPED;

	/* Store per port struct to internal memory */
	addr = BAR_XSTRORM_INTMEM +
			XSTORM_CMNG_PER_PORT_VARS_OFFSET(BP_PORT(bp)) +
			offsetof(struct cmng_struct_per_port,
				 traffic_type_to_priority_cos);
	__storm_memset_struct(bp, addr, sizeof(pricos), (u32*)pricos);


	/* LLFC disabled.*/
	REG_WR8(bp ,BAR_XSTRORM_INTMEM +
		    XSTORM_CMNG_PER_PORT_VARS_OFFSET(BP_PORT(bp)) +
		    offsetof(struct cmng_struct_per_port, llfc_mode),
			LLFC_MODE_NONE);

	/* DCBX disabled.*/
	REG_WR8(bp ,BAR_XSTRORM_INTMEM +
		    XSTORM_CMNG_PER_PORT_VARS_OFFSET(BP_PORT(bp)) +
		    offsetof(struct cmng_struct_per_port, dcb_enabled),
			DCB_DISABLED);
}


static void
bnx2x_dcbx_print_cos_params(struct bnx2x *bp,
			    struct flow_control_configuration *pfc_fw_cfg)
{
#ifdef BNX2X_DUMP_DCBX
	u8 pri = 0;
	u8 cos = 0;

	BNX2X_ERR("pfc_fw_cfg->dcb_version %x\n",pfc_fw_cfg->dcb_version);
	BNX2X_ERR("pdev->params.dcbx_port_params.pfc."
		  "priority_non_pauseable_mask %x\n",
		   bp->dcbx_port_params.pfc.priority_non_pauseable_mask);

	for( cos =0 ; cos < bp->dcbx_port_params.ets.num_of_cos ; cos++) {
		BNX2X_ERR("pdev->params.dcbx_port_params.ets."
			  "cos_params[%d].pri_bitmask %x\n",cos,
			  bp->dcbx_port_params.ets.cos_params[cos].pri_bitmask);

		BNX2X_ERR("pdev->params.dcbx_port_params.ets."
			  "cos_params[%d].bw_tbl %x\n",cos,
			  bp->dcbx_port_params.ets.cos_params[cos].bw_tbl);

		BNX2X_ERR("pdev->params.dcbx_port_params.ets."
			  "cos_params[%d].strict %x\n",cos,
			  bp->dcbx_port_params.ets.cos_params[cos].strict);

		BNX2X_ERR("pdev->params.dcbx_port_params.ets."
			  "cos_params[%d].pauseable %x\n",cos,
			  bp->dcbx_port_params.ets.cos_params[cos].pauseable);
	}

	for (pri = 0; pri < LLFC_DRIVER_TRAFFIC_TYPE_MAX; pri++) {
		BNX2X_ERR("pfc_fw_cfg->traffic_type_to_priority_cos[%d]."
			  "priority %x\n",pri,
			pfc_fw_cfg->traffic_type_to_priority_cos[pri].priority);

		BNX2X_ERR("pfc_fw_cfg->traffic_type_to_priority_cos[%d]."
			  "cos %x\n",pri,
			pfc_fw_cfg->traffic_type_to_priority_cos[pri].cos);
	}
#endif
}

/* helper: read len bytes from addr into buff by REG_RD */
static inline void bnx2x_read_data(struct bnx2x *bp, u32* buff,
				   u32 addr, u32 len)
{
	int i;
	for( i = 0 ;i < len; i += 4, buff++)
		*buff = REG_RD(bp, (addr + i));
}

int bnx2x_dcb_get_lldp_params_ioctl(struct bnx2x *bp, void *uaddr)
{
	struct bnx2x_lldp_params_get	lldp_params;
	struct lldp_params		mcp_lldp_params;
	struct lldp_dcbx_stat		lldp_dcbx_stat;
	int i;
	u32 offset, stat_offset;

	if (copy_from_user(&lldp_params, uaddr,
			   sizeof(struct bnx2x_lldp_params_get)))
		return -EFAULT;

	if (lldp_params.ver_num != LLDP_PARAMS_VER_NUM)
		return -EINVAL; /* incorrect version */

	if(CHIP_IS_E1(bp) || !(SHMEM2_HAS(bp, dcbx_lldp_params_offset)) ||
			     !(SHMEM2_HAS(bp, dcbx_lldp_dcbx_stat_offset)))
		return -EINVAL;  /* unsupported feature */

	offset = SHMEM2_RD(bp, dcbx_lldp_params_offset);
	stat_offset = SHMEM2_RD(bp, dcbx_lldp_dcbx_stat_offset);
	if (offset == SHMEM_LLDP_DCBX_PARAMS_NONE ||
	    stat_offset == SHMEM_LLDP_DCBX_STAT_NONE)
		return -EINVAL; /* feature not configured */

	memset(&lldp_params, 0, sizeof(struct bnx2x_lldp_params_get));

	lldp_params.ver_num = LLDP_PARAMS_VER_NUM;
	lldp_params.config_lldp_params.overwrite_settings =
			bp->lldp_config_params.overwrite_settings;

	offset += LLDP_PARAMS_OFFSET(bp);
	stat_offset += LLDP_STATS_OFFSET(bp);

	/* handle the lldp params */
	bnx2x_read_data(bp, (u32*)&mcp_lldp_params, offset,
			 sizeof(struct lldp_params));

	lldp_params.config_lldp_params.msg_tx_hold
				= mcp_lldp_params.msg_tx_hold;
	lldp_params.config_lldp_params.msg_fast_tx
				= mcp_lldp_params.msg_fast_tx_interval;
	lldp_params.config_lldp_params.tx_credit_max
				= mcp_lldp_params.tx_crd_max;
	lldp_params.config_lldp_params.msg_tx_interval
				= mcp_lldp_params.msg_tx_interval;
	lldp_params.config_lldp_params.tx_fast
				= mcp_lldp_params.tx_fast;

	for(i = 0; i < REM_CHASSIS_ID_STAT_LEN; i++)
		lldp_params.remote_chassis_id[i] =
			mcp_lldp_params.peer_chassis_id[i];

	for(i = 0; i < REM_PORT_ID_STAT_LEN; i++)
		lldp_params.remote_port_id[i] =
			mcp_lldp_params.peer_port_id[i];

	lldp_params.admin_status = mcp_lldp_params.admin_status;


	/* handle the stats */
	bnx2x_read_data(bp, (u32*)&lldp_dcbx_stat, stat_offset,
			 sizeof(struct lldp_dcbx_stat));

	for( i = 0; i < LOCAL_CHASSIS_ID_STAT_LEN; i++ )
		lldp_params.local_chassis_id[i] =
					lldp_dcbx_stat.local_chassis_id[i];
	for( i = 0; i < LOCAL_CHASSIS_ID_STAT_LEN; i++ )
		lldp_params.local_chassis_id[i] =
					lldp_dcbx_stat.local_chassis_id[i];
	if (copy_to_user(uaddr, &lldp_params,
			 sizeof(struct bnx2x_lldp_params_get)))
		return -EFAULT;
	DP(NETIF_MSG_LINK, "get_lldp_params done (%d)\n",
			(int)sizeof(struct bnx2x_lldp_params_get));
	return 0;

}

/* fills help_data according to pg_info */
static void bnx2x_dcbx_get_num_pg_traf_type(struct bnx2x *bp,
					    u32 *pg_pri_orginal_spread,
					    struct pg_help_data *help_data)
{
	bool pg_found  = false;
	u32 i, traf_type, add_traf_type, add_pg;
	u32 *ttp = bp->dcbx_port_params.app.traffic_type_priority;
	struct pg_entry_help_data *data = help_data->data; /*shotcut*/

	/* Set to invalid */
	for (i = 0; i < LLFC_DRIVER_TRAFFIC_TYPE_MAX; i++)
		data[i].pg = DCBX_ILLEGAL_PG;

	for (add_traf_type = 0;
	     add_traf_type < LLFC_DRIVER_TRAFFIC_TYPE_MAX; add_traf_type++) {
		pg_found = false;
		if (ttp[add_traf_type] < MAX_PFC_PRIORITIES) {
			add_pg = (u8)pg_pri_orginal_spread[ttp[add_traf_type]];
			for (traf_type = 0;
			     traf_type < LLFC_DRIVER_TRAFFIC_TYPE_MAX;
			     traf_type++) {
				if (data[traf_type].pg == add_pg) {
					if (!(data[traf_type].pg_priority &
					     (1 << ttp[add_traf_type])))
						data[traf_type].
							num_of_dif_pri++;
					data[traf_type].pg_priority |=
						(1 << ttp[add_traf_type]);
					pg_found = true;
					break;
				}
			}
			if (false == pg_found) {
				data[help_data->num_of_pg].pg = add_pg;
				data[help_data->num_of_pg].pg_priority =
						(1 << ttp[add_traf_type]);
				data[help_data->num_of_pg].num_of_dif_pri = 1;
				help_data->num_of_pg++;
			}
		}
		DP(NETIF_MSG_LINK,"add_traf_type %d pg_found %s num_of_pg %d\n",
				add_traf_type,
				(false == pg_found) ? "NO": "YES",
				help_data->num_of_pg);
	}
}


/*******************************************************************************
 * Description: single priority group
 *
 * Return:
 ******************************************************************************/
static void bnx2x_dcbx_ets_disabled_entry_data(struct bnx2x *bp,
					       struct cos_help_data *cos_data,
					       u32 pri_join_mask)
{
	/* Only one priority than only one COS */
	cos_data->data[0].pausable =
		IS_DCBX_PFC_PRI_ONLY_PAUSE(bp, pri_join_mask);
	cos_data->data[0].pri_join_mask = pri_join_mask;
	cos_data->data[0].cos_bw = 100;
	cos_data->num_of_cos = 1;
}

/*******************************************************************************
 * Description: updating the cos bw
 *
 * Return:
 ******************************************************************************/
static inline void bnx2x_dcbx_add_to_cos_bw(struct bnx2x *bp,
					    struct cos_entry_help_data *data,
					    u8 pg_bw)
{
	if (data->cos_bw == DCBX_INVALID_COS_BW)
		data->cos_bw = pg_bw;
	else
		data->cos_bw += pg_bw;
}

/*******************************************************************************
 * Description: single priority group
 *
 * Return:
 ******************************************************************************/
static void bnx2x_dcbx_separate_pauseable_from_non(struct bnx2x *bp,
						   struct cos_help_data *cos_data,
						   u32 *pg_pri_orginal_spread,
						   struct dcbx_ets_feature *ets)
{
	u32	pri_tested	= 0;
	u8	i		= 0;
	u8	entry		= 0;
	u8	pg_entry	= 0;
	u8	num_of_pri	= LLFC_DRIVER_TRAFFIC_TYPE_MAX;

	cos_data->data[0].pausable = true;
	cos_data->data[1].pausable = false;
	cos_data->data[0].pri_join_mask = cos_data->data[1].pri_join_mask = 0;

	for (i=0 ; i < num_of_pri ; i++) {
		pri_tested = 1 << bp->dcbx_port_params.
					app.traffic_type_priority[i];

		if (pri_tested & DCBX_PFC_PRI_NON_PAUSE_MASK(bp)) {
			cos_data->data[1].pri_join_mask |= pri_tested;
			entry = 1;
		} else {
			cos_data->data[0].pri_join_mask |= pri_tested;
			entry = 0;
		}
		pg_entry = (u8)pg_pri_orginal_spread[bp->dcbx_port_params.
						app.traffic_type_priority[i]];
		/* There can be only one strict pg */
		if ( pg_entry < DCBX_MAX_NUM_PG)
			bnx2x_dcbx_add_to_cos_bw(bp, &cos_data->data[entry],
						 ets->pg_bw_tbl[pg_entry]);
		else
			/* If we join a group and one is strict
			 * than the bw rulls */
			cos_data->data[entry].strict =
						BNX2X_DCBX_COS_HIGH_STRICT;
	}
	if ((0 == cos_data->data[0].pri_join_mask) &&
	    (0 == cos_data->data[1].pri_join_mask))
		BNX2X_ERR("dcbx error: Both groups must have priorities\n");
}


#ifndef POWER_OF_2
#define POWER_OF_2(x)	((0 != x) && (0 == (x &(x-1))))
#endif

static void bxn2x_dcbx_single_pg_to_cos_params(struct bnx2x *bp,
					      struct pg_help_data *pg_help_data,
					      struct cos_help_data *cos_data,
					      u32 pri_join_mask,
					      u8 num_of_dif_pri)
{
	u8 i = 0;
	u32 pri_tested = 0;
	u32 pri_mask_without_pri = 0;
	u32 *ttp = bp->dcbx_port_params.app.traffic_type_priority;
	/*debug*/
	if (num_of_dif_pri == 1) {
		bnx2x_dcbx_ets_disabled_entry_data(bp, cos_data, pri_join_mask);
		return;
	}
	/* single priority group */
	if ( pg_help_data->data[0].pg < DCBX_MAX_NUM_PG) {
		/* If there are both pauseable and non-pauseable priorities,
		 * the pauseable priorities go to the first queue and
		 * the non-pauseable priorities go to the second queue.
		 */
		if (IS_DCBX_PFC_PRI_MIX_PAUSE(bp, pri_join_mask)) {
			/* Pauseable */
			cos_data->data[0].pausable= true;
			/* Non pauseable.*/
			cos_data->data[1].pausable = false;

			if (2 == num_of_dif_pri) {
				cos_data->data[0].cos_bw = 50;
				cos_data->data[1].cos_bw = 50;
			}

			if (3 == num_of_dif_pri) {
				if (POWER_OF_2(DCBX_PFC_PRI_GET_PAUSE(bp,
							pri_join_mask))) {
					cos_data->data[0].cos_bw = 33;
					cos_data->data[1].cos_bw = 67;
				} else {
					cos_data->data[0].cos_bw = 67;
					cos_data->data[1].cos_bw = 33;
				}
			}

		} else if (IS_DCBX_PFC_PRI_ONLY_PAUSE(bp, pri_join_mask)) {
			/* If there are only pauseable priorities,
			 * then one/two priorities go to the first queue
			 * and one priority goes to the second queue.
			 */
			if (2 == num_of_dif_pri) {
				cos_data->data[0].cos_bw = 50;
				cos_data->data[1].cos_bw = 50;
			} else {
				cos_data->data[0].cos_bw = 67;
				cos_data->data[1].cos_bw = 33;
			}
			cos_data->data[1].pausable = true;
			cos_data->data[0].pausable = true;
			/* All priorities except FCOE */
			cos_data->data[0].pri_join_mask = (pri_join_mask &
				((u8)~(1 << ttp[LLFC_TRAFFIC_TYPE_FCOE])));
			/* Only FCOE priority.*/
			cos_data->data[1].pri_join_mask =
				(1 << ttp[LLFC_TRAFFIC_TYPE_FCOE]);
		} else
			/* If there are only non-pauseable priorities,
			 * they will all go to the same queue.
			 */
			bnx2x_dcbx_ets_disabled_entry_data(bp,
						cos_data,pri_join_mask);
	} else {
		/* priority group which is not BW limited (PG#15):*/
		if (IS_DCBX_PFC_PRI_MIX_PAUSE(bp, pri_join_mask)) {
			/* If there are both pauseable and non-pauseable
			 * priorities, the pauseable priorities go to the first
			 * queue and the non-pauseable priorities
			 * go to the second queue.
			 */
			if (DCBX_PFC_PRI_GET_PAUSE(bp,pri_join_mask) >
			    DCBX_PFC_PRI_GET_NON_PAUSE(bp,pri_join_mask)) {
				cos_data->data[0].strict =
					BNX2X_DCBX_COS_HIGH_STRICT;
				cos_data->data[1].strict =
					BNX2X_DCBX_COS_LOW_STRICT;
			} else {
				cos_data->data[0].strict =
					BNX2X_DCBX_COS_LOW_STRICT;
				cos_data->data[1].strict =
					BNX2X_DCBX_COS_HIGH_STRICT;
			}
			/* Pauseable */
			cos_data->data[0].pausable = true;
			/* Non pause-able.*/
			cos_data->data[1].pausable = false;
		} else {
			/* If there are only pauseable priorities or
			 * only non-pauseable,* the lower priorities go
			 * to the first queue and the higherpriorities go
			 * to the second queue.
			 */
			cos_data->data[0].pausable =
				cos_data->data[1].pausable =
				IS_DCBX_PFC_PRI_ONLY_PAUSE(bp,pri_join_mask);

			for (i=0 ; i < LLFC_DRIVER_TRAFFIC_TYPE_MAX; i++) {
				pri_tested = 1 << bp->dcbx_port_params.
					app.traffic_type_priority[i];
				/* Remove priority tested */
				pri_mask_without_pri =
					(pri_join_mask & ((u8)(~pri_tested)));
				if (pri_mask_without_pri < pri_tested)
					break;
			}

			if (i == LLFC_DRIVER_TRAFFIC_TYPE_MAX)
				BNX2X_ERR("Invalid value for pri_join_mask -"
					  " could not find a priority \n");

			cos_data->data[0].pri_join_mask = pri_mask_without_pri;
			cos_data->data[1].pri_join_mask = pri_tested;
			/* Both queues are strict priority,
			 * and that with the highest priority
			 * gets the highest strict priority in the arbiter.
			 */
			cos_data->data[0].strict = BNX2X_DCBX_COS_LOW_STRICT;
			cos_data->data[1].strict = BNX2X_DCBX_COS_HIGH_STRICT;
		}
	}
}

static void bnx2x_dcbx_two_pg_to_cos_params(
			    struct bnx2x 		*bp,
			    struct  pg_help_data 	*pg_help_data,
			    struct dcbx_ets_feature 	*ets,
			    struct cos_help_data 	*cos_data,
			    u32 			*pg_pri_orginal_spread,
			    u32				pri_join_mask,
			    u8				num_of_dif_pri)
{
	u8 i = 0;
	u8 pg[E2_NUM_OF_COS] = {0};

	/* If there are both pauseable and non-pauseable priorities,
	 * the pauseable priorities go to the first queue and
	 * the non-pauseable priorities go to the second queue.
	 */
	if (IS_DCBX_PFC_PRI_MIX_PAUSE(bp, pri_join_mask)) {
		if (IS_DCBX_PFC_PRI_MIX_PAUSE(bp,
					 pg_help_data->data[0].pg_priority) ||
		    IS_DCBX_PFC_PRI_MIX_PAUSE(bp,
					 pg_help_data->data[1].pg_priority)) {
			/* If one PG contains both pauseable and
			 * non-pauseable priorities then ETS is disabled.
			 */
			bnx2x_dcbx_separate_pauseable_from_non(bp, cos_data,
					pg_pri_orginal_spread, ets);
			bp->dcbx_port_params.ets.enabled = false;
			return;
		}

		/* Pauseable */
		cos_data->data[0].pausable = true;
		/* Non pauseable. */
		cos_data->data[1].pausable = false;
		if (IS_DCBX_PFC_PRI_ONLY_PAUSE(bp, pg_help_data->data[0].pg_priority)) {
			/* 0 is pauseable */
			cos_data->data[0].pri_join_mask =
				pg_help_data->data[0].pg_priority;
			pg[0] = pg_help_data->data[0].pg;
			cos_data->data[1].pri_join_mask =
				pg_help_data->data[1].pg_priority;
			pg[1] = pg_help_data->data[1].pg;
		} else {/* 1 is pauseable */
			cos_data->data[0].pri_join_mask =
				pg_help_data->data[1].pg_priority;
			pg[0] = pg_help_data->data[1].pg;
			cos_data->data[1].pri_join_mask =
				pg_help_data->data[0].pg_priority;
			pg[1] = pg_help_data->data[0].pg;
		}
	} else {
		/* If there are only pauseable priorities or
		 * only non-pauseable, each PG goes to a queue.
		 */
		cos_data->data[0].pausable = cos_data->data[1].pausable =
			IS_DCBX_PFC_PRI_ONLY_PAUSE(bp,pri_join_mask);
		cos_data->data[0].pri_join_mask =
			pg_help_data->data[0].pg_priority;
		pg[0] = pg_help_data->data[0].pg;
		cos_data->data[1].pri_join_mask =
			pg_help_data->data[1].pg_priority;
		pg[1] = pg_help_data->data[1].pg;
	}

	/* There can be only one strict pg */
	for (i=0 ; i < E2_NUM_OF_COS; i++) {
		if ( pg[i] < DCBX_MAX_NUM_PG)
			cos_data->data[i].cos_bw =  ets->pg_bw_tbl[pg[i]];
		else
			cos_data->data[i].strict = BNX2X_DCBX_COS_HIGH_STRICT;
	}
}

/*******************************************************************************
 * Description: Still
 *
 * Return:
 ******************************************************************************/
static void bnx2x_dcbx_three_pg_to_cos_params(
			      struct bnx2x		*bp,
			      struct pg_help_data	*pg_help_data,
			      struct dcbx_ets_feature	*ets,
			      struct cos_help_data	*cos_data,
			      u32			*pg_pri_orginal_spread,
			      u32			pri_join_mask,
			      u8			num_of_dif_pri)
{
	u8 i = 0;
	u32 pri_tested = 0;
	u8 entry = 0;
	u8 pg_entry = 0;
	bool b_found_strict = false;
	u8 num_of_pri = LLFC_DRIVER_TRAFFIC_TYPE_MAX;

	cos_data->data[0].pri_join_mask = cos_data->data[1].pri_join_mask = 0;
	/* If there are both pauseable and non-pauseable priorities,
	 * the pauseable priorities go to the first queue and the
	 * non-pauseable priorities go to the second queue.
	 */
	if (IS_DCBX_PFC_PRI_MIX_PAUSE(bp, pri_join_mask))
		bnx2x_dcbx_separate_pauseable_from_non(bp,
				cos_data, pg_pri_orginal_spread,ets);
	else {
		/* If two BW-limited PG-s were combined to one queue,
		 * the BW is their sum.
		 *
		 * If there are only pauseable priorities or only non-pauseable,
		 * and there are both BW-limited and non-BW-limited PG-s,
		 * the BW-limited PG/s go to one queue and the non-BW-limited
		 * PG/s go to the second queue.
		 *
		 * If there are only pauseable priorities or only non-pauseable
		 * and all are BW limited, then	two priorities go to the first
		 * queue and one priority goes to the second queue.
		 *
		 * We will join this two cases:
		 * if one is BW limited it will go to the secoend queue
		 * otherwise the last priority will get it
		 */

		cos_data->data[0].pausable = cos_data->data[1].pausable =
			IS_DCBX_PFC_PRI_ONLY_PAUSE(bp,pri_join_mask);

		for (i=0 ; i < num_of_pri; i++) {
			pri_tested = 1 << bp->dcbx_port_params.
				app.traffic_type_priority[i];
			pg_entry = (u8)pg_pri_orginal_spread[bp->
				dcbx_port_params.app.traffic_type_priority[i]];

			if (pg_entry < DCBX_MAX_NUM_PG) {
				entry = 0;

				if (i == (num_of_pri-1) &&
				    false == b_found_strict)
					/* last entry will be handled separately
					 * If no priority is strict than last
					 * enty goes to last queue.*/
					entry = 1;
				cos_data->data[entry].pri_join_mask |=
								pri_tested;
				bnx2x_dcbx_add_to_cos_bw(bp,
					&cos_data->data[entry],
					ets->pg_bw_tbl[pg_entry]);
			} else {
				b_found_strict = true;
				cos_data->data[1].pri_join_mask |= pri_tested;
				/* If we join a group and one is strict
				 * than the bw rulls */
				cos_data->data[1].strict =
					BNX2X_DCBX_COS_HIGH_STRICT;
			}
		}
	}
}


static void bnx2x_dcbx_fill_cos_params(struct bnx2x *bp,
				       struct pg_help_data *help_data,
				       struct dcbx_ets_feature *ets,
				       u32 *pg_pri_orginal_spread)
{
	struct cos_help_data         cos_data ;
	u8                    i                           = 0;
	u32                   pri_join_mask               = 0;
	u8                    num_of_dif_pri              = 0;

	memset(&cos_data, 0, sizeof(cos_data));
	/* Validate the pg value */
	for ( i = 0; i < help_data->num_of_pg ; i++) {
		if (DCBX_STRICT_PRIORITY != help_data->data[i].pg &&
		    DCBX_MAX_NUM_PG <= help_data->data[i].pg)
			BNX2X_ERR("Invalid pg[%d] data %x\n",i,
				  help_data->data[i].pg);
		pri_join_mask   |=  help_data->data[i].pg_priority;
		num_of_dif_pri  += help_data->data[i].num_of_dif_pri;
	}

	/* default settings */
	cos_data.num_of_cos = 2;
	for ( i = 0; i < E2_NUM_OF_COS ; i++) {
		cos_data.data[i].pri_join_mask    = pri_join_mask;
		cos_data.data[i].pausable         = false;
		cos_data.data[i].strict           = BNX2X_DCBX_COS_NOT_STRICT;
		cos_data.data[i].cos_bw           = DCBX_INVALID_COS_BW;
	}

	switch (help_data->num_of_pg) {
	case 1:

		bxn2x_dcbx_single_pg_to_cos_params(
					       bp,
					       help_data,
					       &cos_data,
					       pri_join_mask,
					       num_of_dif_pri);
		break;
	case 2:
		bnx2x_dcbx_two_pg_to_cos_params(
					    bp,
					    help_data,
					    ets,
					    &cos_data,
					    pg_pri_orginal_spread,
					    pri_join_mask,
					    num_of_dif_pri);
		break;

	case 3:
		bnx2x_dcbx_three_pg_to_cos_params(
					      bp,
					      help_data,
					      ets,
					      &cos_data,
					      pg_pri_orginal_spread,
					      pri_join_mask,
					      num_of_dif_pri);

		break;
	default:
		BNX2X_ERR("Wrong pg_help_data.num_of_pg\n");
		bnx2x_dcbx_ets_disabled_entry_data(bp,&cos_data,pri_join_mask);
	}

	for (i=0; i < cos_data.num_of_cos ; i++) {
		struct bnx2x_dcbx_cos_params *params =
			&bp->dcbx_port_params.ets.cos_params[i];

		params->pauseable = cos_data.data[i].pausable;
		params->strict = cos_data.data[i].strict;
		params->bw_tbl = cos_data.data[i].cos_bw;
		if (params->pauseable){
			params->pri_bitmask =
			DCBX_PFC_PRI_GET_PAUSE(bp,
					cos_data.data[i].pri_join_mask);
			DP(NETIF_MSG_LINK, "COS %d PAUSABLE prijoinmask 0x%x\n",
				  i, cos_data.data[i].pri_join_mask);
		} else {
			params->pri_bitmask =
			DCBX_PFC_PRI_GET_NON_PAUSE(bp,
					cos_data.data[i].pri_join_mask);
			DP(NETIF_MSG_LINK, "COS %d NONPAUSABLE prijoinmask "
					  "0x%x\n",
				  i, cos_data.data[i].pri_join_mask);
		}
	}

	bp->dcbx_port_params.ets.num_of_cos = cos_data.num_of_cos ;
}

static void bnx2x_dcbx_get_ets_pri_pg_tbl(struct bnx2x *bp,
				u32 * set_configuration_ets_pg,
				u8 * mcp_pri_pg_tbl)
{
	int i;
	u32 *buff;
	int size_in_bytes = DCBX_MAX_NUM_PRI_PG_ENTRIES;
	int size_in_dwords = size_in_bytes/sizeof(u32);

	/* convert */
	buff = (u32 *)&mcp_pri_pg_tbl[0];
	for( i = 0; i < size_in_dwords; i++)
		buff[i] = be32_to_cpu(buff[i]);

	/* Nibble handling */
	for(i=0; i < DCBX_MAX_NUM_PRI_PG_ENTRIES*2; i++) {
		if(0 == (i % 2))
			set_configuration_ets_pg[i] =
					mcp_pri_pg_tbl[i/2] & (0xF);
		else
			set_configuration_ets_pg[i] =
					(mcp_pri_pg_tbl[i/2] >> 4) & (0xF);
		DP(NETIF_MSG_LINK,"set_configuration_ets_pg[%d] = 0x%x\n",
		   i, set_configuration_ets_pg[i]);
	}
}

static void bnx2x_dcbx_get_bw_precentage_tbl(struct bnx2x *bp,
				u32 * set_configuration_bw,
				u8 * mcp_pg_bw_tbl) {

	int i;
	u32 *buff;
	int size_in_bytes = DCBX_MAX_NUM_PG;
	int size_in_dwords = size_in_bytes/sizeof(u32);

	/* convert */
	buff = (u32 *)&mcp_pg_bw_tbl[0];
	for( i = 0; i < size_in_dwords ;i++)
		buff[i] = be32_to_cpu(buff[i]);

	for( i = 0 ;i < size_in_bytes ; i++)
		set_configuration_bw[i] = mcp_pg_bw_tbl[i];
}

static void bnx2x_dcbx_get_priority_app_table(struct bnx2x *bp,
		struct bnx2x_admin_priority_app_table * set_priority_app,
		struct dcbx_app_priority_entry * mcp_array)
{
	int i;
	u16 *buff16;

	for(i = 0 ;i < DCBX_MAX_APP_PROTOCOL; i++) {
		set_priority_app[i].valid =
		   GET_FLAGS(mcp_array[i].appBitfield, DCBX_APP_ENTRY_VALID) ?
					   true : false;

		if(GET_FLAGS(mcp_array[i].appBitfield,DCBX_APP_SF_ETH_TYPE))
			set_priority_app[i].traffic_type = TRAFFIC_TYPE_ETH;
		else
			set_priority_app[i].traffic_type = TRAFFIC_TYPE_PORT;

		set_priority_app[i].priority = mcp_array[i].pri_bitmap;

		/* Arrays that there cell are less than 32 bit are still
		 * in big endian mode. */
		buff16 = (u16 *)&mcp_array[i].app_id[0];
		buff16[0] = be16_to_cpu(buff16[0]);
		set_priority_app[i].app_id = (mcp_array[i].app_id[0]) |
					     (mcp_array[i].app_id[1] << 8);
	}
}

int bnx2x_dcb_set_dcbx_params_ioctl(struct bnx2x *bp, void *uaddr)
{
	struct bnx2x_dcbx_params_set dcbx_params;
	int rc = 0;

	if (bp->recovery_state != BNX2X_RECOVERY_DONE) {
		netdev_err(bp->dev, "Handling parity error recovery. "
				"Try again later\n");
		return -EAGAIN;
	}

	if(copy_from_user(&dcbx_params, uaddr,
			  sizeof(struct bnx2x_dcbx_params_set)))
		return -EFAULT;

	if (dcbx_params.ver_num != DCBX_PARAMS_VER_NUM)
		return -EINVAL; /* incorrect version */

	memcpy(&bp->dcbx_config_params, &dcbx_params.config_dcbx_params,
		sizeof(struct bnx2x_config_dcbx_params));

	if (netif_running(bp->dev)) {
		bnx2x_nic_unload(bp, UNLOAD_NORMAL);
		rc = bnx2x_nic_load(bp, LOAD_NORMAL);
	}
	DP(NETIF_MSG_LINK, "set_dcbx_params done (%d)\n", rc);
	return rc;
}

int bnx2x_dcb_get_dcbx_params_ioctl(struct bnx2x *bp, void *uaddr){
	struct bnx2x_dcbx_params_get	*dcbx_params;
	int i, rc = 0;
	struct lldp_admin_mib		admin_mib;
	struct lldp_dcbx_stat		lldp_dcbx_stat;
	u32 *buff,  offset, stat_offset;

	/* E3.0 might be 4...not supported in current shmem */
	if (2 != PORT_MAX ) {
		BNX2X_ERR("Not supported port configuration");
		return -EINVAL;
	}

	dcbx_params = kmalloc(sizeof(struct bnx2x_dcbx_params_get), GFP_KERNEL);

	if (!dcbx_params) {
		BNX2X_ERR("Can't allocate memory\n");
		return -ENOMEM;
	}

	if (copy_from_user(dcbx_params, uaddr,
			   sizeof(struct bnx2x_dcbx_params_get)))
		return -EFAULT;

	if (dcbx_params->ver_num != DCBX_PARAMS_VER_NUM)
		return -EINVAL; /* incorrect version */

	/* chip or MCP does not support dcbx */
	if(CHIP_IS_E1(bp) || !(SHMEM2_HAS(bp, dcbx_lldp_params_offset))
			  || !(SHMEM2_HAS(bp, dcbx_lldp_dcbx_stat_offset)))
		return -EINVAL; /* unsupported feature */

	stat_offset = SHMEM2_RD(bp, dcbx_lldp_dcbx_stat_offset);
	offset = SHMEM2_RD(bp,dcbx_lldp_params_offset);

	if (offset == SHMEM_LLDP_DCBX_PARAMS_NONE ||
	    stat_offset == SHMEM_LLDP_DCBX_STAT_NONE){
		BNX2X_ERR("DCBX not supported by BC");
		return -EINVAL;
	}


	memset(dcbx_params, 0, sizeof(struct bnx2x_dcbx_params_get));

	dcbx_params->ver_num = LLDP_PARAMS_VER_NUM;

	dcbx_params->config_dcbx_params.overwrite_settings =
		bp->dcbx_config_params.overwrite_settings;

	dcbx_params->config_dcbx_params.dcb_enable =
		bp->dcbx_config_params.dcb_enable;

	offset += LLDP_ADMIN_MIB_OFFSET(bp);

	/* Read the data first */
	buff = (u32 *)&admin_mib;
	for(i = 0 ; i < sizeof(struct lldp_admin_mib); i += 4, buff++)
		*buff = REG_RD(bp,(offset + i));

	dcbx_params->config_dcbx_params.admin_dcbx_enable =
		GET_FLAGS(admin_mib.ver_cfg_flags, DCBX_DCBX_ENABLED) ?
		true : false;

	dcbx_params->config_dcbx_params.admin_dcbx_version =
			(admin_mib.ver_cfg_flags & DCBX_CEE_VERSION_MASK )
						>> DCBX_CEE_VERSION_SHIFT;

	dcbx_params->config_dcbx_params.admin_ets_enable =
					admin_mib.features.ets.enabled;

	dcbx_params->config_dcbx_params.admin_pfc_enable =
					admin_mib.features.pfc.enabled;

	/* FOR IEEE bp->dcbx_config_params.admin_tc_supported_tx_enable */
	dcbx_params->config_dcbx_params.admin_ets_configuration_tx_enable =
		GET_FLAGS(admin_mib.ver_cfg_flags, DCBX_ETS_CONFIG_TX_ENABLED) ?
		true : false;

	/* For IEEE admin_ets_recommendation_tx_enable */

	dcbx_params->config_dcbx_params.admin_pfc_tx_enable =
		GET_FLAGS(admin_mib.ver_cfg_flags, DCBX_PFC_CONFIG_TX_ENABLED) ?
		true : false;

	dcbx_params->config_dcbx_params.admin_application_priority_tx_enable =
		GET_FLAGS(admin_mib.ver_cfg_flags, DCBX_APP_CONFIG_TX_ENABLED) ?
		true : false;

	dcbx_params->config_dcbx_params.admin_ets_willing =
		GET_FLAGS(admin_mib.ver_cfg_flags,DCBX_ETS_WILLING) ?
		true : false;

	/* For IEEE admin_ets_reco_valid */
	dcbx_params->config_dcbx_params.admin_pfc_willing =
		GET_FLAGS(admin_mib.ver_cfg_flags,DCBX_PFC_WILLING) ?
		true : false;

	dcbx_params->config_dcbx_params.admin_app_priority_willing =
		GET_FLAGS(admin_mib.ver_cfg_flags,DCBX_APP_WILLING) ?
		true : false;

	bnx2x_dcbx_get_bw_precentage_tbl(bp,
	   dcbx_params->config_dcbx_params.admin_configuration_bw_precentage,
	   admin_mib.features.ets.pg_bw_tbl);

	bnx2x_dcbx_get_ets_pri_pg_tbl(bp,
	   dcbx_params->config_dcbx_params.admin_configuration_ets_pg,
	   admin_mib.features.ets.pri_pg_tbl);


	/* For IEEE admin_recommendation_bw_precentage
	 * For IEEE admin_recommendation_ets_pg */
	dcbx_params->config_dcbx_params.admin_pfc_bitmap =
			admin_mib.features.pfc.pri_en_bitmap;

	bnx2x_dcbx_get_priority_app_table(bp,
	   dcbx_params->config_dcbx_params.admin_priority_app_table,
	   admin_mib.features.app.app_pri_tbl);

	dcbx_params->config_dcbx_params.admin_default_priority =
			admin_mib.features.app.default_pri;


	/* Get negotiation results MIB data */
	offset = SHMEM2_RD(bp, dcbx_neg_res_offset);
	if (SHMEM_DCBX_NEG_RES_NONE != offset) {
		struct lldp_local_mib local_mib;

		if(bnx2x_dcbx_read_mib(bp,
				       (u32 *)&local_mib,
				       offset,
				       DCBX_READ_LOCAL_MIB)){
			rc = -EIO;
			goto out;
		}

		dcbx_params->local_tc_supported =
			local_mib.features.app.tc_supported;
		dcbx_params->local_pfc_caps    =
			local_mib.features.pfc.pfc_caps;
		dcbx_params->local_ets_enable  =
			local_mib.features.ets.enabled;
		dcbx_params->local_pfc_enable  =
			local_mib.features.pfc.enabled;

		bnx2x_dcbx_get_bw_precentage_tbl(bp,
			dcbx_params->local_configuration_bw_precentage,
			local_mib.features.ets.pg_bw_tbl);

		bnx2x_dcbx_get_ets_pri_pg_tbl(bp,
			dcbx_params->local_configuration_ets_pg,
			local_mib.features.ets.pri_pg_tbl);

		dcbx_params->local_pfc_bitmap =
			local_mib.features.pfc.pri_en_bitmap;

		bnx2x_dcbx_get_priority_app_table(bp,
			dcbx_params->local_priority_app_table,
			local_mib.features.app.app_pri_tbl);

		dcbx_params->pfc_mismatch =
			GET_FLAGS(local_mib.error,DCBX_LOCAL_PFC_MISMATCH) ?
			true : false;
		dcbx_params->priority_app_mismatch =
			GET_FLAGS(local_mib.error,DCBX_LOCAL_APP_MISMATCH) ?
			true : false;
	}


	offset = SHMEM2_RD(bp, dcbx_remote_mib_offset);
	if (SHMEM_DCBX_NEG_RES_NONE != offset) {
		struct lldp_remote_mib remote_mib;

		if (bnx2x_dcbx_read_mib(bp,
					(u32*)&remote_mib,
					offset,
					DCBX_READ_REMOTE_MIB)){
			rc = -EIO;
			goto out;
		}

		dcbx_params->remote_tc_supported =
			remote_mib.features.app.tc_supported;
		dcbx_params->remote_pfc_cap =
			remote_mib.features.pfc.pfc_caps;

		dcbx_params->remote_ets_reco_valid =
			GET_FLAGS(remote_mib.flags,DCBX_REMOTE_ETS_RECO_VALID) ?
			true : false;

		dcbx_params->remote_ets_willing =
			GET_FLAGS(remote_mib.flags,DCBX_ETS_REM_WILLING) ?
			true : false;

		dcbx_params->remote_pfc_willing =
			GET_FLAGS(remote_mib.flags,DCBX_PFC_REM_WILLING) ?
			true : false;

		dcbx_params->remote_app_priority_willing =
			GET_FLAGS(remote_mib.flags,DCBX_APP_REM_WILLING) ?
			true : false;

		bnx2x_dcbx_get_bw_precentage_tbl(bp,
				dcbx_params->remote_configuration_bw_precentage,
				remote_mib.features.ets.pg_bw_tbl);

		bnx2x_dcbx_get_ets_pri_pg_tbl(bp,
				dcbx_params->remote_configuration_ets_pg,
				remote_mib.features.ets.pri_pg_tbl);

		dcbx_params->remote_pfc_bitmap =
			remote_mib.features.pfc.pri_en_bitmap;

		bnx2x_dcbx_get_priority_app_table(bp,
				dcbx_params->remote_priority_app_table,
				remote_mib.features.app.app_pri_tbl);
	}

	/* handle the stats */
	bnx2x_read_data(bp, (u32*)&lldp_dcbx_stat, stat_offset,
			 sizeof(struct lldp_dcbx_stat));

	dcbx_params->dcbx_frames_sent = lldp_dcbx_stat.num_tx_dcbx_pkts;
	dcbx_params->dcbx_frames_received = lldp_dcbx_stat.num_rx_dcbx_pkts;

	bnx2x_acquire_phy_lock(bp);
	bnx2x_pfc_statistic(&bp->link_params, &bp->link_vars,
			    dcbx_params->pfc_frames_sent,
			    dcbx_params->pfc_frames_received);
	bnx2x_release_phy_lock(bp);


	if(copy_to_user(uaddr, dcbx_params,
			sizeof(struct bnx2x_dcbx_params_get))){
		BNX2X_ERR("Can't copy to user\n");
		rc = -EFAULT;
	}
out:
	kfree(dcbx_params);

	return rc;
}

/*******************************************************************************
 * Description: Fill pfc_config struct that will be sent in DCBX start ramrod
 *
 * Return:
 ******************************************************************************/
static void bnx2x_pfc_fw_struct_e2(struct bnx2x *bp)
{
	struct flow_control_configuration   *pfc_fw_cfg = 0;
	u16 pri_bit = 0;
	u8 cos = 0, pri = 0;
	struct priority_cos *tt2cos;
	u32 *ttp = bp->dcbx_port_params.app.traffic_type_priority;

	pfc_fw_cfg = (struct flow_control_configuration*)
					bnx2x_sp(bp, pfc_config);
	memset(pfc_fw_cfg, 0, sizeof(struct flow_control_configuration));

	/*shortcut*/
	tt2cos = pfc_fw_cfg->traffic_type_to_priority_cos;

	/* Fw version should be incremented each update */
	pfc_fw_cfg->dcb_version = ++bp->dcb_version;
	pfc_fw_cfg->dcb_enabled = DCB_ENABLED;

	/* Default initialization */
	for (pri = 0; pri < MAX_PFC_TRAFFIC_TYPES ; pri++) {
		tt2cos[pri].priority = LLFC_TRAFFIC_TYPE_TO_PRIORITY_UNMAPPED;
		tt2cos[pri].cos = 0;
	}

	/* Fill priority parameters */
	for (pri = 0; pri < LLFC_DRIVER_TRAFFIC_TYPE_MAX; pri++){
		tt2cos[pri].priority = ttp[pri];
		pri_bit = 1 << tt2cos[pri].priority;

		/* Fill COS parameters based on COS calculated to
		 * make it more generally for future use */
		for ( cos = 0; cos < bp->dcbx_port_params.ets.num_of_cos; cos++)
			if (bp->dcbx_port_params.ets.cos_params[cos].
						pri_bitmask & pri_bit)
					tt2cos[pri].cos = cos;
	}
	bnx2x_dcbx_print_cos_params(bp,	pfc_fw_cfg);
}

