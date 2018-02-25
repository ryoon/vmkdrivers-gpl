/* **********************************************************
 * Copyright 2008 - 2009 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * CSList                                                         */ /**
 * \addtogroup Lib
 * @{
 * \defgroup CSList Counting Singly-Linked List Management
 *
 * Singly-linked lists with an element counter.
 * 
 * @{ 
 ***********************************************************************
 */

#ifndef _VMKAPI_CSLIST_H_
#define _VMKAPI_CSLIST_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/*
 * Structure representing the counting singly-linked list.
 */

typedef struct vmk_CSList {
   /** Underlying singly-linked list */
   vmk_SList slist;
   
   /** Current number of elements in the singly linked list */
   vmk_uint32 numElements;
} vmk_CSList;

/*
 ***********************************************************************
 * VMK_CSLIST_FORALL --                                           */ /**
 *
 * \brief for-loop replacement macro to scan through a list from
 *        the first to the last list member.
 *
 * \note Expressions that contain side effects aren't valid as 
 *       parameters (example: removal).
 *
 * \sa VMK_CSLIST_FORALL_SAFE
 *
 * \param[in]     cslist   The list to scan.
 * \param[in,out] current  Loop pointer that is updated with the current
 *                         list member each time through the loop.
 *
 ***********************************************************************
 */

#define VMK_CSLIST_FORALL(cslist, current) \
        VMK_SLIST_FORALL(&((cslist)->slist), current)

/*
 ***********************************************************************
 * VMK_CSLIST_FORALL_SAFE --                                      */ /**
 *
 * \brief for-loop replacement macro to scan through a list from
 *        the first to the last list member.
 *
 * \note This macro can be used when removal of element is needed while
 *       looping.
 *
 * \note This macro is pretty inefficient as it maintains 3 pointers
 *       per iteration and has additional comparisons to do so. In
 *       case of performance critical loops it is advisable not to use
 *       it and do the necessary bookkeeping (a subset of this)
 *       manually.
 *
 * \sa VMK_CSLIST_FORALL
 *
 * \param[in]     cslist   The list to scan.
 * \param[in,out] prev     Loop pointer that is updated each time through
 *                         the loop with the previous list member.
 * \param[in,out] current  Loop pointer that is updated each time through
 *                         the loop with the current list member.
 * \param[in,out] next     Loop pointer that is updated each time through
 *                         the loop with the next list member.
 *
 ***********************************************************************
 */

#define VMK_CSLIST_FORALL_SAFE(cslist, prev, current, next) \
        VMK_SLIST_FORALL_SAFE(&((cslist)->slist), prev, current, next)

/*
 ***********************************************************************
 * VMK_CSLIST_FORALL_AFTER --                                     */ /**
 *
 * \brief for-loop replacement macro to scan through a list from
 *        the current to the last list member.
 *
 * \note Expressions that contain side effects aren't valid as 
 *       parameters (example: removal).
 *
 * \sa VMK_CSLIST_FORALL
 * \sa VMK_CSLIST_FORALL_SAFE
 *
 * \param[in]     cslist   The list to scan.
 * \param[in,out] current  Loop pointer that indicate the element where to
 *                         start and is updated with the current list member 
 *                         each time through the loop.
 *
 ***********************************************************************
 */

#define VMK_CSLIST_FORALL_AFTER(cslist, current) \
        VMK_SLIST_FORALL_AFTER(&((cslist)->slist), current)

/*
 ***********************************************************************
 * vmk_CSListIsEmpty --                                           */ /**
 *
 * \brief Checks whether a list is empty or not.
 *
 * \param[in]  list        Target list.
 *
 * \retval     VMK_TRUE    The list is empty.
 * \retval     VMK_FALSE   The list is not empty.
 *
 ***********************************************************************
 */

static inline vmk_Bool
vmk_CSListIsEmpty(const vmk_CSList *list)
{
   VMK_ASSERT(list);
   return list->numElements == 0 ? VMK_TRUE : VMK_FALSE;
}

/*
 ***********************************************************************
 * vmk_CSListFirst --                                             */ /**
 *
 * \brief Returns the first element (head) of a list.
 *
 * \param[in]  list        Target list.
 *
 * \retval     NULL        The list is empty.
 * \return                 A pointer to the head of the list.
 *
 ***********************************************************************
 */

static inline vmk_SList_Links *
vmk_CSListFirst(const vmk_CSList *list)
{
   VMK_ASSERT(list);
   return vmk_SListFirst(&list->slist);
}

/*
 ***********************************************************************
 * vmk_CSListLast --                                              */ /**
 *
 * \brief Returns the last element (tail) of a list.
 *
 * \param[in]  list        Target list.
 *
 * \retval     NULL        The list is empty.
 * \return                 A pointer to the tail of the list.
 *
 ***********************************************************************
 */

static inline vmk_SList_Links *
vmk_CSListLast(const vmk_CSList *list)
{
   VMK_ASSERT(list);
   return vmk_SListLast(&list->slist);
}

/*
 ***********************************************************************
 * vmk_CSListPrev --                                              */ /**
 *
 * \brief Returns the previous element in a list. Runs in O(n).
 *
 * \param[in]  list        Target list.
 * \param[in]  element     Element whose previous element is asked for.
 *
 * \retval     NULL        We are at the beginning of the list.
 * \return                 A pointer to the previous element.
 *
 ***********************************************************************
 */

static inline vmk_SList_Links *
vmk_CSListPrev(const vmk_CSList *list, const vmk_SList_Links *element)
{
   VMK_ASSERT(list);
   return vmk_SListPrev(&list->slist, element);
}

/*
 ***********************************************************************
 * vmk_CSListNext --                                              */ /**
 *
 * \brief Returns the following link in a list.
 *
 * \param[in]  element     Target list.
 *
 * \retval     NULL        We are at the end of the list.
 * \return                 A pointer to the next element.
 *
 ***********************************************************************
 */

static inline vmk_SList_Links *
vmk_CSListNext(const vmk_SList_Links *element)
{
   return vmk_SListNext(element);
}

/*
 ***********************************************************************
 * vmk_CSListInit --                                              */ /**
 *
 * \brief Initializes a list to be an empty list.
 *
 * \param[in]  cslist      Target list.
 *
 ***********************************************************************
 */

static inline void
vmk_CSListInit(vmk_CSList *cslist)
{
   VMK_ASSERT(cslist);
   vmk_SListInit(&cslist->slist);
   cslist->numElements = 0;
}

/*
 ***********************************************************************
 * vmk_CSListCount --                                             */ /**
 *
 * \brief Returns the number of element in a list.
 *
 * \param[in]  cslist      Target list.
 *
 * \return                 The number of elements in the list.
 *
 ***********************************************************************
 */

static inline vmk_uint32
vmk_CSListCount(const vmk_CSList *cslist)
{
   VMK_ASSERT(cslist);
   return cslist->numElements;
}

/*
 ***********************************************************************
 * vmk_CSListPop --                                               */ /**
 *
 * \brief Returns the first element (head) of a list and remove it
 * from the list.
 *
 * \param[in]  cslist      Target list.
 *
 * \return                 A pointer to the head of the list.
 *
 ***********************************************************************
 */

static inline vmk_SList_Links *
vmk_CSListPop(vmk_CSList *cslist)
{
   vmk_SList_Links *element;
   VMK_ASSERT(cslist);
   
   element = vmk_SListPop(&cslist->slist);
   cslist->numElements--;
   return element;
}

/*
 ***********************************************************************
 * vmk_CSListInsertAtHead --                                      */ /**
 *
 * \brief Inserts a given element at the beginning of the list.
 *
 * \param[in]  cslist      Target list.
 * \param[in]  element     Element to insert.
 *
 ***********************************************************************
 */

static inline void
vmk_CSListInsertAtHead(vmk_CSList *cslist, vmk_SList_Links *element)
{
   VMK_ASSERT(cslist);
   vmk_SListInsertAtHead(&cslist->slist, element);
   cslist->numElements++;
}

/*
 ***********************************************************************
 * vmk_CSListInsertAtTail --                                      */ /**
 *
 * \brief Inserts a given element at the end of the list.
 *
 * \param[in]  cslist      Target list.
 * \param[in]  element     Element to insert.
 *
 ***********************************************************************
 */

static inline void
vmk_CSListInsertAtTail(vmk_CSList *cslist, vmk_SList_Links *element)
{
   VMK_ASSERT(cslist);
   vmk_SListInsertAtTail(&cslist->slist, element);
   cslist->numElements++;
}

/*
 ***********************************************************************
 * vmk_CSListInsertAfter --                                       */ /**
 *
 * \brief Inserts an element after a given element.
 *
 * \param[in]  cslist      Target list.
 * \param[in]  element     Element to insert.
 * \param[in]  after       Element to insert the new element after.
 *
 ***********************************************************************
 */

static inline void
vmk_CSListInsertAfter(vmk_CSList *cslist, vmk_SList_Links *element,
                      vmk_SList_Links *after)
{
   VMK_ASSERT(cslist);
   vmk_SListInsertAfter(&cslist->slist, element, after);
   cslist->numElements++;
}

/*
 ***********************************************************************
 * vmk_CSListRemove --                                            */ /**
 *
 * \brief Removes a given element from the list knowing its predecessor
 *
 * \param[in]  cslist      Target list.
 * \param[in]  element     Element to remove.
 * \param[in]  prev        Element preceding the element to remove.
 *
 ***********************************************************************
 */

static inline void
vmk_CSListRemove(vmk_CSList *cslist, vmk_SList_Links *element,
                 vmk_SList_Links *prev)
{
   VMK_ASSERT(cslist);
   vmk_SListRemove(&cslist->slist, element, prev);
   cslist->numElements--;
}

/*
 ***********************************************************************
 * vmk_CSListRemoveSlow --                                        */ /**
 *
 * \brief Removes a given element from the list. Runs O(n).
 *
 * \param[in]  cslist      Target list.
 * \param[in]  element     Element to remove.
 *
 ***********************************************************************
 */

static inline void
vmk_CSListRemoveSlow(vmk_CSList *cslist, vmk_SList_Links *element)
{
   VMK_ASSERT(cslist);
   vmk_SListRemoveSlow(&cslist->slist, element);
   cslist->numElements--;
}

/*
 ***********************************************************************
 * vmk_CSListAppend --                                            */ /**
 *
 * \brief Appends all elements of a list at the end of another.
 *
 * \note The list appended is initialized to an empty list.
 *
 * \param[in]  cslist1     Target list.
 * \param[in]  cslist2     List to append.
 *
 ***********************************************************************
 */

static inline void
vmk_CSListAppend(vmk_CSList *cslist1, vmk_CSList *cslist2)
{
   VMK_ASSERT(cslist1);
   VMK_ASSERT(cslist2);
   vmk_SListAppend(&cslist1->slist, &cslist2->slist);
   cslist1->numElements += cslist2->numElements;
   cslist2->numElements = 0;
}

/*
 ***********************************************************************
 * vmk_CSListAppendN --                                           */ /**
 *
 * \brief Appends N elements of a list at the end of another.
 *
 * \note The list appended is initialized to an empty list.
 *
 * \param[in]  listDest    Target list.
 * \param[in]  listSrc     List to append.
 * \param[in]  num         Number of elements to append.
 *
 ***********************************************************************
 */

static inline void
vmk_CSListAppendN(vmk_CSList *listDest, vmk_CSList *listSrc, vmk_uint32 num)
{
   VMK_ASSERT(listDest);
   VMK_ASSERT(listSrc);
   VMK_ASSERT(listSrc->numElements >= num);
   
   vmk_SListAppendN(&listDest->slist, &listSrc->slist, num);
   listDest->numElements += num;
   listSrc->numElements -= num;
}

/*
 ***********************************************************************
 * vmk_CSListAppend --                                            */ /**
 *
 * \brief Insert all elements of a list at the beginning of another.
 *
 * \note The list prepended is initialized to an empty list.
 *
 * \param[in]  cslist1     Target list.
 * \param[in]  cslist2     List to prepend.
 *
 ***********************************************************************
 */

static inline void
vmk_CSListPrepend(vmk_CSList *cslist1, vmk_CSList *cslist2)
{
   VMK_ASSERT(cslist1);
   VMK_ASSERT(cslist2);
   vmk_SListPrepend(&cslist1->slist, &cslist2->slist);
   cslist1->numElements += cslist2->numElements;
   cslist2->numElements = 0;
}

/*
 ***********************************************************************
 * vmk_CSListSplitNHead --                                        */ /**
 *
 * \brief Split a list into two list starting a given element.
 *
 * \note The second list must be empty.
 *
 * \param[in]  cslist1     Target list, becomes left part of the list.
 * \param[in]  cslist2     Right part of the list.
 * \param[in]  n           Index of the element where to start
 *                         splitting.
 *
 ***********************************************************************
 */

static inline void
vmk_CSListSplitNHead(vmk_CSList *cslist1, vmk_CSList *cslist2, vmk_uint64 n)
{
   vmk_uint32 min;

   VMK_ASSERT(cslist1);
   VMK_ASSERT(cslist2);
   vmk_SListSplitNHead(&cslist1->slist, &cslist2->slist, n);
   min = cslist1->numElements < n ? cslist1->numElements : n;
   cslist2->numElements += min;
   cslist1->numElements -= min;
}

/*
 ***********************************************************************
 * vmk_CSListReplace --                                           */ /**
 *
 * \brief Replace the given entry with a new entry. Runs O(1).
 *
 * \param[in] list         List destination.
 * \param[in] targetEntry  Entry to replace.
 * \param[in] newEntry     New entry.
 * \param[in] prevEntry    Predecessor of the entry to replace.
 *
 ***********************************************************************
 */

static inline void
vmk_CSListReplace(vmk_CSList *list, 
                  vmk_SList_Links *targetEntry,
                  vmk_SList_Links *newEntry, 
                  vmk_SList_Links *prevEntry)
{
   VMK_ASSERT(list);
   VMK_ASSERT(targetEntry);

   vmk_SListReplace(&list->slist, targetEntry, newEntry, prevEntry);
}

#endif
/** @} */
/** @} */
