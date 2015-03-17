/* ****************************************************************
 * Copyright 2012 VMware, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * ****************************************************************/

#include <vmklinux_9/vmklinux_dump.h>

/**
 * vmklnx_dump_add_callback - register dump callbcak.
 *
 * This function registers a callback to allow memory to be dumped in a
 * VMKernel core file under the specified name.
 *
 * @name:      The name of the file to be created by vmkdump_extract.
 * @func:      The callback function itself.
 * @cookie:    Opaque cookie passed to the callback.
 * @dumpName:  Tag used for error messages on this file.
 * @h:         Dump Handle is written here upon success.
 *
 * RETURN VALUE:
 * VMK_OK              Callback successfully added to dump-file table.
 * VMK_LIMIT_EXCEEDED  Dump file table is full.
 * VMK_NAME_INVALID    Specified name was invalid.
 * VMK_NO_MEMORY       Allocation from module heap failed.
 *
 */
/* _VMKLNX_CODECHECK_: vmklnx_dump_add_callback */
VMK_ReturnStatus
vmklnx_dump_add_callback(const char *name,
                         vmklnx_dump_callback func,
                         void *cookie,
                         char *dumpName,
                         vmklnx_DumpFileHandle *h)
{
   vmk_ModuleID moduleID;
   vmk_HeapID heapID;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   moduleID = THIS_MODULE->moduleID;
   VMK_ASSERT(moduleID != VMK_INVALID_MODULE_ID);

   heapID = vmk_ModuleGetHeapID(moduleID);
   VMK_ASSERT(heapID != VMK_INVALID_HEAP_ID);

   return vmk_DumpAddFileCallback(moduleID, heapID, name,
                                  (vmk_DumpFileCallback)func,
                                  cookie,
                                  dumpName,
                                  h);
}
EXPORT_SYMBOL(vmklnx_dump_add_callback);

/**
 * vmklnx_dump_delete_callback - unregister dump callback.
 *
 * This function unregisters a callback which allows memory to be dumped in a
 * VMKernel core file under the specified name.
 *
 * @h: Dump handle returned from vmklnx_dump_add_callback()
 *
 * RETURN VALUE:
 * VMK_OK           Callback successfully removed.
 * VMK_NOT_FOUND    Entry could not be found.
 *
 */
/* _VMKLNX_CODECHECK_: vmklnx_dump_delete_callback */
VMK_ReturnStatus
vmklnx_dump_delete_callback(vmklnx_DumpFileHandle h)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   return vmk_DumpDeleteFileCallback(h);
}
EXPORT_SYMBOL(vmklnx_dump_delete_callback);

/**
 * vmklnx_dump_range - dump a region of memory into a VMKernel core file.
 *
 * This function dumps a region of memory into a VMKernel core file.
 * All errors occurring in an invocation will be logged with the dumpName tag
 * registered at dump handle creation.
 *
 * This function is only to be used in a callback registered via
 * vmklnx_dump_add_callback().
 * This function may block when vmklnx_dump_callback was called
 * when liveDump is TRUE.
 *
 * @h:       Dump handle returned from vmklnx_dump_add_callback().
 * @ptr:     Virtual Address to begin dumping.  If zero,
 *           dump zero-byte data upto one PAGE_SIZE.
 * @size:    Length of region to dump.
 *
 * RETURN VALUE:
 * VMK_OK              Region was successfully dumped.
 * VMK_LIMIT_EXCEEDED  More than PAGE_SIZE of zeros requested, or size
 *                     of dump exceeded.
 * VMK_FAILURE         Gzip deflate failed.
 *
 */
/* _VMKLNX_CODECHECK_: vmklnx_dump_range */
VMK_ReturnStatus
vmklnx_dump_range(vmklnx_DumpFileHandle h,
                  void *ptr,
                  uint32_t size)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   return vmk_DumpRange(h, (vmk_VA)ptr, size);
}
EXPORT_SYMBOL(vmklnx_dump_range);

/**
 * vmklnx_dump_pfn - dump a page of memory into a VMKernel core file.
 *
 * This function dumps a page of memory into a VMKernel core file.
 * All errors occurring in an invocation will be logged with the dumpName tag
 * registered at dump handle creation.
 *
 * This function is only to be used in a call back registered via
 * vmklnx_dump_add_callback().
 * This function may block when vmklnx_dump_callback was called
 * when liveDump is TRUE.
 *
 * @h:        Dump handle returned from vmklnx_dump_add_callback().
 * @pfn:      Physical page number to dump.
 *
 * RETURN VALUE:
 * VMK_OK              Page was successfully dumped.
 * VMK_LIMIT_EXCEEDED  More than PAGE_SIZE of zeros requested, or size
 *                     of dump exceeded.
 * VMK_FAILURE         Gzip deflate failed.
 *
 */
/* _VMKLNX_CODECHECK_: vmklnx_dump_pfn */
VMK_ReturnStatus
vmklnx_dump_pfn(vmklnx_DumpFileHandle h,
                unsigned long pfn)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   return vmk_DumpMPN(h, (vmk_MPN)pfn);
}
EXPORT_SYMBOL(vmklnx_dump_pfn);
