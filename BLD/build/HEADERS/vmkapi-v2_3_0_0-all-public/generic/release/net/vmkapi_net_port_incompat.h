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

#ifndef _VMKAPI_NET_PORT_INCOMPAT_H_
#define _VMKAPI_NET_PORT_INCOMPAT_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/**
 * \brief Port state update operations (private portion).
 */
typedef enum vmk_PortUpdateOpPriv {
   /** Default VLAN ID update for internal ports */
   VMK_PORT_UPDATE_INTERNAL_VLAN          = 0x8000,

   /** NIC teaming policy update for internal ports */
   VMK_PORT_UPDATE_INTERNAL_TEAM_FRP      = 0x8001,

   /** Advertise unicast MAC address to physical network */
   VMK_PORT_UPDATE_ADVERTISE_UNICAST_ADDR = 0x8002,
} vmk_PortUpdateOpPriv;

/**
 * \brief Type of Ethernet address filter (private portion).
 */
typedef enum vmk_EthFilterFlagsPriv {
   /** Pass unicast frames that do not match other ports */
   VMK_ETH_FILTER_SINK = 0x00010000
} vmk_EthFilterFlagsPriv;

/**
 * \brief Type of etherswitch NIC teaming policy.
 */
typedef enum vmk_ESTeamFRPType {
   /** No teaming policy and all packets to uplinks are dropped */
   VMK_ES_TEAM_FRP_TYPE_NONE,

   /** Load balancing based on a hash value of IP address */
   VMK_ES_TEAM_FRP_TYPE_LB_IP,

   /** Load balancing based on a hash value of source ethernet address */
   VMK_ES_TEAM_FRP_TYPE_LB_SRCMAC,

   /** Load balancing based on a hash value of source port ID */
   VMK_ES_TEAM_FRP_TYPE_LB_SRCID,

   /** User specified NIC failover order */
   VMK_ES_TEAM_FRP_TYPE_FO_EXPLICIT,

   /** Load balancing based on traffic load of source port to uplinks */
   VMK_ES_TEAM_FRP_TYPE_LB_SRCLOAD
} vmk_ESTeamFRPType;

/**
 * \brief Type of etherswitch NIC teaming criteria.
 */
typedef enum vmk_ESTeamFRPCriteria {
   /** Link speed reported by the driver */
   VMK_ES_TEAM_FRP_CRITERIA_LINK_SPEED   = 0x0001,

   /** Link duplex reported by the driver */
   VMK_ES_TEAM_FRP_CRITERIA_LINK_DUPLEX  = 0x0002,

   /** Beason */
   VMK_ES_TEAM_FRP_CRITERIA_BEACON_STATE = 0x0020,

   /** Errors reported by the driver or hw */
   VMK_ES_TEAM_FRP_CRITERIA_LINK_ERR     = 0x0040,

   /** Link state reported by the driver */
   VMK_ES_TEAM_FRP_CRITERIA_LINK_STATE   = 0x0080,

   /** The port is blocked */
   VMK_ES_TEAM_FRP_CRITERIA_PORT_BLOCKED = 0x0100,

   /** Driver has registered the device */
   VMK_ES_TEAM_FRP_CRITERIA_DRV_PRESENT  = 0x0200,
} vmk_ESTeamFRPCriteria;

/**
 * \brief Type of etherswitch NIC teaming flags.
 */
typedef enum vmk_ESTeamFRPFlags {
   /** Apply policy across all available links */
   VMK_ES_TEAM_FRP_FLAG_ALL_AVAILABLE = 0x0001,

   /** Apply policy to inbound frames */
   VMK_ES_TEAM_FRP_FLAG_REVERSE       = 0x0002,

   /** Try "best" link if none is 100% */
   VMK_ES_TEAM_FRP_FLAG_BEST_EFFORT   = 0x0004,

   /** Rolling link restoration, sort by uptime */
   VMK_ES_TEAM_FRP_FLAG_ROLLING       = 0x0008,

   /** Send on all links if none is 100% */
   VMK_ES_TEAM_FRP_FLAG_SHOTGUN       = 0x0010,

   /** Send rarps on connect and failover to notify physical switches */
   VMK_ES_TEAM_FRP_FLAG_NOTIFY_SWITCH = 0x0020,

   /** Block output */
   VMK_ES_TEAM_FRP_FLAG_BLOCK_OUTPUT  = 0x0040,

   /**
    *  LACP client (set when Link Aggregation Group (LAG)
    *  is configured as active teaming adapter)
    */
   VMK_ES_TEAM_FRP_FLAG_LACP_CLIENT   = 0x40000,
} vmk_ESTeamFRPFlags;

/**
 * \brief Type of etherswitch team FRP data (private portion).
 */
typedef struct vmk_ESTeamFRPDataPriv {
   /** Etherswitch NIC teaming policy */
   vmk_ESTeamFRPType           type;

   /** Etherswitch NIC teaming link criteria */
   vmk_ESTeamFRPCriteria       criteria;

   /** Etherswitch NIC teaming flags */
   vmk_ESTeamFRPFlags          flags;

   /** Etherswitch NIC teaming uplink number */
   vmk_uint8                   numUplinks;

   /** Etherswitch NIC teaming max active uplink number */
   vmk_uint8                   maxActive;

   /** Etherswitch link state tracking */
   vmk_Bool                    lst;
} vmk_ESTeamFRPDataPriv;

/**
 * \brief Type of etherswitch team policy configuration (private portion).
 */
typedef struct vmk_ESTeamPolicy {
   /** Etherswitch NIC teaming load balance policy */
   vmk_uint32      loadBalance;

   /** Etherswitch NIC teaming link failover detection policy */
   vmk_uint32      linkCriteria;

   /** Etherswitch NIC teaming link error percentage */
   vmk_uint8       percentError;

   /** Etherswitch NIC teaming link critera check duplex */
   vmk_uint8       fullDuplex;

   /** Etherswitch NIC teaming link critera check speed */
   vmk_uint16      speed;

   /** Etherswitch NIC teaming flags */
   vmk_uint32      flags;

   /** Etherswitch NIC teaming active uplink number */
   vmk_uint32      numActive;

   /** Etherswitch NIC teaming standby uplink number */
   vmk_uint32      numStandby;
} vmk_ESTeamPolicy;

/*
 ***********************************************************************
 * vmk_PortPrivUpdateVlan --                                        */ /**
 *
 * \brief Update the port default VLAN ID.
 *
 * \note This function is used on only ports that were created through
 *       vmk_PortsetConnectPort().
 *
 * \note vlanID can be set to VMK_VLAN_ID_GUEST_TAGGING which denotes
 *       Virtual Machine Guest Tagging (VGT mode).
 *
 * \note This function may be supported only by etherswitch.
 *
 * \note The calling thread must hold a mutable portset handle for the
 *       portset associated with the specified portID.
 *
 * \param[in]  portID             Numeric ID of a virtual port
 * \param[in]  vlanID             VLAN ID
 *
 * \retval  VMK_OK                         The new default VLAN ID is
 *                                         successfully set.
 * \retval  VMK_BAD_PARAM                  vlanID is not a valid VLAN ID.
 * \retval  VMK_NOT_FOUND                  Port doesn't exist.
 * \retval  VMK_NO_PERMISSION              This port was not created
 *                                         using vmk_PortsetConnectPort().
 * \retval  VMK_NOT_SUPPORTED              The operation is not supported
 *                                         by portset implementation.
 * \retval  VMK_PORTSET_HANDLE_NOT_MUTABLE The caller did not hold a
 *                                         mutable handle
 * \retval  Other                          Failed to notify portset of
 *                                         new VLAN ID update.
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortPrivUpdateVlan(vmk_SwitchPortID portID,
                                        vmk_VlanID vlanID);

/*
 ***********************************************************************
 * vmk_PortPrivUpdateESTeamFRP --                                 */ /**
 *
 * \brief Update the port's NIC teaming policy type.
 *
 * \note This function is used on only ports that were created through
 *       vmk_PortsetConnectPort().
 *
 * \note This function may be supported only by etherswitch.
 *
 * \note The calling thread must hold a mutable portset handle for the
 *       portset associated with the specified portID.
 *
 * \param[in]  portID             Numeric ID of a virtual port
 * \param[in]  teamFRPType        Type of teaming policy.
 *
 * \retval  VMK_OK                         The new teaming policy is
 *                                         successfully updated.
 * \retval  VMK_BAD_PARAM                  teamFRPType is not defined
 *                                         in enum vmk_ESTeamFRPType.
 * \retval  VMK_NOT_FOUND                  Port doesn't exist.
 * \retval  VMK_NO_PERMISSION              This port was not created
 *                                         using vmk_PortsetConnectPort().
 * \retval  VMK_NOT_SUPPORTED              The operation is not supported
 *                                         by portset implementation.
 * \retval  VMK_PORTSET_HANDLE_NOT_MUTABLE The caller did not hold a
 *                                         mutable handle
 * \retval  Other                          Failed to notify portset of new
 *                                         teaming FRP type update.
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortPrivUpdateESTeamFRP(vmk_SwitchPortID portID,
                                             vmk_ESTeamFRPType teamFRPType);

/*
 ***********************************************************************
 * vmk_PortIsServicePort --                                       */ /**
 *
 * \brief Return if a port is a service port or not
 *
 * \note The caller must hold at least an immutable handle for the
 *       portset associated with the specified portID.
 *
 * \note This function will not block.
 *
 * \note The client type is only available when port connect is
 *       completed. Query the client during PortConnect callback may
 *       not return proper value.
 *
 * \param[in]  portID            Port identifier
 * \param[out] isServicePort     VMK_TRUE if port is service port,
 *                               VMK_FALSE if not.
 *
 * \retval     VMK_OK            Success.
 * \retval     VMK_FAILURE       The caller does not hold immutable or
 *                               mutable portset handle.
 * \retval     VMK_BAD_PARAM     PortID is VMK_VSWITCH_INVALID_PORT_ID
 *                               or isServicePort is NULL.
 * \retval     VMK_NOT_FOUND     Couldn't find a port with a matching
 *                               port identifier.
 * \retval     VMK_NOT_SUPPORTED If the matching port is vNic or uplink.
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortIsServicePort(vmk_SwitchPortID portID,
                                       vmk_Bool *isServicePort);


/*
 ***********************************************************************
 * vmk_PortSetServicePort --                                      */ /**
 *
 * \brief Change a port to service port
 *
 * \note The caller must hold a mutable handle for the portset
 *       associated with the specified portID.
 *
 * \note This function will not block.
 *
 * \note The client type is only available when port connect is
 *       completed. Query the client during PortConnect callback may
 *       not return proper value.
 *
 * \param[in]  portID            Port identifier
 *
 * \retval VMK_OK                         Success.
 * \retval VMK_BAD_PARAM                  PortID is
 *                                        VMK_VSWITCH_INVALID_PORT_ID.
 * \retval VMK_NOT_SUPPORTED              If the matching port is
 *                                        vNic or uplink.
 * \retval VMK_NOT_FOUND                  Couldn't find a port with a
 *                                        matching port identifier.
 * \retval VMK_PORTSET_HANDLE_NOT_MUTABLE The caller did not hold a
 *                                        mutable handle
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortSetServicePort(vmk_SwitchPortID portID);


/*
 ***********************************************************************
 * vmk_PortClearServicePort --                                    */ /**
 *
 * \brief Change a port from service port to regular port
 *
 * \note The caller must hold a mutable handle for the portset
 *       associated with the specified portID.
 *
 * \note This function will not block.
 *
 * \note The client type is only available when port connect is
 *       completed. Query the client during PortConnect callback may
 *       not return proper value.
 *
 * \param[in]  portID                     Port identifier
 *
 * \retval VMK_OK                         Success.
 * \retval VMK_BAD_PARAM                  PortID is
 *                                        VMK_VSWITCH_INVALID_PORT_ID.
 * \retval VMK_NOT_SUPPORTED              If the matching port is
 *                                        vNic or uplink.
 * \retval VMK_NOT_FOUND                  Couldn't find a port with a
 *                                        matching port identifier.
 * \retval VMK_PORTSET_HANDLE_NOT_MUTABLE The caller did not hold a
 *                                        mutable handle
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortClearServicePort(vmk_SwitchPortID portID);


/*
 ***********************************************************************
 * vmk_PortEnable --                                           */ /**
 *
 * \brief Enable the given port making it ready to send and receive
 *        packet.
 *
 * \note The caller must hold a mutable handle for the portset
 *       associated with the specified portID.
 *
 * \note This function will not block.
 *
 * \param[in]  portID                     Port identifier
 *
 * \retval VMK_OK                         Port is enabled successfully.
 * \retval VMK_IS_ENABLED                 Port is already enabled.
 * \retval VMK_BAD_PARAM                  PortID is
 *                                        VMK_VSWITCH_INVALID_PORT_ID.
 * \retval VMK_FAILURE                    Enabling the port fails.
 * \retval VMK_NOT_FOUND                  Couldn't find a port with a
 *                                        matching port identifier.
 * \retval VMK_PORTSET_HANDLE_NOT_MUTABLE The caller did not hold a
 *                                        mutable handle
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortEnable(vmk_SwitchPortID portID);


/*
 ***********************************************************************
 * vmk_PortDisable --                                          */ /**
 *
 * \brief Disable the given port.
 *
 * \note The caller must hold a mutable handle for the portset
 *       associated with the specified portID.
 *
 * \note This function will not block.
 *
 * \param[in]  portID            Port identifier
 *
 * \retval VMK_OK                         Port is disabled successfully.
 * \retval VMK_IS_DISABLED                Port is already disabled.
 * \retval VMK_BAD_PARAM                  PortID is
 *                                        VMK_VSWITCH_INVALID_PORT_ID.
 * \retval VMK_FAILURE                    Disabling the port fails.
 * \retval VMK_NOT_FOUND                  Couldn't find a port with a
 *                                        matching port identifier.
 * \retval VMK_PORTSET_HANDLE_NOT_MUTABLE The caller did not hold a
 *                                        mutable handle
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortDisable(vmk_SwitchPortID portID);


/*
 ***********************************************************************
 * vmk_PortsBelongToSamePortset --                                */ /**
 *
 * \brief Check if two ports belong to same portset.
 *
 * \note The caller isn't required to hold any handle for the portset
 *       associated with either one of the specified portIDs.
 *
 * \note This function will not block.
 *
 * \param[in]  portID1           Port identifier for the first port.
 * \param[in]  portID2           Port identifier for the second port.
 * \param[out] result            VMK_TRUE if the two ports belong to
 *                               same portset, VMK_FALSE otherwise.
 *
 * \retval     VMK_OK            The check completes successfully.
 * \retval     VMK_BAD_PARAM     One or both port IDs are invalid, or
 *                               result is NULL.
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortsBelongToSamePortset(vmk_SwitchPortID portID1,
                                              vmk_SwitchPortID portID2,
                                              vmk_Bool *result);


/*
 ***********************************************************************
 * vmk_PortPktAllocForTx --                                       */ /**
 *
 * \brief Allocate a TX packet for specific port.
 *
 * Allocate a vmk_PktHandle for TX, preallocating headroom to be used in
 * later data path functions. The headroom length reserved inside the
 * packet handle allocated is determined by parameter headroomLen
 * passed to vmkapi vmk_PortSetHeadroomLen().
 *
 * \note The caller must hold at least an immutable handle for the
 *       portset associated with the specified portID.
 *
 * \note This function will not block.
 *
 * \param[in]  portID            Port identifier to allocate packet for.
 * \param[in]  length            Length of packet to be allocated in
 *                               bytes.
 * \param[out] pkt               Handle to the packet allocated.
 *
 * \retval     VMK_OK            The packet is allocated successfully.
 * \retval     VMK_BAD_PARAM     Invalid portID or pkt pointer provided.
 * \retval     VMK_NOT_FOUND     The port with specific ID is not found.
 * \retval     VMK_NO_MEMORY     VMkernel has insufficient memory to
 *                               fulfill this request.
 * \retval     VMK_FAILURE       The caller does not hold immutable or
 *                               mutable portset handle.
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortPktAllocForTx(vmk_SwitchPortID portID,
                                       vmk_ByteCountSmall length,
                                       vmk_PktHandle **pkt);

#endif /* _VMKAPI_NET_PORT_INCOMPAT_H_ */
/** @} */
/** @} */
