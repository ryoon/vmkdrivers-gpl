/* **********************************************************
 * Copyright 2012 VMware, Inc.  All rights reserved.
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
 * \addtogroup SpinLocks
 * @{
 * \defgroup Deprecated Deprecated APIs
 * @{
 *
 ******************************************************************************
 */

#ifndef _VMKAPI_SPINLOCK_DEPRECATED_H_
#define _VMKAPI_SPINLOCK_DEPRECATED_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */


/** Invalid lock irql */
#define VMK_SPINLOCK_IRQL_INVALID   ((vmk_LockIRQL)-1)


/**
 * \brief Spinlock Types
 * @{
 */

/** Spinlock usable from a world and interrupt handler context */
#define VMK_SPINLOCK_IRQ   (2)

/** @} */


/**
 * \brief IRQ level of a VMK_SPINLOCK_IRQ lock.
 */
typedef vmk_uint32 vmk_LockIRQL;


/*
 ******************************************************************************
 * vmk_SpinlockLockIRQ --                                                */ /**
 *
 * \brief Acquire a spinlock of type VMK_SPINLOCK_IRQ
 *
 * \deprecated
 *
 * \param[in,out] lock  IRQ Spinlock to be acquired
 * \param[out]    irql  Previous IRQ level of spinlock
 *
 * \return VMK_OK on success, error code otherwise
 *
 * \note Lock checks are only executed when enabled for a given build. They
 *       are always enabled for debug builds.
 * \note A caller has to release the spinlock with a subsequent call to
 *       vmk_SpinlockUnlockIRQ, providing the value returned in irql.
 * \note Callers are required to minimize the time and code that is executed
 *       while any type of spinlock is held.
 * \note This function will not block.
 *
 * \nativedriversdisallowed
 *
 ******************************************************************************
 */

VMK_ReturnStatus
vmk_SpinlockLockIRQ(vmk_Lock lock,
                    vmk_LockIRQL *irql);


/*
 ******************************************************************************
 * vmk_SpinlockUnlockIRQ --                                              */ /**
 *
 * \brief Release a IRQ spinlock previously acquired via vmk_SpinlockLockIRQ
 *
 * \deprecated
 *
 * \param[in,out] lock  IRQ Spinlock to be released
 * \param[in]     irql  IRQ level of spinlock prior to locking
 *
 * \note Callers are required to release spinlocks in the reverse order in which
 *       they were acquired
 * \note This function will not block
 *
 * \nativedriversdisallowed
 *
 ******************************************************************************
 */

void
vmk_SpinlockUnlockIRQ(vmk_Lock lock,
                      vmk_LockIRQL irql);


/*
 ******************************************************************************
 * vmk_SpinlockUnlockIRQOutOfOrder --                                    */ /**
 *
 * \brief Out of order release of a IRQ spinlock previously acquired via
 *        vmk_SpinlockLockIRQ
 *
 * \deprecated
 *
 * \param[in,out] lock  IRQ Spinlock to be released
 * \param[in]     irql  IRQ level of spinlock prior to locking
 *
 * \note Callers are normally required to release spinlocks in the reverse
 *       order in which they were acquired.  This function allows for out
 *       of order releases but this should only be done when it is known to
 *       be safe.  This function should only be used for out of order releases;
 *       in all other cases vmk_SpinlockUnlockIRQ should be used.
 * \note This function will not block
 *
 * \nativedriversdisallowed
 *
 ******************************************************************************
 */

void
vmk_SpinlockUnlockIRQOutOfOrder(vmk_Lock lock,
                                vmk_LockIRQL irql);


/*
 ******************************************************************************
 * vmk_SpinlockAssertHeldOnPCPUInt --
 *
 * This is used by vmk_SpinlockAssertHeldOnPCPU().  VMKAPI clients should not
 * call this function directly.
 *
 * \deprecated
 *
 * \note This function will not block
 *
 * \nativedriversdisallowed
 *
 ******************************************************************************
 */

/** \cond nodoc */
void
vmk_SpinlockAssertHeldOnPCPUInt(vmk_Lock lock);
/** \endcond */


/*
 ******************************************************************************
 * vmk_SpinlockAssertHeldOnPCPU --                                       */ /**
 *
 * \brief Asserts that a lock is held on the current PCPU
 *
 * \deprecated
 *
 * \param[in]  lock  Lock to check
 *
 * \note Checks are only executed on debug builds.
 * \note This function should only be called with a lock of lock type
 *       VMK_SPINLOCK_IRQ
 * \note This function will not block
 *
 * \nativedriversdisallowed
 *
 ******************************************************************************
 */

static VMK_ALWAYS_INLINE void
vmk_SpinlockAssertHeldOnPCPU(
   vmk_Lock lock)
{
#ifdef VMX86_DEBUG
   vmk_SpinlockAssertHeldOnPCPUInt(lock);
#endif
}

#endif
/** @} */
/** @} */
/** @} */
