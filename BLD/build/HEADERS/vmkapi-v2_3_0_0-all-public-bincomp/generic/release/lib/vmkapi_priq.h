/* **********************************************************
 * Copyright 2006 - 2012 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 *******************************************************************************
 * PriQ                                                                   */ /**
 * \addtogroup Lib
 * @{
 * \defgroup PriQ Priority Queue
 *
 * These are interfaces for priority queues.
 *
 * The keys can be duplicated, the ordering will be kept accross the elements,
 * but the values are expected to be unique. The only reason for that is
 * because the vmk_PriQRekey uses the value as an unique identifier to update
 * the key of an element. If two elements with the same value are stored in the
 * queue and the vmk_PriQRekey is used on this element, then the behavior is
 * undefined.
 *
 * @{
 *******************************************************************************
 */

#ifndef _VMKAPI_PRIQ_H_
#define _VMKAPI_PRIQ_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/**
 * \brief Type of the priority queue.
 *
 * \details A priority queue can have elements with the smallest or the biggest
 *          key on front. This enum is used to select the type of priority
 *          queue we use.
 */
typedef enum vmk_PriQType {
   /**
    * \brief The queue will be a "min priority queue", meaning that the elements
    *        with the lowest priority will be on the front.
    */
   VMK_PRIQ_TYPE_MIN,
   /**
    * \brief The queue will be a "max priority queue", meaning that the elements
    *        with the highest priority will be on the front.
    */
   VMK_PRIQ_TYPE_MAX,
} vmk_PriQType;

/**
 * \brief Key used to insert an element into the priority queue.
 */
typedef vmk_uint64 vmk_PriQKey;

/**
 * \brief Opaque data structure for the priority queue.
 */
typedef struct vmk_PriQInternal *vmk_PriQHandle;

/*
 *******************************************************************************
 * vmk_PriQCreate --                                                      */ /**
 *
 * \brief Create a new priority queue.
 *
 * \note vmk_PriQDestroy() needs to be called once done with the priority queue.
 *
 * \note The priority queue returned does not come with locking, it is the
 *       caller's responsibility to provide such mechanism if needed.
 *
 * \param[in]  moduleID   Module ID requesting the priority queue.
 * \param[in]  heapID     The heap used for priority queue internal allocation.
 * \param[in]  type       Type of the priority queue.
 * \param[in]  numElems   Number of elements in the priority queue. The queue
 *                        can be later resized if needed.
 * \param[out] priQ       Handle on the priority queue.
 *
 * \retval VMK_OK         Priority queue initialization and allocation was
 *                        successful.
 * \retval VMK_NO_MEMORY  Memory allocation failure.
 * \retval VMK_BAD_PARAM  An invalid parameter was provided.
 *
 *******************************************************************************
 */
VMK_ReturnStatus
vmk_PriQCreate(vmk_ModuleID moduleID,
               vmk_HeapID heapID,
               vmk_PriQType type,
               vmk_uint64 numElems,
               vmk_PriQHandle *priQ);

/*
 *******************************************************************************
 * vmk_PriQDestroy --                                                     */ /**
 *
 * \brief Destroy a priority queue and its associated resources.
 *
 * \param[in]  priQ       Handle on the priority queue.
 *
 * \retval VMK_OK         Everything went fine, the priority queue is released.
 * \retval VMK_BUSY       The priority queue was not empty. Unable to release it.
 * \retval VMK_BAD_PARAM  An invalid parameter was provided.
 *
 *******************************************************************************
 */
VMK_ReturnStatus
vmk_PriQDestroy(vmk_PriQHandle priQ);

/*
 *******************************************************************************
 * vmk_PriQClear --                                                       */ /**
 *
 * \brief Clear the contents of a priority queue.
 *
 * \param[in]  priQ       Handle on the priority queue.
 *
 * \retval VMK_OK         Everything went fine, the priority queue was emptied.
 * \retval VMK_BAD_PARAM  An invalid parameter was provided.
 *
 *******************************************************************************
 */
VMK_ReturnStatus
vmk_PriQClear(vmk_PriQHandle priQ);

/*
 *******************************************************************************
 * vmk_PriQTotalElems --                                                   */ /**
 *
 * \brief Get the total number of slots in the priority queue.
 *
 * \param[in]  priQ       Handle on the priority queue.
 * \param[out] totalElems Pointer to the output data.
 *
 * \retval VMK_OK         The operation was successful.
 * \retval VMK_BAD_PARAM  An invalid parameter was provided.
 *
 *******************************************************************************
 */
VMK_ReturnStatus
vmk_PriQTotalElems(vmk_PriQHandle priQ,
                   vmk_uint64 *totalElems);

/*
 *******************************************************************************
 * vmk_PriQUsedElems --                                                    */ /**
 *
 * \brief Get the number of used slots in the priority queue.
 *
 * \param[in]  priQ       Handle on the priority queue.
 * \param[out] usedElems  Pointer to the output data.
 *
 * \retval VMK_OK         The operation was successful.
 * \retval VMK_BAD_PARAM  An invalid parameter was provided.
 *
 *******************************************************************************
 */
VMK_ReturnStatus
vmk_PriQUsedElems(vmk_PriQHandle priQ,
                  vmk_uint64 *usedElems);

/*
 *******************************************************************************
 * vmk_PriQIsEmpty --                                                     */ /**
 *
 * \brief Check whether or not a priority queue is empty.
 *
 * \param[in]  priQ       Handle on the priority queue.
 *
 * \retval VMK_TRUE       The priority queue is empty.
 * \retval VMK_FALSE      The priority queue still contains some elements.
 *
 *******************************************************************************
 */
static inline vmk_Bool
vmk_PriQIsEmpty(vmk_PriQHandle priQ)
{
   VMK_ReturnStatus status;
   vmk_uint64 usedElems;

   status = vmk_PriQUsedElems(priQ, &usedElems);
   VMK_ASSERT(status == VMK_OK);

   return (usedElems == 0);
}

/*
 *******************************************************************************
 * vmk_PriQResize --                                                      */ /**
 *
 * \brief Resize a previously allocated priority queue.
 *
 * \param[in]  priQ       Handle on the priority queue.
 * \param[in]  numElems   New number of elements of the priority queue.
 *
 * \retval VMK_OK         Resize operation was successful.
 * \retval VMK_BUSY       Trying to shrink the queue to a size too small for the
 *                        elements that are already in there.
 * \retval VMK_NO_MEMORY  Memory allocation failure. The priority queue is left
 *                        unchanged.
 * \retval VMK_BAD_PARAM  An invalid parameter was provided.
 *
 *******************************************************************************
 */
VMK_ReturnStatus
vmk_PriQResize(vmk_PriQHandle priQ,
               vmk_uint64 numElems);

/*
 *******************************************************************************
 * vmk_PriQInsert --                                                      */ /**
 *
 * \brief Insert an element in the priority queue.
 *
 * \param[in]  priQ       Handle on the priority queue.
 * \param[in]  key        Key of the new element. This is the actual "priority"
 *                        that is used to sort elements in the queue.
 * \param[in]  value      Opaque value to store with the key.
 *
 * \retval VMK_OK         Insertion was successful.
 * \retval VMK_LIMIT_EXCEEDED  The priority queue is already full. The element
 *                        was not inserted. The user needs to resize the queue.
 * \retval VMK_BAD_PARAM  An invalid parameter was provided.
 *
 *******************************************************************************
 */
VMK_ReturnStatus
vmk_PriQInsert(vmk_PriQHandle priQ,
               vmk_PriQKey key,
               void *value);

/*
 *******************************************************************************
 * vmk_PriQFirst --                                                       */ /**
 *
 * \brief Get the first element of the priority queue and/or its associated key.
 *
 * \note This does not remove the first element from the priority queue. One
 *       needs to use vmk_PriQExtractFirst() for that purpose.
 *
 * \param[in]  priQ       Handle on the priority queue.
 * \param[out] key        Pointer to the output key. If this parameter is NULL,
 *                        then it is ignored.
 * \param[out] value      Pointer to the output value. If this parameter is
 *                        NULL, then it is ignored.
 *
 * \retval VMK_OK         The first element of the queue has been "found" and
 *                        stored in the output parameters.
 * \retval VMK_BAD_PARAM  An invalid parameter was provided.
 * \retval VMK_NOT_FOUND  The queue was empty.
 *
 *******************************************************************************
 */
VMK_ReturnStatus
vmk_PriQFirst(vmk_PriQHandle priQ,
              vmk_PriQKey *key,
              void **value);

/*
 *******************************************************************************
 * vmk_PriQExtractFirst --                                                */ /**
 *
 * \brief Returns and deletes the first element of the priority queue.
 *
 * \param[in]  priQ       Handle on the priority queue.
 * \param[out] key        Pointer to the output key. If this parameter is NULL,
 *                        then it is ignored.
 * \param[out] value      Pointer to the output value. If this parameter is
 *                        NULL, then it is ignored.
 *
 * \retval VMK_OK         The first element of the queue has been stored in
 *                        output parameters and deleted from the queue.
 * \retval VMK_BAD_PARAM  An invalid parameter was provided.
 * \retval VMK_NOT_FOUND  The queue was empty.
 *
 *******************************************************************************
 */
VMK_ReturnStatus
vmk_PriQExtractFirst(vmk_PriQHandle priQ,
                     vmk_PriQKey *key,
                     void **value);

/*
 *******************************************************************************
 * vmk_PriQExtract --                                                     */ /**
 *
 * \brief Returns and deletes the element whose value is passed as argument.
 *
 * \note As keys in a priority queue can be duplicated, the only way to
 *       uniquely identify an element is to use the pointer referencing it.
 *
 * \param[in]  priQ       Handle on the priority queue.
 * \param[in]  value      Value that was initially stored in the queue and that
 *                        we want to extract.
 * \param[out] key        Pointer to the output key. If this parameter is NULL,
 *                        then it is ignored.
 *
 * \retval VMK_OK         The element of the queue has been stored in output
 *                        parameters and deleted from the queue.
 * \retval VMK_BAD_PARAM  An invalid parameter was provided.
 * \retval VMK_NOT_FOUND  The element was not found in the queue.
 *
 *******************************************************************************
 */
VMK_ReturnStatus
vmk_PriQExtract(vmk_PriQHandle priQ,
                void *value,
                vmk_PriQKey *key);

/*
 *******************************************************************************
 * vmk_PriQFind --                                                     */ /**
 *
 * \brief Returns the element whose value is passed as argument.
 *
 * \note As keys in a priority queue can be duplicated, the only way to
 *       uniquely identify an element is to use the pointer referencing it.
 *
 * \param[in]  priQ       Handle on the priority queue.
 * \param[in]  value      Value that was initially stored in the queue and that
 *                        we want to find.
 * \param[out] key        Pointer to the output key. If this parameter is NULL,
 *                        then it is ignored.
 *
 * \retval VMK_OK         The element of the queue has been stored in output
 *                        parameters and deleted from the queue.
 * \retval VMK_BAD_PARAM  An invalid parameter was provided.
 * \retval VMK_NOT_FOUND  The element was not found in the queue.
 *
 *******************************************************************************
 */
VMK_ReturnStatus
vmk_PriQFind(vmk_PriQHandle priQ,
             void *value,
             vmk_PriQKey *key);

/*
 *******************************************************************************
 * vmk_PriQRekeyFirst --                                                  */ /**
 *
 * \brief Update the key of the first element in the queue.
 *
 * \details This will lead to the first element beeing moved in the queue if
 *          needed.
 *
 * \param[in]  priQ       Handle on the priority queue.
 * \param[in]  newKey     New key to apply to the first element in the queue.
 *
 * \retval VMK_OK         The first element of the queue has been updated.
 * \retval VMK_BAD_PARAM  An invalid parameter was provided.
 * \retval VMK_NOT_FOUND  The queue was empty.
 *
 *******************************************************************************
 */
VMK_ReturnStatus
vmk_PriQRekeyFirst(vmk_PriQHandle priQ,
                   vmk_PriQKey newKey);

/*
 *******************************************************************************
 * vmk_PriQRekey --                                                       */ /**
 *
 * \brief Update the key of the entry whose value is passed as argument.
 *
 * \details This will lead to the element being moved in the queue if needed.
 *
 * \note As keys in a priority queue can be duplicated, the only way to
 *       uniquely identify an element is to use the pointer referencing it.
 *
 * \param[in]  priQ       Handle on the priority queue.
 * \param[in]  value      Value that was initially stored in the queue, for
 *                        which we will update the key.
 * \param[in]  newKey     New key to apply to the value.
 *
 * \retval VMK_OK         The element's key has been updated.
 * \retval VMK_BAD_PARAM  An invalid parameter was provided.
 * \retval VMK_NOT_FOUND  The element was not found in the queue.
 *
 *******************************************************************************
 */
VMK_ReturnStatus
vmk_PriQRekey(vmk_PriQHandle priQ,
              void *value,
              vmk_PriQKey newKey);

#endif /* _PRIQ_MINMAXHEAP_H_ */
/** @} */
/** @} */
