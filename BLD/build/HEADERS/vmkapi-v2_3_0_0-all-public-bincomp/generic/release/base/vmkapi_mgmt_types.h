/* **********************************************************
 * Copyright 2012 - 2014 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 *******************************************************************************
 * Management Types                                                       */ /**
 * \addtogroup Mgmt
 *
 * Types used by user- and kernel-space when interacting with the vmkapi
 * management interfaces and library.
 *
 * @{
 *******************************************************************************
 */


/*
 * vmkapi_mgmt_types.h --
 *
 *	Interfaces for defining management APIs for vmkernel-based
 *	modules, which need to be shared with user-space.
 */

#ifndef _VMKAPI_MGMT_TYPES_H_
#define _VMKAPI_MGMT_TYPES_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "lib/vmkapi_name.h"
#include "lib/vmkapi_revision.h"

/**
 *  The maximum number of data payload parameters per callback,
 *  excluding the first two metadata pointers.  For user-space callbacks,
 *  the first two parameters are the cookie that was registered during
 *  initialization and the instance ID from which the callback originated,
 *  respectively.  For kernel-space callbacks, the first two parameters are
 *  a pointer to a vmk_MgmtCookies structure and pointer to a vmk_MgmtEnvelope
 *  structure, respectively.  Kernel handles support simultaneous multiple
 *  sessions from user-space handles, so session-specific identifiers and
 *  cookies help kernel callbacks track where a request is coming from or
 *  where a callback request should go to.  Each user handle represents a
 *  single session being communicated with in the kernel, therefore there is
 *  no analagous concept of multiple sessions per handle in user-space.
 */
#define VMK_MGMT_MAX_CALLBACK_PARMS 4

/**
 * \brief Callback IDs 0 through VMK_MGMT_RESERVED_CALLBACKS inclusive
 *        are reserved by the system and may not be used by consumers
 *        of the API.
 */
#define VMK_MGMT_RESERVED_CALLBACKS 15

/** \brief Opaque instance ID associated during	instance registration */
typedef	vmk_uint64 vmk_MgmtInstanceID;

#define VMK_MGMT_NO_INSTANCE_ID ((vmk_MgmtInstanceID) 0)

/**
 * \brief The type of a key used in a key-value management channel.
 */
typedef enum vmk_MgmtKeyType {
   /**
    * \brief An 8-byte integer
    */
   VMK_MGMT_KEY_TYPE_LONG = 1,
   /**
    * \brief A 4096-byte (including nul) character string.
    */
   VMK_MGMT_KEY_TYPE_STRING = 2,
} vmk_MgmtKeyType;

/**
 * \brief The maximum number of bytes in a VMK_MGMT_KEY_TYPE_STRING key.
 */
#define VMK_MGMT_KEY_STRING_MAXLEN 4096

/**
 * \brief The maximum number of keys that can be added to a
 *        vmk_MgmtHandle instance.
 */
#define VMK_MGMT_MAX_KEYS_PER_INSTANCE 32

/**
 * \brief The maximum number of instances that can be added to a management handle.
 */
#define VMK_MGMT_MAX_INSTANCES 32

/**
 * \brief The location where a callback will execute.
 */
typedef enum vmk_MgmtCallbackLocation {
   VMK_MGMT_CALLBACK_KERNEL = 1,
   VMK_MGMT_CALLBACK_USER,
} vmk_MgmtCallbackLocation;

/**
 * \brief The type of a parameter to a callback
 */
typedef enum vmk_MgmtParm {
   /**
    * \brief An input parameter to a callback.
    * \note  The contents of the parameter will be copied before invoking the
    *        callback.  Use this for fixed-length input parameters.
    */
   VMK_MGMT_PARMTYPE_IN = 0,
   /**
    * \brief An output parameter to a callback.
    * \note  Only valid for synchronous callbacks that are executed in the
    *        kernel.  The contents of the parameter will be copied back to
    *        the caller after the callback has executed.  Use this for
    *        fixed-length output parameters.
    */
   VMK_MGMT_PARMTYPE_OUT,
   /**
    * \brief An input-output parameter to a callback.
    * \note  Only valid for synchronous callbacks that are executed in the
    *        kernel.  The contents of the parameter will be copied before
    *        the callback has executed and again copied back to the caller
    *        after execution.  Use this for fixed-length input-output
    *        parameters.
    */
   VMK_MGMT_PARMTYPE_INOUT,
   /**
    * \brief An input parameter that is a variable-length vector.
    * \note  Callbacks sending or receiving this type of parameter must
    *        send or receive a "vmk_MgmtVectorParm *" that describes the
    *        data.
    */
   VMK_MGMT_PARMTYPE_VECTOR_IN,
   /**
    * \brief An output parameter that is a variable-length vector.
    * \note  Callbacks sending or receiving this type of parameter must
    *        send or receive a "vmk_MgmtVectorParm *" that describes the
    *        data.
    */
   VMK_MGMT_PARMTYPE_VECTOR_OUT,
   /**
    * \brief An input-output parameter that is a variable-length vector.
    * \note  Callbacks sending or receiving this type of parameter must
    *        send or receive a "vmk_MgmtVectorParm *" that describes the
    *        data.
    */
   VMK_MGMT_PARMTYPE_VECTOR_INOUT,
} vmk_MgmtParmType;

/**
 * \brief A description of a single callback that is part of an overall API.
 */
typedef struct vmk_MgmtCallbackInfo {
   /** \brief The location where the callback runs. */
   vmk_MgmtCallbackLocation location;
   /**
    * \brief A function pointer to the callback.
    * \note  A callback function pointer only needs to be provided if this
    *        signature is being registered where this callback would run.
    *        For example, if the API is being registered with the kernel,
    *        only VMK_MGMT_CALLBACK_KERNEL callbacks require a value here.
    *        Callback functions must not assume that they execute with any
    *        kind of serialization.  If multiple management handle instances
    *        specify the same callback function, then that callback function
    *        can be executed simultaneously by separate callers.  If your
    *        callback function requires synchronization among multiple
    *        potential callers, then your callback function must implement
    *        that synchronization.
    */
   void *callback;
   /**
     * \brief Flag that this callback is executed synchronously to the caller.
     * \note  As noted for the callback parameter, even if a callback is
     *        marked "synchronous", a callback could be invoked simultaneously
     *        by multiple callers.  "synchronous" describes the semantics as
     *        viewed by the caller, not that the callee (the callback) can
     *        assume that it is invoked atomically.
     */
   vmk_Bool synchronous;
   /**
     * \brief Number of payload parameters to the callback.
     * \note  numParms refers only to the data payload parameters to the
     *        callback and excludes the first two metadata parameters
     *        that the callback must take.  For example, a numParms value
     *        of 0 means that the callback must take two arguments -
     *        the cookies pointer and envelope pointer - but no payload parameters.
     *        The type of parameters used for metadata differ based on whether
     *        a callback is a user-space callback or a kernel-space callback.
     *        User-space callbacks take a cookie (of type vmk_uint64) and
     *        an instance ID (of type vmk_uint64) as the first two parameters,
     *        respectively.  The cookie parameter is the cookie that was
     *        provided to vmk_MgmtUserInit during initialization, and the
     *        instance ID is the unique instance in the kernel from which the
     *        callback originated.  Kernel-space callbacks take a pointer
     *        to a cookies structure (a parameter of type vmk_MgmtCookies *)
     *        and a pointer to an envelope structure (a parameter of type
     *        vmk_MgmtEnvelope *) as the first two parameters, respectively.
     *        The cookies structure contains the handle-wide cookie that was
     *        provided during vmk_MgmtInit and the session-wide cookie that
     *        was set when the sessionAnnounceFn was run, during
     *        creation of the session.  The envelope structure contains
     *        identifiers indicating which session created this callback
     *        request and which instance in the kernel this callback is being
     *        sent to.  Please refer to documentation for more on instances,
     *        sessions, and cookies.
     */
   vmk_uint8 numParms;
   /**
     * \brief The size of each callback, in the order that they are passed.
     * \note  Excludes the cookie-structure pointer and envelope pointer.
     * \note  For parameters that are of variable-length vector type,
     *        the corresponding size registered here should be of the size
     *        of one unit of the vector.
     */
   vmk_uint32 parmSizes[VMK_MGMT_MAX_CALLBACK_PARMS];
   /**
     * \brief The type of each callback, in the order that they are passed.
     * \note  Excludes the cookie-structure pointer and envelope pointer.
     */
   vmk_MgmtParmType parmTypes[VMK_MGMT_MAX_CALLBACK_PARMS];
   /**
     * \brief An identifier for this callback.
     * \note  Must be unique within the scope of call callbacks registered
     *        in an API signature.  May not be 0.
     */
   vmk_uint64 callbackId;
} vmk_MgmtCallbackInfo;

/**
 * \brief The signature for an API being initialized.
 * \note  This signature can be used either with vmk_MgmtInit or
 *        vmk_MgmtUserInit.
 */
typedef struct vmk_MgmtApiSignature {
   /**
     * \brief Version of the API.
     * \note  By default, versions of an API with the same major and minor
     *        are compatible with each other and will allow initialization
     *        and communication between the registered instances on the user
     *        or kernel side.  The minor, update, and patch numbers will
     *        be used for distinguishing minor differences that can be
     *        shimmed by providers.
     */
   vmk_revnum version;
   /**
     *  \brief Name of the API.  The version and name must be unique.
     */
   vmk_Name name;
   /**
     * \brief Vendor implementing this side (user or kernel) of the API.
     * \note  Optional.
     */
   vmk_Name vendor;
   /** \brief The number of callbacks this API has. */
   int numCallbacks;
   /** \brief The information describing each callback */
   vmk_MgmtCallbackInfo *callbacks;
} vmk_MgmtApiSignature;

/**
 * \brief A description of a vector-type parameter for callback invocation.
 * \note  A pointer to a structure of this type must be used as the data
 *        parameter to vmk_MgmtCallbackInvoke() or vmk_MgmtUserCallbackInvoke()
 *        when that data parameter corresponds to a variable-length
 *        parameter type.  That is, parameters that are of type
 *        VMK_MGMT_PARMTYPE_VECTOR_IN, VMK_MGMT_PARMTYPE_VECTOR_OUT, or
 *        VMK_MGMT_PARMTYPE_VECTOR_INOUT must be provided as
 *        vmk_MgmtVectorParm pointers.
 */
typedef struct vmk_MgmtVectorParm
{
   /**
    * \brief The total length, in bytes, of this vector.
    */
   vmk_ByteCount length;
   /**
    * \brief Pointer to the vector's array of data.
    */
   void *dataPtr;
} vmk_MgmtVectorParm;

/**
 * \brief A description of specific instances registered to a management handle.
 * \note  An application can get the instances associated with a handle and
 *        then can subsequently send callbacks to instances using the
 *        instanceIds as reported in this structure.
 */
typedef struct vmk_MgmtInstances {
   /**
    * \brief The number of instances managed on this API handle.
    */
   vmk_uint8 numInstances;

   /**
    * /brief The instance IDs that are managed.
    */
   vmk_uint64 instanceIds[VMK_MGMT_MAX_INSTANCES];

   /**
    * \brief The presentation names for each of the instances.
    */
   vmk_Name instanceNames[VMK_MGMT_MAX_INSTANCES];
} vmk_MgmtInstances;

#endif /* _VMKAPI_MGMT_TYPES_H_ */
/** @} */

