/*
 * Portions Copyright 2008, 2010 VMware, Inc.
 */
/*
 * Adaptec ADP94xx SAS HBA device driver for Linux.
 *
 * Copyright (c) 2004 Adaptec Inc.
 * All rights reserved.
 *
 * Written by : Robert Tarte  <robt@PacificCodeWorks.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 *
 * $Id: //depot/razor/linux/src/adp94xx_sata.c#44 $
 * 
 */	
#include "adp94xx_osm.h"
#include "adp94xx_inline.h"
#include "adp94xx_sata.h"
#include "adp94xx_discover.h"

#if KDB_ENABLE
#include "linux/kdb.h"
#endif

// RST:
// - What if the device doesn't support LBA???
// - What if the number of outstanding (tagged) requests exceeds the
//   number of Empty SCBs
// - figure out tagged queueing modes
// - overlap == disconnect???

static void	asd_sata_linux_scb_done(struct asd_softc *asd, struct scb *scb,
			struct asd_done_list *dl);

#if defined(__VMKLNX__)
/*
 * The function is used to fake good RESERVE/RELEASE command
 * responses.
 */
int
asd_sata_set_good_condition(
struct scsi_cmnd        *cmd
)
{
        memset(cmd->sense_buffer, 0, sizeof(cmd->sense_buffer));

        asd_cmd_set_driver_status(cmd, NO_SENSE);
        asd_cmd_set_host_status(cmd, DID_OK);
        asd_cmd_set_scsi_status(cmd, GOOD);

        return ASD_COMMAND_BUILD_FINISHED;
}
#endif /* #if defined(__VMKLNX__) */

ASD_COMMAND_BUILD_STATUS
asd_build_ata_scb(
struct asd_softc	*asd,
struct scb		*scb,
union asd_cmd		*acmd
)
{
	ASD_COMMAND_BUILD_STATUS	ret;
	struct asd_ata_task_hscb	*ata_hscb;
	struct asd_target		*target;
	struct scsi_cmnd		*cmd;
	struct asd_device		*dev;

	dev = scb->platform_data->dev;

	cmd =  &acmd->scsi_cmd;

	/*
	 * Guard against stale sense data.  The Linux mid-layer assumes that
	 * sense was retrieved anytime the first byte of the sense buffer
	 * looks "sane".
	 */
	cmd->sense_buffer[0] = 0;
	cmd->resid = 0;

	ata_hscb = &scb->hscb->ata_task;

	target = dev->target;

	ata_hscb->header.opcode = SCB_INITIATE_ATA_TASK;
	ata_hscb->protocol_conn_rate =
		PROTOCOL_TYPE_SATA | target->ddb_profile.conn_rate;
	ata_hscb->xfer_len = asd_htole32(cmd->request_bufflen);

	ata_hscb->data_offset = 0;
	ata_hscb->sister_scb = 0xffff;
	ata_hscb->conn_handle = target->ddb_profile.conn_handle;
	ata_hscb->retry_cnt = TASK_RETRY_CNT;
	ata_hscb->affiliation_policy = 0;

#ifdef TAGGED_QUEUING
	// RST - add support for SATA II queueing
	ata_hscb->ata_flags = LEGACY_QUEUING;
#else
	ata_hscb->ata_flags = UNTAGGED;
#endif
	/*
	 * Guard against stale sense data.  The Linux mid-layer assumes that 
	 * sense was retrieved anytime the first byte of the sense buffer 
	 * looks "sane".
	 */
	cmd->sense_buffer[0] = 0;
	cmd->resid = 0;

	asd_push_post_stack(asd, scb, (void *)cmd, asd_sata_linux_scb_done);

	switch (cmd->cmnd[0]) {
#if !defined(__VMKLNX__)
	/*
	 * FORMAT_UNIT code path makes use of an obsolete field "sc_request"
	 * in the struct scsi_cmnd structure.
	 * Commented out the code for now while I work for a workaround.
	 * It does not affect anything at this point.
	 */	
	case FORMAT_UNIT:
		ret = asd_sata_format_unit_build(asd, dev, scb, cmd);

		/*
		 * We will hand build the scb, no need to call asd_setup_data.
		 */
		return ret;
#endif /* #if !defined(__VMKLNX__) */

	case INQUIRY:
		ret = asd_sata_inquiry_build(asd, dev, scb, cmd);
		break;
	case LOG_SENSE:
		ret = asd_sata_log_sense_build(asd, dev, scb, cmd);
		break;
	case MODE_SELECT:
	case MODE_SELECT_10:
		ret = asd_sata_mode_select_build(asd, dev, scb, cmd);
		break;
	case MODE_SENSE:
	case MODE_SENSE_10:
		ret = asd_sata_mode_sense_build(asd, dev, scb, cmd);
		break;
	case READ_6:
	case READ_10:
	case READ_12:
		ret = asd_sata_read_build(asd, dev, scb, cmd);
		break;
	case WRITE_BUFFER:
		ret = asd_sata_write_buffer_build(asd, dev, scb, cmd);
		break;
	case READ_BUFFER:
		ret = asd_sata_read_buffer_build(asd, dev, scb, cmd);
		break;
	case READ_CAPACITY:
		ret = asd_sata_read_capacity_build(asd, dev, scb, cmd);
		break;
	case REPORT_LUNS:
		ret = asd_sata_report_luns_build(asd, dev, scb, cmd);
		break;
	case REQUEST_SENSE:
		ret = asd_sata_request_sense_build(asd, dev, scb, cmd);
		break;
	case REZERO_UNIT:
		ret = asd_sata_rezero_unit_build(asd, dev, scb, cmd);
		break;
	case SEEK_6:
	case SEEK_10:
		ret = asd_sata_seek_build(asd, dev, scb, cmd);
		break;
	case SEND_DIAGNOSTIC:
		ret = asd_sata_send_diagnostic_build(asd, dev, scb, cmd);
		break;
	case START_STOP:
		ret = asd_sata_start_stop_build(asd, dev, scb, cmd);
		break;
	case SYNCHRONIZE_CACHE:
		ret = asd_sata_synchronize_cache_build(asd, dev, scb, cmd);
		break;
	case TEST_UNIT_READY:
		ret = asd_sata_test_unit_ready_build(asd, dev, scb, cmd);
		break;
#ifdef T10_04_136
	case VERIFY:
		ret = asd_sata_verify_build(asd, dev, scb, cmd);
		break;
#endif
	case WRITE_6:
	case WRITE_10:
	case WRITE_12:
		ret = asd_sata_write_build(asd, dev, scb, cmd);
		break;
#ifdef T10_04_136
	case WRITE_VERIFY:
		ret = asd_sata_write_verify_build(asd, dev, scb, cmd);
		break;
#endif

#if defined(__VMKLNX__)
	/*
	 * Fake the RESERVE/RELEASE commands as they are not supported
	 * by SATA disks.
	 */
	case RESERVE:
        case RELEASE:
                ret = asd_sata_set_good_condition(cmd);
                break;
#endif /* #if defined(__VMKLNX__) */
	default:
		asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
				INVALID_COMMAND_OPERATION, 0);

		return ASD_COMMAND_BUILD_FAILED;
	}
	
	if (ret != ASD_COMMAND_BUILD_OK) {
		return ret;
	}

	ret = asd_setup_data(asd, scb, cmd);

	return ret;
}

/* -------------------------------------------------------------------------- */

ASD_COMMAND_BUILD_STATUS
asd_build_atapi_scb(
struct asd_softc	*asd,
struct scb		*scb,
union asd_cmd		*acmd
)
{
	ASD_COMMAND_BUILD_STATUS	ret;
	struct asd_atapi_task_hscb	*atapi_hscb;
	struct hd_driveid		*hd_driveidp;
	struct asd_target		*target;
	struct scsi_cmnd		*cmd;
	struct asd_device		*dev;

	dev = scb->platform_data->dev;

	cmd =  &acmd->scsi_cmd;

	/*
	 * Guard against stale sense data. The Linux mid-layer assumes that 
	 * sense was retrieved anytime the first byte of the sense buffer 
	 * looks "sane".
	 */
	cmd->sense_buffer[0] = 0;
	cmd->resid = 0;

	atapi_hscb = &scb->hscb->atapi_task;

	target = dev->target;

	atapi_hscb->header.opcode = SCB_INITIATE_ATAPI_TASK;
	atapi_hscb->protocol_conn_rate =
		PROTOCOL_TYPE_SATA | target->ddb_profile.conn_rate;
	atapi_hscb->xfer_len = asd_htole32(cmd->request_bufflen);

	atapi_hscb->data_offset = 0;
	atapi_hscb->sister_scb = 0xffff;
	atapi_hscb->conn_handle = target->ddb_profile.conn_handle;
	atapi_hscb->retry_cnt = TASK_RETRY_CNT;
	atapi_hscb->affiliation_policy = 0;

	hd_driveidp = &dev->target->atapi_cmdset.adp_hd_driveid;

#ifdef TAGGED_QUEUING
	// RST - add support for SATA II queueing
	atapi_hscb->ata_flags = LEGACY_QUEUING;
#else
	atapi_hscb->ata_flags = UNTAGGED;
#endif

	memcpy(atapi_hscb->atapi_packet, cmd->cmnd, MAX_COMMAND_SIZE);

	// issue a PACKET command 6.24 of the ATA/ATAPI spec
	asd_sata_setup_fis((struct asd_ata_task_hscb *)atapi_hscb,
		WIN_PACKETCMD);

	switch (cmd->sc_data_direction)
	{
	case SCSI_DATA_READ:
		atapi_hscb->ata_flags |= DATA_DIR_INBOUND;
		break;

	case SCSI_DATA_WRITE:
		atapi_hscb->ata_flags |= DATA_DIR_OUTBOUND;
		break;

	case SCSI_DATA_NONE:
		atapi_hscb->ata_flags |= DATA_DIR_NO_XFER;
		break;

	case SCSI_DATA_UNKNOWN:
		atapi_hscb->ata_flags |= DATA_DIR_UNSPECIFIED;
		break;
	}

	if ((cmd->sc_data_direction != SCSI_DATA_NONE)) {
		SET_NO_IO_MODE(atapi_hscb);
	}
	else if (target->atapi_cmdset.features_state & SATA_USES_DMA) {
		ASD_H2D_FIS(atapi_hscb)->features = PACKET_DMA_BIT;

		if (hd_driveidp->dma_1word & DMADIR_BIT_NEEDED) {
			if (cmd->sc_data_direction == SCSI_DATA_READ) {
				ASD_H2D_FIS(atapi_hscb)->features |= 
					DMADIR_BIT_DEV_TO_HOST;
			} else {
				ASD_H2D_FIS(atapi_hscb)->features &= 
					~DMADIR_BIT_DEV_TO_HOST;
			}
		}

		SET_DMA_MODE(atapi_hscb);
	} else {
		SET_PIO_MODE(atapi_hscb);
	}

	ASD_H2D_FIS(atapi_hscb)->byte_count_lo = cmd->request_bufflen & 0xff;
	ASD_H2D_FIS(atapi_hscb)->byte_count_hi = 
		(cmd->request_bufflen >> 8) & 0xff;

	asd_push_post_stack(asd, scb, (void *)cmd, asd_sata_linux_scb_done);
	asd_push_post_stack(asd, scb, (void *)cmd, asd_sata_atapi_post);

	ret = asd_setup_data(asd, scb, cmd);

	return ret;
}

void
asd_sata_atapi_post(
struct asd_softc	*asd,
struct scb		*scb,
struct asd_done_list	*done_listp
)
{
	struct ata_resp_edb	*ata_resp_edbp;
	struct scsi_cmnd	*cmd;
	u_int			 edb_index;
	struct scb 		 *escb;


	cmd = (struct scsi_cmnd *)scb->io_ctx;

	switch (done_listp->opcode) {
	case ATA_TASK_COMP_W_RESP:
		ata_resp_edbp = asd_sata_get_edb(asd, done_listp,&escb, &edb_index);
		asd_sata_set_check_condition(cmd,
			(ASD_D2H_FIS(ata_resp_edbp)->error & 0xf0) >> 4, 0, 0);
		asd_hwi_free_edb(asd, escb, edb_index);


		asd_pop_post_stack(asd, scb, done_listp);
		return;

	case TASK_COMP_WO_ERR:
		asd_cmd_set_host_status(cmd, DID_OK);
		asd_pop_post_stack(asd, scb, done_listp);
		return;

	default:
		/*
		 * The response will be handled by the routine that we are
		 * "popping" to.
		 */
		asd_pop_post_stack(asd, scb, done_listp);
		return;
	}

	asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
			INVALID_COMMAND_OPERATION, 0);

	asd_pop_post_stack(asd, scb, done_listp);
}

static void
asd_sata_linux_scb_done(
struct asd_softc	*asd,
struct scb		*scb,
struct asd_done_list	*dl
)
{
	Scsi_Cmnd 		*cmd;
	struct asd_device	*dev;

	if ((scb->flags & SCB_ACTIVE) == 0) {
		asd_print("SCB %d done'd twice\n", SCB_GET_INDEX(scb));
		panic("Stopping for safety");
	}

	list_del(&scb->owner_links);

	cmd = (struct scsi_cmnd *)scb->io_ctx;
	dev = scb->platform_data->dev;
	dev->active--;
	dev->openings++;
	if ((scb->flags & SCB_DEV_QFRZN) != 0) {
		scb->flags &= ~SCB_DEV_QFRZN;
		dev->qfrozen--;
	}

	asd_unmap_scb(asd, scb);

	if (dl != NULL) {
#if 0
	if( (dl->opcode != TASK_COMP_WO_ERR) && (dl->opcode != TASK_COMP_W_UNDERRUN) ) 
	{
#if LINUX_VERSION_CODE <  KERNEL_VERSION(2,6,13)
		asd_log(ASD_DBG_INFO, "asd_scb_done with error cmd LBA 0x%02x%02x%02x%02x - 0x%02x%02x (tag:0x%x, pid:0x%x, res_count:0x%x, timeout:0x%x abort:%d) dl->opcode 0x%x\n",
#else
		asd_log(ASD_DBG_INFO, "asd_scb_done with error cmd LBA 0x%02x%02x%02x%02x - 0x%02x%02x (tag:0x%x, pid:0x%x, res_count:0x%x, timeout:0x%x) dl->opcode 0x%x\n",
#endif
			cmd->cmnd[2],
			cmd->cmnd[3],
			cmd->cmnd[4],
			cmd->cmnd[5],
			cmd->cmnd[7],
			cmd->cmnd[8],
		  cmd->tag,
		  cmd->pid,
		  cmd->resid,
		  cmd->timeout_per_command,
#if LINUX_VERSION_CODE <  KERNEL_VERSION(2,6,13)
		  cmd->abort_reason,
#endif
		  dl->opcode);

		}
#endif

		switch (dl->opcode) {
		case TASK_COMP_W_UNDERRUN:
			cmd->resid = asd_le32toh(dl->stat_blk.data.res_len);
			asd_cmd_set_host_status(cmd, DID_OK);
			break;

		case TASK_ABORTED_ON_REQUEST:
			asd_cmd_set_host_status(cmd, DID_ABORT);
			break;

		case TASK_CLEARED:
		{
			struct task_cleared_sb	*task_clr;

			task_clr = &dl->stat_blk.task_cleared;

			asd_log(ASD_DBG_ERROR," Task Cleared for Tag: 0x%x, "
				"TC: 0x%x.\n",
				task_clr->tag_of_cleared_task,
				SCB_GET_INDEX(scb));

			/*
		 	 * Pending command at the firmware's queues aborted
			 * upon request. If the device is offline then failed
			 * the IO.
		 	 * Otherwise, have the command retried again.
	         	 */
			if (task_clr->clr_nxs_ctx == ASD_TARG_HOT_REMOVED) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
				asd_cmd_set_host_status(cmd, DID_NO_CONNECT);
#else
				asd_cmd_set_offline_status(cmd);
#endif
			} else
				asd_cmd_set_host_status(cmd, DID_SOFT_ERROR);

			break;
		}

		case TASK_INT_W_BRK_RCVD:
			asd_log(ASD_DBG_ERROR,
				"TASK INT. WITH BREAK RECEIVED.\n");
			asd_cmd_set_host_status(cmd, DID_SOFT_ERROR);
			break;

		case TASK_ABORTED_BY_ITNL_EXP:
		{
			struct itnl_exp_sb	*itnl_exp;

			itnl_exp = &dl->stat_blk.itnl_exp;

			asd_log(ASD_DBG_ERROR,
				"ITNL EXP for SCB 0x%x Reason = 0x%x.\n",
				SCB_GET_INDEX(scb), itnl_exp->reason);
		
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
			asd_cmd_set_host_status(cmd, DID_NO_CONNECT);
#else
			asd_cmd_set_offline_status(cmd);
#endif
			break;
		}
	
		default:
			//asd_cmd_set_host_status(cmd, DID_SOFT_ERROR);
			break;
		}
	}

	if ((dev->target->flags & ASD_TARG_HOT_REMOVED) != 0) {
		/*
	 	 * If the target had been removed and all active IOs on 
		 * the device have been completed, schedule the device to
		 * be destroyed.
	 	 */
		if (list_empty(&dev->busyq) && (dev->active == 0) &&
		   ((dev->flags & ASD_DEV_DESTROY_WAS_ACTIVE) != 0)) {
			/*
			 * Schedule a deferred process task to destroy
		         * the device.
			 */	 
			asd_setup_dev_dpc_task(dev, asd_destroy_device);
		}
	} else {
		if ((dev->flags & ASD_DEV_ON_RUN_LIST) == 0) {
			list_add_tail(&dev->links,
				      &asd->platform_data->device_runq);
			dev->flags |= ASD_DEV_ON_RUN_LIST;
		}
	}

	/*
	 * Only free the scb if it hasn't timedout.
	 * For SCB that has timedout, error recovery has invoked and
	 * the timedout SCB will be freed in the error recovery path.
	 */
	if ((scb->flags & SCB_TIMEDOUT) == 0)
		asd_hwi_free_scb(asd, scb);
//JDTEST
	else
	{
		asd_log(ASD_DBG_ERROR, "scb 0x%x SCB_TIMEDOUT(0x%x)\n",scb,scb->flags);
		scb->flags |= SCB_ABORT_DONE;
	}

	cmd->scsi_done(cmd);
}

/* -------------------------------------------------------------------------- */

INLINE
void
asd_sata_setup_fis(
struct asd_ata_task_hscb	*ata_hscb,
uint8_t				command
)
{
	memset(&ASD_H2D_FIS(ata_hscb)->features, 0,
		FIS_LENGTH - FIS_OFFSET(features));

	ASD_H2D_FIS(ata_hscb)->fis_type = FIS_HOST_TO_DEVICE;
	ASD_H2D_FIS(ata_hscb)->cmd_devcontrol = FIS_COMMAND;
	ASD_H2D_FIS(ata_hscb)->command = command;
}

void
asd_sata_compute_support(
struct asd_softc	*asd,
struct asd_target	*target
)
{
	struct hd_driveid	*hd_driveidp;
	unsigned		*features_state;
	unsigned		*features_enabled;
	unsigned		dma_mode_bits;
	unsigned		*dma_mode_level;

	switch (target->command_set_type) {
	case ASD_COMMAND_SET_ATA:
		hd_driveidp = &target->ata_cmdset.adp_hd_driveid;
		features_state = &target->ata_cmdset.features_state;
		features_enabled = &target->ata_cmdset.features_enabled;
		dma_mode_level = &target->ata_cmdset.dma_mode_level;
		break;

	case ASD_COMMAND_SET_ATAPI:
		hd_driveidp = &target->atapi_cmdset.adp_hd_driveid;
		features_state = &target->atapi_cmdset.features_state;
		features_enabled = &target->atapi_cmdset.features_enabled;
		dma_mode_level = &target->atapi_cmdset.dma_mode_level;
		break;

	default:
		return;
	}

	*features_state = (hd_driveidp->cfs_enable_2 & IDENT_48BIT_SUPPORT) ?
			SATA_USES_48BIT : 0;

#ifdef TAGGED_QUEUING
	*features_state |= (hd_driveidp->cfs_enable_2 & IDENT_QUEUED_SUPPORT) ?
			SATA_USES_QUEUEING : 0;
#endif

	*features_state |= (hd_driveidp->capability & IDENT_DMA_SUPPORT) ?
			SATA_USES_DMA : 0;

	*features_state |= (hd_driveidp->cfsse & IDENT_WRITE_FUA_SUPPORT) ?
			SATA_USES_WRITE_FUA : 0;

	*features_state |= (hd_driveidp->config & ATA_REMOVABLE) ? 	
		SATA_USES_REMOVABLE : 0;

	if ((hd_driveidp->command_set_1 & ATA_WRITE_BUFFER_CAPABLE) &&
	    (hd_driveidp->cfs_enable_1 & ATA_WRITE_BUFFER_ENABLED)) {
		*features_state |= SATA_USES_WRITE_BUFFER;
	}

	if ((hd_driveidp->command_set_1 & ATA_READ_BUFFER_CAPABLE) &&
	    (hd_driveidp->cfs_enable_1 & ATA_READ_BUFFER_ENABLED)) {
		*features_state |= SATA_USES_READ_BUFFER;
	}

	if ((hd_driveidp->command_set_1 & ATA_SMART_CAPABLE) &&
	    (hd_driveidp->cfs_enable_1 & ATA_SMART_ENABLED)) {
		*features_state |= SATA_USES_SMART;
	}

	if ((hd_driveidp->command_set_1 & ATA_WRITE_CACHE_CAPABLE) &&
	    (hd_driveidp->cfs_enable_1 & ATA_WRITE_CACHE_ENABLED)) {
		*features_state |= SATA_USES_WRITE_CACHE;
	}

	if ((hd_driveidp->command_set_1 & ATA_LOOK_AHEAD_CAPABLE) &&
	    (hd_driveidp->cfs_enable_1 & ATA_LOOK_AHEAD_ENABLED)) {
		*features_state |= SATA_USES_READ_AHEAD;
	}

	*features_enabled = 0;

	/*
	 * features_enabled represents the desired state of the feature.
	 */
	*features_enabled = 
		(hd_driveidp->command_set_1 & ATA_WRITE_CACHE_CAPABLE) ?
		WRITE_CACHE_FEATURE_ENABLED : 0;

	*features_enabled |=
		(hd_driveidp->command_set_1 & ATA_LOOK_AHEAD_CAPABLE) ?
		READ_AHEAD_FEATURE_ENABLED : 0;

	*features_enabled |=
		(hd_driveidp->command_set_1 & ATA_SMART_CAPABLE) ?
		SMART_FEATURE_ENABLED : 0;

	*dma_mode_level = 0;

	if (hd_driveidp->field_valid & ATA_DMA_ULTRA_VALID) {

		dma_mode_bits = hd_driveidp->dma_ultra & DMA_ULTRA_MODE_MASK;

		if (dma_mode_bits != 0) {
			*features_state |= SATA_USES_UDMA;
 			*features_enabled |= NEEDS_XFER_SETFEATURES;

			dma_mode_bits = dma_mode_bits >> 1;

			while (dma_mode_bits != 0) {
				dma_mode_bits = dma_mode_bits >> 1;
				*dma_mode_level = *dma_mode_level + 1;
			}

			*dma_mode_level |= UDMA_XFER_MODE;
		}
	}

#if 0
	... next check byte (field_valid) 53 to see if words 70:64 are valid
	... word 64 - eide_pio_modes
#endif
}

INLINE
void
asd_sata_setup_lba_ext(
struct asd_ata_task_hscb	*ata_hscb,
uint32_t			lba,
unsigned			sectors
)
{
	ASD_H2D_FIS(ata_hscb)->lba0 = lba & 0xff;
	ASD_H2D_FIS(ata_hscb)->lba1 = (lba >> 8) & 0xff;
	ASD_H2D_FIS(ata_hscb)->lba2 = (lba >> 16) & 0xff;
	ASD_H2D_FIS(ata_hscb)->lba3 = (lba >> 24);

	ASD_H2D_FIS(ata_hscb)->sector_count = sectors & 0xff;
	ASD_H2D_FIS(ata_hscb)->sector_count_exp = sectors >> 8;

	/*
	 * Section 3.1.39 of ATA-ATAPI-7 says:
	 * In a serial implementation, the device ignores the DEV bit.
	 */
	ASD_H2D_FIS(ata_hscb)->dev_head = LBA_MODE;
}

INLINE
void
asd_sata_setup_lba(
struct asd_ata_task_hscb	*ata_hscb,
uint32_t			lba,
unsigned			sectors
)
{
	ASD_H2D_FIS(ata_hscb)->lba0 = lba & 0xff;
	ASD_H2D_FIS(ata_hscb)->lba1 = (lba >> 8) & 0xff;
	ASD_H2D_FIS(ata_hscb)->lba2 = (lba >> 16) & 0xff;

	ASD_H2D_FIS(ata_hscb)->sector_count = sectors & 0xff;
	ASD_H2D_FIS(ata_hscb)->dev_head = LBA_MODE | ((lba >> 24) & 0x0f);
}

void
asd_sata_set_check_condition(
struct scsi_cmnd	*cmd,
unsigned		sense_key,
unsigned		additional_sense,
unsigned		additional_sense_qualifier
)
{
	u_char		sense_data[SENSE_DATA_SIZE];

	memset(sense_data, 0, SENSE_DATA_SIZE);

	asd_cmd_set_driver_status(cmd, DRIVER_SENSE);
	asd_cmd_set_host_status(cmd, DID_OK);
	asd_cmd_set_scsi_status(cmd, CHECK_CONDITION << 1);

	sense_data[0] = 0x70;
	sense_data[2] = sense_key;
	sense_data[7] = SENSE_DATA_SIZE - 7;

	sense_data[12] = additional_sense;
	sense_data[13] = additional_sense_qualifier;

	memset(cmd->sense_buffer, 0, sizeof(cmd->sense_buffer));
	memcpy(cmd->sense_buffer, sense_data, sizeof(cmd->sense_buffer));

	// RST - we need to save away the sense data
}

void
asd_sata_check_registers(
struct ata_resp_edb	*ata_resp_edbp,
struct scb		*scb,
struct scsi_cmnd	*cmd
)
{
	/*
	 * Check the interface state registers.
	 */
#if 0
	if ((ata_resp_edbp->sstatus & SSTATUS_IPM_DET_MASK) !=
		(SSTATUS_DET_COM_ESTABLISHED | SSTATUS_IPM_ACTIVE)) {
		printk("sstatus = 0x%x\n", ata_resp_edbp->sstatus);

		asd_sata_set_check_condition(cmd, NOT_READY,
			LOGICAL_UNIT_ASC, 0);

		return ASD_COMMAND_BUILD_FAILED;
	}

	if ((ata_resp_edbp->sstatus & SSTATUS_SPD_MASK) == 0) {
		printk("sstatus = 0x%x\n", ata_resp_edbp->sstatus);

		asd_sata_set_check_condition(cmd, NOT_READY,
			LOGICAL_UNIT_ASC, 0);

		return;
	}
#endif

	if (ata_resp_edbp->serror != 0) {
		asd_print("serror = 0x%x\n", ata_resp_edbp->serror);

		// RST - make this better
		asd_sata_set_check_condition(cmd, NOT_READY,
			LOGICAL_UNIT_ASC, 0);

		return;
	}

	if ((ASD_D2H_FIS(ata_resp_edbp)->status) & (ERR_STAT | WRERR_STAT)) {
		asd_print("status = 0x%x\n",
			ASD_D2H_FIS(ata_resp_edbp)->status);

		asd_sata_completion_status(cmd, ata_resp_edbp);

		return;
	}

	return;
}

void
asd_sata_completion_status(
struct scsi_cmnd	*cmd,
struct ata_resp_edb	*ata_resp_edbp
)
{
	if ((ASD_D2H_FIS(ata_resp_edbp)->error) & NM_ERR) {
		/*
		 * no medium
		 */
		asd_sata_set_check_condition(cmd, MEDIUM_ERROR,
			MEDIUM_NOT_PRESENT, 0);

		return;
	}

	if ((ASD_D2H_FIS(ata_resp_edbp)->error) & ABRT_ERR) {
		/*
		 * command aborted
		 */
		asd_sata_set_check_condition(cmd, ABORTED_COMMAND,
			IO_PROCESS_TERMINATED_ASC, IO_PROCESS_TERMINATED_ASCQ);

		return;
	}

	if ((ASD_D2H_FIS(ata_resp_edbp)->error) & MCR_ERR) {
		/*
		 * medium change request
		 */
		asd_sata_set_check_condition(cmd, UNIT_ATTENTION,
			OPERATOR_ASC, MEDIUM_REMOVAL_REQUEST);

		return;
	}

	if ((ASD_D2H_FIS(ata_resp_edbp)->error) & ID_ERR) {
		/*
		 * address out of bounds
		 */
		asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
			INVALID_FIELD_IN_CDB, 0);

		return;
	}

	if ((ASD_D2H_FIS(ata_resp_edbp)->error) & MC_ERR) {
		/*
		 * medium changed
		 */
		asd_sata_set_check_condition(cmd, UNIT_ATTENTION,
			MEDIUM_CHANGED, 0);

		return;
	}

	if ((ASD_D2H_FIS(ata_resp_edbp)->error) & UNC_ERR) {
		/*
		 * uncorrectable ECC error
		 */
		asd_sata_set_check_condition(cmd, MEDIUM_ERROR,
			LOGICAL_UNIT_ASC, 0);

		return;
	}

	if ((ASD_D2H_FIS(ata_resp_edbp)->error) & ICRC_ERR) {
		/*
		 * CRC error
		 */
		asd_sata_set_check_condition(cmd, MEDIUM_ERROR,
			LOGICAL_UNIT_ASC, COMMUNICATION_CRC_ERROR);

		return;
	}

	asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
			INVALID_COMMAND_OPERATION, 0);

	return;
}
INLINE
struct ata_resp_edb *
asd_sata_get_edb(
struct asd_softc	*asd,
struct asd_done_list	*done_listp,
struct scb **pescb, 
u_int *pedb_index
)
{
	struct response_sb	*responsep;
	unsigned		buflen;
	struct scb		*escb;
	u_int			escb_index;
	u_int			edb_index;
	struct ata_resp_edb	*ata_resp_edbp;
	union edb		*edbp;

	responsep = &done_listp->stat_blk.response;

	buflen = (responsep->empty_buf_elem & EDB_ELEM_MASK) << 4 | 
		responsep->empty_buf_len;

	if (buflen == sizeof(struct ata_resp_edb)) {
		asd_print("buflen is %d - should be %u\n",
			buflen, (unsigned)sizeof(struct ata_resp_edb));
	}

	escb_index = asd_le16toh(responsep->empty_scb_tc);

	edb_index = RSP_EDB_ELEM(responsep) - 1;

	edbp = asd_hwi_indexes_to_edb(asd, &escb, escb_index, edb_index);

	ata_resp_edbp = &edbp->ata_resp;
	*pescb = escb;
	*pedb_index = edb_index;

	return ata_resp_edbp;
}

/*
 * This routine will only get called when a command that was issued by the
 * state machine has completed.
 */
void
asd_sata_mode_select_wakeup_state_machine(
struct state_machine_context	*sm_contextp
)
{
	DISCOVER_RESULTS	results;
	struct asd_softc	*asd;
	struct scb		*scb;
	struct asd_device 	*dev;
	struct asd_target	*target;
	struct scsi_cmnd	*cmd;

	results = asd_run_state_machine(sm_contextp);

	if (results == DISCOVER_OK) {
		/*
		 * The state machine is still running.  We will get called
		 * again.
		 */
		return;
	}

	scb = (struct scb *)sm_contextp->state_handle;

	dev = scb->platform_data->dev;

	target = dev->target;

	asd = (struct asd_softc *)target->softc;

	cmd = (struct scsi_cmnd *)scb->io_ctx;

	if (results == DISCOVER_FINISHED) {
		asd_cmd_set_host_status(cmd, DID_OK);
	} else {
		asd_cmd_set_host_status(cmd, DID_ERROR);
	}

	asd_pop_post_stack(asd, scb, NULL);
}


int
asd_sata_identify_build(
struct asd_softc	*asd,
struct asd_target	*target,
struct scb		*scb
)
{
	struct asd_ata_task_hscb	*ata_hscb;
	int				dir;
	int				error;
	dma_addr_t			addr;

	ata_hscb = &scb->hscb->ata_task;

	ata_hscb->header.opcode = SCB_INITIATE_ATA_TASK;
	ata_hscb->protocol_conn_rate = 
		PROTOCOL_TYPE_SATA | target->ddb_profile.conn_rate;
	ata_hscb->xfer_len = asd_htole32(sizeof(struct hd_driveid));

	switch (target->command_set_type) {
	case ASD_COMMAND_SET_ATAPI:
		asd_sata_setup_fis(ata_hscb, WIN_PIDENTIFY);
		break;
	case ASD_COMMAND_SET_ATA:
		asd_sata_setup_fis(ata_hscb, WIN_IDENTIFY);
		break;
	default:
		return 1;
	}

	ata_hscb->data_offset = 0;
	ata_hscb->sister_scb = 0xffff;
	ata_hscb->conn_handle = target->ddb_profile.conn_handle;
	ata_hscb->retry_cnt = TASK_RETRY_CNT;

	ata_hscb->ata_flags = UNTAGGED | DATA_DIR_INBOUND;
	SET_PIO_MODE(ata_hscb);
	ata_hscb->affiliation_policy = 0;

	dir = scsi_to_pci_dma_dir(SCSI_DATA_READ);

	addr = asd_map_single(asd, 
		&target->ata_cmdset.adp_hd_driveid,
		sizeof(struct hd_driveid), dir);

	scb->platform_data->buf_busaddr = addr;

	error = asd_sg_setup(scb->sg_list, addr, sizeof(struct hd_driveid),
		1);

	if (error != 0) {
		asd_unmap_single(asd, addr, sizeof(struct hd_driveid), dir);

		return 1;
	}

	scb->sg_count = 1;

	memcpy(ata_hscb->sg_elements, scb->sg_list, sizeof(struct sg_element));

	asd_push_post_stack(asd, scb, (void *)target, asd_sata_identify_post);

	return 0;
}

/* shamelessly stolen from ide-iops.c */
static void
asd_sata_ide_fixstring(
u8 *s,
const int bytecount,
const int byteswap
)
{
	u8 *p = s, *end = &s[bytecount & ~1]; /* bytecount must be even */

	if (byteswap) {
		/* convert from big-endian to host byte order */
		for (p = end ; p != s;) {
			unsigned short *pp = (unsigned short *) (p -= 2);
			*pp = ntohs(*pp);
		}
	}
	/* strip leading blanks */
	while (s != end && *s == ' ')
		++s;
	/* compress internal blanks and strip trailing blanks */
	while (s != end && *s) {
		if (*s++ != ' ' || (s != end && *s && *s != ' '))
			*p++ = *(s-1);
	}
	/* wipe out trailing garbage */
	while (p != end)
		*p++ = '\0';
}

void
asd_sata_identify_post(
struct asd_softc	*asd,
struct scb		*scb,
struct asd_done_list	*done_listp
)
{
	struct ata_resp_edb	*ata_resp_edbp;
	// RST - declare this on the stack for now
	struct scsi_cmnd	cmd;
	struct asd_target	*target;
	u_int			 edb_index;
	struct scb 		 *escb;

	target = (struct asd_target *)scb->io_ctx;

	switch (done_listp->opcode) {
	case ATA_TASK_COMP_W_RESP:
		ata_resp_edbp = asd_sata_get_edb(asd, done_listp,&escb, &edb_index);
		asd_sata_check_registers(ata_resp_edbp, scb, &cmd);
		asd_hwi_free_edb(asd, escb, edb_index);
		asd_pop_post_stack(asd, scb, done_listp);
		return;

	case TASK_COMP_WO_ERR:
		break;

	default:
		asd_pop_post_stack(asd, scb, done_listp);
		return;
	}

	asd_sata_ide_fixstring((u8 *)&target->ata_cmdset.adp_hd_driveid.model,
		sizeof(target->ata_cmdset.adp_hd_driveid.model), 1);

	asd_sata_ide_fixstring((u8 *)&target->ata_cmdset.adp_hd_driveid.fw_rev,
		sizeof(target->ata_cmdset.adp_hd_driveid.fw_rev), 1);

	asd_sata_ide_fixstring((u8 *)&target->ata_cmdset.adp_hd_driveid.serial_no,
		sizeof(target->ata_cmdset.adp_hd_driveid.serial_no), 1);

	asd_pop_post_stack(asd, scb, done_listp);

	up(&asd->platform_data->discovery_sem);
}

int
asd_sata_set_features_build(
struct asd_softc	*asd,
struct asd_target	*target,
struct scb		*scb,
uint8_t			feature,
uint8_t			sector_count
)
{
	struct asd_ata_task_hscb	*ata_hscb;

	ata_hscb = &scb->hscb->ata_task;

	ata_hscb->header.opcode = SCB_INITIATE_ATA_TASK;
	ata_hscb->protocol_conn_rate = 
		PROTOCOL_TYPE_SATA | target->ddb_profile.conn_rate;
	ata_hscb->xfer_len = asd_htole32(sizeof(struct hd_driveid));

	asd_sata_setup_fis(ata_hscb, WIN_SETFEATURES);

	ASD_H2D_FIS(ata_hscb)->features = feature;
	ASD_H2D_FIS(ata_hscb)->sector_count = sector_count;

	ata_hscb->data_offset = 0;
	ata_hscb->sister_scb = 0xffff;
	ata_hscb->conn_handle = target->ddb_profile.conn_handle;
	ata_hscb->retry_cnt = TASK_RETRY_CNT;
	ata_hscb->affiliation_policy = 0;

	ata_hscb->ata_flags = UNTAGGED | DATA_DIR_NO_XFER;
	SET_NO_IO_MODE(ata_hscb);

	scb->sg_count = 0;

	memcpy(ata_hscb->sg_elements, scb->sg_list, sizeof(struct sg_element));

	asd_push_post_stack(asd, scb, (void *)target, asd_sata_configure_post);

	return 0;
}

void
asd_sata_configure_post(
struct asd_softc	*asd,
struct scb		*scb,
struct asd_done_list	*done_listp
)
{
	struct ata_resp_edb	*ata_resp_edbp;
	// RST - declare this on the stack for now
	struct scsi_cmnd	cmd;
	struct asd_target	*target;
	u_int			 edb_index;
	struct scb 		 *escb;

	target = (struct asd_target *)scb->io_ctx;

	switch (done_listp->opcode) {
	case ATA_TASK_COMP_W_RESP:
		ata_resp_edbp = asd_sata_get_edb(asd, done_listp,&escb, &edb_index);
		asd_sata_check_registers(ata_resp_edbp, scb, &cmd);
		asd_hwi_free_edb(asd, escb, edb_index);
		asd_pop_post_stack(asd, scb, done_listp);
		return;

	case TASK_COMP_WO_ERR:
		break;

	default:
		asd_pop_post_stack(asd, scb, done_listp);
		return;
	}

	asd_pop_post_stack(asd, scb, done_listp);

	up(&asd->platform_data->discovery_sem);
}

/* -------------------------------------------------------------------------- */

#if !defined(__VMKLNX__)
/*
 * "sc_request" field in "struct scsi_cmnd" is obsolete since 2.6.18.
 * adp94xx sata handling makes use of this field when formatting the unit.
 * We dont really use these code.  Commented out for now.
 */
/* -----------------------------------
 * FORMAT_UNIT: (emulated)
 *
 */
ASD_COMMAND_BUILD_STATUS
asd_sata_format_unit_build(
struct asd_softc	*asd,
struct asd_device 	*dev,
struct scb		*scb,
struct scsi_cmnd	*cmd
)
{
	struct asd_ata_task_hscb	*ata_hscb;
	struct scsi_cmnd		*write_cmd;
	int			  	dir;
	int			  	error;

	ata_hscb = &scb->hscb->ata_task;

	write_cmd = asd_alloc_mem(sizeof(struct scsi_cmnd), GFP_KERNEL);

	if (write_cmd == NULL) {
		asd_sata_format_unit_free_memory(asd, write_cmd);

		asd_sata_set_check_condition(cmd, HARDWARE_ERROR,
			RESOURCE_FAILURE, MEMORY_OUT_OF_SPACE);

		return ASD_COMMAND_BUILD_FAILED;
	}

	/*
	 * Since this is an internal command, we can initialize these fields
	 * to whatever we want.
	 */
	write_cmd->host_scribble = (unsigned char *)cmd;
	write_cmd->sc_request = NULL;
	write_cmd->request_buffer = NULL;

	if (asd_sata_write_build(asd, dev, scb, write_cmd) != 
		ASD_COMMAND_BUILD_OK) {

		asd_sata_format_unit_free_memory(asd, write_cmd);

		asd_sata_set_check_condition(cmd, HARDWARE_ERROR,
			FORMAT_FAILED, FORMAT_COMMAND_FAILED);

		return ASD_COMMAND_BUILD_FAILED;
	}

	/*
	 * We have to do this after sata_write_build, because it will initialize
	 * scb->post also.
	 */
	asd_push_post_stack(asd, scb, (void *)cmd, asd_sata_format_unit_post);

	write_cmd->use_sg = 0;

	write_cmd->request_bufflen = FORMAT_WRITE_BUFFER_LEN;

	write_cmd->sc_request = (Scsi_Request *)
		asd_alloc_mem(sizeof(struct map_node), GFP_KERNEL);

	if (write_cmd->sc_request == NULL) {

		asd_sata_format_unit_free_memory(asd, write_cmd);

		asd_sata_set_check_condition(cmd, HARDWARE_ERROR,
			RESOURCE_FAILURE, MEMORY_OUT_OF_SPACE);

		return ASD_COMMAND_BUILD_FAILED;
	}

	if (asd_alloc_dma_mem(asd, FORMAT_WRITE_BUFFER_LEN,
		&write_cmd->request_buffer,
		&scb->platform_data->buf_busaddr,
		(bus_dma_tag_t *)&write_cmd->device,
		(struct map_node *)write_cmd->sc_request) != 0) {

		asd_sata_format_unit_free_memory(asd, write_cmd);

		asd_sata_set_check_condition(cmd, HARDWARE_ERROR,
			RESOURCE_FAILURE, MEMORY_OUT_OF_SPACE);

		return ASD_COMMAND_BUILD_FAILED;
	}

	memset(write_cmd->request_buffer, 0, FORMAT_WRITE_BUFFER_LEN);

	write_cmd->sc_data_direction = SCSI_DATA_WRITE;

	scb->sg_count = 1;

	dir = scsi_to_pci_dma_dir(write_cmd->sc_data_direction);

	error = asd_sg_setup(scb->sg_list, scb->platform_data->buf_busaddr,
		FORMAT_WRITE_BUFFER_LEN, 1);

	if (error != 0) {
		asd_sata_format_unit_free_memory(asd, write_cmd);

		asd_sata_set_check_condition(cmd, HARDWARE_ERROR,
			RESOURCE_FAILURE, MEMORY_OUT_OF_SPACE);

		return ASD_COMMAND_BUILD_FAILED;
	}

	memcpy(ata_hscb->sg_elements, scb->sg_list,
		scb->sg_count * sizeof(struct sg_element));

	ata_hscb->ata_flags |= DATA_DIR_OUTBOUND;

	write_cmd->cmnd[0] = WRITE_10;
	write_cmd->cmnd[1] = 0;

	// lba
	write_cmd->cmnd[2] = 0;
	write_cmd->cmnd[3] = 0;
	write_cmd->cmnd[4] = 0;
	write_cmd->cmnd[5] = 0;

	// length
	write_cmd->cmnd[7] = (FORMAT_SECTORS >> 8) & 0xff;
	write_cmd->cmnd[8] = FORMAT_SECTORS & 0xff;
	write_cmd->cmnd[9] = 0;

	return ASD_COMMAND_BUILD_FINISHED;
}

void
asd_sata_format_unit_free_memory(
struct asd_softc	*asd,
struct scsi_cmnd	*write_cmd
)
{
	if (write_cmd == NULL) {
		return;
	}

	if (write_cmd->sc_request != NULL) {
		asd_free_mem(write_cmd->sc_request);
	}

	if (write_cmd->request_buffer != NULL) {
		asd_free_dma_mem(asd, (bus_dma_tag_t)write_cmd->device,
			(struct map_node *)write_cmd->sc_request);
	}

	if (write_cmd->sc_request != NULL) {
		asd_free_mem(write_cmd->sc_request);
	}

	asd_free_mem(write_cmd);
}

void
asd_sata_format_unit_post(
struct asd_softc	*asd,
struct scb		*scb,
struct asd_done_list	*done_listp
)
{
	struct ata_resp_edb	*ata_resp_edbp;
	struct scsi_cmnd	*write_cmd;
	struct asd_device 	*dev;
	uint32_t		lba;
	struct hd_driveid	*hd_driveidp;
	u_int			 edb_index;
	struct scb 		 *escb;

	write_cmd = (struct scsi_cmnd *)scb->io_ctx;

	switch (done_listp->opcode) {
	case ATA_TASK_COMP_W_RESP:
		ata_resp_edbp = asd_sata_get_edb(asd, done_listp,&escb, &edb_index);

		asd_sata_check_registers(ata_resp_edbp, scb, write_cmd);

		asd_sata_format_unit_free_memory(asd, write_cmd);

		scb->io_ctx = (asd_io_ctx_t)write_cmd->host_scribble;
		asd_hwi_free_edb(asd, escb, edb_index);
		asd_pop_post_stack(asd, scb, done_listp);
		return;

	case TASK_COMP_WO_ERR:
		break;

	default:
		asd_sata_set_check_condition(write_cmd, ILLEGAL_REQUEST,
			FORMAT_FAILED, FORMAT_COMMAND_FAILED);

		asd_sata_format_unit_free_memory(asd, write_cmd);

		scb->io_ctx = (asd_io_ctx_t)write_cmd->host_scribble;
		asd_pop_post_stack(asd, scb, done_listp);
		return;
	}

	dev = scb->platform_data->dev;

	hd_driveidp = &dev->target->ata_cmdset.adp_hd_driveid;

	lba = (write_cmd->cmnd[2] << 24) | (write_cmd->cmnd[3] << 16) |
		(write_cmd->cmnd[4] << 8) | write_cmd->cmnd[5];

	if (lba == (hd_driveidp->lba_capacity_2 - 1)) {
		/*
		 * We have finished formatting the drive.
		 */
		asd_cmd_set_host_status(write_cmd, DID_OK);

		asd_sata_format_unit_free_memory(asd, write_cmd);
		scb->io_ctx = (asd_io_ctx_t)write_cmd->host_scribble;
		asd_pop_post_stack(asd, scb, done_listp);

		return;
	}

	lba = lba + FORMAT_SECTORS;

	// lba
	write_cmd->cmnd[2] = (lba >> 24) & 0xff;
	write_cmd->cmnd[3] = (lba >> 16) & 0xff;
	write_cmd->cmnd[4] = (lba >> 8) & 0xff;
	write_cmd->cmnd[5] = lba & 0xff;

	list_add_tail(&((union asd_cmd *)write_cmd)->acmd_links, &dev->busyq);

	if ((dev->flags & ASD_DEV_ON_RUN_LIST) == 0) {

		list_add_tail(&dev->links, &asd->platform_data->device_runq);

		dev->flags |= ASD_DEV_ON_RUN_LIST;

		/*
		 * We don't need to run the device queues at this point because
		 * asd_isr (which ulimately called this routine) will call
		 * asd_next_device_to_run() / asd_schedule_runq() which
		 * will schedule the runq tasklet.
		 */
	}
}
#endif /* #if !defined(__VMKLNX__) */

void *asd_sata_setup_data(struct asd_softc *asd, struct scb *scb, Scsi_Cmnd *cmd)
{
	int				dir;
	void 			*buf_ptr;
	struct			scatterlist *cur_seg;
	u_int			nseg;
#if defined(__VMKLNX__)
        dma_addr_t              dma_addr;
#endif


	if (cmd->use_sg != 0) {

		cur_seg = (struct scatterlist *)cmd->request_buffer;
		dir = scsi_to_pci_dma_dir(cmd->sc_data_direction);
		nseg = asd_map_sg(asd, cur_seg, cmd->use_sg, dir);
		if (nseg > ASD_NSEG) {
			asd_unmap_sg(asd, cur_seg, nseg, dir);
			return NULL;
		}
		scb->sg_count = nseg;
#if defined(__VMKLNX__)
		buf_ptr = asd_alloc_coherent(asd, scsi_bufflen(cmd), &dma_addr);
		if (buf_ptr == NULL) {
                        return NULL;
                }

                if (dir == DMA_BIDIRECTIONAL || dir == DMA_TO_DEVICE) {
			sg_copy_to_buffer(scsi_sglist(cmd),
					  scsi_sg_count(cmd),
					  buf_ptr, scsi_bufflen(cmd));
                }
                /* save for later use in asd_sata_unmap_data */
		scb->bounce_buffer = buf_ptr;
#else
		buf_ptr = (void *)(page_address(cur_seg->page) + 
							cur_seg->offset);
#endif
		return buf_ptr;

	} 
	else
	{
		return (void *) cmd->request_buffer;
	}

}
void
asd_sata_unmap_data(struct asd_softc *asd, struct scb *scb, Scsi_Cmnd *cmd)
{
	int direction;
	struct scatterlist *sg;

	direction = scsi_to_pci_dma_dir(cmd->sc_data_direction);

	if (cmd->use_sg != 0) {

		sg = (struct scatterlist *)cmd->request_buffer;
#if defined(__VMKLNX__)
		if (scb->bounce_buffer != NULL) {
                        if (direction == DMA_BIDIRECTIONAL ||
                            direction == DMA_FROM_DEVICE) {
				sg_copy_from_buffer(scsi_sglist(cmd),
						    scsi_sg_count(cmd),
						    scb->bounce_buffer,
						    scsi_bufflen(cmd));
                        }
                        asd_free_coherent(asd, scsi_bufflen(cmd), scb->bounce_buffer,
                                          virt_to_phys(scb->bounce_buffer));
                        scb->bounce_buffer = NULL;
                }
#endif
		asd_unmap_sg(asd, sg, scb->sg_count, direction);
	} 
}
/* -----------------------------------
 * INQUIRY: (emulated)
 */

ASD_COMMAND_BUILD_STATUS
asd_sata_inquiry_build(
struct asd_softc	*asd,
struct asd_device 	*dev,
struct scb		*scb,
struct scsi_cmnd	*cmd
)
{
	struct asd_ata_task_hscb	*ata_hscb;
	struct hd_driveid		*hd_driveidp;
	u_char				inquiry_data[INQUIRY_RESPONSE_SIZE];
	u_char	*cmd_buf_ptr;
	ASD_COMMAND_BUILD_STATUS	ret;

	ata_hscb = &scb->hscb->ata_task;

	hd_driveidp = &dev->target->ata_cmdset.adp_hd_driveid;

	if (cmd->cmnd[1] & CMD_DT) {
		asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
			INVALID_FIELD_IN_CDB, 0);

		return ASD_COMMAND_BUILD_FAILED;
	}

	if (cmd->cmnd[1] & LUN_FIELD) {
		asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
			INVALID_FIELD_IN_CDB, 0);

		return ASD_COMMAND_BUILD_FAILED;
	}

	if (cmd->cmnd[1] & EVPD) {
		ret =  asd_sata_inquiry_evd_build(asd, dev, scb, cmd,
			inquiry_data);

		return ret;
	}

	if (cmd->cmnd[2] != 0x00) {
		asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
			INVALID_FIELD_IN_CDB, 0);

		return ASD_COMMAND_BUILD_FAILED;
	}

	// RST - 
	// A good strategy for this might be to issue the 
	// WIN_IDENTIFY &  WIN_PIDENTIFY
	// ... we might want to set affiliation bits in the ata_hscb
	// ... before we send them out

	memset(inquiry_data, 0, INQUIRY_RESPONSE_SIZE);

	inquiry_data[0] = TYPE_DISK;
	inquiry_data[1] = (dev->target->ata_cmdset.features_state & 
		SATA_USES_REMOVABLE) ?  SCSI_REMOVABLE : 0;
	inquiry_data[2] = SCSI_3;
	inquiry_data[3] = SCSI_3_RESPONSE_DATA_FORMAT;
	inquiry_data[4] = INQUIRY_RESPONSE_SIZE - 4;
	inquiry_data[5] = 0; // PROTECT | 3PC | ALUA | ACC | SCCS

	inquiry_data[6] = 0; // ADDR16 | MCHNGR | MULTIP | ENCSERV | BQUE

#ifdef TAGGED_QUEUING
	// LINKED | SUNC | WBUS16
	inquiry_data[7] = (target->ata_cmdset.features_state & SATA_USES_QUEUEING) ?
		CMD_QUEUE_BIT : 0;
#else
	inquiry_data[7] = 0; // CMD_QUEUE_BIT | LINKED | SUNC | WBUS16
#endif

	memcpy(&inquiry_data[8], hd_driveidp->model, 8);
	memcpy(&inquiry_data[16], hd_driveidp->model + 8, 16);
	memcpy(&inquiry_data[32], hd_driveidp->fw_rev, 4);

	inquiry_data[56] = 0;	// IUS | QAS | CLOCKING

	cmd_buf_ptr=(u_char*) asd_sata_setup_data(asd, scb, cmd);
	memcpy(cmd_buf_ptr, inquiry_data, cmd->request_bufflen);
	asd_sata_unmap_data(asd, scb, cmd);



	asd_cmd_set_host_status(cmd, DID_OK);

	return ASD_COMMAND_BUILD_FINISHED;
}

ASD_COMMAND_BUILD_STATUS
asd_sata_inquiry_evd_build(
struct asd_softc	*asd,
struct asd_device 	*dev,
struct scb		*scb,
struct scsi_cmnd	*cmd,
u_char			*inquiry_data
)
{
	struct hd_driveid		*hd_driveidp;
	u_char *cmd_buf_ptr;

	hd_driveidp = &dev->target->ata_cmdset.adp_hd_driveid;

	memset(inquiry_data, 0, INQUIRY_RESPONSE_SIZE);

	switch (cmd->cmnd[2]) {
	case SUPPORTED_VPD:
		inquiry_data[0] = TYPE_DISK;
		inquiry_data[1] = SUPPORTED_VPD;
		inquiry_data[3] = 2;
		inquiry_data[4] = SUPPORTED_VPD;
		inquiry_data[5] = UNIT_SERIAL_VPD;
		break;

	case UNIT_SERIAL_VPD:
		inquiry_data[0] = TYPE_DISK;
		inquiry_data[1] = UNIT_SERIAL_VPD;
		inquiry_data[3] = ATA_PRODUCT_SERIAL_LENGTH;
		memcpy(&inquiry_data[4], hd_driveidp->serial_no,
			ATA_PRODUCT_SERIAL_LENGTH);
		break;

	default:
		asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
			INVALID_FIELD_IN_CDB, 0);

		return ASD_COMMAND_BUILD_FAILED;
	}


	cmd_buf_ptr=(u_char*) asd_sata_setup_data(asd, scb, cmd);
	memcpy(cmd_buf_ptr, inquiry_data, cmd->request_bufflen);
	asd_sata_unmap_data(asd, scb, cmd);


	asd_cmd_set_host_status(cmd, DID_OK);

	return ASD_COMMAND_BUILD_FINISHED;
}

/* -----------------------------------
 * LOG_SENSE: (translated)
 */

ASD_COMMAND_BUILD_STATUS
asd_sata_log_sense_build(
struct asd_softc	*asd,
struct asd_device 	*dev,
struct scb		*scb,
struct scsi_cmnd	*cmd
)
{
	struct asd_ata_task_hscb	*ata_hscb;
	unsigned			page_control;
	struct hd_driveid		*hd_driveidp;

	hd_driveidp = &dev->target->ata_cmdset.adp_hd_driveid;

	ata_hscb = &scb->hscb->ata_task;

	if ((cmd->cmnd[2] & PAGE_CODE_MASK) != SMART_DATA) {
		asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
			INVALID_FIELD_IN_CDB, 0);

		return ASD_COMMAND_BUILD_FAILED;
	}

	if ((hd_driveidp->command_set_1 & ATA_SMART_CAPABLE) == 0) {

		asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
			INVALID_COMMAND_OPERATION, 0);

		return ASD_COMMAND_BUILD_FAILED;
	}

	/*
	 * "Smart"
	 */
	asd_sata_setup_fis(ata_hscb, WIN_SMART);

	ASD_H2D_FIS(ata_hscb)->features = SMART_READ_DATA;
	ASD_H2D_FIS(ata_hscb)->lba1 =  0x4f;
	ASD_H2D_FIS(ata_hscb)->lba2 =  0xc2;

	page_control = cmd->cmnd[2] & ~PAGE_CODE_MASK;

	switch (page_control) {
	case PAGE_CONTROL_CUMULATIVE:
	case PAGE_CONTROL_CURRENT:
		break;
	default:
		asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
			INVALID_FIELD_IN_CDB, 0);

		return ASD_COMMAND_BUILD_FAILED;
	}

	ata_hscb->ata_flags |= DATA_DIR_INBOUND;
	SET_PIO_MODE(ata_hscb);

	asd_push_post_stack(asd, scb, (void *)cmd, asd_sata_log_sense_post);

	return ASD_COMMAND_BUILD_OK;
}

void
asd_sata_log_sense_post(
struct asd_softc	*asd,
struct scb		*scb,
struct asd_done_list	*done_listp
)
{
	struct ata_resp_edb	*ata_resp_edbp;
	struct scsi_cmnd	*cmd;
	u_int			 edb_index;
	struct scb 		 *escb;

	cmd = (struct scsi_cmnd *)scb->io_ctx;

	switch (done_listp->opcode) {
	case ATA_TASK_COMP_W_RESP:
		ata_resp_edbp = asd_sata_get_edb(asd, done_listp,&escb, &edb_index);
		asd_sata_check_registers(ata_resp_edbp, scb, cmd);
		asd_hwi_free_edb(asd, escb, edb_index);
		asd_pop_post_stack(asd, scb, done_listp);
		return;

	case TASK_COMP_WO_ERR:
		asd_cmd_set_host_status(cmd, DID_OK);
		asd_pop_post_stack(asd, scb, done_listp);
		return;

	default:
		break;
	}

	asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
			INVALID_COMMAND_OPERATION, 0);

	asd_pop_post_stack(asd, scb, done_listp);
}

/* -----------------------------------
 * MODE_SELECT: (emulated / translated)
 * MODE_SELECT_10: (emulated / translated)
 */

ASD_COMMAND_BUILD_STATUS
asd_sata_mode_select_build(
struct asd_softc	*asd,
struct asd_device 	*dev,
struct scb		*scb,
struct scsi_cmnd	*cmd
)
{
	struct asd_ata_task_hscb	*ata_hscb;
	unsigned			parameter_list_length;
	unsigned			len;
	unsigned			page_len;
	uint8_t				*bufptr;
	unsigned			command_translated;
	unsigned			buf_offset;
	ASD_COMMAND_BUILD_STATUS	ret;
	uint8_t				page_code;
	uint8_t				*cmd_buf_ptr;


	ata_hscb = &scb->hscb->ata_task;

	if ((cmd->cmnd[1] & SP_BIT) || ((cmd->cmnd[1] & PF_BIT) != 0)) {
		asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
			INVALID_FIELD_IN_CDB, 0);
	}
	cmd_buf_ptr=(uint8_t*) asd_sata_setup_data(asd, scb, cmd);

	command_translated = 0;
	parameter_list_length = 0;
	page_len = 0;

	switch (cmd->cmnd[0]) {
	case MODE_SELECT:
		parameter_list_length = cmd->cmnd[4];
		break;
	case MODE_SELECT_10:
		parameter_list_length = asd_be32toh(
			*((uint16_t *)&cmd->cmnd[7]));
		break;
	}

	len = MIN(parameter_list_length, cmd->request_bufflen);

	for (buf_offset = 0 ; buf_offset < len ; 
		buf_offset = buf_offset + page_len ) {

		bufptr = cmd_buf_ptr + buf_offset;

		page_code = *bufptr;

		switch (page_code) {
		case READ_WRITE_ERROR_RECOVERY_MODE_PAGE:
			page_len = READ_WRITE_ERROR_RECOVERY_MODE_PAGE_LEN;
			break;

		case CACHING_MODE_PAGE:
			page_len = CACHING_MODE_PAGE_LEN;
			break;

		case CONTROL_MODE_PAGE:
			page_len = CONTROL_MODE_PAGE_LEN;
			break;

		case INFORMATIONAL_EXCEPTION_CONTROL_PAGE:
			page_len = INFORMATIONAL_EXCEPTION_CONTROL_PAGE_LEN;
			break;
		}

		if ((page_len + buf_offset) > len) {
			/*
			 * We will fail any Page Select commands that span
			 * over the end of our request.
			 */
			break;
		}


		switch (page_code) {
		case READ_WRITE_ERROR_RECOVERY_MODE_PAGE:
			ret = asd_sata_read_write_error_recovery_mode_select(
				asd, dev, scb, bufptr);
			break;

		case CACHING_MODE_PAGE:
			ret = asd_sata_caching_mode_select(asd, dev, scb,
				bufptr);
			break;

		case CONTROL_MODE_PAGE:
			ret = asd_sata_control_mode_select(asd, dev, scb,
				bufptr);
			break;

		case INFORMATIONAL_EXCEPTION_CONTROL_PAGE:
			ret = asd_sata_informational_exception_control_select(
				asd, dev, scb, bufptr);
			break;
		default:
			ret = ASD_COMMAND_BUILD_FAILED;
			break;
		}

		switch (ret) {
		case ASD_COMMAND_BUILD_OK:
			/*
			 * This command is being translated, so we have to
			 * wait for it to finish.
			 */
			command_translated = 1;
			break;

		case ASD_COMMAND_BUILD_FINISHED:
			/*
			 * The command was emulated.
			 */
			break;

		case ASD_COMMAND_BUILD_FAILED:
		default:
	asd_sata_unmap_data(asd, scb, cmd);
			return ASD_COMMAND_BUILD_FAILED;
		}
	}
	asd_sata_unmap_data(asd, scb, cmd);
	if (command_translated != 0) {
		/*
		 * This command is being translated, so we have to
		 * wait for it to finish.
		 */
		return ASD_COMMAND_BUILD_OK;
	}

	asd_cmd_set_host_status(cmd, DID_OK);

	return ASD_COMMAND_BUILD_FINISHED;
}

ASD_COMMAND_BUILD_STATUS
asd_sata_read_write_error_recovery_mode_select(
struct asd_softc	*asd,
struct asd_device 	*dev,
struct scb		*scb,
uint8_t			*bufptr
)
{
	if (bufptr[1] != (READ_WRITE_ERROR_RECOVERY_MODE_PAGE_LEN - 2)) {
		return ASD_COMMAND_BUILD_FAILED;
	}

	/*
	 * T10/04-136r0: Everything about this mode select is ignored for now.
	 */

	return ASD_COMMAND_BUILD_FINISHED;
}


ASD_COMMAND_BUILD_STATUS
asd_sata_caching_mode_select(
struct asd_softc	*asd,
struct asd_device 	*dev,
struct scb		*scb,
uint8_t			*bufptr
)
{
	struct asd_target			*target;
	unsigned				state_changed;
	unsigned				*features_enabled;
	unsigned				*features_state;
	DISCOVER_RESULTS			results;
	struct asd_ConfigureATA_SM_Arguments	args;
	struct state_machine_context		*sm_contextp;

	if (bufptr[1] != (CACHING_MODE_PAGE_LEN - 2)) {
		return ASD_COMMAND_BUILD_FAILED;
	}

	target = dev->target;

	state_changed = 0;

	switch (target->command_set_type) {
	case ASD_COMMAND_SET_ATA:
		features_enabled = &target->ata_cmdset.features_enabled;
		features_state = &target->ata_cmdset.features_state;
		break;

	case ASD_COMMAND_SET_ATAPI:
		features_enabled = &target->atapi_cmdset.features_enabled;
		features_state = &target->ata_cmdset.features_state;
		break;

	default:
		return ASD_COMMAND_BUILD_FAILED;
	}

	if (bufptr[12] & SCSI_DRA) {
		if ((*features_state & SATA_USES_READ_AHEAD) == 0) {
			state_changed = 1;
			*features_state |= SATA_USES_READ_AHEAD;
		}
	} else {
		if (*features_state & SATA_USES_READ_AHEAD) {
			state_changed = 1;
			*features_state &= ~SATA_USES_READ_AHEAD;
		}
	}

	if (bufptr[2] & SCSI_WCE) {
 		if ((*features_state & SATA_USES_WRITE_CACHE) == 0) {
 			state_changed = 1;
 			*features_state |= SATA_USES_WRITE_CACHE;
 		}
	} else {
		if (*features_state & SATA_USES_WRITE_CACHE) {
			state_changed = 1;
			*features_state &= ~SATA_USES_WRITE_CACHE;
		}
	}

	if (state_changed == 0) {
		/*
		 * We didn't change anything, so we are done.
		 */
		return ASD_COMMAND_BUILD_FINISHED;
	}

	sm_contextp = asd_alloc_mem(sizeof(struct state_machine_context),
		GFP_KERNEL);

	memset(sm_contextp, 0, sizeof(struct state_machine_context));

	sm_contextp->state_handle = (void *)scb;

	sm_contextp->wakeup_state_machine = 
		asd_sata_mode_select_wakeup_state_machine;

	args.target = target;

	results = ASD_PUSH_STATE_MACHINE(sm_contextp,
		&asd_ConfigureATA_SM, (void *)&args);

	switch (results) {
	case DISCOVER_FINISHED:
		return ASD_COMMAND_BUILD_FINISHED;
		break;

	case DISCOVER_FAILED:
	default:
		return ASD_COMMAND_BUILD_FAILED;
		break;

	case DISCOVER_OK:
		break;
	}

	return ASD_COMMAND_BUILD_OK;
}

ASD_COMMAND_BUILD_STATUS
asd_sata_control_mode_select(
struct asd_softc	*asd,
struct asd_device 	*dev,
struct scb		*scb,
uint8_t			*bufptr
)
{
	if (bufptr[1] != (CONTROL_MODE_PAGE_LEN - 2)) {
		return ASD_COMMAND_BUILD_FAILED;
	}

	/*
	 * T10/04-136r0 doesn't support this command, and Linux libata does
	 * not support select, so we will do nothing.
	 */

	return ASD_COMMAND_BUILD_FINISHED;
}

ASD_COMMAND_BUILD_STATUS
asd_sata_informational_exception_control_select(
struct asd_softc	*asd,
struct asd_device 	*dev,
struct scb		*scb,
uint8_t			*bufptr
)
{
	unsigned			*features_enabled;
	unsigned			*features_state;
	struct asd_target		*target;

	if (bufptr[1] != (INFORMATIONAL_EXCEPTION_CONTROL_PAGE_LEN - 2)) {
		return ASD_COMMAND_BUILD_FAILED;
	}

	target = dev->target;

	switch (target->command_set_type) {
	case ASD_COMMAND_SET_ATA:
		features_enabled = &target->ata_cmdset.features_enabled;
		features_state = &target->ata_cmdset.features_state;
		break;

	case ASD_COMMAND_SET_ATAPI:
		features_enabled = &target->atapi_cmdset.features_enabled;
		features_state = &target->ata_cmdset.features_state;
		break;

	default:
		return ASD_COMMAND_BUILD_FAILED;
	}

	/*
	 * Sense we only support polling and we are translating, there is
	 * nothing more to do.
	 */
	if (bufptr[2] & SCSI_DEXCPT) {
		*features_state &= ~SATA_USES_SMART;
	} else {
		*features_state |= SATA_USES_SMART;
	}

	return ASD_COMMAND_BUILD_FINISHED;
}

/* -----------------------------------
 * MODE_SENSE: (emulated)
 * MODE_SENSE_10: (emulated)
 */

ASD_COMMAND_BUILD_STATUS
asd_sata_mode_sense_build(
struct asd_softc	*asd,
struct asd_device 	*dev,
struct scb		*scb,
struct scsi_cmnd	*cmd
)
{
	struct asd_ata_task_hscb	*ata_hscb;
	unsigned			page_code;
	unsigned			page_control;
	unsigned			subpage_code;
	unsigned			allocation_length;
	unsigned			long_lba_accepted;
	uint8_t				*buffer;
	uint8_t				*bufptr;
	unsigned			transfer_length;
	unsigned			len;
	struct hd_driveid		*hd_driveidp;
	u_char				*cmd_buf_ptr;

	ata_hscb = &scb->hscb->ata_task;

	if (cmd->cmnd[1] & DBD_BIT) {
		/*
		 * We don't support disabling block discriptors.
		 */
		asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
			INVALID_COMMAND_OPERATION, 0);

		return ASD_COMMAND_BUILD_FAILED;
	}

	page_control = cmd->cmnd[2] & PAGE_CONTROL_MASK;

 	if (page_control == PAGE_CONTROL_DEFAULT ||  page_control == PAGE_CONTROL_SAVED ) {
		/*
		 * We only support the current values.
		 */
		asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
			INVALID_COMMAND_OPERATION, 0);

		return ASD_COMMAND_BUILD_FAILED;
	}

	subpage_code = cmd->cmnd[3];

	long_lba_accepted = 0;
	transfer_length = 0;
	allocation_length = 0;

	/*
	 * the first thing we need is a mode_parameter_header
	 * this depends on whether we are using 6 byte or 10 byte CDBs
	 */
	switch (cmd->cmnd[0]) {
	case MODE_SENSE:
		transfer_length = MODE_PARAMETER_HEADER_LENGTH_6;
		allocation_length = cmd->cmnd[4];
		break;
	case MODE_SENSE_10:
		transfer_length = MODE_PARAMETER_HEADER_LENGTH_10;
		long_lba_accepted = cmd->cmnd[1] & LLBA_MASK;
		allocation_length = (cmd->cmnd[7] << 8) | cmd->cmnd[8];
		break;
	}

	if (long_lba_accepted) {
		transfer_length += BLOCK_DESCRIPTOR_LENGTH_8;
	}
	else {
		transfer_length += BLOCK_DESCRIPTOR_LENGTH_16;
	}

	page_code = cmd->cmnd[2] & PAGE_CODE_MASK;

	switch (page_code) {
	case READ_WRITE_ERROR_RECOVERY_MODE_PAGE:
		transfer_length += READ_WRITE_ERROR_RECOVERY_MODE_PAGE_LEN;
		break;

	case CACHING_MODE_PAGE:
		transfer_length += CACHING_MODE_PAGE_LEN;
		break;

	case CONTROL_MODE_PAGE:
		transfer_length += CONTROL_MODE_PAGE_LEN;
		break;

	case INFORMATIONAL_EXCEPTION_CONTROL_PAGE:
		transfer_length += INFORMATIONAL_EXCEPTION_CONTROL_PAGE_LEN;
		break;

	case RETURN_ALL_PAGES:
		transfer_length += 
			READ_WRITE_ERROR_RECOVERY_MODE_PAGE_LEN + 
			CACHING_MODE_PAGE_LEN + 
			CONTROL_MODE_PAGE_LEN + 
			INFORMATIONAL_EXCEPTION_CONTROL_PAGE_LEN;
		break;
	default:
		/*
		 * We don't support disabling block discriptors.
		 */
		asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
			INVALID_COMMAND_OPERATION, 0);

		return ASD_COMMAND_BUILD_FAILED;
	}

	len = MIN(transfer_length, cmd->request_bufflen);
	len = MIN(len, allocation_length);

	/*
	 * We can't transfer more than the size of the buffer.
	 */
	cmd->request_bufflen = len;

	/*
	 * MODE_SENSE (6 byte) only supports 256 bytes of data length.  This
	 * length doesn't include
	 */
	if (cmd->cmnd[0] == MODE_SENSE) {
		/*
		 * The length doesn't include the length field itself, so we
		 * can have 256 bytes even though the length field only holds
		 * 0-255.
		 */
		if (len > 256) {
			len = 256;
		}
	}

	/*
	 * Allocate the whole length that we are going to fill in, fill the
	 * buffer up, and then transfer back what was requested.
	 */
	buffer = (uint8_t *)asd_alloc_mem(transfer_length, GFP_ATOMIC);

	if (buffer == NULL) {
		asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
			RESOURCE_FAILURE, MEMORY_OUT_OF_SPACE);

		return ASD_COMMAND_BUILD_FAILED;
	}

	memset(buffer, 0, transfer_length);

	bufptr = buffer;

	/*
	 * SPC-3 says that the medium type should be 0.
	 *
	 * MODE_SENSE:    bufptr[1] == 0
	 * MODE_SENSE_10: bufptr[2] == 0
	 */
	/*
	 * 3.10.2 and 3.11.2 of T10/04-136r0 says that DPO is ignored and that
	 * FUA is only supported for NCQ (Native Command Queuing) drives.  We
	 * are not supporting NCQ in this release, so the answer to DPOFUA will
	 * be 0 for now.
	 *
	 * MODE_SENSE:    bufptr[2] == 0
	 * MODE_SENSE_10: bufptr[3] == 0
	 */

	/*
	 * Now we can fill in the response.  Start with the mode parameter 
	 * header.
	 */
	switch (cmd->cmnd[0]) {
	case MODE_SENSE:
		bufptr[0] = len - 1;
		bufptr[3] = 8; // only one block descriptor
		bufptr += MODE_PARAMETER_HEADER_LENGTH_6;
		break;

	case MODE_SENSE_10:
		bufptr[0] = ((len - 2) >> 8) & 0xff;
		bufptr[1] = (len - 2) & 0xff;
		bufptr[6] = 0;
		if (long_lba_accepted) {
			bufptr[7] = 16; // only one block descriptor
		} else {
			bufptr[7] = 8; // only one block descriptor
		}
		bufptr += MODE_PARAMETER_HEADER_LENGTH_10;
		break;
	}

	hd_driveidp = &dev->target->ata_cmdset.adp_hd_driveid;

	if (long_lba_accepted) {
		*((uint64_t *)&bufptr[0]) =
			ATA2SCSI_8(*((uint64_t *)&hd_driveidp->lba_capacity_2));

		bufptr[8] = 0;	// density code: T10/04-136r0 - 3.21.2.1.3

		*((uint32_t *)&bufptr[12]) = ATA2SCSI_4(ATA_BLOCK_SIZE);

		bufptr += BLOCK_DESCRIPTOR_LENGTH_16;
	} else {
		*((uint32_t *)&bufptr[0]) =
			ATA2SCSI_4(*((uint64_t *)&hd_driveidp->lba_capacity_2));

		bufptr[4] = 0;	// density code: T10/04-136r0 - 3.21.2.1.3

		bufptr[5] = (ATA_BLOCK_SIZE >> 16) & 0xff;
		bufptr[6] = (ATA_BLOCK_SIZE >> 8) & 0xff;
		bufptr[7] = ATA_BLOCK_SIZE & 0xff;

		bufptr += BLOCK_DESCRIPTOR_LENGTH_8;
	}

	switch (page_code) {
	case READ_WRITE_ERROR_RECOVERY_MODE_PAGE:
		bufptr = asd_sata_read_write_error_recovery_sense(asd,
			dev, bufptr);
		break;

	case CACHING_MODE_PAGE:
 		bufptr = asd_sata_caching_sense(asd, dev, bufptr,page_control);
		break;

	case CONTROL_MODE_PAGE:
		bufptr = asd_sata_control_sense(asd, dev, bufptr);
		break;

	case INFORMATIONAL_EXCEPTION_CONTROL_PAGE:
		bufptr = asd_sata_informational_exception_control_sense(asd,
			dev, bufptr,page_control);
		break;

	case RETURN_ALL_PAGES:
		bufptr = asd_sata_read_write_error_recovery_sense(asd,
			dev, bufptr);

 		bufptr = asd_sata_caching_sense(asd, dev, bufptr,page_control);

		bufptr = asd_sata_control_sense(asd, dev, bufptr);

		bufptr = asd_sata_informational_exception_control_sense(asd,
			dev, bufptr,page_control);

		break;
	}

	cmd_buf_ptr=(u_char *) asd_sata_setup_data(asd, scb, cmd);
	memcpy(cmd_buf_ptr, buffer, len);



	asd_sata_unmap_data(asd, scb, cmd);

	asd_free_mem(buffer);

	asd_cmd_set_host_status(cmd, DID_OK);

	return ASD_COMMAND_BUILD_FINISHED;
}

uint8_t *
asd_sata_read_write_error_recovery_sense(
struct asd_softc	*asd,
struct asd_device 	*dev,
uint8_t			*bufptr
)
{
	bufptr[0] = READ_WRITE_ERROR_RECOVERY_MODE_PAGE;	// PS == 0

	bufptr[1] = READ_WRITE_ERROR_RECOVERY_MODE_PAGE_LEN - 2;
	bufptr[2] = 0xc0;	// DCR == 0,  DTE == 0, PER == 0, ERR == 0
				// RC == 0, TB == 0, ARRE == 1, AWRE == 1

	bufptr[3] = 0x00;	// READ_RETRY_COUNT == 0
	bufptr[8] = 0x00;	// WRITE_RETRY_COUNT == 0

	bufptr[10] = 0x00;	// RECOVERY_TIME_LIMIT == 0
	bufptr[11] = 0x00;

	return bufptr + bufptr[1] + 2;
}

uint8_t *
asd_sata_caching_sense(
struct asd_softc	*asd,
struct asd_device 	*dev,
uint8_t			*bufptr,
unsigned			page_control
)
{
	struct hd_driveid		*hd_driveidp;
	unsigned			features_state;
	unsigned			features_enabled;
	struct asd_target		*target;

	target = dev->target;

	switch (target->command_set_type) {
	case ASD_COMMAND_SET_ATA:
		hd_driveidp = &target->ata_cmdset.adp_hd_driveid;
		features_state = target->ata_cmdset.features_state;
		features_enabled = target->ata_cmdset.features_enabled;
		break;

	case ASD_COMMAND_SET_ATAPI:
		hd_driveidp = &target->atapi_cmdset.adp_hd_driveid;
		features_state = target->atapi_cmdset.features_state;
		features_enabled = target->ata_cmdset.features_enabled;
		break;

	default:
		return bufptr + CACHING_MODE_PAGE_LEN;
	}

	bufptr[0] = CACHING_MODE_PAGE;	// PS == 0
	bufptr[1] = CACHING_MODE_PAGE_LEN - 2;

	bufptr[2] = 0;

	if(page_control == PAGE_CONTROL_CURRENT)
	{
		if (features_state & SATA_USES_WRITE_CACHE) {
			/* 
			 * RCD == 0, MF == 0, SIZE == 0, DISC == 0,
			 * CAP == 0, ABPF == 0, IC == 0
			 */
			/*
			 * After drive reset, we should re-issue IDENTIFY.
			 */
			bufptr[2] |= SCSI_WCE;
		} else {
			bufptr[2] &= ~SCSI_WCE;
		}
	} else
	{
		if(features_enabled & WRITE_CACHE_FEATURE_ENABLED)
		{
			bufptr[2] |= SCSI_WCE;
		}
		else
		{
			bufptr[2] &= ~SCSI_WCE;
		}
	}

	bufptr[3] = 0;  // DEMAND_READ_RETENTION_PROPERTY == 0
			// WRITE_RETENTION_PROPERTY == 0

	bufptr[4] = 0;	// DISABLE_PRE_FETCH_TRANSFER_LENGTH == 0
	bufptr[5] = 0;

	bufptr[6] = 0;	// MINIMUM_PRE_FETCH == 0
	bufptr[7] = 0;

	bufptr[8] = 0;	// MAXIMUM_PRE_FETCH == 0
	bufptr[9] = 0;

	bufptr[10] = 0;	// MAXIMUM_PRE_FETCH_CEILING == 0
	bufptr[11] = 0;

	bufptr[12] = 0;

	if(page_control == PAGE_CONTROL_CURRENT)
	{
 		if ((features_state & SATA_USES_READ_AHEAD) == 0) {
			/*
			 * NV_DIS == 0, FSW == 0, LBCSS == 0, FSW == 0
			 */
			/*
			 * After drive reset, we should re-issue IDENTIFY.
			 */
			bufptr[12] |= SCSI_DRA;
		} else {
			bufptr[12] &= ~SCSI_DRA;
		}
	} else
	{

		if ((features_enabled & READ_AHEAD_FEATURE_ENABLED) == 0)
		{
			bufptr[12] |= SCSI_DRA;
		}
		else
		{
			bufptr[12] &= ~SCSI_DRA;
		}
	}

	bufptr[13] = 0;	// NUMBER_OF_CACHE_SEGMENTS == 0

	bufptr[14] = 0;	// CACHE_SEGMENT_SIZE == 0
	bufptr[15] = 0;

	bufptr[17] = 0;	// NON_CACHE_SEGMENT_SIZE == 0
	bufptr[18] = 0;
	bufptr[19] = 0;

	return bufptr + bufptr[1] + 2;
}

uint8_t *
asd_sata_control_sense(
struct asd_softc	*asd,
struct asd_device 	*dev,
uint8_t			*bufptr
)
{
	bufptr[0] = CONTROL_MODE_PAGE;
	bufptr[1] = CONTROL_MODE_PAGE_LEN - 2;
	bufptr[2] = 2;		// RLEC == 0, GLTSD == 0, D_SENSE == 0, TST == 0
	bufptr[3] = 0;		// QERR == 0, QUEUE_ALGORITHM_MODIFIER == 0
	bufptr[4] = 0;		// SWP == 0, UA_INTLCK_CTRL == 0, 
				// RAC == 0, TAS== 0
	bufptr[5] = 0;		// AUTOLOAD_MODE == 0, APTG_OWN == 0

	bufptr[8] = 0xff;
	bufptr[9] = 0xff;
	bufptr[10] = 0;
	bufptr[11] = 30;

	return bufptr + bufptr[1] + 2;
}

uint8_t *
asd_sata_informational_exception_control_sense(
struct asd_softc	*asd,
struct asd_device 	*dev,
uint8_t			*bufptr,
unsigned			page_control
)
{
	struct hd_driveid		*hd_driveidp;
	unsigned			features_state;
	unsigned			features_enabled;
	struct asd_target		*target;

	target = dev->target;

	switch (target->command_set_type) {
	case ASD_COMMAND_SET_ATA:
		hd_driveidp = &target->ata_cmdset.adp_hd_driveid;
		features_state = target->ata_cmdset.features_state;
		features_enabled = target->ata_cmdset.features_enabled;
		break;

	case ASD_COMMAND_SET_ATAPI:
		hd_driveidp = &target->atapi_cmdset.adp_hd_driveid;
		features_state = target->atapi_cmdset.features_state;
		features_enabled = target->ata_cmdset.features_enabled;
		break;

	default:
		return bufptr + INFORMATIONAL_EXCEPTION_CONTROL_PAGE_LEN;
	}

	bufptr[0] = INFORMATIONAL_EXCEPTION_CONTROL_PAGE;	// PS == 0
	bufptr[1] = INFORMATIONAL_EXCEPTION_CONTROL_PAGE_LEN - 2;

	if(page_control ==PAGE_CONTROL_CURRENT)
	{
		
		if ((features_state & SATA_USES_SMART) == 0) {
			/*
			 * LOGERR == 0, TEST == 0, EWASC == 0, EBF == 0, PERF == 0
			 */
			bufptr[2] |= SCSI_DEXCPT;		// disabled
		}
	}
	else
	{
		if (features_enabled & SMART_FEATURE_ENABLED)
		{

			/*
			 * LOGERR == 0, TEST == 0, EWASC == 0, EBF == 0, PERF == 0
			 */
			bufptr[2] |= SCSI_DEXCPT;		// enabled
		}

	}

	bufptr[3] = 0x06;	// MRIE - report on request

	bufptr[4] = 0;	// INTERVAL_TIMER == 0
	bufptr[5] = 0;
	bufptr[6] = 0;
	bufptr[7] = 0;

	bufptr[8] = 0;	// REPORT_COUNT == 0
	bufptr[9] = 0;
	bufptr[10] = 0;
	bufptr[11] = 0;

	return bufptr + bufptr[1] + 2;
}

/* -----------------------------------
 * READ_6: (translated)
 * READ_10: (translated)
 * READ_12: (translated)
 */

// RST - we need a tag somehow
ASD_COMMAND_BUILD_STATUS
asd_sata_read_build(
struct asd_softc	*asd,
struct asd_device 	*dev,
struct scb		*scb,
struct scsi_cmnd	*cmd
)
{
	struct asd_ata_task_hscb	*ata_hscb;
	uint32_t			lba;
	unsigned			sectors;
	unsigned			fis_command;

	ata_hscb = &scb->hscb->ata_task;

	switch (cmd->cmnd[0]) {
	case READ_6:
		lba = (cmd->cmnd[2] << 8) | cmd->cmnd[3];
		sectors = cmd->cmnd[4];

		break;

	case READ_10:
		if (cmd->cmnd[9] != 0) {
			asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
				INVALID_FIELD_IN_CDB, 0);
		}

		lba = (cmd->cmnd[2] << 24) | (cmd->cmnd[3] << 16) |
			(cmd->cmnd[4] << 8) | cmd->cmnd[5];
		sectors = (cmd->cmnd[7] << 8) | cmd->cmnd[8];

		break;

	case READ_12:
		if (cmd->cmnd[11] != 0) {
			asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
				INVALID_FIELD_IN_CDB, 0);
		}

		lba = (cmd->cmnd[2] << 24) | (cmd->cmnd[3] << 16) |
			(cmd->cmnd[4] << 8) | cmd->cmnd[5];
		sectors = (cmd->cmnd[6] << 24) | (cmd->cmnd[7] << 16) |
			(cmd->cmnd[8] << 8) | cmd->cmnd[9];

		break;

	default:
		lba = 0;
		sectors = 0;
		break;
	}

	switch (cmd->cmnd[0]) {
	case READ_10:
	case READ_12:
		if (cmd->cmnd[1] & 0x01) {
			/*
			 * Obsolete field that we will fail.
			 */
			asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
				INVALID_FIELD_IN_CDB, 0);

			return ASD_COMMAND_BUILD_FAILED;
		}

		if (sectors == 0) {
			asd_cmd_set_host_status(cmd, DID_OK);

			return ASD_COMMAND_BUILD_FINISHED;
		}
	}

	fis_command = 0;

	// RST - what about FUA??

	switch (dev->target->ata_cmdset.features_state & 
		(SATA_USES_DMA | SATA_USES_48BIT | SATA_USES_QUEUEING)) {
	case 0:
		fis_command = WIN_READ;
		break;
	case SATA_USES_DMA:
		fis_command = WIN_READDMA;
		break;
	case SATA_USES_48BIT:
		fis_command = WIN_READ_EXT;
		break;
	case SATA_USES_QUEUEING:
		// Doesn't exist;
		break;
	case SATA_USES_DMA | SATA_USES_48BIT:
		fis_command = WIN_READDMA_EXT;
		break;
	case SATA_USES_DMA | SATA_USES_QUEUEING:
		// RST - sector and feature are swapped for this command
		fis_command = WIN_READDMA_QUEUED;
		break;
	case SATA_USES_48BIT | SATA_USES_QUEUEING:
		// Doesn't exist
		break;
	case SATA_USES_DMA | SATA_USES_48BIT | SATA_USES_QUEUEING:
		// RST - sector and feature are swapped for this command
		fis_command = WIN_READDMA_QUEUED_EXT;
		break;
	default:
		asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
			INVALID_FIELD_IN_CDB, 0);
		return ASD_COMMAND_BUILD_FAILED;
	}

	asd_sata_setup_fis(ata_hscb, fis_command);

	if (dev->target->ata_cmdset.features_state & SATA_USES_DMA) {
		SET_DMA_MODE(ata_hscb);
	} else {
		SET_PIO_MODE(ata_hscb);
	}

	if (dev->target->ata_cmdset.features_state & SATA_USES_48BIT) {

		asd_sata_setup_lba_ext(ata_hscb, lba, sectors);

	} else {
		if (lba >= RW_DMA_LBA_SIZE) {
			asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
				INVALID_FIELD_IN_CDB, 0);

			return ASD_COMMAND_BUILD_FAILED;
		}

		if (sectors > RW_DMA_MAX_SECTORS) {
			asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
				INVALID_FIELD_IN_CDB, 0);

			return ASD_COMMAND_BUILD_FAILED;
		}

		if (sectors == RW_DMA_MAX_SECTORS) {
			sectors = 0;
		}

		asd_sata_setup_lba(ata_hscb, lba, sectors);
	}

	ata_hscb->ata_flags |= DATA_DIR_INBOUND;

	asd_push_post_stack(asd, scb, (void *)cmd, asd_sata_read_post);

	return ASD_COMMAND_BUILD_OK;
}

void
asd_sata_read_post(
struct asd_softc	*asd,
struct scb		*scb,
struct asd_done_list	*done_listp
)
{
	struct ata_resp_edb		*ata_resp_edbp;
	struct scsi_cmnd		*cmd;
#if 0
	unsigned			sectors;
	uint32_t			lba;
	struct asd_ata_task_hscb	*ata_hscb;
	unsigned			remain;
#endif
	u_int			 edb_index;
	struct scb 		 *escb;

	cmd = (struct scsi_cmnd *)scb->io_ctx;

#if 0
	ata_hscb = &scb->hscb->ata_task;
#endif


	switch (done_listp->opcode) {
	case ATA_TASK_COMP_W_RESP:
		ata_resp_edbp = asd_sata_get_edb(asd, done_listp,&escb, &edb_index);
		asd_sata_check_registers(ata_resp_edbp, scb, cmd);
		asd_hwi_free_edb(asd, escb, edb_index);
		asd_pop_post_stack(asd, scb, done_listp);
		return;

	case TASK_COMP_WO_ERR:
		break;

	default:
		asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
				INVALID_COMMAND_OPERATION, 0);

		asd_pop_post_stack(asd, scb, done_listp);
		return;

	}

#if 0
	if (remain > 0) {

		sectors = remain / RW_DMA_MAX_SECTORS ;

		if (sectors >= RW_DMA_MAX_SECTORS) {
			scb->transfer_length = RW_DMA_MAX_SECTORS *
				ATA_BLOCK_SIZE;

			sectors = 0;
		} else {

			scb->transfer_length = remain;
		}


		lba = (ASD_H2D_FIS(ata_hscb)->lba2  << 16) |
			(ASD_H2D_FIS(ata_hscb)->lba1 << 8) |
			(ASD_H2D_FIS(ata_hscb)->lba0);

		lba = lba + sectors;

		printk("%s:%d: finishing request lba 0x%x sectors 0x%x\n",
			__FUNCTION__, __LINE__,
			lba, sectors);

		asd_sata_setup_lba(ata_hscb, lba, sectors);

		asd_hwi_post_scb(asd, scb);

		return;
	}
#endif

	asd_cmd_set_host_status(cmd, DID_OK);

	asd_pop_post_stack(asd, scb, done_listp);
}

/* -----------------------------------
 * WRITE_BUFFER: (translated)
 */

ASD_COMMAND_BUILD_STATUS
asd_sata_write_buffer_build(
struct asd_softc	*asd,
struct asd_device 	*dev,
struct scb		*scb,
struct scsi_cmnd	*cmd
)
{
	struct asd_ata_task_hscb	*ata_hscb;
	unsigned			buffer_offset;
	unsigned			len;
	unsigned			allocation_length;

	ata_hscb = &scb->hscb->ata_task;

	if ((dev->target->ata_cmdset.features_state & 
		SATA_USES_WRITE_BUFFER) == 0) {

		asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
			INVALID_COMMAND_OPERATION, 0);

		return ASD_COMMAND_BUILD_FAILED;
	}

	if ((cmd->cmnd[1] & BUFFER_MODE_MASK) != DATA_ONLY_MODE) {
		asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
			INVALID_FIELD_IN_CDB, 0);

		return ASD_COMMAND_BUILD_FAILED;
	}

	/*
	 * Only support Buffer ID of 0
	 */
	if (cmd->cmnd[2] != 0) {
		asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
			INVALID_FIELD_IN_CDB, 0);

		return ASD_COMMAND_BUILD_FAILED;
	}

	buffer_offset = 
		(cmd->cmnd[3] << 16) | (cmd->cmnd[4] << 8) | cmd->cmnd[5];

	if (buffer_offset >= ATA_BUFFER_SIZE) {
		asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
			INVALID_FIELD_IN_CDB, 0);

		return ASD_COMMAND_BUILD_FAILED;
	}

	allocation_length = 
		(cmd->cmnd[6] << 16) | (cmd->cmnd[7] << 8) | cmd->cmnd[8];

	len = MIN(ATA_BUFFER_SIZE - buffer_offset, cmd->request_bufflen);
	len = MIN(len, allocation_length);

	/*
	 * We can't transfer more than the size of the buffer.
	 */
	cmd->request_bufflen = len;

	asd_sata_setup_fis(ata_hscb, WIN_WRITE_BUFFER);

	ata_hscb->ata_flags |= DATA_DIR_OUTBOUND;
	SET_PIO_MODE(ata_hscb);

	asd_push_post_stack(asd, scb, (void *)cmd, asd_sata_write_buffer_post);

	return ASD_COMMAND_BUILD_OK;
}

void
asd_sata_write_buffer_post(
struct asd_softc	*asd,
struct scb		*scb,
struct asd_done_list	*done_listp
)
{
	struct ata_resp_edb	*ata_resp_edbp;
	struct scsi_cmnd	*cmd;
	u_int			 edb_index;
	struct scb 		 *escb;

	cmd = (struct scsi_cmnd *)scb->io_ctx;

	switch (done_listp->opcode) {
	case ATA_TASK_COMP_W_RESP:
		ata_resp_edbp = asd_sata_get_edb(asd, done_listp,&escb, &edb_index);
		asd_sata_check_registers(ata_resp_edbp, scb, cmd);
		asd_hwi_free_edb(asd, escb, edb_index);
		asd_pop_post_stack(asd, scb, done_listp);
		return;

	case TASK_COMP_WO_ERR:
		asd_cmd_set_host_status(cmd, DID_OK);
		asd_pop_post_stack(asd, scb, done_listp);
		return;

	default:
		break;
	}

	asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
			INVALID_COMMAND_OPERATION, 0);

	asd_pop_post_stack(asd, scb, done_listp);
}

/* -----------------------------------
 * READ_BUFFER: (translated)
 */

ASD_COMMAND_BUILD_STATUS
asd_sata_read_buffer_build(
struct asd_softc	*asd,
struct asd_device 	*dev,
struct scb		*scb,
struct scsi_cmnd	*cmd
)
{
	struct asd_ata_task_hscb	*ata_hscb;
	uint8_t				*read_buffer_descriptor;
	unsigned			buffer_offset;
	unsigned			allocation_length;
	unsigned			len;

	ata_hscb = &scb->hscb->ata_task;

	if ((dev->target->ata_cmdset.features_state & 
		SATA_USES_READ_BUFFER) == 0) {

		asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
			INVALID_COMMAND_OPERATION, 0);

		return ASD_COMMAND_BUILD_FAILED;
	}

	if (cmd->cmnd[2] != 0) {
		/*
		 * We are only supporting 1 buffer ID (== 0).
		 */
		asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
			INVALID_FIELD_IN_CDB, 0);

		return ASD_COMMAND_BUILD_FAILED;
	}

	allocation_length = 0;

	if ((cmd->cmnd[1] & BUFFER_MODE_MASK) == DESCRIPTOR_MODE) {

		if (cmd->request_bufflen < READ_BUFFER_DESCRIPTOR_LENGTH) {

			asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
				INVALID_FIELD_IN_CDB, 0);

			return ASD_COMMAND_BUILD_FAILED;
		}

	read_buffer_descriptor = (uint8_t *)asd_sata_setup_data(asd, scb, cmd);
 		memset(read_buffer_descriptor, 0, 
			READ_BUFFER_DESCRIPTOR_LENGTH);
	asd_sata_unmap_data(asd, scb, cmd);



		/*
		 * ATA only supports ATA_BUFFER_SIZE byte buffer writes.
		 */
		read_buffer_descriptor[0] = 0;
		read_buffer_descriptor[1] = (ATA_BUFFER_SIZE >> 16) & 0xff;
		read_buffer_descriptor[2] = (ATA_BUFFER_SIZE >> 8) & 0xff;
		read_buffer_descriptor[3] = ATA_BUFFER_SIZE & 0xff;

		asd_cmd_set_host_status(cmd, DID_OK);

		return ASD_COMMAND_BUILD_FINISHED;
	}

	if ((cmd->cmnd[1] & BUFFER_MODE_MASK) != DATA_ONLY_MODE) {
		asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
			INVALID_FIELD_IN_CDB, 0);

		return ASD_COMMAND_BUILD_FAILED;
	}

	buffer_offset = 
		(cmd->cmnd[3] << 16) | (cmd->cmnd[4] << 8) | cmd->cmnd[5];

	if (buffer_offset >= ATA_BUFFER_SIZE) {
		asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
			INVALID_FIELD_IN_CDB, 0);

		return ASD_COMMAND_BUILD_FAILED;
	}

	len = MIN(ATA_BUFFER_SIZE - buffer_offset, cmd->request_bufflen);
	len = MIN(len, allocation_length);

	/*
	 * We can't transfer more than the size of the buffer.
	 */
	cmd->request_bufflen = len;

	asd_sata_setup_fis(ata_hscb, WIN_READ_BUFFER);

	ata_hscb->ata_flags |= DATA_DIR_INBOUND;
	SET_PIO_MODE(ata_hscb);

	asd_push_post_stack(asd, scb, (void *)cmd, asd_sata_read_buffer_post);

	return ASD_COMMAND_BUILD_OK;
}

void
asd_sata_read_buffer_post(
struct asd_softc	*asd,
struct scb		*scb,
struct asd_done_list	*done_listp
)
{
	struct ata_resp_edb	*ata_resp_edbp;
	struct scsi_cmnd	*cmd;
	u_int			 edb_index;
	struct scb 		 *escb;

	cmd = (struct scsi_cmnd *)scb->io_ctx;

	switch (done_listp->opcode) {
	case ATA_TASK_COMP_W_RESP:
		ata_resp_edbp = asd_sata_get_edb(asd, done_listp,&escb, &edb_index);
		asd_sata_check_registers(ata_resp_edbp, scb, cmd);
		asd_hwi_free_edb(asd, escb, edb_index);
		asd_pop_post_stack(asd, scb, done_listp);
		return;

	case TASK_COMP_WO_ERR:
		asd_cmd_set_host_status(cmd, DID_OK);
		asd_pop_post_stack(asd, scb, done_listp);
		return;

	default:
		break;
	}

	asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
			INVALID_COMMAND_OPERATION, 0);

	asd_pop_post_stack(asd, scb, done_listp);
}

/* -----------------------------------
 * READ_CAPACITY: (emulated)
 */

ASD_COMMAND_BUILD_STATUS
asd_sata_read_capacity_build(
struct asd_softc	*asd,
struct asd_device 	*dev,
struct scb		*scb,
struct scsi_cmnd	*cmd
)
{
	struct asd_ata_task_hscb	*ata_hscb;
	struct hd_driveid		*hd_driveidp;
	u_char				*read_capacity_data;
	uint64_t			lba_capacity;

	ata_hscb = &scb->hscb->ata_task;

	hd_driveidp = &dev->target->ata_cmdset.adp_hd_driveid;

	if (cmd->request_bufflen < READ_CAPACITY_DATA_LEN) {
		asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
			INVALID_FIELD_IN_CDB, 0);

		return ASD_COMMAND_BUILD_FAILED;
	}

	read_capacity_data = (u_char *)asd_sata_setup_data(asd, scb, cmd);

	lba_capacity = *((uint64_t *)&hd_driveidp->lba_capacity_2);

	if (dev->target->ata_cmdset.features_state & SATA_USES_48BIT) {

		lba_capacity = *((uint64_t *)&hd_driveidp->lba_capacity_2);
	}

	/*
	 * Ignore the Logical Block Address field and the PMI bit
	 */
	if (lba_capacity == 0) {
		lba_capacity = *((uint32_t *)&hd_driveidp->lba_capacity);
	}

	if (lba_capacity == 0) {
		lba_capacity = hd_driveidp->cyls * hd_driveidp->heads *
			hd_driveidp->sectors;
	}

	*((uint32_t *)&read_capacity_data[0]) = ATA2SCSI_4(lba_capacity - 1);

	*((uint32_t *)&read_capacity_data[4]) = ATA2SCSI_4(ATA_BLOCK_SIZE);
 	asd_sata_unmap_data(asd, scb, cmd);

	asd_cmd_set_host_status(cmd, DID_OK);

	return ASD_COMMAND_BUILD_FINISHED;
}

/* -----------------------------------
 * REPORT_LUNS: (emulated)
 */

ASD_COMMAND_BUILD_STATUS
asd_sata_report_luns_build(
struct asd_softc	*asd,
struct asd_device 	*dev,
struct scb		*scb,
struct scsi_cmnd	*cmd
)
{
	u_char		report_luns_data[REPORT_LUNS_SIZE];
	unsigned	len;
	u_char		*cmd_buf_ptr;

	memset(report_luns_data, 0, REPORT_LUNS_SIZE);

	*((uint32_t *)&report_luns_data[0]) = asd_htobe32(8);

	len = MIN(cmd->request_bufflen, REPORT_LUNS_SIZE);

	cmd_buf_ptr = (u_char *)asd_sata_setup_data(asd, scb, cmd);
	memcpy(cmd_buf_ptr, &report_luns_data[0], len);
	asd_sata_unmap_data(asd, scb, cmd);


	asd_cmd_set_host_status(cmd, DID_OK);

	return ASD_COMMAND_BUILD_FINISHED;
}

/* -----------------------------------
 * REQUEST_SENSE: (emulated)
 */

ASD_COMMAND_BUILD_STATUS
asd_sata_request_sense_build(
struct asd_softc	*asd,
struct asd_device 	*dev,
struct scb		*scb,
struct scsi_cmnd	*cmd
)
{
	asd_cmd_set_host_status(cmd, DID_OK);

	return ASD_COMMAND_BUILD_FINISHED;
}

/* -----------------------------------
 * REZERO_UNIT: (emulated)
 */

ASD_COMMAND_BUILD_STATUS
asd_sata_rezero_unit_build(
struct asd_softc	*asd,
struct asd_device	*dev,
struct scb		*scb,
struct scsi_cmnd	*cmd
)
{
	asd_cmd_set_host_status(cmd, DID_OK);

	return ASD_COMMAND_BUILD_FINISHED;
}

/* -----------------------------------
 * SEEK_6: (emulated)
 * SEEK_10: (emulated)
 */

ASD_COMMAND_BUILD_STATUS
asd_sata_seek_build(
struct asd_softc	*asd,
struct asd_device	*dev,
struct scb		*scb,
struct scsi_cmnd	*cmd
)
{
	switch (cmd->cmnd[0]) {
	case SEEK_6:
	case SEEK_10:
		break;
	}

	asd_cmd_set_host_status(cmd, DID_OK);

	return ASD_COMMAND_BUILD_FINISHED;
}

/* -----------------------------------
 * SEND_DIAGNOSTIC: (translated)
 */

ASD_COMMAND_BUILD_STATUS
asd_sata_send_diagnostic_build(
struct asd_softc	*asd,
struct asd_device	*dev,
struct scb		*scb,
struct scsi_cmnd	*cmd
)
{
	struct asd_ata_task_hscb	*ata_hscb;

	ata_hscb = &scb->hscb->ata_task;

	/*
	 * "Execute Device Diagnostic"
	 */
	asd_sata_setup_fis(ata_hscb, WIN_DIAGNOSE);

	ata_hscb->ata_flags |= DATA_DIR_NO_XFER;
	SET_NO_IO_MODE(ata_hscb);

	asd_push_post_stack(asd, scb, (void *)cmd,
		asd_sata_send_diagnostic_post);

	if ((cmd->cmnd[1] & SELFTEST) == 0) {

		asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
				INVALID_COMMAND_OPERATION, 0);

		return ASD_COMMAND_BUILD_FAILED;
	}

	if ((cmd->cmnd[1] & SELFTEST_CODE_MASK) != 0) {

		asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
				INVALID_COMMAND_OPERATION, 0);

		return ASD_COMMAND_BUILD_FAILED;
	}

	return ASD_COMMAND_BUILD_OK;
}

void
asd_sata_send_diagnostic_post(
struct asd_softc	*asd,
struct scb		*scb,
struct asd_done_list	*done_listp
)
{
	struct ata_resp_edb	*ata_resp_edbp=NULL;
	struct scsi_cmnd	*cmd;
	COMMAND_SET_TYPE	command_set_type;
	struct asd_device	*dev;
	unsigned		error;
	u_int			 edb_index;
	struct scb 		 *escb;

	cmd = (struct scsi_cmnd *)scb->io_ctx;

	dev = scb->platform_data->dev;

	switch (done_listp->opcode) {
	case TASK_COMP_WO_ERR:
		break;
	case ATA_TASK_COMP_W_RESP:
		ata_resp_edbp = asd_sata_get_edb(asd, done_listp,&escb, &edb_index);
		asd_sata_check_registers(ata_resp_edbp, scb, cmd);
		asd_hwi_free_edb(asd, escb, edb_index);
		asd_pop_post_stack(asd, scb, done_listp);
		return;
	default:
		asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
				INVALID_COMMAND_OPERATION, 0);
		asd_pop_post_stack(asd, scb, done_listp);
		return;
	}

	if(ata_resp_edbp !=NULL)
	{
		command_set_type = asd_sata_get_type(ASD_D2H_FIS(ata_resp_edbp));
	}
	else
	{
		command_set_type=ASD_COMMAND_SET_UNKNOWN;
	}

	if (command_set_type != dev->target->command_set_type) {
		/*
		 * Signatures don't match.
		 */
		asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
				INVALID_COMMAND_OPERATION, 0);
		asd_pop_post_stack(asd, scb, done_listp);
		return;
	}

	asd_pop_post_stack(asd, scb, done_listp);

	error = ASD_D2H_FIS(ata_resp_edbp)->error;
	
	if (error == 0x01) {
		/*
		 * device 0 passed
		 */
		asd_cmd_set_host_status(cmd, DID_OK);
		asd_pop_post_stack(asd, scb, done_listp);
		return;
	}

	if ((error == 0x00) || (error > 0x02)) {
		/*
		 * The device failed internal tests.
		 */
		asd_sata_set_check_condition(cmd, HARDWARE_ERROR,
			LOGICAL_UNIT_FAILURE, FAILED_SELF_TEST);

		asd_pop_post_stack(asd, scb, done_listp);

		return;
	}

	asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
			INVALID_COMMAND_OPERATION, 0);

	asd_pop_post_stack(asd, scb, done_listp);
}

/* -----------------------------------
 * START_STOP: (translated)
 */

ASD_COMMAND_BUILD_STATUS
asd_sata_start_stop_build(
struct asd_softc	*asd,
struct asd_device	*dev,
struct scb		*scb,
struct scsi_cmnd	*cmd
)
{
	struct asd_ata_task_hscb	*ata_hscb;

	ata_hscb = &scb->hscb->ata_task;

	if (cmd->cmnd[4] & START_STOP_LOEJ) {
		asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
			INVALID_FIELD_IN_CDB, 0);

		return ASD_COMMAND_BUILD_FAILED;
	}

	if (cmd->cmnd[4] & START_STOP_START) {
		asd_sata_setup_fis(ata_hscb, WIN_IDLEIMMEDIATE);
	} else {
		/*
		 * "Standby Immediate"
		 */
		asd_sata_setup_fis(ata_hscb, WIN_STANDBYNOW1);
	}

	ata_hscb->ata_flags |= DATA_DIR_NO_XFER;
	SET_NO_IO_MODE(ata_hscb);

	asd_push_post_stack(asd, scb, (void *)cmd, asd_sata_start_stop_post);

	return ASD_COMMAND_BUILD_OK;
}

void
asd_sata_start_stop_post(
struct asd_softc	*asd,
struct scb		*scb,
struct asd_done_list	*done_listp
)
{
	struct ata_resp_edb	*ata_resp_edbp;
	struct scsi_cmnd	*cmd;
	u_int			 edb_index;
	struct scb 		 *escb;

	cmd = (struct scsi_cmnd *)scb->io_ctx;

	switch (done_listp->opcode) {
	case ATA_TASK_COMP_W_RESP:
		ata_resp_edbp = asd_sata_get_edb(asd, done_listp,&escb, &edb_index);
		asd_sata_check_registers(ata_resp_edbp, scb, cmd);
		asd_hwi_free_edb(asd, escb, edb_index);
		asd_pop_post_stack(asd, scb, done_listp);
		return;

	case TASK_COMP_WO_ERR:
		asd_cmd_set_host_status(cmd, DID_OK);
		asd_pop_post_stack(asd, scb, done_listp);
		return;

	default:
		break;
	}

	asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
			INVALID_COMMAND_OPERATION, 0);

	asd_pop_post_stack(asd, scb, done_listp);
}

/* -----------------------------------
 * SYNCHRONIZE_CACHE: (translated)
 */

ASD_COMMAND_BUILD_STATUS
asd_sata_synchronize_cache_build(
struct asd_softc	*asd,
struct asd_device	*dev,
struct scb		*scb,
struct scsi_cmnd	*cmd
)
{
	struct asd_ata_task_hscb	*ata_hscb;

	ata_hscb = &scb->hscb->ata_task;

	asd_sata_setup_fis(ata_hscb, WIN_FLUSH_CACHE);

	ata_hscb->ata_flags |= DATA_DIR_NO_XFER;
	SET_NO_IO_MODE(ata_hscb);

	asd_push_post_stack(asd, scb, (void *)cmd,
		asd_sata_synchronize_cache_post);

	return ASD_COMMAND_BUILD_OK;
}

void
asd_sata_synchronize_cache_post(
struct asd_softc	*asd,
struct scb		*scb,
struct asd_done_list	*done_listp
)
{
	struct ata_resp_edb	*ata_resp_edbp;
	struct scsi_cmnd	*cmd;
	u_int			 edb_index;
	struct scb 		 *escb;

	cmd = (struct scsi_cmnd *)scb->io_ctx;

	switch (done_listp->opcode) {
	case ATA_TASK_COMP_W_RESP:
		ata_resp_edbp = asd_sata_get_edb(asd, done_listp,&escb, &edb_index);
		asd_sata_check_registers(ata_resp_edbp, scb, cmd);
		asd_hwi_free_edb(asd, escb, edb_index);
		asd_pop_post_stack(asd, scb, done_listp);
		return;

	case TASK_COMP_WO_ERR:
		asd_cmd_set_host_status(cmd, DID_OK);
		asd_pop_post_stack(asd, scb, done_listp);
		return;

	default:
		break;
	}

	asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
			INVALID_COMMAND_OPERATION, 0);

	asd_pop_post_stack(asd, scb, done_listp);
}

/* -----------------------------------
 * TEST_UNIT_READY: (translated)
 */

ASD_COMMAND_BUILD_STATUS
asd_sata_test_unit_ready_build(
struct asd_softc	*asd,
struct asd_device	*dev,
struct scb		*scb,
struct scsi_cmnd	*cmd
)
{
	struct asd_ata_task_hscb	*ata_hscb;
	struct hd_driveid		*hd_driveidp;

	ata_hscb = &scb->hscb->ata_task;

	hd_driveidp = &dev->target->ata_cmdset.adp_hd_driveid;

	if ((hd_driveidp->command_set_1 & POWER_MANAGEMENT_SUPPORTED) == 0) {

		asd_cmd_set_host_status(cmd, DID_OK);

		return ASD_COMMAND_BUILD_FINISHED;
	}

	asd_sata_setup_fis(ata_hscb, WIN_CHECKPOWERMODE1);

	ata_hscb->ata_flags |= DATA_DIR_NO_XFER;
	SET_NO_IO_MODE(ata_hscb);

	asd_push_post_stack(asd, scb, (void *)cmd,
		asd_sata_test_unit_ready_post);

	return ASD_COMMAND_BUILD_OK;
}


void
asd_sata_test_unit_ready_post(
struct asd_softc	*asd,
struct scb		*scb,
struct asd_done_list	*done_listp
)
{
	struct ata_resp_edb		*ata_resp_edbp;
	struct scsi_cmnd		*cmd;
	struct asd_ata_task_hscb	*ata_hscb;
	u_int			 edb_index;
	struct scb 		 *escb;

	cmd = (struct scsi_cmnd *)scb->io_ctx;

	switch (done_listp->opcode) {
	case TASK_COMP_WO_ERR:
		break;
	case ATA_TASK_COMP_W_RESP:
		ata_resp_edbp = asd_sata_get_edb(asd, done_listp,&escb, &edb_index);
		asd_sata_check_registers(ata_resp_edbp, scb, cmd);
		asd_hwi_free_edb(asd, escb, edb_index);
		asd_pop_post_stack(asd, scb, done_listp);
		return;
	default:
		asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
				INVALID_COMMAND_OPERATION, 0);
		asd_pop_post_stack(asd, scb, done_listp);
		return;
	}

	ata_hscb = &scb->hscb->ata_task;

	switch (ASD_H2D_FIS(ata_hscb)->sector_count) {
#if 0
	// RST - this doesn't seem to work
	case ATA_STANDBY_MODE:
		asd_sata_set_check_condition(cmd, NOT_READY,
			LOGICAL_UNIT_NOT_READY, NOTIFY_REQUIRED);
		break;
#endif

	default:
	case ATA_IDLE_MODE:
	case ATA_ACTIVE:
		asd_cmd_set_host_status(cmd, DID_OK);
		break;
	}

	asd_pop_post_stack(asd, scb, done_listp);
}

#ifdef T10_04_136
/* -----------------------------------
 * VERIFY: (translated)
 */

ASD_COMMAND_BUILD_STATUS
asd_sata_verify_build(
struct asd_softc	*asd,
struct asd_device	*dev,
struct scb		*scb,
struct scsi_cmnd	*cmd
)
{
	struct asd_ata_task_hscb	*ata_hscb;

	ata_hscb = &scb->hscb->ata_task;

	if (target->ata_cmdset.features_state & SATA_USES_48BIT) {
		asd_sata_setup_fis(ata_hscb, WIN_VERIFY_EXT);
	} else {
		asd_sata_setup_fis(ata_hscb, WIN_VERIFY);
	}

	ata_hscb->ata_flags |= DATA_DIR_NO_XFER;
	SET_NO_IO_MODE(ata_hscb);

	asd_push_post_stack(asd, scb, (void *)acmd,
		asd_sata_verify_post);

	return ASD_COMMAND_BUILD_OK;
}

void
asd_sata_verify_post(
struct asd_softc	*asd,
struct scb		*scb,
struct asd_done_list	*done_listp
)
{
	// RST - fill this in
	asd_pop_post_stack(asd, scb, done_listp);
}
#endif


/* -----------------------------------
 * WRITE_6: (translated)
 * WRITE_10: (translated)
 * WRITE_12: (translated)
 */

// RST - we need a tag somehow
ASD_COMMAND_BUILD_STATUS
asd_sata_write_build(
struct asd_softc	*asd,
struct asd_device	*dev,
struct scb		*scb,
struct scsi_cmnd	*cmd
)
{
	struct asd_ata_task_hscb	*ata_hscb;
	uint32_t			lba;
	unsigned			sectors;
	unsigned			fis_command;
	unsigned			fua_bit;

	ata_hscb = &scb->hscb->ata_task;

	switch (cmd->cmnd[0]) {
	case WRITE_6:
		lba = (cmd->cmnd[2] << 8) | cmd->cmnd[3];
		sectors = cmd->cmnd[4];

		break;

	case WRITE_10:
		lba = (cmd->cmnd[2] << 24) | (cmd->cmnd[3] << 16) |
			(cmd->cmnd[4] << 8) | cmd->cmnd[5];
		sectors = (cmd->cmnd[7] << 8) | cmd->cmnd[8];

		break;

	case WRITE_12:
		lba = (cmd->cmnd[2] << 24) | (cmd->cmnd[3] << 16) |
			(cmd->cmnd[4] << 8) | cmd->cmnd[5];
		sectors = (cmd->cmnd[6] << 24) | (cmd->cmnd[7] << 16) |
			(cmd->cmnd[8] << 8) | cmd->cmnd[9];

		break;
	default:
		lba = 0;
		sectors = 0;
	}

	fua_bit = 0;

	switch (cmd->cmnd[0]) {
	case WRITE_10:
	case WRITE_12:
		if (cmd->cmnd[1] & 0x01) {
			/*
			 * Obsolete field that we will fail.
			 */
			asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
				INVALID_FIELD_IN_CDB, 0);

			return ASD_COMMAND_BUILD_FAILED;
		}

		if (sectors == 0) {
			asd_cmd_set_host_status(cmd, DID_OK);

			return ASD_COMMAND_BUILD_FINISHED;
		}

		if ((dev->target->ata_cmdset.features_state & 
			SATA_USES_WRITE_FUA) != 0) {

			fua_bit = cmd->cmnd[1] & SCSI_WRITE_FUA_BIT;
		}
		break;
	}

	fis_command = 0;

	switch (dev->target->ata_cmdset.features_state & 
		(SATA_USES_DMA | SATA_USES_48BIT | SATA_USES_QUEUEING)) {
	case 0:
		fis_command = WIN_WRITE;
		break;
	case SATA_USES_DMA:
		fis_command = WIN_WRITEDMA;
		break;
	case SATA_USES_48BIT:
		fis_command = WIN_WRITE_EXT;
		break;
	case SATA_USES_QUEUEING:
		// Doesn't exist;
		break;
	case SATA_USES_DMA | SATA_USES_48BIT:
		if (fua_bit) {
			fis_command = WIN_WRITE_DMA_FUA_EXT;
		} else {
			fis_command = WIN_WRITEDMA_EXT;
		}
		break;
	case SATA_USES_DMA | SATA_USES_QUEUEING:
		fis_command = WIN_WRITEDMA_QUEUED;
		break;
	case SATA_USES_48BIT | SATA_USES_QUEUEING:
		// Doesn't exist
		break;
	case SATA_USES_DMA | SATA_USES_48BIT | SATA_USES_QUEUEING:
		if (fua_bit) {
			fis_command = WIN_WRITE_DMA_QUEUED_FUA_EXT;
		} else {
			fis_command = WIN_WRITEDMA_QUEUED_EXT;
		}
		break;
	default:
		fis_command = 0;
		asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
			INVALID_FIELD_IN_CDB, 0);
		return ASD_COMMAND_BUILD_FAILED;
	}

	asd_sata_setup_fis(ata_hscb, fis_command);

	if (dev->target->ata_cmdset.features_state & SATA_USES_DMA) {
		SET_DMA_MODE(ata_hscb);
	} else {
		SET_PIO_MODE(ata_hscb);
	}

	if (dev->target->ata_cmdset.features_state & SATA_USES_48BIT) {

		asd_sata_setup_lba_ext(ata_hscb, lba, sectors);

	} else {
		if (lba >= RW_DMA_LBA_SIZE) {

			asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
				INVALID_FIELD_IN_CDB, 0);

			return ASD_COMMAND_BUILD_FAILED;
		}

		if (sectors > RW_DMA_MAX_SECTORS) {
			asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
				INVALID_FIELD_IN_CDB, 0);

			return ASD_COMMAND_BUILD_FAILED;
		}

		if (sectors == RW_DMA_MAX_SECTORS) {
			sectors = 0;
		}

		asd_sata_setup_lba(ata_hscb, lba, sectors);
	}

	ata_hscb->ata_flags |= DATA_DIR_OUTBOUND;

	asd_push_post_stack(asd, scb, (void *)cmd,
		asd_sata_write_post);

	return ASD_COMMAND_BUILD_OK;
}

void
asd_sata_write_post(
struct asd_softc	*asd,
struct scb		*scb,
struct asd_done_list	*done_listp
)
{
	struct ata_resp_edb		*ata_resp_edbp;
	struct scsi_cmnd		*cmd;
#if 0
	uint32_t			lba;
	struct asd_ata_task_hscb	*ata_hscb;
	unsigned			sectors;
#endif
	u_int			 edb_index;
	struct scb 		 *escb;

	cmd = (struct scsi_cmnd *)scb->io_ctx;

#if 0
	ata_hscb = &scb->hscb->ata_task;
#endif

	switch (done_listp->opcode) {
	case ATA_TASK_COMP_W_RESP:
		ata_resp_edbp = asd_sata_get_edb(asd, done_listp,&escb, &edb_index);
		asd_sata_check_registers(ata_resp_edbp, scb, cmd);
		asd_hwi_free_edb(asd, escb, edb_index);
		asd_pop_post_stack(asd, scb, done_listp);
		return;

	case TASK_COMP_WO_ERR:
		break;

	default:
		asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
				INVALID_COMMAND_OPERATION, 0);

		asd_pop_post_stack(asd, scb, done_listp);

		return;
	}

#if 0
	if (scb->sectors_remaining != 0) {

		sectors = scb->sectors_remaining;

		if (sectors >= RW_DMA_MAX_SECTORS) {
			scb->sectors_remaining = 
				scb->sectors_remaining - RW_DMA_MAX_SECTORS;

			sectors = 0;
		} else {

			scb->sectors_remaining = 0;
		}

		lba = (ASD_H2D_FIS(ata_hscb)->lba2  << 16) |
			(ASD_H2D_FIS(ata_hscb)->lba1 << 8) |
			(ASD_H2D_FIS(ata_hscb)->lba0);

		lba = lba + sectors;

		printk("%s:%d: finishing request lba 0x%x sectors 0x%x "
			"remaining 0x%x\n",
			__FUNCTION__, __LINE__,
			lba, sectors, scb->sectors_remaining);

		asd_sata_setup_lba(ata_hscb, lba, sectors);

		ret = asd_setup_data(asd, scb, cmd);

		asd_hwi_post_scb(asd, scb);


		return;
	}
#endif

	asd_cmd_set_host_status(cmd, DID_OK);

	asd_pop_post_stack(asd, scb, done_listp);

	return;
}

#ifdef T10_04_136
/* -----------------------------------
 * WRITE_VERIFY: (translated)
 */

ASD_COMMAND_BUILD_STATUS
asd_sata_write_verify_build(
struct asd_softc	*asd,
struct asd_device 	*dev,
struct scb		*scb,
struct scsi_cmnd	*cmd
)
{
	struct asd_ata_task_hscb	*ata_hscb;
	uint32_t			lba;
	unsigned			sectors;

	ata_hscb = &scb->hscb->ata_task;

	lba = (cmd->cmnd[2] << 24) | (cmd->cmnd[3] << 16) |
		(cmd->cmnd[4] << 8) | cmd->cmnd[5];

	sectors = (cmd->cmnd[7] << 8) | cmd->cmnd[8];

	// RST - this should be a WRITE with FUA followed by a read verify.
	// ... we will want to set affiliation bits in the ata_hscb

	if (target->ata_cmdset.features_state & SATA_USES_48BIT) {
		/*
		 * Read Verify Sectors
		 */
		asd_sata_setup_fis(ata_hscb, WIN_VERIFY_EXT);
	} else {
		if (sectors > RW_DMA_MAX_SECTORS) {
			asd_sata_set_check_condition(cmd, ILLEGAL_REQUEST,
				INVALID_FIELD_IN_CDB, 0);

			return ASD_COMMAND_BUILD_FAILED;
		}

		if (sectors == RW_DMA_MAX_SECTORS) {
			sectors = 0;
		}

		asd_sata_setup_fis(ata_hscb, WIN_VERIFY);
	}

	ata_hscb->ata_flags |= DATA_DIR_NO_XFER;
	SET_NO_IO_MODE(ata_hscb);

	asd_push_post_stack(asd, scb, (void *)acmd,
		asd_sata_write_verify_post);

	return ASD_COMMAND_BUILD_OK;
}

void
asd_sata_write_verify_post(
struct asd_softc	*asd,
struct scb		*scb,
struct asd_done_list	*done_listp
)
{
	asd_pop_post_stack(asd, scb, done_listp);
}
#endif

#if 0
void
asd_print_hex(
unsigned char	*s,
unsigned	len
)
{
	unsigned	i;
	unsigned	count;

	while (len != 0) {
		count = (len > 16) ? 16 : len;

		for (i = 0 ; i < count ; i++) {
			printk("%02x ", *(s + i));
		}

		for ( ; i < 16 ; i++) {
			printk("   ");
		}

		for (i = 0 ; i < count ; i++) {
			if (((*(s + i) >= 'a') && (*(s + i) <= 'z')) ||
			    ((*(s + i) >= 'A') && (*(s + i) <= 'Z')) ||
			    ((*(s + i) >= '0') && (*(s + i) <= '9'))) {
				printk("%c", *(s + i));
			} else {
				printk(".");
			}
		}

		printk("\n");

		len = len - count;

		s = s + count;
	}
}
#endif
