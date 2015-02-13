/* bnx2x_sp_verbs.h: Broadcom Everest network driver.
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
 * Written by: Vladislav Zolotarov
 *
 */
#ifndef BNX2X_SP_VERBS
#define BNX2X_SP_VERBS

struct bnx2x;
struct bnx2x_fastpath;

enum {
	RAMROD_TX,
	RAMROD_RX,
	RAMROD_COMP_WAIT,
	RAMROD_DRV_CLR_ONLY,
};

typedef enum {
	BNX2X_OBJ_TYPE_RX,
	BNX2X_OBJ_TYPE_TX,
	BNX2X_OBJ_TYPE_RX_TX,
} bnx2x_obj_type;

/* Filtering states */
enum {
	BNX2X_FILTER_MAC_PENDING,
	BNX2X_FILTER_VLAN_PENDING,
	BNX2X_FILTER_VLAN_MAC_PENDING,
	BNX2X_FILTER_RX_MODE_PENDING,
	BNX2X_FILTER_RX_MODE_SCHED,
	BNX2X_FILTER_ISCSI_ETH_START_SCHED,
	BNX2X_FILTER_ISCSI_ETH_STOP_SCHED,
	BNX2X_FILTER_MCAST_PENDING,
	BNX2X_FILTER_MCAST_SCHED,
	BNX2X_FILTER_RSS_CONF_PENDING,
	BNX2X_FILTER_CL_UPDATE_PENDING,
	BNX2X_FILTER_TX_SWITCH_MAC_PENDING,
};

struct bnx2x_raw_obj {
	int		func_id;

	/* Client params */
	u16		cl_id;
	u32		cid;

	/* Ramrod data buffer params */
	void 		*rdata;
	dma_addr_t	rdata_mapping;

	/* Ramrod state params */
	int		state;   /* "ramrod is pending" state bit */
	unsigned long	*pstate; /* pointer to state buffer */

	bnx2x_obj_type	obj_type;

	int (*wait_comp)(struct bnx2x *bp,
			 struct bnx2x_raw_obj *o);

	bool (*check_pending)(struct bnx2x_raw_obj *o);
	void (*clear_pending)(struct bnx2x_raw_obj *o);
	void (*set_pending)(struct bnx2x_raw_obj *o);
};

/***************** Classification verbs: Set/Del MAC/VLAN/VLAN-MAC ************/

/**
 *  If entry is not NULL, it's valid - query on it.
 */
struct bnx2x_mac_list_query {
	u8 *mac;
	u8 *cl_id;
};

struct bnx2x_vlan_list_query {
	u16 *vlan;
	u8  *cl_id;
};

struct bnx2x_vlan_mac_list_query {
	u16 *vlan;
	u8  *mac;
	u8  *cl_id;
};

union bnx2x_list_query {
	struct bnx2x_mac_list_query mac;
	struct bnx2x_vlan_list_query vlan;
	struct bnx2x_vlan_mac_list_query vlan_mac;
};

struct bnx2x_mac_ramrod_data {
	u8 mac[ETH_ALEN];
};

struct bnx2x_vlan_ramrod_data {
	u16 vlan;
};

struct bnx2x_vlan_mac_ramrod_data {
	u8 mac[ETH_ALEN];
	u16 vlan;
};

/* TODO: Come up with a better name */
struct bnx2x_list_elem {
	struct list_head link;
	union {
		struct bnx2x_mac_ramrod_data mac;
		struct bnx2x_vlan_ramrod_data vlan;
		struct bnx2x_vlan_mac_ramrod_data vlan_mac;
	} data;
};


/* VLAN_MAC specific flags */
enum {
	BNX2X_ETH_MAC,
	BNX2X_BCAST_MAC,
	BNX2X_ISCSI_ETH_MAC,
	BNX2X_NETQ_ETH_MAC,
	BNX2X_DONT_CONSUME_CAM_CREDIT,
};

struct bnx2x_vlan_mac_ramrod_params {
	struct bnx2x_vlan_mac_obj *vlan_mac_obj;
	unsigned long ramrod_flags;
	unsigned long vlan_mac_flags;

	union {
		struct bnx2x_mac_ramrod_data mac;
		struct bnx2x_vlan_ramrod_data vlan;
		struct bnx2x_vlan_mac_ramrod_data vlan_mac;
	} data;
};

struct bnx2x_vlan_mac_obj {
	struct bnx2x_raw_obj raw;

	/* Bookkeeping list: will prevent the addition of already existing
	 * entries.
	 */
	struct list_head	head;

	/* MACs credit pool */
	struct bnx2x_credit_pool_obj *macs_pool;

	/* VLANs credit pool */
	struct bnx2x_credit_pool_obj *vlans_pool;

	/**
	 * Checks if ADD-ramrod with the given params may be performed.
	 *
	 * @return true if the element may be added
	 */

	bool (*check_add)(struct bnx2x_vlan_mac_ramrod_params *p);

	/**
	 * Checks if DEL-ramrod with the given params may be performed.
	 *
	 * @return true if the element may be deleted
	 */
	struct bnx2x_list_elem *
		(*check_del)(struct bnx2x_vlan_mac_ramrod_params *p);

	/**
	 *  Update the relevant credit object(s) (consume/return
	 *  correspondingly).
	 */
	bool (*get_credit)(struct bnx2x_vlan_mac_obj *o);
	bool (*put_credit)(struct bnx2x_vlan_mac_obj *o);

	/**
	 * @param add Set to 0 if DELETE rule is requested
	 */
	int (*config_rule)(struct bnx2x *bp,
			   struct bnx2x_vlan_mac_ramrod_params *p,
			   struct bnx2x_list_elem *pos,
			   bool add);
};

/*  RX_MODE verbs:DROP_ALL/ACCEPT_ALL/ACCEPT_ALL_MULTI/ACCEPT_ALL_VLAN/NORMA */

/* RX_MODE ramrod spesial flags: set in rx_mode_flags field in
 * a bnx2x_rx_mode_ramrod_params.
 */
enum {
	BNX2X_RX_MODE_FCOE_ETH,
};

enum {
	BNX2X_ACCEPT_UNICAST,
	BNX2X_ACCEPT_MULTICAST,
	BNX2X_ACCEPT_ALL_UNICAST,
	BNX2X_ACCEPT_ALL_MULTICAST,
	BNX2X_ACCEPT_BROADCAST,
	BNX2X_ACCEPT_UNMATCHED,
	BNX2X_ACCEPT_ANY_VLAN
};
struct bnx2x_rx_mode_ramrod_params {
	struct bnx2x_rx_mode_obj *rx_mode_obj;
	unsigned long *pstate;
	int state;
	u16 cl_id;
	u32 cid;
	int func_id;
	unsigned long ramrod_flags;
	unsigned long rx_mode_flags;
	/* rdata is either a pointer to eth_filter_rules_ramrod_data(e2) or to
	 * a tstorm_eth_mac_filter_config (e1x).
	 * */
	void *rdata;
	dma_addr_t rdata_mapping;
	unsigned long accept_flags;
};

struct bnx2x_rx_mode_obj {
	int (*config_rx_mode)(struct bnx2x *bp,
			      struct bnx2x_rx_mode_ramrod_params *p);

	int (*wait_comp)(struct bnx2x *bp,
			 struct bnx2x_rx_mode_ramrod_params *p);
};

/********************** Set multicast group ***********************************/

struct bnx2x_mcast_list_elem {
	struct list_head link;
	u8 *mac;
};

struct bnx2x_mcast_ramrod_params {
	struct bnx2x_mcast_obj *mcast_obj;

	/* Relevant options are RAMROD_COMP_WAIT and RAMROD_DRV_CLR_ONLY */
	unsigned long ramrod_flags;

	struct list_head mcast_list; /* list of struct bnx2x_mcast_list_elem */
	/** TODO:
	 *      - rename it to macs_num.
	 *      - Add a new command type for handling pending commands
	 *        (remove "zero semantics").
	 *
	 *  Length of mcast_list. If zero and ADD_CONT command - post
	 *  pending commands.
	 */
	int mcast_list_len;
};

struct bnx2x_mcast_obj {
	struct bnx2x_raw_obj raw;

#define BNX2X_MCAST_BINS_NUM	256
#define BNX2X_MCAST_VEC_SZ	(BNX2X_MCAST_BINS_NUM / 64)
	u64		vec[BNX2X_MCAST_VEC_SZ];

	/** Number of BINs to clear. Should be updated immediately
	 *  when a command arrives in order to properly create DEL
	 *  commands.
	 */
	int num_bins_set;

	/* Pending commands */
	struct		list_head pending_cmds_head;

	/* A state that is set in raw.pstate, when there are pending commands */
	int sched_state;

	/* Maximal number of mcast MACs configured in one command */
	int max_cmd_len;

	/* Total number of currently pending MACs to configure: both
	 * in the pending commands list and in the current command.
	 */
	int total_pending_num;

	/**
	 * @param add if true - add a new multicast group, otherwise
	 *            clears up the previous mcast configuration.
	 */
	int (*config_mcast)(struct bnx2x *bp,
			    struct bnx2x_mcast_ramrod_params *p,
			    bool add);

	int (*enqueue_cmd)(struct bnx2x *bp, struct bnx2x_mcast_obj *o,
			   struct bnx2x_mcast_ramrod_params *p,
			   bool add);

	void (*set_one_rule)(struct bnx2x *bp,
			     struct bnx2x_mcast_ramrod_params *p, int idx,
			     u8 *mac, bool add);

	/** Checks if there are more mcast MACs to be set or a previous
	 *  command is still pending.
	 */
	bool (*check_pending)(struct bnx2x_mcast_obj *o);

	/**
	 * Set/Clear/Check SCHEDULED state of the object
	 */
	void (*set_sched)(struct bnx2x_mcast_obj *o);
	void (*clear_sched)(struct bnx2x_mcast_obj *o);
	bool (*check_sched)(struct bnx2x_mcast_obj *o);

	/* Wait until all pending commands complete */
	int (*wait_comp)(struct bnx2x *bp, struct bnx2x_mcast_obj *o);

	/**
	 * Handle the internal object counters needed for proper
	 * commands handling. Checks that the provided parameters are
	 * feasible.
	 */
	int (*preamble)(struct bnx2x *bp, struct bnx2x_mcast_ramrod_params *p,
			bool add_cont);

	/**
	 * Restore the values of internal counters in case of a failure.
	 */
	void (*postmortem)(struct bnx2x *bp,
			   struct bnx2x_mcast_ramrod_params *p,
			   int old_num_bins);
};

/*************************** Credit handling **********************************/
struct bnx2x_credit_pool_obj {

	/* Current amount of credit in the pool */
	atomic_t		credit;

	/* Maximum allowed credit. put() will check against it. */
	int			pool_sz;

	/**
	 * Get the requested amount of credit from the pool.
	 *
	 * @param cnt Amount of requested credit
	 * @return TRUE if the operation is successful
	 */
	bool (*get)(struct bnx2x_credit_pool_obj *o, int cnt);

	/**
	 * Returns the credit to the pool.
	 *
	 * @param cnt Amount of credit to return
	 * @return TRUE if the operation is successful
	 */
	bool (*put)(struct bnx2x_credit_pool_obj *o, int cnt);

	/**
	 * Reads the current amount of credit.
	 */
	int (*check)(struct bnx2x_credit_pool_obj *o);
};

/*************************** RSS configuration ********************************/
enum {
	/* RSS_MODE bits are mutually exclusive */
	BNX2X_RSS_MODE_DISABLED,
	BNX2X_RSS_MODE_REGULAR,
	BNX2X_RSS_MODE_VLAN_PRI,
	BNX2X_RSS_MODE_E1HOV_PRI,
	BNX2X_RSS_MODE_IP_DSCP,
	BNX2X_RSS_MODE_E2_INTEG,

	BNX2X_RSS_IPV4,
	BNX2X_RSS_IPV4_TCP,
	BNX2X_RSS_IPV6,
	BNX2X_RSS_IPV6_TCP,

	BNX2X_RSS_UPDATE_ETH,
	BNX2X_RSS_UPDATE_TOE,
};

struct bnx2x_config_rss_params {
	struct bnx2x_rss_config_obj *rss_obj;

	/* may have RAMROD_COMP_WAIT set only */
	unsigned long	ramrod_flags;

	/* BNX2X_RSS_X bits */
	unsigned long	rss_flags;

	/* Number hash bits to take into an account */
	u8		rss_result_mask;

	/* Indirection table */
	u8		ind_table[T_ETH_INDIRECTION_TABLE_SIZE];

	/* RSS hash values */
	u32		rss_key[10];

	/* valid only iff BNX2X_RSS_UPDATE_TOE is set */
	u16		toe_rss_bitmap;
};

struct bnx2x_rss_config_obj {
	struct bnx2x_raw_obj raw;

	int (*config_rss)(struct bnx2x *bp,
			  struct bnx2x_config_rss_params *p);
};

/********************** Client state update ***********************************/
enum {
	BNX2X_CL_UPDATE_IN_VLAN_REM,
	BNX2X_CL_UPDATE_IN_VLAN_REM_CHNG,
	BNX2X_CL_UPDATE_OUT_VLAN_REM,
	BNX2X_CL_UPDATE_OUT_VLAN_REM_CHNG,
	BNX2X_CL_UPDATE_ANTI_SPOOF,
	BNX2X_CL_UPDATE_ANTI_SPOOF_CHNG,
	BNX2X_CL_UPDATE_ACTIVATE,
	BNX2X_CL_UPDATE_ACTIVATE_CHNG,
	BNX2X_CL_UPDATE_DEF_VLAN_EN,
	BNX2X_CL_UPDATE_DEF_VLAN_EN_CHNG,
};

struct bnx2x_client_update_params {
	unsigned long	update_flags;
	u32		cid;
	u16		def_vlan;
	u8		cl_id;

	void 		*rdata;
	dma_addr_t	rdata_mapping;
};


/********************** Interfaces ********************************************/
/********************* VLAN-MAC ****************/
void bnx2x_init_mac_obj(struct bnx2x *bp,
			struct bnx2x_vlan_mac_obj *mac_obj,
			u16 cl_id, u32 cid, int func_id, void *rdata,
			dma_addr_t rdata_mapping, int state,
			unsigned long *pstate, bnx2x_obj_type type,
			struct bnx2x_credit_pool_obj *macs_pool);

void bnx2x_init_vlan_obj(struct bnx2x *bp,
			 struct bnx2x_vlan_mac_obj *vlan_obj,
			 u16 cl_id, u32 cid, int func_id, void *rdata,
			 dma_addr_t rdata_mapping, int state,
			 unsigned long *pstate, bnx2x_obj_type type,
			 struct bnx2x_credit_pool_obj *vlans_pool);

void bnx2x_init_vlan_mac_obj(struct bnx2x *bp,
			     struct bnx2x_vlan_mac_obj *vlan_mac_obj,
			     u16 cl_id, u32 cid, int func_id, void *rdata,
			     dma_addr_t rdata_mapping, int state,
			     unsigned long *pstate, bnx2x_obj_type type,
			     struct bnx2x_credit_pool_obj *macs_pool,
			     struct bnx2x_credit_pool_obj *vlans_pool);

int bnx2x_config_vlan_mac(struct bnx2x *bp,
			  struct bnx2x_vlan_mac_ramrod_params *p, bool add);

/********************* RX MODE ****************/

void bnx2x_init_rx_mode_obj(struct bnx2x *bp, struct bnx2x_rx_mode_obj *o);

int bnx2x_config_rx_mode(struct bnx2x *bp, struct bnx2x_rx_mode_ramrod_params *p);

/****************** MULTICASTS ****************/

void bnx2x_init_mcast_obj(struct bnx2x *bp,
			  struct bnx2x_mcast_obj *mcast_obj,
			  u16 mcast_cl_id, u32 mcast_cid, int func_id,
			  void *rdata, dma_addr_t rdata_mapping, int state,
			  unsigned long *pstate, bnx2x_obj_type type);

/**
 * Configure multicast MACs list. May configure a new list
 * provided in p->mcast_list or clean up a current
 * configuration. Also if add_cont is TRUE and p->mcast_list_len
 * is 0 will run commands from the pending list.
 *
 * If previous command is still pending or if number of MACs to
 * configure is more that maximum number of MACs in one command,
 * the current command will be enqueued to the tail of the
 * pending commands list.
 *
 * @param bp
 * @param p
 * @param add_cont if TRUE, configure a new MACs list or
 *                 continue to run pending commands (see above).
 *
 * @return 0 in case of success
 */
int bnx2x_config_mcast(struct bnx2x *bp,
		       struct bnx2x_mcast_ramrod_params *p,
		       bool add_cont);

/****************** CREDIT POOL ****************/
void bnx2x_init_mac_credit_pool(struct bnx2x *bp,
				struct bnx2x_credit_pool_obj *p);
void bnx2x_init_vlan_credit_pool(struct bnx2x *bp,
				 struct bnx2x_credit_pool_obj *p);

/****************** RSS CONFIGURATION ****************/
void bnx2x_init_rss_config_obj(struct bnx2x *bp,
			       struct bnx2x_rss_config_obj *rss_obj,
			       u16 cl_id, u32 cid, int func_id, void *rdata,
			       dma_addr_t rdata_mapping, int state,
			       unsigned long *pstate, bnx2x_obj_type type);

/**
 * Updates RSS configuration according to provided parameters.
 *
 * @param bp
 * @param p
 *
 * @return 0 in case of success
 */
int bnx2x_config_rss(struct bnx2x *bp, struct bnx2x_config_rss_params *p);

/****************** CLIENT STATE UPDATE ****************/

/**
 * Update a state of the existing Client according to the
 * provided parameters.
 *
 * @param bp
 * @param params Set of Client parameters to update.
 *
 * @return int
 */
int bnx2x_fw_cl_update(struct bnx2x *bp,
		       struct bnx2x_client_update_params *params);


#endif /* BNX2X_SP_VERBS */
