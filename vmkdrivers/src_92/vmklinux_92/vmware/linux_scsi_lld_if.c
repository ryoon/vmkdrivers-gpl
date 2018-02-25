/* ****************************************************************
 * Portions Copyright 1998, 2009-2013 VMware, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * ****************************************************************/

/******************************************************************
 *
 *  linux_scsi_lld_if.c
 *
 *      Linux scsi mid layer emulation. Covers all functions used by LLD's 
 *      except for the transport part
 *
 * From linux-2.6.18-8/drivers/scsi/hosts.c:
 *
 * Copyright (C) 1992 Drew Eckhardt
 * Copyright (C) 1993, 1994, 1995 Eric Youngdale
 * Copyright (C) 2002-2003 Christoph Hellwig
 *
 * From linux-2.6.18-8/drivers/scsi/scsi.c:
 *
 * Copyright (C) 1992 Drew Eckhardt
 * Copyright (C) 1993, 1994, 1995, 1999 Eric Youngdale
 * Copyright (C) 2002, 2003 Christoph Hellwig
 *
 * From linux-2.6.18-8/drivers/scsi/scsi_error.c:
 *
 * Copyright (C) 1997 Eric Youngdale
 *
 * From linux-2.6.18-8/drivers/scsi/scsi_scan.c:
 *
 * Copyright (C) 2000 Eric Youngdale,
 * Copyright (C) 2002 Patrick Mansfield
 *
 * From linux-2.6.18-8/drivers/scsi/scsi_lib.c:
 *
 * Copyright (C) 1999 Eric Youngdale
 *
 ******************************************************************/

#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_dbg.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_eh.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_tcq.h>
#include <scsi/scsi_transport.h>
#include <linux/log2.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <asm/proto.h>

#include "vmkapi.h"

#include "linux_scsi.h" 
#include "linux_stubs.h"
#include "linux_scsi_transport.h"
#include "linux_pci.h"
#include "linux_irq.h"
#include "scsi_logging.h"
#include "scsi_priv.h"

#define VMKLNX_LOG_HANDLE LinScsiLLD
#include "vmklinux_log.h"

/* Max. scsi_scan_host delay for pending scans, 30 Sec., in jiffies */
#define SCSI_SCAN_HOST_MAX_WAITQ_DELAY (30*HZ)

static const char *scsi_null_device_strs = "nullnullnullnull";

/* Command group 3 is reserved and should never be used.  */
const unsigned char scsi_command_size[8] =
{
        6, 10, 10, 12,
        16, 12, 10, 10
};
EXPORT_SYMBOL(scsi_command_size);

/* 
 * A blank transport template that is used in drivers that don't
 * yet implement Transport Attributes 
 */
struct scsi_transport_template blank_transport_template = { { { {NULL, }, }, }, };

/*
 * scsi_cmd slab pool for this vmklinux
 */
static struct scsi_host_cmd_pool scsi_cmd_dma_pool = {
	.name		= VMKLNX_MODIFY_NAME(scsi_cmd_cache),
	.objSize	= sizeof(struct scsi_cmnd),
};

struct vmklnx_scsiqdepth_event {
   struct work_struct work;
   vmk_ScsiAdapter *vmkAdapter;
   unsigned int channel;
   unsigned int id;
   unsigned int lun;
   vmk_uint32 tags;
};

struct mutex host_cmd_pool_mutex;

/*
 * SCSI device types
 */
const char *const scsi_device_types[MAX_SCSI_DEVICE_CODE] = {
   "Direct-Access    ",
   "Sequential-Access",
   "Printer          ",
   "Processor        ",
   "WORM             ",
   "CD-ROM           ",
   "Scanner          ",
   "Optical Device   ",
   "Medium Changer   ",
   "Communications   ",
   "Unknown          ",
   "Unknown          ",
   "RAID             ",
   "Enclosure        ",
   "Direct-Access-RBC",
};
EXPORT_SYMBOL(scsi_device_types);

/*
 * externs
 */
extern struct list_head	linuxSCSIAdapterList;
extern vmk_SpinlockIRQ linuxSCSIAdapterLock;
extern struct workqueue_struct *linuxSCSIWQ;

/*
 * Local
 */
static int scsi_setup_command_freelist(struct Scsi_Host *sh);
static void scsi_destroy_command_freelist(struct Scsi_Host *sh);
static void scsi_forget_host(struct Scsi_Host *sh);
static void device_unblock(struct scsi_device *sdev, void *data);
static void device_block(struct scsi_device *sdev, void *data);
static void __scsi_remove_target(struct scsi_target *starget);
static void scsi_host_dev_release(struct device *dev);
static void scsi_device_dev_release(struct device *dev);
static void scsi_target_dev_release(struct device *dev);
static void vmklnx_scsi_unregister_host(struct work_struct *work);
static void vmklnx_scsi_free_host_resources(struct Scsi_Host *sh);
static void vmklnx_clear_scmd(struct scsi_cmnd *scmd);
static void scsi_offline_device(struct scsi_device *sdev);
static void vmklnx_scsi_update_lun_path(struct scsi_device *sdev, void *data);
static inline void vmklnx_init_scmd(struct scsi_cmnd *scmd, struct scsi_device *dev);

/**                                          
 *  scsi_host_alloc - allocate a Scsi_Host structure       
 *  @sht: pointer to scsi host template    
 *  @privateSize: additional size to be allocated as requested by the driver   
 *                                           
 *  Allocate a Scsi_Host structure
 *                                           
 *  ESX Deviation Notes:
 *  This interface will assume a default value for
 *  Scsi_Host->dma_boundary to be 0 if the Scsi Host template does
 *  not specify a value for dma_boundary. This is different from
 *  the linux behavior which defaults to a 4G boundary in a similar
 *  situation.
 *
 *  RETURN VALUE:
 *	On Success pointer to the newly allocated Scsi_Host structure,
 *  otherwise NULL
 */                                          
/* _VMKLNX_CODECHECK_: scsi_host_alloc */
struct Scsi_Host *
scsi_host_alloc(struct scsi_host_template *sht, int privateSize)
{
   struct Scsi_Host *sh = NULL;
   vmk_ScsiAdapter *vmkAdapter;
   int retval = 0;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(sht);

   VMKLNX_DEBUG(0, "SHT name = %s", sht->name);

   sh = (struct Scsi_Host *)vmklnx_kzmalloc(vmklnxLowHeap, sizeof(*sh) + privateSize, GFP_KERNEL);

   if (sh == NULL) {
      VMKLNX_DEBUG(0, "Memory allocation failed for %s", sht->name);
      goto failed_sh_alloc;
   }

   vmkAdapter = vmk_ScsiAllocateAdapter();
   if (vmkAdapter == NULL) {
      VMKLNX_WARN("Could not allocate vmkAdapter %s", sht->name);
      goto failed_vmk_alloc;
   }

   /*
    * Initialize values as done in Linux. We dont want to alter the default
    * values unless there is a strong reason
    */
   sh->shost_gendev.dev_type = SCSI_HOST_TYPE;
   sh->host_lock = &sh->default_lock;
   spin_lock_init(sh->host_lock);
   sh->shost_state = SHOST_CREATED;
   INIT_LIST_HEAD(&sh->__devices);
   INIT_LIST_HEAD(&sh->__targets);
   INIT_LIST_HEAD(&sh->eh_cmd_q);
   INIT_LIST_HEAD(&sh->starved_list);
   INIT_LIST_HEAD(&sh->sht_legacy_list);
   init_waitqueue_head(&sh->host_wait);
   mutex_init(&sh->scan_mutex);

   sh->vmkAdapter = vmkAdapter;
   sh->host_no = vmkAdapter->hostNum;
   sh->dma_channel = 0xff;

   sh->max_channel = 0;
   sh->max_id = 8;
   sh->max_lun = 8;

   sh->transportt = &blank_transport_template;

   sh->max_cmd_len = 12;
   sh->hostt = sht;
   sh->this_id = sht->this_id;
   sh->can_queue = sht->can_queue;
   sh->sg_tablesize = sht->sg_tablesize;
   sh->cmd_per_lun = sht->cmd_per_lun;
   sh->unchecked_isa_dma = sht->unchecked_isa_dma;
   sh->use_clustering = sht->use_clustering;
   sh->ordered_tag = sht->ordered_tag;

   if (sht->max_host_blocked) {
      sh->max_host_blocked = sht->max_host_blocked;
   } else {
      sh->max_host_blocked = SCSI_DEFAULT_HOST_BLOCKED;
   }

   /*
    * If the driver imposes no hard sector transfer limit, start at
    * machine infinity initially.
    */
   if (sht->max_sectors) {
      sh->max_sectors = sht->max_sectors;
   } else {
      sh->max_sectors = SCSI_DEFAULT_MAX_SECTORS;
   }

   if (sht->dma_boundary) {
      sh->dma_boundary = sht->dma_boundary;
   } else {
      /* Linux has a 4G default boundary */
      sh->dma_boundary = DMA_32BIT_MASK;
   }

   retval = scsi_setup_command_freelist(sh);
   if (retval) {
      VMKLNX_WARN("Failed to set command list for %s", sht->name);
      goto failed_free_list_setup;
   }

   /*
    * scsi stack and device are quite interleaved
    * No option but to take it
    */
   device_initialize(&sh->shost_gendev);
   snprintf(sh->shost_gendev.bus_id, BUS_ID_SIZE, "host%d", sh->host_no);
   sh->shost_gendev.release = scsi_host_dev_release;

   if (sht->enable_eh) {
      sh->ehandler = kthread_run(scsi_error_handler, sh,
                                 "scsi_eh_%d", sh->host_no);
      if (IS_ERR(sh->ehandler)) {
         VMKLNX_WARN("Failed to create EH handler for %s", sht->name);
         goto failed_error_handler_create;
      }
   }

   scsi_proc_hostdir_add(sh->hostt);
   vmk_AtomicWrite64(&sh->pendingScanWorkQueueEntries, 0);

   return sh;

failed_error_handler_create:
   scsi_destroy_command_freelist(sh);
failed_free_list_setup:
   vmk_ScsiFreeAdapter(vmkAdapter);
failed_vmk_alloc:
   vmklnx_kfree(vmklnxLowHeap, sh);
failed_sh_alloc:
   return NULL;
}
EXPORT_SYMBOL(scsi_host_alloc);

/**
 * Setup the command freelist
 * @sh: host to allocate the freelist for
 *
 * RETURN VALUE:
 * 0 on success
 *
 * Include:
 * scsi/scsi_host.h
 *
 * ESX Deviation Notes:
 * Our scsi_cmnd cache also includes space for the maximally sized
 * scatterlist array.
 */
static int scsi_setup_command_freelist(struct Scsi_Host *sh)
{
   struct scsi_host_cmd_pool *pool;
   struct scsi_cmnd *cmd = NULL;

   VMK_ASSERT(sh);

   spin_lock_init(&sh->free_list_lock);
   INIT_LIST_HEAD(&sh->free_list);

   /*
    * Select a command slab for this host and create it if not
    * yet existant.
    */
   mutex_lock(&host_cmd_pool_mutex);
   pool = &scsi_cmd_dma_pool;
   if (!pool->users) {
      pool->moduleID = vmklinuxModID;
      pool->slab = vmkplxr_ScsiCmdSlabCreate(pool);
      if (!pool->slab) {
         goto fail;
      }
   }

   pool->users++;
   sh->cmd_pool = pool;
   mutex_unlock(&host_cmd_pool_mutex);

   /*
    * Get one backup command for this host.
    */
   cmd = vmk_SlabAlloc(sh->cmd_pool->slab);
   if (!cmd) {
	goto fail3;
   }
   list_add(&cmd->list, &sh->free_list);		
   return 0;

fail3:
   mutex_lock(&host_cmd_pool_mutex);
   sh->cmd_pool = NULL;
   pool->users--;

   if (!pool->users) {
      vmk_SlabDestroy(pool->slab);
   }

fail:
   mutex_unlock(&host_cmd_pool_mutex);
   return -ENOMEM;
}

static int
vmklnx_setup_adapter_tls(struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter)
{
   vmk_ScsiAdapter *vmkAdapter = vmklnx26ScsiAdapter->vmkAdapter;
   vmk_ScsiCompObjectInfo *compObjectInfo;
   void **tls;
   int i, numCompObjects;
   int status = 0;

   spin_lock_init(&vmklnx26ScsiAdapter->lock);
   numCompObjects = vmk_ScsiGetNumCompObjects();
   tls = vmklnx_kzmalloc(vmklnxLowHeap, numCompObjects * sizeof(*tls), GFP_KERNEL);
   if (tls == NULL) {
      vmk_WarningMessage("Disabling Worldlet Completion mode for %s\n",
                         vmk_ScsiGetAdapterName(vmkAdapter));
      return status;
   }
   memset(tls, 0, numCompObjects * sizeof(*tls));

   for (i = 0; i < numCompObjects; i++) {
      char name[VMK_MISC_NAME_MAX + sizeof "-255"];

      vmk_Snprintf(name, sizeof(name), "%s-%d",
                   vmk_ScsiGetAdapterName(vmkAdapter), i);
      tls[i] = SCSILinuxTLSWorldletCreate(name, vmklnx26ScsiAdapter);
      if (tls[i] == NULL) {
         status = VMK_FAILURE;
         goto tls_create_error;
      }
   }
   compObjectInfo = vmklnx_kzmalloc(vmklnxLowHeap,
                                    numCompObjects * sizeof *compObjectInfo,
				    GFP_KERNEL);
   if (compObjectInfo == NULL) {
      status = VMK_NO_MEMORY;
      goto tls_create_error;
   }
   for (i = 0; i < numCompObjects; i++) {
      compObjectInfo[i].completionHandle.ptr = tls[i];
      compObjectInfo[i].completionWorldlet = SCSILinuxTLSGetWorldlet(tls[i]);
   }
   status = vmk_ScsiRegisterCompObjects(vmklnx26ScsiAdapter->vmkAdapter,
                                        numCompObjects, compObjectInfo);
   vmklnx_kfree(vmklnxLowHeap, compObjectInfo);
   if (status == VMK_OK) {
      vmklnx26ScsiAdapter->tls = (struct scsiLinuxTLS **)tls;
      vmklnx26ScsiAdapter->numTls = numCompObjects;
      return status;
   }

tls_create_error:

   for (i = 0; i < numCompObjects; i++) {
      if (tls[i] != NULL) {
         SCSILinuxTLSWorldletDestroy(tls[i]);
      }
   }
   vmklnx_kfree(vmklnxLowHeap, tls);

   return status;
}


/**                                          
 *  scsi_add_host - Adds a previously alloced Scsi_Host structure to 
 *  the storage subsystem
 *
 *  @sh: host to register 
 *  @pDev: pointer to struct device
 *                                           
 *  Adds a previously alloced Scsi_Host structure to the storage subsystem
 *                                           
 *  RETURN VALUE:
 *  Returns 0 on success, non zero on failure
 *
 *  SEE ALSO:
 *  scsi_host_alloc
 */

/* _VMKLNX_CODECHECK_: scsi_add_host */
int 
scsi_add_host(struct Scsi_Host *sh, struct device *pDev)
{
   struct pci_dev *pciDev = NULL;
   struct pci_dev *pciDevForAdapter = NULL;
   vmk_ScsiAdapter *vmkAdapter;
   struct scsi_host_template *sht;
   int error = -EINVAL;
   VMK_ReturnStatus status;
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter;
   struct vmklnx_ScsiModule *vmklnx26ScsiModule;
   unsigned vmkFlag;
   vmklnx_ScsiTransportType transportType;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(sh);

   sht = sh->hostt;
   VMK_ASSERT(sht);
   
   vmkAdapter = sh->vmkAdapter;
   VMK_ASSERT(vmkAdapter);

   if ((!sht->name) || (!sht->queuecommand) || (!sh->sg_tablesize) ||
	(!sh->can_queue)) {
      VMKLNX_WARN("Insufficient param's for SCSI registration");
      goto out;
   }

   if (!pDev) {
       VMKLNX_WARN("This seems like legacy adapter. Legacy adapters"
                   "are not supported in this driver model");
      goto out;
   }

   vmklnx26ScsiModule = (struct vmklnx_ScsiModule *)sh->transportt->module;

   transportType = VMKLNX_SCSI_TRANSPORT_TYPE_UNKNOWN;
   if (sh->xportFlags) {
      /* Scsi_Host->xportFlags overrides vmklnx_ScsiModule->transportType */
      transportType = sh->xportFlags;
   } else if (vmklnx26ScsiModule) {
      transportType = vmklnx26ScsiModule->transportType;
      sh->xportFlags = vmklnx26ScsiModule->transportType;
   }

   if (!sh->shost_gendev.parent) {
      sh->shost_gendev.parent = pDev;
   }

   error = device_add(&sh->shost_gendev);
   if (error) {
      VMKLNX_WARN("Failed to add device");
      VMK_ASSERT(FALSE);
      goto out;
   }

   /*
    * Take additional self reference as in linux
    * This is weird, but drivers release the ref count after they call
    * scsi_remove_host
    */
   get_device(&sh->shost_gendev);
   /*
    * Get a ref count on parent
    */
   get_device(sh->shost_gendev.parent);   

   vmklnx26ScsiAdapter = (struct vmklnx_ScsiAdapter *)
		 kzalloc(sizeof(struct vmklnx_ScsiAdapterInt), GFP_KERNEL);

   if (vmklnx26ScsiAdapter == NULL) {
      VMKLNX_WARN("Memory allocation failed for %s", sht->name);
      error = -ENOMEM;
      goto release_device;
   }
   /*
    * Store pointer to our data struct for retrieval
    */
   sh->adapter = vmklnx26ScsiAdapter;

   atomic_set(&vmklnx26ScsiAdapter->tmfFlag, 0);

   /*
    * Allocate memory for transport
    */
   if (sh->transportt->host_size &&
    (sh->shost_data = kzalloc(sh->transportt->host_size, GFP_KERNEL))
				 == NULL) {
      error = -ENOMEM;
      goto release_vmklnx;
   }

   /*
    * Create work queue for all hosts (i.e ignore create_work_queue bit)
    * queue depth events will be added to this work queue, that way all
    * the work pending for a host can be flushed before cleaning up the host.
    */
   snprintf(sh->work_q_name, KOBJ_NAME_LEN, "scsi_wq_%d", sh->host_no);
   sh->work_q = create_singlethread_workqueue(sh->work_q_name);
   if (!sh->work_q) {
      error = -ENOSPC;
      goto release_transport;
   }

   vmkAdapter->discover = SCSILinuxDiscover;
   vmkAdapter->command = SCSILinuxCommand;
   vmkAdapter->taskMgmt = SCSILinuxTaskMgmt;
   vmkAdapter->dumpCommand = SCSILinuxDumpCommand;
   vmkAdapter->procInfo = SCSILinuxProcInfo;
   vmkAdapter->close = SCSILinuxClose;
   vmkAdapter->dumpQueue = SCSILinuxDumpQueue;
   vmkAdapter->dumpPollHandler = SCSILinuxDummyDumpPoll;
   vmkAdapter->dumpPollHandlerData = LINUX_BHHANDLER_NO_IRQS;
   vmkAdapter->ioctl = SCSILinuxIoctl;
   vmkAdapter->vportop = SCSILinuxVportOp;
   vmkAdapter->vportDiscover = SCSILinuxVportDiscover;
   vmkAdapter->modifyDeviceQueueDepth = SCSILinuxModifyDeviceQueueDepth;
   vmkAdapter->queryDeviceQueueDepth = SCSILinuxQueryDeviceQueueDepth;
   vmkAdapter->checkTarget = SCSILinuxCheckTarget;

   VMK_ASSERT(sh->sg_tablesize);

   vmkAdapter->constraints.maxTransfer = (vmk_ByteCount)-1;
   if (sh->sg_tablesize > SG_ALL) {
      VMKLNX_WARN("vmkAdapter (%s) sgMaxEntries rounded to %d. "
                  "Reported size was %d",
                  sht->name, SG_ALL, sh->sg_tablesize);
      sh->sg_tablesize = SG_ALL;
   }
   vmkAdapter->constraints.sgMaxEntries = sh->sg_tablesize;
   vmkAdapter->constraints.sgElemSizeMult = sh->sg_elem_size_mult;
   vmkAdapter->constraints.sgElemStraddle = sh->dma_boundary;
   if (vmkAdapter->constraints.sgElemStraddle != 0 &&
       !is_power_of_2(vmkAdapter->constraints.sgElemStraddle)) {
      vmkAdapter->constraints.sgElemStraddle++;
   }
   vmkAdapter->constraints.addressMask = VMK_ADDRESS_MASK_64BIT;

   vmkAdapter->channels = sh->max_channel + 1;
   vmkAdapter->maxTargets = sh->max_id;
   vmkAdapter->maxLUNs = sh->max_lun;
   vmkAdapter->maxCmdLen = sh->max_cmd_len;

   vmkAdapter->qDepthPtr = (vmk_uint32 *)&(sh->can_queue);
   vmkAdapter->targetId = sh->this_id;

   vmkAdapter->hostMaxSectors = sh->max_sectors;

   if ((transportType ==  VMKLNX_SCSI_TRANSPORT_TYPE_FC)  ||
        (transportType == VMKLNX_SCSI_TRANSPORT_TYPE_FCOE) ||
        (transportType ==  VMKLNX_SCSI_TRANSPORT_TYPE_SAS) ||
        (transportType ==  VMKLNX_SCSI_TRANSPORT_TYPE_ISCSI) ||
        (transportType == VMKLNX_SCSI_TRANSPORT_TYPE_XSAN)) {
      /*
       * Enable Periodic scan on FC, iSCSI and SAS adapters
       * Also, disable initial scan from vmkernel. We will make a scan request 
       * when we really need one
       */
      vmkAdapter->flags = VMK_SCSI_ADAPTER_FLAG_REGISTER_WITHOUT_SCAN;
   } else {
      /*
       * Set with no periodic scan.
       */
      vmkAdapter->flags = VMK_SCSI_ADAPTER_FLAG_REGISTER_WITHOUT_SCAN | 
				VMK_SCSI_ADAPTER_FLAG_NO_PERIODIC_SCAN;
   }

   vmkAdapter->clientData = vmklnx26ScsiAdapter; 

   vmklnx26ScsiAdapter->shost = sh;
   vmklnx26ScsiAdapter->vmkAdapter = vmkAdapter;

   if (pDev->dev_type == PCI_DEVICE_TYPE) {
      pciDev = pciDevForAdapter = to_pci_dev(pDev);
   } else {
      struct device *parent;

      for (parent = pDev->parent; parent != NULL; parent = parent->parent) {
         if (parent->dev_type == PCI_DEVICE_TYPE) {
            pciDevForAdapter = to_pci_dev(parent);
            break;
         }
      }
   }

   if (pDev->dev_type == FC_VPORT_TYPE) {
      SCSILinuxVportUpdate(sh, pDev);
      vmkAdapter->flags |= VMK_SCSI_ADAPTER_FLAG_NPIV_VPORT;
   }

   if (pciDevForAdapter != NULL) {
      vmk_PCIDevice vmkPCIDevice;

      pciDevForAdapter->dev.dma_boundary = sh->dma_boundary;

      if (LinuxPCI_DeviceIsPAECapable(pciDevForAdapter)) {
         vmkAdapter->paeCapable = TRUE;
         VMK_ASSERT(vmkAdapter->constraints.addressMask == VMK_ADDRESS_MASK_64BIT);
      } else {
         vmkAdapter->paeCapable = FALSE;
         vmkAdapter->constraints.addressMask = VMK_ADDRESS_MASK_32BIT;
      }

      status = vmk_PCIGetPCIDevice(pci_domain_nr(pciDevForAdapter->bus),
                                   pciDevForAdapter->bus->number,
                                   PCI_SLOT(pciDevForAdapter->devfn),
                                   PCI_FUNC(pciDevForAdapter->devfn),
                                   &vmkPCIDevice);
      VMK_ASSERT(status == VMK_OK);
      status = vmk_PCIGetGenDevice(vmkPCIDevice, &vmkAdapter->device);
      VMK_ASSERT(status == VMK_OK);
   }

   /*
    * The module id is based on looking up the driver name.
    * pciDev is NULL for pseudo adapters
    */
   vmkAdapter->moduleID = SCSILinuxGetModuleID(sh, pciDev);

   if (sh->useDriverNamingDevice) {
      SCSILinuxSetName(vmkAdapter, sh, pciDev);
   } else {
      SCSILinuxNameAdapter(&vmkAdapter->name, pciDev);
   }
   vmk_NameFormat(&vmkAdapter->procName, "%s", sht->proc_name ? sht->proc_name : "");

   status = vmk_ScsiRegisterAdapter(vmkAdapter);

   /*
    * If chosen name is unavailable, let's autogenerate one
    */
   if (status == VMK_EXISTS) {
      SCSILinuxNameAdapter(&vmkAdapter->name, NULL);
      status = vmk_ScsiRegisterAdapter(vmkAdapter);
   }

   if (status != VMK_OK) {
      VMKLNX_WARN("Register vmkAdapter failed with 0x%x for %s",
                  status, sht->name);
      error = -EINVAL;
      goto release_wq;
   }

   /*
    * If this is a valid SCSI transport, check for any specific vmkernel
    * mgmtAdapter registration
    *
    * sh->xportFlags can have multiple transport types, in theory. As of now we
    * only handle one. In the future, if multiple transports need to be handled,
    * the below section of code will have to be modified to handle that. For
    * example, if xportFlags has "VMKLNX_SCSI_TRANSPORT_TYPE_SAS |
    * VMKLNX_SCSI_TRANSPORT_TYPE_PSCSI", then that will have to be handled based
    * on how the user of that value wants it.
    */
   switch (transportType) {
   case VMKLNX_SCSI_TRANSPORT_TYPE_FC:
      error = vmklnx_fc_host_setup(sh);
      if (error) {
         VMKLNX_WARN("Failed to register FC mgmt Adapter '%s': %d",
                     sht->name, error);
         goto release_prealloccmds;
      }
      break;
   case VMKLNX_SCSI_TRANSPORT_TYPE_SAS:
      error = vmklnx_sas_host_setup(sh);
      if (error) {
         VMKLNX_WARN("Failed to register SAS Adapter '%s': %d",
                     sht->name, error);
         goto release_prealloccmds;
      }
      break;
   case VMKLNX_SCSI_TRANSPORT_TYPE_PSCSI:
      vmkAdapter->mgmtAdapter.transport = VMK_STORAGE_ADAPTER_PSCSI;
      break;
   case VMKLNX_SCSI_TRANSPORT_TYPE_USB:
      vmkAdapter->mgmtAdapter.transport = VMK_STORAGE_ADAPTER_USB;
      break;
   case VMKLNX_SCSI_TRANSPORT_TYPE_SATA:
      vmkAdapter->mgmtAdapter.transport = VMK_STORAGE_ADAPTER_SATA;
      break;
   case VMKLNX_SCSI_TRANSPORT_TYPE_IDE:
      vmkAdapter->mgmtAdapter.transport = VMK_STORAGE_ADAPTER_IDE;
      break;
   case VMKLNX_SCSI_TRANSPORT_TYPE_XSAN:
      error = vmklnx_xsan_host_setup(sh);
      if (error) {
         VMKLNX_WARN("Failed to register generic SAN mgmt Adapter '%s': %d",
                     sht->name, error);
         goto release_prealloccmds;
      }
      break;
   case VMKLNX_SCSI_TRANSPORT_TYPE_FCOE:
      error = vmklnx_fcoe_host_setup(sh);
      if (error) {
         VMKLNX_WARN("Failed to register FCoE mgmt "
                     "Adapter '%s': %d\n", sht->name, error);
         goto release_prealloccmds;
      }
      break;
   case VMKLNX_SCSI_TRANSPORT_TYPE_UNKNOWN:
   default:
      vmkAdapter->mgmtAdapter.transport =
         VMK_STORAGE_ADAPTER_TRANSPORT_UNKNOWN;
   }

   /* Serialize ioctl calls */
   mutex_init(&vmklnx26ScsiAdapter->ioctl_mutex);

   error = vmklnx_setup_adapter_tls(vmklnx26ScsiAdapter);
   if (error != 0) {
      goto release_prealloccmds;
   }

   vmkFlag = vmk_SPLockIRQ(&linuxSCSIAdapterLock);
   list_add_tail(&vmklnx26ScsiAdapter->entry, &linuxSCSIAdapterList);
   vmk_SPUnlockIRQ(&linuxSCSIAdapterLock, vmkFlag);

   /*
    * Add proc node for this host
    */
   scsi_proc_host_add(sh);

   sh->shost_state = SHOST_RUNNING;
   error = 0;
   goto out;

release_prealloccmds:
   vmk_ScsiUnregisterAdapter(vmkAdapter);
release_wq:
   if (sh->work_q) {
      destroy_workqueue(sh->work_q);
   }
release_transport:
   kfree(sh->shost_data);
release_vmklnx:
   kfree(vmklnx26ScsiAdapter);
   sh->adapter = NULL;
release_device:
   /*
    * scsi_remove_host will be called by the driver to clean up
    * resources
    */
out:
   return error;
}
EXPORT_SYMBOL(scsi_add_host);

/**                                          
 *  scsi_remove_host - Remove a scsi host       
 *  @sh: the scsi host to be removed   
 *                                           
 *  Remove a scsi host                                           
 *
 *  RETURN VALUE:
 *  NONE                                           
 */                                          
/* _VMKLNX_CODECHECK_: scsi_remove_host */
void 
scsi_remove_host(struct Scsi_Host *sh)
{
   unsigned long flags;
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter;
   vmk_ScsiAdapter *vmkAdapter;
   VMK_ReturnStatus status;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(sh);

   VMKLNX_DEBUG(0, "Ref count of %s is %d", 
                   vmklnx_get_vmhba_name(sh),
                   sh->shost_gendev.kref.refcount.counter);

   spin_lock_irqsave(sh->host_lock, flags);
   if (sh->shost_state == SHOST_DEL) {
      spin_unlock_irqrestore(sh->host_lock, flags);
      VMKLNX_DEBUG(2, "called repeatedly on deleting host %s",
                      sh->hostt->name);
      return;
   }
   sh->shost_state = SHOST_DEL;
   spin_unlock_irqrestore(sh->host_lock, flags);

   /*
    * Iterate now to remove all targets and luns
    */
   scsi_forget_host(sh);

   vmklnx26ScsiAdapter = (struct vmklnx_ScsiAdapter *) sh->adapter;
   if (vmklnx26ScsiAdapter) {
      vmkAdapter = vmklnx26ScsiAdapter->vmkAdapter;
      if (vmkAdapter) {
         /*
          * Delete any missing paths. This has the side effect of
          * deleting the adapter if no more paths.
          * This is a automatic way of simulating:
          *    esxcfg-rescan -d vmhba<n>
          */
         status = SCSILinuxDeleteAdapterPaths(vmkAdapter); 
         if (VMK_OK != status) { 
            VMKLNX_WARN("Failed to remove host %s, status: %s", 
                        sh->hostt->name, vmk_StatusToString(status));
         } else {
            VMKLNX_INFO("Removed Host Adapter %s", vmklnx_get_vmhba_name(sh));
         }
      }
   }

   device_del(&sh->shost_gendev);
   put_device(&sh->shost_gendev);
   return;
}
EXPORT_SYMBOL(scsi_remove_host);

/**
 *  vmklnx_scsi_register_poll_handler - register poll handler with storage layer
 *  @sh: a pointer to scsi_host struct
 *  @irq: irq to register
 *  @handler: irq handler to register
 *  @dev_id: callback data for handler
 *
 *  Registers poll handler with storage layer, which is used during coredump.
 *
 *  RETURN VALUE
 *  None
 */
/* _VMKLNX_CODECHECK_: vmklnx_scsi_register_poll_handler */
void vmklnx_scsi_register_poll_handler(struct Scsi_Host *sh,
                                       unsigned int irq,
                                       irq_handler_t handler,
                                       void *dev_id)
{
   VMK_ReturnStatus result;
   vmk_ScsiAdapter *vmkAdapter;
   void *idr;
   struct device *dev = LinuxIRQ_GetDumpIRQDev(); 

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   vmkAdapter = ((struct vmklnx_ScsiAdapter *)sh->adapter)->vmkAdapter;

   /* don't register for psuedo adapters */
   if (sh->shost_gendev.parent->dev_type != PCI_DEVICE_TYPE) {
      VMKLNX_DEBUG(1, "Coredump Poll handler not registered - %s is psuedo-adapter",
                       vmk_ScsiGetAdapterName(vmkAdapter));
      return;
   }

   if (!irq || irq >= NR_IRQS) {
      VMKLNX_WARN("Coredump Poll handler not registered for %s - invalid irq 0x%x",
                   vmk_ScsiGetAdapterName(vmkAdapter), irq);
      return;
   }

   idr = irqdevres_register(dev, irq, handler, dev_id);
   if (!idr) {
      VMKLNX_WARN("Coredump Poll handler not registered for %s - irqdevres_register failed",
                   vmk_ScsiGetAdapterName(vmkAdapter));
      VMK_ASSERT(0);
      return;
   }

   result = vmk_ScsiRegisterIRQ(vmkAdapter, LinuxIRQ_VectorToCookie(irq), Linux_IRQHandler, idr);
   VMK_ASSERT(result == VMK_OK);
   VMKLNX_DEBUG(0, "Coredump Poll handler registered for adpater %s with irq %d",
                    vmk_ScsiGetAdapterName(vmkAdapter), irq);
}
EXPORT_SYMBOL(vmklnx_scsi_register_poll_handler);

/**
 *  vmklnx_scsi_set_path_maxsectors - set the max transfer size for a path
 *  @sdev: a pointer to scsi_device struct representing the target path
 *  @max_sectors: the max transfer size in 512 byte sectors
 *
 *  Sets the maximum transfer size for a path in 512 byte sectors.
 *
 *  RETURN VALUE
 *  None
 */
/* _VMKLNX_CODECHECK_: vmklnx_scsi_set_path_maxsectors */
void
vmklnx_scsi_set_path_maxsectors(struct scsi_device *sdev,
                                unsigned int max_sectors)
{
	vmk_ScsiAdapter *vmkAdapter;
   VMK_ReturnStatus result;
   
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   vmkAdapter = ((struct vmklnx_ScsiAdapter *)sdev->host->adapter)->vmkAdapter;

   result = vmk_ScsiSetPathXferLimit(vmkAdapter,
                                     sdev->channel,
                                     sdev->id,
                                     sdev->lun,
                                     ((vmk_uint64)max_sectors) * VMK_SECTOR_SIZE);
   VMK_ASSERT(result == VMK_OK);
}
EXPORT_SYMBOL(vmklnx_scsi_set_path_maxsectors);

/**
 * scsi_destroy_command_freelist -- Release the command freelist 
 * for a scsi host
 * @sh: host that's freelist is going to be destroyed
 *
 * RETURN VALUE:
 * None
 *
 * Include:
 * scsi/scsi_host.h
 */
static void 
scsi_destroy_command_freelist(struct Scsi_Host *sh)
{
   while (!list_empty(&sh->free_list)) {
      struct scsi_cmnd *cmd;

      cmd = list_entry(sh->free_list.next, struct scsi_cmnd, list);
      list_del_init(&cmd->list);
      vmk_SlabFree(sh->cmd_pool->slab, cmd);
   }

   mutex_lock(&host_cmd_pool_mutex);
   if (!--sh->cmd_pool->users) {
      vmk_SlabDestroy(sh->cmd_pool->slab);
   }
   mutex_unlock(&host_cmd_pool_mutex);
   sh->cmd_pool = NULL;
}

/*
 * All scanning functions needs PSA backend support. Will depend on
 * completion of PR166189
 */
/**                                          
 *  scsi_scan_host - issue wild card scan for the given SCSI host
 *  @sh: SCSI host to scan
 *                                           
 *  Issues a wild card scan for the given SCSI host
 *                                           
 *  RETURN VALUE:
 *  NONE 
 *
 */                                          
/* _VMKLNX_CODECHECK_: scsi_scan_host */
void 
scsi_scan_host(struct Scsi_Host *sh)
{
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter;
   vmk_ScsiAdapter *vmkAdapter;
   struct scsi_device *sdev;
   int ret;
   unsigned long start, duration;
   vmk_uint64 startingEntryCount;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(sh);

   vmklnx26ScsiAdapter = (struct vmklnx_ScsiAdapter *) sh->adapter;
   vmkAdapter = vmklnx26ScsiAdapter->vmkAdapter;
   
   start = jiffies;
   if (sh->hostt->scan_finished) {
      if (sh->hostt->scan_start) {
                        VMKAPI_MODULE_CALL_VOID(SCSI_GET_MODULE_ID(sh), 
                                                sh->hostt->scan_start, sh);
      }
                VMKAPI_MODULE_CALL(SCSI_GET_MODULE_ID(sh), ret, 
                                   sh->hostt->scan_finished, sh, 
                                   jiffies - start);
      while (!ret) {
         vmk_WorldSleep(100000); /* sleep for 100ms */
                        VMKAPI_MODULE_CALL(SCSI_GET_MODULE_ID(sh), ret,
                                           sh->hostt->scan_finished, sh,
                                           jiffies - start);
                }
   } else {
      /*
       * Linux function scsi_probe_and_add_lun seems to add new devices
       * If a device is present, it continues with the next iteration.
       * So do something very similar here
       */
      if (VMK_OK != SCSILinuxAddPaths(vmkAdapter, SCAN_WILD_CARD,  
         SCAN_WILD_CARD,  SCAN_WILD_CARD)) { 
         VMKLNX_WARN("scsi_scan_host failed for %s", sh->hostt->name);
      }

      /*
       * Issue path probes and notify VMkernel of path updates
       * for each scsi_device.
       */
      shost_for_each_device(sdev, sh) {
         vmk_ScsiNotifyPathStateChange(vmkAdapter, sdev->channel, sdev->id,
            sdev->lun);
      }
   }

   startingEntryCount = vmk_AtomicRead64(&sh->pendingScanWorkQueueEntries);
   if (startingEntryCount) {
      VMKLNX_DEBUG(0, "Waiting for target scans to complete on %s...",
                      vmk_ScsiGetAdapterName(vmkAdapter));
      start = jiffies;	/* Start counting only the ADDITIONAL delay */
      while (vmk_AtomicRead64(&sh->pendingScanWorkQueueEntries) &&
             ((jiffies - start) < SCSI_SCAN_HOST_MAX_WAITQ_DELAY)) {
         vmk_WorldSleep(100000); /* sleep for 100ms */
      }
      duration = jiffies - start;
      VMKLNX_DEBUG(0, "completing target scans on %s "
                      "took %lu jiffies (%lu Sec.)",
                      vmk_ScsiGetAdapterName(vmkAdapter),
                      duration, (duration + (HZ/2)) / HZ);
      if (duration >= SCSI_SCAN_HOST_MAX_WAITQ_DELAY) {
         VMKLNX_WARN("Timed out waiting for target scans to complete on %s.",
                     vmk_ScsiGetAdapterName(vmkAdapter));
      }
   } else {
      VMKLNX_DEBUG(3, "No pending target scans at finish on %s.",
                      vmk_ScsiGetAdapterName(vmkAdapter));
   }
}
EXPORT_SYMBOL(scsi_scan_host);

/*
 * vmklnx_scsi_update_lun_path --
 *
 *      Callback function used in scsi_scan_target which updates
 *      the path state for the given scsi_device and lun.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 */
static void
vmklnx_scsi_update_lun_path(struct scsi_device *sdev, void *data)
{
   struct Scsi_Host *shost;
   unsigned int *lun;
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter;
   vmk_ScsiAdapter *vmkAdapter;

   lun = data;
   VMK_ASSERT(lun);

   shost = sdev->host;
   VMK_ASSERT(shost);

   vmklnx26ScsiAdapter = (struct vmklnx_ScsiAdapter *) shost->adapter;
   VMK_ASSERT(vmklnx26ScsiAdapter);

   vmkAdapter = vmklnx26ScsiAdapter->vmkAdapter;
   VMK_ASSERT(vmkAdapter);
   
   if ((*lun == SCAN_WILD_CARD) || (*lun == sdev->lun)) {
      vmk_ScsiNotifyPathStateChange(vmkAdapter, sdev->channel, sdev->id,
         sdev->lun);
   }
}

/*
 *  scsi_scan_target - scan a SCSI target
 *  @parent:  host to scan
 *  @channel: channel to scan
 *  @id:      target ID to scan
 *  @lun:     specific LUN to scan or SCAN_WILD_CARD
 *  @rescan:  if non-zero, update path status
 *
 *  Issue a scan for a SCSI target
 *                         
 *  RETURN VALUE:
 *  NONE
 *
 */                                          
/* _VMKLNX_CODECHECK_: scsi_scan_target */
void 
scsi_scan_target(struct device *parent, unsigned int channel,
                 unsigned int id, unsigned int lun, int rescan)
{
   struct Scsi_Host *shost;
   struct scsi_target *starget;
   unsigned long flags;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(parent);

   shost = dev_to_shost(parent);
   VMK_ASSERT(shost);

   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter = 
		(struct vmklnx_ScsiAdapter *) shost->adapter; 
   vmk_ScsiAdapter *vmkAdapter = vmklnx26ScsiAdapter->vmkAdapter;

   if (shost->this_id == id) {
      /*
       * No scan on the host adapter
       */
      VMKLNX_INFO("Will not scan on host id");
      return;
   }

   /*
    * Allocate a target if needed. (FC adds targets before scan here)
    */
   spin_lock_irqsave(shost->host_lock, flags);
   starget = vmklnx_scsi_find_target(shost, channel, id);
   spin_unlock_irqrestore(shost->host_lock, flags);

   if (starget) {
      goto target_scan;
   }
   starget = scsi_alloc_target(parent, channel, id);
   if (!starget) {
      VMKLNX_WARN("failed due to lack of resources");
      return;
   }

target_scan:
   if (VMK_OK != SCSILinuxAddPaths(vmkAdapter, channel, id, lun)) {
      VMKLNX_WARN("failed for adapter %s"
                  "channel %d target %d and lun %d",
                  vmk_ScsiGetAdapterName(vmkAdapter), channel, id, lun);
   }

   /*
    * If rescan is set, update the paths for the target according to lun
    */
   if (rescan) {
      starget_for_each_device(starget, &lun, vmklnx_scsi_update_lun_path);
   }
}
EXPORT_SYMBOL(scsi_scan_target);

/**
 * Old style passive scsi registration
 * @sht: scsi_host_template
 * @privsize: private size
 *
 * RETURN VALUE:
 * None
 */
/* _VMKLNX_CODECHECK_: scsi_register*/
struct Scsi_Host *
scsi_register(struct scsi_host_template *sht, int privsize)
{
   struct Scsi_Host *sh = scsi_host_alloc(sht, privsize);
   unsigned vmkFlag;

   if (!sht->detect) {
      VMKLNX_DEBUG(0, "called on new-style template for driver %s",
                      sht->name);
   }

   vmkFlag = vmk_SPLockIRQ(&linuxSCSIAdapterLock);
   if (sh) {
      list_add_tail(&sh->sht_legacy_list, &sht->legacy_hosts);
   }
   vmk_SPUnlockIRQ(&linuxSCSIAdapterLock, vmkFlag);

   return sh;
}

/**
 * Old style passive scsi unregistration
 * @sh: scsi_host_template
 *
 * RETURN VALUE:
 * None
 */
void 
scsi_unregister(struct Scsi_Host *sh)
{
   unsigned vmkFlag;

   VMKLNX_DEBUG(0, "called on driver %s", sh->hostt->name);

   vmkFlag = vmk_SPLockIRQ(&linuxSCSIAdapterLock);
   list_del(&sh->sht_legacy_list);
   vmk_SPUnlockIRQ(&linuxSCSIAdapterLock, vmkFlag);
   scsi_host_put(sh);
}

/**
 * Called by drivers to notify the vmkernel
 * of the new queue Depth.
 *
 * @work: work queue payload containing vmkAdapter, channel, id, lun,
 *              queue depth
 *
 * RETURN VALUE:
 * None
 *
 * Include:
 * scsi/scsi_host.h
 */
static void
vmklnx_scsi_modify_queue_depth(struct work_struct *work)
{
   
   struct vmklnx_scsiqdepth_event *event =
      container_of(work, struct vmklnx_scsiqdepth_event, work);
   vmk_ScsiAdapter *vmkAdapter = event->vmkAdapter;

   VMKLNX_DEBUG(2, "adapter=%s, channel=%d, id=%d, lun=%d",
                   vmk_ScsiGetAdapterName(vmkAdapter), 
                   event->channel, event->id, event->lun);
   /*
    * Notify VMKernel that Queue Depth has been changed
    */
   if (vmk_ScsiModifyQueueDepth(vmkAdapter, event->channel, event->id,
                                event->lun, event->tags) == VMK_NOT_FOUND) {
      VMKLNX_DEBUG(2,
                   "vmklnx_scsi_modify_queue_depth, path not "
                   "found - skipping path update for adapter %s, channel=%d, "
                   "id=%d, lun=%d", vmk_ScsiGetAdapterName(vmkAdapter),
                   event->channel, event->id, event->lun);
   }
   // free event allocation
   kfree(event);
}

/**
 *  scsi_adjust_queue_depth - change the queue depth on a specific SCSI device
 *  @sdev: SCSI Device in question
 *  @tagged: use tagged queueing (non-0) or treat the device as an 
 *           untagged device (0)
 *  @tags: number of tags allowed if tagged queueing enabled,
 *         or number of commands the low level driver can
 *         queue up in non-tagged mode (as per cmd_per_lun)
 *
 *  Changes the queue depth on a specific SCSI device
 *
 *  RETURN VALUE:
 *  None
 */
/* _VMKLNX_CODECHECK_: scsi_adjust_queue_depth */
void 
scsi_adjust_queue_depth(struct scsi_device * sdev, int tagged, int tags)
{
   uint32_t notLocked = 0;
   unsigned long flags=0;
   struct vmklnx_scsiqdepth_event *qd_ev;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if (tags < 0) {
      /* Have queue depth of 1 */
      sdev->queue_depth = 1;
      return;
   }

   /*
    * Not required to notify vmkernel storage stack if there's really 
    * no change to sdev's queue depth.
    * This avoids uselessly allocating a vmklnx_scsiqdepth_event 
    * structure as well as scheduling the work.
    */
   if (tags == sdev->queue_depth && tagged == scsi_get_tag_type(sdev)) {
      return;
   } else if (tags == sdev->queue_depth) {
      /*
       * no qdepth change but user wants to change tag, so skip qdepth
       * notfication and just update tag.
       */
      goto skip_qdepth_event;
   }
   
   qd_ev = (struct vmklnx_scsiqdepth_event *)kmalloc(sizeof(*qd_ev), GFP_ATOMIC);
   if (qd_ev) {
      qd_ev->vmkAdapter =
                 ((struct vmklnx_ScsiAdapter *)sdev->host->adapter)->vmkAdapter;
      qd_ev->channel = sdev->channel;
      qd_ev->id = sdev->id;
      qd_ev->lun = sdev->lun;
      qd_ev->tags = tags;
      INIT_WORK(&qd_ev->work, vmklnx_scsi_modify_queue_depth);

      if (!scsi_queue_work(sdev->host, &qd_ev->work)) {
         VMKLNX_WARN("scsi_queue_work() failed - skipping queue depth update "
                     "for adapter %s, channel=%d, id=%d, lun=%d",
                     vmk_NameToString(&qd_ev->vmkAdapter->name),
                     sdev->channel, sdev->id, sdev->lun);
         kfree(qd_ev);
         return;
      }
   } else {
         VMKLNX_WARN("kmalloc() failed - skipping queue depth adjustment "
                     "channel=%d, id=%d, lun=%d",
                     sdev->channel, sdev->id, sdev->lun);
         return;
   }

skip_qdepth_event:
   notLocked = !vmklnx_spin_is_locked_by_my_cpu(sdev->host->host_lock);
   if (notLocked) {
      spin_lock_irqsave(sdev->host->host_lock, flags);
   }

   sdev->queue_depth = tags;
   /*
    * The following just stores the information in case we need to retrive this
    * later
    */
   switch (tagged) {
      case MSG_HEAD_TAG:
      case MSG_ORDERED_TAG:
         sdev->ordered_tags = 1;
	 sdev->simple_tags = 1;
	 break;
      case MSG_SIMPLE_TAG:
 	 sdev->ordered_tags = 0;
	 sdev->simple_tags = 1;
	 break;
      default:
	 sdev_printk(KERN_WARNING, sdev,
		    "scsi_adjust_queue_depth, bad queue type, "
		    "disabled\n");
      case 0:
         sdev->ordered_tags = sdev->simple_tags = 0;
	 break;
   }

   if (notLocked) {
      spin_unlock_irqrestore(sdev->host->host_lock, flags);
   }
   return;
}
EXPORT_SYMBOL(scsi_adjust_queue_depth);

/**
 *  __scsi_add_device - create new scsi device instance
 *  @host: pointer to scsi host instance
 *  @channel: channel number
 *  @id: target id number 
 *  @lun: logical unit number
 *  @hostdata: passed to scsi_alloc_sdev()
 *                                           
 *  Create new scsi device instance
 *
 *  RETURN VALUE:
 *  pointer to new struct scsi_device instance with bumped reference count 
 *  on success, or ERR_PTR(-ENODEV)
 */                            
/* _VMKLNX_CODECHECK_: __scsi_add_device */
/*
 * Todo: 1. vmk_ScsiScanPaths will be revised, see PR 202578 for detail
 *       2. needs to consider when vmk_ScsiScanPaths returns VMK_BUSY  
 */
struct scsi_device *
__scsi_add_device(struct Scsi_Host *host, uint channel, uint id, uint lun,
		  void *hostdata)
{
   VMK_ReturnStatus status;
   uint32_t notLocked = 0;
   unsigned long flags = 0;
   struct scsi_target *starget;
   struct scsi_device *sdev = ERR_PTR(-ENODEV);
   struct vmklnx_ScsiAdapter *vmklnx26_ScsiAdapter =
                              (struct vmklnx_ScsiAdapter *) host->adapter;
   vmk_ScsiAdapter *adapter = vmklnx26_ScsiAdapter->vmkAdapter;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if (host->this_id == id) {
      /*
       * No scan on the host adapter
       */
      VMKLNX_WARN("Will not add device on channel=%d id=%d", channel, id);
      return sdev;
   }
                                                                                
   /*
    * Allocate a target if needed.
    */
   notLocked = !vmklnx_spin_is_locked_by_my_cpu(host->host_lock);
   if (notLocked) {
      spin_lock_irqsave(host->host_lock, flags);
   }

   starget = vmklnx_scsi_find_target(host, channel, id);

   if (notLocked) {
      spin_unlock_irqrestore(host->host_lock, flags);
   }

   if (!starget) {
      starget = scsi_alloc_target(&host->shost_gendev, channel, id);
      if (!starget) {
         VMKLNX_WARN("failed due to lack of resources");
         return sdev;
      }
   }

   status = SCSILinuxAddPaths(adapter, channel, id, lun);
   if (status == VMK_BUSY) {
       VMKLNX_WARN("__scsi_add_device[%p, %d, %d, %d], busy "
                   "returned by vmk_ScsiScanPaths\n",
                   host, channel, id, lun);
   }

   /*
    * The look up increments the reference count. The function 
    * is expected to return with incremented reference count
    */
   sdev = scsi_device_lookup(host, channel, id, lun);
   if (!sdev) {
      return ERR_PTR(-ENODEV);
   } 

   return sdev;
}
EXPORT_SYMBOL(__scsi_add_device);

/**
 *  scsi_add_device - create new scsi device instance
 *  @host: pointer to scsi host instance
 *  @channel: channel number
 *  @id: target id number 
 *  @lun: logical unit number
 *                                           
 *  Create new scsi device instance
 *
 *  RETURN VALUE:
 *  pointer to new struct scsi_device instance on success, or ERR_PTR(-ENODEV)
 */                            
/* _VMKLNX_CODECHECK_: scsi_add_device */
int 
scsi_add_device(struct Scsi_Host *host, uint channel, uint id, uint lun)
{
   struct scsi_device *sdev;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   sdev = __scsi_add_device(host, channel, id, lun, NULL); 

   if (IS_ERR(sdev)) {
      VMKLNX_WARN("Failed to add device on channel=%d, id=%d, lun=%d",
                  channel, id, lun);

      return ((int)PTR_ERR(sdev));
   }

   /*
    * Lookup in __scsi_add_device increments the reference count.
    * Decrement it now
    */
   scsi_device_put(sdev);

   return 0;
}
EXPORT_SYMBOL(scsi_add_device);

/**                                          
 *  scsi_remove_device - Remove a scsi device from the host 
 *  @sdev: Scsi device to be removed   
 *                                           
 *  Remove a scsi device from the host                                           
 *                                           
 *  RETURN VALUE:
 *  NONE
 */                                          
/* _VMKLNX_CODECHECK_: scsi_remove_device */
void 
scsi_remove_device(struct scsi_device *sdev)
{
   struct Scsi_Host *sh = sdev->host;
   unsigned long flags;
   unsigned int channel, id, lun;
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter = 
      (struct vmklnx_ScsiAdapter *) sh->adapter; 
   vmk_ScsiAdapter *vmkAdapter = vmklnx26ScsiAdapter->vmkAdapter;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(vmkAdapter != NULL); 

   channel = sdev->channel;
   id = sdev->id;
   lun = sdev->lun;

   /*
    * If the path is already dead, dont do anything here
    */
   spin_lock_irqsave(sh->host_lock, flags);
   if ((sdev->sdev_state == SDEV_DEL)) {
      spin_unlock_irqrestore(sh->host_lock, flags);
      VMKLNX_DEBUG(2, "called repeatedly on deleting device %s",
                      sh->hostt->name);
      return;
   }

   /*
    * Set this device as delete
    */
   sdev->sdev_state = SDEV_DEL;

   /*
    * Notify vmkernel that this device is in PDL
    */
   if (sdev->vmkflags & HOT_REMOVED) { 
   	spin_unlock_irqrestore(sh->host_lock, flags);
  	if (VMK_OK != vmk_ScsiSetPathLostByDevice(&vmkAdapter->name, channel, id, lun)) {
      		VMKLNX_DEBUG(2, "Failed to set PDL for %s:%d:%d:%d",
                      vmk_ScsiGetAdapterName(vmkAdapter), channel, id, lun);
  	}
   } else
   	spin_unlock_irqrestore(sh->host_lock, flags);

   /*
    * Mark this path as dead and notify vmkernel
    */
   if (VMK_OK != vmk_ScsiNotifyPathStateChangeAsync(vmkAdapter, channel, id, lun)) {
      VMKLNX_DEBUG(2, "Failed to set path dead for %s:%d:%d:%d",
                      vmk_ScsiGetAdapterName(vmkAdapter), channel, id, lun);
   }

   return;
}
EXPORT_SYMBOL(scsi_remove_device);

/**                                          
 *  scsi_host_get - Increase a Scsi_Host reference count       
 *  @sh: pointer to the Scsi_Host structure    
 *                                           
 *  Increase a Scsi_Host reference count
 *
 *  RETURN VALUE:
 *  On Success pointer to the Scsi_Host structure, otherwise NULL
 *
 */                                          
/* _VMKLNX_CODECHECK_: scsi_host_get */
struct Scsi_Host *scsi_host_get(struct Scsi_Host *sh)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if (unlikely(sh->shost_state == SHOST_CANCEL) ||
      unlikely(sh->shost_state == SHOST_DEL) ||
      unlikely(!get_device(&sh->shost_gendev))) {
      return NULL;
   }
   return sh;
}
EXPORT_SYMBOL(scsi_host_get);

/**                                          
 *  scsi_host_put - Decrease a Scsi_Host reference count
 *  @sh: pointer to the Scsi_Host structure    
 *                                           
 *	Decrease a Scsi_Host reference count, host will be freed if ref count
 *	reaches zero.                                           
 *                                           
 *  RETURN VALUE:
 *  None.
 */                                          
/* _VMKLNX_CODECHECK_: scsi_host_put */
void scsi_host_put(struct Scsi_Host *sh)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   put_device(&sh->shost_gendev);
}
EXPORT_SYMBOL(scsi_host_put);

/**
 *  scsi_block_requests - prevent further commands from being queued to device
 *  @sh: host in question
 *
 *  Prevents further commands from being queued to the device.
 *
 *  RETURN VALUE:
 *  None
 */
/* _VMKLNX_CODECHECK_: scsi_block_requests */
void 
scsi_block_requests(struct Scsi_Host * sh)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   sh->host_self_blocked = TRUE;
}
EXPORT_SYMBOL(scsi_block_requests);

/**                                          
 *  scsi_unblock_requests - Unblocks scsi requests
 *  @sh: pointer to scsi_host
 *                                           
 *  Unblocks scsi requests
 *                                           
 *  RETURN VALUE:
 *  NONE 
 *
 */                                          
/* _VMKLNX_CODECHECK_: scsi_unblock_requests */
void 
scsi_unblock_requests(struct Scsi_Host * sh)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   sh->host_self_blocked = FALSE;
}
EXPORT_SYMBOL(scsi_unblock_requests);

/**
 *  scsi_bios_ptable - read partition table out of first sector of a device
 *  @dev: the device
 *
 *  Reads the first sector from the device and returns 0x42 bytes starting at offset 0x1be
 *
 *  RETURN VALUE:
 *  pointer to partition table data or NULL on error 
 */
/* _VMKLNX_CODECHECK_: scsi_bios_ptable */
unsigned char *
scsi_bios_ptable(struct block_device *dev)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   /*
    * We dont call driver's entry point. Hence this should be OK to have this
    * empty
    */
   VMK_ASSERT(FALSE);
   return ((unsigned char *) NULL);
}
EXPORT_SYMBOL(scsi_bios_ptable);

/**                                          
 *  scsi_partsize - non-operational function in release build.
 *  @buf: partition table, see scsi_bios_ptable
 *  @capacity: size of the disk in sector
 *  @cyls: cylinders 
 *  @hds: heads
 *  @secs: sectors
 *                                           
 *  This is a non-operational function in release build, but can cause panic if being called in non-release mode. 
 *                                           
 *  ESX Deviation Notes:                     
 *  This is a non-operational function in release build, but can cause panic if being called in non-release mode. 
 *
 *  RETURN VALUE:
 *  0
 */                                          
/* _VMKLNX_CODECHECK_: scsi_partsize */
int 
scsi_partsize(unsigned char *buf, unsigned long capacity,
                  unsigned int *cyls, unsigned int *hds, unsigned int *secs)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   /*
    * We dont call driver's entry point. Hence this should be OK to have this
    * empty
    */
   VMK_ASSERT(FALSE);
   return 0;
}
EXPORT_SYMBOL(scsi_partsize);


/*
 * vmklnx_scsi_alloc_target_conditionally -- Allocate a target
 *
 * @parent: parent of the target (need not be a scsi host)
 * @channel: target channel number (zero if no channels)
 * @id: target id number
 * @force_alloc_flag: if set, force to allocate scsi target.
 * @old_target: output value. set as 1 if target already exists, 0 else.
 *
 * RETURN VALUE
 * Pointer to an existing scsi target or a new scsi target.
 *
 * Include:
 * scsi/scsi_host.h
 */
struct scsi_target *
vmklnx_scsi_alloc_target_conditionally(struct device *parent, int channel, uint id,
                                       int force_alloc_flag, int *old_target)
{
   struct Scsi_Host *sh;
   unsigned long flags;
   struct scsi_target *found_target;
   struct vmklnx_ScsiModule *vmklnx26ScsiModule;
 
   VMK_ASSERT(parent);

   VMK_ASSERT(old_target);
   *old_target = 0;
   if (force_alloc_flag == 1) {
      goto alloc_and_return;
   }

   sh = dev_to_shost(parent);
   VMK_ASSERT(sh);
   VMK_ASSERT(sh->transportt);

   spin_lock_irqsave(sh->host_lock, flags);
   found_target = vmklnx_scsi_find_target(sh, channel, id);

   if (found_target) {
      if (found_target->state == STARGET_DEL) {
         spin_unlock_irqrestore(sh->host_lock, flags);
         return NULL;
      } else {
         get_device(&found_target->dev);
         spin_unlock_irqrestore(sh->host_lock, flags);
         *old_target = 1;
         return found_target;
      }
   }
   spin_unlock_irqrestore(sh->host_lock, flags);

   vmklnx26ScsiModule = (struct vmklnx_ScsiModule *)sh->transportt->module;

   /*
    * If this is a FC transport, dont allocate a target
    * rport and scsi_targets are very related and created at the same time
    * If this is a SAS transport, no SAS target is associated with channel/id
    * and don't allocate target. 
    * TODO - In future disable automatic discovery by PSA
    */
   if ((VMKLNX_SCSI_TRANSPORT_TYPE_FC == sh->xportFlags) ||
       (VMKLNX_SCSI_TRANSPORT_TYPE_FCOE == sh->xportFlags) ||
       (VMKLNX_SCSI_TRANSPORT_TYPE_SAS == sh->xportFlags) ||
       (VMKLNX_SCSI_TRANSPORT_TYPE_ISCSI == sh->xportFlags)) {

      if (VMKLNX_SCSI_TRANSPORT_TYPE_FC == sh->xportFlags) {
         VMKLNX_DEBUG(5, "FC target has no rport associated");
      } else if (VMKLNX_SCSI_TRANSPORT_TYPE_FCOE == sh->xportFlags) {
         VMKLNX_DEBUG(5, "FCOE target has no rport associated");
      } else if (VMKLNX_SCSI_TRANSPORT_TYPE_SAS == sh->xportFlags) {
         VMKLNX_DEBUG(5, "no SAS target is associated");
      } else if (VMKLNX_SCSI_TRANSPORT_TYPE_ISCSI == sh->xportFlags) {
         VMKLNX_DEBUG(5, "no ISCSI target is associated");
      }

      return NULL;
   }

alloc_and_return:
   /*
    * These are dependent on PSA discovering the targets
    */
   return (scsi_alloc_target(parent, channel, id));
}

/**
 * Allocate a new or find an existing target
 * @parent: parent of the target (need not be a scsi host)
 * @channel: target channel number (zero if no channels)
 * @id: target id number
 *
 * RETURN VALUE:
 * Pointer to scsi target
 */
struct scsi_target *
vmklnx_scsi_alloc_target(struct device *parent, int channel, uint id)
{
   int old_target = 0;
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   return vmklnx_scsi_alloc_target_conditionally(parent, channel, id, 1, &old_target);
}
EXPORT_SYMBOL(vmklnx_scsi_alloc_target);
EXPORT_SYMBOL_ALIASED(vmklnx_scsi_alloc_target, scsi_alloc_target);

/**
 *  Allocate a new or find an existing target
 *  @parent: parent device
 *  @channel: target id
 *  @id: target id
 *
 *  Allocate a new or find an existing target
 *
 *  RETURN VALUE:
 *  pointer to scsi target.
 */
/* _VMKLNX_CODECHECK_: scsi_alloc_target */
struct scsi_target *
scsi_alloc_target(struct device *parent, int channel, uint id)
{
   int size;
   struct Scsi_Host *sh;
   struct device *dev = NULL;
   unsigned long flags;
   struct scsi_target *stgt;
   struct vmklnx_ScsiModule *vmklnx26ScsiModule; 
   int error = -EINVAL;
 
   VMK_ASSERT(parent);

   sh = dev_to_shost(parent);
   VMK_ASSERT(sh);
   VMK_ASSERT(sh->transportt);

   vmklnx26ScsiModule = (struct vmklnx_ScsiModule *)sh->transportt->module;
   
   spin_lock_irqsave(sh->host_lock, flags);
   if (sh->shost_state == SHOST_DEL) {
      spin_unlock_irqrestore(sh->host_lock, flags);
      VMKLNX_WARN("Trying to allocate a SCSI target on dying host");
      return NULL;
   } 
   spin_unlock_irqrestore(sh->host_lock, flags);

   size = sizeof(struct scsi_target) + sh->transportt->target_size;
   stgt = kzalloc(size, GFP_KERNEL);
   if (!stgt) {
      VMKLNX_WARN("Failed to allocate a SCSI target");
      return NULL;
   }

   stgt->id = id;
   stgt->channel = channel;
   INIT_LIST_HEAD(&stgt->siblings);
   INIT_LIST_HEAD(&stgt->devices);
   stgt->state = STARGET_RUNNING;

   dev = &stgt->dev;
   device_initialize(dev);
   dev->dev_type = SCSI_TARGET_TYPE; 
   stgt->reap_ref = 1;

   /*
    * Take ref count on the parent
   */
   dev->parent = get_device(parent);
   dev->release = scsi_target_dev_release;
   sprintf(dev->bus_id, "target%d:%d:%d", sh->host_no, channel, id);

   stgt->id = id;
   stgt->channel = channel;
   INIT_LIST_HEAD(&stgt->siblings);
   INIT_LIST_HEAD(&stgt->devices);
   stgt->state = STARGET_RUNNING;
   if (vmklnx26ScsiModule) {
      if (VMKLNX_SCSI_TRANSPORT_TYPE_PSCSI == vmklnx26ScsiModule->transportType) {
         spi_setup_transport_attrs(stgt);
      } else 
      if (VMKLNX_SCSI_TRANSPORT_TYPE_XSAN == vmklnx26ScsiModule->transportType) {
         xsan_setup_transport_attrs(sh, stgt);
      }
   }

   error = device_add(dev);
   if (error) {
      VMKLNX_WARN("Failed to add the node for stgt - error %d", error);
      put_device(parent);
      kfree(stgt);
      return NULL;
   }

   spin_lock_irqsave(sh->host_lock, flags);
   list_add_tail(&stgt->siblings, &sh->__targets);
   stgt->state = STARGET_RUNNING;
   spin_unlock_irqrestore(sh->host_lock, flags);

   if (sh->hostt->target_alloc) {
      VMKAPI_MODULE_CALL(SCSI_GET_MODULE_ID(sh), error, sh->hostt->target_alloc, stgt);

      if (error) {
         VMKLNX_WARN("target allocation failed, error %d", error);
         device_del(dev);
         put_device(dev);
         /*
          * scsi_target_dev_release will release the resources
          */
	 return NULL;
      }
   }

   return stgt;
}

/**
 * Find a matching target
 * @sh: scsi host
 * @channel: target id
 * @id: target id
 *
 * RETURN VALUE:
 * None
 *
 * Include:
 * scsi/scsi_host.h
 */
struct scsi_target *
vmklnx_scsi_find_target(struct Scsi_Host *sh,
					      int channel, uint id)
{
   struct scsi_target *starget, *found_starget = NULL;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   /*
    * Search for an existing target for this sdev.
    */
   list_for_each_entry(starget, &sh->__targets, siblings) {
      if (starget->id == id &&
	    starget->channel == channel) {
		found_starget = starget;
		break;
	}
   }

   return found_starget;
}
EXPORT_SYMBOL(vmklnx_scsi_find_target);


/*
 * Find a matching device 
 *  for given target
 *
 * RETURN VALUE:
 * None
 *
 * Include:
 * scsi/scsi_host.h
 *
 * comments - Usually called from interrupt context
 */
struct scsi_device *
__scsi_device_lookup_by_target(struct scsi_target *starget, uint lun)
{
   struct scsi_device *sdev;

   list_for_each_entry(sdev, &starget->devices, same_target_siblings) {
      if (sdev->lun == lun ) {
         return sdev;
      }
   }
   return NULL;
}

/**
 *  find a device given the target
 *  @starget: SCSI target pointer
 *  @lun: Logical Unit Number
 *
 *  Looks up the scsi_device with the specified lun for a given target.
 *  The returned scsi_device has an additional reference that needs to be 
 *  released with scsi_device_put once you're done with it.
 *
 *  RETURN VALUE:
 *  pointer to the scsi_device if found, NULL otherwise
 */
/* _VMKLNX_CODECHECK_: scsi_device_lookup_by_target */
struct scsi_device *
scsi_device_lookup_by_target(struct scsi_target *starget, uint lun)
{
   struct scsi_device *sdev;
   struct Scsi_Host *sh;
   unsigned long flags;
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(starget);
   sh = dev_to_shost(starget->dev.parent);
   spin_lock_irqsave(sh->host_lock, flags);
   sdev = __scsi_device_lookup_by_target(starget, lun);
   if (sdev && scsi_device_get(sdev)) {
      sdev = NULL;
   }
   spin_unlock_irqrestore(sh->host_lock, flags);

   return sdev;
}
EXPORT_SYMBOL(scsi_device_lookup_by_target);


/**
 * Allocate and set up a scsi device
 * @starget: which target to allocate a scsi_device for
 * @lun: which lun
 * @hostdata: usually NULL and set by ->slave_alloc instead
 *
 * RETURN VALUE:
 * return On failure to alloc sdev, return NULL
 *         On other failures, return ERR_PTR(-errno)
 *         On Success, return pointer to sdev (which fails IF_ERR)
 *
 * Include:
 * scsi/scsi_device.h
 *
 * comments - Gets the device ready for IO
 */
struct scsi_device *
scsi_alloc_sdev(struct scsi_target *starget, unsigned int lun, void *hostdata)
{
   struct scsi_device *sdev = ERR_PTR(-ENODEV);
   int ret = 0;
   struct Scsi_Host *sh = dev_to_shost(starget->dev.parent);
   unsigned long flags;
   struct device *dev = NULL;
   int error = -EINVAL;

   VMK_ASSERT(sh);
   VMK_ASSERT(sh->transportt);

   if (!starget) {
      VMKLNX_WARN("Invalid Target");
      return ERR_PTR(error);
   }

   sdev = kzalloc(sizeof(*sdev) + sh->transportt->device_size, GFP_KERNEL);
   if (!sdev) {
      return NULL;
   }

   dev = &sdev->sdev_gendev;
   device_initialize(dev);
   sdev->sdev_gendev.dev_type = SCSI_DEVICE_TYPE;

   /*
    * Get a ref count on parent
    */
   sdev->sdev_gendev.parent = get_device(&starget->dev);
   sdev->sdev_gendev.release = scsi_device_dev_release;
   sprintf(dev->bus_id, "sdev%d:%d:%d:%d", sh->host_no, 
	starget->channel, starget->id, lun);

   error = device_add(dev);
   if (error) {
      ret = -ENOMEM;
      VMKLNX_WARN("device add failed");
      goto out_device_destroy;
   }

   sdev->vendor = scsi_null_device_strs;
   sdev->model = scsi_null_device_strs;
   sdev->rev = scsi_null_device_strs;
   sdev->host = sh;
   sdev->id = starget->id;
   sdev->lun = lun;
   sdev->channel = starget->channel;
   sdev->sdev_state = SDEV_CREATED;
   sdev->sdev_target = starget;
   INIT_LIST_HEAD(&sdev->siblings);
   INIT_LIST_HEAD(&sdev->same_target_siblings);
   INIT_LIST_HEAD(&sdev->cmd_list);
   INIT_LIST_HEAD(&sdev->starved_entry);
   spin_lock_init(&sdev->list_lock);

   /* usually NULL and set by ->slave_alloc instead */
   sdev->hostdata = hostdata;

   /* if the device needs this changing, it may do so in the
    * slave_configure function 
    */
   sdev->max_device_blocked = SCSI_DEFAULT_DEVICE_BLOCKED;

   /*
    * Some low level driver could use device->type
    */
   sdev->type = -1;

   /*
    * Assume that the device will have handshaking problems,
    * and then fix this field later if it turns out it
    * doesn't
    */
   sdev->borken = 1;

   scsi_adjust_queue_depth(sdev, 0, sdev->host->cmd_per_lun);

   if (sh->hostt->slave_alloc) {
        VMKAPI_MODULE_CALL(SCSI_GET_MODULE_ID(sh), ret, sh->hostt->slave_alloc, sdev);

      if (ret) {
         device_del(dev);
         put_device(dev);
         /*
          * The resources are released using scsi_device_dev_release
          */
         goto out_device_del;
      }
   }

   /*
    * The next few lines where the sdev is added to the scsi_host and
    * scsi_target's linked lists are moved after the slave_alloc call to handle
    * a race in cases when slave_alloc fails (see PR 277647).
    * In the linux case, it happens before the slave_alloc
    */
   spin_lock_irqsave(sh->host_lock, flags);
   list_add_tail(&sdev->same_target_siblings, &starget->devices);
   list_add_tail(&sdev->siblings, &sh->__devices);
   spin_unlock_irqrestore(sh->host_lock, flags);

   return sdev;

out_device_destroy:
   put_device(&starget->dev);
   kfree(sdev);
out_device_del:
   if (ret == -ENOMEM) {
      VMKLNX_WARN("Driver failed to allocate memory");
      sdev = NULL;
   } else {
     /* -ENXIO or other errors */
      sdev = ERR_PTR(-ENODEV);
   }
   return sdev;
}

/**
 * scsi_destroy_sdev -- Destroy a scsi device
 * @sdev: sdevice
 *
 * RETURN VALUE:
 * Pointer to sdev
 *
 * Include:
 * scsi/scsi_device.h
 *
 * comments - Removes from all the lists as well
 */
void 
scsi_destroy_sdev(struct scsi_device *sdev)
{
   unsigned long flags;

   spin_lock_irqsave(sdev->host->host_lock, flags);
   sdev->sdev_state = SDEV_DEL;
   spin_unlock_irqrestore(sdev->host->host_lock, flags);

   device_del(&sdev->sdev_gendev);
   /*
    * Get rid of the reference count now
    */
   put_device(&sdev->sdev_gendev);

}

/*
 * __scsi_device_lookup -- Look up for a scsi device given BTL
 * @sh: SCSI host pointer
 * @channel: SCSI channel (zero if only one channel)
 * @id: SCSI target number (physical unit number)
 * @lun: SCSI Logical Unit Number
 *
 * RETURN VALUE:
 * Pointer to scsi device
 *
 * Include:
 * scsi/scsi_device.h
 *
 * comments - Called from IRQ context or with lock held
 */
struct scsi_device *
__scsi_device_lookup(struct Scsi_Host *sh, uint channel, uint id, uint lun)
{
   struct scsi_device *sdev;

   list_for_each_entry(sdev, &sh->__devices, siblings) {
      if (sdev->channel == channel && sdev->id == id &&
          sdev->lun == lun )
         return sdev;
   }
   return NULL;
}

/**
 *  scsi_device_lookup - find a device given the host
 *  @sh: SCSI host pointer
 *  @channel: SCSI channel (zero if only one channel)
 *  @id: SCSI target number (physical unit number)
 *  @lun: SCSI Logical Unit Number
 *
 *  Looks up the scsi_device with the specified channel, id, lun for a
 *  given host.  The returned scsi_device has an additional reference that
 *  needs to be released with scsi_device_put once you're done with it.
 *
 *  RETURN VALUE:
 *  pointer to the scsi_device if found, NULL otherwise
 */
/* _VMKLNX_CODECHECK_: scsi_device_lookup */
struct scsi_device *
scsi_device_lookup(struct Scsi_Host *sh, uint channel, uint id, uint lun)
{
   struct scsi_device *sdev;
   unsigned long flags;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   spin_lock_irqsave(sh->host_lock, flags);
   sdev = __scsi_device_lookup(sh, channel, id, lun);
   if (sdev && scsi_device_get(sdev)) {
      sdev = NULL;
   }
   spin_unlock_irqrestore(sh->host_lock, flags);
   return sdev;
}
EXPORT_SYMBOL(scsi_device_lookup);


/**
 * __scsi_get_command -- Return a Scsi_Cmnd
 *
 * @sh: scsi host
 * @gfp_mask: allocator flags
 *
 * RETURN VALUE:
 * Pointer to sdev
 *
 * Include:
 * scsi/scsi_device.h
 */
struct scsi_cmnd *
__scsi_get_command(struct Scsi_Host *sh, gfp_t gfp_mask)
{
   struct scsi_cmnd *cmd = NULL;

   cmd = vmk_SlabAlloc(sh->cmd_pool->slab);

   if (unlikely(!cmd)) {
      unsigned long flags;

      spin_lock_irqsave(&sh->free_list_lock, flags);
      if (likely(!list_empty(&sh->free_list))) {
         cmd = list_entry(sh->free_list.next, struct scsi_cmnd, list);
         list_del_init(&cmd->list);
      }
      spin_unlock_irqrestore(&sh->free_list_lock, flags);
   }
   return cmd;
}

/*
 * Function:    vmklnx_clear_scmd()
 *
 * Purpose:     Clear the required fields
 *
 * Arguments:   scsi_cmnd
 *
 * Returns:     None
 */
static void
vmklnx_clear_scmd(struct scsi_cmnd *scmd)
{
   scmd->eh_eflags = 0;
   scmd->retries = 0;
   scmd->allowed = 0;
   scmd->timeout_per_command = 0;

   scmd->request_bufflen = 0;
   scmd->eh_timeout.function = NULL;

   scmd->sglist_len = 0;
   scmd->transfersize = 0;
   scmd->resid = 0;
   scmd->request = NULL;

   /*
    * Memset 24 bytes to cover Sense Key specific fields. This is just to avoid
    * any drivers setting specific fields assuming the buffer is zero'ed out
    */
   memset(scmd->sense_buffer, 0, 24);

   memset(&(scmd->SCp), 0, sizeof(struct scsi_pointer));
   scmd->host_scribble = NULL;
   scmd->result = 0;
   scmd->pid = 0;

   scmd->vmkflags = 0;
   VMKLNX_INIT_VMK_SG(scmd->vmksg, NULL);

   return;
}

/*
 * Function:    vmklnx_init_scmd()
 *
 * Purpose:     Initialize required fields
 *
 * Arguments:   scsi_cmnd
 *
 * Returns:     None
 */
static inline void
vmklnx_init_scmd(struct scsi_cmnd *scmd, struct scsi_device *dev)
{
   unsigned long flags;

   scmd->device = dev;
   INIT_LIST_HEAD(&scmd->list);
   INIT_LIST_HEAD(&scmd->bhlist);
   INIT_LIST_HEAD(&scmd->eh_entry);
   init_timer(&scmd->eh_timeout);
   spin_lock_init(&scmd->vmklock);
   spin_lock_irqsave(&dev->list_lock, flags);
   list_add_tail(&scmd->list, &dev->cmd_list);
   spin_unlock_irqrestore(&dev->list_lock, flags);
}

/*
 * Function:	scsi_get_command()
 *
 * Purpose:	Allocate and setup a scsi command block
 *
 * Arguments:	dev	- parent scsi device
 *		gfp_mask- allocator flags
 *
 * Returns:	The allocated scsi command structure.
 */
struct scsi_cmnd *
scsi_get_command(struct scsi_device *dev, gfp_t gfp_mask)
{
   struct scsi_cmnd *cmd;
   struct Scsi_Host *shost = scsi_host_get(dev->host);

   if(unlikely(shost == NULL)) {
     return NULL;
   }

   cmd = __scsi_get_command(shost, gfp_mask);

   if (likely(cmd != NULL)) {
      /*
       * Memset only the required fields
       */
      vmklnx_clear_scmd(cmd);

      vmklnx_init_scmd(cmd, dev);
   } else {
      scsi_host_put(shost);
   }
 
   return cmd;
}

/*
 * Function:    vmklnx_scsi_get_command_urgent()
 *
 * Purpose:     Allocate a scsi command block from emergency heap and setup it
 *
 * Arguments:   dev     - parent scsi device
 *
 * Returns:     The allocated scsi command structure.
 */

struct scsi_cmnd *
vmklnx_scsi_get_command_urgent(struct scsi_device *dev)
{
   struct scsi_cmnd *cmd;
   struct Scsi_Host *shost = scsi_host_get(dev->host);

   if(unlikely(shost == NULL)) {
     return NULL;
   }

   cmd = vmklnx_kmalloc_align(vmklnxEmergencyHeap,
                              sizeof(struct scsi_cmnd),
                              VMK_L1_CACHELINE_SIZE,
                              GFP_ATOMIC);

   if (likely(cmd != NULL)) {
      /*
       * Memset only the required fields
       */
      vmklnx_clear_scmd(cmd);

      vmklnx_init_scmd(cmd, dev);

      cmd->vmkflags |= VMK_FLAGS_FROM_EMERGENCY_HEAP;
   } else {
      scsi_host_put(shost);
   }

   return cmd;
}

/**
 * scsilun_to_int: convert a scsi_lun to an int
 * @scsilun:	struct scsi_lun to be converted.
 *
 * Description:
 *     Convert @scsilun from a struct scsi_lun to a four byte host byte-ordered
 *     integer, and return the result. The caller must check for
 *     truncation before using this function.
 *
 * Notes:
 *     The struct scsi_lun is assumed to be four levels, with each level
 *     effectively containing a SCSI byte-ordered (big endian) short; the
 *     addressing bits of each level are ignored (the highest two bits).
 *     For a description of the LUN format, post SCSI-3 see the SCSI
 *     Architecture Model, for SCSI-3 see the SCSI Controller Commands.
 *
 *     Given a struct scsi_lun of: 0a 04 0b 03 00 00 00 00, this function returns
 *     the integer: 0x0b030a04
 **/
int 
scsilun_to_int(struct scsi_lun *scsilun)
{
	int i;
	unsigned int lun;

	lun = 0;
	for (i = 0; i < sizeof(lun); i += 2)
		lun = lun | (((scsilun->scsi_lun[i] << 8) |
			      scsilun->scsi_lun[i + 1]) << (i * 8));
	return lun;
}

/**                                          
 *  int_to_scsilun - Convert a packed integer representation to a scsi_lun     
 *  @lun: integer to convert
 *  @scsilun: lun corresponding to the integer coding of the lun   
 *
 *  Converts a packed integer value creaded by scsilun_to_int back
 *  into a scsi_lun.
 *
 *  scsilun_to_int packs an 8-byte lun value into a 4 byte integer.                      
 *
 *  The scsilun_to_int() routine does not truly handle all 8 bytes of
 *  the lun value. This functions restores only as much as was packed by
 *  scsilun_to_int.
 *
 *  EXAMPLE:
 *     Given an integer 0x0b030a04, this function returns a
 *     scsi_lun of 0a 04 0b 03 00 00 00 00         
 *                                           
 *  RETURN VALUE:
 *     None.                                 
 */                                          
/* _VMKLNX_CODECHECK_: int_to_scsilun */
void 
int_to_scsilun(unsigned int lun, struct scsi_lun *scsilun)
{
	int i;

	VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
	memset(scsilun->scsi_lun, 0, sizeof(scsilun->scsi_lun));

	for (i = 0; i < sizeof(lun); i += 2) {
		scsilun->scsi_lun[i] = (lun >> 8) & 0xFF;
		scsilun->scsi_lun[i+1] = lun & 0xFF;
		lun = lun >> 16;
	}
}
EXPORT_SYMBOL(int_to_scsilun);

/*
 * Function:	scsi_put_command()
 *
 * Purpose:	Free a scsi command block
 *
 * Arguments:	cmd	- command block to free
 *
 * Returns:	Nothing.
 *
 * Notes:	The command must not belong to any lists. Only abort path
 * 		uses these commands
 */
void 
scsi_put_command(struct scsi_cmnd *scmd)
{
	struct scsi_device *sdev = scmd->device;
	struct Scsi_Host *sh = sdev->host;
	unsigned long flags;

	/* serious error if the command hasn't come from a device list */
	spin_lock_irqsave(&scmd->device->list_lock, flags);
	BUG_ON(list_empty(&scmd->list));
	list_del_init(&scmd->list);
	spin_unlock_irqrestore(&scmd->device->list_lock, flags);

        /*
   	 * Free up the resources allocated now
 	 */
	if (likely(!(scmd->vmkflags & VMK_FLAGS_FROM_EMERGENCY_HEAP))) {
		spin_lock_irqsave(&sh->free_list_lock, flags);
		if (unlikely(list_empty(&sh->free_list))) {
			list_add(&scmd->list, &sh->free_list);
			scmd = NULL;
		}
		spin_unlock_irqrestore(&sh->free_list_lock, flags);

		if (likely(scmd != NULL)) {
			vmk_SlabFree(sh->cmd_pool->slab, scmd);
		}
	} else {
		vmklnx_kfree(vmklnxEmergencyHeap, scmd);
		scmd = NULL;
	}

        scsi_host_put(sh);
}

/**
 *	scsi_execute - queue a scsi request and wait for the result
 *	@sdev:	scsi device
 *	@cmd:	scsi command
 *	@data_direction: data direction
 *	@buffer:	data buffer
 *	@bufflen:	len of buffer
 *	@sense:	optional sense buffer
 *	@timeout:	request timeout in seconds
 *	@retries:	number of times to retry request
 *	@req_flags:	or into request flags;
 *
 *	Queue the scsi request and wait for the result
 *
 *	RETRUN VALUE:
 * 	0 if succeeded, DRIVER_ERROR << 24 if failed.
 **/
/* _VMKLNX_CODECHECK_: scsi_execute */
int 
scsi_execute(struct scsi_device *sdev, const unsigned char *cmd,
		 int data_direction, void *buffer, unsigned bufflen,
		 unsigned char *sense, int timeout, int retries, int req_flags)
{
   struct vmklnx_ScsiIntCmd *vmklnx26_ScsiIntCmd;
   int ret = DRIVER_ERROR << 24;
   struct scsi_cmnd *scmd;
   struct Scsi_Host *shost = sdev->host;
   int status;
   unsigned long flags;
   void* doneFn = NULL;
   vmk_ModuleID mid;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   /*
    * The completion function is called through a pointer.
    * So we have to set up a trampoline at runtime.
    */

   mid = SCSI_GET_MODULE_ID(shost); 
   doneFn = SCSILinuxInternalCommandDone;

   /*
    * Allocate resources for internal command and initialize them
    */
   vmklnx26_ScsiIntCmd = kzalloc(sizeof(*vmklnx26_ScsiIntCmd), GFP_KERNEL);

   if(!vmklnx26_ScsiIntCmd) {
      VMKLNX_WARN("Failed to Allocate SCSI internal Command");
      return ret;
   }

   /*
    * Fill in values required for command structure
    * Fill in vmkCmdPtr with vmklnx26_ScsiIntCmd pointer
    * Fill done with our routine
    */
   SCSILinuxInitInternalCommand(sdev, vmklnx26_ScsiIntCmd);

   scmd = &vmklnx26_ScsiIntCmd->scmd;

   /*
    * Fill in IO specific details
    */
   scmd->request_buffer  = (void *) buffer;
   scmd->request_bufferMA = virt_to_phys(buffer);
   scmd->request_bufflen = bufflen;
   scmd->cmd_len = COMMAND_SIZE(cmd[0]);
   memcpy(scmd->cmnd, cmd, scmd->cmd_len);
   scmd->underflow = 8;
   scmd->use_sg = 0;
   scmd->vmkCmdPtr = (void *)vmklnx26_ScsiIntCmd; /*Store Internal Command Ptr*/
   scmd->sc_data_direction = data_direction;

   VMK_ASSERT(scmd->transfersize == 0);
   VMK_ASSERT(scmd->sglist_len == 0);

   spin_lock_irqsave(&scmd->vmklock, flags);
   scmd->vmkflags |= VMK_FLAGS_NEED_CMDDONE;
   spin_unlock_irqrestore(&scmd->vmklock, flags);

   /* 
    * Acquire lock now
    */
   spin_lock_irqsave(shost->host_lock, flags);
   ++shost->host_busy;
   ++sdev->device_busy;

   /*
    * Add timer for error handling
    */
#define VMK_PSCSI_CMD_TIMEOUT_JIFFIES USEC_TO_JIFFIES(60 * 1000 * 1000)
   scsi_add_timer(scmd, VMK_PSCSI_CMD_TIMEOUT_JIFFIES, SCSILinuxCmdTimedOut);

   /*
    * Send down the command now
    */

   VMKAPI_MODULE_CALL(mid, status, shost->hostt->queuecommand, scmd, doneFn);

   spin_unlock_irqrestore(shost->host_lock, flags);

   if (status) {
      spin_lock_irqsave(&scmd->vmklock, flags);
      if (scmd->vmkflags & VMK_FLAGS_NEED_CMDDONE) {
         /*
          * Driver could not take this command
          */
         spin_unlock_irqrestore(&scmd->vmklock, flags);
         scsi_delete_timer(scmd);
         VMKLNX_WARN("Failed to send down Internal Command to the driver");
         goto free_internal_command;
      }
      spin_unlock_irqrestore(&scmd->vmklock, flags);
   }

   vmk_WorldAssertIsSafeToBlock();

   /*
    * Wait for command completion and wake up
    */
   wait_for_completion(&vmklnx26_ScsiIntCmd->icmd_compl);

   spin_lock_irqsave(&scmd->vmklock, flags);
   if (scmd->vmkflags & VMK_FLAGS_NEED_CMDDONE) {
      VMKLNX_ALERT("Woken up even though the command was not completed");
   }
   spin_unlock_irqrestore(&scmd->vmklock, flags);

   /*
    * Interpret the result. 
    */
   if (scmd->result == DID_OK) {
      ret = 0;
   }

free_internal_command:
   spin_lock_irqsave(shost->host_lock, flags);
   --shost->host_busy;
   --sdev->device_busy;
   spin_unlock_irqrestore(shost->host_lock, flags);
   /*
    * Free the command structure
    */
   kfree(vmklnx26_ScsiIntCmd);

   /*
    * Return the result
    */
   return ret;
}
EXPORT_SYMBOL(scsi_execute);

/**                                          
 *  scsi_normalize_sense - fill up scsi_sense_hdr struct from the buffer of sense data
 *  @sense_buffer: buffer that contains the sense data 
 *  @sb_len: length of the sense buffer   
 *  @sshdr: scsi_sense_hdr struct to be filled in 
 *                                           
 *  Fill up scsi_sense_hdr struct from the buffer of sense data
 *  
 *  RETRUN VALUE:
 *  1 if succeeded, otherwise 0                                           
 */                                          
/* _VMKLNX_CODECHECK_: scsi_normalize_sense */
int scsi_normalize_sense(const u8 *sense_buffer, int sb_len,
                         struct scsi_sense_hdr *sshdr)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if (!sense_buffer || !sb_len) {
      return 0;
   }
   memset(sshdr, 0, sizeof(struct scsi_sense_hdr));
   sshdr->response_code = (sense_buffer[0] & 0x7f);

   if (!scsi_sense_valid(sshdr)) {
      return 0;
   }

   if (sshdr->response_code >= 0x72) {
      /*
       * descriptor format
       */
      if (sb_len > 1) {
	 sshdr->sense_key = (sense_buffer[1] & 0xf);
      }
      if (sb_len > 2) {
	 sshdr->asc = sense_buffer[2];
      }
      if (sb_len > 3) {
	 sshdr->ascq = sense_buffer[3];
      }
      if (sb_len > 7) {
	 sshdr->additional_length = sense_buffer[7];
      }
   } else {
	/* 
	 * fixed format
	 */
      if (sb_len > 2) {
         sshdr->sense_key = (sense_buffer[2] & 0xf);
      }
      if (sb_len > 7) {
         sb_len = (sb_len < (sense_buffer[7] + 8)) ?
			 sb_len : (sense_buffer[7] + 8);
 	 if (sb_len > 12) {
	    sshdr->asc = sense_buffer[12];
         }
	 if (sb_len > 13) {
	    sshdr->ascq = sense_buffer[13];
         }
      }
   }
   return 1;
}
EXPORT_SYMBOL(scsi_normalize_sense);

/**
 *  scsi_device_get - increment reference to a scsi device
 *  @sdev: the device
 *
 *  Increments reference to scsi_device.
 *
 *  RETURN VALUE:
 *  0 on success, negative errno on error
 */
/* _VMKLNX_CODECHECK_: scsi_device_get */
int scsi_device_get(struct scsi_device *sdev)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if (sdev->sdev_state == SDEV_DEL) {
      return -ENXIO;
   }

   if (!get_device(&sdev->sdev_gendev)) {
      return -ENXIO;
   }
   return 0;
}
EXPORT_SYMBOL(scsi_device_get);

/**                                          
 *  scsi_device_put - decrement reference count of scsi device      
 *  @sdev: scsi device in question 
 *                                           
 *	Decrement reference count of scsi device, device will be freed if ref count 
 *	reaches zero.
 *
 *	RETURN VALUE:
 *	None
 */                                          
/* _VMKLNX_CODECHECK_: scsi_device_put */
void scsi_device_put(struct scsi_device *sdev)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   put_device(&sdev->sdev_gendev);
   return;
}
EXPORT_SYMBOL(scsi_device_put);

/**                                          
 *  __scsi_iterate_devices - return the next scsi device from host's device list 
 *  @shost: data struct associated with this scsi host    
 *  @prev: pointer to the previously returned scsi device   
 *                                           
 *  This is a helper for shost_for_each_device to return the next scsi device
 *  from host's device list.
 *                                           
 *  RETURN VALUE:
 *	Next scsi device on host's device list, or NULL if the list is exhausted.
 *
 */                                          
/* _VMKLNX_CODECHECK_: __scsi_iterate_devices */
struct scsi_device *__scsi_iterate_devices(struct Scsi_Host *shost,
					   struct scsi_device *prev)
{
   struct list_head *list = (prev ? &prev->siblings : &shost->__devices);
   struct scsi_device *next = NULL;
   unsigned long flags = 0; 
   uint32_t notLocked = 0; 

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   notLocked = !vmklnx_spin_is_locked_by_my_cpu(shost->host_lock);   
   if (notLocked) { 
      spin_lock_irqsave(shost->host_lock, flags); 
   } 

   while (list->next != &shost->__devices) {
      next = list_entry(list->next, struct scsi_device, siblings);
      /* skip devices that we can't get a reference to */
      if (!scsi_device_get(next))
         break;
      next = NULL;
      list = list->next;
   }

   if (notLocked) { 
      spin_unlock_irqrestore(shost->host_lock, flags); 
   } 

   if (prev)
      scsi_device_put(prev);
   return next;
}
EXPORT_SYMBOL(__scsi_iterate_devices);

/*
 * Following are dummy functions
 */
/**                                          
 *  scsi_is_host_device - Check if a scsi device is scsi host.       
 *  @dev: pointer to the scsi device to be checked    
 *                                           
 *  Check if a scsi device is scsi host                                           
 *                                           
 *  RETURN VALUE:
 *  True if dev_type is SCSI_HOST_TYPE, otherwise false
 *
 */                                          
/* _VMKLNX_CODECHECK_: scsi_is_host_device */
int scsi_is_host_device(const struct device *dev)
{
   if (dev) {
      return(dev->dev_type == SCSI_HOST_TYPE);
  } else {
      return(0);
  }
}
EXPORT_SYMBOL(scsi_is_host_device);

/**
 * scsi_host_lookup - get a reference to a Scsi_Host by host no
 *
 * @hostnum:	host number to locate
 *
 * Return value:
 *	A pointer to located Scsi_Host or NULL. Ref count is incremented
 **/
/* _VMKLNX_CODECHECK_: scsi_host_lookup */
struct Scsi_Host *scsi_host_lookup(unsigned short hostnum)
{
   struct Scsi_Host *shost = ERR_PTR(-ENXIO);
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter;
   unsigned vmkFlag;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   vmkFlag = vmk_SPLockIRQ(&linuxSCSIAdapterLock);
   list_for_each_entry(vmklnx26ScsiAdapter, &linuxSCSIAdapterList, entry) {
      if (vmklnx26ScsiAdapter->shost->host_no == hostnum) {
         shost = scsi_host_get(vmklnx26ScsiAdapter->shost);
         break;
      }
   }
   vmk_SPUnlockIRQ(&linuxSCSIAdapterLock, vmkFlag);

   return shost;
}
EXPORT_SYMBOL(scsi_host_lookup);

/**
 * scsi_forget_host -- Notify all LUNs that a host is going down
 *
 * @sh: host that is being removed
 *
 * RETURN VALUE:
 * None
 *
 * Include:
 * scsi/scsi_host.h
 */
static void scsi_forget_host(struct Scsi_Host *sh)
{
   unsigned long flags;
   struct scsi_target *starget;

restart:
   spin_lock_irqsave(sh->host_lock, flags);
   /*
    * This is slightly different from Linux where scsi_forget_host
    * will free individual devices
    */
   list_for_each_entry(starget, &sh->__targets, siblings) {
      if (starget->state == STARGET_DEL) {
         continue;
      }
      spin_unlock_irqrestore(sh->host_lock, flags);
      scsi_remove_target(&starget->dev);
      goto restart;
   }
   spin_unlock_irqrestore(sh->host_lock, flags);
}

/**                                          
 *  scsi_queue_work - Queue work to the Scsi_Host workqueue       
 *  @shost: host for which requests are queued    
 *  @work: work to be queued 
 *                                           
 *  Queue work to the Scsi_Host workqueue                                          
 * 
 *  RETURN VALUE:
 *  1 if succeeded, <= 0 otherwise                                           
 */                                          
/* _VMKLNX_CODECHECK_: scsi_queue_work */
int 
scsi_queue_work(struct Scsi_Host *shost, struct work_struct *work)
{
   VMK_ASSERT(shost);
   VMK_ASSERT(work);

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if (unlikely(!shost->work_q)) {
      VMKLNX_WARN("ERROR: Scsi host '%s' attempted to queue scsi-work, "
                  "when no workqueue created.", shost->hostt->name);
      return -EINVAL;
   }
   return queue_work(shost->work_q, work);
}
EXPORT_SYMBOL(scsi_queue_work);

/**                                          
 *  scsi_flush_work - Flush a Scsi_Host's workqueue       
 *  @shost: scsi host for which requests are queued 
 *                                           
 *  Flush a Scsi_Host's workqueue
 *
 *  RETURN VALUE:
 *  None.
 */                                          
/* _VMKLNX_CODECHECK_: scsi_flush_work */
void 
scsi_flush_work(struct Scsi_Host *shost)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(shost);

   if (!shost->work_q) {
      VMKLNX_WARN("ERROR: Scsi host '%s' attempted to flush scsi-work, "
                  "when no workqueue created.", shost->hostt->name);
      return;
  }

  flush_workqueue(shost->work_q);
}
EXPORT_SYMBOL(scsi_flush_work);


/**                                          
 *  scsi_execute_req - execute a scsi command       
 *  @sdev:  scsi device
 *  @cmd:   scsi command
 *	@data_direction: data direction
 *	@buffer:    data buffer
 *	@bufflen:   len of buffer
 *	@sshdr:		scsi sense header
 *	@timeout:   request timeout in seconds
 *	@retries:   number of times to retry request
 *
 * 	Execute a scsi command and fill up scsi sense header.
 *                                           
 *  RETRUN VALUE:
 *  0 if succeeded, DRIVER_ERROR << 24 if failed.
 */                                          
/* _VMKLNX_CODECHECK_: scsi_execute_req */
int scsi_execute_req(struct scsi_device *sdev, const unsigned char *cmd,
                     int data_direction, void *buffer, unsigned bufflen,
                     struct scsi_sense_hdr *sshdr, int timeout, int retries)
{
   unsigned char *sense = NULL;
   int result;
                                                                                
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if (sshdr) {
      sense = (unsigned char *)vmklnx_kzmalloc(vmklnxLowHeap,
                                               SCSI_SENSE_BUFFERSIZE,
					       GFP_KERNEL);
      if (!sense) {
         return DRIVER_ERROR << 24;
      }
   }
   result = scsi_execute(sdev, cmd, data_direction, buffer, bufflen,
                         sense, timeout, retries, 0);
   if (sshdr) {
      scsi_normalize_sense(sense, SCSI_SENSE_BUFFERSIZE, sshdr);
   }
                                                                                
   vmklnx_kfree(vmklnxLowHeap, sense);
   return result;
}
EXPORT_SYMBOL(scsi_execute_req);


/**
 *      scsi_mode_sense - issue a mode sense, falling back from 10 to
 *              six bytes if necessary.
 *      @sdev:  SCSI device to be queried
 *      @dbd:   set if mode sense will allow block descriptors to be returned
 *      @modepage: mode page being requested
 *      @buffer: request buffer (may not be smaller than eight bytes)
 *      @len:   length of request buffer.
 *      @timeout: command timeout
 *      @retries: number of retries before failing
 *      @data: returns a structure abstracting the mode header data
 *      @sshdr: place to put sense data (or NULL if no sense to be collected).
 *              must be SCSI_SENSE_BUFFERSIZE big.
 *
 *      Returns zero if unsuccessful, or the header offset (either 4
 *      or 8 depending on whether a six or ten byte command was
 *      issued) if successful.
 **/
int
scsi_mode_sense(struct scsi_device *sdev, int dbd, int modepage,
                unsigned char *buffer, int len, int timeout, int retries,
                struct scsi_mode_data *data, struct scsi_sense_hdr *sshdr)
{
   unsigned char cmd[12];
   int use_10_for_ms;
   int header_length;
   int result;
   struct scsi_sense_hdr my_sshdr;
                                                                                
   memset(data, 0, sizeof(*data));
   memset(&cmd[0], 0, 12);
   cmd[1] = dbd & 0x18;    /* allows DBD and LLBA bits */
   cmd[2] = modepage;
                                                                                
  /* caller might not be interested in sense, but we need it */
   if (!sshdr) {
      sshdr = &my_sshdr;
   }
                                                                                
retry:
   use_10_for_ms = sdev->use_10_for_ms;
   if (use_10_for_ms) {
      if (len < 8)
         len = 8;
                                                                              
      cmd[0] = MODE_SENSE_10;
      cmd[8] = len;
      header_length = 8;
   } else {
      if (len < 4)
         len = 4;
                                                                                
      cmd[0] = MODE_SENSE;
      cmd[4] = len;
      header_length = 4;
   }
                                                                                
   memset(buffer, 0, len);
                                                                                
   result = scsi_execute_req(sdev, cmd, DMA_FROM_DEVICE, buffer, len,
                             sshdr, timeout, retries);
                                                                                
  /* This code looks awful: what it's doing is making sure an
   * ILLEGAL REQUEST sense return identifies the actual command
   * byte as the problem.  MODE_SENSE commands can return
   * ILLEGAL REQUEST if the code page isn't supported.
   */
                                                                                
   if (use_10_for_ms && !scsi_status_is_good(result) &&
      (driver_byte(result) & DRIVER_SENSE)) {
         if (scsi_sense_valid(sshdr)) {
            if ((sshdr->sense_key == ILLEGAL_REQUEST) &&
                (sshdr->asc == 0x20) && (sshdr->ascq == 0)) {
               /*
                * Invalid command operation code
                */
                sdev->use_10_for_ms = 0;
                goto retry;
            }
         }
   }
                                                                                
   if(scsi_status_is_good(result)) {
       if (unlikely(buffer[0] == 0x86 && buffer[1] == 0x0b &&
           (modepage == 6 || modepage == 8))) {
                                                                                
          /* Initio breakage? */
           header_length = 0;
           data->length = 13;
           data->medium_type = 0;
           data->device_specific = 0;
           data->longlba = 0;
           data->block_descriptor_length = 0;
       } else if(use_10_for_ms) {
           data->length = buffer[0]*256 + buffer[1] + 2;
           data->medium_type = buffer[2];
           data->device_specific = buffer[3];
           data->longlba = buffer[4] & 0x01;
           data->block_descriptor_length = buffer[6]*256 + buffer[7];
       } else {
           data->length = buffer[0] + 1;
           data->medium_type = buffer[1];
           data->device_specific = buffer[2];
           data->block_descriptor_length = buffer[3];
       }
       data->header_length = header_length;
   }
                                                                                
   return result;
}

/**                                          
 *  scsi_is_target_device - Check if this is target type device
 *  @dev: device struct associated with this target    
 *                                           
 *  Check if this is target type device                                           
 *                                           
 *  RETURN VALUE:
 *  TRUE if it's dev_type is SCSI_TARGET_TYPE, otherwise false.
 */                                          
/* _VMKLNX_CODECHECK_: scsi_is_target_device */
int 
scsi_is_target_device(const struct device *dev)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if (dev) {
      return (dev->dev_type == SCSI_TARGET_TYPE);
  } else {
      return (0);
  }
}
EXPORT_SYMBOL(scsi_is_target_device);

/**                                          
 *  scsi_report_bus_reset - Notify SCSI bus reset 
 *  @shost: host on which bus reset was observed    
 *  @channel: channel number   
 *                                           
 *  This is a dummy function that logs only warning message 
 *  that there was SCSI bus reset                       
 *                                           
 *  ESX Deviation Notes:                     
 *  Only logs warning message
 *                                           
 *  RETURN VALUE:
 *  NONE
 *
 */                                          
/* _VMKLNX_CODECHECK_: scsi_report_bus_reset */
void 
scsi_report_bus_reset(struct Scsi_Host *shost, int channel)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   /*
    * Dummy function - We will just print out alert message
    * Need to check if we need to issue rescan on this path later
    */
   VMKLNX_WARN("SCSI Bus reset on %s, Host No %d, Channel No %d", 
               shost->hostt->name, shost->host_no, channel);
}
EXPORT_SYMBOL(scsi_report_bus_reset);

/**                                          
 *  scsi_report_device_reset - Notify SCSI device reset 
 *  @shost: host on which device reset was observed 
 *  @channel: channel number 
 *  @target: target number 
 *                                           
 *  Notifies SCSI device reset that was observed.
 *                                           
 *  ESX Deviation Notes:                     
 *  Device reset is just logged, no special handling is done 
 *                                           
 *  RETURN VALUE:
 *  NONE 
 *
 */                                          
/* _VMKLNX_CODECHECK_: scsi_report_device_reset */
void 
scsi_report_device_reset(struct Scsi_Host *shost, int channel, int target)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   /*
    * Dummy function - We will just print out alert message
    * Need to check if we need to issue rescan on this path later
    */
   VMKLNX_WARN("SCSI Bus reset on %s, Host No %d, Channel %d,"
	       " Target %d", shost->hostt->name, shost->host_no, channel, target);
}
EXPORT_SYMBOL(scsi_report_device_reset);

static void
device_unblock(struct scsi_device *sdev, void *data)
{
   struct Scsi_Host *sh = sdev->host;
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter =
      (struct vmklnx_ScsiAdapter *) sh->adapter;
   vmk_ScsiAdapter *vmkAdapter = vmklnx26ScsiAdapter->vmkAdapter;
   uint32_t ownLock = 0;
   unsigned long flags = 0;
   vmk_Bool notifyVmkernel;

   VMK_ASSERT(vmkAdapter != NULL);

   ownLock = !vmklnx_spin_is_locked_by_my_cpu(sdev->host->host_lock);

   if (ownLock) {
      spin_lock_irqsave(sdev->host->host_lock, flags);
   }

   notifyVmkernel = (sdev->sdev_state == SDEV_OFFLINE) ? VMK_TRUE : VMK_FALSE;

   sdev->sdev_state = SDEV_RUNNING;

   if (ownLock) {
      spin_unlock_irqrestore(sdev->host->host_lock, flags);
   }

   /*
    * If we were offline, notify vmkernel
    */
   if (notifyVmkernel == VMK_TRUE) {

      if (VMK_OK != vmk_ScsiNotifyPathStateChangeAsync(vmkAdapter,
          sdev->channel, sdev->id, sdev->lun) ) {
         VMKLNX_DEBUG(2, "Failed to set path On for %s:%d:%d:%d",
                         vmk_ScsiGetAdapterName(vmkAdapter), sdev->channel,
                         sdev->id, sdev->lun);
      }
   }

   return;
}

static int
target_unblock(struct device *dev, void *data)
{
   if (scsi_is_target_device(dev)) {
      starget_for_each_device(to_scsi_target(dev), NULL, device_unblock);
   }
   return 0;
}

/**                                          
 *  scsi_target_unblock - Unblocks the target to accept more commands
 *  @dev: device associated with the target
 *                                           
 *  Unblocks target to accept more commands
 *                                           
 *  RETURN VALUE:
 *  NONE 
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: scsi_target_unblock */
void
scsi_target_unblock(struct device *dev)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(dev);

   if (scsi_is_target_device(dev)) {
      starget_for_each_device(to_scsi_target(dev), NULL, device_unblock);
   } else {
      device_for_each_child(dev, NULL, target_unblock);
   }
}
EXPORT_SYMBOL(scsi_target_unblock);

/**                                          
 *  scsi_track_queue_full - Tracks queue depth for the device
 *  @sdev: track queue depth for the device
 *  @depth: current number of outstanding SCSI commands on this device
 *                                           
 *  This function will track successive QUEUE_FULL events on a specific
 *  SCSI device to determine if and when there is a need to adjust the 
 *  queue depth on the device.
 *
 *  RETURN VALUE:
 *      0 - No change needed
 *      >0 - Adjust queue depth to this new depth
 *      -1 - Drop back to untagged operation using cmd_per_lun value
 *                                           
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: scsi_track_queue_full */
int 
scsi_track_queue_full(struct scsi_device *sdev, int depth)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if ((jiffies >> 4) == sdev->last_queue_full_time) {
      return 0;
   }

   sdev->last_queue_full_time = (jiffies >> 4);
   if (sdev->last_queue_full_depth != depth) {
      sdev->last_queue_full_count = 1;
      sdev->last_queue_full_depth = depth;
   } else {
      sdev->last_queue_full_count++;
   }

   if (sdev->last_queue_full_count <= 10) {
      return 0;
   }
   if (sdev->last_queue_full_depth < 8) {
      /* Drop back to untagged */
      scsi_adjust_queue_depth(sdev, 0, sdev->host->cmd_per_lun);
      return -1;
   }
	
   if (sdev->ordered_tags) {
      scsi_adjust_queue_depth(sdev, MSG_ORDERED_TAG, depth);
   } else {
      scsi_adjust_queue_depth(sdev, MSG_SIMPLE_TAG, depth);
   }
   return depth;
}
EXPORT_SYMBOL(scsi_track_queue_full);

static void
device_block(struct scsi_device *sdev, void *data)
{
   uint32_t ownLock = 0;
   unsigned long flags=0;

   ownLock = !vmklnx_spin_is_locked_by_my_cpu(sdev->host->host_lock);
   if (ownLock) {
      spin_lock_irqsave(sdev->host->host_lock, flags);
   }

   sdev->sdev_state = SDEV_BLOCK;

   if (ownLock) {
      spin_unlock_irqrestore(sdev->host->host_lock, flags);
   }
}

static int
target_block(struct device *dev, void *data)
{
   if (scsi_is_target_device(dev)) {
      starget_for_each_device(to_scsi_target(dev), NULL, device_block);
   }
   return 0;
}

/**                                          
 *  scsi_target_block - Blocks the target to accept more commands
 *  @dev: device associated with the target
 *                                           
 *  Blocks the target to accept more commands
 *                                           
 *  RETURN VALUE:
 *  NONE 
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: scsi_target_block */
void
scsi_target_block(struct device *dev)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(dev);

   if (scsi_is_target_device(dev)) {
      starget_for_each_device(to_scsi_target(dev), NULL, device_block);
   } else {
       device_for_each_child(dev, NULL, target_block);
   }
}
EXPORT_SYMBOL(scsi_target_block);

static int
remove_target(struct device *dev, void *data)
{
   if (scsi_is_target_device(dev)) {
      __scsi_remove_target(to_scsi_target(dev));
   }
   return 0;
}

/**
 *  scsi_remove_target - Remove a SCSI target and devices
 *  @dev: a pointer to struct device
 *
 *  Removes the SCSI target associated with @dev.
 *
 *  RETURN VALUE:
 *  This function does not return a value.
 *
 */
/* _VMKLNX_CODECHECK_: scsi_remove_target */
void 
scsi_remove_target(struct device *dev)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(dev);

   if (scsi_is_target_device(dev)) {
      __scsi_remove_target(to_scsi_target(dev));
      return;
   } else {
      device_for_each_child(dev, NULL, remove_target);
   }
}
EXPORT_SYMBOL(scsi_remove_target);

static void 
__scsi_remove_target(struct scsi_target *starget)
{
   struct Scsi_Host *shost = dev_to_shost(starget->dev.parent);
   unsigned long flags;
   struct scsi_device *sdev;
   struct scsi_device *sdev_prev = NULL;

   spin_lock_irqsave(shost->host_lock, flags);
   if (starget->state == STARGET_DEL) {
      spin_unlock_irqrestore(shost->host_lock, flags);
      VMKLNX_DEBUG(2, "called repeatedly on deleting target %s",
                      shost->hostt->name);
      return;
   }
   starget->state = STARGET_DEL;

   /*
    * In the loop below, the current device node that sdev points to can 
    * be deleted by another thread before an iteration can be completed.
    * This is due to the fact that shost->host_lock needs to be released
    * when sdev points to a matching device.  If sdev is deleted, 
    * then when advancing sdev down the same_target_siblings list in
    * list_for_each_entry, we will get a PSOD. To prevent this race
    * condition, we will bump the refcount of the device node sdev
    * points to when a match is found.  We will lower the refcount after 
    * we have advanced to the next node.  In this way, we can avoid the 
    * race of having the current node being deleted before we have
    * finished accessing it.
    */
   list_for_each_entry(sdev, &starget->devices, same_target_siblings) {

      int s;

      if (sdev->channel != starget->channel || sdev->id != starget->id  || 
	  sdev->sdev_state == SDEV_DEL) {
         continue;
      }

      s = scsi_device_get(sdev);
      VMK_ASSERT(s == 0);

      /*
       * Here, by releasing shost->host_lock, we are opening up the race 
       * condition in which another thread may want to delete sdev. But
       * we should be fine since we have bumped up the refcount above.
       */
      spin_unlock_irqrestore(shost->host_lock, flags);
      if (sdev_prev != NULL) {
         scsi_device_put(sdev_prev);
      }
      scsi_remove_device(sdev);
      spin_lock_irqsave(shost->host_lock, flags);

      sdev_prev = sdev;
   }
   spin_unlock_irqrestore(shost->host_lock, flags);

   if (sdev_prev != NULL) {
      scsi_device_put(sdev_prev);
   }

   device_del(&starget->dev);

   put_device(&starget->dev);
}

/**                                          
 *  scsi_rescan_device - issue a rescan on the given device
 *  @dev: device associated with the target to scan
 *                                           
 *  Issues a rescan on the given device
 *                                           
 *  RETURN VALUE:
 *  NONE 
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: scsi_rescan_device */
void 
scsi_rescan_device(struct device *dev)
{
   struct Scsi_Host *shost;
   struct scsi_device *sdev;
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter;
   vmk_ScsiAdapter *vmkAdapter;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   /*
    * We will do a rescan for the host, seems different from
    * what Linux seems to be having or not??
    */
   VMK_ASSERT(dev);

   sdev = to_scsi_device(dev);
   shost = sdev->host;

   VMK_ASSERT(shost);

   vmklnx26ScsiAdapter = 
		(struct vmklnx_ScsiAdapter *) shost->adapter; 
   vmkAdapter = vmklnx26ScsiAdapter->vmkAdapter;

   if (VMK_OK != 
	SCSILinuxAddPaths(vmkAdapter, sdev->channel, sdev->id, sdev->lun)) {
      VMKLNX_WARN("failed for adapter %s channel %d target %d and lun %d",
                  vmk_ScsiGetAdapterName(vmkAdapter), sdev->channel,
                  sdev->id, sdev->lun);
   }

   /*
    * Notify VMkernel of path updates
    */
   vmk_ScsiNotifyPathStateChange(vmkAdapter, sdev->channel, sdev->id,
      sdev->lun);

   return;
}
EXPORT_SYMBOL(scsi_rescan_device);

/**
 * scsi_device_set_state - Take the given device through the device
 *           state model.
 * @sdev: scsi device to change the state of.
 * @state: state to change to.
 *
 * RETURN VALUE:
 * return zero if unsuccessful or an error if the requested transition 
 * is illegal.
 **/
/**                                          
 *  scsi_device_set_state - Set scsi state to the given scsi device
 *  @sdev: scsi device to change the state of.    
 *  @state: state to change to.   
 *                                           
 *	Set scsi state to the given scsi device
 *
 *	RETURN VALUE:
 *	Zero
 */                                          
/* _VMKLNX_CODECHECK_: scsi_device_set_state */
int
scsi_device_set_state(struct scsi_device *sdev, enum scsi_device_state state)
{
	VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
	/* For simplicity, we just set the state to what is given without
 	 * any validity checking
 	 */ 
        sdev->sdev_state = state;
        return 0;
}
EXPORT_SYMBOL(scsi_device_set_state);

/**
 * vmklnx_get_vmhba_name - Provide the vmhba name
 * @sh: Scsi_Host for the adapter  
 *
 * RETURN VALUE:
 * vmhba Name
 **/
char * 
vmklnx_get_vmhba_name(struct Scsi_Host *sh)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(sh);

   if (sh->adapter != NULL) {
      struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter = 
		(struct vmklnx_ScsiAdapter *) sh->adapter; 
      vmk_ScsiAdapter *vmkAdapter =
      		vmklnx26ScsiAdapter->vmkAdapter;

      return ((char *)vmk_ScsiGetAdapterName(vmkAdapter));
   } else {
      return NULL;
   }
}
EXPORT_SYMBOL(vmklnx_get_vmhba_name);

/*
 * vmklnx_scsi_free_host_resources --
 *
 *      Frees the common host resources
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 */
static void 
vmklnx_scsi_free_host_resources(struct Scsi_Host *sh)
{
   struct device *parent = sh->shost_gendev.parent;

   scsi_destroy_command_freelist(sh);
   mutex_destroy(&sh->scan_mutex);

   scsi_proc_hostdir_rm(sh->hostt);

   if (parent) {
      put_device(parent);
   }

   VMK_ASSERT(sh->vmkAdapter);
   vmk_ScsiFreeAdapter(sh->vmkAdapter);
   vmklnx_kfree(vmklnxLowHeap, sh);
}


/*
 * scsi_host_dev_release --
 *
 *      This function is called when the ref count on the adapter goes to zero
 * The reference count will be held by either scsi_target(usb, pscsi etc),
 * vport(npiv) or by various transports (fc, sas, etc...)
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 */
static void scsi_host_dev_release(struct device *dev)
{
   struct Scsi_Host *sh = dev_to_shost(dev);

   VMK_ASSERT(sh);

   if (sh->adapter != NULL) {
      struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter = 
                (struct vmklnx_ScsiAdapter *) sh->adapter; 

      /*
       * Free up resources for FC and SAS Mgmt Adapters
       */
      switch(sh->xportFlags) {
      case VMKLNX_SCSI_TRANSPORT_TYPE_FC:
         /* Free up the resources associated with the adapter */
         vmklnx_fc_host_free(sh);
         break;
      case VMKLNX_SCSI_TRANSPORT_TYPE_SAS:
         SasLinuxReleaseMgmtAdapter(sh);
         break;
      case VMKLNX_SCSI_TRANSPORT_TYPE_XSAN:
         XsanLinuxReleaseMgmtAdapter(sh);
         break;
      case VMKLNX_SCSI_TRANSPORT_TYPE_FCOE:
         vmklnx_fcoe_host_free(sh);
         break;
      default:
         VMKLNX_DEBUG(5, "Clearing the SCSI mgmt Adapter Registration");
      }

      INIT_WORK(&vmklnx26ScsiAdapter->adapter_destroy_work, 
	vmklnx_scsi_unregister_host);

      /*
       * In case of hot remove of adapters, the current usage
       * is to use "esxcfg-rescan -d" kind of operation. Unfortunately
       * scan code takes 2 references on the adapter and the unregistration
       * would not succeed. An alternative approach would have been to
       * to implement reference count in vmkernel, but with the current
       * API structure, it is not feasible. So we are creating a WQ to
       * to unregister with the vmkernel. The vmkAPI's probably need to change
       * in future
       */
      queue_work(linuxSCSIWQ, &vmklnx26ScsiAdapter->adapter_destroy_work);

   } else {
      vmklnx_scsi_free_host_resources(sh);
   }
   VMKLNX_DEBUG(2, "Exit");
}

void
vmklnx_destroy_adapter_tls(struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter)
{
   if (vmklnx26ScsiAdapter->tls != NULL) {
      int i;

      for (i = 0; i < vmklnx26ScsiAdapter->numTls; i++) {
         SCSILinuxTLSWorldletDestroy(vmklnx26ScsiAdapter->tls[i]);
      }
      vmklnx_kfree(vmklnxLowHeap, vmklnx26ScsiAdapter->tls);
      vmklnx26ScsiAdapter->tls = NULL;
   }
}

/*
 * vmklnx_scsi_unregister_host --
 *
 *      This function is called from a WQ. This fn unregisters ourselves
 * from vmkernel
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 */
static void 
vmklnx_scsi_unregister_host(struct work_struct *work)
{
   struct Scsi_Host *sh = container_of(work, struct vmklnx_ScsiAdapter, adapter_destroy_work)->shost;

   if (sh->adapter != NULL) {
      unsigned vmkFlag;
      VMK_ReturnStatus status;
      struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter = 
                (struct vmklnx_ScsiAdapter *) sh->adapter; 
      vmk_ScsiAdapter *vmkAdapter = vmklnx26ScsiAdapter->vmkAdapter;

      VMKLNX_DEBUG(2, "Freeing resources for  %s",
                   vmk_ScsiGetAdapterName(vmkAdapter));

      status = vmk_ScsiUnregisterAdapter(vmkAdapter);
      if (unlikely(status != VMK_OK)) {
         /*
          * scsi_remove_host() removes all the paths and then releases last
          * reference on the host which results in -
          * scsi_host_dev_release() -> queues vmklnx_scsi_unregister_host()
          * Hence all paths would have been cleaned up by the time this WQ
          * callback get's called and so unregister shouldn't fail!
          */
         VMKLNX_WARN("Failed to unregister adapter %s, status: %s",
                     vmk_ScsiGetAdapterName(vmkAdapter), vmk_StatusToString(status));
         VMK_ASSERT(0);
      }
      mutex_destroy(&vmklnx26ScsiAdapter->ioctl_mutex);

      vmkFlag = vmk_SPLockIRQ(&linuxSCSIAdapterLock);
      list_del(&vmklnx26ScsiAdapter->entry);
      vmk_SPUnlockIRQ(&linuxSCSIAdapterLock, vmkFlag);

      if (sh->ehandler) {
         kthread_stop(sh->ehandler);
      }

      if (sh->work_q) {
         destroy_workqueue(sh->work_q);
      }
      if (sh->shost_data) {
         kfree(sh->shost_data);
      }

      scsi_proc_host_rm(sh);

      vmklnx_destroy_adapter_tls(vmklnx26ScsiAdapter);

      kfree(vmklnx26ScsiAdapter);
   }

   vmklnx_scsi_free_host_resources(sh);

}


/*
 * scsi_target_dev_release --
 *
 *      This function is called when the ref count on the target goes to zero
 * This typically happens after all scsi_devices are freed
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 */
static void scsi_target_dev_release(struct device *dev)
{
   struct device *parent = dev->parent;
   struct scsi_target *starget = to_scsi_target(dev);
   struct Scsi_Host *shost = dev_to_shost(starget->dev.parent);
   unsigned long flags;

   /*
    * Clean up target specific stuff
    */
   if (shost->hostt->target_destroy) {
      VMKAPI_MODULE_CALL_VOID(SCSI_GET_MODULE_ID(shost), 
		shost->hostt->target_destroy, starget);
   }

   spin_lock_irqsave(shost->host_lock, flags);
   list_del(&starget->siblings);
   spin_unlock_irqrestore(shost->host_lock, flags);

   kfree(starget);

   /*
    * Give up reference on parent
    */
   put_device(parent);
}

/*
 * scsi_device_dev_release --
 *
 *      This function is called when the ref count on the device goes to zero
 * This typically means all outstanding commands on a given device are zero'ed
 * out
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 */
static void scsi_device_dev_release(struct device *dev)
{
   struct scsi_device *sdev = to_scsi_device(dev);
   struct Scsi_Host *shost = sdev->host;
   struct device *parent = dev->parent;
   unsigned long flags;

   if (sdev->host->hostt->slave_destroy) {
      VMKAPI_MODULE_CALL_VOID(SCSI_GET_MODULE_ID(sdev->host), 
	sdev->host->hostt->slave_destroy, sdev);
   }

   spin_lock_irqsave(shost->host_lock, flags);
   list_del(&sdev->same_target_siblings);
   list_del(&sdev->siblings);
   spin_unlock_irqrestore(shost->host_lock, flags);

   kfree(sdev);

   /*
    * Give up reference on parent
    */
   put_device(parent);
}

void scsi_device_unbusy(struct scsi_device *sdev)
{
   struct Scsi_Host *shost = sdev->host;
   unsigned long flags;

   spin_lock_irqsave(shost->host_lock, flags);
   if (unlikely(scsi_host_in_recovery(shost) &&
       (shost->host_failed || shost->host_eh_scheduled)))
      scsi_eh_wakeup(shost);
   spin_unlock_irqrestore(shost->host_lock, flags);
}

/*
 * Function:    scsi_finish_command
 *
 * Purpose:     Pass command off to upper layer for finishing of I/O
 *              request, waking processes that are waiting on results,
 *              etc.
 */
void scsi_finish_command(struct scsi_cmnd *cmd)
{
   struct scsi_device *sdev = cmd->device;
   struct Scsi_Host *shost = sdev->host;

   scsi_device_unbusy(sdev);

   /*
    * Clear the flags which say that the device/host is no longer
    * capable of accepting new commands.  These are set in scsi_queue.c
    * for both the queue full condition on a device, and for a
    * host full condition on the host.
    *
    * XXX(hch): What about locking?
    */
   shost->host_blocked = 0;
   sdev->device_blocked = 0;

   /*
    * If we have valid sense information, then some kind of recovery
    * must have taken place.  Make a note of this.
    */
   if (SCSI_SENSE_VALID(cmd))
      cmd->result |= (DRIVER_SENSE << 24);

   SCSI_LOG_MLCOMPLETE(4, sdev_printk(KERN_INFO, sdev,
                         "Notifying upper driver of completion "
                         "(result %x)\n", cmd->result));

   cmd->done(cmd);
}

/**
 * scsi_req_abort_cmd -- Request command recovery for the specified command
 * @cmd: pointer to the SCSI command of interest
 *
 * This function requests that SCSI Core start recovery for the
 * command by deleting the timer and adding the command to the eh
 * queue.  It can be called by either LLDDs or SCSI Core.  LLDDs who
 * implement their own error recovery MAY ignore the timeout event if
 * they generated scsi_req_abort_cmd.
 **/
 /* _VMKLNX_CODECHECK_: scsi_req_abort_cmd*/
void scsi_req_abort_cmd(struct scsi_cmnd *cmd)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if (!scsi_delete_timer(cmd))
      return;
   scsi_times_out(cmd);
}
EXPORT_SYMBOL(scsi_req_abort_cmd);

/* Private entry to scsi_done() to complete a command when the timer
 * isn't running --- used by scsi_times_out */
void __scsi_done(struct scsi_cmnd *cmd)
{
   cmd->done(cmd);
}

/**
 * scsi_host_set_state - Take the given host through the host
 *    state model.
 * @shost: scsi host to change the state of.
 * @state: state to change to.
 *
 * Returns zero if successful or an error if the requested
 * transition is illegal.
 **/
 /* _VMKLNX_CODECHECK_: csi_host_set_state*/
int scsi_host_set_state(struct Scsi_Host *shost, enum scsi_host_state state)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   shost->shost_state = state;
   return 0;
}
EXPORT_SYMBOL(scsi_host_set_state);

static void
device_offline(struct scsi_device *sdev, void *data)
{
   VMK_ASSERT(sdev);

   scsi_offline_device(sdev);
}

static int
target_offline(struct device *dev, void *data)
{
   if (scsi_is_target_device(dev)) {
      starget_for_each_device(to_scsi_target(dev), NULL, device_offline);
   }
   return 0;
}

/**
 *  vmklnx_scsi_target_offline - Mark the target offline
 *  @dev: pointer to the target's device struct
 *
 *  Mark all the scsi devices of the target offline. If the @dev doesn't represent
 *  target device then it also loops through child devices, if any.
 *
 *  ESX Deviation Notes:
 *  ESX specific API
 *
 *  RETURN VALUE:
 *  None.
 *
 */
 /* _VMKLNX_CODECHECK_: vmklnx_scsi_target_offline */
void
vmklnx_scsi_target_offline(struct device *dev)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(dev);

   if (scsi_is_target_device(dev)) {
      starget_for_each_device(to_scsi_target(dev), NULL, device_offline);
   } else {
       device_for_each_child(dev, NULL, target_offline);
   }
}
EXPORT_SYMBOL(vmklnx_scsi_target_offline);
EXPORT_SYMBOL_ALIASED(vmklnx_scsi_target_offline, scsi_target_offline);

/*
 * scsi_offline_device -- Mark the device offline
 * @sdev: scsi device
 *
 * RETURN VALUE:
 * None
 */
static void 
scsi_offline_device(struct scsi_device *sdev)
{
   struct Scsi_Host *sh = sdev->host;
   unsigned long flags;
   unsigned int channel, id, lun;
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter = 
      (struct vmklnx_ScsiAdapter *) sh->adapter; 
   vmk_ScsiAdapter *vmkAdapter = vmklnx26ScsiAdapter->vmkAdapter;
   VMK_ReturnStatus status;

   VMK_ASSERT(vmkAdapter != NULL); 

   channel = sdev->channel;
   id = sdev->id;
   lun = sdev->lun;

   /*
    * If the path is already dead or offlined, dont do anything here
    */
   spin_lock_irqsave(sh->host_lock, flags);
   if ((sdev->sdev_state == SDEV_DEL) || (sdev->sdev_state == SDEV_OFFLINE)) {
      spin_unlock_irqrestore(sh->host_lock, flags);
      VMKLNX_DEBUG(2, "called repeatedly on deleting device %s", sh->hostt->name);
      return;
   }

   /*
    * Set this device as offline
    */
   sdev->sdev_state = SDEV_OFFLINE;

   /*
    * Notify vmkernel that this device is in PDL
    */
   if (sdev->vmkflags & HOT_REMOVED) { 
   	spin_unlock_irqrestore(sh->host_lock, flags);
  	if (VMK_OK != vmk_ScsiSetPathLostByDevice(&vmkAdapter->name, channel, id, lun)) {
      		VMKLNX_DEBUG(2, "Failed to set PDL for %s:%d:%d:%d",
                      vmk_ScsiGetAdapterName(vmkAdapter), channel, id, lun);
  	}
   } else 
   	spin_unlock_irqrestore(sh->host_lock, flags);

   /* Mark this path as dead and notify vmkernel asynchronously */
   status = vmk_ScsiNotifyPathStateChangeAsync(vmkAdapter, channel, id, lun);

   if (VMK_OK != status) {
      VMKLNX_DEBUG(2, "Failed to set path dead for %s:%d:%d:%d",
                      vmk_ScsiGetAdapterName(vmkAdapter), channel, id, lun);
   }

   return;
}

/**
 * get num of queues to create
 * @maxQueue: Maximum queues adapter can create
 *
 * RETURN VALUE:
 * return Number of queues to be created, returns 0 for no additional
 * queues in adapter
 *
 * ESX Deviation Notes:
 *      ESX specific API to query VMKernel for the number of queues to
 *      be created
 */
/* _VMKLNX_CODECHECK_: vmklnx_scsi_get_num_ioqueue*/
int
vmklnx_scsi_get_num_ioqueue(unsigned int maxQueue)
{
    int num_queues;
 
    VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
    num_queues = vmk_ScsiGetNumCompObjects();
    return ((num_queues > maxQueue) || (num_queues <= 1)) ? 0 : num_queues;
}
EXPORT_SYMBOL(vmklnx_scsi_get_num_ioqueue);

/**
 * returns queue handle
 * @cmd: scsi command 
 * @sh: scsi host
 *
 * RETURN VALUE:
 * ioqueue handle
 *
 * Deviation Notes:
 *      ESX specific API to query VMKernel for the ioqueues to be used
 *      to issue the given scsi command
 * None
 */
/* _VMKLNX_CODECHECK_: vmklnx_scsi_get_cmd_ioqueue_handle*/
void * 
vmklnx_scsi_get_cmd_ioqueue_handle(struct scsi_cmnd *cmd,
                                   struct Scsi_Host *sh)
{   
   struct vmklnx_ScsiAdapter *adapter = sh->adapter;
   void *tls = NULL;
  
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if (0 == (cmd->vmkflags & VMK_FLAGS_INTERNAL_COMMAND)) {
      vmk_ScsiCompletionHandle cmpObj;
     
      cmpObj = vmk_ScsiCommandGetCompletionHandle(adapter->vmkAdapter,
                                                  cmd->vmkCmdPtr);
      tls = cmpObj.ptr;
   }
   return tls ? SCSILinuxTLSGetIOQueueHandle(tls) : NULL;
}
EXPORT_SYMBOL(vmklnx_scsi_get_cmd_ioqueue_handle);

/**
 * vmklnx_scsi_register_ioqueue-- pass queue info to vmkernel.
 * ESX specific API to register all the ioqueues in the adapter
 * with VMKernel. For each ioqueue the driver should provide the
 * handle and interrupt vector. The handle will be returned by
 * vmkernel when vmklnx_scsi_get_cmd_ioqueue_handle() is invoked
 * to select queue for a scsi_cmd.
 * @sh: scsi host
 * @numIoQueue: Number of I/O queues
 * @q_info: scsi_ioqueue_info for each queue
 *
 * RETURN VALUE:
 * return zero if successful, error code otherwise
 */
/* _VMKLNX_CODECHECK_: vmklnx_scsi_register_ioqueue*/
int
vmklnx_scsi_register_ioqueue(struct Scsi_Host *sh, unsigned int numIoQueue,
                     struct vmklnx_scsi_ioqueue_info q_info[])
{
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter = sh->adapter;
   int i;
   unsigned long flags;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(vmklnx26ScsiAdapter);
   if (vmklnx26ScsiAdapter == NULL) {
      return VMK_BAD_PARAM;
   }
   if (numIoQueue <= 1) {
      return VMK_BAD_PARAM;
   }
   if (numIoQueue >  vmklnx26ScsiAdapter->numTls) {
      vmk_WarningMessage("%s numQueue(%d) numTLS(%d)\n",
                         vmk_NameToString(&vmklnx26ScsiAdapter->vmkAdapter->name),
                         numIoQueue, (int)vmklnx26ScsiAdapter->numTls);
      return VMK_BAD_PARAM;
   }

   spin_lock_irqsave(&vmklnx26ScsiAdapter->lock, flags);

   /*
    * vmklnx_scsi_register_ioqueue() is not expected to be called
    * again after initialization.
    */
   VMK_ASSERT(!vmklnx26ScsiAdapter->mq_registered);
   if (vmklnx26ScsiAdapter->mq_registered) {
      spin_unlock_irqrestore(&vmklnx26ScsiAdapter->lock, flags);
      return VMK_EXISTS;
   }

   for (i = 0; i < vmklnx26ScsiAdapter->numTls; i++) {
      struct scsiLinuxTLS **tls = vmklnx26ScsiAdapter->tls;
      int j = i % numIoQueue;

      if (SCSILinuxTLSSetIOQueueHandle(tls[j], q_info[i].q_handle) !=
                VMK_OK) {
         goto registeration_error;
      }
   }

   vmklnx26ScsiAdapter->mq_registered = TRUE;
   spin_unlock_irqrestore(&vmklnx26ScsiAdapter->lock, flags);
   return VMK_OK;

registeration_error:

   /* Adapter lock is acquired already */

   for (i = 0; i < vmklnx26ScsiAdapter->numTls; i++) {
      struct scsiLinuxTLS **tls = vmklnx26ScsiAdapter->tls;
      int j = i % numIoQueue;

      SCSILinuxTLSSetIOQueueHandle(tls[j], NULL);
   }
   spin_unlock_irqrestore(&vmklnx26ScsiAdapter->lock, flags);

   return VMK_FAILURE;
}
EXPORT_SYMBOL(vmklnx_scsi_register_ioqueue);

static void
_scsi_device_hot_removed(struct scsi_device *sdev, void* data)
{
	unsigned long flags;

	VMK_ASSERT(sdev != NULL);

   	spin_lock_irqsave(sdev->host->host_lock, flags);
	sdev->vmkflags |= HOT_REMOVED;
   	spin_unlock_irqrestore(sdev->host->host_lock, flags);

	return;
}

/**                                          
 *  vmklnx_scsi_device_hot_removed - Mark a scsi device been hot removed
 *  @sdev: Scsi device been removed   
 *                                           
 *  This will trigger a PDL when scsi_remove_device or
 *  the equivalent are called                                           
 *                                           
 *  RETURN VALUE:
 *  NONE
 */                                          
/* _VMKLNX_CODECHECK_: vmklnx_scsi_device_hot_removed */
void
vmklnx_scsi_device_hot_removed(struct scsi_device *sdev)
{
	_scsi_device_hot_removed(sdev, NULL);
	return;
}
EXPORT_SYMBOL(vmklnx_scsi_device_hot_removed);

/**                                          
 *  vmklnx_scsi_target_hot_removed - Mark all scsi devices connected
 *  to a certain target as been hot removed
 *  @starget: Scsi target been removed   
 *                                           
 *  This will trigger PDL for all the device connected to the target
 *  when scsi_remove_device or the equivalent are called                                           
 *                                           
 *  RETURN VALUE:
 *  NONE
 */                                          
/* _VMKLNX_CODECHECK_: vmklnx_scsi_target_hot_removed */
void
vmklnx_scsi_target_hot_removed(struct scsi_target *starget)
{
	VMK_ASSERT(starget != NULL);
	starget_for_each_device(starget, NULL, _scsi_device_hot_removed);
	return;
}
EXPORT_SYMBOL(vmklnx_scsi_target_hot_removed);

/*
 *----------------------------------------------------------------------
 *
 * SCSILinux_InitLLD
 *
 *    Entry point for SCSI LLD-specific initialization.
 *    Called as part of SCSILinux_Init in linux_scsi.c.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Initializes SCSI LLD log.
 *
 *----------------------------------------------------------------------
 */
void
SCSILinux_InitLLD(void)
{
   VMKLNX_CREATE_LOG();
}

/*
 * SCSILinux_CleanupLLD
 *
 *    Entry point for SCSI LLD-specific teardown.
 *    Called as part of SCSILinux_Cleanup in linux_scsi.c.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Cleans up SCSI LLD log.
 */
void
SCSILinux_CleanupLLD(void)
{
   VMKLNX_DESTROY_LOG();
}

/**                                          
 *  scsi_device_reprobe - Rediscover device status by reprobing if needed       
 *  @sdev: Pointer to scsi_device which needs status update
 *                                           
 *  This function will either hide a device which doesn't need exposure to upper
 *  layers anymore or will initiate a scan to rediscover new devices on the
 *  parent of the passed in scsi_device.
 *                                           
 *  ESX Deviation Notes:                     
 *  Decision is based on the scsi_device's no_uld_attach member
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: scsi_device_reprobe */
void scsi_device_reprobe(struct scsi_device *sdev)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if (sdev == NULL)
      return;

   if (sdev->no_uld_attach) {
      VMKLNX_DEBUG(3, "%s sdev: %p %d:%d:%d no_uld_attach:%d\n",
                   __FUNCTION__, sdev, sdev->channel, sdev->id, sdev->lun,
                   sdev->no_uld_attach);
      scsi_offline_device(sdev);
   } else {
      VMKLNX_DEBUG(3, "%s sdev: %p %d:%d:%d no_uld_attach:%d\n",
                   __FUNCTION__, sdev, sdev->channel, sdev->id, sdev->lun,
                   sdev->no_uld_attach);
      scsi_scan_target(sdev->sdev_gendev.parent, sdev->channel,
                       sdev->id, sdev->lun, 1);
   }
}
EXPORT_SYMBOL(scsi_device_reprobe);

/**
 **********************************************************************
 * vmklnx_scsi_host_set_capabilities - Set adapter capability
 *
 * @sh: Pointer to scsi host
 * @cap: Capabilities to be set
 *
 * Set adapter capabilities. Must be called before scsi_add_host().
 *
 * RETURN VALUE:
 * Returns 0 on success, non zero on failure.
 **********************************************************************
 */
int vmklnx_scsi_host_set_capabilities(struct Scsi_Host *sh,
                                      enum scsi_host_capabilities cap)
{
   vmk_ScsiAdapter *vmkAdapter = sh->vmkAdapter;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(vmkAdapter);

   return vmk_ScsiAdapterSetCapabilities(vmkAdapter, cap);
}
EXPORT_SYMBOL(vmklnx_scsi_host_set_capabilities);


/**
 **********************************************************************
 * vmklnx_scsi_host_get_capabilities - Get adapter capabilities
 *
 * @sh: Pointer to scsi host
 * @mask: Pointer where the adapter capabilities mask will be stored
 *
 * Get adapter capabilities.
 *
 * RETURN VALUE:
 * Returns 0 on success, non zero on failure.
 **********************************************************************
 */
int
vmklnx_scsi_host_get_capabilities(const struct Scsi_Host *sh,
                                  uint32_t *mask)
{
   const vmk_ScsiAdapter *vmkAdapter = sh->vmkAdapter;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(vmkAdapter);

   return vmk_ScsiAdapterGetCapabilities(vmkAdapter, mask);
}
EXPORT_SYMBOL(vmklnx_scsi_host_get_capabilities);


/**
 **********************************************************************
 * vmklnx_scsi_host_has_capabilities - check if capabilities are set
 *
 * @sh: Pointer to scsi host
 * @cap_mask: 32-bit mask of capabilities to be checked
 * @hasCapabilities: Pointer where matched capabilities result is
 *                   stored
 *
 * Check if supplied capabilities matches with the host's capabilities
 *
 * RETURN VALUE:
 * Returns 0 on success, non zero on failure.
 **********************************************************************
 */
int
vmklnx_scsi_host_has_capabilities(const struct Scsi_Host *sh,
                                  uint32_t cap_mask,
                                  bool *hasCapabilities)
{
   const vmk_ScsiAdapter *vmkAdapter = sh->vmkAdapter;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(vmkAdapter);
   VMK_ASSERT_ON_COMPILE(sizeof(*hasCapabilities) ==
                         sizeof(vmk_Bool));

   return vmk_ScsiAdapterHasCapabilities(vmkAdapter, cap_mask,
                  (vmk_Bool *) hasCapabilities);
}
EXPORT_SYMBOL(vmklnx_scsi_host_has_capabilities);


/**
 * vmklnx_scsi_cmd_get_secondlevel_lun_id - get second-level lun id
 *
 * @scmd: SCSI command
 *
 * Get second-level lun id for SCSI command. Returns VMKLNX_SCSI_INVALID_SLLID
 * if the second-level lun id is not set.
 *
 * RETURN VALUE:
 * Returns the 64-bit second-level lun id or VMKLNX_SCSI_INVALID_SLLID.
 */
uint64_t
vmklnx_scsi_cmd_get_secondlevel_lun_id(const struct scsi_cmnd *scmd)
{
   uint64_t sllid = VMKLNX_SCSI_INVALID_SECONDLEVEL_ID;
   const vmk_ScsiCommand *vmkCmdPtr;
   vmk_Bool isInternalCmd = VMK_FALSE;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(scmd);

   vmkCmdPtr = scmd->vmkCmdPtr;

   isInternalCmd = (scmd->vmkflags & VMK_FLAGS_INTERNAL_COMMAND)
                     ? VMK_TRUE : VMK_FALSE;

   if (likely((!isInternalCmd) && vmkCmdPtr)) {
      sllid = vmk_ScsiCmdGetSecondLevelLunId(vmkCmdPtr);
   }

   return sllid;
}
EXPORT_SYMBOL(vmklnx_scsi_cmd_get_secondlevel_lun_id);

/**
 * vmklnx_scsi_cmd_get_sensedata - Get sense data length from SCSI cmd
 * @scmd: SCSI command
 * @buf: Buffer that will contain sense data
 * @bufLen: Length of the sense buffer
 *
 * Command is identified by scmd.  The buffer buf is filled with sense data.
 * The length of the input buffer is passed through bufLen.
 *
 */
/* _VMKLNX_CODECHECK_: vmklnx_scsi_cmd_get_sensedata */
int
vmklnx_scsi_cmd_get_sensedata(struct scsi_cmnd *scmd,
                              uint8_t *buf,
                              uint64_t bufLen)
{
   return vmk_ScsiCmdGetSenseData(scmd->vmkCmdPtr, (vmk_ScsiSenseData *)buf,
                                  bufLen);
}
EXPORT_SYMBOL(vmklnx_scsi_cmd_get_sensedata);

/**
 * vmklnx_scsi_cmd_set_sensedata - Set sense data of SCSI cmd
 * @buf: Buffer that contains sense data to set
 * @scmd: SCSI command
 * @bufLen: Number of bytes to copy
 *
 * Command is identified by scmd.  The number of bytes to copy from buf
 * is passed as bufLen.  If the number of bytes to copy is less than the
 * sense buffer size of SCSI cmd(obtained by calling
 * vmklnx_scsi_cmd_get_supportedsensedata_size), the remaining bytes in
 * SCSI cmd's sense buffer are set to 0.
 */
/* _VMKLNX_CODECHECK_: vmklnx_scsi_cmd_set_sensedata */
int
vmklnx_scsi_cmd_set_sensedata(uint8_t *buf,
                              struct scsi_cmnd *scmd,
                              uint64_t bufLen)
{
   return vmk_ScsiCmdSetSenseData((vmk_ScsiSenseData *)buf, scmd->vmkCmdPtr,
                                  bufLen);
}
EXPORT_SYMBOL(vmklnx_scsi_cmd_set_sensedata);

/**
 * vmklnx_scsi_cmd_clear_sensedata - Clear sense data of SCSI cmd
 * @scmd: SCSI command
 *
 * Command is identified by scmd.
 *
 */
/* _VMKLNX_CODECHECK_: vmklnx_scsi_cmd_clear_sensedata */
int
vmklnx_scsi_cmd_clear_sensedata(struct scsi_cmnd *scmd)
{
   return vmk_ScsiCmdClearSenseData(scmd->vmkCmdPtr);
}
EXPORT_SYMBOL(vmklnx_scsi_cmd_clear_sensedata);

/**
 * vmklnx_scsi_cmd_get_supportedsensedata_size - Get scmd's sense buffer size
 *
 */
/* _VMKLNX_CODECHECK_: vmklnx_scsi_cmd_get_supportedsensedata_size */
int
vmklnx_scsi_cmd_get_supportedsensedata_size()
{
   return (int)vmk_ScsiGetSupportedCmdSenseDataSize();
}
EXPORT_SYMBOL(vmklnx_scsi_cmd_get_supportedsensedata_size);
