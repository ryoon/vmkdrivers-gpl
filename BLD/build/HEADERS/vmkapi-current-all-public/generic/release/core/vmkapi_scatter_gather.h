/* **********************************************************
 * Copyright 2005 - 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * ScatterGather                                                  */ /**
 * \defgroup ScatterGather Scatter Gather Buffer Management
 *
 * Interfaces to manage discontiguous regions of memory for IO.
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_SCATTER_GATHER_H_
#define _VMKAPI_SCATTER_GATHER_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/**
 * \brief Scatter-gather element representing a contiguous region.
 *
 * Scatter-gather elements represent only one kind of contiguous
 * address region at a time.
 */
typedef struct vmk_SgElem {
   union {
      /**
       * \brief Starting machine address of the contiguous
       *        machine address region this element represents.
       *
       * Use this field when this element represents a machine address
       * range.
       */
      vmk_MA addr;

      /**
       * \brief Starting IO address of the contiguous IO address
       *        region this element represents.
       *
       * Use this field when this element represents an IO address
       * range.
       */
      vmk_IOA ioAddr;
   };

   /**
    * \brief Number of machine bytes represented by this element.
    */
   vmk_ByteCountSmall length;
   
   /** \brief Reserved */
   vmk_uint32 reserved;
} vmk_SgElem;

/**
 * \brief Scatter-gather array.
 *
 * A scatter gather array is a collection of contiguous address
 * regions.
 */
typedef struct vmk_SgArray {
   /** \brief The number of elements this scatter-gather array has. */
   vmk_uint32 maxElems;

   /** \brief Number of elements currently in-use. */
   vmk_uint32 numElems;
   
   /** \brief Reserved. */
   vmk_uint64 reserved;
   vmk_uint64 reserved2;

   /** \brief Array of elements. Should be set to zero on init. */
   vmk_SgElem elem[0];
} vmk_SgArray;

/**
 * \brief Opaque handle for scatter-gather operations.
 */
typedef struct vmk_SgOpsHandleInt *vmk_SgOpsHandle;

/**
 * \brief Opaque handle for scatter-gather component operations.
 */
typedef struct vmk_SgComponentOpsHandleInt *vmk_SgComponentOpsHandle;

/**
 * \brief The type of position a vmk_SgPosition represents.
 */
typedef enum vmk_SgPositionType {
   VMK_SG_POSITION_TYPE_NONE = 0,
   VMK_SG_POSITION_TYPE_UNKNOWN = 1,
   VMK_SG_POSITION_TYPE_BUFFER_OFFSET = 2,
   VMK_SG_POSITION_TYPE_ELEMENT = 3,
} vmk_SgPositionType;

/**
 * \brief The encoding of an address represented by an SG array.
 *
 * This struct is used to indicate a position of data or some other
 * offset as represented by an SG array.
 */
typedef struct vmk_SgPosition {
   /** Scatter-gather array the position applies to. */
   vmk_SgArray *sg;

   /** Which  position data in the union to use. */
   vmk_SgPositionType type;

   union {
      /**
       * An offset into the buffer a scatter gather array represents
       * specified using a byte offset into that buffer.
       */
      struct {
         /** Byte offset into the buffer. */
         vmk_ByteCount offset;
      } bufferOffset;
      
      /**
       * An offset into the buffer a scatter gather array represents
       * specified using one of the scatter-gather array's
       * scatter-gather element and a byte offset from the address
       * encoding that element.
       */
      struct {
         /**
          * Element in the scatter-gather array representing where
          * the buffer offset is.
          */
         vmk_uint32 element;

         /** Byte offset from the element's starting address. */
         vmk_ByteCountSmall offset;
      } element;
   };
} vmk_SgPosition;

/**
 * \brief Scatter-gather component
 *
 * A scatter gather component is a scatter gather array as well
 * as a specific data object offset and length describing
 * the start location and size of the I/O operation.
 */
typedef struct vmk_SgComponent {
   /**
    * Byte offset within a data object.
    */
   vmk_ByteCount ioOffset;
   /**
    * Number of bytes of this I/O operation.
    */
   vmk_ByteCount ioLength;
   /**
    * The scatter-gather array specifying the buffer addresses
    * used to satisfy this piece of the I/O operation.
    */
   vmk_SgArray *sg;
} vmk_SgComponent;

/**
 * \brief Scatter-gather component array.
 *
 * A scatter gather component array is a set of I/O operations to
 * a given data object where a unique data object offset, size,
 * and scatter gather array can be specified for each piece of
 * the operation.
 */
typedef struct vmk_SgComponentArray {
   /** \brief The number of SgComponents this array has. */
   vmk_int32   maxComponents;
   /** \brief Number of SgComponents currently in-use. */
   vmk_int32   numComponents;

   /** \brief Reserved. */
   vmk_uint64 reserved;
   vmk_uint64 reserved2;

   /** \brief Array of scatter gather components. Should be
    * set to zero on init.
    */
   vmk_SgComponent sgComponent[0];
} vmk_SgComponentArray;

/*
 ***********************************************************************
 * vmk_SgComputeAllocSize--                                       */ /**
 *
 * \ingroup ScatterGather
 * \brief Compute the number of bytes to allocate for a scatter-gather
 *        array.
 *
 * \note This function will not block.
 *
 * \param[in]  maxElems  Max number of elements in the scatter-
 *                       gather array.
 *
 * \returns Number of bytes necessary to contain a scatter-gather array
 *          that holds, at most, the specified number of scatter-gather
 *          elements.
 *
 ***********************************************************************
 */
static inline vmk_ByteCount
vmk_SgComputeAllocSize(vmk_uint32 maxElems) {
   return (sizeof(vmk_SgArray) + (maxElems) * sizeof(vmk_SgElem));
}


/*
 ***********************************************************************
 * vmk_SgArrayOpAlloc--                                           */ /**
 *
 * \ingroup ScatterGather
 * \brief Callback to allocate and initialize a new scatter-gather
 *        array.
 * 
 * \note Callbacks of this type may not block.
 *
 * The returned array should have it's maxElems field set correctly
 * and nbElems field set to zero.
 *
 * \param[in]  handle      Opaque scatter-gather ops handle.
 * \param[out] sg          The new scatter gather array.
 * \param[in]  numElems    Max elements the new array must support.
 * \param[in]  private     Private data from vmk_SgCreateOpsHandle().
 *
 * \retval VMK_OK          The allocation succeeded.
 * \retval VMK_NO_MEMORY   Not enough memory to allocate a new
 *                         scatter-gather element.
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_SgArrayOpAlloc)(vmk_SgOpsHandle handle,
                                               vmk_SgArray **sg,
                                               vmk_uint32 numElems,
                                               void *private);

/*
 ***********************************************************************
 * vmk_SgComponentArrayOpAlloc--                                  */ /**
 *
 * \ingroup ScatterGather
 * \brief Callback to allocate and initialize a new scatter-gather
 *        component array.
 *
 * \note Callbacks of this type may not block.
 *
 * The returned array should have its maxLength field set correctly
 * and length field set to zero.
 *
 * \param[in]  handle        Opaque scatter-gather ops handle.
 * \param[out] sgComponent   The new scatter gather component array.
 * \param[in]  maxComponents Max scatter-gather operations the new
 *                           array must support.
 * \param[in]  private       Private data from vmk_SgComponentCreateOpsHandle().
 *
 * \retval VMK_OK            The allocation succeeded.
 * \retval VMK_NO_MEMORY     Not enough memory to allocate a new
 *                           scatter-gather component array.
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_SgComponentArrayOpAlloc)(
   vmk_SgComponentOpsHandle handle,
   vmk_SgComponentArray **sgComponent,
   vmk_uint32 maxComponents,
   void *private);


/*
 ***********************************************************************
 * vmk_SgArrayOpFree--                                            */ /**
 *
 * \ingroup ScatterGather
 * \brief Callback to free an existing scatter-gather array.
 *
 * \note Callbacks of this type may not block.
 *
 * \param[in] handle    Opaque scatter-gather ops handle.
 * \param[in] sg        scatter-gather array to free.
 * \param[in] private   Private data from vmk_SgCreateOpsHandle().
 *
 * \retval VMK_OK    The free succeeded.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_SgArrayOpFree)(vmk_SgOpsHandle handle,
                                              vmk_SgArray *sg,
                                              void *private);

/*
 ***********************************************************************
 * vmk_SgComponentArrayOpFree--                                   */ /**
 *
 * \ingroup ScatterGather
 * \brief Callback to free an existing scatter-gather component array.
 *
 * \note Callbacks of this type may not block.
 *
 * \param[in] handle      Opaque scatter-gather ops handle.
 * \param[in] sgComponent scatter-gather component array to free.
 * \param[in] private     Private data from vmk_SgCreateOpsHandle().
 *
 * \retval VMK_OK         The free succeeded.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_SgComponentArrayOpFree)(
   vmk_SgComponentOpsHandle handle,
   vmk_SgComponentArray *sgComponent,
   void *private);

/**
 * \brief Scatter-gather array operations.
 *
 *  Routines not implemented by the caller must be set to NULL.
 *
 *  Caller may override default behavior for any routine by supplying
 *  the routines.
 */
typedef struct vmk_SgArrayOps {
   /** Handler invoked when allocating scatter-gather arrays. */
   vmk_SgArrayOpAlloc alloc;

   /** Handler invoked when freeing scatter-gather arrays. */
   vmk_SgArrayOpFree free;
} vmk_SgArrayOps;

/**
 * \brief Scatter-gather components array operations.
 *
 *  Routines not implemented by the caller must be set to NULL.
 *
 *  Caller may override default behavior for any routine by supplying
 *  the routines.
 */
typedef struct vmk_SgComponentArrayOps {
   /** Handler invoked when allocating scatter-gather component arrays. */
   vmk_SgComponentArrayOpAlloc alloc;

   /** Handler invoked when freeing scatter-gather component arrays. */
   vmk_SgComponentArrayOpFree free;
} vmk_SgComponentArrayOps;

/*
 ***********************************************************************
 * vmk_SgComputeMaxEntries--                                      */ /**
 *
 * \brief Compute worst-case maximum entries to cover a virtually
 *        addressed buffer.
 *
 * \note This will only return the entries necessary for a
 *       machine-address scatter-gather array.
 * \note This function will not block.
 *
 * \param[in]  bufferStart    Virtual address of the buffer.
 * \param[in]  size           Length of the buffer in bytes.
 * \param[out] numElems       Number of scatter-gather entries needed.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SgComputeMaxEntries(void *bufferStart,
                                         vmk_ByteCount size,
                                         vmk_uint32 *numElems);

/*
 ***********************************************************************
 * vmk_SgCreateOpsHandle--                                        */ /**
 *
 * \ingroup ScatterGather
 * \brief Create an opaque handle for scatter-gather operations.
 *
 * The handle is used by other routines to invoke callbacks and track
 * other state related to scatter-gather operations.
 *
 * \note   If ops is non-NULL, both an alloc and a free method must
 *         be provided.
 * \note This function will not block.
 *
 * \param[in]  heapId      HeapID to allocate memory on.
 * \param[out] handle      Opaque scatter-gather ops handle.
 * \param[in]  ops         Scatter-gather ops to associate with
 *                         the opaque handle.
 *                         If this argument is NULL, then the
 *                         default set of scatter-gather ops
 *                         will be used and the supplied heap
 *                         will be used to allocate scatter-gather
 *                         arrays.
 * \param[in]  private     Private data passed to each vmk_SgArrayOps
 *                         method when it is invoked.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SgCreateOpsHandle(vmk_HeapID heapId,
				       vmk_SgOpsHandle *handle,
                                       vmk_SgArrayOps *ops,
                                       void *private);

/*
 ***********************************************************************
 * vmk_SgComponentCreateOpsHandle--                               */ /**
 *
 * \ingroup ScatterGather
 * \brief Create an opaque handle for scatter-gather component operations.
 *
 * The handle is used by other routines to invoke callbacks and track
 * other state related to scatter-gather component operations.
 *
 * \note   If ops is non-NULL, both an alloc and a free method must
 *         be provided.
 * \note This function will not block.
 *
 * \param[in]  heapId      HeapID to allocate memory on.
 * \param[out] handle      Opaque scatter-gather component ops handle.
 * \param[in]  ops         Scatter-gather compoents ops to associate with
 *                         the opaque handle.
 *                         If this argument is NULL, then the
 *                         default set of scatter-gather component ops
 *                         will be used and the supplied heap
 *                         will be used to allocate scatter-gather
 *                         component arrays.
 * \param[in]  private     Private data passed to each
 *                         vmk_SgComponentArrayOps method when it
 *                         is invoked.
 *
 * \retval VMK_OK          The handle creation succeeded.
 * \retval VMK_BAD_PARAM   The heapId or ops setting is invalid.
 * \retval VMK_NO_MEMORY   Not enough memory to allocate a new
 *                         handle.
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SgComponentCreateOpsHandle(
   vmk_HeapID heapId,
   vmk_SgComponentOpsHandle *handle,
   vmk_SgComponentArrayOps *ops,
   void *private);

/*
 ***********************************************************************
 * vmk_SgDestroyOpsHandle--                                       */ /**
 *
 * \ingroup ScatterGather
 * \brief Destroy opaque handle for scatter-gather operations.
 *
 * \note This function will not block.
 *
 * \param[in] handle  Opaque scatter-gather ops handle to be destroyed.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SgDestroyOpsHandle(vmk_SgOpsHandle handle);

/*
 ***********************************************************************
 * vmk_SgComponentDestroyOpsHandle--                              */ /**
 *
 * \ingroup ScatterGather
 * \brief Destroy opaque handle for scatter-gather component operations.
 *
 * \note This function will not block.
 *
 * \param[in] handle  Opaque scatter-gather component ops handle to
 *                    be destroyed.
 *
 * \retval VMK_OK     The handle was destroyed.
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SgComponentDestroyOpsHandle(
   vmk_SgComponentOpsHandle handle);

/*
 ***********************************************************************
 * vmk_SgAlloc--                                                  */ /**
 *
 * \ingroup ScatterGather
 * \brief Allocate a scatter-gather array with a given number of entries.
 *
 * \note This function will not block.
 *
 * \param[in] handle       Opaque scatter-gather ops handle.
 * \param[out] sg          New scatter-gather array.
 * \param[in] maxElements  Maximum number of elements the new
 *                         scatter-gather array should have.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SgAlloc(vmk_SgOpsHandle handle,
                             vmk_SgArray **sg,
                             vmk_uint32 maxElements);

/*
 ***********************************************************************
 * vmk_SgComponentAlloc--                                         */ /**
 *
 * \ingroup ScatterGather
 * \brief Allocate a scatter-gather component array with a given number
 * of component entries.
 *
 * \note This function will not block.
 *
 * \param[in] handle       Opaque scatter-gather component ops handle.
 * \param[out] sgComponent New scatter-gather component array.
 * \param[in] maxLength    Maximum number of components the new
 *                         sactter-gather component array should have.
 *
 * \retval VMK_OK          The allocation succeeded.
 * \retval VMK_NO_MEMORY   Not enough memory to allocate the array.
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SgComponentAlloc(vmk_SgComponentOpsHandle handle,
                                      vmk_SgComponentArray **sgComponent,
                                      vmk_uint32 maxLength);

/*
 ***********************************************************************
 * vmk_SgAllocWithInit--                                          */ /**
 *
 * \ingroup ScatterGather
 * \brief Allocate and initialize a scatter-gather array with
 *        the machine-addresses representing a buffer in
 *        virtual-address space.
 *
 * \note This will always result in a machine-address scatter-gather
 *       array.
 * \note This function will not block.
 *
 * \param[in] handle       Opaque scatter-gather ops handle.
 * \param[in] sg           Scatter-gather array to initialize.
 * \param[in] bufferStart  Virtual address of buffer.
 * \param[in] size         Size in bytes of the buffer.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SgAllocWithInit(vmk_SgOpsHandle handle,
                                     vmk_SgArray **sg,
                                     void *bufferStart,
                                     vmk_ByteCount size);

/*
 ***********************************************************************
 * vmk_SgInit--                                                   */ /**
 *
 * \ingroup ScatterGather
 * \brief Initialize a given scatter-gather array with the machine-
 *        addresses representing a buffer in virtual address space.
 *
 * The scatter-gather array must have enough scatter-gather entries
 * to describe the machine address ranges backing the given buffer.
 *
 * \note This call is for initializing scatter-gather arrays that
 *       represent machine-address ranges.
 * \note This function will not block.
 *
 * \param[in] handle       Opaque scatter-gather ops handle.
 * \param[in] sg           Scatter-gather array to initialize.
 * \param[in] bufferStart  Virtual address of buffer.
 * \param[in] size         Size in bytes of the buffer.
 * \param[in] initSGEntry  Starting entry index for initialization.
 *
 * \sa vmk_SgComputeMaxEntries
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SgInit(vmk_SgOpsHandle handle,
                            vmk_SgArray *sg,
                            void *bufferStart,
                            vmk_ByteCount size,
                            vmk_uint32 initSGEntry);

/*
 ***********************************************************************
 * vmk_SgFree --                                                  */ /**
 *
 * \ingroup ScatterGather
 * \brief Free a scatter-gather array.
 *
 * \note This function will not block.
 *
 * \param[in] handle  Opaque scatter-gather ops handle.
 * \param[in] sgArray Pointer returned by vmk_AllocSgArray()
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SgFree(vmk_SgOpsHandle handle,
                            vmk_SgArray *sgArray);


/*
 ***********************************************************************
 * vmk_SgComponentFree --                                         */ /**
 *
 * \ingroup ScatterGather
 * \brief Free a scatter-gather component array.
 *
 * \note This function will not block.
 *
 * \param[in] handle           Opaque scatter-gather component ops handle.
 * \param[in] sgComponentArray Pointer returned by
 *                             vmk_SgAllocComponentArray().
 *
 * \retval VMK_OK         The array was freed.
 * \retval VMK_BAD_PARAM  The handle is invalid.
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SgComponentFree(
   vmk_SgComponentOpsHandle handle,
   vmk_SgComponentArray *sgComponentArray);

/*
 ***********************************************************************
 * vmk_SgCopyData --                                              */ /**
 *
 * \ingroup ScatterGather
 * \brief Copy a portion of the data represented by one scatter-gather
 *        array to another.
 *
 * This copies the data stored in the machine memory represented by
 * one scatter-gather array into the machine memory represented by
 * another scatter-gather array.
 *
 * \note On error some bytes may have been copied.
 * \note This function will not block.
 *
 * \param[in]  dest              Destination specification.
 * \param[in]  source            Source specification. 
 * \param[in]  bytesToCopy       Number of bytes to copy..
 * \param[out] totalBytesCopied  Number of bytes actually copied when
 *                               this callback completes.
 *
 * \retval VMK_OK       The copy completed successfully
 * \retval VMK_FAILURE  The copy failed. Some bytes may have been copied.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SgCopyData(vmk_SgPosition *dest,
                                vmk_SgPosition *source,
                                vmk_ByteCount bytesToCopy,
                                vmk_ByteCount *totalBytesCopied);

/*
 ***********************************************************************
 * vmk_SgFindPosition--                                           */ /**
 *
 * \ingroup ScatterGather
 * \brief Find where a buffer offset is in a scatter-gather array.
 *
 * This function finds the scatter-gather array element and offset
 * into the memory range the element represents that corresponds to
 * the byte offset into the contiguous virtual buffer that the
 * scatter-gather array represents.
 *
 * \note This call only works with machine-address scatter-gather arrays.
 * \note This function will not block.
 *
 * \param[in]  sgArray        Scatter-gather array to find the offset for.
 * \param[in]  bufOffset      Byte offset into the virtual buffer that
 *                            the scatter-gather array represents.
 * \param[out] position       An element-type position that represents the
 *                            virtual buffer offset.
 *
 * \retval VMK_OK       The copy completed successfully
 * \retval VMK_FAILURE  The copy failed. Some bytes may have been copied.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SgFindPosition(vmk_SgArray *sgArray,
                                    vmk_ByteCount bufOffset,
                                    vmk_SgPosition *position);

/*
 ***********************************************************************
 * vmk_GetSgDataLen --                                            */ /**
 *
 * \ingroup ScatterGather
 * \brief Compute the size of a scatter-gather list's payload in bytes.
 *
 * \note This function will not block.
 *
 ***********************************************************************
 */
vmk_ByteCount vmk_SgGetDataLen(vmk_SgArray *sgArray);

/*
 ***********************************************************************
 * vmk_GetSgComponentDataLen --                                   */ /**
 *
 * \ingroup ScatterGather
 * \brief Compute the size of a scatter-gather component list's
 *        payload in bytes.
 *
 * \note This function will not block.
 *
 ***********************************************************************
 */
vmk_ByteCount vmk_SgComponentGetDataLen(
   vmk_SgComponentArray *sgComponentArray);

/*
 ***********************************************************************
 * vmk_SgCopyTo--                                                 */ /**
 *
 * \ingroup ScatterGather
 * \brief Copy data from a buffer to the machine-addresses
 *        defined in a scatter-gather array.
 *
 * \note On failure, some bytes may have been copied.
 * \note This call only works with machine-address scatter-gather arrays.
 * \note This function will not block.
 * 
 * \retval VMK_OK       The copy completed successfully.
 * \retval VMK_FAILURE  The copy failed. Some bytes may have been copied.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SgCopyTo(vmk_SgArray *sgArray,
                              void *dataBuffer,
                              vmk_ByteCount dataLen);

/*
 ***********************************************************************
 * vmk_SgCopyFrom--                                               */ /**
 *
 * \ingroup ScatterGather
 * \brief Copy data from the machine-addresses of a scatter-gather
 *        array to a buffer.
 *
 * \note On failure, some bytes may have been copied.
 * \note This call only works with machine-address scatter-gather arrays.
 * \note This function will not block.
 *
 * \retval VMK_OK       The copy completed successfully.
 * \retval VMK_FAILURE  The copy failed. Some bytes may have been copied.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SgCopyFrom(void *dataBuffer,
                                vmk_SgArray *sgArray,
                                vmk_ByteCount dataLen);

#endif /* _VMKAPI_SCATTER_GATHER_H_ */
/** @} */
