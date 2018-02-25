/* **********************************************************
 * Copyright 2006 - 2011 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Uplink                                                         */ /**
 * \addtogroup Network
 *@{
 * \addtogroup Uplink Uplink management
 *@{
 * \defgroup Incompat Uplink APIs
 *@{
 *
 * \par Uplink:
 *
 * A module may have many different functional direction and one of
 * them is to be a gateway to external network.
 * Thereby vmkernel could rely on this module to Tx/Rx packets.
 *
 * So one can imagine an uplink as a vmkernel bundle containing
 * all the handle required to interact with a module's internal network
 * object.
 *
 * Something important to understand is that an uplink need to reflect
 * the harware services provided by the network interface is linked to.
 * Thereby if your network interface  do vlan tagging offloading a capability
 * should be passed to vmkernel to express this service and it will be able
 * to use this capability to optimize its internal path when the got
 * corresponding uplink is going to be used.
 *
 ***********************************************************************
 */

#ifndef _VMKAPI_NET_UPLINK_INCOMPAT_H_
#define _VMKAPI_NET_UPLINK_INCOMPAT_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "net/vmkapi_net_types.h"
#include "net/vmkapi_net_pkt.h"
#include "net/vmkapi_net_pktlist.h"
#include "net/vmkapi_net_queue.h"
#include "net/vmkapi_net_queue_incompat.h"
#include "net/vmkapi_net_dcb.h"
#include "net/vmkapi_net_port.h"
#include "net/vmkapi_net_pt.h"
#include "net/vmkapi_net_uplink.h"

/**
 * \brief Unrecognized link speed
 */
#define VMK_LINK_SPEED_UNKNOWN    (-1)

/**
 * \brief Unrecognized link duplex
 */
#define VMK_LINK_DUPLEX_UNKNOWN   (0)

/**
 * \brief Number of bytes for uplink driver info fields.
 */
#define VMK_UPLINK_DRIVER_INFO_MAX      VMK_MODULE_NAME_MAX

/**
 * \brief Capabilities provided by the device associated to an uplink.
 */

typedef vmk_uint64 vmk_UplinkCapabilities;

/**
 * \brief Structure containing PCI information of the device associated to an uplink.
 */

typedef struct {
   /** Device representing the uplink */
   vmk_Device device;

   /** Device DMA constraints */
   vmk_DMAConstraints constraints;
} vmk_UplinkDeviceInfo;


/**
 * \brief Structure containing memory resources information related to the device associated to an uplink.
 */

typedef struct {

   /** Uplink I/O address */
   vmk_AddrCookie  baseAddr;

   /** Shared mem start */
   vmk_AddrCookie  memStart;

   /** Shared mem end */
   vmk_AddrCookie  memEnd;

   /** DMA channel */
   vmk_uint8       dma;
} vmk_UplinkMemResources;


/*
 ***********************************************************************
 * vmk_UplinkOpenDevCB --                                         */ /**
 *
 * \brief Handler used by vmkernel to open the device associated to an
 *        uplink
 *
 * \param[in] driverData   Points to the internal module structure for
 *                         the device associated to the uplink. Before
 *                         calling vmk_DeviceRegister, device driver
 *                         needs to assign this pointer to member
 *                         driverData in structure vmk_UplinkRegData.
 *
 * \retval    VMK_OK       Open succeeded
 * \retval    VMK_FAILURE  Otherwise
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkOpenDevCB)(vmk_AddrCookie driverData);

/*
 ***********************************************************************
 * vmk_UplinkCloseDevCB --                                        */ /**
 *
 * \brief Handler used by vmkernel to close the device associated to an
 *        uplink
 *
 * \param[in] driverData   Points to the internal module structure for
 *                         the device associated to the uplink. Before
 *                         calling vmk_DeviceRegister, device driver
 *                         needs to assign this pointer to member
 *                         driverData in structure vmk_UplinkRegData.
 *
 * \retval    VMK_OK       Close succeeded
 * \retval    VMK_FAILURE  Otherwise
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkCloseDevCB)(vmk_AddrCookie driverData);


/*
 ***********************************************************************
 * vmk_UplinkIoctlCB --                                           */ /**
 *
 * \brief  Handler used by vmkernel to do an ioctl call against the
 *         device associated to an uplink.
 *
 *
 * \param[in]  driverData         driver private data pointer sepecified
 *                                in vmk_UplinkRegData during
 *                                registration
 * \param[in]  cmd                Command ioctl to be issued
 * \param[in]  args               Arguments to be passed to the ioctl call
 * \param[out] result             Result value of the ioctl call
 *
 * \retval    VMK_OK              If ioctl call succeeded
 * \retval    VMK_FAILURE         Otherwise
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkIoctlCB)(vmk_AddrCookie driverData,
                                              vmk_uint32 cmd,
                                              vmk_AddrCookie args,
                                              vmk_uint32 *result);

/*
 ***********************************************************************
 * vmk_UplinkGetMemResourcesCB --                                 */ /**
 *
 * \brief  Handler used by vmkernel to get the memory resources of a device
 *         associated to an uplink.
 *
 * \param[in]  driverData   Points to the internal module structure for
 *                          the device associated to the uplink. Before
 *                          calling vmk_DeviceRegister, device driver
 *                          needs to assign this pointer to member
 *                          driverData in structure vmk_UplinkRegData.
 * \param[out] resources    Memory resources of the device
 *
 * \retval     VMK_OK       If the information is properly stored
 * \retval     VMK_FAILURE  Otherwise
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkGetMemResourcesCB)(vmk_AddrCookie driverData,
                                                        vmk_UplinkMemResources *resources);

/*
 ***********************************************************************
 * vmk_UplinkGetDevicePropertiesCB --                             */ /**
 *
 * \brief  Handler used by vmkernel to get pci properties of a device
 *         associated to an uplink.
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[out]  devInfo      Device properties of the device.
 *
 * \retval      VMK_OK       If the information is properly stored
 * \retval    VMK_FAILURE  Otherwise
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkGetDevicePropertiesCB)(vmk_AddrCookie driverData,
                                                            vmk_UplinkDeviceInfo *devInfo);


/*
 ***********************************************************************
 * vmk_UplinkGetNameCB --                                         */ /**
 *
 * \brief  Handler used by vmkernel to get the name of the device
 *         associated to an uplink
 *
 * \param[in]  driverData   Points to the internal module structure for
 *                          the device associated to the uplink. Before
 *                          calling vmk_DeviceRegister, device driver
 *                          needs to assign this pointer to member
 *                          driverData in structure vmk_UplinkRegData.
 * \param[out] devName      Buffer used to store uplink device name.
 * \param[in]  devNameLen   Length of devName buffer.
 *
 * \retval    VMK_OK              If the name is properly stored.
 * \retval    VMK_LIMIT_EXCEEDED  If the name is too long for provided
 *                                buffer.
 * \retval    VMK_FAILURE         Otherwise.
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkGetNameCB)(vmk_AddrCookie driverData,
                                                char *devName,
                                                vmk_ByteCount devNameLen);


/*
 ***********************************************************************
 * vmk_UplinkSetPktTraceCB --                                     */ /**
 *
 * \brief  Handler used by vmkernel to enable/disable pktTrace for
 *         device associated with an uplink.
 *
 * \param[in]  driverData   Points to the internal module structure for
 *                          the device associated to the uplink. Before
 *                          calling vmk_DeviceRegister, device driver
 *                          needs to assign this pointer to member
 *                          driverData in structure vmk_UplinkRegData.
 * \param[in]  enable       Flag to enable/disable tracing.
 * \param[in]  traceSrcID   Identifier for the uplink device generating
 *                          trace events. Stored locally and used in
 *                          RecordEvent/Interrupt vmkapi calls.
 *
 * \retval    VMK_OK       If the driver information is properly stored
 * \retval    VMK_FAILURE  Otherwise
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkSetPktTraceCB)(vmk_AddrCookie driverData,
                                                    vmk_Bool enable,
                                                    vmk_uint64 traceSrcID);

/**
 * \brief Default value for timeout handling before panic.
 */

#define VMK_UPLINK_WATCHDOG_HIT_CNT_DEFAULT 5

/*
 ***********************************************************************
 * vmk_UplinkGetWatchdogHitCnt --                                 */ /**
 *
 * \brief  Used only if the module provides a timeout mechanism to
 *         recover from a wedged device.
 *         Handler used by vmkernel to get the timeout hit counter needed
 *         before hitting a panic.
 *         If no panic mode is implemented you could ignore this handler.
 *
 * \param[in]  driverData   Points to the internal module structure for
 *                          the device associated to the uplink. Before
 *                          calling vmk_DeviceRegister, device driver
 *                          needs to assign this pointer to member
 *                          driverData in structure vmk_UplinkRegData.
 * \param[out] counter      Used to store the timeout hit counter
 *
 * \retval    VMK_OK        If the information is properly stored
 * \retval    VMK_FAILURE   Otherwise
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkGetWatchdogHitCnt)(vmk_AddrCookie driverData,
                                                        vmk_int16 *counter);

/*
 ***********************************************************************
 * vmk_UplinkSetWatchdogHitCnt --                                 */ /**
 *
 * \brief  Used only if the module provides a timeout mechanism to
 *         recover from a wedged device.
 *         Handler used by vmkernel to set the timeout hit counter
 *         needed before hitting a panic.
 *         If no panic mode is implemented you could ignore this handler.
 *
 * \param[in] driverData   Points to the internal module structure for
 *                         the device associated to the uplink. Before
 *                         calling vmk_DeviceRegister, device driver
 *                         needs to assign this pointer to member
 *                         driverData in structure vmk_UplinkRegData.
 * \param[in] counter      The timeout hit counter
 *
 * \retval    VMK_OK       If the new setting is effective
 * \retval    VMK_FAILURE  Otherwise
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkSetWatchdogHitCnt)(vmk_AddrCookie driverData,
                                                        vmk_int16 counter);

/*
 ***********************************************************************
 * vmk_UplinkGetWatchdogStats --                                  */ /**
 *
 * \brief  Used only if the module provides a timeout mechanism to
 *         recover from a wedged device.
 *         Handler used by vmkernel to know the number of times the recover
 *         process (usually a reset) has been run on the device associated
 *         to an uplink. Roughly the number of times the device got wedged.
 *
 * \param[in]  driverData   Points to the internal module structure for
 *                          the device associated to the uplink. Before
 *                          calling vmk_DeviceRegister, device driver
 *                          needs to assign this pointer to member
 *                          driverData in structure vmk_UplinkRegData.
 * \param[out] counter      The number of times the device got wedged
 *
 * \retval     VMK_OK       If the information is properly stored
 * \retval     VMK_FAILURE  Otherwise
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkGetWatchdogStats)(vmk_AddrCookie driverData,
                                                       vmk_int16 *counter);

/**
 * \brief Define the different status of uplink watchdog panic mod
 */

typedef enum {

   /** \brief The device's watchdog panic mod is disabled */
   VMK_UPLINK_WATCHDOG_PANIC_MOD_DISABLE,

   /** \brief The device's watchdog panic mod is enabled */
   VMK_UPLINK_WATCHDOG_PANIC_MOD_ENABLE
} vmk_UplinkWatchdogPanicModState;

/*
 ***********************************************************************
 * vmk_UplinkGetWatchdogPanicModState --                          */ /**
 *
 * \brief  Used only if the module provides a timeout mechanism to
 *         recover from a wedged device.
 *         Handler used by vmkernel to know if the timeout panic mod
 *         is enabled or not.
 *
 * \param[in]  driverData   Points to the internal module structure for
 *                          the device associated to the uplink. Before
 *                          calling vmk_DeviceRegister, device driver
 *                          needs to assign this pointer to member
 *                          driverData in structure vmk_UplinkRegData.
 * \param[out] status       Status of the watchdog panic mod
 *
 * \retval     VMK_OK       If the information is properly stored
 * \retval     VMK_FAILURE  Otherwise
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkGetWatchdogPanicModState)(vmk_AddrCookie driverData,
                                                               vmk_UplinkWatchdogPanicModState *state);

/*
 ***********************************************************************
 * vmk_UplinkSetWatchdogPanicModState --                          */ /**
 *
 * \brief  Used only if the module provides a timeout mechanism to
 *         recover from a wedged device.
 *         Handler used by vmkernel to enable or disable the timeout
 *         panic mod. Set panic mod could be useful for debugging as it
 *         is possible to get a coredump at this point.
 *
 * \param[in] driverData   Points to the internal module structure for
 *                         the device associated to the uplink. Before
 *                         calling vmk_DeviceRegister, device driver
 *                         needs to assign this pointer to member
 *                         driverData in structure vmk_UplinkRegData.
 * \param[in] enable       Tne status of the watchdog panic mod
 *
 * \retval    VMK_OK       If the new panic mod is effective
 * \retval    VMK_FAILURE  Otherwise
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkSetWatchdogPanicModState)(vmk_AddrCookie driverData,
                                                               vmk_UplinkWatchdogPanicModState state);


/*
 ***********************************************************************
 * vmk_UplinkNetqueueOpFunc --                                    */ /**
 *
 * \brief  Handler used by vmkernel to issue netqueue control operations
 *
 * \param[in] driverData   Points to the internal module structure for
 *                         the device associated to the uplink. Before
 *                         calling vmk_DeviceRegister, device driver
 *                         needs to assign this pointer to member
 *                         driverData in structure vmk_UplinkRegData.
 * \param[in] op           Netqueue operation
 * \param[in] opArgs       Arguments to Netqueue operation
 *
 * \retval    VMK_OK       Operation succeeded
 * \retval    VMK_FAILURE  Otherwise
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkNetqueueOpFunc)(vmk_AddrCookie driverData,
                                                     vmk_NetqueueOp op,
                                                     vmk_AddrCookie opArgs);

/*
 ***********************************************************************
 * vmk_UplinkNetqueueXmit --                                      */ /**
 *
 * \brief  Transmit a list of packets via a Tx queue
 *
 * \param[in]  driverData    Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[in]  queueID       ID of tx queue packet
 * \param[in]  pktList       Handle of packet list to transmit
 *
 * \retval     VMK_OK        Operation succeeded
 * \retval     VMK_FAILURE   Otherwise
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkNetqueueXmit)(vmk_AddrCookie driverData,
                                                   vmk_NetqueueQueueID queueID,
                                                   vmk_PktList pktList);


/*
 ***********************************************************************
 * vmk_UplinkGetStatesCB --                                       */ /**
 *
 * \brief  Get the state of a device associated to an uplink.
 *
 * \param[in] driverData   Points to the internal module structure for
 *                         the device associated to the uplink. Before
 *                         calling vmk_DeviceRegister, device driver
 *                         needs to assign this pointer to member
 *                         driverData in structure vmk_UplinkRegData.
 * \param[out] state       State of the device
 *
 * \retval    VMK_OK       If the information is properly stored
 * \retval    VMK_FAILURE  Otherwise
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkGetStatesCB)(vmk_AddrCookie driverData,
                                                  vmk_PortClientStates *states);


/*
 ***********************************************************************
 * vmk_UplinkPTOpFunc --                                          */ /**
 *
 * \brief  The routine to dispatch PT management operations to
 *         driver exported callbacks
 *
 * \param[in] driverData   Points to the internal module structure for
 *                         the device associated to the uplink. Before
 *                         calling vmk_DeviceRegister, device driver
 *                         needs to assign this pointer to member
 *                         driverData in structure vmk_UplinkRegData.
 * \param[in] op           The operation
 * \param[in] args         The optional arguments for the operation
 *
 * \retval    VMK_OK       If the operation succeeds
 * \retval    VMK_FAILURE  Otherwise
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkPTOpFunc)(vmk_AddrCookie driverData,
                                               vmk_NetPTOP op,
                                               vmk_AddrCookie args);


/**
 * \brief Structure used to have access to the timeout properties of the
 *        device associated to an uplink. If the module does not provide
 *        a timeout mechanism, this information can be ignored.
 */

typedef struct {

   /**
    * Handler used to retrieve the number of times the device handles
    * timeout before hitting a panic
    */
   vmk_UplinkGetWatchdogHitCnt         getHitCnt;

   /**
    * Handler used to set the number of times the device handles timeout
    * before hitting a panic
    */
   vmk_UplinkSetWatchdogHitCnt         setHitCnt;

   /**
    * Handler used to retrieve the global number of times the device
    * hit a timeout
    */
   vmk_UplinkGetWatchdogStats          getStats;

   /**
    *  Handler used to retrieve the timeout panic mod's status for the
    * device
    */
   vmk_UplinkGetWatchdogPanicModState  getPanicMod;

   /** Handler used to set the timeout panic mod's status for the device */
   vmk_UplinkSetWatchdogPanicModState  setPanicMod;
} vmk_UplinkWatchdogFunctions;

typedef struct {
   /** Netqueue control operation entry point */
   vmk_UplinkNetqueueOpFunc               netqOpFunc;

   /** Netqueue packet transmit function (obsolete) */
   vmk_UplinkNetqueueXmit                 netqXmit;

   /** UplinkQueue operations */
   vmk_UplinkQueueOps                     ops;

   /** UplinkQueue dynamic RSS operations */
   vmk_UplinkQueueRSSDynOps               rssDynOps;
} vmk_UplinkNetqueueFunctions;

typedef struct {
   /** dispatch routine for PT management operations */
   vmk_UplinkPTOpFunc                     ptOpFunc;
} vmk_UplinkPTFunctions;


/**
 * \brief Structure used to have access to the properties of the
 *        device associated to an uplink.
 */
typedef struct vmk_UplinkPropOps {

   /** Handler used to set device link status */
   vmk_UplinkLinkStatusSetCB        uplinkLinkStatusSet;

   /** Handler used to set device MAC address */
   vmk_UplinkMACAddrSetCB           uplinkMACAddrSet;

   /** Handler used to restart auto-negotiation on device */
   vmk_UplinkRestartNegotiationCB   uplinkNegotiationRestart;
} vmk_UplinkPropOps;

/**
 * Vmklinux specific stats
 */
typedef struct vmk_UplinkVmklinuxStats {
   /** The number of rx packets received by the interface hosting the driver */
   vmk_uint64  intRxPkt;

   /** The number of tx packets sent by the interface hosting the driver */
   vmk_uint64  intTxPkt;

   /** The number of rx packets dropped by the interface hosting the driver */
   vmk_uint64  intRxDrp;

   /** The number of tx packets dropped by the interface hosting the driver */
   vmk_uint64  intTxDrp;
} vmk_UplinkVmklinuxStats;

/*
 ***********************************************************************
 * vmk_UplinkVmklinuxStatsGetCB --                                */ /**
 *
 * \brief  Used by vmkernel to get vmklinux specific statistics counters
 *
 * \param[in]  driverData   Points to the internal module structure for
 *                          the device associated to the uplink. Before
 *                          calling vmk_DeviceRegister, device driver
 *                          needs to assign this pointer to member
 *                          driverData in structure vmk_UplinkRegData.
 * \param[out] stats        Vmklinux specific stats
 *
 * \retval     VMK_OK       If the information is properly stored
 * \retval     VMK_FAILURE  Otherwise
 *
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_UplinkVmklinuxStatsGetCB)(vmk_AddrCookie driverData,
                                                         vmk_UplinkVmklinuxStats *stats);

/**
 * Uplink is driven by vmklinux
 */
#define VMK_UPLINK_CAP_LEGACY ((vmk_uint32)(~0))

/**
 * Legacy operations for vmklinux drivers
 */
typedef struct vmk_UplinkLegacyOps {

   /** Handler used to set up the resources of the device */
   vmk_UplinkOpenDevCB               open;

   /** Handler used to release the resources of the device */
   vmk_UplinkCloseDevCB              close;

   /** uplink ioctl callback */
   vmk_UplinkIoctlCB                 ioctl;

   /** Handler used to retrieve the name of the device */
   vmk_UplinkGetNameCB               getName;

   /** Handler used to retrieve the state of the device */
   vmk_UplinkGetStatesCB             getStates;

   /** Handler used to retrieve device properties of the device */
   vmk_UplinkGetDevicePropertiesCB   getDeviceProperties;

   /** Handler used to get vmklinux uplink stats */
   vmk_UplinkVmklinuxStatsGetCB      vmklinuxStatsGet;

   /** Set of functions giving access to the PT services of the device */
    vmk_UplinkPTFunctions            ptFns;

   /** Set of functions giving access to the watchdog management of the device */
   vmk_UplinkWatchdogFunctions       watchdogFns;

   /** Handler used to set packet tracing state of the device */
   vmk_UplinkSetPktTraceCB           setPktTrace;
} vmk_UplinkLegacyOps;

/**
 * \brief Structure passed to vmkernel in order to interact with the device
 *        associated to an uplink.
 */

typedef struct vmk_UplinkFunctions {

   /** Set of functions giving access to the core services of the device */
   vmk_UplinkOps                    coreFns;

   /** Set of functions supporting vmklinux drivers */
   vmk_UplinkLegacyOps              legacyFns;

   /** Set of functions giving access to the vlan services of the device */
   vmk_UplinkVLANFilterOps          vlanFns;

   /** Set of functions giving access to the properties/statistics of the device */
   vmk_UplinkPropOps                propFns;

   /** Set of functions giving access to the watchdog management of the device */
   vmk_UplinkWatchdogFunctions      watchdogFns;

   /** Set of functions giving access to the netqueue services of the device */
   vmk_UplinkNetqueueFunctions      netqueueFns;

   /** Set of functions giving access to the PT services of the device */
   vmk_UplinkPTFunctions            ptFns;

   /** Set of functions giving access to the DCB services of the device */
   vmk_UplinkDCBOps                 dcbFns;

   /** Set of functions giving access to NetDump services of the device */
   vmk_UplinkNetDumpOps             netdumpFns;

   /** Set of functions giving access to WOL services of the device */
   vmk_UplinkWOLOps                 wolFns;

   /** Set of functions giving access to coalesce parameters */
   vmk_UplinkCoalesceParamsOps      colFns;

   /** Set of functions giving access to EEPROM */
   vmk_UplinkEEPROMOps              eepromFns;

   /** Set of functions giving access to uplink registers */
   vmk_UplinkRegDumpOps             regDumpFns;

   /** Set of functions giving access to uplink self test */
   vmk_UplinkSelfTestOps            selfTestFns;

   /** Set of functions giving access to RX/TX ring size params */
   vmk_UplinkRingParamsOps          ringParamsFns;

   /** Set of functions giving access to pause parameters */
   vmk_UplinkPauseParamsOps         pauseParamsFns;

   /** Set of functions giving access to private stats */
   vmk_UplinkPrivStatsOps           privStatsFns;

   /** Set of functions giving access to enacapsulation offload settings */
   vmk_UplinkEncapOffloadOps        encapOffloadFns;

   /** Set of fucntions giving access to cable type operations */
   vmk_UplinkCableTypeOps           cableTypeFns;

   /** Set of fucntions giving access to PHY address operations */
   vmk_UplinkPhyAddressOps          phyAddressFns;

   /** Set of fucntions giving access to transceiver type operations */
   vmk_UplinkTransceiverTypeOps     transceiverTypeFns;

   /** Set of fucntions giving access to message level operations */
   vmk_UplinkMessageLevelOps        messageLevelFns;

   /** Set of functions giving access to Geneve offload settings */
   vmk_UplinkGeneveOffloadParams    geneveOffloadFns;
} vmk_UplinkFunctions;


/*
 ***********************************************************************
 * vmk_UplinkWatchdogTimeoutHit --                                */ /**
 *
 * \brief Notify vmkernel that a watchdog timeout has occurred.
 *
 * \note If an uplink driver has a watchdog for the transmit queue of the
 *       device, the driver should notify vmkernel when a timeout occurs.
 *       Vmkernel may use this information to determine the reliability
 *       of a particular uplink.
 *
 * \param[in]  uplink    Uplink aimed
 *
 * \retval     VMK_OK    Always
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkWatchdogTimeoutHit(vmk_Uplink uplink);


/*
 ***********************************************************************
 * vmk_UplinkAllocWithName --                                     */ /**
 *
 * \brief  Find a uplink handle by name, if not found, allocate a uplink
 *         handle with the given name
 *
 * \param[in]  devName        Name of uplink to find
 * \param[out] uplink         Handle of uplink allocated
 *
 * \retval    VMK_OK          Uplink is found or allocation finishes
 *                            successfully
 * \retval    VMK_FAILURE     Otherwise
 ***********************************************************************
 */
VMK_ReturnStatus vmk_UplinkAllocWithName(vmk_Name   *devName,
                                         vmk_Uplink *uplink);


/*
 ***********************************************************************
 * vmk_UplinkRegister --                                          */ /**
 *
 * \brief Notify vmkernel that an uplink has been connected.
 *
 * \note This function create the bond between vmkernel uplink and
 *       a module internal structure.
 *       Through this connection vmkernel will be able to manage
 *       Rx/Tx and other operations on module network services.
 *
 * \note Field moduleID in regData is the ID of device driver module. If
 *       calling module is a broker proxying driver callbacks from
 *       uplink layer to device driver, it should pass its own module ID
 *       as modID.
 *
 * \param[in]  regData       Information passed to vmkernel to bind an
 *                           uplink to a module internal NIC representation
 * \param[in]  uplink        The uplink object returned by
 *                           vmk_UplinkAllocWithName
 * \param[in]  modID         ID of calling module
 *
 * \retval     VMK_OK        if succeed
 * \retval     VMK_FAILURE   otherwise
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkRegister(vmk_UplinkRegData *regData,
                                    vmk_Uplink uplink,
                                    vmk_ModuleID modID);


/*
 ***********************************************************************
 * vmk_UplinkUnregister                                           */ /**
 *
 * \brief Destroys the corresponding uplink
 *
 * \note Once called, the uplink handle is no longer valid.
 *
 * \param[in]  uplink        Uplink to unregister
 *
 * \retval     VMK_OK        if succeed
 * \retval     VMK_FAILURE   otherwise
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkUnregister(vmk_Uplink uplink);


/*
 ***********************************************************************
 * vmk_UplinkOpen --                                              */ /**
 *
 * \brief Open the device associated with the uplink
 *
 * \note This function needs to be called if the device associated with
 *       the uplink is not a PCI device. For PCI device,
 *       vmk_PCIDoPostInsert() should be called instead.
 *
 * \param[in]  uplink        Uplink to be opened
 *
 * \retval     VMK_OK        if succeed
 * \retval     VMK_FAILURE   otherwise
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkOpen(vmk_Uplink uplink);


/*
 ***********************************************************************
 * vmk_UplinkClose --                                             */ /**
 *
 * \brief Close the device associated with the uplink and disconnect the
 *        uplink from the network services
 *
 * \note This function needs to be called if the device associated with
 *       the uplink is not a PCI device. For PCI device,
 *       vmk_PCIDoPreRemove() should be called instead.
 *
 * \param[in]  uplink        Uplink to be closed
 *
 * \retval     VMK_OK        if succeed
 * \retval     VMK_FAILURE   otherwise
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkClose(vmk_Uplink uplink);


/*
 ***********************************************************************
 * vmk_UplinkIsNameAvailable --                                   */ /**
 *
 * \brief Check if a name is already used by an uplink in vmkernel.
 *
 * \param[in]  uplinkName     Name of the uplink
 *
 * \retval     VMK_TRUE       the uplink name is available
 * \retval     VMK_FALSE      otherwise
 *
 ***********************************************************************
 */

vmk_Bool vmk_UplinkIsNameAvailable(char *uplinkName);


/*
 ***********************************************************************
 * vmk_UplinkCapabilitySet --                                     */ /**
 *
 * \brief Enable/Disable a capability for an uplink.
 *
 * \param[in] uplinkCaps    Capabilities to be modified
 * \param[in] cap           Capability to be enabled/disabled
 * \param[in] enable        true => enable, false => disable
 *
 * \retval    VMK_OK        If capability is valid
 * \retval    VMK_NOT_FOUND Otherwise
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkCapabilitySet(vmk_UplinkCapabilities *uplinkCaps,
                                         vmk_PortClientCaps cap,
                                         vmk_Bool enable);


/*
 ***********************************************************************
 * vmk_UplinkCapabilityGet --                                     */ /**
 *
 * \brief Retrieve status of a capability for an uplink.
 *
 * \param[in]  uplinkCaps    Capabilities to be modified
 * \param[in]  cap           Capability to be checked
 * \param[out] status        true => enabled, false => disabled
 *
 * \retval     VMK_OK        If capability is valid
 * \retval     VMK_NOT_FOUND Otherwise
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkCapabilityGet(vmk_UplinkCapabilities *uplinkCaps,
                                         vmk_PortClientCaps cap,
                                         vmk_Bool *status);


/*
 ***********************************************************************
 * vmk_UplinkCapRegisterHighDMA --                                */ /**
 *
 * \brief Device driver claims it can support high DMA capability
 *
 * \param[in]  uplink        uplink handle
 *
 * \retval     VMK_OK        Capability register successfully
 * \retval     VMK_NOT_FOUND Otherwise
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkCapRegisterHighDMA(vmk_Uplink uplink);


/*
 ***********************************************************************
 * vmk_PktListRxProcess --                                        */ /**
 *
 * \brief Process a list of packets from an uplink.
 *
 * \param[in] pktList   Set of packets to process.
 * \param[in] uplink    Uplink from where the packets came from.
 *
 ***********************************************************************
 */
void vmk_PktListRxProcess(vmk_PktList pktList,
                          vmk_Uplink uplink);


/*
 ***********************************************************************
 * vmk_UplinkRecordInterrupt --                                   */ /**
 *
 * \brief Record timestamp for an interrupt on this uplink
 *
 * This function is part of the pktTrace infrastructure and it records
 * timestamp for an interrupt. This handles the case when an interrupt
 * indicates that a packet is available on physical device for
 * reception.
 *
 * \note  Interrupts can be fired for a variety of reasons and packets
 *        can be pulled in polling mode. So the interrupt timestamp is
 *        a "hint" of the maximium amount of time a packet was waiting
 *        in the device. Actual waiting time can be much lower if the
 *        device transitioned to polling mode.
 *
 * \note  Even on VMK_OK, this event might be missing from user-visible
 *        trace, e.g., if the event log buffer is not being drained
 *        fast enough. Log buffer processing tools MUST be designed to
 *        work around missing event traces.
 *
 * \note  As a side effect of calling this function the packet may now
 *        contain a pktTrace attribute.
 *
 * \param[in]  cookie     Cookie of interrupt generating the trace.
 * \param[in]  traceSrcID Identifier for the uplink device that
 *                        generated this interrupt.
 *
 * \retval     VMK_OK         Event recorded successfully.
 * \retval     VMK_FAILURE    Event not recorded.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkRecordInterrupt(vmk_IntrCookie cookie,
                                           vmk_uint64 traceSrcID);


#endif /* _VMKAPI_NET_UPLINK_INCOMPAT_H_ */
/** @} */
/** @} */
/** @} */
