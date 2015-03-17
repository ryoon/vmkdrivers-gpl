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
 *    Portions Copyright 2008-2012 VMware, Inc.
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/timer.h>
#include <linux/seq_file.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/smp_lock.h>
#include <linux/compat.h>
#include <linux/blktrace_api.h>
#if defined (__VMKLNX__)
#include <asm/uaccess.h>
#else //NOT  defined (__VMKLNX__)
#include <linux/uaccess.h>
#endif 
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/completion.h>
#include <linux/moduleparam.h>
#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_tcq.h>
#include <linux/cciss_ioctl.h>
#include <linux/string.h>
#include <linux/bitmap.h>
#include <asm/atomic.h>
#include <linux/kthread.h>
#include "hpsa_cmd.h"
#include "hpsa.h"

#include "hpsa_docs.h"
#if defined(__VMKLNX__)
#include "hpsavm.h"
#endif //if defined(__VMKLNX__)

/* How long to wait (in milliseconds) for board to go into simple mode */
#define MAX_CONFIG_WAIT 30000
#define MAX_IOCTL_CONFIG_WAIT 1000

/*define how many times we will try a command because of bus resets */
#define MAX_CMD_RETRIES 3

static int hpsa_allow_any;
module_param(hpsa_allow_any, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(hpsa_allow_any,
		"Allow hpsa driver to access unknown HP Smart Array hardware");

#include "hpsa_boards.h"
static int number_of_controllers;
#if (defined(HPSA_ESX4_0) || defined(HPSA_ESX4_1))
static irqreturn_t do_hpsa_intr(int irq, void *dev_id, struct pt_regs * regs);
#else
static irqreturn_t do_hpsa_intr(int irq, void *dev_id);
#endif
static int hpsa_ioctl(struct scsi_device *dev, int cmd, void *arg);
static void start_io(struct ctlr_info *h);

#ifdef CONFIG_COMPAT
static int hpsa_compat_ioctl(struct scsi_device *dev, int cmd, void *arg);
#endif

static void cmd_free(struct ctlr_info *h, struct CommandList *c);
static void cmd_special_free(struct ctlr_info *h, struct CommandList *c);
static struct CommandList *cmd_alloc(struct ctlr_info *h);
static struct CommandList *cmd_special_alloc(struct ctlr_info *h);
static void fill_cmd(struct CommandList *c, u8 cmd, struct ctlr_info *h,
	void *buff, size_t size, u8 page_code, unsigned char *scsi3addr,
	int cmd_type);

static int hpsa_scsi_queue_command(struct scsi_cmnd *cmd,
		void (*done)(struct scsi_cmnd *));
static void hpsa_scan_start(struct Scsi_Host *);
static int hpsa_scan_finished(struct Scsi_Host *sh,
	unsigned long elapsed_time);
#if defined (__VMKLNX__)
static int hpsa_change_queue_depth(struct scsi_device *sdev,
	int qdepth);

#else /* NOT defined (__VMKLNX__) */
static int hpsa_change_queue_depth(struct scsi_device *sdev,
	int qdepth, int reason);
#endif

static int hpsa_eh_device_reset_handler(struct scsi_cmnd *scsicmd);
static int hpsa_eh_bus_reset_handler(struct scsi_cmnd *scsicmd);
static int hpsa_eh_abort_handler(struct scsi_cmnd *scsicmd);

static int hpsa_slave_alloc(struct scsi_device *sdev);
static void hpsa_slave_destroy(struct scsi_device *sdev);

static ssize_t raid_level_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t lunid_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t unique_id_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static void hpsa_update_scsi_devices(struct ctlr_info *h, int hostno);
#if defined (__VMKLNX__)
static ssize_t host_store_rescan(struct class_device *dev,
	 const char *buf, size_t count);
#else /* NOT defined (__VMKLNX__) */
static ssize_t host_store_rescan(struct device *dev,
	 struct device_attribute *attr, const char *buf, size_t count);
#endif
static int check_for_unit_attention(struct ctlr_info *h,
	struct CommandList *c);
static void check_ioctl_unit_attention(struct ctlr_info *h,
	struct CommandList *c);
/* performant mode helper functions */
static void calc_bucket_map(int *bucket, int num_buckets,
	int nsgs, int *bucket_map);
static void hpsa_put_ctlr_into_performant_mode(struct ctlr_info *h);
static inline u32 next_command(struct ctlr_info *h);
static int hpsa_vpd_lv_status_supported(struct ctlr_info *h,
	unsigned char scsi3addr[]);
//static void hpsa_show_volume_status(struct ctlr_info *h,
//	unsigned char scsi3addr[], int status);
static void hpsa_show_volume_status(struct ctlr_info *h,
	struct hpsa_scsi_dev_t *sd);
static int hpsa_get_volume_status(struct ctlr_info *h,
	unsigned char scsi3addr[]);
static unsigned char hpsa_volume_offline(struct ctlr_info *h,
	unsigned char scsi3addr[]);
static DEVICE_ATTR(raid_level, S_IRUGO, raid_level_show, NULL);
static DEVICE_ATTR(lunid, S_IRUGO, lunid_show, NULL);
static DEVICE_ATTR(unique_id, S_IRUGO, unique_id_show, NULL);
#if defined (__VMKLNX__)
static CLASS_DEVICE_ATTR(rescan, S_IWUSR, NULL, host_store_rescan);
#else /* NOT defined (__VMKLNX__) */
static DEVICE_ATTR(rescan, S_IWUSR, NULL, host_store_rescan);
#endif

static struct device_attribute *hpsa_sdev_attrs[] = {
	&dev_attr_raid_level,
	&dev_attr_lunid,
	&dev_attr_unique_id,
	NULL,
};

#if defined (__VMKLNX__)
static struct class_device_attribute *hpsa_shost_attrs[] = {
	&class_device_attr_rescan,	 
	NULL,
};
#else /* NOT defined (__VMKLNX__) */
static struct device_attribute *hpsa_shost_attrs[] = {
	&dev_attr_rescan,
	NULL,
};
#endif

static struct scsi_host_template hpsa_driver_template = {
	.module			= THIS_MODULE,
	.name			= "hpsa",
	.proc_name		= "hpsa",
	.proc_info		= hpsa_scsi_proc_info,
	.queuecommand		= hpsa_scsi_queue_command,
	.scan_start		= hpsa_scan_start,
	.scan_finished		= hpsa_scan_finished,
	.change_queue_depth	= hpsa_change_queue_depth,
	.this_id		= -1,
	.use_clustering		= ENABLE_CLUSTERING,
	.eh_device_reset_handler = hpsa_eh_device_reset_handler,
	.eh_bus_reset_handler	= hpsa_eh_bus_reset_handler,
	.eh_abort_handler	= hpsa_eh_abort_handler,
	.ioctl			= hpsa_ioctl,
	.slave_alloc		= hpsa_slave_alloc,
	.slave_destroy		= hpsa_slave_destroy,
#ifdef CONFIG_COMPAT
	.compat_ioctl		= hpsa_compat_ioctl,
#endif
	.sdev_attrs = hpsa_sdev_attrs,
	.shost_attrs = hpsa_shost_attrs,
};

static inline struct ctlr_info *sdev_to_hba(struct scsi_device *sdev)
{
	unsigned long *priv = shost_priv(sdev->host);
	return (struct ctlr_info *) *priv;
}

static inline struct ctlr_info *shost_to_hba(struct Scsi_Host *sh)
{
	unsigned long *priv = shost_priv(sh);
	return (struct ctlr_info *) *priv;
}

static int check_for_unit_attention(struct ctlr_info *h,
	struct CommandList *c)
{
	if (c->err_info->SenseInfo[2] != UNIT_ATTENTION)
		return 0;

	switch (c->err_info->SenseInfo[12]) {
	case STATE_CHANGED:
		dev_warn(&h->pdev->dev, "hpsa%d: a state change "
			"detected, command retried\n", h->ctlr);
		break;
	case LUN_FAILED:
		dev_warn(&h->pdev->dev, "hpsa%d: LUN failure "
			"detected, action required\n", h->ctlr);
		break;
	case REPORT_LUNS_CHANGED:
		dev_warn(&h->pdev->dev, "hpsa%d: report LUN data "
			"changed, action required\n", h->ctlr);
	/*
	 * Note: this REPORT_LUNS_CHANGED condition only occurs on external
	 * target devices.
	 */
		break;
	case POWER_OR_RESET:
		dev_warn(&h->pdev->dev, "hpsa%d: a power on "
			"or device reset detected\n", h->ctlr);
		break;
	case UNIT_ATTENTION_CLEARED:
		dev_warn(&h->pdev->dev, "hpsa%d: unit attention "
		    "cleared by another initiator\n", h->ctlr);
		break;
	default:
		dev_warn(&h->pdev->dev, "hpsa%d: unknown "
			"unit attention detected\n", h->ctlr);
		break;
	}
	return 1;
}


#if defined (__VMKLNX__)
static ssize_t host_store_rescan(struct class_device *dev,
				 const char *buf, size_t count)
#else /* NOT defined (__VMKLNX__) */
static ssize_t host_store_rescan(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
#endif
{
	struct ctlr_info *h;
	struct Scsi_Host *shost = class_to_shost(dev);
	h = shost_to_hba(shost);
	hpsa_scan_start(h->scsi_host);
	return count;
}

/* Enqueuing and dequeuing functions for cmdlists. */
static inline void addQ(struct list_head *list, struct CommandList *c)
{
	list_add_tail(&c->list, list);
}

static inline u32 next_command(struct ctlr_info *h)
{
	u32 a;

	if (unlikely(!(h->transMethod & CFGTBL_Trans_Performant)))
		return h->access.command_completed(h);

	if ((*(h->reply_pool_head) & 1) == (h->reply_pool_wraparound)) {
		a = *(h->reply_pool_head); /* Next cmd in ring buffer */
		(h->reply_pool_head)++;
		h->commands_outstanding--;
	} else {
		a = FIFO_EMPTY;
	}
	/* Check for wraparound */
	if (h->reply_pool_head == (h->reply_pool + h->max_commands)) {
		h->reply_pool_head = h->reply_pool;
		h->reply_pool_wraparound ^= 1;
	}
	return a;
}

/* set_performant_mode: Modify the tag for cciss performant
 * set bit 0 for pull model, bits 3-1 for block fetch
 * register number
 */
static void set_performant_mode(struct ctlr_info *h, struct CommandList *c)
{
	if (likely(h->transMethod & CFGTBL_Trans_Performant))
		c->busaddr |= 1 | (h->blockFetchTable[c->Header.SGList] << 1);
}

static void enqueue_cmd_and_start_io(struct ctlr_info *h,
	struct CommandList *c)
{
	unsigned long flags;

	set_performant_mode(h, c);
	spin_lock_irqsave(&h->lock, flags);
	addQ(&h->reqQ, c);
	h->Qdepth++;
	start_io(h);
	spin_unlock_irqrestore(&h->lock, flags);
}

static inline void removeQ(struct CommandList *c)
{
#if defined(__VMKLNX__)
	do {
		if ( unlikely( list_empty(&c->list) != 0)) {
			printk("BUG: warning at %s:%d/%s() (%s)\n", __FILE__, __LINE__, __FUNCTION__, print_tainted());
			dump_stack();
		}
	} while(0);
#else /* NOT defined(__VMKLNX__) */
	if (WARN_ON(list_empty(&c->list)))
		return;
#endif
	list_del_init(&c->list);
}

static inline int is_hba_lunid(unsigned char scsi3addr[])
{
	return memcmp(scsi3addr, RAID_CTLR_LUNID, 8) == 0;
}

static inline int is_logical_dev_addr_mode(unsigned char scsi3addr[])
{
	return (scsi3addr[3] & 0xC0) == 0x40;
}

static inline int is_scsi_rev_5(struct ctlr_info *h)
{
	if (!h->hba_inquiry_data)
		return 0;
	if ((h->hba_inquiry_data[2] & 0x07) == 5)
		return 1;
	return 0;
}

static const char *raid_label[] = { "0", "4", "1(1+0)", "5", "5+1", "ADG",
        "1(ADM)", "UNKNOWN"
};
#define RAID_UNKNOWN (ARRAY_SIZE(raid_label) - 1)

static ssize_t raid_level_show(struct device *dev,
	     struct device_attribute *attr, char *buf)
{
	ssize_t l = 0;
	unsigned char rlevel;
	struct ctlr_info *h;
	struct scsi_device *sdev;
	struct hpsa_scsi_dev_t *hdev;
	unsigned long flags;

	sdev = to_scsi_device(dev);
	h = sdev_to_hba(sdev);
	spin_lock_irqsave(&h->lock, flags);
	hdev = sdev->hostdata;
	if (!hdev) {
		spin_unlock_irqrestore(&h->lock, flags);
		return -ENODEV;
	}

	/* Is this even a logical drive? */
	if (!is_logical_dev_addr_mode(hdev->scsi3addr)) {
		spin_unlock_irqrestore(&h->lock, flags);
		l = snprintf(buf, PAGE_SIZE, "N/A\n");
		return l;
	}

	rlevel = hdev->raid_level;
	spin_unlock_irqrestore(&h->lock, flags);
	if (rlevel > RAID_UNKNOWN)
		rlevel = RAID_UNKNOWN;
	l = snprintf(buf, PAGE_SIZE, "RAID %s\n", raid_label[rlevel]);
	return l;
}

static ssize_t lunid_show(struct device *dev,
	     struct device_attribute *attr, char *buf)
{
	struct ctlr_info *h;
	struct scsi_device *sdev;
	struct hpsa_scsi_dev_t *hdev;
	unsigned long flags;
	unsigned char lunid[8];

	sdev = to_scsi_device(dev);
	h = sdev_to_hba(sdev);
	spin_lock_irqsave(&h->lock, flags);
	hdev = sdev->hostdata;
	if (!hdev) {
		spin_unlock_irqrestore(&h->lock, flags);
		return -ENODEV;
	}
	memcpy(lunid, hdev->scsi3addr, sizeof(lunid));
	spin_unlock_irqrestore(&h->lock, flags);
	return snprintf(buf, 20, "0x%02x%02x%02x%02x%02x%02x%02x%02x\n",
		lunid[0], lunid[1], lunid[2], lunid[3],
		lunid[4], lunid[5], lunid[6], lunid[7]);
}

static ssize_t unique_id_show(struct device *dev,
	     struct device_attribute *attr, char *buf)
{
	struct ctlr_info *h;
	struct scsi_device *sdev;
	struct hpsa_scsi_dev_t *hdev;
	unsigned long flags;
	unsigned char sn[16];

	sdev = to_scsi_device(dev);
	h = sdev_to_hba(sdev);
	spin_lock_irqsave(&h->lock, flags);
	hdev = sdev->hostdata;
	if (!hdev) {
		spin_unlock_irqrestore(&h->lock, flags);
		return -ENODEV;
	}
	memcpy(sn, hdev->device_id, sizeof(sn));
	spin_unlock_irqrestore(&h->lock, flags);
	return snprintf(buf, 16 * 2 + 2,
			"%02X%02X%02X%02X%02X%02X%02X%02X"
			"%02X%02X%02X%02X%02X%02X%02X%02X\n",
			sn[0], sn[1], sn[2], sn[3],
			sn[4], sn[5], sn[6], sn[7],
			sn[8], sn[9], sn[10], sn[11],
			sn[12], sn[13], sn[14], sn[15]);
}

static int hpsa_find_target_lun(struct ctlr_info *h,
	unsigned char scsi3addr[], int bus, int *target, int *lun)
{
	/* finds an unused bus, target, lun for a new physical device
	 * assumes h->devlock is held
	 */
	int i, found = 0;
	DECLARE_BITMAP(lun_taken, HPSA_MAX_DEVICES);

	memset(&lun_taken[0], 0, HPSA_MAX_DEVICES >> 3);

	for (i = 0; i < h->ndevices; i++) {
		if (h->dev[i]->bus == bus && h->dev[i]->target != -1)
			set_bit(h->dev[i]->target, lun_taken);
	}

	for (i = 0; i < HPSA_MAX_DEVICES; i++) {
		if (!test_bit(i, lun_taken)) {
			/* *bus = 1; */
			*target = i;
			*lun = 0;
			found = 1;
			break;
		}
	}
	return !found;
}

/* Add an entry into h->dev[] array. */
static int hpsa_scsi_add_entry(struct ctlr_info *h, int hostno,
		struct hpsa_scsi_dev_t *device,
		struct hpsa_scsi_dev_t *added[], int *nadded)
{
	/* assumes h->devlock is held */
	int n = h->ndevices;
	int i;
	unsigned char addr1[8], addr2[8];
	struct hpsa_scsi_dev_t *sd;

	hpsa_dbmsg(h, 3, "hpsa_scsi_add_entry: begin. \n");

	if (n >= HPSA_MAX_DEVICES) {
		dev_err(&h->pdev->dev, "too many devices, some will be "
			"inaccessible.\n");
		return -1;
	}

	/* physical devices do not have lun or target assigned until now. */
	if (device->lun != -1)
		/* Logical device, lun is already assigned. */
		goto lun_assigned;

	/* If this device a non-zero lun of a multi-lun device
	 * byte 4 of the 8-byte LUN addr will contain the logical
	 * unit no, zero otherise.
	 */
	if (device->scsi3addr[4] == 0) {
		hpsa_dbmsg(h, 3, "hpsa_scsi_add_entry: "
		"B%d:T%d:L%d is not a non-zero LUN of a multi-lun device.\n",
		device->bus, device->target, device->lun);
		/* This is not a non-zero lun of a multi-lun device */
		if (hpsa_find_target_lun(h, device->scsi3addr,
			device->bus, &device->target, &device->lun) != 0)
			return -1;
		goto lun_assigned;
	}

	/* This is a non-zero lun of a multi-lun device.
	 * Search through our list and find the device which
	 * has the same 8 byte LUN address, excepting byte 4.
	 * Assign the same bus and target for this new LUN.
	 * Use the logical unit number from the firmware.
	 */
	hpsa_dbmsg(h, 3, "hpsa_scsi_add_entry: "
		"B%d:T%d:L%d is a non-zero LUN of a multi-lun device.\n",
		device->bus, device->target, device->lun);
	memcpy(addr1, device->scsi3addr, 8);
	addr1[4] = 0;
	for (i = 0; i < n; i++) {
		sd = h->dev[i];
		memcpy(addr2, sd->scsi3addr, 8);
		addr2[4] = 0;
		/* differ only in byte 4? */
		if (memcmp(addr1, addr2, 8) == 0) {
			device->bus = sd->bus;
			device->target = sd->target;
			device->lun = device->scsi3addr[4];
			break;
		}
	}
	if (device->lun == -1) {
		dev_warn(&h->pdev->dev, "physical device with no LUN=0,"
			" suspect firmware bug or unsupported hardware "
			"configuration.\n");
			return -1;
	}

lun_assigned:

	h->dev[n] = device;
	h->ndevices++;
	added[*nadded] = device;
	(*nadded)++;

	/* initially, (before registering with scsi layer) we don't
	 * know our hostno and we don't want to print anything first
	 * time anyway (the scsi layer's inquiries will show that info)
	 */
	/* if (hostno != -1) */
	hpsa_dbmsg(h, 3, "hpsa_scsi_add_entry: "
		"added %s C%d:B%d:T%d:L%d\n",
		scsi_device_type(device->devtype), hostno,
		device->bus, device->target, device->lun);

		dev_info(&h->pdev->dev, "%s device c%db%dt%dl%d added.\n",
			scsi_device_type(device->devtype), hostno,
			device->bus, device->target, device->lun);
	return 0;
}

/* Update an entry in h->dev[] array. */
static void hpsa_scsi_update_entry(struct ctlr_info *h, int hostno,
	int entry, struct hpsa_scsi_dev_t *new_entry)
{
	/* assumes h->devlock is held */
	BUG_ON(entry < 0 || entry >= HPSA_MAX_DEVICES);

	/* At least one of these attributes changed. update all. */
	h->dev[entry]->raid_level = new_entry->raid_level;
	memcpy(h->dev[entry]->revision, new_entry->revision, 
		sizeof(new_entry->revision));
	dev_info(&h->pdev->dev, "%s device c%db%dt%dl%d updated.\n",
		scsi_device_type(new_entry->devtype), hostno, new_entry->bus,
		new_entry->target, new_entry->lun);
}

/* Replace an entry from h->dev[] array. */
static void hpsa_scsi_replace_entry(struct ctlr_info *h, int hostno,
	int entry, struct hpsa_scsi_dev_t *new_entry,
	struct hpsa_scsi_dev_t *added[], int *nadded,
	struct hpsa_scsi_dev_t *removed[], int *nremoved)
{
	/* assumes h->devlock is held */
	BUG_ON(entry < 0 || entry >= HPSA_MAX_DEVICES);
	removed[*nremoved] = h->dev[entry];
	(*nremoved)++;
	h->dev[entry] = new_entry;
	added[*nadded] = new_entry;
	(*nadded)++;
	dev_info(&h->pdev->dev, "%s device c%db%dt%dl%d changed.\n",
		scsi_device_type(new_entry->devtype), hostno, new_entry->bus,
			new_entry->target, new_entry->lun);
}

/* Remove an entry from h->dev[] array. */
static void hpsa_scsi_remove_entry(struct ctlr_info *h, int hostno, int entry,
	struct hpsa_scsi_dev_t *removed[], int *nremoved)
{
	/* assumes h->devlock is held */
	int i;
	struct hpsa_scsi_dev_t *sd;

	BUG_ON(entry < 0 || entry >= HPSA_MAX_DEVICES);

	sd = h->dev[entry];
	removed[*nremoved] = h->dev[entry];
	(*nremoved)++;

	for (i = entry; i < h->ndevices-1; i++)
		h->dev[i] = h->dev[i+1];
	h->ndevices--;
	dev_info(&h->pdev->dev, "%s device c%db%dt%dl%d removed.\n",
		scsi_device_type(sd->devtype), hostno, sd->bus, sd->target,
		sd->lun);
}

#define SCSI3ADDR_EQ(a, b) ( \
	(a)[7] == (b)[7] && \
	(a)[6] == (b)[6] && \
	(a)[5] == (b)[5] && \
	(a)[4] == (b)[4] && \
	(a)[3] == (b)[3] && \
	(a)[2] == (b)[2] && \
	(a)[1] == (b)[1] && \
	(a)[0] == (b)[0])

static void fixup_botched_add(struct ctlr_info *h,
	struct hpsa_scsi_dev_t *added)
{
	/* called when scsi_add_device fails in order to re-adjust
	 * h->dev[] to match the mid layer's view.
	 */
	unsigned long flags;
	int i, j;

	spin_lock_irqsave(&h->lock, flags);
	for (i = 0; i < h->ndevices; i++) {
		if (h->dev[i] == added) {
			hpsa_db_dev("fixup_botched_add: Add failed, Removing", h, added, -1);
			for (j = i; j < h->ndevices-1; j++)
				h->dev[j] = h->dev[j+1];
			h->ndevices--;
			break;
		}
	}
	spin_unlock_irqrestore(&h->lock, flags);
	kfree(added);
}

static inline int device_is_the_same(struct hpsa_scsi_dev_t *dev1,
	struct hpsa_scsi_dev_t *dev2)
{
	/* we compare everything except lun and target as these
	 * are not yet assigned.  Compare parts likely
	 * to differ first
	 */
	if (memcmp(dev1->scsi3addr, dev2->scsi3addr,
		sizeof(dev1->scsi3addr)) != 0)
		return 0;
	if (memcmp(dev1->device_id, dev2->device_id,
		sizeof(dev1->device_id)) != 0)
		return 0;
	if (memcmp(dev1->model, dev2->model, sizeof(dev1->model)) != 0)
		return 0;
	if (memcmp(dev1->vendor, dev2->vendor, sizeof(dev1->vendor)) != 0)
		return 0;
	if (dev1->devtype != dev2->devtype)
		return 0;
	if (dev1->bus != dev2->bus)
		return 0;
	return 1;
}

static inline int device_updated(struct hpsa_scsi_dev_t *dev1,
	struct hpsa_scsi_dev_t *dev2)
{
	/* Device attributes that can change */
	if (memcmp(dev1->revision, dev2->revision, sizeof(dev1->revision)) != 0)
		return 1; 
        if (dev1->raid_level != dev2->raid_level)
		return 1;
	return 0;
}

/* Find needle in haystack.  If exact match found, return DEVICE_SAME,
 * and return needle location in *index.  If scsi3addr matches, but not
 * vendor, model, serial num, etc. return DEVICE_CHANGED, and return needle
 * location in *index.  
 * In the case of a minor device attribute change, such as RAID level, just
 * return DEVICE_UPDATED, along with the updated device's location in index.
 * If needle not found, return DEVICE_NOT_FOUND.
 */
static int hpsa_scsi_find_entry(struct hpsa_scsi_dev_t *needle,
	struct hpsa_scsi_dev_t *haystack[], int haystack_size,
	int *index)
{
	int i;
#define DEVICE_NOT_FOUND 0
#define DEVICE_CHANGED 1
#define DEVICE_SAME 2
#define DEVICE_UPDATED 3
	for (i = 0; i < haystack_size; i++) {
		if (haystack[i] == NULL) /* previously removed. */
			continue;
		if (SCSI3ADDR_EQ(needle->scsi3addr, haystack[i]->scsi3addr)) {
			*index = i;
			if (device_is_the_same(needle, haystack[i])) {
				if (device_updated(needle, haystack[i]))  
					return DEVICE_UPDATED;
				return DEVICE_SAME;
			}
			else
				return DEVICE_CHANGED;
		}
	}
	*index = -1;
	return DEVICE_NOT_FOUND;
}

#define OFFLINE_DEVICE_POLL_INTERVAL (120 * HZ)
static int hpsa_offline_device_thread(void *v)
{
	struct ctlr_info *h = v;
	unsigned long flags;
	struct offline_device_entry *d;
	unsigned char need_rescan = 0;
	struct list_head *this, *tmp;

	while (1) {
		schedule_timeout_interruptible(OFFLINE_DEVICE_POLL_INTERVAL);
		if (kthread_should_stop())
			break;

		/*Check if any of the offline devices have become ready */
		spin_lock_irqsave(&h->offline_device_lock, flags);
		list_for_each_safe(this, tmp, &h->offline_device_list) {
			d = list_entry(this, struct offline_device_entry,
				offline_list);
			spin_unlock_irqrestore(&h->offline_device_lock, flags);
			if (!hpsa_volume_offline(h, d->scsi3addr)) {
				need_rescan = 1;
				goto do_rescan;
			}
			spin_lock_irqsave(&h->offline_device_lock, flags);
		}
		spin_unlock_irqrestore(&h->offline_device_lock, flags);
	}

do_rescan:
	/* Remove all entries from the list and rescan and exit this thread.
	 * If there are still offline devices, the rescan will make a new list
	 * and create a new offline device monitor thread.
	 */
	spin_lock_irqsave(&h->offline_device_lock, flags);
	list_for_each_safe(this, tmp, &h->offline_device_list) {
		d = list_entry(this, struct offline_device_entry, offline_list);
		list_del_init(this);
		kfree(d);
	}
	h->offline_device_monitor = NULL;
	h->offline_device_thread_state = OFFLINE_DEVICE_THREAD_STOPPED;
	spin_unlock_irqrestore(&h->offline_device_lock, flags);
	if (need_rescan)
		hpsa_scan_start(h->scsi_host);
	return 0;
}

static void hpsa_monitor_offline_device(struct ctlr_info *h,
		unsigned char scsi3addr[])
{
	struct offline_device_entry *device;
	unsigned long flags;

        /*Check to see if device is already on the list */
	spin_lock_irqsave(&h->offline_device_lock, flags);
	list_for_each_entry(device, &h->offline_device_list, offline_list) {
		if (memcmp(device->scsi3addr, scsi3addr,
			sizeof(device->scsi3addr)) == 0 ) {
			spin_unlock_irqrestore(&h->offline_device_lock, flags);
			return;
		}
	}
	spin_unlock_irqrestore(&h->offline_device_lock, flags);

	/* Device is not on the list, add it. */
	device = kmalloc(sizeof(*device) , GFP_KERNEL);
	if (!device) {
		dev_warn(&h->pdev->dev, "out of memory in %s\n", __func__);
		return;
	}
	memcpy(device->scsi3addr, scsi3addr, sizeof(device->scsi3addr));
	spin_lock_irqsave(&h->offline_device_lock, flags);
	list_add_tail(&device->offline_list, &h->offline_device_list);
	if (h->offline_device_thread_state == OFFLINE_DEVICE_THREAD_STOPPED) {
		h->offline_device_thread_state = OFFLINE_DEVICE_THREAD_RUNNING;
		spin_unlock_irqrestore(&h->offline_device_lock, flags);
		h->offline_device_monitor =
			kthread_run(hpsa_offline_device_thread, h, HPSA "-odm");
		spin_lock_irqsave(&h->offline_device_lock, flags);
	}
	if (!h->offline_device_monitor) {
		dev_warn(&h->pdev->dev, "failed to start offline device monitor thread.\n");
		h->offline_device_thread_state = OFFLINE_DEVICE_THREAD_STOPPED;
	}
	spin_unlock_irqrestore(&h->offline_device_lock, flags);
}

static void stop_offline_device_monitor(struct ctlr_info *h)
{
	unsigned long flags;
	int stop_thread;

	spin_lock_irqsave(&h->offline_device_lock, flags);
	stop_thread = (h->offline_device_thread_state ==
		OFFLINE_DEVICE_THREAD_RUNNING);
	if (stop_thread)
		h->offline_device_thread_state =
			OFFLINE_DEVICE_THREAD_STOPPING; /* blocks new starts */
	spin_unlock_irqrestore(&h->offline_device_lock, flags);
	if (stop_thread)
		kthread_stop(h->offline_device_monitor);
}

static void adjust_hpsa_scsi_table(struct ctlr_info *h, int hostno,
	struct hpsa_scsi_dev_t *sd[], int nsds)
{
	/* sd contains scsi3 addresses and devtypes, and inquiry
	 * data.  This function takes what's in sd to be the current
	 * reality and updates h->dev[] to reflect that reality.
	 */
	int i, entry, device_change, changes = 0;
	hpsa_dbmsg(h, 3, "adjust_hpsa_scsi_table: begin\n");
	struct hpsa_scsi_dev_t *csd;
	unsigned long flags;
	struct hpsa_scsi_dev_t **added, **removed;
	int nadded, nremoved;
	struct Scsi_Host *sh = NULL;

	added = kzalloc(sizeof(*added) * HPSA_MAX_DEVICES,
		GFP_KERNEL);
	removed = kzalloc(sizeof(*removed) * HPSA_MAX_DEVICES,
		GFP_KERNEL);

	if (!added || !removed) {
		dev_warn(&h->pdev->dev, "out of memory in "
			"adjust_hpsa_scsi_table\n");
		goto free_and_out;
	}

	spin_lock_irqsave(&h->devlock, flags);

	/* find any devices in h->dev[] that are not in
	 * sd[] and remove them from h->dev[], and for any
	 * devices which have changed, remove the old device
	 * info and add the new device info.
	 * If minor device attributes change, just update
	 * the existing device structure.
	 */
	i = 0;
	nremoved = 0;
	nadded = 0;
	while (i < h->ndevices) {
		csd = h->dev[i];
		device_change = hpsa_scsi_find_entry(csd, sd, nsds, &entry);
		if (device_change == DEVICE_NOT_FOUND) {
			changes++;
			hpsa_db_dev("adjust_hpsa_scsi_table: Remove", h, h->dev[i], i);
			hpsa_scsi_remove_entry(h, hostno, i,
				removed, &nremoved);
			continue; /* remove ^^^, hence i not incremented */
		} else if (device_change == DEVICE_CHANGED) {
			changes++;
			hpsa_db_dev("adjust_hpsa_scsi_table: Replace", h, h->dev[i], i);

			hpsa_scsi_replace_entry(h, hostno, i, sd[entry],
				added, &nadded, removed, &nremoved);
			/* Set it to NULL to prevent it from being freed
			 * at the bottom of hpsa_update_scsi_devices()
			 */
			sd[entry] = NULL;
		} else if (device_change == DEVICE_UPDATED) {
			hpsa_db_dev("adjust_hpsa_scsi_table: Update", h, h->dev[i], i);
			hpsa_scsi_update_entry(h, hostno, i, sd[entry]);
		}
		i++;
	}

	/* Now, make sure every device listed in sd[] is also
	 * listed in h->dev[], adding them if they aren't found
	 */

	for (i = 0; i < nsds; i++) {
		if (!sd[i]) /* if already added above. */
			continue;

		/* Don't add devices which are in one of several NOT READY
		 * states (rapid parity init, erase, encryption actions, etc),
		 * as the SCSI mid-layer does not handle such devices well.
		 * It relentlessly loops, sending TUR at 3 Hz, then READ(10)
		 * at 160Hz, and prevents the system from coming up.
		 */
		if (sd[i]->volume_offline) {
			hpsa_show_volume_status(h, sd[i]);
			dev_info(&h->pdev->dev,
				"C%d:B%d:T%d:L%d Temporarily offline\n",
				h->scsi_host->host_no,
				sd[i]->bus, sd[i]->target, sd[i]->lun);
				continue;
		}

		device_change = hpsa_scsi_find_entry(sd[i], h->dev,
					h->ndevices, &entry);
		if (device_change == DEVICE_NOT_FOUND) {
			changes++;
			hpsa_db_dev("adjust_hpsa_scsi_table: Add", h, sd[i], i);

			if (hpsa_scsi_add_entry(h, hostno, sd[i],
				added, &nadded) != 0)
				break;
			sd[i] = NULL; /* prevent from being freed later. */
		} else if (device_change == DEVICE_CHANGED) {
			/* should never happen... */
			changes++;
			hpsa_db_dev("adjust_hpsa_scsi_table: Changed, Ignore", h, sd[i], i);
			dev_warn(&h->pdev->dev,
				"device unexpectedly changed.\n");
			/* but if it does happen, we just ignore that device */
		}
	}
	spin_unlock_irqrestore(&h->devlock, flags);


	/* Monitor devices which are in one of several NOT READY states, to be
	 * brought online later. This must be done without holding h->devlock,
	 * so don't touch h->dev[].
	 */
	for (i = 0; i < nsds; i++ ) {
		if (!sd[i]) /* if already added above */
			continue;
		if (sd[i]->volume_offline)
			hpsa_monitor_offline_device(h, sd[i]->scsi3addr);
	}
	/* Don't notify scsi mid layer of any changes the first time through
	 * (or if there are no changes) scsi_scan_host will do it later the
	 * first time through.
	 */
	if (hostno == -1 || !changes)
		goto free_and_out;

	sh = h->scsi_host;
	/* Notify scsi mid layer of any removed devices */
	for (i = 0; i < nremoved; i++) {
		hpsa_db_dev("adjust_hpsa_scsi_table: notify SCSI of remove", h, removed[i], i);

		struct scsi_device *sdev =
			scsi_device_lookup(sh, removed[i]->bus,
				removed[i]->target, removed[i]->lun);
		if (sdev != NULL) {
			hpsa_dbmsg(h, 3, "adjust_hpsa_scsi_table: "
				"REMOVING C%d:B%d:T%d:L%d\n", 
				h->scsi_host->host_no,
				sdev->channel, sdev->id, sdev->lun);
			scsi_remove_device(sdev);
			scsi_device_put(sdev);
		} else {
			/* We don't expect to get here.
			 * future cmds to this device will get selection
			 * timeout as if the device was gone.
			 */
			dev_warn(&h->pdev->dev, "didn't find c%db%dt%dl%d "
				" for removal.\n", h->scsi_host->host_no, 
				removed[i]->bus, removed[i]->target, 
				removed[i]->lun);
		}
		kfree(removed[i]);
		removed[i] = NULL;
	}

	/* Notify scsi mid layer of any added devices */
	for (i = 0; i < nadded; i++) {
		hpsa_db_dev("adjust_hpsa_scsi_table: notify SCSI of addition", h, added[i], i);

		if (scsi_add_device(sh, added[i]->bus,
			added[i]->target, added[i]->lun) == 0)
			continue;
		dev_warn(&h->pdev->dev, "scsi_add_device c%db%dt%dl%d failed, "
			"device not added.\n", h->scsi_host->host_no, 
			added[i]->bus, added[i]->target, 
			added[i]->lun);
		/* now we have to remove it from h->dev,
		 * since it didn't get added to scsi mid layer
		 */
		fixup_botched_add(h, added[i]);
	}

free_and_out:
	kfree(added);
	kfree(removed);
}

/*
 * Lookup bus/target/lun and retrun corresponding struct hpsa_scsi_dev_t *
 * Assume's h->devlock is held.
 */
static struct hpsa_scsi_dev_t *lookup_hpsa_scsi_dev(struct ctlr_info *h,
	int bus, int target, int lun)
{
	int i;
	struct hpsa_scsi_dev_t *sd;

	hpsa_dbmsg(h, 3, "lookup_hpsa_scsi_dev: B%d:T%d:L%d\n",
		bus, target, lun);
	for (i = 0; i < h->ndevices; i++) {
		sd = h->dev[i];
		if (sd->bus == bus && sd->target == target && sd->lun == lun)
			return sd;
	}
	hpsa_dbmsg(h, 3, "lookup_hpsa_scsi_dev: "
		"NOT FOUND: B%d:T%d:L%d\n",
		bus, target, lun);
	return NULL;
}

/* link sdev->hostdata to our per-device structure. */
static int hpsa_slave_alloc(struct scsi_device *sdev)
{
	struct hpsa_scsi_dev_t *sd;
	unsigned long flags;
	struct ctlr_info *h;

	h = sdev_to_hba(sdev);

	hpsa_dbmsg(h, 3,  "slave_alloc: host%d B%d:T%d:L%d \n",
		sdev->host->host_no, sdev->channel, sdev->id, sdev->lun);
	spin_lock_irqsave(&h->devlock, flags);
	sd = lookup_hpsa_scsi_dev(h, sdev_channel(sdev),
		sdev_id(sdev), sdev->lun);
	if (sd != NULL) {
		sdev->hostdata = sd;
		hpsa_dbmsg(h, 3,  "slave_alloc: "
			"linked scsi device host %d B%d:T%d:L%d "
			"to hpsa%d C%d:B%d:T%d:L%d \n",
			sdev->host->host_no, 
			sdev_channel(sdev), sdev_id(sdev), sdev->lun, 
			h->ctlr, h->scsi_host->host_no,
			((struct hpsa_scsi_dev_t *)sdev->hostdata)->bus, 
			((struct hpsa_scsi_dev_t *)sdev->hostdata)->target, 
			((struct hpsa_scsi_dev_t *)sdev->hostdata)->lun);

	}
	else {
		/* we just failed a lookup, there may be new/changed devices.*/
		hpsa_dbmsg(h, 3,  "slave_alloc: Failed lookup on "
			"hpsa%d C%d:B%d:T%d:L%d. Waking rescan thread.\n",
			h->ctlr, sdev->host->host_no, 
			sdev->channel, sdev->id, sdev->lun);
		wake_up_process(h->rescan_thread);
	}
	spin_unlock_irqrestore(&h->devlock, flags);
	return 0;

}

static void hpsa_slave_destroy(struct scsi_device *sdev)
{
	/* nothing to do. */
}

static void hpsa_scsi_setup(struct ctlr_info *h)
{
	h->ndevices = 0;
	h->scsi_host = NULL;
	spin_lock_init(&h->devlock);
}

static void hpsa_free_sg_chain_blocks(struct ctlr_info *h)
{
	int i;

	if (!h->cmd_sg_list)
		return;
	for (i = 0; i < h->nr_cmds; i++) {
		kfree(h->cmd_sg_list[i]);
		h->cmd_sg_list[i] = NULL;
	}
	kfree(h->cmd_sg_list);
	h->cmd_sg_list = NULL;
}

static int hpsa_allocate_sg_chain_blocks(struct ctlr_info *h)
{
	int i;

	if (h->chainsize <= 0)
		return 0;

	h->cmd_sg_list = kzalloc(sizeof(*h->cmd_sg_list) * h->nr_cmds,
				GFP_KERNEL);
	if (!h->cmd_sg_list)
		return -ENOMEM;
	for (i = 0; i < h->nr_cmds; i++) {
		h->cmd_sg_list[i] = kmalloc(sizeof(*h->cmd_sg_list[i]) *
						h->chainsize, GFP_KERNEL);
		if (!h->cmd_sg_list[i])
			goto clean;
	}
#ifdef HPSA_DEBUG
	hpsa_dbmsg(h, 0, "DEBUG: allocate %d bytes for cmd_sg_list array\n",
			(sizeof(*h->cmd_sg_list) * h->nr_cmds));
	hpsa_dbmsg(h, 0, "DEBUG: allocate %d bytes for cmd_sg_list chains.\n",
			(sizeof(*h->cmd_sg_list[1])* h->chainsize * h->nr_cmds));
#endif /* HPSA_DEBUG */
	return 0;

clean:
	hpsa_free_sg_chain_blocks(h);
	return -ENOMEM;
}

static void hpsa_map_sg_chain_block(struct ctlr_info *h,
	struct CommandList *c)
{
	struct SGDescriptor *chain_sg, *chain_block;
	u64 temp64;

	chain_sg = &c->SG[h->max_cmd_sg_entries - 1];
	chain_block = h->cmd_sg_list[c->cmdindex];
	chain_sg->Ext = HPSA_SG_CHAIN;
	chain_sg->Len = sizeof(*chain_sg) *
		(c->Header.SGTotal - h->max_cmd_sg_entries);
	temp64 = pci_map_single(h->pdev, chain_block, chain_sg->Len,
				PCI_DMA_TODEVICE);
	chain_sg->Addr.lower = (u32) (temp64 & 0x0FFFFFFFFULL);
	chain_sg->Addr.upper = (u32) ((temp64 >> 32) & 0x0FFFFFFFFULL);
}

#if !defined(__VMKLNX__)
static void hpsa_unmap_sg_chain_block(struct ctlr_info *h,
	struct CommandList *c)
{
	struct SGDescriptor *chain_sg;
	union u64bit temp64;

	if (c->Header.SGTotal <= h->max_cmd_sg_entries)
		return;

	chain_sg = &c->SG[h->max_cmd_sg_entries - 1];
	temp64.val32.lower = chain_sg->Addr.lower;
	temp64.val32.upper = chain_sg->Addr.upper;
	pci_unmap_single(h->pdev, temp64.val, chain_sg->Len, PCI_DMA_TODEVICE);
}
#endif /* NOT defined(__VMKLNX__) */

static void complete_scsi_command(struct CommandList *cp,
	int timeout, u32 tag)
{
	struct scsi_cmnd *cmd;
	struct ctlr_info *h;
	struct ErrorInfo *ei;

	unsigned char sense_key;
	unsigned char asc;      /* additional sense code */
	unsigned char ascq;     /* additional sense code qualifier */

	int debug=1;    	/* show detail at this level */
	char dmsg[256];		/* for dev_warn messaging */
	int dl = 0;

	char msg[256];		/* for debug messaging */
	int ml = 0;
	int orig;		/* remember original scsi status */

	ei = cp->err_info;
	cmd = (struct scsi_cmnd *) cp->scsi_cmd;
	h = cp->h;

#if !defined(__VMKLNX__)
	if ( cmd->use_sg) {
		scsi_dma_unmap(cmd); /* undo the DMA mappings */
		if (cp->Header.SGTotal > h->max_cmd_sg_entries)
			hpsa_unmap_sg_chain_block(h, cp);
	} 
	else if (cmd->request_bufflen ) {
                addr64.val32.lower = cp->SG[0].Addr.lower;
                addr64.val32.upper = cp->SG[0].Addr.upper;
                pci_unmap_single(ctlr->pdev, (dma_addr_t) addr64.val,
                        cmd->request_bufflen,
                                cmd->sc_data_direction); 

	}
#endif

	cmd->result = (DID_OK << 16); 		/* host byte */
	cmd->result |= (COMMAND_COMPLETE << 8);	/* msg byte */
	cmd->result |= ei->ScsiStatus;
	orig = cmd->result;			

	/* copy the sense data whether we need to or not. */
	memcpy(cmd->sense_buffer, ei->SenseInfo,
		ei->SenseLen > SCSI_SENSE_BUFFERSIZE ?
			SCSI_SENSE_BUFFERSIZE :
			ei->SenseLen);
	scsi_set_resid(cmd, ei->ResidualCnt);

	if (ei->CommandStatus == 0) {
		cmd->scsi_done(cmd);
		cmd_free(h, cp);
		return;
	}
	
	/* Move memset out of the completion path for performance
	 * reasons. 
	 */
	memset(dmsg, 0, sizeof(dmsg));
	memset(msg, 0, sizeof(msg));

	/* Indicate unsupported if error info is not valid. */
	//if ( (HPSA_ERROR_BIT & tag) != HPSA_ERROR_BIT ) {
	if ( ! (HPSA_ERROR_BIT & tag) ) {
		hpsa_dbmsg(h, 1, "complete_scsi_command: "
			"Error info invalid. Seting resid to 0.\n");
		cmd->resid = 0;
	}

	/* for debug messaging */ 
	ml += sprintf(msg+ml, "Device:C%d:B%d:T%u:L%u "
		"Command:0x%02x SN:0x%lx ScsiStatus:0x%x "
		"Tag:0x%08x:%08x. ", h->scsi_host->host_no, 
		cmd->device->channel, cmd->device->id, cmd->device->lun,
		cmd->cmnd[0], cmd->serial_number, ei->ScsiStatus,
		cp->Header.Tag.upper, cp->Header.Tag.lower);
	/* for dev_warn */
	dl += sprintf(dmsg+dl, "Device:C%d:B%d:T%u:L%u Command:0x%02x ",
		h->scsi_host->host_no, cmd->device->channel, cmd->device->id, 
		cmd->device->lun, cmd->cmnd[0]);
		

	/* an error has occurred */
	switch (ei->CommandStatus) {


	case CMD_TARGET_STATUS:

		if (ei->ScsiStatus) {
			/* Get sense key */
			sense_key = 0xf & ei->SenseInfo[2];
			/* Get additional sense code */
			asc = ei->SenseInfo[12];
			/* Get addition sense code qualifier */
			ascq = ei->SenseInfo[13];
		}

		/* decode target status */
		switch(ei->ScsiStatus) {
		case SAM_STAT_CHECK_CONDITION:
			ml += sprintf(msg+ml, "CC:%02x/%02x/%02x ",
			sense_key, asc, ascq);
			dl += sprintf(dmsg+dl, "CC:%02x/%02x/%02x ",
			sense_key, asc, ascq);
#if defined (__VMKLNX__)
			/* In Vmware, need to be able to change 
			 * cmd->resid and/or cmd->result in several
			 * error status cases. 
	 		 */
			decode_CC(h, cp, msg, &ml, dmsg, &dl);

#else /* NOT defined (__VMKLNX__) */
			if (check_for_unit_attention(h, cp)) {
				cmd->result = DID_SOFT_ERROR << 16;
				break;
			}
			if (sense_key == ILLEGAL_REQUEST) {
				ml += sprintf(msg+ml, "ILLEGAL_REQUEST. ");
				dl += sprintf(dmsg+dl, "has Illegal Request. ");
				if (cp->Request.CDB[0] == REPORT_LUNS) {
					dl = 0; /* Suppress noisy complaint  */
						/* since SCSI_REPORT_LUNS is */
						/* commonly unsupported.     */
					break;
				}

				if ((asc == 0x20) && (ascq == 0x0)) 
					ml += sprintf(msg+ml, "Invalid OP code. ");
				else if ((asc == 0x24) && (ascq == 0x0)) 
					ml += sprintf(msg+ml, "Invalid field in CDB. ");
				else if ((asc == 0x25) && (ascq == 0x0)) {
					ml += sprintf(msg+ml, "LUN not supported. ");
					cmd->result = DID_NO_CONNECT << 16;
				}
				break;
			}

			if (sense_key == NOT_READY) {
				ml += sprintf(msg+ml, "NOT_READY. ");
				dl += sprintf(dmsg+dl, "NOT READY. ");
				if ((asc == 0x04) && (ascq == 0x0)) 
					ml += sprintf(msg+ml, "Logical unit not ready, "
						"Cause not reportable. ");
				else if ((asc == 0x04) && (ascq == 0x01)) 
					ml += sprintf(msg+ml, "In process of becoming ready. ");
				else if ((asc == 0x04) && (ascq == 0x03)) {
					cmd->result = DID_NO_CONNECT << 16;
					ml += sprintf(msg+ml, "Logical unit not ready, "
						"Manual Intervention required. ");
					dl += sprintf(dmsg+dl, "Logical unit not ready, "
						"Manual Intervention required. ");
				}
				else if ((asc == 0x2a) && (ascq == 0x06)) 
					ml += sprintf(msg+ml, "ALUA asymmetric access state change. ");
				else if ((asc == 0x3a) && (ascq == 0x0)) 
					ml += sprintf(msg+ml, "Medium not present. ");
				else if ((asc == 0x3a) && (ascq == 0x0)) 
					ml += sprintf(msg+ml, "LUN not configured. ");
				else if ((asc == 0x3e) && (ascq == 0x01)) 
					ml += sprintf(msg+ml, "Logical Unit Failure. ");
				break;
			}
			if (sense_key == MEDIUM_ERROR ) {
				ml += sprintf(msg+ml, "MEDIUM_ERROR. ");
				dl += sprintf(dmsg+dl, "has Medium Error. ");
				if ((asc == 0x0c) && (ascq == 0x0)) 
					ml += sprintf(msg+ml, "Unrecovered write. ");
				else if ((asc == 0x11) && (ascq == 0x0)) 
					ml += sprintf(msg+ml, "Unrecovered read. ");
				break;
			}
			if (sense_key == ABORTED_COMMAND) {
				ml += sprintf(msg+ml, "ABORTED_COMMAND. ");
				dl += sprintf(dmsg+dl, "ABORTED COMMAND. ");
				if ((asc == 0x2a) && (ascq == 0x06)) 
					ml += sprintf(msg+ml, "ALUA asymetric "
						"access state change: "
						"forwarded command aborted "
						"during failover. ");
				else if ((asc == 0x47) && (ascq == 0x0)) 
					ml += sprintf(msg+ml, "Parity or "
						"CRC error in data. ");
				else if ((asc == 0x4e) && (ascq == 0x0)) 
					ml += sprintf(msg+ml, "Overlapped "
						"commands. ");
				/* Aborted command is retryable */
				cmd->result = DID_SOFT_ERROR << 16;
				break;
			}
			/* Must be some other type of check condition */
			ml += sprintf(msg+ml, "Unknown type. ");
			dl += sprintf(dsmg+dl, "Unknown type. "
				"Sense: 0x%x ASC: 0x%x ASCQ: 0x%x, "
				"Return result: 0x%x, "
				"cmd=[%02x %02x %02x %02x %02x "
				"%02x %02x %02x %02x %02x %02x "
				"%02x %02x %02x %02x %02x] ",
				sense_key, asc, ascq, cmd->result,
				cmd->cmnd[0], cmd->cmnd[1],
				cmd->cmnd[2], cmd->cmnd[3],
				cmd->cmnd[4], cmd->cmnd[5],
				cmd->cmnd[6], cmd->cmnd[7],
				cmd->cmnd[8], cmd->cmnd[9],
				cmd->cmnd[10], cmd->cmnd[11],
				cmd->cmnd[12], cmd->cmnd[13],
				cmd->cmnd[14], cmd->cmnd[15]);
#endif
			break;
			/* end case ei->ScsiStatus == SAM_STAT_CHECK_CONDITION */

		case SAM_STAT_CONDITION_MET:
			ml += sprintf(msg+ml, "CONDITION_MET. ");
			dl += sprintf(dmsg+dl, "Reports condition met. ");
			break;

		case SAM_STAT_BUSY:
			ml += sprintf(msg+ml, "BUSY. ");
			dl += sprintf(dmsg+dl, "Device is busy. ");
			break;

		case SAM_STAT_INTERMEDIATE:
			ml += sprintf(msg+ml, "INTERMEDIATE CONDITION. ");
			dl += sprintf(dmsg+dl, "Has intermediate condition. ");
			break;

		case SAM_STAT_INTERMEDIATE_CONDITION_MET:
			ml += sprintf(msg+ml, "INTERMEDIATE CONDITION MET. ");
			dl += sprintf(dmsg+dl, "Has met an intermediate condition. ");
			break;

		case SAM_STAT_RESERVATION_CONFLICT:
			ml += sprintf(msg+ml, "RESERVATION CONFLICT. ");
			dl += sprintf(dmsg+dl, "Has reservation conflict. ");
			break;

		case SAM_STAT_COMMAND_TERMINATED:
			ml += sprintf(msg+ml, "COMMAND TERMINATED. ");
			dl += sprintf(dmsg+dl, "Has command terminated. ");
			break;

		case SAM_STAT_TASK_SET_FULL:
			ml += sprintf(msg+ml, "TASK_SET_FULL. ");
			dl += sprintf(dmsg+dl, "Has task set full. ");
			break;

		case SAM_STAT_ACA_ACTIVE:
			ml += sprintf(msg+ml, "ACA_ACTIVE. ");
			dl += sprintf(dmsg+dl, "Has ACA active. ");
			break;

		case SAM_STAT_TASK_ABORTED:
			ml += sprintf(msg+ml, "TASK_ABORTED. ");
			dl += sprintf(dmsg+dl, "Reports task aborted. ");
			break;

		default:	
			ml += sprintf(msg+ml, "UNKNOWN SCSI PROBLEM. ");
			dl += sprintf(dmsg+dl, "SCSI status was 0. "
				"Return no connection. ");
			cmd->result = DID_NO_CONNECT << 16;
			break; 

		} /* end switch decode target status */

		break; /* end case CMD_TARGET_STATUS */

	case CMD_DATA_UNDERRUN: /* let mid layer handle it. */
		dl = 0;	/* don't print any dev_warns on underrun */
		ml = 0;	/* don't print any debug msgs on underrun */ 
		break;

	case CMD_DATA_OVERRUN:
		ml += sprintf(msg+ml, "OVERRUN. ");
		dl += sprintf(dmsg+dl, "has OVERRUN. ");
#ifdef HPSA_DEBUG
		hpsa_db_scmnd("complete_scsi_command: OVERRUN: ", h, cmd);
#endif
		break;

	case CMD_INVALID: 
		ml += sprintf(msg+ml, "CMD_INVALID. ");
		dl += sprintf(dmsg+dl, "Command Invalid. ");
		/* We get CMD_INVALID if you address a non-existent device
		 * instead of a selection timeout (no response).  You will
		 * see this if you yank out a drive, then try to access it.
		 * This is kind of a shame because it means that any other
		 * CMD_INVALID (e.g. driver bug) will get interpreted as a
		 * missing target. */
		cmd->result = DID_NO_CONNECT << 16;
		break;

	case CMD_PROTOCOL_ERR:
		ml += sprintf(msg+ml, "PROTOCOL_ERROR. ");
		dl += sprintf(dmsg+dl, "has protocol error. ");
		break;

	case CMD_HARDWARE_ERR:
		ml += sprintf(msg+ml, "HARDWARE_ERROR. ");
		dl += sprintf(dmsg+dl, "has hardware error. ");
		cmd->result = DID_ERROR << 16;
		break;

	case CMD_CONNECTION_LOST:
		ml += sprintf(msg+ml, "CONNECTION_LOST. ");
		dl += sprintf(dmsg+dl, "has connection lost. ");
		cmd->result = DID_ERROR << 16;
		break;

	case CMD_ABORTED:
		ml += sprintf(msg+ml, "CMD_ABORTED. ");
		dl += sprintf(dmsg+dl, "Command was aborted with status "
			"0x%x. ", ei->ScsiStatus);
#if defined (__VMKLNX__)
		cmd->result = DID_RESET << 16;
		/* Reset residual count to avoid error message
		 * Error BytesXferred > Requested Length 
		 */	
		cmd->resid = cmd->request_bufflen;
#else /* NOT defined (__VMKLNX__) */
		cmd->result = DID_ABORT << 16;
#endif
		break;

	case CMD_ABORT_FAILED:
		ml += sprintf(msg+ml, "CMD_ABORT_FAILED. ");
		dl += sprintf(dmsg+dl, "Abort failed. ");
		cmd->result = DID_ERROR << 16;
		break;

	case CMD_UNSOLICITED_ABORT:
		ml += sprintf(msg+ml, "UNSOLICITED_ABORT. ");
		dl += sprintf(dmsg+dl, "Aborted due to unsolicited abort. ");
#if defined (__VMKLNX__)
		cmd->result = DID_RESET << 16;
		/* Reset residual count to avoid error message
		* Error BytesXferred > Requested Length 
		*/
		cmd->resid = cmd->request_bufflen;
		dev_warn(&h->pdev->dev, "aborted do to an unsolicited "
			"abort\n");
#else /* NOT defined (__VMKLNX__) */
		cmd->result = DID_RESET << 16;
#endif
		break;

	case CMD_TIMEOUT:
		ml += sprintf(msg+ml, "CMD_TIMEOUT. ");
		dl += sprintf(dmsg+dl, "Timed out. ");
		cmd->result = DID_TIME_OUT << 16;
		break;

	case CMD_UNABORTABLE:
		ml += sprintf(msg+ml, "CMD_UNABORTABLE. ");
		dl += sprintf(dmsg+dl, "Command is unabortable. ");
		cmd->result = DID_ERROR << 16;
		break;

	case 0x13:
		ml += sprintf(msg+ml, "Task Management Function status. ");
		dl += sprintf(dmsg+dl, "Returned a Task Management Function status. ");
		break;

	default:
		ml += sprintf(msg+ml, "Unknown status. ");
		dl += sprintf(dmsg+dl, "returned unknown status %x. ",
			ei->CommandStatus);
		cmd->result = DID_ERROR << 16;

	} /* end switch ei->CommandStatus */

        if (cmd->result != orig )
                ml += sprintf(msg+ml, "Returning %s. ",
                        host_status[(cmd->result >> 16)]);

	/* Show either the dev_warn messages,
	 * or detailed messages, depending on
	 * how controller's debug message flag is set.
	 */
	if ((h->debug_msg >= debug) && (ml > 0) )
		hpsa_dbmsg(h, debug, "%s\n", msg);
	else if (dl > 0 )
		dev_warn(&h->pdev->dev, "%s\n", dmsg);

	cmd->scsi_done(cmd);
	cmd_free(h, cp);
}

static int hpsa_scsi_detect(struct ctlr_info *h)
{
	struct Scsi_Host *sh;
	int error;

	sh = scsi_host_alloc(&hpsa_driver_template, sizeof(h));
	if (sh == NULL)
		goto fail;
#if defined(__VMKLNX__)
        if (! hpsa_register_controller(h))
                return 0;
#endif
	sh->io_port = 0;
	sh->n_io_port = 0;
	sh->this_id = -1;
	sh->max_channel = 3;
	sh->max_cmd_len = MAX_COMMAND_SIZE;
	sh->max_lun = HPSA_MAX_LUN;
	sh->max_id = HPSA_MAX_LUN;
	sh->can_queue = h->nr_cmds;
	sh->cmd_per_lun = h->nr_cmds;
	sh->sg_tablesize = h->maxsgentries;
	h->scsi_host = sh;
	sh->hostdata[0] = (unsigned long) h;
	sh->irq = h->intr[PERF_MODE_INT];
	sh->unique_id = sh->irq;

	hpsa_dbmsg(h, 1, "hpsa_scsi_detect: " 
		"maxsgentries = %d.\n", h->maxsgentries);
#if defined(__VMKLNX__)
        sh->transportt = hpsa_transport_template;
#endif
	error = scsi_add_host(sh, &h->pdev->dev);
	if (error)
		goto fail_host_put;

#if (defined(__VMKLNX__) && (defined(HPSA_ESX5_0) || defined(HPSA_ESX5_1) || defined(HPSA_ESX6_0)))

	vmklnx_scsi_register_poll_handler(sh, h->intr[PERF_MODE_INT],
		do_hpsa_intr, h);
#endif /* defined(__VMKLNX__) && defined(HPSA_ESX5_0) || defined(HPSA_ESX5_1) || defined(HPSA_ESX6_0) */

	hpsa_dbmsg(h, 1, "hpsa_scsi_detect: Calling scsi_scan_host.\n");
	scsi_scan_host(sh);
	return 0;

 fail_host_put:
	dev_err(&h->pdev->dev, "hpsa_scsi_detect: scsi_add_host"
		" failed for controller %d\n", h->ctlr);
	scsi_host_put(sh);
	return error;
 fail:
	dev_err(&h->pdev->dev, "hpsa_scsi_detect: scsi_host_alloc"
		" failed for controller %d\n", h->ctlr);
	return -ENOMEM;
}

static void hpsa_pci_unmap(struct pci_dev *pdev,
	struct CommandList *c, int sg_used, int data_direction)
{
	int i;
	union u64bit addr64;

	for (i = 0; i < sg_used; i++) {
		addr64.val32.lower = c->SG[i].Addr.lower;
		addr64.val32.upper = c->SG[i].Addr.upper;
		pci_unmap_single(pdev, (dma_addr_t) addr64.val, c->SG[i].Len,
			data_direction);
	}
}

static void hpsa_map_one(struct pci_dev *pdev,
		struct CommandList *cp,
		unsigned char *buf,
		size_t buflen,
		int data_direction)
{
	u64 addr64;

	if (buflen == 0 || data_direction == PCI_DMA_NONE) {
		cp->Header.SGList = 0;
		cp->Header.SGTotal = 0;
		return;
	}

	addr64 = (u64) pci_map_single(pdev, buf, buflen, data_direction);
	cp->SG[0].Addr.lower =
	  (u32) (addr64 & (u64) 0x00000000FFFFFFFF);
	cp->SG[0].Addr.upper =
	  (u32) ((addr64 >> 32) & (u64) 0x00000000FFFFFFFF);
	cp->SG[0].Len = buflen;
	cp->Header.SGList = (u8) 1;   /* no. SGs contig in this cmd */
	cp->Header.SGTotal = (u16) 1; /* total sgs in this cmd list */
}

static inline void hpsa_scsi_do_simple_cmd_core(struct ctlr_info *h,
	struct CommandList *c)
{
	DECLARE_COMPLETION_ONSTACK(wait);

	c->waiting = &wait;
	enqueue_cmd_and_start_io(h, c);
	wait_for_completion(&wait);
}

static void hpsa_scsi_do_simple_cmd_with_retry(struct ctlr_info *h,
	struct CommandList *c, int data_direction)
{
	int retry_count = 0;

	do {
		memset(c->err_info, 0, sizeof(c->err_info));
		hpsa_scsi_do_simple_cmd_core(h, c);
		retry_count++;
	} while (check_for_unit_attention(h, c) && retry_count <= 3);
	hpsa_pci_unmap(h->pdev, c, 1, data_direction);
}

static void hpsa_scsi_interpret_error(struct CommandList *cp)
{
	struct ErrorInfo *ei;
	struct device *d = &cp->h->pdev->dev;

        unsigned char sense_key = 0;
        unsigned char asc = 0;      /* additional sense code */
        unsigned char ascq = 0;     /* additional sense code qualifier */
	struct scsi_cmnd *cmd;
	char dmsg[256];		/* for dev_warn messages */
	int dl = 0;
	memset(dmsg, 0, sizeof(dmsg));
	char msg[256];		/* for detailed messages */
	int ml = 0;
	memset(msg, 0, sizeof(msg));

	ei = cp->err_info;
	cmd  = (struct scsi_cmnd *) cp->scsi_cmd;
	if (cmd == NULL ) 
		ml += sprintf(msg+ml, "Device:C%d "
			"ScsiStatus:0x%x Tag:0x%08x:%08x. ",
			cp->h->scsi_host->host_no, ei->ScsiStatus,
                        cp->Header.Tag.upper, cp->Header.Tag.lower);
	else 
		ml += sprintf(msg+ml, "Device:C%d:T%u:L%u "
			"Command:0x%02x SN:0x%lx ScsiStatus:0x%x "
                        "Tag:0x%08x:%08x. ",
			cp->h->scsi_host->host_no, 
			cmd->device->id, cmd->device->lun,
			cmd->cmnd[0], cmd->serial_number, ei->ScsiStatus,
                        cp->Header.Tag.upper, cp->Header.Tag.lower);

	dl += sprintf(dmsg+dl, "cmd ");

	switch (ei->CommandStatus) {
	case CMD_TARGET_STATUS:
		ml += sprintf(msg+ml, "TARGET_STATUS. ");
		dl += sprintf(dmsg+dl, "has completed with errors. ");
		dl += sprintf(dmsg+dl, "SCSI status = %x. ",
				ei->ScsiStatus);
		if (ei->ScsiStatus == 0)
			dl += sprintf(dmsg+dl, "SCSI status abnormally zero.  "
			"(probably indicates selection timeout "
			"reported incorrectly due to a known "
			"firmware bug, circa July, 2001.)\n");
		if ( ei->ScsiStatus  ) {
			sense_key = 0xf & ei->SenseInfo[2];
			asc = ei->SenseInfo[12];
			ascq = ei->SenseInfo[13];
			if (ei->ScsiStatus == SAM_STAT_CHECK_CONDITION ) {
				ml += sprintf(msg+ml, "CC:%02x/%02x/%02x ",
					sense_key, asc, ascq);
				dl += sprintf(dmsg+dl, "CC:%02x/%02x/%02x ",
					sense_key, asc, ascq);
				decode_CC(cp->h, cp, msg, &ml, dmsg, &dl);
			}
		}
		else {
			ml += sprintf(msg+ml, "Sense:%02x ASC:%02x ASCQ:%02x ", 
				sense_key, asc, ascq);	

		}
		break;
	case CMD_DATA_UNDERRUN: /* let mid layer handle it. */
			ml = 0; /* Don't print underrun errors. */
			dl = 0; /* Don't print underrun errors. */
			//dev_info(d, "UNDERRUN\n");
		break;
	case CMD_DATA_OVERRUN:
		dl += sprintf(dmsg+dl, "has completed with data overrun. ");
		ml += sprintf(msg+ml, "Completed with data overrun. ");
		break;
	case CMD_INVALID: {
		/* controller unfortunately reports SCSI passthru's
		 * to non-existent targets as invalid commands.
		 */
		dl += sprintf(dmsg+dl, "is reported invalid (probably means "
			"target device no longer present) ");
		ml += sprintf(msg+ml, "Command invalid. ");
		}
		break;
	case CMD_PROTOCOL_ERR:
		dl += sprintf(dmsg+dl, "has protocol error. ");
		ml += sprintf(msg+ml, "Protocol error. ");
		break;
	case CMD_HARDWARE_ERR:
		/* cmd->result = DID_ERROR << 16; */
		dl += sprintf(dmsg+dl, "had hardware error. ");
		ml += sprintf(msg+ml, "Hardware error. ");
		break;
	case CMD_CONNECTION_LOST:
		dl += sprintf(dmsg+dl, "had connection lost. ");
		ml += sprintf(msg+ml, "Connection Lost. ");
		break;
	case CMD_ABORTED:
		dl += sprintf(dmsg+dl, "was aborted. ");
#if defined (__VMKLNX__)
		if (cmd != NULL) {
			cmd->result = DID_RESET << 16;
			/* Reset residual count to avoid error message
			* Error BytesXferred > Requested Length 
			*/
			cmd->resid = cmd->request_bufflen;
		}
#else 
		cmd->result = DID_RESET << 16;
#endif
		ml += sprintf(msg+ml, "ABORTED. Tag: 0x%08x:%08x. ",
			cp->Header.Tag.upper, cp->Header.Tag.lower);
		break;
	case CMD_ABORT_FAILED:
		dl += sprintf(dmsg+dl, "reports abort failed. ");
		ml += sprintf(msg+ml, "Abort Failed. ");
		break;
	case CMD_UNSOLICITED_ABORT:
		dl += sprintf(dmsg+dl, "aborted due to unsolicited abort. ");
		ml += sprintf(msg+ml, "Unsolicited Abort. ");
#if defined (__VMKLNX__)
		if (cmd != NULL) {
			cmd->result = DID_RESET << 16;
			/* Reset residual count to avoid error message
			* Error BytesXferred > Requested Length 
			*/
			cmd->resid = cmd->request_bufflen;
		}
#else 
		ml += sprintf(msg+ml, "Return DID_RESET. ");
		cmd->result = DID_RESET << 16;
#endif
		break;
	case CMD_TIMEOUT:
		dl += sprintf(dmsg+dl, "timed out. ");
		ml += sprintf(msg+ml, "Timed out. ");
		break;
	case CMD_UNABORTABLE:
		dl += sprintf(dmsg+dl, "Command Unabortable. ");
		ml += sprintf(msg+ml, "Command Unabortable. ");
		break;	

	case 0x13: /* Task Management Function Status */
		dl += sprintf(dmsg+dl, "Task Management Function status. ");
		ml += sprintf(msg+ml, "Task Management Function Status. ");
		break;	

	default:
		dl += sprintf(dmsg+dl, "returned unknown status %x\n", 
			ei->CommandStatus);
		ml += sprintf(msg+ml, "Unknown status: %x. ", 
			ei->CommandStatus);
	}
	
        /* Show either the dev_warn or detail messages, 
         * depending on how controller's debug message flag is set.
         */
        int debug=1;    /* show detail at or above this level */
        if ((cp->h->debug_msg >= debug) && (ml > 0) )
                hpsa_dbmsg(cp->h, debug, "interpret error: %s\n", msg);
        else if (dl > 0 )
                dev_warn(d, "%s\n", dmsg);
}

static int hpsa_scsi_do_inquiry(struct ctlr_info *h, unsigned char *scsi3addr,
		unsigned char is_vpd, unsigned char page, unsigned char *buf,
		unsigned char bufsize)
{
	int rc = IO_OK;
	struct CommandList *c;
	struct ErrorInfo *ei;

	c = cmd_special_alloc(h);

	if (c == NULL) {			/* trouble... */
		dev_warn(&h->pdev->dev, "cmd_special_alloc returned NULL!\n");
		return -ENOMEM;
	}

	if (is_vpd)
		fill_cmd(c, HPSA_VPD_INQUIRY, h, buf, bufsize, page, scsi3addr, TYPE_CMD);
	else
		fill_cmd(c, HPSA_INQUIRY, h, buf, bufsize, page, scsi3addr, TYPE_CMD);
	hpsa_scsi_do_simple_cmd_with_retry(h, c, PCI_DMA_FROMDEVICE);
	ei = c->err_info;
	if (ei->CommandStatus != 0 && ei->CommandStatus != CMD_DATA_UNDERRUN) {
		hpsa_scsi_interpret_error(c);
		rc = -1;
	}
	cmd_special_free(h, c);
	return rc;
}

static int hpsa_send_reset(struct ctlr_info *h, unsigned char *scsi3addr, 
	int reset_type)
{
	int rc = IO_OK;
	struct CommandList *c;
	struct ErrorInfo *ei;

	c = cmd_special_alloc(h);

	if (c == NULL) {			/* trouble... */
		dev_warn(&h->pdev->dev, "cmd_special_alloc returned NULL!\n");
		return -ENOMEM;
	}

	fill_cmd(c, reset_type, h, NULL, 0, 0, scsi3addr, TYPE_MSG);
	hpsa_scsi_do_simple_cmd_core(h, c);
	/* no unmap needed here because no data xfer. */

	ei = c->err_info;
	if (ei->CommandStatus != 0) {
		hpsa_scsi_interpret_error(c);
		rc = -1;
	}
	cmd_special_free(h, c);
	return rc;
}

static int hpsa_send_abort(struct ctlr_info *h, unsigned char *scsi3addr,
	void *abort)
{
	int rc = IO_OK;
	struct CommandList *c;
	struct ErrorInfo *ei;

	c = cmd_special_alloc(h);

	if (c == NULL) {	/* trouble... */
		dev_warn(&h->pdev->dev, "cmd_special_alloc returned NULL!\n");
		return -ENOMEM;
	}

	fill_cmd(c, HPSA_ABORT_MSG, h, abort, 0, 0, scsi3addr, TYPE_MSG);
	hpsa_scsi_do_simple_cmd_core(h, c);
	hpsa_dbmsg(h, 3, "hpsa_send_abort: Tag:0x%08x:%08x: "
		"do_simple_cmd_core completed.\n",
		((struct CommandList *)abort)->Header.Tag.upper,
		((struct CommandList *)abort)->Header.Tag.lower);
	/* no unmap needed here because no data xfer. */

	ei = c->err_info;
	if (ei->CommandStatus != 0) {
		hpsa_dbmsg(h, 3, "hpsa_send_abort: Tag:0x%08x:%08x: "
			"interpreting error.\n",
			((struct CommandList *) abort)->Header.Tag.upper,
			((struct CommandList *) abort)->Header.Tag.lower);
		hpsa_scsi_interpret_error(c);
		rc = -1;
	}
	cmd_special_free(h, c);
	hpsa_dbmsg(h, 3, "hpsa_send_abort: Tag:0x%08x:%08x: "
		"Finished.\n",
		((struct CommandList *) abort)->Header.Tag.upper,
		((struct CommandList *) abort)->Header.Tag.lower);
	return rc;
}

static void hpsa_get_raid_level(struct ctlr_info *h,
	unsigned char *scsi3addr, unsigned char *raid_level)
{
	int rc;
	unsigned char *buf;

	*raid_level = RAID_UNKNOWN;
	buf = kzalloc(64, GFP_KERNEL);
	if (!buf)
		return;
	rc = hpsa_scsi_do_inquiry(h, scsi3addr, HPSA_IS_VPD, HPSA_VPD_LV_DEVICE_GEOMETRY, buf, 64);
	if (rc == 0)
		*raid_level = buf[8];
	if (*raid_level > RAID_UNKNOWN)
		*raid_level = RAID_UNKNOWN;
	kfree(buf);
	return;
}

/* Get the device id from inquiry page 0x83 */
static int hpsa_get_device_id(struct ctlr_info *h, unsigned char *scsi3addr,
	unsigned char *device_id, int buflen)
{
	int rc;
	unsigned char *buf;

	if (buflen > 16)
		buflen = 16;
	buf = kzalloc(64, GFP_KERNEL);
	if (!buf)
		return -1;
	rc = hpsa_scsi_do_inquiry(h, scsi3addr, HPSA_IS_VPD, HPSA_VPD_LV_DEVICE_ID, buf, 64);
	if (rc == 0)
		memcpy(device_id, &buf[8], buflen);
	kfree(buf);
	return rc != 0;
}

static int hpsa_scsi_do_ext_report_luns(struct ctlr_info *h, int logical,
		struct ReportExtendedLUNdata *buf, int bufsize,
		int extended_response)
{
	int rc = IO_OK;
	struct CommandList *c;
	unsigned char scsi3addr[8];
	struct ErrorInfo *ei;

	c = cmd_special_alloc(h);
	if (c == NULL) {			/* trouble... */
		dev_err(&h->pdev->dev, "cmd_special_alloc returned NULL!\n");
		return -1;
	}
	/* address the controller */
	memset(scsi3addr, 0, sizeof(scsi3addr));
	fill_cmd(c, logical ? HPSA_REPORT_LOG : HPSA_REPORT_PHYS, h,
		buf, bufsize, 0, scsi3addr, TYPE_CMD);
	if (extended_response)
		c->Request.CDB[1] = extended_response;
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

static int hpsa_scsi_do_report_luns(struct ctlr_info *h, int logical,
		struct ReportLUNdata *buf, int bufsize,
		int extended_response)
{
	int rc = IO_OK;
	struct CommandList *c;
	unsigned char scsi3addr[8];
	struct ErrorInfo *ei;

	c = cmd_special_alloc(h);
	if (c == NULL) {			/* trouble... */
		dev_err(&h->pdev->dev, "cmd_special_alloc returned NULL!\n");
		return -1;
	}
	/* address the controller */
	memset(scsi3addr, 0, sizeof(scsi3addr));
	fill_cmd(c, logical ? HPSA_REPORT_LOG : HPSA_REPORT_PHYS, h,
		buf, bufsize, 0, scsi3addr, TYPE_CMD);
	if (extended_response)
		c->Request.CDB[1] = extended_response;
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

static inline int hpsa_scsi_do_report_phys_luns(struct ctlr_info *h,
		struct ReportLUNdata *buf,
		int bufsize, int extended_response)
{
	return hpsa_scsi_do_report_luns(h, 0, buf, bufsize, extended_response);
}

static inline int hpsa_scsi_do_report_log_luns(struct ctlr_info *h,
		struct ReportLUNdata *buf, int bufsize)
{
	return hpsa_scsi_do_report_luns(h, 1, buf, bufsize, 0);
}

static inline void hpsa_set_bus_target_lun(struct hpsa_scsi_dev_t *device,
	int bus, int target, int lun)
{
	device->bus = bus;
	device->target = target;
	device->lun = lun;
}

/* Does controller support VPD for logical volume status? */
static int hpsa_vpd_lv_status_supported(struct ctlr_info *h,
	unsigned char scsi3addr[])
{
        int rc;
	int i;
	int pages;
        unsigned char *buf;

        buf = kzalloc(64, GFP_KERNEL);
        if (!buf)
		return 0;

	/* Get the size of the page list first */
        rc = hpsa_scsi_do_inquiry(h, scsi3addr, HPSA_IS_VPD,
		HPSA_VPD_SUPPORTED_PAGES, buf, HPSA_VPD_HEADER_SZ);
        if (rc != 0)
		goto exit_unsupported;
	pages = buf[3];

	/* Get the whole VPD page list */
        rc = hpsa_scsi_do_inquiry(h, scsi3addr, HPSA_IS_VPD,
		HPSA_VPD_SUPPORTED_PAGES, buf, pages + HPSA_VPD_HEADER_SZ);
        if (rc != 0)
		goto exit_unsupported;

	pages = buf[3];
	for (i = 1; i <= pages; i++ ) {
		if ( buf[3 + i] == HPSA_VPD_LV_STATUS )
			goto exit_supported;
	}
exit_unsupported:
	dev_warn(&h->pdev->dev, "Could not retrieve list of supported VPD pages.\n");
	kfree(buf);
	return 0;
exit_supported:
	kfree(buf);
	return 1;
}


/* Print a message explaining various volume states */
static void hpsa_show_volume_status(struct ctlr_info *h,
	struct hpsa_scsi_dev_t *sd)
{
	switch (sd->volume_offline) {
		case HPSA_VPD_LV_STATUS_UNSUPPORTED:
	                dev_info(&h->pdev->dev,
				"C%d:B%d:T%d:L%d Volume status is not "
				"available through vital product data pages.\n",
				h->scsi_host->host_no,
	                        sd->bus, sd->target, sd->lun);
			break;
		case HPSA_LV_OK:
			break;
		case HPSA_LV_UNDERGOING_ERASE:
			dev_info(&h->pdev->dev,
				"C%d:B%d:T%d:L%d Volume is undergoing "
				"background erase process.\n",
				h->scsi_host->host_no,
				sd->bus, sd->target, sd->lun);
			break;
		case HPSA_LV_UNDERGOING_RPI:
			dev_info(&h->pdev->dev,
				"C%d:B%d:T%d:L%d Volume is undergoing "
				"rapid parity initialization process.\n",
				h->scsi_host->host_no,
				sd->bus, sd->target, sd->lun);
			break;
		case HPSA_LV_PENDING_RPI:
			dev_info(&h->pdev->dev,
				"C%d:B%d:T%d:L%d Volume is queued for "
				"rapid parity initialization process.\n",
				h->scsi_host->host_no,
				sd->bus, sd->target, sd->lun);
			break;
		case HPSA_LV_ENCRYPTED_NO_KEY:
			dev_info(&h->pdev->dev,
				"C%d:B%d:T%d:L%d "
				"Volume is encrypted and cannot be accessed because "
				"key is not present.\n",
				h->scsi_host->host_no,
				sd->bus, sd->target, sd->lun);
			break;
		case HPSA_LV_PLAINTEXT_IN_ENCRYPT_ONLY_CONTROLLER:
			dev_info(&h->pdev->dev,
				"C%d:B%d:T%d:L%d Volume is not encrypted "
				"and cannot be accessed because "
				"controller is in encryption-only mode.\n",
				h->scsi_host->host_no,
				sd->bus, sd->target, sd->lun);
			break;
		case HPSA_LV_UNDERGOING_ENCRYPTION:
			dev_info(&h->pdev->dev,
				"C%d:B%d:T%d:L%d Volume is undergoing "
				"encryption process.\n",
				h->scsi_host->host_no,
				sd->bus, sd->target, sd->lun);
			break;
		case HPSA_LV_UNDERGOING_ENCRYPTION_REKEYING:
			dev_info(&h->pdev->dev,
				"C%d:B%d:T%d:L%d Volume is undergoing "
				"encryption re-keying process.\n",
				h->scsi_host->host_no,
				sd->bus, sd->target, sd->lun);
			break;
		case HPSA_LV_ENCRYPTED_IN_NON_ENCRYPTED_CONTROLLER:
			dev_info(&h->pdev->dev,
				"C%d:B%d:T%d:L%d Volume is encrypted "
				"and cannot be accessed because "
				"controller does not have encryption enabled.\n",
				h->scsi_host->host_no,
				sd->bus, sd->target, sd->lun);
			break;
		case HPSA_LV_PENDING_ENCRYPTION:
			dev_info(&h->pdev->dev,
				"C%d:B%d:T%d:L%d Volume is pending migration "
				"to encrypted state, but process has not "
				"started.\n",
				h->scsi_host->host_no,
				sd->bus, sd->target, sd->lun);
			break;
		case HPSA_LV_PENDING_ENCRYPTION_REKEYING:
			dev_info(&h->pdev->dev,
				"C%d:B%d:T%d:L%d Volume is encrypted "
				"and is pending encryption rekeying.\n",
				h->scsi_host->host_no,
				sd->bus, sd->target, sd->lun);
			break;


	}
}

/* Use VPD inquiry to get details of volume status */
static int hpsa_get_volume_status(struct ctlr_info *h,
	unsigned char scsi3addr[])
{
	int rc;
        int status;
	int size;
        unsigned char *buf;

        buf = kzalloc(64, GFP_KERNEL);
        if (!buf)
		return HPSA_VPD_LV_STATUS_UNSUPPORTED;

	/* Does controller have VPD for logical volume status? */
	if ( !hpsa_vpd_lv_status_supported(h, scsi3addr)) {
		dev_warn(&h->pdev->dev, "Logical volume status VPD page is unsupported.\n");
		goto exit_failed;
	}

	/* Get the size of the VPD return buffer */
        rc = hpsa_scsi_do_inquiry(h, scsi3addr, HPSA_IS_VPD,
		HPSA_VPD_LV_STATUS, buf, HPSA_VPD_HEADER_SZ);
        if (rc != 0) {
		dev_warn(&h->pdev->dev, "Logical volume status VPD inquiry failed.\n");
		goto exit_failed;
	}
	size = buf[3];

	/* Now get the whole VPD buffer */
        rc = hpsa_scsi_do_inquiry(h, scsi3addr, HPSA_IS_VPD,
		HPSA_VPD_LV_STATUS, buf, size + HPSA_VPD_HEADER_SZ);
        if (rc != 0) {
		dev_warn(&h->pdev->dev, "Logical volume status VPD inquiry failed.\n");
		goto exit_failed;
	}
	status = buf[4]; /* status byte */

	kfree(buf);
	return status;
exit_failed:
        kfree(buf);
	return HPSA_VPD_LV_STATUS_UNSUPPORTED;
}

/* Determine offline status of a volume.
 * Return either:
 *  0 (not offline)
 * -1 (offline for unknown reasons)
 *  # (integer code indicating one of several NOT READY states
 *     describing why a volume is to be kept offline)
 */
static unsigned char hpsa_volume_offline(struct ctlr_info *h,
	unsigned char scsi3addr[])
{
	struct CommandList *c;
	unsigned char *sense, sense_key, asc, ascq;
	u8 ldstat = 0;
#define ASC_LUN_NOT_READY 0x04
#define ASCQ_LUN_NOT_READY_FORMAT_IN_PROGRESS 0x04
#define ASCQ_LUN_NOT_READY_INITIALIZING_CMD_REQ 0x02
	c= cmd_special_alloc(h);
	if (!c)
		return 0;
	fill_cmd(c, TEST_UNIT_READY, h, NULL, 0, 0, scsi3addr, TYPE_CMD);
	hpsa_scsi_do_simple_cmd_core(h, c);
	sense = c->err_info->SenseInfo;
	sense_key = sense[2];
	asc = sense[12];
	ascq = sense[13];
	/* Is the volume 'not ready'? */
	if (c->err_info->CommandStatus == CMD_TARGET_STATUS &&
		c->err_info->ScsiStatus == SAM_STAT_CHECK_CONDITION &&
		sense_key == NOT_READY &&
		asc == ASC_LUN_NOT_READY ) {

		/* Determine the reason for not ready state. */
		ldstat = hpsa_get_volume_status(h, scsi3addr);

		/* If VPD status page isn't available,
		 * use ASC/ASCQ to determine state
		 */
		if (ldstat == HPSA_VPD_LV_STATUS_UNSUPPORTED) {
			if ( (ascq == ASCQ_LUN_NOT_READY_FORMAT_IN_PROGRESS) ||
				(ascq == ASCQ_LUN_NOT_READY_INITIALIZING_CMD_REQ) )
				goto exit_offline;
			else
				goto exit_online;
		}
		/* Keep volume offline in certain cases: */
		switch (ldstat) {
			case HPSA_LV_UNDERGOING_ERASE:
			case HPSA_LV_UNDERGOING_RPI:
			case HPSA_LV_PENDING_RPI:
			case HPSA_LV_ENCRYPTED_NO_KEY:
			case HPSA_LV_PLAINTEXT_IN_ENCRYPT_ONLY_CONTROLLER:
			case HPSA_LV_UNDERGOING_ENCRYPTION:
			case HPSA_LV_UNDERGOING_ENCRYPTION_REKEYING:
			case HPSA_LV_ENCRYPTED_IN_NON_ENCRYPTED_CONTROLLER:
				goto exit_offline;
				break;
		}
	}
exit_online:
	cmd_special_free(h, c);
	return 0;
exit_offline:
	cmd_special_free(h, c);
	return ldstat;
}

static int hpsa_update_device_info(struct ctlr_info *h,
	unsigned char scsi3addr[], struct hpsa_scsi_dev_t *this_device)
{
#define OBDR_TAPE_INQ_SIZE 49
	unsigned char *inq_buff;

	inq_buff = kzalloc(OBDR_TAPE_INQ_SIZE, GFP_KERNEL);
	if (!inq_buff)
		goto bail_out;

	hpsa_dbmsg(h, 1, "hpsa_update_device_info: begin.\n");

	/* Do an inquiry to the device to see what it is. */
	if (hpsa_scsi_do_inquiry(h, scsi3addr, HPSA_NOT_VPD, 0, inq_buff,
		(unsigned char) OBDR_TAPE_INQ_SIZE) != 0) {
		/* Inquiry failed (msg printed already) */
		hpsa_dbmsg(h, 1, "hpsa_update_device_info: "
			"inquiry failed.\n");
		dev_err(&h->pdev->dev,
			"hpsa_update_device_info: inquiry failed\n");
		goto bail_out;
	}

	/* As a side effect, record the firmware version number
	 * if we happen to be talking to the RAID controller.
	 */
	if (is_hba_lunid(scsi3addr))
		memcpy(h->firm_ver, &inq_buff[32], 4);

	this_device->devtype = (inq_buff[0] & 0x1f);
	memcpy(this_device->scsi3addr, scsi3addr, 8);
	memcpy(this_device->vendor, &inq_buff[8],
		sizeof(this_device->vendor));
	memcpy(this_device->model, &inq_buff[16],
		sizeof(this_device->model));
	memcpy(this_device->revision, &inq_buff[32],
		sizeof(this_device->revision));
	memset(this_device->device_id, 0,
		sizeof(this_device->device_id));
	hpsa_get_device_id(h, scsi3addr, this_device->device_id,
		sizeof(this_device->device_id));

	if (this_device->devtype == TYPE_DISK &&
		is_logical_dev_addr_mode(scsi3addr)) {
		hpsa_get_raid_level(h, scsi3addr, &this_device->raid_level);
		this_device->volume_offline =
			hpsa_volume_offline(h, scsi3addr);
	} else {
		this_device->raid_level = RAID_UNKNOWN;
		this_device->volume_offline = 0;
	}

	kfree(inq_buff);
	return 0;

bail_out:
	kfree(inq_buff);
	return 1;
}

static unsigned char *ext_target_model[] = {
	"MSA2012",
	"MSA2024",
	"MSA2312",
	"MSA2324",
	"P2000 G3 SAS",
	NULL,
};

static int is_ext_target(struct ctlr_info *h, struct hpsa_scsi_dev_t *device)
{
	int i;

	for (i = 0; ext_target_model[i]; i++)
		if (strncmp(device->model, ext_target_model[i],
			strlen(ext_target_model[i])) == 0)
			return 1;
	return 0;
}

/* Helper function to assign bus, target, lun mapping of devices.
 * Puts non-external target logical volumes on bus 0, external target logical
 * volumes on bus 1, physical devices on bus 2. and the hba on bus 3.
 * Logical drive target and lun are assigned at this time, but
 * physical device lun and target assignment are deferred (assigned
 * in hpsa_find_target_lun, called by hpsa_scsi_add_entry.)
 * VMware behavior is altered in that all devices reside on bus 0.
 */
static void figure_bus_target_lun(struct ctlr_info *h,
	u8 *lunaddrbytes, int *bus, int *target, int *lun,
	struct hpsa_scsi_dev_t *device)
{
	u32 lunid = 0;

	if (is_logical_dev_addr_mode(lunaddrbytes)) {
		/* logical device */
		lunid = le32_to_cpu(*((__le32 *) lunaddrbytes));
		if (unlikely(is_ext_target(h, device))) {
			/* External arrays assign lunids. 
			 * Target ids are embedded in them,
			 * linked to ports on array controller
			 */
			hpsa_dbmsg(h, 3, "figure_bus_target_lun: "
				"External array logical device\n");
#if defined (__VMKLNX__)
			*bus = 0;	/* Only one bus with VMware */
#else /* NOT defined (__VMKLNX__) */
			*bus = 1;
#endif
			*target = (lunid >> 16) & 0x3fff;
			*lun = lunid & 0x00ff;

		} else {
			if (is_scsi_rev_5(h)) {
				/* SCSI Revision 5: lun ids
				 * match SCSI REPORT LUNS data.
				 */
				hpsa_dbmsg(h, 3, "figure_bus_target_lun: "
					"SCSI Revision 5 logical device\n");
				*bus = 0;
				*target = 0;
				*lun = (lunid & 0x3fff) + 1;
			} else {
				/* Traditional smart array way. */
				hpsa_dbmsg(h, 3, "figure_bus_target_lun: " 
					"Smart Array logical device.\n");
				*bus = 0;
				*lun = lunid & 0x00ff;
				*target = (lunid >> 16) & 0x3fff;
			}
		}
	} else {
		/* physical device */
		if (is_hba_lunid(lunaddrbytes))
			if (unlikely(is_scsi_rev_5(h))) {
				hpsa_dbmsg(h, 3, "figure_bus_target_lun: "
					"SCSI Revision 5 physical device\n");
				*bus = 0; 
				*target = 0;
				*lun = 0;
				return;
			} else {
#if defined (__VMKLNX__)
				hpsa_dbmsg(h, 3, "figure_bus_target_lun: "
					"Smart Array physical controller\n");
				*bus = 0;
#else /* NOT defined (__VMKLNX__) */

				*bus = 3; /* traditional smartarray */
#endif
			}
		else {
			hpsa_dbmsg(h, 3, "figure_bus_target_lun: "
				"Physical disk\n");
			*bus = 2; /* physical disk */
		}
#if defined (__VMKLNX__)
		hpsa_dbmsg(h, 3, "figure_bus_target_lun: "
			"Set target for Physical device.\n");
		*target = (lunid >> 16) & 0x3fff;
		*lun = 0;
#else /* NOT defined (__VMKLNX__) */
		*target = -1;
		*lun = -1; /* we will fill these in later. */
#endif
	}
	hpsa_dbmsg(h, 3, "figure_bus_target_lun: "
		"B%u:T%u:L%u\n",
		*bus, *target, *lun);
}

/*
 * If there is no lun 0 on a target, linux won't find any devices.
 * For the external targets (arrays), we have to manually detect the enclosure
 * which is at lun zero, as CCISS_REPORT_PHYSICAL_LUNS doesn't report
 * it for some reason.  *tmpdevice is the target we're adding,
 * this_device is a pointer into the current element of currentsd[]
 * that we're building up in update_scsi_devices(), below.
 * lunzerobits is a bitmap that tracks which targets already have a
 * lun 0 assigned.
 * Returns 1 if an enclosure was added, 0 if not.
 */
static int add_ext_target_dev(struct ctlr_info *h,
	struct hpsa_scsi_dev_t *tmpdevice,
	struct hpsa_scsi_dev_t *this_device, u8 *lunaddrbytes,
	int bus, int target, int lun, unsigned long lunzerobits[],
	int *num_ext_target_devs)
{
	unsigned char scsi3addr[8];

	if (test_bit(target, lunzerobits)) {
		hpsa_dbmsg(h, 3, "add_enclosure_device: "
			"There is already a lun 0 for T%d:L%d\n",
			target, lun);
		return 0; /* There is already a lun 0 on this target. */
	}

	if (!is_logical_dev_addr_mode(lunaddrbytes)) {
		hpsa_dbmsg(h, 3, "add_enclosure_device: "
			"T%d:L%d is not a logical target.\n",
			target, lun);
		return 0; /* It's the logical targets that may lack lun 0. */
	}

	if (!is_ext_target(h, tmpdevice)) {
		hpsa_dbmsg(h, 3, "add_enclosure_device: "
			"T%d:L%d is not on an external target device.\n",
			target, lun);
		return 0; /* Only external target devices have this problem. */
	}

	if (lun == 0) { /* if lun is 0, then obviously we have a lun 0. */
		hpsa_dbmsg(h, 3, "add_enclosure_device: "
			"T%d:L%d is a LUN 0, so enclosure not needed.\n",
			target, lun);
		return 0;
	}

	memset(scsi3addr, 0, 8);
	scsi3addr[3] = target;

	if (is_hba_lunid(scsi3addr)) {
		hpsa_dbmsg(h, 3, "add_enclosure_device: "
			"T%d:L%d scsi3addr "
			"0x%02x%02x%02x%02x %02x%02x%02x%02x "
			"is the hba's lunid.\n",
			target, lun,
			scsi3addr[0], scsi3addr[1],
			scsi3addr[2], scsi3addr[3],
			scsi3addr[4], scsi3addr[5],
			scsi3addr[6], scsi3addr[7]);
		return 0; /* Don't add the RAID controller here. */
	}

	if (is_scsi_rev_5(h)) {
		hpsa_dbmsg(h, 3, "add_enclosure_device: "
			"T%d:L%d is a SCSI rev5 device. "
			"These do not need added enclosure device.\n",
			target, lun);
		return 0; /* p1210m doesn't need to do this. */
	}

	if (*num_ext_target_devs >= MAX_EXT_TARGETS) {
		dev_warn(&h->pdev->dev, "Maximum number of external "
			"target devices exceeded.  Check your hardware "
			"configuration.");
		return 0;
	}

	hpsa_dbmsg(h, 3, "Add enclosure device for target %d\n", target);
	if (hpsa_update_device_info(h, scsi3addr, this_device)) {
		hpsa_dbmsg(h, 3, "add enclosure failed: "
			"B%d:T%d:L%d: Could not get device info\n",
			bus, target, lun);
		return 0;
	}	
	(*num_ext_target_devs)++;
	hpsa_set_bus_target_lun(this_device, bus, target, 0);
	set_bit(target, lunzerobits);
	hpsa_db_dev("add_enclosure_device", h, this_device, -1);
	return 1;
}

/*
 * Do CISS_REPORT_PHYS and CISS_REPORT_LOG.  Data is returned in physdev,
 * logdev.  The number of luns in physdev and logdev are returned in
 * *nphysicals and *nlogicals, respectively.
 * Returns 0 on success, -1 otherwise.
 */
static int hpsa_gather_lun_info(struct ctlr_info *h,
	int reportlunsize,
	struct ReportLUNdata *physdev, u32 *nphysicals,
	struct ReportLUNdata *logdev, u32 *nlogicals)
{
	if (hpsa_scsi_do_report_phys_luns(h, physdev, reportlunsize, 0)) {
		dev_err(&h->pdev->dev, "report physical LUNs failed.\n");
		return -1;
	}
	*nphysicals = be32_to_cpu(*((__be32 *)physdev->LUNListLength)) / 8;
	if (*nphysicals > HPSA_MAX_PHYS_LUN) {
		dev_warn(&h->pdev->dev, "maximum physical LUNs (%d) exceeded."
			"  %d LUNs ignored.\n", HPSA_MAX_PHYS_LUN,
			*nphysicals - HPSA_MAX_PHYS_LUN);
		*nphysicals = HPSA_MAX_PHYS_LUN;
	}
	if (hpsa_scsi_do_report_log_luns(h, logdev, reportlunsize)) {
		dev_err(&h->pdev->dev, "report logical LUNs failed.\n");
		return -1;
	}
	*nlogicals = be32_to_cpu(*((__be32 *) logdev->LUNListLength)) / 8;
	/* Reject Logicals in excess of our max capability. */
	if (*nlogicals > HPSA_MAX_LUN) {
		dev_warn(&h->pdev->dev,
			"maximum logical LUNs (%d) exceeded.  "
			"%d LUNs ignored.\n", HPSA_MAX_LUN,
			*nlogicals - HPSA_MAX_LUN);
			*nlogicals = HPSA_MAX_LUN;
	}
	if (*nlogicals + *nphysicals > HPSA_MAX_PHYS_LUN) {
		dev_warn(&h->pdev->dev,
			"maximum logical + physical LUNs (%d) exceeded. "
			"%d LUNs ignored.\n", HPSA_MAX_PHYS_LUN,
			*nphysicals + *nlogicals - HPSA_MAX_PHYS_LUN);
		*nlogicals = HPSA_MAX_PHYS_LUN - *nphysicals;
	}
	return 0;
}

u8 *figure_lunaddrbytes(struct ctlr_info *h, int raid_ctlr_position, int i,
	int nphysicals, int nlogicals, struct ReportLUNdata *physdev_list,
	struct ReportLUNdata *logdev_list)
{
	/* Helper function, figure out where the LUN ID info is coming from
	 * given index i, lists of physical and logical devices, where in
	 * the list the raid controller is supposed to appear (first or last)
	 */

	int logicals_start = nphysicals + (raid_ctlr_position == 0);
	int last_device = nphysicals + nlogicals + (raid_ctlr_position == 0);

	if (i == raid_ctlr_position)
		return RAID_CTLR_LUNID;

	if (i < logicals_start)
		return &physdev_list->LUN[i - (raid_ctlr_position == 0)][0];

	if (i < last_device)
		return &logdev_list->LUN[i - nphysicals -
			(raid_ctlr_position == 0)][0];
	BUG();
	return NULL;
}

static void hpsa_update_scsi_devices(struct ctlr_info *h, int hostno)
{
	/* the idea here is we could get notified
	 * that some devices have changed, so we do a report
	 * physical luns and report logical luns cmd, and adjust
	 * our list of devices accordingly.
	 *
	 * The scsi3addr's of devices won't change so long as the
	 * adapter is not reset.  That means we can rescan and
	 * tell which devices we already know about, vs. new
	 * devices, vs.  disappearing devices.
	 */
	hpsa_dbmsg(h, 1, "Begin hpsa_update_scsi_devices.\n");
	struct ReportLUNdata *physdev_list = NULL;
	struct ReportLUNdata *logdev_list = NULL;
	struct ReportExtendedLUNdata *extended_phys_list = NULL;
	struct SenseSubsystem_info *sense_subsystem_info = NULL;
	unsigned char *inq_buff = NULL;
	u32 nphysicals = 0;
	u32 nlogicals = 0;
	u32 ndev_allocated = 0;
	struct hpsa_scsi_dev_t **currentsd, *this_device, *tmpdevice;
	int ncurrent = 0;
	int reportlunsize = sizeof(*physdev_list) + HPSA_MAX_PHYS_LUN * 8;
	int extended_phys_size = sizeof(*extended_phys_list) + HPSA_MAX_PHYS_LUN * 24;
	int i, num_ext_target_devs, ndevs_to_allocate;
	int bus, target, lun;
	int raid_ctlr_position;
	DECLARE_BITMAP(lunzerobits, MAX_EXT_TARGETS);

	currentsd = kzalloc(sizeof(*currentsd) * HPSA_MAX_DEVICES,
		GFP_KERNEL);
	physdev_list = kzalloc(reportlunsize, GFP_KERNEL);
	logdev_list = kzalloc(reportlunsize, GFP_KERNEL);
	extended_phys_list = kzalloc(extended_phys_size, GFP_KERNEL);
	sense_subsystem_info = kzalloc(sizeof(struct SenseSubsystem_info), GFP_KERNEL);
	inq_buff = kmalloc(OBDR_TAPE_INQ_SIZE, GFP_KERNEL);
	tmpdevice = kzalloc(sizeof(*tmpdevice), GFP_KERNEL);

	if (!currentsd || !physdev_list || !logdev_list ||
		!extended_phys_list || !sense_subsystem_info ||
		!inq_buff || !tmpdevice) {
		dev_err(&h->pdev->dev, "out of memory\n");
		goto out;
	}
	memset(lunzerobits, 0, sizeof(lunzerobits));

	/* Set the controller and target SAS IDs. */
	if (hpsa_set_sas_ids(h, extended_phys_size, extended_phys_list,
		sense_subsystem_info, &nphysicals)) {
		hpsa_dbmsg(h, 1, "hpsa_update_scsi_devices: failed to get sas ids.\n");
		goto out;
	}

	if (hpsa_gather_lun_info(h, reportlunsize, physdev_list, &nphysicals,
			logdev_list, &nlogicals))
		goto out;

	/* We might see up to the maximum number of logical and physical disks
	 * plus external target devices, and a device for the local RAID 
	 * controller.
	 */
	ndevs_to_allocate = nphysicals + nlogicals + MAX_EXT_TARGETS + 1;

	/* Allocate the per device structures */
	for (i = 0; i < ndevs_to_allocate; i++) {
		if ( i > HPSA_MAX_DEVICES ) {
			dev_warn(&h->pdev->dev, "maximum devices (%d) exceeded."
				" %d devices ignored.\n", HPSA_MAX_DEVICES,
				ndevs_to_allocate - HPSA_MAX_DEVICES);
			break;
		}

		currentsd[i] = kzalloc(sizeof(*currentsd[i]), GFP_KERNEL);
		if (!currentsd[i]) {
			dev_warn(&h->pdev->dev, "out of memory at %s:%d\n",
				__FILE__, __LINE__);
			goto out;
		}
		ndev_allocated++;
	}

	if (unlikely(is_scsi_rev_5(h)))
		raid_ctlr_position = 0;
	else
		raid_ctlr_position = nphysicals + nlogicals;

	/* adjust our table of devices */
	num_ext_target_devs = 0;
	for (i = 0; i < nphysicals + nlogicals + 1; i++) {
		u8 *lunaddrbytes;

		/* Figure out where the LUN ID info is coming from */
		lunaddrbytes = figure_lunaddrbytes(h, raid_ctlr_position,
			i, nphysicals, nlogicals, physdev_list, logdev_list);

		/* skip masked physical devices. */
		if (lunaddrbytes[3] & 0xC0 &&
			i < nphysicals + (raid_ctlr_position == 0)) {
			hpsa_dbmsg(h, 3, "hpsa_update_scsi_devices: "
				"Masked phys device %d: lunaddrbytes: "
				"0x%02x%02x%02x%02x %02x%02x%02x%02x\n",
				i,
				lunaddrbytes[0], lunaddrbytes[1],
				lunaddrbytes[2], lunaddrbytes[3],
				lunaddrbytes[4], lunaddrbytes[5],
				lunaddrbytes[6], lunaddrbytes[7]);
			continue;
		}

		/* Get device type, vendor, model, device id */
		if (hpsa_update_device_info(h, lunaddrbytes, tmpdevice)) {
			hpsa_dbmsg(h, 3, "hpsa_update_scsi_devices: "
				"No info, skipping device %d: lunaddrbytes: "
				"0x%02x%02x%02x%02x %02x%02x%02x%02x\n",
				i,
				lunaddrbytes[0], lunaddrbytes[1],
				lunaddrbytes[2], lunaddrbytes[3],
				lunaddrbytes[4], lunaddrbytes[5],
				lunaddrbytes[6], lunaddrbytes[7]);
			continue; /* skip it if we can't talk to it. */
		}
		figure_bus_target_lun(h, lunaddrbytes, &bus, &target, &lun,
			tmpdevice);
		this_device = currentsd[ncurrent];

		/*
		 * For the external target devices, we have to insert a LUN 0 which
		 * doesn't show up in CCISS_REPORT_PHYSICAL data, but there
		 * is nonetheless an enclosure device there.  We have to
		 * present that otherwise linux won't find anything if
		 * there is no lun 0.
		 */
		if (add_ext_target_dev(h, tmpdevice, this_device,
				lunaddrbytes, bus, target, lun, lunzerobits,
				&num_ext_target_devs)) {
			ncurrent++;
			this_device = currentsd[ncurrent];
		}

		*this_device = *tmpdevice;
		hpsa_set_bus_target_lun(this_device, bus, target, lun);
		hpsa_db_dev("hpsa_update_scsi_devices", h, this_device, i);

		switch (this_device->devtype) {
		case TYPE_ROM: {
			/* We don't *really* support actual CD-ROM devices,
			 * just "One Button Disaster Recovery" tape drive
			 * which temporarily pretends to be a CD-ROM drive.
			 * So we check that the device is really an OBDR tape
			 * device by checking for "$DR-10" in bytes 43-48 of
			 * the inquiry data.
			 */
				char obdr_sig[7];
#define OBDR_TAPE_SIG "$DR-10"
				strncpy(obdr_sig, &inq_buff[43], 6);
				obdr_sig[6] = '\0';
				if (strncmp(obdr_sig, OBDR_TAPE_SIG, 6) != 0)
					/* Not OBDR device, ignore it. */
					break;
			}
			ncurrent++;
			break;
		case TYPE_DISK:
			if (i < nphysicals)
				break;
			hpsa_dbmsg(h, 3, "hpsa_update_scsi_devices: "
				"device %d: Found a logical disk.\n", i);
			ncurrent++;
			break;
		case TYPE_TAPE:
		case TYPE_MEDIUM_CHANGER:
			ncurrent++;
			break;
		case TYPE_RAID:
			/* Only present the Smartarray HBA as a RAID controller.
			 * If it's a RAID controller other than the HBA itself
			 * (an external RAID controller, MSA500 or similar)
			 * don't present it.
			 */
			hpsa_dbmsg(h, 3, "hpsa_update_scsi_devices: "
				"device %d: Found a RAID controller.\n", 
				i);
			if (!is_hba_lunid(lunaddrbytes)) {
				hpsa_dbmsg(h, 3, "hpsa_update_scsi_devices: "
				"device %d: Not adding remote RAID device.\n",
				 i);
				break;
			}
#if defined (__VMKLNX__)
			hpsa_dbmsg(h, 3, "hpsa_update_scsi_devices: "
				"device %d: Not adding device for SmartArray controller.\n",
				 i);
#else  /*NOT defined (__VMKLNX__) */
			ncurrent++;
			hpsa_dbmsg(h, 3, "hpsa_update_scsi_devices: "
				"device %d: Found a SmartArray controller.\n",
				 i);
#endif
			break;
		default:
			break;
		}
		if (ncurrent >= HPSA_MAX_DEVICES)
			break;
	}
	adjust_hpsa_scsi_table(h, hostno, currentsd, ncurrent);
out:
	kfree(tmpdevice);
	for (i = 0; i < ndev_allocated; i++)
		kfree(currentsd[i]);
	kfree(currentsd);
	kfree(inq_buff);
	kfree(physdev_list);
	kfree(logdev_list);
        kfree(extended_phys_list); 
        kfree(sense_subsystem_info); 
}

/* hpsa_scatter_gather takes a struct scsi_cmnd, (cmd), and does the pci
 * dma mapping  and fills in the scatter gather entries of the
 * hpsa command, cp.
 */
static int hpsa_scatter_gather(struct ctlr_info *h,
		struct CommandList *cp,
		struct scsi_cmnd *cmd)
{
	unsigned int len;
	struct scatterlist *sg;
	u64 addr64;
	int use_sg, i, sg_index, chained;
	struct SGDescriptor *curr_sg;

	BUG_ON(scsi_sg_count(cmd) > h->maxsgentries);

	use_sg = scsi_dma_map(cmd);
	if (use_sg < 0)
		return use_sg;

	if (!use_sg) {
		if (cmd->request_bufflen > 0) {
			hpsa_dbmsg(h, 5, "hpsa_scatter_gather: single address\n");
		//	struct device *pdev =  cmd->device->host->shost_gendev.parent;
			addr64 = (__u64) pci_map_single(h->pdev,
				cmd->request_buffer,
				cmd->request_bufflen,
				cmd->sc_data_direction);
			cp->SG[0].Addr.lower = 
				(__u32) (addr64 & 0x00000000FFFFFFFF);
			cp->SG[0].Addr.upper =
				(__u32) ((addr64 >> 32) & 0x00000000FFFFFFFF);
			cp->SG[0].Len = cmd->request_bufflen;
			use_sg=1;
		} 
		goto sglist_finished;
	}

	curr_sg = cp->SG;
	chained = 0;
	sg_index = 0;
	scsi_for_each_sg(cmd, sg, use_sg, i) {
		if (i == h->max_cmd_sg_entries - 1 &&
			use_sg > h->max_cmd_sg_entries) {
			chained = 1;
			curr_sg = h->cmd_sg_list[cp->cmdindex];
			sg_index = 0;
		}
		addr64 = (u64) sg_dma_address(sg);
		len  = sg_dma_len(sg);
		curr_sg->Addr.lower = (u32) (addr64 & 0x0FFFFFFFFULL);
		curr_sg->Addr.upper = (u32) ((addr64 >> 32) & 0x0FFFFFFFFULL);
		curr_sg->Len = len;
		curr_sg->Ext = 0;  /* we are not chaining */
		curr_sg++;
	}

	if (use_sg + chained > h->maxSG)
		h->maxSG = use_sg + chained;

	if (chained) {
		cp->Header.SGList = h->max_cmd_sg_entries;
		cp->Header.SGTotal = (u16) (use_sg + 1);
		hpsa_map_sg_chain_block(h, cp);
		return 0;
	}

sglist_finished:

	cp->Header.SGList = (u8) use_sg;   /* no. SGs contig in this cmd */
	cp->Header.SGTotal = (u16) use_sg; /* total sgs in this cmd list */
	return 0;
}


static int hpsa_scsi_queue_command(struct scsi_cmnd *cmd,
	void (*done)(struct scsi_cmnd *))
{
	struct ctlr_info *h;
	struct hpsa_scsi_dev_t *dev;
	unsigned char scsi3addr[8];
	struct CommandList *c;
	unsigned long flags;

	/* Get the ptr to our adapter structure out of cmd->host. */
	h = sdev_to_hba(cmd->device);
	dev = cmd->device->hostdata;
	if (!dev) {
		cmd->result = DID_NO_CONNECT << 16;
		hpsa_dbmsg(h, 2, "queue_command: device address "
			"lookup link missing for C%d:B%d:T%d:L%d.\n",
			h->scsi_host->host_no, cmd->device->channel, 
			cmd->device->id, cmd->device->lun);
		done(cmd);
		return 0;
	}

	memcpy(scsi3addr, dev->scsi3addr, sizeof(scsi3addr));

	/* investigate Commands */
	if(h->debug_msg >= 5) {
		hpsa_db_sdev("hpsa_scsi_queue_command: scsi_device", h, cmd->device);
		hpsa_db_dev("hpsa_scsi_queue_command: hpsa device", h, dev, -1);
		hpsa_db_scmnd("hpsa_scsi_queue_command: command", h, cmd);
	}

	/* Need a lock as this is being allocated from the pool */
	spin_lock_irqsave(&h->lock, flags);
	c = cmd_alloc(h);
	spin_unlock_irqrestore(&h->lock, flags);
	if (c == NULL) {			/* trouble... */
		dev_err(&h->pdev->dev, "cmd_alloc returned NULL!\n");
		return SCSI_MLQUEUE_HOST_BUSY;
	}

	/* Fill in the command list header */

	cmd->scsi_done = done;    /* save this for use by completion code */

	/* save c in case we have to abort it  */
	cmd->host_scribble = (unsigned char *) c;

	c->cmd_type = CMD_SCSI;
	c->scsi_cmd = cmd;
	c->Header.ReplyQueue = 0;  /* unused in simple mode */
	memcpy(&c->Header.LUN.LunAddrBytes[0], &scsi3addr[0], 8);
	c->Header.Tag.lower = (c->cmdindex << DIRECT_LOOKUP_SHIFT);
	c->Header.Tag.lower |= DIRECT_LOOKUP_BIT;

	/* Fill in the request block... */

	c->Request.Timeout = 0;
	memset(c->Request.CDB, 0, sizeof(c->Request.CDB));
	BUG_ON(cmd->cmd_len > sizeof(c->Request.CDB));
	c->Request.CDBLen = cmd->cmd_len;
	memcpy(c->Request.CDB, cmd->cmnd, cmd->cmd_len);
	c->Request.Type.Type = TYPE_CMD;
	c->Request.Type.Attribute = ATTR_SIMPLE;
	switch (cmd->sc_data_direction) {
	case DMA_TO_DEVICE:
		c->Request.Type.Direction = XFER_WRITE;
		break;
	case DMA_FROM_DEVICE:
		c->Request.Type.Direction = XFER_READ;
		break;
	case DMA_NONE:
		c->Request.Type.Direction = XFER_NONE;
		break;
	case DMA_BIDIRECTIONAL:
		/* This can happen if a buggy application does a scsi passthru
		 * and sets both inlen and outlen to non-zero. ( see
		 * ../scsi/scsi_ioctl.c:scsi_ioctl_send_command() )
		 */

		c->Request.Type.Direction = XFER_RSVD;
		/* This is technically wrong, and hpsa controllers should
		 * reject it with CMD_INVALID, which is the most correct
		 * response, but non-fibre backends appear to let it
		 * slide by, and give the same results as if this field
		 * were set correctly.  Either way is acceptable for
		 * our purposes here.
		 */

		break;

	default:
		dev_err(&h->pdev->dev, "unknown data direction: %d\n",
			cmd->sc_data_direction);
		BUG();
		break;
	}

	if (hpsa_scatter_gather(h, c, cmd) < 0) { /* Fill SG list */
		cmd_free(h, c);
		return SCSI_MLQUEUE_HOST_BUSY;
	}
	enqueue_cmd_and_start_io(h, c);
	/* the cmd'll come back via intr handler in complete_scsi_command()  */
	return 0;
}

static void hpsa_scan_start(struct Scsi_Host *sh)
{
	struct ctlr_info *h = shost_to_hba(sh);
	unsigned long flags;

	/* wait until any scan already in progress is finished. */
	while (1) {
		spin_lock_irqsave(&h->scan_lock, flags);
		if (h->scan_finished)
			break;
		spin_unlock_irqrestore(&h->scan_lock, flags);
		wait_event(h->scan_wait_queue, h->scan_finished);
		/* Note: We don't need to worry about a race between this
		 * thread and driver unload because the midlayer will
		 * have incremented the reference count, so unload won't
		 * happen if we're in here.
		 */
	}
	h->scan_finished = 0; /* mark scan as in progress */
	spin_unlock_irqrestore(&h->scan_lock, flags);
	hpsa_dbmsg(h, 3, "hpsa_scan_start\n");	
	hpsa_update_scsi_devices(h, h->scsi_host->host_no);

	spin_lock_irqsave(&h->scan_lock, flags);
	h->scan_finished = 1; /* mark scan as finished. */
	wake_up_all(&h->scan_wait_queue);
	spin_unlock_irqrestore(&h->scan_lock, flags);
}

static int hpsa_scan_finished(struct Scsi_Host *sh,
	unsigned long elapsed_time)
{
	struct ctlr_info *h = shost_to_hba(sh);
	unsigned long flags;
	int finished;

	spin_lock_irqsave(&h->scan_lock, flags);
	finished = h->scan_finished;
	spin_unlock_irqrestore(&h->scan_lock, flags);
	return finished;
}

#if defined (__VMKLNX__)
static int hpsa_change_queue_depth(struct scsi_device *sdev,
	int qdepth)
{
	struct ctlr_info *h = sdev_to_hba(sdev);

	if (qdepth < 1)
		qdepth = 1;
	else
		if (qdepth > h->nr_cmds)
			qdepth = h->nr_cmds;
	scsi_adjust_queue_depth(sdev, scsi_get_tag_type(sdev), qdepth);
	return sdev->queue_depth;
}
#else /* NOT defined (__VMKLNX__) */
static int hpsa_change_queue_depth(struct scsi_device *sdev,
	int qdepth, int reason)
{
	struct ctlr_info *h = sdev_to_hba(sdev);

	if (reason != SCSI_QDEPTH_DEFAULT)
		return -ENOTSUPP;

	if (qdepth < 1)
		qdepth = 1;
	else
		if (qdepth > h->nr_cmds)
			qdepth = h->nr_cmds;
	scsi_adjust_queue_depth(sdev, scsi_get_tag_type(sdev), qdepth);
	return sdev->queue_depth;
}
#endif 

static void hpsa_unregister_scsi(struct ctlr_info *h)
{
	/* we are being forcibly unloaded, and may not refuse. */
	scsi_remove_host(h->scsi_host);
	scsi_host_put(h->scsi_host);
	h->scsi_host = NULL;
}

static int hpsa_register_scsi(struct ctlr_info *h)
{
	int rc;

	rc = hpsa_scsi_detect(h);
	if (rc != 0)
		dev_err(&h->pdev->dev, "hpsa_register_scsi: failed"
			" hpsa_scsi_detect(), rc is %d\n", rc);
	return rc;
}

static int wait_for_device_to_become_ready(struct ctlr_info *h,
	unsigned char lunaddr[])
{
	int rc = 0;
	int count = 0;
	int waittime = 1; /* seconds */
	struct CommandList *c;

	c = cmd_special_alloc(h);
	if (!c) {
		dev_warn(&h->pdev->dev, "out of memory in "
			"wait_for_device_to_become_ready.\n");
		return IO_ERROR;
	}

	/* Send test unit ready until device ready, or give up. */
	while (count < HPSA_TUR_RETRY_LIMIT) {

		/* Wait for a bit.  do this first, because if we send
		 * the TUR right away, the reset will just abort it.
		 */
		msleep(1000 * waittime);
		count++;

		/* Increase wait time with each try, up to a point. */
		if (waittime < HPSA_MAX_WAIT_INTERVAL_SECS)
			waittime = waittime * 2;

		/* Send the Test Unit Ready */
		fill_cmd(c, TEST_UNIT_READY, h, NULL, 0, 0, lunaddr, TYPE_CMD);
		hpsa_scsi_do_simple_cmd_core(h, c);
		/* no unmap needed here because no data xfer. */

		if (c->err_info->CommandStatus == CMD_SUCCESS)
			break;

		if (c->err_info->CommandStatus == CMD_TARGET_STATUS &&
			c->err_info->ScsiStatus == SAM_STAT_CHECK_CONDITION &&
			(c->err_info->SenseInfo[2] == NO_SENSE ||
			c->err_info->SenseInfo[2] == UNIT_ATTENTION))
			break;

		dev_warn(&h->pdev->dev, "waiting %d secs "
			"for device to become ready.\n", waittime);
		rc = 1; /* device not ready. */
	}

	if (rc)
		dev_warn(&h->pdev->dev, "giving up on device.\n");
	else
		dev_warn(&h->pdev->dev, "device is ready.\n");

	cmd_special_free(h, c);
	return rc;
}

/* Need at least one of these error handlers to keep ../scsi/hosts.c from
 * complaining.  Doing a host- or bus-reset can't do anything good here.
 */
static int hpsa_eh_device_reset_handler(struct scsi_cmnd *scsicmd)
{
	int rc;
	struct ctlr_info *h;
	struct hpsa_scsi_dev_t *dev;
	int reset_type;

	/* find the controller to which the command to be aborted was sent */
	h = sdev_to_hba(scsicmd->device);
	if (h == NULL) /* paranoia */
		return FAILED;
	dev = scsicmd->device->hostdata;
	if (!dev) {
		dev_err(&h->pdev->dev, "hpsa_eh_device_reset_handler: "
			"device lookup failed.\n");
		return FAILED;
	}


	if (is_logical_dev_addr_mode(dev->scsi3addr)) {
		/* For logicals, use logical unit reset */
		reset_type = HPSA_DEVICE_RESET_MSG;
		hpsa_dbmsg(h, 2, "Perform logical unit reset on "
			"logical device C%d:B%d:T%d:L%d.\n",
			h->scsi_host->host_no, 
			dev->bus, dev->target, dev->lun);
	}
	else {
		/* For physicals, use target reset */
		reset_type = HPSA_TARGET_RESET_MSG;
		hpsa_dbmsg(h, 2, "Perform target reset on "
			"physical device C%d:B%d:T%d:L%d.\n",
			h->scsi_host->host_no, 
			dev->bus, dev->target, dev->lun);
	}

	dev_warn(&h->pdev->dev, "resetting device %d:%d:%d:%d\n",
		h->scsi_host->host_no, dev->bus, dev->target, dev->lun);
	/* send a reset to the SCSI LUN which the command was sent to */
	rc = hpsa_send_reset(h, dev->scsi3addr, reset_type);
	if (rc == 0 && wait_for_device_to_become_ready(h, dev->scsi3addr) == 0)
		return SUCCESS;

	dev_warn(&h->pdev->dev, "resetting device failed.\n");
	return FAILED;
}

/* Send a reset to the SCSI Bus of the logical device.
 */
static int hpsa_eh_bus_reset_handler(struct scsi_cmnd *scsicmd)
{
	int rc;
	struct ctlr_info *h;
	struct hpsa_scsi_dev_t *dev;

	/* find the controller to which the command to be aborted was sent */
	h = sdev_to_hba(scsicmd->device);
	if (h == NULL) /* paranoia */
		return FAILED;
	dev = scsicmd->device->hostdata;
	if (!dev) {
		dev_err(&h->pdev->dev, "hpsa_eh_bus_reset_handler: "
			"device lookup failed.\n");
		return FAILED;
	}
	dev_warn(&h->pdev->dev, "Bus reset on device hpsa:%d:B%d:T%d:L%d\n",
		h->scsi_host->host_no, dev->bus, dev->target, dev->lun);

	rc = hpsa_send_reset(h, dev->scsi3addr, HPSA_BUS_RESET_MSG);
	if (rc == 0 && wait_for_device_to_become_ready(h, dev->scsi3addr) == 0)
		return SUCCESS;

	dev_warn(&h->pdev->dev, "FAILED to reset bus of device hpsa:%d:B%d:T%d:L%d\n",
		h->scsi_host->host_no, dev->bus, dev->target, dev->lun);
	return FAILED;
}

/* Send an abort for the specified command.
 *	If the device and controller support it,
 *		send a 'real' task abort request.
 * 	Otherwise, send a device reset, after waiting
 * 		an appropriate delay, since controller
 *		firmware may have already handled the
 *		abort internally. Check after delaying
 *		to make sure abort is still necessary.
 */
static int hpsa_eh_abort_handler(struct scsi_cmnd *sc)
{
#define ABORT_COMPLETE_WAIT 3	/* If aborted command does not complete
				 * within this time after successful
				 * abort, we fail the abort.
				 */
#define ABORT_WAIT_TIME 30	/* pre-abort delay for emulated aborts. */
	int rc;
	struct ctlr_info *h;
	struct hpsa_scsi_dev_t *dev;
        int wait_iter;		/* Count waits for aborted cmd to complete */
        int emulated=1;		/* Assume we do emulated aborts (resets) */
	int found;		
	struct CommandList *abort; /* pointer to command to be aborted */
	struct scsi_cmnd *as;	/* ptr to scsi cmd inside aborted command. */
	char msg[256];		/* For debug messaging. */		
	int ml=0;

	memset(msg, 0, sizeof(msg));


	/* Find the controller of the command to be aborted */
	h = sdev_to_hba(sc->device);
	if (h == NULL) { /* paranoia */
		dev_err(&h->pdev->dev, "ABORT REQUEST FAILED, "
			"Controller lookup failed.\n");
		return FAILED;
	}

	ml += sprintf(msg+ml, "ABORT REQUEST on C%d:B%d:T%d:L%d ",
		h->scsi_host->host_no, sc->device->channel, 
		sc->device->id, sc->device->lun);

	/* Find the device of the command to be aborted */
	dev = sc->device->hostdata;
	if (!dev) {
		dev_err(&h->pdev->dev, "%s FAILED, "
			"Device lookup failed.\n", msg);
		return FAILED;
	}


	/* Get command to be aborted */
	abort = (struct CommandList *) sc->host_scribble;
	if (abort == NULL)
	{
		dev_err(&h->pdev->dev, "%s FAILED, "
			"Command to abort is NULL.\n", msg);
		return FAILED;
	}

	/* Document the abort request */
	ml += sprintf(msg+ml, "Tag:0x%08x:%08x ",
		abort->Header.Tag.upper, abort->Header.Tag.lower);
	as  = (struct scsi_cmnd *) abort->scsi_cmd;
	if (as != NULL ) {
		ml += sprintf(msg+ml, "Command:0x%x SN:0x%lx ",
			as->cmnd[0], as->serial_number);
	}
	hpsa_dbmsg(h, 1, "%s\n", msg);
	dev_warn(&h->pdev->dev, "Abort request on C%d:B%d:T%d:L%d\n",
		h->scsi_host->host_no, dev->bus, dev->target, dev->lun);


	/* Determine Abort type based on controller abilities
	 * and LUN type (has to be remote logical)
	 */
	if (    (h->TMFSupportFlags & HPSATMF_BITS_SUPPORTED)
		&&
		(h->TMFSupportFlags & HPSATMF_PHYS_TASK_ABORT)
		&&
		(is_ext_target(h, dev))
	) 
		emulated=0; /* not emulated, do real */



	/* Make sure the command is still around. */
	found = hpsa_cmd_waiting(h, sc);
	if (!found) {
		hpsa_dbmsg(h, 1, "%s FAILED. (Abort not needed, "
			"Command already returned)\n", msg);
		return FAILED;
	}

	/* For emulated aborts (resets):
	 * Delay a while;  maybe the controller
	 * firmware is already aborting this for us.
	 */
	if (emulated) {
		hpsa_dbmsg(h, 1, "%s ABORT via reset: "
			"Delaying request up to %d seconds.\n",
			msg, ABORT_WAIT_TIME);

		wait_iter = ABORT_WAIT_TIME;
		while (found) {
			msleep(1000);
			found = hpsa_cmd_waiting(h, sc);
			if (--wait_iter == 0)
				break;
		}

		/* Did command complete during delay? */
		if (!found) {
			dev_err(&h->pdev->dev, "%s (via RESET) FAILED, "
				"Command completed before reset was attempted "
				"after delaying %d seconds.\n",
				msg, ABORT_WAIT_TIME - wait_iter);
			return FAILED;	
			/* Is it right to return failed?
			 * The abort wasn't needed, since the 
			 * command was already aborted (by firmware).
			 */
		}
		/* Command still around, need to do reset. */
		hpsa_dbmsg(h, 1, "%s Command has not returned after delay. "
			"Proceeding with abort (via reset)\n", msg);
		rc = hpsa_send_reset(h, dev->scsi3addr, HPSA_DEVICE_RESET_MSG );
			
	}
	else {
		/* send a REAL abort, not a reset */
		hpsa_dbmsg(h, 1, "%s Using Task Abort\n", msg);
		rc = hpsa_send_abort(h, dev->scsi3addr, abort);
	}

	/* Did abort or reset request suceed? */
	if (rc != 0 ) {
		hpsa_dbmsg(h, 1, "%s Request FAILED.\n", msg);
		dev_warn(&h->pdev->dev, "FAILED abort on device "
			"C%d:B%d:T%d:L%d\n",
			h->scsi_host->host_no, 
			dev->bus, dev->target, dev->lun);
		return FAILED;
	}


	hpsa_dbmsg(h, 1, "%s REQUEST SUCCEEDED.\n", msg);

	
	/* Did device recover? */
	if (emulated) {
		hpsa_dbmsg(h, 1, "%s WAIT for device to recover.\n", msg);
		if ( wait_for_device_to_become_ready(h, dev->scsi3addr) != 0) {
			hpsa_dbmsg(h, 1, "%s FAILED. Device not ready.\n", msg);
			dev_warn(&h->pdev->dev, "FAILED abort on device "
				"C%d:B%d:T%d:L%d. Device not ready.\n",
				h->scsi_host->host_no, dev->bus, 
				dev->target, dev->lun);
			return FAILED;
		}
	}


	/* wait for the completion of the aborted command. */
	wait_iter = ABORT_COMPLETE_WAIT;
	found = hpsa_cmd_waiting(h, sc);
	if (found) {
		hpsa_dbmsg(h, 1, "%s WAITING " 
			"for aborted command to complete.\n", msg);
		while (found) {
			msleep(1000);
			found = hpsa_cmd_waiting(h, sc);
			if (--wait_iter == 0)
				break;
		}
		/* If cmd is still around, abort has failed to kill it. */
		if (found) {
			dev_warn(&h->pdev->dev, "%s FAILED. Aborted command "
				"has not completed after %d seconds.\n", 
				msg, wait_iter);
			return FAILED;	
		}
		ml += sprintf(msg+ml, "Aborted command completed "
			"after %d seconds. ",
			ABORT_COMPLETE_WAIT - wait_iter);
	}
	
	hpsa_dbmsg(h, 1, "%s ABORT COMPLETE.\n", msg);
	return SUCCESS;
}

/*
 * For operations that cannot sleep, a command block is allocated at init,
 * and managed by cmd_alloc() and cmd_free() using a simple bitmap to track
 * which ones are free or in use.  Lock must be held when calling this.
 * cmd_free() is the complement.
 */
static struct CommandList *cmd_alloc(struct ctlr_info *h)
{
	struct CommandList *c;
	int i;
	union u64bit temp64;
	dma_addr_t cmd_dma_handle, err_dma_handle;

	do {
		i = find_first_zero_bit(h->cmd_pool_bits, h->nr_cmds);
		if (i == h->nr_cmds)
			return NULL;
	} while (test_and_set_bit
		 (i & (BITS_PER_LONG - 1),
		  h->cmd_pool_bits + (i / BITS_PER_LONG)) != 0);
	c = h->cmd_pool + i;
	memset(c, 0, sizeof(*c));
	cmd_dma_handle = h->cmd_pool_dhandle
	    + i * sizeof(*c);
	c->err_info = h->errinfo_pool + i;
	memset(c->err_info, 0, sizeof(*c->err_info));
	err_dma_handle = h->errinfo_pool_dhandle
	    + i * sizeof(*c->err_info);
	h->nr_allocs++;

	c->cmdindex = i;

	INIT_LIST_HEAD(&c->list);
	c->busaddr = (u32) cmd_dma_handle;
	temp64.val = (u64) err_dma_handle;
	c->ErrDesc.Addr.lower = temp64.val32.lower;
	c->ErrDesc.Addr.upper = temp64.val32.upper;
	c->ErrDesc.Len = sizeof(*c->err_info);

	c->h = h;
	return c;
}

/* For operations that can wait for kmalloc to possibly sleep,
 * this routine can be called. Lock need not be held to call
 * cmd_special_alloc. cmd_special_free() is the complement.
 */
static struct CommandList *cmd_special_alloc(struct ctlr_info *h)
{
	struct CommandList *c;
	union u64bit temp64;
	dma_addr_t cmd_dma_handle, err_dma_handle;

	c = pci_alloc_consistent(h->pdev, sizeof(*c), &cmd_dma_handle);
	if (c == NULL)
		return NULL;
	memset(c, 0, sizeof(*c));

	c->cmdindex = -1;

	c->err_info = pci_alloc_consistent(h->pdev, sizeof(*c->err_info),
		    &err_dma_handle);

	if (c->err_info == NULL) {
		pci_free_consistent(h->pdev,
			sizeof(*c), c, cmd_dma_handle);
		return NULL;
	}
	memset(c->err_info, 0, sizeof(*c->err_info));

	INIT_LIST_HEAD(&c->list);
	c->busaddr = (u32) cmd_dma_handle;
	temp64.val = (u64) err_dma_handle;
	c->ErrDesc.Addr.lower = temp64.val32.lower;
	c->ErrDesc.Addr.upper = temp64.val32.upper;
	c->ErrDesc.Len = sizeof(*c->err_info);

	c->h = h;
	return c;
}

static void cmd_free(struct ctlr_info *h, struct CommandList *c)
{
	int i;

	i = c - h->cmd_pool;
	clear_bit(i & (BITS_PER_LONG - 1),
		  h->cmd_pool_bits + (i / BITS_PER_LONG));
	h->nr_frees++;
}

static void cmd_special_free(struct ctlr_info *h, struct CommandList *c)
{
	union u64bit temp64;

	temp64.val32.lower = c->ErrDesc.Addr.lower;
	temp64.val32.upper = c->ErrDesc.Addr.upper;
	pci_free_consistent(h->pdev, sizeof(*c->err_info),
			    c->err_info, (dma_addr_t) temp64.val);
	/* clear direct lookup and performant mode bits 
	 * of busaddr (restore address)
	 * to prevent VMware assert failure.
	 */
#define CLEAR_PERFORMANT_BITS(n) (n & ~((1 << DIRECT_LOOKUP_SHIFT)-1))
	pci_free_consistent(h->pdev, sizeof(*c),
			    c, (dma_addr_t) CLEAR_PERFORMANT_BITS(c->busaddr));
}

#ifdef CONFIG_COMPAT

static int hpsa_ioctl32_passthru(struct scsi_device *dev, int cmd, void *arg)
{
	IOCTL32_Command_struct __user *arg32 =
	    (IOCTL32_Command_struct __user *) arg;
	IOCTL_Command_struct arg64;
	IOCTL_Command_struct __user *p = compat_alloc_user_space(sizeof(arg64));
	int err;
	u32 cp;

	err = 0;
	err |= copy_from_user(&arg64.LUN_info, &arg32->LUN_info,
			   sizeof(arg64.LUN_info));
	err |= copy_from_user(&arg64.Request, &arg32->Request,
			   sizeof(arg64.Request));
	err |= copy_from_user(&arg64.error_info, &arg32->error_info,
			   sizeof(arg64.error_info));
	err |= get_user(arg64.buf_size, &arg32->buf_size);
	err |= get_user(cp, &arg32->buf);
	arg64.buf = compat_ptr(cp);
	err |= copy_to_user(p, &arg64, sizeof(arg64));

	if (err)
		return -EFAULT;

	err = hpsa_ioctl(dev, CCISS_PASSTHRU, (void *)p);
	if (err)
		return err;
	err |= copy_in_user(&arg32->error_info, &p->error_info,
			 sizeof(arg32->error_info));
	if (err)
		return -EFAULT;
	return err;
}

static int hpsa_ioctl32_big_passthru(struct scsi_device *dev,
	int cmd, void *arg)
{
	BIG_IOCTL32_Command_struct __user *arg32 =
	    (BIG_IOCTL32_Command_struct __user *) arg;
	BIG_IOCTL_Command_struct arg64;
	BIG_IOCTL_Command_struct __user *p =
	    compat_alloc_user_space(sizeof(arg64));
	int err;
	u32 cp;

	err = 0;
	err |= copy_from_user(&arg64.LUN_info, &arg32->LUN_info,
			   sizeof(arg64.LUN_info));
	err |= copy_from_user(&arg64.Request, &arg32->Request,
			   sizeof(arg64.Request));
	err |= copy_from_user(&arg64.error_info, &arg32->error_info,
			   sizeof(arg64.error_info));
	err |= get_user(arg64.buf_size, &arg32->buf_size);
	err |= get_user(arg64.malloc_size, &arg32->malloc_size);
	err |= get_user(cp, &arg32->buf);
	arg64.buf = compat_ptr(cp);
	err |= copy_to_user(p, &arg64, sizeof(arg64));

	if (err)
		return -EFAULT;

	err = hpsa_ioctl(dev, CCISS_BIG_PASSTHRU, (void *)p);
	if (err)
		return err;
	err |= copy_in_user(&arg32->error_info, &p->error_info,
			 sizeof(arg32->error_info));
	if (err)
		return -EFAULT;
	return err;
}

static int hpsa_compat_ioctl(struct scsi_device *dev, int cmd, void *arg)
{
	switch (cmd) {
	case CCISS_GETPCIINFO:
	case CCISS_GETINTINFO:
	case CCISS_SETINTINFO:
	case CCISS_GETNODENAME:
	case CCISS_SETNODENAME:
	case CCISS_GETHEARTBEAT:
	case CCISS_GETBUSTYPES:
	case CCISS_GETFIRMVER:
	case CCISS_GETDRIVVER:
	case CCISS_REVALIDVOLS:
	case CCISS_DEREGDISK:
	case CCISS_REGNEWDISK:
	case CCISS_REGNEWD:
	case CCISS_RESCANDISK:
	case CCISS_GETLUNINFO:
		return hpsa_ioctl(dev, cmd, arg);

	case CCISS_PASSTHRU32:
		return hpsa_ioctl32_passthru(dev, cmd, arg);
	case CCISS_BIG_PASSTHRU32:
		return hpsa_ioctl32_big_passthru(dev, cmd, arg);

	default:
		return -ENOIOCTLCMD;
	}
}
#endif

static int hpsa_getpciinfo_ioctl(struct ctlr_info *h, void __user *argp)
{
	struct hpsa_pci_info pciinfo;

	if (!argp)
		return -EINVAL;
	pciinfo.domain = pci_domain_nr(h->pdev->bus);
	pciinfo.bus = h->pdev->bus->number;
	pciinfo.dev_fn = h->pdev->devfn;
	pciinfo.board_id = h->board_id;
	if (copy_to_user(argp, &pciinfo, sizeof(pciinfo)))
		return -EFAULT;
	return 0;
}

static int hpsa_getdrivver_ioctl(struct ctlr_info *h, void __user *argp)
{
	DriverVer_type DriverVer;
	unsigned char vmaj, vmin, vsubmin;
	int rc;

	rc = sscanf(HPSA_DRIVER_VERSION, "%hhu.%hhu.%hhu",
		&vmaj, &vmin, &vsubmin);
	if (rc != 3) {
		dev_info(&h->pdev->dev, "driver version string '%s' "
			"unrecognized.", HPSA_DRIVER_VERSION);
		vmaj = 0;
		vmin = 0;
		vsubmin = 0;
	}
	DriverVer = (vmaj << 16) | (vmin << 8) | vsubmin;
	if (!argp)
		return -EINVAL;
	if (copy_to_user(argp, &DriverVer, sizeof(DriverVer_type)))
		return -EFAULT;
	return 0;
}

static int hpsa_passthru_ioctl(struct ctlr_info *h, void __user *argp)
{
	IOCTL_Command_struct iocommand;
	struct CommandList *c;
	char *buff = NULL;
	union u64bit temp64;

	hpsa_dbmsg(h, 5, "hpsa_passthru_ioctl: Begin\n");
	if (!argp)
		return -EINVAL;
	if (!capable(CAP_SYS_RAWIO))
		return -EPERM;
	if (copy_from_user(&iocommand, argp, sizeof(iocommand)))
		return -EFAULT;
	if ((iocommand.buf_size < 1) &&
	    (iocommand.Request.Type.Direction != XFER_NONE)) {
		return -EINVAL;
	}
	if (iocommand.buf_size > 0) {
		buff = kmalloc(iocommand.buf_size, GFP_KERNEL);
		if (buff == NULL)
			return -EFAULT;
	}
	if (iocommand.Request.Type.Direction == XFER_WRITE) {
		/* Copy the data into the buffer we created */
		if (copy_from_user(buff, iocommand.buf, iocommand.buf_size)) {
			kfree(buff);
			return -EFAULT;
		}
	} else
		memset(buff, 0, iocommand.buf_size);
	c = cmd_special_alloc(h);
	if (c == NULL) {
		kfree(buff);
		return -ENOMEM;
	}
	/* Fill in the command type */
	c->cmd_type = CMD_IOCTL_PEND;
	/* Fill in Command Header */
	c->Header.ReplyQueue = 0; /* unused in simple mode */
	if (iocommand.buf_size > 0) {	/* buffer to fill */
		c->Header.SGList = 1;
		c->Header.SGTotal = 1;
	} else	{ /* no buffers to fill */
		c->Header.SGList = 0;
		c->Header.SGTotal = 0;
	}
	memcpy(&c->Header.LUN, &iocommand.LUN_info, sizeof(c->Header.LUN));
	hpsa_dbmsg(h, 5, "hpsa_passthru_ioctl: "
		"bufsize: %d bytes "
		"lunaddrbytes: 0x%02x%02x%02x%02x %02x%02x%02x%02x\n",
		iocommand.buf_size,	
		c->Header.LUN.LunAddrBytes[0], c->Header.LUN.LunAddrBytes[1],
		c->Header.LUN.LunAddrBytes[2], c->Header.LUN.LunAddrBytes[3],
		c->Header.LUN.LunAddrBytes[4], c->Header.LUN.LunAddrBytes[5],
		c->Header.LUN.LunAddrBytes[6], c->Header.LUN.LunAddrBytes[7]);

	
	/* use the kernel address the cmd block for tag */
	c->Header.Tag.lower = c->busaddr;

	/* Fill in Request block */
	memcpy(&c->Request, &iocommand.Request,
		sizeof(c->Request));

	/* Fill in the scatter gather information */
	if (iocommand.buf_size > 0) {
		temp64.val = pci_map_single(h->pdev, buff,
			iocommand.buf_size, PCI_DMA_BIDIRECTIONAL);
		c->SG[0].Addr.lower = temp64.val32.lower;
		c->SG[0].Addr.upper = temp64.val32.upper;
		c->SG[0].Len = iocommand.buf_size;
		c->SG[0].Ext = 0; /* we are not chaining*/
	}
	hpsa_scsi_do_simple_cmd_core(h, c);
	hpsa_pci_unmap(h->pdev, c, 1, PCI_DMA_BIDIRECTIONAL);
	check_ioctl_unit_attention(h, c);

	/* Copy the error information out */
	memcpy(&iocommand.error_info, c->err_info,
		sizeof(iocommand.error_info));
	if (copy_to_user(argp, &iocommand, sizeof(iocommand))) {
		kfree(buff);
		cmd_special_free(h, c);
		return -EFAULT;
	}

	if (iocommand.Request.Type.Direction == XFER_READ) {
		/* Copy the data out of the buffer we created */
		if (copy_to_user(iocommand.buf, buff, iocommand.buf_size)) {
			kfree(buff);
			cmd_special_free(h, c);
			return -EFAULT;
		}
	}
	kfree(buff);
	cmd_special_free(h, c);
	return 0;
}

static int hpsa_big_passthru_ioctl(struct ctlr_info *h, void __user *argp)
{
	BIG_IOCTL_Command_struct *ioc;
	struct CommandList *c;
	unsigned char **buff = NULL;
	int *buff_size = NULL;
	union u64bit temp64;
	BYTE sg_used = 0;
	int status = 0;
	int i;
	u32 left;
	u32 sz;
	BYTE __user *data_ptr;

	if (!argp)
		return -EINVAL;
	if (!capable(CAP_SYS_RAWIO))
		return -EPERM;
	ioc = (BIG_IOCTL_Command_struct *)
	    kmalloc(sizeof(*ioc), GFP_KERNEL);
	hpsa_dbmsg(h, 5, "hpsa_big_passthru_ioctl: begin\n");
	if (!ioc) {
		status = -ENOMEM;
		goto cleanup1;
	}
	if (copy_from_user(ioc, argp, sizeof(*ioc))) {
		status = -EFAULT;
		goto cleanup1;
	}
	if ((ioc->buf_size < 1) &&
	    (ioc->Request.Type.Direction != XFER_NONE)) {
		status = -EINVAL;
		goto cleanup1;
	}
	/* Check kmalloc limits  using all SGs */
	if (ioc->malloc_size > MAX_KMALLOC_SIZE) {
		status = -EINVAL;
		goto cleanup1;
	}
	if (ioc->buf_size > ioc->malloc_size * MAXSGENTRIES) {
		status = -EINVAL;
		goto cleanup1;
	}
	buff = kzalloc(MAXSGENTRIES * sizeof(char *), GFP_KERNEL);
	if (!buff) {
		status = -ENOMEM;
		goto cleanup1;
	}
	buff_size = kmalloc(MAXSGENTRIES * sizeof(int), GFP_KERNEL);
	if (!buff_size) {
		status = -ENOMEM;
		goto cleanup1;
	}
	left = ioc->buf_size;
	data_ptr = ioc->buf;
	while (left) {
		sz = (left > ioc->malloc_size) ? ioc->malloc_size : left;
		buff_size[sg_used] = sz;
		buff[sg_used] = kmalloc(sz, GFP_KERNEL);
		if (buff[sg_used] == NULL) {
			status = -ENOMEM;
			goto cleanup1;
		}
		if (ioc->Request.Type.Direction == XFER_WRITE) {
			if (copy_from_user(buff[sg_used], data_ptr, sz)) {
				status = -ENOMEM;
				goto cleanup1;
			}
		} else
			memset(buff[sg_used], 0, sz);
		left -= sz;
		data_ptr += sz;
		sg_used++;
	}
	c = cmd_special_alloc(h);
	if (c == NULL) {
		status = -ENOMEM;
		goto cleanup1;
	}
	c->cmd_type = CMD_IOCTL_PEND;
	c->Header.ReplyQueue = 0;

	if (ioc->buf_size > 0) {
		c->Header.SGList = sg_used;
		c->Header.SGTotal = sg_used;
	} else {
		c->Header.SGList = 0;
		c->Header.SGTotal = 0;
	}
	memcpy(&c->Header.LUN, &ioc->LUN_info, sizeof(c->Header.LUN));
	c->Header.Tag.lower = c->busaddr;
	memcpy(&c->Request, &ioc->Request, sizeof(c->Request));
	if (ioc->buf_size > 0) {
		int i;
		for (i = 0; i < sg_used; i++) {
			temp64.val = pci_map_single(h->pdev, buff[i],
				    buff_size[i], PCI_DMA_BIDIRECTIONAL);
			c->SG[i].Addr.lower = temp64.val32.lower;
			c->SG[i].Addr.upper = temp64.val32.upper;
			c->SG[i].Len = buff_size[i];
			/* we are not chaining */
			c->SG[i].Ext = 0;
		}
	}
	hpsa_scsi_do_simple_cmd_core(h, c);
	hpsa_pci_unmap(h->pdev, c, sg_used, PCI_DMA_BIDIRECTIONAL);
	check_ioctl_unit_attention(h, c);
	/* Copy the error information out */
	memcpy(&ioc->error_info, c->err_info, sizeof(ioc->error_info));
	if (copy_to_user(argp, ioc, sizeof(*ioc))) {
		cmd_special_free(h, c);
		status = -EFAULT;
		goto cleanup1;
	}
	if (ioc->Request.Type.Direction == XFER_READ) {
		/* Copy the data out of the buffer we created */
		BYTE __user *ptr = ioc->buf;
		for (i = 0; i < sg_used; i++) {
			if (copy_to_user(ptr, buff[i], buff_size[i])) {
				cmd_special_free(h, c);
				status = -EFAULT;
				goto cleanup1;
			}
			ptr += buff_size[i];
		}
	}
	cmd_special_free(h, c);
	status = 0;
cleanup1:
	if (buff) {
		for (i = 0; i < sg_used; i++)
			kfree(buff[i]);
		kfree(buff);
	}
	kfree(buff_size);
	kfree(ioc);
	return status;
}

static void check_ioctl_unit_attention(struct ctlr_info *h,
	struct CommandList *c)
{
	if (c->err_info->CommandStatus == CMD_TARGET_STATUS &&
			c->err_info->ScsiStatus != SAM_STAT_CHECK_CONDITION)
		(void) check_for_unit_attention(h, c);
}
/*
 * ioctl
 */
static int hpsa_ioctl(struct scsi_device *dev, int cmd, void *arg)
{
	struct ctlr_info *h;
	void __user *argp = (void __user *)arg;

	h = sdev_to_hba(dev);

	hpsa_dbmsg(h, 5, "hpsa_ioctl: Begin\n");

	switch (cmd) {
	case CCISS_DEREGDISK:
	case CCISS_REGNEWDISK:
	case CCISS_REGNEWD:
		hpsa_scan_start(h->scsi_host);
		return 0;
	case CCISS_GETPCIINFO:
		return hpsa_getpciinfo_ioctl(h, argp);
	case CCISS_GETDRIVVER:
		return hpsa_getdrivver_ioctl(h, argp);
	case CCISS_PASSTHRU:
		return hpsa_passthru_ioctl(h, argp);
	case CCISS_BIG_PASSTHRU:
		return hpsa_big_passthru_ioctl(h, argp);
	default:
		return -ENOTTY;
	}
}

static void fill_cmd(struct CommandList *c, u8 cmd, struct ctlr_info *h,
	void *buff, size_t size, u8 page_code, unsigned char *scsi3addr,
	int cmd_type)
{
	int pci_dir = XFER_NONE;
	struct CommandList *a;	/* for commands to be aborted */

	c->cmd_type = CMD_IOCTL_PEND;
	c->Header.ReplyQueue = 0;
	if (buff != NULL && size > 0) {
		c->Header.SGList = 1;
		c->Header.SGTotal = 1;
	} else {
		c->Header.SGList = 0;
		c->Header.SGTotal = 0;
	}
	c->Header.Tag.lower = c->busaddr;
	memcpy(c->Header.LUN.LunAddrBytes, scsi3addr, 8);

	c->Request.Type.Type = cmd_type;
	if (cmd_type == TYPE_CMD) {
		switch (cmd) {
		case HPSA_INQUIRY:
			hpsa_dbmsg(h, 5, "fill_cmd: INQUIRY: scsi3addr: "
				"0x%02x%02x%02x%02x %02x%02x%02x%02x "
				"size: %lu pgcode: 0x%x\n",
				scsi3addr[0], scsi3addr[1],
				scsi3addr[2], scsi3addr[3],
				scsi3addr[4], scsi3addr[5],
				scsi3addr[6], scsi3addr[7],
				size, page_code);
			c->Request.CDBLen = 6;
			c->Request.Type.Attribute = ATTR_SIMPLE;
			c->Request.Type.Direction = XFER_READ;
			c->Request.Timeout = 0;
			c->Request.CDB[0] = HPSA_INQUIRY;
			c->Request.CDB[4] = size & 0xFF;
			break;
		case HPSA_VPD_INQUIRY:
			/* we are trying to read a vital product page */
			hpsa_dbmsg(h, 5, "fill_cmd: VPD INQUIRY: scsi3addr: "
				"0x%02x%02x%02x%02x %02x%02x%02x%02x "
				"size: %lu pgcode: 0x%x\n",
				scsi3addr[0], scsi3addr[1],
				scsi3addr[2], scsi3addr[3],
				scsi3addr[4], scsi3addr[5],
				scsi3addr[6], scsi3addr[7],
				size, page_code);
			c->Request.CDB[1] = 0x01; /* turn on the EVPD bit */
			c->Request.CDB[2] = page_code;
			c->Request.CDBLen = 6;
			c->Request.Type.Attribute = ATTR_SIMPLE;
			c->Request.Type.Direction = XFER_READ;
			c->Request.Timeout = 0;
			c->Request.CDB[0] = HPSA_INQUIRY;
			c->Request.CDB[4] = size & 0xFF;
			break;
		case HPSA_REPORT_LOG:
		case HPSA_REPORT_PHYS:
			/* Talking to controller so It's a physical command
			   mode = 00 target = 0.  Nothing to write.
			 */
			c->Request.CDBLen = 12;
			c->Request.Type.Attribute = ATTR_SIMPLE;
			c->Request.Type.Direction = XFER_READ;
			c->Request.Timeout = 0;
			c->Request.CDB[0] = cmd;
			c->Request.CDB[6] = (size >> 24) & 0xFF; /* MSB */
			c->Request.CDB[7] = (size >> 16) & 0xFF;
			c->Request.CDB[8] = (size >> 8) & 0xFF;
			c->Request.CDB[9] = size & 0xFF;
			break;

		case HPSA_SENSE_SUBSYSTEM_INFO:
			c->Request.CDBLen = 12;
			c->Request.Type.Attribute = ATTR_SIMPLE;
			c->Request.Type.Direction = XFER_READ;
			c->Request.Timeout = 0;
			c->Request.CDB[0] = 0x26;
			c->Request.CDB[6] = cmd;
			c->Request.CDB[7] = (size >> 8) & 0xFF;
			c->Request.CDB[8] = size & 0xFF;
			c->Request.CDB[9] = 0;
			c->Request.CDB[10] = 0;
			c->Request.CDB[11] = 0;
			break;
		case HPSA_READ_CAPACITY:
			c->Request.CDBLen = 10;
			c->Request.Type.Attribute = ATTR_SIMPLE;
			c->Request.Type.Direction = XFER_READ;
			c->Request.Timeout = 0;
			c->Request.CDB[0] = cmd;
			break;
		case HPSA_CACHE_FLUSH:
			c->Request.CDBLen = 12;
			c->Request.Type.Attribute = ATTR_SIMPLE;
			c->Request.Type.Direction = XFER_WRITE;
			c->Request.Timeout = 0;
			c->Request.CDB[0] = BMIC_WRITE;
			c->Request.CDB[6] = BMIC_CACHE_FLUSH;
			break;
		case TEST_UNIT_READY:
			c->Request.CDBLen = 6;
			c->Request.Type.Attribute = ATTR_SIMPLE;
			c->Request.Type.Direction = XFER_NONE;
			c->Request.Timeout = 0;
			break;
		default:
			dev_warn(&h->pdev->dev, "unknown command 0x%c\n", cmd);
			BUG();
			return;
		}
	} else if (cmd_type == TYPE_MSG) {
		switch (cmd) {

		case  HPSA_DEVICE_RESET_MSG:
			c->Request.CDBLen = 16;
			c->Request.Type.Type =  1; /* It is a MSG not a CMD */
			c->Request.Type.Attribute = ATTR_SIMPLE;
			c->Request.Type.Direction = XFER_NONE;
			c->Request.Timeout = 0; /* Don't time out */
			c->Request.CDB[0] = HPSA_RESET;
			c->Request.CDB[1] = HPSA_LUN_RESET_TYPE;
			c->Request.CDB[4] = 0x00; /* Clear bits in cbd[4-7]  */
			c->Request.CDB[5] = 0x00; /* on a logical unit reset */
			c->Request.CDB[6] = 0x00; /* means reset specified   */
			c->Request.CDB[7] = 0x00; /* logical unit            */
		break;

		case  HPSA_BUS_RESET_MSG:
			c->Request.CDBLen = 16;
			c->Request.Type.Type = TYPE_MSG;
			c->Request.Type.Attribute = ATTR_SIMPLE;
			c->Request.Type.Direction = XFER_NONE;
			c->Request.Timeout = 0; /* Don't time out */
			c->Request.CDB[0] = HPSA_RESET; 
			c->Request.CDB[1] = HPSA_LUN_RESET_TYPE; 
			c->Request.CDB[4] = 0xff; /* Setting bits in cdb[4-7]*/
			c->Request.CDB[5] = 0xff; /* on a logical unit reset */
			c->Request.CDB[6] = 0xff; /* means reset BUS of the  */
			c->Request.CDB[7] = 0xff; /* logical unit specified. */
		break;

		case  HPSA_TARGET_RESET_MSG:
			c->Request.CDBLen = 16;
			c->Request.Type.Type = TYPE_MSG;
			c->Request.Type.Attribute = ATTR_SIMPLE;
			c->Request.Type.Direction = XFER_NONE;
			c->Request.Timeout = 0; /* Don't time out */
			c->Request.CDB[0] = HPSA_RESET; 
			c->Request.CDB[1] = HPSA_TARGET_RESET_TYPE; 
			c->Request.CDB[4] = 0x00; /* reset the target */
			c->Request.CDB[5] = 0x00; /* Of logical unit  */
			c->Request.CDB[6] = 0x00; 
			c->Request.CDB[7] = 0x00; 
		break;
		case  HPSA_ABORT_MSG:
			a = buff;       /* point to command to be aborted */
			if (a == NULL) {
				hpsa_dbmsg(h, 1, "Abort: fill_cmd: abort pointer is NULL\n");
				BUG();
			}
			hpsa_dbmsg(h, 2, "Abort Tag:0x%08x:%08x "
				"using request Tag:0x%08x:%08x\n",
				a->Header.Tag.upper, a->Header.Tag.lower,
				c->Header.Tag.upper, c->Header.Tag.lower);
			c->Request.CDBLen = 16;
			c->Request.Type.Type = TYPE_MSG;
			c->Request.Type.Attribute = ATTR_SIMPLE;
			c->Request.Type.Direction = XFER_WRITE;
			c->Request.Timeout = 0; /* Don't time out */
			c->Request.CDB[0] = HPSA_TASK_MANAGEMENT;
			c->Request.CDB[1] = HPSA_TMF_ABORT_TASK;
			c->Request.CDB[2] = 0x00; /* reserved */
			c->Request.CDB[3] = 0x00; /* reserved */
			/* Tag to abort goes in CDB[4]-CDB[11] */
			c->Request.CDB[4] = ((a->Header.Tag.lower >> 24) & 0xFF);
			c->Request.CDB[5] = ((a->Header.Tag.lower >> 16) & 0xFF);
			c->Request.CDB[6] = ((a->Header.Tag.lower >>  8) & 0xFF);
			c->Request.CDB[7] = ((a->Header.Tag.lower      ) & 0xFF);
			c->Request.CDB[8] = ((a->Header.Tag.upper >> 24) & 0xFF);
			c->Request.CDB[9] = ((a->Header.Tag.upper >> 16) & 0xFF);
			c->Request.CDB[10]= ((a->Header.Tag.upper >>  8) & 0xFF);
			c->Request.CDB[11]= ((a->Header.Tag.upper      ) & 0xFF);
			c->Request.CDB[12] = 0x00; /* reserved */
			c->Request.CDB[13] = 0x00; /* reserved */
			c->Request.CDB[14] = 0x00; /* reserved */
			c->Request.CDB[15] = 0x00; /* reserved */
		break;

		default:
			dev_warn(&h->pdev->dev, "unknown message type %d\n",
				cmd);
			BUG();
		}
	} else {
		dev_warn(&h->pdev->dev, "unknown command type %d\n", cmd_type);
		BUG();
	}

	switch (c->Request.Type.Direction) {
	case XFER_READ:
		pci_dir = PCI_DMA_FROMDEVICE;
		break;
	case XFER_WRITE:
		pci_dir = PCI_DMA_TODEVICE;
		break;
	case XFER_NONE:
		pci_dir = PCI_DMA_NONE;
		break;
	default:
		pci_dir = PCI_DMA_BIDIRECTIONAL;
	}

	hpsa_map_one(h->pdev, c, buff, size, pci_dir);

	return;
}

/*
 * Map (physical) PCI mem into (virtual) kernel space
 */
static void __iomem *remap_pci_mem(ulong base, ulong size)
{
	ulong page_base = ((ulong) base) & PAGE_MASK;
	ulong page_offs = ((ulong) base) - page_base;
	void __iomem *page_remapped = ioremap(page_base, page_offs + size);

	return page_remapped ? (page_remapped + page_offs) : NULL;
}

/* Takes cmds off the submission queue and sends them to the hardware,
 * then puts them on the queue of cmds waiting for completion.
 */
static void start_io(struct ctlr_info *h)
{
	struct CommandList *c;

	while (!list_empty(&h->reqQ)) {
		c = list_entry(h->reqQ.next, struct CommandList, list);
		/* can't do anything if fifo is full */
		if ((h->access.fifo_full(h))) {
			dev_warn(&h->pdev->dev, "fifo full\n");
			break;
		}

		/* Get the first entry from the Request Q */
		removeQ(c);
		h->Qdepth--;

		/* Tell the controller execute command */
		h->access.submit_command(h, c);

		/* Put job onto the completed Q */
		addQ(&h->cmpQ, c);
	}
}

static inline unsigned long get_next_completion(struct ctlr_info *h)
{
	return h->access.command_completed(h);
}

static inline bool interrupt_pending(struct ctlr_info *h)
{
	return h->access.intr_pending(h);
}

static inline long interrupt_not_for_us(struct ctlr_info *h)
{
	return !(h->msi_vector || h->msix_vector) &&
		((h->access.intr_pending(h) == 0) ||
		(h->interrupts_enabled == 0));
}

static inline int bad_tag(struct ctlr_info *h, u32 tag_index,
	u32 raw_tag)
{
	if (unlikely(tag_index >= h->nr_cmds)) {
		dev_warn(&h->pdev->dev, "bad tag 0x%08x ignored.\n", raw_tag);
		return 1;
	}
	return 0;
}

static inline void finish_cmd(struct CommandList *c, u32 raw_tag)
{
	removeQ(c);
	if (likely(c->cmd_type == CMD_SCSI))
		complete_scsi_command(c, 0, raw_tag);
	else if (c->cmd_type == CMD_IOCTL_PEND)
		complete(c->waiting);
}

static inline u32 hpsa_tag_contains_index(u32 tag)
{
#define DIRECT_LOOKUP_BIT 0x10
	return tag & DIRECT_LOOKUP_BIT;
}

static inline u32 hpsa_tag_to_index(u32 tag)
{
#define DIRECT_LOOKUP_SHIFT 5
	return tag >> DIRECT_LOOKUP_SHIFT;
}

static inline u32 hpsa_tag_discard_error_bits(struct ctlr_info *h, u32 tag)
{
#define HPSA_PERF_ERROR_BITS ((1 << DIRECT_LOOKUP_SHIFT) -1)
#define HPSA_SIMPLE_ERROR_BITS 0x03
	if (unlikely(!(h->transMethod & CFGTBL_Trans_Performant)))
		return tag & ~HPSA_SIMPLE_ERROR_BITS;
	return tag & ~HPSA_PERF_ERROR_BITS;
}

/* process completion of an indexed ("direct lookup") command */
static inline u32 process_indexed_cmd(struct ctlr_info *h,
	u32 raw_tag)
{
	u32 tag_index;
	struct CommandList *c;

	tag_index = hpsa_tag_to_index(raw_tag);
	if (bad_tag(h, tag_index, raw_tag))
		return next_command(h);
	c = h->cmd_pool + tag_index;
#ifdef HPSA_DEBUG
	if (is_abort_cmd(c)) {
		hpsa_dbmsg(h, 1, "process_indexed_cmd: process abort: "
			"Tag:0x%08x:%08x\n",
			c->Header.Tag.upper, c->Header.Tag.lower);
	}
#endif
	finish_cmd(c, raw_tag);
	return next_command(h);
}

/* process completion of a non-indexed command */
static inline u32 process_nonindexed_cmd(struct ctlr_info *h,
	u32 raw_tag)
{
	u32 tag;
	struct CommandList *c = NULL;

	tag = hpsa_tag_discard_error_bits(h, raw_tag);
	list_for_each_entry(c, &h->cmpQ, list) {
		if ((c->busaddr & 0xFFFFFFE0) == (tag & 0xFFFFFFE0)) {
#ifdef HPSA_DEBUG
			if (is_abort_cmd(c))
				hpsa_dbmsg(h, 1, "process_nonindexed_cmd: "
					"process abort Tag:0x%08x:%08x\n",
					c->Header.Tag.upper, 
					c->Header.Tag.lower);
#endif
			finish_cmd(c, raw_tag);
			return next_command(h);
		}
	}
	bad_tag(h, h->nr_cmds + 1, raw_tag);
	return next_command(h);
}

#if (defined(HPSA_ESX4_0) || defined(HPSA_ESX4_1))
static irqreturn_t do_hpsa_intr(int irq, void *dev_id, struct pt_regs * regs)
#else 
static irqreturn_t do_hpsa_intr(int irq, void *dev_id)
#endif
{
	struct ctlr_info *h = dev_id;
	unsigned long flags;
	u32 raw_tag;

	if (interrupt_not_for_us(h))
		return IRQ_NONE;
	spin_lock_irqsave(&h->lock, flags);
	raw_tag = get_next_completion(h);
	while (raw_tag != FIFO_EMPTY) {
		if (hpsa_tag_contains_index(raw_tag))
			raw_tag = process_indexed_cmd(h, raw_tag);
		else
			raw_tag = process_nonindexed_cmd(h, raw_tag);
	}
	spin_unlock_irqrestore(&h->lock, flags);
	return IRQ_HANDLED;
}

#if !defined(__VMKLNX__)
/* Send a message CDB to the firmware. */
static __devinit int hpsa_message(struct pci_dev *pdev, unsigned char opcode,
						unsigned char type)
{
	struct Command {
		struct CommandListHeader CommandHeader;
		struct RequestBlock Request;
		struct ErrDescriptor ErrorDescriptor;
	};
	struct Command *cmd;
	static const size_t cmd_sz = sizeof(*cmd) +
					sizeof(cmd->ErrorDescriptor);
	dma_addr_t paddr64;
	uint32_t paddr32, tag;
	void __iomem *vaddr;
	int i, err;

        struct ctlr_info *h;
        h = pci_get_drvdata(pdev);

	/* kernel.org uses pci_remap_bar() here, but 2.6.27 doesn't have it.*/
	vaddr = ioremap_nocache(pci_resource_start(pdev, 0),
					pci_resource_len(pdev, 0));
	if (vaddr == NULL)
		return -ENOMEM;

	/* The Inbound Post Queue only accepts 32-bit physical addresses for the
	 * CCISS commands, so they must be allocated from the lower 4GiB of
	 * memory.
	 */
	err = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32));
	if (err) {
		iounmap(vaddr);
		return -ENOMEM;
	}

	cmd = pci_alloc_consistent(pdev, cmd_sz, &paddr64);
	if (cmd == NULL) {
		iounmap(vaddr);
		return -ENOMEM;
	}

	/* This must fit, because of the 32-bit consistent DMA mask.  Also,
	 * although there's no guarantee, we assume that the address is at
	 * least 4-byte aligned (most likely, it's page-aligned).
	 */
	paddr32 = paddr64;

	cmd->CommandHeader.ReplyQueue = 0;
	cmd->CommandHeader.SGList = 0;
	cmd->CommandHeader.SGTotal = 0;
	cmd->CommandHeader.Tag.lower = paddr32;
	cmd->CommandHeader.Tag.upper = 0;
	memset(&cmd->CommandHeader.LUN.LunAddrBytes, 0, 8);

	cmd->Request.CDBLen = 16;
	cmd->Request.Type.Type = TYPE_MSG;
	cmd->Request.Type.Attribute = ATTR_HEADOFQUEUE;
	cmd->Request.Type.Direction = XFER_NONE;
	cmd->Request.Timeout = 0; /* Don't time out */
	cmd->Request.CDB[0] = opcode;
	cmd->Request.CDB[1] = type;
	memset(&cmd->Request.CDB[2], 0, 14); /* rest of the CDB is reserved */
	cmd->ErrorDescriptor.Addr.lower = paddr32 + sizeof(*cmd);
	cmd->ErrorDescriptor.Addr.upper = 0;
	cmd->ErrorDescriptor.Len = sizeof(struct ErrorInfo);

	writel(paddr32, vaddr + SA5_REQUEST_PORT_OFFSET);

	for (i = 0; i < HPSA_MSG_SEND_RETRY_LIMIT; i++) {
		tag = readl(vaddr + SA5_REPLY_PORT_OFFSET);
		if (hpsa_tag_discard_error_bits(h, tag) == paddr32)
			break;
		msleep(HPSA_MSG_SEND_RETRY_INTERVAL_MSECS);
	}

	iounmap(vaddr);

	/* we leak the DMA buffer here ... no choice since the controller could
	 *  still complete the command.
	 */
	if (i == HPSA_MSG_SEND_RETRY_LIMIT) {
		dev_err(&pdev->dev, "controller message %02x:%02x timed out\n",
			opcode, type);
		return -ETIMEDOUT;
	}

	pci_free_consistent(pdev, cmd_sz, cmd, paddr64);

	if (tag & HPSA_ERROR_BIT) {
		dev_err(&pdev->dev, "controller message %02x:%02x failed\n",
			opcode, type);
		return -EIO;
	}

	dev_info(&pdev->dev, "controller message %02x:%02x succeeded\n",
		opcode, type);
	return 0;
}

#define hpsa_soft_reset_controller(p) hpsa_message(p, 1, 0)
#define hpsa_noop(p) hpsa_message(p, 3, 0)
#endif /* NOT defined (__VMKLNX__) */

#if !defined(__VMKLNX__)
static __devinit int hpsa_reset_msi(struct pci_dev *pdev)
{
/* the #defines are stolen from drivers/pci/msi.h. */
#define msi_control_reg(base)		(base + PCI_MSI_FLAGS)
#define PCI_MSIX_FLAGS_ENABLE		(1 << 15)

	int pos;
	u16 control = 0;

	pos = pci_find_capability(pdev, PCI_CAP_ID_MSI);
	if (pos) {
		pci_read_config_word(pdev, msi_control_reg(pos), &control);
		if (control & PCI_MSI_FLAGS_ENABLE) {
			dev_info(&pdev->dev, "resetting MSI\n");
			pci_write_config_word(pdev, msi_control_reg(pos),
					control & ~PCI_MSI_FLAGS_ENABLE);
		}
	}

	pos = pci_find_capability(pdev, PCI_CAP_ID_MSIX);
	if (pos) {
		pci_read_config_word(pdev, msi_control_reg(pos), &control);
		if (control & PCI_MSIX_FLAGS_ENABLE) {
			dev_info(&pdev->dev, "resetting MSI-X\n");
			pci_write_config_word(pdev, msi_control_reg(pos),
					control & ~PCI_MSIX_FLAGS_ENABLE);
		}
	}

	return 0;
}
#endif /* NOT defined(__VMKLNX__) */

#if !defined(__VMKLNX__)
/* This does a hard reset of the controller using PCI power management
 * states.
 */
static __devinit int hpsa_hard_reset_controller(struct pci_dev *pdev)
{
	u16 pmcsr, saved_config_space[32];
	int i, pos;

	dev_info(&pdev->dev, "using PCI PM to reset controller\n");

	/* This is very nearly the same thing as
	 *
	 * pci_save_state(pci_dev);
	 * pci_set_power_state(pci_dev, PCI_D3hot);
	 * pci_set_power_state(pci_dev, PCI_D0);
	 * pci_restore_state(pci_dev);
	 *
	 * but we can't use these nice canned kernel routines on
	 * kexec, because they also check the MSI/MSI-X state in PCI
	 * configuration space and do the wrong thing when it is
	 * set/cleared.  Also, the pci_save/restore_state functions
	 * violate the ordering requirements for restoring the
	 * configuration space from the CCISS document (see the
	 * comment below).  So we roll our own ....
	 */

	for (i = 0; i < 32; i++)
		pci_read_config_word(pdev, 2*i, &saved_config_space[i]);

	pos = pci_find_capability(pdev, PCI_CAP_ID_PM);
	if (pos == 0) {
		dev_err(&pdev->dev,
			"hpsa_reset_controller: PCI PM not supported\n");
		return -ENODEV;
	}

	/* Quoting from the Open CISS Specification: "The Power
	 * Management Control/Status Register (CSR) controls the power
	 * state of the device.  The normal operating state is D0,
	 * CSR=00h.  The software off state is D3, CSR=03h.  To reset
	 * the controller, place the interface device in D3 then to
	 * D0, this causes a secondary PCI reset which will reset the
	 * controller."
	 */

	/* enter the D3hot power management state */
	pci_read_config_word(pdev, pos + PCI_PM_CTRL, &pmcsr);
	pmcsr &= ~PCI_PM_CTRL_STATE_MASK;
	pmcsr |= PCI_D3hot;
	pci_write_config_word(pdev, pos + PCI_PM_CTRL, pmcsr);

	msleep(500);

	/* enter the D0 power management state */
	pmcsr &= ~PCI_PM_CTRL_STATE_MASK;
	pmcsr |= PCI_D0;
	pci_write_config_word(pdev, pos + PCI_PM_CTRL, pmcsr);

	msleep(500);

	/* Restore the PCI configuration space.  The Open CISS
	 * Specification says, "Restore the PCI Configuration
	 * Registers, offsets 00h through 60h. It is important to
	 * restore the command register, 16-bits at offset 04h,
	 * last. Do not restore the configuration status register,
	 * 16-bits at offset 06h."  Note that the offset is 2*i.
	 */
	for (i = 0; i < 32; i++) {
		if (i == 2 || i == 3)
			continue;
		pci_write_config_word(pdev, 2*i, saved_config_space[i]);
	}
	wmb();
	pci_write_config_word(pdev, 4, saved_config_space[2]);

	return 0;
}
#endif /* NOT defined(__VMKLNX__) */

/*
 *  We cannot read the structure directly, for portability we must use
 *   the io functions.
 *   This is for debug only.
 */
#ifdef HPSA_DEBUG
static void print_cfg_table(struct device *dev, struct CfgTable *tb)
{
	int i;
	char temp_name[17];

	dev_info(dev, "Controller Configuration information\n");
	dev_info(dev, "------------------------------------\n");
	for (i = 0; i < 4; i++)
		temp_name[i] = readb(&(tb->Signature[i]));
	temp_name[4] = '\0';
	dev_info(dev, "   Signature = %s\n", temp_name);
	dev_info(dev, "   Spec Number = %d\n", readl(&(tb->SpecValence)));
	dev_info(dev, "   Transport methods supported = 0x%x\n",
	       readl(&(tb->TransportSupport)));
	dev_info(dev, "   Transport methods active = 0x%x\n",
	       readl(&(tb->TransportActive)));
	dev_info(dev, "   Requested transport Method = 0x%x\n",
	       readl(&(tb->HostWrite.TransportRequest)));
	dev_info(dev, "   Coalesce Interrupt Delay = 0x%x\n",
	       readl(&(tb->HostWrite.CoalIntDelay)));
	dev_info(dev, "   Coalesce Interrupt Count = 0x%x\n",
	       readl(&(tb->HostWrite.CoalIntCount)));
	dev_info(dev, "   Max outstanding commands = 0x%d\n",
	       readl(&(tb->CmdsOutMax)));
	dev_info(dev, "   Bus Types = 0x%x\n", readl(&(tb->BusTypes)));
	for (i = 0; i < 16; i++)
		temp_name[i] = readb(&(tb->ServerName[i]));
	temp_name[16] = '\0';
	dev_info(dev, "   Server Name = %s\n", temp_name);
	dev_info(dev, "   Heartbeat Counter = 0x%x\n\n\n",
		readl(&(tb->HeartBeat)));
}
#endif				/* HPSA_DEBUG */

static int find_PCI_BAR_index(struct pci_dev *pdev, unsigned long pci_bar_addr)
{
	int i, offset, mem_type, bar_type;

	if (pci_bar_addr == PCI_BASE_ADDRESS_0)	/* looking for BAR zero? */
		return 0;
	offset = 0;
	for (i = 0; i < DEVICE_COUNT_RESOURCE; i++) {
		bar_type = pci_resource_flags(pdev, i) & PCI_BASE_ADDRESS_SPACE;
		if (bar_type == PCI_BASE_ADDRESS_SPACE_IO)
			offset += 4;
		else {
			mem_type = pci_resource_flags(pdev, i) &
			    PCI_BASE_ADDRESS_MEM_TYPE_MASK;
			switch (mem_type) {
			case PCI_BASE_ADDRESS_MEM_TYPE_32:
			case PCI_BASE_ADDRESS_MEM_TYPE_1M:
				offset += 4;	/* 32 bit */
				break;
			case PCI_BASE_ADDRESS_MEM_TYPE_64:
				offset += 8;
				break;
			default:	/* reserved in PCI 2.2 */
				dev_warn(&pdev->dev,
				       "base address is invalid\n");
				return -1;
				break;
			}
		}
		if (offset == pci_bar_addr - PCI_BASE_ADDRESS_0)
			return i + 1;
	}
	return -1;
}

/* If MSI/MSI-X is supported by the kernel we will try to enable it on
 * controllers that are capable. If not, we use IO-APIC mode.
 */

static void __devinit hpsa_interrupt_mode(struct ctlr_info *h,
					   struct pci_dev *pdev, u32 board_id)
{
#ifdef CONFIG_PCI_MSI
	int err;
	struct msix_entry hpsa_msix_entries[4] = { {0, 0}, {0, 1},
	{0, 2}, {0, 3}
	};

	/* Some boards advertise MSI but don't really support it */
	if ((board_id == 0x40700E11) ||
	    (board_id == 0x40800E11) ||
	    (board_id == 0x40820E11) || (board_id == 0x40830E11))
		goto default_int_mode;
	if (pci_find_capability(pdev, PCI_CAP_ID_MSIX)) {
		dev_info(&pdev->dev, "MSIX\n");
		err = pci_enable_msix(pdev, hpsa_msix_entries, 4);
		if (!err) {
			h->intr[0] = hpsa_msix_entries[0].vector;
			h->intr[1] = hpsa_msix_entries[1].vector;
			h->intr[2] = hpsa_msix_entries[2].vector;
			h->intr[3] = hpsa_msix_entries[3].vector;
			h->msix_vector = 1;
			return;
		}
		if (err > 0) {
			dev_warn(&pdev->dev, "only %d MSI-X vectors "
			       "available\n", err);
			goto default_int_mode;
		} else {
			dev_warn(&pdev->dev, "MSI-X init failed %d\n",
			       err);
			goto default_int_mode;
		}
	}
	if (pci_find_capability(pdev, PCI_CAP_ID_MSI)) {
		dev_info(&pdev->dev, "MSI\n");
		if (!pci_enable_msi(pdev))
			h->msi_vector = 1;
		else
			dev_warn(&pdev->dev, "MSI init failed\n");
	}
default_int_mode:
#endif				/* CONFIG_PCI_MSI */
	/* if we get here we're going to use the default interrupt mode */
	h->intr[PERF_MODE_INT] = pdev->irq;
}

static int __devinit hpsa_pci_init(struct ctlr_info *h, struct pci_dev *pdev)
{
	ushort subsystem_vendor_id, subsystem_device_id, command;
	u32 board_id, scratchpad = 0;
	u64 cfg_offset;
	u32 cfg_base_addr;
	u64 cfg_base_addr_index;
	u32 trans_offset;
	int i, prod_index, err;

	subsystem_vendor_id = pdev->subsystem_vendor;
	subsystem_device_id = pdev->subsystem_device;
	board_id = (((u32) (subsystem_device_id << 16) & 0xffff0000) |
		    subsystem_vendor_id);

	for (i = 0; i < ARRAY_SIZE(products); i++)
		if (board_id == products[i].board_id)
			break;

	prod_index = i;

	if (prod_index == ARRAY_SIZE(products)) {
		prod_index--;
		if (subsystem_vendor_id != PCI_VENDOR_ID_HP ||
				!hpsa_allow_any) {
			dev_warn(&pdev->dev, "unrecognized board ID:"
				" 0x%08lx, ignoring.\n",
				(unsigned long) board_id);
			return -ENODEV;
		}
	}
	/* check to see if controller has been disabled
	 * BEFORE trying to enable it
	 */
	(void)pci_read_config_word(pdev, PCI_COMMAND, &command);
	if (!(command & 0x02)) {
		dev_warn(&pdev->dev, "controller appears to be disabled\n");
		return -ENODEV;
	}

	err = pci_enable_device(pdev);
	if (err) {
		dev_warn(&pdev->dev, "unable to enable PCI device\n");
		return err;
	}

	err = pci_request_regions(pdev, "hpsa");
	if (err) {
		dev_err(&pdev->dev, "cannot obtain PCI resources, aborting\n");
		return err;
	}

	/* If the kernel supports MSI/MSI-X we will try to enable that,
	 * else we use the IO-APIC interrupt assigned to us by system ROM.
	 */
	hpsa_interrupt_mode(h, pdev, board_id);

	/* find the memory BAR */
	for (i = 0; i < DEVICE_COUNT_RESOURCE; i++) {
		if (pci_resource_flags(pdev, i) & IORESOURCE_MEM)
			break;
	}
	if (i == DEVICE_COUNT_RESOURCE) {
		dev_warn(&pdev->dev, "no memory BAR found\n");
		err = -ENODEV;
		goto err_out_free_res;
	}

	h->paddr = pci_resource_start(pdev, i); /* addressing mode bits
						 * already removed
						 */

	h->vaddr = remap_pci_mem(h->paddr, 0x250);

	/* Wait for the board to become ready.  */
	for (i = 0; i < HPSA_BOARD_READY_ITERATIONS; i++) {
		scratchpad = readl(h->vaddr + SA5_SCRATCHPAD_OFFSET);
		if (scratchpad == HPSA_FIRMWARE_READY)
			break;
		msleep(HPSA_BOARD_READY_POLL_INTERVAL_MSECS);
	}
	if (scratchpad != HPSA_FIRMWARE_READY) {
		dev_warn(&pdev->dev, "board not ready, timed out.\n");
		err = -ENODEV;
		goto err_out_free_res;
	}

	/* get the address index number */
	cfg_base_addr = readl(h->vaddr + SA5_CTCFG_OFFSET);
	cfg_base_addr &= (u32) 0x0000ffff;
	cfg_base_addr_index = find_PCI_BAR_index(pdev, cfg_base_addr);
	if (cfg_base_addr_index == -1) {
		dev_warn(&pdev->dev, "cannot find cfg_base_addr_index\n");
		err = -ENODEV;
		goto err_out_free_res;
	}

	cfg_offset = readl(h->vaddr + SA5_CTMEM_OFFSET);
	h->cfgtable = remap_pci_mem(pci_resource_start(pdev,
			       cfg_base_addr_index) + cfg_offset,
				sizeof(h->cfgtable));
	/* Find performant mode table. */
	trans_offset = readl(&(h->cfgtable->TransMethodOffset));
	h->transtable = remap_pci_mem(pci_resource_start(pdev,
				cfg_base_addr_index)+cfg_offset+trans_offset,
				sizeof(*h->transtable));

	h->board_id = board_id;
	h->max_commands = readl(&(h->cfgtable->MaxPerformantModeCommands));
	h->maxsgentries = readl(&(h->cfgtable->MaxScatterGatherElements));

	/*
	 * Limit in-command s/g elements to 32 save dma'able memory.
	 * Howvever spec says if 0, use 31
	 */

	h->max_cmd_sg_entries = 31;
	if (h->maxsgentries > 512) {
#if defined (__VMKLNX__)
		/* Enforce an upper limit on max SG entries to reduce
		 * driver max heap size within VMware tolerance.
		 */
		hpsa_limit_maxsgentries(h); 
#endif 
		h->max_cmd_sg_entries = 32;
		h->chainsize = h->maxsgentries - h->max_cmd_sg_entries + 1;
		h->maxsgentries--; /* save one for chain pointer */
	} else {
		h->maxsgentries = 31; /* default to traditional values */
		h->chainsize = 0;
	}
#ifdef HPSA_DEBUG
	hpsa_dbmsg(h, 0, "DEBUG: SGs: maxsgentries(%d) in-cmd-sgs(%d)  "
		"chainsize(%d).\n", h->maxsgentries, h->max_cmd_sg_entries,
		h->chainsize);
#endif /* HPSA_DEBUG */

	h->product_name = products[prod_index].product_name;
	h->access = *(products[prod_index].access);
	/* Allow room for some ioctls */
	h->nr_cmds = h->max_commands - 4;

	if ((readb(&h->cfgtable->Signature[0]) != 'C') ||
	    (readb(&h->cfgtable->Signature[1]) != 'I') ||
	    (readb(&h->cfgtable->Signature[2]) != 'S') ||
	    (readb(&h->cfgtable->Signature[3]) != 'S')) {
		dev_warn(&pdev->dev, "not a valid CISS config table\n");
		err = -ENODEV;
		goto err_out_free_res;
	}

	/* Update the field, and then ring the doorbell */
	writel(CFGTBL_Trans_Simple, &(h->cfgtable->HostWrite.TransportRequest));
	writel(CFGTBL_ChangeReq, h->vaddr + SA5_DOORBELL);

	/* under certain very rare conditions, this can take awhile.
	 * (e.g.: hot replace a failed 144GB drive in a RAID 5 set right
	 * as we enter this code.)
	 */
	for (i = 0; i < MAX_CONFIG_WAIT; i++) {
		if (!(readl(h->vaddr + SA5_DOORBELL) & CFGTBL_ChangeReq))
			break;
		/* delay and try again */
		msleep(10);
	}

#ifdef HPSA_DEBUG
	print_cfg_table(&pdev->dev, h->cfgtable);
#endif				/* HPSA_DEBUG */

	if (!(readl(&(h->cfgtable->TransportActive)) & CFGTBL_Trans_Simple)) {
		dev_warn(&pdev->dev, "unable to get board into simple mode\n");
		err = -ENODEV;
		goto err_out_free_res;
	}
	h->transMethod = CFGTBL_Trans_Simple;
	return 0;

err_out_free_res:
	/*
	 * Deliberately omit pci_disable_device(): it does something nasty to
	 * Smart Array controllers that pci_enable_device does not undo
	 */
	pci_release_regions(pdev);
	return err;
}

static void __devinit hpsa_hba_inquiry(struct ctlr_info *h)
{
	int rc;

#define HBA_INQUIRY_BYTE_COUNT 64
	h->hba_inquiry_data = kmalloc(HBA_INQUIRY_BYTE_COUNT, GFP_KERNEL);
	if (!h->hba_inquiry_data)
		return;
	rc = hpsa_scsi_do_inquiry(h, RAID_CTLR_LUNID, HPSA_NOT_VPD, 0,
		h->hba_inquiry_data, HBA_INQUIRY_BYTE_COUNT);
	if (rc != 0) {
		kfree(h->hba_inquiry_data);
		h->hba_inquiry_data = NULL;
	}
}

static int __devinit hpsa_init_one(struct pci_dev *pdev,
				    const struct pci_device_id *ent)
{
	int rc;
#if !defined(__VMKLNX__)
	int i;
#endif 
	int dac;
	struct ctlr_info *h;

	if (number_of_controllers == 0)
		printk(KERN_INFO DRIVER_NAME "\n");

#if !defined (__VMKLNX__)
	if (reset_devices) {
		/* Reset the controller with a PCI power-cycle */
		if (hpsa_hard_reset_controller(pdev) || hpsa_reset_msi(pdev))
			return -ENODEV;

		/* Some devices (notably the HP Smart Array 5i Controller)
		   need a little pause here */
		msleep(HPSA_POST_RESET_PAUSE_MSECS);

		/* Now try to get the controller to respond to a no-op */
		for (i = 0; i < HPSA_POST_RESET_NOOP_RETRIES; i++) {
			if (hpsa_noop(pdev) == 0)
				break;
			else
				dev_warn(&pdev->dev, "no-op failed%s\n",
						(i < 11 ? "; re-trying" : ""));
		}
	}
#endif //!defined (__VMKLNX__)

	/* Command structures must be aligned on a 32-byte boundary because
	 * the 5 lower bits of the address are used by the hardware. and by
	 * the driver.  See comments in hpsa.h for more info.
	 */
#define COMMANDLIST_ALIGNMENT 32
	BUILD_BUG_ON(sizeof(struct CommandList) % COMMANDLIST_ALIGNMENT);
	h = kzalloc(sizeof(*h), GFP_KERNEL);
	if (!h)
		return -ENOMEM;

	h->busy_initializing = 1;
	INIT_LIST_HEAD(&h->cmpQ);
	INIT_LIST_HEAD(&h->reqQ);
	INIT_LIST_HEAD(&h->offline_device_list);
	spin_lock_init(&h->offline_device_lock);
	rc = hpsa_pci_init(h, pdev);
	if (rc != 0)
		goto clean1;

	sprintf(h->devname, "hpsa%d", number_of_controllers);
	h->ctlr = number_of_controllers;
	cch_devs[number_of_controllers].h = h; /* enable char device lookup */
	number_of_controllers++;
	h->offline_device_thread_state = OFFLINE_DEVICE_THREAD_STOPPED;
	h->pdev = pdev;

	/* configure PCI DMA stuff */
	rc = pci_set_dma_mask(pdev, DMA_BIT_MASK(64));
	if (rc == 0) {
		dac = 1;
	} else {
		rc = pci_set_dma_mask(pdev, DMA_BIT_MASK(32));
		if (rc == 0) {
			dac = 0;
		} else {
			dev_err(&pdev->dev, "no suitable DMA available\n");
			goto clean1;
		}
	}

	/* make sure the board interrupts are off */
	h->access.set_intr_mask(h, HPSA_INTR_OFF);
	rc = request_irq(h->intr[PERF_MODE_INT], do_hpsa_intr,
			IRQF_DISABLED, h->devname, h);
	if (rc) {
		dev_err(&pdev->dev, "unable to get irq %d for %s\n",
		       h->intr[PERF_MODE_INT], h->devname);
		goto clean2;
	}

#if ( ( defined(HPSA_ESX4_0) || ( defined(HPSA_ESX4_1))) && defined(__VMKLNX__) )
        hpsa_set_irq(h, pdev);
#endif /*if defined(HPSA_ESX4_0 or HPSA_ESX4_1) and defined(__VMKLNX__) */

	dev_info(&pdev->dev, "%s: <0x%x> at IRQ %d%s using DAC\n",
	       h->devname, pdev->device,
	       h->intr[PERF_MODE_INT], dac ? "" : " not");

	h->cmd_pool_bits =
	    kmalloc(((h->nr_cmds + BITS_PER_LONG -
		      1) / BITS_PER_LONG) * sizeof(unsigned long), GFP_KERNEL);
	h->cmd_pool = pci_alloc_consistent(h->pdev,
		    h->nr_cmds * sizeof(*h->cmd_pool),
		    &(h->cmd_pool_dhandle));
	h->errinfo_pool = pci_alloc_consistent(h->pdev,
		    h->nr_cmds * sizeof(*h->errinfo_pool),
		    &(h->errinfo_pool_dhandle));
	if ((h->cmd_pool_bits == NULL)
	    || (h->cmd_pool == NULL)
	    || (h->errinfo_pool == NULL)) {
		dev_err(&pdev->dev, "out of memory");
		rc = -ENOMEM;
		goto clean4;
	}
#ifdef HPSA_DEBUG
	hpsa_dbmsg(h, 0, "DEBUG: allocate %d bytes for cmd_pool (%d cmds)\n", 
		(h->nr_cmds * sizeof(*h->cmd_pool)), h->nr_cmds);
	hpsa_dbmsg(h, 0, "DEBUG: allocate %d bytes for errinfo_pool\n", 
		(h->nr_cmds * sizeof(*h->errinfo_pool)));
#endif /* HPSA_DEBUG */
	if (hpsa_allocate_sg_chain_blocks(h))
		goto clean4;
	spin_lock_init(&h->lock);
	spin_lock_init(&h->scan_lock);
	init_waitqueue_head(&h->scan_wait_queue);
	h->scan_finished = 1; /* no scan currently in progress */

	pci_set_drvdata(pdev, h);
	memset(h->cmd_pool_bits, 0,
	       ((h->nr_cmds + BITS_PER_LONG -
		 1) / BITS_PER_LONG) * sizeof(unsigned long));
	hpsa_create_rescan(h);
	/* query controller for task management abilities. */
	hpsa_set_tmf_support(h);
	hpsa_scsi_setup(h);

	/* Turn the interrupts on so we can service requests */
	h->access.set_intr_mask(h, HPSA_INTR_ON);

	hpsa_put_ctlr_into_performant_mode(h);
	hpsa_hba_inquiry(h);
	hpsa_procinit(h);
	hpsa_register_scsi(h);	/* hook ourselves into SCSI subsystem */
	h->busy_initializing = 0;
	return 1;

clean4:
	hpsa_free_sg_chain_blocks(h);
	kfree(h->cmd_pool_bits);
	if (h->cmd_pool)
		pci_free_consistent(h->pdev,
			    h->nr_cmds * sizeof(struct CommandList),
			    h->cmd_pool, h->cmd_pool_dhandle);
	if (h->errinfo_pool)
		pci_free_consistent(h->pdev,
			    h->nr_cmds * sizeof(struct ErrorInfo),
			    h->errinfo_pool,
			    h->errinfo_pool_dhandle);
	free_irq(h->intr[PERF_MODE_INT], h);
clean2:
clean1:
	h->busy_initializing = 0;
	kfree(h);
	return rc;
}

static void hpsa_flush_cache(struct ctlr_info *h)
{
	char *flush_buf;
	struct CommandList *c;

	flush_buf = kzalloc(4, GFP_KERNEL);
	if (!flush_buf)
		return;

	c = cmd_special_alloc(h);
	if (!c) {
		dev_warn(&h->pdev->dev, "cmd_special_alloc returned NULL!\n");
		goto out_of_memory;
	}
	fill_cmd(c, HPSA_CACHE_FLUSH, h, flush_buf, 4, 0,
		RAID_CTLR_LUNID, TYPE_CMD);
	hpsa_scsi_do_simple_cmd_with_retry(h, c, PCI_DMA_TODEVICE);
	if (c->err_info->CommandStatus != 0)
		dev_warn(&h->pdev->dev,
			"error flushing cache on controller\n");
	cmd_special_free(h, c);
out_of_memory:
	kfree(flush_buf);
}

static void hpsa_shutdown(struct pci_dev *pdev)
{
	struct ctlr_info *h;

	h = pci_get_drvdata(pdev);

	if (h == NULL) {
		printk( KERN_ERR "hpsa: Could not shut down controller.\n");
		return;
	}
	/* Turn board interrupts off  and send the flush cache command
	 * sendcmd will turn off interrupt, and send the flush...
	 * To write all data in the battery backed cache to disks
	 */
	hpsa_flush_cache(h);
	h->access.set_intr_mask(h, HPSA_INTR_OFF);
	free_irq(h->intr[PERF_MODE_INT], h);
#ifdef CONFIG_PCI_MSI
	if (h->msix_vector)
		pci_disable_msix(h->pdev);
	else if (h->msi_vector)
		pci_disable_msi(h->pdev);
#endif				/* CONFIG_PCI_MSI */
}

static void __devexit hpsa_remove_one(struct pci_dev *pdev)
{
	struct ctlr_info *h;

	if (pci_get_drvdata(pdev) == NULL) {
		dev_err(&pdev->dev, "unable to remove device \n");
		return;
	}
	h = pci_get_drvdata(pdev);
	hpsa_unregister_scsi(h);	/* unhook from SCSI subsystem */
	stop_offline_device_monitor(h);
	hpsa_stop_rescan(h);		/* kill the rescan thread */
	remove_proc_entry(h->devname, proc_hpsa);
	hpsa_shutdown(pdev);
	iounmap(h->vaddr);
	hpsa_free_sg_chain_blocks(h);
	pci_free_consistent(h->pdev,
		h->nr_cmds * sizeof(struct CommandList),
		h->cmd_pool, h->cmd_pool_dhandle);
	pci_free_consistent(h->pdev,
		h->nr_cmds * sizeof(struct ErrorInfo),
		h->errinfo_pool, h->errinfo_pool_dhandle);
	pci_free_consistent(h->pdev, h->reply_pool_size,
		h->reply_pool, h->reply_pool_dhandle);
	kfree(h->cmd_pool_bits);
	kfree(h->blockFetchTable);
	kfree(h->hba_inquiry_data);
	/*
	 * Deliberately omit pci_disable_device(): it does something nasty to
	 * Smart Array controllers that pci_enable_device does not undo
	 */
	pci_release_regions(pdev);
	pci_set_drvdata(pdev, NULL);
	cch_devs[h->ctlr].h = NULL; /* disable char dev lookup */
	kfree(h);
}

static int hpsa_suspend(__attribute__((unused)) struct pci_dev *pdev,
	__attribute__((unused)) pm_message_t state)
{
	return -ENOSYS;
}

static int hpsa_resume(__attribute__((unused)) struct pci_dev *pdev)
{
	return -ENOSYS;
}

static struct pci_driver hpsa_pci_driver = {
	.name = "hpsa",
	.probe = hpsa_init_one,
	.remove = __devexit_p(hpsa_remove_one),
	.id_table = hpsa_pci_device_id,	/* id_table */
	.shutdown = hpsa_shutdown,
	.suspend = hpsa_suspend,
	.resume = hpsa_resume,
};

/* Fill in bucket_map[], given nsgs (the max number of
 * scatter gather elements supported) and bucket[],
 * which is an array of 8 integers.  The bucket[] array
 * contains 8 different DMA transfer sizes (in 16
 * byte increments) which the controller uses to fetch
 * commands.  This function fills in bucket_map[], which
 * maps a given number of scatter gather elements to one of
 * the 8 DMA transfer sizes.  The point of it is to allow the
 * controller to only do as much DMA as needed to fetch the
 * command, with the DMA transfer size encoded in the lower
 * bits of the command address.
 */
static void  calc_bucket_map(int bucket[], int num_buckets,
	int nsgs, int *bucket_map)
{
	int i, j, b, size;

	/* even a command with 0 SGs requires 4 blocks */
#define MINIMUM_TRANSFER_BLOCKS 4
#define NUM_BUCKETS 8
	/* Note, bucket_map must have nsgs+1 entries. */
	for (i = 0; i <= nsgs; i++) {
		/* Compute size of a command with i SG entries */
		size = i + MINIMUM_TRANSFER_BLOCKS;
		b = num_buckets; /* Assume the biggest bucket */
		/* Find the bucket that is just big enough */
		for (j = 0; j < 8; j++) {
			if (bucket[j] >= size) {
				b = j;
				break;
			}
		}
		/* for a command with i SG entries, use bucket b. */
		bucket_map[i] = b;
	}
}

static void hpsa_put_ctlr_into_performant_mode(struct ctlr_info *h)
{
	u32 trans_support;
	u64 trans_offset;
	/*  5 = 1 s/g entry or 4k
	 *  6 = 2 s/g entry or 8k
	 *  8 = 4 s/g entry or 16k
	 * 10 = 6 s/g entry or 24k
	 */
	int bft[8] = {5, 6, 8, 10, 12, 20, 28, 35}; /* for scatter/gathers */
	int i = 0;
	int l = 0;
	unsigned long register_value;

	trans_support = readl(&(h->cfgtable->TransportSupport));
	if (!(trans_support & PERFORMANT_MODE))
		return;

	h->max_commands = readl(&(h->cfgtable->MaxPerformantModeCommands));
	h->max_sg_entries = 32;
	/* Performant mode ring buffer and supporting data structures */
	h->reply_pool_size = h->max_commands * sizeof(u64);
	h->reply_pool = pci_alloc_consistent(h->pdev, h->reply_pool_size,
				&(h->reply_pool_dhandle));

	/* Need a block fetch table for performant mode */
	h->blockFetchTable = kmalloc(((h->max_sg_entries+1) *
				sizeof(u32)), GFP_KERNEL);
#ifdef HPSA_DEBUG 
	hpsa_dbmsg(h, 0, "DEBUG: allocate %d bytes for reply_pool\n", 
		h->reply_pool_size);
	hpsa_dbmsg(h, 0, "DEBUG: allocate %d bytes for blockFetchTable\n", 
		((h->max_sg_entries+1) * sizeof(u32)));
#endif /* HPSA_DEBUG */
	if ((h->reply_pool == NULL)
		|| (h->blockFetchTable == NULL))
		goto clean_up;

	h->reply_pool_wraparound = 1; /* spec: init to 1 */

	/* Controller spec: zero out this buffer. */
	memset(h->reply_pool, 0, h->reply_pool_size);
	h->reply_pool_head = h->reply_pool;

	trans_offset = readl(&(h->cfgtable->TransMethodOffset));
	bft[7] = h->max_sg_entries + 4;
	calc_bucket_map(bft, ARRAY_SIZE(bft), 32, h->blockFetchTable);
	for (i = 0; i < 8; i++)
		writel(bft[i], &h->transtable->BlockFetch[i]);

	/* size of controller ring buffer */
	writel(h->max_commands, &h->transtable->RepQSize);
	writel(1, &h->transtable->RepQCount);
	writel(0, &h->transtable->RepQCtrAddrLow32);
	writel(0, &h->transtable->RepQCtrAddrHigh32);
	writel(h->reply_pool_dhandle, &h->transtable->RepQAddr0Low32);
	writel(0, &h->transtable->RepQAddr0High32);
	writel(CFGTBL_Trans_Performant | CFGTBL_Trans_use_short_tags,
		&(h->cfgtable->HostWrite.TransportRequest));
	writel(CFGTBL_ChangeReq, h->vaddr + SA5_DOORBELL);
	/* under certain very rare conditions, this can take awhile.
	 * (e.g.: hot replace a failed 144GB drive in a RAID 5 set right
	 * as we enter this code.) */
	for (l = 0; l < MAX_CONFIG_WAIT; l++) {
		register_value = readl(h->vaddr + SA5_DOORBELL);
		if (!(register_value & CFGTBL_ChangeReq))
			break;
		/* delay and try again */
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(10);
	}
	register_value = readl(&(h->cfgtable->TransportActive));
	if (!(register_value & CFGTBL_Trans_Performant)) {
		dev_warn(&h->pdev->dev, "unable to get board into"
					" performant mode\n");
		return;
	}

	/* Change the access methods to the performant access methods */
	h->access = SA5_performant_access;
	h->transMethod = CFGTBL_Trans_Performant;

	hpsa_dbmsg(h, 3, "Performant mode activated.\n");
	return;

clean_up:
	if (h->reply_pool)
		pci_free_consistent(h->pdev, h->reply_pool_size,
			h->reply_pool, h->reply_pool_dhandle);
	kfree(h->blockFetchTable);
}

/*
 *  This is it.  Register the PCI driver information for the cards we control
 *  the OS will call our registered routines when it finds one of our cards.
 */
static int __init hpsa_init(void)
{
	int err;
	int i;
	/* initialize char device lookup array */
	for (i = 0; i < MAX_CTLR; i++ )
	{
		cch_devs[i].chrmajor = -1;
		cch_devs[i].h = NULL;
	}
#if defined(__VMKLNX__)
        hpsa_transport_template = sas_attach_transport(&hpsa_transport_functions);
        if (!hpsa_transport_template) {
                printk("sas_attach_transport FAILED, hpsa_transport_template is NULL\n");
                return -ENODEV;
        }
#endif
	err = pci_register_driver(&hpsa_pci_driver);
	return err;
}

static void __exit hpsa_cleanup(void)
{
	pci_unregister_driver(&hpsa_pci_driver);
	int i;
	/* remove char device for any registered controllers */
	for (i = 0; i < MAX_CTLR; i++ ) {
		if ((cch_devs[i].chrmajor != -1) && 
			(cch_devs[i].h != NULL ) ) 
			unregister_chrdev(cch_devs[i].chrmajor, 
				cch_devs[i].h->devname);	
	}
	remove_proc_entry("hpsa", proc_root_driver);
}

#if defined(__VMKLNX__)
#include "hpsavm.c"
#endif //if defined(__VMKLNX__)

module_init(hpsa_init);
module_exit(hpsa_cleanup);
