/*
 * QLogic NetXtreme II Linux FCoE offload driver.
 * Copyright (c)   2003-2014 QLogic Corporation
 *
 * This file contains the code that low level functions that interact
 * with 57712 FCoE firmware.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * Written by: Bhanu Prakash Gollapudi (bprakash@broadcom.com)
 */

#include "bnx2fc.h"

static void bnx2fc_process_cq_compl(struct bnx2fc_rport *tgt, u16 wqe);
static void bnx2fc_fastpath_notification(struct bnx2fc_hba *hba,
					struct fcoe_kcqe *new_cqe_kcqe);
static void bnx2fc_process_ofld_cmpl(struct bnx2fc_hba *hba,
					struct fcoe_kcqe *ofld_kcqe);
static void bnx2fc_process_enable_conn_cmpl(struct bnx2fc_hba *hba,
						struct fcoe_kcqe *ofld_kcqe);
static void bnx2fc_init_failure(struct bnx2fc_hba *hba, u32 err_code);
static void bnx2fc_process_conn_destroy_cmpl(struct bnx2fc_hba *hba,
					struct fcoe_kcqe *conn_destroy);

int bnx2fc_send_stat_req(struct bnx2fc_hba *hba)
{
	struct fcoe_kwqe_stat stat_req;
	struct kwqe *kwqe_arr[2];
	int num_kwqes = 1;
	int rc = 0;
	unsigned int *x;
	int i;

	bnx2fc_dbg(LOG_INIT, "Entered bnx2fc_send_stat_req\n");

	memset(&stat_req, 0x00, sizeof(struct fcoe_kwqe_stat));
	stat_req.hdr.op_code = FCOE_KWQE_OPCODE_STAT;
	stat_req.hdr.flags = 
		(FCOE_KWQE_LAYER_CODE << FCOE_KWQE_HEADER_LAYER_CODE_SHIFT);

	stat_req.stat_params_addr_lo = (u32) hba->stats_buf_dma;
	stat_req.stat_params_addr_hi = (u32) ((u64)hba->stats_buf_dma >> 32);
	
	kwqe_arr[0] = (struct kwqe *) &stat_req;

	bnx2fc_dbg(LOG_INITV, "Dumping stat_req KWQE...\n");
	x = (unsigned int *)&stat_req;
	for (i=0; i<8; i++) {
		bnx2fc_dbg(LOG_INITV, "%8x ", *x++);
	}
	bnx2fc_dbg(LOG_INITV, "\n");

	if (hba->cnic && hba->cnic->submit_kwqes) 
		rc = hba->cnic->submit_kwqes(hba->cnic, kwqe_arr, num_kwqes);

	return rc;
}

/**
 * bnx2fc_send_fw_fcoe_init_msg - initiates initial handshake with FCoE f/w
 *
 * @hba:	adapter structure pointer
 *
 * Send down FCoE firmware init KWQEs which initiates the initial handshake
 *	with the f/w. 
 *
 */
int bnx2fc_send_fw_fcoe_init_msg(struct bnx2fc_hba *hba)
{
	struct fcoe_kwqe_init1 fcoe_init1;
	struct fcoe_kwqe_init2 fcoe_init2;
	struct fcoe_kwqe_init3 fcoe_init3;
	struct kwqe *kwqe_arr[3];
	int num_kwqes = 3;
	int rc = 0;
	int i;
	unsigned int *x;
	
	if (!hba->cnic) {
		printk(KERN_ALERT PFX "hba->cnic NULL during fcoe fw init\n");
		return -ENODEV;
	}

	/* fill init1 KWQE */
	memset(&fcoe_init1, 0x00, sizeof(struct fcoe_kwqe_init1));
	fcoe_init1.hdr.op_code = FCOE_KWQE_OPCODE_INIT1;
	fcoe_init1.hdr.flags = (FCOE_KWQE_LAYER_CODE << 
					FCOE_KWQE_HEADER_LAYER_CODE_SHIFT);

	fcoe_init1.num_tasks = BNX2FC_MAX_TASKS;
	fcoe_init1.sq_num_wqes = BNX2FC_SQ_WQES_MAX;
	fcoe_init1.rq_num_wqes = BNX2FC_RQ_WQES_MAX;
	fcoe_init1.rq_buffer_log_size = BNX2FC_RQ_BUF_LOG_SZ;
	fcoe_init1.cq_num_wqes = BNX2FC_CQ_WQES_MAX;
	fcoe_init1.dummy_buffer_addr_lo = (u32) hba->dummy_buf_dma;
	fcoe_init1.dummy_buffer_addr_hi = (u32) ((u64)hba->dummy_buf_dma >> 32);
	fcoe_init1.task_list_pbl_addr_lo = (u32) hba->task_ctx_bd_dma;
	fcoe_init1.task_list_pbl_addr_hi =
			(u32) ((u64) hba->task_ctx_bd_dma >> 32);
	fcoe_init1.mtu = BNX2FC_MINI_JUMBO_MTU;

	fcoe_init1.flags = (PAGE_SHIFT <<
				FCOE_KWQE_INIT1_LOG_PAGE_SIZE_SHIFT);

	fcoe_init1.num_sessions_log = BNX2FC_NUM_MAX_SESS_LOG;
	bnx2fc_dbg(LOG_INITV, "Dumping fcoe_init1 KWQE...\n");
        x = (unsigned int *)&fcoe_init1;
        for (i=0; i<8; i++) {
		bnx2fc_dbg(LOG_INITV, "%8x", *x++);
        }
	bnx2fc_dbg(LOG_ELS, "\n");

	/* fill init2 KWQE */
	memset(&fcoe_init2, 0x00, sizeof(struct fcoe_kwqe_init2));
	fcoe_init2.hdr.op_code = FCOE_KWQE_OPCODE_INIT2;
	fcoe_init2.hdr.flags = (FCOE_KWQE_LAYER_CODE << 
					FCOE_KWQE_HEADER_LAYER_CODE_SHIFT);

	fcoe_init2.hash_tbl_pbl_addr_lo = (u32) hba->hash_tbl_pbl_dma;
	fcoe_init2.hash_tbl_pbl_addr_hi = (u32) 
					   ((u64) hba->hash_tbl_pbl_dma >> 32);

	fcoe_init2.t2_hash_tbl_addr_lo = (u32) hba->t2_hash_tbl_dma;
	fcoe_init2.t2_hash_tbl_addr_hi = (u32) 
					  ((u64) hba->t2_hash_tbl_dma >> 32);

	fcoe_init2.t2_ptr_hash_tbl_addr_lo = (u32) hba->t2_hash_tbl_ptr_dma;
	fcoe_init2.t2_ptr_hash_tbl_addr_hi = (u32) 
					((u64) hba->t2_hash_tbl_ptr_dma >> 32);

	fcoe_init2.free_list_count = BNX2FC_NUM_MAX_SESS;
	bnx2fc_dbg(LOG_INITV, "Dumping fcoe_init2 KWQE...\n");
        x = (unsigned int *)&fcoe_init2;
        for (i=0; i<8; i++) {
		bnx2fc_dbg(LOG_INITV, "%8x ", *x++);
        }
	bnx2fc_dbg(LOG_INITV, "\n");
	
	/* fill init3 KWQE */
	memset(&fcoe_init3, 0x00, sizeof(struct fcoe_kwqe_init3));
	fcoe_init3.hdr.op_code = FCOE_KWQE_OPCODE_INIT3;
	fcoe_init3.hdr.flags = (FCOE_KWQE_LAYER_CODE << 
					FCOE_KWQE_HEADER_LAYER_CODE_SHIFT);
	fcoe_init3.error_bit_map_lo = 0xffffffff;
	fcoe_init3.error_bit_map_hi = 0xffffffff;

	fcoe_init3.perf_config = 1;

	bnx2fc_dbg(LOG_INITV, "Dumping fcoe_init3 KWQE...\n");
        x = (unsigned int *)&fcoe_init3;
        for (i=0; i<8; i++) {
		bnx2fc_dbg(LOG_INITV, "%8x ", *x++);
        }
	bnx2fc_dbg(LOG_INITV, "\n");

	kwqe_arr[0] = (struct kwqe *) &fcoe_init1;
	kwqe_arr[1] = (struct kwqe *) &fcoe_init2;
	kwqe_arr[2] = (struct kwqe *) &fcoe_init3;
	
	if (hba->cnic && hba->cnic->submit_kwqes) 
		rc = hba->cnic->submit_kwqes(hba->cnic, kwqe_arr, num_kwqes);

	return rc;
}
int bnx2fc_send_fw_fcoe_destroy_msg(struct bnx2fc_hba *hba)
{
	struct fcoe_kwqe_destroy fcoe_destroy;
	struct kwqe *kwqe_arr[2];
	int num_kwqes = 1;
	int rc = -1;
	
	/* fill destroy KWQE */
	memset(&fcoe_destroy, 0x00, sizeof(struct fcoe_kwqe_destroy));
	fcoe_destroy.hdr.op_code = FCOE_KWQE_OPCODE_DESTROY;
	fcoe_destroy.hdr.flags = (FCOE_KWQE_LAYER_CODE << 
					FCOE_KWQE_HEADER_LAYER_CODE_SHIFT);
	kwqe_arr[0] = (struct kwqe *) &fcoe_destroy;

	bnx2fc_dbg(LOG_INIT, "Submitting destroy KWQE\n");
	if (hba->cnic && hba->cnic->submit_kwqes) 
		rc = hba->cnic->submit_kwqes(hba->cnic, kwqe_arr, num_kwqes);
	bnx2fc_dbg(LOG_INIT, "destroy rc = %d\n", rc);

	return rc;
}

/**
 * bnx2fc_send_session_ofld_req - initiates FCoE Session offload process 
 * 
 * @port:		port structure pointer
 * @tgt:		bnx2fc_rport structure pointer
 */
int bnx2fc_send_session_ofld_req(struct bnx2fc_port *port, 
					struct bnx2fc_rport *tgt)
{
	struct fc_lport *lport = port->lport;
	struct bnx2fc_hba *hba = port->hba;
	struct kwqe *kwqe_arr[4];
	struct fcoe_kwqe_conn_offload1 ofld_req1;
	struct fcoe_kwqe_conn_offload2 ofld_req2;
	struct fcoe_kwqe_conn_offload3 ofld_req3;
	struct fcoe_kwqe_conn_offload4 ofld_req4;
	struct fc_rport_priv *rdata = tgt->rdata;
	struct fc_rport *rport = tgt->rport;
	int num_kwqes = 4;
	u32 port_id;
	int rc = 0;
	u16 conn_id;
	int i;
	unsigned int *x;

 	/* Initialize offload request 1 structure */
	memset(&ofld_req1, 0x00, sizeof(struct fcoe_kwqe_conn_offload1));

	ofld_req1.hdr.op_code = FCOE_KWQE_OPCODE_OFFLOAD_CONN1;
	ofld_req1.hdr.flags = 
		(FCOE_KWQE_LAYER_CODE << FCOE_KWQE_HEADER_LAYER_CODE_SHIFT);

	
	conn_id = (u16)tgt->fcoe_conn_id;
	ofld_req1.fcoe_conn_id = conn_id;
	 

	ofld_req1.sq_addr_lo = (u32) tgt->sq_dma;
	ofld_req1.sq_addr_hi = (u32)((u64) tgt->sq_dma >>32);

	ofld_req1.rq_pbl_addr_lo = (u32) tgt->rq_pbl_dma;
	ofld_req1.rq_pbl_addr_hi = (u32)((u64) tgt->rq_pbl_dma >> 32);

	ofld_req1.rq_first_pbe_addr_lo = (u32) tgt->rq_dma;
	ofld_req1.rq_first_pbe_addr_hi = 
				(u32)((u64) tgt->rq_dma >> 32);

	ofld_req1.rq_prod = 0x8000; 
	bnx2fc_dbg(LOG_SESSV, "Dumping ofld_req1 KWQE...\n");
        x = (unsigned int *)&ofld_req1;
        for (i=0; i<8; i++) {
		bnx2fc_dbg(LOG_SESSV, "%8x  ", *x++);
        }
	bnx2fc_dbg(LOG_SESSV, "\n");
	
 	/* Initialize offload request 2 structure */
	memset(&ofld_req2, 0x00, sizeof(struct fcoe_kwqe_conn_offload2));

	ofld_req2.hdr.op_code = FCOE_KWQE_OPCODE_OFFLOAD_CONN2;
	ofld_req2.hdr.flags = 
		(FCOE_KWQE_LAYER_CODE << FCOE_KWQE_HEADER_LAYER_CODE_SHIFT);

	ofld_req2.tx_max_fc_pay_len = rdata->maxframe_size;	// lport->mfs;

	ofld_req2.cq_addr_lo = (u32) tgt->cq_dma;
	ofld_req2.cq_addr_hi = (u32)((u64)tgt->cq_dma >> 32);

	ofld_req2.xferq_addr_lo = (u32) tgt->xferq_dma;
	ofld_req2.xferq_addr_hi = (u32)((u64)tgt->xferq_dma >> 32);

	ofld_req2.conn_db_addr_lo = (u32)tgt->conn_db_dma;
	ofld_req2.conn_db_addr_hi = (u32)((u64)tgt->conn_db_dma >> 32);
	bnx2fc_dbg(LOG_SESSV, "Dumping ofld_req2 KWQE...\n");
        x = (unsigned int *)&ofld_req2;
        for (i=0; i<8; i++) {
		bnx2fc_dbg(LOG_SESSV, "%8x  ", *x++);
        }
	bnx2fc_dbg(LOG_SESSV, "\n");

 	/* Initialize offload request 3 structure */
	memset(&ofld_req3, 0x00, sizeof(struct fcoe_kwqe_conn_offload3));

	ofld_req3.hdr.op_code = FCOE_KWQE_OPCODE_OFFLOAD_CONN3;
	ofld_req3.hdr.flags = 
		(FCOE_KWQE_LAYER_CODE << FCOE_KWQE_HEADER_LAYER_CODE_SHIFT);

	bnx2fc_dbg(LOG_SESSV, "%s - FCoE VLAN ID 0x%x, fcoe_cid 0x%x(%d)\n",
			 __FUNCTION__, hba->vlan_id, conn_id, conn_id);
	ofld_req3.vlan_tag = hba->vlan_id << 
				FCOE_KWQE_CONN_OFFLOAD3_VLAN_ID_SHIFT;
	ofld_req3.vlan_tag |= 3 << FCOE_KWQE_CONN_OFFLOAD3_PRIORITY_SHIFT;

	port_id = fc_host_port_id(lport->host);
	if (port_id == 0) {
		bnx2fc_dbg(LOG_SESS, "ofld_req: port_id = 0, link down?\n");
		return -EINVAL;
	}

	/* 
	 * Store s_id of the initiator for further reference. This will
	 * be used during disable/destroy during linkdown processing as
	 * when the lport is reset, the port_id also is reset to 0
	 */
	tgt->sid = port_id;	
	ofld_req3.s_id[0] = (port_id & 0x000000FF);
	ofld_req3.s_id[1] = (port_id & 0x0000FF00) >> 8;
	ofld_req3.s_id[2] = (port_id & 0x00FF0000) >> 16;

	port_id = rport->port_id;
	ofld_req3.d_id[0] = (port_id & 0x000000FF);
	ofld_req3.d_id[1] = (port_id & 0x0000FF00) >> 8;
	ofld_req3.d_id[2] = (port_id & 0x00FF0000) >>16;

	ofld_req3.tx_total_conc_seqs = rdata->max_seq;
	
	ofld_req3.tx_max_conc_seqs_c3 = rdata->max_seq;
	ofld_req3.rx_max_fc_pay_len  = lport->mfs;

	ofld_req3.rx_total_conc_seqs = BNX2FC_MAX_SEQS;
	ofld_req3.rx_max_conc_seqs_c3 = BNX2FC_MAX_SEQS;
	ofld_req3.rx_open_seqs_exch_c3 = 1;

	ofld_req3.confq_first_pbe_addr_lo = tgt->confq_dma;
	ofld_req3.confq_first_pbe_addr_hi = (u32)((u64) tgt->confq_dma >> 32);

	/* set mul_n_port_ids supported flag to 0, until it is supported */
	ofld_req3.flags = 0;
	/*
	ofld_req3.flags |= (((lport->send_sp_features & FC_SP_FT_MNA)?1:0) <<
			    FCOE_KWQE_CONN_OFFLOAD3_B_MUL_N_PORT_IDS_SHIFT);			
	*/
	/* Info from PLOGI response */
	ofld_req3.flags |= (((rdata->sp_features & FC_SP_FT_EDTR)? 1 : 0) <<
			     FCOE_KWQE_CONN_OFFLOAD3_B_E_D_TOV_RES_SHIFT);

	ofld_req3.flags |= (((rdata->sp_features & FC_SP_FT_SEQC) ? 1 : 0) <<
			     FCOE_KWQE_CONN_OFFLOAD3_B_CONT_INCR_SEQ_CNT_SHIFT);

	/* vlan flag */
	ofld_req3.flags |= (test_bit(BXN2FC_VLAN_ENABLED, &hba->flags2) << 
			    FCOE_KWQE_CONN_OFFLOAD3_B_VLAN_FLAG_SHIFT);

	/* C2_VALID and ACK flags are not set as they are not suppported */
	
	bnx2fc_dbg(LOG_SESSV, "Dumping ofld_req3 KWQE...\n");
        x = (unsigned int *)&ofld_req3;
        for (i=0; i<8; i++) {
		bnx2fc_dbg(LOG_SESSV, "%8x  ", *x++);
        }
	bnx2fc_dbg(LOG_SESSV, "\n");

 	/* Initialize offload request 4 structure */
	memset(&ofld_req4, 0x00, sizeof(struct fcoe_kwqe_conn_offload4));
	ofld_req4.hdr.op_code = FCOE_KWQE_OPCODE_OFFLOAD_CONN4;
	ofld_req4.hdr.flags = 
		(FCOE_KWQE_LAYER_CODE << FCOE_KWQE_HEADER_LAYER_CODE_SHIFT);

	ofld_req4.e_d_tov_timer_val = lport->e_d_tov / 20;


	ofld_req4.src_mac_addr_lo[0] =  port->data_src_addr[5];
							/* local mac */
	ofld_req4.src_mac_addr_lo[1] =  port->data_src_addr[4];
	ofld_req4.src_mac_addr_mid[0] =  port->data_src_addr[3];
	ofld_req4.src_mac_addr_mid[1] =  port->data_src_addr[2];
	ofld_req4.src_mac_addr_hi[0] =  port->data_src_addr[1];
	ofld_req4.src_mac_addr_hi[1] =  port->data_src_addr[0];
	ofld_req4.dst_mac_addr_lo[0] =  hba->ctlr.dest_addr[5];/* fcf mac */
	ofld_req4.dst_mac_addr_lo[1] =  hba->ctlr.dest_addr[4];
	ofld_req4.dst_mac_addr_mid[0] =  hba->ctlr.dest_addr[3];
	ofld_req4.dst_mac_addr_mid[1] =  hba->ctlr.dest_addr[2];
	ofld_req4.dst_mac_addr_hi[0] =  hba->ctlr.dest_addr[1];
	ofld_req4.dst_mac_addr_hi[1] =  hba->ctlr.dest_addr[0];

	ofld_req4.lcq_addr_lo = (u32) tgt->lcq_dma;
	ofld_req4.lcq_addr_hi = (u32)((u64) tgt->lcq_dma >>32);

	ofld_req4.confq_pbl_base_addr_lo = (u32) tgt->confq_pbl_dma;
	ofld_req4.confq_pbl_base_addr_hi = (u32)((u64) tgt->confq_pbl_dma >> 32);

	kwqe_arr[0] = (struct kwqe *) &ofld_req1;
	kwqe_arr[1] = (struct kwqe *) &ofld_req2;
	kwqe_arr[2] = (struct kwqe *) &ofld_req3;
	kwqe_arr[3] = (struct kwqe *) &ofld_req4;

	bnx2fc_dbg(LOG_SESSV, "Dumping ofld_req4 KWQE...\n");
        x = (unsigned int *)&ofld_req4;
        for (i=0; i<8; i++) {
		bnx2fc_dbg(LOG_SESSV, "%8x  ", *x++);
        }
	bnx2fc_dbg(LOG_SESSV, "\n");

	if (hba->cnic && hba->cnic->submit_kwqes) 
		rc = hba->cnic->submit_kwqes(hba->cnic, kwqe_arr, num_kwqes);

	return rc;
}

/**
 * bnx2fc_send_session_enable_req - initiates FCoE Session enablement 
 * 
 * @port:		port structure pointer
 * @tgt:		bnx2fc_rport structure pointer
 */
int bnx2fc_send_session_enable_req(struct bnx2fc_port *port,
					struct bnx2fc_rport *tgt)
{
	struct kwqe *kwqe_arr[2];
	struct bnx2fc_hba *hba = port->hba;
	struct fcoe_kwqe_conn_enable_disable enbl_req;
	struct fc_lport *lport = port->lport;
	struct fc_rport *rport = tgt->rport;
	int num_kwqes = 1;
	int rc = 0;
	u32 port_id;
	int i;
	unsigned int *x;
	
	memset(&enbl_req, 0x00, 
	       sizeof(struct fcoe_kwqe_conn_enable_disable));
	enbl_req.hdr.op_code = FCOE_KWQE_OPCODE_ENABLE_CONN;
	enbl_req.hdr.flags = 
		(FCOE_KWQE_LAYER_CODE << FCOE_KWQE_HEADER_LAYER_CODE_SHIFT);

	enbl_req.src_mac_addr_lo[0] =  port->data_src_addr[5];
							/* local mac */
	enbl_req.src_mac_addr_lo[1] =  port->data_src_addr[4];
	enbl_req.src_mac_addr_mid[0] =  port->data_src_addr[3];
	enbl_req.src_mac_addr_mid[1] =  port->data_src_addr[2];
	enbl_req.src_mac_addr_hi[0] =  port->data_src_addr[1];
	enbl_req.src_mac_addr_hi[1] =  port->data_src_addr[0];

	enbl_req.dst_mac_addr_lo[0] =  hba->ctlr.dest_addr[5];/* fcf mac */
	enbl_req.dst_mac_addr_lo[1] =  hba->ctlr.dest_addr[4];
	enbl_req.dst_mac_addr_mid[0] =  hba->ctlr.dest_addr[3];
	enbl_req.dst_mac_addr_mid[1] =  hba->ctlr.dest_addr[2];
	enbl_req.dst_mac_addr_hi[0] =  hba->ctlr.dest_addr[1];
	enbl_req.dst_mac_addr_hi[1] =  hba->ctlr.dest_addr[0];

	port_id = fc_host_port_id(lport->host);
	if (port_id != tgt->sid) {
		printk(KERN_ERR PFX "WARN: enable_req port_id = 0x%x,"
				"sid = 0x%x\n", port_id, tgt->sid);
		port_id = tgt->sid;
	}
	enbl_req.s_id[0] = (port_id & 0x000000FF);
	enbl_req.s_id[1] = (port_id & 0x0000FF00) >> 8;
	enbl_req.s_id[2] = (port_id & 0x00FF0000) >> 16;

	port_id = rport->port_id;
	enbl_req.d_id[0] = (port_id & 0x000000FF);
	enbl_req.d_id[1] = (port_id & 0x0000FF00) >> 8;
	enbl_req.d_id[2] = (port_id & 0x00FF0000) >>16;
#if !defined(__FIP_VLAN_DISC_CMPL__)
	if (hba->fcoe_net_dev) {
#ifdef __FCOE_ENABLE_BOOT__
		hba->vlan_id = vmklnx_cna_get_vlan_tag(hba->fcoe_net_dev) & VLAN_VID_MASK;
#else
		hba->vlan_id = vmklnx_cna_get_vlan_tag(hba->fcoe_net_dev);
#endif
		set_bit(BXN2FC_VLAN_ENABLED, &hba->flags2);
	}
#endif
	bnx2fc_dbg(LOG_SESSV, "%s - FCoE VLAN ID 0x%x, vlan_enabled %d\n",
		__FUNCTION__, hba->vlan_id,
		test_bit(BXN2FC_VLAN_ENABLED, &hba->flags2));
	enbl_req.vlan_tag = hba->vlan_id << 
				FCOE_KWQE_CONN_ENABLE_DISABLE_VLAN_ID_SHIFT;
	enbl_req.vlan_tag |= 3 << FCOE_KWQE_CONN_ENABLE_DISABLE_PRIORITY_SHIFT;
	enbl_req.vlan_flag = test_bit(BXN2FC_VLAN_ENABLED, &hba->flags2);
	enbl_req.context_id = tgt->context_id;
	enbl_req.conn_id = tgt->fcoe_conn_id;

	kwqe_arr[0] = (struct kwqe *) &enbl_req;

	bnx2fc_dbg(LOG_SESSV, "submit enable kwqe - context_id = %d\n",
		tgt->context_id);
	bnx2fc_dbg(LOG_SESSV, "Dumping enbl_req KWQE...\n");
        x = (unsigned int *)&enbl_req;
        for (i=0; i<8; i++) {
		bnx2fc_dbg(LOG_SESSV, "%8x  ", *x++);
        }
	bnx2fc_dbg(LOG_SESSV, "\n");
	if (hba->cnic && hba->cnic->submit_kwqes) 
		rc = hba->cnic->submit_kwqes(hba->cnic, kwqe_arr, num_kwqes);
	return rc;
}

int bnx2fc_send_session_disable_req(struct bnx2fc_port *port, 
				    struct bnx2fc_rport *tgt)
{
	struct bnx2fc_hba *hba = port->hba;
	struct fcoe_kwqe_conn_enable_disable disable_req;
	struct kwqe *kwqe_arr[2];
	struct fc_rport *rport = tgt->rport;
	int num_kwqes = 1;
	int rc = 0;
	u32 port_id;

	unsigned int *x;
	int i;

	memset(&disable_req, 0x00, 
	       sizeof(struct fcoe_kwqe_conn_enable_disable));
	disable_req.hdr.op_code = FCOE_KWQE_OPCODE_DISABLE_CONN;
	disable_req.hdr.flags = 
		(FCOE_KWQE_LAYER_CODE << FCOE_KWQE_HEADER_LAYER_CODE_SHIFT);
	
	disable_req.src_mac_addr_lo[0] =  port->data_src_addr[5];
	disable_req.src_mac_addr_lo[1] =  port->data_src_addr[4];
	disable_req.src_mac_addr_mid[0] =  port->data_src_addr[3];
	disable_req.src_mac_addr_mid[1] =  port->data_src_addr[2];
	disable_req.src_mac_addr_hi[0] =  port->data_src_addr[1];
	disable_req.src_mac_addr_hi[1] =  port->data_src_addr[0];

	disable_req.dst_mac_addr_lo[0] =  hba->ctlr.dest_addr[5];/* fcf mac */
	disable_req.dst_mac_addr_lo[1] =  hba->ctlr.dest_addr[4];
	disable_req.dst_mac_addr_mid[0] =  hba->ctlr.dest_addr[3];
	disable_req.dst_mac_addr_mid[1] =  hba->ctlr.dest_addr[2];
	disable_req.dst_mac_addr_hi[0] =  hba->ctlr.dest_addr[1];
	disable_req.dst_mac_addr_hi[1] =  hba->ctlr.dest_addr[0];

	port_id = tgt->sid;
	bnx2fc_dbg(LOG_SESS, "Host Port id = 0x%x\n", port_id);
	disable_req.s_id[0] = (port_id & 0x000000FF);
	disable_req.s_id[1] = (port_id & 0x0000FF00) >> 8;
	disable_req.s_id[2] = (port_id & 0x00FF0000) >> 16;


	port_id = rport->port_id;
	disable_req.d_id[0] = (port_id & 0x000000FF);
	disable_req.d_id[1] = (port_id & 0x0000FF00) >> 8;
	disable_req.d_id[2] = (port_id & 0x00FF0000) >>16;
	disable_req.context_id = tgt->context_id;
	disable_req.conn_id = tgt->fcoe_conn_id;
	disable_req.vlan_tag = hba->vlan_id << 
				FCOE_KWQE_CONN_ENABLE_DISABLE_VLAN_ID_SHIFT;
	disable_req.vlan_tag |=
			3 << FCOE_KWQE_CONN_ENABLE_DISABLE_PRIORITY_SHIFT;
	disable_req.vlan_flag = test_bit(BXN2FC_VLAN_ENABLED, &hba->flags2);

	kwqe_arr[0] = (struct kwqe *) &disable_req;

	bnx2fc_dbg(LOG_SESSV, "Dumping disable_req KWQE...\n");
        x = (unsigned int *)&disable_req;
        for (i=0; i<8; i++) {
		bnx2fc_dbg(LOG_SESSV, "%8x ", *x++);
        }
	bnx2fc_dbg(LOG_SESSV, "\n");

	bnx2fc_dbg(LOG_SESSV, "submit disable req - context_id = 0x%x\n",
		tgt->context_id);
	if (hba->cnic && hba->cnic->submit_kwqes) 
		rc = hba->cnic->submit_kwqes(hba->cnic, kwqe_arr, num_kwqes);

	return rc;
}

int bnx2fc_send_session_destroy_req(struct bnx2fc_hba *hba, 
					struct bnx2fc_rport *tgt)
{
	struct fcoe_kwqe_conn_destroy destroy_req;
	struct kwqe *kwqe_arr[2];
	int num_kwqes = 1;
	int rc = 0;
	unsigned int *x;
	int i;

	memset(&destroy_req, 0x00, sizeof(struct fcoe_kwqe_conn_destroy));
	destroy_req.hdr.op_code = FCOE_KWQE_OPCODE_DESTROY_CONN;
	destroy_req.hdr.flags = 
		(FCOE_KWQE_LAYER_CODE << FCOE_KWQE_HEADER_LAYER_CODE_SHIFT);
	
	destroy_req.context_id = tgt->context_id;
	destroy_req.conn_id = tgt->fcoe_conn_id;

	kwqe_arr[0] = (struct kwqe *) &destroy_req;

	bnx2fc_dbg(LOG_SESSV, "Dumping destroy_req KWQE...\n");
	x = (unsigned int *)&destroy_req;
	for (i=0; i<8; i++) {
		bnx2fc_dbg(LOG_SESSV, "%8x ", *x++);
	}
	bnx2fc_dbg(LOG_SESSV, "\n");

	bnx2fc_dbg(LOG_SESS, "submit destroy req - context_id = 0x%x\n",
		tgt->context_id);
	if (hba->cnic && hba->cnic->submit_kwqes) 
		rc = hba->cnic->submit_kwqes(hba->cnic, kwqe_arr, num_kwqes);

	return rc;
}

void bnx2fc_process_l2_frame_compl(struct bnx2fc_rport *tgt, 
				   unsigned char *buf,
				   u32 frame_len, u16 l2_oxid)
{
	struct bnx2fc_port *port = tgt->port;
	struct bnx2fc_hba *hba = port->hba;
	struct fc_lport *lp = port->lport;
	struct fc_frame *fp;
	struct fc_frame_header *fh;
	struct fcoe_hdr *hp;
	struct fcoe_crc_eof *cp;
	struct fcoe_rcv_info *fr;
	struct bnx2fc_global_s *bg;

	struct sk_buff *skb;
	unsigned int hlen;	/* header length implies the version */
	unsigned int tlen;	/* trailer length */
	unsigned int elen;	/* eth header, may include vlan */
	u32 crc;
	struct ethhdr *eh;

	u32 payload_len;
	u16 oxid;
	int idx;
	u8 op;

	bnx2fc_dbg(LOG_FRAME, "l2_frame_compl l2_oxid = 0x%x, frame_len = %d\n",
		l2_oxid, frame_len);
	payload_len = frame_len - sizeof(struct fc_frame_header);

	fp = fc_frame_alloc(lp, payload_len);
	if (!fp) {
		printk(KERN_ERR PFX "fc_frame_alloc failure\n");
		return;
	}

	fh = (struct fc_frame_header *) fc_frame_header_get(fp);
	/* Copy FC Frame header and payload into the frame */
	memcpy(fh, buf, frame_len);

	if (l2_oxid != FC_XID_UNKNOWN) 
		fh->fh_ox_id = htons(l2_oxid);

	skb = fp_skb(fp);

	if ((fh->fh_r_ctl == FC_RCTL_ELS_REQ) ||
	    (fh->fh_r_ctl == FC_RCTL_ELS_REP)) {

		if (fh->fh_type == FC_TYPE_ELS) {
			op = fc_frame_payload_op(fp);
			if ((op == ELS_TEST) ||	(op == ELS_ESTC) ||
			    (op == ELS_FAN) || (op == ELS_CSU)) {
				/* 
				 * No need to reply for these
				 * ELS requests
				 */
				printk(KERN_ERR PFX "dropping ELS 0x%x\n", op);
				kfree_skb(skb);
				return;
			}
		}
		/* Pass it on to libfc for further processing */
		elen = (hba->netdev->priv_flags & IFF_802_1Q_VLAN) ?
			sizeof(struct vlan_ethhdr) : 
			sizeof(struct ethhdr);
		hlen = sizeof(struct fcoe_hdr);
		tlen = sizeof(struct fcoe_crc_eof);
		skb->ip_summed = CHECKSUM_NONE;
		crc = bnx2fc_crc(fp);

		if (skb_is_nonlinear(skb)) {
			skb_frag_t *frag;
			if (bnx2fc_get_paged_crc_eof(skb, tlen)) {
				printk(KERN_ERR PFX "Frame crc error\n");
				kfree_skb(skb);
				return;
			}
			idx = skb_shinfo(skb)->nr_frags - 1;
			frag = &skb_shinfo(skb)->frags[idx];
			cp = kmap_atomic(frag->page, 
					 KM_SKB_DATA_SOFTIRQ) + 
					 frag->page_offset;
		} else {
			cp = (struct fcoe_crc_eof *)skb_put(skb, tlen);
		}
		memset(cp, 0, sizeof(*cp));
		/* 
		 * We are supposed to receive single frame 
		 * sequences from the firmware
		 */
		cp->fcoe_eof = FC_EOF_T;
		cp->fcoe_crc32 = cpu_to_le32(~crc);

		if (skb_is_nonlinear(skb)) {
			kunmap_atomic(cp, KM_SKB_DATA_SOFTIRQ);
			cp = NULL;
		}

		skb_push(skb, hlen + elen);
		skb_reset_mac_header(skb);
		skb_set_network_header(skb, elen);
		skb_set_transport_header(skb, elen+hlen);
#ifndef __VMKLNX__
		skb->mac_len = elen;
#endif
		skb->protocol = htons(ETH_P_FCOE);
		skb->dev = hba->netdev;

		/* adjust skb->data to point to fcoe_hdr */
		skb_pull(skb, elen);

		eh = eth_hdr(skb);
		eh->h_proto = htons(ETH_P_FCOE);
		memcpy(eh->h_dest, port->data_src_addr, ETH_ALEN);
		memcpy(eh->h_source, hba->ctlr.dest_addr, ETH_ALEN);

		hp = (struct fcoe_hdr *)skb_network_header(skb);
		memset(hp, 0, sizeof(*hp));
		if (FC_FCOE_VER)
			FC_FCOE_ENCAPS_VER(hp, FC_FCOE_VER);

		/* needs to be less than 0x30 - FC_SOF_N3 */
		hp->fcoe_sof = FC_SOF_I3;

		fr = fcoe_dev_from_skb(skb);
		fr->fr_dev = lp;
		oxid =  ntohs(fh->fh_ox_id);

		bg = &bnx2fc_global;
		spin_lock_bh(&bg->fcoe_rx_list.lock);

		if (unlikely(!bg->l2_thread)) {
			spin_unlock_bh(&bg->fcoe_rx_list.lock);
			printk(KERN_ERR PFX "ERROR-thread is NULL\n");
			kfree_skb(skb);
			return;
		}
		bnx2fc_dbg(LOG_FRAME, "l2 frame queued\n");
		__skb_queue_tail(&bg->fcoe_rx_list, skb);
		if (bg->fcoe_rx_list.qlen == 1)
			wake_up_process(bg->l2_thread);
		spin_unlock_bh(&bg->fcoe_rx_list.lock);
	} else {
		bnx2fc_dbg(LOG_FRAME, "fh_r_ctl = 0x%x\n", fh->fh_r_ctl);
		kfree_skb(skb);
	}
}

static void bnx2fc_process_unsol_compl(struct bnx2fc_rport *tgt, u16 wqe)
{
	u8 num_rq;
	struct fcoe_err_report_entry *err_entry; 
	unsigned char *rq_data;
	unsigned char *buf = NULL, *buf1;
	int i;
	u16 xid;
	u32 frame_len, len;
	struct bnx2fc_cmd *io_req = NULL;
	struct fcoe_task_ctx_entry *task, *task_page;
	struct bnx2fc_hba *hba = tgt->port->hba;
	int task_idx, index;
	int rc = 0;


	bnx2fc_dbg(LOG_IOERR, "Entered UNSOL COMPLETION wqe = 0x%x\n", wqe);
	switch (wqe & FCOE_UNSOLICITED_CQE_SUBTYPE) {
	case FCOE_UNSOLICITED_FRAME_CQE_TYPE:
		frame_len = (wqe & FCOE_UNSOLICITED_CQE_PKT_LEN) >>
			     FCOE_UNSOLICITED_CQE_PKT_LEN_SHIFT;

		num_rq = (frame_len + BNX2FC_RQ_BUF_SZ - 1) / BNX2FC_RQ_BUF_SZ;

		rq_data = (unsigned char *)bnx2fc_get_next_rqe(tgt, num_rq);
		if (rq_data) {
			buf = rq_data;
		} else {
			buf1 = buf = kmalloc((num_rq * BNX2FC_RQ_BUF_SZ),
					      GFP_ATOMIC);
			for (i = 0; i < num_rq; i++) {
				rq_data = (unsigned char *)
					   bnx2fc_get_next_rqe(tgt, 1);
				len = BNX2FC_RQ_BUF_SZ;
				memcpy(buf1, rq_data, len);
				buf1 += len;
			}
		}
		bnx2fc_process_l2_frame_compl(tgt, buf, frame_len,
					      FC_XID_UNKNOWN);

		if (buf != rq_data)
			kfree(buf);
		bnx2fc_return_rqe(tgt, num_rq);
		break;

	case FCOE_ERROR_DETECTION_CQE_TYPE:
		/*
		 *In case of error reporting CQE a single RQ entry
		 * is consumes.
		 */
		spin_lock_bh(&tgt->tgt_lock);
		num_rq = 1;
		err_entry = (struct fcoe_err_report_entry *)
			     bnx2fc_get_next_rqe(tgt, 1);
		xid = err_entry->fc_hdr.ox_id;
		bnx2fc_dbg(LOG_UNSOL, "Unsol Error Frame OX_ID = 0x%x\n", xid);
		bnx2fc_dbg(LOG_UNSOL, "err_warn_bitmap = %08x:%08x\n",
			err_entry->data.err_warn_bitmap_hi,
			err_entry->data.err_warn_bitmap_lo);
		bnx2fc_dbg(LOG_UNSOL, "buf_offsets - tx = 0x%x, rx = 0x%x\n",
			err_entry->data.tx_buf_off, err_entry->data.rx_buf_off);

		bnx2fc_return_rqe(tgt, 1);

		if (xid > BNX2FC_MAX_XID) {
			bnx2fc_dbg(LOG_UNSOL, "xid(0x%x) out of FW range\n",
				   xid);
			spin_unlock_bh(&tgt->tgt_lock);
			break;
		}

		task_idx = xid / BNX2FC_TASKS_PER_PAGE;
		index = xid % BNX2FC_TASKS_PER_PAGE;
		task_page = (struct fcoe_task_ctx_entry *)hba->task_ctx[task_idx];
		task = &(task_page[index]);

		io_req = (struct bnx2fc_cmd *)hba->cmd_mgr->cmds[xid];
		if (!io_req) {
			spin_unlock_bh(&tgt->tgt_lock);
			break;
		}

		if (io_req->cmd_type != BNX2FC_SCSI_CMD) {
			printk(KERN_ERR PFX "err_warn: Not a SCSI cmd\n");
			spin_unlock_bh(&tgt->tgt_lock);
			break;
		}

		if (test_and_clear_bit(BNX2FC_FLAG_IO_CLEANUP,
				       &io_req->req_flags)) {
			printk(KERN_ERR PFX "unsol_err: cleanup in "
					    "progress.. ignore unsol err\n");
			spin_unlock_bh(&tgt->tgt_lock);
			break;
		}
	
		/*
		 * If ABTS is already in progress, and FW error is
		 * received after that, do not cancel the timeout_work
		 * and let the error recovery continue by explicitly
		 * logging out the target, when the ABTS eventually 
		 * times out.
		 */
		if (!test_and_set_bit(BNX2FC_FLAG_ISSUE_ABTS, 
				      &io_req->req_flags)) {
			/* 
			 * Cancel the timeout_work, as we received IO 
			 * completion with FW error. 
			 */
			if (cancel_delayed_work(&io_req->timeout_work))
				kref_put(&io_req->refcount,
					 bnx2fc_cmd_release); /* timer hold */

			rc = bnx2fc_initiate_abts(io_req);
			if (rc != SUCCESS) {
				printk(KERN_ERR PFX "err_warn: initiate_abts "
					"failed xid = 0x%x. issue cleanup\n",
					io_req->xid);
				rc = bnx2fc_initiate_cleanup(io_req);
				BUG_ON(rc);
			}
		} else
			printk(KERN_ERR PFX "err_warn: io_req (0x%x) already "
					    "in ABTS processing \n", xid);
		spin_unlock_bh(&tgt->tgt_lock);
		break;

	case FCOE_WARNING_DETECTION_CQE_TYPE:
		/* 
		 *In case of warning reporting CQE a single RQ entry
		 * is consumes.
		 */
		num_rq = 1;
		err_entry = (struct fcoe_err_report_entry *)
			     bnx2fc_get_next_rqe(tgt, 1);
		xid = cpu_to_be16(err_entry->fc_hdr.ox_id);
		bnx2fc_dbg(LOG_UNSOL, "Unsol Warning Frame OX_ID = 0x%x\n",
			xid);
		bnx2fc_dbg(LOG_UNSOL, "err_warn_bitmap = %08x:%08x",
			err_entry->data.err_warn_bitmap_hi,
			err_entry->data.err_warn_bitmap_lo);
		bnx2fc_dbg(LOG_UNSOL, "buf_offsets - tx = 0x%x, rx = 0x%x",
			err_entry->data.tx_buf_off, err_entry->data.rx_buf_off);

		bnx2fc_return_rqe(tgt, 1);
		break;
	
	default:
		printk(KERN_ERR PFX "Unsol Compl: Invalid CQE Subtype\n");
		break;
	}
}

static void bnx2fc_process_cq_compl(struct bnx2fc_rport *tgt, u16 wqe)
{
	struct fcoe_task_ctx_entry *task;
	struct fcoe_task_ctx_entry *task_page;
	struct bnx2fc_port *port = tgt->port;
	struct bnx2fc_hba *hba = port->hba;
	struct bnx2fc_cmd *io_req;
	int task_idx, index;
	u16 xid;
	u8  cmd_type;
	u8 rx_state = 0;
	u8 num_rq;

	spin_lock_bh(&tgt->tgt_lock);
	xid = wqe & FCOE_PEND_WQ_CQE_TASK_ID;
	if (xid >= BNX2FC_MAX_TASKS) {
		printk (KERN_ALERT PFX "ERROR:xid out of range\n");
		spin_unlock_bh(&tgt->tgt_lock);
		return;
	}
	task_idx = xid / BNX2FC_TASKS_PER_PAGE;
	index = xid % BNX2FC_TASKS_PER_PAGE;
	task_page = (struct fcoe_task_ctx_entry *)hba->task_ctx[task_idx];
	task = &(task_page[index]);

	num_rq = ((task->rxwr_txrd.var_ctx.rx_flags &
		   FCOE_TCE_RX_WR_TX_RD_VAR_NUM_RQ_WQE) >>
		   FCOE_TCE_RX_WR_TX_RD_VAR_NUM_RQ_WQE_SHIFT);

	io_req = (struct bnx2fc_cmd *)hba->cmd_mgr->cmds[xid];

	if (io_req == NULL) {
		printk(KERN_ERR PFX "ERROR? cq_compl - io_req is NULL\n");
		spin_unlock_bh(&tgt->tgt_lock);
		return;
	}

	/* Timestamp IO completion time */
	cmd_type = io_req->cmd_type;

	/* optimized completion path */
	if (cmd_type == BNX2FC_SCSI_CMD) {
		rx_state = ((task->rxwr_txrd.var_ctx.rx_flags &
			    FCOE_TCE_RX_WR_TX_RD_VAR_RX_STATE) >>
			    FCOE_TCE_RX_WR_TX_RD_VAR_RX_STATE_SHIFT);

		if (rx_state == FCOE_TASK_RX_STATE_COMPLETED) {
			bnx2fc_process_scsi_cmd_compl(io_req, task, num_rq);
			spin_unlock_bh(&tgt->tgt_lock);
			return;
		}
	}

	/* Process other IO completion types */
	switch (cmd_type) {
	case BNX2FC_SCSI_CMD:
		bnx2fc_dbg(LOG_IOERR, "SCSI_CMD(0x%x) "\
			"complete - rx_state = %d\n", io_req->xid, rx_state);

		if (rx_state == FCOE_TASK_RX_STATE_ABTS_COMPLETED)
			bnx2fc_process_abts_compl(io_req, task, num_rq);
		else if (rx_state ==
			 FCOE_TASK_RX_STATE_EXCHANGE_CLEANUP_COMPLETED)
			bnx2fc_process_cleanup_compl(io_req, task, num_rq);
		else
			printk(KERN_ERR PFX "Invalid rx state - %d\n",
				rx_state);
		break;

	case BNX2FC_TASK_MGMT_CMD:
		bnx2fc_dbg(LOG_IOERR, "Processing TM complete (0x%x)\n",
			   io_req->xid);
		bnx2fc_process_tm_compl(io_req, task, num_rq);
		break;

	case BNX2FC_ABTS:
		/*
		 * ABTS request received by firmware. ABTS response
		 * will be delivered to the task belonging to the IO
		 * that was aborted
		 */
		bnx2fc_dbg(LOG_IOERR, "cq_compl(0x%x) - ABTS sent out "
			   "by fw\n", io_req->xid);
		kref_put(&io_req->refcount, bnx2fc_cmd_release);
		break;

	case BNX2FC_ELS:
		bnx2fc_dbg(LOG_ELS, "cq_compl(0x%x) - call"
			   " process_els_compl\n", io_req->xid);
		bnx2fc_process_els_compl(io_req, task, num_rq);
		break;

	case BNX2FC_CLEANUP:
		bnx2fc_dbg(LOG_IOERR, "cq_compl(0x%x) - cleanup resp rcvd\n",
			   io_req->xid);

		kref_put(&io_req->refcount, bnx2fc_cmd_release);
		break;

	default:
		printk(KERN_ERR PFX "Invalid cmd_type %d\n", cmd_type);
		break;
	}
	spin_unlock_bh(&tgt->tgt_lock);
}

void bnx2fc_arm_cq(struct bnx2fc_rport *tgt)
{
	struct b577xx_fcoe_rx_doorbell *rx_db = &tgt->rx_db;
	u32 msg;
	
	wmb();
	rx_db->doorbell_cq_cons = tgt->cq_cons_idx | (tgt->cq_curr_toggle_bit <<
						  FCOE_CQE_TOGGLE_BIT_SHIFT);
	msg = *((u32 *)rx_db);
	writel(cpu_to_le32(msg), tgt->ctx_base);
	mmiowb();

}

int bnx2fc_process_new_cqes(struct bnx2fc_rport *tgt)
{
	struct fcoe_cqe *cq;
	u32 cq_cons;
	struct fcoe_cqe *cqe;
	u32 num_free_sqes = 0;
	u32 num_cqes = 0;
	u16 wqe;

	/*
	 * cq_lock is a low contention lock used to protect
	 * the CQ data structure from being freed up during
	 * the upload operation
	 */
	spin_lock_bh(&tgt->cq_lock);

	if (!tgt->cq) {
		printk(KERN_ERR PFX "process_new_cqes: cq is NULL\n");
		spin_unlock_bh(&tgt->cq_lock);
		return 0;
	} 
	cq = tgt->cq;
	cq_cons = tgt->cq_cons_idx;
	cqe = &cq[cq_cons];

	while (((wqe = cqe->wqe) & FCOE_CQE_TOGGLE_BIT) == 
	       (tgt->cq_curr_toggle_bit << 
	       FCOE_CQE_TOGGLE_BIT_SHIFT)) {

		/* new entry on the cq */
		if (wqe & FCOE_CQE_CQE_TYPE) {
			/* Unsolicited event notification */
			bnx2fc_process_unsol_compl(tgt, wqe);
		} else {
			/* Pending work request completion */
			bnx2fc_process_cq_compl(tgt, wqe);
			num_free_sqes++;
		}
		cqe++;
		tgt->cq_cons_idx++;
		num_cqes++;

		if (tgt->cq_cons_idx == BNX2FC_CQ_WQES_MAX) {
			tgt->cq_cons_idx = 0;
			cqe = cq;
			tgt->cq_curr_toggle_bit = 
				1 - tgt->cq_curr_toggle_bit;
		}
	}
	if (num_cqes) {
		/* Arm CQ only if doorbell is mapped */
		if (tgt->ctx_base)
			bnx2fc_arm_cq(tgt);
		atomic_add(num_free_sqes, &tgt->free_sqes);
	}

	spin_unlock_bh(&tgt->cq_lock);

	return 0;
}

/**
 * bnx2fc_fastpath_notification - process global event queue (KCQ)
 *
 * @hba:		adapter structure pointer
 * @new_cqe_kcqe:	pointer to newly DMA'd KCQ entry
 *
 * Fast path event notification handler
 */
static void bnx2fc_fastpath_notification(struct bnx2fc_hba *hba,
					struct fcoe_kcqe *new_cqe_kcqe)
{
	u32 conn_id = new_cqe_kcqe->fcoe_conn_id;
	struct bnx2fc_rport *tgt = hba->tgt_ofld_list[conn_id];

	if (!tgt) {
		printk(KERN_ALERT PFX "conn_id 0x%x not valid\n", conn_id);
		return;
	}
	bnx2fc_process_new_cqes(tgt);
}

/**
 * bnx2fc_process_ofld_cmpl - process FCoE session offload completion
 *
 * @hba:	adapter structure pointer
 * @ofld_kcqe:	connection offload kcqe pointer
 *
 * handle session offload completion, enable the session if offload is
 * successful.
 */
static void bnx2fc_process_ofld_cmpl(struct bnx2fc_hba *hba,
					struct fcoe_kcqe *ofld_kcqe)
{
	struct bnx2fc_rport		*tgt;
	struct bnx2fc_port		*port;
	u32				conn_id;
	u32				context_id;
	int				rc;

	conn_id = ofld_kcqe->fcoe_conn_id;
	context_id = ofld_kcqe->fcoe_conn_context_id;
	bnx2fc_dbg(LOG_SESS, "Entered ofld compl - context_id = 0x%x\n",
		ofld_kcqe->fcoe_conn_context_id);
	tgt = hba->tgt_ofld_list[conn_id];
	if (!tgt) {
		printk(KERN_ALERT PFX "ERROR:ofld_cmpl: No pending ofld req\n");
		return;
	}
	port = tgt->port;
	if (hba != tgt->port->hba) {
		printk(KERN_ALERT PFX "ERROR:ofld_cmpl: HBA mis-match\n");
		goto ofld_cmpl_err;
	}
	/*
	 * cnic has allocated a context_id for this session; use this
	 * while enabling the session.
	 */
	tgt->context_id = context_id;
	if (ofld_kcqe->completion_status) {
		if (ofld_kcqe->completion_status ==
				FCOE_KCQE_COMPLETION_STATUS_CTX_ALLOC_FAILURE) {
			printk(KERN_ERR PFX "unable to allocate FCoE context "
				"resources\n");
			set_bit(BNX2FC_FLAG_CTX_ALLOC_FAILURE, &tgt->flags);
		}
		goto ofld_cmpl_err;
	} else {

		/* now enable the session */
		rc = bnx2fc_send_session_enable_req(port, tgt);
		if (rc) {
			printk(KERN_ALERT PFX "enable session failed\n");
			goto ofld_cmpl_err;
		}
	}
	return;
ofld_cmpl_err:
	set_bit(BNX2FC_FLAG_OFLD_REQ_CMPL, &tgt->flags);
	wake_up_interruptible(&tgt->ofld_wait);
}

/**
 * bnx2fc_process_enable_conn_cmpl - process FCoE session enable completion
 * 
 * @hba:	adapter structure pointer
 * @ofld_kcqe:	connection offload kcqe pointer
 *
 * handle session enable completion, mark the rport as ready 
 * Algorithm:
 * o Mark rport as ready if offload is successful.
 * o Add rport to the list of rports
 */

static void bnx2fc_process_enable_conn_cmpl(struct bnx2fc_hba *hba,
						struct fcoe_kcqe *ofld_kcqe)
{
	struct bnx2fc_rport 		*tgt;
	u32				conn_id;
	u32				context_id; 

	context_id = ofld_kcqe->fcoe_conn_context_id; 
	conn_id = ofld_kcqe->fcoe_conn_id;
	tgt = (struct bnx2fc_rport *)hba->tgt_ofld_list[conn_id];

	bnx2fc_dbg(LOG_SESS, "Enable compl - context_id = 0x%x\n",
		ofld_kcqe->fcoe_conn_context_id);
	if (!tgt) {
		printk(KERN_ALERT PFX "ERROR:enbl_cmpl: No pending ofld req\n");
		return;
	}
	/* 
	 * context_id should be the same for this target during offload
	 * and enable 
	 */
	if (tgt->context_id != context_id) {
		printk(KERN_ALERT PFX "context id mis-match\n");
		return;
	}
	if (hba != tgt->port->hba) {
		printk(KERN_ALERT PFX "bnx2fc-enbl_cmpl: HBA mis-match\n");
		goto enbl_cmpl_err;
	}	
	if (ofld_kcqe->completion_status) {
		goto enbl_cmpl_err;
	} else {
		/* enable successful - rport ready for issuing IOs */
		set_bit(BNX2FC_FLAG_OFFLOADED, &tgt->flags);
		set_bit(BNX2FC_FLAG_OFLD_REQ_CMPL, &tgt->flags);
		wake_up_interruptible(&tgt->ofld_wait);
	}
	return;

enbl_cmpl_err:
	set_bit(BNX2FC_FLAG_OFLD_REQ_CMPL, &tgt->flags);
	wake_up_interruptible(&tgt->ofld_wait);
}

static void bnx2fc_process_conn_disable_cmpl(struct bnx2fc_hba *hba,
					struct fcoe_kcqe *disable_kcqe)
{

	struct bnx2fc_rport 		*tgt;
	u32				conn_id;

	conn_id = disable_kcqe->fcoe_conn_id;
	tgt = hba->tgt_ofld_list[conn_id];
	bnx2fc_dbg(LOG_SCSI_TM, "disable_cmpl: conn_id %d tgt 0x%p\n", 
		   conn_id, tgt);
	if (!tgt) {
		printk(KERN_ALERT PFX "ERROR: disable_cmpl: No disable req\n");
		return;
	}

	if (disable_kcqe->completion_status) {
		printk(KERN_ALERT PFX "ERROR: Disable failed with cmpl status %d\n",
			disable_kcqe->completion_status);
		set_bit(BNX2FC_FLAG_DISABLE_FAILED, &tgt->flags);
		set_bit(BNX2FC_FLAG_UPLD_REQ_COMPL, &tgt->flags);
	} else {
		/* disable successful */
		printk(KERN_ERR PFX "disable successful\n");
		clear_bit(BNX2FC_FLAG_OFFLOADED, &tgt->flags);
		set_bit(BNX2FC_FLAG_DISABLED, &tgt->flags);
		set_bit(BNX2FC_FLAG_UPLD_REQ_COMPL, &tgt->flags);
	}
	wake_up_interruptible(&tgt->upld_wait);
}

static void bnx2fc_process_conn_destroy_cmpl(struct bnx2fc_hba *hba,
					struct fcoe_kcqe *destroy_kcqe)
{
	struct bnx2fc_rport 		*tgt;
	u32				conn_id;

	conn_id = destroy_kcqe->fcoe_conn_id;
	tgt = hba->tgt_ofld_list[conn_id];
	bnx2fc_dbg(LOG_SESS, "destroy_cmpl: conn_id %d tgt 0x%p\n",
		conn_id, tgt);
	if (!tgt) {
		printk(KERN_ALERT PFX "destroy_cmpl: No destroy req\n");
		return;
	}

	if (destroy_kcqe->completion_status) {
		printk(KERN_ALERT PFX "Destroy conn failed, cmpl status %d\n",
			destroy_kcqe->completion_status);
		return;
	} else {
		/* destroy successful */
		bnx2fc_dbg(LOG_SESS, "upload successful\n");
		clear_bit(BNX2FC_FLAG_DISABLED, &tgt->flags);
		set_bit(BNX2FC_FLAG_DESTROYED, &tgt->flags);
		set_bit(BNX2FC_FLAG_UPLD_REQ_COMPL, &tgt->flags);
		wake_up_interruptible(&tgt->upld_wait);
	}
}

static void bnx2fc_init_failure(struct bnx2fc_hba *hba, u32 err_code)
{
	switch(err_code) {
	case FCOE_KCQE_COMPLETION_STATUS_INVALID_OPCODE:
		printk(KERN_ERR PFX "init_failure due to invalid opcode\n");
		break;

	case FCOE_KCQE_COMPLETION_STATUS_CTX_ALLOC_FAILURE:
		printk(KERN_ERR PFX "init failed due to ctx alloc failure\n");
		break;

	case FCOE_KCQE_COMPLETION_STATUS_NIC_ERROR:
		printk(KERN_ERR PFX "init_failure due to NIC error\n");
		break;

	default:
		printk(KERN_ERR PFX "Unknown Error code %d\n", err_code);
	}
}

/**
 * bnx2fc_indicae_kcqe - process KCQE
 *
 * @hba:	adapter structure pointer
 * @kcqe:	kcqe pointer
 * @num_cqe:	Number of completion queue elements
 *
 * Generic KCQ event handler
 */
void bnx2fc_indicate_kcqe(void *context, struct kcqe *kcq[],
					u32 num_cqe)
{ 
	struct bnx2fc_hba *hba = (struct bnx2fc_hba *)context;
	int i = 0;
	struct fcoe_kcqe *kcqe = NULL;

	while (i < num_cqe) {
		kcqe = (struct fcoe_kcqe *) kcq[i++];

		switch (kcqe->op_code) {
		case FCOE_KCQE_OPCODE_CQ_EVENT_NOTIFICATION:
			bnx2fc_fastpath_notification(hba, kcqe);
			break;

		case FCOE_KCQE_OPCODE_OFFLOAD_CONN:
			bnx2fc_process_ofld_cmpl(hba, kcqe);
			break;

		case FCOE_KCQE_OPCODE_ENABLE_CONN:
			bnx2fc_process_enable_conn_cmpl(hba, kcqe);
			break;

		case FCOE_KCQE_OPCODE_INIT_FUNC:
			if (kcqe->completion_status != 
					FCOE_KCQE_COMPLETION_STATUS_SUCCESS) {
				bnx2fc_init_failure(hba, 
						kcqe->completion_status);
			} else {
				set_bit(ADAPTER_STATE_UP, &hba->adapter_state);
				bnx2fc_get_link_state(hba);
				printk(KERN_INFO PFX "[%.2x]: FCOE_INIT passed\n",
					(u8)hba->pcidev->bus->number);
			}	
			break;
	
		case FCOE_KCQE_OPCODE_DESTROY_FUNC:
			if (kcqe->completion_status !=
					FCOE_KCQE_COMPLETION_STATUS_SUCCESS) {
		
				printk(KERN_ERR PFX "DESTROY failed\n");
			} else {
				printk(KERN_ERR PFX "DESTROY success\n");
			}
			set_bit(BNX2FC_FLAG_DESTROY_CMPL, &hba->flags);
			wake_up_interruptible(&hba->destroy_wait);
			break;

		case FCOE_KCQE_OPCODE_DISABLE_CONN:
			bnx2fc_process_conn_disable_cmpl(hba, kcqe);
			break;

		case FCOE_KCQE_OPCODE_DESTROY_CONN:
			bnx2fc_process_conn_destroy_cmpl(hba, kcqe);	
			break;
		
		case FCOE_KCQE_OPCODE_STAT_FUNC:
			if (kcqe->completion_status !=
			    FCOE_KCQE_COMPLETION_STATUS_SUCCESS)
				printk(KERN_ERR PFX "STAT failed\n");
			complete(&hba->stat_req_done);
			break;

		case FCOE_KCQE_OPCODE_FCOE_ERROR:
			/* fall thru */
		default:
			printk(KERN_ALERT PFX "unknown opcode 0x%x\n",
								kcqe->op_code);
		}
	}
}

void bnx2fc_add_2_sq(struct bnx2fc_rport *tgt, u16 xid)
{
	struct fcoe_sqe *sqe;

	sqe = &tgt->sq[tgt->sq_prod_idx];

	/* Fill SQ WQE */
	sqe->wqe = xid << FCOE_SQE_TASK_ID_SHIFT;
	sqe->wqe |= tgt->sq_curr_toggle_bit << FCOE_SQE_TOGGLE_BIT_SHIFT;

	/* Advance SQ Prod Idx */
	if (++tgt->sq_prod_idx == BNX2FC_SQ_WQES_MAX) {
		tgt->sq_prod_idx = 0;
		tgt->sq_curr_toggle_bit = 1 - tgt->sq_curr_toggle_bit;
	}
}

void bnx2fc_ring_doorbell(struct bnx2fc_rport *tgt)
{
	struct b577xx_doorbell_set_prod *sq_db = &tgt->sq_db;
	u32 msg;

	wmb();
	sq_db->prod = tgt->sq_prod_idx | 
				(tgt->sq_curr_toggle_bit << 15);
	msg = *((u32 *)sq_db);
	writel(cpu_to_le32(msg), tgt->ctx_base);
	mmiowb();

}

int bnx2fc_map_doorbell(struct bnx2fc_rport *tgt)
{
	u32 context_id = tgt->context_id;
	struct bnx2fc_port *port = tgt->port;
	u32 reg_off;
	resource_size_t reg_base;

	reg_base = pci_resource_start(port->hba->pcidev,
					BNX2X_DOORBELL_PCI_BAR);
	reg_off =  (1 << BNX2X_DB_SHIFT) * (context_id & 0x1FFFF);
	tgt->ctx_base = ioremap_nocache(reg_base + reg_off, 4);
	if (!tgt->ctx_base)
		return -ENOMEM;
	return 0;
}

char *bnx2fc_get_next_rqe(struct bnx2fc_rport *tgt, u8 num_items)
{
	char *buf = (char *)tgt->rq + (tgt->rq_cons_idx * BNX2FC_RQ_BUF_SZ);

	if (tgt->rq_cons_idx + num_items > BNX2FC_RQ_WQES_MAX)
		return NULL;

	tgt->rq_cons_idx += num_items;

	if (tgt->rq_cons_idx >= BNX2FC_RQ_WQES_MAX)
		tgt->rq_cons_idx -= BNX2FC_RQ_WQES_MAX;

	return buf;
}

void bnx2fc_return_rqe(struct bnx2fc_rport *tgt, u8 num_items)
{
	/* return the rq buffer */
	u32 next_prod_idx = tgt->rq_prod_idx + num_items;
	if ((next_prod_idx & 0x7fff) == BNX2FC_RQ_WQES_MAX) {
		/* Wrap around RQ */
		next_prod_idx += 0x8000 - BNX2FC_RQ_WQES_MAX;
	}
	tgt->rq_prod_idx = next_prod_idx;
	tgt->conn_db->rq_prod = tgt->rq_prod_idx;
}

void bnx2fc_init_cleanup_task(struct bnx2fc_cmd *io_req,
			      struct fcoe_task_ctx_entry *task,
			      u16 orig_xid)
{
	u8 task_type = FCOE_TASK_TYPE_EXCHANGE_CLEANUP;
	struct bnx2fc_rport *tgt = io_req->tgt;
	u32 context_id = tgt->context_id;

	memset(task, 0, sizeof(struct fcoe_task_ctx_entry));

	/* Tx Write Rx Read */
	/* init flags */
	task->txwr_rxrd.const_ctx.init_flags = task_type << 
				FCOE_TCE_TX_WR_RX_RD_CONST_TASK_TYPE_SHIFT;
	task->txwr_rxrd.const_ctx.init_flags |= FCOE_TASK_CLASS_TYPE_3 << 
				FCOE_TCE_TX_WR_RX_RD_CONST_CLASS_TYPE_SHIFT;
	task->txwr_rxrd.const_ctx.init_flags |= FCOE_TASK_DEV_TYPE_DISK << 
				FCOE_TCE_TX_WR_RX_RD_CONST_DEV_TYPE_SHIFT;
	task->txwr_rxrd.union_ctx.cleanup.ctx.cleaned_task_id = orig_xid;

	/* Tx flags */
	task->txwr_rxrd.const_ctx.tx_flags = FCOE_TASK_TX_STATE_EXCHANGE_CLEANUP << 
				FCOE_TCE_TX_WR_RX_RD_CONST_TX_STATE_SHIFT;

	/* Rx Read Tx Write */
	task->rxwr_txrd.const_ctx.init_flags = context_id <<
				FCOE_TCE_RX_WR_TX_RD_CONST_CID_SHIFT;
	task->rxwr_txrd.var_ctx.rx_flags |= 1 <<
				FCOE_TCE_RX_WR_TX_RD_VAR_EXP_FIRST_FRAME_SHIFT;
}

void bnx2fc_init_mp_task(struct bnx2fc_cmd *io_req,
				struct fcoe_task_ctx_entry *task)
{
	struct bnx2fc_hba *hba = io_req->port->hba;
	struct bnx2fc_mp_req *mp_req = &(io_req->mp_req);
	struct bnx2fc_rport *tgt = io_req->tgt;
	struct fc_frame_header *fc_hdr;
	u8 task_type = 0;
	u64 *hdr;
	u64 temp_hdr[3];
	u32 context_id;

	bnx2fc_dbg(LOG_IOERR, "Initializing MP task for cmd_type = %d\n",
		io_req->cmd_type);

	/* Obtain task_type */
	if ((io_req->cmd_type == BNX2FC_TASK_MGMT_CMD) ||
	    (io_req->cmd_type == BNX2FC_ELS)) {
		task_type = FCOE_TASK_TYPE_MIDPATH;
	} else if (io_req->cmd_type == BNX2FC_ABTS) {
		task_type = FCOE_TASK_TYPE_ABTS;
	}

	memset(task, 0, sizeof(struct fcoe_task_ctx_entry));

	/* Setup the task from io_req for easy reference */
	io_req->task = task;

	bnx2fc_dbg(LOG_IOERR, "task type = %d\n", task_type);

	/* Tx only */
	if ((task_type == FCOE_TASK_TYPE_MIDPATH) || 
	    (task_type == FCOE_TASK_TYPE_UNSOLICITED)) {
		task->txwr_only.sgl_ctx.sgl.mul_sgl.cur_sge_addr.lo = 
				(u32)mp_req->mp_req_bd_dma;
		task->txwr_only.sgl_ctx.sgl.mul_sgl.cur_sge_addr.hi = 
				(u32)((u64)mp_req->mp_req_bd_dma >> 32);
		task->txwr_only.sgl_ctx.sgl.mul_sgl.sgl_size = 1;
		bnx2fc_dbg(LOG_IOERR, "init_mp_task - bd_dma = 0x%llx\n",
			mp_req->mp_req_bd_dma);
	}

	/* Tx Write Rx Read */
	/* init flags */
	task->txwr_rxrd.const_ctx.init_flags = task_type << 
				FCOE_TCE_TX_WR_RX_RD_CONST_TASK_TYPE_SHIFT;
	task->txwr_rxrd.const_ctx.init_flags |= FCOE_TASK_DEV_TYPE_DISK << 
				FCOE_TCE_TX_WR_RX_RD_CONST_DEV_TYPE_SHIFT;
	task->txwr_rxrd.const_ctx.init_flags |= FCOE_TASK_CLASS_TYPE_3 << 
				FCOE_TCE_TX_WR_RX_RD_CONST_CLASS_TYPE_SHIFT;

	/* tx flags */
	task->txwr_rxrd.const_ctx.tx_flags = FCOE_TASK_TX_STATE_INIT << 
				FCOE_TCE_TX_WR_RX_RD_CONST_TX_STATE_SHIFT;

	task->txwr_rxrd.const_ctx.verify_tx_seq = 0;


	/* Rx Write Tx Read */
	task->rxwr_txrd.const_ctx.data_2_trns = io_req->data_xfer_len;

	/* rx flags */
	task->rxwr_txrd.var_ctx.rx_flags |= 1 <<
				FCOE_TCE_RX_WR_TX_RD_VAR_EXP_FIRST_FRAME_SHIFT;

	context_id = tgt->context_id;
	task->rxwr_txrd.const_ctx.init_flags = context_id <<
				FCOE_TCE_RX_WR_TX_RD_CONST_CID_SHIFT;

	fc_hdr = &(mp_req->req_fc_hdr);
	if (task_type == FCOE_TASK_TYPE_MIDPATH) {
		fc_hdr->fh_ox_id = cpu_to_be16(io_req->xid);
		fc_hdr->fh_rx_id = htons(0xffff);
		task->rxwr_txrd.var_ctx.rx_id = 0xffff;
	} else if (task_type == FCOE_TASK_TYPE_UNSOLICITED) {
		fc_hdr->fh_rx_id = cpu_to_be16(io_req->xid);
	}

	/* Fill FC Header into middle path buffer */
	hdr = (u64 *) &task->txwr_rxrd.union_ctx.tx_frame.fc_hdr;
	memcpy(temp_hdr, fc_hdr, sizeof(temp_hdr));
	hdr[0] = cpu_to_be64(temp_hdr[0]);
	hdr[1] = cpu_to_be64(temp_hdr[1]);
	hdr[2] = cpu_to_be64(temp_hdr[2]);

	/* Rx Only */
	if (task_type == FCOE_TASK_TYPE_MIDPATH) {
	
		task->rxwr_only.union_ctx.read_info.sgl_ctx.sgl.mul_sgl.cur_sge_addr.lo = 
				(u32)mp_req->mp_resp_bd_dma;
		task->rxwr_only.union_ctx.read_info.sgl_ctx.sgl.mul_sgl.cur_sge_addr.hi = 
				(u32)((u64)mp_req->mp_resp_bd_dma >> 32);
		task->rxwr_only.union_ctx.read_info.sgl_ctx.sgl.mul_sgl.sgl_size = 1;
	}
}

void bnx2fc_init_task(struct bnx2fc_cmd *io_req, 
		  	     struct fcoe_task_ctx_entry *task)
{
	u8 task_type;
	struct scsi_cmnd *sc_cmd = io_req->sc_cmd;
	struct io_bdt *bd_tbl = io_req->bd_tbl;
	struct bnx2fc_rport *tgt = io_req->tgt;
	struct fcoe_read_flow_info *rinfo=NULL;
	u64 *fcp_cmnd;
	u64 tmp_fcp_cmnd[4];
	u32 context_id;
	int cnt, i;
	int bd_count;

	memset(task, 0, sizeof(struct fcoe_task_ctx_entry));

	/* Setup the task from io_req for easy reference */
	io_req->task = task;

	if (sc_cmd->sc_data_direction == DMA_TO_DEVICE)
		task_type = FCOE_TASK_TYPE_WRITE;
	else
		task_type = FCOE_TASK_TYPE_READ;

	/* Tx only */
	bd_count = bd_tbl->bd_valid;
	if (task_type == FCOE_TASK_TYPE_WRITE) {
		if (bd_count == 1) {
			struct fcoe_bd_ctx *fcoe_bd_tbl = bd_tbl->bd_tbl;

			task->txwr_only.sgl_ctx.cached_sge.cur_buf_addr.lo =
					fcoe_bd_tbl->buf_addr_lo;
			task->txwr_only.sgl_ctx.cached_sge.cur_buf_addr.hi =
					fcoe_bd_tbl->buf_addr_hi;
			task->txwr_only.sgl_ctx.cached_sge.cur_buf_rem =
					fcoe_bd_tbl->buf_len;
			/* T7.2 only */
			rinfo = &task->rxwr_only.union_ctx.read_info;
			rinfo->sgl_ctx.cached_sge.cur_buf_addr.lo=
					fcoe_bd_tbl->buf_addr_lo;
			rinfo->sgl_ctx.cached_sge.cur_buf_addr.hi=
					fcoe_bd_tbl->buf_addr_hi;
			rinfo->sgl_ctx.cached_sge.cur_buf_rem=
					fcoe_bd_tbl->buf_len;

			task->txwr_rxrd.const_ctx.init_flags |= 1 <<
				FCOE_TCE_TX_WR_RX_RD_CONST_CACHED_SGE_SHIFT;
		} else {
			task->txwr_only.sgl_ctx.sgl.mul_sgl.cur_sge_addr.lo =
					(u32)bd_tbl->bd_tbl_dma;
			task->txwr_only.sgl_ctx.sgl.mul_sgl.cur_sge_addr.hi =
					(u32)((u64)bd_tbl->bd_tbl_dma >> 32);
			task->txwr_only.sgl_ctx.sgl.mul_sgl.sgl_size =
					bd_tbl->bd_valid;
		}
	}

	/*Tx Write Rx Read */
	/* Init state to NORMAL */
	task->txwr_rxrd.const_ctx.init_flags |= task_type << 
				FCOE_TCE_TX_WR_RX_RD_CONST_TASK_TYPE_SHIFT;
	task->txwr_rxrd.const_ctx.init_flags |= FCOE_TASK_DEV_TYPE_DISK << 
				FCOE_TCE_TX_WR_RX_RD_CONST_DEV_TYPE_SHIFT;
	task->txwr_rxrd.const_ctx.init_flags |= FCOE_TASK_CLASS_TYPE_3 << 
				FCOE_TCE_TX_WR_RX_RD_CONST_CLASS_TYPE_SHIFT;
	/* tx flags */
	task->txwr_rxrd.const_ctx.tx_flags = FCOE_TASK_TX_STATE_NORMAL << 
				FCOE_TCE_TX_WR_RX_RD_CONST_TX_STATE_SHIFT;

	/* Set initiative ownership */
	task->txwr_rxrd.const_ctx.verify_tx_seq = 0;

	/* Set initial seq counter */
	task->txwr_rxrd.union_ctx.tx_seq.ctx.seq_cnt = 1;

	/* Fill FCP_CMND IU */
	fcp_cmnd = (u64 *)
		    task->txwr_rxrd.union_ctx.fcp_cmd.opaque;
	bnx2fc_build_fcp_cmnd(io_req, (struct fcp_cmnd *)&tmp_fcp_cmnd);

	/* swap fcp_cmnd */
	cnt = sizeof(struct fcp_cmnd) / sizeof(u64);
	
	for (i=0; i < cnt; i++) {
                *fcp_cmnd = cpu_to_be64(tmp_fcp_cmnd[i]);
		fcp_cmnd++;
        }

	/* Rx Write Tx Read */
	task->rxwr_txrd.const_ctx.data_2_trns = io_req->data_xfer_len;

	context_id = tgt->context_id;
	task->rxwr_txrd.const_ctx.init_flags = context_id <<
				FCOE_TCE_RX_WR_TX_RD_CONST_CID_SHIFT;

	/* rx flags */
	/* Set state to "waiting for the first packet" */
	task->rxwr_txrd.var_ctx.rx_flags |= 1 <<
				FCOE_TCE_RX_WR_TX_RD_VAR_EXP_FIRST_FRAME_SHIFT;
	
	task->rxwr_txrd.var_ctx.rx_id = 0xffff;

	/* Rx Only */
	if (task_type == FCOE_TASK_TYPE_READ) {

		bd_count = bd_tbl->bd_valid;
		if (bd_count == 1) {

			struct fcoe_bd_ctx *fcoe_bd_tbl = bd_tbl->bd_tbl;

			task->rxwr_only.union_ctx.read_info.sgl_ctx.cached_sge.cur_buf_addr.lo
					= fcoe_bd_tbl->buf_addr_lo;
			task->rxwr_only.union_ctx.read_info.sgl_ctx.cached_sge.cur_buf_addr.hi
					= fcoe_bd_tbl->buf_addr_hi;
			task->rxwr_only.union_ctx.read_info.sgl_ctx.cached_sge.cur_buf_rem
					= fcoe_bd_tbl->buf_len;
			task->txwr_rxrd.const_ctx.init_flags |= 1 << 
				FCOE_TCE_TX_WR_RX_RD_CONST_CACHED_SGE_SHIFT;
		} else if (bd_count == 2) {
			struct fcoe_bd_ctx *fcoe_bd_tbl = bd_tbl->bd_tbl;

			task->rxwr_only.union_ctx.read_info.sgl_ctx.cached_sge.cur_buf_addr.lo
					= fcoe_bd_tbl->buf_addr_lo;
			task->rxwr_only.union_ctx.read_info.sgl_ctx.cached_sge.cur_buf_addr.hi
					= fcoe_bd_tbl->buf_addr_hi;
			task->rxwr_only.union_ctx.read_info.sgl_ctx.cached_sge.cur_buf_rem
					= fcoe_bd_tbl->buf_len;

			fcoe_bd_tbl++;
			task->rxwr_only.union_ctx.read_info.sgl_ctx.cached_sge.second_buf_addr.lo
					= fcoe_bd_tbl->buf_addr_lo;
			task->rxwr_only.union_ctx.read_info.sgl_ctx.cached_sge.second_buf_addr.hi
					= fcoe_bd_tbl->buf_addr_hi;
			task->rxwr_only.union_ctx.read_info.sgl_ctx.cached_sge.second_buf_rem
					= fcoe_bd_tbl->buf_len;
			task->txwr_rxrd.const_ctx.init_flags |= 1 << 
				FCOE_TCE_TX_WR_RX_RD_CONST_CACHED_SGE_SHIFT;
		} else { 

			task->rxwr_only.union_ctx.read_info.sgl_ctx.sgl.mul_sgl.cur_sge_addr.lo =
					(u32)bd_tbl->bd_tbl_dma;
			task->rxwr_only.union_ctx.read_info.sgl_ctx.sgl.mul_sgl.cur_sge_addr.hi =
					(u32)((u64)bd_tbl->bd_tbl_dma >> 32);
			task->rxwr_only.union_ctx.read_info.sgl_ctx.sgl.mul_sgl.sgl_size =
					bd_count;
		}
	}
}

/**
 * bnx2fc_setup_task_ctx - allocate and map task context
 *
 * @hba:	pointer to adapter structure
 *
 * allocate memory for task context, and associated BD table to be used
 * by firmware
 *
 */
int bnx2fc_setup_task_ctx(struct bnx2fc_hba *hba)
{
	int rc = 0;
	struct regpair *task_ctx_bdt;
	dma_addr_t addr;
	int i;

	/* 
	 * Allocate task context bd table. A page size of bd table
	 * can map 256 buffers. Each buffer contains 32 task context
	 * entries. Hence the limit with one page is 8192 task context
	 * entries.
	 */
	hba->task_ctx_bd_tbl = bnx2fc_alloc_dma(hba,
						    PAGE_SIZE, 
						    &hba->task_ctx_bd_dma);
	if (!hba->task_ctx_bd_tbl) {
		printk(KERN_ERR PFX "unable to allocate task context BDT\n");
		rc = -1;
		goto out;
	}
	memset(hba->task_ctx_bd_tbl, 0, PAGE_SIZE);

	/* 
	 * Allocate task_ctx which is an array of pointers pointing to
	 * a page containing 32 task contexts
	 */
	hba->task_ctx = kzalloc((BNX2FC_TASK_CTX_ARR_SZ * sizeof(void *)),
				 GFP_KERNEL);
	if (!hba->task_ctx) {
		printk(KERN_ERR PFX "unable to allocate task context array\n");
		rc = -1;
		goto out1;
	}

	/*
	 * Allocate task_ctx_dma which is an array of dma addresses
	 */
	hba->task_ctx_dma = kmalloc((BNX2FC_TASK_CTX_ARR_SZ * 
					sizeof(dma_addr_t)), GFP_KERNEL);
	if (!hba->task_ctx_dma) {
		printk(KERN_ERR PFX "unable to alloc context mapping array\n");
		rc = -1;
		goto out2;
	}

	task_ctx_bdt = (struct regpair *)hba->task_ctx_bd_tbl;
	for (i=0; i < BNX2FC_TASK_CTX_ARR_SZ; i++) {

		hba->task_ctx[i] = bnx2fc_alloc_dma(hba,
					PAGE_SIZE, &hba->task_ctx_dma[i]);
		if (!hba->task_ctx[i]) {
			printk(KERN_ERR PFX "unable to alloc task context\n");
			rc = -1;
			goto out3;
		}
		memset(hba->task_ctx[i], 0, PAGE_SIZE);
		addr = (u64)hba->task_ctx_dma[i];
		task_ctx_bdt->hi = cpu_to_le32((u64)addr >> 32);
		task_ctx_bdt->lo = cpu_to_le32((u32)addr);
		task_ctx_bdt++;
	}
	return 0;

out3: 
	for (i=0; i < BNX2FC_TASK_CTX_ARR_SZ; i++) {
		if (hba->task_ctx[i]) {

			bnx2fc_free_dma(hba, PAGE_SIZE,
				hba->task_ctx[i], hba->task_ctx_dma[i]);
			hba->task_ctx[i] = NULL;
		}
	}

	kfree(hba->task_ctx_dma);
	hba->task_ctx_dma = NULL;
out2: 
	kfree(hba->task_ctx);
	hba->task_ctx = NULL;
out1: 
	bnx2fc_free_dma(hba, PAGE_SIZE,
			hba->task_ctx_bd_tbl, hba->task_ctx_bd_dma);
	hba->task_ctx_bd_tbl = NULL;
out: 
	return rc;
}

void bnx2fc_free_task_ctx(struct bnx2fc_hba *hba)
{
	int i;
	
	if (hba->task_ctx_bd_tbl) {
		bnx2fc_free_dma(hba, PAGE_SIZE,
				    hba->task_ctx_bd_tbl, 
				    hba->task_ctx_bd_dma);
		hba->task_ctx_bd_tbl = NULL;
	}

	if (hba->task_ctx) {
		for (i=0; i < BNX2FC_TASK_CTX_ARR_SZ; i++) {
			if (hba->task_ctx[i]) {
				bnx2fc_free_dma(hba, PAGE_SIZE,
						    hba->task_ctx[i],
						    hba->task_ctx_dma[i]);
				hba->task_ctx[i] = NULL;
			}
		}
		kfree(hba->task_ctx);
		hba->task_ctx = NULL;
	}
	
	if (hba->task_ctx_dma) {
		kfree(hba->task_ctx_dma);
		hba->task_ctx_dma = NULL;
	}
}
		
static void bnx2fc_free_hash_table(struct bnx2fc_hba *hba)
{
	int i;
	int segment_count;
	int hash_table_size;
	u32 *pbl;

	segment_count = hba->hash_tbl_segment_count;
	hash_table_size = BNX2FC_NUM_MAX_SESS * BNX2FC_MAX_ROWS_IN_HASH_TBL *
		sizeof(struct fcoe_hash_table_entry);

	pbl = hba->hash_tbl_pbl;
	for (i = 0; i < segment_count; ++i) {
		dma_addr_t dma_address;

		dma_address = le32_to_cpu(*pbl);
		++pbl;
		dma_address += ((u64)le32_to_cpu(*pbl)) << 32;
		++pbl;
		bnx2fc_free_dma(hba, BNX2FC_HASH_TBL_CHUNK_SIZE,
				    hba->hash_tbl_segments[i],
				    dma_address);

	}

	if (hba->hash_tbl_pbl) {
		bnx2fc_free_dma(hba, PAGE_SIZE,
				    hba->hash_tbl_pbl,
				    hba->hash_tbl_pbl_dma);
		hba->hash_tbl_pbl = NULL;
	}
}

static int bnx2fc_allocate_hash_table(struct bnx2fc_hba *hba)
{
	int i;
	int hash_table_size;
	int segment_count;
	int segment_array_size;
	int dma_segment_array_size;
	dma_addr_t *dma_segment_array;
	u32 *pbl;

	hash_table_size = BNX2FC_NUM_MAX_SESS * BNX2FC_MAX_ROWS_IN_HASH_TBL *
		sizeof(struct fcoe_hash_table_entry);

	segment_count = hash_table_size + BNX2FC_HASH_TBL_CHUNK_SIZE - 1;
	segment_count /= BNX2FC_HASH_TBL_CHUNK_SIZE;
	hba->hash_tbl_segment_count = segment_count;

	segment_array_size = segment_count * sizeof(*hba->hash_tbl_segments);
	hba->hash_tbl_segments = kzalloc(segment_array_size, GFP_KERNEL);
	if (!hba->hash_tbl_segments) {
		printk(KERN_ERR PFX "hash table pointers alloc failed\n");
		return -ENOMEM;
	}
	dma_segment_array_size = segment_count * sizeof(*dma_segment_array);
	dma_segment_array = kzalloc(dma_segment_array_size, GFP_KERNEL);
	if (!dma_segment_array) {
		printk(KERN_ERR PFX "hash table pointers (dma) alloc failed\n");
		return -ENOMEM;
	}

	for (i = 0; i < segment_count; ++i) {
		hba->hash_tbl_segments[i] =
			bnx2fc_alloc_dma(hba,
					     BNX2FC_HASH_TBL_CHUNK_SIZE,
					     &dma_segment_array[i]);
		bnx2fc_dbg(LOG_INIT, "hash table segment[%i]: %p: %llx\n",
		       i, hba->hash_tbl_segments[i],
		       dma_segment_array[i]);
		if (!hba->hash_tbl_segments[i]) {
			printk(KERN_ERR PFX "hash segment alloc failed\n");
			while (--i >= 0) {
				bnx2fc_free_dma(hba,
						    BNX2FC_HASH_TBL_CHUNK_SIZE,
						    hba->hash_tbl_segments[i],
						    dma_segment_array[i]);
				hba->hash_tbl_segments[i] = NULL;
			}
			kfree(dma_segment_array);
			return -ENOMEM;
		}
		memset(hba->hash_tbl_segments[i], 0,
		       BNX2FC_HASH_TBL_CHUNK_SIZE);
	}

	hba->hash_tbl_pbl = bnx2fc_alloc_dma(hba,
						 PAGE_SIZE,
						 &hba->hash_tbl_pbl_dma);
	bnx2fc_dbg(LOG_INIT, "hash table pbl: %p: %llx\n",
		       hba->hash_tbl_pbl, hba->hash_tbl_pbl_dma);
	if (!hba->hash_tbl_pbl) {
		printk(KERN_ERR PFX "hash table pbl alloc failed\n");
		kfree(dma_segment_array);
		return -ENOMEM;
	}
	memset(hba->hash_tbl_pbl, 0, PAGE_SIZE);

	pbl = hba->hash_tbl_pbl;
	for (i = 0; i < segment_count; ++i) {
		u64 paddr = dma_segment_array[i];
		*pbl = cpu_to_le32((u32) paddr);
		++pbl;
		*pbl = cpu_to_le32((u32) (paddr >> 32));
		++pbl;
	}
	pbl = hba->hash_tbl_pbl;
	i = 0;
	while (*pbl && *(pbl + 1)) {
		u32 lo;
		u32 hi;
		lo = *pbl;
		++pbl;
		hi = *pbl;
		++pbl;
		bnx2fc_dbg(LOG_INIT, "hash table[%d]: 0x%08x%08x\n", i, hi, lo);
		++i;
	}
	kfree(dma_segment_array);
	return 0;
}

/**
 * bnx2fc_setup_fw_resc - Allocate and map hash table and dummy buffer
 *
 * @hba:	Pointer to adapter structure
 *
 */
int bnx2fc_setup_fw_resc(struct bnx2fc_hba *hba)
{
	u64 addr;
	u32 mem_size;
	int i;

	if (bnx2fc_allocate_hash_table(hba))
		return -ENOMEM;

	mem_size = BNX2FC_NUM_MAX_SESS * sizeof(struct regpair);
	hba->t2_hash_tbl_ptr = bnx2fc_alloc_dma(hba, mem_size,
						    &hba->t2_hash_tbl_ptr_dma);
	if (!hba->t2_hash_tbl_ptr) {
		printk (KERN_ERR PFX "unable to allocate t2 hash table ptr\n");
		bnx2fc_free_fw_resc(hba);
		return -ENOMEM;
	}
	memset(hba->t2_hash_tbl_ptr, 0x00, mem_size);
	bnx2fc_dbg(LOG_INIT, "t2_hash_tbl_ptr = 0x%p, DMA = 0x%llx\n",
		hba->t2_hash_tbl_ptr, hba->t2_hash_tbl_ptr_dma);

	mem_size = BNX2FC_NUM_MAX_SESS * sizeof(struct fcoe_t2_hash_table_entry);
	hba->t2_hash_tbl = bnx2fc_alloc_dma(hba, mem_size,
						&hba->t2_hash_tbl_dma);
	if (!hba->t2_hash_tbl) {
		printk (KERN_ERR PFX "unable to allocate t2 hash table\n");
		bnx2fc_free_fw_resc(hba);
		return -ENOMEM;
	}
	memset(hba->t2_hash_tbl, 0x00, mem_size);
	for (i=0; i < BNX2FC_NUM_MAX_SESS; i++) {
		addr = (unsigned long) hba->t2_hash_tbl_dma +
			 ((i+1) * sizeof(struct fcoe_t2_hash_table_entry));
		hba->t2_hash_tbl[i].next.lo = addr & 0xffffffff;
		hba->t2_hash_tbl[i].next.hi = addr >> 32;
	}
	bnx2fc_dbg(LOG_INIT, "t2_hash_tbl = 0x%p, DMA = 0x%llx\n",
		hba->t2_hash_tbl, hba->t2_hash_tbl_dma);

	hba->dummy_buffer = bnx2fc_alloc_dma(hba,
                                     PAGE_SIZE, &hba->dummy_buf_dma);
        if (!hba->dummy_buffer) {
                printk(KERN_ERR PFX "unable to alloc MP Dummy Buffer\n");
		bnx2fc_free_fw_resc(hba);
		return -ENOMEM;
        }
	
	hba->stats_buffer = bnx2fc_alloc_dma(hba, PAGE_SIZE,
						 &hba->stats_buf_dma);
	if (!hba->stats_buffer) {
		printk(KERN_ERR PFX "unable to alloc Stats Buffer\n");
		bnx2fc_free_fw_resc(hba);
		return -ENOMEM;
	}
	memset(hba->stats_buffer, 0x00, PAGE_SIZE);

	return 0;
}

void bnx2fc_free_fw_resc(struct bnx2fc_hba *hba)
{
	u32 mem_size;

	if (hba->stats_buffer) {
		bnx2fc_free_dma(hba, PAGE_SIZE, hba->stats_buffer,
				    hba->stats_buf_dma);
		hba->stats_buffer = NULL;
	}

	if (hba->dummy_buffer) {
		bnx2fc_free_dma(hba, PAGE_SIZE, hba->dummy_buffer,
				    hba->dummy_buf_dma);
		hba->dummy_buffer = NULL;
	}
 
	if (hba->t2_hash_tbl_ptr) {
		mem_size = BNX2FC_NUM_MAX_SESS * sizeof(struct regpair);
		bnx2fc_free_dma(hba, mem_size,
				    hba->t2_hash_tbl_ptr, 
				    hba->t2_hash_tbl_ptr_dma);
		hba->t2_hash_tbl_ptr = NULL;
	}

	if (hba->t2_hash_tbl) {
		mem_size = BNX2FC_NUM_MAX_SESS * 
			    sizeof(struct fcoe_t2_hash_table_entry);
		bnx2fc_free_dma(hba, mem_size,
				    hba->t2_hash_tbl, hba->t2_hash_tbl_dma);
		hba->t2_hash_tbl = NULL;
	}
	bnx2fc_free_hash_table(hba);
}
