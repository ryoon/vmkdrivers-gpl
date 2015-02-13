/* bnx2x_sp_verbs.c: Broadcom Everest network driver.
 *
 * Copyright 2010-2011 Broadcom Corporation
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
 *
 * Written by: Vladislav Zolotarov
 */
#include <linux/version.h>
#include <linux/module.h>
#include <linux/crc32.h>
#if (LINUX_VERSION_CODE >= 0x02061b) && !defined(BNX2X_DRIVER_DISK) && !defined(__VMKLNX__) /* BNX2X_UPSTREAM */
#include <linux/crc32c.h>
#endif
#include "bnx2x.h"
#include "bnx2x_common.h"

#if (LINUX_VERSION_CODE >= 0x02061b) /* BNX2X_UPSTREAM */
#define BNX2X_MAC_FMT		"%pM"
#define BNX2X_MAC_PRN_LIST(mac)	(mac)
#else
#define BNX2X_MAC_FMT		"%02x:%02x:%02x:%02x:%02x:%02x"
#define BNX2X_MAC_PRN_LIST(mac)	(mac)[0], (mac)[1], (mac)[2], (mac)[3], (mac)[4], (mac)[5]
#endif

/************************** raw_obj functions *************************/
static bool bnx2x_raw_check_pending(struct bnx2x_raw_obj *o)
{
	return test_bit(o->state, o->pstate);
}

static void bnx2x_raw_clear_pending(struct bnx2x_raw_obj *o)
{
	smp_mb__before_clear_bit();
	clear_bit(o->state, o->pstate);
	smp_mb__after_clear_bit();
}

static void bnx2x_raw_set_pending(struct bnx2x_raw_obj *o)
{
	smp_mb__before_clear_bit();
	set_bit(o->state, o->pstate);
	smp_mb__after_clear_bit();
}

/**
 * Waits until the given bit(state) is cleared in a state memory
 * buffer
 *
 * @param bp
 * @param state state which is to be cleared
 * @param state_p state buffer
 *
 * @return int
 */
static inline int __state_wait(struct bnx2x *bp, int state,
				    unsigned long *pstate)
{
	/* can take a while if any port is running */
	int cnt = 5000;

	if (CHIP_REV_IS_EMUL(bp))
		cnt*=20;

	DP(NETIF_MSG_IFUP, "waiting for state to become %d\n", state);

	might_sleep();
	while (cnt--) {
		if (!test_bit(state, pstate)) {
#ifdef BNX2X_STOP_ON_ERROR
			DP(NETIF_MSG_IFUP, "exit  (cnt %d)\n", 5000 - cnt);
#endif
			return 0;
		}

		msleep(1);

		if (bp->panic)
			return -EIO;
	}

	/* timeout! */
	BNX2X_ERR("timeout waiting for state %d\n", state);
#ifdef BNX2X_STOP_ON_ERROR
	bnx2x_panic();
#endif

	return -EBUSY;
}

static int bnx2x_raw_wait(struct bnx2x *bp, struct bnx2x_raw_obj *raw)
{
	return __state_wait(bp, raw->state, raw->pstate);
}

/***************** Classification verbs: Set/Del MAC/VLAN/VLAN-MAC ************/
/* credit handling callbacks */
static bool bnx2x_get_credit_mac(struct bnx2x_vlan_mac_obj *o)
{
	struct bnx2x_credit_pool_obj *mp = o->macs_pool;

	WARN_ON(!mp);

	return mp->get(mp, 1);
}

static bool bnx2x_get_credit_vlan(struct bnx2x_vlan_mac_obj *o)
{
	struct bnx2x_credit_pool_obj *vp = o->vlans_pool;

	WARN_ON(!vp);

	return vp->get(vp, 1);
}

static bool bnx2x_get_credit_vlan_mac(struct bnx2x_vlan_mac_obj *o)
{
	struct bnx2x_credit_pool_obj *mp = o->macs_pool;
	struct bnx2x_credit_pool_obj *vp = o->vlans_pool;

	if (!mp->get(mp, 1))
		return false;

	if (!vp->get(vp, 1)) {
		mp->put(mp, 1);
		return false;
	}

	return true;
}

static bool bnx2x_put_credit_mac(struct bnx2x_vlan_mac_obj *o)
{
	struct bnx2x_credit_pool_obj *mp = o->macs_pool;

	return mp->put(mp, 1);
}

static bool bnx2x_put_credit_vlan(struct bnx2x_vlan_mac_obj *o)
{
	struct bnx2x_credit_pool_obj *vp = o->vlans_pool;

	return vp->put(vp, 1);
}

static bool bnx2x_put_credit_vlan_mac(struct bnx2x_vlan_mac_obj *o)
{
	struct bnx2x_credit_pool_obj *mp = o->macs_pool;
	struct bnx2x_credit_pool_obj *vp = o->vlans_pool;

	if (!mp->put(mp, 1))
		return false;

	if (!vp->put(vp, 1)) {
		mp->get(mp, 1);
		return false;
	}

	return true;
}


/* check_add() callbacks */
static bool bnx2x_check_mac_add(struct bnx2x_vlan_mac_ramrod_params *p)
{
	struct bnx2x_list_elem *pos;
	struct bnx2x_vlan_mac_obj *o = p->vlan_mac_obj;

	/* Check if a requested MAC already exists */
	list_for_each_entry(pos, &o->head, link)
		if (!memcmp(p->data.mac.mac, pos->data.mac.mac, ETH_ALEN))
			return false;

	return true;
}

static bool bnx2x_check_vlan_add(struct bnx2x_vlan_mac_ramrod_params *p)
{
	struct bnx2x_list_elem *pos;
	struct bnx2x_vlan_mac_obj *o = p->vlan_mac_obj;

	list_for_each_entry(pos, &o->head, link)
		if (p->data.vlan.vlan == pos->data.vlan.vlan)
			return false;

	return true;
}

static bool
	bnx2x_check_vlan_mac_add(struct bnx2x_vlan_mac_ramrod_params *p)
{
	struct bnx2x_list_elem *pos;
	struct bnx2x_vlan_mac_obj *o = p->vlan_mac_obj;

	list_for_each_entry(pos, &o->head, link)
		if ((p->data.vlan_mac.vlan == pos->data.vlan_mac.vlan) &&
		    (!memcmp(p->data.vlan_mac.mac, pos->data.vlan_mac.mac,
			     ETH_ALEN)))
			return false;

	return true;
}


/* check_del() callbacks */
static struct bnx2x_list_elem *
	bnx2x_check_mac_del(struct bnx2x_vlan_mac_ramrod_params *p)
{
	struct bnx2x_list_elem *pos;
	struct bnx2x_vlan_mac_obj *o = p->vlan_mac_obj;

	list_for_each_entry(pos, &o->head, link)
		if (!memcmp(p->data.mac.mac, pos->data.mac.mac, ETH_ALEN))
			return pos;

	return NULL;
}

static struct bnx2x_list_elem *
	bnx2x_check_vlan_del(struct bnx2x_vlan_mac_ramrod_params *p)
{
	struct bnx2x_list_elem *pos;
	struct bnx2x_vlan_mac_obj *o = p->vlan_mac_obj;

	list_for_each_entry(pos, &o->head, link)
		if (p->data.vlan.vlan == pos->data.vlan.vlan)
			return pos;

	return NULL;
}

static struct bnx2x_list_elem *
	bnx2x_check_vlan_mac_del(struct bnx2x_vlan_mac_ramrod_params *p)
{
	struct bnx2x_list_elem *pos;
	struct bnx2x_vlan_mac_obj *o = p->vlan_mac_obj;

	list_for_each_entry(pos, &o->head, link)
		if ((p->data.vlan_mac.vlan == pos->data.vlan_mac.vlan) &&
		    (!memcmp(p->data.vlan_mac.mac, pos->data.vlan_mac.mac,
			     ETH_ALEN)))
			return pos;

	return NULL;
}

static inline u8 __vlan_mac_get_rx_tx_flag(struct bnx2x_vlan_mac_obj *o)
{
	struct bnx2x_raw_obj *raw = &o->raw;
	u8 rx_tx_flag = 0;

	if ((raw->obj_type == BNX2X_OBJ_TYPE_TX) ||
	    (raw->obj_type == BNX2X_OBJ_TYPE_RX_TX))
		rx_tx_flag |= ETH_CLASSIFY_CMD_HEADER_TX_CMD;

	if ((raw->obj_type == BNX2X_OBJ_TYPE_RX) ||
	    (raw->obj_type == BNX2X_OBJ_TYPE_RX_TX))
		rx_tx_flag |= ETH_CLASSIFY_CMD_HEADER_RX_CMD;

	return rx_tx_flag;
}

/* LLH CAM line allocations */
enum {
	LLH_CAM_ISCSI_ETH_LINE = 0,
	LLH_CAM_ETH_LINE,
	LLH_CAM_MAX_PF_LINE = NIG_REG_LLH1_FUNC_MEM_SIZE / 2
};

static void bnx2x_set_mac_in_nig(struct bnx2x *bp,
			  int add,
			  unsigned char* dev_addr,
			  int index)
{
	u32 wb_data[2];
	u32 reg_offset = BP_PORT(bp)? NIG_REG_LLH1_FUNC_MEM :
				 NIG_REG_LLH0_FUNC_MEM;

	if (!IS_MF_SI(bp) || index > LLH_CAM_MAX_PF_LINE)
		return;

	if (add) {
		/* LLH_FUNC_MEM is a u64 WB register */
		reg_offset += 8*index;

		wb_data[0] = ((dev_addr[2] << 24) | (dev_addr[3] << 16) |
			      (dev_addr[4] <<  8) |  dev_addr[5]);
		wb_data[1] = ((dev_addr[0] <<  8) |  dev_addr[1]);

		REG_WR_DMAE(bp, reg_offset, wb_data, 2);
	}

	REG_WR(bp, (BP_PORT(bp) ? NIG_REG_LLH1_FUNC_MEM_ENABLE :
				  NIG_REG_LLH0_FUNC_MEM_ENABLE ) +
		    4*index, add);
}

/* hw_config() callbacks */
static int bnx2x_setup_mac_e2(struct bnx2x *bp,
				    struct bnx2x_vlan_mac_ramrod_params *p,
				    struct bnx2x_list_elem *pos,
				    bool add)
{
	struct bnx2x_raw_obj *raw = &p->vlan_mac_obj->raw;
	struct bnx2x_vlan_mac_obj *o = p->vlan_mac_obj;
	struct eth_classify_rules_ramrod_data *data =
		(struct eth_classify_rules_ramrod_data *)(raw->rdata);

	/* Set LLH CAM entry: currently only iSCSI and ETH macs are
	 * relevant. In addition, current implementation is tuned for a
	 * single ETH MAC.
	 *
	 * When multiple unicast ETH MACs PF configuration in switch
	 * independent mode is required (NetQ, multiple netdev MACs,
	 * etc.), consider better utilisation of 8 per function MAC
	 * entries in the LLH register. There is also
	 * NIG_REG_P[01]_LLH_FUNC_MEM2 registers that complete the
	 * total number of CAM entries to 16.
	 */
	if (test_bit(BNX2X_ISCSI_ETH_MAC, &p->vlan_mac_flags))
		bnx2x_set_mac_in_nig(bp, add, p->data.mac.mac,
				     LLH_CAM_ISCSI_ETH_LINE);
	else
		bnx2x_set_mac_in_nig(bp, add, p->data.mac.mac,
				     LLH_CAM_ETH_LINE);

	/* Update a list element */
	memcpy(pos->data.mac.mac, p->data.mac.mac, ETH_ALEN);

	/* Reset the ramrod data buffer */
	memset(data, 0, sizeof(*data));

	/* Setup ramrod data */
	data->header.echo = ((raw->cid & BNX2X_SWCID_MASK) |
		(o->raw.state << BNX2X_SWCID_SHIFT));
	data->header.rule_cnt = 1;

	data->rules[0].mac.header.client_id = raw->cl_id;
	data->rules[0].mac.header.func_id = raw->func_id;

	/* Rx or/and Tx (internal switching) MAC ? */
	data->rules[0].mac.header.cmd_general_data |=
		__vlan_mac_get_rx_tx_flag(o);

	if (add)
		data->rules[0].mac.header.cmd_general_data |=
			ETH_CLASSIFY_CMD_HEADER_IS_ADD;

	DP(NETIF_MSG_IFUP, "About to %s MAC "BNX2X_MAC_FMT" for Client %d\n",
	   (add ? "add" : "delete"), BNX2X_MAC_PRN_LIST(p->data.mac.mac),
	   raw->cl_id);

	data->rules[0].mac.header.cmd_general_data |=
			CLASSIFY_RULE_OPCODE_MAC;

	/* Set a MAC itself */
	data->rules[0].mac.mac_msb = swab16(*(u16 *)(&p->data.mac.mac[0]));
	data->rules[0].mac.mac_mid = swab16(*(u16 *)(&p->data.mac.mac[2]));
	data->rules[0].mac.mac_lsb = swab16(*(u16 *)(&p->data.mac.mac[4]));

	/* Commit writes towards the memory before sending a ramrod */
	mb();

	/* Send a ramrod */
	return bnx2x_sp_post(bp, RAMROD_CMD_ID_COMMON_CLASSIFICATION_RULES, 0,
			     U64_HI(raw->rdata_mapping),
			     U64_LO(raw->rdata_mapping), 1);
}

/* e1h Classification CAM line allocations */
enum {
	CAM_ETH_LINE = 0,
	CAM_ISCSI_ETH_LINE,
	CAM_FIP_ETH_LINE,
	CAM_FIP_MCAST_LINE,
	CAM_MAX_PF_LINE = CAM_FIP_MCAST_LINE
};

static inline u8 __e1h_cam_offset(int func_id, u8 rel_offset)
{
	return E1H_FUNC_MAX * rel_offset + func_id;
}

static int bnx2x_setup_mac_e1h(struct bnx2x *bp,
			       struct bnx2x_vlan_mac_ramrod_params *p,
			       struct bnx2x_list_elem *pos,
			       bool add)
{
	struct bnx2x_raw_obj *raw = &p->vlan_mac_obj->raw;
	struct mac_configuration_cmd *config =
		(struct mac_configuration_cmd *)(raw->rdata);
	u32 cl_bit_vec = (1 << raw->cl_id);

	/* Update a list element */
	memcpy(pos->data.mac.mac, p->data.mac.mac, ETH_ALEN);

	/* Reset the ramrod data buffer */
	memset(config, 0, sizeof(*config));

	config->hdr.length = 1;

	if (test_bit(BNX2X_ISCSI_ETH_MAC, &p->vlan_mac_flags))
		config->hdr.offset =
			__e1h_cam_offset(raw->func_id, CAM_ISCSI_ETH_LINE);
	else if (test_bit(BNX2X_ETH_MAC, &p->vlan_mac_flags))
		config->hdr.offset = __e1h_cam_offset(raw->func_id,
						      CAM_ETH_LINE);
	else if (test_bit(BNX2X_NETQ_ETH_MAC, &p->vlan_mac_flags))
		config->hdr.offset = __e1h_cam_offset(raw->func_id,
			raw->cl_id + CAM_MAX_PF_LINE);
	else {
		BNX2X_ERR("Invalid MAC type for 57711\n");
		return -EINVAL;
	}

	config->hdr.client_id = 0xff;
	config->hdr.echo = ((raw->cid & BNX2X_SWCID_MASK) |
		(BNX2X_FILTER_MAC_PENDING << BNX2X_SWCID_SHIFT));

	/* primary MAC */
	config->config_table[0].msb_mac_addr =
					swab16(*(u16 *)(&p->data.mac.mac[0]));
	config->config_table[0].middle_mac_addr =
					swab16(*(u16 *)(&p->data.mac.mac[2]));
	config->config_table[0].lsb_mac_addr =
					swab16(*(u16 *)(&p->data.mac.mac[4]));
	config->config_table[0].clients_bit_vector =
					cpu_to_le32(cl_bit_vec);
	config->config_table[0].vlan_id = 0;
	config->config_table[0].pf_id = raw->func_id;
	if (add)
		SET_FLAG(config->config_table[0].flags,
			MAC_CONFIGURATION_ENTRY_ACTION_TYPE,
			T_ETH_MAC_COMMAND_SET);
	else
		SET_FLAG(config->config_table[0].flags,
			MAC_CONFIGURATION_ENTRY_ACTION_TYPE,
			T_ETH_MAC_COMMAND_INVALIDATE);

	DP(NETIF_MSG_IFUP, "%s MAC (%04x:%04x:%04x)  PF_ID %d  CLID mask %d\n",
	   (add ? "setting" : "clearing"),
	   config->config_table[0].msb_mac_addr,
	   config->config_table[0].middle_mac_addr,
	   config->config_table[0].lsb_mac_addr, raw->func_id, cl_bit_vec);

	return bnx2x_sp_post(bp, RAMROD_CMD_ID_COMMON_SET_MAC, 0,
			     U64_HI(raw->rdata_mapping),
			     U64_LO(raw->rdata_mapping), 1);
}

static int bnx2x_setup_mac_e1(struct bnx2x *bp,
			       struct bnx2x_vlan_mac_ramrod_params *p,
			       struct bnx2x_list_elem *pos,
			       bool add)
{
	struct bnx2x_raw_obj *raw = &p->vlan_mac_obj->raw;
	struct mac_configuration_cmd *config =
		(struct mac_configuration_cmd *)(raw->rdata);
	u32 cl_bit_vec = (1 << raw->cl_id);

	/* Update a list element */
	memcpy(pos->data.mac.mac, p->data.mac.mac, ETH_ALEN);

	/* Reset the ramrod data buffer */
	memset(config, 0, sizeof(*config));

	config->hdr.length = 1;
	config->hdr.offset = raw->func_id ? 32 : 0;
	config->hdr.client_id = 0xff;
	config->hdr.echo = ((raw->cid & BNX2X_SWCID_MASK) |
		(BNX2X_FILTER_MAC_PENDING << BNX2X_SWCID_SHIFT));

	/* primary MAC */
	config->config_table[0].msb_mac_addr =
					swab16(*(u16 *)(&p->data.mac.mac[0]));
	config->config_table[0].middle_mac_addr =
					swab16(*(u16 *)(&p->data.mac.mac[2]));
	config->config_table[0].lsb_mac_addr =
					swab16(*(u16 *)(&p->data.mac.mac[4]));
	config->config_table[0].clients_bit_vector =
					cpu_to_le32(cl_bit_vec);
	config->config_table[0].vlan_id = 0;
	config->config_table[0].pf_id = raw->func_id;
	if (add)
		SET_FLAG(config->config_table[0].flags,
			MAC_CONFIGURATION_ENTRY_ACTION_TYPE,
			T_ETH_MAC_COMMAND_SET);
	else
		SET_FLAG(config->config_table[0].flags,
			MAC_CONFIGURATION_ENTRY_ACTION_TYPE,
			T_ETH_MAC_COMMAND_INVALIDATE);

	/* if broadcast */
	if (test_bit(BNX2X_BCAST_MAC, &p->vlan_mac_flags)) {
		SET_FLAG(config->config_table[0].flags,
			MAC_CONFIGURATION_ENTRY_BROADCAST, 1);
		config->hdr.offset++;
	}

	/* if iSCSI ETH MAC */
	if (test_bit(BNX2X_ISCSI_ETH_MAC, &p->vlan_mac_flags))
		config->hdr.offset += 2;

	/* if NETQ MAC */
	if (test_bit(BNX2X_NETQ_ETH_MAC, &p->vlan_mac_flags))
		config->hdr.offset += (2 + raw->cl_id);


	DP(NETIF_MSG_IFUP, "%s MAC (%04x:%04x:%04x)  PF_ID %d  CLID mask %d\n",
	   (add ? "setting" : "clearing"),
	   config->config_table[0].msb_mac_addr,
	   config->config_table[0].middle_mac_addr,
	   config->config_table[0].lsb_mac_addr, raw->func_id, cl_bit_vec);

	return bnx2x_sp_post(bp, RAMROD_CMD_ID_COMMON_SET_MAC, 0,
			     U64_HI(raw->rdata_mapping),
			     U64_LO(raw->rdata_mapping), 1);
}

static int bnx2x_setup_vlan_e2(struct bnx2x *bp,
				      struct bnx2x_vlan_mac_ramrod_params *p,
				      struct bnx2x_list_elem *pos,
				      bool add)
{
	struct bnx2x_raw_obj *raw = &p->vlan_mac_obj->raw;
	struct bnx2x_vlan_mac_obj *o = p->vlan_mac_obj;
	struct eth_classify_rules_ramrod_data *data =
	  (struct eth_classify_rules_ramrod_data *)(raw->rdata);

	/* Update a list element */
	pos->data.vlan.vlan = p->data.vlan.vlan;

	/* Reset the ramrod data buffer */
	memset(data, 0, sizeof(*data));

	/* Setup ramrod data */
	data->header.echo = ((raw->cid & BNX2X_SWCID_MASK) |
		(BNX2X_FILTER_VLAN_PENDING << BNX2X_SWCID_SHIFT));
	data->header.rule_cnt = 1;

	data->rules[0].vlan.header.client_id = raw->cl_id;
	data->rules[0].vlan.header.func_id = raw->func_id;

	/* Rx or/and Tx (internal switching) MAC ? */
	data->rules[0].vlan.header.cmd_general_data |=
		__vlan_mac_get_rx_tx_flag(o);

	if (add)
		data->rules[0].vlan.header.cmd_general_data |=
			ETH_CLASSIFY_CMD_HEADER_IS_ADD;
	DP(NETIF_MSG_IFUP, "About to %s VLAN %d\n", (add ? "add" : "delete"),
		  p->data.vlan.vlan);

	data->rules[0].vlan.header.cmd_general_data |=
			CLASSIFY_RULE_OPCODE_VLAN;

	/* Set a VLAN itself */
	data->rules[0].vlan.vlan = p->data.vlan.vlan;

	/* Commit writes towards the memory before sending a ramrod */
	mb();

	/* Send a ramrod */
	return bnx2x_sp_post(bp, RAMROD_CMD_ID_COMMON_CLASSIFICATION_RULES, 0,
			     U64_HI(raw->rdata_mapping),
			     U64_LO(raw->rdata_mapping), 1);
}

static int bnx2x_setup_vlan_e1x(struct bnx2x *bp,
				      struct bnx2x_vlan_mac_ramrod_params *p,
				      struct bnx2x_list_elem *pos,
				      bool add)
{
	/* Do nothing for 57710 and 57711 */
	p->vlan_mac_obj->raw.clear_pending(&p->vlan_mac_obj->raw);
	return 0;
}

static int bnx2x_setup_vlan_mac_e2(struct bnx2x *bp,
					  struct bnx2x_vlan_mac_ramrod_params *p,
					  struct bnx2x_list_elem *pos,
					  bool add)
{
	struct bnx2x_raw_obj *raw = &p->vlan_mac_obj->raw;
	struct bnx2x_vlan_mac_obj *o = p->vlan_mac_obj;
	struct eth_classify_rules_ramrod_data *data =
	  (struct eth_classify_rules_ramrod_data *)(raw->rdata);

	/* Update a list element */
	pos->data.vlan.vlan = p->data.vlan.vlan;
	memcpy(pos->data.vlan_mac.mac, p->data.vlan_mac.mac, ETH_ALEN);

	/* Reset the ramrod data buffer */
	memset(data, 0, sizeof(*data));

	/* Setup ramrod data */
	data->header.echo = ((raw->cid & BNX2X_SWCID_MASK) |
		(CLASSIFY_RULE_OPCODE_PAIR << BNX2X_SWCID_SHIFT));
	data->header.rule_cnt = 1;

	data->rules[0].pair.header.client_id = raw->cl_id;
	data->rules[0].pair.header.func_id = raw->func_id;

	/* Rx or/and Tx (internal switching) MAC ? */
	data->rules[0].pair.header.cmd_general_data |=
		__vlan_mac_get_rx_tx_flag(o);

	if (add)
		data->rules[0].pair.header.cmd_general_data |=
			ETH_CLASSIFY_CMD_HEADER_IS_ADD;

	data->rules[0].pair.header.cmd_general_data |=
			CLASSIFY_RULE_OPCODE_PAIR;

	/* Set VLAN and MAC themselvs */
	data->rules[0].pair.vlan = p->data.vlan_mac.vlan;
	data->rules[0].pair.mac_msb = swab16(*(u16 *)&p->data.vlan_mac.mac[0]);
	data->rules[0].pair.mac_mid = swab16(*(u16 *)&p->data.vlan_mac.mac[2]);
	data->rules[0].pair.mac_lsb = swab16(*(u16 *)&p->data.vlan_mac.mac[4]);

	/* Commit writes towards the memory before sending a ramrod */
	mb();

	/* Send a ramrod */
	return bnx2x_sp_post(bp, RAMROD_CMD_ID_COMMON_CLASSIFICATION_RULES, 0,
			     U64_HI(raw->rdata_mapping),
			     U64_LO(raw->rdata_mapping), 1);
}

static inline int __vlan_mac_add(struct bnx2x *bp,
				 struct bnx2x_vlan_mac_ramrod_params *p)
{
	struct bnx2x_list_elem *pos;
	struct bnx2x_vlan_mac_obj *o = p->vlan_mac_obj;
	struct bnx2x_raw_obj *raw = &p->vlan_mac_obj->raw;
	int rc;

	/* If this classification can not be added (is already set)
	 * - return a SUCCESS.
	 */
	if (!o->check_add(p))
		return 0;

	/* Consume the credit if not requested not to */
	if (!(test_bit(BNX2X_DONT_CONSUME_CAM_CREDIT,
			&p->vlan_mac_flags) || o->get_credit(o)))
		return -EINVAL;

	/* Add a new list entry */
	pos = kzalloc(sizeof(*pos), GFP_KERNEL);
	if (!pos) {
		BNX2X_ERR("Failed to allocate memory for a new list entry\n");
		rc = -ENOMEM;
		goto error_exit3;
	}

	/* Set 'pending' state */
	raw->set_pending(raw);

	/* Configure the new classification in the chip */
	rc = o->config_rule(bp, p, pos, true);
	if (rc)
		goto error_exit1;

	/* Wait for a ramrod completion if was requested */
	if (test_bit(RAMROD_COMP_WAIT, &p->ramrod_flags)) {
		rc = raw->wait_comp(bp, raw);
		if (rc)
			/** If there was a timeout we don't want to clear a pending
			 *  state but we also don't want to record this operation as
			 *  completed. Timeout should never happen. If it
			 *  does it means that there is something wrong with either
			 *  the FW/HW or the system. Both cases are fatal.
			 */
			goto error_exit2;
	}

	/* Now when we are done record the operation as completed */
	list_add(&pos->link, &o->head);

	return 0;

error_exit1:
	raw->clear_pending(raw);
error_exit2:
	kfree(pos);
error_exit3:
	/* Roll back a credit change */
	if (!test_bit(BNX2X_DONT_CONSUME_CAM_CREDIT, &p->vlan_mac_flags))
		o->put_credit(o);
	return rc;
}


static inline int __vlan_mac_del(struct bnx2x *bp,
				 struct bnx2x_vlan_mac_ramrod_params *p)
{
	struct bnx2x_list_elem *pos = NULL;
	struct bnx2x_vlan_mac_obj *o = p->vlan_mac_obj;
	struct bnx2x_raw_obj *raw = &p->vlan_mac_obj->raw;
	int rc;

	/* If this classification can not be delete (doesn't exist)
	 * - return a SUCCESS.
	 */
	pos = o->check_del(p);
	if (!pos)
		return 0;

	/* Return the credit to the credit pool if not requested not to */
	if (!(test_bit(BNX2X_DONT_CONSUME_CAM_CREDIT,
		       &p->vlan_mac_flags) || o->put_credit(o)))
			return -EINVAL;

	if (test_bit(RAMROD_DRV_CLR_ONLY, &p->ramrod_flags))
		goto clr_only;

	/* Set 'pending' state */
	raw->set_pending(raw);

	/* Configure the new classification in the chip */
	rc = o->config_rule(bp, p, pos, false);
	if (rc)
		goto error_exit1;

	/* Wait for a ramrod completion if was requested */
	if (test_bit(RAMROD_COMP_WAIT, &p->ramrod_flags)) {
		rc = raw->wait_comp(bp, raw);
		if (rc)
			/* See the same case comment in __vlan_mac_add() */
			goto error_exit2;
	}

clr_only:
	/* Now when we are done we may delete the entry from our records */
	list_del(&pos->link);
	kfree(pos);
	return 0;

error_exit1:
	raw->clear_pending(raw);
error_exit2:
	/* Roll back a credit change */
	if (!test_bit(BNX2X_DONT_CONSUME_CAM_CREDIT, &p->vlan_mac_flags))
		o->get_credit(o);

	return rc;
}

int bnx2x_config_vlan_mac(struct bnx2x *bp,
			  struct bnx2x_vlan_mac_ramrod_params *p, bool add)
{
	if (add)
		return __vlan_mac_add(bp, p);
	else
		return __vlan_mac_del(bp, p);
}

static inline void __init_raw_obj(struct bnx2x_raw_obj *raw, u16 cl_id,
	u32 cid, int func_id, void *rdata, dma_addr_t rdata_mapping, int state,
	unsigned long *pstate, bnx2x_obj_type type)
{
	raw->func_id = func_id;
	raw->cid = cid;
	raw->cl_id = cl_id;
	raw->rdata = rdata;
	raw->rdata_mapping = rdata_mapping;
	raw->state = state;
	raw->pstate = pstate;
	raw->obj_type = type;
	raw->check_pending = bnx2x_raw_check_pending;
	raw->clear_pending = bnx2x_raw_clear_pending;
	raw->set_pending = bnx2x_raw_set_pending;
	raw->wait_comp = bnx2x_raw_wait;
}

static inline void __init_vlan_mac_common(struct bnx2x_vlan_mac_obj *o,
			u16 cl_id, u32 cid, int func_id, void *rdata,
			dma_addr_t rdata_mapping, int state,
			unsigned long *pstate, bnx2x_obj_type type,
			struct bnx2x_credit_pool_obj *macs_pool,
			struct bnx2x_credit_pool_obj *vlans_pool)
{
	INIT_LIST_HEAD(&o->head);

	o->macs_pool = macs_pool;
	o->vlans_pool = vlans_pool;

	__init_raw_obj(&o->raw, cl_id, cid, func_id, rdata, rdata_mapping,
		       state, pstate, type);
}


void bnx2x_init_mac_obj(struct bnx2x *bp,
			struct bnx2x_vlan_mac_obj *mac_obj,
			u16 cl_id, u32 cid, int func_id, void *rdata,
			dma_addr_t rdata_mapping, int state,
			unsigned long *pstate, bnx2x_obj_type type,
			struct bnx2x_credit_pool_obj *macs_pool)
{
	__init_vlan_mac_common(mac_obj, cl_id, cid, func_id, rdata,
			       rdata_mapping, state, pstate, type, macs_pool,
			       NULL);

	mac_obj->get_credit = bnx2x_get_credit_mac;
	mac_obj->put_credit = bnx2x_put_credit_mac;

	if (CHIP_IS_E2(bp)) {
		mac_obj->config_rule = bnx2x_setup_mac_e2;
		mac_obj->check_del = bnx2x_check_mac_del;
		mac_obj->check_add = bnx2x_check_mac_add;
	} else if (CHIP_IS_E1(bp)) {
		mac_obj->config_rule = bnx2x_setup_mac_e1;
		mac_obj->check_del = bnx2x_check_mac_del;
		mac_obj->check_add = bnx2x_check_mac_add;
	} else if (CHIP_IS_E1H(bp)) {
		mac_obj->config_rule = bnx2x_setup_mac_e1h;
		mac_obj->check_del = bnx2x_check_mac_del;
		mac_obj->check_add = bnx2x_check_mac_add;
	} else {
		BNX2X_ERR("Do not support chips others than E2\n");
		BUG();
	}
}

void bnx2x_init_vlan_obj(struct bnx2x *bp,
			 struct bnx2x_vlan_mac_obj *vlan_obj,
			 u16 cl_id, u32 cid, int func_id, void *rdata,
			 dma_addr_t rdata_mapping, int state,
			 unsigned long *pstate, bnx2x_obj_type type,
			 struct bnx2x_credit_pool_obj *vlans_pool)
{
	__init_vlan_mac_common(vlan_obj, cl_id, cid, func_id, rdata,
			       rdata_mapping, state, pstate, type, NULL,
			       vlans_pool);

	vlan_obj->get_credit = bnx2x_get_credit_vlan;
	vlan_obj->put_credit = bnx2x_put_credit_vlan;

	if (CHIP_IS_E2(bp)) {
		vlan_obj->config_rule = bnx2x_setup_vlan_e2;
		vlan_obj->check_del = bnx2x_check_vlan_del;
		vlan_obj->check_add = bnx2x_check_vlan_add;
	} else if (CHIP_IS_E1(bp)) {
		vlan_obj->config_rule = bnx2x_setup_vlan_e1x;
		vlan_obj->check_del = bnx2x_check_vlan_del;
		vlan_obj->check_add = bnx2x_check_vlan_add;
	} else if (CHIP_IS_E1H(bp)) {
		vlan_obj->config_rule = bnx2x_setup_vlan_e1x;
		vlan_obj->check_del = bnx2x_check_vlan_del;
		vlan_obj->check_add = bnx2x_check_vlan_add;
	} else {
		BNX2X_ERR("Do not support chips others than E1X and E2\n");
		BUG();
	}
}

void bnx2x_init_vlan_mac_obj(struct bnx2x *bp,
			     struct bnx2x_vlan_mac_obj *vlan_mac_obj,
			     u16 cl_id, u32 cid, int func_id, void *rdata,
			     dma_addr_t rdata_mapping, int state,
			     unsigned long *pstate, bnx2x_obj_type type,
			     struct bnx2x_credit_pool_obj *macs_pool,
			     struct bnx2x_credit_pool_obj *vlans_pool)
{
	__init_vlan_mac_common(vlan_mac_obj, cl_id, cid, func_id, rdata,
			       rdata_mapping, state, pstate, type, macs_pool,
			       vlans_pool);

	vlan_mac_obj->get_credit = bnx2x_get_credit_vlan_mac;
	vlan_mac_obj->put_credit = bnx2x_put_credit_vlan_mac;

	if (CHIP_IS_E2(bp)) {
		vlan_mac_obj->config_rule = bnx2x_setup_vlan_mac_e2;
		vlan_mac_obj->check_del = bnx2x_check_vlan_mac_del;
		vlan_mac_obj->check_add = bnx2x_check_vlan_mac_add;
	} else {
		BNX2X_ERR("Do not support chips others than E2\n");
		BUG();
	}
}

/* RX_MODE verbs: DROP_ALL/ACCEPT_ALL/ACCEPT_ALL_MULTI/ACCEPT_ALL_VLAN/NORMAL */
static inline void __storm_memset_mac_filters(struct bnx2x *bp,
			struct tstorm_eth_mac_filter_config *mac_filters,
			u16 abs_fid)
{
	size_t size = sizeof(struct tstorm_eth_mac_filter_config);

	u32 addr = BAR_TSTRORM_INTMEM +
			TSTORM_MAC_FILTER_CONFIG_OFFSET(abs_fid);

	__storm_memset_struct(bp, addr, size, (u32*)mac_filters);
}

static int bnx2x_set_rx_mode_e1x(struct bnx2x *bp,
				 struct bnx2x_rx_mode_ramrod_params *p)
{
	u32 llh_mask;
	int port = (p->func_id & 0x1);

	/* update the bp MAC filter structure  */
	u32 mask = (1 << p->cl_id);

	struct tstorm_eth_mac_filter_config *mac_filters =
		(struct tstorm_eth_mac_filter_config *)p->rdata;

	/* initial seeting is drop-all */
	u8 drop_all_ucast = 1, drop_all_bcast = 1, drop_all_mcast = 1;
	u8 accp_all_ucast = 0, accp_all_bcast = 0, accp_all_mcast = 0;
	u8 unmatched_unicast = 0;

	if (test_bit(BNX2X_ACCEPT_UNICAST, &p->accept_flags))
		/* accept matched ucast */
		drop_all_ucast = 0;

	if (test_bit(BNX2X_ACCEPT_MULTICAST, &p->accept_flags)) {
		/* accept matched mcast */
		drop_all_mcast = 0;
		if (IS_MF_SI(bp)) /* What about E2 TODO */
			/* since mcast addresses won't arrive with ovlan,
			 * fw needs to accept all of them in
			 * switch-independent mode */
			accp_all_mcast = 1;
	}
	if (test_bit(BNX2X_ACCEPT_ALL_UNICAST, &p->accept_flags)) {
		/* accept all mcast */
		drop_all_ucast = 0;
		accp_all_ucast = 1;
	}
	if (test_bit(BNX2X_ACCEPT_ALL_MULTICAST, &p->accept_flags)) {
		/* accept all mcast */
		drop_all_mcast = 0;
		accp_all_mcast = 1;
	}
	if (test_bit(BNX2X_ACCEPT_BROADCAST, &p->accept_flags)) {
		/* accept (all) bcast */
		drop_all_bcast = 0;
		accp_all_bcast = 1;
	}
	if (test_bit(BNX2X_ACCEPT_UNMATCHED, &p->accept_flags))
		/* accept unmatched unicasts */
		unmatched_unicast = 1;

	mac_filters->ucast_drop_all = drop_all_ucast ?
		mac_filters->ucast_drop_all | mask :
		mac_filters->ucast_drop_all & ~mask;

	mac_filters->mcast_drop_all = drop_all_mcast ?
		mac_filters->mcast_drop_all | mask :
		mac_filters->mcast_drop_all & ~mask;

	mac_filters->bcast_drop_all = drop_all_bcast ?
		mac_filters->bcast_drop_all | mask :
		mac_filters->bcast_drop_all & ~mask;

	mac_filters->ucast_accept_all = accp_all_ucast ?
		mac_filters->ucast_accept_all | mask :
		mac_filters->ucast_accept_all & ~mask;

	mac_filters->mcast_accept_all = accp_all_mcast ?
		mac_filters->mcast_accept_all | mask :
		mac_filters->mcast_accept_all & ~mask;

	mac_filters->bcast_accept_all = accp_all_bcast ?
		mac_filters->bcast_accept_all | mask :
		mac_filters->bcast_accept_all & ~mask;

	mac_filters->unmatched_unicast = unmatched_unicast ?
		mac_filters->unmatched_unicast | mask :
		mac_filters->unmatched_unicast & ~mask;

	DP(NETIF_MSG_IFUP, "drop_ucast 0x%x\ndrop_mcast 0x%x\ndrop_bcast 0x%x\n"
		"accp_ucast 0x%x\naccp_mcast 0x%x\naccp_bcast 0x%x\n",
		mac_filters->ucast_drop_all,
		mac_filters->mcast_drop_all,
		mac_filters->bcast_drop_all,
		mac_filters->ucast_accept_all,
		mac_filters->mcast_accept_all,
		mac_filters->bcast_accept_all
	);

	/*
	 * All non-unicast management frames should be sent to the on-chip
	 * recieve buffer and may pass to the host as well. If
	 * the 'Accept-All-Unicast' flags is set then also unicast management
	 * frames go to the host.
	 *
	 * !!! For New boards the code below  has no effect what so ever,
	 * ALL management frames go to the BRB and are later filtered by the storm ALWAYS,
	 * So instead of this ugliness we should let the init-tool configure
	 * this register so that for older boards behave the same and we
	 * can get rid of this ugliness TALK WITH YARON TODO
	 */
	llh_mask =
		NIG_LLH0_BRB1_DRV_MASK_REG_LLH0_BRB1_DRV_MASK_BRCST |
		NIG_LLH0_BRB1_DRV_MASK_REG_LLH0_BRB1_DRV_MASK_MLCST |
		NIG_LLH0_BRB1_DRV_MASK_REG_LLH0_BRB1_DRV_MASK_VLAN |
		NIG_LLH0_BRB1_DRV_MASK_REG_LLH0_BRB1_DRV_MASK_NO_VLAN;
	if (test_bit(BNX2X_ACCEPT_ALL_UNICAST,&p->accept_flags))
		llh_mask |= NIG_LLH0_BRB1_DRV_MASK_REG_LLH0_BRB1_DRV_MASK_UNCST;

	REG_WR(bp,
	       (port ? NIG_REG_LLH1_BRB1_DRV_MASK : NIG_REG_LLH0_BRB1_DRV_MASK),
	       llh_mask);

	/* write the MAC filter structure*/
	__storm_memset_mac_filters(bp, mac_filters, p->func_id);

	/* The operation is completed */
	clear_bit(p->state, p->pstate);
	smp_mb__after_clear_bit();

	return 0;
}


static int bnx2x_set_rx_mode_e2(struct bnx2x *bp,
				struct bnx2x_rx_mode_ramrod_params *p)
{
	struct eth_filter_rules_ramrod_data *data = p->rdata;

	/* Reset the ramrod data buffer */
	memset(data, 0, sizeof(*data));

	/* Setup ramrod data */
	data->header.echo = p->cid;
	data->header.rule_cnt = 1;

	data->rules[0].client_id = p->cl_id;
	data->rules[0].func_id = p->func_id;

	/* Rx or/and Tx (internal switching) ? */
	if (test_bit(RAMROD_TX, &p->ramrod_flags))
		data->rules[0].cmd_general_data =
			ETH_FILTER_RULES_CMD_TX_CMD;

	if (test_bit(RAMROD_RX, &p->ramrod_flags))
		data->rules[0].cmd_general_data |=
			ETH_FILTER_RULES_CMD_RX_CMD;

	/* start with 'drop-all' */
	data->rules[0].state =
		ETH_FILTER_RULES_CMD_UCAST_DROP_ALL |
		ETH_FILTER_RULES_CMD_MCAST_DROP_ALL;

	if (p->accept_flags) {
		if (test_bit(BNX2X_ACCEPT_UNICAST, &p->accept_flags))
			data->rules[0].state &=
				~ETH_FILTER_RULES_CMD_UCAST_DROP_ALL;
		if (test_bit(BNX2X_ACCEPT_MULTICAST, &p->accept_flags))
			data->rules[0].state &=
				~ETH_FILTER_RULES_CMD_MCAST_DROP_ALL;
		if (test_bit(BNX2X_ACCEPT_ALL_UNICAST, &p->accept_flags)) {
			data->rules[0].state &=
				~ETH_FILTER_RULES_CMD_UCAST_DROP_ALL;
			data->rules[0].state |=
				ETH_FILTER_RULES_CMD_UCAST_ACCEPT_ALL;
		}
		if (test_bit(BNX2X_ACCEPT_ALL_MULTICAST, &p->accept_flags)){
			data->rules[0].state |=
				ETH_FILTER_RULES_CMD_MCAST_ACCEPT_ALL;
			data->rules[0].state &=
				~ETH_FILTER_RULES_CMD_MCAST_DROP_ALL;
		}
		if (test_bit(BNX2X_ACCEPT_BROADCAST, &p->accept_flags))
			data->rules[0].state |=
				ETH_FILTER_RULES_CMD_BCAST_ACCEPT_ALL;
		if (test_bit(BNX2X_ACCEPT_UNMATCHED, &p->accept_flags)) {
			data->rules[0].state &=
				~ETH_FILTER_RULES_CMD_UCAST_DROP_ALL;
			data->rules[0].state |=
				ETH_FILTER_RULES_CMD_UCAST_ACCEPT_UNMATCHED;
		}
		if (test_bit(BNX2X_ACCEPT_ANY_VLAN, &p->accept_flags))
			data->rules[0].state |=
				ETH_FILTER_RULES_CMD_ACCEPT_ANY_VLAN;
	}

	/* If FCoE Client configuration has been requested */
	if (test_bit(BNX2X_RX_MODE_FCOE_ETH, &p->rx_mode_flags)) {
		data->header.rule_cnt = 2;

		data->rules[1].client_id = bnx2x_fcoe(bp, cl_id);
		data->rules[1].func_id = p->func_id;

		/* Rx or Tx (internal switching) ? */
		if (test_bit(RAMROD_TX, &p->ramrod_flags))
			data->rules[1].cmd_general_data =
				ETH_FILTER_RULES_CMD_TX_CMD;

		if (test_bit(RAMROD_RX, &p->ramrod_flags))
			data->rules[1].cmd_general_data |=
				ETH_FILTER_RULES_CMD_RX_CMD;

		data->rules[1].state = data->rules[0].state;
		data->rules[1].state &=
				~ETH_FILTER_RULES_CMD_MCAST_ACCEPT_ALL;
		data->rules[1].state &=
				~ETH_FILTER_RULES_CMD_BCAST_ACCEPT_ALL;
		data->rules[1].state &=
				~ETH_FILTER_RULES_CMD_UCAST_ACCEPT_ALL;
		data->rules[1].state &=
				~ETH_FILTER_RULES_CMD_UCAST_ACCEPT_UNMATCHED;
	}

	DP(NETIF_MSG_IFUP, "About to configure %d rules, "
			   "accept_flags 0x%lx, state[0] 0x%x "
			   "cmd_general_data[0] 0x%x\n",
	   data->header.rule_cnt, p->accept_flags,
	   data->rules[0].state, data->rules[0].cmd_general_data);

	/* Commit writes towards the memory before sending a ramrod */
	mb();

	/* Send a ramrod */
	return bnx2x_sp_post(bp, RAMROD_CMD_ID_COMMON_FILTER_RULES, 0,
			 U64_HI(p->rdata_mapping), U64_LO(p->rdata_mapping), 1);
}

static int bnx2x_wait_rx_mode_comp_e2(struct bnx2x *bp,
				      struct bnx2x_rx_mode_ramrod_params *p)
{
	return __state_wait(bp, p->state, p->pstate);
}

static int bnx2x_empty_rx_mode_wait(struct bnx2x *bp,
				    struct bnx2x_rx_mode_ramrod_params *p)
{
	/* Do nothing */
	return 0;
}

int bnx2x_config_rx_mode(struct bnx2x *bp, struct bnx2x_rx_mode_ramrod_params *p)
{
	int rc;

	/* Configure the new classification in the chip */
	rc = p->rx_mode_obj->config_rx_mode(bp, p);
	if (rc)
		return rc;

	/* Wait for a ramrod completion if was requested */
	if (test_bit(RAMROD_COMP_WAIT, &p->ramrod_flags)) {
		rc = p->rx_mode_obj->wait_comp(bp, p);
		if (rc)
			return rc;
	}

	return 0;
}

void bnx2x_init_rx_mode_obj(struct bnx2x *bp, struct bnx2x_rx_mode_obj *o)
{
	if (CHIP_IS_E2(bp)) {
		o->wait_comp = bnx2x_wait_rx_mode_comp_e2;
		o->config_rx_mode = bnx2x_set_rx_mode_e2;
	} else if (CHIP_IS_E1x(bp)) {
		o->wait_comp = bnx2x_empty_rx_mode_wait;
		o->config_rx_mode = bnx2x_set_rx_mode_e1x;
	} else {
		BNX2X_ERR("Do not support chips others than E1x and E2\n");
		BUG();
	}
}

/********************* Multicast verbs: SET, CLEAR ****************************/
static inline u8 __bin_from_mac(char *mac)
{
	return (crc32c_le(0, mac, ETH_ALEN) >> 24) & 0xff;
}

enum {
	BNX2X_MCAST_CMD_ADD,
	BNX2X_MCAST_CMD_DEL,
};

struct bnx2x_pending_mcast_mac {
	struct list_head link;
	u8 mac[ETH_ALEN];
	u8 pad[2]; /* For a natural alignment of the following buffer */
};

struct bnx2x_pending_mcast_cmd {
	struct list_head link;
	int type;
	struct list_head macs_head;
	u32 macs_num; /* Needed for REM (ALL) command */
};

static int bnx2x_mcast_wait(struct bnx2x *bp, struct bnx2x_mcast_obj *o)
{
	return (__state_wait(bp, o->sched_state, o->raw.pstate) ||
		o->raw.wait_comp(bp,&o->raw));
}

static int bnx2x_enqueue_mcast_cmd(struct bnx2x *bp,
				   struct bnx2x_mcast_obj *o,
				   struct bnx2x_mcast_ramrod_params *p,
				   bool add)
{
	int total_sz;
	struct bnx2x_pending_mcast_cmd *new_cmd;
	struct bnx2x_pending_mcast_mac *cur_mac = NULL;
	struct bnx2x_mcast_list_elem *pos;
	int macs_list_len = (add ? p->mcast_list_len : 0);

	/* If the command is empty ("handle pending commands only"), break */
	if (!p->mcast_list_len)
		return 0;

	 total_sz = sizeof(*new_cmd) +
		macs_list_len * sizeof(struct bnx2x_pending_mcast_mac);

	/* Add mcast is called under spin_lock, thus calling with GFP_ATOMIC */
	new_cmd = kzalloc(total_sz, GFP_ATOMIC);

	if (!new_cmd)
		return -ENOMEM;

	DP(NETIF_MSG_IFUP, "About to enqueue a new \"%s\" command. "
			 "macs_list_len=%d\n",
	   add ? "add" : "del", macs_list_len);

	INIT_LIST_HEAD(&new_cmd->macs_head);

	if (add) {
		new_cmd->type = BNX2X_MCAST_CMD_ADD;

		cur_mac = (struct bnx2x_pending_mcast_mac *)
			((u8*)new_cmd + sizeof(*new_cmd));

		/* Push the MACs of the current command into the pendig command MACs
		 * list: FIFO
		 */
		list_for_each_entry(pos, &p->mcast_list, link) {
			memcpy(cur_mac->mac, pos->mac, ETH_ALEN);
			list_add_tail(&cur_mac->link, &new_cmd->macs_head);
			cur_mac++;
		}
	} else {
		new_cmd->type = BNX2X_MCAST_CMD_DEL;
		new_cmd->macs_num = p->mcast_list_len;
	}

	/* Push the new pending command to the tail of the pending list: FIFO */
	list_add_tail(&new_cmd->link, &o->pending_cmds_head);

	o->set_sched(o);

	return 0;
}

/**
 * Finds the first set bin and clears it.
 *
 * @param o
 *
 * @return The index of the found bin or -1 if none is found
 */
static inline int __clear_first_bin(struct bnx2x_mcast_obj *o)
{
	int i, j;

	for (i = 0; i < BNX2X_MCAST_VEC_SZ; i++)
		if (o->vec[i])
			for (j = 0; j < BIT_VEC64_ELEM_SZ; j++) {
				int cur_bit = j + BIT_VEC64_ELEM_SZ * i;
				if (BIT_VEC64_TEST_BIT(o->vec, cur_bit)) {
					BIT_VEC64_CLEAR_BIT(o->vec, cur_bit);
					return cur_bit;
				}
			}
	return -1;
}

static inline u8 __mcast_get_rx_tx_flag(struct bnx2x_mcast_obj *o)
{
	struct bnx2x_raw_obj *raw = &o->raw;
	u8 rx_tx_flag = 0;

	if ((raw->obj_type == BNX2X_OBJ_TYPE_TX) ||
	    (raw->obj_type == BNX2X_OBJ_TYPE_RX_TX))
		rx_tx_flag |= ETH_MULTICAST_RULES_CMD_TX_CMD;

	if ((raw->obj_type == BNX2X_OBJ_TYPE_RX) ||
	    (raw->obj_type == BNX2X_OBJ_TYPE_RX_TX))
		rx_tx_flag |= ETH_MULTICAST_RULES_CMD_RX_CMD;

	return rx_tx_flag;
}

static void bnx2x_set_one_mcast_rule_e2(struct bnx2x *bp,
					struct bnx2x_mcast_ramrod_params *p,
					int idx, u8 *mac, bool add)
{
	struct bnx2x_mcast_obj *o = p->mcast_obj;
	struct bnx2x_raw_obj *r = &o->raw;
	struct eth_multicast_rules_ramrod_data *data =
		(struct eth_multicast_rules_ramrod_data *)(r->rdata);
	u8 func_id = r->func_id;
	u8 rx_tx_add_flag = __mcast_get_rx_tx_flag(o);
	int bin;

	if (add)
		rx_tx_add_flag |= ETH_MULTICAST_RULES_CMD_IS_ADD;

	data->rules[idx].cmd_general_data |= rx_tx_add_flag;

	/* Get a bin and update a bins vector */
	if (add) {
		bin = __bin_from_mac(mac);
		if (BIT_VEC64_TEST_BIT(o->vec, bin))
			/* If a bin has already been set, decrement the number
			 * of set bins.
			 */
			o->num_bins_set--;
		else
			BIT_VEC64_SET_BIT(o->vec, bin);
	} else
		bin = __clear_first_bin(o);

	WARN_ON(bin < 0);
	DP(NETIF_MSG_IFUP, "Updating bin %d\n", bin);

	data->rules[idx].bin_id = bin;
	data->rules[idx].func_id = func_id;
}

static inline int __handle_pending_mcast_cmds_e2(struct bnx2x *bp,
				struct bnx2x_mcast_ramrod_params *p)
{
	struct bnx2x_pending_mcast_cmd *cmd_pos, *cmd_pos_n;
	struct bnx2x_pending_mcast_mac *pmac_pos, *pmac_pos_n;
	int cnt = 0;
	struct bnx2x_mcast_obj *o = p->mcast_obj;

	list_for_each_entry_safe(cmd_pos, cmd_pos_n, &o->pending_cmds_head,
				 link) {
		if (cmd_pos->type == BNX2X_MCAST_CMD_ADD) {
			list_for_each_entry_safe(pmac_pos, pmac_pos_n,
						 &cmd_pos->macs_head, link) {

				o->set_one_rule(bp, p, cnt, &pmac_pos->mac[0],
						true);

				cnt++;

				DP(NETIF_MSG_IFUP, "About to configure "
				   BNX2X_MAC_FMT" mcast MAC\n",
				   BNX2X_MAC_PRN_LIST(pmac_pos->mac));

				list_del(&pmac_pos->link);

				/* Break if we reached the maximum number
				 * of rules.
				 */
				if (cnt >= o->max_cmd_len)
					break;
			}
		} else {
			while(cmd_pos->macs_num) {
				o->set_one_rule(bp, p, cnt, NULL, false);

				cnt++;

				cmd_pos->macs_num--;

				DP(NETIF_MSG_IFUP, "Deleting MAC. %d left,"
				   "cnt is %d\n", cmd_pos->macs_num, cnt);

				/* Break if we reached the maximum
				 * number of rules.
				 */
				if (cnt >= o->max_cmd_len)
					break;
			}
		}

		/* If the command has been completed - remove it from the list
		 * and free the memory
		 */
		if ((!cmd_pos->macs_num) && list_empty(&cmd_pos->macs_head)) {
			list_del(&cmd_pos->link);
			kfree(cmd_pos);
		}

		/* Break if we reached the maximum number of rules */
		if (cnt >= o->max_cmd_len)
			break;
	}

	return cnt;
}

static inline int __handle_current_mcast_cmd(struct bnx2x *bp,
			struct bnx2x_mcast_ramrod_params *p, bool add,
			int start_cnt)
{
	struct bnx2x_mcast_list_elem *mlist_pos;
	struct bnx2x_mcast_obj *o = p->mcast_obj;
	int cnt = start_cnt;

	DP(NETIF_MSG_IFUP, "p->mcast_list_len=%d\n", p->mcast_list_len);

	if (add)
		list_for_each_entry(mlist_pos, &p->mcast_list, link) {

			o->set_one_rule(bp, p, cnt, mlist_pos->mac, true);

			cnt++;

			DP(NETIF_MSG_IFUP, "About to configure "BNX2X_MAC_FMT
			   " mcast MAC\n", BNX2X_MAC_PRN_LIST(mlist_pos->mac));
		}
	else {
		int i;

		for (i = 0; i < p->mcast_list_len; i++) {
			o->set_one_rule(bp, p, cnt, NULL, false);

			cnt++;

			DP(NETIF_MSG_IFUP, "Deleting MAC. %d left\n",
			   p->mcast_list_len - i - 1);
		}
	}

	/* The current command has been handled */
	p->mcast_list_len = 0;

	return cnt;
}

static int bnx2x_mcast_preamble_e2(struct bnx2x *bp,
				   struct bnx2x_mcast_ramrod_params *p,
				   bool add_cont)
{
	struct bnx2x_mcast_obj *o = p->mcast_obj;

	/* DEL command deletes all currently configured MACs */
	if (!add_cont) {
		p->mcast_list_len = o->num_bins_set;
		o->num_bins_set = 0;
	} else
		/* Here we assume that all new MACs will fall into new bins.
		 * We will correct it during the calculation of the bin index.
		 */
		o->num_bins_set += p->mcast_list_len;

	/* Increase the total number of MACs pending to be configured */
	o->total_pending_num += p->mcast_list_len;

	return 0;
}

static void bnx2x_mcast_postmortem_e2(struct bnx2x *bp,
				      struct bnx2x_mcast_ramrod_params *p,
				      int old_num_bins)
{
	struct bnx2x_mcast_obj *o = p->mcast_obj;

	o->num_bins_set = old_num_bins;
	o->total_pending_num -= p->mcast_list_len;
}

/**
 * Sets a header values in struct eth_multicast_rules_ramrod_data
 *
 * @param bp
 * @param p
 * @param len number of rules to handle
 */
static inline void __mcast_set_rdata_hdr_e2(struct bnx2x *bp,
					struct bnx2x_mcast_ramrod_params *p,
					u8 len)
{
	struct bnx2x_raw_obj *r = &p->mcast_obj->raw;
	struct eth_multicast_rules_ramrod_data *data =
		(struct eth_multicast_rules_ramrod_data *)(r->rdata);

	data->header.echo = ((r->cid & BNX2X_SWCID_MASK) |
			  (BNX2X_FILTER_MCAST_PENDING << BNX2X_SWCID_SHIFT));
	data->header.rule_cnt = len;
}

static int bnx2x_setup_mcast_e2(struct bnx2x *bp,
				struct bnx2x_mcast_ramrod_params *p,
				bool add)
{
	struct bnx2x_raw_obj *raw = &p->mcast_obj->raw;
	struct bnx2x_mcast_obj *o = p->mcast_obj;
	struct eth_multicast_rules_ramrod_data *data =
		(struct eth_multicast_rules_ramrod_data *)(raw->rdata);
	int cnt = 0;

	/* Reset the ramrod data buffer */
	memset(data, 0, sizeof(*data));

	cnt = __handle_pending_mcast_cmds_e2(bp, p);

	/* If there are no more pending commands - clear SCHEDULED state */
	if (list_empty(&o->pending_cmds_head))
		o->clear_sched(o);

	/* The below may be true iff there was enough room in ramrod
	 * data for all pending commands and for the current
	 * command. Otherwise the current command would have been added
	 * to the pending commands and p->mcast_list_len would have been
	 * zeroed.
	 */
	if (p->mcast_list_len > 0)
		cnt = __handle_current_mcast_cmd(bp, p, add, cnt);

	/* We've pulled out some MACs - update the total number of
	 * outstanding.
	 */
	o->total_pending_num -= cnt;

	/* send a ramrod */

	WARN_ON(cnt > o->max_cmd_len);

	__mcast_set_rdata_hdr_e2(bp, p, cnt);

	/* Commit writes towards the memory before sending a ramrod */
	mb();

	/* If CLEAR_ONLY was requested - don't send a ramrod and clear
	 * RAMROD_PENDING status immediately.
	 */
	if (test_bit(RAMROD_DRV_CLR_ONLY, &p->ramrod_flags)) {
		raw->clear_pending(raw);
		return 0;
	} else
		/* Send a ramrod */
		return bnx2x_sp_post(bp, RAMROD_CMD_ID_COMMON_MULTICAST_RULES, 0,
				     U64_HI(raw->rdata_mapping),
				     U64_LO(raw->rdata_mapping), 1);
}

static int bnx2x_mcast_preamble_e1h(struct bnx2x *bp,
				    struct bnx2x_mcast_ramrod_params *p,
				    bool add_cont)
{
	/* Do nothing */
	return 0;
}

static void bnx2x_mcast_postmortem_e1h(struct bnx2x *bp,
				       struct bnx2x_mcast_ramrod_params *p,
				       int old_num_bins)
{
	/* Do nothing */
}

/* On 57711 we write the multicast MACs' aproximate match
 * table by directly into the TSTORM's internal RAM. So we don't
 * really need to handle any tricks to make it work.
 */
static int bnx2x_setup_mcast_e1h(struct bnx2x *bp,
				 struct bnx2x_mcast_ramrod_params *p,
				 bool add)
{
	int i;
	struct bnx2x_mcast_obj *o = p->mcast_obj;
	struct bnx2x_raw_obj *r = &o->raw;

	if (add) {
		u32 mc_filter[MC_HASH_SIZE] = {0};
		u32 bit;
		struct bnx2x_mcast_list_elem *mlist_pos;

		list_for_each_entry(mlist_pos, &p->mcast_list, link) {
			DP(NETIF_MSG_IFUP, "About to configure "BNX2X_MAC_FMT
			   " mcast MAC\n", BNX2X_MAC_PRN_LIST(mlist_pos->mac));


			bit = __bin_from_mac(mlist_pos->mac);
			mc_filter[bit >> 5] |= (1 << (bit & 0x1f));
		}

		for (i = 0; i < MC_HASH_SIZE; i++)
			REG_WR(bp, MC_HASH_OFFSET(bp, i), mc_filter[i]);

	} else {/* DEL */
		DP(NETIF_MSG_IFUP, "Invalidating multicast MACs "
				   "configuration\n");
		for (i = 0; i < MC_HASH_SIZE; i++)
			REG_WR(bp, MC_HASH_OFFSET(bp, i), 0);
	}

	/* We are done */
	r->clear_pending(r);

	return 0;
}

static int bnx2x_mcast_preamble_e1(struct bnx2x *bp,
				   struct bnx2x_mcast_ramrod_params *p,
				   bool add_cont)
{
	struct bnx2x_mcast_obj *o = p->mcast_obj;

	/* DEL command deletes all currently configured MACs */
	if (!add_cont) {
		p->mcast_list_len = o->num_bins_set;
		o->num_bins_set = 0;
	} else {
		/* Multicast MACs on 57710 are configured as unicast MACs and
		 * there is only a limited number of CAM entries for that
		 * matter.
		 */
		if (p->mcast_list_len > o->max_cmd_len) {
			BNX2X_ERR("Can't configure more than %d multicast MACs"
				  "on 57710\n", o->max_cmd_len);
			return -EINVAL;
		}
		/* Every configured MAC should be cleared if DEL command is
		 * called. Only the last ADD command is relevant as long as
		 * every ADD commands overrides the previous configuration.
		 */
		if (p->mcast_list_len > 0)
			o->num_bins_set = p->mcast_list_len;
	}

	/* We want to ensure that commands are executed one by one for 57710.
	 * Therefore each none-empty command will consume o->max_cmd_len.
	 */
	if (p->mcast_list_len)
		o->total_pending_num += o->max_cmd_len;

	return 0;
}

static void bnx2x_mcast_postmortem_e1(struct bnx2x *bp,
				      struct bnx2x_mcast_ramrod_params *p,
				      int old_num_bins)
{
	struct bnx2x_mcast_obj *o = p->mcast_obj;

	o->num_bins_set = old_num_bins;

	/* If current command hasn't been handled yet and we are
	 * here means that it's meant to be dropped and we have to
	 * update the number of outstandling MACs accordingly.
	 */
	if (p->mcast_list_len)
		o->total_pending_num -= o->max_cmd_len;
}

static void bnx2x_set_one_mcast_rule_e1(struct bnx2x *bp,
					struct bnx2x_mcast_ramrod_params *p,
					int idx, u8 *mac, bool add)
{
	struct bnx2x_raw_obj *r = &p->mcast_obj->raw;
	struct mac_configuration_cmd *data =
		(struct mac_configuration_cmd *)(r->rdata);

	/* copy mac */
	if (add) {
		data->config_table[idx].msb_mac_addr =
			swab16(*(u16 *)&mac[0]);
		data->config_table[idx].middle_mac_addr =
			swab16(*(u16 *)&mac[2]);
		data->config_table[idx].lsb_mac_addr =
			swab16(*(u16 *)&mac[4]);

		data->config_table[idx].vlan_id = 0;
		data->config_table[idx].pf_id = r->func_id;
		data->config_table[idx].clients_bit_vector =
			cpu_to_le32(1 << r->cl_id);

		SET_FLAG(data->config_table[idx].flags,
			MAC_CONFIGURATION_ENTRY_ACTION_TYPE,
			T_ETH_MAC_COMMAND_SET);
	}
}

/**
 * Sets a header values in struct mac_configuration_cmd.
 *
 * @param bp
 * @param p
 * @param len number of rules to handle
 */
static inline void __mcast_set_rdata_hdr_e1(struct bnx2x *bp,
					struct bnx2x_mcast_ramrod_params *p,
					u8 len)
{
	struct bnx2x_raw_obj *r = &p->mcast_obj->raw;
	struct mac_configuration_cmd *data =
		(struct mac_configuration_cmd *)(r->rdata);

	u8 offset = (CHIP_REV_IS_SLOW(bp) ?
		     BNX2X_MAX_EMUL_MULTI*(1 + r->func_id) :
		     BNX2X_MAX_MULTICAST*(1 + r->func_id));

	data->hdr.offset = offset;
	data->hdr.client_id = 0xff;
	data->hdr.echo = ((r->cid & BNX2X_SWCID_MASK) |
			  (BNX2X_FILTER_MCAST_PENDING << BNX2X_SWCID_SHIFT));
	data->hdr.length = len;
}


static inline int __handle_pending_mcast_cmds_e1(struct bnx2x *bp,
				struct bnx2x_mcast_ramrod_params *p)
{
	struct bnx2x_pending_mcast_cmd *cmd_pos;
	struct bnx2x_pending_mcast_mac *pmac_pos;
	int cnt = 0;
	struct bnx2x_mcast_obj *o = p->mcast_obj;
	u8 len;


	/* If nothing to be done - return */
	if (list_empty(&o->pending_cmds_head))
		return 0;

	/* Handle the first command */
	cmd_pos = list_first_entry(&o->pending_cmds_head,
				   struct bnx2x_pending_mcast_cmd, link);

	if (cmd_pos->type == BNX2X_MCAST_CMD_ADD) {
		list_for_each_entry(pmac_pos, &cmd_pos->macs_head, link) {

			o->set_one_rule(bp, p, cnt, &pmac_pos->mac[0], true);

			cnt++;

			DP(NETIF_MSG_IFUP, "About to configure "BNX2X_MAC_FMT
			   " mcast MAC\n", BNX2X_MAC_PRN_LIST(pmac_pos->mac));
		}

		len = cnt;
	} else {
		len = cmd_pos->macs_num;
		DP(NETIF_MSG_IFUP, "About to delete %d multicast MACs\n", len);
	}

	list_del(&cmd_pos->link);
	kfree(cmd_pos);

	return len;
}

static int bnx2x_setup_mcast_e1(struct bnx2x *bp,
				struct bnx2x_mcast_ramrod_params *p,
				bool add)
{
	struct bnx2x_raw_obj *raw = &p->mcast_obj->raw;
	struct bnx2x_mcast_obj *o = p->mcast_obj;
	struct mac_configuration_cmd *data =
		(struct mac_configuration_cmd *)(raw->rdata);
	int cnt = 0, i;

	/* Reset the ramrod data buffer */
	memset(data, 0, sizeof(*data));


	/* First set all entries as invalid */
	for (i = 0; i < o->max_cmd_len ; i++)
		SET_FLAG(data->config_table[i].flags,
			MAC_CONFIGURATION_ENTRY_ACTION_TYPE,
			T_ETH_MAC_COMMAND_INVALIDATE);

	/* Handle pending commands first */
	cnt = __handle_pending_mcast_cmds_e1(bp, p);

	/* If there are no more pending commands - clear SCHEDULED state */
	if (list_empty(&o->pending_cmds_head))
		o->clear_sched(o);

	/* The below may be true iff there were no pending commands */
	if (!cnt)
		cnt = __handle_current_mcast_cmd(bp, p, add, 0);

	/* For 57710 every command has o->max_cmd_len length to ensure that
	 * commands are done one at a time.
	 */
	o->total_pending_num -= o->max_cmd_len;

	/* send a ramrod */

	WARN_ON(cnt > o->max_cmd_len);

	__mcast_set_rdata_hdr_e1(bp, p, cnt);

	/* Commit writes towards the memory before sending a ramrod */
	mb();

	/* If CLEAR_ONLY was requested - don't send a ramrod and clear
	 * RAMROD_PENDING status immediately.
	 */
	if (test_bit(RAMROD_DRV_CLR_ONLY, &p->ramrod_flags)) {
		raw->clear_pending(raw);
		return 0;
	} else
		/* Send a ramrod */
		return bnx2x_sp_post(bp, RAMROD_CMD_ID_COMMON_SET_MAC, 0,
				     U64_HI(raw->rdata_mapping),
				     U64_LO(raw->rdata_mapping), 1);
}

int bnx2x_config_mcast(struct bnx2x *bp,
		       struct bnx2x_mcast_ramrod_params *p,
		       bool add_cont)
{
	struct bnx2x_mcast_obj *o = p->mcast_obj;
	struct bnx2x_raw_obj *r = &o->raw;
	int rc, old_num_bins;

	/* This is needed to recover o->num_bins_set in case of failure */
	old_num_bins = o->num_bins_set;

	/* Do some calculations and checks */
	rc = o->preamble(bp, p, add_cont);
	if (rc)
		return rc;

	/* Return if there is no work to do */
	if ((!p->mcast_list_len) && (!o->check_sched(o)))
		return 0;

	DP(NETIF_MSG_IFUP, "o->total_pending_num=%d p->mcast_list_len=%d "
			   "o->max_cmd_len=%d\n", o->total_pending_num,
			p->mcast_list_len, o->max_cmd_len);

	/* Enqueue the current command to the pending list if we can't complete
	 * it in the current iteration
	 */
	if (r->check_pending(r) ||
	    ((o->max_cmd_len > 0) && (o->total_pending_num > o->max_cmd_len))) {
		rc = o->enqueue_cmd(bp, p->mcast_obj, p, add_cont);
		if (rc)
			goto error_exit1;

		/* As long as the current command is in a command list we
		 * don't need to handle it separately.
		 */
		p->mcast_list_len = 0;
	}

	if (!r->check_pending(r)) {

		/* Set 'pending' state */
		r->set_pending(r);

		/* Configure the new classification in the chip */
		rc = o->config_mcast(bp, p, add_cont);
		if (rc)
			goto error_exit2;

		/* Wait for a ramrod completion if was requested */
		if (test_bit(RAMROD_COMP_WAIT, &p->ramrod_flags)) {
			rc = o->wait_comp(bp, o);
			if (rc)
				return rc;
		}
	}

	return 0;

error_exit2:
	r->clear_pending(r);

error_exit1:
	o->postmortem(bp, p, old_num_bins);

	return rc;
}

static void bnx2x_mcast_clear_sched(struct bnx2x_mcast_obj *o)
{
	smp_mb__before_clear_bit();
	clear_bit(o->sched_state, o->raw.pstate);
	smp_mb__after_clear_bit();
}

static void bnx2x_mcast_set_sched(struct bnx2x_mcast_obj *o)
{
	smp_mb__before_clear_bit();
	set_bit(o->sched_state, o->raw.pstate);
	smp_mb__after_clear_bit();
}

static bool bnx2x_mcast_check_sched(struct bnx2x_mcast_obj *o)
{
	return test_bit(o->sched_state, o->raw.pstate);
}

static bool bnx2x_check_mcast_pending(struct bnx2x_mcast_obj *o)
{
	return (o->raw.check_pending(&o->raw) || o->check_sched(o));
}

void bnx2x_init_mcast_obj(struct bnx2x *bp,
			  struct bnx2x_mcast_obj *mcast_obj,
			  u16 mcast_cl_id, u32 mcast_cid, int func_id,
			  void *rdata, dma_addr_t rdata_mapping, int state,
			  unsigned long *pstate, bnx2x_obj_type type)
{
	memset(mcast_obj, 0, sizeof(*mcast_obj));

	__init_raw_obj(&mcast_obj->raw, mcast_cl_id, mcast_cid, func_id, rdata,
		       rdata_mapping, state, pstate, type);

	INIT_LIST_HEAD(&mcast_obj->pending_cmds_head);

	mcast_obj->sched_state = BNX2X_FILTER_MCAST_SCHED;
	mcast_obj->check_sched = bnx2x_mcast_check_sched;
	mcast_obj->set_sched = bnx2x_mcast_set_sched;
	mcast_obj->clear_sched = bnx2x_mcast_clear_sched;

	if (CHIP_IS_E2(bp)) {
		mcast_obj->config_mcast = bnx2x_setup_mcast_e2;
		mcast_obj->enqueue_cmd = bnx2x_enqueue_mcast_cmd;
		mcast_obj->check_pending = bnx2x_check_mcast_pending;
	/* TODO: There should be a proper HSI define for this number!!! */
		mcast_obj->max_cmd_len = 16;
		mcast_obj->wait_comp = bnx2x_mcast_wait;
		mcast_obj->set_one_rule = bnx2x_set_one_mcast_rule_e2;
		mcast_obj->preamble = bnx2x_mcast_preamble_e2;
		mcast_obj->postmortem = bnx2x_mcast_postmortem_e2;
	} else if (CHIP_IS_E1(bp)) {
		mcast_obj->config_mcast = bnx2x_setup_mcast_e1;
		mcast_obj->enqueue_cmd = bnx2x_enqueue_mcast_cmd;
		mcast_obj->check_pending = bnx2x_check_mcast_pending;

		if (CHIP_REV_IS_SLOW(bp))
			mcast_obj->max_cmd_len = BNX2X_MAX_EMUL_MULTI;
		else
			mcast_obj->max_cmd_len = BNX2X_MAX_MULTICAST;

		mcast_obj->wait_comp = bnx2x_mcast_wait;
		mcast_obj->set_one_rule = bnx2x_set_one_mcast_rule_e1;
		mcast_obj->preamble = bnx2x_mcast_preamble_e1;
		mcast_obj->postmortem = bnx2x_mcast_postmortem_e1;
	} else if (CHIP_IS_E1H(bp)) {
		mcast_obj->config_mcast = bnx2x_setup_mcast_e1h;
		mcast_obj->enqueue_cmd = NULL;
		mcast_obj->check_pending = bnx2x_check_mcast_pending;

		/* 57711 doesn't send a ramrod, so it has unlimited credit
		 * for one command.
		 */
		mcast_obj->max_cmd_len = -1;
		mcast_obj->wait_comp = bnx2x_mcast_wait;
		mcast_obj->set_one_rule = NULL;
		mcast_obj->preamble = bnx2x_mcast_preamble_e1h;
		mcast_obj->postmortem = bnx2x_mcast_postmortem_e1h;
	} else {
		BNX2X_ERR("Do not support chips others than E1X and E2\n");
		BUG();
	}
}

/*************************** Credit handling **********************************/

/**
 * atomic_add_ifless - adds if the result is less than a given
 * value.
 * @param v pointer of type atomic_t
 * @param a the amount to add to v...
 * @param u ...if (v + a) is less than u.
 *
 * @return TRUE if (v + a) was less than u, and FALSE
 * otherwise.
 */
static inline bool __atomic_add_ifless(atomic_t *v, int a, int u)
{
	int c, old;

	c = atomic_read(v);
	for (;;) {
		if (unlikely(c + a >= u))
			return false;

		old = atomic_cmpxchg((v), c, c + a);
		if (likely(old == c))
			break;
		c = old;
	}

	return true;
}

/**
 * atomic_dec_ifmoe - dec if the result is more or equal than a
 * given value.
 * @param v pointer of type atomic_t
 * @param a the amount to dec from v...
 * @param u ...if (v - a) is more or equal than u.
 *
 * @return TRUE if (v - a) was more or equal than u, and FALSE
 * otherwise.
 */
static inline bool __atomic_dec_ifmoe(atomic_t *v, int a, int u)
{
	int c, old;

	c = atomic_read(v);
	for (;;) {
		if (unlikely(c - a < u))
			return false;

		old = atomic_cmpxchg((v), c, c - a);
		if (likely(old == c))
			break;
		c = old;
	}

	return true;
}

static bool bnx2x_credit_pool_get(struct bnx2x_credit_pool_obj *o, int cnt)
{
	int rc;

	smp_mb();
	rc = __atomic_dec_ifmoe(&o->credit, cnt, 0);
	smp_mb();

	return rc;
}

static bool bnx2x_credit_pool_put(struct bnx2x_credit_pool_obj *o, int cnt)
{
	bool rc;

	smp_mb();

	/* Don't let to refill if credit + cnt > pool_sz */
	rc = __atomic_add_ifless(&o->credit, cnt, o->pool_sz + 1);

	smp_mb();

	return rc;
}

static int bnx2x_credit_pool_check(struct bnx2x_credit_pool_obj *o)
{
	int cur_credit;

	smp_mb();
	cur_credit = atomic_read(&o->credit);

	return cur_credit;
}

static bool bnx2x_credit_pool_always_true(struct bnx2x_credit_pool_obj *o,
					  int cnt)
{
	return true;
}

/**
 * Initialize credit pool internals.
 *
 * @param p
 * @param credit Pool size - if negative pool operations will
 *               always succeed (unlimited pool)
 */
static inline void __init_credit_pool(struct bnx2x_credit_pool_obj *p,
				      int credit)
{
	/* Zero the object first */
	memset(p, 0, sizeof(*p));

	atomic_set(&p->credit, credit);
	p->pool_sz = credit;

	/* Commit the change */
	smp_mb();

	p->check = bnx2x_credit_pool_check;

	/* if pool credit is negative - disable the checks */
	if (credit >= 0) {
		p->put = bnx2x_credit_pool_put;
		p->get = bnx2x_credit_pool_get;
	} else {
		p->put = bnx2x_credit_pool_always_true;
		p->get = bnx2x_credit_pool_always_true;
	}
}

/**
 * Calculates the number of active (not hidden) functions on the
 * current path.
 *
 * @param bp Function driver handle. It will be used to define
 *           the current path.
 *
 * @return Number of active (not hidden) functions on the
 *         current path
 */
static inline int __get_path_func_num_e2(struct bnx2x *bp)
{
	int func_num = 0, i;

	/* Calculate a number of functions enabled on the current
	 * PATH.
	 */
	for (i = 0; i < E1H_FUNC_MAX / 2; i++) {
		u32 func_config =
			MF_CFG_RD(bp,
				  func_mf_config[BP_PORT(bp) + 2 * i].
				  config);
		func_num +=
			((func_config & FUNC_MF_CFG_FUNC_HIDE) ? 0 : 1);
	}

	WARN_ON(!func_num);

	return func_num;
}


void bnx2x_init_mac_credit_pool(struct bnx2x *bp,
				struct bnx2x_credit_pool_obj *p)
{
	if (CHIP_IS_E1x(bp)) {
		/* We actually bind the MACs location CAM according to the
		 * MAC type for 57710 and 57711 and there is enough space in
		 * CAM for any possible configuration. Therefore there is no
		 * need for crediting on these chips.
		 */
		__init_credit_pool(p, -1);
	} else {
		int func_num = __get_path_func_num_e2(bp);

		/** CAM credit is equaly divided between all active functions
		 *  on the PATH.
		 */
		if (func_num > 0)
			__init_credit_pool(p, MAX_MAC_CREDIT_E2 / func_num);
		else
			 /* this should never happen! Block MAC operations. */
			__init_credit_pool(p, 0);

	}
}

void bnx2x_init_vlan_credit_pool(struct bnx2x *bp,
				 struct bnx2x_credit_pool_obj *p)
{
	if (CHIP_IS_E1x(bp)) {
		/* There is no VLAN configuration in HW on 57710 and 57711 */
		__init_credit_pool(p, -1);
	} else {
		int func_num = __get_path_func_num_e2(bp);

		/** CAM credit is equaly divided between all active functions
		 *  on the PATH.
		 */
		if (func_num > 0)
			__init_credit_pool(p, MAX_VLAN_CREDIT_E2 / func_num);
		else
			 /* this should never happen! Block VLAN operations. */
			__init_credit_pool(p, 0);
	}
}


/****************** RSS Configuration ******************/
static inline void __storm_read_struct(struct bnx2x *bp, u32 addr, size_t size,
				       u32 *data)
{
	int i;
	for (i = 0; i < size/4; i++)
		data[i] = REG_RD(bp, addr + (i * 4));
}

/**
 * Configure RSS for 57710 and 57711.
 * We are not going to send a ramrod but will write directly
 * into the TSTORM internal memory. We will also configure RSS
 * hash keys configuration in SRC block if this is a PMF.
 *
 * @param bp
 * @param p
 *
 * @return 0
 */
static int bnx2x_setup_rss_e1x(struct bnx2x *bp,
			       struct bnx2x_config_rss_params *p)
{
	struct bnx2x_raw_obj *r = &p->rss_obj->raw;
	struct eth_rss_update_ramrod_data_e1x *data =
		(struct eth_rss_update_ramrod_data_e1x *)(r->rdata);
	u8 rss_mode = 0;
	int i;

	memset(data, 0, sizeof(*data));

	DP(NETIF_MSG_IFUP, "Configuring RSS\n");

	/* RSS mode */
	if (test_bit(BNX2X_RSS_MODE_DISABLED, &p->rss_flags))
		rss_mode = ETH_RSS_MODE_DISABLED;
	else if (test_bit(BNX2X_RSS_MODE_REGULAR, &p->rss_flags))
		rss_mode = ETH_RSS_MODE_REGULAR;
	else if (test_bit(BNX2X_RSS_MODE_VLAN_PRI, &p->rss_flags))
		rss_mode = ETH_RSS_MODE_VLAN_PRI;
	else if (test_bit(BNX2X_RSS_MODE_E1HOV_PRI, &p->rss_flags))
	      rss_mode = ETH_RSS_MODE_E1HOV_PRI;
	else if (test_bit(BNX2X_RSS_MODE_IP_DSCP, &p->rss_flags))
	      rss_mode = ETH_RSS_MODE_IP_DSCP;
	else if (test_bit(BNX2X_RSS_MODE_E2_INTEG, &p->rss_flags))
	      rss_mode = ETH_RSS_MODE_E2_INTEG;

	data->func_config.config_flags |= (rss_mode <<
			TSTORM_ETH_FUNCTION_COMMON_CONFIG_RSS_MODE_SHIFT);

	DP(NETIF_MSG_IFUP, "rss_mode=%d\n", rss_mode);

	/* RSS capabilities */
	if (test_bit(BNX2X_RSS_IPV4, &p->rss_flags))
		data->func_config.config_flags |=
			TSTORM_ETH_FUNCTION_COMMON_CONFIG_RSS_IPV4_CAPABILITY;

	if (test_bit(BNX2X_RSS_IPV4_TCP, &p->rss_flags))
		data->func_config.config_flags |=
		TSTORM_ETH_FUNCTION_COMMON_CONFIG_RSS_IPV4_TCP_CAPABILITY;

	if (test_bit(BNX2X_RSS_IPV6, &p->rss_flags))
		data->func_config.config_flags |=
			TSTORM_ETH_FUNCTION_COMMON_CONFIG_RSS_IPV6_CAPABILITY;

	if (test_bit(BNX2X_RSS_IPV6_TCP, &p->rss_flags))
		data->func_config.config_flags |=
		TSTORM_ETH_FUNCTION_COMMON_CONFIG_RSS_IPV6_TCP_CAPABILITY;

	/* TODO: Remove me!!! */
	/* Set TPA_ENABLE flag */
	if (bp->flags & TPA_ENABLE_FLAG)
		data->func_config.config_flags |=
			TSTORM_ETH_FUNCTION_COMMON_CONFIG_ENABLE_TPA;

	/* Hashing mask */
	data->func_config.rss_result_mask = p->rss_result_mask;

	/* Indirection table */
	memcpy(&data->indirection_table[0], &p->ind_table[0],
	       T_ETH_INDIRECTION_TABLE_SIZE);

	/* RSS keys */
	if (bp->port.pmf) {
		int j;
		u32 start, end;

		if (BP_PORT(bp)) {
			start = SRC_REG_KEYRSS1_0;
			end = SRC_REG_KEYRSS1_9;
		} else {
			start = SRC_REG_KEYRSS0_0;
			end = SRC_REG_KEYRSS0_9;
		}

		WARN_ON(end - start + 4 != sizeof(p->rss_key));

		for (i = start, j = 0; i <= end; i += 4, j++)
			REG_WR(bp, i, p->rss_key[j]);
	}

	/* RSS update flags */
	if (test_bit(BNX2X_RSS_UPDATE_ETH, &p->rss_flags))
		data->rss_config.flags |= RSS_UPDATE_CONFIG_ETH_UPDATE_ENABLE;

	if (test_bit(BNX2X_RSS_UPDATE_TOE, &p->rss_flags)) {
		data->rss_config.flags |= RSS_UPDATE_CONFIG_TOE_UPDATE_ENABLE;
		data->rss_config.toe_rss_bitmap = p->toe_rss_bitmap;
	}

	/* Commit writes towards the memory */
	mb();
	mmiowb();


	/* Send a ramrod */
	return bnx2x_sp_post(bp, RAMROD_CMD_ID_ETH_RSS_UPDATE, r->cid,
			     U64_HI(r->rdata_mapping),
			     U64_LO(r->rdata_mapping), 0);
}

/**
 * Configure RSS for 57712: we will send on UPDATE ramrod for
 * that matter.
 *
 * @param bp
 * @param p
 *
 * @return int
 */
static int bnx2x_setup_rss_e2(struct bnx2x *bp,
			      struct bnx2x_config_rss_params *p)
{
	struct bnx2x_raw_obj *r = &p->rss_obj->raw;
	struct eth_rss_update_ramrod_data_e2 *data =
		(struct eth_rss_update_ramrod_data_e2 *)(r->rdata);

	memset(data, 0, sizeof(*data));

	DP(NETIF_MSG_IFUP, "Configuring RSS\n");

	/* RSS capabilities */
	if (test_bit(BNX2X_RSS_IPV4, &p->rss_flags))
		data->capabilities |= ETH_RSS_UPDATE_RAMROD_DATA_E2_IPV4_CAPABILITY;

	if (test_bit(BNX2X_RSS_IPV4_TCP, &p->rss_flags))
		data->capabilities |=
			ETH_RSS_UPDATE_RAMROD_DATA_E2_IPV4_TCP_CAPABILITY;

	if (test_bit(BNX2X_RSS_IPV6, &p->rss_flags))
		data->capabilities |= ETH_RSS_UPDATE_RAMROD_DATA_E2_IPV6_CAPABILITY;

	if (test_bit(BNX2X_RSS_IPV6_TCP, &p->rss_flags))
		data->capabilities |=
			ETH_RSS_UPDATE_RAMROD_DATA_E2_IPV6_TCP_CAPABILITY;

	/* Hashing mask */
	data->rss_result_mask = p->rss_result_mask;

	/* Indirection table */
	memcpy(&data->indirection_table[0], &p->ind_table[0],
	       T_ETH_INDIRECTION_TABLE_SIZE);

	/* RSS keys */
	memcpy(&data->rss_key[0], &p->rss_key[0], sizeof(data->rss_key));

	/* RSS update flags */
	if (test_bit(BNX2X_RSS_UPDATE_ETH, &p->rss_flags))
		data->rss_config.flags |= RSS_UPDATE_CONFIG_ETH_UPDATE_ENABLE;

	if (test_bit(BNX2X_RSS_UPDATE_TOE, &p->rss_flags)) {
		data->rss_config.flags |= RSS_UPDATE_CONFIG_TOE_UPDATE_ENABLE;
		data->rss_config.toe_rss_bitmap = p->toe_rss_bitmap;
	}

	/* Commit writes towards the memory before sending a ramrod */
	mb();

	/* Send a ramrod */
	return bnx2x_sp_post(bp, RAMROD_CMD_ID_ETH_RSS_UPDATE, r->cid,
			     U64_HI(r->rdata_mapping),
			     U64_LO(r->rdata_mapping), 0);
}

int bnx2x_config_rss(struct bnx2x *bp, struct bnx2x_config_rss_params *p)
{
	int rc;
	struct bnx2x_rss_config_obj *o = p->rss_obj;
	struct bnx2x_raw_obj *r = &o->raw;

	r->set_pending(r);

	rc = o->config_rss(bp, p);
	if (rc) {
		r->clear_pending(r);
		return rc;
	}

	if (test_bit(RAMROD_COMP_WAIT, &p->ramrod_flags)) {
		rc = r->wait_comp(bp, r);
		if (rc)
			return rc;
	}

	return 0;
}


void bnx2x_init_rss_config_obj(struct bnx2x *bp,
			       struct bnx2x_rss_config_obj *rss_obj,
			       u16 cl_id, u32 cid, int func_id, void *rdata,
			       dma_addr_t rdata_mapping, int state,
			       unsigned long *pstate, bnx2x_obj_type type)
{
	__init_raw_obj(&rss_obj->raw, cl_id, cid, func_id, rdata, rdata_mapping,
		       state, pstate, type);

	if (CHIP_IS_E2(bp)) {
		rss_obj->config_rss = bnx2x_setup_rss_e2;
	} else if (CHIP_IS_E1x(bp)) {
		rss_obj->config_rss = bnx2x_setup_rss_e1x;
	} else {
		BNX2X_ERR("Do not support chips others than E1X and E2\n");
		BUG();
	}
}

/********************** Client state update ***********************************/

int bnx2x_fw_cl_update(struct bnx2x *bp,
		       struct bnx2x_client_update_params *params)
{
	struct client_update_ramrod_data *data =
		(struct client_update_ramrod_data *)params->rdata;
	dma_addr_t data_mapping = params->rdata_mapping;
	int rc;

	memset(data, 0, sizeof(*data));

	/* Client ID of the client to update */
	data->client_id = params->cl_id;

	/* Default VLAN value */
	data->default_vlan = cpu_to_le16(params->def_vlan);

	/* Inner VLAN stripping */
	data->inner_vlan_removal_enable_flg =
		test_bit(BNX2X_CL_UPDATE_IN_VLAN_REM, &params->update_flags);
	data->inner_vlan_removal_change_flg =
		test_bit(BNX2X_CL_UPDATE_IN_VLAN_REM_CHNG,
			 &params->update_flags);

	/* Outer VLAN sripping */
	data->outer_vlan_removal_enable_flg =
		test_bit(BNX2X_CL_UPDATE_OUT_VLAN_REM, &params->update_flags);
	data->outer_vlan_removal_change_flg =
		test_bit(BNX2X_CL_UPDATE_OUT_VLAN_REM_CHNG,
			 &params->update_flags);

	/* Drop packets that have source MAC that doesn't belong to this
	 * Client.
	 */
	data->anti_spoofing_enable_flg =
		test_bit(BNX2X_CL_UPDATE_ANTI_SPOOF, &params->update_flags);
	data->anti_spoofing_change_flg =
		test_bit(BNX2X_CL_UPDATE_ANTI_SPOOF_CHNG,
			 &params->update_flags);

	/* Activate/Deactivate */
	data->activate_flg =
		test_bit(BNX2X_CL_UPDATE_ACTIVATE, &params->update_flags);
	data->activate_change_flg =
		test_bit(BNX2X_CL_UPDATE_ACTIVATE_CHNG, &params->update_flags);

	/* Enable default VLAN */
	data->default_vlan_enable_flg =
		test_bit(BNX2X_CL_UPDATE_DEF_VLAN_EN, &params->update_flags);
	data->default_vlan_change_flg =
		test_bit(BNX2X_CL_UPDATE_DEF_VLAN_EN_CHNG,
			 &params->update_flags);

	/* Set "pending" bit */
	set_bit(BNX2X_FILTER_CL_UPDATE_PENDING, &bp->sp_state);

	/* Send a ramrod */
	rc = bnx2x_sp_post(bp, RAMROD_CMD_ID_ETH_CLIENT_UPDATE, params->cid,
		      U64_HI(data_mapping), U64_LO(data_mapping), 0);
	if (rc)
		return rc;

	/* Wait for completion */
	if (!bnx2x_wait_sp_comp(bp))
		return -EAGAIN;

	return 0;
}

