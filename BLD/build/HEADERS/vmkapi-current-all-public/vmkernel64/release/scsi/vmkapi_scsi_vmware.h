/***************************************************************************
 * Copyright 2009 VMware, Inc.  All rights reserved.
 ***************************************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 * vmkapi_scsi_vmware.h --
 *
 *	Defines some of the VMware vendor specific SCSI interfaces
 *  
 */

#ifndef _VMKAPI_SCSI_VMWARE_H_
#define _VMKAPI_SCSI_VMWARE_H_

#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif

#include "scsi/vmkapi_scsi.h"

/*
 ***********************************************************************
 * vmk_ScsiSetATSCmdStatus --                                     */ /**
 *
 * \ingroup SCSI
 * \brief Set a command's ATS status
 *
 * On I/O completion, set the command's ATS status.
 *
 * \note This function will not block.
 *
 * \note Spin locks can be held while calling into this function
 *
 * \param[in] vmkCmd     Scsi command whose status to set
 * \param[in] miscompare Boolean to indicate whether the command's
 *                       status should be set to 
 *                       VMK_SCSI_PLUGIN_ATS_MISCOMPARE
 *
 * \pre if vmkCmd does not already indicate that the command failed with a
 *      check condition, this function is a no-op.
 *
 ***********************************************************************
 */
static inline void
vmk_ScsiSetATSCmdStatus(vmk_ScsiCommand *vmkCmd, vmk_Bool miscompare)
{
   if (vmk_ScsiCmdStatusIsCheck(vmkCmd->status)) {
      vmkCmd->status.plugin = miscompare ? VMK_SCSI_PLUGIN_ATS_MISCOMPARE :
         VMK_SCSI_PLUGIN_GOOD;
   }
}

#endif //_VMKAPI_SCSI_VMWARE_H_
