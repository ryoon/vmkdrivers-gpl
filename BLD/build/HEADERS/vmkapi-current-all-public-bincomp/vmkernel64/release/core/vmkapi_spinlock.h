/* **********************************************************
 * Copyright 2010 VMware, Inc.  All rights reserved.
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
 * Spinlock Types
 */
typedef enum vmk_SpinlockType {
   /** Spinlock usable from a world context */
   VMK_SPINLOCK         = 1,
   /** Spinlock usable from a world and interrupt handler context */
   VMK_SPINLOCK_IRQ     = 2,
   /** RW Spinlock usable from a world context */
   VMK_SPINLOCK_RW      = 3,
} vmk_SpinlockType;


/**
 * \brief Rank for locks
 */
typedef vmk_uint16 vmk_LockRank;


/**
 * Spinlock Creation Properties
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
 * Acquire a spinlock of type VMK_SPINLOCK
 *
 * \param[in,out] lock  Spinlock to be acquired
 *
 * \return VMK_OK on success, error code otherwise
 *
 * \note Lock checks are only executed when enabled for a given build. They
 *       are always enabled for debug builds.
 * \note A caller has to release the spinlock with a subsequent call to
 *       vmk_SpinlockUnlock
 * \note Callers are required to minimize the time and code that is executed
 *       while any type of spinlock is held.
 * \note This function will not block
 *
 ******************************************************************************
 */

VMK_ReturnStatus
vmk_SpinlockLock(vmk_Lock lock);


/*
 ******************************************************************************
 * vmk_SpinlockLockIRQ --                                                */ /**
 *
 * Acquire a spinlock of type VMK_SPINLOCK_IRQ
 *
 * \param[in,out] lock  IRQ Spinlock to be acquired
 *
 * \return VMK_OK on success, error code otherwise
 *
 * \note Lock checks are only executed when enabled for a given build. They
 *       are always enabled for debug builds.
 * \note A caller has to release the spinlock with a subsequent call to
 *       vmk_SpinlockUnlockIRQ
 * \note Callers are required to minimize the time and code that is executed
 *       while any type of spinlock is held.
 * \note This function will not block
 *
 ******************************************************************************
 */

VMK_ReturnStatus
vmk_SpinlockLockIRQ(vmk_Lock lock);


/*
 ******************************************************************************
 * vmk_SpinlockReadLock --                                               */ /**
 *
 * Acquire a spinlock of type VMK_SPINLOCK_RW for reading
 *
 * \param[in,out] lock  R/W Spinlock to be acquired
 *
 * \return VMK_OK on success, error code otherwise
 *
 * \note Lock checks are only executed when enabled for a given build. They
 *       are always enabled for debug builds.
 * \note A caller has to release the spinlock with a subsequent call to
 *       vmk_SpinlockReadUnlock
 * \note Callers are required to minimize the time and code that is executed
 *       while any type of spinlock is held.
 * \note This function will not block
 *
 ******************************************************************************
 */

VMK_ReturnStatus
vmk_SpinlockReadLock(vmk_Lock lock);


/*
 ******************************************************************************
 * vmk_SpinlockWriteLock --                                              */ /**
 *
 * Acquire a spinlock of type VMK_SPINLOCK_RW for an exclusive write operation
 *
 * \param[in,out] lock  R/W Spinlock to be acquired
 *
 * \return VMK_OK on success, error code otherwise
 *
 * \note Lock checks are only executed when enabled for a given build. They
 *       are always enabled for debug builds.
 * \note A caller has to release the spinlock with a subsequent call to
 *       vmk_SpinlockUnlock
 * \note Callers are required to minimize the time and code that is executed
 *       while any type of spinlock is held.
 * \note This function will not block
 *
 ******************************************************************************
 */

VMK_ReturnStatus
vmk_SpinlockWriteLock(vmk_Lock lock);


/*
 ******************************************************************************
 * vmk_SpinlockUnlock --                                                 */ /**
 *
 * Release a spinlock previously acquired via vmk_SpinlockLock
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
 * vmk_SpinlockUnlockIRQ --                                              */ /**
 *
 * Release a IRQ spinlock previously acquired via vmk_SpinlockLockIRQ
 *
 * \param[in,out] lock  IRQ Spinlock to be released
 *
 * \note Callers are required to release spinlocks in the reverse order in which
 *       they were acquired
 * \note This function will not block
 *
 ******************************************************************************
 */

void
vmk_SpinlockUnlockIRQ(vmk_Lock lock);


/*
 ******************************************************************************
 * vmk_SpinlockReadUnlock --                                             */ /**
 *
 * Release a R/W spinlock previously acquired via vmk_SpinlockReadLock
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
 * vmk_SpinlockWriteUnlock --                                            */ /**
 *
 * Release a R/W spinlock previously acquired via vmk_SpinlockWriteLock
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
 * vmk_SpinlockAssertHeldOnPCPUInt --
 *
 * This is used by vmk_SpinlockAssertHeldOnPCPU().  VMKAPI clients should not
 * call this function directly.
 *
 * \note This function will not block
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
 * \param[in]  lock  Lock to check
 *
 * \note Checks are only executed on debug builds.
 * \note This function will not block
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
