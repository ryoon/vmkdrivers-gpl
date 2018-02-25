/***************************************************************************
 * Copyright 2004 - 2009 VMware, Inc.  All rights reserved.
 ***************************************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * SCSI Device management types and constants                     */ /**
 * \addtogroup SCSI
 * @{
 *
 * \defgroup SCSImgmt SCSI Device Management Interfaces
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_SCSI_MGMT_TYPES_H_
#define _VMKAPI_SCSI_MGMT_TYPES_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#define VMKDRIVER_DEV_NAME_LENGTH	64
#define VMK_MAX_IP_STRING_LENGTH 	64
#define VMK_MAX_PORT_STRING_LENGTH 	16
#define VMK_MAX_ISCSI_IQN_LENGTH        224
#define VMK_MAX_ISCSI_ISID_STRING_LENGTH      13
#define VMK_MAX_ISCSI_TPGT_STRING_LENGTH      16
#define VMK_MAX_ETHERNET_MAC_LENGTH     6
#define VMK_MAX_ISCSI_PARAM_LENGTH      256
#define VMK_MAX_ISCSI_UID_LENGTH        256

/* Session related Parameters */
#define  VMK_ISCSI_ADAPTER_IQN          0x00000001
#define  VMK_ISCSI_PARAM_DATAPDU_ORDER  0x00000002
#define  VMK_ISCSI_PARAM_DATASEQ_ORDER 	0x00000004
#define  VMK_ISCSI_PARAM_R2T 		0x00000008
#define  VMK_ISCSI_PARAM_IMMDATA 	0x00000010
#define  VMK_ISCSI_PARAM_ERL 		0x00000020
#define  VMK_ISCSI_PARAM_TIME2WAIT 	0x00000040
#define  VMK_ISCSI_PARAM_TIME2RETAIN 	0x00000080
#define  VMK_ISCSI_PARAM_MAXCONNECTIONS	0x00000100
#define  VMK_ISCSI_PARAM_MAXR2T 	0x00000200
#define  VMK_ISCSI_PARAM_FBL 		0x00000400
#define  VMK_ISCSI_PARAM_MBL 		0x00000800
#define  VMK_ISCSI_PARAM_HDRDIGEST 	0x00001000	
#define  VMK_ISCSI_PARAM_DATADIGEST 	0x00002000
#define  VMK_ISCSI_PARAM_RCVDATASEGMENT	0x00004000
#define  VMK_ISCSI_PARAM_ICHAPNAME 	0x00008000
#define  VMK_ISCSI_PARAM_TCHAPNAME 	0x00010000
#define  VMK_ISCSI_PARAM_ICHAPSECRET	0x00020000
#define  VMK_ISCSI_PARAM_TCHAPSECRET 	0x00040000
#define  VMK_ISCSI_PARAM_ISID           0x00080000

/* Target Information */
#define  VMK_ISCSI_IQN_TARGET_NAME 	0x00100000
#define  VMK_ISCSI_TARGET_IP 		0x00200000
#define  VMK_ISCSI_TARGET_PORT 		0x00400000
#define  VMK_ISCSI_TARGET_TPGT          0x00800000

/* Adapter TCP/IP Properties */
#define  VMK_ISCSI_ADAPTER_IP		0x01000000
#define  VMK_ISCSI_ADAPTER_SUBNET	0x02000000
#define  VMK_ISCSI_ADAPTER_GATEWAY	0x04000000
#define  VMK_ISCSI_ADAPTER_PRIMARY_DNS	0x08000000
#define  VMK_ISCSI_ADAPTER_SECONDARY_DNS 0x10000000

/* iscsi_trans max channels, target  mask  0-15: channel, 16-31: target */
#define ISCSI_MAX_CHANNEL_TARGET_MASK      0x0000FFFF

/* max iscsi parm string length */
#define ISCSI_MAX_PARM_STRING_LENGTH     4096

/* iscsi parm types */
typedef enum vmk_IscsiParmType {
   VMK_ISCSI_SESSION_PARM,
   VMK_ISCSI_CONN_PARM,
} vmk_IscsiParmType;

/* FC Port State */
typedef enum vmk_FcPortState {
   VMK_FC_PORTSTATE_UNKNOWN = 0x0,
   VMK_FC_PORTSTATE_NOTPRESENT,
   VMK_FC_PORTSTATE_ONLINE,
   VMK_FC_PORTSTATE_OFFLINE,		
   VMK_FC_PORTSTATE_BLOCKED,
   VMK_FC_PORTSTATE_BYPASSED,
   VMK_FC_PORTSTATE_DIAGNOSTICS,
   VMK_FC_PORTSTATE_LINKDOWN,
   VMK_FC_PORTSTATE_ERROR,
   VMK_FC_PORTSTATE_LOOPBACK,
   VMK_FC_PORTSTATE_DELETED,
} vmk_FcPortState;

/* FC Port Speed */
typedef enum  vmk_FcLinkSpeed {
   VMK_FC_SPEED_UNKNOWN = 0x0,
   VMK_FC_SPEED_1GBIT,
   VMK_FC_SPEED_2GBIT,
   VMK_FC_SPEED_4GBIT,
   VMK_FC_SPEED_8GBIT,
   VMK_FC_SPEED_10GBIT,
   VMK_FC_SPEED_16GBIT,
   VMK_FC_SPEED_32GBIT,
   VMK_FC_SPEED_40GBIT,
} vmk_FcLinkSpeed;

/* Bitwised FC Port Speed */
typedef enum  vmk_FcLinkSpeedBit {
   VMK_FC_SPEED_BIT_UNKNOWN = 0x0,
   VMK_FC_SPEED_BIT_1GBIT = 0x1,
   VMK_FC_SPEED_BIT_2GBIT = 0x2,
   VMK_FC_SPEED_BIT_4GBIT = 0x4,
   VMK_FC_SPEED_BIT_8GBIT = 0x8,
   VMK_FC_SPEED_BIT_10GBIT = 0x10,
   VMK_FC_SPEED_BIT_16GBIT = 0x20,
   VMK_FC_SPEED_BIT_32GBIT = 0x40,
   VMK_FC_SPEED_BIT_40GBIT = 0x80,
} vmk_FcLinkSpeedBit;

/* FC Port TYPE */
typedef enum vmk_FcPortType {
   VMK_FC_PORTTYPE_UNKNOWN = 0x0,
   VMK_FC_PORTTYPE_OTHER,
   VMK_FC_PORTTYPE_NOTPRESENT,
   VMK_FC_PORTTYPE_NPORT,		
   VMK_FC_PORTTYPE_NLPORT,	
   VMK_FC_PORTTYPE_LPORT,
   VMK_FC_PORTTYPE_PTP,
   VMK_FC_PORTTYPE_NPIV,
   VMK_FC_PORTTYPE_VNPORT,
} vmk_FcPortType;

/* Adapter Status */
typedef enum vmk_AdapterStatus  {
   VMK_ADAPTER_STATUS_UNKNOWN = 0x0,
   VMK_ADAPTER_STATUS_ONLINE,
   VMK_ADAPTER_STATUS_OFFLINE,
} vmk_AdapterStatus;

/* Link Rescan Status */
typedef enum vmk_RescanLinkStatus  {
   VMK_RESCAN_LINK_UNSUPPORTED = 0x0,
   VMK_RESCAN_LINK_SUCCEEDED,
   VMK_RESCAN_LINK_FAILED,
} vmk_RescanLinkStatus;

/**
 * \brief FC Adapter Attributes
 * This follows Common HBA API Version 2.18
 */
typedef struct vmk_FcAdapterAttributes {
   /** \brief HBA manufacturer's name in ASCII string */
   vmk_uint8 manufacturer[64];
   /** \brief HBA serial number */
   vmk_uint8 serialNumber[64];
   /** \brief HBA model */
   vmk_uint8 model[256];
   vmk_uint8 modelDescription[256];
   /** \brief WWNN of the HBA */
   vmk_uint64 nodeWWN;
   /** \brief Node symbolic name registered with name server */
   vmk_uint8 nodeSymbolicName[256];
   /** \brief Vendor specifc hardware revision level in ASCII string */
   vmk_uint8 hardwareVersion[256];
   /** \brief Vendor specifc driver version in ASCII string */
   vmk_uint8 driverVersion[256];
   /** \brief Vendor specific option ROM version */
   vmk_uint8 optionROMVersion[256];
   /** \brief Vendor specific firmware version */
   vmk_uint8 firmwareVersion[256];
   /** \brief Vendor specific value */
   vmk_uint32 vendorSpecificID;
   /** \brief Number of ports on this HBA */
   vmk_uint32 numberOfPorts;
   /** \brief Driver file name in full path*/
   vmk_uint8 driverName[256];
} vmk_FcAdapterAttributes;

/**
 * \brief FC Port Attributes
 * This follows Common HBA API Version 2.18
 */
typedef struct vmk_FcPortAttributes {
   /** \brief WWNN associated with this port */
   vmk_uint64 nodeWWN;
   /** \brief WWPN associated with this port */
   vmk_uint64 portWWN;
   /** \brief FcId of the port */
   vmk_uint32 portFcId;
   /** \brief Port Type */
   vmk_FcPortType portType;
   /** \brief Port State */
   vmk_FcPortState portState;
   /** 
    * \brief Supported classes of service of this port.
    * Value defined in FC-GS-5 for class of service.
    */
   vmk_uint32 portSupportedClassOfService;
   /**
    * \brief Supported FC4 types. See HBAAPI spec and
    * FC-GS-5 for detail.
    */
   vmk_uint8  portSupportedFc4Types[32];
   /**
    * \brief FC4 types configured to support. See
    * HBAAPI and FC-GS-5 for detail.
    */
   vmk_uint8  portActiveFc4Types[32];
   /** \brief Port symbolic name registered with name server */
   vmk_uint8  portSymbolicName[256];
   /** \brief OS device name */
   vmk_uint8  osDeviceName[256];
   /** \brief Supported speeds, bitwised */
   vmk_FcLinkSpeedBit portSupportedSpeed;
   /** \brief Speed the port is currrently operating */
   vmk_FcLinkSpeed portSpeed;
   /** \brief Maximum frame size in bytes supported by this port */
   vmk_uint32 portMaxFrameSize;
   /** \brief Fabric name the port is attached */
   vmk_uint64 fabricName;
   /** \brief Number of target ports discovered */
   vmk_uint32 numberOfDiscoveredPorts;
} vmk_FcPortAttributes;

/**
 * \brief FC HBA Port IO Statistics
 * This follows Common HBA API Version 2.18
 */
typedef struct vmk_FcPortStatistics {
   /** \brief Number of seconds since statistics were reset */
   vmk_uint64 secondsSinceLastReset;
   /** \brief Number of total transmitted FC frames */
   vmk_uint64 txFrames;
   /** \brief Numbef of total received FC frames */
   vmk_uint64 rxFrames;
   /** \brief Number of total transmitted FC words */
   vmk_uint64 txWords;
   /** \brief Number of total received FC words */
   vmk_uint64 rxWords;
   /** \brief Number of LIP primitive sequences occured */
   vmk_uint64 lipCount;
   /** \brief Number of NOS primitive sequences occured */
   vmk_uint64 nosCount;
   /** \brief Number of frames received in error */
   vmk_uint64 errorFrames;
   /** \brief Number of frames lost due to lack of host buffers */
   vmk_uint64 dumpedFrames;
   /**
    * \brief Value of LINK FAILURE COUNT field of
    * LINK ERROR STATUS BLOCK
    */
   vmk_uint64 linkFailureCount;
   /**
    * \brief Value of LOSS-OF-SYNCHRONIZATION COUNT field of
    * LINK ERROR STATUS BLOCK
    */
   vmk_uint64 lossOfSyncCount;
   /**
    * \brief Value of LOSS-OF-SIGNAL COUNT field of
    * LINK ERROR STATUS BLOCK
    */
   vmk_uint64 lossOfSignalCount;
   /**
    * \brief Value of PRIMITIVE SEQUENCE PROTOCOL ERROR field of
    * LINK ERROR STATUS BLOCK
    */
   vmk_uint64 primitiveSeqProtocolErrCount;
   /**
    * \brief Value of INVALID TRANSMISSION WORD field of
    * LINK ERROR STATUS BLOCK
    */
   vmk_uint64 invalidTxWordCount;
   /**
    * \brief Value of INVALID CRC COUNT field of
    * LINK ERROR STATUS BLOCK
    */
   vmk_uint64 invalidCrcCount;

   /* fc4 statistics */
   /** \brief Number of FC-4 operations causing FC-4 data input */
   vmk_uint64 inputRequests;
   /** \brief Number of FC-4 operations causing FC-4 data output */
   vmk_uint64 outputRequests;
   /** \brief Number of FC-4 operations not permitting FC-4 data movement */
   vmk_uint64 controlRequests;
   /** \brief Number of megabytes of FC-4 data input */
   vmk_uint64 inputMegabytes;
   /** \brief Number of megabytes of FC-4 data output */
   vmk_uint64 outputMegabytes;
} vmk_FcPortStatistics;

/**
 * \brief FCoE HBA Port IO Statistics
 */
typedef struct vmk_FcoePortStatistics {
   vmk_uint64 secondsSinceLastReset;
   vmk_uint64 txFrames;
   vmk_uint64 rxFrames;
   vmk_uint64 txWords;
   vmk_uint64 rxWords;
   vmk_uint64 errorFrames;
   vmk_uint64 dumpedFrames;
   vmk_uint64 linkFailureCount;
   vmk_uint64 lossOfSignalCount;
   vmk_uint64 invalidTxWordCount;
   vmk_uint64 invalidCrcCount;
   vmk_uint64 vlinkFailureCount;
   vmk_uint64 missDiscAdvCount;

   /* protocol(fc4) statistics */
   vmk_uint64 inputRequests;
   vmk_uint64 outputRequests;
   vmk_uint64 controlRequests;
   vmk_uint64 inputMegabytes;
   vmk_uint64 outputMegabytes;
} vmk_FcoePortStatistics;

/**
 * \brief SAS Port TYPE
 */
typedef enum vmk_SasPortType {
   VMK_SAS_PORTTYPE_UNKNOWN = 1,
   VMK_SAS_PORTTYPE_OTHER,
   VMK_SAS_PORTTYPE_NOTPRESENT,
   VMK_SAS_PORTTYPE_SASDEVICE = 30,
   VMK_SAS_PORTTYPE_SATADEVICE,
   VMK_SAS_PORTTYPE_SASEXPANDER,
} vmk_SasPortType;

/**
 * \brief SAS Port State
 */
typedef enum vmk_SasPortState {
   VMK_SAS_PORTSTATE_UNKNOWN = 1,
   VMK_SAS_PORTSTATE_ONLINE,
   VMK_SAS_PORTSTATE_OFFLINE,
   VMK_SAS_PORTSTATE_BYPASSED,
   VMK_SAS_PORTSTATE_DIAGNOSTICS,
   VMK_SAS_PORTSTATE_LINKDOWN,
   VMK_SAS_PORTSTATE_ERROR,
   VMK_SAS_PORTSTATE_LOOPBACK,
   VMK_SAS_PORTSTATE_DEGRADED,
} vmk_SasPortState;

/**
 * \brief SAS Port Protocol
 */
typedef enum vmk_SasPortProtocol {
   VMK_SAS_PORTPROTOCOL_SSP = 1,
   VMK_SAS_PORTPROTOCOL_STP,
   VMK_SAS_PORTPROTOCOL_SMP,
   VMK_SAS_PORTPROTOCOL_SATA,
} vmk_SasPortProtocol;

/**
 * \brief SAS Port Speed
 */
typedef enum vmk_SasLinkRate {
   VMK_SAS_LINK_RATE_UNKNOWN = 0,
   VMK_SAS_PHY_DISABLED,
   VMK_SAS_LINK_RATE_FAILED,
   VMK_SAS_SATA_SPINUP_HOLD,
   VMK_SAS_SATA_PORT_SELECTOR,
   VMK_SAS_LINK_RATE_1_5_GBPS,
   VMK_SAS_LINK_RATE_3_0_GBPS,
   VMK_SAS_LINK_RATE_6_0_GBPS,
   VMK_SAS_LINK_VIRTUAL,
   VMK_SAS_LINK_RATE_12_0_GBPS,
} vmk_SasLinkRate;

/**
 * \brief SAS Adapter Attributes
 * This follows ANSI SM-HBA Interface
 */
typedef struct vmk_SasAdapterAttributes {
   vmk_uint8 manufacturer[64];
   vmk_uint8 serialNumber[64];
   vmk_uint8 model[256];
   vmk_uint8 modelDescription[256];
   vmk_uint8 hardwareVersion[256];
   vmk_uint8 driverVersion[256];
   vmk_uint8 optionROMVersion[256];
   vmk_uint8 firmwareVersion[256];
   vmk_uint32 vendorSpecificID;
   vmk_uint8 driverName[256];
   vmk_uint8 hbaSymbolicName[256];
   vmk_uint32 numberOfPorts;
} vmk_SasAdapterAttributes;

/**
 * \brief SAS Port Attributes
 * This follows ANSI SM-HBA Interface
 */
typedef struct vmk_SasPortAttributes {
   vmk_SasPortType portType;
   vmk_SasPortState portState;
   vmk_SasPortProtocol portProtocol;
   vmk_uint8 osDeviceName[256];
   vmk_uint64 localSasAddress;
   vmk_uint64 attachedSasAddress;
   vmk_uint32 numberOfDiscoveredPorts;
   vmk_uint32 numberOfPhys;
   vmk_uint64 enclosureID;
   vmk_uint32 bayID;
   vmk_uint8 phyIdentifier;
   vmk_SasLinkRate negotiatedLinkrate;
   vmk_SasLinkRate minimumLinkrate;
   vmk_SasLinkRate maximumLinkrate;
} vmk_SasPortAttributes;

/**
 * \brief SAS HBA Port IO Statistics
 */
typedef struct vmk_SasPortStatistics {
   vmk_uint64 secondsSinceLastReset;
   vmk_uint64 txFrames;
   vmk_uint64 rxFrames;
   vmk_uint64 txWords;
   vmk_uint64 rxWords;
   vmk_uint64 invalidDwordCount;
   vmk_uint64 runningDisparityErrorCount;
   vmk_uint64 lossOfDwordSyncCount;
   vmk_uint64 phyResetProblemCount;

   /* protocol statistics */
   vmk_uint64 inputRequests;
   vmk_uint64 outputRequests;
   vmk_uint64 controlRequests;
   vmk_uint64 inputMegabytes;
   vmk_uint64 outputMegabytes;
} vmk_SasPortStatistics;

/**
 * \brief fc target attributes
 */
typedef struct vmk_FcTargetAttrs {
   /** \brief target node WWN */
   vmk_uint64 nodeName;
   /** \brief target port WWN */
   vmk_uint64 portName;
   /** \brief target port id */
   vmk_uint32 portId;
} vmk_FcTargetAttrs;

/**
 * \brief fc adapter
 *
 * \note "clientData" is from vmk_ScsiAdapter->clientData.
 */
typedef struct vmk_FcAdapter
{
   /** \brief Get FC Node Name */
   vmk_uint64 (*getFcNodeName) (
      void *clientData);
   /** \brief Get FC Port Name */
   vmk_uint64 (*getFcPortName) (
      void *clientData);
   /** \brief Get FC Port Id */
   vmk_uint32 (*getFcPortId) (
      void *clientData);
   /** \brief Get FC Link Speed */
   vmk_FcLinkSpeed (*getFcLinkSpeed) (
      void *clientData);
   /** \brief Get FC Port Type */
   vmk_FcPortType (*getFcPortType) (
      void *clientData);
   /** \brief Get FC Port State */
   vmk_FcPortState (*getFcPortState) (
      void *clientData);
   /** \brief Get FC Target Attributes */
   VMK_ReturnStatus (*getFcTargetAttributes) (
      void *clientData, vmk_FcTargetAttrs *fcAttrib,
      vmk_uint32 channelNo, vmk_uint32 targetNo);
   /** \brief Get FC Adapter Status */
   vmk_AdapterStatus (*getFcAdapterStatus) (
      void *clientData);
   /** \brief rescan FC Link Status */
   vmk_RescanLinkStatus (*rescanFcLink) (
      void *clientData);
   /** \brief Get FC HBA Attributes */
   VMK_ReturnStatus (*getFcAdapterAttributes) (
      void *clientData, vmk_FcAdapterAttributes *adapterAttrib);
   /** \brief Get FC Port Attributes */
   VMK_ReturnStatus (*getFcPortAttributes) (
      void *clientData, vmk_uint32 portId, vmk_FcPortAttributes *portAttrib); 
   /** \brief Get FC Port Statistics */
   VMK_ReturnStatus (*getFcPortStatistics) (
      void *clientData, vmk_FcPortStatistics *portStats);
   /** \brief Issue port reset */
   VMK_ReturnStatus (*issueFcPortReset) (void *clientData, vmk_uint32 portId);

   /**
    * \brief link timeout
    * This is the user provided Link Time out value.
    * If this is not set, default timeout set by
    * the underlying layer(driver layer/driver) is used
    */
   vmk_uint64           linkTimeout;

   /**
    * \brief i/o timeout
    * This is the user provided I/O Time out value.
    * If this is not set, default timeout set by
    * the underlying layer(driver layer/driver) is used
    */
   vmk_uint64           ioTimeout;
   /** \brief reserved */
   vmk_uint32           reserved1[4];
   /** \brief reserved */
   vmk_VA         reserved2[4];
} vmk_FcAdapter;

/**
 * \brief FIP mode used for FCoE discovery
 */
typedef enum vmk_FIPType {
   VMK_FIP_TYPE_UNKNOWN = 0x0,
   VMK_FIP_TYPE_FPMA,
   VMK_FIP_TYPE_SPMA,
} vmk_FIPType;

/**
 * \brief FCoE Adapter attributes
 */
typedef struct vmk_FcoeAdapterAttrs {
   /** \brief FCoE Controller MAC Address */
   vmk_uint8  fcoeContlrMacAddr[VMK_MAX_ETHERNET_MAC_LENGTH];
   /** \brief FCF MAC Address */
   vmk_uint8  fcfMacAddr[VMK_MAX_ETHERNET_MAC_LENGTH];
   /** \brief VN Port MAC Address*/
   vmk_uint8  vnPortMacAddr[VMK_MAX_ETHERNET_MAC_LENGTH];
   /** \brief FIP Type */
   vmk_FIPType fip;
   /** \brief VLAN ID */
   vmk_uint16 vlanId;
   /** \brief Bandwidth Percentage */
   vmk_uint16 bwPercentage;
   /** \brief FCoE MTU */
   vmk_uint32 frameSize;
   /** \brief Underlying vmnic Name in case of CNA */
   char       vmnicName[VMK_DEVICE_NAME_MAX_LENGTH];
} vmk_FcoeAdapterAttrs;

/**
 * \brief fcoe adapter
 *
 * \note "clientData" is from vmk_ScsiAdapter->clientData.
 */
typedef struct vmk_FcoeAdapter
{
   /** \brief Get FCoE adapter Details */
   VMK_ReturnStatus (*getFCoEAdapterAttributes) (
      void *clientData, vmk_FcoeAdapterAttrs *adapterAttrib);
   /** \brief Get FCoE port statstics */
   VMK_ReturnStatus (*getFcoePortStatistics) (
      void *clientData, vmk_FcoePortStatistics *portStats);
   /** \brief Issue port reset */
   VMK_ReturnStatus (*issueFcoePortReset) (void *clientData, vmk_uint32 portId);

   /** \brief Get FC Details */
   vmk_FcAdapter        fc;

   /** \brief reserved */
   vmk_uint32           reserved1[4];
   /** \brief reserved */
   vmk_VA         reserved2[4];
} vmk_FcoeAdapter;

/**
 * \brief SAS target attributes
 */
typedef struct vmk_SasTargetAttrs {
   /** \brief SAS address */
   vmk_uint64 sasAddress;
} vmk_SasTargetAttrs;

/**
 * \brief SAS adapter
 *
 * \note "clientData" is from vmk_ScsiAdapter->clientData.
 */
typedef struct vmk_SasAdapter
{
   /** \brief get the adapter's SAS address */
   vmk_uint64 (*getInitiatorSasAddress) (
      void *clientData);
   /** \brief get the target's attributes */
   VMK_ReturnStatus (*getSasTargetAttributes) (
      void *clientData, vmk_SasTargetAttrs *sasAttrib,
      vmk_uint32 channelNo, vmk_uint32 targetNo);
  /** \brief Get SAS adapter Details */
   VMK_ReturnStatus (*getSasAdapterAttributes) (
      void *clientData, vmk_SasAdapterAttributes *adapterAttrib);
  /** \brief Get SAS Port Attributes */
   VMK_ReturnStatus (*getSasPortAttributes) (
      void *clientData, vmk_uint32 portId, vmk_SasPortAttributes *portAttrib);
   /** \brief Get FCoE port statstics */
   VMK_ReturnStatus (*getSasPortStatistics) (
      void *clientData, vmk_SasPortStatistics *portStats);
   /** \brief Issue port reset */
   VMK_ReturnStatus (*issueSasPortReset) (void *clientData, vmk_uint32 portId);

   /** \brief reserved */
   vmk_uint32           reserved1[4];
   /** \brief reserved */
   vmk_VA         reserved2[4];
} vmk_SasAdapter;

/**
 * \brief Block adapter
 */
typedef struct vmk_BlockAdapter {
   /** \brief device name */
   char         devName[VMKDRIVER_DEV_NAME_LENGTH];
   /** \brief controller instance number  */
   vmk_uint32   controllerInstance;
   /** \brief reserved */
   vmk_uint32   reserved1[4];
   /** \brief reserved */
   vmk_VA reserved2[4];
} vmk_BlockAdapter;

/**
 * \brief Identifier for generic SAN initiator/target
 */
typedef struct vmk_XsanID {
   /** \brief generic SAN ID lower 64bit */
   vmk_uint64 L;
   /** \brief generic SAN ID higher 64bit */
   vmk_uint64 H;
} vmk_XsanID;

/**
 * \brief generic SAN target attributes
 */
typedef struct vmk_XsanTargetAttrs {
   /** \brief generic SAN target ID */
   vmk_XsanID id;
} vmk_XsanTargetAttrs;

/**
 * \brief generic SAN adapter
 *
 * \note "clientData" is from vmk_ScsiAdapter->clientData.
 */
typedef struct vmk_XsanAdapter {
   /** \brief get the generic SAN initiator's ID */
   VMK_ReturnStatus (*getXsanInitiatorID)(void *clientData, vmk_XsanID *xsanID);
   /** \brief get the target's attributes on generic SAN */
   VMK_ReturnStatus (*getXsanTargetAttributes) (
      void *clientData, vmk_XsanTargetAttrs *xsanAttrib,
      vmk_uint32 channelNo, vmk_uint32 targetNo);
} vmk_XsanAdapter;

/* Return types for iSCSI Adapter param settings */
typedef enum vmk_IscsiAdapterParamStatus  {
   VMK_ISCSI_ADAPTER_PARAM_UNSUPPORTED = 0x0,
   VMK_ISCSI_ADAPTER_PARAM_CONFIG_SUCCEEDED,
   VMK_ISCSI_ADAPTER_PARAM_CONFIG_FAILED,
} vmk_IscsiAdapterParamStatus;

/* Return types for iSCSI Session/Connection Param settings */
typedef enum vmk_IscsiParamStatus  {
   VMK_ISCSI_PARAM_CONFIG_UNSUPPORTED = 0x0,
   VMK_ISCSI_PARAM_CONFIG_SUCCEEDED,
   VMK_ISCSI_PARAM_CONFIG_FAILED,
} vmk_IscsiParamStatus;

/**
 * /brief iSCSI Adapter Structure
 *
 * \note "clientData" is from vmk_ScsiAdapter->clientData.
 */
typedef struct vmk_IscsiAdapter
{
   char reserved[5048]; /* not used */ 

   void                 *transport; /* For use by Annex iSCSI */
   vmk_IscsiAdapterParamStatus  (*getAdapterParams)
        (void *clientData, vmk_uint32 param, char *pBuf, vmk_uint32 bufLength);
   /* Configure Iscsi Session/Connection Parameters */
   vmk_IscsiParamStatus  (*getIscsiAttributes)
        (void *clientData, vmk_uint32 sessionId, vmk_uint32 param, char *pBuf, vmk_uint32 bufLength);

   /* Get iSCSI Target Uid */
   VMK_ReturnStatus         (*getIscsiTargetUid)
        (void *pSCSI_Adapter, char *pTargetUid, vmk_uint32 uidLength,
         vmk_uint32 channelNo, vmk_uint32 targetNo);

} vmk_IscsiAdapter;

/**
 * \brief Managed storage adapter types.
 */
typedef enum vmk_StorageTransport {
   VMK_STORAGE_ADAPTER_TRANSPORT_UNKNOWN           = 0x00,
   VMK_STORAGE_ADAPTER_BLOCK                       = 0x01,
   VMK_STORAGE_ADAPTER_FC                          = 0x02,
   VMK_STORAGE_ADAPTER_ISCSI_RESERVED              = 0x03, /* not used */
   VMK_STORAGE_ADAPTER_IDE                         = 0x04,
   VMK_STORAGE_ADAPTER_ISCSI_VENDOR_SUPPLIED_IMA   = 0x05,
   VMK_STORAGE_ADAPTER_SAS                         = 0x06,
   VMK_STORAGE_ADAPTER_SATA                        = 0x07,
   VMK_STORAGE_ADAPTER_USB                         = 0x08,
   VMK_STORAGE_ADAPTER_PSCSI                       = 0x09,
   /**
    * VMK_STORAGE_ADAPTER_XSAN generically represents any type of SAN
    * transport not explicitly encoded elsewhere in this list. It is
    * intended mainly to support novel SAN transports that aren't yet
    * fully integrated and supported in the way other transports are.
    * This type of SAN adapters generally masquerade as
    * locally-attached storage, except that:
    * - They respond to SCSI "Inquiry" commands, yielding unique target IDs.
    * - RDMs are allowed.
    * - Periodic rescanning for new LUNs is performed.
    */
   VMK_STORAGE_ADAPTER_XSAN                        = 0x0a,
   VMK_STORAGE_ADAPTER_FCOE                        = 0x0b,
   VMK_STORAGE_ADAPTER_TRANSPORT_LAST              = 0x0c
} vmk_StorageTransport;

/**
 * \brief Transport specific info required for management.
 */
typedef struct vmk_SCSITransportMgmt {
   /** \brief Storage transport type. */
   vmk_StorageTransport transport;
   /** \brief Reserved */
   vmk_uint8    reserved[4];
   /** \brief Driver transport specific data */
   void         *transportData;
   /** \brief Transport-specifics */
   union {
      /** \brief Block transport specific data. */
      vmk_BlockAdapter *block;
      /** \brief Fibre Channel transport specific data. */
      vmk_FcAdapter    *fc;
      /** \brief ISCSI transport specific data. */
      vmk_IscsiAdapter *iscsi;
      /** \brief SAS transport specific data. */
      vmk_SasAdapter   *sas;
      /** \brief Generic SAN transport specific data. */
      vmk_XsanAdapter  *xsan;
      /** \brief FCoE specific data */
      vmk_FcoeAdapter  *fcoe;
   } t;
} vmk_SCSITransportMgmt;

#endif /* _VMKAPI_SCSI_MGMT_TYPES_H_ */
/** @} */
/** @} */
