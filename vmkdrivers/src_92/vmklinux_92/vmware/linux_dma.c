/* **********************************************************
 * Copyright 2007-2012 VMware, Inc.  All rights reserved.
 *
 * Functions operating on dma_ops came from the inlined 
 * versions in <linux>/include/asm-x86_64/dma-mapping.h.
 *
 * **********************************************************/

#include <linux/pci.h>
#include <asm/dma-mapping.h>
#if defined(__x86_64__)
#include <asm/proto.h>
#endif /* defined(__x86_64__) */
#include <linux/dmapool.h>

#include "vmkapi.h"
#include "linux_pci.h"
#include "linux_stubs.h"

#define VMKLNX_LOG_HANDLE LinDMA
#include "vmklinux_log.h"

#if defined(VMX86_DEBUG)
static vmk_Bool dma_constraint_stress_on;
#define DMA_BOUNDARY_STRESS_MASK 0x3f
#define DMA_MASK_STRESS_MASK     0x7f
#endif

struct dma_status_counters {
#if defined(VMX86_DEBUG)
   atomic_t oks;
#endif
   atomic_t failures;
};

/*
 * The defintions below are the indexes to use with the
 * array dma_map_status_counters[] defined inside struct
 * dma_stats.
 * The index value should match up with the order
 * of the fields in struct op_map_status.
 */
#define DMA_MAP_OP_MA   0
#define DMA_MAP_OP_VA   1
#define DMA_MAP_OP_PAGE 2
#define DMA_MAP_OP_SG   3

struct dma_stats {
   atomic_t total_mask_violations;
   atomic_t total_boundary_violations;
#if defined(VMX86_DEBUG)
   union {
#endif
      struct dma_status_counters map_op_status_counters[4];

#if defined(VMX86_DEBUG)
      /*
       * The struct below is intended for reading the counters
       * easier inside gdb
       */
      struct map_op_status {
         struct dma_status_counters map_op_ma;
         struct dma_status_counters map_op_va;
         struct dma_status_counters map_op_page;
         struct dma_status_counters map_op_sg;
      } map_op_status;
   };
#endif
} dma_stats;

struct dma_pool {
   char	 name [32];
   struct device *dev;
   size_t size;
   size_t align;
   vmk_HeapID heapId;
#ifdef VMX86_DEBUG
   u64 coherent_dma_mask;
#endif
};

static vmk_SgOpsHandle vmklnx_dma_sgops;

dma_addr_t bad_dma_address;
EXPORT_SYMBOL(bad_dma_address);

void vmklnx_dma_free_coherent_by_ma(struct device *dev, size_t size, dma_addr_t handle);
void vmklnx_dma_pool_free_by_ma(struct dma_pool *pool, dma_addr_t addr);
void vmklnx_pci_free_consistent_by_ma(struct pci_dev *hwdev, size_t size, dma_addr_t dma_handle);
void vmklnx_pci_pool_free_by_ma(struct dma_pool *pool, dma_addr_t addr);

static inline vmk_DMADirection
Linux_ToVmkDMADirection(int direction)
{
   switch(direction) {
      case DMA_BIDIRECTIONAL:
         return VMK_DMA_DIRECTION_BIDIRECTIONAL;
      break;
      case DMA_TO_DEVICE:
         return VMK_DMA_DIRECTION_FROM_MEMORY;
      break;
      case DMA_FROM_DEVICE:
         return VMK_DMA_DIRECTION_TO_MEMORY;
      break;
      default:
         VMK_NOT_REACHED();
   }
   
   return VMK_DMA_DIRECTION_BIDIRECTIONAL;
}

static inline VMK_ReturnStatus
Linux_DMACheckConstraints(
   int map_op,
   struct device *hwdev, 
   vmk_MA maddr, 
   size_t length)
{
   VMK_ReturnStatus result  = VMK_OK;
   u64 dma_boundary = hwdev->dma_boundary;
   u64 dma_mask     = hwdev->dma_mask != NULL ? *(hwdev->dma_mask) : 0;

   VMK_DEBUG_ONLY (
      static u64 stress_count = 0;
      if (dma_constraint_stress_on) {
         if ((++stress_count & DMA_BOUNDARY_STRESS_MASK) == DMA_BOUNDARY_STRESS_MASK) {
            dma_boundary = 0xff;
         }
         if ((stress_count & DMA_MASK_STRESS_MASK) == DMA_MASK_STRESS_MASK) {
            dma_mask = 0xff;
         }
      }
   )

   if (unlikely(dma_mask != 0 && maddr > dma_mask)) {
      VMKLNX_WARN("Cannot map machine address = 0x%lx, length = %ld for device %s; "
	          "reason = address exceeds dma_mask (0x%llx))", 
	          maddr, length, hwdev->bus_id, dma_mask);
      atomic_inc(&dma_stats.map_op_status_counters[map_op].failures);
      atomic_inc(&dma_stats.total_mask_violations);
      result = VMK_FAILURE;
      goto out;
   }

   if (unlikely(dma_boundary != 0 && 
       (((maddr & dma_boundary) + (length - 1)) > dma_boundary))) {
      VMKLNX_WARN("Cannot map machine address = 0x%lx, length = %ld for device %s; "
	          "reason = buffer straddles device dma boundary (0x%llx)", 
	          maddr, length, hwdev->bus_id, dma_boundary);
      atomic_inc(&dma_stats.map_op_status_counters[map_op].failures);
      atomic_inc(&dma_stats.total_boundary_violations);
      result = VMK_FAILURE;
      goto out;
   }

   VMK_DEBUG_ONLY( atomic_inc(&dma_stats.map_op_status_counters[map_op].oks); )

out:
   return result;
}

/*
 * Linux_DMAMapMA --
 *
 *   Map a buffer for a given device, where the buffer is
 *   described by a machine address.  Returns a bus address.
 *
 *   @direction must be a valid direction.
 */
static dma_addr_t
Linux_DMAMapMA(struct device *hwdev,    // IN
               vmk_MA maddr,            // IN
               size_t length,           // IN
               int direction,           // IN
               vmk_Bool coherent)       // IN
{
   VMK_ReturnStatus status;
   vmk_DMAEngine engine;
   vmk_DMADirection vmkDirection;
   vmk_DMAMapErrorInfo mapErr;
   vmk_SgElem in, out;

   VMK_ASSERT(valid_dma_direction(direction));

   /*
    * In general, callers can check this themselves to avoid the function
    * call overhead.
    */
   if ((hwdev == NULL) || (hwdev->dma_identity_mapped)) {
      if (hwdev != NULL && 
          Linux_DMACheckConstraints(DMA_MAP_OP_MA,
                                    hwdev,
                                    maddr,
                                    length) == VMK_FAILURE) {
	    return bad_dma_address;
      }
      flush_write_buffers();
      return maddr;
   }

   in.addr = maddr;
   in.length = length;
   
   if (!coherent) {
      engine = (vmk_DMAEngine)hwdev->dma_engine_primary;
   } else {
      engine = (vmk_DMAEngine)hwdev->dma_engine_secondary;
   }
   
   if (engine != VMK_DMA_ENGINE_INVALID) {
      vmkDirection = Linux_ToVmkDMADirection(direction);

      status = vmk_DMAMapElem(engine, vmkDirection, &in, VMK_TRUE, &out,
                              &mapErr);
      if (status != VMK_OK) {
         /*
          * Should only get here if we're out of memory or there's a mapping
          * error because we're out of address space.
          */
         if (status == VMK_DMA_MAPPING_FAILED) {
            VMKLNX_WARN("Failed to map range 0x%lx - 0x%lx for device '%s': %s",
                        in.addr, in.addr + in.length - 1, dev_name(hwdev),
                        vmk_DMAMapErrorReasonToString(mapErr.reason));
         } else {
            VMKLNX_WARN("Failed to map range 0x%lx - 0x%lx for device '%s': %s",
                        in.addr, in.addr + in.length - 1, dev_name(hwdev),
                        vmk_StatusToString(status));
         }
         goto done;
      }

      VMK_ASSERT(out.length == in.length);
   } else {
      /*
       * No engine is defined for this device. This probably means we're
       * working with a pseudo-device or a non-PCI device.
       *
       * So, until we support non-PCI device types, assume we're doing
       * 1:1 machine/IO mappings.
       */
      out.ioAddr = in.ioAddr;
      flush_write_buffers();
   }

   return (dma_addr_t)out.ioAddr;
   
done:
   return bad_dma_address;
}

/*
 * Linux_DMAMapVA --
 *
 *   Map a buffer for a given device, where the buffer is
 *   described by a virtual address.  Returns a bus address.
 */
static dma_addr_t
Linux_DMAMapVA(struct device *hwdev,    // IN
               vmk_VA va,               // IN
               size_t length,           // IN
               int direction,           // IN
               vmk_Bool coherent)       // IN
{
   VMK_ReturnStatus status;
   vmk_MA maddr;

   status = vmk_VA2MA(va, 1, &maddr);
   if (status != VMK_OK) {
      VMKLNX_WARN("Couldn't do VA2MA on address 0x%"VMK_FMT64"x", va);
      VMK_ASSERT(status == VMK_OK);
      return bad_dma_address;
   }

   VMK_ASSERT(valid_dma_direction(direction));

   if ((hwdev == NULL) || (hwdev->dma_identity_mapped)) {
      /* Avoid the function call overhead */
      if (hwdev != NULL && 
          Linux_DMACheckConstraints(DMA_MAP_OP_VA,
                                    hwdev,
                                    maddr,
                                    length) == VMK_FAILURE) {
	    return bad_dma_address;
      }
      flush_write_buffers();
      return maddr;
   }
   else {
      return Linux_DMAMapMA(hwdev, maddr, length, direction, coherent);
   }
}

/*
 * Linux_DMAUnmap --
 *
 *   Unmaps a dma handle for a mapping previously performed via
 *   Linux_DMAMapMA() or Linux_DMAMapVA().
 */
static void
Linux_DMAUnmap(struct device *hwdev,      // IN
               dma_addr_t addr,           // IN
               size_t size,               // IN
               int direction,             // IN
               vmk_Bool coherent)         // IN
{
   VMK_ReturnStatus status;
   vmk_DMAEngine engine;
   vmk_DMADirection vmkDirection;
   vmk_SgElem elem;

   BUG_ON(!valid_dma_direction(direction));

   /*
    * Callers can avoid the function call overhead if they first check
    * the validity of the dma direction, then flush write buffers themselves.
    */
   if ((hwdev == NULL) || (hwdev->dma_identity_mapped)) {
      flush_write_buffers();
      return;
   }

   if (!coherent) {
      engine = (vmk_DMAEngine)hwdev->dma_engine_primary;
   } else {
      engine = (vmk_DMAEngine)hwdev->dma_engine_secondary;
   }

   if (engine != VMK_DMA_ENGINE_INVALID) {
      vmkDirection = Linux_ToVmkDMADirection(direction);

      elem.ioAddr = addr;
      elem.length = size;

      status = vmk_DMAUnmapElem(engine, vmkDirection, &elem);
      if (status != VMK_OK) {
         VMKLNX_WARN("Failed to unmap range for device '%s': %s",
                     dev_name(hwdev), vmk_StatusToString(status));
      }
   } else {
      /*
       * No engine is defined for this device. This probably means we're
       * working with a pseudo-device or a non-PCI device.
       *
       * So, until we support non-PCI device types, assume we're doing
       * 1:1 machine/IO mappings.
       */
      flush_write_buffers();
   }
}

/*
 * Linux_GetModuleHeapID  --
 *
 *   Return the module HeapID from the top of the module stack.
 *   This is supposed to locate the driver that is calling, but
 *   we have a hack inside to substitute the vmklinux Low heap.
 *   (see XXX comment below).
 *
 *   Consequently, no new uses of this function should be added.
 */
static vmk_HeapID 
Linux_GetModuleHeapID(void)
{
   vmk_HeapID heapID;
   vmk_ModuleID moduleID;

   moduleID = vmk_ModuleStackTop();
   VMKLNX_ASSERT_NOT_IMPLEMENTED(moduleID != VMK_INVALID_MODULE_ID);

   /*
    * The warning is logically inaccessible.  However, its too late in the
    * KL.next release cycle to safely turn this into a PSOD for production
    * builds due to the possibility of modcall wrapper outages.
    * XXX: PR 480268
    */
   if (moduleID == VMK_VMKERNEL_MODULE_ID || moduleID == vmklinuxModID ||
       (heapID = vmk_ModuleGetHeapID(moduleID)) == VMK_INVALID_HEAP_ID) {

      char name[VMK_MODULE_NAME_MAX];
      char *moduleName = name;

      if (vmk_ModuleGetName(moduleID, name, VMK_MODULE_NAME_MAX) == VMK_NOT_FOUND) {
         moduleName = "unknown";
      }

#if defined(VMX86_DEBUG)
      VMKLNX_PANIC("Using vmklnxLowHeap for module %"VMK_FMT64"x (%s)",
         vmk_ModuleGetDebugID(moduleID), moduleName);
#else
      VMKLNX_WARN("Using vmklnxLowHeap for module %"VMK_FMT64"x (%s)",
         vmk_ModuleGetDebugID(moduleID), moduleName);
#endif
      heapID = vmklnxLowHeap;
   }

   return heapID;
}

/**                                          
 *  dma_pool_create - Create a DMA memory pool for use with the given device
 *  @name: descriptive name for the pool
 *  @dev: hardware device to be used with this pool
 *  @size: size of memory blocks for this pool, in bytes
 *  @align: alignment specification for blocks in this pool.  Must 
 *          be a power of 2.
 *  @boundary: boundary constraints for blocks in this pool.  Blocks 
 *             in this pool will not cross this boundary.  Must be a 
 *             power of 2.
 *
 *  Creates an allocation pool of coherent DMA memory.  dma_pool_alloc
 *  and dma_pool_free should be used to allocate and free blocks from this pool,
 *  respectively.  Memory allocated from the pool will have DMA mappings, will 
 *  be accessible by the given device, and will be guaranteed to satisfy the 
 *  given alignment and boundary conditions.  (A boundary parameter of '0' means
 *  that there are no boundary conditions).
 *
 *  RETURN VALUE:  
 *  A pointer to the DMA pool, or NULL on failure.
 *
 *  SEE ALSO:
 *  dma_pool_destroy
 *  
 */                                          
/* _VMKLNX_CODECHECK_: dma_pool_create */
struct dma_pool *
dma_pool_create(const char *name, struct device *dev, size_t size,
   size_t align, size_t boundary)
{
   struct dma_pool *pool;
   vmk_HeapID heapID = VMK_INVALID_HEAP_ID;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if (align == 0) {
      align = SMP_CACHE_BYTES;
   } else if ((align & (align - 1)) != 0) {
      return NULL;
   }

   if (boundary != 0 && (boundary < size || (boundary & (boundary - 1)) != 0)) {
      return NULL;
   }

   if (size == 0) {
      return NULL;
   } else if (align < size) {
      // guarantee power of 2
      align = size;
      if ((align & (align - 1)) != 0) {
         align = 1 << fls(align);
      }
      VMK_ASSERT(align >= size);
      VMK_ASSERT(align < size * 2);
      VMK_ASSERT((align & (align - 1)) == 0);
   }

   if (!(pool = kmalloc(sizeof *pool, GFP_KERNEL)))
      return pool;

   strlcpy (pool->name, name, sizeof pool->name);
   pool->dev = dev;
   pool->size = size;
   pool->align = align;
   if (dev != NULL) {
      heapID = (vmk_HeapID) dev->dma_mem;
   }
   pool->heapId = (heapID == VMK_INVALID_HEAP_ID) ? Linux_GetModuleHeapID() :
                  heapID;
#ifdef VMX86_DEBUG
   pool->coherent_dma_mask = (dev == NULL) ? DMA_32BIT_MASK :
                             dev->coherent_dma_mask;
#endif

   return pool;
}
EXPORT_SYMBOL(dma_pool_create);

/**                                          
 *  dma_pool_destroy - Destroy a DMA pool       
 *  @pool: pool to be destroyed    
 *                                           
 *  Destroys the DMA pool.  Use this only after all memory has been given
 *  back to the pool using dma_pool_free.  The caller must guarantee that
 *  the memory will not be used again.
 * 
 *  ESX Deviation Notes:                                
 *  dma_pool_destroy will not free memory that has been allocated by
 *  dma_pool_alloc.  It is the caller's responsibility to make sure that the
 *  memory has been freed before calling dma_pool_destroy.
 *
 *  RETURN VALUE:
 *  Does not return any value
 *
 *  SEE ALSO:
 *  dma_pool_create, dma_pool_free
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: dma_pool_destroy */
void 
dma_pool_destroy(struct dma_pool *pool)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   kfree(pool);
}
EXPORT_SYMBOL(dma_pool_destroy);

/**                                          
 *  dma_pool_alloc - Allocate a block of memory from the dma pool
 *  @pool: pool object from which to allocate
 *  @mem_flags: flags to be used in allocation (see deviation notes)
 *  @handle: output bus address for the allocated memory
 *                               
 *  Allocates a physically and virtually contiguous region of memory having the 
 *  size and alignment characteristics specified for the pool.  The memory is 
 *  set up for DMA with the pool's hardware device.  The memory's virtual 
 *  address is returned, and the bus address is passed back through handle.  If
 *  allocation fails, NULL is returned.
 *                                          
 *  ESX Deviation Notes:                     
 *  mem_flags is ignored on ESX.  The semantics used are those
 *  of mem_flags = GFP_KERNEL.
 *
 *  RETURN VALUE:
 *  Virtual-address of the allocated memory, NULL on allocation failure
 *
 *  SEE ALSO:
 *  dma_pool_free
 *
 */                                          
/* _VMKLNX_CODECHECK_: dma_pool_alloc */
void *
dma_pool_alloc(struct dma_pool *pool, gfp_t mem_flags, dma_addr_t *handle)
{
   void *va;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT((pool->align & (pool->align - 1)) == 0);
   VMK_ASSERT(pool->align >= pool->size);

   va = vmklnx_kmalloc_align(pool->heapId, pool->size, pool->align, mem_flags);

   if (va) {
      if (handle) {
         *handle = Linux_DMAMapVA(pool->dev, (vmk_VA)va, pool->size,
                                  DMA_BIDIRECTIONAL, VMK_TRUE);

         if (unlikely(*handle == bad_dma_address)) {
            vmklnx_kfree(pool->heapId, va);
	    va = NULL;
         }
      }
   }

   return va;
}
EXPORT_SYMBOL(dma_pool_alloc);

/**                                          
 *  vmklnx_dma_pool_free_by_ma - Give back memory to the DMA pool      
 *  @pool: DMA pool structure    
 *  @addr: machine address of the memory being given back
 *                                           
 *  Frees memory that was allocated by the 
 *  DMA pool, given that memory's machine address.  The memory MUST have
 *  been allocated from a contiguous heap.  In general, this function should
 *  not be used, and instead drivers should track the original virtual address
 *  that was given by dma_pool_alloc, and then use that in dma_pool_free
 *                                           
 *  ESX Deviation Notes:                     
 *  This function does not appear in Linux and is provided for drivers that
 *  expect phys_to_virt() to provide an identity mapping back to an original
 *  memory-allocation address in order to later free it.  phys_to_virt does not
 *  work that way on ESX.
 *
 *  RETURN VALUE:
 *  Does not return any value
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: vmklnx_dma_pool_free_by_ma */
void vmklnx_dma_pool_free_by_ma(struct dma_pool *pool, dma_addr_t addr)
{
   if ((pool->dev != NULL) && (pool->dev->dma_identity_mapped)) {
      flush_write_buffers();
   } else {
      Linux_DMAUnmap(pool->dev, addr, pool->size, DMA_BIDIRECTIONAL, VMK_TRUE);
   }
   vmk_HeapFreeByMA(pool->heapId, addr);
}

/**                                          
 *  vmklnx_pci_pool_free_by_ma - Give back memory to the DMA pool      
 *  @pool: DMA pool structure    
 *  @addr: machine address of the memory being given back
 *                                           
 *  Frees memory that was allocated by the 
 *  DMA pool, given that memory's machine address.  The memory MUST have
 *  been allocated from a contiguous heap.  In general, this function should
 *  not be used, and instead drivers should track the original virtual address
 *  that was given by dma_pool_alloc, and then use that in dma_pool_free
 *                                           
 *  ESX Deviation Notes:                     
 *  This function does not appear in Linux and is provided for drivers that
 *  expect phys_to_virt() to provide an identity mapping back to an original
 *  memory-allocation address in order to later free it.  phys_to_virt does not
 *  work that way on ESX.
 *
 *  RETURN VALUE:
 *  Does not return any value
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: vmklnx_pci_pool_free_by_ma */
void
vmklnx_pci_pool_free_by_ma(struct dma_pool *pool, dma_addr_t addr)
{
   vmklnx_dma_pool_free_by_ma(pool, addr);
}

/**                                          
 *  dma_pool_free - Give back memory to the DMA pool       
 *  @pool: DMA pool structure    
 *  @vaddr: virtual address of the memory being returned
 *  @addr: machine address of the memory being returned
 *                                           
 *  Frees memory that was allocated by the DMA pool.  The
 *  virtual address given must match that which was returned originally
 *  by dma_pool_alloc.
 *
 *  RETURN VALUE:
 *  Does not return any value
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: dma_pool_free */
void 
dma_pool_free(struct dma_pool *pool, void *vaddr, dma_addr_t addr)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if ((pool->dev != NULL) && (pool->dev->dma_identity_mapped)) {
      flush_write_buffers();
   } else {
      Linux_DMAUnmap(pool->dev, addr, pool->size, DMA_BIDIRECTIONAL, VMK_TRUE);
   }
   vmklnx_kfree(pool->heapId, vaddr);
}
EXPORT_SYMBOL(dma_pool_free);

/**                                          
 *  dma_alloc_coherent - Allocate memory for DMA use with the given device       
 *  @dev: device to be used in the DMA - may be NULL    
 *  @size: size of the requested memory, in bytes
 *  @handle: Output parameter to hold the bus address for the allocated memory
 *  @gfp: Flags for the memory allocation
*                                           
 *  Allocates a physically contiguous region of memory
 *  and sets up the physical bus connectivity to enable DMA between the
 *  given device and the allocated memory.  The bus address for the allocated
 *  memory is returned in "handle".  This bus address is usable by
 *  the device in DMA operations.  If no device is given (ie, dev = NULL),
 *  dma_alloc_coherent allocates physically contiguous memory that may be
 *  anywhere in the address map, and no specific bus accomodations are made.
 *  Generally this only is usable on systems that do not have an IOMMU.
 *                                           
 *  ESX Deviation Notes:                     
 *  "gfp" is ignored.  dma_alloc_coherent behaves as though "gfp" is always
 *  GFP_KERNEL.  Zone specifiers for gfp are ignored, but the 
 *  dev->coherent_dma_mask is obeyed (see pci_set_consistent_dma_mask).  The 
 *  memory associated with the device (dev->dma_mem) must be suitable for the
 *  requested dma_alloc_coherent allocation.
 *
 *  RETURN VALUE:
 *  Returns a pointer to the allocated memory on success, NULL on failure
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: dma_alloc_coherent */
void *
dma_alloc_coherent(struct device *dev, size_t size, dma_addr_t *handle,
                   gfp_t gfp)
{
   void *ret;
   vmk_HeapID heapID = VMK_INVALID_HEAP_ID;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   /*
    * Preferentially use the coherent dma heap in dev->dma_mem.
    * If its not set, use the module heap.
    *
    * For most drivers, these two are the same heap.  Only drivers
    * with special requirements end up with a distict coherent dma heap.
    */
   if (dev != NULL) {
      heapID = (vmk_HeapID) dev->dma_mem;
   }
   if (heapID == VMK_INVALID_HEAP_ID) {
      heapID = Linux_GetModuleHeapID();
   }

#if defined(VMX86_DEBUG)
   {
      vmk_HeapGetProps props;
      VMK_ReturnStatus status;

      status = vmk_HeapGetProperties(heapID, &props);
      VMK_ASSERT(status == VMK_OK);
      VMK_ASSERT(props.physContiguity == VMK_MEM_PHYS_CONTIGUOUS);

      if (dev != NULL) {
         u64 codma_mask = dev->coherent_dma_mask;
         if (codma_mask < dma_get_required_mask(dev)) {
            if (codma_mask >= DMA_39BIT_MASK) {
               VMK_ASSERT((props.physRange == VMK_PHYS_ADDR_BELOW_512GB) ||
                          (props.physRange == VMK_PHYS_ADDR_BELOW_4GB) ||
                          (props.physRange == VMK_PHYS_ADDR_BELOW_2GB));
            } else if (codma_mask >= DMA_32BIT_MASK) {
               VMK_ASSERT((props.physRange == VMK_PHYS_ADDR_BELOW_4GB) ||
                          (props.physRange == VMK_PHYS_ADDR_BELOW_2GB));
            } else if ( codma_mask == DMA_31BIT_MASK) {
                VMK_ASSERT(props.physRange == VMK_PHYS_ADDR_BELOW_2GB);
            } else {
               VMK_ASSERT(0);
            }
         }
      }
   }
#endif /* defined(VMX86_DEBUG) */

   ret = vmk_HeapAlign(heapID, VMK_PAGE_SIZE << get_order(size), PAGE_SIZE);
   if (ret == NULL) {
      VMKLNX_WARN("Out of memory");
   } else {
      memset(ret, 0, size);
      if (handle != NULL) {
         *handle = Linux_DMAMapVA(dev, (vmk_VA)ret, size,
                                  DMA_BIDIRECTIONAL, VMK_TRUE);

         if (unlikely(*handle == bad_dma_address)) {
            vmk_HeapFree(heapID, ret);
	    ret = NULL;
         }
      }
   }

   return ret;
}
EXPORT_SYMBOL(dma_alloc_coherent);

/**                                          
 *  dma_free_coherent - Free memory that was set up for DMA for the given device       
 *  @dev: device that was used in the DMA   
 *  @size: size of the memory being freed, in bytes   
 *  @vaddr: virtual address of the memory being freed
 *  @handle: bus address of the memory being freed
 *                                           
 *  Frees the given memory and tears down any special bus
 *  connectivity that was needed to make this memory reachable by the given
 *  device.  If the given device is NULL, the memory is just returned to the
 *  module's memory pool.
 *                                           
 *  ESX Deviation Notes:                     
 *  It is valid on some Linux configurations to call dma_free_coherent on 
 *  a subset region of memory that was part of a larger dma_alloc_coherent
 *  allocation.  This is not valid on ESX.  You must free the entire size
 *  (with the base vaddr and handle) that was obtained with dma_alloc_coherent.
 *
 *  RETURN VALUE:
 *  Does not return any value
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: dma_free_coherent */
void 
dma_free_coherent(struct device *dev, size_t size, void *vaddr, dma_addr_t handle)
{
   vmk_HeapID heapID = VMK_INVALID_HEAP_ID;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if ((dev != NULL) && (dev->dma_identity_mapped)) {
      flush_write_buffers();
   } else {
      Linux_DMAUnmap(dev, handle, size, DMA_BIDIRECTIONAL, VMK_TRUE);
   }

   if(dev != NULL) {
      heapID = (vmk_HeapID) dev->dma_mem;
   }

   if (heapID == VMK_INVALID_HEAP_ID) {
        heapID = Linux_GetModuleHeapID();
   }

   vmk_HeapFree(heapID, vaddr);
}
EXPORT_SYMBOL(dma_free_coherent);

/**                                          
 *  vmklnx_dma_free_coherent_by_ma - Free memory that was set up for DMA for the given device       
 *  @dev: device that was used in the DMA   
 *  @size: size of the memory being freed, in bytes   
 *  @handle: bus address of the memory being freed
 *                                           
 *  Frees the given memory and tears down any special bus
 *  connectivity that was needed to make this memory reachable by the given
 *  device.  If the given device is NULL, the memory is just returned to the
 *  module's memory pool.
 *                                           
 *  ESX Deviation Notes:                     
 *  It is valid on some Linux configurations to call dma_free_coherent on 
 *  a subset region of memory that was part of a larger dma_alloc_coherent
 *  allocation.  This is not valid on ESX.  You must free the entire size
 *  that was obtained with dma_alloc_coherent.
 *  This function does not appear in Linux and is provided for drivers that
 *  expect phys_to_virt() to provide an identity mapping back to an original
 *  memory-allocation address in order to later free it.  phys_to_virt does not 
 *  work that way on ESX.
 *
 *  RETURN VALUE:
 *  Does not return any value
 *
 */                                          
/* _VMKLNX_CODECHECK_: vmklnx_dma_free_coherent_by_ma */
void
vmklnx_dma_free_coherent_by_ma(struct device *dev, size_t size, dma_addr_t handle)
{
   vmk_HeapID heapID = VMK_INVALID_HEAP_ID;
   
   if ((dev != NULL) && (dev->dma_identity_mapped)) {
      flush_write_buffers();
   } else {
      Linux_DMAUnmap(dev, handle, size, DMA_BIDIRECTIONAL, VMK_TRUE);
   }
   
   if (dev != NULL) {
      heapID = (vmk_HeapID) dev->dma_mem;
   }
   
   if (heapID == VMK_INVALID_HEAP_ID) {
      heapID = Linux_GetModuleHeapID();
   }
   
   vmk_HeapFreeByMA(heapID, handle);
}

/**                                          
 *  vmklnx_pci_free_consistent_by_ma - Free memory that was set up for DMA for the given device       
 *  @hwdev: PCI device that was used in the DMA   
 *  @size: size of the memory being freed, in bytes   
 *  @handle: bus address of the memory being freed
 *                                           
 *  Frees the given memory and tears down any special bus
 *  connectivity that was needed to make this memory reachable by the given
 *  device.  If the given device is NULL, the memory is just returned to the
 *  module's memory pool.
 *                                           
 *  ESX Deviation Notes:                     
 *  It is valid on some Linux configurations to call dma_free_coherent on 
 *  a subset region of memory that was part of a larger dma_alloc_coherent
 *  allocation.  This is not valid on ESX.  You must free the entire size
 *  that was obtained with dma_alloc_coherent.
 *  This function does not appear in Linux and is provided for drivers that
 *  expect phys_to_virt() to provide an identity mapping back to an original
 *  memory-allocation address in order to later free it.  phys_to_virt does not 
 *  work that way on ESX.
 * 
 *  RETURN VALUE:
 *  Does not return any value
 *
 */                                          
/* _VMKLNX_CODECHECK_: vmklnx_pci_free_consistent_by_ma */
void
vmklnx_pci_free_consistent_by_ma(struct pci_dev *hwdev, size_t size, dma_addr_t handle)
{
   struct device *dev = NULL;
   if (hwdev != NULL) {
      dev = &hwdev->dev;
   }
   vmklnx_dma_free_coherent_by_ma(dev, size, handle);   
}

static struct dma_mapping_ops vmklnx_dma_ops;
struct dma_mapping_ops* dma_ops = &vmklnx_dma_ops;
EXPORT_SYMBOL(dma_ops);

int 
vmklnx_dma_supported(struct vmklnx_codma *codma, struct device *dev, u64 mask)
{     
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   return 1;
}
EXPORT_SYMBOL(vmklnx_dma_supported);

/**                                          
 *  dma_mapping_error - Check a bus address for a mapping error       
 *  @dma_addr: bus address previously returned by dma_map_single or dma_map_page
 *                                           
 *  Performs a platform-specific check to determine if the
 *  mapped bus address is valid for use with DMA
 *                                           
 *  RETURN VALUE:
 *  TRUE if the bus address incurred a mapping error, FALSE otherwise
 *
 *  SEE ALSO:
 *  dma_map_single
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: dma_mapping_error */
int dma_mapping_error(dma_addr_t dma_addr)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   return (dma_addr == bad_dma_address);
}
EXPORT_SYMBOL(dma_mapping_error);

/**                                          
 *  dma_map_single - Map a buffer for streaming DMA use with a given device       
 *  @hwdev: device to be used in the DMA operation    
 *  @ptr: virtual address of the buffer
 *  @size: length of the buffer, in bytes
 *  @direction: direction of the DMA to set up
 *                                           
 *  Sets up any platform-specific bus connectivity required to
 *  make a buffer usable for a DMA operation and returns a mapped bus address
 *  for the buffer.  The mapped address should be checked for an error using 
 *  dma_mapping_error.  When the buffer will no longer be used for DMA, the 
 *  buffer should be unmapped using dma_unmap_single.
 *  'direction' can be any one of
 *  DMA_BIDIRECTIONAL (the device either reads or writes the buffer),
 *  DMA_TO_DEVICE (the device reads the buffer), 
 *  DMA_FROM_DEVICE (the device writes the buffer), or
 *  DMA_NONE (neither reads nor writes should be allowed - may not be supported
 *  on all platforms)
 *
 *  RETURN VALUE:
 *  A bus address accessible by the device
 *
 *  SEE ALSO:
 *  dma_unmap_single, dma_mapping_error
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: dma_map_single */
dma_addr_t
dma_map_single(struct device *hwdev, void *ptr, size_t size,
	       int direction)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   /* XXX This can go away when the mapping code really does something. */
   VMK_ASSERT(vmklnx_is_physcontig(ptr, size));

   BUG_ON(!valid_dma_direction(direction));

   return Linux_DMAMapVA(hwdev, (vmk_VA)ptr, size, direction, VMK_FALSE);
}
EXPORT_SYMBOL(dma_map_single);

/**                                          
 *  dma_map_page
 *  @hwdev: device to be used in the DMA operation    
 *  @page: page that buffer resides in
 *  @offset: offset into page for start of buffer
 *  @size: length of the buffer, in bytes
 *  @direction: direction of the DMA to set up
 *                                           
 *  Sets up any platform-specific bus connectivity required to
 *  make a buffer usable for a DMA operation and returns a mapped bus address
 *  for the buffer.  The mapped address should be checked for an error using 
 *  dma_mapping_error.  When the buffer will no longer be used for DMA, the 
 *  buffer should be unmapped using dma_unmap_single.
 *  'direction' can be any one of
 *  DMA_BIDIRECTIONAL (the device either reads or writes the buffer),
 *  DMA_TO_DEVICE (the device reads the buffer), 
 *  DMA_FROM_DEVICE (the device writes the buffer), or
 *  DMA_NONE (neither reads nor writes should be allowed - may not be supported
 *  on all platforms)
 *
 *  RETURN VALUE:
 *  A bus address accessible by the device
 *
 *  SEE ALSO:
 *  dma_unmap_page, dma_mapping_error
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: dma_map_page */
dma_addr_t
dma_map_page(struct device *hwdev, struct page *page, unsigned long offset,
 	     size_t size, int direction)
{
   vmk_MA maddr;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   BUG_ON(!valid_dma_direction(direction));

   maddr = (page_to_phys(page)) + offset;
   if ((hwdev != NULL) && (hwdev->dma_identity_mapped)) {
      if (Linux_DMACheckConstraints(DMA_MAP_OP_PAGE,
                                    hwdev,
                                    maddr,
                                    size) == VMK_FAILURE) {
	    return bad_dma_address;
      }
      flush_write_buffers();
      return maddr;
   }
   
   return Linux_DMAMapMA(hwdev, maddr, size, direction, VMK_FALSE);
}
EXPORT_SYMBOL(dma_map_page);

/**                                          
 *  dma_unmap_single - Tear down a streaming DMA mapping for a buffer       
 *  @hwdev: device that had been used in the DMA operation     
 *  @addr: mapped bus address for the buffer, previously returned by dma_map_single
 *  @size: length of the buffer, in bytes
 *  @direction: direction of the DMA that was set up by dma_map_single
 *                                           
 *  Tears down the platform-specific bus connectivity that was needed to make a 
 *  buffer usable for DMA.  
 *  'direction' can be any one of
 *  DMA_BIDIRECTIONAL (the device either reads or writes the buffer),
 *  DMA_TO_DEVICE (the device reads the buffer), 
 *  DMA_FROM_DEVICE (the device writes the buffer), or
 *  DMA_NONE (neither reads nor writes should be allowed)
 *
 *  RETURN VALUE:
 *  Does not return any value
 *                                           
 *  SEE ALSO:
 *  dma_map_single
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: dma_unmap_single */
void
dma_unmap_single(struct device *hwdev, dma_addr_t addr, size_t size,
                 int direction)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if ((hwdev != NULL) && (hwdev->dma_identity_mapped)) {
      BUG_ON(!valid_dma_direction(direction));
      flush_write_buffers();
   } else {
      Linux_DMAUnmap(hwdev, addr, size, direction, VMK_FALSE);
   }
}
EXPORT_SYMBOL(dma_unmap_single);

static void
Linux_DmaFlushSingleCommon(struct device *hwdev, dma_addr_t dma_handle,
			   unsigned long offset, size_t size, int direction)
{
   VMK_ReturnStatus status;
   vmk_DMAEngine engine;
   vmk_DMADirection vmkDirection;
   vmk_SgElem elem;

   BUG_ON(!valid_dma_direction(direction));

   if ((hwdev == NULL) || (hwdev->dma_identity_mapped)) {
      flush_write_buffers();
      return;
   }

   engine = (vmk_DMAEngine)hwdev->dma_engine_primary;
   
   if (engine != VMK_DMA_ENGINE_INVALID) {
      vmkDirection = Linux_ToVmkDMADirection(direction);
      
      elem.ioAddr = dma_handle + offset;
      elem.length = size;
      
      status = vmk_DMAFlushElem(engine, direction, &elem);
      if (status != VMK_OK) {
         VMKLNX_WARN("Failed to flush range for device '%s': %s",
                     dev_name(hwdev), vmk_StatusToString(status));
         VMK_ASSERT(status == VMK_OK);
      }
   } else {
      /*
       * No engine is defined for this device. This probably means we're
       * working with a pseudo-device or a non-PCI device.
       *
       * So, until we support non-PCI device types, assume we're doing
       * 1:1 machine/IO mappings.
       */
      flush_write_buffers();
   }
}

/**                                          
 *  dma_sync_single_for_cpu - Allow the CPU to access a buffer that is currently DMA-mapped      
 *  @hwdev: device to which the buffer is mapped    
 *  @dma_handle: bus address of the buffer
 *  @size: length of the buffer, in bytes
 *  @direction: direction of the existing DMA mapping
 *
 *  Transfers access ownership for a buffer that has been set up for DMA back to
 *  the CPU and synchronizes any changes that have been made by the device with
 *  the CPU.  The bus mapping that was created with dma_map_single is not 
 *  destroyed.  Afterward, the CPU can safely read and write the buffer.  The
 *  device should not access the buffer until access rights have been 
 *  transferred back to the device using dma_sync_single_for_device.
 *
 *  RETURN VALUE:
 *  Does not return any value
 *
 *  SEE ALSO:
 *  dma_sync_single_for_device, dma_map_single
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: dma_sync_single_for_cpu */
void
dma_sync_single_for_cpu(struct device *hwdev, dma_addr_t dma_handle,
			size_t size, int direction)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   Linux_DmaFlushSingleCommon(hwdev, dma_handle, 0, size, direction);
}
EXPORT_SYMBOL(dma_sync_single_for_cpu);

/**                                          
 *  dma_sync_single_for_device - Re-enable device access to a DMA-mapped buffer     
 *  @hwdev: device to which the buffer is mapped    
 *  @dma_handle: bus address of the buffer
 *  @size: length of the buffer, in bytes
 *  @direction: direction of the existing DMA mapping
 *                                           
 *  Transfers access ownership back to a device from the CPU and synchronizes 
 *  any changes that the CPU has made so that they will be visible by the device.
 *
 *  RETURN VALUE:
 *  Does not return any value
 *
 *  SEE ALSO:
 *  dma_sync_single_for_cpu, dma_map_single
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: dma_sync_single_for_device */
void
dma_sync_single_for_device(struct device *hwdev, dma_addr_t dma_handle,
			   size_t size, int direction)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   Linux_DmaFlushSingleCommon(hwdev, dma_handle, 0, size, direction);
}
EXPORT_SYMBOL(dma_sync_single_for_device);

void
dma_sync_single_range_for_cpu(struct device *hwdev, dma_addr_t dma_handle,
			      unsigned long offset, size_t size, int direction)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   Linux_DmaFlushSingleCommon(hwdev, dma_handle, offset, size, direction);
}
EXPORT_SYMBOL(dma_sync_single_range_for_cpu);

void
dma_sync_single_range_for_device(struct device *hwdev, dma_addr_t dma_handle,
				 unsigned long offset, size_t size, int direction)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   Linux_DmaFlushSingleCommon(hwdev, dma_handle, offset, size, direction);
}
EXPORT_SYMBOL(dma_sync_single_range_for_device);

static void
Linux_DmaFlushSgCommon(struct device *hwdev, struct scatterlist *sg,
		       int nelems, int direction)
{
   VMK_ReturnStatus status;
   vmk_DMAEngine engine;
   vmk_DMADirection vmkDirection;
   vmk_ByteCount flushLen;
   vmk_sgelem *elem;
   int i;

   BUG_ON(!valid_dma_direction(direction));

   if ((hwdev == NULL) || (hwdev->dma_identity_mapped)) {
      flush_write_buffers();
      return;
   }

   if (!sg->vmkIOsga) {
      return;
   }

   engine = (vmk_DMAEngine)hwdev->dma_engine_primary;
   
   if (engine != VMK_DMA_ENGINE_INVALID) {
      vmkDirection = Linux_ToVmkDMADirection(direction);
   
      if (sg->vmkIOsga->numElems == nelems) {
         /* The common case. Just flush everything. */
         flushLen = VMK_DMA_FLUSH_SG_ALL;
      } else {
         /* Flush part of the array. This is less common. */
         flushLen = 0;
         elem = sg->vmksgel;
         for(i=0; i<nelems; i++) {
            flushLen += elem->length;
         }
      }

      status = vmk_DMAFlushSg(engine, vmkDirection, sg->vmkIOsga, 0, flushLen);
      if (status != VMK_OK) {
         VMKLNX_INFO("Failed to flush sg on device '%s': %s", dev_name(hwdev),
                     vmk_StatusToString(status));
         VMK_ASSERT(status == VMK_OK);
      }
   } else {
      /*
       * No engine is defined for this device. This probably means we're
       * working with a pseudo-device or a non-PCI device.
       *
       * So, until we support non-PCI device types, assume we're doing
       * 1:1 machine/IO mappings.
       */
      flush_write_buffers();
   }
}

void
dma_sync_sg_for_cpu(struct device *hwdev, struct scatterlist *sg,
		    int nelems, int direction)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   Linux_DmaFlushSgCommon(hwdev, sg, nelems, direction);
}
EXPORT_SYMBOL(dma_sync_sg_for_cpu);

void
dma_sync_sg_for_device(struct device *hwdev, struct scatterlist *sg,
		       int nelems, int direction)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   Linux_DmaFlushSgCommon(hwdev, sg, nelems, direction);
}
EXPORT_SYMBOL(dma_sync_sg_for_device);

/**                                          
 *  dma_map_sg - Map scatter/gather buffers for DMA use with a hardware device  
 *  @hwdev: device to be used in the DMA operations    
 *  @sg: start of the scatter/gather list of entries to be mapped
 *  @nents: number of elements in the list to be mapped
 *  @direction: direction of the DMA, with values as in dma_map_single
 *                                           
 *  Sets up the platform-specific bus connectivity for each of the buffers in a 
 *  scatterlist so that they may be used in DMA with the given hardware device.
 *  dma_unmap_sg should be used on these scatterlist elements when they will no
 *  longer be used with DMA.
 *                                           
 *  RETURN VALUE:
 *  0 if a failure was encountered.
 *  A value greater than zero but less than or equal to nents if the
 *  mappings succeeded
 *
 *  SEE ALSO:
 *  dma_map_single, dma_unmap_sg
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: dma_map_sg */
int
dma_map_sg(struct device *hwdev, struct scatterlist *sg, int nents,
           int direction)
{
   VMK_ReturnStatus status;
   vmk_DMAEngine engine;
   vmk_DMADirection vmkDirection;
   vmk_DMAMapErrorInfo mapErr;
   int i;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   BUG_ON(!valid_dma_direction(direction));

   if (sg->premapped) {
      /*
       * If the SG array is premapped, there's nothing to do.
       *
       * Almost any fast-path IO operation should hit this case.
       */
      return nents;
   }

   VMK_ASSERT(sg->vmksgel != NULL);
   
   /*
    * Since this is likely not on an IO fast-path and will likely
    * only contain perhaps a small number of elements, just allocate
    * a new SG array for the machine ranges and use that to map
    * in the addresses.
    */
   status = vmk_SgAlloc(vmklnx_dma_sgops, &(sg->vmksga), nents);
   if (status != VMK_OK) {
      VMKLNX_WARN("Couldn't allocate SG array for device '%s': %s",
                  dev_name(hwdev), vmk_StatusToString(status));
      VMK_ASSERT(status == VMK_OK);
      goto out;
   }
   
   /*
    * XXX
    * Look at making the elem array in the vmk_SGArray a pointer so
    * we can avoid an actual copy here...though, if this isn't a
    * fast-path it may not matter.
    */
   if (hwdev != NULL && hwdev->dma_identity_mapped) {
      for(i=0; i<nents; i++) {
         vmk_MA maddr = sg->vmksgel[i].addr;
	 vmk_ByteCountSmall length = sg->vmksgel[i].length;

         if (Linux_DMACheckConstraints(DMA_MAP_OP_SG,
                                       hwdev,
                                       maddr,
                                       length) == VMK_FAILURE) {
	    goto outFreeSg;
	 }
         sg->vmksga->elem[i] = sg->vmksgel[i];
      }
   } else {
      for(i=0; i<nents; i++) {
         sg->vmksga->elem[i] = sg->vmksgel[i];
      }
   }

   sg->vmksga->numElems=(vmk_uint32)nents;
   
   if (hwdev && (!hwdev->dma_identity_mapped)) {
      engine = (vmk_DMAEngine)hwdev->dma_engine_primary;
   } else {
      engine = VMK_DMA_ENGINE_INVALID;
   }
   
   if (engine != VMK_DMA_ENGINE_INVALID) {
      vmkDirection = Linux_ToVmkDMADirection(direction);

      status = vmk_DMAMapSg(engine, vmkDirection, vmklnx_dma_sgops, sg->vmksga,
                            &(sg->vmkIOsga), &mapErr);
      if (status != VMK_OK) {
         /*
          * Should only get here if we're out of memory or there's a mapping
          * error because we're out of address space.
          */
         if (status == VMK_DMA_MAPPING_FAILED) {
            VMKLNX_WARN("Failed to map SG array for device '%s': %s",
                        dev_name(hwdev),
                        vmk_DMAMapErrorReasonToString(mapErr.reason));
         } else {
            VMKLNX_WARN("Failed to map SG array for device '%s': %s",
                        dev_name(hwdev), vmk_StatusToString(status));
         }
         VMK_ASSERT(status == VMK_OK);
         goto outFreeSg;
      }
   } else {
      /*
       * Either identity mapped, or no engine is defined for this 
       * device. 
       *
       * For non-PCI devices, identity mapped, and pseudo devices, 
       * apply 1:1 machine/IO mappings.
       */
      sg->vmkIOsga = sg->vmksga;
      flush_write_buffers();
   }
   sg->vmkIOsgel = sg->curIOsgel = &(sg->vmkIOsga->elem[0]);
   
   return nents;

outFreeSg:
   vmk_SgFree(vmklnx_dma_sgops, sg->vmksga);
   sg->vmksga = NULL;

out:
   return 0;
}
EXPORT_SYMBOL(dma_map_sg);

/**                                          
 *  dma_unmap_sg - Unmap scatter/gather buffers that were previously mapped for DMA
 *  @hwdev: device to which these buffers have been mapped    
 *  @sg: start of the scatter/gather list of entries to be unmapped
 *  @nents: number of elements in the list to be unmapped
 *  @direction: direction of the existing DMA mapping
 *                                           
 *  Tears down the platform-specific bus connectivity for each of the buffers in 
 *  a scatterlist that had been previously set up for DMA using
 *  dma_map_sg.
 *
 *  RETURN VALUE:
 *  Does not return any value
 *
 *  SEE ALSO:
 *  dma_map_sg, dma_map_single  
 *
 */                                          
/* _VMKLNX_CODECHECK_: dma_unmap_sg */
void
dma_unmap_sg(struct device *hwdev, struct scatterlist *sg, int nents,
	     int direction)
{
   VMK_ReturnStatus status;
   vmk_DMAEngine engine;
   vmk_DMADirection vmkDirection;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   BUG_ON(!valid_dma_direction(direction));
   
   /* If the SG array was pre-mapped. There's nothing to do. */
   if (sg->premapped) {
      return;
   }
   
   if (sg->vmkIOsga == NULL) {
      VMKLNX_PANIC("Passed non-premapped SG array without IO sg array on "
                   "device '%s'", dev_name(hwdev));
   }

   if (hwdev && (!hwdev->dma_identity_mapped)) {
      engine = (vmk_DMAEngine)hwdev->dma_engine_primary;
   } else {
      engine = VMK_DMA_ENGINE_INVALID;
   }
   
   if (engine != VMK_DMA_ENGINE_INVALID) {
      vmkDirection = Linux_ToVmkDMADirection(direction);

      status = vmk_DMAUnmapSg(engine, vmkDirection, vmklnx_dma_sgops,
                              sg->vmkIOsga);
      if (status != VMK_OK) {
         VMKLNX_WARN("Failed unmap on device '%s': %s", dev_name(hwdev),
                     vmk_StatusToString(status));
         VMK_ASSERT(status == VMK_OK);
      }
   } else {
      flush_write_buffers();
   }

   vmk_SgFree(vmklnx_dma_sgops, sg->vmksga);
   
   /* Make sure the SG array is clean for a new mapping. */
   sg->vmksga = NULL;
   sg->vmkIOsga = NULL;
   sg->vmkIOsgel = NULL;
   sg->curIOsgel = NULL;
}
EXPORT_SYMBOL(dma_unmap_sg);

/**
 *  dma_set_mask - Set a device's allowable DMA mask
 *  @dev: device whose mask is being set
 *  @mask: address bitmask
 *
 *  Sets the range in which the device can perform DMA operations.
 *  The mask, when bitwise-ANDed with an arbitrary machine address, expresses
 *  the DMA addressability of the device.  This mask is used by dma_mapping
 *  functions (ie, dma_alloc_coherent, dma_pool_alloc) to guarantee that the
 *  memory allocated is usable by the device.
 *
 *  ESX Deviation Notes:                                
 *  dma_set_mask is only valid for PCI-based devices.
 *
 *  RETURN VALUE:
 *  0 on success
 *  -EIO if the given mask cannot be used for DMA on the system, or if the
 *  dma_mask has not been previously initialized.
 *
 */
/* _VMKLNX_CODECHECK_: dma_set_mask */
int
dma_set_mask(struct device *dev, u64 mask)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   /* For now, we only support DMA on PCI devices */
   BUG_ON(dev->dev_type != PCI_DEVICE_TYPE);
   return pci_set_dma_mask(to_pci_dev(dev), mask);
}
EXPORT_SYMBOL(dma_set_mask);

void
LinuxDMA_Init()
{
   VMK_ReturnStatus status;

   VMKLNX_CREATE_LOG();
   status = vmk_SgCreateOpsHandle(VMK_MODULE_HEAP_ID, &vmklnx_dma_sgops,
                                  NULL, NULL);
   VMK_ASSERT(status == VMK_OK);
}

void
LinuxDMA_Cleanup()
{
   vmk_SgDestroyOpsHandle(vmklnx_dma_sgops);
   VMKLNX_DESTROY_LOG();
}
