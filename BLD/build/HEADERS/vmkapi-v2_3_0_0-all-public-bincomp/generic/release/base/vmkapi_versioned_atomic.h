/* **********************************************************
 * Copyright 1998 - 2012VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */


/*
 ***********************************************************************
 * VersionedAtomic                                                */ /**
 * \defgroup VersionedAtomic Version Atomic
 *
 * \par Versioned atomic synchronization:
 * These synchronization macros allow single-writer/many-reader access
 * to data, based on Leslie Lamport's paper "Concurrent Reading and
 * Writing", Communications of the ACM, November 1977.\n
 * \n
 * Many-writer/many-reader can be implemented on top of versioned
 * atomics by using an additional spin lock to synchronize writers.
 * This is preferable for cases where readers are expected to greatly
 * outnumber writers.\n
 * \n
 * Multiple concurrent writers to the version variables are not allowed.
 * Even if writers are working on lock-free or disjoint data, the
 * version counters are not interlocked for read-modify-write.\n
 * \n
 * Recursive use of versioned atomics in writers is currently not
 * supported.
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_VERSIONED_ATOMIC_H
#define _VMKAPI_VERSIONED_ATOMIC_H

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */


/**
 * \brief Versioned Atomic
 */

typedef struct vmk_VersionedAtomic {
   /** version 0 */
   volatile vmk_uint32 v0;

   /** version 1 */
   volatile vmk_uint32 v1;
} VMK_ATTRIBUTE_ALIGN(4) vmk_VersionedAtomic;


/*
 ***********************************************************************
 * vmk_VersionedAtomicInit --                                     */ /**
 *
 * \brief Intiialize a versioned atomic
 * \param[in]  versions    Pointer to the versioned atomic to be
 *                         initialized
 *
 ***********************************************************************
 */

static VMK_ALWAYS_INLINE void
vmk_VersionedAtomicInit(vmk_VersionedAtomic *versions)
{
	versions->v0 = 0;
	versions->v1 = 0;
}


/*
 ***********************************************************************
 * vmk_VersionedAtomicBeginWrite --                               */ /**
 *
 * \brief Writer begins write to protected data
 *
 * Called by a writer to indicate that the data protected by a given
 * atomic version is about to change. Effectively locks out all readers
 * until EndWrite is called.
 *
 * \param[in]  versions    Pointer to the versioned atomic protecting
 *                         the data
 *
 ***********************************************************************
 */

static VMK_ALWAYS_INLINE void
vmk_VersionedAtomicBeginWrite(vmk_VersionedAtomic *versions)
{
   VMK_ASSERT(((vmk_uint64)(&versions->v0) & (sizeof(versions->v0) - 1)) == 0);
   VMK_ASSERT(versions->v1 == versions->v0);
   versions->v0++;
   vmk_CPUMemFenceReadWrite();
}


/*
 ***********************************************************************
 * vmk_VersionedAtomicEndWrite --                                 */ /**
 *
 * \brief Writer finishes writing to protected data
 *
 * Called by a writer after it is done updating shared data. Lets
 * pending and new readers proceed on shared data.
 *
 * \param[in]  versions    Pointer to the versioned atomic protecting
 *                         the data
 *
 ***********************************************************************
 */

static VMK_ALWAYS_INLINE void
vmk_VersionedAtomicEndWrite(vmk_VersionedAtomic *versions)
{
   VMK_ASSERT(((vmk_uint64)(&versions->v1) & (sizeof(versions->v1) - 1)) == 0);
   VMK_ASSERT(versions->v1 + 1 == versions->v0);
   vmk_CPUMemFenceReadWrite();
   versions->v1 = versions->v0;
}



/*
 ***********************************************************************
 * vmk_VersionedAtomicBeginTryRead --                             */ /**
 *
 * \brief Reader tries to read protected data
 *
 * Called by a reader before it tried to read shared data.
 *
 * \param[in]  versions    Pointer to the versioned atomic protecting
 *                         the data
 *
 * \retval                 Returns a version number to the reader. This
 *                         version number is required to confirm
 *                         validity of the read operation when reader
 *                         calls vmk_VersionedAtomicEndTryRead.
 *
 ***********************************************************************
 */

static VMK_ALWAYS_INLINE vmk_uint32
vmk_VersionedAtomicBeginTryRead(const vmk_VersionedAtomic *versions)
{
   vmk_uint32 readVersion;

   readVersion = versions->v1;
   vmk_CPUMemFenceReadWrite();

   return readVersion;
}


/*
 ***********************************************************************
 * vmk_VersionedAtomicEndTryRead --                               */ /**
 *
 * \brief Reader finished reading protected data
 *
 * Called by a reader after it finishes reading shared data, to confirm
 * validity of the data that was just read, to make sure that a writer
 * did not intervene while the read was in progress.
 *
 * \param[in]  versions    Pointer to the versioned atomic protecting
 *                         the data
 * \param[in]  readVersion The version number the reader just read,
 *                         returned by vmk_VersionedAtomicBeginTryRead
 *
 * \retval     VMK_TRUE    if the data read between
 *                         vmk_VersionedAtomicBeginTryRead() and this
 *                         call is valid.
 * \retval     VMK_FALSE   otherwise.
 *
 ***********************************************************************
 */
static VMK_ALWAYS_INLINE vmk_Bool
vmk_VersionedAtomicEndTryRead(const vmk_VersionedAtomic *versions,
                              vmk_uint32 readVersion)
{
   vmk_CPUMemFenceReadWrite();
   return VMK_LIKELY(versions->v0 == readVersion);
}

#endif //__VMKAPI_VERSIONED_ATOMIC_H
/** @} */
