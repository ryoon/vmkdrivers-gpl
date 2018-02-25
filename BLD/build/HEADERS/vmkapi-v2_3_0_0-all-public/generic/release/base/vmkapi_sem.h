/* **********************************************************
 * Copyright 2004 - 2009 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Semaphores                                                     */ /**
 * \addtogroup Core
 * @{
 * \defgroup Semaphores Semaphores
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_SEM_H_
#define _VMKAPI_SEM_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/**
 * \brief Opaque handle for semaphores
 */
typedef struct vmk_SemaphoreInt *vmk_Semaphore;

/**
 * \brief Opaque handle for readers-writers semaphores
 */
typedef struct vmk_SemaphoreRWInt *vmk_SemaphoreRW;

/*
 ***********************************************************************
 * vmk_SemaCreate --                                       */ /**
 *
 * \brief Allocate and initialize a counting semaphore
 *
 * \note  This function will not block.
 *
 * \note Requires that the module heap be initialized.
 *
 * \param[out] sema              New counting semaphore.
 * \param[in]  moduleID          Module on whose behalf the semaphore
 *                               is created.
 * \param[in]  name              Human-readable name of the semaphore.
 * \param[in]  value             Initial count.
 *
 * \retval VMK_OK The semaphore was successfully created
 * \retval VMK_NO_MEMORY The semaphore could not be allocated
 * \retval VMK_NO_MODULE_HEAP The module has no heap to allocate from
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SemaCreate(
   vmk_Semaphore *sema,
   vmk_ModuleID moduleID,
   const char *name,
   vmk_int32 value);

/*
 ***********************************************************************
 * vmk_BinarySemaCreate --                                 */ /**
 *
 * \brief Allocate and initialize a binary semaphore
 *
 * \note  This function will not block.
 *
 * \note Requires that the module heap be initialized.
 *
 * \param[out] sema              New counting semaphore.
 * \param[in]  moduleID          Module on whose behalf the semaphore
 *                               is created.
 * \param[in]  name              Human-readable name of the semaphore.
 *
 * \retval VMK_OK The semaphore was successfully created
 * \retval VMK_NO_MEMORY The semaphore could not be allocated
 * \retval VMK_NO_MODULE_HEAP The module has no heap to allocate from
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_BinarySemaCreate(
   vmk_Semaphore *sema,
   vmk_ModuleID moduleID,
   const char *name);

/*
 ***********************************************************************
 * vmk_SemaAssertHasState --
 *
 * This is used by vmk_SemaAssertLocked() and vmk_SemaAssertUnlocked().
 * VMKAPI clients should not call this function directly.
 *
 * \note  This function will not block.
 *
 ***********************************************************************
 */

/** \cond nodoc */
void vmk_SemaAssertHasState(
   vmk_Semaphore *sema,
   vmk_Bool locked);
/** \endcond */

/*
 ***********************************************************************
 * vmk_SemaAssertIsLocked --                                      */ /**
 *
 * \brief Assert that a semaphore is currently locked.
 *
 * \note This function will not block.
 * \note This call only performs the check on a debug build.
 *
 * \param[in] sema   Semaphore to check.
 *
 ***********************************************************************
 */

static VMK_ALWAYS_INLINE void
vmk_SemaAssertIsLocked(
   vmk_Semaphore *sema)
{
#ifdef VMX86_DEBUG
   vmk_SemaAssertHasState(sema, VMK_TRUE);
#endif
}


/*
 ***********************************************************************
 * vmk_SemaAssertIsUnlocked --                                    */ /**
 *
 * \brief Assert that a semaphore is currently unlocked.
 *
 * \note This function will not block.
 * \note This call only performs the check on a debug build.
 *
 * \param[in] sema   Semaphore to check.
 *
 ***********************************************************************
 */

static VMK_ALWAYS_INLINE void
vmk_SemaAssertIsUnlocked(
   vmk_Semaphore *sema)
{
#ifdef VMX86_DEBUG
   vmk_SemaAssertHasState(sema, VMK_FALSE);
#endif
}


/*
 ***********************************************************************
 * vmk_SemaLock --                                                */ /**
 *
 * \brief Acquire a semaphore
 *
 * \note  This function may block.
 *
 * \pre Shall be called from a blockable context.
 * \pre The caller shall not already hold a semaphore of lower or equal
 *      rank if the semaphore is a binary semaphore.
 *
 * \param[in] sema   The semaphore to acquire.
 *
 ***********************************************************************
 */
void vmk_SemaLock(
   vmk_Semaphore *sema);

/*
 ***********************************************************************
 * vmk_SemaTryLock --                                             */ /**
 *
 * \brief Try to acquire a semaphore.
 *
 * \note  This function will not block.
 *
 * This tries to acquire the given semaphore once.
 * If the semaphore is already locked, it returns immediately.
 *
 * \pre  Shall be called from a blockable context.
 *
 * \param[in] sema   Semaphore to attempt to acquire.
 *
 * \retval VMK_OK       The semaphore was successfully acquired.
 * \retval VMK_BUSY     The semaphore is currently locked.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SemaTryLock(
   vmk_Semaphore *sema);

/*
 ***********************************************************************
 * vmk_SemaUnlock --                                              */ /**
 *
 * \brief Release a semaphore
 *
 * \note  This function will not block.
 *
 * \param[in] sema   Semaphore to unlock.
 *
 ***********************************************************************
 */
void vmk_SemaUnlock(
   vmk_Semaphore *sema);

/*
 ***********************************************************************
 * vmk_SemaDestroy --                                             */ /**
 *
 * \brief Destroy a semaphore
 *
 * \note  This function will not block.
 *
 * Revert all side effects of vmk_SemaCreate or
 * vmk_BinarySemaCreate.
 *
 * \param[in] sema   Semaphore to destroy.
 *
 ***********************************************************************
 */
void vmk_SemaDestroy(vmk_Semaphore *sema);

/*
 ***********************************************************************
 * vmk_RWSemaCreate --                                     */ /**
 *
 * \brief Allocate and initialize a readers-writers semaphore
 *
 * \note  This function will not block.
 *
 * \param[out] sema              New readers-writers semaphore.
 * \param[in]  moduleID          Module on whose behalf the semaphore
 *                               is created.
 * \param[in]  name              Human-readable name of the semaphore.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RWSemaCreate(
   vmk_SemaphoreRW *sema,
   vmk_ModuleID moduleID,
   const char *name);

/*
 ***********************************************************************
 * vmk_RWSemaDestroy --                                           */ /**
 *
 * \brief Destroy a readers-writers semaphore
 *
 * \note  This function will not block.
 *
 * Revert all side effects of vmk_RWSemaCreate.
 *
 * \param[in] sema   Readers-writers semaphore to destroy.
 *
 ***********************************************************************
 */
void vmk_RWSemaDestroy(vmk_SemaphoreRW *sema);

/*
 ***********************************************************************
 * vmk_RWSemaReadLock --                                          */ /**
 *
 * \brief Acquire a readers-writers semaphore for shared read access.
 *
 * \note  This function may block.
 *
 * \pre Shall be called from a blockable context.
 *
 * \param[in] sema   Readers-writers semaphore to acquire.
 *
 ***********************************************************************
 */
void vmk_RWSemaReadLock(vmk_SemaphoreRW *sema);

/*
 ***********************************************************************
 * vmk_RWSemaTryReadLock --                                       */ /**
 *
 * \brief Attempt to acquire a rw semaphore for shared read access.
 *
 * \note  This function will not block.
 *
 * \param[in] sema   Readers-writers semaphore to acquire.
 *
 * \retval VMK_OK           The semaphore was acquired.
 * \retval VMK_WOULD_BLOCK  The semaphore does not allow immediate reader
 *                          access.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RWSemaTryReadLock(vmk_SemaphoreRW *sema);

/*
 ***********************************************************************
 * vmk_RWSemaWriteLock --                                         */ /**
 *
 * \brief Acquire a readers-writers semaphore for exclusive write
 *        access.
 *
 * \note  This function may block.
 *
 * \pre Shall be called from a blockable context.
 *
 * \param[in] sema   Readers-writers semaphore to acquire.
 *
 ***********************************************************************
 */
void vmk_RWSemaWriteLock(vmk_SemaphoreRW *sema);

/*
 ***********************************************************************
 * vmk_RWSemaReadUnlock --                                        */ /**
 *
 * \brief Release a readers-writers semaphore from shared read access.
 *
 * \note  This function will not block.
 *
 * \param[in] sema   Readers-writers semaphore to release.
 *
 ***********************************************************************
 */
void vmk_RWSemaReadUnlock(vmk_SemaphoreRW *sema);

/*
 ***********************************************************************
 * vmk_RWSemaWriteUnlock --                                       */ /**
 *
 * \brief Release a readers-writers semaphore from exclusive write
 *        access.
 *
 * \note  This function will not block.
 *
 * \param[in] sema   Readers-writers semaphore to release.
 *
 ***********************************************************************
 */
void vmk_RWSemaWriteUnlock(vmk_SemaphoreRW *sema);

/*
 ***********************************************************************
 * vmk_RWSemaUpgradeFromRead --                                   */ /**
 *
 * \brief Upgrade a readers-writers semaphore to exclusive write access.
 *
 * This requests an upgrade to exclusive writers access for the
 * readers-writers semaphore while already holding shared reader access.
 * If the upgrade is not immediately available, only the first caller
 * can block waiting for the upgrade.  Others fail, but they retain
 * shared reader access.
 *
 * \note  This function may block.
 *
 * \pre Shall already have read access to the semaphore.
 *
 * \param[in] sema   Readers-writers semaphore to upgrade.
 *
 * \retval VMK_OK       The semaphore was upgraded.
 * \retval VMK_BUSY     The semaphore could not be upgraded.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RWSemaUpgradeFromRead(vmk_SemaphoreRW *sema);


/*
 ***********************************************************************
 * vmk_RWSemaTryUpgradeFromRead --                                */ /**
 *
 * \brief Attempt to upgrade a readers-writers semaphore to
 *        exclusive write access.
 *
 * \note  This function will not block.
 *
 * This attempts to obtain exclusive writers access to the
 * readers-writers semaphore while already holding shared reader access.
 * If the upgrade is not immediately available, shared reader access
 * is retained.
 *
 * \pre Shall already have read access to the semaphore.
 *
 * \param[in] sema   Readers-writers semaphore to upgrade.
 *
 * \retval VMK_OK       The semaphore was upgraded.
 * \retval VMK_BUSY     The semaphore could not be upgraded.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RWSemaTryUpgradeFromRead(vmk_SemaphoreRW *sema);

/*
 ***********************************************************************
 * vmk_RWSemaDowngradeToRead --                                   */ /**
 *
 * \brief Downgrade a readers-writers semaphore from exclusive
 *        write access to shared read access.
 *
 * \note  This function will not block.
 *
 * \pre Shall already have write access to the semaphore.
 *
 * \param[in] sema   Readers-writers semaphore to downgrade.
 *
 ***********************************************************************
 */
void vmk_RWSemaDowngradeToRead(vmk_SemaphoreRW *sema);

/*
 ***********************************************************************
 * vmk_RWSemaAssertHasReadersInt --
 *
 * This is used internally by vmk_RWSemaAssertHasReaders.  VMKAPI
 * clients should not call this function directly.
 *
 * \note  This function will not block.
 *
 ***********************************************************************
 */

/** \cond nodoc */
void vmk_RWSemaAssertHasReadersInt(vmk_SemaphoreRW *sema);
/** \endcond */

/*
 ***********************************************************************
 * vmk_RWSemaAssertHasReaders --                                  */ /**
 *
 * \brief Assert that a readers-writers semaphore has at least one
 *        shared reader.  The check is only performed in debug builds.
 *
 * \note  This function will not block.
 *
 * \param[in] sema   Semaphore to check.
 *
 ***********************************************************************
 */

static VMK_ALWAYS_INLINE void
vmk_RWSemaAssertHasReaders(vmk_SemaphoreRW *sema)
{
#ifdef VMX86_DEBUG
   vmk_RWSemaAssertHasReadersInt(sema);
#endif
}


/*
 ***********************************************************************
 * vmk_SemaAssertHasWriterInt --
 *
 * This is used internally by vmk_RWSemaAssertHasWriter.  VMKAPI
 * clients should not call this function directly.
 *
 * \note  This function will not block.
 *
 ***********************************************************************
 */

/** \cond nodoc */
void vmk_RWSemaAssertHasWriterInt(vmk_SemaphoreRW *sema);
/** \endcond */


/*
 ***********************************************************************
 * vmk_RWSemaAssertHasWriter --                                   */ /**
 *
 * \brief Assert that a readers-writers semaphore has a writer.
 *        The check is only performed in debug builds.
 *
 * \note This call will not block.
 * 
 * \param[in] sema   Semaphore to check.
 *
 ***********************************************************************
 */

static VMK_ALWAYS_INLINE void
vmk_RWSemaAssertHasWriter(vmk_SemaphoreRW *sema)
{
#ifdef VMX86_DEBUG
   vmk_RWSemaAssertHasWriterInt(sema);
#endif
}


/*
 ***********************************************************************
 * vmk_SemaAssertHasReadersWriterInt --
 *
 * This is used internally by vmk_RWSemaAssertHasReadersWriter.
 * VMKAPI clients should not call this function directly.
 *
 * \note  This function will not block.
 *
 ***********************************************************************
 */

/** \cond nodoc */
void vmk_RWSemaAssertHasReadersWriterInt(vmk_SemaphoreRW *sema);
/** \endcond */

/*
 ***********************************************************************
 * vmk_RWSemaAssertHasReadersWriter --                            */ /**
 *
 * \brief Assert that a readers-writers semaphore has either readers
 *        or a writer. The check is only performed on debug builds.
 *
 * \note  This call will not block.
 *
 * \param[in] sema   Semaphore to check.
 *
 ***********************************************************************
 */

static VMK_ALWAYS_INLINE void
vmk_RWSemaAssertHasReadersWriter(vmk_SemaphoreRW *sema)
{
#ifdef VMX86_DEBUG
   vmk_RWSemaAssertHasReadersWriterInt(sema);
#endif
}


#endif /* _VMKAPI_SEM_H_ */
/** @} */
/** @} */
