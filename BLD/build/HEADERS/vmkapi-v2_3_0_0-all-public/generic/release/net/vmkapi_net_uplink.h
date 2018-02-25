/* **********************************************************
 * Copyright 2006 - 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Uplink                                                         */ /**
 * \addtogroup Network
 *@{
 * \defgroup Uplink Uplink management
 *
 * In VMkernel, uplinks are logical uplink device objects registered by
 * NIC drivers. They provide external connectivity.
 *
 * To register an uplink, device driver prepares uplink registraion data
 * and passes it to vmk_DeviceRegister as registrationData in parameter
 * vmk_DeviceProps.
 *
 * Once uplink device is created and registered, VMKernel calls driver's
 * vmk_UplinkAssociateCB callback, where uplink object handle is handed
 * off to device driver. Device driver should keep a copy of this handle
 * and pass it to subsequent uplink vmkapi calls. Driver must declare
 * all supported capabilities in this callback by calling
 * vmk_UplinkCapRegister.
 *
 * After vmk_DeviceRegister returns, device driver should suppress any
 * TX/RX activities until vmk_UplinkStartIOCB is invoked by VMKernel.
 *
 * vmk_UplinkQuiesceIOCB is invoked when logical uplink device is being
 * quiesced. Driver should flush pending TX/RX packets and arrange for
 * any new TX request to be ignored.
 *
 * vmk_UplinkDisassociateCB is invoked to inform the driver that the
 * uplink handle is invalid. After the callback returns, driver should
 * not use the uplink handle in any VMKAPI calls.
 *
 *@{
 ***********************************************************************
 */
#ifndef _VMKAPI_NET_UPLINK_H_
#define _VMKAPI_NET_UPLINK_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "net/vmkapi_net_poll.h"
#include "net/vmkapi_net_vlan.h"
#include "net/vmkapi_net_dcb.h"
#include "net/vmkapi_net_queue.h"
#include "net/vmkapi_net_vlan.h"


/**
 * \brief Identifier for logical uplink devices.
 */
#define VMK_UPLINK_DEVICE_IDENTIFIER   "com.vmware.uplink"

/**
 * \brief Number of bytes for uplink wake-on-lan strings.
 */
#define VMK_UPLINK_WOL_STRING_MAX       16

/** \brief Uplink event callback handle */
typedef void *vmk_UplinkEventCBHandle;

/** \brief Event identifier for uplink notifications. */
typedef vmk_uint64 vmk_UplinkEvent;

/** \brief Data associated with uplink event */
typedef struct vmk_UplinkEventData vmk_UplinkEventData;

/** Uplink link state is physical up */
#define VMK_UPLINK_EVENT_LINK_UP      0x01

/** Uplink link state is physical down */
#define VMK_UPLINK_EVENT_LINK_DOWN    0x02

/** Uplink has been connected to a portset */
#define VMK_UPLINK_EVENT_CONNECTED    0x04

/** Uplink has been disconnected from a portset */
#define VMK_UPLINK_EVENT_DISCONNECTED 0x08

/** Uplink has been enabled on a portset */
#define VMK_UPLINK_EVENT_ENABLED      0x10

/** Uplink has been disabled on a portset */
#define VMK_UPLINK_EVENT_DISABLED     0x20

/** Uplink has been blocked on a portset */
#define VMK_UPLINK_EVENT_BLOCKED      0x40

/** Uplink bas been unblocked on a portset */
#define VMK_UPLINK_EVENT_UNBLOCKED    0x80


/**
 * \brief Uplink flags for misc. info.
 */
typedef enum vmk_UplinkFlags {
   /** Physical device is hidden from management apps */
   VMK_UPLINK_FLAG_HIDDEN         =       0x01,

   /** Physical device is being registered as pseudo device */
   VMK_UPLINK_FLAG_PSEUDO_REG     =       0x02,
} vmk_UplinkFlags;



/**
 * \brief Uplink state
 */
typedef enum vmk_UplinkState {

   /**
    * Uplink is administratively enabled
    */
   VMK_UPLINK_STATE_ENABLED      = 0x01,

   /**
    * Uplink is administratively disabled
    */
   VMK_UPLINK_STATE_DISABLED     = 0x02,

   /**
    * Uplink is in promiscuous mode
    */
   VMK_UPLINK_STATE_PROMISC      = 0x04,

   /**
    * Uplink can receive broadcast packets
    */
   VMK_UPLINK_STATE_BROADCAST_OK = 0x08,

   /**
    * Uplink can receive multicast packets
    */
   VMK_UPLINK_STATE_MULTICAST_OK = 0x10,
} vmk_UplinkState;


/**
 * \brief Structure advertising a mode (speed/duplex) that is supported by
 *        an uplink.
 */

typedef struct vmk_UplinkSupportedMode {

   /** Supported speed */
   vmk_LinkSpeed  speed;

   /** Supported duplex */
   vmk_LinkDuplex duplex;

} vmk_UplinkSupportedMode;


/**
 * \brief Structure containing the information of the driver controlling
 *        the device associated to an uplink.
 */
typedef struct vmk_UplinkDriverInfo {

   /** String used to store the name of the driver */
   vmk_Name      driver;

   /** String used to store the version of the driver */
   vmk_Name      version;

   /** String used to store the firmware version of the driver */
   vmk_Name      firmwareVersion;

   /** String used to store the name of the module managing this driver */
   vmk_Name      moduleInterface;
} vmk_UplinkDriverInfo;


/**
 * \brief Cable type of a NIC
 */
typedef enum vmk_UplinkCableType {
   /** Other non supported cable type */
   VMK_UPLINK_CABLE_TYPE_OTHER  = 0,

   /** Twisted Pair */
   VMK_UPLINK_CABLE_TYPE_TP     = 0x01,

   /** Attachment Unit Interface */
   VMK_UPLINK_CABLE_TYPE_AUI    = 0x02,

   /** Media Independent Interface */
   VMK_UPLINK_CABLE_TYPE_MII    = 0x04,

   /** Fibre */
   VMK_UPLINK_CABLE_TYPE_FIBRE  = 0x08,

   /** Bayonet Neill-Concelman */
   VMK_UPLINK_CABLE_TYPE_BNC    = 0x10,

   /** Direct Attach Copper */
   VMK_UPLINK_CABLE_TYPE_DA     = 0x20,
} vmk_UplinkCableType;


/*
 ***********************************************************************
 * vmk_UplinkCableTypeGetCB --                                    */ /**
 *
 * \brief Handler used by vmkernel to get an uplink's cable type.
 *
 * \note  This callback may block
 *
 * \param[in]  driverData   Points to the internal module structure for
 *                          the device associated to the uplink. Before
 *                          calling vmk_DeviceRegister, the device
 *                          driver needs to assign this pointer to the
 *                          driverData member of the in
 *                          vmk_UplinkRegData structure.
 * \param[out] cableType    Pointer to be filled in with the cable type
 *                          for this uplink.
 *
 * \retval    VMK_OK        Cable type is returned successfully.
 * \retval    VMK_FAILURE   Otherwise.
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkCableTypeGetCB)(
   vmk_AddrCookie driverData,
   vmk_UplinkCableType *cableType);


/*
 ***********************************************************************
 * vmk_UplinkSupportedCableTypesGetCB --                          */ /**
 *
 * \brief Handler used by vmkernel to get an uplink supported cable
 *        types.
 *
 * \note  This callback may block
 *
 * \param[in]  driverData   Points to the internal module structure for
 *                          the device associated to the uplink. Before
 *                          calling vmk_DeviceRegister, the device
 *                          driver needs to assign this pointer to the
 *                          driverData member of the in
 *                          vmk_UplinkRegData structure.
 * \param[out] cableTypes   Pointer to be filled in with the cable types
 *                          supported by this uplink. It's a bit
 *                          combination of vmk_UplinkCableType.
 *
 * \retval    VMK_OK        Cable types is returned successfully.
 * \retval    VMK_FAILURE   Otherwise.
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkSupportedCableTypesGetCB)(
   vmk_AddrCookie driverData,
   vmk_UplinkCableType *cableTypes);


/*
 ***********************************************************************
 * vmk_UplinkCableTypeSetCB --                                    */ /**
 *
 * \brief Handler used by vmkernel to set an uplink's cable type.
 *
 * \note  This callback may block
 *
 * \param[in]  driverData   Points to the internal module structure for
 *                          the device associated to the uplink. Before
 *                          calling vmk_DeviceRegister, the device
 *                          driver needs to assign this pointer to the
 *                          driverData member of the in
 *                          vmk_UplinkRegData structure.
 * \param[in]  cableType    The cable type to set.
 *
 * \retval    VMK_OK        Cable type is set successfully.
 * \retval    VMK_FAILURE   Otherwise.
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkCableTypeSetCB)(
   vmk_AddrCookie driverData,
   vmk_UplinkCableType cableType);


/**
 * \brief Callbacks to get and set uplink's cable type.
 */
typedef struct vmk_UplinkCableTypeOps {
   /** Callback to get uplink's cable type. */
   vmk_UplinkCableTypeGetCB            getCableType;

   /** Callback to get cable types supported by uplink. */
   vmk_UplinkSupportedCableTypesGetCB  getSupportedCableTypes;

   /** Callback to set uplink's cable type. */
   vmk_UplinkCableTypeSetCB            setCableType;
} vmk_UplinkCableTypeOps;


/*
 ***********************************************************************
 * vmk_UplinkPhyAddressGetCB --                                   */ /**
 *
 * \brief Handler used by vmkernel to get an uplink's PHY address.
 *
 * \note  This callback may block
 *
 * \param[in]  driverData   Points to the internal module structure for
 *                          the device associated to the uplink. Before
 *                          calling vmk_DeviceRegister, the device
 *                          driver needs to assign this pointer to the
 *                          driverData member of the in
 *                          vmk_UplinkRegData structure.
 * \param[out] phyAddr      Pointer to be filled in with the PHY
 *                          address for this uplink.
 *
 * \retval    VMK_OK        PHY address is returned successfully.
 * \retval    VMK_FAILURE   Otherwise.
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkPhyAddressGetCB)(
   vmk_AddrCookie driverData,
   vmk_uint8 *phyAddr);


/*
 ***********************************************************************
 * vmk_UplinkPhyAddressSetCB --                                   */ /**
 *
 * \brief Handler used by vmkernel to set an uplink's PHY address.
 *
 * \note  This callback may block
 *
 * \param[in]  driverData   Points to the internal module structure for
 *                          the device associated to the uplink. Before
 *                          calling vmk_DeviceRegister, the device
 *                          driver needs to assign this pointer to the
 *                          driverData member of the in
 *                          vmk_UplinkRegData structure.
 * \param[in]  phyAddr      The PHY address to set.
 *
 * \retval    VMK_OK        PHY address is set successfully.
 * \retval    VMK_FAILURE   Otherwise.
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkPhyAddressSetCB)(
   vmk_AddrCookie driverData,
   vmk_uint8 phyAddr);

/**
 * \brief Callbacks to get and set uplink's PHY address
 */
typedef struct vmk_UplinkPhyAddressOps {
   /** Callback to get uplink's PHY address. */
   vmk_UplinkPhyAddressGetCB           getPhyAddress;

   /** Callback to set uplink's PHY address. */
   vmk_UplinkPhyAddressSetCB           setPhyAddress;
} vmk_UplinkPhyAddressOps;


/**
 * \brief Tranceiver type of a NIC
 */
typedef enum vmk_UplinkTransceiverType {
   /** Other non supported transceiver type */
   VMK_UPLINK_TRANSCEIVER_TYPE_OTHER      = 0,

   /** Internal transceiver type */
   VMK_UPLINK_TRANSCEIVER_TYPE_INTERNAL   = 1,

   /** External transceiver type */
   VMK_UPLINK_TRANSCEIVER_TYPE_EXTERNAL   = 2,
} vmk_UplinkTransceiverType;


/*
 ***********************************************************************
 * vmk_UplinkTransceiverTypeGetCB --                              */ /**
 *
 * \brief Handler used by vmkernel to get an uplink's transceiver type.
 *
 * \note  This callback may block
 *
 * \param[in]  driverData   Points to the internal module structure for
 *                          the device associated to the uplink. Before
 *                          calling vmk_DeviceRegister, the device
 *                          driver needs to assign this pointer to the
 *                          driverData member of the in
 *                          vmk_UplinkRegData structure.
 * \param[out] xcvrType     Pointer to be filled in with the transceiver
 *                          type for this uplink.
 *
 * \retval    VMK_OK        Transceiver type is returned successfully.
 * \retval    VMK_FAILURE   Otherwise.
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkTransceiverTypeGetCB)(
   vmk_AddrCookie driverData,
   vmk_UplinkTransceiverType *xcvrType);


/*
 ***********************************************************************
 * vmk_UplinkTransceiverTypeSetCB --                              */ /**
 *
 * \brief Handler used by vmkernel to set an uplink's transceiver type.
 *
 * \note  This callback may block
 *
 * \param[in]  driverData   Points to the internal module structure for
 *                          the device associated to the uplink. Before
 *                          calling vmk_DeviceRegister, the device
 *                          driver needs to assign this pointer to the
 *                          driverData member of the in
 *                          vmk_UplinkRegData structure.
 * \param[in]  xcvrType     The transceiver type to set.
 *
 * \retval    VMK_OK        Transceiver type is set successfully.
 * \retval    VMK_FAILURE   Otherwise.
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkTransceiverTypeSetCB)(
   vmk_AddrCookie driverData,
   vmk_UplinkTransceiverType xcvrType);


/**
 * \brief Callbacks to get and set uplink's transceiver type.
 */
typedef struct vmk_UplinkTransceiverTypeOps {
   /** Callback to get uplink's transceiver type. */
   vmk_UplinkTransceiverTypeGetCB      getTransceiverType;

   /** Callback to set uplink's transceiver type. */
   vmk_UplinkTransceiverTypeSetCB      setTransceiverType;
} vmk_UplinkTransceiverTypeOps;


/*
 ***********************************************************************
 * vmk_UplinkMessageLevelGetCB --                                 */ /**
 *
 * \brief Handler used by vmkernel to get an uplink's message level.
 *
 * \note  This callback may block
 *
 * \param[in]  driverData   Points to the internal module structure for
 *                          the device associated to the uplink. Before
 *                          calling vmk_DeviceRegister, the device
 *                          driver needs to assign this pointer to the
 *                          driverData member of the in
 *                          vmk_UplinkRegData structure.
 * \param[out] level        Pointer to be filled in with the messsage
 *                          level for this uplink.
 *
 * \retval    VMK_OK        Message level is returned successfully.
 * \retval    VMK_FAILURE   Otherwise.
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkMessageLevelGetCB)(
   vmk_AddrCookie driverData,
   vmk_uint32 *level);


/*
 ***********************************************************************
 * vmk_UplinkMessageLevelSetCB --                                 */ /**
 *
 * \brief Handler used by vmkernel to set an uplink's message level.
 *
 * \note  This callback may block
 *
 * \param[in]  driverData   Points to the internal module structure for
 *                          the device associated to the uplink. Before
 *                          calling vmk_DeviceRegister, the device
 *                          driver needs to assign this pointer to the
 *                          driverData member of the in
 *                          vmk_UplinkRegData structure.
 * \param[in]  level        The message level to set.
 *
 * \retval    VMK_OK        Message level is set successfully.
 * \retval    VMK_FAILURE   Otherwise.
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkMessageLevelSetCB)(
   vmk_AddrCookie driverData,
   vmk_uint32 level);


/**
 * \brief Callbacks to get and set uplink's message level.
 */
typedef struct vmk_UplinkMessageLevelOps {
   /** Callback to get uplink's message level. */
   vmk_UplinkMessageLevelGetCB         getMessageLevel;

   /** Callback to set uplink's message level. */
   vmk_UplinkMessageLevelSetCB         setMessageLevel;
} vmk_UplinkMessageLevelOps;



/**
 * \brief Structure containing statistics of the device associated to an
 *        uplink
 */
typedef struct vmk_UplinkStats {

   /** The number of rx packets received by the device */
   vmk_uint64 rxPkts;

   /** The number of tx packets sent by the device */
   vmk_uint64 txPkts;

   /** The number of rx bytes by the device */
   vmk_ByteCount rxBytes;

   /** The number of tx bytes by the device */
   vmk_ByteCount txBytes;

   /** The number of rx packets with errors */
   vmk_uint64 rxErrors;

   /** The number of tx packets with errors */
   vmk_uint64 txErrors;

   /** The number of rx packets dropped */
   vmk_uint64 rxDrops;

   /** The number of tx packets dropped */
   vmk_uint64 txDrops;

   /** The number of rx multicast packets */
   vmk_uint64 rxMulticastPkts;

   /** The number of rx broadcast packets */
   vmk_uint64 rxBroadcastPkts;

   /** The number of tx multicast packets */
   vmk_uint64 txMulticastPkts;

   /** The number of tx broadcast packets */
   vmk_uint64 txBroadcastPkts;

   /** The number of collisions */
   vmk_uint64 collisions;

   /** The number of rx length errors */
   vmk_uint64 rxLengthErrors;

   /** The number of rx ring buffer overflow */
   vmk_uint64 rxOverflowErrors;

   /** The number of rx packets with crc errors */
   vmk_uint64 rxCRCErrors;

   /** The number of rx packets with frame alignment error */
   vmk_uint64 rxFrameAlignErrors;

   /** The number of rx fifo overrun */
   vmk_uint64 rxFifoErrors;

   /** The number of rx packets missed */
   vmk_uint64 rxMissErrors;

   /** The number of tx aborted errors */
   vmk_uint64 txAbortedErrors;

   /** The number of tx carriers errors */
   vmk_uint64 txCarrierErrors;

   /** The number of tx fifo errors */
   vmk_uint64 txFifoErrors;

   /** The number of tx heartbeat errors */
   vmk_uint64 txHeartbeatErrors;

   /** The number of tx windows errors */
   vmk_uint64 txWindowErrors;
} vmk_UplinkStats;

/**
 * Uplink offload header alignment
 */
typedef enum vmk_UplinkOffloadHeaderAlignment {
   /** Header starting address requires no alignment */
   VMK_UPLINK_OFFLOAD_HDR_ALIGN_ANY       = 1,

   /** Header starting address must be 2 bytes aligned */
   VMK_UPLINK_OFFLOAD_HDR_ALIGN_2_BYTES   = 2,

   /** Header starting address must be 4 bytes aligned */
   VMK_UPLINK_OFFLOAD_HDR_ALIGN_4_BYTES   = 4,
} vmk_UplinkOffloadHeaderAlignment;


/**
 * Uplink offload constraints
 */
typedef struct vmk_UplinkOffloadConstraints {
   /** L3/L4 header alignment in bytes */
   vmk_UplinkOffloadHeaderAlignment headerAlignment;

   /**
    * Maximum header offset supported by driver
    *    0 = no limit
    *    Otherwise, driver can only support offloading with packet
    *    header offset no larger than maxHeaderOffset.
    */
   vmk_uint32 maxHeaderOffset;
} vmk_UplinkOffloadConstraints;


/**
 * Uplink capabilities
 */
typedef enum vmk_UplinkCap {

   /** Driver supports scatter-gather transmit */
   VMK_UPLINK_CAP_SG_TX            = 1,

   /** Driver supports scatter-gather receive */
   VMK_UPLINK_CAP_SG_RX            = 2,

   /** Driver supports scatter-gather entries spanning multiple pages */
   VMK_UPLINK_CAP_MULTI_PAGE_SG    = 3,

   /** Driver supports IPv4 checksum offload */
   VMK_UPLINK_CAP_IPV4_CSO         = 4,

   /** Driver supports IPv6 checksum offload */
   VMK_UPLINK_CAP_IPV6_CSO         = 5,

   /** Driver supports checksum offload for IPV6 with extension headers */
   VMK_UPLINK_CAP_IPV6_EXT_CSO     = 6,

   /** Driver supports VLAN RX offload (tag stripping) */
   VMK_UPLINK_CAP_VLAN_RX_STRIP    = 7,

   /** Driver supports VLAN TX Offload (tag insertion) */
   VMK_UPLINK_CAP_VLAN_TX_INSERT   = 8,

   /** Driver supports IPv4 TCP segmentation offload (TSO) */
   VMK_UPLINK_CAP_IPV4_TSO         = 9,

   /** Driver supports IPv6 TCP segmentation offload (TSO) */
   VMK_UPLINK_CAP_IPV6_TSO         = 10,

   /** Driver supports TSO for IPV6 with extension headers */
   VMK_UPLINK_CAP_IPV6_EXT_TSO     = 11,

   /** Driver requires to be able to modify packet headers (on TX) */
   VMK_UPLINK_CAP_MOD_TX_HDRS      = 12,

   /** Driver requires no packet scheduling */
   VMK_UPLINK_CAP_NO_SCHEDULER     = 13,

   /** Driver supports accessing private statistics */
   VMK_UPLINK_CAP_PRIV_STATS       = 14,

   /**
    * Driver supports changing link status
    * \note Driver must pass a pointer to vmk_UplinkLinkStatusSetCB
    *       when registering this capability.
    */
   VMK_UPLINK_CAP_LINK_STATUS_SET  = 15,

   /**
    * Driver supports changing the interface MAC address
    * \note Driver must pass a pointer to vmk_UplinkMACAddrSetCB when
    *       registering this capability.
    */
   VMK_UPLINK_CAP_MAC_ADDR_SET     = 16,

   /**
    * Driver supports changing interrupt coalescing parameters
    * \note Driver must pass a pointer to vmk_UplinkCoalesceParamsOps
    *       when registering this capability.
    */
   VMK_UPLINK_CAP_COALESCE_PARAMS  = 17,

   /**
    * Driver supports VLAN filtering
    * \note Driver must pass a pointer to vmk_UplinkVLANFilterOps when
    *       registering this capability.
    */
   VMK_UPLINK_CAP_VLAN_FILTER      = 18,

   /**
    * Driver supports Wake-On-LAN
    * \note Driver must pass a pointer to vmk_UplinkWOLOps when
    *       registering this capability.
    */
   VMK_UPLINK_CAP_WAKE_ON_LAN      = 19,

   /**
    * Driver supports network core dumping
    * \note Driver must pass a pointer to vmk_UplinkNetDumpOps when
    *       registering this capability.
    */
   VMK_UPLINK_CAP_NETWORK_DUMP     = 20,

   /**
    * Driver supports multiple queue
    * \note Driver must pass a pointer to vmk_UplinkQueueOps when
    *       registering this capability.
    */
   VMK_UPLINK_CAP_MULTI_QUEUE      = 21,

   /**
    * Driver supports datacenter bridging (DCB)
    * \note Driver must pass a pointer to vmk_UplinkDCBOps when
    *       registering this capability.
    */
   VMK_UPLINK_CAP_DCB              = 22,

   /** Driver supports UPT */
   VMK_UPLINK_CAP_UPT              = 23,

   /** Driver supports SRIOV */
   VMK_UPLINK_CAP_SRIOV            = 24,

   /**
    * Driver supports encapsulated packet offload (eg. vxlan offload)
    * \note When registering this capability, driver can pass a pointer to
    *       vmk_UplinkEncapOffloadOps if it's interested in VXLAN port
    *       update notification.
    */
   VMK_UPLINK_CAP_ENCAP_OFFLOAD    = 25,

   /**
    * Drvier's TSO/Csum Offloads can be "offset" with constraints
    * \note Driver must pass a pointer to vmk_UplinkOffloadConstraints
    *       when registering this capability.
    */
   VMK_UPLINK_CAP_OFFLOAD_CONSTRAINTS  = 26,

   /**
    * Driver supports EEPROM dump
    * \note Driver must pass a pointer to vmk_UplinkEEPROMOps when
    *       registering this capability.
    */
   VMK_UPLINK_CAP_EEPROM           = 27,

   /**
    * Driver supports register dump
    * \note Driver must pass a pointer to vmk_UplinkRegDumpOps when
    *       registering this capability.
    */
   VMK_UPLINK_CAP_REGDUMP          = 28,

   /**
    * Driver supports self-test
    * \note Driver must pass a pointer to vmk_UplinkSelfTestOps when
    *       registering this capability.
    */
   VMK_UPLINK_CAP_SELF_TEST        = 29,

   /**
    * Driver supports pause frame parameter adjusting
    * \note Driver must pass a pointer to vmk_UplinkPauseParamsOps when
    *       registering this capability.
    */
   VMK_UPLINK_CAP_PAUSE_PARAMS     = 30,

   /**
    * Driver supports restarting negotiation of link speed/duplexity
    * \note Driver must pass a pointer to vmk_UplinkRestartNegotiationCB
    *       when registering this capability.
    */
   VMK_UPLINK_CAP_RESTART_NEG      = 31,

   /**
    * Driver supports hardware large receive offload (LRO)
    * \note Driver must pass a pointer to vmk_UplinkQueueLROConstraints
    *       when registering this capability.
    */
   VMK_UPLINK_CAP_LRO              = 32,

   /**
    * Driver supports getting and setting cable type
    * \note Driver must pass a pointer to vmk_UplinkCableTypeOps when
    *       regisitering this capability.
    */
   VMK_UPLINK_CAP_CABLE_TYPE       = 33,

   /**
    * Driver supports getting and setting PHY address
    * \note Driver must pass a pointer to vmk_UplinkPhyAddressOps when
    *       regisitering this capability.
    */
   VMK_UPLINK_CAP_PHY_ADDRESS      = 34,

   /**
    * Driver supports getting and setting transceiver type
    * \note Driver must pass a pointer to vmk_UplinkTransceiverTypeOps
    *       when regisitering this capability.
    */
   VMK_UPLINK_CAP_TRANSCEIVER_TYPE = 35,

   /**
    * Driver supports getting and setting message level
    * \note Message level is a driver defined variable to control the
    *       verbosity of debug message.
    * \note Driver must pass a pointer to vmk_UplinkMessageLevelOps when
    *       regisitering this capability.
    */
   VMK_UPLINK_CAP_MESSAGE_LEVEL    = 36,

   /**
    * Driver supports getting and setting RX/TX ring parameters
    * \note Driver must pass a pointer to vmk_UplinkRingOps when
    *       registering this capability.
    */
   VMK_UPLINK_CAP_RING_PARAMS      = 37,

   /**
    * Driver supports generic network virtualization encapsulation
    * offload
    * \note Driver must pass a pointer to vmk_UplinkGeneveOffloadParams
    *       when registering this capability.
    */
   VMK_UPLINK_CAP_GENEVE_OFFLOAD   = 38,
} vmk_UplinkCap;


/*
 ***********************************************************************
 * vmk_UplinkTxCB --                                              */ /**
 *
 * \brief Handler used by vmkernel to send packet through the device
 *        associated to an uplink.
 *
 * \note  This callback may not block
 *
 * \param[in] driverData   Points to the internal module structure for
 *                         the device associated to the uplink. Before
 *                         calling vmk_DeviceRegister, device driver
 *                         needs to assign this pointer to member
 *                         driverData in structure vmk_UplinkRegData.
 * \param[in] pktList      The set of packets needed to be sent
 *
 * \retval    VMK_OK       All the packets are being processed
 * \retval    VMK_FAILURE  If the module detects any error during Tx
 *                         process
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkTxCB)(vmk_AddrCookie driverData,
                                           vmk_PktList pktList);


/*
 ***********************************************************************
 * vmk_UplinkMTUSetCB --                                          */ /**
 *
 * \brief  Handler used by vmkernel to set up the mtu of the device
 *         associated with an uplink.
 *
 * \note  This callback may block
 *
 * \param[in] driverData   Points to the internal module structure for
 *                         the device associated to the uplink. Before
 *                         calling vmk_DeviceRegister, device driver
 *                         needs to assign this pointer to member
 *                         driverData in structure vmk_UplinkRegData.
 * \param[in] mtu          The mtu to be set up
 *
 * \retval    VMK_OK       If the mtu setting is effective
 * \retval    VMK_FAILURE  Otherwise
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkMTUSetCB)(vmk_AddrCookie driverData,
                                               vmk_uint32 mtu);


/*
 ***********************************************************************
 * vmk_UplinkStateSetCB --                                        */ /**
 *
 * \brief  Handler used by vmkernel to set uplink state
 *
 * \note  Driver should update field state in its shared data in the
 *        callback handler.
 *
 * \note  This callback may block
 *
 * \param[in] driverData   Points to the internal module structure for
 *                         the device associated to the uplink. Before
 *                         calling vmk_DeviceRegister, device driver
 *                         needs to assign this pointer to member
 *                         driverData in structure vmk_UplinkRegData.
 * \param[in]  state       new uplink state to be set
 *
 * \retval     VMK_OK      set state succeeds
 * \retval     VMK_FAILURE set state fails
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkStateSetCB)(vmk_AddrCookie driverData,
                                                 vmk_UplinkState state);


/*
 ***********************************************************************
 * vmk_UplinkLinkStatusSetCB --                                   */ /**
 *
 *
 * \brief  Handler used by vmkernel to set the link status of a device
 *         associated with an uplink.
 *
 * Besides duplex and link speed, this callback also passes uplink's
 * administrative link status configuration down to NIC driver, denoted
 * by field state in linkInfo. Driver should update field state in its
 * shared data zone accordingly. According to RFC 2863, driver should
 * also bring down NIC's operational link status if administrative
 * link status is set to down.
 *
 * \note  This callback may block
 *
 * \param[in] driverData   Points to the internal module structure for
 *                         the device associated to the uplink. Before
 *                         calling vmk_DeviceRegister, device driver
 *                         needs to assign this pointer to member
 *                         driverData in structure vmk_UplinkRegData.
 * \param[in] linkInfo     Specifies speed and duplex
 *
 * \retval    VMK_OK       If operation was successful
 * \retval    VMK_FAILURE  Otherwise
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkLinkStatusSetCB)(vmk_AddrCookie driverData,
                                                      vmk_LinkStatus *linkInfo);


/*
 ***********************************************************************
 * vmk_UplinkStatsGetCB --                                        */ /**
 *
 * \brief  Handler used by vmkernel to get statistics on a device
 *         associated to an uplink.
 *
 * \note  This callback may block
 *
 * \param[in] driverData   Points to the internal module structure for
 *                         the device associated to the uplink. Before
 *                         calling vmk_DeviceRegister, device driver
 *                         needs to assign this pointer to member
 *                         driverData in structure vmk_UplinkRegData.
 * \param[out] stats       Structure used to store all the requested
 *                         information.
 *
 * \retval    VMK_OK       If the statistics are properly stored
 * \retval    VMK_FAILURE  Otherwise
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkStatsGetCB)(vmk_AddrCookie driverData,
                                                 vmk_UplinkStats *stats);


/*
 ***********************************************************************
 * vmk_UplinkAssociateCB --                                       */ /**
 *
 * \brief Handler used by vmkernal to notify driver that uplink is
 *        associated with device.
 *
 * \note  This callback may block
 * \note  Driver must store the uplink parameter and pass it in
 *        subsequent uplink vmkapi calls, like vmk_UplinkCapRegister.
 * \note  Driver must declare all of its capabilities in this callback.
 *
 * \param[in] driverData   Points to the internal module structure for
 *                         the device associated to the uplink. Before
 *                         calling vmk_DeviceRegister, device driver
 *                         needs to assign this pointer to member
 *                         driverData in structure vmk_UplinkRegData.
 * \param[in] uplink       The uplink object associated with the device.
 *
 * \retval     VMK_OK      capabilities associated successfully
 * \retval     VMK_FAILURE capabilities associated failed
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkAssociateCB)(vmk_AddrCookie driverData,
                                                  vmk_Uplink uplink);

/*
 ***********************************************************************
 * vmk_UplinkDisassociateCB --                                    */ /**
 *
 * \brief Handler used by vmkernal to notify driver that uplink is
 *        disassociated from device.
 *
 * \note  This callback may block
 * \note  Driver doesn't need to unregister its capabilities in this
 *        callback.
 *
 * \param[in] driverData   Points to the internal module structure for
 *                         the device associated to the uplink. Before
 *                         calling vmk_DeviceRegister, device driver
 *                         needs to assign this pointer to member
 *                         driverData in structure vmk_UplinkRegData.
 *
 * \retval     VMK_OK      capabilities disassociated successfully
 * \retval     VMK_FAILURE capabilities disassociated failed
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkDisassociateCB)(vmk_AddrCookie driverData);


/*
 ***********************************************************************
 * vmk_UplinkCapEnableCB --                                       */ /**
 *
 * \brief Handler used by vmkernel to notify driver a capability is
 *        enabled
 *
 * \note  The default behavior for a capability is to start off as
 *        "enabled".
 * \note  This callback may block
 *
 * \param[in] driverData   Points to the internal module structure for
 *                         the device associated to the uplink. Before
 *                         calling vmk_DeviceRegister, device driver
 *                         needs to assign this pointer to member
 *                         driverData in structure vmk_UplinkRegData.
 * \param[in] cap          ID of capability to be enabled
 *
 * \retval    VMK_OK       capability enabled successfully
 * \retval    VMK_FAILURE  capability enabled failed
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkCapEnableCB)(vmk_AddrCookie driverData,
                                                  vmk_UplinkCap cap);


/*
 ***********************************************************************
 * vmk_UplinkCapDisableCB --                                      */ /**
 *
 * \brief Handler used by vmkernel to notify driver a capability is
 *        disabled
 *
 * \note  The default behavior for a capability is to start off as
 *        "enabled".
 *
 * \note  This callback may block
 *
 * \param[in] driverData   Points to the internal module structure for
 *                         the device associated to the uplink. Before
 *                         calling vmk_DeviceRegister, device driver
 *                         needs to assign this pointer to member
 *                         driverData in structure vmk_UplinkRegData.
 * \param[in] cap          ID of capability to be disabled
 *
 * \retval    VMK_OK       capability disabled successfully
 * \retval    VMK_FAILURE  capability disabled failed
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkCapDisableCB)(vmk_AddrCookie driverData,
                                                   vmk_UplinkCap cap);


/*
 ***********************************************************************
 * vmk_UplinkStartIOCB --                                         */ /**
 *
 * \brief Handler used by vmkernel to notify driver uplink is ready to
 *        start IO on the device
 *
 * \note Driver should drop any TX/RX packets prior to this notification
 *
 * \note  This callback may block
 *
 * \param[in] driverData   Points to the internal module structure for
 *                         the device associated to the uplink. Before
 *                         calling vmk_DeviceRegister, device driver
 *                         needs to assign this pointer to member
 *                         driverData in structure vmk_UplinkRegData.
 *
 * \retval     VMK_OK      Always
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkStartIOCB)(vmk_AddrCookie driverData);


/*
 ***********************************************************************
 * vmk_UplinkQuiesceIOCB --                                       */ /**
 *
 * \brief Handler used by vmkernel to notify driver uplink is ready to
 *        quiesce IO on the device
 *
 * \note Driver should flush TX/RX queues upon this notification
 *
 * \note  This callback may block
 *
 * \param[in] driverData   Points to the internal module structure for
 *                         the device associated to the uplink. Before
 *                         calling vmk_DeviceRegister, device driver
 *                         needs to assign this pointer to member
 *                         driverData in structure vmk_UplinkRegData.
 *
 * \retval     VMK_OK      Always
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkQuiesceIOCB)(vmk_AddrCookie driverData);


/*
 ***********************************************************************
 * vmk_UplinkResetCB --                                           */ /**
 *
 * \brief Handler used by vmkernel to notify driver uplink to reset
 *        the tx queues on the device.
 *
 * \note Driver should reset the device upon this notification
 *
 * \note  This callback may block
 *
 * \param[in] driverData   Points to the internal module structure for
 *                         the device associated to the uplink. Before
 *                         calling vmk_DeviceRegister, device driver
 *                         needs to assign this pointer to member
 *                         driverData in structure vmk_UplinkRegData.
 *
 * \retval     VMK_OK      Always
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkResetCB)(vmk_AddrCookie driverData);


/** \brief Basic uplink operations */
typedef struct vmk_UplinkOps {

   /** Transmit packet list callback */
   vmk_UplinkTxCB            uplinkTx;

   /** Set MTU callback */
   vmk_UplinkMTUSetCB        uplinkMTUSet;

   /** Set state callback */
   vmk_UplinkStateSetCB      uplinkStateSet;

   /** Get stats callback */
   vmk_UplinkStatsGetCB      uplinkStatsGet;

   /** Notification of uplink associated with device driver */
   vmk_UplinkAssociateCB     uplinkAssociate;

   /** Notification of uplink disassociated from device driver */
   vmk_UplinkDisassociateCB  uplinkDisassociate;

   /** Capability enable callback */
   vmk_UplinkCapEnableCB     uplinkCapEnable;

   /** Capability disable callback */
   vmk_UplinkCapDisableCB    uplinkCapDisable;

   /** Driver can start IO callback */
   vmk_UplinkStartIOCB       uplinkStartIO;

   /** Driver should quiesce IO callback */
   vmk_UplinkQuiesceIOCB     uplinkQuiesceIO;

   /** Driver reset */
   vmk_UplinkResetCB         uplinkReset;
} vmk_UplinkOps;


/**
 * \brief Capabilities of wake-on-lan (wol)
 */
typedef enum vmk_UplinkWolCaps {
   /** Wake on directed frames */
   VMK_UPLINK_WAKE_ON_PHY         =       0x01,

   /** Wake on unicast frame */
   VMK_UPLINK_WAKE_ON_UCAST       =       0x02,

   /** Wake on multicat frame */
   VMK_UPLINK_WAKE_ON_MCAST       =       0x04,

   /** Wake on broadcast frame */
   VMK_UPLINK_WAKE_ON_BCAST       =       0x08,

   /** Wake on arp */
   VMK_UPLINK_WAKE_ON_ARP         =       0x10,

   /** Wake up magic frame */
   VMK_UPLINK_WAKE_ON_MAGIC       =       0x20,

   /** Wake on magic frame */
   VMK_UPLINK_WAKE_ON_MAGICSECURE =       0x40

} vmk_UplinkWolCaps;


/**
 * \brief Structure describing the wake-on-lan features and state of an
 *        uplink
 */
typedef struct vmk_UplinkWolState {

   /** Uplink supported wake-on-lan features */
   vmk_UplinkWolCaps supported;

   /** Uplink enabled wake-on-lan features */
   vmk_UplinkWolCaps enabled;

   /** Wake-On-LAN secure on password */
   char secureONPassword[VMK_UPLINK_WOL_STRING_MAX];

} vmk_UplinkWolState;


/**
 * \brief Data shared between uplink layer and NIC driver
 *
 * \note This structure is allocated and initialized by driver. It's
 *       readable and writable to driver. And read only to uplink layer.
 *       vmk_VersionedAtomic lock needs to be used for coordinating
 *       access, except accessing volatile fields in structure
 *       vmk_UplinkSharedQueueData.
 *
 * \note For reader, driver can read without the versioned atomic if
 *       reading a single field AND it doesn't care about racing with
 *       its own writer thread. If the driver is reading multiple fields
 *       or it needs to synchronize with the writer thread it can choose
 *       to use the versioned atomic or use another mechanism to
 *       synchronize with writer thread.
 *
 * \note For writer, driver needs to take the versioned atomic to ensure
 *       uplink layer can achieve snapshot consistency. If its possible
 *       for the driver to have multiple threads doing writes, the
 *       driver has to take care of their synchronization since the
 *       versioned atomic lock does NOT serialize multiple writers.
 */
typedef struct vmk_UplinkSharedData {

   /** Lock to ensure snapshot consistency on reads, initialized by driver */
   vmk_VersionedAtomic         lock;

   /** Uplink flags */
   vmk_UplinkFlags             flags;

   /** Uplink state */
   vmk_UplinkState             state;

   /** Uplink operational link status */
   vmk_LinkStatus              link;

   /** Uplink MTU */
   vmk_uint32                  mtu;

   /**
    * Current logical MAC address in use this can be changed via
    * vmk_UplinkMACAddrSetCB()
    */
   vmk_EthAddress              macAddr;

   /** Permanent hardware MAC address */
   vmk_EthAddress              hwMacAddr;

   /** Pointer to supported modes array, list all modes device can support */
   vmk_UplinkSupportedMode    *supportedModes;

   /** Size of supportedModes array in vmk_UplinkSupportedMode */
   vmk_uint32                  supportedModesArraySz;

   /** Driver information */
   vmk_UplinkDriverInfo        driverInfo;

   /** Shared queue information, mandatory for all devices */
   vmk_UplinkSharedQueueInfo  *queueInfo;
} vmk_UplinkSharedData;


/**
 * \brief Uplink registration data
 *
 * \note Before calling vmk_DeviceRegister, device driver needs to allocate
 *       and populate this structure. Then assign its pointer to member
 *       registrationData of structure vmk_DeviceProps, a parameter passed
 *       to vmk_DeviceRegister.
 */
typedef struct vmk_UplinkRegData {

   /**
    * This parameter indicates the vmkapi revision driver compiled with.
    */
   vmk_revnum              apiRevision;

   /** Module ID of device driver */
   vmk_ModuleID            moduleID;

   /**
    * This parameter defines the operation function pointers provided by
    * device driver
    */
   vmk_UplinkOps           ops;

   /**
    * This parameter defines runtime shared data region provided by driver.
    * It's allocated and initialized by driver. It's readable/writable to
    * driver and read only to uplink layer. Access to it needs to be
    * coordinated by its vmk_VersionedAtomic lock member.
    */
   vmk_UplinkSharedData   *sharedData;

   /**
    * This is the private driver context data defined by driver. It will
    * be passed to all callbacks into the driver.
    */
   vmk_AddrCookie          driverData;
} vmk_UplinkRegData;


/*
 ***********************************************************************
 * vmk_UplinkVLANFilterEnableCB --                                */ /**
 *
 * \brief Handler used by vmkernel to enable VLAN filter on device
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 *
 * \retval      VMK_OK       If the operation succeeds
 * \retval      VMK_FAILURE  Otherwise
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkVLANFilterEnableCB)(vmk_AddrCookie driverData);


/*
 ***********************************************************************
 * vmk_UplinkVLANFilterDisableCB --                               */ /**
 *
 * \brief Handler used by vmkernel to disable VLAN filter on device
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 *
 * \retval      VMK_OK       If the operation succeeds
 * \retval      VMK_FAILURE  Otherwise
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkVLANFilterDisableCB)(vmk_AddrCookie driverData);


/*
 ***********************************************************************
 * vmk_UplinkVLANFilterBitmapGetCB --                             */ /**
 *
 * \brief Handler used by vmkernel to get uplink VLAN filter bitmap
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[out]  bitmap       VLAN filter bitmap
 *
 * \retval      VMK_OK       If the operation succeeds
 * \retval      VMK_FAILURE  Otherwise
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkVLANFilterBitmapGetCB)(vmk_AddrCookie driverData,
                                                            vmk_VLANBitmap *bitmap);


/*
 ***********************************************************************
 * vmk_UplinkVLANFilterBitmapSetCB --                             */ /**
 *
 * \brief Handler used by vmkernel to set uplink VLAN filter bitmap
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[in]   bitmap       VLAN bitmap to be set
 *
 * \retval      VMK_OK       if the set VLAN filter bitmap call succeeds
 * \retval      VMK_FAILURE  Otherwise
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkVLANFilterBitmapSetCB)(vmk_AddrCookie driverData,
                                                            vmk_VLANBitmap *bitmap);


/*
 ***********************************************************************
 * vmk_UplinkVLANFilterAddCB --                                   */ /**
 *
 * \brief Handler used by vmkernel to add uplink VLAN filter
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[in]   startID      starting vlan ID
 * \param[in]   endID        ending vlan ID
 *
 * \retval      VMK_OK       if the add VLAN filter call succeeds
 * \retval      VMK_FAILURE  Otherwise
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkVLANFilterAddCB)(vmk_AddrCookie driverData,
                                                      vmk_VlanID startID,
                                                      vmk_VlanID endID);


/*
 ***********************************************************************
 * vmk_UplinkVLANFilterRemoveCB --                                */ /**
 *
 * \brief Handler used by vmkernel to remove uplink VLAN filter
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[in]   startID      Starting VLAN ID of VLAN range
 * \param[in]   endID        ending VLAN ID of VLAN range
 *
 * \retval      VMK_OK       if the remove VLAN filter call succeeds
 * \retval      VMK_FAILURE  Otherwise
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkVLANFilterRemoveCB)(vmk_AddrCookie driverData,
                                                         vmk_VlanID startID,
                                                         vmk_VlanID endID);


/**
 * \brief VLAN filter operations
 */
typedef struct vmk_UplinkVLANFilterOps {

   /** Handler to enable VLAN Filters */
   vmk_UplinkVLANFilterEnableCB     enableVLANFilter;

   /** Handler to disable VLAN Filters */
   vmk_UplinkVLANFilterDisableCB    disableVLANFilter;

   /** Handler to get VLAN Filters bitmap */
   vmk_UplinkVLANFilterBitmapGetCB  getVLANFilterBitmap;

   /** Handler to set VLAN Filters bitmap */
   vmk_UplinkVLANFilterBitmapSetCB  setVLANFilterBitmap;

   /** Handler to add VLAN Filter */
   vmk_UplinkVLANFilterAddCB        addVLANFilter;

   /** Handler to remove VLAN Filter */
   vmk_UplinkVLANFilterRemoveCB     removeVLANFilter;
} vmk_UplinkVLANFilterOps;


/*
 ***********************************************************************
 * vmk_UplinkWOLStateGetCB --                                     */ /**
 *
 * \brief  Handler used by vmkernel to get wake-on-lan state of
 *         device associated with an uplink.
 *
 * \param[in]  driverData   Points to the internal module structure for
 *                          the device associated to the uplink. Before
 *                          calling vmk_DeviceRegister, device driver
 *                          needs to assign this pointer to member
 *                          driverData in structure vmk_UplinkRegData.
 * \param[out] wolState     Structure used to store all the requested
 *                          information.
 *
 * \retval     VMK_OK       If the driver information is properly stored
 * \retval     VMK_FAILURE  Otherwise
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkWOLStateGetCB)(vmk_AddrCookie data,
                                                    vmk_UplinkWolState *wolState);


/*
 ***********************************************************************
 * vmk_UplinkWOLStateSetCB --                                     */ /**
 *
 * \brief  Handler used by vmkernel to set wake-on-lan state of
 *         device associated with an uplink.
 *
 * \param[in]  driverData   Points to the internal module structure for
 *                          the device associated to the uplink. Before
 *                          calling vmk_DeviceRegister, device driver
 *                          needs to assign this pointer to member
 *                          driverData in structure vmk_UplinkRegData.
 * \param[out] wolState     Structure used to store all the requested
 *                          information.
 *
 * \retval     VMK_OK       If the driver information is properly stored
 * \retval     VMK_FAILURE  Otherwise
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkWOLStateSetCB)(vmk_AddrCookie driverData,
                                                    vmk_UplinkWolState *wolState);


/**
 * \brief Wake-On-LAN operations
 */
typedef struct vmk_UplinkWOLOps {

   /** Handler to get uplink WOL state */
   vmk_UplinkWOLStateGetCB    getWOLState;

   /** Handler to set uplink WOL state */
   vmk_UplinkWOLStateSetCB    setWOLState;
} vmk_UplinkWOLOps;


/*
 ***********************************************************************
 * vmk_UplinkEEPROMLenGetCB --                                    */ /**
 *
 * \brief Handler used by vmkernel to get uplink device's EEPROM length
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[out]  len          length of EEPROM in bytes returned by
 *                           driver
 *
 * \retval      VMK_OK       if the get EEPROM length call succeeds
 * \retval      VMK_FAILURE  Otherwise
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkEEPROMLenGetCB)(vmk_AddrCookie driverData,
                                                     vmk_int32 *len);


/*
 ***********************************************************************
 * vmk_UplinkEEPROMDumpCB --                                      */ /**
 *
 * \brief Handler used by vmkernel to dump uplink device's EEPROM into
 *        a vmkernel allocated buffer
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[out]  buf          caller allocated buffer to store
 *                           content from EEPROM
 * \param[in]   bufLen       length of buf in bytes
 * \param[in]   offset       the offset in EEPROM where dump starts
 * \param[out]  outLen       the length actually read
 *
 * \retval      VMK_OK       if the dump EERPOM call succeeds
 * \retval      VMK_FAILURE  Otherwise
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkEEPROMDumpCB)(vmk_AddrCookie driverData,
                                                   vmk_AddrCookie buf,
                                                   vmk_uint32 bufLen,
                                                   vmk_uint32 offset,
                                                   vmk_uint32 *outLen);


/*
 ***********************************************************************
 * vmk_UplinkEEPROMSetCB --                                       */ /**
 *
 * \brief Handler used by vmkernel to write uplink device's EEPROM
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[in]   magic        the magic word device accepts
 * \param[in]   buf          content to write
 * \param[in]   bufLen       content length in bytes
 * \param[in]   offset       the offset in EEPROM where write starts
 *
 * \retval      VMK_OK       if the set EEPROM call succeeds
 * \retval      VMK_FAILURE  Otherwise
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkEEPROMSetCB)(vmk_AddrCookie driverData,
                                                  vmk_uint32 magic,
                                                  vmk_AddrCookie buf,
                                                  vmk_uint32 bufLen,
                                                  vmk_uint32 offset);


/**
 * \brief Device EEPROM related operations
 */
typedef struct vmk_UplinkEEPROMOps {

   /** Handler to get the length of EEPROM */
   vmk_UplinkEEPROMLenGetCB      eepromLenGet;

   /** Handler to dump EEPROM content */
   vmk_UplinkEEPROMDumpCB        eepromDump;

   /** Handler to write EEPROM */
   vmk_UplinkEEPROMSetCB         eepromSet;
} vmk_UplinkEEPROMOps;


/*
 ***********************************************************************
 * vmk_UplinkRegDumpLenGetCB --                                   */ /**
 *
 * \brief Handler used by vmkernel to get device registers dump length
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[out]  len          length of registers in bytes returned by
 *                           driver
 *
 * \retval      VMK_OK       if the get register dump length call succeeds
 * \retval      VMK_FAILURE  Otherwise
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkRegDumpLenGetCB)(vmk_AddrCookie driverData,
                                                      vmk_uint32 *len);


/*
 ***********************************************************************
 * vmk_UplinkRegDumpCB --                                         */ /**
 *
 * \brief Handler used by vmkernel to dump device registers
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[out]  buf          caller allocated buffer to store registers,
 *                           buffer length is returned by handler
 *                           vmk_UplinkRegDumpLenGetCB
 *
 * \retval      VMK_OK       if the dump register call succeeds
 * \retval      VMK_FAILURE  Otherwise
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkRegDumpCB)(vmk_AddrCookie driverData,
                                                vmk_AddrCookie buf);


/**
 * \brief Device registers dump related operations
 */
typedef struct vmk_UplinkRegDumpOps {
   /** Handler to get registers dump length */
   vmk_UplinkRegDumpLenGetCB     regDumpLenGet;

   /** Handler to dump registers */
   vmk_UplinkRegDumpCB           regDump;
} vmk_UplinkRegDumpOps;


/*
 ***********************************************************************
 * vmk_UplinkSelfTestResultLenGetCB --                            */ /**
 *
 * \brief Handler used by vmkernel to get self test result length
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[out]  len          length of self test result in
 *                           vmk_UplinkSelfTestResult or
 *                           vmk_UplinkSelfTestString
 *
 * \retval      VMK_OK       if the get self test result length call
 *                           succeeds
 * \retval      VMK_FAILURE  Otherwise
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkSelfTestResultLenGetCB)(vmk_AddrCookie driverData,
                                                             vmk_uint32 *len);


/**
 * Uplink self test result
 */
typedef vmk_uint64 vmk_UplinkSelfTestResult;


/**
 * Uplink self test string
 */
typedef char vmk_UplinkSelfTestString[32];


/*
 ***********************************************************************
 * vmk_UplinkSelfTestRunCB --                                     */ /**
 *
 * \brief Callback handler to run self run on device and return result
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[in]   online       if TRUE, perform online tests only,
 *                           otherwise both online and offline tests.
 * \param[out]  passed       self test is passed or not
 * \param[out]  resultBuf    caller allocated buffer to store self test
 *                           result, size is returned by
 *                           vmk_UplinkSelfTestResultLenGetCB
 * \param[out]  stringBuf    caller allocated buffer to store self test
 *                           string, size is returned by
 *                           vmk_UplinkSelfTestResultLenGetCB
 *
 * \retval      VMK_OK       if the run self test call succeeds
 * \retval      VMK_FAILURE  Otherwise
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkSelfTestRunCB)(vmk_AddrCookie driverData,
                                                    vmk_Bool online,
                                                    vmk_Bool *passed,
                                                    vmk_UplinkSelfTestResult *resultBuf,
                                                    vmk_UplinkSelfTestString *stringsBuf);


/**
 * \brief Device self test related operations
 */
typedef struct vmk_UplinkSelfTestOps {

   /** Handler to get self test result length */
   vmk_UplinkSelfTestResultLenGetCB    selfTestResultLenGet;

   /** Handler to perform self test */
   vmk_UplinkSelfTestRunCB             selfTestRun;
} vmk_UplinkSelfTestOps;


/**
 * \brief Device RX/TX ring parameters
 */
typedef struct vmk_UplinkRingParams {
   /**
    * Maximum number of RX ring entries
    *
    * \note Drivers should ignore this field in
    *       vmk_UplinkRingParamsSetCB().
    */
   vmk_uint32  rxMaxPending;

   /**
    * Maximum number of RX mini ring entries
    *
    * \note Drivers should ignore this field in
    *        vmk_UplinkRingParamsSetCB().
    */
   vmk_uint32  rxMiniMaxPending;

   /**
    * Maximum number of RX jumbo ring entries
    *
    * \note Drivers should ignore this field in
    *        vmk_UplinkRingParamsSetCB().
    */
   vmk_uint32  rxJumboMaxPending;

   /**
    * Maximum number of TX ring entries
    *
    * \note Drivers should ignore this field in
    *        vmk_UplinkRingParamsSetCB().
    */
   vmk_uint32  txMaxPending;

   /** Number of RX ring entries currently configured */
   vmk_uint32  rxPending;

   /** Number of RX mini ring entries currently configured */
   vmk_uint32  rxMiniPending;

   /** Number of RX jumbo ring entries currently configured */
   vmk_uint32  rxJumboPending;

   /** Number of TX ring entries currently configured */
   vmk_uint32  txPending;
} vmk_UplinkRingParams;


/*
 ***********************************************************************
 * vmk_UplinkRingParamsGetCB --                                   */ /**
 *
 * \brief Handler used by vmkernel to get RX/TX ring size params
 *
 * \param[in]  driverData   Points to the internal module structure for
 *                          the device associated to the uplink. Before
 *                          calling vmk_DeviceRegister, the device
 *                          driver needs to assign this pointer to the
 *                          driverData member of the in
 *                          vmk_UplinkRegData structure.
 * \param[out]  params      RX/TX ring size parameters returned
 *
 * \retval      VMK_OK       if the get ring parameters call succeeds
 * \retval      VMK_FAILURE  Otherwise
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkRingParamsGetCB)(vmk_AddrCookie driverData,
                                                      vmk_UplinkRingParams *params);


/*
 ***********************************************************************
 * vmk_UplinkRingParamsSetCB --                                   */ /**
 *
 * \brief Handler used by vmkernel to set RX/TX ring size params
 *
 * \param[in]  driverData   Points to the internal module structure for
 *                          the device associated to the uplink. Before
 *                          calling vmk_DeviceRegister, the device
 *                          driver needs to assign this pointer to the
 *                          driverData member of the in
 *                          vmk_UplinkRegData structure.
 * \param[in]   params      RX/TX ring size parameters to set
 *
 * \retval      VMK_OK       if the set ring size parameters call
 *                           succeeds
 * \retval      VMK_FAILURE  Otherwise
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkRingParamsSetCB)(vmk_AddrCookie driverData,
                                                      vmk_UplinkRingParams *params);


/**
 * \brief Device RX/TX ring parameters related operations
 */
typedef struct vmk_UplinkRingParamsOps {

   /** Handler to get RX/TX ring size params */
   vmk_UplinkRingParamsGetCB       ringParamsGet;

   /** Handler to set RX/TX ring size params */
   vmk_UplinkRingParamsSetCB       ringParamsSet;
} vmk_UplinkRingParamsOps;


/**
 * \brief Flow control ability according to 802.3 standard
 */
typedef enum vmk_UplinkFlowCtrlAbility {
   /** PAUSE bit, device supports symmetric PAUSE ability when set */
   VMK_UPLINK_FLOW_CTRL_PAUSE       = (1 << 0),

   /** ASM_DIR bit, device supports asymmetric PAUSE ability when set */
   VMK_UPLINK_FLOW_CTRL_ASYM_PAUSE  = (1 << 1),
} vmk_UplinkFlowCtrlAbility;


/**
 * \brief Device pause paramters
 */
typedef struct vmk_UplinkPauseParams {

   /** Link is being auto-negotiated or not */
   vmk_Bool autoNegotiate;

   /**
    * when autoNegotiate is zero, force driver to use/not-use pause
    * RX flow control
    */
   vmk_Bool rxPauseEnabled;

   /**
    * when autoNegotiate is zero, force driver to use/not-use pause
    * TX flow control
    */
   vmk_Bool txPauseEnabled;

   /** The flow control abilities NIC is advertising */
   vmk_UplinkFlowCtrlAbility localDeviceAdvertise;

   /** The flow control abilities link partner is advertising */
   vmk_UplinkFlowCtrlAbility linkPartnerAdvertise;
} vmk_UplinkPauseParams;


/*
 ***********************************************************************
 * vmk_UplinkPauseParamsGetCB --                                  */ /**
 *
 * \brief Handler used by vmkernel to get pause parameters
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[out]  params       pause parameters returned
 *
 * \retval      VMK_OK       if the get pause parameters call succeeds
 * \retval      VMK_FAILURE  Otherwise
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkPauseParamsGetCB)(vmk_AddrCookie driverData,
                                                       vmk_UplinkPauseParams *params);


/*
 ***********************************************************************
 * vmk_UplinkPauseParamsSetCB --                                  */ /**
 *
 * \brief Handler used by vmkernel to set pause parameters
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[in]   params       Pause parameters to set. Device drivers
 *                           should ignore field localDeviceAdvertise
 *                           and linkPartnerAdvertise.
 *
 * \retval      VMK_OK       if the set pause parameters call succeeds
 * \retval      VMK_FAILURE  Otherwise
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkPauseParamsSetCB)(vmk_AddrCookie driverData,
                                                       vmk_UplinkPauseParams params);


/**
 * \brief Device pause parameters related operations
 */
typedef struct vmk_UplinkPauseParamsOps {

   /** Handler to get pause parameters */
   vmk_UplinkPauseParamsGetCB       pauseParamsGet;

   /** Handler to set pause parameters */
   vmk_UplinkPauseParamsSetCB       pauseParamsSet;
} vmk_UplinkPauseParamsOps;


/*
 ***********************************************************************
 * vmk_UplinkCapRegister --                                       */ /**
 *
 * \brief Register a capability with the networking stack
 *
 * \note  Capabilities like IPv4 TSO, IPv6 TSO and IPv6 CSO can be
 *        disabled globally by administrator. If the capability to be
 *        registered is disabled, a VMK_IS_DISABLED will be returned to
 *        indicate that uplink layer will simulate the capability in
 *        software.
 *
 * \param[in]  uplink        Pointer to upink device.
 * \param[in]  cap           Capability to register.
 * \param[in]  capOps        Operation function table of this
 *                           capability. It should be NULL if function
 *                           table is not required for this capability.
 *
 * \retval     VMK_OK            Capability registration succeeded.
 *             VMK_IS_DISABLED   The capalbility is disabled globally.
 *             Other status      Capability registration failed.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_UplinkCapRegister(vmk_Uplink uplink,
                                       vmk_UplinkCap cap,
                                       vmk_AddrCookie capOps);


/*
 ***********************************************************************
 * vmk_UplinkPrivStatsGetCB --                                    */ /**
 *
 * \brief Handler used by vmkernel to get uplink private statistics
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[out]  statBuf      Buffer to keep device private stats
 * \param[in]   length       Length of statBuf in bytes
 *
 * \retval      VMK_OK       if the get private stats call succeeds
 * \retval      VMK_FAILURE  if the uplink doesn't support private stats
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkPrivStatsGetCB)(vmk_AddrCookie driverData,
                                                     char *statBuf,
                                                     vmk_ByteCount length);


/*
 ***********************************************************************
 * vmk_UplinkPrivStatsLengthGetCB --                              */ /**
 *
 * \brief Handler used by vmkernel to get uplink private statistics
 *        length
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[out]  length       Length of private stats in bytes
 *
 * \retval      VMK_OK       if the get private stats call succeeds
 * \retval      VMK_FAILURE  if the uplink doesn't support private stats
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkPrivStatsLengthGetCB)(vmk_AddrCookie driverData,
                                                           vmk_ByteCount *length);

/**
 * Uplink private stats operations
 */
typedef struct vmk_UplinkPrivStatsOps {
   /** Handler used by vmkernel to get driver's private stats length */
   vmk_UplinkPrivStatsLengthGetCB   privStatsLengthGet;

   /** Handler used by vmkernel to get driver's private stats */
   vmk_UplinkPrivStatsGetCB         privStatsGet;
} vmk_UplinkPrivStatsOps;


/*
 ***********************************************************************
 * vmk_UplinkVXLANPortUpdateCB --                                 */ /**
 *
 * \brief Handler used by vmkernel to notify VXLAN port number updated
 *
 * \note This function will not block
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[in]   portNBO      VXLAN port number in network byte order
 *
 * \retval      VMK_OK       If the notification is handled successfully
 * \retval      VMK_FAILURE  Otherwise
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkVXLANPortUpdateCB)(vmk_AddrCookie driverData,
                                                        vmk_uint16 portNBO);


/**
 * Uplink encapsulation offload operations
 */
typedef struct vmk_UplinkEncapOffloadOps {
   /** Handler used by vmkernel to notify VXLAN port number updated */
   vmk_UplinkVXLANPortUpdateCB   vxlanPortUpdate;
} vmk_UplinkEncapOffloadOps;


/*
 ***********************************************************************
 * vmk_UplinkGenevePortUpdateCB --                                */ /**
 *
 * \brief Handler used by vmkernel to notify Geneve port number updated
 *
 * The UDP port number used by Geneve encapsulation can be modified by
 * other components. NIC drivers can register a callback to receive this
 * event and convey the new value to their hardware offloading engine.
 *
 * \note This function must not block
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[in]   portNBO      Geneve port number in network byte order.
 *
 * \retval      VMK_OK       If the notification is handled successfully
 * \retval      VMK_FAILURE  Otherwise
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkGenevePortUpdateCB)(vmk_AddrCookie driverData,
                                                         vmk_uint16 portNBO);


/** Geneve offload support flags */
typedef enum vmk_UplinkGeneveOffloadFlags {
   /** The NIC supports outer UDP checksum offload */
   VMK_UPLINK_GENEVE_FLAG_OUTER_UDP_CSO   = (1 << 0),

   /**
    * The NIC supports dedicated RX queue for operations, administration
    * and management packets.
    */
   VMK_UPLINK_GENEVE_FLAG_OAM_RX_QUEUE    = (1 << 1),
} vmk_UplinkGeneveOffloadFlags;


/** Geneve offload operations, limitation and flags */
typedef struct vmk_UplinkGeneveOffloadParams {
   /** Handler used by vmkernel to notify Geneve port number updated */
   vmk_UplinkGenevePortUpdateCB  portUpdate;

   /**
    * Maximum inner L7 header offset supported by driver
    *    0 = no limit
    *    Otherwise, driver can only support offloading with packet
    *    inner L7 header offset no larger than maxHeaderOffset.
    */
   vmk_uint32  maxHeaderOffset;

   /** Geneve offload flags */
   vmk_UplinkGeneveOffloadFlags  flags;
} vmk_UplinkGeneveOffloadParams;


/*
 ***********************************************************************
 * vmk_UplinkMACAddrSetCB --                                      */ /**
 *
 * \brief Handler used by vmkernel to set uplink MAC address
 *
 * \note This callback is desgined for future use, and not invoked by
 *       current uplink layer implementation.
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[in]   macAddr      mac address to be set
 *
 * \retval      VMK_OK       if the get private stats length call succeeds
 * \retval      VMK_FAILURE  if the uplink doesn't support private stats
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkMACAddrSetCB)(vmk_AddrCookie driverData,
                                                   vmk_EthAddress macAddr);


/*
 ***********************************************************************
 * vmk_UplinkRestartNegotiationCB --                              */ /**
 *
 * \brief Handler used by vmkernel to restart negotiation on uplink
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 *
 * \retval      VMK_OK       if the restart negotitation call succeeds
 * \retval      VMK_FAILURE  if the uplink doesn't support restart
 *                           negotiation
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkRestartNegotiationCB)(vmk_AddrCookie driverData);


/*
 ***********************************************************************
 * vmk_UplinkVXLANPortNBOGet --                                   */ /**
 *
 * \brief  Get current VXLAN port number
 *
 * \return      VXLAN port number in network byte order
 *
 ***********************************************************************
 */
vmk_uint16 vmk_UplinkVXLANPortNBOGet(void);

/*
 ***********************************************************************
 * vmk_UplinkTx --                                                */ /**
 *
 * \brief  Transmit a list of packet to network.
 *
 * \param[in]   uplink            Uplink handle.
 * \param[in]   pktList           List of packets to be transmitted.
 *
 * \retval      VMK_OK              Packet transmission succeed.
 * \retval      VMK_LIMIT_EXCEEDED  The layer 2 payload of one or more
 *                                  non TSO packets in pktList exceeds
 *                                  uplink's MTU.
 * \retval      Other Value         Failed to send pktList out.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_UplinkTx(vmk_Uplink uplink, vmk_PktList pktList);


/*
 ***********************************************************************
 * vmk_UplinkFlagsGet --                                          */ /**
 *
 * \brief  Get uplink flags
 *
 * \param[in]   uplink            Uplink handle
 *
 * \return      Uplink flags
 *
 ***********************************************************************
 */
vmk_UplinkFlags vmk_UplinkFlagsGet(vmk_Uplink uplink);


/*
 ***********************************************************************
 * vmk_UplinkStateGet --                                          */ /**
 *
 * \brief  Get uplink state
 *
 * \param[in]   uplink            Uplink handle
 *
 * \return      Uplink state
 *
 ***********************************************************************
 */
vmk_UplinkState vmk_UplinkStateGet(vmk_Uplink uplink);


/*
 ***********************************************************************
 * vmk_UplinkStateSet --                                          */ /**
 *
 * \brief  Set uplink state
 *
 * \param[in]   uplink            Uplink handle
 * \param[in]   state             State to set
 *
 * \retval      VMK_OK            State is set successfully
 * \retval      Other Value       Setting stste failed
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_UplinkStateSet(vmk_Uplink uplink,
                                    vmk_UplinkState state);


/*
 ***********************************************************************
 * vmk_UplinkLinkStatusGet --                                     */ /**
 *
 * \brief  Return uplink link properties
 *
 * \note  The link state returned is VMK_LINK_STATE_UP only when both
 *        uplink's operational and administrative link status are up.
 *        Otherwise, it will be VMK_LINK_STATE_DOWN.
 *
 * \param[in]   uplink            Uplink handle
 *
 * \return                        Uplink link status
 *
 ***********************************************************************
 */
vmk_LinkStatus vmk_UplinkLinkStatusGet(vmk_Uplink uplink);


/*
 ***********************************************************************
 * vmk_UplinkMTUGet --                                            */ /**
 *
 * \brief  Get uplink MTU
 *
 * \param[in]   uplink            Uplink handle
 *
 * \return      Uplink MTU
 *
 ***********************************************************************
 */
vmk_uint32 vmk_UplinkMTUGet(vmk_Uplink uplink);


/*
 ***********************************************************************
 * vmk_UplinkMTUSet --                                            */ /**
 *
 * \brief  Set uplink MTU
 *
 * \param[in]   uplink            Uplink handle
 * \param[in]   mtu               MTU to set
 *
 * \retval      VMK_OK            MTU is set successfully
 * \retval      Other Value       Otherwise
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_UplinkMTUSet(vmk_Uplink uplink, vmk_uint32 mtu);


/*
 ***********************************************************************
 * vmk_UplinkMACAddrGet --                                        */ /**
 *
 * \brief  Get uplink MAC address
 *
 * \param[in]   uplink            Uplink handle
 * \param[out]  mac               Uplink MAC address
 *
 * \retval      VMK_OK            operation succeeds
 * \retval      Other Value       Otherwise
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_UplinkMACAddrGet(vmk_Uplink uplink,
                                      vmk_EthAddress *mac);


/*
 ***********************************************************************
 * vmk_UplinkDriverInfoGet --                                     */ /**
 *
 * \brief  Return uplink driver info
 *
 * \param[in]   uplink            Uplink handle
 *
 * \return      Uplink driver information
 *
 ***********************************************************************
 */
vmk_UplinkDriverInfo vmk_UplinkDriverInfoGet(vmk_Uplink uplink);


/*
 ***********************************************************************
 * vmk_UplinkStatsGet --                                          */ /**
 *
 * \brief  Return uplink stats
 *
 * \param[in]   uplink            Uplink handle
 *
 * \return      Uplink stats
 *
 ***********************************************************************
 */
vmk_UplinkStats vmk_UplinkStatsGet(vmk_Uplink uplink);


/*
 ***********************************************************************
 * vmk_UplinkPrivStatsLengthGet --                                */ /**
 *
 * \brief  Return the length of device's private stats
 *
 * \param[in]   uplink      Uplink handle
 *
 * \return      Device's private stats in bytes, return 0 if device
 *              doesn't support private stats.
 *
 ***********************************************************************
 */
vmk_ByteCount vmk_UplinkPrivStatsLengthGet(vmk_Uplink uplink);


/*
 ***********************************************************************
 * vmk_UplinkPrivStatsGet --                                      */ /**
 *
 * \brief  Get the content of device's private stats
 *
 * \param[in]   uplink      Uplink handle
 * \param[in]   buffer      Caller allocated buffer to store device's
 *                          private stats
 * \param[in]   length      Length of buffer in bytes
 *
 * \retval      VMK_OK      Private stats stored in buffer successfully
 * \retval      VMK_FAILURE Otherwise
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_UplinkPrivStatsGet(vmk_Uplink uplink,
                                        char *buffer,
                                        vmk_ByteCount length);


/*
 ***********************************************************************
 * vmk_UplinkNameGet --                                           */ /**
 *
 * \brief  Return uplink name
 *
 * \param[in]   uplink            Uplink handle
 *
 * \return      Uplink name
 *
 ***********************************************************************
 */
vmk_Name vmk_UplinkNameGet(vmk_Uplink uplink);


/*
 ***********************************************************************
 * vmk_UplinkCapIsRegistered --                                   */ /**
 *
 * \brief  Indicates if uplink capability is registered or not
 *
 * \param[in]   uplink            Uplink handle
 * \param[in]   cap               Capability
 *
 * \retval      TRUE              if uplink capability is registered
 * \retval      FALSE             Otherwise
 *
 ***********************************************************************
 */
vmk_Bool vmk_UplinkCapIsRegistered(vmk_Uplink uplink, vmk_UplinkCap cap);


/*
 ***********************************************************************
 * vmk_UplinkCapIsEnabled --                                      */ /**
 *
 * \brief  Indicates if uplink capability is enabled or not
 *
 * \param[in]   uplink            Uplink handle
 * \param[in]   cap               Capability
 *
 * \retval      TRUE              if capability is enabled
 * \retval      FALSE             Otherwise
 *
 ***********************************************************************
 */
vmk_Bool vmk_UplinkCapIsEnabled(vmk_Uplink uplink, vmk_UplinkCap cap);

/*
 ***********************************************************************
 * vmk_UplinkPktIRQRx --                                          */ /**
 *
 * \brief Queue a specified packet coming from an uplink for Rx process.
 *
 * \param[in] uplink    Uplink where the packet came from
 * \param[in] pkt       Target packet
 *
 ***********************************************************************
 */
void vmk_UplinkPktIRQRx(vmk_Uplink uplink,
                        vmk_PktHandle *pkt);


/*
 ***********************************************************************
 * vmk_UplinkPktListIRQRx --                                      */ /**
 *
 * \brief Process a list of packets from an uplink.
 *
 * \param[in] uplink    Uplink from where the packets came from.
 * \param[in] pktList   Set of packets to process.
 *
 ***********************************************************************
 */
void vmk_UplinkPktListIRQRx(vmk_Uplink uplink,
                            vmk_PktList pktList);


/*
 ***********************************************************************
 * vmk_UplinkEventCB --                                           */ /**
 *
 * \brief Message callback used for uplink event notification
 *
 * \note Uplink event notifications are asynchronous, meaning that the
 *       event handler should examine the uplink name to determine
 *       current state at the time the callback is made.
 *
 * \note No portset lock is held when callback is made. To acquire a
 *       portset, callback needs to call vmk_UplinkGetByName,
 *       vmk_UplinkGetPortID and vmk_PortsetAcquireByPortID.
 *
 * \param[in]  uplinkName  name of uplink the event implicates
 * \param[in]  event       uplink event
 * \param[in]  eventData   data associated with event, reserved for
 *                         future use, always be NULL.
 * \param[in]  cbData      Data passed to vmk_UplinkRegisterEventCB
 *
 * \retval     None
 *
 ***********************************************************************
 */

typedef void (*vmk_UplinkEventCB)(vmk_Name *uplinkName,
                                  vmk_UplinkEvent event,
                                  vmk_UplinkEventData *eventData,
                                  vmk_AddrCookie cbData);


/*
 ***********************************************************************
 * vmk_UplinkIoctl --                                             */ /**
 *
 * \brief Do an ioctl call against the uplink.
 *
 * This function will call down to device driver to perform an ioctl.
 *
 * \note The caller must not hold any lock.
 *
 * \note The behavior of the ioctl callback is under the responsibility
 * of the driver. The VMkernel cannot guarantee binary compatibility or
 * system stability over this call. It is up to the API user to ensure
 * version-to-version compatibility of ioctl calls with the provider of
 * the driver.
 *
 * \param[in]   uplink            Uplink handle
 * \param[in]   cmd               Ioctl command
 * \param[in]   args              Ioctl arguments
 * \param[out]  result            Ioctl result
 *
 * \retval      VMK_OK            If the ioctl call succeeds
 * \retval      VMK_NOT_SUPPORTED If the uplink doesn't support ioctl
 * \retval      Other status      If the device ioctl call failed
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkIoctl(vmk_Uplink      uplink,
                                 vmk_uint32      cmd,
                                 vmk_AddrCookie  args,
                                 vmk_uint32     *result);


/*
 ***********************************************************************
 * vmk_UplinkReset --                                             */ /**
 *
 * \brief Reset the uplink device underneath.
 *
 * This function will call down to device driver, close and re-open the
 * device. The link state will consequently go down and up.
 *
 * \note The caller must not hold any lock.
 *
 * \note The behavior of the reset callback is under the responsibility
 * of the driver. The VMkernel cannot guarantee binary compatibility or
 * system stability over this call. It is up to the API user to ensure
 * version-to-version compatibility of the reset call with the provider of
 * the driver.
 *
 * \note This call is asynchronous, the function might return before
 * the driver call completed.
 *
 * \param[in]   uplink            Uplink handle
 *
 * \retval      VMK_OK            if the reset call succeeds
 * \retval      VMK_NOT_SUPPORTED if the uplink doesn't support reset
 * \retval      Other status      if the device reset call failed
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkReset(vmk_Uplink uplink);

/*
 ***********************************************************************
 * vmk_UplinkGetWatchdogTimeout --                                */ /**
 *
 * \brief Get the uplink device watchdog timeout value.
 *
 * By default every device's watchdog timeout value is set to
 * pre-configured system wide value. This API can be used to return
 * the current watchdog timeout value used by the specific uplink.
 *
 * \note Default watchdog time out value may be changed by setting the
 *       config option NetSchedWatchdogTimeout to the desired value
 *
 * \param[in]   uplink            Uplink handle
 * \param[out]  timeoutMS         Current watchdog timeout in MS
 *
 * \retval      VMK_OK            on success
 * \retval      VMK_BAD_PARAM     if uplink or timeoutMS is not valid
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_UplinkGetWatchdogTimeout(vmk_Uplink uplink,
                                              vmk_uint16 *timeoutMS);



/*
 ***********************************************************************
 * vmk_UplinkSetWatchdogTimeout --                                */ /**
 *
 * \brief Set the uplink device watchdog timeout value
 *
 * By default every device's watchdog timeout value is set to
 * pre-configured system wide value. This API can be used to change
 * the default watchdog timeout value to a timeout value that is more
 * specific to the device.
 *
 * In some cases the driver may want to call this API before performing
 * operations that stop the queues for a longer time than usual.  This
 * allows the driver to avoid spurious watchdog resets from the uplink
 * layer.  The driver can restore the original timeout value after
 * completing the operation.
 *
 * \note Default watchdog timeout value may be changed by setting the
 *       config option NetSchedWatchdogTimeout to the desired value
 *
 * \note The maximum watchdog timeout value can be set is 60 seconds,
 *       and the minimum is 1 second.
 *
 * \param[in]   uplink            Uplink handle
 * \param[in]   timeoutMS         Timeout in MS
 *
 * \retval      VMK_OK            on success
 * \retval      VMK_BAD_PARAM     if uplink or timeoutMS is not valid
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_UplinkSetWatchdogTimeout(vmk_Uplink uplink,
                                              vmk_uint16 timeoutMS);


/*
 ***********************************************************************
 * vmk_UplinkGetPortID --                                         */ /**
 *
 * \brief Return ID of port uplink is connecting to
 *
 * This function will return the ID of port where uplink connects
 *
 * \note The caller must not hold any lock
 *
 * \note This function will not block
 *
 * \param[in]   uplink            Uplink name
 * \param[out]  portID            Port ID
 *
 * \retval      VMK_OK            If get port ID call succeeds
 * \retval      VMK_BAD_PARAM     If uplink or portID is invalid
 ***********************************************************************
 */
VMK_ReturnStatus vmk_UplinkGetPortID(vmk_Uplink uplink,
                                     vmk_SwitchPortID *portID);


/*
 ***********************************************************************
 * vmk_UplinkUpdateLinkState --                                   */ /**
 *
 * \brief Update operational link status information related to a
 *        specified uplink with a bundle containing the information.
 *
 * \note  Uplink's operational link status will be updated. Driver should
 *        not call this function if uplink's administrative link status
 *        is down but NIC's operational link status is up.
 *
 * \param[in] uplink   Uplink aimed
 * \param[in] linkInfo Structure containing link information
 *
 * \retval    VMK_OK             Operational link status is updated
 *                               successfully.
 *            VMK_IS_DISABLED    Uplink's administrative link status is
 *                               down and driver reports operational
 *                               link status is up.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkUpdateLinkState(vmk_Uplink uplink,
                                           vmk_LinkStatus *linkInfo);



/*
 ***********************************************************************
 * vmk_UplinkRegisterEventCB --                                   */ /**
 *
 * \brief Register a handler to receive uplink event notification
 *
 * \note These are asynchronous event notifications, meaning that the
 *       event handler should examine the uplink to determine current
 *       state at the time the callback is made.
 *
 * \note If NULL uplink is specified, handler will be linked to global
 *       event list and it will receive events from any uplink.
 *
 * \param[in]   uplink            Pointer of uplink where handler will
 *                                be linked
 * \param[in]   eventMask         Combination of uplink event IDs
 *                                handler is interested in
 * \param[in]   cb                Handler to call to notify an uplink
 *                                event
 * \param[in]   cbData            Data to pass to the handler
 * \param[out]  handle            Handle to unregister this handler
 *
 * \retval      VMK_OK            Registration succeeded
 * \retval      VMK_FAILURE       Otherwise
 ***********************************************************************
 */
VMK_ReturnStatus vmk_UplinkRegisterEventCB(
   vmk_Uplink uplink,
   vmk_uint64 eventMask,
   vmk_UplinkEventCB cb,
   vmk_AddrCookie cbData,
   vmk_UplinkEventCBHandle *handle);


/*
 ***********************************************************************
 * vmk_UplinkUnregisterEventCB --                                 */ /**
 *
 * \brief  Unregister a handler to receive uplink event notifications.
 *
 * \param[in]   handle            Handle return by register process
 *
 * \retval      VMK_OK            Always
 ***********************************************************************
 */
VMK_ReturnStatus vmk_UplinkUnregisterEventCB(
   vmk_UplinkEventCBHandle handle);


#endif /* _VMKAPI_NET_UPLINK_H_ */
/** @} */
/** @} */
