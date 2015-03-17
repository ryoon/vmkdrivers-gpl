/* ****************************************************************
 * Portions Copyright 2004, 2010, 2012 VMware, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * ****************************************************************/

/*
 * linux_heap.c --
 *
 *	This file enables a module to have its own private heap, replete with
 *	posion-value checks, etc. to guarantee that modules and other kernel
 *	code don't step on each other's toes. These functions assumes at the
 *      times that they are being called that the module private is already 
 *      been created. 
 */

#include <asm/page.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <linux/pci.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/hardirq.h>
#include <asm/proto.h>

#include "vmkapi.h"
#include "linux_stubs.h"
#include "vmklinux_log.h"

void
vmklnx_kfree(vmk_HeapID heapID, const void *p)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if (!ZERO_OR_NULL_PTR(p)) {
      vmk_HeapFree(heapID, (void *)p);
   }
}
EXPORT_SYMBOL(vmklnx_kfree);

void *
vmklnx_kmalloc(vmk_HeapID heapID, size_t size, gfp_t flags, void *ra)
{
   size_t actual;
   void *d;
   void *callerPC = ra;
   vmk_uint32 timeout = VMK_TIMEOUT_NONBLOCKING;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if (flags & __GFP_WAIT) {
#if 0
   /*
    * Disable for now until all inbox drivers are tested
    * with this assert enabled in a private build.
    */
      VMK_WORLD_ASSERT_IS_SAFE_TO_BLOCK();
#endif
      timeout = VMK_TIMEOUT_UNLIMITED_MS;
   }

   if (unlikely(size == 0)) {
      return ZERO_SIZE_PTR;
   }

   if (likely(ra == NULL)) {
      callerPC = __builtin_return_address(0);
   }

   /*
    * Allocate blocks in powers-of-2 (like Linux), so we tolerate any
    * driver bugs that don't show up because of the large allocation size.
    */
   actual = 1 << (fls(size) - 1);

   if (actual < size) {
      /*
       * The assert below guards against shitting the bit 
       * off.  In any case, we should not have such large
       * size request.
       */
      VMK_ASSERT(actual != ~((((size_t) -1) << 1) >> 1));
      actual <<=1;
   }

   d = vmk_HeapAllocWithTimeoutAndRA(heapID, actual, timeout, callerPC);

   return d;
}
EXPORT_SYMBOL(vmklnx_kmalloc);

void *
vmklnx_kzmalloc(vmk_HeapID heapID, size_t size, gfp_t flags)
{
   void *p;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   p = vmklnx_kmalloc(heapID, size, flags, __builtin_return_address(0));

   if (likely(p != NULL))
      memset(p, 0, size);

   return p;
}
EXPORT_SYMBOL(vmklnx_kzmalloc);

void*
vmklnx_kmalloc_align(vmk_HeapID heapID, size_t size, size_t align, gfp_t flags)
{
   size_t actual;
   void *d;
   vmk_uint32 timeout = VMK_TIMEOUT_NONBLOCKING;

   if (flags & __GFP_WAIT) {
#if 0
   /*
    * Disable for now until all inbox drivers are tested
    * with this assert enabled in a private build.
    */
      VMK_WORLD_ASSERT_IS_SAFE_TO_BLOCK();
#endif
      timeout = VMK_TIMEOUT_UNLIMITED_MS;
   }

   if (unlikely(size == 0)) {
      VMKLNX_WARN("vmklnx_kmalloc_align: size == 0");
      return NULL;
   }

   /*
    * Allocate blocks in powers-of-2 (like Linux), so we tolerate any
    * driver bugs that don't show up because of the large allocation size.
    */
   actual = 1 << (fls(size) - 1);

   if (actual < size) {
      /*
       * The assert below guards against shitting the bit 
       * off.  In any case, we should not have such large
       * size request.
       */
      VMK_ASSERT(actual != ~((((size_t) -1) << 1) >> 1));
      actual <<=1;
   }

   d = vmk_HeapAlignWithTimeoutAndRA(heapID,
				     actual,
				     align,
                                     timeout,
				     __builtin_return_address(0));
   return d;
}

/*
 *----------------------------------------------------------------------
 *
 * vmklnx_kstrdup
 *
 *      Allocate space for and copy an existing string
 *
 * Results:
 *      A pointer to the allocated area where the string has been copied
 *
 * Side effects:
 *      A memory is allocated from the module heap
 *
 *----------------------------------------------------------------------
 */
char *
vmklnx_kstrdup(vmk_HeapID heapID, const char *s, void *ra)
{
   size_t len;
   char *buf;
   void *callerPC = ra;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if (!s) {
      return NULL;
   }
   if (likely(ra == NULL)) {
      callerPC = __builtin_return_address(0);
   }
   len = strlen(s) + 1;
   buf = vmklnx_kmalloc(heapID, len, GFP_KERNEL, callerPC);
   if (buf) {
      memcpy(buf, s, len);
   }
   return buf;
}
EXPORT_SYMBOL(vmklnx_kstrdup);

/* kmem_cache implementation based upon vmkapi Slabs */

#define KMEM_CACHE_MAGIC        0xfa4b9c23

struct kmem_cache_s {
#ifdef VMX86_DEBUG
   vmk_uint32           magic;
#endif
   vmk_SlabID           slabID;
   vmk_HeapID           heapID;
   vmk_Name             slabName;
   void (*ctor)(void *, struct kmem_cache_s *, unsigned long);
   void (*dtor)(void *, struct kmem_cache_s *, unsigned long);
   vmk_ModuleID         moduleID;
};


/*
 *----------------------------------------------------------------------
 *
 * VmklnxKmemCacheConstructor
 *
 *      kmem_alloc_cache constructor
 *
 * Results:
 *      VMK_OK
 *
 * Side effects:
 *      Calls the client's "ctor" function.
 *
 *----------------------------------------------------------------------
 */


static VMK_ReturnStatus
VmklnxKmemCacheConstructor(void *item, vmk_uint32 size,
                     vmk_AddrCookie cookie, int flags)
{
   struct kmem_cache_s *cache = cookie.ptr;

   VMKAPI_MODULE_CALL_VOID(cache->moduleID, cache->ctor, item, cache, 0);

   return VMK_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * VmklnxKmemCacheDestructor
 *
 *      kmem_alloc_cache destructor.
 *
 * Results:
 *      none
 *
 * Side effects:
 *      Calls the client's "dtor" function.
 *
 *----------------------------------------------------------------------
 */

static void
VmklnxKmemCacheDestructor(void *item, vmk_uint32 size, vmk_AddrCookie cookie)
{
   struct kmem_cache_s *cache = cookie.ptr;

   VMKAPI_MODULE_CALL_VOID(cache->moduleID, cache->dtor, item, cache, 0);
}

struct kmem_cache_s *
vmklnx_kmem_cache_create(struct vmklnx_mem_info *mem_info, const char *name , 
		  size_t size, size_t offset,
		  void (*ctor)(void *, struct kmem_cache_s *, unsigned long),
		  void (*dtor)(void *, struct kmem_cache_s *, unsigned long))
{
   vmk_SlabCreateProps slab_props;
   VMK_ReturnStatus status;
   struct kmem_cache_s *cache;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   vmk_Memset(&slab_props, 0, sizeof(vmk_SlabCreateProps)); // Init props

   slab_props.typeSpecific.memPool.physRange = mem_info->mem_phys_addr_type;
   slab_props.typeSpecific.memPool.physContiguity = mem_info->mem_phys_contig_type;

   cache = vmk_HeapAlloc(mem_info->heapID, sizeof(*cache));                                               
   if (cache == NULL) {                                                                         
      VMKLNX_WARN("out of memory to allocate Heap for cache");                                                             
      return NULL;                                                                              
   }                                   

   cache->heapID = mem_info->heapID;
   cache->ctor = ctor;
   cache->dtor = dtor;
#ifdef VMX86_DEBUG
   cache->magic = KMEM_CACHE_MAGIC;
#endif /* VMX86_DEBUG */
   cache->moduleID = vmk_ModuleStackTop();
   status = vmk_NameInitialize(&cache->slabName, name);
   VMK_ASSERT(status == VMK_OK);

   // Init rest of slab_props
   slab_props.creationTimeoutMS = VMK_TIMEOUT_NONBLOCKING;
   slab_props.module = cache->moduleID;
   slab_props.type = VMK_SLAB_TYPE_MEMPOOL;
   slab_props.typeSpecific.memPool.memPool = mem_info->mempool;

   vmk_NameInitialize(&slab_props.name, name);
   VMK_ASSERT(status == VMK_OK);

   slab_props.objSize = size;
   slab_props.alignment = SMP_CACHE_BYTES;
   slab_props.minObj = 1;
   slab_props.maxObj = VMK_SLAB_MAX_UNLIMITED;
   slab_props.ctrlOffset = round_up(size, sizeof(void *));
   slab_props.constructor = (ctor != NULL) ? VmklnxKmemCacheConstructor : NULL;
   slab_props.destructor = (dtor != NULL) ? VmklnxKmemCacheDestructor : NULL;
   slab_props.constructorArg.ptr = cache;

   status = vmk_SlabCreate(&slab_props, &cache->slabID);
   if (status != VMK_OK) {
      VMKLNX_WARN("Slab creation failed for %s status:0x%x maxObj:%u.",
                  vmk_NameToString(&cache->slabName), status, slab_props.maxObj);
      vmk_HeapFree(mem_info->heapID, cache);
      return NULL;
   }

   return cache;
}
EXPORT_SYMBOL(vmklnx_kmem_cache_create);

/*
 *----------------------------------------------------------------------
 *
 * vmklnx_kmem_cache_destroy
 *
 *      Deallocate all elements in a kmem cache and destroy the
 *      kmem_cache object itself
 *
 * Results:
 *      Always 0
 *
 * Side effects:
 *      All cached objects and the kmem_cache object itself are freed
 *
 *----------------------------------------------------------------------
 */

int 
vmklnx_kmem_cache_destroy(struct kmem_cache_s *cache)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(cache != NULL);
   VMK_ASSERT(cache->magic == KMEM_CACHE_MAGIC);

   vmk_SlabDestroy(cache->slabID);
#ifdef VMX86_DEBUG
   cache->magic = 0;
#endif
   vmk_HeapFree(cache->heapID, cache);
   return 0;
}
EXPORT_SYMBOL(vmklnx_kmem_cache_destroy);

/*
 *----------------------------------------------------------------------
 *
 * vmklnx_kmem_cache_alloc
 *
 *      Allocate an element from the cache pool. If no free elements
 *      exist a new one will be allocated from the heap.
 *
 * Results:
 *      A pointer to the allocated object or NULL on error
 *
 * Side effects:
 *      Cache list may change or a new object is allocated from the heap
 *
 *----------------------------------------------------------------------
 */

void *
vmklnx_kmem_cache_alloc(struct kmem_cache_s *cache, gfp_t flags)
{
   vmk_uint32 timeout = VMK_TIMEOUT_NONBLOCKING;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(cache != NULL);
   VMK_ASSERT(cache->magic == KMEM_CACHE_MAGIC);

   if (flags & __GFP_WAIT) {
#if 0
   /*
    * Disable for now until all inbox drivers are tested
    * with this assert enabled in a private build.
    */
      VMK_WORLD_ASSERT_IS_SAFE_TO_BLOCK();
#endif
      timeout = VMK_TIMEOUT_UNLIMITED_MS;
   }

   return vmk_SlabAllocWithTimeout(cache->slabID, timeout);
}
EXPORT_SYMBOL(vmklnx_kmem_cache_alloc);

/*
 *----------------------------------------------------------------------
 *
 * vmklnx_kmem_cache_free
 *
 *      Release an element to the object cache. The memory will not be
 *      freed until the cache is destroyed
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Cache list will change
 *
 *----------------------------------------------------------------------
 */

void 
vmklnx_kmem_cache_free(struct kmem_cache_s *cache, void *item)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(cache != NULL);
   VMK_ASSERT(cache->magic == KMEM_CACHE_MAGIC);

   vmk_SlabFree(cache->slabID, item);
}
EXPORT_SYMBOL(vmklnx_kmem_cache_free);
