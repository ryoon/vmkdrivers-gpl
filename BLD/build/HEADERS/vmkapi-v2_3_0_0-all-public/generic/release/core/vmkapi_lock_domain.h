/* **********************************************************
 * Copyright 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ******************************************************************************
 * Lock Domains                                                          */ /**
 *
 * \addtogroup Core
 * @{
 * \defgroup LockDomains Lock Domains
 * @{
 *
 * Lock domains can be used to implement a local lock ranking scheme within one
 * or more modules. All spinlocks that are created with the same lock domain ID
 * will be rank checked against each other.
 *
 ******************************************************************************
 */

#ifndef _VMKAPI_CORE_LOCKDOMAIN_H_
#define _VMKAPI_CORE_LOCKDOMAIN_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

struct vmkLockDomainInt;

/**
 * \brief Lock Domain ID
 */
typedef struct vmkLockDomainInt *vmk_LockDomainID;

/** Invalid domain ID */
#define VMK_LOCKDOMAIN_INVALID  ((vmk_LockDomainID)-1)

/*
 ******************************************************************************
 * vmk_LockDomainCreate                                                  */ /**
 *
 * \brief Create a lock domain
 *
 * Allocates a lock domain from the given heap and initializes it.
 *
 * \note This function will not block
 *
 * \param[in]     moduleID    Module ID of the caller
 * \param[in]     heapID      Heap to allocate the domain from
 * \param[in]     name        Name of the domain
 * \param[out]    domain      ID of the newly created domain
 *
 * \return VMK_OK if the domain was successfully created, error code otherwise
 *
 ******************************************************************************
 */

VMK_ReturnStatus
vmk_LockDomainCreate(vmk_ModuleID moduleID,
                     vmk_HeapID heapID,
                     vmk_Name *name,
                     vmk_LockDomainID *domain);


/*
 ******************************************************************************
 * vmk_LockDomainDestroy                                                 */ /**
 *
 * \brief Destroy a domain previously created via vmk_LockDomainCreate
 *
 * Destroy the given domain and free the allocated memory
 *
 * \note This function will not block
 *
 * \param[in]     domain      Domain ID
 *
 ******************************************************************************
 */

void
vmk_LockDomainDestroy(vmk_LockDomainID domain);


/*
 ******************************************************************************
 * vmk_LockDomainAllocSize                                               */ /**
 *
 * \brief Size that of the allocation that will be done by vmk_LockDomainCreate
 *
 * \note This function will not block
 *
 * \result Allocation size
 *
 ******************************************************************************
 */

vmk_ByteCountSmall
vmk_LockDomainAllocSize(void);


#endif
/** @} */
/** @} */
