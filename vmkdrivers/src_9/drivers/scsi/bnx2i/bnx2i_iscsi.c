/*
 * QLogic NetXtreme II iSCSI offload driver.
 * Copyright (c)   2003-2014 QLogic Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * Written by: Anil Veerabhadrappa (anilgv@broadcom.com)
 */

#include "bnx2i.h"
#include <linux/ethtool.h>
#ifdef __VMKLNX__
#if (VMWARE_ESX_DDK_VERSION == 41000)
#include <vmklinux26/vmklinux26_scsi.h>
#else
#include <vmklinux_9/vmklinux_scsi.h>
#endif
#endif	/* __VMKLNX__ */
#include <scsi/scsi_transport.h>
#include <scsi/scsi_transport_iscsi.h>

struct scsi_host_template bnx2i_host_template;
struct iscsi_transport bnx2i_iscsi_transport;
struct file_operations bnx2i_mgmt_fops;
extern unsigned int bnx2i_nopout_when_cmds_active;
extern unsigned int tcp_buf_size;
extern unsigned int en_tcp_dack;
extern unsigned int time_stamps;
extern unsigned int en_hba_poll;
#if (VMWARE_ESX_DDK_VERSION >= 50000)
extern unsigned int bnx2i_esx_mtu_max;
#endif

/*
 * Global endpoint resource info
 */
static void *bnx2i_ep_pages[MAX_PAGES_PER_CTRL_STRUCT_POOL];
static struct list_head bnx2i_free_ep_list;
static struct list_head bnx2i_unbound_ep;
static u32 bnx2i_num_free_ep;
static u32 bnx2i_max_free_ep;
static DEFINE_SPINLOCK(bnx2i_resc_lock); /* protects global resources */
struct tcp_port_mngt bnx2i_tcp_port_tbl;

extern unsigned int sq_size;
extern unsigned int rq_size;


int use_poll_timer = 1;

#ifndef __VMKLNX__
/* Char device major number */
static int bnx2i_major_no;
#endif

static struct io_bdt *bnx2i_alloc_bd_table(struct bnx2i_sess *sess,
					   struct bnx2i_cmd *);

static struct scsi_host_template *
bnx2i_alloc_scsi_host_template(struct bnx2i_hba *hba, struct cnic_dev *cnic);
static void
bnx2i_free_scsi_host_template(struct scsi_host_template *scsi_template);
static struct iscsi_transport *
bnx2i_alloc_iscsi_transport(struct bnx2i_hba *hba, struct cnic_dev *cnic, struct scsi_host_template *);
static void bnx2i_free_iscsi_transport(struct iscsi_transport *iscsi_transport);
static void bnx2i_release_session_resc(struct iscsi_cls_session *cls_session);

#ifdef __VMKLNX__
static void bnx2i_conn_main_worker(unsigned long data);
vmk_int32 bnx2i_get_570x_limit(struct iscsi_transport *transport, enum iscsi_param param,
                              vmk_IscsiTransTransportParamLimits *limit, vmk_int32 maxListLen);
vmk_int32 bnx2i_get_5771x_limit(struct iscsi_transport *transport, enum iscsi_param param,
                               vmk_IscsiTransTransportParamLimits *limit, vmk_int32 maxListLen);
extern int bnx2i_cqe_work_pending(struct bnx2i_conn *conn);
#else
#if defined(INIT_DELAYED_WORK_DEFERRABLE) || defined(INIT_WORK_NAR) || (defined(__VMKLNX__) && (VMWARE_ESX_DDK_VERSION >= 50000))
static void bnx2i_conn_main_worker(struct work_struct *work);
#else
static void bnx2i_conn_main_worker(void *data);
extern int bnx2i_cqe_work_pending(struct bnx2i_conn *conn);
#endif
#endif	/*__VMKLNX__ */
#if defined(INIT_DELAYED_WORK_DEFERRABLE) || defined(INIT_WORK_NAR) || (defined(__VMKLNX__) && (VMWARE_ESX_DDK_VERSION >= 50000))
static void ep_tmo_poll_task(struct work_struct *work);
#else
static void ep_tmo_poll_task(void *data);
#endif
static void bnx2i_xmit_work_send_cmd(struct bnx2i_conn *conn, struct bnx2i_cmd *cmd);
static void bnx2i_cleanup_task_context(struct bnx2i_sess *sess,
					struct bnx2i_cmd *cmd, int reason);


#ifdef __VMKLNX__
extern int bnx2i_bind_adapter_devices(struct bnx2i_hba *hba);
#endif
void bnx2i_unbind_adapter_devices(struct bnx2i_hba *hba);
static void bnx2i_conn_poll(unsigned long data);
vmk_int32 bnx2i_conn_stop(struct iscsi_cls_conn *cls_conn, vmk_int32 flag);

/*
 * iSCSI Session's hostdata organization:
 *
 *    *------------------* <== hostdata_session(host->hostdata)
 *    | ptr to class sess|
 *    |------------------| <== iscsi_hostdata(host->hostdata)
 *    | iscsi_session    |
 *    *------------------*
 */

#define hostdata_privsize(_sz)	(sizeof(unsigned long) + _sz + \
				 _sz % sizeof(unsigned long))
#define hostdata_session(_hostdata) (iscsi_ptr(*(unsigned long *)_hostdata))

#define session_to_cls(_sess) 	hostdata_session(_sess->shost->hostdata)


#ifdef __VMKLNX__
void bnx2i_hba_poll_timer(unsigned long data)
{
	struct bnx2i_hba *hba = (struct bnx2i_hba *) data;
	struct bnx2i_sess *sess;
	int iscsi_cid;
	u32 src_ip;
	u32 dst_ip;
	u16 tcp_port;

	spin_lock(&hba->lock);
	list_for_each_entry(sess, &hba->active_sess, link) {

		if (sess->lead_conn && sess->lead_conn->ep) {
			iscsi_cid = sess->lead_conn->ep->ep_iscsi_cid;
			tcp_port = be16_to_cpu(sess->lead_conn->ep->cm_sk->src_port);
			src_ip = be32_to_cpu(sess->lead_conn->ep->cm_sk->src_ip[0]);
			dst_ip = be32_to_cpu(sess->lead_conn->ep->cm_sk->dst_ip[0]);
		} else {
			iscsi_cid = 0xFF;
			tcp_port = src_ip = dst_ip = 0;
		}
		printk(KERN_ALERT "hba_poll[%p] = %p, %lu, %lu, {%d, %x, %x, %x}, "
				 "{%u, %u, %u}, %u, {%u, %u}, %u, %u\n", hba,
				sess, jiffies - sess->timestamp, sess->recovery_state,
				iscsi_cid, src_ip, dst_ip, tcp_port,
				sess->cmdsn, sess->exp_cmdsn, sess->max_cmdsn,
				atomic_read(&sess->tmf_active),
				sess->active_cmd_count, sess->pend_cmd_count,
				sess->cmd_win_closed, sess->host_busy_cmd_win);
	}
	spin_unlock(&hba->lock);

	hba->hba_poll_timer.expires = jiffies + 10 * HZ;
	add_timer(&hba->hba_poll_timer);
}
#endif



/**
 * bnx2i_alloc_tcp_port - allocates a tcp port from the free list
 *
 * assumes this function is called with 'bnx2i_resc_lock' held
 **/
#ifndef __VMKLNX__
static u16 bnx2i_alloc_tcp_port(void)
{
	u16 tcp_port;

	if (!bnx2i_tcp_port_tbl.num_free_ports || !bnx2i_tcp_port_tbl.free_q)
		return 0;

	tcp_port = bnx2i_tcp_port_tbl.free_q[bnx2i_tcp_port_tbl.cons_idx];
	bnx2i_tcp_port_tbl.cons_idx++;
	bnx2i_tcp_port_tbl.cons_idx %= bnx2i_tcp_port_tbl.max_idx;
	bnx2i_tcp_port_tbl.num_free_ports--;

	return tcp_port;
}


/**
 * bnx2i_free_tcp_port - Frees the given tcp port back to free pool
 *
 * @port: 		tcp port number being freed
 *
 * assumes this function is called with 'bnx2i_resc_lock' held
 **/
static void bnx2i_free_tcp_port(u16 port)
{
	if (!bnx2i_tcp_port_tbl.free_q)
		return;

	bnx2i_tcp_port_tbl.free_q[bnx2i_tcp_port_tbl.prod_idx] = port;
	bnx2i_tcp_port_tbl.prod_idx++;
	bnx2i_tcp_port_tbl.prod_idx %= bnx2i_tcp_port_tbl.max_idx;
	bnx2i_tcp_port_tbl.num_free_ports++;
}

/**
 * bnx2i_tcp_port_new_entry - place 'bnx2id' allocated tcp port number
 *		to free list
 *
 * @port: 		tcp port number being added to free pool
 *
 * 'bnx2i_resc_lock' is held while operating on global tcp port table
 **/
void bnx2i_tcp_port_new_entry(u16 tcp_port)
{
	u32 idx = bnx2i_tcp_port_tbl.prod_idx;

	spin_lock(&bnx2i_resc_lock);
	bnx2i_tcp_port_tbl.free_q[idx] = tcp_port;
	bnx2i_tcp_port_tbl.prod_idx++;
	bnx2i_tcp_port_tbl.prod_idx %= bnx2i_tcp_port_tbl.max_idx;
	bnx2i_tcp_port_tbl.num_free_ports++;
	bnx2i_tcp_port_tbl.num_required--;
	spin_unlock(&bnx2i_resc_lock);
}

/**
 * bnx2i_init_tcp_port_mngr - initializes tcp port manager
 *
 */
int bnx2i_init_tcp_port_mngr(void)
{
	int mem_size;
	int rc = 0;

	bnx2i_tcp_port_tbl.num_free_ports = 0;
	bnx2i_tcp_port_tbl.prod_idx = 0;
	bnx2i_tcp_port_tbl.cons_idx = 0;
	bnx2i_tcp_port_tbl.max_idx = 0;
	bnx2i_tcp_port_tbl.num_required = 0;

#define BNX2I_MAX_TCP_PORTS	1024

	bnx2i_tcp_port_tbl.port_tbl_size = BNX2I_MAX_TCP_PORTS;

	mem_size = sizeof(u16) * bnx2i_tcp_port_tbl.port_tbl_size;
	if (bnx2i_tcp_port_tbl.port_tbl_size) {
		bnx2i_tcp_port_tbl.free_q = kmalloc(mem_size, GFP_KERNEL);

		if (bnx2i_tcp_port_tbl.free_q)
			bnx2i_tcp_port_tbl.max_idx =
				bnx2i_tcp_port_tbl.port_tbl_size;
		else
			rc = -ENOMEM;
	}

	return rc;
}


/**
 * bnx2i_cleanup_tcp_port_mngr - frees memory held by global tcp port table
 *
 */
void bnx2i_cleanup_tcp_port_mngr(void)
{
	kfree(bnx2i_tcp_port_tbl.free_q);
	bnx2i_tcp_port_tbl.free_q = NULL;
	bnx2i_tcp_port_tbl.num_free_ports = 0;
}
#endif

static int bnx2i_adapter_ready(struct bnx2i_hba *hba)
{
	int retval = 0;

	if (!hba || !test_bit(ADAPTER_STATE_UP, &hba->adapter_state) ||
	    test_bit(ADAPTER_STATE_GOING_DOWN, &hba->adapter_state) ||
	    test_bit(ADAPTER_STATE_LINK_DOWN, &hba->adapter_state))
		retval = -EPERM;
	return retval;
}


/**
 * bnx2i_get_write_cmd_bd_idx - identifies various BD bookmarks for a
 *			scsi write command
 *
 * @cmd:		iscsi cmd struct pointer
 * @buf_off:		absolute buffer offset
 * @start_bd_off:	u32 pointer to return the offset within the BD
 *			indicated by 'start_bd_idx' on which 'buf_off' falls
 * @start_bd_idx:	index of the BD on which 'buf_off' falls
 *
 * identifies & marks various bd info for imm data, unsolicited data
 *	and the first solicited data seq.
 */
static void bnx2i_get_write_cmd_bd_idx(struct bnx2i_cmd *cmd, u32 buf_off,
				       u32 *start_bd_off, u32 *start_bd_idx)
{
	u32 cur_offset = 0;
	u32 cur_bd_idx = 0;
	struct iscsi_bd *bd_tbl;

	if (!cmd->bd_tbl || !cmd->bd_tbl->bd_tbl)
		return;

	bd_tbl = cmd->bd_tbl->bd_tbl;
	if (buf_off) {
		while (buf_off >= (cur_offset + bd_tbl->buffer_length)) {
			cur_offset += bd_tbl->buffer_length;
			cur_bd_idx++;
			bd_tbl++;
		}
	}

	*start_bd_off = buf_off - cur_offset;
	*start_bd_idx = cur_bd_idx;
}

/**
 * bnx2i_setup_write_cmd_bd_info - sets up BD various information for
 *			scsi write command
 *
 * @cmd:		iscsi cmd struct pointer
 *
 * identifies & marks various bd info for immediate data, unsolicited data
 *	and first solicited data seq which includes BD start index & BD buf off
 *	This function takes into account iscsi parameter such as immediate data
 *	and unsolicited data is support on this connection
 *	
 */
static void bnx2i_setup_write_cmd_bd_info(struct bnx2i_cmd *cmd)
{
	struct bnx2i_sess *sess;
	u32 start_bd_offset;
	u32 start_bd_idx; 
	u32 buffer_offset = 0;
	u32 seq_len = 0;
	u32 fbl, mrdsl;
	u32 cmd_len = cmd->req.total_data_transfer_length;

	sess = cmd->conn->sess;

	/* if ImmediateData is turned off & IntialR2T is turned on,
	 * there will be no immediate or unsolicited data, just return.
	 */
	if (sess->initial_r2t && !sess->imm_data)
		return;

	fbl = sess->first_burst_len;
	mrdsl = cmd->conn->max_data_seg_len_xmit;

	/* Immediate data */
	if (sess->imm_data) {
		seq_len = min(mrdsl, fbl);
		seq_len = min(cmd_len, seq_len);
		buffer_offset += seq_len;
	}

	if (seq_len == cmd_len)
		return;

	if (!sess->initial_r2t) {
		if (seq_len >= fbl)
			goto r2t_data;
		seq_len = min(fbl, cmd_len) - seq_len;
		bnx2i_get_write_cmd_bd_idx(cmd, buffer_offset,
					   &start_bd_offset, &start_bd_idx);
		cmd->req.ud_buffer_offset = start_bd_offset;
		cmd->req.ud_start_bd_index = start_bd_idx;
		buffer_offset += seq_len;
	}
r2t_data:
	if (buffer_offset != cmd_len) {
		bnx2i_get_write_cmd_bd_idx(cmd, buffer_offset,
					   &start_bd_offset, &start_bd_idx);
		if (start_bd_offset > fbl) {
			int i = 0;

			PRINT_ERR(sess->hba, "error, buf offset 0x%x "
				  "bd_valid %d use_sg %d\n", buffer_offset,
				  cmd->bd_tbl->bd_valid,
				  scsi_sg_count(cmd->scsi_cmd));
			for (i = 0; i < cmd->bd_tbl->bd_valid; i++)
				PRINT_ERR(sess->hba, "err, bd[%d]: len %x\n", i,
					  cmd->bd_tbl->bd_tbl[i].buffer_length);
		}
		cmd->req.sd_buffer_offset = start_bd_offset;
		cmd->req.sd_start_bd_index = start_bd_idx;
	}
}


/**
 * bnx2i_split_bd - splits buffer > 64KB into 32KB chunks
 *
 * @cmd:		iscsi cmd struct pointer
 * @addr: 		base address of the buffer
 * @sg_len: 		buffer length
 * @bd_index: 		starting index into BD table
 *
 * This is not required as driver limits max buffer size of less than 64K by
 *	advertising 'max_sectors' within this limit. 5706/5708 hardware limits
 *	BD length to less than or equal to 0xFFFF 
 **/
static int bnx2i_split_bd(struct bnx2i_cmd *cmd, u64 addr, int sg_len,
			  int bd_index)
{
	struct iscsi_bd *bd = cmd->bd_tbl->bd_tbl;
	int frag_size, sg_frags;

	sg_frags = 0;
	while (sg_len) {
		if (sg_len >= BD_SPLIT_SIZE)
			frag_size = BD_SPLIT_SIZE;
		else
			frag_size = sg_len;
		bd[bd_index + sg_frags].buffer_addr_lo = (u32) addr;
		bd[bd_index + sg_frags].buffer_addr_hi = addr >> 32;
		bd[bd_index + sg_frags].buffer_length = frag_size;
		bd[bd_index + sg_frags].flags = 0;
		if ((bd_index + sg_frags) == 0)
			bd[0].flags = ISCSI_BD_FIRST_IN_BD_CHAIN;
		addr += (u64) frag_size;
		sg_frags++;
		sg_len -= frag_size;
	}
	return sg_frags;
}


/**
 * bnx2i_map_single_buf - maps a single buffer and updates the BD table
 *
 * @hba: 		adapter instance
 * @cmd:		iscsi cmd struct pointer
 *
 */
static int bnx2i_map_single_buf(struct bnx2i_hba *hba,
				       struct bnx2i_cmd *cmd)
{
	struct scsi_cmnd *sc = cmd->scsi_cmd;
	struct iscsi_bd *bd = cmd->bd_tbl->bd_tbl;
	int byte_count;
	int bd_count;
	u64 addr;

	byte_count = scsi_bufflen(sc);
	sc->SCp.dma_handle =
		pci_map_single(hba->pcidev, scsi_sglist(sc),
			       scsi_bufflen(sc), sc->sc_data_direction);
	addr = sc->SCp.dma_handle;

	if (byte_count > MAX_BD_LENGTH) {
		bd_count = bnx2i_split_bd(cmd, addr, byte_count, 0);
	} else {
		bd_count = 1;
		bd[0].buffer_addr_lo = addr & 0xffffffff;
		bd[0].buffer_addr_hi = addr >> 32;
		bd[0].buffer_length = scsi_bufflen(sc);
		bd[0].flags = ISCSI_BD_FIRST_IN_BD_CHAIN |
			      ISCSI_BD_LAST_IN_BD_CHAIN;
	}
	bd[bd_count - 1].flags |= ISCSI_BD_LAST_IN_BD_CHAIN;

	return bd_count;
}


/**
 * bnx2i_map_sg - maps IO buffer and prepares the BD table
 *
 * @hba: 		adapter instance
 * @cmd:		iscsi cmd struct pointer
 *
 * map SG list
 */
static int bnx2i_map_sg(struct bnx2i_hba *hba, struct bnx2i_cmd *cmd)
{
	struct scsi_cmnd *sc = cmd->scsi_cmd;
	struct iscsi_bd *bd = cmd->bd_tbl->bd_tbl;
	struct scatterlist *sg;
	int byte_count = 0;
	int sg_frags;
	int bd_count = 0;
	int sg_count;
	int sg_len;
	u64 addr;
	int i;

	sg = scsi_sglist(sc);
#ifdef __VMKLNX__
	sg_count = scsi_sg_count(sc);
#else
	sg_count = pci_map_sg(hba->pcidev, sg, scsi_sg_count(sc),
			      sc->sc_data_direction);
#endif

#if defined(__VMKLNX__) && !(VMWARE_ESX_DDK_VERSION == 41000)
	scsi_for_each_sg(sc, sg, sg_count, i) {
#else /* !(defined(__VMKLNX__) && !(VMWARE_ESX_DDK_VERSION == 41000)) */
	for (i = 0; i < sg_count; i++) {
#endif /* defined(__VMKLNX__) && !(VMWARE_ESX_DDK_VERSION == 41000) */
		sg_len = sg_dma_len(sg);
		addr = sg_dma_address(sg);
		if (sg_len > MAX_BD_LENGTH)
			sg_frags = bnx2i_split_bd(cmd, addr, sg_len,
						  bd_count);
		else {
			sg_frags = 1;
			bd[bd_count].buffer_addr_lo = addr & 0xffffffff;
			bd[bd_count].buffer_addr_hi = addr >> 32;
			bd[bd_count].buffer_length = sg_len;
			bd[bd_count].flags = 0;
			if (bd_count == 0)
				bd[bd_count].flags =
					ISCSI_BD_FIRST_IN_BD_CHAIN;
		}
		byte_count += sg_len;
#if defined(__VMKLNX__) && !(VMWARE_ESX_DDK_VERSION == 41000)
		bd_count += sg_frags;
	}

	/* do reset - for SG_VMK type */
	sg_reset(sg);
#else /* !(defined(__VMKLNX__) && !(VMWARE_ESX_DDK_VERSION == 41000)) */
		sg++;
		bd_count += sg_frags;
	}
#endif /* defined(__VMKLNX__) && !(VMWARE_ESX_DDK_VERSION == 41000) */
	bd[bd_count - 1].flags |= ISCSI_BD_LAST_IN_BD_CHAIN;

#ifdef __VMKLNX__
	VMK_ASSERT(byte_count == scsi_bufflen(sc));
#else
	BUG_ON(byte_count != scsi_bufflen(sc));
#endif /* __VMKLNX__ */
	return bd_count;
}

/**
 * bnx2i_iscsi_map_sg_list - maps SG list
 *
 * @cmd:		iscsi cmd struct pointer
 *
 * creates BD list table for the command
 */
static void bnx2i_iscsi_map_sg_list(struct bnx2i_hba *hba, struct bnx2i_cmd *cmd)
{
	struct scsi_cmnd *sc = cmd->scsi_cmd;
	int bd_count;

	if (scsi_sg_count(sc))
		bd_count = bnx2i_map_sg(hba, cmd);
	else if (scsi_bufflen(sc))
		bd_count = bnx2i_map_single_buf(hba, cmd);
	else {
		struct iscsi_bd *bd = cmd->bd_tbl->bd_tbl;
		bd_count  = 0;
		bd[0].buffer_addr_lo = bd[0].buffer_addr_hi = 0;
		bd[0].buffer_length = bd[0].flags = 0;
	}
	cmd->bd_tbl->bd_valid = bd_count;
}


/**
 * bnx2i_iscsi_unmap_sg_list - unmaps SG list
 *
 * @cmd:		iscsi cmd struct pointer
 *
 * unmap IO buffers and invalidate the BD table
 */
void bnx2i_iscsi_unmap_sg_list(struct bnx2i_hba *hba, struct bnx2i_cmd *cmd)
{
	struct scsi_cmnd *sc = cmd->scsi_cmd;
#ifndef __VMKLNX__
	struct scatterlist *sg;
#endif

	if (cmd->bd_tbl->bd_valid && sc) {
#ifndef __VMKLNX__
		if (scsi_sg_count(sc)) {
			sg = scsi_sglist(sc);
			pci_unmap_sg(hba->pcidev, sg, scsi_sg_count(sc),
				     sc->sc_data_direction);
		} else {
			pci_unmap_single(hba->pcidev, sc->SCp.dma_handle,
					 scsi_bufflen(sc),
					 sc->sc_data_direction);
		}
#endif
		cmd->bd_tbl->bd_valid = 0;
	}
}



static void bnx2i_setup_cmd_wqe_template(struct bnx2i_cmd *cmd)
{
	memset(&cmd->req, 0x00, sizeof(cmd->req));
	cmd->req.op_code = 0xFF;
	cmd->req.bd_list_addr_lo = (u32) cmd->bd_tbl->bd_tbl_dma;
	cmd->req.bd_list_addr_hi =
		(u32) ((u64) cmd->bd_tbl->bd_tbl_dma >> 32);

}


/**
 * bnx2i_unbind_conn_from_iscsi_cid - unbinds conn structure and 'iscsi_cid'
 *
 * @conn: 		pointer to iscsi connection
 * @iscsi_cid:		iscsi context ID, range 0 - (MAX_CONN - 1)
 *
 * Remove connection pointer from iscsi cid table entry. This is required
 *	to stop invalid pointer access if there are spurious KCQE indications
 *	after iscsi logout is performed
 */
static int bnx2i_unbind_conn_from_iscsi_cid(struct bnx2i_conn *conn,
					    u32 iscsi_cid)
{
	struct bnx2i_hba *hba;

	if (!conn || !conn->sess || !conn->sess->hba ||
	    (iscsi_cid >= conn->sess->hba->max_active_conns))
		return -EINVAL;

	hba = conn->sess->hba;

	if (hba && !hba->cid_que.conn_cid_tbl[iscsi_cid]) {
		PRINT_ERR(hba, "conn unbind - entry %d is already free\n",
				iscsi_cid);
		return -ENOENT;
	}

	hba->cid_que.conn_cid_tbl[iscsi_cid] = NULL;
	return 0;
}



/**
 * bnx2i_bind_conn_to_iscsi_cid - bind conn structure to 'iscsi_cid'
 *
 * @conn: 		pointer to iscsi connection
 * @iscsi_cid:		iscsi context ID, range 0 - (MAX_CONN - 1)
 *
 * update iscsi cid table entry with connection pointer. This enables
 *	driver to quickly get hold of connection structure pointer in
 *	completion/interrupt thread using iscsi context ID
 */
static int bnx2i_bind_conn_to_iscsi_cid(struct bnx2i_conn *conn,
					 u32 iscsi_cid)
{
	struct bnx2i_hba *hba;

	if (!conn || !conn->sess)
		return -EINVAL;

	hba = conn->sess->hba;

	if (!hba)
		return -EINVAL;

	if (hba->cid_que.conn_cid_tbl[iscsi_cid]) {
		PRINT_ERR(hba, "conn bind - entry #%d not free\n",
				iscsi_cid);
		return -EBUSY;
	}

	hba->cid_que.conn_cid_tbl[iscsi_cid] = conn;
	return 0;
}


/**
 * bnx2i_get_conn_from_id - maps an iscsi cid to corresponding conn ptr
 * 
 * @hba: 		pointer to adapter instance
 * @iscsi_cid:		iscsi context ID, range 0 - (MAX_CONN - 1)
 */
struct bnx2i_conn *bnx2i_get_conn_from_id(struct bnx2i_hba *hba,
						 u16 iscsi_cid)
{
	if (!hba->cid_que.conn_cid_tbl) {
		PRINT_ERR(hba, "ERROR - missing conn<->cid table\n");
		return NULL;

	} else if (iscsi_cid >= hba->max_active_conns) {
		PRINT_ERR(hba, "wrong cid #%d\n", iscsi_cid);
		return NULL;
	}
	return hba->cid_que.conn_cid_tbl[iscsi_cid];
}


/**
 * bnx2i_alloc_iscsi_cid - allocates a iscsi_cid from free pool
 *
 * @hba: 		pointer to adapter instance
 */
static u32 bnx2i_alloc_iscsi_cid(struct bnx2i_hba *hba)
{
	int idx;
	unsigned long flags;

	if (!hba->cid_que.cid_free_cnt)
		return ISCSI_RESERVED_TAG;

	spin_lock_irqsave(&hba->cid_que_lock, flags);
	idx = hba->cid_que.cid_q_cons_idx;
	hba->cid_que.cid_q_cons_idx++;
	if (hba->cid_que.cid_q_cons_idx == hba->cid_que.cid_q_max_idx)
		hba->cid_que.cid_q_cons_idx = 0;

	hba->cid_que.cid_free_cnt--;
	spin_unlock_irqrestore(&hba->cid_que_lock, flags);
	return hba->cid_que.cid_que[idx];
}


/**
 * bnx2i_free_iscsi_cid - returns tcp port to free list
 *
 * @hba: 		pointer to adapter instance
 * @iscsi_cid:		iscsi context ID to free
 */
static void bnx2i_free_iscsi_cid(struct bnx2i_hba *hba, u16 iscsi_cid)
{
	int idx;
	unsigned long flags;

	if (iscsi_cid == (u16)ISCSI_RESERVED_TAG)
		return;

	spin_lock_irqsave(&hba->cid_que_lock, flags);
	hba->cid_que.cid_free_cnt++;

	idx = hba->cid_que.cid_q_prod_idx;
	hba->cid_que.cid_que[idx] = iscsi_cid;
	hba->cid_que.conn_cid_tbl[iscsi_cid] = NULL;
	hba->cid_que.cid_q_prod_idx++;
	if (hba->cid_que.cid_q_prod_idx == hba->cid_que.cid_q_max_idx)
		hba->cid_que.cid_q_prod_idx = 0;
	spin_unlock_irqrestore(&hba->cid_que_lock, flags);
}


/**
 * bnx2i_setup_free_cid_que - sets up free iscsi cid queue
 *
 * @hba: 		pointer to adapter instance
 *
 * allocates memory for iscsi cid queue & 'cid - conn ptr' mapping table,
 * 	and initialize table attributes
 */
static int bnx2i_setup_free_cid_que(struct bnx2i_hba *hba)
{
	int mem_size;
	int i;

	mem_size = hba->max_active_conns * sizeof(u32);
	mem_size = (mem_size + (PAGE_SIZE - 1)) & PAGE_MASK;

	hba->cid_que.cid_que_base = kmalloc(mem_size, GFP_KERNEL);
	if (!hba->cid_que.cid_que_base)
		return -ENOMEM;

	mem_size = hba->max_active_conns * sizeof(struct bnx2i_conn *);
	mem_size = (mem_size + (PAGE_SIZE - 1)) & PAGE_MASK;
	hba->cid_que.conn_cid_tbl = kmalloc(mem_size, GFP_KERNEL);
	if (!hba->cid_que.conn_cid_tbl) {
		kfree(hba->cid_que.cid_que_base);
		hba->cid_que.cid_que_base = NULL;
		return -ENOMEM;
	}

	hba->cid_que.cid_que = (u32 *)hba->cid_que.cid_que_base;
	hba->cid_que.cid_q_prod_idx = 0;
	hba->cid_que.cid_q_cons_idx = 0;
	hba->cid_que.cid_q_max_idx = hba->max_active_conns;
	hba->cid_que.cid_free_cnt = hba->max_active_conns;

	for (i = 0; i < hba->max_active_conns; i++) {
		hba->cid_que.cid_que[i] = i;
		hba->cid_que.conn_cid_tbl[i] = NULL;
	}
	spin_lock_init(&hba->cid_que_lock);

	return 0;
}


/**
 * bnx2i_release_free_cid_que - releases 'iscsi_cid' queue resources
 *
 * @hba: 		pointer to adapter instance
 */
static void bnx2i_release_free_cid_que(struct bnx2i_hba *hba)
{
	kfree(hba->cid_que.cid_que_base);
	hba->cid_que.cid_que_base = NULL;

	kfree(hba->cid_que.conn_cid_tbl);
	hba->cid_que.conn_cid_tbl = NULL;
}

static void bnx2i_setup_bd_tbl(struct bnx2i_hba *hba, struct bnx2i_dma *dma)
{
	struct iscsi_bd *mp_bdt;
	int pages = dma->size / PAGE_SIZE;
	u64 addr;

	mp_bdt = (struct iscsi_bd *) dma->pgtbl;
	addr = (unsigned long) dma->mem;
	mp_bdt->flags = ISCSI_BD_FIRST_IN_BD_CHAIN;
	do {
		mp_bdt->buffer_addr_lo = addr & 0xffffffff;
		mp_bdt->buffer_addr_hi = addr >> 32;
		mp_bdt->buffer_length = PAGE_SIZE;

		pages--;
		if (!pages)
			break;

		addr += PAGE_SIZE;
		mp_bdt++;
		mp_bdt->flags = 0;
	} while (1);
	mp_bdt->flags |= ISCSI_BD_LAST_IN_BD_CHAIN;
}


/**
 * bnx2i_setup_570x_pgtbl - iscsi QP page table setup function
 *
 * @ep: 		endpoint (transport indentifier) structure
 *
 * Sets up page tables for SQ/RQ/CQ, 1G/sec (5706/5708/5709) devices requires
 * 	64-bit address in big endian format. Whereas 10G/sec (57710) requires
 * 	PT in little endian format
 */
void bnx2i_setup_570x_pgtbl(struct bnx2i_hba *hba, struct bnx2i_dma *dma, int pgtbl_off)
{
	int num_pages;
	u32 *ptbl;
	dma_addr_t page;
	char *pgtbl_virt;

	/* SQ page table */
	pgtbl_virt = dma->pgtbl;
	memset(pgtbl_virt, 0, dma->pgtbl_size);
	num_pages = dma->size / PAGE_SIZE;
	page = dma->mapping;

	ptbl = (u32 *) ((u8 *) dma->pgtbl + pgtbl_off);
	while (num_pages--) {
		/* PTE is written in big endian format for
		 * 5706/5708/5709 devices */
		*ptbl = (u32) ((u64) page >> 32);
		ptbl++;
		*ptbl = (u32) page;
		ptbl++;
		page += PAGE_SIZE;
	}
}

/**
 * bnx2i_setup_5771x_pgtbl - iscsi QP page table setup function
 *
 * @ep: 		endpoint (transport indentifier) structure
 *
 * Sets up page tables for SQ/RQ/CQ, 1G/sec (5706/5708/5709) devices requires
 * 	64-bit address in big endian format. Whereas 10G/sec (57710) requires
 * 	PT in little endian format
 */
void bnx2i_setup_5771x_pgtbl(struct bnx2i_hba *hba, struct bnx2i_dma *dma, int pgtbl_off)
{
	int num_pages;
	u32 *ptbl;
	dma_addr_t page;
	char *pgtbl_virt;

	/* SQ page table */
	pgtbl_virt = dma->pgtbl;
	memset(pgtbl_virt, 0, dma->pgtbl_size);
	num_pages = dma->size / PAGE_SIZE;
	page = dma->mapping;

	ptbl = (u32 *) ((u8 *) dma->pgtbl + pgtbl_off);
	while (num_pages--) {
		/* PTE is written in little endian format for 57710 */
		*ptbl = (u32) page;
		ptbl++;
		*ptbl = (u32) ((u64) page >> 32);
		ptbl++;
		page += PAGE_SIZE;
	}
}


void bnx2i_free_dma(struct bnx2i_hba *hba, struct bnx2i_dma *dma)
{
	if (dma->mem) {
		pci_free_consistent(hba->pcidev, dma->size, dma->mem,
				    dma->mapping);
		dma->mem = NULL;
	}
	if (dma->pgtbl && dma->pgtbl_type) {
		pci_free_consistent(hba->pcidev, dma->pgtbl_size,
				    dma->pgtbl, dma->pgtbl_map);
		dma->pgtbl = NULL;
	}
}


int bnx2i_alloc_dma(struct bnx2i_hba *hba, struct bnx2i_dma *dma,
		    int size, int pgtbl_type, int pgtbl_off)
{
	int pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

	dma->size = size;
	dma->pgtbl_type = pgtbl_type;

	dma->mem = pci_alloc_consistent(hba->pcidev, size, &dma->mapping);
	if (dma->mem == NULL)
		goto mem_err;

        if (!pgtbl_type)
                return 0;

        dma->pgtbl_size = ((pages * 8) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        dma->pgtbl = pci_alloc_consistent(hba->pcidev, dma->pgtbl_size,
                                          &dma->pgtbl_map);
        if (dma->pgtbl == NULL)
                goto mem_err;

	if (pgtbl_type == BNX2I_TBL_TYPE_PG)
        	hba->setup_pgtbl(hba, dma, pgtbl_off);
	if (pgtbl_type == BNX2I_TBL_TYPE_BD)
		bnx2i_setup_bd_tbl(hba, dma);

        return 0;

mem_err:
        bnx2i_free_dma(hba, dma);
        return -ENOMEM;
}



/**
 * bnx2i_alloc_ep - allocates ep structure from global pool
 *
 * @hba: 		pointer to adapter instance
 *
 * routine allocates a free endpoint structure from global pool and
 *	a tcp port to be used for this connection.  Global resource lock,
 *	'bnx2i_resc_lock' is held while accessing shared global data structures
 */
static struct bnx2i_endpoint *bnx2i_alloc_ep(struct bnx2i_hba *hba)
{
	struct bnx2i_endpoint *endpoint;
	struct list_head *listp;
	u16 tcp_port;

	spin_lock_bh(&bnx2i_resc_lock);

#ifdef __VMKLNX__
	tcp_port = 0;
#else
	tcp_port = bnx2i_alloc_tcp_port();
	if (!tcp_port) {
		PRINT_ERR(hba, "unable to allocate tcp ports, "
				"make sure 'bnx2id' is running\n");
		spin_unlock_bh(&bnx2i_resc_lock);
		return NULL;
	}
#endif
	if (list_empty(&bnx2i_free_ep_list)) {
		spin_unlock_bh(&bnx2i_resc_lock);
		PRINT_ERR(hba, "ep struct pool empty\n");
		return NULL;
	}
	listp = (struct list_head *) bnx2i_free_ep_list.next;
	list_del_init(listp);
	bnx2i_num_free_ep--;

	endpoint = (struct bnx2i_endpoint *) listp;
	endpoint->state = EP_STATE_IDLE;
	endpoint->hba = hba;
	endpoint->hba_age = hba->age;
	hba->ofld_conns_active++;
	endpoint->tcp_port = tcp_port;
	init_waitqueue_head(&endpoint->ofld_wait);
	endpoint->in_progress = 1;

	spin_unlock_bh(&bnx2i_resc_lock);
	return endpoint;
}


/**
 * bnx2i_free_ep - returns endpoint struct and tcp port to free pool
 *
 * @endpoint:		pointer to endpoint structure
 *
 */
static void bnx2i_free_ep(struct bnx2i_endpoint *endpoint)
{

	spin_lock_bh(&bnx2i_resc_lock);
	if (endpoint->in_progress == 1)
		endpoint->hba->ofld_conns_active--;

	bnx2i_free_iscsi_cid(endpoint->hba, endpoint->ep_iscsi_cid);
	endpoint->state = EP_STATE_IDLE;

	if (endpoint->conn) {
		endpoint->conn->ep = NULL;
		endpoint->conn = NULL;
	}
	endpoint->sess = NULL;

#ifndef __VMKLNX__
	if (endpoint->tcp_port)
		bnx2i_free_tcp_port(endpoint->tcp_port);
#endif

	endpoint->hba = NULL;
	endpoint->in_progress = 0;
	list_add_tail(&endpoint->link, &bnx2i_free_ep_list);
	bnx2i_num_free_ep++;
	spin_unlock_bh(&bnx2i_resc_lock);
}


/**
 * bnx2i_alloc_ep_pool - alloccates a pool of endpoint structures
 *
 * allocates free pool of endpoint structures, which is used to store
 *	QP related control & PT info and other option-2 information
 */
int bnx2i_alloc_ep_pool(void)
{
	struct bnx2i_endpoint *endpoint;
	int index;
	int ret_val = 0;
	int total_endpoints;
	int page_count = 0;
	void *mem_ptr;
	int mem_size;

	spin_lock_init(&bnx2i_resc_lock);
	INIT_LIST_HEAD(&bnx2i_free_ep_list);
	INIT_LIST_HEAD(&bnx2i_unbound_ep);

	for (index = 0; index < MAX_PAGES_PER_CTRL_STRUCT_POOL; index++) {
		bnx2i_ep_pages[index] = NULL;
	}

	total_endpoints = ISCSI_MAX_CONNS_PER_HBA * ISCSI_MAX_ADAPTERS;
	bnx2i_num_free_ep = 0;
	mem_size = total_endpoints * sizeof(struct bnx2i_endpoint);
	mem_ptr = vmalloc(mem_size);
	if (!mem_ptr) {
		printk(KERN_ERR "ep_pool: mem alloc failed\n");
		goto mem_alloc_err;
	}
	memset(mem_ptr, 0, mem_size);

	bnx2i_ep_pages[page_count++] = mem_ptr;
	endpoint = mem_ptr;

	for (index = 0; index < total_endpoints; index++) {
		INIT_LIST_HEAD(&endpoint->link);
		list_add_tail(&endpoint->link, &bnx2i_free_ep_list);
		endpoint++;
		bnx2i_num_free_ep++;
	}
mem_alloc_err:
	if (bnx2i_num_free_ep == 0)
		ret_val = -ENOMEM;

	bnx2i_max_free_ep = bnx2i_num_free_ep;
	return ret_val;
}


/**
 * bnx2i_release_ep_pool - releases memory resources held by endpoint structs
 */
void bnx2i_release_ep_pool(void)
{
	int index;
	void *mem_ptr;

	for (index = 0; index < MAX_PAGES_PER_CTRL_STRUCT_POOL; index++) {
		mem_ptr = bnx2i_ep_pages[index];
		vfree(mem_ptr);
		bnx2i_ep_pages[index] = NULL;
	}
	bnx2i_num_free_ep = 0;
	return;
}

/**
 * bnx2i_alloc_cmd - allocates a command structure from free pool
 *
 * @sess:		iscsi session pointer
 *
 * allocates a command structures and ITT from free pool
 */
struct bnx2i_cmd *bnx2i_alloc_cmd(struct bnx2i_sess *sess)
{
	struct bnx2i_cmd *cmd;
	struct list_head *listp;

	if (unlikely(!sess || (sess->num_free_cmds == 0))) {
		return NULL;
	}

	if (list_empty(&sess->free_cmds) && sess->num_free_cmds) {
		/* this is wrong */
		PRINT_ERR(sess->hba, "%s: alloc %d, freed %d, num_free %d\n",
			  __FUNCTION__, sess->total_cmds_allocated,
			  sess->total_cmds_freed, sess->num_free_cmds);
		return NULL;
	}

	listp = (struct list_head *) sess->free_cmds.next;
	list_del_init(listp);
	sess->num_free_cmds--;
	sess->total_cmds_allocated++;
	cmd = (struct bnx2i_cmd *) listp;
	cmd->scsi_status_rcvd = 0;

	INIT_LIST_HEAD(&cmd->link);
	bnx2i_setup_cmd_wqe_template(cmd);

	cmd->req.itt = cmd->itt;

	return cmd;
}


/**
 * bnx2i_free_cmd - releases iscsi cmd struct & ITT to respective free pool
 *
 * @sess:		iscsi session pointer
 * @cmd:		iscsi cmd pointer
 *
 * return command structure and ITT back to free pool.
 */
void bnx2i_free_cmd(struct bnx2i_sess *sess, struct bnx2i_cmd *cmd)
{
	if (atomic_read(&cmd->cmd_state) == ISCSI_CMD_STATE_FREED) {
		PRINT_ALERT(sess->hba, "double freeing cmd %p\n", cmd);
		return;
	}
	list_del_init(&cmd->link);
	list_add_tail(&cmd->link, &sess->free_cmds);
	sess->num_free_cmds++;
	sess->total_cmds_freed++;
	atomic_set(&cmd->cmd_state, ISCSI_CMD_STATE_FREED);

}


/**
 * bnx2i_alloc_cmd_pool - allocates and initializes iscsi command pool
 *
 * @sess:		iscsi session pointer
 *
 * Allocate command structure pool for a given iSCSI session. Return 'ENOMEM'
 *	if memory allocation fails
 */
int bnx2i_alloc_cmd_pool(struct bnx2i_sess *sess)
{
	struct bnx2i_cmd *cmdp;
	int index, count;
	int ret_val = 0;
	int total_cmds;
	int num_cmds;
	int page_count;
	int num_cmds_per_page;
	void *mem_ptr;
	u32 mem_size;
	int cmd_i;

	INIT_LIST_HEAD(&sess->free_cmds);
	for (index = 0; index < MAX_PAGES_PER_CTRL_STRUCT_POOL; index++)
		sess->cmd_pages[index] = NULL;

	num_cmds_per_page = PAGE_SIZE / sizeof(struct bnx2i_cmd);

	total_cmds = sess->sq_size;
	mem_size = sess->sq_size * sizeof(struct bnx2i_cmd *);
	mem_size = (mem_size + (PAGE_SIZE - 1)) & PAGE_MASK;
	sess->itt_cmd = kmalloc(mem_size, GFP_KERNEL);
	if (!sess->itt_cmd)
		return -ENOMEM;

	memset(sess->itt_cmd, 0x00, mem_size);

	cmd_i = 0;
	page_count = 0;
	for (index = 0; index < total_cmds;) {
		mem_ptr = kmalloc(PAGE_SIZE, GFP_KERNEL);
		if (mem_ptr == NULL)
			break;

		sess->cmd_pages[page_count++] = mem_ptr;
		num_cmds = num_cmds_per_page;
		if ((total_cmds - index) < num_cmds_per_page)
			num_cmds = (total_cmds - index);

		memset(mem_ptr, 0, PAGE_SIZE);
		cmdp = mem_ptr;
		for (count = 0; count < num_cmds; count++) {
			cmdp->req.itt = ITT_INVALID_SIGNATURE;
			INIT_LIST_HEAD(&cmdp->link);
			cmdp->itt = cmd_i;
			sess->itt_cmd[cmd_i] = cmdp;
			cmd_i++;

			/* Allocate BD table */
			cmdp->bd_tbl = bnx2i_alloc_bd_table(sess, cmdp);
			if (!cmdp->bd_tbl) {
				/* should never fail, as it's guaranteed to have
				 * (ISCSI_MAX_CMDS_PER_SESS + 1) BD tables
				 * allocated before calling this function.
				 */
				PRINT_ERR(sess->hba, "no BD table cmd %p\n",
					  cmdp);
				goto bd_table_failed;
			}
			list_add_tail(&cmdp->link, &sess->free_cmds);
			cmdp++;
		}

		sess->num_free_cmds += num_cmds;
		index += num_cmds;
	}
	sess->allocated_cmds = sess->num_free_cmds;

	if (sess->num_free_cmds == 0)
		ret_val = -ENOMEM;
	return ret_val;

bd_table_failed:
	return -ENOMEM;
}


/**
 * bnx2i_free_cmd_pool - releases memory held by free iscsi cmd pool
 *
 * @sess:		iscsi session pointer
 *
 * Release memory held by command struct pool.
 */
void bnx2i_free_cmd_pool(struct bnx2i_sess *sess)
{
	int index;
	void *mem_ptr;

	if (sess->num_free_cmds != sess->allocated_cmds) {
		/*
		 * WARN: either there is some command struct leak or
		 * still some SCSI commands are pending.
		 */
		PRINT_ERR(sess->hba, "missing cmd structs - %d, %d\n",
			  sess->num_free_cmds, sess->allocated_cmds);
	}
	for (index = 0; index < MAX_PAGES_PER_CTRL_STRUCT_POOL; index++) {
		mem_ptr = sess->cmd_pages[index];
		if(mem_ptr) {
			kfree(mem_ptr);
			sess->cmd_pages[index] = NULL;
		}
	}
	sess->num_free_cmds = sess->allocated_cmds = 0;

	if(sess->itt_cmd) {
		kfree(sess->itt_cmd);
		sess->itt_cmd = NULL;
	}

	return;
}

static struct bnx2i_scsi_task *bnx2i_alloc_scsi_task(struct bnx2i_sess *sess)
{
	struct list_head *listp;
	if (list_empty(&sess->scsi_task_list)) {
		return NULL;
	}
	listp = (struct list_head *) sess->scsi_task_list.next;
	list_del_init(listp);
	return (struct bnx2i_scsi_task *)listp;
}

static void bnx2i_free_scsi_task(struct bnx2i_sess *sess,
				 struct bnx2i_scsi_task *scsi_task)
{
	list_del_init((struct list_head *)scsi_task);
	scsi_task->scsi_cmd = NULL;
	list_add_tail(&scsi_task->link, &sess->scsi_task_list);
}

extern int bnx2i_max_task_pgs;
static int bnx2i_alloc_scsi_task_pool(struct bnx2i_sess *sess)
{
	struct bnx2i_scsi_task *scsi_task;
	int mem_size;
	int task_count;
	int i;

	if (bnx2i_max_task_pgs > 8)
		bnx2i_max_task_pgs = 8;
	else if (bnx2i_max_task_pgs < 2)
		bnx2i_max_task_pgs = 2;
	mem_size = bnx2i_max_task_pgs * PAGE_SIZE;
	INIT_LIST_HEAD(&sess->scsi_task_list);
	sess->task_list_mem = kmalloc(mem_size, GFP_KERNEL);
	if (!sess->task_list_mem)
		return -ENOMEM;

	scsi_task = (struct bnx2i_scsi_task *)sess->task_list_mem;
	task_count = mem_size / sizeof(struct bnx2i_scsi_task);
	sess->max_iscsi_tasks = task_count;
	for (i = 0; i < task_count; i++, scsi_task++) {
		INIT_LIST_HEAD(&scsi_task->link);
		scsi_task->scsi_cmd = NULL;
		list_add_tail(&scsi_task->link, &sess->scsi_task_list);
	}
	return 0;
}

static void bnx2i_free_scsi_task_pool(struct bnx2i_sess *sess)
{
	if(sess->task_list_mem) {
		kfree(sess->task_list_mem);
		sess->task_list_mem = NULL;
	}
	INIT_LIST_HEAD(&sess->scsi_task_list);
/*TODO - clear pend list too */
}

/**
 * bnx2i_alloc_bd_table - Alloc BD table to associate with this iscsi cmd 
 *
 * @sess:		iscsi session pointer
 * @cmd:		iscsi cmd pointer
 *
 * allocates a BD table and assigns it to given command structure. There is
 *	no synchronization issue as this code is executed in initialization
 *	thread
 */
static struct io_bdt *bnx2i_alloc_bd_table(struct bnx2i_sess *sess,
					   struct bnx2i_cmd *cmd)
{
	struct io_bdt *bd_tbl;

	if (list_empty(&sess->bd_tbl_list))
		return NULL;

	bd_tbl = (struct io_bdt *)sess->bd_tbl_list.next;
	list_del(&bd_tbl->link);
	list_add_tail(&bd_tbl->link, &sess->bd_tbl_active);
	bd_tbl->bd_valid = 0;
	bd_tbl->cmdp = cmd;

	return bd_tbl;
}


/**
 * bnx2i_free_all_bdt_resc_pages - releases memory held by BD memory tracker tbl
 *
 * @sess:		iscsi session pointer
 *
 * Free up memory pages allocated held by BD resources
 */
static void bnx2i_free_all_bdt_resc_pages(struct bnx2i_sess *sess)
{
	int i;
	struct bd_resc_page *resc_page;

	spin_lock(&sess->lock);
	while (!list_empty(&sess->bd_resc_page)) {
		resc_page = (struct bd_resc_page *)sess->bd_resc_page.prev;
		list_del(sess->bd_resc_page.prev);
		if(!resc_page)
			continue;
		for(i = 0; i < resc_page->num_valid; i++) {
			if(resc_page->page[i])
				kfree(resc_page->page[i]);
		}
		kfree(resc_page);
	}
	spin_unlock(&sess->lock);
}



/**
 * bnx2i_alloc_bdt_resc_page - allocated a page to track BD table memory
 *
 * @sess:		iscsi session pointer
 *
 * allocated a page to track BD table memory
 */
struct bd_resc_page *bnx2i_alloc_bdt_resc_page(struct bnx2i_sess *sess)
{
	void *mem_ptr;
	struct bd_resc_page *resc_page;

	mem_ptr = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!mem_ptr)
		return NULL;

	resc_page = mem_ptr;
	list_add_tail(&resc_page->link, &sess->bd_resc_page);
	resc_page->max_ptrs = (PAGE_SIZE -
		(u32) &((struct bd_resc_page *) 0)->page[0]) / sizeof(void *);
	resc_page->num_valid = 0;

	return resc_page;
}


/**
 * bnx2i_add_bdt_resc_page - add newly allocated memory page to list
 *
 * @sess:		iscsi session pointer
 * @bd_page:		pointer to page memory
 *
 * link newly allocated memory page to tracker list
 */
int bnx2i_add_bdt_resc_page(struct bnx2i_sess *sess, void *bd_page)
{
	struct bd_resc_page *resc_page;

#define is_resc_page_full(_resc_pg) (_resc_pg->num_valid == _resc_pg->max_ptrs)
#define active_resc_page(_resc_list) 	\
			(list_empty(_resc_list) ? NULL : (_resc_list)->prev)
	if (list_empty(&sess->bd_resc_page)) {
		resc_page = bnx2i_alloc_bdt_resc_page(sess);
	} else {
		resc_page = (struct bd_resc_page *)
					active_resc_page(&sess->bd_resc_page);
	}

	if (!resc_page)
		return -ENOMEM;

	resc_page->page[resc_page->num_valid++] = bd_page;
	if (is_resc_page_full(resc_page)) {
		bnx2i_alloc_bdt_resc_page(sess);
	}
	return 0;
}


/**
 * bnx2i_alloc_bd_table_pool - Allocates buffer descriptor (BD) pool
 *
 * @sess:		iscsi session pointer
 *
 * Allocate a pool of buffer descriptor tables and associated DMA'able memory
 *	to be used with the session.
 */
static int bnx2i_alloc_bd_table_pool(struct bnx2i_sess *sess)
{
	int index, count;
	int ret_val = 0;
	int num_elem_per_page;
	int num_pages;
	struct io_bdt *bdt_info;
	void *mem_ptr;
	int bd_tbl_size;
	u32 mem_size;
	int total_bd_tbl;
	struct bnx2i_dma *dma;

	INIT_LIST_HEAD(&sess->bd_resc_page);
	INIT_LIST_HEAD(&sess->bd_tbl_list);
	INIT_LIST_HEAD(&sess->bd_tbl_active);

	total_bd_tbl = sess->sq_size;
	mem_size = total_bd_tbl * sizeof(struct io_bdt);
	num_elem_per_page = PAGE_SIZE / sizeof(struct io_bdt);

	for (index = 0; index < total_bd_tbl; index += num_elem_per_page) {
		if (((total_bd_tbl - index) * sizeof(struct io_bdt))
		    >= PAGE_SIZE) {
			mem_size = PAGE_SIZE;
			num_elem_per_page = PAGE_SIZE / sizeof(struct io_bdt);
		} else {
			mem_size =
				(total_bd_tbl - index) * sizeof(struct io_bdt);
			num_elem_per_page = (total_bd_tbl - index);
		}
		mem_ptr = kmalloc(mem_size, GFP_KERNEL);
		if (mem_ptr == NULL) {
			PRINT_ERR(sess->hba, "alloc_bd_tbl: kmalloc failed\n");
			ret_val = -ENOMEM;
			goto resc_alloc_failed;
		}
		bnx2i_add_bdt_resc_page(sess, mem_ptr);

		memset(mem_ptr, 0, mem_size);
		bdt_info = mem_ptr;
		for (count = 0; count < num_elem_per_page; count++) {
			INIT_LIST_HEAD(&bdt_info->link);
			list_add_tail(&bdt_info->link, &sess->bd_tbl_list);
			bdt_info++;
		}
	}


	INIT_LIST_HEAD(&sess->bdt_dma_resc);

	bd_tbl_size = ISCSI_MAX_BDS_PER_CMD * sizeof(struct iscsi_bd);
	bdt_info = (struct io_bdt *)sess->bd_tbl_list.next;
	num_elem_per_page = PAGE_SIZE / bd_tbl_size;

	num_pages = ((sess->sq_size * bd_tbl_size) + PAGE_SIZE - 1) &
		    ~(PAGE_SIZE - 1);
	num_pages /= PAGE_SIZE;
	sess->bdt_dma_info = kmalloc(sizeof(*dma) * num_pages, GFP_KERNEL);
	if (sess->bdt_dma_info == NULL)
		goto resc_alloc_failed;

	memset(sess->bdt_dma_info, 0, num_pages * sizeof(*dma));
	dma = (struct bnx2i_dma *)sess->bdt_dma_info;
	while (bdt_info && (bdt_info != (struct io_bdt *)&sess->bd_tbl_list)) {
		if (bnx2i_alloc_dma(sess->hba, dma, PAGE_SIZE, 0, 0)) {
			PRINT_ERR(sess->hba, "bd_tbl: DMA mem alloc failed\n");
			ret_val = -ENOMEM;
			goto dma_alloc_failed;
		}
		list_add_tail(&dma->link, &sess->bdt_dma_resc);

		for (count = 0; count < num_elem_per_page; count++) {
			bdt_info->bd_tbl = (struct iscsi_bd *)(dma->mem +
						(count * bd_tbl_size));
			bdt_info->bd_tbl_dma = dma->mapping + count * bd_tbl_size;
			bdt_info->max_bd_cnt = ISCSI_MAX_BDS_PER_CMD;
			bdt_info->bd_valid = 0;
			bdt_info->cmdp = NULL;
			bdt_info = (struct io_bdt *)bdt_info->link.next;
			if (bdt_info == (struct io_bdt *)&sess->bd_tbl_list)
				break;
		}
		dma++;
	}
	return ret_val;

resc_alloc_failed:
dma_alloc_failed:
	return ret_val;
}


/**
 * bnx2i_free_bd_table_pool - releases resources held by BD table pool
 *
 * @sess:		iscsi session pointer
 *
 * releases BD table pool memory
 */
void bnx2i_free_bd_table_pool(struct bnx2i_sess *sess)
{
	struct bnx2i_dma *dma;

	list_for_each_entry(dma, &sess->bdt_dma_resc, link)
		bnx2i_free_dma(sess->hba, dma);

	if(sess->bdt_dma_info)
		kfree(sess->bdt_dma_info);
}


/**
 * bnx2i_setup_mp_bdt - allocated BD table resources to be used as
 *			the dummy buffer for '0' payload length iscsi requests
 *
 * @hba: 		pointer to adapter structure
 *
 * allocate memory for dummy buffer and associated BD table to be used by
 *	middle path (MP) requests
 */
int bnx2i_setup_mp_bdt(struct bnx2i_hba *hba)
{
	int rc = 0;

	if (bnx2i_alloc_dma(hba, &hba->mp_dma_buf, PAGE_SIZE, BNX2I_TBL_TYPE_BD, 0)) {
		PRINT_ERR(hba, "unable to allocate Middle Path BDT\n");
		rc = -1;
	}
	return rc;
}


/**
 * bnx2i_free_mp_bdt - releases ITT back to free pool
 *
 * @hba: 		pointer to adapter instance
 *
 * free MP dummy buffer and associated BD table
 */
void bnx2i_free_mp_bdt(struct bnx2i_hba *hba)
{
	bnx2i_free_dma(hba, &hba->mp_dma_buf);
}


/**
 * bnx2i_start_iscsi_hba_shutdown - start hba shutdown by cleaning up
 *			all active sessions
 *
 * @hba: 		pointer to adapter instance
 *
 *  interface is being brought down by the user, fail all active iSCSI sessions
 *	belonging to this adapter
 */
void bnx2i_start_iscsi_hba_shutdown(struct bnx2i_hba *hba)
{
	struct bnx2i_sess *sess;
	int lpcnt;
	int rc;

	spin_lock_bh(&hba->lock);
	list_for_each_entry(sess, &hba->active_sess, link) {
		spin_unlock_bh(&hba->lock);
		lpcnt = 4;
		rc = bnx2i_do_iscsi_sess_recovery(sess, DID_NO_CONNECT, 1);
		while ((rc != SUCCESS) && lpcnt--) {
			msleep(1000);
			rc = bnx2i_do_iscsi_sess_recovery(sess, DID_NO_CONNECT, 1);
		}
		spin_lock_bh(&hba->lock);
	}
	spin_unlock_bh(&hba->lock);
}

/**
 * bnx2i_iscsi_hba_cleanup - System is shutting down, cleanup all offloaded connections
 *
 * @hba: 		pointer to adapter instance
 *
 */
void bnx2i_iscsi_hba_cleanup(struct bnx2i_hba *hba)
{
	struct bnx2i_sess *sess;
	struct bnx2i_endpoint *ep;

	spin_lock_bh(&hba->lock);
	list_for_each_entry(sess, &hba->active_sess, link) {
		spin_unlock_bh(&hba->lock);
		if (sess)
			if (sess->lead_conn && sess->lead_conn->ep) {
				ep = sess->lead_conn->ep;
				bnx2i_conn_stop(sess->lead_conn->cls_conn,
						STOP_CONN_RECOVER);
				bnx2i_ep_disconnect((uint64_t)ep);
			}
		spin_lock_bh(&hba->lock);
	}
	spin_unlock_bh(&hba->lock);
}

/**
 * bnx2i_iscsi_handle_ip_event - inetdev callback to handle ip address change
 *
 * @hba: 		pointer to adapter instance
 *
 * IP address change indication, fail all iSCSI connections on this adapter
 *	and let 'iscsid' reinstate the connections
 */
void bnx2i_iscsi_handle_ip_event(struct bnx2i_hba *hba)
{
	struct bnx2i_sess *sess;

	spin_lock_bh(&hba->lock);
	list_for_each_entry(sess, &hba->active_sess, link) {
		spin_unlock_bh(&hba->lock);
		bnx2i_do_iscsi_sess_recovery(sess, DID_RESET, 1);
		spin_lock_bh(&hba->lock);
	}
	spin_unlock_bh(&hba->lock);
}


static void bnx2i_withdraw_sess_recovery(struct bnx2i_sess *sess)
{
	struct bnx2i_hba *hba = sess->hba;
	int cons_idx = hba->sess_recov_cons_idx;

	spin_lock_bh(&hba->lock);
	while (hba->sess_recov_prod_idx != cons_idx) {
		if (sess == hba->sess_recov_list[cons_idx]) {
			hba->sess_recov_list[cons_idx] = NULL;
			break;
		}
		if (cons_idx == hba->sess_recov_max_idx)
			cons_idx = 0;
		else
			cons_idx++;
	}
	spin_unlock_bh(&hba->lock);
}

/**
 * conn_err_recovery_task - does recovery on all queued sessions
 *
 * @work:		pointer to work struct
 *
 * iSCSI Session recovery queue manager
 */
static void
#if defined(INIT_DELAYED_WORK_DEFERRABLE) || defined(INIT_WORK_NAR) || (defined(__VMKLNX__) && (VMWARE_ESX_DDK_VERSION >= 50000))
conn_err_recovery_task(struct work_struct *work)
#else
conn_err_recovery_task(void *data)
#endif
{
#if defined(INIT_DELAYED_WORK_DEFERRABLE) || defined(INIT_WORK_NAR) || (defined(__VMKLNX__) && (VMWARE_ESX_DDK_VERSION >= 50000))
	struct bnx2i_hba *hba = container_of(work, struct bnx2i_hba,
					     err_rec_task);
#else
	struct bnx2i_hba *hba = data;
#endif
	struct bnx2i_sess *sess;
	int cons_idx = hba->sess_recov_cons_idx;

	spin_lock_bh(&hba->lock);
	while (hba->sess_recov_prod_idx != cons_idx) {
		sess = hba->sess_recov_list[cons_idx];
		if (cons_idx == hba->sess_recov_max_idx)
			cons_idx = 0;
		else
			cons_idx++;
		spin_unlock_bh(&hba->lock);
		if (sess) {
			if (sess->state == BNX2I_SESS_IN_LOGOUT)
				bnx2i_do_iscsi_sess_recovery(sess, DID_NO_CONNECT, 1);
			else
				bnx2i_do_iscsi_sess_recovery(sess, DID_RESET, 1);
		}
		spin_lock_bh(&hba->lock);
	}
	hba->sess_recov_cons_idx = cons_idx;
	spin_unlock_bh(&hba->lock);
}

/**
 * bnx2i_ep_destroy_list_add - add an entry to EP destroy list
 *
 * @hba: 		pointer to adapter instance
 * @ep: 		pointer to endpoint (transport indentifier) structure
 *
 * EP destroy queue manager
 */
static int bnx2i_ep_destroy_list_add(struct bnx2i_hba *hba,
				  struct bnx2i_endpoint *ep)
{
#ifdef __VMKLNX__
	spin_lock(&hba->lock);
#else
	write_lock(&hba->ep_rdwr_lock);
#endif
	list_add_tail(&ep->link, &hba->ep_destroy_list);
#ifdef __VMKLNX__
	spin_unlock(&hba->lock);
#else
	write_unlock(&hba->ep_rdwr_lock);
#endif
	return 0;
}

/**
 * bnx2i_ep_destroy_list_del - add an entry to EP destroy list
 *
 * @hba: 		pointer to adapter instance
 * @ep: 		pointer to endpoint (transport indentifier) structure
 *
 * EP destroy queue manager
 */
static int bnx2i_ep_destroy_list_del(struct bnx2i_hba *hba,
				     struct bnx2i_endpoint *ep)
{
#ifdef __VMKLNX__
	spin_lock(&hba->lock);
#else
	write_lock(&hba->ep_rdwr_lock);
#endif
	list_del_init(&ep->link);
#ifdef __VMKLNX__
	spin_unlock(&hba->lock);
#else
	write_unlock(&hba->ep_rdwr_lock);
#endif

	return 0;
}

/**
 * bnx2i_ep_ofld_list_add - add an entry to ep offload pending list
 *
 * @hba: 		pointer to adapter instance
 * @ep: 		pointer to endpoint (transport indentifier) structure
 *
 * pending conn offload completion queue manager
 */
static int bnx2i_ep_ofld_list_add(struct bnx2i_hba *hba,
				  struct bnx2i_endpoint *ep)
{
#ifdef __VMKLNX__
	spin_lock(&hba->lock);
#else
	write_lock(&hba->ep_rdwr_lock);
#endif
	list_add_tail(&ep->link, &hba->ep_ofld_list);
#ifdef __VMKLNX__
	spin_unlock(&hba->lock);
#else
	write_unlock(&hba->ep_rdwr_lock);
#endif
	return 0;
}

/**
 * bnx2i_ep_ofld_list_del - add an entry to ep offload pending list
 *
 * @hba: 		pointer to adapter instance
 * @ep: 		pointer to endpoint (transport indentifier) structure
 *
 * pending conn offload completion queue manager
 */
static int bnx2i_ep_ofld_list_del(struct bnx2i_hba *hba,
				  struct bnx2i_endpoint *ep)
{
#ifdef __VMKLNX__
	spin_lock(&hba->lock);
#else
	write_lock(&hba->ep_rdwr_lock);
#endif
	list_del_init(&ep->link);
#ifdef __VMKLNX__
	spin_unlock(&hba->lock);
#else
	write_unlock(&hba->ep_rdwr_lock);
#endif

	return 0;
}


/**
 * bnx2i_find_ep_in_ofld_list - find iscsi_cid in pending list of endpoints
 *
 * @hba: 		pointer to adapter instance
 * @iscsi_cid:		iscsi context ID to find
 *
 */
struct bnx2i_endpoint *
bnx2i_find_ep_in_ofld_list(struct bnx2i_hba *hba, u32 iscsi_cid)
{
	struct bnx2i_endpoint *ep = NULL;
	struct bnx2i_endpoint *tmp_ep;

#ifdef __VMKLNX__
	spin_lock(&hba->lock);
#else
	read_lock(&hba->ep_rdwr_lock);
#endif
	list_for_each_entry(tmp_ep, &hba->ep_ofld_list, link) {
		if (tmp_ep->ep_iscsi_cid == iscsi_cid) {
			ep = tmp_ep;
			break;
		}
	}
#ifdef __VMKLNX__
	spin_unlock(&hba->lock);
#else
	read_unlock(&hba->ep_rdwr_lock);
#endif

	if (!ep)
		PRINT(hba, "ofld_list - icid %d not found\n", iscsi_cid);

	return ep;
}


/**
 * bnx2i_find_ep_in_destroy_list - find iscsi_cid in destroy list
 *
 * @hba: 		pointer to adapter instance
 * @iscsi_cid:		iscsi context ID to find
 *
 */
struct bnx2i_endpoint *
bnx2i_find_ep_in_destroy_list(struct bnx2i_hba *hba, u32 iscsi_cid)
{
	struct bnx2i_endpoint *ep = NULL;
	struct bnx2i_endpoint *tmp_ep;

#ifdef __VMKLNX__
	spin_lock(&hba->lock);
#else
	read_lock(&hba->ep_rdwr_lock);
#endif
	list_for_each_entry(tmp_ep, &hba->ep_destroy_list, link) {
		if (tmp_ep->ep_iscsi_cid == iscsi_cid) {
			ep = tmp_ep;
			break;
		}
	}
#ifdef __VMKLNX__
	spin_unlock(&hba->lock);
#else
	read_unlock(&hba->ep_rdwr_lock);
#endif

	if (!ep)
		PRINT(hba, "destroy_list - icid %d not found\n", iscsi_cid);

	return ep;
}


#ifdef __VMKLNX__
struct Scsi_Host *bnx2i_alloc_shost(int priv_sz)
{
	struct Scsi_Host *shost;

	shost = scsi_host_alloc(&bnx2i_host_template, priv_sz);
	if (!shost)
		return NULL;

	/* Vmware suggested values */
	shost->max_id = 256;
	shost->max_channel = 64;
	shost->max_lun = bnx2i_iscsi_transport.max_lun;
	shost->max_cmd_len = 16;

	return shost;
}

static int bnx2i_init_pool_mem(struct bnx2i_hba *hba)
{
	VMK_ReturnStatus vmk_ret;

#if (VMWARE_ESX_DDK_VERSION == 41000)
	char pool_name[32];
	vmk_MemPoolProps pool_props;

	snprintf(pool_name, 32, "bnx2i_%s", hba->netdev->name);

	pool_props.reservation = BNX2I_INITIAL_POOLSIZE;
	if (test_bit(BNX2I_NX2_DEV_57710, &hba->cnic_dev_type))
		pool_props.limit = BNX2I_MAX_POOLSIZE_5771X;
	else
		pool_props.limit = BNX2I_MAX_POOLSIZE_570X;

	vmk_ret = vmk_MemPoolCreate(pool_name, &pool_props,
				    &hba->bnx2i_pool);

#else /* !(VMWARE_ESX_DDK_VERSION == 41000) */
	vmk_MemPoolProps pool_props;

	pool_props.module = THIS_MODULE->moduleID;
	pool_props.parentMemPool = VMK_MEMPOOL_INVALID;
	pool_props.memPoolType = VMK_MEM_POOL_LEAF;
	pool_props.resourceProps.reservation = BNX2I_INITIAL_POOLSIZE;
	vmk_ret = vmk_NameFormat(&pool_props.name, "bnx2i_%s", hba->netdev->name);
	VMK_ASSERT(vmk_ret == VMK_OK);

	if (test_bit(BNX2I_NX2_DEV_57710, &hba->cnic_dev_type))
		pool_props.resourceProps.limit = BNX2I_MAX_POOLSIZE_5771X;
	else
		pool_props.resourceProps.limit = BNX2I_MAX_POOLSIZE_570X;

	vmk_ret = vmk_MemPoolCreate(&pool_props, &hba->bnx2i_pool);
#endif /* (VMWARE_ESX_DDK_VERSION == 41000) */

	if (vmk_ret != VMK_OK) {
		printk("%s: pool allocation failed(%x)", __FUNCTION__, vmk_ret);
		return -1;
	}

	return 0;
}
#endif


/**
 * bnx2i_alloc_hba - allocate and init adapter instance
 *
 * @cnic:		cnic device pointer
 *
 * allocate & initialize adapter structure and call other
 *	support routines to do per adapter initialization
 */
struct bnx2i_hba *bnx2i_alloc_hba(struct cnic_dev *cnic)
{
	struct bnx2i_hba *hba;
	struct scsi_host_template *scsi_template;
	struct iscsi_transport *iscsi_transport;
#ifdef __VMKLNX__
	u32 max_sectors;
	struct Scsi_Host *shost;

	if (bnx2i_max_sectors == -1)
		if (test_bit(CNIC_F_BNX2X_CLASS, &cnic->flags))
			max_sectors = 256;
		else
			max_sectors = 127;
	else {
		if (bnx2i_max_sectors < 64)
			max_sectors = 64;
		else if (bnx2i_max_sectors > 256)
			max_sectors = 256;
		else
			max_sectors = bnx2i_max_sectors;
		/* limit 1g max sector to no more than 127 */
		if ((test_bit(CNIC_F_BNX2_CLASS, &cnic->flags)) &&
		    max_sectors > 127)
			max_sectors = 127;
	}
	bnx2i_host_template.max_sectors = max_sectors;

	shost = bnx2i_alloc_shost(hostdata_privsize(sizeof(*hba)));
	if (!shost)
		return NULL;
	if (!cnic->pcidev)
		return NULL;
	shost->dma_boundary = cnic->pcidev->dma_mask;
	hba = shost_priv(shost);
	hba->shost = shost;
#else
	hba = kmalloc(sizeof(struct bnx2i_hba), GFP_KERNEL);
	if (!hba)
		return NULL;

	memset(hba, 0, sizeof(struct bnx2i_hba));
#endif

	/* Get PCI related information and update hba struct members */
	hba->cnic = cnic;
	hba->netdev = cnic->netdev;

	INIT_LIST_HEAD(&hba->active_sess);
	INIT_LIST_HEAD(&hba->ep_ofld_list);
	INIT_LIST_HEAD(&hba->ep_destroy_list);
	INIT_LIST_HEAD(&hba->ep_stale_list);
 	INIT_LIST_HEAD(&hba->ep_tmo_list);
	INIT_LIST_HEAD(&hba->link);
#ifdef __VMKLNX__
#if (VMWARE_ESX_DDK_VERSION >= 50000)
	if (bnx2i_esx_mtu_max && bnx2i_esx_mtu_max <= BNX2I_MAX_MTU_SUPPORTED)
		hba->mtu_supported = bnx2i_esx_mtu_max;
	else
		hba->mtu_supported = BNX2I_MAX_MTU_SUPPORTED;
#else
	hba->mtu_supported = BNX2I_MAX_MTU_SUPPORTED;
#endif
#else
	rwlock_init(&hba->ep_rdwr_lock);
	hba->mtu_supported = BNX2I_MAX_MTU_SUPPORTED;
#endif


	hba->max_active_conns = ISCSI_MAX_CONNS_PER_HBA;

	/* Get device type required to determine default SQ size */
	if (cnic->pcidev) {
		hba->reg_base = pci_resource_start(cnic->pcidev, 0);
		hba->pci_did = cnic->pcidev->device;
		bnx2i_identify_device(hba, cnic);
	}

#ifdef __VMWARE__
	if (bnx2i_init_pool_mem(hba))
		goto mem_pool_error;
#endif /* __VMWARE__ */

	/* SQ/RQ/CQ size can be changed via sysfs interface */
	sq_size = roundup_pow_of_two(sq_size);
	if (test_bit(BNX2I_NX2_DEV_57710, &hba->cnic_dev_type)) {
        	hba->setup_pgtbl = bnx2i_setup_5771x_pgtbl;
		if (sq_size <= BNX2I_5770X_SQ_WQES_MAX && 
			sq_size >= BNX2I_SQ_WQES_MIN) 
			hba->max_sqes = sq_size;
		else {
			hba->max_sqes = BNX2I_5770X_SQ_WQES_DEFAULT;
			PRINT_INFO(hba,
				"bnx2i: sq_size %d out of range, using %d\n",
				sq_size, hba->max_sqes);
		}
	} else {	/* 5706/5708/5709 */
        	hba->setup_pgtbl = bnx2i_setup_570x_pgtbl;
		if (sq_size <= BNX2I_570X_SQ_WQES_MAX && 
			sq_size >= BNX2I_SQ_WQES_MIN)
			hba->max_sqes = sq_size;
		else {
#ifdef __VMKLNX__
			if (test_bit(BNX2I_NX2_DEV_5709, &hba->cnic_dev_type))
				hba->max_sqes = BNX2I_5709_SQ_WQES_DEFAULT;
			else
				hba->max_sqes = BNX2I_570X_SQ_WQES_DEFAULT;
#else
			hba->max_sqes = BNX2I_570X_SQ_WQES_DEFAULT;
#endif
			PRINT_INFO(hba,
				"bnx2i: sq_size %d out of range, using %d\n",
				sq_size, hba->max_sqes);
		}
	}
        
	rq_size = roundup_pow_of_two(rq_size);
        if (rq_size < BNX2I_RQ_WQES_MIN || rq_size > BNX2I_RQ_WQES_MAX) {
		
		PRINT_INFO(hba, "bnx2i: rq_size %d out of range, using %d\n",
			rq_size, BNX2I_RQ_WQES_DEFAULT);
		rq_size = BNX2I_RQ_WQES_DEFAULT;
	}
	hba->max_rqes = rq_size;
	hba->max_cqes = hba->max_sqes + rq_size;
	hba->num_ccell = hba->max_sqes / 2;
	BNX2I_DBG(DBG_CONN_SETUP, hba, "QP CONF: SQ=%d, CQ=%d, RQ=%d\n",
		  hba->max_sqes, hba->max_cqes, hba->max_rqes);

	scsi_template = bnx2i_alloc_scsi_host_template(hba, cnic);
	if (!scsi_template)
		goto scsi_template_err;

	iscsi_transport = bnx2i_alloc_iscsi_transport(hba, cnic, scsi_template);
	if (!iscsi_transport)
		goto iscsi_transport_err;

	if (bnx2i_setup_free_cid_que(hba))
		goto cid_que_err;

	hba->scsi_template = scsi_template;
	hba->iscsi_transport = iscsi_transport;

	spin_lock_init(&hba->lock);
	mutex_init(&hba->net_dev_lock);

	/* initialize timer and wait queue used for resource cleanup when
	 * interface is brought down */
	init_timer(&hba->hba_timer);
	init_waitqueue_head(&hba->eh_wait);
	init_waitqueue_head(&hba->ep_tmo_wait);

#ifdef __VMKLNX__
	if (en_hba_poll) {
		init_timer(&hba->hba_poll_timer);
		hba->hba_poll_timer.expires = jiffies + 12 * HZ;
		hba->hba_poll_timer.function = bnx2i_hba_poll_timer;
		hba->hba_poll_timer.data = (unsigned long) hba;
		add_timer(&hba->hba_poll_timer);
	}
#endif


#if defined(INIT_DELAYED_WORK_DEFERRABLE) || defined(INIT_WORK_NAR) || (defined(__VMKLNX__) && (VMWARE_ESX_DDK_VERSION >= 50000))
	INIT_WORK(&hba->err_rec_task, conn_err_recovery_task);
	INIT_WORK(&hba->ep_poll_task, ep_tmo_poll_task);
#else
	INIT_WORK(&hba->err_rec_task, conn_err_recovery_task, hba);
	INIT_WORK(&hba->ep_poll_task, ep_tmo_poll_task, hba);
#endif
	hba->sess_recov_prod_idx = 0;
	hba->sess_recov_cons_idx = 0;
	hba->sess_recov_max_idx = 0;
	hba->sess_recov_list = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!hba->sess_recov_list)
		goto rec_que_err;
	hba->sess_recov_max_idx = PAGE_SIZE / sizeof (struct bnx2i_sess *) - 1;

	spin_lock_init(&hba->stat_lock);
	memset(&hba->stats, 0, sizeof(struct iscsi_stats_info));
	memset(&hba->login_stats, 0, sizeof(struct iscsi_login_stats_info));

	bnx2i_add_hba_to_adapter_list(hba);

#ifdef __VMKLNX__
	if (bnx2i_bind_adapter_devices(hba))
		goto pcidev_bind_err;
#endif

	return hba;

#ifdef __VMKLNX__
pcidev_bind_err:
#endif
rec_que_err:
	bnx2i_release_free_cid_que(hba);
cid_que_err:
	bnx2i_free_iscsi_transport(iscsi_transport);
iscsi_transport_err:
	bnx2i_free_scsi_host_template(scsi_template);
scsi_template_err:
#ifndef __VMKLNX__
	scsi_host_put(shost);
#else
mem_pool_error:
#endif
	bnx2i_free_hba(hba);

	return NULL;
}


/**
 * bnx2i_free_hba- releases hba structure and resources held by the adapter
 *
 * @hba: 		pointer to adapter instance
 *
 * free adapter structure and call various cleanup routines.
 */
void bnx2i_free_hba(struct bnx2i_hba *hba)
{
	bnx2i_release_free_cid_que(hba);
	INIT_LIST_HEAD(&hba->ep_ofld_list);
	INIT_LIST_HEAD(&hba->ep_destroy_list);

#ifdef __VMKLNX__
	if (en_hba_poll)
		del_timer_sync(&hba->hba_poll_timer);
#endif

	kfree(hba->sess_recov_list);
	INIT_LIST_HEAD(&hba->active_sess);

	bnx2i_free_iscsi_scsi_template(hba);

#ifdef __VMKLNX__
	bnx2i_unbind_adapter_devices(hba);
#endif

	bnx2i_remove_hba_from_adapter_list(hba);
#ifdef __VMKLNX__
	vmk_MemPoolDestroy(hba->bnx2i_pool);
	scsi_host_put(hba->shost);
#endif


#ifndef __VMKLNX__
	kfree(hba);
#endif
}


static int bnx2i_flush_pend_queue(struct bnx2i_sess *sess,
				   struct scsi_cmnd *sc, int reason)
{
	int num_pend_cmds_returned = 0;
	struct list_head *list;
	struct list_head *tmp;
	struct bnx2i_scsi_task *scsi_task;

	spin_lock_bh(&sess->lock);
	list_for_each_safe(list, tmp, &sess->pend_cmd_list) {
		scsi_task = (struct bnx2i_scsi_task *) list;

		/* cmd queue flush request could be due to LUN RESET or
		 * the session recovery. In former case just fail only the
		 * command belonging that particular LUN.
		 */
		if (sc) {
			if (sc == scsi_task->scsi_cmd) {
				PRINT_ALERT(sess->hba,
					"flush_pend_queue: sc == scsi_task->scsi_cmd\n");
			} else if (scsi_task->scsi_cmd->device->lun
				   != sc->device->lun)
				continue;
		}

		num_pend_cmds_returned++;
		list_del_init(&scsi_task->link);
		bnx2i_return_failed_command(sess, scsi_task->scsi_cmd,
			scsi_bufflen(scsi_task->scsi_cmd),
			reason);
		scsi_task->scsi_cmd = NULL;
		list_add_tail(&scsi_task->link, &sess->scsi_task_list);
	}

	BNX2I_DBG(DBG_ITT_CLEANUP, sess->hba, "%s: sess %p, cleaned %d out "
		  "of %d commands from the pend queue\n", __FUNCTION__,
		  sess, num_pend_cmds_returned, sess->pend_cmd_count);

	sess->pend_cmd_count -= num_pend_cmds_returned;
	spin_unlock_bh(&sess->lock);
	return num_pend_cmds_returned;
}

/**
 * bnx2i_flush_cmd_queue - flush active command queue
 *
 * @sess:		iscsi session pointer
 * @reason: 		SCSI ML error code, DID_BUS_BUSY
 *
 * return all commands in active queue which should already have been
 * 	cleaned up by the cnic device.
 */
static int bnx2i_flush_cmd_queue(struct bnx2i_sess *sess,
				  struct scsi_cmnd *scsi_cmd,
				  int reason, int clear_ctx)
{
	struct list_head failed_cmds;
	struct list_head *list;
	struct list_head *tmp;
	struct bnx2i_cmd *cmd;
	int cmd_cnt = 0;
	int cmd_diff_lun = 0;
	int total_sess_active_cmds = 0;
	int iscsi_cid = 0xFFFF;

	if (sess->lead_conn && sess->lead_conn->ep)
		iscsi_cid = sess->lead_conn->ep->ep_iscsi_cid;

	INIT_LIST_HEAD(&failed_cmds);
	spin_lock_bh(&sess->lock);
	list_for_each_safe(list, tmp, &sess->active_cmd_list) {
		cmd = (struct bnx2i_cmd *) list;
		total_sess_active_cmds++;

		if (!cmd->scsi_cmd) {
			BNX2I_DBG(DBG_ITT_CLEANUP, sess->hba, "cid %d, flush q,"
					  " cmd %p is not associated with any"
					  " scsi cmd\n", iscsi_cid, cmd);
			continue;
		}
		/* cmd queue flush request could be due to LUN RESET or
		 * the session recovery. In former case just fail only the
		 * command belonging that particular LUN.
		 */
		if (scsi_cmd) {
			if (cmd->scsi_cmd->device->lun !=
				   scsi_cmd->device->lun) {
				cmd_diff_lun++;
				continue;
			}
		}
		if (atomic_read(&cmd->cmd_state) == ISCSI_CMD_STATE_CMPL_RCVD){
			/* completion pdu is being processed and we will let
			 * it run to completion, fail the request here
			 */
			BNX2I_DBG(DBG_ITT_CLEANUP, sess->hba,
				  "iscsi cid %d, completion & TMF cleanup "
				  "are running in parallel, cmd %p\n",
				  iscsi_cid, cmd);
			continue;
		}
	    	cmd->scsi_cmd->result = (reason << 16);
		/* Now that bnx2i_cleanup_task_context() does not sleep waiting
		 * for completion it is safe to hold sess lock and this will
		 * avoid race between LUN/TARGET RESET TMF completion followed
		 * by command completion with check condition
		 */

		if (clear_ctx) {
			atomic_set(&cmd->cmd_state, ISCSI_CMD_STATE_CLEANUP_START);
			bnx2i_cleanup_task_context(sess, cmd, reason);
		} else {
			list_del_init(&cmd->link);
			list_add_tail(&cmd->link, &failed_cmds);
		}
		cmd_cnt++;
	}
	spin_unlock_bh(&sess->lock);

	list_for_each_safe(list, tmp, &failed_cmds) {
		cmd = (struct bnx2i_cmd *) list;
		cmd->failed_reason = reason;
		bnx2i_fail_cmd(sess, cmd);
	}

	if (scsi_cmd)
		BNX2I_DBG(DBG_ITT_CLEANUP, sess->hba, "sess %p sc %p, lun %x, "
			  "total active %d, total cleaned %d, skipped %d \n",
			  sess, scsi_cmd, scsi_cmd->device->lun,
			  total_sess_active_cmds, cmd_cnt, cmd_diff_lun);
	else
		BNX2I_DBG(DBG_ITT_CLEANUP, sess->hba, "bnx2i: sess %p sc %p,"
			" total active %d, total cleaned %d \n",
			sess, scsi_cmd, total_sess_active_cmds, cmd_cnt);

	BNX2I_DBG(DBG_ITT_CLEANUP, sess->hba, "sess %p active %d, pend %d \n",
			sess, sess->pend_cmd_count, sess->active_cmd_count);

	return cmd_cnt;
}


/**
 * bnx2i_session_recovery_start - start recovery process on given session
 *
 * @sess:		iscsi session pointer
 * @reason: 		SCSI ML error code, DID_BUS_BUSY
 *
 * initiate cleanup of outstanding commands for sess recovery
 */
static int bnx2i_session_recovery_start(struct bnx2i_sess *sess, int reason)
{
	if (sess->state == BNX2I_SESS_IN_LOGOUT ||
	    sess->state == BNX2I_SESS_INITIAL)
		return 0;

	if (!is_sess_active(sess) &&
	    !(sess->state & BNX2I_SESS_INITIAL)) {

		if (sess->recovery_state)
			return -EPERM;

		wait_event_interruptible_timeout(sess->er_wait,
						 (sess->state ==
						  BNX2I_SESS_IN_FFP), 20 * HZ);
		if (signal_pending(current))
			flush_signals(current);
		if (!is_sess_active(sess) &&
		    !(sess->state & BNX2I_SESS_INITIAL)) {
			PRINT_ALERT(sess->hba,
				"sess_reco: sess still not active\n");
			sess->lead_conn->state = CONN_STATE_XPORT_FREEZE;
			return -EPERM;
		}
	}

	return 0;
}


/**
 * bnx2i_do_iscsi_sess_recovery - implements session recovery code
 *
 * @sess:		iscsi session pointer
 * @reason: 		SCSI ML error code, DID_BUS_BUSY, DID_NO_CONNECT,
 *			DID_RESET
 *
 * SCSI host reset handler, which is translates to iSCSI session
 *	recovery. This routine starts internal driver session recovery,
 *	indicates connection error to 'iscsid' which does session reinstatement
 *	This is an synchronous call which waits for completion and returns
 *	the ultimate result of session recovery process to caller
 */
int bnx2i_do_iscsi_sess_recovery(struct bnx2i_sess *sess, int reason, int signal)
{
	struct bnx2i_hba *hba;
	struct bnx2i_conn *conn = sess->lead_conn;
	int cmds_1 = 0;

	if (!conn)
		return FAILED;

	BNX2I_DBG(DBG_SESS_RECO, sess->hba, "%s: sess %p conn %p\n",
		  __FUNCTION__, sess, conn);

	spin_lock_bh(&sess->lock);
	if (sess->recovery_state) {
		BNX2I_DBG(DBG_SESS_RECO, sess->hba,
			  "RECOVERY ALREADY ACTIVE sess %p\n", sess);
		spin_unlock_bh(&sess->lock);
		wait_event_interruptible_timeout(sess->er_wait,
			!atomic_read(&sess->do_recovery_inprogess), 128 * HZ);
		return SUCCESS;
	}
	sess->lead_conn->state = CONN_STATE_XPORT_FREEZE;
	sess->recovery_state = ISCSI_SESS_RECOVERY_START;
	spin_unlock_bh(&sess->lock);
	atomic_set(&sess->do_recovery_inprogess, 1);
#ifdef __VMKLNX__
	if (sess->state != BNX2I_SESS_IN_LOGOUT) {
		/* logout blocks session to prevent accepting more cmds */
		iscsi_block_session(sess->cls_sess);
	}
#else
	iscsi_block_session(sess->cls_sess);
#endif

	hba = sess->hba;

#ifdef __VMKLNX__
	/**
	 *  DO THE CLEANUP HERE
	 */
	if (atomic_read(&sess->tmf_active)) {
		BNX2I_DBG(DBG_TMF, hba, "%s: WAKE UP TMF WAITER sess %p\n",
			  __FUNCTION__, sess);
		atomic_set(&sess->scsi_tmf_cmd->cmd_state, ISCSI_CMD_STATE_TMF_TIMEOUT);
		wake_up(&sess->er_wait);
	} else {
		BNX2I_DBG(DBG_TMF, hba, "%s: TMF NOT ACTIVE sess %p\n",
			  __FUNCTION__, sess);
	}
	/** 
	 *  Aborts have been woken, and should be cleared out 
	 *  We can wait for the tmf_active flag to drop if need be here
	 *  but it should not matter
	 */
	sess->cmd_cleanup_req = 0;
	sess->cmd_cleanup_cmpl = 0;

	BNX2I_DBG(DBG_SESS_RECO, hba, "%s: FLUSH PENDING/ACTIVE sess %p\n",
		  __FUNCTION__, sess);
	cmds_1 = bnx2i_flush_pend_queue(sess, NULL, DID_RESET);
	/* We can't cleanup here because link may be down and CMD_CLEANUP may not
	 * be processed by the firmware because of lack on CCELL. CMD_CLEANUP
	 * works well when TCP layer is function well and the fault lies in
	 * in iSCSI or SCSI backend. Because at this stage we can't guarantee
	 * any of the active commands are setup in F/W and TCP ACK'ed by target,
	 * we can't flush active queue here.
	 bnx2i_flush_cmd_queue(sess, NULL, DID_RESET, 1);
	 */
	BNX2I_DBG(DBG_SESS_RECO, hba, "bnx2i: FLUSH DONE sess %p\n", sess);
#else
	iscsi_block_session(session_to_cls(sess));
#endif
	/** SIGNAL CONNECTION FAULT */
	if (signal) {
		BNX2I_DBG(DBG_SESS_RECO, hba, "%s: Notify daemon to start"
			  " reconnecting\n", __FUNCTION__);
		iscsi_conn_error(conn->cls_conn, ISCSI_ERR_CONN_FAILED);
	}

	if (signal_pending(current))
		flush_signals(current);

	BNX2I_DBG(DBG_SESS_RECO, hba,
		  "sess %p cmds stats: PD=%d, Q=%d, S=%d, D=%d, F=%d, CC=%d\n",
		  sess, cmds_1, sess->total_cmds_queued, sess->total_cmds_sent,
		  sess->total_cmds_completed, sess->total_cmds_failed,
		  sess->total_cmds_completed_by_chip);

	atomic_set(&sess->do_recovery_inprogess, 0);
	barrier();
	wake_up(&sess->er_wait);
	return SUCCESS;
}


/**
 * bnx2i_iscsi_sess_release - cleanup iscsi session & reclaim all resources
 *
 * @hba: 		pointer to adapter instance
 * @sess:		iscsi session pointer
 *
 * free up resources held by this session including ITT queue, cmd struct pool,
 *	BD table pool. HBA lock is held while manipulating active session list
 */
void bnx2i_iscsi_sess_release(struct bnx2i_hba *hba, struct bnx2i_sess *sess)
{
	if (sess->login_nopout_cmd) {
		/* set cmd state so that free_cmd() accepts it */
		atomic_set(&sess->login_nopout_cmd->cmd_state,
			   ISCSI_CMD_STATE_COMPLETED);
		bnx2i_free_cmd(sess, sess->login_nopout_cmd);
	}
	if (sess->scsi_tmf_cmd) {
		atomic_set(&sess->scsi_tmf_cmd->cmd_state,
			   ISCSI_CMD_STATE_COMPLETED);
		bnx2i_free_cmd(sess, sess->scsi_tmf_cmd);
	}
	if (sess->nopout_resp_cmd) {
		atomic_set(&sess->nopout_resp_cmd->cmd_state,
			   ISCSI_CMD_STATE_COMPLETED);
		bnx2i_free_cmd(sess, sess->nopout_resp_cmd);
	}

	sess->login_nopout_cmd = NULL;
	sess->scsi_tmf_cmd = NULL;
	sess->nopout_resp_cmd = NULL;

	bnx2i_free_bd_table_pool(sess);
	bnx2i_free_all_bdt_resc_pages(sess);
	bnx2i_free_cmd_pool(sess);
	bnx2i_free_scsi_task_pool(sess);

	spin_lock_bh(&hba->lock);
	list_del_init(&sess->link);
	hba->num_active_sess--;
	spin_unlock_bh(&hba->lock);
}


/**
 * bnx2i_iscsi_sess_new - initialize newly allocated session structure
 *
 * @hba: 		pointer to adapter instance
 * @sess:		iscsi session pointer
 *
 * initialize session structure elements and allocate per sess resources.
 *	Some of the per session resources allocated are command struct pool,
 *	BD table pool and ITT queue region
 */
int bnx2i_iscsi_sess_new(struct bnx2i_hba *hba, struct bnx2i_sess *sess)
{
	spin_lock_bh(&hba->lock);
	list_add_tail(&sess->link, &hba->active_sess);
	hba->num_active_sess++;
	spin_unlock_bh(&hba->lock);

	sess->sq_size = hba->max_sqes;
	sess->tsih = 0;
	sess->lead_conn = NULL;
	sess->worker_time_slice = 2;

	spin_lock_init(&sess->lock);
	mutex_init(&sess->tmf_mutex);
	spin_lock_init(&sess->device_lock);

	/* initialize active connection list */
	INIT_LIST_HEAD(&sess->conn_list);
	INIT_LIST_HEAD(&sess->free_cmds);

	INIT_LIST_HEAD(&sess->pend_cmd_list);
	sess->pend_cmd_count = 0;
	INIT_LIST_HEAD(&sess->active_cmd_list);
	sess->active_cmd_count = 0;

	atomic_set(&sess->login_noop_pending, 0);
	atomic_set(&sess->logout_pending, 0);
	atomic_set(&sess->tmf_pending, 0);

	sess->login_nopout_cmd = NULL;
	sess->scsi_tmf_cmd = NULL;
	sess->nopout_resp_cmd = NULL;

	sess->num_active_conn = 0;
	sess->max_conns = 1;
	sess->target_name = NULL;

	sess->state = BNX2I_SESS_INITIAL;
	sess->recovery_state = 0;
	atomic_set(&sess->tmf_active, 0);
	sess->alloc_scsi_task_failed = 0;

	if (bnx2i_alloc_bd_table_pool(sess) != 0) {
		PRINT_ERR(hba, "sess_new: unable to alloc bd table pool\n");
		goto err_bd_pool;
	}

	if (bnx2i_alloc_cmd_pool(sess) != 0) {
		PRINT_ERR(hba, "sess_new: alloc cmd pool failed\n");
		goto err_cmd_pool;
	}

	if (bnx2i_alloc_scsi_task_pool(sess) != 0) {
		PRINT_ERR(hba, "sess_new: alloc scsi_task pool failed\n");
		goto err_sc_pool;
	}
	init_timer(&sess->abort_timer);
	init_waitqueue_head(&sess->er_wait);

	return 0;

err_sc_pool:
err_cmd_pool:
	bnx2i_free_cmd_pool(sess);
err_bd_pool:
	bnx2i_free_bd_table_pool(sess);
	bnx2i_free_all_bdt_resc_pages(sess);
	return -ENOMEM;
}

/**
 * bnx2i_conn_free_login_resources - free DMA resources used for login process
 *
 * @hba: 		pointer to adapter instance
 * @conn: 		iscsi connection pointer
 *
 * Login related resources, mostly BDT & payload DMA memory is freed
 */
void bnx2i_conn_free_login_resources(struct bnx2i_hba *hba,
				     struct bnx2i_conn *conn)
{
	bnx2i_free_dma(hba, &conn->gen_pdu.login_req);
	bnx2i_free_dma(hba, &conn->gen_pdu.login_resp);
}

/**
 * bnx2i_conn_alloc_login_resources - alloc DMA resources used for
 *			login / nopout pdus
 *
 * @hba: 		pointer to adapter instance
 * @conn: 		iscsi connection pointer
 *
 * Login & nop-in related resources is allocated in this routine.
 */
static int bnx2i_conn_alloc_login_resources(struct bnx2i_hba *hba,
					    struct bnx2i_conn *conn)
{
	/* Allocate memory for login request/response buffers */
	if (bnx2i_alloc_dma(hba, &conn->gen_pdu.login_req,
			    ISCSI_CONN_LOGIN_BUF_SIZE, BNX2I_TBL_TYPE_BD, 0))
		goto error;

	conn->gen_pdu.req_buf_size = 0;
	conn->gen_pdu.req_wr_ptr = conn->gen_pdu.login_req.mem;

	if (bnx2i_alloc_dma(hba, &conn->gen_pdu.login_resp,
			    ISCSI_CONN_LOGIN_BUF_SIZE, BNX2I_TBL_TYPE_BD, 0))
		goto error;

	conn->gen_pdu.resp_buf_size = ISCSI_CONN_LOGIN_BUF_SIZE;
	conn->gen_pdu.resp_wr_ptr = conn->gen_pdu.login_resp.mem;
	
	return 0;

error:
	PRINT_ERR(hba, "conn login resource alloc failed!!\n");
	bnx2i_conn_free_login_resources(hba, conn);
	return -ENOMEM;

}


/**
 * bnx2i_iscsi_conn_new - initialize newly created connection structure
 *
 * @sess:		iscsi session pointer
 * @conn: 		iscsi connection pointer
 *
 * connection structure is initialized which mainly includes allocation of
 *	login resources and lock/time initialization
 */
int bnx2i_iscsi_conn_new(struct bnx2i_sess *sess, struct bnx2i_conn *conn)
{
	struct bnx2i_hba *hba = sess->hba;

	conn->sess = sess;
	conn->header_digest_en = 0;
	conn->data_digest_en = 0;

	INIT_LIST_HEAD(&conn->link);

	/* 'ep' ptr will be assigned in bind() call */
	conn->ep = NULL;

	if (test_bit(BNX2I_NX2_DEV_57710, &hba->cnic_dev_type))
		conn->ring_doorbell = bnx2i_ring_sq_dbell_bnx2x;
	else
		conn->ring_doorbell = bnx2i_ring_sq_dbell_bnx2;

	if (bnx2i_conn_alloc_login_resources(hba, conn)) {
		PRINT_ALERT(hba, "conn_new: login resc alloc failed!!\n");
		return -ENOMEM;
	}


	atomic_set(&conn->stop_state, 0);
	atomic_set(&conn->worker_running, 0);
#ifdef __VMKLNX__
	tasklet_init(&conn->conn_tasklet, &bnx2i_conn_main_worker,
		     (unsigned long) conn);
	atomic_set(&conn->worker_enabled, 0);
#else
#if defined(INIT_DELAYED_WORK_DEFERRABLE) || defined(INIT_WORK_NAR) || (defined(__VMKLNX__) && (VMWARE_ESX_DDK_VERSION >= 50000))
	INIT_WORK(&conn->conn_worker, bnx2i_conn_main_worker);
#else
	INIT_WORK(&conn->conn_worker, bnx2i_conn_main_worker, conn); 
#endif
	atomic_set(&conn->worker_enabled, 1);
#endif 		/* __VMKLNX__ */

	init_timer(&conn->poll_timer);
	conn->poll_timer.expires = HZ + jiffies;	/* 200 msec */
	conn->poll_timer.function = bnx2i_conn_poll;
	conn->poll_timer.data = (unsigned long) conn;

	return 0;
}


/**
 * bnx2i_login_resp_update_cmdsn - extracts SN & MAX_SN from login response header &
 *			updates driver 'cmdsn' with 
 *
 * @conn: 		iscsi connection pointer
 *
 * extract & update SN counters from login response
 */
static int bnx2i_login_resp_update_cmdsn(struct bnx2i_conn *conn)
{
	u32 max_cmdsn;
	u32 exp_cmdsn;
	u32 stat_sn;
	struct bnx2i_sess *sess = conn->sess;
	struct iscsi_nopin *hdr;

	hdr = (struct iscsi_nopin *) &conn->gen_pdu.resp_hdr;

	max_cmdsn = ntohl(hdr->max_cmdsn);
	exp_cmdsn = ntohl(hdr->exp_cmdsn);
	stat_sn = ntohl(hdr->statsn);
#define SN_DELTA_ISLAND		0xffff
	if (max_cmdsn < exp_cmdsn -1 &&
	    max_cmdsn > exp_cmdsn - SN_DELTA_ISLAND)
		return -EINVAL;

	if (max_cmdsn > sess->max_cmdsn ||
	    max_cmdsn < sess->max_cmdsn - SN_DELTA_ISLAND)
		sess->max_cmdsn = max_cmdsn;

	if (exp_cmdsn > sess->exp_cmdsn ||
	    exp_cmdsn < sess->exp_cmdsn - SN_DELTA_ISLAND)
		sess->exp_cmdsn = exp_cmdsn;

	if (stat_sn == conn->exp_statsn)
		conn->exp_statsn++;

	return 0;
}


/**
 * bnx2i_update_cmd_sequence - update session sequencing parameter
 *
 * @sess:		iscsi session pointer
 * @exp_sn: 		iscsi expected command seq num
 * @max_sn: 		iscsi max command seq num
 *
 * update iSCSI SN counters for the given session
 */
void bnx2i_update_cmd_sequence(struct bnx2i_sess *sess,
			       u32 exp_sn, u32 max_sn)
{
	u32 exp_cmdsn = exp_sn;
	u32 max_cmdsn = max_sn;

	if (max_cmdsn < exp_cmdsn -1 &&
	    max_cmdsn > exp_cmdsn - SN_DELTA_ISLAND) {
		PRINT_ALERT(sess->hba,
				"cmd_sequence: error, exp 0x%x, max 0x%x\n",
				exp_cmdsn, max_cmdsn);
#ifdef __VMKLNX__
		VMK_ASSERT(0);
#else
		BUG_ON(1);
#endif /* __VMKLNX__ */
	}
	if (max_cmdsn > sess->max_cmdsn ||
	    max_cmdsn < sess->max_cmdsn - SN_DELTA_ISLAND)
		sess->max_cmdsn = max_cmdsn;
	if (exp_cmdsn > sess->exp_cmdsn ||
	    exp_cmdsn < sess->exp_cmdsn - SN_DELTA_ISLAND)
		sess->exp_cmdsn = exp_cmdsn;
}


/**
 * bnx2i_process_scsi_resp - complete SCSI command processing by calling
 *			'scsi_done', free iscsi cmd structure to free list
 *
 * @cmd:		iscsi cmd pointer
 * @resp_cqe:		scsi response cqe pointer
 *
 * validates scsi response indication for normal completion, sense data if any
 *	underflow/overflow condition and propogates SCSI response to SCSI-ML by
 *	calling scsi_done() and also returns command struct back to free pool
 */
void bnx2i_process_scsi_resp(struct bnx2i_cmd *cmd,
			    struct iscsi_cmd_response *resp_cqe)
{
	struct scsi_cmnd *sc = cmd->scsi_cmd;
	u16 sense_data[128];
	int data_len;
	u16 sense_len;
	int res_count;
	u8 flags;

	sc->result = (DID_OK << 16) | resp_cqe->status;

	if (resp_cqe->response != ISCSI_STATUS_CMD_COMPLETED) {
		sc->result = (DID_ERROR << 16);
		goto out;
	}

	if (resp_cqe->status) {
		data_len = resp_cqe->data_length;
		if (data_len < 2) {
			if (resp_cqe->status != SAM_STAT_CHECK_CONDITION) {
				if (data_len) {
					/* treat as if no sense data was
					 * received, because it's optional
					 * in any case*/
					bnx2i_get_rq_buf(cmd->conn,
							 (char *)sense_data,
							 data_len);
					bnx2i_put_rq_buf(cmd->conn, 1);
				}
				goto skip_sense;
			}
invalid_len:
			PRINT_ERR(cmd->conn->sess->hba,
				"CHK_CONDITION - invalid data length %d\n",
				data_len);
#if defined(__VMKLNX__) && (VMWARE_ESX_DDK_VERSION >= 50000)
			/*
			 * PR 570447: Returning BAD_TARGET breaks
			 * VMware Pluggable Storage Architecture (PSA)
			 * assumptions. Returning DID_OK with CC to 
			 * indicate specific error.
			 */
			sc->result = DID_OK << 16;
			sc->result |= (CHECK_CONDITION << 1);
			if ((sc->sense_buffer[0] & 0x7f) == 0x70 || 
				 (sc->sense_buffer[0] & 0x7f) == 0x71) {
				/* Sense buffer already set, no action required; */
			} else {
				/* Sense buffer not set, setting to CC w/ HARDWARE ERROR */
				memset (sc->sense_buffer, 0, sizeof(sc->sense_buffer));
				sc->sense_buffer[0] = 0x70;		// Fixed format sense data;
				sc->sense_buffer[2] = 0x4; 		// SENSE KEY = HARDWARE ERROR;
			}
#else
			sc->result = (DID_BAD_TARGET << 16);
#endif
			goto out;
		}

		if (data_len > BNX2I_RQ_WQE_SIZE) {
			PRINT_ALERT(cmd->conn->sess->hba,
					"sense data len %d > RQ sz\n",
					data_len);
			data_len = BNX2I_RQ_WQE_SIZE;
		}
		if (data_len) {
			memset(sc->sense_buffer, 0, sizeof(sc->sense_buffer));
			bnx2i_get_rq_buf(cmd->conn, (char *)sense_data, data_len);
			bnx2i_put_rq_buf(cmd->conn, 1);
			cmd->conn->total_data_octets_rcvd += data_len;
			sense_len = be16_to_cpu(*((__be16 *) sense_data));

			if (data_len < sense_len)
				goto invalid_len;

			if (sense_len > SCSI_SENSE_BUFFERSIZE)
				sense_len = SCSI_SENSE_BUFFERSIZE;

			memcpy(sc->sense_buffer, &sense_data[1],
			       (int) sense_len);
		}
	}
skip_sense:
	flags = resp_cqe->response_flags;
	if (flags & (ISCSI_CMD_RESPONSE_RESIDUAL_UNDERFLOW |
		     ISCSI_CMD_RESPONSE_RESIDUAL_OVERFLOW)) {
		res_count = resp_cqe->residual_count;

		if (res_count > 0 && (flags &
				      ISCSI_CMD_RESPONSE_RESIDUAL_OVERFLOW ||
		    		      res_count <= scsi_bufflen(sc))) {
			if (scsi_bufflen(sc) < res_count)
				scsi_set_resid(sc, scsi_bufflen(sc));
			else
				scsi_set_resid(sc, res_count);
			cmd->conn->total_data_octets_rcvd -= res_count;
		} 
#if defined(__VMKLNX__)
		/*
		 * PR 570447: Returning BAD_TARGET breaks VMware Pluggable Storage 
		 * Architecture (PSA) assumptions. With OVERFLOW/UNDERFLOW condition,
		 * returnning DID_ERROR seems make sense.
		 */
		else if (flags & ISCSI_CMD_RESPONSE_RESIDUAL_UNDERFLOW) {
			if (scsi_bufflen(sc) - res_count < sc->underflow) {
				PRINT_ERR (cmd->conn->sess->hba,
					"RESIDUAL UNDERFLOW, returning ERROR.\n");
//				return DID_ERROR << 16;
			} else {
				PRINT_ERR (cmd->conn->sess->hba,
					"RESIDUAL UNDERFLOW, returning BUS_BUSY.\n");
				scsi_set_resid(sc, res_count);
//				return DID_BUS_BUSY << 16;
			}
		} else if (flags & ISCSI_CMD_RESPONSE_RESIDUAL_OVERFLOW) {
			PRINT_ERR (cmd->conn->sess->hba, 
				"RESIDUAL OVERFLOW, returning ERROR.\n");
//			return DID_ERROR << 16;
		}
#else
		else
			sc->result = (DID_BAD_TARGET << 16) | resp_cqe->status;
#endif
	}
out:
	return;
}

/**
 * bnx2i_indicate_login_resp - process iscsi login response
 *
 * @conn: 		iscsi connection pointer
 *
 * pushes login response PDU to application daemon, 'iscsid' by
 *		calling iscsi_recv_pdu()
 */
int bnx2i_indicate_login_resp(struct bnx2i_conn *conn)
{
	int data_len;
	struct iscsi_login_rsp *login_resp =
		(struct iscsi_login_rsp *) &conn->gen_pdu.resp_hdr;

	/* check if this is the first login response for this connection.
	 * If yes, we need to copy initial StatSN to connection structure.
	 */
	if (conn->exp_statsn == STATSN_UPDATE_SIGNATURE) {
		conn->exp_statsn = ntohl(login_resp->statsn) + 1;
	}

	if (bnx2i_login_resp_update_cmdsn(conn))
		return -EINVAL;

	data_len = conn->gen_pdu.resp_wr_ptr - conn->gen_pdu.resp_buf;
	iscsi_recv_pdu(conn->cls_conn, (struct iscsi_hdr *) login_resp,
		       (char *) conn->gen_pdu.resp_buf, data_len);

	return 0;
}


/**
 * bnx2i_indicate_logout_resp - process iscsi logout response
 *
 * @conn: 		iscsi connection pointer
 *
 * pushes logout response PDU to application daemon, 'iscsid' by
 *		calling iscsi_recv_pdu()
 */
int bnx2i_indicate_logout_resp(struct bnx2i_conn *conn)
{
	struct iscsi_logout_rsp *logout_resp =
		(struct iscsi_logout_rsp *) &conn->gen_pdu.resp_hdr;

	BNX2I_DBG(DBG_CONN_EVENT, conn->sess->hba,
		  "indicate logout resp, cid %d, cls_conn %p, logout_hdr %p\n",
		  conn->ep->ep_iscsi_cid, conn->cls_conn, logout_resp);

	iscsi_recv_pdu(conn->cls_conn, (struct iscsi_hdr *) logout_resp,
		       (char *) NULL, 0);
	return 0;
}


/**
 * bnx2i_indicate_async_mesg - process iscsi ASYNC message indication
 *
 * @conn: 		iscsi connection pointer
 *
 * pushes iSCSI async PDU to application daemon, 'iscsid' by calling
 *	iscsi_recv_pdu()
 */
int bnx2i_indicate_async_mesg(struct bnx2i_conn *conn)
{
	struct iscsi_async *async_msg =
		(struct iscsi_async *) &conn->gen_pdu.async_hdr;

	PRINT(conn->sess->hba, "%s: indicating async message on cid %d\n",
		__FUNCTION__, conn->ep->ep_iscsi_cid);

	iscsi_recv_pdu(conn->cls_conn, (struct iscsi_hdr *) async_msg,
		       (char *) NULL, 0);
	return 0;
}



/**
 * bnx2i_process_nopin - process iscsi nopin pdu
 *
 * @conn: 		iscsi connection pointer
 * @cmd:		iscsi cmd pointer
 * @data_buf:		payload buffer pointer
 * @data_len:		payload length
 *
 * pushes nopin pdu to application daemon, 'iscsid' by calling iscsi_recv_pdu
 */
int bnx2i_process_nopin(struct bnx2i_conn *conn, struct bnx2i_cmd *cmd,
			char *data_buf, int data_len)
{
	struct iscsi_nopin *nopin_msg =
		(struct iscsi_nopin *) &conn->gen_pdu.nopin_hdr;

	iscsi_recv_pdu(conn->cls_conn, (struct iscsi_hdr *) nopin_msg,
		       (char *) data_buf, data_len);

	conn->sess->last_noopin_indicated = jiffies;
	conn->sess->noopin_indicated_count++;

	cmd->iscsi_opcode = 0;
	return 0;
}



/**
 * bnx2i_iscsi_prep_generic_pdu_bd - prepares BD table to be used with
 *			generic iscsi pdus
 *
 * @conn: 		iscsi connection pointer
 *
 * Allocates buffers and BD tables before shipping requests to cnic
 *	for PDUs prepared by 'iscsid' daemon
 */
static void bnx2i_iscsi_prep_generic_pdu_bd(struct bnx2i_conn *conn)
{
	struct iscsi_bd *bd_tbl;

	bd_tbl = (struct iscsi_bd *) conn->gen_pdu.login_req.pgtbl;

	bd_tbl->buffer_addr_hi =
		(u32) ((u64) conn->gen_pdu.login_req.mapping >> 32);
	bd_tbl->buffer_addr_lo = (u32) conn->gen_pdu.login_req.mapping;
	bd_tbl->buffer_length = conn->gen_pdu.req_wr_ptr -
				conn->gen_pdu.req_buf;
	bd_tbl->reserved0 = 0;
	bd_tbl->flags = ISCSI_BD_LAST_IN_BD_CHAIN |
			ISCSI_BD_FIRST_IN_BD_CHAIN;

	bd_tbl = (struct iscsi_bd  *) conn->gen_pdu.login_resp.pgtbl;
	bd_tbl->buffer_addr_hi = (u64) conn->gen_pdu.login_resp.mapping >> 32;
	bd_tbl->buffer_addr_lo = (u32) conn->gen_pdu.login_resp.mapping;
	bd_tbl->buffer_length = ISCSI_CONN_LOGIN_BUF_SIZE;
	bd_tbl->reserved0 = 0;
	bd_tbl->flags = ISCSI_BD_LAST_IN_BD_CHAIN |
			ISCSI_BD_FIRST_IN_BD_CHAIN;
}


/**
 * bnx2i_nopout_check_active_cmds - checks if iscsi link is idle
 *
 * @hba: 		pointer to adapter instance
 *
 * called to check if iscsi connection is idle or not. Pro-active nopout
 *	 is sent only if the link is idle
 */
static int bnx2i_nopout_check_active_cmds(struct bnx2i_conn *conn,
					  struct bnx2i_cmd *cmnd)
{
	struct iscsi_nopin *nopin_msg =
		(struct iscsi_nopin *) &conn->gen_pdu.resp_hdr;

	if ((conn->nopout_num_scsi_cmds == conn->num_scsi_cmd_pdus) &&
	    !conn->sess->active_cmd_count) {
		return -1;
	}

	memset(nopin_msg, 0x00, sizeof(struct iscsi_nopin));
        nopin_msg->opcode = ISCSI_OP_NOOP_IN;
        nopin_msg->flags = ISCSI_FLAG_CMD_FINAL;
        memcpy(nopin_msg->lun, conn->gen_pdu.nopout_hdr.lun, 8);
        nopin_msg->itt = conn->gen_pdu.nopout_hdr.itt;
        nopin_msg->ttt = ISCSI_RESERVED_TAG;
        nopin_msg->statsn = conn->gen_pdu.nopout_hdr.exp_statsn;;
        nopin_msg->exp_cmdsn = htonl(conn->sess->exp_cmdsn);
        nopin_msg->max_cmdsn = htonl(conn->sess->max_cmdsn);

	iscsi_recv_pdu(conn->cls_conn, (struct iscsi_hdr *) nopin_msg,
		       (char *) NULL, 0);

	conn->nopout_num_scsi_cmds = conn->num_scsi_cmd_pdus;
	return 0;
}


/**
 * bnx2i_iscsi_send_generic_request - called to send iscsi login/nopout/logout
 *			pdus
 *
 * @hba: 		pointer to adapter instance
 *
 * called to transmit PDUs prepared by the 'iscsid' daemon. iSCSI login,
 *	Nop-out and Logout requests flow through this path.
 */
static int bnx2i_iscsi_send_generic_request(struct bnx2i_cmd *cmnd)
{
	int rc = 0;
	struct bnx2i_conn *conn = cmnd->conn;

	bnx2i_iscsi_prep_generic_pdu_bd(conn);
	switch (cmnd->iscsi_opcode & ISCSI_OPCODE_MASK) {
	case ISCSI_OP_LOGIN:
		bnx2i_send_iscsi_login(conn, cmnd);
		break;

	case ISCSI_OP_NOOP_OUT:
		if (!bnx2i_nopout_when_cmds_active)
			if (!bnx2i_nopout_check_active_cmds(conn, cmnd)) {
				return 0;
			}

		conn->nopout_num_scsi_cmds = conn->num_scsi_cmd_pdus;
		rc = bnx2i_send_iscsi_nopout(conn, cmnd, NULL, 0);
		break;

	case ISCSI_OP_LOGOUT:
		rc = bnx2i_send_iscsi_logout(conn, cmnd);
		break;

	default:
		PRINT_ALERT(conn->sess->hba, "send_gen: unsupported op 0x%x\n",
				   cmnd->iscsi_opcode);
	}
	return rc;
}


/**********************************************************************
 *		SCSI-ML Interface
 **********************************************************************/

/**
 * bnx2i_cpy_scsi_cdb - copies LUN & CDB fields in required format to sq wqe
 *
 * @sc: 		SCSI-ML command pointer
 * @cmd:		iscsi cmd pointer
 *
 */
static void bnx2i_cpy_scsi_cdb(struct scsi_cmnd *sc,
				      struct bnx2i_cmd *cmd)
{
	u32 dword;
	int lpcnt;
	u8 *srcp;
	u32 *dstp;
	u32 scsi_lun[2];

#if defined(__VMKLNX__) && (VMWARE_ESX_DDK_VERSION >= 60000)
	bnx2i_int_to_scsilun_with_sec_lun_id(sc->device->lun,
		(struct scsi_lun *) scsi_lun,
		vmklnx_scsi_cmd_get_secondlevel_lun_id(sc));
#else
	int_to_scsilun(sc->device->lun, (struct scsi_lun *) scsi_lun);
#endif /* defined(__VMKLNX__) && (VMWARE_ESX_DDK_VERSION >= 60000) */
	cmd->req.lun[0] = ntohl(scsi_lun[0]);
	cmd->req.lun[1] = ntohl(scsi_lun[1]);

	lpcnt = cmd->scsi_cmd->cmd_len / sizeof(dword);
	srcp = (u8 *) sc->cmnd;
	dstp = (u32 *) cmd->req.cdb;
	while (lpcnt--) {
		memcpy(&dword, (const void *) srcp, 4);
		*dstp = cpu_to_be32(dword);
		srcp += 4;
		dstp++;
	}
	if (sc->cmd_len & 0x3) {
		dword = (u32) srcp[0] | ((u32) srcp[1] << 8);
		*dstp = cpu_to_be32(dword);
	}
}


#ifdef __VMKLNX__
static int bnx2i_slave_configure(struct scsi_device *sdev)
{
	return 0;
}

static int bnx2i_slave_alloc(struct scsi_device *sdev)
{
	struct iscsi_cls_session *cls_sess;

	cls_sess = iscsi_lookup_session(sdev->host->host_no,
					sdev->channel, sdev->id);
	if (!cls_sess)
		return FAILED;

	sdev->hostdata = cls_sess->dd_data;
	return 0;
}

static int bnx2i_target_alloc(struct scsi_target *starget)
{
	struct iscsi_cls_session *cls_sess;
	struct Scsi_Host *shost = dev_to_shost(starget->dev.parent);

	cls_sess = iscsi_lookup_session(shost->host_no,
	                                starget->channel, starget->id);
	if (!cls_sess)
		return FAILED;

	starget->hostdata = cls_sess->dd_data;
	return 0;
}

static void bnx2i_target_destroy(struct scsi_target *starget)
{
	struct bnx2i_sess *sess = starget->hostdata;

	if (!sess)
		return;

	if (sess->state == BNX2I_SESS_DESTROYED)
		bnx2i_release_session_resc(sess->cls_sess);
	else
		sess->state = BNX2I_SESS_TARGET_DESTROYED;
}
#endif


#define BNX2I_SERIAL_32 2147483648UL

static int iscsi_cmd_win_closed(struct bnx2i_sess *sess)
{
	u32 cmdsn = sess->cmdsn;
	u32 maxsn = sess->max_cmdsn;

	return ((cmdsn < maxsn && (maxsn - cmdsn > BNX2I_SERIAL_32)) ||
		 (cmdsn > maxsn && (cmdsn - maxsn < BNX2I_SERIAL_32)));
		
}


/**
 * bnx2i_queuecommand - SCSI ML - bnx2i interface function to issue new commands
*			to be shipped to iscsi target
 *
 * @sc: 		SCSI-ML command pointer
 * @done: 		callback function pointer to complete the task
 *
 * handles SCSI command queued by SCSI-ML, allocates a command structure,
 *	assigning CMDSN, mapping SG buffers and delivers it to CNIC for further
 *	processing. This routine also takes care of iSCSI command window full
 *	condition, if session is in recovery process and other error conditions
 */
int bnx2i_queuecommand(struct scsi_cmnd *sc,
		       void (*done) (struct scsi_cmnd *))
{
	struct bnx2i_scsi_task *scsi_task;
	struct bnx2i_sess *sess;
	struct bnx2i_conn *conn;
#if !defined(__VMKLNX__)
	struct Scsi_Host *shost;
#endif

#ifdef __VMKLNX__
	if (sc->device && sc->device->hostdata)
		sess = (struct bnx2i_sess *)sc->device->hostdata;
	else
		goto dev_not_found;
#else
	sess = iscsi_hostdata(sc->device->host->hostdata);
#endif
	sc->scsi_done = done;
	sc->result = 0;

	if (!sess)
		goto dev_not_found;

#ifdef __VMKLNX__
	if (sess->state == BNX2I_SESS_DESTROYED)
		goto dev_offline;
#endif
	if (sess->state == BNX2I_SESS_IN_SHUTDOWN ||
	    sess->state == BNX2I_SESS_IN_LOGOUT || !sess->lead_conn)
#ifdef __VMKLNX__
		/* delay offline indication till session is destroyed */
		goto cmd_not_accepted;
#else
		goto dev_not_found;
#endif
#if 0	/* For performance reasons visor does not like us returning busy */
	if (iscsi_cmd_win_closed(sess) &&
	    sess->pend_cmd_count >= (sess->sq_size / 4)) {
		sess->host_busy_cmd_win++;
		goto cmd_not_accepted;
	}
#endif
		
	spin_lock_bh(&sess->lock);
	if (sess->recovery_state) {
		 if (sess->recovery_state & ISCSI_SESS_RECOVERY_START) {
			spin_unlock_bh(&sess->lock);
			goto cmd_not_accepted;
		} else {
			spin_unlock_bh(&sess->lock);
			goto dev_not_found;
		}
	}

	conn = sess->lead_conn;
	if (!conn || 
	    !conn->ep || 
	    conn->ep->state != EP_STATE_ULP_UPDATE_COMPL) {

		spin_unlock_bh(&sess->lock);
		goto dev_not_found;
	}
	scsi_task = bnx2i_alloc_scsi_task(sess);
	if (!scsi_task) {
		sess->alloc_scsi_task_failed++;
		spin_unlock_bh(&sess->lock);
		goto cmd_not_accepted;
	}

	scsi_task->scsi_cmd = sc;
	list_add_tail(&scsi_task->link, &sess->pend_cmd_list);
	sess->pend_cmd_count++;
	if (sess->hba->max_scsi_task_queued < sess->pend_cmd_count)
		sess->hba->max_scsi_task_queued = sess->pend_cmd_count;
	sess->total_cmds_queued++;
	spin_unlock_bh(&sess->lock);

        if (atomic_read(&conn->worker_enabled)) {
#ifdef __VMKLNX__
		atomic_set(&conn->lastSched,12);
		tasklet_schedule(&conn->conn_tasklet);
#else
		shost = bnx2i_conn_get_shost(conn);
		scsi_queue_work(shost, &conn->conn_worker);
#endif		/* __VMKLNX__ */
	}
	return 0;

cmd_not_accepted:
	return SCSI_MLQUEUE_HOST_BUSY;

#ifdef __VMKLNX__
dev_offline:
#endif
dev_not_found:
	sc->result = (DID_NO_CONNECT << 16);
	scsi_set_resid(sc, scsi_bufflen(sc));
	sc->scsi_done(sc);
	return 0;
}



static void bnx2i_conn_poll(unsigned long data)
{
	struct bnx2i_conn *conn = (struct bnx2i_conn *) data;
#if !defined(__VMKLNX__)
	struct Scsi_Host *shost;
#endif

	if (!atomic_read(&conn->worker_enabled))
		goto exit;

	if (bnx2i_cqe_work_pending(conn) ||
	    !list_empty(&conn->sess->pend_cmd_list)) {
#ifdef __VMKLNX__
		tasklet_schedule(&conn->conn_tasklet);
#else
	shost = bnx2i_conn_get_shost(conn);
	scsi_queue_work(shost, &conn->conn_worker);
#endif
	}
exit:
	conn->poll_timer.expires = 50 + jiffies;	/* 500 msec */
	add_timer(&conn->poll_timer);
}


/**
 * bnx2i_fail_cmd - fail the command back to SCSI-ML
 *
 * @sess: 		iscsi sess pointer
 * @cmd: 		command pointer
 *
 * 	Return failed command to SCSI-ML.
 */
void bnx2i_fail_cmd(struct bnx2i_sess *sess, struct bnx2i_cmd *cmd)
{
	struct scsi_cmnd *sc;
	int reason;

	bnx2i_iscsi_unmap_sg_list(sess->hba, cmd);

	spin_lock_bh(&sess->lock);
	sc = cmd->scsi_cmd;
	reason = cmd->failed_reason;
	cmd->req.itt &= ISCSI_CMD_RESPONSE_INDEX;
	atomic_set(&cmd->cmd_state, ISCSI_CMD_STATE_COMPLETED);
	sess->active_cmd_count--;
	cmd->scsi_cmd = NULL;
	bnx2i_free_cmd(sess, cmd);
	spin_unlock_bh(&sess->lock);

	bnx2i_return_failed_command(sess, sc, scsi_bufflen(sc), reason);
}


/**
 * bnx2i_scsi_cmd_in_pend_list - 
 *
 * @sess: 		session pointer
 * @sc: 		SCSI-ML command pointer
 *
 * Check to see if command to be aborted is in the pending list.
 * This routine is called with sess->lock held.
 */
struct bnx2i_scsi_task *bnx2i_scsi_cmd_in_pend_list(struct bnx2i_sess *sess,
						    struct scsi_cmnd *sc)
{
	struct bnx2i_scsi_task *scsi_task;

	list_for_each_entry(scsi_task, &sess->pend_cmd_list, link) {
		if (scsi_task->scsi_cmd == sc)
			return scsi_task;
	}
	return NULL;
}

/**
 * bnx2i_send_tmf_wait_cmpl - executes scsi command abort process
 *
 * @sc: 		SCSI-ML command pointer
 *
 * initiate command abort process by requesting CNIC to send
 *	an iSCSI TMF request to target
 */
static int bnx2i_send_tmf_wait_cmpl(struct bnx2i_sess *sess)
{
	int rc = 0;
	struct bnx2i_cmd *tmf_cmd = sess->scsi_tmf_cmd;
	struct bnx2i_conn *conn = sess->lead_conn;
#ifndef __VMKLNX__
	struct Scsi_Host *shost = bnx2i_conn_get_shost(conn);
#endif
	struct bnx2i_hba *hba = sess->hba;

	ADD_STATS_64(sess->hba, tx_pdus, 1);
	tmf_cmd->tmf_response = ISCSI_TMF_RSP_REJECTED;

	/* Schedule the tasklet to send out the TMF pdu */
	atomic_set(&sess->tmf_pending, 1);
        if (atomic_read(&conn->worker_enabled)) {
#ifdef __VMKLNX__
		BNX2I_DBG(DBG_TMF, hba, "%s: sess %p cid %d\n",
			  __FUNCTION__, sess, conn->ep->ep_iscsi_cid);
		tasklet_schedule(&conn->conn_tasklet);
#else
		scsi_queue_work(shost, &conn->conn_worker);
#endif
	}

#define BNX2I_TMF_TIMEOUT	60 * HZ
	/* Now we wait here */
	rc = wait_event_timeout(sess->er_wait,
				((sess->recovery_state != 0) ||
				 (atomic_read(&tmf_cmd->cmd_state) != 
				  ISCSI_CMD_STATE_INITIATED)),
				BNX2I_TMF_TIMEOUT);

	if (signal_pending(current))
		flush_signals(current);

	if (rc == 0) {
		/** TIMEOUT **/
		if (conn->ep)
			BNX2I_DBG(DBG_TMF, hba, "TMF timed out for sess %p"
				  " cid %d\n", sess, conn->ep->ep_iscsi_cid);
		atomic_set(&tmf_cmd->cmd_state, ISCSI_CMD_STATE_TMF_TIMEOUT);
		atomic_set(&sess->tmf_pending, 0);
		bnx2i_do_iscsi_sess_recovery(sess, DID_RESET, 1);
		return -1;
	}

	if (atomic_read(&sess->tmf_pending)) {
		BNX2I_DBG(DBG_TMF, hba, "bnx2i: is tmf still pending\n");
		atomic_set(&sess->tmf_pending, 0);
	}

	if (tmf_cmd->tmf_response == ISCSI_TMF_RSP_COMPLETE) {
		/* normal success case */
		return 0;
	} else if (tmf_cmd->tmf_response == ISCSI_TMF_RSP_NO_TASK) {
		if (tmf_cmd->tmf_ref_cmd->scsi_cmd == tmf_cmd->tmf_ref_sc) {
			if (atomic_read(&tmf_cmd->tmf_ref_cmd->cmd_state) == ISCSI_CMD_STATE_COMPLETED) {
				/* task completed while tmf request is pending, driver is
				 * holding on to the completion 
				 */
            			return 0;
          		} else {
				/* missing command, do session recovery */
				goto do_recovery;
			}
		} else {
			return 0; /* command already completed */
		}
	}

do_recovery:
	BNX2I_DBG(DBG_TMF, hba, "%s: tmf failed for sess %p cmd %p\n",
		    __FUNCTION__, sess, tmf_cmd);
	bnx2i_do_iscsi_sess_recovery(sess, DID_RESET, 1);
	return -1;
}

static void bnx2i_cleanup_task_context(struct bnx2i_sess *sess,
					struct bnx2i_cmd *cmd, int reason)
{
	if (!cmd->scsi_cmd)
		return;

	/* cleanup on chip task context for command affected by
	 * ABORT_TASK/LUN_RESET
	 */
	cmd->failed_reason = reason;
	atomic_set(&cmd->cmd_state, ISCSI_CMD_STATE_CLEANUP_PEND);
	sess->cmd_cleanup_req++;
	bnx2i_send_cmd_cleanup_req(sess->hba, cmd);
}

/**
 * bnx2i_initiate_target_reset- executes scsi command target reset
 *
 * @sc: 		SCSI-ML command pointer
 *
 * initiate command abort process by requesting CNIC to send
 *	an iSCSI TMF request to target
 */
static int bnx2i_initiate_target_reset(struct bnx2i_sess *sess)
{
	struct bnx2i_cmd *tmf_cmd;
	struct bnx2i_hba *hba;

	hba = sess->hba;
	tmf_cmd = sess->scsi_tmf_cmd;
	atomic_set(&sess->tmf_active, 1);
	tmf_cmd->conn = sess->lead_conn;
	tmf_cmd->scsi_cmd = NULL;
	tmf_cmd->iscsi_opcode = ISCSI_OP_SCSI_TMFUNC | ISCSI_OP_IMMEDIATE;
	tmf_cmd->tmf_func = ISCSI_TM_FUNC_TARGET_WARM_RESET;
	tmf_cmd->tmf_lun = 0;
	tmf_cmd->tmf_ref_itt = ISCSI_RESERVED_TAG;
	tmf_cmd->tmf_ref_cmd = NULL;
	tmf_cmd->tmf_ref_sc = NULL;
	atomic_set(&tmf_cmd->cmd_state, ISCSI_CMD_STATE_INITIATED);

	BNX2I_DBG(DBG_TMF, hba, "%s: sess %p, conn %p\n",
		  __FUNCTION__, sess, sess->lead_conn);
	return 0;
}


/**
 * bnx2i_initiate_lun_reset- executes scsi command abort process
 *
 * @sc: 		SCSI-ML command pointer
 *
 * initiate command abort process by requesting CNIC to send
 *	an iSCSI TMF request to target
 */
static int bnx2i_initiate_lun_reset(struct bnx2i_sess *sess, struct scsi_cmnd *sc)
{
	struct bnx2i_cmd *tmf_cmd;
	struct bnx2i_conn *conn;
	struct bnx2i_hba *hba;

	hba = sess->hba;
	tmf_cmd = sess->scsi_tmf_cmd;
	atomic_set(&sess->tmf_active, 1);
	tmf_cmd->conn = conn = sess->lead_conn;
	tmf_cmd->scsi_cmd = NULL;
	tmf_cmd->iscsi_opcode = ISCSI_OP_SCSI_TMFUNC | ISCSI_OP_IMMEDIATE;
	tmf_cmd->tmf_func = ISCSI_TM_FUNC_LOGICAL_UNIT_RESET;
	tmf_cmd->tmf_lun = sc->device->lun;
	tmf_cmd->tmf_ref_itt = ISCSI_RESERVED_TAG;
	tmf_cmd->tmf_ref_cmd = NULL;
	tmf_cmd->tmf_ref_sc = NULL;
	atomic_set(&tmf_cmd->cmd_state, ISCSI_CMD_STATE_INITIATED);

	return 0;
}

/**
 * bnx2i_initiate_abort_cmd - executes scsi command abort process
 *
 * @sc: 		SCSI-ML command pointer
 *
 * initiate command abort process by requesting CNIC to send
 *	an iSCSI TMF request to target. Called with sess->lock held
 */
static int bnx2i_initiate_abort_cmd(struct bnx2i_sess *sess, struct scsi_cmnd *sc,
				    struct bnx2i_cmd **aborted_cmd)
{
	struct bnx2i_cmd *cmd;
	struct bnx2i_cmd *tmf_cmd;
	struct bnx2i_conn *conn;
	struct bnx2i_hba *hba;

	*aborted_cmd = NULL;
	hba = sess->hba;
	cmd = (struct bnx2i_cmd *) sc->SCp.ptr;

	if (!cmd || !cmd->scsi_cmd || cmd->scsi_cmd != sc) {
		/* command already completed to scsi mid-layer */
		BNX2I_DBG(DBG_TMF, hba, 
			  "%s: sess %p %x:%x:%x sc %p cmd no longer active\n",
			  __FUNCTION__, sess, sc->device->channel,
			  sc->device->id, sc->device->lun, sc);
		return -ENOENT;
	}

	*aborted_cmd = cmd;
	tmf_cmd = sess->scsi_tmf_cmd;
	atomic_set(&sess->tmf_active, 1);
	if (atomic_read(&cmd->cmd_state) == ISCSI_CMD_STATE_ABORT_REQ)
		atomic_set(&cmd->cmd_state, ISCSI_CMD_STATE_ABORT_PEND);
	atomic_set(&tmf_cmd->cmd_state, ISCSI_CMD_STATE_INITIATED);
	tmf_cmd->conn = conn = sess->lead_conn;
	tmf_cmd->scsi_cmd = NULL;
	tmf_cmd->iscsi_opcode = ISCSI_OP_SCSI_TMFUNC | ISCSI_OP_IMMEDIATE;
	tmf_cmd->tmf_func = ISCSI_TM_FUNC_ABORT_TASK;
	tmf_cmd->tmf_lun = sc->device->lun;
	tmf_cmd->tmf_ref_itt = cmd->req.itt;
	tmf_cmd->tmf_ref_cmd = cmd;
	tmf_cmd->tmf_ref_sc = cmd->scsi_cmd;
	BNX2I_DBG(DBG_TMF, hba, "%s [%lx] sess %p sc %p aborting active cmd "
		  "ref_cmd %p ref_scsi_cmd %p \n", __FUNCTION__, jiffies,
		  sess, sc, tmf_cmd->tmf_ref_cmd, tmf_cmd->tmf_ref_sc);

	return -EINPROGRESS;
}


static void bnx2i_tmf_wait_dev_offlined(struct bnx2i_sess *sess)
{
	int rc;

#define BNX2I_WAIT_OFFLINE_TIMEOUT	(4 * HZ)
	/* Either 'conn' object can get removed (session logged out) or
	 * 'ep' alone is destroyed (session in error recovery), driver need
	 * to handle both conditions.
	 */
	rc = wait_event_timeout(sess->er_wait,
				atomic_read(&sess->device_offline) ||
				(sess->lead_conn ? !sess->lead_conn->ep : 1),
				BNX2I_WAIT_OFFLINE_TIMEOUT);

	if (signal_pending(current))
		flush_signals(current);

	if (rc == 0) {
		BNX2I_DBG(DBG_TMF, sess->hba, "%s: Timeout, sess %p\n",
			  __FUNCTION__, sess);
	}
}


/**
 * bnx2i_execute_tmf_cmd - executes scsi tmf
 *
 * @sc: 		SCSI-ML command pointer
 *
 * initiate scsi tmf, support ABORT_TASK and LUN_RESET
 */
static int bnx2i_execute_tmf_cmd(struct scsi_cmnd *sc, int tmf_func)
{
	int active_cmds_failed = 0;
	int pend_cmds_failed = 0;
	struct bnx2i_cmd *cmd = NULL;
	struct bnx2i_hba *hba = NULL;
#ifndef __VMKLNX__
	struct Scsi_Host *shost;
#endif
	struct bnx2i_sess *sess = NULL;
	int rc = FAILED;
	int wait_rc;

#ifdef __VMKLNX__
	if (sc->device && sc->device->hostdata)
		sess = (struct bnx2i_sess *)sc->device->hostdata;
#else
	shost = sc->device->host;
	sess = iscsi_hostdata(shost->hostdata);
	BUG_ON(shost != sess->shost);
#endif
	if (sess == NULL) {
		printk("%s: TMF session=NULL\n", __FUNCTION__);
		return FAILED;
	}
	hba = sess->hba;

	BNX2I_DBG(DBG_TMF, hba, "%s: start, sess %p sc %p tmf_func %d\n",
		  __FUNCTION__, sess, sc, tmf_func);
	mutex_lock(&sess->tmf_mutex);

	if (test_bit(ADAPTER_STATE_GOING_DOWN, &sess->hba->adapter_state) ||
	    test_bit(ADAPTER_STATE_LINK_DOWN, &sess->hba->adapter_state)) {
		BNX2I_DBG(DBG_TMF, hba, "%s: sess %p sc %p adapter_state %lx\n",
			  __FUNCTION__, sess, sc, sess->hba->adapter_state);
		mutex_unlock(&sess->tmf_mutex);
		return FAILED;
	}

	spin_lock_bh(&sess->lock);
	if (sess->recovery_state || !is_sess_active(sess)) {
		BNX2I_DBG(DBG_TMF, hba, "%s: sess %p, sc %p not active\n",
			  __FUNCTION__, sess, sc);
		/* better to wait till device is offlined to avoid ABORT storm
		 */
		spin_unlock_bh(&sess->lock);
		bnx2i_tmf_wait_dev_offlined(sess);
		mutex_unlock(&sess->tmf_mutex);
		return FAILED;
	}

	atomic_set(&sess->tmf_active, 1);
	if (tmf_func == ISCSI_TM_FUNC_ABORT_TASK) {
		rc = bnx2i_initiate_abort_cmd(sess, sc, &cmd);
		if (rc == -ENOENT) {
			/* cmd not active */
			rc = FAILED;
			spin_unlock_bh(&sess->lock);
			goto done;
		}
		rc = SUCCESS;
	} else if (tmf_func == ISCSI_TM_FUNC_LOGICAL_UNIT_RESET) {
		bnx2i_initiate_lun_reset(sess, sc);
	} else if (tmf_func == ISCSI_TM_FUNC_TARGET_WARM_RESET) {
		bnx2i_initiate_target_reset(sess);
	} else {
		PRINT_ALERT(sess->hba, "unknown Task Mgmt Command %x\n",
		       tmf_func);
		rc = FAILED;
		spin_unlock_bh(&sess->lock);
		goto done;
	}
	spin_unlock_bh(&sess->lock);

	BNX2I_DBG(DBG_TMF, hba, "tmf wait......., sess %p sc %p\n",sess, sc);

	if (bnx2i_send_tmf_wait_cmpl(sess)) {
		/* TMF request timeout */
		rc = FAILED;
		goto done;
	}

	sess->cmd_cleanup_req = 0;
	sess->cmd_cleanup_cmpl = 0;

	if (sess->scsi_tmf_cmd->tmf_response == ISCSI_TMF_RSP_COMPLETE) {
		if (tmf_func == ISCSI_TM_FUNC_ABORT_TASK) {
			if (cmd->scsi_status_rcvd) {
				/* cmd completed while TMF was active.
				 * Now it's safe to complete command
				 * to SCSI-ML
				 */
				bnx2i_complete_cmd(sess, cmd);
			} else {
				spin_lock_bh(&sess->lock);
				bnx2i_cleanup_task_context(sess, cmd, DID_ABORT);
				spin_unlock_bh(&sess->lock);
			}
			active_cmds_failed = 1;
		} else if (tmf_func == ISCSI_TM_FUNC_LOGICAL_UNIT_RESET) {
			/* Pend queue is already flushed before issuing send TMF
			 * request on wire. This is just a redundant flush which
			 * should do allow us to detect any command queued while
			 * TMF is active
			 */
			pend_cmds_failed = bnx2i_flush_pend_queue(sess, sc, DID_RESET);
			active_cmds_failed = bnx2i_flush_cmd_queue(sess, sc, DID_RESET, 1);
		} else if (tmf_func == ISCSI_TM_FUNC_TARGET_WARM_RESET) {
			/* pend queue- Same comments as LUN RESET holds good here */
			pend_cmds_failed = bnx2i_flush_pend_queue(sess, NULL, DID_RESET);
			active_cmds_failed = bnx2i_flush_cmd_queue(sess, NULL, DID_RESET, 1);
		}
		rc = SUCCESS;
	} else if ((sess->scsi_tmf_cmd->tmf_response == ISCSI_TMF_RSP_NO_TASK) &&
		   (tmf_func == ISCSI_TM_FUNC_ABORT_TASK)) {
		if (!cmd->scsi_cmd ||
		    (cmd->scsi_cmd != sess->scsi_tmf_cmd->tmf_ref_sc)) {
			/* command already completed, later case cmd is being
			 * reused for a different I/O
			 */
			rc = FAILED;
		} else if (cmd->scsi_status_rcvd) {
			/* cmd completed while TMF was active. Now it's safe
			 * to complete the command back to SCSI-ML
			 */
			bnx2i_complete_cmd(sess, cmd);
			rc = FAILED;
		} else {
			/* we should never step into this code path as missing command
			 * will trigger session recovery in  bnx2i_send_tmf_wait_cmpl()
			 */
			PRINT(sess->hba, "%s: TMF_ABORT completed with NO_TASK,"
			      " but ITT %x is pending", sess->hba->netdev->name,
			      sess->scsi_tmf_cmd->tmf_ref_itt);
			rc = FAILED;
#ifdef __VMKLNX__
			VMK_ASSERT(0);
#else
			BUG_ON(1);
#endif /* __VMKLNX__ */
		}
	} else
		rc = FAILED;

	wait_rc = wait_event_interruptible_timeout(sess->er_wait,
			!is_sess_active(sess) ||
			(sess->cmd_cleanup_req == sess->cmd_cleanup_cmpl),
			30 * HZ);

	if (!is_sess_active(sess)) {
		/* session went into recovery due to protocol error, there won't
		 * be any CQ completions, active command cleanup will continue
		 * in ep_disconnect()
		 */
		BNX2I_DBG(DBG_TMF, hba, "sess %p in recovery\n", sess);
		rc = FAILED;
	} else if (!wait_rc) {
		BNX2I_DBG(DBG_TMF, hba,
			  "%s: cleanup did not complete in 30 seconds\n",
			  __FUNCTION__);
		/* If TCP layer is working fine, CMD_CLEANUP should complete
		 * 'Cuz all CMD before TMF REQ would have been TCP ACK'ed.
		 * If there is a problem with the TCP layer, TMF request should
		 * have timed out triggering session recovery
		 */
		bnx2i_print_cqe(sess->lead_conn);
		bnx2i_print_sqe(sess->lead_conn);

		PRINT(hba, "%s: sess %p - ITT cleanup request timed out\n",
		      hba->netdev->name, sess);
		/* Force session recovery */
		bnx2i_do_iscsi_sess_recovery(sess, DID_RESET, 1);
		rc = FAILED;
#ifdef __VMKLNX__
			VMK_ASSERT(0);
#else
			BUG_ON(1);
#endif /* __VMKLNX__ */
	}

	if (signal_pending(current))
		flush_signals(current);
	BNX2I_DBG(DBG_TMF, hba, "sess %p async cmd cleanup. req %d, comp %d\n",
		  sess, sess->cmd_cleanup_req, sess->cmd_cleanup_cmpl);

done:
	BNX2I_DBG(DBG_TMF, hba, "%s: sess %p sc %p cmds stats, AC=%d, PD=%d,"
		  " Q=%d, S=%d, D=%d, F=%d, CC=%d\n", __FUNCTION__, sess, sc,
		  active_cmds_failed, pend_cmds_failed,
		  sess->total_cmds_queued, sess->total_cmds_sent,
		  sess->total_cmds_completed, sess->total_cmds_failed,
		  sess->total_cmds_completed_by_chip);

	barrier();
	atomic_set(&sess->tmf_active, 0);
	mutex_unlock(&sess->tmf_mutex);

	return rc;
}

static void bnx2i_wait_for_tmf_completion(struct bnx2i_sess *sess)
{
	int lpcnt = 200;

	while (lpcnt-- && atomic_read(&sess->tmf_active))
		msleep(100);
}

/**
 * bnx2i_abort - 'eh_abort_handler' api function to abort an oustanding
 *			scsi command
 *
 * @sc: 		SCSI-ML command pointer
 *
 * SCSI abort request handler.
 */
int bnx2i_abort(struct scsi_cmnd *sc)
{
	int reason;
	struct bnx2i_hba *hba;
	struct bnx2i_cmd *cmd;
	struct bnx2i_sess *sess = (struct bnx2i_sess *)sc->device->hostdata;;
	struct bnx2i_scsi_task *scsi_task;

	/**
	 * we can ALWAYS abort from the pending queue
	 * since it has not made it to the chip yet
	 * NOTE: the queue has to be protected via spin lock
	 */
	spin_lock_bh(&sess->lock);
	cmd = (struct bnx2i_cmd *) sc->SCp.ptr;
	hba = sess->hba;
	scsi_task = bnx2i_scsi_cmd_in_pend_list(sess, sc);
	if (scsi_task) {
		sc->result = (DID_ABORT << 16);
		list_del_init(&scsi_task->link);
		bnx2i_free_scsi_task(sess, scsi_task);
		sess->pend_cmd_count--;
		spin_unlock_bh(&sess->lock);
		bnx2i_return_failed_command(sess, sc,
						scsi_bufflen(sc), DID_ABORT);
		BNX2I_DBG(DBG_TMF, hba,
			"%s: sess %p %x:%x:%x sc %p aborted from "
			"pending queue\n", __FUNCTION__, sess,
			sc->device->channel, sc->device->id,
			sc->device->lun, sc);
		return SUCCESS;
      	}

	/** It wasn't in the pending queue... and it still has no cmd object
	 * it must have completed out.
	 */
	if (unlikely(!cmd) || cmd->scsi_cmd != sc) {
		/* command already completed to scsi mid-layer */
		BNX2I_DBG(DBG_TMF, sess->hba,
			  "%s: sess %p %x:%x:%x sc %p cmd no longer active\n",
			  __FUNCTION__, sess, sc->device->channel,
			  sc->device->id, sc->device->lun, sc);
		spin_unlock_bh(&sess->lock);
		return FAILED;
	}

	if ((atomic_read(&cmd->cmd_state) != ISCSI_CMD_STATE_INITIATED) ||
	    !cmd->conn->ep) {
		/* Command completion is being processed, fail the abort request
		 * Second condition should never be true unless SCSI layer is
		 * out of sync
		 */
		spin_unlock_bh(&sess->lock);
		return FAILED;
	}
	/* Set cmd_state so that command will not be completed to SCSI-ML
	 * if SCSI_RESP is rcvd for this command
	 */
	atomic_set(&cmd->cmd_state, ISCSI_CMD_STATE_ABORT_REQ);

	BNX2I_DBG(DBG_TMF, sess->hba,
		  "%s: sess %p %x:%x:%x sc %p aborting task\n",
		  __FUNCTION__, sess, sc->device->channel, sc->device->id,
		  sc->device->lun, sc);

	spin_unlock_bh(&sess->lock);

	reason = bnx2i_execute_tmf_cmd(sc, ISCSI_TM_FUNC_ABORT_TASK);
	return reason;
}



/**
 * bnx2i_return_failed_command - return failed command back to SCSI-ML
 *
 * @sess:		iscsi session pointer
 * @cmd:		iscsi cmd pointer
 * @reason: 		SCSI-ML error code, DID_ABORT, DID_BUS_BUSY
 *
 * completes scsi command with appropriate error code to SCSI-ML
 */
void bnx2i_return_failed_command(struct bnx2i_sess *sess, struct scsi_cmnd *sc,
				 int resid, int reason)
{
	sc->result = reason << 16;
	scsi_set_resid(sc, resid);
	sc->SCp.ptr = NULL;
	sc->scsi_done(sc);
	sess->total_cmds_failed++;
}


#ifdef __VMKLNX__
/**
 * bnx2i_host_reset - 'eh_host_reset_handler' entry point
 *
 * @sc: 		SCSI-ML command pointer
 *
 * SCSI host reset handler - Do iSCSI session recovery on all active sessions
 */
int bnx2i_host_reset(struct scsi_cmnd *sc)
{
	struct Scsi_Host *shost;
	struct bnx2i_sess *sess;
	struct bnx2i_conn *conn;
	struct bnx2i_hba *hba;
        int i = 0;

	shost = sc->device->host;
	sess = (struct bnx2i_sess *)sc->device->hostdata;
	hba = sess->hba;

	PRINT_INFO(hba, "reseting host %d\n", shost->host_no);

        for (i = 0; i < hba->max_active_conns; i++) {
                conn = bnx2i_get_conn_from_id(hba, i);
                if (!conn) continue;

		PRINT_INFO(hba, "reseting sess %d\n", conn->ep->ep_iscsi_cid);
		bnx2i_do_iscsi_sess_recovery(conn->sess, DID_RESET, 1);
        }
	return 0;
}


/**
 * bnx2i_device_reset - 'eh_device_reset_handler' entry point
 *
 * @sc: 		SCSI-ML command pointer
 *
 * SCSI host reset handler - iSCSI session recovery
 */
int bnx2i_device_reset (struct scsi_cmnd *sc)
{
	struct bnx2i_sess *sess;
	int rc = 0;

	sess = (struct bnx2i_sess *)sc->device->hostdata;
	if (!sess || !sess->lead_conn || !sess->lead_conn->ep ||
	    atomic_read(&sess->lead_conn->stop_state))
		return FAILED;

	BNX2I_DBG(DBG_TMF, sess->hba, "device reset, iscsi cid %d, lun %x, "
		  "vmkflags=0x%x\n", sess->lead_conn->ep->ep_iscsi_cid,
		  sc->device->lun, sc->vmkflags);

	if (sc->vmkflags & VMK_FLAGS_USE_LUNRESET) {
		/* LUN reset */
		PRINT_INFO(sess->hba, "LUN RESET requested, sess %p\n",
			  sess);
		rc = bnx2i_execute_tmf_cmd(sc, ISCSI_TM_FUNC_LOGICAL_UNIT_RESET);
	} else {
		/* TARGET reset */
		PRINT_INFO(sess->hba, "TARGET RESET requested, sess %p\n",
			  sess);
		rc = bnx2i_execute_tmf_cmd(sc, ISCSI_TM_FUNC_TARGET_WARM_RESET);
	}
	return rc;
}

#else
/**
 * bnx2i_host_reset - 'eh_host_reset_handler' entry point
 *
 * @sc: 		SCSI-ML command pointer
 *
 * SCSI host reset handler - iSCSI session recovery
 */
int bnx2i_host_reset(struct scsi_cmnd *sc)
{
	struct Scsi_Host *shost;
	struct bnx2i_sess *sess;
	int rc = 0;

	shost = sc->device->host;
	sess = iscsi_hostdata(shost->hostdata);
	PRINT_INFO(sess->hba, "attempting to reset host, #%d\n",
			  sess->shost->host_no);

	BUG_ON(shost != sess->shost);
	rc = bnx2i_do_iscsi_sess_recovery(sess, DID_RESET, 1);


	return rc;
}
#endif

int bnx2i_cqe_work_pending(struct bnx2i_conn *conn)
{
	struct qp_info *qp;
	volatile struct iscsi_nop_in_msg *nopin;
	int exp_seq_no;

	qp = &conn->ep->qp;
	nopin = (struct iscsi_nop_in_msg *)qp->cq_cons_qe;

	exp_seq_no = conn->ep->qp.cqe_exp_seq_sn;
	if (exp_seq_no > qp->cqe_size * 2)
		exp_seq_no -= qp->cqe_size * 2;

	if (nopin->cq_req_sn ==  exp_seq_no) {
		return 1;
	} else
		return 0;
}



static void bnx2i_process_control_pdu(struct bnx2i_sess *sess)
{
	struct scsi_cmnd *sc;
	int num_cmds;
	u8 tmf_func;

	spin_lock(&sess->lock);
        if (atomic_read(&sess->tmf_pending)) {
		tmf_func = sess->scsi_tmf_cmd->tmf_func;
		if (tmf_func == ISCSI_TM_FUNC_LOGICAL_UNIT_RESET) {
			sc = sess->scsi_tmf_cmd->tmf_ref_sc;
			spin_unlock(&sess->lock);
			num_cmds = bnx2i_flush_pend_queue(sess, sc,
							  DID_RESET);
			spin_lock(&sess->lock);
		} else if (tmf_func == ISCSI_TM_FUNC_TARGET_WARM_RESET) {
			spin_unlock(&sess->lock);
			num_cmds = bnx2i_flush_pend_queue(sess, NULL,
							  DID_RESET);
			spin_lock(&sess->lock);
		}
		bnx2i_send_iscsi_tmf(sess->lead_conn, sess->scsi_tmf_cmd);
		
		
		atomic_set(&sess->tmf_pending, 0);
	}
        if (atomic_read(&sess->nop_resp_pending)) {
		bnx2i_iscsi_send_generic_request(sess->nopout_resp_cmd);
		atomic_set(&sess->nop_resp_pending, 0);
	}
        if (atomic_read(&sess->login_noop_pending)) {
		bnx2i_iscsi_send_generic_request(sess->login_nopout_cmd);
		atomic_set(&sess->login_noop_pending, 0);
	}
	/* flush pending SCSI cmds before transmitting logout request */
        if (atomic_read(&sess->logout_pending) &&
	    list_empty(&sess->pend_cmd_list)) {
		PRINT(sess->hba, "logout pending on cid %x\n",
			sess->lead_conn->ep->ep_iscsi_cid);
		bnx2i_iscsi_send_generic_request(sess->login_nopout_cmd);
		atomic_set(&sess->logout_pending, 0);
	}
	spin_unlock(&sess->lock);
}

static int bnx2i_conn_transmits_pending(struct bnx2i_conn *conn)
{
	struct bnx2i_sess *sess = conn->sess;
	extern int bnx2i_chip_cmd_max;

	/* If TCP connection is not active or in FFP (connection parameters updated)
	 * then do not transmit anything
	 */
	if (conn->ep && !(conn->ep->state & (EP_STATE_ULP_UPDATE_COMPL |
	    EP_STATE_CONNECT_COMPL)))
		return 0;
		
	if (sess->recovery_state ||
	    test_bit(ADAPTER_STATE_LINK_DOWN, &sess->hba->adapter_state) ||
	    list_empty(&sess->pend_cmd_list))
		return 0;

	if (test_bit(BNX2I_NX2_DEV_57710, &sess->hba->cnic_dev_type))
		return 8;

	if (sess->active_cmd_count < bnx2i_chip_cmd_max)
		return bnx2i_chip_cmd_max - sess->active_cmd_count;
	else
		return 0;
}

static int bnx2i_process_pend_queue(struct bnx2i_sess *sess)
{
	struct bnx2i_cmd *cmd;
	struct bnx2i_conn *conn;
	struct bnx2i_scsi_task *scsi_task;
	struct list_head *list;
	struct list_head *tmp;
	int xmits_per_work;
	int cmds_sent = 0;
	int rc = 0;

	xmits_per_work = bnx2i_conn_transmits_pending(sess->lead_conn);
	if (!xmits_per_work)
		return -EAGAIN;

	conn = sess->lead_conn;
	spin_lock(&sess->lock);
	list_for_each_safe(list, tmp, &sess->pend_cmd_list) {
		/* do not post any SCSI CMDS while TMF is active */
       		if (iscsi_cmd_win_closed(sess)) {
			sess->cmd_win_closed++;
			rc = -EAGAIN;
			break;
		}

		if (conn->ep && ((conn->ep->state == EP_STATE_TCP_FIN_RCVD) ||
    		    (conn->ep->state == EP_STATE_TCP_RST_RCVD))) {
			rc = -EAGAIN;
			break;
		}

		scsi_task = (struct bnx2i_scsi_task *) list;
		cmd = bnx2i_alloc_cmd(sess);
		if (cmd == NULL) {
			rc = -EAGAIN;
			break;
		}

		cmd->scsi_cmd = scsi_task->scsi_cmd;
		sess->pend_cmd_count--;
		list_del_init(&scsi_task->link);
		list_add_tail(&scsi_task->link, &sess->scsi_task_list);

		cmd->conn = sess->lead_conn;
		bnx2i_xmit_work_send_cmd(sess->lead_conn, cmd);
		cmds_sent++;
		sess->total_cmds_sent++;
		if (cmds_sent >= xmits_per_work)
			break;
	}
	spin_unlock(&sess->lock);

	return rc;
}


#ifdef __VMKLNX__
static void bnx2i_conn_main_worker(unsigned long data)
#else
static void
#if defined(INIT_DELAYED_WORK_DEFERRABLE) || defined(INIT_WORK_NAR) || (defined(__VMKLNX__) && (VMWARE_ESX_DDK_VERSION >= 50000))
bnx2i_conn_main_worker(struct work_struct *work)
#else
bnx2i_conn_main_worker(void *data)
#endif	/* INIT_DELAYED_WORK_DEFERRABLE && INIT_WORK_NAR */
#endif	/* __VMKLNX__*/
{
	struct bnx2i_sess *sess;
	int cqe_pending;
	int defer_pendq = 0;
#ifndef __VMKLNX__
	struct Scsi_Host *shost;
#if defined(INIT_DELAYED_WORK_DEFERRABLE) || defined(INIT_WORK_NAR) || (defined(__VMKLNX__) && (VMWARE_ESX_DDK_VERSION >= 50000))
	struct bnx2i_conn *conn =
		container_of(work, struct bnx2i_conn, conn_worker);
#else
	struct bnx2i_conn *conn = (struct bnx2i_conn *)data;
#endif
#else	/* __VMKLNX__ */
	struct bnx2i_conn *conn = (struct bnx2i_conn *)data;
#endif

	if (!atomic_read(&conn->worker_enabled)) {
		PRINT_ERR(conn->sess->hba,
			  "working scheduled while disabled\n");
		return;
	}

	conn->tasklet_entry++;

	sess = conn->sess;


	sess->timestamp = jiffies;	
	conn->tasklet_loop = 0;
	do {

		bnx2i_process_control_pdu(sess);

		defer_pendq = bnx2i_process_pend_queue(sess);

		if (use_poll_timer)
			cqe_pending = bnx2i_process_new_cqes(conn, 0,
						conn->ep->qp.cqe_size);
		else
			cqe_pending = bnx2i_process_new_cqes(conn, 0,
						cmd_cmpl_per_work);


		defer_pendq = bnx2i_process_pend_queue(sess);

		if (time_after(jiffies, sess->timestamp +
			       sess->worker_time_slice)) {
			conn->tasklet_timeslice_exit++;
			break;
		}
	} while (cqe_pending && !defer_pendq);

	
	if (defer_pendq  == -EAGAIN) {
		goto tasklet_exit;
	}

	if (bnx2i_cqe_work_pending(conn) ||
			      !list_empty(&sess->pend_cmd_list)) {
        	if (atomic_read(&conn->worker_enabled)) {
			atomic_set(&conn->lastSched,10);
#ifdef __VMKLNX__
			tasklet_schedule(&conn->conn_tasklet);
#else
			shost = bnx2i_conn_get_shost(conn);
			scsi_queue_work(shost, &conn->conn_worker);
#endif
		}
	}
tasklet_exit:
	bnx2i_arm_cq_event_coalescing(conn->ep, CNIC_ARM_CQE);
}

static void bnx2i_xmit_work_send_cmd(struct bnx2i_conn *conn, struct bnx2i_cmd *cmd)
{
	struct scsi_cmnd *sc = cmd->scsi_cmd;
	struct bnx2i_hba *hba  = conn->ep->hba;
	struct bnx2i_sess *sess  = conn->sess;
	struct iscsi_cmd_request *req = &cmd->req;

	cmd->req.total_data_transfer_length = scsi_bufflen(sc);
	cmd->iscsi_opcode = cmd->req.op_code = ISCSI_OP_SCSI_CMD;
	cmd->req.cmd_sn = sess->cmdsn++;

	bnx2i_iscsi_map_sg_list(hba, cmd);
	bnx2i_cpy_scsi_cdb(sc, cmd);

	req->op_attr = ISCSI_ATTR_SIMPLE;
	if (sc->sc_data_direction == DMA_TO_DEVICE) {
		req->op_attr |= ISCSI_CMD_REQUEST_WRITE;
		req->itt |= (ISCSI_TASK_TYPE_WRITE <<
				 ISCSI_CMD_REQUEST_TYPE_SHIFT);
		bnx2i_setup_write_cmd_bd_info(cmd);
	} else {
		if (scsi_bufflen(sc))
			req->op_attr |= ISCSI_CMD_REQUEST_READ;
		req->itt |= (ISCSI_TASK_TYPE_READ <<
				 ISCSI_CMD_REQUEST_TYPE_SHIFT);
	}
	req->num_bds = cmd->bd_tbl->bd_valid;
	if (!cmd->bd_tbl->bd_valid) {
		req->bd_list_addr_lo =
			(u32) sess->hba->mp_dma_buf.pgtbl_map;
		req->bd_list_addr_hi =
			(u32) ((u64) sess->hba->mp_dma_buf.pgtbl_map >> 32);
		req->num_bds = 1;
	}

	atomic_set(&cmd->cmd_state, ISCSI_CMD_STATE_INITIATED);
	sc->SCp.ptr = (char *) cmd;

	if (req->itt != ITT_INVALID_SIGNATURE) {
		list_add_tail(&cmd->link, &sess->active_cmd_list);
		sess->active_cmd_count++;
		bnx2i_send_iscsi_scsicmd(conn, cmd);
	}
}

/**********************************************************************
 *		open-iscsi interface
 **********************************************************************/


/**
 * bnx2i_update_conn_activity_counter - 
 *
 * updates 'last_rx_time_jiffies' which is used by iscsi transport layer
 *	to issue unsol iSCSI NOPOUT only when the link is truely idle
 */
void bnx2i_update_conn_activity_counter(struct bnx2i_conn *conn)
{
	struct iscsi_cls_conn *cls_conn;

	cls_conn = conn->cls_conn;
	cls_conn->last_rx_time_jiffies = jiffies;
}


/**
 * bnx2i_alloc_scsi_host_template - 
 *
 * allocates memory for SCSI host template, iSCSI template and registers
 *	this instance of NX2 device with iSCSI transport kernel module.
 */
static struct scsi_host_template *
bnx2i_alloc_scsi_host_template(struct bnx2i_hba *hba, struct cnic_dev *cnic)
{
	void *mem_ptr;
#ifndef __VMKLNX__
	u32 pci_bus_no;
	u32 pci_dev_no;
	u32 pci_func_no;
	u32 extra;
	struct ethtool_drvinfo drv_info;
#endif
	struct scsi_host_template *scsi_template;
	int mem_size;

	mem_size = sizeof(struct scsi_host_template);
	scsi_template = kmalloc(sizeof(struct scsi_host_template), GFP_KERNEL);
	if (!scsi_template) {
		PRINT_ALERT(hba, "failed to alloc memory for sht\n");
		return NULL;
	}

	mem_ptr = kmalloc(BRCM_ISCSI_XPORT_NAME_SIZE_MAX, GFP_KERNEL);
	if (mem_ptr == NULL) {
		PRINT_ALERT(hba, "failed to alloc memory for xport name\n");
		goto scsi_name_mem_err;
	}

	memcpy(scsi_template, (const void *) &bnx2i_host_template,
	       sizeof(struct scsi_host_template));
	scsi_template->name = mem_ptr;
	memcpy((void *) scsi_template->name,
	       (const void *) bnx2i_host_template.name,
	       strlen(bnx2i_host_template.name) + 1);

	mem_ptr = kmalloc(BRCM_ISCSI_XPORT_NAME_SIZE_MAX, GFP_KERNEL);
	if (mem_ptr == NULL) {
		PRINT_ALERT(hba, "failed to alloc proc name mem\n");
		goto scsi_proc_name_mem_err;
	}

	scsi_template->proc_name = mem_ptr;
	/* Can't determine device type, 5706/5708 has 40-bit dma addr limit */
	if (test_bit(BNX2I_NX2_DEV_5706, &hba->cnic_dev_type) ||
	    test_bit(BNX2I_NX2_DEV_5708, &hba->cnic_dev_type))
		scsi_template->dma_boundary = DMA_40BIT_MASK;
	else
		scsi_template->dma_boundary = DMA_64BIT_MASK;

	scsi_template->can_queue = hba->max_sqes;
	scsi_template->cmd_per_lun = scsi_template->can_queue / 2;
	if (cnic && cnic->netdev) {
#ifndef __VMKLNX__
		cnic->netdev->ethtool_ops->get_drvinfo(cnic->netdev,
							    &drv_info);
		sscanf(drv_info.bus_info, "%x:%x:%x.%d", &extra,
		       &pci_bus_no, &pci_dev_no, &pci_func_no);

		sprintf(mem_ptr, "%s-%.2x%.2x%.2x", BRCM_ISCSI_XPORT_NAME_PREFIX,
			 (u8)pci_bus_no, (u8)pci_dev_no, (u8)pci_func_no);
#else
		/**  Fill the transport name with [driver]-[pnic mac]-[vmnicX] */
		sprintf(mem_ptr, "%s-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx-%s",
				 BRCM_ISCSI_XPORT_NAME_PREFIX,
				 cnic->netdev->perm_addr[0],
				 cnic->netdev->perm_addr[1],
				 cnic->netdev->perm_addr[2],
				 cnic->netdev->perm_addr[3],
				 cnic->netdev->perm_addr[4],
				 cnic->netdev->perm_addr[5],
				 cnic->netdev->name);
#endif	/* __VMKLNX__ */
	}

	return scsi_template;

scsi_proc_name_mem_err:
	kfree(scsi_template->name);
scsi_name_mem_err:
	kfree(scsi_template);
	PRINT_ALERT(hba, "failed to allocate scsi host template\n");
	return NULL;
}



static void bnx2i_free_scsi_host_template(struct scsi_host_template *scsi_template)
{
	if (scsi_template) {
		kfree(scsi_template->proc_name);
		kfree(scsi_template->name);
		kfree(scsi_template);
	}
}


/**
 * bnx2i_alloc_iscsi_transport - 
 *
 * allocates memory for SCSI host template, iSCSI template and registers
 *	this instance of NX2 device with iSCSI transport kernel module.
 */
static struct iscsi_transport *
bnx2i_alloc_iscsi_transport(struct bnx2i_hba *hba, struct cnic_dev *cnic, 
			    struct scsi_host_template *scsi_template)
{
	void *mem_ptr;
	struct iscsi_transport *iscsi_transport;
	int mem_size;

	mem_size = sizeof(struct iscsi_transport);
	iscsi_transport = kmalloc(sizeof(struct iscsi_transport), GFP_KERNEL);
	if (!iscsi_transport) {
		PRINT_ALERT(hba, "mem error for iscsi_transport template\n");
		goto iscsi_xport_err;
	}

	memcpy((void *) iscsi_transport, (const void *) &bnx2i_iscsi_transport,
	       sizeof(struct iscsi_transport));

	iscsi_transport->host_template = scsi_template;

	mem_ptr = kmalloc(BRCM_ISCSI_XPORT_NAME_SIZE_MAX, GFP_KERNEL);
	if (mem_ptr == NULL) {
		PRINT_ALERT(hba, "mem alloc error, iscsi xport name\n");
		goto xport_name_mem_err;
	}

	iscsi_transport->name = mem_ptr;

	memcpy((void *) mem_ptr, (const void *) scsi_template->proc_name,
	       strlen(scsi_template->proc_name) + 1);

#ifdef __VMKLNX__
	if (test_bit(BNX2I_NX2_DEV_57710, &hba->cnic_dev_type))
		iscsi_transport->get_transport_limit    = bnx2i_get_5771x_limit;
	else
		iscsi_transport->get_transport_limit    = bnx2i_get_570x_limit;
#endif
	return iscsi_transport;

xport_name_mem_err:
	kfree(iscsi_transport);
iscsi_xport_err:
	PRINT_ALERT(hba, "unable to allocate iscsi transport\n");
	return NULL;
}



static void bnx2i_free_iscsi_transport(struct iscsi_transport *iscsi_transport)
{
	if (iscsi_transport) {
		kfree(iscsi_transport->name);
		kfree(iscsi_transport);
	}
}


/**
 * bnx2i_register_xport - register a bnx2i device transport name with
 *			the iscsi transport module
 *
 * @hba: 		pointer to adapter instance
 *
 * allocates memory for SCSI host template, iSCSI template and registers
 *	this instance of NX2 device with iSCSI transport kernel module.
 */
int bnx2i_register_xport(struct bnx2i_hba *hba)
{
#ifdef __VMKLNX__
	struct vmk_ScsiAdapter *vmk_adapter;
	struct vmklnx_ScsiAdapter *vmklnx_adapter;

	if (hba->shost_template)
		return -EEXIST;

	if (!test_bit(CNIC_F_CNIC_UP, &hba->cnic->flags) ||
	    !hba->cnic->max_iscsi_conn)
		return -EINVAL;
#endif

	hba->shost_template = iscsi_register_transport(hba->iscsi_transport);
	if (!hba->shost_template) {
		PRINT_ALERT(hba, "iscsi_register_transport failed\n");
		goto failed_registration;
	}
	PRINT_ALERT(hba, "netif=%s, iscsi=%s\n",
		hba->cnic->netdev->name, hba->scsi_template->proc_name);

#ifdef __VMKLNX__
	hba->shost->transportt = hba->shost_template;
	device_initialize(&hba->vm_pcidev);
	hba->vm_pcidev.parent = &hba->pcidev->dev;

#if (VMWARE_ESX_DDK_VERSION >= 60000)
	/* Register second-level lun addressing capability */
	if (vmklnx_scsi_host_set_capabilities(hba->shost, SHOST_CAP_SECONDLEVEL_ADDRESSING)) {
		PRINT_ALERT(hba, "failed to register 0x%x capability for adapter.\n",
			SHOST_CAP_SECONDLEVEL_ADDRESSING);
	}
#endif /* (VMWARE_ESX_DDK_VERSION >= 60000) */

	if (scsi_add_host(hba->shost, &hba->vm_pcidev))
		goto host_add_err;

	vmklnx_adapter = (struct vmklnx_ScsiAdapter *)hba->shost->adapter;
	vmk_adapter = (struct vmk_ScsiAdapter *)vmklnx_adapter->vmkAdapter;
	vmk_adapter->paeCapable = TRUE;

	if (iscsi_register_host(hba->shost, hba->iscsi_transport))
		goto iscsi_reg_host_err;
#endif

	return 0;

#ifdef __VMKLNX__
iscsi_reg_host_err:
	/* scsi_remove_host() will be called from
	 * bnx2i_unbind_adapter_devices()
	 */
host_add_err:
#endif
	iscsi_unregister_transport(hba->iscsi_transport);
failed_registration:
	return -ENOMEM;
}

/**
 * bnx2i_deregister_xport - unregisters bnx2i adapter's iscsi transport name
 *
 * @hba: 		pointer to adapter instance
 * 
 * de-allocates memory for SCSI host template, iSCSI template and de-registers
 *	a NX2 device instance
 */
int bnx2i_deregister_xport(struct bnx2i_hba *hba)
{
#ifdef __VMKLNX__
	if (hba->shost_template) {
#endif	/*  __VMKLNX__ */
		iscsi_unregister_transport(hba->iscsi_transport);
		hba->shost_template = NULL;
#ifdef __VMKLNX__
	}
#endif	/*  __VMKLNX__ */
	return 0;
}


int bnx2i_free_iscsi_scsi_template(struct bnx2i_hba *hba)
{
	bnx2i_free_scsi_host_template(hba->scsi_template);
	hba->scsi_template = NULL;

	bnx2i_free_iscsi_transport(hba->iscsi_transport);
	hba->iscsi_transport = NULL;

	return 0;
}


/**
 * bnx2i_session_create - create a new iscsi session
 *
 * @it: 		iscsi transport pointer
 * @scsit: 		scsi transport template pointer
 * @cmds_max: 		max commands supported
 * @qdepth: 		scsi queue depth to support
 * @initial_cmdsn: 	initial iscsi CMDSN to be used for this session
 * @host_no: 		pointer to u32 to return host no
 *
 * Creates a new iSCSI session instance on given device.
 */
#ifdef __VMKLNX__
#define _CREATE_SESS_NEW_	1
#endif

struct iscsi_cls_session *
	bnx2i_session_create(struct iscsi_transport *it,
			     struct scsi_transport_template *scsit,
#ifdef _CREATE_SESS_NEW_
			     uint16_t cmds_max, uint16_t qdepth,
#endif
			     uint32_t initial_cmdsn,
#ifdef __VMKLNX__
			     uint32_t target_id, uint32_t channel_id,
#endif
			     uint32_t *host_no)
{
	struct bnx2i_hba *hba;
	struct bnx2i_sess *sess;
	struct Scsi_Host *shost;
	struct iscsi_cls_session *cls_session;
	int ret_code;

	hba = bnx2i_get_hba_from_template(scsit);
#ifdef __VMKLNX__
	BNX2I_DBG(DBG_CONN_SETUP, hba, "%s: tgt id %d, ch id %d, cmds_max %d\n",
		  __FUNCTION__, target_id, channel_id, cmds_max);
#endif
	if (bnx2i_adapter_ready(hba))
		return NULL;

#ifdef __VMKLNX__
	shost = hba->shost;
	if (!shost)
		return NULL;

	cls_session = iscsi_create_session(shost, it, target_id, channel_id);
	if (!cls_session)
		return NULL;

	sess = cls_session->dd_data;
#else
	shost = scsi_host_alloc(hba->iscsi_transport->host_template,
				hostdata_privsize(sizeof(struct bnx2i_sess)));
	if (!shost)
		return NULL;

	shost->max_id = 1;
	shost->max_channel = 1;
	shost->max_lun = hba->iscsi_transport->max_lun;
	shost->max_cmd_len = hba->iscsi_transport->max_cmd_len;
#ifdef _NEW_CREATE_SESSION_
	if (cmds_max)
		shost->can_queue = cmds_max;
	if (qdepth)
		shost->cmd_per_lun = qdepth;
#endif	/* _NEW_CREATE_SESSION_ */
	shost->transportt = scsit;
	shost->transportt->create_work_queue = 1;
	sess = iscsi_hostdata(shost->hostdata);
#endif	/* __VMKLNX__ */
	*host_no = shost->host_no;

	if (!sess)
		goto sess_resc_fail;

	memset(sess, 0, sizeof(struct bnx2i_sess));
	sess->hba = hba;
#ifdef __VMKLNX__
	sess->cls_sess = cls_session;
#else
	sess->shost = shost;
#endif

	/*
	 * For Open-iSCSI, only normal sessions go through bnx2i.
	 * Discovery session goes through host stack TCP/IP stack.
	 */
	ret_code = bnx2i_iscsi_sess_new(hba, sess);
	if (ret_code) {
		/* failed to allocate memory */
		PRINT_ALERT(hba, "bnx2i_sess_create: unable to alloc sess\n");
		goto sess_resc_fail;
	}

	/* Update CmdSN related parameters */
	sess->cmdsn = initial_cmdsn;
	sess->exp_cmdsn = initial_cmdsn + 1;
	sess->max_cmdsn = initial_cmdsn + 1;

#ifndef __VMKLNX__
	if (scsi_add_host(shost, NULL))
		goto add_sh_fail;

	if (!try_module_get(it->owner))
		goto cls_sess_falied;

	cls_session = iscsi_create_session(shost, it, 0);
	if (!cls_session)
		goto module_put;
	*(unsigned long *)shost->hostdata = (unsigned long)cls_session;

	BNX2I_DBG(DBG_CONN_SETUP, hba, "%s: sess %p created successfully\n",
		  __FUNCTION__, sess);
	return hostdata_session(shost->hostdata);
#else
	PRINT(hba, "sess %p created successfully\n", sess);
	return cls_session;
#endif

#ifndef __VMKLNX__
module_put:
	module_put(it->owner);
cls_sess_falied:
	scsi_remove_host(shost);
add_sh_fail:
	bnx2i_iscsi_sess_release(hba, sess);
#endif
sess_resc_fail:
#ifndef __VMKLNX__
	scsi_host_put(shost);
#endif
	return NULL;
}


#ifdef __VMKLNX__
struct iscsi_cls_session *
bnx2i_session_create_vmp(struct iscsi_transport *it,
			 void *scsi_templ,
#ifdef _CREATE_SESS_NEW_
			 uint16_t cmds_max, uint16_t qdepth,
#endif
			 uint32_t initial_cmdsn,
			 uint32_t target_id, uint32_t channel_id,
			 uint32_t *host_no)
{
	struct scsi_transport_template *scsit = scsi_templ;

	return bnx2i_session_create(it, scsit,
#ifdef _CREATE_SESS_NEW_
				    cmds_max, qdepth,
#endif
				    initial_cmdsn, target_id, channel_id, host_no);
}


struct iscsi_cls_session *
	bnx2i_session_create_vm(struct iscsi_transport *it,
			     struct scsi_transport_template *scsit,
#ifdef _CREATE_SESS_NEW_
			     uint16_t cmds_max, uint16_t qdepth,
#endif
			     uint32_t initial_cmdsn,
			     uint32_t *host_no)
{
	struct bnx2i_hba *hba = bnx2i_get_hba_from_template(scsit);

	if (!hba)
		return NULL;
	return bnx2i_session_create(it, scsit,
#ifdef _CREATE_SESS_NEW_
			     cmds_max, qdepth,
#endif
			     initial_cmdsn, hba->target_id++, hba->channel_id, host_no);
}
#endif


static void bnx2i_release_session_resc(struct iscsi_cls_session *cls_session)
{
#ifdef __VMKLNX__
	struct bnx2i_sess *sess = cls_session->dd_data;
#else
	struct Scsi_Host *shost = iscsi_session_to_shost(cls_session);
	struct bnx2i_sess *sess = iscsi_hostdata(shost->hostdata);
	struct module *owner = cls_session->transport->owner;
#endif

	bnx2i_iscsi_sess_release(sess->hba, sess);

	kfree(sess->target_name);
	sess->target_name = NULL;

        iscsi_free_session(cls_session);
#ifndef __VMKLNX__
	scsi_host_put(shost);
        module_put(owner);
#endif
}

/**
 * bnx2i_session_destroy - destroys iscsi session
 *
 * @cls_session: 	pointer to iscsi cls session
 *
 * Destroys previously created iSCSI session instance and releases
 *	all resources held by it
 */

void bnx2i_session_destroy(struct iscsi_cls_session *cls_session)
{
	struct bnx2i_hba *hba;
#ifndef __VMKLNX__
	struct Scsi_Host *shost = iscsi_session_to_shost(cls_session);
	struct bnx2i_sess *sess = cls_session->dd_data;
#else
	struct bnx2i_sess *sess = cls_session->dd_data;
#endif
	hba = sess->hba;

	BNX2I_DBG(DBG_CONN_SETUP, hba, "%s: destroying sess %p\n",
		  __FUNCTION__, sess);
	bnx2i_withdraw_sess_recovery(sess);

	iscsi_remove_session(cls_session);
#ifndef __VMKLNX__
	scsi_remove_host(shost);
	bnx2i_release_session_resc(cls_session);
#else
	if (sess->state == BNX2I_SESS_TARGET_DESTROYED)
		bnx2i_release_session_resc(cls_session);
	else
		sess->state = BNX2I_SESS_DESTROYED;
#endif
	BNX2I_DBG(DBG_CONN_SETUP, hba, "%s: destroying sess %p completed\n",
		  __FUNCTION__, sess);
}

/**
 * bnx2i_sess_recovery_timeo - session recovery timeout handler
 *
 * @cls_session: 	pointer to iscsi cls session
 *
 * session recovery timeout handling routine
 */
void bnx2i_sess_recovery_timeo(struct iscsi_cls_session *cls_session)
{
#ifdef __VMKLNX__
	struct bnx2i_sess *sess = cls_session->dd_data;
#else
	struct Scsi_Host *shost = iscsi_session_to_shost(cls_session);
	struct bnx2i_sess *sess = iscsi_hostdata(shost->hostdata);
#endif

	PRINT(sess->hba, "sess %p recovery timed out\n", sess);
	spin_lock_bh(&sess->device_lock);
	if (sess->recovery_state) {
#ifdef __VMKLNX__
		iscsi_offline_session(sess->cls_sess);
		spin_lock_bh(&sess->lock);
		atomic_set(&sess->device_offline, 1);
		sess->recovery_state |= ISCSI_SESS_RECOVERY_FAILED;
		spin_unlock_bh(&sess->lock);
#else
		spin_lock_bh(&sess->lock);
		sess->recovery_state |= ISCSI_SESS_RECOVERY_FAILED;
		spin_unlock_bh(&sess->lock);
		wake_up(&sess->er_wait);
#endif
	}
	spin_unlock_bh(&sess->device_lock);
}


/**
 * bnx2i_conn_create - create iscsi connection instance
 *
 * @cls_session: 	pointer to iscsi cls session
 * @cid: 		iscsi cid as per rfc (not NX2's CID terminology)
 *
 * Creates a new iSCSI connection instance for a given session
 */
struct iscsi_cls_conn *bnx2i_conn_create(struct iscsi_cls_session *cls_session,
					 uint32_t cid)
{
#ifdef __VMKLNX__
	struct bnx2i_sess *sess = cls_session->dd_data;
#else
	struct Scsi_Host *shost = iscsi_session_to_shost(cls_session);
	struct bnx2i_sess *sess = iscsi_hostdata(shost->hostdata);
#endif
	struct bnx2i_conn *conn;
	struct iscsi_cls_conn *cls_conn;

	cls_conn = iscsi_create_conn(cls_session, cid);
	if (!cls_conn)
		return NULL;

	conn = cls_conn->dd_data;
	memset(conn, 0, sizeof(struct bnx2i_conn));
	conn->cls_conn = cls_conn;
	conn->exp_statsn = STATSN_UPDATE_SIGNATURE;
	conn->state = CONN_STATE_IDLE;
	/* Initialize the connection structure */
	if (bnx2i_iscsi_conn_new(sess, conn))
		goto mem_err;

	conn->conn_cid = cid;
	return cls_conn;

mem_err:
	iscsi_destroy_conn(cls_conn);
	return NULL;
}


/**
 * bnx2i_conn_bind - binds iscsi sess, conn and ep objects together
 *
 * @cls_session: 	pointer to iscsi cls session
 * @cls_conn: 		pointer to iscsi cls conn
 * @transport_fd: 	64-bit EP handle
 * @is_leading: 	leading connection on this session?
 *
 * Binds together iSCSI session instance, iSCSI connection instance
 *	and the TCP connection. This routine returns error code if
 *	TCP connection does not belong on the device iSCSI sess/conn
 *	is bound
 */
#ifdef __VMKLNX__
vmk_int32 bnx2i_conn_bind(struct iscsi_cls_session *cls_session,
			  struct iscsi_cls_conn *cls_conn,
			  vmk_uint64 transport_fd, vmk_int32 is_leading)
#else
int bnx2i_conn_bind(struct iscsi_cls_session *cls_session,
		    struct iscsi_cls_conn *cls_conn,
		    uint64_t transport_fd, int is_leading)
#endif
{
#ifdef __VMKLNX__
	struct bnx2i_sess *sess;
	struct Scsi_Host *shost;
#else
	struct Scsi_Host *shost = iscsi_session_to_shost(cls_session);
	struct bnx2i_sess *sess = iscsi_hostdata(shost->hostdata);
#endif
	struct bnx2i_conn *tmp;
	struct bnx2i_conn *conn;
	int ret_code;
	struct bnx2i_endpoint *ep;
	struct bnx2i_hba *hba;

#ifdef __VMKLNX__
	sess = cls_session->dd_data;
	shost = bnx2i_sess_get_shost(sess);
#endif
	conn = cls_conn->dd_data;
	hba = sess->hba;
	ep = (struct bnx2i_endpoint *) (unsigned long) transport_fd;
	BNX2I_DBG(DBG_CONN_SETUP, hba, "binding sess %p, conn %p, ep %p\n",
		  sess, conn, ep);

	/* If adapter is going down (mtu change, vlan, selftest, etc'),
	 * fail this bind request so that connection context be cleaned
	 */
	if (test_bit(ADAPTER_STATE_GOING_DOWN, &ep->hba->adapter_state)) {
		ep->hba->stop_event_ifc_abort_bind++;
		return -EPERM;
	}

	if ((ep->state == EP_STATE_TCP_FIN_RCVD) ||
	    (ep->state == EP_STATE_TCP_RST_RCVD))
		/* Peer disconnect via' FIN or RST */
		return -EINVAL;

	if (ep->hba != sess->hba) {
		/* Error - TCP connection does not belong to this device
		 */
		PRINT_ALERT(sess->hba, "conn bind, ep=0x%p (%s) does not",
				  ep, ep->hba->netdev->name);
		PRINT_ALERT(sess->hba, "belong to hba (%s)\n",
				  sess->hba->netdev->name);
		return -EEXIST;
	}

#ifdef __VMKLNX__
	spin_lock_bh(ep->hba->shost->host_lock);
#endif
	if (!sess->login_nopout_cmd)
		sess->login_nopout_cmd = bnx2i_alloc_cmd(sess);
	if (!sess->scsi_tmf_cmd)
		sess->scsi_tmf_cmd = bnx2i_alloc_cmd(sess);
	if (!sess->nopout_resp_cmd)
		sess->nopout_resp_cmd = bnx2i_alloc_cmd(sess);
#ifdef __VMKLNX__
	spin_unlock_bh(ep->hba->shost->host_lock);
#endif

	/* adjust dma boundary limit which was set to lower bound of 40-bit
	 * address as required by 5706/5708. 5709/57710 does not have any
	 * address limitation requirements. 'dma_mask' parameter is set
	 * by bnx2 module based on device requirements, we just use whatever
	 * is set.
	 */
	shost->dma_boundary = ep->hba->pcidev->dma_mask;

	/* look-up for existing connection, MC/S is not currently supported */
	spin_lock_bh(&sess->lock);
	tmp = NULL;
	if (!list_empty(&sess->conn_list)) {
		list_for_each_entry(tmp, &sess->conn_list, link) {
			if (tmp == conn)
				break;
		}
	}
	if ((tmp != conn) && (conn->sess == sess)) {
		/* bind iSCSI connection to this session */
		list_add(&conn->link, &sess->conn_list);
		if (is_leading)
			sess->lead_conn = conn;
	}

	if (conn->ep) {
		/* This happens when 'iscsid' is killed and restarted. Daemon
		 * has no clue of tranport handle, but knows active conn/sess
		 * and tried to rebind a new tranport (EP) to already active
		 * iSCSI session/connection
		 */
		spin_unlock_bh(&sess->lock);
		bnx2i_ep_disconnect((uint64_t) (unsigned long) conn->ep);
		spin_lock_bh(&sess->lock);
	}

	conn->ep = (struct bnx2i_endpoint *) (unsigned long) transport_fd;
	conn->ep->conn = conn;
	conn->ep->sess = sess;
	conn->state = CONN_STATE_XPORT_READY;
	conn->iscsi_conn_cid = conn->ep->ep_iscsi_cid;
	conn->fw_cid = conn->ep->ep_cid;

	ret_code = bnx2i_bind_conn_to_iscsi_cid(conn, ep->ep_iscsi_cid);
	spin_unlock_bh(&sess->lock);

	sess->total_cmds_sent = 0;
	sess->total_cmds_queued = 0;
	sess->total_cmds_completed = 0;
	sess->total_cmds_failed = 0;
	sess->total_cmds_completed_by_chip = 0;
	sess->cmd_win_closed = 0;
	sess->host_busy_cmd_win = 0;

	/* 5706/5708/5709 FW takes RQ as full when initiated, but for 57710
	 * driver needs to explicitly replenish RQ index during setup.
  	 */
	if (test_bit(BNX2I_NX2_DEV_57710, &ep->hba->cnic_dev_type))
		bnx2i_put_rq_buf(conn, 0);

	atomic_set(&conn->worker_enabled, 1);
	bnx2i_arm_cq_event_coalescing(conn->ep, CNIC_ARM_CQE);
	return ret_code;
}


/**
 * bnx2i_conn_destroy - destroy iscsi connection instance & release resources
 *
 * @cls_conn: 		pointer to iscsi cls conn
 *
 * Destroy an iSCSI connection instance and release memory resources held by
 *	this connection
 */
void bnx2i_conn_destroy(struct iscsi_cls_conn *cls_conn)
{
	struct bnx2i_conn *conn = cls_conn->dd_data;
	struct bnx2i_sess *sess = conn->sess;
#ifndef __VMKLNX__
	struct Scsi_Host *shost;
	shost = bnx2i_conn_get_shost(conn);
#endif
	BNX2I_DBG(DBG_CONN_SETUP, sess->hba, "%s: destroying conn %p\n",
		  __FUNCTION__, conn);
	bnx2i_conn_free_login_resources(conn->sess->hba, conn);

#ifdef __VMKLNX__
	/* disable will not remove already scheduled tasklet, it just disables it.
	 */
	tasklet_kill(&conn->conn_tasklet);
#else
	scsi_flush_work(shost);
#endif		/* __VMKLNX__ */
	atomic_set(&conn->worker_enabled, 0);

	/* Need to unbind 'conn' and 'iscsi_cid' to avoid any invalid
	 * pointer access due to spurious KCQE notifications.
	 */
	if (conn->ep)
		bnx2i_unbind_conn_from_iscsi_cid(conn, conn->ep->ep_iscsi_cid);

	if (sess) {
		int cmds_a = 0, cmds_p = 0;
		cmds_p = bnx2i_flush_pend_queue(sess, NULL, DID_RESET);
		cmds_a = bnx2i_flush_cmd_queue(sess, NULL, DID_RESET, 0);
		BNX2I_DBG(DBG_ITT_CLEANUP, 
			  sess->hba, 
			  "%s: CMDS FLUSHED, pend=%d, active=%d\n",
			  __FUNCTION__, cmds_p, cmds_a);
	}
	spin_lock_bh(&sess->lock);
	list_del_init(&conn->link);
	if (sess->lead_conn == conn)
		sess->lead_conn = NULL;

	if (conn->ep) {
		conn->ep->sess = NULL;
		conn->ep->conn = NULL;
		conn->ep = NULL;
	}
	spin_unlock_bh(&sess->lock);

	kfree(conn->persist_address);
	conn->persist_address = NULL;
	iscsi_destroy_conn(cls_conn);
}


/**
 * bnx2i_conn_set_param - set iscsi connection parameter
 *
 * @cls_conn: 		pointer to iscsi cls conn
 * @param: 		parameter type identifier
 * @buf: 		buffer pointer
 * @buflen: 		buffer length
 *
 * During FFP migration, user daemon will issue this call to
 *	update negotiated iSCSI parameters to driver.
 */
#ifdef __VMKLNX__
vmk_int32 bnx2i_conn_set_param(struct iscsi_cls_conn *cls_conn,
				enum iscsi_param param, vmk_int8 *buf,
				vmk_int32 buflen)
#else
int bnx2i_conn_set_param(struct iscsi_cls_conn *cls_conn,
			 enum iscsi_param param, char *buf, int buflen)
#endif
{
	struct bnx2i_conn *conn = cls_conn->dd_data;
	struct bnx2i_sess *sess = conn->sess;
	int ret_val = 0;

	spin_lock_bh(&sess->lock);
	if (conn->state != CONN_STATE_IN_LOGIN) {
		PRINT_ERR(sess->hba, "can't change param [%d]\n", param);
		spin_unlock_bh(&sess->lock);
		return -1;
	}
	spin_unlock_bh(&sess->lock);
	switch (param) {
	case ISCSI_PARAM_MAX_RECV_DLENGTH:
		sscanf(buf, "%d", &conn->max_data_seg_len_recv);
		break;
	case ISCSI_PARAM_MAX_XMIT_DLENGTH:
		sscanf(buf, "%d", &conn->max_data_seg_len_xmit);
		break;
	case ISCSI_PARAM_HDRDGST_EN:
		sscanf(buf, "%d", &conn->header_digest_en);
		break;
	case ISCSI_PARAM_DATADGST_EN:
		sscanf(buf, "%d", &conn->data_digest_en);
		break;
	case ISCSI_PARAM_INITIAL_R2T_EN:
		if (conn == sess->lead_conn)
			sscanf(buf, "%d", &sess->initial_r2t);
		break;
	case ISCSI_PARAM_MAX_R2T:
		if (conn == sess->lead_conn)
			sscanf(buf, "%d", &sess->max_r2t);
		break;
	case ISCSI_PARAM_IMM_DATA_EN:
		if (conn == sess->lead_conn)
			sscanf(buf, "%d", &sess->imm_data);
		break;
	case ISCSI_PARAM_FIRST_BURST:
		if (conn == sess->lead_conn)
			sscanf(buf, "%d", &sess->first_burst_len);
		break;
	case ISCSI_PARAM_MAX_BURST:
		if (conn == sess->lead_conn)
			sscanf(buf, "%d", &sess->max_burst_len);
		break;
	case ISCSI_PARAM_PDU_INORDER_EN:
		if (conn == sess->lead_conn)
			sscanf(buf, "%d", &sess->pdu_inorder);
		break;
	case ISCSI_PARAM_DATASEQ_INORDER_EN:
		if (conn == sess->lead_conn)
			sscanf(buf, "%d", &sess->dataseq_inorder);
		break;
	case ISCSI_PARAM_ERL:
		if (conn == sess->lead_conn)
			sscanf(buf, "%d", &sess->erl);
		break;
	case ISCSI_PARAM_IFMARKER_EN:
		sscanf(buf, "%d", &conn->ifmarker_enable);
#ifdef __VMKLNX__
		VMK_ASSERT(!conn->ifmarker_enable);
#else
		BUG_ON(conn->ifmarker_enable);
#endif /* __VMKLNX__ */
		ret_val = -EINVAL;
		break;
	case ISCSI_PARAM_OFMARKER_EN:
		sscanf(buf, "%d", &conn->ofmarker_enable);
#ifdef __VMKLNX__
		VMK_ASSERT(!conn->ifmarker_enable);
#else
		BUG_ON(conn->ifmarker_enable);
#endif /* __VMKLNX__ */
		ret_val = -EINVAL;
		break;
	case ISCSI_PARAM_EXP_STATSN:
		sscanf(buf, "%u", &conn->exp_statsn);
		break;
	case ISCSI_PARAM_TARGET_NAME:
		if (sess->target_name)
			break;
		sess->target_name = kstrdup(buf, GFP_KERNEL);
		if (!sess->target_name)
			ret_val = -ENOMEM;
		break;
	case ISCSI_PARAM_TPGT:
		sscanf(buf, "%d", &sess->tgt_prtl_grp);
		break;
	case ISCSI_PARAM_PERSISTENT_PORT:
		sscanf(buf, "%d", &conn->persist_port);
		break;
	case ISCSI_PARAM_PERSISTENT_ADDRESS:
		if (conn->persist_address)
			break;
		conn->persist_address = kstrdup(buf, GFP_KERNEL);
		if (!conn->persist_address)
			ret_val = -ENOMEM;
		break;
#ifdef __VMKLNX__
	case ISCSI_PARAM_ISID:
		snprintf(sess->isid, sizeof(sess->isid), "%s", buf);
		break;
#endif
	default:
		PRINT_ALERT(sess->hba, "PARAM_UNKNOWN: 0x%x\n", param);
		ret_val = -ENOSYS;
		break;
	}

	return ret_val;
}


/**
 * bnx2i_conn_get_param - return iscsi connection parameter to caller
 *
 * @cls_conn: 		pointer to iscsi cls conn
 * @param: 		parameter type identifier
 * @buf: 		buffer pointer
 *
 * returns iSCSI connection parameters
 */
#ifdef __VMKLNX__
vmk_int32 bnx2i_conn_get_param(struct iscsi_cls_conn *cls_conn,
			 enum iscsi_param param, vmk_int8 *buf)
#else
int bnx2i_conn_get_param(struct iscsi_cls_conn *cls_conn,
			 enum iscsi_param param, char *buf)
#endif
{
	struct bnx2i_conn *conn;
	int len;

	conn = (struct bnx2i_conn *)cls_conn->dd_data;
	if (!conn)
		return -EINVAL;
#ifndef __VMKLNX__
	if (!conn->ep || (conn->ep->state != EP_STATE_ULP_UPDATE_COMPL))
		return -EINVAL;
#endif

	len = 0;
	switch (param) {
	case ISCSI_PARAM_MAX_RECV_DLENGTH:
		len = sprintf(buf, "%u\n", conn->max_data_seg_len_recv);
		break;
	case ISCSI_PARAM_MAX_XMIT_DLENGTH:
		len = sprintf(buf, "%u\n", conn->max_data_seg_len_xmit);
		break;
	case ISCSI_PARAM_HDRDGST_EN:
		len = sprintf(buf, "%d\n", conn->header_digest_en);
		break;
	case ISCSI_PARAM_DATADGST_EN:
		len = sprintf(buf, "%d\n", conn->data_digest_en);
		break;
	case ISCSI_PARAM_IFMARKER_EN:
		len = sprintf(buf, "%u\n", conn->ifmarker_enable);
		break;
	case ISCSI_PARAM_OFMARKER_EN:
		len = sprintf(buf, "%u\n", conn->ofmarker_enable);
		break;
	case ISCSI_PARAM_EXP_STATSN:
		len = sprintf(buf, "%u\n", conn->exp_statsn);
		break;
	case ISCSI_PARAM_PERSISTENT_PORT:
		len = sprintf(buf, "%d\n", conn->persist_port);
		break;
	case ISCSI_PARAM_PERSISTENT_ADDRESS:
		if (conn->persist_address)
			len = sprintf(buf, "%s\n", conn->persist_address);
		break;
	case ISCSI_PARAM_CONN_PORT:
		if (conn->ep)
			len = sprintf(buf, "%u\n",
				      (uint32_t)(be16_to_cpu((__be32)conn->ep->cm_sk->dst_port)));
		else
			len = sprintf(buf, "0\n");
		break;
	case ISCSI_PARAM_CONN_ADDRESS:
		if (conn->ep) {
			if(test_bit(SK_F_IPV6, &conn->ep->cm_sk->flags)) {
				struct in6_addr ip6_addr;
				memcpy(&ip6_addr, conn->ep->cm_sk->dst_ip,
					min(sizeof(struct in6_addr), sizeof(conn->ep->cm_sk->dst_ip)));
				len = sprintf(buf, NIP6_FMT "\n", NIP6(ip6_addr));
			} else
				len = sprintf(buf, NIPQUAD_FMT "\n",
					NIPQUAD(conn->ep->cm_sk->dst_ip));
		} else
			len = sprintf(buf, "0.0.0.0\n");
		break;
	default:
		PRINT_ALERT(conn->sess->hba,
			    "get_param: conn 0x%p param %d not found\n",
			    conn, (u32)param);
		len = -ENOSYS;
	}
#ifdef __VMKLNX__
	if (len > 0)
		buf[len - 1] = '\0';
#endif

	return len;
}


/**
 * bnx2i_session_get_param - returns iscsi session parameter
 *
 * @cls_session: 	pointer to iscsi cls session
 * @param: 		parameter type identifier
 * @buf: 		buffer pointer
 *
 * returns iSCSI session parameters
 */
#ifdef __VMKLNX__
vmk_int32 bnx2i_session_get_param(struct iscsi_cls_session *cls_session,
			    enum iscsi_param param, vmk_int8 *buf)
#else
int bnx2i_session_get_param(struct iscsi_cls_session *cls_session,
			    enum iscsi_param param, char *buf)
#endif
{
#ifdef __VMKLNX__
	struct bnx2i_sess *sess = cls_session->dd_data;
#else
	struct Scsi_Host *shost = iscsi_session_to_shost(cls_session);
	struct bnx2i_sess *sess = iscsi_hostdata(shost->hostdata);
#endif
	int len = 0;

	switch (param) {
	case ISCSI_PARAM_INITIAL_R2T_EN:
		len = sprintf(buf, "%d\n", sess->initial_r2t);
		break;
	case ISCSI_PARAM_MAX_R2T:
		len = sprintf(buf, "%hu\n", sess->max_r2t);
		break;
	case ISCSI_PARAM_IMM_DATA_EN:
		len = sprintf(buf, "%d\n", sess->imm_data);
		break;
	case ISCSI_PARAM_FIRST_BURST:
		len = sprintf(buf, "%u\n", sess->first_burst_len);
		break;
	case ISCSI_PARAM_MAX_BURST:
		len = sprintf(buf, "%u\n", sess->max_burst_len);
		break;
	case ISCSI_PARAM_PDU_INORDER_EN:
		len = sprintf(buf, "%d\n", sess->pdu_inorder);
		break;
	case ISCSI_PARAM_DATASEQ_INORDER_EN:
		len = sprintf(buf, "%d\n", sess->dataseq_inorder);
		break;
	case ISCSI_PARAM_ERL:
		len = sprintf(buf, "%d\n", sess->erl);
		break;
	case ISCSI_PARAM_TARGET_NAME:
		if (sess->target_name)
			len = sprintf(buf, "%s\n", sess->target_name);
		break;
	case ISCSI_PARAM_TPGT:
		len = sprintf(buf, "%d\n", sess->tgt_prtl_grp);
		break;
#ifdef __VMKLNX__
	case ISCSI_PARAM_ISID:
		len = sprintf(buf,"%s\n", sess->isid);
		break;
#endif
	default:
		PRINT_ALERT(sess->hba, "sess_get_param: sess 0x%p", sess);
		PRINT_ALERT(sess->hba, "param (0x%x) not found\n", (u32) param);
		return -ENOSYS;
	}

#ifdef __VMKLNX__
	if (len > 0)
		buf[len - 1] = '\0';
#endif

	return len;
}


/**
 * bnx2i_conn_start - completes iscsi connection migration to FFP
 *
 * @cls_conn: 		pointer to iscsi cls conn
 *
 * last call in FFP migration to handover iscsi conn to the driver
 */
#ifdef __VMKLNX__
vmk_int32 bnx2i_conn_start(struct iscsi_cls_conn *cls_conn)
#else
int bnx2i_conn_start(struct iscsi_cls_conn *cls_conn)
#endif
{
	struct bnx2i_conn *conn = (struct bnx2i_conn *) cls_conn->dd_data;
	struct bnx2i_sess *sess;

	BNX2I_DBG(DBG_CONN_SETUP, conn->sess->hba, "%s: conn %p\n",
		  __FUNCTION__, conn);
	if (conn->state != CONN_STATE_IN_LOGIN) {
		PRINT_ALERT(conn->sess->hba,
			"conn_start: conn 0x%p state 0x%x err!!\n",
			conn, conn->state);
		return -EINVAL;
	}
	sess = conn->sess;

	if ((sess->imm_data || !sess->initial_r2t) &&
		sess->first_burst_len > sess->max_burst_len) {
		PRINT_ALERT(conn->sess->hba, "invalid params, FBL > MBL\n");
			return -EINVAL;
	}

	conn->state = CONN_STATE_FFP_STATE;
	if (sess->lead_conn == conn)
		sess->state = BNX2I_SESS_IN_FFP;

	conn->ep->state = EP_STATE_ULP_UPDATE_START;

        conn->ep->ofld_timer.expires = 10*HZ + jiffies;
        conn->ep->ofld_timer.function = bnx2i_ep_ofld_timer;
        conn->ep->ofld_timer.data = (unsigned long) conn->ep;
        add_timer(&conn->ep->ofld_timer);

	if (bnx2i_update_iscsi_conn(conn)) {
		PRINT_ERR(sess->hba, "unable to send conn update kwqe\n");
		return -ENOSPC;
	}

	/* update iSCSI context for this conn, wait for CNIC to complete */
	wait_event_interruptible(conn->ep->ofld_wait,
				 conn->ep->state != EP_STATE_ULP_UPDATE_START);

	if(conn->ep->state != EP_STATE_ULP_UPDATE_COMPL) {
		BNX2I_DBG(DBG_CONN_SETUP, conn->sess->hba, "Update iscsi conn"
				"failed cid 0x%x iscsi_cid 0x%x state = 0x%x \n",
				conn->ep->ep_cid, conn->ep->ep_iscsi_cid,
				conn->ep->state);
		return -ENOSPC;
	}

	if (signal_pending(current))
		flush_signals(current);
	del_timer_sync(&conn->ep->ofld_timer);

	/* cannot hold 'sess->lock' during iscsi_trans API calls. Doing so will
	 * cause PCPU lockup. That is the reason for taking a lock other than
	 * 'sess->lock'
	 */
	spin_lock_bh(&sess->device_lock);
	switch (atomic_read(&conn->stop_state)) {
	case STOP_CONN_RECOVER:
		BNX2I_DBG(DBG_SESS_RECO, sess->hba, "sess %p recovery CMPL\n",
			  sess);
		sess->recovery_state = 0;
		sess->state = BNX2I_SESS_IN_FFP;
		atomic_set(&conn->stop_state, 0);
#ifdef __VMKLNX__
		iscsi_unblock_session(cls_conn->session);
#else
		iscsi_unblock_session(session_to_cls(sess));
#endif
		atomic_set(&sess->device_offline, 0);
		wake_up(&sess->er_wait);
		break;
	case STOP_CONN_TERM:
		break;
	default:
		;
	}
	spin_unlock_bh(&sess->device_lock);

	if (use_poll_timer)
		add_timer(&conn->poll_timer);

	BNX2I_DBG(DBG_CONN_SETUP, sess->hba, "%s: conn %p completed\n",
		  __FUNCTION__, conn);

	return 0;
}


/**
 * bnx2i_conn_stop - stop any further processing on this connection
 *
 * @cls_conn: 		pointer to iscsi cls conn
 * @flags: 		reason for freezing this connection
 *
 * call to take control of iscsi conn from the driver. Could be called
 *	when login failed, when recovery is to be attempted or during
 *	connection teardown
 */
#ifdef __VMKLNX__
vmk_int32 bnx2i_conn_stop(struct iscsi_cls_conn *cls_conn, vmk_int32 flag)
#else
void bnx2i_conn_stop(struct iscsi_cls_conn *cls_conn, int flag)
#endif
{
	struct bnx2i_conn *conn = (struct bnx2i_conn *)cls_conn->dd_data;
	struct bnx2i_sess *sess = conn->sess;
	int icid = 0xFFFF;

	if (conn->ep)
		icid = conn->ep->ep_iscsi_cid;

	PRINT(sess->hba, "%s::%s - sess %p conn %p, icid %d, "
	      "cmd stats={p=%d,a=%d,ts=%d,tc=%d}, ofld_conns %d\n",
	      __FUNCTION__, sess->hba->netdev->name, sess, conn, icid,
	      sess->pend_cmd_count, sess->active_cmd_count,
	      sess->total_cmds_sent, sess->total_cmds_completed,
	      sess->hba->ofld_conns_active);

	atomic_set(&conn->stop_state, flag);
	conn->state = CONN_STATE_XPORT_FREEZE;
#ifdef __VMKLNX__
	/**
	 * The strategy here is to take the SAME recovery path as if it was triggered 
	 *  internally to the driver. This way we run the same code path in all recovery 
	 *  cases
	 */
	bnx2i_do_iscsi_sess_recovery(conn->sess, DID_RESET, 0); /** Don't signal daemon again */

	/** Update state to */
	spin_lock_bh(&conn->sess->lock);
	conn->sess->recovery_state = ISCSI_SESS_RECOVERY_OPEN_ISCSI;
	spin_unlock_bh(&conn->sess->lock);

#else
	iscsi_block_session(session_to_cls(conn->sess));
#endif

	
#ifndef __VMKLNX__
	atomic_set(&conn->worker_enabled, 0);  
	scsi_flush_work(shost);
#endif

	switch (flag) {
	case STOP_CONN_RECOVER:
		conn->sess->state = BNX2I_SESS_IN_RECOVERY;
		if (!conn->sess->recovery_state) {	/* nopout timeout */

			spin_lock_bh(&conn->sess->lock);
			conn->sess->recovery_state =
				ISCSI_SESS_RECOVERY_OPEN_ISCSI;
			spin_unlock_bh(&conn->sess->lock);
		}
		break;
	case STOP_CONN_TERM:
		if (conn->sess && (conn->sess->state & BNX2I_SESS_IN_FFP)) {
			conn->sess->state = BNX2I_SESS_IN_SHUTDOWN;
		}
		break;
	default:
		PRINT_ERR(conn->sess->hba, "invalid conn stop req %d\n", flag);
	}

	if (use_poll_timer)
		del_timer_sync(&conn->poll_timer);

	/* Wait for TMF code to exit before returning to daemon */
	bnx2i_wait_for_tmf_completion(conn->sess);

#ifdef __VMKLNX__
	BNX2I_DBG(DBG_CONN_SETUP, conn->sess->hba,
		  "%s: conn %p request complete\n", __FUNCTION__, conn);
	return 0;
#else
	return;
#endif
}


/**
 * bnx2i_conn_send_pdu - iscsi transport callback entry point to send
 *			iscsi slow path pdus, such as LOGIN/LOGOUT/NOPOUT, etc
 *
 * @hba: 		pointer to adapter instance
 *
 * sends iSCSI PDUs prepared by user daemon, only login, logout, nop-out pdu
 *	will flow this path.
 */
#ifdef __VMKLNX__
vmk_int32 bnx2i_conn_send_pdu(struct iscsi_cls_conn *cls_conn,
			struct iscsi_hdr *hdr, vmk_int8 *data,
			vmk_uint32 data_size)
#else
int bnx2i_conn_send_pdu(struct iscsi_cls_conn *cls_conn,
			struct iscsi_hdr *hdr, char *data,
			uint32_t data_size)
#endif
{
	struct bnx2i_conn *conn;
	struct bnx2i_cmd *cmnd;
	uint32_t payload_size;
	int count;
#ifndef __VMKLNX__
	struct Scsi_Host *shost;
#endif

	if (!cls_conn) {
		printk(KERN_ALERT "bnx2i_conn_send_pdu: NULL conn ptr. \n");
		return -EIO;
	}
	conn = (struct bnx2i_conn *)cls_conn->dd_data;
	if (!conn->gen_pdu.req_buf) {
		PRINT_ALERT(conn->sess->hba,
			"send_pdu: login buf not allocated\n");
		/* ERR - buffer not allocated, should not happen */
		goto error;
	}

	/* If adapter is going down (mtu change, vlan, selftest, etc'),
	 * fail this pdu send request so that connection cleanup process
	 * can start right away. If the connection proceeds past this stage
	 * before cnic calls 'bnx2i->ulp_stop', will let this connection
	 * complete FFP migration and then make bnx2i_stop() trigger the cleanup
	 * process.
	 */
	if (!conn->sess)
		return -EIO;

	if (!conn->sess->hba)
		return -EIO;
	
	if (test_bit(ADAPTER_STATE_GOING_DOWN, &conn->sess->hba->adapter_state)) {
		conn->sess->hba->stop_event_ifc_abort_login++;
		goto error;
	}

	if (conn->state != CONN_STATE_XPORT_READY &&
	    conn->state != CONN_STATE_IN_LOGIN && 
	    (hdr->opcode & ISCSI_OPCODE_MASK) == ISCSI_OP_LOGIN) {
		/* login pdu request is valid in transport ready state */
		PRINT_ALERT(conn->sess->hba, "send_pdu: %d != XPORT_READY\n",
				  conn->state);
		goto error;
	}

	if (conn->sess->login_nopout_cmd) {
		cmnd = conn->sess->login_nopout_cmd;
	} else 		/* should not happen ever... */
		goto error;

		memset(conn->gen_pdu.req_buf, 0, ISCSI_CONN_LOGIN_BUF_SIZE);
		conn->gen_pdu.req_buf_size = data_size;

		cmnd->conn = conn;
		cmnd->scsi_cmd = NULL;

		ADD_STATS_64(conn->sess->hba, tx_pdus, 1);

		switch (hdr->opcode & ISCSI_OPCODE_MASK) {
		case ISCSI_OP_LOGIN:
			BNX2I_DBG(DBG_CONN_SETUP, conn->sess->hba,
				  "iscsi login send request, %p, payload %d\n",
				  conn, data_size);
			/* Login request, copy hdr & data to buffer in conn struct */
			memcpy(&conn->gen_pdu.pdu_hdr, (const void *) hdr,
				sizeof(struct iscsi_hdr));
			if (conn->state == CONN_STATE_XPORT_READY)
				conn->state = CONN_STATE_IN_LOGIN;
			payload_size = (hdr->dlength[0] << 16) | (hdr->dlength[1] << 8) |
					hdr->dlength[2];

			if (data_size) {
				memcpy(conn->gen_pdu.login_req.mem, (const void *)data,	
				       data_size);
				conn->gen_pdu.req_wr_ptr =
					conn->gen_pdu.req_buf + payload_size;
			}
			cmnd->iscsi_opcode = hdr->opcode;
			smp_mb();
			atomic_set(&conn->sess->login_noop_pending, 1);
			ADD_STATS_64(conn->sess->hba, tx_bytes, payload_size);		
			break;
		case ISCSI_OP_LOGOUT:
			BNX2I_DBG(DBG_CONN_SETUP, conn->sess->hba,
				  "%s: iscsi logout send request, conn %p\n",
				  __FUNCTION__, conn);
			/* Logout request, copy header only */
			memcpy(&conn->gen_pdu.pdu_hdr, (const void *) hdr,
				sizeof(struct iscsi_hdr));
			conn->gen_pdu.req_wr_ptr = conn->gen_pdu.req_buf;
			conn->state = CONN_STATE_IN_LOGOUT;
			conn->sess->state = BNX2I_SESS_IN_LOGOUT;
#ifdef __VMKLNX__
			iscsi_block_session(conn->sess->cls_sess);
#endif
			if (atomic_read(&conn->sess->tmf_active)) {
				/* This should never happen because conn_stop()
				 * will force TMF to fail, also it wait for
				 * 'tmf_active' to clear before returning.
				 */
				bnx2i_wait_for_tmf_completion(conn->sess);
			}

			/* Wait for any outstanding iscsi nopout to complete */
			count = 10;
			while (count-- && cmnd->iscsi_opcode)
				msleep(100);
			if (cmnd->iscsi_opcode)
				goto error;

			cmnd->iscsi_opcode = hdr->opcode;
			smp_mb();
			atomic_set(&conn->sess->logout_pending, 1);
			break;
		case ISCSI_OP_NOOP_OUT:
			conn->sess->last_nooput_requested = jiffies;
			conn->sess->noopout_requested_count++;
			/* connection is being logged out, do not allow NOOP */
			if (conn->state == CONN_STATE_IN_LOGOUT)
				goto error;

			/* unsolicited iSCSI NOOP copy hdr into conn struct */
			memcpy(&conn->gen_pdu.nopout_hdr, (const void *) hdr,
				sizeof(struct iscsi_hdr));
			cmnd->iscsi_opcode = hdr->opcode;
			cmnd->ttt = ISCSI_RESERVED_TAG;
			smp_mb();
			atomic_set(&conn->sess->login_noop_pending, 1);
			break;
		default:
			;
		}

	if (atomic_read(&conn->worker_enabled)) {
#ifdef __VMKLNX__
		atomic_set(&conn->lastSched,11);

		tasklet_schedule(&conn->conn_tasklet);
#else
		shost = bnx2i_conn_get_shost(conn);
		scsi_queue_work(shost, &conn->conn_worker);
#endif
	}
	return 0;
error:
	PRINT(conn->sess->hba, "%s: failed to send pdu!!\n", __FUNCTION__);
	return -EIO;
}


/**
 * bnx2i_conn_get_stats - returns iSCSI stats
 *
 * @cls_conn: 		pointer to iscsi cls conn
 * @stats: 		pointer to iscsi statistic struct
 */
void bnx2i_conn_get_stats(struct iscsi_cls_conn *cls_conn,
			  struct iscsi_stats *stats)
{
	struct bnx2i_conn *conn = (struct bnx2i_conn *) cls_conn->dd_data;

	stats->txdata_octets = conn->total_data_octets_sent;
	stats->rxdata_octets = conn->total_data_octets_rcvd;

	stats->noptx_pdus = conn->num_nopin_pdus;
	stats->scsicmd_pdus = conn->num_scsi_cmd_pdus;
	stats->tmfcmd_pdus = conn->num_tmf_req_pdus;
	stats->login_pdus = conn->num_login_req_pdus;
	stats->text_pdus = 0;
	stats->dataout_pdus = conn->num_dataout_pdus;
	stats->logout_pdus = conn->num_logout_req_pdus;
	stats->snack_pdus = 0;

	stats->noprx_pdus = conn->num_nopout_pdus;
	stats->scsirsp_pdus = conn->num_scsi_resp_pdus;
	stats->tmfrsp_pdus = conn->num_tmf_resp_pdus;
	stats->textrsp_pdus = 0;
	stats->datain_pdus = conn->num_datain_pdus;
	stats->logoutrsp_pdus = conn->num_logout_resp_pdus;
	stats->r2t_pdus = conn->num_r2t_pdus;
	stats->async_pdus = conn->num_async_pdus;
	stats->rjt_pdus = conn->num_reject_pdus;

	stats->digest_err = 0;
	stats->timeout_err = 0;
	stats->custom_length = 0;
}

#ifdef __VMKLNX__
#define TRANPORT_LIMIT_TYPE_LIST VMK_ISCSI_TRANPORT_LIMIT_TYPE_LIST
#define TRANPORT_LIMIT_TYPE_MINMAX VMK_ISCSI_TRANPORT_LIMIT_TYPE_MINMAX
#define TRANPORT_LIMIT_TYPE_UNSUPPORTED VMK_ISCSI_TRANPORT_LIMIT_TYPE_UNSUPPORTED

vmk_int32 bnx2i_get_570x_limit(struct iscsi_transport *transport,
	                       enum iscsi_param param,
                               vmk_IscsiTransTransportParamLimits *limit,
	                       vmk_int32 maxListLen)
{
	limit->param = param;
	switch(param) {
	case  ISCSI_PARAM_MAX_SESSIONS:
		limit->type = TRANPORT_LIMIT_TYPE_LIST;
		limit->hasPreferred = VMK_TRUE;
		limit->limit.list.count = 1;
		if (max_bnx2_sessions > 64) {
			limit->preferred = 64;
			limit->limit.list.value[0] = 64;
		} else {
			limit->preferred = max_bnx2_sessions;
			limit->limit.list.value[0] = max_bnx2_sessions;
		}
		break;
	case  ISCSI_PARAM_MAX_R2T:
		limit->type = TRANPORT_LIMIT_TYPE_MINMAX;
		limit->hasPreferred = VMK_TRUE;
		limit->preferred = 1;
		limit->limit.minMax.min = 1;
		limit->limit.minMax.max = 1;
		break;
	case  ISCSI_PARAM_MAX_BURST:
		limit->type = TRANPORT_LIMIT_TYPE_MINMAX;
		limit->hasPreferred = VMK_TRUE;
		limit->preferred = 262144;
		limit->limit.minMax.min = 8192;
		limit->limit.minMax.max = 16777215;
		break;
	case  ISCSI_PARAM_FIRST_BURST:
		limit->type = TRANPORT_LIMIT_TYPE_MINMAX;
		limit->hasPreferred = VMK_TRUE;
		limit->preferred = 262144;
		limit->limit.minMax.min = 8192;
		limit->limit.minMax.max = 16777215;
		break;
	case  ISCSI_PARAM_MAX_RECV_DLENGTH:
		limit->type = TRANPORT_LIMIT_TYPE_MINMAX;
		limit->hasPreferred = VMK_TRUE;
		limit->preferred = 262144;
		limit->limit.minMax.min = 8192;
		limit->limit.minMax.max = 16777215;
		break;
	default:
		limit->type = TRANPORT_LIMIT_TYPE_UNSUPPORTED;
		break;
	}
	return 0;
}

vmk_int32 bnx2i_get_5771x_limit(struct iscsi_transport *transport,
	                        enum iscsi_param param,
                               vmk_IscsiTransTransportParamLimits *limit,
	                       vmk_int32 maxListLen)
{
	limit->param = param;
	switch(param) {
	case  ISCSI_PARAM_MAX_SESSIONS:
		limit->type = TRANPORT_LIMIT_TYPE_LIST;
		limit->hasPreferred = VMK_TRUE;
		limit->limit.list.count = 1;
		if (max_bnx2x_sessions > 128) {
			limit->preferred = 128;
			limit->limit.list.value[0] = 128;
		} else {
			limit->preferred = max_bnx2x_sessions;
			limit->limit.list.value[0] = max_bnx2x_sessions;
		}
		break;
	case  ISCSI_PARAM_MAX_R2T:
		limit->type = TRANPORT_LIMIT_TYPE_MINMAX;
		limit->hasPreferred = VMK_TRUE;
		limit->preferred = 1;
		limit->limit.minMax.min = 1;
		limit->limit.minMax.max = 1;
		break;
	case  ISCSI_PARAM_MAX_BURST:
		limit->type = TRANPORT_LIMIT_TYPE_MINMAX;
		limit->hasPreferred = VMK_TRUE;
		limit->preferred = 262144;
		limit->limit.minMax.min = 8192;
		limit->limit.minMax.max = 16777215;
		break;
	case  ISCSI_PARAM_FIRST_BURST:
		limit->type = TRANPORT_LIMIT_TYPE_MINMAX;
		limit->hasPreferred = VMK_TRUE;
		limit->preferred = 262144;
		limit->limit.minMax.min = 8192;
		limit->limit.minMax.max = 16777215;
		break;
	case  ISCSI_PARAM_MAX_RECV_DLENGTH:
		limit->type = TRANPORT_LIMIT_TYPE_MINMAX;
		limit->hasPreferred = VMK_TRUE;
		limit->preferred = 262144;
		limit->limit.minMax.min = 8192;
		limit->limit.minMax.max = 16777215;
		break;
	default:
		limit->type = TRANPORT_LIMIT_TYPE_UNSUPPORTED;
		break;
	}
	return 0;
}
#endif



/**
 * bnx2i_check_nx2_dev_busy - this routine unregister devices if
 *			there are no active conns
 */
void bnx2i_check_nx2_dev_busy(void)
{
#if !defined(__VMKLNX__) || defined(_BNX2I_SEL_DEV_UNREG__)
	bnx2i_unreg_dev_all();
#endif
}


#ifdef __VMKLNX__
static struct bnx2i_hba *bnx2i_reg_chosen_device(vmk_IscsiNetHandle iscsi_hndl)
{
	char dev_name[IFNAMSIZ];
	struct net_device *netdev;
	struct bnx2i_hba *hba = NULL;

	if (vmk_IscsiTransportGetUplink(iscsi_hndl, dev_name) != VMK_OK)
		return NULL;

	if ((netdev = dev_get_by_name(dev_name)) == NULL)
		return NULL;

        dev_put(netdev);
	hba = bnx2i_map_netdev_to_hba(netdev);

	if (hba) {
		if (test_bit(ADAPTER_STATE_GOING_DOWN, &hba->adapter_state)) {
			printk("bnx2i: can't register %s, is being shutdown down.\n",
			       hba->netdev->name);
			hba->stop_event_ep_conn_failed++;
			hba = NULL;
		} else
			bnx2i_register_device(hba, BNX2I_REGISTER_HBA_SUPPORTED);
	}

	return hba;
}
#endif

/**
 * bnx2i_check_route - checks if target IP route belongs to one of
 *			NX2 devices
 *
 * @dst_addr: 		target IP address
 *
 * check if route resolves to BNX2 device
 */
#ifdef __VMKLNX__
static struct bnx2i_hba *bnx2i_check_route(struct sockaddr *dst_addr,
					vmk_IscsiNetHandle iscsiNetHandle)
#else
static struct bnx2i_hba *bnx2i_check_route(struct sockaddr *dst_addr)
#endif
{
	struct sockaddr_in *desti = (struct sockaddr_in *) dst_addr;
	struct bnx2i_hba *hba;
	struct cnic_dev *cnic = NULL;

#ifdef __VMKLNX__
	hba = bnx2i_reg_chosen_device(iscsiNetHandle);
#else
	bnx2i_reg_dev_all();

	hba = get_adapter_list_head();
#endif
        if (bnx2i_adapter_ready(hba)) {
                PRINT_ALERT(hba, "check route, hba not found\n");
                goto no_nx2_route;
        }

	if (hba && hba->cnic)
#ifdef __VMKLNX__
		cnic = hba->cnic->cm_select_dev(iscsiNetHandle, desti, CNIC_ULP_ISCSI);
#else
		cnic = hba->cnic->cm_select_dev(desti, CNIC_ULP_ISCSI);
#endif

	if (!cnic) {
		PRINT_ALERT(hba, "check route, can't connect using cnic\n");
		goto no_nx2_route;
	}
	hba = bnx2i_find_hba_for_cnic(cnic);
	if (!hba) {
		goto no_nx2_route;
	}

	if (hba->netdev->mtu > hba->mtu_supported) {
		PRINT_ALERT(hba, "%s network i/f mtu is set to %d\n",
				  hba->netdev->name, hba->netdev->mtu);
		PRINT_ALERT(hba, "iSCSI HBA can support mtu of %d\n",
				  hba->mtu_supported);
		goto no_nx2_route;
	}
	return hba;
no_nx2_route:
	return NULL;
}

/**
 * bnx2i_tear_down_ep - tear down endpoint and free resources
 *
 * @hba:                pointer to adapter instance
 * @ep:                 endpoint (transport indentifier) structure
 *
 * destroys cm_sock structure and on chip iscsi context
 */
static void bnx2i_tear_down_ep(struct bnx2i_hba *hba,
				 struct bnx2i_endpoint *ep)
{
	ep->state = EP_STATE_CLEANUP_START;
	init_timer(&ep->ofld_timer);
	ep->ofld_timer.expires = hba->conn_ctx_destroy_tmo + jiffies;
	ep->ofld_timer.function = bnx2i_ep_ofld_timer;
	ep->ofld_timer.data = (unsigned long) ep;
	add_timer(&ep->ofld_timer);

	bnx2i_ep_destroy_list_add(hba, ep);

	/* destroy iSCSI context, wait for it to complete */
	bnx2i_send_conn_destroy(hba, ep);
	wait_event_interruptible(ep->ofld_wait,
				 (ep->state != EP_STATE_CLEANUP_START));

	if (signal_pending(current))
		flush_signals(current);
	del_timer_sync(&ep->ofld_timer);
	bnx2i_ep_destroy_list_del(hba, ep);

	if (ep->state != EP_STATE_CLEANUP_CMPL)
		/* should never happen */
		PRINT_ALERT(hba, "conn destroy failed\n");
}

/**
 * bnx2i_tear_down_conn - tear down iscsi/tcp connection and free resources
 *
 * @hba: 		pointer to adapter instance
 * @ep: 		endpoint (transport indentifier) structure
 *
 * destroys cm_sock structure and on chip iscsi context
 */
static int bnx2i_tear_down_conn(struct bnx2i_hba *hba,
				 struct bnx2i_endpoint *ep)
{
	if (test_bit(BNX2I_CNIC_REGISTERED, &hba->reg_with_cnic) &&
		ep->state != EP_STATE_DISCONN_TIMEDOUT &&
		ep->cm_sk)
		hba->cnic->cm_destroy(ep->cm_sk);

	if (test_bit(ADAPTER_STATE_GOING_DOWN, &ep->hba->adapter_state))
		ep->state = EP_STATE_DISCONN_COMPL;

	if (test_bit(BNX2I_NX2_DEV_57710, &hba->cnic_dev_type) &&
	    ep->state == EP_STATE_DISCONN_TIMEDOUT) {
		PRINT_ALERT(hba, "####CID leaked %s: sess %p ep %p {0x%x, 0x%x}\n",
				__FUNCTION__, ep->sess, ep, 
				ep->ep_cid, ep->ep_iscsi_cid);
		
		ep->timestamp = jiffies;
		list_add_tail(&ep->link, &hba->ep_tmo_list);
		hba->ep_tmo_active_cnt++;
		if (!atomic_read(&hba->ep_tmo_poll_enabled)) {
			atomic_set(&hba->ep_tmo_poll_enabled, 1);
			schedule_work(&hba->ep_poll_task);
			PRINT_ALERT(hba, "%s: ep_tmo_poll_task is activated.\n",
				__FUNCTION__);
		}
		return 1;
	}
	bnx2i_tear_down_ep(hba, ep);
	return 0;
}


/**
 * bnx2i_ep_connect - establish TCP connection to target portal
 *
 * @dst_addr: 		target IP address
 * @non_blocking: 	blocking or non-blocking call
 * @ep_handle: 		placeholder to return new created  endpoint handle
 *
 * this routine initiates the TCP/IP connection by invoking Option-2 i/f
 *	with l5_core and the CNIC. This is a multi-step process of resolving
 *	route to target, create a iscsi connection context, handshaking with
 *	CNIC module to create/initialize the socket struct and finally
 *	sending down option-2 request to complete TCP 3-way handshake
 */
#ifdef __VMKLNX__
vmk_int32 bnx2i_ep_connect(struct sockaddr *dst_addr, vmk_int32 non_blocking,
		     vmk_uint64 *ep_handle, vmk_IscsiNetHandle iscsiNetHandle)
#else
int bnx2i_ep_connect(struct sockaddr *dst_addr, int non_blocking,
		     uint64_t *ep_handle)
#endif
{
	u32 iscsi_cid = BNX2I_CID_RESERVED;
	struct sockaddr_in *desti;
	struct sockaddr_in6 *desti6;
	struct bnx2i_endpoint *endpoint;
	struct bnx2i_hba *hba;
	struct cnic_dev *cnic;
	struct cnic_sockaddr saddr;
	int rc = 0;

	/* check if the given destination can be reached through NX2 device */
#ifdef __VMKLNX__
	hba = bnx2i_check_route(dst_addr, iscsiNetHandle);
#else
	hba = bnx2i_check_route(dst_addr);
#endif
	if (!hba) {
		rc = -ENOMEM;
		goto check_busy;
	}

	BNX2I_DBG(DBG_CONN_SETUP, hba, "%s: %s:: num active conns %d,"
		  " requested state %lx\n", __FUNCTION__,
		  hba->netdev->name, hba->ofld_conns_active,
		  hba->adapter_state);

	cnic = hba->cnic;
	endpoint = bnx2i_alloc_ep(hba);
	if (!endpoint) {
		*ep_handle = (uint64_t) 0;
		rc = -ENOMEM;
		goto check_busy;
	}

	mutex_lock(&hba->net_dev_lock);

	if (bnx2i_adapter_ready(hba)) {
		rc = -EPERM;
		goto net_if_down;
	}

	atomic_set(&endpoint->fp_kcqe_events, 1);
	endpoint->state = EP_STATE_IDLE;
	endpoint->teardown_mode = BNX2I_ABORTIVE_SHUTDOWN;
	endpoint->ep_iscsi_cid = (u16)ISCSI_RESERVED_TAG;
	iscsi_cid = bnx2i_alloc_iscsi_cid(hba);
	if (iscsi_cid == ISCSI_RESERVED_TAG) {
		PRINT_ALERT(hba, "alloc_ep: unable to allocate iscsi cid\n");
		rc = -ENOMEM;
		goto iscsi_cid_err;
	}
	endpoint->hba_age = hba->age;
	endpoint->ep_iscsi_cid = iscsi_cid & 0xFFFF;

	rc = bnx2i_alloc_qp_resc(hba, endpoint);
	if (rc != 0) {
		PRINT_ALERT(hba, "ep_conn, alloc QP resc error\n");
		rc = -ENOMEM;
		goto qp_resc_err;
	}

	endpoint->state = EP_STATE_OFLD_START;
	bnx2i_ep_ofld_list_add(hba, endpoint);

	init_timer(&endpoint->ofld_timer);
	endpoint->ofld_timer.expires = 2 * HZ + jiffies;
	endpoint->ofld_timer.function = bnx2i_ep_ofld_timer;
	endpoint->ofld_timer.data = (unsigned long) endpoint;
	add_timer(&endpoint->ofld_timer);

	if (bnx2i_send_conn_ofld_req(hba, endpoint)) {
		if (endpoint->state == EP_STATE_OFLD_FAILED_CID_BUSY) {                   
			printk("bnx2i:%s - %s: iscsi cid %d is busy\n",                   
			       __FUNCTION__, hba->netdev->name,                          
			       endpoint->ep_iscsi_cid);                                  
			rc = -EBUSY;                                                      
		} else                                                                    
			rc = -ENOSPC; 
		PRINT_ERR(hba, "unable to send conn offld kwqe\n");
		goto conn_failed;
	}

	/* Wait for CNIC hardware to setup conn context and return 'cid' */
	wait_event_interruptible(endpoint->ofld_wait,
				 endpoint->state != EP_STATE_OFLD_START);

	if (signal_pending(current))
		flush_signals(current);
	del_timer_sync(&endpoint->ofld_timer);
	bnx2i_ep_ofld_list_del(hba, endpoint);
	
	if (endpoint->state != EP_STATE_OFLD_COMPL) {
		if (endpoint->state == EP_STATE_OFLD_FAILED_CID_BUSY) {                   
			printk("bnx2i:%s - %s: iscsi cid %d is busy\n",                   
			       __FUNCTION__, hba->netdev->name,                          
			       endpoint->ep_iscsi_cid);                                  
			rc = -EBUSY;                                                      
		} else                                                                    
			rc = -ENOSPC;     
		goto conn_failed;
	}

	if (!test_bit(BNX2I_CNIC_REGISTERED, &hba->reg_with_cnic)) {
		rc = -EINVAL;
		goto conn_failed;
	} else
		rc = cnic->cm_create(cnic, CNIC_ULP_ISCSI, endpoint->ep_cid,
				     iscsi_cid, &endpoint->cm_sk, endpoint);
	if (rc) {
		rc = -EINVAL;
		goto release_ep;
	}
	if (!test_bit(BNX2I_NX2_DEV_57710, &hba->cnic_dev_type) && time_stamps)
		endpoint->cm_sk->tcp_flags |= SK_TCP_TIMESTAMP;

	endpoint->cm_sk->rcv_buf = tcp_buf_size;
	endpoint->cm_sk->snd_buf = tcp_buf_size;

	memset(&saddr, 0, sizeof(saddr));

	if (dst_addr->sa_family == AF_INET) {
		desti = (struct sockaddr_in *) dst_addr;
		saddr.remote.v4 = *desti;
		saddr.local.v4.sin_port = htons(endpoint->tcp_port);
		saddr.local.v4.sin_family = desti->sin_family;
	} else if (dst_addr->sa_family == AF_INET6) {
		desti6 = (struct sockaddr_in6 *) dst_addr;
		saddr.remote.v6 = *desti6;
		saddr.local.v6.sin6_port = htons(endpoint->tcp_port);
		saddr.local.v6.sin6_family = desti6->sin6_family;
	}

	endpoint->timestamp = jiffies;
	endpoint->state = EP_STATE_CONNECT_START;
	if (!test_bit(BNX2I_CNIC_REGISTERED, &hba->reg_with_cnic)) {
		rc = -EINVAL;
		goto conn_failed;
	} else {
		init_timer(&endpoint->ofld_timer);
		endpoint->ofld_timer.expires = 2 * HZ + jiffies;
		endpoint->ofld_timer.function = bnx2i_ep_ofld_timer;
		endpoint->ofld_timer.data = (unsigned long) endpoint;
		add_timer(&endpoint->ofld_timer);
                rc = cnic->cm_connect(endpoint->cm_sk, &saddr);
	}

	if (rc)
		goto release_ep;
	/* Wait for CNIC hardware to setup conn context and return 'cid' */
	wait_event_interruptible(endpoint->ofld_wait,
			endpoint->state != EP_STATE_CONNECT_START);

	if (signal_pending(current))
		flush_signals(current);
	del_timer_sync(&endpoint->ofld_timer);

	if (endpoint->state != EP_STATE_CONNECT_COMPL) {
		printk("bnx2i:%s:%d - %s: connect failed for iscsi cid %d\n",
				__func__, __LINE__, hba->netdev->name,
				endpoint->ep_iscsi_cid);
		rc = -ENOSPC;
		goto release_ep;
	}

	rc = bnx2i_map_ep_dbell_regs(endpoint);
	if (rc)
		goto release_ep;

	*ep_handle = (uint64_t) (unsigned long) endpoint;
	mutex_unlock(&hba->net_dev_lock);

	BNX2I_DBG(DBG_CONN_SETUP, hba, "%s: ep %p, created successfully\n",
		  __FUNCTION__, endpoint);
	/*
	 * unregister idle devices, without this user can't uninstall
	 * unused bnx2/bnx2x driver because registration will increment
	 * the usage count
	 */
#if defined(_BNX2I_SEL_DEV_UNREG__)
	if (test_bit(ADAPTER_STATE_GOING_DOWN, &hba->adapter_state))
#endif
		bnx2i_check_nx2_dev_busy();

	return 0;

release_ep:
	bnx2i_tear_down_conn(hba, endpoint);
conn_failed:
net_if_down:
iscsi_cid_err:
	bnx2i_free_qp_resc(hba, endpoint);
qp_resc_err:
	bnx2i_free_ep(endpoint);
	mutex_unlock(&hba->net_dev_lock);
check_busy:
	*ep_handle = (uint64_t) 0;
	bnx2i_check_nx2_dev_busy();
	return rc;
}

/**
 * bnx2i_ep_poll - polls for TCP connection establishement
 *
 * @ep_handle: 		TCP connection (endpoint) handle
 * @timeout_ms: 	timeout value in milli secs
 *
 * polls for TCP connect request to complete. Based of the connection
 *     state following status code will be returned,
 *     -1 = failed, 0 = timeout, 1 = successful
 */
#ifdef __VMKLNX__
vmk_int32 bnx2i_ep_poll(vmk_uint64 ep_handle, vmk_int32 timeout_ms)
#else
int bnx2i_ep_poll(uint64_t ep_handle, int timeout_ms)
#endif
{
	struct bnx2i_endpoint *ep;
	int rc = 0;

	ep = (struct bnx2i_endpoint *) (unsigned long) ep_handle;
	if (!ep || (ep_handle == -1))
		return -1;

	if ((ep->state == EP_STATE_IDLE) ||
	    (ep->state == EP_STATE_CONNECT_FAILED) ||
	    (ep->state == EP_STATE_OFLD_FAILED))
		return -1;

	if (ep->state == EP_STATE_CONNECT_COMPL) {
		if (test_bit(ADAPTER_STATE_GOING_DOWN, &ep->hba->adapter_state)) {
			/* Eventhough option2 connect is successful pretend
			 * as if TCP 3-way handshake failed, This will immediately
			 * trigger the connection context cleanup so that the adapter
			 * can be shutdown quickly.
			 */
			ep->hba->stop_event_ifc_abort_poll++;
			return -1;
		} else
			return 1;
	}

	rc = wait_event_interruptible_timeout(ep->ofld_wait,
					      ((ep->state ==
					        EP_STATE_OFLD_FAILED) ||
					       (ep->state ==
					         EP_STATE_CONNECT_FAILED) ||
					       (ep->state ==
					        EP_STATE_CONNECT_COMPL)),
					       msecs_to_jiffies(timeout_ms));
	if (ep->state == EP_STATE_OFLD_FAILED)
		rc = -1;

	if (rc > 0) {
		if (test_bit(ADAPTER_STATE_GOING_DOWN, &ep->hba->adapter_state)) {
			/* Eventhough option2 connect is successful pretend
			 * as if TCP 3-way handshake failed, This will immediately
			 * trigger the connection context cleanup so that the adapter
			 * can be shutdown quickly.
			 */
			ep->hba->stop_event_ifc_abort_poll++;
			return -1;
		} else
			return 1;
	}
	else if (!rc)
		return 0;	/* timeout */
	else
		return rc;
}

/**
 * bnx2i_ep_tcp_conn_active - check EP state transition to check
 *		if underlying TCP connection is active
 *
 * @ep: 		endpoint pointer
 *
 */
static int bnx2i_ep_tcp_conn_active(struct bnx2i_endpoint *ep)
{
	int ret;

	switch (ep->state) {
	case EP_STATE_CLEANUP_FAILED:
	case EP_STATE_OFLD_FAILED:
	case EP_STATE_DISCONN_TIMEDOUT:
		ret = 0;
		break;
	case EP_STATE_CONNECT_COMPL:
	case EP_STATE_ULP_UPDATE_START:
	case EP_STATE_ULP_UPDATE_COMPL:
	case EP_STATE_TCP_FIN_RCVD:
	case EP_STATE_ULP_UPDATE_FAILED:
	case EP_STATE_CONNECT_FAILED:
	case EP_STATE_TCP_RST_RCVD:
		/* cnic need to upload PG for 570x chipsets and there is
		 * an understanding it is safe to call cm_abort() even if
		 * cm_connect() failed for all chip types
		 */
		ret = 1;
		break;
	case EP_STATE_CONNECT_START:
		/* bnx2i will not know whether PG needs to be uploaded or not.
		 * bnx2i will call cm_abort() and let cnic decide the clean-up
		 * action that needs to be taken
		 */
		ret = 1;
		break;
	default:
		ret = 0;
	}

	return ret;
}

/**
 * bnx2i_ep_disconnect - executes TCP connection teardown process
 *
 * @ep_handle: 		TCP connection (endpoint) handle
 *
 * executes  TCP connection teardown process
 */
#ifdef __VMKLNX__
void bnx2i_ep_disconnect(vmk_int64 ep_handle)
#else
void bnx2i_ep_disconnect(uint64_t ep_handle)
#endif
{
	struct bnx2i_endpoint *ep;
	struct cnic_dev *cnic;
	struct bnx2i_hba *hba;
	struct bnx2i_sess *sess;
	int rc;

	ep = (struct bnx2i_endpoint *) (unsigned long) ep_handle;
	if (!ep || (ep_handle == -1))
		return;

	if (ep->in_progress == 0)
		return;

        while ((ep->state == EP_STATE_CONNECT_START) &&
		!time_after(jiffies, ep->timestamp + (12 * HZ)))
                msleep(250);

	/** This could be a bug, we always disable here, but we enable in BIND
	 *  so we should ONLY ever disable if the ep was assigned a conn ( AKA BIND was run )
	 *  Another case is that conn_destroy got called, which sets the ep->conn to NULL 
	 *  which is ok since destroy stops the tasklet
	 */
	if (ep->conn) {
		atomic_set(&ep->conn->worker_enabled, 0);
		tasklet_kill(&ep->conn->conn_tasklet);
	}

	hba = ep->hba;
	if (ep->state == EP_STATE_IDLE)
		goto return_ep;
	cnic = hba->cnic;

	PRINT(hba, "%s: %s: disconnecting ep %p {%d, %x}, conn %p, sess %p, "
	      "hba-state %lx, num active conns %d\n", __FUNCTION__,
	      hba->netdev->name, ep, ep->ep_iscsi_cid, ep->ep_cid,
	      ep->conn, ep->sess, hba->adapter_state,
	      hba->ofld_conns_active);

	mutex_lock(&hba->net_dev_lock);
	if (!test_bit(ADAPTER_STATE_UP, &hba->adapter_state))
		goto free_resc;
	if (ep->hba_age != hba->age)
		goto dev_reset;

	if (!bnx2i_ep_tcp_conn_active(ep))
		goto destory_conn;

	ep->state = EP_STATE_DISCONN_START;

	init_timer(&ep->ofld_timer);
	ep->ofld_timer.expires = hba->conn_teardown_tmo + jiffies;
	ep->ofld_timer.function = bnx2i_ep_ofld_timer;
	ep->ofld_timer.data = (unsigned long) ep;
	add_timer(&ep->ofld_timer);

	if (!test_bit(BNX2I_CNIC_REGISTERED, &hba->reg_with_cnic))
		goto free_resc;

	if (ep->teardown_mode == BNX2I_GRACEFUL_SHUTDOWN)
		rc = cnic->cm_close(ep->cm_sk);
	else
		rc = cnic->cm_abort(ep->cm_sk);

	if (rc == -EALREADY)
		ep->state = EP_STATE_DISCONN_COMPL;

	/* wait for option-2 conn teardown */
	wait_event_interruptible(ep->ofld_wait,
				 ((ep->state != EP_STATE_DISCONN_START)
				 && (ep->state != EP_STATE_TCP_FIN_RCVD)));

	BNX2I_DBG(DBG_CONN_SETUP, hba, "After wait, ep->state:0x%x \n",
			ep->state);

	if (signal_pending(current))
		flush_signals(current);
	del_timer_sync(&ep->ofld_timer);

destory_conn:
	if (bnx2i_tear_down_conn(hba, ep)) {
		mutex_unlock(&hba->net_dev_lock);
		return;
	}

dev_reset:
free_resc:
	/* in case of 3-way handshake failure, there won't be any binding
	 * between EP and SESS
	 */
	if (ep->sess) {
		int cmds_a = 0, cmds_p = 0;
		cmds_p = bnx2i_flush_pend_queue(ep->sess, NULL, DID_RESET);
		cmds_a = bnx2i_flush_cmd_queue(ep->sess, NULL, DID_RESET, 0);
		BNX2I_DBG(DBG_ITT_CLEANUP, hba, "%s: CMDS FLUSHED, pend=%d, active=%d\n",
			  __FUNCTION__, cmds_p, cmds_a);
	}

	mutex_unlock(&hba->net_dev_lock);
	bnx2i_free_qp_resc(hba, ep);
return_ep:
	/* check if session recovery in progress */
	sess = ep->sess;

	bnx2i_free_ep(ep);
	if (sess) {
		sess->state = BNX2I_SESS_INITIAL;
		wake_up(&sess->er_wait);
	}
	wake_up_interruptible(&hba->eh_wait);

	if (!hba->ofld_conns_active)
		bnx2i_check_nx2_dev_busy();

	BNX2I_DBG(DBG_CONN_SETUP, hba, "bnx2i: ep %p, destroyed\n", ep);
	return;
}


#ifndef __VMKLNX__
int bnx2i_check_ioctl_signature(struct bnx2i_ioctl_header *ioc_hdr)
{
	if (strcmp(ioc_hdr->signature, BNX2I_MGMT_SIGNATURE))
		return -EPERM;
	return 0;
}

static int bnx2i_tcp_port_count_ioctl(struct file *file, unsigned long arg)
{
	struct bnx2i_get_port_count __user *user_ioc =
		(struct bnx2i_get_port_count __user *)arg;
	struct bnx2i_get_port_count ioc_req;
	int error = 0;
	unsigned int count = 0;

	if (copy_from_user(&ioc_req, user_ioc, sizeof(ioc_req))) {
		error = -EFAULT;
		goto out;
	}

	error = bnx2i_check_ioctl_signature(&ioc_req.hdr);
	if (error)
		goto out;

	if (bnx2i_tcp_port_tbl.num_free_ports < 10 &&
	    bnx2i_tcp_port_tbl.num_required) {
		if (bnx2i_tcp_port_tbl.num_required < 32)
			count = bnx2i_tcp_port_tbl.num_required;
		else
			count = 32;
	}

	ioc_req.port_count = count;

	if (copy_to_user(&user_ioc->port_count, &ioc_req.port_count,
			 sizeof(ioc_req.port_count))) {
		error = -EFAULT;
		goto out;
	}

out:
	return error;
}


static int bnx2i_tcp_port_ioctl(struct file *file, unsigned long arg)
{
	struct bnx2i_set_port_num __user *user_ioc =
		(struct bnx2i_set_port_num __user *)arg;
	struct bnx2i_set_port_num ioc_req;
	struct bnx2i_set_port_num *ioc_req_mp = NULL;
	int ioc_msg_size = sizeof(ioc_req);
	int error;
	int i;

	if (copy_from_user(&ioc_req, user_ioc, ioc_msg_size)) {
		error = -EFAULT;
		goto out;
	}

	error = bnx2i_check_ioctl_signature(&ioc_req.hdr);
	if (error)
		goto out;

	if (ioc_req.num_ports > 1) {
		ioc_msg_size += (ioc_req.num_ports - 1) *
				sizeof(ioc_req.tcp_port[0]);

		ioc_req_mp = kmalloc(ioc_msg_size, GFP_KERNEL);
		if (!ioc_req_mp)
			goto out;

		if (copy_from_user(ioc_req_mp, user_ioc, ioc_msg_size)) {
			error = -EFAULT;
			goto out_kfree;
		}
	}

	if (ioc_req.num_ports)
		bnx2i_tcp_port_new_entry(ioc_req.tcp_port[0]);

	i = 1;
	while (i < ioc_req_mp->num_ports)
		bnx2i_tcp_port_new_entry(ioc_req_mp->tcp_port[i++]);

	return 0;

out_kfree:
	kfree(ioc_req_mp);
out:
	return error;
}


/*
 * bnx2i_ioctl_init: initialization routine, registers char driver
 */
int bnx2i_ioctl_init(void)
{
	int ret;

        /* Register char device node */
        ret = register_chrdev(0, "bnx2i", &bnx2i_mgmt_fops);

        if (ret < 0) {
                printk(KERN_ERR "bnx2i: failed to register device node\n");
                return ret;
        }

        bnx2i_major_no = ret;

	return 0;
}

void bnx2i_ioctl_cleanup(void)
{
	if (bnx2i_major_no) {
		unregister_chrdev(bnx2i_major_no, "bnx2i");
	}
}

/*
 * bnx2i_mgmt_open -  "open" entry point
 */
static int bnx2i_mgmt_open(struct inode *inode, struct file *filep)
{
        /* only allow access to admin user */
        if (!capable(CAP_SYS_ADMIN)) {
                return -EACCES;
	}

        return 0;
}

/*
 * bnx2i_mgmt_release- "release" entry point
 */
static int bnx2i_mgmt_release(struct inode *inode, struct file *filep)
{
        return 0;
}



/*
 * bnx2i_mgmt_ioctl - char driver ioctl entry point
 */
static int bnx2i_mgmt_ioctl(struct inode *node, struct file *file,
			    unsigned int cmd, unsigned long arg)
{
	long rc = 0;
	switch (cmd) {
		case BNX2I_IOCTL_GET_PORT_REQ:
			rc = bnx2i_tcp_port_count_ioctl(file, arg);
			break;
		case BNX2I_IOCTL_SET_TCP_PORT:
			rc = bnx2i_tcp_port_ioctl(file, arg);
			break;
		default:
			printk(KERN_ERR "bnx2i: unknown ioctl cmd %x\n", cmd);
			return -ENOTTY;
	}

	return rc;
}


#ifdef CONFIG_COMPAT

static int bnx2i_tcp_port_count_compat_ioctl(struct file *file, unsigned long arg)
{
	struct bnx2i_get_port_count __user *user_ioc =
		(struct bnx2i_get_port_count __user *)arg;
	struct bnx2i_get_port_count *ioc_req =
		compat_alloc_user_space(sizeof(struct bnx2i_get_port_count));
	int error;
	unsigned int count = 0;

	if (clear_user(ioc_req, sizeof(*ioc_req)))
		return -EFAULT;

	if (copy_in_user(ioc_req, user_ioc, sizeof(*ioc_req))) {
		error = -EFAULT;
		goto out;
	}

	error = bnx2i_check_ioctl_signature(&ioc_req->hdr);
	if (error)
		goto out;

	if (bnx2i_tcp_port_tbl.num_free_ports < 10 &&
	    bnx2i_tcp_port_tbl.num_required) {
		if (bnx2i_tcp_port_tbl.num_required < 32)
			count = bnx2i_tcp_port_tbl.num_required;
		else
			count = 32;
	}

	if (copy_to_user(&ioc_req->port_count, &count,
			 sizeof(ioc_req->port_count))) {
		error = -EFAULT;
		goto out;
	}

	if (copy_in_user(&user_ioc->port_count, &ioc_req->port_count,
			 sizeof(u32))) {
		error = -EFAULT;
		goto out;
	}
	return 0;

out:
	return error;
}

static int bnx2i_tcp_port_compat_ioctl(struct file *file, unsigned long arg)
{
	struct bnx2i_set_port_num __user *user_ioc =
		(struct bnx2i_set_port_num __user *)arg;
	struct bnx2i_set_port_num *ioc_req =
		compat_alloc_user_space(sizeof(struct bnx2i_set_port_num));
	struct bnx2i_set_port_num *ioc_req_mp = NULL;
	int ioc_msg_size = sizeof(*ioc_req);
	int error;
	int i;

	if (clear_user(ioc_req, sizeof(*ioc_req)))
		return -EFAULT;

	if (copy_in_user(ioc_req, user_ioc, ioc_msg_size)) {
		error = -EFAULT;
		goto out;
	}

	error = bnx2i_check_ioctl_signature(&ioc_req->hdr);
	if (error)
		goto out;

	if (ioc_req->num_ports > 1) {
		ioc_msg_size += (ioc_req->num_ports - 1) *
				sizeof(ioc_req->tcp_port[0]);

		ioc_req_mp = compat_alloc_user_space(ioc_msg_size);
		if (!ioc_req_mp)
			goto out;

		if (copy_in_user(ioc_req_mp, user_ioc, ioc_msg_size)) {
			error = -EFAULT;
			goto out;
		}

		i = 0;
		while ((i < ioc_req_mp->num_ports) && ioc_req_mp)
			bnx2i_tcp_port_new_entry(ioc_req_mp->tcp_port[i++]);

	} else if (ioc_req->num_ports == 1)
		bnx2i_tcp_port_new_entry(ioc_req->tcp_port[0]);

out:
	return error;


}


/*
 * bnx2i_mgmt_compat_ioctl - char node ioctl entry point
 */
static long bnx2i_mgmt_compat_ioctl(struct file *file,
				    unsigned int cmd, unsigned long arg)
{
	int rc = -ENOTTY;

	switch (cmd) {
		case BNX2I_IOCTL_GET_PORT_REQ:
			rc = bnx2i_tcp_port_count_compat_ioctl(file, arg);
			break;
		case BNX2I_IOCTL_SET_TCP_PORT:
			rc = bnx2i_tcp_port_compat_ioctl(file, arg);
			break;
	}

        return rc;
}

#endif

/*
 * File operations structure - management interface
 */
struct file_operations bnx2i_mgmt_fops = {
        .owner = THIS_MODULE,
        .open = bnx2i_mgmt_open,
        .release = bnx2i_mgmt_release,
        .ioctl = bnx2i_mgmt_ioctl,
#ifdef CONFIG_COMPAT
        .compat_ioctl = bnx2i_mgmt_compat_ioctl,
#endif
};
#endif


/*
 * 'Scsi_Host_Template' structure and 'iscsi_tranport' structure template
 * used while registering with the iSCSI transport module.
 */
struct scsi_host_template bnx2i_host_template = {
	.module				= THIS_MODULE,
#ifdef __VMKLNX__
	.name				= "bnx2i",
#else
	.name				= "QLogic NetXtreme II Offload iSCSI Initiator",
#endif
	.queuecommand			= bnx2i_queuecommand,
	.eh_abort_handler		= bnx2i_abort,
	.eh_host_reset_handler		= bnx2i_host_reset,
	.bios_param			= NULL,
#ifdef __VMKLNX__
	.eh_device_reset_handler	= bnx2i_device_reset,
	.slave_configure		= bnx2i_slave_configure,
	.slave_alloc			= bnx2i_slave_alloc,
   	.target_alloc			= bnx2i_target_alloc,
   	.target_destroy			= bnx2i_target_destroy,
	.can_queue			= 1024,
#else
	.can_queue			= 128,
#endif
	.max_sectors			= 256,
	.this_id			= -1,
	.cmd_per_lun			= 64,
	.use_clustering			= ENABLE_CLUSTERING,
	.sg_tablesize			= ISCSI_MAX_BDS_PER_CMD,
#ifdef __VMKLNX__
	.proc_name			= "bnx2i",
	.proc_info			= bnx2i_proc_info
#else
	.proc_name			= NULL
#endif
	};



struct iscsi_transport bnx2i_iscsi_transport = {
	.owner			= THIS_MODULE,
	.name			= "bnx2i",
#ifdef __VMKLNX__
	.description		= "QLogic NetXtreme II iSCSI Adapter",
#endif
	.caps			= CAP_RECOVERY_L0 | CAP_HDRDGST | CAP_MULTI_R2T
#ifdef __VMKLNX__
				  | CAP_KERNEL_POLL | CAP_SESSION_PERSISTENT
#endif
				  | CAP_DATADGST,
	.param_mask		= ISCSI_MAX_RECV_DLENGTH |
				  ISCSI_MAX_XMIT_DLENGTH |
				  ISCSI_HDRDGST_EN |
				  ISCSI_DATADGST_EN |
				  ISCSI_INITIAL_R2T_EN |
				  ISCSI_MAX_R2T |
				  ISCSI_IMM_DATA_EN |
				  ISCSI_FIRST_BURST |
				  ISCSI_MAX_BURST |
				  ISCSI_PDU_INORDER_EN |
				  ISCSI_DATASEQ_INORDER_EN |
				  ISCSI_ERL |
				  ISCSI_CONN_PORT |
				  ISCSI_CONN_ADDRESS |
				  ISCSI_EXP_STATSN |
				  ISCSI_PERSISTENT_PORT |
				  ISCSI_PERSISTENT_ADDRESS |
				  ISCSI_TARGET_NAME |
#if defined(__VMKLNX__) && (VMWARE_ESX_DDK_VERSION >= 50000)
				  ISCSI_TPGT |
				  ISCSI_ISID,
#else
				  ISCSI_TPGT,
#endif
	.host_template		= &bnx2i_host_template,
	.sessiondata_size	= sizeof(struct bnx2i_sess),
	.conndata_size		= sizeof(struct bnx2i_conn),
	.max_conn		= 1,
	.max_cmd_len		= 16,
	.max_lun		= 512,
#ifdef __VMKLNX__
	.create_session_persistent = bnx2i_session_create_vmp,
	.create_session		= bnx2i_session_create_vm,
#else
	.create_session		= bnx2i_session_create,
#endif
	.destroy_session	= bnx2i_session_destroy,
	.create_conn		= bnx2i_conn_create,
	.bind_conn		= bnx2i_conn_bind,
	.destroy_conn		= bnx2i_conn_destroy,
	.set_param		= bnx2i_conn_set_param,
	.get_conn_param		= bnx2i_conn_get_param,
	.get_session_param	= bnx2i_session_get_param,
	.start_conn		= bnx2i_conn_start,
	.stop_conn		= bnx2i_conn_stop,
	.send_pdu		= bnx2i_conn_send_pdu,
	.get_stats		= bnx2i_conn_get_stats,
#ifdef __VMKLNX__
	/* TCP connect - disconnect - option-2 interface calls */
	.ep_connect		= NULL,
	.ep_connect_extended	= bnx2i_ep_connect,
#else
	.ep_connect		= bnx2i_ep_connect,
#endif
	.ep_poll		= bnx2i_ep_poll,
	.ep_disconnect		= bnx2i_ep_disconnect,
	/* Error recovery timeout call */
	.session_recovery_timedout = bnx2i_sess_recovery_timeo
};

static void ep_tmo_cleanup(struct bnx2i_hba *hba, struct bnx2i_endpoint *ep)
{
	struct bnx2i_sess *sess;
	
	hba->ep_tmo_cmpl_cnt++;
	/* remove the recovered EP from the link list */
	list_del_init(&ep->link);
		
	bnx2i_tear_down_ep(hba, ep);
	if (ep->sess) {
		int cmds_a = 0, cmds_p = 0;
		cmds_p = bnx2i_flush_pend_queue(ep->sess, NULL, DID_RESET);
		cmds_a = bnx2i_flush_cmd_queue(ep->sess, NULL, DID_RESET, 0);
		BNX2I_DBG(DBG_ITT_CLEANUP, hba, 
			"%s: CMDS FLUSHED, pend=%d, active=%d\n",
			__FUNCTION__, cmds_p, cmds_a);
	}

	BNX2I_DBG(DBG_CONN_SETUP, hba, "bnx2i: ep %p, destroyed\n", ep);
	PRINT_ALERT(hba, "####CID recovered %s: sess %p ep %p {0x%x, 0x%x}\n",
		__FUNCTION__, ep->sess, ep, ep->ep_cid, ep->ep_iscsi_cid);
	bnx2i_free_qp_resc(hba, ep);
	/* check if session recovery in progress */
	sess = ep->sess;
	bnx2i_free_ep(ep);
	if (sess) {
		sess->state = BNX2I_SESS_INITIAL;
		wake_up(&sess->er_wait);
	}
	wake_up_interruptible(&hba->eh_wait);

	if (!hba->ofld_conns_active)
		bnx2i_check_nx2_dev_busy();
} 

/**
 * ep_tmo_poll_task - check endpoints that are in disconnect timeout state and
 *                    cleanup the endpoint if the chip has acknowledged the 
 *                    disconnect. 
 *
 * @work:		pointer to work struct
 *
 */
static void
#if defined(INIT_DELAYED_WORK_DEFERRABLE) || defined(INIT_WORK_NAR) || (defined(__VMKLNX__) && (VMWARE_ESX_DDK_VERSION >= 50000))
ep_tmo_poll_task(struct work_struct *work)
#else
ep_tmo_poll_task(void *data)
#endif
{
#if defined(INIT_DELAYED_WORK_DEFERRABLE) || defined(INIT_WORK_NAR) || (defined(__VMKLNX__) && (VMWARE_ESX_DDK_VERSION >= 50000))
	struct bnx2i_hba *hba = container_of(work, struct bnx2i_hba,
					     ep_poll_task);
#else
	struct bnx2i_hba *hba = data;
#endif
	struct bnx2i_endpoint *ep, *ep_n;

	mutex_lock(&hba->net_dev_lock);
	list_for_each_entry_safe(ep, ep_n, &hba->ep_stale_list, link) {
		if (test_bit(ADAPTER_STATE_GOING_DOWN, &hba->adapter_state))
			ep->state = EP_STATE_DISCONN_COMPL;
		if (ep->state != EP_STATE_DISCONN_TIMEDOUT) {
			ep_tmo_cleanup(hba, ep);
			break;
		}
	}
	list_for_each_entry_safe(ep, ep_n, &hba->ep_tmo_list, link) {
		if (test_bit(ADAPTER_STATE_GOING_DOWN, &hba->adapter_state))
			ep->state = EP_STATE_DISCONN_COMPL;
		if (ep->state != EP_STATE_DISCONN_TIMEDOUT) {
			ep_tmo_cleanup(hba, ep);
			break;
		} else if (time_after(jiffies, ep->timestamp + 60*HZ)) {
			PRINT_ALERT(hba, 
				"%s: please submit GRC Dump (ep %p {0x%x, 0x%x}),"
				" NW/PCIe trace, driver msgs to developers"
				" for analysis\n",
				__FUNCTION__, ep, ep->ep_cid, ep->ep_iscsi_cid);
			list_del_init(&ep->link);
			list_add_tail(&ep->link, &hba->ep_stale_list);
			
			spin_lock_bh(&bnx2i_resc_lock);
			ep->hba->ofld_conns_active--;
			ep->in_progress = 0;
			spin_unlock_bh(&bnx2i_resc_lock);
		}
	}
	
	if (list_empty(&hba->ep_tmo_list) && list_empty(&hba->ep_stale_list)) {
		atomic_set(&hba->ep_tmo_poll_enabled, 0);
		PRINT_ALERT(hba, "%s: ep_tmo_list and ep_stale_list is now empty and"
			" ep_poll_task is terminated!\n",
			__FUNCTION__);
	}
	mutex_unlock(&hba->net_dev_lock);
	
	if (atomic_read(&hba->ep_tmo_poll_enabled)) {
		wait_event_interruptible_timeout(
			hba->ep_tmo_wait,
			(test_bit(ADAPTER_STATE_GOING_DOWN, &hba->adapter_state) > 0),
			msecs_to_jiffies(1000));
		schedule_work(&hba->ep_poll_task);
	}
}
