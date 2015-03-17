/*
 * QLogic NetXtreme II Linux FCoE offload driver.
 * Copyright (c)   2003-2014 QLogic Corporation
 *
 * This file contains helper routines that handle ELS requests
 * and responses.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * Written by: Bhanu Prakash Gollapudi (bprakash@broadcom.com)
 */

#include "bnx2fc.h"

static void bnx2fc_logo_resp(struct fc_seq *seq, struct fc_frame *fp,
			     void *arg);
static void bnx2fc_flogi_resp(struct fc_seq *seq, struct fc_frame *fp,
			      void *arg);
static int bnx2fc_initiate_els(struct bnx2fc_rport *tgt, unsigned int op,
			void *data, u32 data_len,
			void (*cb_func)(struct bnx2fc_els_cb_arg *cb_arg),
			struct bnx2fc_els_cb_arg *cb_arg, u32 timer_msec);

void bnx2fc_fill_fc_hdr(struct fc_frame_header *fc_hdr, enum fc_rctl r_ctl,
			u32 sid, u32 did, enum fc_fh_type fh_type,
			u32 f_ctl, u32 parm_offset)
{
	hton24(fc_hdr->fh_s_id, sid);
	hton24(fc_hdr->fh_d_id, did);
	fc_hdr->fh_r_ctl = r_ctl;
	fc_hdr->fh_type = fh_type;
	hton24(fc_hdr->fh_f_ctl, f_ctl);
	fc_hdr->fh_cs_ctl = 0;
	fc_hdr->fh_df_ctl = 0;
	fc_hdr->fh_parm_offset = htonl(parm_offset);
}

static void bnx2fc_rrq_compl(struct bnx2fc_els_cb_arg *cb_arg)
{
	struct bnx2fc_cmd *orig_io_req;
	struct bnx2fc_cmd *rrq_req;
	struct bnx2fc_hba *hba;
	int rc = 0;

	BUG_ON(!cb_arg);

	rrq_req = cb_arg->io_req;
	hba = rrq_req->port->hba;

	bnx2fc_dbg(LOG_ELS, "Entered rrq_compl callback\n");
	orig_io_req = cb_arg->aborted_io_req;
	BUG_ON(!orig_io_req);
	bnx2fc_dbg(LOG_ELS, "rrq_compl: orig xid = 0x%x, rrq_xid = 0x%x ",
		orig_io_req->xid, rrq_req->xid);

	kref_put(&orig_io_req->refcount, bnx2fc_cmd_release);

	if (test_and_clear_bit(BNX2FC_FLAG_ELS_TIMEOUT, &rrq_req->req_flags)) {
		/*
		 * els req is timed out. cleanup the IO with FW and
		 * drop the completion. Remove from active_cmd_queue.
		 */
		bnx2fc_dbg(LOG_ELS, "rrq xid - 0x%x timed out, clean it up\n",
			   rrq_req->xid);

		if (rrq_req->on_active_queue) {
			list_del_init(&rrq_req->link);
			rrq_req->on_active_queue = 0;
			rc = bnx2fc_initiate_cleanup(rrq_req);
			BUG_ON(rc);
		}
	}

	kfree(cb_arg);

}
int bnx2fc_send_rrq(struct bnx2fc_cmd *aborted_io_req)
{

	struct fc_els_rrq rrq;
	struct bnx2fc_rport *tgt = aborted_io_req->tgt;
	struct fc_lport *lport = tgt->rdata->local_port;
	struct bnx2fc_els_cb_arg *cb_arg = NULL;
	struct bnx2fc_hba *hba;
	u32 sid = tgt->sid;
	u32 r_a_tov = lport->r_a_tov;
	int rc;

	hba = tgt->port->hba;

	bnx2fc_dbg(LOG_ELS, "Sending RRQ orig_xid = 0x%x\n",
		   aborted_io_req->xid);
	memset(&rrq, 0, sizeof(rrq));

	cb_arg = kzalloc(sizeof(struct bnx2fc_els_cb_arg), GFP_ATOMIC);
	if (!cb_arg) {
		printk(KERN_ERR PFX "Unable to allocate cb_arg for RRQ\n");
		rc = -ENOMEM;
		goto rrq_err;
	}

	cb_arg->aborted_io_req = aborted_io_req;

	rrq.rrq_cmd = ELS_RRQ;
	hton24(rrq.rrq_s_id, sid);
	rrq.rrq_ox_id = htons(aborted_io_req->xid);
	rrq.rrq_rx_id = htons(aborted_io_req->task->rxwr_txrd.var_ctx.rx_id);

	rc = bnx2fc_initiate_els(tgt, ELS_RRQ, &rrq, sizeof(rrq), 
				 bnx2fc_rrq_compl, cb_arg, 
				 r_a_tov);
rrq_err:
	if (rc) {
		bnx2fc_dbg(LOG_ELS, "RRQ failed - release orig io req 0x%x\n",
			aborted_io_req->xid);
		kfree(cb_arg);
		spin_lock_bh(&tgt->tgt_lock);
		kref_put(&aborted_io_req->refcount, bnx2fc_cmd_release);
		spin_unlock_bh(&tgt->tgt_lock);
	}
	return rc;
}

static void bnx2fc_l2_els_compl(struct bnx2fc_els_cb_arg *cb_arg)
{
	struct bnx2fc_hba *hba;
	struct bnx2fc_cmd *els_req;
	struct bnx2fc_rport *tgt;
	struct bnx2fc_mp_req *mp_req;
	struct fc_frame_header *fc_hdr;
	unsigned char *buf;
	void *resp_buf;
	u32 resp_len, hdr_len;
	u16 l2_oxid;
	int frame_len;
	int rc = 0;

	l2_oxid = cb_arg->l2_oxid;
	hba = cb_arg->io_req->port->hba;
	bnx2fc_dbg(LOG_ELS, "ELS COMPL - l2_oxid = 0x%x\n", l2_oxid);

	els_req = cb_arg->io_req;
	bnx2fc_dbg(LOG_ELS, "els_compl: els_req = 0x%p\n", els_req);

	if (test_and_clear_bit(BNX2FC_FLAG_ELS_TIMEOUT, &els_req->req_flags)) {
		/*
		 * els req is timed out. cleanup the IO with FW and
		 * drop the completion. libfc will handle the els timeout
		 */
		if (els_req->on_active_queue) {
			list_del_init(&els_req->link);
			els_req->on_active_queue = 0;
			rc = bnx2fc_initiate_cleanup(els_req);
			BUG_ON(rc);
		}
		goto free_arg;
	}

	tgt = els_req->tgt;
	mp_req = &(els_req->mp_req);
	fc_hdr = &(mp_req->resp_fc_hdr);
	resp_len = mp_req->resp_len;
	resp_buf = mp_req->resp_buf;

	buf = kzalloc(PAGE_SIZE, GFP_ATOMIC);
	if (!buf) {
		printk(KERN_ERR PFX "Unable to alloc mp buf\n");
		goto free_arg;
	}
	hdr_len = sizeof(*fc_hdr);
	if (hdr_len + resp_len > PAGE_SIZE) {
		printk(KERN_ERR PFX "l2_els_compl: resp len is "
				    "beyond page size\n");
		goto free_buf;
	}
	memcpy(buf, fc_hdr, hdr_len);
	memcpy(buf + hdr_len, resp_buf, resp_len);

	frame_len = hdr_len + resp_len;
	
	bnx2fc_process_l2_frame_compl(tgt, buf, frame_len, l2_oxid); 

free_buf:
	kfree(buf);
free_arg:
	kfree(cb_arg);
}

int bnx2fc_send_adisc(struct bnx2fc_rport *tgt, struct fc_frame *fp)
{
	struct fc_els_adisc *adisc;
	struct fc_frame_header *fh;
	struct bnx2fc_els_cb_arg *cb_arg;
	struct fc_lport *lport = tgt->rdata->local_port;
	u32 r_a_tov = lport->r_a_tov;
	struct bnx2fc_hba *hba = tgt->port->hba;
	int rc;

	bnx2fc_dbg(LOG_ELS, "Sending ADISC\n");
	fh = fc_frame_header_get(fp);
	cb_arg = kzalloc(sizeof(struct bnx2fc_els_cb_arg), GFP_ATOMIC);
	if (!cb_arg) {
		printk(KERN_ERR PFX "Unable to allocate cb_arg for ADISC\n");
		return -ENOMEM;
	}

	cb_arg->l2_oxid = ntohs(fh->fh_ox_id);

	bnx2fc_dbg(LOG_ELS, "send ADISC: l2_oxid = 0x%x\n", cb_arg->l2_oxid);
	adisc = fc_frame_payload_get(fp, sizeof(*adisc));
	/* adisc is initialized by libfc */
	rc = bnx2fc_initiate_els(tgt, ELS_ADISC, adisc, sizeof(*adisc),
				 bnx2fc_l2_els_compl, cb_arg, 2 * r_a_tov);
	if (rc)
		kfree(cb_arg);
	return rc;
}

int bnx2fc_send_logo(struct bnx2fc_rport *tgt, struct fc_frame *fp)
{
	struct fc_els_logo *logo;
	struct fc_frame_header *fh;
	struct bnx2fc_els_cb_arg *cb_arg;
	struct fc_lport *lport = tgt->rdata->local_port;
	u32 r_a_tov = lport->r_a_tov;
	struct bnx2fc_hba *hba = tgt->port->hba;
	int rc;

	bnx2fc_dbg(LOG_ELS, "Sending LOGO\n");
	fh = fc_frame_header_get(fp);
	cb_arg = kzalloc(sizeof(struct bnx2fc_els_cb_arg), GFP_ATOMIC);
	if (!cb_arg) {
		printk(KERN_ERR PFX "Unable to allocate cb_arg for LOGO\n");
		return -ENOMEM;
	}
	
	cb_arg->l2_oxid = ntohs(fh->fh_ox_id);

	logo = fc_frame_payload_get(fp, sizeof(*logo));
	/* logo is initialized by libfc */
	rc = bnx2fc_initiate_els(tgt, ELS_LOGO, logo, sizeof(*logo),
				 bnx2fc_l2_els_compl, cb_arg, 2 * r_a_tov);
	if (rc)
		kfree(cb_arg);
	return rc;
}

int bnx2fc_send_rls(struct bnx2fc_rport *tgt, struct fc_frame *fp)
{
	struct fc_els_rls *rls;
	struct fc_frame_header *fh;
	struct bnx2fc_els_cb_arg *cb_arg;
	struct fc_lport *lport = tgt->rdata->local_port;
	u32 r_a_tov = lport->r_a_tov;
	int rc;

	fh = fc_frame_header_get(fp);
	cb_arg = kzalloc(sizeof(struct bnx2fc_els_cb_arg), GFP_ATOMIC);
	if (!cb_arg) {
		printk(KERN_ERR PFX "Unable to allocate cb_arg for LOGO\n");
		return -ENOMEM;
	}

	cb_arg->l2_oxid = ntohs(fh->fh_ox_id);

	rls = fc_frame_payload_get(fp, sizeof(*rls));
	/* rls is initialized by libfc */
	rc = bnx2fc_initiate_els(tgt, ELS_RLS, rls, sizeof(*rls),
				  bnx2fc_l2_els_compl, cb_arg, 2 * r_a_tov);
	if (rc)
		kfree(cb_arg);
	return rc;
}

static int bnx2fc_initiate_els(struct bnx2fc_rport *tgt, unsigned int op,
			void *data, u32 data_len,
			void (*cb_func)(struct bnx2fc_els_cb_arg *cb_arg),
			struct bnx2fc_els_cb_arg *cb_arg, u32 timer_msec)
{
	struct bnx2fc_port *port = tgt->port;
	struct bnx2fc_hba *hba = port->hba;
	struct fc_rport *rport = tgt->rport;
	struct fc_lport *lport = port->lport;
	struct bnx2fc_cmd *els_req;
	struct bnx2fc_mp_req *mp_req;
	struct fc_frame_header *fc_hdr;
	struct fcoe_task_ctx_entry *task;
	struct fcoe_task_ctx_entry *task_page;
	int rc = 0;
	int task_idx, index;
	u32 did, sid;
	u16 xid;
	unsigned long start = jiffies;

	bnx2fc_dbg(LOG_ELS, "Sending ELS\n");

#ifdef __VMKLNX__
	rc = bnx2fc_remote_port_chkready(rport);
#else
	rc = fc_remote_port_chkready(rport);
#endif
	if (rc) {
		printk(KERN_ALERT PFX "els 0x%x: rport not ready\n", op);
		rc = -EINVAL;
		goto els_err;
	}
	if (lport->state != LPORT_ST_READY || !(lport->link_up)) {
		printk(KERN_ALERT PFX "els 0x%x: link is not ready\n", op);
		rc = -EINVAL;
		goto els_err;
	}
	if (!(test_bit(BNX2FC_FLAG_SESSION_READY, &tgt->flags)) ||
	     (test_bit(BNX2FC_FLAG_EXPL_LOGO, &tgt->flags))) {
		printk(KERN_ERR PFX "els 0x%x: tgt not ready\n", op);
		rc = -EINVAL;
		goto els_err;
	}
retry_els:
	els_req = bnx2fc_elstm_alloc(tgt, BNX2FC_ELS);
	if (!els_req) {
		if (time_after(jiffies, start + (10 * HZ))) {
			bnx2fc_dbg(LOG_ELS, "els: Failed els 0x%x", op);
			rc = -ENOMEM;
			goto els_err;
		}
		msleep(20);
		goto retry_els;
	}

	bnx2fc_dbg(LOG_ELS, "initiate_els els_req = 0x%p cb_arg = %p\n", 
		els_req, cb_arg);
	els_req->sc_cmd = NULL;
	els_req->port = port;
	els_req->tgt = tgt;
	els_req->cb_func = cb_func;
	cb_arg->io_req = els_req;
	els_req->cb_arg = cb_arg;

	mp_req = (struct bnx2fc_mp_req *)&(els_req->mp_req);
	rc = bnx2fc_init_mp_req(els_req);
	if (rc == FAILED) {
		printk(KERN_ALERT PFX "ELS MP request init failed\n");
		spin_lock_bh(&tgt->tgt_lock);
		kref_put(&els_req->refcount, bnx2fc_cmd_release);
		spin_unlock_bh(&tgt->tgt_lock);
		goto els_err;
	} else {
		/* rc SUCCESS */
		rc = 0;
	}

	/* Fill ELS Payload */
	if ((op >= ELS_LS_RJT) && (op <= ELS_AUTH_ELS)) {
		memcpy(mp_req->req_buf, data, data_len);
	} else {
		printk(KERN_ALERT PFX "Invalid ELS op 0x%x\n", op);
		els_req->cb_func = NULL;
		els_req->cb_arg = NULL;
		spin_lock_bh(&tgt->tgt_lock);
		kref_put(&els_req->refcount, bnx2fc_cmd_release);
		spin_unlock_bh(&tgt->tgt_lock);
		rc = -EINVAL;
	}

	if (rc)
		goto els_err;

	/* Fill FC header */
	fc_hdr = &(mp_req->req_fc_hdr);

	did = tgt->rport->port_id;
	sid = tgt->sid;

	bnx2fc_fill_fc_hdr(fc_hdr, FC_RCTL_ELS_REQ, sid, did,
			   FC_TYPE_ELS, FC_FC_FIRST_SEQ | FC_FC_END_SEQ |
			   FC_FC_SEQ_INIT, 0);

	/* Obtain exchange id */
	xid = els_req->xid;
	task_idx = xid/BNX2FC_TASKS_PER_PAGE;
	index = xid % BNX2FC_TASKS_PER_PAGE;

	/* Initialize task context for this IO request */
	task_page = (struct fcoe_task_ctx_entry *) hba->task_ctx[task_idx];
	task = &(task_page[index]);
	bnx2fc_init_mp_task(els_req, task);

	spin_lock_bh(&tgt->tgt_lock);

	if (!test_bit(BNX2FC_FLAG_SESSION_READY, &tgt->flags)) {
		printk(KERN_ERR PFX "initiate_els.. session not ready\n");
		els_req->cb_func = NULL;
		els_req->cb_arg = NULL;
		kref_put(&els_req->refcount, bnx2fc_cmd_release);
		spin_unlock_bh(&tgt->tgt_lock);
		return -EINVAL;
	}

	if (timer_msec)
		bnx2fc_cmd_timer_set(els_req, timer_msec);
	bnx2fc_add_2_sq(tgt, xid);

	els_req->on_active_queue = 1;
	list_add_tail(&els_req->link, &tgt->els_queue);

	/* Ring doorbell */
	bnx2fc_dbg(LOG_ELS, "Ringing doorbell for ELS req\n");
	bnx2fc_ring_doorbell(tgt);
	spin_unlock_bh(&tgt->tgt_lock);

els_err:
	return rc;
}

void bnx2fc_process_els_compl(struct bnx2fc_cmd *els_req,
			      struct fcoe_task_ctx_entry *task, u8 num_rq)
{
	struct bnx2fc_mp_req *mp_req;
	struct bnx2fc_hba *hba = els_req->port->hba;
	struct fc_frame_header *fc_hdr;
	u64 *hdr;
	u64 *temp_hdr;

	bnx2fc_dbg(LOG_ELS, "Entered process_els_compl xid = 0x%x\n"
				"cmd_type = %d\n",
			els_req->xid, els_req->cmd_type);

	if (test_and_set_bit(BNX2FC_FLAG_ELS_DONE,
			     &els_req->req_flags)) {
		bnx2fc_dbg(LOG_ELS, "Timer context finished processing this "
			   "els - 0x%x\n", els_req->xid);
		/* This IO doesnt receive cleanup completion */
		kref_put(&els_req->refcount, bnx2fc_cmd_release);
		return;
	}

	/* Cancel the timeout_work, as we received the response */
	if (cancel_delayed_work(&els_req->timeout_work))
		kref_put(&els_req->refcount,
			 bnx2fc_cmd_release); /* drop timer hold */

	if (els_req->on_active_queue) {
		list_del_init(&els_req->link);
		els_req->on_active_queue = 0;
	}

	mp_req = &(els_req->mp_req);
	fc_hdr = &(mp_req->resp_fc_hdr);

	hdr = (u64 *)fc_hdr;
	temp_hdr = (u64 *)
		&task->rxwr_only.union_ctx.comp_info.mp_rsp.fc_hdr;
	hdr[0] = cpu_to_be64(temp_hdr[0]);
	hdr[1] = cpu_to_be64(temp_hdr[1]);
	hdr[2] = cpu_to_be64(temp_hdr[2]);

	mp_req->resp_len = task->rxwr_only.union_ctx.comp_info.mp_rsp.mp_payload_len;
	bnx2fc_dbg(LOG_ELS, "els_compl: resp_len = %d\n", mp_req->resp_len);

	/* Parse ELS response */
	if ((els_req->cb_func) && (els_req->cb_arg)) {
		els_req->cb_func(els_req->cb_arg);
		els_req->cb_arg = NULL;
	}

	kref_put(&els_req->refcount, bnx2fc_cmd_release);
}

static void bnx2fc_flogi_resp(struct fc_seq *seq, struct fc_frame *fp,
			      void *arg)
{
	struct fcoe_ctlr *fip = arg;
	struct fc_exch *exch = fc_seq_exch(seq);
	struct fc_lport *lport = exch->lp;
	u8 *mac;
	u8 op;
	DECLARE_MAC_BUF(macbuf);
#if defined (__VMKLNX__)
	u16 vlan_id;
	struct bnx2fc_port *port = lport_priv(lport);
	struct bnx2fc_hba *hba = port->hba;
#endif

	if (IS_ERR(fp))
		goto done;

	mac = fr_cb(fp)->granted_mac;
	if (is_zero_ether_addr(mac)) {
        	op = fc_frame_payload_op(fp);
		if (lport->vport) {
			if (op == ELS_LS_RJT) {
				printk(KERN_ERR PFX "bnx2fc_flogi_resp is LS_RJT\n");
#ifndef __VMKLNX__
				fc_vport_terminate(lport->vport);
#endif
				fc_frame_free(fp);
				return;
			}
		}
		fcoe_ctlr_recv_flogi(fip, lport, fp);
	}
	if (!is_zero_ether_addr(mac)) {
		struct cnic_dev *cnic_dev = hba->cnic;
		fip->update_mac(lport, mac);

		if (cnic_dev->mf_mode == MULTI_FUNCTION_SD &&
		    !is_zero_ether_addr(hba->ffa_fcoe_mac)) {
			bnx2fc_dbg(LOG_ELS, "Switching to SD MAC: %s",
					    print_mac(macbuf,
						      hba->ffa_fcoe_mac));
			memcpy(fip->dest_addr, hba->ffa_fcoe_mac, ETH_ALEN);
		}
	}

	bnx2fc_dbg(LOG_ELS, "flogi_resp - updated_mac = %s\n",
		   print_mac(macbuf, mac));
#if defined (__VMKLNX__)
        /*
         * Initialize already known information FCOE adapter attributes.
         */
        vlan_id = vmklnx_cna_get_vlan_tag(hba->fcoe_net_dev) & VLAN_VID_MASK;
        vmklnx_init_fcoe_attribs(lport->host, hba->fcoe_net_dev->name,
                                 vlan_id, hba->ctlr.ctl_src_addr,
				 fr_cb(fp)->granted_mac, fip->dest_addr);
#endif /* defined (__VMKLNX__) */
done:
	fc_lport_flogi_resp(seq, fp, lport);
}

static void bnx2fc_logo_resp(struct fc_seq *seq, struct fc_frame *fp,
			     void *arg)
{
	struct fcoe_ctlr *fip = arg;
	struct fc_exch *exch = fc_seq_exch(seq);
	struct fc_lport *lport = exch->lp;
	struct bnx2fc_port *port = lport_priv(lport);
	struct bnx2fc_hba *hba = port->hba;
	static u8 zero_mac[ETH_ALEN] = { 0 };
	DECLARE_MAC_BUF(macbuf);
	u8 command_code;

	/* Making the behavior consist with the libfc behavior */
	/* If state of lport is not LPORT_ST_LOGO, ignore */
	/* If frame opcode is not acceptELS_LS_ACC, ignore */
	if(lport->state != LPORT_ST_LOGO) {
		bnx2fc_dbg(LOG_ELS, "Current state of local port "
				"is not LPORT_ST_LOGO = 0x%x.\n",
				lport->state);
		goto err;
	}

	command_code = fc_frame_payload_op(fp);
	if(command_code != ELS_LS_ACC) {
		bnx2fc_dbg(LOG_ELS, "Fabric LOGO response is not accepted. "
				"cmd code = %d.\n", command_code);
		goto err;
	}
	if (!IS_ERR(fp)) {
		bnx2fc_dbg(LOG_ELS, "Zeroing out the mac address.\n");
		fip->update_mac(lport, zero_mac);
	}

err:
	fc_lport_logo_resp(seq, fp, lport);
}

struct fc_seq *bnx2fc_elsct_send(struct fc_lport *lport, u32 did,
				      struct fc_frame *fp, unsigned int op,
				      void (*resp)(struct fc_seq *,
						   struct fc_frame *,
						   void *),
				      void *arg, u32 timeout)
{
	struct bnx2fc_port *port = lport_priv(lport);
	struct bnx2fc_hba *hba = port->hba;
	struct fcoe_ctlr *fip = &hba->ctlr;
	struct fc_frame_header *fh = fc_frame_header_get(fp);

	bnx2fc_dbg(LOG_ELS, "elsct_send - op = 0x%x "
		"did = 0x%x\n", op, did);

	switch (op) {
	case ELS_FLOGI:
	case ELS_FDISC:
		return fc_elsct_send(lport, did, fp, op, bnx2fc_flogi_resp,
				     fip, timeout);
	case ELS_LOGO:
		/* only hook onto fabric logouts, not port logouts */
		if (did != FC_FID_FLOGI)
			break;
		return fc_elsct_send(lport, did, fp, op, bnx2fc_logo_resp,
				     fip, timeout);
	}
	return fc_elsct_send(lport, did, fp, op, resp, arg, timeout);
}
