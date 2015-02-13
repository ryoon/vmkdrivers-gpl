/*
 * Portions Copyright 2008-2010 VMware, Inc.
 */
/*
 * Adaptec ADP94xx SAS HBA device driver for Linux.
 *
 * Written by: David Chaw <david_chaw@adaptec.com>
 * Modified by: Naveen Chandrasekaran <naveen_chandrasekaran@adaptec.com>
 * Modifications and cleanups: Luben Tuikov <luben_tuikov@adaptec.com>
 *
 * Copyright (c) 2004 Adaptec Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 *
 * $Id: //depot/razor/linux/src/adp94xx_osm.c#204 $
 * 
 */	
#if KDB_ENABLE
#include "linux/kdb.h"
#endif
#include "adp94xx_osm.h"
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
#if !defined(__VMKLNX__)
#include "linux/buffer_head.h"
#endif /* !defined(__VMKLNX__) */
#endif
#include "adp94xx_inline.h"
#include "adp94xx_sata.h"
#include "adp94xx_hwi.h"

#if defined(__VMKLNX__)
#include "vmklinux_scsi.h"
#endif /* defined(__VMKLNX__) */

/* Global variables */
LIST_HEAD(asd_hbas);
static Scsi_Host_Template 	asd_sht;
static asd_init_status_t	asd_init_stat;
static int			asd_init_result;
spinlock_t 			asd_list_spinlock;

#ifdef ASD_EH_SIMULATION
static u_long			cmd_cnt = 0;
#endif

#ifdef ASD_DEBUG
//JD
u_int				debug_mask = ASD_DBG_INIT | ASD_DBG_INFO | ASD_DBG_RUNTIME | ASD_DBG_ISR | ASD_DBG_ERROR;
#else
u_int				debug_mask = 0x0;
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
/* For dynamic sglist size calculation. */
u_int asd_nseg;
#endif

#ifdef MODULE
static char *adp94xx = NULL;

#ifdef MODULE_LICENSE
MODULE_LICENSE("Dual BSD/GPL");
#endif
#ifdef MODULE_VERSION
MODULE_VERSION(ASD_DRIVER_VERSION);
#endif
MODULE_AUTHOR("Maintainer: David Chaw <david_chaw@adaptec.com>");
MODULE_DESCRIPTION("Adaptec Linux SAS/SATA Family Driver");

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
	module_param(adp94xx, charp, S_IRUGO|S_IWUSR);
#else
	MODULE_PARM(adp94xx, "s");
#endif
MODULE_PARM_DESC(adp94xx,
"period delimited, options string.\n"
"	cmd_per_lun:<int>	Queue depth for all attached targets that\n"
"				support tag queueing\n"
"	attach_HostRAID:<int>	Attach to controllers in HostRAID mode\n"
"			(default is 0, false; 1 will enable this feature).\n"
"\n"
"	Sample module configuration line:\n"
"		Set the Queue Depth of all targets to 32.\n"
"\n"
"	options adp94xx 'adp94xx=cmd_per_lun:32'"
"\n"
"	Sample module configuration line:\n"
"		Disable this driver.\n"
"\n"
"	options adp94xx 'adp94xx=disable'");
#endif /* MODULE */

/* By default we do not attach to HostRAID enabled controllers.
 * You can turn this on by passing
 *     adp94xx=attach_HostRAID:1
 * to the driver (kernel command line, module parameter line).
 */
static int asd_attach_HostRAID = 0;

/* Module entry points */
static int __init	asd_init(void);
static void 		asd_exit(void);
static int 		adp94xx_setup(char *s);
static void		asd_setup_qtag_info(char *c);
static void		asd_setup_debug_info(char *c);

/* Initialization */
static void 		asd_size_nseg(void);

/* Midlayer entry points */
static int		asd_detect(Scsi_Host_Template *);
static const char      *asd_info(struct Scsi_Host *);
static int 		asd_queue(Scsi_Cmnd *, void (*)(Scsi_Cmnd *));
#if defined(__VMKLNX__)
irqreturn_t		asd_isr(int, void *);
#else /* !defined(__VMKLNX__) */
irqreturn_t		asd_isr(int, void *, struct pt_regs *);
#endif /* defined(__VMKLNX__) */
static int		asd_abort(Scsi_Cmnd *);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
static int		asd_proc_info(struct Scsi_Host *, char *, char **,
				      off_t, int, int);
static int		asd_slave_alloc(Scsi_Device *);
static int		asd_slave_configure(Scsi_Device *);
static void		asd_slave_destroy(Scsi_Device *);
static int		asd_initiate_bus_scan(struct asd_softc *asd);
#else
static int		asd_release(struct Scsi_Host *);
static int		asd_proc_info(char *, char **, off_t, int, int, int);
static void		asd_select_queue_depth(struct Scsi_Host *, 
					       Scsi_Device *);
#endif

/* Device Queue Depth Handling. */
#define ASD_DEF_TCQ_PER_DEVICE	32
#define ASD_MAX_TCQ_PER_DEVICE	64
#define ASD_MIN_TCQ_PER_DEVICE	1

#ifdef CONFIG_ADP94XX_TCQ_PER_DEVICE
#define ASD_TCQ_PER_DEVICE	CONFIG_ADP94XX_TCQ_PER_DEVICE
#else
#define ASD_TCQ_PER_DEVICE	ASD_DEF_TCQ_PER_DEVICE
#endif

u_int	cmd_per_lun = ASD_TCQ_PER_DEVICE;

typedef enum {
	ASD_QUEUE_NONE,
	ASD_QUEUE_BASIC,
	ASD_QUEUE_TAGGED
} asd_queue_alg;

static void	asd_set_device_queue_depth(struct asd_softc *asd,
					   struct asd_device *dev);
static u_int	asd_get_user_tagdepth(struct asd_softc *asd,
				      struct asd_device *dev);
static void	asd_set_tags(struct asd_softc *asd, struct asd_device *dev,
			     asd_queue_alg alg);
static void 	asd_print_path(struct asd_softc *asd, struct asd_device *dev);

/* Device Queue Handling. */
static struct asd_domain *
		asd_alloc_domain(struct asd_softc *asd, u_int channel_mapping);
static void	asd_free_domain(struct asd_softc *asd, struct asd_domain *dm);
static void	asd_dev_timed_unfreeze(u_long arg);
static void	asd_init_dev_itnl_exp(struct asd_softc *asd,
				      struct asd_target *targ, u_int lun);
void		asd_dev_intl_times_out(u_long arg);

static asd_scb_post_t	asd_scb_done;
static void	asd_handle_sas_status(struct asd_softc *asd,
				      struct asd_device *dev, struct scb *scb,
				      struct ssp_resp_edb *edb, u_int edb_len);
static ASD_COMMAND_BUILD_STATUS	
		asd_build_sas_scb(struct asd_softc *asd, struct scb *scb,
				  union asd_cmd *acmd);
static void	asd_runq_tasklet(unsigned long data);
static void	asd_unblock_tasklet(unsigned long data);
static void	asd_flush_device_queue(struct asd_softc *asd,
				       struct asd_device *dev);
static inline void
		asd_check_device_queue(struct asd_softc *asd,
				       struct asd_device *dev);
inline void 	asd_run_device_queues(struct asd_softc *asd);

/* Discovery and Device async. event thread. */
static int	asd_discovery_thread(void *data);
static void	asd_kill_discovery_thread(struct asd_softc *asd);
static int	asd_check_phy_events(struct asd_softc *asd, u_int phy_id);
static int	asd_check_port_events(struct asd_softc *asd, u_int port_id);
static void	asd_process_id_addr_evt(struct asd_softc *asd, 
					struct asd_phy *phy);
static int	asd_initiate_port_discovery(struct asd_softc *asd, 
				            struct asd_port *port);
static void	asd_handle_loss_of_signal(struct asd_softc *asd, 
					  struct asd_port *port);
static void 	asd_setup_port_data(struct asd_softc *asd,
				    struct asd_port *port, struct asd_phy *phy);
static int	asd_setup_target_data(struct asd_softc *asd,
				      struct asd_phy *phy,
				      struct asd_target *targ);
static void	asd_configure_port_targets(struct asd_softc *asd, 
					   struct asd_port *port);
static void	asd_configure_target(struct asd_softc *asd,
				     struct asd_target *targ);
static void	asd_destroy_target(struct asd_softc *asd,
				   struct asd_target *targ);
static void	asd_clear_device_io(struct asd_softc *asd,
				    struct asd_device *dev);
static asd_scb_post_t	asd_clear_device_io_done;

/* Error Recovery thread. */
static int	asd_ehandler_thread(void *data);
static void	asd_kill_ehandler_thread(struct asd_softc *asd);
static asd_scb_eh_post_t asd_ehandler_done;

#ifdef ASD_EH_SIMULATION
static void	asd_eh_simul_done(struct asd_softc *asd, struct scb *scb);
static int	asd_eh_simul_thread(void *data);
static void	asd_kill_eh_simul_thread(struct asd_softc *asd);
#endif

/* PCI entry points */
static int	asd_pci_dev_probe(struct pci_dev *pdev,
			 	  const struct pci_device_id *id);
static void	asd_pci_dev_remove(struct pci_dev *pdev);

/*
 * PCI Device specific initialization routine.
 */
typedef int (asd_pdev_setup_t)	(struct asd_softc *);

struct asd_pci_driver_data {
	/* Controller Description. */
	char		  *description;

	/* Controller Specific Setup Routine. */
	asd_pdev_setup_t  *setup;
};

static asd_pdev_setup_t asd_aic9410_setup;
static asd_pdev_setup_t asd_aic9405_setup;

/* Supported PCI Vendor & Device ID */
#ifndef PCI_VENDOR_ID_ADAPTEC2
#define PCI_VENDOR_ID_ADAPTEC2	0x9005
#endif
#define PCI_CLASS_DEFAULT	0
#define PCI_CLASS_MASK_DEFAULT	0

static const struct asd_pci_driver_data asd_aic9405_drv_data = {
	"Adaptec AIC-9405W SAS/SATA Host Adapter",
	asd_aic9405_setup
};

static const struct asd_pci_driver_data  asd_aic9410_drv_data = {
	"Adaptec AIC-9410 SAS/SATA Host Adapter",
	asd_aic9410_setup
};

#ifdef ASD_FOR_FUTURE_USE
static const struct asd_pci_driver_data  asd_comstock_drv_data = {
	"Adaptec COMSTOCK SAS/SATA Controller",
	asd_comstock_setup
};
#endif

static struct pci_device_id  asd_pci_ids_table[] = {
	{
		PCI_VENDOR_ID_ADAPTEC2, 0x0410, PCI_ANY_ID, PCI_ANY_ID,
		PCI_CLASS_DEFAULT, PCI_CLASS_MASK_DEFAULT,
		(unsigned long) &asd_aic9410_drv_data
	},
	{
		PCI_VENDOR_ID_ADAPTEC2, 0x0412, PCI_ANY_ID, PCI_ANY_ID,
		PCI_CLASS_DEFAULT, PCI_CLASS_MASK_DEFAULT,
		(unsigned long) &asd_aic9410_drv_data
	},
	{
		PCI_VENDOR_ID_ADAPTEC2, 0x041E, PCI_ANY_ID, PCI_ANY_ID,
		PCI_CLASS_DEFAULT, PCI_CLASS_MASK_DEFAULT,
		(unsigned long) &asd_aic9410_drv_data
	},
	{
		PCI_VENDOR_ID_ADAPTEC2, 0x041F, PCI_ANY_ID, PCI_ANY_ID,
		PCI_CLASS_DEFAULT, PCI_CLASS_MASK_DEFAULT,
		(unsigned long) &asd_aic9410_drv_data
	},
	{
		PCI_VENDOR_ID_ADAPTEC2, 0x0430, PCI_ANY_ID, PCI_ANY_ID,
		PCI_CLASS_DEFAULT, PCI_CLASS_MASK_DEFAULT,
		(unsigned long) &asd_aic9405_drv_data
	},
	{
		PCI_VENDOR_ID_ADAPTEC2, 0x0432, PCI_ANY_ID, PCI_ANY_ID,
		PCI_CLASS_DEFAULT, PCI_CLASS_MASK_DEFAULT,
		(unsigned long) &asd_aic9405_drv_data
	},
	{
		PCI_VENDOR_ID_ADAPTEC2, 0x043E, PCI_ANY_ID, PCI_ANY_ID,
		PCI_CLASS_DEFAULT, PCI_CLASS_MASK_DEFAULT,
		(unsigned long) &asd_aic9405_drv_data
	},
	{
		PCI_VENDOR_ID_ADAPTEC2, 0x043F, PCI_ANY_ID, PCI_ANY_ID,
		PCI_CLASS_DEFAULT, PCI_CLASS_MASK_DEFAULT,
		(unsigned long) &asd_aic9405_drv_data
	},
	{ 0 }
};

MODULE_DEVICE_TABLE(pci, asd_pci_ids_table);

struct pci_driver  adp94xx_pci_driver = {
	.name		= ASD_DRIVER_NAME,
	.probe		= asd_pci_dev_probe,
	.remove 	= asd_pci_dev_remove,
	.id_table	= asd_pci_ids_table
};

/* Local functions */
static struct asd_softc	*asd_get_softc(struct asd_softc *asd);
static int		asd_get_unit(void);
static int		asd_register_host(struct asd_softc *asd);
static void             __asd_unregister_host(struct asd_softc *asd);
static int		asd_init_hw(struct asd_softc *asd);
static int		asd_map_io_handle(struct asd_softc *asd);
static int		asd_mem_mapped_io_handle(struct asd_softc *asd);
static int		asd_io_mapped_io_handle(struct asd_softc *asd);

/********************************* Inlines ************************************/

static inline void	asd_init_tasklets(struct asd_softc *asd);
static inline void	asd_kill_tasklets(struct asd_softc *asd);

static inline void
asd_init_tasklets(struct asd_softc *asd)
{
	tasklet_init(&asd->platform_data->runq_tasklet, asd_runq_tasklet,
		     (unsigned long) asd);
	tasklet_init(&asd->platform_data->unblock_tasklet, asd_unblock_tasklet,
		     (unsigned long) asd);
}

static inline void
asd_kill_tasklets(struct asd_softc *asd)
{
	tasklet_kill(&asd->platform_data->runq_tasklet);
	tasklet_kill(&asd->platform_data->unblock_tasklet);
}	

struct asd_device *
asd_get_device(struct asd_softc *asd, u_int ch, u_int id, u_int lun, int alloc)
{
	struct asd_domain	*dm;
	struct asd_target	*targ;
	struct asd_device	*dev;

	ASD_LOCK_ASSERT(asd);

	/*
	 * Domain and target structures are allocated by our
	 * discovery process.  Fail if our mapping attempt
	 * finds either of these path components missing.
	 */
	if ((ch >= asd->platform_data->num_domains) || 
	    ((dm = asd->platform_data->domains[ch]) == NULL)) {
		return (NULL);
	}

	if ((id >= ASD_MAX_TARGET_IDS) || 
	    ((targ = dm->targets[id]) == NULL)) {
		return (NULL);
	}

	if (lun >= ASD_MAX_LUNS) {
		return (NULL);
	}

	dev = targ->devices[lun];

	return (dev);
}

void
asd_unmap_scb(struct asd_softc *asd, struct scb *scb)
{
	Scsi_Cmnd *cmd;
	int direction;

	cmd = &acmd_scsi_cmd(scb->io_ctx);
	direction = scsi_to_pci_dma_dir(cmd->sc_data_direction);

	if (cmd->use_sg != 0) {
		struct scatterlist *sg;

		sg = (struct scatterlist *)cmd->request_buffer;
		asd_unmap_sg(asd, sg, cmd->use_sg, direction);
	} else if (cmd->request_bufflen != 0) {
		asd_unmap_single(asd,
				 scb->platform_data->buf_busaddr,
				 cmd->request_bufflen, direction);
	}
}

static inline void
asd_check_device_queue(struct asd_softc *asd, struct asd_device *dev)
{
	ASD_LOCK_ASSERT(asd);

	if ((dev->flags & ASD_DEV_FREEZE_TIL_EMPTY) != 0 && dev->active == 0) {
		dev->flags &= ~ASD_DEV_FREEZE_TIL_EMPTY;
		dev->qfrozen--;
	}

	if (list_empty(&dev->busyq) || dev->openings == 0 || 
	    dev->qfrozen != 0 || dev->target->qfrozen != 0)
		return;

	if ((dev->target->src_port->events & ASD_DISCOVERY_PROCESS) != 0)
		return;

	asd_flush_device_queue(asd, dev);
}

inline void
asd_run_device_queues(struct asd_softc *asd)
{
	struct asd_device *dev;

	while ((dev = asd_next_device_to_run(asd)) != NULL) {
		list_del(&dev->links);
		dev->flags &= ~ASD_DEV_ON_RUN_LIST;
		asd_check_device_queue(asd, dev);
	}
}

static inline void asd_target_addref(struct asd_target *targ);
static inline void asd_target_release(struct asd_softc *asd,
				      struct asd_target *targ);
static inline void asd_domain_addref(struct asd_domain *dm);
static inline void asd_domain_release(struct asd_softc *asd,
				      struct asd_target *targ);

static inline void
asd_target_addref(struct asd_target *targ)
{
	targ->refcount++;
}

static inline void
asd_target_release(struct asd_softc *asd, struct asd_target *targ)
{
	targ->refcount--;
	if (targ->refcount == 0) {
		asd_free_target(asd, targ);
	}
}

static inline void
asd_domain_addref(struct asd_domain *dm)
{
	dm->refcount++;
}

static inline void
asd_domain_release(struct asd_softc *asd, struct asd_target *targ)
{
	targ->domain->refcount--;

	targ->domain->targets[targ->target_id] = NULL;

	if (targ->domain->refcount == 0) {
		asd_free_domain(asd, targ->domain);
	}
}

/******************************************************************************/

/* 
 * Function:
 *	asd_get_softc()
 *
 * Description:
 *	Search if the requested host is in our HBA list. 
 */
static struct asd_softc	*
asd_get_softc(struct asd_softc *asd) 
{
	struct asd_softc	*entry;
	unsigned long		 flags;
	
	asd_list_lock(&flags);
	list_for_each_entry(entry, &asd_hbas, link) {
		if (entry == asd) {
			asd_list_unlock(&flags);
			return (asd);
		}
	}
	asd_list_unlock(&flags);
		
	return (NULL);
}

/* 
 * Function:
 *	asd_get_softc_by_hba_index()
 *
 * Description:
 *	Search and return the host corresponding to the user requested 
 *	hba_index from our HBA list. 
 */
struct asd_softc *
asd_get_softc_by_hba_index(uint32_t hba_index)
{
	struct asd_softc	*entry;
	unsigned long		 flags;
	
	asd_list_lock(&flags);
	list_for_each_entry(entry, &asd_hbas, link) {
		if (entry->asd_hba_index == hba_index) {
			asd_list_unlock(&flags);
			return (entry);
		}
	}
	asd_list_unlock(&flags);
		
	return (NULL);
}

/* 
 * Function:
 *	asd_get_number_of_hbas_present()
 *
 * Description:
 *	Return the total number of SAS HBAs present
 */
int 
asd_get_number_of_hbas_present(void)
{
	struct asd_softc	*entry;
	unsigned long		 flags;
	int hba_count;
	
	hba_count = 0;
	asd_list_lock(&flags);
	list_for_each_entry(entry, &asd_hbas, link) {
		hba_count++;
	}
	asd_list_unlock(&flags);
		
	return hba_count;
}

/*
 * Function:
 * 	asd_get_unit()
 *
 * Description:
 *	Find the smallest available unit number to use for a new device.
 * 	Avoid using a static count to handle the "repeated hot-(un)plug"
 * 	scenario.
 */
static int
asd_get_unit(void)
{
	struct asd_softc	*asd;
	unsigned long		 flags;
	int			 unit;

	unit = 0;
	asd_list_lock(&flags);
retry:
	list_for_each_entry(asd, &asd_hbas, link) {
		if (asd->profile.unit == unit) {
			unit++;
			goto retry;
		}
	}
	asd_list_unlock(&flags);
	return (unit);
}

static void
asd_print_path(struct asd_softc *asd, struct asd_device *dev)
{
	if (dev != NULL)
		asd_print("(scsi%d: Ch %d Id %d Lun %d): ",
			  asd->platform_data->scsi_host->host_no,
			  dev->ch, dev->id, dev->lun);
}

static void
asd_setup_qtag_info(char *c)
{
	u_int	tags;

	tags = simple_strtoul(c + 1, NULL, 0) & (~0);

	if ((tags > ASD_MAX_TCQ_PER_DEVICE) || (tags < ASD_MIN_TCQ_PER_DEVICE))
		/* Set the Queue Depth to default (32). */
		tags = ASD_DEF_TCQ_PER_DEVICE;

	asd_print("Setting User Option (cmd_per_lun) : %d\n\n", tags);

	if ((cmd_per_lun > ASD_MAX_TCQ_PER_DEVICE) ||
	    (cmd_per_lun < ASD_MIN_TCQ_PER_DEVICE))
		/* Set the Queue Depth to default (32). */
		cmd_per_lun = ASD_DEF_TCQ_PER_DEVICE;
	else
		cmd_per_lun = tags;
}

static void
asd_setup_debug_info(char *c)
{
	u_int 	dbg_mask;

	dbg_mask = simple_strtoul(c + 1, NULL, 0) & 0xFF;
	asd_print("Setting the Debug Mask : 0x%x\n\n", dbg_mask);
}

/*
 * Function:
 *	adp94xx_setup()
 *
 * Description:
 * 	Handle Linux boot parameters. This routine allows for assigning a value
 * 	to a parameter with a ':' between the parameter and the value.
 * 	ie. adp94xx=cmd_per_lun:32
 */
static int
adp94xx_setup(char *s)
{
	char   *p;
	char   *end;
	int	i, n;

	static struct {
		const char 	*name;
		int 	*flag;
	} options[] = {
		{ "cmd_per_lun", NULL },
		{ "attach_HostRAID", &asd_attach_HostRAID },
	};

	end = strchr(s, '\0');
	n = 0;  

	while ((p = strsep(&s, ",.")) != NULL) {
		if (*p == '\0')
			continue;
		for (i = 0; i < NUM_ELEMENTS(options); i++) {

			n = strlen(options[i].name);
			if (strncmp(options[i].name, p, n) == 0)
				break;
		}
		if (i == NUM_ELEMENTS(options))
			continue;

		if (strncmp(p, "cmd_per_lun", n) == 0) {
			asd_setup_qtag_info(p + n);
		} else if (strncmp(p, "debug_mask", n) == 0) {
			asd_setup_debug_info(p + n);
		} else if (p[n] == ':') {
			*(options[i].flag) = simple_strtoul(p + n + 1, NULL,0);
		} else {
			*(options[i].flag) ^= 0xFFFFFFFF;
		}
	}
	return (1);
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,3,0)
__setup("adp94xx=", adp94xx_setup);
#endif

#ifdef ASD_DEBUG
struct timer_list	debug_timer;
#endif

/* 
 * Function:
 *	asd_init()
 *
 * Description:
 *	This is the entry point which will be called during module loading. 
 */
static int __init
asd_init(void)
{
#if KDB_ENABLE
	KDB_ENTER();
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
	asd_detect(&asd_sht);
#else
	scsi_register_module(MODULE_SCSI_HA, &asd_sht);
#endif
	if (asd_init_result != 0) {
		asd_log(ASD_DBG_INIT, "Module init failed !\n");
		asd_exit();
		return (asd_init_result);
	}

	/* Register the IOCTL char device. */
	if (asd_register_ioctl_dev() != 0) {
		asd_log(ASD_DBG_INIT, "Failed to register IOCTL "
			"dev.\n");
		asd_exit();
		asd_init_result = -1;
	} else {
		asd_init_stat.asd_ioctl_registered = 1;
	}

	return (asd_init_result);
}

/* 
 * Function:
 *	asd_exit()
 *
 * Description:
 *	This is the entry point which will be called during module unloading. 
 */
static void
asd_exit(void)
{
	struct asd_softc  *asd;

	asd_log(ASD_DBG_INIT, "Unloading module ...\n");

	list_for_each_entry(asd, &asd_hbas, link) {
		asd_kill_discovery_thread(asd);

		asd_kill_ehandler_thread(asd);
#ifdef ASD_EH_SIMULATION		
		asd_kill_eh_simul_thread(asd);
#endif
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)	
	scsi_unregister_module(MODULE_SCSI_HA, &asd_sht);
#endif
	if (asd_init_stat.asd_pci_registered == 1)
		pci_unregister_driver(&adp94xx_pci_driver); 

	if (asd_init_stat.asd_ioctl_registered == 1)
		asd_unregister_ioctl_dev();
	
}

static int asd_aic9405_setup(struct asd_softc *asd)
{
//JD
#ifdef ASD_DEBUG
	asd->debug_flag=0;
#endif
	/*
	 * max_cmds_per_lun will be throttled once we know the attached 
	 * device is DASD.
	 */
	asd->profile.max_cmds_per_lun = 2;
	asd->profile.can_queue = ASD_MAX_QUEUE;
	asd->profile.initiator_id = 255;
	asd->profile.max_luns = ASD_MAX_LUNS;
	asd->profile.max_scsi_ids = ASD_MAX_TARGET_IDS;
	asd->profile.max_channels = ASD_MAX_PORTS / 2;
	/*
	 * For now, we assumed that the controller can support 64-bit
	 * addressing. 
	 */
	asd->profile.dma64_addr = 1;
	asd->profile.name = ASD_DRIVER_NAME;
		
	/* Controller specific profile. */
	asd->hw_profile.max_devices = ASD_MAX_DEVICES;
	asd->hw_profile.max_targets = ASD_MAX_TARGETS;
	asd->hw_profile.max_ports = ASD_MAX_PORTS / 2;
	asd->hw_profile.max_phys = ASD_MAX_PHYS;
	/*
	 * enabled_phys could be used as a bitmap for the user to specified 
	 * which phys to be enabled.
	 */ 
	asd->hw_profile.enabled_phys = 0xF0;
#ifndef EXTENDED_SCB
	asd->hw_profile.max_scbs = ASD_MAX_USABLE_SCBS;
#else
	asd->hw_profile.max_scbs = ASD_MAX_USABLE_SCBS + ASD_EXTENDED_SCB_NUMBER;
#endif
	asd->hw_profile.max_ddbs = ASD_MAX_DDBS;	 
	asd->hw_profile.rev_id = asd_pcic_read_byte(asd, PCIC_DEVREV_ID);

	/* LT: shamelessly stolen from aic94xx driver */
	asd->hw_profile.addr_range = 4;
	asd->hw_profile.port_name_base = 0;
	asd->hw_profile.dev_name_base = 4;
	asd->hw_profile.sata_name_base = 8;

	/* 
	 * Default World Wide Name set for the Controller.
	 */
	asd->hw_profile.wwn[0] = 0x50;
	asd->hw_profile.wwn[1] = 0x00;
	asd->hw_profile.wwn[2] = 0x0d;
	asd->hw_profile.wwn[3] = 0x1f;
	asd->hw_profile.wwn[4] = 0xed;
	asd->hw_profile.wwn[5] = 0xcb;
	asd->hw_profile.wwn[6] = 0xa9;
	asd->hw_profile.wwn[7] = 0x89;
	return (0);
}

/* 
 * Function:
 *	asd_aic9410_setup()
 *
 * Description:
 *	Setup host profile initialization for the AIC9410 controller. 
 */
static int
asd_aic9410_setup(struct asd_softc *asd)
{
	/*
	 * max_cmds_per_lun will be throttled once we know the attached 
	 * device is DASD.
	 */
	asd->profile.max_cmds_per_lun = 2;
	asd->profile.can_queue = ASD_MAX_QUEUE;
	asd->profile.initiator_id = 255;
	asd->profile.max_luns = ASD_MAX_LUNS;
	asd->profile.max_scsi_ids = ASD_MAX_TARGET_IDS;
	asd->profile.max_channels = ASD_MAX_PORTS;
	/*
	 * For now, we assumed that the controller can support 64-bit
	 * addressing. 
	 */
	asd->profile.dma64_addr = 1;
	asd->profile.name = ASD_DRIVER_NAME;
		
	/* Controller specific profile. */
	asd->hw_profile.max_devices = ASD_MAX_DEVICES;
	asd->hw_profile.max_targets = ASD_MAX_TARGETS;
	asd->hw_profile.max_ports = ASD_MAX_PORTS;
	asd->hw_profile.max_phys = ASD_MAX_PHYS;
	/*
	 * enabled_phys could be used as a bitmap for the user to specified 
	 * which phys to be enabled.
	 */ 
	asd->hw_profile.enabled_phys = 0xFF;
#ifndef EXTENDED_SCB
	asd->hw_profile.max_scbs = ASD_MAX_USABLE_SCBS;
#else
	asd->hw_profile.max_scbs = ASD_MAX_USABLE_SCBS + ASD_EXTENDED_SCB_NUMBER;
#endif
	asd->hw_profile.max_ddbs = ASD_MAX_DDBS;	 
	asd->hw_profile.rev_id = asd_pcic_read_byte(asd, PCIC_DEVREV_ID);

	/* LT: shamelessly stolen from aic94xx driver */
	asd->hw_profile.addr_range = 8;
	asd->hw_profile.port_name_base = 0;
	asd->hw_profile.dev_name_base = 8;
	asd->hw_profile.sata_name_base = 16;

	/* 
	 * Default World Wide Name set for the Controller.
	 */
	asd->hw_profile.wwn[0] = 0x50;
	asd->hw_profile.wwn[1] = 0x00;
	asd->hw_profile.wwn[2] = 0x0d;
	asd->hw_profile.wwn[3] = 0x1f;
	asd->hw_profile.wwn[4] = 0xed;
	asd->hw_profile.wwn[5] = 0xcb;
	asd->hw_profile.wwn[6] = 0xa9;
	asd->hw_profile.wwn[7] = 0x89;
	return (0);
}

/*
 * Function:
 *	asd_size_nseg()
 *
 * Description:
 *
 * In pre-2.5.X...
 * The midlayer allocates an S/G array dynamically when a command is issued
 * using SCSI malloc.  This array, which is in an OS dependent format that
 * must later be copied to our private S/G list, is sized to house just the
 * number of segments needed for the current transfer.  Since the code that
 * sizes the SCSI malloc pool does not take into consideration fragmentation
 * of the pool, executing transactions numbering just a fraction of our
 * concurrent transaction limit with list lengths aproaching AHC_NSEG will
 * quickly depleat the SCSI malloc pool of usable space.  Unfortunately, the
 * mid-layer does not properly handle this scsi malloc failures for the S/G
 * array and the result can be a lockup of the I/O subsystem.  We try to size
 * our S/G list so that it satisfies our drivers allocation requirements in
 * addition to avoiding fragmentation of the SCSI malloc pool.
 */
static void
asd_size_nseg(void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	u_int cur_size;
	u_int best_size;

	/*
	 * The SCSI allocator rounds to the nearest 512 bytes
	 * and cannot allocate across a page boundary.  Our algorithm
	 * is to start at 1K of scsi malloc space per-command and
	 * loop through all factors of the PAGE_SIZE and pick the best.
	 */
	best_size = 0;
	for (cur_size = 1024; cur_size <= PAGE_SIZE; cur_size *= 2) {
		u_int nseg;

		nseg = cur_size / sizeof(struct scatterlist);
		if (nseg < ASD_LINUX_MIN_NSEG)
			continue;

		if (best_size == 0) {
			best_size = cur_size;
			asd_nseg = nseg;
		} else {
			u_int best_rem;
			u_int cur_rem;

			/*
			 * Compare the traits of the current "best_size"
			 * with the current size to determine if the
			 * current size is a better size.
			 */
			best_rem = best_size % sizeof(struct scatterlist);
			cur_rem = cur_size % sizeof(struct scatterlist);
			if (cur_rem < best_rem) {
				best_size = cur_size;
				asd_nseg = nseg;
			}
		}
	}
#endif
}

/* 
 * Function:
 *	asd_detect()
 *
 * Description:
 *	This routine shall detect any supported controller. 
 */
static int
asd_detect(Scsi_Host_Template *sht)
{
	struct asd_softc	*asd;
	int			 error;
	uint8_t 		 hba_cnt;

	asd_print("Loading AIC-94xx Linux SAS/SATA Family Driver, Rev: "
		  "%d.%d.%d-%d\n\n", 
		  ASD_MAJOR_VERSION, ASD_MINOR_VERSION, 
		  ASD_BUILD_VERSION, ASD_RELEASE_VERSION);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	/*
	 * Release the lock that was held by the midlayer prior to calling us.
	 */
	spin_unlock_irq(&io_request_lock);
#endif
	hba_cnt = 0;
	asd_init_stat.asd_init_state = 1;

#ifdef MODULE
	/*
	 * Parse the command line parameters if we have received any.
	 */
	if (adp94xx) {
		adp94xx_setup(adp94xx);
	}
#endif

	asd_list_lockinit();
	/*
	 * Determine an appropriate size for our SG lists.
	 */
	asd_size_nseg();

	/* Register our PCI entry points and id table. */
	error = pci_module_init(&adp94xx_pci_driver);
	if (error != 0) {	
		asd_init_result = error;
		goto exit;
	} else {
		asd_init_stat.asd_pci_registered = 1;
		asd_init_result = error;
	}

	/* For every controller found, register it with the midlayer. */
	list_for_each_entry(asd, &asd_hbas, link) {
		error = asd_register_host(asd);
		if (error != 0) {
			asd_init_result = error;
			goto exit;
		}
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
		error = asd_initiate_bus_scan(asd);
		if (error != 0) {
			struct pci_dev *pci_dev = asd_pci_dev(asd);
			asd_print("%s: couldn't initiate bus scan for "
				  "PCI device %x:%x.%x (%x:%x)\n",
				  asd_name(asd),
				  pci_dev->bus->number,
				  PCI_SLOT(pci_dev->devfn),
				  PCI_FUNC(pci_dev->devfn),
				  pci_dev->vendor,
				  pci_dev->device);
			__asd_unregister_host(asd);
			continue;
		}
#endif
		asd->asd_hba_index = hba_cnt++;

		asd_ctl_init_internal_data(asd);
	}

	asd_init_stat.asd_init_state = 0;

exit:
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	/* Acquire the lock before returning to midlayer. */
	spin_lock_irq(&io_request_lock);
#endif
	asd_print("AIC-94xx controller(s) attached = %d.\n\n", hba_cnt);

	return (hba_cnt);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
/* 
 * Function:
 *	asd_release()
 *
 * Description:
 *	Free the Scsi_Host structure during unloading module. 
 */
static int
asd_release(struct Scsi_Host *scsi_host)
{
	struct asd_softc	*asd;
	unsigned long		 flags;

	if (scsi_host != NULL) {
		asd = *((struct asd_softc **) scsi_host->hostdata);
		
		if (asd_get_softc(asd) != NULL) {
			asd_list_lock(&flags);
			list_del(&asd->link);
			asd_list_unlock(&flags);
			asd_free_softc(asd);
		}
	}

	return (0);
}
#endif

/* 
 * Function:
 *	asd_register_host()
 *
 * Description:
 *	Register our controller with the scsi midlayer. 
 */
static int		
asd_register_host(struct asd_softc *asd)
{
	struct Scsi_Host	*scsi_host;
	u_long			 flags;
	
	asd_sht.name = ((struct asd_pci_driver_data *) 
			(asd->pci_entry->driver_data))->description;	

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
	scsi_host = scsi_host_alloc(&asd_sht, sizeof(struct asd_softc *));
#else
	scsi_host = scsi_register(&asd_sht, sizeof(struct asd_softc *));
#endif 
	if (scsi_host == NULL)
		return (-ENOMEM);
	
	*((struct asd_softc **) scsi_host->hostdata) = asd;
	
	asd_lock(asd, &flags);

	/* Fill in host related fields. */
	asd->platform_data->scsi_host = scsi_host;
	scsi_host->can_queue = asd->profile.can_queue;
	scsi_host->cmd_per_lun = asd->profile.max_cmds_per_lun;
	scsi_host->sg_tablesize	= ASD_NSEG;
	scsi_host->max_channel = asd->profile.max_channels;
	scsi_host->max_id = asd->profile.max_scsi_ids;
	scsi_host->max_lun = asd->profile.max_luns;
	scsi_host->this_id = asd->profile.initiator_id;
	scsi_host->unique_id = asd->profile.unit = asd_get_unit();
	scsi_host->irq = asd->profile.irq;	
#if defined(__VMKLNX__)
	scsi_host->max_cmd_len = 16;	
#endif	/* defined(__VMKLNX__) */
	asd_assign_host_lock(asd);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,4) && \
    LINUX_VERSION_CODE  < KERNEL_VERSION(2,5,0)	
	scsi_set_pci_device(scsi_host, asd->dev);
#endif	
	asd_unlock(asd, &flags);

	/*
	 * Create a discovery thread and block the OS
	 * discovery of targets until our first discovery
	 * pass has completed.
	 */
	asd_freeze_hostq(asd);
	asd->platform_data->flags |= ASD_DISCOVERY_INIT;

	asd->platform_data->discovery_pid = kernel_thread(
						asd_discovery_thread, 
						asd, 0);
	if (asd->platform_data->discovery_pid < 0) {
		asd_print("%s: Failed to create discovery thread, error=%d\n",
			  asd_name(asd), asd->platform_data->discovery_pid);
		return (-asd->platform_data->discovery_pid);
	}

	asd->platform_data->ehandler_pid = kernel_thread(
						asd_ehandler_thread, 
						asd, 0);
	if (asd->platform_data->ehandler_pid < 0) {
		asd_print("%s: Failed to create error handler thread, "
			  "error=%d\n",
			  asd_name(asd), asd->platform_data->ehandler_pid);
		return (-asd->platform_data->ehandler_pid);
	}

#ifdef ASD_EH_SIMULATION		
	asd->platform_data->eh_simul_pid = kernel_thread(
						asd_eh_simul_thread, 
						asd, 0);
	if (asd->platform_data->eh_simul_pid < 0) {
		asd_print("%s: Failed to create eh_simul thread, error=%d\n",
			  asd_name(asd), asd->platform_data->eh_simul_pid);
		return (-asd->platform_data->eh_simul_pid);
	}
#endif

	return (0);
}

/*
 * Function:
 * __asd_unregister_host()
 *
 * Description:
 * Revert what asd_register_host() did.  This function is to be used
 * from asd_detect().
 */
static void __asd_unregister_host(struct asd_softc *asd)
{
#ifdef ASD_EH_SIMULATION
	/* shutdown the simulation thread */
	asd->platform_data->flags |= ASD_EH_SIMUL_SHUTDOWN;
	up(&asd->platform_data->eh_simul_sem);
#endif /* ASD_EH_SIMULATION */

	/* shutdown the eh and discovery thread */
	asd->platform_data->flags |=
		(ASD_RECOVERY_SHUTDOWN | ASD_DISCOVERY_SHUTDOWN);
	up(&asd->platform_data->ehandler_sem);
	up(&asd->platform_data->discovery_sem);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
	scsi_remove_host(asd->platform_data->scsi_host);
	scsi_host_put(asd->platform_data->scsi_host);
#else
	scsi_unregister(asd->platform_data->scsi_host);
#endif 
}

/***************************** Queue Handling ********************************/

struct asd_device *
asd_alloc_device(struct asd_softc *asd, struct asd_target *targ,
		 u_int ch, u_int id, u_int lun)
{
	struct asd_device *dev;

	dev = asd_alloc_mem(sizeof(*dev), GFP_ATOMIC);
	if (dev == NULL)
		return (NULL);

	memset(dev, 0, sizeof(*dev));
	init_timer(&dev->timer);
	INIT_LIST_HEAD(&dev->busyq);
	dev->flags = ASD_DEV_UNCONFIGURED;
	dev->ch = ch;
	dev->id = id;
	dev->lun = lun;
	memcpy(dev->saslun, &lun, sizeof(u_int)/*8*/);//TBD
	dev->target = targ;
#ifdef MULTIPATH_IO
	dev->current_target = targ;
#endif

	/*
	 * We start out life using untagged
	 * transactions of which we allow one.
	 */
	dev->openings = 1;

	/*
	 * Set maxtags to 0.  This will be changed if we
	 * later determine that we are dealing with
	 * a tagged queuing capable device.
	 */
	dev->maxtags = 0;
	
	targ->devices[lun] = dev;
	asd_target_addref(targ);

	return (dev);
}

void
asd_free_device(struct asd_softc *asd, struct asd_device *dev)
{
	struct asd_target *targ;

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,41)
	/*
	 * Ensure no scheduled workqueue entry is running for
	 * this device prior freeing the device.
	 */
	if ((dev->flags & ASD_DEV_DPC_ACTIVE) != 0)
		flush_scheduled_work();
#endif	
	targ = dev->target;
	targ->devices[dev->lun] = NULL;
	asd_free_mem(dev);
	asd_target_release(asd, targ);
	targ->flags &= ~ASD_TARG_MAPPED;
}

struct asd_target *
asd_alloc_target(struct asd_softc *asd, struct asd_port *src_port)
{
	struct asd_target *targ;

	targ = asd_alloc_mem(sizeof(*targ), GFP_ATOMIC);
	if (targ == NULL)
		return (NULL);
	memset(targ, 0, sizeof(*targ));

	/*
	 * By default, we are not mapped into
	 * the OS topology view.
	 */
	targ->domain = NULL;
	targ->target_id = ASD_MAX_TARGET_IDS;

	/*
	 * Targets stay around until some event drops
	 * the sentinel reference count on the object.
	 */
	targ->refcount = 0;

	targ->softc = asd;
	targ->flags |= ASD_TARG_FLAGS_NONE;
	targ->src_port = src_port;
	INIT_LIST_HEAD(&targ->children);
	INIT_LIST_HEAD(&targ->siblings);
	INIT_LIST_HEAD(&targ->all_domain_targets);
	INIT_LIST_HEAD(&targ->validate_links);
	INIT_LIST_HEAD(&targ->multipath);
	init_timer(&targ->timer);
	return (targ);
}

void
asd_free_target(struct asd_softc *asd, struct asd_target *targ)
{
	list_del(&targ->children);
	list_del(&targ->siblings);
	list_del(&targ->multipath);
	list_del(&targ->all_domain_targets);

	if (targ->domain != NULL) {
		asd_domain_release(asd, targ);
		targ->domain = NULL;
	}

	if (targ->Phy != NULL) {
		asd_free_mem(targ->Phy);
	}

	if (targ->RouteTable != NULL) {
		asd_free_mem(targ->RouteTable);
	}

	asd_free_mem(targ);
}

static struct asd_domain *
asd_alloc_domain(struct asd_softc *asd, u_int channel_mapping)
{
	struct asd_domain *dm;

	dm = asd_alloc_mem(sizeof(*dm), GFP_ATOMIC);
	if (dm == NULL)
		return (NULL);

	memset(dm, 0, sizeof(*dm));
	asd->platform_data->domains[channel_mapping] = dm;
	dm->channel_mapping = channel_mapping;

	dm->refcount = 0;

	return (dm);
}

static void
asd_free_domain(struct asd_softc *asd, struct asd_domain *dm)
{
	u_int i;

	for (i = 0; i < ASD_MAX_TARGET_IDS; i++) {
		if (dm->targets[i] != NULL) {
			asd_print("%s: Freeing non-empty domain %p!\n",
				  asd_name(asd), dm);
			break;
		}
	}
	asd->platform_data->domains[dm->channel_mapping] = NULL;
	asd_free_mem(dm);
}

static void
asd_flush_device_queue(struct asd_softc *asd, struct asd_device *dev)
{
	union asd_cmd 		*acmd;
	struct scsi_cmnd 	*cmd;
	struct scb 		*scb;
	struct asd_port		*port;
	ASD_COMMAND_BUILD_STATUS build_status;

	ASD_LOCK_ASSERT(asd);

	if ((dev->flags & ASD_DEV_ON_RUN_LIST) != 0)
		panic("asd_flush_device_queue: running device on run list");

	while (!list_empty(&dev->busyq) && (dev->openings > 0) && 
	       (dev->qfrozen == 0) && (dev->target->qfrozen == 0)) {

		if (asd->platform_data->qfrozen != 0) {
			/*
			 * Schedule us to run later.  The only reason we are not
			 * running is because the whole controller Q is frozen.
			 */
			list_add_tail(&dev->links,
				      &asd->platform_data->device_runq);
			dev->flags |= ASD_DEV_ON_RUN_LIST;
			return;
		}

		port = dev->target->src_port;
		if (((port->events & ASD_DISCOVERY_PROCESS) != 0) && 
		    ((port->events & ASD_DISCOVERY_REQ) != 0)) {
			/*
			 * Discovery is requested / on-going for the port
			 * that the device is currently attached to.
			 * Prevent any new IOs going to the device until
			 * the discovery is done and configuration is
			 * validated.
			 */
			list_add_tail(&dev->links,
				      &asd->platform_data->device_runq);
			dev->flags |= ASD_DEV_ON_RUN_LIST;
			return;
		}

		acmd = list_entry(dev->busyq.next, union asd_cmd, acmd_links);
		list_del(&acmd->acmd_links);
		cmd = &acmd_scsi_cmd(acmd);

		/*
		 * The target is in the process of being destroyed as
		 * it had been hot-removed. Return the IO back to the
		 * scsi layer.
		 */
		if (dev->target->flags & ASD_TARG_HOT_REMOVED) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
			asd_cmd_set_host_status(cmd, DID_NO_CONNECT);
#else
			asd_cmd_set_offline_status(cmd);
#endif
			cmd->scsi_done(cmd);
			continue;
		}

#if !defined(__VMKLNX__)
/*
 * "sc_magic" field in "struct scsi_cmnd" is obsolete since 2.6.18,
 * ASD_CSMI_COMMAND looks like some adaptec internal command.
 * comment it out for now.
 */
		if (cmd->sc_magic != ASD_CSMI_COMMAND) {
#endif /* !defined(__VMKLNX__) */
			/*
			 * Get an scb to use.
			 */
			if ((scb = asd_hwi_get_scb(asd, 0)) == NULL) {
				list_add_tail(&acmd->acmd_links, &dev->busyq);
				list_add_tail(&dev->links,
					      &asd->platform_data->device_runq);
				dev->flags |= ASD_DEV_ON_RUN_LIST;
				asd->flags |= ASD_SCB_RESOURCE_SHORTAGE;
				asd->platform_data->qfrozen++;
				return;
			}

			scb->platform_data->dev = dev;
			scb->platform_data->targ = dev->target;
			cmd->host_scribble = (unsigned char *)scb;

			/*
			 * SCB build handlers return zero status to indicate
			 * that the SCB should be queued to the controller.
			 * Any other status indicates that the OS's cmd
			 * structure should be completed and that the build
			 * handler has updated its status accordingly.
			 */
			switch (dev->target->command_set_type) {
			case ASD_COMMAND_SET_SCSI:
				build_status = asd_build_sas_scb(
					asd, scb, acmd);
				break;

			case ASD_COMMAND_SET_ATA:
				build_status = asd_build_ata_scb(
					asd, scb, acmd);

				break;

			case ASD_COMMAND_SET_ATAPI:
				build_status = asd_build_atapi_scb(
					asd, scb, acmd);
				break;

			default:
				build_status = ASD_COMMAND_BUILD_FAILED;
				asd_cmd_set_host_status(cmd, DID_NO_CONNECT);

				/*
				 * Fall through to complete and free the scb.
				 */
				break;
			}

			if (build_status != ASD_COMMAND_BUILD_OK) {
				/*
				 * Two cases here:
				 * 1) The command has been emulated, and it
				 *    is ASD_COMMAND_BUILD_FINISHED
				 * 2) The command was malformed and it is
				 *    ASD_COMMAND_BUILD_FAILED.
				 */
				asd_hwi_free_scb(asd, scb);
				cmd->scsi_done(cmd);
				continue;
			}
#if !defined(__VMKLNX__)
		} else {
			/* command generated by CSMI */
			scb = (struct scb *)cmd->host_scribble;
		}
#endif /* !defined(__VMKLNX__) */

		dev->openings--;
		dev->active++;
		dev->commands_issued++;
		list_add_tail(&scb->owner_links,
			      &asd->platform_data->pending_os_scbs);
		scb->flags |= SCB_ACTIVE;
		asd_hwi_post_scb(asd, scb);

#ifdef ASD_EH_SIMULATION
		if (asd_cmd_get_host_status(cmd) == 0x88) {
			scb->flags |= SCB_TIMEDOUT;
			scb->eh_state = SCB_EH_DEV_RESET_REQ;
			scb->eh_post = asd_eh_simul_done;
			list_add_tail(&scb->timedout_links, 
				      &asd->timedout_scbs);
			asd_print("Adding scb(%d) to timedout queue for "
				  "error recv. simulation.\n",
				  SCB_GET_INDEX(scb));
			asd_wakeup_sem(&asd->platform_data->ehandler_sem);
		}
#endif /* ASD_EH_SIMULATION */
#if !defined(__VMKLNX__)
		if (cmd->sc_magic == ASD_CSMI_COMMAND)
			asd_free_mem(cmd);
#endif /* !defined(__VMKLNX__) */
	}

}

#ifdef ASD_EH_SIMULATION
static void
asd_eh_simul_done(struct asd_softc *asd, struct scb *scb)
{
	asd_print("EH SIMULATION DONE.\n");
}

static int
asd_eh_simul_thread(void *data)
{
	struct asd_softc	*asd;
	struct scb		*scb;
	u_long			 flags;	

	asd = (struct asd_softc *) data;

	lock_kernel();
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,60)
	/*
	 * Don't care about any signals.
	 */
	siginitsetinv(&current->blocked, 0);
	daemonize();
	sprintf(current->comm, "asd_eh_simul_%d", asd->profile.unit);
#else
	daemonize("asd_eh_simul_%d", asd->profile.unit);
	current->flags |= PF_FREEZE;	
#endif
	unlock_kernel();

	while (1) {
sleep:
		down_interruptible(&asd->platform_data->eh_simul_sem);

		/* Check to see if we've been signaled to exit. */
		asd_lock(asd, &flags);
		if ((asd->platform_data->flags & ASD_EH_SIMUL_SHUTDOWN) != 0) {
			asd_unlock(asd, &flags);
			break;
		}

		asd_unlock(asd, &flags);
		
		if (!list_empty(&asd->timedout_scbs)) {
			scb = list_entry(asd->timedout_scbs.next, struct scb, 
				 timedout_links);
			if (scb == NULL) {
				asd_print("Timedout queue is empty.\n");
				goto sleep;
			}

			asd_lock(asd, &flags);

			list_del(&scb->timedout_links);
			scb->flags &= ~SCB_TIMEDOUT;
		
			asd_unlock(asd, &flags);
		}
	}

	return (0);
}
#endif /* ASD_EH_SIMULATION */

static void
asd_dev_timed_unfreeze(u_long arg)
{
	struct asd_softc	*asd;
	struct asd_device	*dev;
	u_long			 flags;

	dev = (struct asd_device *) arg;
	asd = dev->target->softc;
	
	asd_lock(asd, &flags);
	/*
	 * Release our hold on the device.
	 */
	dev->flags &= ~ASD_DEV_TIMER_ACTIVE;
	dev->active--;

	if (dev->qfrozen > 0)
		dev->qfrozen--;

	if ((dev->qfrozen == 0) && (dev->target->qfrozen == 0) &&
	    ((dev->flags & ASD_DEV_ON_RUN_LIST) == 0))
		asd_flush_device_queue(asd, dev);
	
	if ((dev->flags & ASD_DEV_UNCONFIGURED) != 0 &&
	     list_empty(&dev->busyq) && dev->active == 0) {
		asd_free_device(asd, dev);
	}
	asd_unlock(asd, &flags);
}

void
asd_timed_run_dev_queue(u_long arg)
{
	struct asd_softc	*asd;
	struct asd_device	*dev;
	u_long			 flags;

	dev = (struct asd_device *) arg;
	asd = dev->target->softc;
	
	asd_lock(asd, &flags);

	dev->flags &= ~ASD_DEV_TIMER_ACTIVE;

	if ((dev->qfrozen == 0) && (dev->target->qfrozen == 0) &&
	    ((dev->target->flags & ASD_TARG_HOT_REMOVED) == 0) &&
	    ((dev->flags & ASD_DEV_ON_RUN_LIST) == 0)) {

		asd_flush_device_queue(asd, dev);
	} else {
		asd_log(ASD_DBG_ERROR, "DEV QF: %d TARG QF: %d "
			"DEV FL: 0x%x TARG FL: 0x%x. ptarget=%p\n",
			dev->qfrozen, dev->target->qfrozen,
			dev->flags, dev->target->flags,dev->target);
	}

	asd_unlock(asd, &flags);
}
	
static void
asd_scb_done(struct asd_softc *asd, struct scb *scb, struct asd_done_list *dl)
{
	Scsi_Cmnd 		*cmd;
	struct asd_device *dev;

	if ((scb->flags & SCB_ACTIVE) == 0) {
		asd_print("SCB %d done'd twice\n", SCB_GET_INDEX(scb));
		panic("Stopping for safety");
	}

	list_del(&scb->owner_links);

	cmd = &acmd_scsi_cmd(scb->io_ctx);
	dev = scb->platform_data->dev;
	dev->active--;
	dev->openings++;
	if ((scb->flags & SCB_DEV_QFRZN) != 0) {
		scb->flags &= ~SCB_DEV_QFRZN;
		dev->qfrozen--;
	}

	asd_unmap_scb(asd, scb);

	/*
	 * Guard against stale sense data.
	 * The Linux mid-layer assumes that sense
	 * was retrieved anytime the first byte of
	 * the sense buffer looks "sane".
	 */
	cmd->sense_buffer[0] = 0;
	cmd->resid = 0;
//JD
#ifdef ASD_DEBUG
	if( (dl->opcode != TASK_COMP_WO_ERR) && (dl->opcode != TASK_COMP_W_UNDERRUN) ) 
#if LINUX_VERSION_CODE <  KERNEL_VERSION(2,6,13)
		asd_log(ASD_DBG_INFO, "asd_scb_done with error cmd LBA 0x%02x%02x%02x%02x - 0x%02x%02x (tag:0x%x, pid:0x%x, res_count:0x%x, timeout:0x%x abort:%d) dl->opcode 0x%x\n",
#else
		asd_log(ASD_DBG_INFO, "asd_scb_done with error cmd LBA 0x%02x%02x%02x%02x - 0x%02x%02x (tag:0x%x, pid:0x%x, res_count:0x%x, timeout:0x%x) dl->opcode 0x%x\n",
#endif
			cmd->cmnd[2],
			cmd->cmnd[3],
			cmd->cmnd[4],
			cmd->cmnd[5],
			cmd->cmnd[7],
			cmd->cmnd[8],
		  cmd->tag,
		  cmd->pid,
		  cmd->resid,
		  cmd->timeout_per_command,
#if LINUX_VERSION_CODE <  KERNEL_VERSION(2,6,13)
		  cmd->abort_reason,
#endif
		  dl->opcode);
#endif
	switch (dl->opcode) {
	case TASK_COMP_W_UNDERRUN:
		cmd->resid = asd_le32toh(dl->stat_blk.data.res_len);
		/* FALLTHROUGH */
	case TASK_COMP_WO_ERR:
		asd_cmd_set_host_status(cmd, DID_OK);
		break;
	
	case SSP_TASK_COMP_W_RESP:
	{
		union edb 		*edb;
		struct response_sb   	*rsp;
		struct ssp_resp_edb  	*redb;
		struct scb 		*escb;
		u_int  			 escb_index;
		u_int			 edb_index;

		rsp = &dl->stat_blk.response;
		escb_index = asd_le16toh(rsp->empty_scb_tc);
		edb_index = RSP_EDB_ELEM(rsp) - 1;
		edb = asd_hwi_indexes_to_edb(asd, &escb, escb_index, edb_index);
		if (edb == NULL) {
			asd_print("Invalid EDB recv for SSP comp w/response.\n"
				  "Returning generic error to OS.\n");
			asd_cmd_set_host_status(cmd, DID_ERROR);
			break;
		}
		redb = &edb->ssp_resp;
		cmd->resid = asd_le32toh(redb->res_len);
		asd_handle_sas_status(asd, dev, scb, redb, RSP_EDB_BUFLEN(rsp));
		asd_hwi_free_edb(asd, escb, edb_index);
		break;
	}
	case TASK_ABORTED_ON_REQUEST:
		asd_cmd_set_host_status(cmd, DID_ABORT);
		break;
	
	case TASK_CLEARED:
	{
		struct task_cleared_sb	*task_clr;

		task_clr = &dl->stat_blk.task_cleared;

		asd_log(ASD_DBG_ERROR," Task Cleared for Tag: 0x%x, "
			"TC: 0x%x.\n",
			task_clr->tag_of_cleared_task, SCB_GET_INDEX(scb));

		/*
		 * Pending command at the firmware's queues aborted upon
		 * request. If the device is offline then failed the IO.
		 * Otherwise, have the command retried again.
	         */
		if (task_clr->clr_nxs_ctx == ASD_TARG_HOT_REMOVED) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
			asd_cmd_set_host_status(cmd, DID_NO_CONNECT);
#else
			asd_cmd_set_offline_status(cmd);
#endif
		} else
			asd_cmd_set_host_status(cmd, DID_SOFT_ERROR);

		break;
	}

	case TASK_INT_W_BRK_RCVD:
		asd_log(ASD_DBG_ERROR, "TASK INT. WITH BREAK RECEIVED.\n");
		asd_cmd_set_host_status(cmd, DID_SOFT_ERROR);
		break;

	case TASK_ABORTED_BY_ITNL_EXP:
	{
		struct itnl_exp_sb	*itnl_exp;

		itnl_exp = &dl->stat_blk.itnl_exp;

		asd_log(ASD_DBG_ERROR, "ITNL EXP for SCB 0x%x Reason = 0x%x.\n",
			SCB_GET_INDEX(scb), itnl_exp->reason);
		
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
		asd_cmd_set_host_status(cmd, DID_NO_CONNECT);
#else
		asd_cmd_set_offline_status(cmd);
#endif
		break;
	}

	case TASK_F_W_NAK_RCVD:
	{
		asd_log(ASD_DBG_ERROR, "TASK FAILED WITH NAK RECEIVED.\n");
		asd_cmd_set_host_status(cmd, DID_SOFT_ERROR);
		break;
	}

	default:
		asd_log(ASD_DBG_ERROR, "UNHANDLED TASK.\n");
		asd_cmd_set_host_status(cmd, DID_SOFT_ERROR);
		break;
	}

	if ((dev->target->flags & ASD_TARG_HOT_REMOVED) != 0) {
		/*
	 	 * If the target had been removed and all active IOs on 
		 * the device have been completed, schedule the device to
		 * be destroyed.
	 	 */
		if (list_empty(&dev->busyq) && (dev->active == 0) &&
		   ((dev->flags & ASD_DEV_DESTROY_WAS_ACTIVE) != 0)) {
			/*
			 * Schedule a deferred process task to destroy
		         * the device.
			 */	 
			asd_setup_dev_dpc_task(dev, asd_destroy_device);
		}
	} else {
		if ((dev->flags & ASD_DEV_ON_RUN_LIST) == 0) {
			list_add_tail(&dev->links,
				      &asd->platform_data->device_runq);
			dev->flags |= ASD_DEV_ON_RUN_LIST;
		}
	}

	/*
	 * Only free the scb if it hasn't timedout.
	 * For SCB that has timedout, error recovery has invoked and
	 * the timedout SCB will be freed in the error recovery path.
	 */
	if ((scb->flags & SCB_TIMEDOUT) == 0)
		asd_hwi_free_scb(asd, scb);
//JDTEST
	else
	{
		asd_log(ASD_DBG_ERROR, "scb 0x%x SCB_TIMEDOUT(0x%x)\n",scb,scb->flags);
		scb->flags |= SCB_ABORT_DONE;
	}

	
	cmd->scsi_done(cmd);
}

void
asd_scb_internal_done(struct asd_softc *asd, struct scb *scb,
		      struct asd_done_list *dl)
{
	if ((scb->flags & SCB_ACTIVE) == 0) {
		asd_print("SCB %d done'd twice\n", SCB_GET_INDEX(scb));
		panic("Stopping for safety");
	}
	list_del(&scb->owner_links);

	/*
	 * In this case, "internal" means that the scb does not have
	 * a Scsi_Cmnd associated with it.
	 */
	if ((scb->flags & SCB_INTERNAL) == 0) {
		asd_unmap_scb(asd, scb);
	}

	switch (dl->opcode) {
	case TASK_COMP_W_UNDERRUN:
		/* FALLTHROUGH */
	case TASK_COMP_WO_ERR:
		break;
	case SSP_TASK_COMP_W_RESP:
	{
		union	edb *edb;
		struct	response_sb   *rsp;
		struct	ssp_resp_edb  *redb;
		struct	scb *escb;
		u_int	escb_index;
		u_int	edb_index;

		rsp = &dl->stat_blk.response;
		escb_index = asd_le16toh(rsp->empty_scb_tc);
		edb_index = RSP_EDB_ELEM(rsp) - 1;
		edb = asd_hwi_indexes_to_edb(asd, &escb, escb_index, edb_index);
		if (edb == NULL) {
			asd_print("Invalid EDB recv for SSP comp w/response.\n"
				  "Returning generic error to OS.\n");
			break;
		}
		redb = &edb->ssp_resp;
		asd_hwi_free_edb(asd, escb, edb_index);
	}
	default:
		break;
	}

	/*
	 * Only free the scb if it hasn't timedout.
	 * For SCB that has timedout, error recovery has invoked and
	 * the timedout SCB will be freed in the error recovery path.
	 */
	if ((scb->flags & SCB_TIMEDOUT) == 0)
		asd_hwi_free_scb(asd, scb);
//JDTEST
	else
	{
		scb->flags |= SCB_ABORT_DONE;
		asd_log(ASD_DBG_ERROR, "scb 0x%x SCB_TIMEDOUT(0x%x)\n",scb,scb->flags);
	}
}

static void
asd_handle_sas_status(struct asd_softc *asd, struct asd_device *dev,
		      struct scb *scb, struct ssp_resp_edb *edb, u_int edb_len)
{
	struct ssp_resp_iu 	*riu;
	Scsi_Cmnd 		*cmd;

	cmd = &acmd_scsi_cmd(scb->io_ctx);
	riu = &edb->resp_frame.riu;
	if (edb_len < offsetof(struct ssp_resp_edb, resp_frame.riu.res_2)) {
		asd_print("Insufficient data recv for SSP comp w/response.\n"
			  "Returning generic error to OS.\n");
		asd_cmd_set_host_status(cmd, DID_ERROR);
		return;
	}

	switch (SSP_RIU_DATAPRES(riu)) {
	case SSP_RIU_DATAPRES_RESP:
	{
		uint8_t	resp_code;

		resp_code = ((struct resp_data_iu *) &riu->data[0])->resp_code;

		asd_print("Unhandled RESPONSE data (resp code: 0x%x).\n"
			  "Returning generic error to OS.\n",
			  resp_code);
		asd_cmd_set_host_status(cmd, DID_SOFT_ERROR);
		return;
	}

	case SSP_RIU_DATAPRES_SENSE:
	{
		uint32_t sense_len;

		/* Copy sense data. */
		if (edb_len < offsetof(struct ssp_resp_edb,
				       resp_frame.riu.sense_len)) {
			asd_print("Insufficient data recv for sense len.\n"
				  "Returning generic error to OS.\n");
			asd_cmd_set_host_status(cmd, DID_SOFT_ERROR);
			return;
		}
		sense_len = edb_len - offsetof(struct ssp_resp_edb,
					       resp_frame.riu.data);
		sense_len = MIN(sense_len, scsi_4btoul(riu->sense_len));
		if (sense_len <= 0) {
			asd_print("Insufficient data recv for sense data.\n"
				  "Returning generic error to OS.\n");
			asd_cmd_set_host_status(cmd, DID_SOFT_ERROR);
			return;
		}
		sense_len = MIN(sizeof(cmd->sense_buffer), sense_len);
		memset(cmd->sense_buffer, 0, sizeof(cmd->sense_buffer));
		memcpy(cmd->sense_buffer, riu->data, sense_len);
		asd_cmd_set_driver_status(cmd, DRIVER_SENSE);

#if !defined(__VMKLNX__)
		/*
		 * Power on reset or bus reset occurred, let's have the 
		 * command retried again.
		 */ 
		if ((cmd->sense_buffer[2] == UNIT_ATTENTION) && 
		    (cmd->sense_buffer[12] == 0x29)) {
			asd_cmd_set_host_status(cmd, DID_ERROR);
			break;
		}
#endif /* !defined(__VMKLNX__) */
	}
		/* FALLTHROUGH*/
	case SSP_RIU_DATAPRES_NONE:
		asd_cmd_set_host_status(cmd, DID_OK);
		asd_cmd_set_scsi_status(cmd, riu->status);
		break;

	default:
		asd_log(ASD_DBG_ERROR, "Unknown response frame format.\n"
			"Returning generic error to OS.\n");
		asd_cmd_set_host_status(cmd, DID_SOFT_ERROR);
		return;
	}

	/*
	 * We don't currently trust the mid-layer to
	 * properly deal with queue full or busy.  So,
	 * when one occurs, we tell the mid-layer to
	 * unconditionally requeue the command to us
	 * so that we can retry it ourselves.  We also
	 * implement our own throttling mechanism so
	 * we don't clobber the device with too many
	 * commands.
	 */
	switch (riu->status) {
	case SCSI_STATUS_OK:
	case SCSI_STATUS_CHECK_COND:
	case SCSI_STATUS_COND_MET:
	case SCSI_STATUS_INTERMED:
	case SCSI_STATUS_INTERMED_COND_MET:
	case SCSI_STATUS_RESERV_CONFLICT:
	case SCSI_STATUS_CMD_TERMINATED:
	case SCSI_STATUS_ACA_ACTIVE:
	case SCSI_STATUS_TASK_ABORTED:
		break;

	case SCSI_STATUS_QUEUE_FULL:
	{
		/*
		 * Note that dev->active may not be 100% accurate
		 * since it counts commands in the outgoing SCB queue
		 * that have yet to be seen by the end device.  In
		 * practice, this doesn't matter since we will not queue
		 * additional commands until we receive a successful
		 * completion.  If we have not dropped the count to
		 * the device's queue depth yet, we will see additional
		 * queue fulls as the outgoing SCB queue drains, resulting
		 * in further drops of the queue depth.
		 */
		asd_print_path(asd, dev);
		asd_print("Queue Full!\n");
		
		dev->tag_success_count = 0;
		if (dev->active != 0) {
			/*
			 * Drop our opening count to the number
			 * of commands currently outstanding.
			 */
			dev->openings = 0;
			asd_print_path(asd, dev);
			asd_print("Dropping tag count to %d\n", dev->active);
			if (dev->active == dev->tags_on_last_queuefull) {
				dev->last_queuefull_same_count++;
				/*
				 * If we repeatedly see a queue full
				 * at the same queue depth, this
				 * device has a fixed number of tag
				 * slots.  Lock in this tag depth
				 * so we stop seeing queue fulls from
				 * this device.
				 */
				if (dev->last_queuefull_same_count == 
				    ASD_LOCK_TAGS_COUNT) {
					dev->maxtags = dev->active;
					asd_print_path(asd, dev);
					asd_print("Locking tag count at %d\n",
						  dev->active);
				}
			} else {
				dev->tags_on_last_queuefull = dev->active;
				dev->last_queuefull_same_count = 0;
			}
			
			asd_set_tags(asd, dev,
				    (dev->flags & ASD_DEV_Q_BASIC) ?
				    ASD_QUEUE_BASIC : ASD_QUEUE_TAGGED);

			asd_cmd_set_retry_status(cmd);
			break;
		}
		/*
		 * Drop down to a single opening, and treat this
		 * as if the target returned BUSY SCSI status.
		 */
		dev->openings = 1;
		asd_cmd_set_scsi_status(cmd, SCSI_STATUS_BUSY);
		/* FALLTHROUGH */
	}
	case SCSI_STATUS_BUSY:
		asd_log(ASD_DBG_ERROR, "REVISITED: SCSI_STATUS_BUSY");

		/*
		 * Set a short timer to defer sending commands for
		 * a bit since Linux will not delay in this case.
		 */
		if ((dev->flags & ASD_DEV_TIMER_ACTIVE) != 0) {
			asd_print("%s:%c:%d: Device Timer still active during "
				  "busy processing\n", asd_name(asd),
				  dev->target->domain->channel_mapping,
				  dev->target->target_id);
			break;
		}
		dev->qfrozen++;
		/*
		 * Keep the active count non-zero during
		 * the lifetime of the timer.  This
		 * guarantees that the device will not
		 * be freed before our timer executes.
		 */
		dev->active++;
		dev->flags |= ASD_DEV_TIMER_ACTIVE;
		init_timer(&dev->timer);
		dev->timer.data = (u_long) dev;
		dev->timer.expires = jiffies + (HZ/2);
		dev->timer.function = asd_dev_timed_unfreeze;
		add_timer(&dev->timer);
		break;
	default:
		/*
		 * Unknown scsi status returned by the target.
		 *  Have the command retried.
		 */ 
		asd_cmd_set_host_status(cmd, DID_SOFT_ERROR);
		break;
	}
}

ASD_COMMAND_BUILD_STATUS
asd_setup_data(struct asd_softc *asd, struct scb *scb, Scsi_Cmnd *cmd)
{
	struct asd_ssp_task_hscb *ssp_hscb;
	struct sg_element 	 *sg;
	int			  dir;
	int			  error;

	/*
	 * All SSP, STP, and SATA SCBs have their direction
	 * flags and SG/elements in the same place, so using
	 * any of their definitions here is safe.
	 */
	ssp_hscb = &scb->hscb->ssp_task;
	scb->sg_count = 0;
	error = 0;

	if (cmd->use_sg != 0) {
		struct	scatterlist *cur_seg;
		u_int	nseg;

		cur_seg = (struct scatterlist *)cmd->request_buffer;
		dir = scsi_to_pci_dma_dir(cmd->sc_data_direction);
		nseg = asd_map_sg(asd, cur_seg, cmd->use_sg, dir);
		scb->sg_count = nseg;
		if (nseg > ASD_NSEG) {
			asd_unmap_sg(asd, cur_seg, nseg, dir);
			return ASD_COMMAND_BUILD_FAILED;
		}
#if defined(__VMKLNX__)
		for (sg = scb->sg_list; nseg > 0; nseg--, sg++) {
#else /* !defined(__VMKLNX__) */
		for (sg = scb->sg_list; nseg > 0; nseg--, cur_seg++, sg++) {
#endif /* defined(__VMKLNX__) */
			dma_addr_t addr;
			uint32_t len;

			addr = sg_dma_address(cur_seg);
			len = sg_dma_len(cur_seg);
			error = asd_sg_setup(sg, addr, len, /*last*/nseg == 1);
			if (error != 0)
				break;
#if defined(__VMKLNX__)
			cur_seg = sg_next(cur_seg);
		}

		/* do reset - for SG_VMK type */
		sg_reset(cur_seg);
#else /* !defined(__VMKLNX__) */
		}
#endif /* defined(__VMKLNX__) */
		if (error != 0) {
			asd_unmap_sg(asd, cur_seg, scb->sg_count, dir);
			return ASD_COMMAND_BUILD_FAILED;
		}
	} else if (cmd->request_bufflen != 0) {
		dma_addr_t addr;

		sg = scb->sg_list;
		dir = scsi_to_pci_dma_dir(cmd->sc_data_direction);
		addr = asd_map_single(asd,
				      cmd->request_buffer,
				      cmd->request_bufflen, dir);
		scb->platform_data->buf_busaddr = addr;
		error = asd_sg_setup(sg, addr, cmd->request_bufflen, /*last*/1);
		if (error != 0) {
			asd_unmap_single(asd, addr, cmd->request_bufflen, dir);
			return ASD_COMMAND_BUILD_FAILED;
		}
		scb->sg_count = 1;
	} else {
		scb->sg_count = 0;
		dir = PCI_DMA_NONE;
	}

	if (scb->sg_count != 0) {
		size_t sg_copy_size;

		sg_copy_size = scb->sg_count * sizeof(*sg);
		if (scb->sg_count > 3)
			sg_copy_size = 2 * sizeof(*sg);
		memcpy(ssp_hscb->sg_elements, scb->sg_list, sg_copy_size);
		if (scb->sg_count > 3) {
			/*
			 * Setup SG sub-list.
			 */
			sg = &ssp_hscb->sg_elements[1];
			sg->next_sg_offset = 2 * sizeof(*sg);
			sg->flags |= SG_EOS;
			sg++;
			sg->bus_address = asd_htole64(scb->sg_list_busaddr);
			memset(&sg->length, 0,
			       sizeof(*sg)-offsetof(struct sg_element, length));
		}
	}

	switch (dir) {
	case PCI_DMA_BIDIRECTIONAL:
		ssp_hscb->data_dir_flags |= DATA_DIR_UNSPECIFIED;
		break;
	case PCI_DMA_TODEVICE:
		ssp_hscb->data_dir_flags |= DATA_DIR_OUTBOUND;
		break;
	case PCI_DMA_FROMDEVICE:
		ssp_hscb->data_dir_flags |= DATA_DIR_INBOUND;
		break;
	case PCI_DMA_NONE:
		ssp_hscb->data_dir_flags |= DATA_DIR_NO_XFER;
		break;
	}

	return ASD_COMMAND_BUILD_OK;
}

static ASD_COMMAND_BUILD_STATUS
asd_build_sas_scb(struct asd_softc *asd, struct scb *scb, union asd_cmd *acmd)
{
	struct asd_ssp_task_hscb	*ssp_hscb;
	struct asd_target 		*targ;
	Scsi_Cmnd 			*cmd;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0) 
	int				 msg_bytes;
	char 			 tag_msgs[2];
#endif

	asd_push_post_stack(asd, scb, acmd, asd_scb_done);

	cmd = &acmd_scsi_cmd(acmd);
#ifdef MULTIPATH_IO
	targ = scb->platform_data->dev->current_target;

	scb->platform_data->dev->current_target = list_entry(
		scb->platform_data->dev->current_target->multipath.next,
		struct asd_target, multipath);
#else
	targ = scb->platform_data->dev->target;
#endif
	ssp_hscb = &scb->hscb->ssp_task;

	ssp_hscb->header.opcode = SCB_INITIATE_SSP_TASK;

	/*
	 * Build the SAS frame header.
	 */
	asd_build_sas_header(targ, ssp_hscb);

	ssp_hscb->protocol_conn_rate |= PROTOCOL_TYPE_SSP;
	ssp_hscb->xfer_len = asd_htole32(cmd->request_bufflen);

	/*
	 * Hnadle for multi-lun devices. 
	 */
	memcpy(ssp_hscb->lun, scb->platform_data->dev->saslun, SAS_LUN_LEN);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0) 
	msg_bytes = scsi_populate_tag_msg(cmd, tag_msgs);
	if (msg_bytes && tag_msgs[0] != MSG_SIMPLE_TAG)
		ssp_hscb->task_attr |= tag_msgs[0] & 0x3;
#endif
	memcpy(ssp_hscb->cdb, cmd->cmnd, cmd->cmd_len);
	memset(&ssp_hscb->cdb[cmd->cmd_len], 0,
	       SCB_EMBEDDED_CDB_SIZE - cmd->cmd_len);

	return (asd_setup_data(asd, scb, cmd));
}

/*
 * This routine is called from a tasklet, so we must re-acquire
 * our lock prior when accessing data-structures that need protection.
 */
static void
asd_runq_tasklet(unsigned long data) {
	struct asd_softc	*asd;
	struct asd_device 	*dev;
	u_long			 flags = 0;
#if defined(__VMKLNX__)
	uint32_t ownLock = 0;
#endif

	asd = (struct asd_softc *) data;
#if defined(__VMKLNX__)
	/*
	 * In the coredump code path, this tasklet is invoked by
	 * the top half interrupt handler in the non-interrupt mode.
	 * The same spinlock is already taken in the top half interrupt handler.
	 * Do not take the lock again to avoid the deadlock.
	 * See PR 329916 for more analysis.
	 */
	ownLock = !vmklnx_spin_is_locked_by_my_cpu(&asd->platform_data->spinlock);
	if (ownLock) {
	    asd_lock(asd, &flags);
	}
#else
	asd_lock(asd, &flags);
#endif
	while ((dev = asd_next_device_to_run(asd)) != NULL) {
		list_del(&dev->links);
		dev->flags &= ~ASD_DEV_ON_RUN_LIST;
		asd_check_device_queue(asd, dev);
		/* Yeild to our interrupt handler */
		asd_unlock(asd, &flags);
		asd_lock(asd, &flags);
	}
#if defined(__VMKLNX__)
	if (ownLock) {
	    asd_unlock(asd, &flags);
	}
#else
	asd_unlock(asd, &flags);
#endif
}

static void
asd_unblock_tasklet(unsigned long data)
{
	struct asd_softc *asd;

	asd = (struct asd_softc *) data;
	scsi_unblock_requests(asd->platform_data->scsi_host);
}

/*
 * Function:
 * 	asd_discovery_thread()
 *
 * Description:
 *	Thread to handle device discovery, topology changes and async. event
 *	from attached device(s).
 */	   	  
static int
asd_discovery_thread(void *data)
{
	struct asd_softc	*asd;
	u_long			 flags;
	u_char			 id;

	asd = (struct asd_softc *) data;

	lock_kernel();
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,60)
	/*
	 * Don't care about any signals.
	 */
	siginitsetinv(&current->blocked, 0);
	daemonize();
	sprintf(current->comm, "asd_disc_%d", asd->profile.unit);
#else
	daemonize("asd_disc_%d", asd->profile.unit);
#if !defined(__VMKLNX__)
	current->flags |= PF_FREEZE;	
#endif /* !defined(__VMKLNX__) */
#endif
	unlock_kernel();
	
	while (1) {
		int 	pending;

		/*
		 * Use down_interruptible() rather than down() to
		 * avoid inclusion in the load average.
		 */
		down_interruptible(&asd->platform_data->discovery_sem);

		/* Check to see if we've been signaled to exit. */
		asd_lock(asd, &flags);
		if ((asd->platform_data->flags & ASD_DISCOVERY_SHUTDOWN) != 0) {
			asd_unlock(asd, &flags);
			break;
		}
		asd_unlock(asd, &flags);

		/*
		 * For now, only handle ID ADDR RCVD event.
		 */
		pending = 0;
		for (id = 0; id < asd->hw_profile.max_phys; id++)
			pending += asd_check_phy_events(asd, id);

		for (id = 0; id < asd->hw_profile.max_ports; id++)
			pending += asd_check_port_events(asd, id);

		if (pending == 0) {
			asd_lock(asd, &flags);
			asd->platform_data->flags &= ~ASD_DISCOVERY_INIT;
			asd_unlock(asd, &flags);
			asd_release_hostq(asd);
		}
	}

	up(&asd->platform_data->discovery_ending_sem);

	return (0);
}

/*
 * Function:
 * 	asd_kill_discovery_thread()
 * 
 * Description:
 * 	Kill the discovery thread.
 */
static void
asd_kill_discovery_thread(struct asd_softc *asd)
{
	u_long	flags;

	asd_lock(asd, &flags);

	if (asd->platform_data->discovery_pid != 0) {
		asd->platform_data->flags |= ASD_DISCOVERY_SHUTDOWN;
		asd_unlock(asd, &flags);

		up(&asd->platform_data->discovery_sem);

		/*
		 * Wait for the discovery thread to exit before continuing
		 * with the unloading processes.
		 */
		down_interruptible(&asd->platform_data->discovery_ending_sem);

		asd->platform_data->discovery_pid = 0;
	} else {
		asd_unlock(asd, &flags);
	}
}

#ifdef ASD_EH_SIMULATION
/*
 * Function:
 * 	asd_kill_eh_simul_thread()
 * 
 * Description:
 * 	Kill the EH Simulation thread.
 */ 	   	  
static void
asd_kill_eh_simul_thread(struct asd_softc  *asd)
{
	u_long	flags;

	asd_lock(asd, &flags);

	if (asd->platform_data->eh_simul_pid != 0) {
		asd->platform_data->flags |= ASD_EH_SIMUL_SHUTDOWN;
		asd_unlock(asd, &flags);
		up(&asd->platform_data->eh_simul_sem);
		down_interruptible(&asd->platform_data->ehandler_ending_sem);
		asd->platform_data->eh_simul_pid = 0;
	} else {
		asd_unlock(asd, &flags);
	}
}
#endif /* ASD_EH_SIMULATION */

/*
 * Function:
 *	asd_check_phy_events()
 *	
 * Description:
 *	Check if any phy events that needs to be handled.
 */
static int
asd_check_phy_events(struct asd_softc *asd, u_int phy_id)
{
	struct	asd_phy	*phy;
	u_long		 flags;

	phy = asd->phy_list[phy_id];

	asd_lock(asd, &flags);

	if ((phy->state != ASD_PHY_WAITING_FOR_ID_ADDR) &&
	    (list_empty(&phy->pending_scbs))) {
		asd_unlock(asd, &flags);
		return 0;
	}

	if ((phy->state == ASD_PHY_WAITING_FOR_ID_ADDR) &&
	    ((phy->attr & ASD_SATA_SPINUP_HOLD) != 0) &&
	    (list_empty(&phy->pending_scbs))) {

#if defined(__VMKLNX__)
		asd_unlock(asd, &flags);
#endif
		asd_hwi_release_sata_spinup_hold(asd, phy);

		return 1;
	}

	/* Handle ID Address Frame that has been received. */
	if ((phy->events & ASD_ID_ADDR_RCVD) != 0) {
		asd_process_id_addr_evt(asd, phy);
		phy->events &= ~ASD_ID_ADDR_RCVD;
		phy->state = ASD_PHY_CONNECTED;
	}

	asd_unlock(asd, &flags);

	return 1;
}

/*
 * Function:
 *	asd_check_port_events()
 *
 * Description:
 *	Check if any port events that need to be handled.
 */
static int
asd_check_port_events(struct asd_softc *asd, u_int port_id)
{
	struct asd_port	*port;

	port = asd->port_list[port_id];
//JD
#ifdef ASD_DEBUG
	if(port->events != 0)
		asd_log(ASD_DBG_INFO, "asd_check_port_events:port(0x%x)->events=0x%x\n",port_id,port->events);
#endif
	if ((port->events & ASD_DISCOVERY_PROCESS) != 0) {
		if ((port->events & ASD_DISCOVERY_EVENT) != 0) {
			port->events &= ~ASD_DISCOVERY_EVENT;

			asd_run_state_machine(&port->dc.sm_context);
		}

		if ((port->events & ASD_DISCOVERY_PROCESS) != 0) {
			return 1;
		}
	}
//JD wait until previous discovery is finished to re-start a new one
	else
	{
		if (((port->events & ASD_DISCOVERY_REQ) != 0) || 
			((port->events & ASD_DISCOVERY_RETRY) != 0)) {
			if (asd_initiate_port_discovery(asd, port) == 0) {
				port->events |= ASD_DISCOVERY_PROCESS;
			/* 
			 * Only clear the event if it is handled successfully. 
			 */
				if ((port->events & ASD_VALIDATION_REQ) != 0) {
				/*
				 * Validate any targets if needed.
				 */
					if (!list_empty(&port->targets_to_validate))
						asd_configure_port_targets(asd, port);

					port->events &= ~ASD_VALIDATION_REQ;
				}

				port->events &= ~(ASD_DISCOVERY_REQ | 
					ASD_DISCOVERY_RETRY | ASD_VALIDATION_REQ);

				asd_do_discovery(asd, port);
			}
		}
	}

	if ((port->events & ASD_LOSS_OF_SIGNAL) != 0) {
		asd_handle_loss_of_signal(asd, port);
		port->events &= ~ASD_LOSS_OF_SIGNAL;
	}

	if ((port->events & ASD_VALIDATION_REQ) != 0) {
		/*
		 * Validate any targets if needed.
		 */
		if (!list_empty(&port->targets_to_validate))
			asd_configure_port_targets(asd, port);

		port->events &= ~ASD_VALIDATION_REQ;
	}

	return (0);
}

/*
 * Function:
 *	asd_configure_port_targets
 *
 * Description:
 *	Configure any new targets that have been added.
 *	Clean up targets that have been removed.
 */
static void
asd_configure_port_targets(struct asd_softc *asd, struct asd_port *port)
{
	struct asd_target	*targ;
	struct asd_target	*tmp_targ;
	struct list_head	 validate_list;
	u_long			 flags;
//JD
#ifdef ASD_DEBUG
	struct asd_phy *phy;
#endif
	INIT_LIST_HEAD(&validate_list);

	/*
	 * TODO:
	 * Access to the validate_list itself is protected, but we still need
	 * to make sure that the individual target's validate_links are
	 * protected.
	 */
	asd_list_lock(&flags);

	list_move_all(&validate_list, &port->targets_to_validate);

	asd_list_unlock(&flags);

	/* 
	 * We might be calling a routine that destroys the target, so use 
	 * the safe version of list traversal.
	 */
	list_for_each_entry_safe(targ, tmp_targ, &validate_list,
				 validate_links) {

		list_del(&targ->validate_links);
//JD
#ifdef ASD_DEBUG
	phy = list_entry(port->phys_attached.next, struct asd_phy, links);

	asd_log(ASD_DBG_INFO, "asd_configure_port_targets:port(0x%x) targ->flags(0x%x) port->phys_attached->id(0x%x)\n",port->id,targ->flags,phy->id);
#endif
		if (targ->flags & ASD_TARG_HOT_ADDED) {
			targ->flags &= ~ASD_TARG_HOT_ADDED;

			/*
			 * We only need to configure the target if it is
			 * a SAS or SATA END-DEVICE.
		      	 */	 
			if (targ->management_type == ASD_DEVICE_END) {
				asd_configure_target(asd, targ);
			} else {
				targ->flags |= ASD_TARG_ONLINE;
			}

		} else if (targ->flags & ASD_TARG_HOT_REMOVED) {
			if (targ->flags & ASD_TARG_MAPPED) {
				/*
				 * Tell the OS that the device is gone.
				 */
				asd_destroy_target(asd, targ);
			} else {
				/*
				 * This target is not exported to the OS,
				 * so we aren't going to report this device as
				 * missing.
				 */
				asd_free_target(asd, targ);
			}
		} else {
			asd_log(ASD_DBG_ERROR, "Invalid Target Flags.\n");
		}
	}
}

/*
 * Function:
 *	asd_configure_target()
 *
 * Description:
 *	Configure target that was HOT-ADDED. 
 */ 
static void
asd_configure_target(struct asd_softc *asd, struct asd_target *targ)
{
	u_int	ch;
	u_int	id;
	u_int	lun;

	ch = targ->src_port->id;
	id = targ->target_id;
	/* TODO : Currently the lun is set 0. This needs to be fixed. */
	lun = 0;

	/* Report the new target found to the user or OS. */
	asd_print("New device attached at "
		  "Host: scsi%d Channel: %d Id: %d Lun: %d\n",
		  asd->platform_data->scsi_host->host_no, ch, id, lun);
	
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
	/*
	 * For 2.6 kernel, notify the scsi layer that a new had been
	 * device added.
	 */
	scsi_add_device(asd->platform_data->scsi_host, ch, id, lun);
#else
	if (proc_scsi != NULL)
	{
		struct proc_dir_entry	*entry;
		char			 buffer[80];
		mm_segment_t		 orig_addr_limit;

		for (entry = proc_scsi->subdir; entry; entry = entry->next) {
			if ((entry->low_ino == 0) || (entry->namelen != 4) ||
				(memcmp ("scsi", entry->name, 4) != 0)) {
				continue;
			}
			if (entry->write_proc == NULL)
				continue;

			sprintf(buffer,"scsi add-single-device %d %d %d %d\n",
				asd->platform_data->scsi_host->host_no, ch, id, lun);

			orig_addr_limit = current->addr_limit;
			current->addr_limit = KERNEL_DS;
			entry->write_proc(NULL, buffer, strlen(buffer), NULL);
			current->addr_limit = orig_addr_limit;
		}
	}
#endif
	targ->flags |= (ASD_TARG_ONLINE | ASD_TARG_MAPPED);
}

/*
 * Function:
 *	asd_destroy_target()
 *
 * Description:
 *	Destroy target that was HOT-REMOVED.
 */ 
static void
asd_destroy_target(struct asd_softc *asd, struct asd_target *targ)
{
	u_long		flags;
	unsigned	num_luns;
	unsigned	i;

	/*
	 * For an end-device, we need to free the device and report 
	 * the change to the user or scsi layer.
	 */
	if (targ->management_type != ASD_DEVICE_END) {
		asd_lock(asd, &flags);
		/* Free the DDB site used by this target. */
		asd_free_ddb(asd, targ->ddb_profile.conn_handle);
		/*
		 * For non end-device, nothing much need to be done.
		 * All we need to do is free up the target.
		 */
		asd_free_target(asd, targ);
		asd_unlock(asd, &flags);
		return;
	}

	switch (targ->command_set_type) {
	case ASD_COMMAND_SET_SCSI:
		num_luns = targ->scsi_cmdset.num_luns;
		break;

	case ASD_COMMAND_SET_ATA:
	case ASD_COMMAND_SET_ATAPI:
	case ASD_COMMAND_SET_BAD:
		num_luns = 1;
		break;

	default:
		return;
	}

	for (i = 0 ; i < num_luns ; i++)
		asd_init_dev_itnl_exp(asd, targ, i);
}

static void
asd_init_dev_itnl_exp(struct asd_softc *asd, struct asd_target *targ,
		      u_int lun)
{
	struct asd_device *dev;
	u_long		   itnl_timeout;
	u_long		   flags;

	asd_lock(asd, &flags);

	dev = targ->devices[lun];
	if (dev == NULL)
		panic("Dev is corrupted.\n");

	itnl_timeout = ((targ->ddb_profile.itnl_const/1000) + 2) * HZ;

	asd_setup_dev_timer(dev, itnl_timeout, asd_dev_intl_times_out);

	asd_unlock(asd, &flags);
}

void
asd_dev_intl_times_out(u_long arg)
{
	struct asd_softc 	*asd;
	struct asd_device	*dev;
	u_long			 flags;

	dev = (struct asd_device *) arg;
	asd = dev->target->softc;

	asd_lock(asd, &flags);
	dev->flags &= ~ASD_DEV_TIMER_ACTIVE;

	/*
	 * It seems that aftet ITNL timer expired, there are still 
	 * outstanding IO(s) pending with the firmware.
	 */
	if (dev->active > 0) {
		/*
		 * Request the firmware to abort all IO(s) pending on
		 * its queue.
		 */
		asd_log(ASD_DBG_ERROR, "ITNL expired, DEV is still ACTIVE.\n");

		dev->flags |= ASD_DEV_DESTROY_WAS_ACTIVE;
		asd_clear_device_io(asd, dev);
	} else {
		/*
		 * No more active IO(s). Schedule a deferred process to
		 * destroy the device.
		 */
		asd_setup_dev_dpc_task(dev, asd_destroy_device);
	}

	asd_unlock(asd, &flags);
}

static void
asd_clear_device_io(struct asd_softc *asd, struct asd_device *dev)
{
	struct scb	*scb;
	
	/*
	 * Send a request to the firmware to abort all IOs pending on its
	 * queue that are intended for the target.
	 */	 
	if ((scb = asd_hwi_get_scb(asd, 1)) == NULL) {
		asd_log(ASD_DBG_ERROR, "Failed to get free SCB "
			"for CLEARING firmware queue.\n");
		return;
 	}

	scb->platform_data->dev = dev;
	scb->platform_data->targ = dev->target;

	asd_hwi_build_clear_nexus(scb, CLR_NXS_I_T_L,
				 (NOT_IN_Q | SEND_Q | EXEC_Q),
				  ASD_TARG_HOT_REMOVED);
				
	scb->flags |= (SCB_INTERNAL | SCB_ACTIVE | SCB_RECOVERY);
	asd_push_post_stack(asd, scb, (void *) dev, asd_clear_device_io_done);
	asd_hwi_post_scb(asd, scb);
}

static void
asd_clear_device_io_done(struct asd_softc *asd, struct scb *scb,
			 struct asd_done_list *dl)
{
	asd_log(ASD_DBG_ERROR, "DL Opcode = 0x%x.\n", dl->opcode);

	asd_hwi_free_scb(asd, scb);
}

void
#if defined(__VMKLNX__)
asd_destroy_device(struct work_struct *arg)
#else /* !defined(__VMKLNX__) */
asd_destroy_device(void *arg)
#endif /* defined(__VMKLNX__) */
{
	struct asd_softc	*asd;
	struct asd_device	*dev;
	struct scsi_cmnd	*cmd;
	union asd_cmd		*acmd;
	u_long	 		 flags;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,5,0)
	uint8_t			ch;
	uint8_t			id;
	uint8_t			lun;
#endif

#if defined(__VMKLNX__)
	dev = container_of(arg, struct asd_device, workq);
#else /* !defined(__VMKLNX__) */
	dev = (struct asd_device *) arg;
#endif /* defined(__VMKLNX__) */

	asd = dev->target->softc;

	asd_lock(asd, &flags);
	
	dev->flags &= ~ASD_DEV_DPC_ACTIVE;
	/*
	 * Prior to free up the device, make sure no IOs are
	 * pending on the device queue.
	 * Return all pending IOs to the scsi layer.
	 */
	while (!list_empty(&dev->busyq)) {
		acmd = list_entry(dev->busyq.next, 
				  union asd_cmd, acmd_links);
		list_del(&acmd->acmd_links);
		cmd = &acmd_scsi_cmd(acmd);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
		asd_cmd_set_host_status(cmd, DID_NO_CONNECT);
#else
		asd_cmd_set_offline_status(cmd);
#endif
		cmd->scsi_done(cmd);
	}

	/* 
	 * If the device has been exposed to the scsi layer, 
	 * we need to notify the scsi layer that the device has
	 * been removed and free up the the device.
	 */
	asd_print("Device attached at ");
	asd_print("Host: scsi%d Channel: %d Id: %d Lun: %d "
		  "has been removed.\n",
		  asd->platform_data->scsi_host->host_no, dev->ch,
		  dev->target->target_id, dev->lun);

	if ((dev->flags & ASD_DEV_UNCONFIGURED) != 0) {
		/*
		 * The device has not been exposed to the scsi 
		 * layer yet, all we need to do is free up the device.
		 */
		asd_free_device(asd, dev);
		asd_unlock(asd, &flags);
		return;
	}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,6)
	if ((dev->flags & ASD_DEV_DESTROY_WAS_ACTIVE) == 0)
		scsi_device_set_state(dev->scsi_device, SDEV_OFFLINE);
#endif	
	/*
	 * For 2.6 kernel, notify the scsi layer that
	 * a device had been removed. Ends up in asd_slave_destroy().
	 */
	asd_unlock(asd, &flags);

	scsi_remove_device(dev->scsi_device);
#else

#if 0
	/*
	 * This code hase been removed because it prevents hot remove from
	 * working.  Orginally, it was intended to force the sd layer to 
	 * stop sending requests to the adp94xx driver.
	 */
	if ((dev->flags & ASD_DEV_DESTROY_WAS_ACTIVE) == 0)
		dev->scsi_device->online = 0;
#endif
	ch=dev->ch;
	id=dev->id;
	lun=dev->lun;

	if (dev->target->refcount == 1) {
		/*
		 * Last device attached on this target.
		 * Free the target's DDB site.
		 */
		asd_free_ddb(asd, dev->target->ddb_profile.conn_handle);
	}
		
	/* 
	 * The device is no longer active.
	 * It is safe to free up the device.
	 */
	asd_free_device(asd, dev);

	asd_unlock(asd, &flags);
#ifdef ASD_DEBUG
	asd_log(ASD_DBG_INFO, "going to send /proc/scsi/scsi\n");
#endif
	if (proc_scsi != NULL)
	{
		struct proc_dir_entry	*entry;
		char			 buffer[80];
		mm_segment_t		 orig_addr_limit;

		for (entry = proc_scsi->subdir; entry; entry = entry->next) {
			if ((entry->low_ino == 0) || (entry->namelen != 4) ||
				(memcmp ("scsi", entry->name, 4) != 0)) {
				continue;
			}
			if (entry->write_proc == NULL)
				continue;

			sprintf(buffer,"scsi remove-single-device %d %d %d %d\n",
				asd->platform_data->scsi_host->host_no,ch,id,lun);
			orig_addr_limit = current->addr_limit;
			current->addr_limit = KERNEL_DS;
			entry->write_proc(NULL, buffer, strlen(buffer), NULL);
			current->addr_limit = orig_addr_limit;
		}
	}
#ifdef ASD_DEBUG
	asd_log(ASD_DBG_INFO, "sent to /proc/scsi/scsi\n");
#endif
#endif
}

static void
asd_handle_loss_of_signal(struct asd_softc *asd, struct asd_port *port)
{
	struct asd_target	*targ;
	struct asd_target	*multipath_target;
	struct asd_phy		*phy;
	struct asd_phy		*tmp_phy;
	u_long			 flags;
	int			 num_phy_online;

	phy = NULL;
	num_phy_online = 0;

	asd_lock(asd, &flags);

	/*
	 * We have lost signal from the port to the attached device.
       	 * If the port is a narrow port, we need to have all the attached
	 * targets validated.
	 * If the port is a wide port, we need to check if there is other 
	 * link is still connected to the attached device. 
	 */	 
	if (port->num_phys > 1) {
		/* 
		 * Wide port. We can still functional if other links still
		 * up and attached to the end device;
		 */
		list_for_each_entry_safe(phy, tmp_phy,
					 &port->phys_attached, links) {

			if (phy->state == ASD_PHY_CONNECTED) {
				num_phy_online++;
				continue;
			}
			/* 
			 * We need to disassociate the phy that no longer
			 * belongs to this wide port.
			 */
			if ((phy->state == ASD_PHY_ONLINE)  || 
			    (phy->state == ASD_PHY_OFFLINE)) {
				list_del_init(&phy->links);
				phy->src_port = NULL;
				port->num_phys--;
				port->conn_mask &= ~(1 << phy->id);
			}
		}
	}

	if (num_phy_online == 0) {
		/* 
		 * Links are down from the port to the attached device. 
		 * All targets attached to the port need to be validated.
		 */
		while (!list_empty(&port->targets)) {
			targ = list_entry(port->targets.next,
					  struct asd_target,
					  all_domain_targets);

			/*
			 * If this is a multipath target, then move the device
			 * mapping before removing the device.
			 */
			if (!list_empty(&targ->multipath)) {

				multipath_target = list_entry(
					targ->multipath.next,
					struct asd_target, multipath);

				asd_remap_device(asd, targ, multipath_target);

				/*
				 * This device wasn't in the target list
				 * because it was a multipath.  Add it to the
				 * list.
				 */
				list_add_tail(
					&multipath_target->all_domain_targets,
					&multipath_target->src_port->targets);

				targ->flags &= ~ASD_TARG_MAPPED;
			}

			list_del_init(&targ->all_domain_targets);
			list_del_init(&targ->children);
			list_del_init(&targ->siblings);
			list_del_init(&targ->multipath);
			targ->flags |= ASD_TARG_HOT_REMOVED;
			list_add_tail(&targ->validate_links,
				      &port->targets_to_validate);
		}
		INIT_LIST_HEAD(&port->targets);
		port->events |= ASD_VALIDATION_REQ;
		/*
		 * We need to disassociate all the attached phys from this
		 * port.
		 */
		while (!list_empty(&port->phys_attached)) {
			phy = list_entry(port->phys_attached.next,
					 struct asd_phy, links);
			list_del_init(&phy->links);
			phy->src_port = NULL;
			port->num_phys--;
			port->conn_mask = 0;
			port->state = ASD_PORT_UNUSED;
		}
	}

	asd_unlock(asd, &flags);
}

/* 
 * parse thro' list of all devices discovered and retireve target by 
 * sas address.
 */
struct asd_target * 
asd_get_sas_target_from_sasaddr(struct asd_softc *asd, struct asd_port *port, 
				uint8_t *sasaddr)
{
	struct asd_target 	*target;
	u_long			 flags;
	
	if ((asd == NULL) || (port == NULL) || (sasaddr == NULL)) {
		return NULL;
	}

	asd_lock(asd, &flags);
	list_for_each_entry(target, &port->targets, all_domain_targets) {
		if (memcmp(target->ddb_profile.sas_addr, 
			   sasaddr, 
			   SAS_ADDR_LEN) == 0) {
			asd_unlock(asd, &flags);
			return target;
		}
	}	
	asd_unlock(asd, &flags);

	return NULL;
}

/* parse thro' devices exported to os and retrieve target by SAS address */
struct asd_target * 
asd_get_os_target_from_sasaddr(struct asd_softc *asd, struct asd_domain *dm,
			       uint8_t *sasaddr)
{
	struct asd_target 	*target;
	int 			 i;
	u_long 			 flags;
	
	if ((asd == NULL) || (dm == NULL))
		return NULL;
	
	asd_lock(asd, &flags);
	for (i = 0; i < ASD_MAX_TARGET_IDS; i++) {
		if ((target = dm->targets[i]) != NULL) {
			if (memcmp(target->ddb_profile.sas_addr, 
			   	   sasaddr, 
			   	   SAS_ADDR_LEN) == 0) {
				asd_unlock(asd, &flags);
				return target;
			}
		}
	}
	asd_unlock(asd, &flags);
	return NULL;
}

/* parse thro' devices exported to os and retrieve target by port 
 * This routine will have to be discarded soon.
 * */
struct asd_target * 
asd_get_os_target_from_port(struct asd_softc *asd, struct asd_port *port, 
			    struct asd_domain *dm)
{
	int i;
	u_long flags;
	
	if ((asd == NULL) || (dm == NULL) || (port == NULL))
		return NULL;
	
	asd_lock(asd, &flags);
	for (i = 0; i < ASD_MAX_TARGET_IDS; i++) {
		if ((dm->targets[i] != NULL) 
		   && (dm->targets[i]->src_port == port)) {
			asd_unlock(asd, &flags);
			return dm->targets[i];
		}
	}
	asd_unlock(asd, &flags);

	return NULL;
}

struct asd_device *
asd_get_device_from_lun(struct asd_softc *asd, struct asd_target *targ, 
			uint8_t *saslun)
{
	int 	k;
	u_long 	flags;
	
	if ((asd == NULL) || (targ == NULL)) 
		return NULL;

	if ((targ->transport_type == ASD_TRANSPORT_STP) || 
	    (targ->transport_type == ASD_TRANSPORT_ATA)) {
		return targ->devices[0];
	}

	if (saslun == NULL)
		return NULL;

	asd_list_lock(&flags);
	for (k = 0; k < ASD_MAX_LUNS; k++) {
		if (targ->devices[k]) {
			if (memcmp(&targ->devices[k]->saslun, 
			    saslun, 8) == 0) {
				asd_list_unlock(&flags);
				return targ->devices[k];
			}
		}
	}
	asd_list_unlock(&flags);

	return NULL;
}

int 
asd_get_os_platform_map_from_sasaddr(struct asd_softc *asd, 
				     struct asd_port *port, 
				     uint8_t *sasaddr, uint8_t *saslun, 
				     uint8_t *host, uint8_t *bus, 
				     uint8_t *target, uint8_t *lun)
{
	Scsi_Device 		*scsi_device;
	struct asd_domain	*dm;
	struct asd_target	*targ;
	struct asd_device	*dev;
	struct Scsi_Host 	*scsi_host;
	
	dm = asd->platform_data->domains[port->id];
	if (dm == NULL)
		return 0;
	
	if ((targ = 
		asd_get_os_target_from_sasaddr(asd, dm, sasaddr)) == NULL)
		return -ENODEV;

	if ((dev = asd_get_device_from_lun(asd, targ, saslun)) == NULL)	
		return -ENODEV;

	scsi_device = dev->scsi_device;
	scsi_host = asd->platform_data->scsi_host;

	*host = scsi_host->host_no;
	*target = scsi_device->id;
	*bus = scsi_device->channel;
	*lun = scsi_device->lun;

	return 0;
}

struct asd_port *
asd_get_sas_addr_from_platform_map(struct asd_softc *asd, 
				   uint8_t host, uint8_t bus, uint8_t target, 
				   uint8_t lun, uint8_t *sasaddr,
				   uint8_t *saslun)
{
	struct asd_domain	*dm;
	struct asd_target	*targ;
	struct asd_device	*dev;
	struct Scsi_Host 	*scsi_host;
	struct asd_port		*port;
	u_long 			 flags;

	asd_lock(asd, &flags);

	port = NULL;
	scsi_host = asd->platform_data->scsi_host;
	if ((scsi_host == NULL) || 
	   ((host != 0xFF) && (host != scsi_host->host_no)))
		goto exit;

	if ((bus >= asd->platform_data->num_domains) ||
	    ((dm = asd->platform_data->domains[bus]) == NULL))
		goto exit;

#if defined(__VMKLNX__)
/*
 * ASD_MAX_TARGET_IDS is set to be 256, target is a uint8_t,
 * the first comparison is always false.
 * Commented out to satisfy the new gcc.
 */
	if ((targ = dm->targets[target]) == NULL)
#else
	if ((target >= ASD_MAX_TARGET_IDS) ||
	    ((targ = dm->targets[target]) == NULL))
#endif /* #if defined(__VMKLNX__) */
		goto exit;

	if (lun >= ASD_MAX_LUNS)
		goto exit;

	if ((dev = targ->devices[lun]) == NULL)
		goto exit;

	/* Get LUN address. */
	memcpy(saslun, dev->saslun, SAS_LUN_LEN);
	/* Get SAS address. */
	memcpy(sasaddr, targ->ddb_profile.sas_addr, SAS_ADDR_LEN);

	port = targ->src_port;

exit:	
	asd_unlock(asd, &flags);
	return (port);
}

struct scb *
asd_find_pending_scb_by_qtag(struct asd_softc *asd, uint32_t qtag)
{
	struct scb 		*list_scb;
	struct scb 		*scb;
	struct scsi_cmnd 	*cmd;
	union asd_cmd 		*acmd;
	u_long 			flags;
	
	list_scb = NULL;
	scb = NULL;
	cmd = NULL;
	acmd = NULL;

	asd_list_lock(&flags);
	list_for_each_entry(list_scb, &asd->platform_data->pending_os_scbs,
			    owner_links) {
		acmd = list_scb->io_ctx;
		cmd = &acmd_scsi_cmd(acmd);
		if (cmd->tag == qtag) {
			scb = list_scb;
			break;
		}
	}
	asd_list_unlock(&flags);
	return scb;
}

#define GET_LEAST_PHY_ID(port, kid) {\
	struct asd_phy	*list_phy; \
	kid = asd->hw_profile.max_phys; \
	list_for_each_entry(list_phy, &port->phys_attached, links) { \
		if(list_phy->id < kid) kid = list_phy->id; \
	} \
}

/*
 * Function:
 * 	get_port_by_least_phy()
 *
 * Description:
 * 	sort and get the port with smallest phy id
 */
struct asd_port	*get_port_by_least_phy(struct asd_softc *asd, int portid)
{
	int phyidN,phyidN1;
	struct asd_port *portN;
	struct asd_port *portN1;

	if(portid>=asd->hw_profile.max_ports) return NULL;
	portN = asd->port_list[portid];
	portN1 = get_port_by_least_phy(asd, portid+1);
	if( (portN->state == ASD_PORT_UNUSED) ||
	    (portN->num_phys == 0) ) return portN1;
	if( portN1 == NULL ) return portN;

	GET_LEAST_PHY_ID(portN, phyidN);
	GET_LEAST_PHY_ID(portN1, phyidN1);
#ifdef ASD_DEBUG
	asd_log(ASD_DBG_INFO, "port[%d](%d)->phy(%d) vs port(%d)->phy(%d)\n",portid,portN->id,phyidN,portN1->id,phyidN1);
#endif
	if( phyidN1 < phyidN )
	{
		struct asd_domain	*dm;
		struct asd_target	*target;
		int i;

		asd->port_list[portid]=portN1;
		asd->port_list[portN1->id]=portN;
		portN->id = portN1->id;
		portN1->id = portid;

		dm = asd->platform_data->domains[portN->id];
		asd->platform_data->domains[portN->id]=asd->platform_data->domains[portN1->id];
		asd->platform_data->domains[portN1->id]=dm;

		list_for_each_entry(target, &portN->targets, all_domain_targets) {
			if(target!=NULL)
			{
				struct asd_device *devicechain;
#ifdef ASD_DEBUG
				asd_log(ASD_DBG_INFO, "target %p: \n",target);
#endif
				for(i=0;i<ASD_MAX_LUNS;i++)
				{
					devicechain=target->devices[i];
					if(devicechain!=NULL)
					{
#ifdef ASD_DEBUG
						asd_log(ASD_DBG_INFO, "target->devices[%d]->ch was %d, now %d\n",i,devicechain->ch,portN->id);
#endif
						devicechain->ch = portN->id;
					}
				}
			}
		}
		list_for_each_entry(target, &portN1->targets, all_domain_targets) {
			if(target!=NULL)
			{
				struct asd_device *devicechain;
#ifdef ASD_DEBUG
				asd_log(ASD_DBG_INFO, "target %p: \n",target);
#endif
				for(i=0;i<ASD_MAX_LUNS;i++)
				{
					devicechain=target->devices[i];
					if(devicechain!=NULL)
					{
#ifdef ASD_DEBUG
						asd_log(ASD_DBG_INFO, "target->devices[%d]->ch was %d, now %d\n",i,devicechain->ch,portN1->id);
#endif
						devicechain->ch = portN1->id;
					}
				}
			}
		}

#ifdef ASD_DEBUG
		if(dm != NULL)
		{
			for(i=0;i<ASD_MAX_TARGET_IDS;i++)
			{
				if(dm->targets[i] !=NULL)
					asd_log(ASD_DBG_INFO, "target %d: %p: \n",i,dm->targets[i]);
			}
		}
#endif
		portN1 = get_port_by_least_phy(asd, portid+1);
	}
	return asd->port_list[portid];
}

/*
 * Function:
 * 	asd_process_id_addr_evt()
 *
 * Description:
 * 	Process Identify Address frame received by the phy. 
 * 	If the phy has no src port, associate the phy with a port.
 *	Otherwise, validate if the phy still belongs to the same src port.
 *	Trigger discovery as needed.
 */
static void
asd_process_id_addr_evt(struct asd_softc *asd, struct asd_phy *phy)
{
	struct asd_port	*port;
	struct asd_phy	*list_phy;
	int		 wide_port;
	int		 prev_attached_phy;
	int		 port_no;
	struct asd_port	*dummyport;

	ASD_LOCK_ASSERT(asd);
	port = NULL;
	prev_attached_phy = 0;
	wide_port = 0;
//JD
#ifdef ASD_DEBUG
	asd_log(ASD_DBG_INFO, "id_addr from phy 0x%x\n",phy->id);
#endif
	/*
	 * Check existing ports whether the current phy attached to the
         * port has the same ID ADDR as this new phy.
	 * If so, this phy shall be associated to the port and aggregate
	 * as a wide port if it a different phy.
	 * Else, if it is the same phy then it must be after enable-phy.
	 */
	for (port_no = 0; port_no < asd->hw_profile.max_ports; port_no++) {
		port = asd->port_list[port_no];
//JD
#ifdef ASD_DEBUG
		asd_log(ASD_DBG_INFO, "asd_process_id_addr_evt: port(0x%x)->state(0x%x)\n", port->id,port->state );
#endif

		if (port->state != ASD_PORT_ONLINE) {
			continue;
		}

		if (port->link_type != ASD_LINK_SAS) {
			/*
			 * For SATA link, check if the phy is the same as
		         * the one currently attached to the port.
			 */
			list_phy = list_entry(port->phys_attached.next,
					      struct asd_phy, links);
			if (list_phy->id == phy->id)
				prev_attached_phy = 1;

			continue;
		}
//JD
#ifdef ASD_DEBUG
		asd_log(ASD_DBG_INFO, "asd_process_id_addr_evt: port_id(0x%x) port->attached_sas_addr %02x%02x%02x%02x%02x%02x%02x%02x\n", port->id,
			port->attached_sas_addr[0],
			port->attached_sas_addr[1],
			port->attached_sas_addr[2],
			port->attached_sas_addr[3],
			port->attached_sas_addr[4],
			port->attached_sas_addr[5],
			port->attached_sas_addr[6],
			port->attached_sas_addr[7]		);
#endif
		list_for_each_entry(list_phy, &port->phys_attached, links) {
			/*
			 * Check the id_addr that we just got in.
			 */
			if (memcmp(PHY_GET_ID_SAS_ADDR(list_phy),
	       			   PHY_GET_ID_SAS_ADDR(phy), 
				   SAS_ADDR_LEN) != 0) {

				continue;
			}
			/*
			 * Check the sas address that assigned by the BIOS
			 */
			if (memcmp(list_phy->sas_addr, phy->sas_addr, 
				   SAS_ADDR_LEN) != 0) {
				continue;
			}
			/*
			 * Check if this is the same phy that was previously
			 * attached to this port.
			 * The new phy can only become a wide port if it isn't
			 * previously attached to this port.
		         */	 
			if (list_phy->id == phy->id)
				prev_attached_phy = 1;
			else
				wide_port = 1;
			
			break;
		}

		if ((wide_port == 1) || (prev_attached_phy == 1))
			break;
	}

	if (prev_attached_phy == 0) {
	/*
	 * If the phy is already on a list, take it off.
	 */
		if (!list_empty(&phy->links))
		{
			list_del_init(&phy->links);
			phy->src_port->num_phys--;
			phy->src_port->conn_mask &= ~(1 << phy->id);
			phy->src_port = NULL;
		}
	}
//JD
#ifdef ASD_DEBUG
	asd_log(ASD_DBG_INFO, "asd_process_id_addr_evt: prev_attached_phy=0x%x \n",prev_attached_phy);
#endif

	if ((wide_port == 0) && (prev_attached_phy == 0)) {
		/*
		 * The phy is not associated with any port. 
		 * Create a new port for this phy.
		 */
		for (port_no = 0; port_no < asd->hw_profile.max_ports; 
		     port_no++) {
			port = asd->port_list[port_no];
			
			if ((port->state == ASD_PORT_UNUSED) && 
			    (port->num_phys == 0))
				break;
		}
//JD
#ifdef ASD_DEBUG
	asd_log(ASD_DBG_INFO, "asd_process_id_addr_evt: assigned port data port_id(0x%x) port->attached_sas_addr %02x%02x%02x%02x%02x%02x%02x%02x\n", port->id,
			port->attached_sas_addr[0],
			port->attached_sas_addr[1],
			port->attached_sas_addr[2],
			port->attached_sas_addr[3],
			port->attached_sas_addr[4],
			port->attached_sas_addr[5],
			port->attached_sas_addr[6],
			port->attached_sas_addr[7]		);
#endif
		memcpy(port->sas_addr, phy->sas_addr, SAS_ADDR_LEN);
		/* 
		 * Generate SAS hashed address to be used as the port 
		 * SAS address. 
		 */
		asd_hwi_hash(port->sas_addr, port->hashed_sas_addr);

		/* 
		 * During enable phy, we have determined whether we have a 
		 * direct attached SATA device.
		 */  
		if (phy->attr & ASD_SATA_MODE) {
			/*
			 * TODO: More things to do for SATA II device which 
			 *	 supports tag queueing. Need to setup sister
			 *	 ddb, other additional SATA information. Need
			 *	 to add SATA specific fields in target DDB
			 *	 profile.
			 */
			port->link_type = ASD_LINK_SATA;
			port->management_type = ASD_DEVICE_END;

		} else if (phy->bytes_dmaed_rcvd.id_addr_rcvd.addr_frame_type & 
			SAS_END_DEVICE) {
			/*
			 * Direct Attached SAS device.
			 */
			port->link_type = ASD_LINK_SAS;
			port->management_type = ASD_DEVICE_END;

		} else if (phy->bytes_dmaed_rcvd.id_addr_rcvd.addr_frame_type & 
			SAS_EDGE_EXP_DEVICE) {
			/*
			 * Edge Expander device.
			 */
			port->link_type = ASD_LINK_SAS;
			port->management_type = ASD_DEVICE_EDGE_EXPANDER;
		} else {
			/*
			 * Fanout Expander device
			 */
			if (phy->bytes_dmaed_rcvd.id_addr_rcvd.addr_frame_type & 
				SAS_FANOUT_EXP_DEVICE) {

				port->link_type = ASD_LINK_SAS;
				port->management_type =
					ASD_DEVICE_FANOUT_EXPANDER;
			}
		}

		port->state = ASD_PORT_ONLINE;
	}

	phy->src_port = port;

	if (prev_attached_phy == 0) {
//JD
#ifdef ASD_DEBUG
	asd_log(ASD_DBG_INFO, "asd_process_id_addr_evt: port_id(0x%x) phy_id(0x%x)\n", port->id, phy->id);
#endif
		list_add_tail(&phy->links, &port->phys_attached);
		port->num_phys++;
	}
	asd_setup_port_data(asd, port, phy);
//JD not really need the port, but just sort it...
	if(asd->platform_data->flags & ASD_DISCOVERY_INIT)
		dummyport = get_port_by_least_phy(asd, 0);
//JD
#ifdef ASD_DEBUG
	asd_log(ASD_DBG_INFO, "asd_process_id_addr_evt: after asd_setup_port_data port_id(0x%x) phy_id(0x%x)\n", port->id, phy->id);


//JD
	asd_log(ASD_DBG_INFO, "asd_process_id_addr_evt: port->events(0x%x)\n",port->events);
#endif

	/*
	 * Check to see if the port already has discovery running on it.
	 */
	if (((port->events & ASD_DISCOVERY_REQ) == 0) &&
	    ((port->events & ASD_DISCOVERY_PROCESS) == 0)) {
		/*
		 * The port does not have a discovery running on it.
		 */
		port->events |= ASD_DISCOVERY_REQ;

		port->conn_mask |= (1 << phy->id);

		asd->num_discovery++;

	} else {
		/*
		 * We should only get here if this is a wide port.  We will not
		 * mark this phy in the connection mask yet, the discovery
		 * thread will do that when it is finished.
		 */
//JD		ASSERT(wide_port == 1);
	}
}

/*
 * Function:
 *	asd_setup_port_data()
 *
 * Description:
 *	Setup port settings based on the phy info.
 */
static void 
asd_setup_port_data(struct asd_softc *asd, struct asd_port *port,
		    struct asd_phy *phy)
{
	u_int	lowest_rate;

	/* 
	 * If this port was previously disabled, change the port state to 
	 * ONLINE.   
	 */
	if (port->state != ASD_PORT_ONLINE)
		port->state = ASD_PORT_ONLINE;

	port->attr = phy->attr;	

	/* 
	 * Get the negotiated connection rate.
	 *
	 * Notice the two different definitions of connection rate.
	 * SAS_RATE_XXX is used on the OPEN ADDRESS FRAME.
	 * For wide port configuration, we need to use lowest link rate of 
	 * all attached phys as the port connection rate.
	 */
	lowest_rate = phy->conn_rate;
	if (port->num_phys > 1) {
		struct asd_phy	*list_phy;

		/* Wide port configuration. */
		list_for_each_entry(list_phy, &port->phys_attached, links) {

			if (list_phy->conn_rate < lowest_rate)
				lowest_rate = phy->conn_rate;
		}
	}

	port->conn_rate = ((lowest_rate == SAS_30GBPS_RATE) ? SAS_RATE_30GBPS :
							      SAS_RATE_15GBPS);
	{
		u8 phy_is_up;
		u8 conn_mask;
		u8 mask;
		int i;

		/* Setup the hardware DDB 0 location. */
		asd_hwi_set_ddbptr(asd, 0);


		conn_mask = port->conn_mask | (1<<phy->id);

		mask = conn_mask;
		/* turn on port_map_by_links */
		for (i = 0; mask != 0; i++, mask >>= 1)
			if (mask & 01) {
				asd_hwi_set_ddbsite_byte(asd,
							 offsetof(struct asd_int_ddb, port_map_by_links)+i,
							 conn_mask);
			}

		/* turn on port_map_by_ports */
		asd_hwi_set_ddbsite_byte(asd,
					 offsetof(struct asd_int_ddb, port_map_by_ports)+port->id,
					 conn_mask);

		/* turn on phy is up */
		phy_is_up = asd_hwi_get_ddbsite_byte(asd, offsetof(struct asd_int_ddb, phy_is_up));
		phy_is_up |= (1<<phy->id);
		asd_hwi_set_ddbsite_byte(asd, offsetof(struct asd_int_ddb, phy_is_up), phy_is_up);
	}
}

/*
 * Function:
 * 	asd_initiate_port_discovery()
 * 
 * Description:
 *	Check any events such as phy events, id_addr frame or dynamic 
 *	configuration changes that required discovery.
 */	   	  
static int
asd_initiate_port_discovery(struct asd_softc *asd, struct asd_port *port)
{
	struct asd_phy		*phy;
	struct asd_target 	*targ;
	struct asd_target	*multipath_target;
	uint64_t		 sas_addr;
	u_long			 flags;

	// TODO: fix locking - lists should be locked

	if (list_empty(&port->phys_attached)) {
		asd_log(ASD_DBG_ERROR, "Corrupted port, no phy(s) attached "
			"to it.\n");
		return (-1);
	}

	phy = list_entry(port->phys_attached.next, struct asd_phy, links);
//JD
#ifdef ASD_DEBUG
	asd_log(ASD_DBG_INFO, "asd_initiate_port_discovery: phy->id(0x%x)\n",phy->id);
	asd_log(ASD_DBG_INFO, "asd->free_ddb_bitmap ptr 0x%x\n",asd->free_ddb_bitmap);
	asd_print("%llx %llx\n",(u64)asd->free_ddb_bitmap[0], (u64)asd->free_ddb_bitmap[1]);
#endif
	if ((port->link_type == ASD_LINK_SATA) &&
	    (port->management_type = ASD_DEVICE_END)) {
		/*
	 	 * For direct-attached SATA end-device, generate the SAS addr
        	 * internally.
	 	*/
		sas_addr = asd_htobe64(asd_be64toh(*((uint64_t *)
						     asd->hw_profile.wwn))
				       + asd->hw_profile.sata_name_base
				       + phy->id);
	} else {
		sas_addr = (*(uint64_t *) PHY_GET_ID_SAS_ADDR(phy));
	}

	/*
	 * Look up this target to see if it already exists.
	 */
	targ = asd_find_target(&port->targets, (uint8_t *) &sas_addr);
	if (targ != NULL) {
		list_del_init(&targ->all_domain_targets);
		list_del_init(&targ->children);
		list_del_init(&targ->siblings);

		while (!list_empty(&targ->multipath)) {
			multipath_target = list_entry(targ->multipath.next,
					  struct asd_target, multipath);

			list_del_init(&multipath_target->multipath);

			list_add_tail(&multipath_target->multipath,
				&port->targets);
		}

		list_add_tail(&targ->all_domain_targets, &port->targets);
	} else {
		targ = asd_alloc_target(asd, port);
	}

	if (targ == NULL) {
		/* 
		 * TODO: Return for now.
		 *  	 Probably, we should return and put
		 *  	 the discovery thread back to sleep
		 *  	 and restart at some point later on. 
		 */
		asd_log(ASD_DBG_ERROR, "Failed to allocate a target !!\n");
		return (-1);
	}

	asd_lock(asd, &flags);
//JD
#ifdef ASD_DEBUG
		asd_log(ASD_DBG_INFO, "asd_initiate_port_discovery: before setup_target_data port_id(0x%x) port->attached_sas_addr %02x%02x%02x%02x%02x%02x%02x%02x\n", port->id,
			port->attached_sas_addr[0],
			port->attached_sas_addr[1],
			port->attached_sas_addr[2],
			port->attached_sas_addr[3],
			port->attached_sas_addr[4],
			port->attached_sas_addr[5],
			port->attached_sas_addr[6],
			port->attached_sas_addr[7]		);
#endif
	if (asd_setup_target_data(asd, phy, targ) != 0) {
#if defined(__VMKLNX__)
		asd_unlock(asd, &flags);
#endif
		asd_log(ASD_DBG_ERROR, "Failed to setup target data !!\n");
		return (-1);
	}

     	memcpy(port->attached_sas_addr, targ->ddb_profile.sas_addr,
	       SAS_ADDR_LEN);
//JD
#ifdef ASD_DEBUG
		asd_log(ASD_DBG_INFO, "asd_initiate_port_discovery: port_id(0x%x) port->attached_sas_addr %02x%02x%02x%02x%02x%02x%02x%02x\n", port->id,
			port->attached_sas_addr[0],
			port->attached_sas_addr[1],
			port->attached_sas_addr[2],
			port->attached_sas_addr[3],
			port->attached_sas_addr[4],
			port->attached_sas_addr[5],
			port->attached_sas_addr[6],
			port->attached_sas_addr[7]		);
#endif
	switch (port->link_type)
	{
	case ASD_LINK_SAS:
		targ->management_type = port->management_type;

		switch (port->management_type)
		{
		case ASD_DEVICE_END:
			/*
			 * It can't be an STP type because it is directly
			 * attached to the initiator.
			 */
			targ->command_set_type = ASD_COMMAND_SET_SCSI;
			targ->device_protocol_type = ASD_DEVICE_PROTOCOL_SCSI;
			targ->transport_type = ASD_TRANSPORT_SSP;
			break;

		case ASD_DEVICE_EDGE_EXPANDER:
			targ->command_set_type = ASD_COMMAND_SET_SMP;
			targ->device_protocol_type = ASD_DEVICE_PROTOCOL_SMP;
			targ->transport_type = ASD_TRANSPORT_SMP;
			break;

		case ASD_DEVICE_FANOUT_EXPANDER:
			targ->command_set_type = ASD_COMMAND_SET_SMP;
			targ->device_protocol_type = ASD_DEVICE_PROTOCOL_SMP;
			targ->transport_type = ASD_TRANSPORT_SMP;
			break;

		case ASD_DEVICE_NONE:
		case ASD_DEVICE_UNKNOWN:
			targ->command_set_type = ASD_COMMAND_SET_UNKNOWN;
			targ->device_protocol_type = 
				ASD_DEVICE_PROTOCOL_UNKNOWN;
			targ->transport_type = ASD_TRANSPORT_UNKNOWN;
			break;
		}
		break;

	case ASD_LINK_SATA:
		targ->command_set_type = ASD_COMMAND_SET_UNKNOWN;
		targ->device_protocol_type = ASD_DEVICE_PROTOCOL_ATA;
		targ->transport_type = ASD_TRANSPORT_ATA;
		targ->management_type = ASD_DEVICE_END;
		break;

	default:
		targ->command_set_type = ASD_COMMAND_SET_UNKNOWN;
		targ->device_protocol_type = ASD_DEVICE_PROTOCOL_UNKNOWN;
		targ->transport_type = ASD_TRANSPORT_UNKNOWN;
		targ->management_type = ASD_DEVICE_NONE;
		break;
	}

	port->tree_root = targ;

	asd_unlock(asd, &flags);

	return (0);
}	

/*
 * Function:
 * 	asd_setup_target_data()
 * 
 * Description:
 *	Setup target info based on the Identify Address frame received.
 * 	Also, setup a hardware ddb site for this target.
 */
static int
asd_setup_target_data(struct asd_softc *asd, struct asd_phy *phy,
		      struct asd_target *targ)
{
	uint64_t	sas_addr;

	ASD_LOCK_ASSERT(asd);

	/* Set the sister ddb to invalid for now. */
	targ->ddb_profile.sister_ddb = ASD_INVALID_DDB_INDEX;

	targ->ddb_profile.conn_rate = phy->src_port->conn_rate;

	/* Set a default ITNL timer, applicable for SAS device only. */
	targ->ddb_profile.itnl_const = ITNL_TIMEOUT_CONST;
		
	/* Setup target protocol. */
	if (phy->bytes_dmaed_rcvd.id_addr_rcvd.tgt_port_type & SSP_TGT_PORT) {

		targ->transport_type = ASD_TRANSPORT_SSP;
		targ->command_set_type = ASD_COMMAND_SET_SCSI;

	} else if (phy->bytes_dmaed_rcvd.id_addr_rcvd.tgt_port_type & 
		SMP_TGT_PORT) {

		targ->transport_type = ASD_TRANSPORT_SMP;
		targ->command_set_type = ASD_COMMAND_SET_SMP;

	} else if (phy->bytes_dmaed_rcvd.id_addr_rcvd.tgt_port_type & 
		STP_TGT_PORT) {
		/* 
		 * We don't know the command set yet (could be ATAPI or ATA)
		 * We won't know until IDENTIFY / PIDENTIFY.
		 */
		targ->transport_type = ASD_TRANSPORT_STP;
	} else {
		/* 
		 * We don't know the command set yet (could be ATAPI or ATA)
		 * We won't know until IDENTIFY / PIDENTIFY.
		 */
		targ->transport_type = ASD_TRANSPORT_ATA;
		targ->ddb_profile.itnl_const = 1;
	}

	if (targ->transport_type == ASD_TRANSPORT_ATA) {

		sas_addr =  asd_htobe64(asd_be64toh(*((uint64_t *)
			asd->hw_profile.wwn)) + 0x10 + phy->id);

		/*
		 * Setup the SAS Address for this target based on the
		 * generated SAS Address.
		 */
		memcpy(targ->ddb_profile.sas_addr, &sas_addr, SAS_ADDR_LEN);
	} else {
		/*
		 * Setup the SAS Address for this target based on the Identify
		 * Frame received for this phy.
		 */
		memcpy(targ->ddb_profile.sas_addr, PHY_GET_ID_SAS_ADDR(phy),
		       SAS_ADDR_LEN);
//JD
#ifdef ASD_DEBUG
		asd_log(ASD_DBG_INFO, "asd_setup_target_data: targ->ddb_profile.sas_addr(xxxx%02x%02x%02x)\n",
			targ->ddb_profile.sas_addr[5],
			targ->ddb_profile.sas_addr[6],
			targ->ddb_profile.sas_addr[7]);
#endif
	}

	asd_hwi_hash(targ->ddb_profile.sas_addr, 
		     targ->ddb_profile.hashed_sas_addr);
		
	/* 
	 * Based on the target port type, enable the OPEN bit so that 
	 * OPEN address will be issued when opening an connection.
	 */
	if ((phy->bytes_dmaed_rcvd.id_addr_rcvd.tgt_port_type & SSP_TGT_PORT) ||
		(phy->bytes_dmaed_rcvd.id_addr_rcvd.tgt_port_type & 
			SMP_TGT_PORT) ||
		(phy->bytes_dmaed_rcvd.id_addr_rcvd.tgt_port_type & 
			STP_TGT_PORT)) {

		targ->ddb_profile.open_affl = OPEN_AFFILIATION;
		if(phy->bytes_dmaed_rcvd.id_addr_rcvd.tgt_port_type & 
			STP_TGT_PORT)
		{
			targ->ddb_profile.open_affl |=
//		    (STP_AFFILIATION | SUPPORTS_AFFILIATION);
		    SUPPORTS_AFFILIATION;
		}
#ifdef SEQUENCER_UPDATE
#ifdef CONCURRENT_SUPPORT
		else 
		{
			targ->ddb_profile.open_affl |= CONCURRENT_CONNECTION_SUPPORT;
		}
#endif
#endif

		/*
		 * TODO: More to be done for STP in the case affiliation is
		 *	 supported.
	 	 */	    
	} else {
		/* 
		 * For direct attached SATA device and SATA Port Multi, 
		 * OPEN Address frame and afflitiation policy are not 
		 * supported.
		 */   
		targ->ddb_profile.open_affl = 0;
	}

	/* Setup a hardware DDB site for this target. */
	if (asd_hwi_setup_ddb_site(asd, targ) != 0) {
		/* 
		 * Failed to setup ddb site due to no free site.
		 * TODO: More handling needed here once ddb site recycling 
		 *	 algorithm is implemented.
		 *	 For now, just return failure.	 
		 */
		return (-1);
	}
	return (0);
}

/*
 * Function:
 * 	asd_map_target()
 *
 * Description:
 *	Mapped the target to the domain.
 */
int
asd_map_target(struct asd_softc *asd, struct asd_target *targ)
{
	struct asd_domain	*dm;
	struct asd_port		*port;
	u_int			 i;
	unsigned		 channel;

	port = targ->src_port;

	/*
	 * Map the target to the domain.
	 */
	if (targ->flags & ASD_TARG_MAP_BOOT) {
		targ->flags &= ~ASD_TARG_MAP_BOOT;
		channel = 0;
	} else {
		channel = port->id;
	}

	dm = asd->platform_data->domains[channel];

	if (dm == NULL) {
		dm = asd_alloc_domain(asd, channel);
		if (dm == NULL)
			return -1;
	}

	if (targ->target_id != ASD_MAX_TARGET_IDS) {
		asd_dprint("target %llx already has ID\n",
			   be64_to_cpu(*(u64 *)targ->ddb_profile.sas_addr));
		/* The target ID was assigned in the discover code,
		 * possibly via querying an SES device.
		 */
		if (dm->targets[targ->target_id] == NULL) {
			dm->targets[targ->target_id] = targ;
			targ->domain = dm;
		} else if (dm->targets[targ->target_id] != targ) {
			asd_dprint("Ooops: target with id %d already exists!\n",
				   targ->target_id);
			return -1;
		}
	} else {
		asd_dprint("target %llx doesn't have ID\n",
			   be64_to_cpu(*(u64 *)targ->ddb_profile.sas_addr));
		/* Normal case: no SES device claimed to know the order
		 * of this device.
		 */
#ifndef NO_SES_SUPPORT 
		for (i = 128; i < ASD_MAX_TARGET_IDS; i++) {
#else
		for (i = 0; i < (ASD_MAX_TARGET_IDS - 128); i++) {
#endif
			if (dm->targets[i] == NULL) {
				dm->targets[i] = targ;
				targ->target_id = i;
				targ->domain = dm;
				break;
			}
		}
		if (i == ASD_MAX_TARGET_IDS)
			return -1;
	}

	/* Increment domain ref count for new target mapped. */
	asd_domain_addref(dm);

	return (0);
}

#ifdef ASD_DEBUG
void
asd_init_debug_timeout(
u_long		val
)
{
	struct asd_softc		*asd;

	asd = (struct asd_softc	*)val;

	asd_hwi_dump_seq_state(asd, asd->hw_profile.enabled_phys);
}
#endif


/***************************** PCI Entry Points *******************************/
/* 
 * Function:
 *	asd_pci_dev_probe()
 *
 * Description:
 *	This routine will be called when OS finds a controller that matches
 *	and entry in our supported PCI ID table.  It will perform hardware
 *	initialization and bring our device to the online state. 
 */
static int
asd_pci_dev_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct asd_softc		*asd;
	struct asd_pci_driver_data	*drv_data;
	asd_dev_t			 dev;
	unsigned long			 flags;
	int				 error;

	asd_print("Probing Adaptec AIC-94xx Controller(s)...\n");
	
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	if (asd_init_stat.asd_init_state != 1) {
		asd_print("%s: Ignoring PCI device found after initialization. "
			  "\n", ASD_DRIVER_NAME);
		return (-ENODEV);
	}
#endif

	/* Sanity checking to make sure the same device is not probed twice. */
	asd_list_lock(&flags);
	list_for_each_entry(asd, &asd_hbas, link) {
		struct pci_dev *probed_pdev;

		probed_pdev = asd_pci_dev(asd);
		if ((probed_pdev->bus->number == pdev->bus->number) && 
		    (probed_pdev->devfn == pdev->devfn)) {
			/* A Duplicate PCI Device found. Ignore it. */
			asd_print("%s: Ignoring duplicate PCI device.\n", 
			  	  ASD_DRIVER_NAME);
			asd_list_unlock(&flags);
			return (-ENODEV);
		}
	}
	asd_list_unlock(&flags);

	drv_data = (struct asd_pci_driver_data *) (id->driver_data);
	if (drv_data == NULL) {
		asd_log(ASD_DBG_ERROR, "PCI Driver Data not found.\n");
		return (-ENODEV);
	}

	if (pdev->class == (PCI_CLASS_STORAGE_RAID << 8)
	    && !asd_attach_HostRAID)
		return -ENODEV;

	if (pci_enable_device(pdev) != 0)
		return (-ENODEV);	
	
	pci_set_master(pdev);
		
	dev = asd_pdev_to_dev(pdev);
	/* Allocate a softc structure for the current PCI Device found. */
	asd = asd_alloc_softc(dev);
	if (asd == NULL)
		return (-ENOMEM);

#ifdef ASD_DEBUG
	init_timer(&debug_timer);
	debug_timer.expires = jiffies + 20 * HZ;
	debug_timer.data = (u_long) asd;
	debug_timer.function = asd_init_debug_timeout;
	//add_timer(&debug_timer);
#endif
	
	asd->pci_entry = id;
	/*
	 * Perform profile setup for the specific controller.
	 * Always success.
	 */
	error = drv_data->setup(asd);
	
	asd->platform_data->domains = asd_alloc_mem(
					    (asd->hw_profile.max_ports *
					    sizeof(struct asd_domain)),
					    GFP_ATOMIC);

	if (asd->platform_data->domains == NULL) {
		asd_free_softc(asd);
		return (-ENOMEM);
	}
	memset(asd->platform_data->domains, 0x0, (asd->hw_profile.max_ports *
						  sizeof(struct asd_domain)));

	/*
	 * Setup PCI consistent dma transfer mask to 32-bit and below 
	 * 4GB boundary.
	 */
	if (asd_set_consistent_dma_mask(asd, 0xFFFFFFFF) != 0) {
		asd_log(ASD_DBG_ERROR, "Failed to set PCI consistent "
			"dma mask.\n");
		asd_free_softc(asd);
		return (-ENODEV);
	}	
	
	/*
	 * Setup the dma transfer mask to 64-bit only the following conditions
	 * are met:
	 *    (a) the system is running on 64-bit or 'PAE' mode.
	 *    (b) the controller can do DMA transfer above 4GB boundary.
	 *    (c) dma address can be above 4GB boundary.
	 * Otherwise set the dma transfer mask to 32-bit.
	 */
	 if ((asd->profile.dma64_addr == 1)	&& 
	     (ASD_DMA_64BIT_SUPPORT == 1)) {
		uint64_t    mask;
		
		mask = 0xFFFFFFFFFFFFFFFFULL;
	 	if (asd_set_dma_mask(asd, mask) != 0) {
			/* 
			 * Failed to set dma mask to 64-bit, throttle down
			 * to 32-bit instead.
			 */
			 mask = 0xFFFFFFFFULL;
			 if (asd_set_dma_mask(asd, mask) != 0) {
			 	/* If this also failed, we need to exit. */
				asd_log(ASD_DBG_ERROR, "Failed to set DMA "
					"mask. \n");
				asd_free_softc(asd);
				return (-ENODEV);
			}	 
	 	}
		asd->profile.dma_mask = (dma_addr_t) mask;		
	 } else {
	 	if (asd_set_dma_mask(asd, 0xFFFFFFFF) != 0) {
			asd_log(ASD_DBG_ERROR, "Failed to set DMA "
					"mask. \n");
			asd_free_softc(asd);
			return (-ENODEV);
		}
		asd->profile.dma_mask = 0xFFFFFFFF;
	}

	/* 
	 * TBD: Review locking. This is a single threaded operation and 
	 * 2.6.x kernel on a SMP machine complains about a potential 
	 * deadlock due to irqs being disabled. 
	 */
	/*asd_lock(asd, &flags);*/
	/* Initialize the controller. */
	if (asd_init_hw(asd) != 0) {
		//asd_unlock(asd, &flags);
		asd_log(ASD_DBG_ERROR, "Failed to initialize the HW.\n");
		asd_free_softc(asd);
		return (-ENODEV);
	} 	
	pci_set_drvdata(pdev, asd);
	/*asd_unlock(asd, &flags);*/

	asd_list_lock(&flags);
	list_add_tail(&asd->link, &asd_hbas);
	asd_list_unlock(&flags);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
	/* The controller is hot-plugged, register it with the SCSI midlayer. */
	if (asd_init_stat.asd_init_state == 0) {
		error = asd_register_host(asd);
		if (error != 0) {
			asd_log(ASD_DBG_ERROR, "Failed to register host.\n");
			asd_list_lock(&flags);
			list_del(&asd->link);
			asd_list_unlock(&flags);
			asd_free_softc(asd);
		}
	}
#endif
	if (!error)
		/* Enable the Host interrupt. */
		asd_intr_enable(asd, 1);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
	/* 
	 * For the controller that is hot-plugged, we need to request the 
	 * midlayer to perform bus scan.
	 */
	if (asd_init_stat.asd_init_state == 0) {
		error = asd_initiate_bus_scan(asd);
		if (error != 0) {
			asd_log(ASD_DBG_ERROR, "Failed in performing "
				"bus scan.\n");
			asd_list_lock(&flags);
			list_del(&asd->link);
			asd_list_unlock(&flags);
			asd_free_softc(asd);
		}
	}	
#endif

	return (error);
}

/* 
 * Function:
 *	asd_pci_dev_remove()
 *
 * Description:
 *	This routine is called when the controller is removed or during 
 *	module unloading. 
 */
static void
asd_pci_dev_remove(struct pci_dev *pdev)
{
	struct asd_softc	*asd;
	unsigned long		 flags;
	
	asd = (struct asd_softc *) pci_get_drvdata(pdev);
	
	if (asd_get_softc(asd) != NULL) {
		asd_list_lock(&flags);	
		list_del(&asd->link);
		asd_list_unlock(&flags);
		asd_free_softc(asd);
	}
}

int
asd_platform_alloc(struct asd_softc *asd)
{
	asd->platform_data = asd_alloc_mem(sizeof(struct asd_platform_data),
					   GFP_ATOMIC);
	if (asd->platform_data == NULL)
		return (-ENOMEM);
	memset(asd->platform_data, 0, sizeof(struct asd_platform_data));

	asd_lock_init(asd);
	INIT_LIST_HEAD(&asd->platform_data->pending_os_scbs);
	INIT_LIST_HEAD(&asd->platform_data->device_runq);
	INIT_LIST_HEAD(&asd->platform_data->completeq);
	INIT_LIST_HEAD(&asd->platform_data->lru_ddb_q);
	init_MUTEX_LOCKED(&asd->platform_data->discovery_sem);
	init_MUTEX_LOCKED(&asd->platform_data->discovery_ending_sem);
	init_MUTEX_LOCKED(&asd->platform_data->ehandler_sem);
	init_MUTEX_LOCKED(&asd->platform_data->ehandler_ending_sem);
	init_MUTEX_LOCKED(&asd->platform_data->eh_sem);
	init_MUTEX_LOCKED(&asd->platform_data->wait_sem);
	init_waitqueue_head(&asd->platform_data->waitq);

	asd_init_tasklets(asd);

#ifdef ASD_EH_SIMULATION
	init_MUTEX_LOCKED(&asd->platform_data->eh_simul_sem);
#endif	
	
	asd->platform_data->num_domains = ASD_MAX_PORTS;

	return (0);
}

void
asd_platform_free(struct asd_softc *asd)
{
	struct asd_io_handle	*io_handle;
	struct asd_domain	*dm;
	struct asd_target	*targ;
	struct asd_device	*dev;
	u_int			 i;
	u_int			 j;
	u_int			 k;

	/* Kill any threads that we created. */
	asd_kill_discovery_thread(asd);
	asd_kill_ehandler_thread(asd);
#ifdef ASD_EH_SIMULATION	
	asd_kill_eh_simul_thread(asd);
#endif

	asd_kill_tasklets(asd);

	/* Deregister the Scsi Host with the OS. */
	if (asd->platform_data->scsi_host) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
		scsi_remove_host(asd->platform_data->scsi_host);
		scsi_host_put(asd->platform_data->scsi_host);
#else
		scsi_unregister(asd->platform_data->scsi_host);
#endif
	}
	
	/* Free up any allocated linux domains, targets and devices. */
	for (i = 0; i < asd->hw_profile.max_phys; i++) {
		dm = asd->platform_data->domains[i];	
		if (dm == NULL)
			continue;

		for (j = 0; j < ASD_MAX_TARGET_IDS; j++) {
			targ = dm->targets[j];
			if (targ == NULL) 
				continue;

			for (k = 0; k < ASD_MAX_LUNS; k++) {
				dev = targ->devices[k];
				if (dev == NULL)
					continue;

				asd_free_device(asd, dev);
			}
			/* 
			 * For target with no devices allocated previously,
			 * we need to free target explicitly.
			 */
			if (dm->targets[j] != NULL)
				asd_free_mem(dm->targets[j]);
		}
		/* 
		 * For domain with no targets allocated previously,
		 * we need to free domain explicitly.
		 */
		if (asd->platform_data->domains[i] != NULL)
			asd_free_mem(asd->platform_data->domains[i]);
	}

	/*
	 * Disable chip interrupts if we have successfully mapped
	 * the controller.  We do this *before* unregistering
	 * our interrupt handler so that stray interrupts from
	 * our controller do not hang the machine.
	 */
	if (asd->io_handle_cnt != 0)
		asd_intr_enable(asd, 0);

	/* Unregister the interrupt handler. */
	if (asd_init_stat.asd_irq_registered == 1)
		free_irq(asd_pci_dev(asd)->irq, asd);

	/* Lock PCIC_MBAR_KEY. */
	asd_pcic_write_dword(asd, PCIC_MBAR_KEY, 0xFFFFFFFF);	

	/* Free the IO Handle(s). */
	for ( ; asd->io_handle_cnt != 0; ) {
		io_handle = asd->io_handle[(--asd->io_handle_cnt)];
		if (io_handle->type == ASD_MEMORY_SPACE) {
			release_mem_region(io_handle->bar_base, 
					   io_handle->length);
		} else {
			release_region(io_handle->bar_base, io_handle->length);
		}	
		asd_free_mem(io_handle);
	}
}

/* 
 * Function:
 *	asd_init_hw()
 *
 * Description:
 *	This routine will call the hwi layer to initialize the controller.
 *	Allocate any required memory, private data structures (such as scb, 
 *	edb, dl, etc) for the controller.
 */
static int
asd_init_hw(struct asd_softc *asd)
{
	uint32_t	mbar_key;
	uint32_t	cmd_stat_reg;
	int 		error;

	/* TODO: Revisit */
	//ASD_LOCK_ASSERT(asd);

	/* Only support for Rev. B0 chip. */
//JD we support B0,B1...
//	if (asd->hw_profile.rev_id != AIC9410_DEV_REV_B0) {
//		asd_print("Only AIC-9410 Rev. B0 is supported !\n");
//		error = -ENODEV;
//		goto exit;
//	}
		
	/*
	 * Check if the PCIC_MBAR_KEY is not unlocked without permission.
	 * Value 0x0 means it has been unlocked.
	 */
	mbar_key = asd_pcic_read_dword(asd, PCIC_MBAR_KEY);
       	if (mbar_key == 0x0)
		asd_log(ASD_DBG_INFO, "MBAR_KEY has been unlocked !!\n");
	
	/* Map the IO handle. */
	error = asd_map_io_handle(asd);
	if (error != 0) {
		asd_log(ASD_DBG_ERROR, "Failed to map IO Handle.\n");
		goto exit;
	}		

	/* Check if bus master is enabled. Enabled it if it is not. */
	cmd_stat_reg = asd_pcic_read_dword(asd, PCIC_COMMAND);
	if (!(cmd_stat_reg & MST_EN)) {
		cmd_stat_reg |= MST_EN;
		asd_pcic_write_dword(asd, PCIC_COMMAND, cmd_stat_reg);	
	}
	
	/*
	 * Now, unlock the PCIC_MBAR_KEY for write access to MBAR.
	 * Read the value from the register and write it back to the register
	 * to unlcok MBAR.
	 */
	mbar_key = asd_pcic_read_dword(asd, PCIC_MBAR_KEY);	
	if (mbar_key != 0x0)
		asd_pcic_write_dword(asd, PCIC_MBAR_KEY, mbar_key);
	
	/*
	 * AIC9410 CHIP Rev. A1 has the issue where the data transfer hangs
	 * on the host write DMA. The workaround for this issue is to disable
	 * PCIX Rewind feature.
	 */ 
	if (asd->hw_profile.rev_id == AIC9410_DEV_REV_A1) {
		asd_pcic_write_dword(asd,  PCIC_HSTPCIX_CNTRL, 
				    (asd_pcic_read_dword(asd, 
							 PCIC_HSTPCIX_CNTRL) | 
							 REWIND_DIS));
	}
//JD fixing DDTS 92647/92648... ABORT issues when PCI read memory timed out and command completed
//  but driver doesn't see in DL.
	asd_pcic_write_dword(asd,  PCIC_HSTPCIX_CNTRL, 
				    (asd_pcic_read_dword(asd, 
							 PCIC_HSTPCIX_CNTRL) | 
							 SC_TMR_DIS));
	error = asd_hwi_init_hw(asd);
	if (error != 0) {
		asd_log(ASD_DBG_ERROR, "Init HW failed.\n");
		goto exit;
	}
	
	/* Register the interrupt handler with the OS. */
	error = request_irq(asd_pci_dev(asd)->irq, asd_isr, SA_SHIRQ,
			    ASD_DRIVER_NAME, asd);
	if (error != 0)
	     asd_log(ASD_DBG_ERROR, "Failed to register IRQ handler.\n"); 

	asd_init_stat.asd_irq_registered = 1;
exit:
	return (error);
}

/* 
 * Function:
 *	asd_map_io_handle()
 *
 * Description:
 *	Map the IO handles for the register access.
 */
static int
asd_map_io_handle(struct asd_softc *asd)
{
	uint32_t	cmd_stat_reg;
	int		error;

	asd->io_handle = asd_alloc_mem((sizeof(struct asd_io_handle) * 
					ASD_MAX_IO_HANDLES), GFP_KERNEL);
	if (asd->io_handle == NULL)
		return (-ENOMEM);
						
	cmd_stat_reg = asd_pcic_read_dword(asd, PCIC_COMMAND);
	
	/* Whenever possible, map the IO handles using Memory Mapped. */
	if (cmd_stat_reg & MEM_EN) {
		error = asd_mem_mapped_io_handle(asd);
		if (error == 0)
			goto exit;
		/*
		 * We will fall back to IO Mapped if we failed to map using
		 * Memory Mapped.
		 */
	}	
	
	if (cmd_stat_reg & IO_EN)
		error = asd_io_mapped_io_handle(asd);	
	else
		error = -ENOMEM;
exit:
	return (error);	
}

/*
 * asd_mem_mapped_io_handle()
 *
 * Description:
 *	Map the IO Handle using Memory Mapped.
 */
static int
asd_mem_mapped_io_handle(struct asd_softc *asd)
{
	struct asd_io_handle	*io_handle;
	int			 error;
	uint32_t		 base_addr;
	uint32_t		 base_page;
	uint32_t		 base_offset;
	uint32_t		 bar_type;
	uint32_t		 bar_len;
	uint8_t			 index;
#if 0
	uint32_t		scb_pro;	
#endif

#define ASD_FREE_PREV_IO_HANDLE(asd)					    \
do {									    \
	for ( ; asd->io_handle_cnt != 0; ) {				    \
		io_handle = asd->io_handle[(--asd->io_handle_cnt)];	    \
		release_mem_region(io_handle->bar_base, io_handle->length); \
		asd_free_mem(io_handle);				    \
	}								    \
} while (0)

	/*
	 * TBRV: MBAR0 and MBAR1 of the controller are both 64-bit. 
	 *	 Linux PCI isn't aware of 64-bit BAR.
	 *	 For now, it is fine we just map the first 32-bit, as the upper
	 *	 32-bit are set to 0.
	 */	
	for (index = 0; index < 3; index = index+2) {
		/*
		 * Acquire the base addr, length of the region to be mapped.
		 */
		base_addr = pci_resource_start(asd_dev_to_pdev(asd->dev), 
					       index);
		bar_type = pci_resource_flags(asd_dev_to_pdev(asd->dev), 
					      index);
		
		if (index == PCIC_MBAR0_OFFSET) {
			uint32_t	mbar0_mask;
			
			/* 
		 	 * For MBAR0, we need to figure out the size of 
			 * the region to be mapped.  The configured size
			 * of the bar is 8K * 2^N, where N is the number of
			 * bits set in the MBAR0 size mask.
		 	 */

			mbar0_mask = asd_pcic_read_dword(asd, PCIC_MBAR0_MASK);
			mbar0_mask = PCIC_MBAR0_SIZE(mbar0_mask);
			bar_len = 0x2000;
			while (mbar0_mask != 0) {
				mbar0_mask >>= 1;
				bar_len <<= 1;
			}
		} else {
			/* For MBAR1, we will map 128K. */
			bar_len = 0x20000;
		}
		
		/*
		 * Sanity checking.
		 * TBRV: Shoud we allow to proceed if we failed to map MBAR1 ?
		 */
		if ((base_addr == 0) || (bar_len == 0)) { 
			asd_log(ASD_DBG_ERROR, "Failed in getting "
				"PCI resources.\n");
			ASD_FREE_PREV_IO_HANDLE(asd);
			error = -ENOMEM;
			goto exit;
		}
			
		io_handle = asd_alloc_mem(sizeof(*io_handle), GFP_KERNEL);
		if (io_handle == NULL) {
			asd_log(ASD_DBG_ERROR, "Out of memory resources.\n");
			ASD_FREE_PREV_IO_HANDLE(asd);
			error = -ENOMEM;
			goto exit;		
		}
		memset(io_handle, 0x0, sizeof(*io_handle));
		io_handle->bar_base = base_addr;	
		io_handle->length = bar_len;
		io_handle->index = index;
		io_handle->type = ASD_MEMORY_SPACE;
			
		if (request_mem_region(base_addr, bar_len,
				       ASD_DRIVER_NAME) == 0) {
			asd_log(ASD_DBG_ERROR, "Failed to request region for "
				"idx = %d, addr = 0x%x, len = 0x%x.\n",
				index, base_addr, bar_len);
			asd_free_mem(io_handle);		
			ASD_FREE_PREV_IO_HANDLE(asd); 
			error = -ENOMEM;
			goto exit;
		}
		base_page = base_addr & PAGE_MASK;
		base_offset = base_addr - base_page;
		/*
		 * Request the MBAR to be remapped in the non-cached region.
		 */
		io_handle->baseaddr.membase = ioremap_nocache(base_page,
							      (bar_len +
							       base_offset));
		if (io_handle->baseaddr.membase == NULL) {
			asd_log(ASD_DBG_ERROR, "Failed to perform ioremap "
				"for addr = 0x%x, len = 0x%x.\n",
				base_page, (bar_len + base_offset));
			release_mem_region(base_addr, bar_len);
			asd_free_mem(io_handle);
			ASD_FREE_PREV_IO_HANDLE(asd);
			error = -ENOMEM;
			goto exit;
		}	

		asd->io_handle[asd->io_handle_cnt] = io_handle;		
		asd->io_handle_cnt++;
	}

	/*
	 * Do a simple test that the region is properly mapped.
	 * We are going to read SCBPRO register, and check the upper 16-bits
	 * value which represent read-only SCBCONS. 
	 * Write any random value to SCBCONS shouldn't take any effect. 
	 */
#if 0
	/* XXX At this point, we do not know if the central sequencer is
	 *     running or not, so touching the producer index is *not*
	 *     safe.  We should either map our sliding window before this
	 *     test so we can pause the CSEQ or come up with a different
	 *     register to use for this test.
	 */
	scb_pro = asd_read_dword(asd, SCBPRO);
	scb_pro++;
	scb_pro &= SCBPRO_MASK;
	asd_write_dword(asd, SCBPRO, scb_pro);
	if (asd_read_dword(asd, SCBPRO) == scb_pro) {
		/* 
		 * If both values matched, that means the SCBCONS got changed.
		 */
		asd_log(ASD_DBG_ERROR, "Failed in testing register mapping.\n");
		ASD_FREE_PREV_IO_HANDLE(asd);
		error = -ENOMEM;
		goto exit; 
	}	
#endif
	/* Reaching here means we succeed in mapping the region. */
	error = 0;
exit:
	return (error);
}

/*
 * asd_io_mapped_io_handle()
 *
 * Description:
 *	Map the IO Handle using IO Mapped.
 */
static int
asd_io_mapped_io_handle(struct asd_softc *asd)
{
	struct asd_io_handle	*io_handle;
	int			 error;
	uint32_t		 base_addr;
	uint32_t		 bar_type;
	uint32_t		 bar_len;
	uint8_t			 index;

	/*
	 * TBRV: IOBAR of the controller is 64-bit. 
	 *	 Linux PCI doesn't aware of 64-bit BAR.
	 *	 For now, it is fine we just map the first 32-bit, as the upper
	 *	 32-bit is set to 0.
	 */
	index = PCIC_IOBAR_OFFSET;
			
	/* Acquire the base addr, length of the region to be mapped. */
	base_addr = pci_resource_start(asd_dev_to_pdev(asd->dev), index);
	bar_type = pci_resource_flags(asd_dev_to_pdev(asd->dev), index);
	bar_len = pci_resource_len(asd_dev_to_pdev(asd->dev), index);
	
	if ((base_addr == 0) || (bar_len == 0)) { 
		asd_log(ASD_DBG_ERROR, "Failed in getting PCI resources.\n");
	       	error = -ENOMEM;
	       	goto exit;
       	}

       	io_handle = asd_alloc_mem(sizeof(*io_handle), GFP_KERNEL);
       	if (io_handle == NULL) {
       		asd_log(ASD_DBG_ERROR, "Out of memory resources.\n");
       		error = -ENOMEM;
       		goto exit;		
       	}
	memset(io_handle, 0x0, sizeof(*io_handle));	
	io_handle->baseaddr.iobase = base_addr;		
       	io_handle->length = bar_len;
       	io_handle->index = index;
       	io_handle->type = ASD_IO_SPACE;

 	if (request_region(base_addr, bar_len, ASD_DRIVER_NAME) == 0) {
		asd_log(ASD_DBG_ERROR, "Failed to request region for "
			"idx = %d, addr = 0x%x, len = 0x%x.\n",
			index, base_addr, bar_len);
		asd_free_mem(io_handle);	 
		error = -ENOMEM;
		goto exit;
       	}	 
	
	/* Reaching here means we succeed in mapping the region. */
	asd->io_handle[asd->io_handle_cnt] = io_handle;
	asd->io_handle_cnt++;	 	 
	error = 0;
exit:
	return (error);	
}

/*
 * asd_isr()
 *
 * Description:
 *	This is the interrupt handler. Check if we have any interrupt pending.
 *	If there is, process it. Otherwise, just return.
 */
irqreturn_t
#if defined(__VMKLNX__)
asd_isr(int  irq, void  *dev_id)
#else /* !defined(__VMKLNX__) */
asd_isr(int  irq, void  *dev_id, struct pt_regs  *regs)
#endif /* defined(__VMKLNX__) */
{
	struct asd_softc	*asd;
	unsigned long		 flags;
	int			 irq_retval;

	asd = (struct asd_softc *) dev_id;
#if 0
 if(asd->debug_flag ==1)
 {
 	asd_log(ASD_DBG_INFO, "ISR\n");
 }
#endif

	asd_lock(asd, &flags);

	irq_retval = asd_hwi_process_irq(asd);

	if (asd_next_device_to_run(asd) != NULL)
		asd_schedule_runq(asd);

	asd_unlock(asd, &flags);
	
	return IRQ_RETVAL(irq_retval);
}

/*
 * asd_queue()
 *
 * Description:
 *	Execute the requested IO.
 */
static int
asd_queue(Scsi_Cmnd  *cmd, void (*scsi_done)(Scsi_Cmnd *))
{
	struct asd_softc	*asd;
	struct asd_device	*dev;

	asd = *((struct asd_softc **) cmd->device->host->hostdata);
#ifdef CHECK_CMD
#if LINUX_VERSION_CODE <  KERNEL_VERSION(2,6,13)
	cmd->abort_reason=jiffies / HZ;
#endif
#if 1
	if(asd->debug_flag ==1)
	{
		asd_log(ASD_DBG_INFO, "asd_queue\n");
	}
#endif
//JD
	if(cmd->cmnd[0]==0x0)
	{
#if LINUX_VERSION_CODE <  KERNEL_VERSION(2,6,13)
	asd_log(ASD_DBG_INFO, "asd_queue for SCSI cmd LBA 0x%02x%02x%02x%02x - 0x%02x%02x (tag:0x%x, pid:0x%x, res_count:0x%x, timeout:0x%x abort:%d), opcode 0x%x.\n", 
#else
	asd_log(ASD_DBG_INFO, "asd_queue for SCSI cmd LBA 0x%02x%02x%02x%02x - 0x%02x%02x (tag:0x%x, pid:0x%x, res_count:0x%x, timeout:0x%x), opcode 0x%x.\n", 
#endif
			cmd->cmnd[2],
			cmd->cmnd[3],
			cmd->cmnd[4],
			cmd->cmnd[5],
			cmd->cmnd[7],
			cmd->cmnd[8],
		  cmd->tag,
		  cmd->pid,
		  cmd->resid,
		  cmd->timeout_per_command,
#if LINUX_VERSION_CODE <  KERNEL_VERSION(2,6,13)
		  cmd->abort_reason,
#endif
		  cmd->cmnd[0]);
	}
#endif
	/*
	 * Save the callback on completion function.
	 */
	cmd->scsi_done = scsi_done;
	asd_sml_lock(asd);

	/*
	 * Close the race of a command that was in the process of
	 * being queued to us just as our controller was frozen.
	 */
	if (asd->platform_data->qfrozen != 0) {

		asd_sml_unlock(asd);
		asd_cmd_set_retry_status(cmd);
		cmd->scsi_done(cmd);
		return (0);
	}

	dev = asd_get_device(asd, cmd->device->channel, cmd->device->id,
			     cmd->device->lun, /*alloc*/1);
	if (dev == NULL) {
		asd_sml_unlock(asd);
		
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
		asd_cmd_set_host_status(cmd, DID_NO_CONNECT);
#else
		asd_cmd_set_offline_status(cmd);
#endif
		cmd->scsi_done(cmd);
		return (0);
	} else if (((dev->flags & ASD_DEV_UNCONFIGURED) != 0) && 
		   (cmd->device->type != -1)) {
		/*
		 * Configure devices that have already successfully
		 * completed an inquiry. This handles the case of
		 * devices being destroyed due to transient selection
		 * timeouts.
		 */
		dev->flags &= ~ASD_DEV_UNCONFIGURED;
	 	dev->scsi_device = cmd->device;
		asd_set_device_queue_depth(asd, dev);
	} else {
		/*
		 * The target is in the process of being destroyed as
		 * it had been hot-removed. Return the IO back to the
		 * scsi layer.
		 */
		if (dev->target->flags & ASD_TARG_HOT_REMOVED) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
			asd_cmd_set_host_status(cmd, DID_NO_CONNECT);
#else
			asd_cmd_set_offline_status(cmd);
#endif
			cmd->scsi_done(cmd);
			return (0);
		}
	}

	/*
	 * DC: Need extra storage for SSP_LONG tasks to hold the CDB.
	 *     For now, just limit the CDB to what we can embed in the SCB.
 	 */
	if (cmd->cmd_len > SCB_EMBEDDED_CDB_SIZE) {
#if !defined(__VMKLNX__)
		asd_cmd_set_host_status(cmd, DID_BAD_TARGET);
#else
		/*
		 * Instead of returning  DID_BAD_TARGET, return
		 * check condition with ILLEGAL REQUEST/INVALID FIELD IN CDB.
		 */
		asd_cmd_set_scsi_status(cmd, SCSI_STATUS_CHECK_COND);
		asd_cmd_set_host_status(cmd, DID_OK);
		asd_cmd_set_driver_status(cmd, DRIVER_SENSE);
		memset(cmd->sense_buffer, 0, sizeof(cmd->sense_buffer));
		cmd->sense_buffer[0] = SSD_ERRCODE_VALID
					| SSD_CURRENT_ERROR;
		cmd->sense_buffer[2] = SSD_KEY_ILLEGAL_REQUEST;
		cmd->sense_buffer[12] = 0x24;
		cmd->sense_buffer[13] = 0x00;
#endif /* !defined(__VMKLNX__) */
		asd_sml_unlock(asd);
		cmd->scsi_done(cmd);
		asd_print("%s: asd94xx_queue -"
			  "CDB length of %d exceeds max!\n",
			  asd_name(asd), cmd->cmd_len);
		return (0);
	}

#ifdef ASD_EH_SIMULATION	
	++cmd_cnt;
	if ((cmd_cnt != 0) && ((cmd_cnt % 888) == 0x0)) {
		asd_print("Setting up cmd %p for eh simulation.\n", cmd);
		asd_cmd_set_host_status(cmd, 0x88);
	} else {
		asd_cmd_set_host_status(cmd, CMD_REQ_INPROG);
	}
#else
	/*
	 * Workaround for some kernel versions, when the cmd is retried but
         * the cmd->result is not clear.
	 */
	cmd->result = 0;
	asd_cmd_set_host_status(cmd, CMD_REQ_INPROG);
#endif /* ASD_EH_SIMULATION */ 	

	list_add_tail(&((union asd_cmd *)cmd)->acmd_links, &dev->busyq);
	if ((dev->flags & ASD_DEV_ON_RUN_LIST) == 0) {
		list_add_tail(&dev->links, &asd->platform_data->device_runq);
		dev->flags |= ASD_DEV_ON_RUN_LIST;
		asd_run_device_queues(asd);
	}

	asd_sml_unlock(asd);
	return (0);
} 

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
/*
 * asd_select_queue_depth()
 *
 * Description:
 *	Adjust the queue depth for each device attached to our controller.
 */
static void
asd_select_queue_depth(struct Scsi_Host *host, Scsi_Device *scsi_devs)
{
	struct asd_softc	*asd;
	Scsi_Device 		*device;
	Scsi_Device 		*ldev;
	u_long			 flags;

	asd = *((struct asd_softc **)host->hostdata);
	asd_lock(asd, &flags);
	for (device = scsi_devs; device != NULL; device = device->next) {
		/*
		 * Watch out for duplicate devices.  This works around
		 * some quirks in how the SCSI scanning code does its
		 * device management.
		 */
		for (ldev = scsi_devs; ldev != device; ldev = ldev->next) {
			if (ldev->host == device->host
			 && ldev->channel == device->channel
			 && ldev->id == device->id
			 && ldev->lun == device->lun)
				break;
		}
		/* Skip duplicate. */
		if (ldev != device)
			continue;

		if (device->host == host) {
			struct asd_device *dev;

			/*
			 * Since Linux has attached to the device, configure
			 * it so we don't free and allocate the device
			 * structure on every command.
			 */
			dev = asd_get_device(asd, device->channel,
					     device->id, device->lun,
					     /*alloc*/1);
			if (dev != NULL) {
				dev->flags &= ~ASD_DEV_UNCONFIGURED;
				dev->scsi_device = device;
				asd_set_device_queue_depth(asd, dev);
				device->queue_depth = dev->openings + 
						      dev->active;
				if ((dev->flags & (ASD_DEV_Q_BASIC | 
						   ASD_DEV_Q_TAGGED)) == 0) {
					/*
					 * We allow the OS to queue 2 untagged
					 * transactions to us at any time even
					 * though we can only execute them
					 * serially on the controller/device.
					 * This should remove some latency.
					 */
					device->queue_depth = 2;
				}
			}
		}
	}
	asd_unlock(asd, &flags);
}

#else

static int
asd_slave_alloc(Scsi_Device *scsi_devs)
{
	struct asd_softc *asd;

	asd = *((struct asd_softc **) scsi_devs->host->hostdata);
	asd_log(ASD_DBG_RUNTIME_B, "%s: Slave Alloc %d %d %d\n", asd_name(asd),
		scsi_devs->channel, scsi_devs->id, scsi_devs->lun);

	return (0);	
}
	
static int
asd_slave_configure(Scsi_Device *scsi_devs)
{
	struct asd_softc 	*asd;
	struct asd_device	*dev;
	u_long			 flags;	

	asd = *((struct asd_softc **) scsi_devs->host->hostdata);
	asd_log(ASD_DBG_RUNTIME_B, "%s: Slave Configure %d %d %d\n", asd_name(asd),
		scsi_devs->channel, scsi_devs->id, scsi_devs->lun);

	asd_lock(asd, &flags);
	/*
	 * Since Linux has attached to the device, configure it so we don't 
	 * free and allocate the device structure on every command.
	 */
	dev = asd_get_device(asd, scsi_devs->channel, scsi_devs->id, 
			     scsi_devs->lun, /*alloc*/1);
	if (dev != NULL) {
		dev->flags &= ~ASD_DEV_UNCONFIGURED;
		dev->flags |= ASD_DEV_SLAVE_CONFIGURED;
		dev->scsi_device = scsi_devs;
		asd_set_device_queue_depth(asd, dev);
	}
	asd_unlock(asd, &flags);

	return (0);
}

static void
asd_slave_destroy(Scsi_Device *scsi_devs)
{
	struct asd_softc 	*asd;
	struct asd_device 	*dev;
	u_long			 flags;	

	asd = *((struct asd_softc **) scsi_devs->host->hostdata);
	asd_log(ASD_DBG_RUNTIME_B, "%s: Slave Destroy %d %d %d\n", asd_name(asd),
		scsi_devs->channel, scsi_devs->id, scsi_devs->lun);

	asd_lock(asd, &flags);

	dev = asd_get_device(asd, scsi_devs->channel, scsi_devs->id, 
			     scsi_devs->lun, /*alloc*/0);

	if (dev == NULL) {
		asd_unlock(asd, &flags);
		return;
	}

	if ((dev->flags & ASD_DEV_SLAVE_CONFIGURED) != 0) {
		if ((list_empty(&dev->busyq)) && (dev->active == 0) &&
		   ((dev->flags & ASD_DEV_TIMER_ACTIVE) == 0)) {
			if (dev->target->refcount == 1) {
				if( dev->target->flags & ASD_TARG_HOT_REMOVED) {
					asd_free_ddb(
						asd,
						dev->target->ddb_profile.conn_handle);
					/* Free the allocated device. */
					asd_free_device(asd, dev);
				} else {
					dev->flags |= ASD_DEV_UNCONFIGURED;
					dev->flags &= ~ASD_DEV_SLAVE_CONFIGURED;
					dev->scsi_device = NULL;
				}
			}
		}
	}

	asd_unlock(asd, &flags);
}

static int
asd_initiate_bus_scan(struct asd_softc *asd)
{
	int	error;

	error = scsi_add_host(asd->platform_data->scsi_host, asd->dev);
	if (error != 0)	
		return (error);

#if defined(__VMKLNX__)
	vmklnx_scsi_register_poll_handler(asd->platform_data->scsi_host,
					  asd_pci_dev(asd)->irq, asd_isr,
					  asd);
#endif /* defined(__VMKLNX__) */		
	scsi_scan_host(asd->platform_data->scsi_host);
	return (0);
}

#endif

/*
 * Function:
 *	asd_get_user_tagdepth()
 *	
 * Description:
 *	Return the user specified device queue depth.
 * 	If none specified, return a default queue depth.
 */
static u_int
asd_get_user_tagdepth(struct asd_softc *asd, struct asd_device *dev)
{
	u_int	usertags;
	
	/*
	 * Sanity Check to make sure the Queue Depth within supported range.
	 */
	if ((cmd_per_lun > ASD_MAX_TCQ_PER_DEVICE) ||
	    (cmd_per_lun < ASD_MIN_TCQ_PER_DEVICE))
		/* Set the Queue Depth to default (32). */
		cmd_per_lun = ASD_DEF_TCQ_PER_DEVICE;
	/*
	 * No queuing support yet for SATA II devices.
 	 */
	if ((dev->target->command_set_type == ASD_COMMAND_SET_ATA) ||
	    (dev->target->command_set_type == ASD_COMMAND_SET_ATAPI))
		usertags = 0;
	else
		usertags = cmd_per_lun;
	
	return (usertags);
}

/*
 * Function:
 *	asd_set_device_queue_depth()
 *
 * Description: 	  
 * 	Determines the queue depth for a given device.
 */
static void
asd_set_device_queue_depth(struct asd_softc *asd, struct asd_device *dev)
{
	u_int	tags;

	ASD_LOCK_ASSERT(asd);

	tags = asd_get_user_tagdepth(asd, dev);
	if (tags != 0 && dev->scsi_device != NULL && 
	    dev->scsi_device->tagged_supported != 0) {
		asd_set_tags(asd, dev, ASD_QUEUE_TAGGED);

		asd_print("%s:%d:%d:%d: Tagged Queuing enabled.  Depth %d\n",
			  asd_name(asd),
			  dev->target->domain->channel_mapping,
			  dev->target->target_id, dev->lun,
			  dev->openings + dev->active);
	} else {
		asd_set_tags(asd, dev, ASD_QUEUE_NONE);
	}
}

/*
 * Function:
 *	asd_set_tags()
 *
 * Description: 	  
 * 	Set the device queue depth.
 */
static void
asd_set_tags(struct asd_softc *asd, struct asd_device *dev, asd_queue_alg alg)
{
	int was_queuing;
	int now_queuing;

	was_queuing = dev->flags & (ASD_DEV_Q_BASIC|ASD_DEV_Q_TAGGED);
	switch (alg) {
	default:
	case ASD_QUEUE_NONE:
		now_queuing = 0;
		break; 
	case ASD_QUEUE_BASIC:
		now_queuing = ASD_DEV_Q_BASIC;
		break;
	case ASD_QUEUE_TAGGED:
		now_queuing = ASD_DEV_Q_TAGGED;
		break;
	}

	dev->flags &= ~(ASD_DEV_Q_BASIC|ASD_DEV_Q_TAGGED);
	if (now_queuing) {
		u_int usertags;

		usertags = asd_get_user_tagdepth(asd, dev);
		if (!was_queuing) {
			/*
			 * Start out agressively and allow our
			 * dynamic queue depth algorithm to take
			 * care of the rest.
			 */
			dev->maxtags = usertags;
			dev->openings = dev->maxtags - dev->active;
		}
		if (dev->maxtags == 0)
			/*
			 * Queueing is disabled by the user.
			 */
			dev->openings = 1;
		else if (alg == ASD_QUEUE_TAGGED)
			dev->flags |= ASD_DEV_Q_TAGGED;
		else
			dev->flags |= ASD_DEV_Q_BASIC;
	} else {
		/* We can only have one opening. */
		dev->maxtags = 0;
		dev->openings =  1 - dev->active;
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
	if (dev->scsi_device != NULL) {
		switch ((dev->flags & (ASD_DEV_Q_BASIC|ASD_DEV_Q_TAGGED))) {
		case ASD_DEV_Q_BASIC:
			scsi_adjust_queue_depth(dev->scsi_device,
						MSG_SIMPLE_TAG,
						dev->maxtags);
			break;
		case ASD_DEV_Q_TAGGED:
			scsi_adjust_queue_depth(dev->scsi_device,
						MSG_ORDERED_TAG,
						dev->maxtags);
			break;
		default:
			/*
			 * We allow the OS to queue 2 untagged transactions to
			 * us at any time even though we can only execute them
			 * serially on the controller/device.  This should
			 * remove some latency.
			 */
			scsi_adjust_queue_depth(dev->scsi_device,
						/* NON-TAGGED */ 0,
						/* Queue Depth */ 2);
			break;
		}
	}
#endif
}

		       
/*
 * asd_info()
 *
 * Description:
 *	Return an info regarding the driver to the OS.
 */
static const char *
asd_info(struct Scsi_Host  *scsi_host)
{
	struct asd_softc	*asd;
	const char		*info;
	
	info = "";
	asd = *((struct asd_softc **) scsi_host->hostdata);
		
	if (asd_get_softc(asd) != NULL) {
		info = ((struct asd_pci_driver_data *) 
				(asd->pci_entry->driver_data))->description;
	}

	return (info);
}


/**************************** OS Error Handling *******************************/

/*
 * Function:
 *	asd_ehandler_thread()
 *
 * Description:
 *	Thread to handle error recovery.
 */ 
static int
asd_ehandler_thread(void *data)
{
	struct asd_softc	*asd;
	u_long			 flags;	

	asd = (struct asd_softc *) data;

	lock_kernel();
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,60)
	/*
	 * Don't care about any signals.
	 */
	siginitsetinv(&current->blocked, 0);
	daemonize();
	sprintf(current->comm, "asd_eh_%d", asd->profile.unit);
#else
	daemonize("asd_eh_%d", asd->profile.unit);
#if !defined(__VMKLNX__)
	current->flags |= PF_FREEZE;	
#endif /* ! defined(__VMKLNX__) */
#endif
	unlock_kernel();

	while (1) {
		/*
		 * Use down_interruptible() rather than down() to
		 * avoid this thread to be counted in the load average as
		 * a running process.
		 */
		down_interruptible(&asd->platform_data->ehandler_sem);

		/* Check to see if we've been signaled to exit. */
		asd_lock(asd, &flags);
		if ((asd->platform_data->flags & ASD_RECOVERY_SHUTDOWN) != 0) {
			asd_unlock(asd, &flags);
			break;
		}
		/* 
		 * Check if any timedout commands that required 
		 * error handling. 
		 */
		if (list_empty(&asd->timedout_scbs)) {
			asd_unlock(asd, &flags);
			continue;
		}
		asd_unlock(asd, &flags);

		asd_recover_cmds(asd);
	}

	up(&asd->platform_data->ehandler_ending_sem);

	return (0);
}

/*
 * Function:
 * 	asd_kill_ehandler_thread()
 * 
 * Description:
 * 	Kill the error handling thread.
 */
static void
asd_kill_ehandler_thread(struct asd_softc *asd)
{
	u_long	flags;

	asd_lock(asd, &flags);

	if (asd->platform_data->ehandler_pid != 0) {
		asd->platform_data->flags |= ASD_RECOVERY_SHUTDOWN;
		asd_unlock(asd, &flags);
		up(&asd->platform_data->ehandler_sem);
		asd->platform_data->ehandler_pid = 0;
	} else {
		asd_unlock(asd, &flags);
	}
}

static void
asd_ehandler_done(struct asd_softc *asd, struct scb *scb)
{
	scb->platform_data->flags &= ~ASD_SCB_UP_EH_SEM;
	asd_wakeup_sem(&asd->platform_data->eh_sem);
}

/*
 * asd_abort()
 *
 * Description:
 *	Perform abort for the requested command.
 */
static int
asd_abort(Scsi_Cmnd *cmd)
{
	struct asd_softc	*asd;
	struct asd_device	*dev;
	struct scb		*scb_to_abort;
	union asd_cmd 		*acmd;
	union asd_cmd 		*list_acmd;
	int			 retval;
	int			 found;
	u_long			flags;

#ifdef ASD_DEBUG
	asd_log(ASD_DBG_INFO, "(scsi%d: Ch %d Id %d Lun %d): ",
		  cmd->device->host->host_no,
		  cmd->device->channel, cmd->device->id, cmd->device->lun);
//JD
#if LINUX_VERSION_CODE <  KERNEL_VERSION(2,6,13)
	asd_log(ASD_DBG_INFO, "Abort requested for SCSI cmd LBA 0x%02x%02x%02x%02x - 0x%02x%02x (tag:0x%x, pid:0x%x, res_count:0x%x, timeout:0x%x abort:%d vs current:%d), opcode 0x%x.\n", 
#else	
	asd_log(ASD_DBG_INFO, "Abort requested for SCSI cmd LBA 0x%02x%02x%02x%02x - 0x%02x%02x (tag:0x%x, pid:0x%x, res_count:0x%x, timeout:0x%x vs current:%d), opcode 0x%x.\n", 
#endif
			cmd->cmnd[2],
			cmd->cmnd[3],
			cmd->cmnd[4],
			cmd->cmnd[5],
			cmd->cmnd[7],
			cmd->cmnd[8],
		  cmd->tag,
		  cmd->pid,
		  cmd->resid,
		  cmd->timeout_per_command,
#if LINUX_VERSION_CODE <  KERNEL_VERSION(2,6,13)
		  cmd->abort_reason,
#endif
		  (jiffies / HZ),
		  cmd->cmnd[0]);
#endif
	asd = *(struct asd_softc **) cmd->device->host->hostdata;
	acmd = (union asd_cmd *) cmd;
	found = 0;
	retval = SUCCESS;
#if defined(__VMKLNX__)
	/*
	 * vmklinux does not take the SCSI host lock before calling the driver
	 * abort handler.  We need to take the spinlock here.
	 */
 	asd_lock(asd, &flags);
#else
#if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,16)
 		asd_lock(asd, &flags);
#else
		asd_sml_lock(asd);
#endif
#endif /* defined(__VMKLNX__) */

	/* See if any existing device owns this command. */
	dev = asd_get_device(asd, cmd->device->channel,
			     cmd->device->id, cmd->device->lun, 0);
	if (dev == NULL) {
		/*
		 * No device exists that owns this command. 
		 * Return abort successful for the requested command.
		 */
		asd_print("(scsi%d: Ch %d Id %d Lun %d): ",
			  cmd->device->host->host_no, cmd->device->channel, 
			  cmd->device->id, cmd->device->lun);
		asd_print("Is not an active device.\n");
		retval = SUCCESS;
		goto exit;
	}

#ifdef ASD_DEBUG	
	{
		struct asd_target *t = NULL;
#ifdef MULTIPATH_IO
		t = dev->current_target;
#else
		t = dev->target;
#endif
		if (!t)
			t = dev->target;
		asd_print("t->ddb_profile.conn_handle=0x%x\n", t->ddb_profile.conn_handle);
/* 		asd_hwi_dump_seq_state(asd, t->src_port->conn_mask); */
/* 		asd_hwi_dump_ddb_site(asd, t->ddb_profile.conn_handle); */
	}
#endif /* ASD_DEBUG */

	/*
	 * Check if the cmd is still in the device queue.
	 */ 
	list_for_each_entry(list_acmd, &dev->busyq, acmd_links) {
//JD
#ifdef ASD_DEBUG
#if LINUX_VERSION_CODE <  KERNEL_VERSION(2,6,13)
		asd_log(ASD_DBG_INFO, "Checking busy queue cmd LBA 0x%02x%02x%02x%02x - 0x%02x%02x (tag:0x%x, pid:0x%x, res_count:0x%x, timeout:0x%x, abort:0x%x).\n",
#else
		asd_log(ASD_DBG_INFO, "Checking busy queue cmd LBA 0x%02x%02x%02x%02x - 0x%02x%02x (tag:0x%x, pid:0x%x, res_count:0x%x, timeout:0x%x).\n",
#endif
			list_acmd->scsi_cmd.cmnd[2],
			list_acmd->scsi_cmd.cmnd[3],
			list_acmd->scsi_cmd.cmnd[4],
			list_acmd->scsi_cmd.cmnd[5],
			list_acmd->scsi_cmd.cmnd[7],
			list_acmd->scsi_cmd.cmnd[8],
			list_acmd->scsi_cmd.tag,
			list_acmd->scsi_cmd.pid,
			list_acmd->scsi_cmd.resid,
#if LINUX_VERSION_CODE <  KERNEL_VERSION(2,6,13)
			list_acmd->scsi_cmd.timeout_per_command,
			list_acmd->scsi_cmd.abort_reason);
#else
			list_acmd->scsi_cmd.timeout_per_command);
#endif
#endif
		if (list_acmd == acmd) {
			/* Found it. */
			found = 1;
			break;
		}
	}
	if (found == 1) {
#ifdef ASD_DEBUG
		asd_log(ASD_DBG_INFO, "Cmd LBA 0x%02x%02x%02x%02x - 0x%02x%02x found on device queue.\n", 
			cmd->cmnd[2],
			cmd->cmnd[3],
			cmd->cmnd[4],
			cmd->cmnd[5],
			cmd->cmnd[7],
			cmd->cmnd[8]);
		asd_print_path(asd, dev);
		asd_print("Cmd %p found on device queue.\n", cmd);
#endif
		list_del(&list_acmd->acmd_links);
		asd_cmd_set_host_status(cmd, DID_ABORT);
		cmd->scsi_done(cmd);
		retval = SUCCESS;
		goto exit;
	}

	/*
	 * Check if the cmd has been submitted to the device.
	 */
	list_for_each_entry(scb_to_abort, &asd->platform_data->pending_os_scbs,
			    owner_links) {
#ifdef ASD_DEBUG
		asd_log(ASD_DBG_INFO, "Checking pending_os_scbs scb_to_abort->io_ctx %p TC 0x%x.\n", scb_to_abort->io_ctx, SCB_GET_INDEX(scb_to_abort));
		asd_log(ASD_DBG_INFO, "scb_to_abort=%p\n", scb_to_abort);

#endif

		if (scb_to_abort->io_ctx == acmd) {
			/* Found it. */

			found = 1;
			break;
		}
	}
	if (found != 1) {
		/*
		 * Looks like we are trying to abort command that has
		 * been completed.
		 */
#ifdef ASD_DEBUG
		asd_log(ASD_DBG_INFO, "Cmd LBA 0x%02x%02x%02x%02x - 0x%02x%02x not found , opcode 0x%x.\n", 			
			cmd->cmnd[2],
			cmd->cmnd[3],
			cmd->cmnd[4],
			cmd->cmnd[5],
			cmd->cmnd[7],
			cmd->cmnd[8],
			cmd->cmnd[0]);
#endif
		asd_print_path(asd, dev);
		asd_print("Cmd %p not found.\n", cmd);
		retval = SUCCESS;
		goto exit;
	}
//JD
#ifdef ASD_DEBUG
#if LINUX_VERSION_CODE <  KERNEL_VERSION(2,6,13)
	asd_log(ASD_DBG_INFO, "Remove this command and put it into timeout SCSI cmd LBA 0x%02x%02x%02x%02x - 0x%02x%02x (tag:0x%x, pid:0x%x, res_count:0x%x, timeout:0x%x abort:%d), opcode 0x%x.\n", 
#else
	asd_log(ASD_DBG_INFO, "Remove this command and put it into timeout SCSI cmd LBA 0x%02x%02x%02x%02x - 0x%02x%02x (tag:0x%x, pid:0x%x, res_count:0x%x, timeout:0x%x), opcode 0x%x.\n", 
#endif
			cmd->cmnd[2],
			cmd->cmnd[3],
			cmd->cmnd[4],
			cmd->cmnd[5],
			cmd->cmnd[7],
			cmd->cmnd[8],
		cmd->tag,
		cmd->pid,
		cmd->resid,
		cmd->timeout_per_command,
#if LINUX_VERSION_CODE <  KERNEL_VERSION(2,6,13)
		cmd->abort_reason,
#endif
		cmd->cmnd[0]);
#endif
#if 0
	asd_log(ASD_DBG_INFO, "aborted cmd sas address=%02x%02x%02x%02x%02x%02x%02x%02x\n",
		dev->target->ddb_profile.sas_addr[0],
		dev->target->ddb_profile.sas_addr[1],
		dev->target->ddb_profile.sas_addr[2],
		dev->target->ddb_profile.sas_addr[3],
		dev->target->ddb_profile.sas_addr[4],
		dev->target->ddb_profile.sas_addr[5],
		dev->target->ddb_profile.sas_addr[6],
		dev->target->ddb_profile.sas_addr[7]);
#endif
#if 0
	    asd_hwi_pause_cseq(asd);
	    asd_hwi_pause_lseq(asd, asd->hw_profile.enabled_phys);
		asd_hwi_dump_ddb_sites_new(asd);
		asd_hwi_dump_scb_sites_new(asd);
		asd_hwi_dump_seq_state(asd, asd->hw_profile.enabled_phys);
		asd_hwi_unpause_cseq(asd);
	    asd_hwi_unpause_lseq(asd, asd->hw_profile.enabled_phys);
#endif
	/*
	 * Set the level of error recovery for the error handler thread
         * to perform.
	 */
	scb_to_abort->eh_state = SCB_EH_ABORT_REQ;
	scb_to_abort->eh_post = asd_ehandler_done;
	/*
	 * Mark this SCB as timedout and add it to the timeout queue.
	 */	 
	scb_to_abort->flags |= SCB_TIMEDOUT;
	list_add_tail(&scb_to_abort->timedout_links, &asd->timedout_scbs);
	
	asd_wakeup_sem(&asd->platform_data->ehandler_sem);
	asd->platform_data->flags |= ASD_SCB_UP_EH_SEM;

	/* Release the host's lock prior putting the process to sleep. */
#if defined(__VMKLNX__)
	asd_unlock(asd, &flags);
#else
#if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,16)
		// it lock by abort rtn, just go to unlock it
		asd_unlock(asd, &flags);
#else
		/* Release the host's lock prior putting the process to sleep. */
		spin_unlock_irq(&asd->platform_data->spinlock);
#endif
#endif /* defined(__VMKLNX__) */
	
	asd_sleep_sem(&asd->platform_data->eh_sem);
 	if (scb_to_abort->eh_status == SCB_EH_SUCCEED)
 		retval = SUCCESS;
 	else
 		retval = FAILED;

#if defined(__VMKLNX__)
	return (retval);
#else
	/* Acquire the host's lock. */
#if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,16)
		asd_lock(asd, &flags);
#else
		spin_lock_irq(&asd->platform_data->spinlock);
#endif
#endif /* defined(__VMKLNX__) */
	
exit:
#if defined(__VMKLNX__)
	asd_unlock(asd, &flags);
#else
#if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,16)
		// it lock by abort rtn, just go to unlock it
		asd_unlock(asd, &flags);
#else
		asd_sml_unlock(asd);
#endif
#endif /* defined(__VMKLNX__) */
	return (retval);
}

/******************************** Bus DMA *************************************/

int
asd_dma_tag_create(struct asd_softc *asd, uint32_t alignment, uint32_t maxsize,
		   int flags, bus_dma_tag_t *ret_tag)
{
	bus_dma_tag_t dmat;

	dmat = asd_alloc_mem(sizeof(*dmat), flags);
	if (dmat == NULL)
		return (-ENOMEM);

	dmat->alignment = alignment;
	dmat->maxsize = maxsize;
	*ret_tag = dmat;
	return (0);
}

void
asd_dma_tag_destroy(struct asd_softc *asd, bus_dma_tag_t dmat)
{
	asd_free_mem(dmat);
}

int
asd_dmamem_alloc(struct asd_softc *asd, bus_dma_tag_t dmat, void** vaddr,
		 int flags, bus_dmamap_t *mapp, dma_addr_t *baddr)
{
	bus_dmamap_t map;

	map = asd_alloc_mem(sizeof(*map), flags);
	if (map == NULL)
		return (-ENOMEM);

	*vaddr = asd_alloc_coherent(asd, dmat->maxsize, &map->bus_addr);
	if (*vaddr == NULL) {
		asd_free_mem(map);
		return (-ENOMEM);
	}
	*mapp = map;
	*baddr = map->bus_addr;
	return(0);
}

void
asd_dmamem_free(struct asd_softc *asd, bus_dma_tag_t dmat, void *vaddr,
		bus_dmamap_t map)
{
	asd_free_coherent(asd, dmat->maxsize, vaddr, map->bus_addr);
}

void
asd_dmamap_destroy(struct asd_softc *asd, bus_dma_tag_t dmat, bus_dmamap_t map)
{
	asd_free_mem(map);
}

int
asd_alloc_dma_mem(struct asd_softc *asd, unsigned length, void **vaddr,
		  dma_addr_t *bus_addr, bus_dma_tag_t *buf_dmat,
		  struct map_node *buf_map)
{
	if (asd_dma_tag_create(asd, 4, length, GFP_ATOMIC, buf_dmat) != 0)
		return (-ENOMEM);

	if (asd_dmamem_alloc(asd, *buf_dmat, (void **) &buf_map->vaddr,
			     GFP_ATOMIC, &buf_map->dmamap,
			     &buf_map->busaddr) != 0) {
		asd_dma_tag_destroy(asd, *buf_dmat);
		return (-ENOMEM);
	}

	*vaddr = (void *) buf_map->vaddr;
	*bus_addr = buf_map->busaddr;
	memset(*vaddr, 0, length);

	return 0;
}

void
asd_free_dma_mem(struct asd_softc *asd, bus_dma_tag_t buf_dmat,
		 struct map_node *buf_map)
{
	asd_dmamem_free(asd, buf_dmat, buf_map->vaddr, buf_map->dmamap);
	asd_dmamap_destroy(asd, buf_dmat, buf_map->dmamap);
	asd_dma_tag_destroy(asd, buf_dmat);
}

/*************************** Platform Data Routines ***************************/

struct asd_scb_platform_data *
asd_alloc_scb_platform_data(struct asd_softc *asd)
{
	struct asd_scb_platform_data *pdata;
	
	pdata = (struct asd_scb_platform_data *) asd_alloc_mem(sizeof(*pdata),
							       GFP_ATOMIC);
	return (pdata);
}

void
asd_free_scb_platform_data(struct asd_softc *asd,
			   struct asd_scb_platform_data *pdata)
{
	asd_free_mem(pdata);
}


/**************************** Proc Filesystem support *************************/

typedef struct proc_info_str {
	char	*buf;
	int	len;
	off_t	off;
	int	pos;
} proc_info_str_t;

static void	copy_mem_info(proc_info_str_t *info_str, char *data, int len);
static int	copy_info(proc_info_str_t *info_str, char *fmt, ...);

static void
copy_mem_info(proc_info_str_t *info_str, char *data, int len)
{
	if (info_str->pos + len > info_str->off + info_str->len)
		len = info_str->off + info_str->len - info_str->pos;

	if (info_str->pos + len < info_str->off) {
		info_str->pos += len;
		return;
	}

	if (info_str->pos < info_str->off) {
		off_t partial;

		partial = info_str->off - info_str->pos;
		data += partial;
		info_str->pos += partial;
		len  -= partial;
	}

	if (len > 0) {
		memcpy(info_str->buf, data, len);
		info_str->pos += len;
		info_str->buf += len;
	}
}

static int
copy_info(proc_info_str_t *info_str, char *fmt, ...)
{
	va_list	args;
	char 	buf[256];
	int 	len;

	va_start(args, fmt);
	len = vsprintf(buf, fmt, args);
	va_end(args);

	copy_mem_info(info_str, buf, len);
	return (len);
}

void
asd_dump_indent(
proc_info_str_t	*info_str,
unsigned	indent
)
{
	unsigned		i;

	for (i = 0 ; i < indent ; i++) {
		copy_info(info_str, "   ");
	}
}

void
asd_dump_conn_rate(
proc_info_str_t	*info_str,
unsigned	conn_rate
)
{
	switch (conn_rate) {
	case SAS_RATE_30GBPS:
		copy_info(info_str, "3000 Mb/s");
		break;
	case SAS_RATE_15GBPS:
		copy_info(info_str, "1500 MB/s");
		break;
	default:
		copy_info(info_str, "\?\? MB/s");
		break;
	}
}

static void
asd_dump_target_info(struct asd_softc *asd, proc_info_str_t *info_str,
	struct asd_port *port, struct asd_target *targ, unsigned indent)
{
	unsigned		i;
	struct Discover		*discover;
	struct asd_target	*child_target;
	struct hd_driveid	*hd_driveidp;

	asd_dump_indent(info_str, indent);

#if 0	
	copy_info(info_str, "  SAS Address: %0llx\n", asd_be64toh(
		*((uint64_t *)targ->ddb_profile.sas_addr)));
#endif
	
	copy_info(info_str, "  Connected to ");
	switch (targ->transport_type) {
	case ASD_TRANSPORT_SSP:
		copy_info(info_str, "SAS End Device. ");	
		copy_info(info_str, "SAS Address: %0llx\n", asd_be64toh(
			  *((uint64_t *)targ->ddb_profile.sas_addr)));

		asd_dump_indent(info_str, indent);
		if (targ->scsi_cmdset.inquiry != NULL) {
			copy_info(info_str,
				"  Vendor: %8.8s Product: %16.16s "
				"Revision: %4.4s\n",
				&targ->scsi_cmdset.inquiry[8],
				&targ->scsi_cmdset.inquiry[16],
				&targ->scsi_cmdset.inquiry[32]);
		}
		break;

	case ASD_TRANSPORT_STP:
	case ASD_TRANSPORT_ATA:
		hd_driveidp = &targ->ata_cmdset.adp_hd_driveid;
		copy_info(info_str, "SATA End Device. ");
		copy_info(info_str, "Mapped SAS Address: %0llx\n", asd_be64toh(
			  *((uint64_t *)targ->ddb_profile.sas_addr)));
		asd_dump_indent(info_str, indent);
		copy_info(info_str,
			"  Vendor: %8.8s Product: %16.16s "
			"Revision: %4.4s\n",
			hd_driveidp->model,
			hd_driveidp->model + 8,
			hd_driveidp->fw_rev);
		break;

	case ASD_TRANSPORT_SMP:
		switch (targ->management_type)
		{
		case ASD_DEVICE_EDGE_EXPANDER:
			copy_info(info_str, "Edge Expander Device. ");
			copy_info(info_str, "SAS Address: %0llx\n", asd_be64toh(
				  *((uint64_t *)targ->ddb_profile.sas_addr)));
			break;

		case ASD_DEVICE_FANOUT_EXPANDER:
			copy_info(info_str, "Fanout Expander Device. ");
			copy_info(info_str, "SAS Address: %0llx\n", asd_be64toh(
				  *((uint64_t *)targ->ddb_profile.sas_addr)));
			break;

		default:
			copy_info(info_str, "Unknown Device.");
			break;
		}

		asd_dump_indent(info_str, indent);
		copy_info(info_str,
			"  Vendor: %8.8s Product: %16.16s Revision: %4.4s - ",
			targ->smp_cmdset.manufacturer_info.VendorIdentification,
			targ->smp_cmdset.manufacturer_info.
				ProductIdentification,
			targ->smp_cmdset.manufacturer_info.	
				ProductRevisionLevel);
		break;

	default:
		copy_info(info_str, "Unknown Device.");
		break;
	}

	if (targ->management_type == ASD_DEVICE_END) {
		asd_dump_indent(info_str, indent);
		copy_info(info_str,
			  "  Host: %d, Channel: %d, Id: %d, LUN: %d\n",
			  asd->platform_data->scsi_host->host_no,
			  targ->src_port->id,
			  targ->target_id,
			  0);
		return;
	}

	copy_info(info_str, "Total Phys: %d\n", targ->num_phys);

	asd_dump_indent(info_str, indent);

	copy_info(info_str, "  Routing: ");

	discover = NULL;

	for (i = 0 ; i < targ->num_phys ; i++) {

		discover = &(targ->Phy[i].Result);

		switch (discover->RoutingAttribute) {
		case DIRECT:
			copy_info(info_str, "%d:D", i);
			break;
		case SUBTRACTIVE:
			copy_info(info_str, "%d:S", i);
			break;
		case TABLE:
			copy_info(info_str, "%d:T", i);
			break;
		default:
			copy_info(info_str, "%d:?", i);
			break;
		}

		if (i != (targ->num_phys - 1)) {
			copy_info(info_str, "|");
		}
	}

	copy_info(info_str, "\n");

	indent++;

	list_for_each_entry(child_target, &targ->children, siblings) {

		for (i = 0 ; i < targ->num_phys ; i++) {

			discover = &(targ->Phy[i].Result);

			if (SAS_ISEQUAL(child_target->ddb_profile.sas_addr,
				discover->AttachedSASAddress)) {
				break;
			}
		}

		if (i == targ->num_phys) {
			continue;
		}

		asd_dump_indent(info_str, indent);

		copy_info(info_str, "+ Phy %d ", i);
		copy_info(info_str, "link rate negotiated: ");

		asd_dump_conn_rate(info_str,
			discover->NegotiatedPhysicalLinkRate);

		copy_info(info_str, " max: ");

		asd_dump_conn_rate(info_str,
			discover->HardwareMaximumPhysicalLinkRate);

		copy_info(info_str, " min: ");

		asd_dump_conn_rate(info_str,
			discover->HardwareMinimumPhysicalLinkRate);

		copy_info(info_str, "\n");

		asd_dump_target_info(asd, info_str, port, child_target, 
			indent);
	}
}
	
static void
asd_dump_port_info(struct asd_softc *asd, proc_info_str_t *info_str, 
		   int port_id)
{
	struct asd_port		*port;
	struct asd_phy		*phy;

	port = asd->port_list[port_id];
	
	copy_info(info_str, "Port %d Settings\n", port_id);
	if (!list_empty(&port->phys_attached)) {
		/* Dump out info for every phy connected to this port. */
		list_for_each_entry(phy, &port->phys_attached, links) {
			asd_dump_indent(info_str, 1);
			copy_info(info_str, "+ Phy %d link rate "
				  "negotiated: %d Mb/s "
				  "max: %d Mb/s min %d Mb/s\n", 
				  phy->id, (phy->conn_rate / 100000), 
				  (phy->max_link_rate / 100000),
				  (phy->min_link_rate / 100000));
			asd_dump_indent(info_str, 1);
			copy_info(info_str, "  Phy SAS Address: %0llx\n", 
				  asd_be64toh(*((uint64_t *)
					      phy->sas_addr)));
		}

		if (port->tree_root != NULL) {

			asd_dump_target_info(asd, info_str, port, 
				port->tree_root, 1);
		}
		else {
			copy_info(info_str, "\n");
		}
	} else {
		copy_info(info_str, "\n");
	}
	
	copy_info(info_str, "\n");
}

/*
 * asd_proc_info()
 *
 * Description:
 *	Entry point for read and write operations to our driver node in the 
 *	procfs filesystem.
 */
static int
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
asd_proc_info(char *proc_buffer, char **proc_start, off_t proc_offset,
	     int proc_length, int proc_hostno, int proc_out)
#else
asd_proc_info(struct Scsi_Host *scsi_host, char *proc_buffer, char **proc_start,
	     off_t proc_offset, int proc_length, int proc_out)
#endif
{
	struct asd_softc  	*asd;
	proc_info_str_t		 info_str;
	int			 retval;
	int			 len;
	int			 i;
	
	retval = -ENOSYS;
	len = 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	list_for_each_entry(asd, &asd_hbas, link) {
		if (asd->platform_data->scsi_host->host_no == proc_hostno)
			break;
	}
#else
	asd = asd_get_softc(*(struct asd_softc **) scsi_host->hostdata);
#endif
	if (asd == NULL)
		goto exit;
	
	if (proc_out) {
		/* 
		 * No support for write yet.
		 */
		retval = len;
	      	goto exit;
     	}

	*proc_start = proc_buffer;

	info_str.buf = proc_buffer;
	info_str.len = proc_length;
	info_str.off = proc_offset;	
	info_str.pos = 0;	

	copy_info(&info_str, "\nAdaptec Linux SAS/SATA Family Driver\n");
	copy_info(&info_str, "Rev: %d.%d.%d-%d\n", 
		  ASD_MAJOR_VERSION, ASD_MINOR_VERSION, 
		  ASD_BUILD_VERSION, ASD_RELEASE_VERSION);
	copy_info(&info_str, "Controller WWN: %0llx\n",
		  asd_be64toh(*((uint64_t *) asd->hw_profile.wwn)));
	copy_info(&info_str, "\n");
	
	for (i = 0; i < asd->hw_profile.max_ports; i++)
		asd_dump_port_info(asd, &info_str, i);		
		
	copy_info(&info_str, "\n");

	retval = info_str.pos > info_str.off ? info_str.pos - info_str.off : 0;
exit:
	return (retval);
}

#if !defined(__VMKLNX__)
/*
 * __bread is not implemented in vmklinux.
 * I checked the code and Samdeep as well.
 * we dont really call this function: asd_bios_param.
 * I justed commented out this function to let the compilation
 * go through.
 */
static int
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
asd_bios_param(struct scsi_device *sdev, struct block_device *bdev,
	       sector_t capacity, int geom[])
{
	unsigned char		*res;
#else
asd_bios_param(Disk *disk, kdev_t dev, int geom[])
{
	struct block_device	*bdev;
	u_long			capacity = disk->capacity;
#endif
	struct buffer_head	*bh;
	int			ret;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
	if (bdev == NULL) {
		return -1;
	}

	res = kmalloc(66, GFP_KERNEL);

	if (res == NULL) {
		return -1;
	}

	bh = __bread(bdev, 0, block_size(bdev));
#else
	bh = bread(MKDEV(MAJOR(dev), MINOR(dev)&~0xf), 0, block_size(dev));
#endif

	if (bh == NULL) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
		kfree(res);
#endif
		return -1;
	}

	/*
	 * try to infer mapping from partition table
	 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
	memcpy(res, bh->b_data + 0x1be, 66);

	ret = scsi_partsize(res, (unsigned long)capacity,
		(unsigned int *)geom + 2, (unsigned int *)geom + 0,
		(unsigned int *)geom + 1);
#else
	ret = scsi_partsize(bh, (unsigned long)capacity,
		(unsigned int *)geom + 2, (unsigned int *)geom + 0,
		(unsigned int *)geom + 1);
#endif

	brelse(bh);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
	kfree(res);
#endif

	if ((ret == 0) && (geom[0] <= 255) && (geom[1] <= 63)) {
		/*
		 * The partition table had the right answer.
		 */
		return 0;
	}

	/*
	 * if something went wrong, then apparently we have to return
	 * a geometry with more than 1024 cylinders
	 */
	geom[0] = 64; // heads
	geom[1] = 32; // sectors
	geom[2] = asd_sector_div(capacity, geom[0], geom[1]);

	if (geom[2] >= 1024) {
		geom[0] = 255;
		geom[1] = 63;
		geom[2] = asd_sector_div(capacity, geom[0], geom[1]);
	}

	if (capacity > 65535*63*255) {
		geom[2] = 65535;
	} else {
		geom[2] = (unsigned long)capacity / (geom[0] * geom[1]);
	}

#if 0
	geom[0] = 64; // heads
	geom[1] = 32; // sectors
	geom[2] = asd_sector_div(capacity, geom[0], geom[1]);
#endif

	return 0;
}
#endif /* if !defined(__VMKLNX__) */

/*************************** ASD Scsi Host Template ***************************/

static Scsi_Host_Template asd_sht = {
	.module			= THIS_MODULE,
	.name			= ASD_DRIVER_NAME,
	.proc_info		= asd_proc_info,
	.proc_name		= ASD_DRIVER_NAME,
	.info			= asd_info,
	.queuecommand		= asd_queue,
	.eh_abort_handler	= asd_abort,
	.can_queue		= 2,
	.this_id		= -1,
	.max_sectors		= ASD_MAX_SECTORS,
	.cmd_per_lun		= 2,
	.use_clustering		= ENABLE_CLUSTERING,
#if !defined(__VMKLNX__)
	.bios_param             = asd_bios_param,
#endif /* #if !defined(__VMKLNX__) */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
	.slave_alloc		= asd_slave_alloc,
	.slave_configure	= asd_slave_configure,
	.slave_destroy		= asd_slave_destroy,
#else
	.detect			= asd_detect,
	.release		= asd_release,
	.select_queue_depths	= asd_select_queue_depth,
	.use_new_eh_code	= 1,
#endif		
};


module_init(asd_init);
module_exit(asd_exit);
