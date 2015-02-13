/***********************************************************
 * Copyright 2008 - 2010 VMware, Inc.  All rights reserved.
 ***********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * VDS                                                           */ /**
 * \addtogroup Network
 * @{
 * \defgroup VDS VDS Portset Interfaces
 *
 * The VDSwitch is an abstraction representation of multiple hosts
 * defining the same vSwitch (same name, same network policy) and
 * portgroup, which are needed to facilitate the concept of VM being
 * connected to the same network as it migrates among multiple hosts.
 *
 * Portsets are groups of ports which, together with policies for frame
 * routing, form virtual networks. Virtual nics connected to the ports
 * may forward packets to each other. The analogy is to a box (like a
 * a hub or a switch) containing some type of backplane for connecting
 * multiple ports in the physical world. If multiple hosts have virtual
 * nics connected to a VDS, each of the hosts should have a portset
 * implementation in ESX.
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_VDS_PORTSET_H_
#define _VMKAPI_VDS_PORTSET_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "net/vmkapi_net_types.h"
#include "net/vmkapi_net_uplink.h"
#include "net/vmkapi_net_dcb.h"
#include "net/vmkapi_net_pt.h"
#include "vds/vmkapi_vds_ether.h"
#include "vds/vmkapi_vds_respools.h"

/**
 * \brief Identifier for a portset.
 */
typedef struct Portset vmk_Portset;

/**
 * \brief Query ops to get portset implementation information.
 */
typedef enum vmk_PortsetQueryID {
   VMK_PORTSET_QUERY_PORTUPLINKPOLICY,
   VMK_PORTSET_QUERY_PORTUPLINKSTATUS,
} vmk_PortsetQueryID;

/**
 * \brief Values returned by a port uplink status request.
 */
typedef enum vmk_PortsetPortUplinkStatus {
   /* All the uplinks that the port uses are link down */
   VMK_PORTSET_PORT_UPLINK_DOWN,

   /* All the uplinks that the port uses are link up */
   VMK_PORTSET_PORT_UPLINK_NORMAL,

   /* Some uplinks that the port uses are link down; at least
    * one uplink that the port uses is link up */
   VMK_PORTSET_PORT_UPLINK_REDUNDANCYLOSS,
} vmk_PortsetPortUplinkStatus;

/**
 * \brief Structure used with VMK_PORTSET_QUERY_PORTUPLINKSTATUS.
 *
 * Used by portset implementation to ask the uplink device status, including
 * link up/down, link speed and duplex.
 */
typedef struct vmk_PortsetQueryPortUplinkStatusData {
   /** the port being asked about */
   vmk_SwitchPortID portID;
   /** (output) uplink status */
   vmk_PortsetPortUplinkStatus *status;
} vmk_PortsetQueryPortUplinkStatusData;

/**
 * \brief Uplink policy flags
 */
typedef enum vmk_UplinkPolicyFlags {
       
   /**
    * Etherchannel static mode is configured on physical switch
    * for the uplinks. (LACP or other negociation are excluded)
    */ 
   VMK_UPLINK_POLICY_ETHERCHANNEL_STATIC = 0x1,
      
} vmk_UplinkPolicyFlags;

/**
 * \brief Uplink attribute.
 *
 *  This attribute indicates whether the uplink is populated
 *  or not.
 */
typedef enum vmk_UplinkAttribute {

   /** Backed by a physical device */
   VMK_UPLINK_ATTRIBUTE_POPULATED = 0x0,

   /** Not backed by a physical device */
   VMK_UPLINK_ATTRIBUTE_UNPOPULATED = 0x1,
} vmk_UplinkAttribute;

/**
 * \brief Structure used with VMK_PORTSET_QUERY_PORTUPLINKPOLICY.
 *
 * Used to ask the portset implementation for the uplink policy information,
 * i.e. uplinks that are associated to the queried port, and flags
 * indicating whether teaming policy requires additional physical switch
 * setting.
 */
typedef struct vmk_PortsetQueryPortUplinkPolicyData {
   /** The port being asked about */
   vmk_SwitchPortID portID;

   /** Uplink policy flag */
   vmk_UplinkPolicyFlags policyFlags;
    
   /** Number of uplinks in policy info (output) */
   vmk_uint32 nUplinks;
    
   /**
    *  Name of each uplink in policy info (output), including
    *  active/standby uplinks.
    *  The user is responsible for both allocating and freeing
    *  the array. The buffer must be large enough to contain
    *  at most uplinkNameArraySz elements.
    */
   vmk_Name *uplinkName;

   /** Flag indicating if the associated uplink is an empty
    *  ('logical') link on a vDS implementation
    */
   vmk_UplinkAttribute *uplinkAttribute;

   /** Number of names that can be stored in uplinkName */
   vmk_uint32 uplinkNameArraySz;
} vmk_PortsetQueryPortUplinkPolicyData;

/**
 * \brief Port filter class.
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
} vmk_PortFilterClass;

/**
 * \brief Filter properties.
 *
 *  These properties are used to classify
 *  a filter. They can be interpreted differently
 *  according to the place they are involved with.
 *  To know more about the way each property is
 *  handled, refer to the used API for example
 *  vmk_UplinkApplyPortFilter() if a filter
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
 *  A port filter is used by portset modules for traffics filtering
 *  purposes. One instance is the load balancer algorithm running in
 *  the uplink layer (refer to vmk_UplinkApplyPortFilter()).
 */
typedef struct vmk_PortFilter {

   /** Port advertising this filter */
   vmk_SwitchPortID portID;

   /** Filter class */
   vmk_PortFilterClass class;

   union {

      /** Filter class mac only */
      vmk_uint8 macaddr[6];

      /** Filter class vlan tag only */
      vmk_uint16 vlan_id;

      /** Filter class vlan tag + mac */
      struct {
         vmk_uint8 macaddr[6];
         vmk_uint16 vlan_id;
      } vlanmac;
   } filter;

   /** Filter properties */
   vmk_PortFilterProperties prop;
} vmk_PortFilter;


/**
 * \brief Port state update operations.
 *
 *  The operations are processed by the portset implementation's
 *  vmk_PortUpdate handler.
 */
typedef enum vmk_PortUpdateOp {
   /** port client link change */
   VMK_PORT_UPDATE_LINK_CHANGE = 1,

   /** vNic port FRP update */
   VMK_PORT_UPDATE_ETH_FRP = 2,

   /** vNic port VLAN update */
   VMK_PORT_UPDATE_VLAN = 3,

   /** port DCB setting change */
   VMK_PORT_UPDATE_DCB_CHANGE = 4,

   /** port VM UUID update */
   VMK_PORT_UPDATE_VMUUID = 5,

   /** Port VF update */
   VMK_PORT_UPDATE_VF = 6,
} vmk_PortUpdateOp;


/**
 * \brief Argument type of PortUpdate for uplink change.
 *
 * This structure is used to give PortUpdate information on
 * VMK_PORT_UPDATE_LINK_CHANGE event concerning a transition
 * that occured on an port.
 */
typedef enum vmk_PortUpdateLinkEvent {

   /* Port client is connected to portset */ 
   VMK_PORT_CLIENT_DISCONNECTED,

   /* Port client is disconnected to portset */
   VMK_PORT_CLIENT_CONNECTED,

   /* Port client is link up */
   VMK_PORT_CLIENT_LINK_UP,

   /* Port client is link down */
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
 */

typedef enum vmk_PortVFOps {
   /** Acquire a VF for a port */
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
   /** The operation requested */
   vmk_PortVFOps  vfOp;
   union {
      /** When VMK_PORT_VF_ACQUIRE operation is requested */
      vmk_PortVFAcquireData acq;
      /** When VMK_PORT_VF_RELEASE operation is requested */
      vmk_PortVFReleaseData rel;
   } u;
} vmk_PortUpdateVFData;


/**
 * \brief Portset Ops flag.
 */
typedef enum vmk_PortsetOpsFlag {

   /** Normal portset activate/deactivate */
   VMK_PORTSETOPS_FLAG_NONE              = 0x0,

   /** Activate/deactivate as part of hotswap operation */
   VMK_PORTSETOPS_FLAG_HOTSWAP           = 0x1,

} vmk_PortsetOpsFlag;

/**
 * \brief Event ids of portset events.
 */
typedef enum vmk_PortsetEvent {

   /** None */
    VMK_PORTSET_EVENT_NONE = 0,

   /**
    * Set number of maximum access ports in a portset. Access ports
    * are used to connect to vmknics and vNICs. Should be called when
    * a portset has limited number of access ports. Typically used
    * when each access port is backed by some hardware resource.
    *
    * If number of maximum access ports is not set for a portset, all
    * ports in the portset are available for vmknic/vNIC connection by
    * default.
    */

   VMK_PORTSET_EVENT_NUMACCESSPORT_CHANGE,

   /** 
    * To notify when the uplinks associated with vmk management interface
    * changes. 
    */

   VMK_PORTSET_EVENT_VMKBACKING_CHANGE,

   /** 
    * An Opaque portset property has changed.
    * Opaque property is partner defined portset property defined in partner
    * solution. For defining opaque portset properties, see
    * vmk_VDSClientPortsetRegister function documentation.
    */
   VMK_PORTSET_EVENT_OPAQUE_CHANGE,

} vmk_PortsetEvent;

/**
 * \brief Client name length.
 */
#define VMK_PORT_CLIENT_NAME_MAX 1040

/**
 * \brief Client display name of a port on a vNetwork Distributed Switch.
 */
typedef char vmk_PortClientName[VMK_PORT_CLIENT_NAME_MAX];

/**
 * \brief Portset name length.
 */
#define VMK_PORTSET_NAME_MAX (32 * 4)

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
 * vmk_PortsetDispatch --                                         */ /**
 *
 * \brief Callback to dispatch packets entering a port.
 *
 * Any packet still left on the pktList when dispatch() returns is
 * released, regardless of success or failure of the function.
 * 
 * \note For performance it is recommended to dispatch to uplinks last
 *       with mayModify=VMK_TRUE when calling vmk_PortOutput() if 
 *       possible. It is likely that an uplink needs to modify the 
 *       packets prior to transmit.
 *
 * \note vmk_PortsetSchedBillToPort() must be used within dispatch().
 *       See the functions documentation for more details.
 *
 * \note This is a required callback.
 *
 * \note This function will not block.
 *
 * \param[in]  ps                Immutable handle to a portset.
 * \param[in]  pktList           List of packets to process
 * \param[in]  portID            Numeric ID of a virtual port
 *
 * \retval     VMK_OK            dispatch was successful
 * \retval     VMK_*             Implementation specific error code,
 *                               to be reported by framework.
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_PortsetDispatch)(vmk_Portset *ps,
                                                vmk_PktList pktList,
                                                vmk_SwitchPortID portID);

/*
 ***********************************************************************
 * vmk_PortsetPortConnect --                                      */ /**
 *
 * \brief Optional callback for port connection.
 *
 * This callback does implementation specific handling of port connection.
 * Set to NULL if not needed.
 *
 * \note This callback cannot block.
 *
 * \param[in]  ps                Mutable handle to a portset.
 * \param[in]  portID            Numeric ID of a virtual port
 *
 * \retval     VMK_OK            Port connected
 * \retval     VMK_*             Implementation specific error code,
 *                               connection is rejected.
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_PortsetPortConnect)(vmk_Portset *ps,
                                                   vmk_SwitchPortID portID);

/*
 ***********************************************************************
 * vmk_PortsetPortDisconnect --                                   */ /**
 *
 * \brief Optional callback for port disconnect.
 *
 * This callback does implementation specific handling of port disconnect.
 * Set to NULL if not needed. Must return VMK_OK if implemented.
 *
 * \note This callback cannot block.
 *
 * \param[in]  ps                Mutable handle to a portset.
 * \param[in]  portID            Numeric ID of a virtual port
 *
 * \retval     VMK_OK            Port disconnected
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_PortsetPortDisconnect)(vmk_Portset *ps,
                                                      vmk_SwitchPortID portID);

/*
 ***********************************************************************
 * vmk_PortsetPortEnable --                                       */ /**
 *
 * \brief Optional callback for port enable.
 *
 * This callback does implementation specific handling of port enable.
 * Set to NULL if not needed. Always invoked after successful port connect
 * and prior to vmk_PortsetDispatch() is called on the port to perform I/O.
 *
 * \note This callback cannot block.
 *
 * \param[in]  ps                Mutable handle to a portset.
 * \param[in]  portID            Numeric ID of a virtual port
 *
 * \retval     VMK_OK            Port is enabled
 * \retval     VMK_*             Implementation specific error code.
 *                               Port is not enabled.
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_PortsetPortEnable)(vmk_Portset *ps,
                                                  vmk_SwitchPortID portID);

/*
 ***********************************************************************
 * vmk_PortsetPortDisable --                                      */ /**
 *
 * \brief Optional callback for port disable.
 *
 * This callback does implementation specific handling of port disable.
 * Set to NULL if not needed. Always invoked after I/O has been stopped
 * on the port.
 *
 * \note  This function must return VMK_OK.
 *
 * \note This function will not block.
 *
 * \param[in]  ps                Mutable handle to a portset.
 * \param[in]  portID            Numeric ID of a virtual port
 *
 * \retval     VMK_OK            Port is disabled
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_PortsetPortDisable)(vmk_Portset *ps,
                                                   vmk_SwitchPortID portID);


/*
 ***********************************************************************
 * vmk_PortsetActivate --                                         */ /**
 *
 * \brief Required callback to activate a portset.
 *
 * This callback does implementation specific handling of portset activation.
 * It is invoked when portset is created, before any virtual NIC connects
 * to the portset.
 *
 * \note This function will not block.
 *
 * \param[in]  ps                Mutable handle to a portset.
 * \param[in]  flag              Reason for activation
 *
 * \retval     VMK_OK            Portset is activated 
 * \retval     VMK_*             Implementation specific error code.
 *                               Portset is not activated.
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_PortsetActivate)(vmk_Portset *ps,
                          vmk_PortsetOpsFlag flag);


/*
 ***********************************************************************
 * vmk_PortsetDeactivate --                                       */ /**
 *
 * \brief Required callback for portset deactivation.
 *
 * This callback does implementation specific handling of portset
 * deactivation. Called as part of portset destruction, after all virtual
 * and physical NICs are disconnected.
 *
 * Must return VMK_OK.
 *
 * \note This callback cannot block.
 *
 * \param[in]  ps                Mutable handle to a portset.
 *
 * \retval     VMK_OK            Portset is activated 
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_PortsetDeactivate)(vmk_Portset *ps,
                                                  vmk_PortsetOpsFlag flag);


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
 * vmk_PortsetQuery --                                            */ /**
 *
 * \brief Optional callback for handling of portset queries.
 *
 * VMK_PORTSET_QUERY_PORTUPLINKPOLICY: queryData is of
 * (vmk_PortsetQueryPortUplinkPolicyData *) type
 *
 * VMK_PORTSET_QUERY_PFLIST: queryData is of
 * (vmk_PortsetQueryPFListData *) type
 *
 * VMK_PORTSET_QUERY_PORTUPLINKSTATUS: queryData is of
 * (vmk_PortsetQueryPortUplinkStatusData *) type
 *
 * \note This function will not block.
 *
 * \param[in]     ps                Mutable handle to a portset.
 * \param[in]     queryID           ID of query being performed
 * \param[in,out] queryData         Query-specific data
 *
 * \retval     VMK_OK               Query succeeded
 * \retval     Other status         Query failed
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_PortsetQuery)(vmk_Portset *ps,
                                             vmk_PortsetQueryID queryID,
                                             void *queryData);

/*
 ***********************************************************************
 * vmk_PortsetPortReserve --                                     */ /**
 *
 * \brief Optional callback for port reservation before port connect.
 *
 * If port connect fails, vmk_PortsetPortUnreserve must be called
 * to cancel reservation. Otherwise the reserved port won't be freed
 * even though no entity is connected to it.
 *
 * vmk_PortsetPortReserve may be called on more than one host for a
 * given vdsPort without vmk_PortsetPortUnreserve being called on the
 * hosts where the vdsPort was previously reserved.
 *
 * \note This function will not block.
 *
 * \param[in]  ps                Mutable handle to a portset.
 * \param[in]  portID            Numeric ID of the virtual port
 *
 * \retval     VMK_OK            If the specified port is successfully reserved
 * \retval     Other status      If the specified port cannot be reserved
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_PortsetPortReserve)(vmk_Portset *ps,
                                                   vmk_SwitchPortID portID);

/*
 ***********************************************************************
 * vmk_PortsetPortUnreserve --                                    */ /**
 *
 * \brief Optional callback for cancelling port reservation.
 *
 * This callback is invoked after a successful vmk_PortsetPortReserve.
 * The port is in disconnected state.
 *
 * \note This callback cannot block.
 *
 * \param[in]  ps                Mutable handle to a portset.
 * \param[in]  portID            Numeric ID of the virtual port
 *
 * \retval     VMK_OK
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_PortsetPortUnreserve)(vmk_Portset *ps,
                                                     vmk_SwitchPortID portID);

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

/*
 * \brief Version number for portset ops structure.
 */
#define VMK_PORTSET_OPS_VERSION_0         0
#define VMK_PORTSET_OPS_VERSION_CURRENT   VMK_PORTSET_OPS_VERSION_0

/**
 * \brief Structure to register portset callbacks.
 */
typedef struct {
   /** Version of portset ops structure; always set to current version */
   vmk_uint32                  version;

   /** Handler for implementation-specific action during portset deactivation */
   vmk_PortsetDeactivate       deactivate;

   /** Handler for implementation-specific action for Pkt dispatch */
   vmk_PortsetDispatch         dispatch;

   /** Handler for implementation-specific action during port connect */
   vmk_PortsetPortConnect      portConnect;

   /** Handler for implementation-specific action during port disconnect */
   vmk_PortsetPortDisconnect   portDisconnect;

   /** Handler for implementation-specific action during port enable */
   vmk_PortsetPortEnable       portEnable;

   /** Handler for implementation-specific action during port disable */
   vmk_PortsetPortDisable      portDisable;

   /** Handler for implementation-specific action during port updates */
   vmk_PortUpdate              portUpdate;

   /** Handler for portset query operations */
   vmk_PortsetQuery            query;

   /** Handler for port reservation before port connect */
   vmk_PortsetPortReserve      portReserve;

   /** Handler for cancel port reservation after port connect fails */
   vmk_PortsetPortUnreserve    portUnreserve;

   /**
    * Handler for getting port statistics, only needed for special
    * uplink ports, i.e. passthroughed hidden uplinks
    */ 
   vmk_PortGetStats            portGetStats;
    
} vmk_PortsetOps;


/**
 * \brief Portset handle context.
 *
 * When requesting a portset handle, the caller specifies a particular
 * context for the handle depending on which actions it wishes to take.
 * In general, the immutable handle is provides read-only, non-exclusive
 * access to the configuration of a portset and its ports, whereas the
 * mutable handle can be used for updating this configuration.
 *
 * \see vmk_PortsetAcquireByName().
 * \see vmk_PortsetAcquireByPortID().
 * \see vmk_PortsetAcquireByVDSID().
 */

typedef enum vmk_PortsetHandleContext {
   /** \brief Handle context with unmodifiable access to the portset. */
   VMK_PORTSET_HANDLE_IMMUTABLE = 1,

   /** \brief Handle context with modifiable access to the portset. */
   VMK_PORTSET_HANDLE_MUTABLE,
} vmk_PortsetHandleContext; 


/*
 ***********************************************************************
 * vmk_PortsetAcquireByName --                                     *//**
 *
 * \brief Acquire a handle to a portset in the requested context.
 *
 * A particular thread may hold at most one portset handle. By
 * extension, this means that a portset implementation may not
 * re-acquire a portset handle, and may not acquire a new portset handle
 * while in a callback where a portset handle is provided as a
 * parameter.
 *
 * Each vmkapi function that requires the caller to hold a portset
 * handle will note this requirement in its doxygen header. If a
 * particular handle context is required, it will be explicitly noted;
 * otherwise, any handle context is acceptable.
 *
 * A thread should not hold a portset handle any longer than necessary.
 * While it holds the handle, it is not blockable and may not sleep or
 * call any blocking function calls. There are several vmkapi function
 * calls that explicitly state that they cannot be called by a thread
 * holding a portset handle.
 *
 * The mutable context should not be used unless absolutely necessary,
 * as it may temporarily halt all traffic traversing the portset.
 *
 * \note The caller must not hold any spin locks while invoking
 *       this function.
 *
 * \note This function will not block.
 *
 * \param[in]  psName   Portset name.
 * \param[in]  context  Requested handle context.
 * \param[out] ps       Portset handle.
 *
 * \retval VMK_OK         Successfully acquired the handle.
 * \retval VMK_BAD_PARAM  Invalid arguments.
 * \retval VMK_NOT_FOUND  The named portset was not found.
 * \retval VMK_FAILURE    The requested portset handle cannot be
 *                        acquired in the current calling context.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortsetAcquireByName(const char *psName,
                                          vmk_PortsetHandleContext context,
                                          vmk_Portset **ps);


/*
 ***********************************************************************
 * vmk_PortsetAcquireByPortID --                                   *//**
 *
 * \brief Acquire a handle to a portset in the requested context.
 *
 * A particular thread may hold at most one portset handle. By
 * extension, this means that a portset implementation may not
 * re-acquire a portset handle, and may not acquire a new portset handle
 * while in a callback where a portset handle is provided as a
 * parameter.
 *
 * Each vmkapi function that requires the caller to hold a portset
 * handle will note this requirement in its doxygen header. If a
 * particular handle context is required, it will be explicitly noted;
 * otherwise, any handle context is acceptable.
 *
 * A thread should not hold a portset handle any longer than necessary.
 * While it holds the handle, it is not blockable and may not sleep or
 * call any blocking function calls. There are several vmkapi function
 * calls that explicitly state that they cannot be called by a thread
 * holding a portset handle.
 *
 * The mutable context should not be used unless absolutely necessary,
 * as it may temporarily halt all traffic traversing the portset.
 *
 * \note The caller must not hold any spin locks while invoking
 *       this function.
 *
 * \note This function will not block.
 *
 * \param[in]  portID   Numeric ID of a virtual port.
 * \param[in]  context  Requested handle context.
 * \param[out] ps       Portset handle.
 *
 * \retval VMK_OK         Successfully acquired the handle.
 * \retval VMK_BAD_PARAM  Invalid arguments.
 * \retval VMK_NOT_FOUND  The port was not found.
 * \retval VMK_FAILURE    The requested portset handle cannot be
 *                        acquired in the current calling context.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortsetAcquireByPortID(vmk_SwitchPortID portID,
                                            vmk_PortsetHandleContext context,
                                            vmk_Portset **ps);

/*
 ***********************************************************************
 * vmk_PortsetAcquireByVDSID --                                    *//**
 *
 * \brief Acquire a handle to a portset in the requested context.
 *
 * A particular thread may hold at most one portset handle. By
 * extension, this means that a portset implementation may not
 * re-acquire a portset handle, and may not acquire a new portset handle
 * while in a callback where a portset handle is provided as a
 * parameter.
 *
 * Each vmkapi function that requires the caller to hold a portset
 * handle will note this requirement in its doxygen header. If a
 * particular handle context is required, it will be explicitly noted;
 * otherwise, any handle context is acceptable.
 *
 * A thread should not hold a portset handle any longer than necessary.
 * While it holds the handle, it is not blockable and may not sleep or
 * call any blocking function calls. There are several vmkapi function
 * calls that explicitly state that they cannot be called by a thread
 * holding a portset handle.
 *
 * The mutable context should not be used unless absolutely necessary,
 * as it may temporarily halt all traffic traversing the portset.
 *
 * \note The caller must not hold any spin locks while invoking
 *       this function.
 *
 * \note This function will not block.
 *
 * \param[in]  vdsID    vDS switch ID string.
 * \param[in]  context  Requested handle context.
 * \param[out] ps       Portset handle.
 *
 * \retval VMK_OK         Successfully acquired the handle.
 * \retval VMK_BAD_PARAM  Invalid arguments.
 * \retval VMK_NOT_FOUND  The vDS switch was not found.
 * \retval VMK_FAILURE    The requested portset handle cannot be
 *                        acquired in the current calling context.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortsetAcquireByVDSID(const char *vdsID,
                                           vmk_PortsetHandleContext context,
                                           vmk_Portset **ps);

/*
 ***********************************************************************
 * vmk_PortsetRelease --                                           *//**
 *
 * \brief Release a handle to a portset.
 *
 * This function should only be called on handles that have been
 * explicitly acquired via vmk_PortsetAcquire().
 *
 * This function must not be used to release a handle that has been
 * provided as an argument to a callback. Such usage is strictly
 * disallowed by vmkernel.
 *
 * \note The caller must not hold any spin locks.
 *
 * \note This function will not block.
 *
 * \param[in,out] ps  Portset handle, which will be reset to NULL.
 *
 ***********************************************************************
 */

void vmk_PortsetRelease(vmk_Portset **ps);

/*
 ***********************************************************************
 * vmk_PortsetAddResPool --                                       */ /**
 *
 * \brief Create a resource pool.
 *
 * \note This function will not block.
 * 
 * \param[in]   psName               Portset name.
 * \param[in]   poolName             Pool name.
 * \param[out]  poolID               Resource pool identifier
 *
 * \retval     VMK_OK                the resource pool has been created
 * \retval     VMK_EXISTS            the resource pool already exists
 * \retval     VMK_LIMIT_EXCEEDED    the limit of resource pools on the host 
 *                                   is reached
 * \retval     VMK_NO_MEMORY         allocation failure
 * \retval     VMK_BAD_PARAM         input params are not valid
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortsetAddResPool(const char *psName,
                                       const char *poolName,
                                       vmk_PortsetResPoolID *poolID);


/*
 ***********************************************************************
 * vmk_PortsetRemoveResPool --                                    */ /**
 *
 * \brief Remove a resource pool.
 *
 * \note This function will not block.
 *
 * \param[in]  poolID                Resource pool identifier
 *
 * \retval     VMK_OK                the resource pool has been removed
 * \retval     VMK_NOT_FOUND         the resource pool does not exist
 * \retval     VMK_BUSY              users are still using the resource pool
 * \retval     VMK_BAD_PARAM         input params are not valid
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortsetRemoveResPool(vmk_PortsetResPoolID poolID);


/*
 ***********************************************************************
 * vmk_PortsetGetResPoolTag --                                    */ /**
 *
 * \brief Retrieve the tag.
 *
 * Retrieves the tag of a given resource pool and increment its
 * associated users reference counter.
 *
 * \note This function will not block.
 *
 * \param[in]  poolID                          Resource pool identifier
 *
 * \retval     vmk_PortsetResPoolTag            resource pool exists
 * \retval     VMK_PORTSET_INVALID_RESPOOL_TAG  otherwise
 * \retval     VMK_BAD_PARAM                    input params are not valid
 ***********************************************************************
 */
vmk_PortsetResPoolTag vmk_PortsetGetResPoolTag(vmk_PortsetResPoolID poolID);


/*
 ***********************************************************************
 * vmk_PortsetPutResPoolTag --                                    */ /**
 *
 * \brief Release the resource pool.
 *
 * Releases the resource pool associated the given tag and decrement
 * the users reference counter.
 *
 * \note This function will not block.
 *
 * \param[in]  poolTag                         Resource pool tag 
 *
 * \retval     VMK_OK                          the resource pool tag is valid
 * \retval     VMK_BAD_PARAM                   input params are not valid
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortsetPutResPoolTag(vmk_PortsetResPoolTag poolTag);


/*
 ***********************************************************************
 * vmk_PortsetPortApplyResPoolCfg --                              */ /**
 *
 * \brief Apply a resource pool configuration to a port. 
 *
 * Old configuraion if existing, is overwritten on success and may be
 * lost on failure. Values outside the range allowed by system max values
 * are substituted with system max values.
 *
 * \note Currently only supported on uplink port, future version
 *       might start supporting it on more port types.
 *
 * \note The caller must hold a mutable handle for the portset
 *       associated with the specified port.
 *
 * \note This function will not block.
 *
 * \param[in]  portID             Numeric ID of a virtual port.
 * \param[in]  poolID             Pool ID of the aimed resource pool.
 * \param[in]  poolCfg            Resource pool configuration.
 *
 * \retval VMK_OK                          The configuration has been
 *                                         applied.
 * \retval VMK_NOT_FOUND                   The resource pool cannot be
 *                                         found.
 * \retval VMK_NO_MEMORY                   Allocation failure.
 * \retval VMK_BAD_PARAM                   Input params are not valid.
 * \retval VMK_FAILURE                     Resource pool scheduling not
 *                                         activated on the port
 * \retval VMK_PORTSET_HANDLE_NOT_MUTABLE  The caller did not hold a
 *                                         mutable handle
 * \retval VMK_NOT_SUPPORTED               Port type is not supported.
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortsetPortApplyResPoolCfg(vmk_SwitchPortID portID, 
                                                vmk_PortsetResPoolID poolID,
                                                vmk_PortsetResPoolCfg *poolCfg);


/*
 ***********************************************************************
 * vmk_PortsetPortRemoveResPoolCfg --                             */ /**
 *
 * \brief Remove a resource pool configuration from a port.
 *
 * \note Currently only supported on uplink port, future version
 *       might start supporting it on more port types.
 *
 * \note The caller must hold a mutable handle for the portset
 *       associated with the specified port.
 *
 * \note This function will not block.
 *
 * \param[in]  portID             Numeric ID of a virtual port.
 * \param[in]  poolID             Pool ID of the aimed resource pool.
 *
 * \retval VMK_OK                          The configuration has been
 *                                         removed.
 * \retval VMK_NOT_FOUND                   The resource pool cannot be
 *                                         found.
 * \retval VMK_BAD_PARAM                   Input params are not valid.
 * \retval VMK_FAILURE                     Resource pool scheduling not
 *                                         activated on the port
 * \retval VMK_PORTSET_HANDLE_NOT_MUTABLE  The caller did not hold a
 *                                         mutable handle 
 * \retval VMK_NOT_SUPPORTED               Port type is not supported.
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortsetPortRemoveResPoolCfg(vmk_SwitchPortID portID,
                                                 vmk_PortsetResPoolID poolID);


/*
 ***********************************************************************
 * vmk_PortsetPortGetResPoolStats --                              */ /**
 *
 * \brief Get resource pool stats.
 *
 * \note Currently only supported on uplink port, future version
 *       might start supporting it on more port types.
 *
 * \note The caller must hold a mutable handle for the portset
 *       associated with the specified port.
 *
 * \note This function will not block.
 *
 * \param[in]      portID       Numeric ID of a virtual port.
 * \param[in]      poolID       Pool ID of the aimed resource pool.
 * \param[out]     poolStats    Resource pool stats.
 *
 * \retval VMK_OK                          Stats successfully returned.
 * \retval VMK_NOT_FOUND                   The resource pool cannot be
 *                                         found.
 * \retval VMK_BAD_PARAM                   Input params are not valid.
 * \retval VMK_FAILURE                     Resource pool scheduling not
 *                                         activated on the port
 * \retval VMK_PORTSET_HANDLE_NOT_MUTABLE  The caller did not hold a
 *                                         mutable handle
 * \retval VMK_NOT_SUPPORTED               Port type is not supported.
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortsetPortGetResPoolStats(vmk_SwitchPortID portID,
                                                vmk_PortsetResPoolID poolID,
                                                vmk_PortsetResPoolStats *poolStats);


/*
 ***********************************************************************
 * vmk_PortsetActivateResPoolsSched --                            */ /**
 *
 * \brief Activate resource pools scheduling on portset.
 *
 * \note This function will not block.
 *
 * \param[in]  ps                 Mutable portset handle.
 *
 * \retval VMK_OK                          Resource pools scheduling
 *                                         activated.
 * \retval VMK_NO_MEMORY                   Allocation failure.
 * \retval VMK_BAD_PARAM                   Input params are not valid
 * \retval VMK_NOT_FOUND                   Portset not found.
 * \retval VMK_PORTSET_HANDLE_NOT_MUTABLE  The caller did not hold a
 *                                         mutable handle
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortsetActivateResPoolsSched(vmk_Portset *ps);


/*
 ***********************************************************************
 * vmk_PortsetDeactivateResPoolsSched --                          */ /**
 *
 * \brief Deactivate resource pools scheduling on portset. Applied
 *        resource pools configuration on the ports won't be persisted.
 *
 * \note This function will not block.
 *
 * \param[in]  ps                  Mutable portset handle.
 *
 * \retval VMK_OK                          Resource pools scheduling
 *                                         activated.
 * \retval VMK_NO_MEMORY                   Allocation failure.
 * \retval VMK_BAD_PARAM                   Input params are not valid
 * \retval VMK_NOT_FOUND                   Portset not found.
 * \retval VMK_PORTSET_HANDLE_NOT_MUTABLE  The caller did not hold a
 *                                         mutable handle
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortsetDeactivateResPoolsSched(vmk_Portset *ps);


/*
 ***********************************************************************
 * vmk_PortsetPortActivateResPoolsSched --                        */ /**
 *
 * \brief Activate resource pools scheduling on a port.
 *
 * \note Currently only supported on uplink port, future version
 *       might start supporting it on more port types.
 *
 * \note The caller must hold a mutable handle for the portset
 *       associated with the specified port.
 *
 * \note This function will not block.
 *
 * \param[in]  portID             Numeric ID of a virtual port
 *
 * \retval VMK_OK                          Resource pools scheduling
 *                                         activated.
 * \retval VMK_BAD_PARAM                   Input params are not valid.
 * \retval VMK_NO_MEMORY                   Allocation failure.
 * \retval VMK_NOT_SUPPORTED               Port type is not supported.
 * \retval VMK_NOT_FOUND                   The specified port was not
 *                                         found.
 * \retval VMK_PORTSET_HANDLE_NOT_MUTABLE  The caller did not hold a
 *                                         mutable handle
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortsetPortActivateResPoolsSched(vmk_SwitchPortID portID);


/*
 ***********************************************************************
 * vmk_PortsetPortDeactivateResPoolsSched --                      */ /**
 *
 * \brief Deactivate resource pools.
 *
 * This function deactivates resource pools scheduling on a port.
 * Resource pools configuration on the port won't be persisted.
 *
 * \note Currently only supported on uplink port, future version
 *       might start supporting it on more port types.
 *
 * \note The caller must hold a mutable handle for the portset
 *       associated with the specified port.
 *
 * \note This function will not block.
 *
 * \param[in]  portID            Numeric ID of a virtual port
 *
 * \retval VMK_OK                          Resource pools scheduling
 *                                         activated.
 * \retval VMK_BAD_PARAM                   Input params are not valid.
 * \retval VMK_NO_MEMORY                   Allocation failure.
 * \retval VMK_NOT_SUPPORTED               Port type is not supported.
 * \retval VMK_NOT_FOUND                   The specified port was not
 *                                         found.
 * \retval VMK_PORTSET_HANDLE_NOT_MUTABLE  The caller did not hold a
 *                                         mutable handle
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortsetPortDeactivateResPoolsSched(vmk_SwitchPortID portID);


/*
 ***********************************************************************
 * vmk_PortsetSize --                                             */ /**
 *
 * \brief Retrieve the number of ports in a portset.
 *
 * \note This function will not block.
 * 
 * \param[in]  ps                Handle to a portset
 *
 * \retval     vmk_uint32        Number of ports in portset, or 0 on
 *                               failure.
 ***********************************************************************
 */
vmk_uint32 vmk_PortsetSize(vmk_Portset *ps);


/*
 ***********************************************************************
 * vmk_PortsetNotifyChange --                                 */ /**
 *
 * \brief Send an event to notify the change of a portset property.
 *
 * It is not required that all runtime property updates need accompanying
 * events, the guidelines to generate events should be -
 * 1) if there is a user visible screen for the property.
 * 2) if there is some reason that the state needs to be persisted right
 *    away.
 *
 * \note This function will not block.
 *
 * \param[in]  ps              Handle to a portset
 * \param[in]  eventType       Type of event
 * \param[in]  data            Data for the event (only for
 *                             VMK_PORTSET_EVENT_NUMACCESSPORT_CHANGE)
 *
 * \retval     VMK_OK              Event notified successfully.
 * \retval     VMK_NOT_FOUND       Portset cannot be found.
 * \retval     VMK_NO_MEMORY       Could not allocate memory.
 * \retval     VMK_LIMIT_EXCEEDED  Data longer than expected.
 * \retval     VMK_NOT_SUPPORTED   Event type not supported.
 * \retval     VMK_BAD_PARAM       NumAccessPorts is bigger than portset size
 *                                 or numAccessPorts is less than number of
 *                                 connected access ports or numAccessPorts
 *                                 is zero.
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortsetNotifyChange (vmk_Portset *ps,
                                          vmk_uint32 eventType,
                                          void *data);


/*
 ***********************************************************************
 * vmk_PortsetPortIndex --                                        */ /**
 *
 * \brief Retrieve the index of a port in the portset.
 *
 * \note The caller must hold a handle for the portset
 *       associated with the specified port.
 *
 * \note This function will not block.
 *
 * \param[in]  portID            Numeric ID of the virtual port
 * \param[out] index             Index of the port on its portset.
 *
 * \retval     VMK_OK            Success.
 * \retval     VMK_NOT_FOUND     There is no port with the given portID.
 * \retval     VMK_BAD_PARAM     Invalid portID or index is NULL.
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortsetPortIndex(vmk_SwitchPortID portID,
                                      vmk_uint32 *index);


/*
 ***********************************************************************
 * vmk_PortsetGetPortFromIndex --                                 */ /**
 *
 * \brief Retrieve the port in the portset given an index
 *
 * \note This function will not block.
 *
 * \param[in]   ps                Handle to a portset.
 * \param[in]   index             The index of the port in the portset
 * \param[out]  portID            Numeric ID of the virtual port
 *
 * \retval      VMK_OK            Success.
 * \retval      VMK_BAD_PARAM     Either ps or portID is NULL.
 * \retval      VMK_NOT_FOUND     The index is out of bounds for this
 *                                portset.
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortsetGetPortFromIndex(vmk_Portset *ps,
                                             vmk_uint32 index,
                                             vmk_SwitchPortID *portID);


/*
 ***********************************************************************
 * vmk_PortFunction --                                            */ /**
 *
 * \brief Callback type for function called by vmk_PortsetForAllPorts.
 *
 * \note The portset handle is the same one that was passed to
 *       vmk_PortsetForAllPorts().
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
 * vmk_PortsetForAllPorts --                                      */ /**
 *
 * \brief Iterate over all in-use ports.
 *
 * This function iterates over all in-use ports on the specified portset,
 * calling the specified function on each port.
 *
 * \note This function will abort port traversal if func returns any
 *       value other than VMK_OK.
 *
 * \note This function will not block.
 *
 * \param[in]   ps               Handle to a portset
 * \param[in]   func             Operation to be performed
 * \param[in]   arg              User-specified callback context cookie,
 *                               passed to func for each port
 *
 * \retval      VMK_OK           If func returns VMK_OK on all ports.
 * \retval      VMK_BAD_PARAM    Either ps or func is NULL.
 * \retval      Other status     Implementation specific error code from
 *                               func.
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortsetForAllPorts(vmk_Portset *ps,
                                        vmk_PortFunction *func,
                                        void *arg);


/*
 ***********************************************************************
 * vmk_PortsetName --                                             */ /**
 *
 * \brief Retrieve the portset name.
 *
 * \note This function will not block.
 *
 * \param[in]   ps                Handle to a portset
 *
 * \retval      char*             Portset name string
 ***********************************************************************
 */
char *vmk_PortsetName(vmk_Portset *ps);


/*
 ***********************************************************************
 * vmk_PortsetPrivDataSet --                                      */ /**
 *
 * \brief Set per-portset private data
 *
 * Only one private data can be set in each portset.
 * 
 * Any module using this API must destroy the private data and call
 * vmk_PortsetPrivDataSet to set portset private data to NULL, if
 * private data is no longer used (i.e. before setting a new privData,
 * during portset deactivate).
 *
 * \note The caller must hold a mutable portset handle.
 *
 * \note This function will not block.
 *
 * \param[in]  ps             Mutable handle to a portset
 * \param[in]  data           Memory location of private data
 *
 * \retval  VMK_OK                          Private data is
 *                                          successfully set
 * \retval  VMK_PORTSET_HANDLE_NOT_MUTABLE  The caller did not hold a
 *                                          mutable handle
 * \retval  VMK_EXISTS                      Private data is already set
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortsetPrivDataSet(vmk_Portset *ps, void *data);


/*
 ***********************************************************************
 * vmk_PortsetPrivDataGet --                                      */ /**
 *
 * \brief Get per-portset private data
 *
 * \note This function will not block.
 *
 * \param[in]  ps             Handle to a portset
 *
 * \retval     void *         Pointer to memory location of private data
 ***********************************************************************
 */
void *vmk_PortsetPrivDataGet(vmk_Portset *ps);


/*
 ***********************************************************************
 * vmk_PortPrivDataSet --                                         */ /**
 *
 * \brief Set per-port private data.
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


/*
 ***********************************************************************
 * vmk_PortsetOpsSet --                                           */ /**
 *
 * \brief Setup portset callbacks.
 *
 * \note This function will not block.
 *
 * \param[in]  ps             Mutable handle to a portset.
 * \param[in]  ops            Pointer to structure containing callbacks
 *
 * \retval VMK_OK             Success.
 * \retval VMK_BAD_PARAM      Either ps or ops is NULL.
 * \retval VMK_NOT_SUPPORTED  The version specified in ops is not
 *                            compatible with vmkernel.
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortsetOpsSet(vmk_Portset *ps,
                                   const vmk_PortsetOps *ops);


/*
 ***********************************************************************
 * vmk_PortsetClassRegister --                                    */ /**
 *
 * \brief Register a portset class.
 *
 * \note The caller must not hold any locks or portset handles.
 *
 * \note This function will not block.
 *
 * \param[in]  psClass        A string identifying the implementation
 * \param[in]  activateFcn    The activation callback for this class
 *
 * \retval     VMK_OK             
 * \retval     VMK_EXISTS       The psClass is already registered            
 * \retval     VMK_NO_RESOURCES No room for new portset classes
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortsetClassRegister(char *psClass,
                                          vmk_PortsetActivate activateFcn);


/*
 ***********************************************************************
 * vmk_PortsetClassUnregister --                                   */ /**
 *
 * \brief Register a portset class.
 *
 * \note The caller must not hold any locks or portset handles.
 *
 * \note This function will not block.
 *
 * \param[in]  psClass        The implementaiton class to unregister
 *
 * \retval     VMK_OK
 * \retval     VMK_NOT_FOUND  psClass not registered
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortsetClassUnregister(char *psClass);


/*
 ***********************************************************************
 * vmk_PortOutput --                                              */ /**
 *
 * \brief Send a list of packets out of a port.
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
 * vmk_PortUpdateEthFRP --                                        */ /**
 *
 * \brief Update ethernet frame routing policy.
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
 * \param[in]  portID             Numeric ID of a virtual port
 * \param[in]  ethFilter          Ethernet address filter
 *                                
 * \retval     VMK_OK             The new FRP is successfully installed.
 * \retval     VMK_BAD_PARAM      ethFilter is not a valid ethernet
 *                                address filter.
 * \retval     VMK_NOT_FOUND      Port doesn't exist.
 * \retval     VMK_NO_MEMORY      Allocation failure.
 * \retval     VMK_NO_PERMISSION  This port was not created using
 *                                vmk_PortsetConnectPort().
 * \retval     Other              Failed to notify portset of new FRP.
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortUpdateEthFRP(vmk_SwitchPortID portID,
                                      vmk_EthFilter *ethFilter);

/*
 ***********************************************************************
 * vmk_PortGetVMUUID --                                           */ /**
 *
 * \brief Query the VM UUID of the VM the port is attached to.
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


/*
 ***********************************************************************
 * vmk_PortsetClaimHiddenUplink --                                */ /**
 *
 * \brief Claim hidden uplink devices.
 *
 * Hidden uplink devices are uplink devices used by portset implementation
 * to perform I/O, but are not visible to management plane.
 *
 * Vmkernel will connect the claimed hidden uplinks to the portset.
 * Vmkernel assumes that only one portset instance will claim hidden
 * uplinks.
 *
 * \note Only hidden devices with VMK_UPLINK_CAP_UPT capability are
 *       allowed to be claimed. This API is only for certain devices
 *       that expose hidden uplinks to vmkernel. Regular
 *       uplinks must be connected through the management.
 *
 * \note This function should be called in the portset activate
 *       callback before vNIC/vmknic are connected to portset.
 *
 * \note During portset deactivate, the claimed hidden uplinks will be
 *       disconnected by vmkernel. No additional call is needed.
 *
 * \note This function will not block.
 *
 * \param[in]  ps               Mutable handle to a portset.
 * \param[in]  uplinkCap        Capabilities of hidden uplinks. 
 *
 * \retval VMK_BAD_PARAM        Ps is NULL or uplinkCap is zero
 * \retval VMK_FAILURE          No hidden uplink successfully connected
 *                              to the portset
 * \retval VMK_NOT_SUPPORTED    UplinkCap is not accepted for claiming
 * \retval VMK_OK               One or more hidden uplinks succeed to
 *                              connect to portset
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortsetClaimHiddenUplink(vmk_Portset *ps,
                                              vmk_uint32 uplinkCap);

/*
 ***********************************************************************
 * vmk_PortsetFindUplinkPort --                                   */ /**
 *
 * \brief Find the first connected uplink port.
 *
 * \note This function will not block.
 *
 * \param[in]   ps            Handle to a portset
 * \param[out]  portID        Numeric port ID of the uplink port. 
 *
 * \retval     VMK_OK         Success.
 * \retval     VMK_NOT_FOUND  No connected team uplink port.
 * \retval     VMK_BAD_PARAM  An argument is NULL.
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortsetFindUplinkPort(vmk_Portset *ps,
                                           vmk_SwitchPortID *portID);


/*
 ***********************************************************************
 * vmk_PortsetReserveMgmtPort --                                  */ /**
 *
 * \brief Reserve an internal management port.
 *
 * \note This function will not block.
 *
 * \param[in]  ps                Mutable handle to a portset
 *
 * \retval     VMK_OK            Success.
 * \retval     VMK_BAD_PARAM     The ps argument is NULL.
 * \retval     Other status      Reserving/connecting the management
 *                               port failed.
 ***********************************************************************
 */
VMK_ReturnStatus  vmk_PortsetReserveMgmtPort(vmk_Portset *ps);


/*
 ***********************************************************************
 * vmk_PortsetGetMgmtPort --                                      */ /**
 *
 * \brief Retrieve the management port on a portset
 *
 * \note This function will not block.
 *
 * \param[in]  ps                Handle to a portset.
 * \param[out] portID            Numeric ID of a virtual port.
 *
 * \retval     VMK_OK            Success.
 * \retval     VMK_BAD_PARAM     Either ps or portID is NULL.
 * \retval     VMK_NOT_FOUND     No management port reserved on this
 *                               portset.
 * \retval     VMK_FAILURE       The caller did not hold immutable or
 *                               mutable portset handle
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortsetGetMgmtPort(vmk_Portset *ps,
                                        vmk_SwitchPortID *portID);


/*
 ***********************************************************************
 * vmk_PortsetPortRxFunc --                                       */ /**
 *
 * \brief Callback that is invoked upon reception of packets on a port.
 *
 * The callback is free to perform any modification to the passed in
 * pktList and the packets contained within it.
 *
 * Any packet still left on the pktList when this function returns is
 * released, regardless of success or failure of the function.
 *
 * \note This function will not block.
 *
 * \param[in]  ps           Immutable handle to a portset.
 * \param[in]  portID       Numeric ID of the port.
 * \param[in]  fxFuncData   Private data that was provided to 
 *                          vmk_PortsetConnectPort().
 * \param[in]  pktList      List of received packets.
 *
 * \return VMK_ReturnStatus  Ignored.
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_PortsetPortRxFunc)(vmk_Portset *ps,
                                                  vmk_SwitchPortID portID,
                                                  void *rxFuncData,
                                                  vmk_PktList pktList);

/*
 ***********************************************************************
 * vmk_PortsetConnectPort --                                      */ /**
 *
 * \brief Connect a port to the given portset and enable the port.
 *
 * The port starts enabled, but possibly blocked by the portset. The
 * portset is free to block the port at any time. The portset is 
 * expected to honor the ports configuration and forward frames that
 * match the Frame Routing Policy (FRP) of the port. However, the 
 * portset MAY choose to not honor it to satisfy stricter security
 * policies. In the desire to improve interoperability, portsets
 * MUST offer a way to turn off such a stricter security policies.
 *
 * \note The VMkernel is allowed to interpose processing of the packets 
 *       between the port client and the portset (e.g. DVFilters)
 * 
 * \note The fixedAddr value is just informational. Injected packets may
 *       use a different source MAC address, but the portset may use
 *       the fixedAddr to enforce strict security policies. Also, the
 *       portset is free to ignore this MAC address when delivering
 *       packets to this port client. The value of fixedAddr is used to
 *       set the initial FRP (see vmk_PortUpdateEthFRP()), but the FRP
 *       can be changed later.
 *
 * \note No multicast or broadcast addresses are allowed for fixedAddr 
 *
 * \note May be called during vmk_PortsetActivate()
 *
 * \note While the port can be disconnected explicitly by calling
 *       vmk_PortsetDisconnectPort(), it will also automatically be
 *       disconnected if the portset is destroyed. In order to properly
 *       manage the lifetime of rxFuncData, it is recommended to use a 
 *       port property and call vmk_PortsetDisconnectPort() during its 
 *       clear() callback to do the cleanup.
 *
 * \note This function will not block.
 *
 * \param[in]  ps             Mutable handle to a portset.
 * \param[in]  clientName     Client name of the port. Used for display
 *                            purposes only. Must not be NULL.
 * \param[in]  fixedAddr      MAC address of this port client. Injected
 *                            packets may use different source MAC 
 *                            addresses. 
 * \param[in]  rxFunc         Callback that is invoked when the port 
 *                            client receives packets
 * \param[in]  rxFuncData     This value is passed back to the rxFunc
 *                            on every invocation
 * \param[out] portID         Pointer to vmk_SwitchPortID, to be updated
 *                            with the portID of the newly connect port
 *
 * \retval     VMK_OK         Successfully connected to port
 * \retval     VMK_BAD_PARAM  Invalid parameter supplied
 * \retval     Other status   Failed to connect to port
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortsetConnectPort(vmk_Portset *ps,
                                        vmk_PortClientName clientName,
                                        vmk_EthAddress fixedAddr,
                                        vmk_PortsetPortRxFunc rxFunc,
                                        void *rxFuncData,
                                        vmk_SwitchPortID *portID);

/*
 ***********************************************************************
 * vmk_PortsetDisconnectPort --                                      */ /**
 *
 * \brief Disonnect a port to the given portset.
 *
 * This function may be used on ports that were created through the
 * vmk_PortsetConnectPort() API. It's typically called at VDS destroy time.
 *
 * \note The caller must hold a mutable portset handle for the
 *       portset associated with the specified portID.
 *
 * \note This function will not block.
 *
 * \param[in]  portID         Numeric ID of a virtual port.
 *
 * \retval VMK_OK             Success.
 * \retval VMK_NO_PERMISSION  This port was not created using
 *                            vmk_PortsetConnectPort().
 * \retval Other status       Failed to disconnect to port.
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortsetDisconnectPort(vmk_SwitchPortID portID);

/*
 ***********************************************************************
 * vmk_PortsetSchedSamplingActive --                              */ /**
 *
 * \brief Get whether system service accounting is active.
 *
 * \note This function will not block.
 *
 * \retval     VMK_TRUE       system service accounting is active
 * \retval     VMK_FALSE      system service accouting is inactive
 ***********************************************************************
 */

vmk_Bool vmk_PortsetSchedSamplingActive(void);

/*
 ***********************************************************************
 * vmk_PortsetSchedBillToPort --                                  */ /**
 *
 * \brief Set the current port that is billed for Rx processing.
 *
 * In order for Rx packet processing to be accounted properly, the portset
 * implementation must use this API to notify the scheduling subsystem
 * which port to bill for all the Rx processing.
 *
 * \note This function must be called inside vmk_PortsetOps's dispatch
 *       callback. After dispatching all packets, dispatch callback can
 *       use a random algorithm to select a port from all destination
 *       ports of packets, and provide the selected portID to this function.
 *
 * \note Before calling this function, vmk_PortsetSchedSamplingActive()
 *       must be called to check whether system account is active. If
 *       sampling is not active, there is no need to call
 *       vmk_PortsetSchedBillToPort(). 
 * 
 * \note The caller must hold a handle for the portset
 *       associated with the specified portID.
 *
 * \note This function will not block.
 *
 * \param[in]  portID         Numeric ID of a virtual port
 * \param[in]  totalNumPkts   Number of packets currently being processed
 *
 * \retval     VMK_OK         Successfully billed packet count to port.
 * \retval     VMK_NOT_FOUND  Port was not found.
 * \retval     VMK_BAD_PARAM  The totalNumPkts argument is 0.
 * \retval     Other status   Failed to bill packet count to port.
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortsetSchedBillToPort(vmk_SwitchPortID portID,
                                            vmk_uint64 totalNumPkts);


/*
 ***********************************************************************
 * vmk_PortBlock --                                             */ /**
 *
 * \brief Disallow I/O traffic through a port.
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
 * \retval     VMK_OK         Operation was successful
 * \retval     VMK_NO_MEMORY  Insufficient memory to complete the operation
 ***********************************************************************
 */
VMK_ReturnStatus  vmk_PortBlock(vmk_SwitchPortID portID);


/*
 ***********************************************************************
 * vmk_PortUnblock --                                             */ /**
 *
 * \brief Allow I/O traffic through a port.
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
 * \retval     VMK_OK             
 * \retval     VMK_NO_MEMORY  Insufficient memory to complete the operation
 ***********************************************************************
 */
VMK_ReturnStatus  vmk_PortUnblock(vmk_SwitchPortID portID);


/*
 ***********************************************************************
 * vmk_PortGetFRP --                                              */ /**
 *
 * \brief Retrieve ethernet frame routing policy on a port.
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
 * vmk_PortIsOutputActive --                                      */ /**
 *
 * \brief Check if a port permit outgoing traffic.
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
 * \note The caller must hold a mutable handle for the portset
 *       associated with the specified portID.
 *
 * \note This function will not block.
 *
 * \param[in]  portID         Numeric ID of a virtual port
 * \param[in]  uplink         Pointer to hold the associated uplink
 *
 * \retval     VMK_OK         Successfully found underlying uplink
 * \retval     VMK_BAD_PARAM  if the port is not connected to an uplink
 * \retval     VMK_NOT_FOUND  if the port id is not valid
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortGetUplink(vmk_SwitchPortID portID,
                                   vmk_Uplink *uplink);


/*
 ***********************************************************************
 * vmk_PortLinkStatusSet --                                       */ /**
 *
 * \brief Set link status of vNic connected to a port.
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
 * \retval     VMK_OK         Success.
 * \retval     VMK_NOT_FOUND  The specified port was not found.
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortLinkStatusSet(vmk_SwitchPortID portID,
                                       vmk_Bool linkUp);


/*
 ***********************************************************************
 * vmk_PortClientNameGet --                                       */ /**
 *
 * \brief Get name of the client connected to the port.
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


/**
 * \brief Handle for a vDS Portset Client.
 */

typedef struct VDSPortsetClient *vmk_VDSPortsetClient;

/** 
 * \brief Flags passed to VDSClient Portset/Port ops.
 */
typedef enum {

   /** None */
   VMK_VDS_CLIENT_OPS_FLAG_NONE    = 0x0,

   /** Ops flags for calls within an initialization */
   VMK_VDS_CLIENT_OPS_FLAG_INIT    = 0x1,

   /** Ops flags for calls within a cleanup */
   VMK_VDS_CLIENT_OPS_FLAG_CLEANUP = 0x2,
} vmk_VDSClientOpsFlags;


/*
 ***********************************************************************
 * vmk_VDSPortsetClientWrite --                                   */ /**
 *
 * \brief Write callback for a portset property of a VDS Portset client.
 *
 * \note This function will not block.
 *
 * \param[in]   ps             Mutable portset handle.
 * \param[in]   dataName       Data name (name of portset property)
 * \param[in]   data           Data to set for name
 * \param[in]   dataLen        Length of data
 * \param[in]   flags          Flags
 *
 * \retval      VMK_OK         Portset client accepts value written to dataName
 * \retval      Other status   Portset client does not accept change of dataName value
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_VDSPortsetClientWrite)(
   vmk_Portset *ps,
   char *dataName,
   void *data, int dataLen,
   vmk_VDSClientOpsFlags flags);


/*
 ************************************************************************
 * vmk_VDSPortsetClientBlockableWrite --                           */ /**
 *
 * \brief Write callback for a portset property of a VDS Portset client.
 *
 * This callback is invoked in a blockable context, but it should not
 * take more than a reasonable amount of time (a few seconds). This
 * callback can only be implemented on selected properties defined
 * by VMware.
 *
 * \note vDS properties may be queried from this callback, but they may
 *       not be set or cleared.
 *
 * \note This function may block.
 *
 * \param[in]   psName         Portset name.
 * \param[in]   dataName       Data name (name of portset property)
 * \param[in]   data           Data to set for name
 * \param[in]   dataLen        Length of data
 * \param[in]   flags          Flags
 *
 ************************************************************************
 */

typedef void (*vmk_VDSPortsetClientBlockableWrite)(const char *psName,
                                                   char *dataName,
                                                   void *data,
                                                   int dataLen,
                                                   vmk_VDSClientOpsFlags flags);

/*
 ***********************************************************************
 * vmk_VDSPortsetClientClear --                                   */ /**
 *
 * \brief Clear callback for a portset property of a VDS Portset client.
 *
 * \note This callback is invoked from a non-blockable context, even for
 *       blockable properties.
 *
 * \note This function will not block.
 *
 * \param[in]   ps             Mutable portset handle.
 * \param[in]   dataName       Data name (name of portset property)
 *
 * \retval      VMK_OK         Portset client accepts clear of property value
 * \retval      Other status   Portset client does not accept clear of property value
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_VDSPortsetClientClear)(vmk_Portset *ps,
                                                      char *dataName);


/**
 * \brief Argument structure for registering portset property handlers.
 */

typedef struct vmk_VDSPortsetClientOps {
   /** handler for writes to non-blockable portset properties */
   vmk_VDSPortsetClientWrite write;

   /** handler for writes to blockable portset properties */
   vmk_VDSPortsetClientBlockableWrite blockableWrite;

   /** handler for clearing portset properties */
   vmk_VDSPortsetClientClear clear;
} vmk_VDSPortsetClientOps;


/*
 ***********************************************************************
 * vmk_VDSClientPortsetRegister --                                */ /**
 *
 * \brief Register VDS Portset property handlers.
 *
 * \note At most one client may be registered for a given property (data
 *       name) on a given portset
 *
 * \note Clients for non-blockable properties must register write() and
 *       clear() callbacks. Clients for blockable properties must
 *       register blockableWrite() and clear() callbacks.
 *
 * \note We only support blockable callback for write operation of
 *       VMK_VDSPROP_HOST_PROXY_STATE property. All other callbacks
 *       for other properties cannot be registered as blockable.
 *
 * \note This function will not block.
 *
 * \param[out]  client            Ptr to handle for VDS Portset Client
 * \param[in]   psName            Portset name
 * \param[in]   dataName          Data name
 * \param[in]   ops               VDSClient portset ops to use
 * \param[in]   blockable         Whether write callback is blockable
 *
 * \retval      VMK_OK            VDSClient registered successfully
 * \retval      VMK_EXISTS        A client already exists for the given
 *                                portset & dataname
 * \retval      VMK_NO_MEMORY     Insufficient memory to perform 
 *                                registration
 * \retval      VMK_BAD_PARAM     A required callback is unspecified
 *                                (NULL)
 * \retval      VMK_NOT_SUPPORTED The specified portset client cannot be
 *                                registered as blockable
 ***********************************************************************
 */

VMK_ReturnStatus vmk_VDSClientPortsetRegister(vmk_VDSPortsetClient *client,
                                              const char *psName,
                                              const char *dataName,
                                              const vmk_VDSPortsetClientOps *ops,
                                              vmk_Bool blockable);

/*
 ***********************************************************************
 * vmk_VDSClientPortsetUnregister --                              */ /**
 *
 * \brief Unregister VDS Portset property handlers.
 *
 * \note This function will not block.
 *
 * \param[in]   client         Handle for VDS Portset Client
 *
 * \retval      VMK_OK         VDSClient unregistered successfully
 * \retval      VMK_NOT_FOUND  The specified client does not exist
 ***********************************************************************
 */

VMK_ReturnStatus vmk_VDSClientPortsetUnregister(vmk_VDSPortsetClient client);


/**
 * \brief Handle for a VDSClient Port Client.
 */

typedef struct VDSPortClient *vmk_VDSPortClient;


/*
 ***********************************************************************
 * vmk_VDSPortClientWrite --                                      */ /**
 *
 * \brief Write callback for a port property of a VDS client.
 *
 * \note Refer to the documentation for vmk_VDSPortDataSet()
 *       for a description of how this callback is used.
 *
 * \note This callback cannot block.
 *
 * \param[in]   ps             Mutable portset handle.
 * \param[in]   portID         Port ID
 * \param[in]   dataName       Data name
 * \param[in]   data           Data to set for name
 * \param[in]   dataLen        Length of data
 * \param[in]   flags          Flags
 *
 * \retval      VMK_OK         Port client accepts value change of the
 *                             data name (property) on the port
 * \retval      Other status   Port client does not accept the value change
 *                             of the data name (property) on the port
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_VDSClientPortWrite)(vmk_Portset *ps,
                                                   vmk_SwitchPortID portID,
                                                   char *dataName, void *data,
                                                   int dataLen,
                                                   vmk_VDSClientOpsFlags flags);


/*
 ***********************************************************************
 * vmk_VDSClientPortRead --                                       */ /**
 *
 * \brief Read callback for a port property of a VDS client.
 *
 * \note Refer to the documentation for vmk_VDSPortDataGet()
 *       for a description of how this callback is used.
 *
 * \note This callback cannot block.
 *
 * \param[in]   ps             Immutable portset handle.
 * \param[in]   portID         Port ID
 * \param[in]   dataName       Data name
 * \param[in]   data           Data to set for name
 * \param[in]   dataLen        Length of data
 * \param[in]   flags          Flags
 *
 * \retval      VMK_OK         Port client successfully retrieved value of
 *                             the data name (property) on the port
 * \retval      Other status   Port client could not retrieve the value
 *                             of the specified data name (property) on
 *                             the port
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_VDSClientPortRead)(vmk_Portset *ps,
                                                  vmk_SwitchPortID portID,
                                                  char *dataName, void *data,
                                                  int dataLen,
                                                  vmk_VDSClientOpsFlags flags);


/*
 ***********************************************************************
 * vmk_VDSClientPortPoll --                                       */ /**
 *
 * \brief Poll callback for a port property of a VDS client.
 *
 * \note Refer to the documentation for vmk_VDSPortDataGet()
 *       for a description of how this callback is used.
 *
 * \note This callback cannot block.
 *
 * \param[in]   ps             Immutable portset handle.
 * \param[in]   portID         Port ID
 * \param[in]   dataName       Data name
 * \param[out]  dataLenPtr     Ptr to length of data available to be read
 *
 * \retval      VMK_TRUE       If the specified data name (property)
 *                             has a new value to be read; in this case
 *                             *dataLenPtr is set to the length of
 *                             the data available to be read.
 * \retval      VMK_FALSE      If the specified data name (property)
 *                             has no new data value to be read.
 ***********************************************************************
 */

typedef vmk_Bool (*vmk_VDSClientPortPoll)(vmk_Portset *ps,
                                          vmk_SwitchPortID portID,
                                          char *dataName, int *dataLengthPtr);


/*
 ***********************************************************************
 * vmk_VDSClientPortCleanup --                                    */ /**
 *
 * \brief Cleanup callback for a port property of a VDS client.
 *
 * \note This callback is called when disabling a port, or when an
 *       enable operation fails; it should clean up and free any
 *       resources associated with the specified data name (port
 *       property) for the specified port.
 *
 * \note This callback cannot block.
 *
 * \param[in]   ps             Mutable portset handle.
 * \param[in]   portID         Port ID
 * \param[in]   dataName       Data name
 *
 * \return      None           No return value
 ***********************************************************************
 */

typedef void (*vmk_VDSClientPortCleanup)(vmk_Portset *ps,
                                         vmk_SwitchPortID portID,
                                         char *dataName);

/**
 * \brief Argument structure when registering port property handlers
 */

typedef struct vmk_VDSPortClientOps {
   /**
    * Mandatory handler for writing to port properties.
    */
   vmk_VDSClientPortWrite write;

   /**
    * Optional handler for reading port properties.
    */
   vmk_VDSClientPortRead read;

   /** 
    * Optional handler for polling port properties; must be provided if
    * a read callback is provided.
    */
   vmk_VDSClientPortPoll poll;

   /** 
    * Optional handler for cleaning up port properties.
    */
   vmk_VDSClientPortCleanup cleanup;
} vmk_VDSPortClientOps;

/*
 ***********************************************************************
 * vmk_VDSClientPortRegister --                                   */ /**
 *
 * \brief Register Port Operations for a VDS Client.
 *
 * \note At most one client may be registered for a given property
 *       (data name) on a specified port and portset.
 *
 * \note This function will not block.
 *
 * \param[out]  client         Ptr to handle for VDS Port Client
 * \param[in]   psName         Portset name
 * \param[in]   dataName       Data name
 * \param[in]   ops            VDSClient port ops to use
 *
 * \retval      VMK_OK         VDSClient registered successfully
 * \retval      VMK_EXISTS     A client already exists for the given
 *                             portset & dataName
 * \retval      VMK_NO_MEMORY  Insufficient memory to perform
 *                             registration
 * \retval      VMK_BAD_PARAM  A required callback is unspecified (NULL)
 ***********************************************************************
 */

VMK_ReturnStatus vmk_VDSClientPortRegister(vmk_VDSPortClient *client,
                                           const char *psName,
                                           const char *dataName,
                                           const vmk_VDSPortClientOps *ops);


/*
 ***********************************************************************
 * vmk_VDSClientPortUnregister --                                 */ /**
 *
 * \brief Unregister a VDS Port Client.
 *
 * \note This function will not block.
 *
 * \param[in]   client         Handle for VDS Port Client
 *
 * \retval      VMK_OK         VDSClient unregistered successfully
 * \retval      VMK_NOT_FOUND  The specified client does not exist
 ***********************************************************************
 */

VMK_ReturnStatus vmk_VDSClientPortUnregister(vmk_VDSPortClient client);

/*
 ***********************************************************************
 * vmk_VDSHostPropLookup --                                       */ /**
 *
 * \brief Lookup a host property by name.
 *
 * \note The caller must not hold any locks or portset handles.
 *
 * \note This function may block.
 *
 * \param[in]   psName         Portset name.
 * \param[in]   dataName       Name of data item to look up
 * \param[out]  dataValue      Pointer to block of data to associated
 *                             with the name
 * \param[out]  dataLength     Length of the block at dataValue
 *
 * \retval      VMK_OK         Success; in this case *entryp is updated
 * \retval      VMK_NOT_FOUND  The switch or specified property cannot
 *                             be found.
 * \retval      VMK_BAD_PARAM  The provided psName, dataName, dataValue
 *                             or dataLength is invalid
 ***********************************************************************
 */

VMK_ReturnStatus vmk_VDSHostPropLookup(const char *psName,
                                       const char *dataName,
                                       void **dataValue,
                                       vmk_uint32 *dataLength);


/*
 ***********************************************************************
 * vmk_VDSHostPropSet --                                          */ /**
 *
 * \brief Set a host property (name, value) pair.
 *
 * The property value to be set must not be NULL; dataLength should
 * not be zero; dataName should not be empty. To clean up property
 * value, use vmk_VDSHostPropClear.
 *
 * \note This operation will call the VDS portset client's
 *       vmk_VDSPortsetClientWrite() callback to set the value of the
 *       property. If this succeeds, the value written will also be
 *       saved in the cache. 
 * 
 * \note The caller must not hold any locks or portset handles.
 *
 * \note This function may block.
 *
 * \param[in]   psName         Portset name.
 * \param[in]   dataName       Name of the data item to set
 * \param[in]   dataValue      Pointer to block of data to associated
 *                             with the name
 * \param[in]   dataLength     Length of the block at dataValue
 *
 * \retval      VMK_OK         Success; the host property (name, value)
 *                             pair has been updated
 * \retval      VMK_NOT_FOUND  The switch cannot be found.
 * \retval      VMK_BAD_PARAM  The provided psName, dataName, dataValue
 *                             or dataLength is invalid
 * \retval      VMK_NO_MEMORY  Insufficient memory for storing the
 *                             (name, value) pair
 * \retval      Other status   vmk_VDSPortsetClientWrite callback failed
 ***********************************************************************
 */
VMK_ReturnStatus vmk_VDSHostPropSet(const char *psName,
                                    const char *dataName,
                                    const void *dataValue,
                                    vmk_uint32 dataLength);

/*
***********************************************************************
* vmk_VDSHostPropClear --                                          */ /**
*
* \brief Clear host property with specified data name
*
* \note This operation will call the VDS portset client's
*       vmk_VDSPortsetClientClear() callback to clear the value of the
*       property. If succeeds, the portset client for this property
*       will be removed. 
*
* \note The caller must not hold any locks or portset handles.
*
* \note This function may block.
*
* \param[in]   psName         Portset name.
* \param[in]   dataName       Name of the data item to clear
*
* \retval      VMK_OK         Success; the host property with dataName
*                             has been cleared
* \retval      VMK_NOT_FOUND  The switch or named data cannot be found.
* \retval      VMK_BAD_PARAM  The provided psName or dataName is
*                             invalid
***********************************************************************
*/

VMK_ReturnStatus vmk_VDSHostPropClear(const char *psName,
                                      const char *dataName);

/*
***********************************************************************
* vmk_VDSHostStatusSet --                                          *//**
*
* \brief Set VDS host status and display string.
*
* The display string will be copied inside the function, and the
* copied string will be propogated to Virtual Center in events.
*
* \note The caller must not hold any locks or portset handles.
*
* \note This function may block.
*
* \param[in]   psName           Portset name.
* \param[in]   vdsStatus        VDS Host status to be set
* \param[in]   displayStr       Detailed status string less than
*                               256 characters 
*
* \retval      VMK_OK            Success; the host status has been updated
* \retval      VMK_NOT_FOUND     VDS is not found
* \retval      VMK_NAME_TOO_LONG displayStr is longer than 256 characters
* \retval      VMK_FAILURE       Failure; host status has not been updated
* \retval      VMK_NO_MEMORY     No memory
* \retval      VMK_BAD_PARAM     The provided psName or displayStr is
*                                invalid
***********************************************************************
*/
VMK_ReturnStatus vmk_VDSHostStatusSet(const char *psName,
                                      vmk_VDSHostStatus vdsStatus,
                                      const char *displayStr);


/*
 ***********************************************************************
 * vmk_VDSPortDataGet --                                          */ /**
 *
 * \brief Get the value part of a specified port data (name, value) pair.
 *
 * This operation will call the VDS Port Client's
 * vmk_VDSClientPortPoll() callback to find out if there
 * is new data to be read.  If so, the client's
 * vmk_VDSClientPortRead() callback will be called to
 * obtain the new data.  This new data will also be
 * saved in a cache.  If the poll operation reports
 * that new data is not available, then the previously
 * cached value will be returned.
 *
 * \note The caller must not hold any locks or portset handles.
 *
 * \note This function may block.
 *
 * \param[in]   portID         Numeric ID of a virtual port.
 * \param[in]   dataName       Name of port data.
 * \param[out]  dataValue      Buffer for value to associated
 *                             with the name.
 * \param[out]  dataLength     Length of data returned.
 *
 *
 * \retval      VMK_OK         Success; dataValuePtr and DataLengthPtr
 *                             updated
 * \retval      VMK_BAD_PARAM  The provided dataName, dataValue, or
 *                             dataLength is invalid
 * \retval      VMK_NOT_FOUND  The specified port cannot be found or
 *                             the port is not associated with vds port;
 *                             the named data cannot be found.
 * \retval      Other status   vmk_VDSClientPortPoll or
 *                             vmk_VDSClientPortRead callback failed
 ***********************************************************************
 */

VMK_ReturnStatus vmk_VDSPortDataGet(vmk_SwitchPortID portID,
                                    const char *dataName,
                                    void **dataValue,
                                    vmk_uint32 *dataLength);


/*
 ***********************************************************************
 * vmk_VDSPortDataSet --                                          */ /**
 *
 * \brief Set a port data (name, value) pair.
 *
 * This operation will call the VDS Port Client's
 * vmk_VDSClientPortWrite() callback to set the value
 * of the property.  If this succeeds, the value written
 * will also be saved in the cache.  See the description
 * of vmk_VDSPortDataGet() and vmk_VDSPortDataClear().
 *
 * \note The caller must not hold any locks or portset handles.
 *
 * \note This function may block.
 *
 * \param[in]   portID         Numeric ID of a virtual port.
 * \param[in]   dataName       Name of port data
 * \param[in]   dataValue      Value to associate with the name
 * \param[in]   dataLength     Length of data
 *
 * \retval      VMK_OK         Success
 * \retval      VMK_BAD_PARAM  The provided dataName, dataValue, or
 *                             dataLength is invalid
 * \retval      VMK_NOT_FOUND  The specified port cannot be found or
 *                             port is not associated with vds port
 * \retval      VMK_FAILURE    No value provided to associated with
 *                             the name
 * \retval      VMK_NO_MEMORY  Insufficient memory for storing the
 *                             (name, value) pair
 * \retval      Other status   vmk_VDSClientPortWrite callback failed
 ***********************************************************************
 */

VMK_ReturnStatus vmk_VDSPortDataSet(vmk_SwitchPortID portID,
                                    const char *dataName,
                                    const void *dataValue,
                                    vmk_uint32 dataLength);

/*
***********************************************************************
* vmk_VDSPortDataClear --                                        */ /**
*
* \brief Set a port data (name, value) pair.
*
* This operation will call the VDS Port Client's
* vmk_VDSClientPortCleanup() callback to clean up the property.
* If this succeeds, the property data will be removed from cache,
* and port client will also be removed. 
*
* \note The caller must not hold any locks or portset handles.
*
* \note This function may block.
*
* \param[in]   portID         Numeric ID of a virtual port.
* \param[in]   dataName       Name of port data
*
* \retval      VMK_OK         Success
* \retval      VMK_BAD_PARAM  The provided dataName is invalid
* \retval      VMK_NOT_FOUND  The specified port or named data cannot
*                             be found.
* \retval      VMK_FAILURE    vmk_VDSClientPortCleanup callback returns
*                             failure or other failure conditions
* \retval      Other status   vmk_VDSClientPortCleanup callback failed
***********************************************************************
*/
VMK_ReturnStatus vmk_VDSPortDataClear(vmk_SwitchPortID portID,
                                      const char *dataName);

/*
 ***********************************************************************
 * vmk_VDSLocalCachePortDataSet --                                */ /**
 *
 * \brief Set port data in vDS host local cache. 
 *
 * This API should be called during port reserve or connect 
 * time (i.e. in portset callbacks) in order to seed port property
 * value.
 *
 * \note This operation will not call property client (i.e.
 *       vmk_VDSClientPortWrite()).
 *
 * \note The caller must hold a mutable handle for the portset.
 *
 * \note The function will not block.
 *
 * \param[in]   portID         Numeric ID of a virtual port.
 * \param[in]   dataName       Name of port data
 * \param[in]   dataValue      Value to associate with the name
 * \param[in]   dataLength     Length of data
 *
 * \retval  VMK_OK                         Success
 * \retval  VMK_BAD_PARAM                  The provided dataName,
 *                                         dataValue, or dataLength is
 *                                         invalid
 * \retval  VMK_NOT_FOUND                  The specified port cannot be
 *                                         found or port is not
 *                                         associated with vds port
 * \retval  VMK_PORTSET_HANDLE_NOT_MUTABLE The caller did not hold a
 *                                         mutable handle
 * \retval  VMK_NO_MEMORY                  Insufficient memory for
 *                                         storing the (name, value) pair
 ***********************************************************************
 */
VMK_ReturnStatus vmk_VDSLocalCachePortDataSet(vmk_SwitchPortID portID,
                                              const char *dataName,
                                              const void *dataValue,
                                              vmk_uint32 dataLength);

/*
 ***********************************************************************
 * vmk_VDSLocalCachePortDataGet --                                */ /**
 *
 * \brief Get port data value from vDS host local cache.
 *
 * \note This operation will not call property client (i.e.
 *       vmk_VDSClientPortRead()). 
 *
 * \note The caller must hold a mutable handle for the portset.
 *
 * \note The function will not block.
 *
 * \param[in]   portID         Numeric ID of a virtual port.
 * \param[in]   dataName       Name of port data
 * \param[in]   dataValue      Value to associate with the name
 * \param[in]   dataLength     Length of data
 *
 * \retval  VMK_OK                          Success
 * \retval  VMK_NOT_FOUND                   The specified port or named
 *                                          data cannot be found.
 * \retval  VMK_BAD_PARAM                   The provided dataName,
 *                                          dataValue, or dataLength is
 *                                          invalid
 * \retval  VMK_PORTSET_HANDLE_NOT_MUTABLE  The caller did not hold a
 *                                          mutable handle
 ***********************************************************************
 */
VMK_ReturnStatus vmk_VDSLocalCachePortDataGet(vmk_SwitchPortID portID,
                                              const char *dataName,
                                              void **dataValue,
                                              vmk_uint32 *dataLength);


/*
 ***********************************************************************
 * vmk_PortLookupVDSPortAlias --                                   *//**
 *
 * \brief Retrieves the vDS port alias for the specified ephemeral port.
 *
 * \note The port alias string is constant for the lifetime of the 
 *       ephemeral port.
 *
 * \note If the vDS port alias is not configured, then this function
 *       will succeed and the string returned in vdsName will be
 *       "unknown".
 *
 * \note The caller must provide a buffer that is
 *       VMK_VDS_PORT_ALIAS_MAX_LEN bytes in length.
 *
 * \note The caller must hold a mutable handle for the portset
 *       associated with the specified portID.
 *
 * \note The function will not block.
 *
 * \param[in]  portID        Numeric ID of a virtual port.
 * \param[out] vdsPortAlias  The buffer for the NULL-terminated vDS
 *                           port alias string.
 *
 * \retval VMK_OK                          Success.
 * \retval VMK_BAD_PARAM                   An argument is NULL.
 * \retval VMK_NOT_FOUND                   The vDS port or vDS port alias
 *                                         was not found
 * \retval VMK_PORTSET_HANDLE_NOT_MUTABLE  The caller did not hold a
 *                                         mutable handle
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortLookupVDSPortAlias(vmk_SwitchPortID portID,
                                            char *vdsPortAlias);


/*
 ***********************************************************************
 * vmk_PortLookupVDSPortID --                                      *//**
 *
 * \brief Retrieves the vDS port ID for the specified ephemeral port.
 *
 * \note The port ID string is constant for the lifetime of the
 *       ephemeral port.
 *
 * \note The caller must provide a buffer that is
 *       VMK_VDS_PORT_ID_MAX_LEN bytes in length.
 *
 * \note The caller must hold a mutable handle for the portset
 *       associated with the specified portID.
 *
 * \note The function will not block.
 *
 * \param[in]  portID     Numeric ID of a virtual port.
 * \param[out] vdsPortID  The buffer for the NULL-terminated vDS port ID
 *                        string.
 *
 * \retval VMK_OK                          Success.
 * \retval VMK_BAD_PARAM                   An argument is NULL.
 * \retval VMK_NOT_FOUND                   The vDS port or vDS port ID
 *                                         was not found
 * \retval VMK_PORTSET_HANDLE_NOT_MUTABLE  The caller did not hold a
 *                                         mutable handle
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortLookupVDSPortID(vmk_SwitchPortID portID,
                                         char *vdsPortID);


/*
 ***********************************************************************
 * vmk_PortsetLookupVDSID --                                       *//**
 *
 * \brief Retrieves the vDS switch ID for the specified portset.
 *
 * \note The vDS switch ID is constant for the lifetime of the portset.
 *
 * \note The caller must provide a buffer that is VMK_VDS_ID_MAX_LEN
 *       bytes in length.
 *
 * \note The function will not block.
 *
 * \param[in]  ps     Mutable portset handle.
 * \param[out] vdsID  The buffer for the NULL-terminated vDS ID string.
 *
 * \retval VMK_OK         Success.
 * \retval VMK_BAD_PARAM  An argument is NULL.
 * \retval VMK_NOT_FOUND  There is no vDS associated with this portset
 *                        or vDS switch ID was not found 
 * \retval VMK_FAILURE    The caller did not hold a immutable/mutable
 *                        handle
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortsetLookupVDSID(vmk_Portset *ps,
                                        char *vdsID);

/*
 ***********************************************************************
 * vmk_PortsetLookupVDSName --                                     *//**
 *
 * \brief Retrieves the vDS switch name for the specified portset.
 *
 * \note The vDS switch name is constant for the lifetime of the
 *       portset.
 *
 * \note If the vDS switch name is not configured, then this function
 *       will succeed and the string returned in vdsName will be
 *       "unknown".
 *
 * \note The caller must provide a buffer that is VMK_VDS_NAME_MAX_LEN
 *       bytes in length.
 *
 * \note The function will not block.
 *
 * \param[in]  ps       Mutable portset handle.
 * \param[out] vdsName  The buffer for the NULL-terminated vDS name 
 *                      string.
 *
 * \retval VMK_OK                          Success.
 * \retval VMK_BAD_PARAM                   An argument is NULL.
 * \retval VMK_NOT_FOUND                   There is no vDS associated
 *                                         with this portset or vDS switch
 *                                         name was not found
 * \retval VMK_PORTSET_HANDLE_NOT_MUTABLE  The caller did not hold a
 *                                         mutable handle
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortsetLookupVDSName(vmk_Portset *ps,
                                          char *vdsName);

/*
 ***********************************************************************
 * vmk_PortsetLookupVDSClassName --                                *//**
 *
 * \brief Retrieves the vDS switch class name for the specified portset.
 *
 * \note The vDS class name is constant for the lifetime of the portset.
 *
 * \note The caller provide a buffer that is VMK_VDS_CLASS_NAME_MAX_LEN
 *       bytes in length.
 *
 * \note The function will not block.
 *
 * \param[in]  ps            Portset handle.
 * \param[out] vdsClassName  The buffer for the NULL-terminated vDS
 *                           class name string.
 *
 * \retval VMK_OK                          Success.
 * \retval VMK_BAD_PARAM                   An argument is NULL.
 * \retval VMK_NOT_FOUND                   There is no vDS associated
 *                                         with this portset or vDS
 *                                         switch class name was not
 *                                         found   
 * \retval VMK_PORTSET_HANDLE_NOT_MUTABLE  The caller did not hold a
 *                                         mutable handle
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortsetLookupVDSClassName(vmk_Portset *ps,
                                               char *vdsClassName);

/*
 ***********************************************************************
 * vmk_PortFindByVDSPortID --                                      *//**
 *
 * \brief Retrieves the numeric port identifier for the ephemeral port.
 *
 * \note The caller must not hold any locks or portset handles.
 *
 * \note The function will not block.
 *
 * \param[in]   vdsID          VDS switch ID string.
 * \param[in]   vdsPortID      VDS port ID string.
 * \param[out]  portID         Numeric ID of a virtual port.
 *
 * \retval      VMK_OK         Success.
 * \retval      VMK_BAD_PARAM  One or more parameters are NULL.
 * \retval      VMK_NOT_FOUND  The specified vdsPort could not be found,
 *                             or is not associated with an ephemeral
 *                             port.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortFindByVDSPortID(const char *vdsID,
                                         const char *vdsPortID,
                                         vmk_SwitchPortID *portID);


/*
 ***********************************************************************
 * vmk_PortsetFindByVDSID --                                       *//**
 *
 * \brief Retrieves the name of the portset.
 *
 * \note The caller must provide a buffer that is VMK_PORTSET_NAME_MAX
 *       bytes in length.
 *
 * \note The caller must not hold any locks or portset handles.
 *
 * \note The function will not block.
 * 
 * \param[in]  vdsID   VDS switch ID string.
 * \param[out] psName  Buffer for the portset name string.
 *
 * \retval      VMK_OK         Success.
 * \retval      VMK_BAD_PARAM  One or more parameters are NULL.
 * \retval      VMK_NOT_FOUND  The specified vds could not be found,
 *                             or is not associated with an ephemeral
 *                             port.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortsetFindByVDSID(const char *vdsID,
                                        char *psName);


/*
 ***********************************************************************
 * vmk_PortDCBIsDCBEnabled --                                     */ /**
 *
 * \brief  Check DCB support on the device connected to a vDS port.
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
 *                                the driver doesn't support the operation.
 * \retval     VMK_NOT_FOUND      If the port is not an uplink port, or
 *                                if the uplink is not found or not opened.
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
 *                                the driver doesn't support the operation.
 * \retval     VMK_NOT_FOUND      If the port is not an uplink port, or
 *                                if the uplink is not found or not opened.
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
 * \brief  Get DCB Priority Group from the device connected to a vDS port.
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
 *                                the driver doesn't support the operation.
 * \retval     VMK_NOT_FOUND      If the port is not an uplink port, or
 *                                if the uplink is not found or not opened.
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
 * Get Priority-based Flow Control configurations from the device connected
 * to a vDS port.
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
 *                                the driver doesn't support the operation.
 * \retval     VMK_NOT_FOUND      If the port is not an uplink port, or
 *                                if the uplink is not found or not opened.
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
 *                                the driver doesn't support the operation.
 * \retval     VMK_NOT_FOUND      If the port is not an uplink port, or
 *                                if the uplink is not found or not opened.
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
 *                                the driver doesn't support the operation.
 * \retval     VMK_NOT_FOUND      If the port is not an uplink port, or
 *                                if the uplink is not found or not opened.
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
 *                                the driver doesn't support the operation.
 * \retval     VMK_NOT_FOUND      If the port is not an uplink port, or
 *                                if the uplink is not found or not opened.
 * \retval     VMK_FAILURE        If the call fails.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortDCBGetCapabilities(vmk_SwitchPortID portID,
                                            vmk_DCBCapabilities *caps);

#endif
/** @} */
/** @} */
