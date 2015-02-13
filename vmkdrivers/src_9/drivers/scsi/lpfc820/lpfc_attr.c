/*******************************************************************
 * This file is part of the Emulex Linux Device Driver for         *
 * Fibre Channel Host Bus Adapters.                                *
 * Copyright (C) 2004-2010 Emulex.  All rights reserved.           *
 * EMULEX and SLI are trademarks of Emulex.                        *
 * www.emulex.com                                                  *
 * Portions Copyright (C) 2004-2008 Christoph Hellwig              *
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

#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/interrupt.h>

#include <scsi/scsi.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_tcq.h>
#include <scsi/scsi_transport_fc.h>

#include "fc_fs.h"
#include "lpfc_hw4.h"
#include "lpfc_hw.h"
#include "lpfc_sli.h"
#include "lpfc_sli4.h"
#include "lpfc_nl.h"
#include "lpfc_disc.h"
#include "lpfc_scsi.h"
#include "lpfc.h"
#include "lpfc_logmsg.h"
#include "lpfc_version.h"
#include "lpfc_compat.h"
#include "lpfc_vport.h"
#include "lpfc_auth_access.h"
#include "hbaapi.h"
#include "lpfc_ioctl.h"
#include "lpfc_crtn.h"

#if defined(__VMKLNX__)
#define LPFC_DEF_DEVLOSS_TMO 10
#else /* !defined(__VMKLNX__) */
#define LPFC_DEF_DEVLOSS_TMO 30
#endif /* defined(__VMKLNX__) */
#define LPFC_MIN_DEVLOSS_TMO 1
#define LPFC_MAX_DEVLOSS_TMO 255

#define LPFC_MAX_LINK_SPEED 8
#define LPFC_LINK_SPEED_BITMAP 0x00000117
#define LPFC_LINK_SPEED_STRING "0, 1, 2, 4, 8"

extern struct bin_attribute sysfs_menlo_attr;

/*
 * Write key size should be multiple of 4. If write key is changed
 * make sure that library write key is also changed.
 */
#define LPFC_REG_WRITE_KEY_SIZE	4
#define LPFC_REG_WRITE_KEY	"EMLX"


/**
 * lpfc_get_hba_info - Return various bits of informaton about the adapter
 * @phba: pointer to the adapter structure.
 * @mxri: max xri count.
 * @axri: available xri count.
 * @mrpi: max rpi count.
 * @arpi: available rpi count.
 * @mvpi: max vpi count.
 * @avpi: available vpi count.
 *
 * Description:
 * If an integer pointer for an count is not null then the value for the
 * count is returned.
 *
 * Returns:
 * zero on error
 * one for success
 **/
int
lpfc_get_hba_info(struct lpfc_hba *phba,
		  uint32_t *mxri, uint32_t *axri,
		  uint32_t *mrpi, uint32_t *arpi,
		  uint32_t *mvpi, uint32_t *avpi)
{
	struct lpfc_mbx_read_config *rd_config;
	LPFC_MBOXQ_t *pmboxq;
	MAILBOX_t *pmb;
	int rc = 0;
	uint32_t max_vpi;

	/*
	 * prevent udev from issuing mailbox commands until the port is
	 * configured.
	 */
	if (phba->link_state < LPFC_LINK_DOWN || !phba->mbox_mem_pool ||
	    (phba->sli.sli_flag & LPFC_SLI_ACTIVE) == 0)
		return 0;

	if (phba->sli.sli_flag & LPFC_BLOCK_MGMT_IO)
		return 0;

	pmboxq = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmboxq)
		return 0;
	memset(pmboxq, 0, sizeof (LPFC_MBOXQ_t));

	pmb = &pmboxq->u.mb;
	pmb->mbxCommand = MBX_READ_CONFIG;
	pmb->mbxOwner = OWN_HOST;
	pmboxq->context1 = NULL;

	if (phba->pport->fc_flag & FC_OFFLINE_MODE)
		rc = MBX_NOT_FINISHED;
	else
		rc = lpfc_sli_issue_mbox_wait(phba, pmboxq, phba->fc_ratov * 2);
	if (rc != MBX_SUCCESS) {
		if (rc != MBX_TIMEOUT)
			mempool_free(pmboxq, phba->mbox_mem_pool);
		return 0;
	}

	if (phba->sli_rev == LPFC_SLI_REV4) {
		rd_config = &pmboxq->u.mqe.un.rd_config;
		if (mrpi)
			*mrpi = bf_get(lpfc_mbx_rd_conf_rpi_count, rd_config);
		if (arpi)
			*arpi = bf_get(lpfc_mbx_rd_conf_rpi_count, rd_config) -
				phba->sli4_hba.max_cfg_param.rpi_used;
		if (mxri)
			*mxri = bf_get(lpfc_mbx_rd_conf_xri_count, rd_config);
		if (axri)
			*axri = bf_get(lpfc_mbx_rd_conf_xri_count, rd_config) -
				phba->sli4_hba.max_cfg_param.xri_used;

		/* Account for differences with SLI-3.  Get vpi count from
		 * mailbox data and subtract one for max vpi value.
		 */
		max_vpi = (bf_get(lpfc_mbx_rd_conf_vpi_count, rd_config) > 0) ?
			(bf_get(lpfc_mbx_rd_conf_vpi_count, rd_config) - 1) : 0;

		if (mvpi)
			*mvpi = max_vpi;
		if (avpi)
			*avpi = max_vpi - phba->sli4_hba.max_cfg_param.vpi_used;
	} else {
		if (mrpi)
			*mrpi = pmb->un.varRdConfig.max_rpi;
		if (arpi)
			*arpi = pmb->un.varRdConfig.avail_rpi;
		if (mxri)
			*mxri = pmb->un.varRdConfig.max_xri;
		if (axri)
			*axri = pmb->un.varRdConfig.avail_xri;
		if (mvpi)
			*mvpi = pmb->un.varRdConfig.max_vpi;
		if (avpi)
			*avpi = pmb->un.varRdConfig.avail_vpi;
	}

	mempool_free(pmboxq, phba->mbox_mem_pool);
	return 1;
}

static ssize_t
lpfc_max_vpi_show(struct class_device *cdev, char *buf)
{
	struct Scsi_Host  *shost = class_to_shost(cdev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	uint32_t cnt;

	if (vport->vport_flag & STATIC_VPORT)
		snprintf(buf, PAGE_SIZE, "%d\n", 0);
	else {
		if (lpfc_get_hba_info(phba, NULL, NULL, NULL, NULL, &cnt, NULL))
			return snprintf(buf, PAGE_SIZE, "%d\n", cnt);
	}
	return snprintf(buf, PAGE_SIZE, "Unknown\n");
}

static ssize_t
lpfc_used_vpi_show(struct class_device *cdev, char *buf)
{
	struct Scsi_Host  *shost = class_to_shost(cdev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	uint32_t cnt, acnt;

	if (vport->vport_flag & STATIC_VPORT)
		snprintf(buf, PAGE_SIZE, "%d\n", 0);
	else {
		if (lpfc_get_hba_info(phba, NULL, NULL, NULL, NULL, &cnt,
				      &acnt))
			return snprintf(buf, PAGE_SIZE, "%d\n", (cnt - acnt));
	}
	return snprintf(buf, PAGE_SIZE, "Unknown\n");
}

static ssize_t
lpfc_npiv_info_show(struct class_device *cdev, char *buf)
{
	struct Scsi_Host  *shost = class_to_shost(cdev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;

	if (!(phba->max_vpi))
		return snprintf(buf, PAGE_SIZE, "NPIV Not Supported\n");
	if ((vport->port_type == LPFC_PHYSICAL_PORT) ||
	    (vport->vport_flag & STATIC_VPORT))
		return snprintf(buf, PAGE_SIZE, "NPIV Physical\n");
	return snprintf(buf, PAGE_SIZE, "NPIV Virtual (VPI %d)\n", vport->vpi);
}

static int
lpfc_do_offline(struct lpfc_hba *phba, uint32_t type)
{
	struct completion online_compl;
	struct lpfc_sli_ring *pring;
	struct lpfc_sli *psli;
	int status = 0;
	int cnt = 0;
	int i;

	init_completion(&online_compl);
	if (!lpfc_workq_post_event(phba, &status, &online_compl,
			      LPFC_EVT_OFFLINE_PREP))
		return -ENOMEM;

	wait_for_completion(&online_compl);

	if (status != 0)
		return -EIO;

	psli = &phba->sli;

	/* Wait a little for things to settle down, but not
	 * long enough for dev loss timeout to expire.
	 */
	for (i = 0; i < psli->num_rings; i++) {
		pring = &psli->ring[i];
		while (pring->txcmplq_cnt) {
			msleep(10);
			if (cnt++ > 500) {  /* 5 secs */
				lpfc_printf_log(phba,
					KERN_WARNING, LOG_INIT,
					"0466 Outstanding IO when "
					"bringing Adapter offline\n");
				break;
			}
		}
	}

	init_completion(&online_compl);
	if (!lpfc_workq_post_event(phba, &status, &online_compl, type))
		return -ENOMEM;
	wait_for_completion(&online_compl);

	if (status != 0)
		return -EIO;

	return 0;
}

int
lpfc_selective_reset(struct lpfc_hba *phba)
{
	struct completion online_compl;
	int status = 0;

	if (!phba->cfg_enable_hba_reset)
		return -EIO;

	status = lpfc_do_offline(phba, LPFC_EVT_OFFLINE);

	if (status != 0)
		return status;

	init_completion(&online_compl);
	if (!lpfc_workq_post_event(phba, &status, &online_compl, LPFC_EVT_ONLINE))
		return -ENOMEM;
	wait_for_completion(&online_compl);

	if (status != 0)
		return -EIO;

	return 0;
}


int
lpfc_board_mode_get(struct lpfc_hba *phba, uint32_t *dataout)
{
	switch (phba->link_state) {
	case LPFC_HBA_ERROR:
		*dataout = DDI_DIAGDI;
		break;
	case LPFC_WARM_START:
		*dataout = DDI_WARMDI;
		break;
	case LPFC_INIT_START:
		*dataout = DDI_OFFDI;
		break;
	default:
		*dataout = DDI_ONDI;
		break;
	}

	return 0;
}

int
lpfc_board_mode_set(struct lpfc_hba *phba, uint32_t ddi_cmd, uint32_t *dataout)
{
	struct completion online_compl;
	int rc = 0;
	int status = 0;

	if (!phba->cfg_enable_hba_reset)
		return EACCES;

	switch (ddi_cmd) {
	case DDI_BRD_ONDI:
		init_completion(&online_compl);
		if (!lpfc_workq_post_event(phba, &status, &online_compl,
				      LPFC_EVT_ONLINE)) {
			rc = ENOMEM;
			break;
		}
		wait_for_completion(&online_compl);
		*dataout = DDI_ONDI;
		break;
	case DDI_BRD_OFFDI:
		rc = lpfc_do_offline(phba, LPFC_EVT_OFFLINE);
		*dataout = DDI_OFFDI;
		break;
	case DDI_BRD_WARMDI:
		rc = lpfc_do_offline(phba, LPFC_EVT_WARM_START);
		*dataout = DDI_WARMDI;
		break;
	case DDI_BRD_DIAGDI:
		rc = lpfc_do_offline(phba, LPFC_EVT_KILL);
		*dataout = DDI_DIAGDI;
		break;
	default:
		rc = EINVAL;
		break;
	}

	return rc;
}

int
lpfc_issue_lip(struct Scsi_Host *shost)
{
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	LPFC_MBOXQ_t *pmboxq;
	int mbxstatus = MBXERR_ERROR;

	if ((vport->fc_flag & FC_OFFLINE_MODE) ||
	    (phba->sli.sli_flag & LPFC_BLOCK_MGMT_IO))
		return -EPERM;

	pmboxq = mempool_alloc(phba->mbox_mem_pool,GFP_KERNEL);

	if (!pmboxq)
		return -ENOMEM;

	memset((void *)pmboxq, 0, sizeof (LPFC_MBOXQ_t));
	pmboxq->u.mb.mbxCommand = MBX_DOWN_LINK;
	pmboxq->u.mb.mbxOwner = OWN_HOST;

	mbxstatus = lpfc_sli_issue_mbox_wait(phba, pmboxq, LPFC_MBOX_TMO * 2);

	if ((mbxstatus == MBX_SUCCESS) && (pmboxq->u.mb.mbxStatus == 0)) {
		memset((void *)pmboxq, 0, sizeof (LPFC_MBOXQ_t));
		lpfc_init_link(phba, pmboxq, phba->cfg_topology,
			       phba->cfg_link_speed);
		mbxstatus = lpfc_sli_issue_mbox_wait(phba, pmboxq,
						     phba->fc_ratov * 2);
	}

	lpfc_set_loopback_flag(phba);
	if (mbxstatus != MBX_TIMEOUT)
		mempool_free(pmboxq, phba->mbox_mem_pool);

	if (mbxstatus == MBXERR_ERROR)
		return -EIO;

	return 0;
}

/**
 * lpfc_param_show - Return a cfg attribute value in decimal
 *
 * Description:
 * Macro that given an attr e.g. hba_queue_depth expands
 * into a function with the name lpfc_hba_queue_depth_show.
 *
 * lpfc_##attr##_show: Return the decimal value of an adapters cfg_xxx field.
 * @dev: class device that is converted into a Scsi_host.
 * @attr: device attribute, not used.
 * @buf: on return contains the attribute value in decimal.
 *
 * Returns: size of formatted string.
 **/
#define lpfc_param_show(attr)	\
uint32_t \
lpfc_##attr##_show(struct lpfc_hba *phba) \
{ \
	return phba->cfg_##attr; \
}

/* Initialize Config Parameter
 *  - Translate all parameter strings from " _" to "-" 
 *  - Initialize the phba config value
 *  - Initialize the phba config array (used for IOCTL management)
 */

#define lpfc_param_init(attr, default, minval, maxval)	\
int \
lpfc_##attr##_init(struct lpfc_hba *phba, int val) \
{ \
	int i=0; \
	while(lpfc_export_##attr.a_string[i++]) \
		if(lpfc_export_##attr.a_string[i] == '_') \
			lpfc_export_##attr.a_string[i] = '-'; \
	if ((phba->brd_no == 0) && (lpfc0_##attr != -1)) {\
		if (lpfc0_##attr >= minval &&  lpfc0_##attr <= maxval)\
			phba->cfg_##attr = lpfc0_##attr;\
		else goto lpfc_##attr##_bad;\
	}\
	else if ((phba->brd_no == 1) && (lpfc1_##attr != -1)) {\
		if (lpfc1_##attr >= minval &&  lpfc1_##attr <= maxval)\
			phba->cfg_##attr = lpfc1_##attr;\
		else goto lpfc_##attr##_bad;\
	}\
	else if ((phba->brd_no == 2) && (lpfc2_##attr != -1)) {\
		if (lpfc2_##attr >= minval &&  lpfc2_##attr <= maxval)\
			phba->cfg_##attr = lpfc2_##attr;\
		else goto lpfc_##attr##_bad;\
	}\
	else if ((phba->brd_no == 3) && (lpfc3_##attr != -1)) {\
		if (lpfc3_##attr >= minval &&  lpfc3_##attr <= maxval)\
			phba->cfg_##attr = lpfc3_##attr;\
		else goto lpfc_##attr##_bad;\
	}\
	else if ((phba->brd_no == 4) && (lpfc4_##attr != -1)) {\
		if (lpfc4_##attr >= minval &&  lpfc4_##attr <= maxval)\
			phba->cfg_##attr = lpfc4_##attr;\
		else goto lpfc_##attr##_bad;\
	}\
	else if ((phba->brd_no == 5) && (lpfc5_##attr != -1)) {\
		if (lpfc5_##attr >= minval &&  lpfc5_##attr <= maxval)\
			phba->cfg_##attr = lpfc5_##attr;\
		else goto lpfc_##attr##_bad;\
	}\
	else if ((phba->brd_no == 6) && (lpfc6_##attr != -1)) {\
		if (lpfc6_##attr >= minval &&  lpfc6_##attr <= maxval)\
			phba->cfg_##attr = lpfc6_##attr;\
		else goto lpfc_##attr##_bad;\
	}\
	else if ((phba->brd_no == 7) && (lpfc7_##attr != -1)) {\
		if (lpfc7_##attr >= minval &&  lpfc7_##attr <= maxval)\
			phba->cfg_##attr = lpfc7_##attr;\
		else goto lpfc_##attr##_bad;\
	}\
	else if ((phba->brd_no == 8) && (lpfc8_##attr != -1)) {\
		if (lpfc8_##attr >= minval &&  lpfc8_##attr <= maxval)\
			phba->cfg_##attr = lpfc8_##attr;\
		else goto lpfc_##attr##_bad;\
	}\
	else if ((phba->brd_no == 9) && (lpfc9_##attr != -1)) {\
		if (lpfc9_##attr >= minval &&  lpfc9_##attr <= maxval)\
			phba->cfg_##attr = lpfc9_##attr;\
		else goto lpfc_##attr##_bad;\
	}\
	else if ((phba->brd_no == 10) && (lpfc10_##attr != -1)) {\
		if (lpfc10_##attr >= minval &&  lpfc10_##attr <= maxval)\
			phba->cfg_##attr = lpfc10_##attr;\
		else goto lpfc_##attr##_bad;\
	}\
	else if ((phba->brd_no == 11) && (lpfc11_##attr != -1)) {\
		if (lpfc11_##attr >= minval &&  lpfc11_##attr <= maxval)\
			phba->cfg_##attr = lpfc11_##attr;\
		else goto lpfc_##attr##_bad;\
	}\
	else if ((phba->brd_no == 12) && (lpfc12_##attr != -1)) {\
		if (lpfc12_##attr >= minval &&  lpfc12_##attr <= maxval)\
			phba->cfg_##attr = lpfc12_##attr;\
		else goto lpfc_##attr##_bad;\
	}\
	else if ((phba->brd_no == 13) && (lpfc13_##attr != -1)) {\
		if (lpfc13_##attr >= minval &&  lpfc13_##attr <= maxval)\
			phba->cfg_##attr = lpfc13_##attr;\
		else goto lpfc_##attr##_bad;\
	}\
	else if ((phba->brd_no == 14) && (lpfc14_##attr != -1)) {\
		if (lpfc14_##attr >= minval &&  lpfc14_##attr <= maxval)\
			phba->cfg_##attr = lpfc14_##attr;\
		else goto lpfc_##attr##_bad;\
	}\
	else if ((phba->brd_no == 15) && (lpfc15_##attr != -1)) {\
		if (lpfc15_##attr >= minval &&  lpfc15_##attr <= maxval)\
			phba->cfg_##attr = lpfc15_##attr;\
		else goto lpfc_##attr##_bad;\
	}\
	else {\
		if (val >= minval &&  val <= maxval)\
			phba->cfg_##attr = val;\
		else goto lpfc_##attr##_bad;\
	}\
	if (phba->cfg_##attr != default) \
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT, \
			"0405 Config Param %s set to x%x\n", \
			lpfc_export_##attr.a_string, phba->cfg_##attr); \
	if (phba->CfgCnt < LPFC_CFG_PARAM_MAX) {\
		phba->CfgTbl[phba->CfgCnt++] = &lpfc_cfg_##attr; \
		return 0; \
	} else {\
	        lpfc_printf_log(phba, KERN_ERR, LOG_INIT, \
			 "0400 Phys Attribute Count Exceeded, Max %d, Actual %d\n", \
                         LPFC_CFG_PARAM_MAX, phba->CfgCnt); \
                return -EINVAL; \
	}\
lpfc_##attr##_bad:\
	lpfc_printf_log(phba, KERN_ERR, LOG_INIT, \
		"0449 Phys Attribute Instance Error.  Defaulting lpfc_"#attr" "\
		"to %d." \
		"Allowed range is ["#minval", "#maxval"]\n", default); \
        phba->cfg_##attr = default;\
	if (phba->CfgCnt < LPFC_CFG_PARAM_MAX) \
		phba->CfgTbl[phba->CfgCnt++] = &lpfc_cfg_##attr; \
	return -EINVAL;\
}

#define lpfc_param_set(attr, default, minval, maxval)	\
int \
lpfc_##attr##_set(struct lpfc_hba *phba, uint32_t val) \
{ \
	if (val >= minval && val <= maxval) {\
		phba->cfg_##attr = val;\
		return 0;\
	}\
	lpfc_printf_log(phba, KERN_ERR, LOG_INIT, \
			"0450 lpfc_"#attr" attribute cannot be set to %d, " \
			"allowed range is ["#minval", "#maxval"]\n", val); \
	return ERANGE;\
}


#define lpfc_vport_param_show(attr)	\
uint32_t \
lpfc_vport_##attr##_show(struct lpfc_vport *vport) \
{ \
	return vport->cfg_##attr;\
}

/*
 * The lpfc_vport_param_init macro builds the list of managable
 * parameters and their value per vport and stores them in the
 * vport's CfgTbl array.
 */
#define lpfc_vport_param_init(attr, default, minval, maxval)	\
int \
lpfc_vport_##attr##_init(struct lpfc_vport *vport, int val) \
{ \
	int i = 0; \
	struct lpfc_hba *phba = vport->phba; \
	while(lpfc_export_##attr.a_string[i++]) \
		if(lpfc_export_##attr.a_string[i] == '_') \
			lpfc_export_##attr.a_string[i] = '-'; \
	if ((phba->brd_no == 0) && (lpfc0_##attr != -1)) {\
		if (lpfc0_##attr >= minval &&  lpfc0_##attr <= maxval)\
			vport->cfg_##attr = lpfc0_##attr;\
		else goto lpfc_##attr##_bad;\
	}\
	else if ((phba->brd_no == 1) && (lpfc1_##attr != -1)) {\
		if (lpfc1_##attr >= minval &&  lpfc1_##attr <= maxval)\
			vport->cfg_##attr = lpfc1_##attr;\
		else goto lpfc_##attr##_bad;\
	}\
	else if ((phba->brd_no == 2) && (lpfc2_##attr != -1)) {\
		if (lpfc2_##attr >= minval &&  lpfc2_##attr <= maxval)\
			vport->cfg_##attr = lpfc2_##attr;\
		else goto lpfc_##attr##_bad;\
	}\
	else if ((phba->brd_no == 3) && (lpfc3_##attr != -1)) {\
		if (lpfc3_##attr >= minval &&  lpfc3_##attr <= maxval)\
			vport->cfg_##attr = lpfc3_##attr;\
		else goto lpfc_##attr##_bad;\
	}\
	else if ((phba->brd_no == 4) && (lpfc4_##attr != -1)) {\
		if (lpfc4_##attr >= minval &&  lpfc4_##attr <= maxval)\
			vport->cfg_##attr = lpfc4_##attr;\
		else goto lpfc_##attr##_bad;\
	}\
	else if ((phba->brd_no == 5) && (lpfc5_##attr != -1)) {\
		if (lpfc5_##attr >= minval &&  lpfc5_##attr <= maxval)\
			vport->cfg_##attr = lpfc5_##attr;\
		else goto lpfc_##attr##_bad;\
	}\
	else if ((phba->brd_no == 6) && (lpfc6_##attr != -1)) {\
		if (lpfc6_##attr >= minval &&  lpfc6_##attr <= maxval)\
			vport->cfg_##attr = lpfc6_##attr;\
		else goto lpfc_##attr##_bad;\
	}\
	else if ((phba->brd_no == 7) && (lpfc7_##attr != -1)) {\
		if (lpfc7_##attr >= minval &&  lpfc7_##attr <= maxval)\
			vport->cfg_##attr = lpfc7_##attr;\
		else goto lpfc_##attr##_bad;\
	}\
	else if ((phba->brd_no == 8) && (lpfc8_##attr != -1)) {\
		if (lpfc8_##attr >= minval &&  lpfc8_##attr <= maxval)\
			vport->cfg_##attr = lpfc8_##attr;\
		else goto lpfc_##attr##_bad;\
	}\
	else if ((phba->brd_no == 9) && (lpfc9_##attr != -1)) {\
		if (lpfc9_##attr >= minval &&  lpfc9_##attr <= maxval)\
			vport->cfg_##attr = lpfc9_##attr;\
		else goto lpfc_##attr##_bad;\
	}\
	else if ((phba->brd_no == 10) && (lpfc10_##attr != -1)) {\
		if (lpfc10_##attr >= minval &&  lpfc10_##attr <= maxval)\
			vport->cfg_##attr = lpfc10_##attr;\
		else goto lpfc_##attr##_bad;\
	}\
	else if ((phba->brd_no == 11) && (lpfc11_##attr != -1)) {\
		if (lpfc11_##attr >= minval &&  lpfc11_##attr <= maxval)\
			vport->cfg_##attr = lpfc11_##attr;\
		else goto lpfc_##attr##_bad;\
	}\
	else if ((phba->brd_no == 12) && (lpfc12_##attr != -1)) {\
		if (lpfc12_##attr >= minval &&  lpfc12_##attr <= maxval)\
			vport->cfg_##attr = lpfc12_##attr;\
		else goto lpfc_##attr##_bad;\
	}\
	else if ((phba->brd_no == 13) && (lpfc13_##attr != -1)) {\
		if (lpfc13_##attr >= minval &&  lpfc13_##attr <= maxval)\
			vport->cfg_##attr = lpfc13_##attr;\
		else goto lpfc_##attr##_bad;\
	}\
	else if ((phba->brd_no == 14) && (lpfc14_##attr != -1)) {\
		if (lpfc14_##attr >= minval &&  lpfc14_##attr <= maxval)\
			vport->cfg_##attr = lpfc14_##attr;\
		else goto lpfc_##attr##_bad;\
	}\
	else if ((phba->brd_no == 15) && (lpfc15_##attr != -1)) {\
		if (lpfc15_##attr >= minval &&  lpfc15_##attr <= maxval)\
			vport->cfg_##attr = lpfc15_##attr;\
		else goto lpfc_##attr##_bad;\
	}\
        else {\
                if (val >= minval && val <= maxval) \
                        vport->cfg_##attr = val; \
                else goto lpfc_##attr##_bad; \
        }\
	if (vport->cfg_##attr != default) \
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT, \
			"0404 Config Param %s set to x%x\n", \
			lpfc_export_##attr.a_string, vport->cfg_##attr); \
        if (vport->CfgCnt < LPFC_CFG_PARAM_MAX) {\
		vport->CfgTbl[vport->CfgCnt++] = &lpfc_cfg_##attr; \
		return 0; \
        } else {\
                lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT, \
	           "0424 Vport Attribute Count Exceeded, Max %d, Actual %d\n", \
                   LPFC_CFG_PARAM_MAX, vport->CfgCnt); \
                 return -EINVAL; \
	}\
lpfc_##attr##_bad:\
	lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT, \
		   "0423 Vport Attribute Instance Error.  Defaulting lpfc_" \
                   #attr" to %d, error value %d, " \
		   "allowed range is ["#minval", "#maxval"]\n", default, val); \
	vport->cfg_##attr = default;\
	return -EINVAL;\
}

#define lpfc_vport_param_set(attr, default, minval, maxval)	\
int \
lpfc_vport_##attr##_set(struct lpfc_vport *vport, uint32_t val) \
{ \
	if (val >= minval && val <= maxval) {\
		vport->cfg_##attr = val;\
		return 0;\
	}\
	lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT, \
			 "0425 lpfc_"#attr" attribute cannot be set to %d, "\
			 "allowed range is ["#minval", "#maxval"]\n", val); \
	return ERANGE;\
}


#define LPFC_ATTR(name, defval, minval, maxval, flg, chgset, string, desc, show, set) \
static int lpfc_##name = defval;\
static int lpfc0_##name = -1;\
static int lpfc1_##name = -1;\
static int lpfc2_##name = -1;\
static int lpfc3_##name = -1;\
static int lpfc4_##name = -1;\
static int lpfc5_##name = -1;\
static int lpfc6_##name = -1;\
static int lpfc7_##name = -1;\
static int lpfc8_##name = -1;\
static int lpfc9_##name = -1;\
static int lpfc10_##name = -1;\
static int lpfc11_##name = -1;\
static int lpfc12_##name = -1;\
static int lpfc13_##name = -1;\
static int lpfc14_##name = -1;\
static int lpfc15_##name = -1;\
module_param(lpfc_##name, int, 0);\
MODULE_PARM_DESC(lpfc_##name, desc);\
module_param(lpfc0_##name, int, 0);\
MODULE_PARM_DESC(lpfc0_##name, desc);\
module_param(lpfc1_##name, int, 0);\
MODULE_PARM_DESC(lpfc1_##name, desc);\
module_param(lpfc2_##name, int, 0);\
MODULE_PARM_DESC(lpfc2_##name, desc);\
module_param(lpfc3_##name, int, 0);\
MODULE_PARM_DESC(lpfc3_##name, desc);\
module_param(lpfc4_##name, int, 0);\
MODULE_PARM_DESC(lpfc4_##name, desc);\
module_param(lpfc5_##name, int, 0);\
MODULE_PARM_DESC(lpfc5_##name, desc);\
module_param(lpfc6_##name, int, 0);\
MODULE_PARM_DESC(lpfc6_##name, desc);\
module_param(lpfc7_##name, int, 0);\
MODULE_PARM_DESC(lpfc7_##name, desc);\
module_param(lpfc8_##name, int, 0);\
MODULE_PARM_DESC(lpfc8_##name, desc);\
module_param(lpfc9_##name, int, 0);\
MODULE_PARM_DESC(lpfc9_##name, desc);\
module_param(lpfc10_##name, int, 0);\
MODULE_PARM_DESC(lpfc10_##name, desc);\
module_param(lpfc11_##name, int, 0);\
MODULE_PARM_DESC(lpfc11_##name, desc);\
module_param(lpfc12_##name, int, 0);\
MODULE_PARM_DESC(lpfc12_##name, desc);\
module_param(lpfc13_##name, int, 0);\
MODULE_PARM_DESC(lpfc13_##name, desc);\
module_param(lpfc14_##name, int, 0);\
MODULE_PARM_DESC(lpfc14_##name, desc);\
module_param(lpfc15_##name, int, 0);\
MODULE_PARM_DESC(lpfc15_##name, desc);\
static struct CfgParam lpfc_export_##name = {string,minval,maxval,defval,defval,flg,chgset,desc};\
static struct CfgEntry lpfc_cfg_##name = {&lpfc_export_##name,show,set};\
lpfc_param_init(name, defval, minval, maxval)

#define LPFC_ATTR_R(name, defval, minval, maxval, flg, chgset, string, desc) \
static int lpfc_##name = defval;\
static int lpfc0_##name = -1;\
static int lpfc1_##name = -1;\
static int lpfc2_##name = -1;\
static int lpfc3_##name = -1;\
static int lpfc4_##name = -1;\
static int lpfc5_##name = -1;\
static int lpfc6_##name = -1;\
static int lpfc7_##name = -1;\
static int lpfc8_##name = -1;\
static int lpfc9_##name = -1;\
static int lpfc10_##name = -1;\
static int lpfc11_##name = -1;\
static int lpfc12_##name = -1;\
static int lpfc13_##name = -1;\
static int lpfc14_##name = -1;\
static int lpfc15_##name = -1;\
module_param(lpfc_##name, int, 0);\
MODULE_PARM_DESC(lpfc_##name, desc);\
module_param(lpfc0_##name, int, 0);\
MODULE_PARM_DESC(lpfc0_##name, desc);\
module_param(lpfc1_##name, int, 0);\
MODULE_PARM_DESC(lpfc1_##name, desc);\
module_param(lpfc2_##name, int, 0);\
MODULE_PARM_DESC(lpfc2_##name, desc);\
module_param(lpfc3_##name, int, 0);\
MODULE_PARM_DESC(lpfc3_##name, desc);\
module_param(lpfc4_##name, int, 0);\
MODULE_PARM_DESC(lpfc4_##name, desc);\
module_param(lpfc5_##name, int, 0);\
MODULE_PARM_DESC(lpfc5_##name, desc);\
module_param(lpfc6_##name, int, 0);\
MODULE_PARM_DESC(lpfc6_##name, desc);\
module_param(lpfc7_##name, int, 0);\
MODULE_PARM_DESC(lpfc7_##name, desc);\
module_param(lpfc8_##name, int, 0);\
MODULE_PARM_DESC(lpfc8_##name, desc);\
module_param(lpfc9_##name, int, 0);\
MODULE_PARM_DESC(lpfc9_##name, desc);\
module_param(lpfc10_##name, int, 0);\
MODULE_PARM_DESC(lpfc10_##name, desc);\
module_param(lpfc11_##name, int, 0);\
MODULE_PARM_DESC(lpfc11_##name, desc);\
module_param(lpfc12_##name, int, 0);\
MODULE_PARM_DESC(lpfc12_##name, desc);\
module_param(lpfc13_##name, int, 0);\
MODULE_PARM_DESC(lpfc13_##name, desc);\
module_param(lpfc14_##name, int, 0);\
MODULE_PARM_DESC(lpfc14_##name, desc);\
module_param(lpfc15_##name, int, 0);\
MODULE_PARM_DESC(lpfc15_##name, desc);\
static struct CfgParam lpfc_export_##name = {string,minval,maxval,defval,defval,flg,chgset,desc};\
lpfc_param_show(name)\
static struct CfgEntry lpfc_cfg_##name = {&lpfc_export_##name,lpfc_##name##_show,0};\
lpfc_param_init(name, defval, minval, maxval)

#define LPFC_ATTR_RW(name, defval, minval, maxval, flg, chgset, string, desc) \
static int lpfc_##name = defval;\
static int lpfc0_##name = -1;\
static int lpfc1_##name = -1;\
static int lpfc2_##name = -1;\
static int lpfc3_##name = -1;\
static int lpfc4_##name = -1;\
static int lpfc5_##name = -1;\
static int lpfc6_##name = -1;\
static int lpfc7_##name = -1;\
static int lpfc8_##name = -1;\
static int lpfc9_##name = -1;\
static int lpfc10_##name = -1;\
static int lpfc11_##name = -1;\
static int lpfc12_##name = -1;\
static int lpfc13_##name = -1;\
static int lpfc14_##name = -1;\
static int lpfc15_##name = -1;\
module_param(lpfc_##name, int, 0);\
MODULE_PARM_DESC(lpfc_##name, desc);\
module_param(lpfc0_##name, int, 0);\
MODULE_PARM_DESC(lpfc0_##name, desc);\
module_param(lpfc1_##name, int, 0);\
MODULE_PARM_DESC(lpfc1_##name, desc);\
module_param(lpfc2_##name, int, 0);\
MODULE_PARM_DESC(lpfc2_##name, desc);\
module_param(lpfc3_##name, int, 0);\
MODULE_PARM_DESC(lpfc3_##name, desc);\
module_param(lpfc4_##name, int, 0);\
MODULE_PARM_DESC(lpfc4_##name, desc);\
module_param(lpfc5_##name, int, 0);\
MODULE_PARM_DESC(lpfc5_##name, desc);\
module_param(lpfc6_##name, int, 0);\
MODULE_PARM_DESC(lpfc6_##name, desc);\
module_param(lpfc7_##name, int, 0);\
MODULE_PARM_DESC(lpfc7_##name, desc);\
module_param(lpfc8_##name, int, 0);\
MODULE_PARM_DESC(lpfc8_##name, desc);\
module_param(lpfc9_##name, int, 0);\
MODULE_PARM_DESC(lpfc9_##name, desc);\
module_param(lpfc10_##name, int, 0);\
MODULE_PARM_DESC(lpfc10_##name, desc);\
module_param(lpfc11_##name, int, 0);\
MODULE_PARM_DESC(lpfc11_##name, desc);\
module_param(lpfc12_##name, int, 0);\
MODULE_PARM_DESC(lpfc12_##name, desc);\
module_param(lpfc13_##name, int, 0);\
MODULE_PARM_DESC(lpfc13_##name, desc);\
module_param(lpfc14_##name, int, 0);\
MODULE_PARM_DESC(lpfc14_##name, desc);\
module_param(lpfc15_##name, int, 0);\
MODULE_PARM_DESC(lpfc15_##name, desc);\
static struct CfgParam lpfc_export_##name = {string,minval,maxval,defval,defval,flg,chgset,desc};\
lpfc_param_show(name)\
lpfc_param_set(name, defval, minval, maxval)\
static struct CfgEntry lpfc_cfg_##name = {&lpfc_export_##name,lpfc_##name##_show,lpfc_##name##_set};\
lpfc_param_init(name, defval, minval, maxval)

/*
 * The LPFC_ATTR macros initialize 16 module parameters because the
 * managable parameter list for an HBA is composed of the phba and
 * physical vport parameter list.  A virtual port instantiated post
 * driver load inherits the current values from the physical vport
 * instance only.
 */
#define LPFC_VPORT_ATTR(name, defval, minval, maxval, flg, chgset, string, desc, show, set) \
static int lpfc_##name = defval;\
static int lpfc0_##name = -1;\
static int lpfc1_##name = -1;\
static int lpfc2_##name = -1;\
static int lpfc3_##name = -1;\
static int lpfc4_##name = -1;\
static int lpfc5_##name = -1;\
static int lpfc6_##name = -1;\
static int lpfc7_##name = -1;\
static int lpfc8_##name = -1;\
static int lpfc9_##name = -1;\
static int lpfc10_##name = -1;\
static int lpfc11_##name = -1;\
static int lpfc12_##name = -1;\
static int lpfc13_##name = -1;\
static int lpfc14_##name = -1;\
static int lpfc15_##name = -1;\
module_param(lpfc_##name, int, 0);\
MODULE_PARM_DESC(lpfc_##name, desc);\
module_param(lpfc0_##name, int, 0);\
MODULE_PARM_DESC(lpfc0_##name, desc);\
module_param(lpfc1_##name, int, 0);\
MODULE_PARM_DESC(lpfc1_##name, desc);\
module_param(lpfc2_##name, int, 0);\
MODULE_PARM_DESC(lpfc2_##name, desc);\
module_param(lpfc3_##name, int, 0);\
MODULE_PARM_DESC(lpfc3_##name, desc);\
module_param(lpfc4_##name, int, 0);\
MODULE_PARM_DESC(lpfc4_##name, desc);\
module_param(lpfc5_##name, int, 0);\
MODULE_PARM_DESC(lpfc5_##name, desc);\
module_param(lpfc6_##name, int, 0);\
MODULE_PARM_DESC(lpfc6_##name, desc);\
module_param(lpfc7_##name, int, 0);\
MODULE_PARM_DESC(lpfc7_##name, desc);\
module_param(lpfc8_##name, int, 0);\
MODULE_PARM_DESC(lpfc8_##name, desc);\
module_param(lpfc9_##name, int, 0);\
MODULE_PARM_DESC(lpfc9_##name, desc);\
module_param(lpfc10_##name, int, 0);\
MODULE_PARM_DESC(lpfc10_##name, desc);\
module_param(lpfc11_##name, int, 0);\
MODULE_PARM_DESC(lpfc11_##name, desc);\
module_param(lpfc12_##name, int, 0);\
MODULE_PARM_DESC(lpfc12_##name, desc);\
module_param(lpfc13_##name, int, 0);\
MODULE_PARM_DESC(lpfc13_##name, desc);\
module_param(lpfc14_##name, int, 0);\
MODULE_PARM_DESC(lpfc14_##name, desc);\
module_param(lpfc15_##name, int, 0);\
MODULE_PARM_DESC(lpfc15_##name, desc);\
static struct CfgParam lpfc_export_##name = {string,minval,maxval,defval,defval,flg,chgset,desc};\
static struct VPCfgEntry lpfc_cfg_##name = {&lpfc_export_##name,show,set}; \
lpfc_vport_param_init(name, defval, minval, maxval)

#define LPFC_VPORT_ATTR_R(name, defval, minval, maxval, flg, chgset, string, desc) \
static int lpfc_##name = defval;\
static int lpfc0_##name = -1;\
static int lpfc1_##name = -1;\
static int lpfc2_##name = -1;\
static int lpfc3_##name = -1;\
static int lpfc4_##name = -1;\
static int lpfc5_##name = -1;\
static int lpfc6_##name = -1;\
static int lpfc7_##name = -1;\
static int lpfc8_##name = -1;\
static int lpfc9_##name = -1;\
static int lpfc10_##name = -1;\
static int lpfc11_##name = -1;\
static int lpfc12_##name = -1;\
static int lpfc13_##name = -1;\
static int lpfc14_##name = -1;\
static int lpfc15_##name = -1;\
module_param(lpfc_##name, int, 0);\
MODULE_PARM_DESC(lpfc_##name, desc);\
module_param(lpfc0_##name, int, 0);\
MODULE_PARM_DESC(lpfc0_##name, desc);\
module_param(lpfc1_##name, int, 0);\
MODULE_PARM_DESC(lpfc1_##name, desc);\
module_param(lpfc2_##name, int, 0);\
MODULE_PARM_DESC(lpfc2_##name, desc);\
module_param(lpfc3_##name, int, 0);\
MODULE_PARM_DESC(lpfc3_##name, desc);\
module_param(lpfc4_##name, int, 0);\
MODULE_PARM_DESC(lpfc4_##name, desc);\
module_param(lpfc5_##name, int, 0);\
MODULE_PARM_DESC(lpfc5_##name, desc);\
module_param(lpfc6_##name, int, 0);\
MODULE_PARM_DESC(lpfc6_##name, desc);\
module_param(lpfc7_##name, int, 0);\
MODULE_PARM_DESC(lpfc7_##name, desc);\
module_param(lpfc8_##name, int, 0);\
MODULE_PARM_DESC(lpfc8_##name, desc);\
module_param(lpfc9_##name, int, 0);\
MODULE_PARM_DESC(lpfc9_##name, desc);\
module_param(lpfc10_##name, int, 0);\
MODULE_PARM_DESC(lpfc10_##name, desc);\
module_param(lpfc11_##name, int, 0);\
MODULE_PARM_DESC(lpfc11_##name, desc);\
module_param(lpfc12_##name, int, 0);\
MODULE_PARM_DESC(lpfc12_##name, desc);\
module_param(lpfc13_##name, int, 0);\
MODULE_PARM_DESC(lpfc13_##name, desc);\
module_param(lpfc14_##name, int, 0);\
MODULE_PARM_DESC(lpfc14_##name, desc);\
module_param(lpfc15_##name, int, 0);\
MODULE_PARM_DESC(lpfc15_##name, desc);\
static struct CfgParam lpfc_export_##name = {string,minval,maxval,defval,defval,flg,chgset,desc};\
lpfc_vport_param_show(name)\
static struct VPCfgEntry lpfc_cfg_##name = {&lpfc_export_##name,lpfc_vport_##name##_show,0};\
lpfc_vport_param_init(name, defval, minval, maxval)

#define LPFC_VPORT_ATTR_RW(name, defval, minval, maxval, flg, chgset, string, desc) \
static int lpfc_##name = defval;\
static int lpfc0_##name = -1;\
static int lpfc1_##name = -1;\
static int lpfc2_##name = -1;\
static int lpfc3_##name = -1;\
static int lpfc4_##name = -1;\
static int lpfc5_##name = -1;\
static int lpfc6_##name = -1;\
static int lpfc7_##name = -1;\
static int lpfc8_##name = -1;\
static int lpfc9_##name = -1;\
static int lpfc10_##name = -1;\
static int lpfc11_##name = -1;\
static int lpfc12_##name = -1;\
static int lpfc13_##name = -1;\
static int lpfc14_##name = -1;\
static int lpfc15_##name = -1;\
module_param(lpfc_##name, int, 0);\
MODULE_PARM_DESC(lpfc_##name, desc);\
module_param(lpfc0_##name, int, 0);\
MODULE_PARM_DESC(lpfc0_##name, desc);\
module_param(lpfc1_##name, int, 0);\
MODULE_PARM_DESC(lpfc1_##name, desc);\
module_param(lpfc2_##name, int, 0);\
MODULE_PARM_DESC(lpfc2_##name, desc);\
module_param(lpfc3_##name, int, 0);\
MODULE_PARM_DESC(lpfc3_##name, desc);\
module_param(lpfc4_##name, int, 0);\
MODULE_PARM_DESC(lpfc4_##name, desc);\
module_param(lpfc5_##name, int, 0);\
MODULE_PARM_DESC(lpfc5_##name, desc);\
module_param(lpfc6_##name, int, 0);\
MODULE_PARM_DESC(lpfc6_##name, desc);\
module_param(lpfc7_##name, int, 0);\
MODULE_PARM_DESC(lpfc7_##name, desc);\
module_param(lpfc8_##name, int, 0);\
MODULE_PARM_DESC(lpfc8_##name, desc);\
module_param(lpfc9_##name, int, 0);\
MODULE_PARM_DESC(lpfc9_##name, desc);\
module_param(lpfc10_##name, int, 0);\
MODULE_PARM_DESC(lpfc10_##name, desc);\
module_param(lpfc11_##name, int, 0);\
MODULE_PARM_DESC(lpfc11_##name, desc);\
module_param(lpfc12_##name, int, 0);\
MODULE_PARM_DESC(lpfc12_##name, desc);\
module_param(lpfc13_##name, int, 0);\
MODULE_PARM_DESC(lpfc13_##name, desc);\
module_param(lpfc14_##name, int, 0);\
MODULE_PARM_DESC(lpfc14_##name, desc);\
module_param(lpfc15_##name, int, 0);\
MODULE_PARM_DESC(lpfc15_##name, desc);\
static struct CfgParam lpfc_export_##name = {string,minval,maxval,defval,defval,flg,chgset,desc};\
lpfc_vport_param_show(name)\
lpfc_vport_param_set(name, defval, minval, maxval)\
static struct VPCfgEntry lpfc_cfg_##name = {&lpfc_export_##name,lpfc_vport_##name##_show,lpfc_vport_##name##_set};\
lpfc_vport_param_init(name, defval, minval, maxval)

static CLASS_DEVICE_ATTR(max_vpi, S_IRUGO, lpfc_max_vpi_show, NULL);
static CLASS_DEVICE_ATTR(used_vpi, S_IRUGO, lpfc_used_vpi_show, NULL);
static CLASS_DEVICE_ATTR(npiv_info, S_IRUGO, lpfc_npiv_info_show, NULL);

static int
lpfc_parse_wwn(const char *ns, uint8_t *nm)
{
	unsigned int i, j;
	memset(nm, 0, 8);

	/* Validate and store the new name */
	for (i=0, j=0; i < 16; i++) {
		if ((*ns >= 'a') && (*ns <= 'f'))
			j = ((j << 4) | ((*ns++ -'a') + 10));
		else if ((*ns >= 'A') && (*ns <= 'F'))
			j = ((j << 4) | ((*ns++ -'A') + 10));
		else if ((*ns >= '0') && (*ns <= '9'))
			j = ((j << 4) | (*ns++ -'0'));
		else
			return -EINVAL;
		if (i % 2) {
			nm[i/2] = j & 0xff;
			j = 0;
		}
	}

	return 0;
}

/*
# lpfc_cnt: Number of IOCBs allocated for ELS, CT, and ABTS
# 	N -  1024 * N buffers.
*/
LPFC_ATTR_R(iocb_cnt, 1, 1, 5,
	CFG_EXPORT | CFG_THIS_HBA | CFG_SLI4, CFG_REBOOT,
	"iocb_cnt",
	"IOCBs allocated for ELS, CT, ABTS in 1024 increments, Default is 1");

/*
# lpfc_poll: Controls FCP ring polling mode.
#	0 - none
#	1 - poll with interrupts enabled
#	3 - poll and disable FCP ring interrupts
*/
LPFC_ATTR(poll, 0, 0, 3,
        CFG_IGNORE, CFG_REBOOT,
        "poll",
        "FCP ring polling mode control: 0 - none, 1 w/intr, 3 w/o intr",
	0,0);

/*
# lpfc_poll_freq: Maximum FCP ring polling frequency.
#	0 - Poll for FCP command completion when new command is queued.
#	N - Maximum number of times driver poll for FCP command completion
#           per second is N.
*/
LPFC_ATTR(poll_freq, 0, 0, 1000000,
	CFG_IGNORE, CFG_REBOOT,
	"poll",
	"Maximum FCP ring polling frequency",
	0, 0);

/*
# lpfc_sli_mode: Select SLI mode.
#	0 - auto (SLI-3 if supported)
#	2 - select SLI-2 even on SLI-3 capable HBAs
#	3 - select SLI-3
*/
LPFC_ATTR_R(sli_mode, 0, 0, 3,
	CFG_IGNORE | CFG_THIS_HBA | CFG_LP, CFG_REBOOT,
        "sli_mode",
        "SLI mode selector: 0 - auto, 2 - SLI-2, 3 - SLI-3");

uint32_t
lpfc_vport_devloss_tmo_show(struct lpfc_vport *vport)
{
	return vport->cfg_devloss_tmo;
}

static void
lpfc_update_rport_devloss_tmo(struct lpfc_vport *vport)
{
        struct Scsi_Host  *shost;
        struct lpfc_nodelist  *ndlp;

        shost = lpfc_shost_from_vport(vport);
        spin_lock_irq(shost->host_lock);
        list_for_each_entry(ndlp, &vport->fc_nodes, nlp_listp)
                if (NLP_CHK_NODE_ACT(ndlp) && ndlp->rport)
                        ndlp->rport->dev_loss_tmo = vport->cfg_devloss_tmo;
        spin_unlock_irq(shost->host_lock);
}

int
lpfc_vport_devloss_tmo_set(struct lpfc_vport *vport, uint32_t val)
{
	int i = 0;
	struct lpfc_vport **vports;

	if (val < LPFC_MIN_DEVLOSS_TMO || val > LPFC_MAX_DEVLOSS_TMO) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT,
			"0403 lpfc_devloss_tmo attribute cannot be set to"
			"%d, allowed range is [%d, %d]\n",
			val, LPFC_MIN_DEVLOSS_TMO, LPFC_MAX_DEVLOSS_TMO);
		return ERANGE;
	}

	vports = lpfc_create_vport_work_array(vport->phba);
	if (vports != NULL) {
		/* Set each active rport with new devloss-tmo value */
		for (; i <= vport->phba->max_vports && vports[i] != NULL; i++) {
			vports[i]->cfg_devloss_tmo = val;
			lpfc_update_rport_devloss_tmo(vports[i]);
		}
	}
	lpfc_destroy_vport_work_array(vport->phba, vports);
	return 0;
}

/*
# lpfc_devloss_tmo: If set, it will hold all I/O errors on devices that disappear
# until the timer expires. Value range is [0,255]. Default value is 10.
*/
LPFC_VPORT_ATTR(devloss_tmo, LPFC_DEF_DEVLOSS_TMO,
                LPFC_MIN_DEVLOSS_TMO, LPFC_MAX_DEVLOSS_TMO,
		CFG_EXPORT | CFG_COMMON | CFG_THIS_HBA, CFG_DYNAMIC,
                "devloss_tmo",
                "Seconds driver hold I/O waiting "
                "for a loss device to return",
                lpfc_vport_devloss_tmo_show, lpfc_vport_devloss_tmo_set);

ssize_t
lpfc_authenticate (struct class_device *cdev, const char *buf, size_t count)
{
	struct Scsi_Host *shost = class_to_shost(cdev);
	struct lpfc_vport *vport = (struct lpfc_vport *)shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	struct lpfc_nodelist *ndlp;
	int status;
	struct lpfc_name wwpn;

	if (lpfc_parse_wwn(buf, wwpn.u.wwn))
		return -EINVAL;

	if (vport->port_state == LPFC_VPORT_FAILED) {
		lpfc_issue_lip(shost);
		return strlen(buf);
	}
	if ((vport->fc_flag & FC_OFFLINE_MODE) ||
	    (phba->sli.sli_flag & LPFC_BLOCK_MGMT_IO) ||
	    (!vport->cfg_enable_auth))
		return -EPERM;

	/* If vport already in the middle of authentication do not restart */
	if ((vport->auth.auth_msg_state == LPFC_AUTH_NEGOTIATE) ||
	    (vport->auth.auth_msg_state == LPFC_DHCHAP_CHALLENGE) ||
	    (vport->auth.auth_msg_state == LPFC_DHCHAP_REPLY))
		return -EAGAIN;

	if (wwn_to_u64(wwpn.u.wwn) == AUTH_FABRIC_WWN)
		ndlp = lpfc_findnode_did(vport, Fabric_DID);
	else
		ndlp = lpfc_findnode_wwnn(vport, &wwpn);
	if (!ndlp || !NLP_CHK_NODE_ACT(ndlp))
		return -EPERM;
	status = lpfc_start_node_authentication(ndlp);
	if (status)
		return status;
	return strlen(buf);
}

ssize_t
lpfc_update_auth_config (struct class_device *cdev, const char *buf,
			 size_t count)
{
	struct Scsi_Host *shost = class_to_shost(cdev);
	struct lpfc_vport *vport = (struct lpfc_vport *)shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	struct lpfc_nodelist *ndlp;
	struct lpfc_name wwpn;
	int status;

	if (lpfc_parse_wwn(buf, wwpn.u.wwn))
		return -EINVAL;

	if ((vport->fc_flag & FC_OFFLINE_MODE) ||
	    (phba->sli.sli_flag & LPFC_BLOCK_MGMT_IO) ||
	    (!vport->cfg_enable_auth))
		return -EPERM;

	/* If vport already in the middle of authentication do not restart */
	if ((vport->auth.auth_msg_state == LPFC_AUTH_NEGOTIATE) ||
	    (vport->auth.auth_msg_state == LPFC_DHCHAP_CHALLENGE) ||
	    (vport->auth.auth_msg_state == LPFC_DHCHAP_REPLY))
		return -EAGAIN;

	if (wwn_to_u64(wwpn.u.wwn) == AUTH_FABRIC_WWN)
		ndlp = lpfc_findnode_did(vport, Fabric_DID);
	else
		ndlp = lpfc_findnode_wwnn(vport, &wwpn);
	if (!ndlp || !NLP_CHK_NODE_ACT(ndlp))
		return -EPERM;
	status = lpfc_get_auth_config(ndlp, &wwpn);
	if (status)
		return -EPERM;
	return strlen(buf);
}

uint32_t
lpfc_log_verbose_show(struct lpfc_vport *vport)
{
	return vport->phba->pport->cfg_log_verbose;
}

int
lpfc_log_verbose_set(struct lpfc_vport *vport, uint32_t val)
{
	int i = 0;
	struct lpfc_vport **vports = lpfc_create_vport_work_array(vport->phba);

	vport->phba->cfg_log_verbose = val;

	if (vports != NULL)
		/* Set each active vport with new log_verbose value */
		for (; i <= vport->phba->max_vports && vports[i] != NULL; i++)
			vports[i]->cfg_log_verbose = val;
	lpfc_destroy_vport_work_array(vport->phba, vports);
	return 0;
}

LPFC_VPORT_ATTR(log_verbose, 0x0, 0x0, LOG_ALL_MSG,
	CFG_EXPORT | CFG_COMMON | CFG_THIS_HBA,
	CFG_DYNAMIC,
	"log_verbose",
	"Verbose logging bit-mask",
	lpfc_log_verbose_show,
	lpfc_log_verbose_set);


/*
# lpfc_enable_da_id: This turns on the DA_ID CT command that deregisters
# objects that have been registered with the nameserver after login.
*/
LPFC_VPORT_ATTR_R(enable_da_id, 0, 0, 1,
	CFG_IGNORE | CFG_COMMON | CFG_THIS_HBA, CFG_REBOOT,
	"enable_da_id",
	"Deregister nameserver objects before LOGO");

uint32_t
lpfc_lun_queue_depth_show(struct lpfc_vport *vport)
{
	return vport->cfg_lun_queue_depth;
}

#define LPFC_MIN_LUN_QDEPTH 0x01
#define LPFC_MAX_LUN_QDEPTH 0x80
#define LPFC_DFLT_LUN_QDEPTH 0x1e

int
lpfc_lun_queue_depth_set(struct lpfc_vport *vport, uint32_t val)
{
	int i = 0, old_val = 0;
	struct lpfc_vport **vports = lpfc_create_vport_work_array(vport->phba);
	struct Scsi_Host  *shost;
	struct scsi_device *sdev;

	if (val <  LPFC_MIN_LUN_QDEPTH || val > LPFC_MAX_LUN_QDEPTH)
		return ERANGE;

	if (vports != NULL) {
		/* Set each active vport with new lun_queue_depth value */
		for (; i <= vport->phba->max_vports && vports[i] != NULL; i++) {
			old_val = vports[i]->cfg_lun_queue_depth;
			vports[i]->cfg_lun_queue_depth = val;

			if (old_val != val) {
				shost = lpfc_shost_from_vport(vports[i]);
				shost_for_each_device(sdev, shost)
					lpfc_change_queue_depth(sdev, val, 0);
			}
		}
	}
	lpfc_destroy_vport_work_array(vport->phba, vports);
	return 0;
}

/*
# lun_queue_depth:  This parameter is used to limit the number of outstanding
# commands per FCP LUN. Value range is [1,128]. Default value is 30.
*/
LPFC_VPORT_ATTR(lun_queue_depth,
		LPFC_DFLT_LUN_QDEPTH,
		LPFC_MIN_LUN_QDEPTH,
		LPFC_MAX_LUN_QDEPTH,
		CFG_EXPORT | CFG_THIS_HBA | CFG_LP | CFG_SLI4, CFG_DYNAMIC,
		"lun_queue_depth",
		"Max number of FCP commands we can queue to a specific LUN",
		lpfc_lun_queue_depth_show,
		lpfc_lun_queue_depth_set);

/*
# tgt_queue_depth:  This parameter is used to limit the number of outstanding
# commands per target port. Value range is [10,8192]. Default value is 8192.
*/
LPFC_VPORT_ATTR_R(tgt_queue_depth,
	LPFC_MAX_TGT_QDEPTH,
	LPFC_MIN_TGT_QDEPTH,
	LPFC_MAX_TGT_QDEPTH,
	CFG_EXPORT | CFG_THIS_HBA | CFG_LP | CFG_SLI4, CFG_REBOOT,
	"tgt_queue_depth",
	"Max number of FCP commands we can queue to a specific target port");

/*
# hba_queue_depth:  This parameter is used to limit the number of outstanding
# commands per lpfc HBA. Value range is [32,8192]. If this parameter
# value is greater than the maximum number of exchanges supported by the HBA,
# then maximum number of exchanges supported by the HBA is used to determine
# the hba_queue_depth.
*/
LPFC_ATTR_R(hba_queue_depth, 8192, 32, 8192,
	CFG_EXPORT | CFG_THIS_HBA | CFG_LP | CFG_SLI4, CFG_REBOOT,
	"hba_queue_depth",
	"Max number of FCP commands we can queue to a lpfc HBA");

/*
# peer_port_login:  This parameter allows/prevents logins
# between peer ports hosted on the same physical port.
# When this parameter is set 0 peer ports of same physical port
# are not allowed to login to each other.
# When this parameter is set 1 peer ports of same physical port
# are allowed to login to each other.
# Default value of this parameter is 0.
*/
LPFC_VPORT_ATTR_R(peer_port_login, 0, 0, 1, 
	CFG_IGNORE | CFG_COMMON | CFG_THIS_HBA, CFG_REBOOT,
	"peer_port_login",
	"Allow logins of peer ports on same physical port");

/*
# restrict_login:  This parameter allows/prevents logins
# between Virtual Ports and remote initiators.
# When this parameter is not set (0) Virtual Ports will accept PLOGIs from
# other initiators and will attempt to PLOGI all remote ports.
# When this parameter is set (1) Virtual Ports will reject PLOGIs from
# remote ports and will not attempt to PLOGI to other initiators.
# This parameter does not restrict to the physical port.
# This parameter does not restrict logins to Fabric resident remote ports.
# Default value of this parameter is 0 so that the physical port
# sends and accepts PLOGIs.  Vports override this value to 1 (don't 
# login) in lpfc_vport_create after the defaults are read.
*/
LPFC_VPORT_ATTR_R(restrict_login, 0, 0, 1,
	CFG_IGNORE | CFG_COMMON | CFG_THIS_HBA, CFG_REBOOT,
	"restrict_login",
	"Restrict virtual ports login to remote initiators");


void
lpfc_vport_validate_restrict_login(struct lpfc_vport *vport)
{
	if (vport->port_type == LPFC_PHYSICAL_PORT)
		vport->cfg_restrict_login = 0;
}

/*
# Some disk devices have a "select ID" or "select Target" capability.
# From a protocol standpoint "select ID" usually means select the
# Fibre channel "ALPA".  In the FC-AL Profile there is an "informative
# annex" which contains a table that maps a "select ID" (a number
# between 0 and 7F) to an ALPA.  By default, for compatibility with
# older drivers, the lpfc driver scans this table from low ALPA to high
# ALPA.
#
# Turning on the scan-down variable (on  = 1, off = 0) will
# cause the lpfc driver to use an inverted table, effectively
# scanning ALPAs from high to low. Value range is [0,1]. Default value is 1.
#
# (Note: This "select ID" functionality is a LOOP ONLY characteristic
# and will not work across a fabric. Also this parameter will take
# effect only in the case when ALPA map is not available.)
*/
LPFC_VPORT_ATTR_R(scan_down, 1, 0, 1,
	CFG_EXPORT | CFG_COMMON | CFG_THIS_HBA | CFG_FC, CFG_REBOOT,
	"scan_down",
	"Start scanning for devices from highest ALPA to lowest");


uint32_t
lpfc_topology_show(struct lpfc_hba *phba)
{
	return phba->cfg_topology;
}

int
lpfc_topology_set(struct lpfc_hba *phba, uint32_t val)
{
	int rc = 0;

	switch (val) {
	case 0:
	case 1:
	case 2:
	case 4:
	case 6:
		phba->cfg_topology = val;
		break;
	default:
		rc = EINVAL;
	}
	return rc;
}

/*
# lpfc_topology:  link topology for init link
#            0x0  = attempt loop mode then point-to-point
#            0x01 = internal loopback mode
#            0x02 = attempt point-to-point mode only
#            0x04 = attempt loop mode only
#            0x06 = attempt point-to-point mode then loop
# Set point-to-point mode if you want to run as an N_Port.
# Set loop mode if you want to run as an NL_Port. Value range is [0,0x6].
# Default value is 0.
*/
LPFC_ATTR(topology, 0, 0, 6,
	CFG_EXPORT | CFG_THIS_HBA | CFG_FC, CFG_REBOOT,
	"topology",
	"Select Fibre Channel topology: "
	"valid values are 0,1,2,4,6. "
	"See driver manual",
	lpfc_topology_show, lpfc_topology_set);


uint32_t
lpfc_link_speed_show(struct lpfc_hba *phba)
{
	return phba->cfg_link_speed;
}

int
lpfc_link_speed_set(struct lpfc_hba *phba, uint32_t val)
{
	int rc = 0;

	switch (val) {
	case 0:
	case 1:
	case 2:
	case 4:
	case 8:
		phba->cfg_link_speed = val;
		break;
	default:
		rc = EINVAL;
	}
	return rc;
}

/*
# lpfc_link_speed: Link speed selection for initializing the Fibre Channel
# connection.
#       0  = auto select (default)
#       1  = 1 Gigabaud
#       2  = 2 Gigabaud
#       4  = 4 Gigabaud
#       8  = 8 Gigabaud
# Value range is [0,8]. Default value is 0.
*/

LPFC_ATTR(link_speed, 0, 0, LPFC_MAX_LINK_SPEED,
	CFG_EXPORT | CFG_THIS_HBA | CFG_FC, CFG_REBOOT,
	"link_speed",
	"Select link speed: valid values are 1, 2, 4, 8 gigabaud",
	lpfc_link_speed_show, lpfc_link_speed_set);

#if !defined(__VMKLNX__)
/* 06/14/10 AER:  No support for AER in vmklinux right now. Needs testing
 * once AER support is kernel-resident.
 */

/*
# lpfc_aer_support: Support PCIe device Advanced Error Reporting (AER)
#       0  = aer disabled or not supported
#       1  = aer supported and enabled (default)
# Value range is [0,1]. Default value is 1.
*/

/**
 * lpfc_aer_support_store - Set the adapter for aer support
 *
 * @dev: class device that is converted into a Scsi_host.
 * @attr: device attribute, not used.
 * @buf: containing the string "selective".
 * @count: unused variable.
 *
 * Description:
 * If the val is 1 and currently the device's AER capability was not
 * enabled, invoke the kernel's enable AER helper routine, trying to
 * enable the device's AER capability. If the helper routine enabling
 * AER returns success, update the device's cfg_aer_support flag to
 * indicate AER is supported by the device; otherwise, if the device
 * AER capability is already enabled to support AER, then do nothing.
 *
 * If the val is 0 and currently the device's AER support was enabled,
 * invoke the kernel's disable AER helper routine. After that, update
 * the device's cfg_aer_support flag to indicate AER is not supported
 * by the device; otherwise, if the device AER capability is already
 * disabled from supporting AER, then do nothing.
 *
 * Returns:
 * length of the buf on success if val is in range the intended mode
 * is supported.
 * -EINVAL if val out of range or intended mode is not supported.
 **/
static ssize_t
lpfc_aer_support_store(struct class_device *dev, const char *buf, size_t count)
{
        struct Scsi_Host *shost = class_to_shost(dev);
        struct lpfc_vport *vport = (struct lpfc_vport *)shost->hostdata;
        struct lpfc_hba *phba = vport->phba;
        int val = 0, rc = -EINVAL;

	if (!isdigit(buf[0]))
		return -EINVAL;
	if (sscanf(buf, "%i", &val) != 1)
		return -EINVAL;

	switch (val) {
	case 0:
		if (phba->hba_flag & HBA_AER_ENABLED) {
			rc = pci_disable_pcie_error_reporting(phba->pcidev);
			if (!rc) {
				spin_lock_irq(&phba->hbalock);
				phba->hba_flag &= ~HBA_AER_ENABLED;
				spin_unlock_irq(&phba->hbalock);
				phba->cfg_aer_support = 0;
				rc = strlen(buf);
			} else
				rc = -EPERM;
		} else {
			phba->cfg_aer_support = 0;
			rc = strlen(buf);
		}
		break;
	case 1:
		if (!(phba->hba_flag & HBA_AER_ENABLED)) {
			rc = pci_enable_pcie_error_reporting(phba->pcidev);
			if (!rc) {
				spin_lock_irq(&phba->hbalock);
				phba->hba_flag |= HBA_AER_ENABLED;
				spin_unlock_irq(&phba->hbalock);
				phba->cfg_aer_support = 1;
				rc = strlen(buf);
			} else
				 rc = -EPERM;
		} else {
			phba->cfg_aer_support = 1;
			rc = strlen(buf);
		}
		break;
	default:
		rc = -EINVAL;
		break;
	}
	return rc;
}

static int lpfc_aer_support = 1;
module_param(lpfc_aer_support, int, 1);
MODULE_PARM_DESC(lpfc_aer_support, "Enable PCIe device AER support");
lpfc_param_show(aer_support)

/**
 * lpfc_aer_support_init - Set the initial adapters aer support flag
 * @phba: lpfc_hba pointer.
 * @val: link speed value.
 *
 * Description:
 * If val is in a valid range [0,1], then set the adapter's initial
 * cfg_aer_support field. It will be up to the driver's probe_one
 * routine to determine whether the device's AER support can be set
 * or not.
 *
 * Notes:
 * If the value is not in range log a kernel error message, and
 * choose the default value of setting AER support and return.
 *
 * Returns:
 * zero if val saved.
 * -EINVAL val out of range
 **/
static int
lpfc_aer_support_init(struct lpfc_hba *phba, int val)
{
        if (val == 0 || val == 1) {
                phba->cfg_aer_support = val;
                return 0;
        }
        lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
                        "2712 lpfc_aer_support attribute value %d out "
                        "of range, allowed values are 0|1, setting it "
                        "to default value of 1\n", val);
        /* By default, try to enable AER on a device */
        phba->cfg_aer_support = 1;
        return -EINVAL;
}

static CLASS_DEVICE_ATTR(lpfc_aer_support, S_IRUGO | S_IWUSR,
			 lpfc_aer_support_show, lpfc_aer_support_store);

/**
 * lpfc_aer_cleanup_state - Clean up aer state to the aer enabled device
 * @dev: class device that is converted into a Scsi_host.
 * @attr: device attribute, not used.
 * @buf: containing the string "selective".
 * @count: unused variable.
 *
 * Description:
 * If the @buf contains 1 and the device currently has the AER support
 * enabled, then invokes the kernel AER helper routine
 * pci_cleanup_aer_uncorrect_error_status to clean up the uncorrectable
 * error status register.
 *
 * Notes:
 *
 * Returns:
 * -EINVAL if the buf does not contain the 1 or the device is not currently
 * enabled with the AER support.
 **/
static ssize_t
lpfc_aer_cleanup_state(struct class_device *dev, const char *buf, size_t count)
{
	struct Scsi_Host  *shost = class_to_shost(dev);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	int val, rc = -1;

	if (!isdigit(buf[0]))
		return -EINVAL;
	if (sscanf(buf, "%i", &val) != 1)
		return -EINVAL;
	if (val != 1)
		return -EINVAL;

	if (phba->hba_flag & HBA_AER_ENABLED)
		rc = pci_cleanup_aer_uncorrect_error_status(phba->pcidev);

	if (rc == 0)
		return strlen(buf);
	else
		return -EPERM;
}

static CLASS_DEVICE_ATTR(lpfc_aer_state_cleanup, S_IWUSR, NULL,
			 lpfc_aer_cleanup_state);
#endif /* __VMKLNX__ */

/*
# lpfc_fcp_class:  Determines FC class to use for the FCP protocol.
# Value range is [2,3]. Default value is 3.
*/
LPFC_VPORT_ATTR_R(fcp_class, 3, 2, 3, 
	CFG_EXPORT  | CFG_COMMON | CFG_THIS_HBA, CFG_REBOOT,
	"fcp_class",
	"Select Fibre Channel class of service for FCP sequences");

uint32_t
lpfc_use_adisc_show(struct lpfc_vport *vport)
{
	return vport->cfg_use_adisc;
}

int
lpfc_use_adisc_set(struct lpfc_vport *vport, uint32_t val)
{
	int i;
	struct lpfc_vport **vports = lpfc_create_vport_work_array(vport->phba);
	if (vports != NULL)
		/* Set each active vport with new use_adisc value */
		for (i = 0; i <= vport->phba->max_vports && vports[i] != NULL; i++)
			vports[i]->cfg_use_adisc = val;
		lpfc_destroy_vport_work_array(vport->phba, vports);
		return 0;
}

/*
# lpfc_use_adisc: Use ADISC for FCP rediscovery instead of PLOGI. Value range
# is [0,1]. Default value is 0.
*/
LPFC_VPORT_ATTR(use_adisc, 0, 0, 1,
	CFG_EXPORT | CFG_COMMON | CFG_THIS_HBA, CFG_DYNAMIC,
	"use_adisc",
	"Use ADISC on rediscovery to authenticate FCP devices",
	lpfc_use_adisc_show,
	lpfc_use_adisc_set);

uint32_t
lpfc_max_scsicmpl_time_show(struct lpfc_vport *vport)
{
	return vport->cfg_max_scsicmpl_time;
}

int
lpfc_max_scsicmpl_time_set(struct lpfc_vport *vport, uint32_t val)
{
	int i = 0;
	struct lpfc_vport **vports = lpfc_create_vport_work_array(vport->phba);
	struct Scsi_Host  *shost;
	struct lpfc_nodelist *ndlp, *next_ndlp;

	if (val == vport->cfg_max_scsicmpl_time)
		return 0;
	if ((val < 0) || (val > 60000))
		return -EINVAL;

	if (vports != NULL) {
		/* Set each active vport with new tgt_queue_depth value */
		for (; i <= vport->phba->max_vports && vports[i] != NULL; i++) {
			vports[i]->cfg_max_scsicmpl_time = val;
			shost = lpfc_shost_from_vport(vports[i]);

			spin_lock_irq(shost->host_lock);
			list_for_each_entry_safe(ndlp, next_ndlp,
						 &vports[i]->fc_nodes,
						 nlp_listp) {
				if (lpfc_nlp_get(ndlp)) {
					if (ndlp->nlp_state ==
							NLP_STE_MAPPED_NODE)
						ndlp->cmd_qdepth =
						     vports[i]->
							cfg_tgt_queue_depth;
					lpfc_nlp_put(ndlp);
				}
			}
			spin_unlock_irq(shost->host_lock);
		}
	}
	lpfc_destroy_vport_work_array(vport->phba, vports);
	return 0;
}

/*
# lpfc_max_scsicmpl_time: Use scsi command completion time to control I/O queue
# depth. Default value is 0. When this parameter has value zero the SCSI command
# completion time is not used for controlling I/O queue depth. When the
# parameter is set to a non-zero value, the I/O queue depth is controlled
# to limit the I/O completion time to the parameter value.
# The value is set in milliseconds.
*/
LPFC_VPORT_ATTR(max_scsicmpl_time, 0, 0, 60000,
		CFG_EXPORT | CFG_COMMON | CFG_THIS_HBA, CFG_DYNAMIC,
		"max_scsicmpl_time",
		"Use command completion time to control queue depth",
		lpfc_max_scsicmpl_time_show,
		lpfc_max_scsicmpl_time_set);

/*
# lpfc_ack0: Use ACK0, instead of ACK1 for class 2 acknowledgement. Value
# range is [0,1]. Default value is 0.
*/
LPFC_ATTR_R(ack0, 0, 0, 1, 
	CFG_EXPORT | CFG_COMMON | CFG_THIS_HBA, CFG_REBOOT,
	"ack0", 
	"Enable ACK0 support");

/*
# lpfc_cr_delay & lpfc_cr_count: Default values for I/O colaesing
# cr_delay (msec) or cr_count outstanding commands. cr_delay can take
# value [0,63]. cr_count can take value [1,255]. Default value of cr_delay
# is 0. Default value of cr_count is 1. The cr_count feature is disabled if
# cr_delay is set to 0.
*/
LPFC_ATTR_RW(cr_delay, 0, 0, 63, 
	CFG_IGNORE | CFG_THIS_HBA | CFG_LP, CFG_REBOOT,
	"cr_delay",
	"A count of milliseconds after which an interrupt response is generated");

LPFC_ATTR_RW(cr_count, 1, 1, 255, 
	CFG_IGNORE | CFG_THIS_HBA | CFG_LP, CFG_REBOOT,
	"cr_count",
	"A count of I/O completions after which an interrupt response is"
	" generated");

/*
# lpfc_multi_ring_support:  Determines how many rings to spread available
# cmd/rsp IOCB entries across.
# Value range is [1,2]. Default value is 1.
*/
LPFC_ATTR_R(multi_ring_support, 1, 1, 2, 
	CFG_IGNORE, CFG_REBOOT,
	"multi_ring_support",
	"SLI rings to spread IOCB entries across");

/*
# lpfc_multi_ring_rctl:  If lpfc_multi_ring_support is enabled, this
# identifies what rctl value to configure the additional ring for.
# Value range is [1,0xff]. Default value is 4 (Unsolicated Data).
*/
LPFC_ATTR_R(multi_ring_rctl, FC_RCTL_DD_UNSOL_DATA, 1, 255,
	    CFG_IGNORE, CFG_REBOOT,
	    "multi_ring_rctl",
	    "Identifies RCTL for additional ring configuration");

/*
# lpfc_multi_ring_type:  If lpfc_multi_ring_support is enabled, this
# identifies what type value to configure the additional ring for.
# Value range is [1,0xff]. Default value is 5 (LLC/SNAP).
*/
LPFC_ATTR_R(multi_ring_type, FC_TYPE_IP, 1, 255,
	    CFG_IGNORE, CFG_REBOOT,
	    "multi_ring_type",
	    "Identifies TYPE for additional ring configuration");

/*
# lpfc_fdmi_on: controls FDMI support.
#       0 = no FDMI support
#       1 = support FDMI without attribute of hostname
#       2 = support FDMI with attribute of hostname
# Value range [0,2]. Default value is 0.
*/
LPFC_VPORT_ATTR_R(fdmi_on, 0, 0, 2,
	CFG_EXPORT | CFG_COMMON | CFG_THIS_HBA, CFG_REBOOT,
	"fdmi_on",
	"Enable FDMI support");

/*
# lpfc_discovery_threads: Specifies the maximum number of ELS cmds we can 
# have outstanding (for discovery). 
# Value range is [1,64]. Default value = 32.
*/
LPFC_VPORT_ATTR_R(discovery_threads, 32, 1, 64,
	CFG_EXPORT | CFG_COMMON | CFG_THIS_HBA, CFG_REBOOT,
	"discovery_threads",
	"Maximum number of ELS commands during discovery");

/*
# lpfc_max_luns: maximum allowed LUN.
# Value range is [0,65535]. Default value is 256.
# NOTE: The SCSI layer might probe all allowed LUN on some old targets.
*/
LPFC_VPORT_ATTR_R(max_luns, 256, 0, 65535, 
	CFG_IGNORE | CFG_COMMON | CFG_THIS_HBA, CFG_REBOOT,
	"max_luns",
	"Maximum allowed LUN");

/*
# lpfc_poll_tmo: .Milliseconds driver will wait between polling FCP ring.
# Value range is [1,255], default value is 10.
*/
LPFC_ATTR_RW(poll_tmo, 10, 1, 255, 
	CFG_IGNORE | CFG_COMMON | CFG_THIS_HBA, CFG_REBOOT,
	"poll_tmo",
	"Milliseconds driver will wait between polling FCP ring");

/*
# lpfc_use_msi: Use MSI (Message Signaled Interrupts) in systems that
#		support this feature
#       0  = MSI disabled (default)
#       1  = MSI enabled
#	2  = MSI-X enabled
# Value range is [0,2]. Default value is 2.
*/
LPFC_ATTR_R(use_msi, LPFC_USE_MSIX, LPFC_USE_INTX, LPFC_USE_MSIX,
	CFG_EXPORT | CFG_THIS_HBA | CFG_SLI4, CFG_REBOOT,
	"use_msi",
	"Use preferred MSI-X interrupt mode if possible");


/*
# lpfc_use_mq: Use MultiQueue Kernel API in systems that support this feature
#       0  = Disable MQ.
#       1  = Enable MQ.
# Value range is [0,1]. Default value is 1.
*/
LPFC_ATTR_R(use_mq, LPFC_ENABLE_MQ, LPFC_DISABLE_MQ, LPFC_ENABLE_MQ,
	CFG_EXPORT | CFG_THIS_HBA | CFG_SLI4, CFG_REBOOT,
	"use_mq",
	"Use ESX MultiQueue feature if possible");

/*
# lpfc_enable_npiv: Enable NPIV functionality.
#       0  = NPIV functionality disabled.
#       1  = NPIV functionality enabled(default).
# Value range is [0,1]. Default value is 1.
*/
LPFC_ATTR_R(enable_npiv, 1, 0, 1,
	    CFG_EXPORT | CFG_COMMON | CFG_THIS_HBA, CFG_REBOOT,
	    "enable_npiv",
	    "Enable NPIV functionality.");

int lpfc_get_enable_npiv(void)
{
	return lpfc_enable_npiv;
}

int
lpfc_vport_enable_auth_set(struct lpfc_vport *vport, uint32_t val)
{
	if (val == vport->cfg_enable_auth)
		return 0;
	if (val == 0) {
		spin_lock_irq(&fc_security_user_lock);
		list_del(&vport->sc_users);
		spin_unlock_irq(&fc_security_user_lock);
		vport->cfg_enable_auth = val;
		lpfc_fc_queue_security_work(vport,
					    &vport->sc_offline_work);
		return 0;
	} else if (val == 1) {
		spin_lock_irq(&fc_security_user_lock);
		list_add_tail(&vport->sc_users, &fc_security_user_list);
		spin_unlock_irq(&fc_security_user_lock);
		vport->cfg_enable_auth = val;
		lpfc_fc_queue_security_work(vport,
					    &vport->sc_online_work);
		return 0;
	}
	lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT,
			 "0426 lpfc_enable_auth attribute cannot be set to %d, "
			 "allowed range is [0, 1]\n", val);
	return ERANGE;
}

/*
# lpfc_enable_auth: controls FC Authentication.
#       0 = Authentication OFF
#       1 = Authentication ON
# Value range [0,1]. Default value is 0.
*/
LPFC_VPORT_ATTR(enable_auth, 0, 0, 1,
        CFG_IGNORE, CFG_REBOOT,
        "enable_auth",
        "Enable FC Authentication",
	0, lpfc_vport_enable_auth_set);


/*
# lpfc_fcp_imax: Set the maximum number of fast-path FCP interrupts per second
#
# Value range is [636,651042]. Default value is 10000.
*/
LPFC_ATTR_R(fcp_imax, LPFC_FP_DEF_IMAX, LPFC_MIM_IMAX, LPFC_DMULT_CONST,
	    CFG_IGNORE | CFG_THIS_HBA | CFG_SLI4, CFG_REBOOT,
	    "fcp_imax",
	    "Set the maximum number of fast-path FCP interrupts per second");

/*
# lpfc_fcp_wq_count: Set the number of fast-path FCP work queues
#
# Value range is [1,31]. Default value is 4.
*/
LPFC_ATTR_R(fcp_wq_count, LPFC_FP_WQN_DEF, LPFC_FP_WQN_MIN, LPFC_FP_WQN_MAX,
	    CFG_EXPORT | CFG_THIS_HBA | CFG_SLI4, CFG_REBOOT,
	    "fcp_wq_count",
	    "Set the number of fast-path FCP work queues, if possible");

/*
# lpfc_fcp_eq_count: Set the number of fast-path FCP event queues
#
# Value range is [1,7]. Default value is 4.
*/
LPFC_ATTR_R(fcp_eq_count, LPFC_FP_EQN_DEF, LPFC_FP_EQN_MIN, LPFC_FP_EQN_MAX,
	    CFG_EXPORT | CFG_THIS_HBA | CFG_SLI4, CFG_REBOOT,
	    "fcp_eq_count",
	    "Set the number of fast-path FCP event queues, if possible");

/*
# lpfc_enable_hba_reset: Allow or prevent HBA resets to the hardware.
#       0  = HBA resets disabled
#       1  = HBA resets enabled (default)
# Value range is [0,1]. Default value is 1.
*/
LPFC_ATTR_R(enable_hba_reset, 1, 0, 1, 
	CFG_IGNORE | CFG_COMMON | CFG_THIS_HBA, CFG_REBOOT,
	"enable_hba_reset",
	"Enable HBA resets from the driver.");

/*
# lpfc_enable_hba_heartbeat: Enable HBA heartbeat timer..
#       0  = HBA Heartbeat disabled
#       1  = HBA Heartbeat enabled (default)
# Value range is [0,1]. Default value is 0.
*/
LPFC_ATTR_R(enable_hba_heartbeat, 0, 0, 1, 
	CFG_IGNORE | CFG_COMMON | CFG_THIS_HBA, CFG_REBOOT,
	"enable_hba_heartbeat",
	"Enable HBA Heartbeat.");

uint32_t
lpfc_sg_seg_cnt_show(struct lpfc_hba *phba)
{
	if (phba->cfg_sg_seg_cnt > LPFC_MAX_SG_SEG_CNT)
		return LPFC_MAX_SG_SEG_CNT;
	else
		return phba->cfg_sg_seg_cnt;
}

/*
 * lpfc_sg_seg_cnt: Initial Maximum DMA Segment Count
 * This value can be set to values between 64 and 256. The default value is
 * 64, but may be increased to allow for larger Max I/O sizes. The scsi layer
 * will be allowed to request I/Os of sizes up to (MAX_SEG_COUNT * SEG_SIZE).
 */
LPFC_ATTR(sg_seg_cnt, LPFC_DEFAULT_SG_SEG_CNT, LPFC_DEFAULT_SG_SEG_CNT,
	LPFC_MAX_SG_SEG_CNT,
	CFG_EXPORT | CFG_COMMON | CFG_THIS_HBA, CFG_REBOOT,
	"sg_seg_cnt",
	"Max Scatter Gather Segment Count",
	lpfc_sg_seg_cnt_show, 0);

uint32_t
lpfc_pci_max_read_show(struct lpfc_hba *phba)
{
	return phba->cfg_pci_max_read;
}

void
lpfc_validate_pci_max_read(struct lpfc_hba *phba)
{
	uint32_t val = phba->cfg_pci_max_read;
	if ((val == 0) || (val == 512) || (val == 1024) ||
	    (val == 2048) || (val == 4096)) {
		phba->cfg_pci_max_read = val;
	} else {
		phba->cfg_pci_max_read = 1024;
	}
}

int
lpfc_pci_max_read_set(struct lpfc_hba *phba, uint32_t val)
{
	uint32_t prev_val;
	int ret;

	prev_val = phba->cfg_pci_max_read;
	if ((val == 0) || (val == 512) || (val == 1024) ||
	    (val == 2048) || (val == 4096)) {
		phba->cfg_pci_max_read = val;
		if ((ret = lpfc_sli_set_dma_length(phba, 0)))
			phba->cfg_pci_max_read = prev_val;
		return ret;
	}

	return EINVAL;
}

/*
# lpfc_pci_max_read:  Maximum DMA read byte count. This parameter can have
# values 512, 1024, 2048, 4096. Default value is 0 meaning automatically
# determine by JEDEC_ID.
*/
LPFC_ATTR(pci_max_read, 0, 0, 4096,
	CFG_EXPORT | CFG_THIS_HBA, CFG_REBOOT,
        "pci_max_read",
        "Maximum DMA read byte count. Allowed values: 0,512,1024,2048,4096",
        lpfc_pci_max_read_show, lpfc_pci_max_read_set);

/*
 * Dynamic FC Host Attributes Support
 */

static void
lpfc_get_host_port_id(struct Scsi_Host *shost)
{
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;

	/* note: fc_myDID already in cpu endianness */
	fc_host_port_id(shost) = vport->fc_myDID;
}

static void
lpfc_get_host_port_type(struct Scsi_Host *shost)
{
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;

	spin_lock_irq(shost->host_lock);

	if (vport->port_type == LPFC_NPIV_PORT) {
#ifdef fc_host_vports
		if (vport->vport_flag & STATIC_VPORT)
			fc_host_port_type(shost) = FC_PORTTYPE_NPORT;
		else
			fc_host_port_type(shost) = FC_PORTTYPE_NPIV;
#else
		fc_host_port_type(shost) = FC_PORTTYPE_NPORT;
#endif
	} else if (lpfc_is_link_up(phba)) {
		if (phba->fc_topology == TOPOLOGY_LOOP) {
			if (vport->fc_flag & FC_PUBLIC_LOOP)
				fc_host_port_type(shost) = FC_PORTTYPE_NLPORT;
			else
				fc_host_port_type(shost) = FC_PORTTYPE_LPORT;
		} else {
			if (vport->fc_flag & FC_FABRIC)
				fc_host_port_type(shost) = FC_PORTTYPE_NPORT;
			else
				fc_host_port_type(shost) = FC_PORTTYPE_PTP;
		}
	} else
		fc_host_port_type(shost) = FC_PORTTYPE_UNKNOWN;

	spin_unlock_irq(shost->host_lock);
}

static void
lpfc_get_host_port_state(struct Scsi_Host *shost)
{
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;

	spin_lock_irq(shost->host_lock);

	if (vport->fc_flag & FC_OFFLINE_MODE)
		fc_host_port_state(shost) = FC_PORTSTATE_OFFLINE;
	else {
		switch (phba->link_state) {
		case LPFC_LINK_UNKNOWN:
		case LPFC_LINK_DOWN:
			fc_host_port_state(shost) = FC_PORTSTATE_LINKDOWN;
			break;
		case LPFC_LINK_UP:
		case LPFC_CLEAR_LA:
		case LPFC_HBA_READY:
			/* Links up, reports port state accordingly */
			if (vport->port_state < LPFC_VPORT_READY)
				fc_host_port_state(shost) =
							FC_PORTSTATE_BYPASSED;
			else
				fc_host_port_state(shost) =
							FC_PORTSTATE_ONLINE;
			break;
		case LPFC_HBA_ERROR:
			fc_host_port_state(shost) = FC_PORTSTATE_ERROR;
			break;
		default:
			fc_host_port_state(shost) = FC_PORTSTATE_UNKNOWN;
			break;
		}
	}

	spin_unlock_irq(shost->host_lock);
}

static void
lpfc_get_host_speed(struct Scsi_Host *shost)
{
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;

	spin_lock_irq(shost->host_lock);

	if (lpfc_is_link_up(phba)) {
		switch(phba->fc_linkspeed) {
		case LA_1GHZ_LINK:
			fc_host_speed(shost) = FC_PORTSPEED_1GBIT;
			break;
		case LA_2GHZ_LINK:
			fc_host_speed(shost) = FC_PORTSPEED_2GBIT;
			break;
		case LA_4GHZ_LINK:
			fc_host_speed(shost) = FC_PORTSPEED_4GBIT;
			break;
		case LA_8GHZ_LINK:
			fc_host_speed(shost) = FC_PORTSPEED_8GBIT;
			break;
		case LA_10GHZ_LINK:
			fc_host_speed(shost) = FC_PORTSPEED_10GBIT;
			break;
		default:
			fc_host_speed(shost) = FC_PORTSPEED_UNKNOWN;
			break;
		}
	} else
		fc_host_speed(shost) = FC_PORTSPEED_UNKNOWN;

	spin_unlock_irq(shost->host_lock);
}

static void
lpfc_get_host_fabric_name (struct Scsi_Host *shost)
{
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	u64 node_name;

	spin_lock_irq(shost->host_lock);

	if ((vport->fc_flag & FC_FABRIC) ||
	    ((phba->fc_topology == TOPOLOGY_LOOP) &&
	     (vport->fc_flag & FC_PUBLIC_LOOP)))
		node_name = wwn_to_u64(phba->fc_fabparam.nodeName.u.wwn);
	else
		/* fabric is local port if there is no F/FL_Port */
		node_name = 0;

	spin_unlock_irq(shost->host_lock);

	fc_host_fabric_name(shost) = node_name;
}

struct fc_host_statistics *
lpfc_get_stats(struct Scsi_Host *shost)
{
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	struct lpfc_sli   *psli = &phba->sli;
	struct fc_host_statistics *hs = &phba->link_stats;
	struct lpfc_lnk_stat * lso = &psli->lnk_stat_offsets;
	LPFC_MBOXQ_t *pmboxq;
	MAILBOX_t *pmb;
	unsigned long seconds;
	int rc = 0;

	/*
	 * prevent udev from issuing mailbox commands until the port is
	 * configured.
	 */
	if (phba->link_state < LPFC_LINK_DOWN ||
	    !phba->mbox_mem_pool ||
	    (phba->sli.sli_flag & LPFC_SLI_ACTIVE) == 0)
		return NULL;

	if (phba->sli.sli_flag & LPFC_BLOCK_MGMT_IO)
		return NULL;

	pmboxq = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmboxq)
		return NULL;
	memset(pmboxq, 0, sizeof (LPFC_MBOXQ_t));

	pmb = &pmboxq->u.mb;
	pmb->mbxCommand = MBX_READ_STATUS;
	pmb->mbxOwner = OWN_HOST;
	pmboxq->context1 = NULL;
	pmboxq->vport = vport;

	if (vport->fc_flag & FC_OFFLINE_MODE)
		rc = lpfc_sli_issue_mbox(phba, pmboxq, MBX_POLL);
	else
		rc = lpfc_sli_issue_mbox_wait(phba, pmboxq, phba->fc_ratov * 2);

	if (rc != MBX_SUCCESS) {
		if (rc != MBX_TIMEOUT)
			mempool_free(pmboxq, phba->mbox_mem_pool);
		return NULL;
	}

	memset(hs, 0, sizeof (struct fc_host_statistics));

	hs->tx_frames = pmb->un.varRdStatus.xmitFrameCnt;
	hs->tx_words = (pmb->un.varRdStatus.xmitByteCnt * 256);
	hs->rx_frames = pmb->un.varRdStatus.rcvFrameCnt;
	hs->rx_words = (pmb->un.varRdStatus.rcvByteCnt * 256);

	memset(pmboxq, 0, sizeof (LPFC_MBOXQ_t));
	pmb->mbxCommand = MBX_READ_LNK_STAT;
	pmb->mbxOwner = OWN_HOST;
	pmboxq->context1 = NULL;
	pmboxq->vport = vport;

	if (vport->fc_flag & FC_OFFLINE_MODE)
		rc = lpfc_sli_issue_mbox(phba, pmboxq, MBX_POLL);
	else
		rc = lpfc_sli_issue_mbox_wait(phba, pmboxq, phba->fc_ratov * 2);

	if (rc != MBX_SUCCESS) {
		if (rc != MBX_TIMEOUT)
			mempool_free(pmboxq, phba->mbox_mem_pool);
		return NULL;
	}

	hs->link_failure_count = pmb->un.varRdLnk.linkFailureCnt;
	hs->loss_of_sync_count = pmb->un.varRdLnk.lossSyncCnt;
	hs->loss_of_signal_count = pmb->un.varRdLnk.lossSignalCnt;
	hs->prim_seq_protocol_err_count = pmb->un.varRdLnk.primSeqErrCnt;
	hs->invalid_tx_word_count = pmb->un.varRdLnk.invalidXmitWord;
	hs->invalid_crc_count = pmb->un.varRdLnk.crcCnt;
	hs->error_frames = pmb->un.varRdLnk.crcCnt;

	hs->link_failure_count -= lso->link_failure_count;
	hs->loss_of_sync_count -= lso->loss_of_sync_count;
	hs->loss_of_signal_count -= lso->loss_of_signal_count;
	hs->prim_seq_protocol_err_count -= lso->prim_seq_protocol_err_count;
	hs->invalid_tx_word_count -= lso->invalid_tx_word_count;
	hs->invalid_crc_count -= lso->invalid_crc_count;
	hs->error_frames -= lso->error_frames;

	if (phba->hba_flag & HBA_FCOE_SUPPORT) {
		hs->lip_count = -1;
		hs->nos_count = (phba->link_events >> 1);
		hs->nos_count -= lso->link_events;
	} else if (phba->fc_topology == TOPOLOGY_LOOP) {
		hs->lip_count = (phba->fc_eventTag >> 1);
		hs->lip_count -= lso->link_events;
		hs->nos_count = -1;
	} else {
		hs->lip_count = -1;
		hs->nos_count = (phba->fc_eventTag >> 1);
		hs->nos_count -= lso->link_events;
	}

	hs->dumped_frames = -1;

	seconds = get_seconds();
	if (seconds < psli->stats_start)
		hs->seconds_since_last_reset = seconds +
				((unsigned long)-1 - psli->stats_start);
	else
		hs->seconds_since_last_reset = seconds - psli->stats_start;

	mempool_free(pmboxq, phba->mbox_mem_pool);

	return hs;
}

static void
lpfc_reset_stats(struct Scsi_Host *shost)
{
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = vport->phba;
	struct lpfc_sli   *psli = &phba->sli;
	struct lpfc_lnk_stat *lso = &psli->lnk_stat_offsets;
	LPFC_MBOXQ_t *pmboxq;
	MAILBOX_t *pmb;
	int rc = 0;

	if (phba->sli.sli_flag & LPFC_BLOCK_MGMT_IO)
		return;

	pmboxq = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmboxq)
		return;
	memset(pmboxq, 0, sizeof(LPFC_MBOXQ_t));

	pmb = &pmboxq->u.mb;
	pmb->mbxCommand = MBX_READ_STATUS;
	pmb->mbxOwner = OWN_HOST;
	pmb->un.varWords[0] = 0x1; /* reset request */
	pmboxq->context1 = NULL;
	pmboxq->vport = vport;

	if ((vport->fc_flag & FC_OFFLINE_MODE) ||
		(!(psli->sli_flag & LPFC_SLI_ACTIVE)))
		rc = lpfc_sli_issue_mbox(phba, pmboxq, MBX_POLL);
	else
		rc = lpfc_sli_issue_mbox_wait(phba, pmboxq, phba->fc_ratov * 2);

	if (rc != MBX_SUCCESS) {
		if (rc != MBX_TIMEOUT)
			mempool_free(pmboxq, phba->mbox_mem_pool);
		return;
	}

	memset(pmboxq, 0, sizeof(LPFC_MBOXQ_t));
	pmb->mbxCommand = MBX_READ_LNK_STAT;
	pmb->mbxOwner = OWN_HOST;
	pmboxq->context1 = NULL;
	pmboxq->vport = vport;

	if ((vport->fc_flag & FC_OFFLINE_MODE) ||
	    (!(psli->sli_flag & LPFC_SLI_ACTIVE)))
		rc = lpfc_sli_issue_mbox(phba, pmboxq, MBX_POLL);
	else
		rc = lpfc_sli_issue_mbox_wait(phba, pmboxq, phba->fc_ratov * 2);

	if (rc != MBX_SUCCESS) {
		if (rc != MBX_TIMEOUT)
			mempool_free( pmboxq, phba->mbox_mem_pool);
		return;
	}

	lso->link_failure_count = pmb->un.varRdLnk.linkFailureCnt;
	lso->loss_of_sync_count = pmb->un.varRdLnk.lossSyncCnt;
	lso->loss_of_signal_count = pmb->un.varRdLnk.lossSignalCnt;
	lso->prim_seq_protocol_err_count = pmb->un.varRdLnk.primSeqErrCnt;
	lso->invalid_tx_word_count = pmb->un.varRdLnk.invalidXmitWord;
	lso->invalid_crc_count = pmb->un.varRdLnk.crcCnt;
	lso->error_frames = pmb->un.varRdLnk.crcCnt;
	if (phba->hba_flag & HBA_FCOE_SUPPORT)
		lso->link_events = (phba->link_events >> 1);
	else
		lso->link_events = (phba->fc_eventTag >> 1);

	psli->stats_start = get_seconds();

	mempool_free(pmboxq, phba->mbox_mem_pool);

	return;
}

/*
 * The LPFC driver treats linkdown handling as target loss events so there
 * are no sysfs handlers for link_down_tmo.
 */

static struct lpfc_nodelist *
lpfc_get_node_by_target(struct scsi_target *starget)
{
	struct Scsi_Host  *shost = dev_to_shost(starget->dev.parent);
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_nodelist *ndlp;

	spin_lock_irq(shost->host_lock);
	/* Search for this, mapped, target ID */
	list_for_each_entry(ndlp, &vport->fc_nodes, nlp_listp) {
		if (NLP_CHK_NODE_ACT(ndlp) &&
		    ndlp->nlp_state == NLP_STE_MAPPED_NODE &&
		    starget->id == ndlp->nlp_sid) {
			spin_unlock_irq(shost->host_lock);
			return ndlp;
		}
	}
	spin_unlock_irq(shost->host_lock);
	return NULL;
}

static void
lpfc_get_starget_port_id(struct scsi_target *starget)
{
	struct lpfc_nodelist *ndlp = lpfc_get_node_by_target(starget);

	fc_starget_port_id(starget) = ndlp ? ndlp->nlp_DID : -1;
}

static void
lpfc_get_starget_node_name(struct scsi_target *starget)
{
	struct lpfc_nodelist *ndlp = lpfc_get_node_by_target(starget);

	fc_starget_node_name(starget) =
		ndlp ? wwn_to_u64(ndlp->nlp_nodename.u.wwn) : 0;
}

static void
lpfc_get_starget_port_name(struct scsi_target *starget)
{
	struct lpfc_nodelist *ndlp = lpfc_get_node_by_target(starget);

	fc_starget_port_name(starget) =
		ndlp ? wwn_to_u64(ndlp->nlp_portname.u.wwn) : 0;
}

static void
lpfc_set_rport_loss_tmo(struct fc_rport *rport, uint32_t timeout)
{
	if (timeout)
		rport->dev_loss_tmo = timeout;
	else
		rport->dev_loss_tmo = 1;
}


#define lpfc_rport_show_function(field, format_string, sz, cast)	\
static ssize_t								\
lpfc_show_rport_##field (struct class_device *cdev, char *buf)		\
{									\
	struct fc_rport *rport = transport_class_to_rport(cdev);	\
	struct lpfc_rport_data *rdata = rport->hostdata;		\
	return snprintf(buf, sz, format_string,				\
		(rdata->target) ? cast rdata->target->field : 0);	\
}

#define lpfc_rport_rd_attr(field, format_string, sz)			\
	lpfc_rport_show_function(field, format_string, sz, )		\
static FC_RPORT_ATTR(field, S_IRUGO, lpfc_show_rport_##field, NULL)

struct class_device_attribute *lpfc_hba_attrs[] = {
	&class_device_attr_max_vpi,
	&class_device_attr_used_vpi,
	&class_device_attr_npiv_info,
	NULL,
};

struct class_device_attribute *lpfc_vport_attrs[] = {
        &class_device_attr_npiv_info,
        NULL,
};

struct fc_function_template lpfc_transport_functions = {
	/* fixed attributes the driver supports */
	.show_host_node_name = 1,
	.show_host_port_name = 1,
	.show_host_supported_classes = 1,
	.show_host_supported_fc4s = 1,
	.show_host_supported_speeds = 1,
	.show_host_maxframe_size = 1,

	/* dynamic attributes the driver supports */
	.get_host_port_id = lpfc_get_host_port_id,
	.show_host_port_id = 1,

	.get_host_port_type = lpfc_get_host_port_type,
	.show_host_port_type = 1,

	.get_host_port_state = lpfc_get_host_port_state,
	.show_host_port_state = 1,

	/* active_fc4s is shown but doesn't change (thus no get function) */
	.show_host_active_fc4s = 1,

	.get_host_speed = lpfc_get_host_speed,
	.show_host_speed = 1,

	.get_host_fabric_name = lpfc_get_host_fabric_name,
	.show_host_fabric_name = 1,

	/*
	 * The LPFC driver treats linkdown handling as target loss events
	 * so there are no sysfs handlers for link_down_tmo.
	 */

	.get_fc_host_stats = lpfc_get_stats,
	.reset_fc_host_stats = lpfc_reset_stats,

	.dd_fcrport_size = sizeof(struct lpfc_rport_data),
	.show_rport_maxframe_size = 1,
	.show_rport_supported_classes = 1,

	.set_rport_dev_loss_tmo = lpfc_set_rport_loss_tmo,
	.show_rport_dev_loss_tmo = 1,

	.get_starget_port_id  = lpfc_get_starget_port_id,
	.show_starget_port_id = 1,

	.get_starget_node_name = lpfc_get_starget_node_name,
	.show_starget_node_name = 1,

	.get_starget_port_name = lpfc_get_starget_port_name,
	.show_starget_port_name = 1,

	.issue_fc_host_lip = lpfc_issue_lip,
	.dev_loss_tmo_callbk = lpfc_dev_loss_tmo_callbk,
	.terminate_rport_io = lpfc_terminate_rport_io,

#ifdef fc_host_vports
	.dd_fcvport_size = sizeof(struct lpfc_vport *),
#endif
};

struct fc_function_template lpfc_vport_transport_functions = {
	/* fixed attributes the driver supports */
	.show_host_node_name = 1,
	.show_host_port_name = 1,
	.show_host_supported_classes = 1,
	.show_host_supported_fc4s = 1,
	.show_host_supported_speeds = 1,
	.show_host_maxframe_size = 1,

	/* dynamic attributes the driver supports */
	.get_host_port_id = lpfc_get_host_port_id,
	.show_host_port_id = 1,

	.get_host_port_type = lpfc_get_host_port_type,
	.show_host_port_type = 1,

	.get_host_port_state = lpfc_get_host_port_state,
	.show_host_port_state = 1,

	/* active_fc4s is shown but doesn't change (thus no get function) */
	.show_host_active_fc4s = 1,

	.get_host_speed = lpfc_get_host_speed,
	.show_host_speed = 1,

	.get_host_fabric_name = lpfc_get_host_fabric_name,
	.show_host_fabric_name = 1,

	/*
	 * The LPFC driver treats linkdown handling as target loss events
	 * so there are no sysfs handlers for link_down_tmo.
	 */

	.get_fc_host_stats = lpfc_get_stats,
	.reset_fc_host_stats = lpfc_reset_stats,

	.dd_fcrport_size = sizeof(struct lpfc_rport_data),
	.show_rport_maxframe_size = 1,
	.show_rport_supported_classes = 1,

	.set_rport_dev_loss_tmo = lpfc_set_rport_loss_tmo,
	.show_rport_dev_loss_tmo = 1,

	.get_starget_port_id  = lpfc_get_starget_port_id,
	.show_starget_port_id = 1,

	.get_starget_node_name = lpfc_get_starget_node_name,
	.show_starget_node_name = 1,

	.get_starget_port_name = lpfc_get_starget_port_name,
	.show_starget_port_name = 1,

	.dev_loss_tmo_callbk = lpfc_dev_loss_tmo_callbk,
	.terminate_rport_io = lpfc_terminate_rport_io,

#ifdef fc_host_vports
	.vport_disable = lpfc_vport_disable,
#endif
};

void
lpfc_get_cfgparam(struct lpfc_hba *phba)
{
	lpfc_cr_delay_init(phba, lpfc_cr_delay);
	lpfc_cr_count_init(phba, lpfc_cr_count);
	lpfc_multi_ring_support_init(phba, lpfc_multi_ring_support);
	lpfc_multi_ring_rctl_init(phba, lpfc_multi_ring_rctl);
	lpfc_multi_ring_type_init(phba, lpfc_multi_ring_type);
	lpfc_ack0_init(phba, lpfc_ack0);
	lpfc_topology_init(phba, lpfc_topology);
	lpfc_pci_max_read_init(phba, lpfc_pci_max_read);
	lpfc_validate_pci_max_read(phba);
	lpfc_link_speed_init(phba, lpfc_link_speed);
	lpfc_poll_tmo_init(phba, lpfc_poll_tmo);
	lpfc_use_msi_init(phba, lpfc_use_msi);
	lpfc_use_mq_init(phba, lpfc_use_mq);
	lpfc_fcp_imax_init(phba, lpfc_fcp_imax);
	lpfc_fcp_wq_count_init(phba, lpfc_fcp_wq_count);
	lpfc_fcp_eq_count_init(phba, lpfc_fcp_eq_count);
	lpfc_enable_hba_reset_init(phba, lpfc_enable_hba_reset);
	lpfc_enable_hba_heartbeat_init(phba, lpfc_enable_hba_heartbeat);
	lpfc_sli_mode_init(phba, lpfc_sli_mode);
	lpfc_poll_init(phba, lpfc_poll);
	lpfc_poll_freq_init(phba, lpfc_poll_freq);
	lpfc_enable_npiv_init(phba, lpfc_enable_npiv);
	phba->cfg_soft_wwnn = 0L;
	phba->cfg_soft_wwpn = 0L;
	lpfc_sg_seg_cnt_init(phba, lpfc_sg_seg_cnt);
	lpfc_iocb_cnt_init(phba, lpfc_iocb_cnt);

	/* These driver parameters have holes in there valid range. Set
	 * these values to their defauls */

	if (lpfc_topology_set(phba, phba->cfg_topology))
		phba->cfg_topology = 0;
	if (lpfc_link_speed_set(phba, phba->cfg_link_speed))
		phba->cfg_link_speed = 0;

	/* Also reinitialize the host templates with new values. */
	lpfc_vport_template.sg_tablesize = phba->cfg_sg_seg_cnt;
	lpfc_template.sg_tablesize = phba->cfg_sg_seg_cnt;

	/*
	 * Since the sg_tablesize is module parameter, the sg_dma_buf_size
	 * used to create the sg_dma_buf_pool must be dynamically calculated.
	 * 2 segments are added since the IOCB needs a command and response bde.
	 */
	phba->cfg_sg_dma_buf_size = sizeof(struct fcp_cmnd) +
			sizeof(struct fcp_rsp) +
			((phba->cfg_sg_seg_cnt + 2) * sizeof(struct ulp_bde64));
	lpfc_hba_queue_depth_init(phba, lpfc_hba_queue_depth);

	phba->cfg_poll = 0;
}

void
lpfc_get_vport_cfgparam(struct lpfc_vport *vport)
{
	lpfc_vport_log_verbose_init(vport, lpfc_log_verbose);
	lpfc_vport_lun_queue_depth_init(vport, lpfc_lun_queue_depth);
	lpfc_vport_tgt_queue_depth_init(vport, lpfc_tgt_queue_depth);
	lpfc_vport_devloss_tmo_init(vport, lpfc_devloss_tmo);
	lpfc_vport_peer_port_login_init(vport, lpfc_peer_port_login);
	lpfc_vport_restrict_login_init(vport, lpfc_restrict_login);
	lpfc_vport_validate_restrict_login(vport);
	lpfc_vport_fcp_class_init(vport, lpfc_fcp_class);
	lpfc_vport_use_adisc_init(vport, lpfc_use_adisc);
	lpfc_vport_max_scsicmpl_time_init(vport, lpfc_max_scsicmpl_time);
	lpfc_vport_fdmi_on_init(vport, lpfc_fdmi_on);
	lpfc_vport_discovery_threads_init(vport, lpfc_discovery_threads);
	lpfc_vport_max_luns_init(vport, lpfc_max_luns);
	lpfc_vport_scan_down_init(vport, lpfc_scan_down);
	lpfc_vport_enable_da_id_init(vport, lpfc_enable_da_id);
	if (vport->phba->sli_rev != LPFC_SLI_REV4)
		lpfc_vport_enable_auth_init(vport, lpfc_enable_auth);
	else
		lpfc_vport_enable_auth_init(vport, 0);
	return;
}
