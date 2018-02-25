/* **********************************************************
 * Copyright 2012 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Dump                                                     */ /**
 * \addtogroup Core
 * @{
 * \defgroup Dump VMKernel Crash Dumps
 *
 * Functions related to VMKernel Crash Dumps.  These functions allow
 * vmkapi users to register files to be created in a zdump file.
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_DUMP_H_
#define _VMKAPI_DUMP_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */


/**
 * \brief Prototype for a dump callback function.
 *
 * After being registered for dump callback, this function is called during
 * system dump time.  There are two types of system dump.  The first one
 * is when the system crashes during normal operation and a core dump is
 * written.  The second type, called a live dump, occurs while the system
 * is still operational and is an additional tool for diagnosing issues
 * without bringing the whole system down.
 *
 * There are two functions a callback can use to add data to the dump.
 * These are vmk_DumpRange() and vmk_DumpMPN().  vmk_DumpRange() may only
 * be called on data currently known to be correctly mapped (consistently
 * across all CPUs), as calls to unmapped spaces will generate page faults.
 *
 * For both functions it is also required that there are no side effects
 * from reading the memory (e.g. a memory mapped PCI device should not
 * treat reading any piece of memory as an acknowledge of some transaction
 * for example).
 *
 * For the live dump type of call vmk_DumpMPN() is the only safe method
 * of adding data that may not be currently mapped.  This may happen when
 * unmap events are not managed with any concurrency protection.
 *
 * \note This function may only block when liveDump is TRUE.
 *
 * \param[in]  cookie     Private data as specified vmk_DumpAddFileCallback().
 * \param[in]  liveDump   TRUE only if the system is still currently functional.
 *                        In this case concurrency issues should be considered
 *                        as normal for the sub-system.  This means that locking
 *                        and/or other synchronization should only be performed
 *                        when liveDump is TRUE.
 */
typedef VMK_ReturnStatus (*vmk_DumpFileCallback)(
   void *cookie,
   vmk_Bool liveDump);


/**
 * \brief Dump file handle
 *
 * Returned as part of the vmk_DumpAddFileCallback() call.  Used for
 * all other calls.
 */
typedef void *vmk_DumpFileHandle;


/*
 *******************************************************************************
 * vmk_DumpAddFileCallback --                                             */ /**
 *
 * \brief Register a file to be created at kernel core dump time.
 * 
 * This function registers a callback to allow memory to be dumped in a
 * VMKernel core file under the specified name.
 *
 * \param[in]  moduleID    Module ID of the caller.
 * \param[in]  heapID      Heap ID to be used for allocations.
 * \param[in]  name        The name of the file to be created by vmkdump_extract.
 * \param[in]  func        The callback function itself.
 * \param[in]  cookie      Opaque cookie passed to the callback.
 * \param[in]  dumpName    Tag used for error messages on this file.
 * \param[out] outHandle   Dump Handle is written here upon success.
 *
 * \note This function will not block.
 *
 * \retval VMK_OK              Callback successfully added to dump-file table.
 * \retval VMK_LIMIT_EXCEEDED  Dump file table is full.
 * \retval VMK_NAME_INVALID    Specified name was invalid.
 * \retval VMK_NO_MEMORY       Allocation from heapID failed.
 *
 *******************************************************************************
 */
VMK_ReturnStatus vmk_DumpAddFileCallback(
   vmk_ModuleID moduleID,
   vmk_HeapID heapID,
   const char *name,
   vmk_DumpFileCallback func,
   void *cookie,
   char *dumpName,
   vmk_DumpFileHandle *outHandle);


/*
 *******************************************************************************
 * vmk_DumpDeleteFileCallback --                                          */ /**
 *
 * \brief Unregister a file to be created at kernel core dump time.
 * 
 * This function unregisters a callback to allow memory to be dumped in a
 * VMKernel core file under the specified name.
 *
 * \param[in]  handle       Dump handle returned from vmk_DumpAddFileCallback().
 *
 * \note This function will not block.
 *
 * \retval VMK_OK           Callback successfully removed to dump-file table.
 * \retval VMK_NOT_FOUND    Dump-file table entry could not be found.
 *
 *******************************************************************************
 */
VMK_ReturnStatus vmk_DumpDeleteFileCallback(
   vmk_DumpFileHandle handle);


/*
 *******************************************************************************
 * vmk_DumpRange --                                          */ /**
 *
 * \brief Dump a region of memory into a VMKernel core file.
 *
 * All errors occurring in an invocation will be logged with the dumpName tag
 * registered at dump handle creation.
 * 
 * \param[in]  handle       Dump handle returned from vmk_DumpAddFileCallback().
 * \param[in]  va           Virtual Address to begin dumping.  If zero,
 *                          dump zero-byte data upto one PAGE_SIZE.
 * \param[in]  size         Length of region to dump.
 *
 * \note This function is only to be used in a call back registered via
 *       vmk_DumpAddFileCallback().
 *
 * \note This function may block when vmk_DumpAddFileCallback() was called
 *       with liveDump is TRUE.
 * 
 * \retval VMK_OK              Region was successfully dumped.
 * \retval VMK_LIMIT_EXCEEDED  More than PAGE_SIZE of zeros requested, or size
 *                             of dump exceeded.
 * \retval VMK_FAILURE         Gzip deflate failed.
 *
 *******************************************************************************
 */
VMK_ReturnStatus vmk_DumpRange(
   vmk_DumpFileHandle handle,
   vmk_VA va,
   vmk_uint32 size);


/*
 *******************************************************************************
 * vmk_DumpMPN --                                          */ /**
 *
 * \brief Dump a page of memory into a VMKernel core file.
 * 
 * All errors occurring in an invocation will be logged with the dumpName tag
 * registered at dump handle creation.
 * 
 * \param[in]  handle       Dump handle returned from vmk_DumpAddFileCallback().
 * \param[in]  mpn          VMKernel machine page number to dump.
 *
 * \note This function is only to be used in a call back registered via
 *       vmk_DumpAddFileCallback().
 *
 * \note This function may block when vmk_DumpAddFileCallback() was called
 *       with liveDump is TRUE.
 *
 * \retval VMK_OK              Page was successfully dumped.
 * \retval VMK_LIMIT_EXCEEDED  More than PAGE_SIZE of zeros requested, or size
 *                             of dump exceeded.
 * \retval VMK_FAILURE         Gzip deflate failed.
 *
 *******************************************************************************
 */
VMK_ReturnStatus vmk_DumpMPN(
   vmk_DumpFileHandle handle,
   vmk_MPN mpn);


#endif /* _VMKAPI_DUMP_H_ */
/** @} */
/** @} */
