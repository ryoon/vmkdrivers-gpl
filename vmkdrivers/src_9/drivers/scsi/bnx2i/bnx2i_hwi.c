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

extern unsigned int event_coal_min;
extern unsigned int time_stamps;
static void bnx2i_recovery_que_add_sess(struct bnx2i_hba *hba,
					struct bnx2i_sess *sess);

/**
 * bnx2i_get_cid_num -
 * @ep: 	endpoint pointer
 *
 * Only applicable to 57710 family of devices
 **/
static u32 bnx2i_get_cid_num(struct bnx2i_endpoint *ep)
{
	u32 cid;

	if (test_bit(BNX2I_NX2_DEV_57710, &ep->hba->cnic_dev_type))
		cid = ep->ep_cid;
	else
		cid = GET_CID_NUM(ep->ep_cid);
	return cid;
}


/**
 * bnx2i_adjust_qp_size - Adjust SQ/RQ/CQ size for 57710 device type
 * @hba: 		Adapter for which adjustments is to be made
 *
 * Only applicable to 57710 family of devices
 **/
static void bnx2i_adjust_qp_size(struct bnx2i_hba *hba)
{
	u32 num_elements_per_pg;

	/* Only 5771x family requires SQ/CQ to be integral number of pages */
	if (test_bit(BNX2I_NX2_DEV_5706, &hba->cnic_dev_type) ||
	    test_bit(BNX2I_NX2_DEV_5708, &hba->cnic_dev_type) ||
	    test_bit(BNX2I_NX2_DEV_5709, &hba->cnic_dev_type))
		return;

	/* Adjust each queue size if the user selection does not
	 * yield integral num of page buffers
	 */
	/* adjust SQ */
	num_elements_per_pg = PAGE_SIZE / BNX2I_SQ_WQE_SIZE;
	if (hba->max_sqes < num_elements_per_pg)
		hba->max_sqes = num_elements_per_pg;
	else if (hba->max_sqes % num_elements_per_pg)
		hba->max_sqes = (hba->max_sqes + num_elements_per_pg - 1) &
				 ~(num_elements_per_pg - 1);

	/* adjust CQ */
	num_elements_per_pg = PAGE_SIZE / BNX2I_CQE_SIZE;
	if (hba->max_cqes < num_elements_per_pg)
		hba->max_cqes = num_elements_per_pg;
	else if (hba->max_cqes % num_elements_per_pg)
		hba->max_cqes = (hba->max_cqes + num_elements_per_pg - 1) &
				 ~(num_elements_per_pg - 1);

	/* adjust RQ */
	num_elements_per_pg = PAGE_SIZE / BNX2I_RQ_WQE_SIZE;
	if (hba->max_rqes < num_elements_per_pg)
		hba->max_rqes = num_elements_per_pg;
	else if (hba->max_rqes % num_elements_per_pg)
		hba->max_rqes = (hba->max_rqes + num_elements_per_pg - 1) &
				 ~(num_elements_per_pg - 1);
}


/**
 * bnx2i_get_link_state - get network interface link state
 * @hba: 		adapter instance pointer
 *
 * updates adapter structure flag based on netdev state
 **/
void bnx2i_get_link_state(struct bnx2i_hba *hba)
{
       	if (test_bit(__LINK_STATE_NOCARRIER, &hba->netdev->state)) {
		set_bit(ADAPTER_STATE_LINK_DOWN, &hba->adapter_state);
	} else {
		clear_bit(ADAPTER_STATE_LINK_DOWN, &hba->adapter_state);
	}
}


/**
 * bnx2i_iscsi_license_error - displays iscsi license related error message
 *
 * @hba: 		adapter instance pointer
 * @error_code:		error classification
 *
 * Puts out an error log when driver is unable to offload iscsi connection
 *	due to license restrictions
 **/
static void bnx2i_iscsi_license_error(struct bnx2i_hba *hba, u32 error_code)
{
	if (error_code == ISCSI_KCQE_COMPLETION_STATUS_ISCSI_NOT_SUPPORTED)
		/* iSCSI offload not supported on this device */
		PRINT_ERR(hba, "iSCSI not supported, dev=%s\n",
				hba->netdev->name);
	else if (error_code ==
		 ISCSI_KCQE_COMPLETION_STATUS_LOM_ISCSI_NOT_ENABLED)
		/* iSCSI offload not supported on this LOM device */
		PRINT_ERR(hba, "LOM is not enable to "
				"offload iSCSI connections, dev=%s\n",
				hba->netdev->name);
	set_bit(ADAPTER_STATE_INIT_FAILED, &hba->adapter_state);
}


extern unsigned int event_coal_div;

/**
 * bnx2i_arm_cq_event_coalescing - arms CQ to enable EQ notification
 *
 * @ep: 		endpoint (transport indentifier) structure
 * @action:		action, ARM or DISARM. For now only ARM_CQE is used
 *
 * Arm'ing CQ will enable chip to generate global EQ events inorder to interrupt
 *	the driver. EQ event is generated CQ index is hit or at least 1 CQ is
 *	outstanding and on chip timer expires
 **/
void bnx2i_arm_cq_event_coalescing(struct bnx2i_endpoint *ep, u8 action)
{
	struct bnx2i_5771x_cq_db *cq_db;
	u16 cq_index;
	u16 next_index;
	u32 num_active_cmds;

#ifndef _570X_ENABLE_EC_
	if (!test_bit(BNX2I_NX2_DEV_57710, &ep->hba->cnic_dev_type))
		return;
#endif

	if (test_bit(BNX2I_NX2_DEV_57710, &ep->hba->cnic_dev_type)) {
		cq_db = (struct bnx2i_5771x_cq_db*)ep->qp.cq_dma.pgtbl;
		if (!atomic_read(&ep->fp_kcqe_events) ||
		    (cq_db->sqn[0] && cq_db->sqn[0] != 0xFFFF))
			return;
	}

	if ((action == CNIC_ARM_CQE) && ep->sess) {
		num_active_cmds = ep->sess->active_cmd_count;
		if (num_active_cmds <= event_coal_min)
			next_index = 1;
		else 
			next_index = event_coal_min + (num_active_cmds - event_coal_min) / event_coal_div ;
		if (!next_index)
			next_index = 1;
		cq_index = ep->qp.cqe_exp_seq_sn + next_index - 1;
		if (cq_index > ep->qp.cqe_size * 2)
			cq_index -= ep->qp.cqe_size * 2;
		if (!cq_index)
			cq_index = 1;

#ifdef _570X_ENABLE_EC_
		if (test_bit(BNX2I_NX2_DEV_57710, &ep->hba->cnic_dev_type)) {
#endif
			cq_db = (struct bnx2i_5771x_cq_db*)ep->qp.cq_dma.pgtbl;
			cq_db->sqn[0] = cq_index;
			atomic_dec(&ep->fp_kcqe_events);
#ifdef _570X_ENABLE_EC_
			return;
		}
		writew(cq_index, ep->qp.ctx_base + CNIC_EVENT_COAL_INDEX);
#endif
	}
#ifdef _570X_ENABLE_EC_
	writeb(action, ep->qp.ctx_base + CNIC_EVENT_CQ_ARM);
#endif
}


/**
 * bnx2i_get_rq_buf - copy RQ buffer contents to driver buffer
 *
 * @conn: 		iscsi connection on which RQ event occured
 * @ptr:		driver buffer to which RQ buffer contents is to
 *			be copied
 * @len:		length of valid data inside RQ buf
 *
 * Copies RQ buffer contents from shared (DMA'able) memory region to
 *	driver buffer. RQ is used to DMA unsolicitated iscsi pdu's and
 *	scsi sense info
 **/
void bnx2i_get_rq_buf(struct bnx2i_conn *conn, char *ptr, int len)
{
	if (conn->ep->qp.rqe_left) {
		conn->ep->qp.rqe_left--;
		memcpy(ptr, (u8 *) conn->ep->qp.rq_cons_qe, len);
		
		if (conn->ep->qp.rq_cons_qe == conn->ep->qp.rq_last_qe) {
			conn->ep->qp.rq_cons_qe = conn->ep->qp.rq_first_qe;
			conn->ep->qp.rq_cons_idx = 0;
		} else {
			conn->ep->qp.rq_cons_qe++;
			conn->ep->qp.rq_cons_idx++;
		}
	}
}

/**
 * bnx2i_ring_577xx_doorbell - ring doorbell register to wake-up the
 *			processing engine
 * @conn: 		iscsi connection
 *
 * Only applicable to 57710 family of devices
 **/
void bnx2i_ring_577xx_doorbell(void __iomem *ctx_addr)
{
	struct bnx2i_5771x_dbell dbell;
	u32 msg;

	memset(&dbell, 0, sizeof(dbell));
	dbell.dbell.header = (B577XX_ISCSI_CONNECTION_TYPE <<
			      B577XX_DOORBELL_HDR_CONN_TYPE_SHIFT);
	msg = *((u32 *)&dbell);
	/* TODO : get doorbell register mapping */
	writel(cpu_to_le32(msg), ctx_addr);
}


/**
 * bnx2i_put_rq_buf - Replenish RQ buffer, if required ring on chip doorbell
 * @conn: 		iscsi connection on which event to post
 * @count: 		number of RQ buffer being posted to chip
 *
 * No need to ring hardware doorbell for 57710 family of devices
 **/
void bnx2i_put_rq_buf(struct bnx2i_conn *conn, int count)
{
	struct bnx2i_5771x_sq_rq_db *rq_db;
	u16 hi_bit = (conn->ep->qp.rq_prod_idx & 0x8000);

	conn->ep->qp.rqe_left += count;
	conn->ep->qp.rq_prod_idx &= 0x7FFF;
	conn->ep->qp.rq_prod_idx += count;

	if (conn->ep->qp.rq_prod_idx >= conn->sess->hba->max_rqes) {
		conn->ep->qp.rq_prod_idx %= conn->sess->hba->max_rqes;
		if (!hi_bit)
			conn->ep->qp.rq_prod_idx |= 0x8000;
	} else
		conn->ep->qp.rq_prod_idx |= hi_bit;

	if (test_bit(BNX2I_NX2_DEV_57710, &conn->ep->hba->cnic_dev_type)) {
		rq_db = (struct bnx2i_5771x_sq_rq_db *)
				conn->ep->qp.rq_dma.pgtbl;
		rq_db->prod_idx = conn->ep->qp.rq_prod_idx;
		/* no need to ring hardware doorbell for 57710 */
	} else {
		writew(conn->ep->qp.rq_prod_idx,
		       conn->ep->qp.ctx_base + CNIC_RECV_DOORBELL);
	}
	mmiowb();
}


void bnx2i_ring_sq_dbell_bnx2(struct bnx2i_conn *conn)
{
	struct qp_info *qp = &conn->ep->qp;

	wmb();	/* flush SQ WQE memory before the doorbell is rung */
	writew(qp->sq_prod_idx, qp->ctx_base + CNIC_SEND_DOORBELL);
	mmiowb(); /* flush posted PCI writes */
}


void bnx2i_ring_sq_dbell_bnx2x(struct bnx2i_conn *conn)
{
	struct bnx2i_5771x_sq_rq_db *sq_db;
	struct qp_info *qp = &conn->ep->qp;

	wmb();	/* flush SQ WQE memory before the doorbell is rung */
	sq_db = (struct bnx2i_5771x_sq_rq_db *) qp->sq_dma.pgtbl;
	sq_db->prod_idx = qp->sq_prod_idx;
	bnx2i_ring_577xx_doorbell(conn->ep->qp.ctx_base);
	mmiowb(); /* flush posted PCI writes */
}


/**
 * bnx2i_ring_dbell_update_sq_params - update SQ driver parameters
 *
 * @conn: 		iscsi connection to which new SQ entries belong
 * @count: 		number of SQ WQEs to post
 *
 * this routine will update SQ driver parameters and ring the doorbell
 **/
void bnx2i_ring_dbell_update_sq_params(struct bnx2i_conn *conn, int count)
{
	int tmp_cnt;
	struct qp_info *qp = &conn->ep->qp;

	if (count == 1) {
		if (qp->sq_prod_qe == qp->sq_last_qe)
			qp->sq_prod_qe = qp->sq_first_qe;
		else
			qp->sq_prod_qe++;
	} else {
		if ((qp->sq_prod_qe + count) <= qp->sq_last_qe)
			qp->sq_prod_qe += count;
		else {
			tmp_cnt = qp->sq_last_qe - qp->sq_prod_qe;
			qp->sq_prod_qe =
				&qp->sq_first_qe[count - (tmp_cnt + 1)];
		}
	}
	qp->sq_prod_idx += count;
	/* Ring the doorbell */
	conn->ring_doorbell(conn);
}


/**
 * bnx2i_send_iscsi_login - post iSCSI login request MP WQE to hardware
 *
 * @conn: 		iscsi connection
 * @cmd: 		driver command structure which is requesting
 *			a WQE to sent to chip for further processing
 *
 * prepare and post an iSCSI Login request WQE to CNIC firmware
 */
int bnx2i_send_iscsi_login(struct bnx2i_conn *conn, struct bnx2i_cmd *cmd)
{
	struct iscsi_login_request *login_wqe;
	struct iscsi_login *login_hdr;
	u32 dword;

	if (!conn->gen_pdu.req_buf || !conn->gen_pdu.resp_buf)
		return -EINVAL;

	login_hdr = (struct iscsi_login *) &conn->gen_pdu.pdu_hdr;
	login_wqe = (struct iscsi_login_request *) conn->ep->qp.sq_prod_qe;

	login_wqe->op_code = ISCSI_OP_LOGIN | ISCSI_OP_IMMEDIATE;
	login_wqe->op_attr = login_hdr->flags;
	login_wqe->version_max = login_hdr->max_version;
	login_wqe->version_min = login_hdr->min_version;
	login_wqe->data_length = ((login_hdr->dlength[0] << 16) |
				  (login_hdr->dlength[1] << 8) |
				  login_hdr->dlength[2]);

	login_wqe->isid_lo = *((u32 *) login_hdr->isid);
	login_wqe->isid_hi = *((u16 *) login_hdr->isid + 2);
	login_wqe->tsih = login_hdr->tsih;
	login_wqe->itt = (cmd->req.itt |
			  (ISCSI_TASK_TYPE_MPATH <<
			   ISCSI_LOGIN_REQUEST_TYPE_SHIFT));
	login_wqe->cid = login_hdr->cid;

	login_wqe->cmd_sn = ntohl(login_hdr->cmdsn);
	login_wqe->exp_stat_sn = ntohl(login_hdr->exp_statsn);

	login_wqe->resp_bd_list_addr_lo =
		(u32) conn->gen_pdu.login_resp.pgtbl_map;
	login_wqe->resp_bd_list_addr_hi =
		(u32) ((u64) conn->gen_pdu.login_resp.pgtbl_map >> 32);

	dword = ((1 << ISCSI_LOGIN_REQUEST_NUM_RESP_BDS_SHIFT) |
		 (conn->gen_pdu.resp_buf_size <<
		  ISCSI_LOGIN_REQUEST_RESP_BUFFER_LENGTH_SHIFT));
	login_wqe->resp_buffer = dword;
	login_wqe->flags = ISCSI_LOGIN_REQUEST_UPDATE_EXP_STAT_SN;
	login_wqe->bd_list_addr_lo = (u32) conn->gen_pdu.login_req.pgtbl_map;
	login_wqe->bd_list_addr_hi =
		(u32) ((u64) conn->gen_pdu.login_req.pgtbl_map >> 32);
	login_wqe->num_bds = 1;
	login_wqe->cq_index = 0; /* CQ# used for completion, 5771x only */
	conn->num_login_req_pdus++;

	bnx2i_ring_dbell_update_sq_params(conn, 1);
	return 0;
}


/**
 * bnx2i_send_iscsi_text - post iSCSI text request MP WQE to hardware
 *
 * @conn: 		iscsi connection
 * @cmd: 		driver command structure which is requesting
 *			a WQE to sent to chip for further processing
 *
 * prepare and post an iSCSI Text request WQE to CNIC firmware
 */
int bnx2i_send_iscsi_text(struct bnx2i_conn *conn, struct bnx2i_cmd *cmd)
{
	conn->num_text_req_pdus++;
	return 0;
}



/**
 * bnx2i_send_iscsi_tmf - post iSCSI task management request MP WQE to hardware
 *
 * @conn: 		iscsi connection
 * @cmd: 		driver command structure which is requesting
 *			a WQE to sent to chip for further processing
 *
 * prepare and post an iSCSI Login request WQE to CNIC firmware
 */
int bnx2i_send_iscsi_tmf(struct bnx2i_conn *conn, struct bnx2i_cmd *cmd)
{
	struct bnx2i_hba *hba = conn->sess->hba;
	u32 dword;
	u32 scsi_lun[2];
	struct iscsi_tmf_request *tmfabort_wqe;
	tmfabort_wqe = (struct iscsi_tmf_request *) conn->ep->qp.sq_prod_qe;
#if defined(__VMKLNX__) && (VMWARE_ESX_DDK_VERSION >= 60000)
	uint64_t sllid;
#endif

	memset(tmfabort_wqe, 0x00, sizeof(struct iscsi_tmf_request));
	tmfabort_wqe->op_code = cmd->iscsi_opcode;
	tmfabort_wqe->op_attr =
		ISCSI_TMF_REQUEST_ALWAYS_ONE | cmd->tmf_func;

#if defined(__VMKLNX__) && (VMWARE_ESX_DDK_VERSION >= 60000)
	if (cmd->tmf_ref_sc && cmd->tmf_ref_sc->vmkCmdPtr) {
		sllid = vmklnx_scsi_cmd_get_secondlevel_lun_id(cmd->tmf_ref_sc);
		BNX2I_DBG(DBG_TMF, hba, "%s: TMF sess %p sllid 0x%lx\n",
			__FUNCTION__, conn->sess, sllid);
		bnx2i_int_to_scsilun_with_sec_lun_id(cmd->tmf_lun,
			(struct scsi_lun *) scsi_lun, sllid);
	} else {
		int_to_scsilun(cmd->tmf_lun, (struct scsi_lun *) scsi_lun);
	}
#else
	int_to_scsilun(cmd->tmf_lun, (struct scsi_lun *) scsi_lun);
#endif
	tmfabort_wqe->lun[0] = ntohl(scsi_lun[0]);
	tmfabort_wqe->lun[1] = ntohl(scsi_lun[1]);

	tmfabort_wqe->itt = (cmd->req.itt | (ISCSI_TASK_TYPE_MPATH << 14));
	tmfabort_wqe->reserved2 = 0;
	tmfabort_wqe->cmd_sn = conn->sess->cmdsn;

	BNX2I_DBG(DBG_TMF, hba, "%s: TMF sess %p CmdSN %x\n",
		  __FUNCTION__, conn->sess, tmfabort_wqe->cmd_sn);

	if (cmd->tmf_func == ISCSI_TM_FUNC_ABORT_TASK) {
		if (cmd->tmf_ref_cmd->req.op_attr & ISCSI_CMD_REQUEST_READ)
			dword = (ISCSI_TASK_TYPE_READ << ISCSI_CMD_REQUEST_TYPE_SHIFT);
		else
			dword = (ISCSI_TASK_TYPE_WRITE << ISCSI_CMD_REQUEST_TYPE_SHIFT);
		tmfabort_wqe->ref_itt = (dword |= cmd->tmf_ref_itt);
		tmfabort_wqe->ref_cmd_sn = cmd->tmf_ref_cmd->req.cmd_sn;
	} else {
		tmfabort_wqe->ref_itt = ISCSI_RESERVED_TAG;
	}

	tmfabort_wqe->bd_list_addr_lo = (u32) hba->mp_dma_buf.pgtbl_map;
	tmfabort_wqe->bd_list_addr_hi =
		(u32) ((u64) hba->mp_dma_buf.pgtbl_map >> 32);
	tmfabort_wqe->num_bds = 1;
	tmfabort_wqe->cq_index = 0; /* CQ# used for completion, 5771x only */
	conn->num_tmf_req_pdus++;

	bnx2i_ring_dbell_update_sq_params(conn, 1);
	return 0;
}

/**
 * bnx2i_send_iscsi_scsicmd - post iSCSI scsicmd request WQE to hardware
 *
 * @conn: 		iscsi connection
 * @cmd: 		driver command structure which is requesting
 *			a WQE to sent to chip for further processing
 *
 * prepare and post an iSCSI SCSI-CMD request WQE to CNIC firmware
 */
int bnx2i_send_iscsi_scsicmd(struct bnx2i_conn *conn, struct bnx2i_cmd *cmd)
{
	struct iscsi_cmd_request *scsi_cmd_wqe;

	if (!conn->ep)
		return -1;

	scsi_cmd_wqe = (struct iscsi_cmd_request *) conn->ep->qp.sq_prod_qe;
	memcpy(scsi_cmd_wqe, &cmd->req, sizeof(struct iscsi_cmd_request));
	scsi_cmd_wqe->cq_index = 0; /* CQ# used for completion, 5771x only */

	conn->num_scsi_cmd_pdus++;
	bnx2i_ring_dbell_update_sq_params(conn, 1);
	return 0;
}

/**
 * bnx2i_send_iscsi_nopout - post iSCSI NOPOUT request WQE to hardware
 *
 * @conn: 		iscsi connection
 * @cmd: 		driver command structure which is requesting
 *			a WQE to sent to chip for further processing
 * @ttt:		TTT to be used when building pdu header
 * @datap:		payload buffer pointer
 * @data_len:		payload data length
 * @unsol:		indicated whether nopout pdu is unsolicited pdu or
 *			in response to target's NOPIN w/ TTT != FFFFFFFF
 *
 * prepare and post a nopout request WQE to CNIC firmware
 */
/*
 */
int bnx2i_send_iscsi_nopout(struct bnx2i_conn *conn, struct bnx2i_cmd *cmd,
			    char *datap, int data_len)
{
	u8 wqe_flags;
	int unsol;
	struct iscsi_nop_out_request *nopout_wqe;
	struct iscsi_nopout *nop_hdr;

	if (!conn->ep)
		return -EINVAL;

	/* Check if it's an unsolicited pdu or a response to target's nopin */
	if (cmd->ttt == ISCSI_RESERVED_TAG) {
conn->sess->unsol_noopout_count++;
		nop_hdr = (struct iscsi_nopout *) &conn->gen_pdu.nopout_hdr;
		unsol = 1;
		wqe_flags = 0;
	} else {
conn->sess->noopout_resp_count++;
		nop_hdr = (struct iscsi_nopout *) &conn->gen_pdu.nopin_hdr;
		unsol = 0;
		wqe_flags = ISCSI_NOP_OUT_REQUEST_LOCAL_COMPLETION;
	}

	nopout_wqe = (struct iscsi_nop_out_request *) conn->ep->qp.sq_prod_qe;
	memset(nopout_wqe, 0, sizeof(struct iscsi_nop_out_request));
	nopout_wqe->op_code = (ISCSI_OP_NOOP_OUT | ISCSI_OP_IMMEDIATE);
	nopout_wqe->op_attr = ISCSI_FLAG_CMD_FINAL;
	memcpy(nopout_wqe->lun, nop_hdr->lun, 8);

	if (test_bit(BNX2I_NX2_DEV_57710, &conn->ep->hba->cnic_dev_type)) {
		u32 tmp = nopout_wqe->lun[0];
		/* 57710 requires LUN field to be swapped */
		nopout_wqe->lun[0] = nopout_wqe->lun[1];
		nopout_wqe->lun[1] = tmp;
	}
	nopout_wqe->ttt = cmd->ttt;
	nopout_wqe->itt = ((u16) cmd->req.itt |
			   (ISCSI_TASK_TYPE_MPATH <<
			    ISCSI_TMF_REQUEST_TYPE_SHIFT));
	nopout_wqe->flags = wqe_flags;
	nopout_wqe->cmd_sn = conn->sess->cmdsn;
	nopout_wqe->data_length = data_len;
	if (data_len) {
		/* handle payload data, not required in first release */
		PRINT_ALERT(conn->sess->hba, "NOPOUT: WARNING!! payload "
				"len != 0\n");
	} else {
		nopout_wqe->bd_list_addr_lo =
			(u32) conn->sess->hba->mp_dma_buf.pgtbl_map;
		nopout_wqe->bd_list_addr_hi =
			(u32) ((u64) conn->sess->hba->mp_dma_buf.pgtbl_map >> 32);
		nopout_wqe->num_bds = 1;
	}
	nopout_wqe->cq_index = 0; /* CQ# used for completion, 5771x only */

	conn->num_nopout_pdus++;
	bnx2i_ring_dbell_update_sq_params(conn, 1);

	conn->sess->last_nooput_sn = conn->sess->cmdsn;
	conn->sess->last_nooput_posted = jiffies;
	conn->sess->noopout_posted_count++;
	return 0;
}


/**
 * bnx2i_send_iscsi_logout - post iSCSI logout request WQE to hardware
 *
 * @conn: 		iscsi connection
 * @cmd: 		driver command structure which is requesting
 *			a WQE to sent to chip for further processing
 *
 * prepare and post logout request WQE to CNIC firmware
 */
int bnx2i_send_iscsi_logout(struct bnx2i_conn *conn, struct bnx2i_cmd *cmd)
{
	struct iscsi_logout_request *logout_wqe;
	struct iscsi_logout *logout_hdr;
	struct bnx2i_hba *hba = conn->sess->hba;

	if (!conn->ep)
		return -EINVAL;

	conn->sess->state = BNX2I_SESS_IN_LOGOUT;

	logout_hdr = (struct iscsi_logout *) &conn->gen_pdu.pdu_hdr;

	logout_wqe = (struct iscsi_logout_request *) conn->ep->qp.sq_prod_qe;
	memset(logout_wqe, 0x00, sizeof(struct iscsi_logout_request));

	logout_wqe->op_code = (logout_hdr->opcode | ISCSI_OP_IMMEDIATE);
	logout_wqe->op_attr =
			logout_hdr->flags | ISCSI_LOGOUT_REQUEST_ALWAYS_ONE;
	logout_wqe->itt = ((u16) cmd->req.itt |
			   (ISCSI_TASK_TYPE_MPATH <<
			    ISCSI_LOGOUT_REQUEST_TYPE_SHIFT));
	logout_wqe->data_length = 0;
	logout_wqe->cmd_sn = conn->sess->cmdsn;
	logout_wqe->cid = conn->conn_cid;

	logout_wqe->bd_list_addr_lo = (u32) hba->mp_dma_buf.pgtbl_map;
	logout_wqe->bd_list_addr_hi =
		(u32) ((u64) hba->mp_dma_buf.pgtbl_map >> 32);
	logout_wqe->num_bds = 1;
	logout_wqe->cq_index = 0; /* CQ# used for completion, 5771x only */

	conn->num_logout_req_pdus++;
	bnx2i_ring_dbell_update_sq_params(conn, 1);
	return 0;
}


/**
 * bnx2i_update_iscsi_conn - post iSCSI logout request WQE to hardware
 *
 * @conn: 		iscsi connection which requires iscsi parameter update 
 *
 * sends down iSCSI Conn Update request to move iSCSI conn to FFP
 */
int bnx2i_update_iscsi_conn(struct bnx2i_conn *conn)
{
	struct bnx2i_hba *hba = conn->sess->hba;
	struct kwqe *kwqe_arr[2];
	struct iscsi_kwqe_conn_update *update_wqe;
	struct iscsi_kwqe_conn_update conn_update_kwqe;
	int rc = -EINVAL;

	update_wqe = &conn_update_kwqe;

	update_wqe->hdr.op_code = ISCSI_KWQE_OPCODE_UPDATE_CONN;
	update_wqe->hdr.flags =
		(ISCSI_KWQE_LAYER_CODE << ISCSI_KWQE_HEADER_LAYER_CODE_SHIFT);

	/* 57710 requires conn context id to be passed as is */
	if (test_bit(BNX2I_NX2_DEV_57710, &conn->ep->hba->cnic_dev_type))
		update_wqe->context_id = conn->ep->ep_cid;
	else {
		update_wqe->context_id = (conn->ep->ep_cid >> 7);
		update_wqe->reserved2 = conn->ep->ep_iscsi_cid;
	}

	update_wqe->conn_flags = 0;
	if (conn->header_digest_en)
		update_wqe->conn_flags |= ISCSI_KWQE_CONN_UPDATE_HEADER_DIGEST;
	if (conn->data_digest_en)
		update_wqe->conn_flags |= ISCSI_KWQE_CONN_UPDATE_DATA_DIGEST;
	if (conn->sess->initial_r2t)
		update_wqe->conn_flags |= ISCSI_KWQE_CONN_UPDATE_INITIAL_R2T;
	if (conn->sess->imm_data)
		update_wqe->conn_flags |= ISCSI_KWQE_CONN_UPDATE_IMMEDIATE_DATA;

	update_wqe->max_send_pdu_length = conn->max_data_seg_len_xmit;
	update_wqe->max_recv_pdu_length = conn->max_data_seg_len_recv;
	update_wqe->first_burst_length = conn->sess->first_burst_len;
	update_wqe->max_burst_length = conn->sess->max_burst_len;
	update_wqe->exp_stat_sn = conn->exp_statsn;
	update_wqe->max_outstanding_r2ts = conn->sess->max_r2t;
	update_wqe->session_error_recovery_level = conn->sess->erl;
	PRINT_ALERT(hba, "conn update: icid %d - MBL 0x%x FBL 0x%x"
			  "MRDSL_I 0x%x MRDSL_T 0x%x flags 0x%x "
			  "exp_stat_sn %d\n",
			  conn->ep->ep_iscsi_cid,
			  update_wqe->max_burst_length,
			  update_wqe->first_burst_length,
			  update_wqe->max_recv_pdu_length,
			  update_wqe->max_send_pdu_length,
			  update_wqe->conn_flags,
			  update_wqe->exp_stat_sn);

	kwqe_arr[0] = (struct kwqe *) update_wqe;
	if (hba->cnic && hba->cnic->submit_kwqes)
		rc = hba->cnic->submit_kwqes(hba->cnic, kwqe_arr, 1);

	return rc;
}


/**
 * bnx2i_ep_ofld_timer - post iSCSI logout request WQE to hardware
 *
 * @data: 		endpoint (transport handle) structure pointer
 *
 * routine to handle connection offload/destroy request timeout
 */
void bnx2i_ep_ofld_timer(unsigned long data)
{
	struct bnx2i_endpoint *ep = (struct bnx2i_endpoint *) data;

	if (ep->state == EP_STATE_OFLD_START) {
		PRINT_ALERT(ep->hba, "[%lx]: ofld_timer: CONN_OFLD "
				"timeout (ep %p {%d, %x}, conn %p, sess %p)\n",
				 jiffies, ep, ep->ep_iscsi_cid, ep->ep_cid,
                                 ep->conn, ep->sess);
		ep->state = EP_STATE_OFLD_FAILED;
	} else if (ep->state == EP_STATE_ULP_UPDATE_START) {
		PRINT_ALERT(ep->hba, "[%lx]: ofld_timer: CONN_UPDATE"
				"timeout (ep %p {%d, %x}, conn %p, sess %p)\n",
				jiffies, ep, ep->ep_iscsi_cid, ep->ep_cid,
				ep->conn, ep->sess);
		ep->state = EP_STATE_ULP_UPDATE_TIMEOUT;
	} else if (ep->state == EP_STATE_DISCONN_START) {
		PRINT_ALERT(ep->hba, "[%lx]: ofld_timer: CONN_DISCON "
				"timeout (ep %p {%d, %x}, conn %p, sess %p)\n",
				jiffies, ep, ep->ep_iscsi_cid, ep->ep_cid,
				ep->conn, ep->sess);
		ep->state = EP_STATE_DISCONN_TIMEDOUT;
	} else if (ep->state == EP_STATE_CONNECT_START) {
		PRINT_ALERT(ep->hba, "[%lx]: ofld_timer: CONN_START"
				"timeout (ep %p {%d, %x}, conn %p, sess %p)\n",
				jiffies, ep, ep->ep_iscsi_cid, ep->ep_cid,
				ep->conn, ep->sess);
		ep->state = EP_STATE_CONNECT_FAILED;
	} else if (ep->state == EP_STATE_CLEANUP_START) {
		PRINT_ALERT(ep->hba, "[%lx]: ofld_timer: CONN_CLEANUP "
				"timeout (ep %p {%d, %x}, conn %p, sess %p)\n", 
				jiffies, ep, ep->ep_iscsi_cid, ep->ep_cid,
				ep->conn, ep->sess);
		ep->state = EP_STATE_CLEANUP_FAILED;
	}

	wake_up_interruptible(&ep->ofld_wait);
}


static int bnx2i_power_of2(u32 val)
{
	u32 power = 0;
	if (val & (val - 1))
		return power;
	val--;
	while (val) {
		val = val >> 1;
		power++;
	}
	return power;
}


/**
 * bnx2i_send_cmd_cleanup_req - send iscsi cmd context clean-up request
 *
 * @hba: 		adapter structure pointer
 * @cmd: 		driver command structure which is requesting
 *			a WQE to sent to chip for further processing
 *
 * prepares and posts CONN_OFLD_REQ1/2 KWQE
 */
void bnx2i_send_cmd_cleanup_req(struct bnx2i_hba *hba, struct bnx2i_cmd *cmd)
{
	struct iscsi_cleanup_request *cmd_cleanup;
	u32 dword;

	cmd_cleanup =
		(struct iscsi_cleanup_request *) cmd->conn->ep->qp.sq_prod_qe;
	memset(cmd_cleanup, 0x00, sizeof(struct iscsi_cleanup_request));

	cmd_cleanup->op_code = ISCSI_OPCODE_CLEANUP_REQUEST;

	if (cmd->req.op_attr & ISCSI_CMD_REQUEST_READ)
		dword = (ISCSI_TASK_TYPE_READ << ISCSI_CMD_REQUEST_TYPE_SHIFT);
	else
		dword = (ISCSI_TASK_TYPE_WRITE << ISCSI_CMD_REQUEST_TYPE_SHIFT);
	cmd_cleanup->itt = (dword | cmd->req.itt);
	cmd_cleanup->cq_index = 0; /* CQ# used for completion, 5771x only */

	bnx2i_ring_dbell_update_sq_params(cmd->conn, 1);
}


/**
 * bnx2i_send_conn_destroy - initiates iscsi connection teardown process
 *
 * @hba: 		adapter structure pointer
 * @ep: 		endpoint (transport indentifier) structure
 *
 * this routine prepares and posts CONN_OFLD_REQ1/2 KWQE to initiate
 * 	iscsi connection context clean-up process
 */
int bnx2i_send_conn_destroy(struct bnx2i_hba *hba, struct bnx2i_endpoint *ep)
{
	struct kwqe *kwqe_arr[2];
	struct iscsi_kwqe_conn_destroy conn_cleanup;
	int rc = -EINVAL;

	memset(&conn_cleanup, 0x00, sizeof(struct iscsi_kwqe_conn_destroy));

	conn_cleanup.hdr.op_code = ISCSI_KWQE_OPCODE_DESTROY_CONN;
	conn_cleanup.hdr.flags =
		(ISCSI_KWQE_LAYER_CODE << ISCSI_KWQE_HEADER_LAYER_CODE_SHIFT);
	/* 57710 requires conn context id to be passed as is */
	if (test_bit(BNX2I_NX2_DEV_57710, &ep->hba->cnic_dev_type))
		conn_cleanup.context_id = ep->ep_cid;
	else
		conn_cleanup.context_id = (ep->ep_cid >> 7);

	conn_cleanup.reserved0 = (u16)ep->ep_iscsi_cid;

	kwqe_arr[0] = (struct kwqe *) &conn_cleanup;
	if (hba->cnic && hba->cnic->submit_kwqes)
		rc = hba->cnic->submit_kwqes(hba->cnic, kwqe_arr, 1);

	return rc;
}


/**
 * bnx2i_570x_send_conn_ofld_req - initiates iscsi connection context setup process
 *
 * @hba: 		adapter structure pointer
 * @ep: 		endpoint (transport indentifier) structure
 *
 * 5706/5708/5709 specific - prepares and posts CONN_OFLD_REQ1/2 KWQE
 */
static int bnx2i_570x_send_conn_ofld_req(struct bnx2i_hba *hba,
					 struct bnx2i_endpoint *ep)
{
	struct kwqe *kwqe_arr[2];
	struct iscsi_kwqe_conn_offload1 ofld_req1;
	struct iscsi_kwqe_conn_offload2 ofld_req2;
	dma_addr_t dma_addr;
	int num_kwqes = 2;
	int rc = -EINVAL;
	u32 *ptbl;

	ofld_req1.hdr.op_code = ISCSI_KWQE_OPCODE_OFFLOAD_CONN1;
	ofld_req1.hdr.flags =
		(ISCSI_KWQE_LAYER_CODE << ISCSI_KWQE_HEADER_LAYER_CODE_SHIFT);

	ofld_req1.iscsi_conn_id = (u16) ep->ep_iscsi_cid;

	dma_addr = ep->qp.sq_dma.pgtbl_map;
	ofld_req1.sq_page_table_addr_lo = (u32) dma_addr;
	ofld_req1.sq_page_table_addr_hi = (u32) ((u64) dma_addr >> 32);

	dma_addr = ep->qp.cq_dma.pgtbl_map;
	ofld_req1.cq_page_table_addr_lo = (u32) dma_addr;
	ofld_req1.cq_page_table_addr_hi = (u32) ((u64) dma_addr >> 32);

	ofld_req2.hdr.op_code = ISCSI_KWQE_OPCODE_OFFLOAD_CONN2;
	ofld_req2.hdr.flags =
		(ISCSI_KWQE_LAYER_CODE << ISCSI_KWQE_HEADER_LAYER_CODE_SHIFT);

	dma_addr = ep->qp.rq_dma.pgtbl_map;
	ofld_req2.rq_page_table_addr_lo = (u32) dma_addr;
	ofld_req2.rq_page_table_addr_hi = (u32) ((u64) dma_addr >> 32);

	ptbl = (u32 *) ep->qp.sq_dma.pgtbl;

	ofld_req2.sq_first_pte.hi = *ptbl++;
	ofld_req2.sq_first_pte.lo = *ptbl;

	ptbl = (u32 *) ep->qp.cq_dma.pgtbl;
	ofld_req2.cq_first_pte.hi = *ptbl++;
	ofld_req2.cq_first_pte.lo = *ptbl;

	kwqe_arr[0] = (struct kwqe *) &ofld_req1;
	kwqe_arr[1] = (struct kwqe *) &ofld_req2;
	ofld_req2.num_additional_wqes = 0;

	if (hba->cnic && hba->cnic->submit_kwqes)
		rc = hba->cnic->submit_kwqes(hba->cnic, kwqe_arr, num_kwqes);

	return rc;
}


/**
 * bnx2i_5771x_send_conn_ofld_req - initiates iscsi connection context setup process
 *
 * @hba: 		adapter structure pointer
 * @ep: 		endpoint (transport indentifier) structure
 *
 * 57710 specific - prepares and posts CONN_OFLD_REQ1/2 KWQE
 */
static int bnx2i_5771x_send_conn_ofld_req(struct bnx2i_hba *hba,
					  struct bnx2i_endpoint *ep)
{
	struct kwqe *kwqe_arr[5];
	struct iscsi_kwqe_conn_offload1 ofld_req1;
	struct iscsi_kwqe_conn_offload2 ofld_req2;
	struct iscsi_kwqe_conn_offload3 ofld_req3[1];
	dma_addr_t dma_addr;
	int num_kwqes = 2;
	int rc = -EINVAL;
	u32 *ptbl;

	ofld_req1.hdr.op_code = ISCSI_KWQE_OPCODE_OFFLOAD_CONN1;
	ofld_req1.hdr.flags =
		(ISCSI_KWQE_LAYER_CODE << ISCSI_KWQE_HEADER_LAYER_CODE_SHIFT);

	ofld_req1.iscsi_conn_id = (u16) ep->ep_iscsi_cid;

	dma_addr = ep->qp.sq_dma.pgtbl_map + ISCSI_SQ_DB_SIZE;
	ofld_req1.sq_page_table_addr_lo = (u32) dma_addr;
	ofld_req1.sq_page_table_addr_hi = (u32) ((u64) dma_addr >> 32);

	dma_addr = ep->qp.cq_dma.pgtbl_map + ISCSI_CQ_DB_SIZE;
	ofld_req1.cq_page_table_addr_lo = (u32) dma_addr;
	ofld_req1.cq_page_table_addr_hi = (u32) ((u64) dma_addr >> 32);

	ofld_req2.hdr.op_code = ISCSI_KWQE_OPCODE_OFFLOAD_CONN2;
	ofld_req2.hdr.flags =
		(ISCSI_KWQE_LAYER_CODE << ISCSI_KWQE_HEADER_LAYER_CODE_SHIFT);

	dma_addr = ep->qp.rq_dma.pgtbl_map + ISCSI_RQ_DB_SIZE;
	ofld_req2.rq_page_table_addr_lo = (u32) dma_addr;
	ofld_req2.rq_page_table_addr_hi = (u32) ((u64) dma_addr >> 32);

	ptbl = (u32 *)((u8 *)ep->qp.sq_dma.pgtbl + ISCSI_SQ_DB_SIZE);
	ofld_req2.sq_first_pte.hi = *ptbl++;
	ofld_req2.sq_first_pte.lo = *ptbl;

	ptbl = (u32 *)((u8 *)ep->qp.cq_dma.pgtbl + ISCSI_CQ_DB_SIZE);
	ofld_req2.cq_first_pte.hi = *ptbl++;
	ofld_req2.cq_first_pte.lo = *ptbl;

	kwqe_arr[0] = (struct kwqe *) &ofld_req1;
	kwqe_arr[1] = (struct kwqe *) &ofld_req2;

	ofld_req2.num_additional_wqes = 1;
	memset(ofld_req3, 0x00, sizeof(ofld_req3[0]));
	ptbl = (u32 *)((u8 *)ep->qp.rq_dma.pgtbl + ISCSI_RQ_DB_SIZE);
	ofld_req3[0].qp_first_pte[0].hi = *ptbl++;
	ofld_req3[0].qp_first_pte[0].lo = *ptbl;

	kwqe_arr[2] = (struct kwqe *) ofld_req3;
	/* need if we decide to go with multiple KCQE's per conn */
	num_kwqes += 1;

	if (hba->cnic && hba->cnic->submit_kwqes)
		rc =hba->cnic->submit_kwqes(hba->cnic, kwqe_arr, num_kwqes);

	return rc;
}

/**
 * bnx2i_send_conn_ofld_req - initiates iscsi connection context setup process
 *
 * @hba: 		adapter structure pointer
 * @ep: 		endpoint (transport indentifier) structure
 *
 * this routine prepares and posts CONN_OFLD_REQ1/2 KWQE
 */
int bnx2i_send_conn_ofld_req(struct bnx2i_hba *hba, struct bnx2i_endpoint *ep)
{
	int rc;

	if (test_bit(BNX2I_NX2_DEV_57710, &hba->cnic_dev_type))
		rc = bnx2i_5771x_send_conn_ofld_req(hba, ep);
	else
		rc = bnx2i_570x_send_conn_ofld_req(hba, ep);

	return rc;
}


/**
 * bnx2i_alloc_qp_resc - allocates requires resources for QP (transport layer
 * 			for iSCSI connection)
 *
 * @hba: 		adapter structure pointer
 * @ep: 		endpoint (transport indentifier) structure
 *
 * Allocate QP resources, DMA'able memory for SQ/RQ/CQ and page tables.
 * 	EP structure elements such as producer/consumer indexes/pointers,
 *	queue sizes and page table contents are setup
 */
int bnx2i_alloc_qp_resc(struct bnx2i_hba *hba, struct bnx2i_endpoint *ep)
{
	struct bnx2i_5771x_cq_db *cq_db;
	u32 num_que_elements;
	int mem_size;
	int sq_pgtbl_off = 0;
	int cq_pgtbl_off = 0;
	int rq_pgtbl_off = 0;

	if (test_bit(BNX2I_NX2_DEV_57710, &ep->hba->cnic_dev_type)) {
		sq_pgtbl_off = ISCSI_SQ_DB_SIZE;
		cq_pgtbl_off = ISCSI_CQ_DB_SIZE;
		rq_pgtbl_off = ISCSI_RQ_DB_SIZE;
	}
	
	ep->hba = hba;
	ep->conn = NULL;
	ep->ep_cid = ep->ep_pg_cid = 0;

	/* Allocate page table memory for SQ which is page aligned */
	mem_size = hba->max_sqes * BNX2I_SQ_WQE_SIZE;
	mem_size = (mem_size + (PAGE_SIZE - 1)) & PAGE_MASK;
	num_que_elements = hba->max_sqes;

	if (bnx2i_alloc_dma(hba, &ep->qp.sq_dma, mem_size,
			    BNX2I_TBL_TYPE_PG, sq_pgtbl_off))
		goto error;

	memset(ep->qp.sq_virt, 0x00, mem_size);

	ep->qp.sq_first_qe = (struct sqe *) ep->qp.sq_virt;
	ep->qp.sq_prod_qe = ep->qp.sq_first_qe;
	ep->qp.sq_last_qe = &ep->qp.sq_first_qe[num_que_elements - 1];
	ep->qp.sq_prod_idx = 0;

	/* Allocate page table memory for CQ which is page aligned */
	mem_size = hba->max_cqes * BNX2I_CQE_SIZE;
	mem_size = (mem_size + (PAGE_SIZE - 1)) & PAGE_MASK;
	num_que_elements = hba->max_cqes;

	if (bnx2i_alloc_dma(hba, &ep->qp.cq_dma, mem_size,
			    BNX2I_TBL_TYPE_PG, cq_pgtbl_off))
		goto error;

	memset (ep->qp.cq_virt, 0x00, mem_size);

	ep->qp.cq_first_qe = (struct cqe *) ep->qp.cq_virt;
	ep->qp.cq_cons_qe = ep->qp.cq_first_qe;
	ep->qp.cq_last_qe = &ep->qp.cq_first_qe[num_que_elements - 1];
	ep->qp.cq_cons_idx = 0;
	ep->qp.cqe_left = num_que_elements;
	ep->qp.cqe_exp_seq_sn = ISCSI_INITIAL_SN;
	ep->qp.cqe_size = hba->max_cqes;

	/* Invalidate all EQ CQE index, req only for 57710 */
	cq_db = (struct bnx2i_5771x_cq_db *) ep->qp.cq_dma.pgtbl;
	memset(cq_db->sqn, 0xFF, sizeof(cq_db->sqn[0]) * BNX2X_MAX_CQS);

	/* Allocate page table memory for RQ which is page aligned */
	mem_size = hba->max_rqes * BNX2I_RQ_WQE_SIZE;
	mem_size = (mem_size + (PAGE_SIZE - 1)) & PAGE_MASK;
	num_que_elements = hba->max_rqes;

	if (bnx2i_alloc_dma(hba, &ep->qp.rq_dma, mem_size,
			    BNX2I_TBL_TYPE_PG, rq_pgtbl_off))
		goto error;

	ep->qp.rq_first_qe = (struct rqe *) ep->qp.rq_virt;
	ep->qp.rq_prod_qe = ep->qp.rq_first_qe;
	ep->qp.rq_cons_qe = ep->qp.rq_first_qe;
	ep->qp.rq_last_qe = &ep->qp.rq_first_qe[num_que_elements - 1];
	ep->qp.rq_prod_idx = 0x8000;
	ep->qp.rq_cons_idx = 0;
	ep->qp.rqe_left = num_que_elements;

	return 0;

error:
	bnx2i_free_qp_resc(hba, ep);
	return -ENOMEM;
}



/**
 * bnx2i_free_qp_resc - free memory resources held by QP
 *
 * @hba: 		adapter structure pointer
 * @ep: 		endpoint (transport indentifier) structure
 *
 * Free QP resources - SQ/RQ/CQ memory and page tables.
 */
void bnx2i_free_qp_resc(struct bnx2i_hba *hba, struct bnx2i_endpoint *ep)
{
	if (ep->qp.ctx_base) {
		iounmap(ep->qp.ctx_base);
		ep->qp.ctx_base = NULL;
	}

	bnx2i_free_dma(hba, &ep->qp.sq_dma);
	bnx2i_free_dma(hba, &ep->qp.cq_dma);
	bnx2i_free_dma(hba, &ep->qp.rq_dma);
}

extern unsigned int error_mask1, error_mask2;
extern unsigned int en_tcp_dack;
extern u64 iscsi_error_mask;

/**
 * bnx2i_send_fw_iscsi_init_msg - initiates initial handshake with iscsi f/w
 *
 * @hba: 		adapter structure pointer
 *
 * Send down iscsi_init KWQEs which initiates the initial handshake with the f/w
 * 	This results in iSCSi support validation and on-chip context manager
 * 	initialization.  Firmware completes this handshake with a CQE carrying
 * 	the result of iscsi support validation. Parameter carried by 
 * 	iscsi init request determines the number of offloaded connection and
 * 	tolerance level for iscsi protocol violation this hba/chip can support
 */
int bnx2i_send_fw_iscsi_init_msg(struct bnx2i_hba *hba)
{
	struct kwqe *kwqe_arr[3];
	struct iscsi_kwqe_init1 iscsi_init;
	struct iscsi_kwqe_init2 iscsi_init2;
	int rc = 0;
	u64 mask64;

	memset(&iscsi_init, 0x00, sizeof(struct iscsi_kwqe_init1));
	memset(&iscsi_init2, 0x00, sizeof(struct iscsi_kwqe_init2));

	bnx2i_adjust_qp_size(hba);

	iscsi_init.flags =
		ISCSI_PAGE_SIZE_4K << ISCSI_KWQE_INIT1_PAGE_SIZE_SHIFT;
	if (en_tcp_dack)
		iscsi_init.flags |= ISCSI_KWQE_INIT1_DELAYED_ACK_ENABLE;
	if (test_bit(BNX2I_NX2_DEV_57710, &hba->cnic_dev_type) && time_stamps)
		iscsi_init.flags |= ISCSI_KWQE_INIT1_TIME_STAMPS_ENABLE;

	iscsi_init.reserved0 = 0;
	iscsi_init.num_cqs = 1;
	iscsi_init.hdr.op_code = ISCSI_KWQE_OPCODE_INIT1;
	iscsi_init.hdr.flags =
		(ISCSI_KWQE_LAYER_CODE << ISCSI_KWQE_HEADER_LAYER_CODE_SHIFT);

	iscsi_init.dummy_buffer_addr_lo = (u32) hba->mp_dma_buf.mapping;
	iscsi_init.dummy_buffer_addr_hi =
		(u32) ((u64) hba->mp_dma_buf.mapping >> 32);

	hba->ctx_ccell_tasks =
			((hba->num_ccell & 0xFFFF) | (hba->max_sqes << 16));
	iscsi_init.num_ccells_per_conn = hba->num_ccell;
	iscsi_init.num_tasks_per_conn = hba->max_sqes;
	iscsi_init.sq_wqes_per_page = PAGE_SIZE / BNX2I_SQ_WQE_SIZE;
	iscsi_init.sq_num_wqes = hba->max_sqes;
	iscsi_init.cq_log_wqes_per_page =
		(u8) bnx2i_power_of2(PAGE_SIZE / BNX2I_CQE_SIZE);
	iscsi_init.cq_num_wqes = hba->max_cqes;
	iscsi_init.cq_num_pages = (hba->max_cqes * BNX2I_CQE_SIZE +
				   (PAGE_SIZE - 1)) / PAGE_SIZE;
	iscsi_init.sq_num_pages = (hba->max_sqes * BNX2I_SQ_WQE_SIZE +
				   (PAGE_SIZE - 1)) / PAGE_SIZE;
	iscsi_init.rq_buffer_size = BNX2I_RQ_WQE_SIZE;
	iscsi_init.rq_num_wqes = hba->max_rqes;


	iscsi_init2.hdr.op_code = ISCSI_KWQE_OPCODE_INIT2;
	iscsi_init2.hdr.flags =
		(ISCSI_KWQE_LAYER_CODE << ISCSI_KWQE_HEADER_LAYER_CODE_SHIFT);
	iscsi_init2.max_cq_sqn = hba->max_cqes * 2 + 1;
	mask64 = 0x0ULL;
	mask64 |= (
		/* CISCO MDS */
		(1UL <<
		  ISCSI_KCQE_COMPLETION_STATUS_PROTOCOL_ERR_TTT_NOT_RSRV) |
		/* HP MSA1510i */
		(1UL <<
		  ISCSI_KCQE_COMPLETION_STATUS_PROTOCOL_ERR_EXP_DATASN) |
		/* EMC */
		(1ULL << ISCSI_KCQE_COMPLETION_STATUS_PROTOCOL_ERR_LUN));
	if (error_mask1) {
		iscsi_init2.error_bit_map[0] = error_mask1;
		mask64 ^= (u32) mask64;
		mask64 |= error_mask1;
	}
	else 
		iscsi_init2.error_bit_map[0] = (u32) mask64;
	if (error_mask2) {
		iscsi_init2.error_bit_map[1] = error_mask2;
		mask64 &= 0xffffffff;
		mask64 |= ((u64)error_mask2 << 32);
	}
	else
		iscsi_init2.error_bit_map[1] = (u32) (mask64 >> 32);

	iscsi_error_mask = mask64;

	kwqe_arr[0] = (struct kwqe *) &iscsi_init;
	kwqe_arr[1] = (struct kwqe *) &iscsi_init2;

	if (hba->cnic && hba->cnic->submit_kwqes) {
		rc = hba->cnic->submit_kwqes(hba->cnic, kwqe_arr, 2);
	}
	return rc;
}


/**
 * bnx2i_complete_cmd - completion CQE processing
 *
 * @sess: 		iscsi sess pointer
 * @cmd: 		command pointer
 *
 * process SCSI CMD Response CQE & complete the request to SCSI-ML
 */
int bnx2i_complete_cmd(struct bnx2i_sess *sess, struct bnx2i_cmd *cmd)
{
	struct scsi_cmnd *sc;
	struct Scsi_Host *shost;
	unsigned long flags;

	spin_lock_bh(&sess->lock);
	sc = cmd->scsi_cmd;
	if (!sc) {
		spin_unlock_bh(&sess->lock);
		PRINT_ALERT(sess->hba, "command already completed\n");
		return -EINVAL;
	}

	if ((sc->result & (DID_OK << 16)) != (DID_OK << 16))
		PRINT(sess->hba, "completing sc %p with bad status\n", sc);

	cmd->scsi_cmd = NULL;
	sess->active_cmd_count--;
	sc->SCp.ptr = NULL;
	bnx2i_free_cmd(sess, cmd);
	spin_unlock_bh(&sess->lock);

	shost = bnx2i_sess_get_shost(sess);
	spin_lock_irqsave(shost->host_lock, flags);
	sc->scsi_done(sc);
	sess->total_cmds_completed++;
	spin_unlock_irqrestore(shost->host_lock, flags);
	return 0;
}


/**
 * bnx2i_process_scsi_cmd_resp - this function handles scsi command
 * 			completion CQE processing
 *
 * @conn: 		iscsi connection
 * @cqe: 		pointer to newly DMA'ed CQE entry for processing
 *
 * process SCSI CMD Response CQE & complete the request to SCSI-ML
 */
static int bnx2i_process_scsi_cmd_resp(struct bnx2i_conn *conn,
				       struct cqe *cqe)
{
	struct iscsi_cmd_response *resp_cqe;
	struct bnx2i_cmd *cmd;
	struct scsi_cmnd *sc;
	u32 itt;

	resp_cqe = (struct iscsi_cmd_response *) cqe;

	bnx2i_update_cmd_sequence(conn->sess, resp_cqe->exp_cmd_sn,
				  resp_cqe->max_cmd_sn);

	itt = (resp_cqe->itt & ISCSI_CMD_RESPONSE_INDEX);
	cmd = get_cmnd(conn->sess, itt);
	spin_lock(&conn->sess->lock);
	if (!cmd || !cmd->scsi_cmd) {
		/* Driver might have failed due to LUN/TARGET RESET and target
		 * might be completing the command with check condition, it's ok
		 * to drop this completion
		 */
		PRINT_ALERT(conn->sess->hba, "scsi rsp ITT=%x not active\n",
			    itt);
		spin_unlock(&conn->sess->lock);
		return 0;
	}

	if (atomic_read(&cmd->cmd_state) == ISCSI_CMD_STATE_INITIATED) {
		/* remove command from the active list because it is
		 * gauranteed this command will be completed to SCSI-ML
		 */
		atomic_set(&cmd->cmd_state, ISCSI_CMD_STATE_CMPL_RCVD);
		list_del_init(&cmd->link);
	} else {
		PRINT_ALERT(conn->sess->hba, "%s: ITT=%x is being "
					     "aborted\n", __FUNCTION__, itt);
		/* Driver will hold on to CMD till it TMF completes */
	}
	spin_unlock(&conn->sess->lock);

	if (cmd->req.op_attr & ISCSI_CMD_REQUEST_READ) {
		conn->num_datain_pdus +=
			resp_cqe->task_stat.read_stat.num_data_ins;
		conn->total_data_octets_rcvd +=
			cmd->req.total_data_transfer_length;
		/* Rx PDU count */
		ADD_STATS_64(conn->sess->hba, rx_pdus,
			     resp_cqe->task_stat.read_stat.num_data_ins);
		ADD_STATS_64(conn->sess->hba, rx_bytes,
			     cmd->req.total_data_transfer_length);
	} else {
		conn->num_dataout_pdus +=
			resp_cqe->task_stat.write_stat.num_data_outs;
		conn->num_r2t_pdus +=
			resp_cqe->task_stat.write_stat.num_r2ts;
		conn->total_data_octets_sent +=
			cmd->req.total_data_transfer_length;
		/* Tx PDU count */
		ADD_STATS_64(conn->sess->hba, tx_pdus,
			     resp_cqe->task_stat.write_stat.num_data_outs);
		ADD_STATS_64(conn->sess->hba, tx_bytes,
			     cmd->req.total_data_transfer_length);
		ADD_STATS_64(conn->sess->hba, rx_pdus,
			     resp_cqe->task_stat.write_stat.num_r2ts);
	}

	sc = cmd->scsi_cmd;
	cmd->scsi_status_rcvd = 1;
	cmd->req.itt &= ISCSI_CMD_RESPONSE_INDEX;

	bnx2i_process_scsi_resp(cmd, resp_cqe);

	bnx2i_iscsi_unmap_sg_list(conn->sess->hba, cmd);

	if (atomic_read(&cmd->cmd_state) == ISCSI_CMD_STATE_CMPL_RCVD) {
		bnx2i_complete_cmd(conn->sess, cmd);
	} else if (atomic_read(&cmd->cmd_state)){
		/* cmd->cmd_state could be ABORT_PEND, ABORT_REQ ABORT_PEND
		 * ABORT_COMPL, CLEANUP_START, CLEANUP_PEND, CLEANUP_CMPL
		 * hold on to the command till Wait for TMF response
		 * is received
		 */
		atomic_set(&cmd->cmd_state, ISCSI_CMD_STATE_COMPLETED);
	} 

	return 0;
}

static void bnx2i_login_stats(struct bnx2i_hba *hba, u8 status_class,
			      u8 status_detail)
{
	switch (status_class) {
	case ISCSI_STATUS_CLS_SUCCESS:
		hba->login_stats.successful_logins++;
		break;
	case ISCSI_STATUS_CLS_REDIRECT: 
		switch (status_detail) {
		case ISCSI_LOGIN_STATUS_TGT_MOVED_TEMP:
		case ISCSI_LOGIN_STATUS_TGT_MOVED_PERM:
			hba->login_stats.login_redirect_responses++;
			break;
		default:
			hba->login_stats.login_failures++;
			break;
		}
		break;
	case ISCSI_STATUS_CLS_INITIATOR_ERR:
		hba->login_stats.login_failures++;
		switch (status_detail) {
		case ISCSI_LOGIN_STATUS_AUTH_FAILED:
		case ISCSI_LOGIN_STATUS_TGT_FORBIDDEN:
			hba->login_stats.login_authentication_failures++;
			break;
		default:
			/* Not sure, but treating all other class2 errors as
			   login negotiation failure */
			hba->login_stats.login_negotiation_failures++;
			break;
		}
		break;
	default:
	hba->login_stats.login_failures++;
		break;
	}
}

/**
 * bnx2i_process_login_resp - this function handles iscsi login response
 * 			CQE processing
 *
 * @conn: 		iscsi connection
 * @cqe: 		pointer to newly DMA'ed CQE entry for processing
 *
 * process Login Response CQE & complete it to open-iscsi user daemon
 */
static int bnx2i_process_login_resp(struct bnx2i_conn *conn, struct cqe *cqe)
{
	struct bnx2i_cmd *cmd;
	struct iscsi_login_response *login;
	struct iscsi_login_rsp *login_resp;
	u32 itt;
	u32 dword;
	int pld_len;
	int pad_len;

	login = (struct iscsi_login_response *) cqe;
	login_resp = (struct iscsi_login_rsp *) &conn->gen_pdu.resp_hdr;
	itt = (login->itt & ISCSI_LOGIN_RESPONSE_INDEX);

	cmd = get_cmnd(conn->sess, itt);
	if (unlikely(!cmd)) {
		PRINT_WARNING(conn->sess->hba, "itt=%x not valid\n", itt);
		return -EINVAL;
	}

	login_resp->opcode = login->op_code;
	login_resp->flags = login->response_flags;
	login_resp->max_version = login->version_max;
	login_resp->active_version = login->version_active;;
	login_resp->hlength = 0;

	dword = login->data_length;
	login_resp->dlength[2] = (u8) dword;
	login_resp->dlength[1] = (u8) (dword >> 8);
	login_resp->dlength[0] = (u8) (dword >> 16);

	memcpy(login_resp->isid, &login->isid_lo, 6);
	login_resp->tsih = htons(login->tsih);
	login_resp->itt = conn->gen_pdu.pdu_hdr.itt;
	login_resp->statsn = htonl(login->stat_sn);

	conn->sess->cmdsn = login->exp_cmd_sn;
	login_resp->exp_cmdsn = htonl(login->exp_cmd_sn);
	login_resp->max_cmdsn = htonl(login->max_cmd_sn);
	login_resp->status_class = login->status_class;
	login_resp->status_detail = login->status_detail;
	pld_len = login->data_length;
	bnx2i_login_stats(conn->sess->hba, login->status_class,
			  login->status_detail);
	conn->gen_pdu.resp_wr_ptr = conn->gen_pdu.resp_buf + pld_len;

	pad_len = 0;
	if (pld_len & 0x3)
		pad_len = 4 - (pld_len % 4);

	if (pad_len) {
		int i = 0;
		for (i = 0; i < pad_len; i++) {
			conn->gen_pdu.resp_wr_ptr[0] = 0;
			conn->gen_pdu.resp_wr_ptr++;
		}
	}
	cmd->iscsi_opcode = 0;

	bnx2i_indicate_login_resp(conn);
	return 0;
}

/**
 * bnx2i_process_tmf_resp - this function handles iscsi TMF response
 * 			CQE processing
 *
 * @conn: 		iscsi connection
 * @cqe: 		pointer to newly DMA'ed CQE entry for processing
 *
 * process iSCSI TMF Response CQE and wake up the driver eh thread.
 */
static int bnx2i_process_tmf_resp(struct bnx2i_conn *conn, struct cqe *cqe)
{
	u32 itt;
	struct bnx2i_cmd *cmd;
	struct bnx2i_cmd *aborted_cmd;
	struct iscsi_tmf_response *tmf_cqe;
	struct bnx2i_hba *hba = conn->sess->hba;

	tmf_cqe = (struct iscsi_tmf_response *) cqe;
	BNX2I_DBG(DBG_TMF, hba, "%s: ITT %x\n", __FUNCTION__, tmf_cqe->itt);
	itt = (tmf_cqe->itt & ISCSI_TMF_RESPONSE_INDEX);
	cmd = get_cmnd(conn->sess, itt);
	if (!cmd) {
		PRINT_ALERT(conn->sess->hba, "tmf_resp: ITT 0x%x "
			"is not active\n", tmf_cqe->itt);
		return -EINVAL;
	}

	bnx2i_update_cmd_sequence(conn->sess, tmf_cqe->exp_cmd_sn,
				  tmf_cqe->max_cmd_sn);

	if (conn->sess->recovery_state) {
		BNX2I_DBG(DBG_TMF, hba, "%s: ignore TMF resp as sess is already "
			 		"in recovery mode\n", __FUNCTION__);
		atomic_set(&cmd->cmd_state, ISCSI_CMD_STATE_FAILED);
		wake_up(&cmd->conn->sess->er_wait);
		return -1;
	}

	cmd->tmf_response = tmf_cqe->response; 
	if (cmd->tmf_func == ISCSI_TM_FUNC_LOGICAL_UNIT_RESET ||
	    cmd->tmf_func == ISCSI_TM_FUNC_TARGET_WARM_RESET) {
		if (tmf_cqe->response == ISCSI_TMF_RSP_COMPLETE)
			atomic_set(&cmd->cmd_state, ISCSI_CMD_STATE_COMPLETED);
		else
			atomic_set(&cmd->cmd_state, ISCSI_CMD_STATE_FAILED);
		BNX2I_DBG(DBG_TMF, hba, "%s : LUN/TGT RESET TMF RESP, "
					"status %x\n", __FUNCTION__,
					tmf_cqe->response);
		goto reset_done;
	} 

	BNX2I_DBG(DBG_TMF, hba, "%s : TMF ABORT CMD RESP, status %x\n",
		  __FUNCTION__, tmf_cqe->response);
	aborted_cmd = cmd->tmf_ref_cmd;

	if (tmf_cqe->response == ISCSI_TMF_RSP_COMPLETE) {
		if (aborted_cmd->scsi_cmd) {
			if (atomic_read(&aborted_cmd->cmd_state) ==
			    ISCSI_CMD_STATE_COMPLETED) {
				BNX2I_DBG(DBG_TMF, hba, 
					  "TMF: completed ITT=0x%x & "
					  "the TMF request to abort it\n",
					  aborted_cmd->req.itt);
				/* scsi_cmd->result is already set will wait
				 * for CMD_CLEANUP to complete before calling
				 * scsi_done()
				 */
			} else {
				aborted_cmd->scsi_cmd->result = DID_ABORT << 16;
				aborted_cmd->scsi_cmd->resid =
					aborted_cmd->scsi_cmd->request_bufflen;
			}
			atomic_set(&cmd->cmd_state, ISCSI_CMD_STATE_COMPLETED);
		}
	} else if (tmf_cqe->response == ISCSI_TMF_RSP_NO_TASK) {
		if (atomic_read(&aborted_cmd->cmd_state) ==
		    ISCSI_CMD_STATE_COMPLETED) {
			BNX2I_DBG(DBG_TMF, hba, "TMF: tgt completed ITT=0x%x "
						" while abort is pending\n",
						aborted_cmd->req.itt);
			/* Target did the right thing, so need to complete the
			 * command and return SUCCESS to SCSI-ML
			 */
		}
		if ((atomic_read(&aborted_cmd->cmd_state) ==
		     ISCSI_CMD_STATE_INITIATED) &&
		    (aborted_cmd->scsi_cmd == cmd->tmf_ref_sc)) {
			/* Something is messed up enter session recovery. */
			BNX2I_DBG(DBG_TMF, hba, "TMF_RESP: task "
						"still allegiant\n");
		}
		atomic_set(&cmd->cmd_state, ISCSI_CMD_STATE_COMPLETED);
	} else {
		BNX2I_DBG(DBG_TMF, hba, "TMF_RESP: failed, ITT 0x%x "
					"REF ITT 0x%x\n",
					cmd->req.itt, aborted_cmd->req.itt);
		atomic_set(&cmd->cmd_state, ISCSI_CMD_STATE_FAILED);
	}

reset_done:
	cmd->iscsi_opcode = 0;

	if (cmd->conn && cmd->conn->sess ) {

		wake_up(&cmd->conn->sess->er_wait);
	}
	return SUCCESS;
}

/**
 * bnx2i_process_logout_resp - this function handles iscsi logout response
 * 			CQE processing
 *
 * @conn: 		iscsi connection
 * @cqe: 		pointer to newly DMA'ed CQE entry for processing
 *
 * process iSCSI Logout Response CQE & make function call to
 * notify the user daemon.
 */
static int bnx2i_process_logout_resp(struct bnx2i_conn *conn, struct cqe *cqe)
{
	struct bnx2i_cmd *cmd;
	struct iscsi_logout_response *logout;
	struct iscsi_logout_rsp *logout_resp;
	u32 itt;

	logout = (struct iscsi_logout_response *) cqe;
	logout_resp = (struct iscsi_logout_rsp *) &conn->gen_pdu.resp_hdr;
	itt = (logout->itt & ISCSI_LOGOUT_RESPONSE_INDEX);

	cmd = get_cmnd(conn->sess, itt);
	if (!cmd || cmd != conn->sess->login_nopout_cmd)
		return -EINVAL;

	logout_resp->opcode = logout->op_code;
	logout_resp->flags = logout->response;
	logout_resp->hlength = 0;

	logout_resp->itt = conn->gen_pdu.pdu_hdr.itt;
	logout_resp->statsn = conn->gen_pdu.pdu_hdr.exp_statsn;
	logout_resp->exp_cmdsn = htonl(logout->exp_cmd_sn);
	logout_resp->max_cmdsn = htonl(logout->max_cmd_sn);

	logout_resp->t2wait = htonl(logout->time_to_wait);
	logout_resp->t2retain = htonl(logout->time_to_retain);

	conn->ep->teardown_mode = BNX2I_GRACEFUL_SHUTDOWN;
	cmd->iscsi_opcode = 0;

	bnx2i_indicate_logout_resp(conn);
	return 0;
}

/**
 * bnx2i_process_nopin_local_cmpl - this function handles iscsi nopin CQE
 * 			processing
 *
 * @conn: 		iscsi connection
 * @cqe: 		pointer to newly DMA'ed CQE entry for processing
 *
 * process iSCSI NOPIN local completion CQE, frees IIT and command structures
 */
void bnx2i_process_nopin_local_cmpl(struct bnx2i_conn *conn, struct cqe *cqe)
{
	u32 itt;
	struct bnx2i_cmd *cmd;
	struct bnx2i_hba *hba = conn->sess->hba;
	struct iscsi_nop_in_msg *nop_in;

	nop_in = (struct iscsi_nop_in_msg *) cqe;

	itt = (nop_in->itt & ISCSI_NOP_IN_MSG_INDEX);
	cmd = get_cmnd(conn->sess, itt);
	if (!cmd || cmd != conn->sess->nopout_resp_cmd) {
		BNX2I_DBG(DBG_ISCSI_NOP, hba, "nop_in_local: ITT %x "
					      "not active\n", itt);
		return;
	}
	cmd->iscsi_opcode = 0;
}

/**
 * bnx2i_process_tgt_noop_resp - this function handles iscsi nopout CQE
 * 			processing
 *
 * @conn: 		iscsi connection
 * @cqe: 		pointer to newly DMA'ed CQE entry for processing
 *
 * Process iSCSI target's nopin response to initiator's proactive nopout
 */
static int bnx2i_process_tgt_noop_resp(struct bnx2i_conn *conn, struct cqe *cqe)
{
	struct iscsi_nopin *nopin_hdr;
	u32 itt;
	struct bnx2i_cmd *cmd;
	struct iscsi_nop_in_msg *nop_in;

	nop_in = (struct iscsi_nop_in_msg *) cqe;
	itt = (nop_in->itt & ISCSI_NOP_IN_MSG_INDEX);
	nopin_hdr = (struct iscsi_nopin *) &conn->gen_pdu.nopin_hdr;

	cmd = get_cmnd(conn->sess, itt);
	
	if (!cmd)
		return -EINVAL;

	if (cmd != conn->sess->login_nopout_cmd) {
		PRINT_ALERT(conn->sess->hba, "nopout resp, invalid ITT, got %x"
				 " expected %x\n", itt, cmd->itt);
		return -EINVAL;
	}

	nopin_hdr->opcode = nop_in->op_code;
	nopin_hdr->flags = ISCSI_FLAG_CMD_FINAL;
	nopin_hdr->rsvd2 = nopin_hdr->rsvd3 = 0;
	nopin_hdr->itt = conn->gen_pdu.nopout_hdr.itt;
	nopin_hdr->ttt = ISCSI_RESERVED_TAG;

	memcpy(nopin_hdr->lun, conn->gen_pdu.nopout_hdr.lun, 8);
	nopin_hdr->statsn = conn->gen_pdu.nopout_hdr.exp_statsn;
	nopin_hdr->exp_cmdsn = htonl(nop_in->exp_cmd_sn);
	nopin_hdr->max_cmdsn = htonl(nop_in->max_cmd_sn);
	memset(nopin_hdr->rsvd4, 0x00, 12);

	bnx2i_process_nopin(conn, cmd, NULL, 0);
	return 0;
}


/**
 * bnx2i_unsol_pdu_adjust_rq - makes adjustments to RQ after unsolicited pdu
 * 			is received
 *
 * @conn: 		iscsi connection
 *
 * Firmware advances RQ producer index for every unsolicited PDU even if
 * 	payload data length is '0'. This function makes corresponding 
 * 	adjustments on the driver side to match this f/w behavior
 */
static void bnx2i_unsol_pdu_adjust_rq(struct bnx2i_conn *conn)
{
	char dummy_rq_data[2];
	bnx2i_get_rq_buf(conn, dummy_rq_data, 1);
	bnx2i_put_rq_buf(conn, 1);
}


/**
 * bnx2i_process_nopin_mesg - this function handles iscsi nopin CQE
 * 			processing
 *
 * @conn: 		iscsi connection
 * @cqe: 		pointer to newly DMA'ed CQE entry for processing
 *
 * process iSCSI target's proactive iSCSI NOPIN request
 */
static void bnx2i_process_nopin_mesg(struct bnx2i_conn *conn, struct cqe *cqe)
{
	u32 itt;
	u32 ttt;
#ifndef __VMKLNX__
	struct Scsi_Host *shost;
#endif
	struct iscsi_nop_in_msg *nop_in;

	nop_in = (struct iscsi_nop_in_msg *) cqe;
	itt = nop_in->itt;
	ttt = nop_in->ttt;

	bnx2i_update_cmd_sequence(conn->sess,
				  nop_in->exp_cmd_sn, nop_in->max_cmd_sn);

	conn->sess->last_noopin_processed = jiffies;
	conn->sess->noopin_processed_count++;

	if (itt == (u16) ISCSI_RESERVED_TAG) {
		struct bnx2i_cmd *cmd;

		bnx2i_unsol_pdu_adjust_rq(conn);
		if (ttt == ISCSI_RESERVED_TAG)
			return;
conn->sess->tgt_noopin_count++;

		cmd = conn->sess->nopout_resp_cmd;
		if (!cmd) {
			/* should not happen as nopout-resp command is reserved
			 * during conn_bind()
			 */
			return;
		}
		cmd->conn = conn;
		cmd->scsi_cmd = NULL;
		cmd->req.total_data_transfer_length = 0;
		cmd->iscsi_opcode = ISCSI_OP_NOOP_OUT;
		cmd->ttt = ttt;

		/* requires reply in the form of Nop-Out */
		if (nop_in->data_length)
			PRINT_ALERT(conn->sess->hba,
				    "Tgt NOPIN with dlen > 0\n");

		atomic_set(&conn->sess->nop_resp_pending, 1);

		if (atomic_read(&conn->worker_enabled)) {
#ifdef __VMKLNX__
			atomic_set(&conn->lastSched,1);
			tasklet_schedule(&conn->conn_tasklet);
#else
			shost = bnx2i_conn_get_shost(conn);
			scsi_queue_work(shost, &conn->conn_worker);
#endif	/* __VMKLNX__ */
		}
	} else	/* target's reply to initiator's Nop-Out */
		bnx2i_process_tgt_noop_resp(conn, cqe);
}


/**
 * bnx2i_process_async_mesg - this function handles iscsi async message
 * 			processing
 *
 * @conn: 		iscsi connection
 * @cqe: 		pointer to newly DMA'ed CQE entry for processing
 *
 * process iSCSI ASYNC Message
 */
static void bnx2i_process_async_mesg(struct bnx2i_conn *conn, struct cqe *cqe)
{
	struct iscsi_async_msg *async_cqe;
	struct iscsi_async *async_hdr;
	u8 async_event;

	bnx2i_unsol_pdu_adjust_rq(conn);

	async_cqe = (struct iscsi_async_msg *) cqe;
	async_event = async_cqe->async_event;

	if (async_event == ISCSI_ASYNC_MSG_SCSI_EVENT) {
		PRINT_ALERT(conn->sess->hba,
			    "async: scsi events not supported\n");
		return;
	}

	async_hdr = (struct iscsi_async *) &conn->gen_pdu.async_hdr;
	async_hdr->opcode = async_cqe->op_code;
	async_hdr->flags = 0x80;
	async_hdr->rsvd2[0] = async_hdr->rsvd2[1] = async_hdr->rsvd3 = 0;
	async_hdr->dlength[0] = async_hdr->dlength[1] = async_hdr->dlength[2] = 0;

	memcpy(async_hdr->lun, async_cqe->lun, 8);
	async_hdr->statsn = htonl(0);
	async_hdr->exp_cmdsn = htonl(async_cqe->exp_cmd_sn);
	async_hdr->max_cmdsn = htonl(async_cqe->max_cmd_sn);

	async_hdr->async_event = async_cqe->async_event;
	async_hdr->async_vcode = async_cqe->async_vcode;

	async_hdr->param1 = htons(async_cqe->param1);
	async_hdr->param2 = htons(async_cqe->param2);
	async_hdr->param3 = htons(async_cqe->param3);
	async_hdr->rsvd5[0] = async_hdr->rsvd5[1] = 0;
	async_hdr->rsvd5[2] = async_hdr->rsvd5[3] = 0;

	bnx2i_indicate_async_mesg(conn);
}


/**
 * bnx2i_process_reject_mesg - process iscsi reject pdu
 *
 * @conn: 		iscsi connection
 * @cqe: 		pointer to newly DMA'ed CQE entry for processing
 *
 * process iSCSI REJECT message
 */
static void bnx2i_process_reject_mesg(struct bnx2i_conn *conn, struct cqe *cqe)
{
	struct iscsi_reject_msg *reject;
	char rej_pdu[BNX2I_RQ_WQE_SIZE];
	int rej_data_len, idx;

	reject = (struct iscsi_reject_msg *) cqe;
	rej_data_len = reject->data_length;
	if (rej_data_len) {
		bnx2i_get_rq_buf(conn, rej_pdu, rej_data_len);
		bnx2i_put_rq_buf(conn, 1);
		PRINT_ALERT(conn->sess->hba, "printing rejected PDU contents");
		idx = 0;
		PRINT_ALERT(conn->sess->hba, "\n[%x]: ", idx);
		while (idx <= rej_data_len) {
			PRINT_ALERT(conn->sess->hba, "%x ", rej_pdu[idx++]);
			if (!(idx % 8))
				PRINT_ALERT(conn->sess->hba, "\n[%x]: ", idx);
		}
	} else
		bnx2i_unsol_pdu_adjust_rq(conn);

	bnx2i_recovery_que_add_sess(conn->sess->hba, conn->sess);
}

/**
 * bnx2i_process_cmd_cleanup_resp - process scsi command clean-up completion
 *
 * @conn: 		iscsi connection
 * @cqe: 		pointer to newly DMA'ed CQE entry for processing
 *
 * process command cleanup response CQE during conn shutdown or error recovery
 */
static void bnx2i_process_cmd_cleanup_resp(struct bnx2i_conn *conn,
					   struct cqe *cqe)
{
	u32 itt;
	struct bnx2i_cmd *cmd;
	struct iscsi_cleanup_response *cmd_clean_rsp;
	struct bnx2i_hba *hba = conn->sess->hba;

	cmd_clean_rsp = (struct iscsi_cleanup_response *) cqe;
	if (cmd_clean_rsp->status || cmd_clean_rsp->err_code)
		hba->task_cleanup_failed++;

	itt = (cmd_clean_rsp->itt & ISCSI_CLEANUP_RESPONSE_INDEX);
	cmd = get_cmnd(conn->sess, itt);
	/* there should not be any synchronization issue, 'Cuz only TMF process
	 * should be waiting on this response. There cannot be race b/w this &
	 * SCSI completion because completion is either processed by now and if
	 * SCSI_RESP arrives after CMD_CLEANUP, F/W will deem it as a protocol
	 * violation and forces session recovery
	 */
	if (!cmd || !cmd->scsi_cmd) {
		BNX2I_DBG(DBG_ITT_CLEANUP, hba, 
			  "cmd clean, ITT %x not active\n", itt);
		/* may be completion came before cleanup response */
	} else {
		atomic_set(&cmd->cmd_state, ISCSI_CMD_STATE_CLEANUP_CMPL);
		if (cmd->scsi_status_rcvd) {
			/** cmd completed while CMD_CLEANUP was pending
			 */
			bnx2i_complete_cmd(conn->sess, cmd);
		} else {
			bnx2i_fail_cmd(conn->sess, cmd);
		}
	}
	conn->sess->cmd_cleanup_cmpl++;
	barrier();
	wake_up(&conn->sess->er_wait);
}

void bnx2i_print_sqe(struct bnx2i_conn *conn)
{
	struct qp_info *qp = &conn->ep->qp;
	struct iscsi_nop_out_request *nopout_wqe;
	int count = conn->sess->sq_size;

	PRINT_ALERT(conn->sess->hba, "dump SQE conn %p, sn %x, max_sn %x\n",
		conn, conn->sess->cmdsn, conn->sess->exp_cmdsn);
	PRINT_ALERT(conn->sess->hba, "\t: OPCODE, ATTR, DLEN, ITT, "
			"TTT, SN, flags\n");

	nopout_wqe = (struct iscsi_nop_out_request *) conn->ep->qp.sq_prod_qe;
	while (count--) {
		PRINT_ALERT(conn->sess->hba, "\t: %.2x, %.4x, %.4x, %.2x, "
			"%.4x, %.4x, %.2x\n",
 			nopout_wqe->op_code, nopout_wqe->op_attr,
			nopout_wqe->data_length, nopout_wqe->itt,
			nopout_wqe->ttt, nopout_wqe->cmd_sn, nopout_wqe->flags);
		if (qp->sq_first_qe == (struct sqe *)nopout_wqe)
			nopout_wqe = (struct iscsi_nop_out_request *) qp->sq_last_qe;
		else
			nopout_wqe--;
	}
}

void bnx2i_print_cqe(struct bnx2i_conn *conn)
{
	struct qp_info *qp = &conn->ep->qp;
	volatile struct iscsi_nop_in_msg *nopin;
	struct cqe *cq_cons;
	int count = qp->cqe_size;

	PRINT_ALERT(conn->sess->hba, "dump CQE conn %p, exp_seq_sn %x\n",
		conn, qp->cqe_exp_seq_sn);
	PRINT_ALERT(conn->sess->hba,
			"\t: OPCODE, EXP_SN, MAX_SN, ITT, TTT, CQSN\n");

	cq_cons = qp->cq_cons_qe;
	while (count--) {
		nopin = (struct iscsi_nop_in_msg *) cq_cons;
		PRINT_ALERT(conn->sess->hba,
			"\t: %.2x, %.4x, %.4x, %.2x, %.4x, %.4x\n",
 			nopin->op_code, nopin->exp_cmd_sn, nopin->max_cmd_sn,
			nopin->itt, nopin->ttt, nopin->cq_req_sn);
		if (qp->cq_first_qe == cq_cons)
			cq_cons = qp->cq_last_qe;
		else
			cq_cons--;
	}
}


/**
 * bnx2i_process_new_cqes - process newly DMA'ed CQE's
 *
 * @conn: 		iscsi connection
 * @soft_irq: 		initial though was iscsi logout and command cleanup reponse will
 *			be directly processed in softirq. But I guess driver still needs 
 *			flush the complete queue and cannot be picky
 * @cqes_per_work:	number of cqes to process per call
 *
 * this function is called by generic KCQ handler to process all pending CQE's
 */
int bnx2i_process_new_cqes(struct bnx2i_conn *conn, int soft_irq,
			   int cqes_per_work)
{
	struct qp_info *qp;
	volatile struct iscsi_nop_in_msg *nopin;
	volatile u32 *sess_state = &conn->sess->state;
	struct cqe *cq_cons;
	int num_cqe = 0;
	int keep_processing = 1;
	
	if (!conn->ep)
		return 0;

	qp = &conn->ep->qp;
	while (keep_processing > 0) {
		cq_cons = qp->cq_cons_qe;
		barrier();
		nopin = (struct iscsi_nop_in_msg *) qp->cq_cons_qe;
		if ((nopin->cq_req_sn != qp->cqe_exp_seq_sn) ||
		    (*sess_state == BNX2I_SESS_IN_SHUTDOWN)) {
			keep_processing = 0;
			break;
		}
		if (num_cqe >= cqes_per_work) {
			break;
		}

		num_cqe++;
		if (nopin->op_code == ISCSI_OP_SCSI_CMD_RSP) {
			conn->num_scsi_resp_pdus++;
			conn->sess->total_cmds_completed_by_chip++;
			bnx2i_process_scsi_cmd_resp(conn, cq_cons);
		} else if (nopin->op_code == ISCSI_OP_SCSI_DATA_IN) {
			conn->sess->total_cmds_completed_by_chip++;
			bnx2i_process_scsi_cmd_resp(conn, cq_cons);
		} else if (nopin->op_code == ISCSI_OP_LOGIN_RSP) {
			conn->num_login_resp_pdus++;
			bnx2i_process_login_resp(conn, cq_cons);
		} else if (nopin->op_code == ISCSI_OP_LOGOUT_RSP) {
			conn->num_logout_resp_pdus++;
			bnx2i_process_logout_resp(conn, cq_cons);
			keep_processing = -EAGAIN;
		} else if (nopin->op_code == ISCSI_OP_SCSI_TMFUNC_RSP) {
			conn->num_tmf_resp_pdus++;
			BNX2I_DBG(DBG_TMF, conn->sess->hba,
				  "%s: TMF_RESP CQE, cid %d\n",
				  __FUNCTION__, conn->ep->ep_iscsi_cid);
			bnx2i_process_tmf_resp(conn, cq_cons);
			/* exit out of completion processing inorder to
			 * TMF condition asap
			 
			keep_processing = -EAGAIN;
			*/
		} else if (nopin->op_code == ISCSI_OP_NOOP_IN) {
			conn->num_nopin_pdus++;
			bnx2i_process_nopin_mesg(conn, cq_cons);
		} else if (nopin->op_code ==
			   ISCSI_OPCODE_NOPOUT_LOCAL_COMPLETION) {
			bnx2i_process_nopin_local_cmpl(conn, cq_cons);
		} else if (nopin->op_code == ISCSI_OP_ASYNC_EVENT) {

			conn->num_async_pdus++;
			bnx2i_process_async_mesg(conn, cq_cons);
		} else if (nopin->op_code == ISCSI_OP_REJECT) {
			PRINT(conn->sess->hba,
				"%s: ISCSI REJECT CQE, cid %d\n",
				__FUNCTION__, conn->ep->ep_iscsi_cid);
			conn->num_reject_pdus++;
			bnx2i_process_reject_mesg(conn, cq_cons);
		} else if (nopin->op_code == ISCSI_OPCODE_CLEANUP_RESPONSE) {
			bnx2i_process_cmd_cleanup_resp(conn, cq_cons);
		} else
			PRINT_ERR(conn->sess->hba,
				  "unknown opcode 0x%x\n", nopin->op_code);

		if (nopin->op_code != ISCSI_OP_SCSI_CMD_RSP ||
		    nopin->op_code !=ISCSI_OP_SCSI_DATA_IN) {
			ADD_STATS_64(conn->sess->hba, rx_pdus, 1);
			ADD_STATS_64(conn->sess->hba, rx_bytes, nopin->data_length);
		}
		
		/* clear out in production version only, till beta keep opcode
		 * field intact, will be helpful in debugging (context dump)
		nopin->op_code = 0;
		 */
		qp->cqe_exp_seq_sn++;
		if (qp->cqe_exp_seq_sn == (qp->cqe_size * 2 + 1))
			qp->cqe_exp_seq_sn = ISCSI_INITIAL_SN;

		if (qp->cq_cons_qe == qp->cq_last_qe) {
			qp->cq_cons_qe = qp->cq_first_qe;
			qp->cq_cons_idx = 0;
		} else {
			qp->cq_cons_qe++;
			qp->cq_cons_idx++;
		}
	}
	return keep_processing;
}

/**
 * bnx2i_fastpath_notification - process global event queue (KCQ)
 *
 * @hba: 		adapter structure pointer
 * @new_cqe_kcqe: 	pointer to newly DMA'ed KCQE entry
 *
 * Fast path event notification handler, KCQ entry carries context id
 *	of the connection that has 1 or more pending CQ entries
 */
static void bnx2i_fastpath_notification(struct bnx2i_hba *hba,
					struct iscsi_kcqe *new_cqe_kcqe)
{
	u32 iscsi_cid;
	struct bnx2i_conn *conn;
#ifndef __VMKLNX__
	struct Scsi_Host *shost;
#endif

	iscsi_cid = new_cqe_kcqe->iscsi_conn_id;
	conn = bnx2i_get_conn_from_id(hba, iscsi_cid);

	if (unlikely(!conn)) {
		PRINT_ERR(hba, "cid #%x not valid\n", iscsi_cid);
		return;
	}
	if (unlikely(!conn->ep)) {
		PRINT_ERR(hba, "cid #%x - ep not bound\n", iscsi_cid);
		return;
	}

	/* propogate activity information to iscsi transport layer */
	bnx2i_update_conn_activity_counter(conn);

	atomic_inc(&conn->ep->fp_kcqe_events);
	if (atomic_read(&conn->worker_enabled)) {
#ifdef __VMKLNX__
		atomic_set(&conn->lastSched,2);
		tasklet_schedule(&conn->conn_tasklet);
#else
		shost = bnx2i_conn_get_shost(conn);
		scsi_queue_work(shost, &conn->conn_worker);
#endif	/* __VMKLNX__ */
	} else {
		/* command cleanup completions needs to be completed for
		 * flush command queue to drain out all active commands else
		 * this could lead to deadlock and PCPU lockup in ESX
		 */
		/* no that tasklet will run till ep_disconnect() is called,
		 * this may not be required - Anil
		 */
		bnx2i_process_new_cqes(conn, 1, conn->ep->qp.cqe_size);
		bnx2i_arm_cq_event_coalescing(conn->ep, CNIC_ARM_CQE);
	}
}


/**
 * bnx2i_process_update_conn_cmpl - process iscsi conn update completion KCQE
 *
 * @hba: 		adapter structure pointer
 * @update_kcqe: 	kcqe pointer
 *
 * CONN_UPDATE completion handler, this completes iSCSI connection FFP migration
 */
static void bnx2i_process_update_conn_cmpl(struct bnx2i_hba *hba,
					   struct iscsi_kcqe *update_kcqe)
{
	struct bnx2i_conn *conn;
	u32 iscsi_cid;

	iscsi_cid = update_kcqe->iscsi_conn_id;
	conn = bnx2i_get_conn_from_id(hba, iscsi_cid);

	if (unlikely(!conn)) {
		PRINT_ALERT(hba, "conn_update: cid %x not valid\n", iscsi_cid);
		return;
	}
	if (unlikely(!conn->ep)) {
		PRINT_ALERT(hba, "cid %x does not have ep bound\n", iscsi_cid);
		return;
	}

	if (update_kcqe->completion_status) {
		PRINT_ALERT(hba, "request failed cid %x\n", iscsi_cid);
		conn->ep->state = EP_STATE_ULP_UPDATE_FAILED;
	} else
		conn->ep->state = EP_STATE_ULP_UPDATE_COMPL;

	wake_up_interruptible(&conn->ep->ofld_wait);
}


/**
 * bnx2i_recovery_que_add_conn - add connection to recovery queue
 *
 * @hba: 		adapter structure pointer
 * @conn: 		iscsi connection
 *
 * Add connection to recovery queue and schedule adapter eh worker
 */
static void bnx2i_recovery_que_add_sess(struct bnx2i_hba *hba,
					struct bnx2i_sess *sess)
{
	int prod_idx = hba->sess_recov_prod_idx;

	/* Start session recovery even when TMF is active and
	 * remote FIN/RST is received
	 */
	if (sess->recovery_state)
		return;

	hba->login_stats.session_failures++;
	spin_lock(&hba->lock);
	hba->sess_recov_list[prod_idx] = sess;
	if (hba->sess_recov_max_idx == hba->sess_recov_prod_idx)
		hba->sess_recov_prod_idx = 0;
	else
		hba->sess_recov_prod_idx++;
	spin_unlock(&hba->lock);

	schedule_work(&hba->err_rec_task);
}


/**
 * bnx2i_process_tcp_error - process error notification on a given connection
 *
 * @hba: 		adapter structure pointer
 * @tcp_err: 		tcp error kcqe pointer
 *
 * handles tcp level error notifications from FW.
 */
static void bnx2i_process_tcp_error(struct bnx2i_hba *hba,
				    struct iscsi_kcqe *tcp_err)
{
	struct bnx2i_conn *conn;
	u32 iscsi_cid;

	iscsi_cid = tcp_err->iscsi_conn_id;
	conn = bnx2i_get_conn_from_id(hba, iscsi_cid);

	if (unlikely(!conn)) {
		PRINT_ERR(hba, "cid 0x%x not valid\n", iscsi_cid);
		return;
	}

	PRINT_ALERT(hba, "cid 0x%x had TCP errors, error code 0x%x\n",
			  iscsi_cid, tcp_err->completion_status);
	bnx2i_recovery_que_add_sess(conn->sess->hba, conn->sess);
}


/**
 * bnx2i_process_iscsi_error - process error notification on a given connection
 *
 * @hba: 		adapter structure pointer
 * @iscsi_err: 		iscsi error kcqe pointer
 *
 * handles iscsi error notifications from the FW. Firmware based in initial
 *	handshake classifies iscsi protocol / TCP rfc violation into either
 *	warning or error indications. If indication is of "Error" type, driver
 *	will initiate session recovery for that connection/session. For
 *	"Warning" type indication, driver will put out a system log message
 *	(there will be only one message for each type for the life of the
 *	session, this is to avoid un-necessarily overloading the system)
 */
static void bnx2i_process_iscsi_error(struct bnx2i_hba *hba,
				      struct iscsi_kcqe *iscsi_err)
{
	struct bnx2i_conn *conn;
	u32 iscsi_cid;
	char warn_notice[] = "iscsi_warning";
	char error_notice[] = "iscsi_error";
	char additional_notice[64];
	char *message;
	int need_recovery;
	u64 err_mask64;
	struct Scsi_Host *shost;

	iscsi_cid = iscsi_err->iscsi_conn_id;
	conn = bnx2i_get_conn_from_id(hba, iscsi_cid);

	if (unlikely(!conn)) {
		PRINT_ERR(hba, "cid 0x%x not valid\n", iscsi_cid);
		return;
	}

	err_mask64 = (0x1ULL << iscsi_err->completion_status);

	if (err_mask64 & iscsi_error_mask) {
		need_recovery = 0;
		message = warn_notice;
	} else {
		need_recovery = 1;
		message = error_notice;
	}

	switch (iscsi_err->completion_status) {
	case ISCSI_KCQE_COMPLETION_STATUS_HDR_DIG_ERR:
		strcpy(additional_notice, "hdr digest err");
		hba->login_stats.digest_errors++;
		break;
	case ISCSI_KCQE_COMPLETION_STATUS_DATA_DIG_ERR:
		strcpy(additional_notice, "data digest err");
		hba->login_stats.digest_errors++;
		break;
	case ISCSI_KCQE_COMPLETION_STATUS_PROTOCOL_ERR_OPCODE:
		strcpy(additional_notice, "wrong opcode rcvd");
		break;
	case ISCSI_KCQE_COMPLETION_STATUS_PROTOCOL_ERR_AHS_LEN:
		strcpy(additional_notice, "AHS len > 0 rcvd");
		break;
	case ISCSI_KCQE_COMPLETION_STATUS_PROTOCOL_ERR_ITT:
		strcpy(additional_notice, "invalid ITT rcvd");
		break;
	case ISCSI_KCQE_COMPLETION_STATUS_PROTOCOL_ERR_STATSN:
		strcpy(additional_notice, "wrong StatSN rcvd");
		break;
	case ISCSI_KCQE_COMPLETION_STATUS_PROTOCOL_ERR_EXP_DATASN:
		strcpy(additional_notice, "wrong DataSN rcvd");
		break;
	case ISCSI_KCQE_COMPLETION_STATUS_PROTOCOL_ERR_PEND_R2T	:
		strcpy(additional_notice, "pend R2T violation");
		break;
	case ISCSI_KCQE_COMPLETION_STATUS_PROTOCOL_ERR_O_U_0:
		strcpy(additional_notice, "ERL0, UO");
		break;
	case ISCSI_KCQE_COMPLETION_STATUS_PROTOCOL_ERR_O_U_1:
		strcpy(additional_notice, "ERL0, U1");
		break;
	case ISCSI_KCQE_COMPLETION_STATUS_PROTOCOL_ERR_O_U_2:
		strcpy(additional_notice, "ERL0, U2");
		break;
	case ISCSI_KCQE_COMPLETION_STATUS_PROTOCOL_ERR_O_U_3:
		strcpy(additional_notice, "ERL0, U3");
		break;
	case ISCSI_KCQE_COMPLETION_STATUS_PROTOCOL_ERR_O_U_4:
		strcpy(additional_notice, "ERL0, U4");
		break;
	case ISCSI_KCQE_COMPLETION_STATUS_PROTOCOL_ERR_O_U_5:
		strcpy(additional_notice, "ERL0, U5");
		break;
	case ISCSI_KCQE_COMPLETION_STATUS_PROTOCOL_ERR_O_U_6:
		strcpy(additional_notice, "ERL0, U6");
		break;
	case ISCSI_KCQE_COMPLETION_STATUS_PROTOCOL_ERR_REMAIN_RCV_LEN:
		strcpy(additional_notice, "invalid resi len");
		break;
	case ISCSI_KCQE_COMPLETION_STATUS_PROTOCOL_ERR_MAX_RCV_PDU_LEN:
		strcpy(additional_notice, "MRDSL violation");
		break;
	case ISCSI_KCQE_COMPLETION_STATUS_PROTOCOL_ERR_F_BIT_ZERO:
		strcpy(additional_notice, "F-bit not set");
		break;
	case ISCSI_KCQE_COMPLETION_STATUS_PROTOCOL_ERR_TTT_NOT_RSRV:
		strcpy(additional_notice, "invalid TTT");
		break;
	case ISCSI_KCQE_COMPLETION_STATUS_PROTOCOL_ERR_DATASN:
		strcpy(additional_notice, "invalid DataSN");
		break;
	case ISCSI_KCQE_COMPLETION_STATUS_PROTOCOL_ERR_REMAIN_BURST_LEN:
		strcpy(additional_notice, "burst len violation");
		break;
	case ISCSI_KCQE_COMPLETION_STATUS_PROTOCOL_ERR_BUFFER_OFF:
		strcpy(additional_notice, "buf offset violation");
		break;
	case ISCSI_KCQE_COMPLETION_STATUS_PROTOCOL_ERR_LUN:
		strcpy(additional_notice, "invalid LUN field");
		break;
	case ISCSI_KCQE_COMPLETION_STATUS_PROTOCOL_ERR_R2TSN:
		strcpy(additional_notice, "invalid R2TSN field");
		break;
#define BNX2I_ERR_DESIRED_DATA_TRNS_LEN_0 	\
	ISCSI_KCQE_COMPLETION_STATUS_PROTOCOL_ERR_DESIRED_DATA_TRNS_LEN_0
	case BNX2I_ERR_DESIRED_DATA_TRNS_LEN_0:
		strcpy(additional_notice, "invalid cmd len1");
		break;
#define BNX2I_ERR_DESIRED_DATA_TRNS_LEN_1 	\
	ISCSI_KCQE_COMPLETION_STATUS_PROTOCOL_ERR_DESIRED_DATA_TRNS_LEN_1
	case BNX2I_ERR_DESIRED_DATA_TRNS_LEN_1:
		strcpy(additional_notice, "invalid cmd len2");
		break;
	case ISCSI_KCQE_COMPLETION_STATUS_PROTOCOL_ERR_PEND_R2T_EXCEED:
		strcpy(additional_notice,
		       "pend r2t exceeds MaxOutstandingR2T value");
		break;
	case ISCSI_KCQE_COMPLETION_STATUS_PROTOCOL_ERR_TTT_IS_RSRV:
		strcpy(additional_notice, "TTT is rsvd");
		break;
	case ISCSI_KCQE_COMPLETION_STATUS_PROTOCOL_ERR_MAX_BURST_LEN:
		strcpy(additional_notice, "MBL violation");
		break;
#define BNX2I_ERR_DATA_SEG_LEN_NOT_ZERO 	\
	ISCSI_KCQE_COMPLETION_STATUS_PROTOCOL_ERR_DATA_SEG_LEN_NOT_ZERO
	case BNX2I_ERR_DATA_SEG_LEN_NOT_ZERO:
		strcpy(additional_notice, "data seg len != 0");
		break;
	case ISCSI_KCQE_COMPLETION_STATUS_PROTOCOL_ERR_REJECT_PDU_LEN:
		strcpy(additional_notice, "reject pdu len error");
		break;
	case ISCSI_KCQE_COMPLETION_STATUS_PROTOCOL_ERR_ASYNC_PDU_LEN:
		strcpy(additional_notice, "async pdu len error");
		break;
	case ISCSI_KCQE_COMPLETION_STATUS_PROTOCOL_ERR_NOPIN_PDU_LEN:
		strcpy(additional_notice, "nopin pdu len error");
		break;
#define BNX2_ERR_PEND_R2T_IN_CLEANUP			\
	ISCSI_KCQE_COMPLETION_STATUS_PROTOCOL_ERR_PEND_R2T_IN_CLEANUP
	case BNX2_ERR_PEND_R2T_IN_CLEANUP:
		strcpy(additional_notice, "pend r2t in cleanup");
		break;

	case ISCI_KCQE_COMPLETION_STATUS_TCP_ERROR_IP_FRAGMENT:
		strcpy(additional_notice, "IP fragments rcvd");
		break;
	case ISCI_KCQE_COMPLETION_STATUS_TCP_ERROR_IP_OPTIONS:
		strcpy(additional_notice, "IP options error");
		break;
	case ISCI_KCQE_COMPLETION_STATUS_TCP_ERROR_URGENT_FLAG:
		strcpy(additional_notice, "urgent flag error");
		break;
	default:
		PRINT_ERR(hba, "iscsi_err - unknown err %x\n",
			  iscsi_err->completion_status);
	}

	if (need_recovery) {
 		shost = bnx2i_conn_get_shost(conn);
		PRINT_ALERT(hba, "%s - %s\n", message, additional_notice);

		PRINT_ALERT(hba, "conn_err - hostno %d conn %p, "
				  "iscsi_cid %x cid %x\n",
			   	  shost->host_no, conn,
			   	  conn->ep->ep_iscsi_cid,
				  conn->ep->ep_cid);
		bnx2i_recovery_que_add_sess(conn->sess->hba, conn->sess);
	} else
		if (!test_and_set_bit(iscsi_err->completion_status,
				      (void *) &conn->sess->violation_notified))
			PRINT_ALERT(hba, "%s - %s\n",
					  message, additional_notice);
}


/**
 * bnx2i_process_conn_destroy_cmpl - process iscsi conn destroy completion
 *
 * @hba: 		adapter structure pointer
 * @conn_destroy: 	conn destroy kcqe pointer
 *
 * handles connection destroy completion request.
 */
static void bnx2i_process_conn_destroy_cmpl(struct bnx2i_hba *hba,
					    struct iscsi_kcqe *conn_destroy)
{
	struct bnx2i_endpoint *ep;

	ep = bnx2i_find_ep_in_destroy_list(hba, conn_destroy->iscsi_conn_id);
	if (unlikely(!ep)) {
		PRINT_ALERT(hba, "bnx2i_conn_destroy_cmpl: no pending "
				 "offload request, unexpected complection\n");
		return;
	}

	if (unlikely(hba != ep->hba)) {
		PRINT_ALERT(hba, "conn destroy- error hba mis-match\n");
		return;
	}

	if (conn_destroy->completion_status) {
		PRINT_ALERT(hba, "conn_destroy_cmpl: op failed\n");
		ep->state = EP_STATE_CLEANUP_FAILED;
	} else
		ep->state = EP_STATE_CLEANUP_CMPL;
	wake_up_interruptible(&ep->ofld_wait);

	if (test_bit(ADAPTER_STATE_GOING_DOWN, &hba->adapter_state))
		wake_up_interruptible(&hba->eh_wait);
}


/**
 * bnx2i_process_ofld_cmpl - process initial iscsi conn offload completion
 *
 * @hba: 		adapter structure pointer
 * @ofld_kcqe: 		conn offload kcqe pointer
 *
 * handles initial connection offload completion, ep_connect() thread is
 *	woken-up to continue with LLP connect process
 */
static void bnx2i_process_ofld_cmpl(struct bnx2i_hba *hba,
				    struct iscsi_kcqe *ofld_kcqe)
{
	u32 cid_addr;
	struct bnx2i_endpoint *ep;
	u32 cid_num;

	ep = bnx2i_find_ep_in_ofld_list(hba, ofld_kcqe->iscsi_conn_id);
	if (unlikely(!ep)) {
		PRINT_ALERT(hba, "ofld_cmpl: no pend offload request\n");
		return;
	}

	if (unlikely(hba != ep->hba)) {
		PRINT_ALERT(hba, "ofld_cmpl: error hba mis-match\n");
		return;
	}

	if (ofld_kcqe->completion_status) {
		ep->state = EP_STATE_OFLD_FAILED;
		if (ofld_kcqe->completion_status ==
		    ISCSI_KCQE_COMPLETION_STATUS_CTX_ALLOC_FAILURE)
			PRINT_ALERT(hba, "unable to allocate"
					  " iSCSI context resources\n");
		else if (ofld_kcqe->completion_status ==
			 ISCSI_KCQE_COMPLETION_STATUS_INVALID_OPCODE)
			PRINT_ALERT(hba, "ofld1 cmpl - invalid opcode\n");
		else if (ofld_kcqe->completion_status ==
			 ISCSI_KCQE_COMPLETION_STATUS_CID_BUSY) {
			/* error status code valid only for 5771x chipset */
			ep->state = EP_STATE_OFLD_FAILED_CID_BUSY;
		} else
			PRINT_ALERT(hba, "ofld1 cmpl - invalid error code %d\n",
				    ofld_kcqe->completion_status);
	} else {
		ep->state = EP_STATE_OFLD_COMPL;
		cid_addr = ofld_kcqe->iscsi_conn_context_id;
		cid_num = bnx2i_get_cid_num(ep);
		ep->ep_cid = cid_addr;
		ep->qp.ctx_base = NULL;
	}
	wake_up_interruptible(&ep->ofld_wait);
}

/**
 * bnx2i_indicate_kcqe - process iscsi conn update completion KCQE
 *
 * @hba: 		adapter structure pointer
 * @update_kcqe: 	kcqe pointer
 *
 * Generic KCQ event handler/dispatcher
 */
static void bnx2i_indicate_kcqe(void *context, struct kcqe *kcqe[],
				u32 num_cqe)
{
	struct bnx2i_hba *hba = (struct bnx2i_hba *) context;
	int i = 0;
	struct iscsi_kcqe *ikcqe = NULL;

	while (i < num_cqe) {
		ikcqe = (struct iscsi_kcqe *) kcqe[i++];

		if (ikcqe->op_code ==
		    ISCSI_KCQE_OPCODE_CQ_EVENT_NOTIFICATION)
			bnx2i_fastpath_notification(hba, ikcqe);
		else if (ikcqe->op_code == ISCSI_KCQE_OPCODE_OFFLOAD_CONN)
			bnx2i_process_ofld_cmpl(hba, ikcqe);
		else if (ikcqe->op_code == ISCSI_KCQE_OPCODE_UPDATE_CONN)
			bnx2i_process_update_conn_cmpl(hba, ikcqe);
		else if (ikcqe->op_code == ISCSI_KCQE_OPCODE_INIT) {
			if (ikcqe->completion_status !=
			    ISCSI_KCQE_COMPLETION_STATUS_SUCCESS)
				bnx2i_iscsi_license_error(hba, ikcqe->\
							  completion_status);
			else {
				set_bit(ADAPTER_STATE_UP, &hba->adapter_state);
				bnx2i_get_link_state(hba);
				PRINT_INFO(hba, "[%.2x:%.2x.%.2x]: "
						 "ISCSI_INIT passed\n",
			 			 (u8)hba->pcidev->bus->number,
			 			 hba->pci_devno,
						 (u8)hba->pci_func);
			}
		} else if (ikcqe->op_code == ISCSI_KCQE_OPCODE_DESTROY_CONN)
			bnx2i_process_conn_destroy_cmpl(hba, ikcqe);
		else if (ikcqe->op_code == ISCSI_KCQE_OPCODE_ISCSI_ERROR) {
			hba->iscsi_error_kcqes++;
			bnx2i_process_iscsi_error(hba, ikcqe);
		} else if (ikcqe->op_code == ISCSI_KCQE_OPCODE_TCP_ERROR) {
			hba->tcp_error_kcqes++;
			bnx2i_process_tcp_error(hba, ikcqe);
		} else
			PRINT_ALERT(hba, "unknown opcode 0x%x\n",
					  ikcqe->op_code);
	}
}


#if !defined(__VMKLNX__)
/**
 * bnx2i_indicate_inetevent - Generic netstack event handler
 *
 * @context: 		adapter structure pointer
 * @event: 		event type
 *
 * Only required to handle NETDEV_UP event at this time
 */
static void bnx2i_indicate_inetevent(void *context, unsigned long event)
{
	struct bnx2i_hba *hba = (struct bnx2i_hba *) context;

	PRINT(hba, "%s: received inet event, %lx for hba %p\n",
		__FUNCTION__, event, hba);

	switch (event) {
	case NETDEV_UP:
		bnx2i_iscsi_handle_ip_event(hba);
		break;
	default:
		;
	}
}

/**
 * bnx2i_indicate_netevent - Generic netdev event handler
 *
 * @context: 		adapter structure pointer
 * @event: 		event type
 * 
 * Handles four netdev events, NETDEV_UP, NETDEV_DOWN,
 *	NETDEV_GOING_DOWN and NETDEV_CHANGE
 */
static void bnx2i_indicate_netevent(void *context, unsigned long event)
{
	struct bnx2i_hba *hba = (struct bnx2i_hba *) context;

	PRINT(hba, "%s: received net event, %lx for hba %p\n",
		__FUNCTION__, event, hba);

	switch (event) {
	case NETDEV_UP:
		if (!test_bit(ADAPTER_STATE_UP, &hba->adapter_state))
	    		bnx2i_send_fw_iscsi_init_msg(hba);
		break;
	case NETDEV_DOWN:
		mutex_lock(&hba->net_dev_lock);
		clear_bit(ADAPTER_STATE_GOING_DOWN, &hba->adapter_state);
		clear_bit(ADAPTER_STATE_UP, &hba->adapter_state);
		mutex_unlock(&hba->net_dev_lock);
		break;
	case NETDEV_GOING_DOWN:
		mutex_lock(&hba->net_dev_lock);
		set_bit(ADAPTER_STATE_GOING_DOWN, &hba->adapter_state);
		mutex_unlock(&hba->net_dev_lock);
		bnx2i_start_iscsi_hba_shutdown(hba);
			break;
	case NETDEV_CHANGE:
		bnx2i_get_link_state(hba);
		break;
	default:
		;
	}
	PRINT(hba, "%s: net event end, %lx for hba %p, state %lx\n",
		__FUNCTION__, event, hba, hba->adapter_state);
}
#endif

/**
 * bnx2i_cm_connect_cmpl - process iscsi conn establishment completion
 *
 * @cm_sk: 		cnic sock structure pointer
 *
 * function callback exported via bnx2i - cnic driver interface to
 *	indicate completion of option-2 TCP connect request.
 */
static void bnx2i_cm_connect_cmpl(struct cnic_sock *cm_sk)
{
	struct bnx2i_endpoint *ep = (struct bnx2i_endpoint *) cm_sk->context;

	if (ep->hba == NULL) {
		printk("%s: no hba associated with ep:%p \n", __func__, ep);
		return;
	}

	if (test_bit(ADAPTER_STATE_GOING_DOWN, &ep->hba->adapter_state))
		ep->state = EP_STATE_CONNECT_FAILED;
	else if (test_bit(SK_F_OFFLD_COMPLETE, &cm_sk->flags)) {
		ep->state = EP_STATE_CONNECT_COMPL;
		BNX2I_DBG(DBG_CONN_SETUP, ep->hba,
			  "bnx2i: cid %d connect complete\n", ep->ep_iscsi_cid);
	} else {
		ep->hba->login_stats.connection_timeouts++;
		ep->state = EP_STATE_CONNECT_FAILED;
		PRINT_ERR(ep->hba, "%s: cid %d failed to connect %x\n",
			  __FUNCTION__, ep->ep_iscsi_cid, ep->state);
	}

	wake_up_interruptible(&ep->ofld_wait);
}


/**
 * bnx2i_cm_close_cmpl - process tcp conn close completion
 *
 * @cm_sk: 		cnic sock structure pointer
 *
 * function callback exported via bnx2i - cnic driver interface to
 *	indicate completion of option-2 graceful TCP connect shutdown
 */
static void bnx2i_cm_close_cmpl(struct cnic_sock *cm_sk)
{
	struct bnx2i_endpoint *ep = (struct bnx2i_endpoint *) cm_sk->context;
	struct bnx2i_hba *hba;

	ep->state = EP_STATE_DISCONN_COMPL;
	hba = ep->hba;
	if (hba == NULL) {
		printk("%s: no hba associated with ep:%p \n", __func__, ep);
		return;
	}

	BNX2I_DBG(DBG_CONN_SETUP, ep->hba, "%s: cid %d gracefully shutdown\n",
		  __FUNCTION__, ep->ep_iscsi_cid);
	if (ep->in_progress == 1)
		wake_up_interruptible(&ep->ofld_wait);
	else
		wake_up_interruptible(&hba->ep_tmo_wait);
}


/**
 * bnx2i_cm_abort_cmpl - process abortive tcp conn teardown completion
 *
 * @cm_sk: 		cnic sock structure pointer
 *
 * function callback exported via bnx2i - cnic driver interface to
 *	indicate completion of option-2 abortive TCP connect termination
 */
static void bnx2i_cm_abort_cmpl(struct cnic_sock *cm_sk)
{
	struct bnx2i_endpoint *ep = (struct bnx2i_endpoint *) cm_sk->context;
	struct bnx2i_hba *hba;

	ep->state = EP_STATE_DISCONN_COMPL;
	hba = ep->hba;

	if (hba == NULL) {
		printk("%s: no hba associated with ep:%p \n", __func__, ep);
		return;
	}

	BNX2I_DBG(DBG_CONN_SETUP, ep->hba, "cid %d torn down successfully\n",
		  ep->ep_iscsi_cid);
	if (ep->in_progress == 1)
		wake_up_interruptible(&ep->ofld_wait);
	else
		wake_up_interruptible(&hba->ep_tmo_wait);
}


/**
 * bnx2i_cm_remote_close - process received TCP FIN
 *
 * @hba: 		adapter structure pointer
 * @update_kcqe: 	kcqe pointer
 *
 * function callback exported via bnx2i - cnic driver interface to indicate
 *	async TCP events such as FIN
 */
static void bnx2i_cm_remote_close(struct cnic_sock *cm_sk)
{
	struct bnx2i_endpoint *ep = (struct bnx2i_endpoint *) cm_sk->context;
	struct bnx2i_sess *sess;

	if (!ep)
		return;
	sess = ep->sess;
	if (!sess)
		return;

	BNX2I_DBG(DBG_CONN_EVENT, ep->hba, "Remote FIN received, cid %d",
		  "IP {%x, %x}, TCP PORT {%x, %x}\n", ep->ep_iscsi_cid,
		  ep->cm_sk->src_ip[0], ep->cm_sk->dst_ip[0],
		  ep->cm_sk->src_port, ep->cm_sk->dst_port);

	spin_lock_bh(&sess->lock);
	ep->state = EP_STATE_TCP_FIN_RCVD;
	if (!ep->conn) {
		/* Conn destroyed before TCP FIN received */
		spin_unlock_bh(&sess->lock);
		return;
	}

	if (sess->state == BNX2I_SESS_IN_FFP)
		bnx2i_recovery_que_add_sess(ep->hba, sess);

	spin_unlock_bh(&sess->lock);
}


/**
 * bnx2i_cm_remote_abort - process TCP RST and start conn cleanup
 *
 * @hba: 		adapter structure pointer
 * @update_kcqe: 	kcqe pointer
 *
 * function callback exported via bnx2i - cnic driver interface to
 *	indicate async TCP events (RST) sent by the peer.
 */
static void bnx2i_cm_remote_abort(struct cnic_sock *cm_sk)
{
	struct bnx2i_endpoint *ep = (struct bnx2i_endpoint *) cm_sk->context;
	struct bnx2i_sess *sess;

	if (!ep)
		return;
	sess = ep->sess;
	if (!sess)
		return;

	BNX2I_DBG(DBG_CONN_EVENT, ep->hba, "Remote RST received, cid %d",
		  "IP {%x, %x}, TCP PORT {%x, %x}\n", ep->ep_iscsi_cid,
		  ep->cm_sk->src_ip[0], ep->cm_sk->dst_ip[0],
		  ep->cm_sk->src_port, ep->cm_sk->dst_port);

	spin_lock_bh(&sess->lock);
	/* force abortive cleanup even if session is being gracefully terminating */
	ep->teardown_mode = BNX2I_ABORTIVE_SHUTDOWN;
	ep->state = EP_STATE_TCP_RST_RCVD;
	if (!ep->conn) {
		/* Conn destroyed before TCP RST received */
		spin_unlock_bh(&sess->lock);
		return;
	}

	if (sess->state == BNX2I_SESS_IN_FFP)
		bnx2i_recovery_que_add_sess(ep->hba, sess);

	spin_unlock_bh(&sess->lock);
}


/**
 * bnx2i_cnic_cb - global template of bnx2i - cnic driver interface structure
 *			carrying callback function pointers
 *
 */
struct cnic_ulp_ops bnx2i_cnic_cb = {
	.version = CNIC_ULP_OPS_VER,
	.cnic_init = bnx2i_ulp_init,
	.cnic_exit = bnx2i_ulp_exit,
	.cnic_start = bnx2i_start,
	.cnic_stop = bnx2i_stop,
	.indicate_kcqes = bnx2i_indicate_kcqe,
#if !defined(__VMKLNX__)
	.indicate_netevent = bnx2i_indicate_netevent,
	.indicate_inetevent = bnx2i_indicate_inetevent,
#endif
	.cm_connect_complete = bnx2i_cm_connect_cmpl,
	.cm_close_complete = bnx2i_cm_close_cmpl,
	.cm_abort_complete = bnx2i_cm_abort_cmpl,
	.cm_remote_close = bnx2i_cm_remote_close,
	.cm_remote_abort = bnx2i_cm_remote_abort,
	.cnic_get_stats = bnx2i_get_stats,
	.owner = THIS_MODULE
};


/**
 * bnx2i_map_ep_dbell_regs - map connection doorbell registers
 *
 * maps connection's SQ and RQ doorbell registers, 5706/5708/5709 hosts these
 *	register in BAR #0. Whereas in 57710 these register are accessed by
 *	mapping BAR #1
 */
int bnx2i_map_ep_dbell_regs(struct bnx2i_endpoint *ep)
{
	u32 cid_num;
	u32 reg_off;
	u32 first_l4l5;
	u32 ctx_sz;
	u32 config2;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18)
	resource_size_t reg_base;
#else
	unsigned long reg_base;
#endif

	cid_num = bnx2i_get_cid_num(ep);

	if (test_bit(BNX2I_NX2_DEV_57710, &ep->hba->cnic_dev_type)) {
		reg_base = pci_resource_start(ep->hba->pcidev,
					      BNX2X_DOORBELL_PCI_BAR);
		reg_off = (1 << BNX2X_DB_SHIFT) *  (cid_num & 0x1FFFF);
		ep->qp.ctx_base = ioremap_nocache(reg_base + reg_off, 4);
		if (ep->qp.ctx_base == NULL)
			goto iomap_err;
		else
			goto arm_cq;
	}

	if ((test_bit(BNX2I_NX2_DEV_5709, &ep->hba->cnic_dev_type)) &&
	    (ep->hba->mail_queue_access == BNX2I_MQ_BIN_MODE)) {
		config2 = REG_RD(ep->hba, BNX2_MQ_CONFIG2);
		first_l4l5 = config2 & BNX2_MQ_CONFIG2_FIRST_L4L5;
		ctx_sz = (config2 & BNX2_MQ_CONFIG2_CONT_SZ) >> 3;
		if (ctx_sz)
			reg_off = CTX_OFFSET + MAX_CID_CNT * MB_KERNEL_CTX_SIZE
			  	  + PAGE_SIZE *
				  (((cid_num - first_l4l5) / ctx_sz) + 256);
		else
			reg_off = CTX_OFFSET + (MB_KERNEL_CTX_SIZE * cid_num);
	} else
		/* 5709 device in normal node and 5706/5708 devices */
		reg_off = CTX_OFFSET + (MB_KERNEL_CTX_SIZE * cid_num);

	ep->qp.ctx_base = ioremap_nocache(ep->hba->reg_base + reg_off,
					  MB_KERNEL_CTX_SIZE);
	if (ep->qp.ctx_base == NULL)
		goto iomap_err;
arm_cq:
	bnx2i_arm_cq_event_coalescing(ep, CNIC_ARM_CQE);
	
	return 0;
	
iomap_err:
	return -ENOMEM;
}
