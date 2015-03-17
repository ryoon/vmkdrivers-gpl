/* **********************************************************
 * Copyright 2012 - 2013 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 *******************************************************************************
 * Management Types                                                       */ /**
 * \addtogroup Mgmt
 *
 * User-facing interfaces exported by vmkmgmt_lib, for use when communicating
 * with vmkapi modules using vmk_Mgmt APIs.
 *
 * @{
 *******************************************************************************
 */

/*
 * vmkapi_mgmt_lib.h --
 *	user-space declarations for datatypes & functions used to interact
 *      with vmkapi modules that use the vmk_Mgmt APIs.  Note that user-space
 *      management APIs and user-specific types are prefixed with "vmk_MgmtUser",
 *      even when they may be used to invoke actions in the kernel.
 */

#ifndef _VMKAPI_MGMT_LIB_H_
#define _VMKAPI_MGMT_LIB_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "base/vmkapi_mgmt_types.h"

/** \brief An opaque handle that is allocated and managed by the library */
typedef struct vmkMgmtUserHandleInt * vmk_MgmtUserHandle;

/**
 ***********************************************************************
 * vmk_MgmtUserInit                                               */ /**
 *
 * \brief Initialize user-space management of a kernel module.
 *
 * \note This initializes your application for interaction with a
 *       kernel module that has an API signature matching the one you
 *       provide here with vmk_MgmtUserInit.  Note that the kernel side
 *       of the API must be successfully registered with
 *       vmk_MgmtInit prior to attempting to initialize the
 *       user-space side.  There can be multiple applications or
 *       threads using vmk_MgmtUserInit for the same API signature,
 *       but only one kernel module can service that API signature.
 *
 * \note The signature provided here must be compatible with the
 *       signature of the API that has been registered with the kernel.
 *       if "vendor" is specified (non-null) in the signature, a
 *       connection will only be made if the vendor matches.
 *
 * \param[in]     sig         The API signature describing the
 *                            management operations that can be done.
 *                            Must match a signature registered in the
 *                            kernel.
 * \param[in]     userCookie  A data cookie that will be provided as
 *                            the first argument to all user-space
 *                            callbacks.
 * \param[out]    handle      An opaque handle that will be used for
 *                            further interaction with the API.
 *
 * \retval 0      Initialization succeeded.
 * \retval other  A UNIX-style error code.
 *
 ***********************************************************************
 */
int
vmk_MgmtUserInit(vmk_MgmtApiSignature *sig,
                 vmk_uint64 userCookie,
                 vmk_MgmtUserHandle *handle);

/**
 ***********************************************************************
 * vmk_MgmtUserGetInstances                                       */ /**
 *
 * \brief Get instance information for all kernel-registered instances.
 *
 * \note  The instanceIds populated in the vmk_MgmtInstances structure
 *        can be subsequently used to send a callback request to a
 *        specific instance with vmk_MgmtUserCallbackInvoke
 *
 * \param[in]  handle     Management handle that was initialized
 * \param[out] instances  Current instance information for given handle
 *
 * \retval 0      Succeeded getting instances.
 * \retval other  A UNIX-style error code.
 *
 ***********************************************************************
 */
int
vmk_MgmtUserGetInstances(vmk_MgmtUserHandle handle,
                         vmk_MgmtInstances *instances);

/**
 ***********************************************************************
 * vmk_MgmtUserBegin                                              */ /**
 *
 * \brief Begin execution of user callbacks.
 *
 * \note  This call creates a separate thread to monitor incoming
 *        callback requests from the kernel for the given management
 *        handle and immediately begins execution of those callbacks
 *        as they arrive.
 * \note  This is not necessary if your application is not meant to
 *        handle callbacks delivered from the kernel.
 *
 *
 * param[in] handle   Management handle that was initialized
 *
 * \retval 0     Succeeded initializing callback execution
 * \retval other A UNIX-style error code
 *
 ***********************************************************************
 */
int
vmk_MgmtUserBegin(vmk_MgmtUserHandle handle);

/**
 ***********************************************************************
 * vmk_MgmtUserContinue                                           */ /**
 *
 * \brief Indefinitely continue monitoring and executing incoming kernel
 *        callback requests in user space.
 *
 * \note  vmk_MgmtUserBegin must have been successfully executed prior
 *        to calling vmk_MgmtUserContinue.
 * \note  Invoking vmk_MgmtUserContinue effectively makes the current
 *        (calling) thread run indefinitely, in a monitoring mode.  This
 *        is typically used by resident CIM providers.  To terminate
 *        monitoring on a particular management handle after
 *        vmk_MgmtUserBegin or vmk_MgmtUserContinue has been executed,
 *        vmk_MgmtUserEnd.  vmk_MgmtUserContinue does not return until
 *        vmk_MgmtUserEnd is separately executed on the same handle
 *        from within the same process.  To use vmk_MgmtUserEnd after
 *        vmk_MgmtUserContinue, you must create a separate pthread that
 *        will invoke vmk_MgmtUserEnd sometime later after your main
 *        thread has invoked vmk_MgmtUserContinue.
 *
 * \param[in] handle   Management handle that was initialized
 *
 * \retval 0           Execution succeeded and was ended without error.
 *                     This will happen at some future moment in time,
 *                     after vmk_MgmtUserEnd has been used.
 * \retval Other       A UNIX error code.  Execution terminated,
 *                     possibly early with error.
 *
 ***********************************************************************
 */
int
vmk_MgmtUserContinue(vmk_MgmtUserHandle handle);

/**
 ***********************************************************************
 * vmk_MgmtUserEnd                                                */ /**
 *
 * \brief Terminate monitoring and execution of kernel-to-user callbacks.
 *
 * \note  This may be called after either vmk_MgmtUserBegin or
 *        vmk_MgmtUserContinue.
 * \note  Because vmk_MgmtUserContinue does not return, a separate
 *        thread within the same process space is required to invoked
 *        vmk_MgmtUserEnd.  The effect of vmk_MgmtUserEnd will be to
 *        cease processing of any additional incoming callback requests
 *        from the kernel, but the management handle will remain open.
 *        Processing of inbound callback requests from the kernel can
 *        be resumed if vmk_MgmtUserBegin is subsequently used on the
 *        given handle.
 *
 * \param[in] handle   Management handle that was initialized.
 *
 * \retval 0           Execution was successfully ended.
 * \retval Other       A UNIX-style error code indicating the nature of
 *                     an error that was encountered.
 *
 ***********************************************************************
 */
int
vmk_MgmtUserEnd(vmk_MgmtUserHandle handle);

/**
 ***********************************************************************
 * vmk_MgmtUserDestroy                                            */ /**
 *
 * \brief Destroy the user side connection to a management handle.
 *
 * \note  This will terminate receiving of callback requests from the
 *        kernel and terminate the ability to send new callback
 *        requests to the kernel.
 *
 * \param[in] handle   Management handle that was initialized.
 *
 * \retval 0           Handle was destroyed completely without error.
 * \retval Other       A UNIX-style error code indicating the nature
 *                     of the failure.  The handle should not be used
 *                     again even after a failure, however.
 *
 ***********************************************************************
 */
int
vmk_MgmtUserDestroy(vmk_MgmtUserHandle handle);

/**
 ***********************************************************************
 * vmk_MgmtUserCallbackInvoke                                     */ /**
 *
 * \brief From user space, invoke a callback inside the kernel.
 *
 * \note  Parameter handling:  For asynchronous callbacks, all
 *        parameters are internally copied for delivery to the kernel.
 *        For synchronous callbacks, input and input-output parameters
 *        are copied internally and delivered to the kernel.  For
 *        input-output and output parameters, such parameters are copied
 *        from the kernel back to user-space (to the pointer indicated
 *        by the corresponding parameter given to this function) after
 *        the callback has executed.  In addition to the payload
 *        parameters passed here, the receiving callback in the kernel
 *        will be provided the kernel-side data cookie that the
 *        receiver registered with vmk_MgmtInit and the instanceId
 *        specificed here.  If an instanceId is specified, the callback
 *        function invoked in the kernel will be the instance-specific
 *        callback (if it was specified when the instance was added) or
 *        the default callback function registered with the API
 *        signature (if no instance-specific callback was specified
 *        for this callback when the instance was added).
 *
 * \param[in] handle      Management handle that was initialized.
 * \param[in] instanceId  The unique instance ID to send this callback
 *                        to.  If this is not an instance-specific
 *                        invocation, use VMK_MGMT_NO_INSTANCE_ID.
 * \param[in] callbackId  The unique ID corresponding to the callback
 *                        to invoke, as registered with the API
 *                        signature.
 * \param[in] ...         A variable list of parameters, each one
 *                        a pointer to the parameter being passed 
 *                        to the callback.  For parameters that are
 *                        fixed-length types parameters, the pointer
 *                        should just be a pointer to the data.
 *                        For parameters that are variable-length
 *                        vectors, the corresponding pointers should
 *                        be to vmk_MgmtVectorParm structures that
 *                        describe the corresponding data.  The number
 *                        of parameters must match that described for
 *                        the given callbackId when the API signature
 *                        was registered.
 * \retval 0     For asynchronous callbacks, this means that the callback
 *               successfully was queued.  For synchronous callbacks,
 *               this means that the callback has executed completely.
 * \retval Other A UNIX-style error code corresponding to the error
 *               encountered when attempting to queue or execute the
 *               callback.
 * \note  The return value is not a indication or return code of the
 *        callback itself.
 *
 */
#define vmk_MgmtUserCallbackInvoke(     \
   /* (vmk_MgmtHandle *) */ handle,     \
   /* (vmk_uint64)       */ instanceId, \
   /* (vmk_uint64)       */ callbackId, \
                            ...)        \
   vmk_MgmtUserCallbackInvokeInt(handle, instanceId, callbackId, VMK_UTIL_NUM_ARGS(__VA_ARGS__), ##__VA_ARGS__)

/*
 ***********************************************************************
 * vmk_MgmtUserCallbackInvokeInt --
 *
 * This is used by vmk_MgmtUserCallbackInvoke().  VMKAPI clients should
 * never call this function directly.
 * 
 ***********************************************************************
 */
int 
vmk_MgmtUserCallbackInvokeInt(vmk_MgmtUserHandle handle,
                              vmk_uint64 instanceId,
                              vmk_uint64 callbackId,
                              vmk_uint32 argCount,
                              ...);

#endif /* _VMKAPI_MGMT_LIB_H_ */
/** @} */
