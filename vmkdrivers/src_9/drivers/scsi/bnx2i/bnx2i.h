/*
 * QLogic NetXtreme II iSCSI offload driver.
 * Copyright (c)   2003-2014 QLogic Corporation
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * Written by: Anil Veerabhadrappa (anilgv@broadcom.com)
 */

#ifndef _BNX2I_H_
#define _BNX2I_H_

#include <linux/version.h>

#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/types.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <asm/byteorder.h>
#include <linux/timer.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <net/tcp.h>
#ifndef __VMKLNX__
#include <linux/kfifo.h>
#include <linux/if_vlan.h>
#endif

#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <asm/semaphore.h>
#ifdef __VMKLNX__
#include <asm/proto.h>
#endif
#include <linux/bitops.h>

#include <scsi/scsi.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_eh.h>
#include <scsi/iscsi_proto.h>

#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,23)
#include <asm/compat.h>
#endif

#if defined(__VMKLNX__) && (VMWARE_ESX_DDK_VERSION >= 60000)
#include <vmklinux_9/vmklinux_scsi.h>
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
#include "../../net/cnic_if.h"
#else
#include <cnic_if.h>
#endif
#include "57xx_iscsi_hsi.h"
#include "57xx_iscsi_constants.h"
#include "bnx2i_ioctl.h"
#include "bnx2x_mfw_req.h"

#ifndef __FREE_IRQ_FIX__
#define __FREE_IRQ_FIX__	1
#endif

#define BNX2_ISCSI_DRIVER_NAME			"bnx2i"

#define PRINT_EMERG(hba, fmt, ...) \
	printk(KERN_EMERG	"bnx2i::%p: " fmt, hba ,##__VA_ARGS__ )
#define PRINT_ALERT(hba, fmt, ...) \
	printk(KERN_ALERT	"bnx2i::%p: " fmt, hba ,##__VA_ARGS__ )
#define PRINT_CRIT(hba, fmt, ...) \
	printk(KERN_CRIT	"bnx2i::%p: " fmt, hba ,##__VA_ARGS__ )
#define PRINT_ERR(hba, fmt, ...) \
	printk(KERN_ERR		"bnx2i::%p: " fmt, hba ,##__VA_ARGS__ )
#define PRINT_WARNING(hba, fmt, ...) \
	printk(KERN_WARNING	"bnx2i::%p: " fmt, hba ,##__VA_ARGS__ )
#define PRINT_NOTICE(hba, fmt, ...) \
	printk(KERN_NOTICE	"bnx2i::%p: " fmt, hba ,##__VA_ARGS__ )
#define PRINT_INFO(hba, fmt, ...) \
	printk(KERN_INFO	"bnx2i::%p: " fmt, hba ,##__VA_ARGS__ )
#define PRINT_DEBUG(hba, fmt, ...) \
	printk(KERN_DEBUG	"bnx2i::%p: " fmt, hba ,##__VA_ARGS__ )
#define PRINT(hba, fmt, ...)	 \
	printk(			"bnx2i::%p: " fmt, hba ,##__VA_ARGS__ )

extern u32 bnx2i_debug_level;
#define DBG_INIT			0x00000001
#define DBG_CONN_SETUP			0x00000002
#define DBG_TMF				0x00000004
#define DBG_ISCSI_NOP			0x00000008
#define DBG_CNIC_IF			0x00000010
#define DBG_ITT_CLEANUP			0x00000020
#define DBG_CONN_EVENT			0x00000040
#define DBG_SESS_RECO			0x00000080

#define BNX2I_DBG(level, hba, fmt, ...)		do {		\
		if (unlikely(level & bnx2i_debug_level))	\
			printk(KERN_INFO "bnx2i::%p: " fmt,	\
				hba, ##__VA_ARGS__);		\
	} while(0)

#ifndef PCI_DEVICE_ID_NX2_5709
#define PCI_DEVICE_ID_NX2_5709			0x1639
#endif

#ifndef PCI_DEVICE_ID_NX2_5709S
#define PCI_DEVICE_ID_NX2_5709S			0x163a
#endif

/*
 *  On ESX the wmb() instruction is defined to only a compiler barrier
 *  The macro wmb() need to be overrode to properly synchronize memory
 */
#if defined(__VMKLNX__)
#undef wmb
#define wmb()   asm volatile("sfence" ::: "memory")
#endif

#define BNX2I_REGISTER_HBA_SUPPORTED		0
#define BNX2I_REGISTER_HBA_FORCED		1

#define ISCSI_MAX_ADAPTERS			8
#define ISCSI_MAX_CONNS_PER_HBA			128
#define ISCSI_MAX_SESS_PER_HBA			ISCSI_MAX_CONNS_PER_HBA
#define ISCSI_MAX_CMDS_PER_SESS			128

#define ISCSI_MAX_BDS_PER_CMD			32

#define MAX_PAGES_PER_CTRL_STRUCT_POOL		16
#define BNX2I_RESERVED_SLOW_PATH_CMD_SLOTS	4

#define BNX2I_5771X_DBELL_PAGE_SIZE		128

/* 5706/08 hardware has limit on maximum buffer size per BD it can handle */
#define MAX_BD_LENGTH				65535
#define BD_SPLIT_SIZE				32768

/* min, max & default values for SQ/RQ/CQ size, configurable via' modparam */
#define BNX2I_SQ_WQES_MIN 			16
#define BNX2I_570X_SQ_WQES_MAX			128
#define BNX2I_5770X_SQ_WQES_MAX			512
#ifdef __VMKLNX__
#define BNX2I_570X_SQ_WQES_DEFAULT 		32
#define BNX2I_5709_SQ_WQES_DEFAULT 		64
#define BNX2I_5770X_SQ_WQES_DEFAULT 		128
#else
#define BNX2I_570X_SQ_WQES_DEFAULT 		128
#define BNX2I_5770X_SQ_WQES_DEFAULT 		256
#endif
#define BNX2I_5770X_SQ_WQES_DEFAULT_X86		64

#define BNX2I_CQ_WQES_MIN 			16
#define BNX2I_CQ_WQES_MAX 			256
#define BNX2I_CQ_WQES_DEFAULT 			128

#define BNX2I_RQ_WQES_MIN 			16
#define BNX2I_RQ_WQES_MAX 			32
#define BNX2I_RQ_WQES_DEFAULT 			16

/* CCELLs per conn */
#define BNX2I_CCELLS_MIN			16
#define BNX2I_CCELLS_MAX			96
#define BNX2I_CCELLS_DEFAULT			64

#define ISCSI_CONN_LOGIN_BUF_SIZE		16384
#define ITT_INVALID_SIGNATURE			0xFFFF

#define ISCSI_CMD_CLEANUP_TIMEOUT		100

#define BNX2I_CONN_CTX_BUF_SIZE			16384

#define BNX2I_SQ_WQE_SIZE			64
#define BNX2I_RQ_WQE_SIZE			256
#define BNX2I_CQE_SIZE				64

#define BNX2I_TCP_WINDOW_MIN			(16 * 1024)
#define BNX2I_TCP_WINDOW_MAX			(1 * 1024 * 1024)
#define BNX2I_TCP_WINDOW_DEFAULT		(64 * 1024)

#define MB_KERNEL_CTX_SHIFT			8
#define MB_KERNEL_CTX_SIZE			(1 << MB_KERNEL_CTX_SHIFT)

#define CTX_SHIFT				7
#define GET_CID_NUM(cid_addr)			((cid_addr) >> CTX_SHIFT)

#define CTX_OFFSET 				0x10000
#define MAX_CID_CNT				0x4000

#define BNX2_TXP_SCRATCH			0x00060000
#define BNX2_TPAT_SCRATCH			0x000a0000
#define BNX2_RXP_SCRATCH			0x000e0000
#define BNX2_COM_SCRATCH			0x00120000
#define BNX2_CP_SCRATCH				0x001a0000

#define BNX2_PCICFG_REG_WINDOW_ADDRESS		0x00000078
#define BNX2_PCICFG_REG_WINDOW_ADDRESS_VAL	(0xfffffL<<2)
#define BNX2_PCICFG_REG_WINDOW			0x00000080

/* 5709 context registers */
#define BNX2_MQ_CONFIG2				0x00003d00
#define BNX2_MQ_CONFIG2_CONT_SZ			(0x7L<<4)
#define BNX2_MQ_CONFIG2_FIRST_L4L5		(0x1fL<<8)

/* 57710's BAR2 is mapped to doorbell registers */
#define BNX2X_DB_SHIFT				3
#define BNX2X_DOORBELL_PCI_BAR			2
#define BNX2X_MAX_CQS				8

#ifndef DMA_64BIT_MASK
#define DMA_64BIT_MASK 				((u64) 0xffffffffffffffffULL)
#define DMA_32BIT_MASK 				((u64) 0x00000000ffffffffULL)
#endif

#ifndef DMA_40BIT_MASK
#define DMA_40BIT_MASK 				((u64) 0x000000ffffffffffULL)
#endif

#define CNIC_ARM_CQE			1
#define CNIC_DISARM_CQE			0

#define BNX2I_TBL_TYPE_NONE		0
#define BNX2I_TBL_TYPE_PG		1
#define BNX2I_TBL_TYPE_BD		2
#define REG_RD(__hba, offset)				\
		readl(__hba->regview + offset)
#define REG_WR(__hba, offset, val)			\
		writel(val, __hba->regview + offset)

#define GET_STATS_64(__hba, dst, field)				\
	do {							\
		dst->field##_lo = __hba->stats.field##_lo;	\
		dst->field##_hi = __hba->stats.field##_hi;	\
	} while (0)

#define ADD_STATS_64(__hba, field, len)				\
	do {							\
		if (spin_trylock(&(__hba)->stat_lock)) {	\
			if ((__hba)->stats.field##_lo + len <	\
			    (__hba)->stats.field##_lo)		\
				(__hba)->stats.field##_hi++;	\
			(__hba)->stats.field##_lo += len;	\
			spin_unlock(&(__hba)->stat_lock);	\
		}						\
	} while (0)

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)

#define scsi_sg_count(cmd) ((cmd)->use_sg)
#define scsi_sglist(cmd) ((struct scatterlist *)(cmd)->request_buffer)
#define scsi_bufflen(cmd) ((cmd)->request_bufflen)

#ifdef __VMKLNX__
#if (VMWARE_ESX_DDK_VERSION == 41000)
#undef _DEFINE_SCSI_SET_RESID
#define _DEFINE_SCSI_GET_RESID	1
#endif /* (VMWARE_ESX_DDK_VERSION == 41000) */

#define BNX2I_INITIAL_POOLSIZE		((1 * VMK_MEGABYTE) >> PAGE_SHIFT)
#define BNX2I_MAX_POOLSIZE_570X		((12 * VMK_MEGABYTE) >> PAGE_SHIFT)
#define BNX2I_MAX_POOLSIZE_5771X	((32 * VMK_MEGABYTE) >> PAGE_SHIFT)
#endif

struct bnx2i_hba;
struct bnx2i_sess;
struct bnx2i_conn;

#if (VMWARE_ESX_DDK_VERSION == 41000)
#ifdef _DEFINE_SCSI_SET_RESID
static inline void scsi_set_resid(struct scsi_cmnd *cmd, int resid)
{
	cmd->resid = resid;
}
#endif

#ifdef _DEFINE_SCSI_GET_RESID
static inline int scsi_get_resid(struct scsi_cmnd *cmd)
{
	return cmd->resid;
}
#endif

#define scsi_for_each_sg(cmd, sg, nseg, __i)			\
        for (__i = 0, sg = scsi_sglist(cmd); __i < (nseg); __i++, (sg)++)
#endif /* (VMWARE_ESX_DDK_VERSION == 41000) */
#endif

struct bnx2i_dma {
	struct list_head link;
	int size;
	char *mem;
	dma_addr_t mapping;
	int pgtbl_type;
	int pgtbl_size;
	char *pgtbl;
	dma_addr_t pgtbl_map;
};

/**
 * struct bd_resc_page - tracks DMA'able memory allocated for BD tables
 *
 * @link:               list head to link elements
 * @max_ptrs:           maximun pointers that can be stored in this page
 * @num_valid:          number of pointer valid in this page
 * @page:               base addess for page pointer array
 *
 * structure to track DMA'able memory allocated for command BD tables
 */
struct bd_resc_page {
	struct list_head link;
	u32 max_ptrs;
	u32 num_valid;
	void *page[1];
};

/**
 * struct io_bdt - I/O buffer destricptor table
 *
 * @link:               list head to link elements
 * @bd_tbl:             BD table's virtual address
 * @bd_tbl_dma:         BD table's dma address
 * @cmdp:               command structure this BD is allocated
 * @max_bd_cnt:         max BD entries in this table
 * @bd_valid:           num valid BD entries
 *
 * IO BD table
 */
struct io_bdt {
	struct list_head link;
	struct iscsi_bd *bd_tbl;
	dma_addr_t bd_tbl_dma;
	struct bnx2i_cmd *cmdp;
	u16 max_bd_cnt;
	u16 bd_valid;
};

/**
 * struct generic_pdu_resc - login pdu resource structure
 *
 * @pdu_hdr:            buffer to copy iscsi header prepared by 'iscsid'
 * @cmd:                iSCSI command pointer
 * @login_itt:          iSCSI ITT to be used with login exchanges
 * @req_buf:            driver buffer used to stage payload associated with
 *                      the login request
 * @req_dma_addr:       dma address for iscsi login request payload buffer
 * @req_buf_size:       actual login request payload length
 * @req_wr_ptr:         pointer into login request buffer when next data is
 *                      to be written
 * @resp_hdr:           iscsi header where iscsi login response header is to
 *                      be recreated
 * @resp_buf:           buffer to stage login response payload
 * @resp_dma_addr:      login response payload buffer dma address
 * @resp_buf_size:      login response paylod length
 * @resp_wr_ptr:        pointer into login response buffer when next data is
 *                      to be written
 * @req_bd_tbl:         BD table to indicate login request payload buffer details
 * @req_bd_dma:         login request BD table dma address
 * @resp_bd_tbl:        BD table to indicate login response payload buffer details
 * @resp_bd_dma:        login request BD table dma address
 *
 * following structure defines buffer info for generic pdus such as iSCSI Login,
 *	Logout and NOP
 */
struct generic_pdu_resc {
	struct iscsi_hdr pdu_hdr;
	u32 login_itt;
	struct bnx2i_dma login_req;
#define req_buf		login_req.mem
	u32 req_buf_size;
	char *req_wr_ptr;
	struct iscsi_hdr resp_hdr;
	struct bnx2i_dma login_resp;
#define resp_buf	login_resp.mem
	u32 resp_buf_size;
	char *resp_wr_ptr;
	struct iscsi_hdr nopout_hdr;
	struct iscsi_hdr nopin_hdr;
	struct iscsi_hdr async_hdr;
};


/**
 * bnx2i_cmd - iscsi command structure
 *
 * @link:               list head to link elements
 * @iscsi_opcode:       iscsi command opcode, NOPIN, LOGIN, SCSICMD, etc'
 * @cmd_state:          command state tracking flag
 * @scsi_status_rcvd:   flag determines whether SCSI response is received
 *                      for this task or not
 * @scsi_cmd:           SCSI-ML task pointer corresponding to this iscsi cmd
 * @tmf_ref_itt:        reference ITT of the command being aborted
 * @tmf_ref_cmd:        pointer of the command being aborted by this command
 * @tmf_ref_sc:         SCSI-ML's task pointer of aborted command
 * @sg:                 SG list
 * @bd_tbl:             buffer descriptor (BD) table
 * @bd_tbl_dma:         buffer descriptor (BD) table's dma address
 */
struct bnx2i_cmd {
	struct list_head link;
	u8 iscsi_opcode;
	u8 rsvd1;
	u16 itt;
	atomic_t cmd_state;
		#define ISCSI_CMD_STATE_FREED			0x000
		#define ISCSI_CMD_STATE_INITIATED		0x001
		#define ISCSI_CMD_STATE_ABORT_REQ		0x002
		#define ISCSI_CMD_STATE_ABORT_PEND		0x004
		#define ISCSI_CMD_STATE_ABORT_COMPL		0x008
		#define ISCSI_CMD_STATE_CLEANUP_START		0x010
		#define ISCSI_CMD_STATE_CLEANUP_PEND		0x020
		#define ISCSI_CMD_STATE_CLEANUP_CMPL		0x040
		#define ISCSI_CMD_STATE_FAILED			0x100
		#define ISCSI_CMD_STATE_TMF_TIMEOUT		0x200
		#define ISCSI_CMD_STATE_CMPL_RCVD		0x400
		#define ISCSI_CMD_STATE_COMPLETED		0x800
	int scsi_status_rcvd;

	struct bnx2i_conn *conn;
	struct scsi_cmnd *scsi_cmd;
	struct scatterlist *sg;
	struct io_bdt *bd_tbl;
	dma_addr_t bd_tbl_dma;
	u32 reserved0;

	struct iscsi_cmd_request req;
	/* TMF RELATED */
	u8 tmf_func;
	u8 tmf_response;
	int tmf_lun;
	u32 tmf_ref_itt;
	struct bnx2i_cmd *tmf_ref_cmd;
	struct scsi_cmnd *tmf_ref_sc;
	int failed_reason;
	/* useful for nop-in processing */
	u32 ttt;

};


/*
 * TCP port manager
 */
struct tcp_port_mngt {
	int num_required;
	u32 port_tbl_size;
	u32 num_free_ports;
	u32 prod_idx;
	u32 cons_idx;
	u32 max_idx;
	u16 *free_q;
};

struct bnx2i_scsi_task {
	struct list_head link;
	struct scsi_cmnd *scsi_cmd;
};

/**
 * struct bnx2i_conn - iscsi connection structure
 *
 * @link:                  list head to link elements
 * @sess:                  iscsi session pointer
 * @cls_conn:              pointer to iscsi cls conn
 * @state:                 flag to trace command state
 * @stop_state:            stop state request by open-iscsi
 * @stage:                 iscsi login state
 * @in_shutdown:           flags to indicate connection is in shutdown mode
 * @lead_conn:             lead iscsi connection of session
 * @conn_cid:              iscsi cid per rfc
 * @exp_statsn:            iscsi expected statsn
 * @header_digest_en:      header digest parameter
 * @data_digest_en:        data digest parameter
 * @max_data_seg_len_xmit: iscsi initiator's mrdsl
 * @max_data_seg_len_recv: iscsi target's mrdsl
 * @ifmarker_enable:       ifmarker parameter
 * @ofmarker_enable:       ofmarker parameter
 * @persist_port:          iscsi target side TCP port number
 * @persist_address:       iscsi target's IP address
 * @iscsi_conn_cid:        iscsi conn id
 * @fw_cid:                firmware iscsi context id
 * @lock:                  lock to synchronize access
 * @ep:                    endpoint structure pointer
 * @gen_pdu:               login/nopout/logout pdu resources
 * @nopout_num_scsi_cmds:  scsi cmds issue counter to detect idle link
 * @total_data_octets_sent:conn stats - data bytes sent on this conn
 * @total_data_octets_rcvd:conn stats - data bytes received on this conn
 * @num_login_req_pdus:    conn stats - num login pdus sent
 * @num_login_resp_pdus:   conn stats - num login pdus received
 * @num_scsi_cmd_pdus:     conn stats - num scsicmd pdus sent
 * @num_scsi_resp_pdus:    conn stats - num scsicmd pdus received
 * @num_nopout_pdus:       conn stats - num nopout pdus sent
 * @num_nopin_pdus         conn stats - num nopout pdus received:
 * @num_reject_pdus:       conn stats - num reject pdus received
 * @num_async_pdus:        conn stats - num async pdus received
 * @num_dataout_pdus:      conn stats - num dout pdus sent
 * @num_r2t_pdus:          conn stats - num r2t pdus received
 * @num_datain_pdus:       conn stats - num din pdus received
 * @num_snack_pdus:        conn stats - num snack pdus received
 * @num_text_req_pdus:     conn stats - num text pdus sent
 * @num_text_resp_pdus:    conn stats - num text pdus received
 * @num_tmf_req_pdus:      conn stats - num tmf pdus sent
 * @num_tmf_resp_pdus:     conn stats - num tmf pdus received
 * @num_logout_req_pdus:   conn stats - num logout pdus sent
 * @num_logout_resp_pdus:  conn stats - num logout pdus received
 *
 * iSCSI connection structure
 */
struct bnx2i_conn {
	struct list_head link;
	struct bnx2i_sess *sess;
	struct iscsi_cls_conn *cls_conn;

	u32 state;
		#define CONN_STATE_IDLE				0x00
		#define CONN_STATE_XPORT_READY			0x01
		#define CONN_STATE_IN_LOGIN			0x02
		#define CONN_STATE_FFP_STATE			0x04
		#define CONN_STATE_IN_LOGOUT			0x08
		#define CONN_STATE_IN_CLEANUP			0x10
		#define CONN_STATE_XPORT_FREEZE			0x20
		#define CONN_STATE_STOPPED			0x80
	atomic_t stop_state;
	u32 stage;
	u32 in_shutdown;

	u32 lead_conn;
	u32 conn_cid;

	struct timer_list poll_timer;
	void (*ring_doorbell)(struct bnx2i_conn *);
	/*
	 * Following are iSCSI sequencing & operational parameters
	 */
	u32 exp_statsn;
		#define STATSN_UPDATE_SIGNATURE		0xFABCAFE
	u32 header_digest_en;
	u32 data_digest_en;
	u32 max_data_seg_len_xmit;	/* Target */
	u32 max_data_seg_len_recv;	/* Initiator */
	int ifmarker_enable;
	int ofmarker_enable;
	int persist_port;
	char *persist_address;

	u32 iscsi_conn_cid;
		#define BNX2I_CID_RESERVED	0x5AFF
	u32 fw_cid;

	/*
	 * Queue Pair (QP) related structure elements.
	 */
	struct bnx2i_endpoint *ep;

	atomic_t worker_running;
	atomic_t worker_enabled;
	atomic_t worker_enabled_cnt;
	atomic_t worker_disabled_cnt;
#ifdef __VMKLNX__
	struct tasklet_struct conn_tasklet;
	char isid[13];
#else
	struct work_struct conn_worker;
#endif
/* DEBUG ONLY */
	u32 tasklet_freeze;
	int tasklet_state;
	int tasklet_tmf_exit;
	int tasklet_timeslice_exit;
	int tasklet_reschedule;
	int tasklet_entry;
	int cqe_process_state;
	unsigned long cqe_process_jiffies;
	int tasklet_loop;
	unsigned long que_jiff;
	unsigned long cqe_jiff;
	unsigned long task_jiff;
	atomic_t lastSched;

	/*
	 * Buffer for login negotiation process
	 */
	struct generic_pdu_resc gen_pdu;

	u32 nopout_num_scsi_cmds;
	/*
	 * Connection Statistics
	 */
	u64 total_data_octets_sent;
	u64 total_data_octets_rcvd;
	u32 num_login_req_pdus;
	u32 num_login_resp_pdus;
	u32 num_scsi_cmd_pdus;
	u32 num_scsi_resp_pdus;
	u32 num_nopout_pdus;
	u32 num_nopin_pdus;
	u32 num_reject_pdus;
	u32 num_async_pdus;
	u32 num_dataout_pdus;
	u32 num_r2t_pdus;
	u32 num_datain_pdus;
	u32 num_snack_pdus;
	u32 num_text_req_pdus;
	u32 num_text_resp_pdus;
	u32 num_tmf_req_pdus;
	u32 num_tmf_resp_pdus;
	u32 num_logout_req_pdus;
	u32 num_logout_resp_pdus;
};



/**
 * struct bnx2i_sess - iscsi session structure
 *
 * @link:                  list head to link elements
 * @hba:                   adapter structure pointer
 * @shost:                 scsi host pointer
 * @state:                 flag to track session state
 * @recovery_state:        recovery state identifier
 * @old_recovery_state:    old recovery state identifier
 * @tmf_active:            TMF is active on this session
 * @lock:                  session lock to synchronize access
 * @abort_timer:           TMF timer
 * @er_wait:               wait queue for recovery process
 * @cmd_pages:             table to track pages allocated for cmd struct
 * @pend_cmds:             pend command list
 * @num_pend_cmds:         number of pend command
 * @free_cmds:             free command list
 * @num_free_cmds:         num free commands
 * @allocated_cmds:        total number of allocated commands
 * @sq_size:               SQ size
 * @itt_q:                 ITT queue
 * @bd_resc_page:          table to track BD resource page memory
 * @bd_tbl_list:           BD table list
 * @bd_tbl_active:         active BD table list
 * @active_cmds:           active command list
 * @num_active_cmds:       num active commands
 * @cmdsn:                 iscsi command sequence number
 * @exp_cmdsn:             iscsi expected command sequence number
 * @max_cmdsn:             iscsi max command sequence number
 * @initial_r2t:           intial R2T is enabled/disable
 * @max_r2t:               maximun outstanding T2T 
 * @imm_data:              indicates if immediate data is enabled
 * @first_burst_len:       negotiated first burst length
 * @max_burst_len:         negotiated max burst length
 * @time2wait:             time 2 wait value
 * @time2retain:           time 2 retain value
 * @pdu_inorder:           indicated if PDU order needs to be maintained
 * @dataseq_inorder:       indicated if data sequence order needs to be
 *                         maintained
 * @erl:                   supported error recovery level
 * @tgt_prtl_grp:          target portal group tag
 * @target_name:           target name
 * @isid:                  isid for this session
 * @tsih:                  target returned TSIH
 * @lead_conn:             points to lead connection pointer
 * @conn_list:             list of connection belonging to this session
 * @num_active_conn:       num active connections
 * @max_conns:             maximun connection per session
 * @violation_notified:    bit mask used to track iscsi error/warning messages
 *                         already printed out
 * iSCSI Session Structure
 */
struct bnx2i_sess {
	struct list_head link;
	struct bnx2i_hba *hba;
#ifdef __VMKLNX__
	struct iscsi_cls_session *cls_sess;
#else
	struct Scsi_Host *shost;
#endif
	unsigned long timestamp;
	unsigned long worker_time_slice;
	u32 state;
		#define BNX2I_SESS_INITIAL		0x01
		#define BNX2I_SESS_IN_FFP		0x02
		#define BNX2I_SESS_IN_RECOVERY		0x04
		#define BNX2I_SESS_IN_SHUTDOWN		0x08
		#define BNX2I_SESS_IN_LOGOUT		0x40
#ifdef __VMKLNX__
		/* Do not notify device offline to vmkernel to until
		 * iscsi transport calls destroy_session()
		 */
		#define BNX2I_SESS_DESTROYED		0x80
		/* if session encounters an error before transitioning
		 * to FFP, target_destroy() will be called before
		 * session_destroy() and this requires another flag
		 * to identify this to make adjustments as to how
		 * resources will be freed
		 */
		#define BNX2I_SESS_TARGET_DESTROYED	0x100
#endif
		#define is_sess_active(_sess)	\
			(((_sess)->state & BNX2I_SESS_IN_FFP))
	unsigned long recovery_state;
		#define ISCSI_SESS_RECOVERY_START	0x01
		#define ISCSI_SESS_RECOVERY_OPEN_ISCSI	0x02
		#define ISCSI_SESS_RECOVERY_COMPLETE 	0x04
		#define ISCSI_SESS_RECOVERY_FAILED	0x08
	unsigned long old_recovery_state;
	atomic_t tmf_active;
	atomic_t do_recovery_inprogess;
	atomic_t device_offline;

#ifndef _USE_ITT_QUE
	struct bnx2i_cmd **itt_cmd;
		#define get_cmnd(sess, itt)	sess->itt_cmd[itt]
#endif

	spinlock_t lock;	/* protects session structure */
	struct mutex tmf_mutex;
	spinlock_t device_lock;	/* serialize device unblock/offline */

	/* Command abort timer */
	struct timer_list abort_timer;
	/* event wait queue used during error recovery */
	wait_queue_head_t er_wait;

	/*
	 * Per session command (task) structure management
	 */
	void *cmd_pages[MAX_PAGES_PER_CTRL_STRUCT_POOL];
	struct list_head free_cmds;
	int num_free_cmds;
	int allocated_cmds;
	int total_cmds_allocated;
	int total_cmds_freed;

	int sq_size;
#ifdef _USE_ITT_QUEUE
	struct itt_queue itt_q;
		#define MAX_BD_RESOURCE_PAGES		8
#endif

	struct list_head bd_resc_page;
	void *bdt_dma_info;
	struct list_head bdt_dma_resc;
	struct list_head bd_tbl_list;
	struct list_head bd_tbl_active;

	/*
	 * command queue management
	 */
	atomic_t login_noop_pending;
	atomic_t tmf_pending;
	atomic_t logout_pending;
	atomic_t nop_resp_pending;
	struct bnx2i_cmd *login_nopout_cmd;
	struct bnx2i_cmd *scsi_tmf_cmd;
	struct bnx2i_cmd *nopout_resp_cmd;

	void *task_list_mem;
	struct list_head scsi_task_list;
	struct list_head pend_cmd_list;
	u32 pend_cmd_count;
	struct list_head active_cmd_list;
	u32 active_cmd_count;
	int cmd_cleanup_req;
	int cmd_cleanup_cmpl;

	/* Debug counter */
	u32 total_cmds_sent;
	u32 total_cmds_queued;
	u32 total_cmds_completed;
	u32 total_cmds_failed;
	u32 total_cmds_completed_by_chip;
	u32 cmd_win_closed;
	u32 host_busy_cmd_win;
	u32 alloc_scsi_task_failed;

	/*
	 * iSCSI session related sequencing parameters.
	 */
	unsigned int cmdsn;
	unsigned int exp_cmdsn;
	unsigned int max_cmdsn;

	/*
	 * Following pointers are linked to corresponding entry in
	 * operational parameter table associated with this session.
	 * These are to be filled when session becomes operational (FFP).
	 */
	int initial_r2t;
	int max_r2t;
	int imm_data;
	u32 first_burst_len;
	u32 max_burst_len;
	int time2wait;
	int time2retain;
	int pdu_inorder;
	int dataseq_inorder;
	int erl;
	int tgt_prtl_grp;
	char *target_name;

	unsigned char isid[13];
	unsigned short tsih;

	struct bnx2i_conn *lead_conn;
	struct list_head conn_list;
	u32 num_active_conn;
	u32 max_conns;

	/* Driver private statistics */
	u64 violation_notified;

	unsigned long last_nooput_requested;
	unsigned long last_nooput_posted;
	unsigned long last_noopin_indicated;
	unsigned long last_noopin_processed;
	u32 last_nooput_sn;
	u32 noopout_resp_count;
	u32 unsol_noopout_count;
	int noopout_requested_count;
	int noopout_posted_count;
	int noopin_indicated_count;
	int noopin_processed_count;
	int tgt_noopin_count;

	u32 max_iscsi_tasks;
};



/**
 * struct iscsi_cid_queue - Per adapter iscsi cid queue
 *
 * @cid_que_base:           queue base memory
 * @cid_que:                queue memory pointer
 * @cid_q_prod_idx:         produce index
 * @cid_q_cons_idx:         consumer index
 * @cid_q_max_idx:          max index. used to detect wrap around condition
 * @cid_free_cnt:           queue size
 * @conn_cid_tbl:           iscsi cid to conn structure mapping table
 *
 * Per adapter iSCSI CID Queue
 */
struct iscsi_cid_queue {
	void *cid_que_base;
	u32 *cid_que;
	u32 cid_q_prod_idx;
	u32 cid_q_cons_idx;
	u32 cid_q_max_idx;
	u32 cid_free_cnt;
	struct bnx2i_conn **conn_cid_tbl;
};

struct iscsi_login_stats_info {
	u32 successful_logins;			/* Total login successes */
	u32 login_failures;			/* Total login failures */
	u32 login_negotiation_failures;		/* Text negotiation failed */
	u32 login_authentication_failures;	/* login Authentication failed */
	u32 login_redirect_responses;		/* Target redirects to another portal */
	u32 connection_timeouts;		/* TCP connection timeouts */
	u32 session_failures;			/* Errors resulting in sess recovery */
	u32 digest_errors;			/* Errors resulting in digest errors */
};

/**
 * struct bnx2i_hba - bnx2i adapter structure
 *
 * @link:                  list head to link elements
 * @cnic:                  pointer to cnic device
 * @pcidev:                pointer to pci dev
 * @netdev:                pointer to netdev structure
 * @regview:               mapped PCI register space
 * @class_dev:             class dev to operate sysfs node
 * @age:                   age, incremented by every recovery
 * @cnic_dev_type:         cnic device type, 5706/5708/5709/57710
 * @mail_queue_access:     mailbox queue access mode, applicable to 5709 only
 * @reg_with_cnic:         indicates whether the device is register with CNIC
 * @adapter_state:         adapter state, UP, GOING_DOWN, LINK_DOWN
 * @mtu_supported:         Ethernet MTU supported
 * @scsi_template:         pointer to scsi host template
 * @iscsi_transport:       pointer to iscsi transport template
 * @shost_template:        pointer to shost template
 * @max_sqes:              SQ size
 * @max_rqes:              RQ size
 * @max_cqes:              CQ size
 * @num_ccell:             number of command cells per connection
 * @active_sess:           active session list head
 * @num_active_sess:       number of active connections
 * @ofld_conns_active:     active connection list
 * @max_active_conns:      max offload connections supported by this device
 * @cid_que:               iscsi cid queue
 * @ep_rdwr_lock:          read / write lock to synchronize various ep lists
 * @ep_ofld_list:          connection list for pending offload completion
 * @ep_destroy_list:       connection list for pending offload completion
 * @mp_bd_tbl:             BD table to be used with middle path requests
 * @mp_bd_dma:             DMA address of 'mp_bd_tbl' memory buffer
 * @dummy_buffer:          Dummy buffer to be used with zero length scsicmd reqs
 * @dummy_buf_dma:         DMA address of 'dummy_buffer' memory buffer
 * @lock:              	   lock to synchonize access to hba structure
 * @hba_timer:             timer block
 * @eh_wait:               wait queue to be used during error handling
 * @err_rec_task:          error handling worker
 * @sess_recov_list:       session list which are queued for recovery
 * @sess_recov_prod_idx:   producer index to manage session recovery list
 * @sess_recov_cons_idx:   producer index to manage session recovery list
 * @sess_recov_max_idx:    max index to manage session recovery list
 * @mac_addr:              MAC address
 * @conn_teardown_tmo:     connection teardown timeout
 * @conn_ctx_destroy_tmo:  connection context destroy timeout
 * @hba_shutdown_tmo:      hba shutdown cleanup timeout
 * @pci_did:               PCI device ID
 * @pci_vid:               PCI vendor ID
 * @pci_sdid:              PCI subsystem device ID
 * @pci_svid:              PCI subsystem vendor ID
 * @pci_func:              PCI function number in system pci tree
 * @pci_devno:             PCI device number in system pci tree
 * @num_wqe_sent:          statistic counter, total wqe's sent
 * @num_cqe_rcvd:          statistic counter, total cqe's received
 * @num_intr_claimed:      statistic counter, total interrupts claimed
 * @link_changed_count:    statistic counter, num of link change notifications
 *                         received
 * @ipaddr_changed_count:  statistic counter, num times IP address changed while
 *                         at least one connection is offloaded
 * @num_sess_opened:       statistic counter, total num sessions opened
 * @num_conn_opened:       statistic counter, total num conns opened on this hba
 * @stat_lock:             statistic lock to maintain coherency
 * @stats:                 iSCSI statistic structure memory
 * @login_stats:           iSCSI login statistic structure memory
 *
 * Adapter Data Structure
 */
struct bnx2i_hba {
	struct list_head link;
	struct cnic_dev *cnic;
	struct pci_dev *pcidev;
	struct net_device *netdev;
 	void __iomem *regview;
	resource_size_t reg_base;
	struct class_device class_dev;
	u32 age;
	unsigned long cnic_dev_type;
		#define BNX2I_NX2_DEV_5706		0x0
		#define BNX2I_NX2_DEV_5708		0x1
		#define BNX2I_NX2_DEV_5709		0x2
		#define BNX2I_NX2_DEV_57710		0x3
	u32 mail_queue_access;
		#define BNX2I_MQ_KERNEL_MODE		0x0
		#define BNX2I_MQ_KERNEL_BYPASS_MODE	0x1
		#define BNX2I_MQ_BIN_MODE		0x2
	unsigned long  reg_with_cnic;
		#define BNX2I_CNIC_REGISTERED		1

	unsigned long  adapter_state;
		#define ADAPTER_STATE_UP		0
		#define ADAPTER_STATE_GOING_DOWN	1
		#define ADAPTER_STATE_LINK_DOWN		2
		#define ADAPTER_STATE_INIT_FAILED	31
	unsigned int mtu_supported;
#if (VMWARE_ESX_DDK_VERSION >= 50000)
		#define BNX2I_MAX_MTU_SUPPORTED		9000
#else
		#define BNX2I_MAX_MTU_SUPPORTED		1500
#endif

	struct scsi_host_template *scsi_template;
	struct iscsi_transport *iscsi_transport;
#ifdef __VMKLNX__
	vmk_MemPool bnx2i_pool;
		#define BRCM_ISCSI_XPORT_NAME_PREFIX		"bnx2i"
#else
		#define BRCM_ISCSI_XPORT_NAME_PREFIX		"bcm570x"
#endif
		#define BRCM_ISCSI_XPORT_NAME_SIZE_MAX		128
	struct scsi_transport_template *shost_template;

#ifdef __VMKLNX__
	struct Scsi_Host *shost;
	u32 target_id;
	u32 channel_id;
	struct device vm_pcidev;
#endif

	u32 max_sqes;
	u32 max_rqes;
	u32 max_cqes;
	u32 num_ccell;

#ifdef __VMKLNX__
	struct timer_list hba_poll_timer;
#endif

	/* different page table setup requirments for 5771x and 570x */
	void (*setup_pgtbl)(struct bnx2i_hba *hba,
			    struct bnx2i_dma *dma,
			    int pgtbl_off);

	struct list_head active_sess;
	int num_active_sess;
	int ofld_conns_active;

	int max_active_conns;
	struct iscsi_cid_queue cid_que;
	spinlock_t cid_que_lock;

#ifndef __VMKLNX__
	rwlock_t ep_rdwr_lock;
#endif
	struct list_head ep_ofld_list;
	struct list_head ep_destroy_list;

	/*
	 * BD table to be used with MP (Middle Path requests.
	 */
	struct bnx2i_dma mp_dma_buf;

	spinlock_t lock;	/* protects hba structure access */
	struct mutex net_dev_lock;/* sync net device access */

	/* Error handling */
	struct timer_list hba_timer;
	wait_queue_head_t eh_wait;
	struct work_struct err_rec_task;
	struct bnx2i_sess **sess_recov_list;
	int sess_recov_prod_idx;
	int sess_recov_cons_idx;
	int sess_recov_max_idx;
	
	unsigned char mac_addr[MAX_ADDR_LEN];

	int conn_teardown_tmo;
	int conn_ctx_destroy_tmo;
	int hba_shutdown_tmo;
	unsigned int ctx_ccell_tasks;
	/*
	 * PCI related info.
	 */
	u16 pci_did;
	u16 pci_vid;
	u16 pci_sdid;
	u16 pci_svid;
	u16 pci_func;
	u16 pci_devno;

	/*
	 * Following are a bunch of statistics useful during development
	 * and later stage for score boarding.
	 */
	u32 num_wqe_sent;
	u32 num_cqe_rcvd;
	u32 num_intr_claimed;
	u32 link_changed_count;
	u32 ipaddr_changed_count;
	u32 num_sess_opened;
	u32 num_conn_opened;
	u32 stop_event_ifc_abort_poll;
	u32 stop_event_ifc_abort_bind;
	u32 stop_event_ifc_abort_login;
	u32 stop_event_ep_conn_failed;
	u32 stop_event_repeat;
	u32 task_cleanup_failed;
	u32 tcp_error_kcqes;
	u32 iscsi_error_kcqes;

	spinlock_t stat_lock;
	struct iscsi_stats_info stats;
	struct iscsi_login_stats_info login_stats;

	struct list_head ep_stale_list;
        
	/* conn disconnect timeout handling */
	wait_queue_head_t ep_tmo_wait;
	struct list_head ep_tmo_list;
	struct work_struct ep_poll_task;
	atomic_t ep_tmo_poll_enabled;
	u32    ep_tmo_active_cnt;
	u32    ep_tmo_cmpl_cnt;
	u32    max_scsi_task_queued;
};


/*******************************************************************************
 * 	QP [ SQ / RQ / CQ ] info.
 ******************************************************************************/

/*
 * SQ/RQ/CQ generic structure definition
 */
struct 	sqe {
	u8 sqe_byte[BNX2I_SQ_WQE_SIZE];
};

struct 	rqe {
	u8 rqe_byte[BNX2I_RQ_WQE_SIZE];
};

struct 	cqe {
	u8 cqe_byte[BNX2I_CQE_SIZE];
};


enum {
#if defined(__LITTLE_ENDIAN)
	CNIC_EVENT_COAL_INDEX	= 0x0,
	CNIC_SEND_DOORBELL	= 0x4,
	CNIC_EVENT_CQ_ARM	= 0x7,
	CNIC_RECV_DOORBELL	= 0x8
#elif defined(__BIG_ENDIAN)
	CNIC_EVENT_COAL_INDEX	= 0x2,
	CNIC_SEND_DOORBELL	= 0x6,
	CNIC_EVENT_CQ_ARM	= 0x4,
	CNIC_RECV_DOORBELL	= 0xa
#endif
};



/*
 * CQ DB
 */
struct bnx2x_iscsi_cq_pend_cmpl {
	/* CQ producer, updated by Ustorm */
        u16 ustrom_prod;
	/* CQ pending completion counter */
        u16 pend_cntr;
};


struct bnx2i_5771x_cq_db {
        struct bnx2x_iscsi_cq_pend_cmpl qp_pend_cmpl[BNX2X_MAX_CQS];
	/* CQ pending completion ITT array */
        u16 itt[BNX2X_MAX_CQS];
	/* Cstorm CQ sequence to notify array, updated by driver */;
        u16 sqn[BNX2X_MAX_CQS];
        u32 reserved[4] /* 16 byte allignment */;
};


struct bnx2i_5771x_sq_rq_db {
	u16 prod_idx;
	u8 reserved0[62]; /* Pad structure size to 64 bytes */
};


struct bnx2i_5771x_dbell_hdr {
        u8 header;
	/* 1 for rx doorbell, 0 for tx doorbell */
#define B577XX_DOORBELL_HDR_RX				(0x1<<0)
#define B577XX_DOORBELL_HDR_RX_SHIFT			0
	/* 0 for normal doorbell, 1 for advertise wnd doorbell */
#define B577XX_DOORBELL_HDR_DB_TYPE			(0x1<<1)
#define B577XX_DOORBELL_HDR_DB_TYPE_SHIFT		1
	/* rdma tx only: DPM transaction size specifier (64/128/256/512B) */
#define B577XX_DOORBELL_HDR_DPM_SIZE			(0x3<<2)
#define B577XX_DOORBELL_HDR_DPM_SIZE_SHIFT		2
	/* connection type */
#define B577XX_DOORBELL_HDR_CONN_TYPE			(0xF<<4)
#define B577XX_DOORBELL_HDR_CONN_TYPE_SHIFT		4
};

struct bnx2i_5771x_dbell {
	struct bnx2i_5771x_dbell_hdr dbell;
	u8 pad[3];

};


/**
 * struct qp_info - QP (share queue region) atrributes structure
 *
 * @ctx_base:           ioremapped pci register base to access doorbell register
 *                      pertaining to this offloaded connection
 * @sq_virt:            virtual address of send queue (SQ) region
 * @sq_phys:            DMA address of SQ memory region
 * @sq_mem_size:        SQ size
 * @sq_prod_qe:         SQ producer entry pointer
 * @sq_cons_qe:         SQ consumer entry pointer
 * @sq_first_qe:        virtaul address of first entry in SQ
 * @sq_last_qe:         virtaul address of last entry in SQ
 * @sq_prod_idx:        SQ producer index
 * @sq_cons_idx:        SQ consumer index
 * @sqe_left:           number sq entry left
 * @sq_pgtbl_virt:      page table describing buffer consituting SQ region
 * @sq_pgtbl_phys:      dma address of 'sq_pgtbl_virt'
 * @sq_pgtbl_size:      SQ page table size
 * @cq_virt:            virtual address of completion queue (CQ) region
 * @cq_phys:            DMA address of RQ memory region
 * @cq_mem_size:        CQ size
 * @cq_prod_qe:         CQ producer entry pointer
 * @cq_cons_qe:         CQ consumer entry pointer
 * @cq_first_qe:        virtaul address of first entry in CQ
 * @cq_last_qe:         virtaul address of last entry in CQ
 * @cq_prod_idx:        CQ producer index
 * @cq_cons_idx:        CQ consumer index
 * @cqe_left:           number cq entry left
 * @cqe_size:           size of each CQ entry
 * @cqe_exp_seq_sn:     next expected CQE sequence number
 * @cq_pgtbl_virt:      page table describing buffer consituting CQ region  
 * @cq_pgtbl_phys:      dma address of 'cq_pgtbl_virt'  
 * @cq_pgtbl_size:    	CQ page table size    
 * @rq_virt:            virtual address of receive queue (RQ) region
 * @rq_phys:            DMA address of RQ memory region
 * @rq_mem_size:        RQ size
 * @rq_prod_qe:         RQ producer entry pointer
 * @rq_cons_qe:         RQ consumer entry pointer
 * @rq_first_qe:        virtaul address of first entry in RQ
 * @rq_last_qe:         virtaul address of last entry in RQ
 * @rq_prod_idx:        RQ producer index
 * @rq_cons_idx:        RQ consumer index
 * @rqe_left:           number rq entry left
 * @rq_pgtbl_virt:      page table describing buffer consituting RQ region
 * @rq_pgtbl_phys:      dma address of 'rq_pgtbl_virt'
 * @rq_pgtbl_size:      RQ page table size
 *
 * queue pair (QP) is a per connection shared data structure which is used
 *	to send work requests (SQ), receive completion notifications (CQ)
 *	and receive asynchoronous / scsi sense info (RQ). 'qp_info' structure
 *	below holds queue memory, consumer/producer indexes and page table
 *	information
 */
struct qp_info {
	void __iomem *ctx_base;
#define DPM_TRIGER_TYPE			0x40

#define BNX2I_570x_QUE_DB_SIZE		0
#define BNX2I_5771x_QUE_DB_SIZE		16
	struct bnx2i_dma sq_dma;
#define sq_virt		sq_dma.mem
	struct sqe *sq_prod_qe;
	struct sqe *sq_first_qe;
	struct sqe *sq_last_qe;
	u16 sq_prod_idx;

	struct bnx2i_dma cq_dma;
#define cq_virt		cq_dma.mem
	struct cqe *cq_cons_qe;
	struct cqe *cq_first_qe;
	struct cqe *cq_last_qe;
	u16 cq_cons_idx;
	u32 cqe_left;
	u32 cqe_size;
	u32 cqe_exp_seq_sn;

	struct bnx2i_dma rq_dma;
#define rq_virt		rq_dma.mem

	struct rqe *rq_prod_qe;
	struct rqe *rq_cons_qe;
	struct rqe *rq_first_qe;
	struct rqe *rq_last_qe;
	u16 rq_prod_idx;
	u16 rq_cons_idx;
	u32 rqe_left;
};



/*
 * CID handles
 */
struct ep_handles {
	u32 fw_cid;
	u32 drv_iscsi_cid;
	u16 pg_cid;
	u16 rsvd;
};


/**
 * struct bnx2i_endpoint - representation of tcp connection in NX2 world
 *
 * @link:               list head to link elements
 * @hba:                adapter to which this connection belongs
 * @conn:               iscsi connection this EP is linked to
 * @sess:               iscsi session this EP is linked to
 * @cm_sk:              cnic sock struct
 * @hba_age:            age to detect if 'iscsid' issues ep_disconnect()
 *                      after HBA reset is completed by bnx2i/cnic/bnx2
 *                      modules
 * @state:              tracks offload connection state machine
 * @tcp_port:           Local TCP port number used in this connection
 * @qp:                 QP information
 * @ids:                contains chip allocated *context id* & driver assigned
 *                      *iscsi cid*
 * @ofld_timer:         offload timer to detect timeout
 * @ofld_wait:          wait queue
 *
 * Endpoint Structure - equivalent of tcp socket structure
 */
struct bnx2i_endpoint {
	struct list_head link;
	struct bnx2i_hba *hba;
	struct bnx2i_conn *conn;
	struct bnx2i_sess *sess;
	struct cnic_sock *cm_sk;
	u32 hba_age;
	u32 state;
		#define EP_STATE_IDLE			0x00000000
		#define EP_STATE_PG_OFLD_START		0x00000001
		#define EP_STATE_PG_OFLD_COMPL		0x00000002
		#define EP_STATE_OFLD_START		0x00000004
		#define EP_STATE_OFLD_COMPL		0x00000008
		#define EP_STATE_CONNECT_START		0x00000010
		#define EP_STATE_CONNECT_COMPL		0x00000020
		#define EP_STATE_ULP_UPDATE_START	0x00000040
		#define EP_STATE_ULP_UPDATE_COMPL	0x00000080
		#define EP_STATE_DISCONN_START		0x00000100
		#define EP_STATE_DISCONN_COMPL		0x00000200
		#define EP_STATE_CLEANUP_START		0x00000400
		#define EP_STATE_CLEANUP_CMPL		0x00000800
		#define EP_STATE_TCP_FIN_RCVD		0x00001000
		#define EP_STATE_TCP_RST_RCVD		0x00002000
                #define EP_STATE_ULP_UPDATE_TIMEOUT	0x00004000
		#define EP_STATE_PG_OFLD_FAILED		0x01000000
		#define EP_STATE_ULP_UPDATE_FAILED	0x02000000
		#define EP_STATE_CLEANUP_FAILED		0x04000000
		#define EP_STATE_OFLD_FAILED		0x08000000
		#define EP_STATE_CONNECT_FAILED		0x10000000
		#define EP_STATE_DISCONN_TIMEDOUT	0x20000000
		#define EP_STATE_OFLD_FAILED_CID_BUSY	0x80000000

	unsigned long timestamp;
	int teardown_mode;
#define BNX2I_ABORTIVE_SHUTDOWN		0
#define BNX2I_GRACEFUL_SHUTDOWN		1
	u16 tcp_port;

	atomic_t fp_kcqe_events;
	struct qp_info qp;
	struct ep_handles ids;
		#define ep_iscsi_cid	ids.drv_iscsi_cid
		#define ep_cid		ids.fw_cid
		#define ep_pg_cid	ids.pg_cid
	struct timer_list ofld_timer;
	wait_queue_head_t ofld_wait;
	u32 in_progress;
};


static inline struct Scsi_Host *bnx2i_conn_get_shost(struct bnx2i_conn *conn)
{
	struct Scsi_Host *shost;

#if defined(__VMKLNX__)
	shost = conn->sess->hba->shost;
#else
	shost = conn->sess->shost;
#endif
	return shost;
}

static inline struct Scsi_Host *bnx2i_sess_get_shost(struct bnx2i_sess *sess)
{
	struct Scsi_Host *shost;

#if defined(__VMKLNX__)
	shost = sess->hba->shost;
#else
	shost = sess->shost;
#endif
	return shost;
}


#ifdef __VMKLNX__

#define pci_alloc_consistent pci_alloc_consistent_esx
#define pci_free_consistent pci_free_consistent_esx
extern struct bnx2i_hba *bnx2i_map_pcidev_to_hba(struct pci_dev *pdev);

static inline char *pci_alloc_consistent_esx(struct pci_dev *pdev, size_t size,
					     dma_addr_t *mapping)
{
	char *virt_mem;
	VMK_ReturnStatus status;
#if (VMWARE_ESX_DDK_VERSION == 41000)
	vmk_MachPage pfn;
	vmk_MemPoolAllocProps pool_alloc_props;
#else
	vmk_MPN pfn;
	vmk_MemPoolAllocProps pool_alloc_props;
	vmk_MpnRange range;
	vmk_MemPoolAllocRequest alloc_request;
#endif /* (VMWARE_ESX_DDK_VERSION == 41000) */
	struct bnx2i_hba *hba = bnx2i_map_pcidev_to_hba(pdev);

	if (!hba || !hba->bnx2i_pool)
		return NULL;

#if (VMWARE_ESX_DDK_VERSION == 41000)
	pool_alloc_props.alignment = 0;
	if (dma_get_required_mask(&pdev->dev) >= pdev->dma_mask)
		pool_alloc_props.maxPage = VMK_MEMPOOL_MAXPAGE_LOW;
	else
		pool_alloc_props.maxPage = VMK_MEMPOOL_MAXPAGE_ANY;

	status = vmk_MemPoolAlloc(hba->bnx2i_pool, &pool_alloc_props,
				  1 << get_order(size), VMK_FALSE, &pfn);
#else
        pool_alloc_props.physContiguity = VMK_MEM_PHYS_CONTIGUOUS;
        pool_alloc_props.physRange = VMK_PHYS_ADDR_ANY;
        pool_alloc_props.creationTimeoutMS = VMK_TIMEOUT_NONBLOCKING;
        if (dma_get_required_mask(&pdev->dev) >= pdev->dma_mask)
                pool_alloc_props.physRange = VMK_PHYS_ADDR_BELOW_4GB;

        alloc_request.numPages = 1 << get_order(size);
        alloc_request.numElements = 1;
        alloc_request.mpnRanges = &range;

        status = vmk_MemPoolAlloc(hba->bnx2i_pool, &pool_alloc_props, &alloc_request);
#endif /* (VMWARE_ESX_DDK_VERSION == 41000) */
	if (unlikely(status != VMK_OK)) {
		printk("allocation failed size=%lu\n", size);
		return NULL;
	}
#if (VMWARE_ESX_DDK_VERSION != 41000)
	pfn = range.startMPN;
#endif /* (VMWARE_ESX_DDK_VERSION != 41000) */
	virt_mem = page_to_virt(pfn_to_page(pfn));

	memset(virt_mem, 0, size);
	*mapping = pci_map_single(pdev, virt_mem, size, PCI_DMA_BIDIRECTIONAL);

	return virt_mem;
}

static inline void pci_free_consistent_esx(struct pci_dev *pdev, size_t size,
					   void *virt, dma_addr_t mapping)
{
#if (VMWARE_ESX_DDK_VERSION == 41000)
	vmk_MachPage pfn;
#else
	vmk_MpnRange range;
	vmk_MemPoolAllocRequest alloc_request;
#endif

	struct bnx2i_hba *hba = bnx2i_map_pcidev_to_hba(pdev);

	if (!hba) {
		printk("pci_free_consistent_esx: could not find the hba associated with the dma mapping to free.\n");
		return;
	}

	 pci_unmap_single(pdev, mapping, size, PCI_DMA_BIDIRECTIONAL);

#if (VMWARE_ESX_DDK_VERSION == 41000)
	pfn = virt_to_page(virt);
	vmk_MemPoolFree(&pfn);
#else
	range.startMPN = virt_to_page(virt);
	range.numPages = 1 << get_order(size);
	alloc_request.mpnRanges = &range;
	alloc_request.numPages = range.numPages;
	alloc_request.numElements = 1;
	vmk_MemPoolFree(&alloc_request);
#endif /* (VMWARE_ESX_DDK_VERSION == 41000) */
}

#if (VMWARE_ESX_DDK_VERSION >= 60000)
static inline void bnx2i_int_to_scsilun_with_sec_lun_id(uint16_t lun,
							struct scsi_lun *scsi_lun,
							uint64_t sllid)
{
	if (sllid != VMKLNX_SCSI_INVALID_SECONDLEVEL_ID) {
		memset(scsi_lun, 0, 8);
		scsi_lun->scsi_lun[0] = (lun >> 8) & 0xFF;
		scsi_lun->scsi_lun[1] = lun & 0xFF;
		scsi_lun->scsi_lun[2] = (uint8_t)((sllid >> 56) & 0xFF); /* sllid msb */
		scsi_lun->scsi_lun[3] = (uint8_t)((sllid >> 48) & 0xFF);
		scsi_lun->scsi_lun[4] = (uint8_t)((sllid >> 40) & 0xFF);
		scsi_lun->scsi_lun[5] = (uint8_t)((sllid >> 32) & 0xFF);
		scsi_lun->scsi_lun[6] = (uint8_t)((sllid >> 24) & 0xFF);
		scsi_lun->scsi_lun[7] = (uint8_t)((sllid >> 16) & 0xFF); /* sllid lsb */
	} else {
		int_to_scsilun(lun, scsi_lun);
	}
}
#endif /* (VMWARE_ESX_DDK_VERSION >= 60000) */

#endif /* __VMKLNX__ */

extern unsigned int cmd_cmpl_per_work;
extern unsigned int max_bnx2x_sessions;
extern unsigned int max_bnx2_sessions;
#ifdef __VMKLNX__
extern int bnx2i_max_sectors;
#endif

/*
 * Function Prototypes
 */
extern int bnx2i_reg_device;
void bnx2i_identify_device(struct bnx2i_hba *hba, struct cnic_dev *dev);
void bnx2i_register_device(struct bnx2i_hba *hba, int force);
void bnx2i_check_nx2_dev_busy(void);
#ifdef __VMKLNX__
void bnx2i_get_link_state(struct bnx2i_hba *hba);
void bnx2i_ep_disconnect(vmk_int64 ep_handle);
struct bnx2i_hba *bnx2i_map_netdev_to_hba(struct net_device *netdev);
#else
void bnx2i_ep_disconnect(uint64_t ep_handle);
#endif

void bnx2i_ulp_init(struct cnic_dev *dev);
void bnx2i_ulp_exit(struct cnic_dev *dev);
void bnx2i_start(void *handle);
void bnx2i_stop(void *handle);
void bnx2i_reg_dev_all(void);
void bnx2i_unreg_dev_all(void);
struct bnx2i_hba *get_adapter_list_head(void);
void bnx2i_add_hba_to_adapter_list(struct bnx2i_hba *hba);
void bnx2i_remove_hba_from_adapter_list(struct bnx2i_hba *hba);

int bnx2i_ioctl_init(void);
void bnx2i_ioctl_cleanup(void);

struct bnx2i_conn *bnx2i_get_conn_from_id(struct bnx2i_hba *hba,
					  u16 iscsi_cid);

int bnx2i_alloc_ep_pool(void);
void bnx2i_release_ep_pool(void);
struct bnx2i_endpoint *bnx2i_ep_ofld_list_next(struct bnx2i_hba *hba);
struct bnx2i_endpoint *bnx2i_ep_destroy_list_next(struct bnx2i_hba *hba);

struct bnx2i_cmd *bnx2i_alloc_cmd(struct bnx2i_sess *sess);
void bnx2i_free_cmd(struct bnx2i_sess *sess, struct bnx2i_cmd *cmd);
int bnx2i_tcp_conn_active(struct bnx2i_conn *conn);

struct bnx2i_hba *bnx2i_find_hba_for_cnic(struct cnic_dev *cnic);
struct bnx2i_hba *bnx2i_get_hba_from_template(
	struct scsi_transport_template *scsit);

struct bnx2i_hba *bnx2i_alloc_hba(struct cnic_dev *cnic);
void bnx2i_free_hba(struct bnx2i_hba *hba);
int bnx2i_process_new_cqes(struct bnx2i_conn *conn, int soft_irq, int num_cqes);
void bnx2i_process_scsi_resp(struct bnx2i_cmd *cmd,
			     struct iscsi_cmd_response *resp_cqe);
int bnx2i_process_nopin(struct bnx2i_conn *conn,
	struct bnx2i_cmd *cmnd, char *data_buf, int data_len);


void bnx2i_update_cmd_sequence(struct bnx2i_sess *sess, u32 expsn, u32 maxsn);

void bnx2i_get_rq_buf(struct bnx2i_conn *conn, char *ptr, int len);
void bnx2i_put_rq_buf(struct bnx2i_conn *conn, int count);

int bnx2i_indicate_login_resp(struct bnx2i_conn *conn);
int bnx2i_indicate_logout_resp(struct bnx2i_conn *conn);
int bnx2i_indicate_async_mesg(struct bnx2i_conn *conn);

void bnx2i_iscsi_unmap_sg_list(struct bnx2i_hba *hba, struct bnx2i_cmd *cmd);

void bnx2i_iscsi_hba_cleanup(struct bnx2i_hba *hba);
void bnx2i_start_iscsi_hba_shutdown(struct bnx2i_hba *hba);
void bnx2i_iscsi_handle_ip_event(struct bnx2i_hba *hba);
int bnx2i_do_iscsi_sess_recovery(struct bnx2i_sess *sess, int err_code, int signal);
void bnx2i_return_failed_command(struct bnx2i_sess *sess,
				 struct scsi_cmnd *cmd, int resid, int err_code);
void bnx2i_fail_cmd(struct bnx2i_sess *sess, struct bnx2i_cmd *cmd);
int bnx2i_complete_cmd(struct bnx2i_sess *sess, struct bnx2i_cmd *cmd);

void bnx2i_cleanup_tcp_port_mngr(void);
int bnx2i_init_tcp_port_mngr(void);

int bnx2i_alloc_dma(struct bnx2i_hba *hba, struct bnx2i_dma *dma,
		    int size, int pgtbl_type, int pgtbl_off);
void bnx2i_free_dma(struct bnx2i_hba *hba, struct bnx2i_dma *dma);
int bnx2i_setup_mp_bdt(struct bnx2i_hba *hba);
void bnx2i_free_mp_bdt(struct bnx2i_hba *hba);
void bnx2i_init_ctx_dump_mem(struct bnx2i_hba *hba);
void bnx2i_free_ctx_dump_mem(struct bnx2i_hba *hba);

extern int bnx2i_send_fw_iscsi_init_msg(struct bnx2i_hba *hba);
extern int bnx2i_send_iscsi_login(struct bnx2i_conn *conn,
				  struct bnx2i_cmd *cmnd);
extern int bnx2i_send_iscsi_text(struct bnx2i_conn *conn,
				 struct bnx2i_cmd *cmnd);
extern int bnx2i_send_iscsi_tmf(struct bnx2i_conn *conn,
				struct bnx2i_cmd *cmnd);
extern int bnx2i_send_iscsi_scsicmd(struct bnx2i_conn *conn,
				    struct bnx2i_cmd *cmnd);
extern int bnx2i_send_iscsi_nopout(struct bnx2i_conn *conn,
				   struct bnx2i_cmd *cmnd,
				   char *datap, int data_len);
extern int bnx2i_send_iscsi_logout(struct bnx2i_conn *conn,
				   struct bnx2i_cmd *cmnd);
extern void bnx2i_send_cmd_cleanup_req(struct bnx2i_hba *hba,
				       struct bnx2i_cmd *cmd);
extern int bnx2i_send_conn_ofld_req(struct bnx2i_hba *hba,
				     struct bnx2i_endpoint *ep);
extern int bnx2i_update_iscsi_conn(struct bnx2i_conn *conn);
extern int bnx2i_send_conn_destroy(struct bnx2i_hba *hba,
				    struct bnx2i_endpoint *ep);
extern int bnx2i_alloc_qp_resc(struct bnx2i_hba *hba,
			       struct bnx2i_endpoint *ep);
extern void bnx2i_free_qp_resc(struct bnx2i_hba *hba,
			       struct bnx2i_endpoint *ep);
extern void bnx2i_ep_ofld_timer(unsigned long data);
struct bnx2i_endpoint *bnx2i_find_ep_in_ofld_list(struct bnx2i_hba *hba,
						  u32 iscsi_cid);
struct bnx2i_endpoint *bnx2i_find_ep_in_destroy_list(struct bnx2i_hba *hba,
						     u32 iscsi_cid);
void bnx2i_ring_sq_dbell_bnx2(struct bnx2i_conn *conn);
void bnx2i_ring_sq_dbell_bnx2x(struct bnx2i_conn *conn);
int bnx2i_map_ep_dbell_regs(struct bnx2i_endpoint *ep);

void bnx2i_arm_cq_event_coalescing(struct bnx2i_endpoint *ep, u8 action);

int bnx2i_register_xport(struct bnx2i_hba *hba);
int bnx2i_deregister_xport(struct bnx2i_hba *hba);
int bnx2i_free_iscsi_scsi_template(struct bnx2i_hba *hba);
void bnx2i_update_conn_activity_counter(struct bnx2i_conn *conn);

/* Debug related function prototypes */
extern void bnx2i_print_pend_cmd_queue(struct bnx2i_conn *conn);
extern void bnx2i_print_active_cmd_queue(struct bnx2i_conn *conn);
extern void bnx2i_print_xmit_pdu_queue(struct bnx2i_conn *conn);
extern void bnx2i_print_recv_state(struct bnx2i_conn *conn);
extern void bnx2i_print_cqe(struct bnx2i_conn *conn);
extern void bnx2i_print_sqe(struct bnx2i_conn *conn);

extern int bnx2i_get_stats(void *handle);

#ifdef __VMKLNX__
#define bnx2i_setup_ictx_dump(__hba, __conn)	do { } while (0)
#define  bnx2i_sysfs_setup()			do { } while (0)
#define  bnx2i_sysfs_cleanup()			do { } while (0)
#define  bnx2i_register_sysfs(__hba)		0
#define  bnx2i_unregister_sysfs(__hba)		do { } while (0)
#define bnx2i_init_mips_idle_counters(__hba)	do { } while (0)

int bnx2i_proc_info(struct Scsi_Host *shost, char *buffer, char **start,
		    off_t offset, int length, int inout);
#else
extern void bnx2i_setup_ictx_dump(struct bnx2i_hba *hba,
				  struct bnx2i_conn *conn);
extern int bnx2i_sysfs_setup(void);
extern void bnx2i_sysfs_cleanup(void);
extern int bnx2i_register_sysfs(struct bnx2i_hba *hba);
extern void bnx2i_unregister_sysfs(struct bnx2i_hba *hba);
void bnx2i_init_mips_idle_counters(struct bnx2i_hba *hba);
void bnx2i_tcp_port_new_entry(u16 tcp_port);
#endif

#endif
