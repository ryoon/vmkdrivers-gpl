/* **********************************************************
 * Copyright 2012 - 2013 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Management Interfaces                                          */ /**
 * \defgroup Mgmt Management
 *
 * Interfaces that allow management of vmkapi modules (runtime
 * parameterization, notifications to modules from user space and
 * to user space from modules).  
 *
 * @{
 ***********************************************************************
 */

/*
 * vmkapi_mgmt.h --
 *
 * 	vmkernel declarations for datatypes & functions used for
 *	enabling per-module management APIs between user-space and
 *	vmkernel modules.
 */



#ifndef _VMKAPI_MGMT_H_
#define _VMKAPI_MGMT_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "base/vmkapi_mgmt_types.h"

/** \brief Opaque generic handle allocated by the API */
typedef struct vmkMgmtHandleInt * vmk_MgmtHandle;

/** \brief Opaque session ID assigned during session announcement */
typedef vmk_uint64 vmk_MgmtSessionID;

/**
 * \brief The session ID to send a callback to all current sessions.
 * \note  Using this session ID in the envelope's sessionId member
 *        will send a callback request, via vmk_MgmtCallbackInvoke,
 *        to all current sessions.
 */
#define VMK_MGMT_SESSION_ID_ALL ((vmk_uint64) 0)

/**
 * \brief Addressing information describing a callback invocation.
 * \note  An envelope structure is used both when receiving a callback
 *        (as the second parameter to the kernel callback) and
 *        when sending a callback to user-space from via
 *        vmk_MgmtCallbackInvoke.  When receiving a callback in
 *        the kernel, the envelope describes the session from
 *        which the callback originates and the instance to
 *        which the callback is addressed (if any).  When sending
 *        a callback to user-space, the envelope describes the
 *        session to which the callback should be sent and the
 *        instance from which the callback originates (if any).
 */
typedef struct vmk_MgmtEnvelope {
   /**
    * \brief Instance ID this callback is being sent to or coming from.
    * \note  VMK_MGMT_NO_INSTANCE_ID can be provided if the callback is
    *        not originating from any specific instance or is not
    *        addressed to a specific instance.
    */
   vmk_MgmtInstanceID instanceId;

   /**
    * \brief Session ID this callback is being sent to or coming from.
    * \note  VMK_MGMT_SESSION_ID_ALL can be used to address all
    *        sessions simultaneously from the kernel, but
    *        VMK_MGMT_SESSION_ID_ALL will never be provided as
    *        the session from which a callback originated.
    */
   vmk_MgmtSessionID sessionId;
} vmk_MgmtEnvelope;

/**
 * \brief Cookie information provided to a single callback invocation.
 */
typedef struct vmk_MgmtCookies {
   /**
    * \brief Handle cookie that was provided to vmk_MgmtInit
    */
   vmk_uint64 handleCookie;

   /**
    * \brief Session cookie assigned by session announcement function.
    */
   vmk_uint64 sessionCookie;
} vmk_MgmtCookies;

/**
 ***********************************************************************
 * vmk_MgmtSessionAnnounceFn --                                   */ /**
 *
 * \brief Session-announce function prototype
 *
 * This function runs when a new managing session (usually a user-space
 * program such as a CIM provider) connects to a management handle.
 * You can use this function to initialize per-session state.  To
 * refer to that session state later, assign the state metadata (such
 * as a pointer) to the sessionCookie output parameter.  The
 * sessionCookie will be provided to subsequent callbacks originating
 * from this particular session and to the session cleanup function
 * when the session ends.
 *
 * \param[in]  handle         The handle associated with the session
 *                            being created.
 * \param[in]  handleCookie   The cookie that was provided to
 *                            vmk_MgmtInit.
 * \param[in]  sessionId      A unique identifier for the new session.
 * \param[out] sessionCookie  Optional developer-provided cookie that
 *                            will be provided on subsequent
 *                            callback invocations and to the
 *                            session cleanup function.
 *
 * \retval VMK_OK The session could be established.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus
(*vmk_MgmtSessionAnnounceFn)(vmk_MgmtHandle handle,
                             vmk_uint64 handleCookie,
                             vmk_MgmtSessionID sessionId,
                             vmk_uint64 *sessionCookie);

/**
 ***********************************************************************
 * vmk_MgmtSessionCleanupFn --                                    */ /**
 *
 * \brief Session-cleanup function prototype.
 *
 * This function runs when a managing session (usually a user-space
 * program such as a CIM provider) disconnects from the management
 * handle.  Note that if your cleanup function encounters an
 * internal error that prevents it from successfully completing,
 * the management session will still be removed and will not be
 * used again.
 *
 * \note You must not use vmk_MgmtCallbackInvoke() from within
 *       your session-cleanup function to the session that
 *       is being cleaned up.
 *
 * \param[in]  handle         The handle associated with the session
 *                            being cleaned up.
 * \param[in]  handleCookie   The cookie that was provided to
 *                            vmk_MgmtInit.
 * \param[in]  sessionId      A unique identifier for session.
 * \param[in]  sessionCookie  The cookie that was established by the
 *                            announce function when the session
 *                            started.
 *
 * \retval None.
 *
 ***********************************************************************
 */
typedef void
(*vmk_MgmtSessionCleanupFn)(vmk_MgmtHandle handle,
                            vmk_uint64 handleCookie,
                            vmk_MgmtSessionID sessionId,
                            vmk_AddrCookie sessionCookie);


/**
 ***********************************************************************
 * vmk_MgmtCleanupFn --                                           */ /**
 *
 * \brief Prototype for a management interface's cleanup callback.
 *
 * \param[in]  private  Optional cookie data to be used by the callback,
 *                      as was originally provided to vmk_MgmtInit().
 *
 ***********************************************************************
 */
typedef void (*vmk_MgmtCleanupFn)(vmk_uint64 handleCookie);

/**
 ***********************************************************************
 * vmk_MgmtKeyGetFn --                                            */ /**
 *
 * \brief Prototype for get-key function.
 *
 * \note  This prototype is for a module-supplied "get" function
 *        for fetching a key's value, for a key that was registered
 *        using vmk_MgmtAddKey.
 *
 * \param[in]    cookie  Cookie supplied with vmk_MgmtInit.
 * \param[out]   keyVal  Value of the key that was read.  The type of
 *                       pointer this represents depends on the type
 *                       of key that was added using this function.
 *
 * \retval VMK_OK The 'get' function executed correctly.
 *                This is not an indicator of the success or failure of
 *                the operations in the function, but merely that they
 *                ran.  
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_MgmtKeyGetFn)(vmk_uint64 cookie,
                                             void *keyVal);

/**
 ***********************************************************************
 * vmk_MgmtKeySetFn --                                            */ /**
 *
 * \brief Prototype for set-key function.
 *
 * \note  This prototype is for a module-supplied "set" function
 *        for storing a key's value, for a key that was registered
 *        using vmk_MgmtAddKey.
 *
 * \param[in]    cookie  Cookie supplied with vmk_MgmtKeyValueInit.
 * \param[in]    keyVal  Value of the key to set.  The type of
 *     	       	       	 pointer this represents depends on the	type
 *     	       	       	 of key	that was added using this function.
 *
 * \retval VMK_OK The 'set' function executed correctly.
 *                This is not an indicator of the success or failure of
 *                the operations in the function, but merely that they
 *                ran.  
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_MgmtKeySetFn)(vmk_uint64 cookie,
                                             void *keyVal);

/**
 * \brief Properties of a kernel management handle being initialized.
 */
typedef struct vmk_MgmtProps {
   /**
    * \brief Module ID registering this handle.
    */
   vmk_ModuleID modId;

   /**
    * \brief Heap ID used for allocating metadata and parameter data.
    * \note The heap must have sufficient capacity to handle static
    *       allocations associated with metadata overhead and to handle
    *       dynamic allocations associated with backing parameter
    *       data during callback invocations and key-value operations.
    *       The static overhead is approximately 16KB per management
    *       handle plus approximately 2KB per simultaneous
    *       application session (e.g., CIM provider or other
    *       caller of vmk_MgmtUserInit).  The dynamic overhead per
    *       callback invocation is a maximum of 9KB plus the total of
    *       all parameter sizes for that specific callback.  If multiple
    *       concurrent callbacks (or concurrent callbacks of
    *       different type with different parameter sizes) should
    *       be supported, then the heap should be large enough to
    *       handle the simultaneous allocation of those dynamic
    *       allocations while those callbacks are being processed.
    *       Similarly, dynamic overhead for a key-value operation is
    *       approximately 9KB per operation while that operation is
    *       ongoing.  Typically key-value operations are processed
    *       serially from user-space, and only one operation is
    *       ongoing at any given.
    */
   vmk_HeapID heapId;

   /**
    * \brief Signature describing the API being initialized.
    * \note  To be useable, API passed must have an equivalent
    *        signature that is passed to the library interface
    *        in user-space.
    */
   vmk_MgmtApiSignature *sig;

   /**
    * \brief Cleanup function run during management handle teardown.
    * \note  This is optional and may be NULL.
    */
   vmk_MgmtCleanupFn cleanupFn;

   /**
    * \brief Function that runs when a management session starts.
    * \note  This is optional and may be NULL.
    */
   vmk_MgmtSessionAnnounceFn sessionAnnounceFn;

   /**
    * \brief Function that runs when a management session ends.
    * \note  This is optional and may be NULL.
    */
   vmk_MgmtSessionCleanupFn sessionCleanupFn;

   /**
    * \brief Handle-wide cookie that used by subsequent callbacks.
    * \note  This is optional and may be 0.
    */
   vmk_uint64 handleCookie;
} vmk_MgmtProps;


/**
 ***********************************************************************
 * vmk_MgmtInit                                                   */ /**
 *
 * \brief Initialize the kernel side of a user/kernel management API
 *
 * \param[in]     props     The properties describing the API being
 *                          initialized.
 * \param[out]    handle    The handle that will be allocated for
 *                          accessing this API.
 *
 * \retval VMK_OK         Initialization succeeded.
 * \retval VMK_BAD_PARAM  Either the modId or signature were invalid.
 * \retval VMK_NO_MEMORY  Internal metadata for operation could not be 
 *                        allocated.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_MgmtInit(vmk_MgmtProps *props,
             vmk_MgmtHandle *handle);

/**
 ***********************************************************************
 * vmk_MgmtRegisterInstanceCallbacks                              */ /**
 *
 * \brief Register instance-specific management callbacks.
 *
 * \note  This API registers an instance and instance-specific callbacks
 *        that will be associated with a given management handle.  If
 *        you provide instance-specific callbacks, those callbacks will
 *        be invoked instead of the default corresponding callbacks that
 *        were originally registered with the handle.  Note that it is
 *        valid to supply a subset of instance-specific callbacks
 *        (or even none).
 *
 * \param[in]   handle        The management handle that was initialized.
 * \param[in]   instanceId    The unique instance that will have its
 *                            callbacks registered.  Must be unique for
 *                            the current handle, and must not be 0.
 * \param[in]   modId         The modId of the module where the
 *                            callbacks reside.
 * \param[in]   heapId        The heapId from the module where the
 *                            callbacks reside.
 * \param[in]   displayName   The name that will be displayed for
 *                            this instance when it's listed.
 * \param[in]   numCallbacks  The number of instance-specific
 *                            callbacks that are being registered.  0
 *                            is valid, if the instance does not
 *                            supply instance-specific callbacks.
 * \param[in]   callbacks     The callback information for each
 *                            instance-specific callback, corresponding
 *                            to callbacks that override those
 *                            registered in the API signature
 *                            for this handle.
 *
 * \retval VMK_OK         Initialization succeeded.
 * \retval VMK_BAD_PARAM  Parameters couldn't be validated.
 * \retval VMK_NO_MEMORY  Internal metadata for operation could not be
 *                        allocated.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_MgmtRegisterInstanceCallbacks(vmk_MgmtHandle handle,
                                  vmk_uint64 instanceId,
                                  vmk_ModuleID modId,
                                  vmk_HeapID heapId,
                                  vmk_Name *displayName,
                                  vmk_uint32 numCallbacks,
                                  vmk_MgmtCallbackInfo *callbacks);

/**
 ***********************************************************************
 * vmk_MgmtUnregisterInstanceCallbacks                            */ /**
 *
 * \brief Unregister an instance from being management handle.
 *
 * \param[in]  handle      The management handle that was initialized
 *                         and to which this instance is associated.
 * \param[in]  instanceId  The unique instance that was already
 *                         registered for management.
 *
 * \retval VMK_OK          Unregistration succeeded.
 * \retval VMK_BAD_PARAM   The instance was not already registered.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_MgmtUnregisterInstanceCallbacks(vmk_MgmtHandle handle,
                                    vmk_uint64 instanceId);


/**
 ***********************************************************************
 * vmk_MgmtDestroy                                                */ /**
 * 
 * \brief Destroy the kernel side of a user/kernel management API
 *
 * \param[in]  handle   The handle that was passed and initialized with 
 *                      vmk_MgmtInit
 *
 * \note The heap that was passed to vmk_MgmtInit should not be
 *       destroyed until all access to the management channel has
 *       stopped, and thus the cleanup function has run.  If you call
 *       vmk_MgmtDestroy during module-unload, you are assured that the
 *       management channel is not in use & thus you can safely destroy
 *       the heap immediately.
 *
 * \retval VMK_BAD_PARAM   The API has already been destroyed or has
 *                         already been requested to be destroyed.
 * \retval VMK_OK          The API will be destroyed once all in-flight
 *                         operations conclude (may be immediate, if
 *                         none are currently in-flight).
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_MgmtDestroy(vmk_MgmtHandle handle);

/*
 ***********************************************************************
 * vmk_MgmtCallbackInvokeInt --
 *
 * This is used by vmk_MgmtCallbackInvoke().  VMKAPI clients should
 * never call this function directly.
 *
 ***********************************************************************
 */

/** \cond nodoc */
VMK_ReturnStatus
vmk_MgmtCallbackInvokeInt(vmk_MgmtHandle handle,
                          vmk_MgmtEnvelope *envelope,
                          vmk_uint64 callbackId,
                          vmk_uint32 argCount,
                          ...);
/** \endcond */


/**
 ***********************************************************************
 * vmk_MgmtCallbackInvoke                                         */ /**
 *
 * \brief Invoke a user-space callback that has zero or more parameters. 
 * \note The callback will be asynchronously delivered to user-space,
 *       but only if there currently is a user-space process associated
 *       with this management handle that is listening for callback
 *       requests.  This call does not block.
 * \note Parameters are immediately copied for delivery.  Sizes are 
 *       determined by the API signature that was registered with 
 *       vmk_MgmtInit.  Additionally, the data cookie that was 
 *       registered with the receiver (in user space) will be provided
 *       as the first argument to the callback that is delivered in
 *       user space.
 *
 * \param[in] handle     The handle that was passed and initialized with 
 *                       vmk_MgmtInit
 * \param[in] envelope   The envelope describing the instanceId from
 *                       which this callback originates and the
 *                       session ID to which this callback should be
 *                       sent.  If this callback should be sent to
 *                       all current sessions, use VMK_MGMT_SESSION_ID_ALL
 *                       as the session ID.  If this callback is not
 *                       instance-specific, use VMK_MGMT_NO_INSTANCE_ID
 *                       as the instance ID.
 *                       specific invocation, use VMK_MGMT_NO_INSTANCE_ID.
 * \param[in] callbackId The unique ID corresponding to the callback to
 *                       invoke, as registered with the API signature.
 * \param[in] ...        Pointers to the parameters to copy and pass.
 *                       The number of parameters passed here must match
 *                       the number used by the callback indicated by
 *                       callbackId.  Any parameter that is declared
 *    	       	       	 as a vector type must be encapsulated in a
 *    	     	       	 vmk_MgmtVectorParm structure.
 *
 * \retval VMK_OK         The callback was prepared for delivery.  This
                          does not indicate that it has run, however.
 * \retval VMK_BAD_PARAM  The callback or number of parameters supplied
 *                        was invalid.
 * \retval VMK_NO_MEMORY  Temporary storage required to deliver the event
 *                        was unavailable.
 * \retval VMK_NOT_FOUND  There is no listening user-space process
 *                        running that can receive this callback request.
 *
 ***********************************************************************
 */

#define vmk_MgmtCallbackInvoke(                 \
         /* (vmk_MgmtHandle)     */ handle,     \
         /* (vmk_MgmtEnvelope *) */ envelope,   \
         /* (vmk_uint64)         */ callbackId, \
                                    ...)        \
   vmk_MgmtCallbackInvokeInt(handle, envelope, callbackId, VMK_UTIL_NUM_ARGS(__VA_ARGS__), ##__VA_ARGS__)

/**
 ***********************************************************************
 * vmk_MgmtAddKey                                                 */ /**
 *
 * \brief Add a key to be managed as a key-value pair.
 *
 * \note  This creates a key-value pair that can be managed using
 *        the vmkmgmt_keyval utility.  The name of the management
 *        handle that was initialized is the name of the key-value
 *        instance that would be the "instance" argument to
 *        vmkmgmt_keyval.  For the get and set functions registered
 *        here, the cookie that is given back is the cookie that
 *        was initialized with vmk_MgmtInit.
 *
 * \param[in]     handle    The handle that was initialized by
 *                          vmk_MgmtInit.
 * \param[in]     keyType   The type of the key being added.
 * \param[in]     keyName   The name of the key being added.  Must be
 *                          unique compared to other registered
 *                          keys for this management handle.
 * \param[in]     getFn     The function that will be used to get the key
 *                          value at runtime.
 * \param[in]     setFn     The function that will be used to set the key
 *                          value at runtime.
 *
 * \note Both the getFn and setFn must be provided.
 *
 * \retval VMK_OK         The key was added.
 * \retval VMK_BAD_PARAM  A bad parameter was given.
 * \retval VMK_NO_MEMORY  Memory was not available to allocate the required
 *                        metadata structures.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_MgmtAddKey(vmk_MgmtHandle handle,
               vmk_MgmtKeyType keyType,
               vmk_Name *keyName,
               vmk_MgmtKeyGetFn getFn,
               vmk_MgmtKeySetFn setFn);


#endif /* _VMKAPI_MGMT_H_ */
/** @} */
