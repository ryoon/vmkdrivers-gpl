/*******************************************************************
 * This file is part of the Emulex Linux Device Driver for         *
 * Fibre Channel Host Bus Adapters.                                *
 * Copyright (C) 2010 Emulex.  All rights reserved.                *
 * EMULEX and SLI are trademarks of Emulex.                        *
 * www.emulex.com                                                  *
 *                                                                 *
 * This program is free software; you can redistribute it and/or   *
 * modify it under the terms of version 2 of the GNU General       *
 * Public License as published by the Free Software Foundation.    *
 * This program is distributed in the hope that it will be useful. *
 * ALL EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND          *
 * WARRANTIES, INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY,  *
 * FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT, ARE      *
 * DISCLAIMED, EXCEPT TO THE EXTENT THAT SUCH DISCLAIMERS ARE HELD *
 * TO BE LEGALLY INVALID.  See the GNU General Public License for  *
 * more details, a copy of which can be found in the file COPYING  *
 * included with this package.                                     *
 *******************************************************************/

#include <linux/delay.h>
#include <linux/blkdev.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#if !defined(LINUX_VERSION_CODE)
#include <linux/version.h>
#endif

#include <scsi/scsi.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_tcq.h>
#include <scsi/scsi_transport_fc.h>

#include "lpfc_version.h"
#include "fc_fs.h"
#include "lpfc_hw4.h"
#include "lpfc_hw.h"
#include "lpfc_compat.h"
#include "lpfc_sli.h"
#include "lpfc_sli4.h"
#include "lpfc_nl.h"
#include "lpfc_disc.h"
#include "lpfc.h"
#include "lpfc_logmsg.h"
#include "lpfc_scsi.h"
#include "hbaapi.h"
#include "lpfc_ioctl.h"
#include "lpfc_crtn.h"
#include "lpfc_vport.h"
#include "lpfc_events.h"

/* DFC SD TEST HARNESS */

#define DFC_MAX_PORTS	16
#define DFC_MAX_EVENTS	20

/* Set to one by IOCTLs for san diag test */
uint32_t sd_test;

/* Set to one to inject a SD event */
uint8_t inject_array [DFC_MAX_PORTS] [DFC_MAX_EVENTS];

void 
dfc_inject_sdevent(struct lpfc_hba *phba,
		struct lpfc_iocbq *pIocbIn,
		struct lpfc_iocbq *pIocbOut)
{
	struct lpfc_scsi_buf *lpfc_cmd =
		(struct lpfc_scsi_buf *) pIocbIn->context1;

	if (inject_array [phba->brd_no][SD_INJECT_FBUSY]) {
		lpfc_cmd->status = IOSTAT_FABRIC_BSY;
		pIocbOut->iocb.ulpStatus = IOSTAT_FABRIC_BSY;
		inject_array [phba->brd_no][SD_INJECT_FBUSY] = 0;
		return;	
	}
	if (inject_array [phba->brd_no][SD_INJECT_PBUSY]) {
		lpfc_cmd->status = IOSTAT_NPORT_BSY;
		pIocbOut->iocb.ulpStatus = IOSTAT_NPORT_BSY;
		inject_array [phba->brd_no][SD_INJECT_PBUSY] = 0;
		return;
	}
	if (inject_array [phba->brd_no][SD_INJECT_FCPERR]) {
		struct scsi_cmnd *cmnd = lpfc_cmd->pCmd;
		struct fcp_rsp *fcprsp = lpfc_cmd->fcp_rsp;
		if (cmnd->sc_data_direction == DMA_FROM_DEVICE) {
			lpfc_cmd->status = IOSTAT_FCP_RSP_ERROR;
			pIocbOut->iocb.ulpStatus = IOSTAT_FCP_RSP_ERROR;
			pIocbOut->iocb.un.fcpi.fcpi_parm++;
			fcprsp->rspStatus2 = RESID_OVER;
			inject_array [phba->brd_no][SD_INJECT_FCPERR] = 0;
		}
		return;
	}
	if (inject_array [phba->brd_no][SD_INJECT_QFULL]) {
		lpfc_cmd->status = IOSTAT_FCP_RSP_ERROR;
		pIocbOut->iocb.ulpStatus = IOSTAT_FCP_RSP_ERROR;
		lpfc_cmd->pCmd->result = SAM_STAT_TASK_SET_FULL;
		pIocbOut->iocb.un.ulpWord[4] = SAM_STAT_TASK_SET_FULL;
		inject_array [phba->brd_no][SD_INJECT_QFULL] = 0;
		return;
	}
	if (inject_array [phba->brd_no][SD_INJECT_DEVBSY]) {
		lpfc_cmd->status = IOSTAT_FCP_RSP_ERROR;
		pIocbOut->iocb.ulpStatus = IOSTAT_FCP_RSP_ERROR;
		lpfc_cmd->pCmd->result = SAM_STAT_BUSY;
		pIocbOut->iocb.un.ulpWord[4] = SAM_STAT_BUSY;
		inject_array [phba->brd_no][SD_INJECT_DEVBSY] = 0;
		return;
	}
}

void
dfc_inject_qdepth_down_s4(struct lpfc_hba *phba,
		struct lpfc_wcqe_complete *wcqe) {
	if (inject_array[phba->brd_no][SD_INJECT_QDEPTH]) {
		bf_set(lpfc_wcqe_c_status, wcqe, IOSTAT_LOCAL_REJECT);
		wcqe->parameter = IOERR_NO_RESOURCES;
		inject_array[phba->brd_no][SD_INJECT_QDEPTH] = 0;
	}

}

void
dfc_inject_qdepth_down(struct lpfc_hba *phba, IOCB_t *rsp)
{
	if (inject_array[phba->brd_no][SD_INJECT_QDEPTH]) {
		rsp->ulpStatus = IOSTAT_LOCAL_REJECT;
		rsp->un.ulpWord[4] = IOERR_NO_RESOURCES;
		inject_array[phba->brd_no][SD_INJECT_QDEPTH] = 0;
	}
}



/* ELS EVENTS */
void
dfc_sd_plogi(struct lpfc_hba *phba, HBA_WWN wwpn)
{
	struct lpfc_els_event_header *els_data;
	struct event_type evt_type;
	int sd_event_size = sizeof(struct lpfc_sd_event)
			  + sizeof(struct lpfc_els_event_header);
        struct lpfc_sd_event *sd_event = kzalloc(sd_event_size, GFP_KERNEL);

	if (!sd_event)
                return;

	els_data = (void *)&sd_event->event_payload;

        els_data->event_type = FC_REG_ELS_EVENT;
        els_data->subcategory = LPFC_EVENT_PLOGI_RCV;
	memcpy (&els_data->wwpn, &wwpn, sizeof(wwpn));
	memcpy (&els_data->wwnn, &wwpn, sizeof(wwpn));
	
	evt_type.mask = FC_REG_SANDIAGS_EVENT;
	evt_type.cat = els_data->event_type;
	evt_type.sub = els_data->subcategory;
	lpfc_send_event(phba, sd_event_size, sd_event, evt_type);
}

void
dfc_sd_prlo(struct lpfc_hba *phba, HBA_WWN wwpn)
{
	struct lpfc_els_event_header *els_data;
	struct event_type evt_type;
	int sd_event_size = sizeof(struct lpfc_sd_event)
			  + sizeof(struct lpfc_els_event_header);
	struct lpfc_sd_event *sd_event = kzalloc(sd_event_size, GFP_KERNEL);

	if (!sd_event)
		return;

	els_data = (void *)&sd_event->event_payload;

	els_data->event_type = FC_REG_ELS_EVENT;
	els_data->subcategory = LPFC_EVENT_PRLO_RCV;

	memcpy (&els_data->wwpn, &wwpn, sizeof(wwpn));
	memcpy (&els_data->wwnn, &wwpn, sizeof(wwpn));

	evt_type.mask = FC_REG_SANDIAGS_EVENT;
	evt_type.cat = els_data->event_type;
	evt_type.sub = els_data->subcategory;
	lpfc_send_event(phba, sd_event_size, sd_event, evt_type);
}

void
dfc_sd_adisc(struct lpfc_hba *phba, HBA_WWN wwpn)
{
	struct lpfc_els_event_header *els_data;
	struct event_type evt_type;
	int sd_event_size = sizeof(struct lpfc_sd_event)
			  + sizeof(struct lpfc_els_event_header);
	struct lpfc_sd_event *sd_event = kzalloc(sd_event_size, GFP_KERNEL);

	if (!sd_event)
		return;

	els_data = (void *)&sd_event->event_payload;

	els_data->event_type= FC_REG_ELS_EVENT;
	els_data->subcategory = LPFC_EVENT_ADISC_RCV;

	memcpy (&els_data->wwpn, &wwpn, sizeof(wwpn));
	memcpy (&els_data->wwnn, &wwpn, sizeof(wwpn));

	evt_type.mask = FC_REG_SANDIAGS_EVENT;
	evt_type.cat = els_data->event_type;
	evt_type.sub = els_data->subcategory;
	lpfc_send_event(phba, sd_event_size, sd_event, evt_type);
}

void
dfc_sd_lsrjt(struct lpfc_hba *phba, HBA_WWN wwpn)
{
	struct lpfc_lsrjt_event *els_data;
	struct event_type evt_type;
	int sd_event_size = sizeof(struct lpfc_sd_event)
			  + sizeof(struct lpfc_lsrjt_event);
	struct lpfc_sd_event *sd_event = kzalloc(sd_event_size, GFP_KERNEL);

	if (!sd_event)
		return;

	els_data = (void *)&sd_event->event_payload;

	els_data->header.event_type = FC_REG_ELS_EVENT;
	els_data->header.subcategory = LPFC_EVENT_LSRJT_RCV;
	els_data->command = ELS_CMD_PLOGI;
	els_data->reason_code = LSRJT_LOGICAL_BSY;
	els_data->explanation = LSEXP_OUT_OF_RESOURCE;

        memcpy (&els_data->header.wwpn, &wwpn, sizeof(wwpn));
        memcpy (&els_data->header.wwnn, &wwpn, sizeof(wwpn));

	evt_type.mask = FC_REG_SANDIAGS_EVENT;
	evt_type.cat = els_data->header.event_type;
	evt_type.sub = els_data->header.subcategory;
	lpfc_send_event(phba, sd_event_size, sd_event, evt_type);
}

void
dfc_sd_logo(struct lpfc_hba *phba, HBA_WWN wwpn)
{
	struct lpfc_els_event_header *els_data = NULL;
	struct lpfc_logo_event *logo_data = NULL;
        int sd_event_size = sizeof(struct lpfc_sd_event)
			+ sizeof(struct lpfc_logo_event); 
	struct event_type evt_type;
	
	sd_event_size = sizeof(struct lpfc_sd_event)
		      + sizeof(struct lpfc_logo_event);

	struct lpfc_sd_event *sd_event = kzalloc(sd_event_size, GFP_KERNEL);
	
	if (!sd_event)
		return;

        logo_data = (void *)&sd_event->event_payload;
	els_data = &logo_data->header;

	els_data->event_type = FC_REG_ELS_EVENT;
	els_data->subcategory = LPFC_EVENT_LOGO_RCV;

	memcpy (&els_data->wwpn, &wwpn, sizeof(wwpn));
	memcpy (&els_data->wwnn, &wwpn, sizeof(wwpn));
	memcpy (&logo_data->logo_wwpn, &wwpn, sizeof(wwpn));

	evt_type.mask = FC_REG_SANDIAGS_EVENT;
	evt_type.cat = els_data->event_type;
	evt_type.sub = els_data->subcategory;
	lpfc_send_event(phba, sd_event_size, sd_event, evt_type);
}
		
/* FABRIC EVENTS */
void
dfc_sd_fabric_busy(struct lpfc_hba *phba, HBA_WWN wwpn)
{
	struct lpfc_fabric_event_header *fabric_data;
	struct event_type evt_type;
	int sd_event_size = sizeof(struct lpfc_sd_event)
			  + sizeof(struct lpfc_fabric_event_header);
	struct lpfc_sd_event *sd_event = kzalloc(sd_event_size, GFP_KERNEL);

	if (!sd_event)
		return;

	fabric_data = (void *)&sd_event->event_payload;


	fabric_data->event_type = FC_REG_FABRIC_EVENT;
	fabric_data->subcategory = LPFC_EVENT_FABRIC_BUSY;

	evt_type.mask = FC_REG_SANDIAGS_EVENT;
	evt_type.cat = fabric_data->event_type;
	evt_type.sub = fabric_data->subcategory;
	lpfc_send_event(phba, sd_event_size, sd_event, evt_type);
}

void
dfc_sd_port_busy(struct lpfc_hba *phba, HBA_WWN wwpn)
{
	struct lpfc_fabric_event_header *fabric_data;
	struct event_type evt_type;
	int sd_event_size = sizeof(struct lpfc_sd_event)
			  + sizeof(struct lpfc_fabric_event_header);
	struct lpfc_sd_event *sd_event = kzalloc(sd_event_size, GFP_KERNEL);

	if (!sd_event)
		return;

	fabric_data = (void *)&sd_event->event_payload;

	fabric_data->event_type = FC_REG_FABRIC_EVENT;
	fabric_data->subcategory = LPFC_EVENT_PORT_BUSY;

	memcpy (&fabric_data->wwpn, &wwpn, sizeof(wwpn));

	evt_type.mask = FC_REG_SANDIAGS_EVENT;
	evt_type.cat = fabric_data->event_type;
	evt_type.sub = fabric_data->subcategory;
	lpfc_send_event(phba, sd_event_size, sd_event, evt_type);
}

void
dfc_sd_fcprdchkerr(struct lpfc_hba *phba, HBA_WWN wwpn)
{
	struct lpfc_fcprdchkerr_event *fabric_data;
	struct event_type evt_type;
	int sd_event_size = sizeof(struct lpfc_sd_event)
			  + sizeof(struct lpfc_fcprdchkerr_event);
	struct lpfc_sd_event *sd_event = kzalloc(sd_event_size, GFP_KERNEL);

	if (!sd_event)
		return;

	fabric_data = (void *)&sd_event->event_payload;

	fabric_data->header.event_type = FC_REG_FABRIC_EVENT;
	fabric_data->header.subcategory = LPFC_EVENT_FCPRDCHKERR;
	fabric_data->lun = 0x2a;
	fabric_data->opcode = 0x2b;
	fabric_data->fcpiparam = 0x2c;

	memcpy (&fabric_data->header.wwpn, &wwpn, sizeof(wwpn));
	memcpy (&fabric_data->header.wwnn, &wwpn, sizeof(wwpn));

	evt_type.mask = FC_REG_SANDIAGS_EVENT;
	evt_type.cat = fabric_data->header.event_type;
	evt_type.sub = fabric_data->header.subcategory;
	lpfc_send_event(phba, sd_event_size, sd_event, evt_type);
}


/* SCSI EVENTS */
void
dfc_sd_qfull(struct lpfc_hba *phba, HBA_WWN wwpn)
{
	struct lpfc_scsi_event_header *scsi_data;
	struct event_type evt_type;
	int sd_event_size = sizeof(struct lpfc_sd_event)
			  + sizeof(struct lpfc_scsi_event_header);
	struct lpfc_sd_event *sd_event = kzalloc(sd_event_size, GFP_KERNEL);

	if (!sd_event)
		return;

	scsi_data = (void *)&sd_event->event_payload;

	scsi_data->event_type = FC_REG_SCSI_EVENT;
	scsi_data->subcategory = LPFC_EVENT_QFULL;
	scsi_data->lun = 0x2d;

	memcpy (&scsi_data->wwpn, &wwpn, sizeof(wwpn));
	memcpy (&scsi_data->wwnn, &wwpn, sizeof(wwpn));

	evt_type.mask = FC_REG_SANDIAGS_EVENT;
	evt_type.cat = scsi_data->event_type;
	evt_type.sub = scsi_data->subcategory;
	lpfc_send_event(phba, sd_event_size, sd_event, evt_type);
}


void
dfc_sd_devbsy(struct lpfc_hba *phba, HBA_WWN wwpn)
{
	struct lpfc_scsi_event_header *scsi_data;
	struct event_type evt_type;
	int sd_event_size = sizeof(struct lpfc_sd_event)
			  + sizeof(struct lpfc_scsi_event_header);
	struct lpfc_sd_event *sd_event = kzalloc(sd_event_size, GFP_KERNEL);

	if (!sd_event)
		return;

	scsi_data = (void *)&sd_event->event_payload;

	scsi_data->event_type = FC_REG_SCSI_EVENT;
	scsi_data->subcategory = LPFC_EVENT_DEVBSY;
	scsi_data->lun = 0x2e;

	memcpy (&scsi_data->wwpn, &wwpn, sizeof(wwpn));
	memcpy (&scsi_data->wwnn, &wwpn, sizeof(wwpn));

	evt_type.mask = FC_REG_SANDIAGS_EVENT;
	evt_type.cat = scsi_data->event_type;
	evt_type.sub = scsi_data->subcategory;
	lpfc_send_event(phba, sd_event_size, sd_event, evt_type);
}

void
dfc_sd_chkcond(struct lpfc_hba *phba, HBA_WWN wwpn)
{
/* special case scsi check condition event */

	struct lpfc_scsi_check_condition_event *scsi_data;
	struct event_type evt_type;
	int sd_event_size = sizeof(struct lpfc_sd_event)
			  + sizeof(struct lpfc_scsi_check_condition_event);
	struct lpfc_sd_event *sd_event = kzalloc(sd_event_size, GFP_KERNEL);

	if (!sd_event)
		return;

	scsi_data = (void *)&sd_event->event_payload;

	scsi_data->scsi_event.event_type = FC_REG_SCSI_EVENT;
	scsi_data->scsi_event.subcategory = LPFC_EVENT_CHECK_COND;
	scsi_data->scsi_event.lun = 0x30;
	scsi_data->opcode = 0x8; 
	scsi_data->sense_key = 0x5;
	scsi_data->asc = 0x29;
	scsi_data->ascq =0x1;

	memcpy (&scsi_data->scsi_event.wwpn, &wwpn, sizeof(wwpn));
	memcpy (&scsi_data->scsi_event.wwnn, &wwpn, sizeof(wwpn));

	evt_type.mask = FC_REG_SANDIAGS_EVENT;
	evt_type.cat = scsi_data->scsi_event.event_type;
	evt_type.sub = scsi_data->scsi_event.subcategory;
	lpfc_send_event(phba, sd_event_size, sd_event, evt_type);
}

void
dfc_sd_lunreset(struct lpfc_hba *phba, HBA_WWN wwpn)
{
	struct lpfc_scsi_event_header *scsi_data;
	struct event_type evt_type;
	int sd_event_size = sizeof(struct lpfc_sd_event)
	  + sizeof(struct lpfc_scsi_event_header);
	struct lpfc_sd_event *sd_event = kzalloc(sd_event_size, GFP_KERNEL);

	if (!sd_event)
		return;

        scsi_data = (void *)&sd_event->event_payload;

	scsi_data->event_type = FC_REG_SCSI_EVENT;
	scsi_data->subcategory = LPFC_EVENT_LUNRESET;
	scsi_data->lun = 0x31;

	memcpy (&scsi_data->wwpn, &wwpn, sizeof(wwpn));
	memcpy (&scsi_data->wwnn, &wwpn, sizeof(wwpn));

	evt_type.mask = FC_REG_SANDIAGS_EVENT;
	evt_type.cat = scsi_data->event_type;
	evt_type.sub = scsi_data->subcategory;
	lpfc_send_event(phba, sd_event_size, sd_event, evt_type);

}

void
dfc_sd_tgtreset(struct lpfc_hba *phba, HBA_WWN wwpn)
{
        struct lpfc_scsi_event_header *scsi_data;
        struct event_type evt_type;
        int sd_event_size = sizeof(struct lpfc_sd_event)
		          + sizeof(struct lpfc_scsi_event_header);
        struct lpfc_sd_event *sd_event = kzalloc(sd_event_size, GFP_KERNEL);

	if (!sd_event)
                return;

        scsi_data = (void *)&sd_event->event_payload;

        scsi_data->event_type = FC_REG_SCSI_EVENT;
        scsi_data->subcategory = LPFC_EVENT_TGTRESET;
        scsi_data->lun = 0x32;

        memcpy (&scsi_data->wwpn, &wwpn, sizeof(wwpn));
        memcpy (&scsi_data->wwnn, &wwpn, sizeof(wwpn));

        evt_type.mask = FC_REG_SANDIAGS_EVENT;
        evt_type.cat = scsi_data->event_type;
        evt_type.sub = scsi_data->subcategory;
        lpfc_send_event(phba, sd_event_size, sd_event, evt_type);
}

void
dfc_sd_busreset(struct lpfc_hba *phba, HBA_WWN wwpn)
{
	struct lpfc_scsi_event_header *scsi_data;
	struct event_type evt_type;
	int sd_event_size = sizeof(struct lpfc_sd_event)
			  + sizeof(struct lpfc_scsi_event_header);
	struct lpfc_sd_event *sd_event = kzalloc(sd_event_size, GFP_KERNEL);

	if (!sd_event)
		return;

	scsi_data = (void *)&sd_event->event_payload;

	scsi_data->event_type = FC_REG_SCSI_EVENT;
	scsi_data->subcategory = LPFC_EVENT_BUSRESET;
	scsi_data->lun = 0x33;

	memcpy (&scsi_data->wwpn, &wwpn, sizeof(wwpn));
	memcpy (&scsi_data->wwnn, &wwpn, sizeof(wwpn));

	evt_type.mask = FC_REG_SANDIAGS_EVENT;
	evt_type.cat = scsi_data->event_type;
	evt_type.sub = scsi_data->subcategory;
	lpfc_send_event(phba, sd_event_size, sd_event, evt_type);
}

void
dfc_sd_varquedepth(struct lpfc_hba *phba, HBA_WWN wwpn)
{
	struct lpfc_scsi_varqueuedepth_event *scsi_data;
	struct event_type evt_type;
	int sd_event_size = sizeof(struct lpfc_sd_event)
			  + sizeof(struct lpfc_scsi_varqueuedepth_event);
        struct lpfc_sd_event *sd_event = kzalloc(sd_event_size, GFP_KERNEL);

	if (!sd_event)
                return;

	scsi_data = (void *)&sd_event->event_payload;

	memcpy (&scsi_data->scsi_event.wwpn, &wwpn, sizeof(wwpn));
	memcpy (&scsi_data->scsi_event.wwnn, &wwpn, sizeof(wwpn));

	scsi_data->scsi_event.event_type = FC_REG_SCSI_EVENT;
	scsi_data->scsi_event.subcategory = LPFC_EVENT_VARQUEDEPTH;
	scsi_data->scsi_event.lun = 0x34;
	scsi_data->oldval = 100;
	scsi_data->newval = 256;

	evt_type.mask = FC_REG_SANDIAGS_EVENT;
	evt_type.cat = scsi_data->scsi_event.event_type;
	evt_type.sub = scsi_data->scsi_event.subcategory;
	lpfc_send_event(phba, sd_event_size, sd_event, evt_type);
}


/* BOARD EVENTS */
void
dfc_sd_portinterr(struct lpfc_hba *phba)
{
	lpfc_board_errevt_to_mgmt(phba);
}

/* ADAPTER EVENTS */
void
dfc_sd_arrival(struct lpfc_hba *phba, HBA_WWN wwpn)
{
	struct lpfc_adapter_event_header *adapter_data;
	struct event_type evt_type;
	int sd_event_size = sizeof(struct lpfc_sd_event)
			  + sizeof(struct lpfc_adapter_event_header);
	struct lpfc_sd_event *sd_event = kzalloc(sd_event_size, GFP_KERNEL);

	if (!sd_event)
		return;

	adapter_data = (void *)&sd_event->event_payload;

	adapter_data->event_type = FC_REG_ADAPTER_EVENT;
	adapter_data->subcategory = LPFC_EVENT_ARRIVAL;

	evt_type.mask = FC_REG_SANDIAGS_EVENT;
	evt_type.cat = adapter_data->event_type;
	evt_type.sub = adapter_data->subcategory;
	lpfc_send_event(phba, sd_event_size, sd_event, evt_type);
}


int
lpfc_ioctl_set_sd_test(struct lpfc_hba *phba, struct lpfcCmdInput *cip)
{
	/* Remove any stale data in the array */
	 memset(inject_array, 0, sizeof(inject_array));
	sd_test = cip->lpfc_arg4;
	return 0;
}

int
lpfc_ioctl_sd_event(struct lpfc_hba *phba, struct lpfcCmdInput *cip)
{
	HBA_WWN wwpn;
	uint32_t offset;
	int rc = 0;

	if (sd_test == 0)
		return EBUSY;

	if (copy_from_user(&wwpn, cip->lpfc_arg1, sizeof (wwpn)))
		return EIO;

        offset = cip->lpfc_arg4;
        switch (offset) {
	/* els events */
        case SD_INJECT_PLOGI:
		dfc_sd_plogi(phba, wwpn);
		break;
	case SD_INJECT_PRLO:
		dfc_sd_prlo(phba, wwpn);
		break;
        case SD_INJECT_ADISC:
		dfc_sd_adisc(phba, wwpn);
		break;
        case SD_INJECT_LSRJCT:
		dfc_sd_lsrjt(phba, wwpn);
		break;
        case SD_INJECT_LOGO:
                dfc_sd_logo(phba, wwpn);
                break;


	/* fabric events */
        case SD_INJECT_FBUSY:
		if (cip->lpfc_arg5)
			inject_array[phba->brd_no][SD_INJECT_FBUSY] = 1;
		else
			dfc_sd_fabric_busy(phba, wwpn);
		break;
        case SD_INJECT_PBUSY:
		if (cip->lpfc_arg5)
			inject_array[phba->brd_no][SD_INJECT_PBUSY] = 1;
		else
			dfc_sd_port_busy(phba, wwpn);
		break;
        case SD_INJECT_FCPERR:
		if (cip->lpfc_arg5)
			inject_array[phba->brd_no][SD_INJECT_FCPERR] = 1;
		else
			dfc_sd_fcprdchkerr(phba, wwpn);
		break;
	
	/* scsi events */
        case SD_INJECT_QFULL:
		if (cip->lpfc_arg5)
			inject_array[phba->brd_no][SD_INJECT_QFULL] = 1;
		else
			dfc_sd_qfull(phba, wwpn);
		break;
        case SD_INJECT_DEVBSY:
		if (cip->lpfc_arg5)
			inject_array[phba->brd_no][SD_INJECT_DEVBSY] = 1;
		else
			dfc_sd_devbsy(phba, wwpn);
		break;
        case SD_INJECT_CHKCOND:
		dfc_sd_chkcond(phba, wwpn);
		break;
        case SD_INJECT_LUNRST:
		dfc_sd_lunreset(phba, wwpn);
		break;
        case SD_INJECT_TGTRST:
		dfc_sd_tgtreset(phba, wwpn);
		break;
        case SD_INJECT_BUSRST:
		dfc_sd_busreset(phba, wwpn);
		break;
        case SD_INJECT_QDEPTH:
		if (cip->lpfc_arg5)
			inject_array[phba->brd_no][SD_INJECT_QDEPTH] = 1;
		else
			dfc_sd_varquedepth(phba, wwpn);
		break;

	/* board events */
        case SD_INJECT_PERR:
		dfc_sd_portinterr(phba);
		break;

	/* adapter events */
        case SD_INJECT_ARRIVE:
		dfc_sd_arrival(phba, wwpn);
		break;

	default:
		rc = EINVAL;
	}
	cip->lpfc_outsz = 0;
	return rc;
}
