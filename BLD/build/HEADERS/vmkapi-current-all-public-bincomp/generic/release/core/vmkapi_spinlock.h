/* **********************************************************
 * Copyright 2010-2012 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ******************************************************************************
 * Spin Locks                                                            */ /**
 *
 * \addtogroup Core
 * @{
 * \defgroup SpinLocks Spin Locks
 *
 * \par Lock acquisition behavior for lock types VMK_SPINLOCK and
 *      VMK_SPINLOCK_RW:\n
 * In the case of lock contention, locks of types VMK_SPINLOCK and
 * VMK_SPINLOCK_RW will spin for a short amount of time. If the lock
 * acquisition does not succeed during the spinning phase, the lock acquisition
 * function will deschedule the calling context.  This is referred to as
 * involuntary descheduling and is different from blocking.
 *
 * \note This distinction between involuntary descheduling and blocking should
 *       be noted.  Throughout vmkapi documentation functions will indicate
 *       that they will not block, will block, or may involuntarily deschedule
 *       the calling context.  Documentation of vmkapi callbacks will also
 *       indicate which of these functions are allowed to be called.
 *       A callback that is allowed to block will always be allowed to call
 *       functions that involuntarily deschedule the calling context, however
 *       a callback that is allowed to call functions that involuntarily
 *       deschedule the calling context will not be allowed to call functions
 *       that block unless that is explicitly stated.
 *
 * @{
 *
 ******************************************************************************
 */

#ifndef _VMKAPI_SPINLOCKS_H_
#define _VMKAPI_SPINLOCKS_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */


/** Invalid lock handle */
#define VMK_LOCK_INVALID  ((vmk_Lock)-1)

/** Invalid lock rank */
#define VMK_SPINLOCK_RANK_INVALID   (0)

/** Lock rank for unranked locks */
#define VMK_SPINLOCK_UNRANKED   (0xFFFF)


/**
 * \brief Lock Handle
 */
typedef struct vmk_LockInt *vmk_Lock;


/**
 * \brief Spinlock Types
 * @{
 */
typedef vmk_uint8 vmk_SpinlockType;

/** Spinlock usable from a world context */
#define VMK_SPINLOCK    (1)
/** RW Spinlock usable from a world context */
#define VMK_SPINLOCK_RW (3)

/** @} */


/**
 * \brief Rank for locks
 */
typedef vmk_uint16 vmk_LockRank;


/**
 * \brief Spinlock Creation Properties
 */
typedef struct vmk_SpinlockCreateProps {
   /** Module ID for which the spinlock is created */
   vmk_ModuleID     moduleID;
   /** Heap from which the spinlock will be allocated */
   vmk_HeapID       heapID;
   /** Name of the spinlock */
   vmk_Name         name;
   /** Type of the lock */
   vmk_SpinlockType type;
   /**
    * Domain in which the spinlock will be rank checked in. If domain is set to
    * invalid then the rank also has to be set to invalid and vice versa.
    */
   vmk_LockDomainID domain;
   /** Rank of the spinlock. May only be set if domain is set to a valid value. */
   vmk_LockRank rank;
} vmk_SpinlockCreateProps;


/*
 ******************************************************************************
 * vmk_SpinlockCreate --                                                 */ /**
 *
 * \brief Allocate and initialize a spinlock
 *
 * Creates a new spinlock initializes it.
 *
 * \param[in]  props    Pointer to a valid vmk_SpinlockCreateProps struct
 * \param[out] lock     Pointer to the initialized spinlock
 *
 * \retval VMK_OK on success, error code otherwise
 *
 * \note Spinlocks can only be created from a world context
 * \note This function will not block
 *
 ******************************************************************************
 */

VMK_ReturnStatus
vmk_SpinlockCreate(vmk_SpinlockCreateProps *props,
                   vmk_Lock *lock);


/*
 ******************************************************************************
 * vmk_SpinlockDestroy --                                                */ /**
 *
 * \brief Destroy a spinlock previously created with vmk_SpinlockCreate
 *
 * Destroy the given lock and free the allocated memory
 *
 * \param[in] lock        Pointer to the spinlock
 *
 * \note This function will not block
 *
 ******************************************************************************
 */

void
vmk_SpinlockDestroy(vmk_Lock lock);


/*
 ******************************************************************************
 * vmk_SpinlockAllocSize                                                 */ /**
 *
 * \brief Size of an allocated spinlock of the specified type
 *
 * \result Allocation size
 *
 * \note This function will not block
 *
 ******************************************************************************
 */

vmk_ByteCountSmall
vmk_SpinlockAllocSize(vmk_SpinlockType type);


/*
 ******************************************************************************
 * vmk_SpinlockLock --                                                   */ /**
 *
 * \brief Acquire a spinlock of type VMK_SPINLOCK
 *
 * \param[in,out] lock  Spinlock to be acquired
 *
 * \note Lock checks are only executed when enabled for a given build. They
 *       are always enabled for debug builds.
 * \note A caller has to release the spinlock with a subsequent call to
 *       vmk_SpinlockUnlock.
 * \note Callers are required to minimize the time and code that is executed
 *       while any type of spinlock is held.
 * \note This function might involuntarily deschedule the calling context in
 *       the case of lock contention.
 * \note The lock acquisition might not succeed if the world receives a
 *       VMK_DEATH_PENDING signal while it is waiting for the lock.
 *
 * \retval VMK_OK            World acquired the lock.
 * \retval VMK_DEATH_PENDING World descheduled during the lock acquisition
 *                           and awoken because the world is dying and
 *                           being reaped by the scheduler. The caller
 *                           is expected to return as soon as possible.
 *                           The lock has not been acquired.
 *
 ******************************************************************************
 */

VMK_ReturnStatus
vmk_SpinlockLock(vmk_Lock lock);


/*
 ******************************************************************************
 * vmk_SpinlockReadLock --                                               */ /**
 *
 * \brief Acquire a spinlock of type VMK_SPINLOCK_RW for reading
 *
 * \param[in,out] lock  R/W Spinlock to be acquired
 *
 * \note Lock checks are only executed when enabled for a given build. They
 *       are always enabled for debug builds.
 * \note A caller has to release the spinlock with a subsequent call to
 *       vmk_SpinlockReadUnlock
 * \note Callers are required to minimize the time and code that is executed
 *       while any type of spinlock is held.
 * \note This function might involuntarily deschedule the calling context in
 *       the case of lock contention.
 * \note The lock acquisition might not succeed if the world receives a
 *       VMK_DEATH_PENDING signal while it is waiting for the lock.
 *
 * \retval VMK_OK            World acquired the read lock.
 * \retval VMK_DEATH_PENDING World descheduled during the lock acquisition
 *                           and awoken because the world is dying and
 *                           being reaped by the scheduler. The caller
 *                           is expected to return as soon as possible.
 *                           The read lock has not been acquired.
 *
 ******************************************************************************
 */

VMK_ReturnStatus
vmk_SpinlockReadLock(vmk_Lock lock);


/*
 ******************************************************************************
 * vmk_SpinlockWriteLock --                                              */ /**
 *
 * \brief Acquire a spinlock of type VMK_SPINLOCK_RW for an exclusive write
 *        operation
 *
 * \param[in,out] lock  R/W Spinlock to be acquired
 *
 * \note Lock checks are only executed when enabled for a given build. They
 *       are always enabled for debug builds.
 * \note A caller has to release the spinlock with a subsequent call to
 *       vmk_SpinlockWriteUnlock
 * \note Callers are required to minimize the time and code that is executed
 *       while any type of spinlock is held.
 * \note This function might involuntarily deschedule the calling context in
 *       the case of lock contention.
 * \note The lock acquisition might not succeed if the world receives a
 *       VMK_DEATH_PENDING signal while it is waiting for the lock.
 *
 * \retval VMK_OK            World acquired the write lock.
 * \retval VMK_DEATH_PENDING World descheduled during the lock acquisition
 *                           and awoken because the world is dying and
 *                           being reaped by the scheduler. The caller
 *                           is expected to return as soon as possible.
 *                           The write lock has not been acquired.
 *
 ******************************************************************************
 */

VMK_ReturnStatus
vmk_SpinlockWriteLock(vmk_Lock lock);


/*
 ******************************************************************************
 * vmk_SpinlockLockIgnoreDeathPending --                                 */ /**
 *
 * \brief Acquire a spinlock of type VMK_SPINLOCK
 *
 * \param[in,out] lock  Spinlock to be acquired
 *
 * \note This functions should only be used by callers that must acquire a
 *       lock, for instance to perform clean up, even after a world has
 *       already received the VMK_DEATH_PENDING signal.
 * \note Lock checks are only executed when enabled for a given build. They
 *       are always enabled for debug builds.
 * \note A caller has to release the spinlock with a subsequent call to
 *       vmk_SpinlockUnlock
 * \note Callers are required to minimize the time and code that is executed
 *       while any type of spinlock is held.
 * \note This function might involuntarily deschedule the calling context in
 *       the case of lock contention.
 *
 ******************************************************************************
 */

void
vmk_SpinlockLockIgnoreDeathPending(vmk_Lock lock);


/*
 ******************************************************************************
 * vmk_SpinlockReadLockIgnoreDeathPending --                             */ /**
 *
 * \brief Acquire a spinlock of type VMK_SPINLOCK_RW for reading
 *
 * \param[in,out] lock  R/W Spinlock to be acquired
 *
 * \note This functions should only be used by callers that must acquire a
 *       lock, for instance to perform clean up, even after a world has
 *       already received the VMK_DEATH_PENDING signal.
 * \note Lock checks are only executed when enabled for a given build. They
 *       are always enabled for debug builds.
 * \note A caller has to release the spinlock with a subsequent call to
 *       vmk_SpinlockReadUnlock
 * \note Callers are required to minimize the time and code that is executed
 *       while any type of spinlock is held.
 * \note This function might involuntarily deschedule the calling context in
 *       the case of lock contention.
 *
 ******************************************************************************
 */

void
vmk_SpinlockReadLockIgnoreDeathPending(vmk_Lock lock);


/*
 ******************************************************************************
 * vmk_SpinlockWriteLockIgnoreDeathPending --                            */ /**
 *
 * \brief Acquire a spinlock of type VMK_SPINLOCK_RW for an exclusive write
 *        operation
 *
 * \param[in,out] lock  R/W Spinlock to be acquired
 *
 * \note This functions should only be used by callers that must acquire a
 *       lock, for instance to perform clean up, even after a world has
 *       already received the VMK_DEATH_PENDING signal.
 * \note Lock checks are only executed when enabled for a given build. They
 *       are always enabled for debug builds.
 * \note A caller has to release the spinlock with a subsequent call to
 *       vmk_SpinlockWriteUnlock
 * \note Callers are required to minimize the time and code that is executed
 *       while any type of spinlock is held.
 * \note This function might involuntarily deschedule the calling context in
 *       the case of lock contention.
 *
 ******************************************************************************
 */

void
vmk_SpinlockWriteLockIgnoreDeathPending(vmk_Lock lock);


/*
 ******************************************************************************
 * vmk_SpinlockUnlock --                                                 */ /**
 *
 * \brief Release a spinlock previously acquired via vmk_SpinlockLock
 *
 * \param[in,out] lock  Spinlock to be released
 *
 * \note Callers are required to release spinlocks in the reverse order in which
 *       they were acquired
 * \note This function will not block
 *
 ******************************************************************************
 */

void
vmk_SpinlockUnlock(vmk_Lock lock);


/*
 ******************************************************************************
 * vmk_SpinlockUnlockOutOfOrder --                                       */ /**
 *
 * \brief Out of order release of a spinlock previously acquired via
 *        vmk_SpinlockLock
 *
 * \param[in,out] lock  Spinlock to be released
 *
 * \note Callers are normally required to release spinlocks in the reverse
 *       order in which they were acquired.  This function allows for out
 *       of order releases but this should only be done when it is known to
 *       be safe.  This function should only be used for out of order releases;
 *       in all other cases vmk_SpinlockUnlock should be used.
 * \note This function will not block
 *
 ******************************************************************************
 */

void
vmk_SpinlockUnlockOutOfOrder(vmk_Lock lock);


/*
 ******************************************************************************
 * vmk_SpinlockReadUnlock --                                             */ /**
 *
 * \brief Release a R/W spinlock previously acquired via vmk_SpinlockReadLock
 *
 * \param[in,out] lock  Spinlock to be released
 *
 * \note Callers are required to release spinlocks in the reverse order in which
 *       they were acquired
 * \note This function will not block
 *
 ******************************************************************************
 */

void
vmk_SpinlockReadUnlock(vmk_Lock lock);


/*
 ******************************************************************************
 * vmk_SpinlockReadUnlockOutOfOrder --                                   */ /**
 *
 * \brief Out of order release of a R/W spinlock previously acquired via
 *        vmk_SpinlockReadLock
 *
 * \param[in,out] lock  Spinlock to be released
 *
 * \note Callers are normally required to release spinlocks in the reverse
 *       order in which they were acquired.  This function allows for out
 *       of order releases but this should only be done when it is known to
 *       be safe.  This function should only be used for out of order releases;
 *       in all other cases vmk_SpinlockReadUnlock should be used.
 * \note This function will not block
 *
 ******************************************************************************
 */

void
vmk_SpinlockReadUnlockOutOfOrder(vmk_Lock lock);


/*
 ******************************************************************************
 * vmk_SpinlockWriteUnlock --                                            */ /**
 *
 * \brief Release a R/W spinlock previously acquired via vmk_SpinlockWriteLock
 *
 * \param[in,out] lock  Spinlock to be released
 *
 * \note Callers are required to release spinlocks in the reverse order in which
 *       they were acquired
 * \note This function will not block
 *
 ******************************************************************************
 */

void
vmk_SpinlockWriteUnlock(vmk_Lock lock);


/*
 ******************************************************************************
 * vmk_SpinlockWriteUnlockOutOfOrder --                                  */ /**
 *
 * \brief Out of order release of a R/W spinlock previously acquired via
 *        vmk_SpinlockWriteLock
 *
 * \param[in,out] lock  Spinlock to be released
 *
 * \note Callers are normally required to release spinlocks in the reverse
 *       order in which they were acquired.  This function allows for out
 *       of order releases but this should only be done when it is known to
 *       be safe.  This function should only be used for out of order releases;
 *       in all other cases vmk_SpinlockWriteUnlock should be used.
 * \note This function will not block
 *
 ******************************************************************************
 */

void
vmk_SpinlockWriteUnlockOutOfOrder(vmk_Lock lock);

/*
 ******************************************************************************
 * vmk_SpinlockAssertReaderHeldByWorldInt --
 *
 * This is used by vmk_SpinlockAssertHeldByWorld().  VMKAPI clients should not
 * call this function directly.
 *
 * \note This function will not block
 *
 ******************************************************************************
 */

/** \cond nodoc */
void
vmk_SpinlockAssertReaderHeldByWorldInt(vmk_Lock lock);
/** \endcond */

/*
 ******************************************************************************
 * vmk_SpinlockAssertReaderHeldByWorld --                                 */ /**
 *
 * \brief Asserts that a VMK_SPINLOCK_RW reader lock is held by the current world
 *
 * \param[in]  lock  VMK_SPINLOCK_RW reader lock to check
 *
 * \note Checks are only executed on debug builds.
 * \note This function should only be called with a lock of lock type
 *       VMK_SPINLOCK_RW which is holding a VMK_SPINLOCK_RW reader lock.
 * \note This function will not block
 *
 ******************************************************************************
 */

static VMK_ALWAYS_INLINE void
vmk_SpinlockAssertReaderHeldByWorld(
   vmk_Lock lock)
{
#ifdef VMX86_DEBUG
   vmk_SpinlockAssertReaderHeldByWorldInt(lock);
#endif
}

/*
 ******************************************************************************
 * vmk_SpinlockAssertHeldByWorldInt --
 *
 * This is used by vmk_SpinlockAssertHeldByWorld().  VMKAPI clients should not
 * call this function directly.
 *
 * \note This function will not block
 *
 ******************************************************************************
 */

/** \cond nodoc */
void
vmk_SpinlockAssertHeldByWorldInt(vmk_Lock lock);
/** \endcond */

/*
 ******************************************************************************
 * vmk_SpinlockAssertHeldByWorld --                                       */ /**
 *
 * \brief Asserts that a lock is held by the current world
 *
 * \param[in]  lock  Lock to check
 *
 * \note Checks are only executed on debug builds.
 * \note This function should only be called with a lock of lock type
 *       VMK_SPINLOCK or VMK_SPINLOCK_RW (RW locks that are writer locks only).
 *       For reader locks, use vmk_SpinlockAssertReaderHeldByWorld().
 * \note This function will not block
 *
 ******************************************************************************
 */

static VMK_ALWAYS_INLINE void
vmk_SpinlockAssertHeldByWorld(
   vmk_Lock lock)
{
#ifdef VMX86_DEBUG
   vmk_SpinlockAssertHeldByWorldInt(lock);
#endif
}

#endif
/** @} */
/** @} */
