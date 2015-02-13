/* **********************************************************
 * Copyright 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * DCB
 * \addtogroup Network
 *@{
 * \defgroup DCB DCB (Data Center Bridging) Interfaces
 *
 * DCB (Data Center Bridging) is set of extension protocols that try to
 * enhance ethernet for use in data centers. This group defines DCB data
 * structures and APIs for getting/setting DCB parameters from/to the
 * hardware.
 *
 * This implementation is based on the DCB spec Rev1.01. Priority-based
 * Flow Control, Priority Group, and Application structures are all
 * derived from the DCB spec Rev 1.01. 
 *
 * Link to the DCB spec Rev 1.01:
 * http://www.ieee802.org/1/files/public/docs2008/az-wadekar-dcbx-capability-exchange-discovery-protocol-1108-v1.01.pdf
 *
 *@{
 *
 ***********************************************************************
 */
#ifndef _VMKAPI_NET_DCB_H_
#define _VMKAPI_NET_DCB_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/** Maximum number of different User Priority */
#define VMK_DCB_MAX_PRIORITY_COUNT          8

/** Maximum number of Priority Groups */
#define VMK_DCB_MAX_PG_COUNT                8

/** Maximum number of DCB Applications */
#define VMK_DCB_MAX_APP_COUNT              16

/**
 * \brief DCB version
 */
typedef struct vmk_DCBVersion {
   /**
    * \brief Major version of DCB
    */
   vmk_uint8 majorVersion;

   /**
    * \brief Minor version of DCB
    */
   vmk_uint8 minorVersion;
} vmk_DCBVersion;

/**
 * \brief Traffic Classes information of the device.
 *
 * Traffic Class information indicates how many Traffic Classes supported
 * by the hardware for Priority Group and Priority-based Flow Control.
 */
typedef struct vmk_DCBNumTCs {
   /**
    * \brief Number of Traffic Classes supported for Priority Group.
    */
   vmk_uint8 pgTcs;

   /**
    * \brief Number of Traffic Classes supported for Priority-based
    * Flow Control.
    */
   vmk_uint8 pfcTcs;
} vmk_DCBNumTCs;

/**
 * \brief DCB Priority Group parameters of the device.
 *
 * DCB Priority Group Parameters include User Priority to Priority Group
 * mapping and bandwidth divvying between Priority Groups.
 */
typedef struct vmk_DCBPriorityGroup {
   /** \brief Maps PGID : Link Bandwidth %. */
   vmk_uint8 pgBwPercent[VMK_DCB_MAX_PG_COUNT];

   /** \brief Maps UP : PGID.
    *
    * upToPgIDMap[X] = Y indicates that UP X belongs to Priority Group Y.
    */
   vmk_uint8 upToPgIDMap[VMK_DCB_MAX_PRIORITY_COUNT];
} vmk_DCBPriorityGroup;

/**
 * \brief DCB Priority-based Flow Control parameters.
 *
 * DCB Priority-based Flow Control Parameters indicate if PFC is enabled on a
 * certain priority.
 */
typedef struct vmk_DCBPriorityFlowControlCfg {
   /** \brief PFC configuration. */
   vmk_Bool pfcEnable[VMK_DCB_MAX_PRIORITY_COUNT];
} vmk_DCBPriorityFlowControlCfg;

/**
 * \brief DCB capabilities of the device.
 *
 * DCB capability attributes indicates whether the hardware supports a
 * specific DCB capability or not. DCB capabilities include Priority Group,
 * Priority-based Flow Control, and Traffic Classes supported.
 */
typedef struct vmk_DCBCapabilities {
   /** \brief Capability of Priority Groups. */
   vmk_Bool priorityGroup;

   /** \brief Capability of Priority-based Flow Control. */
   vmk_Bool priorityFlowControl;

   /** \brief Number of PG Traffic Classes supported. */
   vmk_uint8 pgTrafficClasses;

   /** \brief Number of PFC Traffic Classes supported. */
   vmk_uint8 pfcTrafficClasses;
} vmk_DCBCapabilities;

/**
 * \brief DCB Application Protocol Selector Field.
 *
 * DCB Application Protocol Selector Field types.
 */
typedef enum {
   /** \brief App Proto ID carries L2 EtherType. */
   VMK_DCB_APP_L2_ETHTYPE           = 0x0,

   /** \brief App Proto ID carries Socket Number (TCP/UDP). */
   VMK_DCB_APP_SOCKET_NUM           = 0x1,

   /** \brief Reserved. */
   VMK_DCB_APP_RSVD1                = 0x2,

   /** \brief Reserved. */
   VMK_DCB_APP_RSVD2                = 0x3,
} vmk_DCBAppSelectorField;

/**
 * \brief DCB Application Protocol parameters.
 *
 * DCB Application Protocol parameters indicate whether a DCB Application
 * Protocol is enabled, and which priority it uses.
 */
typedef struct vmk_DCBApplication {
   /** \brief Whether this Application is enabled. */
   vmk_Bool enabled;

   /** \brief Selector Field. */
   vmk_DCBAppSelectorField sf;

   /** \brief Application Protocol ID. */
   vmk_uint16 protoID;

   /** \brief 802.1p User Priority. */
   vmk_VlanPriority priority;
} vmk_DCBApplication;

/**
 * \brief All DCB Application Protocols been supported.
 */
typedef struct vmk_DCBApplications {
   /** \brief Stores all available DCB Application Protocols. */
   vmk_DCBApplication app[VMK_DCB_MAX_APP_COUNT];
} vmk_DCBApplications;

/**
 * \brief Cached DCB configuration data of the device associated to an uplink.
 */
typedef struct vmk_DCBConfig {
   /** \brief DCB version. */
   vmk_DCBVersion version;

   /** \brief Whether DCB is enabled on the device. */
   vmk_Bool dcbEnabled;

   /** \brief Traffic Classes information of the device. */
   vmk_DCBNumTCs numTCs;

   /** \brief DCB Priority Group parameters of the device. */
   vmk_DCBPriorityGroup priorityGroup;

   /** \brief Whether Priority-based Flow Control is enabled on the device. */
   vmk_Bool pfcEnabled;

   /** \brief DCB Priority-based Flow Control parameters. */
   vmk_DCBPriorityFlowControlCfg pfcCfg;

   /** \brief DCB capabilities of the device. */
   vmk_DCBCapabilities caps;

   /** \brief DCB application settings of the device. */
   vmk_DCBApplications apps;
} vmk_DCBConfig;

#endif /* _VMKAPI_NET_DCB_H_ */
/** @} */
/** @} */
