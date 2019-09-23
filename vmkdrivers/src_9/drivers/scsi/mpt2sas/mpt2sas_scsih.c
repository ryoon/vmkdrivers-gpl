/*
 * Portions Copyright 2009 VMware, Inc.
 */ 
/*
 * Scsi Host Layer for MPT (Message Passing Technology) based controllers
 *
 * This code is based on drivers/scsi/mpt2sas/mpt2_scsih.c
 * Copyright (C) 2007-2013  LSI Corporation
 *  (mailto:DL-MPTFusionLinux@lsi.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * NO WARRANTY
 * THE PROGRAM IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED INCLUDING, WITHOUT
 * LIMITATION, ANY WARRANTIES OR CONDITIONS OF TITLE, NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE. Each Recipient is
 * solely responsible for determining the appropriateness of using and
 * distributing the Program and assumes all risks associated with its
 * exercise of rights under this Agreement, including but not limited to
 * the risks and costs of program errors, damage to or loss of data,
 * programs or equipment, and unavailability or interruption of operations.

 * DISCLAIMER OF LIABILITY
 * NEITHER RECIPIENT NOR ANY CONTRIBUTORS SHALL HAVE ANY LIABILITY FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING WITHOUT LIMITATION LOST PROFITS), HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OR DISTRIBUTION OF THE PROGRAM OR THE EXERCISE OF ANY RIGHTS GRANTED
 * HEREUNDER, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGES

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/blkdev.h>
#include <linux/sched.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#if !defined(__VMKLNX__)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19))
#include <linux/aer.h>
#endif
#include <linux/raid_class.h>
#endif

#include "mpt2sas_base.h"

#if defined(__VMKLNX__)
#if (ESX_VERS > ESX41_VERS)
#include "vmklinux_scsi.h"
#else
#include "vmklinux26_scsi.h"
#endif

static int mpt2sas_sas_queue_depth = MPT2SAS_SAS_QUEUE_DEPTH;
module_param(mpt2sas_sas_queue_depth, int, 0);
MODULE_PARM_DESC(mpt2sas_sas_queue_depth,
    " Max SAS Device Queue Depth (default="
    __MODULE_STRING(MPT2SAS_SAS_QUEUE_DEPTH) ")");

static int mpt2sas_sata_queue_depth = MPT2SAS_SATA_QUEUE_DEPTH;
module_param(mpt2sas_sata_queue_depth, int, 0);
MODULE_PARM_DESC(mpt2sas_sata_queue_depth,
    " Max SATA Device Queue Depth (default="
    __MODULE_STRING(MPT2SAS_SATA_QUEUE_DEPTH) ")");

static int mpt2sas_raid_queue_depth = MPT2SAS_RAID_QUEUE_DEPTH;
module_param(mpt2sas_raid_queue_depth, int, 0);
MODULE_PARM_DESC(mpt2sas_raid_queue_depth,
    " Max RAID Device Queue Depth (default="
    __MODULE_STRING(MPT2SAS_RAID_QUEUE_DEPTH) ")");
#define GUARANTEE_ONE(val) (val = ((val > 0) ? val : 1))
#endif

MODULE_AUTHOR(MPT2SAS_AUTHOR);
MODULE_DESCRIPTION(MPT2SAS_DESCRIPTION);
MODULE_LICENSE("GPL");
MODULE_VERSION(MPT2SAS_DRIVER_VERSION);


/* forward proto's */
static void _scsih_expander_node_remove(struct MPT2SAS_ADAPTER *ioc,
    struct _sas_node *sas_expander);
#if (ESX_VERS > ESX41_VERS) || (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,19))
static void _firmware_event_work(struct work_struct *work);
static void _firmware_event_work_delayed(struct work_struct *work);
#else
static void _firmware_event_work(void *arg);
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27))
static enum device_responsive_state
_scsih_read_capacity_16(struct MPT2SAS_ADAPTER *ioc, u16 handle, u32 lun,
    void *data, u32 data_length);
#endif

static enum device_responsive_state
_scsih_inquiry_vpd_sn(struct MPT2SAS_ADAPTER *ioc, u16 handle,
    u8 **serial_number);

static enum device_responsive_state
_scsih_inquiry_vpd_supported_pages(struct MPT2SAS_ADAPTER *ioc, u16 handle,
    u32 lun, void *data, u32 data_length);

static enum device_responsive_state
_scsih_wait_for_target_to_become_ready(struct MPT2SAS_ADAPTER *ioc, u16 handle,
    u8 retry_count, u8 is_pd);
static enum device_responsive_state
_scsih_wait_for_device_to_become_ready(struct MPT2SAS_ADAPTER *ioc, u16 handle,
    u8 retry_count, u8 is_pd, int lun);
static void _scsih_remove_device(struct MPT2SAS_ADAPTER *ioc,
    struct _sas_device *sas_device);
static int _scsih_add_device(struct MPT2SAS_ADAPTER *ioc, u16 handle,
    u8 retry_count, u8 is_pd);

static u8 _scsih_check_for_pending_tm(struct MPT2SAS_ADAPTER *ioc, u16 smid);

/* global parameters */
LIST_HEAD(mpt2sas_ioc_list);

/* local parameters */
static u8 scsi_io_cb_idx = -1;
static u8 tm_cb_idx = -1;
static u8 ctl_cb_idx = -1;
static u8 ctl_tm_cb_idx = -1;
static u8 ctl_diag_cb_idx = -1;
static u8 base_cb_idx = -1;
static u8 transport_cb_idx = -1;
static u8 scsih_cb_idx = -1;
static u8 scsih_q_cb_idx = -1;
static u8 config_cb_idx = -1;
static int mpt_ids;

static u8 tm_tr_cb_idx = -1 ;
static u8 tm_tr_volume_cb_idx = -1 ;
static u8 tm_tr_internal_cb_idx =-1;
#ifdef MPT2SAS_MULTIPATH
static u8 tm_tr_mp_cb_idx = -1 ;
#endif
static u8 tm_sas_control_cb_idx = -1;

/* command line options */
static u32 logging_level;
#if defined(__VMKLNX__)
module_param(logging_level, int, 0);
#endif
MODULE_PARM_DESC(logging_level, " bits for enabling additional logging info "
    "(default=0)");

static int command_retry_count = 144;
module_param(command_retry_count, int, 0);
MODULE_PARM_DESC(command_retry_count, " Device discovery TUR command retry "
    "count: (default=144)");

/* scsi-mid layer global parmeter is max_report_luns, which is 511 */
#define MPT2SAS_MAX_LUN (16895)
static int max_lun = MPT2SAS_MAX_LUN;
module_param(max_lun, int, 0);
MODULE_PARM_DESC(max_lun, " max lun, default=16895 ");


static short max_sectors = -1;
module_param(max_sectors, short, 0);
MODULE_PARM_DESC(max_sectors, "max sectors, range 64 to 8192 default=8192");

#ifdef MPT2SAS_MULTIPATH
static int mpt2sas_multipath = -1;
module_param(mpt2sas_multipath, int, 0);
MODULE_PARM_DESC(mpt2sas_multipath, " enabling mulipath support for target "
    "resets (default=0)");
#endif

/* Enable or disable EEDP support */
static int disable_eedp = 1;
module_param(disable_eedp, uint, 0);
MODULE_PARM_DESC(disable_eedp, " disable EEDP support: (default=1)");

#if (defined(CONFIG_SUSE_KERNEL) && defined(scsi_is_sas_phy_local)) || LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18)
#define MPT_WIDE_PORT_API	1
#define MPT_WIDE_PORT_API_PLUS	1
#endif

/* raid transport support */
#if !defined(__VMKLNX__)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27))
static struct raid_template *mpt2sas_raid_template;
#endif 
#endif 

/**
 * enum device_responsive_state - responsive state
 * @DEVICE_READY: device is ready to be added
 * @DEVICE_RETRY: device can be retried later
 * @DEVICE_RETRY_UA: retry unit attentions
 * @DEVICE_START_UNIT: requires start unit 
 * @DEVICE_STOP_UNIT: requires stop unit 
 * @DEVICE_ERROR: device reported some fatal error
 *
 * Look at _scsih_wait_for_target_to_become_ready()
 *
 */
enum device_responsive_state {
	DEVICE_READY,
	DEVICE_RETRY,
	DEVICE_RETRY_UA,
	DEVICE_START_UNIT,
	DEVICE_STOP_UNIT,
	DEVICE_ERROR,
};

/**
 * struct sense_info - common structure for obtaining sense keys
 * @skey: sense key
 * @asc: additional sense code
 * @ascq: additional sense code qualifier
 */
struct sense_info {
	u8 skey;
	u8 asc;
	u8 ascq;
};

#ifdef MPT2SAS_MULTIPATH
/**
 * struct mpt2sas_abort_task_set - abort task set
 * @handle: device handle
 * @lun: lun
 */
struct mpt2sas_abort_task_set {
	u16 handle;
	u32 lun;
};
#endif

#define MPT2SAS_TURN_ON_FAULT_LED (0xFFFC)
#define MPT2SAS_ABRT_TASK_SET (0xFFFE)
#define MPT2SAS_RESCAN_AFTER_HOST_RESET (0xFFFF)
/**
 * struct fw_event_work - firmware event struct
 * @list: link list framework
 * @work: work object (ioc->fault_reset_work_q)
 * @cancel_pending_work: flag set during reset handling
 * @ioc: per adapter object
 * @device_handle: device handle
 * @VF_ID: virtual function id
 * @VP_ID: virtual port id
 * @host_reset_handling: handling events during host reset
 * @ignore: flag meaning this event has been marked to ignore
 * @event: firmware event MPI2_EVENT_XXX defined in mpt2_ioc.h
 * @event_data: reply event data payload follows
 * @retries: number of times this event has been retried(for each device)
 *
 * This object stored on ioc->fw_event_list.
 */
struct fw_event_work {
	struct list_head 	list;
	struct work_struct	work;
	u8			cancel_pending_work;
#if (ESX_VERS > ESX41_VERS) || (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,19))
	struct delayed_work	delayed_work;
	u8			delayed_work_active;
#endif
	struct MPT2SAS_ADAPTER *ioc;
	u16			device_handle;
	u8			VF_ID;
	u8			VP_ID;
	u8			host_reset_handling;
	u8			ignore;
	u16			event;
	void			*event_data;
	u8			*retries;
};

/**
 * struct _scsi_io_transfer - scsi io transfer
 * @handle: sas device handle (assigned by firmware)
 * @is_raid: flag set for hidden raid components
 * @dir: DMA_TO_DEVICE, DMA_FROM_DEVICE,
 * @data_length: data transfer length
 * @data_dma: dma pointer to data
 * @sense: sense data
 * @lun: lun number
 * @cdb_length: cdb length
 * @cdb: cdb contents
 * @timeout: timeout for this command
 * @VF_ID: virtual function id
 * @VP_ID: virtual port id
 * @valid_reply: flag set for reply message
 * @sense_length: sense length
 * @ioc_status: ioc status
 * @scsi_state: scsi state
 * @scsi_status: scsi staus
 * @log_info: log information
 * @transfer_length: data length transfer when there is a reply message
 *
 * Used for sending internal scsi commands to devices within this module.
 * Refer to _scsi_send_scsi_io().
 */
struct _scsi_io_transfer {
	u16	handle;
	u8	is_raid;
	enum dma_data_direction dir;
	u32	data_length;
	dma_addr_t data_dma;
	u8 	sense[SCSI_SENSE_BUFFERSIZE];
	u32	lun;
	u8	cdb_length;
	u8	cdb[32];
	u8	timeout;
	u8	VF_ID;
	u8	VP_ID;
	u8	valid_reply;
  /* the following bits are only valid when 'valid_reply = 1' */
	u32	sense_length;
	u16	ioc_status;
	u8	scsi_state;
	u8	scsi_status;
	u32	log_info;
	u32	transfer_length;
};

/*
 * The pci device ids are defined in mpi/mpi2_cnfg.h.
 */
static struct pci_device_id scsih_pci_table[] = {
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SAS2004,
		PCI_ANY_ID, PCI_ANY_ID },
	/* Falcon ~ 2008*/
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SAS2008,
		PCI_ANY_ID, PCI_ANY_ID },
	/* Liberator ~ 2108 */
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SAS2108_1,
		PCI_ANY_ID, PCI_ANY_ID },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SAS2108_2,
		PCI_ANY_ID, PCI_ANY_ID },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SAS2108_3,
		PCI_ANY_ID, PCI_ANY_ID },
	/* Meteor ~ 2116 */
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SAS2116_1,
		PCI_ANY_ID, PCI_ANY_ID },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SAS2116_2,
		PCI_ANY_ID, PCI_ANY_ID },
	/* Thunderbolt ~ 2208 */
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SAS2208_1,
		PCI_ANY_ID, PCI_ANY_ID },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SAS2208_2,
		PCI_ANY_ID, PCI_ANY_ID },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SAS2208_3,
		PCI_ANY_ID, PCI_ANY_ID },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SAS2208_4,
		PCI_ANY_ID, PCI_ANY_ID },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SAS2208_5,
		PCI_ANY_ID, PCI_ANY_ID },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SAS2208_6,
		PCI_ANY_ID, PCI_ANY_ID },
	/* Mustang ~ 2308 */
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SAS2308_1,
		PCI_ANY_ID, PCI_ANY_ID },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SAS2308_2,
		PCI_ANY_ID, PCI_ANY_ID },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SAS2308_3,
		PCI_ANY_ID, PCI_ANY_ID },
	/* SSS6200 */
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SSS6200,
		PCI_ANY_ID, PCI_ANY_ID },
	{0}	/* Terminating entry */
};
MODULE_DEVICE_TABLE(pci, scsih_pci_table);

/**
 * _scsih_set_debug_level - global setting of ioc->logging_level.
 *
 * Note: The logging levels are defined in mpt2sas_debug.h.
 */
#if defined(__VMKLNX__)
int
scsih_set_debug_level(const char *val, struct kernel_param *kp)
#else
static int
_scsih_set_debug_level(const char *val, struct kernel_param *kp)
#endif
{
#if defined(__VMKLNX__)
	/* param_set_int is not defined; see PR 258107 */
	int ret = 0;
#else
	int ret = param_set_int(val, kp);
#endif
	struct MPT2SAS_ADAPTER *ioc;

	if (ret)
		return ret;

	printk(KERN_INFO "setting logging_level(0x%08x)\n", logging_level);
	list_for_each_entry(ioc, &mpt2sas_ioc_list, list)
		ioc->logging_level = logging_level;
	return 0;
}
module_param_call(logging_level, _scsih_set_debug_level, param_get_int,
    &logging_level, 0644);

/**
 * _scsih_srch_boot_sas_address - search based on sas_address
 * @sas_address: sas address
 * @boot_device: boot device object from bios page 2
 *
 * Returns 1 when there's a match, 0 means no match.
 */
static inline int
_scsih_srch_boot_sas_address(u64 sas_address,
    Mpi2BootDeviceSasWwid_t *boot_device)
{
	return (sas_address == le64_to_cpu(boot_device->SASAddress)) ?  1 : 0;
}

/**
 * _scsih_srch_boot_device_name - search based on device name
 * @device_name: device name specified in INDENTIFY fram
 * @boot_device: boot device object from bios page 2
 *
 * Returns 1 when there's a match, 0 means no match.
 */
static inline int
_scsih_srch_boot_device_name(u64 device_name,
    Mpi2BootDeviceDeviceName_t *boot_device)
{
	return (device_name == le64_to_cpu(boot_device->DeviceName)) ? 1 : 0;
}

/**
 * _scsih_srch_boot_encl_slot - search based on enclosure_logical_id/slot
 * @enclosure_logical_id: enclosure logical id
 * @slot_number: slot number
 * @boot_device: boot device object from bios page 2
 *
 * Returns 1 when there's a match, 0 means no match.
 */
static inline int
_scsih_srch_boot_encl_slot(u64 enclosure_logical_id, u16 slot_number,
    Mpi2BootDeviceEnclosureSlot_t *boot_device)
{
	return (enclosure_logical_id == le64_to_cpu(boot_device->
	    EnclosureLogicalID) && slot_number == le16_to_cpu(boot_device->
	    SlotNumber)) ? 1 : 0;
}

/**
 * _scsih_is_boot_device - search for matching boot device.
 * @sas_address: sas address
 * @device_name: device name specified in INDENTIFY fram
 * @enclosure_logical_id: enclosure logical id
 * @slot_number: slot number
 * @form: specifies boot device form
 * @boot_device: boot device object from bios page 2
 *
 * Returns 1 when there's a match, 0 means no match.
 */
static int
_scsih_is_boot_device(u64 sas_address, u64 device_name,
    u64 enclosure_logical_id, u16 slot, u8 form,
    Mpi2BiosPage2BootDevice_t *boot_device)
{
	int rc = 0;

	switch (form) {
	case MPI2_BIOSPAGE2_FORM_SAS_WWID:
		if (!sas_address)
			break;
		rc = _scsih_srch_boot_sas_address(
		    sas_address, &boot_device->SasWwid);
		break;
	case MPI2_BIOSPAGE2_FORM_ENCLOSURE_SLOT:
		if (!enclosure_logical_id)
			break;
		rc = _scsih_srch_boot_encl_slot(
		    enclosure_logical_id,
		    slot, &boot_device->EnclosureSlot);
		break;
	case MPI2_BIOSPAGE2_FORM_DEVICE_NAME:
		if (!device_name)
			break;
		rc = _scsih_srch_boot_device_name(
		    device_name, &boot_device->DeviceName);
		break;
	case MPI2_BIOSPAGE2_FORM_NO_DEVICE_SPECIFIED:
		break;
	}

	return rc;
}

/**
 * _scsih_get_sas_address - set the sas_address for given device handle
 * @handle: device handle
 * @sas_address: sas address
 *
 * Returns 0 success, non-zero when failure
 */
static int
_scsih_get_sas_address(struct MPT2SAS_ADAPTER *ioc, u16 handle,
    u64 *sas_address)
{
	Mpi2SasDevicePage0_t sas_device_pg0;
	Mpi2ConfigReply_t mpi_reply;
	u32 ioc_status;
#if defined(__VMKLNX__)
	u32 device_info;
#endif

	if (handle <= ioc->sas_hba.num_phys) {
		*sas_address = ioc->sas_hba.sas_address;
		return 0;
	} else
		*sas_address = 0;

	if ((mpt2sas_config_get_sas_device_pg0(ioc, &mpi_reply, &sas_device_pg0,
	    MPI2_SAS_DEVICE_PGAD_FORM_HANDLE, handle))) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		return -ENXIO;
	}

	ioc_status = le16_to_cpu(mpi_reply.IOCStatus) &
	    MPI2_IOCSTATUS_MASK;
	if (ioc_status != MPI2_IOCSTATUS_SUCCESS) {
		printk(MPT2SAS_ERR_FMT "handle(0x%04x), ioc_status(0x%04x)"
		    "\nfailure at %s:%d/%s()!\n", ioc->name, handle, ioc_status,
		     __FILE__, __LINE__, __func__);
		return -EIO;
	}

#if defined(__VMKLNX__)
	device_info = le32_to_cpu(sas_device_pg0.DeviceInfo);
	if ((MPI2_SAS_DEVICE_INFO_END_DEVICE ==
	     (device_info & MPI2_SAS_DEVICE_INFO_MASK_DEVICE_TYPE)) &&
	    (device_info & MPI2_SAS_DEVICE_INFO_SATA_DEVICE)) {
		/* SATA end devices */
		return(mpt2sas_scsih_get_sas_address_for_sata_disk(ioc,
			sas_address, handle));
	}
#endif
	*sas_address = le64_to_cpu(sas_device_pg0.SASAddress);
	return 0;
}

/**
 * _scsih_determine_boot_device - determine boot device.
 * @ioc: per adapter object
 * @device: either sas_device or raid_device object
 * @is_raid: [flag] 1 = raid object, 0 = sas object
 *
 * Determines whether this device should be first reported device to
 * to scsi-ml or sas transport, this purpose is for persistant boot device.
 * There are primary, alternate, and current entries in bios page 2. The order
 * priority is primary, alternate, then current.  This routine saves
 * the corresponding device object and is_raid flag in the ioc object.
 * The saved data to be used later in _scsih_probe_boot_devices().
 */
static void
_scsih_determine_boot_device(struct MPT2SAS_ADAPTER *ioc,
    void *device, u8 is_raid)
{
	struct _sas_device *sas_device;
	struct _raid_device *raid_device;
	u64 sas_address;
	u64 device_name;
	u64 enclosure_logical_id;
	u16 slot;

	 /* only process this function when driver loads */
	if (!ioc->wait_for_port_enable_to_complete)
		return;

	if (!is_raid) {
		sas_device = device;
		sas_address = sas_device->sas_address;
		device_name = sas_device->device_name;
		enclosure_logical_id = sas_device->enclosure_logical_id;
		slot = sas_device->slot;
	} else {
		raid_device = device;
		sas_address = raid_device->wwid;
		device_name = 0;
		enclosure_logical_id = 0;
		slot = 0;
	}

	if (!ioc->req_boot_device.device) {
		if (_scsih_is_boot_device(sas_address, device_name,
		    enclosure_logical_id, slot,
		    (ioc->bios_pg2.ReqBootDeviceForm &
		    MPI2_BIOSPAGE2_FORM_MASK),
		    &ioc->bios_pg2.RequestedBootDevice)) {
			dinitprintk(ioc, printk(MPT2SAS_INFO_FMT
			   "%s: req_boot_device(0x%016llx)\n",
			    ioc->name, __func__,
			    (unsigned long long)sas_address));
			ioc->req_boot_device.device = device;
			ioc->req_boot_device.is_raid = is_raid;
		}
	}

	if (!ioc->req_alt_boot_device.device) {
		if (_scsih_is_boot_device(sas_address, device_name,
		    enclosure_logical_id, slot,
		    (ioc->bios_pg2.ReqAltBootDeviceForm &
		    MPI2_BIOSPAGE2_FORM_MASK),
		    &ioc->bios_pg2.RequestedAltBootDevice)) {
			dinitprintk(ioc, printk(MPT2SAS_INFO_FMT
			   "%s: req_alt_boot_device(0x%016llx)\n",
			    ioc->name, __func__,
			    (unsigned long long)sas_address));
			ioc->req_alt_boot_device.device = device;
			ioc->req_alt_boot_device.is_raid = is_raid;
		}
	}

	if (!ioc->current_boot_device.device) {
		if (_scsih_is_boot_device(sas_address, device_name,
		    enclosure_logical_id, slot,
		    (ioc->bios_pg2.CurrentBootDeviceForm &
		    MPI2_BIOSPAGE2_FORM_MASK),
		    &ioc->bios_pg2.CurrentBootDevice)) {
			dinitprintk(ioc, printk(MPT2SAS_INFO_FMT
			   "%s: current_boot_device(0x%016llx)\n",
			    ioc->name, __func__,
			    (unsigned long long)sas_address));
			ioc->current_boot_device.device = device;
			ioc->current_boot_device.is_raid = is_raid;
		}
	}
}

#if defined(__VMKLNX__)
/**
 * mpt2sas_scsih_starget_find_by_sas_address - starget search
 * @ioc: per adapter object
 * @sas_address: sas address
 *
 * Find the starget based on sas_address
 */
struct scsi_target *
mpt2sas_scsih_starget_find_by_sas_address(struct MPT2SAS_ADAPTER *ioc,
    u64 sas_address)
{
	struct MPT2SAS_TARGET *sas_target_priv_data;
	list_for_each_entry(sas_target_priv_data, &ioc->target_list, list) {
		if (sas_address == sas_target_priv_data->sas_address)
			return sas_target_priv_data->starget;
	}
	return NULL;
}
#endif

/**
 * mpt2sas_scsih_sas_device_find_by_sas_address - sas device search
 * @ioc: per adapter object
 * @sas_address: sas address
 * Context: Calling function should acquire ioc->sas_device_lock
 *
 * This searches for sas_device based on sas_address, then return sas_device
 * object.
 */
struct _sas_device *
mpt2sas_scsih_sas_device_find_by_sas_address(struct MPT2SAS_ADAPTER *ioc,
    u64 sas_address)
{
	struct _sas_device *sas_device, *r;

	r = NULL;
	/* then check the sas_device_list */
	list_for_each_entry(sas_device, &ioc->sas_device_list, list) {
#if defined(__VMKLNX__)
		if ((sas_device->sas_address != sas_address) &&
		    (!sas_device->sata_id ||
		     (sas_device->sata_id != sas_address)))
#else
		if (sas_device->sas_address != sas_address)
#endif
			continue;
		r = sas_device;
		goto out;
	}

	/* check the sas_device_init_list */
	list_for_each_entry(sas_device, &ioc->sas_device_init_list, list) {
#if defined(__VMKLNX__)
		if ((sas_device->sas_address != sas_address) &&
		   (!sas_device->sata_id ||
		    (sas_device->sata_id != sas_address)))
#else
		if (sas_device->sas_address != sas_address)
#endif
			continue;
		r = sas_device;
		goto out;
	}

 out:
	return r;
}

/**
 * _scsih_sas_device_find_by_handle - sas device search
 * @ioc: per adapter object
 * @handle: sas device handle (assigned by firmware)
 * Context: Calling function should acquire ioc->sas_device_lock
 *
 * This searches for sas_device based on sas_address, then return sas_device
 * object.
 */
static struct _sas_device *
_scsih_sas_device_find_by_handle(struct MPT2SAS_ADAPTER *ioc, u16 handle)
{
	struct _sas_device *sas_device;

	list_for_each_entry(sas_device, &ioc->sas_device_list, list)
		if (sas_device->handle == handle)
			return sas_device;

	list_for_each_entry(sas_device, &ioc->sas_device_init_list, list)
		if (sas_device->handle == handle)
			return sas_device;

	return NULL;
}

static void free_sas_device(struct kref *kref) {
	struct _sas_device *sas_device = container_of(kref, struct _sas_device,
	    kref);

	kfree(sas_device);
}

static void put_sas_device(struct _sas_device *sas_device) {
	kref_put(&sas_device->kref, free_sas_device);
}

/**
 * _scsih_sas_device_remove - remove sas_device from list.
 * @ioc: per adapter object
 * @sas_device: the sas_device object
 * Context: This function will acquire ioc->sas_device_lock.
 *
 * Removing object and freeing associated memory from the ioc->sas_device_list.
 */
static void
_scsih_sas_device_remove(struct MPT2SAS_ADAPTER *ioc,
    struct _sas_device *sas_device)
{
	unsigned long flags;
	int was_on_sas_device_list = 0;

	if (!sas_device)
		return;

	spin_lock_irqsave(&ioc->sas_device_lock, flags);
	if (!list_empty(&sas_device->list)) {
		list_del_init(&sas_device->list);
		was_on_sas_device_list = 1;
	}
	spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
	if (was_on_sas_device_list) {
		kfree(sas_device->serial_number);
		put_sas_device(sas_device);
	}
}

/**
 * _scsih_device_remove_by_handle - removing device object by handle
 * @ioc: per adapter object
 * @handle: device handle
 *
 * Return nothing.
 */
static void
_scsih_device_remove_by_handle(struct MPT2SAS_ADAPTER *ioc, u16 handle)
{
	struct _sas_device *sas_device;
	unsigned long flags;
	int was_on_sas_device_list = 0;

	if (ioc->shost_recovery)
		return;

	spin_lock_irqsave(&ioc->sas_device_lock, flags);
	sas_device = _scsih_sas_device_find_by_handle(ioc, handle);
	if (sas_device) {
		if (!list_empty(&sas_device->list)) {
			list_del_init(&sas_device->list);
			was_on_sas_device_list = 1;
		}
	}
	spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
	if (was_on_sas_device_list)
		_scsih_remove_device(ioc, sas_device);
}

/**
 * _scsih_device_remove_by_sas_address - removing device object by sas address
 * @ioc: per adapter object
 * @sas_address: device sas_address
 *
 * Return nothing.
 */
static void
_scsih_device_remove_by_sas_address(struct MPT2SAS_ADAPTER *ioc,
    u64 sas_address)
{
	struct _sas_device *sas_device;
	unsigned long flags;
	int was_on_sas_device_list = 0;

	if (ioc->shost_recovery)
		return;

	spin_lock_irqsave(&ioc->sas_device_lock, flags);
	sas_device = mpt2sas_scsih_sas_device_find_by_sas_address(ioc,
	    sas_address);
	if (sas_device) {
		if (!list_empty(&sas_device->list)) {
			list_del_init(&sas_device->list);
			was_on_sas_device_list = 1;
		}
	}
	spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
	if (was_on_sas_device_list)
		_scsih_remove_device(ioc, sas_device);
}

/**
 * _scsih_sas_device_add - insert sas_device to the list.
 * @ioc: per adapter object
 * @sas_device: the sas_device object
 * Context: This function will acquire ioc->sas_device_lock.
 *
 * Adding new object to the ioc->sas_device_list.
 */
static void
_scsih_sas_device_add(struct MPT2SAS_ADAPTER *ioc,
    struct _sas_device *sas_device)
{
	unsigned long flags;

	dewtprintk(ioc, printk(MPT2SAS_INFO_FMT "%s: handle"
	    "(0x%04x), sas_addr(0x%016llx)\n", ioc->name, __func__,
	    sas_device->handle, (unsigned long long)sas_device->sas_address));

	/* Get an extra refcount... */
	kref_get(&sas_device->kref);

	spin_lock_irqsave(&ioc->sas_device_lock, flags);
	list_add_tail(&sas_device->list, &ioc->sas_device_list);
	spin_unlock_irqrestore(&ioc->sas_device_lock, flags);

	if (ioc->hide_drives)
		return;

	if (!mpt2sas_transport_port_add(ioc, sas_device->handle,
	     sas_device->sas_address_parent))
		_scsih_sas_device_remove(ioc, sas_device);
#if defined(__VMKLNX__)
	else {
		/*
		 * Locate the associate starget/vtarget for a
		 * coming-back device
		 */
		if (!sas_device->starget) {
			u64 sas_addr = 0;
			if ((MPI2_SAS_DEVICE_INFO_END_DEVICE ==
			    (sas_device->device_info &
			     MPI2_SAS_DEVICE_INFO_MASK_DEVICE_TYPE)) &&
			    (sas_device->device_info &
			     MPI2_SAS_DEVICE_INFO_SATA_DEVICE)) {
				sas_addr = sas_device->sata_id;
			} else {
				sas_addr = sas_device->sas_address;
			}
			spin_lock_irqsave(&ioc->target_list_lock, flags);
			sas_device->starget =
			    mpt2sas_scsih_starget_find_by_sas_address(ioc,
				sas_addr);
			spin_unlock_irqrestore(&ioc->target_list_lock, flags);
			if (sas_device->starget) {
				struct MPT2SAS_TARGET *sas_target_priv_data
				    = (struct MPT2SAS_TARGET *)
				    sas_device->starget->hostdata;
				sas_device->channel =
				    sas_device->starget->channel;
				sas_device->id = sas_device->starget->id;
				sas_target_priv_data->handle =
				    sas_device->handle;
				if (test_bit(sas_device->handle,
					ioc->pd_handles))
					sas_target_priv_data->flags
					    |= MPT_TARGET_FLAGS_RAID_COMPONENT;
                                else
					sas_target_priv_data->flags
					    &= ~MPT_TARGET_FLAGS_RAID_COMPONENT;
			}
		}
	}
#endif
}

/**
 * _scsih_sas_device_init_add - insert sas_device to the list.
 * @ioc: per adapter object
 * @sas_device: the sas_device object
 * Context: This function will acquire ioc->sas_device_lock.
 *
 * Adding new object at driver load time to the ioc->sas_device_init_list.
 */
static void
_scsih_sas_device_init_add(struct MPT2SAS_ADAPTER *ioc,
    struct _sas_device *sas_device)
{
	unsigned long flags;

	dewtprintk(ioc, printk(MPT2SAS_INFO_FMT "%s: handle"
	    "(0x%04x), sas_addr(0x%016llx)\n", ioc->name, __func__,
	    sas_device->handle, (unsigned long long)sas_device->sas_address));

	kref_get(&sas_device->kref);

	spin_lock_irqsave(&ioc->sas_device_lock, flags);
	list_add_tail(&sas_device->list, &ioc->sas_device_init_list);
	_scsih_determine_boot_device(ioc, sas_device, 0);
	spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
}

/**
 * _scsih_raid_device_find_by_id - raid device search
 * @ioc: per adapter object
 * @id: sas device target id
 * @channel: sas device channel
 * Context: Calling function should acquire ioc->raid_device_lock
 *
 * This searches for raid_device based on target id, then return raid_device
 * object.
 */
#if !defined(__VMKLNX__)
static struct _raid_device *
#else
struct _raid_device *
#endif
_scsih_raid_device_find_by_id(struct MPT2SAS_ADAPTER *ioc, int id, int channel)
{
	struct _raid_device *raid_device, *r;

	r = NULL;
	list_for_each_entry(raid_device, &ioc->raid_device_list, list) {
		if (raid_device->id == id && raid_device->channel == channel) {
			r = raid_device;
			goto out;
		}
	}

 out:
	return r;
}

/**
 * _scsih_raid_device_find_by_handle - raid device search
 * @ioc: per adapter object
 * @handle: sas device handle (assigned by firmware)
 * Context: Calling function should acquire ioc->raid_device_lock
 *
 * This searches for raid_device based on handle, then return raid_device
 * object.
 */
static struct _raid_device *
_scsih_raid_device_find_by_handle(struct MPT2SAS_ADAPTER *ioc, u16 handle)
{
	struct _raid_device *raid_device, *r;

	r = NULL;
	list_for_each_entry(raid_device, &ioc->raid_device_list, list) {
		if (raid_device->handle != handle)
			continue;
		r = raid_device;
		goto out;
	}

 out:
	return r;
}

/**
 * _scsih_raid_device_find_by_wwid - raid device search
 * @ioc: per adapter object
 * @handle: sas device handle (assigned by firmware)
 * Context: Calling function should acquire ioc->raid_device_lock
 *
 * This searches for raid_device based on wwid, then return raid_device
 * object.
 */
static struct _raid_device *
_scsih_raid_device_find_by_wwid(struct MPT2SAS_ADAPTER *ioc, u64 wwid)
{
	struct _raid_device *raid_device, *r;

	r = NULL;
	list_for_each_entry(raid_device, &ioc->raid_device_list, list) {
		if (raid_device->wwid != wwid)
			continue;
		r = raid_device;
		goto out;
	}

 out:
	return r;
}

/**
 * _scsih_raid_device_add - add raid_device object
 * @ioc: per adapter object
 * @raid_device: raid_device object
 *
 * This is added to the raid_device_list link list.
 */
static void
_scsih_raid_device_add(struct MPT2SAS_ADAPTER *ioc,
    struct _raid_device *raid_device)
{
	unsigned long flags;

	dewtprintk(ioc, printk(MPT2SAS_INFO_FMT "%s: handle"
	    "(0x%04x), wwid(0x%016llx)\n", ioc->name, __func__,
	    raid_device->handle, (unsigned long long)raid_device->wwid));

	spin_lock_irqsave(&ioc->raid_device_lock, flags);
	list_add_tail(&raid_device->list, &ioc->raid_device_list);
	spin_unlock_irqrestore(&ioc->raid_device_lock, flags);
}

/**
 * _scsih_raid_device_remove - delete raid_device object
 * @ioc: per adapter object
 * @raid_device: raid_device object
 *
 * This is removed from the raid_device_list link list.
 */
static void
_scsih_raid_device_remove(struct MPT2SAS_ADAPTER *ioc,
    struct _raid_device *raid_device)
{
	unsigned long flags;

	spin_lock_irqsave(&ioc->raid_device_lock, flags);
	list_del(&raid_device->list);
	memset(raid_device, 0, sizeof(struct _raid_device));
	kfree(raid_device);
	spin_unlock_irqrestore(&ioc->raid_device_lock, flags);
}

/**
 * mpt2sas_scsih_expander_find_by_handle - expander device search
 * @ioc: per adapter object
 * @handle: expander handle (assigned by firmware)
 * Context: Calling function should acquire ioc->sas_device_lock
 *
 * This searches for expander device based on handle, then returns the
 * sas_node object.
 */
struct _sas_node *
mpt2sas_scsih_expander_find_by_handle(struct MPT2SAS_ADAPTER *ioc, u16 handle)
{
	struct _sas_node *sas_expander, *r;

	r = NULL;
	list_for_each_entry(sas_expander, &ioc->sas_expander_list, list) {
		if (sas_expander->handle != handle)
			continue;
		r = sas_expander;
		goto out;
	}
 out:
	return r;
}

/**
 * mpt2sas_scsih_expander_find_by_sas_address - expander device search
 * @ioc: per adapter object
 * @sas_address: sas address
 * Context: Calling function should acquire ioc->sas_node_lock.
 *
 * This searches for expander device based on sas_address, then returns the
 * sas_node object.
 */
struct _sas_node *
mpt2sas_scsih_expander_find_by_sas_address(struct MPT2SAS_ADAPTER *ioc,
    u64 sas_address)
{
	struct _sas_node *sas_expander, *r;

	r = NULL;
	list_for_each_entry(sas_expander, &ioc->sas_expander_list, list) {
		if (sas_expander->sas_address != sas_address)
			continue;
		r = sas_expander;
		goto out;
	}
 out:
	return r;
}

/**
 * _scsih_expander_node_add - insert expander device to the list.
 * @ioc: per adapter object
 * @sas_expander: the sas_device object
 * Context: This function will acquire ioc->sas_node_lock.
 *
 * Adding new object to the ioc->sas_expander_list.
 *
 * Return nothing.
 */
static void
_scsih_expander_node_add(struct MPT2SAS_ADAPTER *ioc,
    struct _sas_node *sas_expander)
{
	unsigned long flags;

	spin_lock_irqsave(&ioc->sas_node_lock, flags);
	list_add_tail(&sas_expander->list, &ioc->sas_expander_list);
	spin_unlock_irqrestore(&ioc->sas_node_lock, flags);
}

/**
 * _scsih_is_end_device - determines if device is an end device
 * @device_info: bitfield providing information about the device.
 * Context: none
 *
 * Returns 1 if end device.
 */
static int
_scsih_is_end_device(u32 device_info)
{
	if (device_info & MPI2_SAS_DEVICE_INFO_END_DEVICE &&
		((device_info & MPI2_SAS_DEVICE_INFO_SSP_TARGET) |
		(device_info & MPI2_SAS_DEVICE_INFO_STP_TARGET) |
		(device_info & MPI2_SAS_DEVICE_INFO_SATA_DEVICE)))
		return 1;
	else
		return 0;
}

/**
 * _scsih_scsi_lookup_get - returns scmd entry
 * @ioc: per adapter object
 * @smid: system request message index
 *
 * Returns the smid stored scmd pointer.
 */
static struct scsi_cmnd *
_scsih_scsi_lookup_get(struct MPT2SAS_ADAPTER *ioc, u16 smid)
{
	return ioc->scsi_lookup[smid - 1].scmd;
}

/**
 * _scsih_scsi_lookup_get_clear - returns scmd entry
 * @ioc: per adapter object
 * @smid: system request message index
 *
 * Returns the smid stored scmd pointer.
 * Then will derefrence the stored scmd pointer.
 */
static inline struct scsi_cmnd *
_scsih_scsi_lookup_get_clear(struct MPT2SAS_ADAPTER *ioc, u16 smid)
{
	unsigned long flags;
	struct scsi_cmnd *scmd;

	spin_lock_irqsave(&ioc->scsi_lookup_lock, flags);
	scmd = ioc->scsi_lookup[smid - 1].scmd;
	ioc->scsi_lookup[smid - 1].scmd = NULL;
	spin_unlock_irqrestore(&ioc->scsi_lookup_lock, flags);

	return scmd;
}

/**
 * _scsih_scsi_lookup_find_by_scmd - scmd lookup
 * @ioc: per adapter object
 * @smid: system request message index
 * @scmd: pointer to scsi command object
 * Context: This function will acquire ioc->scsi_lookup_lock.
 *
 * This will search for a scmd pointer in the scsi_lookup array,
 * returning the revelent smid.  A returned value of zero means invalid.
 */
static u16
_scsih_scsi_lookup_find_by_scmd(struct MPT2SAS_ADAPTER *ioc, struct scsi_cmnd
    *scmd)
{
	u16 smid;
	unsigned long	flags;
	int i;

	spin_lock_irqsave(&ioc->scsi_lookup_lock, flags);
	smid = 0;
	for (i = 0; i < ioc->scsiio_depth; i++) {
		if (ioc->scsi_lookup[i].scmd == scmd) {
			smid = ioc->scsi_lookup[i].smid;
			goto out;
		}
	}
 out:
	spin_unlock_irqrestore(&ioc->scsi_lookup_lock, flags);
	return smid;
}

#if defined(__VMKLNX__)
/**
 * _scsih_scsi_lookup_find_by_channel_target_lun - search for matching channel:id:lun
 * @ioc: per adapter object
 * @channel: channel
 * @id: target id or ~0 for wild card
 * @lun: lun or ~0 for wild card 
 * Context: This function will acquire ioc->scsi_lookup_lock.
 *
 * This will search for a matching channel:id:lun in the scsi_lookup array,
 * If id is a wild card, lun is automatically set a wild card.
 * returning 1 if found.
 */
static u8
_scsih_scsi_lookup_find_by_channel_target_lun(struct MPT2SAS_ADAPTER *ioc, int channel,
    int id, int lun)
{
	int i;
	int lun_num;
	u8 found;
	unsigned long	flags;

	found = 0;
	/* Force lun to a wild card if id is a wild card */
	lun_num = (SCAN_WILD_CARD == id) ? SCAN_WILD_CARD : lun;	
	spin_lock_irqsave(&ioc->scsi_lookup_lock, flags);
	for (i = 0 ; i < ioc->scsiio_depth; i++) {
		if (ioc->scsi_lookup[i].scmd &&
		    ((ioc->scsi_lookup[i].scmd->device->channel == channel) &&
		     ((SCAN_WILD_CARD == id) || (ioc->scsi_lookup[i].scmd->device->id == id)) &&
		     ((SCAN_WILD_CARD == lun_num) || (ioc->scsi_lookup[i].scmd->device->lun == lun_num)))) {
			found = 1;
			break;
		}
	}
	spin_unlock_irqrestore(&ioc->scsi_lookup_lock, flags);

	return found;
}
#else
/**
 * _scsih_scsi_lookup_find_by_target - search for matching channel:id
 * @ioc: per adapter object
 * @id: target id
 * @channel: channel
 * Context: This function will acquire ioc->scsi_lookup_lock.
 *
 * This will search for a matching channel:id in the scsi_lookup array,
 * returning 1 if found.
 */
static u8
_scsih_scsi_lookup_find_by_target(struct MPT2SAS_ADAPTER *ioc, int id,
    int channel)
{
	u8 found;
	unsigned long	flags;
	int i;

	spin_lock_irqsave(&ioc->scsi_lookup_lock, flags);
	found = 0;
	for (i = 0 ; i < ioc->scsiio_depth; i++) {
		if (ioc->scsi_lookup[i].scmd &&
		    (ioc->scsi_lookup[i].scmd->device->id == id &&
		    ioc->scsi_lookup[i].scmd->device->channel == channel)) {
			found = 1;
			goto out;
		}
	}
 out:
	spin_unlock_irqrestore(&ioc->scsi_lookup_lock, flags);
	return found;
}
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26))
/**
 * _scsih_scsi_lookup_find_by_lun - search for matching channel:id:lun
 * @ioc: per adapter object
 * @id: target id
 * @lun: lun number
 * @channel: channel
 * Context: This function will acquire ioc->scsi_lookup_lock.
 *
 * This will search for a matching channel:id:lun in the scsi_lookup array,
 * returning 1 if found.
 */
static u8
_scsih_scsi_lookup_find_by_lun(struct MPT2SAS_ADAPTER *ioc, int id,
    unsigned int lun, int channel)
{
	u8 found;
	unsigned long	flags;
	int i;

	spin_lock_irqsave(&ioc->scsi_lookup_lock, flags);
	found = 0;
	for (i = 0 ; i < ioc->scsiio_depth; i++) {
		if (ioc->scsi_lookup[i].scmd &&
		    (ioc->scsi_lookup[i].scmd->device->id == id &&
		    ioc->scsi_lookup[i].scmd->device->channel == channel &&
		    ioc->scsi_lookup[i].scmd->device->lun == lun)) {
			found = 1;
			goto out;
		}
	}
 out:
	spin_unlock_irqrestore(&ioc->scsi_lookup_lock, flags);
	return found;
}
#endif

/**
 * _scsih_get_chain_buffer_tracker - obtain chain tracker
 * @ioc: per adapter object
 * @smid: smid associated to an IO request
 *
 * Returns chain tracker(from ioc->free_chain_list)
 */
static struct chain_tracker *
_scsih_get_chain_buffer_tracker(struct MPT2SAS_ADAPTER *ioc, u16 smid)
{
	struct chain_tracker *chain_req;
	unsigned long flags;

	spin_lock_irqsave(&ioc->scsi_lookup_lock, flags);
	if (list_empty(&ioc->free_chain_list)) {
		spin_unlock_irqrestore(&ioc->scsi_lookup_lock, flags);
		printk(MPT2SAS_WARN_FMT "chain buffers not available\n",
		    ioc->name);
		return NULL;
	}
	chain_req = list_entry(ioc->free_chain_list.next,
	    struct chain_tracker, tracker_list);
	list_del_init(&chain_req->tracker_list);
	list_add_tail(&chain_req->tracker_list,
	    &ioc->scsi_lookup[smid - 1].chain_list);
	spin_unlock_irqrestore(&ioc->scsi_lookup_lock, flags);
	return chain_req;
}

/**
 * _scsih_build_scatter_gather - main sg creation routine
 * @ioc: per adapter object
 * @scmd: scsi command
 * @smid: system request message index
 * Context: none.
 *
 * The main routine that builds scatter gather table from a given
 * scsi request sent via the .queuecommand main handler.
 *
 * Returns 0 success, anything else error
 */
static int
_scsih_build_scatter_gather(struct MPT2SAS_ADAPTER *ioc,
    struct scsi_cmnd *scmd, u16 smid)
{
	Mpi2SCSIIORequest_t *mpi_request;
	dma_addr_t chain_dma;
	struct scatterlist *sg_scmd;
	void *sg_local, *chain;
	u32 chain_offset;
	u32 chain_length;
	u32 chain_flags;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23))
	u32 sges_left;
#else
	int sges_left;
#endif
	u32 sges_in_segment;
	u32 sgl_flags;
	u32 sgl_flags_last_element;
	u32 sgl_flags_end_buffer;
	struct chain_tracker *chain_req;

	mpi_request = mpt2sas_base_get_msg_frame(ioc, smid);

	/* init scatter gather flags */
	sgl_flags = MPI2_SGE_FLAGS_SIMPLE_ELEMENT;
	if (scmd->sc_data_direction == DMA_TO_DEVICE)
		sgl_flags |= MPI2_SGE_FLAGS_HOST_TO_IOC;
	sgl_flags_last_element = (sgl_flags | MPI2_SGE_FLAGS_LAST_ELEMENT)
	    << MPI2_SGE_FLAGS_SHIFT;
	sgl_flags_end_buffer = (sgl_flags | MPI2_SGE_FLAGS_LAST_ELEMENT |
	    MPI2_SGE_FLAGS_END_OF_BUFFER | MPI2_SGE_FLAGS_END_OF_LIST)
	    << MPI2_SGE_FLAGS_SHIFT;
	sgl_flags = sgl_flags << MPI2_SGE_FLAGS_SHIFT;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23))
	/* single buffer sge */
	if (!scmd->use_sg) {
		scmd->SCp.dma_handle = pci_map_single(ioc->pdev,
		    scmd->request_buffer, scmd->request_bufflen,
		    scmd->sc_data_direction);
		if (pci_dma_mapping_error(scmd->SCp.dma_handle)) {
			sdev_printk(KERN_ERR, scmd->device, "pci_map_single"
			" failed: request for %d bytes!\n",
			scmd->request_bufflen);
			return -ENOMEM;
		}
		ioc->base_add_sg_single(&mpi_request->SGL,
		    sgl_flags_end_buffer | scmd->request_bufflen,
		    scmd->SCp.dma_handle);
		return 0;
	}

	/* sg list provided */
	sg_scmd = (struct scatterlist *) scmd->request_buffer;
	sges_left = pci_map_sg(ioc->pdev, sg_scmd, scmd->use_sg,
	    scmd->sc_data_direction);

#if defined(CRACK_MONKEY_EEDP) 
	if (!ioc->disable_eedp_support) {
		if (scmd->cmnd[0] == INQUIRY) {
			scmd->host_scribble =
				page_address(((struct scatterlist *)
					scmd->request_buffer)[0].page)+
				      ((struct scatterlist *)
				       scmd->request_buffer)[0].offset;
		}
	}

#endif /* CRACK_MONKEY_EEDP */
	if (!sges_left) {
		sdev_printk(KERN_ERR, scmd->device, "pci_map_sg"
		" failed: request for %d bytes!\n", scmd->request_bufflen);
		return -ENOMEM;
	}
#else
	sg_scmd = scsi_sglist(scmd);
	sges_left = scsi_dma_map(scmd);
	if (sges_left < 0) {
		sdev_printk(KERN_ERR, scmd->device, "pci_map_sg"
		" failed: request for %d bytes!\n", scsi_bufflen(scmd));
		return -ENOMEM;
	}

#if defined(CRACK_MONKEY_EEDP)
	if (!ioc->disable_eedp_support) {
		if (scmd->cmnd[0] == INQUIRY)
			scmd->host_scribble = page_address(sg_page(sg_scmd)) +
				sg_scmd[0].offset;
	}
#endif /* CRACK_MONKEY_EEDP */
#endif


	sg_local = &mpi_request->SGL;
	sges_in_segment = ioc->max_sges_in_main_message;
	if (sges_left <= sges_in_segment)
		goto fill_in_last_segment;

	mpi_request->ChainOffset = (offsetof(Mpi2SCSIIORequest_t, SGL) +
	    (sges_in_segment * ioc->sge_size))/4;

	/* fill in main message segment when there is a chain following */
	while (sges_in_segment) {
		if (sges_in_segment == 1)
			ioc->base_add_sg_single(sg_local,
			    sgl_flags_last_element | sg_dma_len(sg_scmd),
			    sg_dma_address(sg_scmd));
		else
			ioc->base_add_sg_single(sg_local, sgl_flags |
			    sg_dma_len(sg_scmd), sg_dma_address(sg_scmd));
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24))
#if defined(__VMKLNX__)
		sg_scmd = sg_next(sg_scmd);
#else
		sg_scmd++;
#endif
#else
		sg_scmd = sg_next(sg_scmd);
#endif
		sg_local += ioc->sge_size;
		sges_left--;
		sges_in_segment--;
	}

	/* initializing the chain flags and pointers */
	chain_flags = MPI2_SGE_FLAGS_CHAIN_ELEMENT << MPI2_SGE_FLAGS_SHIFT;
	chain_req = _scsih_get_chain_buffer_tracker(ioc, smid);
	if (!chain_req)
		return -1;
	chain = chain_req->chain_buffer;
	chain_dma = chain_req->chain_buffer_dma;
	do {
		sges_in_segment = (sges_left <=
		    ioc->max_sges_in_chain_message) ? sges_left :
		    ioc->max_sges_in_chain_message;
		chain_offset = (sges_left == sges_in_segment) ?
		    0 : (sges_in_segment * ioc->sge_size)/4;
		chain_length = sges_in_segment * ioc->sge_size;
		if (chain_offset) {
			chain_offset = chain_offset <<
			    MPI2_SGE_CHAIN_OFFSET_SHIFT;
			chain_length += ioc->sge_size;
		}
		ioc->base_add_sg_single(sg_local, chain_flags | chain_offset |
		    chain_length, chain_dma);
		sg_local = chain;
		if (!chain_offset)
			goto fill_in_last_segment;

		/* fill in chain segments */
		while (sges_in_segment) {
			if (sges_in_segment == 1)
				ioc->base_add_sg_single(sg_local,
				    sgl_flags_last_element |
				    sg_dma_len(sg_scmd),
				    sg_dma_address(sg_scmd));
			else
				ioc->base_add_sg_single(sg_local, sgl_flags |
				    sg_dma_len(sg_scmd),
				    sg_dma_address(sg_scmd));
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24))
#if defined(__VMKLNX__)
			sg_scmd = sg_next(sg_scmd);
#else
			sg_scmd++;
#endif
#else
			sg_scmd = sg_next(sg_scmd);
#endif
			sg_local += ioc->sge_size;
			sges_left--;
			sges_in_segment--;
		}

		chain_req = _scsih_get_chain_buffer_tracker(ioc, smid);
		if (!chain_req)
			return -1;
		chain = chain_req->chain_buffer;
		chain_dma = chain_req->chain_buffer_dma;
	} while (1);


 fill_in_last_segment:

	/* fill the last segment */
	while (sges_left) {
		if (sges_left == 1)
			ioc->base_add_sg_single(sg_local, sgl_flags_end_buffer |
			    sg_dma_len(sg_scmd), sg_dma_address(sg_scmd));
		else
			ioc->base_add_sg_single(sg_local, sgl_flags |
			    sg_dma_len(sg_scmd), sg_dma_address(sg_scmd));
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24))
#if defined(__VMKLNX__)
		sg_scmd = sg_next(sg_scmd);
#else
		sg_scmd++;
#endif
#else
		sg_scmd = sg_next(sg_scmd);
#endif
		sg_local += ioc->sge_size;
		sges_left--;
	}

	return 0;
}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32))
static void
_scsih_adjust_queue_depth(struct scsi_device *sdev, int qdepth)
{
	struct Scsi_Host *shost = sdev->host;
	int max_depth;
	struct MPT2SAS_ADAPTER *ioc = shost_private(shost);
	struct MPT2SAS_DEVICE *sas_device_priv_data;
	struct MPT2SAS_TARGET *sas_target_priv_data;
	struct _sas_device *sas_device;
	unsigned long flags;

	max_depth = shost->can_queue;

	/* limit max device queue for SATA to 32 */
	sas_device_priv_data = sdev->hostdata;
	if (!sas_device_priv_data)
		goto not_sata;
	sas_target_priv_data = sas_device_priv_data->sas_target;
	if (!sas_target_priv_data)
		goto not_sata;
	if ((sas_target_priv_data->flags & MPT_TARGET_FLAGS_VOLUME))
		goto not_sata;
	spin_lock_irqsave(&ioc->sas_device_lock, flags);
	sas_device = mpt2sas_scsih_sas_device_find_by_sas_address(ioc,
	   sas_device_priv_data->sas_target->sas_address);
	if (sas_device && sas_device->device_info &
	    MPI2_SAS_DEVICE_INFO_SATA_DEVICE)
		max_depth = MPT2SAS_SATA_QUEUE_DEPTH;
	spin_unlock_irqrestore(&ioc->sas_device_lock, flags);

 not_sata:

	if (!sdev->tagged_supported)
		max_depth = 1;
	if (qdepth > max_depth)
		qdepth = max_depth;
	scsi_adjust_queue_depth(sdev, scsi_get_tag_type(sdev), qdepth);
}

/**
 * _scsih_change_queue_depth - setting device queue depth
 * @sdev: scsi device struct
 * @qdepth: requested queue depth
 * @reason: SCSI_QDEPTH_DEFAULT/SCSI_QDEPTH_QFULL/SCSI_QDEPTH_RAMP_UP
 * (see include/scsi/scsi_host.h for definition)
 *
 * Returns queue depth.
 */
static int
_scsih_change_queue_depth(struct scsi_device *sdev, int qdepth, int reason)
{
	if (reason == SCSI_QDEPTH_DEFAULT || reason == SCSI_QDEPTH_RAMP_UP)
		_scsih_adjust_queue_depth(sdev, qdepth);
	else if (reason == SCSI_QDEPTH_QFULL)
		scsi_track_queue_full(sdev, qdepth);
	else
		return -EOPNOTSUPP;

	if (sdev->inquiry_len > 7)
		sdev_printk(KERN_INFO, sdev, "qdepth(%d), tagged(%d), "
		"simple(%d), ordered(%d), scsi_level(%d), cmd_que(%d)\n",
		sdev->queue_depth, sdev->tagged_supported, sdev->simple_tags,
		sdev->ordered_tags, sdev->scsi_level,
		(sdev->inquiry[7] & 2) >> 1);

	return sdev->queue_depth;
}
#else
/**
 * _scsih_change_queue_depth - setting device queue depth
 * @sdev: scsi device struct
 * @qdepth: requested queue depth
 *
 * Returns queue depth.
 */
static int
_scsih_change_queue_depth(struct scsi_device *sdev, int qdepth)
{
	struct Scsi_Host *shost = sdev->host;
	int max_depth;
	int tag_type;
	struct MPT2SAS_ADAPTER *ioc = shost_private(shost);
	struct MPT2SAS_DEVICE *sas_device_priv_data;
	struct MPT2SAS_TARGET *sas_target_priv_data;
	struct _sas_device *sas_device;
	unsigned long flags;

	max_depth = shost->can_queue;

	/* limit max device queue for SATA to 32 */
	sas_device_priv_data = sdev->hostdata;
	if (!sas_device_priv_data)
		goto not_sata;
	sas_target_priv_data = sas_device_priv_data->sas_target;
	if (!sas_target_priv_data)
		goto not_sata;
	if ((sas_target_priv_data->flags & MPT_TARGET_FLAGS_VOLUME))
		goto not_sata;
	spin_lock_irqsave(&ioc->sas_device_lock, flags);
	sas_device = mpt2sas_scsih_sas_device_find_by_sas_address(ioc,
	   sas_device_priv_data->sas_target->sas_address);
	if (sas_device && sas_device->device_info &
	    MPI2_SAS_DEVICE_INFO_SATA_DEVICE)
		max_depth = MPT2SAS_SATA_QUEUE_DEPTH;
	spin_unlock_irqrestore(&ioc->sas_device_lock, flags);

 not_sata:

	if (!sdev->tagged_supported)
		max_depth = 1;
	if (qdepth > max_depth)
		qdepth = max_depth;
	tag_type = (qdepth == 1) ? 0 : MSG_SIMPLE_TAG;
	scsi_adjust_queue_depth(sdev, tag_type, qdepth);

	if (sdev->inquiry_len > 7)
		sdev_printk(KERN_INFO, sdev, "qdepth(%d), tagged(%d), "
		"simple(%d), ordered(%d), scsi_level(%d), cmd_que(%d)\n",
		sdev->queue_depth, sdev->tagged_supported, sdev->simple_tags,
		sdev->ordered_tags, sdev->scsi_level,
		(sdev->inquiry[7] & 2) >> 1);

	return sdev->queue_depth;
}
#endif

/**
 * _scsih_change_queue_type - changing device queue tag type
 * @sdev: scsi device struct
 * @tag_type: requested tag type
 *
 * Returns queue tag type.
 */
static int
_scsih_change_queue_type(struct scsi_device *sdev, int tag_type)
{
	if (sdev->tagged_supported) {
		scsi_set_tag_type(sdev, tag_type);
		if (tag_type)
			scsi_activate_tcq(sdev, sdev->queue_depth);
		else
			scsi_deactivate_tcq(sdev, sdev->queue_depth);
	} else
		tag_type = 0;

	return tag_type;
}

/**
 * _scsih_target_alloc - target add routine
 * @starget: scsi target struct
 *
 * Returns 0 if ok. Any other return is assumed to be an error and
 * the device is ignored.
 */
static int
_scsih_target_alloc(struct scsi_target *starget)
{
	struct Scsi_Host *shost = dev_to_shost(&starget->dev);
	struct MPT2SAS_ADAPTER *ioc = shost_private(shost);
	struct MPT2SAS_TARGET *sas_target_priv_data;
	struct _sas_device *sas_device;
	struct _raid_device *raid_device;
	unsigned long flags;
	struct sas_rphy *rphy;

	sas_target_priv_data = kzalloc(sizeof(struct scsi_target), GFP_KERNEL);
	if (!sas_target_priv_data)
		return -ENOMEM;

	starget->hostdata = sas_target_priv_data;
	sas_target_priv_data->starget = starget;
	sas_target_priv_data->handle = MPT2SAS_INVALID_DEVICE_HANDLE;

	/* RAID volumes */
	if (starget->channel == RAID_CHANNEL) {
		spin_lock_irqsave(&ioc->raid_device_lock, flags);
		raid_device = _scsih_raid_device_find_by_id(ioc, starget->id,
		    starget->channel);
		if (raid_device) {
			sas_target_priv_data->handle = raid_device->handle;
			sas_target_priv_data->sas_address = raid_device->wwid;
			sas_target_priv_data->flags |= MPT_TARGET_FLAGS_VOLUME;
			if (ioc->is_warpdrive)
				sas_target_priv_data->raid_device = raid_device;
			raid_device->starget = starget;
		}
#if defined(__VMKLNX__)
		else {
			spin_unlock_irqrestore(&ioc->raid_device_lock, flags);
			kfree(sas_target_priv_data);
			starget->hostdata = NULL;
			return -ENXIO;
		}
#endif
		spin_unlock_irqrestore(&ioc->raid_device_lock, flags);
#if defined(__VMKLNX__)
		goto out;
#else
		return 0;
#endif
	}

	/* sas/sata devices */
	spin_lock_irqsave(&ioc->sas_device_lock, flags);
	rphy = dev_to_rphy(starget->dev.parent);
	sas_device = mpt2sas_scsih_sas_device_find_by_sas_address(ioc,
	   rphy->identify.sas_address);

	if (sas_device) {
		sas_target_priv_data->handle = sas_device->handle;
#if defined(__VMKLNX__)
		if ((MPI2_SAS_DEVICE_INFO_END_DEVICE ==
		    (sas_device->device_info & MPI2_SAS_DEVICE_INFO_MASK_DEVICE_TYPE)) &&
		    (sas_device->device_info & MPI2_SAS_DEVICE_INFO_SATA_DEVICE)) { 
			sas_target_priv_data->sas_address = sas_device->sata_id;
		} else {
			sas_target_priv_data->sas_address = sas_device->sas_address;
		}
#else
		sas_target_priv_data->sas_address = sas_device->sas_address;
#endif
		sas_device->starget = starget;
		sas_device->id = starget->id;
		sas_device->channel = starget->channel;
		if (test_bit(sas_device->handle, ioc->pd_handles))
			sas_target_priv_data->flags |=
			    MPT_TARGET_FLAGS_RAID_COMPONENT;
	}
#if defined(__VMKLNX__)
	else {
		spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
		kfree(sas_target_priv_data);
		starget->hostdata = NULL;
		return -ENXIO;
	}
#endif
	spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
#if defined(__VMKLNX__)
out:
	/* Add it to target_list */
	spin_lock_irqsave(&ioc->target_list_lock, flags);
	list_add_tail(&sas_target_priv_data->list, &ioc->target_list);
	spin_unlock_irqrestore(&ioc->target_list_lock, flags);
#endif
	return 0;
}

/**
 * _scsih_target_destroy - target destroy routine
 * @starget: scsi target struct
 *
 * Returns nothing.
 */
static void
_scsih_target_destroy(struct scsi_target *starget)
{
	struct Scsi_Host *shost = dev_to_shost(&starget->dev);
	struct MPT2SAS_ADAPTER *ioc = shost_private(shost);
	struct MPT2SAS_TARGET *sas_target_priv_data;
	struct _sas_device *sas_device;
	struct _raid_device *raid_device;
	unsigned long flags;
	struct sas_rphy *rphy;

	sas_target_priv_data = starget->hostdata;
	if (!sas_target_priv_data)
		return;

	if (starget->channel == RAID_CHANNEL) {
		spin_lock_irqsave(&ioc->raid_device_lock, flags);
		raid_device = _scsih_raid_device_find_by_id(ioc, starget->id,
		    starget->channel);
		if (raid_device) {
			raid_device->starget = NULL;
			raid_device->sdev = NULL;
		}
		spin_unlock_irqrestore(&ioc->raid_device_lock, flags);
		goto out;
	}

	spin_lock_irqsave(&ioc->sas_device_lock, flags);
	rphy = dev_to_rphy(starget->dev.parent);
	sas_device = mpt2sas_scsih_sas_device_find_by_sas_address(ioc,
	   rphy->identify.sas_address);
	if (sas_device && (sas_device->starget == starget) &&
	    (sas_device->id == starget->id) &&
	    (sas_device->channel == starget->channel))
		sas_device->starget = NULL;

	spin_unlock_irqrestore(&ioc->sas_device_lock, flags);

 out:
#if defined(__VMKLNX__)
	/* Remove it from target_list */
	spin_lock_irqsave(&ioc->target_list_lock, flags);
	list_del(&sas_target_priv_data->list);
	spin_unlock_irqrestore(&ioc->target_list_lock, flags);
#endif
	kfree(sas_target_priv_data);
	starget->hostdata = NULL;
}

/**
 * _scsih_slave_alloc - device add routine
 * @sdev: scsi device struct
 *
 * Returns 0 if ok. Any other return is assumed to be an error and
 * the device is ignored.
 */
static int
_scsih_slave_alloc(struct scsi_device *sdev)
{
	struct Scsi_Host *shost;
	struct MPT2SAS_ADAPTER *ioc;
	struct MPT2SAS_TARGET *sas_target_priv_data;
	struct MPT2SAS_DEVICE *sas_device_priv_data;
	struct scsi_target *starget;
	struct _raid_device *raid_device;
	struct _sas_device *sas_device;
	unsigned long flags;

	starget = scsi_target(sdev);
	sas_target_priv_data = starget->hostdata;
#if defined(__VMKLNX__)
	/*
	 * If a RAID-member target is already configured, fail slave_alloc here
	 * so the upper layer will not try to create/configure/claim a path for
	 * a RAID-member disk target during periodic probe.
	 */
	if (sas_target_priv_data && sas_target_priv_data->configured)
		return -ENXIO;
#endif
	sas_device_priv_data = kzalloc(sizeof(struct scsi_device), GFP_KERNEL);
	if (!sas_device_priv_data)
		return -ENOMEM;

	sas_device_priv_data->lun = sdev->lun;
	sas_device_priv_data->flags = MPT_DEVICE_FLAGS_INIT;

	sas_target_priv_data->num_luns++;
	sas_device_priv_data->sas_target = sas_target_priv_data;
	sdev->hostdata = sas_device_priv_data;
	if ((sas_target_priv_data->flags & MPT_TARGET_FLAGS_RAID_COMPONENT))
		sdev->no_uld_attach = 1;

	shost = dev_to_shost(&starget->dev);
	ioc = shost_private(shost);
	if (starget->channel == RAID_CHANNEL) {
		spin_lock_irqsave(&ioc->raid_device_lock, flags);
		raid_device = _scsih_raid_device_find_by_id(ioc,
		    starget->id, starget->channel);
		if (raid_device)
			raid_device->sdev = sdev; /* raid is single lun */
		spin_unlock_irqrestore(&ioc->raid_device_lock, flags);

		if (!(sas_target_priv_data->flags & MPT_TARGET_FLAGS_VOLUME)) {
			spin_lock_irqsave(&ioc->sas_device_lock, flags);
			sas_device = mpt2sas_scsih_sas_device_find_by_sas_address(ioc,
			    sas_target_priv_data->sas_address);
			if (sas_device && (sas_device->starget == NULL)) {
				sdev_printk(KERN_INFO, sdev, "%s : sas_device->starget set to starget @ %d\n", __func__, __LINE__);
				sas_device->starget = starget;
			}
			spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
		}

#if defined(__VMKLNX__)
	} else {

		spin_lock_irqsave(&ioc->sas_device_lock, flags);
		sas_device = mpt2sas_scsih_sas_device_find_by_sas_address(ioc,
		   sas_device_priv_data->sas_target->sas_address);

		if (sas_device && (sas_device->device_info &
		    MPI2_SAS_DEVICE_INFO_SATA_DEVICE)) {
			if (!ioc->sata_dev_found)
				ioc->sata_dev_found = 1;
			sas_device_priv_data->flags |= MPT_DEVICE_SATA;
		}
		if (!(sas_target_priv_data->flags & MPT_TARGET_FLAGS_VOLUME)) {
			if (sas_device && (sas_device->starget == NULL)) {
				sdev_printk(KERN_INFO, sdev, "%s : sas_device->starget set to starget @ %d\n", __func__, __LINE__);
				sas_device->starget = starget;
			}
		}
		spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
#endif
	}

	return 0;
}

/**
 * _scsih_slave_destroy - device destroy routine
 * @sdev: scsi device struct
 *
 * Returns nothing.
 */
static void
_scsih_slave_destroy(struct scsi_device *sdev)
{
	struct MPT2SAS_TARGET *sas_target_priv_data;
	struct scsi_target *starget;

	if (!sdev->hostdata)
		return;

	starget = scsi_target(sdev);
	sas_target_priv_data = starget->hostdata;
	sas_target_priv_data->num_luns--;
#if defined(__VMKLNX__)
        /* Clear nexus_loss and deleted when the last lun on the target is destroyed */
	if (!sas_target_priv_data->num_luns) {
		sas_target_priv_data->deleted = 0;
		sas_target_priv_data->nexus_loss = 0;
	}
#endif
	kfree(sdev->hostdata);
	sdev->hostdata = NULL;
}

/**
 * _scsih_display_sata_capabilities - sata capabilities
 * @ioc: per adapter object
 * @sas_device: the sas_device object
 * @sdev: scsi device struct
 */
static void
_scsih_display_sata_capabilities(struct MPT2SAS_ADAPTER *ioc,
    u16 handle, struct scsi_device *sdev)
{
	Mpi2ConfigReply_t mpi_reply;
	Mpi2SasDevicePage0_t sas_device_pg0;
	u32 ioc_status;
	u16 flags;
	u32 device_info;

	if ((mpt2sas_config_get_sas_device_pg0(ioc, &mpi_reply, &sas_device_pg0,
	    MPI2_SAS_DEVICE_PGAD_FORM_HANDLE, handle))) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		return;
	}

	ioc_status = le16_to_cpu(mpi_reply.IOCStatus) &
	    MPI2_IOCSTATUS_MASK;
	if (ioc_status != MPI2_IOCSTATUS_SUCCESS) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		return;
	}

	flags = le16_to_cpu(sas_device_pg0.Flags);
	device_info = le32_to_cpu(sas_device_pg0.DeviceInfo);

	sdev_printk(KERN_INFO, sdev,
	    "atapi(%s), ncq(%s), asyn_notify(%s), smart(%s), fua(%s), "
	    "sw_preserve(%s)\n",
	    (device_info & MPI2_SAS_DEVICE_INFO_ATAPI_DEVICE) ? "y" : "n",
	    (flags & MPI2_SAS_DEVICE0_FLAGS_SATA_NCQ_SUPPORTED) ? "y" : "n",
	    (flags & MPI2_SAS_DEVICE0_FLAGS_SATA_ASYNCHRONOUS_NOTIFY) ? "y" :
	    "n",
	    (flags & MPI2_SAS_DEVICE0_FLAGS_SATA_SMART_SUPPORTED) ? "y" : "n",
	    (flags & MPI2_SAS_DEVICE0_FLAGS_SATA_FUA_SUPPORTED) ? "y" : "n",
	    (flags & MPI2_SAS_DEVICE0_FLAGS_SATA_SW_PRESERVE) ? "y" : "n");
}

#if !defined(__VMKLNX__)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27))
/*
 * raid transport support -
 * Enabled for SLES11 and newer, in older kernels the driver will panic when
 * unloading the driver followed by a load - I beleive that the subroutine
 * raid_class_release() is not cleaning up properly.
 */

/**
 * _scsih_is_raid - return boolean indicating device is raid volume
 * @dev the device struct object
 */
static int
_scsih_is_raid(struct device *dev)
{
	struct scsi_device *sdev = to_scsi_device(dev);
	struct MPT2SAS_ADAPTER *ioc = shost_private(sdev->host);
 
	if (ioc->is_warpdrive)
		return 0;
	return (sdev->channel == RAID_CHANNEL) ? 1 : 0;
}

/**
 * _scsih_get_resync - get raid volume resync percent complete
 * @dev the device struct object
 */
static void
_scsih_get_resync(struct device *dev)
{
	struct scsi_device *sdev = to_scsi_device(dev);
	struct MPT2SAS_ADAPTER *ioc = shost_private(sdev->host);
	static struct _raid_device *raid_device;
	unsigned long flags;
	Mpi2RaidVolPage0_t vol_pg0;
	Mpi2ConfigReply_t mpi_reply;
	u32 volume_status_flags;
	u8 percent_complete;
	u8 handle;

	percent_complete = 0;
	handle = 0;
	if (ioc->is_warpdrive)
		goto out;

	spin_lock_irqsave(&ioc->raid_device_lock, flags);
	raid_device = _scsih_raid_device_find_by_id(ioc, sdev->id,
	    sdev->channel);
	if (raid_device) {
		handle = raid_device->handle;
		percent_complete = raid_device->percent_complete;
	}
	spin_unlock_irqrestore(&ioc->raid_device_lock, flags);

	if (!handle)
		goto out;

	if (mpt2sas_config_get_raid_volume_pg0(ioc, &mpi_reply, &vol_pg0,
	     MPI2_RAID_VOLUME_PGAD_FORM_HANDLE, handle,
	     sizeof(Mpi2RaidVolPage0_t))) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		percent_complete = 0;
		goto out;
	}

	volume_status_flags = le32_to_cpu(vol_pg0.VolumeStatusFlags);
	if (!(volume_status_flags &
		MPI2_RAIDVOL0_STATUS_FLAG_RESYNC_IN_PROGRESS))
		percent_complete = 0;

 out:
	raid_set_resync(mpt2sas_raid_template, dev, percent_complete);
}

/**
 * _scsih_get_state - get raid volume level
 * @dev the device struct object
 */
static void
_scsih_get_state(struct device *dev)
{
	struct scsi_device *sdev = to_scsi_device(dev);
	struct MPT2SAS_ADAPTER *ioc = shost_private(sdev->host);
	static struct _raid_device *raid_device;
	unsigned long flags;
	Mpi2RaidVolPage0_t vol_pg0;
	Mpi2ConfigReply_t mpi_reply;
	u32 volstate;
	enum raid_state state = RAID_STATE_UNKNOWN;
	u16 handle = 0;

	spin_lock_irqsave(&ioc->raid_device_lock, flags);
	raid_device = _scsih_raid_device_find_by_id(ioc, sdev->id,
	    sdev->channel);
	if (raid_device)
		handle = raid_device->handle;
	spin_unlock_irqrestore(&ioc->raid_device_lock, flags);

	if (!handle)
		goto out;

	if (mpt2sas_config_get_raid_volume_pg0(ioc, &mpi_reply, &vol_pg0,
	     MPI2_RAID_VOLUME_PGAD_FORM_HANDLE, handle,
	     sizeof(Mpi2RaidVolPage0_t))) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		goto out;
	}

	volstate = le32_to_cpu(vol_pg0.VolumeStatusFlags);
	if (volstate & MPI2_RAIDVOL0_STATUS_FLAG_RESYNC_IN_PROGRESS) {
		state = RAID_STATE_RESYNCING;
		goto out;
	}

	switch (vol_pg0.VolumeState) {
	case MPI2_RAID_VOL_STATE_OPTIMAL:
	case MPI2_RAID_VOL_STATE_ONLINE:
		state = RAID_STATE_ACTIVE;
		break;
	case  MPI2_RAID_VOL_STATE_DEGRADED:
		state = RAID_STATE_DEGRADED;
		break;
	case MPI2_RAID_VOL_STATE_FAILED:
	case MPI2_RAID_VOL_STATE_MISSING:
		state = RAID_STATE_OFFLINE;
		break;
	}
 out:
	raid_set_state(mpt2sas_raid_template, dev, state);
}

/**
 * _scsih_set_level - set raid level
 * @sdev: scsi device struct
 * @volume_type: volume type
 */
static void
_scsih_set_level(struct scsi_device *sdev, u8 volume_type)
{
	enum raid_level level = RAID_LEVEL_UNKNOWN;

	switch (volume_type) {
	case MPI2_RAID_VOL_TYPE_RAID0:
		level = RAID_LEVEL_0;
		break;
	case MPI2_RAID_VOL_TYPE_RAID10:
	case MPI2_RAID_VOL_TYPE_RAID1E:
		level = RAID_LEVEL_10;
		break;
	case MPI2_RAID_VOL_TYPE_RAID1:
		level = RAID_LEVEL_1;
		break;
	}

	raid_set_level(mpt2sas_raid_template, &sdev->sdev_gendev, level);
}
#endif /* raid transport support - (2.6.27 and newer) */
#endif /* raid transport support - (2.6.27 and newer) */

/**
 * _scsih_get_volume_capabilities - volume capabilities
 * @ioc: per adapter object
 * @sas_device: the raid_device object
 *
 * Returns 0 for success, else 1
 */
static int
_scsih_get_volume_capabilities(struct MPT2SAS_ADAPTER *ioc,
    struct _raid_device *raid_device)
{
	Mpi2RaidVolPage0_t *vol_pg0;
	Mpi2RaidPhysDiskPage0_t pd_pg0;
	Mpi2SasDevicePage0_t sas_device_pg0;
	Mpi2ConfigReply_t mpi_reply;
	u16 sz;
	u8 num_pds;

	if ((mpt2sas_config_get_number_pds(ioc, raid_device->handle,
	    &num_pds)) || !num_pds) {
		dfailprintk(ioc, printk(MPT2SAS_WARN_FMT
		    "failure at %s:%d/%s()!\n", ioc->name, __FILE__, __LINE__,
		    __func__));
		return 1;
	}

	raid_device->num_pds = num_pds;
	sz = offsetof(Mpi2RaidVolPage0_t, PhysDisk) + (num_pds *
	    sizeof(Mpi2RaidVol0PhysDisk_t));
	vol_pg0 = kzalloc(sz, GFP_KERNEL);
	if (!vol_pg0) {
		dfailprintk(ioc, printk(MPT2SAS_WARN_FMT
		    "failure at %s:%d/%s()!\n", ioc->name, __FILE__, __LINE__,
		    __func__));
		return 1;
	}

	if ((mpt2sas_config_get_raid_volume_pg0(ioc, &mpi_reply, vol_pg0,
	     MPI2_RAID_VOLUME_PGAD_FORM_HANDLE, raid_device->handle, sz))) {
		dfailprintk(ioc, printk(MPT2SAS_WARN_FMT
		    "failure at %s:%d/%s()!\n", ioc->name, __FILE__, __LINE__,
		    __func__));
		kfree(vol_pg0);
		return 1;
	}

	raid_device->volume_type = vol_pg0->VolumeType;

	/* figure out what the underlying devices are by
	 * obtaining the device_info bits for the 1st device
	 */
	if (!(mpt2sas_config_get_phys_disk_pg0(ioc, &mpi_reply,
	    &pd_pg0, MPI2_PHYSDISK_PGAD_FORM_PHYSDISKNUM,
	    vol_pg0->PhysDisk[0].PhysDiskNum))) {
		if (!(mpt2sas_config_get_sas_device_pg0(ioc, &mpi_reply,
		    &sas_device_pg0, MPI2_SAS_DEVICE_PGAD_FORM_HANDLE,
		    le16_to_cpu(pd_pg0.DevHandle)))) {
			raid_device->device_info =
			    le32_to_cpu(sas_device_pg0.DeviceInfo);
		}
	}

	kfree(vol_pg0);
	return 0;
}

/**
 * _scsih_disable_ddio - Disable direct I/O for all the volumes
 * @ioc: per adapter object
 */
static void
_scsih_disable_ddio(struct MPT2SAS_ADAPTER *ioc)
{
	Mpi2RaidVolPage1_t vol_pg1;
	Mpi2ConfigReply_t mpi_reply;
	struct _raid_device *raid_device;
	u16 handle;
	u16 ioc_status;
	unsigned long flags;

	handle = 0xFFFF;
	while (!(mpt2sas_config_get_raid_volume_pg1(ioc, &mpi_reply,
	    &vol_pg1, MPI2_RAID_VOLUME_PGAD_FORM_GET_NEXT_HANDLE, handle))) {
		ioc_status = le16_to_cpu(mpi_reply.IOCStatus) &
		    MPI2_IOCSTATUS_MASK;
		if (ioc_status != MPI2_IOCSTATUS_SUCCESS) {
			printk(MPT2SAS_INFO_FMT "\tbreak from %s: "
			    "ioc_status(0x%04x), loginfo(0x%08x)\n",
			    ioc->name, __func__, ioc_status,
			    le32_to_cpu(mpi_reply.IOCLogInfo));
			break;
		}
		handle = le16_to_cpu(vol_pg1.DevHandle);
		spin_lock_irqsave(&ioc->raid_device_lock, flags);
		raid_device = _scsih_raid_device_find_by_handle(ioc, handle);
		if (raid_device)
			raid_device->direct_io_enabled = 0;
		spin_unlock_irqrestore(&ioc->raid_device_lock, flags);
	}
	return;
}


/**
 * _scsih_get_num_volumes - Get number of volumes in the ioc
 * @ioc: per adapter object
 */
static u8
_scsih_get_num_volumes(struct MPT2SAS_ADAPTER *ioc)
{
	Mpi2RaidVolPage1_t vol_pg1;
	Mpi2ConfigReply_t mpi_reply;
	u16 handle;
	u8 vol_cnt = 0;
	u16 ioc_status;

	handle = 0xFFFF;
	while (!(mpt2sas_config_get_raid_volume_pg1(ioc, &mpi_reply,
	    &vol_pg1, MPI2_RAID_VOLUME_PGAD_FORM_GET_NEXT_HANDLE, handle))) {
		ioc_status = le16_to_cpu(mpi_reply.IOCStatus) &
		    MPI2_IOCSTATUS_MASK;
		if (ioc_status != MPI2_IOCSTATUS_SUCCESS) {
			printk(MPT2SAS_INFO_FMT "\tbreak from %s: "
			    "ioc_status(0x%04x), loginfo(0x%08x)\n",
			    ioc->name, __func__, ioc_status,
			    le32_to_cpu(mpi_reply.IOCLogInfo));
			break;
		}
		vol_cnt++;
		handle = le16_to_cpu(vol_pg1.DevHandle);
	}
	return vol_cnt;
}


/**
 * _scsih_init_warpdrive_properties - Set properties for warpdrive direct I/O.
 * @ioc: per adapter object
 * @raid_device: the raid_device object
 */
static void
_scsih_init_warpdrive_properties(struct MPT2SAS_ADAPTER *ioc,
	struct _raid_device *raid_device)
{
	Mpi2RaidVolPage0_t *vol_pg0;
	Mpi2RaidPhysDiskPage0_t pd_pg0;
	Mpi2ConfigReply_t mpi_reply;
	u16 sz;
	u8 num_pds, count;
	unsigned long stripe_sz, block_sz;
	u8 stripe_exp, block_exp;
	u64 dev_max_lba;

	if (!ioc->is_warpdrive)
		return;

	if (ioc->mfg_pg10_hide_flag ==  MFG_PAGE10_EXPOSE_ALL_DISKS) {
		printk(MPT2SAS_INFO_FMT "WarpDrive : Direct IO is disabled "
		    "globally as drives are exposed\n", ioc->name);
		return;
	}
	if (_scsih_get_num_volumes(ioc) > 1) {
		_scsih_disable_ddio(ioc);
		printk(MPT2SAS_INFO_FMT "WarpDrive : Direct IO is disabled "
		    "globally as number of drives > 1\n", ioc->name);
		return;
	}
	if ((mpt2sas_config_get_number_pds(ioc, raid_device->handle,
	    &num_pds)) || !num_pds) {
		printk(MPT2SAS_INFO_FMT "WarpDrive : Direct IO is disabled "
		    "Failure in computing number of drives\n", ioc->name);
		return;
	}

	sz = offsetof(Mpi2RaidVolPage0_t, PhysDisk) + (num_pds *
	    sizeof(Mpi2RaidVol0PhysDisk_t));
	vol_pg0 = kzalloc(sz, GFP_KERNEL);
	if (!vol_pg0) {
		printk(MPT2SAS_INFO_FMT "WarpDrive : Direct IO is disabled "
		    "Memory allocation failure for RVPG0\n", ioc->name);
		return;
	}

	if ((mpt2sas_config_get_raid_volume_pg0(ioc, &mpi_reply, vol_pg0,
	     MPI2_RAID_VOLUME_PGAD_FORM_HANDLE, raid_device->handle, sz))) {
		printk(MPT2SAS_INFO_FMT "WarpDrive : Direct IO is disabled "
		    "Failure in retrieving RVPG0\n", ioc->name);
		kfree(vol_pg0);
		return;
	}

	/*
	 * WARPDRIVE:If number of physical disks in a volume exceeds the max pds
	 * assumed for WARPDRIVE, disable direct I/O
	 */
	if (num_pds > MPT_MAX_WARPDRIVE_PDS) {
		printk(MPT2SAS_WARN_FMT "WarpDrive : Direct IO is disabled "
		    "for the drive with handle(0x%04x): num_mem=%d, "
		    "max_mem_allowed=%d\n", ioc->name, raid_device->handle,
		    num_pds, MPT_MAX_WARPDRIVE_PDS);
		kfree(vol_pg0);
		return;
	}
	for (count = 0; count < num_pds; count++) {
		if (mpt2sas_config_get_phys_disk_pg0(ioc, &mpi_reply,
		    &pd_pg0, MPI2_PHYSDISK_PGAD_FORM_PHYSDISKNUM,
		    vol_pg0->PhysDisk[count].PhysDiskNum) ||
		    pd_pg0.DevHandle == MPT2SAS_INVALID_DEVICE_HANDLE) {
			printk(MPT2SAS_INFO_FMT "WarpDrive : Direct IO is "
			    "disabled for the drive with handle(0x%04x) member"
			    "handle retrieval failed for member number=%d\n",
			    ioc->name, raid_device->handle,
			    vol_pg0->PhysDisk[count].PhysDiskNum);
			goto out_error;
		}
		/* Disable direct I/O if member drive lba exceeds 4 bytes */
		dev_max_lba = le64_to_cpu(pd_pg0.DeviceMaxLBA);
		if (dev_max_lba >> 32) {
			printk(MPT2SAS_INFO_FMT "WarpDrive : Direct IO is "
			    "disabled for the drive with handle(0x%04x) member"
			    "handle (0x%04x) unsupported max lba 0x%016llx\n",
			    ioc->name, raid_device->handle,
			    le16_to_cpu(pd_pg0.DevHandle),
			    (unsigned long long)dev_max_lba);
			goto out_error;
		}
		raid_device->pd_handle[count] = le16_to_cpu(pd_pg0.DevHandle);
	}

	/*
	 * Assumption for WD: Direct I/O is not supported if the volume is
	 * not RAID0
	 */
	if (raid_device->volume_type != MPI2_RAID_VOL_TYPE_RAID0) {
		printk(MPT2SAS_INFO_FMT "WarpDrive : Direct IO is disabled "
		    "for the drive with handle(0x%04x): type=%d, "
		    "s_sz=%uK, blk_size=%u\n", ioc->name,
		    raid_device->handle, raid_device->volume_type,
		    (le32_to_cpu(vol_pg0->StripeSize) *
		    le16_to_cpu(vol_pg0->BlockSize)) / 1024,
		    le16_to_cpu(vol_pg0->BlockSize));
		goto out_error;
	}


	stripe_sz = le32_to_cpu(vol_pg0->StripeSize);
	stripe_exp = find_first_bit(&stripe_sz, 32);
	if (stripe_exp == 32) {
		printk(MPT2SAS_INFO_FMT "WarpDrive : Direct IO is disabled "
		    "for the drive with handle(0x%04x) invalid stripe sz %uK\n",
		    ioc->name, raid_device->handle,
		    (le32_to_cpu(vol_pg0->StripeSize) *
		    le16_to_cpu(vol_pg0->BlockSize)) / 1024);
		goto out_error;
	}
	raid_device->stripe_exponent = stripe_exp;

	block_sz = le16_to_cpu(vol_pg0->BlockSize);
	block_exp = find_first_bit(&block_sz, 16);
	if (block_exp == 16) {
		printk(MPT2SAS_INFO_FMT "WarpDrive : Direct IO is disabled "
		    "for the drive with handle(0x%04x) invalid block sz %u\n",
		    ioc->name, raid_device->handle,
		    le16_to_cpu(vol_pg0->BlockSize));
		goto out_error;
	}

	raid_device->block_exponent = block_exp;
	raid_device->direct_io_enabled = 1;

	printk(MPT2SAS_INFO_FMT "WarpDrive : Direct IO is Enabled for the drive"
	    " with handle(0x%04x)\n", ioc->name, raid_device->handle);
	/*
	 * WARPDRIVE: Though the following fields are not used for direct IO,
	 * stored for future purpose:
	 */
	raid_device->max_lba = le64_to_cpu(vol_pg0->MaxLBA);
	raid_device->stripe_sz = le32_to_cpu(vol_pg0->StripeSize);
	raid_device->block_sz = le16_to_cpu(vol_pg0->BlockSize);


	kfree(vol_pg0);
	return;

out_error:
	raid_device->direct_io_enabled = 0;
	for (count = 0; count < num_pds; count++)
		raid_device->pd_handle[count] = 0;
	kfree(vol_pg0);
	return;
}

#ifdef MPT2SAS_MULTIPATH
/**
 * _scsih_detect_multipath - find vpd-sn, and dual path
 * @ioc:
 * @sdev: scsi device struct
 * @sas_device:
 *
 */
static void
_scsih_detect_multipath(struct MPT2SAS_ADAPTER *ioc, struct scsi_device *sdev,
    struct _sas_device *sas_device)
{
	struct _sas_device *sas_device_alt;
	struct MPT2SAS_ADAPTER *ioc_alt;

	if (mpt2sas_multipath == -1 || mpt2sas_multipath == 0)
		return;

	if (sdev->type != TYPE_DISK || sas_device->serial_number == NULL)
		return;

	list_for_each_entry(ioc_alt, &mpt2sas_ioc_list, list) {
		list_for_each_entry(sas_device_alt, &ioc_alt->sas_device_list,
		    list) {
			if (sas_device_alt == sas_device)
				continue;
			if (sas_device_alt->serial_number == NULL)
				continue;
			if (strcmp(sas_device_alt->serial_number,
			     sas_device->serial_number) != 0)
				continue;
			sas_device->ioc = ioc;
			sas_device->sas_device_alt = sas_device_alt;
			sas_device_alt->sas_device_alt = sas_device;
			sas_device_alt->ioc = ioc_alt;
		}
	}
}
#endif

/**
 * _scsih_enable_tlr - setting TLR flags
 * @ioc: per adapter object
 * @sdev: scsi device struct
 *
 * Enabling Transaction Layer Retries for tape devices when
 * vpd page 0x90 is present
 *
 */
static void
_scsih_enable_tlr(struct MPT2SAS_ADAPTER *ioc, struct scsi_device *sdev)
{
	u8 data[30];
	u8 page_len, ii;
	struct MPT2SAS_DEVICE *sas_device_priv_data;
	struct MPT2SAS_TARGET *sas_target_priv_data;

	/* only for TAPE */
	if (sdev->type != TYPE_TAPE)
		return;

	if (!(ioc->facts.IOCCapabilities & MPI2_IOCFACTS_CAPABILITY_TLR))
		return;

	sas_device_priv_data = sdev->hostdata;
	if (!sas_device_priv_data)
		return;
	sas_target_priv_data = sas_device_priv_data->sas_target;
	if (!sas_target_priv_data)
		return;

	/* is Protocol-specific logical unit information (0x90) present ?? */
	if (_scsih_inquiry_vpd_supported_pages(ioc,
	    sas_target_priv_data->handle, sdev->lun, data,
	    sizeof(data)) != DEVICE_READY)
		return;

	page_len = data[3];
	for (ii = 4; ii < page_len + 4; ii++) {
		if (data[ii] == 0x90) {
			sas_device_priv_data->flags |= MPT_DEVICE_TLR_ON;
			return;
		}
	}
}

/**
 * _scsih_slave_configure - device configure routine.
 * @sdev: scsi device struct
 *
 * Returns 0 if ok. Any other return is assumed to be an error and
 * the device is ignored.
 */
static int
_scsih_slave_configure(struct scsi_device *sdev)
{
	struct Scsi_Host *shost = sdev->host;
	struct MPT2SAS_ADAPTER *ioc = shost_private(shost);
	struct MPT2SAS_DEVICE *sas_device_priv_data;
	struct MPT2SAS_TARGET *sas_target_priv_data;
	struct _sas_device *sas_device;
	struct _raid_device *raid_device;
	unsigned long flags;
	int qdepth;
	u8 ssp_target = 0;
	char *ds = "";
	char *r_level = "";
	u16 handle, volume_handle = 0;
	u64 volume_wwid = 0;
	u8 *serial_number = NULL;

	qdepth = 1;
	sas_device_priv_data = sdev->hostdata;
	sas_device_priv_data->configured_lun = 1;
	sas_device_priv_data->flags &= ~MPT_DEVICE_FLAGS_INIT;
	sas_target_priv_data = sas_device_priv_data->sas_target;
	handle = sas_target_priv_data->handle;

	/* raid volume handling */
	if (sas_target_priv_data->flags & MPT_TARGET_FLAGS_VOLUME) {

		spin_lock_irqsave(&ioc->raid_device_lock, flags);
		raid_device = _scsih_raid_device_find_by_handle(ioc, handle);
		spin_unlock_irqrestore(&ioc->raid_device_lock, flags);
		if (!raid_device) {
			dfailprintk(ioc, printk(MPT2SAS_WARN_FMT
			    "failure at %s:%d/%s()!\n", ioc->name, __FILE__,
			    __LINE__, __func__));
			return 1;
		}

		if (_scsih_get_volume_capabilities(ioc, raid_device)) {
			dfailprintk(ioc, printk(MPT2SAS_WARN_FMT
			    "failure at %s:%d/%s()!\n", ioc->name, __FILE__,
			    __LINE__, __func__));
			return 1;
		}

		/*
		 * WARPDRIVE: Initialize the required data for Direct IO
		 */
		_scsih_init_warpdrive_properties(ioc, raid_device);

		/* RAID Queue Depth Support
		 * IS volume = underlying qdepth of drive type, either
		 *    MPT2SAS_SAS_QUEUE_DEPTH or MPT2SAS_SATA_QUEUE_DEPTH
		 * IM/IME/R10 = 128 (MPT2SAS_RAID_QUEUE_DEPTH)
		 */
		if (raid_device->device_info &
		    MPI2_SAS_DEVICE_INFO_SSP_TARGET) {
#if defined(__VMKLNX__)
			qdepth = GUARANTEE_ONE(mpt2sas_sas_queue_depth);
#else
			qdepth = MPT2SAS_SAS_QUEUE_DEPTH;
#endif
			ds = "SSP";
		} else {
#if defined(__VMKLNX__)
			qdepth = GUARANTEE_ONE(mpt2sas_sata_queue_depth);
#else
			qdepth = MPT2SAS_SATA_QUEUE_DEPTH;
#endif
			 if (raid_device->device_info &
			    MPI2_SAS_DEVICE_INFO_SATA_DEVICE)
				ds = "SATA";
			else
				ds = "STP";
		}

		switch (raid_device->volume_type) {
		case MPI2_RAID_VOL_TYPE_RAID0:
			r_level = "RAID0";
			break;
		case MPI2_RAID_VOL_TYPE_RAID1E:
#if defined(__VMKLNX__)
			qdepth = GUARANTEE_ONE(mpt2sas_raid_queue_depth);
#else
			qdepth = MPT2SAS_RAID_QUEUE_DEPTH;
#endif
			if (ioc->manu_pg10.OEMIdentifier &&
			    (le32_to_cpu(ioc->manu_pg10.GenericFlags0) &
			    MFG10_GF0_R10_DISPLAY) &&
			    !(raid_device->num_pds % 2))
				r_level = "RAID10";
			else
				r_level = "RAID1E";
			break;
		case MPI2_RAID_VOL_TYPE_RAID1:
#if defined(__VMKLNX__)
			qdepth = GUARANTEE_ONE(mpt2sas_raid_queue_depth);
#else
			qdepth = MPT2SAS_RAID_QUEUE_DEPTH;
#endif
			r_level = "RAID1";
			break;
		case MPI2_RAID_VOL_TYPE_RAID10:
#if defined(__VMKLNX__)
			qdepth = GUARANTEE_ONE(mpt2sas_raid_queue_depth);
#else
			qdepth = MPT2SAS_RAID_QUEUE_DEPTH;
#endif
			r_level = "RAID10";
			break;
		case MPI2_RAID_VOL_TYPE_UNKNOWN:
		default:
#if defined(__VMKLNX__)
			qdepth = GUARANTEE_ONE(mpt2sas_raid_queue_depth);
#else
			qdepth = MPT2SAS_RAID_QUEUE_DEPTH;
#endif
			r_level = "RAIDX";
			break;
		}

		if (!ioc->hide_ir_msg)
			sdev_printk(KERN_INFO, sdev, "%s: handle(0x%04x), "
			    "wwid(0x%016llx), pd_count(%d), type(%s)\n",
			    r_level, raid_device->handle,
			    (unsigned long long)raid_device->wwid,
			    raid_device->num_pds, ds);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32))
		_scsih_change_queue_depth(sdev, qdepth, SCSI_QDEPTH_DEFAULT);
#else
		_scsih_change_queue_depth(sdev, qdepth);
#endif
/* raid transport support */
#if !defined(__VMKLNX__)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27))
		if (!ioc->is_warpdrive)
			_scsih_set_level(sdev, raid_device->volume_type);
#endif
#endif
		return 0;
	}

	/* non-raid handling */
	if (sas_target_priv_data->flags & MPT_TARGET_FLAGS_RAID_COMPONENT) {
		if (mpt2sas_config_get_volume_handle(ioc, handle,
		    &volume_handle)) {
			dfailprintk(ioc, printk(MPT2SAS_WARN_FMT
			    "failure at %s:%d/%s()!\n", ioc->name,
			    __FILE__, __LINE__, __func__));
			return 1;
		}
		if (volume_handle && mpt2sas_config_get_volume_wwid(ioc,
		    volume_handle, &volume_wwid)) {
			dfailprintk(ioc, printk(MPT2SAS_WARN_FMT
			    "failure at %s:%d/%s()!\n", ioc->name,
			    __FILE__, __LINE__, __func__));
			return 1;
		}
	}

	_scsih_inquiry_vpd_sn(ioc, handle, &serial_number);

	spin_lock_irqsave(&ioc->sas_device_lock, flags);
	sas_device = mpt2sas_scsih_sas_device_find_by_sas_address(ioc,
	   sas_device_priv_data->sas_target->sas_address);
	if (!sas_device) {
		spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
		dfailprintk(ioc, printk(MPT2SAS_WARN_FMT
		    "failure at %s:%d/%s()!\n", ioc->name, __FILE__, __LINE__,
		    __func__));
		kfree(serial_number);
		return 1;
	}

	sas_device->volume_handle = volume_handle;
	sas_device->volume_wwid = volume_wwid;
	sas_device->serial_number = serial_number;

	if (sas_device->device_info & MPI2_SAS_DEVICE_INFO_SSP_TARGET) {
#if defined(__VMKLNX__)
		qdepth = GUARANTEE_ONE(mpt2sas_sas_queue_depth);
#else
		qdepth = MPT2SAS_SAS_QUEUE_DEPTH;
#endif
		ssp_target = 1;
		ds = "SSP";
	} else {
#if defined(__VMKLNX__)
		qdepth = GUARANTEE_ONE(mpt2sas_sata_queue_depth);
#else
		qdepth = MPT2SAS_SATA_QUEUE_DEPTH;
#endif
		if (sas_device->device_info &
		    MPI2_SAS_DEVICE_INFO_STP_TARGET)
			ds = "STP";
		else if (sas_device->device_info &
		    MPI2_SAS_DEVICE_INFO_SATA_DEVICE)
			ds = "SATA";
	}

	sdev_printk(KERN_INFO, sdev, "%s: handle(0x%04x), "
	    "sas_addr(0x%016llx), phy(%d), device_name(0x%016llx)\n",
	    ds, sas_device->handle,
	    (unsigned long long)sas_device->sas_address,
	    sas_device->phy,
	    (unsigned long long)sas_device->device_name);
	sdev_printk(KERN_INFO, sdev, "%s: "
	    "enclosure_logical_id(0x%016llx), slot(%d)\n", ds,
	    (unsigned long long)sas_device->enclosure_logical_id,
	    sas_device->slot);

#ifdef MPT2SAS_MULTIPATH
	_scsih_detect_multipath(ioc, sdev, sas_device);
#endif

	spin_unlock_irqrestore(&ioc->sas_device_lock, flags);

	if (!ssp_target)
		_scsih_display_sata_capabilities(ioc, handle, sdev);

	if (serial_number)
		sdev_printk(KERN_INFO, sdev, "serial_number(%s)\n",
		    serial_number);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32))
	_scsih_change_queue_depth(sdev, qdepth, SCSI_QDEPTH_DEFAULT);
#else
	_scsih_change_queue_depth(sdev, qdepth);
#endif

	if (ssp_target) {
		sas_read_port_mode_page(sdev);
		_scsih_enable_tlr(ioc, sdev);
	}

	if (!ioc->disable_eedp_support) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27))
		if (ssp_target && (!(sas_target_priv_data->flags &
		    MPT_TARGET_FLAGS_RAID_COMPONENT))) {
			struct read_cap_parameter data;
			enum device_responsive_state retcode;
			u8 retry_count = 0;

			if (!(sdev->inquiry[5] & 1))
				goto out;
 retry:
			/* issue one retry to handle UA's */
			memset(&data, 0, sizeof(struct read_cap_parameter));
			retcode = _scsih_read_capacity_16(ioc,
			    sas_target_priv_data->handle, sdev->lun, &data,
			    sizeof(struct read_cap_parameter));

			if ((retcode == DEVICE_RETRY || retcode == DEVICE_RETRY_UA)
			    && (!retry_count++))
				goto retry;
			if (retcode != DEVICE_READY)
				goto out;
			if (!data.prot_en)
				goto out;
			sas_device_priv_data->eedp_type = data.p_type + 1;

			if (sas_device_priv_data->eedp_type == 2) {
				sdev_printk(KERN_INFO, sdev, "formatted with "
				    "DIF Type 2 protection which is currently "
				    "unsupported. \n");
				goto out;
			}

			sas_device_priv_data->eedp_enable = 1;
			sdev_printk(KERN_INFO, sdev, "Enabling DIF Type %d "
			    "protection\n", sas_device_priv_data->eedp_type);
		}
 out:
		return 0;
#endif
	}

#if defined(__VMKLNX__)
	/* Mark a RAID-member target as configured. */
	if (sas_target_priv_data && sdev->no_uld_attach)
		sas_target_priv_data->configured = 1;
#endif
	return 0;
}

/**
 * _scsih_bios_param - fetch head, sector, cylinder info for a disk
 * @sdev: scsi device struct
 * @bdev: pointer to block device context
 * @capacity: device size (in 512 byte sectors)
 * @params: three element array to place output:
 *              params[0] number of heads (max 255)
 *              params[1] number of sectors (max 63)
 *              params[2] number of cylinders
 *
 * Return nothing.
 */
static int
_scsih_bios_param(struct scsi_device *sdev, struct block_device *bdev,
    sector_t capacity, int params[])
{
	int		heads;
	int		sectors;
	sector_t	cylinders;
	ulong 		dummy;

	heads = 64;
	sectors = 32;

	dummy = heads * sectors;
	cylinders = capacity;
	sector_div(cylinders, dummy);

	/*
	 * Handle extended translation size for logical drives
	 * > 1Gb
	 */
	if ((ulong)capacity >= 0x200000) {
		heads = 255;
		sectors = 63;
		dummy = heads * sectors;
		cylinders = capacity;
		sector_div(cylinders, dummy);
	}

	/* return result */
	params[0] = heads;
	params[1] = sectors;
	params[2] = cylinders;

	return 0;
}

/**
 * _scsih_response_code - translation of device response code
 * @ioc: per adapter object
 * @response_code: response code returned by the device
 *
 * Return nothing.
 */
static void
_scsih_response_code(struct MPT2SAS_ADAPTER *ioc, u8 response_code)
{
	char *desc;

	switch (response_code) {
	case MPI2_SCSITASKMGMT_RSP_TM_COMPLETE:
		desc = "task management request completed";
		break;
	case MPI2_SCSITASKMGMT_RSP_INVALID_FRAME:
		desc = "invalid frame";
		break;
	case MPI2_SCSITASKMGMT_RSP_TM_NOT_SUPPORTED:
		desc = "task management request not supported";
		break;
	case MPI2_SCSITASKMGMT_RSP_TM_FAILED:
		desc = "task management request failed";
		break;
	case MPI2_SCSITASKMGMT_RSP_TM_SUCCEEDED:
		desc = "task management request succeeded";
		break;
	case MPI2_SCSITASKMGMT_RSP_TM_INVALID_LUN:
		desc = "invalid lun";
		break;
	case 0xA:
		desc = "overlapped tag attempted";
		break;
	case MPI2_SCSITASKMGMT_RSP_IO_QUEUED_ON_IOC:
		desc = "task queued, however not sent to target";
		break;
	default:
		desc = "unknown";
		break;
	}
	printk(MPT2SAS_WARN_FMT "response_code(0x%01x): %s\n",
		ioc->name, response_code, desc);
}

/**
 * _scsih_tm_done - tm completion routine
 * @ioc: per adapter object
 * @smid: system request message index
 * @msix_index: MSIX table index supplied by the OS
 * @reply: reply message frame(lower 32bit addr)
 * Context: none.
 *
 * The callback handler when using scsih_issue_tm.
 *
 * Return 1 meaning mf should be freed from _base_interrupt
 *        0 means the mf is freed from this function.
 */
static u8
_scsih_tm_done(struct MPT2SAS_ADAPTER *ioc, u16 smid, u8 msix_index, u32 reply)
{
	MPI2DefaultReply_t *mpi_reply;

	if (ioc->tm_cmds.status == MPT2_CMD_NOT_USED)
		return 1;
	if (ioc->tm_cmds.smid != smid)
		return 1;
	mpt2sas_base_flush_reply_queues(ioc);
	ioc->tm_cmds.status |= MPT2_CMD_COMPLETE;
	mpi_reply =  mpt2sas_base_get_reply_virt_addr(ioc, reply);
	if (mpi_reply) {
		memcpy(ioc->tm_cmds.reply, mpi_reply, mpi_reply->MsgLength*4);
		ioc->tm_cmds.status |= MPT2_CMD_REPLY_VALID;
	}
	ioc->tm_cmds.status &= ~MPT2_CMD_PENDING;
	complete(&ioc->tm_cmds.done);
	return 1;
}

/**
 * mpt2sas_scsih_set_tm_flag - set per target tm_busy
 * @ioc: per adapter object
 * @handle: device handle
 *
 * During taskmangement request, we need to freeze the device queue.
 */
void
mpt2sas_scsih_set_tm_flag(struct MPT2SAS_ADAPTER *ioc, u16 handle)
{
	struct MPT2SAS_DEVICE *sas_device_priv_data;
	struct scsi_device *sdev;
	u8 skip = 0;

	shost_for_each_device(sdev, ioc->shost) {
		if (skip)
			continue;
		sas_device_priv_data = sdev->hostdata;
		if (!sas_device_priv_data)
			continue;
		if (sas_device_priv_data->sas_target->handle == handle) {
			sas_device_priv_data->sas_target->tm_busy = 1;
			skip = 1;
			ioc->ignore_loginfos = 1;
		}
	}
}

/**
 * mpt2sas_scsih_clear_tm_flag - clear per target tm_busy
 * @ioc: per adapter object
 * @handle: device handle
 *
 * During taskmangement request, we need to freeze the device queue.
 */
void
mpt2sas_scsih_clear_tm_flag(struct MPT2SAS_ADAPTER *ioc, u16 handle)
{
	struct MPT2SAS_DEVICE *sas_device_priv_data;
	struct scsi_device *sdev;
	u8 skip = 0;

	shost_for_each_device(sdev, ioc->shost) {
		if (skip)
			continue;
		sas_device_priv_data = sdev->hostdata;
		if (!sas_device_priv_data)
			continue;
		if (sas_device_priv_data->sas_target->handle == handle) {
			sas_device_priv_data->sas_target->tm_busy = 0;
			skip = 1;
			ioc->ignore_loginfos = 0;
		}
	}
}

/**
 * mpt2sas_scsih_issue_tm - main routine for sending tm requests
 * @ioc: per adapter struct
 * @device_handle: device handle
 * @channel: the channel assigned by the OS
 * @id: the id assigned by the OS
 * @lun: lun number
 * @type: MPI2_SCSITASKMGMT_TASKTYPE__XXX (defined in mpi2_init.h)
 * @smid_task: smid assigned to the task
 * @timeout: timeout in seconds
 * Context: user
 *
 * A generic API for sending task management requests to firmware.
 *
 * The callback index is set inside `ioc->tm_cb_idx`.
 *
 * Return SUCCESS or FAILED.
 */
int
mpt2sas_scsih_issue_tm(struct MPT2SAS_ADAPTER *ioc, u16 handle, uint channel,
    uint id, uint lun, u8 type, u16 smid_task, ulong timeout,
    struct scsi_cmnd *scmd)
{
	Mpi2SCSITaskManagementRequest_t *mpi_request;
	Mpi2SCSITaskManagementReply_t *mpi_reply;
	u16 smid = 0;
	u32 ioc_state;
	unsigned long timeleft;
	struct scsi_cmnd *scmd_lookup;
	int rc;

	mutex_lock(&ioc->tm_cmds.mutex);
	if (ioc->tm_cmds.status != MPT2_CMD_NOT_USED) {
		printk(MPT2SAS_INFO_FMT "%s: tm_cmd busy!!!\n",
		    __func__, ioc->name);
		rc = FAILED;
		goto err_out;
	}

	if (ioc->shost_recovery || ioc->remove_host ||
	    ioc->pci_error_recovery) {
		printk(MPT2SAS_INFO_FMT "%s: host reset in progress!\n",
		    __func__, ioc->name);
		rc = FAILED;
		goto err_out;
	}

	ioc_state = mpt2sas_base_get_iocstate(ioc, 0);
	if (ioc_state & MPI2_DOORBELL_USED) {
		dhsprintk(ioc, printk(MPT2SAS_INFO_FMT "unexpected doorbell "
		    "active!\n", ioc->name));
		mpt2sas_base_hard_reset_handler(ioc, CAN_SLEEP,
		    FORCE_BIG_HAMMER);
		rc = SUCCESS;
		goto err_out;
	}

	if ((ioc_state & MPI2_IOC_STATE_MASK) == MPI2_IOC_STATE_FAULT) {
		mpt2sas_base_fault_info(ioc, ioc_state &
		    MPI2_DOORBELL_DATA_MASK);
		mpt2sas_base_hard_reset_handler(ioc, CAN_SLEEP,
		    FORCE_BIG_HAMMER);
		rc = SUCCESS;
		goto err_out;
	}

	smid = mpt2sas_base_get_smid_hpr(ioc, ioc->tm_cb_idx);
	if (!smid) {
		printk(MPT2SAS_ERR_FMT "%s: failed obtaining a smid\n",
		    ioc->name, __func__);
		rc = FAILED;
		goto err_out;
	}

	dtmprintk(ioc, printk(MPT2SAS_INFO_FMT "sending tm: handle(0x%04x),"
	    " task_type(0x%02x), smid(%d)\n", ioc->name, handle, type,
	    smid_task));
	ioc->tm_cmds.status = MPT2_CMD_PENDING;
	mpi_request = mpt2sas_base_get_msg_frame(ioc, smid);
	ioc->tm_cmds.smid = smid;
	memset(mpi_request, 0, sizeof(Mpi2SCSITaskManagementRequest_t));
	mpi_request->Function = MPI2_FUNCTION_SCSI_TASK_MGMT;
	mpi_request->DevHandle = cpu_to_le16(handle);
	mpi_request->TaskType = type;
	mpi_request->TaskMID = cpu_to_le16(smid_task);
	int_to_scsilun(lun, (struct scsi_lun *)mpi_request->LUN);
	mpt2sas_scsih_set_tm_flag(ioc, handle);
	init_completion(&ioc->tm_cmds.done);
	mpt2sas_base_put_smid_hi_priority(ioc, smid);
	timeleft = wait_for_completion_timeout(&ioc->tm_cmds.done, timeout*HZ);
	if (!(ioc->tm_cmds.status & MPT2_CMD_COMPLETE)) {
		printk(MPT2SAS_ERR_FMT "%s: timeout\n",
		    ioc->name, __func__);
		_debug_dump_mf(mpi_request,
		    sizeof(Mpi2SCSITaskManagementRequest_t)/4);
		if (!(ioc->tm_cmds.status & MPT2_CMD_RESET)) {
			mpt2sas_base_hard_reset_handler(ioc, CAN_SLEEP,
			    FORCE_BIG_HAMMER);
			rc = SUCCESS;
			ioc->tm_cmds.status = MPT2_CMD_NOT_USED;
			mpt2sas_scsih_clear_tm_flag(ioc, handle);
			goto err_out;
		}
	}

	if (ioc->tm_cmds.status & MPT2_CMD_REPLY_VALID) {
		mpi_reply = ioc->tm_cmds.reply;
		dtmprintk(ioc, printk(MPT2SAS_INFO_FMT "complete tm: "
		    "ioc_status(0x%04x), loginfo(0x%08x), term_count(0x%08x)\n",
		    ioc->name, le16_to_cpu(mpi_reply->IOCStatus),
		    le32_to_cpu(mpi_reply->IOCLogInfo),
		    le32_to_cpu(mpi_reply->TerminationCount)));
		if (ioc->logging_level & MPT_DEBUG_TM) {
			_scsih_response_code(ioc, mpi_reply->ResponseCode);
			if (mpi_reply->IOCStatus)
				_debug_dump_mf(mpi_request,
				    sizeof(Mpi2SCSITaskManagementRequest_t)/4);
		}
	}

	/* sanity check:
	 * Check to see the commands were terminated.
	 * This is only needed for eh callbacks, hence the scmd check.
	 */
	rc = FAILED;
	if (scmd == NULL) {
		rc = SUCCESS;
		goto bypass_sanity_checks;
	}
	switch (type) {
	case MPI2_SCSITASKMGMT_TASKTYPE_ABORT_TASK:
		scmd_lookup = _scsih_scsi_lookup_get(ioc, smid_task);
		if (scmd_lookup && (scmd_lookup->serial_number ==
		    scmd->serial_number))
			rc = FAILED;
		else
			rc = SUCCESS;
		break;

	case MPI2_SCSITASKMGMT_TASKTYPE_TARGET_RESET:
#if defined(__VMKLNX__)
		if (_scsih_scsi_lookup_find_by_channel_target_lun(ioc, channel,
			id, SCAN_WILD_CARD))
#else
		if (_scsih_scsi_lookup_find_by_target(ioc, id, channel))
#endif
			rc = FAILED;
		else
			rc = SUCCESS;
		break;

	case MPI2_SCSITASKMGMT_TASKTYPE_LOGICAL_UNIT_RESET:
#if defined(__VMKLNX__)
		if (_scsih_scsi_lookup_find_by_channel_target_lun(ioc, channel,
			id, lun))
#else
		if (_scsih_scsi_lookup_find_by_lun(ioc, id, lun, channel))
#endif
			rc = FAILED;
		else
			rc = SUCCESS;
		break;
	}

 bypass_sanity_checks:

	mpt2sas_scsih_clear_tm_flag(ioc, handle);
	ioc->tm_cmds.status = MPT2_CMD_NOT_USED;
	mutex_unlock(&ioc->tm_cmds.mutex);

#ifdef MPT2SAS_MULTIPATH
	mpt2sas_scsih_check_tm_for_multipath(ioc, handle, type);
#endif
	return rc;

 err_out:
	mutex_unlock(&ioc->tm_cmds.mutex);
	return rc;
}

/**
 * _scsih_tm_display_info - displays info about the device
 * @ioc: per adapter struct
 * @scmd: pointer to scsi command object
 *
 * Called by task management callback handlers.
 */
static void
_scsih_tm_display_info(struct MPT2SAS_ADAPTER *ioc, struct scsi_cmnd *scmd)
{
	struct scsi_target *starget = scmd->device->sdev_target;
	struct MPT2SAS_TARGET *priv_target = starget->hostdata;
	struct _sas_device *sas_device = NULL;
	unsigned long flags;
	char *device_str = NULL;

	if (!priv_target)
		return;
	if (ioc->hide_ir_msg)
		device_str = "WarpDrive";
	else
		device_str = "volume";

	scsi_print_command(scmd);
	if (priv_target->flags & MPT_TARGET_FLAGS_VOLUME) {
		starget_printk(KERN_INFO, starget, "%s handle(0x%04x), "
		    "%s wwid(0x%016llx)\n", device_str, priv_target->handle,
		    device_str, (unsigned long long)priv_target->sas_address);
	} else {
		spin_lock_irqsave(&ioc->sas_device_lock, flags);
		sas_device = mpt2sas_scsih_sas_device_find_by_sas_address(ioc,
		    priv_target->sas_address);
		if (sas_device) {
			if (priv_target->flags &
			    MPT_TARGET_FLAGS_RAID_COMPONENT) {
				starget_printk(KERN_INFO, starget,
				    "volume handle(0x%04x), "
				    "volume wwid(0x%016llx)\n",
				    sas_device->volume_handle,
				   (unsigned long long)sas_device->volume_wwid);
			}
			starget_printk(KERN_INFO, starget,
			    "handle(0x%04x), sas_address(0x%016llx), phy(%d)\n",
			    sas_device->handle,
			    (unsigned long long)sas_device->sas_address,
			    sas_device->phy);
			starget_printk(KERN_INFO, starget,
			    "enclosure_logical_id(0x%016llx), slot(%d)\n",
			   (unsigned long long)sas_device->enclosure_logical_id,
			    sas_device->slot);
		}
		spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
	}
}

/**
 * _scsih_abort - eh threads main abort routine
 * @scmd: pointer to scsi command object
 *
 * Returns SUCCESS if command aborted else FAILED
 */
static int
_scsih_abort(struct scsi_cmnd *scmd)
{
	struct MPT2SAS_ADAPTER *ioc = shost_private(scmd->device->host);
	struct MPT2SAS_DEVICE *sas_device_priv_data;
	u16 smid;
	u16 handle;
	int r;

	sdev_printk(KERN_INFO, scmd->device, "attempting task abort! "
	    "scmd(%p)\n", scmd);
	_scsih_tm_display_info(ioc, scmd);

	sas_device_priv_data = scmd->device->hostdata;
	if (!sas_device_priv_data || !sas_device_priv_data->sas_target) {
		sdev_printk(KERN_INFO, scmd->device, "device been deleted! "
		    "scmd(%p)\n", scmd);
		scmd->result = DID_NO_CONNECT << 16;
		scmd->scsi_done(scmd);
		r = SUCCESS;
		goto out;
	}

	/* search for the command */
	smid = _scsih_scsi_lookup_find_by_scmd(ioc, scmd);
	if (!smid) {
		scmd->result = DID_RESET << 16;
		r = SUCCESS;
		goto out;
	}

	/* for hidden raid components and volumes this is not supported */
	if (sas_device_priv_data->sas_target->flags &
	    MPT_TARGET_FLAGS_RAID_COMPONENT ||
	    sas_device_priv_data->sas_target->flags & MPT_TARGET_FLAGS_VOLUME) {
		scmd->result = DID_RESET << 16;
		r = FAILED;
		goto out;
	}

	mpt2sas_halt_firmware(ioc);

	handle = sas_device_priv_data->sas_target->handle;
	r = mpt2sas_scsih_issue_tm(ioc, handle, scmd->device->channel,
	    scmd->device->id, scmd->device->lun,
	    MPI2_SCSITASKMGMT_TASKTYPE_ABORT_TASK, smid, 30, scmd);

 out:
	sdev_printk(KERN_INFO, scmd->device, "task abort: %s scmd(%p)\n",
	    ((r == SUCCESS) ? "SUCCESS" : "FAILED"), scmd);
	return r;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26))
/**
 * _scsih_dev_reset - eh threads main device reset routine
 * @scmd: pointer to scsi command object
 *
 * Returns SUCCESS if command aborted else FAILED
 */
static int
_scsih_dev_reset(struct scsi_cmnd *scmd)
{
	struct MPT2SAS_ADAPTER *ioc = shost_private(scmd->device->host);
	struct MPT2SAS_DEVICE *sas_device_priv_data;
	struct _sas_device *sas_device;
	unsigned long flags;
	u16	handle;
	int r;

	sdev_printk(KERN_INFO, scmd->device, "attempting device reset! "
	    "scmd(%p)\n", scmd);
	_scsih_tm_display_info(ioc, scmd);

	sas_device_priv_data = scmd->device->hostdata;
	if (!sas_device_priv_data || !sas_device_priv_data->sas_target) {
		sdev_printk(KERN_INFO, scmd->device, "device been deleted! "
		    "scmd(%p)\n", scmd);
		scmd->result = DID_NO_CONNECT << 16;
		scmd->scsi_done(scmd);
		r = SUCCESS;
		goto out;
	}

	/* for hidden raid components obtain the volume_handle */
	handle = 0;
	if (sas_device_priv_data->sas_target->flags &
	    MPT_TARGET_FLAGS_RAID_COMPONENT) {
		spin_lock_irqsave(&ioc->sas_device_lock, flags);
		sas_device = _scsih_sas_device_find_by_handle(ioc,
		   sas_device_priv_data->sas_target->handle);
		if (sas_device)
			handle = sas_device->volume_handle;
		spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
	} else
		handle = sas_device_priv_data->sas_target->handle;

	if (!handle) {
		scmd->result = DID_RESET << 16;
		r = FAILED;
		goto out;
	}

	r = mpt2sas_scsih_issue_tm(ioc, handle, scmd->device->channel,
	    scmd->device->id, scmd->device->lun,
	    MPI2_SCSITASKMGMT_TASKTYPE_LOGICAL_UNIT_RESET, 0, 30, scmd);

 out:
	sdev_printk(KERN_INFO, scmd->device, "device reset: %s scmd(%p)\n",
	    ((r == SUCCESS) ? "SUCCESS" : "FAILED"), scmd);
	return r;
}

/**
 * _scsih_target_reset - eh threads main target reset routine
 * @scmd: pointer to scsi command object
 *
 * Returns SUCCESS if command aborted else FAILED
 */
static int
_scsih_target_reset(struct scsi_cmnd *scmd)
{
	struct MPT2SAS_ADAPTER *ioc = shost_private(scmd->device->host);
	struct MPT2SAS_DEVICE *sas_device_priv_data;
	struct _sas_device *sas_device;
	unsigned long flags;
	u16	handle;
	int r;
	struct scsi_target *starget = scmd->device->sdev_target;

	starget_printk(KERN_INFO, starget, "attempting target reset! "
	    "scmd(%p)\n", scmd);
	_scsih_tm_display_info(ioc, scmd);

	sas_device_priv_data = scmd->device->hostdata;
	if (!sas_device_priv_data || !sas_device_priv_data->sas_target) {
		starget_printk(KERN_INFO, starget, "target been deleted! "
		    "scmd(%p)\n", scmd);
		scmd->result = DID_NO_CONNECT << 16;
		scmd->scsi_done(scmd);
		r = SUCCESS;
		goto out;
	}

	/* for hidden raid components obtain the volume_handle */
	handle = 0;
	if (sas_device_priv_data->sas_target->flags &
	    MPT_TARGET_FLAGS_RAID_COMPONENT) {
		spin_lock_irqsave(&ioc->sas_device_lock, flags);
		sas_device = _scsih_sas_device_find_by_handle(ioc,
		   sas_device_priv_data->sas_target->handle);
		if (sas_device)
			handle = sas_device->volume_handle;
		spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
	} else
		handle = sas_device_priv_data->sas_target->handle;

	if (!handle) {
		scmd->result = DID_RESET << 16;
		r = FAILED;
		goto out;
	}

	r = mpt2sas_scsih_issue_tm(ioc, handle, scmd->device->channel,
	    scmd->device->id, 0,
	    MPI2_SCSITASKMGMT_TASKTYPE_TARGET_RESET, 0, 30, scmd);
	/*
	 *  sanity check see whether all commands to this target been
	 *  completed
	 */
 out:
	starget_printk(KERN_INFO, starget, "target reset: %s scmd(%p)\n",
	    ((r == SUCCESS) ? "SUCCESS" : "FAILED"), scmd);
	return r;
}

#else /* prior to 2.6.26 kernel */


/**
 * _scsih_dev_reset - eh threads main device reset routine
 * @scmd: pointer to scsi command object
 *
 * Returns SUCCESS if command aborted else FAILED
 */
static int
_scsih_dev_reset(struct scsi_cmnd *scmd)
{
	struct MPT2SAS_ADAPTER *ioc = shost_private(scmd->device->host);
	struct MPT2SAS_DEVICE *sas_device_priv_data;
	struct _sas_device *sas_device;
	unsigned long flags;
	u16	handle;
	int r;
	struct scsi_target *starget = scmd->device->sdev_target;

#if defined(__VMKLNX__)
        int tm_type = (scmd->vmkflags & VMK_FLAGS_USE_LUNRESET) ?
                        MPI2_SCSITASKMGMT_TASKTYPE_LOGICAL_UNIT_RESET :
                        MPI2_SCSITASKMGMT_TASKTYPE_TARGET_RESET;

        char *tm_name = (MPI2_SCSITASKMGMT_TASKTYPE_TARGET_RESET == tm_type) ?
                        "target reset" : "lun reset";
	printk(MPT2SAS_INFO_FMT "attempting %s! scmd(%p) on C%d:T%d:L%d\n",
	    ioc->name, tm_name, scmd,
	    scmd->device->channel, scmd->device->id, scmd->device->lun);
	_scsih_tm_display_info(ioc, scmd);
#else

	starget_printk(KERN_INFO, starget, "attempting target reset! "
	    "scmd(%p)\n", scmd);
	_scsih_tm_display_info(ioc, scmd);
#endif

	sas_device_priv_data = scmd->device->hostdata;
	if (!sas_device_priv_data || !sas_device_priv_data->sas_target) {
		starget_printk(KERN_INFO, starget, "target been deleted! "
		    "scmd(%p)\n", scmd);
		scmd->result = DID_NO_CONNECT << 16;
		scmd->scsi_done(scmd);
		r = SUCCESS;
		goto out;
	}

	/* for hidden raid components obtain the volume_handle */
	handle = 0;
	if (sas_device_priv_data->sas_target->flags &
	    MPT_TARGET_FLAGS_RAID_COMPONENT) {
		spin_lock_irqsave(&ioc->sas_device_lock, flags);
		sas_device = _scsih_sas_device_find_by_handle(ioc,
		   sas_device_priv_data->sas_target->handle);
		if (sas_device)
			handle = sas_device->volume_handle;
		spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
	} else
		handle = sas_device_priv_data->sas_target->handle;

	if (!handle) {
		scmd->result = DID_RESET << 16;
		r = FAILED;
		goto out;
	}

#if defined(__VMKLNX__)
	r = mpt2sas_scsih_issue_tm(ioc, handle, scmd->device->channel,
	    scmd->device->id,
	    (MPI2_SCSITASKMGMT_TASKTYPE_TARGET_RESET == tm_type) ? 0 :
	    scmd->device->lun, tm_type, 0, 30, scmd);
#else
	r = mpt2sas_scsih_issue_tm(ioc, handle, scmd->device->channel,
	    scmd->device->id, 0,
	    MPI2_SCSITASKMGMT_TASKTYPE_TARGET_RESET, 0, 30, scmd);
#endif

 out:
#if defined(__VMKLNX__)
	printk(MPT2SAS_INFO_FMT "%s: %s scmd(%p)\n",
	    ioc->name, tm_name, ((r == SUCCESS) ? "SUCCESS" : "FAILED"), scmd);
#else
	starget_printk(KERN_INFO, starget, "target reset: %s scmd(%p)\n",
	    ((r == SUCCESS) ? "SUCCESS" : "FAILED"), scmd);
#endif
	return r;
}
#endif


#if defined(__VMKLNX__)
/**
 * scsih_bus_reset - eh threads bus reset routine
 *                   firmware does not support bus reset so actual implement is to reset all targets one by one
 *                   Time to complete this bus reset will depend on the number of targets to be reset.
 * @sdev: scsi device struct
 *
 * Returns SUCCESS if command aborted else FAILED
 */
static int
scsih_bus_reset(struct scsi_cmnd *scmd)
{
	int r, ret;
	unsigned long flags;
	struct scsi_target *starget = scmd->device->sdev_target;
	struct MPT2SAS_TARGET *sas_target_priv_data;
	struct MPT2SAS_ADAPTER *ioc = shost_private(scmd->device->host);
	struct reset_struct {
		struct list_head list;
		u16 handle; /* fw needs handle (a number) to do target reset */
		unsigned int id; /* target id is needed to check pending commands on the target */
	};
	struct reset_struct *r_struct, *r_struct2;
	LIST_HEAD(reset_list);

	starget_printk(KERN_INFO, starget, "attempting bus reset! "
	    "scmd(%p)\n", scmd);
	_scsih_tm_display_info(ioc, scmd);

	r = SUCCESS;

	spin_lock_irqsave(&ioc->target_list_lock, flags);
	list_for_each_entry(sas_target_priv_data, &ioc->target_list, list) {
		if ((starget = sas_target_priv_data->starget) &&
		    (starget->channel == scmd->device->channel) &&
		    !(sas_target_priv_data->flags & MPT_TARGET_FLAGS_RAID_COMPONENT)){
			r_struct = kzalloc(sizeof(struct reset_struct), GFP_KERNEL);
			r_struct->id = starget->id;
			r_struct->handle = sas_target_priv_data->handle;
			list_add_tail(&r_struct->list, &reset_list);
		}
	}
	spin_unlock_irqrestore(&ioc->target_list_lock, flags);
	list_for_each_entry_safe(r_struct, r_struct2, &reset_list, list) {
		/* target reset */
		ret = mpt2sas_scsih_issue_tm(ioc, r_struct->handle,
		    scmd->device->channel, r_struct->id, 0,
		    MPI2_SCSITASKMGMT_TASKTYPE_TARGET_RESET, 0, 30, scmd);
		if (ret == FAILED)
			r = FAILED;
		list_del(&r_struct->list);
		kfree(r_struct);
	}
	printk(MPT2SAS_INFO_FMT "bus reset: %s scmd(%p)\n",
	    ioc->name, ((r == SUCCESS) ? "SUCCESS" : "FAILED"), scmd);
	return r;
}
#endif

/**
 * _scsih_host_reset - eh threads main host reset routine
 * @scmd: pointer to scsi command object
 *
 * Returns SUCCESS if command aborted else FAILED
 */
static int
_scsih_host_reset(struct scsi_cmnd *scmd)
{
	struct MPT2SAS_ADAPTER *ioc = shost_private(scmd->device->host);
	int r, retval;

	printk(MPT2SAS_INFO_FMT "attempting host reset! scmd(%p)\n",
	    ioc->name, scmd);
	scsi_print_command(scmd);

	retval = mpt2sas_base_hard_reset_handler(ioc, CAN_SLEEP,
	    FORCE_BIG_HAMMER);
	r = (retval < 0) ? FAILED : SUCCESS;
	printk(MPT2SAS_INFO_FMT "host reset: %s scmd(%p)\n",
	    ioc->name, ((r == SUCCESS) ? "SUCCESS" : "FAILED"), scmd);

	return r;
}

/**
 * _scsih_fw_event_add - insert and queue up fw_event
 * @ioc: per adapter object
 * @fw_event: object describing the event
 * Context: This function will acquire ioc->fw_event_lock.
 *
 * This adds the firmware event object into link list, then queues it up to
 * be processed from user context.
 *
 * Return nothing.
 */
static void
_scsih_fw_event_add(struct MPT2SAS_ADAPTER *ioc, struct fw_event_work *fw_event)
{
	unsigned long flags;

	if (ioc->firmware_event_thread == NULL)
		return;

	spin_lock_irqsave(&ioc->fw_event_lock, flags);
	INIT_LIST_HEAD(&fw_event->list);
	list_add_tail(&fw_event->list, &ioc->fw_event_list);
#if (ESX_VERS > ESX41_VERS) || (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,19))
	INIT_WORK(&fw_event->work, _firmware_event_work);
#else
	INIT_WORK(&fw_event->work, _firmware_event_work, (void *)fw_event);
#endif
	queue_work(ioc->firmware_event_thread, &fw_event->work);
	spin_unlock_irqrestore(&ioc->fw_event_lock, flags);
}

/**
 * _scsih_fw_event_free - delete fw_event
 * @ioc: per adapter object
 * @fw_event: object describing the event
 * Context: This function will acquire ioc->fw_event_lock.
 *
 * This removes firmware event object from link list, frees associated memory.
 *
 * Return nothing.
 */
static void
_scsih_fw_event_free(struct MPT2SAS_ADAPTER *ioc, struct fw_event_work
    *fw_event)
{
	unsigned long flags;

	spin_lock_irqsave(&ioc->fw_event_lock, flags);
	list_del(&fw_event->list);
	kfree(fw_event->retries);
	kfree(fw_event->event_data);
	kfree(fw_event);
	spin_unlock_irqrestore(&ioc->fw_event_lock, flags);
}

/**
 * _scsih_fw_event_add - requeue an event
 * @ioc: per adapter object
 * @fw_event: object describing the event
 * Context: This function will acquire ioc->fw_event_lock.
 *
 * Return nothing.
 */
static void
_scsih_fw_event_requeue(struct MPT2SAS_ADAPTER *ioc, struct fw_event_work
    *fw_event, unsigned long delay)
{
	unsigned long flags;

	if (ioc->firmware_event_thread == NULL)
		return;

	spin_lock_irqsave(&ioc->fw_event_lock, flags);
#if (ESX_VERS > ESX41_VERS) || (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,19))
	if (!fw_event->delayed_work_active) {
		fw_event->delayed_work_active = 1;
		INIT_DELAYED_WORK(&fw_event->delayed_work,
		    _firmware_event_work_delayed);
	}
	queue_delayed_work(ioc->firmware_event_thread, &fw_event->delayed_work,
	    msecs_to_jiffies(delay));
#else
	queue_delayed_work(ioc->firmware_event_thread, &fw_event->work,
	    msecs_to_jiffies(delay));
#endif
	spin_unlock_irqrestore(&ioc->fw_event_lock, flags);
}

/**
 * _scsih_queue_rescan - queue a topology rescan from user context
 * @ioc: per adapter object
 *
 * Return nothing.
 */
static void
_scsih_queue_rescan(struct MPT2SAS_ADAPTER *ioc)
{
	struct fw_event_work *fw_event;

	if (ioc->wait_for_port_enable_to_complete)
		return;
	fw_event = kzalloc(sizeof(struct fw_event_work), GFP_ATOMIC);
	if (!fw_event)
		return;
	fw_event->event = MPT2SAS_RESCAN_AFTER_HOST_RESET;
	fw_event->ioc = ioc;
	_scsih_fw_event_add(ioc, fw_event);
}

/**
 * _scsih_fw_event_cleanup_queue - cleanup event queue
 * @ioc: per adapter object
 *
 * Walk the firmware event queue, either killing timers, or waiting
 * for outstanding events to complete
 *
 * Return nothing.
 */
static void
_scsih_fw_event_cleanup_queue(struct MPT2SAS_ADAPTER *ioc)
{
	struct fw_event_work *fw_event, *next;

	if (list_empty(&ioc->fw_event_list) ||
	     !ioc->firmware_event_thread || in_interrupt())
		return;

	list_for_each_entry_safe(fw_event, next, &ioc->fw_event_list, list) {
#if (ESX_VERS > ESX41_VERS) || (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,19))
		if (fw_event->delayed_work_active &&
		    cancel_delayed_work(&fw_event->delayed_work)) {
			_scsih_fw_event_free(ioc, fw_event);
			continue;
		}
#else
		if (cancel_delayed_work(&fw_event->work)) {
			_scsih_fw_event_free(ioc, fw_event);
			continue;
		}
#endif
		fw_event->cancel_pending_work = 1;
	}
}

#if !defined(__VMKLNX__)
/**
 * _scsih_ublock_io_all_device - unblock every device
 * @ioc: per adapter object
 *
 * make sure device is reponsponding before unblocking
 */
static void
_scsih_ublock_io_all_device(struct MPT2SAS_ADAPTER *ioc)
{
	struct MPT2SAS_DEVICE *sas_device_priv_data;
	struct MPT2SAS_TARGET *sas_target;
	enum device_responsive_state rc;
	struct scsi_device *sdev;
	int count;

	shost_for_each_device(sdev, ioc->shost) {
		sas_device_priv_data = sdev->hostdata;
		if (!sas_device_priv_data)
			continue;
		sas_target = sas_device_priv_data->sas_target;
		if (!sas_target || sas_target->deleted)
			continue;
		if (!sas_device_priv_data->block)
			continue;
		count = 0;
		do {
			rc = _scsih_wait_for_device_to_become_ready(ioc,
			    sas_target->handle, 0, (sas_target->flags &
			    MPT_TARGET_FLAGS_RAID_COMPONENT), sdev->lun);
			if (rc == DEVICE_RETRY || rc == DEVICE_START_UNIT ||
			    rc == DEVICE_STOP_UNIT || rc == DEVICE_RETRY_UA)
				ssleep(1);
		} while ((rc == DEVICE_RETRY || rc == DEVICE_START_UNIT ||
		    rc == DEVICE_STOP_UNIT ||  rc == DEVICE_RETRY_UA) 
		    && count++ < command_retry_count);
		sas_device_priv_data->block = 0;
		if (rc != DEVICE_READY)
			sas_device_priv_data->deleted = 1;
		scsi_internal_device_unblock(sdev);
		if (rc != DEVICE_READY) {
			sdev_printk(KERN_WARNING, sdev, "device_offlined, "
			    "handle(0x%04x)\n",
			    sas_device_priv_data->sas_target->handle);
			scsi_device_set_state(sdev, SDEV_OFFLINE);
		} else
			sdev_printk(KERN_WARNING, sdev, "device_unblocked, "
			    "handle(0x%04x)\n",
			    sas_device_priv_data->sas_target->handle);
	}
}

/**
 * _scsih_ublock_io_device_wait - unblock IO for target
 * @ioc: per adapter object
 * @sas_addr: sas address
 *
 * make sure device is reponsponding before unblocking
 */
static void
_scsih_ublock_io_device_wait(struct MPT2SAS_ADAPTER *ioc, u64 sas_address)
{
	struct MPT2SAS_DEVICE *sas_device_priv_data;
	struct MPT2SAS_TARGET *sas_target;
	enum device_responsive_state rc;
	struct scsi_device *sdev;
	int count, keep_looping;

	/* moving devices from SDEV_OFFLINE to SDEV_BLOCK */
	shost_for_each_device(sdev, ioc->shost) {
		sas_device_priv_data = sdev->hostdata;
		if (!sas_device_priv_data)
			continue;
		sas_target = sas_device_priv_data->sas_target;
		if (!sas_target)
			continue;
		if (sas_target->sas_address != sas_address)
			continue;
		if (sdev->sdev_state == SDEV_OFFLINE) {
			sas_device_priv_data->block = 1;
			sas_device_priv_data->deleted = 0;
			scsi_device_set_state(sdev, SDEV_RUNNING);
			scsi_internal_device_block(sdev);
		}
	}

	/* moving devices from SDEV_BLOCK to SDEV_RUNNING state */
	count = 0;
	do {
		keep_looping = 0;
		shost_for_each_device(sdev, ioc->shost) {
			sas_device_priv_data = sdev->hostdata;
			if (!sas_device_priv_data)
				continue;
			sas_target = sas_device_priv_data->sas_target;
			if (!sas_target)
				continue;
			if (sas_target->sas_address != sas_address)
				continue;
			if (!sas_device_priv_data->block)
				continue;
			rc = _scsih_wait_for_device_to_become_ready(ioc,
			    sas_target->handle, 0, (sas_target->flags &
			    MPT_TARGET_FLAGS_RAID_COMPONENT), sdev->lun);
			if (rc == DEVICE_RETRY || rc == DEVICE_START_UNIT ||
			    rc == DEVICE_STOP_UNIT || rc == DEVICE_RETRY_UA) {
				keep_looping = 1;
				continue;
			}
			sas_device_priv_data->block = 0;
			if (rc != DEVICE_READY)
				sas_device_priv_data->deleted = 1;
			scsi_internal_device_unblock(sdev);
			if (rc != DEVICE_READY) {
				sdev_printk(KERN_WARNING, sdev,
				    "device_offlined, handle(0x%04x)\n",
				    sas_device_priv_data->sas_target->handle);
				scsi_device_set_state(sdev, SDEV_OFFLINE);
			} else
				sdev_printk(KERN_WARNING, sdev,
				    "device_unblocked, handle(0x%04x)\n",
				    sas_device_priv_data->sas_target->handle);
		}
		if (keep_looping)
			ssleep(1);
	} while (keep_looping && count++ < command_retry_count);
}

/**
 * _scsih_ublock_io_device - prepare device to be deleted
 * @ioc: per adapter object
 * @sas_addr: sas address
 *
 * unblock then put device in offline state
 */
static void
_scsih_ublock_io_device(struct MPT2SAS_ADAPTER *ioc, u64 sas_address)
{
	struct MPT2SAS_DEVICE *sas_device_priv_data;
	struct scsi_device *sdev;

	shost_for_each_device(sdev, ioc->shost) {
		sas_device_priv_data = sdev->hostdata;
		if (!sas_device_priv_data)
			continue;
		if (sas_device_priv_data->sas_target->sas_address
		    != sas_address)
			continue;
		if (sas_device_priv_data->block) {
			sas_device_priv_data->block = 0;
			scsi_internal_device_unblock(sdev);
		}
		scsi_device_set_state(sdev, SDEV_OFFLINE);
	}
}

/**
 * _scsih_block_io_device - set the device state to SDEV_BLOCK
 * @ioc: per adapter object
 * @handle: device handle
 *
 * During device pull we need to appropiately set the sdev state.
 */
static void
_scsih_block_io_device(struct MPT2SAS_ADAPTER *ioc, u16 handle)
{
	struct MPT2SAS_DEVICE *sas_device_priv_data;
	struct scsi_device *sdev;

	shost_for_each_device(sdev, ioc->shost) {
		sas_device_priv_data = sdev->hostdata;
		if (!sas_device_priv_data)
			continue;
		if (sas_device_priv_data->sas_target->handle != handle)
			continue;
		if (sas_device_priv_data->block)
			continue;
		sas_device_priv_data->block = 1;
		scsi_internal_device_block(sdev);
		sdev_printk(KERN_INFO, sdev, "device_blocked, "
		    "handle(0x%04x)\n", handle);
	}
}

#else
/**
 * _scsih_ublock_io_all_device - unblock every device
 * @ioc: per adapter object
 *
 * make sure device is reponsponding before unblocking
 */
static void
_scsih_ublock_io_all_device(struct MPT2SAS_ADAPTER *ioc)
{
	struct MPT2SAS_DEVICE *sas_device_priv_data;
	struct MPT2SAS_TARGET *sas_target;
	enum device_responsive_state rc;
	struct scsi_device *sdev;
	int count;

	shost_for_each_device(sdev, ioc->shost) {
		sas_device_priv_data = sdev->hostdata;
		if (!sas_device_priv_data)
			continue;
		sas_target = sas_device_priv_data->sas_target;
		if (!sas_target || sas_target->deleted)
			continue;
		count = 0;
		do {
			rc = _scsih_wait_for_device_to_become_ready(ioc,
			    sas_target->handle, 0,
			    (sas_target->flags & MPT_TARGET_FLAGS_RAID_COMPONENT),
			    sdev->lun);
			if (rc == DEVICE_RETRY || rc == DEVICE_START_UNIT ||
			    rc == DEVICE_STOP_UNIT ||rc == DEVICE_RETRY_UA)
				ssleep(1);
		} while ((rc == DEVICE_RETRY || rc == DEVICE_START_UNIT ||
		    rc == DEVICE_STOP_UNIT ||  rc == DEVICE_RETRY_UA)
		    && count++ < command_retry_count);
	}
}

/**
 * _scsih_ublock_io_device_wait - unblock IO for target
 * @ioc: per adapter object
 * @sas_addr: sas address
 *
 * make sure device is reponsponding before unblocking
 */
static void
_scsih_ublock_io_device_wait(struct MPT2SAS_ADAPTER *ioc, u64 sas_address)
{
	struct MPT2SAS_DEVICE *sas_device_priv_data;
	struct MPT2SAS_TARGET *sas_target;
	enum device_responsive_state rc;
	struct scsi_device *sdev;
	int count;

	/* unblock devices */
	shost_for_each_device(sdev, ioc->shost) {
		sas_device_priv_data = sdev->hostdata;
		if (!sas_device_priv_data)
			continue;
		sas_target = sas_device_priv_data->sas_target;
		if (!sas_target)
			continue;
		if (sas_device_priv_data->sas_target->sas_address != 
		    sas_address)
			continue;
		count = 0;
		do {
			rc = _scsih_wait_for_device_to_become_ready(ioc,
			    sas_target->handle, 0,
			    (sas_target->flags & MPT_TARGET_FLAGS_RAID_COMPONENT),
			    sdev->lun);
			if (rc == DEVICE_RETRY || rc == DEVICE_START_UNIT ||
			    rc == DEVICE_STOP_UNIT ||  rc == DEVICE_RETRY_UA)
				ssleep(1);
		} while ((rc == DEVICE_RETRY || rc == DEVICE_START_UNIT ||
		    rc == DEVICE_STOP_UNIT ||  rc == DEVICE_RETRY_UA)
		    && count++ < command_retry_count);
	}
}

/**
 * _scsih_ublock_io_device - unblock IO for target
 * @ioc: per adapter object
 * @sas_addr: SAS Address
 *
 * we don't care if device is responding or not
 */
static void
_scsih_ublock_io_device(struct MPT2SAS_ADAPTER *ioc, u64 sas_address)
{
}

/**
 * _scsih_block_io_device - set the device state to SDEV_BLOCK
 * @ioc: per adapter object
 * @handle: device handle
 *
 * During device pull we need to appropiately set the sdev state.
 */
static void
_scsih_block_io_device(struct MPT2SAS_ADAPTER *ioc, u16 handle)
{
}
#endif



/**
 * _scsih_block_io_to_children_attached_to_ex
 * @ioc: per adapter object
 * @sas_expander: the sas_device object
 *
 * This routine set sdev state to SDEV_BLOCK for all devices
 * attached to this expander. This function called when expander is
 * pulled.
 */
static void
_scsih_block_io_to_children_attached_to_ex(struct MPT2SAS_ADAPTER *ioc,
    struct _sas_node *sas_expander)
{
	struct _sas_port *mpt2sas_port;
	struct _sas_device *sas_device;
	struct _sas_node *expander_sibling;

	if (!sas_expander)
		return;

	list_for_each_entry(mpt2sas_port,
	   &sas_expander->sas_port_list, port_list) {
		if (mpt2sas_port->remote_identify.device_type ==
		    SAS_END_DEVICE) {
			spin_lock(&ioc->sas_device_lock);
			sas_device =
			    mpt2sas_scsih_sas_device_find_by_sas_address(ioc,
			   mpt2sas_port->remote_identify.sas_address);
			if (sas_device)
				set_bit(sas_device->handle,
				    ioc->blocking_handles);
			spin_unlock(&ioc->sas_device_lock);
		}
	}

	list_for_each_entry(mpt2sas_port,
	   &sas_expander->sas_port_list, port_list) {

		if (mpt2sas_port->remote_identify.device_type ==
		    MPI2_SAS_DEVICE_INFO_EDGE_EXPANDER ||
		    mpt2sas_port->remote_identify.device_type ==
		    MPI2_SAS_DEVICE_INFO_FANOUT_EXPANDER) {

			expander_sibling =
			    mpt2sas_scsih_expander_find_by_sas_address(
			    ioc, mpt2sas_port->remote_identify.sas_address);
			_scsih_block_io_to_children_attached_to_ex(ioc,
			    expander_sibling);
		}
	}
}

/**
 * _scsih_block_io_to_children_attached_directly
 * @ioc: per adapter object
 * @event_data: topology change event data
 *
 * This routine set sdev state to SDEV_BLOCK for all devices
 * direct attached during device pull.
 */
static void
_scsih_block_io_to_children_attached_directly(struct MPT2SAS_ADAPTER *ioc,
    Mpi2EventDataSasTopologyChangeList_t *event_data)
{
	int i;
	u16 handle;
	u16 reason_code;
	u8 phy_number;

	for (i = 0; i < event_data->NumEntries; i++) {
		handle = le16_to_cpu(event_data->PHY[i].AttachedDevHandle);
		if (!handle)
			continue;
		phy_number = event_data->StartPhyNum + i;
		reason_code = event_data->PHY[i].PhyStatus &
		    MPI2_EVENT_SAS_TOPO_RC_MASK;
		if (reason_code == MPI2_EVENT_SAS_TOPO_RC_DELAY_NOT_RESPONDING)
			_scsih_block_io_device(ioc, handle);
	}
}

/**
 * _scsih_tm_tr_send - send task management request
 * @ioc: per adapter object
 * @handle: device handle
 * Context: interrupt time.
 *
 * This code is to initiate the device removal handshake protocal
 * with controller firmware.  This function will issue target reset
 * using high priority request queue.  It will send a sas iounit
 * controll request (MPI2_SAS_OP_REMOVE_DEVICE) from this completion.
 *
 * This is designed to send muliple task management request at the same
 * time to the fifo. If the fifo is full, we will append the request,
 * and process it in a future completion.
 */
static void
_scsih_tm_tr_send(struct MPT2SAS_ADAPTER *ioc, u16 handle)
{
	Mpi2SCSITaskManagementRequest_t *mpi_request;
	u16 smid;
	struct _sas_device *sas_device;
	struct MPT2SAS_TARGET *sas_target_priv_data;
	unsigned long flags;
	struct _tr_list *delayed_tr;

	if (ioc->shost_recovery || ioc->remove_host ||
	    ioc->pci_error_recovery) {
		dewtprintk(ioc, printk(MPT2SAS_INFO_FMT "%s: host reset in "
		   "progress!\n", __func__, ioc->name));
		return;
	}

	/* if PD, then return */
	if (test_bit(handle, ioc->pd_handles))
		return;

	spin_lock_irqsave(&ioc->sas_device_lock, flags);
	sas_device = _scsih_sas_device_find_by_handle(ioc, handle);
	if (sas_device && sas_device->starget &&
	     sas_device->starget->hostdata) {
		sas_target_priv_data = sas_device->starget->hostdata;
		sas_target_priv_data->deleted = 1;
		dewtprintk(ioc, printk(MPT2SAS_INFO_FMT
		    "setting delete flag: handle(0x%04x), "
		    "sas_addr(0x%016llx)\n", ioc->name, handle,
		    (unsigned long long) sas_device->sas_address));
		_scsih_ublock_io_device(ioc, sas_device->sas_address);
		sas_target_priv_data->handle = MPT2SAS_INVALID_DEVICE_HANDLE;
	}
	spin_unlock_irqrestore(&ioc->sas_device_lock, flags);

	smid = mpt2sas_base_get_smid_hpr(ioc, ioc->tm_tr_cb_idx);
	if (!smid) {
		delayed_tr = kzalloc(sizeof(*delayed_tr), GFP_ATOMIC);
		if (!delayed_tr)
			return;
		INIT_LIST_HEAD(&delayed_tr->list);
		delayed_tr->handle = handle;
		list_add_tail(&delayed_tr->list, &ioc->delayed_tr_list);
		dewtprintk(ioc, printk(MPT2SAS_INFO_FMT
		    "DELAYED:tr:handle(0x%04x), (open)\n",
		    ioc->name, handle));
		return;
	}

	dewtprintk(ioc, printk(MPT2SAS_INFO_FMT "tr_send:handle(0x%04x), "
	    "(open), smid(%d), cb(%d)\n", ioc->name, handle, smid,
	    ioc->tm_tr_cb_idx));
	mpi_request = mpt2sas_base_get_msg_frame(ioc, smid);
	memset(mpi_request, 0, sizeof(Mpi2SCSITaskManagementRequest_t));
	mpi_request->Function = MPI2_FUNCTION_SCSI_TASK_MGMT;
	mpi_request->DevHandle = cpu_to_le16(handle);
	mpi_request->TaskType = MPI2_SCSITASKMGMT_TASKTYPE_TARGET_RESET;
	mpt2sas_base_put_smid_hi_priority(ioc, smid);
}

/**
 * _scsih_tm_tr_complete -
 * @ioc: per adapter object
 * @smid: system request message index
 * @msix_index: MSIX table index supplied by the OS
 * @reply: reply message frame(lower 32bit addr)
 * Context: interrupt time.
 *
 * This is the target reset completion routine.
 * This code is part of the code to initiate the device removal
 * handshake protocal with controller firmware.
 * It will send a sas iounit controll request (MPI2_SAS_OP_REMOVE_DEVICE)
 *
 * Return 1 meaning mf should be freed from _base_interrupt
 *        0 means the mf is freed from this function.
 */
static u8
_scsih_tm_tr_complete(struct MPT2SAS_ADAPTER *ioc, u16 smid, u8 msix_index,
    u32 reply)
{
	u16 handle;
	Mpi2SCSITaskManagementRequest_t *mpi_request_tm;
	Mpi2SCSITaskManagementReply_t *mpi_reply =
	    mpt2sas_base_get_reply_virt_addr(ioc, reply);
	Mpi2SasIoUnitControlRequest_t *mpi_request;
	u16 smid_sas_ctrl;

	if (ioc->shost_recovery || ioc->remove_host ||
	    ioc->pci_error_recovery) {
		dewtprintk(ioc, printk(MPT2SAS_INFO_FMT "%s: host reset in "
		   "progress!\n", __func__, ioc->name));
		return 1;
	}

	if (unlikely(!mpi_reply)) {
		printk(MPT2SAS_ERR_FMT "mpi_reply not valid at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		return 1;
	}

	mpi_request_tm = mpt2sas_base_get_msg_frame(ioc, smid);
	handle = le16_to_cpu(mpi_request_tm->DevHandle);
	if (handle != le16_to_cpu(mpi_reply->DevHandle)) {
		dewtprintk(ioc, printk("spurious interrupt: "
		    "handle(0x%04x:0x%04x), smid(%d)!!!\n", handle,
		    le16_to_cpu(mpi_reply->DevHandle), smid));
		return 0;
	}

	dewtprintk(ioc, printk(MPT2SAS_INFO_FMT
	    "tr_complete:handle(0x%04x), (open) smid(%d), ioc_status(0x%04x), "
	    "loginfo(0x%08x), completed(%d)\n", ioc->name,
	    handle, smid, le16_to_cpu(mpi_reply->IOCStatus),
	    le32_to_cpu(mpi_reply->IOCLogInfo),
	    le32_to_cpu(mpi_reply->TerminationCount)));

	smid_sas_ctrl = mpt2sas_base_get_smid(ioc, ioc->tm_sas_control_cb_idx);
	if (!smid_sas_ctrl) {
		printk(MPT2SAS_ERR_FMT "%s: failed obtaining a smid\n",
		    ioc->name, __func__);
		return 1;
	}

	dewtprintk(ioc, printk(MPT2SAS_INFO_FMT "sc_send:handle(0x%04x), "
	    "(open), smid(%d), cb(%d)\n", ioc->name, handle, smid_sas_ctrl,
	    ioc->tm_sas_control_cb_idx));
	mpi_request = mpt2sas_base_get_msg_frame(ioc, smid_sas_ctrl);
	memset(mpi_request, 0, sizeof(Mpi2SasIoUnitControlRequest_t));
	mpi_request->Function = MPI2_FUNCTION_SAS_IO_UNIT_CONTROL;
	mpi_request->Operation = MPI2_SAS_OP_REMOVE_DEVICE;
	mpi_request->DevHandle = mpi_request_tm->DevHandle;
	mpt2sas_base_put_smid_default(ioc, smid_sas_ctrl);

	return _scsih_check_for_pending_tm(ioc, smid);
}

/**
 * _scsih_sas_control_complete - completion routine
 * @ioc: per adapter object
 * @smid: system request message index
 * @msix_index: MSIX table index supplied by the OS
 * @reply: reply message frame(lower 32bit addr)
 * Context: interrupt time.
 *
 * This is the sas iounit controll completion routine.
 * This code is part of the code to initiate the device removal
 * handshake protocal with controller firmware.
 *
 * Return 1 meaning mf should be freed from _base_interrupt
 *        0 means the mf is freed from this function.
 */
static u8
_scsih_sas_control_complete(struct MPT2SAS_ADAPTER *ioc, u16 smid,
    u8 msix_index, u32 reply)
{
	Mpi2SasIoUnitControlReply_t *mpi_reply =
	    mpt2sas_base_get_reply_virt_addr(ioc, reply);

	if (likely(mpi_reply)) {
		dewtprintk(ioc, printk(MPT2SAS_INFO_FMT
		    "sc_complete:handle(0x%04x), (open) "
		    "smid(%d), ioc_status(0x%04x), loginfo(0x%08x)\n",
		    ioc->name, le16_to_cpu(mpi_reply->DevHandle), smid,
		    le16_to_cpu(mpi_reply->IOCStatus),
		    le32_to_cpu(mpi_reply->IOCLogInfo)));
	} else {
		printk(MPT2SAS_ERR_FMT "mpi_reply not valid at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
	}
	return 1;
}

/**
 * _scsih_tm_tr_volume_send - send target reset request for volumes
 * @ioc: per adapter object
 * @handle: device handle
 * Context: interrupt time.
 *
 * This is designed to send muliple task management request at the same
 * time to the fifo. If the fifo is full, we will append the request,
 * and process it in a future completion.
 */
static void
_scsih_tm_tr_volume_send(struct MPT2SAS_ADAPTER *ioc, u16 handle)
{
	Mpi2SCSITaskManagementRequest_t *mpi_request;
	u16 smid;
	struct _tr_list *delayed_tr;

	if (ioc->shost_recovery || ioc->remove_host ||
	    ioc->pci_error_recovery) {
		dewtprintk(ioc, printk(MPT2SAS_INFO_FMT "%s: host reset in "
		   "progress!\n", __func__, ioc->name));
		return;
	}

	smid = mpt2sas_base_get_smid_hpr(ioc, ioc->tm_tr_volume_cb_idx);
	if (!smid) {
		delayed_tr = kzalloc(sizeof(*delayed_tr), GFP_ATOMIC);
		if (!delayed_tr)
			return;
		INIT_LIST_HEAD(&delayed_tr->list);
		delayed_tr->handle = handle;
		list_add_tail(&delayed_tr->list, &ioc->delayed_tr_volume_list);
		dewtprintk(ioc, printk(MPT2SAS_INFO_FMT
		    "DELAYED:tr:handle(0x%04x), (open)\n",
		    ioc->name, handle));
		return;
	}

	dewtprintk(ioc, printk(MPT2SAS_INFO_FMT "tr_send:handle(0x%04x), "
	    "(open), smid(%d), cb(%d)\n", ioc->name, handle, smid,
	    ioc->tm_tr_volume_cb_idx));
	mpi_request = mpt2sas_base_get_msg_frame(ioc, smid);
	memset(mpi_request, 0, sizeof(Mpi2SCSITaskManagementRequest_t));
	mpi_request->Function = MPI2_FUNCTION_SCSI_TASK_MGMT;
	mpi_request->DevHandle = cpu_to_le16(handle);
	mpi_request->TaskType = MPI2_SCSITASKMGMT_TASKTYPE_TARGET_RESET;
	mpt2sas_base_put_smid_hi_priority(ioc, smid);
}

/**
 * _scsih_tm_volume_tr_complete - target reset completion
 * @ioc: per adapter object
 * @smid: system request message index
 * @msix_index: MSIX table index supplied by the OS
 * @reply: reply message frame(lower 32bit addr)
 * Context: interrupt time.
 *
 * Return 1 meaning mf should be freed from _base_interrupt
 *        0 means the mf is freed from this function.
 */
static u8
_scsih_tm_volume_tr_complete(struct MPT2SAS_ADAPTER *ioc, u16 smid,
    u8 msix_index, u32 reply)
{
	u16 handle;
	Mpi2SCSITaskManagementRequest_t *mpi_request_tm;
	Mpi2SCSITaskManagementReply_t *mpi_reply =
	    mpt2sas_base_get_reply_virt_addr(ioc, reply);

	if (ioc->shost_recovery || ioc->remove_host ||
	    ioc->pci_error_recovery) {
		dewtprintk(ioc, printk(MPT2SAS_INFO_FMT "%s: host reset in "
		   "progress!\n", __func__, ioc->name));
		return 1;
	}

	if (unlikely(!mpi_reply)) {
		printk(MPT2SAS_ERR_FMT "mpi_reply not valid at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		return 1;
	}

	mpi_request_tm = mpt2sas_base_get_msg_frame(ioc, smid);
	handle = le16_to_cpu(mpi_request_tm->DevHandle);
	if (handle != le16_to_cpu(mpi_reply->DevHandle)) {
		dewtprintk(ioc, printk("spurious interrupt: "
		    "handle(0x%04x:0x%04x), smid(%d)!!!\n", handle,
		    le16_to_cpu(mpi_reply->DevHandle), smid));
		return 0;
	}

	dewtprintk(ioc, printk(MPT2SAS_INFO_FMT
	    "tr_complete:handle(0x%04x), (open) smid(%d), ioc_status(0x%04x), "
	    "loginfo(0x%08x), completed(%d)\n", ioc->name,
	    handle, smid, le16_to_cpu(mpi_reply->IOCStatus),
	    le32_to_cpu(mpi_reply->IOCLogInfo),
	    le32_to_cpu(mpi_reply->TerminationCount)));

	return _scsih_check_for_pending_tm(ioc, smid);
}

/**
 * _scsih_tm_internal_tr_send - send target reset request 
 * @ioc: per adapter object
 * @handle: device handle
 * Context: interrupt time.
 *
 * This is designed to send multiple task management request (TR)at the same
 * time to the fifo. If the fifo is full, we will append the request,
 * and process it in a future completion.
 */
static void
_scsih_tm_internal_tr_send(struct MPT2SAS_ADAPTER *ioc, u16 handle)
{
	struct _tr_list *delayed_tr;
	Mpi2SCSITaskManagementRequest_t *mpi_request;
	u16 smid;

	
	smid = mpt2sas_base_get_smid_hpr(ioc, ioc->tm_tr_internal_cb_idx);
	if (!smid) {
		delayed_tr = kzalloc(sizeof(*delayed_tr), GFP_ATOMIC);
		if (!delayed_tr)
			return;
		INIT_LIST_HEAD(&delayed_tr->list);
		delayed_tr->handle = handle;
		list_add_tail(&delayed_tr->list, &ioc->delayed_internal_tm_list);
		dewtprintk(ioc, printk(MPT2SAS_INFO_FMT
		    "DELAYED:tr:handle(0x%04x), (open)\n",
		    ioc->name, handle));
		return;
	}

	dewtprintk(ioc, printk(MPT2SAS_INFO_FMT "tr_send:handle(0x%04x), "
	    "(open), smid(%d), cb(%d)\n", ioc->name, handle, smid,
	    ioc->tm_tr_internal_cb_idx));
	mpi_request = mpt2sas_base_get_msg_frame(ioc, smid);
	memset(mpi_request, 0, sizeof(Mpi2SCSITaskManagementRequest_t));
	mpi_request->Function = MPI2_FUNCTION_SCSI_TASK_MGMT;
	mpi_request->DevHandle = cpu_to_le16(handle);
	mpi_request->TaskType = MPI2_SCSITASKMGMT_TASKTYPE_TARGET_RESET;
	mpt2sas_base_put_smid_hi_priority(ioc, smid);
}

/**
 * _scsih_tm_internal_tr_complete - internal target reset completion
 * @ioc: per adapter object
 * @smid: system request message index
 * @msix_index: MSIX table index supplied by the OS
 * @reply: reply message frame(lower 32bit addr)
 * Context: interrupt time.
 *
 * Return 1 meaning mf should be freed from _base_interrupt
 *        0 means the mf is freed from this function.
 */
static u8
_scsih_tm_internal_tr_complete(struct MPT2SAS_ADAPTER *ioc, u16 smid,
	u8 msix_index, u32 reply)
{
	Mpi2SCSITaskManagementReply_t *mpi_reply =
	mpt2sas_base_get_reply_virt_addr(ioc, reply);


	if (likely(mpi_reply)) {
		dewtprintk(ioc, printk(MPT2SAS_INFO_FMT
		"tr_complete:handle(0x%04x), (open) "
		"smid(%d), ioc_status(0x%04x), loginfo(0x%08x)\n",
		ioc->name, le16_to_cpu(mpi_reply->DevHandle), smid,
		le16_to_cpu(mpi_reply->IOCStatus),
		le32_to_cpu(mpi_reply->IOCLogInfo)));

	} else {
		printk(MPT2SAS_ERR_FMT "mpi_reply not valid at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		return 1;
	}
	return _scsih_check_for_pending_tm(ioc, smid);;
}

#ifdef MPT2SAS_MULTIPATH
/**
 * _scsih_tm_tr_mp_send - send task management request (multipath)
 * @ioc: per adapter object
 * @handle: device handle
 * Context: interrupt time.
 *
 * This code is for sending target resets over to the active path
 * when there is a delay_not_responding event. The reason for this
 * code is to handle cases when target doesn't respond to commands
 * following cable pull.
 *
 * This is designed to send muliple task management request at the same
 * time to the fifo. If the fifo is full, we will append the request,
 * and process it in a future completion.
 */
static void
_scsih_tm_tr_mp_send(struct MPT2SAS_ADAPTER *ioc, u16 handle)
{
	Mpi2SCSITaskManagementRequest_t *mpi_request;
	struct MPT2SAS_TARGET *sas_target_priv_data;
	u16 smid;
	struct _sas_device *sas_device;
	unsigned long flags;
	struct MPT2SAS_ADAPTER *ioc_alt;
	struct _sas_device *sas_device_alt;
	struct _tr_list *delayed_tr;

	spin_lock_irqsave(&ioc->sas_device_lock, flags);
	sas_device = _scsih_sas_device_find_by_handle(ioc, handle);

	if (!sas_device) {
		spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
		return;
	}

	if (!sas_device->sas_device_alt) {
		spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
		return;
	}

	sas_device_alt = sas_device->sas_device_alt;
	ioc_alt = sas_device_alt->ioc;
	spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
	spin_lock_irqsave(&ioc_alt->sas_device_lock, flags);

	if (!sas_device_alt->starget) {
		spin_unlock_irqrestore(&ioc_alt->sas_device_lock, flags);
		return;
	}

	if (ioc_alt->shost_recovery || ioc_alt->remove_host ||
	    ioc_alt->pci_error_recovery) {
		dewtprintk(ioc_alt, printk(MPT2SAS_INFO_FMT "%s: host reset in "
		   "progress!\n", __func__, ioc_alt->name));
		spin_unlock_irqrestore(&ioc_alt->sas_device_lock, flags);
		return;
	}

	smid = mpt2sas_base_get_smid_hpr(ioc_alt, ioc->tm_tr_mp_cb_idx);
	if (!smid) {
		delayed_tr = kzalloc(sizeof(*delayed_tr), GFP_ATOMIC);
		if (!delayed_tr) {
			spin_unlock_irqrestore(&ioc_alt->sas_device_lock,
			    flags);
			return;
		}
		INIT_LIST_HEAD(&delayed_tr->list);
		delayed_tr->handle = handle;
		list_add_tail(&delayed_tr->list,
		    &ioc->delayed_tr_mp_list);
		dewtprintk(ioc, starget_printk(KERN_INFO,
		    sas_device_alt->starget, "DELAYED:tr:handle(0x%04x), "
		    "(active)\n", sas_device_alt->handle));
		spin_unlock_irqrestore(&ioc_alt->sas_device_lock, flags);
		return;
	}

	if (sas_device_alt->starget->hostdata) {
		sas_target_priv_data = sas_device_alt->starget->hostdata;
		sas_target_priv_data->tm_busy = 1;
	}

	dewtprintk(ioc, starget_printk(KERN_INFO, sas_device_alt->starget,
	    "tr_send:handle(0x%04x), (active)\n", sas_device_alt->handle));

	mpi_request = mpt2sas_base_get_msg_frame(ioc_alt, smid);
	memset(mpi_request, 0, sizeof(Mpi2SCSITaskManagementRequest_t));
	mpi_request->Function = MPI2_FUNCTION_SCSI_TASK_MGMT;
	mpi_request->DevHandle = cpu_to_le16(sas_device_alt->handle);
	mpi_request->TaskType = MPI2_SCSITASKMGMT_TASKTYPE_TARGET_RESET;
	spin_unlock_irqrestore(&ioc_alt->sas_device_lock, flags);
	mpt2sas_base_put_smid_hi_priority(ioc_alt, smid);
}

/**
 * _scsih_tm_tr_mp_complete -
 * @ioc: per adapter object
 * @smid: system request message index
 * @msix_index: MSIX table index supplied by the OS
 * @reply: reply message frame(lower 32bit addr)
 * Context: interrupt time.
 *
 * This is the target reset completion routine.
 *
 * Return 1 meaning mf should be freed from _base_interrupt
 *        0 means the mf is freed from this function.
 */
static u8
_scsih_tm_tr_mp_complete(struct MPT2SAS_ADAPTER *ioc, u16 smid, u8 msix_index,
    u32 reply)
{
	u16 handle;
	Mpi2SCSITaskManagementRequest_t *mpi_request;
	Mpi2SCSITaskManagementReply_t *mpi_reply =
	    mpt2sas_base_get_reply_virt_addr(ioc, reply);
	struct _sas_device *sas_device;
	unsigned long flags;
	struct MPT2SAS_TARGET *sas_target_priv_data;

	if (unlikely(!mpi_reply)) {
		printk(MPT2SAS_ERR_FMT "mpi_reply not valid at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		return 1;
	}

	mpi_request = mpt2sas_base_get_msg_frame(ioc, smid);
	handle = le16_to_cpu(mpi_request->DevHandle);
	if (handle != le16_to_cpu(mpi_reply->DevHandle)) {
		dewtprintk(ioc, printk("spurious interrupt: "
		    "handle(0x%04x:0x%04x), smid(%d)!!!\n", handle,
		    le16_to_cpu(mpi_reply->DevHandle), smid));
		return 0;
	}

	spin_lock_irqsave(&ioc->sas_device_lock, flags);
	sas_device = _scsih_sas_device_find_by_handle(ioc, handle);
	if (sas_device && sas_device->starget->hostdata) {
		sas_target_priv_data = sas_device->starget->hostdata;
		sas_target_priv_data->tm_busy = 0;
	}
	spin_unlock_irqrestore(&ioc->sas_device_lock, flags);

	dewtprintk(ioc, printk(MPT2SAS_INFO_FMT
	    "tr_complete:handle(0x%04x), (active) ioc_status(0x%04x), "
	    "loginfo(0x%08x), completed(%d)\n", ioc->name,
	    handle, le16_to_cpu(mpi_reply->IOCStatus),
	    le32_to_cpu(mpi_reply->IOCLogInfo),
	    le32_to_cpu(mpi_reply->TerminationCount)));

	return _scsih_check_for_pending_tm(ioc, smid);
}
#endif

/**
 * _scsih_check_for_pending_tm - check for pending task management
 * @ioc: per adapter object
 * @smid: system request message index
 *
 * This will check delayed target reset list, and feed the
 * next reqeust.
 *
 * Return 1 meaning mf should be freed from _base_interrupt
 *        0 means the mf is freed from this function.
 */
static u8
_scsih_check_for_pending_tm(struct MPT2SAS_ADAPTER *ioc, u16 smid)
{
	struct _tr_list *delayed_tr;

	if (!list_empty(&ioc->delayed_tr_volume_list)) {
		delayed_tr = list_entry(ioc->delayed_tr_volume_list.next,
		    struct _tr_list, list);
		mpt2sas_base_free_smid(ioc, smid);
		_scsih_tm_tr_volume_send(ioc, delayed_tr->handle);
		list_del(&delayed_tr->list);
		kfree(delayed_tr);
		return 0;
	}

	if (!list_empty(&ioc->delayed_tr_list)) {
		delayed_tr = list_entry(ioc->delayed_tr_list.next,
		    struct _tr_list, list);
		mpt2sas_base_free_smid(ioc, smid);
		_scsih_tm_tr_send(ioc, delayed_tr->handle);
		list_del(&delayed_tr->list);
		kfree(delayed_tr);
		return 0;
	}

	if (!list_empty(&ioc->delayed_internal_tm_list)) {
		delayed_tr = list_entry(ioc->delayed_internal_tm_list.next,
		    struct _tr_list, list);
		mpt2sas_base_free_smid(ioc, smid);
		_scsih_tm_internal_tr_send(ioc, delayed_tr->handle);
		list_del(&delayed_tr->list);
		kfree(delayed_tr);
		return 0;
	}

#ifdef MPT2SAS_MULTIPATH
	if (!list_empty(&ioc->delayed_tr_mp_list)) {
		delayed_tr = list_entry(ioc->delayed_tr_mp_list.next,
		    struct _tr_list, list);
		mpt2sas_base_free_smid(ioc, smid);
		_scsih_tm_tr_mp_send(ioc, delayed_tr->handle);
		list_del(&delayed_tr->list);
		kfree(delayed_tr);
		return 0;
	}
#endif
	return 1;
}

/**
 * _scsih_check_topo_delete_events - sanity check on topo events
 * @ioc: per adapter object
 * @event_data: the event data payload
 *
 * This routine added to better handle cable breaker.
 *
 * This handles the case where driver recieves multiple expander
 * add and delete events in a single shot.  When there is a delete event
 * the routine will void any pending add events waiting in the event queue.
 *
 * Return nothing.
 */
static void
_scsih_check_topo_delete_events(struct MPT2SAS_ADAPTER *ioc,
    Mpi2EventDataSasTopologyChangeList_t *event_data)
{
	struct fw_event_work *fw_event;
	Mpi2EventDataSasTopologyChangeList_t *local_event_data;
	u16 expander_handle;
	struct _sas_node *sas_expander;
	unsigned long flags;
	int i, reason_code;
	u16 handle;

	for (i = 0 ; i < event_data->NumEntries; i++) {
		handle = le16_to_cpu(event_data->PHY[i].AttachedDevHandle);
		if (!handle)
			continue;
		reason_code = event_data->PHY[i].PhyStatus &
		    MPI2_EVENT_SAS_TOPO_RC_MASK;
		if (reason_code == MPI2_EVENT_SAS_TOPO_RC_TARG_NOT_RESPONDING)
			_scsih_tm_tr_send(ioc, handle);
#ifdef MPT2SAS_MULTIPATH
		if (reason_code == MPI2_EVENT_SAS_TOPO_RC_DELAY_NOT_RESPONDING)
			_scsih_tm_tr_mp_send(ioc, handle);
#endif
	}

	expander_handle = le16_to_cpu(event_data->ExpanderDevHandle);
	if (expander_handle < ioc->sas_hba.num_phys) {
		_scsih_block_io_to_children_attached_directly(ioc, event_data);
		return;
	}

	if (event_data->ExpStatus == MPI2_EVENT_SAS_TOPO_ES_DELAY_NOT_RESPONDING
	 || event_data->ExpStatus == MPI2_EVENT_SAS_TOPO_ES_NOT_RESPONDING) {
		spin_lock_irqsave(&ioc->sas_node_lock, flags);
		sas_expander = mpt2sas_scsih_expander_find_by_handle(ioc,
		    expander_handle);
		_scsih_block_io_to_children_attached_to_ex(ioc, sas_expander);
		spin_unlock_irqrestore(&ioc->sas_node_lock, flags);
		do {
			handle = find_first_bit(ioc->blocking_handles,
			    ioc->facts.MaxDevHandle);
			if (handle < ioc->facts.MaxDevHandle)
				_scsih_block_io_device(ioc, handle);
		} while (test_and_clear_bit(handle, ioc->blocking_handles));
	} else if (event_data->ExpStatus == MPI2_EVENT_SAS_TOPO_ES_RESPONDING)
		_scsih_block_io_to_children_attached_directly(ioc, event_data);

	if (event_data->ExpStatus != MPI2_EVENT_SAS_TOPO_ES_NOT_RESPONDING)
		return;

	/* mark ignore flag for pending events */
	spin_lock_irqsave(&ioc->fw_event_lock, flags);
	list_for_each_entry(fw_event, &ioc->fw_event_list, list) {
		if (fw_event->event != MPI2_EVENT_SAS_TOPOLOGY_CHANGE_LIST ||
		    fw_event->ignore)
			continue;
		local_event_data = fw_event->event_data;
		if (local_event_data->ExpStatus ==
		    MPI2_EVENT_SAS_TOPO_ES_ADDED ||
		    local_event_data->ExpStatus ==
		    MPI2_EVENT_SAS_TOPO_ES_RESPONDING) {
			if (le16_to_cpu(local_event_data->ExpanderDevHandle) ==
			    expander_handle) {
				dewtprintk(ioc, printk(MPT2SAS_INFO_FMT
				    "setting ignoring flag\n", ioc->name));
				fw_event->ignore = 1;
			}
		}
	}
	spin_unlock_irqrestore(&ioc->fw_event_lock, flags);
}

/**
 * _scsih_set_volume_delete_flag - setting volume delete flag
 * @ioc: per adapter object
 * @handle: device handle
 *
 * This Return nothing.
 */
static void
_scsih_set_volume_delete_flag(struct MPT2SAS_ADAPTER *ioc, u16 handle)
{
	struct _raid_device *raid_device;
	struct MPT2SAS_TARGET *sas_target_priv_data;
	unsigned long flags;

	spin_lock_irqsave(&ioc->raid_device_lock, flags);
	raid_device = _scsih_raid_device_find_by_handle(ioc, handle);
	if (raid_device && raid_device->starget &&
	    raid_device->starget->hostdata) {
		sas_target_priv_data =
		    raid_device->starget->hostdata;
		sas_target_priv_data->deleted = 1;
		dewtprintk(ioc, printk(MPT2SAS_INFO_FMT
		    "setting delete flag: handle(0x%04x), "
		    "wwid(0x%016llx)\n", ioc->name, handle,
		    (unsigned long long) raid_device->wwid));
	}
	spin_unlock_irqrestore(&ioc->raid_device_lock, flags);
}

/**
 * _scsih_set_volume_handle_for_tr - set handle for target reset to volume
 * @handle: input handle
 * @a: handle for volume a
 * @b: handle for volume b
 *
 * IR firmware only supports two raid volumes.  The purpose of this
 * routine is to set the volume handle in either a or b. When the given
 * input handle is non-zero, or when a and b have not been set before.
 */
static void
_scsih_set_volume_handle_for_tr(u16 handle, u16 *a, u16 *b)
{
	if (!handle || handle == *a || handle == *b)
		return;
	if (!*a)
		*a = handle;
	else if (!*b)
		*b = handle;
}

/**
 * _scsih_check_ir_config_unhide_events - check for UNHIDE events
 * @ioc: per adapter object
 * @event_data: the event data payload
 * Context: interrupt time.
 *
 * This routine will send target reset to volume, followed by target
 * resets to the PDs. This is called when a PD has been removed, or
 * volume has been deleted or removed. When the target reset is sent
 * to volume, the PD target resets need to be queued to start upon
 * completion of the volume target reset.
 *
 * Return nothing.
 */
static void
_scsih_check_ir_config_unhide_events(struct MPT2SAS_ADAPTER *ioc,
    Mpi2EventDataIrConfigChangeList_t *event_data)
{
	Mpi2EventIrConfigElement_t *element;
	int i;
	u16 handle, volume_handle, a, b;
	struct _tr_list *delayed_tr;

	a = 0;
	b = 0;

	if (ioc->is_warpdrive)
		return;

	/* Volume Resets for Deleted or Removed */
	element = (Mpi2EventIrConfigElement_t *)&event_data->ConfigElement[0];
	for (i = 0; i < event_data->NumElements; i++, element++) {
		if (le32_to_cpu(event_data->Flags) &
		    MPI2_EVENT_IR_CHANGE_FLAGS_FOREIGN_CONFIG)
			continue;
		if (element->ReasonCode ==
		    MPI2_EVENT_IR_CHANGE_RC_VOLUME_DELETED ||
		    element->ReasonCode ==
		    MPI2_EVENT_IR_CHANGE_RC_REMOVED) {
			volume_handle = le16_to_cpu(element->VolDevHandle);
			_scsih_set_volume_delete_flag(ioc, volume_handle);
			_scsih_set_volume_handle_for_tr(volume_handle, &a, &b);
		}
	}

	/* Volume Resets for UNHIDE events */
	element = (Mpi2EventIrConfigElement_t *)&event_data->ConfigElement[0];
	for (i = 0; i < event_data->NumElements; i++, element++) {
		if (le32_to_cpu(event_data->Flags) &
		    MPI2_EVENT_IR_CHANGE_FLAGS_FOREIGN_CONFIG)
			continue;
		if (element->ReasonCode == MPI2_EVENT_IR_CHANGE_RC_UNHIDE) {
			volume_handle = le16_to_cpu(element->VolDevHandle);
			_scsih_set_volume_handle_for_tr(volume_handle, &a, &b);
		}
	}

	if (a)
		_scsih_tm_tr_volume_send(ioc, a);
	if (b)
		_scsih_tm_tr_volume_send(ioc, b);

	/* PD target resets */
	element = (Mpi2EventIrConfigElement_t *)&event_data->ConfigElement[0];
	for (i = 0; i < event_data->NumElements; i++, element++) {
		if (element->ReasonCode != MPI2_EVENT_IR_CHANGE_RC_UNHIDE)
			continue;
		handle = le16_to_cpu(element->PhysDiskDevHandle);
		volume_handle = le16_to_cpu(element->VolDevHandle);
		clear_bit(handle, ioc->pd_handles);
		if (!volume_handle)
			_scsih_tm_tr_send(ioc, handle);
		else if (volume_handle == a || volume_handle == b) {
			delayed_tr = kzalloc(sizeof(*delayed_tr), GFP_ATOMIC);
			BUG_ON(!delayed_tr);
			INIT_LIST_HEAD(&delayed_tr->list);
			delayed_tr->handle = handle;
			list_add_tail(&delayed_tr->list, &ioc->delayed_tr_list);
			dewtprintk(ioc, printk(MPT2SAS_INFO_FMT
			    "DELAYED:tr:handle(0x%04x), (open)\n", ioc->name,
			    handle));
		} else
			_scsih_tm_tr_send(ioc, handle);
	}
}


/**
 * _scsih_check_volume_delete_events - set delete flag for volumes
 * @ioc: per adapter object
 * @event_data: the event data payload
 * Context: interrupt time.
 *
 * This will handle the case when the cable connected to entire volume is
 * pulled. We will take care of setting the deleted flag so normal IO will
 * not be sent.
 *
 * Return nothing.
 */
static void
_scsih_check_volume_delete_events(struct MPT2SAS_ADAPTER *ioc,
    Mpi2EventDataIrVolume_t *event_data)
{
	u32 state;

	if (event_data->ReasonCode != MPI2_EVENT_IR_VOLUME_RC_STATE_CHANGED)
		return;
	state = le32_to_cpu(event_data->NewValue);
	if (state == MPI2_RAID_VOL_STATE_MISSING || state ==
	    MPI2_RAID_VOL_STATE_FAILED)
		_scsih_set_volume_delete_flag(ioc,
		    le16_to_cpu(event_data->VolDevHandle));
}

/**
 * _scsih_temp_threshold_events - display temperature threshold exceeded events
 * @ioc: per adapter object
 * @event_data: the temp threshold event data
 * Context: interrupt time.
 *
 * Return nothing.
 */
static void
_scsih_temp_threshold_events(struct MPT2SAS_ADAPTER *ioc,
	Mpi2EventDataTemperature_t *event_data)
{
	if (ioc->temp_sensors_count >= event_data->SensorNum) {
		printk(MPT2SAS_ERR_FMT "Temperature Threshold flags %s%s%s%s" 
			"exceeded for Sensor: %d !!!\n", ioc->name,
			((event_data->Status & 0x1) == 1) ? "0 " : " ",
			((event_data->Status & 0x2) == 2) ? "1 " : " ",
			((event_data->Status & 0x4) == 4) ? "2 " : " ",
			((event_data->Status & 0x8) == 8) ? "3 " : " ",
			event_data->SensorNum);
		printk(MPT2SAS_ERR_FMT "Current Temp In Celsius: %d\n",
			ioc->name, event_data->CurrentTemperature);
	}
}


/**
 * _scsih_flush_running_cmds - completing outstanding commands.
 * @ioc: per adapter object
 *
 * The flushing out of all pending scmd commands following host reset,
 * where all IO is dropped to the floor.
 *
 * Return nothing.
 */
static void
_scsih_flush_running_cmds(struct MPT2SAS_ADAPTER *ioc)
{
	struct scsi_cmnd *scmd;
	u16 smid;
	u16 count = 0;

	for (smid = 1; smid <= ioc->scsiio_depth; smid++) {
		scmd = _scsih_scsi_lookup_get_clear(ioc, smid);
		if (!scmd)
			continue;
		count++;
		mpt2sas_base_free_smid(ioc, smid);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23))
		if (scmd->use_sg) {
			pci_unmap_sg(ioc->pdev,
			    (struct scatterlist *) scmd->request_buffer,
			    scmd->use_sg, scmd->sc_data_direction);
		} else if (scmd->request_bufflen) {
			pci_unmap_single(ioc->pdev,
			    scmd->SCp.dma_handle, scmd->request_bufflen,
			    scmd->sc_data_direction);
		}
#else
		scsi_dma_unmap(scmd);
#endif
		if (ioc->pci_error_recovery)
			scmd->result = DID_NO_CONNECT << 16;
		else
			scmd->result = DID_RESET << 16;
		scmd->scsi_done(scmd);
	}
	dtmprintk(ioc, printk(MPT2SAS_INFO_FMT "completing %d cmds\n",
	    ioc->name, count));
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27))
static u8 opcode_protection[256] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, PRO_R, 0, PRO_W, 0, 0, 0, PRO_W, PRO_V,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, PRO_W, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, PRO_R, 0, PRO_W, 0, 0, 0, PRO_W, PRO_V,
	0, 0, 0, PRO_W, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, PRO_R, 0, PRO_W, 0, 0, 0, PRO_W, PRO_V,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
#endif
/**
 * _scsih_setup_eedp - setup MPI request for EEDP transfer
 * @scmd: pointer to scsi command object
 * @mpi_request: pointer to the SCSI_IO reqest message frame
 *
 * Supporting protection 1 and 3.
 *
 * Returns nothing
 */
static void
_scsih_setup_eedp(struct scsi_cmnd *scmd, Mpi2SCSIIORequest_t *mpi_request)
{
	u16 eedp_flags;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27))
	struct MPT2SAS_DEVICE *sas_device_priv_data;
	u8 scsi_opcode;

	sas_device_priv_data = scmd->device->hostdata;

	if (!sas_device_priv_data->eedp_enable)
		return;

	/* check whether scsi opcode supports eedp transfer */
	scsi_opcode = scmd->cmnd[0];
	eedp_flags = opcode_protection[scsi_opcode];
	if (!eedp_flags)
		return;

	/* set RDPROTECT, WRPROTECT, VRPROTECT bits to (001b) */
	scmd->cmnd[1] = (scmd->cmnd[1] & 0x1F) | 0x20;

	switch (sas_device_priv_data->eedp_type) {
	case 1: /* type 1 */

		/*
		* enable ref/guard checking
		* auto increment ref tag
		*/
		eedp_flags |= MPI2_SCSIIO_EEDPFLAGS_INC_PRI_REFTAG |
		    MPI2_SCSIIO_EEDPFLAGS_CHECK_REFTAG |
		    MPI2_SCSIIO_EEDPFLAGS_CHECK_GUARD;
		mpi_request->CDB.EEDP32.PrimaryReferenceTag =
		    cpu_to_be32(scsi_get_lba(scmd));

		break;

	case 3: /* type 3 */

		/*
		* enable guard checking
		*/
		eedp_flags |= MPI2_SCSIIO_EEDPFLAGS_CHECK_GUARD;

		break;
	}

#else /* sles11 and newer */

	unsigned char prot_op = scsi_get_prot_op(scmd);
	unsigned char prot_type = scsi_get_prot_type(scmd);

	if (prot_type == SCSI_PROT_DIF_TYPE0 || prot_op == SCSI_PROT_NORMAL)
		return;

	if (prot_op ==  SCSI_PROT_READ_STRIP)
		eedp_flags = MPI2_SCSIIO_EEDPFLAGS_CHECK_REMOVE_OP;
	else if (prot_op ==  SCSI_PROT_WRITE_INSERT)
		eedp_flags = MPI2_SCSIIO_EEDPFLAGS_INSERT_OP;
	else
		return;

	switch (prot_type) {
	case SCSI_PROT_DIF_TYPE1:
	case SCSI_PROT_DIF_TYPE2:

		/*
		* enable ref/guard checking
		* auto increment ref tag
		*/
		eedp_flags |= MPI2_SCSIIO_EEDPFLAGS_INC_PRI_REFTAG |
		    MPI2_SCSIIO_EEDPFLAGS_CHECK_REFTAG |
		    MPI2_SCSIIO_EEDPFLAGS_CHECK_GUARD;
		mpi_request->CDB.EEDP32.PrimaryReferenceTag =
		    cpu_to_be32(scsi_get_lba(scmd));
		break;

	case SCSI_PROT_DIF_TYPE3:

		/*
		* enable guard checking
		*/
		eedp_flags |= MPI2_SCSIIO_EEDPFLAGS_CHECK_GUARD;
		break;
	}
#endif
	mpi_request->EEDPBlockSize = cpu_to_le32(scmd->device->sector_size);
	mpi_request->EEDPFlags = cpu_to_le16(eedp_flags);
}

/**
 * _scsih_eedp_error_handling - return sense code for EEDP errors
 * @scmd: pointer to scsi command object
 * @ioc_status: ioc status
 *
 * Returns nothing
 */
static void
_scsih_eedp_error_handling(struct scsi_cmnd *scmd, u16 ioc_status)
{
	u8 ascq;
	u8 sk;
	u8 host_byte;

	switch (ioc_status) {
	case MPI2_IOCSTATUS_EEDP_GUARD_ERROR:
		ascq = 0x01;
		break;
	case MPI2_IOCSTATUS_EEDP_APP_TAG_ERROR:
		ascq = 0x02;
		break;
	case MPI2_IOCSTATUS_EEDP_REF_TAG_ERROR:
		ascq = 0x03;
		break;
	default:
		ascq = 0x00;
		break;
	}

	if (scmd->sc_data_direction == DMA_TO_DEVICE) {
		sk = ILLEGAL_REQUEST;
		host_byte = DID_ABORT;
	} else {
		sk = ABORTED_COMMAND;
		host_byte = DID_OK;
	}

	mpt_scsi_build_sense_buffer(0, scmd->sense_buffer, sk, 0x10, ascq);
	scmd->result = DRIVER_SENSE << 24 | (host_byte << 16) |
	    SAM_STAT_CHECK_CONDITION;
}

/**
 * _scsih_scsi_direct_io_get - returns direct io flag
 * @ioc: per adapter object
 * @smid: system request message index
 *
 * Returns the smid stored scmd pointer.
 */
static inline u8
_scsih_scsi_direct_io_get(struct MPT2SAS_ADAPTER *ioc, u16 smid)
{
	return ioc->scsi_lookup[smid - 1].direct_io;
}

/**
 * _scsih_scsi_direct_io_set - sets direct io flag
 * @ioc: per adapter object
 * @smid: system request message index
 * @direct_io: Zero or non-zero value to set in the direct_io flag
 *
 * Returns Nothing.
 */
static inline void
_scsih_scsi_direct_io_set(struct MPT2SAS_ADAPTER *ioc, u16 smid, u8 direct_io)
{
	ioc->scsi_lookup[smid - 1].direct_io = direct_io;
}

/**
 * _scsih_setup_direct_io - setup MPI request for WARPDRIVE Direct I/O
 * @ioc: per adapter object
 * @scmd: pointer to scsi command object
 * @raid_device: pointer to raid device data structure
 * @mpi_request: pointer to the SCSI_IO reqest message frame
 * @smid: system request message index
 *
 * Returns nothing
 */
static void
_scsih_setup_direct_io(struct MPT2SAS_ADAPTER *ioc, struct scsi_cmnd *scmd,
	struct _raid_device *raid_device, Mpi2SCSIIORequest_t *mpi_request,
	u16 smid)
{
	u32 p_lba, stripe_off, stripe_unit, column, io_size;
	u64 v_lba;
	u32 stripe_sz, stripe_exp;
	u8 num_pds, *cdb_ptr, i;
	u8 cdb0 = scmd->cmnd[0];
	u64 v_llba;

	/*
	 * Try Direct I/O to RAID memeber disks
	 */
	if (cdb0 == READ_16 || cdb0 == READ_10 ||
	    cdb0 == WRITE_16 || cdb0 == WRITE_10) {
		cdb_ptr = mpi_request->CDB.CDB32;

		if ((cdb0 < READ_16) || !(cdb_ptr[2] | cdb_ptr[3] | cdb_ptr[4]
			| cdb_ptr[5])) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23))
			io_size = scmd->request_bufflen >>
			    raid_device->block_exponent;
#else
			io_size = scsi_bufflen(scmd) >>
			    raid_device->block_exponent;
#endif

			i = 2;  

			if ((cdb0 < READ_16)) {
			    v_lba = be32_to_cpu(*(__be32 *)(&cdb_ptr[i]));
			}
			else {
			    v_lba = be64_to_cpu(*(__be64 *)(&cdb_ptr[i]));
			}

			i = (cdb0 < READ_16) ? 2 : 6;
			if (((u64)v_lba + (u64)io_size - 1) <= raid_device->max_lba) {
				stripe_sz = raid_device->stripe_sz;
				stripe_exp = raid_device->stripe_exponent;
				stripe_off = v_lba & (stripe_sz - 1);

				/* Check whether IO falls within a stripe */
				if ((stripe_off + io_size) <= stripe_sz) {
					num_pds = raid_device->num_pds;
					p_lba = v_lba >> stripe_exp;
					stripe_unit = p_lba / num_pds;
					column = p_lba % num_pds;
					p_lba = (stripe_unit << stripe_exp) +
					    stripe_off;
					mpi_request->DevHandle =
						cpu_to_le16(raid_device->
						    pd_handle[column]);
					(*(__be32 *)(&cdb_ptr[i])) =
					    cpu_to_be32(p_lba);
					/*
					* WD: To indicate this I/O is directI/O
					*/
					_scsih_scsi_direct_io_set(ioc, smid, 1);
#ifdef MPT2SAS_WD_DDIOCOUNT
					ioc->ddio_count++;
#endif
#ifdef MPT2SAS_WD_LOGGING
					if (ioc->logging_level &
					    MPT_DEBUG_SCSI) {
						printk(MPT2SAS_INFO_FMT
						    "scmd(%p) as direct IO\n",
						    ioc->name, scmd);
						scsi_print_command(scmd);
					}
#endif
				}
			}
		} else {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23))
			io_size = scmd->request_bufflen >>
			    raid_device->block_exponent;
#else
			io_size = scsi_bufflen(scmd) >>
			    raid_device->block_exponent;
#endif
			/* get virtual lba */
			v_llba = be64_to_cpu(*(__be64 *)(&cdb_ptr[2]));

			if ((v_llba + (u64)io_size - 1) <=
			    raid_device->max_lba) {
				stripe_sz = raid_device->stripe_sz;
				stripe_exp = raid_device->stripe_exponent;
				stripe_off = (u32) (v_llba & (stripe_sz - 1));

				/* Check whether IO falls within a stripe */
				if ((stripe_off + io_size) <= stripe_sz) {
					num_pds = raid_device->num_pds;
					p_lba = (u32)(v_llba >> stripe_exp);
					stripe_unit = p_lba / num_pds;
					column = p_lba % num_pds;
					p_lba = (stripe_unit << stripe_exp) +
					    stripe_off;
					mpi_request->DevHandle =
						cpu_to_le16(raid_device->
						    pd_handle[column]);
					(*(__be64 *)(&cdb_ptr[2])) =
					    cpu_to_be64((u64)p_lba);
					/*
					* WD: To indicate this I/O is directI/O
					*/
					_scsih_scsi_direct_io_set(ioc, smid, 1);
#ifdef MPT2SAS_WD_DDIOCOUNT
					ioc->ddio_count++;
#endif
#ifdef MPT2SAS_WD_LOGGING
					if (ioc->logging_level &
					    MPT_DEBUG_SCSI) {
						printk(MPT2SAS_INFO_FMT
						    "scmd(%p) as direct IO\n",
						    ioc->name, scmd);
						scsi_print_command(scmd);
					}
#endif
				}
			}
		}
	}
}

/**
 * _scsih_qcmd - main scsi request entry point
 * @scmd: pointer to scsi command object
 * @done: function pointer to be invoked on completion
 *
 * The callback index is set inside `ioc->scsi_io_cb_idx`.
 *
 * Returns 0 on success.  If there's a failure, return either:
 * SCSI_MLQUEUE_DEVICE_BUSY if the device queue is full, or
 * SCSI_MLQUEUE_HOST_BUSY if the entire host queue is full
 */
static int
_scsih_qcmd(struct scsi_cmnd *scmd, void (*done)(struct scsi_cmnd *))
{
	struct MPT2SAS_ADAPTER *ioc = shost_private(scmd->device->host);
	struct MPT2SAS_DEVICE *sas_device_priv_data;
	struct MPT2SAS_TARGET *sas_target_priv_data;
	struct _raid_device *raid_device;
	Mpi2SCSIIORequest_t *mpi_request;
	u32 mpi_control;
	u16 smid;
	u16 handle;

#ifdef CONFIG_SCSI_MPT2SAS_LOGGING
	if (ioc->logging_level & MPT_DEBUG_SCSI)
		scsi_print_command(scmd);
#endif

	scmd->scsi_done = done;
	sas_device_priv_data = scmd->device->hostdata;
	if (!sas_device_priv_data || !sas_device_priv_data->sas_target) {
		scmd->result = DID_NO_CONNECT << 16;
		scmd->scsi_done(scmd);
		return 0;
	}

	if (ioc->pci_error_recovery) {
		scmd->result = DID_NO_CONNECT << 16;
		scmd->scsi_done(scmd);
		return 0;
	}

	sas_target_priv_data = sas_device_priv_data->sas_target;

	/* invalid device handle */
	handle = sas_target_priv_data->handle;
	if (handle == MPT2SAS_INVALID_DEVICE_HANDLE) {
		scmd->result = DID_NO_CONNECT << 16;
		scmd->scsi_done(scmd);
		return 0;
	}

#if defined(__VMKLNX__)
	if (sas_target_priv_data->nexus_loss) {
		scmd->result = DID_NO_CONNECT << 16;
		scmd->scsi_done(scmd);
		return 0;
	}
#endif
	 /*
	 * Avoid error handling escallation when blocked
	 */
	if (sas_device_priv_data->block &&
	    scmd->device->host->shost_state == SHOST_RECOVERY &&
	    scmd->cmnd[0] == TEST_UNIT_READY) {
		scmd->result = (DRIVER_SENSE << 24) |
		    SAM_STAT_CHECK_CONDITION;
		scmd->sense_buffer[0] = 0x70;
		scmd->sense_buffer[2] = UNIT_ATTENTION;
		scmd->sense_buffer[12] = 0x29;
		/* ASCQ = I_T NEXUS LOSS OCCURRED */
		scmd->sense_buffer[13] = 0x07;
		scmd->scsi_done(scmd);
		return 0;
	}

	/* host recovery or link resets sent via IOCTLs */
	if (ioc->shost_recovery || ioc->ioc_link_reset_in_progress)
		return SCSI_MLQUEUE_HOST_BUSY;
	/* device has been deleted */
	else if (sas_target_priv_data->deleted ||
	    sas_device_priv_data->deleted) {
		scmd->result = DID_NO_CONNECT << 16;
		scmd->scsi_done(scmd);
		return 0;
	/* device busy with task managment */
	} else if (sas_target_priv_data->tm_busy ||
	    sas_device_priv_data->block)
		return SCSI_MLQUEUE_DEVICE_BUSY;

#if defined(__VMKLNX__)
	if (((scmd->cmnd[0] == RESERVE) || (scmd->cmnd[0] == RELEASE)) &&
	    (sas_device_priv_data->flags & MPT_DEVICE_SATA)) {
		scmd->result = DID_OK << 16;
		scmd->scsi_done(scmd);
		return 0;
	}
#endif
	if (scmd->sc_data_direction == DMA_FROM_DEVICE)
		mpi_control = MPI2_SCSIIO_CONTROL_READ;
	else if (scmd->sc_data_direction == DMA_TO_DEVICE)
		mpi_control = MPI2_SCSIIO_CONTROL_WRITE;
	else
		mpi_control = MPI2_SCSIIO_CONTROL_NODATATRANSFER;

	/* set tags */
	if (!(sas_device_priv_data->flags & MPT_DEVICE_FLAGS_INIT)) {
#if defined(__VMKLNX__)
                char tag[2];
                if (scsi_populate_tag_msg(scmd, tag)) {
                        switch (tag[0]) {
                        case MSG_HEAD_TAG:
				mpi_control |= MPI2_SCSIIO_CONTROL_HEADOFQ;
                                break;
                        case MSG_ORDERED_TAG:
				mpi_control |= MPI2_SCSIIO_CONTROL_ORDEREDQ;
                                break;
                        default:
				mpi_control |= MPI2_SCSIIO_CONTROL_SIMPLEQ;
                                break;
                        }
                }
#else
		if (scmd->device->tagged_supported) {
			if (scmd->device->ordered_tags)
				mpi_control |= MPI2_SCSIIO_CONTROL_ORDEREDQ;
			else
				mpi_control |= MPI2_SCSIIO_CONTROL_SIMPLEQ;
		} else
			mpi_control |= MPI2_SCSIIO_CONTROL_SIMPLEQ;
#endif
	} else
		mpi_control |= MPI2_SCSIIO_CONTROL_SIMPLEQ;

	if ((sas_device_priv_data->flags & MPT_DEVICE_TLR_ON) &&
	    scmd->cmd_len != 32)
		mpi_control |= MPI2_SCSIIO_CONTROL_TLR_ON;

	smid = mpt2sas_base_get_smid_scsiio(ioc, ioc->scsi_io_cb_idx, scmd);
	if (!smid) {
		printk(MPT2SAS_ERR_FMT "%s: failed obtaining a smid\n",
		    ioc->name, __func__);
		goto out;
	}
	mpi_request = mpt2sas_base_get_msg_frame(ioc, smid);
	memset(mpi_request, 0, sizeof(Mpi2SCSIIORequest_t));

	if (!ioc->disable_eedp_support) {
		_scsih_setup_eedp(scmd, mpi_request);
	}
	if (scmd->cmd_len == 32)
		mpi_control |= 4 << MPI2_SCSIIO_CONTROL_ADDCDBLEN_SHIFT;
	mpi_request->Function = MPI2_FUNCTION_SCSI_IO_REQUEST;
	if (sas_device_priv_data->sas_target->flags &
	    MPT_TARGET_FLAGS_RAID_COMPONENT)
		mpi_request->Function = MPI2_FUNCTION_RAID_SCSI_IO_PASSTHROUGH;
	else
		mpi_request->Function = MPI2_FUNCTION_SCSI_IO_REQUEST;
	mpi_request->DevHandle = cpu_to_le16(handle);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23))
	mpi_request->DataLength = cpu_to_le32(scmd->request_bufflen);
#else
	mpi_request->DataLength = cpu_to_le32(scsi_bufflen(scmd));
#endif
	mpi_request->Control = cpu_to_le32(mpi_control);
	mpi_request->IoFlags = cpu_to_le16(scmd->cmd_len);
	mpi_request->MsgFlags = MPI2_SCSIIO_MSGFLAGS_SYSTEM_SENSE_ADDR;
	mpi_request->SenseBufferLength = SCSI_SENSE_BUFFERSIZE;
	mpi_request->SenseBufferLowAddress =
	    mpt2sas_base_get_sense_buffer_dma(ioc, smid);
	mpi_request->SGLOffset0 = offsetof(Mpi2SCSIIORequest_t, SGL) / 4;
	mpi_request->SGLFlags = cpu_to_le16(MPI2_SCSIIO_SGLFLAGS_TYPE_MPI +
	    MPI2_SCSIIO_SGLFLAGS_SYSTEM_ADDR);
	mpi_request->VF_ID = 0;
	mpi_request->VP_ID = 0;
	int_to_scsilun(sas_device_priv_data->lun, (struct scsi_lun *)
	    mpi_request->LUN);
	memcpy(mpi_request->CDB.CDB32, scmd->cmnd, scmd->cmd_len);

	if (!mpi_request->DataLength) {
		mpt2sas_base_build_zero_len_sge(ioc, &mpi_request->SGL);
	} else {
		if (_scsih_build_scatter_gather(ioc, scmd, smid)) {
			mpt2sas_base_free_smid(ioc, smid);
			goto out;
		}
	}

	raid_device = sas_target_priv_data->raid_device;
	if (raid_device && raid_device->direct_io_enabled)
		_scsih_setup_direct_io(ioc, scmd, raid_device, mpi_request,
		    smid);

	if (likely(mpi_request->Function == MPI2_FUNCTION_SCSI_IO_REQUEST))
		mpt2sas_base_put_smid_scsi_io(ioc, smid,
		    le16_to_cpu(mpi_request->DevHandle));
	else
		mpt2sas_base_put_smid_default(ioc, smid);

	return 0;

 out:
	return SCSI_MLQUEUE_HOST_BUSY;
}

/**
 * _scsih_normalize_sense - normalize descriptor and fixed format sense data
 * @sense_buffer: sense data returned by target
 * @data: normalized skey/asc/ascq
 *
 * Return nothing.
 */
static void
_scsih_normalize_sense(char *sense_buffer, struct sense_info *data)
{
	if ((sense_buffer[0] & 0x7F) >= 0x72) {
		/* descriptor format */
		data->skey = sense_buffer[1] & 0x0F;
		data->asc = sense_buffer[2];
		data->ascq = sense_buffer[3];
	} else {
		/* fixed format */
		data->skey = sense_buffer[2] & 0x0F;
		data->asc = sense_buffer[12];
		data->ascq = sense_buffer[13];
	}
}

#ifdef CONFIG_SCSI_MPT2SAS_LOGGING
/**
 * _scsih_scsi_ioc_info - translated non-succesfull SCSI_IO request
 * @ioc: per adapter object
 * @scmd: pointer to scsi command object
 * @mpi_reply: reply mf payload returned from firmware
 *
 * scsi_status - SCSI Status code returned from target device
 * scsi_state - state info associated with SCSI_IO determined by ioc
 * ioc_status - ioc supplied status info
 *
 * Return nothing.
 */
static void
_scsih_scsi_ioc_info(struct MPT2SAS_ADAPTER *ioc, struct scsi_cmnd *scmd,
    Mpi2SCSIIOReply_t *mpi_reply, u16 smid)
{
	u32 response_info;
	u8 *response_bytes;
	u16 ioc_status = le16_to_cpu(mpi_reply->IOCStatus) &
	    MPI2_IOCSTATUS_MASK;
	u8 scsi_state = mpi_reply->SCSIState;
	u8 scsi_status = mpi_reply->SCSIStatus;
	char *desc_ioc_state = NULL;
	char *desc_scsi_status = NULL;
	char *desc_scsi_state = ioc->tmp_string;
	struct _sas_device *sas_device = NULL;
	unsigned long flags;
	struct scsi_target *starget = scmd->device->sdev_target;
	struct MPT2SAS_TARGET *priv_target = starget->hostdata;
	char *device_str = NULL;

	if (!priv_target)
		return;

	if (ioc->hide_ir_msg)
		device_str = "WarpDrive";
	else
		device_str = "volume";

#if !defined(__VMKLNX__)
	u32 log_info = le32_to_cpu(mpi_reply->IOCLogInfo);

	if (log_info == 0x31170000)
		return;
#endif

	switch (ioc_status) {
	case MPI2_IOCSTATUS_SUCCESS:
		desc_ioc_state = "success";
		break;
	case MPI2_IOCSTATUS_INVALID_FUNCTION:
		desc_ioc_state = "invalid function";
		break;
	case MPI2_IOCSTATUS_SCSI_RECOVERED_ERROR:
		desc_ioc_state = "scsi recovered error";
		break;
	case MPI2_IOCSTATUS_SCSI_INVALID_DEVHANDLE:
		desc_ioc_state = "scsi invalid dev handle";
		break;
	case MPI2_IOCSTATUS_SCSI_DEVICE_NOT_THERE:
		desc_ioc_state = "scsi device not there";
		break;
	case MPI2_IOCSTATUS_SCSI_DATA_OVERRUN:
		desc_ioc_state = "scsi data overrun";
		break;
	case MPI2_IOCSTATUS_SCSI_DATA_UNDERRUN:
		desc_ioc_state = "scsi data underrun";
		break;
	case MPI2_IOCSTATUS_SCSI_IO_DATA_ERROR:
		desc_ioc_state = "scsi io data error";
		break;
	case MPI2_IOCSTATUS_SCSI_PROTOCOL_ERROR:
		desc_ioc_state = "scsi protocol error";
		break;
	case MPI2_IOCSTATUS_SCSI_TASK_TERMINATED:
		desc_ioc_state = "scsi task terminated";
		break;
	case MPI2_IOCSTATUS_SCSI_RESIDUAL_MISMATCH:
		desc_ioc_state = "scsi residual mismatch";
		break;
	case MPI2_IOCSTATUS_SCSI_TASK_MGMT_FAILED:
		desc_ioc_state = "scsi task mgmt failed";
		break;
	case MPI2_IOCSTATUS_SCSI_IOC_TERMINATED:
		desc_ioc_state = "scsi ioc terminated";
		break;
	case MPI2_IOCSTATUS_SCSI_EXT_TERMINATED:
		desc_ioc_state = "scsi ext terminated";
		break;
	case MPI2_IOCSTATUS_EEDP_GUARD_ERROR:
		if (!ioc->disable_eedp_support) {
			desc_ioc_state = "eedp guard error";
			break;
		}
	case MPI2_IOCSTATUS_EEDP_REF_TAG_ERROR:
		if (!ioc->disable_eedp_support) {
			desc_ioc_state = "eedp ref tag error";
			break;
		}
	case MPI2_IOCSTATUS_EEDP_APP_TAG_ERROR:
		if (!ioc->disable_eedp_support) {
			desc_ioc_state = "eedp app tag error";
			break;
		}
	default:
		desc_ioc_state = "unknown";
		break;
	}

	switch (scsi_status) {
	case MPI2_SCSI_STATUS_GOOD:
		desc_scsi_status = "good";
		break;
	case MPI2_SCSI_STATUS_CHECK_CONDITION:
		desc_scsi_status = "check condition";
		break;
	case MPI2_SCSI_STATUS_CONDITION_MET:
		desc_scsi_status = "condition met";
		break;
	case MPI2_SCSI_STATUS_BUSY:
		desc_scsi_status = "busy";
		break;
	case MPI2_SCSI_STATUS_INTERMEDIATE:
		desc_scsi_status = "intermediate";
		break;
	case MPI2_SCSI_STATUS_INTERMEDIATE_CONDMET:
		desc_scsi_status = "intermediate condmet";
		break;
	case MPI2_SCSI_STATUS_RESERVATION_CONFLICT:
		desc_scsi_status = "reservation conflict";
		break;
	case MPI2_SCSI_STATUS_COMMAND_TERMINATED:
		desc_scsi_status = "command terminated";
		break;
	case MPI2_SCSI_STATUS_TASK_SET_FULL:
		desc_scsi_status = "task set full";
		break;
	case MPI2_SCSI_STATUS_ACA_ACTIVE:
		desc_scsi_status = "aca active";
		break;
	case MPI2_SCSI_STATUS_TASK_ABORTED:
		desc_scsi_status = "task aborted";
		break;
	default:
		desc_scsi_status = "unknown";
		break;
	}

	desc_scsi_state[0] = '\0';
	if (!scsi_state)
		desc_scsi_state = " ";
	if (scsi_state & MPI2_SCSI_STATE_RESPONSE_INFO_VALID)
		strcat(desc_scsi_state, "response info ");
	if (scsi_state & MPI2_SCSI_STATE_TERMINATED)
		strcat(desc_scsi_state, "state terminated ");
	if (scsi_state & MPI2_SCSI_STATE_NO_SCSI_STATUS)
		strcat(desc_scsi_state, "no status ");
	if (scsi_state & MPI2_SCSI_STATE_AUTOSENSE_FAILED)
		strcat(desc_scsi_state, "autosense failed ");
	if (scsi_state & MPI2_SCSI_STATE_AUTOSENSE_VALID)
		strcat(desc_scsi_state, "autosense valid ");

	scsi_print_command(scmd);

	if (priv_target->flags & MPT_TARGET_FLAGS_VOLUME) {
		printk(MPT2SAS_WARN_FMT "\t%s wwid(0x%016llx)\n", ioc->name,
		    device_str, (unsigned long long)priv_target->sas_address);
	} else {
		spin_lock_irqsave(&ioc->sas_device_lock, flags);
		sas_device = mpt2sas_scsih_sas_device_find_by_sas_address(ioc,
		    priv_target->sas_address);
		if (sas_device) {
			printk(MPT2SAS_WARN_FMT "\tsas_address(0x%016llx), "
			    "phy(%d)\n", ioc->name, (unsigned long long)
			    sas_device->sas_address, sas_device->phy);
			printk(MPT2SAS_WARN_FMT
			    "\tenclosure_logical_id(0x%016llx), slot(%d)\n",
			    ioc->name, (unsigned long long)
			    sas_device->enclosure_logical_id, sas_device->slot);
		}
		spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
	}

	printk(MPT2SAS_WARN_FMT "\thandle(0x%04x), ioc_status(%s)(0x%04x), "
	    "smid(%d)\n", ioc->name, le16_to_cpu(mpi_reply->DevHandle),
	    desc_ioc_state, ioc_status, smid);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23))
	printk(MPT2SAS_WARN_FMT "\trequest_len(%d), underflow(%d), "
	    "resid(%d)\n", ioc->name, scmd->request_bufflen, scmd->underflow,
	    scmd->resid);
#else
	printk(MPT2SAS_WARN_FMT "\trequest_len(%d), underflow(%d), "
	    "resid(%d)\n", ioc->name, scsi_bufflen(scmd), scmd->underflow,
	    scsi_get_resid(scmd));
#endif
	printk(MPT2SAS_WARN_FMT "\ttag(%d), transfer_count(%d), "
	    "sc->result(0x%08x)\n", ioc->name, le16_to_cpu(mpi_reply->TaskTag),
	    le32_to_cpu(mpi_reply->TransferCount), scmd->result);
	printk(MPT2SAS_WARN_FMT "\tscsi_status(%s)(0x%02x), "
	    "scsi_state(%s)(0x%02x)\n", ioc->name, desc_scsi_status,
	    scsi_status, desc_scsi_state, scsi_state);

	if (scsi_state & MPI2_SCSI_STATE_AUTOSENSE_VALID) {
		struct sense_info data;
#if defined(__VMKLNX__)
		_scsih_normalize_sense((char *)scmd->sense_buffer, &data);
#else
		_scsih_normalize_sense(scmd->sense_buffer, &data);
#endif
		printk(MPT2SAS_WARN_FMT "\t[sense_key,asc,ascq]: "
		    "[0x%02x,0x%02x,0x%02x], count(%d)\n", ioc->name, data.skey,
		    data.asc, data.ascq, le32_to_cpu(mpi_reply->SenseCount));
	}

	if (scsi_state & MPI2_SCSI_STATE_RESPONSE_INFO_VALID) {
		response_info = le32_to_cpu(mpi_reply->ResponseInfo);
		response_bytes = (u8 *)&response_info;
		_scsih_response_code(ioc, response_bytes[0]);
	}
}
#endif

#ifdef MPT2SAS_MULTIPATH
/**
 * _scsih_abort_task_set - issue a delayed ABRT_TASK_SET
 * @ioc: per adapter object
 * @fw_event: The fw_event_work object
 *
 * issue TM following target reset using custom event MPT2SAS_ABRT_TASK_SET
 *
 * Return nothing.
 */
static void
_scsih_abort_task_set(struct MPT2SAS_ADAPTER *ioc,
    struct fw_event_work *fw_event)
{
	struct mpt2sas_abort_task_set *tm_data = fw_event->event_data;

	mpt2sas_scsih_issue_tm(ioc, tm_data->handle, 0, 0, tm_data->lun,
	    MPI2_SCSITASKMGMT_TASKTYPE_ABRT_TASK_SET, 0, 5, NULL);
}

/**
 * _scsih_abort_task_set_schedule - schedule a ABRT_TASK_SET
 * @ioc: per adapter object
 * @handle: device handle
 * @lun: lun
 * @delay:
 *
 * schedule a ABRT_TASK_SET following UA bus reset
 *
 * Return nothing.
 */
static void
_scsih_abort_task_set_schedule(struct MPT2SAS_ADAPTER *ioc, u16 handle,
    u32 lun, ulong delay)
{
	struct fw_event_work *fw_event;
	struct mpt2sas_abort_task_set *tm_data;
	unsigned long flags;

	if (ioc->remove_host)
		return;

	fw_event = kzalloc(sizeof(struct fw_event_work), GFP_ATOMIC);
	if (!fw_event) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		return;
	}

	tm_data = kzalloc(sizeof(struct mpt2sas_abort_task_set), GFP_ATOMIC);
	if (!tm_data) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		kfree(fw_event);
		return;
	}

	fw_event->event_data = tm_data;
	tm_data->handle = handle;
	tm_data->lun = lun;
	fw_event->ioc = ioc;
	fw_event->VF_ID = 0;
	fw_event->VP_ID = 0;
	fw_event->event = MPT2SAS_ABRT_TASK_SET;

	spin_lock_irqsave(&ioc->fw_event_lock, flags);
	list_add_tail(&fw_event->list, &ioc->fw_event_list);
#if (ESX_VERS > ESX41_VERS) || (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,19))
	INIT_WORK(&fw_event->work, _firmware_event_work);
	queue_work(ioc->firmware_event_thread, &fw_event->work);
#else
	INIT_WORK(&fw_event->work, _firmware_event_work, (void *)fw_event);
	queue_delayed_work(ioc->firmware_event_thread, &fw_event->work,
	    msecs_to_jiffies(delay));
#endif
	spin_unlock_irqrestore(&ioc->fw_event_lock, flags);
}

/**
 * mpt2sas_scsih_check_tm_for_multipath -
 * @ioc: per adapter object
 * @handle: device handle
 * @task_type:
 *
 * For multipath, a target reset to one path will kill all the IO to the
 * other.  This code issues an abrt_task_set to the other path, so as
 * to prevent timeouts.
 */
void
mpt2sas_scsih_check_tm_for_multipath(struct MPT2SAS_ADAPTER *ioc, u16 handle,
    u8 task_type)
{
	struct MPT2SAS_ADAPTER *ioc_alt;
	struct _sas_device *sas_device, *sas_device_alt;
	unsigned long flags;
	struct scsi_device *sdev;
	int id_alt;
	int channel_alt;
	u16 handle_alt;

	if (mpt2sas_multipath == -1 || mpt2sas_multipath == 0)
		return;

	if (task_type != MPI2_SCSITASKMGMT_TASKTYPE_TARGET_RESET &&
	    task_type != MPI2_SCSITASKMGMT_TASKTYPE_LOGICAL_UNIT_RESET)
		return;

	spin_lock_irqsave(&ioc->sas_device_lock, flags);
	sas_device = _scsih_sas_device_find_by_handle(ioc, handle);
	if (!sas_device || !sas_device->sas_device_alt) {
		spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
		return;
	}
	ioc_alt = sas_device->sas_device_alt->ioc;
	id_alt = sas_device->sas_device_alt->id;
	channel_alt = sas_device->sas_device_alt->channel;
	handle_alt = sas_device->sas_device_alt->handle;
	spin_unlock_irqrestore(&ioc->sas_device_lock, flags);

	/* sending abort task 5 seconds later on alternate path */
	shost_for_each_device(sdev, ioc_alt->shost) {
		if (sdev->id != id_alt || sdev->channel != channel_alt)
			continue;
		_scsih_abort_task_set_schedule(ioc_alt, handle_alt, sdev->lun,
		    5000);
	}
}
#endif

/**
 * _scsih_turn_on_pfa_led - illuminate PFA LED
 * @ioc: per adapter object
 * @handle: device handle
 * Context: process
 *
 * Return nothing.
 */
static void
_scsih_turn_on_pfa_led(struct MPT2SAS_ADAPTER *ioc, u16 handle)
{
	Mpi2SepReply_t mpi_reply;
	Mpi2SepRequest_t mpi_request;
	struct _sas_device *sas_device;

	sas_device = _scsih_sas_device_find_by_handle(ioc, handle);
	if (!sas_device) 
		return;

	memset(&mpi_request, 0, sizeof(Mpi2SepRequest_t));
	mpi_request.Function = MPI2_FUNCTION_SCSI_ENCLOSURE_PROCESSOR;
	mpi_request.Action = MPI2_SEP_REQ_ACTION_WRITE_STATUS;
	mpi_request.SlotStatus =
	    cpu_to_le32(MPI2_SEP_REQ_SLOTSTATUS_PREDICTED_FAULT);
	mpi_request.DevHandle = cpu_to_le16(handle);
	mpi_request.Flags = MPI2_SEP_REQ_FLAGS_DEVHANDLE_ADDRESS;
	if ((mpt2sas_base_scsi_enclosure_processor(ioc, &mpi_reply,
	    &mpi_request)) != 0) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n", ioc->name,
		__FILE__, __LINE__, __func__);
		return;
	}

	sas_device->fault_led_on = 1;
	
	
	if (mpi_reply.IOCStatus || mpi_reply.IOCLogInfo) {
		dewtprintk(ioc, printk(MPT2SAS_INFO_FMT "enclosure_processor: "
		    "ioc_status (0x%04x), loginfo(0x%08x)\n", ioc->name,
		    le16_to_cpu(mpi_reply.IOCStatus),
		    le32_to_cpu(mpi_reply.IOCLogInfo)));
		return;
	}
}

/**
 * _scsih_turn_off_pfa_led - turn off PFA LED
 * @ioc: per adapter object
 * @handle: device handle
 * Context: process
 *
 * Return nothing.
 */
static void
_scsih_turn_off_pfa_led(struct MPT2SAS_ADAPTER *ioc,
    struct _sas_device *sas_device)
{
	Mpi2SepReply_t mpi_reply;
	Mpi2SepRequest_t mpi_request;

	memset(&mpi_request, 0, sizeof(Mpi2SepRequest_t));
	mpi_request.Function = MPI2_FUNCTION_SCSI_ENCLOSURE_PROCESSOR;
	mpi_request.Action = MPI2_SEP_REQ_ACTION_WRITE_STATUS;
	mpi_request.SlotStatus = 0;
	mpi_request.Slot = cpu_to_le16(sas_device->slot);
	mpi_request.DevHandle = 0;
	mpi_request.EnclosureHandle = cpu_to_le16(sas_device->enclosure_handle);
	mpi_request.Flags = MPI2_SEP_REQ_FLAGS_ENCLOSURE_SLOT_ADDRESS;
	if ((mpt2sas_base_scsi_enclosure_processor(ioc, &mpi_reply,
	    &mpi_request)) != 0) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n", ioc->name,
		__FILE__, __LINE__, __func__);
		return;
	}

	if (mpi_reply.IOCStatus || mpi_reply.IOCLogInfo) {
		dewtprintk(ioc, printk(MPT2SAS_INFO_FMT "enclosure_processor: "
		    "ioc_status (0x%04x), loginfo(0x%08x)\n", ioc->name,
		    le16_to_cpu(mpi_reply.IOCStatus),
		    le32_to_cpu(mpi_reply.IOCLogInfo)));
		return;
	}
}

/**
 * _scsih_send_event_to_turn_on_fault_led - fire delayed event
 * @ioc: per adapter object
 * @handle: device handle
 * Context: interrupt.
 *
 * Return nothing.
 */
static void
_scsih_send_event_to_turn_on_fault_led(struct MPT2SAS_ADAPTER *ioc, u16 handle)
{
	struct fw_event_work *fw_event;

	fw_event = kzalloc(sizeof(struct fw_event_work), GFP_ATOMIC);
	if (!fw_event)
		return;
	fw_event->event = MPT2SAS_TURN_ON_FAULT_LED;
	fw_event->device_handle = handle;
	fw_event->ioc = ioc;
	_scsih_fw_event_add(ioc, fw_event);
}

/**
 * _scsih_smart_predicted_fault - process smart errors
 * @ioc: per adapter object
 * @handle: device handle
 * Context: interrupt.
 *
 * Return nothing.
 */
static void
_scsih_smart_predicted_fault(struct MPT2SAS_ADAPTER *ioc, u16 handle)
{
	struct scsi_target *starget;
	struct MPT2SAS_TARGET *sas_target_priv_data;
	Mpi2EventNotificationReply_t *event_reply;
	Mpi2EventDataSasDeviceStatusChange_t *event_data;
	struct _sas_device *sas_device;
	ssize_t sz;
	unsigned long flags;

	/* only handle non-raid devices */
	spin_lock_irqsave(&ioc->sas_device_lock, flags);
	sas_device = _scsih_sas_device_find_by_handle(ioc, handle);
	if (!sas_device) {
		spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
		return;
	}
	starget = sas_device->starget;
	sas_target_priv_data = starget->hostdata;

	if ((sas_target_priv_data->flags & MPT_TARGET_FLAGS_RAID_COMPONENT) ||
	   ((sas_target_priv_data->flags & MPT_TARGET_FLAGS_VOLUME))) {
		spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
		return;
	}
	starget_printk(KERN_WARNING, starget, "predicted fault\n");
	spin_unlock_irqrestore(&ioc->sas_device_lock, flags);

	if (ioc->pdev->subsystem_vendor == PCI_VENDOR_ID_IBM)
		_scsih_send_event_to_turn_on_fault_led(ioc, handle);

	/* insert into event log */
	sz = offsetof(Mpi2EventNotificationReply_t, EventData) +
	     sizeof(Mpi2EventDataSasDeviceStatusChange_t);
	event_reply = kzalloc(sz, GFP_ATOMIC);
	if (!event_reply) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		return;
	}

	event_reply->Function = MPI2_FUNCTION_EVENT_NOTIFICATION;
	event_reply->Event =
	    cpu_to_le16(MPI2_EVENT_SAS_DEVICE_STATUS_CHANGE);
	event_reply->MsgLength = sz/4;
	event_reply->EventDataLength =
	    cpu_to_le16(sizeof(Mpi2EventDataSasDeviceStatusChange_t)/4);
	event_data = (Mpi2EventDataSasDeviceStatusChange_t *)
	    event_reply->EventData;
	event_data->ReasonCode = MPI2_EVENT_SAS_DEV_STAT_RC_SMART_DATA;
	event_data->ASC = 0x5D;
	event_data->DevHandle = cpu_to_le16(handle);
	event_data->SASAddress = cpu_to_le64(sas_target_priv_data->sas_address);
	mpt2sas_ctl_add_to_event_log(ioc, event_reply);
	kfree(event_reply);
}

/**
 * _scsih_io_done - scsi request callback
 * @ioc: per adapter object
 * @smid: system request message index
 * @msix_index: MSIX table index supplied by the OS
 * @reply: reply message frame(lower 32bit addr)
 *
 * Callback handler when using _scsih_qcmd.
 *
 * Return 1 meaning mf should be freed from _base_interrupt
 *        0 means the mf is freed from this function.
 */
static u8
_scsih_io_done(struct MPT2SAS_ADAPTER *ioc, u16 smid, u8 msix_index, u32 reply)
{
	Mpi2SCSIIORequest_t *mpi_request;
	Mpi2SCSIIOReply_t *mpi_reply;
	struct scsi_cmnd *scmd;
	u16 ioc_status;
	u32 xfer_cnt;
	u8 scsi_state;
	u8 scsi_status;
	u32 log_info;
	struct MPT2SAS_DEVICE *sas_device_priv_data;
	u32 response_code = 0;
	unsigned long flags;

	mpi_reply = mpt2sas_base_get_reply_virt_addr(ioc, reply);
	scmd = _scsih_scsi_lookup_get_clear(ioc, smid);
	if (scmd == NULL)
		return 1;

	mpi_request = mpt2sas_base_get_msg_frame(ioc, smid);

	if (mpi_reply == NULL) {
		scmd->result = DID_OK << 16;
		goto out;
	}

	sas_device_priv_data = scmd->device->hostdata;
	if (!sas_device_priv_data || !sas_device_priv_data->sas_target ||
	     sas_device_priv_data->sas_target->deleted) {
		scmd->result = DID_NO_CONNECT << 16;
		goto out;
	}

#if defined(CONFIG_SCSI_MPT2SAS_LOGGING)
	if (ioc->logging_level & MPT_DEBUG_SCSI)
		printk(MPT2SAS_INFO_FMT "Response: smid=%d,"
		    "MSIxIndex=%d\n", ioc->name, smid, 
		    msix_index);
#endif

	ioc_status = le16_to_cpu(mpi_reply->IOCStatus);
	/*
	 * WARPDRIVE: If direct_io is set then it is directIO,
	 * the failed direct I/O should be redirected to volume
	 */
	if (_scsih_scsi_direct_io_get(ioc, smid) &&
	    ((ioc_status & MPI2_IOCSTATUS_MASK)
	    != MPI2_IOCSTATUS_SCSI_TASK_TERMINATED)) {
#ifdef MPT2SAS_WD_DDIOCOUNT
		ioc->ddio_err_count++;
#endif
#ifdef MPT2SAS_WD_LOGGING
		if (ioc->logging_level & MPT_DEBUG_SCSI) {
			printk(MPT2SAS_INFO_FMT "scmd(%p) failed when issued"
			    "as direct IO, retrying\n", ioc->name, scmd);
			scsi_print_command(scmd);
		}
#endif
		spin_lock_irqsave(&ioc->scsi_lookup_lock, flags);
		ioc->scsi_lookup[smid - 1].scmd = scmd;
		_scsih_scsi_direct_io_set(ioc, smid, 0);
		spin_unlock_irqrestore(&ioc->scsi_lookup_lock, flags);
		memcpy(mpi_request->CDB.CDB32, scmd->cmnd, scmd->cmd_len);
		mpi_request->DevHandle =
		    cpu_to_le16(sas_device_priv_data->sas_target->handle);
		mpt2sas_base_put_smid_scsi_io(ioc, smid,
		    sas_device_priv_data->sas_target->handle);
		return 0;
	}

	/* turning off TLR */
	scsi_state = mpi_reply->SCSIState;
	if (scsi_state & MPI2_SCSI_STATE_RESPONSE_INFO_VALID)
		response_code =
		    le32_to_cpu(mpi_reply->ResponseInfo) & 0xFF;
	if (!sas_device_priv_data->tlr_snoop_check) {
		sas_device_priv_data->tlr_snoop_check++;
		if ((sas_device_priv_data->flags & MPT_DEVICE_TLR_ON) &&
		    response_code == MPI2_SCSITASKMGMT_RSP_INVALID_FRAME)
			sas_device_priv_data->flags &=
			    ~MPT_DEVICE_TLR_ON;
	}

	xfer_cnt = le32_to_cpu(mpi_reply->TransferCount);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23))
	scmd->resid = scmd->request_bufflen - xfer_cnt;
#else
	scsi_set_resid(scmd, scsi_bufflen(scmd) - xfer_cnt);
#endif
	if (ioc_status & MPI2_IOCSTATUS_FLAG_LOG_INFO_AVAILABLE)
		log_info =  le32_to_cpu(mpi_reply->IOCLogInfo);
	else
		log_info = 0;
	ioc_status &= MPI2_IOCSTATUS_MASK;
	scsi_status = mpi_reply->SCSIStatus;

	if (ioc_status == MPI2_IOCSTATUS_SCSI_DATA_UNDERRUN && xfer_cnt == 0 &&
	    (scsi_status == MPI2_SCSI_STATUS_BUSY ||
	     scsi_status == MPI2_SCSI_STATUS_RESERVATION_CONFLICT ||
	     scsi_status == MPI2_SCSI_STATUS_TASK_SET_FULL)) {
		ioc_status = MPI2_IOCSTATUS_SUCCESS;
	}

	if (scsi_state & MPI2_SCSI_STATE_AUTOSENSE_VALID) {
		struct sense_info data;
		const void *sense_data = mpt2sas_base_get_sense_buffer(ioc,
		    smid);
		u32 sz = min_t(u32, SCSI_SENSE_BUFFERSIZE,
		    le32_to_cpu(mpi_reply->SenseCount));
#if defined(__VMKLNX__)
#if (ESX_VERS > ESX41_VERS)
		/* Descriptor based sense data is not supported in the ESX5. So,
		 * if a 3TB SAS drive is connected, when the OS tries to determine
		 * whether the drive is SSD or not by issuing EVPD Page 0x89
		 * (ATA IDENTIFY DEVICE), the drive returns descriptor based
		 * sense data. Since the OS can not understand the sense info,
		 * it gives up adding the device. To workaround this problem,
		 * the driver need to convert the descriptor based sense data to
		 * fixed sense data only for EVPD 0x89 requests.
		 */ 
		char *srcsense = (char *) sense_data;
		if (scmd->cmnd[0] == 0x12 && scmd->cmnd[1] == 0x01 &&
		    scmd->cmnd[2] == 0x89 && ((srcsense[0] & 0x7F) >= 0x72)) {
			if (sz > 3)
				mpt_scsi_build_sense_buffer(0,
				    scmd->sense_buffer, (srcsense[1] & 0x0F),
				    srcsense[2], srcsense[3]);
			else
				memcpy(scmd->sense_buffer, sense_data, sz);
		} else
#endif /* ESX_VERS > ESX41_VERS */
			memcpy(scmd->sense_buffer, sense_data, sz);
		_scsih_normalize_sense((char *)scmd->sense_buffer, &data);
#else
		memcpy(scmd->sense_buffer, sense_data, sz);
		_scsih_normalize_sense(scmd->sense_buffer, &data);
#endif /* __VMKLNX__ */
		/* failure prediction threshold exceeded */
		if (data.asc == 0x5D)
			_scsih_smart_predicted_fault(ioc,
			    le16_to_cpu(mpi_reply->DevHandle));
	}

	switch (ioc_status) {
	case MPI2_IOCSTATUS_BUSY:
	case MPI2_IOCSTATUS_INSUFFICIENT_RESOURCES:
		scmd->result = SAM_STAT_BUSY;
		break;

	case MPI2_IOCSTATUS_SCSI_DEVICE_NOT_THERE:
		scmd->result = DID_NO_CONNECT << 16;
		break;

	case MPI2_IOCSTATUS_SCSI_IOC_TERMINATED:
#if defined(__VMKLNX__)
		/* Return NO_CONNECT and set nexus_loss upon detecting NEXUS_LOSS */
		if (log_info == MPT2SAS_LOGINFO_NEXUS_LOSS) {
                        scmd->result = DID_NO_CONNECT << 16;
                        if (sas_device_priv_data->sas_target &&
			    !sas_device_priv_data->sas_target->nexus_loss) {
                                sas_device_priv_data->sas_target->nexus_loss = 1;
                        }
			dtmprintk(ioc, printk(MPT2SAS_INFO_FMT
                            "NL_NO_CONNECT sn=%lu C%d:T%d:L%d cmd_count=%d CMD=%02x\n",
                            ioc->name, scmd->serial_number,
                            scmd->device->channel, scmd->device->id,
                            scmd->device->lun,
                            scmd->device->device_busy, scmd->cmnd[0]));
                        break;
                }
#endif
		if (sas_device_priv_data->block) {
			scmd->result = DID_TRANSPORT_DISRUPTED << 16;
#if defined(__VMKLNX__)
			/* To continue on logging - _scsih_scsi_ioc_info */
			break;
#else
			goto out;
#endif
		}
		if (log_info == 0x31110630) {
			if (scmd->retries > 2) {
				scmd->result = DID_NO_CONNECT << 16;
			} else {
				scmd->result = DID_SOFT_ERROR << 16;
				scmd->device->expecting_cc_ua = 1;
			}
			break;
		}
	case MPI2_IOCSTATUS_SCSI_TASK_TERMINATED:
	case MPI2_IOCSTATUS_SCSI_EXT_TERMINATED:
		scmd->result = DID_RESET << 16;
		break;

	case MPI2_IOCSTATUS_SCSI_RESIDUAL_MISMATCH:
		if ((xfer_cnt == 0) || (scmd->underflow > xfer_cnt))
			scmd->result = DID_SOFT_ERROR << 16;
		else
			scmd->result = (DID_OK << 16) | scsi_status;
		break;

	case MPI2_IOCSTATUS_SCSI_DATA_UNDERRUN:
		scmd->result = (DID_OK << 16) | scsi_status;

		if ((scsi_state & MPI2_SCSI_STATE_AUTOSENSE_VALID))
			break;

		if (xfer_cnt < scmd->underflow) {
			if (scsi_status == SAM_STAT_BUSY)
				scmd->result = SAM_STAT_BUSY;
			else
				scmd->result = DID_SOFT_ERROR << 16;
		} else if (scsi_state & (MPI2_SCSI_STATE_AUTOSENSE_FAILED |
		     MPI2_SCSI_STATE_NO_SCSI_STATUS))
			scmd->result = DID_SOFT_ERROR << 16;
		else if (scsi_state & MPI2_SCSI_STATE_TERMINATED)
			scmd->result = DID_RESET << 16;
		else if (!xfer_cnt && scmd->cmnd[0] == REPORT_LUNS) {
			mpi_reply->SCSIState = MPI2_SCSI_STATE_AUTOSENSE_VALID;
			mpi_reply->SCSIStatus = SAM_STAT_CHECK_CONDITION;
			scmd->result = (DRIVER_SENSE << 24) |
			    SAM_STAT_CHECK_CONDITION;
			scmd->sense_buffer[0] = 0x70;
			scmd->sense_buffer[2] = ILLEGAL_REQUEST;
			scmd->sense_buffer[12] = 0x20;
			scmd->sense_buffer[13] = 0;
		}
		break;

	case MPI2_IOCSTATUS_SCSI_DATA_OVERRUN:
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23))
		scmd->resid = 0;
#else
		scsi_set_resid(scmd, 0);
#endif
	case MPI2_IOCSTATUS_SCSI_RECOVERED_ERROR:
	case MPI2_IOCSTATUS_SUCCESS:
		scmd->result = (DID_OK << 16) | scsi_status;
		if (response_code ==
		    MPI2_SCSITASKMGMT_RSP_INVALID_FRAME ||
		    (scsi_state & (MPI2_SCSI_STATE_AUTOSENSE_FAILED |
		     MPI2_SCSI_STATE_NO_SCSI_STATUS)))
			scmd->result = DID_SOFT_ERROR << 16;
		else if (scsi_state & MPI2_SCSI_STATE_TERMINATED)
			scmd->result = DID_RESET << 16;
		break;

	case MPI2_IOCSTATUS_EEDP_GUARD_ERROR:
	case MPI2_IOCSTATUS_EEDP_REF_TAG_ERROR:
	case MPI2_IOCSTATUS_EEDP_APP_TAG_ERROR:
		if (!ioc->disable_eedp_support) {
			_scsih_eedp_error_handling(scmd, ioc_status);
			break;
		}

	case MPI2_IOCSTATUS_SCSI_PROTOCOL_ERROR:
	case MPI2_IOCSTATUS_INVALID_FUNCTION:
	case MPI2_IOCSTATUS_INVALID_SGL:
	case MPI2_IOCSTATUS_INTERNAL_ERROR:
	case MPI2_IOCSTATUS_INVALID_FIELD:
	case MPI2_IOCSTATUS_INVALID_STATE:
	case MPI2_IOCSTATUS_SCSI_IO_DATA_ERROR:
	case MPI2_IOCSTATUS_SCSI_TASK_MGMT_FAILED:
	default:
		scmd->result = DID_SOFT_ERROR << 16;
		break;

	}

#ifdef CONFIG_SCSI_MPT2SAS_LOGGING
	if (scmd->result && (ioc->logging_level & MPT_DEBUG_REPLY))
		_scsih_scsi_ioc_info(ioc , scmd, mpi_reply, smid);
#endif

 out:

#if defined(CRACK_MONKEY_EEDP)
	if (!ioc->disable_eedp_support) {
		if (scmd->cmnd[0] == INQUIRY && scmd->host_scribble) {
			char *some_data = scmd->host_scribble;
			char inq_str[16];

			memset(inq_str, 0, 16);
			strncpy(inq_str, &some_data[16], 10);
			if (!strcmp(inq_str, "Harpy Disk"))
				some_data[5] |= 1;
			scmd->host_scribble = NULL;
		}
	}
#endif /* CRACK_MONKEY_EEDP */

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23))
	if (scmd->use_sg)
		pci_unmap_sg(ioc->pdev, (struct scatterlist *)
		    scmd->request_buffer, scmd->use_sg,
		    scmd->sc_data_direction);
	else if (scmd->request_bufflen)
		pci_unmap_single(ioc->pdev, scmd->SCp.dma_handle,
		    scmd->request_bufflen, scmd->sc_data_direction);
#else
	scsi_dma_unmap(scmd);
#endif

	scmd->scsi_done(scmd);
	return 1;
}

/**
 * _scsih_sas_host_refresh - refreshing sas host object contents
 * @ioc: per adapter object
 * Context: user
 *
 * During port enable, fw will send topology events for every device. Its
 * possible that the handles may change from the previous setting, so this
 * code keeping handles updating if changed.
 *
 * Return nothing.
 */
static void
_scsih_sas_host_refresh(struct MPT2SAS_ADAPTER *ioc)
{
	u16 sz;
	u16 ioc_status;
	int i;
	Mpi2ConfigReply_t mpi_reply;
	Mpi2SasIOUnitPage0_t *sas_iounit_pg0 = NULL;
	u16 attached_handle;

	dtmprintk(ioc, printk(MPT2SAS_INFO_FMT
	    "updating handles for sas_host(0x%016llx)\n",
	    ioc->name, (unsigned long long)ioc->sas_hba.sas_address));

	sz = offsetof(Mpi2SasIOUnitPage0_t, PhyData) + (ioc->sas_hba.num_phys
	    * sizeof(Mpi2SasIOUnit0PhyData_t));
	sas_iounit_pg0 = kzalloc(sz, GFP_KERNEL);
	if (!sas_iounit_pg0) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		return;
	}

	if ((mpt2sas_config_get_sas_iounit_pg0(ioc, &mpi_reply,
	    sas_iounit_pg0, sz)) != 0)
		goto out;
	ioc_status = le16_to_cpu(mpi_reply.IOCStatus) & MPI2_IOCSTATUS_MASK;
	if (ioc_status != MPI2_IOCSTATUS_SUCCESS)
		goto out;
	for (i = 0; i < ioc->sas_hba.num_phys ; i++) {
		if (i == 0)
			ioc->sas_hba.handle = le16_to_cpu(sas_iounit_pg0->
			    PhyData[0].ControllerDevHandle);
		ioc->sas_hba.phy[i].handle = ioc->sas_hba.handle;
		attached_handle = le16_to_cpu(sas_iounit_pg0->PhyData[i].
		    AttachedDevHandle);
		mpt2sas_transport_update_links(ioc, ioc->sas_hba.sas_address,
		    attached_handle, i, sas_iounit_pg0->PhyData[i].
		    NegotiatedLinkRate >> 4);
	}
 out:
	kfree(sas_iounit_pg0);
}

/**
 * _scsih_sas_host_add - create sas host object
 * @ioc: per adapter object
 *
 * Creating host side data object, stored in ioc->sas_hba
 *
 * Return nothing.
 */
static void
_scsih_sas_host_add(struct MPT2SAS_ADAPTER *ioc)
{
	int i;
	Mpi2ConfigReply_t mpi_reply;
	Mpi2SasIOUnitPage0_t *sas_iounit_pg0 = NULL;
	Mpi2SasIOUnitPage1_t *sas_iounit_pg1 = NULL;
	Mpi2SasPhyPage0_t phy_pg0;
	Mpi2SasDevicePage0_t sas_device_pg0;
	Mpi2SasEnclosurePage0_t enclosure_pg0;
	u16 ioc_status;
	u16 sz;
	u8 device_missing_delay;

	mpt2sas_config_get_number_hba_phys(ioc, &ioc->sas_hba.num_phys);
	if (!ioc->sas_hba.num_phys) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		return;
	}

	/* sas_iounit page 0 */
	sz = offsetof(Mpi2SasIOUnitPage0_t, PhyData) + (ioc->sas_hba.num_phys *
	    sizeof(Mpi2SasIOUnit0PhyData_t));
	sas_iounit_pg0 = kzalloc(sz, GFP_KERNEL);
	if (!sas_iounit_pg0) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		return;
	}
	if ((mpt2sas_config_get_sas_iounit_pg0(ioc, &mpi_reply,
	    sas_iounit_pg0, sz))) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		goto out;
	}
	ioc_status = le16_to_cpu(mpi_reply.IOCStatus) &
	    MPI2_IOCSTATUS_MASK;
	if (ioc_status != MPI2_IOCSTATUS_SUCCESS) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		goto out;
	}

	/* sas_iounit page 1 */
	sz = offsetof(Mpi2SasIOUnitPage1_t, PhyData) + (ioc->sas_hba.num_phys *
	    sizeof(Mpi2SasIOUnit1PhyData_t));
	sas_iounit_pg1 = kzalloc(sz, GFP_KERNEL);
	if (!sas_iounit_pg1) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		goto out;
	}
	if ((mpt2sas_config_get_sas_iounit_pg1(ioc, &mpi_reply,
	    sas_iounit_pg1, sz))) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		goto out;
	}
	ioc_status = le16_to_cpu(mpi_reply.IOCStatus) &
	    MPI2_IOCSTATUS_MASK;
	if (ioc_status != MPI2_IOCSTATUS_SUCCESS) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		goto out;
	}

	ioc->io_missing_delay =
	    sas_iounit_pg1->IODeviceMissingDelay;
	device_missing_delay =
	    sas_iounit_pg1->ReportDeviceMissingDelay;
	if (device_missing_delay & MPI2_SASIOUNIT1_REPORT_MISSING_UNIT_16)
		ioc->device_missing_delay = (device_missing_delay &
		    MPI2_SASIOUNIT1_REPORT_MISSING_TIMEOUT_MASK) * 16;
	else
		ioc->device_missing_delay = device_missing_delay &
		    MPI2_SASIOUNIT1_REPORT_MISSING_TIMEOUT_MASK;
#if defined(__VMKLNX__)
	printk("io_missing_delay=%d device_missing_delay=%d\n", ioc->io_missing_delay, ioc->device_missing_delay);
#endif
	ioc->sas_hba.parent_dev = &ioc->shost->shost_gendev;
	ioc->sas_hba.phy = kcalloc(ioc->sas_hba.num_phys,
	    sizeof(struct _sas_phy), GFP_KERNEL);
	if (!ioc->sas_hba.phy) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		goto out;
	}
	for (i = 0; i < ioc->sas_hba.num_phys ; i++) {
		if ((mpt2sas_config_get_phy_pg0(ioc, &mpi_reply, &phy_pg0,
		    i))) {
			printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
			    ioc->name, __FILE__, __LINE__, __func__);
			goto out;
		}
		ioc_status = le16_to_cpu(mpi_reply.IOCStatus) &
		    MPI2_IOCSTATUS_MASK;
		if (ioc_status != MPI2_IOCSTATUS_SUCCESS) {
			printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
			    ioc->name, __FILE__, __LINE__, __func__);
			goto out;
		}

		if (i == 0)
			ioc->sas_hba.handle = le16_to_cpu(sas_iounit_pg0->
			    PhyData[0].ControllerDevHandle);
		ioc->sas_hba.phy[i].handle = ioc->sas_hba.handle;
		ioc->sas_hba.phy[i].phy_id = i;
		mpt2sas_transport_add_host_phy(ioc, &ioc->sas_hba.phy[i],
		    phy_pg0, ioc->sas_hba.parent_dev);
	}
	if ((mpt2sas_config_get_sas_device_pg0(ioc, &mpi_reply, &sas_device_pg0,
	    MPI2_SAS_DEVICE_PGAD_FORM_HANDLE, ioc->sas_hba.handle))) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		goto out;
	}
	ioc->sas_hba.enclosure_handle =
	    le16_to_cpu(sas_device_pg0.EnclosureHandle);
	ioc->sas_hba.sas_address = le64_to_cpu(sas_device_pg0.SASAddress);
	printk(MPT2SAS_INFO_FMT "host_add: handle(0x%04x), "
	    "sas_addr(0x%016llx), phys(%d)\n", ioc->name, ioc->sas_hba.handle,
	    (unsigned long long) ioc->sas_hba.sas_address,
	    ioc->sas_hba.num_phys) ;

	if (ioc->sas_hba.enclosure_handle) {
		if (!(mpt2sas_config_get_enclosure_pg0(ioc, &mpi_reply,
		    &enclosure_pg0, MPI2_SAS_ENCLOS_PGAD_FORM_HANDLE,
		   ioc->sas_hba.enclosure_handle)))
			ioc->sas_hba.enclosure_logical_id =
			    le64_to_cpu(enclosure_pg0.EnclosureLogicalID);
	}

 out:
	kfree(sas_iounit_pg1);
	kfree(sas_iounit_pg0);
}

/**
 * _scsih_expander_add -  creating expander object
 * @ioc: per adapter object
 * @handle: expander handle
 *
 * Creating expander object, stored in ioc->sas_expander_list.
 *
 * Return 0 for success, else error.
 */
static int
_scsih_expander_add(struct MPT2SAS_ADAPTER *ioc, u16 handle)
{
	struct _sas_node *sas_expander;
	Mpi2ConfigReply_t mpi_reply;
	Mpi2ExpanderPage0_t expander_pg0;
	Mpi2ExpanderPage1_t expander_pg1;
	Mpi2SasEnclosurePage0_t enclosure_pg0;
	u32 ioc_status;
	u16 parent_handle;
	u64 sas_address, sas_address_parent = 0;
	int i;
	unsigned long flags;
	struct _sas_port *mpt2sas_port = NULL;

	int rc = 0;

	if (!handle)
		return -1;

	if (ioc->shost_recovery || ioc->pci_error_recovery)
		return -1;

	if ((mpt2sas_config_get_expander_pg0(ioc, &mpi_reply, &expander_pg0,
	    MPI2_SAS_EXPAND_PGAD_FORM_HNDL, handle))) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		return -1;
	}

	ioc_status = le16_to_cpu(mpi_reply.IOCStatus) &
	    MPI2_IOCSTATUS_MASK;
	if (ioc_status != MPI2_IOCSTATUS_SUCCESS) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		return -1;
	}

	/* handle out of order topology events */
	parent_handle = le16_to_cpu(expander_pg0.ParentDevHandle);
	if (_scsih_get_sas_address(ioc, parent_handle, &sas_address_parent)
	    != 0) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		return -1;
	}
	if (sas_address_parent != ioc->sas_hba.sas_address) {
		spin_lock_irqsave(&ioc->sas_node_lock, flags);
		sas_expander = mpt2sas_scsih_expander_find_by_sas_address(ioc,
		    sas_address_parent);
		spin_unlock_irqrestore(&ioc->sas_node_lock, flags);
		if (!sas_expander) {
			rc = _scsih_expander_add(ioc, parent_handle);
			if (rc != 0)
				return rc;
		}
	}

	spin_lock_irqsave(&ioc->sas_node_lock, flags);
	sas_address = le64_to_cpu(expander_pg0.SASAddress);
	sas_expander = mpt2sas_scsih_expander_find_by_sas_address(ioc,
	    sas_address);
	spin_unlock_irqrestore(&ioc->sas_node_lock, flags);

	if (sas_expander)
		return 0;

	sas_expander = kzalloc(sizeof(struct _sas_node),
	    GFP_KERNEL);
	if (!sas_expander) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		return -1;
	}

	sas_expander->handle = handle;
	sas_expander->num_phys = expander_pg0.NumPhys;
	sas_expander->sas_address_parent = sas_address_parent;
	sas_expander->sas_address = sas_address;

	printk(MPT2SAS_INFO_FMT "expander_add: handle(0x%04x),"
	    " parent(0x%04x), sas_addr(0x%016llx), phys(%d)\n", ioc->name,
	    handle, parent_handle, (unsigned long long)
	    sas_expander->sas_address, sas_expander->num_phys);

	if (!sas_expander->num_phys)
		goto out_fail;
	sas_expander->phy = kcalloc(sas_expander->num_phys,
	    sizeof(struct _sas_phy), GFP_KERNEL);
	if (!sas_expander->phy) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		rc = -1;
		goto out_fail;
	}

	INIT_LIST_HEAD(&sas_expander->sas_port_list);
	mpt2sas_port = mpt2sas_transport_port_add(ioc, handle,
	    sas_address_parent);
	if (!mpt2sas_port) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		rc = -1;
		goto out_fail;
	}
	sas_expander->parent_dev = &mpt2sas_port->rphy->dev;

	for (i = 0 ; i < sas_expander->num_phys ; i++) {
		if ((mpt2sas_config_get_expander_pg1(ioc, &mpi_reply,
		    &expander_pg1, i, handle))) {
			printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
			    ioc->name, __FILE__, __LINE__, __func__);
			rc = -1;
			goto out_fail;
		}
		sas_expander->phy[i].handle = handle;
		sas_expander->phy[i].phy_id = i;

		if ((mpt2sas_transport_add_expander_phy(ioc,
		    &sas_expander->phy[i], expander_pg1,
		    sas_expander->parent_dev))) {
			printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
			    ioc->name, __FILE__, __LINE__, __func__);
			rc = -1;
			goto out_fail;
		}
	}

	if (sas_expander->enclosure_handle) {
		if (!(mpt2sas_config_get_enclosure_pg0(ioc, &mpi_reply,
		    &enclosure_pg0, MPI2_SAS_ENCLOS_PGAD_FORM_HANDLE,
		   sas_expander->enclosure_handle)))
			sas_expander->enclosure_logical_id =
			    le64_to_cpu(enclosure_pg0.EnclosureLogicalID);
	}

	_scsih_expander_node_add(ioc, sas_expander);
	 return 0;

 out_fail:

	if (mpt2sas_port)
		mpt2sas_transport_port_remove(ioc, sas_expander->sas_address,
		    sas_address_parent);
	kfree(sas_expander);
	return rc;
}

/**
 * _scsih_expander_remove - removing expander object
 * @ioc: per adapter object
 * @sas_address: expander sas_address
 *
 * Return nothing.
 */
static void
_scsih_expander_remove(struct MPT2SAS_ADAPTER *ioc, u64 sas_address)
{
	struct _sas_node *sas_expander;
	unsigned long flags;

	if (ioc->shost_recovery)
		return;

	spin_lock_irqsave(&ioc->sas_node_lock, flags);
	sas_expander = mpt2sas_scsih_expander_find_by_sas_address(ioc,
	    sas_address);
	spin_unlock_irqrestore(&ioc->sas_node_lock, flags);
	_scsih_expander_node_remove(ioc, sas_expander);
}

/**
 * _scsih_done -  internal SCSI_IO callback handler.
 * @ioc: per adapter object
 * @smid: system request message index
 * @msix_index: MSIX table index supplied by the OS
 * @reply: reply message frame(lower 32bit addr)
 *
 * Callback handler when sending internal generated SCSI_IO.
 * The callback index passed is `ioc->scsih_cb_idx`
 *
 * Return 1 meaning mf should be freed from _base_interrupt
 *        0 means the mf is freed from this function.
 */
static u8
_scsih_done(struct MPT2SAS_ADAPTER *ioc, u16 smid, u8 msix_index, u32 reply)
{
	MPI2DefaultReply_t *mpi_reply;

	mpi_reply =  mpt2sas_base_get_reply_virt_addr(ioc, reply);
	if (ioc->scsih_cmds.status == MPT2_CMD_NOT_USED)
		return 1;
	if (ioc->scsih_cmds.smid != smid)
		return 1;
	ioc->scsih_cmds.status |= MPT2_CMD_COMPLETE;
	if (mpi_reply) {
		memcpy(ioc->scsih_cmds.reply, mpi_reply,
		    mpi_reply->MsgLength*4);
		ioc->scsih_cmds.status |= MPT2_CMD_REPLY_VALID;
	}
	ioc->scsih_cmds.status &= ~MPT2_CMD_PENDING;
	complete(&ioc->scsih_cmds.done);
	return 1;
}

/**
 * _scsih_q_internal_done -  internal queue SCSI_IO callback handler.
 * @ioc: per adapter object
 * @smid: system request message index
 * @msix_index: MSIX table index supplied by the OS
 * @reply: reply message frame(lower 32bit addr)
 *
 * Callback handler when sending internal generated SCSI ququed IO.
 * The callback index passed is `ioc->scsih_q_cb_idx`
 *
 * Return 1 meaning mf should be freed from _base_interrupt
 *        0 means the mf is freed from this function.
 */
static u8
_scsih_q_internal_done(struct MPT2SAS_ADAPTER *ioc, u16 smid, u8 msix_index, u32 reply)
{
	Mpi2SCSIIORequest_t *mpi_request;
	Mpi2SCSIIOReply_t *mpi_reply;
	struct _internal_qcmd *scsih_internal_qcmd;
	struct _internal_qcmd *r;
	struct scsi_cmnd *scmd;

	mpi_reply =  mpt2sas_base_get_reply_virt_addr(ioc, reply);
		
	scmd = _scsih_scsi_lookup_get_clear(ioc, smid);

	mpi_request = mpt2sas_base_get_msg_frame(ioc, smid);
	
	r = NULL;
	list_for_each_entry(scsih_internal_qcmd, &ioc->scsih_q_intenal_cmds, list) {
		if (scsih_internal_qcmd->request != mpi_request)
			continue;
		r = scsih_internal_qcmd;
		r->request = NULL;
		break;
	}

	if (r == NULL)
	return 1;

	if (r->status == MPT2_CMD_NOT_USED)
		return 1;
	
	r->status |= MPT2_CMD_COMPLETE;

	if (mpi_reply) {
		memcpy(r->reply, mpi_reply, mpi_reply->MsgLength*4);
		r->status |= MPT2_CMD_REPLY_VALID;
	}

	
	r->status &= ~MPT2_CMD_PENDING;
	return 1;

}

/**
 * _scsi_send_scsi_io - send internal SCSI_IO to target
 * @ioc: per adapter object
 * @transfer_packet: packet describing the transfer
 * Context: user
 *
 * Returns 0 for success, non-zero for failure.
 */
static int
_scsi_send_scsi_io(struct MPT2SAS_ADAPTER *ioc, struct _scsi_io_transfer
    *transfer_packet)
{
	Mpi2SCSIIOReply_t *mpi_reply;
	Mpi2SCSIIORequest_t *mpi_request;
	u16 smid;
	u32 ioc_state;
	unsigned long timeleft;
	u8 issue_reset = 0;
	int rc;
	void *priv_sense;
	u32 mpi_control;
	u32 sgl_flags;
	u16 wait_state_count;
	u16 handle;

	if (ioc->shost_recovery || ioc->pci_error_recovery) {
		printk(MPT2SAS_INFO_FMT "%s: host reset in progress!\n",
		    __func__, ioc->name);
		return -EFAULT;
	}

	handle = transfer_packet->handle;
	if (handle == MPT2SAS_INVALID_DEVICE_HANDLE) {
		printk(MPT2SAS_INFO_FMT "%s: no device!\n",
		    __func__, ioc->name);
		return -EFAULT;
	}

	mutex_lock(&ioc->scsih_cmds.mutex);

	if (ioc->scsih_cmds.status != MPT2_CMD_NOT_USED) {
		printk(MPT2SAS_ERR_FMT "%s: scsih_cmd in use\n",
		    ioc->name, __func__);
		rc = -EAGAIN;
		goto out;
	}
	ioc->scsih_cmds.status = MPT2_CMD_PENDING;

	wait_state_count = 0;
	ioc_state = mpt2sas_base_get_iocstate(ioc, 1);
	while (ioc_state != MPI2_IOC_STATE_OPERATIONAL) {
		if (wait_state_count++ == 10) {
			printk(MPT2SAS_ERR_FMT
			    "%s: failed due to ioc not operational\n",
			    ioc->name, __func__);
			rc = -EFAULT;
			goto out;
		}
		ssleep(1);
		ioc_state = mpt2sas_base_get_iocstate(ioc, 1);
		printk(MPT2SAS_INFO_FMT "%s: waiting for "
		    "operational state(count=%d)\n", ioc->name,
		    __func__, wait_state_count);
	}
	if (wait_state_count)
		printk(MPT2SAS_INFO_FMT "%s: ioc is operational\n",
		    ioc->name, __func__);

	smid = mpt2sas_base_get_smid_scsiio(ioc, ioc->scsih_cb_idx, NULL);
	if (!smid) {
		printk(MPT2SAS_ERR_FMT "%s: failed obtaining a smid\n",
		    ioc->name, __func__);
		rc = -EAGAIN;
		goto out;
	}

	rc = 0;
	mpi_request = mpt2sas_base_get_msg_frame(ioc, smid);
	ioc->scsih_cmds.smid = smid;
	memset(mpi_request, 0, sizeof(Mpi2SCSIIORequest_t));
	if (transfer_packet->is_raid)
		mpi_request->Function = MPI2_FUNCTION_RAID_SCSI_IO_PASSTHROUGH;
	else
		mpi_request->Function = MPI2_FUNCTION_SCSI_IO_REQUEST;
	mpi_request->DevHandle = cpu_to_le16(handle);

	/* set scatter gather flags */
	sgl_flags = (MPI2_SGE_FLAGS_SIMPLE_ELEMENT |
	    MPI2_SGE_FLAGS_LAST_ELEMENT | MPI2_SGE_FLAGS_END_OF_BUFFER |
	    MPI2_SGE_FLAGS_END_OF_LIST);
	if (transfer_packet->dir == DMA_TO_DEVICE)
		sgl_flags |= MPI2_SGE_FLAGS_HOST_TO_IOC;
	sgl_flags = sgl_flags << MPI2_SGE_FLAGS_SHIFT;

	switch (transfer_packet->dir) {
	case DMA_TO_DEVICE:
		ioc->base_add_sg_single(&mpi_request->SGL, sgl_flags |
		    transfer_packet->data_length, transfer_packet->data_dma);
		mpi_control = MPI2_SCSIIO_CONTROL_WRITE;
		break;
	case DMA_FROM_DEVICE:
		ioc->base_add_sg_single(&mpi_request->SGL, sgl_flags |
		    transfer_packet->data_length, transfer_packet->data_dma);
		mpi_control = MPI2_SCSIIO_CONTROL_READ;
		break;
	case DMA_BIDIRECTIONAL:
		mpi_control = MPI2_SCSIIO_CONTROL_BIDIRECTIONAL;
		BUG();
		break;
	default:
	case DMA_NONE:
		mpi_control = MPI2_SCSIIO_CONTROL_NODATATRANSFER;
		mpt2sas_base_build_zero_len_sge(ioc, &mpi_request->SGL);
		break;
	}
	mpi_request->Control = cpu_to_le32(mpi_control |
	    MPI2_SCSIIO_CONTROL_SIMPLEQ);
	mpi_request->DataLength = cpu_to_le32(transfer_packet->data_length);
	mpi_request->MsgFlags = MPI2_SCSIIO_MSGFLAGS_SYSTEM_SENSE_ADDR;
	mpi_request->SenseBufferLength = SCSI_SENSE_BUFFERSIZE;
	mpi_request->SenseBufferLowAddress =
	    mpt2sas_base_get_sense_buffer_dma(ioc, smid);
	priv_sense = mpt2sas_base_get_sense_buffer(ioc, smid);
	mpi_request->SGLOffset0 = offsetof(Mpi2SCSIIORequest_t, SGL) / 4;
	mpi_request->SGLFlags = cpu_to_le16(MPI2_SCSIIO_SGLFLAGS_TYPE_MPI +
	    MPI2_SCSIIO_SGLFLAGS_SYSTEM_ADDR);
	mpi_request->IoFlags = cpu_to_le16(transfer_packet->cdb_length);
	int_to_scsilun(transfer_packet->lun, (struct scsi_lun *)
	    mpi_request->LUN);
	memcpy(mpi_request->CDB.CDB32, transfer_packet->cdb,
	    transfer_packet->cdb_length);
	mpi_request->VF_ID = transfer_packet->VF_ID;
	mpi_request->VP_ID = transfer_packet->VP_ID;
	init_completion(&ioc->scsih_cmds.done);
	if (likely(mpi_request->Function == MPI2_FUNCTION_SCSI_IO_REQUEST))
		mpt2sas_base_put_smid_scsi_io(ioc, smid, handle);
	else
		mpt2sas_base_put_smid_default(ioc, smid);

	timeleft = wait_for_completion_timeout(&ioc->scsih_cmds.done,
	    transfer_packet->timeout*HZ);
	if (!(ioc->scsih_cmds.status & MPT2_CMD_COMPLETE)) {
		printk(MPT2SAS_ERR_FMT "%s: timeout\n",
		    ioc->name, __func__);

		_debug_dump_mf(mpi_request, sizeof(Mpi2SCSIIORequest_t)/4);

		if (!(ioc->scsih_cmds.status & MPT2_CMD_RESET))
			issue_reset = 1;
		goto issue_target_reset;
	}
	if (ioc->scsih_cmds.status & MPT2_CMD_REPLY_VALID) {
		transfer_packet->valid_reply = 1;
		mpi_reply = ioc->scsih_cmds.reply;
		transfer_packet->sense_length =
		   le32_to_cpu(mpi_reply->SenseCount);
		if (transfer_packet->sense_length)
			memcpy(transfer_packet->sense, priv_sense,
			    transfer_packet->sense_length);
		transfer_packet->transfer_length =
		    le32_to_cpu(mpi_reply->TransferCount);
		transfer_packet->ioc_status =
		    le16_to_cpu(mpi_reply->IOCStatus) &
		    MPI2_IOCSTATUS_MASK;
		transfer_packet->scsi_state = mpi_reply->SCSIState;
		transfer_packet->scsi_status = mpi_reply->SCSIStatus;
		transfer_packet->log_info =
		    le32_to_cpu(mpi_reply->IOCLogInfo);
	}
	goto out;

 issue_target_reset:
	if (issue_reset) {
		printk(MPT2SAS_INFO_FMT "issue target reset: handle"
		    "(0x%04x)\n", ioc->name, handle);
		mpt2sas_scsih_issue_tm(ioc, handle, 0, 0, 0,
		    MPI2_SCSITASKMGMT_TASKTYPE_TARGET_RESET, 0, 30, NULL);

		if (ioc->scsih_cmds.status & MPT2_CMD_COMPLETE) {
			printk(MPT2SAS_INFO_FMT "target reset completed: handle"
			    "(0x%04x)\n", ioc->name, handle);
			rc = -EAGAIN;
		} else {
			printk(MPT2SAS_INFO_FMT "target reset didn't complete:"
			    " handle(0x%04x)\n", ioc->name, handle);
			rc = -EFAULT;
		}
	} else
		rc = -EAGAIN;

 out:
	ioc->scsih_cmds.status = MPT2_CMD_NOT_USED;
	mutex_unlock(&ioc->scsih_cmds.mutex);
	return rc;
}


/**
 * _scsi_send_internal_queue_scsi_io - send internal SCSI_IO - SSU  to target
 * @ioc: per adapter object
 * @transfer_packet: packet describing the transfer
 * Context: user
 *
 * Returns 0 for success, non-zero for failure.
 */
static int
_scsi_send_internal_queue_scsi_io(struct MPT2SAS_ADAPTER *ioc, struct _scsi_io_transfer
	*transfer_packet)
{
	Mpi2SCSIIORequest_t *mpi_request;
	u16 smid;
	u32 ioc_state;
	int rc;
	void *priv_sense;
	u32 mpi_control;
	u32 sgl_flags;
	u16 wait_state_count;
	u16 handle;
	struct _internal_qcmd *scsih_qcmd;
	unsigned long flags;


	if (ioc->shost_recovery || ioc->pci_error_recovery) {
		printk(MPT2SAS_INFO_FMT "%s: host reset in progress!\n",
		    __func__, ioc->name);
		return -EFAULT;
	}

	handle = transfer_packet->handle;
	if (handle == MPT2SAS_INVALID_DEVICE_HANDLE) {
		printk(MPT2SAS_INFO_FMT "%s: no device!\n",
		    __func__, ioc->name);
		return -EFAULT;
	}

	wait_state_count = 0;

	ioc_state = mpt2sas_base_get_iocstate(ioc, 1);
	while (ioc_state != MPI2_IOC_STATE_OPERATIONAL) {
		if (wait_state_count++ == 10) {
			printk(MPT2SAS_ERR_FMT
			    "%s: failed due to ioc not operational\n",
			    ioc->name, __func__);
			rc = -EFAULT;
			goto out;
		}
		ssleep(1);
		ioc_state = mpt2sas_base_get_iocstate(ioc, 1);
		printk(MPT2SAS_INFO_FMT "%s: waiting for "
		    "operational state(count=%d)\n", ioc->name,
		    __func__, wait_state_count);
	}


	if (wait_state_count)
		printk(MPT2SAS_INFO_FMT "%s: ioc is operational\n",
		    ioc->name, __func__);

	scsih_qcmd =kzalloc(sizeof(struct _internal_qcmd), GFP_KERNEL);
	if (!scsih_qcmd) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		rc = DEVICE_RETRY;
		goto out;
	}

	scsih_qcmd->reply = kzalloc(ioc->reply_sz, GFP_KERNEL);
	if (!scsih_qcmd->reply ) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		kfree(scsih_qcmd);
		rc = DEVICE_RETRY;
		goto out;
	}


	smid = mpt2sas_base_get_smid_scsiio(ioc, ioc->scsih_q_cb_idx, NULL);
	if (!smid) {
		printk(MPT2SAS_ERR_FMT "%s: failed obtaining a smid\n",
		    ioc->name, __func__);
		rc = DEVICE_RETRY;
		kfree(scsih_qcmd->reply);
		kfree(scsih_qcmd);
		goto out;
	}

	spin_lock_irqsave(&ioc->scsih_q_internal_lock, flags);
	list_add_tail(&scsih_qcmd->list, &ioc->scsih_q_intenal_cmds);
	spin_unlock_irqrestore(&ioc->scsih_q_internal_lock, flags);

	rc = DEVICE_READY;
	mpi_request = mpt2sas_base_get_msg_frame(ioc, smid);
	scsih_qcmd->status = MPT2_CMD_PENDING;
	
	scsih_qcmd->request = mpi_request;
	scsih_qcmd->smid = smid;
	scsih_qcmd->transfer_packet = transfer_packet;
	memset(mpi_request, 0, sizeof(Mpi2SCSIIORequest_t));
	if (transfer_packet->is_raid)
		mpi_request->Function = MPI2_FUNCTION_RAID_SCSI_IO_PASSTHROUGH;
	else
		mpi_request->Function = MPI2_FUNCTION_SCSI_IO_REQUEST;
	mpi_request->DevHandle = cpu_to_le16(handle);

	/* set scatter gather flags */
	sgl_flags = (MPI2_SGE_FLAGS_SIMPLE_ELEMENT |
	    MPI2_SGE_FLAGS_LAST_ELEMENT | MPI2_SGE_FLAGS_END_OF_BUFFER |
	    MPI2_SGE_FLAGS_END_OF_LIST);
	if (transfer_packet->dir == DMA_TO_DEVICE)
		sgl_flags |= MPI2_SGE_FLAGS_HOST_TO_IOC;
	sgl_flags = sgl_flags << MPI2_SGE_FLAGS_SHIFT;

	switch (transfer_packet->dir) {
	case DMA_TO_DEVICE:
		ioc->base_add_sg_single(&mpi_request->SGL, sgl_flags |
		    transfer_packet->data_length, transfer_packet->data_dma);
		mpi_control = MPI2_SCSIIO_CONTROL_WRITE;
		break;
	case DMA_FROM_DEVICE:
		ioc->base_add_sg_single(&mpi_request->SGL, sgl_flags |
		    transfer_packet->data_length, transfer_packet->data_dma);
		mpi_control = MPI2_SCSIIO_CONTROL_READ;
		break;
	case DMA_BIDIRECTIONAL:
		mpi_control = MPI2_SCSIIO_CONTROL_BIDIRECTIONAL;
		/* TODO - is BIDI support needed ?? */
		BUG();
		break;
	default:
	case DMA_NONE:
		mpi_control = MPI2_SCSIIO_CONTROL_NODATATRANSFER;
		mpt2sas_base_build_zero_len_sge(ioc, &mpi_request->SGL);
		break;
	}
	mpi_request->Control = cpu_to_le32(mpi_control |
	    MPI2_SCSIIO_CONTROL_SIMPLEQ);
	mpi_request->DataLength = cpu_to_le32(transfer_packet->data_length);
	mpi_request->MsgFlags = MPI2_SCSIIO_MSGFLAGS_SYSTEM_SENSE_ADDR;
	mpi_request->SenseBufferLength = SCSI_SENSE_BUFFERSIZE;
	mpi_request->SenseBufferLowAddress =
	    mpt2sas_base_get_sense_buffer_dma(ioc, smid);
	priv_sense = mpt2sas_base_get_sense_buffer(ioc, smid);
	mpi_request->SGLOffset0 = offsetof(Mpi2SCSIIORequest_t, SGL) / 4;
	mpi_request->SGLFlags = cpu_to_le16(MPI2_SCSIIO_SGLFLAGS_TYPE_MPI +
	    MPI2_SCSIIO_SGLFLAGS_SYSTEM_ADDR);
	mpi_request->IoFlags = cpu_to_le16(transfer_packet->cdb_length);
	int_to_scsilun(transfer_packet->lun, (struct scsi_lun *)
	    mpi_request->LUN);
	memcpy(mpi_request->CDB.CDB32, transfer_packet->cdb,
	    transfer_packet->cdb_length);
	mpi_request->VF_ID = transfer_packet->VF_ID; 
	mpi_request->VP_ID = transfer_packet->VP_ID;

	if (likely(mpi_request->Function == MPI2_FUNCTION_SCSI_IO_REQUEST))
		mpt2sas_base_put_smid_scsi_io(ioc, smid, handle);
	else
		mpt2sas_base_put_smid_default(ioc, smid);

 out:
	return rc;
}

/**
 * _scsih_determine_disposition -
 * @ioc: per adapter object
 * @transfer_packet: packet describing the transfer
 * Context: user
 *
 * Determines if an internal generated scsi_io is good data, or
 * whether it needs to be retried or treated as an error.
 *
 * Returns device_responsive_state
 */
static enum device_responsive_state
_scsih_determine_disposition(struct MPT2SAS_ADAPTER *ioc,
    struct _scsi_io_transfer *transfer_packet)
{
	static enum device_responsive_state rc;
	struct sense_info sense_info = {0, 0, 0};
	u8 check_sense = 0;
#ifdef CONFIG_SCSI_MPT2SAS_LOGGING
	char *desc = NULL;
#endif

	if (!transfer_packet->valid_reply)
		return DEVICE_READY;

	switch (transfer_packet->ioc_status) {
	case MPI2_IOCSTATUS_BUSY:
	case MPI2_IOCSTATUS_INSUFFICIENT_RESOURCES:
	case MPI2_IOCSTATUS_SCSI_TASK_TERMINATED:
	case MPI2_IOCSTATUS_SCSI_IO_DATA_ERROR:
	case MPI2_IOCSTATUS_SCSI_EXT_TERMINATED:
		rc = DEVICE_RETRY;
		break;
	case MPI2_IOCSTATUS_SCSI_IOC_TERMINATED:
		if (transfer_packet->log_info ==  0x31170000) {
			rc = DEVICE_RETRY;
			break;
		}
		if (transfer_packet->cdb[0] == REPORT_LUNS)
			rc = DEVICE_READY;
		else
			rc = DEVICE_RETRY;
		break;
	case MPI2_IOCSTATUS_SCSI_DATA_UNDERRUN:
	case MPI2_IOCSTATUS_SCSI_RECOVERED_ERROR:
	case MPI2_IOCSTATUS_SUCCESS:
		if (!transfer_packet->scsi_state &&
		    !transfer_packet->scsi_status) {
			rc = DEVICE_READY;
			break;
		}
		if (transfer_packet->scsi_state &
		    MPI2_SCSI_STATE_AUTOSENSE_VALID) {
			rc = DEVICE_ERROR;
			check_sense = 1;
			break;
		}
		if (transfer_packet->scsi_state &
		    (MPI2_SCSI_STATE_AUTOSENSE_FAILED |
		    MPI2_SCSI_STATE_NO_SCSI_STATUS |
		    MPI2_SCSI_STATE_TERMINATED)) {
			rc = DEVICE_RETRY;
			break;
		}
		if (transfer_packet->scsi_status >=
		    MPI2_SCSI_STATUS_BUSY) {
			rc = DEVICE_RETRY;
			break;
		}
		rc = DEVICE_READY;
		break;
	case MPI2_IOCSTATUS_SCSI_PROTOCOL_ERROR:
		if (transfer_packet->scsi_state &
		    MPI2_SCSI_STATE_TERMINATED)
			rc = DEVICE_RETRY;
		else
			rc = DEVICE_ERROR;
		break;
	default:
		rc = DEVICE_ERROR;
		break;
	}

	if (check_sense) {
#if defined(__VMKLNX__)
		_scsih_normalize_sense((char *)transfer_packet->sense, &sense_info);
#else
		_scsih_normalize_sense(transfer_packet->sense, &sense_info);
#endif
		if (sense_info.skey == UNIT_ATTENTION)
			rc = DEVICE_RETRY_UA;
		else if (sense_info.skey == NOT_READY) {
			/* medium isn't present */
			if (sense_info.asc == 0x3a)
				rc = DEVICE_READY;
			/* LOGICAL UNIT NOT READY */
			else if (sense_info.asc == 0x04) {
				if (sense_info.ascq == 0x03 ||
				   sense_info.ascq == 0x0b ||
				   sense_info.ascq == 0x0c) {
					rc = DEVICE_ERROR;
				} else
					rc = DEVICE_START_UNIT;
			}
			/* LOGICAL UNIT HAS NOT SELF-CONFIGURED YET */
			else if (sense_info.asc == 0x3e && !sense_info.ascq)
				rc = DEVICE_START_UNIT;
		} else if (sense_info.skey == ILLEGAL_REQUEST &&
		    transfer_packet->cdb[0] == REPORT_LUNS) {
			rc = DEVICE_READY;
		} else if (sense_info.skey == MEDIUM_ERROR) {

			/* medium is corrupt, lets add the device so
			 * users can collect some info as needed
			 */

			if (sense_info.asc == 0x31)
				rc = DEVICE_READY;
		} else if (sense_info.skey == HARDWARE_ERROR) {
			/* Defect List Error, still add the device */
			if (sense_info.asc == 0x19)
				rc = DEVICE_READY;
		}
	}

#ifdef CONFIG_SCSI_MPT2SAS_LOGGING
	if (ioc->logging_level & MPT_DEBUG_EVENT_WORK_TASK) {
		switch (rc) {
		case DEVICE_READY:
			desc = "ready";
			break;
		case DEVICE_RETRY:
			desc = "retry";
			break;
		case DEVICE_RETRY_UA:
			desc = "retry_ua";
			break;
		case DEVICE_START_UNIT:
			desc = "start_unit";
			break;
		case DEVICE_STOP_UNIT:
			desc = "stop_unit";
			break;
		case DEVICE_ERROR:
			desc = "error";
			break;
		}

		printk(MPT2SAS_INFO_FMT "\tioc_status(0x%04x), "
		    "loginfo(0x%08x), scsi_status(0x%02x), "
		    "scsi_state(0x%02x), rc(%s)\n",
		    ioc->name, transfer_packet->ioc_status,
		    transfer_packet->log_info, transfer_packet->scsi_status,
		    transfer_packet->scsi_state, desc);

		if (check_sense)
			printk(MPT2SAS_INFO_FMT "\t[sense_key,asc,ascq]: "
			    "[0x%02x,0x%02x,0x%02x]\n", ioc->name,
			    sense_info.skey, sense_info.asc, sense_info.ascq);
	}
#endif
	return rc;
}

/**
 * _scsi_send_stop_unit_cmd_completion_status - check the SSU cmds completeion status
 * @ioc: per adapter object
 * Context: user
 *
 * Returns: None
 */
static void
_scsi_send_stop_unit_cmd_completion_status(struct MPT2SAS_ADAPTER *ioc)
{
	struct _internal_qcmd *scsih_int_qcmd, *scsih_int_qcmd_next;
	unsigned long flags;
	u8 issue_reset = 0;

	spin_lock_irqsave(&ioc->scsih_q_internal_lock, flags);
	list_for_each_entry_safe(scsih_int_qcmd, scsih_int_qcmd_next, 
				&ioc->scsih_q_intenal_cmds, list) {
		if (*scsih_int_qcmd->transfer_packet->cdb != START_STOP)
			continue;

		if (!(scsih_int_qcmd->status & MPT2_CMD_COMPLETE) ||
				(scsih_int_qcmd->status & MPT2_CMD_RESET)) {


			printk(MPT2SAS_ERR_FMT "%s: SSU cmd not complete\n",
			    ioc->name, __func__);

			printk(MPT2SAS_INFO_FMT "issue target reset: handle"
			    "(0x%04x)\n", ioc->name, scsih_int_qcmd->transfer_packet->handle);
			issue_reset = 1;
			_scsih_tm_internal_tr_send(ioc, scsih_int_qcmd->transfer_packet->handle);
		}
		list_del(&scsih_int_qcmd->list);
		kfree(scsih_int_qcmd->reply);
		kfree(scsih_int_qcmd->transfer_packet);
		kfree(scsih_int_qcmd);
	}
	spin_unlock_irqrestore(&ioc->scsih_q_internal_lock, flags);
	if (issue_reset ) {
		/* wait 10 sec  */
		mdelay(10000);
	}
}

#if defined(__VMKLNX__)
int
mpt2sas_scsih_get_sas_address_for_sata_disk(struct MPT2SAS_ADAPTER *ioc,
	U64 *pSASAddress, u16 handle)
{
#define SERIAL_NUMBER_LENGTH 20
#define MODEL_NUMBER_LENGTH  40
#define WORKING_BUFFER_LENGTH (SERIAL_NUMBER_LENGTH + MODEL_NUMBER_LENGTH)
	Mpi2SataPassthroughReply_t mpi_reply;
        int             i, rc, try_count;
        u32             *pU32;
        SAS_ADDRESS     Hash;
	ATA_IDENTIFY_DEVICE_DATA ata_identify;
        u8              buffer[WORKING_BUFFER_LENGTH]; /* 40 + 20 = 60 */

        /*
         * buffer[0]  = ModelNumber[0];
         *     ...
         * buffer[39] = ModelNumber[39];
         * buffer[40] = SerialNumber[0];
         *    ...
         * buffer[59] = SerialNumber[19];
         */
        memset(&ata_identify, 0, sizeof(ata_identify));
	try_count = 0;
	do {
		/* retry in case of slow devices */
        	rc = mpt2sas_base_get_sata_identify(ioc, handle, &mpi_reply,
			(char *)&ata_identify, sizeof(ata_identify));
		try_count++;
	} while ((rc || mpi_reply.IOCStatus || mpi_reply.SASStatus) &&
	    (try_count < 5));

        if (rc == 0 && !mpi_reply.IOCStatus && !mpi_reply.SASStatus) {
		printk(MPT2SAS_INFO_FMT "Get SATA identify successfully for "
			"handle=0x%x with try_count=%d\n",
			ioc->name, handle, try_count);
	} else {
		dfailprintk(ioc, printk(MPT2SAS_INFO_FMT
		    "mpt2sas_scsih_get_sas_address_for_sata_disk (handle=0x%x)"
		    " failed\n", ioc->name, handle));
                return -1;
        }
	/* Copy & byteswap the 40 byte model number to a buffer */
	for(i = 0; i < MODEL_NUMBER_LENGTH; i += 2) {
		buffer[i] = ((u8 *)ata_identify.ModelNumber)[i + 1];
		buffer[i + 1] = ((u8 *)ata_identify.ModelNumber)[i];
	}
	/* Copy & byteswap the 20 byte serial number to a buffer */
	for(i = 0; i < SERIAL_NUMBER_LENGTH; i += 2) {
		buffer[MODEL_NUMBER_LENGTH + i] =
			((u8 *)ata_identify.SerialNumber)[i + 1];
		buffer[MODEL_NUMBER_LENGTH + i + 1] =
			((u8 *)ata_identify.SerialNumber)[i];
	}
        pU32 = (U32 *)buffer;
        /* There are 60 bytes to hash down to 8. 60 isn't divisible by 8,
         * so loop through the first 56 bytes (7*8),
         * and then add in the last dword.
         */
        Hash.Word.Low  = 0;
        Hash.Word.High = 0;
        for (i = 0; (i < (WORKING_BUFFER_LENGTH/8)); i++)
        {
                Hash.Word.Low += *pU32;
                pU32++;
                Hash.Word.High+= *pU32;
                pU32++;
        }
        /* Add the last dword */
        Hash.Word.Low += *pU32;
        /* Make sure the hash doesn't start with 5, because it could clash
         * with a SAS address. Change 5 to a D.
         */
        if ((Hash.Word.High & 0x000000F0) == (0x00000050)) {
                Hash.Word.High |= 0x00000080;
        }
        *pSASAddress =	(u64)Hash.WWID[0] << 56 | (u64)Hash.WWID[1] << 48 |
			(u64)Hash.WWID[2] << 40 | (u64)Hash.WWID[3] << 32 |
			(u64)Hash.WWID[4] << 24 | (u64)Hash.WWID[5] << 16 |
			(u64)Hash.WWID[6] <<  8 | (u64)Hash.WWID[7];
        return 0;
}

void
mpt2sas_connect_devices(struct MPT2SAS_ADAPTER *ioc, u64 sas_address,
    u16 handle)
{
        unsigned long flags;
        struct MPT2SAS_TARGET *sas_target_priv_data;

        spin_lock_irqsave(&ioc->target_list_lock, flags);
        list_for_each_entry(sas_target_priv_data, &ioc->target_list, list) {
                if (sas_target_priv_data->starget &&
                    (sas_target_priv_data->sas_address == sas_address)) {
			if (sas_target_priv_data->deleted) {
                            dtmprintk(ioc, printk(MPT2SAS_INFO_FMT
                                "Connect C%d:T%d deleted=%d is set to 0\n",
                                ioc->name,
                                sas_target_priv_data->starget->channel,
                                sas_target_priv_data->starget->id,
                                sas_target_priv_data->deleted));
                       	    sas_target_priv_data->deleted = 0;
			}
			if (sas_target_priv_data->tm_busy) {
                            dtmprintk(ioc, printk(MPT2SAS_INFO_FMT
                                "Connect C%d:T%d tm_busy=%d is set to 0\n",
                                ioc->name,
                                sas_target_priv_data->starget->channel,
                                sas_target_priv_data->starget->id,
                                sas_target_priv_data->tm_busy));
				sas_target_priv_data->tm_busy = 0;
			}
			if (sas_target_priv_data->nexus_loss) {
                            dtmprintk(ioc, printk(MPT2SAS_INFO_FMT
                                "Connect C%d:T%d nexus_loss=%d is set to 0\n",
                                ioc->name,
                                sas_target_priv_data->starget->channel,
                                sas_target_priv_data->starget->id,
                                sas_target_priv_data->nexus_loss));
                            sas_target_priv_data->nexus_loss = 0;
			}
			sas_target_priv_data->handle = handle;
                }
        }
        spin_unlock_irqrestore(&ioc->target_list_lock, flags);
}
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27))
/**
 * _scsih_read_capacity_16 - send READ_CAPACITY_16 to target
 * @ioc: per adapter object
 * @handle: expander handle
 * @data: report luns data payload
 * @data_length: length of data in bytes
 * Context: user
 *
 * Returns device_responsive_state
 */
static enum device_responsive_state
_scsih_read_capacity_16(struct MPT2SAS_ADAPTER *ioc, u16 handle, u32 lun,
    void *data, u32 data_length)
{
	struct _scsi_io_transfer *transfer_packet;
	enum device_responsive_state rc;
	void *parameter_data;
	int return_code;

	parameter_data = NULL;
	transfer_packet = kzalloc(sizeof(struct _scsi_io_transfer), GFP_KERNEL);
	if (!transfer_packet) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		rc = DEVICE_RETRY;
		goto out;
	}

	parameter_data = pci_alloc_consistent(ioc->pdev, data_length,
		&transfer_packet->data_dma);
	if (!parameter_data) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		rc = DEVICE_RETRY;
		goto out;
	}

	rc = DEVICE_READY;
	memset(parameter_data, 0, data_length);
	transfer_packet->handle = handle;
	transfer_packet->lun = lun;
	transfer_packet->dir = DMA_FROM_DEVICE;
	transfer_packet->data_length = data_length;
	transfer_packet->cdb_length = 16;
	transfer_packet->cdb[0] = SERVICE_ACTION_IN;
	transfer_packet->cdb[1] = 0x10;
	transfer_packet->cdb[13] = data_length;
	transfer_packet->timeout = 10;

	return_code = _scsi_send_scsi_io(ioc, transfer_packet);
	switch (return_code) {
	case 0:
		rc = _scsih_determine_disposition(ioc, transfer_packet);
		if (rc == DEVICE_READY)
			memcpy(data, parameter_data, data_length);
		break;
	case -EAGAIN:
		rc = DEVICE_RETRY;
		break;
	case -EFAULT:
	default:
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		rc = DEVICE_ERROR;
		break;
	}

 out:
	if (parameter_data)
		pci_free_consistent(ioc->pdev, data_length, parameter_data,
		    transfer_packet->data_dma);
	kfree(transfer_packet);
	return rc;
}
#endif

/**
 * _scsih_inquiry_vpd_sn - obtain device serial number
 * @ioc: per adapter object
 * @handle: device handle
 * @serial_number: returns pointer to serial_number
 * Context: user
 *
 * Returns device_responsive_state
 */
static enum device_responsive_state
_scsih_inquiry_vpd_sn(struct MPT2SAS_ADAPTER *ioc, u16 handle,
    u8 **serial_number)
{
	struct _scsi_io_transfer *transfer_packet;
	enum device_responsive_state rc;
	u8 *inq_data;
	int return_code;
	u32 data_length;
	u8 len;

	inq_data = NULL;
	transfer_packet = kzalloc(sizeof(struct _scsi_io_transfer), GFP_KERNEL);
	if (!transfer_packet) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		rc = DEVICE_RETRY;
		goto out;
	}

	data_length = 252;
	inq_data = pci_alloc_consistent(ioc->pdev, data_length,
		&transfer_packet->data_dma);
	if (!inq_data) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		rc = DEVICE_RETRY;
		goto out;
	}

	rc = DEVICE_READY;
	memset(inq_data, 0, data_length);
	transfer_packet->handle = handle;
	transfer_packet->dir = DMA_FROM_DEVICE;
	transfer_packet->data_length = data_length;
	transfer_packet->cdb_length = 6;
	transfer_packet->cdb[0] = INQUIRY;
	transfer_packet->cdb[1] = 1;
	transfer_packet->cdb[2] = 0x80;
	transfer_packet->cdb[4] = data_length;
	transfer_packet->timeout = 5;

	return_code = _scsi_send_scsi_io(ioc, transfer_packet);
	switch (return_code) {
	case 0:
		rc = _scsih_determine_disposition(ioc, transfer_packet);
		if (rc == DEVICE_READY) {
			len = strlen(&inq_data[4]) + 1;
			*serial_number = kmalloc(len, GFP_KERNEL);
			if (*serial_number)
				strncpy(*serial_number, &inq_data[4], len);
		}
		break;
	case -EAGAIN:
		rc = DEVICE_RETRY;
		break;
	case -EFAULT:
	default:
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		rc = DEVICE_ERROR;
		break;
	}

 out:
	if (inq_data)
		pci_free_consistent(ioc->pdev, data_length, inq_data,
		    transfer_packet->data_dma);
	kfree(transfer_packet);
	return rc;
}

/**
 * _scsih_inquiry_vpd_supported_pages - get supported pages
 * @ioc: per adapter object
 * @handle: device handle
 * @data: report luns data payload
 * @data_length: length of data in bytes
 * Context: user
 *
 * Returns device_responsive_state
 */
static enum device_responsive_state
_scsih_inquiry_vpd_supported_pages(struct MPT2SAS_ADAPTER *ioc, u16 handle,
    u32 lun, void *data, u32 data_length)
{
	struct _scsi_io_transfer *transfer_packet;
	enum device_responsive_state rc;
	void *inq_data;
	int return_code;

	inq_data = NULL;
	transfer_packet = kzalloc(sizeof(struct _scsi_io_transfer), GFP_KERNEL);
	if (!transfer_packet) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		rc = DEVICE_RETRY;
		goto out;
	}

	inq_data = pci_alloc_consistent(ioc->pdev, data_length,
		&transfer_packet->data_dma);
	if (!inq_data) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		rc = DEVICE_RETRY;
		goto out;
	}

	rc = DEVICE_READY;
	memset(inq_data, 0, data_length);
	transfer_packet->handle = handle;
	transfer_packet->dir = DMA_FROM_DEVICE;
	transfer_packet->data_length = data_length;
	transfer_packet->cdb_length = 6;
	transfer_packet->lun = lun;
	transfer_packet->cdb[0] = INQUIRY;
	transfer_packet->cdb[1] = 1;
	transfer_packet->cdb[4] = data_length;
	transfer_packet->timeout = 5;

	return_code = _scsi_send_scsi_io(ioc, transfer_packet);
	switch (return_code) {
	case 0:
		rc = _scsih_determine_disposition(ioc, transfer_packet);
		if (rc == DEVICE_READY)
			memcpy(data, inq_data, data_length);
		break;
	case -EAGAIN:
		rc = DEVICE_RETRY;
		break;
	case -EFAULT:
	default:
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		rc = DEVICE_ERROR;
		break;
	}

 out:
	if (inq_data)
		pci_free_consistent(ioc->pdev, data_length, inq_data,
		    transfer_packet->data_dma);
	kfree(transfer_packet);
	return rc;
}

/**
 * _scsih_report_luns - send REPORT_LUNS to target
 * @ioc: per adapter object
 * @handle: expander handle
 * @data: report luns data payload
 * @data_length: length of data in bytes
 * @is_pd: is this hidden raid component
 * Context: user
 *
 * Returns device_responsive_state
 */
static enum device_responsive_state
_scsih_report_luns(struct MPT2SAS_ADAPTER *ioc, u16 handle, void *data,
    u32 data_length, u8 is_pd)
{
	struct _scsi_io_transfer *transfer_packet;
	enum device_responsive_state rc;
	void *lun_data;
	int return_code;
	int retries;

	lun_data = NULL;
	transfer_packet = kzalloc(sizeof(struct _scsi_io_transfer), GFP_KERNEL);
	if (!transfer_packet) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		rc = DEVICE_RETRY;
		goto out;
	}

	lun_data = pci_alloc_consistent(ioc->pdev, data_length,
		&transfer_packet->data_dma);
	if (!lun_data) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		rc = DEVICE_RETRY;
		goto out;
	}

	for (retries = 0; retries < 4; retries++) {
		rc = DEVICE_ERROR;
		printk(MPT2SAS_INFO_FMT "REPORT_LUNS: handle(0x%04x), "
		    "retries(%d)\n", ioc->name, handle, retries);
		memset(lun_data, 0, data_length);
		transfer_packet->handle = handle;
		transfer_packet->dir = DMA_FROM_DEVICE;
		transfer_packet->data_length = data_length;
		transfer_packet->cdb_length = 12;
		transfer_packet->cdb[0] = REPORT_LUNS;
		transfer_packet->cdb[6] = (data_length >> 24) & 0xFF;
		transfer_packet->cdb[7] = (data_length >> 16) & 0xFF;
		transfer_packet->cdb[8] = (data_length >>  8) & 0xFF;
		transfer_packet->cdb[9] = data_length & 0xFF;
		transfer_packet->timeout = 5;
		transfer_packet->is_raid = is_pd;

		return_code = _scsi_send_scsi_io(ioc, transfer_packet);
		switch (return_code) {
		case 0:
			rc = _scsih_determine_disposition(ioc, transfer_packet);
			if (rc == DEVICE_READY) {
				memcpy(data, lun_data, data_length);
				goto out;
			} else if (rc == DEVICE_ERROR)
				goto out;
			break;
		case -EAGAIN:
			break;
		case -EFAULT:
		default:
			printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
			    ioc->name, __FILE__, __LINE__, __func__);
			goto out;
		}
	}
 out:
	if (rc ==  DEVICE_RETRY) {
		rc = DEVICE_ERROR;
	}

	if (lun_data)
		pci_free_consistent(ioc->pdev, data_length, lun_data,
		    transfer_packet->data_dma);
	kfree(transfer_packet);
	return rc;
}

/**
 * _scsih_start_unit - send START_UNIT to target
 * @ioc: per adapter object
 * @handle: expander handle
 * @lun: lun number
 * @is_pd: is this hidden raid component
 * Context: user
 *
 * Returns device_responsive_state
 */
static enum device_responsive_state
_scsih_start_unit(struct MPT2SAS_ADAPTER *ioc, u16 handle, u32 lun, u8 is_pd)
{
	struct _scsi_io_transfer *transfer_packet;
	enum device_responsive_state rc;
	int return_code;

	transfer_packet = kzalloc(sizeof(struct _scsi_io_transfer), GFP_KERNEL);
	if (!transfer_packet) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		rc = DEVICE_RETRY;
		goto out;
	}

	rc = DEVICE_READY;
	transfer_packet->handle = handle;
	transfer_packet->dir = DMA_NONE;
	transfer_packet->lun = lun;
	transfer_packet->cdb_length = 6;
	transfer_packet->cdb[0] = START_STOP;
	transfer_packet->cdb[1] = 1;
	transfer_packet->cdb[4] = 1;
	transfer_packet->timeout = 1;
	transfer_packet->is_raid = is_pd;

	printk(MPT2SAS_INFO_FMT "START_UNIT: handle(0x%04x), "
	    "lun(%d)\n", ioc->name, handle, lun);

	return_code = _scsi_send_scsi_io(ioc, transfer_packet);
	switch (return_code) {
	case 0:
		rc = _scsih_determine_disposition(ioc, transfer_packet);
		break;
	case -EAGAIN:
		rc = DEVICE_RETRY;
		break;
	case -EFAULT:
	default:
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		rc = DEVICE_ERROR;
		break;
	}
 out:
	kfree(transfer_packet);
	return rc;
}

static enum device_responsive_state
_scsih_stop_unit(struct MPT2SAS_ADAPTER *ioc, u16 handle, u32 lun)
{
	struct _scsi_io_transfer *transfer_packet;
	enum device_responsive_state rc;


	transfer_packet = kzalloc(sizeof(struct _scsi_io_transfer), GFP_KERNEL);
	if (!transfer_packet) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		rc = DEVICE_RETRY;
		goto out;
	}

	rc = DEVICE_READY;
	transfer_packet->handle = handle;
	transfer_packet->dir = DMA_NONE;
	transfer_packet->lun = lun;
	transfer_packet->cdb_length = 6;
	transfer_packet->cdb[0] = START_STOP;
	transfer_packet->cdb[1] = 0;
	transfer_packet->cdb[4] = 0;
	transfer_packet->timeout = 0;
	transfer_packet->is_raid = 0;

	printk(MPT2SAS_INFO_FMT "STOP_UNIT: handle(0x%04x), "
	    "lun(%d)\n", ioc->name, handle, lun);

	rc = _scsi_send_internal_queue_scsi_io(ioc, transfer_packet);
	
 out:
 	if(rc != DEVICE_READY)
		kfree(transfer_packet);

	return rc;
}

static void
_scsih_ssu_to_sata_devices(struct MPT2SAS_ADAPTER *ioc)
{
	enum device_responsive_state rc =DEVICE_ERROR;
	struct scsi_device *sdev;
	struct MPT2SAS_DEVICE *sas_device_priv_data;
	struct _sas_device *sas_device;
	u8 ssu_cmd_sent = 0;
	int count;
	
	__shost_for_each_device(sdev, ioc->shost) {
		sas_device_priv_data = sdev->hostdata;
		if (!sas_device_priv_data) 
			continue;

		sas_device = mpt2sas_scsih_sas_device_find_by_sas_address(ioc,
				sas_device_priv_data->sas_target->sas_address);
		if (!((sas_device && sas_device->device_info &
		    MPI2_SAS_DEVICE_INFO_SATA_DEVICE) &&
		    (!(sdev->inquiry[0] & 0x1f )) && (!(sas_device_priv_data->sas_target->flags &
		    MPT_TARGET_FLAGS_RAID_COMPONENT ||
		    sas_device_priv_data->sas_target->flags & MPT_TARGET_FLAGS_VOLUME))))
			continue;

		sdev_printk(KERN_INFO, sdev, "device is Direct attached sata device, "
		    "handle(0x%04x)\n",
		    sas_device_priv_data->sas_target->handle);

		count = 0;
		do {
			rc = _scsih_stop_unit(ioc, sas_device_priv_data->sas_target->handle, sdev->lun);
			if (rc == DEVICE_RETRY)
				msleep(10);
		} while ((rc == DEVICE_RETRY) && (count++ < command_retry_count));

		if (rc != DEVICE_READY) {
			printk(MPT2SAS_ERR_FMT "%s: Device Not ready handle(0x%04x), sas_addr(0x%016llx) \n",
				ioc->name, __func__, sas_device_priv_data->sas_target->handle, (unsigned long long)
				sas_device_priv_data->sas_target->sas_address);
		}
		else 
			ssu_cmd_sent =1;	
	}
	if (ssu_cmd_sent) {
		/* wait 10 sec  */
		mdelay(10000);
		_scsi_send_stop_unit_cmd_completion_status(ioc);
	}
}
/**
 * _scsih_test_unit_ready - send TUR to target
 * @ioc: per adapter object
 * @handle: expander handle
 * @lun: lun number
 * @is_pd: is this hidden raid component
 * Context: user
 *
 * Returns device_responsive_state
 */
static enum device_responsive_state
_scsih_test_unit_ready(struct MPT2SAS_ADAPTER *ioc, u16 handle, u32 lun,
    u8 is_pd)
{
	struct _scsi_io_transfer *transfer_packet;
	enum device_responsive_state rc;
	int return_code;
	int sata_init_failure = 0;

	transfer_packet = kzalloc(sizeof(struct _scsi_io_transfer), GFP_KERNEL);
	if (!transfer_packet) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		rc = DEVICE_RETRY;
		goto out;
	}

	rc = DEVICE_READY;
	transfer_packet->handle = handle;
	transfer_packet->dir = DMA_NONE;
	transfer_packet->lun = lun;
	transfer_packet->cdb_length = 6;
	transfer_packet->cdb[0] = TEST_UNIT_READY;
	transfer_packet->timeout = 10;
	transfer_packet->is_raid = is_pd;

sata_init_retry:
	printk(MPT2SAS_INFO_FMT "TEST_UNIT_READY: handle(0x%04x), "
	    "lun(%d)\n", ioc->name, handle, lun);

	return_code = _scsi_send_scsi_io(ioc, transfer_packet);
	switch (return_code) {
	case 0:
		rc = _scsih_determine_disposition(ioc, transfer_packet);
		if (rc == DEVICE_RETRY &&
		    transfer_packet->log_info == 0x31111000) {
			if (!sata_init_failure++) {
				printk(MPT2SAS_INFO_FMT
				    "SATA Initialization Timeout,"
				    "sending a retry\n", ioc->name);
				rc = DEVICE_READY;
				goto sata_init_retry;
			} else {
				printk(MPT2SAS_ERR_FMT
				    "SATA Initialization Failed\n", ioc->name);
				rc = DEVICE_ERROR;
			}
		}
		break;
	case -EAGAIN:
		rc = DEVICE_RETRY;
		break;
	case -EFAULT:
	default:
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		rc = DEVICE_ERROR;
		break;
	}
 out:
	kfree(transfer_packet);
	return rc;
}

#define MPT2_MAX_LUNS (255)

/**
 * _scsih_wait_for_device_to_become_ready - handle busy devices
 * @ioc: per adapter object
 * @handle: expander handle
 * @retry_count: number of times this event has been retried
 * @is_pd: is this hidden raid component
 * @lun: lun number
 *
 * Some devices spend too much time in busy state, queue event later
 *
 * Return the device_responsive_state.
 */
static enum device_responsive_state
_scsih_wait_for_device_to_become_ready(struct MPT2SAS_ADAPTER *ioc, u16 handle,
    u8 retry_count, u8 is_pd, int lun)
{
	enum device_responsive_state rc;

	if (ioc->shost_recovery || ioc->pci_error_recovery)
		return DEVICE_ERROR;

	rc = _scsih_test_unit_ready(ioc, handle, lun, is_pd);
	if (rc == DEVICE_READY || rc == DEVICE_ERROR)
		return rc;
	else if (rc == DEVICE_START_UNIT) {
		rc = _scsih_start_unit(ioc, handle, lun, is_pd);
		if (rc == DEVICE_ERROR)
			return rc;
		rc = _scsih_test_unit_ready(ioc, handle, lun, is_pd);
	}

	if ((rc == DEVICE_RETRY || rc == DEVICE_START_UNIT ||
	    rc == DEVICE_RETRY_UA) && retry_count >= command_retry_count)
		rc = DEVICE_ERROR;
	return rc;
}

/**
 * _scsih_wait_for_target_to_become_ready - handle busy devices
 * @ioc: per adapter object
 * @handle: expander handle
 * @retry_count: number of times this event has been retried
 * @is_pd: is this hidden raid component
 *
 * Some devices spend too much time in busy state, queue event later
 *
 * Return the device_responsive_state.
 */
static enum device_responsive_state
_scsih_wait_for_target_to_become_ready(struct MPT2SAS_ADAPTER *ioc, u16 handle,
    u8 retry_count, u8 is_pd)
{
	enum device_responsive_state rc;
	struct scsi_lun *lun_data;
	u32 length, num_luns;
	u8 *data;
	int lun;

	lun_data = kcalloc(MPT2_MAX_LUNS, sizeof(struct scsi_lun), GFP_KERNEL);
	if (!lun_data) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		return DEVICE_RETRY;
	}

	rc = _scsih_report_luns(ioc, handle, lun_data,
	    MPT2_MAX_LUNS * sizeof(struct scsi_lun), is_pd);

	if (rc != DEVICE_READY)
		goto out;

	/* some debug bits*/
	data = (u8 *)lun_data;
	length = ((data[0] << 24) | (data[1] << 16) |
		(data[2] << 8) | (data[3] << 0));

	num_luns = (length / sizeof(struct scsi_lun));
#if 0 /* debug */
	if (num_luns) {
		struct scsi_lun *lunp;
		for (lunp = &lun_data[1]; lunp <= &lun_data[num_luns];
		    lunp++)
			printk(KERN_INFO "%x\n", mpt_scsilun_to_int(lunp));
	}
#endif
	lun = (num_luns) ? mpt_scsilun_to_int(&lun_data[1]) : 0;
	rc = _scsih_wait_for_device_to_become_ready(ioc, handle, retry_count,
	    is_pd, lun);
out:
	kfree(lun_data);
	return rc;
}

/**
 * _scsih_check_access_status - check access flags
 * @ioc: per adapter object
 * @sas_address: sas address
 * @handle: sas device handle
 * @access_flags: errors returned during discovery of the device
 *
 * Return 0 for success, else failure
 */
static u8
_scsih_check_access_status(struct MPT2SAS_ADAPTER *ioc, u64 sas_address,
   u16 handle, u8 access_status)
{
	u8 rc = 1;
	char *desc = NULL;

	switch (access_status) {
	case MPI2_SAS_DEVICE0_ASTATUS_NO_ERRORS:
	case MPI2_SAS_DEVICE0_ASTATUS_SATA_NEEDS_INITIALIZATION:
		rc = 0;
		break;
	case MPI2_SAS_DEVICE0_ASTATUS_SATA_CAPABILITY_FAILED:
		desc = "sata capability failed";
		break;
	case MPI2_SAS_DEVICE0_ASTATUS_SATA_AFFILIATION_CONFLICT:
		desc = "sata affiliation conflict";
		break;
	case MPI2_SAS_DEVICE0_ASTATUS_ROUTE_NOT_ADDRESSABLE:
		desc = "route not addressable";
		break;
	case MPI2_SAS_DEVICE0_ASTATUS_SMP_ERROR_NOT_ADDRESSABLE:
		desc = "smp error not addressable";
		break;
	case MPI2_SAS_DEVICE0_ASTATUS_DEVICE_BLOCKED:
		desc = "device blocked";
		break;
	case MPI2_SAS_DEVICE0_ASTATUS_SATA_INIT_FAILED:
	case MPI2_SAS_DEVICE0_ASTATUS_SIF_UNKNOWN:
	case MPI2_SAS_DEVICE0_ASTATUS_SIF_AFFILIATION_CONFLICT:
	case MPI2_SAS_DEVICE0_ASTATUS_SIF_DIAG:
	case MPI2_SAS_DEVICE0_ASTATUS_SIF_IDENTIFICATION:
	case MPI2_SAS_DEVICE0_ASTATUS_SIF_CHECK_POWER:
	case MPI2_SAS_DEVICE0_ASTATUS_SIF_PIO_SN:
	case MPI2_SAS_DEVICE0_ASTATUS_SIF_MDMA_SN:
	case MPI2_SAS_DEVICE0_ASTATUS_SIF_UDMA_SN:
	case MPI2_SAS_DEVICE0_ASTATUS_SIF_ZONING_VIOLATION:
	case MPI2_SAS_DEVICE0_ASTATUS_SIF_NOT_ADDRESSABLE:
	case MPI2_SAS_DEVICE0_ASTATUS_SIF_MAX:
		desc = "sata initialization failed";
		break;
	default:
		desc = "unknown";
		break;
	}

	if (!rc)
		return 0;

	printk(MPT2SAS_ERR_FMT "discovery errors(%s): sas_address(0x%016llx), "
	    "handle(0x%04x)\n", ioc->name, desc,
	    (unsigned long long)sas_address, handle);
	return rc;
}

/**
 * _scsih_check_device - checking device responsiveness
 * @ioc: per adapter object
 * @parent_sas_address: sas address of parent expander or sas host
 * @handle: attached device handle
 * @phy_numberv: phy number
 * @link_rate: new link rate
 *
 * Returns nothing.
 */
static void
_scsih_check_device(struct MPT2SAS_ADAPTER *ioc,
    u64 parent_sas_address, u16 handle, u8 phy_number, u8 link_rate)
{
	Mpi2ConfigReply_t mpi_reply;
	Mpi2SasDevicePage0_t sas_device_pg0;
	struct _sas_device *sas_device;
	u32 ioc_status;
	unsigned long flags;
	u64 sas_address;
	struct scsi_target *starget;
	struct MPT2SAS_TARGET *sas_target_priv_data;
	u32 device_info;
#if !defined(__VMKLNX__)
	u8 *serial_number = NULL;
	u8 *original_serial_number = NULL;
	int rc;
#endif

	if ((mpt2sas_config_get_sas_device_pg0(ioc, &mpi_reply, &sas_device_pg0,
	    MPI2_SAS_DEVICE_PGAD_FORM_HANDLE, handle)))
		return;

	ioc_status = le16_to_cpu(mpi_reply.IOCStatus) & MPI2_IOCSTATUS_MASK;
	if (ioc_status != MPI2_IOCSTATUS_SUCCESS)
		return;

	/* wide port handling ~ we need only handle device once for the phy that
	 * is matched in sas device page zero
	 */
	if (phy_number != sas_device_pg0.PhyNum)
		return;

	/* check if this is end device */
	device_info = le32_to_cpu(sas_device_pg0.DeviceInfo);
	if (!(_scsih_is_end_device(device_info)))
		return;

	spin_lock_irqsave(&ioc->sas_device_lock, flags);
	sas_address = le64_to_cpu(sas_device_pg0.SASAddress);
	sas_device = mpt2sas_scsih_sas_device_find_by_sas_address(ioc,
	    sas_address);

	if (!sas_device) {
		spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
		return;
	}

	if (unlikely(sas_device->handle != handle)) {
		starget = sas_device->starget;
		sas_target_priv_data = starget->hostdata;
		starget_printk(KERN_INFO, starget, "handle changed from(0x%04x)"
		   " to (0x%04x)!!!\n", sas_device->handle, handle);
		sas_target_priv_data->handle = handle;
		sas_device->handle = handle;
	}

	/* check if device is present */
	if (!(le16_to_cpu(sas_device_pg0.Flags) &
	    MPI2_SAS_DEVICE0_FLAGS_DEVICE_PRESENT)) {
		printk(MPT2SAS_ERR_FMT "device is not present "
		    "handle(0x%04x), flags!!!\n", ioc->name, handle);
		spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
		return;
	}

	/* check if there were any issues with discovery */
	if (_scsih_check_access_status(ioc, sas_address, handle,
	    sas_device_pg0.AccessStatus)) {
		spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
		return;
	}
#if !defined(__VMKLNX__)
	original_serial_number = sas_device->serial_number;
#endif
	spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
	_scsih_ublock_io_device_wait(ioc, sas_address);

#if !defined(__VMKLNX__)
	/* check to see if serial number still the same, if not, delete
	 * and re-add new device
	 */
	if (!original_serial_number)
		return;

	if (_scsih_inquiry_vpd_sn(ioc, handle, &serial_number) == DEVICE_READY
	    && serial_number)  {
		rc = strcmp(original_serial_number, serial_number);
		kfree(serial_number);
		if (!rc)
			return;
		_scsih_device_remove_by_handle(ioc, handle);
		mpt2sas_transport_update_links(ioc, parent_sas_address,
		    handle, phy_number, link_rate);
		_scsih_add_device(ioc, handle, 0, 0);
	}
#endif

}


/**
 * _scsih_add_device -  creating sas device object
 * @ioc: per adapter object
 * @handle: sas device handle
 * @retry_count: number of times this event has been retried
 * @is_pd: is this hidden raid component
 *
 * Creating end device object, stored in ioc->sas_device_list.
 *
 * Return 1 means queue the event later, 0 means complete the event
 */
static int
_scsih_add_device(struct MPT2SAS_ADAPTER *ioc, u16 handle, u8 retry_count,
    u8 is_pd)
{
	Mpi2ConfigReply_t mpi_reply;
	Mpi2SasDevicePage0_t sas_device_pg0;
	Mpi2SasEnclosurePage0_t enclosure_pg0;
	struct _sas_device *sas_device;
	u32 ioc_status;
	u64 sas_address;
	u32 device_info;
	unsigned long flags;
	enum device_responsive_state rc;

	if ((mpt2sas_config_get_sas_device_pg0(ioc, &mpi_reply, &sas_device_pg0,
	    MPI2_SAS_DEVICE_PGAD_FORM_HANDLE, handle))) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		return 0;
	}

	ioc_status = le16_to_cpu(mpi_reply.IOCStatus) &
	    MPI2_IOCSTATUS_MASK;
	if (ioc_status != MPI2_IOCSTATUS_SUCCESS) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		return 0;
	}

	/* check if this is end device */
	device_info = le32_to_cpu(sas_device_pg0.DeviceInfo);
	if (!(_scsih_is_end_device(device_info)))
		return 0;
	sas_address = le64_to_cpu(sas_device_pg0.SASAddress);

	/* check if device is present */
	if (!(le16_to_cpu(sas_device_pg0.Flags) &
	    MPI2_SAS_DEVICE0_FLAGS_DEVICE_PRESENT)) {
		printk(MPT2SAS_ERR_FMT "device is not present "
		    "handle(0x04%x)!!!\n", ioc->name, handle);
		return 0;
	}

	/* check if there were any issues with discovery */
	if (_scsih_check_access_status(ioc, sas_address, handle,
	    sas_device_pg0.AccessStatus))
		return 0;

	spin_lock_irqsave(&ioc->sas_device_lock, flags);
	sas_device = mpt2sas_scsih_sas_device_find_by_sas_address(ioc,
	    sas_address);
	spin_unlock_irqrestore(&ioc->sas_device_lock, flags);

	if (sas_device)
		return 0;

	/*
	 * Wait for device that is becoming ready
	 * queue request later if device is busy.
	 */
	if (!ioc->wait_for_port_enable_to_complete) {
		printk(MPT2SAS_INFO_FMT "detecting: handle(0x%04x), "
		    "sas_address(0x%016llx), phy(%d)\n", ioc->name, handle,
		    (unsigned long long)sas_address, sas_device_pg0.PhyNum);
		rc = _scsih_wait_for_target_to_become_ready(ioc, handle,
		    retry_count, is_pd);
		if (rc == DEVICE_RETRY || rc == DEVICE_START_UNIT ||
		    rc == DEVICE_RETRY_UA)
			return 1;
		else if (rc == DEVICE_ERROR)
			return 0;
	}

	sas_device = kzalloc(sizeof(struct _sas_device),
	    GFP_KERNEL);
	if (!sas_device) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		return 0;
	}

	kref_init(&sas_device->kref);
	sas_device->handle = handle;
	if (_scsih_get_sas_address(ioc,
	    le16_to_cpu(sas_device_pg0.ParentDevHandle),
	    &sas_device->sas_address_parent) != 0)
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
	sas_device->enclosure_handle =
	    le16_to_cpu(sas_device_pg0.EnclosureHandle);
	sas_device->slot =
	    le16_to_cpu(sas_device_pg0.Slot);
	sas_device->device_info = device_info;
	sas_device->sas_address = sas_address;
 	sas_device->phy = sas_device_pg0.PhyNum;
#if defined(__VMKLNX__)
	/* Get sata_id for SATA end device */
	if ((MPI2_SAS_DEVICE_INFO_END_DEVICE ==
	     (device_info & MPI2_SAS_DEVICE_INFO_MASK_DEVICE_TYPE)) &&
	    (device_info & MPI2_SAS_DEVICE_INFO_SATA_DEVICE)) {
		mpt2sas_scsih_get_sas_address_for_sata_disk(ioc,
		    &sas_device->sata_id, handle);
	}
#endif

	/* get enclosure_logical_id */
	if (sas_device->enclosure_handle && !(mpt2sas_config_get_enclosure_pg0(
	   ioc, &mpi_reply, &enclosure_pg0, MPI2_SAS_ENCLOS_PGAD_FORM_HANDLE,
	   sas_device->enclosure_handle)))
		sas_device->enclosure_logical_id =
		    le64_to_cpu(enclosure_pg0.EnclosureLogicalID);

	/* get device name */
	sas_device->device_name = le64_to_cpu(sas_device_pg0.DeviceName);

	if (ioc->wait_for_port_enable_to_complete)
		_scsih_sas_device_init_add(ioc, sas_device);
	else
		_scsih_sas_device_add(ioc, sas_device);

	return 0;
}

/**
 * _scsih_remove_device -  removing sas device object
 * @ioc: per adapter object
 * @sas_device_delete: the sas_device object
 *
 * Return nothing.
 */
static void
_scsih_remove_device(struct MPT2SAS_ADAPTER *ioc,
    struct _sas_device *sas_device)
{
	struct MPT2SAS_TARGET *sas_target_priv_data;

#ifdef MPT2SAS_MULTIPATH
	if (sas_device->sas_device_alt) {
		sas_device->sas_device_alt->sas_device_alt = NULL;
		sas_device->sas_device_alt->ioc = NULL;
	}
#endif

	if ((ioc->pdev->subsystem_vendor == PCI_VENDOR_ID_IBM) &&
		(sas_device->fault_led_on)){
		_scsih_turn_off_pfa_led(ioc, sas_device);
		sas_device->fault_led_on = 0;	
	}
	
	dewtprintk(ioc, printk(MPT2SAS_INFO_FMT "%s: enter: "
	    "handle(0x%04x), sas_addr(0x%016llx)\n", ioc->name, __func__,
	    sas_device->handle, (unsigned long long)
	    sas_device->sas_address));

	if (sas_device->starget && sas_device->starget->hostdata) {
		sas_target_priv_data = sas_device->starget->hostdata;
		if (sas_target_priv_data->deleted == 0) {
			sas_target_priv_data->deleted = 1;
			_scsih_ublock_io_device(ioc, sas_device->sas_address);
			sas_target_priv_data->handle =
			    MPT2SAS_INVALID_DEVICE_HANDLE;
		}

#if defined(__VMKLNX__)
		sas_target_priv_data->configured = 0;
#endif
	}

	if (!ioc->hide_drives)
		mpt2sas_transport_port_remove(ioc,
		    sas_device->sas_address,
		    sas_device->sas_address_parent);

	printk(MPT2SAS_INFO_FMT "removing handle(0x%04x), sas_addr"
	    "(0x%016llx)\n", ioc->name, sas_device->handle,
	    (unsigned long long) sas_device->sas_address);

	dewtprintk(ioc, printk(MPT2SAS_INFO_FMT "%s: exit: "
	    "handle(0x%04x), sas_addr(0x%016llx)\n", ioc->name, __func__,
	    sas_device->handle, (unsigned long long)
	    sas_device->sas_address));

	kfree(sas_device->serial_number);
	put_sas_device(sas_device);
}

#ifdef CONFIG_SCSI_MPT2SAS_LOGGING
/**
 * _scsih_sas_topology_change_event_debug - debug for topology event
 * @ioc: per adapter object
 * @event_data: event data payload
 * Context: user.
 */
static void
_scsih_sas_topology_change_event_debug(struct MPT2SAS_ADAPTER *ioc,
    Mpi2EventDataSasTopologyChangeList_t *event_data)
{
	int i;
	u16 handle;
	u16 reason_code;
	u8 phy_number;
	char *status_str = NULL;
	u8 link_rate, prev_link_rate;

	switch (event_data->ExpStatus) {
	case MPI2_EVENT_SAS_TOPO_ES_ADDED:
		status_str = "add";
		break;
	case MPI2_EVENT_SAS_TOPO_ES_NOT_RESPONDING:
		status_str = "remove";
		break;
	case MPI2_EVENT_SAS_TOPO_ES_RESPONDING:
	case 0:
		status_str =  "responding";
		break;
	case MPI2_EVENT_SAS_TOPO_ES_DELAY_NOT_RESPONDING:
		status_str = "remove delay";
		break;
	default:
		status_str = "unknown status";
		break;
	}
	printk(MPT2SAS_INFO_FMT "sas topology change: (%s)\n",
	    ioc->name, status_str);
	printk(KERN_DEBUG "\thandle(0x%04x), enclosure_handle(0x%04x) "
	    "start_phy(%02d), count(%d)\n",
	    le16_to_cpu(event_data->ExpanderDevHandle),
	    le16_to_cpu(event_data->EnclosureHandle),
	    event_data->StartPhyNum, event_data->NumEntries);
	for (i = 0; i < event_data->NumEntries; i++) {
		handle = le16_to_cpu(event_data->PHY[i].AttachedDevHandle);
		if (!handle)
			continue;
		phy_number = event_data->StartPhyNum + i;
		reason_code = event_data->PHY[i].PhyStatus &
		    MPI2_EVENT_SAS_TOPO_RC_MASK;
		switch (reason_code) {
		case MPI2_EVENT_SAS_TOPO_RC_TARG_ADDED:
			status_str = "target add";
			break;
		case MPI2_EVENT_SAS_TOPO_RC_TARG_NOT_RESPONDING:
			status_str = "target remove";
			break;
		case MPI2_EVENT_SAS_TOPO_RC_DELAY_NOT_RESPONDING:
			status_str = "delay target remove";
			break;
		case MPI2_EVENT_SAS_TOPO_RC_PHY_CHANGED:
			status_str = "link rate change";
			break;
		case MPI2_EVENT_SAS_TOPO_RC_NO_CHANGE:
			status_str = "target responding";
			break;
		default:
			status_str = "unknown";
			break;
		}
		link_rate = event_data->PHY[i].LinkRate >> 4;
		prev_link_rate = event_data->PHY[i].LinkRate & 0xF;
		printk(KERN_DEBUG "\tphy(%02d), attached_handle(0x%04x): %s:"
		    " link rate: new(0x%02x), old(0x%02x)\n", phy_number,
		    handle, status_str, link_rate, prev_link_rate);

	}
}
#endif

/**
 * _scsih_sas_topology_change_event - handle topology changes
 * @ioc: per adapter object
 * @fw_event: The fw_event_work object
 * Context: user.
 *
 */
static int
_scsih_sas_topology_change_event(struct MPT2SAS_ADAPTER *ioc,
    struct fw_event_work *fw_event)
{
	int i;
	u16 parent_handle, handle;
	u16 reason_code;
	u8 phy_number, max_phys;
	struct _sas_node *sas_expander;
	u64 sas_address;
	unsigned long flags;
	u8 link_rate, prev_link_rate;
	int rc;
	int requeue_event;
	Mpi2EventDataSasTopologyChangeList_t *event_data = fw_event->event_data;

#ifdef CONFIG_SCSI_MPT2SAS_LOGGING
	if (ioc->logging_level & MPT_DEBUG_EVENT_WORK_TASK)
		_scsih_sas_topology_change_event_debug(ioc, event_data);
#endif

	if (ioc->shost_recovery || ioc->remove_host || ioc->pci_error_recovery)
		return 0;

	if (!ioc->sas_hba.num_phys)
		_scsih_sas_host_add(ioc);
	else
		_scsih_sas_host_refresh(ioc);

	if (fw_event->ignore) {
		dewtprintk(ioc, printk(MPT2SAS_INFO_FMT "ignoring expander "
		    "event\n", ioc->name));
		return 0;
	}

	parent_handle = le16_to_cpu(event_data->ExpanderDevHandle);

	/* handle expander add */
	if (event_data->ExpStatus == MPI2_EVENT_SAS_TOPO_ES_ADDED)
		if (_scsih_expander_add(ioc, parent_handle) != 0)
			return 0;

	spin_lock_irqsave(&ioc->sas_node_lock, flags);
	sas_expander = mpt2sas_scsih_expander_find_by_handle(ioc,
	    parent_handle);
	if (sas_expander) {
		sas_address = sas_expander->sas_address;
		max_phys = sas_expander->num_phys;
	} else if (parent_handle < ioc->sas_hba.num_phys) {
		sas_address = ioc->sas_hba.sas_address;
		max_phys = ioc->sas_hba.num_phys;
	} else {
		spin_unlock_irqrestore(&ioc->sas_node_lock, flags);
		return 0;
	}
	spin_unlock_irqrestore(&ioc->sas_node_lock, flags);

	/* handle siblings events */
	for (i = 0, requeue_event = 0; i < event_data->NumEntries; i++) {
		if (fw_event->ignore) {
			dewtprintk(ioc, printk(MPT2SAS_INFO_FMT "ignoring "
			    "expander event\n", ioc->name));
			return 0;
		}
		if (ioc->shost_recovery || ioc->remove_host ||
		    ioc->pci_error_recovery)
			return 0;
		phy_number = event_data->StartPhyNum + i;
		if (phy_number >= max_phys)
			continue;
		reason_code = event_data->PHY[i].PhyStatus &
		    MPI2_EVENT_SAS_TOPO_RC_MASK;
		if ((event_data->PHY[i].PhyStatus &
		    MPI2_EVENT_SAS_TOPO_PHYSTATUS_VACANT) && (reason_code !=
		    MPI2_EVENT_SAS_TOPO_RC_TARG_NOT_RESPONDING))
				continue;
		handle = le16_to_cpu(event_data->PHY[i].AttachedDevHandle);
		if (!handle)
			continue;
		link_rate = event_data->PHY[i].LinkRate >> 4;
		prev_link_rate = event_data->PHY[i].LinkRate & 0xF;
		switch (reason_code) {
		case MPI2_EVENT_SAS_TOPO_RC_PHY_CHANGED:

			if (link_rate == prev_link_rate)
				break;

			mpt2sas_transport_update_links(ioc, sas_address,
			    handle, phy_number, link_rate);

			if (link_rate < MPI2_SAS_NEG_LINK_RATE_1_5)
				break;
#if defined(__VMKLNX__)
			/* Clear nexus_loss and deleted flags if devices are inserted before DMD timeout */
			u64 temp_sas_address = 0;
 			_scsih_check_device(ioc, sas_address, handle,
 			    phy_number, link_rate);

			if (_scsih_get_sas_address(ioc, handle,
			    &temp_sas_address) == 0) {
				mpt2sas_connect_devices(ioc,
				    temp_sas_address, handle);
			}
#else
 			_scsih_check_device(ioc, sas_address, handle,
 			    phy_number, link_rate);
#endif

			break;
		case MPI2_EVENT_SAS_TOPO_RC_TARG_ADDED:

			mpt2sas_transport_update_links(ioc, sas_address,
			    handle, phy_number, link_rate);

			if (link_rate < MPI2_SAS_NEG_LINK_RATE_1_5)
				break;

			rc = _scsih_add_device(ioc, handle,
			    fw_event->retries[i], 0);
			if (rc) {/* retry due to busy device */
				fw_event->retries[i]++;
				requeue_event = 1;
			} else {/* mark entry vacant */
				event_data->PHY[i].PhyStatus |=
			    MPI2_EVENT_SAS_TOPO_PHYSTATUS_VACANT;
			}
			break;
		case MPI2_EVENT_SAS_TOPO_RC_TARG_NOT_RESPONDING:

			_scsih_device_remove_by_handle(ioc, handle);
			break;
		}
	}

	/* handle expander removal */
	if (event_data->ExpStatus == MPI2_EVENT_SAS_TOPO_ES_NOT_RESPONDING &&
	    sas_expander)
		_scsih_expander_remove(ioc, sas_address);

	return requeue_event;
}

#ifdef CONFIG_SCSI_MPT2SAS_LOGGING
/**
 * _scsih_sas_device_status_change_event_debug - debug for device event
 * @event_data: event data payload
 * Context: user.
 *
 * Return nothing.
 */
static void
_scsih_sas_device_status_change_event_debug(struct MPT2SAS_ADAPTER *ioc,
    Mpi2EventDataSasDeviceStatusChange_t *event_data)
{
	char *reason_str = NULL;

	switch (event_data->ReasonCode) {
	case MPI2_EVENT_SAS_DEV_STAT_RC_SMART_DATA:
		reason_str = "smart data";
		break;
	case MPI2_EVENT_SAS_DEV_STAT_RC_UNSUPPORTED:
		reason_str = "unsupported device discovered";
		break;
	case MPI2_EVENT_SAS_DEV_STAT_RC_INTERNAL_DEVICE_RESET:
		reason_str = "internal device reset";
		break;
	case MPI2_EVENT_SAS_DEV_STAT_RC_TASK_ABORT_INTERNAL:
		reason_str = "internal task abort";
		break;
	case MPI2_EVENT_SAS_DEV_STAT_RC_ABORT_TASK_SET_INTERNAL:
		reason_str = "internal task abort set";
		break;
	case MPI2_EVENT_SAS_DEV_STAT_RC_CLEAR_TASK_SET_INTERNAL:
		reason_str = "internal clear task set";
		break;
	case MPI2_EVENT_SAS_DEV_STAT_RC_QUERY_TASK_INTERNAL:
		reason_str = "internal query task";
		break;
	case MPI2_EVENT_SAS_DEV_STAT_RC_SATA_INIT_FAILURE:
		reason_str = "sata init failure";
		break;
	case MPI2_EVENT_SAS_DEV_STAT_RC_CMP_INTERNAL_DEV_RESET:
		reason_str = "internal device reset complete";
		break;
	case MPI2_EVENT_SAS_DEV_STAT_RC_CMP_TASK_ABORT_INTERNAL:
		reason_str = "internal task abort complete";
		break;
	case MPI2_EVENT_SAS_DEV_STAT_RC_ASYNC_NOTIFICATION:
		reason_str = "internal async notification";
		break;
	case MPI2_EVENT_SAS_DEV_STAT_RC_EXPANDER_REDUCED_FUNCTIONALITY:
		reason_str = "expander reduced functionality";
		break;
	case MPI2_EVENT_SAS_DEV_STAT_RC_CMP_EXPANDER_REDUCED_FUNCTIONALITY:
		reason_str = "expander reduced functionality complete";
		break;
	default:
		reason_str = "unknown reason";
		break;
	}
	printk(MPT2SAS_INFO_FMT "device status change: (%s)\n"
	    "\thandle(0x%04x), sas address(0x%016llx)", ioc->name,
	    reason_str, le16_to_cpu(event_data->DevHandle),
	    (unsigned long long)le64_to_cpu(event_data->SASAddress));
	if (event_data->ReasonCode == MPI2_EVENT_SAS_DEV_STAT_RC_SMART_DATA)
		printk(MPT2SAS_INFO_FMT ", ASC(0x%x), ASCQ(0x%x)\n", ioc->name,
		    event_data->ASC, event_data->ASCQ);
	printk(KERN_INFO "\n");
}
#endif

/**
 * _scsih_sas_device_status_change_event - handle device status change
 * @ioc: per adapter object
 * @fw_event: The fw_event_work object
 * Context: user.
 *
 * Return nothing.
 */
static void
_scsih_sas_device_status_change_event(struct MPT2SAS_ADAPTER *ioc,
    struct fw_event_work *fw_event)
{
	struct MPT2SAS_TARGET *target_priv_data;
	struct _sas_device *sas_device;
	u64 sas_address;
	unsigned long flags;
	Mpi2EventDataSasDeviceStatusChange_t *event_data =
	    fw_event->event_data;

#ifdef CONFIG_SCSI_MPT2SAS_LOGGING
	if (ioc->logging_level & MPT_DEBUG_EVENT_WORK_TASK)
		_scsih_sas_device_status_change_event_debug(ioc,
		     event_data);
#endif
	if ((ioc->facts.HeaderVersion >> 8) < 0xC)
		return;

	if (event_data->ReasonCode !=
	    MPI2_EVENT_SAS_DEV_STAT_RC_INTERNAL_DEVICE_RESET &&
	   event_data->ReasonCode !=
	    MPI2_EVENT_SAS_DEV_STAT_RC_CMP_INTERNAL_DEV_RESET)
		return;

	spin_lock_irqsave(&ioc->sas_device_lock, flags);
	sas_address = le64_to_cpu(event_data->SASAddress);
	sas_device = mpt2sas_scsih_sas_device_find_by_sas_address(ioc,
	    sas_address);

	if (!sas_device || !sas_device->starget){
		spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
		return;
	}

	target_priv_data = sas_device->starget->hostdata;
	if (!target_priv_data) {
		spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
		return;
	}

	if (event_data->ReasonCode ==
	    MPI2_EVENT_SAS_DEV_STAT_RC_INTERNAL_DEVICE_RESET)
		target_priv_data->tm_busy = 1;
	else
		target_priv_data->tm_busy = 0;
	spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
}

#ifdef CONFIG_SCSI_MPT2SAS_LOGGING
/**
 * _scsih_sas_enclosure_dev_status_change_event_debug - debug for enclosure event
 * @ioc: per adapter object
 * @event_data: event data payload
 * Context: user.
 *
 * Return nothing.
 */
static void
_scsih_sas_enclosure_dev_status_change_event_debug(struct MPT2SAS_ADAPTER *ioc,
    Mpi2EventDataSasEnclDevStatusChange_t *event_data)
{
	char *reason_str = NULL;

	switch (event_data->ReasonCode) {
	case MPI2_EVENT_SAS_ENCL_RC_ADDED:
		reason_str = "enclosure add";
		break;
	case MPI2_EVENT_SAS_ENCL_RC_NOT_RESPONDING:
		reason_str = "enclosure remove";
		break;
	default:
		reason_str = "unknown reason";
		break;
	}

	printk(MPT2SAS_INFO_FMT "enclosure status change: (%s)\n"
	    "\thandle(0x%04x), enclosure logical id(0x%016llx)"
	    " number slots(%d)\n", ioc->name, reason_str,
	    le16_to_cpu(event_data->EnclosureHandle),
	    (unsigned long long)le64_to_cpu(event_data->EnclosureLogicalID),
	    le16_to_cpu(event_data->StartSlot));
}
#endif

/**
 * _scsih_sas_enclosure_dev_status_change_event - handle enclosure events
 * @ioc: per adapter object
 * @fw_event: The fw_event_work object
 * Context: user.
 *
 * Return nothing.
 */
static void
_scsih_sas_enclosure_dev_status_change_event(struct MPT2SAS_ADAPTER *ioc,
    struct fw_event_work *fw_event)
{
#ifdef CONFIG_SCSI_MPT2SAS_LOGGING
	if (ioc->logging_level & MPT_DEBUG_EVENT_WORK_TASK)
		_scsih_sas_enclosure_dev_status_change_event_debug(ioc,
		     fw_event->event_data);
#endif
}

/**
 * _scsih_sas_broadcast_primative_event - handle broadcast events
 * @ioc: per adapter object
 * @fw_event: The fw_event_work object
 * Context: user.
 *
 * Return nothing.
 */
static void
_scsih_sas_broadcast_primative_event(struct MPT2SAS_ADAPTER *ioc,
    struct fw_event_work *fw_event)
{
	struct scsi_cmnd *scmd;
	struct scsi_device *sdev;
	u16 smid, handle;
	u32 lun;
	struct MPT2SAS_DEVICE *sas_device_priv_data;
	u32 termination_count;
	u32 query_count;
	Mpi2SCSITaskManagementReply_t *mpi_reply;
	Mpi2EventDataSasBroadcastPrimitive_t *event_data = fw_event->event_data;
	u16 ioc_status;
	unsigned long flags;
	int r;
	
	dewtprintk(ioc, printk(MPT2SAS_INFO_FMT "broadcast primative: "
	    "phy number(%d), width(%d), primative(%d)\n", ioc->name,
	    event_data->PhyNum, event_data->PortWidth, event_data->Primitive));

	dtmprintk(ioc, printk(MPT2SAS_INFO_FMT "%s: enter\n", ioc->name,
	    __func__));

	if (event_data->Primitive != MPI2_EVENT_PRIMITIVE_ASYNCHRONOUS_EVENT)
		return;

	spin_lock_irqsave(&ioc->scsi_lookup_lock, flags);
	ioc->broadcast_aen_busy = 0;
	termination_count = 0;
	query_count = 0;
	mpi_reply = ioc->tm_cmds.reply;
	for (smid = 1; smid <= ioc->scsiio_depth; smid++) {
		scmd = _scsih_scsi_lookup_get(ioc, smid);
		if (!scmd)
			continue;
		sdev = scmd->device;
		sas_device_priv_data = sdev->hostdata;
		if (!sas_device_priv_data || !sas_device_priv_data->sas_target)
			continue;
		 /* skip hidden raid components */
		if (sas_device_priv_data->sas_target->flags &
		    MPT_TARGET_FLAGS_RAID_COMPONENT)
			continue;
		 /* skip volumes */
		if (sas_device_priv_data->sas_target->flags &
		    MPT_TARGET_FLAGS_VOLUME)
			continue;

		handle = sas_device_priv_data->sas_target->handle;
		lun = sas_device_priv_data->lun;
		query_count++;

		spin_unlock_irqrestore(&ioc->scsi_lookup_lock, flags);
		mpt2sas_scsih_issue_tm(ioc, handle, 0, 0, lun,
		    MPI2_SCSITASKMGMT_TASKTYPE_QUERY_TASK, smid, 30, NULL);
		ioc->tm_cmds.status = MPT2_CMD_NOT_USED;
		ioc_status = le16_to_cpu(mpi_reply->IOCStatus)
		    & MPI2_IOCSTATUS_MASK;
		if ((ioc_status == MPI2_IOCSTATUS_SUCCESS) &&
		    (mpi_reply->ResponseCode ==
		     MPI2_SCSITASKMGMT_RSP_TM_SUCCEEDED ||
		     mpi_reply->ResponseCode ==
		     MPI2_SCSITASKMGMT_RSP_IO_QUEUED_ON_IOC)) {
			spin_lock_irqsave(&ioc->scsi_lookup_lock, flags);
			continue;
		}
		r = mpt2sas_scsih_issue_tm(ioc, handle, sdev->channel, sdev->id,
		    sdev->lun, MPI2_SCSITASKMGMT_TASKTYPE_ABORT_TASK, smid, 30,
		    scmd);
		if (r == FAILED)
			sdev_printk(KERN_WARNING, sdev, "task abort: FAILED "
			    "scmd(%p)\n", scmd);
		termination_count += le32_to_cpu(mpi_reply->TerminationCount);
		spin_lock_irqsave(&ioc->scsi_lookup_lock, flags);
	}
	spin_unlock_irqrestore(&ioc->scsi_lookup_lock, flags);

	dtmprintk(ioc, printk(MPT2SAS_INFO_FMT
	    "%s - exit, query_count = %d termination_count = %d\n",
	    ioc->name, __func__, query_count, termination_count));
}

/**
 * _scsih_sas_discovery_event - handle discovery events
 * @ioc: per adapter object
 * @fw_event: The fw_event_work object
 * Context: user.
 *
 * Return nothing.
 */
static void
_scsih_sas_discovery_event(struct MPT2SAS_ADAPTER *ioc,
    struct fw_event_work *fw_event)
{
	Mpi2EventDataSasDiscovery_t *event_data = fw_event->event_data;

#ifdef CONFIG_SCSI_MPT2SAS_LOGGING
	if (ioc->logging_level & MPT_DEBUG_EVENT_WORK_TASK) {
		printk(MPT2SAS_INFO_FMT "discovery event: (%s)", ioc->name,
		    (event_data->ReasonCode == MPI2_EVENT_SAS_DISC_RC_STARTED) ?
		    "start" : "stop");
	if (event_data->DiscoveryStatus)
		printk("discovery_status(0x%08x)",
		    le32_to_cpu(event_data->DiscoveryStatus));
	printk("\n");
	}
#endif

	if (event_data->ReasonCode == MPI2_EVENT_SAS_DISC_RC_STARTED &&
	    !ioc->sas_hba.num_phys)
		_scsih_sas_host_add(ioc);

	if (ioc->wait_for_port_enable_to_complete &&
	    event_data->ReasonCode == MPI2_EVENT_SAS_DISC_RC_COMPLETED)
		complete(&ioc->port_enable_done);
}

/**
 * _scsih_reprobe_lun - reprobing lun
 * @sdev: scsi device struct
 * @no_uld_attach: sdev->no_uld_attach flag setting
 *
 **/
static void
_scsih_reprobe_lun(struct scsi_device *sdev, void *no_uld_attach)
{
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,18))
	int rc;
#endif
	sdev->no_uld_attach = no_uld_attach ? 1 : 0;
	sdev_printk(KERN_INFO, sdev, "%s raid component\n",
	    sdev->no_uld_attach ? "hidding" : "exposing");
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,18))
	rc = scsi_device_reprobe(sdev);
#else
	scsi_device_reprobe(sdev);
#endif
}

/**
 * _scsih_sas_volume_add - add new volume
 * @ioc: per adapter object
 * @element: IR config element data
 * Context: user.
 *
 * Return nothing.
 */
static void
_scsih_sas_volume_add(struct MPT2SAS_ADAPTER *ioc,
    Mpi2EventIrConfigElement_t *element)
{
	struct _raid_device *raid_device;
	unsigned long flags;
	u64 wwid;
	u16 handle = le16_to_cpu(element->VolDevHandle);
	int rc;

	mpt2sas_config_get_volume_wwid(ioc, handle, &wwid);
	if (!wwid) {
		printk(MPT2SAS_ERR_FMT
		    "failure at %s:%d/%s()!\n", ioc->name,
		    __FILE__, __LINE__, __func__);
		return;
	}

	spin_lock_irqsave(&ioc->raid_device_lock, flags);
	raid_device = _scsih_raid_device_find_by_wwid(ioc, wwid);
	spin_unlock_irqrestore(&ioc->raid_device_lock, flags);

	if (raid_device)
		return;

	raid_device = kzalloc(sizeof(struct _raid_device), GFP_KERNEL);
	if (!raid_device) {
		printk(MPT2SAS_ERR_FMT
		    "failure at %s:%d/%s()!\n", ioc->name,
		    __FILE__, __LINE__, __func__);
		return;
	}

	raid_device->id = ioc->sas_id++;
	raid_device->channel = RAID_CHANNEL;
	raid_device->handle = handle;
	raid_device->wwid = wwid;
	_scsih_raid_device_add(ioc, raid_device);
	if (!ioc->wait_for_port_enable_to_complete) {
		rc = scsi_add_device(ioc->shost, RAID_CHANNEL,
		    raid_device->id, 0);
		if (rc)
			_scsih_raid_device_remove(ioc, raid_device);
	} else {
		spin_lock_irqsave(&ioc->raid_device_lock, flags);
		_scsih_determine_boot_device(ioc, raid_device, 1);
		spin_unlock_irqrestore(&ioc->raid_device_lock, flags);
	}
}

/**
 * _scsih_sas_volume_delete - delete volume
 * @ioc: per adapter object
 * @handle: volume device handle
 * Context: user.
 *
 * Return nothing.
 */
static void
_scsih_sas_volume_delete(struct MPT2SAS_ADAPTER *ioc, u16 handle)
{
	struct _raid_device *raid_device;
	unsigned long flags;
	struct MPT2SAS_TARGET *sas_target_priv_data;
	struct scsi_target *starget = NULL;

	spin_lock_irqsave(&ioc->raid_device_lock, flags);
	raid_device = _scsih_raid_device_find_by_handle(ioc, handle);
	if (raid_device) {
		if (raid_device->starget) {
			starget = raid_device->starget;
			sas_target_priv_data = raid_device->starget->hostdata;
			sas_target_priv_data->deleted = 1;
		}
		printk(MPT2SAS_INFO_FMT "removing handle(0x%04x), wwid"
		    "(0x%016llx)\n", ioc->name,  raid_device->handle,
		    (unsigned long long) raid_device->wwid);
		list_del(&raid_device->list);
		kfree(raid_device);
	}
	spin_unlock_irqrestore(&ioc->raid_device_lock, flags);
	if (starget)
		scsi_remove_target(&starget->dev);
}

/**
 * _scsih_sas_pd_expose - expose pd component to /dev/sdX
 * @ioc: per adapter object
 * @element: IR config element data
 * Context: user.
 *
 * Return nothing.
 */
static void
_scsih_sas_pd_expose(struct MPT2SAS_ADAPTER *ioc,
    Mpi2EventIrConfigElement_t *element)
{
	struct _sas_device *sas_device;
	struct scsi_target *starget = NULL;
	struct MPT2SAS_TARGET *sas_target_priv_data;
	unsigned long flags;
	u16 handle = le16_to_cpu(element->PhysDiskDevHandle);

	spin_lock_irqsave(&ioc->sas_device_lock, flags);
	sas_device = _scsih_sas_device_find_by_handle(ioc, handle);
	if (sas_device) {
		sas_device->volume_handle = 0;
		sas_device->volume_wwid = 0;
		clear_bit(handle, ioc->pd_handles);
		if (sas_device->starget && sas_device->starget->hostdata) {
			starget = sas_device->starget;
			sas_target_priv_data = starget->hostdata;
			sas_target_priv_data->flags &=
			    ~MPT_TARGET_FLAGS_RAID_COMPONENT;
#if defined(__VMKLNX__)
			sas_target_priv_data->configured = 0;
#endif
		}
	}
	spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
	if (!sas_device)
		return;

	/* exposing raid component */
	if (starget) {
		starget_for_each_device(starget, NULL, _scsih_reprobe_lun);
#if defined(__VMKLNX__)
		scsi_scan_target(&starget->dev, 0, starget->id, ~0, 1);
#endif
	}
}

/**
 * _scsih_sas_pd_hide - hide pd component from /dev/sdX
 * @ioc: per adapter object
 * @element: IR config element data
 * Context: user.
 *
 * Return nothing.
 */
static void
_scsih_sas_pd_hide(struct MPT2SAS_ADAPTER *ioc,
    Mpi2EventIrConfigElement_t *element)
{
	struct _sas_device *sas_device;
	struct scsi_target *starget = NULL;
	struct MPT2SAS_TARGET *sas_target_priv_data;
	unsigned long flags;
	u16 handle = le16_to_cpu(element->PhysDiskDevHandle);
	u16 volume_handle = 0;
	u64 volume_wwid = 0;

	mpt2sas_config_get_volume_handle(ioc, handle, &volume_handle);
	if (volume_handle)
		mpt2sas_config_get_volume_wwid(ioc, volume_handle,
		    &volume_wwid);

	spin_lock_irqsave(&ioc->sas_device_lock, flags);
	sas_device = _scsih_sas_device_find_by_handle(ioc, handle);
	if (sas_device) {
		set_bit(handle, ioc->pd_handles);
		if (sas_device->starget && sas_device->starget->hostdata) {
			starget = sas_device->starget;
			sas_target_priv_data = starget->hostdata;
			sas_target_priv_data->flags |=
			    MPT_TARGET_FLAGS_RAID_COMPONENT;
			sas_device->volume_handle = volume_handle;
			sas_device->volume_wwid = volume_wwid;
		}
	}
	spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
	if (!sas_device)
		return;

	/* hiding raid component */
	if (starget)
		starget_for_each_device(starget, (void *)1, _scsih_reprobe_lun);

}

/**
 * _scsih_sas_pd_delete - delete pd component
 * @ioc: per adapter object
 * @element: IR config element data
 * Context: user.
 *
 * Return nothing.
 */
static void
_scsih_sas_pd_delete(struct MPT2SAS_ADAPTER *ioc,
    Mpi2EventIrConfigElement_t *element)
{
	u16 handle = le16_to_cpu(element->PhysDiskDevHandle);

	_scsih_device_remove_by_handle(ioc, handle);
}

/**
 * _scsih_sas_pd_add - remove pd component
 * @ioc: per adapter object
 * @element: IR config element data
 * Context: user.
 *
 * Return nothing.
 */
static void
_scsih_sas_pd_add(struct MPT2SAS_ADAPTER *ioc,
    Mpi2EventIrConfigElement_t *element)
{
	struct _sas_device *sas_device;
	unsigned long flags;
	u16 handle = le16_to_cpu(element->PhysDiskDevHandle);
	Mpi2ConfigReply_t mpi_reply;
	Mpi2SasDevicePage0_t sas_device_pg0;
	u32 ioc_status;
	u64 sas_address;
	u16 parent_handle;

	set_bit(handle, ioc->pd_handles);

	spin_lock_irqsave(&ioc->sas_device_lock, flags);
	sas_device = _scsih_sas_device_find_by_handle(ioc, handle);
	spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
	if (sas_device)
		return;

	if ((mpt2sas_config_get_sas_device_pg0(ioc, &mpi_reply, &sas_device_pg0,
	    MPI2_SAS_DEVICE_PGAD_FORM_HANDLE, handle))) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		return;
	}

	ioc_status = le16_to_cpu(mpi_reply.IOCStatus) &
	    MPI2_IOCSTATUS_MASK;
	if (ioc_status != MPI2_IOCSTATUS_SUCCESS) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		return;
	}

	parent_handle = le16_to_cpu(sas_device_pg0.ParentDevHandle);
	if (!_scsih_get_sas_address(ioc, parent_handle, &sas_address))
		mpt2sas_transport_update_links(ioc, sas_address, handle,
		    sas_device_pg0.PhyNum, MPI2_SAS_NEG_LINK_RATE_1_5);

	_scsih_add_device(ioc, handle, 0, 1);
}

#ifdef CONFIG_SCSI_MPT2SAS_LOGGING
/**
 * _scsih_sas_ir_config_change_event_debug - debug for IR Config Change events
 * @ioc: per adapter object
 * @event_data: event data payload
 * Context: user.
 *
 * Return nothing.
 */
static void
_scsih_sas_ir_config_change_event_debug(struct MPT2SAS_ADAPTER *ioc,
    Mpi2EventDataIrConfigChangeList_t *event_data)
{
	Mpi2EventIrConfigElement_t *element;
	u8 element_type;
	int i;
	char *reason_str = NULL, *element_str = NULL;

	element = (Mpi2EventIrConfigElement_t *)&event_data->ConfigElement[0];

	printk(MPT2SAS_INFO_FMT "raid config change: (%s), elements(%d)\n",
	    ioc->name, (le32_to_cpu(event_data->Flags) &
	    MPI2_EVENT_IR_CHANGE_FLAGS_FOREIGN_CONFIG) ?
	    "foreign" : "native", event_data->NumElements);
	for (i = 0; i < event_data->NumElements; i++, element++) {
		switch (element->ReasonCode) {
		case MPI2_EVENT_IR_CHANGE_RC_ADDED:
			reason_str = "add";
			break;
		case MPI2_EVENT_IR_CHANGE_RC_REMOVED:
			reason_str = "remove";
			break;
		case MPI2_EVENT_IR_CHANGE_RC_NO_CHANGE:
			reason_str = "no change";
			break;
		case MPI2_EVENT_IR_CHANGE_RC_HIDE:
			reason_str = "hide";
			break;
		case MPI2_EVENT_IR_CHANGE_RC_UNHIDE:
			reason_str = "unhide";
			break;
		case MPI2_EVENT_IR_CHANGE_RC_VOLUME_CREATED:
			reason_str = "volume_created";
			break;
		case MPI2_EVENT_IR_CHANGE_RC_VOLUME_DELETED:
			reason_str = "volume_deleted";
			break;
		case MPI2_EVENT_IR_CHANGE_RC_PD_CREATED:
			reason_str = "pd_created";
			break;
		case MPI2_EVENT_IR_CHANGE_RC_PD_DELETED:
			reason_str = "pd_deleted";
			break;
		default:
			reason_str = "unknown reason";
			break;
		}
		element_type = le16_to_cpu(element->ElementFlags) &
		    MPI2_EVENT_IR_CHANGE_EFLAGS_ELEMENT_TYPE_MASK;
		switch (element_type) {
		case MPI2_EVENT_IR_CHANGE_EFLAGS_VOLUME_ELEMENT:
			element_str = "volume";
			break;
		case MPI2_EVENT_IR_CHANGE_EFLAGS_VOLPHYSDISK_ELEMENT:
			element_str = "phys disk";
			break;
		case MPI2_EVENT_IR_CHANGE_EFLAGS_HOTSPARE_ELEMENT:
			element_str = "hot spare";
			break;
		default:
			element_str = "unknown element";
			break;
		}
		printk(KERN_DEBUG "\t(%s:%s), vol handle(0x%04x), "
		    "pd handle(0x%04x), pd num(0x%02x)\n", element_str,
		    reason_str, le16_to_cpu(element->VolDevHandle),
		    le16_to_cpu(element->PhysDiskDevHandle),
		    element->PhysDiskNum);
	}
}
#endif

/**
 * _scsih_sas_ir_config_change_event - handle ir configuration change events
 * @ioc: per adapter object
 * @fw_event: The fw_event_work object
 * Context: user.
 *
 * Return nothing.
 */
static void
_scsih_sas_ir_config_change_event(struct MPT2SAS_ADAPTER *ioc,
    struct fw_event_work *fw_event)
{
	Mpi2EventIrConfigElement_t *element;
	int i;
	u8 foreign_config;
	Mpi2EventDataIrConfigChangeList_t *event_data = fw_event->event_data;

#ifdef CONFIG_SCSI_MPT2SAS_LOGGING
	if ((ioc->logging_level & MPT_DEBUG_EVENT_WORK_TASK)
	    && !ioc->hide_ir_msg)
		_scsih_sas_ir_config_change_event_debug(ioc, event_data);

#endif

	foreign_config = (le32_to_cpu(event_data->Flags) &
	    MPI2_EVENT_IR_CHANGE_FLAGS_FOREIGN_CONFIG) ? 1 : 0;

	element = (Mpi2EventIrConfigElement_t *)&event_data->ConfigElement[0];
	for (i = 0; i < event_data->NumElements; i++, element++) {

		switch (element->ReasonCode) {
		case MPI2_EVENT_IR_CHANGE_RC_VOLUME_CREATED:
		case MPI2_EVENT_IR_CHANGE_RC_ADDED:
			if (!foreign_config)
				_scsih_sas_volume_add(ioc, element);
			break;
		case MPI2_EVENT_IR_CHANGE_RC_VOLUME_DELETED:
		case MPI2_EVENT_IR_CHANGE_RC_REMOVED:
			if (!foreign_config)
				_scsih_sas_volume_delete(ioc,
				    le16_to_cpu(element->VolDevHandle));
			break;
		case MPI2_EVENT_IR_CHANGE_RC_PD_CREATED:
			if (!ioc->is_warpdrive)
				_scsih_sas_pd_hide(ioc, element);
			break;
		case MPI2_EVENT_IR_CHANGE_RC_PD_DELETED:
			if (!ioc->is_warpdrive)
				_scsih_sas_pd_expose(ioc, element);
			break;
		case MPI2_EVENT_IR_CHANGE_RC_HIDE:
			if (!ioc->is_warpdrive)
				_scsih_sas_pd_add(ioc, element);
			break;
		case MPI2_EVENT_IR_CHANGE_RC_UNHIDE:
			if (!ioc->is_warpdrive)
				_scsih_sas_pd_delete(ioc, element);
			break;
		}
	}
}

/**
 * _scsih_sas_ir_volume_event - IR volume event
 * @ioc: per adapter object
 * @fw_event: The fw_event_work object
 * Context: user.
 *
 * Return nothing.
 */
static void
_scsih_sas_ir_volume_event(struct MPT2SAS_ADAPTER *ioc,
    struct fw_event_work *fw_event)
{
	u64 wwid;
	unsigned long flags;
	struct _raid_device *raid_device;
	u16 handle;
	u32 state;
	int rc;
	Mpi2EventDataIrVolume_t *event_data = fw_event->event_data;
#if defined(__VMKLNX__)
	struct scsi_target *starget = NULL;
	struct MPT2SAS_TARGET *sas_target_priv_data = NULL;
#endif

	if (event_data->ReasonCode != MPI2_EVENT_IR_VOLUME_RC_STATE_CHANGED)
		return;

	handle = le16_to_cpu(event_data->VolDevHandle);
	state = le32_to_cpu(event_data->NewValue);
	if (!ioc->hide_ir_msg)
		dewtprintk(ioc, printk(MPT2SAS_INFO_FMT "%s: handle(0x%04x), "
		    "old(0x%08x), new(0x%08x)\n", ioc->name, __func__,  handle,
		    le32_to_cpu(event_data->PreviousValue), state));

	switch (state) {
	case MPI2_RAID_VOL_STATE_MISSING:
	case MPI2_RAID_VOL_STATE_FAILED:
#if defined(__VMKLNX__)
		spin_lock_irqsave(&ioc->raid_device_lock, flags);
		raid_device = _scsih_raid_device_find_by_handle(ioc, handle);
		spin_unlock_irqrestore(&ioc->raid_device_lock, flags);
		if (!raid_device)
			break;
		if (raid_device->starget) {

			sas_target_priv_data = raid_device->starget->hostdata;
			if (sas_target_priv_data) {
				sas_target_priv_data->deleted = 1;
			}
#if (ESX_VERS > ESX41_VERS)
			vmklnx_scsi_target_offline(&raid_device->starget->dev);
#endif
		}
		_scsih_raid_device_remove(ioc, raid_device);
#else
		_scsih_sas_volume_delete(ioc, handle);
#endif
		break;

	case MPI2_RAID_VOL_STATE_ONLINE:
	case MPI2_RAID_VOL_STATE_DEGRADED:
	case MPI2_RAID_VOL_STATE_OPTIMAL:

		spin_lock_irqsave(&ioc->raid_device_lock, flags);
		raid_device = _scsih_raid_device_find_by_handle(ioc, handle);
		spin_unlock_irqrestore(&ioc->raid_device_lock, flags);

		if (raid_device)
			break;

		mpt2sas_config_get_volume_wwid(ioc, handle, &wwid);
		if (!wwid) {
			printk(MPT2SAS_ERR_FMT
			    "failure at %s:%d/%s()!\n", ioc->name,
			    __FILE__, __LINE__, __func__);
			break;
		}

		raid_device = kzalloc(sizeof(struct _raid_device), GFP_KERNEL);
		if (!raid_device) {
			printk(MPT2SAS_ERR_FMT
			    "failure at %s:%d/%s()!\n", ioc->name,
			    __FILE__, __LINE__, __func__);
			break;
		}

#if defined(__VMKLNX__)
		mpt2sas_connect_devices(ioc, wwid, handle);
		starget = mpt2sas_scsih_starget_find_by_sas_address(ioc, wwid);
		if (starget) {
			/* A coming-back RAID */
			raid_device->starget = starget;
			raid_device->id = starget->id;
			sas_target_priv_data = starget->hostdata;
			if (sas_target_priv_data) {
				sas_target_priv_data->handle = handle;
			}
			scsi_target_unblock(&starget->dev);
		} else {
			raid_device->id = ioc->sas_id++;
		}
#else
		raid_device->id = ioc->sas_id++;
#endif
		raid_device->channel = RAID_CHANNEL;
		raid_device->handle = handle;
		raid_device->wwid = wwid;
		_scsih_raid_device_add(ioc, raid_device);
		rc = scsi_add_device(ioc->shost, RAID_CHANNEL,
		    raid_device->id, 0);
		if (rc)
			_scsih_raid_device_remove(ioc, raid_device);
		break;

	case MPI2_RAID_VOL_STATE_INITIALIZING:
	default:
		break;
	}
}

/**
 * _scsih_sas_ir_physical_disk_event - PD event
 * @ioc: per adapter object
 * @fw_event: The fw_event_work object
 * Context: user.
 *
 * Return nothing.
 */
static void
_scsih_sas_ir_physical_disk_event(struct MPT2SAS_ADAPTER *ioc,
    struct fw_event_work *fw_event)
{
	u16 handle, parent_handle;
	u32 state;
	struct _sas_device *sas_device;
	unsigned long flags;
	Mpi2ConfigReply_t mpi_reply;
	Mpi2SasDevicePage0_t sas_device_pg0;
	u32 ioc_status;
	Mpi2EventDataIrPhysicalDisk_t *event_data = fw_event->event_data;
	u64 sas_address;

	if (event_data->ReasonCode != MPI2_EVENT_IR_PHYSDISK_RC_STATE_CHANGED)
		return;

	handle = le16_to_cpu(event_data->PhysDiskDevHandle);
	state = le32_to_cpu(event_data->NewValue);

	if (!ioc->hide_ir_msg)
		dewtprintk(ioc, printk(MPT2SAS_INFO_FMT "%s: handle(0x%04x), "
		    "old(0x%08x), new(0x%08x)\n", ioc->name, __func__,  handle,
		    le32_to_cpu(event_data->PreviousValue), state));

	switch (state) {
	case MPI2_RAID_PD_STATE_ONLINE:
	case MPI2_RAID_PD_STATE_DEGRADED:
	case MPI2_RAID_PD_STATE_REBUILDING:
	case MPI2_RAID_PD_STATE_OPTIMAL:
	case MPI2_RAID_PD_STATE_HOT_SPARE:

		if (!ioc->is_warpdrive)
			set_bit(handle, ioc->pd_handles);

		spin_lock_irqsave(&ioc->sas_device_lock, flags);
		sas_device = _scsih_sas_device_find_by_handle(ioc, handle);
		spin_unlock_irqrestore(&ioc->sas_device_lock, flags);

		if (sas_device)
			return;

		if ((mpt2sas_config_get_sas_device_pg0(ioc, &mpi_reply,
		    &sas_device_pg0, MPI2_SAS_DEVICE_PGAD_FORM_HANDLE,
		    handle))) {
			printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
			    ioc->name, __FILE__, __LINE__, __func__);
			return;
		}

		ioc_status = le16_to_cpu(mpi_reply.IOCStatus) &
		    MPI2_IOCSTATUS_MASK;
		if (ioc_status != MPI2_IOCSTATUS_SUCCESS) {
			printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
			    ioc->name, __FILE__, __LINE__, __func__);
			return;
		}

		parent_handle = le16_to_cpu(sas_device_pg0.ParentDevHandle);
		if (!_scsih_get_sas_address(ioc, parent_handle, &sas_address))
			mpt2sas_transport_update_links(ioc, sas_address, handle,
			    sas_device_pg0.PhyNum, MPI2_SAS_NEG_LINK_RATE_1_5);

		_scsih_add_device(ioc, handle, 0, 1);

		break;

	case MPI2_RAID_PD_STATE_OFFLINE:
	case MPI2_RAID_PD_STATE_NOT_CONFIGURED:
	case MPI2_RAID_PD_STATE_NOT_COMPATIBLE:
	default:
		break;
	}
}

#ifdef CONFIG_SCSI_MPT2SAS_LOGGING
/**
 * _scsih_sas_ir_operation_status_event_debug - debug for IR op event
 * @ioc: per adapter object
 * @event_data: event data payload
 * Context: user.
 *
 * Return nothing.
 */
static void
_scsih_sas_ir_operation_status_event_debug(struct MPT2SAS_ADAPTER *ioc,
    Mpi2EventDataIrOperationStatus_t *event_data)
{
	char *reason_str = NULL;

	switch (event_data->RAIDOperation) {
	case MPI2_EVENT_IR_RAIDOP_RESYNC:
		reason_str = "resync";
		break;
	case MPI2_EVENT_IR_RAIDOP_ONLINE_CAP_EXPANSION:
		reason_str = "online capacity expansion";
		break;
	case MPI2_EVENT_IR_RAIDOP_CONSISTENCY_CHECK:
		reason_str = "consistency check";
		break;
	case MPI2_EVENT_IR_RAIDOP_BACKGROUND_INIT:
		reason_str = "background init";
		break;
	case MPI2_EVENT_IR_RAIDOP_MAKE_DATA_CONSISTENT:
		reason_str = "make data consistent";
		break;
	}

	if (!reason_str)
		return;

	printk(MPT2SAS_INFO_FMT "raid operational status: (%s)"
	    "\thandle(0x%04x), percent complete(%d)\n",
	    ioc->name, reason_str,
	    le16_to_cpu(event_data->VolDevHandle),
	    event_data->PercentComplete);
}
#endif

/**
 * _scsih_sas_ir_operation_status_event - handle RAID operation events
 * @ioc: per adapter object
 * @fw_event: The fw_event_work object
 * Context: user.
 *
 * Return nothing.
 */
static void
_scsih_sas_ir_operation_status_event(struct MPT2SAS_ADAPTER *ioc,
    struct fw_event_work *fw_event)
{
	Mpi2EventDataIrOperationStatus_t *event_data = fw_event->event_data;
#if !defined(__VMKLNX__)
	static struct _raid_device *raid_device;
	unsigned long flags;
	u16 handle;
#endif

#ifdef CONFIG_SCSI_MPT2SAS_LOGGING
	if ((ioc->logging_level & MPT_DEBUG_EVENT_WORK_TASK)
	    && !ioc->hide_ir_msg)
		_scsih_sas_ir_operation_status_event_debug(ioc,
		     event_data);
#endif

#if !defined(__VMKLNX__)
	/* code added for raid transport support */
	if (event_data->RAIDOperation == MPI2_EVENT_IR_RAIDOP_RESYNC) {
		spin_lock_irqsave(&ioc->raid_device_lock, flags);
		handle = le16_to_cpu(event_data->VolDevHandle);
		raid_device = _scsih_raid_device_find_by_handle(ioc, handle);
		if (raid_device)
			raid_device->percent_complete =
			    event_data->PercentComplete;
		spin_unlock_irqrestore(&ioc->raid_device_lock, flags);
	}
#endif
}

/**
 * _scsih_prep_device_scan - initialize parameters prior to device scan
 * @ioc: per adapter object
 *
 * Set the deleted flag prior to device scan.  If the device is found during
 * the scan, then we clear the deleted flag.
 */
static void
_scsih_prep_device_scan(struct MPT2SAS_ADAPTER *ioc)
{
	struct MPT2SAS_DEVICE *sas_device_priv_data;
	struct scsi_device *sdev;

	shost_for_each_device(sdev, ioc->shost) {
		sas_device_priv_data = sdev->hostdata;
		if (sas_device_priv_data && sas_device_priv_data->sas_target)
			sas_device_priv_data->sas_target->deleted = 1;
	}
}

/**
 * _scsih_mark_responding_sas_device - mark a sas_devices as responding
 * @ioc: per adapter object
 * @sas_address: sas address
 * @slot: enclosure slot id
 * @handle: device handle
 *
 * After host reset, find out whether devices are still responding.
 * Used in _scsih_remove_unresponsive_sas_devices.
 *
 * Return nothing.
 */
static void
_scsih_mark_responding_sas_device(struct MPT2SAS_ADAPTER *ioc, u64 sas_address,
    u16 slot, u16 handle)
{
	struct MPT2SAS_TARGET *sas_target_priv_data = NULL;
	struct scsi_target *starget;
	struct _sas_device *sas_device;
	unsigned long flags;

	spin_lock_irqsave(&ioc->sas_device_lock, flags);
	list_for_each_entry(sas_device, &ioc->sas_device_list, list) {
		if (sas_device->sas_address == sas_address &&
		    sas_device->slot == slot) {
			sas_device->responding = 1;
			starget = sas_device->starget;
			if (starget && starget->hostdata) {
				sas_target_priv_data = starget->hostdata;
				sas_target_priv_data->tm_busy = 0;
				sas_target_priv_data->deleted = 0;
			} else
				sas_target_priv_data = NULL;
			if (starget)
				starget_printk(KERN_INFO, starget,
				    "handle(0x%04x), sas_addr(0x%016llx), "
				    "enclosure logical id(0x%016llx), "
				    "slot(%d)\n", handle,
				    (unsigned long long)sas_device->sas_address,
				    (unsigned long long)
				    sas_device->enclosure_logical_id,
				    sas_device->slot);
			if (sas_device->handle == handle)
				goto out;
			printk(KERN_INFO "\thandle changed from(0x%04x)!!!\n",
			    sas_device->handle);
			sas_device->handle = handle;
			if (sas_target_priv_data)
				sas_target_priv_data->handle = handle;
			goto out;
		}
	}
 out:
	spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
}

/**
 * _scsih_search_responding_sas_devices -
 * @ioc: per adapter object
 *
 * After host reset, find out whether devices are still responding.
 * If not remove.
 *
 * Return nothing.
 */
static void
_scsih_search_responding_sas_devices(struct MPT2SAS_ADAPTER *ioc)
{
	Mpi2SasDevicePage0_t sas_device_pg0;
	Mpi2ConfigReply_t mpi_reply;
	u16 ioc_status;
	u16 handle;
	u32 device_info;

	printk(MPT2SAS_INFO_FMT "%s\n", ioc->name, __func__);

	if (list_empty(&ioc->sas_device_list))
		return;

	handle = 0xFFFF;
	while (!(mpt2sas_config_get_sas_device_pg0(ioc, &mpi_reply,
	    &sas_device_pg0, MPI2_SAS_DEVICE_PGAD_FORM_GET_NEXT_HANDLE,
	    handle))) {
		ioc_status = le16_to_cpu(mpi_reply.IOCStatus) &
		    MPI2_IOCSTATUS_MASK;
		if (ioc_status != MPI2_IOCSTATUS_SUCCESS) {
			printk(MPT2SAS_INFO_FMT "\tbreak from %s: "
			    "ioc_status(0x%04x), loginfo(0x%08x)\n",
			    ioc->name, __func__, ioc_status,
			    le32_to_cpu(mpi_reply.IOCLogInfo));
			break;
		}
		handle = le16_to_cpu(sas_device_pg0.DevHandle);
		device_info = le32_to_cpu(sas_device_pg0.DeviceInfo);
		if (!(_scsih_is_end_device(device_info)))
			continue;
		_scsih_mark_responding_sas_device(ioc,
		    le64_to_cpu(sas_device_pg0.SASAddress),
		    le16_to_cpu(sas_device_pg0.Slot), handle);
	}
}

/**
 * _scsih_mark_responding_raid_device - mark a raid_device as responding
 * @ioc: per adapter object
 * @wwid: world wide identifier for raid volume
 * @handle: device handle
 *
 * After host reset, find out whether devices are still responding.
 * Used in _scsih_remove_unresponsive_raid_devices.
 *
 * Return nothing.
 */
static void
_scsih_mark_responding_raid_device(struct MPT2SAS_ADAPTER *ioc, u64 wwid,
    u16 handle)
{
	struct MPT2SAS_TARGET *sas_target_priv_data;
	struct scsi_target *starget;
	struct _raid_device *raid_device;
	unsigned long flags;

	spin_lock_irqsave(&ioc->raid_device_lock, flags);
	list_for_each_entry(raid_device, &ioc->raid_device_list, list) {
		if (raid_device->wwid == wwid && raid_device->starget) {
			starget = raid_device->starget;
			if (starget && starget->hostdata) {
				sas_target_priv_data = starget->hostdata;
				sas_target_priv_data->deleted = 0;
			} else
				sas_target_priv_data = NULL;
			raid_device->responding = 1;
			spin_unlock_irqrestore(&ioc->raid_device_lock, flags);
			starget_printk(KERN_INFO, raid_device->starget,
			    "handle(0x%04x), wwid(0x%016llx)\n", handle,
			    (unsigned long long)raid_device->wwid);
			/*
			 * WARPDRIVE: The handles of the PDs might have changed
			 * across the host reset so re-initialize the
			 * required data for Direct IO
			 */
			_scsih_init_warpdrive_properties(ioc, raid_device);
			spin_lock_irqsave(&ioc->raid_device_lock, flags);
			if (raid_device->handle == handle) {
				spin_unlock_irqrestore(&ioc->raid_device_lock,
				    flags);
				return;
			}
			printk(KERN_INFO "\thandle changed from(0x%04x)!!!\n",
			    raid_device->handle);
			raid_device->handle = handle;
			if (sas_target_priv_data)
				sas_target_priv_data->handle = handle;
			spin_unlock_irqrestore(&ioc->raid_device_lock, flags);
			return;
		}
	}
	spin_unlock_irqrestore(&ioc->raid_device_lock, flags);
}

/**
 * _scsih_search_responding_raid_devices -
 * @ioc: per adapter object
 *
 * After host reset, find out whether devices are still responding.
 * If not remove.
 *
 * Return nothing.
 */
static void
_scsih_search_responding_raid_devices(struct MPT2SAS_ADAPTER *ioc)
{
	Mpi2RaidVolPage1_t volume_pg1;
#if !defined(__VMKLNX__)
	Mpi2RaidVolPage0_t volume_pg0;
#endif
	Mpi2RaidPhysDiskPage0_t pd_pg0;
	Mpi2ConfigReply_t mpi_reply;
	u16 ioc_status;
	u16 handle;
	u8 phys_disk_num;

	printk(MPT2SAS_INFO_FMT "%s\n", ioc->name, __func__);

	if (list_empty(&ioc->raid_device_list))
		return;

	handle = 0xFFFF;
	while (!(mpt2sas_config_get_raid_volume_pg1(ioc, &mpi_reply,
	    &volume_pg1, MPI2_RAID_VOLUME_PGAD_FORM_GET_NEXT_HANDLE, handle))) {
		ioc_status = le16_to_cpu(mpi_reply.IOCStatus) &
		    MPI2_IOCSTATUS_MASK;
		if (ioc_status != MPI2_IOCSTATUS_SUCCESS) {
			printk(MPT2SAS_INFO_FMT "\tbreak from %s: "
			    "ioc_status(0x%04x), loginfo(0x%08x)\n",
			    ioc->name, __func__, ioc_status,
			    le32_to_cpu(mpi_reply.IOCLogInfo));
			break;
		}
		handle = le16_to_cpu(volume_pg1.DevHandle);
#if !defined(__VMKLNX__)
		if (mpt2sas_config_get_raid_volume_pg0(ioc, &mpi_reply,
		    &volume_pg0, MPI2_RAID_VOLUME_PGAD_FORM_HANDLE, handle,
		     sizeof(Mpi2RaidVolPage0_t)))
			continue;

/* Make sure we get a valid reply.  If not, move on 
so we don't mark this as a responding device */
		ioc_status = le16_to_cpu(mpi_reply.IOCStatus) & 
		    MPI2_IOCSTATUS_MASK;
		if (ioc_status != MPI2_IOCSTATUS_SUCCESS) 
			continue;

		if (volume_pg0.VolumeState == MPI2_RAID_VOL_STATE_OPTIMAL ||
		    volume_pg0.VolumeState == MPI2_RAID_VOL_STATE_ONLINE ||
		    volume_pg0.VolumeState == MPI2_RAID_VOL_STATE_DEGRADED)
			_scsih_mark_responding_raid_device(ioc,
			    le64_to_cpu(volume_pg1.WWID), handle);
#else
		_scsih_mark_responding_raid_device(ioc,
		    le64_to_cpu(volume_pg1.WWID), handle);
#endif

	}

	/* refresh the pd_handles */
	if (!ioc->is_warpdrive) {
		phys_disk_num = 0xFF;
		memset(ioc->pd_handles, 0, ioc->pd_handles_sz);
		while (!(mpt2sas_config_get_phys_disk_pg0(ioc, &mpi_reply,
			    &pd_pg0, MPI2_PHYSDISK_PGAD_FORM_GET_NEXT_PHYSDISKNUM,
			    phys_disk_num))) {
			ioc_status = le16_to_cpu(mpi_reply.IOCStatus) &
			    MPI2_IOCSTATUS_MASK;
			if (ioc_status != MPI2_IOCSTATUS_SUCCESS) {
				printk(MPT2SAS_INFO_FMT "\tbreak from %s: "
				       "ioc_status(0x%04x), loginfo(0x%08x)\n",
				       ioc->name, __func__, ioc_status,
				       le32_to_cpu(mpi_reply.IOCLogInfo));
				break;
			}
			phys_disk_num = pd_pg0.PhysDiskNum;
			handle = le16_to_cpu(pd_pg0.DevHandle);
			set_bit(handle, ioc->pd_handles);
		}
	}
}

/**
 * _scsih_mark_responding_expander - mark a expander as responding
 * @ioc: per adapter object
 * @sas_address: sas address
 * @handle:
 *
 * After host reset, find out whether devices are still responding.
 * Used in _scsih_remove_unresponsive_expanders.
 *
 * Return nothing.
 */
static void
_scsih_mark_responding_expander(struct MPT2SAS_ADAPTER *ioc, u64 sas_address,
     u16 handle)
{
	struct _sas_node *sas_expander;
	unsigned long flags;
	int i;

	spin_lock_irqsave(&ioc->sas_node_lock, flags);
	list_for_each_entry(sas_expander, &ioc->sas_expander_list, list) {
		if (sas_expander->sas_address != sas_address)
			continue;
		sas_expander->responding = 1;
		if (sas_expander->handle == handle)
			goto out;
		printk(KERN_INFO "\texpander(0x%016llx): handle changed"
		    " from(0x%04x) to (0x%04x)!!!\n",
		    (unsigned long long)sas_expander->sas_address,
		    sas_expander->handle, handle);
		sas_expander->handle = handle;
		for (i = 0 ; i < sas_expander->num_phys ; i++)
			sas_expander->phy[i].handle = handle;
		goto out;
	}
 out:
	spin_unlock_irqrestore(&ioc->sas_node_lock, flags);
}

/**
 * _scsih_search_responding_expanders -
 * @ioc: per adapter object
 *
 * After host reset, find out whether devices are still responding.
 * If not remove.
 *
 * Return nothing.
 */
static void
_scsih_search_responding_expanders(struct MPT2SAS_ADAPTER *ioc)
{
	Mpi2ExpanderPage0_t expander_pg0;
	Mpi2ConfigReply_t mpi_reply;
	u16 ioc_status;
	u64 sas_address;
	u16 handle;

	printk(MPT2SAS_INFO_FMT "%s\n", ioc->name, __func__);

	if (list_empty(&ioc->sas_expander_list))
		return;

	handle = 0xFFFF;
	while (!(mpt2sas_config_get_expander_pg0(ioc, &mpi_reply, &expander_pg0,
	    MPI2_SAS_EXPAND_PGAD_FORM_GET_NEXT_HNDL, handle))) {

		ioc_status = le16_to_cpu(mpi_reply.IOCStatus) &
		    MPI2_IOCSTATUS_MASK;
		if (ioc_status != MPI2_IOCSTATUS_SUCCESS) {
			printk(MPT2SAS_INFO_FMT "\tbreak from %s: "
			    "ioc_status(0x%04x), loginfo(0x%08x)\n",
			    ioc->name, __func__, ioc_status,
			    le32_to_cpu(mpi_reply.IOCLogInfo));
			break;
		}

		handle = le16_to_cpu(expander_pg0.DevHandle);
		sas_address = le64_to_cpu(expander_pg0.SASAddress);
		printk(KERN_INFO "\texpander present: handle(0x%04x), "
		    "sas_addr(0x%016llx)\n", handle,
		    (unsigned long long)sas_address);
		_scsih_mark_responding_expander(ioc, sas_address, handle);
	}

}

/**
 * _scsih_remove_unresponsive_sas_devices - removing unresponding devices
 * @ioc: per adapter object
 *
 * Return nothing.
 */
static void
_scsih_remove_unresponsive_sas_devices(struct MPT2SAS_ADAPTER *ioc)
{
	struct _sas_device *sas_device, *sas_device_next;
	struct _sas_node *sas_expander;
	struct _raid_device *raid_device, *raid_device_next;
	unsigned long flags;

	printk(MPT2SAS_INFO_FMT "%s\n", ioc->name, __func__);

	list_for_each_entry_safe(sas_device, sas_device_next,
	    &ioc->sas_device_list, list) {
		if (sas_device->responding) {
			sas_device->responding = 0;
			continue;
		}
		if (sas_device->starget)
			starget_printk(KERN_INFO, sas_device->starget,
			    "removing: handle(0x%04x), sas_addr(0x%016llx), "
			    "enclosure logical id(0x%016llx), slot(%d)\n",
			    sas_device->handle,
			    (unsigned long long)sas_device->sas_address,
			    (unsigned long long)
			    sas_device->enclosure_logical_id,
			    sas_device->slot);
		spin_lock_irqsave(&ioc->sas_device_lock, flags);
		list_del(&sas_device->list);
		spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
		_scsih_remove_device(ioc, sas_device);
	}

	list_for_each_entry_safe(raid_device, raid_device_next,
	    &ioc->raid_device_list, list) {
		if (raid_device->responding) {
			raid_device->responding = 0;
			continue;
		}
		if (raid_device->starget) {
			starget_printk(KERN_INFO, raid_device->starget,
			    "removing: handle(0x%04x), wwid(0x%016llx)\n",
			      raid_device->handle,
			    (unsigned long long)raid_device->wwid);
			scsi_remove_target(&raid_device->starget->dev);
		}
		_scsih_raid_device_remove(ioc, raid_device);
	}

 retry_expander_search:
	sas_expander = NULL;
	list_for_each_entry(sas_expander, &ioc->sas_expander_list, list) {
		if (sas_expander->responding) {
			sas_expander->responding = 0;
			continue;
		}
		_scsih_expander_remove(ioc, sas_expander->sas_address);
		goto retry_expander_search;
	}

	/* unblock devices */
	_scsih_ublock_io_all_device(ioc);
}

/**
 * _scsih_hide_unhide_sas_devices - add/remove device to/from OS
 * @ioc: per adapter object
 *
 * Return nothing.
 */
static void
_scsih_hide_unhide_sas_devices(struct MPT2SAS_ADAPTER *ioc)
{
	struct _sas_device *sas_device, *sas_device_next;

	if (!ioc->is_warpdrive || ioc->mfg_pg10_hide_flag !=
	    MFG_PAGE10_HIDE_IF_VOL_PRESENT)
		return;

	if (ioc->hide_drives) {
		if (_scsih_get_num_volumes(ioc))
			return;
		ioc->hide_drives = 0;
		list_for_each_entry_safe(sas_device, sas_device_next,
		    &ioc->sas_device_list, list) {
			if (!mpt2sas_transport_port_add(ioc, sas_device->handle,
				sas_device->sas_address_parent)) {
				_scsih_sas_device_remove(ioc, sas_device);
			} else if (!sas_device->starget) {
				mpt2sas_transport_port_remove(ioc,
				    sas_device->sas_address,
				    sas_device->sas_address_parent);
				_scsih_sas_device_remove(ioc, sas_device);
			}
		}
	} else {
		if (!_scsih_get_num_volumes(ioc))
			return;
		ioc->hide_drives = 1;
		list_for_each_entry_safe(sas_device, sas_device_next,
		    &ioc->sas_device_list, list) {
			mpt2sas_transport_port_remove(ioc,
			    sas_device->sas_address,
			    sas_device->sas_address_parent);
		}
	}
}

/**
 * mpt2sas_scsih_reset_handler - reset callback handler (for scsih)
 * @ioc: per adapter object
 * @reset_phase: phase
 *
 * The handler for doing any required cleanup or initialization.
 *
 * The reset phase can be MPT2_IOC_PRE_RESET, MPT2_IOC_AFTER_RESET,
 * MPT2_IOC_DONE_RESET
 *
 * Return nothing.
 */
void
mpt2sas_scsih_reset_handler(struct MPT2SAS_ADAPTER *ioc, int reset_phase)
{
	struct _internal_qcmd *scsih_internal_qcmd, *scsih_internal_qcmd_next;
	unsigned long flags;

	switch (reset_phase) {
	case MPT2_IOC_PRE_RESET:
		dtmprintk(ioc, printk(MPT2SAS_INFO_FMT "%s: "
		    "MPT2_IOC_PRE_RESET\n", ioc->name, __func__));
		break;
	case MPT2_IOC_AFTER_RESET:
		dtmprintk(ioc, printk(MPT2SAS_INFO_FMT "%s: "
		    "MPT2_IOC_AFTER_RESET\n", ioc->name, __func__));
		if (ioc->scsih_cmds.status & MPT2_CMD_PENDING) {
			ioc->scsih_cmds.status |= MPT2_CMD_RESET;
			mpt2sas_base_free_smid(ioc, ioc->scsih_cmds.smid);
			complete(&ioc->scsih_cmds.done);
		}
		if (ioc->tm_cmds.status & MPT2_CMD_PENDING) {
			ioc->tm_cmds.status |= MPT2_CMD_RESET;
			mpt2sas_base_free_smid(ioc, ioc->tm_cmds.smid);
			complete(&ioc->tm_cmds.done);
		}

		spin_lock_irqsave(&ioc->scsih_q_internal_lock, flags);
		list_for_each_entry_safe(scsih_internal_qcmd, scsih_internal_qcmd_next, &ioc->scsih_q_intenal_cmds, list) {	
			scsih_internal_qcmd->status |= MPT2_CMD_RESET;
			mpt2sas_base_free_smid(ioc, scsih_internal_qcmd->smid);
		}
		spin_unlock_irqrestore(&ioc->scsih_q_internal_lock, flags);

#ifdef MPT2SAS_WD_DDIOCOUNT
		ioc->ddio_count = 0;
		ioc->ddio_err_count = 0;
#endif
		_scsih_fw_event_cleanup_queue(ioc);
		_scsih_flush_running_cmds(ioc);
		_scsih_queue_rescan(ioc);
		break;
	case MPT2_IOC_DONE_RESET:
		dtmprintk(ioc, printk(MPT2SAS_INFO_FMT "%s: "
		    "MPT2_IOC_DONE_RESET\n", ioc->name, __func__));
		_scsih_sas_host_refresh(ioc);
		_scsih_prep_device_scan(ioc);
		_scsih_search_responding_sas_devices(ioc);
		_scsih_search_responding_raid_devices(ioc);
		_scsih_search_responding_expanders(ioc);
		break;
	}
}

/**
 * _mpt2sas_fw_work - delayed task for processing firmware events
 * @ioc: per adapter object
 * @fw_event: The fw_event_work object
 * Context: user.
 *
 * Return nothing.
 */
static void
_mpt2sas_fw_work(struct MPT2SAS_ADAPTER *ioc, struct fw_event_work *fw_event)
{
	unsigned long flags;
	if (fw_event->event == MPT2SAS_RESCAN_AFTER_HOST_RESET) {
		_scsih_fw_event_free(ioc, fw_event);
		spin_lock_irqsave(&ioc->ioc_reset_in_progress_lock, flags);
		if (ioc->shost_recovery) {
			spin_unlock_irqrestore(&ioc->ioc_reset_in_progress_lock,
			    flags);
			init_completion(&ioc->shost_recovery_done);
			wait_for_completion(&ioc->shost_recovery_done);
		} else
			spin_unlock_irqrestore(&ioc->ioc_reset_in_progress_lock,
			    flags);
		while (scsi_host_in_recovery(ioc->shost))
			ssleep(1);
		_scsih_remove_unresponsive_sas_devices(ioc);
		_scsih_hide_unhide_sas_devices(ioc);
		return;
	}

	/* the queue is being flushed so ignore this event */
	if (ioc->remove_host || fw_event->cancel_pending_work ||
	    ioc->shost_recovery || ioc->pci_error_recovery) {
		_scsih_fw_event_free(ioc, fw_event);
		return;
	}

	switch (fw_event->event) {
#ifdef MPT2SAS_MULTIPATH
	case MPT2SAS_ABRT_TASK_SET:
		_scsih_abort_task_set(ioc, fw_event);
		break;
#endif
	case MPT2SAS_TURN_ON_FAULT_LED:
		_scsih_turn_on_pfa_led(ioc, fw_event->device_handle);
		break;
	case MPI2_EVENT_SAS_TOPOLOGY_CHANGE_LIST:
		if (_scsih_sas_topology_change_event(ioc, fw_event)) {
			_scsih_fw_event_requeue(ioc, fw_event, 1000);
			return;
		}
		break;
	case MPI2_EVENT_SAS_DEVICE_STATUS_CHANGE:
		_scsih_sas_device_status_change_event(ioc, fw_event);
		break;
	case MPI2_EVENT_SAS_DISCOVERY:
		_scsih_sas_discovery_event(ioc, fw_event);
		break;
	case MPI2_EVENT_SAS_BROADCAST_PRIMITIVE:
		_scsih_sas_broadcast_primative_event(ioc, fw_event);
		break;
	case MPI2_EVENT_SAS_ENCL_DEVICE_STATUS_CHANGE:
		_scsih_sas_enclosure_dev_status_change_event(ioc,
		    fw_event);
		break;
	case MPI2_EVENT_IR_CONFIGURATION_CHANGE_LIST:
		_scsih_sas_ir_config_change_event(ioc, fw_event);
		break;
	case MPI2_EVENT_IR_VOLUME:
		_scsih_sas_ir_volume_event(ioc, fw_event);
		break;
	case MPI2_EVENT_IR_PHYSICAL_DISK:
		_scsih_sas_ir_physical_disk_event(ioc, fw_event);
		break;
	case MPI2_EVENT_IR_OPERATION_STATUS:
		_scsih_sas_ir_operation_status_event(ioc, fw_event);
		break;
	}
	_scsih_fw_event_free(ioc, fw_event);
}

/**
 * _firmware_event_work and _firmware_event_work_delayed
 * @ioc: per adapter object
 * @work: The fw_event_work object
 * Context: user.
 *
 * wrappers for the work thread handling firmware events
 *
 * Return nothing.
 */

#if (ESX_VERS > ESX41_VERS) || (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,19))
static void
_firmware_event_work(struct work_struct *work)
{
	struct fw_event_work *fw_event = container_of(work,
	    struct fw_event_work, work);

	_mpt2sas_fw_work(fw_event->ioc, fw_event);
}
static void
_firmware_event_work_delayed(struct work_struct *work)
{
	struct fw_event_work *fw_event = container_of(work,
	    struct fw_event_work, delayed_work.work);

	_mpt2sas_fw_work(fw_event->ioc, fw_event);
}
#else
static void
_firmware_event_work(void *arg)
{
	struct fw_event_work *fw_event = (struct fw_event_work *)arg;

	_mpt2sas_fw_work(fw_event->ioc, fw_event);
}
#endif

/**
 * mpt2sas_scsih_event_callback - firmware event handler (called at ISR time)
 * @ioc: per adapter object
 * @msix_index: MSIX table index supplied by the OS
 * @reply: reply message frame(lower 32bit addr)
 * Context: interrupt.
 *
 * This function merely adds a new work task into ioc->firmware_event_thread.
 * The tasks are worked from _firmware_event_work in user context.
 *
 * Returns void.
 */
void
mpt2sas_scsih_event_callback(struct MPT2SAS_ADAPTER *ioc, u8 msix_index,
    u32 reply)
{
	struct fw_event_work *fw_event;
	Mpi2EventNotificationReply_t *mpi_reply;
	u16 event;
	u16 sz;

	/* events turned off due to host reset or driver unloading */
	if (ioc->remove_host || ioc->pci_error_recovery)
		return;

	mpi_reply = mpt2sas_base_get_reply_virt_addr(ioc, reply);

	if (unlikely(!mpi_reply)) {
		printk(MPT2SAS_ERR_FMT "mpi_reply not valid at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		return;
	}

	event = le16_to_cpu(mpi_reply->Event);

	switch (event) {
	/* handle these */
	case MPI2_EVENT_SAS_BROADCAST_PRIMITIVE:
	{
		Mpi2EventDataSasBroadcastPrimitive_t *baen_data =
		    (Mpi2EventDataSasBroadcastPrimitive_t *)
		    mpi_reply->EventData;

		if (baen_data->Primitive !=
		    MPI2_EVENT_PRIMITIVE_ASYNCHRONOUS_EVENT ||
		    ioc->broadcast_aen_busy)
			return;
		ioc->broadcast_aen_busy = 1;
		break;
	}

	case MPI2_EVENT_SAS_TOPOLOGY_CHANGE_LIST:
		_scsih_check_topo_delete_events(ioc,
		    (Mpi2EventDataSasTopologyChangeList_t *)
		    mpi_reply->EventData);
		break;
	case MPI2_EVENT_IR_CONFIGURATION_CHANGE_LIST:
		_scsih_check_ir_config_unhide_events(ioc,
		    (Mpi2EventDataIrConfigChangeList_t *)
		    mpi_reply->EventData);
		break;
	case MPI2_EVENT_IR_VOLUME:
		_scsih_check_volume_delete_events(ioc,
		    (Mpi2EventDataIrVolume_t *)
		    mpi_reply->EventData);
		break;
	case MPI2_EVENT_LOG_ENTRY_ADDED:
	{
		Mpi2EventDataLogEntryAdded_t *log_entry;
		u32 *log_code;

		if (!ioc->is_warpdrive)
			break;

		log_entry = (Mpi2EventDataLogEntryAdded_t *)
		    mpi_reply->EventData;
		log_code = (u32 *)log_entry->LogData;

		if (le16_to_cpu(log_entry->LogEntryQualifier)
		    != MPT2_WARPDRIVE_LOGENTRY)
			break;

		switch (le32_to_cpu(*log_code)) {
		case MPT2_WARPDRIVE_LC_SSDT:
			printk(MPT2SAS_WARN_FMT "WarpDrive Warning: "
			    "IO Throttling has occurred in the WarpDrive "
			    "subsystem. Check WarpDrive documentation for "
			    "additional details.\n", ioc->name);
			break;
		case MPT2_WARPDRIVE_LC_SSDLW:
			printk(MPT2SAS_WARN_FMT "WarpDrive Warning: "
			    "Program/Erase Cycles for the WarpDrive subsystem "
			    "in degraded range. Check WarpDrive documentation "
			    "for additional details.\n", ioc->name);
			break;
		case MPT2_WARPDRIVE_LC_SSDLF:
			printk(MPT2SAS_ERR_FMT "WarpDrive Fatal Error: "
			    "There are no Program/Erase Cycles for the "
			    "WarpDrive subsystem. The storage device will be "
			    "in read-only mode. Check WarpDrive documentation "
			    "for additional details.\n", ioc->name);
			break;
		case MPT2_WARPDRIVE_LC_BRMF:
			printk(MPT2SAS_ERR_FMT "WarpDrive Fatal Error: "
			    "The Backup Rail Monitor has failed on the "
			    "WarpDrive subsystem. Check WarpDrive "
			    "documentation for additional details.\n",
			    ioc->name);
			break;
		}
	}
	case MPI2_EVENT_SAS_DEVICE_STATUS_CHANGE:
	case MPI2_EVENT_IR_OPERATION_STATUS:
	case MPI2_EVENT_SAS_DISCOVERY:
	case MPI2_EVENT_SAS_ENCL_DEVICE_STATUS_CHANGE:
	case MPI2_EVENT_IR_PHYSICAL_DISK:
		break;

	case MPI2_EVENT_TEMP_THRESHOLD:
		_scsih_temp_threshold_events(ioc,
		    (Mpi2EventDataTemperature_t *)
		    mpi_reply->EventData);	
		break;

	default: /* ignore the rest */
		return;
	}

	fw_event = kzalloc(sizeof(struct fw_event_work), GFP_ATOMIC);
	if (!fw_event) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		return;
	}
	sz = le16_to_cpu(mpi_reply->EventDataLength) * 4;
	fw_event->event_data = kzalloc(sz, GFP_ATOMIC);
	if (!fw_event->event_data) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		kfree(fw_event);
		return;
	}

	if (event == MPI2_EVENT_SAS_TOPOLOGY_CHANGE_LIST) {
		Mpi2EventDataSasTopologyChangeList_t *topo_event_data =
		    (Mpi2EventDataSasTopologyChangeList_t *)
		    mpi_reply->EventData;
		fw_event->retries = kzalloc(topo_event_data->NumEntries,
		    GFP_ATOMIC);
		if (!fw_event->retries) {
			printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
			    ioc->name, __FILE__, __LINE__, __func__);
			kfree(fw_event->event_data);
			kfree(fw_event);
			return;
		}
	}

	memcpy(fw_event->event_data, mpi_reply->EventData, sz);
	fw_event->ioc = ioc;
	fw_event->VF_ID = mpi_reply->VF_ID;
	fw_event->VP_ID = mpi_reply->VP_ID;
	fw_event->event = event;
	_scsih_fw_event_add(ioc, fw_event);
	return;
}

/* shost template */
static struct scsi_host_template scsih_driver_template = {
	.module				= THIS_MODULE,
	.name				= "Fusion MPT SAS Host",
	.proc_name			= MPT2SAS_DRIVER_NAME,
	.queuecommand			= _scsih_qcmd,
	.target_alloc			= _scsih_target_alloc,
	.slave_alloc			= _scsih_slave_alloc,
	.slave_configure		= _scsih_slave_configure,
	.target_destroy			= _scsih_target_destroy,
	.slave_destroy			= _scsih_slave_destroy,
	.change_queue_depth 		= _scsih_change_queue_depth,
	.change_queue_type		= _scsih_change_queue_type,
	.eh_abort_handler		= _scsih_abort,
	.eh_device_reset_handler	= _scsih_dev_reset,
#if defined(__VMKLNX__)
	.eh_bus_reset_handler		= scsih_bus_reset,
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26))
	.eh_target_reset_handler	= _scsih_target_reset,
#endif
	.eh_host_reset_handler		= _scsih_host_reset,
	.bios_param			= _scsih_bios_param,
	.can_queue			= 1,
	.this_id			= -1,
	.sg_tablesize			= MPT2SAS_SG_DEPTH,
	.max_sectors			= 8192,
	.cmd_per_lun			= 7,
	.use_clustering			= ENABLE_CLUSTERING,
	.shost_attrs			= mpt2sas_host_attrs,
	.sdev_attrs			= mpt2sas_dev_attrs,
};

/**
 * _scsih_expander_node_remove - removing expander device from list.
 * @ioc: per adapter object
 * @sas_expander: the sas_device object
 * Context: Calling function should acquire ioc->sas_node_lock.
 *
 * Removing object and freeing associated memory from the
 * ioc->sas_expander_list.
 *
 * Return nothing.
 */
static void
_scsih_expander_node_remove(struct MPT2SAS_ADAPTER *ioc,
    struct _sas_node *sas_expander)
{
	struct _sas_port *mpt2sas_port;
	struct _sas_node *expander_sibling;
	unsigned long flags;

	if (!sas_expander)
		return;

	/* remove sibling ports attached to this expander */
 retry_device_search:
	list_for_each_entry(mpt2sas_port,
	   &sas_expander->sas_port_list, port_list) {
		if (ioc->shost_recovery)
			return;
		if (mpt2sas_port->remote_identify.device_type ==
		    SAS_END_DEVICE) {
			_scsih_device_remove_by_sas_address(ioc,
			    mpt2sas_port->remote_identify.sas_address);
			goto retry_device_search;
		}
	}

 retry_expander_search:
	list_for_each_entry(mpt2sas_port,
	   &sas_expander->sas_port_list, port_list) {

		if (mpt2sas_port->remote_identify.device_type ==
		    MPI2_SAS_DEVICE_INFO_EDGE_EXPANDER ||
		    mpt2sas_port->remote_identify.device_type ==
		    MPI2_SAS_DEVICE_INFO_FANOUT_EXPANDER) {

			spin_lock_irqsave(&ioc->sas_node_lock, flags);
			expander_sibling =
			    mpt2sas_scsih_expander_find_by_sas_address(
			    ioc, mpt2sas_port->remote_identify.sas_address);
			spin_unlock_irqrestore(&ioc->sas_node_lock, flags);
			if (!expander_sibling)
				continue;
			_scsih_expander_remove(ioc,
			    expander_sibling->sas_address);
			if (ioc->shost_recovery)
				return;
			goto retry_expander_search;
		}
	}

	mpt2sas_transport_port_remove(ioc, sas_expander->sas_address,
	    sas_expander->sas_address_parent);

	printk(MPT2SAS_INFO_FMT "expander_remove: handle"
	   "(0x%04x), sas_addr(0x%016llx)\n", ioc->name,
	    sas_expander->handle, (unsigned long long)
	    sas_expander->sas_address);

	list_del(&sas_expander->list);
	kfree(sas_expander->phy);
	kfree(sas_expander);
}

/**
 * _scsih_ir_shutdown - IR shutdown notification
 * @ioc: per adapter object
 *
 * Sending RAID Action to alert the Integrated RAID subsystem of the IOC that
 * the host system is shutting down.
 *
 * Return nothing.
 */
static void
_scsih_ir_shutdown(struct MPT2SAS_ADAPTER *ioc)
{
	Mpi2RaidActionRequest_t *mpi_request;
	Mpi2RaidActionReply_t *mpi_reply;
	u16 smid;

	/* is IR firmware build loaded ? */
	if (!ioc->ir_firmware)
		return;

	mutex_lock(&ioc->scsih_cmds.mutex);

	if (ioc->scsih_cmds.status != MPT2_CMD_NOT_USED) {
		printk(MPT2SAS_ERR_FMT "%s: scsih_cmd in use\n",
		    ioc->name, __func__);
		goto out;
	}
	ioc->scsih_cmds.status = MPT2_CMD_PENDING;

	smid = mpt2sas_base_get_smid(ioc, ioc->scsih_cb_idx);
	if (!smid) {
		printk(MPT2SAS_ERR_FMT "%s: failed obtaining a smid\n",
		    ioc->name, __func__);
		ioc->scsih_cmds.status = MPT2_CMD_NOT_USED;
		goto out;
	}

	mpi_request = mpt2sas_base_get_msg_frame(ioc, smid);
	ioc->scsih_cmds.smid = smid;
	memset(mpi_request, 0, sizeof(Mpi2RaidActionRequest_t));

	mpi_request->Function = MPI2_FUNCTION_RAID_ACTION;
	mpi_request->Action = MPI2_RAID_ACTION_SYSTEM_SHUTDOWN_INITIATED;

	if (!ioc->hide_ir_msg)
		printk(MPT2SAS_INFO_FMT "IR shutdown (sending)\n", ioc->name);
	init_completion(&ioc->scsih_cmds.done);
	mpt2sas_base_put_smid_default(ioc, smid);
	wait_for_completion_timeout(&ioc->scsih_cmds.done, 10*HZ);

	if (!(ioc->scsih_cmds.status & MPT2_CMD_COMPLETE)) {
		printk(MPT2SAS_ERR_FMT "%s: timeout\n",
		    ioc->name, __func__);
		goto out;
	}

	if (ioc->scsih_cmds.status & MPT2_CMD_REPLY_VALID) {
		mpi_reply = ioc->scsih_cmds.reply;

		if (!ioc->hide_ir_msg)
			printk(MPT2SAS_INFO_FMT "IR shutdown (complete): "
			    "ioc_status(0x%04x), loginfo(0x%08x)\n",
			    ioc->name, le16_to_cpu(mpi_reply->IOCStatus),
			    le32_to_cpu(mpi_reply->IOCLogInfo));
	}

 out:
	ioc->scsih_cmds.status = MPT2_CMD_NOT_USED;
	mutex_unlock(&ioc->scsih_cmds.mutex);
}


/**
 * _scsih_remove - detach and remove add host
 * @pdev: PCI device struct
 *
 * Routine called when unloading the driver.
 * Return nothing.
 */
static void __devexit
_scsih_remove(struct pci_dev *pdev)
{
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct MPT2SAS_ADAPTER *ioc = shost_private(shost);
	struct _sas_port *mpt2sas_port;
	struct _sas_node *expander_sibling;
	struct _raid_device *raid_device, *next;
	struct MPT2SAS_TARGET *sas_target_priv_data;
	struct workqueue_struct	*wq;
	unsigned long flags;

	ioc->remove_host = 1;
	_scsih_fw_event_cleanup_queue(ioc);

	spin_lock_irqsave(&ioc->fw_event_lock, flags);
	wq = ioc->firmware_event_thread;
	ioc->firmware_event_thread = NULL;
	spin_unlock_irqrestore(&ioc->fw_event_lock, flags);
	if (wq)
		destroy_workqueue(wq);

	_scsih_ir_shutdown(ioc);
	_scsih_ssu_to_sata_devices(ioc);
	/* release all the volumes */
	list_for_each_entry_safe(raid_device, next, &ioc->raid_device_list,
	    list) {
		if (raid_device->starget) {
			sas_target_priv_data =
			    raid_device->starget->hostdata;
			sas_target_priv_data->deleted = 1;
			scsi_remove_target(&raid_device->starget->dev);
		}
		printk(MPT2SAS_INFO_FMT "removing handle(0x%04x), wwid"
		    "(0x%016llx)\n", ioc->name,  raid_device->handle,
		    (unsigned long long) raid_device->wwid);
		_scsih_raid_device_remove(ioc, raid_device);
	}

	/* free ports attached to the sas_host */
 retry_again:
	list_for_each_entry(mpt2sas_port,
	   &ioc->sas_hba.sas_port_list, port_list) {
		if (mpt2sas_port->remote_identify.device_type ==
		    SAS_END_DEVICE) {
			_scsih_device_remove_by_sas_address(ioc,
			    mpt2sas_port->remote_identify.sas_address);
			goto retry_again;
		} else {
			expander_sibling =
			    mpt2sas_scsih_expander_find_by_sas_address(ioc,
			    mpt2sas_port->remote_identify.sas_address);
			if (expander_sibling) {
				_scsih_expander_remove(ioc,
				    expander_sibling->sas_address);
				goto retry_again;
			}
		}
	}

	/* free phys attached to the sas_host */
	if (ioc->sas_hba.num_phys) {
		kfree(ioc->sas_hba.phy);
		ioc->sas_hba.phy = NULL;
		ioc->sas_hba.num_phys = 0;
	}

	sas_remove_host(shost);
	mpt2sas_base_detach(ioc);
	list_del(&ioc->list);
	scsi_remove_host(shost);
	scsi_host_put(shost);
}


/**
 * _scsih_shutdown - routine call during system shutdown
 * @pdev: PCI device struct
 *
 * Return nothing.
 */
static void
_scsih_shutdown(struct pci_dev *pdev)
{
	_scsih_remove(pdev);
}

/**
 * _scsih_probe_boot_devices - reports 1st device
 * @ioc: per adapter object
 *
 * If specified in bios page 2, this routine reports the 1st
 * device scsi-ml or sas transport for persistent boot device
 * purposes.  Please refer to function _scsih_determine_boot_device()
 */
static void
_scsih_probe_boot_devices(struct MPT2SAS_ADAPTER *ioc)
{
	u8 is_raid;
	void *device;
	struct _sas_device *sas_device;
	struct _raid_device *raid_device;
	u16 handle;
	u64 sas_address_parent;
	u64 sas_address;
	unsigned long flags;
	int rc;

	device = NULL;
	if (ioc->req_boot_device.device) {
		device =  ioc->req_boot_device.device;
		is_raid = ioc->req_boot_device.is_raid;
	} else if (ioc->req_alt_boot_device.device) {
		device =  ioc->req_alt_boot_device.device;
		is_raid = ioc->req_alt_boot_device.is_raid;
	} else if (ioc->current_boot_device.device) {
		device =  ioc->current_boot_device.device;
		is_raid = ioc->current_boot_device.is_raid;
	}

	if (!device)
		return;

	if (is_raid) {
		raid_device = device;
		rc = scsi_add_device(ioc->shost, RAID_CHANNEL,
		    raid_device->id, 0);
		if (rc)
			_scsih_raid_device_remove(ioc, raid_device);
	} else {
		spin_lock_irqsave(&ioc->sas_device_lock, flags);
		sas_device = device;
		handle = sas_device->handle;
		sas_address_parent = sas_device->sas_address_parent;
		sas_address = sas_device->sas_address;
		list_move_tail(&sas_device->list, &ioc->sas_device_list);
		spin_unlock_irqrestore(&ioc->sas_device_lock, flags);

		if (ioc->hide_drives)
			return;
		if (!mpt2sas_transport_port_add(ioc, handle,
		    sas_address_parent)) {
			_scsih_sas_device_remove(ioc, sas_device);
		} else if (!sas_device->starget) {
			mpt2sas_transport_port_remove(ioc, sas_address,
			    sas_address_parent);
			_scsih_sas_device_remove(ioc, sas_device);
		}
	}
}

/**
 * _scsih_probe_raid - reporting raid volumes to scsi-ml
 * @ioc: per adapter object
 *
 * Called during initial loading of the driver.
 */
static void
_scsih_probe_raid(struct MPT2SAS_ADAPTER *ioc)
{
	struct _raid_device *raid_device, *raid_next;
	int rc;

	list_for_each_entry_safe(raid_device, raid_next,
	    &ioc->raid_device_list, list) {
		if (raid_device->starget)
			continue;
		rc = scsi_add_device(ioc->shost, RAID_CHANNEL,
		    raid_device->id, 0);
		if (rc)
			_scsih_raid_device_remove(ioc, raid_device);
	}
}

/**
 * _scsih_probe_sas - reporting sas devices to sas transport
 * @ioc: per adapter object
 *
 * Called during initial loading of the driver.
 */
static void
_scsih_probe_sas(struct MPT2SAS_ADAPTER *ioc)
{
	struct _sas_device *sas_device, *next;
	unsigned long flags;

	/* SAS Device List */
	list_for_each_entry_safe(sas_device, next, &ioc->sas_device_init_list,
	    list) {

		if (ioc->hide_drives) {
			spin_lock_irqsave(&ioc->sas_device_lock, flags);
			list_move_tail(&sas_device->list,
			    &ioc->sas_device_list);
			spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
			continue;
		}

		if (!mpt2sas_transport_port_add(ioc, sas_device->handle,
		    sas_device->sas_address_parent)) {
			if (!list_empty(&sas_device->list)) {
				list_del(&sas_device->list);
			}
			put_sas_device(sas_device);
			continue;
		} else if (!sas_device->starget) {
			mpt2sas_transport_port_remove(ioc,
			    sas_device->sas_address,
			    sas_device->sas_address_parent);
			if (!list_empty(&sas_device->list)) {
				list_del(&sas_device->list);
			}
			put_sas_device(sas_device);
			continue;
		}

		spin_lock_irqsave(&ioc->sas_device_lock, flags);
		list_move_tail(&sas_device->list, &ioc->sas_device_list);
		spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
	}
}

/**
 * _scsih_probe_devices - probing for devices
 * @ioc: per adapter object
 *
 * Called during initial loading of the driver.
 */
static void
_scsih_probe_devices(struct MPT2SAS_ADAPTER *ioc)
{
	u16 volume_mapping_flags =
	    le16_to_cpu(ioc->ioc_pg8.IRVolumeMappingFlags) &
	    MPI2_IOCPAGE8_IRFLAGS_MASK_VOLUME_MAPPING_MODE;

	if (!(ioc->facts.ProtocolFlags & MPI2_IOCFACTS_PROTOCOL_SCSI_INITIATOR))
		return;  /* return when IOC doesn't support initiator mode */

	_scsih_probe_boot_devices(ioc);

	if (ioc->ir_firmware) {
		if ((volume_mapping_flags &
		     MPI2_IOCPAGE8_IRFLAGS_HIGH_VOLUME_MAPPING)) {
			_scsih_probe_sas(ioc);
			_scsih_probe_raid(ioc);
		} else {
			_scsih_probe_raid(ioc);
			_scsih_probe_sas(ioc);
		}
	} else
		_scsih_probe_sas(ioc);
}

/**
 * _scsih_probe - attach and add scsi host
 * @pdev: PCI device struct
 * @id: pci device id
 *
 * Returns 0 success, anything else error.
 */
static int
_scsih_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct MPT2SAS_ADAPTER *ioc;
	struct Scsi_Host *shost;

	shost = scsi_host_alloc(&scsih_driver_template,
	    sizeof(struct MPT2SAS_ADAPTER));
	if (!shost)
		return -ENODEV;

	/* init local params */
	ioc = shost_private(shost);
	memset(ioc, 0, sizeof(struct MPT2SAS_ADAPTER));
	INIT_LIST_HEAD(&ioc->list);
	list_add_tail(&ioc->list, &mpt2sas_ioc_list);
	ioc->shost = shost;
	ioc->id = mpt_ids++;
	sprintf(ioc->name, "%s%d", MPT2SAS_DRIVER_NAME, ioc->id);
	if (id->device == MPI2_MFGPAGE_DEVID_SSS6200) {
		ioc->is_warpdrive = 1;
		ioc->hide_ir_msg = 1;
	} else
		ioc->mfg_pg10_hide_flag = MFG_PAGE10_EXPOSE_ALL_DISKS;
	ioc->pdev = pdev;
	ioc->scsi_io_cb_idx = scsi_io_cb_idx;
	ioc->tm_cb_idx = tm_cb_idx;
	ioc->ctl_cb_idx = ctl_cb_idx;
	ioc->ctl_tm_cb_idx = ctl_tm_cb_idx;
	ioc->ctl_diag_cb_idx = ctl_diag_cb_idx;
	ioc->base_cb_idx = base_cb_idx;
	ioc->transport_cb_idx = transport_cb_idx;
	ioc->scsih_cb_idx = scsih_cb_idx;
	ioc->scsih_q_cb_idx= scsih_q_cb_idx;
	ioc->config_cb_idx = config_cb_idx;
	ioc->tm_tr_cb_idx = tm_tr_cb_idx;
	ioc->tm_tr_volume_cb_idx = tm_tr_volume_cb_idx;
	ioc->tm_tr_internal_cb_idx = tm_tr_internal_cb_idx;
#ifdef MPT2SAS_MULTIPATH
	ioc->tm_tr_mp_cb_idx = tm_tr_mp_cb_idx;
#endif
	ioc->tm_sas_control_cb_idx = tm_sas_control_cb_idx;
	ioc->logging_level = logging_level;
	/* misc semaphores and spin locks */
	mutex_init(&ioc->reset_in_progress_mutex);
	spin_lock_init(&ioc->ioc_reset_in_progress_lock);
	spin_lock_init(&ioc->scsi_lookup_lock);
	spin_lock_init(&ioc->sas_device_lock);
	spin_lock_init(&ioc->sas_node_lock);
	spin_lock_init(&ioc->fw_event_lock);
	spin_lock_init(&ioc->raid_device_lock);
	spin_lock_init(&ioc->scsih_q_internal_lock);
#if defined(__VMKLNX__)
	spin_lock_init(&ioc->target_list_lock);
	INIT_LIST_HEAD(&ioc->target_list);
	printk("%s: setting logging_level(0x%08x)\n", ioc->name, logging_level);
#endif

	INIT_LIST_HEAD(&ioc->sas_device_list);
	INIT_LIST_HEAD(&ioc->sas_device_init_list);
	INIT_LIST_HEAD(&ioc->sas_expander_list);
	INIT_LIST_HEAD(&ioc->fw_event_list);
	INIT_LIST_HEAD(&ioc->raid_device_list);
	INIT_LIST_HEAD(&ioc->sas_hba.sas_port_list);
	INIT_LIST_HEAD(&ioc->delayed_tr_list);
	INIT_LIST_HEAD(&ioc->delayed_tr_volume_list);
	INIT_LIST_HEAD(&ioc->delayed_internal_tm_list);
	INIT_LIST_HEAD(&ioc->scsih_q_intenal_cmds);
#ifdef MPT2SAS_MULTIPATH
	INIT_LIST_HEAD(&ioc->delayed_tr_mp_list);
#endif

	init_completion(&ioc->base_cmds.done);
	init_completion(&ioc->transport_cmds.done);
	init_completion(&ioc->scsih_cmds.done);
	init_completion(&ioc->tm_cmds.done);
	init_completion(&ioc->ctl_cmds.done);
	init_completion(&ioc->config_cmds.done);
	init_completion(&ioc->shost_recovery_done);

	/* init shost parameters */
	shost->max_cmd_len = 32;
	shost->max_lun = max_lun;
	shost->transportt = mpt2sas_transport_template;
	shost->unique_id = ioc->id;

	if (max_sectors != -1) {
		if (max_sectors < 64) {
			printk(MPT2SAS_WARN_FMT "Invalid value %d passed "
			    "for max_sectors, range is 64 to 8192. Assigning "
			    "value of 64.\n", ioc->name, max_sectors);
			shost->max_sectors = 64;
		} else if (max_sectors > 8192) {
			printk(MPT2SAS_WARN_FMT "Invalid value %d passed "
			    "for max_sectors, range is 64 to 8192. Assigning "
			    "default value of 8192.\n", ioc->name, max_sectors);
			shost->max_sectors = 8192;
		} else {
			max_sectors &= 0xFFFE;
			printk(MPT2SAS_INFO_FMT "The max_sectors value is "
			    "set to %d\n", ioc->name, max_sectors);
			shost->max_sectors = max_sectors;
		}
	}

#if !defined(__VMKLNX__)
	if ((scsi_add_host(shost, &pdev->dev))) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		list_del(&ioc->list);
		goto out_add_shost_fail;
	}
#endif

	ioc->disable_eedp_support = disable_eedp;
	if (!ioc->disable_eedp_support) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27))
		scsi_host_set_prot(shost, SHOST_DIF_TYPE1_PROTECTION
		    | SHOST_DIF_TYPE2_PROTECTION | SHOST_DIF_TYPE3_PROTECTION);
		scsi_host_set_guard(shost, SHOST_DIX_GUARD_CRC);
#endif
	}

	/* event thread */
	snprintf(ioc->firmware_event_name, sizeof(ioc->firmware_event_name),
	    "fw_event%d", ioc->id);
	ioc->firmware_event_thread = create_singlethread_workqueue(
	    ioc->firmware_event_name);
	if (!ioc->firmware_event_thread) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		goto out_thread_fail;
	}

	ioc->wait_for_port_enable_to_complete = 1;
	if ((mpt2sas_base_attach(ioc))) {
		printk(MPT2SAS_ERR_FMT "failure at %s:%d/%s()!\n",
		    ioc->name, __FILE__, __LINE__, __func__);
		goto out_attach_fail;
	}

	ioc->wait_for_port_enable_to_complete = 0;

	if (ioc->is_warpdrive) {
		if (ioc->mfg_pg10_hide_flag ==  MFG_PAGE10_EXPOSE_ALL_DISKS)
			ioc->hide_drives = 0;
		else if (ioc->mfg_pg10_hide_flag ==  MFG_PAGE10_HIDE_ALL_DISKS)
			ioc->hide_drives = 1;
		else {
			if (_scsih_get_num_volumes(ioc))
				ioc->hide_drives = 1;
			else
				ioc->hide_drives = 0;
		}
	} else
		ioc->hide_drives = 0;

	_scsih_probe_devices(ioc);
	return 0;

 out_attach_fail:
	destroy_workqueue(ioc->firmware_event_thread);
 out_thread_fail:
	list_del(&ioc->list);
#if !defined(__VMKLNX__)
	scsi_remove_host(shost);
 out_add_shost_fail:
#endif
	return -ENODEV;
}

#ifdef CONFIG_PM
/**
 * _scsih_suspend - power management suspend main entry point
 * @pdev: PCI device struct
 * @state: PM state change to (usually PCI_D3)
 *
 * Returns 0 success, anything else error.
 */
static int
_scsih_suspend(struct pci_dev *pdev, pm_message_t state)
{
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct MPT2SAS_ADAPTER *ioc = shost_private(shost);
	pci_power_t device_state;

	mpt2sas_base_stop_watchdog(ioc);
	flush_scheduled_work();
	scsi_block_requests(shost);
	_scsih_ir_shutdown(ioc);
	_scsih_ssu_to_sata_devices(ioc);
	device_state = pci_choose_state(pdev, state);
	printk(MPT2SAS_INFO_FMT "pdev=0x%p, slot=%s, entering "
	    "operating state [D%d]\n", ioc->name, pdev,
	    pci_name(pdev), device_state);

	pci_save_state(pdev);
	mpt2sas_base_free_resources(ioc);
	pci_set_power_state(pdev, device_state);
	return 0;
}

/**
 * _scsih_resume - power management resume main entry point
 * @pdev: PCI device struct
 *
 * Returns 0 success, anything else error.
 */
static int
_scsih_resume(struct pci_dev *pdev)
{
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct MPT2SAS_ADAPTER *ioc = shost_private(shost);
	pci_power_t device_state = pdev->current_state;
	int r;

	printk(MPT2SAS_INFO_FMT "pdev=0x%p, slot=%s, previous "
	    "operating state [D%d]\n", ioc->name, pdev,
	    pci_name(pdev), device_state);

	pci_set_power_state(pdev, PCI_D0);
	pci_enable_wake(pdev, PCI_D0, 0);
	pci_restore_state(pdev);
	ioc->pdev = pdev;
	r = mpt2sas_base_map_resources(ioc);
	if (r)
		return r;

	mpt2sas_base_hard_reset_handler(ioc, CAN_SLEEP, SOFT_RESET);
	scsi_unblock_requests(shost);
	mpt2sas_base_start_watchdog(ioc);
	return 0;
}
#endif /* CONFIG_PM */

#if !defined(__VMKLNX__)
/**
 * _scsih_pci_error_detected - Called when a PCI error is detected.
 * @pdev: PCI device struct
 * @state: PCI channel state
 *
 * Description: Called when a PCI error is detected.
 *
 * Return value:
 *      PCI_ERS_RESULT_NEED_RESET or PCI_ERS_RESULT_DISCONNECT
 */
static pci_ers_result_t
_scsih_pci_error_detected(struct pci_dev *pdev, pci_channel_state_t state)
{
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct MPT2SAS_ADAPTER *ioc = shost_private(shost);

	printk(MPT2SAS_INFO_FMT "PCI error: detected callback, state(%d)!!\n",
	    ioc->name, state);

	switch (state) {
	case pci_channel_io_normal:
		ioc->pci_error_recovery = 1;
		return PCI_ERS_RESULT_CAN_RECOVER;
	case pci_channel_io_frozen:
		/* Fatal error, prepare for slot reset */
		ioc->pci_error_recovery = 1;
		scsi_block_requests(ioc->shost);
		mpt2sas_base_stop_watchdog(ioc);
		mpt2sas_base_free_resources(ioc);
		return PCI_ERS_RESULT_NEED_RESET;
	case pci_channel_io_perm_failure:
		/* Permanent error, prepare for device removal */
		ioc->pci_error_recovery = 1;
		mpt2sas_base_stop_watchdog(ioc);
		_scsih_flush_running_cmds(ioc);
		return PCI_ERS_RESULT_DISCONNECT;
	}
	return PCI_ERS_RESULT_NEED_RESET;
}

/**
 * _scsih_pci_slot_reset - Called when PCI slot has been reset.
 * @pdev: PCI device struct
 *
 * Description: This routine is called by the pci error recovery
 * code after the PCI slot has been reset, just before we
 * should resume normal operations.
 */
static pci_ers_result_t
_scsih_pci_slot_reset(struct pci_dev *pdev)
{
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct MPT2SAS_ADAPTER *ioc = shost_private(shost);
	int rc;

	printk(MPT2SAS_INFO_FMT "PCI error: slot reset callback!!\n",
	    ioc->name);

	ioc->pci_error_recovery = 0;
	ioc->pdev = pdev;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19))
	pci_restore_state(pdev);
#endif
	rc = mpt2sas_base_map_resources(ioc);
	if (rc) {
		ioc->pci_error_recovery = 1;
			return PCI_ERS_RESULT_DISCONNECT;
    }

	rc = mpt2sas_base_hard_reset_handler(ioc, CAN_SLEEP,
	    FORCE_BIG_HAMMER);

	printk(MPT2SAS_WARN_FMT "hard reset: %s\n", ioc->name,
	    (rc == 0) ? "success" : "failed");

	if (!rc)
		return PCI_ERS_RESULT_RECOVERED;
	else {
		ioc->pci_error_recovery = 1;
		return PCI_ERS_RESULT_DISCONNECT;
	}
}

/**
 * _scsih_pci_resume() - resume normal ops after PCI reset
 * @pdev: pointer to PCI device
 *
 * Called when the error recovery driver tells us that its
 * OK to resume normal operation. Use completion to allow
 * halted scsi ops to resume.
 */
static void
_scsih_pci_resume(struct pci_dev *pdev)
{
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct MPT2SAS_ADAPTER *ioc = shost_private(shost);

	printk(MPT2SAS_INFO_FMT "PCI error: resume callback!!\n", ioc->name);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19))
	pci_cleanup_aer_uncorrect_error_status(pdev);
#endif
	mpt2sas_base_start_watchdog(ioc);
	scsi_unblock_requests(ioc->shost);
}

/**
 * _scsih_pci_mmio_enabled - Enable MMIO and dump debug registers
 * @pdev: pointer to PCI device
 */
static pci_ers_result_t
_scsih_pci_mmio_enabled(struct pci_dev *pdev)
{
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct MPT2SAS_ADAPTER *ioc = shost_private(shost);

	printk(MPT2SAS_INFO_FMT "PCI error: mmio enabled callback!!\n",
	    ioc->name);

	/* Request a slot reset. */
	return PCI_ERS_RESULT_NEED_RESET;
}

/* raid transport support */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27))
static struct raid_function_template mpt2sas_raid_functions = {
	.cookie		= &scsih_driver_template,
	.is_raid	= _scsih_is_raid,
	.get_resync	= _scsih_get_resync,
	.get_state	= _scsih_get_state,
};
#endif

static struct pci_error_handlers _scsih_err_handler = {
	.error_detected = _scsih_pci_error_detected,
	.mmio_enabled = _scsih_pci_mmio_enabled,
	.slot_reset =	_scsih_pci_slot_reset,
	.resume = 	_scsih_pci_resume,
};
#endif

static struct pci_driver scsih_driver = {
	.name		= MPT2SAS_DRIVER_NAME,
	.id_table	= scsih_pci_table,
	.probe		= _scsih_probe,
	.remove		= __devexit_p(_scsih_remove),
	.shutdown	= _scsih_shutdown,
#if !defined(__VMKLNX__)
	.err_handler	= &_scsih_err_handler,
#endif
#ifdef CONFIG_PM
	.suspend	= _scsih_suspend,
	.resume		= _scsih_resume,
#endif
};


/**
 * _scsih_init - main entry point for this driver.
 *
 * Returns 0 success, anything else error.
 */
static int __init
_scsih_init(void)
{
	int error;

	mpt_ids = 0;

	printk(KERN_INFO "%s version %s loaded\n", MPT2SAS_DRIVER_NAME,
	    MPT2SAS_DRIVER_VERSION);

	mpt2sas_transport_template =
	    sas_attach_transport(&mpt2sas_transport_functions);
	if (!mpt2sas_transport_template)
		return -ENODEV;

#if !defined(__VMKLNX__)
/* raid transport support */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27))
	mpt2sas_raid_template = raid_class_attach(&mpt2sas_raid_functions);
	if (!mpt2sas_raid_template) {
		sas_release_transport(mpt2sas_transport_template);
		return -ENODEV;
	}
#endif
#endif

	mpt2sas_base_initialize_callback_handler();

	 /* queuecommand callback hander */
	scsi_io_cb_idx = mpt2sas_base_register_callback_handler(_scsih_io_done);

	/* task managment callback handler */
	tm_cb_idx = mpt2sas_base_register_callback_handler(_scsih_tm_done);

	/* base internal commands callback handler */
	base_cb_idx = mpt2sas_base_register_callback_handler(mpt2sas_base_done);

	/* transport internal commands callback handler */
	transport_cb_idx = mpt2sas_base_register_callback_handler(
	    mpt2sas_transport_done);

	/* scsih internal commands callback handler */
	scsih_cb_idx = mpt2sas_base_register_callback_handler(_scsih_done);

	/* scsih internal queue commands callback handler */
	scsih_q_cb_idx = mpt2sas_base_register_callback_handler(_scsih_q_internal_done);

	/* configuration page API internal commands callback handler */
	config_cb_idx = mpt2sas_base_register_callback_handler(
	    mpt2sas_config_done);

	/* ctl module callback handler */
	ctl_cb_idx = mpt2sas_base_register_callback_handler(mpt2sas_ctl_done);
	ctl_tm_cb_idx = mpt2sas_base_register_callback_handler
	    (mpt2sas_ctl_tm_done);
	ctl_diag_cb_idx = mpt2sas_base_register_callback_handler
	    (mpt2sas_ctl_diag_done);

	tm_tr_cb_idx = mpt2sas_base_register_callback_handler(
	    _scsih_tm_tr_complete);

	tm_tr_volume_cb_idx = mpt2sas_base_register_callback_handler(
	    _scsih_tm_volume_tr_complete);

	tm_tr_internal_cb_idx = mpt2sas_base_register_callback_handler(
	    _scsih_tm_internal_tr_complete);

#ifdef MPT2SAS_MULTIPATH
	tm_tr_mp_cb_idx = mpt2sas_base_register_callback_handler(
	    _scsih_tm_tr_mp_complete);
#endif
	tm_sas_control_cb_idx = mpt2sas_base_register_callback_handler(
	    _scsih_sas_control_complete);

	mpt2sas_ctl_init();

	error = pci_register_driver(&scsih_driver);
	if (error) {
#if !defined(__VMKLNX__)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27))
		/* raid transport support */
		raid_class_release(mpt2sas_raid_template);
#endif
#endif
		sas_release_transport(mpt2sas_transport_template);
	}

	return error;
}

/**
 * _scsih_exit - exit point for this driver (when it is a module).
 *
 * Returns 0 success, anything else error.
 */
static void __exit
_scsih_exit(void)
{
	printk(KERN_INFO "mpt2sas version %s unloading\n",
	    MPT2SAS_DRIVER_VERSION);

	pci_unregister_driver(&scsih_driver);

	mpt2sas_ctl_exit();

	mpt2sas_base_release_callback_handler(scsi_io_cb_idx);
	mpt2sas_base_release_callback_handler(tm_cb_idx);
	mpt2sas_base_release_callback_handler(base_cb_idx);
	mpt2sas_base_release_callback_handler(transport_cb_idx);
	mpt2sas_base_release_callback_handler(scsih_cb_idx);
	mpt2sas_base_release_callback_handler(scsih_q_cb_idx);
	mpt2sas_base_release_callback_handler(config_cb_idx);
	mpt2sas_base_release_callback_handler(ctl_cb_idx);
	mpt2sas_base_release_callback_handler(ctl_tm_cb_idx);
	mpt2sas_base_release_callback_handler(ctl_diag_cb_idx);
	mpt2sas_base_release_callback_handler(tm_tr_cb_idx);
	mpt2sas_base_release_callback_handler(tm_tr_volume_cb_idx);
	mpt2sas_base_release_callback_handler(tm_tr_internal_cb_idx);
#ifdef MPT2SAS_MULTIPATH
	mpt2sas_base_release_callback_handler(tm_tr_mp_cb_idx);
#endif
	mpt2sas_base_release_callback_handler(tm_sas_control_cb_idx);

#if !defined(__VMKLNX__)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27))
	/* raid transport support */
	raid_class_release(mpt2sas_raid_template);
#endif
#endif
	sas_release_transport(mpt2sas_transport_template);
}

module_init(_scsih_init);
module_exit(_scsih_exit);
