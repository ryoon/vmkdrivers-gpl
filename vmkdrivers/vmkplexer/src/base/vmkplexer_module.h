/* **********************************************************
 * Copyright 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * vmkplexer_module.h
 *
 *      Module-specific information regarding the vmkplexer
 */

#ifndef VMKPLEXER_MODULE_H
#define VMKPLEXER_MODULE_H

extern vmk_ModuleID vmkplxr_module_id;
extern vmk_HeapID vmkplxr_heap_id;

#define VMKPLXR_HEAP_INITIAL (256*1024)
#define VMKPLXR_HEAP_MAX (20*1024*1024)

#endif /* VMKPLEXER_MODULE_H */
