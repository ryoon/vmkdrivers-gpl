/* **********************************************************
 * Copyright 2006 - 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/* **********************************************************
 * Portions of the code in this file require the following
 * copyright notice to be displayed:
 *
 * Copyright (C) 1985, 1988 Regents of the University of California
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * List                                                           */ /**
 * \addtogroup Lib
 * @{
 * \defgroup List Doubly-Linked Lists
 *
 * The following are interfaces for a list abstraction  which enables
 * arbitrary data structures to be linked together into a doubly-linked
 * circular list.
 *
 * A list is composed of a sentinel header and its real members, if any.
 * Thus, an empty list consists of a single header element whose nextPtr
 * and prevPtr fields point to itself.
 *
 * The links are contained in a two-element structure called vmk_ListLinks.
 * A list joins vmk_ListLinks records (that is, each vmk_ListLinks structure
 * points to other vmk_ListLinks structures), but if the vmk_ListLinks is the
 * first field within a larger structure, then the larger structures are
 * effectively linked together as follows:
 *
 * \code
 *            header
 *        (vmk_ListLinks)          First element           Second Element
 *      -----------------       ------------------       -----------------
 * ..-> |    nextPtr    | ----> |  vmk_ListLinks | ----> | vmk_ListLinks |  ----..
 *      | - - - - - - - |       |                |       |               |
 * ..-- |    prevPtr    | <---- |                | <---- |               | <---..
 *      -----------------       - ---  ---  --- -        - ---  ---  --- -
 *                              |    rest of     |       |    rest of    |
 *                              |   structure    |       |   structure   |
 *                              |                |       |               |
 *                              |      ...       |       |      ...      |
 *                              -----------------        -----------------
 * \endcode
 *
 * It is possible to link structures through vmk_ListLinks fields that are
 * not at the beginning of the larger structure, but it is then necessary
 * to use VMK_LIST_ENTRY to extract the surrounding structure from
 * the list element.
 *
 * \par A typical structure might be something like:
 *
 * \code
 * typedef struct {
 *    vmk_ListLinks links;
 *    char ch;
 *    integer flags;
 * } Example;
 * \endcode
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_LIST_H_
#define _VMKAPI_LIST_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/**
 * \brief List links container.
 */
typedef struct vmk_ListLinks {
   struct vmk_ListLinks *prevPtr;
   struct vmk_ListLinks *nextPtr;
} vmk_ListLinks;


/*
 ***********************************************************************
 * VMK_LIST_ENTRY --                                              */ /**
 *
 * \brief Get a pointer to the structure containing a given list
 *        element.
 *
 * \param[in] itemPtr            List element that is contained by
 *                               another structure.
 * \param[in] containerType      C type of the container.
 * \param[in] fieldInContainer   Name of the structure field in the
 *                               container that itemPtr is pointing to.
 *
 * This macro allows a list pointer embedded in a structure to be
 * somwhere other than the initial item in the structure.
 *
 * \par Example:
 * \code
 * typedef struct
 * {
 *     int a;
 *     vmk_ListLinks links;
 *     int b;
 * }
 * myType;
 *
 * myType example;
 * myType *ptr;
 * vmk_ListLinks *listElem;
 *
 * listElem = &(example.links);
 * ptr = VMK_LIST_ENTRY(listElem, myType, links);
 * \endcode
 * "ptr" should now point at "example"
 *
 ***********************************************************************
 */
#define VMK_LIST_ENTRY(itemPtr, containerType, fieldInContainer) \
  ((containerType *) ((vmk_uint8 *) (itemPtr) - \
                      ((unsigned long) &((containerType *)0)->fieldInContainer)))

/*
 ***********************************************************************
 * vmk_ListInit --                                                */ /**
 *
 * \brief Initialize a header pointer to point to an empty list.
 *
 * \param[in] headerPtr    List header to initialize.
 *
 ***********************************************************************
 */
static inline void
vmk_ListInit(vmk_ListLinks *headerPtr)
{
   VMK_ASSERT(headerPtr != NULL);
   headerPtr->nextPtr = headerPtr;
   headerPtr->prevPtr = headerPtr;
}

/*
 ***********************************************************************
 * vmk_ListInitElement --                                         */ /**
 *
 * \brief Initialize a list element.
 *
 * \param[in] elementPtr   List element to initialize.
 *
 ***********************************************************************
 */
static inline void
vmk_ListInitElement(vmk_ListLinks *elementPtr)
{
    elementPtr->prevPtr = (vmk_ListLinks *) NULL;
    elementPtr->nextPtr = (vmk_ListLinks *) NULL;
}


/*
 ***********************************************************************
 * vmk_ListIsUnlinkedElement --                                   */ /**
 *
 * \brief Check whether a given element is part of a list or not.
 *
 * \param[in] elementPtr   List element to check.
 *
 * \retval VMK_TRUE     The element is not in a list.
 * \retval VMK_FALSE    The element is linked into a list.
 *
 ***********************************************************************
 */
static inline vmk_Bool
vmk_ListIsUnlinkedElement(vmk_ListLinks *elementPtr)
{
   return (elementPtr->prevPtr == (vmk_ListLinks *) NULL &&
           elementPtr->nextPtr == (vmk_ListLinks *) NULL);
}


/*
 ***********************************************************************
 * VMK_LIST_FORALL --                                             */ /**
 *
 * \brief for-loop replacement macro to scan through a list from
 *        the first to the last list member.
 *
 * \param[in]  headerPtr   The list to scan
 * \param[out] itemPtr     Loop pointer that is updated with the current
 *                         list member each time through the loop.
 *
 * \note This macro does not tolerate the removal of itemPtr from
 *      the list during the loop.
 *
 * \sa VMK_LIST_FORALL_SAFE
 *
 ***********************************************************************
 */
#define VMK_LIST_FORALL(headerPtr, itemPtr) \
        for (itemPtr = vmk_ListFirst(headerPtr); \
             !vmk_ListIsAtEnd((headerPtr),itemPtr); \
             itemPtr = vmk_ListNext(itemPtr))


/*
 ***********************************************************************
 * VMK_LIST_FORALL_REVERSE --                                     */ /**
 *
 * \brief for-loop replacement macro to scan through a list from
 *        the last to the first list member.
 *
 * \param[in]  headerPtr   The list to scan
 * \param[out] itemPtr     Loop pointer that is updated with the current
 *                         list member each time through the loop.
 *
 * \note This macro does not tolerate the removal of itemPtr from
 *       the list during the loop.
 *
 * \sa VMK_LIST_FORALL_REVERSE_SAFE
 *
 ***********************************************************************
 */
#define VMK_LIST_FORALL_REVERSE(headerPtr, itemPtr) \
        for (itemPtr = vmk_ListLast(headerPtr); \
             !vmk_ListIsAtEnd((headerPtr),itemPtr); \
             itemPtr = vmk_ListPrev(itemPtr))


/*
 ***********************************************************************
 * VMK_LIST_FORALL_SAFE --                                        */ /**
 *
 * \brief for-loop replacement macro to scan through a list from
 *        the first to the last list member.
 *
 * \param[in]  headerPtr   The list to scan.
 * \param[out] itemPtr     Loop pointer that is updated each time
 *                         through the loop with the current list member.
 * \param[out] nextPtr     Loop pointer that is updated each time
 *                         through the loop with the next list member.
 *
 * \note This macro should be used if itemPtr must be removed from
 *       the list during the loop.
 *
 ***********************************************************************
 */
#define VMK_LIST_FORALL_SAFE(headerPtr, itemPtr, nextPtr) \
        for (itemPtr = vmk_ListFirst(headerPtr), nextPtr = vmk_ListNext(itemPtr); \
             !vmk_ListIsAtEnd((headerPtr),itemPtr); \
             itemPtr = nextPtr, nextPtr = vmk_ListNext(nextPtr))


/*
 ***********************************************************************
 * VMK_LIST_FORALL_REVERSE_SAFE --                                */ /**
 *
 * \brief for-loop replacement macro to scan through a list from
 *        the last to the first list member.
 *
 * \param[in]  headerPtr   The list to scan.
 * \param[out] itemPtr     Loop pointer that is updated each time
 *                         through the loop with the current list member.
 * \param[out] prevPtr     Loop pointer that is updated each time
 *                         through the loop with the next list member.
 *
 * \note This macro should be used if itemPtr must be removed from
 *       the list during the loop.
 *
 ***********************************************************************
 */
#define VMK_LIST_FORALL_REVERSE_SAFE(headerPtr, itemPtr, prevPtr) \
        for (itemPtr = vmk_ListLast(headerPtr), prevPtr = vmk_ListPrev(itemPtr); \
             !vmk_ListIsAtEnd((headerPtr),itemPtr); \
             itemPtr = prevPtr, prevPtr = vmk_ListPrev(prevPtr))


/*
 ***********************************************************************
 * VMK_LIST_ITER --                                               */ /**
 *
 * \brief while-loop replacement macro to scan through a list from
 *        a given list member to the last list member.
 *
 * \param[in]     headerPtr   The list to scan.
 * \param[in,out] itemPtr     The first list member to scan. This pointer
 *                            is also updated with the current list member
 *                            each time through the loop.
 *
 * \note This macro does not tolerate the removal of itemPtr from the
 *       list during the loop.
 *
 * \sa VMK_LIST_ITER_SAFE
 *
 ***********************************************************************
 */
#define VMK_LIST_ITER(headerPtr, itemPtr) \
        for (; \
             !vmk_ListIsAtEnd((headerPtr),itemPtr); \
             itemPtr = vmk_ListNext(itemPtr))


/*
 ***********************************************************************
 * VMK_LIST_ITER_REVERSE --                                       */ /**
 *
 * \brief while-loop replacement macro to scan through a list from
 *        a given list member to the first list member.
 *
 * \param[in]     headerPtr   The list to scan.
 * \param[in,out] itemPtr     The first list member to scan. This pointer
 *                            is also updated with the current list member
 *                            each time through the loop.
 *
 * \note This macro does not tolerate the removal of itemPtr from the
 *       list during the loop.
 *
 * \sa VMK_LIST_ITER_REVERSE_SAFE
 *
 ***********************************************************************
 */
#define VMK_LIST_ITER_REVERSE(headerPtr, itemPtr) \
        for (; \
             !vmk_ListIsAtEnd((headerPtr),itemPtr); \
             itemPtr = vmk_ListPrev(itemPtr))


/*
 ***********************************************************************
 * VMK_LIST_ITER_SAFE --                                          */ /**
 *
 * \brief while-loop replacement macro to scan through a list from
 *        a given list member to the last list member.
 *
 * \param[in] headerPtr    The list to scan.
 * \param[in,out] itemPtr  The first list member to scan. This pointer
 *                         is also updated with the current list member
 *                         each time through the loop.
 * \param[out] nextPtr     Loop pointer that is updated each time through
 *                         the loop with the next list member.
 *
 * \note This macro should be used if itemPtr must be removed from
 *       the list during the loop.
 *
 ***********************************************************************
 */
#define VMK_LIST_ITER_SAFE(headerPtr, itemPtr, nextPtr) \
        for (nextPtr = vmk_ListNext(itemPtr); \
             !vmk_ListIsAtEnd((headerPtr),itemPtr); \
             itemPtr = nextPtr, nextPtr = vmk_ListNext(nextPtr))


/*
 ***********************************************************************
 * VMK_LIST_ITER_REVERSE_SAFE --                                  */ /**
 *
 * \brief while-loop replacement macro to scan through a list from
 *        a given list member to the first list member.
 *
 * \param[in] headerPtr    The list to scan.
 * \param[in,out] itemPtr  The first list member to scan. This pointer
 *                         is also updated with the current list member
 *                         each time through the loop.
 * \param[out] prevPtr     Loop pointer that is updated each time through
 *                         the loop with the next list member.
 *
 * \note This macro should be used if itemPtr must be removed from
 *       the list during the loop.
 *
 ***********************************************************************
 */
#define VMK_LIST_ITER_REVERSE_SAFE(headerPtr, itemPtr, prevPtr) \
        for (prevPtr = vmk_ListPrev(itemPtr); \
             !vmk_ListIsAtEnd((headerPtr),itemPtr); \
             itemPtr = prevPtr, prevPtr = vmk_ListPrev(prevPtr))


/*
 ***********************************************************************
 * vmk_ListIsEmpty --                                             */ /**
 *
 * \brief Check if a list does not contain any members.
 *
 * \param[in] headerPtr    The head of the list to check
 *
 * \retval VMK_TRUE     The list is empty
 * \retval VMK_FALSE    The list is not empty
 *
 ***********************************************************************
 */
static inline vmk_Bool
vmk_ListIsEmpty(vmk_ListLinks *headerPtr)
{
   return (headerPtr == headerPtr->nextPtr);
}


/*
 ***********************************************************************
 * vmk_ListIsAtEnd --                                             */ /**
 *
 * \brief Check if itemPtr is pointing just past the last list element.
 *
 * This function is useful for loops where it can be used to check if
 * the loop has scanned all the list elements by checking to see if
 * the loop's list pointer has scanned past the end of the list.
 *
 * \param[in] headerPtr  The head of the list to check.
 * \param[in] itemPtr    The list entry pointer to check.
 *
 * \retval VMK_TRUE     itemPtr points past the last list element.
 * \retval VMK_FALSE    itemPtr does not point past the last
 *                      list element.
 *
 ***********************************************************************
 */
static inline vmk_Bool
vmk_ListIsAtEnd(vmk_ListLinks *headerPtr, vmk_ListLinks *itemPtr)
{
   return (itemPtr == headerPtr);
}


/*
 ***********************************************************************
 * vmk_ListFirst --                                               */ /**
 *
 * \brief Get the first list member in a list.
 *
 * \param[in] headerPtr    The list from which to retrieve the element.
 *
 * \return A pointer to the first member in the list.
 *
 ***********************************************************************
 */
static inline vmk_ListLinks *
vmk_ListFirst(vmk_ListLinks *headerPtr)
{
   return (headerPtr->nextPtr);
}


/*
 ***********************************************************************
 * vmk_ListLast --                                                */ /**
 *
 * \brief Get the last list member in a list.
 *
 * \param[in] headerPtr    The list from which to retrieve the element.
 *
 * \return A pointer to the last member in the list
 *
 ***********************************************************************
 */
static inline vmk_ListLinks *
vmk_ListLast(vmk_ListLinks *headerPtr)
{
   return (headerPtr->prevPtr);
}


/*
 ***********************************************************************
 * vmk_ListPrev --                                                */ /**
 *
 * \brief Return the preceeding list memeber.
 *
 * This function will return the list header if the memeber is the
 * first list member.
 *
 * \param[in] itemPtr   The list member from which to get the
 *                      previous member.
 *
 * \return A pointer to the preceeding list member
 *
 ***********************************************************************
 */
static inline vmk_ListLinks *
vmk_ListPrev(vmk_ListLinks *itemPtr)
{
   return (itemPtr->prevPtr);
}


/*
 ***********************************************************************
 * vmk_ListNext --                                                */ /**
 *
 * \brief Return the following list memeber .
 *
 * This function will return the list header if the memeber is the
 * last list member.
 *
 * \param[in] itemPtr   The list member from which to get the
 *                      following member.
 *
 * \return A pointer to the following list member.
 *
 ***********************************************************************
 */
static inline vmk_ListLinks *
vmk_ListNext(vmk_ListLinks *itemPtr)
{
   return (itemPtr->nextPtr);
}


/*
 ***********************************************************************
 * vmk_ListInsert --                                              */ /**
 *
 * \brief Insert a list element into a list after another element.
 *
 * vmk_ListAfter, vmk_ListBefore, vmk_ListAtFront, and
 * vmk_ListAtRear can be used to determine destPtr.
 *
 * \par For example:
 * \code
 * vmk_ListInsert(myElement, vmk_ListAtFront(myList));
 * \endcode
 *
 * \param[in] itemPtr   List element to insert in the list.
 * \param[in] destPtr   List member to insert after.
 *
 ***********************************************************************
 */
static inline void
vmk_ListInsert(vmk_ListLinks *itemPtr, vmk_ListLinks *destPtr)
{
   VMK_ASSERT(itemPtr != NULL && destPtr != NULL);
   VMK_ASSERT(itemPtr != destPtr);  /* Can't insert something after itself. */
   itemPtr->nextPtr = destPtr->nextPtr;
   itemPtr->prevPtr = destPtr;
   destPtr->nextPtr->prevPtr = itemPtr;
   destPtr->nextPtr = itemPtr;
}


/*
 ***********************************************************************
 * vmk_ListAfter --                                                */ /**
 *
 * \brief Get the appropriate itemPtr for vmk_ListInsert
 *        so that insertion will take place after a given element.
 *
 * \param[in] itemPtr   List member to insert after.
 *
 * \return A pointer to a list element that will allow list
 *         insertion after the given list member.
 *
 ***********************************************************************
 */
static inline vmk_ListLinks *
vmk_ListAfter(vmk_ListLinks *itemPtr)
{
   return itemPtr;
}


/*
 ***********************************************************************
 * vmk_ListBefore --                                              */ /**
 *
 * \brief Get the appropriate itemPtr for vmk_ListInsert()
 *        so that insertion will take place before a given element.
 *
 * \param[in] itemPtr   List member to insert before.
 *
 * \return A pointer to a list element that will allow list
 *         insertion before the given list member.
 *
 ***********************************************************************
 */
static inline vmk_ListLinks *
vmk_ListBefore(vmk_ListLinks *itemPtr)
{
   return itemPtr->prevPtr;
}


/*
 ***********************************************************************
 * vmk_ListAtFront --                                             */ /**
 *
 * \brief Get the appropriate itemPtr for vmk_ListInsert() to insert
 *        at the head of a given list.
 *
 * \param[in] headerPtr    List on which the insertion will take place.
 *
 * \return A pointer to a list element that will allow list insertion
 *         at the head of the given list.
 *
 ***********************************************************************
 */
static inline vmk_ListLinks *
vmk_ListAtFront(vmk_ListLinks *headerPtr)
{
   return headerPtr;
}


/*
 ***********************************************************************
 * vmk_ListAtRear --                                              */ /**
 *
 * \brief Get the appropriate itemPtr for vmk_ListInsert() to insert
 *        at the tail of a given list.
 *
 * \param[in] headerPtr    List on which the insertion will take place.
 *
 * \return A pointer to the list element that will allow list insertion
 *         at the tail of the given list.
 *
 ***********************************************************************
 */
static inline vmk_ListLinks *
vmk_ListAtRear(vmk_ListLinks *headerPtr)
{
   return headerPtr->prevPtr;
}


/*
 ***********************************************************************
 * vmk_ListRemove --                                              */ /**
 *
 * \brief Remove a list element from the list in which it is contained.
 *
 * \param[in] itemPtr   List element to be removed.
 *
 ***********************************************************************
 */
static inline void
vmk_ListRemove(vmk_ListLinks *itemPtr)
{
   VMK_ASSERT(itemPtr != NULL && itemPtr != itemPtr->nextPtr);
   VMK_ASSERT(itemPtr->prevPtr->nextPtr == itemPtr &&
              itemPtr->nextPtr->prevPtr == itemPtr);
   itemPtr->prevPtr->nextPtr = itemPtr->nextPtr;
   itemPtr->nextPtr->prevPtr = itemPtr->prevPtr;
   vmk_ListInitElement(itemPtr);
}


/*
 ***********************************************************************
 * vmk_ListSplitBefore --                                         */ /**
 *
 * \brief Remove items from the head of a list up to and including
 *        a given list member and put them at the head of another list.
 *
 * \param[in] headerPtr    The source list from which the members will
 *                         be moved.
 * \param[in] headerPtr2   The destination list to which the members
 *                         will be moved.
 * \param[in] itemPtr      The list member in the source list that is
 *                         the last element to be moved.
 *
 ***********************************************************************
 */
static inline void
vmk_ListSplitBefore(vmk_ListLinks *headerPtr, vmk_ListLinks *headerPtr2,
                    vmk_ListLinks *itemPtr)
{
   VMK_ASSERT(headerPtr);
   VMK_ASSERT(headerPtr2);
   VMK_ASSERT(headerPtr != headerPtr2);
   VMK_ASSERT(vmk_ListIsEmpty(headerPtr2));

   /* set up new list */
   headerPtr2->nextPtr = headerPtr->nextPtr;
   headerPtr2->prevPtr = itemPtr;

   /* fix old list */
   headerPtr->nextPtr = itemPtr->nextPtr;
   itemPtr->nextPtr->prevPtr = headerPtr;

   /* fix rest of new list entries */
   itemPtr->nextPtr = headerPtr2;
   headerPtr2->nextPtr->prevPtr = headerPtr2;
}


/*
 ***********************************************************************
 * vmk_ListSplitAfter --                                           */ /**
 *
 * \brief Remove items from the tail of a list up to and including
 *        a given list member and puts them at the tail of another list.
 *
 * \param[in] headerPtr       The non-empty source list from which the
 *                            members will be moved.
 * \param[in] headerPtr2      The empty destination list to which the
 *                            members will be moved.
 * \param[in] itemPtr         The list member in the source list that is
 *                            the first element to be moved.
 *
 ***********************************************************************
 */
static inline void
vmk_ListSplitAfter(vmk_ListLinks *headerPtr, vmk_ListLinks *headerPtr2,
                   vmk_ListLinks *itemPtr)
{
   VMK_ASSERT(headerPtr);
   VMK_ASSERT(headerPtr2);
   VMK_ASSERT(headerPtr != headerPtr2);
   VMK_ASSERT(!vmk_ListIsEmpty(headerPtr));
   VMK_ASSERT(vmk_ListIsEmpty(headerPtr2));

   /* set up new list */
   headerPtr2->prevPtr = headerPtr->prevPtr;
   headerPtr2->nextPtr = itemPtr;

   /* fix old list */
   headerPtr->prevPtr = itemPtr->prevPtr;
   itemPtr->prevPtr->nextPtr = headerPtr;

   /* fix rest of new list entries */
   itemPtr->prevPtr = headerPtr2;
   headerPtr2->prevPtr->nextPtr = headerPtr2;
}

#endif /* _VMKAPI_LIST_H_ */
/** @} */
/** @} */
