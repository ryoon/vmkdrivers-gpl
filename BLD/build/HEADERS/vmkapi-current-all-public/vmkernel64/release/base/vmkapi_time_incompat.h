/* **********************************************************
 * Copyright 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Time                                                           */ /**
 *                                                                       
 * \addtogroup Time
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_TIME_INCOMPAT_H_
#define _VMKAPI_TIME_INCOMPAT_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#define VMK_USECS_PER_JIFFY      10000
#define VMK_JIFFIES_PER_SECOND   (VMK_USEC_PER_SEC/VMK_USECS_PER_JIFFY)

/*
 ***********************************************************************
 * vmk_jiffies --                                                */ /**
 *
 * \brief A global that increments every VMK_USECS_PER_JIFFY
 *        microsceonds.
 *
 ***********************************************************************
 */
extern volatile unsigned long vmk_jiffies;


/*
 ***********************************************************************
 * vmk_TimerAdd --                                          */ /**
 *
 * \brief Schedule a timer.
 *
 * The VMKernel can schedule simultaneously a limited number of timers
 * for each CPU.
 *
 * \warning Timers are a limited resource.  The VMKernel does not
 *          guarantee to provide more than 100 concurrent timers per CPU
 *          system-wide, and exceeding the limit is a fatal error.
 *
 * \param[in]  callback     Timer callback.
 * \param[in]  data         Argument passed to the timer callback on
 *                          timeout.
 * \param[in]  timeoutUs    Timeout in microseconds.
 * \param[in]  periodic     Whether the timer should automatically
 *                          reschedule itself.
 * \param[in]  rank         Major rank of the timer; see explanation of
 *                          lock and timer ranks in vmkapi_lock.h.
 * \param[out] timer        Timer reference.
 * 
 * \retval VMK_NO_RESOURCES Couldn't schedule the timer.
 * \retval VMK_OK           The timer was successfully scheduled.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_TimerAdd(
   vmk_TimerCallback callback,
   vmk_TimerCookie data,
   vmk_int32 timeoutUs,
   vmk_Bool periodic,
   vmk_SpinlockRank rank,
   vmk_Timer *timer);


/*
 ***********************************************************************
 * vmk_TimerModifyOrAdd --                                  */ /**
 *
 * \brief Schedule or reschedule a timer.
 *
 * Atomically remove the timer referenced by *timer (if pending) and
 * reschedule it with the given new parameters, possibly replacing
 * *timer with a new timer reference.  It is permissible for *timer to
 * be VMK_INVALID_TIMER on input; in this case a new timer reference is
 * always returned.  This function is slower than vmk_TimerAdd and
 * should be used only if the atomic replacement semantics are needed.
 *
 * \param[in]     callback   Timer callback
 * \param[in]     data       Argument passed to the timer callback on
 *                           timeout.
 * \param[in]     timeoutUs  Timeout in microseconds
 * \param[in]     periodic   Whether the timer should automatically
 *                           reschedule itself
 * \param[in]     rank       Major rank of the timer; see explanation of
 *                           lock and timer ranks in vmkapi_lock.h.
 * \param[in,out] timer      Timer reference
 * \param[out]    pending    Whether the timer was still pending when
 *                           modified.
 * 
 * \retval VMK_NO_RESOURCES  Couldn't schedule the timer.
 * \retval VMK_OK            The timer was successfully scheduled.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_TimerModifyOrAdd(
   vmk_TimerCallback callback,
   vmk_TimerCookie data,
   vmk_int32 timeoutUs,
   vmk_Bool periodic,
   vmk_SpinlockRank rank,
   vmk_Timer *timer,
   vmk_Bool *pending);


/*
 ***********************************************************************
 * vmk_TimerRemove --                                             */ /**
 *
 * \ingroup Time
 * \brief Cancel a scheduled timer.
 *
 * \param[in]  timer     A timer reference.
 *
 * \retval VMK_OK        The timer was successfully cancelled. If the
 *                       timer was one-shot, it did not fire and never
 *                       will.
 * \retval VMK_NOT_FOUND The timer had previously been removed, was a
 *                       one-shot that already fired, or is
 *                       VMK_INVALID_TIMER.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_TimerRemove(
   vmk_Timer timer);


/*
 ***********************************************************************
 * vmk_TimerRemoveSync --                                         */ /**
 *
 * \ingroup Time
 * \brief Cancel a scheduled timer
 *
 * If the timer fired before it could be cancelled, spin until the timer
 * callback completes.
 *
 * \warning This function must not be called from the timer callback
 *          itself.  It must be called with current lock rank less than
 *          the timer's rank; see an explanation of lock and timer ranks
 *          in vmkapi_lock.h.
 *
 * \param[in]  timer    A timer reference.
 *
 * \retval VMK_OK          The timer was successfully cancelled. If the
 *                         timer was one-shot, it did not fire and
 *                         never will.
 * \retval VMK_NOT_FOUND   The timer had previously been removed, was a
 *                         one-shot that already fired, or is
 *                         VMK_INVALID_TIMER.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_TimerRemoveSync(
  vmk_Timer timer);


#endif /* _VMKAPI_TIME_INCOMPAT_H_ */
/** @} */
