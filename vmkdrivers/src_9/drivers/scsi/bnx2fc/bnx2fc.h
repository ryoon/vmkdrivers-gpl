/*
 * QLogic NetXtreme II Linux FCoE offload driver.
 * Copyright (c)   2003-2014 QLogic Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * Written by: Bhanu Prakash Gollapudi (bprakash@broadcom.com)
 */

#ifndef BNX2FC_H
#define BNX2FC_H

#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/kthread.h>
#include <linux/crc32.h>
#ifndef __VMKLNX__
#include <linux/cpu.h>
#endif

#include <linux/types.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/bitops.h>
#include <linux/log2.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/io.h>

#include <scsi/scsi.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_eh.h>
#include <scsi/scsi_tcq.h>
#include <scsi/libfc.h>
#include <scsi/libfcoe.h>
#include <scsi/fc_frame.h>
#ifdef __VMKLNX__
#include <scsi/fcoe_compat.h>
#include <linux/dcbnl.h>
#endif

#include <scsi/fc/fc_fcoe.h>

#include <scsi/scsi_transport.h>
#include <scsi/scsi_transport_fc.h>
#include <scsi/fc/fc_fip.h>
#include <scsi/fc/fc_fc2.h>
#include <scsi/fc_frame.h>
#include <scsi/fc/fc_fcoe.h>
#include <scsi/fc/fc_fcp.h>

#include "57xx_hsi_bnx2fc.h"

#include "bnx2fc_debug.h"
#ifdef __VMKLNX__
#include "cnic_if.h"
#include "bnx2x_mfw_req.h"
#else
#include "../../bnx2/src/cnic_if.h"
#endif

/*
 * Multi-function modes
 */
enum mf_mode {
	SINGLE_FUNCTION,
	MULTI_FUNCTION_SD,
	MULTI_FUNCTION_SI,
	MULTI_FUNCTION_AFEX,
	MAX_MF_MODE};

#include "57xx_fcoe_constants.h"

#define BNX2FC_NAME		"bnx2fc"
#define BNX2FC_VERSION	"1.78.78.v60.8"
#define PFX			"bnx2fc: "

#ifndef DEFINE_PCI_DEVICE_TABLE
#define DEFINE_PCI_DEVICE_TABLE(_table) struct pci_device_id _table[]
#endif
#ifndef PCI_DEVICE_ID_NX2_57712
#define PCI_DEVICE_ID_NX2_57712		0x1662
#endif
#ifndef PCI_DEVICE_ID_NX2_57712E
#define PCI_DEVICE_ID_NX2_57712E	0x1663
#endif
#ifndef PCI_DEVICE_ID_NX2_57800
#define PCI_DEVICE_ID_NX2_57800		0x168a
#endif
#ifndef PCI_DEVICE_ID_NX2_57800_MF
#define PCI_DEVICE_ID_NX2_57800_MF	0x16a5
#endif
#ifndef PCI_DEVICE_ID_NX2_57810
#define PCI_DEVICE_ID_NX2_57810		0x168e
#endif
#ifndef PCI_DEVICE_ID_NX2_57810_MF
#define PCI_DEVICE_ID_NX2_57810_MF	0x16ae
#endif
#ifndef PCI_DEVICE_ID_NX2_57840_4_10
#define PCI_DEVICE_ID_NX2_57840_4_10    0x16a1
#endif
#ifndef PCI_DEVICE_ID_NX2_57840_2_20
#define PCI_DEVICE_ID_NX2_57840_2_20    0x16a2
#endif
#ifndef PCI_DEVICE_ID_NX2_57840_MF
#define PCI_DEVICE_ID_NX2_57840_MF      0x16a4
#endif

/*
 *  On ESX the wmb() instruction is defined to only a compiler barrier
 *  The macro wmb() need to be overrode to properly synchronize memory
 */
#if defined(__VMKLNX__)
#undef wmb
#define wmb()   asm volatile("sfence" ::: "memory")
#endif

#define BNX2X_DB_SHIFT				3
#define BNX2X_DOORBELL_PCI_BAR                  2

#define BNX2FC_MAX_BD_LEN		0xffff
#define BNX2FC_BD_SPLIT_SZ		0x8000
#define BNX2FC_MAX_BDS_PER_CMD		256
#define MAX_PAGES_PER_EXCHG_CTX_POOL	1024

#define BNX2FC_SQ_WQES_MAX	1024

#define BNX2FC_SCSI_MAX_SQES	((3 * BNX2FC_SQ_WQES_MAX) / 8)
#define BNX2FC_TM_MAX_SQES	((BNX2FC_SQ_WQES_MAX) / 2)
#define BNX2FC_ELS_MAX_SQES	(BNX2FC_TM_MAX_SQES - 1)

#define BNX2FC_RQ_WQES_MAX	16
#define BNX2FC_CQ_WQES_MAX	(BNX2FC_SQ_WQES_MAX + BNX2FC_RQ_WQES_MAX)

#define BNX2FC_MAX_SESS		2048
#define BNX2FC_NUM_MAX_SESS	128
#define BNX2FC_NUM_MAX_SESS_LOG	(ilog2(BNX2FC_NUM_MAX_SESS))

#ifdef __VMKLNX__
#define BNX2FC_MAX_OUTSTANDING_CMNDS	2048
#define BNX2FC_INITIAL_POOLSIZE         ((1 * VMK_MEGABYTE) >> PAGE_SHIFT)
#define BNX2FC_MAX_POOLSIZE_5771X       ((24 * VMK_MEGABYTE) >> PAGE_SHIFT)
#else	/* __VMKLNX__ */
#ifdef CONFIG_X86_64
#define BNX2FC_MAX_OUTSTANDING_CMNDS	4096
#else
#define BNX2FC_MAX_OUTSTANDING_CMNDS	1024
#endif
#endif	/* __VMKLNX__ */

#define BNX2FC_MAX_NPIV			0

#define BNX2FC_MIN_PAYLOAD		256
#define BNX2FC_MAX_PAYLOAD		2048

#define BNX2FC_RQ_BUF_SZ		256
#define BNX2FC_RQ_BUF_LOG_SZ		(ilog2(BNX2FC_RQ_BUF_SZ))

#define BNX2FC_SQ_WQE_SIZE		(sizeof(struct fcoe_sqe))
#define BNX2FC_CQ_WQE_SIZE		(sizeof(struct fcoe_cqe))
#define BNX2FC_RQ_WQE_SIZE		(BNX2FC_RQ_BUF_SZ)
#define BNX2FC_LCQ_WQE_SIZE		(sizeof(struct fcoe_lcqe))
#define BNX2FC_XFERQ_WQE_SIZE		(sizeof(struct fcoe_xfrqe))
#define BNX2FC_CONFQ_WQE_SIZE		(sizeof(struct fcoe_confqe))
#define BNX2FC_5771X_DB_PAGE_SIZE	128

#define BNX2FC_MAX_TASKS		BNX2FC_MAX_OUTSTANDING_CMNDS
#define BNX2FC_TASK_SIZE		128
/*#define BNX2FC_TASK_SIZE		(sizeof(struct fcoe_task_ctx_entry)) */
#define	BNX2FC_TASKS_PER_PAGE		(PAGE_SIZE/BNX2FC_TASK_SIZE)
#define BNX2FC_TASK_CTX_ARR_SZ		(BNX2FC_MAX_TASKS/BNX2FC_TASKS_PER_PAGE)

#define BNX2FC_MAX_ROWS_IN_HASH_TBL	8
#define BNX2FC_HASH_TBL_CHUNK_SIZE	(16 * 1024)

#define BNX2FC_MAX_SEQS			255
#define BNX2FC_MAX_RETRY_CNT		3
#define BNX2FC_MAX_RPORT_RETRY_CNT	255

#define BNX2FC_READ			(1 << 1)
#define BNX2FC_WRITE			(1 << 0)

#define BNX2FC_MIN_XID			0
#define BNX2FC_MAX_XID			(BNX2FC_MAX_OUTSTANDING_CMNDS - 1)
#define FCOE_MIN_XID			(BNX2FC_MAX_OUTSTANDING_CMNDS)
#define FCOE_MAX_XID		\
			(BNX2FC_MAX_OUTSTANDING_CMNDS + (nr_cpu_ids * 256))
#define BNX2FC_MAX_LUN			0xFFFF
#define BNX2FC_MAX_FCP_TGT		256
#define BNX2FC_MAX_CMD_LEN		16

#define BNX2FC_TM_TIMEOUT		10	/* secs  */
#define BNX2FC_IO_TIMEOUT		20000UL	/* msecs */

#define BNX2FC_WAIT_CNT			120
#define BNX2FC_FW_TIMEOUT		(3 * HZ)

#define PORT_MAX			2

#define CMD_SCSI_STATUS(Cmnd)		((Cmnd)->SCp.Status)

/* FC FCP Status */
#define	FC_GOOD				0

#define BNX2FC_RNID_HBA			0x7


#define BNX2FC_MIN_PAYLOAD              256
#define BNX2FC_MAX_PAYLOAD              2048
#define BNX2FC_MFS                      \
                        (BNX2FC_MAX_PAYLOAD + sizeof(struct fc_frame_header))
#define BNX2FC_MINI_JUMBO_MTU           2500

#define BNX2FC_RELOGIN_WAIT_TIME        200
#define BNX2FC_RELOGIN_WAIT_CNT         10

#define BNX2FC_SYM_NAME_LEN		16

struct bnx2fc_global_s {
	struct task_struct *l2_thread;
	struct sk_buff_head fcoe_rx_list;
	struct page *crc_eof_page;
	int crc_eof_offset;
};
extern struct bnx2fc_global_s bnx2fc_global;

struct bnx2fc_hba {
	struct list_head 		link;
	struct cnic_dev 		*cnic;

	struct bnx2fc_cmd_mgr 		*cmd_mgr;
	struct workqueue_struct		*timer_work_queue;

	/* Active list of offloaded sessions */
	struct bnx2fc_rport *tgt_ofld_list[BNX2FC_NUM_MAX_SESS];
	int num_ofld_sess;

	struct fcoe_task_ctx_entry **task_ctx;

	struct kref			kref;
	spinlock_t 			hba_lock;
	struct mutex 			hba_mutex;
	unsigned long 			adapter_state;
		#define ADAPTER_STATE_UP		0
		#define ADAPTER_STATE_GOING_DOWN	1
		#define ADAPTER_STATE_LINK_DOWN		2
		#define ADAPTER_STATE_READY		3
		#define ADAPTER_STATE_INIT_FAILED	31
	u32 flags;
		#define BNX2FC_FLAG_DESTROY_CMPL	1
	u32 flags2;
		#define BNX2FC_CNIC_REGISTERED          1
		#define BXN2FC_VLAN_ENABLED		2
		#define BNX2FC_CNA_QUEUES_ALLOCED	3

	unsigned long init_done;
		#define BNX2FC_FW_INIT_DONE		0
		#define BNX2FC_CTLR_INIT_DONE		1
		#define BNX2FC_CREATE_DONE		2

	struct pci_dev 			*pcidev;
	struct net_device 		*netdev;
	struct net_device 		*phys_dev;
#ifdef __VMKLNX__
	struct net_device 		*fcoe_net_dev;
#endif

	struct fcoe_ctlr ctlr;
	__u8 granted_mac[ETH_ALEN];
	__u8 ffa_fcoe_mac[ETH_ALEN];
#define BNX2FC_ADAPTER_FFA			1
#define BNX2FC_ADAPTER_BOOT			2
	unsigned long adapter_type;
	int vlan_id;
	u32 next_conn_id;

	dma_addr_t *task_ctx_dma;
	struct regpair *task_ctx_bd_tbl;
	dma_addr_t task_ctx_bd_dma;

	int hash_tbl_segment_count;
	void **hash_tbl_segments;
	void *hash_tbl_pbl;
	dma_addr_t hash_tbl_pbl_dma;
	struct fcoe_t2_hash_table_entry *t2_hash_tbl;
	dma_addr_t t2_hash_tbl_dma;
	char *t2_hash_tbl_ptr;
	dma_addr_t t2_hash_tbl_ptr_dma;

	char *dummy_buffer;
	dma_addr_t dummy_buf_dma;

	struct fcoe_statistics_params *stats_buffer;
	dma_addr_t stats_buf_dma;
#ifdef __FCOE_IF_RESTART_WQ__
	struct delayed_work fcoe_disc_work; /* timer for ULP timeouts */
#endif

	/*
	 * PCI related info.
	 */
	u16 pci_did;
	u16 pci_vid;
	u16 pci_sdid;
	u16 pci_svid;
	u16 pci_func;
	u16 pci_devno;

	struct task_struct *l2_thread;
	struct sk_buff_head fcoe_rx_list;

	/* linkdown handling */
	wait_queue_head_t shutdown_wait;
	int wait_for_link_down;

	/*destroy handling */
	struct timer_list destroy_timer;
	wait_queue_head_t destroy_wait;


	/* statistics */
	struct completion stat_req_done;
	struct fcoe_capabilities fcoe_cap;
#ifdef __VMKLNX__
	vmk_MemPool bnx2fc_dma_pool;
#endif

	u64 vlan_grp_unset_drop_stat;
	u64 vlan_invalid_drop_stat;
	u32 num_max_frags;

	u64 cna_init_queue;
	u64 cna_clean_queue;

	struct packet_type 		fcoe_packet_type;
	struct packet_type 		fip_packet_type;

#ifdef __FCOE_IF_RESTART_WQ__
	struct workqueue_struct		*fcoe_if_restart_wq;
#endif

	int io_cmpl_abort_race;
	int io_cmpl_abort_race2;

	int ffa_cvl_fix;
	int ffa_cvl_fix_alloc;
	int ffa_cvl_fix_alloc_err;

	char sym_name[BNX2FC_SYM_NAME_LEN];	/* Symbolic Name */

	struct sk_buff *last_recv_skb;
	u64 drop_d_id_mismatch;
	u64 drop_fpma_mismatch;
	u64 drop_vn_port_zero;
	u64 drop_wrong_source_mac;
	struct sk_buff *last_vn_port_zero;
	u64 drop_abts_seq_ex_ctx_set;
};

struct bnx2fc_port {
	struct bnx2fc_hba *hba;
	struct fc_lport *lport;
	struct timer_list timer;
	struct work_struct destroy_work;
	struct sk_buff_head fcoe_pending_queue;
	struct device 			dummy_dev;
	u8 fcoe_pending_queue_active;
	u8 data_src_addr[ETH_ALEN];
};

#define bnx2fc_from_ctlr(fip) 	container_of(fip, struct bnx2fc_hba, ctlr)
	 
struct bnx2fc_cmd_mgr {
	struct bnx2fc_hba *hba;
	spinlock_t cmgr_lock;
	u16 next_idx;
	struct bnx2fc_cmd *bnx2fc_cmd_pool;
	struct list_head free_list;
	struct io_bdt **io_bdt_pool;
	struct bnx2fc_cmd **cmds;
};

struct bnx2fc_rport {
	struct bnx2fc_port *port;
	struct fc_rport *rport;
	struct fc_rport_priv *rdata;
#define DPM_TRIGER_TYPE		0x40
	u32 fcoe_conn_id;
	u32 context_id;
	u32 sid;

	unsigned long flags;
#define BNX2FC_FLAG_SESSION_READY	0x1
#define BNX2FC_FLAG_OFFLOADED		0x2
#define BNX2FC_FLAG_DISABLED		0x3
#define BNX2FC_FLAG_DESTROYED		0x4
#define BNX2FC_FLAG_OFLD_REQ_CMPL	0x5
#define BNX2FC_FLAG_CTX_ALLOC_FAILURE	0x6
#define BNX2FC_FLAG_UPLD_REQ_COMPL	0x7
#define BNX2FC_FLAG_EXPL_LOGO		0x8
#define BNX2FC_FLAG_DISABLE_FAILED	0x9

	u32 max_sqes;
	u32 max_cqes;
	atomic_t free_sqes;

	struct b577xx_doorbell_set_prod sq_db;
	void __iomem *ctx_base;

	struct fcoe_sqe *sq;
	struct fcoe_sqe *cur_sq;
	u16 sq_prod_idx;
	u16 sq_curr_toggle_bit;
	u32 sq_mem_size;

	struct fcoe_cqe *cq;
	u16 cq_cons_idx;
	u16 cq_curr_toggle_bit;
	u32 cq_mem_size;

	spinlock_t cq_lock;

	struct list_head active_cmd_queue;

	spinlock_t tgt_lock;
	atomic_t num_active_ios;
	u32 flush_in_prog;
	struct list_head els_queue;
	struct list_head io_retire_queue;
	struct list_head active_tm_queue;

	struct timer_list ofld_timer;
	wait_queue_head_t ofld_wait;

	struct timer_list upld_timer;
	wait_queue_head_t upld_wait;

	dma_addr_t cq_dma;
	dma_addr_t sq_dma;

	struct b577xx_fcoe_rx_doorbell rx_db;
	u32 max_rqes;

	void *rq;
	dma_addr_t rq_dma;
	u32 rq_prod_idx;
	u32 rq_cons_idx;
	u32 rq_mem_size;

	void *rq_pbl;
	dma_addr_t rq_pbl_dma;
	u32 rq_pbl_size;

	struct fcoe_xfrqe *xferq;
	dma_addr_t xferq_dma;
	u32 xferq_mem_size;

	struct fcoe_confqe *confq;
	dma_addr_t confq_dma;
	u32 confq_mem_size;

	void *confq_pbl;
	dma_addr_t confq_pbl_dma;
	u32 confq_pbl_size;

	struct fcoe_conn_db *conn_db;
	dma_addr_t conn_db_dma;
	u32 conn_db_mem_size;

	struct fcoe_sqe *lcq;
	dma_addr_t lcq_dma;
	u32 lcq_mem_size;

	void *ofld_req[4];
	dma_addr_t ofld_req_dma[4];
	void *enbl_req;
	dma_addr_t enbl_req_dma;
};

struct bnx2fc_mp_req {
	u8 tm_flags;

	u32 req_len;
	void *req_buf;
	dma_addr_t req_buf_dma;
	struct fcoe_bd_ctx *mp_req_bd;
	dma_addr_t mp_req_bd_dma;
	struct fc_frame_header req_fc_hdr;

	u32 resp_len;
	void *resp_buf;
	dma_addr_t resp_buf_dma;
	struct fcoe_bd_ctx *mp_resp_bd;
	dma_addr_t mp_resp_bd_dma;
	struct fc_frame_header resp_fc_hdr;
};

struct bnx2fc_els_cb_arg {
	struct bnx2fc_cmd *aborted_io_req;
	struct bnx2fc_cmd *io_req;
	u16 l2_oxid;
};

/* bnx2fc command structure */
struct bnx2fc_cmd {
	struct list_head link;
	struct bnx2fc_port *port;
	struct bnx2fc_rport *tgt;
	struct scsi_cmnd *sc_cmd;
	struct bnx2fc_cmd_mgr *cmd_mgr;

	u16 xid;
	u32 on_active_queue;
	u32 on_tmf_queue;
	u32 cmd_type;
#define BNX2FC_SCSI_CMD 		1
#define BNX2FC_TASK_MGMT_CMD 		2
#define BNX2FC_ABTS			3
#define BNX2FC_ELS			4
#define BNX2FC_CLEANUP			5
	u32 io_req_flags;

	struct kref refcount;
	struct fcoe_task_ctx_entry *task;
	struct io_bdt *bd_tbl;
	size_t data_xfer_len;

	struct delayed_work timeout_work; /* timer for ULP timeouts */
	void (*cb_func)(struct bnx2fc_els_cb_arg *cb_arg);
	struct bnx2fc_els_cb_arg *cb_arg;
	struct completion tm_done;
	int 	wait_for_comp;

	struct fcp_rsp *rsp;
	unsigned long req_flags;
#define BNX2FC_FLAG_ISSUE_RRQ		0x1
#define BNX2FC_FLAG_ISSUE_ABTS		0x2
#define BNX2FC_FLAG_ABTS_DONE		0x3
#define BNX2FC_FLAG_TM_COMPL		0x4
#define BNX2FC_FLAG_TM_TIMEOUT		0x5
#define BNX2FC_FLAG_IO_CLEANUP		0x6
#define BNX2FC_FLAG_RETIRE_OXID		0x7
#define	BNX2FC_FLAG_EH_ABORT		0x8
#define BNX2FC_FLAG_IO_COMPL		0x9
#define BNX2FC_FLAG_ELS_DONE		0xa
#define BNX2FC_FLAG_ELS_TIMEOUT		0xb

	u32 fcp_resid;
	u32 fcp_rsp_len;
	u32 fcp_sns_len;
	u8 cdb_status; /* SCSI IO status */
	u8 fcp_status; /* FCP IO status */
	u8 fcp_rsp_code;
	u8 scsi_comp_flags;

	struct bnx2fc_mp_req mp_req;
};

struct io_bdt {
	struct bnx2fc_cmd *io_req;
	struct fcoe_bd_ctx *bd_tbl;
	dma_addr_t bd_tbl_dma;
	u16 bd_valid;
};

#ifdef __FCOE_IF_RESTART_WQ__
struct bnx2fc_netdev_work {
	struct work_struct work;
	struct bnx2fc_hba *hba;
	unsigned long event;
};
#endif

void *bnx2fc_alloc_dma(struct bnx2fc_hba *hba, size_t size,
					     dma_addr_t *mapping);
void bnx2fc_free_dma(struct bnx2fc_hba *hba, size_t size,
					   void *virt, dma_addr_t mapping);
void *bnx2fc_alloc_dma_bd(struct bnx2fc_hba *hba, size_t size,
					     dma_addr_t *mapping);

struct bnx2fc_cmd *bnx2fc_elstm_alloc(struct bnx2fc_rport *tgt, int type);
void bnx2fc_cmd_release(struct kref *ref);
int bnx2fc_queuecommand(struct scsi_cmnd *sc_cmd,
				void (*done)(struct scsi_cmnd *));
int bnx2fc_send_fw_fcoe_init_msg(struct bnx2fc_hba *hba);
int bnx2fc_send_fw_fcoe_destroy_msg(struct bnx2fc_hba *hba);
int bnx2fc_send_session_ofld_req(struct bnx2fc_port *port,
					struct bnx2fc_rport *tgt);
int bnx2fc_send_session_disable_req(struct bnx2fc_port *port,
				    struct bnx2fc_rport *tgt);
int bnx2fc_send_session_destroy_req(struct bnx2fc_hba *hba,
					struct bnx2fc_rport *tgt);
int bnx2fc_map_doorbell(struct bnx2fc_rport *tgt);
void bnx2fc_indicate_kcqe(void *context, struct kcqe *kcq[],
					u32 num_cqe);
int bnx2fc_setup_task_ctx(struct bnx2fc_hba *hba);
void bnx2fc_free_task_ctx(struct bnx2fc_hba *hba);
int bnx2fc_setup_fw_resc(struct bnx2fc_hba *hba);
void bnx2fc_free_fw_resc(struct bnx2fc_hba *hba);
struct bnx2fc_cmd_mgr *bnx2fc_cmd_mgr_alloc(struct bnx2fc_hba *hba,
						u16 min_xid, u16 max_xid);
void bnx2fc_cmd_mgr_free(struct bnx2fc_cmd_mgr *cmgr);
void bnx2fc_get_link_state(struct bnx2fc_hba *hba);
char *bnx2fc_get_next_rqe(struct bnx2fc_rport *tgt, u8 num_items);
void bnx2fc_return_rqe(struct bnx2fc_rport *tgt, u8 num_items);
u32 bnx2fc_crc(struct fc_frame *fp);
int bnx2fc_get_paged_crc_eof(struct sk_buff *skb, int tlen);
void bnx2fc_fill_fc_hdr(struct fc_frame_header *fc_hdr, enum fc_rctl r_ctl,
			u32 sid, u32 did, enum fc_fh_type fh_type,
			u32 f_ctl, u32 param_offset);
int bnx2fc_send_rrq(struct bnx2fc_cmd *aborted_io_req);
int bnx2fc_send_adisc(struct bnx2fc_rport *tgt, struct fc_frame *fp);
int bnx2fc_send_logo(struct bnx2fc_rport *tgt, struct fc_frame *fp);
int bnx2fc_send_rls(struct bnx2fc_rport *tgt, struct fc_frame *fp);
int bnx2fc_initiate_cleanup(struct bnx2fc_cmd *io_req);
int bnx2fc_initiate_abts(struct bnx2fc_cmd *io_req);
void bnx2fc_cmd_timer_set(struct bnx2fc_cmd *io_req,
			  unsigned int timer_msec);
int bnx2fc_init_mp_req(struct bnx2fc_cmd *io_req);
void bnx2fc_init_cleanup_task(struct bnx2fc_cmd *io_req,
			      struct fcoe_task_ctx_entry *task,
			      u16 orig_xid);
void bnx2fc_init_mp_task(struct bnx2fc_cmd *io_req,
			 struct fcoe_task_ctx_entry *task);
void bnx2fc_init_task(struct bnx2fc_cmd *io_req,
			     struct fcoe_task_ctx_entry *task);
void bnx2fc_add_2_sq(struct bnx2fc_rport *tgt, u16 xid);
void bnx2fc_ring_doorbell(struct bnx2fc_rport *tgt);
int bnx2fc_eh_abort(struct scsi_cmnd *sc_cmd);
int bnx2fc_eh_host_reset(struct scsi_cmnd *sc_cmd);
int bnx2fc_eh_target_reset(struct scsi_cmnd *sc_cmd);
int bnx2fc_eh_device_reset(struct scsi_cmnd *sc_cmd);
int bnx2fc_eh_bus_reset(struct scsi_cmnd *sc_cmd);
void bnx2fc_rport_event_handler(struct fc_lport *lport,
				struct fc_rport_priv *rport,
				enum fc_rport_event event);
void bnx2fc_process_scsi_cmd_compl(struct bnx2fc_cmd *io_req,
				   struct fcoe_task_ctx_entry *task,
				   u8 num_rq);
void bnx2fc_process_cleanup_compl(struct bnx2fc_cmd *io_req,
			       struct fcoe_task_ctx_entry *task,
			       u8 num_rq);
void bnx2fc_process_abts_compl(struct bnx2fc_cmd *io_req,
			       struct fcoe_task_ctx_entry *task,
			       u8 num_rq);
void bnx2fc_process_tm_compl(struct bnx2fc_cmd *io_req,
			     struct fcoe_task_ctx_entry *task,
			     u8 num_rq);
void bnx2fc_process_els_compl(struct bnx2fc_cmd *els_req,
			      struct fcoe_task_ctx_entry *task,
			      u8 num_rq);
void bnx2fc_build_fcp_cmnd(struct bnx2fc_cmd *io_req,
			   struct fcp_cmnd *fcp_cmnd);



void bnx2fc_flush_active_ios(struct bnx2fc_rport *tgt);
struct fc_seq *bnx2fc_elsct_send(struct fc_lport *lport, u32 did,
				      struct fc_frame *fp, unsigned int op,
				      void (*resp)(struct fc_seq *,
						   struct fc_frame *,
						   void *),
				      void *arg, u32 timeout);
int bnx2fc_process_new_cqes(struct bnx2fc_rport *tgt);
struct bnx2fc_rport *bnx2fc_tgt_lookup(struct bnx2fc_port *port,
					     u32 port_id);
void bnx2fc_process_l2_frame_compl(struct bnx2fc_rport *tgt,
				   unsigned char *buf,
				   u32 frame_len, u16 l2_oxid);
int bnx2fc_send_stat_req(struct bnx2fc_hba *hba);

#ifdef __VMKLNX__
static inline u32 bnx2fc_remote_port_chkready(struct fc_rport *rport)
{
	int rval;
	struct fc_rport_libfc_priv *rpriv;

	if (!rport || !rport->dd_data)
		return DID_IMM_RETRY;

	if (!(rval = fc_remote_port_chkready(rport))) {
		rpriv = (struct fc_rport_libfc_priv *)rport->dd_data;

		switch (rpriv->rp_state) {
		case RPORT_ST_READY:
			break;

		case RPORT_ST_INIT:
		case RPORT_ST_PLOGI:
		case RPORT_ST_PRLI:
		case RPORT_ST_RTV:
		case RPORT_ST_ADISC:
			rval = DID_IMM_RETRY << 16;
			break;

		case RPORT_ST_LOGO:
		case RPORT_ST_DELETE:
		default:
			rval = DID_NO_CONNECT << 16;
			break;
		}
	}

	return rval;
}

int bnx2fc_proc_info(struct Scsi_Host *shost, char *buffer, char **start,
    off_t offset, int length, int func);
#endif

#endif
