/***********************************************************
 * Copyright 2008 - 2009 VMware, Inc.  All rights reserved.
 ***********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Port                                                           */ /**
 * \addtogroup VDS
 * @{
 * \defgroup Portset Port APIs
 *
 * The ports on virtual switch provide logical connection points
 * among virtual devices and between virtual and physical devices.
 *
 * In vSphere platform, virtual switch implementation manages ports
 * connected to virtual ethernet adapters in guest OSes (vNICs),
 * virtual device in vmkernel (vmknic), physical adapters (uplinks),
 * and internal ports (i.e. switch management port).
 *
 * Uplink ports are ports associated with physical adapters, providing
 * a connection between a virtual network and a physical network.
 * Physical adapters connect to uplink ports when they are initialized
 * by a device driver or when the teaming policies for virtual switches
 * are reconfigured. Vmkernel and physical adapters provide additional
 * features on uplink ports, e.g. netqueue load balancer, IO resource
 * management, hardware offloading.
 *
 * With vmk_SwitchPortID, switch implementation can call port APIs to
 * control states and operations of ports, including uplinks.
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_VDS_PORT_H_
#define _VMKAPI_VDS_PORT_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "net/vmkapi_net_uplink.h"
#include "net/vmkapi_net_vlan.h"
#include "vds/vmkapi_vds_prop.h"
#include "vds/vmkapi_vds_portset.h"

/**
 * \brief Possible capabilities for the device associated to an port.
 */

typedef enum {
   /** Scatter-gather */
   VMK_PORT_CLIENT_CAP_SG                =        0x1,

   /** Checksum offloading */
   VMK_PORT_CLIENT_CAP_IP4_CSUM          =        0x2,
   
   /** High dma */
   VMK_PORT_CLIENT_CAP_HIGH_DMA          =        0x8,

   /** TCP Segmentation Offload */
   VMK_PORT_CLIENT_CAP_TSO               =       0x20,   

   /** VLAN tagging offloading on tx path */
   VMK_PORT_CLIENT_CAP_HW_TX_VLAN        =      0x100,
   
   /** VLAN tagging offloading on rx path */
   VMK_PORT_CLIENT_CAP_HW_RX_VLAN        =      0x200,
   
   /** Scatter-gather span multiple pages */
   VMK_PORT_CLIENT_CAP_SG_SPAN_PAGES     =    0x40000,

   /** checksum IPv6 **/
   VMK_PORT_CLIENT_CAP_IP6_CSUM          =    0x80000,

   /** TSO for IPv6 **/
   VMK_PORT_CLIENT_CAP_TSO6              =   0x100000,

   /** TSO size up to 256kB **/
   VMK_PORT_CLIENT_CAP_TSO256k           =   0x200000,

   /** Uniform Passthru, only configurable by uplink device if supported.
       Uplink port shall not explicitly activate/de-activate it */
   VMK_PORT_CLIENT_CAP_UPT               =   0x400000,

   /** Port client modifies the inet headers for any reason */
   VMK_PORT_CLIENT_CAP_RDONLY_INETHDRS   =   0x800000,

   /**
    * Network Plugin Architecture (Passthru), only configurable by
    * uplink port client if supported.  Uplink port shall not explicitly
    * activate/de-activate it
    */
   VMK_PORT_CLIENT_CAP_NPA               =  0x1000000,

   /**
    * Data Center Bridging, only configurable by uplink port client if
    * supported. Uplink port shall not explicitly activate/de-activate it
    */
   VMK_PORT_CLIENT_CAP_DCB               =  0x2000000,

   /** TSO/Csum Offloads can be "offset" with an 8 bit value */
   VMK_PORT_CLIENT_CAP_OFFLOAD_8OFFSET   =  0x4000000,

   /** TSO/Csum Offloads can be "offset" with a 16 bit value */
   VMK_PORT_CLIENT_CAP_OFFLOAD_16OFFSET  =  0x8000000,

   /** CSUM for IPV6 with extension headers */
   VMK_PORT_CLIENT_CAP_IP6_CSUM_EXT_HDRS = 0x10000000,

   /** TSO for IPV6 with extension headers */
   VMK_PORT_CLIENT_CAP_TSO6_EXT_HDRS     = 0x20000000,

   /** Network scheduling capable which basically means
       capable of interrupt steering and interrupt coalescing control. */
   VMK_PORT_CLIENT_CAP_SCHED             = 0x40000000,
} vmk_PortClientCaps;

/**
 * \brief State of the device's link associated to a port.
 */

typedef enum {

   /** The device's link state is down */
   VMK_LINK_STATE_DOWN,

   /** The device's link state is up */
   VMK_LINK_STATE_UP
} vmk_LinkState;

/**
 * \brief State of the device associated to a port.
 */

typedef vmk_uint64 vmk_PortClientStates;

typedef enum {

   /** The device associated to a port is present */
   VMK_PORT_CLIENT_STATE_PRESENT     = 0x1,

   /** The device associated to a port is ready */
   VMK_PORT_CLIENT_STATE_READY       = 0x2,

   /** The device associated to a port is running */
   VMK_PORT_CLIENT_STATE_RUNNING     = 0x4,

   /** The device's queue associated to a port is operational */
   VMK_PORT_CLIENT_STATE_QUEUE_OK    = 0x8,

   /** The device associated to a port is linked */
   VMK_PORT_CLIENT_STATE_LINK_OK     = 0x10,

   /** The device associated to a port is in promiscious mode */
   VMK_PORT_CLIENT_STATE_PROMISC     = 0x20,

   /** The device associated to a port supports broadcast packets */
   VMK_PORT_CLIENT_STATE_BROADCAST   = 0x40,
   
   /** The device associated to a port supports multicast packets */
   VMK_PORT_CLIENT_STATE_MULTICAST   = 0x80
} vmk_PortClientState;

/**
 * \brief Event ids of port events
 */
typedef enum vmk_PortEvent {

   /** None */
    VMK_PORT_EVENT_NONE = 0,

   /** MAC address of a port changes  */
    VMK_PORT_EVENT_MAC_CHANGE = 1,

   /** VLAN ID of a port changes  */
    VMK_PORT_EVENT_VLAN_CHANGE = 2,

   /** MTU of a port changes  */
    VMK_PORT_EVENT_MTU_CHANGE = 3,

   /**
    * An Opaque port property has changed.
    *
    * Opaque property is partner defined port property defined in partner
    * solution. For defining opaque port properties, see
    * vmk_VDSClientPortRegister function documentation.
    */
   VMK_PORT_EVENT_OPAQUE_CHANGE = 4,

} vmk_PortEvent;


/**
 * \brief Number of bytes for port private stats string.
 */
#define VMK_PORT_PRIVATE_STATS_MAX    4096

/**
 * \brief Structure containing statistics of the device associated to a port.
 */

typedef struct {

   /** The number of rx packets received by the driver */
   vmk_uint64 rxPkt;

   /** The number of tx packets sent by the driver */
   vmk_uint64 txPkt;

   /** The number of rx bytes by the driver */
   vmk_ByteCount rxBytes;

   /** The number of tx bytes by the driver */
   vmk_ByteCount txBytes;

   /** The number of rx packets with errors */
   vmk_uint64 rxErr;

   /** The number of tx packets with errors */
   vmk_uint64 txErr;

   /** The number of rx packets dropped */
   vmk_uint64 rxDrp;

   /** The number of tx packets dropped */
   vmk_uint64 txDrp;

   /** The number of rx multicast packets */
   vmk_uint64 mltCast;

   /** The number of collisions */
   vmk_uint64 col;

   /** The number of rx length errors */
   vmk_uint64 rxLgtErr;

   /** The number of rx ring buffer overflow */
   vmk_uint64 rxOvErr;

   /** The number of rx packets with crc errors */
   vmk_uint64 rxCrcErr;

   /** The number of rx packets with frame alignment error */
   vmk_uint64 rxFrmErr;

   /** The number of rx fifo overrun */
   vmk_uint64 rxFifoErr;

   /** The number of rx packets missed */
   vmk_uint64 rxMissErr;

   /** The number of tx aborted errors */
   vmk_uint64 txAbortErr;

   /** The number of tx carriers errors */
   vmk_uint64 txCarErr;

   /** The number of tx fifo errors */
   vmk_uint64 txFifoErr;
   
   /** The number of tx heartbeat errors */
   vmk_uint64 txHeartErr;

   /** The number of tx windows errors */
   vmk_uint64 txWinErr;

   /** The number of rx packets received by the interface hosting the driver */
   vmk_uint64 intRxPkt;

   /** The number of tx packets sent by the interface hosting the driver */
   vmk_uint64 intTxPkt;

   /** The number of rx packets dropped by the interface hosting the driver */
   vmk_uint64 intRxDrp;

   /** The number of tx packets dropped by the interface hosting the driver  */
   vmk_uint64 intTxDrp;

   /**
    * String used to store the information specific the device associated
    * to a port
    */
   char privateStats[VMK_PORT_PRIVATE_STATS_MAX];
} vmk_PortClientStats;


/**
 * \brief Link speed in Mb/s.
 */

typedef vmk_uint32 vmk_LinkSpeed;

#define VMK_LINK_SPEED_AUTO ((vmk_LinkSpeed) 0)


/**
 * \brief Link duplex setting.
 */

typedef enum {

   /** Duplex autonegotiation */
   VMK_LINK_DUPLEX_AUTO = -1,

   /** Half duplex */
   VMK_LINK_DUPLEX_HALF =  1,

   /** Full duplex */
   VMK_LINK_DUPLEX_FULL =  2,
} vmk_LinkDuplex;


/**
 * \brief Link status (state, speed & duplex).
 */
typedef struct {

   /** State of the link */
   vmk_LinkState state;

   /** Speed of the link */
   vmk_LinkSpeed       speed;

   /** Duplex of the link */
   vmk_LinkDuplex      duplex;
} vmk_LinkStatus;


/*
 ***********************************************************************
 * vmk_PortSetLinkStatus --                                       */ /**
 *
 * \brief Set link status of a switch port.
 *
 * The link status includes link state, speed, and duplex.
 *
 * \note As this function will asynchronously call down to the device,
 *       the device state may not have changed yet when this API
 *       returns and vmk_PortGetLinkStatus may return the
 *       non-updated state.
 *
 * \note The calling thread must hold a mutable handle for the portset
 *       associated with the specified portID.
 *
 * \note This API currently only supports ports connected to uplinks
 *
 * \note Link status cannot be set for hidden uplinks.
 *
 * \note This function will not block.
 *
 * \param[in]    portID             Numeric ID of a virtual port
 * \param[in]    linkStatus         Status of the link
 *
 * \retval       VMK_OK             The status was successfully set.
 * \retval       VMK_NOT_FOUND      The device cannot be found or
 *                                  portID is invalid
 * \retval       VMK_NOT_SUPPORTED  The speed/duplex is not supported.
 * \retval       VMK_BAD_PARAM      The speed/duplex is not a standard
 *                                  value.
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortSetLinkStatus(vmk_SwitchPortID portID,
                                       vmk_LinkStatus *linkStatus);

/*
 ***********************************************************************
 * vmk_PortGetLinkStatus --                                       */ /**
 *
 * \brief Get link status of a switch port.
 *
 * The link status includes link state, speed, and duplex.
 *
 * \see vmk_PortSetLinkStatus().
 *
 * \note A link status change might be pending.
 *
 * \note This API currently only supports ports connected to uplinks
 *
 * \note The calling thread must hold a mutable handle for the portset
 *       associated with the specified portID.
 *
 * \note This function will not block.
 *
 * \param[in]    portID         Numeric ID of a virtual port
 * \param[out]   linkStatus     Status of the link
 *
 * \retval       VMK_OK         The status was successfully retrieved
 * \retval       VMK_NOT_FOUND  The device cannot be found or portID is
 *                              invalid
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortGetLinkStatus(vmk_SwitchPortID portID,
                                       vmk_LinkStatus *linkStatus);


/*
***********************************************************************
* vmk_PortUpdateDevCap --                                        */ /**
*
* \brief Request or release capabilities on a switch port.
*
* For capabilities that are allowed to be updated, refer to definition
* of vmk_PortCapability.
*
* Some pNIC hardware only support a subset of capabilities. When
* setting a capability that hardware doesn't support, vmkernel will
* insert software routine to do the packet processing. 
*
* \see vmk_PortCapability().
*
* \note This API currently only supports ports connected to uplinks
*
* \note The calling thread must hold a mutable handle for the portset
*       associated with the specified portID.
*
* \note This function will not block.
*
* \param[in]    portID         Numeric ID of a virtual port
* \param[in]    cap            Pointer to new capabilities. Logical
*                              OR of vmk_PortCapability enum flag
* \param[out]   resCap         Pointer to result capabilities. Logical
*                              OR of vmk_PortCapability enum flag
*
* \retval       VMK_OK         If capabilities update is successful
* \retval       VMK_NOT_FOUND  If device cannot be found or portID is
*                              invalid
* \retval       VMK_FAILURE    Failed to update device cap
***********************************************************************
*/


VMK_ReturnStatus vmk_PortUpdateDevCap(vmk_SwitchPortID portID,
                                      vmk_uint32 cap,
                                      vmk_uint32 *resCap);

/*
 ***********************************************************************
 * vmk_PortQueryDevCap --                                   */ /** 
 *
 * \brief Get the enabled software capabilities on a switch port.
 *
 * The function returns all capabilities enabled, whether implemented
 * in software or in hardware.
 *
 * \note This API currently only supports ports connected to uplinks
 *
 * \note The calling thread must hold a mutable handle for the portset
 *       associated with the specified portID.
 *
 * \note This function will not block.
 *
 * \param[in]    portID         Numeric ID of a virtual port
 * \param[out]   cap            Pointer to result of capabilities. Logic
 *                              OR of vmk_PortCapability enum flag
 *
 * \retval       VMK_OK         If capability query is successful
 * \retval       VMK_NOT_FOUND  If device cannot be found or portID is
 *                              invalid
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortQueryDevCap(vmk_SwitchPortID portID,
                                     vmk_uint32 *cap);

/*
 ***********************************************************************
 * vmk_PortGetHwCapSupported --
 *
 * \brief Get hardware capabilities supported.
 *
 * Retrieve hardware capabilities supported by the device connected to
 * a switch port.
 *
 * \note This API currently only supports ports connected to uplinks
 *
 * \note The calling thread must hold a mutable handle for the portset
 *       associated with the specified portID.
 *
 * \note This function will not block.
 *
 * \param[in]    portID         Numeric ID of a virtual port
 * \param[out]   cap            Pointer to result of capabilities. Logic
 *                              OR of vmk_PortCapability enum flag
 *
 * \retval       VMK_OK         Get operation successfully finished
 * \retval       VMK_NOT_FOUND  If device is not found or portID is
 *                              invalid
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortGetHwCapSupported(vmk_SwitchPortID portID,
                                           vmk_uint32 *cap);

/*
 ***********************************************************************
 * vmk_PortUpdateVlan --                                    */ /**
 *
 * \brief Update vlan IDs on the device connected to a switch port.
 *
 * This function updates the set of vlan IDs the device will process.
 * The bitmap, if not NULL, marks permitted vlan IDs on the port.
 * Packets with non-permitted vlan IDs may be filtered by device.
 *
 * Caller must perform two steps to configure VLAN:
 * 1) call vmk_PortUpdateDevCap to set hw or sw vlan capability
 * 2) call vmk_PortUpdateVlan to update vlan bitmap
 * If caller does not first set vlan capabilities on port, this function
 * will return VMK_NOT_READY.
 *
 * \note This API currently only supports ports connected to uplinks
 *
 * \note Some hardware devices do not implement vlan filtering on
 *       packet receive. The switch implementation must be prepared to
 *       receive packets with non-permitted vlan IDs.
 *
 * \note The calling thread must hold a mutable handle for the portset
 *       associated with the specified portID.
 *
 * \note This function will not block.
 *
 * \param[in]    portID         Numeric ID of a virtual port
 * \param[in]    bitmap         Permitted vlan IDs. 
 *
 * \retval       VMK_OK         Update successfully finished
 * \retval       VMK_NOT_FOUND  If device is not found or portID is
 *                              invalid
 * \retval       VMK_FAILURE    Failed to set vlan bitmap
 * \retval       VMK_NOT_READY  The port has not configured vlan
 *                              capabilities
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortUpdateVlan(vmk_SwitchPortID portID,
                                    vmk_VLANBitmap *bitmap);


/*
 ***********************************************************************
 * vmk_PortGetStates --                                     */ /**
 *
 * \brief Fetch device state of the port. 
 *
 * \note This API currently only supports ports connected to uplinks
 *
 * \note The calling thread must not hold any locks or portset handles.
 *
 * \note This function may block.
 *
 * \param[in]    portID         Numeric ID of a virtual port
 * \param[out]   states         Logical OR of vmk_UplinkState enum flag 
 *
 * \retval       VMK_OK         Get operation successfully finished
 * \retval       VMK_NOT_FOUND  If device is not found or portID is
 *                              invalid
 * \retval       VMK_FAILURE    Failed to get states
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortGetStates(vmk_SwitchPortID portID,
                                   vmk_PortClientStates *states);

/*
 ***********************************************************************
 * vmk_PortSetMTU --                                              */ /**
 *
 * \brief Change device MTU size on the port.
 *
 * \note This API currently only supports ports connected to uplinks
 *
 * \note The calling thread must not hold any locks or portset handles.
 *
 * \note This function may block.
 *
 * \param[in]    portID         Numeric ID of a virtual port
 * \param[in]    mtu            New MTU size 
 *
 * \retval       VMK_OK         Set operation successfully finished
 * \retval       VMK_FAILURE    Set operation failed
 * \retval       VMK_NOT_FOUND  Cannot find device or portID is invalid
 * \retval       VMK_BAD_PARAM  Mtu is bigger than max jumbo frame size
 *                              that vmkernel supports
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortSetMTU(vmk_SwitchPortID portID,
                                vmk_uint32 mtu);

/*
 ***********************************************************************
 * vmk_PortGetMTU --                                              */ /**
 *
 * \brief Get device MTU size on the port.
 *
 * \note This API currently only supports ports connected to uplinks
 *
 * \note The calling thread must hold a mutable handle for the portset
 *       associated with the specified uplink.
 *
 * \note This function will not block.
 *
 * \param[in]    portID         Numeric ID of a virtual port
 * \param[in]    mtu            Current device MTU size 
 *
 * \retval       VMK_OK         Get operation successfully finished
 * \retval       VMK_NOT_FOUND  Device cannot be found or portID is
 *                              invalid
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortGetMTU(vmk_SwitchPortID portID,
                                vmk_uint32 *mtu);


/*
 ***********************************************************************
 * vmk_PortGetDevName --                                          */ /**
 *                                                                        
 * \brief Get name of the device connected to the switch port.
 *
 * \note A buffer of VMK_DEVICE_NAME_MAX_LENGTH or larger ensures that
 *       the name will fit.
 *
 * \note This API currently only supports ports connected to uplinks.
 *
 * \note The calling thread must hold a mutable handle for the portset
 *       associated with the specified portID.
 *
 * \note This function will not block.
 *
 * \param[in]    portID        Numeric ID of a virtual port
 * \param[out]   devName       Device name string pointer.
 * \param[in]    devNameLen    Size of buffer for device name string
 *                             pointer.
 *
 * \retval       VMK_OK              If device name is successfully
 *                                   retrieved.
 * \retval       VMK_NOT_FOUND       If device is not found or portID
 *                                   is invalid
 * \retval       VMK_LIMIT_EXCEEDED  If device name is too long for
 *                                   provided buffer.
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortGetDevName(vmk_SwitchPortID portID,
                                    char *devName,
                                    vmk_ByteCount devNameLen);

/*
 ***********************************************************************
 * vmk_PortGetVirtualMACAddr --                                   */ /**
 *
 * \brief Get virtual MAC address of the port.
 *
 * \note  This virtual MAC address may differ from device's physical
 *        MAC address.
 *
 * \note This API currently only supports ports connected to uplinks
 *
 * \note The calling thread must hold a mutable handle for the portset
 *       associated with the specified portID.
 *
 * \note This function will not block.
 *
 * \param[in]    portID         Numeric ID of a virtual port
 * \param[out]   macAddr        MAC address of device 
 *
 * \retval       VMK_OK         Get operation successfully finished
 * \retval       VMK_NOT_FOUND  Device is not found or portID is invalid
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortGetVirtualMACAddr(vmk_SwitchPortID portID,
                                           vmk_EthAddress macAddr);

/*
 ***********************************************************************
 * vmk_PortGetDevStats --                                         */ /**
 *
 * \brief Get device statistics on the port.
 *
 * The stats on pNICs are maintained by device drivers, and
 * some drivers only update a subset of stats. Therefore, there is no
 * guarantee that all counters returned are non-zero.
 *
 * Caller must initiate privateStats field in stats data structure
 * before calling this function. Caller must not hold portset lock
 * before calling this function.
 *
 * \note This API currently only supports ports connected to uplinks
 *
 * \note The calling thread must not hold any locks or portset handles.
 *
 * \note This function may block.
 *
 * \param[in]    portID         Numeric ID of a virtual port
 * \param[out]   stats          Device stats 
 *
 * \retval       VMK_OK         Successfully get stats 
 * \retval       VMK_NOT_FOUND  Device is not found or portID is invalid
 * \retval       VMK_FAILURE    Failed to get stats 
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortGetDevStats(vmk_SwitchPortID portID,
                                     vmk_PortClientStats *stats);

/*
 ***********************************************************************
 * vmk_PortApplyPortFilter --                                     */ /**
 *
 * \brief Apply a port filter to a port.
 *
 *  An advertised filter will become part of the rx load balancing
 *  algorithm running in the uplink layer. The nominated "top
 *  performers" filters will be given one of the populated uplink
 *  hardware queues. Such queues are not dedicated to a unique filter
 *  in the case that the uplink can handle multiple filters.
 *
 *  Note that an advertised filter with VMK_NETQUEUE_FILTER_PROP_MGMT
 *  is considered "top performer" by default. This means that such
 *  filters should be affiliated to a hardware queue promptly. Though,
 *  this is done on a best effort basis according to actual number of
 *  populated hardware queues.
 *
 * \note This API currently only supports ports connected to uplinks 
 *
 * \note The calling thread must hold a mutable handle for the portset
 *       associated with the specified uplink.
 *
 * \note This function will not block.
 *
 * \param[in]    portID        Numeric ID of a virtual port.
 * \param[in]    filter        Filter advertised for this port.
 *
 * \retval       VMK_OK        Filter advertised successfully. 
 * \retval       VMK_BAD_PARAM Invalid filter (invalid class, properties).
 * \retval       VMK_FAILURE   Filter has not been applied.
 * \retval       VMK_NOT_FOUND Device cannot be found or portID is
 *                             invalid
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortApplyPortFilter(vmk_SwitchPortID portID,
                                         vmk_PortFilter *filter);

/*
 ***********************************************************************
 * vmk_PortRemovePortFilter --                                  */ /**
 *
 * \brief Remove a port filter from an port.
 *
 *  The filter won't be part of the rx load balancing algorithm running in
 *  the uplink layer. If a queue was associated to this filter, it will be
 *  released for other filters.
 *
 * \note This API currently only supports ports connected to uplinks
 *
 * \note The calling thread must hold a mutable handle for the portset
 *       associated with the specified uplink.
 *
 * \note This function will not block.
 *
 * \param[in]    portID         Numeric ID of a virtual port.
 * \param[in]    filter         Filter advertised for this port.
 *
 * \retval       VMK_OK         Filter removed successfully.
 * \retval       VMK_BAD_PARAM  Invalid filter (invalid class, properties).
 * \retval       VMK_NOT_FOUND  Filter not found or portID is invalid or
 *                              device cannot be found
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortRemovePortFilter(vmk_SwitchPortID portID,
                                          vmk_PortFilter *filter);


/*
 ***********************************************************************
 * vmk_PortNotifyChange --                                 */ /**
 *
 * \brief Notify the change of a port property.
 *
 * This function generates an event to notify management software that
 * the value of a property has been modified by the switch implementation.
 * It is not required that all runtime property updates need accompanying
 * events, the guidelines to generate events should be -
 * 1) If there is a user visible screen for the property and the management
 *    software does not poll the property value.
 * 2) If there is some reason that the state needs to be persisted right away.
 *
 * For example, the UI already poll for statistics, so there is no reason
 * to send an event every time a statistics value changes.
 *
 * \note The calling thread must hold a mutable handle for the portset
 *       associated with the specified port.
 * 
 * \note This function will not block.
 *
 * \param[in]  portID          Numeric ID of the virtual port
 *
 * \param[in]  eventType       Type of event
 *
 * \retval  VMK_OK                          Event notified successfully.
 * \retval  VMK_NOT_FOUND                   Portset cannot be found.
 * \retval  VMK_NO_MEMORY                   Could not allocate memory.
 * \retval  VMK_NOT_SUPPORTED               Event type not supported.
 * \retval  VMK_BAD_PARAM                   Portset is not in VDS or
 *                                          port is not associated to
 *                                          DVS
 * \retval  VMK_PORTSET_HANDLE_NOT_MUTABLE  The caller did not hold a
 *                                          mutable handle.
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortNotifyChange (vmk_SwitchPortID portID,
                                       vmk_uint32 eventType);

#endif
/** @} */
/** @} */
