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

#include <scsi/scsi.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_transport_fc.h>

#include "lpfc_version.h"
#include "lpfc_nl.h"
#include "lpfc_hw4.h"
#include "lpfc_hw.h"
#include "lpfc_compat.h"
#include "lpfc_sli.h"
#include "lpfc_sli4.h"
#include "lpfc_disc.h"
#include "lpfc.h"
#include "lpfc_logmsg.h"
#include "lpfc_scsi.h"
#include "hbaapi.h"
#include "lpfc_ioctl.h"
#include "lpfc_crtn.h"

/* Define forward prototypes. */
static int lpfc_ioctl_hba_adapterattributes(struct lpfc_hba *,
					    struct lpfcCmdInput *, void *);
static int lpfc_ioctl_hba_portattributes(struct lpfc_hba *,
					 struct lpfcCmdInput *, void *);
static int lpfc_ioctl_hba_portstatistics(struct lpfc_hba *,
					 struct lpfcCmdInput *, void *);
static int lpfc_ioctl_hba_wwpnportattributes(struct lpfc_hba *,
					     struct lpfcCmdInput *, void *);
static int lpfc_ioctl_hba_discportattributes(struct lpfc_hba *,
					     struct lpfcCmdInput *, void *);
static int lpfc_ioctl_hba_indexportattributes(struct lpfc_hba *,
					      struct lpfcCmdInput *, void *);
static int lpfc_ioctl_hba_setmgmtinfo(struct lpfc_hba *, struct lpfcCmdInput *);
static int lpfc_ioctl_hba_getmgmtinfo(struct lpfc_hba *, struct lpfcCmdInput *,
				      void *);
static int lpfc_ioctl_hba_refreshinfo(struct lpfc_hba *, struct lpfcCmdInput *,
				      void *);
static int lpfc_ioctl_hba_rnid(struct lpfc_hba *, struct lpfcCmdInput *,
			       void *);
static int lpfc_ioctl_hba_getevent(struct lpfc_hba *, struct lpfcCmdInput *,
				   void *);
static int lpfc_ioctl_hba_fcptargetmapping(struct lpfc_hba *,
					   struct lpfcCmdInput *, void *,
					   uint32_t);
static void lpfc_determine_port_type(struct lpfc_hba *, struct lpfc_nodelist *,
				     HBA_PORTATTRIBUTES *);
static int lpfc_ioctl_port_attrib(struct lpfc_hba *, void *);
static int lpfc_ioctl_found_port(struct lpfc_hba *, struct lpfc_nodelist *,
				 void *, HBA_PORTATTRIBUTES *);

int
lpfc_process_ioctl_hbaapi(struct lpfc_hba *phba, struct lpfcCmdInput * cip)
{
	int rc = -1;
	uint32_t total_mem;
	void   *dataout;
	
	/*
 	 * At one time, Linux kmalloc didn't allow an allocation from
 	 * kmalloc that exceeded 64KB.  That restriction does not apply
 	 * to vmkernel's memory manager allowing this code to be 
 	 * simplified.
 	 */
	if (cip->lpfc_outsz >= 4096)
		total_mem = cip->lpfc_outsz;	
	else
		total_mem = 4096;

	dataout = kmalloc(total_mem, GFP_KERNEL);
	if (!dataout)
		return ENOMEM;

	switch (cip->lpfc_cmd) {
	case LPFC_HBA_ADAPTERATTRIBUTES:
		rc = lpfc_ioctl_hba_adapterattributes(phba, cip, dataout);
		break;

	case LPFC_HBA_PORTATTRIBUTES:
		rc = lpfc_ioctl_hba_portattributes(phba, cip, dataout);
		break;

	case LPFC_HBA_PORTSTATISTICS:
		rc = lpfc_ioctl_hba_portstatistics(phba, cip, dataout);
		break;

	case LPFC_HBA_WWPNPORTATTRIBUTES:
		rc = lpfc_ioctl_hba_wwpnportattributes(phba, cip, dataout);
		break;

	case LPFC_HBA_DISCPORTATTRIBUTES:
		rc = lpfc_ioctl_hba_discportattributes(phba, cip, dataout);
		break;

	case LPFC_HBA_INDEXPORTATTRIBUTES:
		rc = lpfc_ioctl_hba_indexportattributes(phba, cip, dataout);
		break;

	case LPFC_HBA_SETMGMTINFO:
		rc = lpfc_ioctl_hba_setmgmtinfo(phba, cip);
		break;

	case LPFC_HBA_GETMGMTINFO:
		rc = lpfc_ioctl_hba_getmgmtinfo(phba, cip, dataout);
		break;

	case LPFC_HBA_REFRESHINFO:
		rc = lpfc_ioctl_hba_refreshinfo(phba, cip, dataout);
		break;

	case LPFC_HBA_RNID:
		rc = lpfc_ioctl_hba_rnid(phba, cip, dataout);
		break;

	case LPFC_HBA_GETEVENT:
		rc = lpfc_ioctl_hba_getevent(phba, cip, dataout);
		break;

	case LPFC_HBA_FCPTARGETMAPPING:
		rc = lpfc_ioctl_hba_fcptargetmapping(phba, cip, dataout,
						     total_mem);
		break;

	case LPFC_HBA_FCPBINDING:
		/* There is no persistent binding support in the driver. */
		rc = EPERM;
		break;
	}

	if (rc == 0) {
		if (cip->lpfc_outsz) {
			if (copy_to_user((uint8_t *) cip->lpfc_dataout,
					 (uint8_t *) dataout,
					 (int)cip->lpfc_outsz)) {
				rc = EIO;
			}
		}
	}

	kfree(dataout);
	return rc;
}

static int
lpfc_ioctl_hba_adapterattributes(struct lpfc_hba * phba,
				 struct lpfcCmdInput * cip, void *dataout)
{
	HBA_ADAPTERATTRIBUTES *ha;
	struct pci_dev *pdev;
	char *pNodeSymbolicName;
	char fwrev[32];
	uint32_t incr;
	lpfc_vpd_t *vp;
	int rc = 0;
	int i, j = 0;
	struct lpfc_vport *vport = phba->pport;

	pNodeSymbolicName = kmalloc(256, GFP_KERNEL);
	if (!pNodeSymbolicName)
		return ENOMEM;

	pdev = phba->pcidev;
	vp = &phba->vpd;
	ha = (HBA_ADAPTERATTRIBUTES *) dataout;
	memset(dataout, 0, (sizeof (HBA_ADAPTERATTRIBUTES)));
	ha->NumberOfPorts = 1;
	ha->VendorSpecificID = 
	    ((((uint32_t) pdev->device) << 16) | (uint32_t) (pdev->vendor));
	ha->VendorSpecificID = le32_to_cpu(ha->VendorSpecificID);
	sprintf(ha->DriverVersion, "%s", LPFC_DRIVER_VERSION);
	lpfc_decode_firmware_rev(phba, fwrev, 1);
	sprintf(ha->FirmwareVersion, "%s", fwrev);
	memcpy((uint8_t *) & ha->NodeWWN,
	       (uint8_t *) & vport->fc_sparam.nodeName, sizeof (HBA_WWN));
	sprintf(ha->Manufacturer, "%s", "Emulex Corporation");
	sprintf(ha->Model, "%s", phba->ModelName);
	sprintf(ha->ModelDescription, "%s", phba->ModelDesc);
	sprintf(ha->DriverName, "%s", LPFC_DRIVER_NAME);
	sprintf(ha->SerialNumber, "%s", phba->SerialNumber);
	sprintf(ha->OptionROMVersion, "%s", phba->OptionROMVersion);

	/* Convert JEDEC ID to ascii for hardware version */
	incr = vp->rev.biuRev;
	for (i = 0; i < 8; i++) {
		j = (incr & 0xf);
		if (j <= 9)
			ha->HardwareVersion[7 - i] =
			    (char)((uint8_t) 0x30 + (uint8_t) j);
		else
			ha->HardwareVersion[7 - i] =
			    (char)((uint8_t) 0x61 + (uint8_t) (j - 10));
		incr = (incr >> 4);
	}
	ha->HardwareVersion[8] = 0;

  	lpfc_vport_symbolic_node_name(phba->pport, pNodeSymbolicName, 256);
	memcpy(ha->NodeSymbolicName, pNodeSymbolicName, 256);

	if (pNodeSymbolicName)
		kfree(pNodeSymbolicName);
	return rc;
}

static int
lpfc_ioctl_hba_portattributes(struct lpfc_hba * phba,
			      struct lpfcCmdInput * cip, void *dataout)
{

	HBA_PORTATTRIBUTES *hp;
	struct serv_parm *hsp;
	HBA_OSDN *osdn;
	uint32_t cnt;
	struct lpfc_vport *vport = phba->pport;

	cnt = 0;
	hsp = (struct serv_parm *) (&vport->fc_sparam);
	hp = (HBA_PORTATTRIBUTES *) dataout;

	memset(dataout, 0, (sizeof (HBA_PORTATTRIBUTES)));
	memcpy((uint8_t *) & hp->NodeWWN,
	       (uint8_t *) & vport->fc_sparam.nodeName, sizeof (HBA_WWN));
	memcpy((uint8_t *) & hp->PortWWN,
	       (uint8_t *) & vport->fc_sparam.portName, sizeof (HBA_WWN));

	switch(phba->fc_linkspeed) {
	case LA_1GHZ_LINK:
		hp->PortSpeed = HBA_PORTSPEED_1GBIT;
		break;
	case LA_2GHZ_LINK:
		hp->PortSpeed = HBA_PORTSPEED_2GBIT;
		break;
	case LA_4GHZ_LINK:
		hp->PortSpeed = HBA_PORTSPEED_4GBIT;
		break;
	case LA_8GHZ_LINK:
		hp->PortSpeed = HBA_PORTSPEED_8GBIT;
		break;
	case LA_10GHZ_LINK:
		hp->PortSpeed = HBA_PORTSPEED_10GBIT;
		break;
	default:
		hp->PortSpeed = HBA_PORTSPEED_UNKNOWN;
		break;
	}

	hp->PortSupportedSpeed = 0;
	if (phba->lmt & LMT_10Gb)
		hp->PortSupportedSpeed |= HBA_PORTSPEED_10GBIT;
	if (phba->lmt & LMT_8Gb)
		hp->PortSupportedSpeed |= HBA_PORTSPEED_8GBIT;
	if (phba->lmt & LMT_4Gb)
		hp->PortSupportedSpeed |= HBA_PORTSPEED_4GBIT;
	if (phba->lmt & LMT_2Gb)
		hp->PortSupportedSpeed |= HBA_PORTSPEED_2GBIT;
	if (phba->lmt & LMT_1Gb)
		hp->PortSupportedSpeed |= HBA_PORTSPEED_1GBIT;

	hp->PortFcId = vport->fc_myDID;
	hp->PortType = HBA_PORTTYPE_UNKNOWN;
	if (phba->fc_topology == TOPOLOGY_LOOP) {
		if (vport->fc_flag & FC_PUBLIC_LOOP) {
			hp->PortType = HBA_PORTTYPE_NLPORT;
			memcpy((uint8_t *) & hp->FabricName,
			       (uint8_t *) & phba->fc_fabparam.nodeName,
			       sizeof (HBA_WWN));
		} else {
			hp->PortType = HBA_PORTTYPE_LPORT;
		}
	} else {
		if (vport->fc_flag & FC_FABRIC) {
			hp->PortType = HBA_PORTTYPE_NPORT;
			memcpy((uint8_t *) & hp->FabricName,
			       (uint8_t *) & phba->fc_fabparam.nodeName,
			       sizeof (HBA_WWN));
		} else {
			hp->PortType = HBA_PORTTYPE_PTP;
		}
	}

	if (vport->fc_flag & FC_BYPASSED_MODE)
		hp->PortState = HBA_PORTSTATE_BYPASSED;
	else if (vport->fc_flag & FC_OFFLINE_MODE)
		hp->PortState = HBA_PORTSTATE_DIAGNOSTICS;
	else {
		switch (phba->link_state) {
		case LPFC_INIT_START:
		case LPFC_INIT_MBX_CMDS:
			hp->PortState = HBA_PORTSTATE_UNKNOWN;
			hp->PortSpeed = HBA_PORTSPEED_NOT_NEGOTIATED;
			break;
		case LPFC_LINK_DOWN:
		case LPFC_LINK_UP:
		case LPFC_CLEAR_LA:
			hp->PortState = HBA_PORTSTATE_LINKDOWN;
			hp->PortSpeed = HBA_PORTSPEED_NOT_NEGOTIATED;
			break;
		case LPFC_HBA_READY:
			hp->PortState = HBA_PORTSTATE_ONLINE;
			break;
		case LPFC_HBA_ERROR:
		default:
			hp->PortState = HBA_PORTSTATE_ERROR;
			hp->PortSpeed = HBA_PORTSPEED_NOT_NEGOTIATED;
			break;
		}
	}
	cnt = vport->fc_map_cnt + vport->fc_unmap_cnt;
	hp->NumberofDiscoveredPorts = cnt;
	if (hsp->cls1.classValid) {
		/* bit 1 */
		hp->PortSupportedClassofService |= 2;
	}
	if (hsp->cls2.classValid) {
		/* bit 2 */
		hp->PortSupportedClassofService |= 4;
	}
	if (hsp->cls3.classValid) {
		/* bit 3 */
		hp->PortSupportedClassofService |= 8;
	}

	hp->PortMaxFrameSize = (((uint32_t) hsp->cmn.bbRcvSizeMsb) << 8) |
	    (uint32_t) hsp->cmn.bbRcvSizeLsb;

	hp->PortSupportedFc4Types.bits[2] = 0x1;
	hp->PortSupportedFc4Types.bits[3] = 0x20;
	hp->PortSupportedFc4Types.bits[7] = 0x1;
	hp->PortActiveFc4Types.bits[2] = 0x1;
	hp->PortActiveFc4Types.bits[7] = 0x1;

	osdn = (HBA_OSDN *) & hp->OSDeviceName[0];
	sprintf(osdn->drvname, "%s", LPFC_DRIVER_NAME);
	osdn->instance = phba->brd_no;
	osdn->target = (uint32_t) (-1);
	osdn->lun = (uint32_t) (-1);
	osdn->bus = (lpfc_shost_from_vport(phba->pport))->host_no;
	return 0;
}

static int
lpfc_ioctl_hba_portstatistics(struct lpfc_hba * phba,
			      struct lpfcCmdInput * cip, void *dataout)
{
	int rc = 0;
	struct fc_host_statistics *host_stats = NULL;
	HBA_PORTSTATISTICS *hs;
	struct Scsi_Host *shost = lpfc_shost_from_vport(phba->pport);

	/* From lpfc_attr.c */
	extern struct fc_host_statistics * lpfc_get_stats(struct Scsi_Host *shost);

	/*
	 * The lpfc_get_stats function works with a fc_transport
	 * data structure called fc_host_statistics, whereas this
	 * routine works with an Emulex application data structure
	 * called HBA_PORTSTATISTICS.  The memcpy operation must
	 * reference the HBA_PORTSTATISTICS size as the two data
	 * structures have a different byte size.
	 */
	memset(dataout, 0, (sizeof (HBA_PORTSTATISTICS)));
	hs = (HBA_PORTSTATISTICS *) dataout;
	host_stats = lpfc_get_stats(shost);

	/*
	 * If lpfc_get_stats fails, the return value is a NULL pointer.  Don't
	 * call memcpy in this case. 
	 */
	if (!host_stats)
		return EIO;

	memcpy(hs, host_stats, sizeof(HBA_PORTSTATISTICS));
	return rc;
}

static int
lpfc_ioctl_hba_wwpnportattributes(struct lpfc_hba * phba,
				  struct lpfcCmdInput * cip, void *dataout)
{
	HBA_WWN findwwn;
	struct lpfc_nodelist *ndlp;
	HBA_PORTATTRIBUTES *hp;
	int rc = 0;
	int found = 0;
	struct lpfc_vport *vport = phba->pport;
        struct Scsi_Host *shost = lpfc_shost_from_vport(vport);

	hp = (HBA_PORTATTRIBUTES *) dataout;
	memset(hp, 0, (sizeof (HBA_PORTATTRIBUTES)));

	if (copy_from_user((uint8_t *) &findwwn, (uint8_t *) cip->lpfc_arg1,
			   sizeof (HBA_WWN))) {
		return EIO;
	}

	spin_lock_irq(shost->host_lock);
	list_for_each_entry(ndlp, &vport->fc_nodes, nlp_listp) {
		rc = memcmp(&ndlp->nlp_portname, &findwwn, sizeof(HBA_WWN));
		if (rc == 0) {
			found = 1;
			break;
		}
	}
	spin_unlock_irq(shost->host_lock);

	if (found) {
		if (!lpfc_nlp_get(ndlp))
			goto err_exit;
		rc = lpfc_ioctl_found_port(phba, ndlp, dataout, hp);
		lpfc_nlp_put(ndlp);
		return rc;
	}

 err_exit:
	return ERANGE;
}

static int
lpfc_ioctl_hba_discportattributes(struct lpfc_hba * phba,
				  struct lpfcCmdInput * cip, void *dataout)
{
	HBA_PORTATTRIBUTES *hp;
	struct lpfc_nodelist *ndlp;
	uint32_t refresh, offset, cnt;
	int rc = 0;
	int found = 0;
	struct lpfc_vport *vport = phba->pport;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);

	hp = (HBA_PORTATTRIBUTES *) dataout;
	memset(dataout, 0, (sizeof (HBA_PORTATTRIBUTES)));

	offset = (ulong) cip->lpfc_arg2;
	refresh = (ulong) cip->lpfc_arg3;
	if (refresh != phba->nport_event_cnt) {
		/*
		 * This is an error, need refresh, just return zero'ed out
		 * portattr and FcID as -1.
		 */
		hp->PortFcId = 0xffffffff;
		return 0;
	}

	cnt = 0;
	spin_lock_irq(shost->host_lock);
	list_for_each_entry(ndlp, &vport->fc_nodes, nlp_listp) {
		/* libdfc requirement is for mapped and unmapped
		 * nodes to be counted.
		 */
		switch (ndlp->nlp_state) {
		case NLP_STE_UNMAPPED_NODE:
		case NLP_STE_MAPPED_NODE:
			if (cnt == offset) {
				found = 1;
				goto exit;
			} else
				cnt += 1;
		}
	}

 exit:
	spin_unlock_irq(shost->host_lock);

	if (found) {
		if (!lpfc_nlp_get(ndlp))
			goto err_exit;

		rc = lpfc_ioctl_found_port(phba, ndlp, dataout, hp);
		lpfc_nlp_put(ndlp);
		return rc;
	}
	
 err_exit:
	return ERANGE;
}

static int
lpfc_ioctl_hba_indexportattributes(struct lpfc_hba * phba,
				   struct lpfcCmdInput * cip, void *dataout)
{
	HBA_PORTATTRIBUTES *hp;
	struct lpfc_nodelist *ndlp;
	uint32_t refresh, offset, cnt;
	int rc = 0, found = 0;
  	struct lpfc_vport *vport = phba->pport;
	struct Scsi_Host *shost = lpfc_shost_from_vport(vport);

	hp = (HBA_PORTATTRIBUTES *) dataout;
	memset(dataout, 0, (sizeof (HBA_PORTATTRIBUTES)));
	offset = (ulong) cip->lpfc_arg2;
	refresh = (ulong) cip->lpfc_arg3;
	if (refresh != phba->nport_event_cnt) {
		/*
		 * This is an error, need refresh, just return zero'ed out
		 * portattr and FcID as -1.
		 */
		hp->PortFcId = 0xffffffff;
		return 0;
	}

	cnt = 0;
	spin_lock_irq(shost->host_lock);
	list_for_each_entry(ndlp, &vport->fc_nodes, nlp_listp) {
		if (cnt == offset) {
			found = 1;
			break;
		}
		cnt += 1;
	}
	spin_unlock_irq(shost->host_lock);

	if (found) {
		if (!lpfc_nlp_get(ndlp))
			goto err_exit;

		rc = lpfc_ioctl_found_port(phba, ndlp, dataout, hp);
		lpfc_nlp_put(ndlp);
		return rc;
	}
	
 err_exit:
	return ERANGE;
}

static int
lpfc_ioctl_hba_setmgmtinfo(struct lpfc_hba * phba,
			   struct lpfcCmdInput * cip)
{
	HBA_MGMTINFO *mgmtinfo;
	int rc = 0;

	/*
	 * This an HBAAPI requirement.  Although the setmgmtinfo is not
	 * required, other applications may require it.  Just store the
	 * data for other apps to retrieve.
	 */
	mgmtinfo = kmalloc(4096, GFP_KERNEL);
	if (!mgmtinfo)
		return ENOMEM;

	rc = copy_from_user((uint8_t *) mgmtinfo, (uint8_t *) cip->lpfc_arg1,
			    sizeof (HBA_MGMTINFO));
	if (rc) {
		kfree(mgmtinfo);
		return EIO;
	}

	/* Can ONLY set UDP port and IP Address */
	phba->ipVersion = mgmtinfo->IPVersion;
	phba->UDPport = mgmtinfo->UDPPort;

	memcpy((uint8_t *) & phba->ipAddr[0],
	       (uint8_t *) & mgmtinfo->IPAddress[0], 4);
	if (phba->ipVersion == RNID_IPV6) {
		memcpy((uint8_t *) &phba->ipAddr[0],
		       (uint8_t *) &mgmtinfo->IPAddress[0], 16);

	}

	kfree(mgmtinfo);
	return 0;
}

static int
lpfc_ioctl_hba_getmgmtinfo(struct lpfc_hba * phba,
			   struct lpfcCmdInput * cip, void *dataout)
{
	HBA_MGMTINFO *mgmtinfo;

	mgmtinfo = (HBA_MGMTINFO *) dataout;

	mgmtinfo->unittype = RNID_HBA;
	mgmtinfo->PortId = phba->pport->fc_myDID;
	mgmtinfo->NumberOfAttachedNodes = 0;
	mgmtinfo->TopologyDiscoveryFlags = 0;
	mgmtinfo->IPVersion = phba->ipVersion;
	mgmtinfo->UDPPort = phba->UDPport;

	memcpy((uint8_t *) & mgmtinfo->wwn,
	       (uint8_t *) &phba->pport->fc_nodename,
	       sizeof(mgmtinfo->wwn));

	memcpy((void *)&mgmtinfo->IPAddress[0],
	       (void *)&phba->ipAddr[0], 4);

	if (phba->ipVersion == RNID_IPV6) {
		memcpy((void *)&mgmtinfo->IPAddress[0],
		       (void *)&phba->ipAddr[0], 16);
	}

	return 0;
}

static int
lpfc_ioctl_hba_refreshinfo(struct lpfc_hba * phba,
			   struct lpfcCmdInput * cip, void *dataout)
{
	uint32_t *lptr;
	lptr = (uint32_t *) dataout;
	*lptr = phba->nport_event_cnt;
	return 0;
}

static int
lpfc_ioctl_hba_rnid(struct lpfc_hba * phba, struct lpfcCmdInput * cip, void *dataout)
{
	struct nport_id idn;
	struct lpfc_iocbq *cmdiocbq = NULL;
	struct lpfc_iocbq *rspiocbq = NULL;
	RNID *prsp;
	uint32_t *pcmd;
	uint32_t *psta;
	IOCB_t *rsp;
	void *context2;
	int i0;
	int rtnbfrsiz;
	struct lpfc_nodelist *pndl;
	int rc = 0;

	if (phba->sli.sli_flag & LPFC_BLOCK_MGMT_IO)
		return EAGAIN;

	if (copy_from_user((uint8_t *) idn.wwpn,
			   (void __user *) cip->lpfc_arg1,
			   sizeof(HBA_WWN))) {
		return EIO;
	}

	pndl = lpfc_findnode_wwnn(phba->pport,
				 (struct lpfc_name *) idn.wwpn);

	if (pndl == 0)
		pndl = lpfc_findnode_wwpn(phba->pport,
					 (struct lpfc_name *) idn.wwpn);

	if (!pndl || !NLP_CHK_NODE_ACT(pndl))
		return ENODEV;

	for (i0 = 0;
	     i0 < 10 && (pndl->nlp_flag & NLP_ELS_SND_MASK) == NLP_RNID_SND;
	     i0++) {
		mdelay(1000);
	}

	if (i0 == 10) {
		pndl->nlp_flag &= ~NLP_RNID_SND;
		return EBUSY;
	}

	cmdiocbq = lpfc_prep_els_iocb(phba->pport, 1, (2 * sizeof(uint32_t)), 0,
				      pndl, pndl->nlp_DID, ELS_CMD_RNID);
	if (!cmdiocbq)
		return ENOMEM;

	/*
	 *  Context2 is used by prep/free to locate cmd and rsp buffers,
	 *  but context2 is also used by iocb_wait to hold a rspiocb ptr.
	 *  The rsp iocbq can be returned from the completion routine for
	 *  iocb_wait, so save the prep/free value locally . It will be
	 *  restored after returning from iocb_wait.
	 */
	context2 = cmdiocbq->context2;

	if ((rspiocbq = lpfc_sli_get_iocbq(phba)) == NULL) {
		rc = ENOMEM;
		goto sndrndqwt;
	}
	rsp = &(rspiocbq->iocb);

	pcmd = (uint32_t *) (((struct lpfc_dmabuf *) cmdiocbq->context2)->virt);
	*pcmd++ = ELS_CMD_RNID;

	memset((void *) pcmd, 0, sizeof (RNID));
	((RNID *) pcmd)->Format = 0;
	((RNID *) pcmd)->Format = RNID_TOPOLOGY_DISC;
	cmdiocbq->context1 = NULL;
	cmdiocbq->context2 = NULL;
	cmdiocbq->iocb_flag |= LPFC_IO_LIBDFC;

	pndl->nlp_flag |= NLP_RNID_SND;
	cmdiocbq->iocb.ulpTimeout = (phba->fc_ratov * 2) + 3 ;

	rc = lpfc_sli_issue_iocb_wait(phba, LPFC_ELS_RING, cmdiocbq, rspiocbq,
				(phba->fc_ratov * 2) + LPFC_DRVR_TIMEOUT);

	pndl->nlp_flag &= ~NLP_RNID_SND;
	cmdiocbq->context2 = context2;

	if (rc == IOCB_TIMEDOUT) {
		lpfc_sli_release_iocbq(phba, rspiocbq);
		cmdiocbq->context1 = NULL;
		cmdiocbq->iocb_cmpl = lpfc_ioctl_timeout_iocb_cmpl;
		return EIO;
	}

	if (rc == IOCB_BUSY) {
		rc = EAGAIN;
		goto sndrndqwt;
	}

	if (rc != IOCB_SUCCESS) {
		rc = EIO;
		goto sndrndqwt;
	}

	if (rsp->ulpStatus == IOSTAT_SUCCESS) {
		struct lpfc_dmabuf *buf_ptr1, *buf_ptr;
		buf_ptr1 = (struct lpfc_dmabuf *)(cmdiocbq->context2);
		buf_ptr = list_entry(buf_ptr1->list.next, struct lpfc_dmabuf,
					list);
		psta = (uint32_t*)buf_ptr->virt;
		prsp = (RNID *) (psta + 1);	/*  then rnid response data */
		rtnbfrsiz = prsp->CommonLen + prsp->SpecificLen +
							sizeof (uint32_t);
		memcpy((uint8_t *) dataout, (uint8_t *) psta, rtnbfrsiz);

		if (rtnbfrsiz > cip->lpfc_outsz)
			rtnbfrsiz = cip->lpfc_outsz;
		if (copy_to_user
		    ((void __user *) cip->lpfc_arg2, (uint8_t *) & rtnbfrsiz,
		     sizeof (int)))
			rc = EIO;
	} else if (rsp->ulpStatus == IOSTAT_LS_RJT)  {
		uint8_t ls_rjt[8];
		uint32_t *ls_rjtrsp;

		ls_rjtrsp = (uint32_t*)(ls_rjt + 4);

		/* construct the LS_RJT payload */
		ls_rjt[0] = 0x01;
		ls_rjt[1] = 0x00;
		ls_rjt[2] = 0x00;
		ls_rjt[3] = 0x00;

		*ls_rjtrsp = be32_to_cpu(rspiocbq->iocb.un.ulpWord[4]);
		rtnbfrsiz = 8;
		memcpy((uint8_t *) dataout, (uint8_t *) ls_rjt, rtnbfrsiz);
		if (copy_to_user
		    ((void __user *) cip->lpfc_arg2, (uint8_t *) & rtnbfrsiz,
		     sizeof (int)))
			rc = EIO;
	} else
		rc = EACCES;

sndrndqwt:
	if (cmdiocbq)
		lpfc_els_free_iocb(phba, cmdiocbq);

	if (rspiocbq)
		lpfc_sli_release_iocbq(phba, rspiocbq);

	return rc;
}

static int
lpfc_ioctl_hba_getevent(struct lpfc_hba * phba,
			struct lpfcCmdInput * cip, void *dataout)
{
	uint32_t outsize, size;
	HBAEVT_t *recout;
	int rc;

	/*
	 * Fetch all HBAAPI events and return them to the caller.
	 * The caller has set lpfc_arg1 with the total number of available
	 * records in their buffer.
	 */
	size = (uint32_t) (ulong) cip->lpfc_arg1;
	recout = (HBAEVT_t *) dataout;
	for (outsize = 0; outsize < MAX_HBAEVT; outsize++) {
		/*
		 * Two conditions are detected:  The caller's buffer has been
		 * filled and the driver's recording buffer is full.  Both
		 * events cause the copy operation to stop.
		 */
		if ((phba->hba_event_get == phba->hba_event_put) ||
		    (outsize == size))
			break;

		memcpy((uint8_t *) recout,
		       (uint8_t *) &phba->hbaevt[phba->hba_event_get],
		       sizeof (HBAEVT_t));

		recout++;
		phba->hba_event_get++;
		if (phba->hba_event_get >= MAX_HBAEVT)
			phba->hba_event_get = 0;
	}

	/* copy back size of response */
	rc = copy_to_user((uint8_t *) cip->lpfc_arg2, (uint8_t *) & outsize,
			  sizeof (uint32_t));
	if (rc)
		return EIO;

	/* copy back number of missed records */
	rc = copy_to_user((uint8_t *) cip->lpfc_arg3,
			  (uint8_t *) &phba->hba_event_missed,
			  sizeof (uint32_t));
	if (rc)
		return EIO;

	phba->hba_event_missed = 0;
	cip->lpfc_outsz = (uint32_t) (outsize * sizeof (HBA_EVENTINFO));
	return 0;
}

static int
lpfc_ioctl_hba_fcptargetmapping(struct lpfc_hba * phba,
				struct lpfcCmdInput * cip,
				void *dataout, uint32_t buff_size)
{
	struct lpfc_vport *vport = phba->pport;
	struct lpfc_nodelist *ndlp;
	uint32_t num_caller_entries = (uint32_t)((unsigned long) cip->lpfc_arg1);
	uint32_t total = 0;
	int count = 0;
	int size = 0;
	HBA_OSDN *osdn;
	uint32_t fcpLun[2];
	struct Scsi_Host *shost;
	struct scsi_device *sdev;
	union {
		char* p;
		HBA_FCPSCSIENTRY *se;
	} ep;

	char *appPtr;
	char *drvPtr;
	int rc = 0;

#ifdef CONFIG_COMPAT
	uint32_t temp_val = 0;
	char temp_array[256] = {0};
	char *ScsiIdPtr;
#endif

	/*
	 * The packing in a 32-bit data structure from user space is not
	 * aligned with the same data structure in a 64 bit kernel build.
	 * For the 32-bit in a 64-bit kernel case, the data offsets have
	 * a different alignment.  
	 */
#ifdef CONFIG_COMPAT
	if (cip->lpfc_cntl == LPFC_CNTL_X86_APP) {
		appPtr = ((char *) cip->lpfc_dataout);
		appPtr += sizeof(HBA_UINT32);
		drvPtr = (char *) dataout;
	} else {
#endif		
		appPtr = ((char *) cip->lpfc_dataout) +
			offsetof(HBA_FCPTARGETMAPPING, entry);
		drvPtr = (char *) &((HBA_FCPTARGETMAPPING *) dataout)->entry[0];
#ifdef CONFIG_COMPAT
	}
#endif /* CONFIG_COMPAT */

	ep.p = drvPtr;

	/*
	 * This loop computes the number of lun entries mapped
	 * to the scsihost bound to the driver.
	 */
	shost = lpfc_shost_from_vport(vport);
	spin_lock_irq(shost->host_lock);
	list_for_each_entry(ndlp, &vport->fc_nodes, nlp_listp) {
		if ((ndlp->nlp_state != NLP_STE_MAPPED_NODE) ||
		    !NLP_CHK_NODE_ACT(ndlp))
			continue;

		if (!lpfc_nlp_get(ndlp))
			continue;
		spin_unlock_irq(shost->host_lock);
		shost_for_each_device(sdev, shost) {
			/*
			 * If the number of entries found exceeds the
			 * number of entries provisioned by the caller,
			 * just keep counting, but don't copy any more
			 * data.  The caller is required to callback.
			 */
			if (ndlp->nlp_sid == sdev->id)
				total++;
		}
		spin_lock_irq(shost->host_lock);
		lpfc_nlp_put(ndlp);
	}
	spin_unlock_irq(shost->host_lock);

	/*
	 * There are two failure cases.  (1) If the driver has more lun entries
	 * than the caller allocated, just fail the request with ERANGE and copy
	 * the number of entries required.  (2) The driver found no entries.  In
	 * this case, set NumberOfEntries to 0 indicating no targets.
	 */
	if ((total > num_caller_entries) || (total == 0)) {
		rc = copy_to_user(cip->lpfc_dataout, &total, sizeof(HBA_UINT32));
		if (rc)
			return EIO;

		rc = ERANGE;
		if (total == 0) {
			cip->lpfc_outsz = 0;
			rc = 0;
		}
		return rc;
	}

	/*
	 * This loop copies the lun entry data required for every
	 * discovered lun.  The assumption is that the caller's buffer
	 * has enough entries allocated.
	 */
	spin_lock_irq(shost->host_lock);
	list_for_each_entry(ndlp, &vport->fc_nodes, nlp_listp) {
		if ((ndlp->nlp_state != NLP_STE_MAPPED_NODE) ||
		    !NLP_CHK_NODE_ACT(ndlp))
			continue;

		if (!lpfc_nlp_get(ndlp))
			continue;
		spin_unlock_irq(shost->host_lock);
		shost_for_each_device(sdev, shost) {
			if (ndlp->nlp_sid != sdev->id)
				continue;

			/*
			 * If some nodes are dynamically added to the 
			 * fc_nodes list or if the number of luns is dynamically
			 * increased while this loop is running, clamp the 
			 * current operation to that initially discovered.
			 */
			if (count >= total)
				break;
#ifdef CONFIG_COMPAT
			if (cip->lpfc_cntl == LPFC_CNTL_X86_APP) {
				/*
				 * Clear out user OSDeviceName buffer and 
				 * reference where the ScsiId bytes begin
				 * in the user's buffer.
				 */
				copy_to_user(appPtr, &temp_array[0], 256);
				ScsiIdPtr = appPtr + 256;

				/*Set osdn driver name. */
				copy_to_user(appPtr, LPFC_DRIVER_NAME,
					     sizeof(LPFC_DRIVER_NAME));
				/* 
				 * The HBA_OSDN drvname array is hardcoded
				 * in hbaapi.h to 32.
				 */
				appPtr += 32;

				/* Set osdn instance. */
				copy_to_user(appPtr, &phba->brd_no, sizeof(uint32_t));
				appPtr += sizeof(uint32_t);

				/* Set osdn target. */
				copy_to_user(appPtr, &sdev->id, sizeof(uint32_t));
				appPtr += sizeof(uint32_t);

				/* Set osdn lun. */
				copy_to_user(appPtr, &sdev->lun, sizeof(uint32_t));
				appPtr += sizeof(uint32_t);

				/*
				 * Start ScsiID data loads - first move app
				 * ptr to ScsiId fields. Set ScsiId.Bus 
				 */
				appPtr = ScsiIdPtr;
				copy_to_user(appPtr, &temp_val, sizeof(HBA_UINT32));
				appPtr += sizeof(HBA_UINT32);

				/* Set ScsiId.Target */
				copy_to_user(appPtr, &sdev->id, sizeof(HBA_UINT32));
				appPtr += sizeof(HBA_UINT32);

				/* Set ScsiId.Lun */
				copy_to_user(appPtr, &sdev->lun, sizeof(HBA_UINT32));
				appPtr += sizeof(HBA_UINT32);

				/* Start FcpID data loads. */
				/* Set FcpId.Lun */
				copy_to_user(appPtr, &ndlp->nlp_DID, sizeof(HBA_UINT32));
				appPtr += sizeof(HBA_UINT32);

				/* Set FcpID.NodeWWN */
				copy_to_user(appPtr, &ndlp->nlp_nodename, sizeof (HBA_WWN));
				appPtr += sizeof(HBA_WWN);

				/* Set FcpID.PortWWN */
				copy_to_user(appPtr, &ndlp->nlp_portname, sizeof (HBA_WWN));
				appPtr += sizeof(HBA_WWN);

				/* Set FcpID.FcpLun */
				memset((char *)fcpLun, 0, sizeof (HBA_UINT64));
				fcpLun[0] = (sdev->lun << 8);
				copy_to_user(appPtr, &fcpLun[0], sizeof (HBA_UINT64));
				appPtr += sizeof(HBA_UINT64);

				count++;
			} else {
#endif /* CONFIG_COMPAT */
				memset((char *)&ep.se->ScsiId.OSDeviceName[0], 0, 256);
	
				/* OSDeviceName is device info filled into HBA_OSDN */
				osdn = (HBA_OSDN *) &ep.se->ScsiId.OSDeviceName[0];
				sprintf(osdn->drvname, "%s", LPFC_DRIVER_NAME);
				osdn->instance = phba->brd_no;
				osdn->target = sdev->id;
				osdn->lun = sdev->lun;
				osdn->flags = 0;
				ep.se->ScsiId.ScsiTargetNumber = sdev->id;
				ep.se->ScsiId.ScsiOSLun = sdev->lun;
				ep.se->ScsiId.ScsiBusNumber = 0;
				ep.se->FcpId.FcId = ndlp->nlp_DID;
				memset((char *)&fcpLun[0], 0, sizeof (HBA_UINT64));
				fcpLun[0] = (sdev->lun << 8);
				memcpy((char *)&ep.se->FcpId.FcpLun,
				       (char *)&fcpLun[0],
				       sizeof (HBA_UINT64));
				memcpy((uint8_t *) &ep.se->FcpId.PortWWN,
				       &ndlp->nlp_portname,
				       sizeof (HBA_WWN));
				memcpy((uint8_t *) &ep.se->FcpId.NodeWWN,
				       &ndlp->nlp_nodename,
				       sizeof (HBA_WWN));
				count++;
				ep.se++;
				size += sizeof(HBA_FCPSCSIENTRY);
#ifdef CONFIG_COMPAT
			}
#endif
		}
		spin_lock_irq(shost->host_lock);
		lpfc_nlp_put(ndlp);
	}
	spin_unlock_irq(shost->host_lock);
	if (count > 0) {
		/* Update the number of entries. */
		rc = copy_to_user(cip->lpfc_dataout, &count,
				  sizeof(HBA_UINT32));
		if (rc)
			return EIO;

		/* If native mode, update the entry positions. */
		if (size) {
			rc = copy_to_user(appPtr, drvPtr, size);
			if (rc)
				return EIO;
		}
	}

	/*
	 * Don't allow a second copy to user buffer.  The copy operation
	 * is already done.
	 */
	cip->lpfc_outsz = 0;
	return 0;
}

static int
lpfc_ioctl_port_attrib(struct lpfc_hba * phba, void *dataout)
{
	lpfc_vpd_t *vp;
	struct serv_parm *hsp;
	HBA_PORTATTRIBUTES *hp;
	HBA_OSDN *osdn;
	uint32_t cnt;
	int rc = 0;
	struct lpfc_vport *vport = phba->pport;

	vp = &phba->vpd;
	hsp = (struct serv_parm *) (&vport->fc_sparam);
	hp = (HBA_PORTATTRIBUTES *) dataout;
	memset(dataout, 0, (sizeof (HBA_PORTATTRIBUTES)));
	memcpy((uint8_t *) & hp->NodeWWN,
	       (uint8_t *) & vport->fc_sparam.nodeName, sizeof (HBA_WWN));
	memcpy((uint8_t *) & hp->PortWWN,
	       (uint8_t *) & vport->fc_sparam.portName, sizeof (HBA_WWN));

	if (phba->fc_linkspeed == LA_2GHZ_LINK)
		hp->PortSpeed = HBA_PORTSPEED_2GBIT;
	else
		hp->PortSpeed = HBA_PORTSPEED_1GBIT;

	if (FC_JEDEC_ID(vp->rev.biuRev) == VIPER_JEDEC_ID)
		hp->PortSupportedSpeed = HBA_PORTSPEED_10GBIT;
	else if ((FC_JEDEC_ID(vp->rev.biuRev) == CENTAUR_2G_JEDEC_ID) ||
		 (FC_JEDEC_ID(vp->rev.biuRev) == PEGASUS_JEDEC_ID) ||
		 (FC_JEDEC_ID(vp->rev.biuRev) == THOR_JEDEC_ID))
		hp->PortSupportedSpeed = HBA_PORTSPEED_2GBIT;
	else
		hp->PortSupportedSpeed = HBA_PORTSPEED_1GBIT;

	hp->PortFcId = vport->fc_myDID;
	hp->PortType = HBA_PORTTYPE_UNKNOWN;
	if (phba->fc_topology == TOPOLOGY_LOOP) {
		if (vport->fc_flag & FC_PUBLIC_LOOP) {
			hp->PortType = HBA_PORTTYPE_NLPORT;
			memcpy((uint8_t *) &hp->FabricName,
			       (uint8_t *) &phba->fc_fabparam.nodeName,
			       sizeof (HBA_WWN));
		} else {
			hp->PortType = HBA_PORTTYPE_LPORT;
		}
	} else {
		if (vport->fc_flag & FC_FABRIC) {
			hp->PortType = HBA_PORTTYPE_NPORT;
			memcpy((uint8_t *) & hp->FabricName,
			       (uint8_t *) & phba->fc_fabparam.nodeName,
			       sizeof (HBA_WWN));
		} else {
			hp->PortType = HBA_PORTTYPE_PTP;
		}
	}

	if (vport->fc_flag & FC_BYPASSED_MODE) {
		hp->PortState = HBA_PORTSTATE_BYPASSED;
	} else if (vport->fc_flag & FC_OFFLINE_MODE) {
		hp->PortState = HBA_PORTSTATE_DIAGNOSTICS;
	} else {
		switch (vport->port_state) {
		case LPFC_INIT_START:
		case LPFC_INIT_MBX_CMDS:
			hp->PortState = HBA_PORTSTATE_UNKNOWN;
			break;

		case LPFC_LINK_DOWN:
		case LPFC_LINK_UP:
		case LPFC_CLEAR_LA:
			hp->PortState = HBA_PORTSTATE_LINKDOWN;
			break;

		case LPFC_HBA_READY:
			hp->PortState = HBA_PORTSTATE_ONLINE;
			break;

		case LPFC_HBA_ERROR:
		default:
			hp->PortState = HBA_PORTSTATE_ERROR;
			break;
		}
	}

	cnt = vport->fc_map_cnt + vport->fc_unmap_cnt;
	hp->NumberofDiscoveredPorts = cnt;
	if (hsp->cls1.classValid) {
		hp->PortSupportedClassofService |= 2;	/* bit 1 */
	}

	if (hsp->cls2.classValid) {
		hp->PortSupportedClassofService |= 4;	/* bit 2 */
	}

	if (hsp->cls3.classValid) {
		hp->PortSupportedClassofService |= 8;	/* bit 3 */
	}

	hp->PortMaxFrameSize = (((uint32_t) hsp->cmn.bbRcvSizeMsb) << 8) |
	    (uint32_t) hsp->cmn.bbRcvSizeLsb;

	hp->PortSupportedFc4Types.bits[2] = 0x1;
	hp->PortSupportedFc4Types.bits[3] = 0x20;
	hp->PortSupportedFc4Types.bits[7] = 0x1;
	hp->PortActiveFc4Types.bits[2] = 0x1;

	hp->PortActiveFc4Types.bits[7] = 0x1;

	osdn = (HBA_OSDN *) & hp->OSDeviceName[0];
	sprintf(osdn->drvname, "%s", LPFC_DRIVER_NAME);
	osdn->instance = phba->brd_no;
	osdn->target = (uint32_t) (-1);
	osdn->lun = (uint32_t) (-1);

	return rc;
}

static int
lpfc_ioctl_found_port(struct lpfc_hba *phba,
		      struct lpfc_nodelist *ndlp,
		      void *dataout,
		      HBA_PORTATTRIBUTES *hp)
{
	struct lpfc_vport *vport = phba->pport;
	HBA_OSDN *osdn;

	/* Check if its the local port */
	if (vport->fc_myDID == ndlp->nlp_DID)
		return lpfc_ioctl_port_attrib(phba, dataout);

	hp->PortSupportedClassofService = ndlp->nlp_class_sup;
	hp->PortMaxFrameSize = ndlp->nlp_maxframe;

	memcpy((uint8_t *) & hp->NodeWWN, (uint8_t *) & ndlp->nlp_nodename,
	       sizeof (HBA_WWN));
	memcpy((uint8_t *) & hp->PortWWN, (uint8_t *) & ndlp->nlp_portname,
	       sizeof (HBA_WWN));
	hp->PortSpeed = 0;

	/* The driver only knows the speed if the device is on the same loop. */
	if (((vport->fc_myDID & 0xffff00) == (ndlp->nlp_DID & 0xffff00)) &&
	    (phba->fc_topology == TOPOLOGY_LOOP)) {
		if (phba->fc_linkspeed == LA_2GHZ_LINK)
			hp->PortSpeed = HBA_PORTSPEED_2GBIT;
		else
			hp->PortSpeed = HBA_PORTSPEED_1GBIT;
	}

	hp->PortFcId = ndlp->nlp_DID;
	if ((vport->fc_flag & FC_FABRIC) &&
	    ((vport->fc_myDID & 0xff0000) == (ndlp->nlp_DID & 0xff0000))) {
		/* The remote node is in the same domain as the driver. */
		memcpy((uint8_t *) &hp->FabricName,
		       (uint8_t *) &phba->fc_fabparam.nodeName,
		       sizeof (HBA_WWN));
	}

	hp->PortState = HBA_PORTSTATE_ONLINE;
	if (ndlp->nlp_type & NLP_FCP_TARGET)
		hp->PortActiveFc4Types.bits[2] = 0x1;

	hp->PortActiveFc4Types.bits[7] = 0x1;
	hp->PortType = HBA_PORTTYPE_UNKNOWN;
	lpfc_determine_port_type(phba, ndlp, hp);

	if (ndlp->nlp_type == NLP_FCP_TARGET) {
		osdn = (HBA_OSDN *) & hp->OSDeviceName[0];
		sprintf(osdn->drvname, "%s", LPFC_DRIVER_NAME);
		osdn->instance = phba->brd_no;
		osdn->target = ndlp->nlp_sid;
		osdn->lun = (uint32_t) (-1);
	}

	return 0;
}

static void
lpfc_determine_port_type(struct lpfc_hba *phba, 
			 struct lpfc_nodelist *ndlp,
		         HBA_PORTATTRIBUTES *hp)
{
	struct lpfc_vport *vport = phba->pport;
	int ret = 0;

	switch (phba->fc_topology) {
	case TOPOLOGY_LOOP:
		hp->PortType = HBA_PORTTYPE_LPORT;
		if (vport->fc_flag & FC_PUBLIC_LOOP) {
			ret = memcmp(&ndlp->nlp_nodename, 
				     &phba->fc_fabparam.nodeName,
				     sizeof(struct lpfc_name));
			if (ret == 0)
				hp->PortType = HBA_PORTTYPE_FLPORT;
		}
		break;
	default:
		hp->PortType = HBA_PORTTYPE_PTP;
		if (vport->fc_flag & FC_FABRIC) {
			ret = memcmp(&ndlp->nlp_nodename,
				     &phba->fc_fabparam.nodeName,
				     sizeof(struct lpfc_name));
			if (ret == 0)
				hp->PortType = HBA_PORTTYPE_FPORT;
		}
		break;
	}

	/* Based on DID */
	if (ret != 0) {
		if ((ndlp->nlp_DID & 0xff) == 0) {
			hp->PortType = HBA_PORTTYPE_NPORT;
		} else {
			if ((ndlp->nlp_DID & 0xff0000) != 0xff0000)
				hp->PortType = HBA_PORTTYPE_NLPORT;
		}
	}
}

int
lpfc_hba_put_event(struct lpfc_hba *phba, uint32_t evcode, uint32_t evdata1,
		  uint32_t evdata2, uint32_t evdata3, uint32_t evdata4)
{
	HBAEVT_t *rec;

	rec = &phba->hbaevt[phba->hba_event_put];
	rec->fc_eventcode = evcode;
	rec->fc_evdata1 = evdata1;
	rec->fc_evdata2 = evdata2;
	rec->fc_evdata3 = evdata3;
	rec->fc_evdata4 = evdata4;
	phba->hba_event_put++;
	if (phba->hba_event_put >= MAX_HBAEVT)
		phba->hba_event_put = 0;

	if (phba->hba_event_put == phba->hba_event_get) {
		phba->hba_event_missed++;
		phba->hba_event_get++;
		if (phba->hba_event_get >= MAX_HBAEVT)
			phba->hba_event_get = 0;
	}

	return 0;
}






