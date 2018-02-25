/* **********************************************************
 * Copyright 2008 - 2012 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 * vmkapi_npiv.h --
 *
 *    Defines some of the vmkernel specific VPORT types used to interact with
 *    NPIV VPORT aware drivers.
 */

#ifndef _VMKAPI_NPIV_H_
#define _VMKAPI_NPIV_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "npiv/vmkapi_npiv_wwn.h"

/**
 * \brief VPORT API commands
 *
 * These are the commands that are allowed in the "cmd" field 
 * via the NPIV API entry point "vportOp".
 */
typedef enum vmk_VportOpCmd {
   VMK_VPORT_CREATE     = 1, /** Create a vport */
   VMK_VPORT_DELETE     = 2, /** Delete a vport */
   VMK_VPORT_INFO       = 3, /** Get vport info from phys hba */
   VMK_VPORT_SUSPEND    = 4  /** suspend/resume vport */
} vmk_VportOpCmd;

/**
 * \brief VPORT API Flags passed by the VMKernel to the driver
 *
 */
typedef enum vmk_ScsiVportFlags {
   /** Create a "Legacy" VPORT
    *  Legacy VPORT will have no presence in the PSA stack,
    *  i.e. no paths or associated device.
    *  Non-Legacy VPORTs are created just like any other adapter */
   VMK_SCSI_VPORT_FLAG_LEGACY   = 0x00000001, 
   /** VMK_VPORT_SUSPEND VPORT flag
    *  Suspend SAN operations on a running VPORT when set on a
    *  VMK_VPORT_SUSPEND command. Resume SAN operations on a suspened
    *  VPORT when not set on a VMK_VPORT_SUSPEND command */
   VMK_SCSI_VPORT_FLAG_SUSPEND  = 0x00000002,
} vmk_ScsiVportFlags;

/**
 * \brief VPORT API Args list is used to pass arguments to lowlevel underlying
 * transport layer and eventually to an HBA driver through the NPIV API
 *
 * This Structure is passed for all vmk_VportOpCmd command calls made from the
 * vmkernel to the underlying transport layer.
 *
 * Field Usage per command (unused fields are 0):
 * VMK_VPORT_CREATE:
 *    wwpn, wwnn (IN) - passes the wwn pair of the VPORT to be created
 *    flags (IN) - Legacy flag
 *    virtAdapter (OUT) - vmk_ScsiAdapter pointer for VPORT returned by the driver
 * VMK_VPORT_DELETE:
 *    virtAdapter (IN) - virtual port to delete
 * VMK_VPORT_INFO:
 *    info (IN/OUT) - pointer to the vmk_VportInfo struct allocated by the
 *                    VMKernel, but filled in by the driver.
 * VMK_VPORT_SUSPEND:
 *    virtAdapter (IN) - virtual port to suspend or resume
 *    flags (IN) - Suspend flag
 *
 */
typedef struct vmk_ScsiVportArgs {
   /** \brief Node World Wide Name */
   vmk_VportWwn wwpn;
   /** \brief Port World Wide Name */
   vmk_VportWwn wwnn;
   /** \brief vmk_ScsiAdapter Adapter pointer */
   void         *virtAdapter;
   /** \brief vmk_VportInfo struct passed on VMK_VPORT_INFO calls, NULL otherwise */
   void         *info;
   /** \brief Vport flags */
   vmk_ScsiVportFlags flags;
} vmk_ScsiVportArgs;

/**
 * \brief VPORT API link type
 *
 * Used by underlying transport to inform the VMKernel of the link type
 */
typedef enum vmk_VportLinkType {
   VMK_VPORT_TYPE_PHYSICAL = 0, /** physical FC-Port */
   VMK_VPORT_TYPE_VIRTUAL       /** Virtual Vport Port */
} vmk_VportLinkType;

/**
 * \brief VPORT API port state
 */
typedef enum vmk_VportState {
   VMK_VPORT_STATE_OFFLINE = 0, /** vport is offline */
   VMK_VPORT_STATE_ACTIVE,      /** vport is active */
   VMK_VPORT_STATE_FAILED       /** vport is failed */
} vmk_VportState;

/**
 * \brief VPORT API port specific fail reason
 */
typedef enum vmk_VportFailReason {
   VMK_VPORT_FAIL_UNKNOWN = 0,     /** vport fail unknown reason */
   VMK_VPORT_FAIL_LINKDOWN,        /** physical link is down */
   VMK_VPORT_FAIL_FAB_UNSUPPORTED, /** san fabric does not support npiv */
   VMK_VPORT_FAIL_FAB_NORESOURCES, /** not enough reasources in san fabric */
   VMK_VPORT_FAIL_FAB_LOGOUT,      /** san logged out the vport */
   VMK_VPORT_FAIL_ADAP_NORESOURCES 
} vmk_VportFailReason;

/**
 * \brief VPORT API default invalid count value for vports_max and vports_inuse.
 */
#define  VMK_VPORT_CNT_INVALID   0xFFFFFFFF

/**
 * \brief VPORT API This Structure is used to get the NPIV specific information
 * from a physical host bus adapter.
 *
 * Info structure allocated by the VMKernel and filled in by the VMK_VPORT_INFO
 * command made to the physical HBA adapter driver.
 */
typedef struct vmk_VportInfo {
   /** \brief Vport link type */
   vmk_VportLinkType   linktype;
   /** \brief State of vport support */
   vmk_VportState       state;
   /** \brief reason for VportInfo failure */
   vmk_VportFailReason fail_reason;
   /** \brief previous reason for VportInfo failure */
   vmk_VportFailReason prev_fail_reason;
   /** \brief Node World Wide Name */
   vmk_VportWwn        node_name;
   /** \brief Port World Wide Name */
   vmk_VportWwn        port_name;

   /** Following values are valid only on physical ports */
   /** \brief maximum number of vports supported by fc hba */
   vmk_uint32          vports_max;
   /** \brief number of vports that are in use on fc hba */
   vmk_uint32          vports_inuse;
} vmk_VportInfo;

#endif /* _VMKAPI_NPIV_H_ */
