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

#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/idr.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/pci.h>
#include <linux/spinlock.h>

#include <scsi/scsi.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_transport_fc.h>
#include "lpfc_hw4.h"
#include "lpfc_hw.h"
#include "lpfc_sli.h"
#include "lpfc_sli4.h"
#include "lpfc_nl.h"
#include "lpfc_disc.h"
#include "lpfc_scsi.h"
#include "lpfc.h"
#include "lpfc_logmsg.h"
#include "hbaapi.h"
#include "lpfc_ioctl.h"
#include "lpfc_crtn.h"
#include "lpfc_version.h"
#include "lpfc_vport.h"
#include "lpfc_auth_access.h"

static int
__lpfc_do_vport_create(struct Scsi_Host *, struct fc_vport *, bool,
		       struct lpfc_vport **, uint8_t *, uint8_t *,
		       uint8_t *);

inline void lpfc_vport_set_state(struct lpfc_vport *vport,
				 enum fc_vport_state new_state)
{
#ifdef fc_host_vports
	struct fc_vport *fc_vport = vport->fc_vport;

	if (fc_vport) {
		/*
		 * When the transport defines fc_vport_set state we will replace
		 * this code with the following line
		 */
		/* fc_vport_set_state(fc_vport, new_state); */
		if (new_state != FC_VPORT_INITIALIZING)
			fc_vport->vport_last_state = fc_vport->vport_state;
		fc_vport->vport_state = new_state;
	}
#endif

	/* for all the error states we will set the invternal state to FAILED */
	switch (new_state) {
	case FC_VPORT_NO_FABRIC_SUPP:
	case FC_VPORT_NO_FABRIC_RSCS:
	case FC_VPORT_FABRIC_LOGOUT:
	case FC_VPORT_FABRIC_REJ_WWN:
	case FC_VPORT_FAILED:
		vport->port_state = LPFC_VPORT_FAILED;
		break;
	case FC_VPORT_LINKDOWN:
		vport->port_state = LPFC_VPORT_UNKNOWN;
		break;
	default:
		/* do nothing */
		break;
	}
}

static int
lpfc_alloc_vpi(struct lpfc_hba *phba)
{
	int  vpi;

	spin_lock_irq(&phba->hbalock);
	/* Start at bit 1 because vpi zero is reserved for the physical port */
	vpi = find_next_zero_bit(phba->vpi_bmask, (phba->max_vpi + 1), 1);
	if (vpi > phba->max_vpi)
		vpi = 0;
	else
		set_bit(vpi, phba->vpi_bmask);
	if (phba->sli_rev == LPFC_SLI_REV4)
		phba->sli4_hba.max_cfg_param.vpi_used++;
	spin_unlock_irq(&phba->hbalock);
	return vpi;
}

static void
lpfc_free_vpi(struct lpfc_hba *phba, int vpi)
{
	if (vpi == 0)
		return;
	spin_lock_irq(&phba->hbalock);
	clear_bit(vpi, phba->vpi_bmask);
	if (phba->sli_rev == LPFC_SLI_REV4)
		phba->sli4_hba.max_cfg_param.vpi_used--;
	spin_unlock_irq(&phba->hbalock);
}

static int
lpfc_vport_sparm(struct lpfc_hba *phba, struct lpfc_vport *vport)
{
	LPFC_MBOXQ_t *pmb;
	MAILBOX_t *mb;
	struct lpfc_dmabuf *mp;
	int  rc;

	pmb = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb) {
		return -ENOMEM;
	}
	mb = &pmb->u.mb;

	rc = lpfc_read_sparam(phba, pmb, vport->vpi);
	if (rc) {
		mempool_free(pmb, phba->mbox_mem_pool);
		return -ENOMEM;
	}

	/*
	 * Grab buffer pointer and clear context1 so we can use
	 * lpfc_sli_issue_box_wait
	 */
	mp = (struct lpfc_dmabuf *) pmb->context1;
	pmb->context1 = NULL;

	pmb->vport = vport;
	rc = lpfc_sli_issue_mbox_wait(phba, pmb, phba->fc_ratov * 2);
	if (rc != MBX_SUCCESS) {
		if (signal_pending(current)) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT | LOG_VPORT,
					 "1830 Signal aborted mbxCmd x%x\n",
					 mb->mbxCommand);
			lpfc_mbuf_free(phba, mp->virt, mp->phys);
			kfree(mp);
			if (rc != MBX_TIMEOUT)
				mempool_free(pmb, phba->mbox_mem_pool);
			return -EINTR;
		} else {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_INIT | LOG_VPORT,
					 "1818 VPort failed init, mbxCmd x%x "
					 "READ_SPARM mbxStatus x%x, rc = x%x\n",
					 mb->mbxCommand, mb->mbxStatus, rc);
			lpfc_mbuf_free(phba, mp->virt, mp->phys);
			kfree(mp);
			if (rc != MBX_TIMEOUT)
				mempool_free(pmb, phba->mbox_mem_pool);
			return -EIO;
		}
	}

	memcpy(&vport->fc_sparam, mp->virt, sizeof (struct serv_parm));
	memcpy(&vport->fc_nodename, &vport->fc_sparam.nodeName,
	       sizeof (struct lpfc_name));
	memcpy(&vport->fc_portname, &vport->fc_sparam.portName,
	       sizeof (struct lpfc_name));

	lpfc_mbuf_free(phba, mp->virt, mp->phys);
	kfree(mp);
	mempool_free(pmb, phba->mbox_mem_pool);

	return 0;
}

static int
lpfc_valid_wwn_format(struct lpfc_hba *phba, struct lpfc_name *wwn,
		      const char *name_type)
{
				/* ensure that IEEE format 1 addresses
				 * contain zeros in bits 59-48
				 */
	if (!((wwn->u.wwn[0] >> 4) == 1 &&
	      ((wwn->u.wwn[0] & 0xf) != 0 || (wwn->u.wwn[1] & 0xf) != 0)))
		return 1;

	lpfc_printf_log(phba, KERN_ERR, LOG_VPORT,
			"1822 Invalid %s: %02x:%02x:%02x:%02x:"
			"%02x:%02x:%02x:%02x\n",
			name_type,
			wwn->u.wwn[0], wwn->u.wwn[1],
			wwn->u.wwn[2], wwn->u.wwn[3],
			wwn->u.wwn[4], wwn->u.wwn[5],
			wwn->u.wwn[6], wwn->u.wwn[7]);
	return 0;
}

static int
lpfc_unique_wwpn(struct lpfc_hba *phba, struct lpfc_vport *new_vport)
{
	struct lpfc_vport *vport;
	unsigned long flags;

	spin_lock_irqsave(&phba->hbalock, flags);
	list_for_each_entry(vport, &phba->port_list, listentry) {
		if (vport == new_vport)
			continue;
		/* If they match, return not unique */
		if (memcmp(&vport->fc_sparam.portName,
			   &new_vport->fc_sparam.portName,
			   sizeof(struct lpfc_name)) == 0) {
			spin_unlock_irqrestore(&phba->hbalock, flags);
			return 0;
		}
	}
	spin_unlock_irqrestore(&phba->hbalock, flags);
	return 1;
}
/**
 * lpfc_discovery_in_progress - Is Nport discovery in progress.
 * @vport: The virtual port for which this call is being executed.
 *
 * This function iterate through all NPorts to check if there is
 * any Nport discovery is in progress. If all Nports completed
 * discovery it return 0 else return 1.
 **/
static int
lpfc_discovery_in_progress(struct lpfc_vport *vport)
{
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);
	struct lpfc_nodelist *ndlp;

	spin_lock_irq(shost->host_lock);
	list_for_each_entry(ndlp, &vport->fc_nodes, nlp_listp) {
		switch (ndlp->nlp_state) {
		case NLP_STE_REG_LOGIN_ISSUE:
		case NLP_STE_PRLI_ISSUE:
		case NLP_STE_PLOGI_ISSUE:
		case NLP_STE_ADISC_ISSUE:
			spin_unlock_irq(shost->host_lock);
			return 1;
		default:
			continue;

		}
	}
	spin_unlock_irq(shost->host_lock);
	return 0;
}

/**
 * lpfc_discovery_wait - Wait for driver discovery to quiesce
 * @vport: The virtual port for which this call is being executed.
 *
 * This driver calls this routine specifically from lpfc_vport_delete
 * to enforce a synchronous execution of vport
 * delete relative to discovery activities.  The
 * lpfc_vport_delete routine should not return until it
 * can reasonably guarantee that discovery has quiesced.
 * Post FDISC LOGO, the driver must wait until its SAN teardown is
 * complete and all resources recovered before allowing
 * cleanup.
 *
 * This routine does not require any locks held.
 **/
static void lpfc_discovery_wait(struct lpfc_vport *vport)
{
	struct lpfc_hba *phba = vport->phba;
	uint32_t wait_flags = 0;
	unsigned long wait_time_max;
	unsigned long start_time;

	wait_flags = FC_RSCN_MODE | FC_RSCN_DISCOVERY | FC_NLP_MORE |
		     FC_RSCN_DEFERRED | FC_NDISC_ACTIVE | FC_DISC_TMO;

	/*
	 * The time constraint on this loop is a balance between the
	 * fabric RA_TOV value and dev_loss tmo.  The driver's
	 * devloss_tmo is 10 giving this loop a 3x multiplier minimally.
	 */
	wait_time_max = msecs_to_jiffies(((phba->fc_ratov * 3) + 3) * 1000);
	wait_time_max += jiffies;
	start_time = jiffies;
	while (time_before(jiffies, wait_time_max)) {
		if ((vport->num_disc_nodes > 0)    ||
		    (vport->fc_flag & wait_flags)  ||
		    lpfc_discovery_in_progress(vport) ||
		    ((vport->port_state > LPFC_VPORT_FAILED) &&
		     (vport->port_state < LPFC_VPORT_READY))) {
			lpfc_printf_vlog(vport, KERN_INFO, LOG_VPORT,
					"1833 Vport discovery quiesce Wait:"
					" state x%x fc_flags x%x"
					" num_nodes x%x, waiting 1000 msecs"
					" total wait msecs x%x\n",
					vport->port_state, vport->fc_flag,
					vport->num_disc_nodes,
					jiffies_to_msecs(jiffies - start_time));
			msleep(1000);
		} else {
			/* Base case.  Wait variants satisfied.  Break out */
			lpfc_printf_vlog(vport, KERN_INFO, LOG_VPORT,
					 "1834 Vport discovery quiesced:"
					 " state x%x fc_flags x%x"
					 " wait msecs x%x\n",
					 vport->port_state, vport->fc_flag,
					 jiffies_to_msecs(jiffies
						- start_time));
			break;
		}
	}

	if (time_after(jiffies, wait_time_max))
		lpfc_printf_vlog(vport, KERN_ERR, LOG_VPORT,
				"1835 Vport discovery quiesce failed:"
				" state x%x fc_flags x%x wait msecs x%x\n",
				vport->port_state, vport->fc_flag,
				jiffies_to_msecs(jiffies - start_time));
}

/*
 * This is a vmkernel NPIV entry point function.  Vmkernel calls this
 * routine to create a vport on behalf of a virtual machine.
 */
int
lpfc_vport_create(struct fc_vport *fc_vport, bool disable)
{
	struct Scsi_Host *shost = fc_vport->shost;
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
	int rc;

	if (vport->vport_flag & STATIC_VPORT)
		return VPORT_ERROR;

	rc = __lpfc_do_vport_create(shost, fc_vport, disable, NULL,
				    fc_vport->symbolic_name,
				    NULL, NULL);
	if (rc == VPORT_OK) {
		if (vport->port_state != LPFC_VPORT_READY)
			lpfc_discovery_wait(vport);
	}

	return rc;
}

/*
 * The driver calls this routine to create virtual hba ports
 * during probe.  The WWN pairs come from the Emulex BIOS.
 * The scsi host binding gets created as if this is an hba
 * port.
 */
int
__lpfc_vport_create(struct Scsi_Host *shost, uint8_t *wwnn,
		  uint8_t *wwpn, char *vname,
		  struct lpfc_vport **new_vport)
{
	int rc;
	rc = __lpfc_do_vport_create(shost, NULL, false, new_vport,
				    vname, wwnn, wwpn);
	return rc;
}

static int
__lpfc_do_vport_create(struct Scsi_Host *shost, struct fc_vport *fc_vport,
		       bool disable, struct lpfc_vport **new_vport,
		       uint8_t *vname, uint8_t *wwnn, uint8_t *wwpn)
{
	struct lpfc_nodelist *ndlp;
	struct lpfc_vport *pport = (struct lpfc_vport *) shost->hostdata;
	struct lpfc_hba   *phba = pport->phba;
	struct lpfc_vport *vport = NULL;
	int instance;
	int vpi;
	int rc = VPORT_ERROR;
	int status;
	int size;

	if (new_vport)
		*new_vport = NULL;

	if ((phba->sli_rev < LPFC_SLI_REV3) || !(phba->cfg_enable_npiv)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_VPORT,
				"1808 Create VPORT failed: "
				"NPIV is not enabled: SLImode:%d\n",
				phba->sli_rev);
		rc = VPORT_INVAL;
		goto error_out;
	}

	vpi = lpfc_alloc_vpi(phba);
	if (vpi == 0) {
		lpfc_printf_log(phba, KERN_ERR, LOG_VPORT,
				"1809 Create VPORT failed: "
				"Max VPORTs (%d) exceeded\n",
				phba->max_vpi);
		rc = VPORT_NORESOURCES;
		goto error_out;
	}

	/* Assign an unused board number */
	instance = lpfc_get_shost_instance();
	if (instance < 0) {
		lpfc_printf_log(phba, KERN_ERR, LOG_VPORT,
				"1810 Create VPORT failed: Cannot get "
				"instance number\n");
		lpfc_free_vpi(phba, vpi);
		rc = VPORT_NORESOURCES;
		goto error_out;
	}

#ifndef fc_host_vports
	vport = lpfc_create_port(phba, instance, &shost->shost_gendev);
#else
	if (fc_vport)
		vport = lpfc_create_port(phba, instance, &fc_vport->dev);
	else
		vport = lpfc_create_port(phba, instance, &phba->pcidev->dev);
#endif
	if (!vport) {
		lpfc_printf_log(phba, KERN_ERR, LOG_VPORT,
				"1811 Create VPORT failed: vpi x%x\n", vpi);
		lpfc_free_vpi(phba, vpi);
		rc = VPORT_NORESOURCES;
		goto error_out;
	}
	vport->vpi = vpi;

#ifndef fc_host_vports
	vport->port_type = LPFC_NPIV_PORT;
#endif

	/*
	 * The port_type needs to be overwritten now.  If the static Vport
	 * logic is calling, overwrite the port type set by lpfc_create_port
	 * to account for a static vport on the physical pcidev.
	 */
	if (new_vport)
		vport->port_type = LPFC_NPIV_PORT;

	/* vport inherits driver parameter values from physical port */
	vport->cfg_log_verbose = phba->pport->cfg_log_verbose;
	vport->cfg_devloss_tmo = phba->pport->cfg_devloss_tmo;
	vport->cfg_max_scsicmpl_time = phba->pport->cfg_max_scsicmpl_time;
	vport->cfg_use_adisc = phba->pport->cfg_use_adisc;
	/*
	 * Override the vport's restrict-login.  The physical port needs to
	 * login to all ports, but vports do not to conserve rpis.
	 */
	vport->cfg_restrict_login = 1;
	lpfc_debugfs_initialize(vport);

	/*
	 * Read the service parameters up from the Port.  Use the WWNs
	 * from this operation as the default WWN for the new vport.
	 * If vmkernel is calling and has defined its own WWNs, override
	 * the service parameters.  Similarly for npiv-boot.
	 */
	if ((status = lpfc_vport_sparm(phba, vport))) {
		if (status == -EINTR) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_VPORT,
					 "1831 Create VPORT Interrupted.\n");
			rc = VPORT_ERROR;
		} else {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_VPORT,
					 "1813 Create VPORT failed. "
					 "Cannot get sparam\n");
			rc = VPORT_NORESOURCES;
		}
		lpfc_free_vpi(phba, vpi);
		destroy_port(vport);
		goto error_out;
	}

	memcpy(vport->fc_portname.u.wwn, vport->fc_sparam.portName.u.wwn, 8);
	memcpy(vport->fc_nodename.u.wwn, vport->fc_sparam.nodeName.u.wwn, 8);
	size = strnlen(vname, LPFC_VNAME_LEN);
	if (size) {
		vport->vname = kzalloc(size+1, GFP_KERNEL);
		if (!vport->vname) {
			lpfc_printf_vlog(vport, KERN_ERR, LOG_VPORT,
					 "1839 Create VPORT failed. "
					 "vname allocation failed.\n");
			rc = VPORT_ERROR;
			lpfc_free_vpi(phba, vpi);
			destroy_port(vport);
			goto error_out;
		}
		memcpy(vport->vname, vname, size+1);
	}

#ifndef fc_host_vports
	if (wwnn && memcmp(wwnn, null_name, 8))
		memcpy(vport->fc_nodename.u.wwn, wwnn, 8);
	if (wwpn && memcmp(wwpn, null_name, 8))
		memcpy(vport->fc_portname.u.wwn, wwpn, 8);
#else
	if (fc_vport) {
		if (fc_vport->node_name != 0)
			u64_to_wwn(fc_vport->node_name,
				   vport->fc_nodename.u.wwn);
		if (fc_vport->port_name != 0)
			u64_to_wwn(fc_vport->port_name,
				   vport->fc_portname.u.wwn);
	} else {
		if (wwnn)
			memcpy(vport->fc_nodename.u.wwn, wwnn, 8);
		if (wwpn)
			memcpy(vport->fc_portname.u.wwn, wwpn, 8);
	}
#endif

	memcpy(&vport->fc_sparam.portName, vport->fc_portname.u.wwn, 8);
	memcpy(&vport->fc_sparam.nodeName, vport->fc_nodename.u.wwn, 8);

	if (!lpfc_valid_wwn_format(phba, &vport->fc_sparam.nodeName, "WWNN") ||
	    !lpfc_valid_wwn_format(phba, &vport->fc_sparam.portName, "WWPN")) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_VPORT,
				 "1821 Create VPORT failed. "
				 "Invalid WWN format\n");
		lpfc_free_vpi(phba, vpi);
		destroy_port(vport);
		rc = VPORT_INVAL;
		goto error_out;
	}

	if (!lpfc_unique_wwpn(phba, vport)) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_VPORT,
				 "1823 Create VPORT failed. "
				 "Duplicate WWN on HBA\n");
		lpfc_free_vpi(phba, vpi);
		destroy_port(vport);
		rc = VPORT_INVAL;
		goto error_out;
	}

	shost = lpfc_shost_from_vport(vport);

	if ((lpfc_get_security_enabled)(shost)){
		spin_lock_irq(&fc_security_user_lock);
		list_add_tail(&vport->sc_users, &fc_security_user_list);
		spin_unlock_irq(&fc_security_user_lock);
		if (fc_service_state == FC_SC_SERVICESTATE_ONLINE) {
			lpfc_fc_queue_security_work(vport,
				&vport->sc_online_work);
		}
	}

	/* Create binary sysfs attribute for vport */
#if !defined(__VMKLNX__)
	lpfc_alloc_sysfs_attr(vport);
#endif

	if (fc_vport) {
		*(struct lpfc_vport **)fc_vport->dd_data = vport;
		vport->fc_vport = fc_vport;
	} else {
		if (new_vport)
			*new_vport = vport;
	}

	/*
	 * In SLI4, the vpi must be activated before it can be used
	 * by the port.
	 */
	if ((phba->sli_rev == LPFC_SLI_REV4) &&
		(pport->fc_flag & FC_VFI_REGISTERED)) {
		rc = lpfc_sli4_init_vpi(phba, vpi);
		if (rc) {
			lpfc_printf_log(phba, KERN_ERR, LOG_VPORT,
					"1838 Failed to INIT_VPI on vpi %d "
					"status %d\n", vpi, rc);
			rc = VPORT_NORESOURCES;
			lpfc_free_vpi(phba, vpi);
			goto error_out;
		}
	} else if (phba->sli_rev == LPFC_SLI_REV4) {
		/*
		 * Driver cannot INIT_VPI now. Set the flags to
		 * init_vpi when reg_vfi complete.
		 */
		vport->fc_flag |= FC_VPORT_NEEDS_INIT_VPI;
		lpfc_vport_set_state(vport, FC_VPORT_LINKDOWN);
		rc = VPORT_OK;
		goto out;
	}

	if ((phba->link_state < LPFC_LINK_UP) ||
	    (pport->port_state < LPFC_FABRIC_CFG_LINK) ||
	    (phba->fc_topology == TOPOLOGY_LOOP)) {
		lpfc_vport_set_state(vport, FC_VPORT_LINKDOWN);
		rc = VPORT_OK;
		goto out;
	}

	if (disable) {
		rc = VPORT_OK;
		goto out;
	}

	/* Use the Physical nodes Fabric NDLP to determine if the link is
	 * up and ready to FDISC.
	 */
	ndlp = lpfc_findnode_did(pport, Fabric_DID);
	if (ndlp && NLP_CHK_NODE_ACT(ndlp) &&
	    ndlp->nlp_state == NLP_STE_UNMAPPED_NODE) {
		if (phba->link_flag & LS_NPIV_FAB_SUPPORTED) {
			lpfc_set_disctmo(vport);
			lpfc_initial_fdisc(vport);
		} else {
			lpfc_vport_set_state(vport, FC_VPORT_NO_FABRIC_SUPP);
			lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
					 "0262 No NPIV Fabric support\n");
		}
	} else {
		lpfc_vport_set_state(vport, FC_VPORT_FAILED);
	}
	rc = VPORT_OK;

out:
	lpfc_printf_vlog(vport, KERN_ERR, LOG_VPORT,
			"1825 Vport Created.\n");
	lpfc_host_attrib_init(lpfc_shost_from_vport(vport));
error_out:
	return rc;
}

#ifdef fc_host_vports
static int
disable_vport(struct fc_vport *fc_vport)
{
	struct lpfc_vport *vport = *(struct lpfc_vport **)fc_vport->dd_data;
	struct lpfc_hba   *phba = vport->phba;
	struct lpfc_nodelist *ndlp = NULL, *next_ndlp = NULL;
	long timeout;

	ndlp = lpfc_findnode_did(vport, Fabric_DID);
	if (ndlp && NLP_CHK_NODE_ACT(ndlp)
	    && phba->link_state >= LPFC_LINK_UP) {
		vport->unreg_vpi_cmpl = VPORT_INVAL;
		timeout = msecs_to_jiffies(phba->fc_ratov * 2000);
		if (!lpfc_issue_els_npiv_logo(vport, ndlp))
			while (vport->unreg_vpi_cmpl == VPORT_INVAL && timeout)
				timeout = schedule_timeout(timeout);
	}

	lpfc_sli_host_down(vport);

	/* Mark all nodes for discovery so we can remove them by
	 * calling lpfc_cleanup_rpis(vport, 1)
	 */
	list_for_each_entry_safe(ndlp, next_ndlp, &vport->fc_nodes, nlp_listp) {
		if (!NLP_CHK_NODE_ACT(ndlp))
			continue;
		if (ndlp->nlp_state == NLP_STE_UNUSED_NODE)
			continue;
		lpfc_disc_state_machine(vport, ndlp, NULL,
					NLP_EVT_DEVICE_RECOVERY);
	}
	lpfc_cleanup_rpis(vport, 1);

	lpfc_stop_vport_timers(vport);
	lpfc_unreg_all_rpis(vport);
	lpfc_unreg_default_rpis(vport);
	/*
	 * Completion of unreg_vpi (lpfc_mbx_cmpl_unreg_vpi) does the
	 * scsi_host_put() to release the vport.
	 */
	lpfc_mbx_unreg_vpi(vport);

	lpfc_vport_set_state(vport, FC_VPORT_DISABLED);
	lpfc_printf_vlog(vport, KERN_ERR, LOG_VPORT,
			 "1826 Vport Disabled.\n");
	return VPORT_OK;
}

static int
enable_vport(struct fc_vport *fc_vport)
{
	struct lpfc_vport *vport = *(struct lpfc_vport **)fc_vport->dd_data;
	struct lpfc_hba   *phba = vport->phba;
	struct lpfc_nodelist *ndlp = NULL;
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);

	if ((phba->link_state < LPFC_LINK_UP) ||
	    (phba->fc_topology == TOPOLOGY_LOOP)) {
		lpfc_vport_set_state(vport, FC_VPORT_LINKDOWN);
		return VPORT_OK;
	}

	spin_lock_irq(shost->host_lock);
	vport->load_flag |= FC_LOADING;
	vport->fc_flag |= FC_VPORT_NEEDS_REG_VPI;
	spin_unlock_irq(shost->host_lock);

	/* Use the Physical nodes Fabric NDLP to determine if the link is
	 * up and ready to FDISC.
	 */
	ndlp = lpfc_findnode_did(phba->pport, Fabric_DID);
	if (ndlp && NLP_CHK_NODE_ACT(ndlp)
	    && ndlp->nlp_state == NLP_STE_UNMAPPED_NODE) {
		if (phba->link_flag & LS_NPIV_FAB_SUPPORTED) {
			lpfc_set_disctmo(vport);
			lpfc_initial_fdisc(vport);
		} else {
			lpfc_vport_set_state(vport, FC_VPORT_NO_FABRIC_SUPP);
			lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
					 "0264 No NPIV Fabric support\n");
		}
	} else {
		lpfc_vport_set_state(vport, FC_VPORT_FAILED);
	}
	lpfc_printf_vlog(vport, KERN_ERR, LOG_VPORT,
			 "1827 Vport Enabled.\n");
	return VPORT_OK;
}

int
lpfc_vport_disable(struct fc_vport *fc_vport, bool disable)
{
	if (disable)
		return disable_vport(fc_vport);
	else
		return enable_vport(fc_vport);
}

#endif

int
#ifndef fc_host_vports
lpfc_vport_delete(struct Scsi_Host *shost)
#else
lpfc_vport_delete(struct fc_vport *fc_vport)
#endif
{
#ifndef fc_host_vports
	struct lpfc_vport *vport = (struct lpfc_vport *) shost->hostdata;
#else
	struct Scsi_Host *shost = (struct Scsi_Host *) fc_vport->shost;
	struct lpfc_vport *vport = *(struct lpfc_vport **)fc_vport->dd_data;
#endif
	return __lpfc_vport_delete(shost, vport);
}

int
__lpfc_vport_delete(struct Scsi_Host *shost, struct lpfc_vport *vport)
{
	struct lpfc_nodelist *ndlp = NULL, *next_ndlp = NULL;
	struct lpfc_hba   *phba = vport->phba;
	struct lpfc_sli_ring *pring;
	struct lpfc_iocbq *iocb;
	long timeout;
	uint32_t iocb_pending;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
	int i;
#endif

	if (vport->port_type == LPFC_PHYSICAL_PORT) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_VPORT,
				 "1812 vport_delete failed: Cannot delete "
				 "physical host\n");
		return VPORT_ERROR;
	}

	/* If the vport is a static vport fail the deletion. */
	if ((vport->vport_flag & STATIC_VPORT) &&
		!(phba->pport->load_flag & FC_UNLOADING)) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_VPORT,
				 "1837 vport_delete failed: Cannot delete "
				 "static vport.\n");
		return VPORT_ERROR;
	}
	spin_lock_irq(&phba->hbalock);
	vport->load_flag |= FC_UNLOADING;
	spin_unlock_irq(&phba->hbalock);
	/*
	 * If we are not unloading the driver then prevent the vport_delete
	 * from happening until after this vport's discovery is finished.
	 */
	if (!(phba->pport->load_flag & FC_UNLOADING)) {
		int check_count = 0;
		while (check_count < ((phba->fc_ratov * 3) + 3) &&
		       vport->port_state > LPFC_VPORT_FAILED &&
		       vport->port_state < LPFC_VPORT_READY) {
			check_count++;
			msleep(1000);
		}
		if (vport->port_state > LPFC_VPORT_FAILED &&
		    vport->port_state < LPFC_VPORT_READY)
			return -EAGAIN;
	}
	/*
	 * This is a bit of a mess.  We want to ensure the shost doesn't get
	 * torn down until we're done with the embedded lpfc_vport structure.
	 *
	 * Beyond holding a reference for this function, we also need a
	 * reference for outstanding I/O requests we schedule during delete
	 * processing.  But once we scsi_remove_host() we can no longer obtain
	 * a reference through scsi_host_get().
	 *
	 * So we take two references here.  We release one reference at the
	 * bottom of the function -- after delinking the vport.  And we
	 * release the other at the completion of the unreg_vpi that get's
	 * initiated after we've disposed of all other resources associated
	 * with the port.
	 */
	if (!scsi_host_get(shost))
		return VPORT_INVAL;
	if (!scsi_host_get(shost)) {
		scsi_host_put(shost);
		return VPORT_INVAL;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
	/* Determine whether we are in a proper state for the vport delete:
	 * In cases of linkdown or topology change to loop mode, the devloss
	 * timeout callback function from FC layer does not provide valid
	 * indication that SCSI layer has released all the SCSI targets on
	 * the vports. It takes the SCSI layer for its timeout before its
	 * releasing the SCSI targets. Only at the time that SCSI layer has
	 * released all the targets associated with the vport, it is safe
	 * for the driver to delete vports. The similar situation can happen
	 * when a target node dispear by itself.
	 *
	 * In case it is not in a proper state for vport delete, we will
	 * not delete the vport, re-test the proper state after 1 second.
	 * After all ndlp node getting to the proper state for vport delete,
	 * we proceed delete the vport. If, for a given ndlp node, after 30
	 * retries, it's still not in the proper state for vport delete, we
	 * log a message for the user to delete the vport later and fail the
	 * vport delete. Note that since the SCSI layer will time out all
	 * the dispearing target nodes at same time, the wait will properly
	 * done only on the first ndlp target node on the list.
	 */
	i = 0;
	list_for_each_entry_safe(ndlp, next_ndlp, &vport->fc_nodes, nlp_listp) {
		if (!NLP_CHK_NODE_ACT(ndlp))
			continue;
		while (ndlp->nlp_type & NLP_FCP_TARGET
		       && ndlp->nlp_state == NLP_STE_NPR_NODE) {
			if (++i > vport->cfg_devloss_tmo) {
				/* Log the message */
				lpfc_printf_vlog(vport, KERN_ERR, LOG_VPORT,
						"1840 Delete VPORT can not "
						"proceed at this time due "
						"to SCSI layer busy...\n");
				return VPORT_ERROR;
			}
			msleep(1000);
		}
	}
#endif

#if !defined(__VMKLNX__)
	lpfc_free_sysfs_attr(vport);
#endif
	kfree(vport->vname);
	lpfc_debugfs_terminate(vport);
	fc_remove_host(lpfc_shost_from_vport(vport));
	scsi_remove_host(lpfc_shost_from_vport(vport));

	ndlp = lpfc_findnode_did(phba->pport, Fabric_DID);

	/* In case of driver unload, we shall not perform fabric logo as the
	 * worker thread already stopped at this stage and, in this case, we
	 * can safely skip the fabric logo.
	 */
	if (phba->pport->load_flag & FC_UNLOADING) {
		if (ndlp && NLP_CHK_NODE_ACT(ndlp) &&
		    ndlp->nlp_state == NLP_STE_UNMAPPED_NODE &&
		    phba->link_state >= LPFC_LINK_UP) {
			/* First look for the Fabric ndlp */
			ndlp = lpfc_findnode_did(vport, Fabric_DID);
			if (!ndlp)
				goto skip_logo;
			else if (!NLP_CHK_NODE_ACT(ndlp)) {
				ndlp = lpfc_enable_node(vport, ndlp,
							NLP_STE_UNUSED_NODE);
				if (!ndlp)
					goto skip_logo;
			}
			/* Remove ndlp from vport npld list */
			lpfc_dequeue_node(vport, ndlp);

			/* Indicate free memory when release */
			spin_lock_irq(&phba->ndlp_lock);
			NLP_SET_FREE_REQ(ndlp);
			spin_unlock_irq(&phba->ndlp_lock);
			/* Kick off release ndlp when it can be safely done */
			lpfc_nlp_put(ndlp);
		}
		goto skip_logo;
	}

	/* Otherwise, we will perform fabric logo as needed */
	if (ndlp && NLP_CHK_NODE_ACT(ndlp) &&
	    ndlp->nlp_state == NLP_STE_UNMAPPED_NODE &&
	    phba->link_state >= LPFC_LINK_UP &&
	    phba->fc_topology != TOPOLOGY_LOOP) {
		if (vport->cfg_enable_da_id) {
			timeout = msecs_to_jiffies(phba->fc_ratov * 2000);
			if (!lpfc_ns_cmd(vport, SLI_CTNS_DA_ID, 0, 0))
				while (vport->ct_flags && timeout)
					timeout = schedule_timeout(timeout);
			else
				lpfc_printf_log(vport->phba, KERN_WARNING,
						LOG_VPORT,
						"1829 CT command failed to "
						"delete objects on fabric\n");
		}
		/* First look for the Fabric ndlp */
		ndlp = lpfc_findnode_did(vport, Fabric_DID);
		if (!ndlp) {
			/* Cannot find existing Fabric ndlp, allocate one */
			ndlp = mempool_alloc(phba->nlp_mem_pool, GFP_KERNEL);
			if (!ndlp)
				goto skip_logo;
			lpfc_nlp_init(vport, ndlp, Fabric_DID);
			/* Indicate free memory when release */
			spin_lock_irq(&phba->ndlp_lock);
			NLP_SET_FREE_REQ(ndlp);
			spin_unlock_irq(&phba->ndlp_lock);
		} else {
			if (!NLP_CHK_NODE_ACT(ndlp))
				ndlp = lpfc_enable_node(vport, ndlp,
						NLP_STE_UNUSED_NODE);
				if (!ndlp)
					goto skip_logo;

			/* Remove ndlp from vport npld list */
			lpfc_dequeue_node(vport, ndlp);
			spin_lock_irq(&phba->ndlp_lock);
			if (!NLP_CHK_FREE_REQ(ndlp))
				/* Indicate free memory when release */
				NLP_SET_FREE_REQ(ndlp);
			else {
				/* Skip this if ndlp is already in free mode */
				spin_unlock_irq(&phba->ndlp_lock);
				goto skip_logo;
			}
			spin_unlock_irq(&phba->ndlp_lock);
		}
		if (!(vport->vpi_state & LPFC_VPI_REGISTERED))
			goto skip_logo;
		vport->unreg_vpi_cmpl = VPORT_INVAL;
		timeout = msecs_to_jiffies(phba->fc_ratov * 2000);
		if (!lpfc_issue_els_npiv_logo(vport, ndlp))
			while (vport->unreg_vpi_cmpl == VPORT_INVAL && timeout)
				timeout = schedule_timeout(timeout);
	}

	if (!(phba->pport->load_flag & FC_UNLOADING))
		lpfc_discovery_wait(vport);

skip_logo:
	lpfc_cleanup(vport);
	lpfc_sli_host_down(vport);

	/* Wait for all IO on the FCP ring to drain for this vport. */
	pring = &phba->sli.ring[LPFC_FCP_RING];
	do {
		iocb_pending = 0;
		spin_lock_irq(&phba->hbalock);
		list_for_each_entry(iocb, &pring->txcmplq, list) {
			if (iocb->vport == vport)
				iocb_pending++;
		}
		spin_unlock_irq(&phba->hbalock);
		/*
		 * If there are iocbs pending on this vport, wait 10mS and
		 * then count again.  This value must go to zero before the
		 * vport delete can proceed.
		 */
		if (iocb_pending > 0)
			msleep(10);
	} while (iocb_pending != 0);

	lpfc_stop_vport_timers(vport);

	if (!(phba->pport->load_flag & FC_UNLOADING)) {
		lpfc_unreg_all_rpis(vport);
		lpfc_unreg_default_rpis(vport);
		/*
		 * Completion of unreg_vpi (lpfc_mbx_cmpl_unreg_vpi)
		 * does the scsi_host_put() to release the vport.
		 */
		if (lpfc_mbx_unreg_vpi(vport)) {
			/*
			 * Need to release the reference count to shost
			 */
			scsi_host_put(shost);
		}
	}
	else {
		/*
		 * Need to release the reference count to shost
		 */
		scsi_host_put(shost);
	}

	lpfc_free_vpi(phba, vport->vpi);
	vport->work_port_events = 0;
	spin_lock_irq(&phba->hbalock);
	list_del_init(&vport->listentry);
	spin_unlock_irq(&phba->hbalock);
	lpfc_printf_vlog(vport, KERN_ERR, LOG_VPORT,
			 "1828 Vport Deleted.\n");
	scsi_host_put(shost);
	return VPORT_OK;
}


struct lpfc_vport **
lpfc_create_vport_work_array(struct lpfc_hba *phba)
{
	struct lpfc_vport *port_iterator;
	struct lpfc_vport **vports;
	int index = 0;
	vports = kzalloc((phba->max_vports + 1) * sizeof(struct lpfc_vport *),
			 GFP_KERNEL);
	if (vports == NULL)
		return NULL;
	spin_lock_irq(&phba->hbalock);
	list_for_each_entry(port_iterator, &phba->port_list, listentry) {
		if (!scsi_host_get(lpfc_shost_from_vport(port_iterator))) {
			if (!(port_iterator->load_flag & FC_UNLOADING))
				lpfc_printf_vlog(port_iterator, KERN_ERR,
					 LOG_VPORT,
					 "1801 Create vport work array FAILED: "
					 "cannot do scsi_host_get\n");
			continue;
		}
		vports[index++] = port_iterator;
	}
	spin_unlock_irq(&phba->hbalock);
	return vports;
}

void
lpfc_destroy_vport_work_array(struct lpfc_hba *phba, struct lpfc_vport **vports)
{
	int i;
	if (vports == NULL)
		return;
	for (i = 0; i <= phba->max_vports && vports[i] != NULL; i++)
		scsi_host_put(lpfc_shost_from_vport(vports[i]));
	kfree(vports);
}


/**
 * lpfc_vport_reset_stat_data - Reset the statistical data for the vport
 * @vport: Pointer to vport object.
 *
 * This function resets the statistical data for the vport. This function
 * is called with the host_lock held
 **/
void
lpfc_vport_reset_stat_data(struct lpfc_vport *vport)
{
	struct lpfc_nodelist *ndlp = NULL, *next_ndlp = NULL;

	list_for_each_entry_safe(ndlp, next_ndlp, &vport->fc_nodes, nlp_listp) {
		if (!NLP_CHK_NODE_ACT(ndlp))
			continue;
		if (ndlp->lat_data)
			memset(ndlp->lat_data, 0, LPFC_MAX_BUCKET_COUNT *
				sizeof(struct lpfc_scsicmd_bkt));
	}
}


/**
 * lpfc_alloc_bucket - Allocate data buffer required for statistical data
 * @vport: Pointer to vport object.
 *
 * This function allocates data buffer required for all the FC
 * nodes of the vport to collect statistical data.
 **/
void
lpfc_alloc_bucket(struct lpfc_vport *vport)
{
	struct lpfc_nodelist *ndlp = NULL, *next_ndlp = NULL;

	list_for_each_entry_safe(ndlp, next_ndlp, &vport->fc_nodes, nlp_listp) {
		if (!NLP_CHK_NODE_ACT(ndlp))
			continue;

		kfree(ndlp->lat_data);
		ndlp->lat_data = NULL;

		if (ndlp->nlp_state == NLP_STE_MAPPED_NODE) {
			ndlp->lat_data = kcalloc(LPFC_MAX_BUCKET_COUNT,
					 sizeof(struct lpfc_scsicmd_bkt),
					 GFP_ATOMIC);

			if (!ndlp->lat_data)
				lpfc_printf_vlog(vport, KERN_ERR, LOG_NODE,
					"0287 lpfc_alloc_bucket failed to "
					"allocate statistical data buffer DID "
					"0x%x\n", ndlp->nlp_DID);
		}
	}
}

/**
 * lpfc_free_bucket - Free data buffer required for statistical data
 * @vport: Pointer to vport object.
 *
 * Th function frees statistical data buffer of all the FC
 * nodes of the vport.
 **/
void
lpfc_free_bucket(struct lpfc_vport *vport)
{
	struct lpfc_nodelist *ndlp = NULL, *next_ndlp = NULL;

	list_for_each_entry_safe(ndlp, next_ndlp, &vport->fc_nodes, nlp_listp) {
		if (!NLP_CHK_NODE_ACT(ndlp))
			continue;

		kfree(ndlp->lat_data);
		ndlp->lat_data = NULL;
	}
}
