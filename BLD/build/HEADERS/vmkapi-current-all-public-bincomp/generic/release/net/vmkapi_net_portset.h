
/* **********************************************************
 * Copyright 2013 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Portset                                                        */ /**
 * \addtogroup Network
 * @{
 * \defgroup Portset Interface
 *
 * Portsets are groups of ports which, together with policies for frame
 * routing, form virtual networks. Virtual nics connected to the ports
 * may forward packets to each other. The analogy is to a box (like a
 * a hub or a switch) containing some type of backplane for connecting
 * multiple ports in the physical world.
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_NET_PORTSET_H_
#define _VMKAPI_NET_PORTSET_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "net/vmkapi_net_types.h"
#include "net/vmkapi_net_uplink.h"
#include "net/vmkapi_net_port.h"
#include "net/vmkapi_net_pktlist.h"
#include "vds/vmkapi_vds_respools.h"

/**
 * \brief Portset name length.
 */
#define VMK_PORTSET_NAME_MAX (32 * 4)

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
 * \nativedriversdisallowed
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
 * \nativedriversdisallowed
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
 * vmk_PortsetRelease --                                           *//**
 *
 * \brief Release a handle to a portset.
 *
 * \nativedriversdisallowed
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
 * vmk_PortsetForAllPorts --                                      */ /**
 *
 * \brief Iterate over all in-use ports.
 *
 * \nativedriversdisallowed
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
 * vmk_PortsetPortIndex --                                        */ /**
 *
 * \brief Retrieve the index of a port in the portset.
 *
 * \nativedriversdisallowed
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
 * \nativedriversdisallowed
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
 * vmk_PortsetName --                                             */ /**
 *
 * \brief Retrieve the portset name.
 *
 * \nativedriversdisallowed
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
 * vmk_PortsetSize --                                             */ /**
 *
 * \brief Retrieve the number of ports in a portset.
 *
 * \nativedriversdisallowed
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
 * vmk_PortsetPrivDataSet --                                      */ /**
 *
 * \brief Set per-portset private data
 *
 * \nativedriversdisallowed
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
 * \nativedriversdisallowed
 *
 * \note This function will not block.
 *
 * \param[in]  ps             Handle to a portset
 *
 * \retval     void *         Pointer to memory location of private data
 ***********************************************************************
 */
void *vmk_PortsetPrivDataGet(vmk_Portset *ps);

/**
 * \brief Event ids of portset events.
 */
typedef enum vmk_PortsetEvent {

   /** None. */
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

/*
 ***********************************************************************
 * vmk_PortsetNotifyChange --                                 */ /**
 *
 * \brief Send an event to notify the change of a portset property.
 *
 * \nativedriversdisallowed
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

/**
 * \brief Portset Ops flag.
 */
typedef enum vmk_PortsetOpsFlag {
   /** Normal portset activate/deactivate. */
   VMK_PORTSETOPS_FLAG_NONE              = 0x0,
   /** Activate/deactivate as part of hotswap operation. */
   VMK_PORTSETOPS_FLAG_HOTSWAP           = 0x1,
} vmk_PortsetOpsFlag;

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

/**
 * \brief Query ops to get portset implementation information.
 */
typedef enum vmk_PortsetQueryID {
   /** Query the uplink policy of the given port. */
   VMK_PORTSET_QUERY_PORTUPLINKPOLICY,

   /** Query the uplink status of the given port. */
   VMK_PORTSET_QUERY_PORTUPLINKSTATUS,

   /** Query the list of PF's behind the given port. */
   VMK_PORTSET_QUERY_PFLIST,

   /** Query the packet stats of a port */
   VMK_PORTSET_QUERY_PORT_PACKET_STATS,
} vmk_PortsetQueryID;

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

   /** Backed by a physical device. */
   VMK_UPLINK_ATTRIBUTE_POPULATED = 0x0,

   /** Not backed by a physical device. */
   VMK_UPLINK_ATTRIBUTE_UNPOPULATED = 0x1,
} vmk_UplinkAttribute;

/**
 * \brief Parameter structure for a port uplink policy query request.
 *
 * Used to ask the portset implementation for the uplink policy information,
 * i.e. uplinks that are associated to the queried port, and flags
 * indicating whether teaming policy requires additional physical switch
 * setting.
 */
typedef struct vmk_PortsetQueryPortUplinkPolicyData {
   /** The port being asked about. */
   vmk_SwitchPortID portID;
   /** Uplink policy flag. */
   vmk_UplinkPolicyFlags policyFlags;
   /** Number of uplinks in policy info (output). */
   vmk_uint32 nUplinks;
   /**
    *  Name of each uplink in policy info (output), including
    *  active/standby uplinks.
    *  The user is responsible for both allocating and freeing
    *  the array. The buffer must be large enough to contain
    *  at most uplinkNameArraySz elements.
    */
   vmk_Name *uplinkName;
   /**
    * Flag indicating if the associated uplink is an empty
    *  ('logical') link on a vDS implementation.
    */
   vmk_UplinkAttribute *uplinkAttribute;
   /** Number of names that can be stored in uplinkName. */
   vmk_uint32 uplinkNameArraySz;
} vmk_PortsetQueryPortUplinkPolicyData;

/**
 * \brief Values returned by a port uplink status request.
 */
typedef enum vmk_PortsetPortUplinkStatus {
   /** All the uplinks that the port uses are link down. */
   VMK_PORTSET_PORT_UPLINK_DOWN,
   /** All the uplinks that the port uses are link up. */
   VMK_PORTSET_PORT_UPLINK_NORMAL,
   /**
    * Some uplinks that the port uses are link down; at least
    * one uplink that the port uses is link up.
    */
   VMK_PORTSET_PORT_UPLINK_REDUNDANCYLOSS,
} vmk_PortsetPortUplinkStatus;

/**
 * \brief Parameter structure for a port uplink status request.
 *
 * Used by portset implementation to ask the uplink device status, including
 * link up/down, link speed and duplexity.
 */
typedef struct vmk_PortsetQueryPortUplinkStatusData {
   /** The port being asked about. */
   vmk_SwitchPortID portID;
   /** (output) uplink status. */
   vmk_PortsetPortUplinkStatus *status;
} vmk_PortsetQueryPortUplinkStatusData;

/**
 * \brief Parameter structure for a port pf list query request.
 *
 * Used to ask the pf identifiers that support passthrough.
 */
typedef struct vmk_PortsetQueryPFListData {
   /** (input) port. */
   vmk_SwitchPortID portID;
   /** (input) passthru type. */
   vmk_NetPTType  ptType;
   /**
    *   (input/output) pf count.
    *   On input contains the max size of pfList and on output it
    *   contains the list size which is populated.
    */
   vmk_uint32 *pfCount;
   /** (output) list of pf supporting passthrough. */
   vmk_uint32 *pfList;
} vmk_PortsetQueryPFListData;

/**
 * \brief Parameter structure for port packet stats query request.
 */
typedef struct vmk_PortsetQueryPortPacketStatsData {
   /** The ID of port to query */
   vmk_SwitchPortID portID;

   /** The direction of packet stats to query */
   vmk_PortPktStatsDir dir;

   /** The type of packet stats to query */
   vmk_PortPktStatsType type;

   /** The packet stats returned by portset implementation */
   vmk_uint64 stats;
} vmk_PortsetQueryPortPacketStatsData;


/*
 ***********************************************************************
 * vmk_PortsetQuery --                                            */ /**
 *
 * \brief Optional callback for handling of portset queries.
 *
 * \note queryData is a pointer to a
 *       vmk_PortsetQueryPortUplinkPolicyData type when queryData equals
 *       VMK_PORTSET_QUERY_PORTUPLINKPOLICY.
 *
 * \note queryData is a pointer to a vmk_PortsetQueryPFListData type
 *       when queryData equals VMK_PORTSET_QUERY_PFLIST.
 *
 * \note queryData is a pointer to a
 *       vmk_PortsetQueryPortUplinkStatusData type when queryData equals
 *       VMK_PORTSET_QUERY_PORTUPLINKSTATUS.
 *
 * \note queryData is a pointer to a vmk_PortsetQueryPortPacketStatsData
 *       type when queryData equals VMK_PORTSET_QUERY_PORT_PACKET_STATS.
 *
 * \note This function will not block.
 *
 * \param[in]      ps               Mutable handle to a portset.
 * \param[in]      queryID          ID of query being performed
 * \param[in,out]  queryData        Query-specific data
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
 ***********************************************************************
 * vmk_PortsetPortBlock --                                        */ /**
 *
 * \brief Optional callback received when a port is blocked.
 *
 * \note This callback can not block.
 *
 * \param[in]  ps                Mutable handle to a portset.
 * \param[in]  portID            Numeric ID of the virtual port.
 *
 * \retval     VMK_OK
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_PortsetPortBlock)(vmk_Portset *ps,
                                                 vmk_SwitchPortID portID);

/*
 ***********************************************************************
 * vmk_PortsetPortUnblock --                                      */ /**
 *
 * \brief Optional callback received when a port is unblocked.
 *
 * \note This callback cannot block.
 *
 * \param[in]  ps                Mutable handle to a portset.
 * \param[in]  portID            Numeric ID of the virtual port.
 *
 * \retval     VMK_OK
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_PortsetPortUnblock)(vmk_Portset *ps,
                                                   vmk_SwitchPortID portID);

/*
 * \brief Version number for portset ops structure.
 */
#define VMK_PORTSET_OPS_VERSION_0         0
#define VMK_PORTSET_OPS_VERSION_CURRENT   VMK_PORTSET_OPS_VERSION_0

/**
 * \brief Structure to register portset callbacks.
 */
typedef struct {
   /** Version of portset ops structure; always set to current version. */
   vmk_uint32                  version;

   /** Handler for implementation-specific action during portset deactivation. */
   vmk_PortsetDeactivate       deactivate;

   /** Handler for implementation-specific action for Pkt dispatch. */
   vmk_PortsetDispatch         dispatch;

   /** Handler for implementation-specific action during port connect. */
   vmk_PortsetPortConnect      portConnect;

   /** Handler for implementation-specific action during port disconnect. */
   vmk_PortsetPortDisconnect   portDisconnect;

   /** Handler for implementation-specific action during port enable. */
   vmk_PortsetPortEnable       portEnable;

   /** Handler for implementation-specific action during port disable. */
   vmk_PortsetPortDisable      portDisable;

   /** Handler for implementation-specific action during port updates. */
   vmk_PortUpdate              portUpdate;

   /** Handler for portset query operations. */
   vmk_PortsetQuery            query;

   /** Handler for port reservation before port connect. */
   vmk_PortsetPortReserve      portReserve;

   /** Handler for cancel port reservation after port connect fails. */
   vmk_PortsetPortUnreserve    portUnreserve;

   /**
    * Handler for getting port statistics, only needed for special
    * uplink ports, i.e. passthroughed hidden uplinks.
    */
   vmk_PortGetStats            portGetStats;

   /** Handler for port block operation */
   vmk_PortsetPortBlock        portBlock;

   /** Handler for port unblock operation */
   vmk_PortsetPortUnblock      portUnblock;

} vmk_PortsetOps;

/*
 ***********************************************************************
 * vmk_PortsetOpsSet --                                           */ /**
 *
 * \brief Setup portset callbacks.
 *
 * \nativedriversdisallowed
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
 * \nativedriversdisallowed
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
 * \nativedriversdisallowed
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
 * vmk_PortsetAddUplink --                                        */ /**
 *
 * \brief Request to add an uplink to a portset.
 *
 * An vswitch implementation can use this API to add a named uplink to a
 * named portset.
 *
 * Vmkernel will connect the uplink to the relevant portset and enable
 * the port if connection is successful.
 *
 * \note This API is not to be used for VDS based vswitch. The operation
 *       will be triggered automatically from user space for VDS vswitch.
 *
 * \note If the uplink is already associated with a vSwitch, the operation
 *       may fail.
 *
 * \note The portset must be activated.
 *
 * \note Shouldn't be holding any handles to the portset.
 *
 * \note This function will not block.
 *
 * \param[in]  psName           portset name.
 * \param[in]  uplinkDevName    uplink device name (vmnic*)
 * \param[out] portID           This is optional, the portID the uplink
 *                              connect to.
 *
 * \retval VMK_BAD_PARAM        psName or uplinkDevName is NULL.
 * \retval VMK_NOT_FOUND        uplinkDevName not found
 * \retval VMK_BUSY             the uplink already connected to a portset
 * \retval VMK_LIMIT_EXCEEDED   no more port for the uplink.
 * \retval VMK_NO_MEMORY        memory allocation failure
 * \retval VMK_OK               Successfully add the uplink to the
 *                              portset.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortsetAddUplink(char *psName,
                                      char *uplinkDevName,
                                      vmk_SwitchPortID *portID);

/*
 ***********************************************************************
 * vmk_PortsetRemoveUplink --                                     */ /**
 *
 * \brief Request to remove an uplink from a portset.
 *
 * An vswitch implementation can use this API to remove a named uplink
 * from a named portset.
 *
 * Vmkernel will disable the uplink and disconnect from the relevant
 * portset.
 *
 * \note This API is not to be used for VDS based vswitch. The operation
 *       will be triggered automatically from user space.
 *
 * \note Shouldn't be holding any handles to the portset.
 *
 * \note The portset must be activated.
 *
 * \note This function will not block.
 *
 * \param[in]  psName           portset name.
 * \param[in]  uplinkDevName    uplink device name (vmnic*)
 *
 * \retval VMK_BAD_PARAM        psName or uplinkDevName is NULL.
 * \retval VMK_NOT_FOUND        named portset not found
 * \retval VMK_FAILURE          other errors.
 * \retval VMK_OK               The relevant uplink is removed from the
 *                              portset.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortsetRemoveUplink(char *psName,
                                         char *uplinkDevName);

/*
 ***********************************************************************
 * vmk_PortsetFindUplinkPort --                                   */ /**
 *
 * \brief Find the first connected uplink port.
 *
 * \nativedriversdisallowed
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
 * vmk_PortsetGetUplinks --                                       */ /**
 *
 * \brief Get all uplinks connected to a portset
 *
 * \nativedriversdisallowed
 *
 * \note This function will not block.
 *
 * \note The caller should hold at least an immutable portset handle
 *
 * \note Caller needs to allocate/free buffer for uplinks.
 *
 * \param[in]     ps             Handle to a portset
 * \param[out]    uplinks        Array to keep uplink pointers
 * \param[in]     arraySize      The number of vmk_Uplinks in the array
                                 uplinks
 * \param[out]    count          Number of uplinks connected to portset
 *
 * \retval     VMK_OK         Success.
 * \retval     VMK_NO_MEMORY  Buffer is too small to keep uplink
 *                            pointers, required buffer size returned in
 *                            count
 * \retval     VMK_BAD_PARAM  An argument is NULL.
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PortsetGetUplinks(vmk_Portset *ps,
                                       vmk_Uplink *uplinks,
                                       vmk_uint32 arraySize,
                                       vmk_uint32 *count);

/*
 ***********************************************************************
 * vmk_PortsetClaimHiddenUplink --                                */ /**
 *
 * \brief Claim hidden uplink devices.
 *
 * \nativedriversdisallowed
 *
 * Hidden uplink devices are uplink devices used by portset implementation
 * to perform I/O, but are not visible to management plane.
 *
 * Vmkernel will connect the claimed hidden uplinks to the portset.
 * Vmkernel assumes that only one portset instance will claim hidden
 * uplinks.
 *
 * \note Only hidden devices with VMK_PORT_CLIENT_CAP_UPT capability are
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
 * vmk_PortsetCreate --                                           */ /**
 *
 * \brief Request to create a portset
 *
 * External module can use this API to create a portset with a specific
 * calass.
 *
 * Vmkernel will create and activate a portset
 *
 * \note This API is not to be used for VDS based vswitch. The operation
 *       will be triggered automatically from user space.
 *
 * \note Shouldn't be holding any handles to the portset.
 *
 * \note This function will not block.
 *
 * \param[in]  psName           portset name.
 * \param[in]  className        portset class name.
 * \param[in]  numPorts         number of ports the new portset supports.
 * \param[in]  flags            reserved, set to zero
 *
 * \retval VMK_BAD_PARAM        psName or className is NULL or className
 *                              is null string
 * \retval VMK_NOT_SUPPORTED    The request portset class not found
 * \retval VMK_EXIST            portset with the request psName exists.
 * \retval VMK_NO_RESOURCES     no free portset available.
 * \retval VMK_LIMIT_EXCEEDED   the request numPort exceed the supported
 *                              limit.
 * \retval VMK_NO_MEMORY        memory allocation failure
 * \retval VMK_OK               Successfully create a portset.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortsetCreate(char *psName,
                                   char *className,
                                   vmk_uint32 numPorts,
                                   vmk_uint32 flags);

/*
 ***********************************************************************
 * vmk_PortsetDestroy --                                          */ /**
 *
 * \brief Request to destroy a portset
 *
 * External module can use this API to destroy a named portset.
 *
 * VMKernel will try to first disconnect all ports and deactivate the
 * portset.
 *
 * \note This API is not to be used for VDS based vswitch. The operation
 *       will be triggered automatically from user space.
 *
 * \note Shouldn't be holding any handles to the portset.
 *
 * \note This function will not block.
 *
 * \note All connected port will be disconnected. Normally the caller
 *       shoild make sure all clients are disabled and disconnected.
 *
 * \param[in]  psName           portset name.
 *
 * \retval VMK_BAD_PARAM        psName is NULL or length is too large.
 * \retval VMK_NOT_FOUND        named portset not found
 * \retval VMK_OK               Successfully destroyed the portset.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PortsetDestroy(char *psName);

/*
 ***********************************************************************
 * vmk_PortsetPortRxFunc --                                       */ /**
 *
 * \brief Callback that is invoked upon reception of packets on a port.
 *
 * \nativedriversdisallowed
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
 * \nativedriversdisallowed
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
 * vmk_PortsetDisconnectPort --                                   */ /**
 *
 * \brief Disonnect a port to the given portset.
 *
 * \nativedriversdisallowed
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
 * vmk_PortsetReserveMgmtPort --                                  */ /**
 *
 * \brief Reserve an internal management port.
 *
 * \nativedriversdisallowed
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
 * \nativedriversdisallowed
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
 ************************************************************************
 * vmk_PortsetGetMTU --                                            */ /**
 *
 * \brief Get MTU setting on a portset
 *
 * \nativedriversdisallowed
 *
 * \note This function will not block.
 *
 * \note The caller should hold at least an immutable portset handle
 *
 * \param[in]     ps             Handle to a portset
 * \param[out]    mtu            MTU setting on the portset
 *
 * \retval     VMK_OK         Success.
 * \retval     VMK_BAD_PARAM  An argument is NULL.
 * \retval     VMK_FAILURE    Improper portset handle is held
 *
 ************************************************************************
 */
VMK_ReturnStatus vmk_PortsetGetMTU(vmk_Portset *ps,
                                   vmk_uint32 *mtu);

/*
 ***********************************************************************
 * vmk_PortsetAddResPool --                                       */ /**
 *
 * \brief Create a resource pool.
 *
 * \nativedriversdisallowed
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
 * \nativedriversdisallowed
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
 * \nativedriversdisallowed
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
 ***********************************************************************
 */
vmk_PortsetResPoolTag vmk_PortsetGetResPoolTag(vmk_PortsetResPoolID poolID);

/*
 ***********************************************************************
 * vmk_PortsetPutResPoolTag --                                    */ /**
 *
 * \brief Release the resource pool.
 *
 * \nativedriversdisallowed
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
 * \nativedriversdisallowed
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
 * vmk_PortsetPortGetResPoolCfg --                              */ /**
 *
 * \brief Get resource pool configuration that was applied on a port
 *
 * \nativedriversdisallowed
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
 * \param[out] poolCfg            Resource pool configuration.
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

VMK_ReturnStatus vmk_PortsetPortGetResPoolCfg(vmk_SwitchPortID portID,
                                              vmk_PortsetResPoolID poolID,
                                              vmk_PortsetResPoolCfg *poolCfg);
/*
 ***********************************************************************
 * vmk_PortsetPortRemoveResPoolCfg --                             */ /**
 *
 * \brief Remove a resource pool configuration from a port.
 *
 * \nativedriversdisallowed
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
 * \nativedriversdisallowed
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
 * \nativedriversdisallowed
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
 * \nativedriversdisallowed
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
 * \nativedriversdisallowed
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
 * \nativedriversdisallowed
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

#endif /* _VMKAPI_NET_PORTSET_H_ */
/** @} */
/** @} */
