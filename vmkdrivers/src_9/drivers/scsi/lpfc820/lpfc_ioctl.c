/*******************************************************************
 * This file is part of the Emulex Linux Device Driver for         *
 * Fibre Channel Host Bus Adapters.                                *
 * Copyright (C) 2006-2011 Emulex.  All rights reserved.           *
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

struct event_data {
	struct event_type type;
	uint32_t immed_dat;
	void * data;
	uint32_t len;
};

struct lpfcdfc_event {
	struct list_head node;

	/* Event type and waiter identifiers */
	uint32_t type_mask;
	uint32_t req_id;
	uint32_t reg_id;

	/* next two flags are here for the auto-delete logic */
	/* Not certain what this comment is referring to. -dcw */
	unsigned long wait_time_stamp;
	int waiting;

	/* Event Data */
	struct event_data evtData;
};

/* Provide forward prototypes. */
static int 
lpfc_ioctl_queue_event(struct lpfcdfc_host *,struct lpfcdfc_event *,uint32_t);

static struct lpfc_dmabufext *
dfc_cmd_data_alloc(struct lpfc_hba * phba,
		   uint8_t *indataptr, struct ulp_bde64 * bpl, uint32_t size);

struct unsol_rcv_ct_ctx {
	uint32_t ctxt_id;
	uint32_t SID;
	uint32_t oxid;
	uint32_t flags;
#define UNSOL_VALD	0x00000001
};

struct lpfc_dmabufext *
__dfc_cmd_data_alloc(
	struct lpfc_hba  *phba,
	uint8_t *indataptr,
	struct ulp_bde64 *bpl, 
	uint32_t size,
	int nocopydata);

/* Returns TRUE if the port supports persistent link disable */
static inline bool
lpfc_is_persistent_link_disable(struct lpfc_hba *phba)
{
	return (((phba->sli_rev == LPFC_SLI_REV4) &&
		(phba->hba_flag & HBA_FCOE_SUPPORT)) ? false : true);
}

static int dfc_cmd_data_free(struct lpfc_hba *, struct lpfc_dmabufext *);
static int lpfc_evt_reclaim(struct lpfc_hba *, struct lpfcdfc_event *);
static int lpfc_dmabuf_reclaim(struct lpfc_hba *, struct lpfc_dmabuf *);
static int lpfc_mbuf_reclaim(struct lpfc_hba *, struct lpfc_dmabuf *);
static bool lpfcProcessSLI2Event(struct lpfc_hba *, struct lpfcdfc_host *,
			struct lpfc_sli_ring *, struct lpfc_iocbq *);
static bool lpfcProcessSLI34Event(struct lpfc_hba *, struct lpfcdfc_host *,
			struct lpfc_sli_ring *, struct lpfc_iocbq *);
static int lpfcdfc_loop_post_rxbufs(struct lpfc_hba *,
			uint16_t, size_t, struct lpfc_dmabuf *);
static int lpfc_ioctl_write_pci(struct lpfc_hba *, struct lpfcCmdInput *);
static int lpfc_ioctl_read_pci(struct lpfc_hba *, struct lpfcCmdInput *,
			       void *);
static int lpfc_ioctl_write_mem(struct lpfc_hba *, struct lpfcCmdInput *);
static int lpfc_ioctl_read_mem(struct lpfc_hba *, struct lpfcCmdInput *,
			       void *);
static int lpfc_ioctl_write_ctlreg(struct lpfc_hba *, struct lpfcCmdInput *);
static int lpfc_ioctl_read_ctlreg(struct lpfc_hba *, struct lpfcCmdInput *,
				  void *);
static int lpfc_ioctl_initboard(struct lpfc_hba *, struct lpfcCmdInput *, void *);
static int lpfc_ioctl_setdiag(struct lpfc_hba *, struct lpfcCmdInput *, void *);
static int lpfc_ioctl_send_scsi_fcp(struct lpfc_hba *, struct lpfcCmdInput *);
static int lpfc_ioctl_send_els(struct lpfc_hba *, struct lpfcCmdInput *,
			       void *);
static int lpfc_ioctl_send_mgmt_rsp(struct lpfc_hba *, struct lpfcCmdInput *);
static int lpfc_ioctl_send_mgmt_cmd(struct lpfc_hba *, struct lpfcCmdInput *,
				    void *);
static int lpfc_ioctl_mbox(struct lpfc_hba *, struct lpfcCmdInput *, void *);
static int lpfc_ioctl_linkinfo(struct lpfc_hba *, struct lpfcCmdInput *, void *);
static int lpfc_ioctl_ioinfo(struct lpfc_hba *, struct lpfcCmdInput *, void *);
static int lpfc_ioctl_nodeinfo(struct lpfc_hba *, struct lpfcCmdInput *, void *,
			       int);
static int lpfc_ioctl_getcfg(struct lpfc_hba *, struct lpfcCmdInput *, void *);
static int lpfc_ioctl_vport_getcfg(struct lpfc_hba *, struct lpfcCmdInput *, 
				void *);
static int lpfc_ioctl_setcfg(struct lpfc_hba *, struct lpfcCmdInput *);
static int lpfc_ioctl_vport_setcfg(struct lpfc_hba *, struct lpfcCmdInput *);
static int lpfc_ioctl_hba_get_event(struct lpfc_hba *, struct lpfcCmdInput *);
static int lpfc_ioctl_hba_set_event(struct lpfc_hba *, struct lpfcCmdInput *);
static int lpfc_ioctl_hba_unset_event(struct lpfc_hba *, struct lpfcCmdInput *);
static int lpfc_ioctl_list_bind(struct lpfc_hba *, struct lpfcCmdInput *,
				void *);
static int lpfc_ioctl_get_vpd(struct lpfc_hba *, struct lpfcCmdInput *, void *);
static struct lpfc_dmabufext *dfc_fcp_cmd_data_alloc(struct lpfc_hba *,
		   char *, struct ulp_bde64 *, uint32_t , struct lpfc_dmabuf *);
static int dfc_rsp_data_copy_to_buf(struct lpfc_hba *, uint8_t *,
				    struct lpfc_dmabuf *, uint32_t);
static int dfc_rsp_data_copy(struct lpfc_hba *, uint8_t *,
			     struct lpfc_dmabufext *, uint32_t);
static int lpfc_issue_ct_rsp(struct lpfc_hba *, uint32_t, struct lpfc_dmabuf *,
			     struct lpfc_dmabufext *);
static int lpfc_ioctl_vport_attrib(struct lpfc_hba *, struct lpfcCmdInput *,
				   void *);
static int lpfc_ioctl_vport_resrc(struct lpfc_hba *, struct lpfcCmdInput *,
				  void *);
static int lpfc_ioctl_vport_list(struct lpfc_hba *, struct lpfcCmdInput *,
				 void *);
static int lpfc_ioctl_npiv_ready(struct lpfc_hba *, struct lpfcCmdInput *,
				 void *);
static int lpfc_ioctl_vport_nodeinfo(struct lpfc_hba *, struct lpfcCmdInput *,
				     void *);
static int lpfc_ioctl_get_dumpregion(struct lpfc_hba *, struct lpfcCmdInput  *,
				     void *);
static int lpfc_ioctl_get_lpfcdfc_info(struct lpfc_hba *, struct lpfcCmdInput *,
				       void *);
static int lpfc_ioctl_loopback_mode(struct lpfc_hba *, struct lpfcCmdInput  *,
				    void *);
static int lpfc_ioctl_loopback_test(struct lpfc_hba *, struct lpfcCmdInput  *,
				    void *);
static struct lpfcdfc_host * lpfcdfc_host_from_hba(struct lpfc_hba *);
static int lpfc_ioctl_index_find(struct lpfcCmdInput *);
static bool lpfc_process_iocb_timeout(IOCB_TIMEOUT_T *);
static int lpfc_ioctl_issue_dump_mbox(struct lpfc_hba *, struct lpfcCmdInput *,
				     void *);
static int lpfc_ioctl_issue_update_mbox(struct lpfc_hba *,
					struct lpfcCmdInput *, void *);
static int lpfc_ioctl_get_fcf_list(struct lpfc_hba *, struct lpfcCmdInput *,
				   void *);
static bool adapter_is_blade_engine(struct lpfc_hba *);

/* values for a_topology */
#define LNK_LOOP                0x1
#define LNK_PUBLIC_LOOP         0x2
#define LNK_FABRIC              0x3
#define LNK_PT2PT               0x4

/* values for a_linkState */
#define LNK_DOWN                0x1
#define LNK_UP                  0x2
#define LNK_FLOGI               0x3
#define LNK_DISCOVERY           0x4
#define LNK_REDISCOVERY         0x5
#define LNK_READY               0x6
#define LNK_DOWN_PERSIST        0x7

/* Values for which IOCTL module to call. */
#define LPFC_DEBUG_IOCTL	1
#define LPFC_IOCTL		2
#define LPFC_HBAAPI_IOCTL	3
#define LPFC_IOCTL_CMD_ERROR	4

#define CT_CTX_SIZE             64

struct lpfcdfc_host {
	struct list_head node;
	int inst;
	struct lpfc_hba * phba;
	struct lpfc_vport *vport;
	struct Scsi_Host * host;
	struct pci_dev * dev;
	void (*base_ct_unsol_event)(struct lpfc_hba *,
				    struct lpfc_sli_ring *,
				    struct lpfc_iocbq *);

	void (*base_ct_unsol_abort)(struct lpfc_hba *, 
				    struct lpfc_sli_ring *, 
				    struct lpfc_iocbq *);
	/*
	 *  Boolean TRUE if SET_EVENT called.
	 *  FALSE if UNSET_EVENT called.
	 */
	bool    qEvents;		/* Queue control */
	uint8_t qCount;			/* Count of items on CT Queue */

	struct list_head evtQueue;	/* CT Event processing Queue */
	struct list_head diagQ;		/* LOOPBACK processing Queue */

	wait_queue_head_t waitQ;	/* Wait Queue */

	struct unsol_rcv_ct_ctx ct_ctx[CT_CTX_SIZE];
	uint32_t ctx_idx;
	uint32_t blocked;
	uint32_t ref_count;
};

DEFINE_MUTEX(lpfcdfc_lock);

static struct list_head lpfcdfc_hosts = LIST_HEAD_INIT(lpfcdfc_hosts);

static int lpfcdfc_major = 0;

#define min_value(x,y) ((x) < (y)) ? (x) : (y)

static bool
lpfc_process_iocb_timeout(IOCB_TIMEOUT_T *args)
{
	struct lpfc_hba *phba = NULL;
	struct lpfc_timedout_iocb_ctxt *iocb_ctxt = NULL;
	struct lpfc_iocbq *cmdiocbq = NULL;
	bool timeout_status = true;
	unsigned long iflags = 0;

	phba = args->phba;

	/*
 	 * If we fail to get this memory then things have gone
 	 * horribly wrong and recovery becomes impossible.
 	 */
	iocb_ctxt = kzalloc(sizeof(struct lpfc_timedout_iocb_ctxt), GFP_KERNEL);
	if (iocb_ctxt == NULL) {
		lpfc_printf_log(phba, KERN_ERR, LOG_EVENT,
			"1944 ENOMEM kernel memory resource unavailale\n");
		return false;
	}

	cmdiocbq = args->cmdiocbq;

	iocb_ctxt->rspiocbq   = args->rspiocbq;
	iocb_ctxt->lpfc_cmd   = args->lpfc_cmd;
	iocb_ctxt->dmabuf0    = args->db0;
	iocb_ctxt->dmabuf1    = args->db1;
	iocb_ctxt->dmabuf2    = args->db2;
	iocb_ctxt->dmabuf3    = args->db3;
	iocb_ctxt->dmabufext0 = args->dbext0;
	iocb_ctxt->dmabufext1 = args->dbext1;
	iocb_ctxt->dmabufext2 = args->dbext2;
	iocb_ctxt->dmabufext3 = args->dbext3;

	/* 
 	 * To account for a race condition try one more time to see if this
 	 * iocb completed.
 	 */ 
	spin_lock_irqsave(&phba->hbalock, iflags);
	if (cmdiocbq->iocb_flag & LPFC_IO_WAKE) {
		/* Caught race condition - handle normally */
		cmdiocbq->iocb_flag &= ~LPFC_IO_WAKE;
		timeout_status = false;
	}
	else {
		/* Timed out so issue callback context */
		cmdiocbq->context1  = iocb_ctxt;
		cmdiocbq->context2  = NULL;
		cmdiocbq->iocb_cmpl = lpfc_ioctl_timeout_iocb_cmpl;
		timeout_status = true;
	}
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	
	if (timeout_status == false) {
		lpfc_printf_log(phba, KERN_INFO, LOG_EVENT,
			"1953 Caught iocb timeout race condition\n");
		kfree(iocb_ctxt);
	}
	else {
		lpfc_printf_log(phba, KERN_INFO, LOG_EVENT,
			"1948 IOCB command timeout\n");
	}

	/* timeout_status true : iocbq command has timed out and has been handled here
 	 *                false: we caught a race condition and processing can proceed
 	 *                       normally.
 	 */ 
	return timeout_status;
}

void
lpfc_ioctl_timeout_iocb_cmpl(struct lpfc_hba * phba,
			     struct lpfc_iocbq * cmd_iocb_q,
			     struct lpfc_iocbq * rsp_iocb_q)
{
	struct lpfc_timedout_iocb_ctxt *iocb_ctxt = cmd_iocb_q->context1;

	if (!iocb_ctxt) {
		if (cmd_iocb_q->context2)
			lpfc_els_free_iocb(phba, cmd_iocb_q);
		else
			lpfc_sli_release_iocbq(phba,cmd_iocb_q);
		return;
	}

	/* Free lpfc_mbuf resources */
	if (iocb_ctxt->dmabuf0)
		(void) lpfc_mbuf_reclaim(phba, iocb_ctxt->dmabuf0);
	if (iocb_ctxt->dmabuf1)
		(void) lpfc_mbuf_reclaim(phba, iocb_ctxt->dmabuf1);
	if (iocb_ctxt->dmabuf2)
		(void) lpfc_mbuf_reclaim(phba, iocb_ctxt->dmabuf2);
	if (iocb_ctxt->dmabuf3)
		(void) lpfc_mbuf_reclaim(phba, iocb_ctxt->dmabuf3);

	/* Free lpfc_dmabufext resources */
	if (iocb_ctxt->dmabufext0)
		dfc_cmd_data_free(phba, iocb_ctxt->dmabufext0);
	if (iocb_ctxt->dmabufext1)
		dfc_cmd_data_free(phba, iocb_ctxt->dmabufext1);
	if (iocb_ctxt->dmabufext2)
		dfc_cmd_data_free(phba, iocb_ctxt->dmabufext2);
	if (iocb_ctxt->dmabufext3)
		dfc_cmd_data_free(phba, iocb_ctxt->dmabufext3);

	lpfc_sli_release_iocbq(phba,cmd_iocb_q);

	if (iocb_ctxt->rspiocbq)
		lpfc_sli_release_iocbq(phba, iocb_ctxt->rspiocbq);

	kfree(iocb_ctxt);
	return;
}

int
lpfc_ioctl_write_pci(struct lpfc_hba * phba, struct lpfcCmdInput * cip)
{
	uint32_t offset, cnt;
	int i;
	uint32_t *buffer, *p_buf;
	struct lpfc_vport *vport = phba->pport;

	offset = (ulong) cip->lpfc_arg1;
	cnt = (ulong) cip->lpfc_arg2;

	if (!(vport->fc_flag & FC_OFFLINE_MODE))
		return EPERM;

	if ((cnt + offset) > 256)
		return ERANGE;

	buffer = kmalloc(4096, GFP_ATOMIC);
	if (!buffer)
		return ENOMEM;

	if (copy_from_user(buffer, cip->lpfc_dataout, cnt)) {
		kfree(buffer);
		return EIO;
	}

	if (!(vport->fc_flag & FC_OFFLINE_MODE)) {
		kfree(buffer);
		return EPERM;
	}

	p_buf = buffer;
	for (i = offset; i < (offset + cnt); i += 4) {
		pci_write_config_dword(phba->pcidev, i, *p_buf);
		p_buf++;
	}

	kfree(buffer);
	return 0;
}

int
lpfc_ioctl_read_pci(struct lpfc_hba * phba, struct lpfcCmdInput * cip, void *dataout)
{
	uint32_t offset, cnt;
	uint32_t *destp;
	int i;

	offset = (ulong) cip->lpfc_arg1;
	cnt = (ulong) cip->lpfc_arg2;
	destp = (uint32_t *) dataout;

	if ((cnt + offset) > 4096)
		return ERANGE;

	for (i = offset; i < (offset + cnt); i += 4) {
		pci_read_config_dword(phba->pcidev, i, destp);
		destp++;
	}

	return 0;
}

int
lpfc_ioctl_write_mem(struct lpfc_hba * phba, struct lpfcCmdInput * cip)
{
	uint32_t offset, cnt;
	struct lpfc_sli *psli;
	uint8_t *buffer;
	struct lpfc_vport *vport = phba->pport;

	psli = &phba->sli;
	offset = (ulong) cip->lpfc_arg1;
	cnt = (ulong) cip->lpfc_arg2;

	if (!(vport->fc_flag & FC_OFFLINE_MODE)) {
		if (offset != 256)
			return EPERM;

		/* Allow writing of first 128 bytes after mailbox in online mode */
		if (cnt > 128)
			return EPERM;
	}

	if (offset >= 4096)
		return ERANGE;

	cnt = (ulong) cip->lpfc_arg2;
	if ((cnt + offset) > 4096)
		return ERANGE;

	buffer =  kmalloc(4096, GFP_KERNEL);
	if (!buffer)
		return ENOMEM;

	if (copy_from_user((uint8_t *) buffer, (uint8_t *) cip->lpfc_dataout,
			   (ulong) cnt)) {
		kfree(buffer);
		return EIO;
	}

	if (psli->sli_flag & LPFC_SLI_ACTIVE) {
		/* copy into SLIM2 */
		lpfc_sli_pcimem_bcopy((uint32_t *) buffer,
				     ((uint32_t *) phba->slim2p + offset),
				     cnt >> 2);
	} else {
		/* First copy command data */
		lpfc_memcpy_to_slim(phba->MBslimaddr, (void *)buffer, cnt);
	}
	kfree(buffer);
	return 0;
}

int
lpfc_ioctl_read_mem(struct lpfc_hba * phba, struct lpfcCmdInput * cip, void *dataout)
{
	uint32_t offset, cnt;
	struct lpfc_sli *psli;
	int i;

	psli = &phba->sli;
	offset = (ulong) cip->lpfc_arg1;
	cnt = (ulong) cip->lpfc_arg2;

	if (psli->sli_flag & LPFC_SLI_ACTIVE) {
		/* The SLIM2 size is stored in the next field */
		i = SLI2_SLIM_SIZE;
	} else {
		i = 4096;
	}

	if (offset >= i)
		return ERANGE;

	if ((cnt + offset) > i) {
		/* Adjust cnt instead of error ret */
		cnt = (i - offset);
	}

	if (psli->sli_flag & LPFC_SLI_ACTIVE) {
		/* copy results back to user */
		lpfc_sli_pcimem_bcopy((uint32_t *) phba->slim2p,
				     (uint32_t *) dataout, cnt);
	} else {
		/* First copy command data from SLIM */
		lpfc_memcpy_from_slim( dataout,
			       phba->MBslimaddr,
			       MAILBOX_CMD_SIZE );		
	}
	return 0;
}

int
lpfc_ioctl_write_ctlreg(struct lpfc_hba * phba,
			struct lpfcCmdInput * cip)
{
	uint32_t offset, incr;
	struct lpfc_sli *psli;
	struct lpfc_vport *vport = phba->pport;

	psli = &phba->sli;
	offset = (ulong) cip->lpfc_arg1;
	incr = (ulong) cip->lpfc_arg2;

	if (!(vport->fc_flag & FC_OFFLINE_MODE))
		return EPERM;

	if (offset > 255)
		return ERANGE;

	if (offset % 4)
		return EINVAL;

	writel(incr, (phba->ctrl_regs_memmap_p) + offset);
	return 0;
}

int
lpfc_ioctl_read_ctlreg(struct lpfc_hba * phba, struct lpfcCmdInput *cip, void *dataout)
{
	uint32_t offset, incr;

	offset = (ulong) cip->lpfc_arg1;

	if (offset > 255)
		return ERANGE;

	if (offset % 4)
		return EINVAL;

	incr = readl((phba->ctrl_regs_memmap_p) + offset);
	*((uint32_t *) dataout) = incr;

	return 0;
}

int
lpfc_ioctl_initboard(struct lpfc_hba * phba, struct lpfcCmdInput *cip,
		     void *dataout)
{
	struct pci_dev *pdev;
	struct dfc_info *di;
	char lpfc_fwrevision[32];

	pdev = phba->pcidev;
	if (!pdev)
		return 1;

	di = (struct dfc_info *) dataout;
	di->a_onmask = (ONDI_MBOX | ONDI_RMEM | ONDI_RPCI | ONDI_RCTLREG |
			ONDI_IOINFO | ONDI_LNKINFO | ONDI_NODEINFO |
			ONDI_CFGPARAM | ONDI_CT | ONDI_HBAAPI);
	di->a_offmask = (OFFDI_MBOX | OFFDI_RMEM | OFFDI_WMEM | OFFDI_RPCI |
			 OFFDI_WPCI | OFFDI_RCTLREG | OFFDI_WCTLREG);

	if (phba->pport->fc_flag & FC_OFFLINE_MODE)
		di->a_offmask |= OFFDI_OFFLINE;

	di->a_onmask |= ONDI_SLI2;

	/* set endianness of driver diagnotic interface */
#if __BIG_ENDIAN
	di->a_onmask |= ONDI_BIG_ENDIAN;
#else	/*  __LITTLE_ENDIAN */
	di->a_onmask |= ONDI_LTL_ENDIAN;
#endif

	di->a_pci = ((((uint32_t) pdev->device) << 16) |
		     (uint32_t) (pdev->vendor));

	di->a_ddi = phba->brd_no;

	di->a_pciFunc = PCI_FUNC(phba->pcidev->devfn);

	if (pdev->bus)
		di->a_busid = (uint32_t) (pdev->bus->number);
	else
		di->a_busid = 0;

	di->a_devid = (uint32_t) (pdev->devfn);

	sprintf((char *) di->a_drvrid, "%s", LPFC_DRIVER_VERSION);
	lpfc_decode_firmware_rev(phba, lpfc_fwrevision, 1);
	sprintf((char *) di->a_fwname, "%s", lpfc_fwrevision);

	memcpy(di->a_wwpn, &phba->pport->fc_portname, sizeof(struct lpfc_name));
	cip->lpfc_outsz = sizeof (struct dfc_info);
	return 0;
}
int
lpfc_ioctl_setdiag(struct lpfc_hba *phba, struct lpfcCmdInput *cip,
		   void *dataout)
{
	uint32_t offset;
	int rc = 0;
	offset = (ulong) cip->lpfc_arg1;

	switch (offset) {
	case DDI_ONDI:
	case DDI_OFFDI:
	case DDI_SHOW:
		rc = ENXIO;
		break;

	case DDI_BRD_ONDI:
	case DDI_BRD_OFFDI:
	case DDI_BRD_WARMDI:
	case DDI_BRD_DIAGDI:
		rc = lpfc_board_mode_set(phba, offset, (uint32_t *) dataout);
		break;

	case DDI_BRD_SHOW:
		rc = lpfc_board_mode_get(phba, (uint32_t *) dataout);
		break;
	default:
		rc = ERANGE;
		break;
	}

	return rc;
}


int
lpfc_ioctl_send_scsi_fcp(struct lpfc_hba * phba,
			 struct lpfcCmdInput * cip)
{
	struct lpfc_sli *psli = &phba->sli;
	int reqbfrcnt;
	int snsbfrcnt;
	int i, j = 0;
  	struct fcp_cmnd *fcpcmd;
	struct fcp_rsp *fcprsp;
	struct ulp_bde64 *bpl;
	struct lpfc_nodelist *pndl;
	struct lpfc_iocbq *cmdiocbq = 0;
	struct lpfc_iocbq *rspiocbq = 0;
  	struct lpfc_dmabufext *outdmp = 0;
	IOCB_t *cmd = 0;
	IOCB_t *rsp = 0;
	struct lpfc_dmabuf *mp = 0;
	struct lpfc_dmabuf *bmp = 0;
	char *outdta;
	uint32_t clear_count;
	int rc = 0;
	uint32_t iocb_wait_timeout = cip->lpfc_arg5;
	uint32_t iocb_retries;
	struct lpfc_vport *vport = NULL;
	struct lpfc_vport **vports = NULL;
	struct lpfc_dmabuf *mlast, *mlast_next;
	IOargUn arg3;
	int found = 0;

	/*
	 * Rspcnt is really data buffer size
	 * Snscnt is sense count in case of LPFC_HBA_SEND_SCSI or
	 * it is fcp response size in case of LPFC_HBA_SEND_FCP
	 */
	struct {
		uint32_t rspcnt;
		uint32_t snscnt;
	} count;


	if (phba->sli.sli_flag & LPFC_BLOCK_MGMT_IO)
		return EAGAIN;

	reqbfrcnt = cip->lpfc_arg4;
	snsbfrcnt = cip->lpfc_flag;

	if ((reqbfrcnt + cip->lpfc_outsz) > (80 * 4096)) {
		rc = ERANGE;
		goto sndsczout;
	}

	if (copy_from_user((uint8_t *) &arg3, (uint8_t *) cip->lpfc_arg3,
			   (ulong) (sizeof (IOargUn)))) {
		rc = EIO;
		goto sndsczout;
	}

	/*
	 * Determine whether the application is sending the command on a 
	 * vport or the physical port.  Presume it's the physical port
	 * and search for a vport.
	 */
	vport = phba->pport;
	if ((uint64_t)arg3.Iarg.vport_wwpn.wwn[0]) {
		vports = lpfc_create_vport_work_array(phba);
		if (!vports)
			return ENOMEM;

		for (i = 0; i <= phba->max_vpi && vports[i] != NULL; i++) {
			if (vports[i]->load_flag)
				continue;
			rc = memcmp(&arg3.Iarg.vport_wwpn,
			            &vports[i]->fc_portname, sizeof (HBA_WWN));
			if (rc == 0) {
				found = 1;
				vport = vports[i];
				break;
			}
		}

		lpfc_destroy_vport_work_array(phba, vports);
		if (!found) {
			rc = ENOENT;
			goto sndsczout;
                }
	}

	/* Using the target wwpn, get the target node pointer. */
	pndl = lpfc_findnode_wwpn(vport, (struct lpfc_name*)&arg3.Iarg.targ_wwpn);
	if (pndl) {
		if (pndl->nlp_state != NLP_STE_MAPPED_NODE) {
                        pndl = (struct lpfc_nodelist *) 0;
			rc = EACCES;
			goto sndsczout;
		}
	} else {
		rc = EACCES;
		goto sndsczout;
	}

	if (!(psli->sli_flag & LPFC_SLI_ACTIVE)) {
		rc = EACCES;
		goto sndsczout;
	}

	if (pndl->nlp_flag & NLP_ELS_SND_MASK) {
		rc = ENODEV;
		goto sndsczout;
	}

	/* Allocate buffer for command iocb */
	if ((cmdiocbq = lpfc_sli_get_iocbq(phba)) == 0) {
		rc = ENOMEM;
		goto sndsczout;
	}

	cmdiocbq->vport = vport;
	cmd = &(cmdiocbq->iocb);

	/* Allocate buffer for response iocb */
	if ((rspiocbq = lpfc_sli_get_iocbq(phba)) == 0) {
		rc = ENOMEM;
		goto sndsczout;
	}

	rsp = &(rspiocbq->iocb);
	rspiocbq->vport = vport;

	/* Allocate buffer for Buffer ptr list */
	if (((bmp = kmalloc(sizeof (struct lpfc_dmabuf), GFP_KERNEL)) == 0) ||
	    ((bmp->virt = lpfc_mbuf_alloc(phba, 0, &(bmp->phys))) == 0)) {
		if (bmp)
			kfree(bmp);
		bmp = NULL;
		rc = ENOMEM;
		goto sndsczout;
	}
	INIT_LIST_HEAD(&bmp->list);
	bpl = (struct ulp_bde64 *) bmp->virt;

	/* Allocate buffer for FCP CMND / FCP RSP */
	if (((mp = kmalloc(sizeof (struct lpfc_dmabuf), GFP_KERNEL)) == 0) ||
	    ((mp->virt = lpfc_mbuf_alloc(phba, MEM_PRI, &(mp->phys))) == 0)) {
		if (mp)
			kfree(mp);
		mp = NULL;
		rc = ENOMEM;
		goto sndsczout;
	}

	INIT_LIST_HEAD(&mp->list);
	fcpcmd = (struct fcp_cmnd *)(uint8_t *) mp->virt;
	fcprsp = (struct fcp_rsp *) ((uint8_t *) mp->virt + sizeof (struct fcp_cmnd));
	memset((void *)fcpcmd, 0, sizeof (struct fcp_cmnd) + sizeof (struct fcp_rsp));

	/* Setup FCP CMND and FCP RSP */
	bpl->addrHigh = le32_to_cpu( putPaddrHigh(mp->phys) );
	bpl->addrLow = le32_to_cpu( putPaddrLow(mp->phys) );
	bpl->tus.f.bdeSize = sizeof (struct fcp_cmnd);
	bpl->tus.f.bdeFlags = BUFF_TYPE_BDE_64;
	bpl->tus.w = le32_to_cpu(bpl->tus.w);
	bpl++;
	bpl->addrHigh = le32_to_cpu(putPaddrHigh(mp->phys + sizeof (struct fcp_cmnd)));
	bpl->addrLow = le32_to_cpu(putPaddrLow(mp->phys + sizeof (struct fcp_cmnd)));
	bpl->tus.f.bdeSize = sizeof (struct fcp_rsp);
	bpl->tus.f.bdeFlags = BUFF_TYPE_BDE_64;
	bpl->tus.w = le32_to_cpu(bpl->tus.w);
	bpl++;

	/*
	 * Copy user data into fcpcmd buffer at this point to see if its a read
	 * or a write.
	 */
	if (copy_from_user((uint8_t *) fcpcmd, (uint8_t *) cip->lpfc_arg1,
			   (ulong) (reqbfrcnt))) {
		rc = EIO;
		goto sndsczout;
	}

	outdta = (char *)(fcpcmd->fcpCntl3 == WRITE_DATA ? cip->lpfc_dataout : 0);

	/* Allocate data buffer, and fill it if its a write */
	if (cip->lpfc_outsz == 0)
		outdmp = dfc_fcp_cmd_data_alloc(phba, outdta, bpl, 512, bmp);
	else
		outdmp = dfc_fcp_cmd_data_alloc(phba, outdta, bpl, cip->lpfc_outsz, bmp);

	if (outdmp == 0) {
		rc = ENOMEM;
		goto sndsczout;
	}

	cmd->un.fcpi64.bdl.ulpIoTag32 = 0;
	cmd->un.fcpi64.bdl.addrHigh = putPaddrHigh(bmp->phys);
	cmd->un.fcpi64.bdl.addrLow = putPaddrLow(bmp->phys);
	cmd->un.fcpi64.bdl.bdeSize = (3 * sizeof (struct ulp_bde64));
	cmd->un.fcpi64.bdl.bdeFlags = BUFF_TYPE_BLP_64;
	cmd->ulpBdeCount = 1;
	cmd->ulpContext = pndl->nlp_rpi;
	cmd->ulpClass = pndl->nlp_fcp_info & 0x0f;
	cmd->ulpOwner = OWN_CHIP;
	cmd->ulpTimeout = 30 + 16;
	cmd->ulpLe = 1;

	if (pndl->nlp_fcp_info & NLP_FCP_2_DEVICE)
		cmd->ulpFCP2Rcvy = 1;

	if ((fcpcmd->fcpCntl3) && (fcpcmd->fcpDl == 0))
		fcpcmd->fcpCntl3 = 0;

	switch (fcpcmd->fcpCntl3) {
	case READ_DATA:
		cmd->ulpCommand = CMD_FCP_IREAD64_CR;
		cmd->ulpPU = PARM_READ_CHECK;
		cmd->un.fcpi.fcpi_parm = cip->lpfc_outsz;
	  	cmd->un.fcpi64.bdl.bdeSize =
  		    ((outdmp->flag + 2) * sizeof (struct ulp_bde64));
		break;
	case WRITE_DATA:
		cmd->ulpCommand = CMD_FCP_IWRITE64_CR;
	  	cmd->un.fcpi64.bdl.bdeSize =
  		    ((outdmp->flag + 2) * sizeof (struct ulp_bde64));
		break;
	default:
		cmd->ulpCommand = CMD_FCP_ICMND64_CR;
		cmd->un.fcpi64.bdl.bdeSize = (2 * sizeof (struct ulp_bde64));
		break;
	}

	cmdiocbq->context1 = pndl;
	cmdiocbq->context2 = NULL;
	cmdiocbq->context3 = bmp;
	cmdiocbq->iocb_flag |= (LPFC_IO_LIBDFC | LPFC_IO_FCP);

	/* Set up the timeout value for the iocb wait command. */
	if (iocb_wait_timeout == 0) {
	        iocb_wait_timeout = 30 + 16 + LPFC_DRVR_TIMEOUT;
		iocb_retries = 4;
	} else {
		iocb_retries = 1;
	}

	cmdiocbq->vport = vport;
	rc = lpfc_sli_issue_iocb_wait(phba, LPFC_FCP_RING, cmdiocbq, rspiocbq,
				      iocb_wait_timeout);
	if (rc == IOCB_TIMEDOUT) {
		bool timedout = true;
		IOCB_TIMEOUT_T args;

		(void) memset(&args, 0, sizeof(IOCB_TIMEOUT_T));

		args.phba     = phba;
		args.cmdiocbq = cmdiocbq;
		args.rspiocbq = rspiocbq;
		args.db0      = mp;
		args.db1      = bmp;
		args.dbext0   = outdmp;

		timedout = lpfc_process_iocb_timeout(&args);

		if (timedout)
			return ETIMEDOUT;
		else
			rc = IOCB_SUCCESS;
 	}

	if (rc == IOCB_BUSY) {
		rc = EAGAIN;
		goto sndsczout;
	}

	if (rc != IOCB_SUCCESS) {
		rc = EIO;
		goto sndsczout;
	}

	/*
	 * For LPFC_HBA_SEND_FCP, just return struct fcp_rsp unless we got
	 * an IOSTAT_LOCAL_REJECT.
	 *
	 * For SEND_FCP case, snscnt is really struct fcp_rsp length. In the
	 * switch statement below, the snscnt should not get destroyed.
	 */
	if (cmd->ulpCommand == CMD_FCP_IWRITE64_CX) {
		clear_count = (rsp->ulpStatus == IOSTAT_SUCCESS ? 1 : 0);
	} else {
		clear_count = cmd->un.fcpi.fcpi_parm;
	}
	if ((cip->lpfc_cmd == LPFC_HBA_SEND_FCP) &&
	    (rsp->ulpStatus != IOSTAT_LOCAL_REJECT)) {
		if (snsbfrcnt < sizeof (struct fcp_rsp))
			count.snscnt = snsbfrcnt;
		else
			count.snscnt = sizeof (struct fcp_rsp);

		if (copy_to_user((uint8_t *) cip->lpfc_arg2, (uint8_t *) fcprsp,
				 count.snscnt)) {
			rc = EIO;
			goto sndsczout;
		}
	}

	switch (rsp->ulpStatus) {
	case IOSTAT_SUCCESS:
 cpdata:
		if (cip->lpfc_outsz < clear_count) {
			cip->lpfc_outsz = 0;
			rc = ERANGE;
			break;
		}
		cip->lpfc_outsz = clear_count;
		if (cip->lpfc_cmd == LPFC_HBA_SEND_SCSI) {
			count.rspcnt = cip->lpfc_outsz;
			count.snscnt = 0;
		} else {
			/* For LPFC_HBA_SEND_FCP, snscnt is already set */
			count.rspcnt = cip->lpfc_outsz;
		}

		/* Return data length */
		if (copy_to_user((uint8_t *)cip->lpfc_arg3, 
				(uint8_t *)&count,
				 (2 * sizeof (uint32_t)))) {
			rc = EIO;
			break;
		}

		cip->lpfc_outsz = 0;
		if (count.rspcnt) {
			if (dfc_rsp_data_copy
			    (phba, (uint8_t *) cip->lpfc_dataout, outdmp,
			     count.rspcnt)) {
				rc = EIO;
				break;
			}
		}

		break;
	case IOSTAT_LOCAL_REJECT:
		cip->lpfc_outsz = 0;
		if (rsp->un.grsp.perr.statLocalError == IOERR_SEQUENCE_TIMEOUT) {
			rc = ETIMEDOUT;
			break;
		}
		rc = EFAULT;

		/* count.rspcnt and count.snscnt is already 0 */
		goto sndsczout;
	case IOSTAT_FCP_RSP_ERROR:
		/* 
		 * At this point, clear_count is the residual count. 
		 * We are changing it to the amount actually xfered.
		 */
		if (fcpcmd->fcpCntl3 == READ_DATA) {
			if ((fcprsp->rspStatus2 & RESID_UNDER)
			    && (fcprsp->rspStatus3 == 0)) {
				goto cpdata;
			}
		} else {
			clear_count = 0;
		}

		count.rspcnt = (uint32_t) clear_count;
		cip->lpfc_outsz = 0;
		if (fcprsp->rspStatus2 & RSP_LEN_VALID)
			j = be32_to_cpu(fcprsp->rspRspLen);

		if (fcprsp->rspStatus2 & SNS_LEN_VALID) {
			if (cip->lpfc_cmd == LPFC_HBA_SEND_SCSI) {
				if (snsbfrcnt < be32_to_cpu(fcprsp->rspSnsLen))
					count.snscnt = snsbfrcnt;
				else
					count.snscnt = be32_to_cpu(fcprsp->rspSnsLen);

				/* Return sense info from rsp packet */
				if (copy_to_user((uint8_t *) cip->lpfc_arg2,
						 ((uint8_t *) & fcprsp->rspInfo0) + j,
						 count.snscnt)) {
					rc = EIO;
					break;
				}
			}
		} else {
			rc = EFAULT;
			break;
		}

		if (copy_to_user((uint8_t *)cip->lpfc_arg3, 
				(uint8_t *)&count,
				 (2 * sizeof(uint32_t)))) {
			rc = EIO;
			break;
		}

		/* return data for read */
		if (count.rspcnt) {
			if (dfc_rsp_data_copy(phba, (uint8_t *) cip->lpfc_dataout,
					      outdmp, count.rspcnt)) {
				rc = EIO;
				break;
			}
		}
		break;
	default:
		cip->lpfc_outsz = 0;
		rc = EFAULT;
		break;
	}

 sndsczout:
	if (outdmp)
		dfc_cmd_data_free(phba, outdmp);

	if (mp)
		(void) lpfc_mbuf_reclaim(phba, mp);

	if (bmp) {
		list_for_each_entry_safe(mlast, mlast_next, &bmp->list, list) {
			list_del(&mlast->list);
			(void) lpfc_mbuf_reclaim(phba, mlast);
		}

		(void) lpfc_mbuf_reclaim(phba, bmp);
	}

	if (cmdiocbq)
		lpfc_sli_release_iocbq(phba, cmdiocbq);

	if (rspiocbq)
		lpfc_sli_release_iocbq(phba, rspiocbq);

	return rc;
}


static int
lpfc_ioctl_send_els(struct lpfc_hba * phba, struct lpfcCmdInput * cip, void *dataout)
{
	struct lpfc_iocbq *cmdiocbq, *rspiocbq;
	struct lpfc_dmabufext *pcmdext = NULL, *prspext = NULL;
	struct lpfc_nodelist *ndlp, *ndlp_stat;
	struct ulp_bde64 *bpl;
	IOCB_t *rsp;
	struct lpfc_dmabuf *pcmd, *prsp, *pbuflist = NULL;
	uint16_t rpi = 0;
	struct nport_id destID;
	int rc = 0;
	uint32_t cmdsize;
	uint32_t rspsize;
	uint32_t elscmd;
	int iocb_status;

	if (phba->sli.sli_flag & LPFC_BLOCK_MGMT_IO)
		return EAGAIN;

	/* Get the command and response buffers sizes passed by the caller. */
	cmdsize = cip->lpfc_arg4;
	rspsize = cip->lpfc_outsz;

	/* Copy the destination DID by-reference. */
	if (copy_from_user((uint8_t *)&destID, (void __user *)cip->lpfc_arg1,
			   sizeof(struct nport_id)))
		return EIO;

	/* Copy just the ELS command value from the ELS command payload. */
	if (copy_from_user((uint8_t *)&elscmd, (void __user *)cip->lpfc_arg2,
			   min_value(sizeof(uint32_t), cmdsize)))
		return EIO;

	/* Filter what is and is not allowed */
	switch (elscmd) {
	case ELS_CMD_PLOGI:
	case ELS_CMD_FLOGI:
	case ELS_CMD_LOGO:
	case ELS_CMD_PRLI:
	case ELS_CMD_PRLO:
	case ELS_CMD_FDISC:
		return EIO;
	}

	if ((rspiocbq = lpfc_sli_get_iocbq(phba)) == NULL)
		return ENOMEM;

	rsp = &rspiocbq->iocb;

	switch (destID.idType) {
	case NPORT_ID_TYPE_WWPN:
		ndlp = lpfc_findnode_wwpn(phba->pport,
					  (struct lpfc_name *) destID.wwpn);
		break;
	case NPORT_ID_TYPE_DID:
		destID.d_id = (destID.d_id & Mask_DID);
		ndlp = lpfc_findnode_did(phba->pport, destID.d_id);
		break;
	case NPORT_ID_TYPE_WWNN:
	default:
		ndlp = NULL;
		return EIO;
	}

	if (ndlp && !(lpfc_is_unmapped_node_logged_in(phba->pport,
							ndlp->nlp_DID))) {
		lpfc_sli_release_iocbq(phba, rspiocbq);
		return ENODEV;
	}

	ndlp_stat = lpfc_nlp_get(ndlp);
	if (!ndlp_stat) {
		lpfc_sli_release_iocbq(phba, rspiocbq);
		return ENODEV;
	}
	rpi = ndlp->nlp_rpi;

	cmdiocbq = lpfc_prep_els_iocb(phba->pport, 1, cmdsize, 0, ndlp,
				      ndlp->nlp_DID, elscmd);
	if (cmdiocbq == NULL) {
		lpfc_sli_release_iocbq(phba, rspiocbq);
		return EIO;
	}

	pcmd = (struct lpfc_dmabuf *) cmdiocbq->context2;
	prsp = (struct lpfc_dmabuf *) pcmd->list.next;

	/*
	 * If the caller's command or response size exceeds the driver's
	 * 1KB mbuf default, cleanup and resize the command/response buffers.
	 */
	if ((cmdsize > LPFC_BPL_SIZE) || (rspsize > LPFC_BPL_SIZE)) {
		(void) lpfc_mbuf_reclaim(phba, pcmd);
		(void) lpfc_mbuf_reclaim(phba, prsp);
		cmdiocbq->context2 = NULL;

		/*
		 * The pbuflist embedded in context3 does not require a resizing
		 * as it only contains the DMA addresses and sizes of the
		 * command and response. Just reallocate the command and
		 * response and rebuild the bdl.  For pcmdext, cip->lpfc_arg2
		 * contains the entire ELS command payload.
		 */
		pbuflist = (struct lpfc_dmabuf *) cmdiocbq->context3;
		bpl = (struct ulp_bde64 *) pbuflist->virt;
		pcmdext = dfc_cmd_data_alloc(phba, cip->lpfc_arg2, bpl, cmdsize);
		if (!pcmdext) {
			lpfc_els_free_iocb(phba, cmdiocbq);
			lpfc_sli_release_iocbq(phba, rspiocbq);
			return ENOMEM;
		}
		bpl += pcmdext->flag;
		prspext = dfc_cmd_data_alloc(phba, NULL, bpl, rspsize);
		if (!prspext) {
			dfc_cmd_data_free(phba, pcmdext);
			lpfc_els_free_iocb(phba, cmdiocbq);
			lpfc_sli_release_iocbq(phba, rspiocbq);
			return ENOMEM;
		}
	} else {
		/* Copy the command from user space */
		if (copy_from_user((uint8_t *) pcmd->virt,
				   (void __user *) cip->lpfc_arg2,
				   cmdsize)) {
			lpfc_els_free_iocb(phba, cmdiocbq);
			lpfc_sli_release_iocbq(phba, rspiocbq);
			return EIO;
		}
	}

	/*
	 * The ndlp is no longer needed.  Release the reference count and clear
	 * context1. No memory was allocated in context1.  Also note that the
	 * cmdiocbq->context3 field was initialized by the lpfc_prep_els_iocb
	 * routine.
	 */
	lpfc_nlp_put(ndlp);
	cmdiocbq->context2 = NULL;
	cmdiocbq->iocb_flag |= LPFC_IO_LIBDFC;
	iocb_status = lpfc_sli_issue_iocb_wait(phba, LPFC_ELS_RING, cmdiocbq, rspiocbq,
				      (phba->fc_ratov * 2) + LPFC_DRVR_TIMEOUT);
	if (iocb_status == IOCB_TIMEDOUT) {
		bool timedout = true;
		IOCB_TIMEOUT_T args;

		(void) memset(&args, 0, sizeof(IOCB_TIMEOUT_T));

		args.phba     = phba;
		args.cmdiocbq = cmdiocbq;
		args.rspiocbq = rspiocbq;
		args.db0      = pcmd;
		args.db1      = prsp;
		args.db2      = pbuflist;
		args.dbext0   = pcmdext;
		args.dbext0   = prspext;

		timedout = lpfc_process_iocb_timeout(&args);

		if (timedout)
			return ETIMEDOUT;
		else
			iocb_status = IOCB_SUCCESS;
	}

	if (iocb_status == IOCB_SUCCESS) {
		if (rsp->ulpStatus == IOSTAT_SUCCESS) {
			if (rspsize < (rsp->un.ulpWord[0] & 0xffffff)) {
				rc = ERANGE;
			} else {
				rspsize = rsp->un.ulpWord[0] & 0xffffff;
				if (pbuflist) {
					if (dfc_rsp_data_copy(
						phba,
						(uint8_t *) cip->lpfc_dataout,
						prspext,
						rspsize)) {
						rc = EIO;
					} else {
						cip->lpfc_outsz = 0;
					}
				} else {
					if (copy_to_user( (void __user *)
						cip->lpfc_dataout,
						(uint8_t *) prsp->virt,
						rspsize)) {
						rc = EIO;
					} else {
						cip->lpfc_outsz = 0;
					}
				}
			}
		} else if (rsp->ulpStatus == IOSTAT_LS_RJT) {
			uint8_t ls_rjt[8];

			/* construct the LS_RJT payload */
			ls_rjt[0] = 0x01;
			ls_rjt[1] = 0x00;
			ls_rjt[2] = 0x00;
			ls_rjt[3] = 0x00;
			memcpy(&ls_rjt[4], (uint8_t *) &rsp->un.ulpWord[4],
			       sizeof(uint32_t));

			if (rspsize < 8)
				rc = ERANGE;
			else
				rspsize = 8;

			memcpy(dataout, ls_rjt, rspsize);
		} else
			rc = EIO;

		if (copy_to_user((void __user *)cip->lpfc_arg3,
				 (uint8_t *)&rspsize, sizeof(uint32_t)))
			rc = EIO;

	} else if (rc == IOCB_BUSY)
		rc = EAGAIN;

	else
		rc = EIO;

	if (pbuflist) {
		dfc_cmd_data_free(phba, pcmdext);
		dfc_cmd_data_free(phba, prspext);
	} else
		cmdiocbq->context2 = (uint8_t *) pcmd;

	if (iocb_status != IOCB_TIMEDOUT)
		lpfc_els_free_iocb(phba, cmdiocbq);

	lpfc_sli_release_iocbq(phba, rspiocbq);
	return rc;
}

static int
lpfc_ioctl_send_mgmt_rsp(struct lpfc_hba * phba,
			 struct lpfcCmdInput * cip)
{
	struct ulp_bde64 *bpl;
	struct lpfc_dmabuf *bmp = NULL;
	struct lpfc_dmabufext *indmp = NULL;
	uint32_t tag =  (uint32_t)cip->lpfc_flag; /* XRI for XMIT_SEQUENCE */
	unsigned long reqbfrcnt = (unsigned long)cip->lpfc_arg2;
	int rc = 0;

	if (!reqbfrcnt || (reqbfrcnt > (80 * BUF_SZ_4K))) {
		rc = ERANGE;
		return rc;
	}

	bmp = kmalloc(sizeof (struct lpfc_dmabuf), GFP_KERNEL);
	if (!bmp) {
		rc = ENOMEM;
		goto send_mgmt_rsp_exit;
	}

	bmp->virt = lpfc_mbuf_alloc(phba, 0, &bmp->phys);
	if (!bmp->virt) {
		rc = ENOMEM;
		goto send_mgmt_rsp_free_bmp;
	}

	INIT_LIST_HEAD(&bmp->list);
	bpl = (struct ulp_bde64 *) bmp->virt;

	indmp = dfc_cmd_data_alloc(phba, cip->lpfc_arg1, bpl, reqbfrcnt);
	if (!indmp) {
		rc = ENOMEM;
		goto send_mgmt_rsp_free_bmpvirt;
	}

	rc = lpfc_issue_ct_rsp(phba, tag, bmp, indmp);
	if (rc == ETIMEDOUT) {
		/* Resources have already been released */
		goto send_mgmt_rsp_exit;
	}
	else if (rc != IOCB_SUCCESS)
		rc = EACCES;

	dfc_cmd_data_free(phba, indmp);

send_mgmt_rsp_free_bmpvirt:
send_mgmt_rsp_free_bmp:
	(void) lpfc_mbuf_reclaim(phba, bmp);
send_mgmt_rsp_exit:
	return rc;
}

typedef struct lpfc_build_iocb {
	struct lpfc_iocbq *cmdiocbq;
	struct lpfc_iocbq *rspiocbq;
	int    cmdsize;
	int    rspsize;
	struct lpfc_dmabuf *bmp;
	struct lpfc_dmabufext *indmp; 
	struct lpfc_dmabufext *outdmp;
	uint8_t   *datain_ptr;
}lpfc_build_iocb_t;

void
lpfc_iocb_cleanup(struct lpfc_hba *phba,
	struct lpfc_build_iocb *piocb)
{
	if (piocb->outdmp)
		dfc_cmd_data_free(phba, piocb->outdmp);
	if (piocb->indmp)
		dfc_cmd_data_free(phba, piocb->indmp);

	(void) lpfc_mbuf_reclaim(phba, piocb->bmp);

	if (piocb->rspiocbq)
        	lpfc_sli_release_iocbq(phba, piocb->rspiocbq);
	if (piocb->cmdiocbq)
		lpfc_sli_release_iocbq(phba, piocb->cmdiocbq);

	return;
}

/*
 * This routine is menlo specific, it will always set the DID to 
 * 0xFC0E (menlo DID).
 */
int
lpfc_prep_iocb(struct lpfc_hba *phba,
	       struct lpfc_build_iocb *piocb)
{
	struct ulp_bde64 *bpl = NULL;
	IOCB_t *cmd = NULL, *rsp = NULL;
	int rc = 0;

	if (!piocb->cmdsize || !piocb->rspsize 
		|| (piocb->cmdsize + piocb->rspsize > 80 * 4096)) {
		rc = ERANGE;
		goto prep_iocb_exit;
	}

	if ((piocb->cmdiocbq = lpfc_sli_get_iocbq(phba)) == 0) {
		rc = ENOMEM;
		goto prep_iocb_exit;
	}

	cmd = &piocb->cmdiocbq->iocb;

	if ((piocb->rspiocbq = lpfc_sli_get_iocbq(phba)) == 0) {
		rc = ENOMEM;
		goto prep_iocb_err;
	}

	rsp = &piocb->rspiocbq->iocb;

	piocb->bmp = kmalloc(sizeof (struct lpfc_dmabuf), GFP_KERNEL);
	if (!piocb->bmp) {
		rc = ENOMEM;
		goto prep_iocb_err;
	}
	piocb->bmp->virt = lpfc_mbuf_alloc(phba, 0, &piocb->bmp->phys);
	if (!piocb->bmp->virt) {
		rc = ENOMEM;
		goto prep_iocb_err;
	}

	INIT_LIST_HEAD(&piocb->bmp->list);
	bpl = (struct ulp_bde64*) piocb->bmp->virt;

	piocb->indmp = dfc_cmd_data_alloc(phba, piocb->datain_ptr, bpl, piocb->cmdsize);
	if (!piocb->indmp) {
		rc = ENOMEM;
		goto prep_iocb_err;
	}

	/* flag contains total number of BPLs for xmit */
	bpl += piocb->indmp->flag; 

	piocb->outdmp = dfc_cmd_data_alloc(phba, 0, bpl, piocb->rspsize);
	if (!piocb->outdmp) {
		rc = ENOMEM;
		goto prep_iocb_err;
	}

	cmd->un.genreq64.bdl.ulpIoTag32 = 0;
	cmd->un.genreq64.bdl.addrHigh = putPaddrHigh(piocb->bmp->phys);
	cmd->un.genreq64.bdl.addrLow = putPaddrLow(piocb->bmp->phys);
	cmd->un.genreq64.bdl.bdeFlags = BUFF_TYPE_BLP_64;
	cmd->un.genreq64.bdl.bdeSize =
	    (piocb->outdmp->flag + piocb->indmp->flag) * sizeof (struct ulp_bde64);
	cmd->ulpCommand = CMD_GEN_REQUEST64_CR;
	cmd->un.genreq64.w5.hcsw.Fctl = (SI | LA);
	cmd->un.genreq64.w5.hcsw.Dfctl = 0;
	cmd->un.genreq64.w5.hcsw.Rctl = FC_RCTL_DD_UNSOL_CMD;
	cmd->un.genreq64.w5.hcsw.Type = MENLO_TRANSPORT_TYPE; /* 0xfe */
	cmd->un.ulpWord[4] = MENLO_DID; /* 0x0000FC0E */
	cmd->ulpBdeCount = 1;
	cmd->ulpLe = 1;
	cmd->ulpPU = MENLO_PU;
	cmd->ulpClass = CLASS3;
	cmd->ulpOwner = OWN_CHIP;
	piocb->cmdiocbq->context1 = (uint8_t *) 0;
	piocb->cmdiocbq->context2 = (uint8_t *) 0;
	piocb->cmdiocbq->iocb_flag |= LPFC_IO_LIBDFC;

	cmd->ulpTimeout = 25;

	piocb->cmdiocbq->vport = phba->pport;

prep_iocb_exit:
	return rc;
prep_iocb_err:
	lpfc_iocb_cleanup(phba,piocb);
	return rc;
}

int
lpfc_ioctl_send_menlo_mgmt_cmd(struct lpfc_hba * phba,
			 struct lpfcCmdInput * cip, void *dataout)
{
	IOCB_t *cmd = NULL, *rsp = NULL;
	struct lpfc_sli *psli = &phba->sli;
	int rc = 0;
	uint32_t cmdcode;
	struct lpfc_build_iocb iocb;
	struct lpfc_build_iocb *piocb = NULL;
	uint16_t  ulpCtxt;

	piocb = &iocb;
	if ((phba->link_state == LPFC_LINK_DOWN) || 
	    (phba->pport->port_state == LPFC_LINK_DOWN)) {
		if (!(psli->sli_flag & LPFC_MENLO_MAINT)) {
			rc = EACCES;
			goto send_menlo_mgmt_cmd_exit;
		}
	}
	
	if (!(psli->sli_flag & LPFC_SLI_ACTIVE)) {
		rc = EACCES;
		goto send_menlo_mgmt_cmd_exit;
	}

	if (copy_from_user(&cmdcode, cip->lpfc_arg1, sizeof(cmdcode)))
		return EIO;

	if (cmdcode == MENLO_CMD_FW_DOWNLOAD) {
		piocb->cmdsize = MENLO_CMD_HDR_SIZE;
		piocb->rspsize = 4;
	} else {
		piocb->cmdsize = (uint32_t)(unsigned long)cip->lpfc_arg2;
		piocb->rspsize = (int)cip->lpfc_outsz;
	}

	piocb->datain_ptr = (uint8_t *) cip->lpfc_arg1;
	rc = lpfc_prep_iocb(phba, piocb);
	if (rc) {
		rc = ENOMEM;
		goto send_menlo_mgmt_cmd_exit;
	}
	cmd = &piocb->cmdiocbq->iocb;
issueIocb:
	rsp = &piocb->rspiocbq->iocb;
	rc = lpfc_sli_issue_iocb_wait(phba, LPFC_ELS_RING, piocb->cmdiocbq, piocb->rspiocbq, 30); 
	if (rc == IOCB_TIMEDOUT) {
		bool timedout = true;
		IOCB_TIMEOUT_T args;

		(void) memset(&args, 0, sizeof(IOCB_TIMEOUT_T));

		args.phba     = phba;
		args.cmdiocbq = piocb->cmdiocbq;
		args.rspiocbq = piocb->rspiocbq;
		args.db0      = piocb->bmp;
		args.dbext0   = piocb->outdmp;
		args.dbext0   = piocb->indmp;

		timedout = lpfc_process_iocb_timeout(&args);

		if (timedout)
			return ETIMEDOUT;
		else
			rc = IOCB_SUCCESS;
 	}

	if (rc == IOCB_BUSY) {
		rc = EAGAIN;
		goto send_menlo_mgmt_cmd_free_piocb;
	}

	if (rc != IOCB_SUCCESS) {
		rc = EACCES;
		goto send_menlo_mgmt_cmd_free_piocb;
	}

	if (rsp->ulpStatus) {
		if (rsp->ulpStatus == IOSTAT_LOCAL_REJECT) {
			switch (rsp->un.ulpWord[4] & 0xff) {
			case IOERR_SEQUENCE_TIMEOUT:
				rc = ETIMEDOUT;
				break;
			case IOERR_INVALID_RPI:
				rc = EFAULT;
				break;
			default:
				rc = EACCES;
				break;
			}
			goto send_menlo_mgmt_cmd_free_piocb;
		}
	} else {
		piocb->outdmp->flag = rsp->un.genreq64.bdl.bdeSize;
	}

	/* Copy back response data */
	if (piocb->outdmp->flag > piocb->rspsize) {
		rc = ERANGE;
		goto send_menlo_mgmt_cmd_free_piocb;
	}

	if (cmdcode == MENLO_CMD_FW_DOWNLOAD 
		&& piocb->cmdiocbq->iocb.ulpCommand == CMD_GEN_REQUEST64_CR) {
		ulpCtxt = rsp->ulpContext;	
		lpfc_iocb_cleanup(phba,piocb);
		piocb->cmdsize = (uint32_t)(unsigned long)cip->lpfc_arg2 
				- MENLO_CMD_HDR_SIZE;
		piocb->rspsize = (int)cip->lpfc_outsz;
		piocb->datain_ptr = (uint8_t *) cip->lpfc_arg1
				+ MENLO_CMD_HDR_SIZE;
		rc = lpfc_prep_iocb(phba, piocb);
		if (rc) {
			rc = ENOMEM;
			goto send_menlo_mgmt_cmd_exit;
		}
		cmd = &piocb->cmdiocbq->iocb;
		cmd->ulpContext = ulpCtxt;
		cmd->ulpCommand = CMD_GEN_REQUEST64_CX;
		cmd->un.ulpWord[4] = 0;
		cmd->ulpPU = 1;
		goto issueIocb;
	}

	/* copy back size of response, and response itself */
	memcpy(dataout, &piocb->outdmp->flag, sizeof (int));

	rc = 0;
	rc = dfc_rsp_data_copy_to_buf(phba, dataout, &piocb->outdmp->dma,
				       piocb->outdmp->flag);
	if (rc)
		rc = EIO;

send_menlo_mgmt_cmd_free_piocb:
	lpfc_iocb_cleanup(phba,piocb);
send_menlo_mgmt_cmd_exit:
	return rc;
}


static int
lpfc_ioctl_send_mgmt_cmd(struct lpfc_hba * phba,
			 struct lpfcCmdInput * cip, void *dataout)
{
	struct lpfc_nodelist *pndl = NULL;
	struct ulp_bde64 *bpl = NULL;
	struct lpfc_name findwwn;
	uint32_t finddid, timeout;
	struct lpfc_iocbq *cmdiocbq = NULL, *rspiocbq = NULL;
	struct lpfc_dmabufext *indmp = NULL, *outdmp = NULL;
	IOCB_t *cmd = NULL, *rsp = NULL;
	struct lpfc_dmabuf *bmp = NULL;
	struct lpfc_sli *psli = NULL;
	int i0 = 0, rc = 0, reqbfrcnt, snsbfrcnt;

	psli = &phba->sli;

	if (!(psli->sli_flag & LPFC_SLI_ACTIVE)) {
		rc = EACCES;
		goto common_exit;
	}

	reqbfrcnt = cip->lpfc_arg4;
	snsbfrcnt = cip->lpfc_arg5;

	if (!reqbfrcnt || !snsbfrcnt
		|| (reqbfrcnt + snsbfrcnt > 80 * BUF_SZ_4K)) {
		rc = ERANGE;
		goto common_exit;
	}

	if (phba->pport->port_state != LPFC_VPORT_READY) {
		rc = ENODEV;
		goto common_exit;
	}

	if (cip->lpfc_cmd == LPFC_HBA_SEND_MGMT_CMD) {
		rc = copy_from_user(&findwwn, (void __user *)cip->lpfc_arg3,
						sizeof(struct lpfc_name));
		if (rc) {
			rc = EIO;
			goto common_exit;
		}

		/* Is the node logged in ? */
		pndl = lpfc_findnode_wwpn(phba->pport, &findwwn);

		if (!pndl) {
			rc = ENODEV;
			goto common_exit;
		}

		if (!(lpfc_is_unmapped_node_logged_in(phba->pport,
							pndl->nlp_DID))) {
			rc = ENODEV;
			goto common_exit;
		}

		/* Do additional get to pndl found so that at the end of the
		 * function we can do unconditional lpfc_nlp_put on it.
		 */
		lpfc_nlp_get(pndl);

	} else {
		finddid = (uint32_t)(unsigned long)cip->lpfc_arg3;

		/* Validate LPFC_CT DIDs.  According to LIBDFC, these
		 * are the only DIDs supported.
		 */
		switch (finddid) {
		case NameServer_DID:
		case TimeServer_DID:
		case FDMI_DID:
		case AliasServer_DID:
			break;
		default:
			rc = ENODEV;
			goto common_exit;
		}

		/* Is the node logged in ? */
		if (!(lpfc_is_unmapped_node_logged_in(phba->pport,
						      finddid))) {

			/* The NameServer is managed by base driver */
			if (finddid == NameServer_DID) {
				rc = ENODEV;
				goto common_exit;
			}
			/* Management Server may be managed by the base
			 * driver.  If it is, don't allow login.
			 */
			if ((finddid == FDMI_DID) && phba->pport->cfg_fdmi_on) {
				rc = ENODEV;
				goto common_exit;
			}
			/* For all other nodes, plogi allowed */
			if (phba->pport->fc_flag & FC_FABRIC) {
				pndl = lpfc_findnode_did(phba->pport, finddid);
				if (!pndl) {
					pndl = kmalloc(sizeof
						(struct lpfc_nodelist),
						GFP_KERNEL);
					if (!pndl) {
						rc = ENODEV;
						goto common_exit;
					}
					lpfc_nlp_init(phba->pport, pndl,
							finddid);
					lpfc_nlp_set_state(phba->pport,
						pndl, NLP_STE_PLOGI_ISSUE);
				} else
					lpfc_enable_node(phba->pport,
						pndl, NLP_STE_PLOGI_ISSUE);

				if (lpfc_issue_els_plogi(phba->pport,
							 pndl->nlp_DID, 0)) {
					rc = ENODEV;
					goto common_exit;
				}

				/* Allow the node to complete discovery */
				while (i0++ < 4) {
					if (pndl->nlp_state ==
						NLP_STE_UNMAPPED_NODE)
						break;
					msleep(500);
				}

				if (i0 == 4) {
					rc = ENODEV;
					goto common_exit;
				}
			} else {
				rc = ENODEV;
				goto common_exit;
			}
		} else {
			/* Do additional get to pndl found so at the end of
			 * the function we can do unconditional lpfc_nlp_put.
			 */
			pndl = lpfc_findnode_did(phba->pport, finddid);
			lpfc_nlp_get(pndl);
		}
	}

	if (!pndl || !NLP_CHK_NODE_ACT(pndl)) {
		rc = ENODEV;
		goto common_exit;
	}

	if (pndl->nlp_flag & NLP_ELS_SND_MASK) {
		rc = ENODEV;
		goto common_exit;
	}

	cmdiocbq = lpfc_sli_get_iocbq(phba);
	if (!cmdiocbq) {
		rc = ENOMEM;
		goto common_exit;
	}
	cmd = &cmdiocbq->iocb;

	rspiocbq = lpfc_sli_get_iocbq(phba);
	if (!rspiocbq) {
		rc = ENOMEM;
		goto common_exit;
	}

	rsp = &rspiocbq->iocb;

	bmp = kmalloc(sizeof (struct lpfc_dmabuf), GFP_KERNEL);
	if (!bmp) {
		rc = ENOMEM;
		goto common_exit;
	}
	
	bmp->virt = lpfc_mbuf_alloc(phba, 0, &bmp->phys);
	if (!bmp->virt) {
		rc = ENOMEM;
		goto common_exit;
	}

	INIT_LIST_HEAD(&bmp->list);
	bpl = (struct ulp_bde64 *) bmp->virt;
	indmp = dfc_cmd_data_alloc(phba, cip->lpfc_arg1, bpl, reqbfrcnt);
	if (!indmp) {
		rc = ENOMEM;
		goto common_exit;
	}

	/*
 	 * The value of flag is the total number of BDEs allocated for 
 	 * the command sequence.  Set the bdeFlags to 64I so that the
 	 * response BDE's get properly configured for GEN_REQUEST.
 	 */
	bpl += indmp->flag;
	bpl->tus.f.bdeFlags = BUFF_TYPE_BDE_64I;
	outdmp = dfc_cmd_data_alloc(phba, NULL, bpl, snsbfrcnt);
	if (!outdmp) {
		rc = ENOMEM;
		goto common_exit;
	}

	cmd->un.genreq64.bdl.ulpIoTag32 = 0;
	cmd->un.genreq64.bdl.addrHigh = putPaddrHigh(bmp->phys);
	cmd->un.genreq64.bdl.addrLow = putPaddrLow(bmp->phys);
	cmd->un.genreq64.bdl.bdeFlags = BUFF_TYPE_BLP_64;
	cmd->un.genreq64.bdl.bdeSize =
	    (outdmp->flag + indmp->flag) * sizeof (struct ulp_bde64);
	cmd->ulpCommand = CMD_GEN_REQUEST64_CR;
	cmd->un.genreq64.w5.hcsw.Fctl = (SI | LA);
	cmd->un.genreq64.w5.hcsw.Dfctl = 0;
	cmd->un.genreq64.w5.hcsw.Rctl = FC_RCTL_DD_UNSOL_CTL;
	cmd->un.genreq64.w5.hcsw.Type = FC_TYPE_CT;
	cmd->ulpBdeCount = 1;
	cmd->ulpLe = 1;
	cmd->ulpClass = CLASS3;
	cmd->ulpContext = pndl->nlp_rpi;
	cmd->ulpOwner = OWN_CHIP;
	cmdiocbq->vport = phba->pport;
	cmdiocbq->context1 = pndl;
	cmdiocbq->context2 = NULL;
	cmdiocbq->context3 = bmp;
	cmdiocbq->iocb_flag |= LPFC_IO_LIBDFC;

	if (phba->sli.sli_flag & LPFC_BLOCK_MGMT_IO)
		return EAGAIN;

	timeout = cip->lpfc_flag;
	if (cip->lpfc_flag == 0 )
		timeout = phba->fc_ratov * 2 ;

	cmd->ulpTimeout = timeout;

	rc = lpfc_sli_issue_iocb_wait(phba, LPFC_ELS_RING, cmdiocbq, rspiocbq,
					timeout + LPFC_DRVR_TIMEOUT);
	if (rc == IOCB_TIMEDOUT) {
		bool timedout = true;
		IOCB_TIMEOUT_T args;

		(void) memset(&args, 0, sizeof(IOCB_TIMEOUT_T));

		args.phba     = phba;
		args.cmdiocbq = cmdiocbq;
		args.db0      = bmp;
		args.dbext0   = outdmp;
		args.dbext0   = indmp;
		timedout = lpfc_process_iocb_timeout(&args);
		if (timedout)
			return ETIMEDOUT;
		else
			rc = IOCB_SUCCESS;
 	}

	if (rc == IOCB_BUSY) {
		rc = EAGAIN;
		goto common_exit;
	}

	if (rc != IOCB_SUCCESS) {
		rc = EACCES;
		goto common_exit;
	}

	if (rsp->ulpStatus) {
		if (rsp->ulpStatus == IOSTAT_LOCAL_REJECT) {
			switch (rsp->un.ulpWord[4] & 0xff) {
			case IOERR_SEQUENCE_TIMEOUT:
				rc = ETIMEDOUT;
				break;
			case IOERR_INVALID_RPI:
				rc = EFAULT;
				break;
			default:
				rc = EACCES;
				break;
			}
			goto common_exit;
		}
	} else
		outdmp->flag = rsp->un.genreq64.bdl.bdeSize;

	/* Copy back response data */
	if (outdmp->flag > snsbfrcnt) {
		rc = ERANGE;
		lpfc_printf_log(phba, KERN_ERR, LOG_LIBDFC,
				"1209 C_CT Request error Data: x%x x%x\n",
				outdmp->flag, BUF_SZ_4K);
		goto common_exit;
	}

	/* Copy back size of response and the response data. */
	copy_to_user(cip->lpfc_dataout, &outdmp->flag, sizeof(int));
	cip->lpfc_outsz = 0;
	rc = dfc_rsp_data_copy (phba, cip->lpfc_arg2, outdmp, outdmp->flag);
	if (rc)
		rc = EIO;

common_exit:

	if (outdmp != NULL)
		dfc_cmd_data_free(phba, outdmp);

	if (indmp != NULL)
		dfc_cmd_data_free(phba, indmp);

	if (bmp != NULL)
		(void) lpfc_mbuf_reclaim(phba, bmp);

	if (rspiocbq != NULL)
		lpfc_sli_release_iocbq(phba, rspiocbq);

	if (cmdiocbq != NULL)
		lpfc_sli_release_iocbq(phba, cmdiocbq);

	if (pndl != NULL)
		lpfc_nlp_put(pndl);

	return rc;
}

static inline struct lpfcdfc_event *
lpfcdfc_event_new(
	uint32_t ev_mask,
	int      ev_reg_id,
	uint32_t ev_req_id)
{
	struct lpfcdfc_event *evt = NULL;
	int size = 0;

	size = sizeof(struct lpfcdfc_event);
	evt  = kzalloc(size,GFP_KERNEL);

	if (evt != NULL) {
		INIT_LIST_HEAD(&evt->node);
		evt->type_mask = ev_mask;
		evt->req_id = ev_req_id;
		evt->reg_id = ev_reg_id;
		evt->wait_time_stamp = jiffies;
	}

	return evt;
}

int
lpfc_ioctl_mbox(struct lpfc_hba *phba, struct lpfcCmdInput *cip, void *dataout)
{
	MAILBOX_t *pmbox;
	MAILBOX_t *pmb;
	struct lpfc_sli *psli = &phba->sli;
	uint32_t size;
	dma_addr_t lptr;
	LPFC_MBOXQ_t *pmboxq = NULL;
	struct lpfc_dmabuf *pbfrnfo = NULL;
	struct lpfc_dmabuf *pxmitbuf = NULL;
	int count = 0;
	int rc = 0;
	int mbxstatus = 0;
	struct lpfc_vport *vport = phba->pport;
  	uint32_t incnt = (uint32_t)(unsigned long) cip->lpfc_arg2;
	int wait_4_menlo_maint = 0;
	unsigned long loop_timeout;
	struct ioctl_mailbox_ext_data *ext_data = 0;

	if (phba->sli.sli_flag & LPFC_BLOCK_MGMT_IO)
		return EAGAIN;

	/* Allocate mbox structure */
	pmbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (pmbox == NULL)
		return ENOMEM;

	pxmitbuf = kmalloc(sizeof (struct lpfc_dmabuf), GFP_KERNEL);
	if (!pxmitbuf) {
		mempool_free(pmbox, phba->mbox_mem_pool);
		return ENOMEM;
	}

	pxmitbuf->virt = lpfc_mbuf_alloc(phba, 0, &pxmitbuf->phys);
	if (!pxmitbuf->virt) {
		kfree(pxmitbuf);
		mempool_free(pmbox, phba->mbox_mem_pool);
		return ENOMEM;
        }

	/* Allocate mbox extension */
	if (cip->lpfc_arg3) {
		ext_data = kmalloc(sizeof(struct ioctl_mailbox_ext_data), GFP_KERNEL);
		if (!ext_data) {
			(void) lpfc_mbuf_reclaim(phba, pxmitbuf);
			mempool_free(pmbox, phba->mbox_mem_pool);
			return ENOMEM;
		}
		memset (ext_data, 0, sizeof(ext_data));
		if ((copy_from_user(ext_data, cip->lpfc_arg3,
				sizeof(struct ioctl_mailbox_ext_data)))) {
			kfree(ext_data);
			(void) lpfc_mbuf_reclaim(phba, pxmitbuf);
			mempool_free(pmbox, phba->mbox_mem_pool);
			return EIO;
		}
	}
	INIT_LIST_HEAD(&pxmitbuf->list);

	/* Allocate mboxq structure */
	pmboxq = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmboxq) {
		(void) lpfc_mbuf_reclaim(phba, pxmitbuf);
                mempool_free(pmbox, phba->mbox_mem_pool);
		if (ext_data)
			kfree(ext_data);
		return ENOMEM;
	}

	/* Allocate mbuf structure */
	if (((pbfrnfo = kmalloc(sizeof (struct lpfc_dmabuf), GFP_KERNEL)) == 0) ||
	    ((pbfrnfo->virt = lpfc_mbuf_alloc(phba,
					      0, &(pbfrnfo->phys))) == 0)) {
		if (pbfrnfo)
			kfree(pbfrnfo);
		(void) lpfc_mbuf_reclaim(phba, pxmitbuf);
		mempool_free(pmbox, phba->mbox_mem_pool);
		mempool_free(pmboxq, phba->mbox_mem_pool);
		if (ext_data)
			kfree(ext_data);
		return ENOMEM;
	}
	INIT_LIST_HEAD(&pbfrnfo->list);

	if (copy_from_user((uint8_t *) pmbox, (uint8_t *) cip->lpfc_arg1,
			min_value(incnt, MAILBOX_CMD_SIZE))) {
		mempool_free(pmbox, phba->mbox_mem_pool);
		mempool_free(pmboxq, phba->mbox_mem_pool);
		(void) lpfc_mbuf_reclaim(phba, pbfrnfo);
		(void) lpfc_mbuf_reclaim(phba, pxmitbuf);
		if (ext_data)
			kfree(ext_data);
		return EIO;
	}

        while (phba->sli.sli_flag & LPFC_SLI_MBOX_ACTIVE) {
		msleep(10);
                if (count++ == 200)
                        break;
        }

        if (count >= 200) {
                pmbox->mbxStatus = MBXERR_ERROR;
                rc = EAGAIN;
                goto mbout_err;
        } else {
#ifdef _LP64
		if ((pmbox->mbxCommand == MBX_READ_SPARM) ||
		    (pmbox->mbxCommand == MBX_READ_RPI) ||
		    (pmbox->mbxCommand == MBX_REG_LOGIN) ||
		    (pmbox->mbxCommand == MBX_READ_LA)) {
			/* Must use 64 bit versions of these mbox cmds */
			pmbox->mbxStatus = MBXERR_ERROR;
			rc = ENODEV;
			goto mbout_err;
		}
#endif
	}

	if (lpfc_is_sli4(phba) && pmbox->mbxCommand == MBX_DUMP_MEMORY) {
		/* LIBDFC should use LPFC_HBA_ISSUE_DMP_MBX */
		rc = ENOEXEC;
		goto mbout_freemem;
	}

/*          phba->di_fc_flag |= DFC_MBOX_ACTIVE; */
	lptr = 0;
	size = 0;
	switch (pmbox->mbxCommand) {
	/* Offline only */
	case MBX_INIT_LINK:
	case MBX_DOWN_LINK:
	case MBX_CONFIG_LINK:
	case MBX_CONFIG_RING:
	case MBX_RESET_RING:
	case MBX_UNREG_LOGIN:
	case MBX_CLEAR_LA:
	case MBX_DUMP_CONTEXT:
	case MBX_RUN_DIAGS:
	case MBX_RESTART:
	case MBX_SET_MASK:
		if (!(vport->fc_flag & FC_OFFLINE_MODE)) {
			pmbox->mbxStatus = MBXERR_ERROR;
			/*  phba->di_fc_flag &= ~DFC_MBOX_ACTIVE; */
			goto mbout_err;
		}
		break;

	/* Online / Offline */
	case MBX_LOAD_SM:
	case MBX_READ_NV:
	case MBX_READ_CONFIG:
	case MBX_READ_RCONFIG:
	case MBX_READ_STATUS:
	case MBX_READ_XRI:
	case MBX_READ_REV:
	case MBX_READ_LNK_STAT:
	case MBX_DUMP_MEMORY:
	case MBX_DOWN_LOAD:
	case MBX_UPDATE_CFG:
	case MBX_LOAD_AREA:
	case MBX_LOAD_EXP_ROM:
	case MBX_BEACON:
	case MBX_DEL_LD_ENTRY:
	case MBX_SET_DEBUG:
	case MBX_WRITE_EVENT_LOG:
	case MBX_READ_EVENT_LOG_STATUS:
		break;

	case MBX_SET_VARIABLE:
		if ((pmbox->un.varWords[0] == SETVAR_MLOMNT)
			&& (pmbox->un.varWords[1] == 1)) {
			wait_4_menlo_maint = 1;
		}
		break;

	case MBX_WRITE_VPARMS:
	case MBX_WRITE_WWN:
	case MBX_WRITE_NV:
		break;

	/* Online / Offline - with DMA */
        case MBX_READ_EVENT_LOG:
		lptr = getPaddr(pmbox->un.varRdEventLog.rcv_bde64.addrHigh,
				pmbox->un.varRdEventLog.rcv_bde64.addrLow);
		size = (int)pmbox->un.varRdEventLog.rcv_bde64.tus.f.bdeSize;

		if (size > LPFC_BPL_SIZE) {
			/* user bde too big */
                        pmbox->mbxStatus = MBX_STATUS_INTERNAL_DRIVER_ERROR;
                        goto mbout_err;
                }
		
		if (lptr) {
			pmbox->un.varRdEventLog.rcv_bde64.addrHigh =
				 putPaddrHigh(pbfrnfo->phys);
			pmbox->un.varRdEventLog.rcv_bde64.addrLow =
				putPaddrLow(pbfrnfo->phys);
		}
		break;

	case MBX_READ_SPARM64:
		lptr = getPaddr(pmbox->un.varRdSparm.un.sp64.addrHigh,
				pmbox->un.varRdSparm.un.sp64.addrLow);
		size = (int)pmbox->un.varRdSparm.un.sp64.tus.f.bdeSize;

		if (size > LPFC_BPL_SIZE) {
			/* user bde too big */
			pmbox->mbxStatus = MBX_STATUS_INTERNAL_DRIVER_ERROR;
			goto mbout_err;
		}

		if (lptr) {
			pmbox->un.varRdSparm.un.sp64.addrHigh =
			    putPaddrHigh(pbfrnfo->phys);
			pmbox->un.varRdSparm.un.sp64.addrLow =
			    putPaddrLow(pbfrnfo->phys);
		}
		break;

	case MBX_READ_RPI64:
		/* This is only allowed when online is SLI2 mode */
		lptr = getPaddr(pmbox->un.varRdRPI.un.sp64.addrHigh,
				pmbox->un.varRdRPI.un.sp64.addrLow);
		size = (int)pmbox->un.varRdRPI.un.sp64.tus.f.bdeSize;

		if (size > LPFC_BPL_SIZE) {
			/* user bde too big */
			pmbox->mbxStatus = MBX_STATUS_INTERNAL_DRIVER_ERROR;
			goto mbout_err;
		}

		if (lptr) {
			pmbox->un.varRdRPI.un.sp64.addrHigh =
			    putPaddrHigh(pbfrnfo->phys);
			pmbox->un.varRdRPI.un.sp64.addrLow =
			    putPaddrLow(pbfrnfo->phys);
		}
		break;

        case MBX_RUN_BIU_DIAG64:
		lptr = getPaddr(pmbox->un.varBIUdiag.un.s2.xmit_bde64.addrHigh,
				pmbox->un.varBIUdiag.un.s2.xmit_bde64.addrLow);
		if (lptr) {
			rc = copy_from_user((uint8_t *)pxmitbuf->virt,
					(uint8_t *)(unsigned long)lptr,
					pmbox->un.varBIUdiag.un.s2.xmit_bde64.tus.f.bdeSize);
			if (rc) {
				rc = EIO;
				goto mbout_err;
			}

			pmbox->un.varBIUdiag.un.s2.xmit_bde64.addrHigh =
				putPaddrHigh(pxmitbuf->phys);
			pmbox->un.varBIUdiag.un.s2.xmit_bde64.addrLow =
				putPaddrLow(pxmitbuf->phys);
		}

		lptr = getPaddr(pmbox->un.varBIUdiag.un.s2.rcv_bde64.addrHigh,
				pmbox->un.varBIUdiag.un.s2.rcv_bde64.addrLow);
		size = pmbox->un.varBIUdiag.un.s2.rcv_bde64.tus.f.bdeSize;

		if (size > LPFC_BPL_SIZE) {
			/* user bde too big */
			pmbox->mbxStatus = MBX_STATUS_INTERNAL_DRIVER_ERROR;
			goto mbout_err;
		}

		if (lptr) {
			pmbox->un.varBIUdiag.un.s2.rcv_bde64.addrHigh =
				putPaddrHigh(pbfrnfo->phys);
			pmbox->un.varBIUdiag.un.s2.rcv_bde64.addrLow =
				putPaddrLow(pbfrnfo->phys);
		}
		break;

	case MBX_READ_LA:
	case MBX_READ_LA64:
	case MBX_REG_LOGIN:
	case MBX_REG_LOGIN64:
	case MBX_CONFIG_PORT:
	case MBX_RUN_BIU_DIAG:
		/* Do not allow SLI-2 commands */
		pmbox->mbxStatus = MBXERR_ERROR;
		/*  phba->di_fc_flag &= ~DFC_MBOX_ACTIVE; */
		goto mbout_err;
	default:
		/*
		 * Offline only.  Let firmware return error for unsupported
		 * commands
		 */
		if (!(vport->fc_flag & FC_OFFLINE_MODE)) {
			pmbox->mbxStatus = MBXERR_ERROR;
                        /*  phba->di_fc_flag &= ~DFC_MBOX_ACTIVE; */
			goto mbout_err;
		}
		break;
	} /* switch pmbox->mbxCommand */

	memset((void *)pmboxq, 0, sizeof (LPFC_MBOXQ_t));
	pmb = &pmboxq->u.mb;
	pmb->mbxCommand = pmbox->mbxCommand;
	pmb->mbxOwner = pmbox->mbxOwner;
	pmb->un = pmbox->un;
	pmb->us = pmbox->us;
	pmboxq->context1 = (uint8_t *) 0;
	pmboxq->vport = vport;

	if (ext_data) {
		pmboxq->context2 = &ext_data->mbox_extension_data[0];
		pmboxq->in_ext_byte_len = ext_data->in_ext_byte_len;
		pmboxq->out_ext_byte_len = ext_data->out_ext_byte_len;
		pmboxq->mbox_offset_word = ext_data->mbox_offset_word;
	}

	if (phba->sli.sli_flag & LPFC_BLOCK_MGMT_IO) {
		rc = EAGAIN;
		goto mbout_err;
	}
	if ((vport->fc_flag & FC_OFFLINE_MODE) ||
	    (!(psli->sli_flag & LPFC_SLI_ACTIVE))){
		mbxstatus = lpfc_sli_issue_mbox(phba, pmboxq, MBX_POLL);
	} else {
		mbxstatus = lpfc_sli_issue_mbox_wait(phba, pmboxq,
				lpfc_mbox_tmo_val(phba, pmbox->mbxCommand));
	}

        /*  phba->di_fc_flag &= ~DFC_MBOX_ACTIVE; */
	if (mbxstatus == MBX_TIMEOUT) {
		rc = EBUSY;
		goto mbout;
	} else if (mbxstatus != MBX_SUCCESS) {
		rc = ENODEV;
		goto mbout;
	} else if (wait_4_menlo_maint) {
		loop_timeout = jiffies + (30 * HZ);
		phba->wait_4_mlo_maint_flg = 1;

		/* Menlo Maintenance Flag to clear. */
		set_current_state(TASK_INTERRUPTIBLE);
		while (time_before (jiffies, loop_timeout)) {
			schedule_timeout(1);
			if (phba->wait_4_mlo_maint_flg == 0)
				break;
		}

		set_current_state(TASK_RUNNING);
		if (phba->wait_4_mlo_maint_flg != 0) {
			phba->wait_4_mlo_maint_flg = 0;
			rc =  ETIME;
			goto mbout;
		}
	}
	
	/* Copy the extended data to user space memory */
	if (ext_data && ext_data->out_ext_byte_len) {
		rc = copy_to_user(cip->lpfc_arg3, ext_data,
				  sizeof(struct ioctl_mailbox_ext_data));
		if (rc) {
			rc = EIO;
			goto mbout_err;
		}
	}

	if (lptr) {
		rc = copy_to_user((void __user *) lptr, pbfrnfo->virt,
				  (ulong) size);
		if (rc)
			rc = EIO;
	}

 mbout:
	cip->lpfc_outsz = min_value(cip->lpfc_outsz ,MAILBOX_CMD_SIZE);
	memcpy(dataout, (char *)pmb, cip->lpfc_outsz);
	goto mbout_freemem;

 mbout_err:
	/* Jump here only if there is an error and copy the status */
	memcpy(dataout, (char *)pmbox, MAILBOX_CMD_SIZE);

 mbout_freemem:
	/* Free allocated mbox extension */
	if (ext_data)
		kfree(ext_data);

	/* Free allocated mbox memory */
	if (pmbox)
		mempool_free(pmbox, phba->mbox_mem_pool);

	if (pxmitbuf)
		(void) lpfc_mbuf_reclaim(phba, pxmitbuf);

	/* Free allocated mboxq memory */
	if (pmboxq) {
		if (mbxstatus == MBX_TIMEOUT) {
			/*
			 * The SLI layer releases the mboxq if mbox command
			 * completed after timeout.
			 */
			pmboxq->mbox_cmpl = 0;
		} else
			mempool_free(pmboxq, phba->mbox_mem_pool);
	}

	/* Free allocated mbuf memory */
	if (pbfrnfo)
		(void) lpfc_mbuf_reclaim(phba, pbfrnfo);

	return rc;
}

int
lpfc_ioctl_linkinfo(struct lpfc_hba *phba, struct lpfcCmdInput *cip, void *dataout)
{
	int search_vport = 0;
	struct lpfc_link_info *linkinfo;
	struct lpfc_vport *vport = phba->pport;

	linkinfo = (struct lpfc_link_info *) dataout;
	linkinfo->a_linkEventTag = phba->fc_eventTag;
	linkinfo->a_linkUp = phba->fc_stat.LinkUp;
	linkinfo->a_linkDown = phba->fc_stat.LinkDown;
	linkinfo->a_linkMulti = phba->fc_stat.LinkMultiEvent;
	linkinfo->a_DID = vport->fc_myDID;
	if (phba->sli.sli_flag & LPFC_MENLO_MAINT)
		linkinfo->a_topology = LPFC_LNK_MENLO_MAINT;
	else if (phba->fc_topology == TOPOLOGY_LOOP) {
		if (vport->fc_flag & FC_PUBLIC_LOOP) {
			linkinfo->a_topology = LNK_PUBLIC_LOOP;
			memcpy((uint8_t *) linkinfo->a_alpaMap,
			       (uint8_t *) phba->alpa_map,
			       sizeof(linkinfo->a_alpaMap));
			linkinfo->a_alpaCnt = phba->alpa_map[0];
		} else {
			linkinfo->a_topology = LNK_LOOP;
			memcpy((uint8_t *) linkinfo->a_alpaMap,
			       (uint8_t *) phba->alpa_map,
			       sizeof(linkinfo->a_alpaMap));
			linkinfo->a_alpaCnt = phba->alpa_map[0];
		}
	} else {
		memset((uint8_t *) linkinfo->a_alpaMap, 0, 128);
		linkinfo->a_alpaCnt = 0;
		if (vport->fc_flag & FC_FABRIC) {
			linkinfo->a_topology = LNK_FABRIC;
		} else {
			linkinfo->a_topology = LNK_PT2PT;
		}
	}

	linkinfo->a_linkState = 0;
	switch (phba->link_state) {
	case LPFC_INIT_START:
	case LPFC_LINK_DOWN:
		if (phba->hba_flag & LINK_DISABLED)
			linkinfo->a_linkState = LNK_DOWN_PERSIST;
		else
			linkinfo->a_linkState = LNK_DOWN;
		memset((uint8_t *) linkinfo->a_alpaMap, 0, 128);
		linkinfo->a_alpaCnt = 0;
		break;
	case LPFC_LINK_UP:
		linkinfo->a_linkState = LNK_UP;
		break;
	case LPFC_CLEAR_LA:
		linkinfo->a_linkState = LNK_DISCOVERY;
		break;
	case LPFC_HBA_READY:
		linkinfo->a_linkState = LNK_READY;
		break;
	default:
		search_vport = 1;
		break;
	}

	if (search_vport) {
		switch (vport->port_state) {
		case LPFC_LOCAL_CFG_LINK:
			linkinfo->a_linkState = LNK_UP;
			break;
		case LPFC_FLOGI:
			linkinfo->a_linkState = LNK_FLOGI;
			break;
		case LPFC_DISC_AUTH:
		case LPFC_FABRIC_CFG_LINK:
		case LPFC_NS_REG:
		case LPFC_NS_QRY:
			linkinfo->a_linkState = LNK_DISCOVERY;
			break;
		default:
			break;
		}
	}

	linkinfo->a_alpa = (uint8_t) (vport->fc_myDID & 0xff);
	memcpy(linkinfo->a_wwpName, &vport->fc_portname,
	       sizeof(linkinfo->a_wwpName));
	memcpy(linkinfo->a_wwnName, &vport->fc_nodename,
	       sizeof(linkinfo->a_wwnName));
	return 0;
}

int
lpfc_ioctl_ioinfo(struct lpfc_hba * phba, struct lpfcCmdInput * cip, void *dataout)
{
	struct lpfc_io_info *ioinfo;
	struct lpfc_sli *psli;
	int rc = 0;

	psli = &phba->sli;
	ioinfo = (struct lpfc_io_info *) dataout;
	memset((void *)ioinfo, 0, sizeof (struct lpfc_io_info));

	ioinfo->a_mbxCmd = psli->slistat.mbox_cmd;
	ioinfo->a_mboxCmpl = psli->slistat.mbox_event;
	ioinfo->a_mboxErr = psli->slistat.mbox_stat_err;
	ioinfo->a_iocbCmd = psli->ring[cip->lpfc_ring].stats.iocb_cmd;
	ioinfo->a_iocbRsp = psli->ring[cip->lpfc_ring].stats.iocb_rsp;
	ioinfo->a_adapterIntr = (psli->lnk_stat_offsets.link_events +
				 psli->ring[cip->lpfc_ring].stats.iocb_rsp +
				 psli->slistat.mbox_event);
	ioinfo->a_fcpCmd = phba->fc_stat.fcpCmd;
	ioinfo->a_fcpCmpl = phba->fc_stat.fcpCmpl;
	ioinfo->a_fcpErr = phba->fc_stat.fcpRspErr +
	    phba->fc_stat.fcpRemoteStop + phba->fc_stat.fcpPortRjt +
	    phba->fc_stat.fcpPortBusy + phba->fc_stat.fcpError +
	    phba->fc_stat.fcpLocalErr;
	ioinfo->a_bcastRcv = phba->fc_stat.frameRcvBcast;
	ioinfo->a_RSCNRcv = phba->fc_stat.elsRcvRSCN;

	return rc;
}

/*
 * This function works on the vport bound to the physical port.  There is 
 * no need to check the vport for unloading flags since the driver instance
 * to which this vport is bound cannot unload.
 */
int
lpfc_ioctl_nodeinfo(struct lpfc_hba *phba, struct lpfcCmdInput *cip, void *dataout, int size)
{
	struct lpfc_node_info *np;
	uint32_t cnt;
	uint32_t total_mem = size;
	struct lpfc_nodelist *ndlp;
	struct lpfc_vport *vport = phba->pport;
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);

	np = (struct lpfc_node_info *) dataout;
	cnt = 0;
	
	/* 
	 * Get the complete driver node list and build up the node data.
	 * Make sure the ndlp is an active node first and synchronize
	 * this routine's access to the node list with discovery.
	 */
	total_mem -= sizeof (struct lpfc_nodelist);
	spin_lock_irq(shost->host_lock);
	list_for_each_entry(ndlp, &vport->fc_nodes, nlp_listp) {
		if (total_mem <= 0)
			break;
		if (!NLP_CHK_NODE_ACT(ndlp))
			continue;

		memset(np, 0, sizeof(struct lpfc_nodelist));
		if (ndlp->nlp_flag & NLP_ADISC_SND) {
			np->a_flag |= NODE_ADDR_AUTH;
			np->a_state = NODE_LIMBO;
		}
		if (ndlp->nlp_flag & NLP_PLOGI_SND)
			np->a_state = NODE_PLOGI;
		if (ndlp->nlp_flag & NLP_PRLI_SND)
			np->a_state = NODE_PRLI;
		if (ndlp->nlp_flag & NLP_LOGO_SND)
			np->a_state = NODE_LOGOUT;
		if (ndlp->nlp_flag & NLP_ELS_SND_MASK)
			np->a_flag |= NODE_REQ_SND;
		if (ndlp->nlp_flag & NLP_DEFER_RM)
			np->a_state |= NODE_LIMBO;
		if (ndlp->nlp_flag & NLP_DELAY_TMO)
			np->a_state |= NODE_LIMBO;
		if (ndlp->nlp_flag & NLP_NPR_2B_DISC)
			np->a_state |= NODE_LIMBO;
		if (ndlp->nlp_flag & NLP_LOGO_ACC)
			np->a_state |= NODE_LOGOUT;
		if (ndlp->nlp_flag & NLP_TGT_NO_SCSIID)
			np->a_state |= NODE_LIMBO;
		if (ndlp->nlp_flag & NLP_NPR_ADISC)
			np->a_state |= NODE_LIMBO;
		if (ndlp->nlp_flag & NLP_RM_DFLT_RPI)
			np->a_state |= NODE_UNUSED;
		if (ndlp->nlp_flag & NLP_NODEV_REMOVE)
			np->a_state |= NODE_UNUSED;
		if (ndlp->nlp_flag & NLP_TARGET_REMOVE)
			np->a_state |= NODE_UNUSED;
		if (ndlp->nlp_type & NLP_FABRIC) {
			np->a_flag |= NODE_FABRIC;
			np->a_state |= NODE_ALLOC;
		}
		if (ndlp->nlp_type & NLP_FCP_INITIATOR) {
			np->a_flag |= NODE_AUTOMAP;
			np->a_state |= NODE_ALLOC;
		}
		if (ndlp->nlp_type & NLP_FCP_TARGET) {
			np->a_flag |= NODE_FCP_TARGET;
			np->a_flag |= NODE_SEED_WWPN;
			np->a_state |= NODE_ALLOC;
		}
	
		np->a_did = ndlp->nlp_DID;
		np->a_targetid = ndlp->nlp_sid;
		memcpy(np->a_wwpn, &ndlp->nlp_portname, sizeof(np->a_wwpn));
		memcpy(np->a_wwnn, &ndlp->nlp_nodename, sizeof(np->a_wwnn));
		total_mem -= sizeof (struct lpfc_nodelist);
		np++;
		cnt++;
	}
	spin_unlock_irq(shost->host_lock);
	cip->lpfc_outsz = (uint32_t) (cnt * sizeof (struct lpfc_node_info));
	return 0;
}

int
lpfc_ioctl_getcfg(struct lpfc_hba *phba, struct lpfcCmdInput *cip, void *dataout)
{
	struct CfgParam *cp;
	struct CfgEntry *ce;
        struct VPCfgEntry *vce;
	struct CfgParam *export = dataout;
	int i, rc = 0, cnt = 0;

	/* First uint32_t word will be count */
	cp = (struct CfgParam *) dataout;

	/* 
	 * The legacy get must operate on the phba and its vport to correctly
	 * propagate the full range of managable parameters.  Build up the list
	 * of configuration parameters by parsing the vport attributes first
	 * and the physical hba second.
	 */
	for (i = 0; i < phba->pport->CfgCnt; i++) {
		/* First validate parameter */
		if (!(vce = phba->pport->CfgTbl[i]))
			return EINVAL;
		if (!(cp = vce->entry)) 
			return EINVAL;
		if (vce->getcfg) 
			cp->a_current = vce->getcfg(phba->pport);
		if ((cnt+1 * sizeof(struct CfgParam)) > cip->lpfc_outsz) {
			rc = E2BIG;
			break;
		}
		/* Next check if parameter is valid for adapter */
		if (0 == (strncmp(cp->a_string, "pci-max-read",
				  sizeof("pci-max-read")))) {
			if (phba->hba_flag & HBA_DMA_RESTRICT)
				goto keep1;
		}
		if (cp->a_flag & CFG_COMMON)
			goto keep1;
		if (lpfc_is_fc(phba) && (cp->a_flag & CFG_FC))
			goto keep1;
		if (lpfc_is_fcoe(phba) && (cp->a_flag & CFG_FCOE))
			goto keep1;
		if (lpfc_is_lp(phba) && (cp->a_flag & CFG_LP))
			goto keep1;
		if (lpfc_is_sli4(phba) && (cp->a_flag & CFG_SLI4))
			goto keep1;
		else
			continue;
 keep1:
		memcpy((uint8_t*)export, (uint8_t*)cp, sizeof(struct CfgParam));
		export++;
		cnt++;
	}

	for (i = 0; i < phba->CfgCnt; i++) {
		/* First validate parameter */
		if (!(ce = phba->CfgTbl[i]))
			return EINVAL;
		if (!(cp = ce->entry))
			return EINVAL;
		if (ce->getcfg)
			cp->a_current = ce->getcfg(phba);
		if ((cnt+1 * sizeof(struct CfgParam)) > cip->lpfc_outsz) {
			rc = E2BIG;
			break;
		}
		/* Next check if parameter is valid for adapter */
		if (0 == (strncmp(cp->a_string, "pci-max-read",
			  sizeof("pci-max-read")))) {
			if (phba->hba_flag & HBA_DMA_RESTRICT)
				goto keep2;
		}
		if (cp->a_flag & CFG_COMMON)
			goto keep2;
		if (lpfc_is_fc(phba) && (cp->a_flag & CFG_FC))
			goto keep2;
		if (lpfc_is_fcoe(phba) && (cp->a_flag & CFG_FCOE))
			goto keep2;
		if (lpfc_is_lp(phba) && (cp->a_flag & CFG_LP))
			goto keep2;
		if (lpfc_is_sli4(phba) && (cp->a_flag & CFG_SLI4))
			goto keep2;
		else
			continue;
 keep2:
		memcpy ((uint8_t*)export, (uint8_t*)cp, sizeof(struct CfgParam));
		export++;
		cnt++;
	}

	cip->lpfc_outsz = (uint32_t)(cnt * sizeof(struct CfgParam));
	return rc;
}

int
lpfc_ioctl_setcfg(struct lpfc_hba *phba, struct lpfcCmdInput *cip)
{
	struct CfgParam *cp=0;
	struct CfgEntry *ce=0;
	struct VPCfgEntry *vce=0;
	uint32_t offset, val;
	int i, j;

	offset = (ulong) cip->lpfc_arg1;
	val = (ulong) cip->lpfc_arg2;

	/* 
	 * The set operates on the phba and its vport.  Build up the list of
	 * configuration parameters by parsing the vport attributes first and
	 * the phba attributes second.
	 */
	j = offset;
	for (i = 0; i < phba->pport->CfgCnt; i++) {
		/* First do sanity check */
		if (!(vce = phba->pport->CfgTbl[i]))
			return EINVAL;
		if (!(cp = vce->entry))
			return EINVAL;
		/* Next check if parameter is valid for adapter */
		if (0 == (strncmp(cp->a_string, "pci-max-read",
			  sizeof("pci-max-read")))) {
			if (phba->hba_flag & HBA_DMA_RESTRICT)
				goto is_valid1;
		}
		if (cp->a_flag & CFG_COMMON)
			goto is_valid1;
		if (lpfc_is_fc(phba) && (cp->a_flag & CFG_FC))
			goto is_valid1;
		if (lpfc_is_fcoe(phba) && (cp->a_flag & CFG_FCOE))
			goto is_valid1;
		if (lpfc_is_lp(phba) && (cp->a_flag & CFG_LP))
			goto is_valid1;
		if (lpfc_is_sli4(phba) && (cp->a_flag & CFG_SLI4))
			goto is_valid1;
		else
			continue;
 is_valid1:
		if (j == 0) {
			if (cp->a_changestate != CFG_DYNAMIC)
				return EPERM;
			if (vce->setcfg)
				return( vce->setcfg(phba->pport, val) );
			else
				return EINVAL;
		}
		j--;
	}

	for (i = 0; i < phba->CfgCnt; i++) {
		/* First do sanity check */
		if (!(ce = phba->CfgTbl[i]))
			return EINVAL;
		if (!(cp = ce->entry))
			return EINVAL;
		/* Next check if parameter is valid for adapter */
		if (0 == (strncmp(cp->a_string, "pci-max-read",
			  sizeof("pci-max-read")))) {
			if (phba->hba_flag & HBA_DMA_RESTRICT)
				goto is_valid2;
		}
		if (cp->a_flag & CFG_COMMON)
			goto is_valid2;
		if (lpfc_is_fc(phba) && (cp->a_flag & CFG_FC))
			goto is_valid2;
		if (lpfc_is_fcoe(phba) && (cp->a_flag & CFG_FCOE))
			goto is_valid2;
		if (lpfc_is_lp(phba) && (cp->a_flag & CFG_LP))
			goto is_valid2;
		if (lpfc_is_sli4(phba) && (cp->a_flag & CFG_SLI4))
			goto is_valid2;
		else
			continue;
 is_valid2:
		if (j == 0) {
			if (cp->a_changestate != CFG_DYNAMIC)
				return EPERM;
			if (ce->setcfg)
				return( ce->setcfg(phba, val) );
			else
				return EINVAL;
		}
		j--;
	}

	return EINVAL;
}

static int
lpfc_ioctl_hba_get_event(struct lpfc_hba * phba,
			 struct lpfcCmdInput * cip)
{
	struct lpfcdfc_host  * dfchba  = NULL;
	struct lpfcdfc_event * evt     = NULL;
	struct event_data    * evtData = NULL;
	/* Bytes to copy_to_user */
	unsigned int bLen = 0;
	/* Bytes remaining after copy_to or from_user returns */
	unsigned int bRem = 0;
	int  retVal = 0;
	bool found  = false;

	dfchba = lpfcdfc_host_from_hba(phba);
	if (dfchba == NULL) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_EVENT,
			"1930 Exec Format Error, Dropping event\n");
		retVal = ENOEXEC;
		goto get_event_common_exit;
	}

	found  = false;
	retVal = EWOULDBLOCK;

	mutex_lock(&lpfcdfc_lock);
	if (!(list_empty(&dfchba->evtQueue))) {
		evt = list_entry(dfchba->evtQueue.next, struct lpfcdfc_event, node);
		evtData = &evt->evtData;
		/* Check output buffer size */
		if (evtData->len <= cip->lpfc_outsz) {
			list_del(&evt->node);
			(dfchba->qCount)--;
			found  = true;
			retVal = 0;
		}
		else {
			/* Too small... */
			bLen   = evtData->len;
			retVal = EFBIG;
		}
	}
	mutex_unlock(&lpfcdfc_lock);

	if (found == false) {
		if (retVal == EFBIG) {
			/* Inform library of required buffer size */
			bRem = copy_to_user(
				(void __user *)cip->lpfc_arg1,
				&bLen,
				sizeof (unsigned int)
			);
			if (bRem != 0)
				retVal = EFAULT;
		}
		goto get_event_common_exit;
	}

	/*
	 * We found an event on the queue and it's data buffer
	 * is the correct size. Cleared to proceed...
	 *
	 * From here on make certain to free memory we've pulled off 
	 * various lists
	 */
	evt->wait_time_stamp = jiffies;

	bLen = min_value(evtData->len, cip->lpfc_outsz);

	/* Event type */
	bRem = copy_to_user(
		(void __user *)cip->lpfc_arg3,
		&evtData->type,
		sizeof(struct event_type));

	if (bRem != 0) {
		retVal = EFAULT;
		goto get_event_free_mem;
	}

	/* Event specific tag information */
	bRem = copy_to_user(
		(void __user *)cip->lpfc_arg2,
		&evtData->immed_dat,
		sizeof (unsigned int));

	if (bRem != 0) {
		retVal = EFAULT;
		goto get_event_free_mem;
	}

	/* Amount of data being returned */
	bRem = copy_to_user(
		(void __user *)cip->lpfc_arg1,
		&bLen,
		sizeof (unsigned int));

	if (bRem != 0) {
		retVal = EFAULT;
		goto get_event_free_mem;
	}

	/* Event specific data - may also be NULL, i.e. no data to return */
	if (bLen > 0) {
		bRem = copy_to_user((void __user *) cip->lpfc_dataout,
			(const void  *) evtData->data, bLen);

		if (bRem != 0) {
			retVal = EFAULT;
			goto get_event_free_mem;
		}
	}

get_event_free_mem:
	evtData = &evt->evtData;
	lpfc_printf_log(phba, KERN_INFO, LOG_EVENT,
		"1904 Event delivered, Data: x%x x%x x%x x%x\n",
		evtData->type.mask, evtData->type.cat,
		evtData->type.sub, evtData->len);
	evtData->type.mask = FC_REG_DUMP_EVENT;
	(void) lpfc_evt_reclaim(phba, evt);

get_event_common_exit:
	/* Make certain the IOCTL dispatcher doesn't do 2nd copy */
	cip->lpfc_outsz = 0;
	return retVal;
}

static int
lpfc_ioctl_hba_set_event(struct lpfc_hba * phba, struct lpfcCmdInput * cip)
{
	struct lpfcdfc_host * dfchba = NULL;

	int  retVal = 0;
	bool found  = false;

	mutex_lock(&lpfcdfc_lock);
	list_for_each_entry(dfchba, &lpfcdfc_hosts, node) {
		if (dfchba->phba == phba) {
			found = true;
			/* Now interested in events */
			dfchba->qEvents = true;
			break;
		}
	}
	mutex_unlock(&lpfcdfc_lock);

	if (found == true) {
		lpfc_printf_log(phba, KERN_INFO, LOG_EVENT,
			"1903 Event Queuing enabled\n");
	}
	else
		retVal = ENOEXEC;

	/* Make certain the IOCTL dispatcher doesn't do 2nd copy */
	cip->lpfc_outsz = 0;
	return retVal;
}

static int
lpfc_ioctl_hba_unset_event(struct lpfc_hba * phba, struct lpfcCmdInput * cip)
{
	struct lpfcdfc_host  *dfchba = NULL;
	struct lpfcdfc_event *evt    = NULL;

	int  retVal = 0;
	bool found  = false;

	mutex_lock(&lpfcdfc_lock);
	list_for_each_entry(dfchba, &lpfcdfc_hosts, node) {
		if (dfchba->phba == phba) {
			found = true;
			/* No longer interested in events */
			dfchba->qEvents = false;
			break;
		}
	}
	mutex_unlock(&lpfcdfc_lock);

	if (found == true) {
		lpfc_printf_log(phba, KERN_INFO, LOG_EVENT,
			"1905 Event Queuing disabled\n");
		mutex_lock(&lpfcdfc_lock);
		while (! (list_empty(&dfchba->evtQueue)) ) {
			evt = list_entry(dfchba->evtQueue.next, struct lpfcdfc_event, node);
			list_del(&evt->node);
			(void) lpfc_evt_reclaim(phba, evt);
		}
		mutex_unlock(&lpfcdfc_lock);
	}
	else {
		retVal = ENOEXEC;
	}

	/* Make certain the IOCTL dispatcher doesn't do 2nd copy */
	cip->lpfc_outsz = 0;
	return retVal;
}


int
lpfc_ioctl_list_bind(struct lpfc_hba *phba, struct lpfcCmdInput *cip, void *dataout)
{
	unsigned long index = 0;
	unsigned long max_index = (unsigned long)cip->lpfc_arg1;
	HBA_BIND_LIST *bl;
	HBA_BIND_ENTRY *ba;
	struct lpfc_nodelist *ndlp;
	struct lpfc_vport *vport = phba->pport;
	enum fc_tgtid_binding_type node_bind_type;
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	int rc = 0;

	bl = (HBA_BIND_LIST *) dataout;
	ba = &bl->entry[0];

	spin_lock_irq(shost->host_lock);
	/* Iterate through the Node List and return all bound Nports. */
	list_for_each_entry(ndlp, &vport->fc_nodes, nlp_listp) {
		if (!NLP_CHK_NODE_ACT(ndlp))
			continue;

		if (index >= max_index) {
			rc = E2BIG;
			index++;
			continue;
		}

		memset(&ba[index], 0, sizeof (HBA_BIND_ENTRY));

		ba[index].did = ndlp->nlp_DID;
		memcpy(&ba[index].wwpn, &ndlp->nlp_portname, sizeof (HBA_WWN));
		memcpy(&ba[index].wwnn, &ndlp->nlp_nodename, sizeof (HBA_WWN));

		node_bind_type = fc_host_tgtid_bind_type(shost);
		switch (node_bind_type) {
		case FC_TGTID_BIND_BY_WWPN:
			ba[index].bind_type |= NODE_INFO_TYPE_WWPN;
			break;
		case FC_TGTID_BIND_BY_WWNN: 
			ba[index].bind_type |= NODE_INFO_TYPE_WWNN;
			break;
		case FC_TGTID_BIND_BY_ID:
			ba[index].bind_type |= NODE_INFO_TYPE_DID;
			break;
		default:
			ba[index].bind_type |= NODE_INFO_TYPE_AUTOMAP;
			break;
		}

		if (ndlp->nlp_type == NLP_FCP_TARGET) {
			ba[index].flags |= HBA_BIND_MAPPED;
			ba[index].scsi_id = ndlp->nlp_sid;

			if (ndlp->nlp_state == NLP_STE_NPR_NODE)
				ba[index].flags |= HBA_BIND_NODEVTMO;
		} else {
			if (ndlp->nlp_type == (NLP_FABRIC | NLP_FCP_INITIATOR)) {
				ba[index].flags |= HBA_BIND_UNMAPPED;
				if (ndlp->nlp_flag & NLP_TGT_NO_SCSIID)
					ba[index].flags |= HBA_BIND_NOSCSIID;

				if (ndlp->nlp_state == NLP_STE_NPR_NODE)
					ba[index].flags |= HBA_BIND_NODEVTMO;
			}
		}

		index++;
	}
	spin_unlock_irq(shost->host_lock);
	bl->NumberOfEntries = index;
	return rc;
}

int
lpfc_ioctl_get_vpd(struct lpfc_hba *phba, struct lpfcCmdInput *cip, void *dataout)
{
	struct vpd *dp;
	int rc = 0;

	dp = (struct vpd *) dataout;

	if (cip->lpfc_arg4 != VPD_VERSION1)
		rc = EINVAL;

	dp->version = VPD_VERSION1;

	memset(dp->ModelDescription, 0, sizeof(dp->ModelDescription));
	memset(dp->Model, 0, sizeof(dp->Model));
	memset(dp->ProgramType, 0, sizeof(dp->ProgramType));
	memset(dp->PortNum, 0, sizeof(dp->PortNum));

	if (phba->vpd_flag & VPD_MASK) {
		if (phba->vpd_flag & VPD_MODEL_DESC) {
			memcpy(dp->ModelDescription, phba->ModelDesc,
			       sizeof(dp->ModelDescription));
		}
		if (phba->vpd_flag & VPD_MODEL_NAME) {
			memcpy(dp->Model, phba->ModelName, sizeof(dp->Model));
		}
		if (phba->vpd_flag & VPD_PROGRAM_TYPE) {
			memcpy(dp->ProgramType, phba->ProgramType,
			       sizeof(dp->ProgramType));
		}
		if (phba->vpd_flag & VPD_PORT) {
			memcpy(dp->PortNum, phba->Port, sizeof(dp->PortNum));
		}
	}

	return rc;
}

int
lpfc_ioctl_get_dumpregion(struct lpfc_hba *phba, struct lpfcCmdInput  *cip, void *dataout)
{
	uint32_t identifier = (uint32_t)(unsigned long) cip->lpfc_arg1;
	uint32_t size = cip->lpfc_outsz;
	uint32_t *bufp = (uint32_t *) dataout;
	int rc = 0;

	switch (identifier) {
	case 0:		/* SLI Registers */
		if (size < 16) {
			/*
			 * App is hunting for size requirements.
			 * Return minimum buffer size that
			 * is required and return ENOMEM.
			 */
			rc = ENOMEM;
			size = 16;
			cip->lpfc_outsz = size;
		}
		else {
			/*
			 * App's buffer is large enough to
			 * hold SLI registers
			 */
			*bufp++ = readl((phba->ctrl_regs_memmap_p) + 0);
			*bufp++ = readl((phba->ctrl_regs_memmap_p) + 4);
			*bufp++ = readl((phba->ctrl_regs_memmap_p) + 8);
			*bufp++ = readl((phba->ctrl_regs_memmap_p) + 12);
                        size = 16;
			cip->lpfc_outsz = size;
		}

		/*
		 * Return buffer size in arg 2.
		 */
		if (copy_to_user((uint8_t *)cip->lpfc_arg2,
			(uint8_t *)&size, sizeof(uint32_t))) {
			rc = EIO;
		}
		break;

	case 1:		/* Board SLIM */
	case 2:		/* Port Control Block */
	case 3:		/* Mailbox in Host Memory */
	case 4:		/* Host Get/Put pointer array */
	case 5:		/* Port Get/Put pointer array */
	case 6:		/* Command/Response Ring */
	case 7:		/* DriverInternal Structures */
		rc = ENOENT;
		break;
	default:
		rc = EINVAL;
		break;
	}

	return rc;
}

int
lpfc_ioctl_get_lpfcdfc_info(struct lpfc_hba *phba, struct lpfcCmdInput * cip,
			    void *dataout)
{
	struct lpfc_dfc_drv_info *info;

	/* validate the user buffer size */
	if (cip->lpfc_outsz < sizeof(struct lpfc_dfc_drv_info))
		return EINVAL;

	info = (struct lpfc_dfc_drv_info *) dataout;
	sprintf((char *) info->version, "%s", LPFC_DRIVER_VERSION);
	sprintf((char *) info->name, "%s", LPFC_DRIVER_NAME);
	info->sliMode = phba->sli_rev;
	info->featureList = FEATURE_LIST;

	if (phba->hba_flag & HBA_FCOE_SUPPORT)
		info->featureList |= FBIT_FCOE_SUPPORT;

	if (lpfc_is_persistent_link_disable(phba))
		info->featureList |= FBIT_PERSIST_LINK_SUPPORT;

	info->hbaType = phba->pcidev->device;
	cip->lpfc_outsz = sizeof(struct lpfc_dfc_drv_info);
	return 0;
}


static int
lpfc_ioctl_loopback_mode(struct lpfc_hba *phba,
			 struct lpfcCmdInput  *cip, void *dataout)
{
	struct Scsi_Host *shost;
	struct lpfc_sli *psli = &phba->sli;
	struct lpfc_sli_ring *pring = &psli->ring[LPFC_FCP_RING];
	uint32_t link_flags = cip->lpfc_arg4;
	uint32_t timeout = cip->lpfc_arg5 * 1000;
	struct lpfc_vport **vports;
	LPFC_MBOXQ_t *pmboxq;
	int mbxstatus;
	int i = 0;
	int rc = 0;

	cip->lpfc_outsz = 0;

	if ((phba->link_state == LPFC_HBA_ERROR) ||
	    (psli->sli_flag & LPFC_BLOCK_MGMT_IO) ||
	    (!(psli->sli_flag & LPFC_SLI_ACTIVE)))
		return EACCES;

	if ((pmboxq = mempool_alloc(phba->mbox_mem_pool,GFP_KERNEL)) == 0)
		return ENOMEM;

	vports = lpfc_create_vport_work_array(phba);
	if (!vports) {
		mempool_free(pmboxq, phba->mbox_mem_pool);
		return ENOMEM;
	}

	for(i = 0; ((i <= phba->max_vpi) && (vports[i] != NULL)); i++) {
		if (vports[i]->load_flag)
			continue;
		shost = lpfc_shost_from_vport(vports[i]);
		scsi_block_requests(shost);
	}
	lpfc_destroy_vport_work_array(phba, vports);

	while (pring->txcmplq_cnt) {
		if (i++ > 1000)	/* wait up to 10 seconds */
			break;
		mdelay(10);
	}

	memset((void *)pmboxq, 0, sizeof (LPFC_MBOXQ_t));
	pmboxq->u.mb.mbxCommand = MBX_DOWN_LINK;
	pmboxq->u.mb.mbxOwner   = OWN_HOST;

	mbxstatus = lpfc_sli_issue_mbox_wait(phba, pmboxq, LPFC_MBOX_TMO);

	if ((mbxstatus == MBX_SUCCESS) && (pmboxq->u.mb.mbxStatus == 0)) {

		/* wait for link down before proceeding */
		i = 0;
		while (phba->link_state != LPFC_LINK_DOWN) {
			if (i++ > timeout) {
				rc = ETIMEDOUT;
				goto loopback_mode_exit;
			}
			msleep(10);
		}

		memset((void *)pmboxq, 0, sizeof (LPFC_MBOXQ_t));

		if (link_flags == INTERNAL_LOOP_BACK)
			pmboxq->u.mb.un.varInitLnk.link_flags = FLAGS_LOCAL_LB;
		else
			pmboxq->u.mb.un.varInitLnk.link_flags = FLAGS_TOPOLOGY_MODE_LOOP;

		pmboxq->u.mb.mbxCommand = MBX_INIT_LINK;
		pmboxq->u.mb.mbxOwner   = OWN_HOST;

		mbxstatus = lpfc_sli_issue_mbox_wait(phba, pmboxq, LPFC_MBOX_TMO);

		if ((mbxstatus != MBX_SUCCESS) || (pmboxq->u.mb.mbxStatus))
			rc = ENODEV;
		else {
			phba->link_flag |= LS_LOOPBACK_MODE;
			/* wait for the link attention interrupt */
			msleep(1000);

			i = 0;
			while (phba->link_state != LPFC_HBA_READY) {
				if (i++ > timeout) {
					rc = ETIMEDOUT;
					break;
				}
				msleep(10);
			}
		}
	} else
		rc = ENODEV;

loopback_mode_exit:
	vports = lpfc_create_vport_work_array(phba);
	if (vports) {
		for(i = 0; i <= phba->max_vpi && vports[i] != NULL; i++){
			if (vports[i]->load_flag)
				continue;
			shost = lpfc_shost_from_vport(vports[i]);
			scsi_unblock_requests(shost);
		}
		lpfc_destroy_vport_work_array(phba, vports);
	}
	else 
		rc = ENOMEM;

	/* SLI layer frees mboxq when status is TIMEOUT */
	if (mbxstatus != MBX_TIMEOUT)
		mempool_free(pmboxq, phba->mbox_mem_pool);

	return rc;
}

static int
lpfcdfc_loop_self_reg(struct lpfc_hba *phba, uint16_t * rpi)
{
	LPFC_MBOXQ_t *mbox;
	struct lpfc_dmabuf *dmabuff;
	int status;

	if (phba->sli.sli_flag & LPFC_BLOCK_MGMT_IO)
		return EAGAIN;

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (mbox == NULL)
		return ENOMEM;

	status = lpfc_reg_rpi(phba, phba->pport->vpi, phba->pport->fc_myDID,
			      (uint8_t *) &phba->pport->fc_sparam, mbox, 0);
	if (status) {
		mempool_free(mbox, phba->mbox_mem_pool);
		return ENOMEM;
	}

	dmabuff = (struct lpfc_dmabuf *) mbox->context1;
	mbox->context1 = NULL;
	status = lpfc_sli_issue_mbox_wait(phba, mbox, LPFC_MBOX_TMO);

	if ((status != MBX_SUCCESS) || (mbox->u.mb.mbxStatus)) {
		(void) lpfc_mbuf_reclaim(phba, dmabuff);
		if (status != MBX_TIMEOUT)
			mempool_free(mbox, phba->mbox_mem_pool);
		return ENODEV;
	}

	if (phba->sli_rev == LPFC_SLI_REV4)
		*rpi = mbox->u.mb.un.varRegLogin.rpi;
	else
		*rpi = mbox->u.mb.un.varWords[0];

	(void) lpfc_mbuf_reclaim(phba, dmabuff);

	mempool_free(mbox, phba->mbox_mem_pool);

	return 0;
}

static int
lpfcdfc_loop_self_unreg(struct lpfc_hba *phba, uint16_t rpi)
{
	LPFC_MBOXQ_t * mbox;
	int status;

	if (phba->sli.sli_flag & LPFC_BLOCK_MGMT_IO)
		return EAGAIN;

	/* Allocate mboxq structure */
	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (mbox == NULL)
		return ENOMEM;

	lpfc_unreg_login(phba, 0, rpi, mbox);
	status = lpfc_sli_issue_mbox_wait(phba, mbox, LPFC_MBOX_TMO);

	if ((status != MBX_SUCCESS) || (mbox->u.mb.mbxStatus)) {
		if (status != MBX_TIMEOUT)
			mempool_free(mbox, phba->mbox_mem_pool);
		return EIO;
	}

	mempool_free(mbox, phba->mbox_mem_pool);
	return 0;
}


static int
lpfcdfc_loop_get_xri(struct lpfc_hba *phba, uint16_t rpi,
		     uint16_t *txxri, uint16_t * rxxri)
{
	struct lpfcdfc_host * dfchba;
	struct lpfcdfc_event * evt;

	struct lpfc_iocbq *cmdiocbq, *rspiocbq;
	IOCB_t *cmd, *rsp;

	struct lpfc_dmabuf * dmabuf;
	struct ulp_bde64 *bpl = NULL;
	struct lpfc_sli_ct_request *ctreq = NULL;

	int  ret_val = 0;
	long timeout = 0;

	bool qEmpty = true;

	*txxri = 0;
	*rxxri = 0;

	if (phba->sli.sli_flag & LPFC_BLOCK_MGMT_IO)
		return EAGAIN;

	dfchba = lpfcdfc_host_from_hba(phba);
	if (dfchba == NULL) {
		lpfc_printf_log(phba, KERN_ERR, LOG_EVENT,
			"1929 Exec format error\n");
		return ENOEXEC;
	}

	cmdiocbq = lpfc_sli_get_iocbq(phba);
	rspiocbq = lpfc_sli_get_iocbq(phba);

	dmabuf = kmalloc(sizeof (struct lpfc_dmabuf), GFP_KERNEL);
	if (dmabuf) {
		dmabuf->virt = lpfc_mbuf_alloc(phba, 0, &dmabuf->phys);
		INIT_LIST_HEAD(&dmabuf->list);
		bpl = (struct ulp_bde64 *) dmabuf->virt;
		memset(bpl, 0, sizeof(*bpl));
		ctreq = (struct lpfc_sli_ct_request *)(bpl + 1);
		bpl->addrHigh =
			le32_to_cpu(putPaddrHigh(dmabuf->phys + sizeof(*bpl)));
		bpl->addrLow =
			le32_to_cpu(putPaddrLow(dmabuf->phys + sizeof(*bpl)));
		bpl->tus.f.bdeFlags = 0;
		bpl->tus.f.bdeSize = ELX_LOOPBACK_HEADER_SZ;
		bpl->tus.w = le32_to_cpu(bpl->tus.w);
	}

	if (cmdiocbq == NULL || rspiocbq == NULL ||
	    dmabuf == NULL || bpl == NULL || ctreq == NULL) {
		ret_val = ENOMEM;
		goto err_get_xri_exit;
	}

	cmd = &cmdiocbq->iocb;
	rsp = &rspiocbq->iocb;

	(void) memset(ctreq, 0, ELX_LOOPBACK_HEADER_SZ);

	ctreq->RevisionId.bits.Revision = SLI_CT_REVISION;
	ctreq->RevisionId.bits.InId = 0;
	ctreq->FsType = SLI_CT_ELX_LOOPBACK;
	ctreq->FsSubType = 0;
	ctreq->CommandResponse.bits.CmdRsp = ELX_LOOPBACK_XRI_SETUP;
	ctreq->CommandResponse.bits.Size = 0;

	cmd->un.xseq64.bdl.addrHigh = putPaddrHigh(dmabuf->phys);
	cmd->un.xseq64.bdl.addrLow  = putPaddrLow(dmabuf->phys);
	cmd->un.xseq64.bdl.bdeFlags = BUFF_TYPE_BLP_64;
	cmd->un.xseq64.bdl.bdeSize  = sizeof(struct ulp_bde64);
	cmd->un.xseq64.w5.hcsw.Fctl  = LA;
	cmd->un.xseq64.w5.hcsw.Dfctl = 0;
	cmd->un.xseq64.w5.hcsw.Rctl = FC_RCTL_DD_UNSOL_CTL;
	cmd->un.xseq64.w5.hcsw.Type = FC_TYPE_CT;
	cmd->ulpCommand = CMD_XMIT_SEQUENCE64_CR;
	cmd->ulpBdeCount = 1;
	cmd->ulpLe = 1;
	cmd->ulpClass = CLASS3;
	cmd->ulpContext = rpi;

	cmdiocbq->iocb_flag |= LPFC_IO_LIBDFC;
	cmdiocbq->vport = phba->pport;

	ret_val = lpfc_sli_issue_iocb_wait(phba, LPFC_ELS_RING, cmdiocbq, rspiocbq,
			(phba->fc_ratov * 2) + LPFC_DRVR_TIMEOUT);
	if (ret_val == IOCB_TIMEDOUT) {
		bool timedout = true;
		IOCB_TIMEOUT_T args;

		(void) memset(&args, 0, sizeof(IOCB_TIMEOUT_T));

		args.phba     = phba;
		args.cmdiocbq = cmdiocbq;
		args.rspiocbq = rspiocbq;
		args.db0      = dmabuf;

		timedout = lpfc_process_iocb_timeout(&args);

		if (timedout)
			return ETIMEDOUT;
		else 	ret_val = IOCB_SUCCESS;
	}

	if (ret_val)
		goto err_get_xri_exit;

	*txxri = rsp->ulpContext;

	timeout = (phba->fc_ratov * 2) + (LPFC_DRVR_TIMEOUT * HZ);

	ret_val = wait_event_interruptible_timeout(
			dfchba->waitQ,
			(!list_empty(&dfchba->diagQ)),
			timeout);

	if (ret_val == 0) {
		ret_val = ETIMEDOUT;
		goto err_get_xri_exit;
	}

	mutex_lock(&lpfcdfc_lock);
	qEmpty = list_empty(&dfchba->diagQ);
	if (qEmpty == true) {
		ret_val = EIO;
		evt     = NULL;
	}
	else {
		evt = list_entry(dfchba->diagQ.next, struct lpfcdfc_event, node);
		list_del(&evt->node);
		ret_val = IOCB_SUCCESS;
	}
	mutex_unlock(&lpfcdfc_lock);

	if (evt != NULL) {
		*rxxri = evt->evtData.immed_dat;
		(void) lpfc_evt_reclaim(phba, evt);
	}

err_get_xri_exit:

	/* Resources free'd in lpfc_ioctl_timeout_iocb_cmpl */
	if (ret_val == IOCB_SUCCESS) {
		if (dmabuf)
			(void) lpfc_mbuf_reclaim(phba, dmabuf);
	}

	if (cmdiocbq && (ret_val != IOCB_TIMEDOUT))
		lpfc_sli_release_iocbq(phba, cmdiocbq);
	if ((rspiocbq != NULL) && (ret_val != IOCB_TIMEDOUT))
		lpfc_sli_release_iocbq(phba, rspiocbq);

	return ret_val;
}

/* Only called in SL2 mode */
static int 
lpfcdfc_loop_post_rxbufs(struct lpfc_hba *phba, uint16_t rxxri, size_t len,
			 struct lpfc_dmabuf *rxbmp)
{
	struct lpfc_sli *psli = NULL;
	struct lpfc_sli_ring *pring = NULL;
	struct lpfc_iocbq *cmdiocbq = NULL;
	struct ulp_bde64 *rxbpl = NULL;
	struct lpfc_dmabufext *mlist = NULL;
	struct lpfc_dmabufext *mnext = NULL;
	struct lpfc_dmabuf *mp = NULL;
	struct lpfc_dmabuf *mpSave[] = {NULL,NULL};
	int rc = 0;
	uint32_t flag = 0;
	uint8_t numBde = 0;
	uint8_t k = 0;
	IOCB_t *cmd = NULL;

	if (rxbmp == NULL) {
		lpfc_printf_log(phba, KERN_ERR, LOG_EVENT,
			"1949 ENOEXEC NULL parameter passed to function\n");
		return ENOEXEC;
	}

	psli  = &phba->sli;
	pring = &psli->ring[LPFC_ELS_RING];
	INIT_LIST_HEAD(&rxbmp->list);

	rxbmp->virt = lpfc_mbuf_alloc(phba, 0, &rxbmp->phys);
	if (rxbmp->virt == NULL) {
		lpfc_printf_log(phba, KERN_ERR, LOG_EVENT,
			"1951 ENOMEM MBUF resource navailale\n");
		return ENOMEM;
	}

	/* If errors occur after this point do not attempt to free
 	 * the mbuf resource. Clean up for that resource is done
 	 * in the calling function.
 	 */
	rxbpl = (struct ulp_bde64 *) rxbmp->virt;
	mlist = dfc_cmd_data_alloc(phba, NULL, rxbpl, len);
	if (mlist == NULL) {
		lpfc_printf_log(phba, KERN_ERR, LOG_EVENT,
			"1952 ENOMEM DMA resource navailale\n");
		return ENOMEM;
	}

	numBde = mlist->flag;

	/* This logic can only handle a single iocbq. Therefore the 
 	 * number of BDEs is limited to two.
 	 */
	if ( (numBde == 0) || (numBde > 2) ) {
		lpfc_printf_log(phba, KERN_ERR, LOG_EVENT,
			"1957 EPERM Illegal BDE count [%d]\n",numBde);
		dfc_cmd_data_free(phba, mlist);
		return EPERM;
	}

	cmdiocbq = lpfc_sli_get_iocbq(phba);
	if (cmdiocbq == NULL) {
		lpfc_printf_log(phba, KERN_ERR, LOG_EVENT,
			"1950 ENOMEM IOCB resource not available\n");
		dfc_cmd_data_free(phba, mlist);
		return ENOMEM;
	}

	cmd = &cmdiocbq->iocb;
	mnext = mlist;
	mp = &mnext->dma;
	for (k = 0; k < numBde; k++) {
		list_del_init(&mp->list);
		mpSave[k] = mp;
		cmd->un.cont64[k].addrHigh = putPaddrHigh(mp->phys);
		cmd->un.cont64[k].addrLow  = putPaddrLow(mp->phys);
		cmd->un.cont64[k].tus.f.bdeSize = mp->bsize;
		lpfc_sli_ringpostbuf_put(phba, pring, mp);
		mnext = (struct lpfc_dmabufext *) mnext->dma.list.next;
		mp = &mnext->dma;

	} /* END for (k = 0; k < numBde; k++) */

	cmd->ulpBdeCount = numBde;
	cmd->ulpCommand  = CMD_QUE_XRI_BUF64_CX;
	cmd->ulpClass    = CLASS3;
	cmd->ulpContext  = rxxri;
	cmd->ulpLe       = 1;

	flag |= SLI_IOCB_RET_IOCB;

	rc = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, cmdiocbq, flag);
	
	lpfc_printf_log(phba, KERN_INFO, LOG_EVENT,
			"1906 IOCB completion status: %d\n", rc);
	
	if ((rc == IOCB_BUSY) || (rc == IOCB_ERROR)) {
		/* We need to reclaim 1 and maybe two mbufs */
		for (k = 0; k < numBde; k++) {
			mp = mpSave[k];		
			if (mp != NULL) {
				(void) lpfc_sli_ringpostbuf_get(phba, pring, mp->phys);
				(void) lpfc_dmabuf_reclaim(phba, mp);
			}
		}
		dfc_cmd_data_free(phba, mlist);
	}

	lpfc_sli_release_iocbq(phba, cmdiocbq);
	return rc;
}

static int
lpfc_ioctl_loopback_test(struct lpfc_hba *phba,
		   struct lpfcCmdInput *cip, void *dataout)
{
	struct lpfcdfc_host * dfchba = NULL;
	struct lpfcdfc_event * evt = NULL;
	struct lpfc_sli *psli = &phba->sli;
	uint32_t size = cip->lpfc_outsz;
	uint32_t full_size = size + ELX_LOOPBACK_HEADER_SZ;
	size_t segment_len = 0, segment_offset = 0, current_offset = 0;
	uint16_t rpi;
	struct lpfc_iocbq *cmdiocbq, *rspiocbq;
	IOCB_t *cmd, *rsp;
	struct lpfc_sli_ct_request *ctreq;
	struct lpfc_dmabuf *txbmp;
	struct ulp_bde64 *txbpl = NULL;
	struct lpfc_dmabufext *txbuffer = NULL;
	struct lpfc_dmabuf *curr;
	uint16_t txxri, rxxri;
	uint32_t num_bde;
	int rc;
	uint8_t *ptr = NULL, *rx_databuf = NULL;
	struct lpfc_dmabuf *rxbmp = NULL;
	struct list_head head;
	bool sli2 = false;

	if ((phba->link_state == LPFC_HBA_ERROR)  ||
	    (psli->sli_flag & LPFC_BLOCK_MGMT_IO) ||
	    (!(psli->sli_flag & LPFC_SLI_ACTIVE)))
		return EACCES;

	if (!lpfc_is_link_up(phba) || !(phba->link_flag & LS_LOOPBACK_MODE))
		return EACCES;

	if ((size == 0) || (full_size > LOOPBACK_MAX_BUFSIZE)) /* 8192 bytes MAX */
		return ERANGE;

	dfchba = lpfcdfc_host_from_hba(phba);
	if (dfchba == NULL) {
		lpfc_printf_log(phba, KERN_ERR, LOG_EVENT,
			"1926 Exec format error\n");
		return ENOEXEC;
	}

        sli2 = (phba->sli3_options & LPFC_SLI3_HBQ_ENABLED) ? false : true;

	rc = lpfcdfc_loop_self_reg(phba, &rpi);
	if (rc)
		return rc;

	rc = lpfcdfc_loop_get_xri(phba, rpi, &txxri, &rxxri);
	if (rc) {
		lpfcdfc_loop_self_unreg(phba, rpi);
		return rc;
	}

        if (sli2) {
		rxbmp = kzalloc(sizeof(struct lpfc_dmabuf), GFP_KERNEL);
		if (rxbmp == NULL) {
			lpfcdfc_loop_self_unreg(phba, rpi);
			return ENOMEM;
		}
		rc = lpfcdfc_loop_post_rxbufs(phba, rxxri, full_size, rxbmp);
		if (rc) {
			/* Resources free'd in post_rxbufs logic */
			lpfcdfc_loop_self_unreg(phba, rpi);
			return rc;
		}
	}

	/* If SLI2 then we may or may not have outstanding receive 
 	 * buffers depending on where and how the failure occurs. We 
 	 * may not be able to reclaim these resources if we encounter
 	 * from this point on.
 	 */
	cmdiocbq = lpfc_sli_get_iocbq(phba);
	if (cmdiocbq == NULL) {
		lpfcdfc_loop_self_unreg(phba, rpi);
		(void) lpfc_mbuf_reclaim(phba, rxbmp);
		return ENOMEM;
	}

	rspiocbq = lpfc_sli_get_iocbq(phba);
	if (rspiocbq == NULL) {
		lpfcdfc_loop_self_unreg(phba, rpi);
		lpfc_sli_release_iocbq(phba, cmdiocbq);
		(void) lpfc_mbuf_reclaim(phba, rxbmp);
		return ENOMEM;
	}

	txbmp = kmalloc(sizeof (struct lpfc_dmabuf), GFP_KERNEL);
	if (txbmp == NULL) {
		lpfcdfc_loop_self_unreg(phba, rpi);
		lpfc_sli_release_iocbq(phba, cmdiocbq);
		lpfc_sli_release_iocbq(phba, rspiocbq);
		(void) lpfc_mbuf_reclaim(phba, rxbmp);
		return ENOMEM;
	}

	txbmp->virt = lpfc_mbuf_alloc(phba, 0, &txbmp->phys);
	INIT_LIST_HEAD(&txbmp->list);
	txbpl = (struct ulp_bde64 *) txbmp->virt;
	if (txbpl)
		txbuffer = dfc_cmd_data_alloc(phba, NULL, txbpl, full_size);

	if ((txbmp == NULL) || (txbpl == NULL) || (txbuffer == NULL)) {
		rc = ENOMEM;
		goto err_loopback_test_exit;
	}

	num_bde = (uint32_t)txbuffer->flag;

	if (num_bde == 0) {
		rc = ENOMEM;
		lpfc_printf_log(phba, KERN_ERR, LOG_EVENT,
			"1936 ENOMEM Kernel resource unavailale\n");
		goto err_loopback_test_exit;
	}

	cmd = &cmdiocbq->iocb;
	rsp = &rspiocbq->iocb;

	INIT_LIST_HEAD(&head);
	list_add_tail(&head, &txbuffer->dma.list);
	list_for_each_entry(curr, &head, list) {
		segment_len = ((struct lpfc_dmabufext *)curr)->size;
		if (current_offset == 0) {
			ctreq = curr->virt;
			memset(ctreq, 0, ELX_LOOPBACK_HEADER_SZ);
			ctreq->RevisionId.bits.Revision = SLI_CT_REVISION;
			ctreq->RevisionId.bits.InId = 0;
			ctreq->FsType = SLI_CT_ELX_LOOPBACK;
			ctreq->FsSubType = 0;
			ctreq->CommandResponse.bits.CmdRsp = ELX_LOOPBACK_DATA ;
			ctreq->CommandResponse.bits.Size   = size;
			segment_offset = ELX_LOOPBACK_HEADER_SZ;
		} else
			segment_offset = 0;

		BUG_ON(segment_offset >= segment_len);
		if (copy_from_user (curr->virt + segment_offset,
				    (void __user *)cip->lpfc_arg1
				    + current_offset,
				    segment_len - segment_offset)) {
			rc = EIO;
			goto err_loopback_test_exit;
		}

		current_offset += segment_len - segment_offset;
		BUG_ON(current_offset > size);
	}
	list_del(&head);

	/* Build the XMIT_SEQUENCE iocb */

	cmd->un.xseq64.bdl.addrHigh = putPaddrHigh(txbmp->phys);
	cmd->un.xseq64.bdl.addrLow  = putPaddrLow(txbmp->phys);
	cmd->un.xseq64.bdl.bdeFlags = BUFF_TYPE_BLP_64;
	cmd->un.xseq64.bdl.bdeSize  = (num_bde * sizeof(struct ulp_bde64));

	cmd->un.xseq64.w5.hcsw.Fctl = (LS | LA);
	cmd->un.xseq64.w5.hcsw.Dfctl = 0;
	cmd->un.xseq64.w5.hcsw.Rctl = FC_RCTL_DD_UNSOL_CTL;
	cmd->un.xseq64.w5.hcsw.Type = FC_TYPE_CT;

	cmd->ulpCommand = CMD_XMIT_SEQUENCE64_CX;
	cmd->ulpBdeCount = 1;
	cmd->ulpLe = 1;
	cmd->ulpClass = CLASS3;
	cmd->ulpContext = txxri;

	cmdiocbq->iocb_flag |= LPFC_IO_LIBDFC;
	cmdiocbq->vport = phba->pport;

	rc = lpfc_sli_issue_iocb_wait(phba, LPFC_ELS_RING, cmdiocbq, rspiocbq,
			(phba->fc_ratov * 2) + LPFC_DRVR_TIMEOUT);
	if (rc == IOCB_TIMEDOUT) {
		bool timedout = true;
		IOCB_TIMEOUT_T args;
		(void) memset(&args, 0, sizeof(IOCB_TIMEOUT_T));
		args.phba     = phba;
		args.cmdiocbq = cmdiocbq;
		args.rspiocbq = rspiocbq;
		args.db0      = txbmp;
		args.dbext0   = txbuffer;
		timedout = lpfc_process_iocb_timeout(&args);
		if (timedout)
			return ETIMEDOUT;
		else
			rc = IOCB_SUCCESS;
 	}

	if (rc != IOCB_SUCCESS)
		goto err_loopback_test_exit;


	rc = wait_event_interruptible_timeout(
		dfchba->waitQ,
		(!list_empty(&dfchba->diagQ)),
		((phba->fc_ratov * 2) + (LPFC_DRVR_TIMEOUT * HZ)));

	if (rc == 0) {
		rc = ETIMEDOUT;
		goto err_loopback_test_exit;
	}

	mutex_lock(&lpfcdfc_lock);
	if (list_empty(&dfchba->diagQ)) {
		rc  = EIO;
		evt = NULL;
	}
	else {
		evt = list_entry(dfchba->diagQ.next, struct lpfcdfc_event, node);
		list_del(&evt->node);
	}
	mutex_unlock(&lpfcdfc_lock);

	if (evt == NULL) {
		lpfc_printf_log(phba, KERN_ERR, LOG_EVENT,
			"1935 Loopback test did receive any data\n");
		goto err_loopback_test_exit;
	}
	

	if (evt->evtData.len != full_size) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_EVENT,
			"1931 Loopback test did not receive expected "
			"data length. received length %d expected "
			"length %d\n", evt->evtData.len, full_size);
		rc = EIO;
		goto err_loopback_test_exit;
	}

	ptr        = (uint8_t *)dataout;
	rx_databuf = (uint8_t *)evt->evtData.data;
	rx_databuf += ELX_LOOPBACK_HEADER_SZ;

	if (rx_databuf == NULL) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_EVENT,
			"1932 Loopback test failure ENOMEM\n");
		rc = ENOMEM;
		goto err_loopback_test_exit;
	}

	(void) memcpy(ptr, rx_databuf, size);

	rc = IOCB_SUCCESS;

err_loopback_test_exit:

	lpfcdfc_loop_self_unreg(phba, rpi);

	(void) lpfc_evt_reclaim(phba, evt);

	if (cmdiocbq != NULL)
		lpfc_sli_release_iocbq(phba, cmdiocbq);

	if (rspiocbq != NULL)
		lpfc_sli_release_iocbq(phba, rspiocbq);

	if (txbuffer != NULL)
		dfc_cmd_data_free(phba, txbuffer);

	if (txbmp != NULL)
		(void) lpfc_mbuf_reclaim(phba, txbmp);

	if ((sli2) && (rxbmp))
		(void) lpfc_mbuf_reclaim(phba, rxbmp);

	return rc;
}

uint32_t 
dfc_npiv_checklist (struct lpfc_vport *vport)
{
	struct lpfc_hba *phba = vport->phba;
	uint32_t checklist = 0;
	uint32_t mvpi, avpi;
	int ret;

	checklist |= CHECKLIST_BIT_NPIV;
	if (phba->sli_rev == LPFC_SLI_REV3) {
		checklist |= CHECKLIST_BIT_SLI3;

		if (phba->vpd.rev.feaLevelHigh >= 0x09)
			checklist |= CHECKLIST_BIT_HBA;

		mvpi = avpi = 0;
		ret = lpfc_get_hba_info(phba, NULL, NULL, NULL, NULL,
			&mvpi, &avpi);
		if (ret && avpi > 0)
			checklist |= CHECKLIST_BIT_RSRC;
	}

	/* Use the physical link state */
	switch (phba->link_state) {
	case LPFC_LINK_UP:
	case LPFC_CLEAR_LA:
	case LPFC_HBA_READY:
		/*
		 * dfc_npiv spec says:
		 * Bit 0, 1, 2, 4 will be reported regardless
		 * of the driver parameter setting for enable NPIV
		 */
		checklist |= CHECKLIST_BIT_LINK;

		/*
		 * dfc_npiv spec says:
		 * Bit 5 -7 should be ignored, 
		 * if any one of the bits (bit 0 - 4) is '0'
		 */
		if ((CHECKLIST_BIT_NPIV + CHECKLIST_BIT_SLI3 + 
		     CHECKLIST_BIT_HBA + CHECKLIST_BIT_RSRC) == 
		    (checklist & 
		     (CHECKLIST_BIT_NPIV + CHECKLIST_BIT_SLI3 +
		      CHECKLIST_BIT_HBA + CHECKLIST_BIT_RSRC))) {
			if (phba->fc_topology == TOPOLOGY_PT_PT)
        	               	checklist |= CHECKLIST_BIT_FBRC;
			if (phba->sli3_options & LPFC_SLI3_NPIV_ENABLED)
				checklist |= CHECKLIST_BIT_FSUP + 
					     CHECKLIST_BIT_NORSRC;
	      	}
		break;
	case LPFC_LINK_UNKNOWN:
	case LPFC_WARM_START:
	case LPFC_INIT_START:
	case LPFC_INIT_MBX_CMDS:
	case LPFC_LINK_DOWN:
	case LPFC_HBA_ERROR:
	default:
		break;
	}
	return checklist;
}


int
lpfc_ioctl_vport_getcfg(struct lpfc_hba *phba, struct lpfcCmdInput *cip,
			void *dataout)
{
        struct CfgParam *cp;
        struct VPCfgEntry *vce;
	struct CfgEntry *ce;
        struct CfgParam *export = dataout;
        int i, rc = 0, cnt = 0, found = 0;
	struct lpfc_vport *vport = NULL;
	struct lpfc_vport **vports;
	HBA_WWN vport_wwpn;

	if (copy_from_user(&vport_wwpn, cip->lpfc_arg1, sizeof (vport_wwpn)))
		return 0;

	vports = lpfc_create_vport_work_array(phba);
	if (!vports)
		return ENOMEM;

	for (i = 0; i <= phba->max_vpi && vports[i] != NULL; i++) {
		if (vports[i]->load_flag)
			continue;
		rc = memcmp(&vport_wwpn, &vports[i]->fc_portname,
			    sizeof (HBA_WWN));
		if (rc == 0) {
			found = 1;
			vport = vports[i];
			break;
		}
        }

	lpfc_destroy_vport_work_array(phba, vports);
        if (!found) {
                cip->lpfc_outsz = 0;
		return 0;
        }

	/* Always get the vport parameters. */
	cp = (struct CfgParam *) dataout;
	for (i = 0; i < vport->CfgCnt; i++) {
		if (!(vce = vport->CfgTbl[i])) 
			return EINVAL;
		if (!(cp = vce->entry)) 
			return EINVAL;
		if (vce->getcfg) 
			cp->a_current = vce->getcfg(vport);
		if ((cnt+1 * sizeof(struct CfgParam)) > cip->lpfc_outsz) {
			rc = E2BIG;
			break;
		}
		memcpy((uint8_t*)export, (uint8_t*)cp, sizeof(struct CfgParam));
		export++;
		cnt++;
	}

	/* If this is the phba's WWPN, return the HBA parameters */ 
	if (vport->port_type == LPFC_PHYSICAL_PORT) {
		for (i = 0; i < phba->CfgCnt; i++) {
			if (!(ce = phba->CfgTbl[i]))
				return EINVAL;
			if (!(cp = ce->entry))
				return EINVAL;
			if (ce->getcfg)
				cp->a_current = ce->getcfg(phba);
			if ((cnt+1 * 
			     sizeof(struct CfgParam)) > cip->lpfc_outsz) {
				rc = E2BIG;
				break;
			}
			memcpy((uint8_t*)export, (uint8_t*)cp,
			       sizeof(struct CfgParam));
			export++;
			cnt++;
		}
	}

	cip->lpfc_outsz = (uint32_t)(cnt * sizeof(struct CfgParam));
	return rc;
}


int
lpfc_ioctl_vport_setcfg(struct lpfc_hba *phba, struct lpfcCmdInput *cip)
{
	uint32_t offset, val;
	int i, j, rc;
	struct VPCfgEntry *vce = NULL;
	struct CfgEntry *ce = NULL;
	struct CfgParam *cp = NULL;
	int found = 0;
	struct lpfc_vport *vport = NULL;
	struct lpfc_vport **vports;
	HBA_WWN vport_wwpn;

	if (copy_from_user(&vport_wwpn, cip->lpfc_arg1, sizeof (vport_wwpn)))
		return 0;

	vports = lpfc_create_vport_work_array(phba);
	if (!vports)
		return ENOMEM;

	for (i = 0; i <= phba->max_vpi && vports[i] != NULL; i++) {
		if (vports[i]->load_flag)
			continue;
		rc = memcmp(&vport_wwpn, &vports[i]->fc_portname,
			    sizeof (HBA_WWN));
		if (rc == 0) {
			found = 1;
			vport = vports[i];
			break;
		}
        }

	lpfc_destroy_vport_work_array(phba, vports);
	if (!found) {
		cip->lpfc_outsz = 0;
		return 0;
	}

	offset = (ulong) cip->lpfc_arg2;
	val = (ulong) cip->lpfc_arg3;

	/* First, check for vport parameter */
	j = offset;
	for (i = 0; i < vport->CfgCnt; i++) {
		if (!(vce = vport->CfgTbl[i]))
			return EINVAL;
		if (!(cp = vce->entry))
			return EINVAL;
		if (j == 0) {
			if ((cp->a_changestate != CFG_DYNAMIC) &&
			    (cp->a_changestate != CFG_LINKRESET))
				return EPERM;
			if (vce->setcfg)
				return( vce->setcfg(vport, val) );
			else
				return EINVAL;
		}
		j--;
	}

	/* Second, check for phba parameter */
	for (i = 0; i < phba->CfgCnt; i++) {
		if (!(ce = phba->CfgTbl[i]))
			return EINVAL;
		if (!(cp = ce->entry))
			return EINVAL;
		if (j == 0) {
			if ((cp->a_changestate != CFG_DYNAMIC) &&
			   (cp->a_changestate != CFG_LINKRESET))
				return EPERM;
			if (ce->setcfg)
				return( ce->setcfg(phba, val) );
			else
				return EINVAL;
		}
		j--;
	}
	return EINVAL;
}

int
lpfc_ioctl_vport_attrib(struct lpfc_hba *phba, struct lpfcCmdInput *cip,
			void *dataout)
{
	int i, rc = 0;
	int found = 0;
	struct lpfc_vport *vport = NULL;
	struct lpfc_vport **vports;
	HBA_WWN vport_wwpn;
	DFC_VPAttrib *pAttrib = dataout;

	if (copy_from_user(&vport_wwpn, cip->lpfc_arg1, sizeof (vport_wwpn)))
		return EIO;

	/* Search for user's vport_wwpn */
	vports = lpfc_create_vport_work_array(phba);
	if (!vports)
		return ENOMEM;

	for (i = 0; i <= phba->max_vpi && vports[i] != NULL; i++) {
		if (vports[i]->load_flag)
			continue;
		rc = memcmp(&vport_wwpn, &vports[i]->fc_portname, sizeof (HBA_WWN));
		if (rc == 0) {
			if (vports[i]->port_type == LPFC_PHYSICAL_PORT)
				continue;
			found = 1;
			vport = vports[i];
			break;
		}
        }

	lpfc_destroy_vport_work_array(phba, vports);
	if (!found) {
		cip->lpfc_outsz = 0;
		rc = ENOENT;
		goto exit;
	}

	pAttrib->ver = VP_ATTRIB_VERSION;
	pAttrib->flags = VP_ATTRIB_NO_DELETE;
	pAttrib->portFcId = vport->fc_myDID;
	memcpy(&pAttrib->wwpn, &vport->fc_portname, 
		sizeof(pAttrib->wwpn));
	memcpy(&pAttrib->wwnn, &vport->fc_nodename,
		sizeof(pAttrib->wwnn));
	memcpy(&pAttrib->fabricName, &phba->fc_fabparam.nodeName,
		sizeof(pAttrib->fabricName));
	lpfc_vport_symbolic_port_name(vport, pAttrib->name,
				      sizeof(pAttrib->name));

	switch (vport->port_state) {
	case LPFC_LOCAL_CFG_LINK:
	case LPFC_FLOGI:
	case LPFC_FABRIC_CFG_LINK:
	case LPFC_NS_REG:
	case LPFC_NS_QRY:
	case LPFC_BUILD_DISC_LIST:
	case LPFC_DISC_AUTH:
		pAttrib->state = ATTRIB_STATE_INIT;
		break;
	case LPFC_VPORT_READY:
		pAttrib->state = ATTRIB_STATE_ACTIVE;
		break;
	case LPFC_VPORT_UNKNOWN:
	case LPFC_VPORT_FAILED:
	default:
		pAttrib->state = ATTRIB_STATE_FAILED;
		break;
	}
	pAttrib->checklist = dfc_npiv_checklist(vport);

 exit:
        cip->lpfc_outsz = min_value(cip->lpfc_outsz, sizeof(DFC_VPAttrib));
	return rc;
}

int
lpfc_ioctl_npiv_ready(struct lpfc_hba *phba, struct lpfcCmdInput *cip, void *dataout)
{
	uint32_t *pchecklist = dataout;

	if (cip->lpfc_outsz >= sizeof(uint32_t)) {
		*pchecklist = dfc_npiv_checklist(phba->pport);
		cip->lpfc_outsz = sizeof(uint32_t);
		return 0;
	}

	return E2BIG;
}

int
lpfc_ioctl_vport_resrc(struct lpfc_hba *phba, struct lpfcCmdInput *cip, void *dataout)
{
	uint32_t mrpi, arpi, mvpi, avpi;
	DFC_VPResource *pResrc = dataout;

	mrpi = arpi = mvpi = avpi = 0;
	if (lpfc_get_hba_info(phba, NULL, NULL, &mrpi, &arpi, &mvpi, &avpi)) {
		pResrc->vlinks_max = mvpi;
		pResrc->vlinks_inuse = mvpi - avpi;
		pResrc->rpi_max = mrpi;
		pResrc->rpi_inuse = mrpi - arpi;
		cip->lpfc_outsz = min_value(cip->lpfc_outsz,
			sizeof(DFC_VPResource));
		return 0;
	}
	return EIO;
}

int
lpfc_ioctl_vport_list(struct lpfc_hba *phba, struct lpfcCmdInput *cip, void *dataout)
{
	int i, rc;
	struct lpfc_vport **vports;
	int entry = 0;
	uint32_t numberOfEntries = 0;
	uint32_t len = sizeof(numberOfEntries);
	DFC_VPEntryList *vplist = dataout;
	DFC_VPEntry *vpentry = vplist->vpentry;

	/* Copy user data to get number of host entries available */
	if (copy_from_user(&numberOfEntries, cip->lpfc_arg1, sizeof(numberOfEntries)))
		return EIO;

	/* Find all the vports on this pport */
        vports = lpfc_create_vport_work_array(phba);
        if (!vports)
		return ENOMEM;

	for(i = 0; i <= phba->max_vpi && vports[i] != NULL; i++) {
		if (vports[i]->port_type == LPFC_PHYSICAL_PORT)
			continue;
		if (vports[i]->load_flag)
			continue;
		if (entry < numberOfEntries) {
			memcpy(&vpentry->wwnn, &vports[i]->fc_nodename,
			       sizeof (HBA_WWN));
			memcpy(&vpentry->wwpn, &vports[i]->fc_portname,
			       sizeof (HBA_WWN));
			vpentry->PortFcId = vports[i]->fc_myDID;
			len += sizeof(DFC_VPEntry);
			vpentry++;
		}
		entry++;
	}

	rc = 0;
	if (entry > numberOfEntries)
		rc = E2BIG;

	vplist->numberOfEntries = entry;
	cip->lpfc_outsz = min_value(len, cip->lpfc_outsz);
	lpfc_destroy_vport_work_array(phba, vports);
	return rc;
}

int
lpfc_ioctl_vport_nodeinfo(struct lpfc_hba *phba, struct lpfcCmdInput *cip,
			  void *dataout)
{

	int i, rc;
	struct lpfc_vport *vport = NULL;
	struct lpfc_vport **vports = NULL;
	int entry = 0, found = 0;
	uint32_t numberOfEntries = 0;
	DFC_GetNodeInfo *vplist = dataout;
	DFC_NodeInfoEntry *np = vplist->nodeInfo;
        HBA_WWN vport_wwpn;
	struct lpfc_nodelist *ndlp;
	enum fc_tgtid_binding_type node_bind_type;
	struct Scsi_Host *shost = NULL;
#ifdef CONFIG_COMPAT
	uint8_t *app_ptr = 0;

	if (cip->lpfc_cntl == LPFC_CNTL_X86_APP) {
		/*
		 * The driver is talking to an application in 32-bit
		 * addressing mode. Initialize app_ptr to the caller's
		 * data buffer and bump past numberOfEntries in
		 * DFC_GetNodeInfo
		 */
		app_ptr = (uint8_t *) cip->lpfc_dataout;
		app_ptr += sizeof(uint32_t);
	}	
#endif

	if (copy_from_user(&vport_wwpn, cip->lpfc_arg1, sizeof (vport_wwpn)))
		return EIO;

	numberOfEntries = (ulong) cip->lpfc_arg2;

	/* Search for user's vport_wwpn */
        vports = lpfc_create_vport_work_array(phba);
        if (!vports)
		return ENOMEM;

	for(i = 0; i <= phba->max_vpi && vports[i] != NULL; i++) {
		if (vports[i]->load_flag)
			continue;
		rc = memcmp(&vport_wwpn, &vports[i]->fc_portname,
			    sizeof (HBA_WWN));
		if (rc == 0) {
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
		if (!NLP_CHK_NODE_ACT(ndlp) ||
		    (ndlp->nlp_state != NLP_STE_MAPPED_NODE) ||
		    (ndlp->nlp_type & NLP_FABRIC))
			continue;

		if (entry >= numberOfEntries)
			goto skip_entry;

		np->type = 0;
		node_bind_type = fc_host_tgtid_bind_type(shost);
		switch (node_bind_type) {
		case FC_TGTID_BIND_BY_WWPN:
			np->type |= NODE_INFO_TYPE_WWPN;
			break;
		case FC_TGTID_BIND_BY_WWNN: 
			np->type |= NODE_INFO_TYPE_WWNN;
			break;
		case FC_TGTID_BIND_BY_ID:
			np->type |= NODE_INFO_TYPE_DID;
			break;
		default:
			np->type |= NODE_INFO_TYPE_AUTOMAP;
			break;
		}
#ifdef CONFIG_COMPAT
		if (cip->lpfc_cntl == LPFC_CNTL_X86_APP) {
			spin_unlock_irq(shost->host_lock);
			copy_to_user(app_ptr, &np->type, sizeof(uint32_t));
			spin_lock_irq(shost->host_lock);
			app_ptr += sizeof(uint32_t);
		}
#endif

#ifdef CONFIG_COMPAT
		if (cip->lpfc_cntl == LPFC_CNTL_X86_APP)
			app_ptr += 256;
#endif

		np->scsiId.ScsiBusNumber = 0;
#ifdef CONFIG_COMPAT
		if (cip->lpfc_cntl == LPFC_CNTL_X86_APP) {
			spin_unlock_irq(shost->host_lock);
			copy_to_user(app_ptr, &np->scsiId.ScsiBusNumber,
				     sizeof(HBA_UINT32));
			spin_lock_irq(shost->host_lock);
			app_ptr += sizeof(HBA_UINT32);
		}
#endif

		np->scsiId.ScsiTargetNumber = ndlp->nlp_sid;
#ifdef CONFIG_COMPAT
		if (cip->lpfc_cntl == LPFC_CNTL_X86_APP) {
			spin_unlock_irq(shost->host_lock);
			copy_to_user(app_ptr, &np->scsiId.ScsiTargetNumber,
				     sizeof(HBA_UINT32));
			spin_lock_irq(shost->host_lock);
			app_ptr += sizeof(HBA_UINT32);
		}
#endif


		np->scsiId.ScsiOSLun = 0;
#ifdef CONFIG_COMPAT
		if (cip->lpfc_cntl == LPFC_CNTL_X86_APP) {
			spin_unlock_irq(shost->host_lock);
			copy_to_user(app_ptr, &np->scsiId.ScsiOSLun,
				     sizeof(HBA_UINT32));
			spin_lock_irq(shost->host_lock);
			app_ptr += sizeof(HBA_UINT32);
		}
#endif

		np->fcpId.FcId = ndlp->nlp_DID;
#ifdef CONFIG_COMPAT
		if (cip->lpfc_cntl == LPFC_CNTL_X86_APP) {
			spin_unlock_irq(shost->host_lock);
			copy_to_user(app_ptr, &np->fcpId.FcId,
				     sizeof(HBA_UINT32));
			spin_lock_irq(shost->host_lock);
			app_ptr += sizeof(HBA_UINT32);
		}
#endif

		memcpy(&np->fcpId.NodeWWN, &ndlp->nlp_nodename,
		       sizeof(HBA_WWN));
#ifdef CONFIG_COMPAT
		if (cip->lpfc_cntl == LPFC_CNTL_X86_APP) {
			spin_unlock_irq(shost->host_lock);
			copy_to_user(app_ptr, &np->fcpId.NodeWWN,
				     sizeof(HBA_WWN));
			spin_lock_irq(shost->host_lock);
			app_ptr += sizeof(HBA_WWN);
		}
#endif

		memcpy(&np->fcpId.PortWWN, &ndlp->nlp_portname,
		       sizeof(HBA_WWN));
#ifdef CONFIG_COMPAT
		if (cip->lpfc_cntl == LPFC_CNTL_X86_APP) {
			spin_unlock_irq(shost->host_lock);
			copy_to_user(app_ptr, &np->fcpId.PortWWN,
				     sizeof(HBA_WWN));
			spin_lock_irq(shost->host_lock);
			app_ptr += sizeof(HBA_WWN);
		}
#endif

		np->fcpId.FcpLun = 0;
#ifdef CONFIG_COMPAT
		if (cip->lpfc_cntl == LPFC_CNTL_X86_APP) {
			spin_unlock_irq(shost->host_lock);
			copy_to_user(app_ptr, &np->fcpId.FcpLun,
				     sizeof(HBA_UINT64));
			spin_lock_irq(shost->host_lock);
			app_ptr += sizeof(HBA_UINT64);
		}
#endif
		
		switch (ndlp->nlp_state) {
		case NLP_STE_MAPPED_NODE:
			np->nodeState = NODE_INFO_STATE_EXIST;
			break;
		case NLP_STE_UNMAPPED_NODE:
			np->nodeState = NODE_INFO_STATE_EXIST |
					NODE_INFO_STATE_UNMAPPED;
			break;
		default:
			break;
		}

		switch (vport->port_state) {
       		case FC_VPORT_LINKDOWN:
			np->nodeState |= NODE_INFO_STATE_LINKDOWN;
			break;
		case FC_VPORT_ACTIVE:
			np->nodeState |= NODE_INFO_STATE_READY;
			break;
		default:
			np->nodeState |= NODE_INFO_STATE_EXIST;
		}

#ifdef CONFIG_COMPAT
		if (cip->lpfc_cntl == LPFC_CNTL_X86_APP) {
			spin_unlock_irq(shost->host_lock);
			copy_to_user(app_ptr, &np->nodeState, sizeof(uint32_t));
			spin_lock_irq(shost->host_lock);
			app_ptr += sizeof(uint32_t);

			/* Skip over the reserved field. */
			app_ptr += sizeof(uint32_t);
		}
#endif

 skip_entry:
		np++;
		entry++;
	}
	spin_unlock_irq(shost->host_lock);
	rc = 0;
	if (entry > numberOfEntries)
		rc = E2BIG;

	/*
	 * The scsi_host_put call is only valid if the driver matched the
	 * vport wwpn.  An error path jump would not have found the match.
	 */
	scsi_host_put(shost);
 exit:
	vplist->numberOfEntries = entry;
	cip->lpfc_outsz = min_value((uint32_t)(entry * sizeof(DFC_NodeInfoEntry)
				     + sizeof(uint32_t)), cip->lpfc_outsz);

#ifdef CONFIG_COMPAT
	if (cip->lpfc_cntl == LPFC_CNTL_X86_APP) {
		cip->lpfc_outsz = 0;
		app_ptr = (uint8_t *) cip->lpfc_dataout;
		copy_to_user(app_ptr, &vplist->numberOfEntries, sizeof(uint32_t));
	}
#endif

	return rc;
}

static int
lpfc_ioctl_issue_dump_mbox(struct lpfc_hba *phba, struct lpfcCmdInput *cip,
			   void *dataout)
{
	int      rc, retval;
	uint32_t retlen, retsts;
	uint32_t regionId, regionLen;
	uint32_t mbox_tmo;
	LPFC_MBOXQ_t       *mboxq = NULL;
	MAILBOX_t          *mb  = NULL;
	struct lpfc_dmabuf *mp  = NULL;
	struct lpfc_mqe    *mqe = NULL;

	retval = 0;

	if (phba->sli.sli_flag & LPFC_BLOCK_MGMT_IO)
		return EAGAIN;

	/* Make certain this is SLI4 hardware */
	if (!(phba->sli_rev == LPFC_SLI_REV4))
		return ENOSYS;

	regionId  = cip->lpfc_arg4;
	regionLen = cip->lpfc_outsz;

	mboxq = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (mboxq == NULL)
		return ENOMEM;

	(void) memset(mboxq, 0, sizeof(LPFC_MBOXQ_t));

	mb  = &mboxq->u.mb;
	mqe = &mboxq->u.mqe;

	mp = kmalloc(sizeof(struct lpfc_dmabuf), GFP_KERNEL);
	if (mp == NULL) {
		mempool_free(mboxq, phba->mbox_mem_pool);
		return ENOMEM;
	}

	mp->virt = lpfc_mbuf_alloc(phba, 0, &mp->phys);
	if (mp->virt == NULL)  {
		kfree(mp);
		mempool_free(mboxq, phba->mbox_mem_pool);
		return ENOMEM;
	}

	(void) memset(mp->virt, 0, LPFC_BPL_SIZE);
	INIT_LIST_HEAD(&mp->list);

	mb->mbxCommand = MBX_DUMP_MEMORY;
	mb->un.varDmp.type = DMP_NV_PARAMS;

	/* type dump word in arg5 */
	mb->un.varWords[0] |= cip->lpfc_arg5 << 24;
	mb->un.varDmp.region_id = regionId;
	mb->un.varDmp.sli4_length = regionLen;
	mb->un.varWords[3] = putPaddrLow(mp->phys);
	mb->un.varWords[4] = putPaddrHigh(mp->phys);
	mbox_tmo = lpfc_mbox_tmo_val(phba, MBX_DUMP_MEMORY);
	rc = lpfc_sli_issue_mbox_wait(phba, mboxq, mbox_tmo);
	if (rc) {
		retval = EIO;
		goto DONE;
	}

	retsts = bf_get(lpfc_mqe_status, mqe);
	retlen = mqe->un.mb_words[5];

	if (retsts != 0) {
		retval = EIO;
		/* Inform user of mailbox status */
		(void) copy_to_user(cip->lpfc_arg2, &retsts, sizeof(uint32_t));
		goto DONE;
	}

	/* Tell user how much data they will be receiving */
	if (copy_to_user(cip->lpfc_arg1, &retlen, sizeof(uint32_t))) {
		retval = EIO;
		goto DONE;
	}

	(void) memcpy(dataout, mp->virt, retlen);
	cip->lpfc_outsz = retlen;
	retval = 0;
DONE:
	lpfc_mbuf_free(phba, mp->virt, mp->phys);
	kfree(mp);
	mempool_free(mboxq, phba->mbox_mem_pool);
	return retval;
}

static int
lpfc_ioctl_issue_update_mbox(struct lpfc_hba *phba, struct lpfcCmdInput *cip,
			     void *dataout)
{
	int      rc, retval;
	uint32_t retsts;
	uint32_t regionId, regionLen;
	uint32_t mbox_tmo;
	LPFC_MBOXQ_t       *mboxq  = NULL;
	MAILBOX_t          *mb   = NULL;
	struct lpfc_dmabuf *mp   = NULL;
	struct lpfc_mqe    *mqe  = NULL;
	struct ulp_bde64   *sp64 = NULL;

	retval = 0;
	cip->lpfc_outsz = 0;
	regionId  = cip->lpfc_arg4;
	regionLen = cip->lpfc_arg5;

	if (phba->sli.sli_flag & LPFC_BLOCK_MGMT_IO)
		return EAGAIN;

	/* Make certain this is SLI4 hardware */
	if (!(phba->sli_rev == LPFC_SLI_REV4))
		return ENOSYS;

	if ((regionId == 17) && (adapter_is_blade_engine(phba)))
		return ENOSYS;

	mboxq = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (mboxq == NULL)
		return ENOMEM;
	(void) memset(mboxq, 0, sizeof(LPFC_MBOXQ_t));

	mb  = &mboxq->u.mb;
	mqe = &mboxq->u.mqe;

	mp = kmalloc(sizeof(struct lpfc_dmabuf), GFP_KERNEL);
	if (mp == NULL) {
		mempool_free(mboxq, phba->mbox_mem_pool);
		return ENOMEM;
	}

	mp->virt = lpfc_mbuf_alloc(phba, 0, &mp->phys);
	if (mp->virt == NULL)  {
		kfree(mp);
		mempool_free(mboxq, phba->mbox_mem_pool);
		return ENOMEM;
	}

	(void) memset(mp->virt, 0, LPFC_BPL_SIZE);
	INIT_LIST_HEAD(&mp->list);
	if (copy_from_user(mp->virt, cip->lpfc_dataout, cip->lpfc_arg5)) {
		retval = EIO;
		goto DONE;
	}

	mb->mbxCommand = MBX_UPDATE_CFG;
	mb->un.varWords[0]  = UPDATE_DATA;
	mb->un.varWords[0] |= 1 << 5; /* DI bit */
	mb->un.varWords[0] |= 1 << 4; /* V  bit */
	mb->un.varWords[1] = (regionId | (regionLen << 16));
	mb->un.varWords[3] = regionLen;

	sp64 = (struct ulp_bde64 *)&(mb->un.varWords[4]);
	sp64->tus.f.bdeSize = regionLen;
	sp64->addrHigh = putPaddrHigh(mp->phys);
	sp64->addrLow  = putPaddrLow(mp->phys);
	mbox_tmo = lpfc_mbox_tmo_val(phba, MBX_UPDATE_CFG);
	rc = lpfc_sli_issue_mbox_wait(phba, mboxq, 15);
	if (rc) {
		retval = EIO;
		goto DONE;
	}

	retsts = bf_get(lpfc_mqe_status, mqe);
	if (copy_to_user(cip->lpfc_arg1, &retsts, sizeof retsts))
		retval = EIO;
	retval = 0;
DONE:
	lpfc_mbuf_free(phba, mp->virt, mp->phys);
	kfree(mp);
	mempool_free(mboxq, phba->mbox_mem_pool);
	return retval;
}

static int
lpfc_ioctl_get_fcf_list(struct lpfc_hba *phba, struct lpfcCmdInput *cip,
			void *dataout)
{
	struct fcf_record fcf;
	uint32_t fcf_size;
	uint16_t next_index;
	uint16_t fcf_index;
	uint32_t fcf_active = 0;

	fcf_size = sizeof (struct fcf_record);
	(void) memset(&fcf, 0, fcf_size);

	if (!(adapter_is_blade_engine(phba)))
		return ENOSYS;

	if (cip->lpfc_arg5 < fcf_size)
		return E2BIG;

	fcf_index  = cip->lpfc_arg4;

	/*
	 * Send a request for an FCF record at fcf_index.  The sli4_fcf_record
	 * routine implements a wait strategy for the answer.
	 */
	next_index = lpfc_sli4_fcf_record_get(phba, &fcf, fcf_index);

	/* Return next index to caller */
	if (copy_to_user(cip->lpfc_arg1, &next_index, sizeof next_index))
		return EIO;

	/* Clause sends a one to user  if this FCF record is active */
	if (lpfc_findnode_wwnn(phba->pport, (struct lpfc_name *)&fcf.word5))
		fcf_active = 1;
	if (copy_to_user(cip->lpfc_arg2, &fcf_active, sizeof fcf_active))
		return EIO;

	cip->lpfc_outsz = fcf_size;
	(void) memcpy(dataout, &fcf, fcf_size);
	return 0;
}

static int
lpfc_ioctl_get_logical_link_speed(struct lpfc_hba *phba,
				struct lpfcCmdInput *cip,
				void *dataout)
{
	int rc = 0;
	uint32_t *buf = dataout;
	if (cip->lpfc_outsz < sizeof(phba->sli4_hba.link_state.logical_speed)) {
		cip->lpfc_outsz = 0;
		rc = ERANGE;
	} else {
		*buf = phba->sli4_hba.link_state.logical_speed;
		cip->lpfc_outsz =
		  sizeof(phba->sli4_hba.link_state.logical_speed);
	}
	return rc;
}

static bool
adapter_is_blade_engine(struct lpfc_hba *phba)
{
	bool retval = false;

	switch (phba->pcidev->device) {
	case PCI_DEVICE_ID_TIGERSHARK:
	case PCI_DEVICE_ID_TOMCAT:
		retval = true;
		break;
	default:
		retval = false;
		break;
	}

	return retval;
}

int
dfc_rsp_data_copy_to_buf(struct lpfc_hba * phba, uint8_t * outdataptr,
			 struct lpfc_dmabuf * mlist, uint32_t size)
{
        struct lpfc_dmabufext *mlast = 0;
        int cnt, offset = 0;
	struct list_head head;

        if (!mlist)
                return 0;

	INIT_LIST_HEAD(&head);
        list_add_tail(&head, &mlist->list);

        list_for_each_entry(mlast, &head, dma.list) {
                if (!size)
                        break;
                if (!mlast)
                        break;

                /* We copy chucks of 4K */
                if (size > 4096)
                        cnt = 4096;
                else
                        cnt = size;

                if (outdataptr) {
                        memcpy((uint8_t *) (outdataptr + offset),
			       (uint8_t *) mlast->dma.virt, (ulong) cnt);
                }
                offset += cnt;
                size -= cnt;
        }

	list_del(&head);
        return 0;
}


static int
dfc_rsp_data_copy(struct lpfc_hba * phba,
		  uint8_t * outdataptr, struct lpfc_dmabufext * mlist,
		  uint32_t size)
{
	struct lpfc_dmabufext *mlast = NULL;
	int cnt, offset = 0;
	struct list_head head;

	if (!mlist)
		return 0;

	INIT_LIST_HEAD(&head);
	list_add_tail(&head, &mlist->dma.list);

	cnt = 0;
        list_for_each_entry(mlast, &head, dma.list) {
		if (!size)
			break;

		if (size > BUF_SZ_4K)
			cnt = BUF_SZ_4K;
		else
			cnt = size;

		if (outdataptr) {
			pci_dma_sync_single_for_cpu(phba->pcidev,
			    mlast->dma.phys, LPFC_BPL_SIZE, PCI_DMA_FROMDEVICE);

			/* Copy data to user space */
			if (copy_to_user((void __user *) (outdataptr + offset),
					 (uint8_t *) mlast->dma.virt, cnt))
				return 1;
		}
		offset += cnt;
		size -= cnt;
	}

	list_del(&head);
	return 0;
}

static int
lpfc_issue_ct_rsp(struct lpfc_hba * phba, uint32_t tag,
		  struct lpfc_dmabuf * bmp,
		  struct lpfc_dmabufext * inp)
{
	struct lpfc_sli *psli;
	IOCB_t *icmd;
	struct lpfc_iocbq *ctiocb = NULL;
	struct lpfc_sli_ring *pring;
	uint32_t num_entry;
	int rc = 0;
	struct lpfc_nodelist *ndlp = NULL;
	struct lpfcdfc_host *dfchba = lpfcdfc_host_from_hba(phba);

	if (phba->sli.sli_flag & LPFC_BLOCK_MGMT_IO)
		return EAGAIN;

	psli = &phba->sli;
	pring = &psli->ring[LPFC_ELS_RING];
	num_entry = inp->flag;
	inp->flag = 0;

	/* Allocate buffer for  command iocb */
	ctiocb = lpfc_sli_get_iocbq(phba);
	if (!ctiocb) {
		rc = ENOMEM;
		goto issue_ct_rsp_exit;
	}
	icmd = &ctiocb->iocb;
	icmd->un.xseq64.bdl.ulpIoTag32 = 0;
	icmd->un.xseq64.bdl.addrHigh = putPaddrHigh(bmp->phys);
	icmd->un.xseq64.bdl.addrLow = putPaddrLow(bmp->phys);
	icmd->un.xseq64.bdl.bdeFlags = BUFF_TYPE_BLP_64;
	icmd->un.xseq64.bdl.bdeSize = (num_entry * sizeof (struct ulp_bde64));
	icmd->un.xseq64.w5.hcsw.Fctl = (LS | LA);
	icmd->un.xseq64.w5.hcsw.Dfctl = 0;
	icmd->un.xseq64.w5.hcsw.Rctl = FC_RCTL_DD_SOL_CTL;
	icmd->un.xseq64.w5.hcsw.Type = FC_TYPE_CT;

	pci_dma_sync_single_for_device(phba->pcidev, bmp->phys, LPFC_BPL_SIZE,
							PCI_DMA_TODEVICE);

	/* Fill in rest of iocb */
	icmd->ulpCommand = CMD_XMIT_SEQUENCE64_CX;
	icmd->ulpBdeCount = 1;
	icmd->ulpLe = 1;
	icmd->ulpClass = CLASS3;
	if (phba->sli_rev == LPFC_SLI_REV4) {
		/* Do not issue unsol response if oxid not marked as valid */
		if (!(dfchba->ct_ctx[tag].flags & UNSOL_VALD)) {
			rc = IOCB_ERROR;
			goto issue_ct_rsp_exit;
		}
		icmd->ulpContext = dfchba->ct_ctx[tag].oxid;
		ndlp = lpfc_findnode_did(phba->pport, dfchba->ct_ctx[tag].SID);
		if (!ndlp) {
			lpfc_printf_log(phba, KERN_WARNING, LOG_EVENT,
				 "2721 ndlp null for oxid %x SID %x\n",
					icmd->ulpContext,
					dfchba->ct_ctx[tag].SID);
			rc = IOCB_ERROR;
			goto issue_ct_rsp_exit;
		}

		/* Make sure the login is valid*/
		if (!(lpfc_is_unmapped_node_logged_in(phba->pport,
						ndlp->nlp_DID))) {
			rc = ENODEV;
			goto issue_ct_rsp_exit;
		}
		icmd->un.ulpWord[3] = ndlp->nlp_rpi;
		/* The exchange is done, mark the entry as invalid */
		dfchba->ct_ctx[tag].flags &= ~UNSOL_VALD;
	} else
		icmd->ulpContext = (ushort) tag;

	icmd->ulpTimeout = phba->fc_ratov * 2;

	/* Xmit CT response on exchange <xid> */
	lpfc_printf_log(phba, KERN_INFO, LOG_EVENT,
			"2722 Xmit CT response on exchange x%x Data: x%x x%x\n",
			icmd->ulpContext, icmd->ulpIoTag, phba->link_state);

	ctiocb->iocb_cmpl = NULL;
	ctiocb->iocb_flag |= LPFC_IO_LIBDFC;
	ctiocb->vport = phba->pport;
	ctiocb->context3 = bmp;
	ctiocb->context1 = ndlp;
	rc = lpfc_sli_issue_iocb_wait(phba, LPFC_ELS_RING, ctiocb, NULL,
				     phba->fc_ratov * 2 + LPFC_DRVR_TIMEOUT);
	if (rc == IOCB_TIMEDOUT) {
		bool timedout = true;
		IOCB_TIMEOUT_T args;

		(void) memset(&args, 0, sizeof(IOCB_TIMEOUT_T));

		args.phba     = phba;
		args.cmdiocbq = ctiocb;
		args.db0      = bmp;
		args.dbext0   = inp;

		timedout = lpfc_process_iocb_timeout(&args);

		if (timedout)
			return ETIMEDOUT;
		else
			rc = IOCB_SUCCESS;
 	}

	if (rc == IOCB_BUSY) {
		rc = EAGAIN;
		goto issue_ct_rsp_exit;
	}

	/* Calling routine takes care of IOCB_ERROR => EIO translation */
	if (rc != IOCB_SUCCESS)
		rc = IOCB_ERROR;

issue_ct_rsp_exit:
	if (ctiocb)
		lpfc_sli_release_iocbq(phba, ctiocb);
	return rc;
}

/* For some reclaim functions phba is not needed. It is passed to this
 * logic in case we want to enable logging in these functions. All
 * reclaim functions return the number of bytes reclaimed in case we
 * want to enable statistics gathering.
 */
static int
lpfc_evt_reclaim(struct lpfc_hba *phba, struct lpfcdfc_event *evt)
{
	int bytes = 0;

	if (evt != NULL) {
		if (evt->evtData.data != NULL) {
			bytes += evt->evtData.len;
			kfree(evt->evtData.data);
		}
		kfree(evt);
		bytes += sizeof(struct lpfcdfc_event);
	}

	return bytes;
}

static int
lpfc_mbuf_reclaim(struct lpfc_hba *phba, struct lpfc_dmabuf *mbuf)
{
	int bytes = 0;

	if (mbuf != NULL) {
		if (mbuf->virt != NULL) {
			lpfc_mbuf_free(phba, mbuf->virt, mbuf->phys);
			mbuf->virt = NULL;
			mbuf->phys = 0;
			bytes += LPFC_BPL_SIZE;
		}

		kfree(mbuf);
		bytes += sizeof(struct lpfc_dmabuf);
	}

	return bytes;
}

static int
lpfc_dmabuf_reclaim(struct lpfc_hba *phba, struct lpfc_dmabuf *db)
{
	int bytes = 0;
	struct pci_dev *pcidev = NULL;
	struct lpfc_dmabufext *mp = NULL;

	pcidev = phba->pcidev;
	if (db != NULL) {
		mp = container_of(db, struct lpfc_dmabufext, dma);
		bytes += db->bsize;
		dma_free_coherent(&pcidev->dev, db->bsize, db->virt, db->phys);
		kfree(mp);
		bytes += sizeof(struct lpfc_dmabufext);
	}

	lpfc_printf_log(phba, KERN_INFO, LOG_EVENT,
		"1912 Reclaimed dmabuf resource %d bytes\n", bytes);

	return bytes;
}

static bool
lpfcProcessSLI2Event(struct lpfc_hba      *phba, 
		     struct lpfcdfc_host  *dfchba,
		     struct lpfc_sli_ring *pring,
		     struct lpfc_iocbq    *piocbq)
{
	struct lpfcdfc_event *evt = NULL;
	struct ulp_bde64 *bde = NULL;
	struct lpfc_dmabuf *dmaBfr = NULL;
	struct lpfc_iocbq *iocbq = NULL;
	dma_addr_t dmaAdr;
	uint32_t fsType = 0, bytesCopied = 0;
	uint8_t k = 0, *dPtr = NULL, *kmem = NULL;
	size_t kmemSize = 0;
	bool resourceError = false;
	iocbq = piocbq;

	do {
		for (k = 0; k < iocbq->iocb.ulpBdeCount; k++) {
			register int blen = 0;
			blen = iocbq->iocb.un.cont64[k].tus.f.bdeSize;
			kmemSize += blen;
		}

		iocbq = (struct lpfc_iocbq *) iocbq->list.next;
	} while (iocbq != piocbq);

	if (kmemSize) {
		kmem = kzalloc(kmemSize, GFP_KERNEL);
		dPtr = kmem;
	}

	if (kmem == NULL) {
		/*
 		 * Kernel memory is not available. We still need
 		 * to process the iocbq or things get bad quickly.
 		 * and eventually PSOD. -DrW
 		 */
		resourceError = true;
		lpfc_printf_log(phba, KERN_WARNING, LOG_EVENT,
			"1940 Dropping SLI2 CT event ENOMEM requested [%d] bytes\n",(int)kmemSize);
	}

	iocbq = piocbq;
LOOP:
	for (k = 0; ((k < iocbq->iocb.ulpBdeCount) && (k < 2)); k++) {
		register int blen;

		blen   = iocbq->iocb.un.cont64[k].tus.f.bdeSize;
		bde    = &iocbq->iocb.un.cont64[k];
		dmaAdr = getPaddr(bde->addrHigh, bde->addrLow);
		dmaBfr = lpfc_sli_ringpostbuf_get(phba, pring, dmaAdr);

		if (dmaBfr == NULL) {
			lpfc_printf_log(phba, KERN_WARNING, LOG_EVENT,
				"1958 SLI2 DMA resource not available\n");
			continue;
		}

		fsType = ((struct lpfc_sli_ct_request *) (dmaBfr->virt))->FsType;

		if ((!resourceError) && (kmemSize)) {
			(void) memcpy(dPtr, dmaBfr->virt, blen);
			bytesCopied += blen;
			dPtr        += blen;
		}

		if (fsType == SLI_CT_ELX_LOOPBACK)
			lpfc_dmabuf_reclaim(phba,dmaBfr);
		else    lpfc_sli_ringpostbuf_put(phba, pring, dmaBfr);

	}/*END  for (k = 0; k < iocbq->iocb.ulpBdeCount; k++) */

	iocbq = (struct lpfc_iocbq *) iocbq->list.next;

	if (iocbq != piocbq)
		goto LOOP;

	if (resourceError)
		goto DONE;

	evt = lpfcdfc_event_new(FC_REG_CT_EVENT, 1313, fsType);

	if (evt == NULL) {
		kfree(kmem);
		lpfc_printf_log(phba, KERN_WARNING, LOG_EVENT,
			"1945 Dropping SLI2 CT event ENOMEM\n");
		goto DONE;
	}

	evt->evtData.type.mask = FC_REG_CT_EVENT;
	evt->evtData.immed_dat = piocbq->iocb.ulpContext;
	evt->evtData.len       = bytesCopied;
	evt->evtData.data      = kmem;

	lpfc_printf_log(phba, KERN_WARNING, LOG_EVENT,
		"1954 SLI2Event bytes copied [%d]\n", bytesCopied);

	(void) lpfc_ioctl_queue_event(dfchba, evt, fsType);

	if (fsType == SLI_CT_ELX_LOOPBACK)
		wake_up(&dfchba->waitQ);
DONE:
	return((fsType == SLI_CT_ELX_LOOPBACK) ? false : true);
}

/* Returns count of events added to unsolicited or diagnostic event queue */
static int
lpfc_ioctl_queue_event(struct lpfcdfc_host  *dfchba,
		       struct lpfcdfc_event *evt,
		       uint32_t fsType)
{
	register uint8_t qDepth = 0;
	bool qEvents = false;
	struct lpfc_hba *phba = NULL;
	uint8_t queued = 0, type = 0;

	mutex_lock(&lpfcdfc_lock);
	phba = dfchba->phba;
	if (fsType == SLI_CT_ELX_LOOPBACK) {
		list_add_tail(&evt->node, &dfchba->diagQ);
		queued++;
	}
	else {
		qEvents = dfchba->qEvents;
		qDepth  = dfchba->qCount;
	}
	mutex_unlock(&lpfcdfc_lock);

        type = (phba->sli3_options & LPFC_SLI3_HBQ_ENABLED) ? 1 : 0;

	if (fsType == SLI_CT_ELX_LOOPBACK) {
		lpfc_printf_log(phba, KERN_INFO, LOG_EVENT,
			"1947 Queuing LOOPBACK event SLI%d\n",
			phba->sli_rev);
		goto DONE;
	}

	if (!qEvents) {
		lpfc_printf_log(phba, KERN_INFO, LOG_EVENT,
			"1956 Queuing CT events has not been enabled SLI%d\n",
			phba->sli_rev);
		goto DONE;
	}

	if (qDepth >= LPFC_IOCTL_MAX_EVENTS) {
		lpfc_printf_log(phba, KERN_INFO, LOG_EVENT,
				"1943 WARNING CT event Queue full SLI%d\n",
				phba->sli_rev);
		goto DONE;
	}

	lpfc_printf_log(phba, KERN_INFO, LOG_EVENT,
			"1942 Queuing CT event SLI%d\n",
			phba->sli_rev);

	mutex_lock(&lpfcdfc_lock);
	list_add_tail(&evt->node, &dfchba->evtQueue);
	(dfchba->qCount)++;
	mutex_unlock(&lpfcdfc_lock);

	queued++;

DONE:
	if (queued == 0)
		(void) lpfc_evt_reclaim(phba, evt);

	return queued;
}

static bool
lpfcProcessSLI34Event(struct lpfc_hba *phba, struct lpfcdfc_host *dfchba,
			struct lpfc_sli_ring *pring, struct lpfc_iocbq *piocbq)
{
	struct lpfcdfc_event *evt     = NULL;
	struct event_data    *evtData = NULL;
	struct lpfc_iocbq    *iocbq   = NULL;
	struct lpfc_dmabuf   *bdeBfr  = NULL;

	uint32_t fsType = 0, cmdRsp = 0;
	uint32_t bytesToCopy = 0, bytesCopied = 0;
	uint8_t k = 0, *dPtr = NULL, *kPtr = NULL;

	bool resourceError = false;

	/*
	 * The driver has at least one IOCB. Process that one	
 	 * and then process its list if one exists
 	 */ 
	bdeBfr = (struct lpfc_dmabuf *) piocbq->context2;

        if (bdeBfr == NULL) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_EVENT,
			"1938 Dropping SLI3 CT event\n");
		return true;
	}

	fsType = ((struct lpfc_sli_ct_request *)
			(bdeBfr->virt))->FsType;
	cmdRsp = ((struct lpfc_sli_ct_request *)
			(bdeBfr->virt))->CommandResponse.bits.CmdRsp;

	/* For SLI3 the last iocb contains the accumulated byte count */
	iocbq = list_entry(piocbq->list.prev, struct lpfc_iocbq, list);
	bytesToCopy = iocbq->iocb.unsli3.rcvsli3.acc_len;
	if (bytesToCopy) {
		kPtr = kmalloc(bytesToCopy, GFP_KERNEL);
		dPtr = kPtr;
	}
	if (kPtr == NULL) {
		/*
 		 * Kernel memory is not available. We still need
 		 * to process the iocbq or things get bad quickly.
 		 * and eventually PSOD.
 		 */
		resourceError = true;
		lpfc_printf_log(phba, KERN_WARNING, LOG_EVENT,
			"1941 Dropping SLI3 CT event ENOMEM requested "
			"[%d] bytes\n",(int)bytesToCopy);
		return true;
	}

	iocbq = piocbq;
	do {
		for (k = 0; ((k < iocbq->iocb.ulpBdeCount) && (k < 2)); k++) {
			register int bLen, bMove;
			register IOCB_t *iocb;

			iocb = &iocbq->iocb;

			if (iocb->ulpBdeCount == 0) continue;
			bdeBfr = (k == 0) ? iocbq->context2 : iocbq->context3;
	
			if (bdeBfr == NULL) {
				lpfc_printf_log(phba, KERN_WARNING, LOG_EVENT,
					"1959 SLI3 DMA resource not available\n");
				continue;
			}

			if (k == 0)
				bLen = iocb->un.cont64[0].tus.f.bdeSize;
			else
				bLen = iocb->unsli3.rcvsli3.bde2.tus.f.bdeSize;
		
			bMove = (bytesToCopy <= bLen) ? bytesToCopy : bLen;

			if ((!resourceError) && (bMove)) {
				(void) memcpy(dPtr, bdeBfr->virt, bMove);
				dPtr += bMove;
			}
	
			bytesCopied += bMove;
			bytesToCopy -= bMove;
			lpfc_in_buf_free(phba, bdeBfr);
		}/* END  for (k = 0; ((k < iocbq->iocb.ulpBdeCount) && (k < 2)); k++) */

		if (iocbq->context2) iocbq->context2 = NULL;
		if (iocbq->context3) iocbq->context3 = NULL;

		evt = lpfcdfc_event_new(FC_REG_CT_EVENT, 1313, fsType);

		if (evt == NULL) {
			kfree(kPtr);
			lpfc_printf_log(phba, KERN_WARNING, LOG_EVENT,
					"1939 Dropping SLI3 CT event ENOMEM\n");
			goto DONE;
		}

		evtData = &evt->evtData;
		evtData->type.mask = FC_REG_CT_EVENT;

		if (phba->sli_rev == LPFC_SLI_REV4) {
			evtData->immed_dat = dfchba->ctx_idx;
			dfchba->ctx_idx = (dfchba->ctx_idx + 1) % CT_CTX_SIZE;
			/* Provide warning for over-run of the ct_ctx array */
			if (dfchba->ct_ctx[evtData->immed_dat].flags &
			    UNSOL_VALD)
				lpfc_printf_log(phba, KERN_WARNING, LOG_EVENT,
						"2717 CT context array entry "
						"[%d] over-run: oxid:x%x, "
						"sid:x%x\n", dfchba->ctx_idx,
						dfchba->ct_ctx[
						    evtData->immed_dat].oxid,
						dfchba->ct_ctx[
						    evtData->immed_dat].SID);
			dfchba->ct_ctx[evtData->immed_dat].oxid =
					piocbq->iocb.ulpContext;
			dfchba->ct_ctx[evtData->immed_dat].SID =
					piocbq->iocb.un.rcvels.remoteID;
			/* Setting valid bit while clear up all other bits */
			dfchba->ct_ctx[evtData->immed_dat].flags = UNSOL_VALD;
		} else
			evtData->immed_dat = piocbq->iocb.ulpContext;

		iocbq = (struct lpfc_iocbq *) iocbq->list.next;

	} while ((bytesToCopy) && (iocbq != piocbq));

	if (resourceError)
		goto DONE;

	evtData->len = bytesCopied;
	evtData->data = kPtr;

	lpfc_printf_log(phba, KERN_WARNING, LOG_EVENT,
		"1955 SLI3Event bytes copied [%d]\n", bytesCopied);

	(void) lpfc_ioctl_queue_event(dfchba, evt, fsType);

	if (fsType == SLI_CT_ELX_LOOPBACK)
		wake_up(&dfchba->waitQ);
DONE:
	return true;
}

/*&&&PAE.  Need to look at this routine.  In Linux, this grew substantially. */
static void
lpfcdfc_ct_unsol_event(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
		       struct lpfc_iocbq *piocbq)
{
	struct lpfcdfc_host *dfchba = NULL;
	bool sli34Enabled = false;
	bool callback = false;

	dfchba = lpfcdfc_host_from_hba(phba);
	if (dfchba == NULL) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_EVENT,
			"1911 Exec Format Error, Dropping CT event\n");
		goto error_unsol_ct_exit;
	}

	if ((phba->link_state == LPFC_HBA_ERROR) ||
	    (!(phba->sli.sli_flag & LPFC_SLI_ACTIVE)))
		goto error_unsol_ct_exit;

	if (piocbq == NULL)
		goto error_unsol_ct_exit;

	/*
	 * For SLI3 and 4 the driver must call the callback function. In SLI2
	 * the driver does not call the callback function in loopback mode.
	 */
	sli34Enabled = ((phba->sli_rev == LPFC_SLI_REV4) ||
			(phba->sli3_options & LPFC_SLI3_HBQ_ENABLED));
	if (sli34Enabled) {
		(void) lpfcProcessSLI34Event(phba, dfchba, pring, piocbq);
		callback = true;
	}
	else
		callback = lpfcProcessSLI2Event(phba, dfchba, pring, piocbq);

 error_unsol_ct_exit:
	if ((dfchba->base_ct_unsol_event != NULL) && (callback))
		(dfchba->base_ct_unsol_event)(phba, pring, piocbq);
}

static void
lpfcdfc_sli4_ct_unsol_abort(struct lpfc_hba *phba,
                       struct lpfc_sli_ring *pring,
                       struct lpfc_iocbq *piocbq)
{
        struct lpfcdfc_host *dfchba;
        int idx;

        dfchba = lpfcdfc_host_from_hba(phba);
        BUG_ON(&dfchba->node == &lpfcdfc_hosts);

        if (piocbq->iocb.ulpBdeCount == 0 ||
            piocbq->iocb.un.cont64[0].tus.f.bdeSize == 0)
                goto unsol_ct_abort_exit;

        if (phba->link_state == LPFC_HBA_ERROR ||
            (!(phba->sli.sli_flag & LPFC_SLI_ACTIVE)))
                goto unsol_ct_abort_exit;

        mutex_lock(&lpfcdfc_lock);
        for (idx = 0; idx < CT_CTX_SIZE; idx++) {
                if (!(dfchba->ct_ctx[idx].flags & UNSOL_VALD))
                        continue;
                if (dfchba->ct_ctx[idx].oxid != piocbq->iocb.ulpContext)
                        continue;
                if (dfchba->ct_ctx[idx].SID != piocbq->iocb.un.rcvels.remoteID)
                        continue;
                /* Mark the entry aborted as invalid */
                dfchba->ct_ctx[idx].flags &= ~UNSOL_VALD;
                break;
        }
        mutex_unlock(&lpfcdfc_lock);

unsol_ct_abort_exit:
        if (dfchba->base_ct_unsol_abort)
                (dfchba->base_ct_unsol_abort)(phba, pring, piocbq);
        return;
}

/**
 * __dfc_cmd_data_alloc - Alloc and init BDEs for an ELS/CT IOCTL IO.
 * @phba: The driver instance to execute this call.
 * @indataptr: A pointer to the command data for this IO.
 * @bpl: A pointer to the starting BLP address.
 * @size: The number of bytes described in this IO.
 * @nocopydata: A hint used by menlo only for the indataptr memory.
 *
 * This routine allocates and initializes the BDEs necessary to fully
 * describe the command or response request.  When the caller specifies
 * indataptr, this routine treats it as a command BDE sequence and
 * copies the callers data pointed to in indataptr to the DMA regions
 * allocated.  If indataptr is NULL, this routine treats it as a
 * response buffer and just allocated enough DMA memory to receive the
 * IO request.  The BDEs allocated here are not put into a traditional
 * list head because an IOCB doesn't understand list heads in the list.h
 * API format.
 *
 * Notes:  The nocopydata argument was added for the lpfc_menlo.c module.
 * It is a mechanism that allows the lpfc_menlo.c module to handle the
 * command operations and still allow this routine to complete the IOCB
 * BLP/BDE initialization.  In ESX, the ioctl code doesn't call the
 * lpfc_menlo routines rendering this arument useless.  Need to decide on
 * the IOCTL or lpfc_menlo implementation and remove the other.
 *
 * Returns: A list of BDEs that describe memory regions for data.
 * This is not a list consistent with the list.h macros.
 **/
struct lpfc_dmabufext *
__dfc_cmd_data_alloc(struct lpfc_hba *phba,
		   uint8_t *indataptr, struct ulp_bde64 *bpl, uint32_t size,
		   int nocopydata)
{
	struct lpfc_dmabufext *mlist  = NULL;
	struct lpfc_dmabufext *dmp    = NULL;
	struct pci_dev *pcidev = phba->pcidev;
	int cnt = 0, offset = 0, bdeCnt = 0;

	while (size > 0) {
		/*
	 	 * The driver asks for a page size allocation to increase the
		 * probability that it can allocate the entire application 
		 * buffer size.  Emulex/OEM apps can ask for 64KB+ sizes and
		 * single allocation calls are more likely to fail.
	 	 */
		cnt = ((size > LPFC_IOCTL_PAGESIZE) ? LPFC_IOCTL_PAGESIZE : size);

		dmp = kmalloc(sizeof (struct lpfc_dmabufext), GFP_KERNEL);
		if (dmp == 0)
			goto out;
		INIT_LIST_HEAD(&dmp->dma.list);

		/* Allocate the data buffer */
		dmp->dma.virt = dma_alloc_coherent(&pcidev->dev, cnt,
						   &(dmp->dma.phys),
						   GFP_KERNEL);
		if (dmp->dma.virt == NULL) {
			kfree(dmp);
			goto out;
		}

		dmp->size = dmp->dma.bsize = cnt;

		/*
		 * Command payloads are specified in indataptr.  Response
		 * payloads pass indataptr as NULL.  The nocopydata argument
		 * is used by the menlo module to prevent the command payoad
		 * copy but still complete IOCB initialization.
		 */
		if (indataptr || nocopydata) {
			if (indataptr)
				if (copy_from_user ((uint8_t *) dmp->dma.virt,
					(void __user *) (indataptr + offset),
					cnt)) {
					lpfc_dmabuf_reclaim(phba, &dmp->dma);
					goto out;
				}
			bpl->tus.f.bdeFlags = BUFF_TYPE_BDE_64;
			pci_dma_sync_single_for_device(phba->pcidev,
			        dmp->dma.phys, LPFC_BPL_SIZE, PCI_DMA_TODEVICE);

		} else {
			memset((uint8_t *)dmp->dma.virt, 0, cnt);
			bpl->tus.f.bdeFlags = BUFF_TYPE_BDE_64I;
		}

		/* Build the BPL for the IOCB */
		bpl->addrLow = le32_to_cpu(putPaddrLow(dmp->dma.phys));
		bpl->addrHigh = le32_to_cpu(putPaddrHigh(dmp->dma.phys));
		bpl->tus.f.bdeSize = (ushort) cnt;
		bpl->tus.w = le32_to_cpu(bpl->tus.w);
		bpl++;

		/* Queue it to a linked list */
		if (mlist != NULL)
			list_add_tail(&dmp->dma.list, &mlist->dma.list);
		else
			mlist = dmp;
		bdeCnt += 1;
		offset += cnt;
		size   -= cnt;
	}/*END  while (size > 0) */

	mlist->flag = bdeCnt;
	return mlist;

out:
	/* Error return */
	lpfc_printf_log(phba, KERN_ERR, LOG_EVENT,
		"1934 ENOMEM DMA coherent resource unavailale\n");

	if (mlist != NULL)
		dfc_cmd_data_free(phba, mlist);
	return NULL;
}

static struct lpfc_dmabufext *
dfc_cmd_data_alloc(struct lpfc_hba * phba,
		   uint8_t *indataptr, struct ulp_bde64 * bpl, uint32_t size)
{
	return (__dfc_cmd_data_alloc(phba, indataptr, bpl, size, 0));
}

/**
 * dfc_fcp_cmd_data_alloc - Alloc and init BDEs for an FCP IOCTL IO.
 * @phba: The driver instance to execute this call.
 * @indataptr: A pointer to the command data for this IO.
 * @bpl: A pointer to the starting BLP address.
 * @size: The number of bytes described in this IO.
 *
 * This routine allocates and initializes the BDEs necessary to fully
 * describe the FCP command or response request.  When the caller specifies
 * indataptr, this routine treats it as a command BDE sequence and
 * copies the callers data pointed to in indataptr to the DMA regions
 * allocated.  If indataptr is NULL, this routine treats it as a
 * response buffer and just allocated enough DMA memory to receive the
 * IO request.  Much like the __dfc_cmd_data_alloc routine, the BDEs
 * allocated here are not put into a traditional list head because
 * an IOCB doesn't understand list heads in the list.h API format.
 *
 * Notes: This routine shares similarities to the __dfc_cmd_data_alloc
 * routine, but it is not the same.  This routine preps an FCP IO
 * for both Emulex apps and third party apps sending FCP IO to a 
 * target.  The BLPs for an FCP IO are different than ELS or CT
 * and this routine accounts for those differences.
 *
 * Returns: A list of BDEs that describe memory regions for data.
 * This is not a list consistent with the list.h macros.
 **/
static struct lpfc_dmabufext *
dfc_fcp_cmd_data_alloc(struct lpfc_hba * phba,
		   char *indataptr, struct ulp_bde64 * bpl,
		   uint32_t size, struct lpfc_dmabuf *bmp_list)
{
	struct lpfc_dmabufext *mlist = 0;
	struct lpfc_dmabufext *dmp;
	int cnt, offset = 0, i = 0;
	struct pci_dev *pcidev;
	uint32_t num_bmps, num_bde, max_bde;
	uint32_t top_num_bdes = 0;
	struct lpfc_dmabuf *mlast, *mlast_next;
	struct lpfc_dmabuf *bmp;
	struct ulp_bde64 *topbpl = NULL;
	num_bmps = 1;
	num_bde = 0;

	pcidev = phba->pcidev;

	/*
	 * 3 bdes are reserved for a fcp_command, a fcp_reponse and
	 * a continuation bde.
	 */
	max_bde = (LPFC_BPL_SIZE / sizeof(struct ulp_bde64)) - 3;
	while (size) {
		/*
	 	 * The driver asks for a page size allocation to increase the
		 * probability that it can allocate the entire application 
		 * buffer size.  Emulex/OEM apps can ask for 64KB+ sizes and
		 * single allocation calls are more likely to fail.
	 	 */
		cnt = ((size > LPFC_IOCTL_PAGESIZE) ? LPFC_IOCTL_PAGESIZE : size);

		/*
		 * Consume the BDEs provided in a single mbuf first.  Allocate
		 * more only when needed.
		 */
		if (num_bde == max_bde) {
			bmp = kmalloc(sizeof (struct lpfc_dmabuf), GFP_KERNEL);
			if (bmp == 0)
				goto out;

			memset(bmp, 0, sizeof (struct lpfc_dmabuf));
			bmp->virt = lpfc_mbuf_alloc(phba, 0, &bmp->phys);
			if (!bmp->virt) {
				kfree(bmp);
				goto out;
			}

			max_bde = ((LPFC_BPL_SIZE /
				    sizeof(struct ulp_bde64)) - 3);

			/* Fill in continuation entry to next bpl */
			bpl->addrHigh =
				le32_to_cpu(putPaddrHigh(bmp->phys));
			bpl->addrLow =
				le32_to_cpu(putPaddrLow(bmp->phys));
			bpl->tus.f.bdeFlags = BUFF_TYPE_BLP_64;
			num_bde++;
			if (num_bmps == 1) {
				top_num_bdes = num_bde;
			} else {
				topbpl->tus.f.bdeSize = (num_bde *
					sizeof (struct ulp_bde64));
				topbpl->tus.w =
					le32_to_cpu(topbpl->tus.w);
			}
			topbpl = bpl;
			bpl = (struct ulp_bde64 *) bmp->virt;
			list_add_tail(&bmp->list, &bmp_list->list);
			num_bde = 0;
			num_bmps++;
		}

		/*
		 * Populate this BLP with BDEs until the command/response is
		 * fully described.
		 */
		dmp = kzalloc(sizeof (struct lpfc_dmabufext), GFP_KERNEL);
		if (dmp == NULL)
			goto out;
		INIT_LIST_HEAD(&dmp->dma.list);

		if (mlist)
			list_add_tail(&dmp->dma.list, &mlist->dma.list);
		else
			mlist = dmp;

		/*
		 * Allocate the data buffer.  If the caller has specified
		 * command data, copy in the bytes at offset.
		 */
		dmp->dma.virt = dma_alloc_coherent(&pcidev->dev, cnt,
						     &(dmp->dma.phys),
						     GFP_KERNEL);
		if (dmp->dma.virt == 0)
			goto out;
		dmp->size = cnt;
		if (indataptr) {
			if (copy_from_user((uint8_t *) dmp->dma.virt,
			     (uint8_t *) (indataptr + offset), (ulong) cnt)) {
				goto out;
			}
			bpl->tus.f.bdeFlags = BUFF_TYPE_BDE_64;
		} else {
			memset((uint8_t *)dmp->dma.virt, 0, cnt);
			bpl->tus.f.bdeFlags = BUFF_TYPE_BDE_64I;
		}

		/* Update the IOCB BPL for this BDE. */
		bpl->addrLow = le32_to_cpu(putPaddrLow(dmp->dma.phys));
		bpl->addrHigh = le32_to_cpu(putPaddrHigh(dmp->dma.phys));
		bpl->tus.f.bdeSize = (ushort) cnt;
		bpl->tus.w = le32_to_cpu(bpl->tus.w);
		bpl++;
		num_bde++;
		i++;
		offset += cnt;
		size -= cnt;
	}

	if (num_bmps == 1)
		mlist->flag = i;
	else {
		mlist->flag = top_num_bdes;
		topbpl->tus.f.bdeSize = (num_bde *
			sizeof (struct ulp_bde64));
		topbpl->tus.w =
			le32_to_cpu(topbpl->tus.w);
	}

	return mlist;
out:

	list_for_each_entry_safe(mlast, mlast_next, &bmp_list->list, list) {
		list_del(&mlast->list);
		(void) lpfc_mbuf_reclaim(phba, mlast);
	}

	dfc_cmd_data_free(phba, mlist);
	return 0;
}

int
__dfc_cmd_data_free(struct lpfc_hba * phba, struct lpfc_dmabufext * mlist)
{
	return(dfc_cmd_data_free(phba, mlist));
}

/**
 * dfc_cmd_data_free - Release the BLP and BDE memory assigned to an IOCB.
 * @phba: The driver instance to execute this call.
 * @mlist: The list of BDEs allocated by dfc_cmd_data_alloc or 
 *         dfc_fcp_cmd_data_alloc.
 *
 * This routine releases all memory allocated by dfc_cmd_data_alloc or
 * dfc_fcp_cmd_data_alloc.  Because the allocation routines populate a
 * BPL in the IOCB, there is no list head associated with the memory
 * referenced in @mlist.  This routine does not attempt to rebuild a 
 * list_head consistent with the list.h macros as it is just as easy
 * to traverse the list as is and free the memory elements.
 **/
static int
dfc_cmd_data_free(struct lpfc_hba * phba, struct lpfc_dmabufext * mlist)
{
	struct pci_dev *pcidev = phba->pcidev;
	int bytes = 0;
	struct lpfc_dmabufext *mp = NULL;

	if ((mlist == NULL) || (!lpfc_is_link_up(phba) &&
		(phba->link_flag & LS_LOOPBACK_MODE))) {
		return 0;
	}

	/* Free list members first. */
	while (!(list_empty(&mlist->dma.list))) {
                mp = list_entry(mlist->dma.list.next, struct lpfc_dmabufext, dma.list);
                list_del(&mp->dma.list);
                bytes += mp->dma.bsize;

                dma_free_coherent(&pcidev->dev, mp->dma.bsize, mp->dma.virt, mp->dma.phys);
                bytes += sizeof(struct lpfc_dmabufext);
                kfree(mp);
	}

	/* Release the list head now. */
	dma_free_coherent(&pcidev->dev, mlist->dma.bsize, mlist->dma.virt, mlist->dma.phys);
	kfree(mlist);

	return bytes;
}

static struct lpfcdfc_host *
lpfcdfc_host_from_hba(struct lpfc_hba *phba)
{
	struct lpfcdfc_host * dfchba = NULL;
	struct lpfcdfc_host * tmpPtr = NULL;

	mutex_lock(&lpfcdfc_lock);
	list_for_each_entry(tmpPtr, &lpfcdfc_hosts, node) {
		if (tmpPtr->phba == phba) {
			dfchba = tmpPtr;
			break;
		}
	}
	mutex_unlock(&lpfcdfc_lock);
	return dfchba;
}

struct lpfcdfc_host *
lpfcdfc_host_add(struct pci_dev *dev,
		 struct Scsi_Host *host,
		 struct lpfc_hba *phba)
{
	struct lpfcdfc_host * dfchba = NULL;
	struct lpfc_sli_ring_mask * prt = NULL;

	dfchba = kzalloc(sizeof(*dfchba), GFP_KERNEL);
	if (dfchba == NULL)
		return NULL;

	dfchba->inst    = phba->brd_no;
	dfchba->phba    = phba;
	dfchba->vport   = phba->pport;
	dfchba->host    = host;
	dfchba->dev     = dev;
	dfchba->blocked = 0;
	dfchba->qEvents = false;
	dfchba->qCount  = 0;

	INIT_LIST_HEAD(&dfchba->evtQueue);
	INIT_LIST_HEAD(&dfchba->diagQ);
	init_waitqueue_head(&dfchba->waitQ);

	spin_lock_irq(&phba->hbalock);
	prt = phba->sli.ring[LPFC_ELS_RING].prt;
	dfchba->base_ct_unsol_event = prt[2].lpfc_sli_rcv_unsol_event;
	dfchba->base_ct_unsol_abort = prt[4].lpfc_sli_rcv_unsol_event;
	prt[2].lpfc_sli_rcv_unsol_event = lpfcdfc_ct_unsol_event;
	prt[3].lpfc_sli_rcv_unsol_event = lpfcdfc_ct_unsol_event;
	prt[4].lpfc_sli_rcv_unsol_event = lpfcdfc_sli4_ct_unsol_abort;
	spin_unlock_irq(&phba->hbalock);

	mutex_lock(&lpfcdfc_lock);
	list_add_tail(&dfchba->node, &lpfcdfc_hosts);
	mutex_unlock(&lpfcdfc_lock);

	return dfchba;
}

void
lpfcdfc_host_del(struct lpfcdfc_host *dfchba)
{
	struct Scsi_Host * host = NULL;
	struct lpfc_hba * phba = NULL;
	struct lpfc_sli_ring_mask * prt = NULL;

	mutex_lock(&lpfcdfc_lock);
	dfchba->blocked = 1;

	while (dfchba->ref_count) {
		mutex_unlock(&lpfcdfc_lock);
		msleep(2000);
		mutex_lock(&lpfcdfc_lock);
	}

	if (dfchba->dev->driver) {
		host = pci_get_drvdata(dfchba->dev);
		if ((host != NULL) &&
		    (struct lpfc_vport *)host->hostdata == dfchba->vport) {
			phba = dfchba->phba;
			mutex_unlock(&lpfcdfc_lock);
			spin_lock_irq(&phba->hbalock);
			prt = phba->sli.ring[LPFC_ELS_RING].prt;
			prt[2].lpfc_sli_rcv_unsol_event =
				dfchba->base_ct_unsol_event;
			prt[3].lpfc_sli_rcv_unsol_event =
				dfchba->base_ct_unsol_event;
			spin_unlock_irq(&phba->hbalock);
			mutex_lock(&lpfcdfc_lock);
		}
	}
	list_del_init(&dfchba->node);
	mutex_unlock(&lpfcdfc_lock);
	kfree(dfchba);
}

/**
 * lpfcdfc_get_phba_by_inst - Retrieve the dfchba matching inst
 * @inst - The driver instance (board number) to match.
 *
 * This routine uses @inst as the matching criteria for every known
 * dfchba instance in the global list of lpfcdfc_hosts.
 *
 * Returns:
 *	If found return the dfchba instance.
 * 	NULL otherwise.
 **/
static struct lpfcdfc_host *
lpfcdfc_get_phba_by_inst(int inst)
{
	struct Scsi_Host * host = NULL;
	struct lpfcdfc_host * dfchba;

	mutex_lock(&lpfcdfc_lock);
	list_for_each_entry(dfchba, &lpfcdfc_hosts, node) {
		if (dfchba->inst == inst) {
			if (dfchba->dev->driver) {
				host = pci_get_drvdata(dfchba->dev);
				if ((host != NULL) &&
				    (struct lpfc_vport *)host->hostdata ==
					dfchba->vport) {
					mutex_unlock(&lpfcdfc_lock);
					BUG_ON(dfchba->phba->brd_no != inst);
					return dfchba;
				}
			}
			mutex_unlock(&lpfcdfc_lock);
			return NULL;
		}
	}
	mutex_unlock(&lpfcdfc_lock);
	return NULL;
}


static int
lpfcdfc_do_ioctl(struct lpfc_hba *phba, struct lpfcCmdInput *cip)
{
	int rc = 0;
	uint32_t total_mem = 0;
	void * dataout = NULL;

	if (cip->lpfc_outsz >= BUF_SZ_4K) {
		/*
		 * Allocate memory for ioctl data. If buffer is bigger than 64k,
		 * then we allocate 64k and re-use that buffer over and over to
		 * xfer the whole block. This is because Linux kernel has a
		 * problem allocating more than 120k of kernel space memory. Saw
		 * problem with GET_FCPTARGETMAPPING...
		 */
		if (cip->lpfc_outsz <= (64 * 1024))
			total_mem = cip->lpfc_outsz;
		else
			total_mem = 64 * 1024;
	} else {
		/* Allocate memory for ioctl data */
		total_mem = BUF_SZ_4K;
	}

	/* Don't allocate memory for any of these IOCTLs -dcw */
	switch (cip->lpfc_cmd) {
	case LPFC_HBA_GET_EVENT:
	case LPFC_HBA_SET_EVENT:
	case LPFC_VPORT_SETCFG:
	case LPFC_HBA_UNSET_EVENT:
	case LPFC_LOOPBACK_MODE:
		dataout = NULL;
		break;
	default:
		dataout = kmalloc(total_mem, GFP_KERNEL);
		if (dataout == NULL)
			return ENOMEM;
		break;
	}

	switch (cip->lpfc_cmd) {
	case LPFC_WRITE_PCI:
		rc = lpfc_ioctl_write_pci(phba, cip);
		break;

	case LPFC_READ_PCI:
		rc = lpfc_ioctl_read_pci(phba, cip, dataout);
		break;

	case LPFC_WRITE_MEM:
		rc = lpfc_ioctl_write_mem(phba, cip);
		break;

	case LPFC_READ_MEM:
		rc = lpfc_ioctl_read_mem(phba, cip, dataout);
		break;

	case LPFC_WRITE_CTLREG:
		rc = lpfc_ioctl_write_ctlreg(phba, cip);
		break;

	case LPFC_READ_CTLREG:
		rc = lpfc_ioctl_read_ctlreg(phba, cip, dataout);
		break;

	case LPFC_GET_DFC_REV:
		((struct DfcRevInfo *) dataout)->a_Major = DFC_MAJOR_REV;
		((struct DfcRevInfo *) dataout)->a_Minor = DFC_MINOR_REV;
		cip->lpfc_outsz = sizeof (struct DfcRevInfo);
		rc = 0;
		break;

	case LPFC_INITBRDS:
		rc = lpfc_ioctl_initboard(phba, cip, dataout);
		break;

	case LPFC_SETDIAG:
		rc = lpfc_ioctl_setdiag(phba, cip, dataout);
		break;

	case LPFC_HBA_SEND_SCSI:
	case LPFC_HBA_SEND_FCP:
		rc = lpfc_ioctl_send_scsi_fcp(phba, cip);
		break;

	case LPFC_SEND_ELS:
		rc = lpfc_ioctl_send_els(phba, cip, dataout);
		break;

	case LPFC_HBA_SEND_MGMT_RSP:
		rc = lpfc_ioctl_send_mgmt_rsp(phba, cip);
		break;

	case LPFC_HBA_SEND_MGMT_CMD:
	case LPFC_CT:
		rc = lpfc_ioctl_send_mgmt_cmd(phba, cip, dataout);
		break;

	case LPFC_MENLO:
		rc = lpfc_ioctl_send_menlo_mgmt_cmd(phba, cip, dataout);
		break;

	case LPFC_MBOX:
		rc = lpfc_ioctl_mbox(phba, cip, dataout);
		break;

	case LPFC_LINKINFO:
		rc = lpfc_ioctl_linkinfo(phba, cip, dataout);
		break;

	case LPFC_IOINFO:
		rc = lpfc_ioctl_ioinfo(phba, cip, dataout);
		break;

	case LPFC_NODEINFO:
		rc = lpfc_ioctl_nodeinfo(phba, cip, dataout, total_mem);
		break;

	case LPFC_GETCFG:
		rc = lpfc_ioctl_getcfg(phba, cip, dataout);
		break;

	case LPFC_SETCFG:
		rc = lpfc_ioctl_setcfg(phba, cip);
		break;

	case LPFC_VPORT_GETCFG:
		rc = lpfc_ioctl_vport_getcfg(phba, cip, dataout);
		break;

	case LPFC_VPORT_SETCFG:
		rc = lpfc_ioctl_vport_setcfg(phba, cip);
		break;

	case LPFC_HBA_GET_EVENT:
		rc = lpfc_ioctl_hba_get_event(phba, cip);
		break;

	case LPFC_HBA_SET_EVENT:
		rc = lpfc_ioctl_hba_set_event(phba, cip);
		break;

	case LPFC_HBA_UNSET_EVENT:
		rc = lpfc_ioctl_hba_unset_event(phba, cip);
		break;

	case LPFC_TEMP_SENSOR_SUPPORT:
		rc = (phba->temp_sensor_support) ? 0 : ENODEV;
		break;

	case LPFC_LIST_BIND:
		rc = lpfc_ioctl_list_bind(phba, cip, dataout);
		break;

	case LPFC_GET_VPD:
		rc = lpfc_ioctl_get_vpd(phba, cip, dataout);
		break;

        case LPFC_VPORT_GET_LIST:
                rc = lpfc_ioctl_vport_list(phba, cip, dataout);
                break;

	case LPFC_VPORT_GET_ATTRIB:
		rc = lpfc_ioctl_vport_attrib(phba, cip, dataout);
		break;

	case LPFC_VPORT_GET_RESRC:
		/* Driver Lock is not needed */
		rc = lpfc_ioctl_vport_resrc(phba, cip, dataout);
		break;

	case LPFC_NPIV_READY:
		rc = lpfc_ioctl_npiv_ready(phba, cip, dataout);
		break;

	case LPFC_VPORT_GET_NODE_INFO:
		rc = lpfc_ioctl_vport_nodeinfo(phba, cip, dataout);
		break;

	case LPFC_GET_DUMPREGION:
		rc = lpfc_ioctl_get_dumpregion(phba, cip, dataout);
		break;

	case LPFC_GET_LPFCDFC_INFO:
		rc = lpfc_ioctl_get_lpfcdfc_info(phba, cip, dataout);
 		break;

	case LPFC_LOOPBACK_MODE:
		rc = lpfc_ioctl_loopback_mode(phba, cip, dataout);
		break;

	case LPFC_LOOPBACK_TEST:
		rc = lpfc_ioctl_loopback_test(phba, cip, dataout);
		break;

	case LPFC_HBA_ISSUE_DMP_MBX:
		rc = lpfc_ioctl_issue_dump_mbox(phba, cip, dataout);
		break;

	case LPFC_HBA_ISSUE_UPD_MBX:
		rc = lpfc_ioctl_issue_update_mbox(phba, cip, dataout);
		break;

	case LPFC_HBA_GET_FCF_LIST:
		rc = lpfc_ioctl_get_fcf_list(phba, cip, dataout);
		break;

	case LPFC_GET_LOGICAL_LINK_SPEED:
		rc = lpfc_ioctl_get_logical_link_speed(phba, cip, dataout);
		break;

	default:
		rc = EINVAL;
		break;
	}

	/* 
	 * Copy data to user space config method.  If return
	 * code is E2BIG, data length has been truncated
	 */
	if (rc == 0 || rc == E2BIG) {
		if ((cip->lpfc_outsz) && (dataout != NULL)) {
			if (copy_to_user((uint8_t *) cip->lpfc_dataout,
				(uint8_t *) dataout, (int)cip->lpfc_outsz)) {
					rc = EIO;
			}
		}
	}

	if (dataout != NULL)
		kfree(dataout);

	return rc;
}

static int
lpfc_ioctl_index_find(struct lpfcCmdInput *cip)
{
	int index = 0;

	if ((cip->lpfc_cmd >= LPFC_LIP) &&
	    (cip->lpfc_cmd <= LPFC_DEVP))
		index = LPFC_DEBUG_IOCTL;
	else if ((cip->lpfc_cmd >= LPFC_WRITE_PCI) &&
		 (cip->lpfc_cmd <= LPFC_PRIMARY_IOCTL_RANGE_END))
		index = LPFC_IOCTL;
	else if ((cip->lpfc_cmd >= LPFC_HBA_ADAPTERATTRIBUTES) &&
		 (cip->lpfc_cmd <= LPFC_HBA_GETEVENT))
		index = LPFC_HBAAPI_IOCTL;
	else
		index = LPFC_IOCTL_CMD_ERROR;

	return index;
}

static int
lpfcdfc_ioctl(struct inode *inode,
	      struct file *file, unsigned int cmd, unsigned long arg)
{
	int rc, index;
	struct lpfcCmdInput *ci;
	struct lpfcdfc_host * dfchba = NULL;
	struct lpfc_hba *phba = NULL;

	if (!arg)
		return -EINVAL;

	ci = kmalloc(sizeof (struct lpfcCmdInput), GFP_KERNEL);
	if (!ci)
		return -ENOMEM;
	memset(ci, 0, sizeof(struct lpfcCmdInput));

	rc = copy_from_user((uint8_t *) ci, (void __user *) arg,
			    sizeof (struct lpfcCmdInput)); 
	if (rc) {
		kfree(ci);
		return -EIO;
	}

	dfchba = lpfcdfc_get_phba_by_inst(ci->lpfc_brd);
	if (dfchba == NULL) {
		kfree(ci);
		return -EINVAL;
	}

	phba = dfchba->phba;
	if (!phba) {
		kfree(ci);
		return -EINVAL;
	}

	mutex_lock(&lpfcdfc_lock);
	if (dfchba->blocked) {
		mutex_unlock(&lpfcdfc_lock);
		kfree(ci);
		return -EINVAL;
	}
	dfchba->ref_count++;
	mutex_unlock(&lpfcdfc_lock);

	index = lpfc_ioctl_index_find(ci);
	switch (index) {
	case LPFC_DEBUG_IOCTL:
		rc = lpfc_process_ioctl_dfc(phba, ci);
		break;
	case LPFC_IOCTL:
		rc = lpfcdfc_do_ioctl(phba, ci);
		break;
	case LPFC_HBAAPI_IOCTL:
		rc = lpfc_process_ioctl_hbaapi(phba, ci);
		break;
	default:
		break;
	}

	mutex_lock(&lpfcdfc_lock);
	dfchba->ref_count--;
	mutex_unlock(&lpfcdfc_lock);

	if (phba) {
		if (0 == ((ci->lpfc_cmd == LPFC_HBA_GET_EVENT) ||
		   (ci->lpfc_cmd == LPFC_HBA_SET_EVENT) ||
		   (ci->lpfc_cmd == LPFC_HBA_UNSET_EVENT)))
			lpfc_printf_log(phba, KERN_INFO, LOG_LIBDFC,
				"1600 ioctl x%x stat x%x Data: "
				"x%lx x%lx x%lx x%x, x%x x%x x%lx\n",
				ci->lpfc_cmd, rc,
				(unsigned long) ci->lpfc_arg1,
				(unsigned long) ci->lpfc_arg2,
				(unsigned long) ci->lpfc_arg3,
				ci->lpfc_arg4, ci->lpfc_arg5,
				ci->lpfc_outsz,
				(unsigned long) ci->lpfc_dataout);
		else
			lpfc_printf_log(phba, KERN_INFO, LOG_DAEMON,
				"1601 ioctl x%x stat x%x Data: "
				"x%lx x%lx x%lx x%x, x%x x%x x%lx\n",
				ci->lpfc_cmd, rc,
				(unsigned long) ci->lpfc_arg1,
				(unsigned long) ci->lpfc_arg2,
				(unsigned long) ci->lpfc_arg3,
				ci->lpfc_arg4, ci->lpfc_arg5,
				ci->lpfc_outsz,
				(unsigned long) ci->lpfc_dataout);
	}
	kfree(ci);
	return -rc;
}

#ifdef CONFIG_COMPAT
static long
lpfcdfc_compat_ioctl(struct file * file, unsigned int cmd, unsigned long arg)
{
	int index;
	struct lpfcCmdInput *ci;
	struct lpfcdfc_host *dfchba = NULL;
	struct lpfc_hba *phba = NULL;
	struct lpfcCmdInput32 arg32;
	int ret = 0, rc = 0;

	if (!arg)
		return -EINVAL;

	ci = kmalloc(sizeof (struct lpfcCmdInput), GFP_KERNEL);
	if (!ci)
		return -ENOMEM;

	memset(ci, 0, sizeof(struct lpfcCmdInput));

	ret = copy_from_user(&arg32, (void __user *) arg,
			     sizeof(struct lpfcCmdInput32));
	if (ret) {
		ret = EFAULT;
		goto free_mem_exit;
	}

	dfchba = lpfcdfc_get_phba_by_inst(arg32.lpfc_brd);
	if (dfchba == NULL) {
		ret = EINVAL;
		goto free_mem_exit;
	}

	phba = dfchba->phba;
	if (!phba) {
		ret = EINVAL;
		goto free_mem_exit;
	}

	mutex_lock(&lpfcdfc_lock);
	if (dfchba->blocked) {
		mutex_unlock(&lpfcdfc_lock);
		ret = EINVAL;
		goto hba_exit;
	}
	dfchba->ref_count++;
	mutex_unlock(&lpfcdfc_lock);

	/* Execute the 32->64 bit copy attribute-by-attribute. */
	ci->lpfc_brd = arg32.lpfc_brd;
	ci->lpfc_ring = arg32.lpfc_ring;
	ci->lpfc_iocb = arg32.lpfc_iocb;
	ci->lpfc_flag = arg32.lpfc_flag;
	ci->lpfc_arg1 = (void *)(unsigned long) arg32.lpfc_arg1;
	ci->lpfc_arg2 = (void *)(unsigned long) arg32.lpfc_arg2;
	ci->lpfc_arg3 = (void *)(unsigned long) arg32.lpfc_arg3;
	ci->lpfc_dataout = (void *)(unsigned long) arg32.lpfc_dataout;
	ci->lpfc_cmd = arg32.lpfc_cmd;
	ci->lpfc_outsz = arg32.lpfc_outsz;
	ci->lpfc_arg4 = arg32.lpfc_arg4;
	ci->lpfc_arg5 = arg32.lpfc_arg5;
	ci->lpfc_cntl = arg32.lpfc_cntl;

	index = lpfc_ioctl_index_find(ci);
	switch (index) {
	case LPFC_DEBUG_IOCTL:
		ret = lpfc_process_ioctl_dfc(phba, ci);
		break;
	case LPFC_IOCTL:
		ret = lpfcdfc_do_ioctl(phba, ci);
		break;
	case LPFC_HBAAPI_IOCTL:
		ret = lpfc_process_ioctl_hbaapi(phba, ci);
		break;
	default:
		ret = EINVAL;
		goto ref_count_exit;
	}

	/* 
	 * Copy the 32-bit data back to the 32-bit data structure.  Note that
	 * lpfc_arg1 - 5 and dataout are 64-bit address mode void * pointers.
	 */
	arg32.lpfc_brd = ci->lpfc_brd;
	arg32.lpfc_ring = ci->lpfc_ring;
	arg32.lpfc_iocb = ci->lpfc_iocb;
	arg32.lpfc_flag = ci->lpfc_flag;
	arg32.lpfc_arg1 = (u32) (unsigned long) ci->lpfc_arg1;
	arg32.lpfc_arg2 = (u32) (unsigned long) ci->lpfc_arg2;
	arg32.lpfc_arg3 = (u32) (unsigned long) ci->lpfc_arg3;
	arg32.lpfc_dataout = (u32) (unsigned long) ci->lpfc_dataout;
	arg32.lpfc_cmd = ci->lpfc_cmd;
	arg32.lpfc_outsz = ci->lpfc_outsz;
	arg32.lpfc_arg4 = ci->lpfc_arg4;
	arg32.lpfc_arg5 = ci->lpfc_arg5;

	/* 
	 * The lpfc_dataout point, if a pointer to user buffer space, has
	 * already been written to by the driver's ioctl handler.  Don't
	 * copy it again.
	 */
	rc = copy_to_user((void __user *) arg, &arg32,
			  sizeof(struct lpfcCmdInput32));
	if (rc)
		ret = EFAULT;

 ref_count_exit:
	mutex_lock(&lpfcdfc_lock);
	dfchba->ref_count--;
	mutex_unlock(&lpfcdfc_lock);

 hba_exit:
	if (0 == ((ci->lpfc_cmd == LPFC_HBA_GET_EVENT) ||
	   (ci->lpfc_cmd == LPFC_HBA_SET_EVENT) ||
	   (ci->lpfc_cmd == LPFC_HBA_UNSET_EVENT)))
		lpfc_printf_log(phba, KERN_INFO, LOG_LIBDFC,
			"1602 ioctl x%x stat x%x Data: "
			"x%lx x%lx x%lx x%x x%x x%x x%lx\n",
			ci->lpfc_cmd, rc,
			(unsigned long) ci->lpfc_arg1,
			(unsigned long) ci->lpfc_arg2,
			(unsigned long) ci->lpfc_arg3,
			ci->lpfc_arg4, ci->lpfc_arg5,
			ci->lpfc_outsz,
			(unsigned long) ci->lpfc_dataout);
	else
		lpfc_printf_log(phba, KERN_INFO, LOG_DAEMON,
			"1603 ioctl x%x stat x%x Data: "
			"x%lx x%lx x%lx x%x x%x x%x x%lx\n",
			ci->lpfc_cmd, rc,
			(unsigned long) ci->lpfc_arg1,
			(unsigned long) ci->lpfc_arg2,
			(unsigned long) ci->lpfc_arg3,
			ci->lpfc_arg4, ci->lpfc_arg5,
			ci->lpfc_outsz,
			(unsigned long) ci->lpfc_dataout);
 free_mem_exit:
	kfree(ci);
	return -ret;
}
#endif

static struct file_operations lpfc_fops = {
	.owner        = THIS_MODULE,
	.ioctl        = lpfcdfc_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = lpfcdfc_compat_ioctl,
#endif
};

int
lpfc_cdev_init(void)
{
	/*
 	 * Although DEFINE_MUTEX declares the mutex and sets the lock count,
 	 * it doesn't initialize the sleepers count or wait_queue.  The
 	 * call to mutex_init handles this.
 	 */
	mutex_init(&lpfcdfc_lock);

	lpfcdfc_major = register_chrdev(0,  LPFC_CHAR_DEV_NAME, &lpfc_fops);
	if (lpfcdfc_major < 0) {
		printk(KERN_ERR "%s:%d Unable to register \"%s\" device.\n",
		       __func__, __LINE__, LPFC_CHAR_DEV_NAME);
		return lpfcdfc_major;
	}

	return 0;
}

void
lpfc_cdev_exit(void)
{
	unregister_chrdev(lpfcdfc_major, LPFC_CHAR_DEV_NAME);
}
 
void 
lpfc_ioctl_temp_event(struct lpfc_hba * phba, void * pEvent)
{
	struct lpfcdfc_host  * dfchba     = NULL;
	struct lpfcdfc_event * evt        = NULL;
	struct event_data    * evtData    = NULL;
	struct temp_event    * ptempEvent = NULL;

	ptempEvent = (struct temp_event *) pEvent;

	dfchba = lpfcdfc_host_from_hba(phba);
	if (dfchba == NULL) {
		lpfc_printf_log(phba, KERN_ERR, LOG_EVENT,
			"1927 Exec format error, Dropping temp event\n");
		return;
	}

	/* tempEvent.event_type = FC_REG_TEMPERATURE_EVENT;
	 * tempEvent.event_code = LPFC_CRIT_TEMP;
	 * tempEvent.data = (uint32_t)temperature;
	 */

	evt = lpfcdfc_event_new(
		FC_REG_CT_EVENT,
		1313,
		FC_REG_DRIVER_GENERATED_EVENT);

	if (evt == NULL) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_EVENT,
			"1907 Dropping Dump event\n");
		return;
	}

	evt->wait_time_stamp = jiffies;
	evtData = &evt->evtData;
	evtData->type.mask = FC_REG_TEMPERATURE_EVENT;
	evtData->len  = sizeof (struct temp_event);
	evtData->data = kzalloc(evtData->len, GFP_KERNEL);
	if (evtData->data == NULL) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_EVENT,
			"1908 Dropping Temperature event\n");
		kfree(evt);
		return;
	}

	(void) memcpy(evtData->data, ptempEvent, evtData->len);

	lpfc_printf_log(phba, KERN_INFO, LOG_EVENT,
		"1901 Queuing Temperature event\n");

	(void) lpfc_ioctl_queue_event(dfchba, evt, FC_REG_CT_EVENT);
}

void 
lpfc_ioctl_dump_event(struct lpfc_hba * phba)
{
	struct lpfcdfc_host  * dfchba   = NULL;
	struct lpfcdfc_event * evt      = NULL;
	struct event_data    * evtData  = NULL;

	dfchba = lpfcdfc_host_from_hba(phba);
	if (dfchba == NULL) {
		lpfc_printf_log(phba, KERN_ERR, LOG_EVENT,
			"1928 Exec format error, Dropping dump event\n");
		return;
	}

	evt = lpfcdfc_event_new(
		FC_REG_CT_EVENT,
		1313,
		FC_REG_DRIVER_GENERATED_EVENT);

	if (evt == NULL) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_EVENT,
			"1909 Dropping Dump event\n");
		return;
	}

	evt->wait_time_stamp = jiffies;
	evtData = &evt->evtData;
	evtData->type.mask = FC_REG_DUMP_EVENT;
	evtData->len  = 0;
	evtData->data = NULL;

	lpfc_printf_log(phba, KERN_INFO, LOG_EVENT,
		"1902 Queuing Dump event\n");

	(void) lpfc_ioctl_queue_event(dfchba, evt, FC_REG_CT_EVENT);
}

void
lpfc_ioctl_linkstate_event(struct lpfc_hba *phba, uint32_t stateCode)
{
	struct lpfcdfc_host  * dfchba   = NULL;
	struct lpfcdfc_event * evt      = NULL;
	struct event_data    * evtData  = NULL;

	dfchba = lpfcdfc_host_from_hba(phba);

	if (dfchba == NULL) {
		lpfc_printf_log(phba, KERN_ERR, LOG_EVENT,
			"1920 Exec format error, Dropping Link state event\n");
		return;
	}

	evt = lpfcdfc_event_new(FC_REG_CT_EVENT, 1313,
				FC_REG_DRIVER_GENERATED_EVENT);

	if (evt == NULL) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_EVENT,
			"1921 ENOMEM Dropping Link state event\n");
		return;
	}

	evt->wait_time_stamp = jiffies;
	evtData = &evt->evtData;
	evtData->type.mask = FC_REG_LINK_EVENT;
	evtData->len  = sizeof (struct lpfc_link_info);
	evtData->data = kzalloc(evtData->len, GFP_KERNEL);
	if (evtData->data != NULL) {
		struct lpfcCmdInput cip;
		(void) memset(&cip, 0, sizeof(struct lpfcCmdInput));
		/* 
 		 * Note: This function does not actually use the cip structure
 		 * so it is safe to pass an initialized structure. Also the 
 		 * function always returns zero so we can safely ignore the
 		 * return value for now.   -dcw
 		 */
		(void) lpfc_ioctl_linkinfo(phba, &cip, evtData->data);
	}
	else {
		(void) lpfc_evt_reclaim(phba, evt);
		lpfc_printf_log(phba, KERN_WARNING, LOG_EVENT,
			"1933 ENOMEM Discarding Link state data\n");
		return;
	}

	/* Map HBA events to IOCTL events for management daemon */
	switch (stateCode) {
	case HBA_EVENT_LINK_UP:
		evtData->immed_dat = LPFC_EVENT_LINK_UP;
		break;
	case HBA_EVENT_LINK_DOWN:
		evtData->immed_dat = LPFC_EVENT_LINK_DOWN;
		break;
	}

	lpfc_printf_log(phba, KERN_INFO, LOG_EVENT,
		"1922 Queuing Link state event\n");

	(void) lpfc_ioctl_queue_event(dfchba, evt, FC_REG_CT_EVENT);
}

void
lpfc_ioctl_rscn_event(struct lpfc_hba *phba, void *data, uint32_t length)
{
	struct lpfcdfc_host  * dfchba   = NULL;
	struct lpfcdfc_event * evt      = NULL;
	struct event_data    * evtData  = NULL;

	dfchba = lpfcdfc_host_from_hba(phba);

	if (dfchba == NULL) {
		lpfc_printf_log(phba, KERN_ERR, LOG_EVENT,
			"1923 Exec format error, Dropping rscn event\n");
		return;
	}

	evt = lpfcdfc_event_new(FC_REG_CT_EVENT, 1313,
				FC_REG_DRIVER_GENERATED_EVENT);

	if (evt == NULL) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_EVENT,
			"1924 ENOMEM Dropping rscn event\n");
		return;
	}

	evt->wait_time_stamp = jiffies;
	evtData = &evt->evtData;
	evtData->immed_dat = HBA_EVENT_RSCN;
	evtData->type.mask = FC_REG_RSCN_EVENT;
	evtData->len  = length;
	evtData->data = data;

	lpfc_printf_log(phba, KERN_INFO, LOG_EVENT,
		"1925 Queuing rscn event\n");

	(void) lpfc_ioctl_queue_event(dfchba, evt, FC_REG_CT_EVENT);
}

/* Queue an SD event */
void lpfc_send_event(void *hba, u_int32_t event_len, void *event_data,
		struct event_type type)
{
	struct lpfc_hba *phba = hba;

	uint8_t qCount;
	bool qEvents;
	struct lpfcdfc_event *evt;
	struct lpfcdfc_host *dfchba = lpfcdfc_host_from_hba(phba);

	if (!dfchba) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_EVENT,
				"1964 Failed to get the host,"
				" dropping the event\n");
		goto error;
	}

	mutex_lock(&lpfcdfc_lock);
	qCount = dfchba->qCount;
	qEvents = dfchba->qEvents;
	mutex_unlock(&lpfcdfc_lock);

	if (!qEvents) {
		lpfc_printf_log(phba, KERN_INFO, LOG_EVENT,
				"1966 Events not enabled\n");
		goto error;
	}
	if (qCount >= LPFC_IOCTL_MAX_EVENTS) {
		lpfc_printf_log(phba, KERN_INFO, LOG_EVENT,
				"1967 Event Q Full, dropping the event\n");
		goto error;
	}

	evt = kzalloc(sizeof(struct lpfcdfc_event), GFP_KERNEL);
	if (!evt) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_EVENT,
				"1965 Failed to allocate memory,"
				" dropping the event\n");
		goto error;
	}

	/* Prepare the event node to queue it */
	evt->evtData.type      = type;
	evt->evtData.immed_dat = 0; /* not used */
	evt->evtData.len       = event_len;
	evt->evtData.data      = event_data;

	mutex_lock(&lpfcdfc_lock);
	list_add_tail(&evt->node, &dfchba->evtQueue);
	(dfchba->qCount)++;
	mutex_unlock(&lpfcdfc_lock);

	return;

error:
	if (event_data != NULL)
		kfree(event_data);
	return;
}

