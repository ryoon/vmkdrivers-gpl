/* ****************************************************************
 * Portions Copyright 2011-2012 VMware, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * ****************************************************************/

#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_host.h>
#include <vmklinux_9/vmklinux_iodm.h>

#include "vmkapi.h"
#include "linux_scsi.h"
#include "vmklinux_log.h"

/*-----------------------------------------------------------------------------
 *  vmklnx_iodm_event - add an iodm event to event pool
 *  @sh: a pointer to scsi_host struct
 *  @id: an envent id
 *  @addr: a pointer related to this event
 *  @data: data that relates to this event
 *
 *  RETURN VALUE
 *  None
 *-----------------------------------------------------------------------------
 */
void
vmklnx_iodm_event(struct Scsi_Host *sh, unsigned int id, void *addr,
                  unsigned long data)
{
   vmk_ScsiAdapter *vmkAdapter;
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter;
   vmk_AddrCookie parameter;
   VMK_ReturnStatus status;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);

   if (sh == NULL) {
      VMKLNX_WARN("sh cannot be NULL when calling vmklnx_iodm_event!");
      return;
   }

   vmklnx26ScsiAdapter = (struct vmklnx_ScsiAdapter *) sh->adapter;

   if (vmklnx26ScsiAdapter == NULL) {
      VMKLNX_DEBUG(0, "Dropping event(%u), as scsi_add_host is not yet called",
                   id);
      return;
   }

   vmkAdapter = vmklnx26ScsiAdapter->vmkAdapter;
   parameter.ptr = addr;

   switch (id) {
      case IODM_RSCN:
      case IODM_LINKUP:
      case IODM_LINKDOWN:
      case IODM_FCOE_CVL:
      case IODM_LUNRESET:
         break;
      case IODM_IOERROR:
      case IODM_FRAMEDROP: {
         struct scsi_cmnd *scmd;

         VMK_ASSERT(addr);

         if (addr == NULL) {
            VMKLNX_WARN("'addr' cannot be NULL for %u event : %s",
                        id, vmklnx_get_vmhba_name(sh));
            return;
         }

         scmd = (struct scsi_cmnd *)addr;
         /* vmk_IodmEvent expects pointer to vmk_ScsiCommand */
         parameter.ptr = scmd->vmkCmdPtr;
         break;
      }
      default:
         break;
   }

   status = vmk_IodmEvent(vmkAdapter, id, parameter, data);

   if (status != VMK_OK) {
      VMKLNX_DEBUG(2, "vmk_IodmEvent returned '%s' for event %u on '%s'",
                   vmk_StatusToString(status), id, vmklnx_get_vmhba_name(sh));
   }

   return;
}
EXPORT_SYMBOL(vmklnx_iodm_event);

/*-----------------------------------------------------------------------------
 *  vmklnx_iodm_enable_events - Enable iodm scsi events
 *  @sh: a pointer to scsi_host struct of the calling HBA
 *
 *  Enable iodm scsi events tracking analysis for the calling HBA
 *
 *  RETURN VALUE
 *  None
 *-----------------------------------------------------------------------------
 */
void
vmklnx_iodm_enable_events(struct Scsi_Host *sh)
{
   vmk_ScsiAdapter *vmkAdapter;
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter;
   VMK_ReturnStatus status;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(sh);

   if (sh == NULL || sh->adapter == NULL) {
      VMKLNX_WARN("vmklnx_iodm_enable_events should be "
                  "called after scsi_add_host!");
      return;
   }

   vmklnx26ScsiAdapter = (struct vmklnx_ScsiAdapter *) sh->adapter;
   vmkAdapter = vmklnx26ScsiAdapter->vmkAdapter;
   status = vmk_IodmEnableEvents(vmkAdapter);

   if (status != VMK_OK) {
      VMKLNX_DEBUG(0, "Failed to enable IODM event collection for '%s' : %s",
                  vmklnx_get_vmhba_name(sh), vmk_StatusToString(status));
   } else {
      VMKLNX_DEBUG(2, "Enabled IODM event collection for '%s'",
                   vmklnx_get_vmhba_name(sh));
   }

   return;
}
EXPORT_SYMBOL(vmklnx_iodm_enable_events);

/*-----------------------------------------------------------------------------
 *  vmklnx_iodm_disable_events - Disable iodm scsi events
 *  @sh: a pointer to scsi_host struct of the calling HBA
 *
 *  Disable iodm scsi events for the calling HBA and dealloc event buffer
 *
 *  RETURN VALUE
 *  None
 *-----------------------------------------------------------------------------
 */
void
vmklnx_iodm_disable_events(struct Scsi_Host *sh)
{
   vmk_ScsiAdapter *vmkAdapter;
   struct vmklnx_ScsiAdapter *vmklnx26ScsiAdapter;
   VMK_ReturnStatus status;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(sh);

   if (sh == NULL || sh->adapter == NULL) {
      VMKLNX_WARN("vmklnx_iodm_disable_events is called with invalid "
                  "Scsi_Host(%p)!", sh);
      return;
   }

   vmklnx26ScsiAdapter = (struct vmklnx_ScsiAdapter *) sh->adapter;
   vmkAdapter = vmklnx26ScsiAdapter->vmkAdapter;
   status = vmk_IodmDisableEvents(vmkAdapter);

   if (status != VMK_OK) {
      VMKLNX_DEBUG(0, "Failed to disable IODM event collection for '%s' : %s",
                  vmklnx_get_vmhba_name(sh), vmk_StatusToString(status));
   } else {
      VMKLNX_DEBUG(2, "Disabled IODM event collection for '%s'",
                   vmklnx_get_vmhba_name(sh));
   }

   return;
}
EXPORT_SYMBOL(vmklnx_iodm_disable_events);
