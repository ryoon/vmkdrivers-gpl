/* **********************************************************
 * Copyright 1998 - 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Slabs                                                          */ /**
 * \defgroup Slab Slab Allocation
 * @{
 *
 * ESX Server supports slab allocation for high performance driver/stack
 * implementations:
 * - Reduces memory fragmentation, especially for smaller data structures
 *   allocated in high volume.
 * - Reduces CPU consumption for data structure initialization/teardown.
 * - Improves CPU hardware cache performance.
 * - Provides finer grained control of memory consumption.
 *
 ***********************************************************************
 */

#ifndef _VMKAPI_SLAB_H_
#define _VMKAPI_SLAB_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/**
 * \brief Opaque handle for a slab cache.
 */
typedef struct vmk_SlabIDInt *vmk_SlabID;

#define VMK_INVALID_SLAB_ID ((vmk_SlabID)NULL)

/**
 * \brief Constant for specifying an unlimited number of slab objects.
 */
#define VMK_SLAB_MAX_UNLIMITED ((vmk_ByteCountSmall) -1)

/*
 ***********************************************************************
 * vmk_SlabItemConstructor --                                     */ /**
 *
 * \brief Item constructor - optional user defined function.  Runs for
 *                           each object when a cluster of memory is
 *                           allocated from the heap.
 *
 * \note   When the control structure is placed inside the free object,
 *         then the constructor must take care not to modify the control
 *         structure.
 *
 * \note   A callback of this type must not block or call any API
 *         functions that may block.
 *
 * \param[in] object     Object to be constructed.
 * \param[in] size       Size of buffer (possibly greater than objSize).
 * \param[in] arg        constructorArg (see vmk_SlabCreateProps).
 * \param[in] flags      Currently unused (reserved for future use).
 *
 * \retval VMK_OK to indicate object construction has succeded.
 * \return Other To indicate failure.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_SlabItemConstructor)(void *object,
                                                    vmk_ByteCountSmall size,
                                                    vmk_AddrCookie arg,
                                                    int flags);


/*
 ***********************************************************************
 *  vmk_SlabItemDestructor --                                     */ /**
 *
 * \brief Item destructor - optional user defined function.  Runs for
 *                          each buffer just before a cluster of memory
 *                          is returned to the heap.
 *
 * \note   When the control structure is placed inside the free object,
 *         then the destructor must take care not to modify the control
 *         structure.
 *
 * \note   A callback of this type must not block or call any API
 *         functions that may block.
 *
 * \param[in] object     Object to be destroyed.
 * \param[in] size       Size of buffer (possibly greater than objSize).
 * \param[in] arg        constructorArg (see vmk_SlabCreateProps).
 *
 ***********************************************************************
 */
typedef void (*vmk_SlabItemDestructor)(void *object, vmk_ByteCountSmall size,
                                       vmk_AddrCookie arg);

/**
 * \brief Memory requirements for slabs.
 */
typedef struct vmk_SlabMemSize {
   /** \brief Minimum number of memory pages this slab will occupy. */
   vmk_uint32 minPages;
   
   /** \brief Maximum number of memory pages this slab will occupy. */
   vmk_uint32 maxPages;
} vmk_SlabMemSize;


/**
 * \brief Types of slab
 */
typedef enum vmk_SlabType {
   /** 
    * \brief Slabs that get their own memory pool. 
    *
    * Slabs of type VMK_SLAB_TYPE_SIMPLE get a contiguity of
    * VMK_MEM_PHYS_ANY_CONTIGUITY and a physical address constraint
    * of VMK_PHYS_ADDR_ANY.  For slabs with different constraints,
    * use type VMK_SLAB_TYPE_CUSTOM or VMK_SLAB_TYPE_MEMPOOL.
    */
   VMK_SLAB_TYPE_SIMPLE = 0,

   /** \brief Slabs with custom memory properties. */
   VMK_SLAB_TYPE_CUSTOM = 1,

   /** \brief Slabs whose memory comes from an existing memPool. */
   VMK_SLAB_TYPE_MEMPOOL = 2,
   
} vmk_SlabType;

/**
 * \brief Properties of a slab allocator
 */
typedef struct vmk_SlabCreateProps {
   /** \brief Type of slab */
   vmk_SlabType type;

   /**
    * \brief Name of the slab.
    */
   vmk_Name name;

   /** \brief Module ID of the module creating this slab. */
   vmk_ModuleID module;

   /** 
    * \brief Byte Size of each object 
    *
    * objSize must be at least 1 byte and not more than the value
    * returned by vmk_SlabGetMaxObjectSize().
    **/
   vmk_ByteCountSmall objSize;

   /** 
    * \brief Byte alignment for each object 
    *
    * The alignment must be a power of 2, and it must be less than or
    * equal to VMK_PAGE_SIZE.
    */
   vmk_ByteCountSmall alignment;

   /** \brief Called after an object is allocated (or NULL for no action) */
   vmk_SlabItemConstructor constructor;
   
   /** \brief Called before an object is freed (or NULL for no action) */
   vmk_SlabItemDestructor destructor;

   /** \brief Argument for constructor/destructor calls */
   vmk_AddrCookie constructorArg;

   /** 
    * \brief Offset in allocation for slab control structure.
    * \sa vmk_SlabControlSize
    */
   vmk_ByteCountSmall ctrlOffset;

   /** \brief Minimum number of objects allocatable from this slab. */
   vmk_uint32 minObj;
   
   /** \brief Maximum number of objects allocatable from this slab. */
   vmk_uint32 maxObj;

   /** \brief How long to wait for memory during slab creation. */
   vmk_uint32 creationTimeoutMS;

   /** \brief Type-specific slab properties */
   union {
      /** \brief Properties for VMK_SLAB_TYPE_CUSTOM. */
      struct {
         /** \brief Physical contiguity of objects allocated from this slab. */
         vmk_MemPhysContiguity physContiguity;
         
         /** \brief Physical address restrictions. */
         vmk_MemPhysAddrConstraint physRange;
      } custom;

      /** \brief Properties for VMK_SLAB_TYPE_MEMPOOL. */
      struct {
         /** \brief Physical contiguity allocated from this slab. */
         vmk_MemPhysContiguity physContiguity;
         
         /** \brief Physical address restrictions. */
         vmk_MemPhysAddrConstraint physRange;

         /** \brief Memory pool. */
         vmk_MemPool memPool;
      } memPool;

   } typeSpecific;
} vmk_SlabCreateProps;


/**
 * \brief Properties structure for querying a slab allocator.
 */
typedef struct vmk_SlabGetProps {
   /** \brief Name of the slab. */
   vmk_Name name;

   /** \brief Module ID of the module creating this slab. */
   vmk_ModuleID module;

   /** \brief Byte Size of each object. */
   vmk_ByteCountSmall objSize;

   /** 
    * \brief Byte alignment for each object 
    */
   vmk_ByteCountSmall alignment;

   /** \brief Called after an object is allocated (or NULL for no action) */
   vmk_SlabItemConstructor constructor;
   
   /** \brief Called before an object is freed (or NULL for no action) */
   vmk_SlabItemDestructor destructor;

   /** \brief Argument for constructor/destructor calls */
   vmk_AddrCookie constructorArg;

   /** \brief Offset in allocation for slab control structure. */
   vmk_ByteCountSmall ctrlOffset;

   /** \brief Minimum number of objects allocatable from this slab. */
   vmk_uint32 minObj;
   
   /** \brief Maximum number of objects allocatable from this slab. */
   vmk_uint32 maxObj;

   /** \brief Physical contiguity allocated from this slab. */
   vmk_MemPhysContiguity physContiguity;

   /** \brief Physical address restrictions. */
   vmk_MemPhysAddrConstraint physRange;

   /** \brief Memory pool. */
   vmk_MemPool memPool;
} vmk_SlabGetProps;


/*
 ***********************************************************************
 *  vmk_SlabCreate --                                             */ /**
 *
 * \brief Create a slab allocator cache.
 *
 * A slab is created with the specified creation properties.  Enough 
 * memory is allocated for props->minObj to be created.  Slab creation 
 * will fail if enough memory cannot be allocated.
 *
 * \note   This function may block if creationTimeoutMS is not 
 *         VMK_TIMEOUT_NONBLOCKING.
 *
 * \param [in]  props      Properties of the new cache.
 * \param [out] cache      For use with vmk_SlabAlloc, etc.
 *
 * \retval VMK_OK          indicates that the slab was created.
 * \retval VMK_BAD_PARAM   indicates that some slab parameters were
 *                         invalid.
 * \retval VMK_NO_MEMORY   indicates that insufficient memory was
 *                         available.
 * \retval VMK_TIMEOUT     indicates that the timeout was reached before
 *                         enough memory could be allocated.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SlabCreate(vmk_SlabCreateProps *props,
                                vmk_SlabID *cache);


/*
 ***********************************************************************
 *  vmk_SlabAlloc --                                              */ /**
 *
 * \brief Allocate an item from a slab.
 *
 * The vmk_SlabItemConstructor (if defined) was previously called,
 * or the object was previously freed via vmk_SlabFree().
 *
 * If the slab is not yet at maximum size, this function may attempt to
 * allocate additional physical memory to satisfy the allocation request.
 * If physical memory is not currently available, this function will not
 * block to wait for memory to become available.
 *
 * \note   When the control structure is placed inside the free object,
 *         the caller of vmk_SlabAlloc() must assume this portion of
 *         the object is uninitialized.
 *
 * \note   This function will not block.
 *
 * \param[in] cache      Slab from which allocation will take place.
 *
 * \retval NULL   Memory could not be allocated.
 *
 ***********************************************************************
 */
void *vmk_SlabAlloc(vmk_SlabID cache);


/*
 ***********************************************************************
 *  vmk_SlabAllocWithTimeout --                                   */ /**
 *
 * \brief Allocate an item, possibly waiting for physical memory.
 *
 * The vmk_SlabItemConstructor (if defined) was previously called,
 * or the object was previously freed via vmk_SlabFree().
 *
 * If the slab is not yet at maximum size, this function may attempt to
 * allocate additional physical memory to satisfy the allocation request.
 * If physical memory is not currently available, this function will use
 * the value of timeout to determine if it may block until memory is
 * available, and if so, for how long.
 *
 * \note   When the control structure is placed inside the free object,
 *         the caller of vmk_SlabAlloc() must assume this portion of
 *         the object is uninitialized.
 *
 * \note   This function may block.
 *
 * \param[in] cache      Slab from which allocation will take place.
 * \param[in] timeoutMS  Timeout specifying how long the call may wait.
 *
 * \retval NULL   Memory could not be allocated.
 *
 ***********************************************************************
 */
void *vmk_SlabAllocWithTimeout(
   vmk_SlabID cache,
   vmk_uint32 timeoutMS);

/*
 ***********************************************************************
 *  vmk_SlabFree --                                               */ /**
 *
 * \brief Free memory allocated by vmk_SlabAlloc.
 *
 * The memory object will be retained by the slab.  If at some point 
 * the slab chooses to give the memory back to the system, the
 * vmk_SlabItemDestructor (if defined) will be called.
 *
 * \note   This function will not block.
 *
 * \param[in] cache      Slab from which the item was allocated.
 * \param[in] object     object to be freed.
 *
 ***********************************************************************
 */
void vmk_SlabFree(
   vmk_SlabID cache,
   void *object);


/*
 ***********************************************************************
 *  vmk_SlabDestroy --                                            */ /**
 *
 * \brief Destroy a slab cache previously created by vmk_SlabCreate.
 *
 * \note   This function may block.
 *
 * \param[in] cache      The cache to be destroyed.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SlabDestroy(vmk_SlabID cache);


/*
 ***********************************************************************
 *  vmk_SlabControlSize --                                        */ /**
 *
 * \brief  Get the size of the per-object "control" structure.
 *
 * The slab maintains a control structure for each free object
 * cached by the slab.  When VMK_SLAB_TYPE_SIMPLE properties are
 * used to create the slab, the control structure will be tacked
 * past the end of the client's object.  To save space, the control
 * structure can be placed within the user's free object using the
 * ctrlOffset paramter to VMK_SLAB_TYPE_NOMINAL properties.
 *
 * \note   For best performance, it is recommended that the control structure
 *         offset is a multiple of sizeof (void *), but this is not a
 *         requirement.
 *
 * \note   See vmk_SlabItemConstructor, vmk_SlabItemDestructor and
 *         and vmk_SlabAlloc for the constraints that must be obeyed
 *         when the control structure is placed inside the object.
 *
 * \note   This function will not block.
 *
 * \return Size of the control structure in bytes.
 *
 ***********************************************************************
 */
vmk_ByteCountSmall vmk_SlabControlSize(void);

/*
 ***********************************************************************
 *  vmk_SlabGetMemSize --                                         */ /**
 *
 * \brief  Returns the memory requirements for a slab.
 *
 * This function computes the minimum and maximum required
 * memory for a slab with the provided characteristics.
 *
 * For a slab of type VMK_SLAB_TYPE_MEMPOOL, the provided memPool
 * may be VMK_MEMPOOL_LEGACY_INVALID, so this function can be used
 * to compute needed sizes for constructing a memory pool.
 *
 * \note   This function will not block.
 *
 * \return VMK_BAD_PARAM   minObj and maxObj were inconsistent,
 *                         or maxObj was VMK_SLAB_MAX_UNLIMITED
 *                         for a slab of type VMK_SLAB_TYPE_SIMPLE or
 *                         VMK_SLAB_TYPE_CUSTOM.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SlabGetMemSize(vmk_SlabCreateProps   *props,
                                    vmk_SlabMemSize *size);


/*
 ***********************************************************************
 *  vmk_SlabGetProperties --                                      */ /**
 *
 * \brief  Returns the allocation properties for a slab.
 *
 * \note   This function will not block.
 *
 * \return VMK_OK          Properties is filled in.
 * \return VMK_NOT_FOUND   slab was not a valid slab handle.
 *
 ***********************************************************************
 */

VMK_ReturnStatus
vmk_SlabGetProperties(vmk_SlabID slab, vmk_SlabGetProps *props);


/*
 ***********************************************************************
 *  vmk_SlabGetMaxObjectSize --                                   */ /**
 *
 * \brief  Returns the maximum allowed size for an object
 *
 * \note   This function will not block.
 *
 * \return Max Object size
 *
 ***********************************************************************
 */

vmk_ByteCountSmall
vmk_SlabGetMaxObjectSize(vmk_ByteCountSmall alignment);


#endif /* _VMKAPI_SLAB_H_ */
/** @} */
