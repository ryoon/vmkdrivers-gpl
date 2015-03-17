/*
 * ****************************************************************
 * Portions Copyright 2008,2009 VMware, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * ****************************************************************/

#if defined(__VMKLNX__)

#include <linux/kernel.h>
#include <linux/libata.h>
#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include "libata.h"
#include "kcompat.h"

/*
 * This function is defined to help free the memory allocated by libata module
 * In VMware ESX, only the module allocating the memory can free it.
 */
void ata_kfree(const void *p)
{
	kfree(p);
}
EXPORT_SYMBOL_GPL(ata_kfree);

static inline int ata_cmd_completed(struct ata_port *ap, struct ata_queued_cmd *qc)
{
	return (qc->tag == ATA_TAG_POISON);
}

static int ata_port_reset(struct ata_port *ap)
{
	struct ata_queued_cmd *qc;
	unsigned long flags;
	int rc;

	spin_lock_irqsave(ap->lock, flags);
	qc = ata_qc_from_tag(ap, ap->link.active_tag);
	if (qc) {
		ata_port_flush_task(ap);
		rc = ata_port_freeze(ap);
		spin_unlock_irqrestore(ap->lock, flags);
		ata_port_printk(ap, KERN_INFO, 
				"%s: %d cmds aborted, freezing port\n", 
				__FUNCTION__, rc);
		if (rc != 0) {
			ata_port_wait_eh(ap);
		}
	} else {
		spin_unlock_irqrestore(ap->lock, flags);
	}
	return SUCCESS;
}

int ata_scsi_abort(struct scsi_cmnd *cmd)
{
	struct ata_port *ap = ata_shost_to_port(cmd->device->host);
	struct ata_queued_cmd *qc;
	unsigned long flags;
	unsigned long timeout;
	int rc = FAILED;
	int i; 

	ata_port_printk(ap, KERN_INFO, "%s: cmd %p, code %02x\n", 
			__FUNCTION__, cmd, cmd->cmnd[0]);

	spin_lock_irqsave(ap->lock, flags);
	/* If the cmd is not found, return abort failed */
	for (i = 0; i < ATA_MAX_QUEUE; i++) {
		if ((qc = __ata_qc_from_tag(ap, i))) {
			if (qc->scsicmd == cmd)
				break;
		}
	}
	if (i == ATA_MAX_QUEUE) {
		ata_port_printk(ap, KERN_INFO, "%s: cmd %p not found\n", 
				__FUNCTION__, cmd);
		goto exit_abort;
	}

	timeout = jiffies + 60 * HZ; /* 60 seconds max */
	for (i = 0; i < 5; i++) {
		if (ata_cmd_completed(ap, qc)) {
			ata_port_printk(ap, KERN_INFO, 
					"%s: cmd %p completed within %d seconds\n",
					__FUNCTION__, cmd, i+1);
			rc = SUCCESS;
			break;
		}
		spin_unlock_irqrestore(ap->lock, flags);
		msleep(1000);
		spin_lock_irqsave(ap->lock, flags);
	}

	if (rc != SUCCESS) {
		/* Cmd not completed, reset the port */
		qc->err_mask |= AC_ERR_TIMEOUT;
		spin_unlock_irqrestore(ap->lock, flags);
		rc = ata_port_reset(ap);
		spin_lock_irqsave(ap->lock, flags);
		while(1) {
			if (ata_cmd_completed(ap, qc)) {
				rc = SUCCESS;
				break;
			}
			if (time_after(jiffies, timeout)) {
				ata_port_printk(ap, KERN_ERR, 
						"%s: cmd %p not completed "
						"within 60 seconds\n",
						__FUNCTION__, cmd);
				break;
			}
			spin_unlock_irqrestore(ap->lock, flags);
			msleep(1000);
			spin_lock_irqsave(ap->lock, flags);
		}
	}

exit_abort:
	spin_unlock_irqrestore(ap->lock, flags);
	ata_port_printk(ap, KERN_INFO, "%s: cmd %p, %s.\n", 
			__FUNCTION__, cmd, (rc==SUCCESS)?"SUCCEEDED":"FAILED");
	return rc;
}

int ata_scsi_device_reset(struct scsi_cmnd *cmd)
{
	struct ata_port *ap = ata_shost_to_port(cmd->device->host);

	ata_port_printk(ap, KERN_INFO, "%s: cmd %p, code %02x\n", 
			__FUNCTION__, cmd, cmd->cmnd[0]);
	
	return ata_port_reset(ap); 
}

int ata_scsi_bus_reset(struct scsi_cmnd *cmd)
{
	struct ata_port *ap = ata_shost_to_port(cmd->device->host);

	ata_port_printk(ap, KERN_INFO, "%s: cmd %p, code %02x\n", 
			__FUNCTION__, cmd, cmd->cmnd[0]);
	
	return ata_port_reset(ap); 
}

int ata_scsi_host_reset(struct scsi_cmnd *cmd)
{
	struct ata_port *ap = ata_shost_to_port(cmd->device->host);

	ata_port_printk(ap, KERN_INFO, "%s: cmd %p, code %02x\n", 
			__FUNCTION__, cmd, cmd->cmnd[0]);
	
	return ata_port_reset(ap); 
}

#endif /* defined(__VMLNX__) */
