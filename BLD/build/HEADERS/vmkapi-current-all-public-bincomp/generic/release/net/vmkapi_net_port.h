/***************************************************************************
 * Copyright 2013 VMware, Inc.  All rights reserved.
 ***************************************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Port                                                           */ /**
 * \addtogroup Network
 * @{
 * \defgroup Networking Port APIs
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
 */

#ifndef _VMKAPI_NET_PORT_H_
#define _VMKAPI_NET_PORT_H_

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

/*
 ***********************************************************************
 * vmk_PortOutput --                                              */ /**
 *
 * \brief Send a list of packets out of a port.
 *
 * \nativedriversdisallowed
 *
 * This function can be used when a portset sends packets to a port
 * for transmission. This function is intended for internal use by
 * portset only. Other clients within VMkernel have to use
 * vmk_PortInput() to inject packets and it is the portsets'
 * responsibility to decide on which port packets will be sent out of.
 *
 * The VMkernel may apply further processing on packets (e.g. DVFilters)
 * as part of vmk_PortOutput() before they are delivered to the port
 * client.
 *
 * \note In case of mayModify being VMK_TRUE, on success or failure,
 *       the pktList will be cleared and all packets will be released.
 *
 * \note By setting mayModify to VMK_FALSE the portset can optimize
 *       performance in case it wants to pass the same PktList
 *       to multiple recipients. Note that mayModify set to VMK_FALSE
 *       does not imply that packets will reach the port client
 *       unmodified, as VMkernel may take (shallow) packet copies
 *       that it can modify and deliver those instead.
 *
 * \note This function should be called inside vmk_PortsetOps's callback
 *       functions.
 *
 * \note The caller must hold a handle for the porset associated
 *       with the specified portID. For performance reasons, it is
 *       recommended that this function be invoked with an immutable
 *       handle.
 *
 * \note This function will not block.
 *
 * \param[in]  portID         Numeric ID of a virtual port
 * \param[in]  pktList        List of packets to send
 * \param[in]  mayModify      If set to VMK_FALSE, VMkernel will not
 *                            modify the PktList nor any of the
 *                            contained packets. If set to VMK_TRUE,
 *                            VMkernel is free to perform any
 *                            modification.
 *
 * \retval     VMK_OK            The PktList is processed successfully
 * \retval     VMK_IS_DISABLED   The port is disabled
 * \retval     VMK_NOT_FOUND     PortID is invalid or port not found
 * \retval     VMK_NO_RESOURCES  Allocation failure
 * \retval     Other status      The PktList couldn't be delivered
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortOutput(vmk_SwitchPortID portID,
                                vmk_PktList pktList,
                                vmk_Bool mayModify);

/*
 ***********************************************************************
 * vmk_PortInput --                                              */ /**
 *
 * \brief Send a list of packets into a port.
 *
 * \nativedriversdisallowed
 *
 * The input is from the portset's pespective. The function is used when
 * port client sends packets to portset. The packet list will be emptied
 * on success or failure.
 *
 * The VMkernel may apply further processing on packets (e.g. DVFilters)
 * as part of vmk_PortInput() before they are delivered to the portset.
 *
 * \note The caller must hold an immutable handle for the porset
 *       associated with the specified portID.
 *
 * \note This function will not block.
 *
 * \param[in]  portID         Numeric ID of a virtual port
 * \param[in]  pktList        List of packets to send
 *
 * \retval     VMK_OK           If the PktList is processed successfully
 * \retval     VMK_NOT_FOUND    If the supplied port was invalid
 * \retval     VMK_IS_DISABLED  If the port is disabled or blocked
 * \retval     Other status     If the PktList cannot be processed
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortInput(vmk_SwitchPortID portID,
                               vmk_PktList pktList);

/*
 ***********************************************************************
 * vmk_PortBlock --                                             */ /**
 *
 * \brief Disallow I/O traffic through a port.
 *
 * \nativedriversdisallowed
 *
 * Portset implementations call this interface to stop traffic.
 *
 * \param[in]  portID         Numeric ID of a virtual port
 *
 * \note This function may fail for reasons not listed below
 *       due to errors in 3rd party modules invoked during the operation.
 *
 * \note The caller must hold a mutable handle for the portset
 *       associated with the specified portID.
 *
 * \note This function will not block.
 *
 * \retval  VMK_OK                         Operation was successful
 * \retval  VMK_NO_MEMORY                  Insufficient memory to
 *                                         complete the operation
 * \retval  VMK_PORTSET_HANDLE_NOT_MUTABLE The caller did not hold a
 *                                         mutable handle
 ***********************************************************************
 */
VMK_ReturnStatus  vmk_PortBlock(vmk_SwitchPortID portID);

/*
 ***********************************************************************
 * vmk_PortUnblock --                                             */ /**
 *
 * \brief Allow I/O traffic through a port.
 *
 * \nativedriversdisallowed
 *
 * A port always starts blocked following portset creation.
 * Portset implementations call this interface when ready to
 * accept traffic. This applies to uplink ports as well as ports
 * connected to virtual NICs.
 *
 * \note This function may fail for reasons not listed below
 *       depending on the portset implementation.
 *
 * \note The caller must hold a mutable handle for the portset
 *       associated with the specified portID.
 *
 * \note This function will not block.
 *
 * \param[in]  portID         Numeric ID of a virtual port
 *
 * \retval  VMK_OK
 * \retval  VMK_NO_MEMORY                  Insufficient memory to
 *                                         complete the operation
 * \retval  VMK_PORTSET_HANDLE_NOT_MUTABLE The caller did not hold a
 *                                         mutable handle
 ***********************************************************************
 */
VMK_ReturnStatus  vmk_PortUnblock(vmk_SwitchPortID portID);

/*
 ***********************************************************************
 * vmk_PortLinkStatusSet --                                       */ /**
 *
 * \brief Set link status of vNic connected to a port.
 *
 * \nativedriversdisallowed
 *
 * vNetwork admin may disable/enable a virtual port. When this
 * happens, the portset implementation calls this interface to
 * notify guest vNic that link is down and I/O traffic is no
 * longer flowing. The link down status will be visible to the
 * guest administrator. If the port is not connected to a VM's vNic,
 * the interface is a no-op.
 *
 * \note The caller must hold a mutable handle for the portset
 *       associated with the specified portID.
 *
 * \note This function will not block.
 *
 * \param[in]  portID         Numeric ID of a virtual port
 * \param[in]  linkUp         Whether link should be up
 *
 * \retval VMK_OK                         Success.
 * \retval VMK_NOT_FOUND                  The specified port was
 *                                        not found.
 * \retval VMK_PORTSET_HANDLE_NOT_MUTABLE The caller did not hold a
 *                                        mutable handle
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortLinkStatusSet(vmk_SwitchPortID portID,
                                       vmk_Bool linkUp);

/*
 ***********************************************************************
 * vmk_PortIsOutputActive --                                      */ /**
 *
 * \brief Check if a port permit outgoing traffic.
 *
 * \nativedriversdisallowed
 *
 * \note The caller must hold a handle for the portset
 *       associated with the specified portID.
 *
 * \note This function will not block.
 *
 * \param[in]  portID         Numeric ID of a virtual port
 * \param[out] isActive       Whether output traffic is permitted
 *
 * \retval     VMK_OK         Successfully get port's active state
 * \retval     VMK_NOT_FOUND  Portset is not active or port is not found
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortIsOutputActive(vmk_SwitchPortID portID,
                                        vmk_Bool *isActive);

/*
 ***********************************************************************
 * vmk_PortIsUplink --                                            */ /**
 *
 * \brief Check if a port is connected to an uplink.
 *
 * \nativedriversdisallowed
 *
 * \note The caller must hold a handle for the portset
 *       associated with the specified portID.
 *
 * \note This function will not block.
 *
 * \param[in]  portID            Port identifier
 * \param[out] isUplink          VMK_TRUE if this is an uplink port,
 *                               otherwise VMK_FALSE.
 *
 * \retval     VMK_OK            Success.
 * \retval     VMK_BAD_PARAM     isUplink is NULL.
 * \retval     VMK_NOT_FOUND     Couldn't find a port with a matching
 *                               port identifier.
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortIsUplink(vmk_SwitchPortID portID,
                                  vmk_Bool *isUplink);

/*
 ***********************************************************************
 * vmk_PortGetUplink --                                           */ /**
 *
 * \brief Get the underlying uplink of a port.
 *
 * \nativedriversdisallowed
 *
 * \note The caller must hold a mutable/immutable handle for the portset
 *       associated with the specified portID.
 *
 * \note This function will not block.
 *
 * \param[in]  portID         Numeric ID of a virtual port
 * \param[in]  uplink         Pointer to hold the associated uplink
 *
 * \retval  VMK_OK                         Successfully found
 *                                         underlying uplink
 * \retval  VMK_BAD_PARAM                  if the port is not
 *                                         connected to an uplink
 * \retval  VMK_NOT_FOUND                  if the port id is not valid
 * \retval  VMK_FAILURE                    The caller did not hold
 *                                         immutable or mutable portset
 *                                         handle
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortGetUplink(vmk_SwitchPortID portID,
                                   vmk_Uplink *uplink);

/*
************************************************************************
* vmk_PortGetStats --                                             */ /**
*
* \brief Optional callback for getting statistics on special uplink ports.
*
* Special uplinks include passthroughed hidden uplinks, whose stats are
* maintained in passthroughed device drivers or portsets. For portsets
* not using passthroughed uplink device, this callback is not needed.
*
* \note This function will not block.
*
* \deprecated This function will be modified to return vmk_UplinkStats instead
*             of vmk_VDSPortStatistics in a future release.
*
* \param[in]  ps                Immutable handle to a portset.
* \param[in]  portID            Numeric ID of the virtual port
* \param[out] stats             Port statistics
*
* \retval     VMK_OK            If port stats are successfully fetched
* \retval     other value       If port stats are not successfully
*                               fetched
***********************************************************************
*/

typedef VMK_ReturnStatus (*vmk_PortGetStats)(vmk_Portset *ps,
                                             vmk_SwitchPortID portID,
                                             vmk_VDSPortStatistics *stats);

/**
 * \brief An array of port numbers.
 * \note  To allocate a buffer for this structure, use the
 *        vmk_PortArraySize() calculate the size.
 */

typedef struct {
   /** Number of switch port ID in the array. */
   vmk_uint32          numPorts;

   /** The array of vswitch port numbers. */
   vmk_SwitchPortID    ports[];

} vmk_PortArray;

/**
 * \brief Get the size of n ports Array.
 */

static inline vmk_uint32
vmk_PortArraySize(vmk_uint32 nPorts)
{
   return vmk_offsetof(vmk_PortArray, ports[nPorts]);
}

/**
 * \brief Port state update operations.
 *
 *  The operations are processed by the portset implementation's
 *  vmk_PortUpdate handler.
 */
typedef enum vmk_PortUpdateOp {
   /** port client link change. */
   VMK_PORT_UPDATE_LINK_CHANGE = 1,

   /** vNic port FRP update. */
   VMK_PORT_UPDATE_ETH_FRP = 2,

   /** vNic port VLAN update. */
   VMK_PORT_UPDATE_VLAN = 3,

   /** port DCB setting change. */
   VMK_PORT_UPDATE_DCB_CHANGE = 4,

   /** port VM UUID update. */
   VMK_PORT_UPDATE_VMUUID = 5,

   /** Port VF update. */
   VMK_PORT_UPDATE_VF = 6,

   /** vNic port is quiesced. */
   VMK_PORT_UPDATE_QUIESCED = 7,

   /** Update port's output uplink. */
   VMK_PORT_UPDATE_OUTPUT_UPLINK = 8,

   /** Clear port's packet stats. */
   VMK_PORT_UPDATE_CLEAR_PACKET_STATS = 9,
} vmk_PortUpdateOp;


/** \brief Port packet stats types. */
typedef enum vmk_PortPktStatsType {
   /** Number of unicast packets received/transmitted by the port. */
   VMK_PORT_PKT_STATS_UNICAST_PKTS = 0,

   /** Number of unicast bytes received/transmitted by the port. */
   VMK_PORT_PKT_STATS_UNICAST_BYTES,

   /** Number of multicast packets recevied/transmitted by the port. */
   VMK_PORT_PKT_STATS_MULTICAST_PKTS,

   /** Number of multicast bytes recevied/transmitted by the port. */
   VMK_PORT_PKT_STATS_MULTICAST_BYTES,

   /** Number of broadcast packets recevied/transmitted by the port. */
   VMK_PORT_PKT_STATS_BROADCAST_PKTS,

   /** Number of broadcast bytes recevied/transmitted by the port. */
   VMK_PORT_PKT_STATS_BROADCAST_BYTES,

   /** Number of exception packets recevied/transmitted by the port. */
   VMK_PORT_PKT_STATS_EXCEPTIONS_PKTS,

   /** Number of packets dropped by the port. */
   VMK_PORT_PKT_STATS_DROPPED_PKTS,
} vmk_PortPktStatsType;


/** \brief Port packet stats direction. */
typedef enum vmk_PortPktStatsDir {
   /** Packets dispatched from port to portset. */
   VMK_PORT_PKT_STATS_DIR_IN = 0,

   /** Packets dispatched from portset to port. */
   VMK_PORT_PKT_STATS_DIR_OUT,
} vmk_PortPktStatsDir;


/**
 * \brief Parameter structure for port packet stats clear request.
 *
 * This structure is used to give PortUpdate information on
 * VMK_PORT_UPDATE_CLEAR_PACKET_STATS event to clear port's portset
 * implementation internal packet stats.
 */
typedef struct vmk_PortUpdateClearPacketStatsData {
   /** The direction of packet stats to clear */
   vmk_PortPktStatsDir dir;

   /** The type of packet stats to clear */
   vmk_PortPktStatsType type;
} vmk_PortUpdateClearPacketStatsData;


/**
 * \brief Argument type of PortUpdate for uplink change.
 *
 * This structure is used to give PortUpdate information on
 * VMK_PORT_UPDATE_LINK_CHANGE event concerning a transition
 * that occured on an port.
 */
typedef enum vmk_PortUpdateLinkEvent {

   /** Port client is connected to portset. */
   VMK_PORT_CLIENT_DISCONNECTED,

   /** Port client is disconnected to portset. */
   VMK_PORT_CLIENT_CONNECTED,

   /** Port client is link up. */
   VMK_PORT_CLIENT_LINK_UP,

   /** Port client is link down. */
   VMK_PORT_CLIENT_LINK_DOWN,

} vmk_PortUpdateLinkEvent;

/**
 * \brief Argument structure of PortUpdate for guest VLANs.
 *
 * This structure is used to give PortUpdate information on
 * VMK_PORT_UPDATE_VLAN event concerning a change in the VLAN
 * membership of a guest.
 */

typedef struct vmk_PortUpdateVLANData {
   /**
    * VLAN bitmap, each bit corresponds to one VLAN.
    */
   const struct vmk_VLANBitmap *vlanGroups;
} vmk_PortUpdateVLANData;

/**
 * \brief VF operations for a port.
 *
 * Currently we have two API to acquire/release a VF:
 *
 *  One is defined here for 3rd Party vDS to use. It is based on
 *  Portset_UpdatePortVF() API. The operation is sent to the VDS
 *  implementation.
 *  The other one is uplink async call based, and the operation
 *  does not go through vDS implementation.
 *
 * While the first one is the preferred way for the operation, we need the
 * second one due to the locking issue. (Can not hold lock to call into
 * device layer).
 */

typedef enum vmk_PortVFOps {
   /** Acquire a VF for a port. */
   VMK_PORT_VF_ACQUIRE =  0x1,
   /** Release a VF previously acquired for a port. */
   VMK_PORT_VF_RELEASE =  0x2,
} vmk_PortVFOps;

/**
 * \brief Argument structure for the VMK_PORT_VF_ACQUIRE operation.
 *
 * This structure is used to exchange information with PortUpdate on
 * VMK_PORT_VF_ACQUIRE operation.
 */

typedef struct vmk_PortVFAcquireData {

   /**
    *  The passthru type the VF will be used for.
    *  This field is populated by the caller.
    */
   vmk_NetPTType  ptType;
   /**
    * The requirements for the VF returned.
    * This field is populated by the caller.
    */
   vmk_NetVFRequirements  req;

   /** The VF returned by the callee. */
   vmk_VFDesc  vfDesc;
} vmk_PortVFAcquireData;

/**
 * \brief Argument structure for the VMK_PORT_VF_RELEASE operation.
 *
 * This structure is used to exchange information with PortUpdate on
 * VMK_PORT_VF_RELEASE operation.
 */

typedef struct vmk_PortVFReleaseData {
   /** The VF to be released. */
   vmk_VFDesc vfDesc;
} vmk_PortVFReleaseData;

/**
 * \brief Argument structure for VF operations.
 *
 * This structure is used to exchange information with PortUpdate on
 * VMK_PORT_UPDATE_VF event.
 */

typedef struct vmk_PortUpdateVFData {
   /** The operation requested. */
   vmk_PortVFOps  vfOp;
   union {
      /** When VMK_PORT_VF_ACQUIRE operation is requested. */
      vmk_PortVFAcquireData acq;
      /** When VMK_PORT_VF_RELEASE operation is requested. */
      vmk_PortVFReleaseData rel;
   } u;
} vmk_PortUpdateVFData;

/*
 ***********************************************************************
 * vmk_PortUpdate --                                              */ /**
 *
 * \brief Required callback for handling port update notifications.
 *
 * VMK_PORT_UPDATE_LINK_CHANGE: data is of (vmk_PortUpdateLinkEvent *)
 * type
 *
 * VMK_PORT_UPDATE_ETH_FRP: data is of (vmk_EthFRP *) type. The
 * requestedFilter is read-only, the callback can set acceptedFilter.
 *
 * VMK_PORT_UPDATE_VLAN: data is of (vmk_PortUpdateVLANData *) type
 *
 * VMK_PORT_UPDATE_DCB_CHANGE: there is no data
 *
 * VMK_PORT_UPDATE_VMUUID: there is no data
 *
 * VMK_PORT_UPDATE_VF: data is of (vmk_PortUpdateVFData *) type
 *
 * VMK_PORT_UPDATE_QUIESCED: there is no data
 *
 * \note This callback cannot block.
 *
 * \param[in]      ps                Mutable handle to a portset.
 * \param[in]      portID            Numeric ID of the virtual port
 * \param[in]      opcode            Kind of update
 * \param[in,out]  data              Data specific to operation
 *
 * \retval     VMK_OK                Update is successful
 * \retval     VMK_NOT_SUPPORTED     The request is not supported
 * \retval     Other status          Update failure
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_PortUpdate)(vmk_Portset *ps,
                                           vmk_SwitchPortID portID,
                                           vmk_PortUpdateOp opcode,
                                           void *data);

/*
 ***********************************************************************
 * vmk_PortFunction --                                            */ /**
 *
 * \brief Callback type for function called by vmk_PortsetForAllPorts.
 *
 * \nativedriversdisallowed
 *
 * \note The portset handle is the same one that was passed to
 *       vmk_PortsetForAllPorts() and vmk_PortsetForAllActiveDVPorts.
 *
 * \note This function must not release the portset handle.
 *
 * \note This function will not block.
 *
 * \param[in]   ps               Handle to a portset.
 * \param[in]   portID           Numeric ID of a virtual port
 * \param[in]   arg              User-specified argument for operation
 *                               (callback context cookie)
 *
 * \retval      VMK_OK           Success.
 * \retval      Other status     Implementation specific error code.
 ***********************************************************************
 */
typedef VMK_ReturnStatus vmk_PortFunction(vmk_Portset *ps,
                                          vmk_SwitchPortID portID,
                                          void *arg);

/*
 ***********************************************************************
 * vmk_PortGetFRP --                                              */ /**
 *
 * \brief Retrieve ethernet frame routing policy on a port.
 *
 * \nativedriversdisallowed
 *
 * \note The caller must hold a handle for the portset
 *       associated with the specified portID.
 *
 * \note This function will not block.
 *
 * \param[in]  portID         Numeric ID of a virtual port
 * \param[out] ethFRP         Ethernet frame routing policy on port
 *
 * \retval     VMK_OK         Successfully get port ethFRP
 * \retval     VMK_NOT_FOUND  Portset is not active or port not found
 * \retval     VMK_BAD_PARAM  The ethFRP argument is NULL.
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortGetFRP(vmk_SwitchPortID portID,
                                vmk_EthFRP *ethFRP);

/*
 ***********************************************************************
 * vmk_PortUpdateEthFRP --                                        */ /**
 *
 * \brief Update ethernet frame routing policy.
 *
 * \nativedriversdisallowed
 *
 * This function may be used on ports that were created through the
 * vmk_PortsetConnectPort() API. It is used to inform the portset
 * implementation about the kind of ethernet packets, as specified in
 * ethFilter, the port client expects to receive.
 *
 * \note The caller must hold a mutable portset handle for the
 *       portset associated with the specified portID.
 *
 * \note This function will not block.
 *
 * \param[in]  portID          Numeric ID of a virtual port
 * \param[in]  ethFilter       Ethernet address filter
 *
 * \retval  VMK_OK                         The new FRP is successfully
 *                                         installed.
 * \retval  VMK_BAD_PARAM                  ethFilter is not a valid
 *                                         ethernet address filter.
 * \retval  VMK_NOT_FOUND                  Port doesn't exist.
 * \retval  VMK_NO_MEMORY                  Allocation failure.
 * \retval  VMK_NO_PERMISSION              This port was not created
 *                                         using vmk_PortsetConnectPort().
 * \retval  VMK_PORTSET_HANDLE_NOT_MUTABLE The caller did not hold a
 *                                         mutable handle
 * \retval  Other                          Failed to notify portset of
 *                                         new FRP.
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortUpdateEthFRP(vmk_SwitchPortID portID,
                                      vmk_EthFilter *ethFilter);

/*
 ***********************************************************************
 * vmk_PortPrivDataDestructor --                                  */ /**
 *
 * \brief Destructor callback for port private data.
 *
 * This callback should be registered in vmk_PortPrivDataSet(). It's an
 * optional callback. See the description of vmk_PortPrivDataSet().
 *
 * \note This function will not block.
 *
 * \param[in]  portID            Numeric ID of a virtual port
 * \param[in]  data              Port private data that should be
 *                               destroyed
 ***********************************************************************
 */
typedef void (*vmk_PortPrivDataDestructor)(vmk_SwitchPortID portID,
                                           void *data);

/*
 ***********************************************************************
 * vmk_PortPrivDataSet --                                         */ /**
 *
 * \brief Set per-port private data.
 *
 * \nativedriversdisallowed
 *
 * Only one private data can be set on each port.
 *
 * This API should be called during port reserve or connect time
 * (i.e. in portset callbacks).

 * The destructor is optional. If provided, the destructor will be
 * called to free the existing private data during
 * (1) port unreserve time and (2) setting a new private data
 * on the port
 *
 * If caller does not provide destructor, caller must free private
 * data explicitly in port unreserve callback. If caller wants to
 * set new private data, it must first
 * destroy previous private data and call vmk_PortsetPrivDataSet
 * to reset existing private data and destructor to NULL. Otherwsie,
 * there is memory leak in the old private data.
 *
 * \note The caller must hold a mutable portset handle for the
 *       portset associated with the specified portID.
 *
 * \note This function will not block.
 *
 * \param[in]  portID         Numeric ID of a virtual port
 * \param[in]  data           Memory location of private data
 * \param[in]  destructor     Optional destructor function
 *
 * \retval  VMK_OK                         PrivData and destructor are
 *                                         succesfully set
 * \retval  VMK_NOT_FOUND                  portID is invalid or port
 *                                         not found
 * \retval  VMK_PORTSET_HANDLE_NOT_MUTABLE The caller did not hold a
 *                                         mutable handle
 * \retval  VMK_EXISTS                     Private data is already set
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortPrivDataSet(vmk_SwitchPortID portID, void *data,
                                     vmk_PortPrivDataDestructor destructor);

/*
 ***********************************************************************
 * vmk_PortPrivDataGet --                                         */ /**
 *
 * \brief Get per-port private data
 *
 * \nativedriversdisallowed
 *
 * The caller must not cache private data or change its value without
 * proper synchronization.
 *
 * \note If caller does not change value in private data, immutable
 *       portset handle should be acquired; Otherwise, mutable portset
 *       handle should be acquired.
 *
 * \note This function will not block.
 *
 * \param[in]  portID         Numeric ID of a virtual port
 * \param[out] data           Pointer to memory location of private data
 *
 * \retval     VMK_OK         Successfully get privData
 * \retval     VMK_NOT_FOUND  portID is invalid or port not found
 * \retval     VMK_FAILURE    The caller did not hold immutable or
 *                            mutable portset handle
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortPrivDataGet(vmk_SwitchPortID portID,
                                     void **data);


/**
  * \brief Client Type for port connected to a vSwitch.
  */

typedef enum {
   /** Unknown type. **/
   VMK_PORT_CLIENT_UNKNOWN  = 0,

   /** Port connect to a TCPIP stack. **/
   VMK_PORT_CLIENT_TCPIP    = 1,

   /** Port connected to a vNIC. **/
   VMK_PORT_CLIENT_VNIC     = 2,

   /** Port connected to a uplink. **/
   VMK_PORT_CLIENT_UPLINK   = 3,

   /** Port connected to a LACP lag. **/
   VMK_PORT_CLIENT_LAG      = 4,
} vmk_PortClientType;

/*
 ***********************************************************************
 * vmk_PortGetClientType --                                       */ /**
 *
 * \brief Get port client type
 *
 * \note The caller must hold a handle for the portset
 *       associated with the specified portID.
 *
 * \note This function will not block.
 *
 * \note The client type is only available when port connect is
 *       completed. Query the client during PortConnect callback may
 *       not return proper value.
 *
 * \param[in]  portID            Port identifier
 * \param[out] type              will contain vmk_PortClientType
 *                               if return VMK_OK.
 *
 * \retval     VMK_OK            Success.
 * \retval     VMK_BAD_PARAM     type is NULL.
 * \retval     VMK_NOT_FOUND     Couldn't find a port with a matching
 *                               port identifier.
 ***********************************************************************
 */

VMK_ReturnStatus
vmk_PortGetClientType(vmk_SwitchPortID portID,
                      vmk_PortClientType *type);

/*
 ***********************************************************************
 * vmk_PortClientNameGet --                                       */ /**
 *
 * \brief Get name of the client connected to the port.
 *
 * \nativedriversdisallowed
 *
 * The client name string should be used for display and debugging
 * purposes only; it should not be parsed. The string is valid for the
 * lifetime of the ephemeral port.
 *
 * \note The caller must hold a handle for the portset
 *       associated with the specified portID.
 *
 * \note This function will not block.
 *
 * \note Port client name is configured at port connect time. If this
 *       function is called in portReserve callback, VMK_NOT_FOUND
 *       may be returned.
 *
 * \param[in]  portID      Numeric ID of a virtual port.
 * \param[out] clientName  The buffer for the NULL-terminated client
 *                         name string.
 *
 * \retval VMK_OK          Success.
 * \retval VMK_BAD_PARAM   An argument is NULL.
 * \retval VMK_NOT_FOUND   The port was not found or the port has
 *                         not set client name
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortClientNameGet(vmk_SwitchPortID portID,
                                       vmk_PortClientName clientName);

/*
 ***********************************************************************
 * vmk_PortClientFixedEthAddrGet --                               */ /**
 *
 * \brief Get fixed mac address of the client connected to the port.
 *
 * \nativedriversdisallowed
 *
 * Called from portset implementation to get VM's mac address assigned
 * by vSphere admin. The returned information is owned by framework
 * and remains valid while the port is connected. The VM cannot change
 * the "fixed" mac address, but is free to send packets with different
 * source mac addresses. The fixed mac address is typically used to
 * enforce security policies, such as preventing mac address forging.
 *
 * \note The caller must hold a handle for the portset
 *       associated with the specified portID.
 *
 * \note This function will not block.
 *
 * \param[in]  portID         Numeric ID of a virtual port.
 * \param[out] addr           Buffer to hold ethernet address
 *
 * \retval     VMK_OK         Success.
 * \retval     VMK_BAD_PARAM  The provided addr is NULL.
 * \retval     VMK_NOT_FOUND  Couldn't find a port with a matching port
 *                            identifier.
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortClientFixedEthAddrGet(vmk_SwitchPortID portID,
                                               vmk_EthAddress addr);

/*
 ***********************************************************************
 * vmk_PortGetVMUUID --                                           */ /**
 *
 * \brief Query the VM UUID of the VM the port is attached to.
 *
 * \nativedriversdisallowed
 *
 * \note Please call vmk_VMUUIDGetMaxLength() to size the buffer
 *       appropriately.
 *
 * \note VM UUIDs are identification numbers assigned to each VM.
 *
 * \note This UUID can change at run time, in which case the portset is
 *       notified by a VMK_PORT_UPDATE_VMUUID event that can be handled by
 *       the switch implementation in vmk_PortUpdate().
 *
 * \note The caller must hold a handle for the portset
 *       associated with the specified portID.
 *
 * \note This function will not block.
 *
 * \param[in]  portID  Handle to a virtual port.
 * \param[out] uuid    Pointer to a buffer to fill in with the UUID.
 * \param[in]  len     Length of the supplied buffer.
 *
 * \retval VMK_NOT_FOUND        VM UUID could not be found.
 * \retval VMK_NOT_INITIALIZED  No VM UUID has been set for this VM.
 * \retval VMK_BUF_TOO_SMALL    Supplied buffer is too small.
 * \retval VMK_BAD_PARAM        NULL buffer or 0 length.
 * \retval VMK_OK               Success.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortGetVMUUID(vmk_SwitchPortID portID,
                                   vmk_UUID uuid,
                                   vmk_ByteCount len);


/**
 * \brief Event ids of port events
 */
typedef enum vmk_PortEvent {

   /** None. */
    VMK_PORT_EVENT_NONE = 0,

   /** MAC address of a port changes.  */
    VMK_PORT_EVENT_MAC_CHANGE = 1,

   /** VLAN ID of a port changes.  */
    VMK_PORT_EVENT_VLAN_CHANGE = 2,

   /** MTU of a port changes.  */
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

/*
 ***********************************************************************
 * vmk_PortNotifyChange --                                        */ /**
 *
 * \brief Notify the change of a port property.
 *
 * \nativedriversdisallowed
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
 * \retval  VMK_NOT_FOUND                   Could not find the port.
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

/*
 ***********************************************************************
 * vmk_PortUpdateDispatchStats --                                 */ /**
 *
 * \brief Update the port's dispatch stats.
 *
 * \nativedriversdisallowed
 *
 * In normal configurations this function need not be called by the vds
 * implementations. This function will be needed in VXLan type of
 * configuration where vmknic port is used for configuration purpose and
 * its mac address is used to do encapsulation.
 *
 * \note The calling thread must hold immutable handle for the portset
 *       associated with the specified VXLAN vmknic port.
 *
 * \note This function will not block.
 *
 * \param[in]    portID         Numeric ID of a virtual port.
 * \param[in]    pkt            Pkt which is being processed.
 *
 * \retval  VMK_OK         Successfully updated dispatch stats of port.
 * \retval  VMK_BAD_PARAM  portID refers invalid Port
 * \retval  VMK_NOT_FOUND  portID is invalid
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortUpdateDispatchStats(vmk_SwitchPortID portID,
                                             vmk_PktHandle *pkt);

/*
 ***********************************************************************
 * vmk_PortUpdateDispatchStatsDir --                              */ /**
 *
 * \brief Update the port's dispatch stats.
 *
 * \nativedriversdisallowed
 *
 * This function has similar behavior to vmk_PortUpdateDispatchStats(),
 * but is preferable in cases where the caller knows the Pkt direction
 * because it is more effiecient.
 * In normal configurations this function need not be called by the vds
 * implementations. This function will be needed in VXLan type of
 * configuration where vmknic port is used for configuration purpose and
 * its mac address is used to do encapsulation.
 *
 * \note The calling thread must hold immutable handle for the portset
 *       associated with the specified VXLAN vmknic port.
 *
 * \note This function will not block.
 *
 * \param[in]    portID         Numeric ID of a virtual port.
 * \param[in]    pkt            Pkt which is being processed.
 * \param[in]    IsOutbound     Indicate the direction in which Pkt is being
 *                              tranmitted.
 *                              VMK_TRUE  : Pkt being sent to uplink port
 *                              VMK_FALSE : Pkt being sent to backend port
 *
 * \retval  VMK_OK         Successfully updated dispatch stats of port.
 * \retval  VMK_BAD_PARAM  portID refers invalid Port
 * \retval  VMK_NOT_FOUND  portID is invalid
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortUpdateDispatchStatsDir(vmk_SwitchPortID portID,
                                                vmk_PktHandle *pkt,
                                                vmk_Bool IsOutbound);


/*
 ***********************************************************************
 * vmk_PortSetHeadroomLen --                                      */ /**
 *
 * \brief Set port's headroom length.
 *
 * \nativedriversdisallowed
 *
 * Packets allocated by port clients will have that much headroom
 * pre-allocated in the packets.
 *
 * \note The calling thread must hold a mutable handle for the portset
 *       associated with the specified port.
 *
 * \note This function will not block.
 *
 * \param[in]    portID         Numeric ID of a virtual port.
 * \param[in]    headroomLen    Length of headroom requested.
 *
 * \retval  VMK_OK                          Headroom set successfully.
 * \retval  VMK_BAD_PARAM                   portID refers invalid Port
 * \retval  VMK_NOT_FOUND                   portID is invalid
 * \retval  VMK_PORTSET_HANDLE_NOT_MUTABLE  The caller did not hold a
 *                                          mutable handle.
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortSetHeadroomLen(vmk_SwitchPortID portID,
                                        vmk_uint32 headroomLen);

/*
 ***********************************************************************
 * vmk_PortGetHeadroomLen --                                      */ /**
 *
 * \brief Get port's headroom length.
 *
 * \nativedriversdisallowed
 *
 * \note The calling thread must hold a mutable handle for the portset
 *       associated with the specified port.
 *
 * \note This function will not block.
 *
 * \param[in]    portID         Numeric ID of a virtual port.
 * \param[out]   headroomLen    Length of headroom requested.
 *
 * \retval  VMK_OK                          Successfully got headroom
 *                                          length configured for port.
 * \retval  VMK_BAD_PARAM                   portID refers invalid Port
 * \retval  VMK_NOT_FOUND                   portID is invalid
 * \retval  VMK_PORTSET_HANDLE_NOT_MUTABLE  The caller did not hold a
 *                                          mutable handle.
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortGetHeadroomLen(vmk_SwitchPortID portID,
                                        vmk_uint32 *headroomLen);

/*
 ***********************************************************************
 * vmk_PortIsHiddenUplink --                                      */ /**
 *
 * \brief Check if a port is connected to a Hidden Uplink.
 *
 * \nativedriversdisallowed
 *
 * \note The caller must hold a handle for the portset
 *  associated with the specified portID.
 *
 * \note This function will not block.
 *
 * \param[in]  portID            Port identifier
 * \param[out] isHiddenUplink    VMK_TRUE if this is a hidden uplink,
 *                               otherwise VMK_FALSE.
 *
 * \retval     VMK_OK            Success.
 * \retval     VMK_BAD_PARAM     isHiddenUplink is NULL.
 * \retval     VMK_NOT_FOUND     Couldn't find a port with a matching
 *                               port identifier.
 * *********************************************************************
 */
VMK_ReturnStatus vmk_PortIsHiddenUplink(vmk_SwitchPortID portID,
                                        vmk_Bool *isHiddenUplink);

/**
 * \brief Queue features advertised by a port.
 *
 *  A port's queue feature is used by load balancer to decide on what
 *  type of rx queue to allocate. This queue feature is to be
 *  advertised only for non-uplink ports.
 */
typedef enum vmk_PortQueueFeatures {

    /** No feature. */
    VMK_PORT_QUEUE_FEAT_NONE     = 0x0,

    /** HW LRO feature. */
    VMK_PORT_QUEUE_FEAT_LRO      = 0x1,

    /** reserved. */
    VMK_PORT_QUEUE_FEAT_RESERVED = 0x2,

    /** RSS feature. */
    VMK_PORT_QUEUE_FEAT_RSS      = 0x4,
} vmk_PortQueueFeatures;

/*
 ***********************************************************************
 * vmk_PortRequestQueueFeature --                                 */ /**
 *
 * \brief Set port's requested queue feature.
 *
 * \nativedriversdisallowed
 *
 * Port's requested queue feature will be used by load balancer when
 * allocating rx queue. Typically, VDS implmentation will call this API
 * for vmknic ports.
 *
 * \note The calling thread must hold a mutable handle for the portset
 *       associated with the specified port.
 *
 * \note This function will not block.
 *
 * \note This function shouldn't be called for uplink ports.
 *
 * \param[in]    portID         Numeric ID of a virtual port.
 * \param[in]    feature        Feature to be requested.
 *
 * \retval  VMK_OK                          Successfully requested
 *                                          queue features for port.
 * \retval  VMK_BAD_PARAM                   portID refers invalid port.
 * \retval  VMK_NOT_FOUND                   portID is invalid.
 * \retval  VMK_PORTSET_HANDLE_NOT_MUTABLE  The caller did not hold a
 *                                          mutable handle.
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortRequestQueueFeature(vmk_SwitchPortID portID,
                                             vmk_PortQueueFeatures feature);


/*
 ***********************************************************************
 * vmk_PortClearQueueFeature --                                   */ /**
 *
 * \brief Clear port's requested queue feature.
 *
 * \nativedriversdisallowed
 *
 * With this function, port's already requested queue features can be
 * cleared. After port's queue feauture is cleared, load balancer will
 * allocate no-feature rx queue when it allocates new rx queue for that
 * port.
 *
 * \note The calling thread must hold a mutable handle for the portset
 *       associated with the specified port.
 *
 * \note This function shouldn't be called for uplink ports.
 *
 * \note This function will not block.
 *
 * \param[in]    portID         Numeric ID of a virtual port.
 *
 * \retval  VMK_OK                          Successfully cleared queue
 *                                          features of port.
 * \retval  VMK_BAD_PARAM                   portID refers invalid port.
 * \retval  VMK_NOT_FOUND                   portID is invalid.
 * \retval  VMK_PORTSET_HANDLE_NOT_MUTABLE  The caller did not hold a
 *                                          mutable handle.
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortClearQueueFeature(vmk_SwitchPortID portID);


/*
 ***********************************************************************
 * vmk_PortGetQueueFeature --                                     */ /**
 *
 * \brief Get port's requested queue feature.
 *
 * \nativedriversdisallowed
 *
 * \note The calling thread must hold a mutable handle for the portset
 *       associated with the specified port.
 *
 * \note This function shouldn't be called for uplink ports.
 *
 * \note This function will not block.
 *
 * \param[in]    portID         Numeric ID of a virtual port.
 * \param[in]    feature        Feature .
 *
 * \retval  VMK_OK                          Successfully got queue
 *                                          feature of the port.
 * \retval  VMK_BAD_PARAM                   portID refers invalid port.
 * \retval  VMK_NOT_FOUND                   portID is invalid.
 * \retval  VMK_PORTSET_HANDLE_NOT_MUTABLE  The caller did not hold a
 *                                          mutable handle.
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortGetQueueFeature(vmk_SwitchPortID portID,
                                         vmk_PortQueueFeatures *feature);

/**
 * \brief Possible capabilities for the device associated to an port.
 */

typedef enum {
   /** Scatter-gather. */
   VMK_PORT_CLIENT_CAP_SG                =        0x1,

   /** Checksum offloading. */
   VMK_PORT_CLIENT_CAP_IP4_CSUM          =        0x2,

   /** High dma. */
   VMK_PORT_CLIENT_CAP_HIGH_DMA          =        0x8,

   /** TCP Segmentation Offload. */
   VMK_PORT_CLIENT_CAP_TSO               =       0x20,

   /** VLAN tagging offloading on tx path. */
   VMK_PORT_CLIENT_CAP_HW_TX_VLAN        =      0x100,

   /** VLAN tagging offloading on rx path. */
   VMK_PORT_CLIENT_CAP_HW_RX_VLAN        =      0x200,

   /** Scatter-gather span multiple pages. */
   VMK_PORT_CLIENT_CAP_SG_SPAN_PAGES     =    0x40000,

   /** checksum IPv6. **/
   VMK_PORT_CLIENT_CAP_IP6_CSUM          =    0x80000,

   /** TSO for IPv6. **/
   VMK_PORT_CLIENT_CAP_TSO6              =   0x100000,

   /** TSO size up to 256kB. **/
   VMK_PORT_CLIENT_CAP_TSO256k           =   0x200000,

   /** Uniform Passthru, only configurable by uplink device if supported.
       Uplink port shall not explicitly activate/de-activate it. */
   VMK_PORT_CLIENT_CAP_UPT               =   0x400000,

   /** Port client modifies the inet headers for any reason. */
   VMK_PORT_CLIENT_CAP_RDONLY_INETHDRS   =   0x800000,

   /** Encapsulated Packet Offload (eg. vxlan offload). */
   VMK_PORT_CLIENT_CAP_ENCAP              =  0x1000000,

   /**
    * Data Center Bridging, only configurable by uplink port client if
    * supported. Uplink port shall not explicitly activate/de-activate it.
    */
   VMK_PORT_CLIENT_CAP_DCB               =  0x2000000,

   /** TSO/Csum Offloads can be "offset" with an 8 bit value. */
   VMK_PORT_CLIENT_CAP_OFFLOAD_8OFFSET   =  0x4000000,

   /** TSO/Csum Offloads can be "offset" with a 16 bit value. */
   VMK_PORT_CLIENT_CAP_OFFLOAD_16OFFSET  =  0x8000000,

   /** CSUM for IPV6 with extension headers. */
   VMK_PORT_CLIENT_CAP_IP6_CSUM_EXT_HDRS = 0x10000000,

   /** TSO for IPV6 with extension headers. */
   VMK_PORT_CLIENT_CAP_TSO6_EXT_HDRS     = 0x20000000,

   /**
    * Network scheduling capable which basically means capable of
    * interrupt steering and interrupt coalescing control.
    */
   VMK_PORT_CLIENT_CAP_SCHED             = 0x40000000,

   /**
    * Single-Root I/O Virtulaization. Uplink port shall not explicitly
    * activate/de-activate it.
    */
   VMK_PORT_CLIENT_CAP_SRIOV             = 0x80000000UL,

   /** Offload L3/L4 header alignment. */
   VMK_PORT_CLIENT_CAP_OFFLOAD_ALIGN_ANY = 0x1000000000000ULL,

   /**
    * Generic hardware offload (eg. vxlan encapsulation offload and offset
    * based offload).
    */
   VMK_PORT_CLIENT_CAP_GENERIC_OFFLOAD   = 0x2000000000000ULL,
} vmk_PortClientCaps;

/**
 * \brief State of the device associated to a port.
 */

typedef vmk_uint64 vmk_PortClientStates;

typedef enum {

   /** The device associated to a port is present. */
   VMK_PORT_CLIENT_STATE_PRESENT     = 0x1,

   /** The device associated to a port is ready. */
   VMK_PORT_CLIENT_STATE_READY       = 0x2,

   /** The device associated to a port is running. */
   VMK_PORT_CLIENT_STATE_RUNNING     = 0x4,

   /** The device's queue associated to a port is operational. */
   VMK_PORT_CLIENT_STATE_QUEUE_OK    = 0x8,

   /** The device associated to a port is linked. */
   VMK_PORT_CLIENT_STATE_LINK_OK     = 0x10,

   /** The device associated to a port is in promiscious mode. */
   VMK_PORT_CLIENT_STATE_PROMISC     = 0x20,

   /** The device associated to a port supports broadcast packets. */
   VMK_PORT_CLIENT_STATE_BROADCAST   = 0x40,

   /** The device associated to a port supports multicast packets. */
   VMK_PORT_CLIENT_STATE_MULTICAST   = 0x80
} vmk_PortClientState;
#endif /* _VMKAPI_NET_PORT_H_ */
/** @} */
/** @} */
