/* **********************************************************
 * Copyright 2008 - 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * HelperWorlds                                                   */ /**
 * \defgroup HelperWorlds Helper Worlds
 *
 * Helper Worlds are a mechanism that allows work to be queued and
 * run by a dynamically managed pool of worlds.
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_HELPER_WORLDS_H_
#define _VMKAPI_HELPER_WORLDS_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/** Invalid helper ID */
#define VMK_HELPER_INVALID_ID          ((vmk_Helper)-1)

/** Use default request properties */
#define VMK_HELPER_DEFAULT_REQ_PROPS   ((const vmk_HelperRequestProps *)NULL)

/* Opaque helper definition */
typedef vmk_uintptr_t vmk_Helper;

/*
 ***********************************************************************
 * vmk_HelperTagComparator --                                     */ /**
 *
 * \ingroup HelperWorlds
 * \brief Compare two request tags to see if they are equal
 *
 * This function is used to find a request or set of requests based
 * on their tag, such as during cancellation.
 *
 * \note Callbacks of this type may not block.
 *
 * \param[in] tag1   First tag to compare.
 * \param[in] tag2   Second tag to compare.
 *
 * \retval VMK_TRUE     The tags match.
 * \retval VMK_FALSE    The tags do not match.
 *
 ***********************************************************************
 */
typedef vmk_Bool (*vmk_HelperTagComparator)(vmk_AddrCookie tag1,
                                            vmk_AddrCookie tag2);

/*
 ***********************************************************************
 * vmk_HelperConstructor --                                       */ /**
 *
 * \ingroup HelperWorlds
 * \brief Function invoked when a new helper world is spawned.
 *
 * \note Callbacks of this type may not block.
 *
 * \param[in] arg   Constructor argument that was stored in the helper
 *                  queue's properties.
 *
 ***********************************************************************
 */
typedef void (*vmk_HelperConstructor)(vmk_AddrCookie arg);

/*
 ***********************************************************************
 * vmk_HelperCancelFunc --                                       */ /**
 *
 * \ingroup HelperWorlds
 * \brief Function invoked when a request is canceled.
 *
 * \note Callbacks of this type may not block.
 *
 * \param[in] data   Data that was passed in with the request.
 *
 ***********************************************************************
 */
typedef void (*vmk_HelperCancelFunc)(vmk_AddrCookie data);

/*
 ***********************************************************************
 * vmk_HelperRequestFunc --                                       */ /**
 *
 * \ingroup HelperWorlds
 * \brief Function invoked when a request is processed
 *
 * \note Callbacks of this type may not block.
 *
 * \param[in] data   Data that was passed in with the request.
 *
 ***********************************************************************
 */
typedef void (*vmk_HelperRequestFunc)(vmk_AddrCookie data);

/**
 * \brief Properties for a helper queue that may be updated
 *        after it is created.
 */
typedef struct vmk_HelperMutableProps
{
   /**
    * Minimum number of worlds that will continue to exist to serve
    * the helper queue.
    *
    * Setting this to a value less than the value of maxWorlds indicates
    * that the helper queue can dynamically create and destroy worlds for
    * the helper queue but will always maintain at least the specified
    * number of worlds in the queue.
    *
    * Setting this to -1 or the same value as maxWorlds indicates that
    * the caller does not want helper worlds to exit and does not want
    * the pool of worlds to shrink dynamically.
    */
   vmk_int32 minWorlds;

   /**
    * Maximum number of worlds that will service the helper queue.
    *
    * Setting this to 0 or less indicates that the helper queue should
    * use the default number of worlds to service the helper queue.
    *
    * Setting this to 1 or more indicates that the helper queue should
    * use the specified number of worlds as the maximum number of worlds
    * that can back the queue.
    */
   vmk_int32 maxWorlds;

   /**
    * Maximum number of milliseconds a world backing the helper
    * queue can be idle before it is retired.
    *
    * Setting this to 0 or less indicates that the default idle time
    * should be used.
    *
    * Setting this to 1 or more indiciates that if a world in the pool
    * backing the helper queue has been idle for the specified number
    * of milliseconds it will be retired.
    */
   vmk_int32 maxIdleTime;

   /**
    * Maximum number of milliseconds a request can wait in the
    * queue before a new world will be created to service the request.
    *
    * Setting this to 0 or less indiciates that the helper queue should
    * use the default time.
    *
    * Setting this to 1 or more indicates that after the specified number
    * of milliseconds a new world will be added to the pool of worlds
    * backing the helper queue.
    *
    * \note The specified maximum number of worlds will not be
    *       exceeded even if a request has been waiting longer
    *       than the specified time.
    */
   vmk_int32 maxRequestBlockTime;
}
vmk_HelperMutableProps;

/**
 * \brief Properties for a new helper queue
 */
typedef struct
{
   /** Human readable name for the helper queue */
   vmk_Name name;

   /** A heap the helper can use for allocations */
   vmk_HeapID heap;

   /**
    * Should an IRQ spinlock should be used when synchronizing
    * between producers and consumers of the helper queue?
    */
   vmk_Bool useIrqSpinlock;

   /**
    * Should all memory for requests be preallocated when
    * the helper queue is created?
    */
   vmk_Bool preallocRequests;

   /**
    * Can callers block when submitting requests?
    * Callers can override this setting on a per-call basis.
    */
   vmk_Bool blockingSubmit;

   /**
    * Maximum number of requests this queue can support
    */
   vmk_uint32 maxRequests;

   /**
    * Initial setting for properties which may be updated after
    * the helper queue is created.
    */
   vmk_HelperMutableProps mutables;

   /**
    * Function to compare two tags that identify requests
    *
    * Can be set to NULL if no function is desired.
    */
   vmk_HelperTagComparator tagCompare;

   /**
    * Function to call when a new world is added to the
    * pool of worlds backing the helper queue.
    *
    * This function will run in the context of the
    * new world.
    *
    * This function will be called before running
    * any helper requests on the new world.
    *
    * This can be set to NULL if no function is desired.
    */
   vmk_HelperConstructor  constructor;

   /** Data to pass to the constructor function */
   vmk_AddrCookie constructorArg;
}
vmk_HelperProps;

/**
 * \brief Properties for a helper request
 */
typedef struct vmk_HelperRequestProps {
   /** Should the submission of this request block the caller? */
   vmk_Bool requestMayBlock;

   /**
    * A caller-supplied tag to identify the request.
    *
    * This tag is used to identify the request for calls
    * such as vmk_HelperCancelRequest().
    */
   vmk_AddrCookie tag;

   /**
    * An optional function to call after a request is canceled
    * to perform cleanup.
    *
    * May be set to NULL if no function is required.
    *
    * This field cannot be set if the tag field is set to zero.
    */
   vmk_HelperCancelFunc cancelFunc;

   /**
    * World to which to bill helper work or VMK_INVALID_WORLD_ID
    * if there is no world to bill.
    */
   vmk_WorldID worldToBill;
} vmk_HelperRequestProps;

/*
 ***********************************************************************
 * vmk_HelperCreate --                                            */ /**
 *
 * \ingroup HelperWorlds
 * \brief Create a new helper world queue.
 *
 * \note This function may block.
 *
 * \param[in]  props    Creation properties for the new helper queue.
 * \param[out] helper   The newly created helper queue.
 *
 *
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_HelperCreate(vmk_HelperProps *props,
                                  vmk_Helper *helper);

/*
 ***********************************************************************
 * vmk_HelperDestroy --                                           */ /**
 *
 * \ingroup HelperWorlds
 * \brief Destroy an existing helper world queue.
 *
 * \note This function may block.
 *
 * \param[in] helper    Helper queue to destroy.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_HelperDestroy(vmk_Helper helper);

/*
 ***********************************************************************
 * vmk_HelperUpdateProps --                                       */ /**
 *
 * \ingroup HelperWorlds
 * \brief Update the mutable properties on an existing helper queue.
 *
 * \note This function will not block.
 *
 * \param[in] helper    Helper queue to destroy.
 * \param[in] props     New properties for the helper queue.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_HelperUpdateProps(vmk_Helper helper,
                                       const vmk_HelperMutableProps *props);

/*
 ***********************************************************************
 * vmk_HelperRequestPropsInit --                                  */ /**
 *
 * \ingroup HelperWorlds
 * \brief Initialize a vmk_HelperRequestProps structure.
 *
 * The resulting structure will have all properties set to their default.
 *
 * \note This function will not block.
 *
 * \param[in] props   Pointer to the parameters to initialize.
 *
 * \retval VMK_INVALID_NAME   Name for the helper queue is too long
 *
 ***********************************************************************
 */
void vmk_HelperRequestPropsInit(vmk_HelperRequestProps *props);

/*
 ***********************************************************************
 * vmk_HelperSubmitRequest --                                     */ /**
 *
 * \ingroup HelperWorlds
 * \brief Submit a request to a helper world queue to be executed later
 *
 * \note This function may block if the queue has been created with
 *       blocking properties.
 *
 * \param[in] helper       Helper queue to send the request to.
 * \param[in] requestFunc  Function to execute
 * \param[in] data         Data to pass to the function that will be
 *                         executed.
 * \param[in] props        Properties of this request submission;
 *                         use vmk_HelperRequestPropsInit if the default
 *                         parameters are desired.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_HelperSubmitRequest(vmk_Helper helper,
                                         vmk_HelperRequestFunc requestFunc,
                                         vmk_AddrCookie data,
                                         const vmk_HelperRequestProps *props);

/*
 ***********************************************************************
 * vmk_HelperSubmitDelayedRequest --                              */ /**
 *
 * \ingroup HelperWorlds
 * \brief Submit a request to a helper world queue to be executed only
 *        after a specified amount of time.
 *
 * \note This function may block if the queue has been created with
 *       blocking properties.
 *
 * \param[in] helper       Helper queue to send the request to.
 * \param[in] requestFunc  Function to execute
 * \param[in] data         Data to pass to the function that will be
 *                         executed.
 * \param[in] props        Properties of this request submission;
 *                         use vmk_HelperRequestPropsInit if the default
 *                         parameters are desired.
 * \param[in] timeoutMS    Timeout in millseconds after which the function
 *                         would be executed by the helper.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_HelperSubmitDelayedRequest(vmk_Helper helper,
                                                vmk_HelperRequestFunc requestFunc,
                                                vmk_AddrCookie data,
                                                vmk_uint32 timeoutMS,
                                                const vmk_HelperRequestProps *props);

/*
 ***********************************************************************
 * vmk_HelperCancelRequest --                                     */ /**
 *
 * \ingroup HelperWorlds
 * \brief Cancel requests on the queue that have the specified tag.
 *
 * \note This function may block if the queue has been created with
 *       blocking properties.
 *
 * \param[in]  helper         Helper queue on which to issue the cancel.
 * \param[in]  requestTag     Tag used to identify which requests to
 *                            cancel.
 * \param[out] numCancelled   Number of requests that were actually
 *                            cancelled.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_HelperCancelRequest(vmk_Helper helper,
                                         vmk_AddrCookie requestTag,
                                         vmk_uint32 *numCancelled);

/*
 ***********************************************************************
 * vmk_HelperCurrentRequestCount --                               */ /**
 *
 * \ingroup HelperWorlds
 * \brief Get a snapshot of the current number of pending requests on
 *        a helper queue.
 *
 * \note The count returned by this function does not include the
 *       count of requests that are actively being processed.
 *
 * \note This function will not block.
 *
 * \param[in]  helper         Helper queue to check.
 *
 * \return The number of pending requests on the helper queue.
 *
 ***********************************************************************
 */
vmk_uint32 vmk_HelperCurrentRequestCount(vmk_Helper helper);

#endif /* _VMKAPI_HELPER_WORLDS_H_ */
/** @} */
