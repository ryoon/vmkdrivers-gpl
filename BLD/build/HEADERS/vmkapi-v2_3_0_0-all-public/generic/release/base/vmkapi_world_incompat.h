/* **********************************************************
 * Copyright 2010, 2012 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Worlds                                                         */ /**
 * \defgroup Worlds Worlds
 *
 *@{
 ***********************************************************************
 */

#ifndef _VMKAPI_WORLD_INCOMPAT_H_
#define _VMKAPI_WORLD_INCOMPAT_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/**
 * \brief General category of reason for blocking.
 */
typedef enum {
    /** \brief Used to indicate miscellaneous blocking */
   VMK_WORLD_WAIT_MISC=0,

   /** \brief Used to indicate blocking during SCSI plugin work */
   VMK_WORLD_WAIT_SCSI_PLUGIN=1,
   
   /** \brief Used to indicate blocking during Network operations */
   VMK_WORLD_WAIT_NET=2,
   
   /**
    * \brief All wait reasons will be less than this.
    * \note This is not a valid wait reason.
    */
   VMK_WORLD_WAIT_MAX
} vmk_WorldWaitReason;


/*
 ***********************************************************************
 * vmk_WorldWait --                                               */ /**
 *
 * \ingroup Worlds
 * \brief Deschedule a World holding an Non-IRQ spinlock until awakened.
 *
 * \note Spurious wakeups are possible.
 *
 * \note The lock should be of rank VMK_SP_RANK_IRQ_BLOCK_LEGACY or lower
 *       otherwise a lock rank violation will occur.
 *
 * \param[in] eventId   System wide unique identifier of the event
 *                      to sleep on.
 * \param[in] lock      Non-IRQ spinlock to release before descheduling
 *                      the world.
 * \param[in] reason    Subsystem/reason for the descheduling.
 *
 * \retval VMK_OK                Woken up by a vmk_WorldWakeup on
 *                               eventId.
 * \retval VMK_DEATH_PENDING     Woken up because the world is dying
 *                               and being reaped by the scheduler.
 * \retval VMK_WAIT_INTERRUPTED  Woken for some other reason.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_WorldWaitLegacy(
   vmk_WorldEventID eventId,
   vmk_Spinlock *lock,
   vmk_WorldWaitReason reason);
   
/*
 ***********************************************************************
 * vmk_WorldWaitIRQ --                                            */ /**
 *
 * \ingroup Worlds
 * \brief Deschedule a World holding an IRQ spinlock until awakened.
 *
 * \note Spurious wakeups are possible.
 *
 * \param[in] eventId   System wide unique identifier of the event
 *                      to sleep on.
 * \param[in] lock      IRQ spinlock to release before descheduling
 *                      the world.
 * \param[in] reason    Subsystem/reason for the descheduling.
 * \param[in] flags     IRQ flags returned by IRQ spinlock function.
 *
 * \retval VMK_OK                Woken up by a vmk_WorldWakeup on
 *                               eventId.
 * \retval VMK_DEATH_PENDING     Woken up because the world is dying and
 *                               being reaped by the scheduler.
 * \retval VMK_WAIT_INTERRUPTED  Woken for some other reason.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_WorldWaitIRQLegacy(
   vmk_WorldEventID eventId,
   vmk_SpinlockIRQ *lock,
   vmk_WorldWaitReason reason,
   unsigned long flags);

/*
 ***********************************************************************
 * vmk_WorldTimedWait --                                          */ /**
 *
 * \ingroup Worlds
 * \brief Deschedule a World holding an Non-IRQ Spinlock until awakened
 *        or until the specified timeout expires.
 *
 * \note Spurious wakeups are possible.
 *
 * \param[in]  eventId     System wide unique identifier of the event
 *                         to sleep on.
 * \param[in]  lock        Non-IRQ spinlock to release before
 *                         descheduling the world.
 * \param[in]  reason      Subsystem/reason for the descheduling
 * \param[in]  timeoutUs   Number of microseconds before timeout.
 * \param[out] timedOut    If non-NULL, set to TRUE if wakeup was
 *                         due to timeout expiration, FALSE otherwise.
 *
 * \retval VMK_OK                Woken up by a vmk_WorldWakeup on
 *                               eventId or by timeout expiration.
 * \retval VMK_DEATH_PENDING     Woken up because the world is dying
 *                               and being reaped by the scheduler.
 * \retval VMK_WAIT_INTERRUPTED  Woken for some other reason.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_WorldTimedWaitLegacy(
   vmk_WorldEventID eventId,
   vmk_Spinlock *lock,
   vmk_WorldWaitReason reason,
   vmk_uint64 timeoutUs,
   vmk_Bool *timedOut);

/*
 ***********************************************************************
 * vmk_WorldSetAffinity --                                        */ /**
 *
 * \ingroup Worlds
 * \brief Set the PCPU affinity for the given non-running world
  
 * \note If the world is currently running and migration must take place
 *       for compliance with the given affinity, the change will not
 *       be immediate.
 *
 * \param[in] id              World ID to identify the world.
 * \param[in] affinityMask    Bitmask of physical CPUs on which the
 *                            world will be allowed to run.
 *
 * \retval VMK_NOT_FOUND   The given world ID does not exist.
 * \retval VMK_BAD_PARAM   The specified affinity is invalid for the
 *                         given world.
 *
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_WorldSetAffinity(
    vmk_WorldID id,
    vmk_AffinityMask affinityMask);

#endif /* _VMKAPI_WORLD_INCOMPAT_H_ */
/** @} */
