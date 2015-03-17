/***************************************************************************
 * Copyright 2007 - 2009 VMware, Inc.  All rights reserved.
 ***************************************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * DVFilter                                                       */ /**
 * \addtogroup Network
 * @{
 * \defgroup DVFilter DVFilter
 * @{
 *
 * Callbacks may only take locks with major rank VMK_SP_RANK_LEAF_LEGACY.
 * It is expected that the FastPath agent will allocate a lock class of
 * this major rank and manage the assignment of minor lock ranks.
 * Callbacks may be invoked with internal VMkernel locks held. No
 * other locks may be held when calling a DVFilter vmkapi function
 * unless otherwise noted. Unless otherwise noted the callback is
 * not allowed to block.
 *
 * When calling a DVFilter vmkapi function outside of a DVFilter
 * callback (e.g. in a helper world or timer callback), the caller
 * may not hold any locks beyond those internal to the VMkernel
 * unless otherwise noted.
 *
 * These locking semantics are only guaranteed on version 1.1.0.0.
 *
 ***********************************************************************
 */
#ifndef _VMKAPI_DVFILTER_H_
#define _VMKAPI_DVFILTER_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "net/vmkapi_net_pkt.h"
#include "net/vmkapi_net_pktlist.h"

/** \cond never */
#define VMK_DVFILTER_REVISION_MAJOR       1
#define VMK_DVFILTER_REVISION_MINOR       1
#define VMK_DVFILTER_REVISION_UPDATE      0
#define VMK_DVFILTER_REVISION_PATCH_LEVEL 0

#define VMK_DVFILTER_REVISION VMK_REVISION_NUMBER(VMK_DVFILTER)

#define VMKV40_DVFILTER_REVISION_MAJOR       1
#define VMKV40_DVFILTER_REVISION_MINOR       0
#define VMKV40_DVFILTER_REVISION_UPDATE      0
#define VMKV40_DVFILTER_REVISION_PATCH_LEVEL 0

#define VMKV40_DVFILTER_REVISION VMK_REVISION_NUMBER(VMKV40_DVFILTER)
/** \endcond never */

/**
 * Maximum filter name length, including the NUL terminating character.
 */
#define VMK_DVFILTER_MAX_NAME_LEN 100

/**
 * Maximum filter configuration parameter string length,
 * including the NUL terminating character.
 */
#define VMK_DVFILTER_MAX_CONFIGPARAM_LEN 256

#define VMK_DVFILTER_MAX_PARAMS 16

typedef struct DVFilterFastPath *vmk_DVFilterFastPathHandle;
#define VMK_DVFILTER_INVALID_FASTPATH_HANDLE 0

typedef void *vmk_DVFilterFastPathImpl;

typedef struct DVFilter *vmk_DVFilterHandle;

typedef void *vmk_DVFilterImpl;

typedef vmk_uint32 vmk_DVFilterSlowPathHandle;
#define VMK_DVFILTER_INVALID_SLOWPATH_HANDLE 0

typedef void *vmk_DVFilterSlowPathImpl;

#define VMK_DVFILTER_INVALID_REQUEST_ID 0
typedef vmk_uint32 vmk_DVFilterRequestId;

typedef enum vmk_DVFilterDirection {
   VMK_DVFILTER_FROM_SWITCH   = 1,
   VMK_DVFILTER_TO_SWITCH     = 2,
   VMK_DVFILTER_BIDIRECTIONAL = 3,
} vmk_DVFilterDirection;

typedef enum vmk_DVFilterFaultingFlags {
   /** Don't release the packets or the packet list that was passed in */
   VMK_DVFILTER_FAULTING_COPY = 0x00000001,
} vmk_DVFilterFaultingFlags;

typedef enum vmk_DVFilterUnregisterReason {
   VMK_DVFILTER_UNREGISTER_SLOWPATH,
   VMK_DVFILTER_LOST_SLOWPATH,
} vmk_DVFilterUnregisterReason;

typedef enum vmk_DVFilterEndPointType {
   VMK_DVFILTER_VNIC,
   VMK_DVFILTER_VMKNIC,
   VMK_DVFILTER_VSWIF,
   VMK_DVFILTER_UPLINK,
} vmk_DVFilterEndPointType;

typedef enum vmk_DVFilterFailurePolicy {
   VMK_DVFILTER_FAIL_CLOSED,
   VMK_DVFILTER_FAIL_OPEN,
} vmk_DVFilterFailurePolicy;

typedef enum vmk_DVFilterStatType {
   VMK_DVFILTER_PKTS_FILTERED,
   VMK_DVFILTER_PKTS_FAULTED,
   VMK_DVFILTER_PKTS_QUEUED,
   VMK_DVFILTER_PKT_ERRORS,
   VMK_DVFILTER_PKTS_INJECTED,
} vmk_DVFilterStatType;

/*
 ***********************************************************************
 *                                                                */ /**
 * \ingroup DVFilter
 * \brief DVFilter FastPath callbacks.
 *
 * Parameters marked "optional" may be NULL.
 *
 ***********************************************************************
 */
typedef struct vmk_DVFilterFastPathOps {

   /********************************************************************
    *                                                             */ /**
    * \brief Register a SlowPath agent.
    *
    * This callback is invoked when a slow path agent registers with
    * the DVFilter framework with the same name used by the fast
    * path agent in vmk_DVFilterRegisterFastPath().
    *
    * The privateData field needs to be used in such a way that
    * the fast path agent is able to lookup the slow path handle
    * (i.e. the slowPath parameter) given the privateData pointer,
    * as the slow path handle has to be passed to all functions that
    * are related to slow path interaction.
    *
    *
    * \param[in]  fastPath     The fast path agent the slow path agent
    *                          chose to register/associate with.
    * \param[in]  slowPath     The slow path agent being registered.
    * \param[out] privateData  Opaque pointer to the slow path agent
    *                          object.
    *
    * \return Every value other than VMK_OK leads to the slow path
    *         agent not getting registered. The slow path agent will be
    *         informed about this failure.
    *
    ********************************************************************
    */
   VMK_ReturnStatus (*registerSlowPath)(
      vmk_DVFilterFastPathImpl fastPath,
      vmk_DVFilterSlowPathHandle slowPath,
      vmk_DVFilterSlowPathImpl *privateData);

   /********************************************************************
    *                                                             */ /**
    * \brief Unregister a slow path agent previously registered via the
    *        registerSlowPath() callback.
    *
    * This callback is invoked when the agent unregisters explicitly
    * or the connection is lost.
    *
    * Note: VMK_OK and VMK_BUSY are the only allowed return values.
    *
    * \param[in]  slowPathImpl  Opaque pointer to the slow path agent.
    *                           object (privateData supplied during
    *                           registerSlowPath()).
    * \param[in]  reason        The reason for unregistering.
    *
    * \retval VMK_OK    Success.
    * \retval VMK_BUSY  Indicates a temporary failure. The DVFilter
    *                   subsystem will call unregisterSlowPath again
    *                   later. Do not use VMK_BUSY for permanent
    *                   failures. This behavior was added in rev 1.1.0.0.
    *
    ********************************************************************
    */
   VMK_ReturnStatus (*unregisterSlowPath)(
      vmk_DVFilterSlowPathImpl slowPathImpl,
      vmk_DVFilterUnregisterReason reason);

   /********************************************************************
    *                                                             */ /**
    * \brief Instantiate a filter.
    *
    * This callback is invoked to instantiate a filter, upon creation
    * of the filter's end-point.
    *
    * It is called in a blockable context.
    *
    * The privateData parameter must be set before doing any
    * action that could result in a filter specific operation being
    * called. For example, if one would call vmk_DVFilterSetSlowPath()
    * before setting privateData, a race would be introduced. If
    * vmk_DVFilterSetSlowPath() completed before privateData was set
    * eventually, a NULL pointer instead of a valid privateData pointer
    * would be passed to the invoked callback handler.
    *
    * Note: NULL is a reserved value for privateData and must not be
    * used by the agent.
    *
    * Once assigned, the privateData parameter must not be changed,
    * especially not to NULL. The data structure it is pointing to
    * must not be freed, even if the operation fails and createFilter()
    * decides to return an error code. In that case the DVFilter
    * subsystem will call destroyFilter() to do the proper cleanup.
    *
    * \param[in]  fastPath       Fast path agent on which a filter is
    *                            getting attached.
    * \param[in]  filter         Handle to the VMkernel filter object.
    * \param[out] privateData    Opaque pointer to the agent's filter
    *                            object.
    * \param[in]  expectRestore  Whether the restoreState callback will
    *                            be invoked.
    *
    * \return Every value but VMK_OK will be treated as an error
    *         and logged. While the DVFilter subsystem does not
    *         differentiate between error codes, providing an
    *         accurate error code is useful for troubleshooting.
    *
    ********************************************************************
    */
   VMK_ReturnStatus (*createFilter)(
      vmk_DVFilterFastPathImpl fastPath,
      vmk_DVFilterHandle filter,
      vmk_DVFilterImpl *privateData,
      vmk_Bool expectRestore);

   /********************************************************************
    *                                                             */ /**
    * \brief Destroy a filter.
    *
    * This callback is invoked when the filter's end-point is
    * destroyed.
    *
    * It is called in a blockable context.
    *
    * Before returning, this function should wait for the termination
    * of all pending calls and prevent any new calls to the DVFilter
    * APIs using this filter.
    *
    * Note: VMK_OK and VMK_BUSY are the only allowed return values.
    *
    * \param[in] filter  Opaque pointer to the agent's filter object.
    *
    * \retval VMK_OK    Success.
    * \retval VMK_BUSY  Indicates a temporary failure. The DVFilter
    *                   subsystem will call destroyFilter again later.
    *                   Do not use VMK_BUSY for permanent failures.
    *
    ********************************************************************
    */
   VMK_ReturnStatus (*destroyFilter)(
      vmk_DVFilterImpl filter);

   /********************************************************************
    *                                                             */ /**
    * \brief Informs the agent of an upcoming state query.
    *
    * This callback is invoked shortly before getSavedStateLen()
    * and saveState() get invoked. This gives the agent the
    * chance to prepare for this upcoming state query, e.g. the
    * agent can call vmk_DVFilterRetrieveSlowPathState()
    * to minimize the time the system needs to wait for the VM
    * agent state during VMotion.
    *
    * The DVFilter subsystem guarantees to call getSavedStateLen()
    * after calling prefetchSavedState() in order to allow for proper
    * cleanup even in cases where VMotion has to be aborted. See
    * getSavedStateLen() for details on the error case.
    *
    * \param[in] filter  Opaque pointer to the agent's filter object.
    *
    ********************************************************************
    */
   void (*prefetchSavedState)(
      vmk_DVFilterImpl filter);

   /********************************************************************
    *                                                             */ /**
    * \brief Query the filter for the size of the saved state buffer.
    *
    * This callback is always invoked just before calling
    * saveState().
    *
    * In case the VMotion had to be aborted prematurely
    * getSavedStateLen() will be called with len set to NULL.
    * This gives agents a chance to release buffers that were
    * allocated due to a previous call to prefetchSavedState().
    * In this case the only allowed return value is VMK_OK.
    *
    * \param[in]  filter  Opaque pointer to the agent's filter object.
    * \param[out] len     Length (in bytes) required to save the
    *                     filter's state.  Zero is an acceptable
    *                     minimum length. This is the reservation
    *                     request for the buffer used in saveState().
    *
    * \retval VMK_OK           Success. *len got updated.
    * \retval VMK_EINPROGRESS  State is not ready yet. DVFilter
    *                          subsystem might try again later
    *                          (no guarantee, e.g. in case of timeouts.)
    *
    * \return Any other value than VMK_OK or VMK_EINPROGRESS indicates
    *         permanent failure. The VMotion will be aborted.
    *
    ********************************************************************
    */
   VMK_ReturnStatus (*getSavedStateLen)(
      vmk_DVFilterImpl filter,
      vmk_ByteCount *len);

   /********************************************************************
    *                                                             */ /**
    * \brief Saves the state of the filter.
    *
    * This callback is guaranteed to be invoked after
    * getSavedStateLen().
    *
    * In case the VMotion had to be aborted prematurely,
    * saveState() will be invoked but with a NULL buffer and a
    * NULL length pointer. This gives agents a chance to release
    * previously allocated buffers and to perform similar
    * cleanup. In this case the only allowed return value is
    * VMK_OK.
    *
    * If VMotion failure had already been communicated by
    * calling getSavedStateLen() with the length pointer passed
    * as NULL, saveState() will not be invoked.
    *
    * Caller ensures that the size of the supplied buffer is at
    * least the value previously returned by getSavedStateLen()
    * The len parameter must be set to reflect to number of
    * bytes actually used, which may be less than the value
    * of len supplied. References to the buffer outside the
    * supplied region can cause unpredictable behavior.
    *
    * When a previous call to getSavedStateLen() returned a zero len,
    * the call to save the state for this filter is still
    * performed, although no state can be saved within the
    * supplied buffer.
    *
    * \param[in]     filter  Opaque pointer to the agent's filter
    *                        object.
    * \param[in]     buf     Buffer to save the state.
    * \param[in,out] len     On entry, size of the supplied buffer.
    *                        On exit, size of the data actually
    *                        populated in the buffer. It is possible
    *                        for the length to be zero at entry.
    *
    * \retval VMK_OK  On success. Length has been updated and state has
    *                 been written to the buffer.
    * \return Any error code other than VMK_OK indicates a permanent
    *         error.
    *
    ********************************************************************
    */
   VMK_ReturnStatus (*saveState)(
      vmk_DVFilterImpl filter,
      void *buf,
      vmk_ByteCount *len);

   /********************************************************************
    *                                                             */ /**
    * \brief Restores the state of the filter.
    *
    * This callback is invoked after createFilter(), if the agent has
    * some persistent state saved, i.e. after a VMotion.
    *
    * In the case that there was no saved state available after a
    * VMotion, restoreState() will be called with buf = NULL and
    * len = 0. This allows the fast path agent to react to such a
    * situation. This situation can happen if the filter creation
    * (i.e. createFilter()) failed on the source host, but succeeded
    * on the destination host.
    *
    *
    * \param[in] filter  Opaque pointer to the agent's filter object.
    * \param[in] buf     Buffer to the saved state.
    * \param[in] len     Size of the supplied buffer.
    *
    * \return For every value other than VMK_OK the failure policy of
    *         the filter will be executed. A failOpen policy means
    *         the error will be ignored (i.e. the filter will have to
    *         handle packets!). A failClose policy will lead to the
    *         vNic getting disconnected.
    *
    ********************************************************************
    */
   VMK_ReturnStatus (*restoreState)(
      vmk_DVFilterImpl filter,
      void *buf,
      vmk_ByteCount len);

   /********************************************************************
    *                                                             */ /**
    * \brief Process inbound and outbound packets that traverse the
    *        filter.
    *
    * On input the pktList contains the list of packets received by
    * the filter.
    *
    * Packets left or added to the list when the function returns are
    * passed to the next layer (the virtual NIC, the virtual switch,
    * or another filter).
    *
    * Packets added to the list by the fast path must meet
    * the requirements listed for vmk_DVFilterIssuePackets().
    *
    * Packets no longer on the list are ignored. It is the agent's
    * responsibility to ultimately destroy them via vmk_PktRelease()
    * or re-inject them using vmk_DVFilterIssuePackets().
    *
    * This function can be called concurrently.
    *
    * \param[in]     filter     Opaque pointer to the agent's filter
    *                           object.
    * \param[in,out] pktList    List of packets traversing the filter.
    * \param[in]     direction  The direction of the packets.
    *
    * \return Any value other than VMK_OK will result in the packets
    *         being released and not delivered to the target.
    *
    ********************************************************************
    */
   VMK_ReturnStatus (*processPackets)(
      vmk_DVFilterImpl filter,
      vmk_PktList pktList,
      vmk_DVFilterDirection direction);

   /********************************************************************
    *                                                             */ /**
    * \brief Process inbound and outbound packets injected from
    *        the slow path (injected from the appliance).
    *
    * Optional - if missing, injected packets will be issued
    * directly with vmk_DVFilterIssuePackets().
    *
    * \param[in]     filter     Opaque pointer to the agent's filter
    *                           object.
    * \param[in,out] pktList    List of packets injected from the slow
    *                           path.
    * \param[in]     direction  The direction of the pkt.
    *
    * \retval VMK_OK  Upon return the pktList must be empty.
    * \retval Other   Packets remaining on the list upon return are
    *                 automatically issued via vmk_DVFilterIssuePackets().
    *
    ********************************************************************
    */
   VMK_ReturnStatus (*processSlowPathPackets)(
      vmk_DVFilterImpl filter,
      vmk_PktList pktList,
      vmk_DVFilterDirection direction);

   /********************************************************************
    *                                                             */ /**
    * \brief Process an ioctl request for a particular filter.
    *
    * This callback is invoked in response to a message from
    * the slow path agent requesting an ioctl operation associated
    * with a particular filter.
    *
    * Use this type of ioctl for communication between the slow
    * path and fast path agents when a particular filter is
    * concerned.
    *
    * It is possible for a filter ioctl request to send a zero length
    * ioctl request. When a request is received with a zero length
    * payloadLen, a payload pointer is passed as a NULL value.
    *
    *
    * \param[in] filter      Opaque pointer to the agent's filter object.
    * \param[in] requestId   Identifier of the request. Used as a
    *                        parameter to vmk_DVFilterSendFilterIoctlReply()
    *                        to pair the reply with the request. If the
    *                        requestId is VMK_DVFILTER_INVALID_IOCTL_REQUEST_ID
    *                        then the caller does not expect a reply.
    * \param[in] payload     Buffer containing the payload.
    * \param[in] payloadLen  Length of the payload in bytes.
    *
    ********************************************************************
    */
   void (*filterIoctlRequest)(
      vmk_DVFilterImpl filter,
      vmk_DVFilterRequestId requestId,
      void *payload,
      vmk_ByteCount payloadLen);

   /********************************************************************
    *                                                             */ /**
    * \brief Process an ioctl request from a slow path agent.
    *
    * This callback is invoked in response to a message from
    * the slow path agent requesting an ioctl operation. In contrast
    * to filterIoctlRequest(), the ioctl operation is not
    * specific to a particular filter.
    *
    * It is possible for a length slow path ioctl to send a zero
    * length ioctl request.  When a zero length ioctl request is
    * received the payload pointer is passed as a NULL value.
    *
    * Use this type of ioctl for general communication between the
    * slow path and fast path agents, i.e. where no particular
    * filter is concerned.
    *
    *
    * \param[in] slowPath    The slow path agent this request came from.
    * \param[in] requestId   Identifier of the request. Used as a
    *                        parameter to vmk_DVFilterSendSlowPathIoctlReply()
    *                        to pair the reply with the request. If the
    *                        requestId is VMK_DVFILTER_INVALID_IOCTL_REQUEST_ID
    *                        then the caller does not expect a reply.
    * \param[in] payload     Buffer containing the payload.
    * \param[in] payloadLen  Length of the payload in bytes.
    *
    ********************************************************************
    */
   void (*slowPathIoctlRequest)(
      vmk_DVFilterSlowPathImpl slowPath,
      vmk_DVFilterRequestId requestId,
      void *payload,
      vmk_ByteCount payloadLen);

   /********************************************************************
    *                                                             */ /**
    * \brief Respond to a filter configuration change.
    *
    * This callback is invoked when the filter configuration
    * changes. This includes parameters, the VM UUID and MTU.
    * Future versions might invoke this callback on additional
    * not yet known occasions.
    *
    * Optional - if missing, any parameter changes will not
    * be propagated to the filter, but VM UUID changes will be.
    *
    * Introduced in revision 1.1.0.0.
    *
    * \param[in] filter  Opaque pointer to the agent's filter object.
    *
    ********************************************************************
    */
   void (*configurationChanged)(vmk_DVFilterImpl filter);

} vmk_DVFilterFastPathOps;

/*
 ***********************************************************************
 * vmk_DVFilterSetSlowPathReply --                                */ /**
 *
 * \ingroup DVFilter
 * \brief Return the completion status of a previous
 *        vmk_DVFilterSetSlowPath() request.
 *
 * \param[in]  filter        Opaque pointer to the agent's filter object.
 * \param[in]  replyCbkData  Parameter set in vmk_DVFilterSetSlowPath().
 * \param[in]  status        VMK_OK (The filter was successfully attached
 *                                   to slow path agent),
 *                           VMK_NO_CONNECT (The filter or the slow path
 *                                          agent was disconnected),
 *                           VMK_TIMEOUT (The request timed out).
 *
 ***********************************************************************
 */
typedef void (*vmk_DVFilterSetSlowPathReply)(
   vmk_DVFilterImpl filter,
   void *replyCbkData,
   VMK_ReturnStatus status);

/********************************************************************
 * vmk_DVFilterFilterIoctlReply                                */ /**
 *
 * \ingroup DVFilter
 * \brief Process an ioctl reply.
 *
 * Callback function type for vmk_DVFilterSendFilterIoctlRequest().
 *
 * The reply callback is invoked upon reception of a reply
 * from the slow path agent, or on timeout.
 *
 * It is possible for DVFilter_SendFilterIoctlReply()
 * to send a zero length payload. If the payload length of the
 * reply is zero, then the payload buffer pointer passed is NULL.
 *
 * \param[in] filter        The filter that received the reply.
 * \param[in] replyCbkData  Parameter set in
 *                          vmk_DVFilterSendFilterIoctlRequest().
 * \param[in] payload       Buffer containing the payload.
 * \param[in] payloadLen    Length of the payload in bytes.
 * \param[in] status        VMK_OK if the reply is valid. Error code
 *                          otherwise.
 *
 ********************************************************************
 */
typedef void (*vmk_DVFilterFilterIoctlReply)(
   vmk_DVFilterImpl filter,
   void *replyCbkData,
   void *payload,
   vmk_ByteCount payloadLen,
   VMK_ReturnStatus status);

/********************************************************************
 * vmk_DVFilterRestoreSlowPathStateReply                       */ /**
 *
 * \ingroup DVFilter
 * \brief Process a RestoreSlowPathState reply.
 *
 * Callback function type for vmk_DVFilterRestoreSlowPathState().
 *
 * The reply callback is invoked upon reception of a reply
 * from the slow path agent, or on timeout.
 *
 * \param[in] filter         The filter that received the reply.
 * \param[in] replyCbkData   Parameter set in
 *                           vmk_DVFilterRestoreSlowPathState().
 * \param[in] status         VMK_OK if the reply is valid. Error code
 *                           otherwise.
 *
 ********************************************************************
 */
typedef void (*vmk_DVFilterRestoreSlowPathStateReply)(
   vmk_DVFilterImpl filter,
   void *replyCbkData,
   VMK_ReturnStatus status);

/********************************************************************
 * vmk_DVFilterSlowPathIoctlReply                              */ /**
 *
 * \ingroup DVFilter
 * \brief Process an ioctl reply.
 *
 * Callback function type for vmk_DVFilterSendSlowPathIoctlRequest().
 *
 * The reply callback is invoked upon reception of a reply
 * from the slow path agent, or on timeout.
 *
 * It is possible for DVFilter_SendSlowPathIoctlReply()
 * to send a zero length payload. If the payload length of the
 * reply is zero, then the payload buffer pointer passed is NULL.
 *
 * \param[in] slowPath      The slow path agent this reply came from.
 * \param[in] replyCbkData  Parameter set in
 *                          vmk_DVFilterSendSlowPathIoctlRequest().
 * \param[in] payload       Buffer containing the payload.
 * \param[in] payloadLen    Length of the payload in bytes.
 * \param[in] status        VMK_OK if the reply is valid. Error code
 *                          otherwise.
 *
 ********************************************************************
 */
typedef void (*vmk_DVFilterSlowPathIoctlReply)(
   vmk_DVFilterSlowPathImpl slowPath,
   void *replyCbkData,
   void *payload,
   vmk_ByteCount payloadLen,
   VMK_ReturnStatus status);


/********************************************************************
 * vmk_DVFilterRetrieveSlowPathStateReply                      */ /**
 *
 * \ingroup DVFilter
 * \brief Process a RetrieveSlowPathState reply.
 *
 * Callback function type for vmk_DVFilterRetrieveSlowPathState().
 *
 * Its possible for the saveState() request within the
 * appliance to provide a zero length reply. When the reply
 * is zero length, the payload buffer pointer passed is NULL.
 *
 * The reply callback is invoked upon reception of a reply
 * from the slow path agent, or on timeout.
 *
 * \param[in] filter        The filter that received the reply.
 * \param[in] replyCbkData  Parameter set in
 *                          vmk_DVFilterRetrieveSlowPathState().
 * \param[in] payload       Buffer containing the payload.
 * \param[in] payloadLen    Length of the payload in bytes.
 * \param[in] status        VMK_OK if the reply is valid. Error
 *                          code otherwise.
 *
 ********************************************************************
 */
typedef void (*vmk_DVFilterRetrieveSlowPathStateReply)(
   vmk_DVFilterImpl filter,
   void *replyCbkData,
   void *payload,
   vmk_ByteCount payloadLen,
   VMK_ReturnStatus status);


/**
 * \brief Handle for a DVFilter to track a property registration
 */
typedef struct DVFilterProperty *vmk_DVFilterProperty;

/**
 * \brief Flag fields passed to DVFilter property handler
 *
 * Currently, the flag can only take the value of VMK_DVFILTER_CLIENT_OPS_NONE.
 * Additional values can be added in the future.
 */
typedef enum vmk_DVFilterPropertyOpsFlags {
   /** not used right now */
   VMK_DVFILTER_CLIENT_OPS_NONE = 0x00000000,
} vmk_DVFilterPropertyOpsFlags;


/*
 ***********************************************************************
 * vmk_DVFilterPropertyWrite --                                   */ /**
 *
 * \brief Write callback for a DVFilter property
 *
 * This callback is invoked when management application initializes
 * a property or sets the property to a new value.
 *
 * \note For compatibility reasons, the implementation should check the
 *       flag value. Return not supported if it is not
 *       VMK_DVFILTER_CLIENT_OPS_NONE
 *
 * \param[in]   filter         Handle to DVFilter instance data
 * \param[in]   propName       Property name
 * \param[in]   propVal        Property value
 * \param[in]   propLen        Length of the property value in bytes
 * \param[in]   flags          Must be VMK_DVFILTER_CLIENT_OPS_NONE
 *                             Reserved for future use
 *
 * \retval      VMK_OK         DVFilter accepts value change of the
 *                             property on the instance
 * \retval      VMK_NOT_READY  DVFilter cannot accept value change of the
 *                             property on the instance. Likely due to
 *                             temporarily invalid filter. Write will be
 *                             called again with same data after filter
 *                             is established.
 * \retval      VMK_FAILURE    DVFilter does not accept the value change
 *                             of the property on the instance
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_DVFilterPropertyWrite)(
   vmk_DVFilterImpl filter,
   char *propName,
   void *propVal,
   vmk_ByteCountSmall propLen,
   vmk_DVFilterPropertyOpsFlags flags);


/*
 ***********************************************************************
 * vmk_DVFilterPropertyRead --                                    */ /**
 *
 * \brief Read callback for a property of a DVFilter client
 *
 * This callback is invoked when management application retrieves
 * the current value of the property. It is called only after
 * vmk_DVFilterPropertyPoll() returns VMK_TRUE.
 *
 * \param[in]   filter         Handle to DVFilter instance data
 * \param[in]   propName       Property name
 * \param[in]   propVal        Buffer to store property value
 * \param[in]   propLen        Length of the buffer
 * \param[in]   flags          Reserved for future use
 *
 * \retval      VMK_OK         Port client successfully retrieved value of
 *                             the data name (property) on the port
 * \retval      VMK_FAILURE    Port client could not retrieve the value
 *                             of the specified data name (property) on
 *                             the port
 ***********************************************************************
 */

typedef VMK_ReturnStatus (*vmk_DVFilterPropertyRead)(
   vmk_DVFilterImpl filter,
   char *propName,
   void *propVal,
   vmk_ByteCountSmall propLen,
   vmk_DVFilterPropertyOpsFlags flags);


/*
 ***********************************************************************
 * vmk_DVFilterPropertyPoll --                                    */ /**
 *
 * \brief Poll callback for a port property of a VDS client
 *
 * This callback is invoked when management queries the implementation
 * if there has been a change in the property value.
 *
 * \param[in]   filter         Handle to DVFilter instance data
 * \param[in]   propName       Property name
 * \param[out]  propLenPtr     Location to put the length of property value.
 *                             It is always a valid pointer.
 *
 * \retval      VMK_TRUE       If the specified property name
 *                             has a new value to be read; in this case
 *                             *propLenPtr is set to the length of
 *                             the data available to be read.
 * \retval      VMK_FALSE      If the specified data name (property)
 *                             has no new data value to be read.
 ***********************************************************************
 */

typedef vmk_Bool (*vmk_DVFilterPropertyPoll)(
   vmk_DVFilterImpl filter,
   char *propName,
   vmk_ByteCountSmall *propLenPtr);


/*
 ***********************************************************************
 * vmk_DVFilterPropertyCleanup --                                 */ /**
 *
 * \brief Cleanup callback for removing property
 *
 * This callback is called to unset a property. It is typically called
 * before a DVFilter is destroyed. The implementation should clean up
 * and free any resources associated with the specified property name
 * for the specified DVFilter instance
 *
 * \param[in]   handle         DVFilter handle
 * \param[in]   dataName       Data name
 *
 * \return      None           No return value
 ***********************************************************************
 */

typedef void (*vmk_DVFilterPropertyCleanup)(
   vmk_DVFilterImpl filter,
   char *propName);


/**
 * \brief Argument structure for specifyin operations
 *        when registering a DVFilter Client
 */

typedef struct vmk_DVFilterPropertyOps {
   /** handler for writing to properties */
   vmk_DVFilterPropertyWrite write;

   /** handler for reading properties */
   vmk_DVFilterPropertyRead read;

   /** handler for polling for changes to properties */
   vmk_DVFilterPropertyPoll poll;

   /** handler for cleaning up properties */
   vmk_DVFilterPropertyCleanup cleanup;
} vmk_DVFilterPropertyOps;


/*
 *
 * Functions
 *
 */


/*
 ***********************************************************************
 * vmk_DVFilterRegisterFastPath --                                */ /**
 *
 * \ingroup DVFilter
 * \brief Register a DVFilter fast path agent with the network stack.
 *
 * \nativedriversdisallowed
 *
 * \note This function may block.
 *
 * \param[in]  name            Human readable name of the agent.
 * \param[in]  apiRev          Version number of the DVFilter API
 *                             implemented by this agent.
 * \param[in]  agentRev        Fast path agent revision.
 * \param[in]  moduleID        ID of the agent's VMkernel module.
 * \param[in]  ops             Array of function pointers to the
 *                             agent's ops.
 * \param[in]  privateData     Opaque pointer to the agent object.
 * \param[out] fastPathHandle  Pointer to the VMkernel fast path
 *                             agent handle.
 *
 * \retval VMK_NOT_SUPPORTED API  Version mismatch.
 * \retval VMK_BAD_PARAM          Supplied parameters are not acceptable,
 *                                e.g. mandatory operations were missing
 *                                or the supplied name is too short.
 * \retval VMK_NAME_TOO_LONG      Supplied name is too long.
 * \retval VMK_NO_MEMORY          Out of memory.
 * \retval VMK_EXISTS             An agent with that name is already
 *                                registered.
 * \retval VMK_FAILURE            General error, e.g. DVFilter not
 *                                enabled on the host due to missing
 *                                license.
 * \retval VMK_OK                 Agent was successfully registered.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_DVFilterRegisterFastPath(
   const char *name,
   vmk_revnum apiRev,
   vmk_revnum agentRev,
   vmk_ModuleID moduleID,
   vmk_DVFilterFastPathOps *ops,
   vmk_DVFilterFastPathImpl privateData,
   vmk_DVFilterFastPathHandle *fastPathHandle);

/*
 ***********************************************************************
 * vmk_DVFilterUnregisterFastPath --                              */ /**
 *
 * \ingroup DVFilter
 * \brief Unregister a DVFilter fast path agent previously registered
 *        by vmk_DVFilterRegisterFastPath.
 *
 * \nativedriversdisallowed
 *
 * \note This function may block.
 *
 * \param[in] fastPathHandle  Handle to the VMkernel fast path agent.
 *
 * \retval VMK_BUSY  Agent is busy and can't be unregistered.
 * \retval VMK_OK    Agent successfully unregistered.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_DVFilterUnregisterFastPath(
   vmk_DVFilterFastPathHandle fastPathHandle);


/*
 ***********************************************************************
 * vmk_DVFilterSetSlowPath --                                     */ /**
 *
 * \ingroup DVFilter
 * \brief Associate a filter with the specified slow path agent.
 *
 * \nativedriversdisallowed
 *
 * \note This function will not block.
 *
 * Once the association is established, packets can be forwarded to
 * the agent via vmk_DVFilterFaultPackets().
 *
 * If the return value is not VMK_OK, the replyCbk will
 * not be invoked at a later time.
 *
 * \pre The filter and the slow path agent are handled by the same agent.
 *
 * \pre The filter is not currently associated with a slow path agent.
 *      In other words, you can't call vmk_DVFilterSetSlowPath twice,
 *      unless the slow path agent got disconnected between the two calls.
 *      This could happen due to a error on the communication channel
 *      or by calling vmk_DVFilterClearSlowPath().
 *
 * \param[in] filter        The filter to connect to the slow path agent.
 * \param[in] slowPath      The agent the filter is to be created on.
 * \param[in] replyCbk      Callback invoked upon response of the
 *                          slow path, disconnect, or timeout.
 * \param[in] replyCbkData  Parameter passed to the replyCbk.
 * \param[in] timeoutMs     Request timeout in milliseconds.
 *
 * \retval VMK_OK            The request was successfully queued.
 * \retval VMK_NOT_FOUND     Either the filter or the slowPath could not
 *                           be found.
 * \retval VMK_NO_CONNECT    Slow path not connected (yet).
 * \retval VMK_BUSY          The filter is already associated with a
 *                           different slow path agent.
 * \retval VMK_ESHUTDOWN     Failed due to filter currently shutting down.
 * \retval VMK_NO_MEMORY     Failed due to inability to acquire memory.
 * \retval VMK_NO_RESOURCES  Failed due to inability to allocate message
 *                           identifier. This may happen when too many
 *                           messages are in flight.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_DVFilterSetSlowPath(
   vmk_DVFilterHandle filter,
   vmk_DVFilterSlowPathHandle slowPath,
   vmk_DVFilterSetSlowPathReply replyCbk,
   void *replyCbkData,
   vmk_uint32 timeoutMs);

/*
 ***********************************************************************
 * vmk_DVFilterClearSlowPath --                                   */ /**
 *
 * \ingroup DVFilter
 * \brief Dissociate a filter from its slow path agent.
 *
 * \nativedriversdisallowed
 *
 * \note This function will not block.
 *
 * This function has no effect if the filter has no associated slow
 * path agent.
 *
 * From the moment of calling vmk_DVFilterSetSlowPath() till the
 * association actually succeeded or failed (indicated by the
 * vmk_DVFilterSetSlowPathReply callback), vmk_DVFilterClearSlowPath
 * can't be called (will return VMK_BUSY).
 *
 * \param[in] filter  Handle to the VMkernel filter object
 *
 * \retval VMK_OK              Successfully dissociated filter from the
 *                             slow path.
 * \retval VMK_BUSY            The filter is currently being associated
 *                             with a slow path agent. This operation
 *                             has to succeed or fail first.
 * \retval VMK_NO_MEMORY       No memory to send notification to
 *                             slow path.
 * \retval VMK_LIMIT_EXCEEDED  Not enough space in transmit queue for
 *                             notification to slow path.
 * \retval VMK_NOT_FOUND       Filter could not be found.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_DVFilterClearSlowPath(
   vmk_DVFilterHandle filter);

/*
 ***********************************************************************
 * vmk_DVFilterPktAllocMetadata --                                */ /**
 *
 * \ingroup DVFilter
 * \brief Allocate metadata for this packet.
 *
 * \nativedriversdisallowed
 *
 * \note This function will not block.
 *
 * The allocated metadata buffer can be used to store data
 * together with the packet. This mechanism may only be used
 * for packets being faulted to the slow path agent. The slow
 * path agent will be able to access the metadata.
 *
 * The use of this function for any other purpose is not
 * allowed. vmk_DVFilterIssuePackets() will free all left over
 * metadata, but it is not allowed to leave/place packets
 * with metadata on the pktList in processPackets().
 * You may not clone or free packets that have metadata associated
 * with them. Before performing any such operation, the meta data
 * needs to be freed using vmk_DVFilterPktReleaseMetadata.
 *
 * The metadata buffer is property of the DVFilter subsystem
 * and therefore it must not be freed by the caller of
 * this function. Instead, call vmk_DVFilterPktReleaseMetadata.
 *
 * Once metadata has been allocated, the size can't be changed
 * for the packet.
 *
 * This function may be called with fast path locks held.
 *
 * \param[in]  pkt   The packet to allocate metadata for.
 * \param[in]  len   Length of the metadata.
 * \param[out] addr  Pointer to the metadata buffer.
 *
 * \retval VMK_OK         Metadata has been allocated and addr has been
 *                        updated.
 * \retval VMK_NO_MEMORY  Memory allocation failed.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_DVFilterPktAllocMetadata(
   vmk_PktHandle *pkt,
   vmk_ByteCountSmall len,
   vmk_VA *addr);

/*
 ***********************************************************************
 * vmk_DVFilterPktReleaseMetadata --                              */ /**
 *
 * \ingroup DVFilter
 * \brief Release metadata for this packet.
 *
 * \nativedriversdisallowed
 *
 * \note This function will not block.
 *
 * Releases the metadata of this packet. Afterwards, the scope of
 * the packet is no longer limited, i.e. it can be freely passed
 * around in the VMkernel.
 *
 * This function may be called with fast path locks held.
 *
 * \param[in] pkt  The packet to free metadata on.
 *
 ***********************************************************************
 */
void vmk_DVFilterPktReleaseMetadata(
   vmk_PktHandle *pkt);

/*
 ***********************************************************************
 * vmk_DVFilterPktGetMetadata --                                  */ /**
 *
 * \ingroup DVFilter
 * \brief Get pointer to metadata of this packet.
 *
 * \nativedriversdisallowed
 *
 * \note This function will not block.
 *
 * Gets the address of the metadata associated with this packet,
 * as well as the length of the metadata. In case there is no
 * metadata the function will return NULL. The metadata is
 * property of the DVFilter subsystem, i.e. can't be freed by
 * the caller directly. Instead call vmk_DVFilterPktReleaseMetadata.
 *
 * \param[in]  pkt  The packet to get the metadata for.
 * \param[out] len  Length of the metadata.
 *
 * \return Address of the metadata.
 *
 ***********************************************************************
 */
vmk_VA vmk_DVFilterPktGetMetadata(
   vmk_PktHandle *pkt,
   vmk_ByteCountSmall *len);

/*
 ***********************************************************************
 * vmk_DVFilterFaultPackets --                                    */ /**
 *
 * \ingroup DVFilter
 * \brief Fault specified packets to the slow path agent associated
 *        with the filter.
 *
 * \nativedriversdisallowed
 *
 * \note This function will not block.
 *
 * \note Unless the VMK_DVFILTER_FAULTING_COPY flag is used, the
 *       function clears the packet list on success as well as
 *       failures other than VMK_BAD_PARAM. This releases the packets.
 *
 * \param[in] filter     Handle to the VMkernel filter object.
 * \param[in] pktList    List of packets to send.
 * \param[in] direction  Direction the packets should be sent.
 * \param[in] flags      Flags.
 *
 * \retval VMK_OK          The packets were successfully enqueued
 *                         for forwarding.
 * \retval VMK_NO_CONNECT  The filter is not associated with a slow
 *                         path agent.
 * \retval VMK_NOT_FOUND   Filter could not be found.
 * \retval VMK_BAD_PARAM   Invalid flags or direction.
 * \retval VMK_FAILURE     Unknown failure.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_DVFilterFaultPackets(
   vmk_DVFilterHandle filter,
   vmk_PktList pktList,
   vmk_DVFilterDirection direction,
   vmk_DVFilterFaultingFlags flags);

/*
 ***********************************************************************
 * vmk_DVFilterIssuePackets --                                    */ /**
 *
 * \ingroup DVFilter
 * \brief Inject packets in a filter.
 *
 * \nativedriversdisallowed
 *
 * \note This function will not block.
 *
 * Used to inject new packets, or resume processing of packets
 * passed to the filter via the processPackets() callback.
 *
 * Any packet injected by the fast-path agent must meet a number
 * of requirements, including:
 *
 * - Newly vmk_PktAlloc()'d frames must have the source port ID
 *   assigned (see: vmk_DVFilterPktSetSourcePort() )
 * - All packets must have complete ethernet headers.
 * - The mapped region of the packet must be mapped from
 *   the first frag (see vmk_PktFragGet() and other vmkapi pkt
 *   frag APIs for more details).
 * - The sum of all the fragment lengths must be equal to the
 *   frame length.
 *
 * For TSO frames, additional consistency requirements must be met:
 *
 * - The mapped region should at least contain the complete
 *   internet headers: ethernet header, IPv4/IPv6 header plus
 *   options, the TCP header plus any TCP options.
 * - The IP header should not indicate any fragmentation.
 * - The protocol described by the IP header header must be TCP.
 * - The packet's frame length must be larger than the TSO MSS,
 * - When the IP frame's length fields is non-zero,
 *   the packet frame length must contain at least that length.
 *
 * Any packets passed to the fast path through processPackets()
 * have already met these requirements
 *
 * \note The function clears the packet list on both success and
 *       failure. This releases the packets.
 *
 * \param[in] filter     Handle to the VMkernel filter object.
 * \param[in] pktList    List of packets to send.
 * \param[in] direction  Direction the packets should be sent.
 *
 * \retval VMK_OK         The packets were successfully enqueued
 *                        for forwarding.
 * \retval VMK_NOT_FOUND  Filter or its backing port could not be found.
 * \retval VMK_BAD_PARAM  Direction parameter invalid.
 * \retval Other          The packets could not be forwarded.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_DVFilterIssuePackets(
   vmk_DVFilterHandle filter,
   vmk_PktList pktList,
   vmk_DVFilterDirection direction);

/*
 ***********************************************************************
 * vmk_DVFilterGetName --                                         */ /**
 *
 * \ingroup DVFilter
 * \brief Query the human readable name of the DVFilter.
 *
 * \nativedriversdisallowed
 *
 * \note This function will not block.
 *
 * \param[in]  filter  Handle to the VMkernel filter object
 * \param[out] name    Pointer to a buffer to fill in with the
 *                     filter's name.
 * \param[in]  len     Length of the supplied buffer.
 *
 * \retval VMK_NOT_FOUND      Filter could not be found.
 * \retval VMK_BUF_TOO_SMALL  Supplied buffer is too small.
 * \retval VMK_OK             Success.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_DVFilterGetName(
   vmk_DVFilterHandle filter,
   char *name,
   vmk_ByteCount len);

/*
 ***********************************************************************
 * vmk_DVFilterGetVMUUID --                                       */ /**
 *
 * \ingroup DVFilter
 * \brief Query the VM UUID of the VM the DVFilter is attached to.
 *
 * \nativedriversdisallowed
 *
 * \note This function will not block.
 *
 * \note This function may fail with VMK_INVALID_WORLD in the
 *       destroyFilter callback.
 *
 * \note Please call vmk_VMUUIDGetMaxLength to size the buffer
 *       appropriately.
 *
 * \param[in]  filter  Handle to the VMkernel filter object.
 * \param[out] uuid    Pointer to a buffer to fill in with the UUID.
 * \param[in]  len     Length of the supplied buffer.
 *
 * \retval VMK_NOT_FOUND        Filter could not be found.
 * \retval VMK_NOT_INITIALIZED  No VM UUID has been set for this VM.
 * \retval VMK_BUF_TOO_SMALL    Supplied buffer is too small.
 * \retval VMK_NOT_SUPPORTED    Filter does not have a VM endpoint.
 * \retval VMK_INVALID_WORLD    Filter's world has been destroyed.
 * \retval VMK_OK               Success.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_DVFilterGetVMUUID(
   vmk_DVFilterHandle filter,
   vmk_UUID uuid,
   vmk_ByteCount len);

/*
 ***********************************************************************
 * vmk_DVFilterGetConfigParameter --                              */ /**
 *
 * \ingroup DVFilter
 * \brief Query a configuration parameter value of the DVFilter.
 *
 * \nativedriversdisallowed
 *
 * \note This function will not block.
 *
 * These parameters may change at any time if the fastpath op
 * configurationChanged is set. Callers should use the
 * configurationChanged callback to ensure consistency if
 * necessary. If configurationChanged is not set, the parameters
 * will not change.
 *
 * \param[in]  filter      Handle to the VMkernel filter object.
 * \param[in]  paramIndex  Index of the filter configuration parameter
                           that should be retrieved.
 * \param[out] param       Pointer to a buffer to fill in with the
 *                         requested filter configuration parameter
 *                         value.
 * \param[in]  len         Length of the supplied buffer.
 *
 * \retval VMK_NOT_FOUND      Filter or parameter could not be found
 * \retval VMK_BAD_PARAM      paramIndex out of range
 * \retval VMK_BUF_TOO_SMALL  Supplied buffer is too small
 * \retval VMK_OK             Success
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_DVFilterGetConfigParameter(
   vmk_DVFilterHandle filter,
   vmk_uint8 paramIndex,
   char *param,
   vmk_ByteCount len);

/*
 ***********************************************************************
 * vmk_DVFilterGetFailurePolicy --                                */ /**
 *
 * \ingroup DVFilter
 * \brief Get the failure policy the user configured for this filter.
 *
 * \nativedriversdisallowed
 *
 * \note This function will not block.
 *
 * \param[in]  filter  Handle to the VMkernel filter object.
 * \param[out] policy  Policy for the filter object.
 *
 * \retval VMK_OK         Success.
 * \retval VMK_NOT_FOUND  Invalid filter handle.
 * \retval VMK_BAD_PARAM  Policy is NULL.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_DVFilterGetFailurePolicy(
   vmk_DVFilterHandle filter,
   vmk_DVFilterFailurePolicy *policy);


/*
 ***********************************************************************
 * vmk_DVFilterGetSwitchMTU --                                    */ /**
 *
 *  \ingroup DVFilter
 *  \brief Get the MTU the switch has configured for this filter
 *
 * \nativedriversdisallowed
 *
 * \note This function will not block.
 *
 * Value of *mtu is not defined in case the function was not successful.
 *
 * \note The vNic might have a different MTU configured. This API
 *       provides the MTU configured on the switch, not the vNic.
 *
 * \note The switch MTU value is not always available.  In particular,
 *       the MTU cannot be determined if the filter is attached to a
 *       3rd party vDS.  This limitation may be removed in the future.
 *
 * \note This function was introduced in API revision v2_0_0_0.
 *
 * \param[in]  filter Handle to the VMkernel filter object
 * \param[out] mtu    MTU of the switch for this filter
 *
 * \retval VMK_OK             Success
 * \retval VMK_NOT_SUPPORTED  MTU value not available on this switch
 * \retval VMK_NOT_FOUND      Invalid filter handle
 * \retval VMK_BAD_PARAM      mtu is NULL
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_DVFilterGetSwitchMTU(
   vmk_DVFilterHandle filter,
   vmk_uint32 *mtu);

/*
 ***********************************************************************
 * vmk_DVFilterGetEndPointType --                                 */ /**
 *
 * \ingroup DVFilter
 * \brief Get the type of the filter's end-point.
 *
 * \nativedriversdisallowed
 *
 * \note This function will not block.
 *
 * The filter end-point might be a virtual nic of a VM, a VMkernel
 * NIC (vmknic) or an uplink.
 *
 * Note: This function is for future extension. Right now DVFilter
 * only supports VM vNics.
 *
 * \param[in]  filter  Handle to the VMkernel filter object.
 * \param[out] type    Pointer of the vmk_DVFilterEndPointType to fill in.
 *
 * \retval VMK_OK         Success.
 * \retval VMK_NOT_FOUND  Invalid filter handle.
 * \retval VMK_BAD_PARAM  Type is NULL.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_DVFilterGetEndPointType(
   vmk_DVFilterHandle filter,
   vmk_DVFilterEndPointType *type);

/*
 ***********************************************************************
 * vmk_DVFilterGetPortID --                                    */ /**
 *
 * \ingroup DVFilter
 * \brief Get the portID that the filter is attached to.
 *
 * \nativedriversdisallowed
 *
 * \note This function will not block.
 *
 * \param[in]  filter  Handle to the VMkernel filter object.
 * \param[out] portID  Pointer of the vmk_SwitchPortID to fill in.
 *
 * \retval VMK_OK             Success.
 * \retval VMK_NOT_FOUND      Failed to lookup filter.
 * \retval VMK_BAD_PARAM      portID is NULL.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_DVFilterGetPortID(
   vmk_DVFilterHandle filter,
   vmk_SwitchPortID *portID);

/*
 ***********************************************************************
 * vmk_DVFilterGetVnicIndex --                                    */ /**
 *
 * \ingroup DVFilter
 * \brief Get the index of the vNic this filter is attached to.
 *
 * \nativedriversdisallowed
 *
 * \note This function will not block.
 *
 * This function is only supported for VM endpoints.
 *
 * \param[in]  filter  Handle to the VMkernel filter object.
 * \param[out] index   Pointer of the vmk_uint32 to fill in.
 *
 * \retval VMK_OK             Success.
 * \retval VMK_NOT_FOUND      Invalid filter handle.
 * \retval VMK_NOT_SUPPORTED  Filter does not have a VM endpoint.
 * \retval VMK_BAD_PARAM      index is NULL.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_DVFilterGetVnicIndex(
   vmk_DVFilterHandle filter,
   vmk_uint32 *index);


/*
 ***********************************************************************
 * vmk_DVFilterSetMinPktMappedLen                                 */ /**
 *
 * \ingroup DVFilter
 * \brief Set the minimum mapped packet length required by this filter.
 *
 * \nativedriversdisallowed
 *
 * \note This function will not block.
 *
 * The function requests new packets to have a specific
 * mapped length. It's not currently possible for all pkt
 * generators to adhere to this mapped value. If a specific
 * mapped length is required, code will need to interrogate each pkt
 * to validate the required length is present.
 *
 * To avoid unnecessary remapping overhead, filters should only
 * request as many bytes as necessary.
 *
 * Due to VMkernel needs, every attempt is made to try to
 * provide all inet headers within the mapped length.
 *
 * \param[in] filter     Handle to the VMkernel filter object.
 * \param[in] len        Minimum packet length to map for each and
 *                       every packet received on the filter.
 * \param[in] direction  Requested direction(s).
 *
 * \retval VMK_NOT_FOUND Filter could not be found
 * \retval VMK_OK Success
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_DVFilterSetMinPktMappedLen(
   vmk_DVFilterHandle filter,
   vmk_ByteCountSmall len,
   vmk_DVFilterDirection direction);

/*
 ***********************************************************************
 * vmk_DVFilterSendFilterIoctlRequest --                          */ /**
 *
 * \ingroup DVFilter
 * \brief Send an ioctl request associated with a particular filter to
 *        the slow path.
 *
 * \nativedriversdisallowed
 *
 * \note This function will not block.
 *
 * Send an ioctl request to the slow path agent associated with the
 * filter. The provided callback will be called if the slow path agent
 * replies, on disconnect and timeout, but only if this function
 * returns VMK_OK.
 *
 * Use this type of ioctl for communication between the slow
 * path and fast path agents when a particular filter is
 * concerned.
 *
 * It is acceptable to send filter ioctl requests with a zero dataLen.
 * When the dataLen value is zero, the data pointer is never
 * dereferenced.
 *
 * If the return value is not VMK_OK, the replyCbk will
 * not be invoked at any later time.
 *
 * \param[in] filter        Filter to send the request on.
 * \param[in] data          Payload.
 * \param[in] dataLen       Length of the payload in bytes.
 * \param[in] replyCbk      Callback invoked on reply, disconnect,
 *                          or timeout.
 * \param[in] replyCbkData  Parameter to the replyCbk.
 * \param[in] timeoutMs     Request timeout in milliseconds.
 *
 * \retval VMK_OK              Ioctl request succeeded.
 * \retval VMK_ESHUTDOWN       Failed due to filter currently
 *                             shutting down.
 * \retval VMK_NO_MEMORY       Failed due to inability to acquire memory.
 * \retval VMK_NO_RESOURCES    Failed due to inability to allocate
 *                             message identifier. This may happen when
 *                             too many messages are in flight.
 * \retval VMK_NO_CONNECT      Slow path no longer connected.
 * \retval VMK_LIMIT_EXCEEDED  Transmit queue length is too large.
 * \retval VMK_BAD_PARAM       NULL filter or NULL data with
 *                             nonzero length.
 * \retval Other               The filter ioctl request could not
 *                             be queued.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_DVFilterSendFilterIoctlRequest(
   vmk_DVFilterHandle filter,
   void *data,
   vmk_ByteCount dataLen,
   vmk_DVFilterFilterIoctlReply replyCbk,
   void *replyCbkData,
   vmk_uint32 timeoutMs);

/*
 ***********************************************************************
 * vmk_DVFilterSendFilterIoctlReply --                            */ /**
 *
 * \ingroup DVFilter
 * \brief Send an ioctl reply in filter context to the guest appliance.
 *
 * \nativedriversdisallowed
 *
 * \note This function will not block.
 *
 * Send an ioctl reply to the slow path agent associated with the
 * filter.
 *
 * It is acceptable to send ioctl reply with a zero dataLen.
 * If the dataLen is zero, the data pointer is never dereferenced.
 *
 * \param[in] filter     Filter that received the original request.
 * \param[in] requestId  Identifier of the original request. See
 *                       vmk_DVFilterFastPathOps::ioctlRequest().
 * \param[in] data       Payload.
 * \param[in] dataLen    Length of the payload in bytes.
 *
 * \retval VMK_OK              The ioctl reply was successfully queued.
 * \retval VMK_NO_CONNECT      Slow path no longer connected.
 * \retval VMK_NO_MEMORY       Failed due to inability to acquire memory.
 * \retval VMK_LIMIT_EXCEEDED  Transmit queue length is too large.
 * \retval VMK_BAD_PARAM       NULL filter, or NULL data with
 *                             nonzero length.
 * \retval Other               The ioctl reply could not be queued.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_DVFilterSendFilterIoctlReply(
   vmk_DVFilterHandle filter,
   vmk_DVFilterRequestId requestId,
   void *data,
   vmk_ByteCount dataLen);


/*
 ***********************************************************************
 * vmk_DVFilterSendSlowPathIoctlRequest --                        */ /**
 *
 * \ingroup DVFilter
 * \brief Send an ioctl request to a slow path agent.
 *
 * \nativedriversdisallowed
 *
 * \note This function will not block.
 *
 * Send an ioctl request to the slow path agent. The provided
 * callback will be called if the slow path agent replies, on disconnect
 * and timeout, but only if this function returns VMK_OK.
 *
 * Use this type of ioctl for general communication between the
 * slow path and fast path agents, i.e. where no particular
 * filter is concerned.
 *
 * It is acceptable to send an slow path ioctl request with
 * a zero length dataLen.  When dataLen is zero, the data
 * pointer value is never dereferenced.
 *
 * If the return value is not VMK_OK, the replyCbk will
 * not be invoked at any later time.
 *
 * \param[in]  slowPath      Slow path agent to send the request to.
 * \param[in]  data          Payload.
 * \param[in]  dataLen       Length of the payload in bytes.
 * \param[in]  replyCbk      Callback invoked on reply, disconnect,
 *                           or timeout.
 * \param[in]  replyCbkData  Parameter to the replyCbk.
 * \param[in]  timeoutMs     Request timeout in milliseconds.
 *
 * \retval VMK_OK              Slow path ioctl request succeeded.
 * \retval VMK_NO_MEMORY       Failed due to inability to acquire memory.
 * \retval VMK_NO_RESOURCES    Failed due to inability to allocate.
 *                             message identifier. This may happen
 *                             when too many messages are in flight.
 * \retval VMK_NOT_FOUND       Could not identify slow path from
 *                             slowPath handle.
 * \retval VMK_NO_CONNECT      Slow path no longer connected.
 * \retval VMK_LIMIT_EXCEEDED  Transmit queue length is too large.
 * \retval VMK_BAD_PARAM       NULL data with nonzero length.
 * \retval Other               The slow path ioctl request could
 *                             not be queued.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_DVFilterSendSlowPathIoctlRequest(
   vmk_DVFilterSlowPathHandle slowPath,
   void *data,
   vmk_ByteCount dataLen,
   vmk_DVFilterSlowPathIoctlReply replyCbk,
   void *replyCbkData,
   vmk_uint32 timeoutMs);

/*
 ***********************************************************************
 * vmk_DVFilterSendSlowPathIoctlReply --                          */ /**
 *
 * \ingroup DVFilter
 * \brief Send an ioctl reply to a slow path agent.
 *
 * \nativedriversdisallowed
 *
 * \note This function will not block.
 *
 * It is acceptable to send a slow path reply with a zero length.
 * If the dataLen is zero, then the data pointer is never dereferenced.
 *
 * \param[in] slowPath   Slow path agent to send the reply to.
 * \param[in] requestId  Identifier of the original request. See
 *                       vmk_DVFilterFastPathOps::ioctlRequest().
 * \param[in] data       Payload.
 * \param[in] dataLen    Length of the payload in bytes.
 *
 * \retval VMK_OK              The slow path ioctl reply was
 *                             successfully queued.
 * \retval VMK_NO_MEMORY       Failed due to inability to acquire memory.
 * \retval VMK_NOT_FOUND       Could not identify slow path from
 *                             slowPath handle.
 * \retval VMK_NO_CONNECT      Slow path no longer connected.
 * \retval VMK_LIMIT_EXCEEDED  Transmit queue length is too large.
 * \retval VMK_BAD_PARAM       NULL data with nonzero length.
 * \retval Other               The slow path ioctl reply could not
 *                             be queued.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_DVFilterSendSlowPathIoctlReply(
   vmk_DVFilterSlowPathHandle slowPath,
   vmk_DVFilterRequestId requestId,
   void *data,
   vmk_ByteCount dataLen);

/*
 ***********************************************************************
 * vmk_DVFilterRestoreSlowPathState --                            */ /**
 *
 * \ingroup DVFilter
 * \brief Restore slow path state in the guest appliance
 *
 * \nativedriversdisallowed
 *
 * \note This function will not block.
 *
 * The slow path's state is sent to the slow path agent. The provided
 * callback will be called when the restore reply message is
 * returned, or when the indicated timeout expires.
 *
 * If the dataLen is zero, then the data pointer is never dereferenced.
 *
 * If the return value is not VMK_OK, the replyCbk will
 * not be invoked at any later time.
 *
 *
 * \param[in] filter        Filter to send the request on.
 * \param[in] data          Payload (i.e. state).
 * \param[in] dataLen       Length of the payload/state in bytes.
 * \param[in] replyCbk      Callback invoked on reply, disconnect,
 *                          or timeout.
 * \param[in] replyCbkData  Parameter to the replyCbk.
 * \param[in] timeoutMs     Request timeout in milliseconds.
 *
 * \retval VMK_OK              Restore slow state request succeeded.
 * \retval VMK_ESHUTDOWN       Failed due to filter currently
 *                             shutting down.
 * \retval VMK_NO_MEMORY       Failed due to inability to acquire memory.
 * \retval VMK_NO_RESOURCES    Failed due to inability to allocate
 *                             message identifier. This may happen
 *                             when too many messages are in flight.
 * \retval VMK_NO_CONNECT      Slow path no longer connected.
 * \retval VMK_LIMIT_EXCEEDED  Transmit queue length is too large.
 * \retval VMK_BAD_PARAM       NULL data with nonzero length.
 * \retval Other               The restore slow path state request
 *                             could not be queued.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_DVFilterRestoreSlowPathState(
   vmk_DVFilterHandle filter,
   void *data,
   vmk_ByteCount dataLen,
   vmk_DVFilterRestoreSlowPathStateReply replyCbk,
   void *replyCbkData,
   vmk_uint32 timeoutMs);

/*
 ***********************************************************************
 * vmk_DVFilterRetrieveSlowPathState --                           */ /**
 *
 * \ingroup DVFilter
 * \brief Retrieve slow path state from the guest appliance.
 *
 * \nativedriversdisallowed
 *
 * \note This function will not block.
 *
 * The slow path agent is asked to provide its state. The provided
 * callback (replyCbk) will be called, containing the
 * state the slow path agent provided (on success). The callback will
 * be invoked when the retrieve reply is returned, or when the
 * indicated timeout expires.
 *
 * If the return value is not VMK_OK, the replyCbk will
 * not be invoked at any later time.
 *
 * \param[in] filter        Filter to send the request on.
 * \param[in] replyCbk      Callback invoked on reply, disconnect,
 *                          or timeout.
 * \param[in] replyCbkData  Parameter to the replyCbk.
 * \param[in] timeoutMs     Request timeout in milliseconds.
 *
 * \retval VMK_OK              Retrieve slow state request succeeded.
 * \retval VMK_ESHUTDOWN       Failed due to filter currently
 *                             shutting down.
 * \retval VMK_NO_MEMORY       Failed due to inability to acquire memory.
 * \retval VMK_NO_RESOURCES    Failed due to inability to allocate
 *                             message identifier. This may happen when
 *                             too many messages are in flight.
 * \retval VMK_NO_CONNECT      Slow path no longer connected.
 * \retval VMK_BAD_PARAM       replyCbk missing, it must be supplied.
 * \retval VMK_LIMIT_EXCEEDED  Transmit queue length is too large.
 * \retval VMK_BAD_PARAM       NULL filter argument.
 * \retval Other               The retrieve slow path state request
 *                             could not be queued.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_DVFilterRetrieveSlowPathState(
   vmk_DVFilterHandle filter,
   vmk_DVFilterRetrieveSlowPathStateReply replyCbk,
   void *replyCbkData,
   vmk_uint32 timeoutMs);

/*
 ***********************************************************************
 * vmk_DVFilterPktSetSourcePort --                                */ /**
 *
 * \ingroup DVFilter
 * \brief Set the src port to the port this filter is installed on.
 *
 * \nativedriversdisallowed
 *
 * \note This function will not block.
 *
 * Set the packet source port identification field to the port
 * the filter is attached to. The source port identification is
 * used to determine the originator of the packet, and among other
 * uses, contributes to resource stats gathering (also:
 * vmk_PktSetCompletionData() ).
 *
 * A source port must be assigned for the packet to be correctly
 * processed by vmk_DVFilterIssuePackets() or by processPackets().
 * It is the responsibility of the module which allocates the packet
 * to assign the source port id. Newly allocated packets from
 * vmk_PktAlloc() do not have the source port id assigned.
 * Packets passed to processPackets() ought to already have
 * the source port identifier assigned.
 *
 * Please note that the port the filter is attached to might
 * change, e.g. upon reboot of the VM. Those events are transparent
 * to the filter. As a consequence, in these situations the packet
 * might get dropped.
 *
 * If the pkt's metadata is not writable, then VMK_NOT_SUPPORTED is
 * returned. If the pkt has completion data associated,
 * VMK_NOT_SUPPORTED is returned. Completion data is associated for
 * pkts using resources belonging to the pkt's originator,
 * for example guest buffers, or possibly physical nic resources.
 *
 * \param[in] filter  Filter the port information should be taken from.
 * \param[in] pkt     The packet.
 *
 * \retval VMK_OK             Success.
 * \retval VMK_NOT_FOUND      Invalid filter handle.
 *         VMK_NOT_SUPPORTED  Meta data is not writable, or the
 *                            pktHandle has completion data associated.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_DVFilterPktSetSourcePort(
   vmk_DVFilterHandle filter,
   vmk_PktHandle *pkt);

/*
 ***********************************************************************
 * vmk_DVFilterAllocPktTag --                                     */ /**
 *
 * \ingroup DVFilter
 * \brief Allocate a packet tag for the indicated fast path agent.
 *
 * \nativedriversdisallowed
 *
 * \note This function will not block.
 *
 * DVFilter provides for tagging pkts as an aid in
 * preventing multiple inspections of the same pkt.
 * The tags are a limited resource. Not all agents
 * may want to use the tagging facility to tag frames.
 *
 * Tags returned by this function can be or'd with each other.
 *
 *
 * \param[in]  fastPathHandle  Fast path agent to be associated with
 *                             the tag bit.
 * \param[out] tagMask         The allocated tag.
 *
 * \retval VMK_OK            Success. tagMask was updated.
 * \retval VMK_BAD_PARAM     Null tagMask.
 * \retval VMK_NOT_FOUND     Unknown agent.
 * \retval VMK_NO_RESOURCES  No tag bit is available.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_DVFilterAllocPktTag(
   vmk_DVFilterFastPathHandle fastPathHandle,
   vmk_uint32 *tagMask);

/*
 ***********************************************************************
 * vmk_DVFilterFreePktTags --                                     */ /**
 *
 * \ingroup DVFilter
 * \brief Free all the packet tags allocated by the supplied agent.
 *
 * \nativedriversdisallowed
 *
 * \note This function will not block.
 *
 * One or more pkt tags, each one bit, acquired from
 * vmk_DVFilterAllocPktTag() are released.
 *
 * \param[in]  fastPathHandle Fast path agent whose tags are freed.
 *
 ***********************************************************************
 */
void vmk_DVFilterFreePktTags(
   vmk_DVFilterFastPathHandle fastPathHandle);


/*
 ***********************************************************************
 * vmk_DVFilterPktSetTags --                                      */ /**
 *
 * \ingroup DVFilter
 * \brief Tag the packet with the specified tag.
 *
 * \nativedriversdisallowed
 *
 * \note This function will not block.
 *
 * \pre The tag must belong to the supplied fast path agent.
 *
 * Set the indicated tags bits of packet with respect
 * to the identified fast path agent.
 *
 * The tag is intended to be used to allow filters to
 * identify those pkts which have been already inspected.
 * The tags currently set on a packet can be queried using
 * vmk_DVFilterGetPktTags().
 *
 * If neither vmk_DVFilterPktSetTags() nor vmk_DVFilterPktClearTags()
 * is performed on a packet, vmk_DVFilterGetPktTags() will return
 * a result as if vmk_DVFilterPktClearTags() had been initially
 * performed on the packet.
 *
 * The setMask parameter is an or'd mask of bits returned
 * through vmk_DVFilterAllocPktTag(). Multiple bits can
 * be enabled in the mask.
 *
 * If a bit which wasn't acquired through vmk_DVFilterAllocPktTag()
 * is set in setMask, VMK_FAILURE is returned, and no bits
 * within the pkt tag bits are modified.
 *
 * \param[in] fastPathHandle  Fast path agent whose tags are set.
 * \param[in] pkt             The packet.
 * \param[in] setMask         The or'd collection of bits to set.
 *
 * \retval VMK_OK         The indicated tag is available for the agent,
 *                        and its value has been set.
 * \retval VMK_NOT_FOUND  FastPathHandle parameter invalid.
 * \retval VMK_FAILURE    SetMask contains a bit not allocated for
 *                        this agent.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_DVFilterPktSetTags(
   vmk_DVFilterFastPathHandle fastPathHandle,
   vmk_PktHandle *pkt,
   vmk_uint32 setMask);


/*
 ***********************************************************************
 * vmk_DVFilterPktClearTags --                                    */ /**
 *
 * \ingroup DVFilter
 * \brief Clear a fast path's tags from a packet.
 *
 * \nativedriversdisallowed
 *
 * \note This function will not block.
 *
 * Clear the tag or tag bits of the pkt with respect to the
 * identified fast path agent
 *
 * The clearMask parameter is an or'd mask of bits returned
 * through vmk_DVFilterAllocPktTag(). Multiple bits can
 * be enabled in the mask.
 *
 * If multiple tag bits were acquired via vmk_DVFilterAllocPktTag(),
 * and not all of the these bits are set in the clearMask,
 * the zeroed bits within the mask will not change the value of the
 * associated zero bits within the pkt's tags. The clearMask
 * only indicates those mask bits to set to zero, it does
 * not describe bits to be set.
 *
 * If a bit which wasn't acquired through vmk_DVFilterAllocPktTag()
 * is set in clearMask, VMK_FAILURE is returned, and no
 * bits within the pkt tags are modified.
 *
 * \param[in]  fastPathHandle  Fast path agent whose tags are cleared.
 * \param[in]  pkt             The packet.
 * \param[in]  clearMask       The or'd collection of bits to clear.
 *
 * \retval VMK_OK         The indicated tag is available for the agent,
 *                        and its value has been cleared.
 * \retval VMK_NOT_FOUND  fastPathHandle parameter invalid.
 * \retval VMK_FAILURE    clearMask contains a bit not allocated
 *                        for this agent.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_DVFilterPktClearTags(
   vmk_DVFilterFastPathHandle fastPathHandle,
   vmk_PktHandle *pkt,
   vmk_uint32 clearMask);


/*
 ***********************************************************************
 * vmk_DVFilterGetPktTags --                                      */ /**
 *
 * \ingroup DVFilter
 * \brief Query the values of one or more bits from the pkt tag.
 *
 * \nativedriversdisallowed
 *
 * \note This function will not block.
 *
 * Query the tag bits set on a packet.
 * The bits of interest are passed in through getMask.
 * The getMask must be a collection of or'd bits
 * from previous calls to vmk_DVFilterAllocPktTag().
 *
 * \param[in] fastPathHandle  fastPathHandle whose tags are queried.
 * \param[in] pkt             The packet.
 * \param[in] getMask         One or more or'd bits acquired through
 *                            vmk_DVFilterAllocPktTag(), describing the
 *                            bits to select from within the pkt tags.
 * \param[out] setMask        A pointer to a vmk_uint32. The indirected
 *                            pointer will contain the result of
 *                            the query.
 *
 * \retval VMK_OK         Success, result stored in *setMask.
 * \retval VMK_NOT_FOUND  fastPathHandle parameter invalid.
 * \retval VMK_FAILURE    getMask contains a bit not allocated for
 *                        this agent.
 * \retval VMK_BAD_PARAM  setMask is NULL
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_DVFilterGetPktTags(
   vmk_DVFilterFastPathHandle fastPathHandle,
   vmk_PktHandle *pkt,
   vmk_uint32 getMask,
   vmk_uint32 *setMask);


/*
 ***********************************************************************
 * vmk_DVFilterPropertyRegister --                                */ /**
 *
 * \brief Register property operations for a DVFilter
 *
 * \nativedriversdisallowed
 *
 * DVFilter implementation calls this interface to register a property
 * handler for a property of a given name. The property name must
 * start with the agent name pass to vmk_DVFilterRegisterFastPath().
 *
 * On success, handle is updated to contain an opaque value to be
 * used later to unregsiter the property handler.
 *
 * \param[out]  handle         Ptr to handle for DVFilter property
 * \param[in]   propName       Property name
 * \param[in]   ops            DVFilter property ops to use
 *
 * \retval      VMK_OK         DVFilter client registered successfully
 * \retval      VMK_EXISTS     A client already exists for the given
 *                             dataName
 * \retval      VMK_NO_MEMORY  Insufficient memory to perform
 *                             registration
 * \retval      VMK_BAD_PARAM  A required callback is unspecified (NULL)
 ***********************************************************************
 */

VMK_ReturnStatus vmk_DVFilterPropertyRegister(
   vmk_DVFilterProperty *handle,
   const char *propName,
   const vmk_DVFilterPropertyOps *ops);


/*
 ***********************************************************************
 * vmk_DVFilterPropertyUnregister --                              */ /**
 *
 * \brief Unregister a DVFilter Client
 *
 * \nativedriversdisallowed
 *
 * The handle is the token obtained from vmk_DVFilterPropertyRegister().
 *
 * \param[in]   handle         Handle for DVFilter property registration
 *
 * \retval      VMK_OK         DVFilter client unregistered successfully
 * \retval      VMK_NOT_FOUND  The specified client does not exist
 ***********************************************************************
 */

VMK_ReturnStatus vmk_DVFilterPropertyUnregister(
   vmk_DVFilterProperty handle);


/*
 ***********************************************************************
 * vmk_DVFilterStatInc --                                         */ /**
 *
 * \brief Increment a DVFilter client stat.
 *
 * \nativedriversdisallowed
 *
 * Updates the DVFilter's client stat of the specified type in the
 * specified direction.
 *
 * \param[in]   handle           Handle to the VMKernel filter object
 * \param[in]   direction        Direction in which packets are flowing
 * \param[in]   type             The stat to update
 * \param[in]   inc              Increment amount
 *
 * \retval      VMK_OK           Stats updated successfully
 * \retval      VMK_NOT_FOUND    Specified handle does not exist
 * \retval      VMK_BAD_PARAM    Null DVFilter handle or invalid
 *                               direction or invalid stat type
 ***********************************************************************
 */

VMK_ReturnStatus vmk_DVFilterStatInc(
   vmk_DVFilterHandle handle,
   vmk_DVFilterDirection direction,
   vmk_DVFilterStatType type,
   vmk_uint64 inc);


/*
 ***********************************************************************
 * vmk_DVFilterStatGet --                                         */ /**
 *
 * \brief Get the current value of a DVFilter client stat.
 *
 * \nativedriversdisallowed
 *
 * Fetches the DVFilter's client stat of the specified type in the
 * specified direction.
 *
 * \param[in]   handle           Handle to the VMKernel filter object
 * \param[in]   direction        Direction in which packets are flowing
 * \param[in]   type             The stat to get
 * \param[out]  stat             Value of the stat
 *
 * \retval      VMK_OK           Stats updated successfully
 * \retval      VMK_NOT_FOUND    Specified handle does not exist
 * \retval      VMK_BAD_PARAM    Null DVFilter handle or invalid
 *                               direction or invalid stat type
 ***********************************************************************
 */

VMK_ReturnStatus vmk_DVFilterStatGet(
   vmk_DVFilterHandle handle,
   vmk_DVFilterDirection direction,
   vmk_DVFilterStatType type,
   vmk_uint64 *stat);


#endif /* _VMKAPI_DVFILTER_H_ */
/** @} */
/** @} */
