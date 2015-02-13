/* **********************************************************
 * Copyright 2007 - 2009 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Worlds                                                         */ /**
 * \defgroup Worlds Worlds
 *
 * Worlds are a schedulable entity in the VMkernel. For example worlds
 * are used to represent kernel threads, user processes, virtual CPUs,
 * etc. Each world is identified by an ID that is unique within the
 * system called the World ID. Worlds are blockable contexts; it is safe
 * to call functions that may block from a world context. 
 *
 *@{
 ***********************************************************************
 */

#ifndef _VMKAPI_WORLD_H_
#define _VMKAPI_WORLD_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/**
 * \brief Event to block on.
 *
 * Events to block on are, by convention, always kernel virtual addresses.
 */
typedef vmk_VA vmk_WorldEventID;

/** \brief The body function of a world. */
typedef VMK_ReturnStatus (*vmk_WorldStartFunc)(void *data);

/** \brief Opaque handle for a world */
typedef vmk_int32 vmk_WorldID;

#define VMK_INVALID_WORLD_ID ((vmk_WorldID)0)

/**
 * \brief Indication of unlimited CPU allocation (max)
 */
#define VMK_CPU_ALLOC_UNLIMITED ((vmk_uint32) -1)

/*
 ***********************************************************************
 * vmk_WorldCreate --                                             */ /**
 *
 * \ingroup Worlds
 * \brief Create and start a new kernel thread
 *
 * \note  This function may block.
 *
 * \warning Code running inside a world should allow the scheduler to
 *          run as often as possible. This can be achieved by calling
 *          vmk_WorldYield. At most code should run 30ms without calling
 *          vmk_WorldYield.
 *
 * \param[in] moduleID        Module on whose behalf the world is running.
 * \param[in] name            A string that describes the world. The name
 *                            will show up as debug information.
 * \param[in] startFunction   Function that the world begins executing
 *                            on creation.
 * \param[in] data            Argument to be passed to startFunction.
 * \param[out] worldId        World ID associated with the newly
 *                            created world. May be set to NULL if
 *                            the caller does not need the World ID.
 *
 * \retval VMK_OK             World created.
 * \retval VMK_NO_MEMORY      Ran out of memory.
 * \retval VMK_DEATH_PENDING  World is in the process of dying.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_WorldCreate(
   vmk_ModuleID moduleID,
   const char *name,
   vmk_WorldStartFunc startFunction,
   void *data,
   vmk_WorldID *worldId);

/*
 ***********************************************************************
 * vmk_WorldExit --                                               */ /**
 *
 * \ingroup Worlds
 * \brief End execution of the calling world.
 *
 * \note  This function never returns.
 *
 * \param[in] status   Status of the world on exit.
 *
 ***********************************************************************
 */
void vmk_WorldExit(VMK_ReturnStatus status);

/*
 ***********************************************************************
 * vmk_WorldGetID --                                              */ /**
 *
 * \ingroup Worlds
 * \brief Get the ID of the current world.
 *
 * \note  This function will not block.
 *
 * \return  WorldID of the currently running world that this
 *          call was invoked from or VMK_INVALID_WORLD_ID if
 *          this call was not invoked from a world context.
 *
 ***********************************************************************
 */
vmk_WorldID vmk_WorldGetID(
   void);

/*
 ***********************************************************************
 * vmk_WorldAssertIsSafeToBlockInt --
 *
 * This is used by vmk_WorldAssertIsSafeToBlock.  VMKAPI clients should
 * not call this function directly.
 *
 ***********************************************************************
 */

/** \cond nodoc */
void
vmk_WorldAssertIsSafeToBlockInt(
   void);
/** \endcond */

/*
 ***********************************************************************
 * vmk_WorldAssertIsSafeToBlock --                                */ /**
 *
 * \ingroup Globals
 * \brief Assert that it is OK for the caller to block.
 *
 * \note Only performs checking in debug builds.
 * \note This function will not block.
 *
 ***********************************************************************
 */

static VMK_ALWAYS_INLINE void 
vmk_WorldAssertIsSafeToBlock(
   void)
{
#ifdef VMX86_DEBUG
   vmk_WorldAssertIsSafeToBlockInt();
#endif
}

/*
 ***********************************************************************
 * vmk_WorldWait --                                               */ /**
 *
 * \brief Deschedule a World holding a Lock until awakened or until the
 *        specified timeout expires.
 *
 * \note  This function may block.
 *
 * \note Spurious wakeups are possible
 *
 * \param[in]  eventId     System wide unique identifier of the event
 *                         to sleep on.
 * \param[in]  lock        Lock of type VMK_SPINLOCK or VMK_SPINLOCK_IRQ
 *                         to release before descheduling the world.
 *                         VMK_LOCK_INVALID indicates that no lock needs
 *                         to be released before descheduling the world.
 * \param[in]  timeoutMS   Number of milliseconds before timeout
 *                         VMK_TIMEOUT_UNLIMITED_MS indicates that the
 *                         caller wants to block forever.
 *                         VMK_TIMEOUT_NONBLOCKING is not a valid value
 *                         in this context.
 * \param[in]  reason      A short string that explains the reason for
 *                         the vmk_WorldWait call.
 *
 * \retval VMK_OK                World was descheduled and awoken by a
 *                               vmk_WorldWakeup on eventId.
 * \retval VMK_BAD_PARAM         World was not descheduled because a
 *                               provided parameter was invalid.  If a
 *                               lock was provided then it was not
 *                               released.
 * \retval VMK_TIMEOUT           World was descheduled and awoken
 *                               because of timeout expiration.
 * \retval VMK_DEATH_PENDING     World was descheduled and awoken
 *                               because the world is dying and being
 *                               reaped by the scheduler. The caller is
 *                               expected to return as soon as possible.
 * \retval VMK_WAIT_INTERRUPTED  World was descheduled and awoken for
 *                               some other reason not specified by
 *                               previous return codes. The caller is
 *                               allowed to re-enter vmk_WorldWait.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_WorldWait(
   vmk_WorldEventID eventId,
   vmk_Lock lock,
   vmk_uint32 timeoutMS,
   const char *reason);


/*
 ***********************************************************************
 * vmk_WorldWakeup --                                             */ /**
 *
 * \ingroup Worlds
 * \brief Wake up all the Worlds waiting on a event eventId
 *
 * \note  This function may block.
 *
 * \param[in] eventId      System wide unique identifier of the event.
 *
 * \retval VMK_OK          One or more worlds was awakened.
 * \retval VMK_NOT_FOUND   No worlds were found that wake up to eventId.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_WorldWakeup(
   vmk_WorldEventID eventId);

/*
 ***********************************************************************
 * vmk_WorldSleep --                                              */ /**
 *
 * \ingroup Worlds
 * \brief Wait for at least the amount of time given.
 *
 * \note  This function may block.
 *
 * \note  Very short delays may not result in the calling world
 *        blocking.
 *
 * \param[in] delayUs   Duration to wait in microseconds.
 *
 * \retval VMK_OK                Awakened after at least the specified
 *                               delay.
 * \retval VMK_DEATH_PENDING     Awakened because the world is dying
 *                               and being reaped by the scheduler.
 *                               The entire delay may not have passed.
 * \retval VMK_WAIT_INTERRUPTED  Awakened for some other reason.  The
 *                               entire delay may not have passed.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_WorldSleep(
   vmk_uint64 delayUs);

/*
 ***********************************************************************
 * vmk_WorldYield --                                              */ /**
 *
 * \ingroup Worlds
 * \brief Invokes the scheduler, possibly descheduling the calling
 *        World in favor of another.  The calling world continues to
 *        be runnable and may resume running at any time.
 *
 * \note  This function may block.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_WorldYield(
   void);

/*
 ***********************************************************************
 * vmk_WorldGetCPUMinMax --                                       */ /**
 *
 * \ingroup Worlds
 * \brief Get the min/max CPU allocation in MHz for the running world.
 *
 * \note  This function will not block.

 * \param[out] min   Minimum CPU allocation of the current running
 *                   world in MHz.
 * \param[out] max   Maximum CPU allocation of the current running
 *                   world in MHz.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_WorldGetCPUMinMax(
    vmk_uint32 *min,
    vmk_uint32 *max);

/*
 ***********************************************************************
 * vmk_WorldSetCPUMinMax --                                       */ /**
 *
 * \ingroup Worlds
 * \brief Set the min/max CPU allocation in MHz for the running world

 * \note The scheduling of the current world will change based on the
 *       input parameters.
 *
 * \note  This function will not block.
 *
 * \param[in] min    Minimum CPU allocation in MHz that will be
 *                   guaranteed.
 * \param[in] max    Maximum CPU allocation in MHz that the world can
 *                   use.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_WorldSetCPUMinMax(
    vmk_uint32 min,
    vmk_uint32 max);

/*
 ***********************************************************************
 * vmk_WorldsMax --                                               */ /**
 *
 * \ingroup Worlds
 * \brief Returns the maximum possible number of worlds the system
 *        will support.
 *
 * \note  This function will not block.

 * \note This includes VMs and any other worlds that the system needs
 *       to execute.
 *
 ***********************************************************************
 */
vmk_uint32 vmk_WorldsMax(void);

/*
 ***********************************************************************
 * vmk_WorldWaitForDeath --                                      */ /**
 *
 * \ingroup Worlds
 * \brief Waits until the specified world is no longer running.
 *
 * Note that holding an open world private storage handle on the
 * target world (via the vmk_WorldStorageLookUp interface) will
 * prevent the target world from dying, and will thus cause this
 * call to deadlock with the target world.  All such open handles
 * on the target world must be released via vmk_WorldStorageRelease
 * before calling this function.
 *
 * \note  This function may block.
 *
 * \retval VMK_OK              The requested world has died.
 * \retval VMK_INVALID_WORLD   The specified world was invalid.  It
 *                             may have already died.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_WorldWaitForDeath(vmk_WorldID worldID);

#endif /* _VMKAPI_WORLD_H_ */
/** @} */
