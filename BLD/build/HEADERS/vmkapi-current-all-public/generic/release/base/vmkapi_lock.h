/* **********************************************************
 * Copyright 2007 - 2009 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Spinlocks                                                      */ /**
 * \defgroup Spinlocks Spin Locks
 *
 * \par Lock and Timer Ranks:
 * All locks are associated with a numeric major rank \em R and a
 * numeric  minor rank \em r. While holding lock \em L1 (ranks: \em R1,
 * \em r1) a lock \em L2 (ranks: \em R2, \em r2) can be acquired if one
 * of the following hold true:\n
 * - \em R2 > \em R1 \n
 * - \em R2 == \em R1, \em L2 and \em  L1 belong to the same class,
 *    and \em r2 > \em  r1.\n
 * \n
 * The only exception to the above rule is for "unranked locks", which
 * are applicable only for timers.  An unranked timer does not participate
 * in the ranking rules.  Standalone unranked spinlocks are not supported.\n
 * \n
 * Two types of spinlocks, with respect to lock ranks are supported:\n
 * \n
 * \li <b>Simple Ranked Spinlock:</b> \n
 *    A stand-alone spinlock which has an explicit major rank other 
 *    than VMK_SP_RANK_UNRANKED. The minor rank (which is implicitly set) 
 *    is always equal to '0'.
 * \li <b>Class Based Ranked Spinlock:</b>\n
 *    Belongs to a class of spinlocks all of which share the same 
 *    explicit major rank other than VMK_SP_RANK_UNRANKED. Members of 
 *    the same class differ only with respect to their minor rank, which is 
 *    explicitly set to a desired value, other than VMK_SP_RANK_UNRANKED, 
 *    during lock initialization.
 * 
 * \par Timers and Ranks:
 * Timers also have ranks within the lock ranking system.  While a
 * timer callback is firing, the callback is treated as holding a lock
 * of the timer's rank, so it is not allowed to acquire locks of lower
 * rank.  A call to vmk_TimerRemoveSync is treated as acquiring (and
 * releasing) a lock of the timer's rank, and thus is not allowed when
 * holding locks of higher or equal rank.\n
 * \n
 * Timers can only be unranked or simple, not class-based.  It is
 * common to give a timer the rank VMK_SP_RANK_LOWEST_MAJOR_LEGACY; this 
 * allows the callback to acquire any higher-ranked lock, but requires
 * that code calling vmk_TimerRemoveSync on the timer hold no locks.
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_SPLOCK_H_
#define _VMKAPI_SPLOCK_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/**
 * \brief Recursive flag
 *
 * Recursive flag indicates that an instace of lock \em L can be held while
 * acquiring another instance of lock \em L. It is assumed the caller knows
 * what they're doing.
 */
#define VMK_SP_RANK_RECURSIVE_FLAG_LEGACY  _VMK_SP_RANK_RECURSIVE_FLAG_LEGACY

/**
 * \brief Mask for major rank value.
 */
#define VMK_SP_RANK_NUMERIC_MASK_MAJOR_LEGACY  _VMK_SP_RANK_NUMERIC_MASK_MAJOR_LEGACY

/**
 * \brief Mask for minor rank value.
 */
#define VMK_SP_RANK_NUMERIC_MASK_MINOR_LEGACY _VMK_SP_RANK_NUMERIC_MASK_MINOR_LEGACY

/**
 * \brief Lowest major rank for locks
 */
#define VMK_SP_RANK_LOWEST_MAJOR_LEGACY  _VMK_SP_RANK_LOWEST_MAJOR_LEGACY

/**
 * \brief highest major rank for non-IRQ locks
 */
#define VMK_SP_RANK_HIGHEST_MAJOR_LEGACY  _VMK_SP_RANK_HIGHEST_MAJOR_LEGACY

/**
 * \brief Maximum minor rank for locks.
 */
#define VMK_SP_RANK_MAX_MINOR_LEGACY (_VMK_SP_RANK_NUMERIC_MASK_MINOR_LEGACY)

/**
 * \brief Minimum minor rank for locks.
 */
#define VMK_SP_RANK_MIN_MINOR_LEGACY (0)

/**
 * \brief Maximum major rank for locks
 */
#define VMK_SP_RANK_MAX_MAJOR_LEGACY  (VMK_SP_RANK_NUMERIC_MASK_MAJOR_LEGACY - 1)

/**
 * \brief Minimum major rank for locks
 */
#define VMK_SP_RANK_MIN_MAJOR_LEGACY  (0)

/**
 * \brief Leaf rank for IRQ locks
 *
 * To be used for IRQ locks that leafs, except for log/warning
 *
 */
#define VMK_SP_RANK_IRQ_LEAF_LEGACY  (0x5ff9)

/**
 * \brief Rank used by calls potentially invoked by timers that need
 *        to manage memory. For instance: an allocator callable by
 *        timers.
 */
#define VMK_SP_RANK_IRQ_MEMTIMER_LEGACY (0x5000)

/**
 * \brief Block rank for IRQ locks
 *
 * To be used for IRQ locks that depend on eventqueue/cpusched locks
 */
#define VMK_SP_RANK_IRQ_BLOCK_LEGACY  (0x4000)

/**
 * \brief Lowest major rank for IRQ locks
 */
#define VMK_SP_RANK_IRQ_LOWEST_MAJOR_LEGACY  _VMK_SP_RANK_IRQ_LOWEST_MAJOR_LEGACY

/**
 * \brief Highest major rank for IRQ locks.
 */
#define VMK_SP_RANK_IRQ_HIGHEST_MAJOR_LEGACY _VMK_SP_RANK_IRQ_HIGHEST_MAJOR_LEGACY

/**
 * \brief Leaf rank for locks
 *
 * Leaf locks are ranked lower than spin locks protecting semaphores,
 * so that one can grab a semaphore, grab a leaf lock and then call
 * vmk_SemaIsLocked() on the semaphore.
 */
#define VMK_SP_RANK_LEAF_LEGACY  (0x2ffd)

/**
 * \brief Rank for all SCSI locks
 *
 */
#define VMK_SP_RANK_SCSI_LEGACY _VMK_SP_RANK_SCSI_LEGACY

/**
 * \brief Rank for all SCSI plugin locks
 *
 */
#define VMK_SP_RANK_SCSI_PLUGIN2 _VMK_SP_RANK_SCSI_PLUGIN2
#define VMK_SP_RANK_SCSI_PLUGIN1 _VMK_SP_RANK_SCSI_PLUGIN1

/**
 * \brief Lowest rank for all SCSI locks
 *
 */
#define VMK_SP_RANK_SCSI_LOWEST_LEGACY _VMK_SP_RANK_SCSI_LOWEST_LEGACY

/**
 * \brief Lowest rank for locks in FS Device Switch component
 *
 * Lowest possible rank for locks used by FS Device Switch. Modules
 * operating above the device switch, and calling into the device switch
 * should use locks ranked lower than this.
 */
#define VMK_SP_RANK_FDS_LOWEST_LEGACY  _VMK_SP_RANK_FDS_LOWEST_LEGACY

/**
 * \brief Lowest rank for locks in VMK FS drivers
 *
 * Lowest possible rank for locks used by VMK FS drivers. FSS and everyone
 * above should use locks ranked lower than this.
 */
#define VMK_SP_RANK_FSDRIVER_LOWEST_LEGACY  _VMK_SP_RANK_FSDRIVER_LOWEST_LEGACY

/**
 * \brief Rank for Network locks
 *
 * Usual lock rank used for module internal usage which doesn't imply
 * vmkapi call.
 */
#define VMK_SP_RANK_NETWORK_LEGACY _VMK_SP_RANK_NETWORK_LEGACY

/**
 * \brief Lowest Rank for Network locks
 *
 * Should be used if the module is intended to hold a lock while calling 
 * a vmkapi function.
 */
#define VMK_SP_RANK_NETWORK_LOWEST_LEGACY _VMK_SP_RANK_NETWORK_LOWEST_LEGACY

/**
 * \brief Highest Rank for Network locks
 *
 * Should be used if the module is intended to hold a lock while calling 
 * a vmkapi function.
 */
#define VMK_SP_RANK_NETWORK_HIGHEST_LEGACY _VMK_SP_RANK_NETWORK_HIGHEST_LEGACY

/**
 * \brief Tcpip stack highest rank.
 *
 */
#define VMK_SP_RANK_TCPIP_HIGHEST_LEGACY _VMK_SP_RANK_TCPIP_HIGHEST_LEGACY

/**
 * \brief Unranked timers.
 */
#define VMK_SP_RANK_UNRANKED_TIMER_LEGACY VMK_SPINLOCK_UNRANKED

/**
 * \brief Handle for lock classes
 */
typedef struct vmk_SpinlockClass_LEGACYInt *vmk_SpinlockClass_LEGACY;

/**
 * \brief Handle for non-IRQ locks
 */
typedef struct vmk_SpinlockInt *vmk_Spinlock;

/**
 * \brief Handle for IRQ locks
 */
typedef struct vmk_SpinlockIRQInt *vmk_SpinlockIRQ;

/**
 * \brief Handle for non-IRQ rw locks
 */
typedef struct vmk_SpinlockRWInt *vmk_SpinlockRW;

/**
 * \brief Rank for locks
 */
typedef vmk_uint32 vmk_SpinlockRank;


/*
 ***********************************************************************
 * vmk_SPClassCreate_LEGACY --                                    */ /**
 *
 * \ingroup Spinlocks
 * \brief Allocate and initialize a spinlock class
 *
 * Creates a new spinlock class and initializes it.
 *
 * The spinlock class's rank serves as the major rank for the
 * spinlocks that are members of the class.
 *
 * \note Requires that the module heap be initialized.
 *
 * \param[out] lockClass   Pointer to the new spinlock class.
 * \param[in]  moduleID    Module ID who manages the created
 *                         spinlock class.
 * \param[in]  name        Name of the spinlock class.
 * \param[in]  rank        Rank of the spinlock class.
 *                         The specified rank must be greater than or
 *                         equal to VMK_SP_RANK_LOWEST_MAJOR_LEGACY
 *                         and less than or equal to
 *                         VMK_SP_RANK_HIGHEST_MAJOR_LEGACY.
 *
 * \retval VMK_OK The spinlock  class was successfully initialized
 * \retval VMK_NO_MEMORY The spinlock class could not be allocated
 * \retval VMK_NO_MODULE_HEAP The module has no heap to allocate from
 * \retval VMK_BAD_PARAM Rank was invalid.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SPClassCreate_LEGACY(
   vmk_SpinlockClass_LEGACY *lockClass,
   vmk_ModuleID moduleID,
   const char *name,
   vmk_SpinlockRank rank);

/*
 ***********************************************************************
 * vmk_SPClassDestroy_LEGACY --                                   */ /**
 *
 * \ingroup Spinlocks
 * \brief Destroy a spinlock class
 *
 * Revert all side effects of vmk_SPClassCreate_LEGACY
 *
 * \param[in] lockClass    Pointer to the spinlock class to be deleted.
 *
 ***********************************************************************
 */
void vmk_SPClassDestroy_LEGACY(vmk_SpinlockClass_LEGACY *lockClass);

/*
 ***********************************************************************
 * vmk_SPCreate_LEGACY --                                         */ /**
 *
 * \ingroup Spinlocks
 * \brief Allocate and initialize a non-IRQ spinlock
 *
 * Creates a new non-IRQ spinlock and initializes it.
 *
 * \note Requires that the module heap be initialized.
 *
 * \param[out] lock        Pointer to the new spinlock class.
 * \param[in]  moduleID    Module that owns the new spinlock.
 * \param[in]  name        Name of the spinlock,
 * \param[in]  lockClass   Pointer to a spinlock class, or NULL if the
 *                         new spinlock should be a simple (non-classed)
 *                         spinlock.
 * \param[in]  rank        The spinlock's rank.\n
 *                         If lockClass is NULL then this parameter
 *                         specifies the lock's major rank.
 *                         The specified rank must be greater than or
 *                         equal to VMK_SP_RANK_LOWEST_MAJOR_LEGACY and
 *                         less than or equal to 
 *                         VMK_SP_RANK_HIGHEST_MAJOR_LEGACY.\n
 *                         If lockClass is set to a valid spinlock
 *                         class then this parameter specifies the
 *                         new spinlock's minor rank with the lock's
 *                         major rank being inherited from the
 *                         spinlock class.
 *                         The specified rank must be greater than or
 *                         equal to VMK_SP_RANK_MIN and less than or
 *                         equal to VMK_SP_RANK_MAX.\n
 *
 * \retval VMK_OK The spinlock was successfully initialized
 * \retval VMK_NO_MEMORY The spinlock could not be allocated
 * \retval VMK_NO_MODULE_HEAP The module has no heap to allocate from
 * \retval VMK_BAD_PARAM Rank was invalid.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SPCreate_LEGACY(
   vmk_Spinlock *lock,
   vmk_ModuleID moduleID,
   const char *name,
   vmk_SpinlockClass_LEGACY *lockClass,
   vmk_SpinlockRank rank);

/*
 ***********************************************************************
 * vmk_SPAssertHasState --
 *
 * This is used by vmk_SPAssertIsLocked() and vmk_SPAssertIsUnlocked().
 * VMKAPI clients should not call this function directly.
 *
 ***********************************************************************
 */

/** \cond nodoc */
void vmk_SPAssertHasState(
   vmk_Spinlock *lock,
   vmk_Bool locked);
/** \endcond */

/*
 ***********************************************************************
 * vmk_SPAssertIsLocked --                                        */ /**
 *
 * \ingroup Spinlocks
 * \brief Assert that a spinlock is currently locked.
 *
 * \note This call only performs the check on a debug build.
 *       Other builds always succeed.
 * \note This call will not block.
 *
 * \param[in] lock   Non-IRQ spinlock to check
 *
 ***********************************************************************
 */

static VMK_ALWAYS_INLINE void
vmk_SPAssertIsLocked(
   vmk_Spinlock *lock)
{
#ifdef VMX86_DEBUG
   vmk_SPAssertHasState(lock, VMK_TRUE);
#endif
}


/*
 ***********************************************************************
 * vmk_SPAssertNotUnlocked --                                     */ /**
 *
 * \ingroup Spinlocks
 * \brief Assert that a spinlock is currently unlocked.
 *
 * \note This call only performs the check on a debug build.
 *       Other builds always succeed.
 * \note This call will not block.
 *
 * \param[in] lock   Non-IRQ spinlock to check
 *
 ***********************************************************************
 */

static VMK_ALWAYS_INLINE void
vmk_SPAssertIsUnlocked(
   vmk_Spinlock *lock)
{
#ifdef VMX86_DEBUG
   vmk_SPAssertHasState(lock, VMK_FALSE);
#endif
}


/*
 ***********************************************************************
 * vmk_SPAssertNoLocksHeldInt --                                  */ /**
 *
 * This is used by vmk_SPAssertNoLocksHeld().  VMKAPI clients should
 * not call this function directly.
 *
 ***********************************************************************
 */

/** \cond nodoc */
void
vmk_SPAssertNoLocksHeldInt(
   void);
/** \endcond */


/*
 ***********************************************************************
 * vmk_SPAssertNoLocksHeld --                                     */ /**
 *
 * \ingroup Spinlocks
 * \brief Assert that no non-IRQ spinlocks are held by this CPU.
 *
 * \note This call only performs the check on a debug build.
 *       Other builds always succeed.
 * \note This call will not block.
 *
 ***********************************************************************
 */

static VMK_ALWAYS_INLINE void
vmk_SPAssertNoLocksHeld(
   void)
{
#ifdef VMX86_DEBUG
   vmk_SPAssertNoLocksHeldInt();
#endif
}


/*
 ***********************************************************************
 * vmk_SPAssertNoLocksHeldIRQInt --
 *
 * This is used by vmk_SPAssertNoLocksHeldIRQ.  VMKAPI clients should not
 * call this function directly.
 *
 ***********************************************************************
 */

/** \cond nodoc */
void
vmk_SPAssertNoLocksHeldIRQInt(
   void);
/** \endcond */

/*
 ***********************************************************************
 * vmk_SPAssertNoLocksHeldIRQ --                                  */ /**
 *
 * \ingroup Spinlocks
 * \brief Assert that no IRQ spinlocks are held by this CPU.
 *
 * \note This call only performs the check on a debug build.
 *       Other builds always succeed.
 * \note This call will not block.
 *
 ***********************************************************************
 */

static VMK_ALWAYS_INLINE void
vmk_SPAssertNoLocksHeldIRQ(
   void) 
{
#ifdef VMX86_DEBUG
   vmk_SPAssertNoLocksHeldIRQInt();
#endif
}


/*
 ***********************************************************************
 * vmk_SPLock --                                                  */ /**
 *
 * \ingroup Spinlocks
 * \brief Acquire the non-IRQ spinlock
 *
 * \pre The caller shall not already hold a spinlock of equal or
 *      higher rank.
 *
 * \param[in,out] lock  Non-IRQ spinlock to acquire.
 *
 * \return None.
 *
 ***********************************************************************
 */
void vmk_SPLock(
   vmk_Spinlock *lock);

/*
 ***********************************************************************
 * vmk_SPTryLock --                                               */ /**
 *
 * \ingroup Spinlocks
 * \brief Try acquire a non-IRQ spinlock
 *
 * This function tries to acquire the given spinlock:
 * - If the given spinlock has not been locked, this function locks it.
 * - If the given spinlock has been locked already, this function does
 *   nothing and returns immediately.
 *
 * \pre This caller shall not already hold a spinlock of lower or
 *      equal rank.
 *
 * \param[in,out] lock   Non-IRQ spinlock to attempt to acquire.
 *
 * \retval VMK_OK    The given spinlock is successfully locked by this
 *                   call.
 * \retval VMK_BUSY  The given spinlock has been locked already.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SPTryLock(
   vmk_Spinlock *lock);

/*
 ***********************************************************************
 * vmk_SPUnlock --                                                */ /**
 *
 * \ingroup Spinlocks
 * \brief Release a non-IRQ spinlock
 *
 * \pre The caller currently owns the spinlock.
 *
 * \param[in,out] lock  Non-IRQ spinlock to release.
 *
 ***********************************************************************
 */
void vmk_SPUnlock(
   vmk_Spinlock *lock);

/*
 ***********************************************************************
 * vmk_SPDestroy --                                               */ /**
 *
 * \ingroup Spinlocks
 * \brief Destroy a non-IRQ spinlock
 *
 * Revert all side effects of vmk_SPCreate_LEGACY
 *
 * \param[in] lock   Non-IRQ spinlock to be destroyed.
 *
 ***********************************************************************
 */
void vmk_SPDestroy(vmk_Spinlock *lock);

/*
 ***********************************************************************
 * vmk_SPCreateIRQ_LEGACY --                                      */ /**
 *
 * \ingroup Spinlocks
 * \brief Allocate and initialize an IRQ spinlock
 *
 * Creates a new IRQ spinlock and initializes it.
 *
 * \note Requires that the module heap be initialized.
 *
 * \param[out] lock        Pointer to the new spinlock class.
 * \param[in]  moduleID    Module that owns the new spinlock.
 * \param[in]  name        Name of the spinlock,
 * \param[in]  lockClass   Pointer to a spinlock class, or NULL if the
 *                         new spinlock should be a simple (non-classed)
 *                         spinlock.
 * \param[in]  rank        The spinlock's rank.\n
 *                         If lockClass is NULL then this parameter
 *                         specifies the lock's major rank.
 *                         The specified rank must be greater than or
 *                         equal to VMK_SP_RANK_IRQ_LOWEST and less
 *                         than or equal to VMK_SP_RANK_IRQ_HIGHEST.\n
 *                         The specified rank must be greater than or
 *                         equal to VMK_SP_RANK_MIN_MAJOR_LEGACY and
 *                         less than or equal to 
 *                         VMK_SP_RANK_MAX_MAJOR_LEGACY.\n
 *                         If lockClass is set to a valid spinlock
 *                         class then this parameter specifies the
 *                         new spinlock's minor rank with the lock's
 *                         major rank being inherited from the
 *                         spinlock class.  Minor spin ranks must be
 *                         greater than or equal to
 *                         VMK_SP_RANK_MIN_MINOR_LEGACY and less than or
 *                         equal to VMK_SP_RANK_MAX_MINOR_LEGACY.
 *
 * \retval VMK_OK The spinlock was successfully initialized
 * \retval VMK_NO_MEMORY The spinlock could not be allocated
 * \retval VMK_NO_MODULE_HEAP The module has no heap to allocate from
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SPCreateIRQ_LEGACY(
   vmk_SpinlockIRQ *lock,
   vmk_ModuleID moduleID,
   const char *name,
   vmk_SpinlockClass_LEGACY *lockClass,
   vmk_SpinlockRank rank);

/*
 ***********************************************************************
 * vmk_SPAssertHasStateIRQ --
 *
 * This is use by vmk_SPAssertIsLockedIRQ and 
 * vmk_SPAssertIsUnlockedIRQ().  VMKAPI clients should not call this
 * function directly.
 *
 ***********************************************************************
 */

/** \cond nodoc */
void vmk_SPAssertHasStateIRQ(
   vmk_SpinlockIRQ *lock,
   vmk_Bool         locked);
/** \endcond */


/*
 ***********************************************************************
 * vmk_SPAssertIsLockedIRQ --                                     */ /**
 *
 * \ingroup Spinlocks
 * \brief Assert that an IRQ spinlock is currently locked.
 *
 * \note This call only performs the check on a debug build.
 * \note This call will not block.
 *
 * \param[in] lock   IRQ spinlock to check
 *
 ***********************************************************************
 */

static VMK_ALWAYS_INLINE void
vmk_SPAssertIsLockedIRQ(
   vmk_SpinlockIRQ *lock)
{
#ifdef VMX86_DEBUG
   vmk_SPAssertHasStateIRQ(lock, VMK_TRUE);
#endif
}


/*
 ***********************************************************************
 * vmk_SPAssertIsUnlockedIRQ --                                   */ /**
 *
 * \ingroup Spinlocks
 * \brief Assert that an IRQ spinlock is currently unlocked.
 *
 * \note This call only performs the check on a debug build.
 * \note This call will not block.
 *
 * \param[in] lock   IRQ spinlock to check
 *
 ***********************************************************************
 */

static VMK_ALWAYS_INLINE void
vmk_SPAssertIsUnlockedIRQ(
   vmk_SpinlockIRQ *lock)
{
#ifdef VMX86_DEBUG
   vmk_SPAssertHasStateIRQ(lock, VMK_FALSE);
#endif
}


/*
 ***********************************************************************
 * vmk_SPLockIRQ --                                               */ /**
 *
 * \ingroup Spinlocks
 * \brief Acquire the IRQ spinlock
 *
 * Acquires an IRQ spinlock and disable interrupts on the current CPU
 * if they are not already disabled.
 *
 * \pre The caller shall not already hold a spinlock of equal or
 *      higher rank.
 *
 * \param[in,out] lock  IRQ spinlock to acquire.
 *
 * \return Previous IRQ state.
 * \retval 0 interrupts were enabled.
 * \retval 1 interrupts were disabled.
 *
 ***********************************************************************
 */
unsigned long vmk_SPLockIRQ(
   vmk_SpinlockIRQ *lock);

/*
 ***********************************************************************
 * vmk_SPTryLockIRQ --                                            */ /**
 *
 * \ingroup Spinlocks
 * \brief Try acquire an IRQ spinlock
 *
 * This function tries to acquire the given spinlock:
 * - If the given IRQ spinlock has not been locked, this function locks it
 *   and disables interrupts on the current CPU if interrupts were not
 *   already disabled.
 * - If the given IRQ spinlock has been locked already, this function does
 *   nothing and returns immediately.
 *
 * \pre This caller shall not already hold a spinlock of lower or
 *      equal rank.
 *
 * \param[in,out] lock   IRQ spinlock to attempt to acquire.
 * \param[out]    flags  Previous IRQ level. 0 if interrupts were enabled.
 *                       1 if interrupts were disabled.
 *
 * \retval VMK_OK    The given spinlock is successfully locked by this
 *                   call.
 * \retval VMK_BUSY  The given spinlock has been locked already.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SPTryLockIRQ(
   vmk_SpinlockIRQ *lock,
   unsigned long *flags);

/*
 ***********************************************************************
 * vmk_SPUnlockIRQ --                                             */ /**
 *
 * \ingroup Spinlocks
 * \brief Release an IRQ spinlock
 *
 * \pre The caller currently owns the spinlock.
 *
 * \param[in,out] lock    IRQ spinlock to unlock.
 * \param[in]     prevIRQ IRQ level from returned from vmk_SPLockIRQ
 *                        or vmk_SPTryLockIRQ.
 *
 ***********************************************************************
 */
void vmk_SPUnlockIRQ(
   vmk_SpinlockIRQ *lock,
   unsigned long prevIRQ);

/*
 ***********************************************************************
 * vmk_SPDestroyIRQ --                                            */ /**
 *
 * \ingroup Spinlocks
 * \brief Destroy an IRQ spinlock
 *
 * Revert all side effects of vmk_SPCreateIRQ_LEGACY
 *
 * \param lock    IRQ spinlock to be detroyed.
 *
 ***********************************************************************
 */
void vmk_SPDestroyIRQ(vmk_SpinlockIRQ *lock);

/*
 ***********************************************************************
 * vmk_RWCreate_LEGACY --                                         */ /**
 *
 * \ingroup Spinlocks
 * \brief Allocate and initialize a reader/writer spinlock
 *
 * Creates a new reader/writer spinlock and initializes it as
 * ranked spinlock.
 *
 * \note Requires that the module heap be initialized.
 *
 * \param[out] lock        Pointer to the new spinlock class.
 * \param[in]  moduleID    Module that owns the new spinlock.
 * \param[in]  name        Name of the spinlock,
 * \param[in]  lockClass   Pointer to a spinlock class, or NULL if the
 *                         new spinlock should be a simple (non-classed)
 *                         spinlock.
 * \param[in]  rank        The spinlock's rank.\n
 *                         If lockClass is NULL then this parameter
 *                         specifies the lock's major rank.
 *                         The specified rank must be greater than or
 *                         equal to VMK_SP_RANK_LOWEST and less than or
 *                         equal to VMK_SP_RANK_HIGHEST.\n
 *                         If lockClass is set to a valid spinlock
 *                         class then this parameter specifies the
 *                         new spinlock's minor rank with the lock's
 *                         major rank being inherited from the
 *                         spinlock class.
 *                         The specified rank must be greater than or
 *                         equal to VMK_SP_RANK_MIN_MAJOR_LEGACY and 
 *                         less than or equal to 
 *                         VMK_SP_RANK_MAX_MAJOR_LEGACY.\n
 *
 * \retval VMK_OK The spinlock was successfully initialized
 * \retval VMK_NO_MEMORY The spinlock could not be allocated
 * \retval VMK_NO_MODULE_HEAP The module has no heap to allocate from
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RWCreate_LEGACY(
   vmk_SpinlockRW *lock,
   vmk_ModuleID moduleID,
   const char *name,
   vmk_SpinlockClass_LEGACY *lockClass,
   vmk_SpinlockRank rank);

/*
 ***********************************************************************
 * vmk_RWReadLock --                                              */ /**
 *
 * \ingroup Spinlocks
 * \brief Acquire a reader/writer spinlock for reading.
 *
 * \pre The caller shall not already hold a spinlock of lower or
 *      equal rank.
 *
 * \param[in,out] lock  Reader/writer spinlock to acquire.
 *
 ***********************************************************************
 */
void vmk_RWReadLock(
   vmk_SpinlockRW *lock);

/*
 ***********************************************************************
 * vmk_RWTryReadLock --                                           */ /**
 *
 * \ingroup Spinlocks
 * \brief Try acquire the reader/writer spinlock for reading
 *
 * This function tries to acquire the given spinlock:
 * - If the given r/w spinlock has not been locked, this function locks it.
 * - If the given r/w spinlock has been locked already, this function does
 *   nothing and returns immediately.
 *
 * \pre This caller shall not already hold a spinlock of lower or
 *      equal rank.
 *
 * \param[in,out] lock   Reader/writer spinlock to attempt to acquire.
 *
 * \retval VMK_OK    The given spinlock is successfully locked by
 *                   this call.
 * \retval VMK_BUSY  The given spinlock has been locked already.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RWTryReadLock(
   vmk_SpinlockRW *lock);

/*
 ***********************************************************************
 * vmk_RWIsRLocked --                                             */ /**
 *
 * \ingroup Spinlocks
 * \brief Tests a reader/writer spinlock to see if it is currently
 *        read-locked.
 *
 * \param[in]  lock  Reader/writer spinlock to check
 *
 * \retval VMK_OK          Lock is unlocked.
 * \retval VMK_BUSY        Lock is locked.
 * \retval VMK_BAD_PARAM   Not a valid lock.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RWIsRLocked(
   vmk_SpinlockRW *lock);

/*
 ***********************************************************************
 * VMK_ASSERT_RWLOCK_RLOCKED --                                    */ /**
 *
 * \ingroup Spinlocks
 * \brief Assert that a reader/writer lock is currently read-locked.
 *
 * \note This call only performs the check on a debug build.
 *       Other builds always succeed.
 *
 * \param[in] lock   Reader/writer spinlock to check
 *
 ***********************************************************************
 */
#define VMK_ASSERT_RWLOCK_RLOCKED(lock) \
   VMK_ASSERT(vmk_RWIsRLocked(lock) == VMK_BUSY)


/*
 ***********************************************************************
 * vmk_RWWriteLock --                                              */ /**
 *
 * \ingroup Spinlocks
 * \brief Acquire a reader/writer spinlock for writing.
 *
 * \pre The caller shall not already hold a spinlock of lower or
 *      equal rank.
 *
 * \param[in,out] lock  Reader/writer spinlock to acquire.
 *
 * \return None.
 *
 ***********************************************************************
 */
void vmk_RWWriteLock(
   vmk_SpinlockRW *lock);

/*
 ***********************************************************************
 * vmk_RWTryWriteLock --                                          */ /**
 *
 * \ingroup Spinlocks
 * \brief Try acquire the reader/writer spinlock for writing
 *
 * This function tries to acquire the given spinlock:
 * - If the given r/w spinlock has not been locked, this function locks it.
 * - If the given r/w spinlock has been locked already, this function does
 *   nothing and returns immediately.
 *
 * \pre This caller shall not already hold a spinlock of lower or
 *      equal rank.
 *
 * \param[in,out] lock   Reader/writer spinlock to attempt to acquire.
 *
 * \retval VMK_OK    The given spinlock is successfully locked by this
 *                   call.
 * \retval VMK_BUSY  The given spinlock has been locked already.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RWTryWriteLock(
   vmk_SpinlockRW *lock);

/*
 ***********************************************************************
 * vmk_RWIsWLocked --                                             */ /**
 *
 * \ingroup Spinlocks
 * \brief Tests a reader/writer spinlock to see if it is currently
 *        write-locked.
 *
 * \param[in]  lock  Reader/writer spinlock to check
 *
 * \retval VMK_OK          Lock is unlocked.
 * \retval VMK_BUSY        Lock is locked.
 * \retval VMK_BAD_PARAM   Not a valid lock.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RWIsWLocked(
   vmk_SpinlockRW *lock);

/*
 ***********************************************************************
 * VMK_ASSERT_RWLOCK_WLOCKED --                                    */ /**
 *
 * \ingroup Spinlocks
 * \brief Assert that a reader/writer lock is currently write-locked.
 *
 * \note This call only performs the check on a debug build.
 *       Other builds always succeed.
 *
 * \param[in] lock   Reader/writer spinlock to check
 *
 ***********************************************************************
 */
#define VMK_ASSERT_RWLOCK_WLOCKED(lock) \
   VMK_ASSERT(vmk_RWIsWLocked(lock) == VMK_BUSY)

/*
 ***********************************************************************
 * vmk_RWReadUnlock --                                            */ /**
 *
 * \ingroup Spinlocks
 * \brief Release a reader/writer spinlock from a read-lock.
 *
 * \pre The caller currently has a read-lock on the spinlock.
 *
 * \param[in,out] lock  Reader/writer spinlock to release.
 *
 ***********************************************************************
 */
void vmk_RWReadUnlock(
   vmk_SpinlockRW *lock);

/*
 ***********************************************************************
 * vmk_RWWriteUnlock --                                            */ /**
 *
 * \ingroup Spinlocks
 * \brief Release a reader/writer spinlock from a write-lock.
 *
 * \pre The caller currently has a write-lock on the spinlock.
 *
 * \param[in,out] lock  Reader/writer spinlock to release.
 *
 ***********************************************************************
 */
void vmk_RWWriteUnlock(
   vmk_SpinlockRW *lock);

/*
 ***********************************************************************
 * vmk_SPDestroy --                                               */ /**
 *
 * \ingroup Spinlocks
 * \brief Destroy a reader/writer spinlock
 *
 * Revert all side effects of vmk_RWCreate_LEGACY
 *
 * \param[in] lock   Reader/writer spinlock to be destroyed.
 *
 ***********************************************************************
 */
void vmk_RWDestroy(vmk_SpinlockRW *lock);

#endif /* _VMKAPI_SPLOCK_H_ */
/** @} */
