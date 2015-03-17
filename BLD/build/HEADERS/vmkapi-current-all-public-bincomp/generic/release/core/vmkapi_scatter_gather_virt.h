/* **********************************************************
 * Copyright 2014 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * ScatterGatherVirt                                              */ /**
 * \defgroup ScatterGatherVirt Scatter Gather Virtual Buffer Management
 *
 * Interfaces to manage discontiguous VA regions of memory for IO.
 *
 * This API is not thread safe. The caller of this API is expected
 * to provide synchronization.
 *
 * Example:
 *
 * 1. vmk_SgVirtOpsHandleCreate: must register ops.alloc and ops.free.
 *    Typically a one-time operation.
 *
 * 2. To create a scatter-gather list of VAs
 *       a. vmk_SgVirtAllocArrayWithInit(..., maxElems,...);
 *             invokes ops.alloc and later initializes the array.
 *       b. For every element in the sgVirtArray
 *             vmk_SgVirtElemInit(...)
 *
 * 3. Freeing the array after use: vmk_SgVirtArryFree(...). This
 *    results in a call to previously registered ops.free.
 *
 * 4. Walking the elements of the sgVirtArray
 *       a. vmk_SgVirtArrayGetUsedElems: get usedElems.
 *       b. For every element i in the sgVirtArray
 *             vmk_SgVirtArrayGetElem(...);
 *             vmk_SgVirtElemGetInfo(...);
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_SCATTER_GATHER_VIRT_H_
#define _VMKAPI_SCATTER_GATHER_VIRT_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/**
 * \brief Opaque handle for scatter-gather VA operations.
 */
typedef struct vmk_SgVirtOpsHandleInt *vmk_SgVirtOpsHandle;

/**
 * \brief Opaque handle for scatter-gather VA element.
 *
 * A scatter-gather element represents only one VA region at a time.
 */
typedef struct vmk_SgVirtElemInt *vmk_SgVirtElem;

/**
 * \brief Opaque handle for scatter-gather VA array.
 *
 * A scatter gather VA array is a collection of contiguous VA
 * regions.
 */
typedef struct vmk_SgVirtArrayInt *vmk_SgVirtArray;

/*
 ***********************************************************************
 * vmk_SgVirtArrayOpAlloc --                                      */ /**
 *
 * \ingroup ScatterGatherVirt
 * \brief Callback to allocate a scatter-gather VA array.
 *
 * This is invoked when vmk_SgVirtArrayAllocWithInit is called.
 *
 * \note Callbacks of this type may not block.
 *
 * \param[in]  sgVirtOpsHandle  Opaque scatter-gather VA ops handle.
 * \param[in]  heapId           HeapID scatter-gather VA ops handle
 *                              is allocated from.
 * \param[in]  handlePrivate    Private data that was registered with
 *                              vmk_SgVirtOpsHandleCreate().
 * \param[out] sgVirtArray      SG VA array to allocate.
 * \param[in]  sgVirtArraySize  Size in bytes of the SG VA array to
 *                              allocate.
 *
 * \retval VMK_OK              The free succeeded.
 * \retval VMK_NO_MEMORY       Not enough memory to allocate a new
 *                             scatter-gather VA array.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus
(*vmk_SgVirtArrayOpAlloc)(vmk_SgVirtOpsHandle sgVirtOpsHandle,
                          vmk_HeapID heapId,
                          void *handlePrivate,
                          vmk_SgVirtArray *sgVirtArray,
                          vmk_ByteCountSmall sgVirtArraySize);

/*
 ***********************************************************************
 * vmk_SgVirtArrayOpFree --                                       */ /**
 *
 * \ingroup ScatterGatherVirt
 * \brief Callback to free an existing scatter-gather VA array.
 *
 * This is invoked when vmk_SgVirtArrayFree is called.
 *
 * \note Callbacks of this type may not block.
 *
 * \param[in]  sgVirtOpsHandle  Opaque scatter-gather VA ops handle.
 * \param[in]  heapId           HeapID scatter-gather VA ops handle
 *                              is allocated from.
 * \param[in]  handlePrivate    Private data that was registered with
 *                              vmk_SgVirtOpsHandleCreate().
 * \param[in]  sgVirtArray      Scatter-gather VA array to free.
 * \param[in]  arrayPrivate     Private data per sgVirtArray.
 *
 * \retval VMK_OK    The free succeeded.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus
(*vmk_SgVirtArrayOpFree)(vmk_SgVirtOpsHandle sgVirtOpsHandle,
                         vmk_HeapID heapId,
                         void *handlePrivate,
                         vmk_SgVirtArray sgVirtArray,
                         void *arrayPrivate);

/**
 * \brief Scatter-gather VA array operations.
 */
typedef struct vmk_SgVirtArrayOps {
   /**
    * Mandatory: Handler invoked when allocating scatter-gather VA
    * arrays.
    */
   vmk_SgVirtArrayOpAlloc alloc;

   /**
    * Mandatory: Handler invoked when freeing scatter-gather VA
    * arrays.
    */
   vmk_SgVirtArrayOpFree free;
} vmk_SgVirtArrayOps;

/*
 ***********************************************************************
 * vmk_SgVirtOpsHandleCreate --                                   */ /**
 *
 * \ingroup ScatterGatherVirt
 * \brief Create an opaque handle for scatter-gather VA operations.
 *
 * The handle is used by other routines to invoke callbacks and track
 * other state related to scatter-gather VA operations.
 *
 * \note The ops must be a non-NULL and a free method must always be
 *       provided.
 * \note This function will not block.
 *
 * \param[in]  heapId           HeapID to allocate memory from, for the
 *                              ops handle.
 * \param[out] sgVirtOpsHandle  Opaque scatter-gather VA ops handle.
 * \param[in]  ops              Scatter-gather VA ops to associate with
 *                              the opaque handle.
 * \param[in]  handlePrivate    Private data passed to each
 *                              vmk_SgVirtArrayOps method when it is
 *                              invoked.
 *
 * \retval VMK_OK          The handle creation succeeded.
 * \retval VMK_BAD_PARAM   NULL handle or invalid ops.
 * \retval VMK_NO_MEMORY   Not enough memory to allocate a new
 *                         scatter-gather VA ops handle.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_SgVirtOpsHandleCreate(vmk_HeapID heapId,
                          vmk_SgVirtOpsHandle *sgVirtOpsHandle,
                          vmk_SgVirtArrayOps *ops,
                          void *handlePrivate);

/*
 ***********************************************************************
 * vmk_SgVirtOpsHandleDestroy --                                  */ /**
 *
 * \ingroup ScatterGatherVirt
 * \brief Destroy opaque handle for scatter-gather VA operations.
 *
 * \note This function will not block.
 *
 * \param[in]  sgVirtOpsHandle  Opaque scatter-gather VA ops handle to be
 *                              destroyed.
 *
 * \retval VMK_OK          The free succeeded.
 * \retval VMK_BAD_PARAM   NULL or invalid handle.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_SgVirtOpsHandleDestroy(vmk_SgVirtOpsHandle sgVirtOpsHandle);

/*
 ***********************************************************************
 * vmk_SgVirtArrayAllocWithInit --                                */ /**
 *
 * \ingroup ScatterGatherVirt
 * \brief Initializes a scatter-gather VA array metadata.
 *
 * This will result in call to vmk_SgVirtArrayOpAlloc() i.e. the
 * callback registered during vmk_SgVirtOpsHandleCreate().
 *
 * \note This function does not initialize elements of the SG VA array.
 * \note This function will not block.
 *
 * \param[in]  sgVirtOpsHandle  Opaque scatter-gather VA ops handle.
 * \param[out] sgVirtArray      New scatter-gather VA array to allocate
 *                              and initialize.
 * \param[in]  maxElems         Maximum number of elements the new
 *                              scatter-gather VA array can have.
 * \param[in]  arrayPrivate     Private data per sgVirtArray.
 *
 * \retval VMK_OK          The initialization succeeded.
 * \retval VMK_BAD_PARAM   Invalid vmk_SgVirtOpsHandle passed in or
 *                         NULL sgVirtArray or maxElems are 0.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_SgVirtArrayAllocWithInit(vmk_SgVirtOpsHandle sgVirtOpsHandle,
                             vmk_SgVirtArray *sgVirtArray,
                             vmk_uint32 maxElems,
                             void *arrayPrivate);

/*
 ***********************************************************************
 * vmk_SgVirtElemInit --                                          */ /**
 *
 * \ingroup ScatterGatherVirt
 * \brief Initializes next element of scatter-gather VA array.
 *
 * The scatter-gather VA array must have enough scatter-gather entries.
 *
 * \note Initializes next element in the scatter-gather VA array with
 *       parameters input to this function.
 * \note After this function returns, the position of the new element
 *       can be determined by calling vmk_SgVirtArrayGetUsedElems.
 * \note This function will not block.
 *
 * \param[in]  sgVirtArray  Scatter-gather VA array to initialize.
 * \param[in]  bufferStart  Virtual address of buffer.
 * \param[in]  length       Length in bytes of the buffer.
 *
 * \retval VMK_OK              The initialization succeeded.
 * \retval VMK_BAD_PARAM       Invalid vmk_SgVirtArray.
 * \retval VMK_LIMIT_EXCEEDED  SG VA array is full.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SgVirtElemInit(vmk_SgVirtArray sgVirtArray,
                                    void *bufferStart,
                                    vmk_uint32 length);

/*
 ***********************************************************************
 * vmk_SgVirtArrayFree --                                         */ /**
 *
 * \ingroup ScatterGatherVirt
 * \brief Free a scatter-gather VA array.
 *
 * This will result in call to vmk_SgVirtArrayOpFree() i.e. the callback
 * registered during vmk_SgVirtOpsHandleCreate().
 *
 * \note This function will not block.
 *
 * \param[in]  sgVirtArray  The SG VA array to free.
 *
 * \retval VMK_OK          Successful.
 * \retval VMK_BAD_PARAM   NULL or invalid handle.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SgVirtArrayFree(vmk_SgVirtArray sgVirtArray);

/*
 ***********************************************************************
 * vmk_SgVirtArrayGetMaxElems --                                  */ /**
 *
 * \ingroup ScatterGatherVirt
 * \brief Get maximum number of elements in a scatter-gather VA array.
 *
 * \note This function will not block.
 *
 * \param[in]  sgVirtArray  Scatter-gather VA array being queried.
 * \param[out] maxElems     The maximum number of elements for this
 *                          scatter-gather VA array.
 *
 * \retval VMK_OK          Successful.
 * \retval VMK_BAD_PARAM   Invalid vmk_SgVirtArray or NULL out arg.
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_SgVirtArrayGetMaxElems(vmk_SgVirtArray sgVirtArray,
                           vmk_uint32 *maxElems);

/*
 ***********************************************************************
 * vmk_SgVirtArrayGetUsedElems --                                 */ /**
 *
 * \ingroup ScatterGatherVirt
 * \brief Get number of elements in-use in a scatter-gather VA array.
 *
 * \note This function will not block.
 *
 * \param[in]  sgVirtArray  Scatter-gather VA array being queried.
 * \param[out] usedElems    The number of elements in-use for this
 *                          scatter-gather VA array.
 *
 * \retval VMK_OK          Successful.
 * \retval VMK_BAD_PARAM   Invalid vmk_SgVirtArray or NULL out arg.
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_SgVirtArrayGetUsedElems(vmk_SgVirtArray sgVirtArray,
                            vmk_uint32 *usedElems);

/*
 ***********************************************************************
 * vmk_SgVirtArrayGetElem --                                      */ /**
 *
 * \ingroup ScatterGatherVirt
 * \brief Get particular element in a scatter-gather VA array.
 *
 * \note This function will not block.
 *
 * \param[in]  sgVirtArray     Scatter-gather VA array being queried.
 * \param[in]  index           Position of the sgVirtElem queried.
 * \param[out] sgVirtElem      The sgVirtElem queried.
 *
 * \retval VMK_OK              Successful.
 * \retval VMK_BAD_PARAM       Invalid vmk_SgVirtArray or NULL out arg.
 * \retval VMK_LIMIT_EXCEEDED  Index out of bounds.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_SgVirtArrayGetElem(vmk_SgVirtArray sgVirtArray,
                       vmk_uint32 index,
                       vmk_SgVirtElem *sgVirtElem);

/*
 ***********************************************************************
 * vmk_SgVirtElemGetInfo --                                       */ /**
 *
 * \ingroup ScatterGatherVirt
 * \brief Get all the info associated with a scatter-gather VA element.
 *
 * \note This function will not block.
 *
 * \param[in]  sgVirtElem Scatter-gather VA element being queried.
 * \param[out] vaddr      VA for the scatter-gather VA element.
 * \param[out] length     Length of the VA.
 *
 * \retval VMK_OK          Successful.
 * \retval VMK_BAD_PARAM   Invalid vmk_SgVirtElem or NULL out arg(s).
 ***********************************************************************
 */
VMK_ReturnStatus vmk_SgVirtElemGetInfo(vmk_SgVirtElem sgVirtElem,
                                       vmk_VA *vaddr,
                                       vmk_uint32 *length);

#endif /* _VMKAPI_SCATTER_GATHER_VIRT_H_ */
/** @} */
