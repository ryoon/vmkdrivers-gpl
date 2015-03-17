/* **********************************************************
 * Copyright 2012 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ******************************************************************************
 * Worlds                                                                */ /**
 * 
 * \addtogroup Core
 * @{
 * \addtogroup Worlds
 * @{
 * \defgroup Deprecated Deprecated APIs
 * @{
 *
 ******************************************************************************
 */


#ifndef _VMKAPI_WORLDS_DEPRECATED_H_
#define _VMKAPI_WORLDS_DEPRECATED_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */


/*
 ***********************************************************************
 * vmk_WorldWaitIRQ --                                            */ /**
 *
 * \brief Deschedule a World holding an IRQ Lock until awakened or until
 *        the specified timeout expires.
 *
 * \deprecated
 *
 * \note  This function may block.
 *
 * \note Spurious wakeups are possible
 *
 * \note For worlds holding non-IRQ locks, use vmk_WorldWait().
 *
 * \param[in]  eventId     System wide unique identifier of the event
 *                         to sleep on.
 * \param[in]  lock        Lock of type VMK_SPINLOCK or VMK_SPINLOCK_IRQ
 *                         to release before descheduling the world.
 *                         VMK_LOCK_INVALID indicates that no lock needs
 *                         to be released before descheduling the world.
 * \param[in]  irql        IRQ level of lock provided by
 *                         vmk_SpinlockLockIRQ().
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

VMK_ReturnStatus vmk_WorldWaitIRQ(
   vmk_WorldEventID eventId,
   vmk_Lock lock,
   vmk_LockIRQL irql,
   vmk_uint32 timeoutMS,
   const char *reason);


#endif
/** @} */
/** @} */
/** @} */
