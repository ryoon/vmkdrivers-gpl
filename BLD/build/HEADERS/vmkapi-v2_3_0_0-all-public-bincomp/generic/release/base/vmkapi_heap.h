/* **********************************************************
 * Copyright 1998 - 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Heap                                                           */ /**
 * \addtogroup Core
 * @{
 * \defgroup Heap Heaps
 *
 * vmkernel has local heaps to help isolate VMKernel subsystems from
 * one another. Benefits include:
 * \li Makes it easier to track per-subssystem memory consumption,
 *     enforce a cap on  how much memory a given subsystem can allocate,
 *     locate the origin of memory leaks, ...
 * \li Confines most memory corruptions to the guilty subsystem.
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_HEAP_H_
#define _VMKAPI_HEAP_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#define VMK_INVALID_HEAP_ID NULL

typedef struct Heap* vmk_HeapID;

/**
 * \brief Types of heaps.
 */
typedef enum {
   /** 
    * \brief Heaps that get their own memory pool.
    *
    * Heaps of type VMK_HEAP_TYPE_SIMPLE get memory of type
    * VMK_MEM_PHYS_ANY_CONTIGUITY and VMK_PHYS_ADDR_ANY, which are suitable
    * for purely software heaps, and may be suitable for some heaps that
    * need to be addressable by hardware.  A new memory pool will be 
    * constructed for this heap.  VMK_HEAP_TYPE_CUSTOM or VMK_HEAP_TYPE_MEMPOOL
    * must be used in order to specify different contiguity or physical
    * constraints, or to use an existing memory pool.
    */
   VMK_HEAP_TYPE_SIMPLE = 0,

   /** \brief Heaps that can customize additional parameters. */
   VMK_HEAP_TYPE_CUSTOM = 1,

   /** \brief Heaps whose memory comes from a specified memPool. */
   VMK_HEAP_TYPE_MEMPOOL = 2,
} vmk_HeapType;


/**
 * \brief Properties for creating a heap allocator.
 */
typedef struct vmk_HeapCreateProps {
   /** \brief Type of heap. */
   vmk_HeapType type;

   /** \brief Name associated with this heap. */
   vmk_Name name;

   /** \brief Module ID of the module creating this heap. */
   vmk_ModuleID module;

   /** \brief Initial size of the heap in bytes. */
   vmk_ByteCountSmall initial;

   /** \brief Maximum size of the heap in bytes. */
   vmk_ByteCountSmall max;

   /** \brief How long to wait for memory during heap creation. */
   vmk_uint32 creationTimeoutMS;
   
   /** \brief Type-specific heap properties. */
   union {
      /** \brief Properties for VMK_HEAP_TYPE_CUSTOM. */
      struct {
         /** \brief Physical contiguity allocated from this heap. */
         vmk_MemPhysContiguity physContiguity;

         /** \brief Physical address resrictions. */
         vmk_MemPhysAddrConstraint physRange;
      } custom;
    
      /** \brief Properties for VMK_HEAP_TYPE_MEMPOOL. */
      struct {
         /** \brief Physical contiguity allocated from this heap. */
         vmk_MemPhysContiguity physContiguity;

         /** \brief Physical address resrictions. */
         vmk_MemPhysAddrConstraint physRange;

         /** \brief Memory pool to back this heap. */
         vmk_MemPool memPool;
      } memPool;
   } typeSpecific;
} vmk_HeapCreateProps;

/**
 * \brief Structure for querying properties of a heap.
 */
typedef struct vmk_HeapGetProps {
   /** \brief Name associated with this heap. */
   vmk_Name name;

   /** \brief Module ID of the module that created this heap. */
   vmk_ModuleID module;

   /** \brief Initial size of the heap in bytes. */
   vmk_ByteCountSmall initial;

   /** \brief Maximum size of the heap in bytes. */
   vmk_ByteCountSmall max;

   /** \brief Physical contiguity allocated from this heap. */
   vmk_MemPhysContiguity physContiguity;

   /** \brief Physical address resrictions. */
   vmk_MemPhysAddrConstraint physRange;

   /** \brief Memory pool backing this heap. */
   vmk_MemPool memPool;
} vmk_HeapGetProps;


/**
 * \brief Describes characteristics of allocation from a heap.
 */
typedef struct vmk_HeapAllocationDescriptor {
   /** \brief The size of the object being allocated. */
   vmk_ByteCount size;
   /** \brief The alignment at which these objects will be allocated. */
   vmk_ByteCount alignment;
   /** \brief The number of objects of this size and alignment being allocated. */
   vmk_uint64 count;
} vmk_HeapAllocationDescriptor;


/*
 ***********************************************************************
 * vmk_HeapCreate --                                              */ /**
 *
 * \brief Create a heap that can grow dynamically up to the max size.
 *
 * \note  This function may block if heapCreationTimeoutMS is not
 *        VMK_TIMEOUT_NONBLOCKING.
 *
 * \param[in]  props     Properties of the heap.
 * \param[out] heapID    Newly created heap or VMK_INVALID_HEAP_ID
 *                       on failure.
 *
 * \retval VMK_NO_MEM    The heap could not be allocated.
 * \retval VMK_BAD_PARAM Invalid combination of props->initial and props->max
 *                       was specified.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_HeapCreate(vmk_HeapCreateProps *props,
                                vmk_HeapID *heapID);

/*
 ***********************************************************************
 * vmk_HeapDestroy --                                             */ /**
 *
 * \brief Destroy a dynamic heap
 *
 * \note  This function will not block.
 *
 * \param[in] heap   Heap to destroy.
 *
 * \pre All memory allocated on the heap should be freed before the heap
 *      is destroyed.
 *
 ***********************************************************************
 */
void vmk_HeapDestroy(vmk_HeapID heap);

/*
 ***********************************************************************
 * vmk_HeapDestroySync --                                         */ /**
 *
 * \brief Destroy a dynamic heap.  If the heap is non-empty, wait
 *        for it to become empty before destroying.
 *
 * \note  This function may block if the heap is non-empty.
 *
 * \param[in] heap      Heap to destroy.
 * \param[in] timeoutMS Timeout in milliseconds.  Zero means no timeout.
 *
 * \retval VMK_OK               Heap was destroyed successfully.
 * \retval VMK_DEATH_PENDING    World was killed while waiting and the
 *                              heap was not destroyed.
 * \retval VMK_WAIT_INTERRUPTED The wait was interrupted and the heap
 *                              was not destroyed.
 * \retval VMK_TIMEOUT          The wait timed out and the heap was
 *                              not destroyed.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_HeapDestroySync(vmk_HeapID heap,
                                     vmk_int64 timeoutMS);

/*
 ***********************************************************************
 * vmk_HeapFree --                                                */ /**
 *
 * \brief Free memory allocated with vmk_HeapAlloc.
 *
 * \note  This function will not block.
 *
 * \param[in] heap   Heap that memory to be freed was allocated from.
 * \param[in] mem    Memory to be freed. Should not be NULL.
 *
 ***********************************************************************
 */
void vmk_HeapFree(vmk_HeapID heap, void *mem);

/*
 ***********************************************************************
 * vmk_HeapAllocWithRA --                                         */ /**
 *
 * \brief Allocate memory and specify the caller's address.
 *
 * \note  This function will not block.
 *
 * This is useful when allocating memory from a wrapper function.
 *
 * \param[in] heap   Heap that memory to be freed was allocated from.
 * \param[in] size   Number of bytes to allocate.
 * \param[in] ra     Address to return to.
 *
 * \retval NULL Cannot allocate 'size' bytes from specified heap.
 * \return Address of allocated memory of the specified size.
 *
 ***********************************************************************
 */
void *vmk_HeapAllocWithRA(vmk_HeapID heap, vmk_uint32 size, void *ra);

/*
 ***********************************************************************
 * vmk_HeapAlignWithRA --                                         */ /**
 *
 * \brief Allocate aligned memory and specify the caller's address
 *
 * This is useful when allocating memory from a wrapper function.
 *
 * \note  This function will not block.
 *
 * \param[in] heap   Heap that memory to be freed was allocated from.
 * \param[in] size   Number of bytes to allocate.
 * \param[in] align  Number of bytes the allocation should be aligned
 *                   on.
 * \param[in] ra     Address to return to after allocation.
 *
 * \retval NULL Cannot allocate 'size' bytes from specified heap.
 * \return Address of allocated memory of the specified size.
 *
 ***********************************************************************
 */
void *vmk_HeapAlignWithRA(vmk_HeapID heap, vmk_uint32 size,
			  vmk_uint32 align, void *ra);

/*
 ***********************************************************************
 * vmk_HeapAlloc --                                               */ /**
 *
 * \brief Allocate memory in the specified heap
 *
 * \note  This function will not block.
 * 
 * \param[in] _heap   Heap to allocate from.
 * \param[in] _size   Number of bytes to allocate.
 *
 * \retval NULL Cannot allocate 'size' bytes from specified heap.
 * \return Address of allocated memory of the specified size.
 *
 ***********************************************************************
 */
#define vmk_HeapAlloc(_heap, _size) \
   (vmk_HeapAllocWithRA((_heap), (_size), __builtin_return_address(0)))

/*
 ***********************************************************************
 * vmk_HeapAlign --                                               */ /**
 *
 * \brief Allocate aligned memory
 *
 * \param[in] _heap       Heap to allocate from.
 * \param[in] _size       Number of bytes to allocate.
 * \param[in] _alignment  Number of bytes the allocation should be
 *                        aligned on.
 *
 * \note  This function will not block.
 *
 * \retval NULL Cannot allocate 'size' bytes from specified heap.
 * \return Address of allocated memory of the specified size.
 *
 ***********************************************************************
 */
#define vmk_HeapAlign(_heap, _size, _alignment)         \
   (vmk_HeapAlignWithRA((_heap), (_size), (_alignment), \
                        __builtin_return_address(0)))

/*
 ***********************************************************************
 * vmk_HeapAllocWithTimeoutAndRA --                               */ /**
 *
 * \brief Allocate memory, possibly waiting for physical memory.
 *
 * If the heap is not already at maximum size, this call may block
 * for up to timeoutMS milliseconds for memory to become available
 * to grow the heap.  If the heap is full and is already at maximum
 * size, no blocking will be performed.
 *
 * This is useful when allocating memory from a wrapper function.
 *
 * \note  This function may block.
 *
 * \param[in] heap         Heap to allocate from.
 * \param[in] size         Number of bytes to allocate.
 * \param[in] timeoutMS    Maximum wait time in milliseconds.
 * \param[in] ra           Address to return to.
 *
 * \retval NULL Cannot allocate 'size' bytes from specified heap.
 * \return Address of allocated memory of the specified size.
 *
 ***********************************************************************
 */
void *vmk_HeapAllocWithTimeoutAndRA(vmk_HeapID heap, vmk_uint32 size, 
                                    vmk_uint32 timeoutMS, void *ra);

/*
 ***********************************************************************
 * vmk_HeapAlignWithTimeoutAndRA --                               */ /**
 *
 * \brief Allocate aligned memory, possibly waiting for physical memory.
 *
 * If the heap is not already at maximum size, this call may block
 * for up to timeoutMS milliseconds for memory to become available
 * to grow the heap.  If the heap is full and is already at maximum
 * size, no blocking will be performed.
 *
 * This is useful when allocating memory from a wrapper function.
 *
 * \note  This function may block.
 *
 * \param[in] heap        Heap to allocate from.
 * \param[in] size        Number of bytes to allocate.
 * \param[in] align       Number of bytes the allocation should be
 *                        aligned on.
 * \param[in] timeoutMS   Maximum wait time in milliseconds.
 * \param[in] ra          Address to return to after allocation.
 *
 * \retval NULL Cannot allocate 'size' bytes from specified heap.
 * \return Address of allocated memory of the specified size.
 *
 ***********************************************************************
 */
void *vmk_HeapAlignWithTimeoutAndRA(vmk_HeapID heap, vmk_uint32 size,
                                    vmk_uint32 align, vmk_uint32 timeoutMS,
                                    void *ra);

/*
 ***********************************************************************
 * vmk_HeapAllocWithTimeout --                                    */ /**
 *
 * \brief Allocate memory, possibly waiting for physical memory.
 *
 * If the heap is not already at maximum size, this call may block
 * for up to timeoutMS milliseconds for memory to become available
 * to grow the heap.  If the heap is full and is already at maximum
 * size, no blocking will be performed.
 *
 * \note  This function may block.
 *
 * \param[in] _heap        Heap to allocate from.
 * \param[in] _size        Number of bytes to allocate.
 * \param[in] _timeoutMS   Maximum wait time in milliseconds.
 *
 * \retval NULL Cannot allocate 'size' bytes from specified heap.
 * \return Address of allocated memory of the specified size.
 *
 ***********************************************************************
 */
#define vmk_HeapAllocWithTimeout(_heap, _size, _timeoutMS)             \
        (vmk_HeapAllocWithTimeoutAndRA((_heap), (_size), (_timeoutMS), \
                                       __builtin_return_address(0)))

/*
 ***********************************************************************
 * vmk_HeapAlignWithTimeout --                                    */ /**
 *
 * \brief Allocate aligned memory, possibly waiting for physical memory.
 *
 * If the heap is not already at maximum size, this call may block
 * for up to timeoutMS milliseconds for memory to become available
 * to grow the heap.  If the heap is full and is already at maximum
 * size, no blocking will be performed.
 *
 * \note  This function may block.
 *
 * \param[in] _heap         Heap to allocate from.
 * \param[in] _size         Number of bytes to allocate.
 * \param[in] _alignment    Number of bytes the allocation should be
 *                          aligned on.
 * \param[in] _timeoutMS    Maximum wait time in milliseconds.
 *
 * \retval NULL Cannot allocate 'size' bytes from specified heap.
 * \return Address of allocated memory of the specified size.
 *
 ***********************************************************************
 */
#define vmk_HeapAlignWithTimeout(_heap, _size, _alignment, _timeoutMS)       \
   (vmk_HeapAlignWithTimeoutAndRA((_heap), (_size), (_alignment),            \
                                  (_timeoutMS),  __builtin_return_address(0)))

/*
 ***********************************************************************
 * vmk_HeapGetProperties --                                       */ /**
 *
 * \brief Get the properties of the heap
 *
 * \note  The properties returned may not necessarily be the same as
 *        those specified when the heap was created.  Some properties are
 *        adjusted by the heap internal implementation to conform to
 *        implementation details.  For example, a heap maximum size may
 *        be rounded up due to alignment of allocations.  The values
 *        returned are those that are actually used by the heap.
 *
 * \note  This function will not block.
 *
 * \param[in]  heap      Heap that caller is interogating.
 * \param[out] props     The heap's properties.
 *
 * \retval VMK_OK          Properties successfully retrieved.
 * \retval VMK_BAD_PARAM   heap was not a valid heap ID.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_HeapGetProperties(vmk_HeapID heap, vmk_HeapGetProps *props);

/*
 ***********************************************************************
 * vmk_HeapDetermineMaxSize --                                    */ /**
 *
 * \brief Determines the required maximum size of a heap in bytes.
 *
 * Determines the required maximum size of a heap that will contain
 * the descriptors described in the descriptors array. Note that this
 * does not include overhead for fragmentation within the heap and it
 * is the caller's responsibility to add that in when creating the heap.
 *
 * \note  This function will not block.
 *
 * \param[in]  descriptors     Pointer to an array of descriptors that
 *                             the heap will contain.  An alignment of
 *                             zero for a descriptor indicates no
 *                             alignment is specified by the caller and
 *                             the heap code will use a value it
 *                             determines.
 * \param[in]  numDescriptors  Number of vmk_HeapAllocationDescriptors
 *                             in the array pointed to by descriptors.
 * \param[out] maxSize         The determined maximum size.
 *
 * \retval VMK_OK              Size determined successfully and placed
 *                             in maxSize.
 * \retval VMK_BAD_PARAM       One of the arguments is invalid, or one
 *                             of the descriptors contains an invalid
 *                             value.
 * \retval VMK_LIMIT_EXCEEDED  The total size required for the
 *                             provided descriptors results in a heap
 *                             greater than the largest possible
 *                             allowed.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_HeapDetermineMaxSize(vmk_HeapAllocationDescriptor *descriptors,
                         vmk_uint64 numDescriptors,
                         vmk_ByteCount *maxSize);

#endif /* _VMKAPI_HEAP_H_ */
/** @} */
/** @} */
