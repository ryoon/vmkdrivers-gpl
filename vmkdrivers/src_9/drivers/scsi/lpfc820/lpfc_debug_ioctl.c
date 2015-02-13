/*******************************************************************
 * This file is part of the Emulex Linux Device Driver for         *
 * Fibre Channel Host Bus Adapters.                                *
 * Copyright (C) 2006-2010 Emulex.  All rights reserved.           *
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

#include <scsi/scsi_host.h>
#include <scsi/scsi.h>
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

#define min_value(x, y) ((x) < (y)) ? (x) : (y)

/* Define forward prototypes. */
static int lpfc_ioctl_lip(struct lpfc_hba *, struct lpfcCmdInput *, void *);
static int lpfc_ioctl_reset(struct lpfc_hba *, struct lpfcCmdInput *);

int
lpfc_process_ioctl_dfc(struct lpfc_hba * phba, struct lpfcCmdInput * cip)
{
	int rc = -1;
	uint32_t total_mem;
	void   *dataout;

	if (cip->lpfc_outsz >= 4096) {
		/*
		 * Allocate memory for ioctl data. If buffer is bigger than
		 * 64k, then we allocate 64k and re-use that buffer over and
		 * over to xfer the whole block. This is because Linux kernel
		 * has a problem allocating more than 120k of kernel space
		 * memory. Saw problem with GET_FCPTARGETMAPPING.
		 */
		if (cip->lpfc_outsz <= (64 * 1024))
			total_mem = cip->lpfc_outsz;
		else
			total_mem = 64 * 1024;		
	} else {
		total_mem = 4096;
	}

	dataout = kmalloc(total_mem, GFP_KERNEL);
	if (!dataout)
		return ENOMEM;

	switch (cip->lpfc_cmd) {
	case LPFC_LIP:
		rc = lpfc_ioctl_lip(phba, cip, dataout);
		break;
	case LPFC_RESET:
		rc = lpfc_ioctl_reset(phba, cip);
		break;
	case LPFC_SET_SD_TEST:
		rc = lpfc_ioctl_set_sd_test(phba, cip);
		break;
	case LPFC_SD_TEST:
		rc = lpfc_ioctl_sd_event(phba, cip);
		break;
	case LPFC_IOCTL_NODE_STAT:
		rc = lpfc_ioctl_node_stat(phba, cip, dataout);
		break;
	}

	if (rc == 0) {
		if (cip->lpfc_outsz) {
			if (copy_to_user
			    ((uint8_t *) cip->lpfc_dataout,
			     (uint8_t *) dataout, (int)cip->lpfc_outsz)) {
				rc = EIO;
			}
		}
	}

	kfree(dataout);
	return rc;
}

int
lpfc_ioctl_lip(struct lpfc_hba *phba, struct lpfcCmdInput *cip, void *dataout)
{
	struct lpfc_sli_ring *pring;
	LPFC_MBOXQ_t *pmboxq;
	int mbxstatus;
	int i;
	struct lpfc_vport *vport = phba->pport;

	mbxstatus = MBXERR_ERROR;
	if (vport->port_state == LPFC_HBA_READY) {

		pmboxq = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
		if (pmboxq == NULL)
			return ENOMEM;

		i = 0;
		pring = &phba->sli.ring[phba->sli.fcp_ring];
		while (pring->txcmplq_cnt) {
			if (i++ > 500)
				break;
			mdelay(10);
		}

		lpfc_init_link(phba, pmboxq, phba->cfg_topology,
			       phba->cfg_link_speed);

		mbxstatus = lpfc_sli_issue_mbox_wait(phba, pmboxq,
						     phba->fc_ratov * 2);
		if (mbxstatus == MBX_TIMEOUT)
			pmboxq->mbox_cmpl = 0;
		else
			mempool_free(pmboxq, phba->mbox_mem_pool);
	}

	memcpy(dataout, (char *)&mbxstatus, sizeof (uint16_t));
	return 0;
}

int
lpfc_ioctl_reset (struct lpfc_hba * phba, struct lpfcCmdInput * cip)
{
        uint32_t offset;
        int rc = 0;

        offset = (ulong) cip->lpfc_arg1;
        switch (offset) {
        case 1:
		/* Selective reset */
		rc = lpfc_selective_reset(phba);
                break;
	case 2:
        default:
		/* The driver only supports selective reset. */
                rc = ERANGE;
                break;
        }

        return rc;
}

/**
 * lpfc_ioctl_node_stat - Provide FCP Target Node Status for libdfc
 * @phba: pointer to lpfc hba data structure.
 * @cip.arg1 WWPN of physical or virtual port
 * @cip.arg2 number of node entries allocated by application
 *
 * @dataout: pointer to write DFC_GetNodeStat.
 * cip.outsiz: number of bytes of data in the dataout buff
 *
 * This routine is invoked by libdfc through the char device.
 **/

int
lpfc_ioctl_node_stat(struct lpfc_hba *phba, struct lpfcCmdInput *cip,
			  void *dataout)
{
	int i, rc = 0;
	struct lpfc_vport *vport = NULL;
	struct lpfc_vport **vports = NULL;
	int found = 0;
	uint32_t entry = 0, numberOfEntries = 0;
	struct DFC_GetNodeStat *vplist = dataout;
	struct DFC_NodeStatEntry_V1 *np = vplist->nodeStat;
	HBA_WWN vport_wwpn;
	struct lpfc_nodelist *ndlp;
	struct Scsi_Host *shost = NULL;

	if (cip->lpfc_cntl == LPFC_CNTL_X86_APP)
		return ENOENT;

	if (copy_from_user(&vport_wwpn, cip->lpfc_arg1, sizeof(vport_wwpn)))
		return EIO;

	numberOfEntries = (ulong) cip->lpfc_arg2;

	/* Search for user's vport_wwpn */
	vports = lpfc_create_vport_work_array(phba);
	if (!vports)
		return ENOMEM;

	for (i = 0; i <= phba->max_vpi && vports[i] != NULL; i++) {
		if (vports[i]->load_flag & (FC_UNLOADING | FC_LOADING))
			continue;
		if (!memcmp(&vport_wwpn, &vports[i]->fc_portname,
			    sizeof(HBA_WWN))) {
			found = 1;
			vport = vports[i];
			shost = lpfc_shost_from_vport(vport);
			scsi_host_get(shost);
			break;
		}
	}
	lpfc_destroy_vport_work_array(phba, vports);
	if (!found) {
		cip->lpfc_outsz = 0;
		rc = ENOENT;
		goto exit;
	}

	spin_lock_irq(shost->host_lock);
	list_for_each_entry(ndlp, &vport->fc_nodes, nlp_listp) {
		if (entry > numberOfEntries)
			entry++;
		else if (lpfc_nlp_get(ndlp)) {
			if (ndlp->nlp_state == NLP_STE_MAPPED_NODE) {
				np->TargetNumber = ndlp->nlp_sid;
				np->fc_did = ndlp->nlp_DID;
				memcpy(&np->wwnn, &ndlp->nlp_nodename,
				       sizeof(HBA_WWN));
				memcpy(&np->wwpn, &ndlp->nlp_portname,
				       sizeof(HBA_WWN));
				np->TargetQDepth = ndlp->cmd_qdepth;
				np->TargetMaxCnt = ndlp->cmd_max;
				np->TargetActiveCnt =
					atomic_read(&ndlp->cmd_pending);
				np->TargetBusyCnt = ndlp->scsi_busy_cnt;
				np->TargetFcpErrCnt = ndlp->fcperr_cnt;
				np->TargetAbtsCnt = ndlp->abts_cnt;
				np->TargetTimeOutCnt = ndlp->tmo_cnt;
				np->TargetNoRsrcCnt = ndlp->no_rsrc_cnt;
				np->TargetInvldRpiCnt = ndlp->invld_rpi_cnt;
				np->TargetLclRjtCnt = ndlp->rjt_cnt;
				np->TargetResetCnt = ndlp->tgt_rst;
				np->TargetLunResetCnt = ndlp->lun_rst;
				np++;
				entry++;
			}
			lpfc_nlp_put(ndlp);
		}
	}
	spin_unlock_irq(shost->host_lock);
	if (entry > numberOfEntries)
		rc = E2BIG;

	/*
	 * The scsi_host_put call is only valid if the driver matched the
	 * vport wwpn.  An error path jump would not have found the match.
	 */
	scsi_host_put(shost);
 exit:
	vplist->version = NODE_STAT_VERSION;
	vplist->numberOfEntries = entry;
	cip->lpfc_outsz =
	min_value((uint32_t)(entry *
			sizeof(struct DFC_NodeStatEntry_V1) +
			sizeof(struct DFC_GetNodeStat)),
			cip->lpfc_outsz);
	return rc;
}
