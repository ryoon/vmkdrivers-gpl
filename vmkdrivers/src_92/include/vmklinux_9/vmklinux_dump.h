/* **********************************************************
 * Copyright 2012 VMware, Inc.  All rights reserved.
 * **********************************************************/

#ifndef _VMKLNX_DUMP_H
#define _VMKLNX_DUMP_H

#include <linux/types.h>
#include <linux/module.h>
#include "vmkapi.h"

typedef vmk_DumpFileHandle vmklnx_DumpFileHandle;
typedef VMK_ReturnStatus (* vmklnx_dump_callback)(void *cookie, vmk_Bool liveDump);

VMK_ReturnStatus
vmklnx_dump_add_callback(const char *name,
                         vmklnx_dump_callback func,
                         void *cookie,
                         char *dumpName,
                         vmklnx_DumpFileHandle *h);

VMK_ReturnStatus
vmklnx_dump_delete_callback(vmklnx_DumpFileHandle h);

VMK_ReturnStatus
vmklnx_dump_range(vmklnx_DumpFileHandle h,
                  void *ptr,
                  uint32_t size);

VMK_ReturnStatus
vmklnx_dump_pfn(vmklnx_DumpFileHandle h,
                unsigned long pfn);

#endif  /* _VMKLNX_DUMP_H */
