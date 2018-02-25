/***********************************************************
 * Copyright 2008 - 2014 VMware, Inc.  All rights reserved.
 ***********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Portset                                                      */ /**
 * \addtogroup VDS
 * @{
 * \defgroup VDSProp VDS Property
 *
 * This file defines VDS property names and data structures for
 * communication between management apps and data plan (portset
 * implementation). There are three types of properties:
 * - VDS global properties
 * - VDS host properties
 * - VDS port properties.
 *
 * VDS global properties are global to a VDS instance. The properties
 * are present on every host member of VDS and the property values are
 * identical on every host. Such properties are set/cleared by vCenter
 * are read-only from host's perspective.
 *
 * Host properties are associated with portset on each host. The property
 * value may vary from host to host. Some host properties are set by
 * VDS framework to describe the host implementation. Other host properties
 * are set by implementation to report the status.
 *
 * Port properties are associated with individual VDS ports. Some port
 * properties are set by vCenter to communicate the configuration setting.
 * Other properties are set by the switch data plane report port status
 * and statistics.
 *
 * An implementation registers portset clients to handle VDS global and
 * host properties. It registers port client callbacks to handle port
 * property changes.
 *
 * Property names follow conventions defined here
 * - Common Properties ("com.vmware.common...") are shared across all
 *   vendors, as well as by VMware VDS.
 * - Vendors Properties ("com.<vendor>....") are controlled by the
 *   VDS provider. Such properties are not interpreted by ESX.
 * Vendor properties are accessed from VMOL interfaces as opaque properties.
 * 
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_VDS_PROP_H_
#define _VMKAPI_VDS_PROP_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/**
 *  \brief Global property for DVS name.
 *
 *  It is typically set when a host joins a VDS.
 */
#define VMK_VDSPROP_SWITCH_ALIAS          "com.vmware.common.alias"

/**
 *  \brief Global property for VDS version.
 *
 *  It is set when a host joins a vDS and is tied to the lowest version of
 *  the ESX on which the switch can be enabled.
 */

#define VMK_VDSPROP_SWITCH_VERSION          "com.vmware.common.version"
/**
 *  \brief Property for associating uplink ports to DVS.
 * 
 *  This property will be deprecated. Do not use.
 */
#define VMK_VDSPROP_SWITCH_UPLINKPORTS    "com.vmware.common.uplinkPorts"

/**
 *  \brief Property of Overlay.
 *
 *  This property will be deprecated. Do not use.
 */
#define VMK_VDSPROP_SWITCH_OVERLAY        "com.vmware.common.overlay"

/**
 *  \brief Property of resource pools.
 *
 *  This property will be deprecated. Do not use.
 */  
#define VMK_VDSPROP_SWITCH_RESPOOLS_LIST  "com.vmware.common.respools.list"

/**
 *  \brief Property of resource pool scheduler.
 *
 *  This property will be deprecated. Do not use.
 */
#define VMK_VDSPROP_SWITCH_RESPOOLS_SCHED "com.vmware.common.respools.sched"

/**
 * \brief VDS host dataplane properties.
 *
 *  Volatile properties are not persisted in vCenter.
 */
#define VMK_VDSPROP_HOST_STATUS   "com.vmware.common.host.volatile.status"

/**
 * \brief Property of VDS host dataplane states.
 *
 * Vendor should register portset client callbacks for this property to receive
 * notification of portset state change. Supported states for notification are
 * defined in vmk_VDSPropPortsetState. The write callback of this property can
 * be blockable, which means vmkernel will wait for the return of callbacks
 * before changing state.
 *
 * Volatile property is not persisted in vCenter
 */
#define VMK_VDSPROP_HOST_PROXY_STATE \
   "com.vmware.common.host.portset.volatile.state"


/**
 * \brief VDS host dataplane portset name
 */
#define VMK_VDSPROP_HOST_PROXY            "com.vmware.common.host.portset"

/**
 * \brief Property describing uplink ports on a DVS.
 *
 * This property will be deprecated. Do not use.
 */
#define VMK_VDSPROP_HOST_UPLINKPORTS      "com.vmware.common.host.uplinkPorts"

/**
 * \brief Volatile port properties are not persisted
 */
#define VMK_VDSPROP_PORT_STATUS           "com.vmware.common.port.volatile.status"
#define VMK_VDSPROP_PORT_TYPE             "com.vmware.common.port.volatile.type"
#define VMK_VDSPROP_PORT_VLAN             "com.vmware.common.port.volatile.vlan"
#define VMK_VDSPROP_PORT_PERSIST          "com.vmware.common.port.volatile.persist"

/**
 * \brief Port properties, configured/persisted from vCenter
 */
#define VMK_VDSPROP_PORT_PTALLOWED        "com.vmware.common.port.ptAllowed"
#define VMK_VDSPROP_PORT_PTALLOWEDRT      "com.vmware.common.port.ptAllowedRT"
#define VMK_VDSPROP_PORT_STATISTICS       "com.vmware.common.port.statistics"
#define VMK_VDSPROP_PORT_ALIAS            "com.vmware.common.port.alias"
#define VMK_VDSPROP_PORT_CONNECTID        "com.vmware.common.port.connectid"
#define VMK_VDSPROP_PORT_VMKNIC_VLANID    "com.vmware.common.port.vmknicVlanID"
#define VMK_VDSPROP_PORT_VNIC_CONNECTEE   "com.vmware.common.port.vnicConnectee"
#define VMK_VDSPROP_PORT_PORTGROUP        "com.vmware.common.port.portgroupid"
#define VMK_VDSPROP_PORT_BLOCK            "com.vmware.common.port.block"
#define VMK_VDSPROP_PORT_LINK_STATUS      "com.vmware.common.port.linkstatus"
#define VMK_VDSPROP_PORT_RESPOOLS_CFG     "com.vmware.common.port.respools.cfg"
#define VMK_VDSPROP_PORT_RESPOOL_ASSOC    "com.vmware.common.port.respool.assoc"


/**
 * \brief Port properties for shaper, overlay, dvfilter modules.
 *
 * These properties will be deprecated.
 */
#define VMK_VDSPROP_PORT_SHAPER_INPUT     "com.vmware.common.port.shaper.input"
#define VMK_VDSPROP_PORT_SHAPER_BROADCAST "com.vmware.common.port.shaper.broadcast"
#define VMK_VDSPROP_PORT_SHAPER_OUTPUT    "com.vmware.common.port.shaper.output"
#define VMK_VDSPROP_PORT_OVERLAY          "com.vmware.common.port.overlay"
#define VMK_VDSPROP_PORT_DVFILTER         "com.vmware.common.port.dvfilter"

/**
  * \brief Block the port due to forward engine not ready
  *        The property data is a string of "true" or "false".
  */
#define VMK_VDSPORT_FORWARD_NOT_READY_BLOCK "com.vmware.port.forward.not.ready.block"

/**
 * \brief For Cisco to show CDP info on the UI. It should be removed later.
 */
#define VMK_CISCO_PROP_SWITCH_CDP               "com.cisco.svs.switch.cdp"

/**
 * \brief Event notification about portset state changes.
 *
 * This is used with VMK_VDSPROP_HOST_PROXY_STATE property
 */
typedef enum {
   /* Portset is going to be deactivated */ 
   VMK_VDSPORTSET_STATE_DEACTIVE = 0x1,

   /** Portset is going to be hotswapped to a different switch class */
   VMK_VDSPORTSET_STATE_HOTSWAP = 0x2,
} vmk_VDSPropPortsetState;

/**
 * \brief Maximum buffer length required for a vDS ID string.
 *
 * This buffer length includes the NULL terminator.
 */
#define VMK_VDS_ID_MAX_LEN 64

/**
 * \brief Maximum buffer length required for a vDS name string.
 *
 * This buffer length includes the NULL terminator.
 */
#define VMK_VDS_NAME_MAX_LEN 81

/**
 * \brief Maximum buffer length required for a vDS class name string.
 *
 * This buffer length includes the NULL terminator.
 */
#define VMK_VDS_CLASS_NAME_MAX_LEN 128

/**
 * \brief Maximum buffer length required for a vDS port ID string.
 *
 * This buffer length includes the NULL terminator.
 */
#define VMK_VDS_PORT_ID_MAX_LEN 48

/**
 * \brief Maximum buffer length required for a vDS port ID string
 *        on DVS switch which is compatible with ESX5.5 or earlier
 *        release.
 *
 * This buffer length includes the NULL terminator.
 */
#define VMK_VDS_PORT_ID_LEGACY_MAX_LEN 16


/**
 * \brief Maximum buffer length required for a vDS port alias string.
 *
 * This buffer length includes the NULL terminator.
 */
#define VMK_VDS_PORT_ALIAS_MAX_LEN 81

/**
 * \brief Port status flag.
 *
 * A port can have multiple status bits set
 * (i.e. VMK_VDSPORT_STATUS_INUSE | VMK_VDSPORT_STATUS_LINKUP).
 */
typedef enum {
   VMK_VDSPORT_STATUS_INUSE = 0x1,
   VMK_VDSPORT_STATUS_LINKUP = 0x2,
   VMK_VDSPORT_STATUS_BLOCKED = 0x4,
   /**
    * This flag should be ignored when the solution
    * doesn't support vNIC passthrough
    */
   VMK_VDSPORT_STATUS_PASSTHRU = 0x8
} vmk_VDSPortStatusFlag;

/**
 * \brief Type of data item.
 *
 * It is passed in to callback registered for VMK_VDSPROP_PORT_STATUS property.
 */  
typedef struct vmk_VDSPortStatus {
   /** Port status flags defined in vmk_VDSPortStatusFlag */ 
   vmk_uint64 flags;

   /**
    * Flag indicating why vNIC passthrough is disabled. This flag should be
    * ignored when 3rd party solution doesn't support vNIC passthrough.
    */ 
   vmk_uint64 noPassthruReason;

   /** Ephemeral port ID of the port */ 
   vmk_uint32 portID;

   /**
    * Display string to report port state, for example "Port blocked by
    * admin"
    */ 
   char   displayStr[80];
} VMK_ATTRIBUTE_PACKED vmk_VDSPortStatus;


/**
 * \brief Port statistics structure.
 *
 * This is the type of data item passed in to callback registered
 * for VMK_VDSPROP_PORT_STATISTICS property and vmk_PortGetStats
 * portset callback.
 */
typedef struct vmk_VDSPortStatistics {
   vmk_uint64 pktsInUnicast;
   vmk_uint64 bytesInUnicast;
   vmk_uint64 pktsInMulticast;
   vmk_uint64 bytesInMulticast;
   vmk_uint64 pktsInBroadcast;
   vmk_uint64 bytesInBroadcast;
   vmk_uint64 pktsOutUnicast;
   vmk_uint64 bytesOutUnicast;
   vmk_uint64 pktsOutMulticast;
   vmk_uint64 bytesOutMulticast;
   vmk_uint64 pktsOutBroadcast;
   vmk_uint64 bytesOutBroadcast;
   vmk_uint64 pktsInDropped;
   vmk_uint64 pktsOutDropped;
   vmk_uint64 pktsInException;
   vmk_uint64 pktsOutException;
} VMK_ATTRIBUTE_PACKED vmk_VDSPortStatistics;

/**
 * \brief Type of VlanID range. 
 */  
typedef struct vmk_VDSPortVIDRange {
   vmk_uint16 minVID;
   vmk_uint16 maxVID;
} VMK_ATTRIBUTE_PACKED vmk_VDSPortVIDRange;

/**
 * \brief Port VLAN info structure
 *
 * This is the type of data item passed in to callback registered for
 * VMK_VDSPROP_PORT_VLAN property.
 */
typedef struct vmk_VDSPortVlanInfo {
   vmk_uint16 vlanID;
   vmk_uint16 numRanges;
   vmk_VDSPortVIDRange vidRanges[0];
} VMK_ATTRIBUTE_PACKED vmk_VDSPortVlanInfo;

/**
 * \brief Virtual distributed switch host status.
 *
 * It reflects whether VDS is functional on a particular host. vSphere
 * looks at the host status when making VM placement decisions.
 */
typedef enum {
   /** VDS is in full service. */ 
   VMK_VDS_STATUS_OK = 0x0,

   /** VDS is at reduced service. */
   VMK_VDS_STATUS_DEGRADED = 0x1,

   /** VDS is out of service. */
   VMK_VDS_STATUS_DOWN = 0x2

} vmk_VDSHostStatus;

#if defined(VMK_BUILDING_FOR_KERNEL_MODE)
/*
 ***********************************************************************
 * vmk_VDSGlobalPropLookup --                                     */ /**
 *
 * \brief Lookup a VDS global property by name, and copy out its value
 *
 * \note  The global property dataValue must be linear data
 *
 * \param[in]  psName            Name of the portset/VDS
 * \param[in]  dataName          Name of the VDS global property
 * \param[in]  bufLength         Length of the buffer for data
 * \param[out] dataValue         Value of the property data
 * \param[out] dataLength        Length of the property data
 *
 * \retval     VMK_OK            Get the global property value
 * \retval     VMK_BAD_PARAM     Invalid psName, dataName, dataValue or
 *                               dataLength
 * \retval     VMK_NOT_FOUND     The specified VDS or global property is
 *                               not found
 * \retval     VMK_BUF_TOO_SMALL The length of the provided buffer is
 *                               not enough to save the found property
 *                               data
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_VDSGlobalPropLookup(const char *psName,
                                         const char *dataName,
                                         vmk_ByteCountSmall bufLength,
                                         vmk_AddrCookie dataValue,
                                         vmk_ByteCountSmall *dataLength);

#endif /* VMK_BUILDING_FOR_KERNEL_MODE */

#endif
/** @} */
/** @} */
