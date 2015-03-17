/* **********************************************************
 * Copyright 2007 - 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * MemPool                                                        */ /**
 * \defgroup MemPool Managed Machine-Page Pools
 *
 * Memory pools are used to manage machine memory resources for admission 
 * control and for better resource tracking. Each MemPool entity represents
 * a set of limits that the internal resource management algorithms honor.
 * The functions here provide operations to add such pools starting at
 * the root represented by kmanaged group and and also introduces APIs 
 * to allocate/free memory based on the restrictions of the mempool.
 * 
 * @{
 ***********************************************************************
 */
 
#ifndef _VMKAPI_MEMPOOL_H_
#define _VMKAPI_MEMPOOL_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/** \brief Memory pool with no limit. */
#define VMK_MEMPOOL_NO_LIMIT 0

/**
 * \brief The invalid MemPool object.
 */
#define VMK_MEMPOOL_INVALID ((vmk_MemPool) NULL)

/**
 * \ingroup MemPool
 * \brief Memory pool handle
 */
typedef struct vmk_MemPoolInt* vmk_MemPool;

/**
 * \ingroup MemPool
 * \brief Types of memory pools
 *
 * \note Only VMK_MEM_POOL_LEAF memory pools can be used as memory pools for
 *       allocation functions.
 *
 * \note VMK_MEM_POOL_PARENT memory pools can be used to group other memory
 *       pools together for easier resource admission control and better
 *       resource tracking.
 */
typedef enum vmk_MemPoolType {
   /** \brief Memory pool can be parent of other memory pools. */
   VMK_MEM_POOL_PARENT = 0,

   /** \brief Memory pool cannot be parent of another memory pool. */
   VMK_MEM_POOL_LEAF  = 1,
} vmk_MemPoolType;

/**
 * \ingroup MemPool
 * \brief Properties of the resource limitations of a memory pool
 */
typedef struct vmk_MemPoolResourceProps {
   /** \brief Specifies the min num of guaranteed pages reserved for the pool. */
   vmk_uint32     reservation;
   
   /** \brief Specifies the max num of pages the pool can offer. */
   vmk_uint32     limit;
} vmk_MemPoolResourceProps;

/**
 * \ingroup MemPool
 * \brief Properties of a memory pool
 */
typedef struct vmk_MemPoolProps {
   /** \brief Name associated with this memory pool. */
   vmk_Name name;

   /** \brief Module ID of the module creating this memory pool. */
   vmk_ModuleID module;

   /** \brief Parent memory pool handle, or VMK_MEMPOOL_INVALID. */
   vmk_MemPool parentMemPool;

   /** \brief Type of memory pool. */
   vmk_MemPoolType memPoolType;

   /** \brief Resource properties of memory pool. */
   vmk_MemPoolResourceProps resourceProps;
} vmk_MemPoolProps;

/**
 * \ingroup MemPool
 * \brief Properties of a memory pool allocation
 */
typedef struct vmk_MemPoolAllocProps {
   /** \brief Physical contiguity for this memory allocation. */
   vmk_MemPhysContiguity physContiguity;

   /** \brief Physical address resrictions. */
   vmk_MemPhysAddrConstraint physRange;

   /** \brief How long to wait for memory during allocation. */
   vmk_uint32 creationTimeoutMS;

} vmk_MemPoolAllocProps;

/**
 * \ingroup MemPool
 * \brief Properties of a memory pool allocation
 *
 * This struct describes a memory pool allocation request of machine pages.
 * The user provides a mpns array of size numElements.
 * For an allocation of physical contiguity type VMK_MEM_PHYS_CONTIGUOUS, only
 * the first MPN of the contiguous range is set in the mpnRanges array, otherwise
 * all elements in the mpnRanges array are set.
 */
typedef struct vmk_MemPoolAllocRequest {
   /** \brief Number of pages that should be allocated. */
   vmk_uint32 numPages;

   /** \brief Number of elements in vmk_MpnRange array. */
   vmk_uint32 numElements;

   /** \brief Array of numElements vmk_MpnRanges. */
   vmk_MpnRange *mpnRanges;
} vmk_MemPoolAllocRequest;

/*
 ***********************************************************************
 * vmk_MemPoolCreate --                                           */ /**
 *
 * \ingroup MemPool
 * \brief Create a machine memory pool.
 *
 * This function create a memory pool. A memory pool can be a child
 * of another created memory pool. It cannot exceed the limit or
 * reservation of its parent memory pool.
 *
 * \note A memory pool cannot exceed the reservation of its parent,
 *       but its limit can be larger than the limit of its parent.
 *       However, it will not be possible to allocate more pages than
 *       allowed by the parents limit.
 * \note This function will not block.
 *
 * \param[in]  props          Properties of the new memory pool.
 * \param[out] pool           Handle to the newly created memory pool.
 *
 * \retval VMK_BAD_PARAM             The pool or properties arguments
 *                                   were NULL, or the properties values
 *                                   were invalid.
 * \retval VMK_INVALID_NAME          Provided name was invalid.
 * \retval VMK_RESERVATION_GT_LIMIT  Reservation was larger than the
 *                                   limit in the pool properties. The
 *                                   reservation should always be less 
 *                                   or equal to the limit.
 * \retval VMK_NO_RESOURCES          The requested pool reservation was
 *                                   rejected because the parent's pool
 *                                   did not have the resources to
 *                                   fulfill it. If parentMemPool is 
 *                                   VMK_MEMPOOL_INVALID, the entire
 *                                   system did not have enough
 *                                   resources to fulfill the resource
 *                                   reservation.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_MemPoolCreate(
   vmk_MemPoolProps *props,
   vmk_MemPool *pool);

/*
 ***********************************************************************
 * vmk_MemPoolSetProps --                                         */ /**
 *
 * \ingroup MemPool
 * \brief Change the properties of an existing memory pool
 *
 * \note This function will not block.
 *
 * \param[in,out] pool   Memory pool to change
 * \param[in]     props  New properties for the memory pool
 *
 * \retval VMK_BAD_PARAM             The pool or properties arguments
 *                                   were NULL, the pool was invalid,
 *                                   or the properties values were
 *                                   invalid.
 * \retval VMK_RESERVATION_GT_LIMIT  Reservation was larger than the
 *                                   limit in the pool properties. The
 *                                   reservation should always be less 
 *                                   or equal to the limit.
 * \retval VMK_NO_RESOURCES          The requested pool reservation was
 *                                   rejected because the parent's pool
 *                                   did not have the resources to 
 *                                   fulfill it. If parentMemPool is 
 *                                   VMK_MEMPOOL_INVALID, the entire
 *                                   system did not have enough
 *                                   resources to fulfill the resource
 *                                   reservation.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_MemPoolSetProps(
   const vmk_MemPool pool,
   const vmk_MemPoolResourceProps *props);

/*
 ***********************************************************************
 * vmk_MemPoolGetProps --                                         */ /**
 *
 * \ingroup MemPool
 * \brief   Get the properties of an existing memory pool
 *
 * \note    This function will not block.
 *
 * \param[in]  pool     Memory pool to query
 * \param[out] props    Properties associated with the
 *                      memory pool
 *
 * \retval VMK_BAD_PARAM  The pool or properties argument was invalid.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_MemPoolGetProps(
   const vmk_MemPool pool,
   vmk_MemPoolResourceProps *props);

/*
 ***********************************************************************
 * vmk_MemPoolDestroy --                                          */ /**
 *
 * \ingroup MemPool
 * \brief Destroy a memory pool.
 *
 * \note The memory pool must have no children and no pages allocated to
 *       it, otherwise the operation will fail.
 *
 * \note This function will not block.
 *
 * \param[in]  pool   Memory pool to destroy
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_MemPoolDestroy(
   vmk_MemPool pool);

/*
 ***********************************************************************
 * vmk_MemPoolAlloc --                                            */ /**
 *
 * \ingroup MemPool
 * \brief Allocate a contiguous range of machine pages from a specified
 *        memory pool.
 *
 * \note The provided memory pool must be of type VMK_MEM_POOL_LEAF.
 * 
 * \note For allocations which are not of type VMK_MEM_PHYS_CONTIGUOUS,
 *       one vmk_MpnRange is required per page, otherwise one
 *       vmk_MpnRange is sufficient.
 *
 * \note This function may block if the creationTimeoutMS field of
 *       props is not VMK_TIMEOUT_NONBLOCKING.
 *
 * \param[in] pool            Memory pool to allocate from.
 * \param[in] props           Attributes for this allocation.
 * \param[in,out] request     Allocation request. The mpns array gets
 *                            filled on success.
 *
 * \retval VMK_BAD_PARAM       The pool or props argument was invalid,
 *                             the number of pages requested was zero,
 *                             mpnRanges array was not set, or
 *                             the number of elements was less than one.
 * \retval VMK_LIMIT_EXCEEDED  The number of elements is not sufficient
 *                             to represent the used vmk_MpnRanges.
 * \retval VMK_NO_RESOURCES    The requested allocation was rejected
 *                             because the pool or the pools group
 *                             did not have the resources to fulfill it.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_MemPoolAlloc(
   vmk_MemPool pool,
   const vmk_MemPoolAllocProps *props,
   const vmk_MemPoolAllocRequest *request);

/*
 ***********************************************************************
 * vmk_MemPoolFree --                                             */ /**
 *
 * \ingroup MemPool
 * \brief Free all pages described by the allocRequest.
 *
 * \param[in]  request   Request describing the vmk_MpnRanges to free.
 *
 * \note This function will not block.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_MemPoolFree(
   const vmk_MemPoolAllocRequest *request);

#endif /* _VMKAPI_MEMPOOL_H_ */
/** @} */
