/*
 * QLogic iSCSI HBA Driver
 * Copyright (c)  2003-2006 QLogic Corporation
 *
 * See LICENSE.qla4xxx for copyright and licensing details.
 */

#include "ql4_def.h"
#include "ql4_version.h"
#include "ql4_glbl.h"
#include "ql4_dbg.h"
#include "ql4_inline.h"
#include <scsi/iscsi_proto.h>

/**
 * qla4xxx_check_and_copy_sense - copy sense data into cmd sense buffer
 * @ha: Pointer to host adapter structure.
 * @sts_entry: Pointer to status entry structure.
 * @srb: Pointer to srb structure.
 **/
static void qla4xxx_check_and_copy_sense(struct scsi_qla_host *ha,
                               struct status_entry *sts_entry,
                               struct srb *srb)
{
	struct scsi_cmnd *cmd = srb->cmd;
	uint16_t sense_len;

	memset(cmd->sense_buffer, 0, SCSI_SENSE_BUFFERSIZE);
	sense_len = le16_to_cpu(sts_entry->senseDataByteCnt);
	if (sense_len == 0)
		return;

	/* Save total available sense length,
	 * not to exceed cmd's sense buffer size */
	sense_len = min(sense_len, (uint16_t) SCSI_SENSE_BUFFERSIZE);
	srb->req_sense_ptr = cmd->sense_buffer;
	srb->req_sense_len = sense_len;

	/* Copy sense from sts_entry pkt */
	sense_len = min(sense_len, (uint16_t) IOCB_MAX_SENSEDATA_LEN);
	memcpy(cmd->sense_buffer, sts_entry->senseData, sense_len);

	DEBUG2(printk("scsi%ld:%d:%d:%d: %s: sense key = %x, "
		"ASL = %02x, ASC/ASCQ = %02x/%02x\n", ha->host_no,
		cmd->device->channel, cmd->device->id,
		cmd->device->lun, __func__,
		sts_entry->senseData[2] & 0x0f,
		sts_entry->senseData[7],
		sts_entry->senseData[12],
		sts_entry->senseData[13]));

	DEBUG5(qla4xxx_dump_buffer(cmd->sense_buffer, sense_len));
	srb->flags |= SRB_GOT_SENSE;

	/* Update srb, in case a sts_cont pkt follows */
	srb->req_sense_ptr += sense_len;
	srb->req_sense_len -= sense_len;
	if (srb->req_sense_len != 0)
		ha->status_srb = srb;
	else
		ha->status_srb = NULL;

	if ((srb->flags & SRB_SCSI_PASSTHRU))
		return;

	/* check for vaild sense data */
	if ((sts_entry->senseData[0] & 0x70) != 0x70)
		return;

	switch (sts_entry->senseData[2] & 0x0f) {
	case UNIT_ATTENTION:
		if (sts_entry->senseData[12] == 0x3F &&
		    sts_entry->senseData[13] == 0x0E) {
			struct ddb_entry *ddb_entry;

			ddb_entry = qla4xxx_lookup_ddb_by_os_index(ha,
				cmd->device->id);
			if (ddb_entry) {
				dev_info(&ha->pdev->dev,"%s: ddb[%d] os[%d] "
					 "schedule dynamic lun scan\n",
                                         __func__, ddb_entry->fw_ddb_index,
					 ddb_entry->os_target_id);

				set_bit(DF_DYNAMIC_LUN_SCAN_NEEDED,
					&ddb_entry->flags);
				set_bit(DPC_DYNAMIC_LUN_SCAN, &ha->dpc_flags);
			}
		}
		break;
	}
}

/**
 * qla4xxx_status_cont_entry() - Process a Status Continuations entry.
 * @ha: SCSI driver HA context
 * @sts_cont: Entry pointer
 *
 * Extended sense data.
 */
static void
qla4xxx_status_cont_entry(struct scsi_qla_host *ha,
			  struct status_cont_entry *sts_cont)
{
	struct srb *srb = ha->status_srb;
	struct scsi_cmnd *cmd;
	uint16_t sense_len;

	if (srb == NULL)
		return;

	cmd = srb->cmd;
	if (cmd == NULL) {
		DEBUG2(printk("scsi%ld: %s: Cmd already returned back to OS "
			"srb=%p srb->state:%d\n", ha->host_no, __func__, srb, srb->state));
		ha->status_srb = NULL;
		return;
	}

	/* Copy sense data. */
	sense_len = min(srb->req_sense_len, (uint16_t) IOCB_MAX_EXT_SENSEDATA_LEN);
	memcpy(srb->req_sense_ptr, sts_cont->extSenseData, sense_len);
	DEBUG5(qla4xxx_dump_buffer(srb->req_sense_ptr, sense_len));

	srb->req_sense_ptr += sense_len;
	srb->req_sense_len -= sense_len;

	/* Place command on done queue. */
	if (srb->req_sense_len == 0) {
		sp_put(ha, srb);
		ha->status_srb = NULL;
	}
}

/**
 * qla4xxx_status_entry - processes status IOCBs
 * @ha: Pointer to host adapter structure.
 * @sts_entry: Pointer to status entry structure.
 **/
static void qla4xxx_status_entry(struct scsi_qla_host *ha,
				 struct status_entry *sts_entry)
{
	uint8_t scsi_status;
	struct scsi_cmnd *cmd;
	struct srb *srb;
	struct ddb_entry *ddb_entry;
	uint32_t residual;

	srb = qla4xxx_del_from_active_array(ha, le32_to_cpu(sts_entry->handle));
	if (!srb) {
		dev_warn(&ha->pdev->dev, "%s invalid status entry:"
			" handle=0x%0x\n", __func__, sts_entry->handle);
		set_bit(DPC_RESET_HA, &ha->dpc_flags);
		return;
	}

	cmd = srb->cmd;
	if (cmd == NULL) {
		dev_warn(&ha->pdev->dev, "%s Command is NULL: srb=%p"
			" sts_handle=0x%0x srb_state=0x%0x\n", __func__,
			srb, sts_entry->handle, srb->state);
		return;
	}

	ddb_entry = srb->ddb;
	if (ddb_entry == NULL) {
		cmd->result = DID_NO_CONNECT << 16;
		goto status_entry_exit;
	}

	residual = le32_to_cpu(sts_entry->residualByteCnt);

	/* Translate ISP error to a Linux SCSI error. */
	scsi_status = sts_entry->scsiStatus;
	switch (sts_entry->completionStatus) {
	case SCS_COMPLETE:
		if (sts_entry->iscsiFlags & ISCSI_FLAG_RESIDUAL_OVER) {
			cmd->result = DID_ERROR << 16;
			break;
		}
		if (sts_entry->iscsiFlags & ISCSI_FLAG_RESIDUAL_UNDER) {
			cmd->resid = residual;
			if (!scsi_status && ((cmd->request_bufflen - residual) <
				   cmd->underflow)) {
				cmd->result = DID_ERROR << 16;
				break;
			}
		}

		cmd->result = DID_OK << 16 | scsi_status;

		if (scsi_status != SCSI_CHECK_CONDITION)
			break;

		qla4xxx_check_and_copy_sense(ha, sts_entry, srb);
		break;

	case SCS_INCOMPLETE:
		/* Always set the status to DID_ERROR, since
		 * all conditions result in that status anyway */
		cmd->result = DID_ERROR << 16;
		break;

	case SCS_RESET_OCCURRED:
		DEBUG2(printk("scsi%ld:%d:%d:%d: %s: Device RESET occurred\n",
			      ha->host_no, cmd->device->channel,
			      cmd->device->id, cmd->device->lun, __func__));

		cmd->result = DID_RESET << 16;
		break;

	case SCS_ABORTED:
		DEBUG2(printk("scsi%ld:%d:%d:%d: %s: Abort occurred\n",
			      ha->host_no, cmd->device->channel,
			      cmd->device->id, cmd->device->lun, __func__));

		cmd->result = DID_RESET << 16;
		break;

	case SCS_TIMEOUT:
		DEBUG2(printk(KERN_INFO "scsi%ld:%d:%d:%d: Timeout\n",
			      ha->host_no, cmd->device->channel,
			      cmd->device->id, cmd->device->lun));

		cmd->result = DID_BUS_BUSY << 16;

		/*
		 * Mark device missing so that we won't continue to send
		 * I/O to this device.	We should get a ddb state change
		 * AEN soon.
		 */
		if (atomic_read(&ddb_entry->state) == DDB_STATE_ONLINE)
			qla4xxx_mark_device_missing(ha, ddb_entry);
		break;

	case SCS_DATA_UNDERRUN:
	case SCS_DATA_OVERRUN:
		if ((sts_entry->iscsiFlags & ISCSI_FLAG_RESIDUAL_OVER) ||
			(sts_entry->completionStatus == SCS_DATA_OVERRUN)) {
			DEBUG2(printk("scsi%ld:%d:%d:%d: %s: "
				      "Data overrun\n", ha->host_no,
				      cmd->device->channel, cmd->device->id,
				      cmd->device->lun, __func__));

			cmd->result = DID_ERROR << 16;
			break;
		}

		cmd->resid = residual;

		/*
		 * If there is scsi_status, it takes precedense over
		 * underflow condition.
		 */
		if (scsi_status != 0) {
			cmd->result = DID_OK << 16 | scsi_status;

			if (scsi_status != SCSI_CHECK_CONDITION)
				break;

			qla4xxx_check_and_copy_sense(ha, sts_entry, srb);
		} else {
			/*
			 * If RISC reports underrun and target does not
			 * report it then we must have a lost frame, so
			 * tell upper layer to retry it by reporting a
			 * bus busy.
			 */
			if ((sts_entry->iscsiFlags &
			     ISCSI_FLAG_RESIDUAL_UNDER) == 0) {
				cmd->result = DID_BUS_BUSY << 16;
			} else if ((cmd->request_bufflen - residual) <
				   cmd->underflow) {
				/*
				 * Handle mid-layer underflow???
				 *
				 * For kernels less than 2.4, the driver must
				 * return an error if an underflow is detected.
				 * For kernels equal-to and above 2.4, the
				 * mid-layer will appearantly handle the
				 * underflow by detecting the residual count --
				 * unfortunately, we do not see where this is
				 * actually being done.	 In the interim, we
				 * will return DID_ERROR.
				 */
				DEBUG2(printk("scsi%ld:%d:%d:%d: %s: "
 					"Mid-layer Data underrun len = 0x%x, "
					"resid = 0x%x, compstat = 0x%x\n",
					ha->host_no, cmd->device->channel,
					cmd->device->id, cmd->device->lun,
					__func__, cmd->request_bufflen,
					residual,
					sts_entry->completionStatus));

				cmd->result = DID_ERROR << 16;
			} else {
				cmd->result = DID_OK << 16;
			}
		}
		break;

	case SCS_DEVICE_LOGGED_OUT:
	case SCS_DEVICE_UNAVAILABLE:
		/*
		 * Mark device missing so that we won't continue to
		 * send I/O to this device.  We should get a ddb
		 * state change AEN soon.
		 */
		DEBUG2(printk(KERN_INFO "scsi%ld:%d:%d:%d: DEVICE_UNAVAILABLE "
			      "or DEVICE_LOGGED_OUT\n",
			      ha->host_no, cmd->device->channel,
			      cmd->device->id, cmd->device->lun));

		if (atomic_read(&ddb_entry->state) == DDB_STATE_ONLINE)
			qla4xxx_mark_device_missing(ha, ddb_entry);

		cmd->result = DID_BUS_BUSY << 16;
		break;

	case SCS_QUEUE_FULL:
		/*
		 * SCSI Mid-Layer handles device queue full
		 */
		cmd->result = DID_OK << 16 | sts_entry->scsiStatus;
		DEBUG2(printk("scsi%ld:%d:%d: %s: QUEUE FULL detected "
			      "compl=%02x, scsi=%02x, state=%02x, iFlags=%02x,"
			      " iResp=%02x\n", ha->host_no, cmd->device->id,
			      cmd->device->lun, __func__,
			      sts_entry->completionStatus,
			      sts_entry->scsiStatus, sts_entry->state_flags,
			      sts_entry->iscsiFlags,
			      sts_entry->iscsiResponse));
		break;

	default:
		cmd->result = DID_ERROR << 16;
		break;
	}

status_entry_exit:

	/* complete the request, if not waiting for status_continuation pkt */
	srb->cc_stat = sts_entry->completionStatus;
	if (ha->status_srb == NULL)
		sp_put(ha, srb);
}

/**
 * qla4xxx_process_response_queue - process response queue completions
 * @ha: Pointer to host adapter structure.
 *
 * This routine process response queue completions in interrupt context.
 * Hardware_lock locked upon entry
 **/
static void qla4xxx_process_response_queue(struct scsi_qla_host * ha)
{
	uint32_t count = 0;
	struct srb *srb = NULL;
	struct status_entry *sts_entry;
#ifndef __VMKLNX__
	struct async_pdu_iocb *apdu;
	struct iscsi_hdr *pdu_hdr;
	struct async_msg_pdu_iocb *apdu_iocb;
#endif /* __VMKLNX__ */

	/* Process all responses from response queue */
	while ((ha->response_in =
		(uint16_t)le32_to_cpu(ha->shadow_regs->rsp_q_in)) !=
	       ha->response_out) {
		sts_entry = (struct status_entry *) ha->response_ptr;
		count++;

		/* Advance pointers for next entry */
		if (ha->response_out == (RESPONSE_QUEUE_DEPTH - 1)) {
			ha->response_out = 0;
			ha->response_ptr = ha->response_ring;
		} else {
			ha->response_out++;
			ha->response_ptr++;
		}

		/* process entry */
		switch (sts_entry->hdr.entryType) {
		case ET_STATUS:
			qla4xxx_status_entry(ha, sts_entry);
			break;

		case ET_PASSTHRU_STATUS:
			break;
 
#ifdef __VMKLNX__
		case ET_ASYNC_PDU:
			/* Just throw away the async entries */
			DEBUG2(printk("scsi%ld: %s: Async entry - "
				      "ignoring\n", ha->host_no, __func__));
			break;
#else /*  NOT __VMKLNX__ */
		case ET_ASYNC_PDU:
			apdu = (struct async_pdu_iocb *)sts_entry;
			if (apdu->status != ASYNC_PDU_IOCB_STS_OK)
				break;

			pdu_hdr = (struct iscsi_hdr *)apdu->iscsi_pdu_hdr;
			if (pdu_hdr->hlength || pdu_hdr->dlength[0] ||
			    pdu_hdr->dlength[1] || pdu_hdr->dlength[2]) {
				apdu_iocb =
                                    kmalloc(sizeof(struct async_msg_pdu_iocb),
                                            GFP_ATOMIC);
				if (apdu_iocb) {
					memcpy(apdu_iocb->iocb, apdu,
					    sizeof(struct async_pdu_iocb));
					list_add_tail(&apdu_iocb->list,
                                                      &ha->async_iocb_list);
					DEBUG2(printk("scsi%ld:"
						"%s: schedule async msg pdu\n",
						ha->host_no, __func__));
					set_bit(DPC_ASYNC_PDU, &ha->dpc_flags);
				} else {
					DEBUG2(printk("scsi%ld:"
					    "%s: unable to alloc ASYNC PDU\n",
					    ha->host_no, __func__));
				}
			}
			break;
#endif /* __VMKLNX __ */

		case ET_STATUS_CONTINUATION:
			qla4xxx_status_cont_entry(ha,
				(struct status_cont_entry *) sts_entry);
			break;

		case ET_COMMAND:
			/* ISP device queue is full. Command not
			 * accepted by ISP.  Queue command for
			 * later */

			srb = qla4xxx_del_from_active_array(ha,
						    le32_to_cpu(sts_entry->
								handle));
			if (srb == NULL)
				goto exit_prq_invalid_handle;

			DEBUG2(printk("scsi%ld: %s: FW device queue full, "
				      "srb %p\n", ha->host_no, __func__, srb));

			/* ETRY normally by sending it back with
			 * DID_BUS_BUSY */
			srb->cmd->result = DID_BUS_BUSY << 16;
			sp_put(ha, srb);
			break;

		case ET_CONTINUE:
			/* Just throw away the continuation entries */
			DEBUG2(printk("scsi%ld: %s: Continuation entry - "
				      "ignoring\n", ha->host_no, __func__));
			break;

		default:
			/*
			 * Invalid entry in response queue, reset RISC
			 * firmware.
			 */
			DEBUG2(printk("scsi%ld: %s: Invalid entry %x in "
				      "response queue \n", ha->host_no,
				      __func__,
				      sts_entry->hdr.entryType));
			break;
		}
	}

	/*
	 * Done with responses, update the ISP For QLA4010, this also clears
	 * the interrupt.
	 */
	writel(ha->response_out, &ha->reg->rsp_q_out);
	readl(&ha->reg->rsp_q_out);

	return;

exit_prq_invalid_handle:
	printk(KERN_WARNING "scsi%ld: %s: Invalid handle(srb)=%p"
                      " type=%x IOCS=%x\n",
		      ha->host_no, __func__, srb, sts_entry->hdr.entryType,
		      sts_entry->completionStatus);
	/*
	 * Done with responses, update the ISP For QLA40xx, this also clears
	 * the interrupt.
	 */
	writel(ha->response_out, &ha->reg->rsp_q_out);
	readl(&ha->reg->rsp_q_out);
}

/**
 * qla4xxx_isr_decode_mailbox - decodes mailbox status
 * @ha: Pointer to host adapter structure.
 * @mailbox_status: Mailbox status.
 *
 * This routine decodes the mailbox status during the ISR.
 * Hardware_lock locked upon entry. runs in interrupt context.
 **/
static void qla4xxx_isr_decode_mailbox(struct scsi_qla_host * ha,
				       uint32_t mbox_status)
{
	int i;
	uint32_t mbox_stat2, mbox_stat3;

	if ((mbox_status == MBOX_STS_BUSY) ||
	    (mbox_status == MBOX_STS_INTERMEDIATE_COMPLETION) ||
	    (mbox_status >> 12 == MBOX_COMPLETION_STATUS)) {
		ha->mbox_status[0] = mbox_status;

		if (test_bit(AF_MBOX_COMMAND, &ha->flags)) {
			/*
			 * Copy all mailbox registers to a temporary
			 * location and set mailbox command done flag
			 */
			for (i = 1; i < ha->mbox_status_count; i++)
				ha->mbox_status[i] =
					readl(&ha->reg->mailbox[i]);

			set_bit(AF_MBOX_COMMAND_DONE, &ha->flags);
		}
	} else if (mbox_status >> 12 == MBOX_ASYNC_EVENT_STATUS) {

		/* Queue all AENs into internal AEN database.  The driver will
		 * report AEN information to Application layer when requested.*/
		qla4xxx_queue_aen_log(ha, &mbox_status);

		/* NOTE: Non-DDB_CHANGED AENs are processed immediately,
		 * and DDB_CHANGED AENs are processed in the DPC */
		switch (mbox_status) {
		case MBOX_ASTS_SYSTEM_ERROR:
			/* Log Mailbox registers */
#ifdef __VMKLNX__
			/*
			 * Unrecoverable internal firmware errors.  These should
			 * never occur and based on the ISP40XX spec section 10.3
			 * we need the mbx 0-7 to even report them to qlogic.  The
			 * driver will attempt to recover the firmware and continue
			 * IO.
			 */
			dev_info(&ha->pdev->dev,
				"scsi%ld: %s: adapter system error "
				"(0x%08x:0x%08x:0x%08x:0x%08x:0x%08x:0x%08x:0x%08x:0x%08x)\n",
				ha->host_no, __func__,
				readl(&ha->reg->mailbox[0]), readl(&ha->reg->mailbox[1]),
				readl(&ha->reg->mailbox[2]), readl(&ha->reg->mailbox[3]),
				readl(&ha->reg->mailbox[4]), readl(&ha->reg->mailbox[5]),
				readl(&ha->reg->mailbox[6]), readl(&ha->reg->mailbox[7]));
#endif

			if (ql4xdontresethba) {
				DEBUG2(printk("%s:Dont Reset HBA\n",
					      __func__));
			} else {
				set_bit(AF_GET_CRASH_RECORD, &ha->flags);
				set_bit(DPC_RESET_HA, &ha->dpc_flags);
			}
			break;

		case MBOX_ASTS_REQUEST_TRANSFER_ERROR:
		case MBOX_ASTS_RESPONSE_TRANSFER_ERROR:
		case MBOX_ASTS_NVRAM_INVALID:
		case MBOX_ASTS_IP_ADDRESS_CHANGED:
		case MBOX_ASTS_DHCP_LEASE_EXPIRED:
			DEBUG2(printk("scsi%ld: AEN %04x, ERROR Status, "
				      "Reset HA\n", ha->host_no, mbox_status));
			set_bit(DPC_RESET_HA, &ha->dpc_flags);
			break;

		case MBOX_ASTS_LINK_UP:
			set_bit(AF_LINK_UP, &ha->flags);
#ifdef __VMKLNX__
			if test_bit(AF_INIT_DONE, &ha->flags)
				set_bit(DPC_LINK_CHANGED, &ha->dpc_flags);
			dev_info(&ha->pdev->dev,
				"scsi%ld: %s: adapter LINK UP\n",
				ha->host_no, __func__);
#endif
			break;

		case MBOX_ASTS_LINK_DOWN:
			clear_bit(AF_LINK_UP, &ha->flags);
#ifdef __VMKLNX__
			set_bit(DPC_LINK_CHANGED, &ha->dpc_flags);
			dev_info(&ha->pdev->dev,
				"scsi%ld: %s: adapter LINK DOWN\n",
				ha->host_no, __func__);
#endif
			break;

		case MBOX_ASTS_HEARTBEAT:
			ha->seconds_since_last_heartbeat = 0;
			break;

		case MBOX_ASTS_DHCP_LEASE_ACQUIRED:
			DEBUG2(printk("scsi%ld: AEN %04x DHCP LEASE "
				      "ACQUIRED\n", ha->host_no, mbox_status));
			set_bit(DPC_GET_DHCP_IP_ADDR, &ha->dpc_flags);
			break;

		case MBOX_ASTS_PROTOCOL_STATISTIC_ALARM:
		case MBOX_ASTS_SCSI_COMMAND_PDU_REJECTED: /* Target
							   * mode
							   * only */
		case MBOX_ASTS_UNSOLICITED_PDU_RECEIVED:  /* Connection mode */
		case MBOX_ASTS_IPSEC_SYSTEM_FATAL_ERROR:
		case MBOX_ASTS_SUBNET_STATE_CHANGE:
			/* No action */
			DEBUG2(printk("scsi%ld: AEN %04x\n", ha->host_no,
				      mbox_status));
			break;

		case MBOX_ASTS_IP_ADDR_STATE_CHANGED:
			mbox_stat2 = readl(&ha->reg->mailbox[2]);
			mbox_stat3 = readl(&ha->reg->mailbox[3]);

			if ((mbox_stat3 == 5) && (mbox_stat2 == 3)) 
				set_bit(DPC_GET_DHCP_IP_ADDR, &ha->dpc_flags);
			else if ((mbox_stat3 == 2) && (mbox_stat2 == 5)) 
				set_bit(DPC_RESET_HA, &ha->dpc_flags);
			break;

		case MBOX_ASTS_MAC_ADDRESS_CHANGED:
		case MBOX_ASTS_DNS:
			/* No action */
			DEBUG2(printk(KERN_INFO "scsi%ld: AEN %04x, "
				      "mbox_sts[1]=%04x, mbox_sts[2]=%04x\n",
				      ha->host_no, mbox_status,
				      readl(&ha->reg->mailbox[1]),
				      readl(&ha->reg->mailbox[2])));
			break;

		case MBOX_ASTS_SELF_TEST_FAILED:
		case MBOX_ASTS_LOGIN_FAILED:
			/* No action */
			DEBUG2(printk("scsi%ld: AEN %04x, mbox_sts[1]=%04x, "
				      "mbox_sts[2]=%04x, mbox_sts[3]=%04x\n",
				      ha->host_no, mbox_status,
				      readl(&ha->reg->mailbox[1]),
				      readl(&ha->reg->mailbox[2]),
				      readl(&ha->reg->mailbox[3])));
			break;

		case MBOX_ASTS_DATABASE_CHANGED:
			/* Queue AEN information and process it in the DPC
			 * routine */
			if (ha->aen_q_count > 0) {

				/* decrement available counter */
				ha->aen_q_count--;

				for (i = 1; i < MBOX_AEN_REG_COUNT; i++)
					ha->aen_q[ha->aen_in].mbox_sts[i] =
						readl(&ha->reg->mailbox[i]);

				ha->aen_q[ha->aen_in].mbox_sts[0] = mbox_status;

				/* print debug message */
				DEBUG2(printk("scsi%ld: AEN[%d] %04x queued"
					      " mb1:0x%x mb2:0x%x mb3:0x%x mb4:0x%x\n",
					      ha->host_no, ha->aen_in,
					      mbox_status,
					      ha->aen_q[ha->aen_in].mbox_sts[1],
					      ha->aen_q[ha->aen_in].mbox_sts[2],
					      ha->aen_q[ha->aen_in].mbox_sts[3],
					      ha->aen_q[ha->aen_in].  mbox_sts[4]));
				/* advance pointer */
				ha->aen_in++;
				if (ha->aen_in == MAX_AEN_ENTRIES)
					ha->aen_in = 0;

				/* The DPC routine will process the aen */
				set_bit(DPC_AEN, &ha->dpc_flags);
			} else {
				DEBUG2(printk("scsi%ld: %s: aen %04x, queue "
					      "overflowed!  AEN LOST!!\n",
					      ha->host_no, __func__,
					      mbox_status));

				DEBUG2(printk("scsi%ld: DUMP AEN QUEUE\n",
					      ha->host_no));

				for (i = 0; i < MAX_AEN_ENTRIES; i++) {
					DEBUG2(printk("AEN[%d] %04x %04x %04x "
						      "%04x\n", i,
						      ha->aen_q[i].mbox_sts[0],
						      ha->aen_q[i].mbox_sts[1],
						      ha->aen_q[i].mbox_sts[2],
						      ha->aen_q[i].mbox_sts[3]));
				}
			}
			break;

		default:
			DEBUG2(printk(KERN_WARNING
				      "scsi%ld: AEN %04x UNKNOWN\n",
				      ha->host_no, mbox_status));
			break;
		}
	} else {
		DEBUG2(printk("scsi%ld: Unknown mailbox status %08X\n",
			      ha->host_no, mbox_status));

		ha->mbox_status[0] = mbox_status;
	}
}

/**
 * qla4xxx_interrupt_service_routine - isr
 * @ha: pointer to host adapter structure.
 *
 * This is the main interrupt service routine.	
 * hardware_lock locked upon entry. runs in interrupt context.
 **/
void qla4xxx_interrupt_service_routine(struct scsi_qla_host * ha,
				       uint32_t intr_status)
{
	/* Process response queue interrupt. */
	if (intr_status & CSR_SCSI_COMPLETION_INTR)
		qla4xxx_process_response_queue(ha);

	/* Process mailbox/asynch event	 interrupt.*/
	if (intr_status & CSR_SCSI_PROCESSOR_INTR) {
		qla4xxx_isr_decode_mailbox(ha,
					   readl(&ha->reg->mailbox[0]));

		/* Clear Mailbox Interrupt */
		writel(set_rmask(CSR_SCSI_PROCESSOR_INTR),
		       &ha->reg->ctrl_status);
		readl(&ha->reg->ctrl_status);
	}
}

/**
 * qla4xxx_intr_handler - hardware interrupt handler.
 * @irq: Unused
 * @dev_id: Pointer to host adapter structure
 * @regs: Unused
 **/
#if defined(__VMKLNX__)
irqreturn_t qla4xxx_intr_handler(int irq, void *dev_id)
#else /* !defined(__VMKLNX__) */
irqreturn_t qla4xxx_intr_handler(int irq, void *dev_id, struct pt_regs *regs)
#endif /* defined(__VMKLNX__) */
{
	struct scsi_qla_host *ha;
	uint32_t intr_status;
	unsigned long flags = 0;
	uint8_t reqs_count = 0;

	ha = (struct scsi_qla_host *) dev_id;
	if (!ha) {
		DEBUG2(printk(KERN_INFO
			      "qla4xxx: Interrupt with NULL host ptr\n"));
		return IRQ_NONE;
	}

	spin_lock_irqsave(&ha->hardware_lock, flags);

	DEBUG2(ha->seconds_since_last_intr = 0);
	ha->isr_count++;
	/*
	 * Repeatedly service interrupts up to a maximum of
	 * MAX_REQS_SERVICED_PER_INTR
	 */
	while (1) {
		/*
		 * Read interrupt status
		 */
		if (le32_to_cpu(ha->shadow_regs->rsp_q_in) !=
		    ha->response_out)
			intr_status = CSR_SCSI_COMPLETION_INTR;
		else
			intr_status = readl(&ha->reg->ctrl_status);

		if ((intr_status &
		     (CSR_SCSI_RESET_INTR|CSR_FATAL_ERROR|INTR_PENDING)) ==
		    0) {
			if (reqs_count == 0)
				ha->spurious_int_count++;
			break;
		}

		if (intr_status & CSR_FATAL_ERROR) {
			DEBUG2(printk(KERN_INFO "scsi%ld: Fatal Error, "
				      "Status 0x%04x\n", ha->host_no,
				      readl(isp_port_error_status (ha))));

			/* Issue Soft Reset to clear this error condition.
			 * This will prevent the RISC from repeatedly
			 * interrupting the driver; thus, allowing the DPC to
			 * get scheduled to continue error recovery.
			 * NOTE: Disabling RISC interrupts does not work in
			 * this case, as CSR_FATAL_ERROR overrides
			 * CSR_SCSI_INTR_ENABLE */
			if ((readl(&ha->reg->ctrl_status) &
			     CSR_SCSI_RESET_INTR) == 0) {
				writel(set_rmask(CSR_SOFT_RESET),
				       &ha->reg->ctrl_status);
				readl(&ha->reg->ctrl_status);
			}

			writel(set_rmask(CSR_FATAL_ERROR),
			       &ha->reg->ctrl_status);
			readl(&ha->reg->ctrl_status);

			__qla4xxx_disable_intrs(ha);

			set_bit(DPC_RESET_HA, &ha->dpc_flags);

			break;
		} else if (intr_status & CSR_SCSI_RESET_INTR) {
			clear_bit(AF_ONLINE, &ha->flags);
#ifdef __VMKLNX__
                        dev_info(&ha->pdev->dev,
                           "scsi%ld: %s: adapter OFFLINE\n",
                           ha->host_no, __func__);
#endif

			__qla4xxx_disable_intrs(ha);

			writel(set_rmask(CSR_SCSI_RESET_INTR),
			       &ha->reg->ctrl_status);
			readl(&ha->reg->ctrl_status);

			if (!ql4_mod_unload)
				set_bit(DPC_RESET_HA_INTR, &ha->dpc_flags);

			break;
		} else if (intr_status & INTR_PENDING) {
			qla4xxx_interrupt_service_routine(ha, intr_status);
			ha->total_io_count++;
			if (++reqs_count == MAX_REQS_SERVICED_PER_INTR)
				break;

			intr_status = 0;
		}
	}

	spin_unlock_irqrestore(&ha->hardware_lock, flags);

	return IRQ_HANDLED;
}

/**
 * qla4xxx_process_aen - processes AENs generated by firmware
 * @ha: pointer to host adapter structure.
 * @process_aen: type of AENs to process
 *
 * Processes specific types of Asynchronous Events generated by firmware. 
 * The type of AENs to process is specified by process_aen and can be
 *	PROCESS_ALL_AENS	 0
 *	FLUSH_DDB_CHANGED_AENS	 1
 *	RELOGIN_DDB_CHANGED_AENS 2
 *  	PROCESS_FOR_PROBE 3
 *
 * Only DDB Changed AENs are processed here.  All other AENs are processed
 * in the interrupt context as soon as they arrive.
 **/
void qla4xxx_process_aen(struct scsi_qla_host * ha, uint8_t process_aen)
{
	uint32_t mbox_sts[MBOX_AEN_REG_COUNT];
	struct aen *aen;
	int i;
	unsigned long flags;

	DEBUG6(dev_info(&ha->pdev->dev, "%s proc_aen 0x%x\n",
		__func__, process_aen));

	spin_lock_irqsave(&ha->hardware_lock, flags);
	if (process_aen == FLUSH_DDB_CHANGED_AENS) {
		ha->aen_q_count = MAX_AEN_ENTRIES;
		ha->aen_out = ha->aen_in = 0;
		spin_unlock_irqrestore(&ha->hardware_lock, flags);
		return;
	}

	while (ha->aen_out != ha->aen_in) {
		aen = &ha->aen_q[ha->aen_out];
		/* copy aen information to local structure */
		for (i = 0; i < MBOX_AEN_REG_COUNT; i++)
			mbox_sts[i] = aen->mbox_sts[i];

		ha->aen_q_count++;
		ha->aen_out++;

		if (ha->aen_out == MAX_AEN_ENTRIES)
			ha->aen_out = 0;

		DEBUG6(dev_info(&ha->pdev->dev, "%s mbx0 0x%x mbx1 0x%x mbx2 "
			"0x%x mbx3 0x%x ddb 0x%p\n", __func__, mbox_sts[0],
			mbox_sts[1], mbox_sts[2], mbox_sts[3],
			qla4xxx_lookup_ddb_by_fw_index(ha, mbox_sts[2])));

		spin_unlock_irqrestore(&ha->hardware_lock, flags);

		if (process_aen == RELOGIN_DDB_CHANGED_AENS) {
			/* for use during init time, we only want to
			 * relogin non-active ddbs */
			struct ddb_entry *ddb_entry;

			ddb_entry = qla4xxx_lookup_ddb_by_fw_index(ha, mbox_sts[2]);

			if (ddb_entry) {

				DEBUG6(dev_info(&ha->pdev->dev, "%s ddb 0x%p "
					"sess 0x%p conn 0x%p state 0x%x\n",
					__func__, ddb_entry, ddb_entry->sess,
					ddb_entry->conn, ddb_entry->state));

				ddb_entry->dev_scan_wait_to_complete_relogin = 0;
				ddb_entry->dev_scan_wait_to_start_relogin =
					jiffies +
					((ddb_entry->default_time2wait + 4) * HZ);

				DEBUG2(printk("scsi%ld: ddb index [%d] initate"
				      " RELOGIN after %d seconds\n", ha->host_no,
					ddb_entry->fw_ddb_index,
					ddb_entry->default_time2wait + 4));
			}
		} else if (mbox_sts[0] == MBOX_ASTS_DATABASE_CHANGED) {
			if (mbox_sts[1] == 0) {	/* Global DB change. */
				qla4xxx_reinitialize_ddb_list(ha);
			} else if (mbox_sts[1] == 1) {	/* Specific device. */
				qla4xxx_process_ddb_changed(ha, mbox_sts[2],
					mbox_sts[3], mbox_sts[4],
					((process_aen == PROCESS_FOR_PROBE) ? 1 : 0 ));
			}
		}
		spin_lock_irqsave(&ha->hardware_lock, flags);
	}
	spin_unlock_irqrestore(&ha->hardware_lock, flags);
}

