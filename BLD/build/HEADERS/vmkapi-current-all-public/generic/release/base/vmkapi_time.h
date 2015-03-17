/* **********************************************************
 * Copyright 2007 - 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Time                                                           */ /**
 *
 * \addtogroup Core
 * @{
 * \defgroup Time Time and Timers
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_TIME_H_
#define _VMKAPI_TIME_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/** \brief Known invalid value for a timer */
#define VMK_INVALID_TIMER ((vmk_Timer)NULL)

/** \brief Known invalid value for a timer queue */
#define VMK_INVALID_TIMER_QUEUE ((vmk_TimerQueue)NULL)

typedef vmk_int64  vmk_TimerRelCycles;
typedef vmk_uint64 vmk_TimerCycles;

typedef vmk_AddrCookie vmk_TimerCookie;
typedef struct vmk_TimerInt *vmk_Timer;
typedef struct vmk_TimerQueueInt *vmk_TimerQueue;

/**
 * \brief Representation for Time
 */
typedef struct {
   vmk_int64 sec;                /* seconds */
   vmk_int64 usec;               /* microseconds */
} vmk_TimeVal;

/**
 * \brief Attributes dictating timer queue behavior
 */
typedef vmk_uint64 vmk_TimerQueueAttributes;

/** normal timer queue */
#define VMK_TIMER_QUEUE_ATTR_NONE (0)

/** low latency timer queue */
#define VMK_TIMER_QUEUE_ATTR_LOW_LATENCY (1 << 0)

/**
 * \brief Properties for a new timer queue
 */
typedef struct {
   vmk_Name name;
   vmk_ModuleID moduleID;
   vmk_HeapID heapID;
   vmk_TimerQueueAttributes attribs;
} vmk_TimerQueueProps;

/**
 * \brief Timer Attributes
 */
typedef vmk_uint64 vmk_TimerAttributes;

/** One shot timer */
#define VMK_TIMER_ATTR_NONE     (0)
/** Periodic timer */
#define VMK_TIMER_ATTR_PERIODIC (1 << 0)
/** Default tolerance value */
#define VMK_TIMER_DEFAULT_TOLERANCE  (-1)


/* Convenient time constants */
#define VMK_USEC_PER_SEC         VMK_CONST64(1000000)
#define VMK_MSEC_PER_SEC         VMK_CONST64(1000)
#define VMK_USEC_PER_MSEC        VMK_CONST64(1000)

/* Constants for specifying special timeouts. */

/**
 * \brief Constant for specifying a nonblocking timeout.
 */
#define VMK_TIMEOUT_NONBLOCKING (0)

/**
 * \brief Constant for specifying an unlimited timeout.
 */
#define VMK_TIMEOUT_UNLIMITED_MS (VMK_UINT32_MAX)


/*
 ***********************************************************************
 * vmk_TimerCallback --                                           */ /**
 *
 * \brief Callback function invoked when timer fires.
 *
 * \note This callback is allowed to block; however, ready timers on
 *       the same vmk_TimerQueue will not fire until the firing timer
 *       has finished, so blocking in this callback may delay other
 *       scheduled timers.
 *
 * \param[in] data  vmk_TimerCookie provided when timer scheduled.
 *
 ***********************************************************************
 */
typedef void (*vmk_TimerCallback)(vmk_TimerCookie data);


/*
 ***********************************************************************
 * vmk_GetTimerCycles --                                          */ /**
 *
 * \brief Return the time elapsed since the VMKernel was loaded.
 *
 * The time is obtained from a clock that is fast to read (typically
 * under 100 CPU cycles) but that may not be exactly synchronized
 * across different physical CPUs.  Therefore, time may appear to go
 * backward slightly (typically less than 100 usec) if you compare
 * readings taken close together in time on different PCPUs.  Most
 * applications can deal with this behavior by always treating the
 * difference between two times as a signed quantity and (if needed)
 * treating a negative difference as zero.  See also vmk_GetUptime.
 *
 * \note  This function will not block.
 *
 ***********************************************************************
 */
vmk_TimerCycles vmk_GetTimerCycles(void);

/*
 ***********************************************************************
 * vmk_TimerCyclesPerSecond --                                    */ /**
 *
 * \brief Return the frequency in Hz of the vmk_GetTimerCycles() clock.
 *
 * \note  This function will not block.
 *
 ***********************************************************************
 */
vmk_uint64 vmk_TimerCyclesPerSecond(void);

/*
 ***********************************************************************
 * vmk_TimerUSToTC --                                             */ /**
 *
 * \brief Convert signed nanoseconds into signed timer cycles.
 *
 * \note  This function will not block.
 *
 ***********************************************************************
 */
vmk_TimerRelCycles vmk_TimerNSToTC(
   vmk_int64 ns);

/*
 ***********************************************************************
 * vmk_TimerTCToNS --                                             */ /**
 *
 * \brief Convert signed timer cycles into signed nanoseconds.
 *
 * \note  This function will not block.
 *
 ***********************************************************************
 */
vmk_int64 vmk_TimerTCToNS(
   vmk_TimerRelCycles cycles);

/*
 ***********************************************************************
 * vmk_TimerUSToTC --                                             */ /**
 *
 * \brief Convert signed microseconds into signed timer cycles.
 *
 * \note  This function will not block.
 *
 ***********************************************************************
 */
vmk_TimerRelCycles vmk_TimerUSToTC(
   vmk_int64 us);

/*
 ***********************************************************************
 * vmk_TimerTCToUS --                                             */ /**
 *
 * \brief Convert signed timer cycles into signed microseconds.
 *
 * \note  This function will not block.
 *
 ***********************************************************************
 */
vmk_int64 vmk_TimerTCToUS(
   vmk_TimerRelCycles cycles);

/*
 ***********************************************************************
 * vmk_TimerMSToTC --                                             */ /**
 *
 * \brief Convert signed milliseconds into signed timer cycles.
 *
 * \note  This function will not block.
 *
 ***********************************************************************
 */
vmk_TimerRelCycles vmk_TimerMSToTC(
   vmk_int64 ms);

/*
 ***********************************************************************
 * vmk_TimerTCToMS --                                             */ /**
 *
 * \brief Convert signed timer cycles into signed milliseconds.
 *
 * \note  This function will not block.
 *
 ***********************************************************************
 */
vmk_int64 vmk_TimerTCToMS(
   vmk_TimerRelCycles cycles);

/*
 ***********************************************************************
 * vmk_TimerUnsignedNSToTC --                                     */ /**
 *
 * \brief Convert unsigned nanoseconds into unsigned timer cycles.
 *
 * \note  This function will not block.
 *
 ***********************************************************************
 */
vmk_TimerCycles vmk_TimerUnsignedNSToTC(
   vmk_uint64 ns);

/*
 ***********************************************************************
 * vmk_TimerUnsignedTCToNS --                                     */ /**
 *
 * \brief Convert unsigned timer cycles into unsigned nanoseconds.
 *
 * \note  This function will not block.
 *
 ***********************************************************************
 */
vmk_uint64 vmk_TimerUnsignedTCToNS(
   vmk_TimerCycles cycles);

/*
 ***********************************************************************
 * vmk_TimerUnsignedUSToTC --                                     */ /**
 *
 * \brief Convert unsigned microseconds into unsigned timer cycles.
 *
 * \note  This function will not block.
 *
 ***********************************************************************
 */
vmk_TimerCycles vmk_TimerUnsignedUSToTC(
   vmk_uint64 us);

/*
 ***********************************************************************
 * vmk_TimerUnsignedTCToUS --                                     */ /**
 *
 * \brief Convert unsigned timer cycles into unsigned microseconds.
 *
 * \note  This function will not block.
 *
 ***********************************************************************
 */
vmk_uint64 vmk_TimerUnsignedTCToUS(
   vmk_TimerCycles cycles);

/*
 ***********************************************************************
 * vmk_TimerUnsignedMSToTC --                                     */ /**
 *
 * \brief Convert unsigned millieconds into unsigned timer cycles.
 *
 * \note  This function will not block.
 *
 ***********************************************************************
 */
vmk_TimerCycles vmk_TimerUnsignedMSToTC(
   vmk_uint64 ms);

/*
 ***********************************************************************
 * vmk_TimerUnsignedTCToMS --                                     */ /**
 *
 * \brief Convert unsigned timer cycles into unsigned milliseconds.
 *
 * \note  This function will not block.
 *
 ***********************************************************************
 */
vmk_uint64 vmk_TimerUnsignedTCToMS(
   vmk_TimerCycles cycles);

/*
 ***********************************************************************
 * VMK_ABS_TIMEOUT_MS --                                          */ /**
 *
 * \brief Convert a delay in milliseconds into an absolute timeout in
 *        milliseconds.
 *
 * \param[in] delay_ms  Millisecond delay to convert to absolute time.
 *
 * \return If delay_ms is nonzero, returns the absolute time in
 *         milliseconds that is delay_ms from the time of the call.  If
 *         delay_ms is 0, returns 0, avoiding a call to vmk_GetTimerCycles.
 *
 ***********************************************************************
 */
#define VMK_ABS_TIMEOUT_MS(delay_ms) \
   ((delay_ms) ? vmk_TimerUnsignedTCToMS(vmk_GetTimerCycles()) + (delay_ms) : 0)

/*
 ***********************************************************************
 * vmk_GetTimeOfDay --                                            */ /**
 *
 * \brief Get the time in vmk_TimeVal representation.
 *
 * \note  This function will not block.
 *
 ***********************************************************************
 */
void vmk_GetTimeOfDay(
   vmk_TimeVal *tv);

/*
 ***********************************************************************
 * vmk_GetUptime --                                               */ /**
 *
 * \brief Get the uptime in vmk_TimeVal representation.
 *
 * vmk_GetUptime returns the system uptime as measured by a
 * rate-corrected clock.  The zero point is the same as that of
 * vmk_GetTimerCycles, but a rate correction computed by the system's
 * clock synchronization daemon is applied to measure the uptime more
 * accurately.  Note that this implies vmk_GetUptime may run slightly
 * faster or slower than vmk_GetTimerCycles.  Unlike
 * vmk_GetTimerCycles, vmk_GetUptime contains internal synchronization
 * that ensures it cannot appear to go backward even if you compare
 * readings taken on different physical CPUs.  Unlike
 * vmk_GetTimeOfDay, which is rate-corrected in the same way but also
 * can be reset from user space by the settimeofday system call,
 * vmk_GetUptime always moves forward smoothly.  The synchronization
 * and rate-correction make vmk_GetUptime slower to read than
 * vmk_GetTimerCycles.
 *
 * \note  This function will not block.
 *
 ***********************************************************************
 */
void vmk_GetUptime(
   vmk_TimeVal *tv);

/*
 ***********************************************************************
 * vmk_DelayUsecs --                                            */ /**
 *
 * \brief Spin-wait for a specified number of microseconds.
 *
 * \note  This function will not block.
 *
 ***********************************************************************
 */
void vmk_DelayUsecs(
   vmk_uint32 uSecs);


/*
 ***********************************************************************
 * vmk_TimerScheduleCustom --                                     */ /**
 *
 * \brief Schedule a timer
 *
 * The VMKernel can schedule simultaneously a limited number of timers
 * for each CPU.
 *
 * \note  This function will not block.
 *
 * \param[in]  moduleID    ID of the module to which callback belongs
 * \param[in]  queue       The timer queue to use for the timer
 * \param[in]  callback    Timer callback.
 * \param[in]  data        Argument passed to the timer callback on
 *                         timeout.
 * \param[in]  timeoutUS   Timeout in microseconds.
 * \param[in]  toleranceUS Tolerance in microseconds.  Indicates a
 *                         request that the timer fire approximately
 *                         no later than the provided value from the
 *                         requested time.  The majority of callers
 *                         should specify a tolerance of
 *                         VMK_TIMER_DEFAULT_TOLERANCE and only use
 *                         other values for timers that have strict
 *                         timing requirements.
 * \param[in]  attributes  Additional timer attributes
 * \param[in]  lockDomain  Lock domain in which to check the lock rank.
 *                         Note that if lockDomain is set to invalid
 *                         then rank has to be set to unranked and if
 *                         lockDomain is set to a valid value then
 *                         rank has to contain a valid rank value.
 * \param[in]  lockRank    Rank of the timer
 * \param[out] wasPending  If rescheduling a timer, was it previously
 *                         pending? (optional)
 * \param[out] timer       Timer Handle
 *
 * \retval VMK_NO_RESOURCES Couldn't schedule the timer.
 * \retval VMK_OK           The timer was successfully scheduled.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_TimerScheduleCustom(
   vmk_ModuleID moduleID,
   vmk_TimerQueue queue,
   vmk_TimerCallback callback,
   vmk_TimerCookie data,
   vmk_int64 timeoutUS,
   vmk_int64 toleranceUS,
   vmk_TimerAttributes attributes,
   vmk_LockDomainID lockDomain,
   vmk_LockRank lockRank,
   vmk_Bool *wasPending,
   vmk_Timer *timer);


/*
 ***********************************************************************
 * vmk_TimerSchedule --                                           */ /**
 *
 * \brief Schedule a timer
 *
 * The VMKernel can schedule simultaneously a limited number of timers
 * for each CPU.
 *
 * \note  This function will not block.
 *
 * \param[in]  queue       The timer queue to use for the timer
 * \param[in]  callback    Timer callback.
 * \param[in]  data        Argument passed to the timer callback on
 *                         timeout.
 * \param[in]  timeoutUS   Timeout in microseconds.
 * \param[in]  toleranceUS Tolerance in microseconds.  Indicates a
 *                         request that the timer fire approximately
 *                         no later than the provided value from the
 *                         requested time.  The majority of callers
 *                         should specify a tolerance of
 *                         VMK_TIMER_DEFAULT_TOLERANCE and only use
 *                         other values for timers that have strict
 *                         timing requirements.
 * \param[in]  attributes  Additional timer attributes
 * \param[in]  lockDomain  Lock domain in which to check the lock rank.
 *                         Note that if lockDomain is set to invalid
 *                         then rank has to be set to unranked and if
 *                         lockDomain is set to a valid value then
 *                         rank has to contain a valid rank value.
 * \param[in]  lockRank    Rank of the timer
 * \param[out] timer       Timer Handle
 *
 * \retval VMK_NO_RESOURCES Couldn't schedule the timer.
 * \retval VMK_OK           The timer was successfully scheduled.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_TimerSchedule(
   vmk_TimerQueue queue,
   vmk_TimerCallback callback,
   vmk_TimerCookie data,
   vmk_int64 timeoutUS,
   vmk_int64 toleranceUS,
   vmk_TimerAttributes attributes,
   vmk_LockDomainID lockDomain,
   vmk_LockRank lockRank,
   vmk_Timer *timer);


/*
 ***********************************************************************
 * vmk_TimerCancel --                                             */ /**
 *
 * \brief Cancel a scheduled timer
 *
 * If wait is set to TRUE and the timer is currently executing, waits
 * until the timer has finished executing before returning.  If wait is
 * set to FALSE, returns immediately after attempting to cancel the
 * timer, even if the timer is still executing.
 *
 * \note  This function will not block.
 *
 * \warning If wait is set to TRUE then this function must not be
 *          called from the timer callback itself. It must be called
 *          with current lock rank less than the timer's rank.
 *
 * \param[in]  timer    A timer reference.
 * \param[in]  wait     Determines whether to wait for a timer that is
 *                      currently executing.
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

VMK_ReturnStatus vmk_TimerCancel(
  vmk_Timer timer,
  vmk_Bool wait);


/*
 ***********************************************************************
 * vmk_TimerIsPending --                                            */ /**
 *
 * \brief Query a timer to see if it is pending.
 *
 * \note  This function will not block.
 *
 * \param[in]  timer  A timer reference.
 *
 * \retval VMK_TRUE   The timer is pending.  For one-shot timers, this
 *                    means the timer has neither fired nor been removed.
 *                    For periodic timers, it means the timer has not
 *                    been removed.
 * \retval VMK_FALSE  The timer is not pending.  For one-shot timers,
 *                    this means the timer has already fired, is in
 *                    the process of firing, or has been removed.  For
 *                    periodic timers, it means the timer has been
 *                    removed.  VMK_FALSE is also returned for
 *                    VMK_INVALID_TIMER.
 *
 ***********************************************************************
 */

vmk_Bool vmk_TimerIsPending(
   vmk_Timer timer);


/*
 ***********************************************************************
 * vmk_TimerQueueCreate --                                        */ /**
 *
 * \brief Create a timer queue.
 *
 * \note  This function may block.
 *
 * \param[in]  props    Properties of the new timer queue.
 *
 * \param[out] queue    The newly created queue.
 *
 * \retval VMK_OK             The timer queue was created successfuly,
 *                            and can now be used to schedule timers.
 *
 * \retval VMK_BAD_PARAM      Invalid parameters passed into the function.
 *                            Make sure that all fields in the provided
 *                            properties are valid.
 *
 * \retval VMK_NO_RESOURCES   Ran out of resources to create the timer
 *                            queue.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_TimerQueueCreate(
      vmk_TimerQueueProps *props,
      vmk_TimerQueue *queue);


/*
 ***********************************************************************
 * vmk_TimerQueueDestroy --                                       */ /**
 *
 * \brief Destroy a timer queue. All timers in the queue are cancelled.
 *        Memory for the timer queue and associate timers is released
 *        back to the heap that the queue was allocated from.
 *
 * \note  This function may block.
 *
 * \param[in]  queue    The queue.
 *
 * \retval VMK_OK       The timer queue is destroyed.
 *
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_TimerQueueDestroy(
      vmk_TimerQueue queue);


#endif /* _VMKAPI_TIME_H_ */
/** @} */
/** @} */
