/* ****************************************************************
 * Portions Copyright 1998, 2010, 2012, 2015 VMware, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * ****************************************************************/

/******************************************************************
 *
 *  linux_scsi.c
 *
 *      vmklinux main entry points and scsi utility functions
 *
 * From linux-2.4.31/drivers/scsi/scsi_error.c:
 *
 * Copyright (C) 1997 Eric Youngdale
 *
 ******************************************************************/

#include <scsi/scsi.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_eh.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_transport.h>
#include <scsi/scsi_tcq.h>
#include <scsi/scsi_eh.h>
#include <linux/pci.h>

#include "vmkapi.h"
#include "linux_stubs.h" 
#include "linux_scsi.h"
#include "linux_stress.h"
#include "linux_pci.h"

#define VMKLNX_LOG_HANDLE LinScsi
#include "vmklinux_log.h"

/*
 *  Static Local Functions
 ********************************************************************
 */
static int SCSILinuxTryBusDeviceReset(struct scsi_cmnd * SCpnt, int timeout);
static int SCSILinuxTryBusReset(struct scsi_cmnd * SCpnt);
static void SCSILinuxProcessStandardInquiryResponse(struct scsi_cmnd *scmd);
static VMK_ReturnStatus SCSILinuxComputeSGArray(struct scsi_cmnd *scmd, 
						vmk_ScsiCommand *vmkCmdPtr);
static VMK_ReturnStatus SCSILinuxAbortCommand(struct Scsi_Host *sh, 
					      struct scsi_cmnd *cmdPtr, 
					      int32_t abortReason);
static void SCSIProcessCmdTimedOut(struct work_struct *work);

#define SCSI_AT_SET_THRESHOLD    60
#define SCSI_AT_DROP_THRESHOLD   40
#define SCSI_AT_UPDATE_PERIOD    50000

/*
 * Globals
 ********************************************************************
 */


typedef struct scsiLinuxTLS {
   /*
    * isrDoneCmds is a per-context list of completions.
    */
   struct list_head isrDoneCmds;

   /*
    * bhDoneCmds is a per-PCPU list of completions, only accessed at
    * post-processing time. So we can access it without locks.
    */
   struct list_head bhDoneCmds;

   /*
    * cpu this object is assign to -- only has meaning for BHs.
    */
   vmk_uint32           cpu;

   /*
    * lock is used for synchronizing access to isrDoneCmds when using a
    * worldlet.
    */
   spinlock_t           lock;

   /*
    * worldlet object, if TLS is worldlet-specific.
    */
   vmk_Worldlet         worldlet;
   vmk_WorldletID       worldletId;

   /*
    * Interrupt that activates the worldlet.
    */
   vmk_IntrCookie       activatingIntr;

   /*
    * Affinity tracker that will figure out the best affinity settings for the
    * worldlet.
    */
   vmk_WorldletAffinityTracker  *tracker;

   /*
    * Scsi Adapter Object to which this TLS belong
    */
   struct vmklnx_ScsiAdapter *vmk26Adap;

   /*
    * Handle of the Adapter's IO queue which is associated with this TLS.
    */
   void                *adapterIOQueueHandle;

   char pad[0] VMK_ATTRIBUTE_L1_ALIGNED; // Pad to make struct 128 byte aligned
} scsiLinuxTLS_t;

#if !defined(__VMKLNX__)
scsiLinuxTLS_t *scsiLinuxTLS[NR_CPUS] VMK_ATTRIBUTE_L1_ALIGNED;
#else /* defined(__VMKLNX__) */
scsiLinuxTLS_t **scsiLinuxTLS;

static vmk_PCPUStorageHandle tlsPCPUHandle;

/*
 * SCSI Adapter list and lock associated with this
 */
struct list_head	linuxSCSIAdapterList;
vmk_SpinlockIRQ 	linuxSCSIAdapterLock;

/*
 * Command Serial Number
 *
 * DO NOT access this directly. Instead, use SCSILinuxGetSerialNumber()
 */
vmk_atomic64 SCSILinuxSerialNumber = 1;

/* 
 * Stress option handles
 */
vmk_StressOptionHandle stressScsiAdapterIssueFail;
static vmk_StressOptionHandle stressVmklnxDropCmdScsiDone;
static vmk_StressOptionHandle stressVmklinuxAbortCmdFailure;

/*
 * Work Queue used to destroy individual adapter instance
 * and handle I/O timeouts.
 */
struct workqueue_struct *linuxSCSIWQ;
struct scsiIO_work_struct {
   struct work_struct work;
   struct scsi_cmnd *scmd;
} linuxSCSIIO_work;

/*
 * Externs
 ********************************************************************
 */
extern struct mutex host_cmd_pool_mutex;
extern void SCSILinux_InitLLD();
extern void SCSILinux_InitTransport();
extern void SCSILinux_InitVmkIf();
extern void SCSILinux_CleanupLLD();
extern void SCSILinux_CleanupTransport();
extern void SCSILinux_CleanupVmkIf();

/* Throttle timeout */
#define THROTTLE_TO               (15 * 60)	// 15 mins

/* 
 * WARNING: The following constants are defined in a future version
 *          of vmkapi, but this version of vmklinux is compiled against
 *          against vmkapi v2_3_0_0.  It is not possible to re-write
 *          vmkapi history on main.  So we define them here in vmklinux.
 *
 *          While this version of vmklinux should, in theory, execute
 *          properly against the actual 6.0 GA vmkernel, that won't be
 *          tested.  So therefore, this version of vmklinux will contain
 *          an esx-base dependency for 6.1 or above.
 */

/* Logical unit is not configured (array only). */
#define VMKLNX_SCSI_ASC_ATA_PASSTHROUGH_INFO_AVAILABLE                  0x00
#define VMKLNX_SCSI_ASCQ_ATA_PASSTHROUGH_INFO_AVAILABLE                 0x1d

/*
 * Description Type field
 * SPC 4 r33, Section 4.5.2.1 table 27
 */
#define VMKLNX_SCSI_SENSE_DESCRIPTOR_TYPE_INFORMATION                    0x0
#define VMKLNX_SCSI_SENSE_DESCRIPTOR_TYPE_COMMAND_SPECIFIC_INFIRMATION   0x1
#define VMKLNX_SCSI_SENSE_DESCRIPTOR_TYPE_SENSE_KEY_SPECIFIC             0x2
#define VMKLNX_SCSI_SENSE_DESCRIPTOR_TYPE_FIELD_REPLACABLE_UNIT          0x3
#define VMKLNX_SCSI_SENSE_DESCRIPTOR_TYPE_ATA_STATUS_RETURN              0x9
/*
 * END WARNING
 */


/*
 *-----------------------------------------------------------------------------
 *
 * SCSILinuxTLSPCPUInit --
 *
 *      Initialize per-PCPU storage of active TLS objects.  When a TLS is
 *      executing, we note that in per-PCPU storage so that we can identify it
 *      in case it submits new requests. (We then avoid counting the TLS
 *      worldlet for affinity tracker purposes.)
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
static void
SCSILinuxTLSPCPUInit(void)
{
   vmk_PCPUStorageProps props;
   VMK_ReturnStatus status;

   props.type = VMK_PCPU_STORAGE_TYPE_WRITE_LOCAL;
   props.moduleID = vmklinuxModID;
   vmk_NameInitialize(&props.name, "SCSILinuxTLSActiveTbl");
   props.constructor = NULL;
   props.destructor = NULL;
   props.size = sizeof(struct scsiLinuxTLS_t*);
   props.align = 0;

   status = vmk_PCPUStorageCreate(&props, &tlsPCPUHandle);
   VMK_ASSERT(status == VMK_OK);
   if (VMK_UNLIKELY(status != VMK_OK)) {
      vmk_Panic("Failed to allocate SCSI TLS table.");
   }
}

/*
 *-----------------------------------------------------------------------------
 *
 * SCSILinuxTLSSetActive --
 *
 *      Set the specified TLS (or NULL) as "active" on the current PCPU.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static inline void
SCSILinuxTLSSetActive(scsiLinuxTLS_t *curr)
{
   VMK_ReturnStatus status;
   scsiLinuxTLS_t **location;
   VMK_WITH_PCPU_DO(pcpu) {
      status = vmk_PCPUStorageLookUp(pcpu, tlsPCPUHandle, (void*)&location);
      if (VMK_UNLIKELY(status != VMK_OK)) {
         vmk_Panic("Failed SCSI TLS lookup.");
      }
      *location = curr;
      vmk_PCPUStorageRelease(tlsPCPUHandle, (void*)location);
   } VMK_END_WITH_PCPU(pcpu);
}

/*
 *-----------------------------------------------------------------------------
 *
 * SCSILinuxTLSGetActive --
 *
 *      Retrieve the current active TLS on this PCPU (if any)
 *
 * Results:
 *      Pointer to current scsiLinuxTLS_t.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static scsiLinuxTLS_t*
SCSILinuxTLSGetActive(void)
{
   VMK_ReturnStatus status;
   void *tmp;
   scsiLinuxTLS_t *tls;
   VMK_WITH_PCPU_DO(pcpu) {
      status = vmk_PCPUStorageLookUp(pcpu, tlsPCPUHandle, &tmp);
      if (VMK_UNLIKELY(status != VMK_OK)) {
         vmk_Panic("Failed SCSI TLS lookup.");
      }
      tls = *(scsiLinuxTLS_t**)tmp;

      vmk_PCPUStorageRelease(tlsPCPUHandle, tmp);
   } VMK_END_WITH_PCPU(pcpu);
   return tls;
}





/*
 * Implementation
 ********************************************************************
 */

/*
 *----------------------------------------------------------------------
 *
 * SCSILinux_Init
 *
 *      This is the init entry point for SCSI. Called from vmklinux 
 *    init from linux_stubs.c
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */
void
SCSILinux_Init(void)
{
   VMK_ReturnStatus status;

   VMK_ASSERT_ON_COMPILE(sizeof(scsiLinuxTLS_t) % L1_CACHE_BYTES == 0);
   VMKLNX_CREATE_LOG();

   status = vmk_SPCreateIRQ_LEGACY(&linuxSCSIAdapterLock, vmklinuxModID, 
			"scsiHostListLck", NULL, VMK_SP_RANK_IRQ_LEAF_LEGACY);

   if (status != VMK_OK) {
      VMKLNX_ALERT("Failed to initialize Adapter Spinlock");
      VMK_ASSERT(status == VMK_OK);
   }

   scsiLinuxTLS = vmklnx_kmalloc_align(VMK_MODULE_HEAP_ID,
                                       smp_num_cpus * sizeof(scsiLinuxTLS_t *),
                                       VMK_L1_CACHELINE_SIZE,
                                       GFP_KERNEL);
   if (scsiLinuxTLS == NULL) {
      VMKLNX_PANIC("Unable to allocate per-PCPU pointer array in vmklinux");
   }

   INIT_LIST_HEAD(&linuxSCSIAdapterList);

   mutex_init(&host_cmd_pool_mutex);

   linuxSCSIWQ = create_singlethread_workqueue("linuxSCSIWQ");
   VMK_ASSERT(linuxSCSIWQ);

   status = vmk_StressOptionOpen(VMK_STRESS_OPT_SCSI_ADAPTER_ISSUE_FAIL,
                                 &stressScsiAdapterIssueFail);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_StressOptionOpen(VMK_STRESS_OPT_VMKLINUX_DROP_CMD_SCSI_DONE,
                                 &stressVmklnxDropCmdScsiDone);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_StressOptionOpen(VMK_STRESS_OPT_VMKLINUX_ABORT_CMD_FAILURE,
                                 &stressVmklinuxAbortCmdFailure);
   VMK_ASSERT(status == VMK_OK);

   SCSILinux_InitLLD();
   SCSILinux_InitTransport();
   SCSILinux_InitVmkIf();
   SCSILinuxTLSPCPUInit();
}

/*
 *----------------------------------------------------------------------
 *
 * SCSILinux_Cleanup
 *
 *      This is the cleanup entry point for SCSI. Called during vmklinux 
 *    unload from linux_stubs.c
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */
void
SCSILinux_Cleanup(void)
{
   VMK_ReturnStatus status;

   SCSILinux_CleanupVmkIf();
   SCSILinux_CleanupTransport();
   SCSILinux_CleanupLLD();

   vmk_SPDestroyIRQ(&linuxSCSIAdapterLock);

   mutex_destroy(&host_cmd_pool_mutex);

   vmklnx_kfree(VMK_MODULE_HEAP_ID, scsiLinuxTLS);

   destroy_workqueue(linuxSCSIWQ);
   status = vmk_StressOptionClose(stressScsiAdapterIssueFail);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_StressOptionClose(stressVmklnxDropCmdScsiDone);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_StressOptionClose(stressVmklinuxAbortCmdFailure);
   VMK_ASSERT(status == VMK_OK);

   VMKLNX_DESTROY_LOG();
}



/*
 *-----------------------------------------------------------------------------
 *
 * SCSILinuxGetTLS --
 *
 *      Retrieve the TLS for a given command.  This is expected to be an
 *      adapter-specific TLS corresponding to a worldlet, but as long as the
 *      panic switch (PR 471425) is in place, the TLS may be PCPU specific.
 *
 * Results:
 *      scsiLinuxTLS_t pointer as above.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static inline scsiLinuxTLS_t *
SCSILinuxGetTLS(struct scsi_cmnd *cmd)
{
   struct vmklnx_ScsiAdapter *adp = NULL;

   if (cmd && cmd->device && cmd->device->host && cmd->device->host->adapter) {
      adp = (struct vmklnx_ScsiAdapter*)cmd->device->host->adapter;
   }

   VMK_ASSERT(adp != NULL);
   if (cmd->vmkCmdPtr && (cmd->vmkflags & VMK_FLAGS_INTERNAL_COMMAND) == 0) {
      vmk_ScsiCompletionHandle cmpObj;

      cmpObj = vmk_ScsiCommandGetCompletionHandle(adp->vmkAdapter,
                                                  cmd->vmkCmdPtr);
      if (cmpObj.ptr != NULL) {
         return cmpObj.ptr;
      }
   }

   VMK_ASSERT(adp->tls != NULL && adp->tls[0]);
   return adp->tls[0];
}


/*
 *-----------------------------------------------------------------------------
 *
 * SCSILinuxTLSWorkPending --
 *
 *      Indicate if a particular execution context has work pending.
 *
 * Results:
 *      TRUE/FALSE as above.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static vmk_Bool
SCSILinuxTLSWorkPending(scsiLinuxTLS_t *tls)
{
   return !list_empty(&tls->isrDoneCmds) || !list_empty(&tls->bhDoneCmds);
}


/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxSetName
 *
 *      Set adapter name with given name by the driver
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      If the device is associated with PCI bus, update the PCI device
 *      name list.
 *
 *----------------------------------------------------------------------
 */
void
SCSILinuxSetName(vmk_ScsiAdapter *vmkAdapter, struct Scsi_Host const *sHost,
                 struct pci_dev *pciDev)
{
   vmk_PCIDevice vmkDev;
   char adapterName[VMK_MISC_NAME_MAX];
   VMK_ReturnStatus status = VMK_OK; 

   if (LinuxPCI_IsValidPCIBusDev(pciDev)) {
      vmk_PCIGetPCIDevice(pci_domain_nr(pciDev->bus),
                          pciDev->bus->number,
                          PCI_SLOT(pciDev->devfn),
                          PCI_FUNC(pciDev->devfn), &vmkDev);
      vmk_PCISetDeviceName(vmkDev, sHost->name);
   }

   VMK_ASSERT_ON_COMPILE(VMK_DEVICE_NAME_MAX_LENGTH <= VMK_MISC_NAME_MAX);
   strncpy(adapterName, sHost->name, VMK_MISC_NAME_MAX);
   adapterName[VMK_MISC_NAME_MAX - 1] = 0;
   status = vmk_NameInitialize(&vmkAdapter->name, adapterName);
   VMK_ASSERT(status == VMK_OK);
}

/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxNameAdapter
 *
 *      Find a good adapter name for the given pci device
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */
void
SCSILinuxNameAdapter(vmk_Name *adapterName, struct pci_dev *pciDev)
{
   vmk_PCIDevice vmkDev = NULL;
   VMK_ReturnStatus status = VMK_FAILURE;
   char pciDeviceName[VMK_MISC_NAME_MAX];

   if (pciDev != NULL) {

      status = vmk_PCIGetPCIDevice(pci_domain_nr(pciDev->bus),
                                pciDev->bus->number, 
                                PCI_SLOT(pciDev->devfn), 
                                PCI_FUNC(pciDev->devfn), &vmkDev);

      VMK_ASSERT(status == VMK_OK);

      status = vmk_PCIGetDeviceName(vmkDev, 
                                    pciDeviceName, 
                                    VMK_MISC_NAME_MAX);
      if (status == VMK_OK) {
         vmk_NameInitialize(adapterName, pciDeviceName);
      }
   }

   if (status != VMK_OK) {
      /*
       * Get a new name.
       */
      vmk_ScsiAdapterUniqueName(adapterName);

      VMKLNX_DEBUG(0, "Adapter name %s", vmk_NameToString(adapterName));

      if (vmkDev != NULL) {
         vmk_PCISetDeviceName(vmkDev, vmk_NameToString(adapterName));
      }
   }
}

/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxGetModuleID --
 *
 *
 * Results: 
 *      valid module id
 *      VMK_INVALID_MODULE_ID if the id cannot be determined 
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
vmk_ModuleID
SCSILinuxGetModuleID(struct Scsi_Host *sh, struct pci_dev *pdev)
{
   vmk_ModuleID moduleID = VMK_INVALID_MODULE_ID;
   struct vmklnx_ScsiModule *vmklnx26ScsiModule;

   /*
    * We will start with Module wide structures
    */
   if (pdev != NULL) {
      /*
       * PCI structure has the driver name. So start with it
       * This does not hold good for pseudo drivers
       */
      if (pdev->driver) {
         moduleID = pdev->driver->driver.owner->moduleID;
      }
   }

   if (moduleID != VMK_INVALID_MODULE_ID) {
      return(moduleID);
   }

   /*
    * Fall back 1. The transport parameters are module wide, so try to
    * extract the details from transport template if the driver has one
    * Drawback is some of the scsi drivers dont have transport structs
    */
   vmklnx26ScsiModule = (struct vmklnx_ScsiModule *)sh->transportt->module;

   if (vmklnx26ScsiModule) {
      moduleID = vmklnx26ScsiModule->moduleID;
      if (moduleID != VMK_INVALID_MODULE_ID) {
         return (moduleID);
      }
   }

   /*
    * Fallback 2 - Now it is possible that calling function is not the module
    * Fall back to 24 flow as below. Is USB a good example?
    */
   moduleID = vmk_ModuleStackTop();

   /*
    * Try to get if the SHT has module.
    */   
   if (sh->hostt->module != NULL) {
      /*
       * If possible extract the ID from the host template information
       * If don't match prefer the host template ID but Log just in case
       */
      vmk_ModuleID shtModuleID = sh->hostt->module->moduleID;

      if (shtModuleID != VMK_INVALID_MODULE_ID && shtModuleID != moduleID) {
         VMKLNX_DEBUG(0, "Current module ID is %"VMK_FMT64"x "
                         "but ht module ID is %"VMK_FMT64"x, using the latter "
                         "for driver that supports %s.\n",
                         vmk_ModuleGetDebugID(moduleID),
                         vmk_ModuleGetDebugID(shtModuleID),
                         sh->hostt->name);
         moduleID = shtModuleID;
      }
   }

   if (moduleID == VMK_INVALID_MODULE_ID) {
      VMKLNX_DEBUG(0, "Could not get the module id for"
                      "the driver that supports %s."
                      "The device will not be registered correctly.\n",
                      sh->hostt->name);
      VMK_ASSERT(moduleID != VMK_INVALID_MODULE_ID);
   }

   return(moduleID);
}

/*
 *-----------------------------------------------------------------------------
 *
 * SCSILinuxATASenseDescriptorToFixed --
 *
 *      Convert ATA status return encapsulated in descriptor format sense
 *      data to fixed format. See sec 12.2.2.6 in sat3r05b.pdf
 *
 * Results:
 *      returns VMK_TRUE if  buffer is modified.
 *
 * Side effects:
 *
 *-----------------------------------------------------------------------------
 */
static vmk_Bool
SCSILinuxATASenseDescriptorToFixed(unsigned char *buffer)
{
   if (buffer[0] == 0x72) {
      vmk_ScsiSenseDataSimple *senseData = (vmk_ScsiSenseDataSimple *)buffer;
      if (senseData->format.descriptor.key == VMK_SCSI_SENSE_KEY_RECOVERED_ERROR &&
          senseData->format.descriptor.asc == VMKLNX_SCSI_ASC_ATA_PASSTHROUGH_INFO_AVAILABLE &&
          senseData->format.descriptor.ascq == VMKLNX_SCSI_ASCQ_ATA_PASSTHROUGH_INFO_AVAILABLE &&
          senseData->format.descriptor.optLen == 0xe &&
          senseData->format.descriptor.additional[0] == VMKLNX_SCSI_SENSE_DESCRIPTOR_TYPE_ATA_STATUS_RETURN) {
         vmk_ScsiSenseData fixed = {0};

         fixed.error = VMK_SCSI_SENSE_ERROR_CURCMD;
         fixed.key = VMK_SCSI_SENSE_KEY_NONE;
         fixed.asc = senseData->format.descriptor.asc;
         fixed.ascq = senseData->format.descriptor.ascq;
         fixed.optLen = 24;
         vmk_Memcpy(&fixed.additional[0],
                    &senseData->format.descriptor.additional[0],
                    senseData->format.descriptor.optLen);
         vmk_Memcpy(buffer, &fixed, sizeof(fixed));

         return VMK_TRUE;
      }
   }
   return VMK_FALSE;
}

/*
 *-----------------------------------------------------------------------------
 *
 * SCSILinuxProcessCompletions --
 *
 *      Process completions pending for a given tls struct.
 *      Run until time reaches "yield".
 *      Update "*now" with current time whenever probing time.
 *
 * Results:
 *      Returns the number of IO completions processed.
 *
 * Side effects:
 *      Updates "*now".
 *
 *-----------------------------------------------------------------------------
 */

static int
SCSILinuxProcessCompletions(scsiLinuxTLS_t *tls,       // IN
                            vmk_TimerCycles yield,     // IN
                            vmk_TimerCycles *now)      // IN/OUT

{
   vmk_ScsiHostStatus hostStatus;
   vmk_ScsiDeviceStatus deviceStatus;
   int num_cmp = 0;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   while (!list_empty(&tls->bhDoneCmds) && *now < yield) {
      struct scsi_cmnd *scmd;
#ifdef VMKLNX_TRACK_IOS_DOWN
      struct scsi_device *sdev;
#endif
      vmk_ScsiCommand *vmkCmdPtr;
      unsigned long flags;

      scmd = list_entry(tls->bhDoneCmds.next, struct scsi_cmnd, bhlist);
      list_del(&scmd->bhlist);

      /*
       * First we check for internal commands, which we complete directly.
       */
      if (scmd->vmkflags & VMK_FLAGS_INTERNAL_COMMAND) {
         scmd->done(scmd);
         *now = vmk_GetTimerCycles();
         continue;
      }

      /*
       * Map the Linux driver status bytes in to something the VMkernel
       * knows how to deal with.
       */
      switch (driver_byte(VMKLNX_SCSI_STATUS_NO_SUGGEST(scmd->result))) {
      case DRIVER_OK:
         hostStatus = VMKLNX_SCSI_HOST_STATUS(scmd->result);
         deviceStatus = VMKLNX_SCSI_DEVICE_STATUS(scmd->result);
         break;

      case DRIVER_BUSY:
      case DRIVER_SOFT:
         hostStatus = VMK_SCSI_HOST_BUS_BUSY;
         deviceStatus = VMK_SCSI_DEVICE_GOOD;
         break;

      case DRIVER_MEDIA:
      case DRIVER_ERROR:
      case DRIVER_HARD:
      case DRIVER_INVALID:
         hostStatus = VMK_SCSI_HOST_ERROR;
         deviceStatus = VMK_SCSI_DEVICE_GOOD;
         break;

      case DRIVER_TIMEOUT:
         hostStatus = VMK_SCSI_HOST_TIMEOUT;
         deviceStatus = VMK_SCSI_DEVICE_GOOD;
         break;

      case DRIVER_SENSE:
         hostStatus = VMKLNX_SCSI_HOST_STATUS(scmd->result);
         deviceStatus = VMKLNX_SCSI_DEVICE_STATUS(scmd->result);
         VMK_ASSERT((hostStatus == VMK_SCSI_HOST_OK) &&
                    (deviceStatus == VMK_SCSI_DEVICE_CHECK_CONDITION));
         break;

      default:
         VMKLNX_WARN("Unknown driver code: %x",
                     VMKLNX_SCSI_STATUS_NO_SUGGEST(scmd->result));
         VMK_ASSERT(0);
         hostStatus = VMKLNX_SCSI_HOST_STATUS(scmd->result);
         deviceStatus = VMKLNX_SCSI_DEVICE_STATUS(scmd->result);
         break;
      }


#ifdef VMKLNX_TRACK_IOS_DOWN
      sdev = scmd->device
#endif

      vmkCmdPtr = scmd->vmkCmdPtr;
      vmkCmdPtr->bytesXferred = scmd->request_bufflen - scmd->resid;

      num_cmp++;
      if (tls->worldlet) {
         /*
          * If there's a worldlet that's submitting the request, then we'd
          * prefer to affinitize to that worldlet rather than the associated
          * world.  After all, the worldlet has established locality of the
          * data structures and the world may not have.  We also have to ensure
          * that we don't affinitize to ourselves.
          */
         if (vmkCmdPtr->worldletId != VMK_INVALID_WORLDLET_ID &&
             tls->worldletId != vmkCmdPtr->worldletId) {
            vmk_WorldletAffinityTrackerAddWorldletSample(tls->tracker,
                                                         vmkCmdPtr->worldletId,
                                                         1);
         } else {
            vmk_WorldletAffinityTrackerAddWorldSample(tls->tracker,
                                                      vmkCmdPtr->worldId, 1);
         }
      }

      if (unlikely(vmkCmdPtr->bytesXferred > scmd->request_bufflen)) {
         if (likely((hostStatus != VMK_SCSI_HOST_OK) ||
                    (deviceStatus != VMK_SCSI_DEVICE_GOOD))) {
            vmkCmdPtr->bytesXferred = 0;

            VMKLNX_WARN("Error BytesXferred > Requested Length "
	                "Marking transfer length as 0 - vmhba = %s, "
                        "Driver Name = %s, "
	                "Requested length = %d, Resid = %d",
		        vmklnx_get_vmhba_name(scmd->device->host), 
		        scmd->device->host->hostt->name,
                        scmd->request_bufflen, 
		        scmd->resid);
         } else {
            /*
             * We should not reach here
             */
            VMKLNX_ALERT("Error BytesXferred > Requested Length "
                         "but HOST_OK/DEVICE_GOOD!"
                         "vmhba = %s, Driver Name = %s, "
                         "Requested length = %d, Resid = %d\n",
                         vmklnx_get_vmhba_name(scmd->device->host), 
                         scmd->device->host->hostt->name,
                         scmd->request_bufflen, scmd->resid);
            vmkCmdPtr->bytesXferred = 0;
            scmd->result = DID_ERROR << 16;
            hostStatus = VMK_SCSI_HOST_ERROR;
            deviceStatus = VMK_SCSI_DEVICE_GOOD;
         }
      }

      /*
       * Copy the sense buffer whenever there's an error.
       *
       * The buffer should only really be valid when we
       * hit HOST_OK/CHECK_CONDITION but sometimes broken
       * drivers will return valuable debug data anyway.
       * So copy it here so that it can be logged/examined
       * later.
       */
      if (unlikely(!((hostStatus == VMK_SCSI_HOST_OK) &&
                     (deviceStatus == VMK_SCSI_DEVICE_GOOD)))) {
         vmk_ByteCount size;

         if (hostStatus == VMK_SCSI_HOST_ERROR) {
            VMKLNX_DEBUG(0, "Command 0x%x (%p) to \"%s:C%d:T%d:L%d\" failed. "
                         "Driver: \"%s\", H:0x%x D:0x%x",
                         scmd->cmnd[0], scmd,
                         vmklnx_get_vmhba_name(scmd->device->host),
                         scmd->device->channel, scmd->device->id,
                         scmd->device->lun, 
                         scmd->device->host->hostt->name,
                         hostStatus, deviceStatus);
         }
         VMK_ASSERT(deviceStatus != VMK_SCSI_DEVICE_CHECK_CONDITION ||
                    scmd->sense_buffer[0] != 0);
         SCSILinuxATASenseDescriptorToFixed(scmd->sense_buffer);
         size = vmk_ScsiGetSupportedCmdSenseDataSize();
         /* Use the additional sense length field for sense data length */
         size = min(size, (vmk_ByteCount)(scmd->sense_buffer[7] + 8));
         vmk_ScsiCmdSetSenseData((vmk_ScsiSenseData *)scmd->sense_buffer,
                                 scmd->vmkCmdPtr,
                                 min((vmk_ByteCount)SCSI_SENSE_BUFFERSIZE,
                                     size));
      } else if (unlikely(scmd->cmnd[0] == VMK_SCSI_CMD_INQUIRY)) {
	    SCSILinuxProcessStandardInquiryResponse(scmd);
      }

      spin_lock_irqsave(scmd->device->host->host_lock, flags);
      --scmd->device->host->host_busy;
      --scmd->device->device_busy;
      spin_unlock_irqrestore(scmd->device->host->host_lock, flags);
 
      /* wake up host error handler if necessary */
      if (unlikely(scmd->device->host->host_eh_scheduled
                && !scmd->device->host->host_busy)) {
              wake_up_process(scmd->device->host->ehandler);
      }

      /* 
       * Put back the scsi command before calling scsi upper layer
       * Otherwise, if a destroy path is issued by upper layer and
       * it will destroy scsi device which has list_lock required
       * by scsi_put_command. Even worse, list_lock is filled with 0 
       * and scsi_put_command interprets it as locked by cpu 0.
       * This causes cpu locked up.
       */
      scsi_put_command(scmd);

      /*
       * VMKernel does not support DID_BAD_TARGET, so convert it to
       * DID_NO_CONNECT. Do this just prior to returning the command
       * to VMkernel to catch all instances of this.
       */
      if (unlikely(hostStatus == DID_BAD_TARGET)) {
         static vmk_TimerCycles lastLog = 0;

         if (vmk_TimerTCToMS(*now - lastLog) >= 5000) {
            lastLog = *now;
            VMKLNX_WARN("Command 0x%x (%p) to \"%s:C%d:T%d:L%d\" "
                        "failed (looks like driver \"%s\" is broken) "
                        "with DID_BAD_TARGET - converting to DID_NO_CONNECT.",
                        scmd->cmnd[0], scmd,
                        vmklnx_get_vmhba_name(scmd->device->host),
                        scmd->device->channel, scmd->device->id,
                        scmd->device->lun, 
                        scmd->device->host->hostt->name);
            VMK_ASSERT_BUG(VMK_FALSE); /* See PR 503363 */
         }
         hostStatus = DID_NO_CONNECT;
      }
      /*
       * Convert any Linux host statuses that ESX does not support
       * to something that gives an equivalent semantic.
       */
      if (unlikely(hostStatus == DID_BAD_INTR)) {
         hostStatus = DID_IMM_RETRY;
      }
      if (unlikely(hostStatus == DID_PASSTHROUGH)) {
         hostStatus = DID_ERROR;
      }
      if (unlikely(hostStatus == DID_REQUEUE)) {
         hostStatus = DID_IMM_RETRY;
      }

      SCSILinuxCompleteCommand(vmkCmdPtr, hostStatus, deviceStatus);

#ifdef VMKLNX_TRACK_IOS_DOWN
      /*
       * Decrement the ref count on this device now
       */
      put_device(&sdev->sdev_gendev);
#endif

      *now = vmk_GetTimerCycles();
   }

   add_disk_randomness(NULL);
   return num_cmp;
}


/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxQueueCommand --
 *
 *      Queue up a SCSI command.
 *
 * Results:
 *      VMK return status
 *      VMK_OK - cmd queued or will be completed with error
 *      VMK_WOULD_BLOCK - cmd not queued because of QD limit or device quiesse etc 
 *      VMK_NO_MEMORY - cmd not queued because of failed alloc of scmd struct
 *
 * Side effects:
 *      A command is allocated, set up, and queueud.
 *
 *----------------------------------------------------------------------
 */

VMK_ReturnStatus
SCSILinuxQueueCommand(struct Scsi_Host *shost,
                      struct scsi_device *sdev, 
                      vmk_ScsiCommand *vmkCmdPtr)
{
   struct scsi_cmnd *scmd;
   int status = 0;
   vmk_ScsiAdapter *vmkAdapter;
   unsigned long flags;
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter;
   VMK_ReturnStatus vmkStatus;
   void* doneFn = NULL;
   vmk_ModuleID mid;
   vmk_Worldlet wdt;
   scsiLinuxTLS_t *curr = SCSILinuxTLSGetActive();

   vmklnx26ScsiAdapter = (struct vmklnx_ScsiAdapter *) shost->adapter; 
   vmkAdapter = vmklnx26ScsiAdapter->vmkAdapter;

   /* Never issue a command to the host ID */
   VMK_ASSERT(sdev);
   VMK_ASSERT(sdev->id != shost->this_id);

   if (curr == NULL && vmk_WorldletGetCurrent(&wdt, NULL) == VMK_OK) {
      vmk_WorldletGetId(wdt, &vmkCmdPtr->worldletId);
   }

   /*
    * The completion function is called through a pointer.
    * So we have to set up a trampoline at runtime.
    */

   mid = SCSI_GET_MODULE_ID(shost); 
   doneFn = SCSILinuxCmdDone;

   scmd = scsi_get_command(sdev, GFP_ATOMIC);

   if (unlikely(scmd == NULL && (vmkCmdPtr->flags & VMK_SCSI_COMMAND_FLAGS_SWAP_IO))) {
      scmd = vmklnx_scsi_get_command_urgent(sdev);
   }

   if (unlikely(scmd == NULL)) {
      /*
       * If ths scsi host is being deleted, the scmd structure could not be
       * allocated because we were unable to get a reference for the scsi host.
       * In that case, we must complete the command with a status that informs
       * the upper layers that the adapter is going away so that it can
       * failover to a different path.
       */
      if ((shost->shost_state == SHOST_DEL) ||
          (shost->shost_state == SHOST_CANCEL)) {
         vmkCmdPtr->status.host = VMK_SCSI_HOST_NO_CONNECT;
         vmkCmdPtr->status.device = VMK_SCSI_DEVICE_GOOD;
         vmk_ScsiSchedCommandCompletion(vmkCmdPtr);
         return VMK_OK;
      } else {
         VMKLNX_DEBUG(0, "Insufficient memory");
         return VMK_NO_MEMORY; /* VMK_WOULD_BLOCK ? */
      }
   }

   vmkStatus = SCSILinuxComputeSGArray(scmd, vmkCmdPtr);
   if (vmkStatus == VMK_NO_MEMORY) {
      scsi_put_command(scmd);
      VMKLNX_DEBUG(0, "SCSILinuxComputeSGArray failed");
      return VMK_NO_MEMORY; 	
   }

#ifdef VMKLNX_TRACK_IOS_DOWN
   if (!get_device(&sdev->sdev_gendev)) {
      scsi_put_command(scmd);
      VMKLNX_DEBUG(0, "Getting ref count on sdev failed");
      return VMK_NO_MEMORY; 	
   }
#endif

   VMK_ASSERT_ON_COMPILE(MAX_COMMAND_SIZE >= VMK_SCSI_MAX_CDB_LEN);

   /*
    * Get the serial number and update tag info etc 
    */
   SCSILinuxInitScmd(sdev, scmd);

   if (likely(scmd->device->tagged_supported)) {
      if (unlikely(vmkCmdPtr->flags & VMK_SCSI_COMMAND_FLAGS_ISSUE_WITH_ORDERED_TAG)) {
         scmd->tag = ORDERED_QUEUE_TAG;
      } else if (unlikely(vmkCmdPtr->flags & VMK_SCSI_COMMAND_FLAGS_ISSUE_WITH_HEAD_OF_Q_TAG)) {
         scmd->tag = HEAD_OF_QUEUE_TAG;
      } else {
         scmd->tag = SIMPLE_QUEUE_TAG;
      }
   } else {
      scmd->tag = 0;
   }

   memcpy(scmd->cmnd, vmkCmdPtr->cdb, vmkCmdPtr->cdbLen);
   scmd->cmd_len = vmkCmdPtr->cdbLen;
   /*
    * As per SPC3 section 4.3.4.1, SCSI commands in group 3 are reserved,
    * and group 6 and 7 are vendor-specific and so we can't guess about
    * command length of those and so we trust the command length supplied
    * in cmd_len for those commands.
    * Note: Variable-length commands have opcode 0x7f which falls under
    * group 3 and COMMAND_SIZE currently do not support variable-length
    * commands and so just trust cmd_len for those commands too!
    */
   if (likely(SCSI_CMD_GROUP_CODE(scmd->cmnd[0]) < 6) &&
       likely(SCSI_CMD_GROUP_CODE(scmd->cmnd[0]) != 3) &&
       unlikely(scmd->cmd_len != COMMAND_SIZE(scmd->cmnd[0]))) {
      static int thrtl_cntr = 0;

      VMKLNX_THROTTLED_INFO(thrtl_cntr,
                            "Trusting cmd_len %d in cmd 0x%02x (expected:%d)\n",
                            scmd->cmd_len, scmd->cmnd[0],
                            COMMAND_SIZE(scmd->cmnd[0]));
   }
   
   scmd->vmkCmdPtr = vmkCmdPtr;
   scmd->underflow = vmkCmdPtr->requiredDataLen;

   /*
    * Set the data direction on every command, so the residue dir is
    * not re-used.
    */
   scmd->sc_data_direction = SCSILinuxGetDataDirection(scmd,
                                                      vmkCmdPtr->dataDirection);

   /*
    * Verify that none of the vmware supported drivers are using these
    * fields. If so, we need to set them appropriately.
    * 
    * Note that the SCp and host_scribble fields can be used by the
    * driver without midlayer interference.
    */
   VMK_ASSERT(scmd->transfersize == 0);
   VMK_ASSERT(scmd->sglist_len == 0);

   VMK_DEBUG_ONLY(
      if (vmk_ScsiDebugDropCommand(vmkAdapter, vmkCmdPtr)) {
      VMKLNX_WARN("Dropping command: SN %#"VMK_FMT64"x, initiator %p",
                  vmkCmdPtr->cmdId.serialNumber,
                  vmkCmdPtr->cmdId.initiator);
      vmk_LogBacktraceMessage();
      spin_lock_irqsave(&scmd->vmklock, flags);
      scmd->vmkflags |= (VMK_FLAGS_DROP_CMD|VMK_FLAGS_NEED_CMDDONE);
      spin_unlock_irqrestore(&scmd->vmklock, flags);
      spin_lock_irqsave(shost->host_lock, flags);
      ++shost->host_busy;
      ++sdev->device_busy;
      spin_unlock_irqrestore(shost->host_lock, flags);
      return VMK_OK;
   }
   )

   if (vmkAdapter->constraints.sgMaxEntries) {
      VMK_ASSERT(scmd->use_sg <= vmkAdapter->constraints.sgMaxEntries);
   }

   if (unlikely(scmd->cmd_len > shost->max_cmd_len)) {
      scmd->result = (DID_OK << 16) | SAM_STAT_CHECK_CONDITION;
      scmd->resid = scmd->request_bufflen; /* no data returned */
      scsi_build_sense_buffer(0, scmd->sense_buffer, ILLEGAL_REQUEST, 0x20, 0x0);
      if (!(vmkCmdPtr->flags & VMK_SCSI_COMMAND_FLAGS_PROBE_FOR_SUPPORT)) {
         VMKLNX_DEBUG(0, "cmd_len %d, max_cmd_len %d, cmd %x not supported by driver %s",
                      scmd->cmd_len, shost->max_cmd_len, scmd->cmnd[0], shost->hostt->name);
      }
      spin_lock_irqsave(shost->host_lock, flags);
      ++shost->host_busy;
      ++sdev->device_busy;
      scmd->vmkflags |= VMK_FLAGS_NEED_CMDDONE;
      spin_unlock_irqrestore(shost->host_lock, flags);
      SCSILinuxCmdDone(scmd);  
      return VMK_OK;
   }

   spin_lock_irqsave(shost->host_lock, flags);

   if (unlikely((shost->shost_state == SHOST_CANCEL) ||
                (shost->shost_state == SHOST_DEL))) {
      scmd->result = (DID_NO_CONNECT << 16)|SAM_STAT_GOOD;
      ++shost->host_busy;
      ++sdev->device_busy;
      scmd->vmkflags |= VMK_FLAGS_NEED_CMDDONE;
      spin_unlock_irqrestore(shost->host_lock, flags);
      SCSILinuxCmdDone(scmd);
      return VMK_OK;
   }

   /* linux tests all these states without holding the host_lock */
   if (unlikely((shost->host_self_blocked) || 
                (sdev->sdev_state == SDEV_BLOCK) ||
                (sdev->sdev_state == SDEV_QUIESCE) || 
                (shost->shost_state == SHOST_RECOVERY) ||
                (sdev->device_busy >= sdev->queue_depth) ||
                (shost->host_busy >= shost->can_queue))) {
      spin_unlock_irqrestore(shost->host_lock, flags);
#ifdef VMKLNX_TRACK_IOS_DOWN
      put_device(&sdev->sdev_gendev);
#endif
      scsi_put_command(scmd);
      VMKLNX_DEBUG(0,
                   "h: sb=%d, b=%d; d: (%d:%d:%d:%d) s=%d, b=%d, cq=%d, qd=%d",
                   shost->host_self_blocked,
                   shost->host_busy,
                   shost->host_no, sdev->channel, sdev->id, sdev->lun,
                   sdev->sdev_state,
                   sdev->device_busy,
                   shost->can_queue,
                   sdev->queue_depth);
      return VMK_WOULD_BLOCK;
   }

   scmd->vmkflags |= VMK_FLAGS_NEED_CMDDONE;
   ++shost->host_busy;
   ++sdev->device_busy;

   if (unlikely((sdev->sdev_state == SDEV_DEL) ||
                (sdev->sdev_state == SDEV_OFFLINE))) { 
      scmd->result = (DID_NO_CONNECT << 16)|SAM_STAT_GOOD;
      spin_unlock_irqrestore(shost->host_lock, flags);
      VMKLNX_DEBUG(2, " - The device is up for delete");
      SCSILinuxCmdDone(scmd);  
      return VMK_OK;
   }

   if ((shost->xportFlags == VMKLNX_SCSI_TRANSPORT_TYPE_FC) &&
      (vmkAdapter->mgmtAdapter.t.fc->ioTimeout) &&
      (shost->hostt->eh_abort_handler)) {
      scmd->vmkflags |= VMK_FLAGS_IO_TIMEOUT;
      scsi_add_timer(scmd, vmkAdapter->mgmtAdapter.t.fc->ioTimeout, 
         SCSILinuxCmdTimedOut);
   }

   /*
    * Linux is weird and holds the host_lock while calling
    * the drivers queuecommand entrypoint.
    */
   VMKAPI_MODULE_CALL(mid,
                      status,
                      shost->hostt->queuecommand,
                      scmd,
                      doneFn);
   spin_unlock_irqrestore(shost->host_lock, flags);

   if (unlikely(status)) {
      static atomic_t repeat_cnt = ATOMIC_INIT(0);
      static atomic64_t throttle_to = ATOMIC64_INIT(0);
      uint32_t cnt;
      unsigned long tto = 0;

      if (scmd->vmkflags & VMK_FLAGS_IO_TIMEOUT)
         scsi_delete_timer(scmd);

      scmd->result = (DID_OK << 16)|SAM_STAT_BUSY;

      /*
       * In order to limit the # of error messages printed, the below
       * heuristic will print it every THROTTLE_TO secs. Refer to PR 359133.
       * When the window to print opens up, there is a chance that multiple
       * messages can get printed. Also, when multiple messages get printed,
       * there is a chance that the repeat count will get printed out of order.
       * For example, if the repeat_cnt was 80 and then went to 1 (after a reset
       * to 0), it could print 1 and then 80. But these are not too bad and
       * so no logic is in place to prevent that.
       *
       * Maybe this can be expanded someday to make this per shost. For now,
       * lets keep it simple.
       */
      atomic_inc(&repeat_cnt);
      tto = atomic64_read(&throttle_to);

      if ((long)(jiffies - tto) >= 0) {
         tto = jiffies + THROTTLE_TO * HZ;
         atomic_set(&throttle_to, tto);
         cnt = atomic_xchg(&repeat_cnt, 0); // cnt will have old repeat_cnt val

         if (likely(cnt > 0)) {
            VMKLNX_WARN("queuecommand failed with status = 0x%x %s "
                        "%s:%d:%d:%d "
                        "(driver name: %s) - Message repeated %d time%s",
                        status,
                        (status == SCSI_MLQUEUE_HOST_BUSY) ? "Host Busy" :
                        vmk_StatusToString(status),
                        SCSI_GET_NAME(shost), sdev->channel, sdev->id,
                        sdev->lun,
                        shost->hostt->name ? shost->hostt->name : "NULL", cnt,
                        (cnt == 1) ? "" : "s");
         }
      }

      /* 
       * We really should not need to check NEED_CMDDONE here,
       * if it is not set, it is a driver bug. We don't need to
       * hold the vmk_lock because nobody else should be accessing
       * the scmd.
       */
      if (scmd->vmkflags & VMK_FLAGS_NEED_CMDDONE) {
         SCSILinuxCmdDone(scmd);
      } else {
         VMKLNX_WARN("scsi_done called when queuecommand failed!");
      }
   }
   return VMK_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxComputeSGArray --
 *
 *      Compute the scatter-gather array for the vmk command
 *
 * Results:
 *      VMK return status
 *
 * Side effects:
 *      A scatter gather array is allocated and initialized
 *
 *----------------------------------------------------------------------
 */

static VMK_ReturnStatus
SCSILinuxComputeSGArray(struct scsi_cmnd *scmd, vmk_ScsiCommand *vmkCmdPtr)
{
   int i, sgArrLen;
   vmk_SgArray *sgArray = vmkCmdPtr->sgArray;
   vmk_SgArray *sgIOArray = vmkCmdPtr->sgIOArray;
   vmk_ScsiAdapter *vmkAdapter;
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter;
   struct scatterlist *sg = NULL;

   VMK_ASSERT(scmd->device);

   vmklnx26ScsiAdapter = 
	(struct vmklnx_ScsiAdapter *) scmd->device->host->adapter; 
   vmkAdapter = vmklnx26ScsiAdapter->vmkAdapter;

   sgArrLen = sgArray ? sgArray->numElems : 0;

   if (unlikely(sgArrLen == 0)) {
      /* 
       * There can be few commands without any SG. E.g TUR
       */
      VMKLNX_DEBUG(5, "Sglen is zero for Cmd 0x%x "
                      "(sgArray:%p)", vmkCmdPtr->cdb[0],
                      (sgArray ? sgArray : NULL));
      scmd->request_buffer = NULL; 
      scmd->request_bufferMA = 0; 
      scmd->use_sg = 0;
      return VMK_OK;
   }

   for (i = 0; i < sgArrLen; i++) {
      scmd->request_bufflen += sgArray->elem[i].length;
      vmk_MAAssertIOAbility(sgArray->elem[i].addr, sgArray->elem[i].length);
   }
   sg = scmd->vmksg;
   VMKLNX_INIT_VMK_SG_WITH_ARRAYS(sg, sgArray, sgIOArray);

   /* Always use SG since the IO addresses are already mapped */
   scmd->use_sg = sgArrLen;
   scmd->request_buffer = sg;
   scmd->request_bufferMA = 0;

   return VMK_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxGetDataDirection --
 *
 *      This is a general purpose routine that will return the data
 *      direction for a SCSI command based on the actual command.
 *      Guests can send any SCSI command, so this routine must handle
 *      *all* scsi commands correctly.
 *
 *       This code is compliant with SPC-2 (ANSI X3.351:200x)
 *       - Except we do not support 16 byte CDBs
 *
 * Results:
 *      sc_data_direction value
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */
int
SCSILinuxGetDataDirection(struct scsi_cmnd * cmdPtr, 
			  vmk_ScsiCommandDirection guestDataDirection)
{
   unsigned char cmd = cmdPtr->cmnd[0];

   /*
    * Upper layers (vscsi, buslogic emulation) resets transfer direction of
    * commands to READ/WRITE even for commands that don't do data transfer,
    * so return DMA_NONE for commands with zero transfer length. PR# 449751
    */
   if (!cmdPtr->request_bufflen) {
      return DMA_NONE;
   }

  /*
   * First check if the caller has specified the data direction. If the 
   * direction is set to READ, WRITE or NONE, we will just go by it. 
   * If the direction is set to UNKNOWN, then we will try to find out
   * the direction to the best of our knowledge. This approach will help us
   * support vendor specific commands without modifying the vmkernel.
   */
   if (likely(guestDataDirection != VMK_SCSI_COMMAND_DIRECTION_UNKNOWN)) {
      if (likely(guestDataDirection == VMK_SCSI_COMMAND_DIRECTION_READ)) {
         return DMA_FROM_DEVICE;
      } else if (likely(guestDataDirection == 
      					VMK_SCSI_COMMAND_DIRECTION_WRITE)) {
         return DMA_TO_DEVICE;
      } else {
         return DMA_NONE;
      }
   }

   switch (cmd) {
   case VMK_SCSI_CMD_FORMAT_UNIT	       :  //   0x04	//
   case VMK_SCSI_CMD_REASSIGN_BLOCKS       :  //   0x07	// 
 //case VMK_SCSI_CMD_INIT_ELEMENT_STATUS   :  //   0x07	// Media changer
   case VMK_SCSI_CMD_WRITE6	               :  //   0x0a	// write w/ limited addressing
 //case VMK_SCSI_CMD_PRINT	               :  //   0x0a	// print data
 //case VMK_SCSI_CMD_SEEK6	               :  //   0x0b	// seek to LBN
   case VMK_SCSI_CMD_SLEW_AND_PRINT	       :  //   0x0b	// advance and print
   case VMK_SCSI_CMD_MODE_SELECT	       :  //   0x15	// set device parameters
   case VMK_SCSI_CMD_COPY	               :  //   0x18	// autonomous copy from/to another device
   case VMK_SCSI_CMD_SEND_DIAGNOSTIC       :  //   0x1d	// initiate self-test
   case VMK_SCSI_CMD_SET_WINDOW	       :  //   0x24	// set scanning window
   case VMK_SCSI_CMD_WRITE10               :  //   0x2a	// write
   case VMK_SCSI_CMD_WRITE_VERIFY	       :  //   0x2e	// write w/ verify of success
   case VMK_SCSI_CMD_SEARCH_DATA_HIGH      :  //   0x30	// search for data pattern
   case VMK_SCSI_CMD_SEARCH_DATA_EQUAL     :  //   0x31	// search for data pattern
   case VMK_SCSI_CMD_SEARCH_DATA_LOW       :  //   0x32	// search for data pattern
   case VMK_SCSI_CMD_MEDIUM_SCAN	       :  //   0x38	// search for free area
   case VMK_SCSI_CMD_COMPARE               :  //   0x39	// compare data
   case VMK_SCSI_CMD_COPY_VERIFY	       :  //   0x3a	// autonomous copy w/ verify
   case VMK_SCSI_CMD_WRITE_BUFFER	       :  //   0x3b	// write data buffer
   case VMK_SCSI_CMD_UPDATE_BLOCK	       :  //   0x3d	// substitute block with an updated one
   case VMK_SCSI_CMD_WRITE_LONG            :  //   0x3f	// write data and ECC
   case VMK_SCSI_CMD_CHANGE_DEF 	       :  //   0x40	// set SCSI version
   case VMK_SCSI_CMD_WRITE_SAME            :  //   0x41	// 
   case VMK_SCSI_CMD_LOG_SELECT            :  //   0x4c	// select statistics
   case VMK_SCSI_CMD_MODE_SELECT10	       :  //   0x55	// set device parameters
   case VMK_SCSI_CMD_RESERVE_UNIT10        :  //   0x56	//
   case VMK_SCSI_CMD_SEND_CUE_SHEET        :  //   0x5d	// (CDR Related?)
   case VMK_SCSI_CMD_PERSISTENT_RESERVE_OUT:  //   0x5f	//
   case VMK_SCSI_CMD_WRITE12               :  //   0xaa	// write data
   case VMK_SCSI_CMD_WRITE_VERIFY12	       :  //   0xae	// write logical block, verify success
   case VMK_SCSI_CMD_SEARCH_DATA_HIGH12    :  //   0xb0	// search data pattern
   case VMK_SCSI_CMD_SEARCH_DATA_EQUAL12   :  //   0xb1	// search data pattern
   case VMK_SCSI_CMD_SEARCH_DATA_LOW12     :  //   0xb2	// search data pattern
   case VMK_SCSI_CMD_SEND_VOLUME_TAG       :  //   0xb6	//
 //case VMK_SCSI_CMD_SET_STREAMING         :  //   0xb6	// For avoiding over/underrun
   case VMK_SCSI_CMD_SEND_DVD_STRUCTURE    :  //   0xbf	// burning DVDs?
        return DMA_TO_DEVICE; 

   case VMK_SCSI_CMD_REQUEST_SENSE	       :  //   0x03	// return detailed error information
   case VMK_SCSI_CMD_READ_BLOCKLIMITS      :  //   0x05	//
   case VMK_SCSI_CMD_READ6                 :  //   0x08	// read w/ limited addressing
   case VMK_SCSI_CMD_READ_REVERSE	       :  //   0x0f	// read backwards
   case VMK_SCSI_CMD_INQUIRY               :  //   0x12	// return LUN-specific information
   case VMK_SCSI_CMD_RECOVER_BUFFERED      :  //   0x14	// recover buffered data
   case VMK_SCSI_CMD_MODE_SENSE            :  //   0x1a	// read device parameters
   case VMK_SCSI_CMD_RECV_DIAGNOSTIC       :  //   0x1c	// read self-test results
   case VMK_SCSI_CMD_READ_CAPACITY	       :  //   0x25	// read number of logical blocks
 //case VMK_SCSI_CMD_GET_WINDOW            :  //   0x25	// get scanning window
   case VMK_SCSI_CMD_READ10	               :  //   0x28	// read
   case VMK_SCSI_CMD_READ_GENERATION       :  //   0x29	// read max generation address of LBN
   case VMK_SCSI_CMD_READ_UPDATED_BLOCK    :  //   0x2d	// read specific version of changed block
   case VMK_SCSI_CMD_PREFETCH              :  //   0x34	// read data into buffer
 //case VMK_SCSI_CMD_READ_POSITION	       :  //   0x34	// read current tape position
   case VMK_SCSI_CMD_READ_DEFECT_DATA      :  //   0x37	// 
   case VMK_SCSI_CMD_READ_BUFFER	       :  //   0x3c	// read data buffer
   case VMK_SCSI_CMD_READ_LONG             :  //   0x3e	// read data and ECC
   case VMK_SCSI_CMD_READ_SUBCHANNEL       :  //   0x42	// read subchannel data and status
   case VMK_SCSI_CMD_GET_CONFIGURATION     :  //   0x46	// get configuration (SCSI-3)
   case VMK_SCSI_CMD_READ_TOC              :  //   0x43	// read contents table
   case VMK_SCSI_CMD_READ_HEADER	       :  //   0x44	// read LBN header
   case VMK_SCSI_CMD_GET_EVENT_STATUS_NOTIFICATION :  //   0x4a
   case VMK_SCSI_CMD_LOG_SENSE             :  //   0x4d	// read statistics
   case VMK_SCSI_CMD_READ_DISC_INFO        :  //   0x51	// info on CDRs
   case VMK_SCSI_CMD_READ_TRACK_INFO       :  //   0x52	// track info on CDRs (also xdread in SBC-2)
   case VMK_SCSI_CMD_MODE_SENSE10	       :  //   0x5a	// read device parameters
   case VMK_SCSI_CMD_READ_BUFFER_CAPACITY  :  //   0x5c	// CDR burning info.
   case VMK_SCSI_CMD_PERSISTENT_RESERVE_IN :  //   0x5e	//
   case VMK_SCSI_CMD_SERVICE_ACTION_IN     :  //   0x9e 
   case VMK_SCSI_CMD_REPORT_LUNS           :  //   0xa0	// 
   case VMK_SCSI_CMD_READ12	               :  //   0xa8	// read (SCSI-3)
   case VMK_SCSI_CMD_READ_DVD_STRUCTURE    :  //   0xad	// read DVD structure (SCSI-3)
   case VMK_SCSI_CMD_READ_DEFECT_DATA12    :  //   0xb7	// read defect data information
   case VMK_SCSI_CMD_READ_ELEMENT_STATUS   :  //   0xb8	// read element status
 //case VMK_SCSI_CMD_SELECT_CDROM_SPEED    :  //   0xb8	// set data rate
   case VMK_SCSI_CMD_READ_CD_MSF	       :  //   0xb9	// read CD information (all formats, MSF addresses)
   case VMK_SCSI_CMD_SEND_CDROM_XA_DATA    :  //   0xbc
 //case VMK_SCSI_CMD_PLAY_CD               :  //   0xbc
   case VMK_SCSI_CMD_MECH_STATUS           :  //   0xbd
   case VMK_SCSI_CMD_READ_CD               :  //   0xbe	// read CD information (all formats, MSF addresses)
        return DMA_FROM_DEVICE; 

   case VMK_SCSI_CMD_TEST_UNIT_READY       :  //   0x00	// test if LUN ready to accept a command
   case VMK_SCSI_CMD_REZERO_UNIT	       :  //   0x01	// seek to track 0
   case VMK_SCSI_CMD_RESERVE_UNIT	       :  //   0x16	// make LUN accessible only to certain initiators
   case VMK_SCSI_CMD_RELEASE_UNIT	       :  //   0x17	// make LUN accessible to other initiators
   case VMK_SCSI_CMD_ERASE                 :  //   0x19	// 
   case VMK_SCSI_CMD_START_UNIT            :  //   0x1b	// load/unload medium
 //case VMK_SCSI_CMD_SCAN                  :  //   0x1b	// perform scan
 //case VMK_SCSI_CMD_STOP_PRINT            :  //   0x1b	// interrupt printing
   case VMK_SCSI_CMD_MEDIUM_REMOVAL	       :  //   0x1e	// lock/unlock door
   case VMK_SCSI_CMD_SEEK10                :  //   0x2b	// seek LBN
 //case VMK_SCSI_CMD_POSITION_TO_ELEMENT   :  //   0x2b	// media changer
   case VMK_SCSI_CMD_VERIFY                :  //   0x2f	// verify success
   case VMK_SCSI_CMD_SET_LIMITS            :  //   0x33	// define logical block boundaries
   case VMK_SCSI_CMD_SYNC_CACHE            :  //   0x35	// re-read data into buffer
   case VMK_SCSI_CMD_LOCKUNLOCK_CACHE      :  //   0x36	// lock/unlock data in cache
   case VMK_SCSI_CMD_PLAY_AUDIO10	       :  //   0x45	// audio playback
   case VMK_SCSI_CMD_PLAY_AUDIO_MSF	       :  //   0x47	// audio playback starting at MSF address
   case VMK_SCSI_CMD_PLAY_AUDIO_TRACK      :  //   0x48	// audio playback starting at track/index
   case VMK_SCSI_CMD_PLAY_AUDIO_RELATIVE   :  //   0x49	// audio playback starting at relative track
   case VMK_SCSI_CMD_PAUSE                 :  //   0x4b	// audio playback pause/resume
   case VMK_SCSI_CMD_STOP_PLAY             :  //   0x4e	// audio playback stop
   case VMK_SCSI_CMD_RESERVE_TRACK         :  //   0x53	// leave space for data on CDRs
   case VMK_SCSI_CMD_RELEASE_UNIT10        :  //   0x57	//
   case VMK_SCSI_CMD_CLOSE_SESSION         :  //   0x5b	// close area/sesssion (recordable)
   case VMK_SCSI_CMD_BLANK                 :  //   0xa1	// erase RW media
   case VMK_SCSI_CMD_MOVE_MEDIUM	       :  //   0xa5	// 
 //case VMK_SCSI_CMD_PLAY_AUDIO12	       :  //   0xa5	// audio playback
   case VMK_SCSI_CMD_EXCHANGE_MEDIUM       :  //   0xa6	//
 //case VMK_SCSI_CMD_LOADCD                :  //   0xa6	//
   case VMK_SCSI_CMD_PLAY_TRACK_RELATIVE   :  //   0xa9	// audio playback starting at relative track
   case VMK_SCSI_CMD_ERASE12               :  //   0xac	// erase logical block
   case VMK_SCSI_CMD_VERIFY12              :  //   0xaf	// verify data
   case VMK_SCSI_CMD_SET_LIMITS12	       :  //   0xb3	// set block limits
   case VMK_SCSI_CMD_REQUEST_VOLUME_ELEMENT_ADDR :  //   0xb5 //
   case VMK_SCSI_CMD_AUDIO_SCAN            :  //   0xba	// fast audio playback
   case VMK_SCSI_CMD_SET_CDROM_SPEED       :  //   0xbb // (proposed)
        return DMA_NONE; 

   /*-
	the scsi committee chose to make certain optical/dvd commands
	move data in directions opposite to the spc-3 maintenance commands...
    */
   case VMK_SCSI_CMD_MAINTENANCE_OUT       :  //   0xa4	// maintenance cmds
// case VMK_SCSI_CMD_REPORT_KEY            :  //   0xa4     // report key SCSI-3
        switch (cmdPtr->device->type) {
	case VMK_SCSI_CLASS_OPTICAL:
	case VMK_SCSI_CLASS_CDROM:
	case VMK_SCSI_CLASS_WORM:
		/* REPORT_KEY */
		return DMA_FROM_DEVICE;
	}
        return DMA_TO_DEVICE; 

   case VMK_SCSI_CMD_MAINTENANCE_IN        :  //   0xa3	// maintenance cmds
// case VMK_SCSI_CMD_SEND_KEY              :  //   0xa3     // send key SCSI-3
        switch (cmdPtr->device->type) {
	case VMK_SCSI_CLASS_OPTICAL:
	case VMK_SCSI_CLASS_CDROM:
	case VMK_SCSI_CLASS_WORM:
		/* SEND_KEY */
		return DMA_TO_DEVICE;
	}
        return DMA_FROM_DEVICE; 
        
   /*
    *   Vendor Specific codes defined in SPC-2
    */
   case 0x02: case 0x06: case 0x09: case 0x0c: case 0x0d: case 0x0e: case 0x13:
   case 0x20: case 0x21: case 0x23: case 0x27:
        return DMA_FROM_DEVICE; 

// Codes defined in the Emulex driver; used by FSC.
#define MDACIOCTL_DIRECT_CMD                  0x22
#define MDACIOCTL_STOREIMAGE                  0x2C
#define MDACIOCTL_WRITESIGNATURE              0xA6
#define MDACIOCTL_SETREALTIMECLOCK            0xAC
#define MDACIOCTL_PASS_THRU_CDB               0xAD
#define MDACIOCTL_PASS_THRU_INITIATE          0xAE
#define MDACIOCTL_CREATENEWCONF               0xC0
#define MDACIOCTL_ADDNEWCONF                  0xC4
#define MDACIOCTL_MORE                        0xC6
#define MDACIOCTL_SETPHYSDEVPARAMETER         0xC8
#define MDACIOCTL_SETLOGDEVPARAMETER          0xCF
#define MDACIOCTL_SETCONTROLLERPARAMETER      0xD1
#define MDACIOCTL_WRITESANMAP                 0xD4
#define MDACIOCTL_SETMACADDRESS               0xD5
   case MDACIOCTL_DIRECT_CMD: 
   {
            switch (cmdPtr->cmnd[2]) {
            case MDACIOCTL_STOREIMAGE:
            case MDACIOCTL_WRITESIGNATURE:
            case MDACIOCTL_SETREALTIMECLOCK:
            case MDACIOCTL_PASS_THRU_CDB:
            case MDACIOCTL_CREATENEWCONF:
            case MDACIOCTL_ADDNEWCONF:
            case MDACIOCTL_MORE:
            case MDACIOCTL_SETPHYSDEVPARAMETER:
            case MDACIOCTL_SETLOGDEVPARAMETER:
            case MDACIOCTL_SETCONTROLLERPARAMETER:
            case MDACIOCTL_WRITESANMAP:
            case MDACIOCTL_SETMACADDRESS:
                  return DMA_TO_DEVICE;
            case MDACIOCTL_PASS_THRU_INITIATE:
                  if (cmdPtr->cmnd[3] & 0x80) {
                           return DMA_TO_DEVICE;
                  } else {
                           return DMA_FROM_DEVICE;
                  }
            default:
                  return DMA_FROM_DEVICE;
            }
   }

   /*
    *   Additional codes defined in SPC-2
    *   Can not support the 16 byte cdb commands, so only list others here
    */
   case 0x50                           :  //   0x50 // xdwrite 10
   case 0x54                           :  //   0x54 // send opc information
        return DMA_TO_DEVICE;

   case 0x59                           :  //   0x59 // read master cue
   case 0xb4                           :  //   0xb4 // read element status attached 12
        return DMA_FROM_DEVICE; 

   case 0x58                           :  //   0x58 // repair track
        return DMA_NONE; 

   case 0xee                           :  //   0xee // EMC specific
         VMKLNX_DEBUG(5,
                      "SCSILinuxGetDataDirection: EMC opcode: %x,%x,%x,%x,%x,%x [sn=%d]",
                      cmdPtr->cmnd[0],
                      cmdPtr->cmnd[1],
                      cmdPtr->cmnd[2],
                      cmdPtr->cmnd[3],
                      cmdPtr->cmnd[4],
                      cmdPtr->cmnd[5],
                      (int)cmdPtr->serial_number);
        return DMA_TO_DEVICE; 
  case 0xef                           :  //   0xef // EMC specific
        VMKLNX_DEBUG(5,
                      "SCSILinuxGetDataDirection: EMC opcode: %x,%x,%x,%x,%x,%x [sn=%d]",
                     cmdPtr->cmnd[0],
                     cmdPtr->cmnd[1],
                     cmdPtr->cmnd[2],
                     cmdPtr->cmnd[3],
                     cmdPtr->cmnd[4],
                     cmdPtr->cmnd[5],
                     (int)cmdPtr->serial_number);
        return DMA_FROM_DEVICE; 

   case 0xff                          : // Intel RAID cache service
        return DMA_BIDIRECTIONAL;

    /*
     * Vendor unique codes used by NEC (See PR 58167) that need a direction set
     * in the command. Some of these are standard SCSI commands that normally 
     * would not have a direction set (DATA_NONE) or are not defined (default).
     *
     * Commands 0x27, 0x2d, 0x3b, and 0x3c are handled earlier in the switch 
     * statement because they are standard SCSI-3 commands.
     * 
     * The special direction cases that are left are coded here.
     * Code	Name			Direction	Special
     * 0x10	READ_SUBSYSTEM_INFO	Read		Yes
     * 0x11	READ_SOLUTION_INFO	Read		Yes
     * 0x26	CHECKSUM_SELECT		Write		Yes
     * 0x27	CHECKSUM_SENSE		Read
     * 0x2c	DDRF_RDRF_SELECT	Write		Yes
     * 0x2d	DDRF_RDRF_SENSE		Read
     * 0x3b	WRITE_BUFFER		Write
     * 0x3c	READ_BUFFER		Read
     * 0xe6	FORCE_RESERVE		DataNone	Yes
     */
   case 0x26                           :  //   0x26 // Vendor Unique Command
   case 0x2c                           :  //   0x2c // erase logical block 10
	if (cmdPtr->request_bufflen)
        	return DMA_TO_DEVICE;   //   If any data, NEC specific command
	else				  //    otherwise, just set no data like normal command
        	return DMA_NONE; 

   case VMK_SCSI_CMD_WRITE_FILEMARKS       :  //   0x10 // 
 //case VMK_SCSI_CMD_SYNC_BUFFER	       :  //   0x10 // print contents of buffer
   case VMK_SCSI_CMD_SPACE                 :  //   0x11 // NEC does a data read with this
	if (cmdPtr->request_bufflen)
        	return DMA_FROM_DEVICE;    //   If any data, NEC specific command
					  //    otherwise, just set no data like normal command
   case 0xe6                           :  //   0xe6 // NEC specific
        return DMA_NONE; 
    
    /*
     * Vendor unique codes used by Fujitsu (See PR 58370)
     */
   case 0xe9                           :  //   0xe9 // Fujitsu specific
        return DMA_TO_DEVICE;
   case 0xeb                           :  //   0xeb // Fujitsu specific
        return DMA_FROM_DEVICE; 

    /*
     *  All 0x8x and 0x9x commands are 16 byte cdbs, these should come here
     */
   case 0x80                           :  //   0x80 // xdwrite extended 16
 //case 0x80                           :  //   0x80 // write filemarks 16
   case 0x81                           :  //   0x81 // rebuild 16 (disk-write)
 //case 0x81                           :  //   0x81 // read reverse 16 (tape-read)
   case 0x82                           :  //   0x82 // regenerate
   case 0x83                           :  //   0x83 // extended copy
   case 0x84                           :  //   0x84 // receive copy results
   case 0x86                           :  //   0x86 // access control in (SPC-3)
   case 0x87                           :  //   0x87 // access control out (SPC-3)
   case 0x88                           :  //   0x88 // read 16
   case 0x89                           :  //   0x89 // device locks (SPC-3)
   case 0x8a                           :  //   0x8a // write 16
   case 0x8c                           :  //   0x8c // read attributes (SPC-3)
   case 0x8d                           :  //   0x8d // write attributes (SPC-3)
   case 0x8e                           :  //   0x8e // write and verify 16
   case 0x8f                           :  //   0x8f // verify 16
   case 0x90                           :  //   0x90 // pre-fetch 16
 //case 0x91                           :  //   0x91 // synchronize cache 16
   case 0x91                           :  //   0x91 // space 16
 //case 0x92                           :  //   0x92 // lock unlock cache 16
   case 0x92                           :  //   0x92 // locate 16
 //case 0x93                           :  //   0x93 // write same 16
   case 0x93                           :  //   0x93 // erase 16
   case 0xa2                           :  //   0xa2 // send event
   case 0xa7                           :  //   0xa7 // move medium attached
 //case 0xa7                           :  //   0xa7 // set read ahead
   default:
        /*
         *  Should never get here unless we don't support the command,
         *  so print a message
         */
        VMKLNX_DEBUG(0, "unknown opcode: 0x%x\n", cmd);
        return DMA_BIDIRECTIONAL;
   }
}

/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxDumpCmdDone --
 *
 *      Callback when a SCSI dump command completes.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
void 
SCSILinuxDumpCmdDone(struct scsi_cmnd *scmd)
{
   vmk_ScsiCommand *vmkCmdPtr = scmd->vmkCmdPtr;
   uint32_t ownLock = 0;
   unsigned long flags = 0;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   ownLock = !vmklnx_spin_is_locked_by_my_cpu(scmd->device->host->host_lock);

   VMK_ASSERT_BUG(scmd->vmkflags & VMK_FLAGS_NEED_CMDDONE);

   if (ownLock) {
      spin_lock_irqsave(scmd->device->host->host_lock, flags);
   }

   scmd->vmkflags &= ~VMK_FLAGS_NEED_CMDDONE;
   scmd->serial_number = 0;
   --scmd->device->host->host_busy;
   --scmd->device->device_busy;

   if (ownLock) {
      spin_unlock_irqrestore(scmd->device->host->host_lock, flags);
   }

   vmkCmdPtr->bytesXferred = scmd->request_bufflen - scmd->resid;

   if (vmkCmdPtr->bytesXferred > scmd->request_bufflen) {
      if (likely(VMKLNX_SCSI_STATUS_NO_LINUX(scmd->result) != 0)) {
         vmkCmdPtr->bytesXferred = 0;

         VMKLNX_WARN("Error BytesXferred > Requested Length "
                     "Marking transfer length as 0 - vmhba = %s, Driver Name = %s "
                     "Requested length = %d, Resid = %d", 
                     vmklnx_get_vmhba_name(scmd->device->host), 
                     scmd->device->host->hostt->name, scmd->request_bufflen, 
                     scmd->resid);
      } else {
         /*
          * We should not reach here
          */
         VMKLNX_ALERT("Error BytesXferred > Requested Length");
         vmkCmdPtr->bytesXferred = 0;
         scmd->result = DID_ERROR << 16;
      }
   }

   if (unlikely(VMKLNX_SCSI_STATUS_NO_LINUX(scmd->result) ==
                ((DID_OK << 16) | SAM_STAT_CHECK_CONDITION))) {
      vmk_ByteCount size;

      VMK_ASSERT(scmd->sense_buffer[0] != 0);
      size = vmk_ScsiGetSupportedCmdSenseDataSize();
      /* Use the additional sense length field for sense data length */
      size = min(size, (vmk_ByteCount)(scmd->sense_buffer[7] + 8));
      vmk_ScsiCmdSetSenseData((vmk_ScsiSenseData *)scmd->sense_buffer,
                              scmd->vmkCmdPtr,
                              min((vmk_ByteCount)SCSI_SENSE_BUFFERSIZE, size));
   }

   SCSILinuxCompleteCommand(vmkCmdPtr, VMKLNX_SCSI_HOST_STATUS(scmd->result),
                            VMKLNX_SCSI_DEVICE_STATUS(scmd->result));
   return;
}

/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxAbortCommands --
 *
 *      Abort all commands originating from a SCSI command.
 *
 * Results:
 *      VMK_OK, driver indicated that the abort succeeded or the driver
 *	  indicated that it has already completed the command.
 *      VMK_FAILURE, the driver indicated that the abort failed in some way.
 *
 * Notes:
 *      This function no more returns VMK_ABORT_NOT_RUNNING as upper layers
 *      were not being acted on this status. Both LSILOGIC and BUSLOGIC
 *      emulation layers anyway wait for the original I/O to finish. Only
 *      sidenote is that in case of BUSLOGIC, there is a possibility that
 *      the emulation driver returns success to ABORT but later the actual
 *      I/O could be returned with success.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
VMK_ReturnStatus
SCSILinuxAbortCommands(struct Scsi_Host *shost, struct scsi_device *sdev,
                       vmk_ScsiTaskMgmt *vmkTaskMgmtPtr)
{
   struct scsi_cmnd *scmd, *safecmd;
   VMK_ReturnStatus retval = VMK_OK;
   unsigned long flags, vmkflags;
   struct list_head abort_list;

   VMK_ASSERT(sdev != NULL);

   VMKLNX_DEBUG(4, "entered");

   /*
    * This will hold all the commands that need to be aborted
    */
   INIT_LIST_HEAD(&abort_list);

   /*
    * In the first loop, identify the commands that need to be aborted
    * and put them to a local queue. We also mark each of the commands
    * we need to abort with DELAY_CMD_DONE to defer any command completions
    * that come during this time
    */
   spin_lock_irqsave(&sdev->list_lock, flags);
   list_for_each_entry_safe(scmd, safecmd, &sdev->cmd_list, list) {
      vmk_ScsiTaskMgmtAction taskMgmtAction;

      /*
       * Check if the Linux SCSI command has already been
       * completed or is the one PSA is interested in aborting
       */
      spin_lock_irqsave(&scmd->vmklock, vmkflags);
      if (scmd->vmkflags & VMK_FLAGS_NEED_CMDDONE && 
          !(scmd->vmkflags & VMK_FLAGS_CMDDONE_ATTEMPTED) &&
	  ((taskMgmtAction = vmk_ScsiQueryTaskMgmt(vmkTaskMgmtPtr, 
	    scmd->vmkCmdPtr)) & VMK_SCSI_TASKMGMT_ACTION_ABORT)) {

         if (scmd->vmkflags & VMK_FLAGS_DELAY_CMDDONE) {
            VMKLNX_DEBUG(4, "cmd %p is already in the process of being aborted",
                         scmd);
            goto skip_sending_abort;
         } 
         scmd->vmkflags |= VMK_FLAGS_DELAY_CMDDONE;

         /*
          * Store the commands to be aborted in a temp list.
          * This is a reentrant function so we are keeping them in a local list
          * After the abort process is done, we put it back to sdev
          */
         list_move_tail(&scmd->list, &abort_list);

skip_sending_abort:;
      }
      spin_unlock_irqrestore(&scmd->vmklock, vmkflags);
   }
   spin_unlock_irqrestore(&sdev->list_lock, flags);

   /*
    * Drivers dont like to get called with locks held
    */
   list_for_each_entry(scmd, &abort_list, list) {
      VMK_ReturnStatus status;

      status = SCSILinuxAbortCommand(shost, scmd, DID_TIME_OUT);
      if (status == VMK_FAILURE) {
         retval = VMK_FAILURE; // For errors, we always change final retval
         VMKLNX_WARN("Failed, Driver %s, for %s",
                     shost->hostt->name, vmklnx_get_vmhba_name(shost));
      }
   }

   /*
    * Time to complete commands to vmkernel that were completed during the
    * the time we were aborting. We dont have to hold the lock during
    * the entire time and instead lock just when we do a list_move_tail, 
    * but considering reentrancy, this makes the code read bit simpler
    */
   spin_lock_irqsave(&sdev->list_lock, flags);
   list_for_each_entry_safe(scmd, safecmd, &abort_list, list) {

      VMK_ASSERT(scmd->vmkflags & VMK_FLAGS_DELAY_CMDDONE);

      spin_lock_irqsave(&scmd->vmklock, vmkflags);
      scmd->vmkflags &= ~VMK_FLAGS_DELAY_CMDDONE;

      list_move_tail(&scmd->list, &sdev->cmd_list);

      if (scmd->vmkflags & VMK_FLAGS_CMDDONE_ATTEMPTED) {
         spin_unlock_irqrestore(&scmd->vmklock, vmkflags);
         /*
          * We use this routine for both ABORT and VIRT_RESET
          * and so calls driver's abort handler for both types.
          * Abort handler sets scmd->result to DID_ABORT and so
          * we need to use DID_RESET for VIRT_RESET type.
          * PR# 412658
          */
         if (vmkTaskMgmtPtr->type == VMK_SCSI_TASKMGMT_VIRT_RESET &&
             host_byte(scmd->result) == DID_ABORT) {
            scmd->result = (DID_RESET << 16);
         }

         /*
          * This will trigger completion processing.
          * Note that abort can complete before that has a
          * chance to run
          */
         scmd->done(scmd);
      } else {
         spin_unlock_irqrestore(&scmd->vmklock, vmkflags);
      }
   }
   spin_unlock_irqrestore(&sdev->list_lock, flags);

   VMKLNX_DEBUG(4, "exit");

   return retval;
}

/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxAbortCommand --
 *
 *      Abort a single command
 *
 * Results:
 *      VMK_OK, if cmd was aborted or not running
 *      VMK_FAILURE, if abort failed for some reason
 *
 * Side effects:
 *      This function is called with  VMK_FLAGS_DELAY_CMDDONE set    
 * 
 *
 *----------------------------------------------------------------------
 */

static VMK_ReturnStatus
SCSILinuxAbortCommand(struct Scsi_Host *shost, struct scsi_cmnd *scmd, 
                      int32_t abortReason)
{
   int abortStatus = FAILED;
   unsigned long flags = 0;
   struct vmklnx_ScsiIntCmd *vmklnx26_ScsiIntCmd;

   VMKLNX_DEBUG(4, "cmd=%p, ser=%lu, op=0x%x", 
                   scmd, scmd->serial_number, scmd->cmnd[0]);

   if (vmkApiDebug && (scmd->vmkflags & VMK_FLAGS_DROP_CMD)) {
      // Fake a completion for this abort command
      scmd->result = (DID_ABORT << 16);
      if ((scmd->vmkflags & VMK_FLAGS_NEED_CMDDONE) &&
          VMKLNX_STRESS_DEBUG_COUNTER(stressVmklnxDropCmdScsiDone)) {
         VMK_ASSERT(scmd->vmkflags & 
		(VMK_FLAGS_NEED_CMDDONE|VMK_FLAGS_CMDDONE_ATTEMPTED));
         VMKLNX_DEBUG(0, "dropCmd calling done");
         scmd->done(scmd);
      } else {
         VMKLNX_DEBUG(0, "dropCmd NOT calling done");
      }
      if (VMKLNX_STRESS_DEBUG_COUNTER(stressVmklinuxAbortCmdFailure)) {
         VMKLNX_DEBUG(0, "dropCmd returning FAILED");
         abortStatus = FAILED;
      } else {
         VMKLNX_DEBUG(0, "dropCmd returning SUCCESS");
         abortStatus = SUCCESS;
      }
   } else if (VMKLNX_STRESS_DEBUG_COUNTER(stressVmklinuxAbortCmdFailure)) {
         VMKLNX_DEBUG(0, "VMKLINUX_ABORT_CMD_FAILURE Stress Fired");
         abortStatus = FAILED;
   } else {
      abortStatus = SCSILinuxTryAbortCommand(scmd, ABORT_TIMEOUT);
   }

   // Convert FAILED/SUCCESS into vmkernel error handling codes
   switch (abortStatus) {
   case SUCCESS:
      spin_lock_irqsave(&scmd->vmklock, flags);
      if (scmd->vmkflags & VMK_FLAGS_INTERNAL_COMMAND) {
         spin_unlock_irqrestore(&scmd->vmklock, flags);

         if (abortReason == DID_TIME_OUT) {
            scmd->result = DID_TIME_OUT << 16;
         }
         scmd->serial_number = 0;

         /*
          * Get pointer to internal Command
          */
         vmklnx26_ScsiIntCmd = (struct vmklnx_ScsiIntCmd *) scmd->vmkCmdPtr;

         /*
          * For internal commands, we need to call up for the command's
          * semaphore.
          */
	 complete(&vmklnx26_ScsiIntCmd->icmd_compl);

         return VMK_OK;
      } else if (!(scmd->vmkflags & VMK_FLAGS_CMDDONE_ATTEMPTED)) {
         spin_unlock_irqrestore(&scmd->vmklock, flags);
         VMKLNX_WARN("The driver failed to call done from its"
                     "abort handler and yet it returned SUCCESS");
         return VMK_FAILURE;
      } else {
         spin_unlock_irqrestore(&scmd->vmklock, flags);
         return VMK_OK;
      }
   default:
      VMKLNX_WARN("unexpected abortStatus = %x", abortStatus);
      VMK_ASSERT(0);
   case FAILED:
      spin_lock_irqsave(&scmd->vmklock, flags);
      if (scmd->vmkflags & VMK_FLAGS_CMDDONE_ATTEMPTED) {
         spin_unlock_irqrestore(&scmd->vmklock, flags);
         return VMK_OK;
      }
      spin_unlock_irqrestore(&scmd->vmklock, flags);
      return VMK_FAILURE;
   }

   VMK_NOT_REACHED();
}

/*
 * Function:	SCSILinuxTryAbortCommand
 *
 * Purpose:	Ask host adapter to abort a running command.
 *
 * Returns:	FAILED		Operation failed or not supported.
 *		SUCCESS		Succeeded.
 *
 * Notes:   This function will not return until the user's completion
 *	         function has been called.  There is no timeout on this
 *          operation.  If the author of the low-level driver wishes
 *          this operation to be timed, they can provide this facility
 *          themselves.  Helper functions in scsi_error.c can be supplied
 *          to make this easier to do.
 *
 * Notes:	It may be possible to combine this with all of the reset
 *	         handling to eliminate a lot of code duplication.  I don't
 *	         know what makes more sense at the moment - this is just a
 *	         prototype.
 */
int
SCSILinuxTryAbortCommand(struct scsi_cmnd * scmd, int timeout)
{
   int status;

   if (scmd->device->host->hostt->eh_abort_handler == NULL) {
      return FAILED;
   }

   /* 
    * scsi_done was called just after the command timed out and before
    * we had a chance to process it. (DB)
    */
   if (scmd->serial_number == 0)
      return SUCCESS;

   VMKAPI_MODULE_CALL(SCSI_GET_MODULE_ID(scmd->device->host),
              status, scmd->device->host->hostt->eh_abort_handler, scmd);

   return status;
}

/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxDoReset
 *
 *     Do a synchronous reset by calling appropriate driver entry point.
 * 
 * Results:
 *      VMK_OK, driver said reset succeeded.
 *      VMK_FAILURE, driver said reset failed in some way.
 *
 *----------------------------------------------------------------------
 */
static VMK_ReturnStatus
SCSILinuxDoReset(struct Scsi_Host *shost, vmk_ScsiTaskMgmt *vmkTaskMgmtPtr,
		 struct scsi_cmnd *scmd)
{
   int resetStatus = FAILED;
   unsigned long flags = 0;

   switch(vmkTaskMgmtPtr->type) {
      case VMK_SCSI_TASKMGMT_LUN_RESET:
         spin_lock_irqsave(&scmd->vmklock, flags);
	 scmd->vmkflags |= VMK_FLAGS_USE_LUNRESET;
         spin_unlock_irqrestore(&scmd->vmklock, flags);
	 // Fall through
      case VMK_SCSI_TASKMGMT_DEVICE_RESET:
         resetStatus = SCSILinuxTryBusDeviceReset(scmd, 0);
	 break;
      case VMK_SCSI_TASKMGMT_BUS_RESET:
         resetStatus = SCSILinuxTryBusReset(scmd);
	 break;
      default:
	 VMKLNX_NOT_IMPLEMENTED();
	 break;
   }
   if (resetStatus == SUCCESS) {
         return VMK_OK;
   } else if (resetStatus == FAILED) {
         return VMK_FAILURE;
   }

   VMKLNX_WARN("unexpected resetStatus = %x", resetStatus);
   VMK_ASSERT(0);

   return VMK_FAILURE;
}

/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxResetCommand --
 *
 *      Reset the SCSI bus. We would like the bus reset to always occur
 *      and therefore we create a cmdPtr and forces a SYNCHRONOUS reset
 *      on it. This is similar to what Linux does when you issue a
 *      reset through the Generic SCSI interface (the function
 *      scsi_reset_provider in drivers/scsi/scsi.c). Some drivers like
 *      Qlogic and mptscsi will call the completion for the cmdPtr we
 *      pass in if they cannot find the command (and they won't since
 *      we just create a cmdPtr to fill out).
 *
 * Results:
 *      VMK_OK, driver said reset succeeded.
 *      VMK_FAILURE, driver said reset failed in some way.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
VMK_ReturnStatus
SCSILinuxResetCommand(struct Scsi_Host *shost, struct scsi_device *sdev,
		      vmk_ScsiTaskMgmt *vmkTaskMgmtPtr)
{
   struct scsi_cmnd *scmd; 
   VMK_ReturnStatus retval;
   unsigned long flags;
   struct vmklnx_ScsiIntCmd *vmklnx26_ScsiIntCmd;
 
   VMK_ASSERT(sdev != NULL);

   /* Since we may have split a command, we want to do a full synchronous
    * BUS reset to get all outstanding commands back, since simply picking
    * one fragment of a command may cause that reset to fail because that
    * fragment just happened to complete before we called SCSILinuxDoReset
    */
   vmklnx26_ScsiIntCmd = kzalloc(sizeof(struct vmklnx_ScsiIntCmd), GFP_KERNEL);

   if(vmklnx26_ScsiIntCmd == NULL) {
      VMKLNX_WARN("Failed to Allocate SCSI internal Command");
      return VMK_NO_MEMORY;
   }

   VMK_DEBUG_ONLY(
      spin_lock_irqsave(&sdev->list_lock, flags);

      list_for_each_entry(scmd, &sdev->cmd_list, list) {
         // Fake a completion if we threw it away...
         if (scmd->serial_number &&
	     (scmd->vmkflags & VMK_FLAGS_DROP_CMD) &&
	     vmk_ScsiQueryTaskMgmt(vmkTaskMgmtPtr, scmd->vmkCmdPtr) != 
	     VMK_SCSI_TASKMGMT_ACTION_IGNORE) {
	    scmd->result = (DID_RESET << 16);
	    SCSILinuxCmdDone(scmd);
	 }
      }
      spin_unlock_irqrestore(&sdev->list_lock, flags);
   )

   /*
    * Fill in values required for command structure
    * Fill in vmkCmdPtr with vmklnx26_ScsiIntCmd pointer
    * Fill done with InternalCmdDone routine
    */
   SCSILinuxInitInternalCommand(sdev, vmklnx26_ScsiIntCmd);

   scmd = &vmklnx26_ScsiIntCmd->scmd;

   /*
    * Set the state of this command as timed out
    * This is only used in completion if drivers call this
    */
   scmd->eh_eflags = SCSI_STATE_TIMEOUT;
   spin_lock_irqsave(&scmd->vmklock, flags);
   scmd->vmkflags = VMK_FLAGS_TMF_REQUEST;
   spin_unlock_irqrestore(&scmd->vmklock, flags);

   retval = SCSILinuxDoReset(shost, vmkTaskMgmtPtr, scmd);

   /*
    * Free the command structure
    */
   kfree(vmklnx26_ScsiIntCmd);

   return retval;
}


/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxScheduleCompletion
 *
 *      Adds the given scsi_cmnd to the completion list and schedules
 *      a bottom half.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Mutates the given scsiLinuxTLS_t.
 *
 *----------------------------------------------------------------------
 */
static inline 
void SCSILinuxScheduleCompletion(struct scsi_cmnd *scmd)
{
   scsiLinuxTLS_t *tls = SCSILinuxGetTLS(scmd);
   vmk_Bool wasEmpty;
   unsigned long flags;

   spin_lock_irqsave(&tls->lock, flags);
   wasEmpty = !SCSILinuxTLSWorkPending(tls);

   list_add_tail(&scmd->bhlist, &tls->isrDoneCmds);
   spin_unlock_irqrestore(&tls->lock, flags);

   if (wasEmpty) {
      vmk_IntrCookie intr;

      if (VMK_TRUE == vmk_ContextIsInterruptHandler(&intr)) {
         tls->activatingIntr = intr;
      }
      vmk_WorldletActivate(tls->worldlet);
   }
}


/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxCmdDone --
 *
 *      Callback when a SCSI command completes. Typically from ISR 
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      This function can be called with list_lock held
 *
 *----------------------------------------------------------------------
 */

void 
SCSILinuxCmdDone(struct scsi_cmnd *scmd)
{
   unsigned long flags = 0;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
#if defined(VMX86_DEBUG)
   if (scmd->use_sg == 0) {
      if (!vmklnx_is_physcontig(scmd->request_buffer, scmd->request_bufflen)) {
        VMKLNX_PANIC("SCSILinuxCmdDone: "
                     "discontiguous request buffer %p, cmd = %p",
                     scmd->request_buffer, scmd);
      }
   }
#endif /* defined(VMX86_DEBUG) */

   spin_lock_irqsave(&scmd->vmklock, flags);
   if (unlikely((scmd->vmkflags & VMK_FLAGS_IO_TIMEOUT) != 0)) {
      spin_unlock_irqrestore(&scmd->vmklock, flags);
      scsi_delete_timer(scmd);
      spin_lock_irqsave(&scmd->vmklock, flags);
   }

   if (unlikely((scmd->vmkflags & VMK_FLAGS_TMF_REQUEST) != 0)) {
      spin_unlock_irqrestore(&scmd->vmklock, flags);
      VMKLNX_DEBUG(2, "Command Completion on TMF's scmd");
      return;
   }

   if (unlikely(!(scmd->vmkflags & VMK_FLAGS_NEED_CMDDONE))) {
      spin_unlock_irqrestore(&scmd->vmklock, flags);
      VMKLNX_PANIC("Attempted double completion");
   }

   if (unlikely((scmd->vmkflags & VMK_FLAGS_DELAY_CMDDONE) != 0)) {
      scmd->vmkflags |= VMK_FLAGS_CMDDONE_ATTEMPTED;
      spin_unlock_irqrestore(&scmd->vmklock, flags);
      return;
   }

   scmd->vmkflags &= ~VMK_FLAGS_NEED_CMDDONE;
   spin_unlock_irqrestore(&scmd->vmklock, flags);

   /*
    * Reset the Serial Number to indicate the command is no more
    * active with the driver
    */
   scmd->serial_number = 0; 

   /* Fail cmd on underflow since many drivers fail to do so */
   if (likely(scmd->result == DID_OK)) {
      if (unlikely(scmd->resid < 0)) {
         VMKLNX_WARN("Driver reported -ve resid %d, yet had cmd success", scmd->resid);
         scmd->result = DID_ERROR << 16;
      } else if (unlikely(scmd->request_bufflen - scmd->resid <
                                                             scmd->underflow)) {
         scmd->result = DID_ERROR << 16;
         VMKLNX_WARN("Underrun detected: SN %#"VMK_FMT64"x, initiator %p "
		     "bufflen %d resid %d underflow %d",
                     scmd->vmkCmdPtr->cmdId.serialNumber,
                     scmd->vmkCmdPtr->cmdId.initiator,
		     scmd->request_bufflen, scmd->resid, scmd->underflow);
      }
   }

   /*
    * Add to the completion list and schedule follow-up processing.
    */
   SCSILinuxScheduleCompletion(scmd);
}

/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxInitCommand --
 *
 *      Initialize Linux SCSI command Pointer
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *     
 *----------------------------------------------------------------------
 */
void
SCSILinuxInitCommand(struct scsi_device *sdev, struct scsi_cmnd *scmd)
{
   VMK_ASSERT(sdev->host != NULL);

   scmd->serial_number = SCSILinuxGetSerialNumber();
   scmd->jiffies_at_alloc = jiffies;
}

/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxDVCommandDone --
 *
 *      Completion routine for internal DV commands.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Removes the timer associated with the DV command and calls
 *      "up" for its semaphore.
 *     
 *----------------------------------------------------------------------
 */
static void
SCSILinuxDVCommandDone(struct scsi_cmnd *scmd)
{
   struct vmklnx_ScsiIntCmd *vmklnx26_ScsiIntCmd;

   /*
    * vmkCmdPtr is valid for DV Commands
    */
   VMK_ASSERT(scmd->vmkCmdPtr);

   /*
    * Get pointer to internal Command
    */
   vmklnx26_ScsiIntCmd = (struct vmklnx_ScsiIntCmd *) scmd->vmkCmdPtr;

   /*
    * Remove timer
    */
   scsi_delete_timer(scmd);

   /*
    * Wake up the waiting thread
    */
   complete(&vmklnx26_ScsiIntCmd->icmd_compl);
}

/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxInitInternalCommand --
 *
 *      Initialize an vmklinux internal SCSI command
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *     
 *----------------------------------------------------------------------
 */
void
SCSILinuxInitInternalCommand(
                struct scsi_device *sdev,
                struct vmklnx_ScsiIntCmd *vmklnx26_ScsiIntCmd)
{
   struct scsi_cmnd *scmd;

   VMK_ASSERT(vmklnx26_ScsiIntCmd);
   VMK_ASSERT(sdev);

   scmd = &vmklnx26_ScsiIntCmd->scmd;
   VMK_ASSERT(scmd);

   scmd->vmkflags |= VMK_FLAGS_INTERNAL_COMMAND;
   INIT_LIST_HEAD(&scmd->bhlist);
   spin_lock_init(&scmd->vmklock);
   init_completion(&vmklnx26_ScsiIntCmd->icmd_compl);
   scmd->device = sdev;

   SCSILinuxInitCommand(sdev, scmd);

   if (scmd->device->tagged_supported) {
        scmd->tag = SIMPLE_QUEUE_TAG;
   } else {
        scmd->tag = 0;
   } 
   scmd->done = SCSILinuxInternalCommandDone;
}


/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxInitScmd --
 *
 *      Initialize a scsi command. Used in the IO path
 *
 * Results:
 *      None.
 *
 * Side effects:
 *       None
 *     
 *----------------------------------------------------------------------
 */
void
SCSILinuxInitScmd(struct scsi_device *sdev, struct scsi_cmnd *scmd)
{
   SCSILinuxInitCommand(sdev, scmd);
   scmd->done = SCSILinuxCmdDone;
}

/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxTryBusDeviceReset --
 *
 *	Ask host adapter to perform a bus device reset for a given device.
 *
 * Results:
 *      FAILED or SUCCESS
 *
 * Side effects:
 * 	There is no timeout for this operation.  If this operation is
 *      unreliable for a given host, then the host itself needs to put a
 *      timer on it, and set the host back to a consistent state prior
 *      to returning.
 *     
 *----------------------------------------------------------------------
 */
static int
SCSILinuxTryBusDeviceReset(struct scsi_cmnd * scmd, int timeout)
{
   int rtn;

   if (scmd->device->host->hostt->eh_device_reset_handler == NULL) {
      return FAILED;
   }
  
   VMKLNX_DEBUG(2, "Start");
   VMKAPI_MODULE_CALL(SCSI_GET_MODULE_ID(scmd->device->host),
              rtn, scmd->device->host->hostt->eh_device_reset_handler, scmd);
   VMKLNX_DEBUG(2, "End");

   return rtn;
}

#define BUS_RESET_SETTLE_TIME   5000 /* ms */

/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxTryBusReset --
 *
 * 	Ask host adapter to perform a bus reset for a host.
 *
 * Results:
 *      SUCCESS or FAILED
 *
 * Side effects:
 *      None. Caller must host adapter lock
 *     
 *----------------------------------------------------------------------
 */
static int
SCSILinuxTryBusReset(struct scsi_cmnd * scmd)
{
   int rtn;

   if (scmd->device->host->hostt->eh_bus_reset_handler == NULL) {
      return FAILED;
   }

   VMKLNX_DEBUG(2, "Start");
   VMKAPI_MODULE_CALL(SCSI_GET_MODULE_ID(scmd->device->host),
              rtn, scmd->device->host->hostt->eh_bus_reset_handler, scmd);
   VMKLNX_DEBUG(2, "End");

   /*
    * If we had a successful bus reset, mark the command blocks to expect
    * a condition code of unit attention.
    */
   vmk_WorldSleep(BUS_RESET_SETTLE_TIME * 1000); 

#if 0
   /*
    * Need to look up where this is cleared
    */
   if (rtn == SUCCESS) {
      struct scsi_device *sdev;
      shost_for_each_device(sdev, scmd->device->host) {
         if (scmd->device->channel == sdev->channel) {
            sdev->was_reset = 1;
            sdev->expecting_cc_ua = 1;
         }
      }
   }
#else
   /*
    * No consumers for this state. Will remove post all driver bring up
    */
#endif

   return rtn;
}

/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxProcessInquiryResponse --
 *
 *      Parse the response to a standard inquiry command and populate
 *      the device accordingly
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None. 
 *
 *----------------------------------------------------------------------
 */
static void
SCSILinuxProcessStandardInquiryResponse(struct scsi_cmnd *scmd)
{
   struct scsi_device  *sdev;
   vmk_ScsiInquiryCmd *inqCmd;
   VMK_ReturnStatus status;
   int type;
   unsigned long flags;

   inqCmd = (vmk_ScsiInquiryCmd *) scmd->cmnd;

   if (inqCmd->opcode != VMK_SCSI_CMD_INQUIRY || inqCmd->evpd) {
      return;
   }

   sdev = scmd->device;

   VMKLNX_DEBUG(5, "Start");

   spin_lock_irqsave(sdev->host->host_lock, flags);

   if (sdev->vmkflags & HAVE_CACHED_INQUIRY) {
      goto done;
   }

   status = vmk_SgCopyFrom(sdev->inquiryResult,
                           scmd->vmkCmdPtr->sgArray,
                           min(sizeof(sdev->inquiryResult),
                                     (size_t)scmd->vmkCmdPtr->bytesXferred));
   if (status != VMK_OK) {
      goto done;
   }

   sdev->vendor = (char *) (sdev->inquiryResult + 8);
   sdev->model = (char *) (sdev->inquiryResult + 16);
   sdev->rev = (char *) (sdev->inquiryResult + 32);

   /* 
    * Some of the macro's need to access this data at a later time
    */
   sdev->inquiry = (unsigned char *) (sdev->inquiryResult);

   sdev->removable = (0x80 & sdev->inquiryResult[1]) >> 7;
   sdev->lockable = sdev->removable;

   sdev->inq_periph_qual = (sdev->inquiryResult[0] >> 5) & 7;
      
   sdev->soft_reset = (sdev->inquiryResult[7] & 1) && 
			((sdev->inquiryResult[3] & 7) == 2);
      
   sdev->scsi_level = sdev->inquiryResult[2] & 0x07;
   if (sdev->scsi_level >= 2 || 
       (sdev->scsi_level == 1 && (sdev->inquiryResult[3] & 0x0f) == 1)) {
      sdev->scsi_level++;
   }

   sdev->inquiry_len = sdev->inquiryResult[4] + 5;

   if (sdev->scsi_level >= SCSI_3 || (sdev->inquiry_len > 56 &&
		sdev->inquiryResult[56] & 0x04)) {
      sdev->ppr = 1;
   }

   if (sdev->inquiryResult[7] & 0x60) {
      sdev->wdtr = 1;
   }
   if (sdev->inquiryResult[7] & 0x10) {
      sdev->sdtr = 1;
   }

   sdev->use_10_for_rw = 1;
      
   /*
    * Currently, all sequential devices are assumed to be tapes, all random
    * devices disk, with the appropriate read only flags set for ROM / WORM
    * treated as RO.
    */
   type = (sdev->inquiryResult[0] & 0x1f);
   switch (type) {
   case TYPE_PROCESSOR:
   case TYPE_TAPE:
   case TYPE_DISK:
   case TYPE_PRINTER:
   case TYPE_MOD:
   case TYPE_SCANNER:
   case TYPE_MEDIUM_CHANGER:
   case TYPE_ENCLOSURE:
      sdev->writeable = 1;
      break;
   case TYPE_WORM:
   case TYPE_ROM:
      sdev->writeable = 0;
      break;
   default:
      break;
   }

   sdev->type = (type & 0x1f);

   /*
    * Set the tagged_queue flag for SCSI-II devices that purport to support
    * tagged queuing in the INQUIRY data. The CMDQUE bit (and BQUE bit) indicate 
    * whether the logical unit supports the full task management model.
    */
   if ((sdev->scsi_level >= SCSI_2) && (sdev->inquiryResult[7] & 2)) {
      sdev->tagged_supported = 1;
      sdev->current_tag = 0;
   }

   /*
    * VMKLINUX FLAG - Safe under host_lock
    */ 
   sdev->vmkflags |= HAVE_CACHED_INQUIRY;

   /*
    * The fields need to be initialized on a need basis, None so far
    * fields are
    * sdev->borken
    * sdev->select_no_atn
    * sdev->no_start_on_add
    * sdev->single_lun
    * sdev->skip_ms_page_8
    * sdev->skip_ms_page_3f
    * sdev->use_10_for_ms
    * sdev->use_192_bytes_for_3f
    * sdev->retry_hwerror
    */
done:
   spin_unlock_irqrestore(sdev->host->host_lock, flags);
   VMKLNX_DEBUG(5, "End");
}

/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxInternalCommandDone --
 *
 *      SCSI internal Command Completion routine 
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None. 
 *
 *----------------------------------------------------------------------
 */
void
SCSILinuxInternalCommandDone(struct scsi_cmnd *scmd)
{
   struct vmklnx_ScsiIntCmd *vmklnx26_ScsiIntCmd;
   unsigned long flags = 0;

   spin_lock_irqsave(&scmd->vmklock, flags);

   /*
    * Some drivers call Complete Command even on Reset requests
    */
   if (unlikely((scmd->vmkflags & VMK_FLAGS_TMF_REQUEST) != 0)) {
      spin_unlock_irqrestore(&scmd->vmklock, flags);
      VMKLNX_DEBUG(2, "Command Completion on TMF's scmd");
      return;
   }

   /*
    * vmkCmdPtr is valid for DV Commands
    */

   VMK_ASSERT(scmd->vmkCmdPtr);

   /*
    * Get pointer to internal Command
    */
   vmklnx26_ScsiIntCmd = (struct vmklnx_ScsiIntCmd *) scmd->vmkCmdPtr;

   /*
    * Internal commands are used only for Task Mgmt and
    * DV requests. We have handled the first case above. 
    * Second case - DV sets up a command timer
    * Check if the command has timed out already
    */
   if (scmd->vmkflags & VMK_FLAGS_NEED_CMDDONE) {
      scmd->serial_number = 0;
      scmd->vmkflags &= ~VMK_FLAGS_NEED_CMDDONE;
      spin_unlock_irqrestore(&scmd->vmklock, flags);

      scmd->done = SCSILinuxDVCommandDone;

      /*
       * Add to the completion list and schedule follow-up procesing.
       */
      SCSILinuxScheduleCompletion(scmd);
   } else {
      spin_unlock_irqrestore(&scmd->vmklock, flags);
      VMKLNX_WARN("Command was timed out. Dropping the completion");
      return;
   }
}

#define  VMKLNX_SCSI_SCAN_BUSY_RETRIES    60
#define  VMKLNX_SCSI_SCAN_BUSY_RETRY_DELAY   (1 * VMK_USEC_PER_SEC)

/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxAddPaths--
 *
 *     Add new paths
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None. 
 *
 *----------------------------------------------------------------------
 */
VMK_ReturnStatus 
SCSILinuxAddPaths(vmk_ScsiAdapter *vmkAdapter, unsigned int channel,
                 unsigned int target, unsigned int lun)
{
   int busyCount;
   VMK_ReturnStatus ret = VMK_FAILURE;
   unsigned int channelMap = channel, targetMap = target, lunMap = lun;

   vmk_WorldAssertIsSafeToBlock();

   /*
    * VMKernel does not deal with scan wild cards
    */
   if (channelMap == SCAN_WILD_CARD) {
      channelMap = VMK_SCSI_PATH_ANY_CHANNEL;
   }

   if (targetMap == SCAN_WILD_CARD) {
      targetMap = VMK_SCSI_PATH_ANY_TARGET;
   }

   if (lunMap == SCAN_WILD_CARD) {
      lunMap = VMK_SCSI_PATH_ANY_LUN;
   }

   for (busyCount = 0; busyCount < VMKLNX_SCSI_SCAN_BUSY_RETRIES;
        ++busyCount) {
      ret = vmk_ScsiScanAndClaimPaths(&vmkAdapter->name,
                                      channelMap, targetMap, lunMap);

      if (ret == VMK_BUSY) {
         /*
          * Sleep in micro seconds
          */
         vmk_WorldSleep(VMKLNX_SCSI_SCAN_BUSY_RETRY_DELAY);
      } else {
         /*
          * Not a busy state. So break out
          */
         break;
      }
   }
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxDeleteAdapterPaths --
 *
 *     Delete all paths on an adapter.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None. 
 *
 *----------------------------------------------------------------------
 */
VMK_ReturnStatus 
SCSILinuxDeleteAdapterPaths(vmk_ScsiAdapter *vmkAdapter)
{
   int busyCount;
   VMK_ReturnStatus ret;

   vmk_WorldAssertIsSafeToBlock();

   for (busyCount = 0; busyCount < VMKLNX_SCSI_SCAN_BUSY_RETRIES;
        ++busyCount) {
      ret = vmk_ScsiScanDeleteAdapterPaths(vmkAdapter);

      if (ret == VMK_BUSY) {
         /*
          * Sleep in micro seconds
          */
         vmk_WorldSleep(VMKLNX_SCSI_SCAN_BUSY_RETRY_DELAY);
      } else {
         /*
          * Not a busy state. So break out
          */
         break;
      }
   }
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxCreatePath --
 *
 *     Create a path. Returns status and pointer to sdev that is newly 
 * allocated
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None. 
 *
 *----------------------------------------------------------------------
 */
VMK_ReturnStatus SCSILinuxCreatePath(struct Scsi_Host *sh,
                int channel, int target, int lun, struct scsi_device **sdevdata)
{
   struct scsi_device  *sdev;
   struct scsi_target  *stgt;
   unsigned long flags;
   VMK_ReturnStatus ret = VMK_OK;
   int old_target = 0;

   if (sh->reverse_ordering) {
      VMKLNX_WARN("Scanning in reverse order not supported");
      return VMK_FAILURE;
   }

   stgt = vmklnx_scsi_alloc_target_conditionally(&sh->shost_gendev, channel, target, 0,
                                                 &old_target);

   if (!stgt) {
      return VMK_NO_CONNECT;
   }

   /*
    * Dont really care what state sdev is. As long as sdev is in the list
    * fail any path creation request. The drivers dont want to receive
    * any requests that is either created or being deleted
    */
   spin_lock_irqsave(sh->host_lock, flags);
   sdev = __scsi_device_lookup(sh, channel, target, lun);
   spin_unlock_irqrestore(sh->host_lock, flags);

   if (sdev) {
      VMKLNX_WARN("Trying to discover path (%s:%d:%d:%d) that is"
                  "present with state %d", vmklnx_get_vmhba_name(sh), channel, 
		  target, lun, sdev->sdev_state);

      ret = VMK_BUSY;
      goto out;
   }

   sdev = scsi_alloc_sdev(stgt, lun, NULL);

   if (!sdev) {
      ret = VMK_NO_MEMORY;
      goto out;
   }

   if (IS_ERR(sdev)) {
      ret = VMK_NO_CONNECT;
      goto out;
   }

   *sdevdata = sdev;

out:
   if (old_target) {
      put_device(&stgt->dev);
   }
   return ret;
}

/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxConfigurePath --
 *
 *     Configure a path if it is found. 
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None. 
 *
 *----------------------------------------------------------------------
 */
VMK_ReturnStatus SCSILinuxConfigurePath(struct Scsi_Host *sh,
                int channel, int target, int lun)
{
   struct scsi_device  *sdev;
   unsigned long flags;

   sdev = scsi_device_lookup(sh, channel, target, lun);
   if (sdev) {
      /*
       * Down the reference count now
       */
      scsi_device_put(sdev);

      if (sh->hostt->slave_configure) {
         int ret;

         VMKAPI_MODULE_CALL(SCSI_GET_MODULE_ID(sh), ret,
                    sh->hostt->slave_configure, sdev);

         if (ret) {
           return VMK_FAILURE;
         }
      }
      /*
      * If no_uld_attach is ON, return VMK_NOT_FOUND to hide the device
      * from upper layer.
      */
      if (sdev->no_uld_attach) {
        return VMK_NOT_FOUND;
      }
   } else {
      return VMK_FAILURE;
   }

   spin_lock_irqsave(sh->host_lock, flags);
   sdev->sdev_state = SDEV_RUNNING;
   spin_unlock_irqrestore(sh->host_lock, flags);

   return VMK_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxDestroyPath --
 *
 *     Destroy a given path. 
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None. 
 *
 *----------------------------------------------------------------------
 */
VMK_ReturnStatus SCSILinuxDestroyPath(struct Scsi_Host *sh,
                int channel, int target, int lun)
{
   struct scsi_device  *sdev;
   unsigned long flags;

   /*
    * Does not matter what state sdev is
    */
   spin_lock_irqsave(sh->host_lock, flags);
   sdev = __scsi_device_lookup(sh, channel, target, lun);
   spin_unlock_irqrestore(sh->host_lock, flags);

   if (sdev) {
      scsi_destroy_sdev(sdev);
   } else {
      return VMK_FAILURE;
   }

   return VMK_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxCmdTimedOut --
 *
 *     Handle a timed-out command
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
void
SCSILinuxCmdTimedOut(struct scsi_cmnd *scmd)
{
   unsigned long flags;

   VMK_ASSERT((scmd->vmkflags & VMK_FLAGS_IO_TIMEOUT) ||
              (scmd->vmkflags & VMK_FLAGS_INTERNAL_COMMAND));

   spin_lock_irqsave(&scmd->vmklock, flags);

   scmd->vmkflags &= ~VMK_FLAGS_IO_TIMEOUT;

   /*
    * Check if the command timed out or completed already
    */
   if (scmd->vmkflags & VMK_FLAGS_NEED_CMDDONE) {
      if (unlikely((scmd->vmkflags & VMK_FLAGS_DELAY_CMDDONE) != 0)) {
         spin_unlock_irqrestore(&scmd->vmklock, flags);
         VMKLNX_DEBUG(4, "cmd %p is already in the process of being aborted",
                         scmd);
         return;
      } else {
         scmd->vmkflags |= VMK_FLAGS_DELAY_CMDDONE;
      }
   } else {
      spin_unlock_irqrestore(&scmd->vmklock, flags);
      return;
   }

   /*
    * Note deviation -- we are clearing VMK_FLAGS_NEED_CMDDONE here,
    * when it would normally only be cleared by vmklinux when the
    * driver completes the IO.  It is possible that a completion for
    * the command is in-flight when we do this.
    */
   if (scmd->vmkflags & VMK_FLAGS_INTERNAL_COMMAND) {
      scmd->vmkflags &= ~VMK_FLAGS_NEED_CMDDONE;
      scmd->serial_number = 0;
   }

   spin_unlock_irqrestore(&scmd->vmklock, flags);

   /* Intialize work for timed-out command */
   linuxSCSIIO_work.scmd = scmd;
   INIT_WORK(&linuxSCSIIO_work.work, SCSIProcessCmdTimedOut);
   queue_work(linuxSCSIWQ, &linuxSCSIIO_work.work);

   return;
}

/*
 *----------------------------------------------------------------------
 *
 * SCSIProcessCmdTimedOut --
 *
 *     Handles timeout in case of command time outs
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
void
SCSIProcessCmdTimedOut(struct work_struct *work)
{
   struct scsi_cmnd *scmd = 
      container_of(work, struct scsiIO_work_struct, work)->scmd;
   VMK_ReturnStatus status = VMK_OK;
   unsigned long vmkflags;


   VMKLNX_DEBUG(4, "Aborting cmd");

   status = SCSILinuxAbortCommand(scmd->device->host, scmd, DID_TIME_OUT);

   if (status == VMK_FAILURE) {
      VMKLNX_WARN("Failed, Driver %s, for %s",
                  scmd->device->host->hostt->name,
                  vmklnx_get_vmhba_name(scmd->device->host));
   }

   /*
    * Time to complete command to vmkernel that were completed during the
    * the time we were aborting. 
    */
   
   VMK_ASSERT(scmd->vmkflags & VMK_FLAGS_DELAY_CMDDONE);

   spin_lock_irqsave(&scmd->vmklock, vmkflags);
   scmd->vmkflags &= ~VMK_FLAGS_DELAY_CMDDONE;

   if (unlikely((scmd->vmkflags & VMK_FLAGS_CMDDONE_ATTEMPTED) != 0)) {
      spin_unlock_irqrestore(&scmd->vmklock, vmkflags);
      /*
       * This will trigger completion processing.
       * Note that abort can complete before that has a
       * chance to run
       */
       scmd->done(scmd);
   } else {
      spin_unlock_irqrestore(&scmd->vmklock, vmkflags);
   }
   return;
}

/*
 *-----------------------------------------------------------------------------
 *
 * SCSILinuxWorldletFn --
 *
 *      Worldlet function to process completions.
 *
 *      The location of this worldlet is determined by the affinity tracker
 *      object.  We feed into the affinity tracker the id's of worldlets and
 *      worlds that are submitting requests in (in
 *      SCSILinuxProcessCompletions).  In this function we "check" the affinity
 *      tracker, and it will periodically update the affinity of this worldlet
 *      to the correct, heaviest, user of the HBA queue that this worldlet is
 *      associated with..
 *
 * Results:
 *      VMK_OK - executed correctly.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

VMK_ReturnStatus
SCSILinuxWorldletFn(vmk_Worldlet wdt, void *data,
                    vmk_WorldletRunData *runData)
{
   VMK_ReturnStatus status;
   vmk_TimerCycles yield, now;
   unsigned long flags;
   scsiLinuxTLS_t *tls = data;
   vmk_Bool shouldYield = VMK_FALSE;
   int n_cmp = 0;
   struct Scsi_Host *shost = tls->vmk26Adap->shost;

   LIST_HEAD(sentry);

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);

   /* Cannot use scsi_host_get here because it's possible that if an adapter
    * is being removed that scsi_remove_host has already been called and put
    * the shost into the SHOST_DEL state. If this worldlet is running, the
    * shost reference count has not yet gone to 0 because there are outstanding
    * commands in either the isrDoneCmds queue or the bhDoneCmds queue. Those
    * commands need to be completed, but we need to ensure that the shost
    * reference count does not go to 0 before the worldlet has finished running
    * (which otherwise could happen when the last command completion is
    * processed). Taking a direct reference on the shost_gendev without
    * checking shost_state here will accomplish this goal.
    */
   get_device(&shost->shost_gendev);

   now = 0;

   SCSILinuxTLSSetActive(tls);

   /*
    * XXX: PR 471442 fix will give us a yield time.
    * We need a yield time so that SCSILinuxProcessCompletions knows
    * how long it should run.
    */
   yield = vmk_GetTimerCycles() + vmk_TimerUSToTC(300); /* 300 us */

   spin_lock_irqsave(&tls->lock, flags);
   while(SCSILinuxTLSWorkPending(tls) && shouldYield == FALSE) {
      int num;

      list_splice_init(&tls->isrDoneCmds, tls->bhDoneCmds.prev);
      INIT_LIST_HEAD(&sentry);
      list_add_tail(&sentry, &tls->isrDoneCmds);

      spin_unlock_irqrestore(&tls->lock, flags);

      num = SCSILinuxProcessCompletions(tls, yield, &now);
      n_cmp += num;
      runData->userData1 += num;
      runData->userData2 += 1;

      status = vmk_WorldletShouldYield(wdt, &shouldYield);
      VMK_ASSERT(status == VMK_OK);

      spin_lock_irqsave(&tls->lock, flags);
      list_del(&sentry);

      /*
       * We're getting closer to the yield time, so next time we should
       * not spend as much time running as before.  If < 300us has passed
       * up until now, then we're likely to finish because we're out of work.
       */
      yield += vmk_TimerUSToTC(100);
   }

   spin_unlock_irqrestore(&tls->lock, flags);

   if (tls->tracker != NULL) {
      vmk_WorldletAffinityTrackerCheck(tls->tracker, now);
   }

   vmk_IntrTrackerAddSample(tls->activatingIntr, n_cmp, now);

   if (SCSILinuxTLSWorkPending(tls)) {
      runData->state = VMK_WDT_READY;
   } else {
      runData->state = VMK_WDT_SUSPEND;
   }

   SCSILinuxTLSSetActive(NULL);

   put_device(&shost->shost_gendev);

   return VMK_OK;
}


/*
 *-----------------------------------------------------------------------------
 *
 * SCSILinuxTLSWorldletCreate --
 *
 *      Create a worldlet-backed scsiLinuxTLS_t for a particular host.
 *
 * Results:
 *      Pointer to newly created object, NULL on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

scsiLinuxTLS_t *
SCSILinuxTLSWorldletCreate(const char *shostName,
                           struct vmklnx_ScsiAdapter *vmklnxAdap)
{
   VMK_ReturnStatus status;
   scsiLinuxTLS_t *tls;
   vmk_ServiceAcctID srv;
   vmk_Name worldletName;

   status = vmk_ServiceGetID(VMK_SERVICE_ACCT_NAME_SCSI, &srv);
   if (status != VMK_OK) {
      return NULL;
   }

   tls = vmklnx_kmalloc_align(VMK_MODULE_HEAP_ID,
                              sizeof(scsiLinuxTLS_t),
                              VMK_L1_CACHELINE_SIZE,
			      GFP_KERNEL);
   if (tls == NULL) {
      VMK_ASSERT(0);
      return NULL;
   }
   memset(tls, 0, sizeof(*tls));
   INIT_LIST_HEAD(&tls->bhDoneCmds);
   INIT_LIST_HEAD(&tls->isrDoneCmds);
   spin_lock_init(&tls->lock);

   tls->activatingIntr = VMK_INVALID_INTRCOOKIE;

   /*
    * Ignore truncation here.
    */
   (void) vmk_NameInitialize(&worldletName, shostName);

   status = vmk_WorldletCreate(&tls->worldlet, &worldletName, srv,
                               vmklinuxModID, VMK_MODULE_HEAP_ID,
                               SCSILinuxWorldletFn, tls);
   if (status != VMK_OK) {
      kfree(tls);
      return NULL;
   }
   vmk_WorldletGetId(tls->worldlet, &tls->worldletId);

   status = vmk_WorldletAffinityTrackerCreate(tls->worldlet,
                                              VMK_MODULE_HEAP_ID,
                                              &tls->tracker);
   VMK_ASSERT(status == VMK_OK);
   if (status != VMK_OK) {
      vmk_WorldletUnref(tls->worldlet);
      kfree(tls);
      return NULL;
   }

   vmk_WorldletAffinityTrackerConfig(tls->tracker,
                                     SCSI_AT_SET_THRESHOLD,
                                     SCSI_AT_DROP_THRESHOLD,
                                     SCSI_AT_UPDATE_PERIOD /* ms */,
                                     VMK_WDT_AFFINITY_LOOSE,
                                     VMK_WDT_AFFINITY_EXACT);
   tls->vmk26Adap = vmklnxAdap;
   return tls;
}


/*
 *-----------------------------------------------------------------------------
 *
 * SCSILinuxTLSWorldletDestroy --
 *
 *      Destroys a scsiLinuxTLS_t object.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Destroys the worldlet associated with the object.
 *
 *-----------------------------------------------------------------------------
 */

void
SCSILinuxTLSWorldletDestroy(scsiLinuxTLS_t *tls)
{
   VMK_ASSERT(!SCSILinuxTLSWorkPending(tls));

   if (tls->worldlet != VMK_INVALID_WORLDLET) {
      VMK_ReturnStatus status;

      vmk_WorldletDisable(tls->worldlet);
      status = vmk_WorldletWaitForQuiesce(tls->worldlet, 10000);

      /*
       * We shouldn't even be here if the worldlet is in use, so this should
       * succeed.
       */
      VMK_ASSERT(status == VMK_OK);
      vmk_WorldletAffinityTrackerDestroy(tls->tracker);
      vmk_WorldletUnref(tls->worldlet);
   }
   vmklnx_kfree(VMK_MODULE_HEAP_ID, tls);
}

/*
 *-----------------------------------------------------------------------------
 *
 * SCSILinuxTLSGetWorldlet --
 *
 *      Retrives the Worldlet created for given scsiLinuxTLS_t object.
 *
 * Results:
 *      Returns the pointer to the worldlet.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

vmk_Worldlet
SCSILinuxTLSGetWorldlet(scsiLinuxTLS_t *tls)
{
   return tls->worldlet;
}

/*
 *-----------------------------------------------------------------------------
 *
 * SCSILinuxTLSGetIOQueueHandle --
 *
 *      Retrives the Adapter IOQueue handle from scsiLinuxTLS_t object.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void *
SCSILinuxTLSGetIOQueueHandle(scsiLinuxTLS_t *tls)
{
   return tls->adapterIOQueueHandle;
}


/*
 *-----------------------------------------------------------------------------
 *
 * SCSILinuxTLSSetIOQueueHandle --
 *
 *      Sets the Adapter IOQueue handle in scsiLinuxTLS_t object.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

VMK_ReturnStatus
SCSILinuxTLSSetIOQueueHandle(scsiLinuxTLS_t *tls, void *q_handle)
{
   if (SCSILinuxTLSGetIOQueueHandle(tls) == NULL || q_handle == NULL) {
      tls->adapterIOQueueHandle = q_handle;
      return VMK_OK;
   }
   return VMK_EXISTS;
}
#endif /* !defined(__VMKLNX__) */
