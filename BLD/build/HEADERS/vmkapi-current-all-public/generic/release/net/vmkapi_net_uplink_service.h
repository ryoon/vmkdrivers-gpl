/* **********************************************************
 * Copyright 2014 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Uplink Services                                                */ /**
 * \addtogroup Network
 *@{
 * \defgroup UplinkServices Uplink Services
 *
 * In VMkernel, uplink layer provides services for port clients to
 * offload portion of their packet processing work down to lower layer,
 * by leveraging underneath NIC driver's hardware capabilities. These
 * services include TCP checksum offloading, TCP segmentation
 * offloading, VLAN tagging and untagging, etc.
 *
 * Uplink layer will add a software simulation if a service requested is
 * not supported by NIC drvier. Port clients can request uplink service
 * after uplink is connected to portset.
 *
 * Besides offloading packet processing services, uplink layer also
 * provides interfaces for port clients to program uplink's RX queue
 * filter, and VLAN filter to achieve better performance in data path.
 *
 *@{
 ***********************************************************************
 */

#ifndef _VMKAPI_NET_UPLINK_SERVICE_H_
#define _VMKAPI_NET_UPLINK_SERVICE_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "net/vmkapi_net_uplink.h"


/**
 * Definition of services supported by uplink layer.
 *
 * The actual provider of a uplink service can be either uplink layer
 * or the NIC driver underneath, depending on service type and driver's
 * hardware capabilites. Caller can request and release these services
 * calling vmk_UplinkServiceRequest() and vmk_UplinkServiceRelease().
 */
typedef enum vmk_UplinkService {
   /**
    * Scatter gather transmit service
    * \note This service allows port client to transmit packets having
    *       multiple elements in their scatter-gather array.
    */
   VMK_UPLINK_SERVICE_SG_TX = 1,

   /**
    * IPv4 checksum offloading service
    * \note This service allows port client to offload TX packets' IPv4
    *       TCP/UDP checksum calculation down to uplink layer or NIC
    *       driver. Port client must mark TX packets as "CSO needed" by
    *       calling vmk_PktSetMustCsum().
    */
   VMK_UPLINK_SERVICE_IPV4_CSO = 2,

   /**
    * Service to overcome DMA constraints
    * \note This service allows port client to transmit packets with
    *       payload allocated at any address, regardless the DMA engine
    *       constraints limited by NIC driver.
    */
   VMK_UPLINK_SERVICE_NO_DMA_CONSTRAINTS = 3,

   /**
    * IPv4 TCP segmentation offloading service
    * \note This service allows port client to offload IPv4 TCP
    *       segmentation down to uplink layer or NIC driver. Port client
    *       must mark TX packets as "TSO needed" and set proper MSS by
    *       calling vmk_PktSetLargeTcpPacket().
    */
   VMK_UPLINK_SERVICE_IPV4_TSO = 4,

   /**
    * Service to insert VLAN tag in TX packets
    * \note This service allows port client to offload TX packets' VLAN
    *       tag insertion down to uplink layer or NIC driver. Port
    *       client must mark TX packets as "Must VLAN tag" by calling
    *       vmk_PktMustVlanTag().
    */
   VMK_UPLINK_SERVICE_VLAN_TX_INSERT = 5,

   /**
    * Service to strip VLAN tag in RX packets
    * \note This service allows port client to offload the removal of
    *       RX packets' VLAN tag down to uplink layer or NIC driver.
    *       Port client can retrieve the stripped VLAN tag by calling
    *       vmk_PktVlanIDGet().
    */
   VMK_UPLINK_SERVICE_VLAN_RX_STRIP = 6,

   /**
    * Scatter gather transmit spanning multiple pages service
    * \note This service allows port client to transmit packets having
    *       scatter-gather array elements spanning across multiple
    *       pages.
    */
   VMK_UPLINK_SERVICE_MULTI_PAGE_SG = 7,

   /**
    * IPv6 checksum offloading service
    * \note This service allows port client to offload TX packets' IPv6
    *       TCP/UDP checksum calculation down to uplink layer or NIC
    *       driver. Port client must mark TX packets as "CSO needed" by
    *       calling vmk_PktSetMustCsum().
    */
   VMK_UPLINK_SERVICE_IPV6_CSO = 8,

   /**
    * IPv6 TCP segmentation offloading service
    * \note This service allows port client to offload IPv6 TCP
    *       segmentation down to uplink layer or NIC driver. Port client
    *       must mark TX packets as "TSO needed" and set proper MSS by
    *       calling vmk_PktSetLargeTcpPacket().
    */
   VMK_UPLINK_SERVICE_IPV6_TSO = 9,

   /**
    * Service to split large TSO packet
    * \note This service allows port client to transmit TCP packets
    *       having payload size larger than TSO size limit, up to
    *       256KB. Such packets will be split into multiple TSO packets
    *       no larger than TSO size limit.
    */
   VMK_UPLINK_SERVICE_TSO_256K = 10,

   /**
    * IPv6 with extension headers checksum offloading service
    * \note This service allows port client to offload TCP/UDP checksum
    *       calculation of IPv6 TX packets with extension headers, down
    *       to uplink layer or NIC driver. Port client must mark TX
    *       packets as "CSO needed" by calling vmk_PktSetMustCsum().
    */
   VMK_UPLINK_SERVICE_IPV6_EXT_CSO = 11,

   /**
    * IPv6 with extension headers TCP segmentation offloading service
    * \note This service allows port client to offload TCP segmentation
    *       of IPv6 TX packets with extension headers, down to uplink
    *       layer or NIC driver. Port client must mark TX packets as
    *       "TSO needed" and set proper MSS by calling
    *       vmk_PktSetLargeTcpPacket().
    */
   VMK_UPLINK_SERVICE_IPV6_EXT_TSO = 12,

   /**
    * Service to adjust TX packet INET header alignment
    * \note Some NIC drivers require TX packet's INET header to be
    *       aligned. This service adjusts INET header in TX packet to
    *       be compliant with driver's alignment requirement.
    */
   VMK_UPLINK_SERVICE_OFFLOAD_NO_ALIGNMENT = 13,

   /**
    * Generic encapsulation offloading service
    * \note This service performs TCP checksum and segmentation for
    *       VXLAN and GRE encapsulated TX packets. If driver supports
    *       VMK_UPLINK_CAP_ENCAP_OFFLOAD, checksum and segmentation are
    *       fully offloaded to driver. If driver supports
    *       VMK_UPLINK_CAP_OFFLOAD_CONSTRAINTS, half of the work is
    *       done in uplink layer, the rest will be done in driver.
    */
   VMK_UPLINK_SERVICE_GENERIC_OFFLOAD = 14,
} vmk_UplinkService;


/*
 ***********************************************************************
 * vmk_UplinkServiceRequest --                                    */ /**
 *
 * \brief Request a uplink service defined in vmk_UplinkService
 *
 * \nativedriversdisallowed
 *
 * Some NIC drivers only provide a subset of uplink service. When
 * requesting a service that hardware doesn't provide, vmkernel will
 * insert a software routine to do the packet processing.
 *
 * \note The calling thread must hold a mutable handle for the portset
 *       associated with the specified uplink.
 *
 * \note This function will not block.
 *
 * \param[in]    uplink         Handle of uplink to update caps on.
 * \param[in]    service        Service to request.
 *
 * \retval  VMK_OK                           If service request is
 *                                           successful.
 * \retval  VMK_BAD_PARAM                    Uplink or service is
 *                                           invalid.
 * \retval  VMK_FAILURE                      Request operation fails.
 * \retval  VMK_PORTSET_HANDLE_NOT_MUTABLE   Caller is not holding a
 *                                           mutable portset handle.
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkServiceRequest(vmk_Uplink uplink,
                                          vmk_UplinkService service);


/*
 ***********************************************************************
 * vmk_UplinkServiceRelease --                                    */ /**
 *
 * \brief Release a uplink service defined in vmk_UplinkService
 *
 * \nativedriversdisallowed
 *
 * Some NIC drivers only provide a subset of uplink service. When
 * releasing a service that hardware doesn't provide, vmkernel will
 * remove the software routine inserted by vmk_UplinkServiceRequest()
 * if the service is not requested by others.
 *
 * \note The calling thread must hold a mutable handle for the portset
 *       associated with the specified uplink.
 *
 * \note This function will not block.
 *
 * \param[in]    uplink         Handle of uplink to update caps on.
 * \param[in]    service        Service to release.
 *
 * \retval  VMK_OK                           If service release is
 *                                           successful.
 * \retval  VMK_BAD_PARAM                    Uplink or service is
 *                                           invalid.
 * \retval  VMK_FAILURE                      Release operation fails.
 * \retval  VMK_PORTSET_HANDLE_NOT_MUTABLE   Caller is not holding a
 *                                           mutable portset handle.
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkServiceRelease(vmk_Uplink uplink,
                                          vmk_UplinkService service);


/*
 ***********************************************************************
 * vmk_UplinkServiceIsActive --                                   */ /**
 *
 * \brief Check if a service is active on a uplink.
 *
 * \nativedriversdisallowed
 *
 * The function returns the activation status of specific service,
 * either provided by uplink layer or driver.
 *
 * \note The calling thread must hold at least an immutable handle for
 *       the portset associated with the specified uplink.
 *
 * \note This function will not block.
 *
 * \param[in]    uplink         Handle of uplink.
 * \param[out]   service        Service to check its activation status.
 *
 * \retval       VMK_TRUE       If service is active.
 * \retval       VMK_FALSE      If service is inactive or not supported.
 ***********************************************************************
 */

vmk_Bool vmk_UplinkServiceIsActive(vmk_Uplink uplink,
                                   vmk_UplinkService service);


/*
 ***********************************************************************
 * vmk_UplinkVLANFilterBitmapGet --                               */ /**
 *
 * \brief Return VLAN filter bitmap on a uplink.
 *
 * \nativedriversdisallowed
 *
 * Caller must perform these two steps to return VLAN bitmap:
 * 1) call vmk_UplinkServiceRequest() to request VLAN insertion or
 *    strip service.
 * 2) call vmk_UplinkVLANFilterGet() to return VLAN filter bitmap.
 *
 * \note The calling thread must hold at least an immutable handle for
 *       the portset associated with the specified uplink.
 *
 * \note This function will not block.
 *
 * \param[in]    uplink         Uplink handle
 * \param[out]   bitmap         Uplink's permitted VLAN IDs bitmap.
 *
 * \retval       VMK_OK         Update successfully finished.
 * \retval       VMK_BAD_PARAM  Etiher uplink or bitmap is invalid.
 * \retval       VMK_FAILURE    Failed to get VLAN bitmap.
 * \retval       VMK_NOT_READY  The port has not configured VLAN
 *                              capabilities.
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkVLANFilterBitmapGet(vmk_Uplink uplink,
                                               vmk_VLANBitmap *bitmap);


/*
 ***********************************************************************
 * vmk_UplinkVLANFilterIsSet --                                   */ /**
 *
 * \brief Return the filter of specific VLAN ID is set on a uplink.
 *
 * \nativedriversdisallowed
 *
 * Caller must perform these two steps to check VLAN filter:
 * 1) call vmk_UplinkServiceRequest() to request VLAN insertion or
 *    strip service.
 * 2) call vmk_UplinkVLANFilterIsSet() to check VLAN filter.
 *
 * \note The calling thread must hold at least an immutable handle for
 *       the portset associated with the specified uplink.
 *
 * \note This function will not block.
 *
 * \param[in]    uplink         Uplink handle.
 * \param[in]    vlanID         ID of VLAN to check.
 *
 * \retval       VMK_TRUE       The specific VLAN ID is set.
 * \retval       VMK_FALSE      Otherwise.
 ***********************************************************************
 */

vmk_Bool vmk_UplinkVLANFilterIsSet(vmk_Uplink uplink, vmk_VlanID vlanID);


/*
 ***********************************************************************
 * vmk_UplinkVLANFilterBitmapSet --                               */ /**
 *
 * \brief Update VLAN filter bitmap on a uplink.
 *
 * \nativedriversdisallowed
 *
 * This function updates the set of VLAN IDs the device will process.
 * The bitmap, if not NULL, marks permitted VLAN IDs on the port.
 * Packets with non-permitted VLAN IDs may be filtered by device.
 *
 * Caller must perform these two steps to configure VLAN bitmap:
 * 1) call vmk_UplinkServiceRequest() to request VLAN insertion or
 *    strip service.
 * 2) call vmk_UplinkVLANFilterSet() to update VLAN filter bitmap.
 *
 * \note Some hardware devices do not implement VLAN filtering on
 *       packet receive. The switch implementation must be prepared to
 *       receive packets with non-permitted VLAN IDs.
 *
 * \note The calling thread must hold a mutable handle for the portset
 *       associated with the specified uplink.
 *
 * \param[in]    uplink         Uplink handle
 * \param[in]    bitmap         Permitted VLAN IDs.
 *
 * \retval  VMK_OK                           Update successfully
 *                                           finished.
 * \retval  VMK_BAD_PARAM                    Etiher uplink or bitmap is
 *                                           invalid.
 * \retval  VMK_FAILURE                      Failed to set VLAN bitmap.
 * \retval  VMK_NOT_READY                    The port has not configured
 *                                           VLAN capabilities.
 * \retval  VMK_PORTSET_HANDLE_NOT_MUTABLE   Caller is not holding a
 *                                           mutable portset handle.
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkVLANFilterBitmapSet(vmk_Uplink uplink,
                                               vmk_VLANBitmap *bitmap);


/*
 ***********************************************************************
 * vmk_UplinkVLANFilterRangeAdd --                                */ /**
 *
 * \brief Add a range of VLAN IDs to a uplink's VLAN filter bitmap.
 *
 * \nativedriversdisallowed
 *
 * This function adds a range of VLAN IDs to the uplink's VLAN filter
 * bitmap. Packets within that VLAN ID range will be accepted by the
 * corresponding NIC driver.
 *
 * Caller must perform these two steps to add a range of accepted VLAN
 * IDs:
 * 1) call vmk_UplinkServiceRequest() to request VLAN insertion or
 *    strip service.
 * 2) call vmk_UplinkVLANFilterRangeAdd() to add VLAN filters.
 *
 * \note Some hardware devices do not implement VLAN filtering on
 *       packet receive. The switch implementation must be prepared to
 *       receive packets with non-permitted VLAN IDs.
 *
 * \note The calling thread must hold a mutable handle for the portset
 *       associated with the specified uplink.
 *
 * \param[in]    uplink         Uplink handle.
 * \param[in]    startID        The start VLAN ID in the range to set.
 * \param[in]    endID          The end VLAN in the range to set.
 *
 * \retval  VMK_OK                           Addition successfully
 *                                           finished.
 * \retval  VMK_BAD_PARAM                    Etiher uplink, startID or
 *                                           endID is invalid.
 * \retval  VMK_FAILURE                      Failed to set VLAN filter
 *                                           range.
 * \retval  VMK_NOT_READY                    The port has not configured
 *                                           VLAN capabilities.
 * \retval  VMK_PORTSET_HANDLE_NOT_MUTABLE   Caller is not holding a
 *                                           mutable portset handle.
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkVLANFilterRangeAdd(vmk_Uplink uplink,
                                              vmk_VlanID startID,
                                              vmk_VlanID endID);


/*
 ***********************************************************************
 * vmk_UplinkVLANFilterRangeRemove --                             */ /**
 *
 * \brief Remove a range of VLAN IDs from a uplink's VLAN filter bitmap.
 *
 * \nativedriversdisallowed
 *
 * This function removes a VLAN ID range from the uplink's VLAN filter
 * bitmap. Packets within that VLAN ID range will be filtered by the
 * corresponding NIC driver.
 *
 * Caller must perform these two steps to remove VLAN ID range:
 * 1) call vmk_UplinkServiceRequest() to request VLAN insertion or
 *    strip service.
 * 2) call vmk_UplinkVLANFilterRangeRemove() to remove VLAN filters.
 *
 * \note Some hardware devices do not implement VLAN filtering on
 *       packet receive. The switch implementation must be prepared to
 *       receive packets with non-permitted VLAN IDs.
 *
 * \note The calling thread must hold a mutable handle for the portset
 *       associated with the specified uplink.
 *
 * \param[in]    uplink         Uplink handle.
 * \param[in]    startID        The start VLAN ID in the range to
 *                              remove.
 * \param[in]    endID          The end VLAN in the range to clear.
 *
 * \retval  VMK_OK                           Removal successfully
 *                                           finished.
 * \retval  VMK_BAD_PARAM                    Etiher uplink, startID or
 *                                           endID is invalid.
 * \retval  VMK_FAILURE                      Failed to clear VLAN filter
 *                                           range.
 * \retval  VMK_NOT_READY                    The port has not configured
 *                                           VLAN capabilities.
 * \retval  VMK_PORTSET_HANDLE_NOT_MUTABLE   Caller is not holding a
 *                                           mutable portset handle.
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkVLANFilterRangeRemove(vmk_Uplink uplink,
                                                 vmk_VlanID startID,
                                                 vmk_VlanID endID);


/*
 ***********************************************************************
 * vmk_UplinkVirtualMACAddrGet --                                 */ /**
 *
 * \brief Get virtual MAC address of a uplink.
 *
 * \nativedriversdisallowed
 *
 * \note  This virtual MAC address may differ from device's physical
 *        MAC address.
 *
 * \note The calling thread must hold at least an immutable handle for
 *       the portset associated with the specified uplink.
 *
 * \note This function will not block.
 *
 * \param[in]    uplink         Uplink handle
 * \param[out]   macAddr        MAC address of uplink
 *
 * \retval       VMK_OK         Get operation successfully finished.
 * \retval       VMK_BAD_PARAM  Either uplink or macAddr is invalid.
 * \retval       VMK_NOT_FOUND  The portset associated with uplink is
 *                              not found.
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkVirtualMACAddrGet(vmk_Uplink uplink,
                                             vmk_EthAddress macAddr);


/*
 ***********************************************************************
 * vmk_UplinkQueueApplyPortFilter --                              */ /**
 *
 * \brief Apply a port filter to a uplink.
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
 * \note The calling thread must hold a mutable handle for the portset
 *       associated with the specified port.
 *
 * \note This function will not block.
 *
 * \param[in]    uplink        Handle of uplink to apply filter to.
 * \param[in]    portID        Numeric ID of a virtual port.
 * \param[in]    filter        Filter advertised for this port.
 * \param[in]    prop          Properties of this filter.
 *
 * \retval  VMK_OK                           Filter applied
 *                                           successfully.
 * \retval  VMK_BAD_PARAM                    Invalid uplink, portID or
 *                                           filter.
 * \retval  VMK_FAILURE                      Filter has not been
 *                                           applied.
 * \retval  VMK_PORTSET_HANDLE_NOT_MUTABLE   Caller is not holding a
 *                                           mutable portset handle.
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkQueueApplyPortFilter(vmk_Uplink uplink,
                                                vmk_SwitchPortID portID,
                                                vmk_UplinkQueueFilter *filter,
                                                vmk_UplinkQueueFilterProperties prop);

/*
 ***********************************************************************
 * vmk_UplinkQueueRemovePortFilter --                             */ /**
 *
 * \brief Remove a port filter from a uplink.
 *
 * \nativedriversdisallowed
 *
 * The filter will no longer be part of the rx load balancing algorithm
 * running in the uplink layer. If a queue was associated to this
 * filter, it will be released for other filters.
 *
 * \note The calling thread must hold a mutable handle for the portset
 *       associated with the specified port.
 *
 * \note This function will not block.
 *
 * \param[in]    uplink         Handle of uplink to remove filter from.
 * \param[in]    portID         Numeric ID of a virtual port.
 * \param[in]    filter         Filter advertised for this port.
 *
 * \retval  VMK_OK                           Filter removed
 *                                           successfully.
 * \retval  VMK_BAD_PARAM                    Invalid uplink, portID or
 *                                           filter.
 * \retval  VMK_FAILURE                      Failed to remove filter.
 * \retval  VMK_PORTSET_HANDLE_NOT_MUTABLE   Caller is not holding a
 *                                           mutable portset handle.
 ***********************************************************************
 */
VMK_ReturnStatus vmk_UplinkQueueRemovePortFilter(vmk_Uplink uplink,
                                                 vmk_SwitchPortID portID,
                                                 vmk_UplinkQueueFilter *filter);


/*
 ***********************************************************************
 * vmk_UplinkGetByName --                                         */ /**
 *
 * \brief Get uplink pointer by its name
 *
 * This function will look through uplink list and return the matched
 * uplink.
 *
 * \note The caller must call vmk_UplinkRelease() to release the handle
 *       returned.
 *
 * \note This function may block
 *
 * \param[in]   uplinkName        Uplink name
 * \param[out]  uplink            Pointer to uplink
 *
 * \retval      VMK_OK            if uplink is found
 * \retval      VMK_NOT_FOUND     if uplink is not found
 ***********************************************************************
 */
VMK_ReturnStatus vmk_UplinkGetByName(vmk_Name *uplinkName,
                                     vmk_Uplink *uplink);


/*
 ***********************************************************************
 * vmk_UplinkRelease --                                           */ /**
 *
 * \brief Release the uplink handle acquired by vmk_UplinkGetByName()
 *
 * \note This function will not block.
 *
 * \note This function must not be used to release a uplink handle that
 *       has been provided as an argument to a callback.
 *
 * \param[in] uplink    The handle of uplink object to release
 *
 ***********************************************************************
 */
void vmk_UplinkRelease(vmk_Uplink uplink);


#endif /* _VMKAPI_NET_UPLINK_SERVICE_H_ */
/** @} */
/** @} */
