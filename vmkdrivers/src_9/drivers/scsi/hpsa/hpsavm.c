/*
 *    Disk Array driver for HP Smart Array SAS controllers
 *    Copyright 2000-2012 Hewlett-Packard Development Company, L.P.
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; version 2 of the License.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *    NON INFRINGEMENT.  See the GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *    Questions/Comments/Bugfixes to iss_storagedev@hp.com
 *
 */
/*
 * Portions Copyright 2008-2012 VMware, Inc.
 */
/*
 * This file contains VMware-specific changes to the standard Linux hpsa driver.
 */


/* hpsa_limit_maxsgentries
 *	Set h->maxsgentries to reduce total memory consumed by sg chaining
 *	on VMware, since total memory consumed by driver for all controllers
 *	needs to be < 30MB.
 */
static inline void hpsa_limit_maxsgentries(struct ctlr_info *h)
{
	if ( h->maxsgentries > HPSA_VMWARE_SGLIMIT ) {
		hpsa_dbmsg(h, 0, "Reducing controller-supported maxsgentries "
			"of %d to %d to conserve driver heap memory.\n",
			h->maxsgentries, HPSA_VMWARE_SGLIMIT);
		h->maxsgentries = HPSA_VMWARE_SGLIMIT;
	}
}

/**
 * scsi_device_type - Return 17 char string indicating device type.
 * @type: type number to look up
 */
const char * scsi_device_type(unsigned type) 
{
	if (type == 0x1e)
		return "Well-known LUN   ";
	if (type == 0x1f)
		return "No Device        ";
	if (type >= ARRAY_SIZE(scsi_device_types))
		return "Unknown          ";
	return scsi_device_types[type];
}

/**
 * scsi_dma_map - perform DMA mapping against command's sg lists
 * @cmd:	scsi command
 *
 * Returns the number of sg lists actually used, zero if the sg lists
 * is NULL, or -ENOMEM if the mapping failed.
 */
int scsi_dma_map(struct scsi_cmnd *cmd)
{
	int nseg = 0;
	
	if (scsi_sg_count(cmd)) {
		struct device *dev = cmd->device->host->shost_gendev.parent;

		nseg = dma_map_sg(dev, scsi_sglist(cmd), scsi_sg_count(cmd),
			cmd->sc_data_direction);
		if (unlikely(!nseg))
			return -ENOMEM;
	}
	return nseg;
}

/**
 * scsi_dma_unmap - unmap command's sg lists mapped by scsi_dma_map
 * @cmd:        scsi command
 */
void scsi_dma_unmap(struct scsi_cmnd *cmd)
{
	if (scsi_sg_count(cmd)) {
		struct device *dev = cmd->device->host->shost_gendev.parent;
		dma_unmap_sg(dev, scsi_sglist(cmd), scsi_sg_count(cmd),
			cmd->sc_data_direction);
	}
}

/* hpsa_dbmsg
 *	Print the indicated message,
 *	with variable args,
 *	if controller's debug level
 *	is at or above given value.
 */
static void hpsa_dbmsg(struct ctlr_info *h, int debuglvl, char *fmt, ...)
{
	if(h->debug_msg >= debuglvl) {
		char msg[256];
		va_list argp;   /* pointer to var args */
		va_start(argp, fmt);
		vsprintf(msg, fmt, argp);
		va_end(argp);
		printk(KERN_WARNING "hpsa%d: %s", h->ctlr, msg);
	}
}


/* hpsa_db_dev
 * Show some hpsa device information.
 */
static void hpsa_db_dev(char * label, struct ctlr_info *h, struct hpsa_scsi_dev_t *d, int dnum)
{
	if (dnum < 0 ) {
		/* don't include device number if it is irrelevant: */
		hpsa_dbmsg(h, 5, "%s: B%d:T%d:L%d\n", 
			label, d->bus, d->target, d->lun);
		hpsa_dbmsg(h, 5, "%s: devtype:    "
			"0x%02x\n", label, d->devtype);
		hpsa_dbmsg(h, 5, "%s: vendor:     "
			"%.8s\n", label, d->vendor);
		hpsa_dbmsg(h, 5, "%s: model:      "
			"%.16s\n", label, d->model);
		hpsa_dbmsg(h, 5, "%s: revision:   "
			"%.4s\n", label, d->revision);
		hpsa_dbmsg(h, 5, "%s: device_id:  "
			"0x%02x%02x%02x%02x %02x%02x%02x%02x "
			"%02x%02x%02x%02x %02x%02x%02x%02x\n",
			label, dnum,
			d->device_id[0], d->device_id[1],
			d->device_id[2], d->device_id[3],
			d->device_id[4], d->device_id[5],
			d->device_id[6], d->device_id[7],
			d->device_id[8], d->device_id[9],
			d->device_id[10], d->device_id[11],
			d->device_id[12], d->device_id[13],
			d->device_id[14], d->device_id[15]);
		hpsa_dbmsg(h, 5, "%s: raid_level: "
			"%d\n", label, d->raid_level);
		hpsa_dbmsg(h, 5, "%s: scsi3addr:  "
			"0x%02x%02x%02x%02x %02x%02x%02x%02x\n",
			label, 
			d->scsi3addr[0], d->scsi3addr[1],
			d->scsi3addr[2], d->scsi3addr[3],
			d->scsi3addr[4], d->scsi3addr[5],
			d->scsi3addr[6], d->scsi3addr[7]);
	}
	else {
		/* include device number in messages*/
		hpsa_dbmsg(h, 5, "%s: device %d: B%d:T%d:L%d\n", 
			label, dnum, d->bus, d->target, d->lun);
		hpsa_dbmsg(h, 5, "%s: device %d: devtype:    "
			"0x%02x\n", label, dnum, d->devtype);
		hpsa_dbmsg(h, 5, "%s: device %d: vendor:     "
			"%.8s\n", label, dnum, d->vendor);
		hpsa_dbmsg(h, 5, "%s: device %d: model:      "
			"%.16s\n", label, dnum, d->model);
		hpsa_dbmsg(h, 5, "%s: device %d: revision:   "
			"%.4s\n", label, dnum, d->revision);
		hpsa_dbmsg(h, 5, "%s: device %d: device_id:  "
			"0x%02x%02x%02x%02x %02x%02x%02x%02x "
			"%02x%02x%02x%02x %02x%02x%02x%02x\n",
			label, dnum,
			d->device_id[0], d->device_id[1],
			d->device_id[2], d->device_id[3],
			d->device_id[4], d->device_id[5],
			d->device_id[6], d->device_id[7],
			d->device_id[8], d->device_id[9],
			d->device_id[10], d->device_id[11],
			d->device_id[12], d->device_id[13],
			d->device_id[14], d->device_id[15]);
		hpsa_dbmsg(h, 5, "%s: device %d: raid_level: "
			"%d\n", label, dnum, d->raid_level);
		hpsa_dbmsg(h, 5, "%s: device %d: scsi3addr:  "
			"0x%02x%02x%02x%02x %02x%02x%02x%02x\n",
			label, dnum,
			d->scsi3addr[0], d->scsi3addr[1],
			d->scsi3addr[2], d->scsi3addr[3],
			d->scsi3addr[4], d->scsi3addr[5],
			d->scsi3addr[6], d->scsi3addr[7]);
	}
}

/* hpsa_db_sdev
 * Show some scsi device information.
 */
static void hpsa_db_sdev(char * label, struct ctlr_info *h, struct scsi_device *d)
{
	hpsa_dbmsg(h, 5, "%s: B%d:T%d:L%d\n", 
		label, d->channel, d->id, d->lun);
	hpsa_dbmsg(h, 5, "%s: Mfg:         "
		"%u\n", label, d->manufacturer);
	hpsa_dbmsg(h, 5, "%s: type:        "
		"0x%02x\n", label, d->type);
	hpsa_dbmsg(h, 5, "%s: scsi_level:  "
		"0x%02x\n", label, d->scsi_level);
	hpsa_dbmsg(h, 5, "%s: periph_qual: "
		"0x%02x\n", label, d->inq_periph_qual);
	hpsa_dbmsg(h, 5, "%s: vendor:      "
		"%.8s\n", label, d->vendor);
	hpsa_dbmsg(h, 5, "%s: model:       "
		"%.16s\n", label, d->model);
	hpsa_dbmsg(h, 5, "%s: rev:         "
		"%.4s\n", label, d->rev);
	hpsa_dbmsg(h, 5, "%s: scsi_target: "
		"B%d:T%d\n", label, 
		d->sdev_target->channel, 
		d->sdev_target->id);
}

/* hpsa_db_scmnd
 * Show some scsi command information
 */
static void hpsa_db_scmnd(char * label, struct ctlr_info *h, struct scsi_cmnd *c)
{

	if (c == NULL ) {
		hpsa_dbmsg(h, 5, "%s: command: NULL\n", label);
		return;
	}
	if (c->device == NULL ) {
		hpsa_dbmsg(h, 5, "%s: command: DEVICE is NULL\n", label);
		return;
	}
	
	hpsa_dbmsg(h, 5, "%s: command: sent to device : B%d:T%d:L%d\n", 
		label, c->device->channel, c->device->id, c->device->lun);
	hpsa_dbmsg(h, 5, "%s: command: SN:              %lu\n", 
		label, c->serial_number);
	//hpsa_dbmsg(h, 3, "%s: command: cmd_len:         %u\n", 
	//	label, c->cmd_len);
	hpsa_dbmsg(h, 5, "%s: command: request_bufflen: %u\n", 
		label, c->request_bufflen);

	hpsa_dbmsg(h, 5, "%s: command:                  "
		"[%02x %02x %02x %02x %02x %02x %02x %02x"
		" %02x %02x %02x %02x %02x %02x %02x %02x]\n",
		label, 
		c->cmnd[0], c->cmnd[1], c->cmnd[2], c->cmnd[3],
		c->cmnd[4], c->cmnd[5], c->cmnd[6], c->cmnd[7],
		c->cmnd[8], c->cmnd[9], c->cmnd[10], c->cmnd[11],
		c->cmnd[12], c->cmnd[13], c->cmnd[14], c->cmnd[15]);
	hpsa_dbmsg(h, 5, "%s: command: use_sg:           %u\n", 
		label, c->use_sg);
	hpsa_dbmsg(h, 5, "%s: command: sglist_len:       %u\n", 
		label, c->sglist_len);
	hpsa_dbmsg(h, 5, "%s: command: underflow:        %u\n", 
		label, c->underflow);
	hpsa_dbmsg(h, 5, "%s: command: transfersize:     %u\n", 
		label, c->transfersize);
	hpsa_dbmsg(h, 5, "%s: command: resid:            %u\n", 
		label, c->resid);
	hpsa_dbmsg(h, 5, "%s: command: result:           %d\n", 
		label, c->result);
}

/* hpsa_set_sas_ids
 *      This is used within update_scsi_devices function.
 *      Use extended report physical luns to get the 
 *      sas ID of each target and record in a global
 *      array that stores sas IDs for each target, by controller.
 *
 *      Likewise, use sense_susbsystem function to get 
 *      the SAS ID of the controller itself and store in a 
 *      separate global array of controller SAS IDs. 
 *      The controller's SAS Id is also used to mock up 
 *      a target SAS ID for controller#:Target 0.
 *
 *      These controller and target SAS IDs are used by 
 *      the various SAS transport query functions.
 *
 *      Returns 1 on success, 0 on failure.
 */
static int
hpsa_set_sas_ids(struct ctlr_info *h, 
	int reportlunsize, 
	struct ReportExtendedLUNdata *physdev, 
	struct SenseSubsystem_info *senseinfo, 
	u32 *nphysicals)
{

	int i, j;
	int sas_target = 0;
	u64 sas_id = 0;
	char *ptr = (char *)&sas_id;

	if (hpsa_scsi_do_ext_report_luns(h, 0, physdev, reportlunsize, 2)) {
		printk(KERN_ERR "hpsa%d: set_sas_ids: "
			"report extended physical LUNs failed.\n", h->ctlr);
		//dev_err(&h->pdev->dev, "set_sas_ids:report extended physical LUNs failed.\n");
		return -1;
	}
	*nphysicals = be32_to_cpu(*((__be32 *)physdev->LUNListLength)) / 24;
	if (*nphysicals > HPSA_MAX_PHYS_LUN) {
		printk(KERN_ERR "hpsa%d: set_sas_ids: "
			"max physical LUNs (%d) exceeded."
			"%d LUNs ignored.\n", h->ctlr,  
			HPSA_MAX_PHYS_LUN,
			*nphysicals - HPSA_MAX_PHYS_LUN);
		//dev_warn(&h->pdev->dev, "set_sas_ids: maximum physical LUNs (%d) exceeded."
		//	"  %d LUNs ignored.\n", HPSA_MAX_PHYS_LUN,
		//	*nphysicals - HPSA_MAX_PHYS_LUN);
		*nphysicals = HPSA_MAX_PHYS_LUN;
	}

	
	/* Process each device looking for controller devices */
	for (i = 0; i < *nphysicals; i++) 
	{
		sas_target = (int)physdev->LUN[i][3] & 0x3f;
		
		/* reverse endianness */
		ptr = (char *)&sas_id;
		for (j = 15; j > 7; j--)
			*ptr++ = physdev->LUN[i][j];
	
		/* Is it a controller device? */
		if (physdev->LUN[i][16] == CONTROLLER_DEVICE)
		{

			if (sas_target > MAX_EXT_TARGETS)
			{
				printk(KERN_ERR "hpsa%d: set_sas_ids: "
					"sas target exceeds max targets=%d\n", 
					sas_target, MAX_EXT_TARGETS);
				return -1;
			}
			else 
				/* save sas id by controller number. */
				target_sas_id[h->ctlr][sas_target] = sas_id;
				hpsa_dbmsg(h, 3, "set_sas_ids: "
					"Target C%d:T%d ID = 0x%llx\n", 
					h->scsi_host->host_no, sas_target,
					target_sas_id[h->ctlr][sas_target] );
		}
	}

	/*
	 * Set the controller SAS IDs
	 * (and C#:T0 SAS IDs)
	 */
	if (hpsa_sense_subsystem_info(h, senseinfo, 
		sizeof(struct SenseSubsystem_info))) {
		printk(KERN_ERR "hpsa: sense subsystem info failed. \n");
		return -1;
	}
	ptr = (char *)&sas_id;

	/* reverse endianness */
	for (j = 7; j >= 0; j--)
		*ptr++ = senseinfo->portname[j];

	/* Save off controller sas_id by controller number */
	cntl_sas_id[h->ctlr] = sas_id;
	target_sas_id[h->ctlr][0] = 
		sas_id & 0x00ffffffffffffff;

	hpsa_dbmsg(h, 3, "set_sas_ids: "
		"Controller C%d ID = 0x%llx\n", 
		h->scsi_host->host_no, cntl_sas_id[h->ctlr]);
	hpsa_dbmsg(h, 3, "set_sas_ids: "
		"Target C%d:T0 ID = 0x%llx\n", 
		h->scsi_host->host_no, target_sas_id[h->ctlr][0] );
	return 0;
}

static int hpsa_sense_subsystem_info(struct ctlr_info *h, 
		struct SenseSubsystem_info *buf, int bufsize) 
{
	int rc = IO_OK;
	struct CommandList *c;
	unsigned char scsi3addr[8];
	struct ErrorInfo *ei;
	
	hpsa_dbmsg(h, 1, "sense_subsystem_info.\n");
	c = cmd_special_alloc(h);
	if (c == NULL) {                        /* trouble... */
		dev_err(&h->pdev->dev, "cmd_special_alloc returned NULL!\n");
		return -1;
	}
	/* address the controller */
	memset(scsi3addr, 0, sizeof(scsi3addr));
	fill_cmd(c, HPSA_SENSE_SUBSYSTEM_INFO, h,
		buf, bufsize, 0, scsi3addr, TYPE_CMD);

	hpsa_scsi_do_simple_cmd_with_retry(h, c, PCI_DMA_FROMDEVICE);
	ei = c->err_info;
	if (ei->CommandStatus != 0 &&
		ei->CommandStatus != CMD_DATA_UNDERRUN) {
		hpsa_scsi_interpret_error(c);
		rc = -1;
	}
       	cmd_special_free(h, c);
	return rc;
}

static int
hpsa_get_linkerrors(struct sas_phy *phy)
{
    /* dummy function - placeholder */
    return 0;
}

static int
hpsa_get_enclosure_identifier(struct sas_rphy *rphy, u64 *identifier)
{
   /* dummy function - placeholder */
   return 0;
}

static int
hpsa_get_bay_identifier(struct sas_rphy *rphy)
{
   /* dummy function - placeholder */
   return 0;
}

static int
hpsa_phy_reset(struct sas_phy *phy, int hard_reset)
{
   /* dummy function - placeholder */
   return 0;
}

static int
hpsa_get_initiator_sas_identifier(struct Scsi_Host *sh, u64 *sas_id)
{
        struct ctlr_info *ci;
        int ctlr;
        ci = (struct ctlr_info *) sh->hostdata[0];
        if (ci == NULL) {
        	printk(KERN_INFO "hpsa: hpsa_get_sas_initiator_identifier: "
			"hostdata is NULL for host_no %d\n", sh->host_no);
        	return -EINVAL;
        }
        ctlr = ci->ctlr;
        if (ctlr > MAX_CTLR || ctlr < 0) {
                printk(KERN_INFO "hpsa: hpsa_get_sas_initiator_identifier: "
			"MAX_CTLR exceeded.\n");
                return -EINVAL;
        }
        *sas_id = cntl_sas_id[ctlr];

   return SUCCESS;
}

static int
hpsa_get_target_sas_identifier(struct scsi_target *starget, u64 *sas_id)
{
        struct ctlr_info *ci;
        int ctlr;
        struct Scsi_Host *host = dev_to_shost(&starget->dev);

        ci = (struct ctlr_info *) host->hostdata[0];
        if (ci == NULL) {
        	printk(KERN_INFO "hpsa: hpsa_get_sas_target_identifier:"
			" hostdata is NULL\n");
	        return -EINVAL;
        }
        ctlr = ci->ctlr;
        if (ctlr > MAX_CTLR || ctlr < 0) {
                printk(KERN_INFO "hpsa: hpsa_get_sas_target_identifier: "
			"MAX_CTLR exceeded.\n");
                return -EINVAL;
        }
        *sas_id = target_sas_id[ctlr][starget->id];

   return SUCCESS;
}

/*
 * Report information about this controller.
 */
static int hpsa_proc_get_info(char *buffer, char **start, off_t offset,
			       int length, int *eof, void *data)
{
	off_t len = 0;
	int size, i, ctlr;
	int logicals = 0;
	struct ctlr_info *h = (struct ctlr_info *) data;
	struct hpsa_scsi_dev_t *drv;
	unsigned long flags;
#define HPSA_MAXPROCINFO_LINE 256
	char line[HPSA_MAXPROCINFO_LINE];
	static int loop_resume = 0;

	ctlr = h->ctlr;

	/* prevent displaying bogus info during configuration
	 * or deconfiguration of a logical volume
	 */
	spin_lock_irqsave(&h->lock, flags);
	if (h->busy_initializing) {
		spin_unlock_irqrestore(&h->lock, flags);
		return -EBUSY;
	}
	h->busy_initializing = 1;
	spin_unlock_irqrestore(&h->lock, flags);

	if(!offset)
	{
		/* count the logical disk devices */
		for (i = 0; i < h->ndevices; i++) {
			drv = h->dev[i];
			if (drv == NULL )
				continue;
			if (drv->devtype == TYPE_DISK)
				logicals++;
		}

		size = sprintf(buffer, "%s: HP %s Controller\n"
			"Board ID: 0x%08lx\n"
			"Firmware Version: %c%c%c%c\n"
			"Driver Version: %s\n"
			"Driver Build: %s\n"
			"IRQ: %d\n"
			"Logical drives: %d\n"
			"Current Q depth: %d\n"
			"Current # commands on controller: %d\n"
			"Max Q depth since init: %d\n"
			"Max # commands on controller since init: %d\n"
			"Max SG entries since init: %d\n"
			"Max Commands supported: %d\n"
			"SCSI host number: %d\n"
			"Offline volume monitoring: %s\n\n",
			h->devname, h->product_name,
			(unsigned long)h->board_id,
			h->firm_ver[0], h->firm_ver[1],
			h->firm_ver[2], h->firm_ver[3],
			DRIVER_NAME, DRIVER_BUILD,
			(unsigned int)h->intr[SIMPLE_MODE_INT],
			logicals, h->Qdepth, h->commands_outstanding,
			h->maxQsinceinit, h->max_outstanding, h->maxSG,
			h->nr_cmds, h->scsi_host->host_no,
			(h->offline_device_thread_state == OFFLINE_DEVICE_THREAD_STOPPED) ? "Not running": "Running");
		len += size;
	}

	for (i = loop_resume; i < h->ndevices; i++) {
		drv = h->dev[i];
		if (drv == NULL )
			continue;
		/* Only show disk and enclosure information */
		if (drv->devtype != TYPE_DISK &&
			drv->devtype != TYPE_ENCLOSURE)
			continue;

                if (drv->raid_level > RAID_UNKNOWN)
			drv->raid_level = RAID_UNKNOWN;

		size = snprintf(line, HPSA_MAXPROCINFO_LINE, "hpsa%d/"
			"C%d:B%d:T%d:L%d"
                	"\t%s\t%.16s\t%.4s\tRAID %s\n",
                        ctlr, h->scsi_host->host_no,
			drv->bus, drv->target, drv->lun,
			scsi_device_type(drv->devtype),
			drv->model, drv->revision,
			raid_label[drv->raid_level]);
		/* avoid buffer overflow */
		if ((len + size) > length) {
			loop_resume = i;
			break;
		}
		sprintf(buffer + len, "%s", line);

		len += size;
	}

	if (len == 0 ||
		i == h->ndevices) {
		*eof = 1;
		loop_resume = 0;
	}
	else {
		*eof = 0;
	}

	*start = buffer;
	h->busy_initializing = 0;
	return len;
}

static int
hpsa_proc_write(struct file *file, const char __user *buffer,
		 unsigned long count, void *data)
{
	struct ctlr_info *h = (struct ctlr_info *) data;

	unsigned char cmd[20];
	int len;

	if (count > sizeof(cmd) -1)
		return -EINVAL;

	memcpy(cmd, buffer, count);
	cmd[count]='\0';
	len = strlen(cmd);

	if (cmd[len-1] == '\n')
		cmd[--len] = '\0';


	if (hpsa_set_debug(h, cmd) )
		return count;

	return -EINVAL;

}

/*
 * Get us a file in /proc/hpsa that says something about each controller.
 * Create /proc/hpsa if it doesn't exist yet.
 */
//static void __devinit hpsa_procinit(int i)
static void __devinit hpsa_procinit(struct ctlr_info *h)
{
	struct proc_dir_entry *pde;

	if (proc_hpsa == NULL) {
		proc_hpsa = proc_mkdir("hpsa", proc_root_driver);
		if (!proc_hpsa)
			return;
	}

	pde = create_proc_read_entry(h->devname,
				     S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH,
				     proc_hpsa, hpsa_proc_get_info, h);
	pde->write_proc = hpsa_proc_write;
}

/* hpsa_set_debug
 * 	Set controller debug level feature 
 *	and return true if proc_write content
 * 	is a command to set or change debug level.
 *	Otherwise return false.
 */
static int hpsa_set_debug(struct ctlr_info *h, char *cmd)
{
	unsigned long flags;

        /* debug printing level */
	if (strncmp("debug", cmd, 5)==0) {
		spin_lock_irqsave( &h->lock, flags);
		if (cmd[5] == '\0')
			h->debug_msg=1;
		else if (cmd[5] == '1')
			h->debug_msg=1;
		else if (cmd[5] == '2')
			h->debug_msg=2;
		else if (cmd[5] == '3')
			h->debug_msg=3;
		else if (cmd[5] == '4')
			h->debug_msg=4;
		else if (cmd[5] == '5')
			h->debug_msg=5;
		else
			h->debug_msg=0;
		spin_unlock_irqrestore(&h->lock, flags);
		printk(KERN_WARNING
			"hpsa%d: debug messaging set to level %d\n",
			h->ctlr, h->debug_msg);
		return 1;
	}
	/* no debug printing */
	else if (strcmp("nodebug", cmd)==0) {
		spin_lock_irqsave(&h->lock, flags);
		h->debug_msg=0;
		spin_unlock_irqrestore(&h->lock, flags);
		printk(KERN_WARNING
			"hpsa%d: Disabled debug messaging.\n",
			h->ctlr);
		return 1;
	}
	return 0;
}

/* hpsa_set_tmf_support
 *      Determine Task Management capabilities of controller
 *      and remember these in controller info structure so we 
 *      don't have to query controller every time.
 */
static void hpsa_set_tmf_support(struct ctlr_info *h)
{
        h->TMFSupportFlags = readl(&(h->cfgtable->TMFSupportFlags));
        if (h->TMFSupportFlags & HPSATMF_BITS_SUPPORTED) {
		hpsa_dbmsg(h, 0, "Task Management function bits"
			" are supported on this controller.\n");

		/* Mask support */
                if ( h->TMFSupportFlags & HPSATMF_MASK_SUPPORTED)
			hpsa_dbmsg(h, 0, "TMF Support for: "
				"Task Management Functions MASK\n");

		/* Physical abilities */
                if ( h->TMFSupportFlags & HPSATMF_PHYS_LUN_RESET)
			hpsa_dbmsg(h, 0, "TMF Support for: "
				"Physical LUN Reset\n");
                if ( h->TMFSupportFlags & HPSATMF_PHYS_NEX_RESET)
			hpsa_dbmsg(h, 0, "TMF Support for: "
				"Physical Nexus Reset\n");
                if ( h->TMFSupportFlags & HPSATMF_PHYS_TASK_ABORT)
			hpsa_dbmsg(h, 0, "TMF Support for: "
				"Physical Task Aborts\n");
                if ( h->TMFSupportFlags & HPSATMF_PHYS_TSET_ABORT)
			hpsa_dbmsg(h, 0, "TMF Support for: "
				"Physical Task Set Abort\n");
                if ( h->TMFSupportFlags & HPSATMF_PHYS_CLEAR_ACA)
			hpsa_dbmsg(h, 0, "TMF Support for: "
				"Physical Clear ACA\n");
                if ( h->TMFSupportFlags & HPSATMF_PHYS_CLEAR_TSET)
			hpsa_dbmsg(h, 0, "TMF Support for: "
				"Physical Clear Task Set\n");
                if ( h->TMFSupportFlags & HPSATMF_PHYS_QRY_TASK)
			hpsa_dbmsg(h, 0, "TMF Support for: "
				"Physical Query Task\n");
                if ( h->TMFSupportFlags & HPSATMF_PHYS_QRY_TSET)
			hpsa_dbmsg(h, 0, "TMF Support for: "
				"Physical Query Task Set\n");
                if ( h->TMFSupportFlags & HPSATMF_PHYS_QRY_ASYNC)
			hpsa_dbmsg(h, 0, "TMF Support for: "
				"Physical Query Async\n");


		/* Logical abilities */
                if ( h->TMFSupportFlags & HPSATMF_LOG_LUN_RESET)
			hpsa_dbmsg(h, 0, "TMF Support for: "
				"Logical LUN Reset\n");
                if ( h->TMFSupportFlags & HPSATMF_LOG_NEX_RESET)
			hpsa_dbmsg(h, 0, "TMF Support for: "
				"Logical Nexus Reset\n");
                if ( h->TMFSupportFlags & HPSATMF_LOG_TASK_ABORT)
			hpsa_dbmsg(h, 0, "TMF Support for: "
				"Logical Task Aborts\n");
                if ( h->TMFSupportFlags & HPSATMF_LOG_TASK_ABORT)
			hpsa_dbmsg(h, 0, "TMF Support for: "
				"Logical Task Aborts\n");
                if ( h->TMFSupportFlags & HPSATMF_LOG_TSET_ABORT)
			hpsa_dbmsg(h, 0, "TMF Support for: "
				"Logical Task Set Abort\n");
                if ( h->TMFSupportFlags & HPSATMF_LOG_CLEAR_ACA)
			hpsa_dbmsg(h, 0, "TMF Support for: "
				"Logical Clear ACA\n");
                if ( h->TMFSupportFlags & HPSATMF_LOG_CLEAR_TSET)
			hpsa_dbmsg(h, 0, "TMF Support for: "
				"Logical Clear Task Set\n");
                if ( h->TMFSupportFlags & HPSATMF_LOG_QRY_TASK)
			hpsa_dbmsg(h, 0, "TMF Support for: "
				"Logical Query Task\n");
                if ( h->TMFSupportFlags & HPSATMF_LOG_QRY_TSET)
			hpsa_dbmsg(h, 0, "TMF Support for: "
				"Logical Query Task Set\n");
                if ( h->TMFSupportFlags & HPSATMF_LOG_QRY_ASYNC)
			hpsa_dbmsg(h, 0, "TMF Support for: "
				"Logical Query Async \n");
        }
        else 
		hpsa_dbmsg(h, 0, "Task Management function bits"
			" are not supported on this controller.\n");
}

/* hpsa_cmd_waiting
 *      Used to determine whether a command (find) is still waiting 
 *	to be completed. 
 *	Used to to avoid unnecessary aborts. 
 *	Used after aborts to detect if/when aborted command completes.
 *      Returns 
 *	0 if command is already completed. 
 *	1 if command is still waiting.
 */
static int hpsa_cmd_waiting(struct ctlr_info *h, struct scsi_cmnd *find)
{
        unsigned long flags;
	struct CommandList *c = NULL;	/* ptr into cmpQ */
	struct scsi_cmnd *waiting;	
        int found = 0;

	spin_lock_irqsave(&h->lock, flags);
	
	if (find == NULL ) {
		hpsa_dbmsg(h, 2, "hpsa_cmd_waiting: "
			"NULL lookup value.\n");
		spin_unlock_irqrestore(&h->lock, flags);
		return found;
	}

	hpsa_dbmsg(h, 2, "Search in cmpQ: "
		"C%d:B%d:T%d:L%d SN:0x%lx\n",
		h->scsi_host->host_no, find->device->channel,
		find->device->id, find->device->lun,
		find->serial_number);
	
	list_for_each_entry(c, &h->cmpQ, list) {
		waiting = (struct scsi_cmnd *)c->scsi_cmd;
		if (waiting == NULL) {
			hpsa_dbmsg(h, 2, "hpsa_cmd_waiting: NULL scsi_cmnd\n");
			continue;
		}
#ifdef HPSA_DEBUG
		hpsa_dbmsg(h, 4, "Cmd waits in Q: "
			"C%d:B%d:T%d:L%d SN:0x%lx %x\n",
			c->h->scsi_host->host_no, waiting->device->channel,
			waiting->device->id, waiting->device->lun, 
			waiting->serial_number, waiting->cmnd[0]);
#endif /* HPSA_DEBUG */
		if (waiting == find) {
			found = 1;
			hpsa_dbmsg(h, 2, "Found cmd in Q: "
				"C%d:B%d:T%d:L%d SN:0x%lx " 
		        	"Tag:0x%08x:%08x\n",
				h->scsi_host->host_no, 
				waiting->device->channel, waiting->device->id,
				waiting->device->lun, waiting->serial_number,
				c->Header.Tag.upper, c->Header.Tag.lower);
			break;
		}
	} 	
	
	spin_unlock_irqrestore(&h->lock, flags);
	return found;
}


/*
 * decode_CC
 * 	Decode check condition codes.
 *	For certain types of errors,
 * 	change command's result and/or 
 * 	change command's residual count.
 * Args: 
 * 	h	The controller info pointer
 * 	cp	The command pointer
 * 	msg	Buffer for detailed decode messages
 * 	ml 	Detail message length, cumulative 
 * 	dmsg	Buffer for error decode msgs for dev_warn
 * 	dl 	dev_warn message length, cumulative
 * Returns:
 *	1 - if we changed the cmd->result.
 *	0 - if we did not change it.
 */
static int decode_CC( 
	struct ctlr_info *h,
	struct CommandList *cp,
	char *msg, 
	int *ml, 
	char *dmsg, 
	int *dl) 
{
	struct scsi_cmnd *cmd;
	struct ErrorInfo *ei;

	unsigned char sense_key;
	unsigned char asc;      /* additional sense code */
	unsigned char ascq;     /* additional sense code qualifier */
	int orig;		/* For detecting when we change cmd result */
	
        ei = cp->err_info;
        cmd = (struct scsi_cmnd *) cp->scsi_cmd;
	if ( cmd == NULL )
		return 0;
	orig = cmd->result;	/* remember the original result */
		
	if ( ei->ScsiStatus  ) {
		sense_key = 0xf & ei->SenseInfo[2];   
		asc = ei->SenseInfo[12];             
		ascq = ei->SenseInfo[13];           
	}
	else {
		sense_key = 0;
		asc = 0;
		ascq = 0;
	} 

	
	switch(sense_key) 
	{
	
	case RECOVERED_ERROR:
		*ml+=sprintf(msg+*ml,"RECOVERED_ERROR. ");
		*dl+=sprintf(dmsg+*dl,"Recovered error. ");
		break;

	case NOT_READY:
		*ml+=sprintf(msg+*ml, "NOT_READY. ");
		*dl+=sprintf(dmsg+*dl, "is not ready. ");
		*ml+=sprintf(msg+*ml, "Reset resid. ");
		cmd->resid = cmd->request_bufflen;

		if ( ( asc == 0x04)  && (ascq == 0x0) ) 
			*ml+=sprintf(msg+*ml, "Cause not reportable. ");
		else if ( ( asc == 0x04)  && (ascq == 0x01) ) 
			*ml+=sprintf(msg+*ml, "In process, becoming ready. ");
		else if ( ( asc == 0x04)  && (ascq == 0x02) )
			*ml+=sprintf(msg+*ml, "Start unit command needed. ");
		else if ( ( asc == 0x04)  && (ascq == 0x03) ) {
			cmd->result = DID_NO_CONNECT << 16;
			*ml+=sprintf(msg+*ml, "Manual Intervention required. ");
		}
		else if ( ( asc == 0x05)  && (ascq == 0x0) )
			*ml+=sprintf(msg+*ml, "Logical unit does not respond "
				"to selection. ");
		else if ( ( asc == STATE_CHANGED)  && (ascq == 0x06) ) 
			*ml+=sprintf(msg+*ml, "ALUA asymmetric access "
				"state change. ");
		else if ( ( asc == 0x3a)  && (ascq == 0x0) ) 
			*ml+=sprintf(msg+*ml, "Medium not present. ");
		else if ( ( asc == 0x3e)  && (ascq == 0x01) )
			*ml+=sprintf(msg+*ml, "Logical Unit Failure. ");
		else if ( ( asc == 0x3e)  && (ascq == 0x0) )
			*ml+=sprintf(msg+*ml, "LUN not configured. ");
		break;
	

	case MEDIUM_ERROR:
		*ml+=sprintf(msg+*ml, "MEDIUM ERROR. ");
		*dl+=sprintf(dmsg+*dl, "has medium error. ");
		*ml+=sprintf(msg+*ml, "Reset resid. ");
		cmd->resid = cmd->request_bufflen;

		if ( ( asc == 0x0c)  && (ascq == 0x0) ) 
			*ml+=sprintf(msg+*ml, "Unrecovered write. ");
		else if ( ( asc == 0x11)  && (ascq == 0x0) ) 
			*ml+=sprintf(msg+*ml, "Unrecovered read. ");
		break;


	case HARDWARE_ERROR:
		*ml+=sprintf(msg+*ml, "HARDWARE_ERROR. ");
		*dl+=sprintf(dmsg+*dl, "has hardware error. ");
		if ( ( asc == 0x0c)  && (ascq == 0x0) ) 
			*ml+=sprintf(msg+*ml, "Write error. ");
		break;

	case ILLEGAL_REQUEST:
		*ml+=sprintf(msg+*ml, "ILLEGAL_REQUEST. ");
		*dl+=sprintf(dmsg+*dl, "Illegal Request. ");
		*ml+=sprintf(msg+*ml, "Reset resid. ");
		cmd->resid = cmd->request_bufflen;

		/* 
		 * Suppress Illegal Request messages in hpsa for hpsa's 
		 * unsupported scsi commands. 
		 *
		 * Note: The decode for command 0x93 WRITE SAME (16) is not
		 * in the scsi.h decodes as a standard defined value. It
		 * is an optional SCSI command not supported in hpsa, but
		 * causes occasional log spew when the calling storage
		 * stacks invoke it.
		 */

		/* suppress msg for unsupported LOG_SENSE code */
		if (  (	cp->Request.CDB[0] == LOG_SENSE ) ) {
				*ml = 0;
				*dl = 0;
		}
		/* suppress msg for unsupported 0x93 WRITE SAME (16) code */
		else if (  (	cp->Request.CDB[0] == 0x93 ) ) {
				*ml = 0;
				*dl = 0;
		}
		 else if ( ( asc == 0x20)  && (ascq == 0x0) ) {
			*ml+=sprintf(msg+*ml, "Invalid OP Code. ");
			if (cp->Request.CDB[0] == REPORT_LUNS) {
				*ml = 0; /* suppress noisy complaint */
				*dl = 0; /* SA does not support report luns. */
			}
		}
		else if ( ( asc == 0x24)  && (ascq == 0x0) ) {
			*ml+=sprintf(msg+*ml, "Invalid field in CDB. ");

			/* suppress msg for unsupported inquiry codes */
			if  (	cp->Request.CDB[0] == INQUIRY
				&& 
				( cp->Request.CDB[2] != 0x00 &&
				cp->Request.CDB[2] != 0x83 &&
				cp->Request.CDB[2] != 0xC1 &&
				cp->Request.CDB[2] != 0xC2
				) ) {
				*ml = 0;
				*dl = 0;
			}
			/* suppress msg for unsupported sense codes */
			if  (	cp->Request.CDB[0] == MODE_SENSE
				&& 
				( cp->Request.CDB[2] != 0x00 &&
				cp->Request.CDB[2] != 0x01 &&
				cp->Request.CDB[2] != 0x03 &&
				cp->Request.CDB[2] != 0x04 &&
				cp->Request.CDB[2] != 0x08 &&
				cp->Request.CDB[2] != 0x3F &&
				cp->Request.CDB[2] != 0x19 
				) ) {
				*ml = 0; 
				*dl = 0; 
			}
		}
		else if ( ( asc == 0x25)  && (ascq == 0x0) ) {
			*ml+=sprintf(msg+*ml, "LUN not supported. ");
			*dl+=sprintf(dmsg+*dl, "LUN not supported. No Connection. ");

			/*
			 * Stop converting 0x05/0x25/0x00 to DID_NO_CONNECT to
			 * support PDL,PR 583042.
			 */
			cmd->resid = 0;
			cmd->result = DID_OK << 16;
		}
		else if ( ( asc == 0x26)  && (ascq == 0x04) )
			*ml+=sprintf(msg+*ml, "Invalid release of "
				"persistent reservation. ");
		else if ( ( asc == 0x27)  && (ascq == 0x01) )
			*ml+=sprintf(msg+*ml, "LUN is write protected "
				"by LUN mapping configuration. ");
		else if ( ( asc == 0x55)  && (ascq == 0x02) )
			*ml+=sprintf(msg+*ml, "Insufficient persistent "
				"reservation resources. ");
		break;

	case UNIT_ATTENTION:
		*ml+=sprintf(msg+*ml, "UNIT_ATTENTION. "); 
		*dl+=sprintf(dmsg+*dl, "has unit attention. "); 
		*ml+=sprintf(msg+*ml, "Reset resid. ");
		cmd->resid = cmd->request_bufflen;
		cmd->result = DID_SOFT_ERROR << 16;
		if ( ( asc == 0x28)  && (ascq == 0x00) )
			*ml+=sprintf(msg+*ml, "Logical Drive just created. ");
		else if ( ( asc == POWER_OR_RESET)  && (ascq == 0x00) ) 
			*ml+=sprintf(msg+*ml, "Power On, reset, or Target "
				"reset occurred. ");
		else if ( ( asc == POWER_OR_RESET)  && (ascq == 0x01) )
			*ml+=sprintf(msg+*ml, "Power On or controller "
				"reboot occurred. ");
		else if ( ( asc == POWER_OR_RESET)  && (ascq == 0x02) )
			*ml+=sprintf(msg+*ml, "LIP or SCSI bus "
				"reset occurred. ");
		else if ( ( asc == POWER_OR_RESET)  && (ascq == 0x03) ) 
			*ml+=sprintf(msg+*ml, "Target or Logical Drive "
				"reset occurred. ");
		else if ( ( asc == POWER_OR_RESET)  && (ascq == 0x04) )
			*ml+=sprintf(msg+*ml, "Controller Failover or "
				"reset occurred. ");
		else if ( ( asc == STATE_CHANGED)  && (ascq == 0x03) )
			*ml+=sprintf(msg+*ml, "Reservations preempted. ");
		else if ( ( asc == STATE_CHANGED)  && (ascq == 0x06) ) 
			*ml+=sprintf(msg+*ml, "ALUA asymmetric access "
				"state change. ");
		else if ( ( asc == STATE_CHANGED)  && (ascq == 0x07) ) 
			*ml+=sprintf(msg+*ml, "ALUA implicit  "
				"state transition failed. ");
		else if ( ( asc == STATE_CHANGED)  && (ascq == 0x09) )
			*ml+=sprintf(msg+*ml, "LUN size has changed. ");
		else if ( ( asc == 0x2f)  && (ascq == 0x00) )
			*ml+=sprintf(msg+*ml, "Commands cleared by "
				"another initiator. ");
		else if ( ( asc == 0x3F)  && (ascq == 0x0E) ) { 
			*ml+=sprintf(msg+*ml, "Reported LUN data changed. ");
			*ml+=sprintf(msg+*ml, "Waking rescan thread. ");
			wake_up_process(h->rescan_thread);
		}
		break; 

	case DATA_PROTECT:	
		*ml+=sprintf(msg+*ml, "DATA_PROTECT. ");
		*dl+=sprintf(dmsg+*dl, "Data is protected. ");
		break; 


	case BLANK_CHECK:	
		*ml+=sprintf(msg+*ml, "BLANK_CHECK. ");
		*dl+=sprintf(dmsg+*dl, "Blank Check. ");
		break; 


	case 0x09: /* Vendor-specific issues */
		*ml+=sprintf(msg+*ml, "HP-SPECIFIC ERROR. ");
		*dl+=sprintf(dmsg+*dl, "A vendor-specific error occured. ");
		if ( ( asc == 0x80)  && (ascq == 0x0) ) 
			*ml+=sprintf(msg+*ml, "Could not allocate memory. ");
		else if ( ( asc == 0x80)  && (ascq == 0x01) ) 
			*ml+=sprintf(msg+*ml, "Passthrough not allowed. ");
		else if ( ( asc == 0x80)  && (ascq == 0x02) ) 
			*ml+=sprintf(msg+*ml, "Invalid Backend channel. ");
		else if ( ( asc == 0x80)  && (ascq == 0x03) ) 
			*ml+=sprintf(msg+*ml, "Target did not respond. ");
		else if ( ( asc == 0x80)  && (ascq == 0x04) ) 
			*ml+=sprintf(msg+*ml, "General error. ");
		break;

	case COPY_ABORTED:
		*ml+=sprintf(msg+*ml, "COPY_ABORTED. ");
		*dl+=sprintf(dmsg+*dl, "Copy operation aborted. ");
		break;

	case ABORTED_COMMAND:
		*ml+=sprintf(msg+*ml, "ABORTED_COMMAND. "); 
		*dl+=sprintf(dmsg+*dl, "Aborted command. "); 
		*ml+=sprintf(msg+*ml, "Reset resid. ");
		cmd->resid = cmd->request_bufflen;
		if ( ( asc == STATE_CHANGED)  && (ascq == 0x06) ) 
			*ml+=sprintf(msg+*ml, 
				"ALUA asymmetric access state change: " 
				"Forwarded command aborted during failover. ");
		else if ( ( asc == 0x47)  && (ascq == 0x0) ) 
			*ml+=sprintf(msg+*ml, "Parity or CRC error in data. ");
		else if ( ( asc == 0x4e)  && (ascq == 0x0) ) 
			*ml+=sprintf(msg+*ml, "Overlapped commands. ");
		break;

	case VOLUME_OVERFLOW:
		*ml+=sprintf(msg+*ml, "VOLUME_OVERFLOW. ");
		*dl+=sprintf(dmsg+*dl, "detected volume overflow. ");
		break;

	case MISCOMPARE:
		*ml+=sprintf(msg+*ml, "MISCOMPARE. ");
		*dl+=sprintf(dmsg+*dl, "detected miscompare. ");
		break;

	default:
		*ml+=sprintf(msg+*ml, "UNKNOWN SENSE KEY. ");
		*dl+=sprintf(dmsg+*dl, "Had unknown sense key. ");
		break; 

	} /* end switch sense_key */


	/* If we changed the cmd->result, indicate that
	 * via the return value.
	 */
	if (cmd->result == orig )
		return 0;	/* no change */
	else 
		return 1;	/* changed */	
}

#ifdef HPSA_DEBUG
/* is_abort_cmd
 *	Return true if the indicated command
 *	is a Task Management abort request.
 */
static int is_abort_cmd(struct CommandList *c)
{
	if (c == NULL )
		return 0;
	if (	(c->cmd_type == CMD_IOCTL_PEND)
		&&
		(c->Request.CDB[0] == HPSA_TASK_MANAGEMENT)
		&&
		(c->Request.CDB[1] == HPSA_TMF_ABORT_TASK)
	)
		return 1;

	return 0;
}
#endif /* HPSA_DEBUG */

/* hpsa_create_rescan
 *	Create the rescan thread for this controller.
 * 	Returns 1 on success, 0 on failure.
 */
static int hpsa_create_rescan(struct ctlr_info *h)
{
        h->rescan_thread = kthread_create(hpsa_do_rescan, 
		h, "hpsa_rescan");
        if (IS_ERR(h->rescan_thread)) {
		dev_err(&h->pdev->dev, "hpsa%d: Unable to "
			"start rescan thread!\n", h->ctlr);
                return 0;
        }
	return 1;
}

/* hpsa_do_rescan
 *	Wake rescan thread to update scsi device view.
 */
static int hpsa_do_rescan(void *data)
{
	struct ctlr_info *h;
	h = (struct ctlr_info *)data;

	set_user_nice(current, -20);
	
	while (!kthread_should_stop()) {
	
		/* rescan thread sleeping */
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
	
		__set_current_state(TASK_RUNNING);
		
		/* rescan thread awake */
		hpsa_dbmsg(h, 2, "hpsa%d: rescan thread "
			"updating scsi devices\n", h->ctlr);
		hpsa_update_scsi_devices(h, h->scsi_host->host_no);
	}
	
	dev_warn(&h->pdev->dev, "hpsa%d: rescan thread "
		"STOPPED\n", h->ctlr);
	
	return 0;
}

/* hpsa_stop_rescan
 *	Kill the rescan thread for designated controller.
 */
static void hpsa_stop_rescan(struct ctlr_info *h)
{
        if (h->rescan_thread) {
                struct task_struct *t = h->rescan_thread;
                kthread_stop(t);
        }
}

/**
 * hpsa_char_open()
 * @inode - unused
 * @filep - unused
 * Allow only superuser to access this ioctl interface 
 */
static int hpsa_char_open (struct inode *inode, struct file *filep)
{
        if( !capable(CAP_SYS_ADMIN) ) return -EACCES;
        return 0;
}

static int hpsa_char_ioctl(struct inode *inode, struct file *f,
	unsigned cmd, unsigned long arg)
{
	int i;
	int found = 0;
	int chrmajor = 0;
	struct scsi_device dev;
	struct scsi_device *p_dev = &dev;

	chrmajor = MAJOR(inode->i_rdev);
	if (!chrmajor) {
		printk(KERN_WARNING "hpsa: Invalid Char Device major: %d\n",
		MAJOR(inode->i_rdev));
		return -ENODEV;
	}
	for (i = 0; i < MAX_CTLR; i++) {
		if (cch_devs[i].chrmajor == chrmajor ) {
			found = 1;
			break;
		}
	}
	if (!found)
	{
		printk(KERN_WARNING "hpsa: No matching char dev found"
			" for major number %d\n", chrmajor);
		return -ENODEV;
	}
	if (cch_devs[i].h == NULL )
	{
		printk(KERN_WARNING "hpsa: Failed lookup of character device "
				"for major number %d\n", chrmajor);	
		return -ENODEV;
	}
	p_dev->host = cch_devs[i].h->scsi_host;
	return hpsa_ioctl(p_dev, cmd, (void *)arg);
}

#ifdef CONFIG_COMPAT
static long hpsa_char_compat_ioctl(struct file *f,
                   unsigned cmd, unsigned long arg)
{
        int i;
        int found = 0;
        int chrmajor = 0;
        struct scsi_device dev;
        struct scsi_device *p_dev = &dev;
	struct inode *inode = f->f_dentry->d_inode;


        chrmajor = MAJOR(inode->i_rdev);
        if (!chrmajor) {
                printk(KERN_WARNING "hpsa: Invalid Char Device major: %d\n",
                        MAJOR(inode->i_rdev));
                return -ENODEV;
        }
	for (i = 0; i < MAX_CTLR; i++) {
		if (cch_devs[i].chrmajor == chrmajor ) {
			found = 1;
			break;
		}
	}
        if (!found)
        {                 printk(KERN_WARNING "hpsa: No matching char dev found"
				" for major number %d\n", chrmajor);
                return -ENODEV;
        }
	if (cch_devs[i].h == NULL )
	{
		printk(KERN_WARNING "hpsa: Failed lookup of character device "
				"for major number %d\n", chrmajor);	
		return -ENODEV;
	}
	p_dev->host = cch_devs[i].h->scsi_host;
        return hpsa_compat_ioctl(p_dev, cmd, (void *)arg);
}
#endif

/* hpsa_register_controller
 *      Register a character device to represent the controller
 *	since ESX 4.x does not support IOCTLs to block devices.
 *	Returns 1 on success, 0 on failure.
 */
static int hpsa_register_controller(struct ctlr_info *h) 
{
	int retval;

        cch_devs[h->ctlr].chrmajor = register_chrdev(0, h->devname, &hpsa_char_fops); 
        if (cch_devs[h->ctlr].chrmajor < 0) {
                printk(KERN_WARNING 
                        "hpsa%d: failed to register char device %s\n",
                         h->ctlr, h->devname);
                retval = 0;
        }
        else {
		hpsa_dbmsg(h, 1, "Created character device %s, major %d\n",
                        h->devname, cch_devs[h->ctlr].chrmajor);
		retval = 1;
	}
	return retval;
}

static int
is_keyword(char *ptr, int len, char *verb)  /* Thanks to ncr53c8xx.c */
{
	int verb_len = strlen(verb);
	if (len >= verb_len && !memcmp(verb,ptr,verb_len))
		return verb_len;
	else
		return 0;
}

static int
hpsa_scsi_user_command(struct ctlr_info *h, char *buffer, int length)
{
	int arg_len;

	if ((arg_len = is_keyword(buffer, length, "rescan")) != 0)
		hpsa_update_scsi_devices(h, h->scsi_host->host_no);
	else
		return -EINVAL;
	return length;
}


static int
hpsa_scsi_proc_info(struct Scsi_Host *sh,
		char *buffer,	   /* data buffer */
		char **start, 	   /* where data in buffer starts */
		off_t offset,	   /* offset from start of imaginary file */
		int length, 	   /* length of data in buffer */
		int func)	   /* 0 == read, 1 == write */
{

	int buflen, datalen;
	struct ctlr_info *h;
        int i;
	struct hpsa_scsi_dev_t *drv;

	h = (struct ctlr_info *) sh->hostdata[0];
	if (h == NULL)  /* This really shouldn't ever happen. */
		return -EINVAL;

	buflen = sprintf(buffer, "%s\n", DRIVER_NAME);
	if (func == 0) {	/* User is reading from /proc/scsi/hpsa*?/?*  */
		buflen += sprintf(&buffer[buflen], 
			"hpsa%d: SCSI host: %d nr_cmds: %d\n",
			h->ctlr, sh->host_no, h->nr_cmds); 
		/* This is used by apps to link hpsa device to
		 * scsi host number without querying a target.
		 */

		/* VMware has 1-page limit. Avoid exceeding it.
		 * when there are lots of attached LUNs 
		 */ 
		for (i = 0; i < h->ndevices; i++) {
			drv = h->dev[i];
			if (drv == NULL )
				continue;
			/* Show only disk and enclosures */
			if (drv->devtype != TYPE_DISK &&
				drv->devtype != TYPE_ENCLOSURE)
				continue;
#if defined(__VMKLNX__)
                	/* Reserve 80 for vmkadapter name */
			int len = length - buflen - 80;
			if (len <= 0) { 
				break;
                        }
			buflen += snprintf(&buffer[buflen], len,
				"c%db%dt%dl%d %02d "
#else
			buflen += sprintf(&buffer[buflen], 
				"c%db%dt%dl%d %02d "
#endif
				"0x%02x%02x%02x%02x%02x%02x%02x%02x\n",
				sh->host_no, drv->bus, drv->target, drv->lun,
				drv->devtype,
				drv->scsi3addr[0], drv->scsi3addr[1],
				drv->scsi3addr[2], drv->scsi3addr[3],
				drv->scsi3addr[4], drv->scsi3addr[5],
				drv->scsi3addr[6], drv->scsi3addr[7]);
		}
		buflen = min(buflen, length - 80);
		datalen = buflen - offset;
		if (datalen < 0) { 	/* read past EOF. */
			datalen = 0;
			*start = buffer+buflen;	
		} else
			*start = buffer + offset;
		return(datalen);
	} else 	/* writing to /proc/scsi/hpsa*?/?*  ... */
		return hpsa_scsi_user_command(h, buffer, length);	
} 

