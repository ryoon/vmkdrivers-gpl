/* **********************************************************
 * Copyright 2007 - 2014 VMware, Inc.  All rights reserved.
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
 * User processes also belong to cartels.  A cartel is a group of worlds
 * that share certain resources including but not limited to their
 * address space.  A cartel can be thought of as a user process and the
 * individual worlds that make up the cartel can be thought of as user
 * threads.  Each user world will have a world ID and a cartel ID.  Each
 * world in a cartel will share the same cartel ID.  Note that it is
 * possible for a world's world ID and cartel ID to be equal, but
 * this can only be true for at most one world in the cartel.
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

#include "hardware/vmkapi_intr_types.h"

#if defined(VMK_BUILDING_FOR_KERNEL_MODE)
/**
 * \brief Event to block on.
 *
 * Events to block on are, by convention, always kernel virtual addresses.
 */
typedef vmk_VA vmk_WorldEventID;

/** \brief The body function of a world. */
typedef VMK_ReturnStatus (*vmk_WorldStartFunc)(void *data);

#endif /* VMK_BUILDING_FOR_KERNEL_MODE */

/** \brief Opaque handle for a world */
typedef vmk_int32 vmk_WorldID;

#define VMK_INVALID_WORLD_ID ((vmk_WorldID)0)

#if defined(VMK_BUILDING_FOR_KERNEL_MODE)
/** \brief Special vmk_WorldEventID for a no-event wait
 *
 * When used as the eventId parameter to vmk_WorldWait(),
 * the world will wait without being added to the system's
 * internal event queue.  This allows for a lighter weight
 * synchronization mechanism, though the world sleeping in
 * this fashion can be awoken only by vmk_WorldForceWakeup(),
 * by a timeout, or if the world is being destroyed.
 */
#define VMK_EVENT_NONE ((vmk_WorldEventID)0)

/**
 * \brief Indication of unlimited CPU allocation (max)
 */
#define VMK_CPU_ALLOC_UNLIMITED ((vmk_uint32) -1)

/**
 * \brief Scheduler Class for a World
 *
 * Worlds of class VMK_WORLD_SCHED_CLASS_QUICK will be scheduled
 * to run ahead of worlds of class VMK_WORLD_SCHED_CLASS_DEFAULT.
 * Please be careful to use VMK_WORLD_SCHED_CLASS_QUICK only for
 * small deferred tasks that must execute promptly.
 *
 * If unsure of what class to use, pick VMK_WORLD_SCHED_CLASS_DEFAULT.
 */
typedef enum {

   /** Default scheduler class for kernel threads. */
   VMK_WORLD_SCHED_CLASS_DEFAULT = 1,

   /** Scheduler class for kernel threads that must execute prompty. */
   VMK_WORLD_SCHED_CLASS_QUICK = 100,

} vmk_WorldSchedClass;

/**
 * \brief Properties for creating a new world.
 */
typedef struct vmk_WorldProps {
   /** \brief Name associated with this world. */
   const char *name;

   /** \brief Module ID of the module creating this world. */
   vmk_ModuleID moduleID;

   /** \brief Function that the world begins executing at creation. */
   vmk_WorldStartFunc startFunction;

   /** \brief Opaque argument to the startFunction. */
   void *data;

   /** \brief Scheduler class for the new world. */
   vmk_WorldSchedClass schedClass;

   /** \brief Heap provided by the caller used for world creation only. */
   vmk_HeapID heapID;

} vmk_WorldProps;

/*
 ***********************************************************************
 * vmk_WorldCreateAllocSize --                                    */ /**
 *
 * \ingroup Worlds
 * \brief Returns heap size needed during world creation.
 *
 * \allocsizenote
 *
 * \param[out] alignment  Required pointer to where alignment value
 *                        is written.
 *
 * \retval vmk_ByteCount  Number of bytes allocated from Heap.
 *
 ***********************************************************************
 */
vmk_ByteCount vmk_WorldCreateAllocSize(vmk_ByteCount *alignment);

/*
 ***********************************************************************
 * vmk_WorldCreate --                                             */ /**
 *
 * \ingroup Worlds
 * \brief Create and start a new system world
 *
 * \note  This function may block.
 *
 * \note  This function allocates memory from the provided heap.
 *        Therefore, if the caller's heap needs to be destroyed
 *        use vmk_HeapDestroySync() to make sure the heap stays valid
 *        until the new world runs.
 *
 * \warning Code running inside a system world can be preempted by
 *          default.  Code should be careful not to access Per-PCPU
 *          Storage, or to hold locks of type VMK_SPINLOCK_IRQ, for
 *          a period time exceeding 10us - since these acquisitions
 *          block preemption.
 *
 * \param[in]  props          Properties of this world.
 * \param[out] worldId        World ID associated with the newly
 *                            created world. If caller sets this
 *                            poitner to NULL, then the World ID is
 *                            not returned.
 *
 * \retval VMK_OK             World created.
 * \retval VMK_NO_MEMORY      Ran out of memory.
 * \retval VMK_DEATH_PENDING  World is in the process of dying.
 * \retval VMK_NO_MODULE_HEAP The module's heap is not set.
 * \retval VMK_BAD_PARAM      The priority or the heapId specified
 *                            in the properties is invalid.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_WorldCreate(vmk_WorldProps *props,
                                 vmk_WorldID *worldId);

/*
 ***********************************************************************
 * vmk_WorldDestroy --                                             */ /**
 *
 * \ingroup Worlds
 * \brief Destroy a world created by vmk_WorldCreate.
 *
 * \note  This function may block.
 *
 * \note  This function does not wait for the world to actually die.
 *        Use vmk_WorldWaitForDeath() to wait for death.
 *
 * \param[in] worldID         vmk_WorldID of the world to destroy.
 *
 * \retval VMK_OK             Kill was successfully posted.
 * \retval VMK_NOT_FOUND      Specified worldID was not found.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_WorldDestroy(
   vmk_WorldID worldID);

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
 * vmk_WorldGetCartelID --                                        */ /**
 *
 * \ingroup Worlds
 * \brief Get the Cartel ID of the current world.
 *
 * \note  This function will not block.
 *
 * \return  WorldID of the cartel of the currently running world that
 *          this call was invoked from or VMK_INVALID_WORLD_ID if
 *          this call was not invoked from a user world context.
 *
 ***********************************************************************
 */
vmk_WorldID vmk_WorldGetCartelID(
   void);

/*
 ***********************************************************************
 * vmk_WorldIDToCartelID --                                       */ /**
 *
 * \ingroup Worlds
 * \brief Get the Cartel ID of the specified world.
 *
 * \note  This function will not block.
 *
 * \return  WorldID of the cartel of the provided world ID or
 *          VMK_INVALID_WORLD_ID if the provided world is not a user
 *          world context.
 *
 ***********************************************************************
 */
vmk_WorldID vmk_WorldIDToCartelID(
   vmk_WorldID worldID);

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
 * \brief Deschedule a World holding a non-IRQ Lock until awakened or
 *        until the specified timeout expires.
 *
 * \note  This function may block.
 *
 * \note Spurious wakeups are possible.  Specifially, users of this
 *       API must always verify that the condition on which they
 *       are waiting has actually occurred before taking action.
 *
 * \param[in]  eventId     Either a system wide unique identifier for the
 *                         event to sleep on, or VMK_EVENT_NONE.  When
 *                         VMK_EVENT_NONE is specified, the world can be
 *                         awoken only by vmk_WorldForceWakeup(), by a
 *                         timeout, or if the world is being destroyed.
 * \param[in]  lock        Lock of type VMK_SPINLOCK to release before
 *                         descheduling the world.  VMK_LOCK_INVALID 
 *                         indicates that no lock needs to be released
 *                         before descheduling the world.
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
 * vmk_WorldForceWakeup --                                        */ /**
 *
 * \ingroup Worlds
 * \brief Wake up a specific world directly, regardless of what
 *        event it is waiting on.
 *
 * \note  This function may block.
 *
 * \note  A forced wakeup is special, in that the wakeup it generates
 *        is "stateful" when used in conjuction with VMK_EVENT_NONE.
 *        Specifically, if a world is not waiting when a forced wakeup
 *        is generated, then the wakeup goes pending.  If the world
 *        then attempts to wait on VMK_EVENT_NONE, the world will
 *        not sleep (and the pending is cleared).
 *
 * \param[in] worldID      World ID of the world to wake up.
 *
 * \retval VMK_OK             The world was awoken.
 * \retval VMK_NOT_FOUND      The world was not found to need wakeup.
 * \retval VMK_INVALID_WORLD  The world ID did not correspond to an
 *                            existing world.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_WorldForceWakeup(
   vmk_WorldID worldID);

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
VMK_ReturnStatus vmk_WorldWaitForDeath(
   vmk_WorldID worldID);

/*
 ***********************************************************************
 * vmk_WorldInterruptSet --                                       */ /**
 *
 * \brief Sets an interrupt association for the specified world.
 *
 * Interrupts will be delivered to the same PCPU that this world is
 * running on.
 *
 * \note  This function will not block.
 *
 * \param[in] worldID          ID of the world to set interrupt
 *                             association for.
 * \param[in] intrCookie       Interrupt cookie previously retrieved via
 *                             vmk_IntrRegister
 *
 * \retval VMK_OK              The interrupt association has been set.
 * \retval VMK_BAD_PARAM       The specified interrupt cookie is
 *                             invalid.
 * \retval VMK_LIMIT_EXCEEDED  The specified interrupt cookie cannot be
 *                             added.
 * \retval VMK_INVALID_WORLD   The specified world id was invalid.
 * \retval VMK_FAILURE         The interrupt association cannot be set.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_WorldInterruptSet(
   vmk_WorldID worldID,
   vmk_IntrCookie intrCookie);

/*
 ***********************************************************************
 * vmk_WorldInterruptUnset --                                     */ /**
 *
 * \ingroup Worlds
 * \brief Unsets an interrupt association for the specified world.
 *
 * \note  This function will not block.
 *
 * \param[in] worldID          ID of the world to unset interrupt
 *                             association for.
 * \param[in] intrCookie       Interrupt cookie previously retrieved via
 *                             vmk_IntrRegister
 *
 * \retval VMK_OK              The interrupt association has been unset.
 * \retval VMK_NOT_FOUND       The interrupt is currently not associated
 *                             with the world.
 * \retval VMK_INVALID_WORLD   The specified world id was invalid.
 * \retval VMK_FAILURE         The interrupt association cannot be unset.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_WorldInterruptUnset(
   vmk_WorldID worldID,
   vmk_IntrCookie intrCookie);

/*
 ***********************************************************************
 * vmk_WorldRelationAdd --                                        */ /**
 *
 * \brief Add a relationship between two worlds
 *
 * This interface should be used to give a hint to the scheduler that there is a
 * communication flow from world A to world B. This is useful for multi-threaded
 * producer-consumer implementations. World A would equal the producer and world
 * B would equal the consumer. If both worlds can be a producer and consumer
 * then the interface should be called twice.
 * Establishing a relationship between worlds is an important performance
 * optimization and should always be done for worlds that are part of a hot
 * path.
 *
 * \note  This function might block.
 *
 * \param[in] worldA           ID of the world initiating the communication.
 * \param[in] worldB           ID of the world receiving the communication.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_WorldRelationAdd(
   vmk_WorldID worldA,
   vmk_WorldID worldB);


/*
 ***********************************************************************
 * vmk_WorldRelationRemove --                                     */ /**
 *
 * \brief Remove a relationship between two worlds
 *
 * The relationship must have previously been set through vmk_WorldRelationAdd.
 *
 * \note  This function might block.
 *
 * \param[in] worldA           ID of the world initiating the communication.
 * \param[in] worldB           ID of the world receiving the communication.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_WorldRelationRemove(
   vmk_WorldID worldA,
   vmk_WorldID worldB);

#endif /* VMK_BUILDING_FOR_KERNEL_MODE */

#endif /* _VMKAPI_WORLD_H_ */
/** @} */
