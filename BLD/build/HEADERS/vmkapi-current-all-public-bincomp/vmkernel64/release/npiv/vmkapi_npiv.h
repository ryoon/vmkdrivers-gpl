/* **********************************************************
 * Copyright 2008 - 2009 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 * vmkapi_npiv.h --
 *
 *    Defines some of the vmkernel specific VPORT types used to interact with
 *    NPIV VPORT aware drivers.
 *       Version 1 - ESX 3.5
 *       Version 2 - ESX 4.0
 */

#ifndef _VMKAPI_NPIV_H_
#define _VMKAPI_NPIV_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "npiv/vmkapi_npiv_wwn.h"

/*
 * Definitions
 */

/**
 * \brief VPORT API version values
 */
#define  VMK_VPORT_API_VERSION       0x00000002     // ESX 4.0 (Latest)
#define  VMK_VPORT_API_VERSION_35    0x00000001     // ESX 3.5

/**
 * \brief VPORT API vport specific error codes returned by the fc driver
 * 
 * Any of the VPORT API commands can return these errors
 */
#define VMK_VPORT_OK             0 /* command completed OK */
#define VMK_VPORT_ERROR         (-1) /* general vport error */
#define VMK_VPORT_INVAL         (-2) /* invalid value passed */
#define VMK_VPORT_NOMEM         (-3) /* no memory available for command */
#define VMK_VPORT_NORESOURCES   (-4) /* no vport resources */
#define VMK_VPORT_PARAMETER_ERR (-5) /* wrong parameter passed */

/**
 * \brief VPORT API default invalid count value
 */
#define  VMK_VPORT_CNT_INVALID   0xFFFFFFFF

/**
 * \brief VPORT API autoretry flag
 */
#define  VMK_VPORT_OPT_AUTORETRY 0x01

/**
 * \brief VPORT API Symbolic (VM) name - NULL terminated string associated with a vport
 *
 * This name will be used at the time of creating a Vport at fc driver. This is
 * an array of 128 bytes (max 128, so the max name length is 127 bytes + 1 NULL).
 */
#define	VMK_VPORT_VM_NAME_LENGTH 128

/**
 * \brief VPORT API future VSAN API use
 */
#define VMK_VPORT_VF_ID_UNDEFINED      0xFFFFFFFF 
#define	VMK_VPORT_FABRIC_NAME_LENGTH   8

/**
 * \brief  VPORT API bit masks for active FC4 roles
 */
#define VMK_VPORT_ROLE_FCP_INITIATOR   0x01
#define VMK_VPORT_ROLE_FCP_TARGET      0x02
#define VMK_VPORT_ROLE_IP_OVER_FC      0x04

/*
 * Data structures
 */

/**
 * \brief VPORT API commands
 *
 * These are the commands that are allowed via the NPIV API entry point
 */
typedef enum {
   VMK_VPORT_CREATE     = 1, /* Create a vport */
   VMK_VPORT_DELETE     = 2, /* Delete a vport */
   VMK_VPORT_INFO       = 3, /* Get vport info from phys hba */
   VMK_VPORT_TGT_REMOVE = 4, /* Target Remove (not used) */
   VMK_VPORT_SUSPEND    = 5  /* suspend/resume vport */
} vmk_VportOpCmd;

/**
 * \brief VPORT API link type
 *
 * Used by underlying transport to inform link type to scsi midlayer
 */
typedef enum  {
   VMK_VPORT_TYPE_PHYSICAL = 0, /* physical FC-Port */
   VMK_VPORT_TYPE_VIRTUAL       /* Virtual Vport Port */
} vmk_VportLinkType;


/**
 * \brief VPORT API port state
 */
typedef enum {
   VMK_VPORT_STATE_OFFLINE = 0, /* vport is offline */
   VMK_VPORT_STATE_ACTIVE,      /* vport is active */
   VMK_VPORT_STATE_FAILED       /* vport is failed */
} vmk_VporState;

/**
 * \brief VPORT API port specific fail reason
 */
typedef enum {
   VMK_VPORT_FAIL_UNKNOWN = 0,     /* vport fail unknown reason */
   VMK_VPORT_FAIL_LINKDOWN,        /* physical link is down */
   VMK_VPORT_FAIL_FAB_UNSUPPORTED, /* san fabric does not support npiv */
   VMK_VPORT_FAIL_FAB_NORESOURCES, /* not enough reasources in san fabric */
   VMK_VPORT_FAIL_FAB_LOGOUT,      /* san logged out the vport */
   VMK_VPORT_FAIL_ADAP_NORESOURCES 
} vmk_VportFailReason;

/**
 * \brief VPORT API vport-type flag used by scsi midlayer
 *
 * Vport type flag passed using vmk_ScsiVportArgs.flags used scsi midlayer
 * to inform vmkernel about the vport type backing a vmkernel adapter.
 */
typedef enum {
   VMK_SCSI_VPORT_TYPE_LEGACY   = 0x00000001, /* vport hba is legacy vport */
   VMK_SCSI_VPORT_TYPE_FULLHBA  = 0x00000002, /* vport hba has full HBA status */
   VMK_SCSI_VPORT_TYPE_PASSTHRU = 0x00000004  /* vport hba is a passthru */
} vmk_ScsiVportTypeFlags;

/**
 * \brief VPORT API Args list is used to pass arguments to lowlevel underlying
 * transport layer and eventually to HBA driver through the NPIV API
 *
 * This Structure is used for all types of vport calls made from vmkernel
 * to underlying transport layer.
 */
typedef struct vmk_ScsiVportArgs {
   /** \brief Node World Wide Name */
   vmk_VportWwn wwpn;
   /** \brief Port World Wide Name */
   vmk_VportWwn wwnn;
   /** \brief Vport's ScsiHost pointer */
   void *virthost;
   /** \brief Reserved for arguments structure */
   void         *arg;
   /** \brief Symbolic name, generally VM's name is used */
   char         *name;
   /** \brief Vport's Scsi Adapter */
   void         *virtAdapter;
   /** \brief Vport type flag, internal use only */
   vmk_uint32   flags;
} vmk_ScsiVportArgs;

/**
 * \brief VPORT API This Structure is used to get the NPIV specific information
 * from physical host bus adapter.
 *
 * Info structure returned by the VMK_VPORT_INFO command made to the physical
 * HBA adapter
 */
typedef struct vmk_VportInfo {
   /** \brief Vport API version */
   vmk_uint32          api_version;
   /** \brief Vport link type */
   vmk_VportLinkType   linktype;
   /** \brief State of vport support */
   vmk_VporState       state;
   /** \brief reason for VportInfo failure */
   vmk_VportFailReason fail_reason;
   /** \brief previous reason for VportInfo failure */
   vmk_VportFailReason prev_fail_reason;
   /** \brief Node World Wide Name */
   vmk_VportWwn        node_name;
   /** \brief Port World Wide Name */
   vmk_VportWwn        port_name;

   /* Following values are valid only on physical ports */
   /** \brief maximum number of vports supported by fc hba */
   vmk_uint32          vports_max;
   /** \brief number of vports that are in use on fc hba */
   vmk_uint32          vports_inuse;
   /** \brief maximum number of RPIs available in the fc FW */
   vmk_uint32          rpi_max;
   /** \brief number of RPIs currently in-use in the fc FW */
   vmk_uint32          rpi_inuse;

   /* QoS Values */
   /** \brief QoS (Quality of Service) Priority */
   vmk_uint8           QoSPriority;
   /** \brief QoS (Quality of Service) Bandwidth percentage */
   vmk_uint8           QosBandwith;

   /** \brief Virtual SAN (VSAN) number */
   vmk_int16           vsan_number;
   /** \brief Virtual Fabric (VF) id number, undefined value is -1 */
   vmk_int32           vf_id;
   /** \brief Vport role id initiator/target etc */
   vmk_int32           role_id;
} vmk_VportInfo;

/**
 * \brief  VPORT API This structure is to communicate with the host bus adapter
 * driver at the time of vport create operation.
 *
 * On success, a new ScsiHost will be assigned to vport_shost for the newly
 * created virtual port.
 */
struct vmk_VportData {
   /** \brief Vport API version */
   vmk_uint32   api_version;
   /** \brief optional values */
   vmk_uint32   options;
   /** \brief Node World Wide Name */
   vmk_VportWwn node_name;
   /** \brief Port World Wide Name */
   vmk_VportWwn port_name;
   /** \brief Vport's ScsiHost pointer */
   void *vport_shost;

   /** \brief fabric name, zero's if direct connect/private loop */
   vmk_uint8    fabric_name[VMK_VPORT_FABRIC_NAME_LENGTH];

   /* QoS Values */
   /** \brief QoS (Quality of Service) Priority */
   vmk_uint8    QoSPriority;
   /** \brief QoS (Quality of Service) Bandwidth percentage */
   vmk_uint8    QosBandwith;

   /** \brief Virtual SAN (VSAN) number */
   vmk_int16    vsan_number;
   /** \brief Virtual Fabric (VF) id number, undefined value is -1 */
   vmk_int32    vf_id;
   /** \brief Vport role id initiator/target etc */
   vmk_int32    role_id;

   /** \brief Symbolic name, generally VM's name is used */
   vmk_int8     symname[VMK_VPORT_VM_NAME_LENGTH];
};

#endif /* _VMKAPI_NPIV_H_ */
