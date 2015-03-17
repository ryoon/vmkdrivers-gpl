/* **********************************************************
 * Copyright 2012 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * PageSlab                                                       */ /**
 * \addtogroup Core
 * @{
 * \defgroup PageSlab Slab page allocator
 *
 * Functions related to page slabs. A page slab is a page-wise
 * allocator that allows for fast page allocations and freeing.
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_PAGESLAB_H_
#define _VMKAPI_PAGESLAB_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/** \brief INVALID page slab ID */
#define VMK_INVALID_PAGESLAB_ID  ((vmk_PageSlabID)NULL)

/** \brief Page slab ID */
typedef struct vmkPageSlabInt* vmk_PageSlabID;

/**
 * \brief Properties of a page slab.
 */
typedef struct vmk_PageSlabCreateProps {
   /** \brief Heap to allocate page slab metadata from. The size of the
    *         allocation can be retrieved via vmk_PageSlabAllocationSize.
    */
   vmk_HeapID heapID;
   /** \brief Name for this page slab. */
   vmk_Name name;
   /** \brief Minimum number of pages that should be held in the slab.
    *         Please be careful when sizing this number as any memory
    *         that is used for the minimum will be unavailable to VMs, etc.
    */
   vmk_uint32 minPages;
   /** \brief Maximum number of pages that the slab should provide for. */
   vmk_uint32 maxPages;
   /** \brief Restrictions on the physical address space that pages for
    *         the page slab are allocated from. Please choose the least
    *         restrictive constraint possible as memory below 2GB or 4GB
    *         is a scarce resource.
    */
   vmk_MemPhysAddrConstraint physRange;
   /** \brief Memory pool that the slab should allocate pages from. Set to
    *         VMK_MEMPOOL_INVALID to have vmk_PageSlabCreate create a new
    *         memory pool for the slab.
    */
   vmk_MemPool memPool;
} vmk_PageSlabCreateProps;


/*
 ***********************************************************************
 * vmk_PageSlabCreateCustom --                                    */ /**
 *
 * \brief Create a new page slab.
 *
 * This function creates a new page slab with the given properties.
 *
 * \note This function might block.
 *
 * \param[in]  moduleID       Module ID of the module that this
 *                            page slab will belong to.
 * \param[in]  createProps    Properties of the new page slab.
 * \param[out] pageSlabID    Handle to the newly created page slab.
 *
 ***********************************************************************
 */

VMK_ReturnStatus
vmk_PageSlabCreateCustom(vmk_ModuleID moduleID,
			 vmk_PageSlabCreateProps *createProps,
			 vmk_PageSlabID *pageSlabID);

/*
 ***********************************************************************
 * vmk_PageSlabCreate --                                         */ /**
 *
 * \brief Convenience wrapper around vmk_PageSlabCreateCustom
 *
 * This function creates a new page slab with the given properties.
 *
 * \note This function might block.
 *
 * \param[in]  createProps    Properties of the new page slab.
 * \param[out] pageSlabID    Handle to the newly created page slab.
 *
 ***********************************************************************
 */

static VMK_INLINE VMK_ReturnStatus
vmk_PageSlabCreate(vmk_PageSlabCreateProps *createProps,
		    vmk_PageSlabID *pageSlabID)
{
   return vmk_PageSlabCreateCustom(vmk_ModuleCurrentID, createProps, pageSlabID);
}


/*
 ***********************************************************************
 * vmk_PageSlabDestroy --                                         */ /**
 *
 * \brief Destroy a page slab.
 *
 * \note The page slab must be empty meaning all previously allocated
 *       pages must have been freed back to the slab.
 * \note This function might block.
 *
 * \param[in]  pageSlabID   Page slab identifier acquired through a
 *                          preceding vmk_PageSlabCreate(Custom) call.
 *
 ***********************************************************************
 */

void
vmk_PageSlabDestroy(vmk_PageSlabID pageSlabID);


/*
 ***********************************************************************
 * vmk_PageSlabAllocationSize --                                  */ /**
 *
 * \brief Return the amount of memory that vmk_PageSlabCreate is going
 *        to allocate from the passed in heap for each call.
 *
 * \note This function will not block.
 *
 ***********************************************************************
 */

vmk_ByteCountSmall
vmk_PageSlabAllocationSize(void);


/*
 ***********************************************************************
 * vmk_PageSlabAlloc --	                                          */ /**
 *
 * \brief Allocate a page from the given page slab.
 *
 * \note This function may block based on the blocking parameter
 *
 * \param[in]  pageSlabID  Page slab previously created via
 *			    vmk_PageSlabCreate(Custom)
 * \param[in]  blocking	    Allow the page slab to block and wait for
 *			    a page to become free if none are readily
 *			    available.
 * \param[out] mpn	    Machine page number of the new page. Only
 * 			    filled if return status was VMK_OK;
 *
 * \retval VMK_NO_MEMORY  The slab was unable to allocate a page.
 *
 ***********************************************************************
 */

VMK_ReturnStatus
vmk_PageSlabAlloc(vmk_PageSlabID pageSlabID,
		   vmk_Bool	   blocking,
		   vmk_MPN	  *mpn);


/*
 ***********************************************************************
 * vmk_PageSlabFree --		                                  */ /**
 *
 * \brief Free a page to the given page slab.
 *
 * \param[in]  pageSlabID  Page slab previously created via
 *			    vmk_PageSlabCreate(Custom)
 * \param[in]  mpn	    An machine page number previously returned
 *			    by vmk_PageSlabAlloc
 *
 ***********************************************************************
 */

void
vmk_PageSlabFree(vmk_PageSlabID pageSlabID,
		  vmk_MPN	   mpn);


#endif /* _VMKAPI_PAGESLAB_H_ */
/** @} */
/** @} */
