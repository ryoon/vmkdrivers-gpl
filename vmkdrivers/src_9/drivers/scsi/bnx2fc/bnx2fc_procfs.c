/*
 * QLogic NetXtreme II Linux FCoE offload driver.
 * Copyright (c)   2003-2014 QLogic Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 */

#include "bnx2fc.h"

#if defined(__VMKLNX__)

enum _bnx2fc_verbose_level{
	BNX2FC_VERBOSE_FC = 0,
	BNX2FC_VERBOSE_HBA,
	BNX2FC_VERBOSE_LPORT,
	BNX2FC_VERBOSE_RPORT,

	/* END.. */
	BNX2FC_VERBOSE_ALL,
};

static unsigned int bnx2fc_verbose = BNX2FC_VERBOSE_LPORT;

struct bnx2fc_proc_param {
	const char *name;
	int type;
	unsigned int *value;
};

#define BNX2FC_PARAM_TYPE_HEX		1
#define BNX2FC_PARAM_TYPE_DEC		2
#define BNX2FC_PARAM_TYPE_STR		3

static struct bnx2fc_proc_param proc_param[] = {
	{ "bnx2fc_debug_level=", BNX2FC_PARAM_TYPE_HEX, (unsigned int *)
			&bnx2fc_debug_level},
	{ "bnx2fc_verbose=", BNX2FC_PARAM_TYPE_DEC, &bnx2fc_verbose},
	{ NULL, 0, NULL },
};

struct info_str {
	char *buffer;
	int length;
	off_t offset;
	int pos;
};

static void
copy_mem_info(struct info_str *info, char *data, int len)
{
	if (info->pos + len > info->offset + info->length)
		len = info->offset + info->length - info->pos;

	if (info->pos + len < info->offset) {
		info->pos += len;
		return;
	}

	if (info->pos < info->offset) {
		off_t partial;

		partial = info->offset - info->pos;
		data += partial;
		info->pos += partial;
		len  -= partial;
	}

	if (len > 0) {
		memcpy(info->buffer, data, len);
		info->pos += len;
		info->buffer += len;
	}
}

static int
copy_info(struct info_str *info, char *fmt, ...)
{
	va_list args;
	char buf[256];
	int len;

	va_start(args, fmt);
	len = vsprintf(buf, fmt, args);
	va_end(args);

	copy_mem_info(info, buf, len);
	return len;
}

static void
bnx2fc_read_proc_data(struct bnx2fc_hba *hba, char *buffer)
{
	int i;

	for (i = 0; proc_param[i].name != NULL; i++) {
		if (strncmp(buffer, proc_param[i].name,
		    strlen(proc_param[i].name)) == 0) {
			switch (proc_param[i].type) {
			case BNX2FC_PARAM_TYPE_HEX:
				sscanf(buffer + strlen(proc_param[i].name),
					"0x%x", proc_param[i].value);
				break;
			case BNX2FC_PARAM_TYPE_DEC:
				sscanf(buffer + strlen(proc_param[i].name),
					"%d", proc_param[i].value);
				break;
			default:
				printk(KERN_ERR "bnx2fc: unknown proc param\n");
			}
		}
	}
}

static void
bnx2fc_fcinfo_rport(struct info_str *info, struct fc_lport *lport)
{
	struct fc_disc *disc;
	struct fc_rport_priv *rdata;
	int index = 1;

	disc = &lport->disc;
	mutex_lock(&disc->disc_mutex);
	list_for_each_entry(rdata, &disc->rports, peers) {
		struct fc_rport_identifiers *ids = &rdata->ids;

		copy_info(info, "  Port%d\n", index);
		copy_info(info, "   WWNN      %16.16llx\n", ids->node_name);
		copy_info(info, "   WWPN      %16.16llx\n", ids->port_name);
		copy_info(info, "   PortID    0x%x\n", ids->port_id);

		index++;
	}
	mutex_unlock(&disc->disc_mutex);
}

static void
bnx2fc_fcinfo_fcf(struct info_str *info, struct bnx2fc_hba *hba)
{
	struct fcoe_ctlr *fip = &hba->ctlr;
	struct fcoe_fcf *fcf, *next;
	int fcf_cnt = 1;

	copy_info(info, "FCFs List :\n");
	list_for_each_entry_safe(fcf, next, &fip->fcfs, list) {
		copy_info(info, "  FCF%d\n", fcf_cnt);
		copy_info(info,	"   Switch Name         %16.16llx\n",
				fcf->switch_name);
		copy_info(info,	"   Fabric Name         %16.16llx\n",
				fcf->fabric_name);
		copy_info(info, "   FC Map              %x\n", fcf->fc_map);
		copy_info(info, "   VFID                %d\n", fcf->vfid);
		copy_info(info, "   Priority            %x\n", fcf->pri);
		copy_info(info, "   Flags               %x\n", fcf->flags);
		copy_info(info, "   Keep Alive Period   %x\n", fcf->fka_period);
		copy_info(info, "   MAC Address         "
				"%02x:%02x:%02x:%02x:%02x:%02x\n",
			fcf->fcf_mac[0], fcf->fcf_mac[1], fcf->fcf_mac[2],
			fcf->fcf_mac[3], fcf->fcf_mac[4], fcf->fcf_mac[5]);
		copy_info(info, "   Type                %s\n",
			fcf == fip->sel_fcf ? "Selected" : "Backup");
		fcf_cnt++;
	}
}

static void
bnx2fc_fcinfo_wwn(struct info_str *info, struct fc_lport *lport)
{
	mutex_lock(&lport->lp_mutex);
	copy_info(info, "Physical Port WWNN    %16.16llx\n", lport->wwnn);
	copy_info(info, "Physical Port WWPN    %16.16llx\n", lport->wwpn);
	copy_info(info, "PortID                0x%x\n",
			fc_host_port_id(lport->host));
	mutex_unlock(&lport->lp_mutex);
}

const char *bnx2fc_lport_state_str[] = {
	"LPORT_ST_DISABLED", "LPORT_ST_FLOGI", "LPORT_ST_DNS",
	"LPORT_ST_RNN_ID", "LPORT_ST_RSNN_NN", "LPORT_ST_RSPN_ID",
	"LPORT_ST_RFT_ID", "LPORT_ST_RFF_ID", "LPORT_ST_SCR",
	"LPORT_ST_READY", "LPORT_ST_LOGO", "LPORT_ST_RESET"
};


static void
bnx2fc_fcinfo_lport(struct info_str *info, struct fc_lport *lp)
{
#ifdef _BNX2FC_EN_STATS_
	struct fcoe_dev_stats *ds = NULL;
	struct fc_host_statistics *hs = NULL;
#endif

	if (!info || !lp)
		return;

	copy_info(info, "   lport(%p) information\n", lp);

	copy_info(info, "     link_up:%d qfull:%d boot_time:0x%lx"
			" retry_count:%d\n", lp->link_up, lp->qfull,
			lp->boot_time, lp->retry_count);

	if (lp->state <  sizeof(bnx2fc_lport_state_str)/sizeof(char *))
		copy_info(info, "     State: %s\n",
			bnx2fc_lport_state_str[lp->state]);
	else
		copy_info(info, "     State: Unknown\n");

	copy_info(info, "     WWNN[%16.16llx] WWPN[%16.16llx] "
			"SParam:0x%x e_d_tov:%d r_a_tov:%d \n",
			lp->wwnn, lp->wwpn, lp->service_params,
			lp->e_d_tov, lp->r_a_tov);

	copy_info(info, "     sg_supp: %s\n",
			(lp->sg_supp == 1) ? "Enabled" : "Disabled");
	copy_info(info, "     seq_offload: %s\n",
			(lp->seq_offload == 1) ? "Enabled" : "Disabled");
	copy_info(info, "     crc_offload: %s\n",
			(lp->crc_offload == 1) ? "Enabled" : "Disabled");
	copy_info(info, "     lro_enabled: %s\n",
			(lp->lro_enabled == 1) ? "Enabled" : "Disabled");
	copy_info(info, "     does_npiv: %s\n",
			(lp->does_npiv == 1) ? "Yes" : "No");
	copy_info(info, "     npiv_enabled: %s\n",
			(lp->npiv_enabled == 1) ? "Enabled" : "Disabled");

	copy_info(info, "     mfs:%d max_retry_count:%d"
			" max_rport_retry_count:%d\n", lp->mfs,
			lp->max_retry_count, lp->max_rport_retry_count);

	copy_info(info, "     link_supported_speed: ");
	if (lp->link_supported_speeds & FC_PORTSPEED_1GBIT)
		copy_info(info, "1GB, "); 
	if (lp->link_supported_speeds & FC_PORTSPEED_2GBIT)
		copy_info(info, "2GB, "); 
	if (lp->link_supported_speeds & FC_PORTSPEED_4GBIT)
		copy_info(info, "4GB, "); 
	if (lp->link_supported_speeds & FC_PORTSPEED_10GBIT)
		copy_info(info, "10GB, "); 
	if (lp->link_supported_speeds & FC_PORTSPEED_8GBIT)
		copy_info(info, "8GB, "); 
	if (lp->link_supported_speeds & FC_PORTSPEED_16GBIT)
		copy_info(info, "16GB, "); 
	if (lp->link_supported_speeds & FC_PORTSPEED_NOT_NEGOTIATED)
		copy_info(info, "NOT_NEGOTIATED"); 
	copy_info(info, "\n");

	switch (lp->link_speed) {
	case FC_PORTSPEED_1GBIT:
		copy_info(info, "     link_speed: 1GB\n");
		break;
	case FC_PORTSPEED_2GBIT:
		copy_info(info, "     link_speed: 2GB\n");
		break;
	case FC_PORTSPEED_4GBIT:
		copy_info(info, "     link_speed: 4GB\n");
		break;
	case FC_PORTSPEED_10GBIT:
		copy_info(info, "     link_speed: 10GB\n");
		break;
	case FC_PORTSPEED_8GBIT:
		copy_info(info, "     link_speed: 8GB\n");
		break;
	case FC_PORTSPEED_16GBIT:
		copy_info(info, "     link_speed: 16GB\n");
		break;
	case FC_PORTSPEED_UNKNOWN:
	default:
		copy_info(info, "     link_speed: Unknown\n");
		break;
	}
	copy_info(info, "     lro_xid:%d lso_max:%d \n",
			lp->lro_xid, lp->lso_max);

#ifdef _BNX2FC_EN_STATS_
	hs = &lp->host_stats;

	copy_info(info, "     HOST_STATS \n");
	copy_info(info, "      seconds_since_last_reset:%d tx_frames:%s"
			"tx_words:%d rx_frames:%d\n",
			hs->seconds_since_last_reset,
			hs->tx_frames, hs->tx_words, hs->rx_frames);
	copy_info(info, "      rx_words:%d lip_count:%d nos_count:%d"
			" error_frames:%d\n", hs->rx_words, hs->lip_count,
			hs->nos_count, hs->error_frames);
	copy_info(info, "      dumped_frames:%d link_failure_count:%d"
			" loss_of_sync_count:%d loss_of_signal_count:%d\n",
			hs->dumped_frames, hs->link_failure_count,
			hs->loss_of_sync_count, hs->loss_of_signal_count);
	copy_info(info, "      prim_seq_protocol_err_count:%s"
			" invalid_tx_word_count:%d invalid_crc_count:%d\n",
			hs->prim_seq_protocol_err_count,
			hs->invalid_tx_word_count, hs->invalid_crc_count);
	/* fc4 statistics  (only FCP supported currently) */
	copy_info(info, "      fcp_input_requests:%d fcp_output_requests:%d"
			" fcp_control_requests:%d\n", hs->fcp_input_requests,
			hs->fcp_output_requests, hs->fcp_control_requests);
	copy_info(info, "      fcp_input_megabytes:%d fcp_output_megabyte:%d\n",
			hs->fcp_input_megabytes, hs->fcp_output_megabytes);

	ds = lp->dev_stats;
	copy_info(info, "     DEV_STATS \n");

	copy_info(info, "       SecondsSinceLastReset:%d TxFrames:%d"
				" TxWords:%d\n", ds->SecondsSinceLastReset,
				ds->TxFrames, ds->TxWords);
	copy_info(info, "       RxFrames:%d RxWords:%d ErrorFrames:%d"
			" DumpedFrames:%d\n", ds->RxFrames, ds->RxWords,
				ds->ErrorFrames, ds->DumpedFrames);
	copy_info(info, "       LinkFailureCount:%d LossOfSignalCount:%d"
			" InvalidTxWordCount:%d\n", ds->LinkFailureCount,
				ds->LossOfSignalCount, ds->InvalidTxWordCount);
	copy_info(info, "       InvalidCRCCount:%d InputRequests:%d"
			" OutputRequests:%d ControlRequests:%d\n",
				ds->InvalidCRCCount,
				ds->InputRequests, ds->OutputRequests,
				ds->ControlRequests);
	copy_info(info, "       InputMegabytes:%d OutputMegabytes:%d"
			" VLinkFailureCount:%d MissDiscAdvCount:%d\n",
			ds->InputMegabytes, ds->OutputMegabytes,
			ds->VLinkFailureCount, ds->MissDiscAdvCount);

#endif
}


static void
bnx2fc_show_fcinfo(struct Scsi_Host *shost, struct info_str *info)
{
	struct fc_lport *lport = NULL;
	struct bnx2fc_port *port;
	struct bnx2fc_hba *hba;

	lport = shost_priv(shost);
	port = lport_priv(lport);
	hba = port->hba;

	bnx2fc_fcinfo_wwn(info, lport);
	bnx2fc_fcinfo_fcf(info, hba);
	bnx2fc_fcinfo_rport(info, lport);
}

static void
bnx2fc_show_scsi_cmd(struct info_str *info, struct bnx2fc_cmd *cmd)
{
	struct scsi_cmnd *sc = cmd->sc_cmd;
	int i;

	if (!sc)
		return;

	copy_info(info, "       scsi_cmnd:%p, device:%p, done:%p\n",
			sc, sc->device, sc->done);
	copy_info(info, "       cmd_len:%d, eh_eflags:0x%x, ser_no:%d\n",
			sc->cmd_len, sc->eh_eflags, sc->serial_number);
	copy_info(info, "       jiffies_at_alloc:0x%lx, retries:%d,"
			"allowed:%d timeout_per_command:%d\n",
			sc->jiffies_at_alloc, sc->retries, sc->allowed,
			sc->timeout_per_command);
	if (sc->sc_data_direction == DMA_BIDIRECTIONAL)
		copy_info(info, "       Data Xfer Direction BIDIRECTIONAL\n");
	if (sc->sc_data_direction == DMA_TO_DEVICE)
		copy_info(info, "       Data Xfer Direction I --> T (Write)\n");
	if (sc->sc_data_direction == DMA_FROM_DEVICE)
		copy_info(info, "       Data Xfer Direction T --> I (Read)\n");
	if (sc->sc_data_direction == DMA_NONE)
		copy_info(info, "       NO Data Xfer Requested\n");

	copy_info(info, "       CDB: ");
	for (i = 0; i < sc->cmd_len; i++)
		copy_info(info, "0x%02x  ", sc->cmnd[i]);
	copy_info(info, "\n");

	copy_info(info, "       request_bufflen:%d, request_buffer:%p,"
			" use_sg:%d\n", sc->request_bufflen,
			sc->request_buffer, sc->use_sg);
	copy_info(info, "       underflow:%d, transfersize:%d, resid:%d \n",
			sc->underflow, sc->transfersize, sc->resid);
}
static void
bnx2fc_show_cmd(struct info_str *info, struct bnx2fc_cmd *cmd)
{
	struct io_bdt *bdt = NULL;
	u16 i = 0;

	if (!info || !cmd)
		return;

	copy_info(info, "      bnx2fc_cmd(%p): type: ", cmd);

	switch (cmd->cmd_type) {
	case BNX2FC_SCSI_CMD:
		copy_info(info, "SCSI_CMD\n");
		break;
	case BNX2FC_TASK_MGMT_CMD:
		copy_info(info, "TASK_MGMT\n");
		break;
	case BNX2FC_ABTS:
		copy_info(info, "ABTS\n");
		break;
	case BNX2FC_ELS:
		copy_info(info, "ELS\n");
		break;
	case BNX2FC_CLEANUP:
		copy_info(info, "CLEANUP\n");
		break;
	default:
		copy_info(info, "Unknown\n");
		break;
	}

	if (bnx2fc_verbose < BNX2FC_VERBOSE_ALL)
		return;

	copy_info(info, "      io_req_flags:0x%x port:%p tgt:%p sc_cmd:%p\n",
			cmd->io_req_flags, cmd->port, cmd->tgt, cmd->sc_cmd);

	copy_info(info, "      xid:0x%x req_flags:0x%x data_xfr_len:%d"
			" fcp_rsp:%p\n", cmd->xid, cmd->req_flags,
			cmd->data_xfer_len, cmd->rsp);

	copy_info(info, "      fcp_resid:0x%d fcp_rsp_len:%d fcp_sns_len:%d\n",
			cmd->fcp_resid, cmd->fcp_rsp_len, cmd->fcp_sns_len);

	bdt = cmd->bd_tbl;

	for (i = 0; i < bdt->bd_valid; i++) {
		copy_info(info, "       BD[%i]:addr_hi:lo[0x%x:%x] len:%d "
			"Flags:0x%x\n", i, bdt->bd_tbl->buf_addr_hi,
			bdt->bd_tbl->buf_addr_lo, bdt->bd_tbl->buf_len,
			bdt->bd_tbl->flags);
	}

	copy_info(info, "      req_flags: following bits set - ");
	if (test_bit(BNX2FC_FLAG_ISSUE_RRQ, &cmd->req_flags))
		copy_info(info, "ISSUE_RRQ, ");

	if (test_bit(BNX2FC_FLAG_ISSUE_ABTS, &cmd->req_flags))
		copy_info(info, "ISSUE_ABTS, ");

	if (test_bit(BNX2FC_FLAG_ABTS_DONE, &cmd->req_flags))
		copy_info(info, "ABTS_DONE, ");

	if (test_bit(BNX2FC_FLAG_TM_COMPL, &cmd->req_flags))
		copy_info(info, "TM_COMPL, ");

	if (test_bit(BNX2FC_FLAG_TM_TIMEOUT, &cmd->req_flags))
		copy_info(info, "TM_TIMEOUT, ");

	if (test_bit(BNX2FC_FLAG_IO_CLEANUP, &cmd->req_flags))
		copy_info(info, "IO_CLEANUP, ");

	if (test_bit(BNX2FC_FLAG_RETIRE_OXID, &cmd->req_flags))
		copy_info(info, "RETIRE_OXID, ");

	if (test_bit(BNX2FC_FLAG_EH_ABORT, &cmd->req_flags))
		copy_info(info, "ABORT, ");

	if (test_bit(BNX2FC_FLAG_IO_COMPL, &cmd->req_flags))
		copy_info(info, "IO_COMPL, ");

	if (test_bit(BNX2FC_FLAG_ELS_DONE, &cmd->req_flags))
		copy_info(info, "ELS_DONE, ");

	if (test_bit(BNX2FC_FLAG_ELS_TIMEOUT, &cmd->req_flags))
		copy_info(info, "ELS_TIMEOUT");

	copy_info(info, "\n");

	copy_info(info, "      cdb_status:0x%d fcp_status:%d fcp_rsp_code:%d"
			" scsi_comp_flags:0x%x\n",
			cmd->cdb_status, cmd->fcp_status, cmd->fcp_rsp_code,
			cmd->scsi_comp_flags);

	if (cmd->cmd_type == BNX2FC_SCSI_CMD)
		bnx2fc_show_scsi_cmd(info, cmd);
}

static void
bnx2fc_show_rport(struct info_str *info, struct bnx2fc_rport *rport)
{
	struct list_head *list;
	struct list_head *tmp;
	struct bnx2fc_cmd *cmd;

	if (!info || !rport)
		return;

	copy_info(info, "   rport(%p) information\n", rport);
	copy_info(info, "     port:%p fc_lport:%p\n",
				rport->port, rport->port->lport);
	copy_info(info, "     fc_rport:%p rdata:%p\n",
				rport->rport, rport->rdata);
	copy_info(info, "     ctx_base:%p conn_id:%d context_id:%d sid:%d\n",
				rport->ctx_base, rport->fcoe_conn_id,
				rport->context_id, rport->sid);
	copy_info(info, "     flags : ");

	if (test_bit(BNX2FC_FLAG_SESSION_READY, &rport->flags))
		copy_info(info, "Session_Ready, ");

	if (test_bit(BNX2FC_FLAG_OFFLOADED, &rport->flags))
		copy_info(info, "Offloaded, ");

	if (test_bit(BNX2FC_FLAG_DISABLED, &rport->flags))
		copy_info(info, "Disabled, ");

	if (test_bit(BNX2FC_FLAG_OFLD_REQ_CMPL, &rport->flags))
		copy_info(info, "Offload_Req_Complete, ");

	if (test_bit(BNX2FC_FLAG_DESTROYED, &rport->flags))
		copy_info(info, "Destroy_Complete, ");

	if (test_bit(BNX2FC_FLAG_CTX_ALLOC_FAILURE, &rport->flags))
		copy_info(info, "Ctx_Alloc_Failure, ");

	if (test_bit(BNX2FC_FLAG_UPLD_REQ_COMPL, &rport->flags))
		copy_info(info, "Upload_Req_Complete, ");

	if (test_bit(BNX2FC_FLAG_EXPL_LOGO, &rport->flags))
		copy_info(info, "Explicit_Logo, ");

	copy_info(info, "\n");

	copy_info(info, "     max_sqes:%d max_rqes:%d max_cqes:%d\n",
			rport->max_sqes, rport->max_rqes, rport->max_cqes);

	copy_info(info, "     SQ_DBELL: prod:ox%x header:0x%x\n",
			rport->sq_db.prod, rport->sq_db.header.header);

	copy_info(info, "     RX_DBELL: header:0x%x params cq_cons\n",
			rport->rx_db.hdr.header, rport->rx_db.params,
			rport->rx_db.doorbell_cq_cons);

	copy_info(info, "     SQ:%p prod:0x%x toggle:0x%x mem_sz:0x%x\n",
			rport->sq_dma, rport->sq_prod_idx,
			rport->sq_curr_toggle_bit, rport->sq_mem_size);

	copy_info(info, "     CQ:%p cons:0x%x toggle:0x%x mem_sz:0x%x\n",
			rport->cq_dma, rport->cq_cons_idx,
			rport->cq_curr_toggle_bit, rport->cq_mem_size);

	copy_info(info, "     RQ:%p prod:0x%x cons:0x%x mem_sz:0x%x\n",
			rport->rq_dma, rport->rq_prod_idx,
			rport->rq_cons_idx, rport->rq_mem_size);

	copy_info(info, "     RQ:%p prod:0x%x cons:0x%x mem_sz:0x%x\n",
			rport->rq_dma, rport->rq_prod_idx,
			rport->rq_cons_idx, rport->rq_mem_size);

	copy_info(info, "     active_ios:%d flush_in_prog:%d \n",
			rport->num_active_ios, rport->flush_in_prog);

	if (!list_empty(&rport->active_cmd_queue)) {
		copy_info(info, "     ACTIVE_QUE(%p) information \n",
				rport->active_cmd_queue);
		spin_lock_bh(&rport->tgt_lock);
		list_for_each_safe(list, tmp, &rport->active_cmd_queue) {
			cmd = (struct bnx2fc_cmd *)list;
			bnx2fc_show_cmd(info, cmd);
		}
		spin_unlock_bh(&rport->tgt_lock);
	} else
		copy_info(info, "     ACTIVE_QUE(%p) empty \n",
				rport->active_cmd_queue);

	if (!list_empty(&rport->els_queue)) {
		copy_info(info, "     ELS_QUE(%p) information \n",
				rport->els_queue);
		spin_lock_bh(&rport->tgt_lock);
		list_for_each_safe(list, tmp, &rport->els_queue) {
			cmd = (struct bnx2fc_cmd *)list;
			bnx2fc_show_cmd(info, cmd);
		}
		spin_unlock_bh(&rport->tgt_lock);
	} else
		copy_info(info, "     ELS_QUE(%p) empty \n",
				rport->els_queue);

	if (!list_empty(&rport->io_retire_queue)) {
		copy_info(info, "     IORETIRE_QUE(%p) information\n",
				rport->io_retire_queue);
		spin_lock_bh(&rport->tgt_lock);
		list_for_each_safe(list, tmp, &rport->io_retire_queue) {
			cmd = (struct bnx2fc_cmd *)list;
			bnx2fc_show_cmd(info, cmd);
		}
		spin_unlock_bh(&rport->tgt_lock);
	} else
		copy_info(info, "     IORETIRE_QUE(%p) empty\n",
				rport->io_retire_queue);

	if (!list_empty(&rport->active_tm_queue)) {
		copy_info(info, "     TMF-Q(%p) information \n",
				rport->active_tm_queue);
		spin_lock_bh(&rport->tgt_lock);
		list_for_each_safe(list, tmp, &rport->active_tm_queue) {
			cmd = (struct bnx2fc_cmd *)list;
			bnx2fc_show_cmd(info, cmd);
		}
		spin_unlock_bh(&rport->tgt_lock);
	} else
		copy_info(info, "     TMF-Q(%p) empty\n",
				rport->active_tm_queue);

}

#ifdef _BNX2FC_EN_STATS_
static void
bnx2fc_show_stats(struct info_str *info, struct bnx2fc_hba *hba)
{
	if (!hba->stats_buffer) {
		copy_info(info, "     NO_STATS\n");
		return;
	}

	copy_info(info, "     TX_STATS: FCoE(pkt_cnt:%d byte_cnt:%d)"
			"FCP(pkt_cnt:%d)\n",
			hba->stats_buffer->tx_stat.fcoe_tx_pkt_cnt,
			hba->stats_buffer->tx_stat.fcoe_tx_byte_cnt,
			hba->stats_buffer->tx_stat.fcp_tx_pkt_cnt);

	copy_info(info, "     RX_STATS0: FCoE(pkt_cnt:%d byte_cnt:%d)\n",
			hba->stats_buffer->rx_stat0.fcoe_rx_pkt_cnt,
			hba->stats_buffer->rx_stat0.fcoe_rx_byte_cnt);

	copy_info(info, "     RX_STATS1: FCoE(ver_cnt:%d drop_cnt:%d)\n",
			hba->stats_buffer->rx_stat1.fcoe_ver_cnt,
			hba->stats_buffer->rx_stat1.fcoe_rx_drop_pkt_cnt);

	copy_info(info, "     RX_STATS2: fc_crc_cnt:%d eofa_del_cnt:%d "
			"miss_frame_cnt:%d\n",
			hba->stats_buffer->rx_stat2.fc_crc_cnt,
			hba->stats_buffer->rx_stat2.eofa_del_cnt,
			hba->stats_buffer->rx_stat2.miss_frame_cnt);

	copy_info(info, "                seq_timeout:%d drop_seq:%d "
			"fcoe_rx_drop_pkt_cnt:%d fcp_rx_pkt_cnt:%d \n ",
			hba->stats_buffer->rx_stat2.seq_timeout_cnt,
			hba->stats_buffer->rx_stat2.drop_seq_cnt,
			hba->stats_buffer->rx_stat2.fcoe_rx_drop_pkt_cnt,
			hba->stats_buffer->rx_stat2.fcp_rx_pkt_cnt);
}
#endif

const char *bnx2fc_fip_state_str[] = {
	"FIP_ST_DISABLED", "FIP_ST_LINK_WAIT", "FIP_ST_AUTO",
	"FIP_ST_NON_FIP", "FIP_ST_ENABLED"
};
static void
bnx2fc_show_fip(struct info_str *info, struct bnx2fc_hba *hba)
{
	struct fcoe_ctlr *fip = &hba->ctlr;

	if (!fip)
		return;

	copy_info(info, "   fip(%p) lp(%p) information\n", fip, fip->lp);
	if (fip->state <  sizeof(bnx2fc_fip_state_str)/sizeof(char *))
		copy_info(info, "     State: %s\n",
			bnx2fc_fip_state_str[fip->state]);
	else
		copy_info(info, "     State:%d: Unknown\n", fip->state);


	if (fip->mode <  sizeof(bnx2fc_fip_state_str)/sizeof(char *))
		copy_info(info, "     lld selected mode: %s\n",
			bnx2fc_fip_state_str[fip->mode]);
	else
		copy_info(info, "     lld selected mode:%d Unknown\n");


	copy_info(info, "     fcf_count:%d sol_time:%d "
			"sel_time:%d port_ka_time:%d \n",
			fip->fcf_count, fip->sol_time, fip->sel_time,
			fip->port_ka_time);
	copy_info(info, "     ctlr_ka_time:%d user_mfs:%d "
			"flogi_oxid:0x%x flogi_count:%d \n",
			fip->ctlr_ka_time, fip->user_mfs, fip->flogi_oxid,
			fip->flogi_count);
	copy_info(info, "     reset_req:%d map_dest:%d "
			"spma:%d send_ctlr_ka:%d send_port_ka:%d \n",
			fip->reset_req, fip->map_dest, fip->spma,
			fip->send_ctlr_ka, fip->send_port_ka);
	copy_info(info, "     dest_addr:%02x:%02x:%02x:%02x:%02x:%02x\n",
			fip->dest_addr[0], fip->dest_addr[1],
			fip->dest_addr[2], fip->dest_addr[3],
			fip->dest_addr[4], fip->dest_addr[5]);
	copy_info(info, "     ctl_src_addr:%02x:%02x:%02x:%02x:%02x:%02x\n",
			fip->ctl_src_addr[0], fip->ctl_src_addr[1],
			fip->ctl_src_addr[2], fip->ctl_src_addr[3],
			fip->ctl_src_addr[4], fip->ctl_src_addr[5]);

	if (test_bit(BNX2FC_ADAPTER_BOOT, &hba->adapter_type))
		copy_info(info, "     vlan_id:%d\n", hba->vlan_id);
	else
		copy_info(info, "     vlan_id:%d\n", fip->vlan_id);
}


static void
bnx2fc_show_hba(struct info_str *info, struct bnx2fc_hba *hba,
	struct fc_lport *lport)
{
	struct bnx2fc_rport *tgt;
	int i;

	copy_info(info, " HBA:%p for netdev:%s\n",
				hba, hba->netdev->name);

	if (bnx2fc_verbose < BNX2FC_VERBOSE_LPORT)
		return;

	copy_info(info, "     Adapter state: ");
	if (test_bit(ADAPTER_STATE_UP, &hba->adapter_state))
		copy_info(info, "UP, ");

	if (test_bit(ADAPTER_STATE_GOING_DOWN, &hba->adapter_state))
		copy_info(info, "GOING_DOWN, ");

	if (test_bit(ADAPTER_STATE_LINK_DOWN, &hba->adapter_state))
		copy_info(info, "LINK_DOWN, ");

	if (test_bit(ADAPTER_STATE_READY, &hba->adapter_state))
		copy_info(info, "READY, ");

	if (test_bit(ADAPTER_STATE_INIT_FAILED, &hba->adapter_state))
		copy_info(info, "INIT_FAILED ");

	if (test_bit(BNX2FC_ADAPTER_BOOT, &hba->adapter_type))
		copy_info(info, "BOOT ADAPTER ");
	if (test_bit(BNX2FC_ADAPTER_FFA, &hba->adapter_type))
		copy_info(info, "FFA ");

	copy_info(info, "\n");

	copy_info(info, "     %s with cnic\n",
			test_bit(BNX2FC_CNIC_REGISTERED, &hba->flags2)
			? "Registered" : "Not registered");

	copy_info(info, "     Flags:0x%x\n", hba->flags);

	copy_info(info, "     init_done: ");

	if (test_bit(BNX2FC_FW_INIT_DONE, &hba->init_done))
		copy_info(info, "FW_INIT_DONE, ");

	if (test_bit(BNX2FC_CTLR_INIT_DONE, &hba->init_done))
		copy_info(info, "CTLR_INIT_DONE, ");

	if (test_bit(BNX2FC_CREATE_DONE, &hba->init_done))
		copy_info(info, "CREATE_DONE, ");

	copy_info(info, "\n");

	if (test_bit(BXN2FC_VLAN_ENABLED, &hba->flags2))
		copy_info(info, "     Vlan (ID=0x%x) Enabled\n", hba->vlan_id);
	else
		copy_info(info, "     Vlan Disabled\n");

	copy_info(info, "     Vlan Filter Stats: invalid grp: %ld, "
			" invalid vlan: %ld\n", 
		  hba->vlan_grp_unset_drop_stat, hba->vlan_invalid_drop_stat);

	copy_info(info, "     IO CMPL - ABORT RACE: %d, %d\n",
		  hba->io_cmpl_abort_race, hba->io_cmpl_abort_race2);

#ifdef _BNX2FC_EN_STATS_
	/* Fix needed. */
	copy_info(info, "     MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n",
			hba->mac_addr[0], hba->mac_addr[1], hba->mac_addr[2],
			hba->mac_addr[3], hba->mac_addr[4], hba->mac_addr[5]);

	bnx2fc_show_stats(info, hba);

	/* Fix needed. */
	copy_info(info, "    PCI: DID:x%x VID:0x%x SDID:0x%x SVID:0x%x"
			"FUNC:0x%x DEVNO:0x%x\n",
			hba->pci_did, hba->pci_vid, hba->pci_sdid,
			hba->pci_svid, hba->pci_func, hba->pci_devno);
#endif

	copy_info(info, "     next_conn_id:0x%x\n", hba->next_conn_id);
	copy_info(info, "     num_offload_sess:%d\n", hba->num_ofld_sess);
	copy_info(info, "     cna_init_queue:%d\n", hba->cna_init_queue);
	copy_info(info, "     cna_clean_queue:%d\n", hba->cna_clean_queue);

	/* fip */
	bnx2fc_show_fip(info, hba);

	/* lport */
	bnx2fc_fcinfo_lport(info, lport);

	/* rport */
	copy_info(info, "   Offloaded ports:\n");

	for (i = 0; i < BNX2FC_NUM_MAX_SESS; i++) {
		tgt = hba->tgt_ofld_list[i];
		if (tgt) {
			if (bnx2fc_verbose >= BNX2FC_VERBOSE_RPORT)
				bnx2fc_show_rport(info, tgt);
			else
				copy_info(info, "   rport:%p cid:%d\n",
					tgt, tgt->fcoe_conn_id);
		}
	}
	copy_info(info, "     cvl_workaround: %ld\n",  hba->ffa_cvl_fix);
	copy_info(info, "     cvl_workaround alloc: %ld\n",  hba->ffa_cvl_fix_alloc);
	copy_info(info, "     cvl_workaround alloc_err: %ld\n",  hba->ffa_cvl_fix_alloc_err);

	copy_info(info, "     Drop D_ID mismatch: %ld\n",  hba->drop_d_id_mismatch);
	copy_info(info, "     Drop FPMA mismatch: %ld\n",  hba->drop_fpma_mismatch);
	copy_info(info, "     Drop vn_port == 0: %ld\n",  hba->drop_vn_port_zero);
	copy_info(info, "     Drop wrong source MAC %ld\n",  hba->drop_wrong_source_mac);
	copy_info(info, "     Drop ABTS response that has both SEQ/EX CTX set: %ld\n",  hba->drop_abts_seq_ex_ctx_set);
}


int bnx2fc_proc_info(struct Scsi_Host *shost, char *buffer, char **start,
    off_t offset, int length, int func)
{
	int retval = -EINVAL;
	struct info_str info;
	struct fc_lport *lport = NULL;
	struct bnx2fc_port *port;
	struct bnx2fc_hba *hba;

	lport = shost_priv(shost);
	port = lport_priv(lport);
	hba = port->hba;

	if (shost == NULL)
		return -EINVAL;

	if (func) {
		bnx2fc_read_proc_data(hba, buffer);
		retval = length;
		goto done;
	}

	if (start)
		*start = buffer;

	info.buffer     = buffer;
	info.length     = length;
	info.offset     = offset;
	info.pos        = 0;

	copy_info(&info, "%s:%s v%s over %s\n", "QLogic Offload FCoE",
			BNX2FC_NAME, BNX2FC_VERSION, hba->netdev->name);

	if (bnx2fc_verbose == BNX2FC_VERBOSE_FC) {
		bnx2fc_show_fcinfo(shost, &info);
		goto retval;
	}

	spin_lock_bh(&hba->hba_lock);
	bnx2fc_show_hba(&info, hba, lport);
	spin_unlock_bh(&hba->hba_lock);

retval:
	copy_info(&info, "\n\0");
	retval = info.pos > info.offset ? info.pos - info.offset : 0;
done:
	return retval;
}

#endif /* defined(__VMKLNX__) */
