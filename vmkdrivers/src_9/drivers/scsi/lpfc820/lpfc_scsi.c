/*******************************************************************
 * This file is part of the Emulex Linux Device Driver for         *
 * Fibre Channel Host Bus Adapters.                                *
 * Copyright (C) 2004-2010 Emulex.  All rights reserved.           *
 * EMULEX and SLI are trademarks of Emulex.                        *
 * www.emulex.com                                                  *
 * Portions Copyright (C) 2004-2005 Christoph Hellwig              *
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

#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#include <scsi/scsi.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_tcq.h>
#include <scsi/scsi_transport_fc.h>

#if defined(__VMKLNX__)
#include <vmklinux_9/vmklinux_scsi.h>
#endif

#include "lpfc_version.h"
#include "lpfc_hw4.h"
#include "lpfc_hw.h"
#include "lpfc_sli.h"
#include "lpfc_sli4.h"
#include "lpfc_nl.h"
#include "lpfc_disc.h"
#include "lpfc_scsi.h"
#include "lpfc.h"
#include "lpfc_logmsg.h"
#include "lpfc_vport.h"
#include "hbaapi.h"
#include "lpfc_ioctl.h"
#include "lpfc_events.h"
#include "lpfc_crtn.h"

#define LPFC_RESET_WAIT  2
#define LPFC_ABORT_WAIT  2

#define TGT_BYTES       82      /* Bytes consumed per target display in proc */
#define SAN_BYTES       300     /* Bytes consumed by the SAN display in proc */
#define VPORT_BYTES     117     /* Bytes consumed by the Vport display in proc */
#define INFO_BUF        8192    /* Max bytes per proc node dump. */
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
unsigned long lpfc_cpu_clock;

static void
lpfc_release_scsi_buf_s4(struct lpfc_hba *phba, struct lpfc_scsi_buf *psb);
static void
lpfc_release_scsi_buf_s3(struct lpfc_hba *phba, struct lpfc_scsi_buf *psb);


void
lpfc_set_clock() {
	uint64_t start_time, end_time, time_delta;

	if (lpfc_cpu_clock)
		return;

	rdtscll(start_time);
	mdelay(1);
	rdtscll(end_time);

	if (end_time >= start_time)
		time_delta = end_time - start_time;
	else
		time_delta = 0xffffffffffffffff - start_time + end_time;

	/* Clock speed in MHz */
	lpfc_cpu_clock = time_delta / 1000;
}
/**
 * lpfc_sli4_set_rsp_sgl_last - Set the last bit in the response sge.
 * @phba: Pointer to HBA object.
 * @lpfc_cmd: lpfc scsi command object pointer.
 *
 * This function is called from the lpfc_prep_task_mgmt_cmd function to
 * set the last bit in the response sge entry.
 **/
static void
lpfc_sli4_set_rsp_sgl_last(struct lpfc_hba *phba,
				struct lpfc_scsi_buf *lpfc_cmd)
{
	struct sli4_sge *sgl = (struct sli4_sge *)lpfc_cmd->fcp_bpl;
	if (sgl) {
		sgl += 1;
		sgl->word2 = le32_to_cpu(sgl->word2);
		bf_set(lpfc_sli4_sge_last, sgl, 1);
		sgl->word2 = cpu_to_le32(sgl->word2);
	}
}

/**
 * lpfc_update_stats - Update statistical data for the command completion
 * @phba: Pointer to HBA object.
 * @lpfc_cmd: lpfc scsi command object pointer.
 *
 * This function is called when there is a command completion and this
 * function updates the statistical data for the command completion.
 **/
static void
lpfc_update_stats(struct lpfc_hba *phba, struct  lpfc_scsi_buf *lpfc_cmd)
{
	struct lpfc_rport_data *rdata = lpfc_cmd->rdata;
	struct lpfc_nodelist *pnode = rdata->pnode;
	struct scsi_cmnd *cmd = lpfc_cmd->pCmd;
	unsigned long flags;
	struct Scsi_Host  *shost = cmd->device->host;
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	unsigned long latency;
	int i;

	if (cmd->result)
		return;

	latency = jiffies_to_msecs((long)jiffies - (long)lpfc_cmd->start_time);

	spin_lock_irqsave(shost->host_lock, flags);
	if (!vport->stat_data_enabled ||
		vport->stat_data_blocked ||
		!pnode ||
		!pnode->lat_data ||
		(phba->bucket_type == LPFC_NO_BUCKET)) {
		spin_unlock_irqrestore(shost->host_lock, flags);
		return;
	}

	if (phba->bucket_type == LPFC_LINEAR_BUCKET) {
		i = (latency + phba->bucket_step - 1 - phba->bucket_base)/
			phba->bucket_step;
		/* check array subscript bounds */
		if (i < 0)
			i = 0;
		else if (i >= LPFC_MAX_BUCKET_COUNT)
			i = LPFC_MAX_BUCKET_COUNT - 1;
	} else {
		for (i = 0; i < LPFC_MAX_BUCKET_COUNT-1; i++)
			if (latency <= (phba->bucket_base +
				((1<<i)*phba->bucket_step)))
				break;
	}

	pnode->lat_data[i].cmd_count++;
	spin_unlock_irqrestore(shost->host_lock, flags);
}

/**
 * lpfc_send_sdev_queuedepth_change_event - Posts a queuedepth change event
 * @phba: Pointer to HBA context object.
 * @vport: Pointer to vport object.
 * @ndlp: Pointer to FC node associated with the target.
 * @lun: Lun number of the scsi device.
 * @old_val: Old value of the queue depth.
 * @new_val: New value of the queue depth.
 *
 * This function sends an event to the mgmt application indicating
 * there is a change in the scsi device queue depth.
 **/
static void
lpfc_send_sdev_queuedepth_change_event(struct lpfc_hba *phba,
		struct lpfc_vport  *vport,
		struct lpfc_nodelist *ndlp,
		uint32_t lun,
		uint32_t old_val,
		uint32_t new_val)
{
	struct lpfc_fast_path_event *fast_path_evt;
	unsigned long flags;

	fast_path_evt = lpfc_alloc_fast_evt(phba);
	if (!fast_path_evt)
		return;

	fast_path_evt->un.queue_depth_evt.scsi_event.event_type =
		FC_REG_SCSI_EVENT;
	fast_path_evt->un.queue_depth_evt.scsi_event.subcategory =
		LPFC_EVENT_VARQUEDEPTH;

	/* Report all luns with change in queue depth */
	fast_path_evt->un.queue_depth_evt.scsi_event.lun = lun;
	if (ndlp && NLP_CHK_NODE_ACT(ndlp)) {
		memcpy(&fast_path_evt->un.queue_depth_evt.scsi_event.wwpn,
			&ndlp->nlp_portname, sizeof(struct lpfc_name));
		memcpy(&fast_path_evt->un.queue_depth_evt.scsi_event.wwnn,
			&ndlp->nlp_nodename, sizeof(struct lpfc_name));
	}

	fast_path_evt->un.queue_depth_evt.oldval = old_val;
	fast_path_evt->un.queue_depth_evt.newval = new_val;
	fast_path_evt->vport = vport;

	fast_path_evt->work_evt.evt = LPFC_EVT_FASTPATH_MGMT_EVT;
	spin_lock_irqsave(&phba->hbalock, flags);
	list_add_tail(&fast_path_evt->work_evt.evt_listp, &phba->work_list);
	spin_unlock_irqrestore(&phba->hbalock, flags);
	lpfc_worker_wake_up(phba);

	return;
}

/**
 * lpfc_change_queue_depth - Alter scsi device queue depth
 * @sdev: Pointer the scsi device on which to change the queue depth.
 * @qdepth: New queue depth to set the sdev to.
 * @reason: The reason for the queue depth change.
 *
 * This function is called by the midlayer and the LLD to alter the queue
 * depth for a scsi device. This function sets the queue depth to the new
 * value and sends an event out to log the queue depth change.
 **/
int
lpfc_change_queue_depth(struct scsi_device *sdev, int qdepth, int reason)
{
	struct lpfc_vport *vport = (struct lpfc_vport *) sdev->host->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	struct lpfc_rport_data *rdata;
	unsigned long new_queue_depth, old_queue_depth;

	old_queue_depth = sdev->queue_depth;
	scsi_adjust_queue_depth(sdev, scsi_get_tag_type(sdev), qdepth);
	new_queue_depth = sdev->queue_depth;
	rdata = sdev->hostdata;
	if (rdata)
		lpfc_send_sdev_queuedepth_change_event(phba, vport,
						       rdata->pnode, sdev->lun,
						       old_queue_depth,
						       new_queue_depth);
	return sdev->queue_depth;
}

/**
 * lpfc_rampdown_queue_depth - Post RAMP_DOWN_QUEUE event to worker thread
 * @phba: The Hba for which this call is being executed.
 *
 * This routine is called when there is resource error in driver or firmware.
 * This routine posts WORKER_RAMP_DOWN_QUEUE event for @phba. This routine
 * posts at most 1 event each second. This routine wakes up worker thread of
 * @phba to process WORKER_RAM_DOWN_EVENT event.
 *
 * This routine should be called with no lock held.
 **/
void
lpfc_rampdown_queue_depth(struct lpfc_hba *phba)
{
	unsigned long flags;
	uint32_t evt_posted;

	spin_lock_irqsave(&phba->hbalock, flags);
	atomic_inc(&phba->num_rsrc_err);
	phba->last_rsrc_error_time = jiffies;

	if ((phba->last_ramp_down_time + QUEUE_RAMP_DOWN_INTERVAL) > jiffies) {
		spin_unlock_irqrestore(&phba->hbalock, flags);
		return;
	}

	phba->last_ramp_down_time = jiffies;

	spin_unlock_irqrestore(&phba->hbalock, flags);

	spin_lock_irqsave(&phba->pport->work_port_lock, flags);
	evt_posted = phba->pport->work_port_events & WORKER_RAMP_DOWN_QUEUE;
	if (!evt_posted)
		phba->pport->work_port_events |= WORKER_RAMP_DOWN_QUEUE;
	spin_unlock_irqrestore(&phba->pport->work_port_lock, flags);

	if (!evt_posted)
		lpfc_worker_wake_up(phba);
	return;
}

/**
 * lpfc_rampup_queue_depth - Post RAMP_UP_QUEUE event for worker thread
 * @phba: The Hba for which this call is being executed.
 *
 * This routine post WORKER_RAMP_UP_QUEUE event for @phba vport. This routine
 * post at most 1 event every 5 minute after last_ramp_up_time or
 * last_rsrc_error_time.  This routine wakes up worker thread of @phba
 * to process WORKER_RAM_DOWN_EVENT event.
 *
 * This routine should be called with no lock held.
 **/
static inline void
lpfc_rampup_queue_depth(struct lpfc_vport  *vport,
			uint32_t queue_depth)
{
	unsigned long flags;
	struct lpfc_hba *phba = vport->phba;
	uint32_t evt_posted;
	atomic_inc(&phba->num_cmd_success);

	if (vport->cfg_lun_queue_depth <= queue_depth)
		return;
	spin_lock_irqsave(&phba->hbalock, flags);
	if (time_before(jiffies,
			phba->last_ramp_up_time + QUEUE_RAMP_UP_INTERVAL) ||
	    time_before(jiffies,
			phba->last_rsrc_error_time + QUEUE_RAMP_UP_INTERVAL)) {
		spin_unlock_irqrestore(&phba->hbalock, flags);
		return;
	}
	phba->last_ramp_up_time = jiffies;
	spin_unlock_irqrestore(&phba->hbalock, flags);

	spin_lock_irqsave(&phba->pport->work_port_lock, flags);
	evt_posted = phba->pport->work_port_events & WORKER_RAMP_UP_QUEUE;
	if (!evt_posted)
		phba->pport->work_port_events |= WORKER_RAMP_UP_QUEUE;
	spin_unlock_irqrestore(&phba->pport->work_port_lock, flags);

	if (!evt_posted)
		lpfc_worker_wake_up(phba);
	return;
}

/**
 * lpfc_ramp_down_queue_handler - WORKER_RAMP_DOWN_QUEUE event handler
 * @phba: The Hba for which this call is being executed.
 *
 * This routine is called to  process WORKER_RAMP_DOWN_QUEUE event for worker
 * thread.This routine reduces queue depth for all scsi device on each vport
 * associated with @phba.
 **/
void
lpfc_ramp_down_queue_handler(struct lpfc_hba *phba)
{
	struct lpfc_vport **vports;
	struct Scsi_Host  *shost;
	struct scsi_device *sdev;
	unsigned long new_queue_depth;
	unsigned long num_rsrc_err, num_cmd_success;
	int i;

	num_rsrc_err = atomic_read(&phba->num_rsrc_err);
	num_cmd_success = atomic_read(&phba->num_cmd_success);

	vports = lpfc_create_vport_work_array(phba);
	if (vports != NULL)
		for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++) {
			shost = lpfc_shost_from_vport(vports[i]);
			shost_for_each_device(sdev, shost) {
				new_queue_depth =
					sdev->queue_depth * num_rsrc_err /
					(num_rsrc_err + num_cmd_success);
				if (!new_queue_depth)
					new_queue_depth = sdev->queue_depth - 1;
				else
					new_queue_depth = sdev->queue_depth -
								new_queue_depth;
				lpfc_change_queue_depth(sdev, new_queue_depth,
							0);
			}
		}
	lpfc_destroy_vport_work_array(phba, vports);
	atomic_set(&phba->num_rsrc_err, 0);
	atomic_set(&phba->num_cmd_success, 0);
}

/**
 * lpfc_ramp_up_queue_handler - WORKER_RAMP_UP_QUEUE event handler
 * @phba: The Hba for which this call is being executed.
 *
 * This routine is called to  process WORKER_RAMP_UP_QUEUE event for worker
 * thread.This routine increases queue depth for all scsi device on each vport
 * associated with @phba by 1. This routine also sets @phba num_rsrc_err and
 * num_cmd_success to zero.
 **/
void
lpfc_ramp_up_queue_handler(struct lpfc_hba *phba)
{
	struct lpfc_vport **vports;
	struct Scsi_Host  *shost;
	struct scsi_device *sdev;
	int i;

	vports = lpfc_create_vport_work_array(phba);
	if (vports != NULL)
		for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++) {
			shost = lpfc_shost_from_vport(vports[i]);
			shost_for_each_device(sdev, shost) {
				if (vports[i]->cfg_lun_queue_depth <=
				    sdev->queue_depth)
					continue;
				lpfc_change_queue_depth(sdev,
							sdev->queue_depth+1, 0);
			}
		}
	lpfc_destroy_vport_work_array(phba, vports);
	atomic_set(&phba->num_rsrc_err, 0);
	atomic_set(&phba->num_cmd_success, 0);
}

/**
 * lpfc_scsi_dev_block - set all scsi hosts to block state
 * @phba: Pointer to HBA context object.
 *
 * This function walks vport list and set each SCSI host to block state
 * by invoking fc_remote_port_delete() routine. This function is invoked
 * with EEH when device's PCI slot has been permanently disabled.
 **/
void
lpfc_scsi_dev_block(struct lpfc_hba *phba)
{
	struct lpfc_vport **vports;
	struct Scsi_Host  *shost;
	struct scsi_device *sdev;
	struct fc_rport *rport;
	int i;

	vports = lpfc_create_vport_work_array(phba);
	if (vports != NULL)
		for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++) {
			shost = lpfc_shost_from_vport(vports[i]);
			shost_for_each_device(sdev, shost) {
				rport = starget_to_rport(scsi_target(sdev));
				fc_remote_port_delete(rport);
			}
		}
	lpfc_destroy_vport_work_array(phba, vports);
}

void
lpfc_scsi_dev_rescan(struct lpfc_hba *phba)
{
	struct lpfc_vport **vports;
	struct Scsi_Host  *shost;
	int i;

	vports = lpfc_create_vport_work_array(phba);
	if (vports != NULL)
		for (i = 0; i <= phba->max_vpi && vports[i] != NULL; i++) {
			shost = lpfc_shost_from_vport(vports[i]);
			scsi_scan_host(shost);
		}
	lpfc_destroy_vport_work_array(phba, vports);
}

/**
 * lpfc_new_scsi_buf_s3 - Scsi buffer allocator for HBA with SLI3 IF spec
 * @vport: The virtual port for which this call being executed.
 * @num_to_allocate: The requested number of buffers to allocate.
 *
 * This routine allocates a scsi buffer for device with SLI-3 interface spec,
 * the scsi buffer contains all the necessary information needed to initiate
 * a SCSI I/O. The non-DMAable buffer region contains information to build
 * the IOCB. The DMAable region contains memory for the FCP CMND, FCP RSP,
 * and the initial BPL. In addition to allocating memory, the FCP CMND and
 * FCP RSP BDEs are setup in the BPL and the BPL BDE is setup in the IOCB.
 *
 * Return codes:
 *   int - number of scsi buffers that were allocated.
 *   0 = failure, less than num_to_alloc is a partial failure.
 **/
static int
lpfc_new_scsi_buf_s3(struct lpfc_vport *vport, int num_to_alloc)
{
	struct lpfc_hba *phba = vport->phba;
	struct lpfc_scsi_buf *psb;
	struct ulp_bde64 *bpl;
	IOCB_t *iocb;
	dma_addr_t pdma_phys_fcp_cmd;
	dma_addr_t pdma_phys_fcp_rsp;
	dma_addr_t pdma_phys_bpl;
	uint16_t iotag;
	int bcnt;

	for (bcnt = 0; bcnt < num_to_alloc; bcnt++) {
		psb = kzalloc(sizeof(struct lpfc_scsi_buf), GFP_KERNEL);
		if (!psb)
			break;

		/*
		 * Get memory from the pci pool to map the virt space to pci
		 * bus space for an I/O.  The DMA buffer includes space for the
		 * struct fcp_cmnd, struct fcp_rsp and the number of bde's
		 * necessary to support the sg_tablesize.
		 */
		psb->data = pci_pool_alloc(phba->lpfc_scsi_dma_buf_pool,
					GFP_KERNEL, &psb->dma_handle);
		if (!psb->data) {
			kfree(psb);
			break;
		}

		/* Initialize virtual ptrs to dma_buf region. */
		memset(psb->data, 0, phba->cfg_sg_dma_buf_size);

		/* Allocate iotag for psb->cur_iocbq. */
		iotag = lpfc_sli_next_iotag(phba, &psb->cur_iocbq);
		if (iotag == 0) {
			pci_pool_free(phba->lpfc_scsi_dma_buf_pool,
					psb->data, psb->dma_handle);
			kfree(psb);
			break;
		}
		psb->cur_iocbq.iocb_flag |= LPFC_IO_FCP;

		psb->fcp_cmnd = psb->data;
		psb->fcp_rsp = psb->data + sizeof(struct fcp_cmnd);
		psb->fcp_bpl = psb->data + sizeof(struct fcp_cmnd) +
			sizeof(struct fcp_rsp);

		/* Initialize local short-hand pointers. */
		bpl = psb->fcp_bpl;
		pdma_phys_fcp_cmd = psb->dma_handle;
		pdma_phys_fcp_rsp = psb->dma_handle + sizeof(struct fcp_cmnd);
		pdma_phys_bpl = psb->dma_handle + sizeof(struct fcp_cmnd) +
			sizeof(struct fcp_rsp);

		/*
		 * The first two bdes are the FCP_CMD and FCP_RSP. The balance
		 * are sg list bdes.  Initialize the first two and leave the
		 * rest for queuecommand.
		 */
		bpl[0].addrHigh = le32_to_cpu(putPaddrHigh(pdma_phys_fcp_cmd));
		bpl[0].addrLow = le32_to_cpu(putPaddrLow(pdma_phys_fcp_cmd));
		bpl[0].tus.f.bdeSize = sizeof(struct fcp_cmnd);
		bpl[0].tus.f.bdeFlags = BUFF_TYPE_BDE_64;
		bpl[0].tus.w = le32_to_cpu(bpl[0].tus.w);

		/* Setup the physical region for the FCP RSP */
		bpl[1].addrHigh = le32_to_cpu(putPaddrHigh(pdma_phys_fcp_rsp));
		bpl[1].addrLow = le32_to_cpu(putPaddrLow(pdma_phys_fcp_rsp));
		bpl[1].tus.f.bdeSize = sizeof(struct fcp_rsp);
		bpl[1].tus.f.bdeFlags = BUFF_TYPE_BDE_64;
		bpl[1].tus.w = le32_to_cpu(bpl[1].tus.w);

		/*
		 * Since the IOCB for the FCP I/O is built into this
		 * lpfc_scsi_buf, initialize it with all known data now.
		 */
		iocb = &psb->cur_iocbq.iocb;
		iocb->un.fcpi64.bdl.ulpIoTag32 = 0;
		iocb->un.fcpi64.bdl.bdeFlags = BUFF_TYPE_BLP_64;
		iocb->un.fcpi64.bdl.bdeSize =
				(2 * sizeof(struct ulp_bde64));
		iocb->un.fcpi64.bdl.addrLow =
				putPaddrLow(pdma_phys_bpl);
		iocb->un.fcpi64.bdl.addrHigh =
				putPaddrHigh(pdma_phys_bpl);
		iocb->ulpBdeCount = 1;
		iocb->ulpLe = 1;
		iocb->ulpClass = CLASS3;
		psb->status = IOSTAT_SUCCESS;
		/* Put it back into the SCSI buffer list */
		lpfc_release_scsi_buf_s3(phba, psb);
	}

	return bcnt;
}

/**
 * lpfc_sli4_fcp_xri_aborted - Fast-path process of fcp xri abort
 * @phba: pointer to lpfc hba data structure.
 * @axri: pointer to the fcp xri abort wcqe structure.
 *
 * This routine is invoked by the worker thread to process a SLI4 fast-path
 * FCP aborted xri.
 **/
void
lpfc_sli4_fcp_xri_aborted(struct lpfc_hba *phba,
			  struct sli4_wcqe_xri_aborted *axri)
{
	uint16_t xri = bf_get(lpfc_wcqe_xa_xri, axri);
	uint16_t rxid = bf_get(lpfc_wcqe_xa_remote_xid, axri);
	struct lpfc_scsi_buf *psb, *next_psb;
	unsigned long iflag = 0;
	struct lpfc_sglq *sglq_entry = NULL, *sglq_next = NULL;
	struct lpfc_iocbq *iocbq;
	int i;
	struct lpfc_nodelist *ndlp;
	int rrq_empty = 0;
	struct lpfc_sli_ring *pring = &phba->sli.ring[LPFC_ELS_RING];

	spin_lock_irqsave(&phba->hbalock, iflag);
	spin_lock(&phba->sli4_hba.abts_scsi_buf_list_lock);
	list_for_each_entry_safe(psb, next_psb,
		&phba->sli4_hba.lpfc_abts_scsi_buf_list, list) {
		if (psb->cur_iocbq.sli4_xritag == xri) {
			list_del(&psb->list);
			psb->exch_busy = 0;
			psb->status = IOSTAT_SUCCESS;
			spin_unlock(
				&phba->sli4_hba.abts_scsi_buf_list_lock);
			ndlp = psb->rdata->pnode;
			rrq_empty = list_empty(&phba->active_rrq_list);
			spin_unlock_irqrestore(&phba->hbalock, iflag);
			if (ndlp)
				lpfc_set_rrq_active(phba, ndlp, xri, rxid, 1);
			lpfc_release_scsi_buf_s4(phba, psb);
			if (rrq_empty)
				lpfc_worker_wake_up(phba);
			return;
		}
	}
	spin_unlock(&phba->sli4_hba.abts_scsi_buf_list_lock);
	for (i = 1; i <= phba->sli.last_iotag; i++) {
		iocbq = phba->sli.iocbq_lookup[i];

		if (!(iocbq->iocb_flag & LPFC_IO_FCP) ||
		    (iocbq->iocb_flag & LPFC_IO_LIBDFC))
			continue;
		if (iocbq->sli4_xritag != xri)
			continue;
		psb = container_of(iocbq, struct lpfc_scsi_buf, cur_iocbq);
		psb->exch_busy = 0;
		spin_unlock_irqrestore(&phba->hbalock, iflag);
		if (pring->txq_cnt)
			lpfc_worker_wake_up(phba);
		return;

	}

	/* For ESX4.0 we need to check the ELS list also */
	spin_lock(&phba->sli4_hba.abts_sgl_list_lock);
	list_for_each_entry_safe(sglq_entry, sglq_next,
		&phba->sli4_hba.lpfc_abts_els_sgl_list, list) {
		if (sglq_entry->sli4_xritag == xri) {
			list_del(&sglq_entry->list);
			spin_unlock(&phba->sli4_hba.abts_sgl_list_lock);

			sglq_entry->state = SGL_FREED;
			list_add_tail(&sglq_entry->list,
				&phba->sli4_hba.lpfc_sgl_list);
			spin_unlock_irqrestore(&phba->hbalock, iflag);
			return;
		}
	}
	spin_unlock(&phba->sli4_hba.abts_sgl_list_lock);
	sglq_entry = __lpfc_get_active_sglq(phba, xri);
	if (!sglq_entry || (sglq_entry->sli4_xritag != xri)) {
		spin_unlock_irqrestore(&phba->hbalock, iflag);
		return;
	}
	sglq_entry->state = SGL_XRI_ABORTED;
	spin_unlock_irqrestore(&phba->hbalock, iflag);
}

/**
 * lpfc_sli4_repost_scsi_sgl_list - Repsot the Scsi buffers sgl pages as block
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine walks the list of scsi buffers that have been allocated and
 * repost them to the HBA by using SGL block post. This is needed after a
 * pci_function_reset/warm_start or start. The lpfc_hba_down_post_s4 routine
 * is responsible for moving all scsi buffers on the lpfc_abts_scsi_sgl_list
 * to the lpfc_scsi_buf_list. If the repost fails, reject all scsi buffers.
 *
 * Returns: 0 = success, non-zero failure.
 **/
int
lpfc_sli4_repost_scsi_sgl_list(struct lpfc_hba *phba)
{
	struct lpfc_scsi_buf *psb;
	int index, status, bcnt = 0, rcnt = 0, rc = 0;
	LIST_HEAD(sblist);

	for (index = 0; index < phba->sli4_hba.scsi_xri_cnt; index++) {
		psb = phba->sli4_hba.lpfc_scsi_psb_array[index];
		if (psb) {
			/* Remove from SCSI buffer list */
			list_del(&psb->list);
			/* Add it to a local SCSI buffer list */
			list_add_tail(&psb->list, &sblist);
			if (++rcnt == LPFC_NEMBED_MBOX_SGL_CNT) {
				bcnt = rcnt;
				rcnt = 0;
			}
		} else
			/* A hole present in the XRI array, need to skip */
			bcnt = rcnt;

		if (index == phba->sli4_hba.scsi_xri_cnt - 1)
			/* End of XRI array for SCSI buffer, complete */
			bcnt = rcnt;

		/* Continue until collect up to a nembed page worth of sgls */
		if (bcnt == 0)
			continue;
		/* Now, post the SCSI buffer list sgls as a block */
		status = lpfc_sli4_post_scsi_sgl_block(phba, &sblist, bcnt);
		/* Reset SCSI buffer count for next round of posting */
		bcnt = 0;
		while (!list_empty(&sblist)) {
			list_remove_head(&sblist, psb, struct lpfc_scsi_buf,
					 list);
			if (status) {
				/* Put this back on the abort scsi list */
				psb->exch_busy = 1;
				rc++;
			} else {
				psb->exch_busy = 0;
				psb->status = IOSTAT_SUCCESS;
			}
			/* Put it back into the SCSI buffer list */
			lpfc_release_scsi_buf_s4(phba, psb);
		}
	}
	return rc;
}

/**
 * lpfc_new_scsi_buf_s4 - Scsi buffer allocator for HBA with SLI4 IF spec
 * @vport: The virtual port for which this call being executed.
 * @num_to_allocate: The requested number of buffers to allocate.
 *
 * This routine allocates a scsi buffer for device with SLI-4 interface spec,
 * the scsi buffer contains all the necessary information needed to initiate
 * a SCSI I/O.
 *
 * Return codes:
 *   int - number of scsi buffers that were allocated.
 *   0 = failure, less than num_to_alloc is a partial failure.
 **/
static int
lpfc_new_scsi_buf_s4(struct lpfc_vport *vport, int num_to_alloc)
{
	struct lpfc_hba *phba = vport->phba;
	struct lpfc_scsi_buf *psb;
	struct sli4_sge *sgl;
	IOCB_t *iocb;
	dma_addr_t pdma_phys_fcp_cmd;
	dma_addr_t pdma_phys_fcp_rsp;
	dma_addr_t pdma_phys_bpl, pdma_phys_bpl1;
	uint16_t iotag, last_xritag = NO_XRI;
	int status = 0, index;
	int bcnt;
	int non_sequential_xri = 0;
	LIST_HEAD(sblist);

	for (bcnt = 0; bcnt < num_to_alloc; bcnt++) {
		psb = kzalloc(sizeof(struct lpfc_scsi_buf), GFP_KERNEL);
		if (!psb)
			break;

		/*
		 * Get memory from the pci pool to map the virt space to pci bus
		 * space for an I/O.  The DMA buffer includes space for the
		 * struct fcp_cmnd, struct fcp_rsp and the number of bde's
		 * necessary to support the sg_tablesize.
		 */
		psb->data = pci_pool_alloc(phba->lpfc_scsi_dma_buf_pool,
						GFP_KERNEL, &psb->dma_handle);
		if (!psb->data) {
			kfree(psb);
			break;
		}

		/* Initialize virtual ptrs to dma_buf region. */
		memset(psb->data, 0, phba->cfg_sg_dma_buf_size);

		/* Allocate iotag for psb->cur_iocbq. */
		iotag = lpfc_sli_next_iotag(phba, &psb->cur_iocbq);
		if (iotag == 0) {
			pci_pool_free(phba->lpfc_scsi_dma_buf_pool,
				psb->data, psb->dma_handle);
			kfree(psb);
			break;
		}

		psb->cur_iocbq.sli4_xritag = lpfc_sli4_next_xritag(phba);
		if (psb->cur_iocbq.sli4_xritag == NO_XRI) {
			pci_pool_free(phba->lpfc_scsi_dma_buf_pool,
			      psb->data, psb->dma_handle);
			kfree(psb);
			break;
		}
		if (last_xritag != NO_XRI
			&& psb->cur_iocbq.sli4_xritag != (last_xritag+1)) {
			non_sequential_xri = 1;
		} else
			list_add_tail(&psb->list, &sblist);
		last_xritag = psb->cur_iocbq.sli4_xritag;

		index = phba->sli4_hba.scsi_xri_cnt++;
		psb->cur_iocbq.iocb_flag |= LPFC_IO_FCP;
		if (phba->cfg_use_mq == LPFC_ENABLE_MQ)
			psb->cur_iocbq.iocb_flag |= LPFC_USE_FCPWQIDX;

		psb->fcp_bpl = psb->data;
		psb->fcp_cmnd = (psb->data + phba->cfg_sg_dma_buf_size)
			- (sizeof(struct fcp_cmnd) + sizeof(struct fcp_rsp));
		psb->fcp_rsp = (struct fcp_rsp *)((uint8_t *)psb->fcp_cmnd +
					sizeof(struct fcp_cmnd));

		/* Initialize local short-hand pointers. */
		sgl = (struct sli4_sge *)psb->fcp_bpl;
		pdma_phys_bpl = psb->dma_handle;
		pdma_phys_fcp_cmd =
			(psb->dma_handle + phba->cfg_sg_dma_buf_size)
			 - (sizeof(struct fcp_cmnd) + sizeof(struct fcp_rsp));
		pdma_phys_fcp_rsp = pdma_phys_fcp_cmd + sizeof(struct fcp_cmnd);

		/*
		 * The first two bdes are the FCP_CMD and FCP_RSP.  The balance
		 * are sg list bdes.  Initialize the first two and leave the
		 * rest for queuecommand.
		 */
		sgl->addr_hi = cpu_to_le32(putPaddrHigh(pdma_phys_fcp_cmd));
		sgl->addr_lo = cpu_to_le32(putPaddrLow(pdma_phys_fcp_cmd));
		bf_set(lpfc_sli4_sge_last, sgl, 0);
		sgl->word2 = cpu_to_le32(sgl->word2);
		sgl->sge_len = cpu_to_le32(sizeof(struct fcp_cmnd));
		sgl++;

		/* Setup the physical region for the FCP RSP */
		sgl->addr_hi = cpu_to_le32(putPaddrHigh(pdma_phys_fcp_rsp));
		sgl->addr_lo = cpu_to_le32(putPaddrLow(pdma_phys_fcp_rsp));
		bf_set(lpfc_sli4_sge_last, sgl, 1);
		sgl->word2 = cpu_to_le32(sgl->word2);
		sgl->sge_len = cpu_to_le32(sizeof(struct fcp_rsp));

		/*
		 * Since the IOCB for the FCP I/O is built into this
		 * lpfc_scsi_buf, initialize it with all known data now.
		 */
		iocb = &psb->cur_iocbq.iocb;
		iocb->un.fcpi64.bdl.ulpIoTag32 = 0;
		iocb->un.fcpi64.bdl.bdeFlags = BUFF_TYPE_BDE_64;
		/* setting the BLP size to 2 * sizeof BDE may not be correct.
		 * We are setting the bpl to point to out sgl. An sgl's
		 * entries are 16 bytes, a bpl entries are 12 bytes.
		 */
		iocb->un.fcpi64.bdl.bdeSize = sizeof(struct fcp_cmnd);
		iocb->un.fcpi64.bdl.addrLow = putPaddrLow(pdma_phys_fcp_cmd);
		iocb->un.fcpi64.bdl.addrHigh = putPaddrHigh(pdma_phys_fcp_cmd);
		iocb->ulpBdeCount = 1;
		iocb->ulpLe = 1;
		iocb->ulpClass = CLASS3;
		if (phba->cfg_sg_dma_buf_size > SGL_PAGE_SIZE)
			pdma_phys_bpl1 = pdma_phys_bpl + SGL_PAGE_SIZE;
		else
			pdma_phys_bpl1 = 0;
		psb->dma_phys_bpl = pdma_phys_bpl;
		phba->sli4_hba.lpfc_scsi_psb_array[index] = psb;
		if (non_sequential_xri) {
			status = lpfc_sli4_post_sgl(phba, pdma_phys_bpl,
						pdma_phys_bpl1,
						psb->cur_iocbq.sli4_xritag);
			if (status) {
				/* Put this back on the abort scsi list */
				psb->exch_busy = 1;
			} else {
				psb->exch_busy = 0;
				psb->status = IOSTAT_SUCCESS;
			}
			/* Put it back into the SCSI buffer list */
			lpfc_release_scsi_buf_s4(phba, psb);
			break;
		}
	}
	if (bcnt) {
		status = lpfc_sli4_post_scsi_sgl_block(phba, &sblist, bcnt);
		/* Reset SCSI buffer count for next round of posting */
		while (!list_empty(&sblist)) {
			list_remove_head(&sblist, psb, struct lpfc_scsi_buf,
				 list);
			if (status) {
				/* Put this back on the abort scsi list */
				psb->exch_busy = 1;
			} else {
				psb->exch_busy = 0;
				psb->status = IOSTAT_SUCCESS;
			}
			/* Put it back into the SCSI buffer list */
			lpfc_release_scsi_buf_s4(phba, psb);
		}
	}

	return bcnt + non_sequential_xri;
}

/**
 * lpfc_new_scsi_buf - Wrapper funciton for scsi buffer allocator
 * @vport: The virtual port for which this call being executed.
 * @num_to_allocate: The requested number of buffers to allocate.
 *
 * This routine wraps the actual SCSI buffer allocator function pointer from
 * the lpfc_hba struct.
 *
 * Return codes:
 *   int - number of scsi buffers that were allocated.
 *   0 = failure, less than num_to_alloc is a partial failure.
 **/
static inline int
lpfc_new_scsi_buf(struct lpfc_vport *vport, int num_to_alloc)
{
	return vport->phba->lpfc_new_scsi_buf(vport, num_to_alloc);
}

/**
 * lpfc_get_scsi_buf_s3 - Get a scsi buffer from lpfc_scsi_buf_list of the HBA
 * @phba: The HBA for which this call is being executed.
 *
 * This routine removes a scsi buffer from head of @phba lpfc_scsi_buf_list list
 * and returns to caller.
 *
 * Return codes:
 *   NULL - Error
 *   Pointer to lpfc_scsi_buf - Success
 **/
static struct lpfc_scsi_buf*
lpfc_get_scsi_buf_s3(struct lpfc_hba *phba, struct lpfc_nodelist *ndlp)
{
	struct  lpfc_scsi_buf * lpfc_cmd = NULL;
	struct list_head *scsi_buf_list = &phba->lpfc_scsi_buf_list;
	unsigned long iflag = 0;

	spin_lock_irqsave(&phba->scsi_buf_list_lock, iflag);
	list_remove_head(scsi_buf_list, lpfc_cmd, struct lpfc_scsi_buf, list);
	spin_unlock_irqrestore(&phba->scsi_buf_list_lock, iflag);
	if (lpfc_cmd) {
		lpfc_cmd->seg_cnt = 0;
		lpfc_cmd->nonsg_phys = 0;
	}
	return  lpfc_cmd;
}
/**
 * lpfc_get_scsi_buf_s4 - Get a scsi buffer from lpfc_scsi_buf_list of the HBA
 * @phba: The HBA for which this call is being executed.
 *
 * This routine removes a scsi buffer from head of @phba lpfc_scsi_buf_list list
 * and returns to caller.
 *
 * Return codes:
 *   NULL - Error
 *   Pointer to lpfc_scsi_buf - Success
 **/
static struct lpfc_scsi_buf*
lpfc_get_scsi_buf_s4(struct lpfc_hba *phba, struct lpfc_nodelist *ndlp)
{
	struct lpfc_scsi_buf *lpfc_cmd ;
	unsigned long iflag = 0;
	int found = 0;

	spin_lock_irqsave(&phba->scsi_buf_list_lock, iflag);
	list_for_each_entry(lpfc_cmd, &phba->lpfc_scsi_buf_list,
							list) {
		if (lpfc_test_rrq_active(phba, ndlp,
					 lpfc_cmd->cur_iocbq.sli4_xritag))
			continue;
		list_del(&lpfc_cmd->list);
		found = 1;
		lpfc_cmd->seg_cnt = 0;
		lpfc_cmd->nonsg_phys = 0;
		break;
	}
	spin_unlock_irqrestore(&phba->scsi_buf_list_lock,
						 iflag);
	if (!found)
		return NULL;
	else
		return  lpfc_cmd;
}
/**
 * lpfc_get_scsi_buf - Get a scsi buffer from lpfc_scsi_buf_list of the HBA
 * @phba: The HBA for which this call is being executed.
 *
 * This routine removes a scsi buffer from head of @phba lpfc_scsi_buf_list list
 * and returns to caller.
 *
 * Return codes:
 *   NULL - Error
 *   Pointer to lpfc_scsi_buf - Success
 **/
static struct lpfc_scsi_buf*
lpfc_get_scsi_buf(struct lpfc_hba *phba, struct lpfc_nodelist *ndlp)
{
	return  phba->lpfc_get_scsi_buf(phba, ndlp);
}

/**
 * lpfc_release_scsi_buf - Return a scsi buffer back to hba scsi buf list
 * @phba: The Hba for which this call is being executed.
 * @psb: The scsi buffer which is being released.
 *
 * This routine releases @psb scsi buffer by adding it to tail of @phba
 * lpfc_scsi_buf_list list.
 **/
static void
lpfc_release_scsi_buf_s3(struct lpfc_hba *phba, struct lpfc_scsi_buf *psb)
{
	unsigned long iflag = 0;

	spin_lock_irqsave(&phba->scsi_buf_list_lock, iflag);
	psb->pCmd = NULL;
	list_add_tail(&psb->list, &phba->lpfc_scsi_buf_list);
	spin_unlock_irqrestore(&phba->scsi_buf_list_lock, iflag);
}

/**
 * lpfc_release_scsi_buf_s4 - DMA mapping for scsi buffer to SLI3 IF spec
 * @phba: The Hba for which this call is being executed.
 * @psb: The scsi buffer which is being released.
 *
 * This routine releases @psb scsi buffer by adding it to tail of @phba
 * lpfc_scsi_buf_list list. For SLI4 XRI's are tied to the scsi buffer
 * and cannot be reused for at least RA_TOV amount of time if it was
 * aborted.
 **/
static void
lpfc_release_scsi_buf_s4(struct lpfc_hba *phba, struct lpfc_scsi_buf *psb)
{
	unsigned long iflag = 0;

	if (psb->exch_busy) {
		spin_lock_irqsave(&phba->sli4_hba.abts_scsi_buf_list_lock,
					iflag);
		psb->pCmd = NULL;
		list_add_tail(&psb->list,
			&phba->sli4_hba.lpfc_abts_scsi_buf_list);
		spin_unlock_irqrestore(&phba->sli4_hba.abts_scsi_buf_list_lock,
					iflag);
	} else {

		spin_lock_irqsave(&phba->scsi_buf_list_lock, iflag);
		psb->pCmd = NULL;
		list_add_tail(&psb->list, &phba->lpfc_scsi_buf_list);
		spin_unlock_irqrestore(&phba->scsi_buf_list_lock, iflag);
	}
}

/**
 * lpfc_release_scsi_buf: Return a scsi buffer back to hba scsi buf list.
 * @phba: The Hba for which this call is being executed.
 * @psb: The scsi buffer which is being released.
 *
 * This routine releases @psb scsi buffer by adding it to tail of @phba
 * lpfc_scsi_buf_list list.
 **/
static void
lpfc_release_scsi_buf(struct lpfc_hba *phba, struct lpfc_scsi_buf *psb)
{

	phba->lpfc_release_scsi_buf(phba, psb);
}

/**
 * lpfc_scsi_prep_dma_buf_s3 - DMA mapping for scsi buffer to SLI3 IF spec
 * @phba: The Hba for which this call is being executed.
 * @lpfc_cmd: The scsi buffer which is going to be mapped.
 *
 * This routine does the pci dma mapping for scatter-gather list of scsi cmnd
 * field of @lpfc_cmd for device with SLI-3 interface spec. This routine scans
 * through sg elements and format the bdea. This routine also initializes all
 * IOCB fields which are dependent on scsi command request buffer.
 *
 * Return codes:
 *   1 - Error
 *   0 - Success
 **/
static int
lpfc_scsi_prep_dma_buf_s3(struct lpfc_hba *phba, struct lpfc_scsi_buf *lpfc_cmd)
{
	struct scsi_cmnd *scsi_cmnd = lpfc_cmd->pCmd;
	struct scatterlist *sgel = NULL;
	struct fcp_cmnd *fcp_cmnd = lpfc_cmd->fcp_cmnd;
	struct ulp_bde64 *bpl = lpfc_cmd->fcp_bpl;
	IOCB_t *iocb_cmd = &lpfc_cmd->cur_iocbq.iocb;
	uint32_t vpi = (lpfc_cmd->cur_iocbq.vport
			? lpfc_cmd->cur_iocbq.vport->vpi
			: 0);
	dma_addr_t physaddr;
	uint32_t i, num_bde = 0;
	int datadir = scsi_cmnd->sc_data_direction;
	int dma_error;

	/*
	 * There are three possibilities here - use scatter-gather segment, use
	 * the single mapping, or neither.  Start the lpfc command prep by
	 * bumping the bpl beyond the fcp_cmnd and fcp_rsp regions to the first
	 * data bde entry.
	 */
	bpl += 2;
	if (scsi_cmnd->use_sg) {
		/*
		 * The driver stores the segment count returned from pci_map_sg
		 * because this a count of dma-mappings used to map the use_sg
		 * pages.  They are not guaranteed to be the same for those
		 * architectures that implement an IOMMU.
		 */
		sgel = (struct scatterlist *)scsi_cmnd->request_buffer;
		lpfc_cmd->seg_cnt = dma_map_sg(&phba->pcidev->dev, sgel,
						scsi_cmnd->use_sg, datadir);
		if (lpfc_cmd->seg_cnt == 0)
			return 1;

		if (lpfc_cmd->seg_cnt > phba->cfg_sg_seg_cnt) {
			printk(KERN_ERR "%s: Too many sg segments from "
			       "dma_map_sg.  Config %d, seg_cnt %d\n",
			       __func__, phba->cfg_sg_seg_cnt,
			       lpfc_cmd->seg_cnt);
			dma_unmap_sg(&phba->pcidev->dev, sgel,
				     lpfc_cmd->seg_cnt, datadir);
			return 1;
		}

		/*
		 * The driver established a maximum scatter-gather segment count
		 * during probe that limits the number of sg elements in any
		 * single scsi command.  Just run through the seg_cnt and format
		 * the bde's.
		 */
		for (i = 0; i < lpfc_cmd->seg_cnt; i++) {
			physaddr = sg_dma_address(sgel);
			bpl->addrLow = le32_to_cpu(putPaddrLow(physaddr));
			bpl->addrHigh = le32_to_cpu(putPaddrHigh(physaddr));
			bpl->tus.f.bdeSize = sg_dma_len(sgel);
			if (datadir == DMA_TO_DEVICE)
				bpl->tus.f.bdeFlags = 0;
			else
				bpl->tus.f.bdeFlags = BUFF_TYPE_BDE_64I;
			bpl->tus.w = le32_to_cpu(bpl->tus.w);
			bpl++;
#if defined(__VMKLNX__)
			sgel = sg_next(sgel);
#else
			sgel++;
#endif
			num_bde++;

		}
	} else if (scsi_cmnd->request_buffer && scsi_cmnd->request_bufflen) {
		physaddr = dma_map_single(&phba->pcidev->dev,
					  scsi_cmnd->request_buffer,
					  scsi_cmnd->request_bufflen,
					  datadir);
		dma_error = dma_mapping_error(physaddr);
		if (dma_error) {
			lpfc_printf_log(phba, KERN_ERR, LOG_FCP,
					"(%d):0718 Unable to dma_map_single "
					"request_buffer: x%x\n",
					vpi, dma_error);
			return 1;
		}

		lpfc_cmd->nonsg_phys = physaddr;
		bpl->addrLow = le32_to_cpu(putPaddrLow(physaddr));
		bpl->addrHigh = le32_to_cpu(putPaddrHigh(physaddr));
		bpl->tus.f.bdeSize = scsi_cmnd->request_bufflen;
		if (datadir == DMA_TO_DEVICE)
			bpl->tus.f.bdeFlags = 0;
		else
			bpl->tus.f.bdeFlags = BUFF_TYPE_BDE_64I;
		bpl->tus.w = le32_to_cpu(bpl->tus.w);
		num_bde = 1;
		bpl++;
	}

	/*
	 * Finish initializing those IOCB fields that are dependent on the
	 * scsi_cmnd request_buffer.  Note that the bdeSize is explicitly
	 * reinitialized since all iocb memory resources are used many times
	 * for transmit, receive, and continuation bpl's.
	 */
	iocb_cmd->un.fcpi64.bdl.bdeSize = (2 * sizeof (struct ulp_bde64));
	iocb_cmd->un.fcpi64.bdl.bdeSize +=
		(num_bde * sizeof (struct ulp_bde64));
	iocb_cmd->ulpBdeCount = 1;
	iocb_cmd->ulpLe = 1;
	fcp_cmnd->fcpDl = cpu_to_be32(scsi_cmnd->request_bufflen);
	return 0;
}

/**
 * lpfc_scsi_prep_dma_buf_s4: DMA mapping for scsi buffer to SLI4 IF spec.
 * @phba: The Hba for which this call is being executed.
 * @lpfc_cmd: The scsi buffer which is going to be mapped.
 *
 * This routine does the pci dma mapping for scatter-gather list of scsi cmnd
 * field of @lpfc_cmd for device with SLI-4 interface spec.
 *
 * Return codes:
 * 	1 - Error
 * 	0 - Success
 **/
static int
lpfc_scsi_prep_dma_buf_s4(struct lpfc_hba *phba, struct lpfc_scsi_buf *lpfc_cmd)
{
	struct scsi_cmnd *scsi_cmnd = lpfc_cmd->pCmd;
	struct scatterlist *sgel = NULL;
	struct fcp_cmnd *fcp_cmnd = lpfc_cmd->fcp_cmnd;
	struct sli4_sge *sgl = (struct sli4_sge *)lpfc_cmd->fcp_bpl;
	IOCB_t *iocb_cmd = &lpfc_cmd->cur_iocbq.iocb;
	uint32_t vpi = (lpfc_cmd->cur_iocbq.vport
			? lpfc_cmd->cur_iocbq.vport->vpi
			: 0);
	dma_addr_t physaddr;
	uint32_t num_bde = 0;
	uint32_t dma_len = 0;
	uint32_t dma_offset = 0;
	int dma_error, nseg, datadir = scsi_cmnd->sc_data_direction;

	/*
	 * There are three possibilities here - use scatter-gather segment, use
	 * the single mapping, or neither.  Start the lpfc command prep by
	 * bumping the bpl beyond the fcp_cmnd and fcp_rsp regions to the first
	 * data bde entry.
	 */
	if (scsi_cmnd->use_sg) {
		/*
		 * The driver stores the segment count returned from pci_map_sg
		 * because this a count of dma-mappings used to map the use_sg
		 * pages.  They are not guaranteed to be the same for those
		 * architectures that implement an IOMMU.
		 */
		sgel = (struct scatterlist *)scsi_cmnd->request_buffer;
		nseg = dma_map_sg(&phba->pcidev->dev, sgel,
				  scsi_cmnd->use_sg, datadir);
		if (unlikely(!nseg))
			return 1;
		sgl += 1;
		/* clear the last flag in the fcp_rsp map entry */
		sgl->word2 = le32_to_cpu(sgl->word2);
		bf_set(lpfc_sli4_sge_last, sgl, 0);
		sgl->word2 = cpu_to_le32(sgl->word2);
		sgl += 1;

		lpfc_cmd->seg_cnt = nseg;
		if (lpfc_cmd->seg_cnt > phba->cfg_sg_seg_cnt) {
			printk(KERN_ERR "%s: Too many sg segments from "
			       "dma_map_sg.  Config %d, seg_cnt %d\n",
			       __func__, phba->cfg_sg_seg_cnt,
			       lpfc_cmd->seg_cnt);
				dma_unmap_sg(&phba->pcidev->dev, sgel,
						lpfc_cmd->seg_cnt, datadir);
			return 1;
		}

		/*
		 * The driver established a maximum scatter-gather segment count
		 * during probe that limits the number of sg elements in any
		 * single scsi command.  Just run through the seg_cnt and format
		 * the sge's.
		 * When using SLI-3 the driver will try to fit all the BDEs into
		 * the IOCB. If it can't then the BDEs get added to a BPL as it
		 * does for SLI-2 mode.
		 */
		sgel = (struct scatterlist *)scsi_cmnd->request_buffer;
		for (num_bde = 0; num_bde < nseg;) {
			physaddr = sg_dma_address(sgel);
			dma_len = sg_dma_len(sgel);
			sgl->addr_lo = cpu_to_le32(putPaddrLow(physaddr));
			sgl->addr_hi = cpu_to_le32(putPaddrHigh(physaddr));
			if ((num_bde + 1) == nseg)
				bf_set(lpfc_sli4_sge_last, sgl, 1);
			else
				bf_set(lpfc_sli4_sge_last, sgl, 0);
			bf_set(lpfc_sli4_sge_offset, sgl, dma_offset);
			sgl->word2 = cpu_to_le32(sgl->word2);
			sgl->sge_len = cpu_to_le32(dma_len);
			dma_offset += dma_len;
			sgl++;
#if defined(__VMKLNX__)
			sgel = sg_next(sgel);
#else
			sgel++;
#endif
			num_bde++;
		}
	} else if (scsi_cmnd->request_buffer && scsi_cmnd->request_bufflen) {
		physaddr = dma_map_single(&phba->pcidev->dev,
					  scsi_cmnd->request_buffer,
					  scsi_cmnd->request_bufflen,
					  datadir);
		dma_error = dma_mapping_error(physaddr);
		if (dma_error) {
			lpfc_printf_log(phba, KERN_ERR, LOG_FCP,
					"(%d):2729 Unable to dma_map_single "
					"request_buffer: x%x\n",
					vpi, dma_error);
			return 1;
		}
		sgl += 1;
		/* clear the last flag in the fcp_rsp map entry */
		sgl->word2 = le32_to_cpu(sgl->word2);
		bf_set(lpfc_sli4_sge_last, sgl, 0);
		sgl->word2 = cpu_to_le32(sgl->word2);
		sgl += 1;

		sgl->addr_lo = cpu_to_le32(putPaddrLow(physaddr));
		sgl->addr_hi = cpu_to_le32(putPaddrHigh(physaddr));
		bf_set(lpfc_sli4_sge_last, sgl, 1);
		bf_set(lpfc_sli4_sge_offset, sgl, dma_offset);
		sgl->word2 = cpu_to_le32(sgl->word2);
		sgl->sge_len = cpu_to_le32(scsi_cmnd->request_bufflen);

		lpfc_cmd->nonsg_phys = physaddr;
	} else {
		sgl += 1;
		/* clear the last flag in the fcp_rsp map entry */
		sgl->word2 = le32_to_cpu(sgl->word2);
		bf_set(lpfc_sli4_sge_last, sgl, 1);
		sgl->word2 = cpu_to_le32(sgl->word2);
	}

	/*
	 * Finish initializing those IOCB fields that are dependent on the
	 * scsi_cmnd request_buffer.  Note that for SLI-2 the bdeSize is
	 * explicitly reinitialized.
	 * all iocb memory resources are reused.
	 */
	fcp_cmnd->fcpDl = cpu_to_be32(scsi_cmnd->request_bufflen);

	/*
	 * Due to difference in data length between DIF/non-DIF paths,
	 * we need to set word 4 of IOCB here
	 */
	iocb_cmd->un.fcpi.fcpi_parm = scsi_cmnd->request_bufflen;
	return 0;
}

/**
 * lpfc_scsi_prep_dma_buf - Wrapper function for DMA mapping of scsi buffer
 * @phba: The Hba for which this call is being executed.
 * @lpfc_cmd: The scsi buffer which is going to be mapped.
 *
 * This routine wraps the actual DMA mapping function pointer from the
 * lpfc_hba struct.
 *
 * Return codes:
 * 	1 - Error
 * 	0 - Success
 **/
static inline int
lpfc_scsi_prep_dma_buf(struct lpfc_hba *phba, struct lpfc_scsi_buf *lpfc_cmd)
{
	return phba->lpfc_scsi_prep_dma_buf(phba, lpfc_cmd);
}

/**
 * lpfc_send_scsi_error_event - Posts an event when there is SCSI error
 * @phba: Pointer to hba context object.
 * @vport: Pointer to vport object.
 * @lpfc_cmd: Pointer to lpfc scsi command which reported the error.
 * @rsp_iocb: Pointer to response iocb object which reported error.
 *
 * This function posts an event when there is a SCSI command reporting
 * error from the scsi device.
 **/
static void
lpfc_send_scsi_error_event(struct lpfc_hba *phba, struct lpfc_vport *vport,
		struct lpfc_scsi_buf *lpfc_cmd, struct lpfc_iocbq *rsp_iocb) {
	struct scsi_cmnd *cmnd = lpfc_cmd->pCmd;
	struct fcp_rsp *fcprsp = lpfc_cmd->fcp_rsp;
	uint32_t resp_info = fcprsp->rspStatus2;
	uint32_t scsi_status = fcprsp->rspStatus3;
	uint32_t fcpi_parm = rsp_iocb->iocb.un.fcpi.fcpi_parm;
	struct lpfc_fast_path_event *fast_path_evt = NULL;
	struct lpfc_nodelist *pnode = lpfc_cmd->rdata->pnode;
	unsigned long flags;

	if (!pnode || !NLP_CHK_NODE_ACT(pnode))
		return;

	/* If there is queuefull or busy condition send a scsi event */
	if ((cmnd->result == SAM_STAT_TASK_SET_FULL) ||
		(cmnd->result == SAM_STAT_BUSY)) {
		fast_path_evt = lpfc_alloc_fast_evt(phba);
		if (!fast_path_evt)
			return;
		fast_path_evt->un.scsi_evt.event_type =
			FC_REG_SCSI_EVENT;
		fast_path_evt->un.scsi_evt.subcategory =
		(cmnd->result == SAM_STAT_TASK_SET_FULL) ?
		LPFC_EVENT_QFULL : LPFC_EVENT_DEVBSY;
		fast_path_evt->un.scsi_evt.lun = cmnd->device->lun;
		memcpy(&fast_path_evt->un.scsi_evt.wwpn,
			&pnode->nlp_portname, sizeof(struct lpfc_name));
		memcpy(&fast_path_evt->un.scsi_evt.wwnn,
			&pnode->nlp_nodename, sizeof(struct lpfc_name));
	} else if ((resp_info & SNS_LEN_VALID) && fcprsp->rspSnsLen) {
		fast_path_evt = lpfc_alloc_fast_evt(phba);
		if (!fast_path_evt)
			return;
		fast_path_evt->un.check_cond_evt.scsi_event.event_type =
			FC_REG_SCSI_EVENT;
		fast_path_evt->un.check_cond_evt.scsi_event.subcategory =
			LPFC_EVENT_CHECK_COND;
		fast_path_evt->un.check_cond_evt.scsi_event.lun =
			cmnd->device->lun;
		memcpy(&fast_path_evt->un.check_cond_evt.scsi_event.wwpn,
			&pnode->nlp_portname, sizeof(struct lpfc_name));
		memcpy(&fast_path_evt->un.check_cond_evt.scsi_event.wwnn,
			&pnode->nlp_nodename, sizeof(struct lpfc_name));
		fast_path_evt->un.check_cond_evt.sense_key =
			cmnd->sense_buffer[2] & 0xf;
		fast_path_evt->un.check_cond_evt.asc = cmnd->sense_buffer[12];
		fast_path_evt->un.check_cond_evt.ascq = cmnd->sense_buffer[13];
	} else if ((cmnd->sc_data_direction == DMA_FROM_DEVICE) &&
		     fcpi_parm &&
		     ((be32_to_cpu(fcprsp->rspResId) != fcpi_parm) ||
			((scsi_status == SAM_STAT_GOOD) &&
			!(resp_info & (RESID_UNDER | RESID_OVER))))) {
		/*
		 * If status is good or resid does not match with fcp_param and
		 * there is valid fcpi_parm, then there is a read_check error
		 */
		fast_path_evt = lpfc_alloc_fast_evt(phba);
		if (!fast_path_evt)
			return;
		fast_path_evt->un.read_check_error.header.event_type =
			FC_REG_FABRIC_EVENT;
		/* SanDiag change to FABRIC Event type */
		fast_path_evt->un.read_check_error.header.subcategory =
			LPFC_EVENT_FCPRDCHKERR;
		memcpy(&fast_path_evt->un.read_check_error.header.wwpn,
			&pnode->nlp_portname, sizeof(struct lpfc_name));
		memcpy(&fast_path_evt->un.read_check_error.header.wwnn,
			&pnode->nlp_nodename, sizeof(struct lpfc_name));
		fast_path_evt->un.read_check_error.lun = cmnd->device->lun;
		fast_path_evt->un.read_check_error.opcode = cmnd->cmnd[0];
		fast_path_evt->un.read_check_error.fcpiparam =
			fcpi_parm;
	} else
		return;

	fast_path_evt->vport = vport;
	spin_lock_irqsave(&phba->hbalock, flags);
	list_add_tail(&fast_path_evt->work_evt.evt_listp, &phba->work_list);
	spin_unlock_irqrestore(&phba->hbalock, flags);
	lpfc_worker_wake_up(phba);
	return;
}

/**
 * lpfc_scsi_unprep_dma_buf - Un-map DMA mapping of SG-list for dev
 * @phba: The HBA for which this call is being executed.
 * @psb: The scsi buffer which is going to be un-mapped.
 *
 * This routine does DMA un-mapping of scatter gather list of scsi command
 * field of @lpfc_cmd for device with SLI-3 interface spec.
 **/
static void
lpfc_scsi_unprep_dma_buf(struct lpfc_hba *phba, struct lpfc_scsi_buf *psb)
{
	/*
	 * There are only two special cases to consider.  (1) the scsi command
	 * requested scatter-gather usage or (2) the scsi command allocated
	 * a request buffer, but did not request use_sg.  There is a third
	 * case, but it does not require resource deallocation.
	 */
	if ((psb->seg_cnt > 0) && (psb->pCmd->use_sg)) {
		dma_unmap_sg(&phba->pcidev->dev, psb->pCmd->request_buffer,
				psb->seg_cnt, psb->pCmd->sc_data_direction);
	} else {
		 if ((psb->nonsg_phys) && (psb->pCmd->request_bufflen)) {
			dma_unmap_single(&phba->pcidev->dev, psb->nonsg_phys,
						psb->pCmd->request_bufflen,
						psb->pCmd->sc_data_direction);
		 }
	}
}

/**
 * lpfc_handler_fcp_err - FCP response handler
 * @vport: The virtual port for which this call is being executed.
 * @lpfc_cmd: Pointer to lpfc_scsi_buf data structure.
 * @rsp_iocb: The response IOCB which contains FCP error.
 *
 * This routine is called to process response IOCB with status field
 * IOSTAT_FCP_RSP_ERROR. This routine sets result field of scsi command
 * based upon SCSI and FCP error.
 **/
static void
lpfc_handle_fcp_err(struct lpfc_hba *phba, struct lpfc_vport *vport,
		    struct lpfc_scsi_buf *lpfc_cmd, struct lpfc_iocbq *rsp_iocb)
{
	struct scsi_cmnd *cmnd = lpfc_cmd->pCmd;
	struct fcp_cmnd *fcpcmd = lpfc_cmd->fcp_cmnd;
	struct fcp_rsp *fcprsp = lpfc_cmd->fcp_rsp;
	uint32_t fcpi_parm = rsp_iocb->iocb.un.fcpi.fcpi_parm;
	uint32_t resp_info = fcprsp->rspStatus2;
	uint32_t scsi_status = fcprsp->rspStatus3;
	uint32_t *lp;
	uint32_t host_status = DID_OK;
	uint32_t rsplen = 0;
	uint32_t logit = LOG_FCP | LOG_FCP_ERROR;


	/*
	 *  If this is a task management command, there is no
	 *  scsi packet associated with this lpfc_cmd.  The driver
	 *  consumes it.
	 */
	if (fcpcmd->fcpCntl2) {
		scsi_status = 0;
		goto out;
	}

	if (resp_info & RSP_LEN_VALID) {
		rsplen = be32_to_cpu(fcprsp->rspRspLen);
		if (rsplen != 0 && rsplen != 4 && rsplen != 8) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_FCP,
				 "2719 Invalid response length: "
				 "tgt x%x lun x%x cmnd x%x rsplen x%x, "
				 "rspInfo3 x%x\n",
				 cmnd->device->id,
				 cmnd->device->lun, cmnd->cmnd[0],
				 rsplen, fcprsp->rspInfo3);
			host_status = DID_BUS_BUSY;
			goto out;
		}
		if (fcprsp->rspInfo3 != RSP_NO_FAILURE) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_FCP,
				 "2757 Protocol failure detected during "
				 "processing of FCP I/O op: "
				 "tgt x%x lun x%x cmnd x%x rspInfo3 x%x\n",
				 cmnd->device->id,
				 cmnd->device->lun, cmnd->cmnd[0],
				 fcprsp->rspInfo3);
			host_status = DID_BUS_BUSY;
			goto out;
		}
	}

	if ((resp_info & SNS_LEN_VALID) && fcprsp->rspSnsLen) {
		uint32_t snslen = be32_to_cpu(fcprsp->rspSnsLen);
		if (snslen > SCSI_SENSE_BUFFERSIZE)
			snslen = SCSI_SENSE_BUFFERSIZE;

		if (resp_info & RSP_LEN_VALID)
		  rsplen = be32_to_cpu(fcprsp->rspRspLen);
		memcpy(cmnd->sense_buffer, &fcprsp->rspInfo0 + rsplen, snslen);
	}
	lp = (uint32_t *)cmnd->sense_buffer;

	if (!scsi_status && (resp_info & RESID_UNDER))
		logit = LOG_FCP;

	lpfc_printf_log(phba, KERN_WARNING, logit,
			"(%d):0730 FCP command x%x failed: x%x SNS x%x x%x "
			"Data: x%x x%x x%x x%x x%x\n",
			(vport ? vport->vpi : 0),
			cmnd->cmnd[0], scsi_status,
			be32_to_cpu(*lp), be32_to_cpu(*(lp + 3)), resp_info,
			be32_to_cpu(fcprsp->rspResId),
			be32_to_cpu(fcprsp->rspSnsLen),
			be32_to_cpu(fcprsp->rspRspLen),
			fcprsp->rspInfo3);

	cmnd->resid = 0;
	if (resp_info & RESID_UNDER) {
		cmnd->resid = be32_to_cpu(fcprsp->rspResId);

		lpfc_printf_log(phba, KERN_INFO, LOG_FCP,
				"(%d):0716 FCP Read Underrun, expected %d, "
				"residual %d Data: x%x x%x x%x\n",
				(vport ? vport->vpi : 0),
				be32_to_cpu(fcpcmd->fcpDl),
				cmnd->resid, fcpi_parm, cmnd->cmnd[0],
				cmnd->underflow);
		/*
		 * If there is an under run check if under run reported by
		 * storage array is same as the under run reported by HBA.
		 * If this is not same, there is a dropped frame.
		 */
		if ((cmnd->sc_data_direction == DMA_FROM_DEVICE) &&
			fcpi_parm &&
			(cmnd->resid != fcpi_parm)) {
			lpfc_printf_log(phba, KERN_WARNING,
					LOG_FCP | LOG_FCP_ERROR,
					"(%d):0735 FCP Read Check Error "
					"and Underrun Data: x%x x%x x%x x%x\n",
					(vport ? vport->vpi : 0),
					be32_to_cpu(fcpcmd->fcpDl),
					cmnd->resid, fcpi_parm,
					cmnd->cmnd[0]);
			cmnd->resid = cmnd->request_bufflen;
			host_status = DID_BUS_BUSY;
		}
		/*
		 * The cmnd->underflow is the minimum number of bytes that must
		 * be transfered for this command.  Provided a sense condition
		 * is not present, make sure the actual amount transferred is at
		 * least the underflow value or fail.
		 */
		if (!(resp_info & SNS_LEN_VALID) &&
		    (scsi_status == SAM_STAT_GOOD) &&
		    (cmnd->request_bufflen - cmnd->resid) < cmnd->underflow) {
			lpfc_printf_log(phba, KERN_ERR, LOG_FCP,
					"(%d):0717 FCP command x%x residual "
					"underrun converted to error "
					"Data: x%x x%x x%x\n",
					(vport ? vport->vpi : 0),
					cmnd->cmnd[0], cmnd->request_bufflen,
					cmnd->resid, cmnd->underflow);
			host_status = DID_ERROR;
		}
	} else if (resp_info & RESID_OVER) {
		lpfc_printf_log(phba, KERN_ERR, LOG_FCP,
				"(%d):0720 FCP command x%x residual "
				"overrun error. Data: x%x x%x\n",
				(vport ? vport->vpi : 0),
				cmnd->cmnd[0],
				cmnd->request_bufflen, cmnd->resid);
		host_status = DID_ERROR;

	/*
	 * Check SLI validation that all the transfer was actually done
	 * (fcpi_parm should be zero). Apply check only to reads.
	 */
	} else if (fcpi_parm && (cmnd->sc_data_direction == DMA_FROM_DEVICE)) {
		lpfc_printf_vlog(vport, KERN_WARNING, LOG_FCP | LOG_FCP_ERROR,
				 "9029 FCP Read Check Error Data: "
				 "x%x x%x x%x x%x x%x\n",
				 be32_to_cpu(fcpcmd->fcpDl),
				 be32_to_cpu(fcprsp->rspResId),
				 fcpi_parm, cmnd->cmnd[0], scsi_status);
		switch (scsi_status) {
		case SAM_STAT_GOOD:
		case SAM_STAT_CHECK_CONDITION:
			/* Fabric dropped a data frame. Fail any successful 
			 * command in which we detected dropped frames. 
			 * A status of good or some check conditions could
			 * be considered a successful command.
			 */
			host_status = DID_BUS_BUSY;
			break;
		}
		cmnd->resid = cmnd->request_bufflen;
	}

 out:
	lpfc_send_scsi_error_event(phba, vport, lpfc_cmd, rsp_iocb);
	cmnd->result = ScsiResult(host_status, scsi_status);
}

/**
 * lpfc_scsi_cmd_iocb_cmpl - Scsi cmnd IOCB completion routine
 * @phba: The Hba for which this call is being executed.
 * @pIocbIn: The command IOCBQ for the scsi cmnd.
 * @pIocbOut: The response IOCBQ for the scsi cmnd.
 *
 * This routine assigns scsi command result by looking into response IOCB
 * status field appropriately. This routine handles QUEUE FULL condition as
 * well by ramping down device queue depth.
 **/
static void
lpfc_scsi_cmd_iocb_cmpl(struct lpfc_hba *phba, struct lpfc_iocbq *pIocbIn,
			struct lpfc_iocbq *pIocbOut)
{
	struct lpfc_scsi_buf *lpfc_cmd =
		(struct lpfc_scsi_buf *) pIocbIn->context1;
	struct lpfc_vport      *vport = pIocbIn->vport;
	struct lpfc_rport_data *rdata = lpfc_cmd->rdata;
	struct lpfc_nodelist *pnode = rdata->pnode;
	struct scsi_cmnd *cmd;
	int result;
	struct scsi_device *tmp_sdev;
	int depth;
	unsigned long flags;
	struct lpfc_fast_path_event *fast_path_evt;
	struct Scsi_Host *shost;
	uint32_t queue_depth, scsi_id;

	/*
	 * This clause does fault injection for SAN Diag testing
	 */
	if (unlikely(sd_test)) {
		if (likely(IOSTAT_SUCCESS == pIocbOut->iocb.ulpStatus))
			dfc_inject_sdevent(phba, pIocbIn, pIocbOut);
	}

	/* Sanity check on return of outstanding command */
	if (!(lpfc_cmd->pCmd))
		return;
	cmd = lpfc_cmd->pCmd;
	shost = cmd->device->host;

	lpfc_cmd->result = pIocbOut->iocb.un.ulpWord[4];
	lpfc_cmd->status = pIocbOut->iocb.ulpStatus;
	/* pick up SLI4 exhange busy status from HBA */
	lpfc_cmd->exch_busy = pIocbOut->iocb_flag & LPFC_EXCHANGE_BUSY;

	if (pnode && NLP_CHK_NODE_ACT(pnode))
		atomic_dec(&pnode->cmd_pending);

	if (likely(IOSTAT_SUCCESS == lpfc_cmd->status)) {
		cmd->result = ScsiResult(DID_OK, 0);
		goto fast_scsi_cmd_iocb_cmpl;
	}
	if (lpfc_cmd->status) {
		if (lpfc_cmd->status == IOSTAT_LOCAL_REJECT &&
		    (lpfc_cmd->result & IOERR_DRVR_MASK))
			lpfc_cmd->status = IOSTAT_DRIVER_REJECT;
		else if (lpfc_cmd->status >= IOSTAT_CNT)
			lpfc_cmd->status = IOSTAT_DEFAULT;

		lpfc_printf_log(phba, KERN_WARNING, LOG_FCP,
				 "0729 FCP cmd x%x failed <%d/%d> "
				 "status: x%x result: x%x Data: x%x x%x\n",
				 cmd->cmnd[0],
				 vport ? vport->vpi : 0,
				 cmd->device ? cmd->device->lun : 0xffff,
				 lpfc_cmd->status, lpfc_cmd->result,
				 pIocbOut->iocb.ulpContext,
				 lpfc_cmd->cur_iocbq.iocb.ulpIoTag);

		switch (lpfc_cmd->status) {
		case IOSTAT_FCP_RSP_ERROR:
			if (pnode && NLP_CHK_NODE_ACT(pnode))
				pnode->fcperr_cnt++;

			/* Call FCP RSP handler to determine result */
			lpfc_handle_fcp_err(phba, vport, lpfc_cmd, pIocbOut);
			break;

		case IOSTAT_NPORT_BSY:
		case IOSTAT_FABRIC_BSY:
			cmd->result = ScsiResult(DID_TRANSPORT_DISRUPTED, 0);
			fast_path_evt = lpfc_alloc_fast_evt(phba);
			if (!fast_path_evt)
				break;
			fast_path_evt->un.fabric_evt.event_type =
				FC_REG_FABRIC_EVENT;
			fast_path_evt->un.fabric_evt.subcategory =
				(lpfc_cmd->status == IOSTAT_NPORT_BSY) ?
				LPFC_EVENT_PORT_BUSY : LPFC_EVENT_FABRIC_BUSY;
			if (pnode && NLP_CHK_NODE_ACT(pnode)) {
				memcpy(&fast_path_evt->un.fabric_evt.wwpn,
					&pnode->nlp_portname,
					sizeof(struct lpfc_name));
				memcpy(&fast_path_evt->un.fabric_evt.wwnn,
					&pnode->nlp_nodename,
					sizeof(struct lpfc_name));
			}
			fast_path_evt->vport = vport;
			fast_path_evt->work_evt.evt =
				LPFC_EVT_FASTPATH_MGMT_EVT;
			spin_lock_irqsave(&phba->hbalock, flags);
			list_add_tail(&fast_path_evt->work_evt.evt_listp,
				&phba->work_list);
			spin_unlock_irqrestore(&phba->hbalock, flags);
			lpfc_worker_wake_up(phba);
			break;
		case IOSTAT_LOCAL_REJECT:
			if (pnode && NLP_CHK_NODE_ACT(pnode))
				switch (lpfc_cmd->result) {
				case IOERR_ABORT_REQUESTED:
					pnode->abts_cnt++;
					break;
				case IOERR_SEQUENCE_TIMEOUT:
					pnode->tmo_cnt++;
					break;
				case IOERR_INVALID_RPI:
					pnode->invld_rpi_cnt++;
					break;
				case IOERR_NO_RESOURCES:
					pnode->no_rsrc_cnt++;
					break;
				default:
					pnode->rjt_cnt++;
				}
			if (lpfc_cmd->result == IOERR_INVALID_RPI ||
			    lpfc_cmd->result == IOERR_NO_RESOURCES ||
			    lpfc_cmd->result == IOERR_ABORT_REQUESTED ||
			    lpfc_cmd->result == IOERR_SLER_CMD_RCV_FAILURE) {
				cmd->result = ScsiResult(DID_REQUEUE, 0);
				break;
			} /* else: fall through */

		default:
			cmd->result = ScsiResult(DID_BUS_BUSY, 0);
			break;
		}

		if (!pnode || !NLP_CHK_NODE_ACT(pnode)
		    || (pnode->nlp_state != NLP_STE_MAPPED_NODE))
			cmd->result = ScsiResult(DID_TRANSPORT_DISRUPTED,
			SAM_STAT_BUSY);
	} else {
		cmd->result = ScsiResult(DID_OK, 0);
	}

	if (cmd->result || lpfc_cmd->fcp_rsp->rspSnsLen) {
		uint32_t *lp = (uint32_t *)cmd->sense_buffer;
		lpfc_printf_log(phba, KERN_INFO, LOG_FCP,
				 "0710 Iodone <%d/%d> cmd %p, error "
				 "x%x SNS x%x x%x Data: x%x x%x\n",
				 vport ? vport->vpi : 0, cmd->device->lun, cmd,
				 cmd->result, *lp, *(lp + 3), cmd->retries,
				 cmd->resid);
	}

 fast_scsi_cmd_iocb_cmpl:
	if (vport->stat_data_enabled)
		lpfc_update_stats(phba, lpfc_cmd);
	result = cmd->result;
	if (vport->cfg_max_scsicmpl_time &&
	   time_after(jiffies, lpfc_cmd->start_time +
		msecs_to_jiffies(vport->cfg_max_scsicmpl_time))) {
		spin_lock_irqsave(shost->host_lock, flags);
		if (pnode && NLP_CHK_NODE_ACT(pnode)) {
			if (pnode->cmd_qdepth >
				atomic_read(&pnode->cmd_pending) &&
				(atomic_read(&pnode->cmd_pending) >
				LPFC_MIN_TGT_QDEPTH) &&
				((cmd->cmnd[0] == READ_10) ||
				(cmd->cmnd[0] == WRITE_10)))
				pnode->cmd_qdepth =
					atomic_read(&pnode->cmd_pending);

			pnode->last_change_time = jiffies;
		}
		spin_unlock_irqrestore(shost->host_lock, flags);
	} else if (pnode && NLP_CHK_NODE_ACT(pnode)) {
		if ((pnode->cmd_qdepth < vport->cfg_tgt_queue_depth) &&
		   time_after(jiffies, pnode->last_change_time +
			      msecs_to_jiffies(LPFC_TGTQ_INTERVAL))) {
			spin_lock_irqsave(shost->host_lock, flags);
			depth = pnode->cmd_qdepth * LPFC_TGTQ_RAMPUP_PCENT
				/ 100;
			depth = depth ? depth : 1;
			pnode->cmd_qdepth += depth;
			if (pnode->cmd_qdepth > vport->cfg_tgt_queue_depth)
				pnode->cmd_qdepth = vport->cfg_tgt_queue_depth;
			pnode->last_change_time = jiffies;
			spin_unlock_irqrestore(shost->host_lock, flags);
		}
	}

	/* The sdev is not guaranteed to be valid post scsi_done upcall. */
	lpfc_scsi_unprep_dma_buf(phba, lpfc_cmd);
	queue_depth = cmd->device->queue_depth;
	scsi_id = cmd->device->id;
	spin_lock_irqsave(&phba->eh_waitlock, flags);
	lpfc_cmd->pCmd = NULL;
	spin_unlock_irqrestore(&phba->eh_waitlock, flags);
#if defined(__VMKLNX__)
	if (unlikely((cmd->result & SAM_STAT_CHECK_CONDITION) &&
		     (!cmd->sense_buffer[0]))) {
		struct fcp_rsp *fcprsp = lpfc_cmd->fcp_rsp;
		uint32_t resp_info = fcprsp->rspStatus2;
		uint32_t scsi_status = fcprsp->rspStatus3;
		uint32_t *lp = (uint32_t *)cmd->sense_buffer;

		lpfc_printf_log(phba, KERN_ERR, LOG_FCP,
			"0731 Invalid sense info (%d) FCP command x%x "
			"failed on <%d/%d>, x%x SNS x%x x%x "
			"Data: x%x x%x x%x x%x x%x\n",
			(vport ? vport->vpi : 0),
			cmd->cmnd[0], cmd->device->id, cmd->device->lun,
			scsi_status,
			be32_to_cpu(*lp), be32_to_cpu(*(lp + 3)), resp_info,
			be32_to_cpu(fcprsp->rspResId),
			be32_to_cpu(fcprsp->rspSnsLen),
			be32_to_cpu(fcprsp->rspRspLen),
			fcprsp->rspInfo3);

		cmd->sense_buffer[0] = 0x70;
		cmd->sense_buffer[2] = 0x0;
	}
#endif
	cmd->scsi_done(cmd);

	if (phba->cfg_poll & ENABLE_FCP_RING_POLLING) {
		/*
		 * If there is a thread waiting for command completion
		 * wake up the thread.
		 */
		spin_lock_irqsave(shost->host_lock, flags);
		lpfc_cmd->pCmd = NULL;
		if (lpfc_cmd->waitq)
			wake_up(lpfc_cmd->waitq);
		spin_unlock_irqrestore(shost->host_lock, flags);
		lpfc_release_scsi_buf(phba, lpfc_cmd);
		return;
	}

	if (!vport)
		goto out_vport_deleted;

	if (!result)
		lpfc_rampup_queue_depth(vport, queue_depth);

	if (!result && pnode && NLP_CHK_NODE_ACT(pnode) &&
	   ((jiffies - pnode->last_ramp_up_time) >
		LPFC_Q_RAMP_UP_INTERVAL * HZ) &&
	   ((jiffies - pnode->last_q_full_time) >
		LPFC_Q_RAMP_UP_INTERVAL * HZ) &&
	   (vport->cfg_lun_queue_depth > queue_depth)) {
		shost_for_each_device(tmp_sdev, shost) {
			if (vport->cfg_lun_queue_depth > tmp_sdev->queue_depth){
				if (tmp_sdev->id != scsi_id)
					continue;
				if (tmp_sdev->ordered_tags)
					scsi_adjust_queue_depth(tmp_sdev,
						MSG_ORDERED_TAG,
						tmp_sdev->queue_depth+1);
				else
					scsi_adjust_queue_depth(tmp_sdev,
						MSG_SIMPLE_TAG,
						tmp_sdev->queue_depth+1);

				pnode->last_ramp_up_time = jiffies;
			}
			lpfc_send_sdev_queuedepth_change_event(phba, vport,
							       pnode,
							       tmp_sdev->lun,
							       queue_depth,
							       queue_depth+1);
		}
	}

	/*
	 * Check for queue full.  If the lun is reporting queue full, then
	 * back off the lun queue depth to prevent target overloads.
	 */
	if (result == SAM_STAT_TASK_SET_FULL && pnode &&
	    NLP_CHK_NODE_ACT(pnode)) {
		pnode->last_q_full_time = jiffies;

		shost_for_each_device(tmp_sdev, shost) {
			if (tmp_sdev->id != scsi_id)
				continue;
			depth = scsi_track_queue_full(tmp_sdev,
						      tmp_sdev->queue_depth-1);
			if (depth <= 0)
				continue;
			lpfc_printf_vlog(vport, KERN_WARNING, LOG_FCP,
					 "0711 detected queue full - lun queue "
					 "depth adjusted to %d.\n", depth);
			lpfc_send_sdev_queuedepth_change_event(phba, vport,
							       pnode,
							       tmp_sdev->lun,
							       depth+1, depth);
		}
	}

 out_vport_deleted:
	/*
	 * If there is a thread waiting for command completion
	 * wake up the thread.
	 */
	spin_lock_irqsave(&phba->eh_waitlock, flags);
	if (lpfc_cmd->waitq)
		wake_up(lpfc_cmd->waitq);
	spin_unlock_irqrestore(&phba->eh_waitlock, flags);

	lpfc_release_scsi_buf(phba, lpfc_cmd);
}

/**
 * lpfc_scsi_prep_cmnd - Wrapper func for convert scsi cmnd to FCP info unit
 * @vport: The virtual port for which this call is being executed.
 * @lpfc_cmd: The scsi command which needs to send.
 * @pnode: Pointer to lpfc_nodelist.
 *
 * This routine initializes fcp_cmnd and iocb data structure from scsi command
 * to transfer for device with SLI3 interface spec.
 **/
static void
lpfc_scsi_prep_cmnd(struct lpfc_vport *vport, struct lpfc_scsi_buf *lpfc_cmd,
		    struct lpfc_nodelist *pnode)
{
	struct lpfc_hba *phba = vport->phba;
	struct scsi_cmnd *scsi_cmnd = lpfc_cmd->pCmd;
	struct fcp_cmnd *fcp_cmnd = lpfc_cmd->fcp_cmnd;
	IOCB_t *iocb_cmd = &lpfc_cmd->cur_iocbq.iocb;
	struct lpfc_iocbq *piocbq = &(lpfc_cmd->cur_iocbq);
	int datadir = scsi_cmnd->sc_data_direction;
	char tag[2];

	if (!pnode || !NLP_CHK_NODE_ACT(pnode))
		return;

	lpfc_cmd->fcp_rsp->rspSnsLen = 0;
	/* clear task management bits */
	lpfc_cmd->fcp_cmnd->fcpCntl2 = 0;

	int_to_scsilun(lpfc_cmd->pCmd->device->lun,
			&lpfc_cmd->fcp_cmnd->fcp_lun);

	memcpy(&fcp_cmnd->fcpCdb[0], scsi_cmnd->cmnd, 16);

	if (scsi_populate_tag_msg(scsi_cmnd, tag)) {
		switch (tag[0]) {
		case HEAD_OF_QUEUE_TAG:
			fcp_cmnd->fcpCntl1 = HEAD_OF_Q;
			break;
		case ORDERED_QUEUE_TAG:
			fcp_cmnd->fcpCntl1 = ORDERED_Q;
			break;
		default:
			fcp_cmnd->fcpCntl1 = SIMPLE_Q;
			break;
		}
	} else
		fcp_cmnd->fcpCntl1 = 0;

	/*
	 * There are three possibilities here - use scatter-gather segment, use
	 * the single mapping, or neither.  Start the lpfc command prep by
	 * bumping the bpl beyond the fcp_cmnd and fcp_rsp regions to the first
	 * data bde entry.
	 */
	if (scsi_cmnd->use_sg) {
		if (datadir == DMA_TO_DEVICE) {
			iocb_cmd->ulpCommand = CMD_FCP_IWRITE64_CR;
			if (phba->sli_rev < LPFC_SLI_REV4) {
				iocb_cmd->un.fcpi.fcpi_parm = 0;
				iocb_cmd->ulpPU = 0;
			} else
				iocb_cmd->ulpPU = PARM_READ_CHECK;
			fcp_cmnd->fcpCntl3 = WRITE_DATA;
			phba->fc4OutputRequests++;
		} else {
			iocb_cmd->ulpCommand = CMD_FCP_IREAD64_CR;
			iocb_cmd->ulpPU = PARM_READ_CHECK;
			iocb_cmd->un.fcpi.fcpi_parm =
				scsi_cmnd->request_bufflen;
			fcp_cmnd->fcpCntl3 = READ_DATA;
			phba->fc4InputRequests++;
		}
	} else if (scsi_cmnd->request_buffer && scsi_cmnd->request_bufflen) {
		if (datadir == DMA_TO_DEVICE) {
			iocb_cmd->ulpCommand = CMD_FCP_IWRITE64_CR;
			if (phba->sli_rev < LPFC_SLI_REV4) {
				iocb_cmd->un.fcpi.fcpi_parm = 0;
				iocb_cmd->ulpPU = 0;
			} else
				iocb_cmd->ulpPU = PARM_READ_CHECK;
			fcp_cmnd->fcpCntl3 = WRITE_DATA;
			phba->fc4OutputRequests++;
		} else {
			iocb_cmd->ulpCommand = CMD_FCP_IREAD64_CR;
			iocb_cmd->ulpPU = PARM_READ_CHECK;
			iocb_cmd->un.fcpi.fcpi_parm =
				scsi_cmnd->request_bufflen;
			fcp_cmnd->fcpCntl3 = READ_DATA;
			phba->fc4InputRequests++;
		}
	} else {
		iocb_cmd->ulpCommand = CMD_FCP_ICMND64_CR;
		iocb_cmd->un.fcpi.fcpi_parm = 0;
		iocb_cmd->ulpPU = 0;
		fcp_cmnd->fcpCntl3 = 0;
		phba->fc4ControlRequests++;
	}

	/*
	 * Finish initializing those IOCB fields that are independent
	 * of the scsi_cmnd request_buffer
	 */
	piocbq->iocb.ulpContext = pnode->nlp_rpi;
	if (pnode->nlp_fcp_info & NLP_FCP_2_DEVICE)
		piocbq->iocb.ulpFCP2Rcvy = 1;
	else
		piocbq->iocb.ulpFCP2Rcvy = 0;

	piocbq->iocb.ulpClass = (pnode->nlp_fcp_info & 0x0f);
	piocbq->context1  = lpfc_cmd;
	piocbq->iocb_cmpl = lpfc_scsi_cmd_iocb_cmpl;
	piocbq->iocb.ulpTimeout = lpfc_cmd->timeout;
	piocbq->vport = vport;
}

/**
 * lpfc_scsi_prep_task_mgmt_cmnd - Convert SLI3 scsi TM cmd to FCP info unit
 * @vport: The virtual port for which this call is being executed.
 * @lpfc_cmd: Pointer to lpfc_scsi_buf data structure.
 * @lun: Logical unit number.
 * @task_mgmt_cmd: SCSI task management command.
 *
 * This routine creates FCP information unit corresponding to @task_mgmt_cmd
 * for device with SLI-3 interface spec.
 *
 * Return codes:
 *   0 - Error
 *   1 - Success
 **/
static int
lpfc_scsi_prep_task_mgmt_cmd(struct lpfc_vport *vport,
			     struct lpfc_scsi_buf *lpfc_cmd,
			     unsigned int lun,
			     uint8_t task_mgmt_cmd)
{
	struct lpfc_iocbq *piocbq;
	IOCB_t *piocb;
	struct fcp_cmnd *fcp_cmnd;
	struct lpfc_rport_data *rdata = lpfc_cmd->rdata;
	struct lpfc_nodelist *ndlp = rdata->pnode;

	if (!ndlp || !NLP_CHK_NODE_ACT(ndlp) ||
	    ndlp->nlp_state != NLP_STE_MAPPED_NODE)
		return 0;

	piocbq = &(lpfc_cmd->cur_iocbq);
	piocbq->vport = vport;

	piocb = &piocbq->iocb;

	fcp_cmnd = lpfc_cmd->fcp_cmnd;
	int_to_scsilun(lun, &lpfc_cmd->fcp_cmnd->fcp_lun);
	fcp_cmnd->fcpCntl2 = task_mgmt_cmd;

	piocb->ulpCommand = CMD_FCP_ICMND64_CR;

	piocb->ulpContext = ndlp->nlp_rpi;
	if (ndlp->nlp_fcp_info & NLP_FCP_2_DEVICE) {
		piocb->ulpFCP2Rcvy = 1;
	}
	piocb->ulpClass = (ndlp->nlp_fcp_info & 0x0f);

	/* ulpTimeout is only one byte */
	if (lpfc_cmd->timeout > 0xff) {
		/*
		 * Do not timeout the command at the firmware level.
		 * The driver will provide the timeout mechanism.
		 */
		piocb->ulpTimeout = 0;
	} else
		piocb->ulpTimeout = lpfc_cmd->timeout;

	if (vport->phba->sli_rev == LPFC_SLI_REV4)
		lpfc_sli4_set_rsp_sgl_last(vport->phba, lpfc_cmd);

	return 1;
}

/**
 * lpfc_scsi_api_table_setup - Set up scsi api fucntion jump table
 * @phba: The hba struct for which this call is being executed.
 * @dev_grp: The HBA PCI-Device group number.
 *
 * This routine sets up the SCSI interface API function jump table in @phba
 * struct.
 * Returns: 0 - success, -ENODEV - failure.
 **/
int
lpfc_scsi_api_table_setup(struct lpfc_hba *phba, uint8_t dev_grp)
{

	phba->lpfc_scsi_unprep_dma_buf = lpfc_scsi_unprep_dma_buf;
	phba->lpfc_scsi_prep_cmnd = lpfc_scsi_prep_cmnd;

	switch (dev_grp) {
	case LPFC_PCI_DEV_LP:
		phba->lpfc_new_scsi_buf = lpfc_new_scsi_buf_s3;
		phba->lpfc_scsi_prep_dma_buf = lpfc_scsi_prep_dma_buf_s3;
		phba->lpfc_release_scsi_buf = lpfc_release_scsi_buf_s3;
		phba->lpfc_get_scsi_buf = lpfc_get_scsi_buf_s3;
		break;
	case LPFC_PCI_DEV_OC:
		phba->lpfc_new_scsi_buf = lpfc_new_scsi_buf_s4;
		phba->lpfc_scsi_prep_dma_buf = lpfc_scsi_prep_dma_buf_s4;
		phba->lpfc_release_scsi_buf = lpfc_release_scsi_buf_s4;
		phba->lpfc_get_scsi_buf = lpfc_get_scsi_buf_s4;
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1418 Invalid HBA PCI-device group: 0x%x\n",
				dev_grp);
		return -ENODEV;
		break;
	}
	phba->lpfc_rampdown_queue_depth = lpfc_rampdown_queue_depth;
	return 0;
}

/**
 * lpfc_taskmgmt_def_cmpl - IOCB completion routine for task management command
 * @phba: The Hba for which this call is being executed.
 * @cmdiocbq: Pointer to lpfc_iocbq data structure.
 * @rspiocbq: Pointer to lpfc_iocbq data structure.
 *
 * This routine is IOCB completion routine for device reset and target reset
 * routine. This routine release scsi buffer associated with lpfc_cmd.
 **/
static void
lpfc_tskmgmt_def_cmpl(struct lpfc_hba *phba,
			struct lpfc_iocbq *cmdiocbq,
			struct lpfc_iocbq *rspiocbq)
{
	struct lpfc_scsi_buf *lpfc_cmd =
		(struct lpfc_scsi_buf *) cmdiocbq->context1;
	if (lpfc_cmd)
		lpfc_release_scsi_buf(phba, lpfc_cmd);
	return;
}

/**
 * lpfc_info - Info entry point of scsi_host_template data structure
 * @host: The scsi host for which this call is being executed.
 *
 * This routine provides module information about hba.
 *
 * Reutrn code:
 *   Pointer to char - Success.
 **/
const char *
lpfc_info(struct Scsi_Host *host)
{
	struct lpfc_vport *vport = (struct lpfc_vport *) host->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	int len;
#if !defined(__VMKLNX__)
	static char  lpfcinfobuf[384];
#else /* defined(__VMKLNX__) */
#define LPFC_INFO_BUF_SZ        389
	static char  lpfcinfobuf[LPFC_INFO_BUF_SZ];
#endif

#if !defined(__VMKLNX__)
	memset(lpfcinfobuf,0,384);
#else /* defined(__VMKLNX__) */
	memset(lpfcinfobuf,0,LPFC_INFO_BUF_SZ);
#endif /* defined(__VMKLNX__) */
	if (phba && phba->pcidev){
		strncpy(lpfcinfobuf, phba->ModelDesc, 256);
		len = strlen(lpfcinfobuf);
		snprintf(lpfcinfobuf + len,
#if !defined(__VMKLNX__)
			384-len,
			" on PCI bus %02x device %02x irq %d",
#else /* defined(__VMKLNX__) */
			LPFC_INFO_BUF_SZ-len,
			" on PCI bus %04x:%02x device %02x irq %d",
                        pci_domain_nr(phba->pcidev->bus),
#endif /* defined(__VMKLNX__) */
			phba->pcidev->bus->number,
			phba->pcidev->devfn,
			phba->pcidev->irq);
		len = strlen(lpfcinfobuf);
		if (phba->Port[0]) {
			snprintf(lpfcinfobuf + len,
#if !defined(__VMKLNX__)
				 384-len,
#else /* defined(__VMKLNX__) */
				 LPFC_INFO_BUF_SZ-len,
#endif /* defined(__VMKLNX__) */
				 " port %s",
				 phba->Port);
		}
		len = strlen(lpfcinfobuf);
		if (phba->sli4_hba.link_state.logical_speed) {
			snprintf(lpfcinfobuf + len,
				 384-len,
				 " Logical Link Speed: %d Mbps",
				 phba->sli4_hba.link_state.logical_speed * 10);
		}
	}
	return lpfcinfobuf;
}

struct lpfc_vport**
lpfc_create_and_sort_vport_list(struct lpfc_hba *phba, int *vport_list_len)
{
	struct lpfc_vport *port_iterator;
	struct lpfc_vport **vports;
	struct lpfc_vport *temp_vport;
	int index = 0;
	int i, j;
	vports = kzalloc((phba->max_vports + 1) * sizeof(struct lpfc_vport *),
			 GFP_KERNEL);
	if (vports == NULL)
		return NULL;
	spin_lock_irq(&phba->hbalock);
	list_for_each_entry(port_iterator, &phba->port_list, listentry) {
		if (LPFC_PHYSICAL_PORT == port_iterator->port_type)
			continue;
		vports[index++] = port_iterator;
	}
	*vport_list_len = index;
	for (i = j = 0; i < index; i++) {
		if (vports[i] == NULL)
			continue;
		for (j = i+1; j < index; j++)
			if (vports[j] != NULL &&
				((struct lpfc_vport *)vports[i])->vpi >
				((struct lpfc_vport *)vports[j])->vpi) {
				temp_vport = vports[i];
				vports[i]  = vports[j];
				vports[j]  = temp_vport;
			}
	}
	spin_unlock_irq(&phba->hbalock);
	return vports;
}

int
lpfc_proc_info(struct Scsi_Host *shost, char *buffer, char **start, off_t offset,
	       int length, int func)
{
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_vport *temp_vport = NULL;
	struct lpfc_hba *phba = vport->phba;
	char fwrev[64];
	struct lpfc_nodelist *ndlp;
	uint8_t name[sizeof (struct lpfc_name)];
	int len = 0;
	uint32_t mrpi, arpi, mvpi, avpi, mxri, axri;
	uint32_t lpfc_log_setting;
	uint32_t san_display = 0;
	char log_cmd[32] = {0};
	int str_stat;
	char *info_buf;
	int byte_cnt;
	struct lpfc_vport **vports;
	struct Scsi_Host *vp_shost;
	int i, vport_list_len;
	uint32_t verbose = 0;
	int ret;

	if (phba->pport)
		verbose = phba->pport->cfg_log_verbose & LOG_PROC ? 1 : 0;

	if (func) {
		/**
		 * Write path.  Only allow lpfc_log_verbose here.
		 * The log_cmd buffer is only 32 entries deep. Discount one
		 * for the NULL terminator and make sure the incoming buffer
		 * does not exceed the temporary driver buffer.
		 */
		sscanf(buffer, "%31s%x", log_cmd, &lpfc_log_setting);
		str_stat = strncmp(log_cmd, "lpfc_log_verbose",
				   sizeof("lpfc_log_verbose"));
		if (str_stat == 0) {
			lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
					"0701 Received %s value x%x\n",
					log_cmd, lpfc_log_setting);

			lpfc_log_verbose_set(phba->pport, lpfc_log_setting);
			return length;
		}

		str_stat = strncmp(log_cmd, "dump_npiv_cnt",
				   sizeof("dump_npiv_cnt"));
		if (str_stat == 0) {
			vports = lpfc_create_vport_work_array(phba);
			if (vports == NULL)
				return length;

			for (i = 0; i <= phba->max_vports && vports[i] != NULL;
			     i++) {
				memcpy(&name[0], &vports[i]->fc_portname,
				       sizeof(struct lpfc_name));
				vp_shost = lpfc_shost_from_vport(vports[i]);
				lpfc_printf_log(phba, KERN_INFO, LOG_FCP,
				       "0703 [%s: %d:%d %02x:%02x:%02x:%02x:"
				       "%02x:%02x:%02x:%02x] tx cnt 0x%llx\n",
				       vmklnx_get_vmhba_name(vp_shost),
				       phba->brd_no, vports[i]->vpi,
				       name[0], name[1], name[2], name[3],
				       name[4], name[5], name[6], name[7],
				       vports[i]->tx_cnt);
			}

			lpfc_destroy_vport_work_array(phba, vports);
			return length;
		}
		return length;
	} else {
		*start = buffer;
	}

	info_buf = kzalloc(INFO_BUF, GFP_KERNEL);
	if (info_buf == NULL)
		return 0;

	if (phba->pci_dev_grp == LPFC_PCI_DEV_LP)
		len += snprintf(info_buf+len, INFO_BUF-len,
				LPFC_MODULE_DESC_LP "\n");
	else
		len += snprintf(info_buf+len, INFO_BUF-len,
				LPFC_MODULE_DESC_OC "\n");

	len += snprintf(info_buf+len, INFO_BUF-len, "%s\n",
			lpfc_info(shost));

	if (!(vport->vport_flag & STATIC_VPORT))
		len += snprintf(info_buf+len, INFO_BUF-len, "BoardNum: %d\n",
				phba->brd_no);
	else
		len += snprintf(info_buf+len, INFO_BUF-len, "BoardNum: %d:%d\n",
				phba->brd_no, vport->vpi);

	len += snprintf(info_buf+len, INFO_BUF-len, "ESX Adapter: %s\n",
			vmklnx_get_vmhba_name(shost));

	lpfc_decode_firmware_rev(phba, fwrev, 1);
	len += snprintf(info_buf+len, INFO_BUF-len, "Firmware Version: %s\n",
			fwrev);

	len += snprintf(info_buf+len, INFO_BUF-len, "Portname: ");
	memcpy(&name[0], &vport->fc_portname, sizeof(struct lpfc_name));
	len += snprintf(info_buf+len, INFO_BUF-len,
			"%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
			name[0], name[1], name[2],
			name[3], name[4], name[5],
			name[6], name[7]);

	len += snprintf(info_buf+len, INFO_BUF-len, "   Nodename: ");
	memcpy(&name[0], &vport->fc_nodename,
		sizeof (struct lpfc_name));

	len += snprintf(info_buf+len, INFO_BUF-len,
			"%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
			name[0], name[1], name[2],
			name[3], name[4], name[5],
			name[6], name[7]);

	if (phba->sli_rev >= LPFC_SLI_REV3) {
		/*
		 * This operation needs to be synchronized as a vport could
		 * be getting destroyed while a proc script is running.
		 */
		ret = lpfc_get_hba_info(phba, &mxri, &axri, &mrpi, &arpi, &mvpi,
				  &avpi);
		len += snprintf(info_buf+len, INFO_BUF-len, "\nSLI Rev: %d\n",
				phba->sli_rev);
		if (phba->sli_rev >= LPFC_SLI_REV4) {
			if (phba->cfg_use_mq == LPFC_ENABLE_MQ) {
				len += snprintf(info_buf+len, INFO_BUF-len,
						"\n   MQ: Enabled with 0x%x queues\n",
						phba->mq_num_qs);
			} else {
				len += snprintf(info_buf+len, INFO_BUF-len,
						"\n   MQ: Disabled\n");
			}
		} else {
			len += snprintf(info_buf+len, INFO_BUF-len,
					"\n   MQ: Unavailable\n");
		}
		if (vport->vport_flag & STATIC_VPORT) {
			len += snprintf(info_buf+len, INFO_BUF-len,
						"   Static Flex Vport");
			goto port_data;
		}
		if (phba->sli3_options & LPFC_SLI3_NPIV_ENABLED) {
			if (phba->link_flag & LS_NPIV_FAB_SUPPORTED) {
				len += snprintf(info_buf+len, INFO_BUF-len,
						"   NPIV Supported: ");
				if (ret) {
					len += snprintf(info_buf+len,
						INFO_BUF-len,
						"VPIs max %d  ", mvpi);
					len += snprintf(info_buf+len,
						INFO_BUF-len,
						"VPIs used %d\n", mvpi - avpi);
				} else
					len += snprintf(info_buf+len,
						INFO_BUF-len, "\n");
			} else {
				len += snprintf(info_buf+len, INFO_BUF-len,
					"   NPIV Unsupported by Fabric\n");
			}
		} else {
			if (phba->sli3_options & LPFC_SLI3_NPIV_ENABLED) {
				len += snprintf(info_buf+len, INFO_BUF-len,
					"   NPIV feature disabled.\n");
			} 
		}

		if (ret) {
			len += snprintf(info_buf+len, INFO_BUF-len,
				"   RPIs max %d  ", mrpi);
			len += snprintf(info_buf+len, INFO_BUF-len,
				"RPIs used %d", mrpi - arpi);
		}

		len += snprintf(info_buf+len, INFO_BUF-len,
				"   IOCBs inuse %d  ", phba->iocb_cnt);
		len += snprintf(info_buf+len, INFO_BUF-len, "IOCB max %d",
				phba->iocb_max);

		len += snprintf(info_buf+len, INFO_BUF-len, "   txq cnt %d  ",
				phba->sli.ring[LPFC_ELS_RING].txq_cnt);
		len += snprintf(info_buf+len, INFO_BUF-len, "txq max %d",
				phba->sli.ring[LPFC_ELS_RING].txq_max);

		len += snprintf(info_buf+len, INFO_BUF-len, "  txcmplq %d\n\n",
				phba->sli.ring[LPFC_ELS_RING].txcmplq_cnt);

		len += snprintf(info_buf+len, INFO_BUF-len, "   Vport List:\n");

		/*
		 * The info_buf limit is INFO_BUF, 8192.  Size the number of
		 * vports by the number of targets and the fabric data so that
		 * the entire SAN is displayed along with the vports.  The
		 * assumption is that the number of actual targets is typically
		 * small allowing the code to optimize how many vports will fit.
		 */
		san_display = (TGT_BYTES * vport->fc_map_cnt) + SAN_BYTES;
		vports = NULL;
		vport_list_len = 0;
		vports = lpfc_create_and_sort_vport_list(phba, &vport_list_len);
		if (NULL == vports)
			goto port_data;
		spin_lock_irq(&phba->hbalock);
		for (i = 0; vports[i] != NULL && i < vport_list_len; i++) {
			temp_vport = vports[i];
			vp_shost = lpfc_shost_from_vport(temp_vport);
			len += snprintf(info_buf+len, INFO_BUF-len,
					"   ESX Adapter: %s\n",
					vmklnx_get_vmhba_name(vp_shost));
			len += snprintf(info_buf+len, INFO_BUF-len,
				"   Vport DID 0x%x, vpi %d, state 0x%x\n",
				temp_vport->fc_myDID, temp_vport->vpi,
				temp_vport->port_state);
			len += snprintf(info_buf+len, INFO_BUF-len,
				"      Portname: ");
			memcpy(&name[0], &temp_vport->fc_portname,
				sizeof (struct lpfc_name));
			len += snprintf(info_buf+len, INFO_BUF-len,
				"%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
				name[0], name[1], name[2],
				name[3], name[4], name[5],
				name[6], name[7]);
			len += snprintf(info_buf+len, INFO_BUF-len,
					"  Nodename: ");
			memcpy(&name[0], &temp_vport->fc_nodename,
				sizeof (struct lpfc_name));
			len += snprintf(info_buf+len, INFO_BUF-len,
				"%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
				name[0], name[1], name[2],
				name[3], name[4], name[5],
				name[6], name[7]);

			/* Is there room to insert another vport display? */
			if (((INFO_BUF - len) - VPORT_BYTES) <= san_display)
				break;
		}
		spin_unlock_irq(&phba->hbalock);
		kfree(vports);
	}

 port_data:
	switch (vport->port_state) {
	case LPFC_VPORT_UNKNOWN:
	case LPFC_VPORT_FAILED:
	case LPFC_LOCAL_CFG_LINK:
		switch (phba->link_state) {
		case LPFC_LINK_UNKNOWN:
			len += snprintf(info_buf + len, INFO_BUF-len,
				"\nLink Down - Unknown\n");
			break;
		case LPFC_WARM_START:
		case LPFC_INIT_START:
		case LPFC_INIT_MBX_CMDS:
			len += snprintf(info_buf + len, INFO_BUF-len,
				"\nLink Down - Adapter Restart\n");
			break;
		case LPFC_LINK_DOWN:
		case LPFC_CLEAR_LA:
			len += snprintf(info_buf + len, INFO_BUF-len,
					"\nLink Down\n");
			break;
		case LPFC_HBA_READY:
		case LPFC_LINK_UP:
			len += snprintf(info_buf + len, INFO_BUF-len,
					"\nLink Up");
			if ((phba->hba_flag & HBA_FCOE_SUPPORT) &&
			    (phba->pport->fc_flag & FC_VPORT_CVL_RCVD))
				len += snprintf(info_buf + len, INFO_BUF-len,
					" - No Virtual Link\n");
			else if ((phba->hba_flag & HBA_FCOE_SUPPORT) &&
			    (phba->hba_flag & FCF_TS_INPROG))
				len += snprintf(info_buf + len, INFO_BUF-len,
						" - FCF Discovery\n");
			else if ((phba->hba_flag & HBA_FCOE_SUPPORT) &&
				 (phba->fcf.fcf_flag & FCF_DEAD_DISC))
				len += snprintf(info_buf + len, INFO_BUF-len,
						" - FCF Dead\n");
			else if (phba->hba_flag & HBA_FCOE_SUPPORT)
				len += snprintf(info_buf + len, INFO_BUF-len,
						" - FCF not available\n");
			else
				len += snprintf(info_buf + len, INFO_BUF-len,
				"\n");
			break;
		case LPFC_HBA_ERROR:
			len += snprintf(info_buf + len, INFO_BUF-len,
					"\nLink Down - Adapter Offline\n");
			break;
		default:
			len += snprintf(info_buf + len, INFO_BUF-len,
					"\nLink State %d\n", phba->link_state);
			break;
		}
		break;
	case LPFC_FLOGI:
	case LPFC_FDISC:
	case LPFC_FABRIC_CFG_LINK:
	case LPFC_NS_REG:
	case LPFC_NS_QRY:
	case LPFC_BUILD_DISC_LIST:
	case LPFC_DISC_AUTH:
		len += snprintf(info_buf + len, INFO_BUF-len, "\nLink Up - Discovery\n");
	case LPFC_VPORT_READY:
		len += snprintf(info_buf + len, INFO_BUF-len,
				"\nLink Up - Ready:\n");
		len += snprintf(info_buf + len, INFO_BUF-len, "   PortID 0x%x\n",
				vport->fc_myDID);
		if (phba->fc_topology == TOPOLOGY_LOOP) {
			if (vport->fc_flag & FC_PUBLIC_LOOP)
				len += snprintf(info_buf + len, INFO_BUF-len,
						"   Public Loop\n");
			else
				len += snprintf(info_buf + len, INFO_BUF-len,
						"   Private Loop\n");
		} else {
			if (vport->fc_flag & FC_FABRIC)
				len += snprintf(info_buf + len, INFO_BUF-len,
						"   Fabric\n");
			else
				len += snprintf(info_buf + len, INFO_BUF-len,
						"   Point-2-Point\n");
		}

		if (phba->fc_linkspeed == LA_10GHZ_LINK)
			len += snprintf(info_buf + len, INFO_BUF-len,
					"   Current speed 10G\n\n");
		else if (phba->fc_linkspeed == LA_8GHZ_LINK)
			len += snprintf(info_buf + len, INFO_BUF-len,
					"   Current speed 8G\n\n");
		else if (phba->fc_linkspeed == LA_4GHZ_LINK)
			len += snprintf(info_buf + len, INFO_BUF-len,
					"   Current speed 4G\n\n");
		else if (phba->fc_linkspeed == LA_2GHZ_LINK)
			len += snprintf(info_buf + len, INFO_BUF-len,
					"   Current speed 2G\n\n");
		else if (phba->fc_linkspeed == LA_1GHZ_LINK)
			len += snprintf(info_buf + len, INFO_BUF-len,
					"   Current speed 1G\n\n");
		else
			len += snprintf(info_buf + len, INFO_BUF-len,
					"   Current speed unknown\n\n");

		len += snprintf(info_buf + len, INFO_BUF-len,
				"Port Discovered Nodes: Count %d\n",
				vport->fc_map_cnt);

		spin_lock_irq(shost->host_lock);
		list_for_each_entry(ndlp, &vport->fc_nodes, nlp_listp) {
			if (verbose ||
			    (ndlp->nlp_state == NLP_STE_MAPPED_NODE)) {
				len += snprintf(info_buf + len, INFO_BUF -len,
					"t%04x DID %06x WWPN ",
					ndlp->nlp_sid, ndlp->nlp_DID);

				memcpy (&name[0], &ndlp->nlp_portname,
					sizeof (struct lpfc_name));
				len += snprintf(info_buf + len, INFO_BUF-len,
				"%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
				name[0], name[1], name[2],
				name[3], name[4], name[5],
				name[6], name[7]);

				len += snprintf(info_buf + len, INFO_BUF-len,
						" WWNN ");

				memcpy (&name[0], &ndlp->nlp_nodename,
					sizeof (struct lpfc_name));
				len += snprintf(info_buf + len, INFO_BUF-len,
				"%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
				name[0], name[1], name[2],
				name[3], name[4], name[5],
				name[6], name[7]);

				len += snprintf(info_buf + len, INFO_BUF - len,
					" qdepth %d max %d active %d busy %d",
					ndlp->cmd_qdepth,
					ndlp->cmd_max,
					atomic_read(&ndlp->cmd_pending),
					ndlp->scsi_busy_cnt);

				if (verbose)
					len += snprintf(info_buf + len,
							INFO_BUF - len,
							" || RPI %03x "
							"State x%x "
							"Flag x%08x "
							"kref %d\n",
							ndlp->nlp_rpi,
							ndlp->nlp_state,
							ndlp->nlp_flag,
							atomic_read(
							 &ndlp->kref.refcount));
				else
					len += snprintf(info_buf + len,
							INFO_BUF - len,
							"\n");
			}

			/* Is there room to insert another target display? */
			if (((INFO_BUF - len) - TGT_BYTES) <= TGT_BYTES)
				break;
		}
		spin_unlock_irq(shost->host_lock);
		break;

	default:
		len += snprintf(info_buf + len, INFO_BUF-len,
				"\nError: State is %d\n", vport->port_state);
		break;
	}

	/*
	 * The proc filesystem will repeatedly call this handler until
	 * handler returns zero bytes.  The MIN value and associated logic
	 * have to ensure this requirement is met.  Assume a byte_cnt of zero
	 * and produce the difference only when justified.
	 */
	byte_cnt = 0;
	if (len >= offset)
		byte_cnt = len - offset;

	byte_cnt = MIN(byte_cnt, length);
	len = 0;
	if (byte_cnt > 0) {
		memcpy(buffer, info_buf + offset, byte_cnt);
		len = byte_cnt;
	}

	kfree(info_buf);
	return len;
}

/**
 * lpfc_poll_rearm_time - Routine to modify fcp_poll timer of hba
 * @phba: The Hba for which this call is being executed.
 *
 * This routine modifies fcp_poll_timer  field of @phba by cfg_poll_tmo.
 * The default value of cfg_poll_tmo is 10 milliseconds.
 **/
static __inline__ void lpfc_poll_rearm_timer(struct lpfc_hba * phba)
{
	unsigned long  poll_tmo_expires =
		(jiffies + msecs_to_jiffies(phba->cfg_poll_tmo));

	if (phba->sli.ring[LPFC_FCP_RING].txcmplq_cnt)
		mod_timer(&phba->fcp_poll_timer,
			  poll_tmo_expires);
}

/**
 * lpfc_poll_start_timer - Routine to start fcp_poll_timer of HBA
 * @phba: The Hba for which this call is being executed.
 *
 * This routine starts the fcp_poll_timer of @phba.
 **/
void lpfc_poll_start_timer(struct lpfc_hba * phba)
{
	lpfc_poll_rearm_timer(phba);
}

/**
 * lpfc_poll_timeout - Restart polling timer
 * @ptr: Map to lpfc_hba data structure pointer.
 *
 * This routine restarts fcp_poll timer, when FCP ring  polling is enable
 * and FCP Ring interrupt is disable.
 **/

void lpfc_poll_timeout(unsigned long ptr)
{
	struct lpfc_hba *phba = (struct lpfc_hba *) ptr;

	if (phba->cfg_poll & ENABLE_FCP_RING_POLLING) {
		lpfc_sli_handle_fast_ring_event(phba,
			&phba->sli.ring[LPFC_FCP_RING], HA_R0RE_REQ);

		if (phba->cfg_poll & DISABLE_FCP_RING_INT)
			lpfc_poll_rearm_timer(phba);
	}
}

static void
lpfc_poll_fcp_cmpl(struct lpfc_hba *phba, uint32_t scan_eqid)
{
	uint64_t curr_time, time_delta;

	rdtscll(curr_time);
	if (!phba->last_fp_poll_time)
		phba->last_fp_poll_time = curr_time;

	if (phba->cfg_poll_freq) {
		if (curr_time >= phba->last_fp_poll_time)
			time_delta = curr_time - phba->last_fp_poll_time;
		else
			time_delta = 0xffffffffffffffff - curr_time +
				phba->last_fp_poll_time;

		if (time_delta < lpfc_cpu_clock  * 1000000 /
			phba->cfg_poll_freq)
			return;
	}

	if (lpfc_is_sli4(phba))
		lpfc_sli4_check_fp_eq(phba, scan_eqid);
	else
		lpfc_sli_handle_fast_ring_event(phba,
			&phba->sli.ring[LPFC_FCP_RING],
			HA_R0RE_REQ);

	phba->last_fp_poll_time = curr_time;
}


/**
 * lpfc_queuecommand - scsi_host_template queuecommand entry point
 * @cmnd: Pointer to scsi_cmnd data structure.
 * @done: Pointer to done routine.
 *
 * Driver registers this routine to scsi midlayer to submit a @cmd to process.
 * This routine prepares an IOCB from scsi command and provides to firmware.
 * The @done callback is invoked after driver finished processing the command.
 *
 * Return value :
 *   0 - Success
 *   SCSI_MLQUEUE_HOST_BUSY - Block all devices served by this host temporarily.
 **/
static int
lpfc_queuecommand(struct scsi_cmnd *cmnd, void (*done) (struct scsi_cmnd *))
{
	struct Scsi_Host  *shost = cmnd->device->host;
	struct Scsi_Host  *phys_sh = NULL;
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	struct lpfc_rport_data *rdata = cmnd->device->hostdata;
	struct lpfc_nodelist *ndlp;
	struct lpfc_scsi_buf *lpfc_cmd;
	struct scsi_device *sdev;
	struct fc_rport *rport = starget_to_rport(scsi_target(cmnd->device));
	int err;
	uint32_t cnt, fcp_wqidx = 0;

	err = fc_remote_port_chkready(rport);
	if (err) {
		cmnd->result = err;
		sdev = cmnd->device;
		if ((phba->link_state == LPFC_HBA_ERROR) &&
			(cmnd->result == ScsiResult(DID_NO_CONNECT, 0)) &&
			(sdev->queue_depth != 1)) {

			/* If we reach this point, the HBA is not responding
			 * and has been taken offline. If alot of SCSI IO
			 * was active, this could cause a massive amount of
			 * SCSI layer initiated logging. On some systems,
			 * this massive amount of logging has been known to
			 * cause CPU soft lockups. In an attempt to throttle
			 * the amount of logging, set the sdev queue depth to 1.
			 */
			if (sdev->ordered_tags)
				scsi_adjust_queue_depth(sdev,
					MSG_ORDERED_TAG, 1);
			else
				scsi_adjust_queue_depth(sdev,
					MSG_SIMPLE_TAG, 1);
		}
		goto out_fail_command;
	}
	ndlp = rdata->pnode;

	/*
	 * Catch race where our node has transitioned, but the
	 * transport is still transitioning.
	 */
	if (!ndlp || !NLP_CHK_NODE_ACT(ndlp)) {
		cmnd->result = ScsiResult(DID_TRANSPORT_DISRUPTED, 0);
		goto out_fail_command;
	}
	if (atomic_read(&ndlp->cmd_pending) >= ndlp->cmd_qdepth)
		goto out_host_busy;

	/*
	 * The host lock is not required at this point. Unlock it to allow
	 * other driver threads to run.
	 */
	spin_unlock(shost->host_lock);
	lpfc_cmd = lpfc_get_scsi_buf(phba, ndlp);
	if (lpfc_cmd == NULL) {
		lpfc_rampdown_queue_depth(phba);

		lpfc_printf_vlog(vport, KERN_INFO, LOG_FCP,
				 "0707 driver's buffer pool is empty, "
				 "IO busied\n");
		goto out_host_busy_grab_lock;
	}

	/*
	 * Store the midlayer's command structure for the completion phase
	 * and complete the command initialization.
	 */
	lpfc_cmd->pCmd  = cmnd;
	lpfc_cmd->rdata = rdata;
	lpfc_cmd->timeout = LPFC_SCSI_CMD_TMO;
	lpfc_cmd->start_time = jiffies;
	cmnd->host_scribble = (unsigned char *)lpfc_cmd;
	cmnd->scsi_done = done;

	err = lpfc_scsi_prep_dma_buf(phba, lpfc_cmd);
	if (err)
		goto out_host_busy_free_buf;

	lpfc_scsi_prep_cmnd(vport, lpfc_cmd, ndlp);

#if !defined(__VMKLNX__)
	if ((cmnd->cmnd[0] == REPORT_LUNS) && phba->cfg_enable_npiv)
		mod_timer(&cmnd->eh_timeout, jiffies + 60 * HZ);
#endif

	atomic_inc(&ndlp->cmd_pending);

	cnt =  atomic_read(&ndlp->cmd_pending);
	if (cnt > ndlp->cmd_max)
		ndlp->cmd_max = cnt;

	/*
	 * All MQ IO is run over WQ's that are mapped to the physical port
	 * instance.  For static or virtual ports, get the shost for the
	 * physical port to find out which WQ gets this IO.
	 */
	if (phba->cfg_use_mq == LPFC_ENABLE_MQ) {
		phys_sh = shost;
		if (vport->port_type != LPFC_PHYSICAL_PORT)
			phys_sh = lpfc_shost_from_vport(vport->phba->pport);
		fcp_wqidx = (uint32_t)(unsigned long)
			vmklnx_scsi_get_cmd_ioqueue_handle(cmnd, phys_sh);
		if ((fcp_wqidx >= phba->cfg_fcp_wq_count) ||
		    (fcp_wqidx == EINVAL)) {
			lpfc_printf_log(phba, KERN_ERR, LOG_FCP,
					"2817 Failed to get a valid queue ID "
					"failing MQ IO request\n");
			goto out_host_busy_free_buf;
		}
		lpfc_cmd->cur_iocbq.fcp_wqidx = fcp_wqidx;
	}
	err = lpfc_sli_issue_iocb(phba, LPFC_FCP_RING,
				  &lpfc_cmd->cur_iocbq, SLI_IOCB_RET_IOCB);
	if (err) {
		atomic_dec(&ndlp->cmd_pending);
		goto out_host_busy_free_buf;
	}

	vport->tx_cnt++;
	if (phba->cfg_use_mq == LPFC_ENABLE_MQ) {
		/* The fcp wq and eq indices are identical. */
		lpfc_poll_fcp_cmpl(phba, fcp_wqidx);
	} else {
		lpfc_poll_fcp_cmpl(phba, 0);
	}
	if (phba->cfg_poll & ENABLE_FCP_RING_POLLING) {
		lpfc_sli_handle_fast_ring_event(phba,
			&phba->sli.ring[LPFC_FCP_RING], HA_R0RE_REQ);
		if (phba->cfg_poll & DISABLE_FCP_RING_INT)
			lpfc_poll_rearm_timer(phba);
	}

	spin_lock(shost->host_lock);
	return 0;

 out_host_busy_free_buf:
	lpfc_scsi_unprep_dma_buf(phba, lpfc_cmd);
	lpfc_release_scsi_buf(phba, lpfc_cmd);
 out_host_busy_grab_lock:
	spin_lock(shost->host_lock);
 out_host_busy:
	if (ndlp && NLP_CHK_NODE_ACT(ndlp))
		ndlp->scsi_busy_cnt++;
	return SCSI_MLQUEUE_HOST_BUSY;

 out_fail_command:
	done(cmnd);
	return 0;
}

/**
 * lpfc_block_error_handler - Routine to block error  handler
 * @cmnd: Pointer to scsi_cmnd data structure.
 *
 *  This routine blocks execution while the fc_rport state is
 *  FC_PORSTATE_BLOCKED.
 **/
static void
lpfc_block_error_handler(struct scsi_cmnd *cmnd)
{
	struct Scsi_Host *shost = cmnd->device->host;
	struct fc_rport *rport = starget_to_rport(scsi_target(cmnd->device));

	spin_lock_irq(shost->host_lock);
	while (rport->port_state == FC_PORTSTATE_BLOCKED) {
		spin_unlock_irq(shost->host_lock);
		msleep(1000);
		spin_lock_irq(shost->host_lock);
	}
	spin_unlock_irq(shost->host_lock);
	return;
}

/**
 * lpfc_abort_handler - scsi_host_template eh_abort_handler entry point
 * @cmnd: Pointer to scsi_cmnd data structure.
 *
 * This routine aborts @cmnd pending in base driver.
 *
 * Return code :
 *   0x2003 - Error
 *   0x2002 - Success
 **/
static int
lpfc_abort_handler(struct scsi_cmnd *cmnd)
{
	struct Scsi_Host  *shost = cmnd->device->host;
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	struct lpfc_iocbq *iocb;
	struct lpfc_iocbq *abtsiocb;
	struct lpfc_scsi_buf *lpfc_cmd;
	IOCB_t *cmd, *icmd;
	int ret = SUCCESS;
#ifdef DECLARE_WAIT_QUEUE_HEAD_ONSTACK
	DECLARE_WAIT_QUEUE_HEAD_ONSTACK(waitq);
#else
	DECLARE_WAIT_QUEUE_HEAD(waitq);
#endif


	lpfc_block_error_handler(cmnd);
	spin_lock_irq(&phba->eh_waitlock);
	lpfc_cmd = (struct lpfc_scsi_buf *)cmnd->host_scribble;
	spin_unlock_irq(&phba->eh_waitlock);
#if defined(__VMKLNX__)
	if (!lpfc_cmd) {
		printk(KERN_ERR "%s: SCSI Layer try to abort a completed I/O %p "
			"ID %d LUN %d snum %#lx\n", __FUNCTION__, cmnd,
			cmnd->device->id, cmnd->device->lun, cmnd->serial_number);
		return FAILED;
	}
#else
	BUG_ON(!lpfc_cmd);
#endif

	/*
	 * If pCmd field of the corresponding lpfc_scsi_buf structure
	 * points to a different SCSI command, then the driver has
	 * already completed this command, but the midlayer did not
	 * see the completion before the eh fired.  Just return
	 * SUCCESS.
	 */
	iocb = &lpfc_cmd->cur_iocbq;
	if (lpfc_cmd->pCmd != cmnd)
		goto out;

	BUG_ON(iocb->context1 != lpfc_cmd);

	abtsiocb = lpfc_sli_get_iocbq(phba);
	if (abtsiocb == NULL) {
		ret = FAILED;
		goto out;
	}

	/*
	 * The scsi command can not be in txq and it is in flight because the
	 * pCmd is still pointig at the SCSI command we have to abort. There
	 * is no need to search the txcmplq. Just send an abort to the FW.
	 */

	cmd = &iocb->iocb;
	icmd = &abtsiocb->iocb;
	icmd->un.acxri.abortType = ABORT_TYPE_ABTS;
	icmd->un.acxri.abortContextTag = cmd->ulpContext;
	if (phba->sli_rev == LPFC_SLI_REV4)
		icmd->un.acxri.abortIoTag = iocb->sli4_xritag;
	else
		icmd->un.acxri.abortIoTag = cmd->ulpIoTag;

	icmd->ulpLe = 1;
	icmd->ulpClass = cmd->ulpClass;

	/* ABTS WQE must go to the same WQ as the WQE to be aborted */
	abtsiocb->fcp_wqidx = iocb->fcp_wqidx;
	abtsiocb->iocb_flag |= LPFC_USE_FCPWQIDX;

	if (lpfc_is_link_up(phba))
		icmd->ulpCommand = CMD_ABORT_XRI_CN;
	else
		icmd->ulpCommand = CMD_CLOSE_XRI_CN;

	abtsiocb->iocb_cmpl = lpfc_sli_abort_fcp_cmpl;
	abtsiocb->vport = vport;
	if (lpfc_sli_issue_iocb(phba, LPFC_FCP_RING, abtsiocb, 0) ==
	    IOCB_ERROR) {
		lpfc_sli_release_iocbq(phba, abtsiocb);
		ret = FAILED;
		goto out;
	}

	if (phba->cfg_poll & DISABLE_FCP_RING_INT)
		lpfc_sli_handle_fast_ring_event(phba,
			&phba->sli.ring[LPFC_FCP_RING], HA_R0RE_REQ);

	spin_lock_irq(&phba->eh_waitlock);
	lpfc_cmd->waitq = &waitq;
	spin_unlock_irq(&phba->eh_waitlock);

	/* Wait for abort to complete */
	wait_event_timeout(waitq,
			  (lpfc_cmd->pCmd != cmnd),
			   (2*vport->cfg_devloss_tmo*HZ));

	spin_lock_irq(&phba->eh_waitlock);
	lpfc_cmd->waitq = NULL;
	spin_unlock_irq(&phba->eh_waitlock);
	if (lpfc_cmd->pCmd == cmnd) {
		ret = FAILED;
		lpfc_printf_vlog(vport, KERN_ERR, LOG_FCP,
				 "0748 abort handler timed out waiting "
				 "for abort to complete: ret %#x, ID %d, "
				 "LUN %d, snum %#lx\n",
				 ret, cmnd->device->id, cmnd->device->lun,
				 cmnd->serial_number);
	}

 out:
	lpfc_printf_vlog(vport, KERN_WARNING, LOG_FCP,
			 "0749 SCSI Layer I/O Abort Request Status x%x ID %d "
			 "LUN %d snum %#lx\n", ret, cmnd->device->id,
			 cmnd->device->lun, cmnd->serial_number);
	return ret;
}

static char *
lpfc_taskmgmt_name(uint8_t task_mgmt_cmd)
{
	switch (task_mgmt_cmd) {
	case FCP_ABORT_TASK_SET:
		return "ABORT_TASK_SET";
	case FCP_CLEAR_TASK_SET:
		return "FCP_CLEAR_TASK_SET";
	case FCP_BUS_RESET:
		return "FCP_BUS_RESET";
	case FCP_LUN_RESET:
		return "FCP_LUN_RESET";
	case FCP_TARGET_RESET:
		return "FCP_TARGET_RESET";
	case FCP_CLEAR_ACA:
		return "FCP_CLEAR_ACA";
	case FCP_TERMINATE_TASK:
		return "FCP_TERMINATE_TASK";
	default:
		return "unknown";
	}
}

/**
 * lpfc_send_taskmgmt - Generic SCSI Task Mgmt Handler
 * @vport: The virtual port for which this call is being executed.
 * @rdata: Pointer to remote port local data
 * @tgt_id: Target ID of remote device.
 * @lun_id: Lun number for the TMF
 * @task_mgmt_cmd: type of TMF to send
 *
 * This routine builds and sends a TMF (SCSI Task Mgmt Function) to
 * a remote port.
 *
 * Return Code:
 *   0x2003 - Error
 *   0x2002 - Success.
 **/
static int
lpfc_send_taskmgmt(struct lpfc_vport *vport, struct lpfc_rport_data *rdata,
		    unsigned  tgt_id, unsigned int lun_id,
		    uint32_t task_mgmt_cmd)
{
	struct lpfc_hba   *phba = vport->phba;
	struct lpfc_scsi_buf *lpfc_cmd;
	struct lpfc_iocbq *iocbq;
	struct lpfc_iocbq *iocbqrsp;
	struct lpfc_nodelist *pnode = rdata->pnode;
	int ret;
	int status;
	uint32_t req_task_mgmt_cmd;

	if (!pnode || !NLP_CHK_NODE_ACT(pnode))
		return FAILED;

	lpfc_cmd = lpfc_get_scsi_buf(phba, rdata->pnode);
	if (lpfc_cmd == NULL)
		return FAILED;
	lpfc_cmd->timeout = 60;
	lpfc_cmd->rdata = rdata;

	status = lpfc_scsi_prep_task_mgmt_cmd(vport, lpfc_cmd,
					      lun_id,
					      task_mgmt_cmd);
	req_task_mgmt_cmd = task_mgmt_cmd;

	if (!status) {
		lpfc_release_scsi_buf(phba, lpfc_cmd);
		return FAILED;
	}

	iocbq = &lpfc_cmd->cur_iocbq;
	iocbqrsp = lpfc_sli_get_iocbq(phba);
	/* Always send taskmgmt to WQ 0 explicitly.  Data could be stale. */
	iocbq->fcp_wqidx = 0;
	if (iocbqrsp == NULL) {
		lpfc_release_scsi_buf(phba, lpfc_cmd);
		return FAILED;
	}

	lpfc_printf_vlog(vport, KERN_INFO, LOG_FCP,
			 "0702 Issue %s to TGT %d LUN %d "
			 "rpi x%x nlp_flag x%x\n",
			 lpfc_taskmgmt_name(req_task_mgmt_cmd), tgt_id, lun_id,
			 pnode->nlp_rpi, pnode->nlp_flag);

	status = lpfc_sli_issue_iocb_wait(phba, LPFC_FCP_RING,
					  iocbq, iocbqrsp, lpfc_cmd->timeout);
	if (status != IOCB_SUCCESS) {
		if (status == IOCB_TIMEDOUT) {
			iocbq->iocb_cmpl = lpfc_tskmgmt_def_cmpl;
			ret = TIMEOUT_ERROR;
		} else
			ret = FAILED;
		lpfc_cmd->status = IOSTAT_DRIVER_REJECT;
		lpfc_printf_vlog(vport, KERN_ERR, LOG_FCP,
			 "0727 TMF %s to TGT %d LUN %d failed (%d, %d)\n",
			 lpfc_taskmgmt_name(req_task_mgmt_cmd),
			 tgt_id, lun_id, iocbqrsp->iocb.ulpStatus,
			 iocbqrsp->iocb.un.ulpWord[4]);
	} else if (status == IOCB_BUSY)
		ret = FAILED;
	else
		ret = SUCCESS;

	if ((ret == SUCCESS) && rdata && rdata->pnode &&
	     NLP_CHK_NODE_ACT(rdata->pnode)) {
		if (req_task_mgmt_cmd == FCP_TARGET_RESET)
			rdata->pnode->tgt_rst++;
		else if (req_task_mgmt_cmd == FCP_LUN_RESET)
			rdata->pnode->lun_rst++;
	 }

	lpfc_sli_release_iocbq(phba, iocbqrsp);


	if (ret != TIMEOUT_ERROR)
		lpfc_release_scsi_buf(phba, lpfc_cmd);

	return ret;
}

/**
 * lpfc_chk_tgt_mapped -
 * @vport: The virtual port to check on
 * @cmnd: Pointer to scsi_cmnd data structure.
 *
 * This routine delays until the scsi target (aka rport) for the
 * command exists (is present and logged in) or we declare it non-existent.
 *
 * Return code :
 *  0x2003 - Error
 *  0x2002 - Success
 **/
static int
lpfc_chk_tgt_mapped(struct lpfc_vport *vport, struct scsi_cmnd *cmnd)
{
	struct lpfc_rport_data *rdata = cmnd->device->hostdata;
	struct lpfc_nodelist *pnode;
	unsigned long later;

	if (!rdata) {
		lpfc_printf_vlog(vport, KERN_INFO, LOG_FCP,
			"0797 Tgt Map rport failure: rdata x%p\n", rdata);
		return FAILED;
	}
	pnode = rdata->pnode;
	/*
	 * If target is not in a MAPPED state, delay until
	 * target is rediscovered or devloss timeout expires.
	 */
	later = msecs_to_jiffies(2 * vport->cfg_devloss_tmo * 1000) + jiffies;
	while (time_after(later, jiffies)) {
		if (!pnode || !NLP_CHK_NODE_ACT(pnode))
			return FAILED;
		if (pnode->nlp_state == NLP_STE_MAPPED_NODE)
			return SUCCESS;
		schedule_timeout_uninterruptible(msecs_to_jiffies(500));
		rdata = cmnd->device->hostdata;
		if (!rdata)
			return FAILED;
		pnode = rdata->pnode;
	}
	if (!pnode || !NLP_CHK_NODE_ACT(pnode) ||
	    (pnode->nlp_state != NLP_STE_MAPPED_NODE))
		return FAILED;
	return SUCCESS;
}

/**
 * lpfc_reset_flush_io_context -
 * @vport: The virtual port (scsi_host) for the flush context
 * @tgt_id: If aborting by Target contect - specifies the target id
 * @lun_id: If aborting by Lun context - specifies the lun id
 * @context: specifies the context level to flush at.
 *
 * After a reset condition via TMF, we need to flush orphaned i/o
 * contexts from the adapter. This routine aborts any contexts
 * outstanding, then waits for their completions. The wait is
 * bounded by devloss_tmo though.
 *
 * Return code :
 *  0x2003 - Error
 *  0x2002 - Success
 **/
static int
lpfc_reset_flush_io_context(struct lpfc_vport *vport, uint16_t tgt_id,
			uint64_t lun_id, lpfc_ctx_cmd context,
			unsigned long tstamp)
{
	struct lpfc_hba   *phba = vport->phba;
	unsigned long later;
	int cnt = 0;
	int fail_cnt = 0;

	cnt = lpfc_sli_sum_iocb(vport, tgt_id, lun_id, context, tstamp, 0);
	if (cnt) {
		fail_cnt = lpfc_sli_abort_iocb(vport,
				    &phba->sli.ring[phba->sli.fcp_ring],
				    tgt_id, lun_id, context, LPFC_RESET_ABORT);
		if (fail_cnt) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_FCP,
					"0706 IOCB Abort failed - outstanding "
					"%d failed %d\n",
					cnt, fail_cnt);
		}
	}

	later = msecs_to_jiffies(2 * LPFC_SCSI_CMD_TMO * 1000) + jiffies;
	while (time_after(later, jiffies) && cnt) {
		schedule_timeout_uninterruptible(msecs_to_jiffies(20));
		cnt = lpfc_sli_sum_iocb(vport, tgt_id, lun_id,
					context, tstamp, LPFC_RESET_ABORT);
	}
	if (cnt) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_FCP,
			"0724 I/O flush failure for context %s on <%d:%lld>: "
			"cnt x%x\n",
			((context == LPFC_CTX_LUN) ? "LUN" :
			 ((context == LPFC_CTX_TGT) ? "TGT" :
			  ((context == LPFC_CTX_HOST) ? "HOST" : "Unknown"))),
			tgt_id, lun_id, cnt);
		return FAILED;
	}
	return SUCCESS;
}

/**
 * lpfc_device_reset_handler - scsi_host_template eh_device_reset entry point
 * @cmnd: Pointer to scsi_cmnd data structure.
 *
 * This routine sends a task management command to the remote target Nport.
 * For ESX, this routine copies the vmkernel request from the scsi command
 * and passes it to the lpfc_send_taskmgmt routine to execute the vmkernel
 * request.
 *
 * Return code :
 *  0x2003 - Error
 *  0x2002 - Success
 **/
static int
lpfc_device_reset_handler(struct scsi_cmnd *cmnd)
{
	struct Scsi_Host  *shost = cmnd->device->host;
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_rport_data *rdata = cmnd->device->hostdata;
	struct lpfc_nodelist *pnode;
	unsigned tgt_id = cmnd->device->id;
	unsigned int lun_id = cmnd->device->lun;
	int status;
	int tm_status = SUCCESS;
	int fl_status = SUCCESS;
	uint32_t req_task_mgmt;
	struct lpfc_sd_event *sd_event;
	int sd_event_size = 0;
	struct event_type evt_type;
	lpfc_ctx_cmd ctx_cmd = LPFC_CTX_LUN;
	unsigned long reset_marker;

#if defined(__VMKLNX__)
	struct lpfc_scsi_event_header *scsi_event;
	uint32_t subcategory = 0;
	if (cmnd->vmkflags & VMK_FLAGS_USE_LUNRESET)
		subcategory = LPFC_EVENT_LUNRESET;
	else
		subcategory = LPFC_EVENT_TGTRESET;
#else
        struct lpfc_scsi_event_header scsi_event;
#endif

	if (!rdata) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_FCP,
			"0798 Device Reset rport failure: rdata x%p\n", rdata);
		return FAILED;
	}
	pnode = rdata->pnode;
	lpfc_block_error_handler(cmnd);

	status = lpfc_chk_tgt_mapped(vport, cmnd);
	if (status == FAILED) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_FCP,
			"0721 Device Reset rport failure: rdata x%p\n", rdata);
		return FAILED;
	}

#if defined(__VMKLNX__)
	sd_event_size = sizeof(struct lpfc_sd_event)
		      + sizeof(struct lpfc_scsi_event_header);
	sd_event = kzalloc(sd_event_size, GFP_KERNEL);
	if (sd_event != NULL) {
		scsi_event = (void *)&sd_event->event_payload;
		scsi_event->event_type  = FC_REG_SCSI_EVENT;
		scsi_event->subcategory = subcategory;
		scsi_event->lun = 0;
		(void) memcpy(&sd_event->vport_name, vport->fc_portname.u.wwn,
					sizeof(sd_event->vport_name));
		(void) memcpy(&scsi_event->wwpn, &pnode->nlp_portname,
					sizeof(struct lpfc_name));
		(void) memcpy(&scsi_event->wwnn, &pnode->nlp_nodename,
					sizeof(struct lpfc_name));
		evt_type.mask = FC_REG_SANDIAGS_EVENT;
		evt_type.cat = FC_REG_SCSI_EVENT;
		evt_type.sub = subcategory;
		lpfc_send_event(vport->phba, sd_event_size, sd_event, evt_type);
	}
	if (cmnd->vmkflags & VMK_FLAGS_USE_LUNRESET)
		req_task_mgmt = FCP_LUN_RESET;
	else
		req_task_mgmt = FCP_TARGET_RESET;

#else
	scsi_event.event_type = FC_REG_SCSI_EVENT;
	scsi_event.subcategory = LPFC_EVENT_TGTRESET;
	scsi_event.lun = lun_id;
	memcpy(scsi_event.wwpn, &pnode->nlp_portname, sizeof(struct lpfc_name));
	memcpy(scsi_event.wwnn, &pnode->nlp_nodename, sizeof(struct lpfc_name));
	fc_host_post_vendor_event(shost, fc_get_event_number(),
		 sizeof(scsi_event), (char *)&scsi_event, LPFC_NL_VENDOR_ID);
	req_task_mgmt = FCP_TARGET_RESET;
#endif
	reset_marker = jiffies;
	tm_status = lpfc_send_taskmgmt(vport, rdata, tgt_id, lun_id,
				       req_task_mgmt);

	/*
	 * Clean up all IO.  The IO may be orphaned by the TMF or if the TMF
	 * failed, the IO may be in an indeterminate state.
	 */
#if defined(__VMKLNX__)
	if (req_task_mgmt & VMK_FLAGS_USE_TARGETRESET)
		ctx_cmd = LPFC_CTX_TGT;
#endif

	fl_status = lpfc_reset_flush_io_context(vport, tgt_id,
					lun_id, ctx_cmd, reset_marker);
	lpfc_printf_vlog(vport, KERN_ERR, LOG_FCP,
			"0713 SCSI layer issued Device Reset (%d, %d) "
			"reset status x%x flush status x%x\n", tgt_id, lun_id,
			 tm_status, fl_status);

	status = tm_status;
	if ((fl_status != SUCCESS) && (tm_status == SUCCESS))
		status = fl_status;
	return status;
}

/**
 * lpfc_bus_reset_handler - scsi_host_template eh_bus_reset_handler entry point
 * @cmnd: Pointer to scsi_cmnd data structure.
 *
 * This routine does target reset to all targets on @cmnd->device->host.
 * This emulates Parallel SCSI Bus Reset Semantics.
 *
 * Return code :
 *  0x2003 - Error
 *  0x2002 - Success
 **/
static int
lpfc_bus_reset_handler(struct scsi_cmnd *cmnd)
{
	struct Scsi_Host  *shost = cmnd->device->host;
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_nodelist *ndlp = NULL;
	struct lpfc_scsi_event_header *scsi_event;
	int match;
	int ret = SUCCESS, status, i;

	struct lpfc_sd_event *sd_event;
	int sd_event_size;
	struct event_type evt_type;

	sd_event_size = sizeof(struct lpfc_sd_event)
		      + sizeof(struct lpfc_scsi_event_header);

	sd_event = kzalloc(sd_event_size, GFP_KERNEL);

	if (sd_event != NULL) {
		(void) memcpy(&sd_event->vport_name, vport->fc_portname.u.wwn,
					sizeof(sd_event->vport_name));
		scsi_event = (void *)&sd_event->event_payload;
		scsi_event->event_type  = FC_REG_SCSI_EVENT;
		scsi_event->subcategory = LPFC_EVENT_BUSRESET;
		scsi_event->lun = 0;
		(void) memcpy(scsi_event->wwpn, &vport->fc_portname,
					sizeof(struct lpfc_name));
		(void) memcpy(scsi_event->wwnn, &vport->fc_nodename,
					sizeof(struct lpfc_name));

		evt_type.mask = FC_REG_SANDIAGS_EVENT;
		evt_type.cat = FC_REG_SCSI_EVENT;
		evt_type.sub = LPFC_EVENT_BUSRESET;
		lpfc_send_event(vport->phba, sd_event_size, sd_event, evt_type);
	}

	lpfc_block_error_handler(cmnd);

	/*
	 * Since the driver manages a single bus device, reset all
	 * targets known to the driver.  Should any target reset
	 * fail, this routine returns failure to the midlayer.
	 */
	for (i = 0; i < LPFC_MAX_TARGET; i++) {
		/* Search for mapped node by target ID */
		match = 0;
		spin_lock_irq(shost->host_lock);
		list_for_each_entry(ndlp, &vport->fc_nodes, nlp_listp) {
			if (!NLP_CHK_NODE_ACT(ndlp))
				continue;
			if (ndlp->nlp_state == NLP_STE_MAPPED_NODE &&
			    ndlp->nlp_sid == i &&
			    ndlp->rport) {
				match = 1;
				break;
			}
		}
		spin_unlock_irq(shost->host_lock);
		if (!match)
			continue;

		status = lpfc_send_taskmgmt(vport, ndlp->rport->dd_data,
					i, 0, FCP_TARGET_RESET);

		if (status != SUCCESS) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_FCP,
					 "0700 Bus Reset on target %d failed\n",
					 i);
			ret = FAILED;
		}
	}
	/*
	 * We have to clean up i/o as : they may be orphaned by the TMFs
	 * above; or if any of the TMFs failed, they may be in an
	 * indeterminate state.
	 * We will report success if all the i/o aborts successfully.
	 */

	status = lpfc_reset_flush_io_context(vport, 0, 0,
				LPFC_CTX_HOST, jiffies);

	if (status != SUCCESS)
		ret = FAILED;

	lpfc_printf_vlog(vport, KERN_ERR, LOG_FCP,
			 "0714 SCSI layer issued Bus Reset Data: x%x\n", ret);
	return ret;
}

/**
 * lpfc_slave_alloc - scsi_host_template slave_alloc entry point
 * @sdev: Pointer to scsi_device.
 *
 * This routine populates the cmds_per_lun count + 2 scsi_bufs into  this host's
 * globally available list of scsi buffers. This routine also makes sure scsi
 * buffer is not allocated more than HBA limit conveyed to midlayer. This list
 * of scsi buffer exists for the lifetime of the driver.
 *
 * Return codes:
 *   non-0 - Error
 *   0 - Success
 **/
static int
lpfc_slave_alloc(struct scsi_device *sdev)
{
	struct lpfc_vport *vport = (struct lpfc_vport *) sdev->host->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	struct fc_rport *rport = starget_to_rport(scsi_target(sdev));
	uint32_t total = 0;
	uint32_t num_to_alloc = 0;
	int num_allocated = 0;
	uint32_t sdev_cnt;

	if (!rport || fc_remote_port_chkready(rport))
		return -ENXIO;

	sdev->hostdata = rport->dd_data;
	sdev_cnt = atomic_inc_return(&phba->sdev_cnt);

	/*
	 * Populate the cmds_per_lun count scsi_bufs into this host's globally
	 * available list of scsi buffers.  Don't allocate more than the
	 * HBA limit conveyed to the midlayer via the host structure.  The
	 * formula accounts for the lun_queue_depth + error handlers + 1
	 * extra.  This list of scsi bufs exists for the lifetime of the driver.
	 */
	total = phba->total_scsi_bufs;
	num_to_alloc = vport->cfg_lun_queue_depth + 2;

	/* If allocated buffers are enough do nothing */
	if ((sdev_cnt * (vport->cfg_lun_queue_depth + 2)) < total)
		return 0;

	/* Allow some exchanges to be available always to complete discovery */
	if (total >= phba->cfg_hba_queue_depth - LPFC_DISC_IOCB_BUFF_COUNT ) {
		lpfc_printf_vlog(vport, KERN_WARNING, LOG_FCP,
				 "0704 At limitation of %d preallocated "
				 "command buffers\n", total);
		return 0;
	/* Allow some exchanges to be available always to complete discovery */
	} else if (total + num_to_alloc >
		phba->cfg_hba_queue_depth - LPFC_DISC_IOCB_BUFF_COUNT ) {
		lpfc_printf_vlog(vport, KERN_WARNING, LOG_FCP,
				 "0705 Allocation request of %d "
				 "command buffers will exceed max of %d.  "
				 "Reducing allocation request to %d.\n",
				 num_to_alloc, phba->cfg_hba_queue_depth,
				 (phba->cfg_hba_queue_depth - total));
		num_to_alloc = phba->cfg_hba_queue_depth - total;
	}
	num_allocated = lpfc_new_scsi_buf(vport, num_to_alloc);
	if (num_to_alloc != num_allocated) {
			lpfc_printf_vlog(vport, KERN_WARNING, LOG_FCP,
				 "0708 Allocation request of %d "
				 "command buffers did not succeed.  "
				 "Allocated %d buffers.\n",
				 num_to_alloc, num_allocated);
	}
	if (num_allocated > 0)
		phba->total_scsi_bufs += num_allocated;
	return 0;
}

/**
 * lpfc_slave_configure - scsi_host_template slave_configure entry point
 * @sdev: Pointer to scsi_device.
 *
 * This routine configures following items
 *   - Tag command queuing support for @sdev if supported.
 *   - Dev loss time out value of fc_rport.
 *   - Enable SLI polling for fcp ring if ENABLE_FCP_RING_POLLING flag is set.
 *
 * Return codes:
 *   0 - Success
 **/
static int
lpfc_slave_configure(struct scsi_device *sdev)
{
	struct lpfc_vport *vport = (struct lpfc_vport *) sdev->host->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	struct fc_rport   *rport = starget_to_rport(sdev->sdev_target);

	if (sdev->tagged_supported)
		scsi_activate_tcq(sdev, vport->cfg_lun_queue_depth);
	else
		scsi_deactivate_tcq(sdev, vport->cfg_lun_queue_depth);

	/*
	 * Initialize the fc transport attributes for the target
	 * containing this scsi device.  Also note that the driver's
	 * target pointer is stored in the starget_data for the
	 * driver's sysfs entry point functions.
	 */
	rport->dev_loss_tmo = vport->cfg_devloss_tmo;

	if (phba->cfg_poll & ENABLE_FCP_RING_POLLING) {
		lpfc_sli_handle_fast_ring_event(phba,
			&phba->sli.ring[LPFC_FCP_RING], HA_R0RE_REQ);
		if (phba->cfg_poll & DISABLE_FCP_RING_INT)
			lpfc_poll_rearm_timer(phba);
	}

	return 0;
}

/**
 * lpfc_slave_destroy - slave_destroy entry point of SHT data structure
 * @sdev: Pointer to scsi_device.
 *
 * This routine sets @sdev hostatdata filed to null.
 **/
static void
lpfc_slave_destroy(struct scsi_device *sdev)
{
	struct lpfc_vport *vport = (struct lpfc_vport *) sdev->host->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	atomic_dec(&phba->sdev_cnt);
	sdev->hostdata = NULL;
	return;
}


struct scsi_host_template lpfc_template = {
	.module			= THIS_MODULE,
	.name			= LPFC_DRIVER_NAME,
	.info			= lpfc_info,
	.queuecommand		= lpfc_queuecommand,
	.eh_abort_handler	= lpfc_abort_handler,
	.eh_device_reset_handler = lpfc_device_reset_handler,
	.eh_bus_reset_handler	= lpfc_bus_reset_handler,
	.slave_alloc		= lpfc_slave_alloc,
	.slave_configure	= lpfc_slave_configure,
	.slave_destroy		= lpfc_slave_destroy,
	.scan_finished		= lpfc_scan_finished,
	.proc_info		= lpfc_proc_info,
	.proc_name		= LPFC_DRIVER_NAME,
	.this_id		= -1,
	.sg_tablesize		= LPFC_DEFAULT_SG_SEG_CNT,
	.cmd_per_lun		= LPFC_CMD_PER_LUN,
	.use_clustering		= ENABLE_CLUSTERING,
	.shost_attrs		= lpfc_hba_attrs,
	.max_sectors		= 0xFFFF,
};

struct scsi_host_template lpfc_vport_template = {
	.module			= THIS_MODULE,
	.name			= LPFC_DRIVER_NAME,
	.info			= lpfc_info,
	.queuecommand		= lpfc_queuecommand,
	.eh_abort_handler	= lpfc_abort_handler,
	.eh_device_reset_handler = lpfc_device_reset_handler,
	.eh_bus_reset_handler	= lpfc_bus_reset_handler,
	.slave_alloc		= lpfc_slave_alloc,
	.slave_configure	= lpfc_slave_configure,
	.slave_destroy		= lpfc_slave_destroy,
	.scan_finished		= lpfc_scan_finished,
	.this_id		= -1,
	.sg_tablesize		= LPFC_DEFAULT_SG_SEG_CNT,
	.cmd_per_lun		= LPFC_CMD_PER_LUN,
	.use_clustering		= ENABLE_CLUSTERING,
	.shost_attrs		= lpfc_vport_attrs,
	.max_sectors		= 0xFFFF,
};
