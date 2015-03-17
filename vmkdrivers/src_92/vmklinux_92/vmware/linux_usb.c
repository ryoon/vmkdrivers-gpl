/*
 * ****************************************************************
 * Portions Copyright 2008, 2010-2011 VMware, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * ****************************************************************/


/*
 * linux_usb.c --
 *
 *      vmklinux USB utility functions
 */


#include <linux/usb.h>
#include <linux/pci.h>
#include <vmklinux_92/vmklinux_log.h>

#include "vmkapi.h"
#include "linux_stubs.h" 
#include "linux_usb.h"
#include "linux_stress.h"
#include "linux_pci.h"
/*
 *  Static Local Functions
 ********************************************************************
 */

/*
 * Globals
 ********************************************************************
 */


/* Stress option handles */
vmk_StressOptionHandle stressUSBBulkDelayProcessURB;
EXPORT_SYMBOL(stressUSBBulkDelayProcessURB);

vmk_StressOptionHandle stressUSBBulkURBFakeTransientError;
EXPORT_SYMBOL(stressUSBBulkURBFakeTransientError);

vmk_StressOptionHandle stressUSBStorageDelaySCSIDataPhase;
EXPORT_SYMBOL(stressUSBStorageDelaySCSIDataPhase);

vmk_StressOptionHandle stressUSBStorageDelaySCSITransfer;
EXPORT_SYMBOL(stressUSBStorageDelaySCSITransfer);

vmk_StressOptionHandle stressUSBDelayProcessTD;
vmk_StressOptionHandle stressUSBFailGPHeapAlloc;

/*
 * Externs
 ********************************************************************
 */

/*
 * Implementation
 ********************************************************************
 */

/*
 *----------------------------------------------------------------------
 *
 * LinuxUSB_Init
 *
 *      This is the init entry point for USB. Called from vmklinux 
 *      init from linux_stubs.c
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
LinuxUSB_Init(void)
{
   VMK_ReturnStatus status;

   status = vmk_StressOptionOpen(VMK_STRESS_OPT_USB_BULK_DELAY_PROCESS_URB,
                                 &stressUSBBulkDelayProcessURB);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_StressOptionOpen(VMK_STRESS_OPT_USB_BULK_URB_FAKE_TRANSIENT_ERROR,
                                 &stressUSBBulkURBFakeTransientError);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_StressOptionOpen(VMK_STRESS_OPT_USB_DELAY_PROCESS_TD,
                                 &stressUSBDelayProcessTD);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_StressOptionOpen(VMK_STRESS_OPT_USB_FAIL_GP_HEAP_ALLOC,
                                 &stressUSBFailGPHeapAlloc);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_StressOptionOpen(VMK_STRESS_OPT_USB_STORAGE_DELAY_SCSI_DATA_PHASE,
                                 &stressUSBStorageDelaySCSIDataPhase);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_StressOptionOpen(VMK_STRESS_OPT_USB_STORAGE_DELAY_SCSI_TRANSFER,
                                 &stressUSBStorageDelaySCSITransfer);
   VMK_ASSERT(status == VMK_OK);

   if (status != VMK_OK) {
      vmk_AlertMessage("%s - Failed to initialize USB common layer",
	 __FUNCTION__);
      VMK_ASSERT(status == VMK_OK);
   }

   return;
}

/*
 *----------------------------------------------------------------------
 *
 * LinuxUSB_Cleanup
 *
 *      This is the cleanup entry point for USB. Called during vmklinux 
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
LinuxUSB_Cleanup(void)
{
   VMK_ReturnStatus status;

   status = vmk_StressOptionClose(stressUSBBulkDelayProcessURB);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_StressOptionClose(stressUSBBulkURBFakeTransientError);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_StressOptionClose(stressUSBDelayProcessTD);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_StressOptionClose(stressUSBFailGPHeapAlloc);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_StressOptionClose(stressUSBStorageDelaySCSIDataPhase);
   VMK_ASSERT(status == VMK_OK);
   status = vmk_StressOptionClose(stressUSBStorageDelaySCSITransfer);
   VMK_ASSERT(status == VMK_OK);
}
