/***************************************************************************
 * Copyright 2014 VMware, Inc.  All rights reserved.
 ***************************************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Port                                                           */ /**
 *
 * \addtogroup Network
 * @{
 * \addtogroup Networking Port APIs
 * @{
 * \defgroup Deprecated Networking Port APIs
 * @{
 ***********************************************************************
 */


#ifndef _VMKAPI_NET_PORT_DEPRECATED_H_
#define _VMKAPI_NET_PORT_DEPRECATED_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "net/vmkapi_net_dcb.h"
#include "net/vmkapi_net_types.h"
#include "net/vmkapi_net_eth.h"
#include "net/vmkapi_net_pt.h"
#include "net/vmkapi_net_queue.h"
#include "net/vmkapi_net_vlan.h"
#include "vds/vmkapi_vds_prop.h"


/**
 * Port MAC address filter information
 *
 * \deprecated This definition will be removed in a future release in favor
 *             of vmk_UplinkQueueMACFilterInfo.
 */
typedef vmk_UplinkQueueMACFilterInfo vmk_PortMACFilterInfo;

/**
 * Port VLAN tag filter information
 *
 * \deprecated This definition will be removed in a future release in favor
 *             of vmk_UplinkQueueVLANFilterInfo.
 */
typedef vmk_UplinkQueueVLANFilterInfo vmk_PortVLANFilterInfo;

/**
 * Port VLAN tag + MAC filter information
 *
 * \deprecated This definition will be removed in a future release in favor
 *             of vmk_UplinkQueueVLANMACFilterInfo.
 */
typedef vmk_UplinkQueueVLANMACFilterInfo vmk_PortVLANMACFilterInfo;

/**
 * Port VXLAN filter information
 *
 * \deprecated This definition will be removed in a future release in favor
 *             of vmk_UplinkQueueVXLANFilterInfo.
 */
typedef vmk_UplinkQueueVXLANFilterInfo vmk_PortVXLANFilterInfo;

/**
 * \brief Port filter class.
 *
 * \deprecated This definition will be removed in a future release in favor
 *             of vmk_UplinkQueueFilterClass.
 *
 *  This class allows 3rd party module to interpret properly
 *  the filter being passed.
 */
typedef enum vmk_PortFilterClass {

   /** Invalid filter */
   VMK_PORT_FILTER_NONE            = 0x0,

   /** Mac address filter */
   VMK_PORT_FILTER_MACADDR         = 0x1,

   /** Vlan tag filter */
   VMK_PORT_FILTER_VLAN            = 0x2,

   /** Vlan tag + mac addr filter */
   VMK_PORT_FILTER_VLANMACADDR     = 0x3,

   /** VXLAN filter */
   VMK_PORT_FILTER_VXLAN           = 0x4,
} vmk_PortFilterClass;

/**
 * \brief Filter properties.
 *
 * \deprecated This definition will be removed in a future release in favor
 *             of vmk_UplinkQueueFilterProperties.
 *
 *  These properties are used to classify
 *  a filter. They can be interpreted differently
 *  according to the place they are involved with.
 *  To know more about the way each property is
 *  handled, refer to the used API for example
 *  vmk_PortApplyPortFilter() if a filter
 *  is to be advertised on an uplink.
 */
typedef enum vmk_PortFilterProperties {

   /** None */
   VMK_PORT_FILTER_PROP_NONE = 0x0,

   /** Management filter */
   VMK_PORT_FILTER_PROP_MGMT = 0x1,
} vmk_PortFilterProperties;

/**
 * \brief Filter information advertised by a port.
 *
 * \deprecated This definition will be removed in a future release in favor
 *             of vmk_UplinkQueueFilter.
 *
 *  A port filter is used by portset modules for traffics filtering
 *  purposes. One instance is the load balancer algorithm running in
 *  the uplink layer (refer to vmk_PortApplyPortFilter()).
 */
typedef struct vmk_PortFilter {

   /** Port advertising this filter */
   vmk_SwitchPortID portID;

   /** Filter class */
   vmk_PortFilterClass class;

   /**
    * \brief Filter info
    *
    * Filter information is cast to following structures according to
    * class value:
    *
    *    class:                        filter:
    *    VMK_PORT_FILTER_MACADDR       vmk_PortMACFilterInfo
    *    VMK_PORT_FILTER_VLAN          vmk_PortVLANFilterInfo
    *    VMK_PORT_FILTER_VLANMACADDR   vmk_PortVLANMACFilterInfo
    *    VMK_PORT_FILTER_VXLAN         vmk_PortVXLANFilterInfo
    */
   void *filter;

   /** Filter properties */
   vmk_PortFilterProperties prop;
} vmk_PortFilter;

/*
 ***********************************************************************
 * vmk_PortApplyPortFilter --                                     */ /**
 *
 * \brief Apply a port filter to a port.
 *
 * \deprecated This function will be removed in a future release. Use
 *             vmk_UplinkQueueApplyPortFilter() to apply a port filter
 *             on a uplink.
 *
 * \nativedriversdisallowed
 *
 * An advertised filter will become part of the rx load balancing
 * algorithm running in the uplink layer. The nominated "top
 * performers" filters will be given one of the populated uplink
 * hardware queues. Such queues are not dedicated to a unique filter
 * in the case that the uplink can handle multiple filters.
 *
 * Note that an advertised filter with VMK_NETQUEUE_FILTER_PROP_MGMT
 * is considered "top performer" by default. This means that such
 * filters should be affiliated to a hardware queue promptly. Though,
 * this is done on a best effort basis according to actual number of
 * populated hardware queues.
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
 * \retval       VMK_BAD_PARAM Invalid class or properties in filter.
 * \retval       VMK_FAILURE   Filter has not been applied.
 * \retval       VMK_NOT_FOUND Device cannot be found or portID is
 *                             invalid
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortApplyPortFilter(vmk_SwitchPortID portID,
                                         vmk_PortFilter *filter);

/*
 ***********************************************************************
 * vmk_PortRemovePortFilter --                                    */ /**
 *
 * \brief Remove a port filter from an port.
 *
 * \deprecated This function will be removed in a future release. Use
 *             vmk_UplinkQueueRemovePortFilter() to remove a port filter
 *             on a uplink.
 *
 * \nativedriversdisallowed
 *
 * The filter won't be part of the rx load balancing algorithm running
 * in the uplink layer. If a queue was associated to this filter, it
 * will be released for other filters.
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
 * \retval       VMK_BAD_PARAM  Invalid class or properties in filter.
 * \retval       VMK_NOT_FOUND  Filter not found or portID is invalid or
 *                              device cannot be found
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortRemovePortFilter(vmk_SwitchPortID portID,
                                          vmk_PortFilter *filter);



/*
 ***********************************************************************
 * vmk_PortUpdateDevCap --                                        */ /**
 *
 * \brief Request or release capabilities on a switch port.
 *
 * \deprecated This function will be removed in a future release. Use
 *             vmk_UplinkServiceRequest() or vmk_UplinkServiceRelease()
 *             to request or release uplink capabilities.
 *
 * \nativedriversdisallowed
 *
 * For capabilities that are allowed to be updated, refer to definition
 * of vmk_PortCapability.
 *
 * Some pNIC hardware only support a subset of capabilities. When
 * setting a capability that hardware doesn't support, vmkernel will
 * insert software routine to do the packet processing.
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
                                      vmk_uint64 cap,
                                      vmk_uint64 *resCap);

/*
 ***********************************************************************
 * vmk_PortQueryDevCap --                                         */ /**
 *
 * \brief Get the enabled software capabilities on a switch port.
 *
 * \deprecated This function will be removed in a future release. Use
 *             vmk_UplinkServiceIsActive() to query uplink capabilities.
 *
 * \nativedriversdisallowed
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
                                     vmk_uint64 *cap);

/*
 ***********************************************************************
 * vmk_PortGetHwCapSupported --                                   */ /**
 *
 * \brief Get hardware capabilities supported.
 *
 * \deprecated This function will be removed in a future release. Use
 *             vmk_UplinkCapIsRegistered() to get uplink hardware
 *             supported capabilities.
 *
 * \nativedriversdisallowed
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
                                           vmk_uint64 *cap);


/*
 ***********************************************************************
 * vmk_PortGetStates --                                           */ /**
 *
 * \brief Fetch device state of the port.
 *
 * \deprecated This function will be removed in a future release. Use
 *             vmk_UplinkStateGet() to fetech uplink state.
 *
 * \nativedriversdisallowed
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

/**
 * \brief Structure containing statistics of the device associated to a port.
 *
 * \deprecated This definition will be removed in a future release in favor
 *             of vmk_UplinkStats.
 */

typedef struct vmk_PortClientStats{

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
   char *privateStats;

   /** The length of privateStats in bytes */
   vmk_ByteCount privateStatsLength;
} vmk_PortClientStats;

/*
 ***********************************************************************
 * vmk_PortGetDevStats --                                         */ /**
 *
 * \brief Get device statistics on the port.
 *
 * \deprecated This function will be removed in a future release. Use
 *             vmk_UplinkStatsGet() and vmk_UplinkPrivStatsGet() to
 *             fetech uplink stats and private stats.
 *
 * \nativedriversdisallowed
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
 * vmk_PortGetDevName --                                          */ /**
 *
 * \brief Get name of the device connected to the switch port.
 *
 * \deprecated This function will be removed in a future release. Use
 *             vmk_UplinkNameGet() to fetech uplink name.
 *
 * \nativedriversdisallowed
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
 * vmk_PortSetLinkStatus --                                       */ /**
 *
 * \brief Set link status of a switch port.
 *
 * \deprecated This function will be removed in a future release. Use
 *             vmk_UplinkUpdateLinkState() to set uplink's link status.
 *
 * \nativedriversdisallowed
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
 * \deprecated This function will be removed in a future release. Use
 *             vmk_UplinkLinkStatusGet() to fetech uplink link status.
 *
 * \nativedriversdisallowed
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
 * vmk_PortUpdateVlan --                                          */ /**
 *
 * \brief Update vlan IDs on the device connected to a switch port.
 *
 * \deprecated This function will be removed in a future release. Use
 *             vmk_UplinkVLANFilterBitmapSet() to update VLAN IDs on
 *             a uplink.
 *
 * \nativedriversdisallowed
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
 * vmk_PortSetMTU --                                              */ /**
 *
 * \brief Change device MTU size on the port.
 *
 * \deprecated This function will be removed in a future release. Use
 *             vmk_UplinkMTUSet() to change uplink MTU.
 *
 * \nativedriversdisallowed
 *
 * \note This API currently only supports ports connected to uplinks
 *
 * \note The calling thread must not hold any locks or portset handles.
 *
 * \note For uplink ports, the actual MTU might be set to a higher value
 *       if the requested size is too small.
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
 * \deprecated This function will be removed in a future release. Use
 *             vmk_UplinkMTUGet() to fetech uplink MTU.
 *
 * \nativedriversdisallowed
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
 * vmk_PortGetVirtualMACAddr --                                   */ /**
 *
 * \brief Get virtual MAC address of the port.
 *
 * \deprecated This function will be removed in a future release. Use
 *             vmk_UplinkVirtualMACAddrGet() to fetech uplink's virtual
 *             MAC address.
 *
 * \nativedriversdisallowed
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
 * vmk_PortDCBIsDCBEnabled --                                     */ /**
 *
 * \brief  Check DCB support on the device connected to a vDS port.
 *
 * \deprecated This function will be demoted to binary incompatible then
 *             removed in future release.
 *
 * \nativedriversdisallowed
 *
 * \note This function may block. No locks should be held while calling
 *       this function.
 *
 * \note Currently only supported on uplink port.
 *
 * \param[in]  portID      Numeric ID of a virtual port.
 * \param[out] enabled     Used to store the DCB state of the device.
 * \param[out] version     Used to store the DCB version supported by
 *                         the device.
 *
 * \retval     VMK_OK             If the call completes successfully.
 * \retval     VMK_NOT_SUPPORTED  If the device is not DCB capable or if
 *                                the driver doesn't support the
 *                                operation.
 * \retval     VMK_NOT_FOUND      If the port is not an uplink port, or
 *                                if the uplink is not found or not
 *                                opened.
 * \retval     VMK_FAILURE        If the call fails.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortDCBIsDCBEnabled(vmk_SwitchPortID portID,
                                         vmk_Bool *enabled,
                                         vmk_DCBVersion *version);

/*
 ***********************************************************************
 * vmk_PortDCBGetNumTCs --                                        */ /**
 *
 * \brief  Get Traffic Classes from the device connected to a vDS port.
 *
 * \deprecated This function will be demoted to binary incompatible then
 *             removed in future release.
 *
 * \nativedriversdisallowed
 *
 * \note This function may block. No locks should be held while calling
 *       this function.
 *
 * \note Currently only supported on uplink port.
 *
 * \param[in]  portID      Numeric ID of a virtual port.
 * \param[out] numTCs      Used to store the Traffic Class information.
 *
 * \retval     VMK_OK             If the call completes successfully.
 * \retval     VMK_NOT_SUPPORTED  If the device is not DCB capable or if
 *                                the driver doesn't support the
 *                                operation.
 * \retval     VMK_NOT_FOUND      If the port is not an uplink port, or
 *                                if the uplink is not found or not
 *                                opened.
 * \retval     VMK_FAILURE        If the call fails.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortDCBGetNumTCs(vmk_SwitchPortID portID,
                                      vmk_DCBNumTCs *numTCs);

/*
 ***********************************************************************
 * vmk_PortDCBGetPriorityGroup --                                 */ /**
 *
 * \brief  Get DCB Priority Group from the device connected to vDS port.
 *
 * \deprecated This function will be demoted to binary incompatible then
 *             removed in future release.
 *
 * \nativedriversdisallowed
 *
 * \note This function may block. No locks should be held while calling
 *       this function.
 * \note Currently only supported on uplink port.
 *
 * \param[in]  portID      Numeric ID of a virtual port.
 * \param[out] pg          Used to stored the current PG setting.
 *
 * \retval     VMK_OK             If the call completes successfully.
 * \retval     VMK_NOT_SUPPORTED  If the device is not DCB capable or if
 *                                the driver doesn't support the
 *                                operation.
 * \retval     VMK_NOT_FOUND      If the port is not an uplink port, or
 *                                if the uplink is not found or not
 *                                opened.
 * \retval     VMK_FAILURE        If the call fails.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortDCBGetPriorityGroup(vmk_SwitchPortID portID,
                                             vmk_DCBPriorityGroup *pg);

/*
 ***********************************************************************
 * vmk_PortDCBGetPFCCfg --                                        */ /**
 *
 * \brief  Retrieve Priority-based Flow Control configurations.
 *
 * \deprecated This function will be demoted to binary incompatible then
 *             removed in future release.
 *
 * \nativedriversdisallowed
 *
 * Get Priority-based Flow Control configurations from the device
 * connected to a vDS port.
 *
 * \note  vmk_PortDCBIsPFCEnabled should be call first to check if
 *        PFC support is enabled on the device before calling this
 *        function.
 * \note This function may block. No locks should be held while calling
 *       this function.
 * \note Currently only supported on uplink port.
 *
 * \param[in]  portID      Numeric ID of a virtual port.
 * \param[out] pfcCfg      Used to stored the current PFC configuration.
 *
 * \retval     VMK_OK             If the call completes successfully.
 * \retval     VMK_NOT_SUPPORTED  If the device is not DCB capable or if
 *                                the driver doesn't support the
 *                                operation.
 * \retval     VMK_NOT_FOUND      If the port is not an uplink port, or
 *                                if the uplink is not found or not
 *                                opened.
 * \retval     VMK_FAILURE        If the call fails.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortDCBGetPFCCfg(vmk_SwitchPortID portID,
                                      vmk_DCBPriorityFlowControlCfg *pfcCfg);

/*
 ***********************************************************************
 * vmk_PortDCBIsPFCEnabled --                                     */ /**
 *
 * \brief  Check if Priority-based Flow Control support is enabled.
 *
 * \deprecated This function will be demoted to binary incompatible then
 *             removed in future release.
 *
 * \nativedriversdisallowed
 *
 * \note This function may block. No locks should be held while calling
 *       this function.
 *
 * \note Currently only supported on uplink port.
 *
 * \param[in]  portID      Numeric ID of a virtual port.
 * \param[out] enabled     Used to stored the current PFC support state.
 *
 * \retval     VMK_OK             If the call completes successfully.
 * \retval     VMK_NOT_SUPPORTED  If the device is not DCB capable or if
 *                                the driver doesn't support the
 *                                operation.
 * \retval     VMK_NOT_FOUND      If the port is not an uplink port, or
 *                                if the uplink is not found or not
 *                                opened.
 * \retval     VMK_FAILURE        If the call fails.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortDCBIsPFCEnabled(vmk_SwitchPortID portID,
                                         vmk_Bool *enabled);

/*
 ***********************************************************************
 * vmk_PortDCBGetApplications --                                  */ /**
 *
 * \brief  Retrieve all DCB Application Protocols settings.
 *
 * \deprecated This function will be demoted to binary incompatible then
 *             removed in future release.
 *
 * \nativedriversdisallowed
 *
 * Get DCB Application Protocol settings from the device connected
 * to a vDS port.
 *
 * \note This function may block. No locks should be held while
 *       calling this function.
 *
 * \note Currently only supported on uplink port.
 *
 * \param[in]  portID      Numeric ID of a virtual port.
 * \param[out] apps        Used to store the DCB Applications
 *                         settings of the device.
 *
 * \retval     VMK_OK             If the call completes successfully.
 * \retval     VMK_NOT_SUPPORTED  If the device is not DCB capable or if
 *                                the driver doesn't support the
 *                                operation.
 * \retval     VMK_NOT_FOUND      If the port is not an uplink port, or
 *                                if the uplink is not found or not
 *                                opened.
 * \retval     VMK_FAILURE        If the call fails.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortDCBGetApplications(vmk_SwitchPortID portID,
                                            vmk_DCBApplications *apps);

/*
 ***********************************************************************
 * vmk_PortDCBGetCapabilities --                                  */ /**
 *
 * \brief  Retrieve DCB capabilities information.
 *
 * \deprecated This function will be demoted to binary incompatible then
 *             removed in future release.
 *
 * \nativedriversdisallowed
 *
 * Get DCB capabilities from the device connected to a vDS port.
 *
 * \note This function may block. No locks should be held while calling
 *       this function.
 * \note Currently only supported on uplink port.
 *
 * \param[in]  portID      Numeric ID of a virtual port.
 * \param[out] caps        Used to store the DCB capabilities
 *                         information of the device.
 *
 * \retval     VMK_OK             If the call completes successfully.
 * \retval     VMK_NOT_SUPPORTED  If the device is not DCB capable or if
 *                                the driver doesn't support the
 *                                operation.
 * \retval     VMK_NOT_FOUND      If the port is not an uplink port, or
 *                                if the uplink is not found or not
 *                                opened.
 * \retval     VMK_FAILURE        If the call fails.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortDCBGetCapabilities(vmk_SwitchPortID portID,
                                            vmk_DCBCapabilities *caps);


#endif /* _VMKAPI_NET_PORT_DEPRECATED_H_ */
/** @} */
/** @} */
/** @} */
