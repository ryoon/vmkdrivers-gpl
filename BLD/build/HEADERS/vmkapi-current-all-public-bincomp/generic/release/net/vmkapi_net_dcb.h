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

/** Maximum number of Traffic Classes Supported */
#define VMK_DCB_IEEE_MAX_TCS                8

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
 * \brief DCBX Protocol Subtype Capability Field
 *
 * DCB Protocol subtype information field indicating the negotiated
 * mode that the device should use.
 */
typedef enum vmk_DCBSubType {
    /* \brief DCBX negotiation subtype is invalid */
   VMK_DCB_CAP_DCBX_SUBTYPE_INVALID = 0,

    /* \brief DCBX pre CEE protocol subtype version 1.0 */
   VMK_DCB_CAP_DCBX_SUBTYPE_PRE_CEE = 0x01,

    /* \brief DCBX CEE protocol subtype verion 1.1 */
   VMK_DCB_CAP_DCBX_SUBTYPE_CEE     = 0x02,

    /* \brief DCBX IEEE protocol subtype version 2.0 */
   VMK_DCB_CAP_DCBX_SUBTYPE_IEEE    = 0x03,

    /* \brief DCBX engine static configuration, no host negotiation */
   VMK_DCB_CAP_DCBX_SUBTYPE_STATIC  = 0x10,
} vmk_DCBSubType;

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
typedef enum vmk_DCBAppSelectorField {
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

/**
 * \brief DCB IEEE Priority-based Flow Control parameters.
 *
 * DCB IEEE Priority-based Flow Control Parameters for a given device
 */
typedef struct vmk_DCBIEEEPfcCfg {
   /** \brief Indicates the number of traffic classes on the local device. */
   vmk_uint8  pfcCap;
   /** \brief Bitmap indicating pfc enabled traffic classes. */
   vmk_uint8  pfcEnabled;
   /** \brief Enable MAC Security bypass capability. */
   vmk_uint8  pfcMbc;
   /** \brief Allowance made for a round-trip propagation delay of link. */
   vmk_uint16 pfcDelay;
   /** \brief Count of the sent pfc frames. */
   vmk_uint64 pfcReq[VMK_DCB_IEEE_MAX_TCS];
   /** \brief Count of the received pfc frames. */
   vmk_uint64 pfcInd[VMK_DCB_IEEE_MAX_TCS];
} vmk_DCBIEEEPfcCfg;

/**
 * \brief DCB IEEE ETS Control parameters.
 *
 * DCB IEEE Enhanced Transmission Selection Control Parameters for a given device
 */
typedef struct vmk_DCBIEEEEtsCfg {
   /** \brief Willing bit in ETS configuration TLV. */
   vmk_uint8  etsWilling;
   /** \brief Indicates supported capacity of ets feature. */
   vmk_uint8  etsCap;
   /** \brief Credit based shaper ets algorithm supported. */
   vmk_uint8  etsCbs;
   /** \brief Tc tx bandwidth indexed by traffic class. */
   vmk_uint8  etsTcTxBw[VMK_DCB_IEEE_MAX_TCS];
   /** \brief Tc rx bandwidth indexed by traffic class. */
   vmk_uint8  etsTcRxBw[VMK_DCB_IEEE_MAX_TCS];
   /** \brief TSA Assignment table, indexed by traffic class. */
   vmk_uint8  etsTcTsa[VMK_DCB_IEEE_MAX_TCS];
   /** \brief Priority assignment table mapping 8021Qp to traffic class. */
   vmk_uint8  etsPrioTc[VMK_DCB_IEEE_MAX_TCS];
   /** \brief Recommended tc bandwidth indexed by traffic class for TLV. */
   vmk_uint8  etsTcRecoBw[VMK_DCB_IEEE_MAX_TCS];
   /** \brief Recommended tc bandwidth indexed by traffic class for TLV. */
   vmk_uint8  etsTcRecoTsa[VMK_DCB_IEEE_MAX_TCS];
   /** \brief Recommended tc tx bandwidth indexed by traffic class for TLV. */
   vmk_uint8  etsRecoPrioTc[VMK_DCB_IEEE_MAX_TCS];
} vmk_DCBIEEEEtsCfg;

/*
 ***********************************************************************
 * vmk_UplinkDCBIsEnabledCB --                                    */ /**
 *
 * \brief  Handler used by vmkernel to check whether DCB support is
 *         enabled on the device.
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[out]  enabled      Used to store the DCB state of the device.
 * \param[out]  version      Used to store the DCB version supported by
 *                           the device.
 *
 * \retval      VMK_OK       If operation succeeds.
 * \retval      VMK_FAILURE  If the operation fails or if the device is
 *                           not DCB capable.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkDCBIsEnabledCB)(vmk_AddrCookie driverData,
                                                     vmk_Bool *enabled,
                                                     vmk_DCBVersion *version);


/*
 ***********************************************************************
 * vmk_UplinkDCBEnableCB --                                       */ /**
 *
 * \brief  Handler used by vmkernel to enable DCB support on the device.
 *
 * \note   It should only be called from the DCB daemon that does
 *         DCB negotiation on behalf of this device.
 *
 * \note   Uplink layer will call vmk_UplinkDCBApplySettingsCB() after
 *         this call to guarantee the changes will be flushed onto the
 *         device.
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 *
 * \retval      VMK_OK       If operation succeeds.
 * \retval      VMK_FAILURE  If operation fails.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkDCBEnableCB)(vmk_AddrCookie driverData);


/*
 ***********************************************************************
 * vmk_UplinkDCBDisableCB --                                      */ /**
 *
 * \brief  Handler used by vmkernel to disable DCB support on the device.
 *
 * \note   It should only be called from the DCB daemon that does
 *         DCB negotiation on behalf of this device.
 *
 * \note   Uplink layer will call vmk_UplinkDCBApplySettingsCB() after
 *         this call to guarantee the changes will be flushed onto the
 *         device.
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 *
 * \retval      VMK_OK       If operation succeeds.
 * \retval      VMK_FAILURE  If operation fails.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkDCBDisableCB)(vmk_AddrCookie driverData);


/*
 ***********************************************************************
 * vmk_UplinkDCBNumTCsGetCB --                                    */ /**
 *
 * \brief  Handler used by vmkernel to retrieve Traffic Classes
 *         information from the device.
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[out]  numTCs       Used to store the Traffic Class
 *                           information.
 *
 * \retval      VMK_OK       If operation succeeds.
 * \retval      VMK_FAILURE  If operation fails.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkDCBNumTCsGetCB)(vmk_AddrCookie driverData,
                                                     vmk_DCBNumTCs *numTCs);


/*
 ***********************************************************************
 * vmk_UplinkDCBPriorityGroupGetCB --                             */ /**
 *
 * \brief  Handler used by vmkernel to retrieve DCB Priority Group
 *         settings from the device.
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[out]  pg           Used to stored the current PG setting.
 *
 * \retval      VMK_OK       If operation succeeds.
 * \retval      VMK_FAILURE  If operation fails.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkDCBPriorityGroupGetCB)(vmk_AddrCookie driverData,
                                                            vmk_DCBPriorityGroup *pg);


/*
 ***********************************************************************
 * vmk_UplinkDCBPriorityGroupSetCB --                             */ /**
 *
 * \brief  Handler used by vmkernel to pushdown DCB Priority Group
 *         settings to the device.
 *
 * \note   It should only be called from the DCB daemon that does
 *         DCB negotiation on behalf of this device.
 *
 * \note   Uplink layer will call vmk_UplinkDCBApplySettingsCB() after
 *         this call to guarantee the changes will be flushed onto the
 *         device.
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[in]   pg           The Priority Group to be set up.
 *
 * \retval      VMK_OK       If operation succeeds.
 * \retval      VMK_FAILURE  If operation fails.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkDCBPriorityGroupSetCB)(vmk_AddrCookie driverData,
                                                            vmk_DCBPriorityGroup *pg);


/*
 ***********************************************************************
 * vmk_UplinkDCBPriorityFCGetCB --                                */ /**
 *
 * \brief  Handler used by vmkernel to retrieve Priority-based Flow
 *         Control configurations from the device.
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[out]  pfcCfg       Used to stored the current PFC configuration.
 *
 * \retval      VMK_OK       If operation succeeds.
 * \retval      VMK_FAILURE  If operation fails.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkDCBPriorityFCGetCB)(vmk_AddrCookie driverData,
                                                         vmk_DCBPriorityFlowControlCfg *pfcCfg);


/*
 ***********************************************************************
 * vmk_UplinkDCBPriorityFCSetCB --                                */ /**
 *
 * \brief  Handler used by vmkernel to pushdown Priority-based Flow
 *         Control configurations to the device.
 *
 * \note   It should only be called from the DCB daemon that does
 *         DCB negotiation on behalf of this device.
 *
 * \note   Uplink layer will call vmk_UplinkDCBApplySettingsCB() after
 *         this call to guarantee the changes will be flushed onto the
 *         device.
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[in]   pfcCfg       The PFC configuration to be set.
 *
 * \retval      VMK_OK       If operation succeeds.
 * \retval      VMK_FAILURE  If operation fails.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkDCBPriorityFCSetCB)(vmk_AddrCookie driverData,
                                                         vmk_DCBPriorityFlowControlCfg *pfcCfg);


/*
 ***********************************************************************
 * vmk_UplinkDCBIsPriorityFCEnabledCB --                          */ /**
 *
 * \brief  Handler used by vmkernel to check whether Priority-based Flow
 *         Control support is enabled on the device.
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[out]  enabled      Used to stored the current PFC support
 *                           state.
 *
 * \retval      VMK_OK       If operation succeeds.
 * \retval      VMK_FAILURE  If operation fails.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkDCBIsPriorityFCEnabledCB)(vmk_AddrCookie driverData,
                                                               vmk_Bool *enabled);


/*
 ***********************************************************************
 * vmk_UplinkDCBPriorityFCEnableCB --                             */ /**
 *
 * \brief  Handler used by vmkernel to enable Priority-based Flow
 *         Control support on the device.
 *
 * \note   It should only be called from the DCB daemon that does
 *         DCB negotiation on behalf of this device.
 *
 * \note   PFC configurations must be setup correctly before enabling
 *         PFC support on the device.
 *
 * \note   Uplink layer will call vmk_UplinkDCBApplySettingsCB() after
 *         this call to guarantee the changes will be flushed onto the
 *         device.
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 *
 * \retval      VMK_OK       If operation succeeds.
 * \retval      VMK_FAILURE  If operation fails.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkDCBPriorityFCEnableCB)(vmk_AddrCookie driverData);


/*
 ***********************************************************************
 * vmk_UplinkDCBPriorityFCDisableCB --                            */ /**
 *
 * \brief  Handler used by vmkernel to disable Priority-based Flow
 *         Control support on the device.
 *
 * \note   It should only be called from the DCB daemon that does
 *         DCB negotiation on behalf of this device.
 *
 * \note   Uplink layer will call vmk_UplinkDCBApplySettingsCB() after
 *         this call to guarantee the changes will be flushed onto the
 *         device.
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 *
 * \retval      VMK_OK       If operation succeeds.
 * \retval      VMK_FAILURE  If operation fails.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkDCBPriorityFCDisableCB)(vmk_AddrCookie driverData);


/*
 ***********************************************************************
 * vmk_UplinkDCBApplicationsGetCB --                              */ /**
 *
 * \brief  Handler used by vmkernel to retrieve all DCB Application
 *         Protocols settings from the device.
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[out]  apps         Used to store the DCB Applications
 *                           settings of the device.
 *
 * \retval      VMK_OK       If operation succeeds.
 * \retval      VMK_FAILURE  If operation fails.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkDCBApplicationsGetCB)(vmk_AddrCookie driverData,
                                                           vmk_DCBApplications *apps);


/*
 ***********************************************************************
 * vmk_UplinkDCBApplicationsSetCB --                              */ /**
 *
 * \brief  Handler used by vmkernel to pushdown DCB Application Protocol
 *         settings to the device.
 *
 * \note   It should only be called from the DCB daemon that does
 *         DCB negotiation on behalf of this device.
 *
 * \note   Uplink layer will call vmk_UplinkDCBApplySettingsCB() after
 *         this call to guarantee the changes will be flushed onto the
 *         device.
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[in]   app          DCB Application Protocol setting of the
 *                           device.
 *
 * \retval      VMK_OK       If operation succeeds.
 * \retval      VMK_FAILURE  If operation fails.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkDCBApplicationsSetCB)(vmk_AddrCookie driverData,
                                                           vmk_DCBApplication *app);


/*
 ***********************************************************************
 * vmk_UplinkDCBCapabilitiesGetCB --                              */ /**
 *
 * \brief  Handler used by vmkernel to retrieve DCB capabilities
 *         information from the device.
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[out]  caps         Used to store the DCB capabilities
 *                           information of the device.
 *
 * \retval      VMK_OK       If operation succeeds.
 * \retval      VMK_FAILURE  If operation fails.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkDCBCapabilitiesGetCB)(vmk_AddrCookie driverData,
                                                           vmk_DCBCapabilities *caps);


/*
 ***********************************************************************
 * vmk_UplinkDCBSettingsApplyCB --                                */ /**
 *
 * \brief  Handler used by vmkernel to flush out all pending DCB
 *         configuration changes on the device.
 *
 * \note   It should only be called from the DCB daemon that does
 *         DCB negotiation on behalf of this device. DCB daemon
 *         calls this routine after all DCB parameters are negotiated
 *         and pushed down to the driver.
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 *
 * \retval      VMK_OK       If operation succeeds.
 * \retval      VMK_FAILURE  If operation fails.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkDCBSettingsApplyCB)(vmk_AddrCookie driverData);


/*
 ***********************************************************************
 * vmk_UplinkDCBSettingsGetCB --                                  */ /**
 *
 * \brief  Handler used by vmkernel to retrieve all DCB settings from
 *         the device.
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[out]  dcb          Used to store the DCB configurations of
 *                           the device.
 *
 * \retval      VMK_OK       If operation succeeds.
 * \retval      VMK_FAILURE  If operation fails.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkDCBSettingsGetCB)(vmk_AddrCookie driverData,
                                                       vmk_DCBConfig *dcb);

/*
 ***********************************************************************
 * vmk_UplinkDCBDcbxModeGetCB --                                  */ /**
 *
 * \brief  Get DCB IEEE Protocol mode from pNic device.
 *
 * \param[in]   driverData   Points to the internal module structure for
 *			     the device associated to the uplink. Before
 *			     calling vmk_DeviceRegister, device driver
 *			     needs to assign this pointer to member
 *			     driverData in structure vmk_UplinkRegData.
 * \param[in]   mode	     The current mode device using.
 *
 * \retval	VMK_OK	     If operation succeeds.
 * \retval	VMK_FAILURE  If operation fails.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkDCBDcbxModeGetCB) (vmk_AddrCookie driverData,
                                                        vmk_DCBSubType *mode);

/*
 ***********************************************************************
 * vmk_UplinkDCBDcbxModeSetCB --                                  */ /**
 *
 * \brief  Set DCB IEEE Protocol mode on pNic device.
 *
 * \param[in]   driverData   Points to the internal module structure for
 *			     the device associated to the uplink. Before
 *			     calling vmk_DeviceRegister, device driver
 *			     needs to assign this pointer to member
 *			     driverData in structure vmk_UplinkRegData.
 * \param[in]   mode	     The mode to set on the device.
 *
 * \retval	VMK_OK	     If operation succeeds.
 * \retval	VMK_FAILURE  If operation fails.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkDCBDcbxModeSetCB) (vmk_AddrCookie driverData,
                                                        vmk_DCBSubType mode);

/*
 ***********************************************************************
 * vmk_UplinkDCBIEEEEtsCfgGetCB --                                */ /**
 *
 * \brief  Get DCB IEEE Protocol ETS settings from pNic device.
 *
 * \note   This callback will only be called from DCB daemon that does
 *         DCB negotiation on behalf of this device.
 *
 * \note   This callback is supported only for DCB version 2.0 or higher
 *
 * \note   Uplink layer will call vmk_UplinkDCBApplySettingsCB() after
 *         this call to guarantee the changes will be flushed onto the
 *         device.
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[in]   etscfg       DCB Protocol ETS setting of the device.
 *
 * \retval      VMK_OK       If operation succeeds.
 * \retval      VMK_FAILURE  If operation fails.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkDCBIEEEEtsCfgGetCB) (vmk_AddrCookie driverData,
                                                          vmk_DCBIEEEEtsCfg *etscfg);

/*
 ***********************************************************************
 * vmk_UplinkDCBIEEEEtsCfgSetCB --                                */ /**
 *
 * \brief  Set DCB IEEE Protocol ETS settings on pNic device.
 *
 * \note   This callback will only be called from DCB daemon that does
 *         DCB negotiation on behalf of this device.
 *
 * \note   This callback is supported only for DCB version 2.0 or higher
 *
 * \note   Uplink layer will call vmk_UplinkDCBApplySettingsCB() after
 *         this call to guarantee the changes will be flushed onto the
 *         device.
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[in]   etscfg       DCB Protocol ETS setting of the device.
 *
 * \retval      VMK_OK       If operation succeeds.
 * \retval      VMK_FAILURE  If operation fails.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkDCBIEEEEtsCfgSetCB) (vmk_AddrCookie driverData,
                                                          vmk_DCBIEEEEtsCfg *etscfg);

/*
 ***********************************************************************
 * vmk_UplinkDCBIEEEPfcCfgGetCB --                                */ /**
 *
 * \brief  Get DCB IEEE Protocol PFC settings from pNic device.
 *
 * \note   This callback will only be called from DCB daemon that does
 *         DCB negotiation on behalf of this device.
 *
 * \note   This callback is supported only for DCB version 2.0 or higher
 *
 * \note   Uplink layer will call vmk_UplinkDCBApplySettingsCB() after
 *         this call to guarantee the changes will be flushed onto the
 *         device.
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[in]   etscfg       DCB Protocol PFC setting of the device.
 *
 * \retval      VMK_OK       If operation succeeds.
 * \retval      VMK_FAILURE  If operation fails.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkDCBIEEEPfcCfgGetCB) (vmk_AddrCookie driverData,
                                                          vmk_DCBIEEEPfcCfg *pfccfg);

/*
 ***********************************************************************
 * vmk_UplinkDCBIEEEPfcCfgSetCB --                                */ /**
 *
 * \brief  Set DCB IEEE Protocol PFC settings on pNic device.
 *
 * \note   This callback will only be called from DCB daemon that does
 *         DCB negotiation on behalf of this device.
 *
 * \note   This callback is supported only for DCB version 2.0 or higher
 *
 * \note   Uplink layer will call vmk_UplinkDCBApplySettingsCB() after
 *         this call to guarantee the changes will be flushed onto the
 *         device.
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[in]   etscfg       DCB Protocol PFC setting of the device.
 *
 * \retval      VMK_OK       If operation succeeds.
 * \retval      VMK_FAILURE  If operation fails.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkDCBIEEEPfcCfgSetCB) (vmk_AddrCookie driverData,
                                                          vmk_DCBIEEEPfcCfg *pfccfg);

/*
 ***********************************************************************
 * vmk_UplinkDCBIEEEAppCfgGetCB --                                */ /**
 *
 * \brief  Get DCB IEEE Application settings on pNic device.
 *
 * \note   This callback will only be called from DCB daemon that does
 *         DCB negotiation on behalf of this device.
 *
 * \note   This callback is supported only for DCB version 2.0 or higher
 *
 * \note   Uplink layer will call vmk_UplinkDCBApplySettingsCB() after
 *         this call to guarantee the changes will be flushed onto the
 *         device.
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[in]   appcfg       DCB Application Protocol setting of the
 *                           device.
 *
 * \retval      VMK_OK       If operation succeeds.
 * \retval      VMK_FAILURE  If operation fails.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkDCBIEEEAppCfgGetCB) (vmk_AddrCookie driverData,
                                                          vmk_DCBApplication *appcfg);

/*
 ***********************************************************************
 * vmk_UplinkDCBIEEEAppCfgSetCB --                                */ /**
 *
 * \brief  Set DCB IEEE Application settings on pNic device.
 *
 * \note   This callback will only be called from DCB daemon that does
 *         DCB negotiation on behalf of this device.
 *
 * \note   This callback is supported only for DCB version 2.0 or higher
 *
 * \note   Uplink layer will call vmk_UplinkDCBApplySettingsCB() after
 *         this call to guarantee the changes will be flushed onto the
 *         device.
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[in]   appcfg       DCB Application Protocol setting of the
 *                           device.
 *
 * \retval      VMK_OK       If operation succeeds.
 * \retval      VMK_FAILURE  If operation fails.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkDCBIEEEAppCfgSetCB) (vmk_AddrCookie driverData,
                                                          vmk_DCBApplication *appcfg);

/*
 ***********************************************************************
 * vmk_UplinkDCBIEEEAppCfgDelCB --                                */ /**
 *
 * \brief  Delete DCB IEEE Application settings from pNic device.
 *
 * \note   This callback will only be called from DCB daemon that does
 *         DCB negotiation on behalf of this device.
 *
 * \note   This callback is supported only for DCB version 2.0 or higher
 *
 * \note   Uplink layer will call vmk_UplinkDCBApplySettingsCB() after
 *         this call to guarantee the changes will be flushed onto the
 *         device.
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[in]   appcfg       DCB Application Protocol setting of the
 *                           device.
 *
 * \retval      VMK_OK       If operation succeeds.
 * \retval      VMK_FAILURE  If operation fails.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkDCBIEEEAppCfgDelCB) (vmk_AddrCookie driverData,
                                                          vmk_DCBApplication *appcfg);


typedef struct vmk_UplinkDCBOps {

   /** Handler used to check whether DCB is enabled on the deivce */
   vmk_UplinkDCBIsEnabledCB           isDCBEnabled;

   /** Handler used to enable DCB support on the device */
   vmk_UplinkDCBEnableCB              enableDCB;

   /** Handler used to disable DCB support on the device */
   vmk_UplinkDCBDisableCB             disableDCB;

   /**
    * Handler used to retrieve Traffic Classes information from the
    * device
    */
   vmk_UplinkDCBNumTCsGetCB           getNumTCs;

   /**
    * Handler used to retrieve Priority Group information from the
    * device
    */
   vmk_UplinkDCBPriorityGroupGetCB    getPG;

   /**
    * Handler used to push down Priority Group settings to the
    * device
    */
   vmk_UplinkDCBPriorityGroupSetCB    setPG;

   /**
    * Handler used to retrieve Priority-based Flow Control
    * configurations from the device
    */
   vmk_UplinkDCBPriorityFCGetCB       getPFCCfg;

   /**
    * Handler used to pushdown Priority-based Flow Control
    * configurations to the device
    */
   vmk_UplinkDCBPriorityFCSetCB       setPFCCfg;

   /**
    * Handler used to check whether Priority-based Flow Control support
    * is enabled on the device
    */
   vmk_UplinkDCBIsPriorityFCEnabledCB isPFCEnabled;

   /**
    * Handler used to enable Priority-based Flow Control on the
    * device
    */
   vmk_UplinkDCBPriorityFCEnableCB    enablePFC;

   /**
    * Handler used to disable Priority-based Flow Control on the
    * device
    */
   vmk_UplinkDCBPriorityFCDisableCB   disablePFC;

   /**
    * Handler used to retrieve all DCB Application Protocols settings
    * from the device
    */
   vmk_UplinkDCBApplicationsGetCB     getApps;

   /**
    * Handler used to pushdown DCB Application Protocol settings to the
    * device
    */
   vmk_UplinkDCBApplicationsSetCB     setApp;

   /**
    * Handler used to retrieve DCB capabilities information from the
    * device
    */
   vmk_UplinkDCBCapabilitiesGetCB     getCaps;

   /**
    * Handler used to flush all pending DCB configuration changes to
    * the device
    */
   vmk_UplinkDCBSettingsApplyCB       applySettings;

   /** Handler used to retrieve all DCB settings from the device */
   vmk_UplinkDCBSettingsGetCB         getSettings;

   /** Handler used to get dcbx mode from the device */
   vmk_UplinkDCBDcbxModeGetCB         getDcbxMode;

   /** Handler used to set dcbx mode on the device */
   vmk_UplinkDCBDcbxModeSetCB         setDcbxMode;

   /** Handler used to get IEEE ETS Configuration fom the device,
    *  supported only for DCB version 2.0 or higher
    */
   vmk_UplinkDCBIEEEEtsCfgGetCB       getIEEEEtsCfg;

   /** Handler used to set IEEE ETS Configuration on the device,
    *  supported only for DCB version 2.0 or higher
    */
   vmk_UplinkDCBIEEEEtsCfgSetCB       setIEEEEtsCfg;

   /** Handler used to get IEEE PFC configuration from the device,
    *  supported only for DCB version 2.0 or higher
    */
   vmk_UplinkDCBIEEEPfcCfgGetCB       getIEEEPfcCfg;

   /** Handler used to set IEEE PFC configuration on the device,
    *  supported only for DCB version 2.0 or higher
    */
   vmk_UplinkDCBIEEEPfcCfgSetCB       setIEEEPfcCfg;

   /** Handler used to get IEEE Application Protocol Configuration
    *  on the device, supported only for DCB version 2.0 or higher
    */
   vmk_UplinkDCBIEEEAppCfgGetCB       getIEEEAppCfg;

   /** Handler used to set IEEE Application Protocol Configuration
    *  on the device, supported only for DCB version 2.0 or higher
    */
   vmk_UplinkDCBIEEEAppCfgSetCB       setIEEEAppCfg;

   /** Handler used to delete IEEE Application Protocol Configuration
    *  from the device, supported only for DCB version 2.0 or higher
    */
   vmk_UplinkDCBIEEEAppCfgDelCB       delIEEEAppCfg;

} vmk_UplinkDCBOps;


#endif /* _VMKAPI_NET_DCB_H_ */
/** @} */
/** @} */
