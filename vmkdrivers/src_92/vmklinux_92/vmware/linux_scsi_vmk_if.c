/*
 * ****************************************************************
 * Portions Copyright 1998, 2010-2013 VMware, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * ****************************************************************/

/*
 * linux_scsi_vmk_if.c --
 *
 *      Interface functions that deal with vmkernel storage stack.
 */
#include <scsi/scsi.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_tcq.h>
#include <scsi/scsi_transport.h>
#include <scsi/scsi_transport_fc.h>
#include <scsi/scsi_transport_sas.h>
#include <scsi/libfc.h>
#include <vmklinux_92/vmklinux_scsi.h>

#include "vmkapi.h"
#include "linux_stubs.h" 

#include "linux_scsi.h"
#include "linux_scsi_transport.h"
#include "linux_stress.h"
#include "linux_stubs.h"

#define VMKLNX_LOG_HANDLE LinScsiVmk
#include "vmklinux_log.h"

/*
 *  Externs
 */
extern struct list_head	linuxBHCommandList;
extern vmk_SpinlockIRQ linuxSCSIBHLock;

/*
 *  Static
 */
static vmk_AdapterStatus SCSILinuxAdapterStatus(void *vmkAdapter);
static vmk_RescanLinkStatus SCSILinuxRescanLink(void *vmkAdapter);
static vmk_FcLinkSpeed FcLinuxLinkSpeed(void *vmkAdapter);
static vmk_uint64 FcLinuxAdapterPortName(void *vmkAdapter);
static vmk_uint64 FcLinuxAdapterNodeName(void *vmkAdapter);
static vmk_uint32 FcLinuxAdapterPortId(void *vmkAdapter);
static vmk_FcPortType FcLinuxAdapterPortType(void *vmkAdapter);
static vmk_FcPortState FcLinuxAdapterPortState(void *vmkAdapter);
static vmk_uint64 SasLinuxInitiatorAddress(void *pAdapter);
static void FcLinuxPopulateCB(vmk_FcAdapter *pFcAdapter);

static VMK_ReturnStatus FcLinuxTargetAttributes(void *clientData,
                                  vmk_FcTargetAttrs *vmkFcTarget,
                                  vmk_uint32 channelNo, vmk_uint32 targetNo);
static VMK_ReturnStatus FcLinuxAdapterAttributes(void *clientData,
                                  vmk_FcAdapterAttributes *adapterAttrib);
static VMK_ReturnStatus FcLinuxPortAttributes(void *clientData,
                                  vmk_uint32 portId,
                                  vmk_FcPortAttributes *portAttrib);
static VMK_ReturnStatus FcLinuxPortStatistics(void *clientData,
                                  vmk_FcPortStatistics *portStats);
static VMK_ReturnStatus FcLinuxPortReset(void *clientData, vmk_uint32 portId);

static VMK_ReturnStatus FcoeLinuxAdapterAttributes(void *clientData,
                                  vmk_FcoeAdapterAttrs *adapterAttrib);
static VMK_ReturnStatus FcoeLinuxPortStatistics(void *clientData,
                                  vmk_FcoePortStatistics *portStats);
static VMK_ReturnStatus FcoeLinuxPortReset(void *clientData, vmk_uint32 portId);

static VMK_ReturnStatus SasLinuxTargetAttributes(void *clientData,
                                  vmk_SasTargetAttrs *targetAttrib,
                                  vmk_uint32 channelNo, vmk_uint32 targetNo);
static VMK_ReturnStatus SasLinuxAdapterAttributes(void *clientData,
                                  vmk_SasAdapterAttributes *adapterAttrib);
static VMK_ReturnStatus SasLinuxPortAttributes(void *clientData,
                                  vmk_uint32 portId,
                                  vmk_SasPortAttributes *portAttrib);
static VMK_ReturnStatus SasLinuxPortStatistics(void *clientData,
                                  vmk_SasPortStatistics *portStats);
static VMK_ReturnStatus SasLinuxPortReset(void *clientData, vmk_uint32 portId);



/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxDiscover --
 *
 *      Create/Configure/Destroy a new paths
 *
 * Results:
 *      VMK_OK Success
 *      Other An error occured
 *      
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
VMK_ReturnStatus
SCSILinuxDiscover(void *clientData, vmk_ScanAction action,
		  int channel, int target, int lun,
		  void **deviceData)         
{
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter = 
		(struct vmklnx_ScsiAdapter *) clientData; 
   struct Scsi_Host    *sh = vmklnx26ScsiAdapter->shost;
   vmk_ScsiAdapter    *vmkAdapter = vmklnx26ScsiAdapter->vmkAdapter;
   struct scsi_device  *sdev;
   VMK_ReturnStatus status;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(sh);
   VMK_ASSERT(sh->transportt);

   switch (action) {
   case VMK_SCSI_SCAN_CREATE_PATH: {
      VMKLNX_DEBUG(5, "Create path %s:%d:%d:%d",
                      vmk_ScsiGetAdapterName(vmkAdapter), channel, target, lun);

      status = SCSILinuxCreatePath(sh, channel, target, lun, &sdev);

      if (status == VMK_OK) {
         break;
      } else {
         return status;
      }
      break;
   }

   case VMK_SCSI_SCAN_CONFIGURE_PATH: {
      VMKLNX_DEBUG(5, "Keep path %s:%d:%d:%d",
                      vmk_ScsiGetAdapterName(vmkAdapter), channel, target, lun);

      status = SCSILinuxConfigurePath(sh, channel, target, lun);

      return status;
   }

   case VMK_SCSI_SCAN_DESTROY_PATH: {
      VMKLNX_DEBUG(5, "Destroy path %s:%d:%d:%d",
                      vmk_ScsiGetAdapterName(vmkAdapter), channel, target, lun);

      status = SCSILinuxDestroyPath(sh, channel, target, lun);

      if (status == VMK_OK) {
	 sdev = NULL;
      } else {
	 return status;
      }
      break;
   }

   default:
      VMKLNX_WARN("Unknown path discover request %s:%d:%d:%d",
                  vmk_ScsiGetAdapterName(vmkAdapter), channel, target, lun);
      VMK_ASSERT(FALSE);
      return VMK_NOT_IMPLEMENTED;
   }

   *deviceData = (void *)sdev;

   return VMK_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxCommand --
 *
 *      Process a SCSI command.
 *
 * Results:
 *      VMK_OK if the IO was sent successfully.
 *      Error code otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
VMK_ReturnStatus
SCSILinuxCommand(void *clientData, vmk_ScsiCommand *vmkCmdPtr, 
		 void *deviceData)
{
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter = 
		(struct vmklnx_ScsiAdapter *) clientData; 
   struct Scsi_Host    *sh = vmklnx26ScsiAdapter->shost;
   struct scsi_device *sdev = (struct scsi_device *) deviceData;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if (sdev == NULL) {
      VMK_ASSERT(sdev);
      return VMK_FAILURE;
   }
   
   /* Never issue a command to the host ID */
   VMK_ASSERT(sdev->id != sh->this_id);

   if (VMKLNX_STRESS_DEBUG_COUNTER(stressScsiAdapterIssueFail)) {
      VMKLNX_DEBUG(1, "SCSI_ADAPTER_ISSUE_FAIL Stress Counter Triggered");
      return VMK_WOULD_BLOCK;
   }
   
   VMK_ASSERT((vmkCmdPtr->sgIOArray != NULL) ||
              (vmkCmdPtr->dataDirection == VMK_SCSI_COMMAND_DIRECTION_NONE) ||
              (vmkCmdPtr->requiredDataLen == 0));
   return SCSILinuxQueueCommand(sh, sdev, vmkCmdPtr);
}


/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxProcInfo --
 *
 *      Call proc handler for the given SCSI host adapter. 
 *
 * Results:
 *      VMK_OK on success,  VMK_FAILURE if proc handler fails.
 *      VMK_NOT_SUPPORTED if host template has no proc handler. 
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
VMK_ReturnStatus 
SCSILinuxProcInfo(void *clientData, 
                  char* buf, 
                  vmk_ByteCountSmall offset,
                  vmk_ByteCountSmall count,
                  vmk_ByteCountSmall *nbytes,
		  int isWrite)
{
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter = 
		(struct vmklnx_ScsiAdapter *) clientData; 
   struct Scsi_Host    *sh = vmklnx26ScsiAdapter->shost;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if (sh->hostt) {
      if (sh->hostt->proc_info) {
	 char *start = buf;
	 VMKLNX_DEBUG(5, "off=%d count=%d", offset, count);  

	 VMKAPI_MODULE_CALL(SCSI_GET_MODULE_ID(sh), *nbytes, sh->hostt->proc_info,
                    sh, buf, &start, offset, count, isWrite); 

	 VMK_ASSERT(start == buf || *nbytes == 0);

         if ((int32_t)*nbytes < 0) {
             VMKLNX_DEBUG(5, "failed %d", *nbytes);
             return VMK_FAILURE;
         }

	 /* 
          * We don't support proc nodes that generate one page of data or more
          */
	 VMK_ASSERT(*nbytes <= count);
	 VMKLNX_DEBUG(2, "nbytes=%d", *nbytes);  
	 return VMK_OK;
      } else if (!isWrite) {
	 if (sh->hostt->info) {
            const char *tmp_info;
            VMKAPI_MODULE_CALL(SCSI_GET_MODULE_ID(sh), tmp_info, sh->hostt->info, sh);
	    *nbytes = snprintf(buf, count, "%s\n", tmp_info);
	 } else {
	    *nbytes = snprintf(buf, count,
			       "The driver does not yet support the proc-fs\n");
	 }
	 buf[count - 1] = '\0';

	 /* 
          * We don't support proc nodes that generate one page of data or more
          */
	 VMK_ASSERT(*nbytes <= count);

	 if (*nbytes < offset) {
	    *nbytes = 0;
	 }
	 return VMK_OK;
      }
   }
   
   *nbytes = 0;
   return VMK_NOT_SUPPORTED;
}


/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxClose --
 *
 *      Close the SCSI device.
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
SCSILinuxClose(void *clientData)
{
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter = 
		(struct vmklnx_ScsiAdapter *) clientData; 
   struct Scsi_Host    *sh = vmklnx26ScsiAdapter->shost;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if (sh->hostt->release != NULL) {
      /*
       * This function should only be called if we are unloading the vmkernel.
       * The module cleanup code is going to release this scsi adapter 
       * also.  
       */
      VMKLNX_DEBUG(0, "Releasing adapter");
      VMKAPI_MODULE_CALL_VOID(SCSI_GET_MODULE_ID(sh), sh->hostt->release, sh);
   } else {
      VMKLNX_DEBUG(0, "No release function. Manually unload");
   }
}


/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxIoctl --
 *
 *      Special ioctl commands.
 *
 * Results:
 *      VMK_OK - success, (Driver retval in out arg)
 *      VMK_NOT_SUPPORTED - no ioctl() for this driver. 
 *      VMK_INVALID_TARGET - coudln't find device<target,lun>,
 *      
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

VMK_ReturnStatus
SCSILinuxIoctl(void *clientData,                 // IN:
               void *deviceData,                 // IN:
               uint32_t fileFlags,               // IN: ignored for SCSI ioctls
               uint32_t cmd,                     // IN: 
               vmk_VA userArgsPtr,               // IN/OUT:
               vmk_IoctlCallerSize callerSize,   // IN: ioctl caller size
               int32_t *drvErr)                  // OUT: error returned by driver.
{
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter = 
		(struct vmklnx_ScsiAdapter *) clientData; 
   struct Scsi_Host    *sh = vmklnx26ScsiAdapter->shost;
   struct scsi_device *sdev = (struct scsi_device *) deviceData;
   int done;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMKLNX_DEBUG(1, "start");

   if (sh == NULL
       || sh->hostt == NULL
       || sh->hostt->ioctl == NULL) {
      return VMK_NOT_SUPPORTED; /* most likely the third test failed. */
   }

   if (sdev == NULL) {
      return VMK_INVALID_TARGET; 
   }

   mutex_lock(&vmklnx26ScsiAdapter->ioctl_mutex);
   done = 0;
   if (callerSize == VMK_IOCTL_CALLER_32 && sh->hostt->compat_ioctl) {
      VMKAPI_MODULE_CALL(SCSI_GET_MODULE_ID(sh), *drvErr, sh->hostt->compat_ioctl,
                 sdev, cmd, (void *)userArgsPtr);
      if (*drvErr != -ENOIOCTLCMD) {
         done = 1;
      }
   }
   if (!done) {
      VMKAPI_MODULE_CALL(SCSI_GET_MODULE_ID(sh), *drvErr, sh->hostt->ioctl,
                 sdev, cmd, (void *)userArgsPtr);
   }
   mutex_unlock(&vmklnx26ScsiAdapter->ioctl_mutex);

   VMKLNX_DEBUG(1, "ret=%d", *drvErr); 

   return VMK_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxQueryDeviceQueueDepth
 *
 * Result:
 *     This returns the queue depth set by the device.
 *
 * Side effects:
 *     None
 *----------------------------------------------------------------------
 */
int
SCSILinuxQueryDeviceQueueDepth(void *clientData,
                               void *deviceData)
{
   struct scsi_device  *sdev = (struct scsi_device *) deviceData;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(sdev);

   /*
    * We pass in clientData (aka vmklnx_ScsiAdapter) just in case
    * we need it in the future.
    */
   clientData = clientData;

   return sdev ? sdev->queue_depth : 0;
}


/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxModifyDeviceQueueDepth
 *
 *     If possible set a new queue depth.
 *
 * Result:
 *     This returns the queue depth set by the device.
 *
 * Side effects:
 *     None
 *----------------------------------------------------------------------
 */
int
SCSILinuxModifyDeviceQueueDepth(void *clientData,
                                int qDepth,
                                void *deviceData)
{
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter = 
		(struct vmklnx_ScsiAdapter *) clientData; 
   struct Scsi_Host    *sh = vmklnx26ScsiAdapter->shost;
   struct scsi_device  *sdev = (struct scsi_device *) deviceData;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(sdev);

   if (sdev == NULL) {
      return 0; 
   }

   /*
    * Check if the drivers can set the new queue depth
    * else, return the current queue depth
    */
   if (sh->hostt->change_queue_depth) {
      int ret;

      /*
       * Most drivers will turn around and call scsi_adjust_queue_depth here
       * So be careful about reentrant code
       */
      VMKAPI_MODULE_CALL(SCSI_GET_MODULE_ID(sh), ret, sh->hostt->change_queue_depth,
              sdev, qDepth);
   }

   return sdev->queue_depth;
}


/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxDumpCommand --
 *
 *      Queue up a SCSI command when dumping.
 *
 * Results:
 *      VMK_OK - cmd queued;
 *      VMK_FAILURE - couldn't queue cmd;
 *      VMK_WOULD_BLOCK - device queue full or host not accepting cmds;
 *      VMK_BUSY - eh active. 
 *
 * Side effects:
 *      No one else can issue commands to this device anymore.
 *
 *----------------------------------------------------------------------
 */
VMK_ReturnStatus
SCSILinuxDumpCommand(void *clientData,
		     vmk_ScsiCommand *vmkCmdPtr, 
		     void *deviceData)
{
   struct vmklnx_ScsiAdapter *vmklnx26_ScsiAdapter = 
		(struct vmklnx_ScsiAdapter *) clientData; 
   struct Scsi_Host *sh = vmklnx26_ScsiAdapter->shost;
   struct scsi_cmnd *scmd;
   struct scsi_device *sdev = (struct scsi_device *) deviceData;
   unsigned long flags;
   int status = 0;
   void *dump_done_fn = SCSILinuxDumpCmdDone;
   static struct scsi_cmnd dump_scmd;
   struct scatterlist *sg;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(sdev);

   if (sdev == NULL) {
      VMKLNX_ALERT("Invalid sdev ptr during dump");
      return VMK_FAILURE; 
   }

   scmd = &dump_scmd;
   memset(scmd, 0, sizeof(*scmd));

   /*
    * Fill in values required for dump command
    */
   scmd->device = sdev;
   SCSILinuxInitCommand(sdev, scmd);
   scmd->done = dump_done_fn;
   if (scmd->device->tagged_supported) {
      scmd->tag = SIMPLE_QUEUE_TAG;
   } else {
      scmd->tag = 0;
   }

   /* Never issue a command to the host ID */
   VMK_ASSERT(sdev->id != sh->this_id);

   spin_lock_irqsave(sh->host_lock, flags);

   sdev->dumpActive = 1;

   /*
    * First, make sure dumpCmd can be queued. 
    */
   if (scsi_host_in_recovery(sh)) {
      VMKLNX_ALERT("In error recovery - %d", sh->shost_state);
   }

   if (sh->host_self_blocked 
       || (sdev->device_busy == sdev->queue_depth) 
       || (sh->host_busy == sh->can_queue)) {
      VMKLNX_DEBUG(0, "devcif=%u depth=%u hostcif=%u "
                      "can_queue=%d self_blocked=%u",
                      sdev->device_busy, sdev->queue_depth,
                      sh->host_busy, sh->can_queue,
                      sh->host_self_blocked);
      spin_unlock_irqrestore(sh->host_lock, flags);
      return VMK_WOULD_BLOCK;
   }

   ++scmd->device->host->host_busy;
   ++sdev->device_busy;
   spin_unlock_irqrestore(sh->host_lock, flags);

   memcpy(scmd->cmnd, vmkCmdPtr->cdb, vmkCmdPtr->cdbLen);

   /*
    * SCSI_OnPanic() could issue RELEASE command for clearing SCSI-2 reservations,
    * for which the sgArray is set to NULL.
    * Refer PR 827500.
    */
   if (vmkCmdPtr->sgArray) {
      /* 
       * Should never fail unless someone changes SCSI_Dump
       * We should only get one element here
       */
      VMK_ASSERT(vmkCmdPtr->sgArray->numElems == 1);
      sg = scmd->vmksg;
      VMKLNX_INIT_VMK_SG_WITH_ARRAYS(sg, vmkCmdPtr->sgArray, vmkCmdPtr->sgIOArray);
      VMK_ASSERT(vmkCmdPtr->sgIOArray->numElems == 1);
      scmd->use_sg = vmkCmdPtr->sgIOArray->numElems;
      scmd->request_bufflen = vmkCmdPtr->sgIOArray->elem[0].length;
      scmd->request_buffer = sg;
      scmd->request_bufferMA = 0;
      scmd->underflow = scmd->request_bufflen;
   } else {
      VMK_ASSERT(vmkCmdPtr->dataDirection == VMK_SCSI_COMMAND_DIRECTION_NONE);
   }
   scmd->cmd_len = COMMAND_SIZE(scmd->cmnd[0]);
   scmd->vmkCmdPtr = vmkCmdPtr;
   /*
    * A release command is a zero transfer length cmd, so DMA_NONE direction is fine.
    * PR# 781961
    */
   scmd->sc_data_direction = SCSILinuxGetDataDirection(scmd,
                                                       vmkCmdPtr->dataDirection);

   /*
    * Mark this as Dump Request
    * We don't need to hold vmk_lock, because
    *  - no one has access to scmd at this point other than us
    * Moreover,
    *  - dump path uses static data structures and can't be multithreaded
    *  - at this point, all the other CPUs have come to rest already
    */
   scmd->vmkflags = VMK_FLAGS_NEED_CMDDONE | VMK_FLAGS_DUMP_REQUEST;

   spin_lock_irqsave(sh->host_lock, flags);
   VMKAPI_MODULE_CALL(SCSI_GET_MODULE_ID(sh), status, sh->hostt->queuecommand, 
		      scmd, dump_done_fn);

   spin_unlock_irqrestore(sh->host_lock, flags);
   if (unlikely(status)) {
      VMKLNX_ALERT("queuecommand failed with status = 0x%x %s",
                   status, vmk_StatusToString(status));
      return VMK_FAILURE;
   } else {
      return VMK_OK;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxDummyDumpPoll --
 *
 *      This is a no-op function.
 *
 *      This poll handler is provided here to satisfy vmk_ScsiAdapter
 *      which requires a real function for the entry point 'dumpPollHandler'.
 *      Since each driver registers its own dump completion routine,
 *      the dumpPollHandler callback is no longer needed.
 *
 * Results:
 *      VMK_OK
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
void
SCSILinuxDummyDumpPoll(void *clientData)
{
   return;
}


/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxVportOp --
 * 
 * Entry point in vmklinux module for doing virtual link ops.
 * Converts arguments to the appropriate form for the NPIV aware
 * driver, and calls the driver's fc_transport NPIV handler entry
 * points as needed. The interface used is based on Linux 2.6.23.
 *
 * Results:
 *      VMK_OK - success, (Driver retval in out arg)
 *      VMK_NOT_SUPPORTED - no NPIV support for this driver. 
 *      VMK_FAILURE - for all driver related errors
 *
 * Side effects:
 *      When executing the VMK_VPORT_CREATE command, the driver will
 *      call scsi_add_host() to add the VPORT.
 *      When executing the VMK_VPORT_DELETE command, the driver will
 *      call scsi_remove_host() to remove the VPORT.
 *
 *----------------------------------------------------------------------
 */

VMK_ReturnStatus
SCSILinuxVportOp(void *clientData,    // IN:
		 uint32_t cmd,        // IN: 
		 void *args)          // IN/OUT:
{
   struct Scsi_Host *shp, *shost;
   vmk_ScsiVportArgs *vmkargs;
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter;
   vmk_ScsiAdapter *vmkAdapter;
   int drvErr;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   vmkAdapter = (vmk_ScsiAdapter *)clientData;
   VMK_ASSERT(vmkAdapter);
   vmklnx26ScsiAdapter = (struct vmklnx_ScsiAdapter *)vmkAdapter->clientData;
   VMK_ASSERT(vmklnx26ScsiAdapter);
   shp = vmklnx26ScsiAdapter->shost;
   VMK_ASSERT(shp);
   vmkargs = (vmk_ScsiVportArgs *) args;
   VMK_ASSERT(vmkargs);

   VMKLNX_DEBUG(5, "SCSILinuxVportOp: client=%p, host=%p, cmd=0x%x, uargs=%p", 
                   clientData, shp, cmd, (void *)vmkargs);

   if (shp == NULL
       || shp->transportt == NULL ||
       shp->adapter == NULL) {
      return VMK_NOT_SUPPORTED;
   }

   // need further checks for NPIV to work
   if (vmkAdapter->mgmtAdapter.transport != VMK_STORAGE_ADAPTER_FC) {
      return VMK_NOT_SUPPORTED;
   }

   switch (cmd) {
   case VMK_VPORT_CREATE:
      {
      struct   vmk_ScsiAdapter   *aPhys, *aVport;
      void *vport_shost = NULL;

      VMKLNX_DEBUG(2, "SCSILinuxVportOp: VMK_VPORT_CREATE: args=%p",
                      (void *)vmkargs);

      drvErr = vmk_fc_vport_create(shp, scsi_get_device(shp), vmkargs, &vport_shost);
      if (drvErr) {
         goto handle_driver_error;
      }

      aPhys = vmklnx26ScsiAdapter->vmkAdapter;  // Physical hba
      shost = (struct Scsi_Host *)vport_shost;
      vmklnx26ScsiAdapter = shost->adapter; // Vport's lnxscsi adapter
      aVport = vmklnx26ScsiAdapter->vmkAdapter;  // Vport hba
      vmkargs->virtAdapter = aVport;
      
      // set the pae flag to match the physical hba
      aVport->paeCapable = aPhys->paeCapable;

      // set the sg fields from the physical hba
      aVport->constraints.sgElemSizeMult = aPhys->constraints.sgElemSizeMult;
      aVport->constraints.sgElemAlignment = aPhys->constraints.sgElemAlignment;

      scsi_scan_host(shost);

      }
      break;
   case VMK_VPORT_DELETE:
      vmkAdapter = vmkargs->virtAdapter;
      vmklnx26ScsiAdapter = (struct vmklnx_ScsiAdapter *)vmkAdapter->clientData;
      VMK_ASSERT(vmklnx26ScsiAdapter);
      shost = vmklnx26ScsiAdapter->shost;
      VMKLNX_DEBUG(2, "SCSILinuxVportOp: VMK_VPORT_DELETE: virthost=%p, shp=%p",
                      (void *)vmkargs->virtAdapter, (void *)shp);
      drvErr = vmk_fc_vport_delete(shost);
      break;
   case VMK_VPORT_INFO:
      {
      VMKLNX_DEBUG(5, "SCSILinuxVportOp: VMK_VPORT_INFO: host=%p, info=%p",
                      (void *)shp, (void *)vmkargs->info);
      // the returned structure is already allocated by the vmkernel
      drvErr = vmk_fc_vport_getinfo(shp, vmkargs->info);
      VMKLNX_DEBUG(5, "SCSILinuxVportOp: drvErr %x", drvErr);
      }
      break;
   case VMK_VPORT_SUSPEND:
      vmkAdapter = vmkargs->virtAdapter;
      vmklnx26ScsiAdapter = (struct vmklnx_ScsiAdapter *)vmkAdapter->clientData;
      VMK_ASSERT(vmklnx26ScsiAdapter);
      shost = vmklnx26ScsiAdapter->shost;
      VMKLNX_DEBUG(2, "SCSILinuxVportOp: VMK_VPORT_SUSPEND:"
                      " virthost=%p, shp=%p, flags=%x",
                      (void *)vmkargs->virtAdapter, (void *)shp, vmkargs->flags);
      drvErr = vmk_fc_vport_suspend(shost, (vmkargs->flags & VMK_SCSI_VPORT_FLAG_SUSPEND));
      break;
   default:
      return VMK_NOT_SUPPORTED;
   }

handle_driver_error:
   switch(drvErr) {
      case  0:
         return VMK_OK;
      case  -ENOENT:
         // NPIV not support by this driver
         return VMK_NOT_SUPPORTED;
      case  -ENOMEM:
         // out of memory
         return VMK_NOT_SUPPORTED;
      case  -ENOSPC:
         // no more vports
         return VMK_NOT_SUPPORTED;
      default:
         return VMK_FAILURE;
   }
 
   return VMK_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxVportUpdate
 *
 * Stub called by scsi_add_host to handle updating the vport with
 * scsi host data
 *
 *----------------------------------------------------------------------
 */
void SCSILinuxVportUpdate(void *clientdata, void *device)
{
   struct Scsi_Host *sh;
   struct device *dev;
   struct fc_vport *vport;

   sh = (struct Scsi_Host *) clientdata;
   dev = (struct device *)device;

   vport = dev_to_vport(dev);
   vport->vhost = sh;
   if (vport->flags & FC_VPORT_LEGACY) {
      // set the vmkadapter flag
      ((vmk_ScsiAdapter*)sh->vmkAdapter)->flags |=
            VMK_SCSI_ADAPTER_FLAG_NPIV_LEGACY_VPORT;
   }
}

#define VPORT_DISCOVERY_RETRY_COUNT 10
#define VPORT_DISCOVERY_DELAY_COUNT 2000000   // delay in us = 2 sec

/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxVportDiscover
 *
 * Special front-end for SCSIScanPaths used to scan LUNs on a VPORT and
 * delete vport related paths
 *
 * Result:
 *    See SCSIScanPaths
 *
 * Side Effects:
 *    Creates the LUN path if it is not found
 *    vmkAdapter is set to the correct pointer
 *    deviceData is set to the scsi_device pointer
 *
 *----------------------------------------------------------------------
 */
VMK_ReturnStatus 
SCSILinuxVportDiscover(void *clientData, vmk_ScanAction action,
                       int ctlr, int devNum, int lun,
                       vmk_ScsiAdapter **vmkAdapter,
                       void **deviceData)
{
   struct Scsi_Host *hostPtr;
   vmk_ScsiAdapter *adapter;
   struct scsi_device  *dp;
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter;
   VMK_ReturnStatus status;
   uint32_t retryCount = 0;
   unsigned long flags;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   adapter = (vmk_ScsiAdapter *)clientData;
   VMK_ASSERT(adapter);
   vmklnx26ScsiAdapter = (struct vmklnx_ScsiAdapter *)adapter->clientData;
   VMK_ASSERT(vmklnx26ScsiAdapter);
   hostPtr = vmklnx26ScsiAdapter->shost;
   VMK_ASSERT(hostPtr);

   switch (action) {
   case VMK_SCSI_SCAN_CREATE_PATH: {

      VMKLNX_DEBUG(2, "SCSILinuxVportDiscover: client=%p, host=%p, adapter=%p", 
                      clientData, hostPtr, adapter);

      VMKLNX_DEBUG(1, "VPORT Adapter name=%s [%p]", vmk_NameToString(&adapter->name),
                      adapter); 
      if (vmkAdapter) {
         *vmkAdapter = adapter;
      }

      /*
       * scsi_device_lookup takes a reference on the scsi_device and that
       * reference is released as part of VMK_SCSI_SCAN_DESTROY_PATH for
       * vport devices
       */
      if ((devNum != VMK_SCSI_PATH_ANY_TARGET) && (lun != VMK_SCSI_PATH_ANY_LUN)) {
         dp = scsi_device_lookup(hostPtr, ctlr, devNum, lun);
         if (dp) {
            if (deviceData) {
               *deviceData = dp;
            }
            // don't scan if the lun already exist
            return VMK_OK;
         }
      }

      vmk_WorldAssertIsSafeToBlock();

      while (((status = vmk_ScsiScanPaths(&adapter->name,
                                          ctlr, devNum, lun)) ==
              VMK_BUSY) && (retryCount++ < VPORT_DISCOVERY_RETRY_COUNT)) {
         vmk_WorldSleep(VPORT_DISCOVERY_DELAY_COUNT);
         VMKLNX_DEBUG(1, "Retrying vmk_ScsiScanPaths %dth" 
                         " time on VPORT Adapter name=%s [%p]",
                         retryCount, vmk_NameToString(&adapter->name),
                         adapter);
      }

      if ((status == VMK_BUSY) && (retryCount >= VPORT_DISCOVERY_RETRY_COUNT)) {
         VMKLNX_WARN("Failed Discovery on VPORT Adapter name=%s [%p] with error: %d", 
                     vmk_NameToString(&adapter->name), adapter, status);
         return status;
      }

      if ((devNum != VMK_SCSI_PATH_ANY_TARGET) && (lun != VMK_SCSI_PATH_ANY_LUN)) {
         // If this is for a specific LUN/TARGET, see if LUN is really found
         dp = scsi_device_lookup(hostPtr, ctlr, devNum, lun);
         if (dp) {
            if (deviceData) {
               *deviceData = dp;
            }
            return VMK_OK;
         }
      }
      else {
         return VMK_OK;
      }

      // LUN was not found, return failure
      return VMK_NOT_FOUND;
   }

   case VMK_SCSI_SCAN_DESTROY_PATH: {
      VMKLNX_DEBUG(2, "Destroy vport path %s:%d:%d:%d deviceData=%p",
                      vmk_NameToString(&adapter->name), ctlr, devNum,
                      lun, *deviceData);

      spin_lock_irqsave(hostPtr->host_lock, flags);
      dp = __scsi_device_lookup(hostPtr, ctlr, devNum, lun);
      spin_unlock_irqrestore(hostPtr->host_lock, flags);
      if (dp && (*deviceData != NULL)) {
         /*
          * Release reference that was taken while doing SCSI_SCAN_CREATE_PATH
          * operation as part of vport discovery.
          */
         scsi_device_put(dp);
      }
      status = SCSILinuxDestroyPath(hostPtr, ctlr, devNum, lun);
      return status;
      break;
   }

   case VMK_SCSI_SCAN_CONFIGURE_PATH:
   // This is not used, added to make compiler happy
   default:
      VMKLNX_WARN("Unknown path discover request %s:%d:%d:%d",
                  vmk_NameToString(&adapter->name), ctlr, devNum, lun);
      VMK_ASSERT(FALSE);
      return VMK_NOT_IMPLEMENTED;
   }

   // Return failure if we reach here
   return VMK_FAILURE;
}


/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxTaskMgmt --
 *
 *      Process a task managemnet request.
 *
 * Results:
 *      VMK_OK
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
VMK_ReturnStatus
SCSILinuxTaskMgmt(void *clientData, vmk_ScsiTaskMgmt *vmkTaskMgmt,
		  void *deviceData) 
{
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter = 
		(struct vmklnx_ScsiAdapter *) clientData; 
   struct Scsi_Host    *sh = vmklnx26ScsiAdapter->shost;
   struct scsi_device *sdev = (struct scsi_device *) deviceData;
   VMK_ReturnStatus status = VMK_FAILURE;
   unsigned long flags;
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(sdev);

   /* Never issue a command to the host ID */
   VMK_ASSERT(sdev->id != sh->this_id);

   VMKLNX_DEBUG(4, "Start processing %s of "
                   "initiator:%p S/N:%#"VMK_FMT64"x on "
                   SCSI_PATH_NAME_FMT,
                   vmk_ScsiGetTaskMgmtTypeName(vmkTaskMgmt->type),
                   vmkTaskMgmt->cmdId.initiator, vmkTaskMgmt->cmdId.serialNumber,
                   SCSI_PATH_NAME(sh, sdev->channel, sdev->id, sdev->lun));

   switch (vmkTaskMgmt->type) {
   case VMK_SCSI_TASKMGMT_ABORT:
      status = SCSILinuxAbortCommands(sh, sdev, vmkTaskMgmt);
      break;

   case VMK_SCSI_TASKMGMT_LUN_RESET:
   case VMK_SCSI_TASKMGMT_DEVICE_RESET:
   case VMK_SCSI_TASKMGMT_BUS_RESET:
       /* 
        * Set the state to recovery, some of the drivers dont like to
        * receive commands at this time. See PR236958 for more info
        */
       atomic_inc(&vmklnx26ScsiAdapter->tmfFlag); 

       spin_lock_irqsave(sh->host_lock, flags);
       sh->shost_state = SHOST_RECOVERY;
       spin_unlock_irqrestore(sh->host_lock, flags);

      /*
       * Destructive reset
       */
      status = SCSILinuxResetCommand(sh, sdev, vmkTaskMgmt);
      /* 
       * Set the state back to normal 
       */
      if (atomic_dec_and_test(&vmklnx26ScsiAdapter->tmfFlag)) {

         spin_lock_irqsave(sh->host_lock, flags);
         sh->shost_state = SHOST_RUNNING;
         spin_unlock_irqrestore(sh->host_lock, flags);

      }
      break;

   case VMK_SCSI_TASKMGMT_VIRT_RESET:
      status = SCSILinuxAbortCommands(sh, sdev, vmkTaskMgmt);
      break;

   default:
      VMKLNX_WARN("Unknown task management type 0x%x", vmkTaskMgmt->type);
      VMK_ASSERT(FALSE);
   }

   VMKLNX_DEBUG(4, "Done");

   return status;
}

/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxDumpQueue --
 *
 *      Dump Queue details.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
void
SCSILinuxDumpQueue(void *clientData)
{
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter = 
		(struct vmklnx_ScsiAdapter *) clientData; 
   struct Scsi_Host    *sh = vmklnx26ScsiAdapter->shost;
   struct scsi_device *sdev;
   struct scsi_cmnd *scmd;
   unsigned long flags, cmdFlags;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(sh);

   spin_lock_irqsave(sh->host_lock, flags);
   shost_for_each_device(sdev, sh) {
      spin_lock_irqsave(&sdev->list_lock, cmdFlags);
      list_for_each_entry(scmd, &sdev->cmd_list, list) {
         VMKLNX_DEBUG(3, "%p", scmd);
      }
      spin_unlock_irqrestore(&sdev->list_lock, cmdFlags);
   }
   spin_unlock_irqrestore(sh->host_lock, flags);
}

/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxCheckTarget --
 *
 *      Check if a Target exists
 *
 * Results:
 *      VMK_OK if the Target exists.
 *      Error code otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
VMK_ReturnStatus
SCSILinuxCheckTarget(void *clientData,
      int channelNo, int targetNo)
{
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter = 
		(struct vmklnx_ScsiAdapter *) clientData; 
   struct Scsi_Host    *shost;
   unsigned long        flags;
   struct scsi_target  *sTarget;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(vmklnx26ScsiAdapter);

   shost = vmklnx26ScsiAdapter->shost;
   VMK_ASSERT(shost);

   spin_lock_irqsave(shost->host_lock, flags);
   sTarget = vmklnx_scsi_find_target(shost, channelNo, targetNo);
   spin_unlock_irqrestore(shost->host_lock, flags);

   if (!sTarget) {
      VMKLNX_DEBUG(5, "No Valid targets bound (channel %d, target %d)",
                      channelNo, targetNo);
      return VMK_FAILURE;
   }

   return VMK_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * FcLinuxLinkSpeed
 *
 *     Get FC Link Speed
 *
 * Result:
 *
 *     VMK_FC_SPEED_UNKNOWN
 *     VMK_FC_SPEED_1GBIT
 *     VMK_FC_SPEED_2GBIT
 *     VMK_FC_SPEED_4GBIT
 *     VMK_FC_SPEED_8GBIT
 *     VMK_FC_SPEED_10GBIT
 *     VMK_FC_SPEED_16GBIT
 *----------------------------------------------------------------------
 */
static vmk_FcLinkSpeed
FcLinuxLinkSpeed(void *clientData)
{
   vmk_FcLinkSpeed speed = VMK_FC_SPEED_UNKNOWN;
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter = 
		(struct vmklnx_ScsiAdapter *) clientData; 
   struct fc_internal *i;
   struct fc_host_attrs *fc_host;
   struct Scsi_Host    *shost;

   VMK_ASSERT(vmklnx26ScsiAdapter);

   shost = vmklnx26ScsiAdapter->shost;
   VMK_ASSERT(shost);

   fc_host = shost_to_fc_host(shost);
   VMK_ASSERT(fc_host);

   i = to_fc_internal(shost->transportt);
   VMK_ASSERT(i);

   if(i->f->get_host_speed) {
      VMKAPI_MODULE_CALL_VOID(SCSI_GET_MODULE_ID(shost), 
		i->f->get_host_speed, shost);

      switch (fc_host->speed) {
         case FC_PORTSPEED_1GBIT:
            speed = VMK_FC_SPEED_1GBIT;
            break;
         case FC_PORTSPEED_2GBIT:
            speed = VMK_FC_SPEED_2GBIT;
            break;
         case FC_PORTSPEED_4GBIT:
            speed = VMK_FC_SPEED_4GBIT;
            break;
         case FC_PORTSPEED_10GBIT:
            speed = VMK_FC_SPEED_10GBIT;
            break;
         case FC_PORTSPEED_8GBIT:
            speed = VMK_FC_SPEED_8GBIT;
            break;
         case FC_PORTSPEED_16GBIT:
            speed = VMK_FC_SPEED_16GBIT;
            break;
         case FC_PORTSPEED_NOT_NEGOTIATED:
         case FC_PORTSPEED_UNKNOWN:
         default:
            speed = VMK_FC_SPEED_UNKNOWN;
      } 
   }
   return speed;
}

/*
 *----------------------------------------------------------------------
 *
 * FcLinuxSupportedSpeed
 *
 *     Convert FC port supported speed from linux format to vmkApi format
 *
 * Result:
 *
 *     ORed value of type vmk_FcLinkSpeedBit bits
 *----------------------------------------------------------------------
 */
static vmk_FcLinkSpeedBit
FcLinuxSupportedSpeed(u32 vmklnx_SupportedSpeed)
{
   vmk_FcLinkSpeedBit vmkSupportedSpeed = 0;

   if (vmklnx_SupportedSpeed & FC_PORTSPEED_1GBIT)
      vmkSupportedSpeed |= VMK_FC_SPEED_BIT_1GBIT;
   if (vmklnx_SupportedSpeed & FC_PORTSPEED_2GBIT)
      vmkSupportedSpeed |= VMK_FC_SPEED_BIT_2GBIT;
   if (vmklnx_SupportedSpeed & FC_PORTSPEED_4GBIT)
      vmkSupportedSpeed |= VMK_FC_SPEED_BIT_4GBIT;
   if (vmklnx_SupportedSpeed & FC_PORTSPEED_8GBIT)
      vmkSupportedSpeed |= VMK_FC_SPEED_BIT_8GBIT;
   if (vmklnx_SupportedSpeed & FC_PORTSPEED_10GBIT)
      vmkSupportedSpeed |= VMK_FC_SPEED_BIT_10GBIT;
   if (vmklnx_SupportedSpeed & FC_PORTSPEED_16GBIT)
      vmkSupportedSpeed |= VMK_FC_SPEED_BIT_16GBIT;

   return vmkSupportedSpeed;
}

/*
 *----------------------------------------------------------------------
 *
 * FcLinuxAdapterPortName
 *
 *     Get FC Adapter Port Name
 *
 * Result:
 *     Return 64 bit port name
 *----------------------------------------------------------------------
 */
static vmk_uint64
FcLinuxAdapterPortName(void *clientData)
{
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter = 
		(struct vmklnx_ScsiAdapter *) clientData; 
   struct fc_host_attrs *fc_host;
   struct Scsi_Host    *shost;

   VMK_ASSERT(vmklnx26ScsiAdapter);

   shost = vmklnx26ScsiAdapter->shost;
   VMK_ASSERT(shost);

   fc_host = shost_to_fc_host(shost);
   VMK_ASSERT(fc_host);

   return (fc_host->port_name);
}

/*
 *----------------------------------------------------------------------
 *
 * FcLinuxAdapterPortId
 *
 *     Get FC Adapter Port Id
 *
 * Result:
 *     Return 32 bit port id
 *----------------------------------------------------------------------
 */
static vmk_uint32
FcLinuxAdapterPortId(void *clientData)
{
   vmk_uint32 portId = 0;
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter = 
		(struct vmklnx_ScsiAdapter *) clientData; 
   struct fc_host_attrs *fc_host;
   struct fc_internal *i;
   struct Scsi_Host    *shost;

   VMK_ASSERT(vmklnx26ScsiAdapter);

   shost = vmklnx26ScsiAdapter->shost;
   VMK_ASSERT(shost);

   fc_host = shost_to_fc_host(shost);
   VMK_ASSERT(fc_host);

   i = to_fc_internal(shost->transportt);
   VMK_ASSERT(i);

   if (i->f->get_host_port_id) {
      VMKAPI_MODULE_CALL_VOID(SCSI_GET_MODULE_ID(shost), 
		i->f->get_host_port_id, shost);
      portId = fc_host->port_id;
   }

   return portId;
}

/*
 *----------------------------------------------------------------------
 *
 * FcLinuxAdapterNodeName
 *
 *     Get FC Adapter Node Name
 *
 * Result:
 *     Return 64 bit node name
 *----------------------------------------------------------------------
 */
static vmk_uint64
FcLinuxAdapterNodeName(void *clientData)
{
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter = 
		(struct vmklnx_ScsiAdapter *) clientData; 
   struct fc_host_attrs *fc_host;
   struct Scsi_Host    *shost;

   VMK_ASSERT(vmklnx26ScsiAdapter);

   shost = vmklnx26ScsiAdapter->shost;
   VMK_ASSERT(shost);

   fc_host = shost_to_fc_host(shost);
   VMK_ASSERT(fc_host);

   return (fc_host->node_name);
}

/*
 *----------------------------------------------------------------------
 *
 * FcLinuxAdapterPortType
 *
 *     Get FC Adapter Port Type
 *
 * Result:
 *     VMK_FC_PORTTYPE_UNKNOWN
 *     VMK_FC_PORTTYPE_OTHER
 *     VMK_FC_PORTTYPE_NOTPRESENT
 *     VMK_FC_PORTTYPE_NPORT		
 *     VMK_FC_PORTTYPE_NLPORT	
 *     VMK_FC_PORTTYPE_LPORT
 *     VMK_FC_PORTTYPE_PTP
 *----------------------------------------------------------------------
 */
static vmk_FcPortType
FcLinuxAdapterPortType(void *clientData)
{
   vmk_FcPortType portType = VMK_FC_PORTTYPE_UNKNOWN;
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter = 
		(struct vmklnx_ScsiAdapter *) clientData; 
   struct fc_internal *i;
   struct fc_host_attrs *fc_host;
   struct Scsi_Host    *shost;

   VMK_ASSERT(vmklnx26ScsiAdapter);

   shost = vmklnx26ScsiAdapter->shost;
   VMK_ASSERT(shost);

   fc_host = shost_to_fc_host(shost);
   VMK_ASSERT(fc_host);

   i = to_fc_internal(shost->transportt);
   VMK_ASSERT(i);

   if (i->f->get_host_port_type) {
      VMKAPI_MODULE_CALL_VOID(SCSI_GET_MODULE_ID(shost), 
	i->f->get_host_port_type, shost);

      switch (fc_host->port_type) {
         case FC_PORTTYPE_OTHER:
            portType = VMK_FC_PORTTYPE_OTHER;
            break;
         case FC_PORTTYPE_NOTPRESENT:
            portType = FC_PORTTYPE_NOTPRESENT;
            break;
         case FC_PORTTYPE_NPORT:
            portType = VMK_FC_PORTTYPE_NPORT;
            break;
         case FC_PORTTYPE_NLPORT:
            portType = VMK_FC_PORTTYPE_NLPORT;
            break;
         case FC_PORTTYPE_LPORT:
            portType = VMK_FC_PORTTYPE_LPORT;
            break;
         case FC_PORTTYPE_PTP:
            portType = VMK_FC_PORTTYPE_PTP;
            break;
         case FC_PORTTYPE_NPIV:
            portType = VMK_FC_PORTTYPE_NPIV;
            break;
         case FC_PORTTYPE_UNKNOWN:
         default:
            portType = VMK_FC_PORTTYPE_UNKNOWN;
      } 
   }
   return portType;
}

/*
 *----------------------------------------------------------------------
 *
 * FcLinuxAdapterPortState
 *
 *     Get FC Adapter Port State
 *
 * Result:
 *     VMK_FC_PORTSTATE_UNKNOWN
 *     VMK_FC_PORTSTATE_NOTPRESENT
 *     VMK_FC_PORTSTATE_ONLINE
 *     VMK_FC_PORTSTATE_OFFLINE		
 *     VMK_FC_PORTSTATE_BLOCKED
 *     VMK_FC_PORTSTATE_BYPASSED
 *     VMK_FC_PORTSTATE_DIAGNOSTICS
 *     VMK_FC_PORTSTATE_LINKDOWN
 *     VMK_FC_PORTSTATE_ERROR
 *     VMK_FC_PORTSTATE_LOOPBACK
 *     VMK_FC_PORTSTATE_DELETED
 *----------------------------------------------------------------------
 */
static vmk_FcPortState
FcLinuxAdapterPortState(void *clientData)
{
   vmk_FcPortType portState = VMK_FC_PORTSTATE_UNKNOWN;
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter = 
		(struct vmklnx_ScsiAdapter *) clientData; 
   struct fc_internal *i;
   struct fc_host_attrs *fc_host;
   struct Scsi_Host    *shost;

   VMK_ASSERT(vmklnx26ScsiAdapter);

   shost = vmklnx26ScsiAdapter->shost;
   VMK_ASSERT(shost);

   fc_host = shost_to_fc_host(shost);
   VMK_ASSERT(fc_host);

   i = to_fc_internal(shost->transportt);
   VMK_ASSERT(i);

   if (i->f->get_host_port_state) {
      VMKAPI_MODULE_CALL_VOID(SCSI_GET_MODULE_ID(shost), 
		i->f->get_host_port_state, shost);

      switch (fc_host->port_state) {
         case FC_PORTSTATE_NOTPRESENT:
            portState = VMK_FC_PORTSTATE_NOTPRESENT;
            break;
         case FC_PORTSTATE_ONLINE:
            portState = VMK_FC_PORTSTATE_ONLINE;
            break;
         case FC_PORTSTATE_OFFLINE:
            portState = VMK_FC_PORTSTATE_OFFLINE;
            break;
         case FC_PORTSTATE_BLOCKED:
            portState = VMK_FC_PORTSTATE_BLOCKED;
            break;
         case FC_PORTSTATE_BYPASSED:
            portState = VMK_FC_PORTSTATE_BYPASSED;
            break;
         case FC_PORTSTATE_DIAGNOSTICS:
            portState = VMK_FC_PORTSTATE_DIAGNOSTICS;
            break;
         case FC_PORTSTATE_LINKDOWN:
            portState = VMK_FC_PORTSTATE_LINKDOWN;
            break;
         case FC_PORTSTATE_ERROR:
            portState = VMK_FC_PORTSTATE_ERROR;
            break;
         case FC_PORTSTATE_LOOPBACK:
            portState = VMK_FC_PORTSTATE_LOOPBACK;
            break;
         case FC_PORTSTATE_DELETED:
            portState = VMK_FC_PORTSTATE_DELETED;
            break;
         case FC_PORTSTATE_UNKNOWN:
         default:
            portState = VMK_FC_PORTSTATE_UNKNOWN;
      } 
   }
   return portState;
}

/*
 *----------------------------------------------------------------------
 *
 * FcLinuxTargetAttributes
 *
 *     Get FC Target Attributes
 *
 * Result:
 *     Return FC Transport information for the specified target
 *----------------------------------------------------------------------
 */
static VMK_ReturnStatus
FcLinuxTargetAttributes(void *clientData, vmk_FcTargetAttrs *vmkFcTarget, 
vmk_uint32 channelNo, vmk_uint32 targetNo)
{
   struct scsi_target *sTarget;
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter = 
		(struct vmklnx_ScsiAdapter *) clientData; 
   struct Scsi_Host    *shost;
   struct fc_internal *i; 
   unsigned long flags;
   struct fc_rport *rport;

   VMK_ASSERT(vmklnx26ScsiAdapter);

   shost = vmklnx26ScsiAdapter->shost;
   VMK_ASSERT(shost);

   i = to_fc_internal(shost->transportt);
   VMK_ASSERT(i);

   spin_lock_irqsave(shost->host_lock, flags);
   sTarget = vmklnx_scsi_find_target(shost, channelNo, targetNo);
   spin_unlock_irqrestore(shost->host_lock, flags);

   if (!sTarget) {
      VMKLNX_DEBUG(0, "No Valid FC targets bound");
      return VMK_FAILURE;
   }

   rport = starget_to_rport(sTarget); 
   if (!rport) {
      VMKLNX_DEBUG(0, "No Valid rports found");
      return VMK_FAILURE;
   }

   vmkFcTarget->nodeName = rport->node_name;
   vmkFcTarget->portName = rport->port_name;
   vmkFcTarget->portId = rport->port_id;

   return VMK_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * FcLinuxAdapterAttributes
 *
 *     Get FC Adapter Attributes
 *
 * Result:
 *     Return FC adapter information for the specified HBA
 *----------------------------------------------------------------------
 */
static VMK_ReturnStatus
FcLinuxAdapterAttributes(void *clientData, vmk_FcAdapterAttributes *attrib)
{
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter =
                (struct vmklnx_ScsiAdapter *) clientData;
   struct fc_host_attrs *fc_host;
   struct Scsi_Host    *shost;

   VMK_ASSERT(attrib);

   VMK_ASSERT(vmklnx26ScsiAdapter);

   shost = vmklnx26ScsiAdapter->shost;
   VMK_ASSERT(shost);

   fc_host = shost_to_fc_host(shost);
   VMK_ASSERT(fc_host);

   vmk_Strncpy(attrib->serialNumber, fc_host_serial_number(shost), FC_SERIAL_NUMBER_SIZE);
   vmk_Strncpy(attrib->nodeSymbolicName, fc_host_symbolic_name(shost), FC_SYMBOLIC_NAME_SIZE);

   return VMK_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * FcLinuxPortAttributes
 *
 *     Get FC Port Attributes
 *
 * Result:
 *     Return FC port information for the specified HBA port
 *----------------------------------------------------------------------
 */
static VMK_ReturnStatus
FcLinuxPortAttributes(void *clientData, vmk_uint32 portId, vmk_FcPortAttributes *portAttrib)
{
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter = 
		(struct vmklnx_ScsiAdapter *) clientData; 
   struct Scsi_Host *shost;
   struct fc_internal *i;
   struct fc_function_template *ft;
   struct fc_lport *lport;

   VMK_ASSERT(portAttrib);

   VMK_ASSERT(vmklnx26ScsiAdapter);

   shost = vmklnx26ScsiAdapter->shost;
   VMK_ASSERT(shost);

   i = to_fc_internal(shost->transportt);
   VMK_ASSERT(i);

   ft = i->f;
   if (ft == NULL) {
      return VMK_NOT_FOUND;
   }

   if (ft->get_host_port_id) {
      VMKAPI_MODULE_CALL_VOID(SCSI_GET_MODULE_ID(shost), ft->get_host_port_id, shost);
   }

   if (ft->get_host_speed) {
      VMKAPI_MODULE_CALL_VOID(SCSI_GET_MODULE_ID(shost), ft->get_host_speed, shost);
   }

   lport = shost_priv(shost);
   memset(portAttrib, 0, sizeof(vmk_FcPortAttributes));

   portAttrib->nodeWWN = fc_host_node_name(shost);
   portAttrib->portWWN = fc_host_port_name(shost);
   portAttrib->portFcId = fc_host_port_id(shost);
   portAttrib->portType = fc_host_port_type(shost);
   portAttrib->portState = fc_host_port_state(shost);
   portAttrib->portSupportedClassOfService = fc_host_supported_classes(shost);
   memcpy(portAttrib->portSupportedFc4Types, fc_host_supported_fc4s(shost), FC_FC4_LIST_SIZE);
   memcpy(portAttrib->portActiveFc4Types, fc_host_active_fc4s(shost), FC_FC4_LIST_SIZE);
   memcpy(portAttrib->portSymbolicName, fc_host_symbolic_name(shost), FC_SYMBOLIC_NAME_SIZE);
   memcpy(portAttrib->osDeviceName, fc_host_system_hostname(shost), FC_SYMBOLIC_NAME_SIZE);
   portAttrib->portSupportedSpeed = FcLinuxSupportedSpeed(lport->link_supported_speeds);
   portAttrib->portSpeed = FcLinuxLinkSpeed(clientData);
   portAttrib->portMaxFrameSize = fc_host_maxframe_size(shost);
   portAttrib->fabricName = fc_host_fabric_name(shost);

   return VMK_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * FcLinuxPortStatistics
 *
 *     Get FC IO Statistics
 *
 * Result:
 *     Return FC IO statistics information for the specified HBA
 *----------------------------------------------------------------------
 */
static VMK_ReturnStatus
FcLinuxPortStatistics(void *clientData, vmk_FcPortStatistics *portStats)
{
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter = 
		(struct vmklnx_ScsiAdapter *) clientData; 
   struct Scsi_Host *shost;
   struct fc_internal *i;
   struct fc_function_template *ft;
   struct fc_host_statistics *fc_stat;

   VMK_ASSERT(portStats);

   VMK_ASSERT(vmklnx26ScsiAdapter);

   shost = vmklnx26ScsiAdapter->shost;
   VMK_ASSERT(shost);

   i = to_fc_internal(shost->transportt);
   VMK_ASSERT(i);

   ft = i->f;
   if (ft == NULL) {
      return VMK_NOT_FOUND;
   }

   if (ft->get_fc_host_stats == NULL) {
      return VMK_UNDEFINED_VMKCALL;
   }

   VMKAPI_MODULE_CALL(SCSI_GET_MODULE_ID(shost), fc_stat, ft->get_fc_host_stats, shost);
   if (!fc_stat) {
      return VMK_FAILURE;
   }

   portStats->secondsSinceLastReset = fc_stat->seconds_since_last_reset;
   portStats->txFrames = fc_stat->tx_frames;
   portStats->rxFrames = fc_stat->rx_frames;
   portStats->txWords = fc_stat->tx_words;
   portStats->rxWords = fc_stat->rx_words;
   portStats->lipCount = fc_stat->lip_count;
   portStats->nosCount = fc_stat->nos_count;
   portStats->errorFrames = fc_stat->error_frames;
   portStats->dumpedFrames = fc_stat->dumped_frames;
   portStats->linkFailureCount = fc_stat->link_failure_count;
   portStats->lossOfSyncCount = fc_stat->loss_of_sync_count;
   portStats->lossOfSignalCount = fc_stat->loss_of_signal_count;
   portStats->primitiveSeqProtocolErrCount = fc_stat->prim_seq_protocol_err_count;
   portStats->invalidTxWordCount = fc_stat->invalid_tx_word_count;
   portStats->invalidCrcCount = fc_stat->invalid_crc_count;

   /* fc4 statistics */
   portStats->inputRequests = fc_stat->fcp_input_requests;
   portStats->outputRequests = fc_stat->fcp_output_requests;
   portStats->controlRequests = fc_stat->fcp_control_requests;
   portStats->inputMegabytes = fc_stat->fcp_input_megabytes;
   portStats->outputMegabytes = fc_stat->fcp_output_megabytes;

   return VMK_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * FcLinuxPortReset
 *
 *     Reset A Given FC HBA Port
 *
 * Result:
 *     None
 *----------------------------------------------------------------------
 */
static VMK_ReturnStatus
FcLinuxPortReset(void *clientData, vmk_uint32 portId)
{
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter = 
		(struct vmklnx_ScsiAdapter *) clientData; 
   vmk_ScsiAdapter    *vmkAdapter;
   struct Scsi_Host *shost;
   struct fc_internal *i;
   struct fc_function_template *ft;

   VMK_ASSERT(vmklnx26ScsiAdapter);
   vmkAdapter = vmklnx26ScsiAdapter->vmkAdapter;
   VMK_ASSERT(vmkAdapter);

   shost = vmklnx26ScsiAdapter->shost;
   VMK_ASSERT(shost);

   i = to_fc_internal(shost->transportt);
   VMK_ASSERT(i);

   ft = i->f;
   if (ft == NULL) {
      VMKLNX_WARN("FcLinuxPortReset, %s, no ft found", vmk_NameToString(&vmkAdapter->name));
      return VMK_NOT_FOUND;
   }

   if (ft->issue_fc_host_lip == NULL) {
      VMKLNX_WARN("FcLinuxPortReset, %s, no ft->issue_fc_host_lip found", vmk_NameToString(&vmkAdapter->name));
      return VMK_NOT_SUPPORTED;
   }

   VMKAPI_MODULE_CALL_VOID(SCSI_GET_MODULE_ID(shost), ft->issue_fc_host_lip, shost);

   return VMK_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * FcoeLinuxPortStatistics
 *
 *     Get FCoE IO Statistics
 *
 * Result:
 *     Return FCoE IO statistics information for the specified HBA
 *----------------------------------------------------------------------
 */
static VMK_ReturnStatus
FcoeLinuxPortStatistics(void *clientData, vmk_FcoePortStatistics *portStats)
{
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter = clientData;
   struct Scsi_Host *shost;
   struct fc_lport *lport;
   struct timespec v0, v1;
   unsigned int cpu;

   VMK_ASSERT(portStats);
   VMK_ASSERT(vmklnx26ScsiAdapter);

   shost = vmklnx26ScsiAdapter->shost;
   VMK_ASSERT(shost);

   lport = shost_priv(shost);

   memset(portStats, 0, sizeof(vmk_FcoePortStatistics));

   jiffies_to_timespec(jiffies, &v0);
   jiffies_to_timespec(lport->boot_time, &v1);
   portStats->secondsSinceLastReset = (v0.tv_sec - v1.tv_sec);

   for_each_possible_cpu(cpu) {
      struct fcoe_dev_stats *stats;

      stats = FCOE_PER_CPU_PTR(lport->dev_stats, cpu, sizeof(struct fcoe_dev_stats));

      portStats->txFrames += stats->TxFrames;
      portStats->txWords += stats->TxWords;
      portStats->rxFrames += stats->RxFrames;
      portStats->rxWords += stats->RxWords;
      portStats->errorFrames += stats->ErrorFrames;
      portStats->dumpedFrames = stats->DumpedFrames;
      portStats->linkFailureCount += stats->LinkFailureCount;
      portStats->lossOfSignalCount += stats->LossOfSignalCount;
      portStats->invalidTxWordCount += stats->InvalidTxWordCount;
      portStats->invalidCrcCount += stats->InvalidCRCCount;
      portStats->inputRequests += stats->InputRequests;
      portStats->outputRequests += stats->OutputRequests;
      portStats->controlRequests += stats->ControlRequests;
      portStats->inputMegabytes += stats->InputMegabytes;
      portStats->outputMegabytes += stats->OutputMegabytes;
      portStats->vlinkFailureCount += stats->VLinkFailureCount;
      portStats->missDiscAdvCount += stats->MissDiscAdvCount;
   }

   return VMK_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * FcoeLinuxPortReset
 *
 *     Reset A Given FCoE HBA Port
 *
 * Result:
 *     None
 *----------------------------------------------------------------------
 */
static VMK_ReturnStatus
FcoeLinuxPortReset(void *clientData, vmk_uint32 portId)
{
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter = clientData;
   struct Scsi_Host *shost;
   struct fc_internal *i;
   struct fc_function_template *ft;

   VMK_ASSERT(vmklnx26ScsiAdapter);

   shost = vmklnx26ScsiAdapter->shost;
   VMK_ASSERT(shost);

   i = to_fc_internal(shost->transportt);
   VMK_ASSERT(i);

   ft = i->f;
   if (ft == NULL) {
      return VMK_NOT_FOUND;
   }

   if (ft->issue_fc_host_lip == NULL) {
      return VMK_NOT_SUPPORTED;
   }

   VMKAPI_MODULE_CALL_VOID(SCSI_GET_MODULE_ID(shost), ft->issue_fc_host_lip, shost);

   return VMK_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxAdapterStatus
 *
 *     Get adapter status
 *
 * Result:
 *
 *     VMK_ADAPTER_STATUS_UNKNOWN - Unknown adapter Status
 *     VMK_ADAPTER_STATUS_ONLINE - Adapter is Online and working
 *     VMK_ADAPTER_STATUS_OFFLINE - Adapter is not capable of accepting new 
 *				    commands
 *----------------------------------------------------------------------
 */
static vmk_AdapterStatus
SCSILinuxAdapterStatus(void *vmkAdapter)
{
   return VMK_ADAPTER_STATUS_ONLINE;
   /* 
    * For now return as it is. We will have to add this entry point new 
    */
}


/*
 *----------------------------------------------------------------------
 *
 * SCSILinuxRescanLink
 *
 *     Rescan Link
 *
 * Result:
 *
 *     VMK_RESCAN_LINK_UNSUPPORTED - Rescanning the link not supported
 *     VMK_RESCAN_LINK_SUCCEEDED - Rescan succeeded
 *     VMK_RESCAN_LINK_FAILED - Rescan failed
 *----------------------------------------------------------------------
 */
static vmk_RescanLinkStatus
SCSILinuxRescanLink(void *clientData)
{
   return VMK_RESCAN_LINK_SUCCEEDED;
   /* 
    * For now return as it is. Newer kernels > 2.6.18 do support
    * link scan. If so, we can call them later
    */
}

/*
 *----------------------------------------------------------------------
 *
 * FcLinuxAttachMgmtAdapter
 *
 *     Attach FC mgmt Adapter structure
 *
 * Result:
 *     Attach to FC transport. Returns  0 on SUCCESS 
 *----------------------------------------------------------------------
 */
int
FcLinuxAttachMgmtAdapter(struct Scsi_Host *shost)
{
   vmk_FcAdapter *pFcAdapter;
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter = 
		(struct vmklnx_ScsiAdapter *) shost->adapter; 
   vmk_ScsiAdapter *vmkAdapter;

   VMK_ASSERT(shost);
   VMK_ASSERT(vmklnx26ScsiAdapter);
   
   vmkAdapter = vmklnx26ScsiAdapter->vmkAdapter;
   VMK_ASSERT(vmkAdapter);

   pFcAdapter = 
	(vmk_FcAdapter *)kzalloc(sizeof(vmk_FcAdapter), GFP_KERNEL);
   if (!pFcAdapter) {
      VMKLNX_WARN("No memory for FC adapter struct");
      return -ENOMEM;
   }

   pFcAdapter->getFcAdapterStatus = SCSILinuxAdapterStatus;
   pFcAdapter->rescanFcLink = SCSILinuxRescanLink;
   pFcAdapter->getFcNodeName = FcLinuxAdapterNodeName;
   pFcAdapter->getFcPortName = FcLinuxAdapterPortName;
   pFcAdapter->getFcPortId = FcLinuxAdapterPortId;
   pFcAdapter->getFcLinkSpeed = FcLinuxLinkSpeed;
   pFcAdapter->getFcPortType = FcLinuxAdapterPortType; 
   pFcAdapter->getFcPortState = FcLinuxAdapterPortState; 
   pFcAdapter->getFcTargetAttributes = FcLinuxTargetAttributes;
   pFcAdapter->getFcAdapterAttributes = FcLinuxAdapterAttributes;
   pFcAdapter->getFcPortAttributes = FcLinuxPortAttributes;
   pFcAdapter->getFcPortStatistics = FcLinuxPortStatistics;
   pFcAdapter->issueFcPortReset = FcLinuxPortReset;

   vmkAdapter->mgmtAdapter.t.fc = pFcAdapter;

   vmkAdapter->mgmtAdapter.transport = VMK_STORAGE_ADAPTER_FC;

   return 0; 
}

/*
 *----------------------------------------------------------------------
 *
 * FcLinuxReleaseMgmtAdapter
 *
 *     Free up the FC mgmt Adapter structure
 *
 * Result:
 *     Unregister the FC transport
 *----------------------------------------------------------------------
 */
void
FcLinuxReleaseMgmtAdapter(struct Scsi_Host *shost)
{
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter;
   vmk_ScsiAdapter *vmkAdapter;

   VMK_ASSERT(shost);

   vmklnx26ScsiAdapter = (struct vmklnx_ScsiAdapter *) shost->adapter; 
   VMK_ASSERT(vmklnx26ScsiAdapter);
   
   vmkAdapter = vmklnx26ScsiAdapter->vmkAdapter;
   VMK_ASSERT(vmkAdapter);

   if ( vmkAdapter->mgmtAdapter.transport == VMK_STORAGE_ADAPTER_FC ) {
      /*
       * Make sure we mark this template as useless from now on
       */
      vmkAdapter->mgmtAdapter.transport = 
		VMK_STORAGE_ADAPTER_TRANSPORT_UNKNOWN;
      kfree(vmkAdapter->mgmtAdapter.t.fc);
   }

   return;
}

/**
 *
 *  \globalfn find_initiator_sas_address - get the lowest initiator sas address 
 *
 *  \param dev a pointer to a struct device 
 *  \param data a pointer to a u64 sas address
 *  \return 0  
 *  \par Include:
 *  scsi/scsi_transport_sas.h
 *  \par ESX Deviation Notes:
 *  None.
 *  \sa None.
 *  \comments -
 *
 */
static int
find_initiator_sas_address(struct device *dev, void *data)
{
   if (dev &&  data && (SAS_PHY_DEVICE_TYPE == dev->dev_type)) {
      u64 * pSasAddr = (u64 *) data;
      struct sas_phy *phy = dev_to_phy(dev);
      if (phy && (phy->identify.sas_address < (*pSasAddr))) {
         *pSasAddr = phy->identify.sas_address;
      }
   }
   return 0;
}

/*
 *-------------------------------------------------------------------
 * find_initiator_sas_phy
 *     Helper function to find SAS_PHY from scsi_host.
 * Result:
 *     Return SAS PHY information for the specified HBA.
 *--------------------------------------------------------------------
*/
static int
find_initiator_sas_phy(struct device *dev, void *data)
{
   if (dev &&  data && (SAS_PHY_DEVICE_TYPE == dev->dev_type)) {
      struct sas_phy *phy = dev_to_phy(dev);
      struct sas_phy **phyPtr = (struct sas_phy **)data;
      *phyPtr = phy;
   }
   return 0;
}

/**
 *
 *  \globalfn SasLinuxInitiatorAddress -- return inititator sas address
 *
 *  \param clientData a pointer to a struct vmklnx_ScsiAdapter 
 *  \return lowest initiator sas address or 0 if none 
 *  \par Include:
 *  scsi/scsi_transport_sas.h
 *  \par ESX Deviation Notes:
 *  None.
 *  \sa None.
 *  \comments -
 *
 */
vmk_uint64
SasLinuxInitiatorAddress(void *clientData)
{
   struct Scsi_Host   *shost;
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter;
   vmk_uint64 initiatorAddress = 0;

   vmklnx26ScsiAdapter = (struct vmklnx_ScsiAdapter *)clientData;
   if (NULL == vmklnx26ScsiAdapter) {
      return(initiatorAddress);
   }
   shost = vmklnx26ScsiAdapter->shost;
   if (NULL == shost) {
      return(initiatorAddress);
   }
   initiatorAddress = ~0; 
   device_for_each_child(&shost->shost_gendev,
                        (void *)&initiatorAddress, find_initiator_sas_address); 
   if((~0) == initiatorAddress) {
      /* In case not found, set initiator sas address to 0 */
      int ret = FAILED;
      struct sas_internal *i = to_sas_internal(shost->transportt);
      VMK_ASSERT(i);
      initiatorAddress = 0; 
      if (i->f && i->f->get_initiator_sas_identifier) {
         /*
          * This provides a workaround for hpsa SAS driver and
          * it should be removed when HP driver has real SAS transport support
          */
         VMKAPI_MODULE_CALL(SCSI_GET_MODULE_ID(shost), ret,
                i->f->get_initiator_sas_identifier, shost, (u64 *)&initiatorAddress);
      }
   }
   return(initiatorAddress);
}

/**
 *
 *  \globalfn SasLinuxTargetAttributes -- retrieve target attributes 
 *
 *  \param clientData a pointer to a struct vmklnx_ScsiAdapter
 *  \param sasAttrib a pointer to vmk_SasTargetAttrs 
 *  \param channelNo  a channel number 
 *  \param targetNo  a target number 
 *  \return VMK_ReturnStatus 
 *  \par Include:
 *  scsi/scsi_transport_sas.h
 *  \par ESX Deviation Notes:
 *  None.
 *  \sa None.
 *  \comments -
 *
 */
VMK_ReturnStatus
SasLinuxTargetAttributes(void *clientData, vmk_SasTargetAttrs *sasAttrib,
                         vmk_uint32 channelNo, vmk_uint32 targetNo)
{
   unsigned long      flags;
   struct Scsi_Host   *shost;
   struct sas_internal *i;
   struct scsi_target *starget;
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter;
   int ret = FAILED;

   if (NULL == sasAttrib) {
      return(VMK_FAILURE);
   }
   memset(sasAttrib, 0, sizeof(vmk_SasTargetAttrs));
   vmklnx26ScsiAdapter = (struct vmklnx_ScsiAdapter *)clientData;
   if ((NULL == vmklnx26ScsiAdapter) || (NULL == vmklnx26ScsiAdapter->shost)) {
      return(VMK_FAILURE);
   }
   shost = vmklnx26ScsiAdapter->shost;
   spin_lock_irqsave(shost->host_lock, flags);   
   starget = vmklnx_scsi_find_target(shost, channelNo, targetNo);
   spin_unlock_irqrestore(shost->host_lock, flags);
   if (NULL == starget) {
      return(VMK_FAILURE);
   }
   i = to_sas_internal(shost->transportt);
   VMK_ASSERT(i);

   /* Let each individual driver to provide SAS target information */
   if (i->f && i->f->get_target_sas_identifier) {
      VMKAPI_MODULE_CALL(SCSI_GET_MODULE_ID(shost), ret,
                i->f->get_target_sas_identifier, starget, (u64 *)&sasAttrib->sasAddress);
   }
   if (SUCCESS != ret) {
      VMKLNX_WARN("ret=0x%x - Cannot get SAS target identifier for "
                  "Channel=%d Target=%d. Please make sure driver can provide "
                  "SAS target information", ret, channelNo, targetNo);
      return(VMK_FAILURE);
   } else {
      return(VMK_OK);
   }   
}

/*
 *----------------------------------------------------------------------
 *
 * SasLinuxAdapterAttributes
 *
 *     Get SAS Adapter Attributes
 *
 * Result:
 *     Return SAS adapter information for the specified HBA
 *----------------------------------------------------------------------
 */
VMK_ReturnStatus
SasLinuxAdapterAttributes(void *clientData,
                          vmk_SasAdapterAttributes *adapterAttrib)
{
   /* TODO */
   return VMK_NOT_IMPLEMENTED;
}

/*
 *----------------------------------------------------------------------
 *
 * SasLinuxPortAttributes
 *
 *     Get SAS Port Attributes
 *
 * Result:
 *     Return SAS port information for the specified SAS port
 *----------------------------------------------------------------------
 */
VMK_ReturnStatus
SasLinuxPortAttributes(void *clientData,
                       vmk_uint32 portId,
                       vmk_SasPortAttributes *portAttrib)
{
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter;
   struct Scsi_Host *shost;
   struct sas_phy *phy = NULL;

   VMK_ASSERT(portAttrib);

   vmklnx26ScsiAdapter = (struct vmklnx_ScsiAdapter *) clientData;
   VMK_ASSERT(vmklnx26ScsiAdapter);

   shost = vmklnx26ScsiAdapter->shost;
   VMK_ASSERT(shost);

   memset(portAttrib, 0, sizeof(vmk_SasPortAttributes));
   device_for_each_child(&shost->shost_gendev, (void *)&phy,
                          find_initiator_sas_phy);

   if (NULL == phy) {
       VMKLNX_DEBUG(0, "Can not get sas_phy from scsi_host");
       return VMK_NOT_FOUND;
   }

   portAttrib->attachedSasAddress = phy->identify.sas_address;
   portAttrib->phyIdentifier = phy->identify.phy_identifier;
   portAttrib->negotiatedLinkrate = phy->negotiated_linkrate;
   portAttrib->minimumLinkrate = phy->minimum_linkrate;
   portAttrib->maximumLinkrate = phy->maximum_linkrate;

   return VMK_OK;
}

/*
 *-----------------------------------------------------------------------------
 * collect_sas_phy_stats
 *      Helper function to go through all sas_phys on a scsi_host, and collects
 *      all the error stats
 *-----------------------------------------------------------------------------
 */
static int
SasLinuxCollectPhyStats(struct device *dev, void *stats)
{
   vmk_SasPortStatistics *portStats = (vmk_SasPortStatistics *)stats;
   struct Scsi_Host *sh = dev_to_shost(dev);
   struct sas_function_template *ft;
   struct sas_internal *i = to_sas_internal(sh->transportt);
   int error = 0;

   VMK_ASSERT(i);
   ft = i->f;
   if (ft == NULL) {
      VMKLNX_DEBUG(0, "SAS transport function not registered");
      return (-ENOSYS);
   }
   if (portStats && (SAS_PHY_DEVICE_TYPE == dev->dev_type)) {
      struct sas_phy *phy = dev_to_phy(dev);

      if (ft->get_linkerrors) {
         VMKAPI_MODULE_CALL(SCSI_GET_MODULE_ID(sh), error, ft->get_linkerrors, phy);
         if (error)
            return error;
      }
      VMKLNX_DEBUG(1, "Phy #: %d, SAS address: 0x%llX PHY ID: %d "
                   "linkrate: %d min %d max %d",
                   phy->number,
                   phy->identify.sas_address,
                   phy->identify.phy_identifier,
                   phy->negotiated_linkrate,
                   phy->minimum_linkrate,
                   phy->maximum_linkrate);
      portStats->invalidDwordCount += phy->invalid_dword_count;
      portStats->runningDisparityErrorCount += phy->running_disparity_error_count;
      portStats->lossOfDwordSyncCount += phy->loss_of_dword_sync_count;
      portStats->phyResetProblemCount += phy->phy_reset_problem_count;
   }
   return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * SasLinuxPortStatistics
 *
 *     Get SAS IO Statistics
 *
 * Result:
 *     Return SAS IO statistics information for the specified HBA
 *----------------------------------------------------------------------
 */
VMK_ReturnStatus
SasLinuxPortStatistics(void *clientData,
                       vmk_SasPortStatistics *portStats)
{
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter;
   struct Scsi_Host *shost;
   int ret = 0;

   VMK_ASSERT(portStats);

   vmklnx26ScsiAdapter = (struct vmklnx_ScsiAdapter *) clientData;
   VMK_ASSERT(vmklnx26ScsiAdapter);

   shost = vmklnx26ScsiAdapter->shost;
   VMK_ASSERT(shost);

   memset(portStats, 0, sizeof(vmk_SasPortStatistics));

   /* sas_phy_alloc set up sas_phy parent dev as shost_gendev */
   ret = device_for_each_child(&shost->shost_gendev, (void *)portStats,
                               SasLinuxCollectPhyStats);

   if (ret)
      return VMK_EXEC_FAILURE;

   return VMK_OK;
}

/*
 *-----------------------------------------------------------------------------
 *
 * SasLinuxPhyReset
 *      Helper function to reset the link of sys_phy
 *-----------------------------------------------------------------------------
 */
static int
SasLinuxPhyReset(struct device *dev, void *data)
{
   struct Scsi_Host *sh = dev_to_shost(dev);
   struct sas_function_template *ft;
   struct sas_internal *i = to_sas_internal(sh->transportt);
   int error = 0;

   VMK_ASSERT(i);
   ft = i->f;
   if (ft == NULL) {
      VMKLNX_DEBUG(0, "SAS transport function not registered");
      return (-ENOSYS);
   }
   if (dev &&  data && (SAS_PHY_DEVICE_TYPE == dev->dev_type)) {
      struct sas_phy *phy = dev_to_phy(dev);

      if (ft->phy_reset) {
         VMKAPI_MODULE_CALL(SCSI_GET_MODULE_ID(sh), error, ft->phy_reset, phy, 0);
         if (error)
            return error;
      }
   }
  return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * SasLinuxPortReset
 *
 *     Reset A Given SAS HBA Port
 *
 * Result:
 *     None
 *----------------------------------------------------------------------
 */
static VMK_ReturnStatus
SasLinuxPortReset(void *clientData, vmk_uint32 portId)
{
   vmk_ScsiAdapter *vmkAdapter;
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter;
   struct Scsi_Host *shost;
   int ret = 0;


   vmklnx26ScsiAdapter = (struct vmklnx_ScsiAdapter *) clientData;
   VMK_ASSERT(vmklnx26ScsiAdapter);

   vmkAdapter = vmklnx26ScsiAdapter->vmkAdapter;
   VMK_ASSERT(vmkAdapter);
   shost = vmklnx26ScsiAdapter->shost;
   VMK_ASSERT(shost);

   ret = device_for_each_child(&shost->shost_gendev, NULL, SasLinuxPhyReset);
   if (ret) {
      VMKLNX_DEBUG(0, "SAS PHY reset failed on %s", vmk_NameToString(&vmkAdapter->name));
      return VMK_EXEC_FAILURE;
   } else {
      VMKLNX_DEBUG(0, "SAS PHY reset succeeded on %s", vmk_NameToString(&vmkAdapter->name));
      return VMK_OK;
   }
}

/*
 *----------------------------------------------------------------------
 *
 * SasLinuxAttachMgmtAdapter
 *
 *     Attach SAS mgmt Adapter structure
 *
 * Result:
 *     Attach to SAS transport. Returns  0 on SUCCESS 
 *----------------------------------------------------------------------
 */
int
SasLinuxAttachMgmtAdapter(struct Scsi_Host *shost)
{
   vmk_SasAdapter *pSasAdapter;
   vmk_ScsiAdapter *vmkAdapter;
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter;

   VMK_ASSERT(shost);
   vmklnx26ScsiAdapter = (struct vmklnx_ScsiAdapter *) shost->adapter; 
   VMK_ASSERT(vmklnx26ScsiAdapter);
   vmkAdapter = vmklnx26ScsiAdapter->vmkAdapter;
   VMK_ASSERT(vmkAdapter);
   pSasAdapter = (vmk_SasAdapter *)kzalloc(sizeof(vmk_SasAdapter), GFP_KERNEL);
   if (NULL == pSasAdapter) {
      VMKLNX_WARN("No memory for SAS management adapter struct allocation");
      return -ENOMEM;
   }

   pSasAdapter->getInitiatorSasAddress = SasLinuxInitiatorAddress;
   pSasAdapter->getSasTargetAttributes = SasLinuxTargetAttributes;
   pSasAdapter->getSasAdapterAttributes = SasLinuxAdapterAttributes;
   pSasAdapter->getSasPortAttributes = SasLinuxPortAttributes;
   pSasAdapter->getSasPortStatistics = SasLinuxPortStatistics;
   pSasAdapter->issueSasPortReset = SasLinuxPortReset;
   
   vmkAdapter->mgmtAdapter.t.sas = pSasAdapter;
   vmkAdapter->mgmtAdapter.transport = VMK_STORAGE_ADAPTER_SAS;
   return 0; 
}

/*
 *----------------------------------------------------------------------
 *
 * SasLinuxReleaseMgmtAdapter
 *
 *     Free up the SAS mgmt Adapter structure
 *
 * Result:
 *     Unregister the SAS transport
 *----------------------------------------------------------------------
 */
void
SasLinuxReleaseMgmtAdapter(struct Scsi_Host *shost)
{
   vmk_ScsiAdapter *vmkAdapter;
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter;

   VMK_ASSERT(shost);
   vmklnx26ScsiAdapter = (struct vmklnx_ScsiAdapter *) shost->adapter; 
   VMK_ASSERT(vmklnx26ScsiAdapter);
   vmkAdapter = vmklnx26ScsiAdapter->vmkAdapter;
   VMK_ASSERT(vmkAdapter);

   if (VMK_STORAGE_ADAPTER_SAS == vmkAdapter->mgmtAdapter.transport) {
      /*
       * Make sure we mark this template as useless from now on
       */
      vmkAdapter->mgmtAdapter.transport = VMK_STORAGE_ADAPTER_TRANSPORT_UNKNOWN;
      kfree(vmkAdapter->mgmtAdapter.t.sas);
   }
   return;
}

/*
 *----------------------------------------------------------------------
 *
 * XsanLinuxInitiatorID
 *
 *     Return generic SAN inititator ID
 *
 * Result:
 *     Return generic SAN transport initiator ID in the initiatorID
 *----------------------------------------------------------------------
 */
static VMK_ReturnStatus
XsanLinuxInitiatorID(void *clientData, vmk_XsanID *initiatorID)
{
   struct Scsi_Host   *shost;
   vmk_ScsiAdapter *vmkAdapter;
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter;
   struct xsan_internal *i;

   if (NULL == initiatorID) {
      return VMK_FAILURE;
   }
   vmklnx26ScsiAdapter = (struct vmklnx_ScsiAdapter *)clientData;
   if (NULL == vmklnx26ScsiAdapter) {
      return VMK_FAILURE;
   }
   vmkAdapter = vmklnx26ScsiAdapter->vmkAdapter;
   if (NULL == vmkAdapter) {
      return VMK_FAILURE;
   }

   shost = vmklnx26ScsiAdapter->shost;
   if (NULL == shost) {
      return VMK_FAILURE;
   }

   i = to_xsan_internal(shost->transportt);
   VMK_ASSERT(i);

   if (i->f && i->f->get_initiator_xsan_identifier) {
      int ret = FAILED;

      VMK_ASSERT_ON_COMPILE(sizeof(vmk_XsanID) == sizeof(struct xsan_id));
      VMK_ASSERT_ON_COMPILE(offsetof(vmk_XsanID, L) == offsetof(struct xsan_id, L));
      VMK_ASSERT_ON_COMPILE(offsetof(vmk_XsanID, H) == offsetof(struct xsan_id, H));

      VMKAPI_MODULE_CALL(SCSI_GET_MODULE_ID(shost), ret,
         i->f->get_initiator_xsan_identifier, shost, (struct xsan_id *)initiatorID);
      if (SUCCESS != ret) {
         VMKLNX_WARN(
            "ret=0x%x - Cannot get generic SAN initiator identifier for %s",
            ret, vmk_ScsiGetAdapterName(vmkAdapter));
         return VMK_FAILURE;
      }
   }
   return VMK_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * XsanLinuxTargetAttributes
 *
 *     Retrieve target attributes for generic SAN
 *
 * Result:
 *     Return generic SAN transport information for the specified target
 *     in the xsanAttrib.
 *----------------------------------------------------------------------
 */
static VMK_ReturnStatus
XsanLinuxTargetAttributes(void *clientData, vmk_XsanTargetAttrs *xsanAttrib,
                          vmk_uint32 channelNo, vmk_uint32 targetNo)
{
   unsigned long      flags;
   struct Scsi_Host   *shost;
   struct xsan_internal *i;
   struct scsi_target *starget;
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter;
   int ret = FAILED;

   if (NULL == xsanAttrib) {
      return  VMK_FAILURE;
   }
   memset(xsanAttrib, 0, sizeof(vmk_XsanTargetAttrs));
   vmklnx26ScsiAdapter = (struct vmklnx_ScsiAdapter *)clientData;
   if (NULL == vmklnx26ScsiAdapter || NULL == vmklnx26ScsiAdapter->shost) {
      return VMK_FAILURE;
   }
   shost = vmklnx26ScsiAdapter->shost;
   VMK_ASSERT(shost);
   spin_lock_irqsave(shost->host_lock, flags);   
   starget = vmklnx_scsi_find_target(shost, channelNo, targetNo);
   spin_unlock_irqrestore(shost->host_lock, flags);
   if (NULL == starget) {
      return VMK_FAILURE;
   }
   i = to_xsan_internal(shost->transportt);
   VMK_ASSERT(i);

   /* Let each individual driver to provide generic SAN target information */
   if (i->f && i->f->get_target_xsan_identifier) {
      VMKAPI_MODULE_CALL(SCSI_GET_MODULE_ID(shost), ret,
         i->f->get_target_xsan_identifier, starget, (struct xsan_id *)&xsanAttrib->id);
   }
   if (SUCCESS != ret) {
      VMKLNX_WARN("ret=0x%x - Cannot get generic SAN target identifier for "
                  "Channel=%d Target=%d. Please make sure driver can provide "
                  "generic SAN target information", ret, channelNo, targetNo);
      return VMK_FAILURE;
   }   
   return VMK_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * XsanLinuxAttachMgmtAdapter
 *
 *     Attach generic SAN mgmt Adapter structure
 *
 * Result:
 *     Attach to generic SAN transport. Returns  0 on SUCCESS 
 *----------------------------------------------------------------------
 */
int
XsanLinuxAttachMgmtAdapter(struct Scsi_Host *shost)
{
   vmk_XsanAdapter *pXsanAdapter;
   vmk_ScsiAdapter *vmkAdapter;
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter;

   VMK_ASSERT(shost);
   vmklnx26ScsiAdapter = (struct vmklnx_ScsiAdapter *) shost->adapter; 
   VMK_ASSERT(vmklnx26ScsiAdapter);
   vmkAdapter = vmklnx26ScsiAdapter->vmkAdapter;
   VMK_ASSERT(vmkAdapter);
   pXsanAdapter = (vmk_XsanAdapter *)kzalloc(sizeof(vmk_XsanAdapter), GFP_KERNEL);
   if (NULL == pXsanAdapter) {
      VMKLNX_WARN("No memory for generic SAN adapter struct allocation");
      return -ENOMEM;
   }

   pXsanAdapter->getXsanInitiatorID = XsanLinuxInitiatorID;
   pXsanAdapter->getXsanTargetAttributes = XsanLinuxTargetAttributes;
   vmkAdapter->mgmtAdapter.t.xsan = pXsanAdapter;
   vmkAdapter->mgmtAdapter.transport = VMK_STORAGE_ADAPTER_XSAN;
   return 0; 
}

/*
 *----------------------------------------------------------------------
 *
 * XsanLinuxReleaseMgmtAdapter
 *
 *     Free up the generic SAN mgmt Adapter structure
 *
 * Result:
 *     Unregister the generic SAN transport
 *----------------------------------------------------------------------
 */
void
XsanLinuxReleaseMgmtAdapter(struct Scsi_Host *shost)
{
   vmk_ScsiAdapter *vmkAdapter;
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter;

   VMK_ASSERT(shost);
   vmklnx26ScsiAdapter = (struct vmklnx_ScsiAdapter *) shost->adapter; 
   VMK_ASSERT(vmklnx26ScsiAdapter);
   vmkAdapter = vmklnx26ScsiAdapter->vmkAdapter;
   VMK_ASSERT(vmkAdapter);

   if (VMK_STORAGE_ADAPTER_XSAN == vmkAdapter->mgmtAdapter.transport) {
      /*
       * Make sure we mark this template as useless from now on
       */
      vmkAdapter->mgmtAdapter.transport = VMK_STORAGE_ADAPTER_TRANSPORT_UNKNOWN;
      kfree(vmkAdapter->mgmtAdapter.t.xsan);
   }
   return;
}


/*
 *----------------------------------------------------------------------
 *
 * FcoeLinuxAttachMgmtAdapter
 *
 *     Attach FCoE mgmt Adapter structure
 *
 * Result:
 *     Attach to FCoE transport. Returns  0 on SUCCESS 
 *----------------------------------------------------------------------
 */
int
FcoeLinuxAttachMgmtAdapter(struct Scsi_Host *shost)
{
   vmk_FcoeAdapter *pFcoeAdapter;
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter = 
		(struct vmklnx_ScsiAdapter *) shost->adapter; 
   vmk_ScsiAdapter *vmkAdapter;
   struct fc_host_attrs *fc_host;
   vmk_FcoeAdapterAttrs *fcoeAttrs;

   VMK_ASSERT(vmklnx26ScsiAdapter);

   /*
    * fc_host is allcoated during shost creation
    */
   fc_host = shost_to_fc_host(shost);
   VMK_ASSERT(fc_host);
   
   vmkAdapter = vmklnx26ScsiAdapter->vmkAdapter;
   VMK_ASSERT(vmkAdapter);

   pFcoeAdapter = 
	(vmk_FcoeAdapter *)kzalloc(sizeof(vmk_FcoeAdapter), GFP_KERNEL);
   if (!pFcoeAdapter) {
      vmk_WarningMessage("No memory for FCoE adapter struct");
      return -ENOMEM;
   }

   fcoeAttrs = 
	(vmk_FcoeAdapterAttrs *)kzalloc(sizeof(vmk_FcoeAdapterAttrs), GFP_KERNEL);
   if (!fcoeAttrs) {
      vmk_WarningMessage("No memory for FCoE adapter Attribute struct");
      kfree(pFcoeAdapter);
      return -ENOMEM;
   }

   fc_host->cna_ops = (void *) fcoeAttrs;

   pFcoeAdapter->getFCoEAdapterAttributes = FcoeLinuxAdapterAttributes;
   pFcoeAdapter->getFcoePortStatistics = FcoeLinuxPortStatistics;
   pFcoeAdapter->issueFcoePortReset = FcoeLinuxPortReset;
   FcLinuxPopulateCB(&pFcoeAdapter->fc);
   pFcoeAdapter->fc.linkTimeout = 15;

   vmkAdapter->mgmtAdapter.t.fcoe = pFcoeAdapter;

   vmkAdapter->mgmtAdapter.transport = VMK_STORAGE_ADAPTER_FCOE;

   return 0; 
}

/*
 *----------------------------------------------------------------------
 *
 * FcoeLinuxReleaseMgmtAdapter
 *
 *     Free up the FCoE mgmt Adapter structure
 *
 * Result:
 *     Unregister the FCoE transport
 *----------------------------------------------------------------------
 */
void
FcoeLinuxReleaseMgmtAdapter(struct Scsi_Host *shost)
{
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter;
   vmk_ScsiAdapter *vmkAdapter;
   struct fc_host_attrs *fc_host;

   VMK_ASSERT(shost);

   vmklnx26ScsiAdapter = (struct vmklnx_ScsiAdapter *) shost->adapter; 
   VMK_ASSERT(vmklnx26ScsiAdapter);
   
   vmkAdapter = vmklnx26ScsiAdapter->vmkAdapter;
   VMK_ASSERT(vmkAdapter);

   fc_host = shost_to_fc_host(shost);
   VMK_ASSERT(fc_host);
   
   if (vmkAdapter->mgmtAdapter.transport == VMK_STORAGE_ADAPTER_FCOE) {
      /*
       * Make sure we mark this template as useless from now on
       */
      vmkAdapter->mgmtAdapter.transport = 
		VMK_STORAGE_ADAPTER_TRANSPORT_UNKNOWN;
      kfree(vmkAdapter->mgmtAdapter.t.fcoe);
      kfree(fc_host->cna_ops);
   }

   return;
}

/*
 *----------------------------------------------------------------------
 *
 * FcoeLinuxAdapterAttributes
 *
 *     Copy FCoE attributes to the management stack
 *
 * Result:
 *     VMK_OK on success, failure otherwise
 *----------------------------------------------------------------------
 */
VMK_ReturnStatus
FcoeLinuxAdapterAttributes(void *clientData, vmk_FcoeAdapterAttrs *fcoeAttrs) 
{
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter = 
		(struct vmklnx_ScsiAdapter *) clientData; 
   struct fc_host_attrs *fc_host;
   struct Scsi_Host    *shost;

   VMK_ASSERT(vmklnx26ScsiAdapter);

   shost = vmklnx26ScsiAdapter->shost;
   VMK_ASSERT(shost);

   fc_host = shost_to_fc_host(shost);
   VMK_ASSERT(fc_host);

   /*
    * Fill in the data structures
    */
   memcpy(fcoeAttrs, fc_host->cna_ops, sizeof(*fcoeAttrs));

   return VMK_OK;

}

/*
 *----------------------------------------------------------------------
 *
 * FcLinuxPopulateCB -
 *
 *     Populate Call back function for FC Mgmt
 *
 * Result:
 *     None
 *----------------------------------------------------------------------
 */
static void   
FcLinuxPopulateCB(vmk_FcAdapter *pFcAdapter)
{
   pFcAdapter->getFcAdapterStatus = SCSILinuxAdapterStatus;
   pFcAdapter->rescanFcLink = SCSILinuxRescanLink;
   pFcAdapter->getFcNodeName = FcLinuxAdapterNodeName;
   pFcAdapter->getFcPortName = FcLinuxAdapterPortName;
   pFcAdapter->getFcPortId = FcLinuxAdapterPortId;
   pFcAdapter->getFcLinkSpeed = FcLinuxLinkSpeed;
   pFcAdapter->getFcPortType = FcLinuxAdapterPortType; 
   pFcAdapter->getFcPortState = FcLinuxAdapterPortState; 
   pFcAdapter->getFcTargetAttributes = FcLinuxTargetAttributes;
   pFcAdapter->getFcAdapterAttributes = FcLinuxAdapterAttributes;
   pFcAdapter->getFcPortAttributes = FcLinuxPortAttributes;
   pFcAdapter->getFcPortStatistics = FcLinuxPortStatistics;

  return;
}

/*
 *----------------------------------------------------------------------
 *
 * SCSILinux_InitVmkIf
 *
 *    Entry point for SCSI VMkernel interface-specific initialization.
 *    Called as part of SCSILinux_Init in linux_scsi.c.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Initializes SCSI VMkernel interface log.
 *
 *----------------------------------------------------------------------
 */
void
SCSILinux_InitVmkIf(void)
{
   VMKLNX_CREATE_LOG();
}

/*
 *----------------------------------------------------------------------
 *
 * SCSILinux_CleanupVmkIf
 *
 *    Entry point for SCSI VMkernel interface-specific teardown.
 *    Called as part of SCSILinux_Cleanup in linux_scsi.c.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Cleans up SCSI VMkernel interface log.
 *
 *----------------------------------------------------------------------
 */
void
SCSILinux_CleanupVmkIf(void)
{
   VMKLNX_DESTROY_LOG();
}
