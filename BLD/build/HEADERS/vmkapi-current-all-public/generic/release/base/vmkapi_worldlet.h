/* **********************************************************
 * Copyright 2007 - 2009 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 *******************************************************************************
 * Worldlets                                                              */ /**
 * \defgroup Worldlet Worldlets
 *
 * A worldlet is an object that performs a specified function at
 * some point in the future (relative to the time of invocation, which
 * is accomplished via vmk_WorldletActivate).
 *
 * \note
 * All functions of this API require that callers do not hold
 * any locks with rank _VMK_SP_RANK_IRQ_WORLDLET_LEGACY.
 *
 * @{
 * \defgroup Deprecated Deprecated APIs
 * @{
 *******************************************************************************
 */
#ifndef _VMKAPI_WORLDLET_H_
#define _VMKAPI_WORLDLET_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/**
 * \brief Affinity relationships.
 * \deprecated Worldlets are deprecated, use worlds instead.
 */
typedef enum {
   /**
    * \brief Exact CPU match required.
    */
   VMK_WDT_AFFINITY_EXACT = 0,
   /**
    * \brief Exact affinity match not required.  Will attempt to place worldlet
    * topologically close to the target.
    */
   VMK_WDT_AFFINITY_LOOSE = 1,
} vmk_WorldletAffinityType;

/**
 * \brief Worldlet Affinity Tracker object.
 * \deprecated Worldlets are deprecated, use worlds instead.
 */
typedef struct vmk_WorldletAffinityTracker vmk_WorldletAffinityTracker;

/*
 *******************************************************************************
 * vmk_WorldletFn --                                                      */ /**
 *
 * \ingroup Worldlet
 * \brief Prototype for Worldlet callback function.
 *
 * \deprecated Worldlets are deprecated, use worlds instead.
 * \note  Callbacks of this type may not block.
 *
 * \param[in]   wdt     Worldlet handle representing the executing worldlet.
 * \param[in]   private Private data as specified to vmk_WorldletCreate.
 * \param[out]  runData The Worldlet sets the "state" field of this struct to
 *                      define its state upon completion of the callback
 *                      function:
 *
 *                      VMK_WDT_RELEASE: Implies call to vmk_WorldletUnref
 *                                       and VMK_WDT_SUSPEND if the worldlet is
 *                                       not freed.
 *                      VMK_WDT_SUSPEND: The worldlet will not execute unless
 *                                       directed to do so by a
 *                                       vmk_WorldletActivate call.
 *                      VMK_WDT_READY:   The worldlet will be called again when
 *                                       the system decides to grant it CPU
 *                                       time.
 *
 *                      The "userData1" and "userData2" fields may be filled
 *                      with worldlet-specific performance data and the totals
 *                      over all worldlet invocations are published via VSI.
 *
 * \retval VMK_OK The worldlet function executed correctly.
 *                This is not a status of whether the actions of the function
 *                were successfully completed, but rather an indication that
 *                the code of the function executed.  The return of any other
 *                value may have undefined side-effects.
 *
 *******************************************************************************
 */
typedef VMK_ReturnStatus (*vmk_WorldletFn)(vmk_Worldlet wdt, void *data,
                                           vmk_WorldletRunData *runData);

/*
 *******************************************************************************
 * vmk_WorldletAllocSize --                                               */ /**
 *
 * \ingroup Worldlet
 * \brief Get the amount of memory to set aside for a single worldlet.
 *
 * \deprecated Worldlets are deprecated, use worlds instead.
 * \note  This function will not block.
 *
 * This call can be used to help compute the size of the heap necessary
 * to allocate worldlets.
 *
 * \returns Number of bytes to set aside in a heap for a single worldlet.
 *
 *******************************************************************************
 */
vmk_ByteCount vmk_WorldletAllocSize(
   void);

/*
 *******************************************************************************
 * vmk_WorldletCreate --                                                  */ /**
 *
 * \ingroup Worldlet
 * \brief Create a worldlet object.
 *
 * \deprecated Worldlets are deprecated, use worlds instead.
 * \note  This function will not block.
 *
 * \param[out] worldlet   Pointer to new vmk_Worldlet.
 * \param[in]  name       Descriptive name for the worldlet.
 * \param[in]  serviceID  Service ID against which the worldlet is charged.
 * \param[in]  moduleID   Id of module making request.
 * \param[in]  heapID     Id of the heap to allocate the worldlet from.
 * \param[in]  worldletFn Pointer to function called when dispatching the
 *                        request.
 * \param[in]  private    Arbitrary data to associate with the vmk_Worldlet.
 *
 * \retval VMK_OK               The worldlet was successfully initialized.
 * \retval VMK_NO_MEMORY        The worldlet could not be allocated.
 * \retval VMK_INVALID_HANDLE   The specified service ID is invalid.
 * \retval VMK_INVALID_NAME     The requested name was invalid.
 *
 *******************************************************************************
 */
VMK_ReturnStatus vmk_WorldletCreate(
   vmk_Worldlet       *worldlet,
   vmk_Name           *name,
   vmk_ServiceAcctID  serviceID,
   vmk_ModuleID       moduleID,
   vmk_HeapID         heapID,
   vmk_WorldletFn     worldletFn,
   void               *private);


/*
 *******************************************************************************
 * vmk_WorldletCheckState --                                              */ /**
 *
 * \ingroup Worldlet
 * \brief Query the state of a worldlet.
 *
 * \deprecated Worldlets are deprecated, use worlds instead.
 * \note  This function will not block.
 *
 * \param[in]  worldlet Pointer to new vmk_Worldlet.
 * \param[out] state    State of the worldlet.
 *
 * \retval VMK_OK             The worldlet state was successfully returned.
 * \retval VMK_BAD_PARAM      "state" is a bad pointer.
 * \retval VMK_INVALID_HANDLE "worldlet" is invalid or corrupted.
 *
 *******************************************************************************
 */
VMK_ReturnStatus
vmk_WorldletCheckState(
   vmk_Worldlet worldlet,
   vmk_WorldletState *state);

/*
 *******************************************************************************
 * vmk_WorldletActivate --                                                */ /**
 *
 * \ingroup Worldlet
 * \brief Activate a worldlet object.
 *
 * The worldlet's callback function will be called at least once following the
 * successful execution of this function (though that may be deferred due to
 * the worldlet being disabled).
 *
 * \deprecated Worldlets are deprecated, use worlds instead.
 * \note  This function will not block.
 *
 * \param[in] worldlet Worldlet to activate.
 *
 * \retval VMK_OK       The worldlet was successfully activated.
 *
 *******************************************************************************
 */
VMK_ReturnStatus vmk_WorldletActivate(
   vmk_Worldlet  worldlet);

/*
 *******************************************************************************
 * vmk_WorldletUnref --                                                   */ /**
 *
 * \ingroup Worldlet
 * \brief See vmk_WorldletUnrefWithData.
 *
 *******************************************************************************
 */
VMK_ReturnStatus vmk_WorldletUnref(
   vmk_Worldlet  worldlet);

/*
 *******************************************************************************
 * vmk_WorldletUnrefWithData --                                           */ /**
 *
 * \ingroup Worldlet
 * \brief Decrement the reference count on a worldlet, releasing the current
 * reference.
 *
 * When the reference count reaches 0, the worldlet will be destroyed (though
 * this may happen asynchronously with the reference being released).
 *
 * The vmkernel may maintain internal references to worldlets that may
 * temporarily prevent the reference count reaching 0.
 *
 * Users must ensure that a worldlet is quiesced in order to destroy it.
 * Thus a destruction sequence may contain a sequence of calls to:
 * - vmk_WorldletDisable() - prevent future activations
 * - vmk_WorldletWaitForQuiesce() - waiting for pending activations to finish
 * - vmk_WorldletUnref() - release the reference
 *
 * Unlike vmk_WorldletUnref, vmk_WorldletUnrefWithData returns the private
 * data registered with the worldlet when it was created.
 *
 * \deprecated Worldlets are deprecated, use worlds instead.
 * \note  This function will not block.
 *
 * \param[in]  worldlet  Worldlet to release.
 * \param[out] private   Private data associated with worldlet.
 *
 * \retval VMK_OK       The worldlet was successfully released.
 * \retval VMK_BUSY     The worldlet is still in use.
 *
 *******************************************************************************
 */
VMK_ReturnStatus vmk_WorldletUnrefWithData(
   vmk_Worldlet  worldlet,
   void          **private);

/*
 *******************************************************************************
 * vmk_WorldletReapSync --                                                */ /**
 *
 * \ingroup Worldlet
 * \brief Waits until the worldlet with the provided ID has been fully reaped.
 *
 * When the reference count reaches 0, the destruction of the worldlet is
 * scheduled but may happen asynchronously.  This function guarantees that all
 * resources for the worldlet (which may reside on a caller's heap) have been
 * released before it returns with success.
 *
 * \deprecated Worldlets are deprecated, use worlds instead.
 * \note  This function may block.
 *
 * \param[in]  id  Worldlet ID of the worldlet to wait on.
 *
 * \retval VMK_OK                The worldlet was reaped successfully.
 * \retval VMK_WAIT_INTERRUPTED  The waiting world was interrupted and may
 *                               reissue its wait by calling this function
 *                               again.
 * \retval VMK_DEATH_PENDING     The waiting world has been scheduled for death
 *                               and is expected to return as soon as possible.
 *
 *******************************************************************************
 */
VMK_ReturnStatus vmk_WorldletReapSync(
   vmk_WorldletID id);

/*
 *******************************************************************************
 * vmk_WorldletShouldYield --                                             */ /**
 *
 * \ingroup Worldlet
 * \brief Returns an indicator of whether a worldlet should yield.
 *
 * \deprecated Worldlets are deprecated, use worlds instead.
 * \note  This function will not block.
 *
 * \param[in]  worldlet       Currently executing worldlet.
 * \param[out] yield          Set to VMK_TRUE/VMK_FALSE to indicate if
 *                            worldlet should yield.
 *
 * \retval VMK_OK             Success.
 * \retval VMK_INVALID_HANDLE Worldlet is invalid or not running.
 * \retval VMK_BAD_PARAM      "yield" is a bad pointer.
 *
 *******************************************************************************
 */
VMK_ReturnStatus vmk_WorldletShouldYield(
   vmk_Worldlet   worldlet,
   vmk_Bool       *yield);

/*
 *******************************************************************************
 * vmk_WorldletGetCurrent --                                              */ /**
 *
 * \ingroup Worldlet
 * \brief Returns current executing worldlet and private data.
 *
 * \deprecated Worldlets are deprecated, use worlds instead.
 * \note  This function will not block.
 *
 * \param[out] worldlet       Currently executing worldlet.
 * \param[out] private        Private data associated with worldlet.
 *
 * \retval VMK_OK             Success.
 * \retval VMK_NOT_FOUND      Not running on a worldlet.
 * \retval VMK_NOT_SUPPORTED  System is in a state that could not return
 *                            the requested information.
 *
 *******************************************************************************
 */
VMK_ReturnStatus vmk_WorldletGetCurrent(
   vmk_Worldlet   *worldlet,
   void           **private);

/*
 *******************************************************************************
 * vmk_WorldletGetFn --                                                   */ /**
 *
 * \ingroup Worldlet
 * \brief Returns the function associated with a worldlet.
 *
 * \deprecated Worldlets are deprecated, use worlds instead.
 * \note  This function will not block.
 *
 * \param[in]  worldlet       Worldlet object.
 * \param[out] worldletFn     Pointer to function called when dispatching
 *                            the request.
 *
 * \retval VMK_OK             Success.
 * \retval VMK_INVALID_HANDLE "worldlet" is invalid.
 *
 *******************************************************************************
 */
VMK_ReturnStatus vmk_WorldletGetFn(
   vmk_Worldlet   worldlet,
   vmk_WorldletFn *worldletFn);

/*
 ******************************************************************************
 * vmk_WorldletSetAffinityToWorldlet --                                 */ /**
 *
 * \ingroup Worldlet
 * \brief Sets the affinity of one worldlet to another.
 *
 * This means that the system will attempt to execute the "worldlet" worldlet
 * on the same CPU as the "target" worldlet.  Unsetting this affinity is
 * accomplished by using a NULL value for the "target" worldlet.
 *
 * This function alters internal reference counts therefore if a worldlet is
 * used as a "target" worldlet the affinity to that worldlet must be torn down
 * prior to it being destroyed.
 *
 * \deprecated Worldlets are deprecated, use worlds instead.
 * \note  This function will not block.
 *
 * \param[in]  worldlet Worldlet whose affinity will be changed.
 * \param[in]  target   Worldlet to which "worldlet" will have
 *                      affinity to. (May be NULL.)
 * \param[in]  type     Type of affinity.
 *
 * \retval VMK_OK             Success.
 * \retval VMK_INVALID_HANDLE "worldlet" or "target" are invalid.
 *
 ******************************************************************************
 */
VMK_ReturnStatus vmk_WorldletSetAffinityToWorldlet(
   vmk_Worldlet                 worldlet,
   vmk_Worldlet                 target,
   vmk_WorldletAffinityType     type);

/*
 *******************************************************************************
 * vmk_WorldletSetAffinityToWorld --                                      */ /**
 *
 * \ingroup Worldlet
 * \brief Sets the affinity of one worldlet to a world.
 *
 * This means that the system will attempt to execute the "worldlet" worldlet on
 * the same CPU as the "target" world. Unsetting this affinity is accomplished
 * by using a VMK_INVALID_WORLD_ID value for the "target" world.
 *
 * \deprecated Worldlets are deprecated, use worlds instead.
 * \note  This function will not block.
 *
 * \param[in]  worldlet Worldlet whose affinity will be changed.
 * \param[in]  target   World to which "worldlet" will have
 *                      affinity to. (May be VMK_INVALID_WORLD_ID.)
 * \param[in]  type     Type of affinity.
 *
 * \retval VMK_OK             Success.
 * \retval VMK_INVALID_HANDLE "worldlet" or "target" are invalid.
 *
 *******************************************************************************
 */
VMK_ReturnStatus vmk_WorldletSetAffinityToWorld(
   vmk_Worldlet                 worldlet,
   vmk_WorldID                  target,
   vmk_WorldletAffinityType     type);

/*
 *******************************************************************************
 * vmk_WorldletInterruptSet --                                            */ /**
 *
 * \ingroup Worldlet
 * \brief Sets the interrupt cookie for the worldlet.
 *
 * Once set, the worldlet scheduler takes over interrupt scheduling. When the
 * worldlet moves, it's corresponding interrupt is moved too.
 *
 * \deprecated Worldlets are deprecated, use worlds instead.
 * \note  This function will not block.
 *
 * \param[in]  worldlet       Worldlet to set the interrupt association
 * \param[in]  intrCookie     Interrupt cookie previously registered via
 *                            vmk_IntrRegister
 *
 * \retval VMK_OK             Success.
 * \retval VMK_INVALID_HANDLE "worldlet" or "target" are invalid.
 *
 *******************************************************************************
 */
VMK_ReturnStatus
vmk_WorldletInterruptSet(
   vmk_Worldlet worldlet,
   vmk_IntrCookie intrCookie);


/*
 *******************************************************************************
 * vmk_WorldletInterruptUnSet --                                          */ /**
 *
 * \ingroup Worldlet
 * \brief Disassociate interrupt cookie associated with the worldlet.
 *
 * \deprecated Worldlets are deprecated, use worlds instead.
 * \note  This function will not block.
 *
 * \param[in]  worldlet       Worldlet the interrupt is associated to
 *
 * \retval VMK_OK             Success.
 * \retval VMK_INVALID_HANDLE "worldlet" or "target" are invalid.
 *
 *******************************************************************************
 */
VMK_ReturnStatus
vmk_WorldletInterruptUnSet(
   vmk_Worldlet worldlet);

/*
 *******************************************************************************
 * vmk_WorldletNameSet --                                                 */ /**
 *
 * \ingroup Worldlet
 * \brief Set the name of a worldlet object.
 *
 * \deprecated Worldlets are deprecated, use worlds instead.
 * \note  This function will not block.
 *
 * \param[in] worldlet   Worldlet object.
 * \param[in] name       Descriptive name for the worldlet.

 * \retval VMK_OK               Success.
 * \retval VMK_INVALID_HANDLE   "worldlet" or "target" are invalid.
 * \retval VMK_INVALID_NAME     "name" was invalid.
 *
 *******************************************************************************
 */
VMK_ReturnStatus vmk_WorldletNameSet(
   vmk_Worldlet worldlet,
   vmk_Name     *name);

/*
 *******************************************************************************
 * vmk_WorldletDisable --                                                 */ /**
 *
 * \ingroup Worldlet
 * \brief "Disable" a worldlet from running.
 *
 * A disabled worldlet is blocked from performing a transition from READY to
 * ACTIVE state; i.e.  if/when it reaches the front of the run-queue it is
 * prevented from being called.  When a worldlet is disabled, it may still be
 * running and that current invocation is not affected.
 *
 * \deprecated Worldlets are deprecated, use worlds instead.
 * \note  This function will not block.
 *
 * \param[in] worldlet   Worldlet object.
 *
 * \retval VMK_OK               Success.
 * \retval VMK_INVALID_HANDLE   "worldlet" is invalid.
 *
 *******************************************************************************
 */
VMK_ReturnStatus vmk_WorldletDisable(
   vmk_Worldlet worldlet);

/*
 *******************************************************************************
 * vmk_WorldletEnable --                                                  */ /**
 *
 * \ingroup Worldlet
 * \brief Reverses the effect of vmk_WorldletDisable.
 *
 * \deprecated Worldlets are deprecated, use worlds instead.
 * \note  This function will not block.
 *
 * \param[in] worldlet   Worldlet object.
 *
 * \retval VMK_OK               Success.
 * \retval VMK_INVALID_HANDLE   "worldlet" is invalid.
 *
 *******************************************************************************
 */
VMK_ReturnStatus vmk_WorldletEnable(
   vmk_Worldlet worldlet);

/*
 *******************************************************************************
 * vmk_WorldletIsQuiesced --                                              */ /**
 *
 * \ingroup Worldlet
 * \brief Identifies if a worldlet is quiesced; disabled and not active.
 *
 * \deprecated Worldlets are deprecated, use worlds instead.
 * \note  This function will not block.
 *
 * \param[in]  worldlet  Worldlet object.
 * \param[out] quiesced  Quiesce status of the worldlet.
 *
 * \retval VMK_OK               Success.
 * \retval VMK_INVALID_HANDLE   "worldlet" is invalid.
 * \retval VMK_BAD_PARAM        "quieseced" is a bad pointer.
 *
 *******************************************************************************
 */
VMK_ReturnStatus vmk_WorldletIsQuiesced(
   vmk_Worldlet worldlet,
   vmk_Bool *quiesced);

/*
 *******************************************************************************
 * vmk_WorldletWaitForQuiesce --                                          */ /**
 *
 * \ingroup Worldlet
 * \brief Waits at most "maxWaitUs" for "worldlet" to become quiesced.
 *
 * \deprecated Worldlets are deprecated, use worlds instead.
 * \note  This function will not block.
 *
 * \param[in] worldlet   Worldlet object.
 * \param[in] maxWaitUS  Maximum number of microseconds to wait.
 *
 * \retval VMK_OK               Success.
 * \retval VMK_INVALID_HANDLE   "worldlet" is invalid.
 * \retval VMK_IS_ENABLED       "worldlet" is enabled.
 * \retval VMK_TIMEOUT          "worldlet" did not quiesce.
 *
 *******************************************************************************
 */
VMK_ReturnStatus vmk_WorldletWaitForQuiesce(
   vmk_Worldlet worldlet,
   vmk_uint32 maxWaitUS);

/*
 *******************************************************************************
 * vmk_WorldletGetId --                                                   */ /**
 *
 * \ingroup Worldlet
 * \brief Returns id of given worldlet
 *
 * \deprecated Worldlets are deprecated, use worlds instead.
 * \note  This function will not block.
 *
 * \param[in]  worldlet   Worldlet object.
 * \param[out] id         Worldlet id.
 *
 * \retval VMK_OK               Success.
 * \retval VMK_INVALID_HANDLE   "worldlet" is invalid.
 *
 *******************************************************************************
 */
VMK_ReturnStatus vmk_WorldletGetId(
   vmk_Worldlet worldlet,
   vmk_WorldletID *id);

/*
 *******************************************************************************
 * vmk_WorldletAffinityTrackerAllocSize --                                */ /**
 *
 * \ingroup Worldlet
 * \brief Get the amount of memory to set aside for a single worldlet
 *        affinity tracker.
 *
 * \deprecated Worldlets are deprecated, use worlds instead.
 * \note  This function will not block.
 *
 * This call can be used to help compute the size of the heap necessary
 * to allocate worldlet affinity trackers.
 *
 * \returns Number of bytes to set aside in a heap for a single worldlet
 *          affinity tracker.
 *
 *******************************************************************************
 */
vmk_ByteCount vmk_WorldletAffinityTrackerAllocSize(
   void);

/*
 *******************************************************************************
 * vmk_WorldletAffinityTrackerCreate --                                   */ /**
 *
 * \ingroup Worldlet
 * \brief Create a worldlet affinity tracker object.
 *
 * An affinity tracker object collects samples of world or worldlet ids that
 * have a potential affinity relationship with a particular target worldlet.
 * Periodically the user of the tracker object will ask it to calculate an
 * affinity setting based on the id which collected the most samples over the
 * predetermined time frame (resetTime).  A "set threshold" and "drop
 * threshold" setting determine the percentage of samples that an id must
 * represent so that affinity is set or dropped.
 * vmk_WorldletAffinityTrackerCreate should be used to allocate the object and
 * subsequently vmk_WorldletAffinityTrackerConfig should be used to set the
 * parameters of the tracker object.  Until vmk_WorldletAffinityTrackerConfig
 * is called the object will not set any affinities.
 *
 * Caller is required to provide appropriate locking or exclusion.
 *
 * \deprecated Worldlets are deprecated, use worlds instead.
 * \note  This function will not block.
 *
 * \param[in]  tracked     Worldlet who's affinity setting will be managed.
 * \param[in]  heap        Heap the affinity tracker will be allocated from.
 * \param[out] tracker     Tracker object created.
 *
 * \retval VMK_OK               Success.
 * \retval VMK_NO_MEMORY        Allocation failed.
 * \retval VMK_BAD_PARAM        Bad "tracker" or "tracked" pointer.
 * \retval VMK_INVALID_HANDLE   Bad "tracked" worldlet.
 *
 *******************************************************************************
 */
VMK_ReturnStatus
vmk_WorldletAffinityTrackerCreate(
   vmk_Worldlet tracked,
   vmk_HeapID heap,
   vmk_WorldletAffinityTracker **tracker
);

/*
 *******************************************************************************
 * vmk_WorldletAffinityTrackerConfig --                                   */ /**
 *
 * \ingroup Worldlet
 * \brief Configures a worldlet affinity tracker object.
 *
 * Defines the thresholds, reset frequency and affinity types of an affinity
 * tracker object.  May be called multiple times.
 *
 * Caller is required to provide appropriate locking or exclusion.
 *
 * \deprecated Worldlets are deprecated, use worlds instead.
 * \note  This function will not block.
 *
 * \param[in]  tracker      Tracker object to initialize.
 * \param[in]  setThresh    Usage threshold (%) at which to establish affinity.
 * \param[in]  dropThresh   Usage threshold (%) at which to drop affinity.
 * \param[in]  resetTimeUS  Minimum microseconds between updates.
 * \param[in]  worldAffType Type of affinity to apply to worlds.
 * \param[in]  wdtAffType   Type of affinity to apply to worldlets.
 *
 * \retval VMK_OK               Success.
 * \retval VMK_BAD_PARAM        A threshold is invalid (> 100).
 *
 *******************************************************************************
 */
VMK_ReturnStatus
vmk_WorldletAffinityTrackerConfig(
   vmk_WorldletAffinityTracker *tracker,
   vmk_uint16 setThresh,
   vmk_uint16 dropThresh,
   vmk_uint16 resetTimeUS,
   vmk_WorldletAffinityType worldAffType,
   vmk_WorldletAffinityType wdtAffType
);

/*
 *******************************************************************************
 * vmk_WorldletAffinityTrackerDestroy --                                  */ /**
 *
 * \ingroup Worldlet
 * \brief Destroy a worldlet affinity tracker object.
 *
 * Does not alter the affinity of the worldlet for which affinity tracking is
 * being performed.
 *
 * Caller is required to provide appropriate locking or exclusion.
 *
 * \deprecated Worldlets are deprecated, use worlds instead.
 * \note  This function will not block.
 *
 * \param[in]  tracker     Tracker object to destroy.
 *
 * \retval VMK_OK               Success.
 * \retval VMK_BAD_PARAM        Bad "tracker" pointer.
 *
 *******************************************************************************
 */
VMK_ReturnStatus
vmk_WorldletAffinityTrackerDestroy(
   vmk_WorldletAffinityTracker *tracker
);

/*
 *******************************************************************************
 * vmk_WorldletAffinityTrackerAddWorldSample --                           */ /**
 *
 * \ingroup Worldlet
 * \brief Adds a sample for a world id to an affinity tracker.
 *
 * Caller is required to provide appropriate locking or exclusion.
 *
 * \deprecated Worldlets are deprecated, use worlds instead.
 * \note  This function will not block.
 *
 * \param[in]  tracker     Tracker object to add sample to.
 * \param[in]  userID      World id of sample to add.
 * \param[in]  count       Number of samples to add.
 *
 * \retval VMK_OK          Success.
 *
 *******************************************************************************
 */

VMK_ReturnStatus
vmk_WorldletAffinityTrackerAddWorldSample(
   vmk_WorldletAffinityTracker *tracker,
   vmk_WorldID userID,
   vmk_uint32 count);

/*
 *******************************************************************************
 * vmk_WorldletAffinityTrackerAddWorldletSample --                        */ /**
 *
 * \ingroup Worldlet
 * \brief Adds a sample for a world id to an affinity tracker.
 *
 * Caller is required to provide appropriate locking or exclusion.
 *
 * \deprecated Worldlets are deprecated, use worlds instead.
 * \note  This function will not block.
 *
 * \param[in]  tracker     Tracker object to add sample to.
 * \param[in]  userID      Worldlet id of sample to add.
 * \param[in]  count       Number of samples to add.
 *
 * \retval VMK_OK          Success.
 *
 *******************************************************************************
 */

VMK_ReturnStatus
vmk_WorldletAffinityTrackerAddWorldletSample(
   vmk_WorldletAffinityTracker *tracker,
   vmk_WorldletID userID,
   vmk_uint32 count);

/*
 *******************************************************************************
 * vmk_WorldletAffinityTrackerCheck --                                    */ /**
 *
 * \ingroup Worldlet
 * \brief Update the tracked worldlet's affinity based on collected samples.
 *
 * If the indicated time has surpassed the resetTimeUS as indicated in the
 * object initializaiton then look at the collected samples and update the
 * tracked worldlet's affinity as appropriate according to the tracker object's
 * specified threshold settings.  Reset the collected samples and begin a new
 * sample period.
 *
 * Caller is required to provide appropriate locking or exclusion.
 *
 * \deprecated Worldlets are deprecated, use worlds instead.
 * \note  This function will not block.
 *
 * \param[in]  tracker Tracker object to check.
 * \param[in]  now     Current time to consider.
 *
 * \retval VMK_OK      Success.
 * \retval VMK_RETRY   Not enough time has passed since the last updated.
 *
 *******************************************************************************
 */

VMK_ReturnStatus
vmk_WorldletAffinityTrackerCheck(
   vmk_WorldletAffinityTracker *tracker,
   vmk_TimerCycles now);

/*
 *******************************************************************************
 * vmk_WorldletOptionSet --                                               */ /**
 *
 * \ingroup Worldlet
 * \brief Sets one of the option flags for a worldlet to the given value.
 *        Operations on invalid flags always succeed (the system ignores the
 *        settings of such flags).
 *
 * \deprecated Worldlets are deprecated, use worlds instead.
 * \note  This function will not block.
 *
 * \param[in]  wdt       Worldlet to be operated on.
 * \param[in]  flag      Flag to modify.
 * \param[in]  value     Value to set the flag to.
 *
 * \retval VMK_OK        Success.
 * \retval VMK_BAD_PARAM "wdt" parameter is invalid.
 *
 *******************************************************************************
 */

VMK_ReturnStatus
vmk_WorldletOptionSet(
   vmk_Worldlet wdt,
   vmk_WorldletOptions flag,
   vmk_Bool value);

#endif /* _VMKAPI_WORLDLET_H_ */
/** @} */
/** @} */
