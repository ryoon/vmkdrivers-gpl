/* **********************************************************
 * Copyright 2008 - 2009 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * SList                                                          */ /**
 * \addtogroup Lib
 * @{
 * \defgroup SList Singly-linked List Management
 *
 * Singly-linked lists.
 * 
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_SLIST_H_
#define _VMKAPI_SLIST_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/**
 * \brief Singly-linked list link.
 *
 * This link can be embedded within other data structures to allow them
 * to be added to a list.
 */
typedef struct vmk_SList_Links {
   struct vmk_SList_Links *next;
} vmk_SList_Links;

/**
 * \brief A singly-linked list.
 *
 * This structure represents the entire list.
 */
typedef struct vmk_SList {
   vmk_SList_Links *head;
   vmk_SList_Links *tail;
} vmk_SList;

/*
 ***********************************************************************
 * VMK_SLIST_ENTRY --                                             */ /**
 *
 * \brief Get a pointer to the structure containing a given list element.
 *
 * \param itemPtr  List element that is contained by another structure
 * \param STRUCT   C type of the container
 * \param MEMBER   Name of the structure field in the container
 *                 that itemPtr is pointing to.
 *
 ***********************************************************************
 */

#define VMK_SLIST_ENTRY(itemPtr, STRUCT, MEMBER) \
  ((STRUCT *) ((vmk_uint8 *) (itemPtr) - vmk_offsetof (STRUCT, MEMBER)))

/*
 ***********************************************************************
 * VMK_SLIST_FORALL --                                            */ /**
 *
 * \brief for-loop replacement macro to scan through a list from
 *        the first to the last list member
 *
 * \note Expressions that contain side effects aren't valid as 
 *       parameters (example: removal).
 *
 * \param list     The list to scan
 * \param current  Loop pointer that is updated with the current
 *                 list member each time through the loop
 *
 ***********************************************************************
 */

#define VMK_SLIST_FORALL(list, current) \
        for ((current) = (list)->head; \
             (current) != NULL; \
             (current) = (current)->next)

/*
 ***********************************************************************
 * VMK_SLIST_FORALL_SAFE --                                       */ /**
 *
 * \brief for-loop replacement macro to scan through a list from
 *        the first to the last list member
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
 * \param list     The list to scan
 * \param prevPtr  Loop pointer that is updated each time through the
 *                 loop with the previous list member
 * \param current  Loop pointer that is updated each time through the
 *                 loop with the current list member 
 * \param nextPtr  Loop pointer that is updated each time through the
 *                 loop with the next list member
 *
 ***********************************************************************
 */

#define VMK_SLIST_FORALL_SAFE(list, prevPtr, current, nextPtr) \
        for ((prevPtr) = NULL, (current) = (list)->head, \
               (nextPtr) = (current) ? (current)->next : NULL; \
               (current) != NULL; \
               (prevPtr) = (!(prevPtr) && ((list)->head != (current)))? NULL : \
               ((prevPtr) && ((prevPtr)->next == (nextPtr)) ? (prevPtr) : (current)), \
                (current) = (nextPtr), (nextPtr) = (current) ? (current)->next : NULL)

/*
 ***********************************************************************
 * VMK_SLIST_FORALL_AFTER --                                      */ /**
 *
 * \brief for-loop replacement macro to scan through a list from
 *        the current to the last list member
 *
 * \note Expressions that contain side effects aren't valid as 
 *       parameters (example: removal).
 *
 * \param list     The list to scan
 * \param current  Loop pointer that indicate the element where to start
 *                 and is updated with the current list member each time
 *                 through the loop
 *
 ***********************************************************************
 */

#define VMK_SLIST_FORALL_AFTER(list, current) \
        for (; \
             (current) != NULL; \
             (current) = (current)->next)

/*
 ***********************************************************************
 * VMK_SLIST_FORN --                                              */ /**
 *
 * \brief for-loop replacement macro to scan through a list from
 *        the first to the nth list member
 *
 * \note Expressions that contain side effects aren't valid as 
 *       parameters (example: removal).
 *
 * \note On exit, current points to the (n+1)th member or NULL if the
 *       list has a number of members lower than or equal to n.
 *
 * \param list     The list to scan
 * \param current  Loop pointer that is updated with the current
 *                 list member each time through the loop
 * \param n        Index of the element where to stop
 *
 ***********************************************************************
 */

#define VMK_SLIST_FORN(list, current, n) \
        for ((current) = (list)->head; \
             ((current) != NULL) && (n != 0); \
             (current) = (current)->next, n--)

/*
 ***********************************************************************
 * vmk_SListInitElement --                                        */ /**
 *
 * \brief Initialize a list link.
 *
 * \param[in]  element     Element to initialize
 *
 ***********************************************************************
 */
static inline void
vmk_SListInitElement(vmk_SList_Links *element)
{
   VMK_ASSERT(element);
   element->next = NULL;
}

/*
 ***********************************************************************
 * vmk_SListIsEmpty --                                            */ /**
 *
 * \brief Checks whether a list is empty or not.
 *
 * \param[in]  list        Target list
 *
 * \retval     VMK_TRUE    The list is empty
 * \retval     VMK_FALSE   The list is not empty
 *
 ***********************************************************************
 */
static inline vmk_Bool
vmk_SListIsEmpty(const vmk_SList *list)
{
   VMK_ASSERT(list);
   return list->head == NULL ? VMK_TRUE : VMK_FALSE;
}

/*
 ***********************************************************************
 * vmk_SListFirst --                                              */ /**
 *
 * \brief Returns the first element (head) of a list.
 *
 * \param[in]  list        Target list
 *
 * \retval     NULL        The list is empty
 * \return                 A pointer to the head of the list
 *
 ***********************************************************************
 */
static inline vmk_SList_Links *
vmk_SListFirst(const vmk_SList *list)
{
   VMK_ASSERT(list);
   return list->head;
}

/*
 ***********************************************************************
 * vmk_SListLast --                                               */ /**
 *
 * \brief Returns the last element (tail) of a list.
 *
 * \param[in]  list        Target list
 *
 * \retval     NULL        The list is empty
 * \return                 A pointer to the tail of the list
 *
 ***********************************************************************
 */
static inline vmk_SList_Links *
vmk_SListLast(const vmk_SList *list)
{
   VMK_ASSERT(list);
   return list->tail;
}

/*
 ***********************************************************************
 * vmk_SListNext --                                               */ /**
 *
 * \brief Returns the following link in a list.
 *
 * \param[in]  element     Target list
 *
 * \retval     NULL        We are at the end of the list
 * \return                 A pointer to the next element
 *
 ***********************************************************************
 */
static inline vmk_SList_Links *
vmk_SListNext(const vmk_SList_Links *element)
{
   VMK_ASSERT(element);
   return element->next;
}

/*
 ***********************************************************************
 * vmk_SListInit --                                               */ /**
 *
 * \brief Initializes a list to be an empty list.
 *
 * \param[in]  list        Target list
 *
 ***********************************************************************
 */
static inline void
vmk_SListInit(vmk_SList *list)
{
   VMK_ASSERT(list);
   list->head = NULL;
   list->tail = NULL;
}

/*
 ***********************************************************************
 * vmk_SListPrev --                                               */ /**
 *
 * \brief Returns the previous element in a list. Runs in O(n).
 *
 * \param[in]  list        Target list
 * \param[in]  element     Element whose previous element is asked for
 *
 * \retval     NULL        We are at the beginning of the list
 * \return                 A pointer to the previous element
 *
 ***********************************************************************
 */
static inline vmk_SList_Links *
vmk_SListPrev(const vmk_SList *list, const vmk_SList_Links *element)
{
   vmk_SList_Links *cur;
   VMK_ASSERT(list);
   VMK_ASSERT(element);

   if (element == list->head) {
      return NULL;
   }

   VMK_SLIST_FORALL(list, cur) {
      if (cur->next == element) {
         return cur;
      }
   }

   VMK_ASSERT(0); /* Element not on the list. */

   return NULL;
}

/*
 ***********************************************************************
 * vmk_SListPop --                                                */ /**
 *
 * \brief Returns the first element (head) of a list and remove it
 * from the list.
 *
 * \param[in]  list        Target list
 *
 * \return                 A pointer to the head of the list
 *
 ***********************************************************************
 */
static inline vmk_SList_Links *
vmk_SListPop(vmk_SList *list)
{
   vmk_SList_Links *oldhead;
   VMK_ASSERT(list);

   oldhead = list->head;
   VMK_ASSERT(oldhead);

   list->head = oldhead->next;

   if (list->head == NULL) {
      list->tail = NULL;
   }

   oldhead->next = NULL;

   return oldhead;
}

/*
 ***********************************************************************
 * vmk_SListInsertAtHead --                                       */ /**
 *
 * \brief Inserts a given element at the beginning of the list.
 *
 * \param[in]  list        Target list
 * \param[in]  element     Element to insert
 *
 ***********************************************************************
 */
static inline void
vmk_SListInsertAtHead(vmk_SList *list, vmk_SList_Links *element)
{
   VMK_ASSERT(list);
   VMK_ASSERT(element);

   element->next = list->head;

   if (list->tail == NULL) {
      VMK_ASSERT(list->head == NULL);
      list->tail = element;
   }

   list->head = element;
}

/*
 ***********************************************************************
 * vmk_SListInsertAtTail --                                       */ /**
 *
 * \brief Inserts a given element at the end of the list.
 *
 * \param[in]  list        Target list
 * \param[in]  element     Element to insert
 *
 ***********************************************************************
 */
static inline void
vmk_SListInsertAtTail(vmk_SList *list, vmk_SList_Links *element)
{
   VMK_ASSERT(list);
   VMK_ASSERT(element);

   if (list->tail == NULL) {
      list->head = element;
      list->tail = element;
   } else {
      list->tail->next = element;
      list->tail = element;
   }

   element->next = NULL;
}

/*
 ***********************************************************************
 * vmk_SListInsertAfter --                                        */ /**
 *
 * \brief Inserts an element after a given element.
 *
 * \param[in]  list        Target list
 * \param[in]  element     Element to insert
 * \param[in]  other       Element to insert the new element after
 *
 ***********************************************************************
 */
static inline void
vmk_SListInsertAfter(vmk_SList *list, vmk_SList_Links *element, vmk_SList_Links *other)
{
   VMK_ASSERT(list);
   VMK_ASSERT(element);
   VMK_ASSERT(other);
   VMK_ASSERT(list->head != NULL);

   element->next = other->next;
   other->next = element;

   if (list->tail == other) {
      list->tail = element;
   }
}

/*
 ***********************************************************************
 * vmk_SListRemove --                                             */ /**
 *
 * \brief Removes a given element from the list knowing its predecessor
 *
 * \param[in]  list        Target list
 * \param[in]  element     Element to remove
 * \param[in]  prev        Element preceding the element to remove
 *
 ***********************************************************************
 */
static inline void
vmk_SListRemove(vmk_SList *list, vmk_SList_Links *element,
                vmk_SList_Links *prev)
{
   VMK_ASSERT(list);
   VMK_ASSERT(element);

   if (prev) {
      VMK_ASSERT(prev->next == element);
      prev->next = element->next;
      if (list->tail == element) {
         list->tail = prev;
      }
   } else {
      VMK_ASSERT(list->head == element);
      list->head = element->next;
      if (list->tail == element) {
         list->tail = list->head;
      }
   }

#if defined(VMX86_DEBUG)
   /* don't reinitialize the removed element in release builds */
   vmk_SListInitElement(element);
#endif
}

/*
 ***********************************************************************
 * vmk_SListRemoveSlow --                                         */ /**
 *
 * \brief Removes a given element from the list. Runs O(n).
 *
 * \param[in]  list        Target list
 * \param[in]  element     Element to remove
 *
 ***********************************************************************
 */
static inline void
vmk_SListRemoveSlow(vmk_SList *list, vmk_SList_Links *element)
{
   vmk_SList_Links *prev = vmk_SListPrev(list, element);
   vmk_SListRemove(list, element, prev);
}

/*
 ***********************************************************************
 * vmk_SListAppend --                                             */ /**
 *
 * \brief Appends all elements of a list at the end of another.
 *
 * \note Items appended to listDest are removed from listSrc.
 *
 * \param[in]  listDest    Target list
 * \param[in]  listSrc     List to append
 *
 ***********************************************************************
 */
static inline void
vmk_SListAppend(vmk_SList *listDest,
                vmk_SList *listSrc)
{
   VMK_ASSERT(listDest);
   VMK_ASSERT(listSrc);

   if (!listSrc->head) {
      /* Source list empty, nothing to append. */
      return;
   }

   if (!listDest->head) {
      listDest->head = listSrc->head;
      listDest->tail = listSrc->tail;
   } else {
      listDest->tail->next = listSrc->head;
      listDest->tail = listSrc->tail;
   }

   vmk_SListInit(listSrc);
}

/*
 ***********************************************************************
 * vmk_SListAppendN --                                            */ /**
 *
 * \brief Appends up to num elements of a list at the end of another.
 *
 * \note Items appended to listDest are removed from listSrc.
 *
 * \param[in]  listDest    Target list
 * \param[in]  listSrc     List to append
 * \param[in]  num         Number of elements to append
 *
 ***********************************************************************
 */

static inline void
vmk_SListAppendN(vmk_SList *listDest,
                 vmk_SList *listSrc,
                 vmk_uint32 num)
{
   vmk_SList_Links *appendListHead;
   vmk_SList_Links *appendListTail;
   vmk_uint32 i;

   VMK_ASSERT(listDest);
   VMK_ASSERT(listSrc);

   if (num == 0) {
      return;
   }

   /* Find the last element to be transferred to the destination. */
   appendListHead = appendListTail = listSrc->head;
   if (appendListHead == NULL) {
      /* This list has no elements. */
      return;
   }

   for (i = 0; i < num - 1; i++) {
      appendListTail = appendListTail->next;
      if (appendListTail == listSrc->tail) {
         /* Source list has fewer than num elements: append them all. */
         break;
      }
      VMK_ASSERT(appendListTail);
   }

   /* Fix the source list. */
   if (appendListTail == listSrc->tail) {
      vmk_SListInit(listSrc);
   } else {
      listSrc->head = appendListTail->next;
   }

   /* Fix the destination list. */
   if (listDest->tail != NULL) {
      listDest->tail->next = appendListHead;
   } else {
      listDest->head = appendListHead;
   }
   listDest->tail = appendListTail;
   appendListTail->next = NULL;
}


/*
 ***********************************************************************
 * vmk_SListPrepend --                                            */ /**
 *
 * \brief Insert all elements of a list at the beginning of another.
 *
 * \note Items prepended to listDest are removed from listSrc.
 *
 * \param[in]  listDest    Target list
 * \param[in]  listSrc     List to prepend
 *
 ***********************************************************************
 */
static inline void
vmk_SListPrepend(vmk_SList *listDest,
                 vmk_SList *listSrc)
{
   VMK_ASSERT(listDest);
   VMK_ASSERT(listSrc);

   if (!listSrc->head) {
      /* Source list empty, nothing to prepend. */
      return;
   }

   if (!listDest->head) {
      listDest->head = listSrc->head;
      listDest->tail = listSrc->tail;
   } else {
      listSrc->tail->next = listDest->head;
      listDest->head = listSrc->head;
   }

   vmk_SListInit(listSrc);
}

/*
 ***********************************************************************
 * vmk_SListSplitHead --                                          */ /**
 *
 * \brief Split a list into two list at a given entry.
 *
 * \note The second list must be empty.
 *
 * \param[in]  list1       Target list, becomes left part of the list
 * \param[in]  list2       Right part of the list
 * \param[in]  element     Element where to split, this element is moved
 *                         into list2
 *
 ***********************************************************************
 */
static inline void
vmk_SListSplitHead(vmk_SList *list1,
                   vmk_SList *list2,
                   vmk_SList_Links *element)
{
   VMK_ASSERT(list1);
   VMK_ASSERT(list2);
   VMK_ASSERT(vmk_SListIsEmpty(list2));
   VMK_ASSERT(element);

   list2->head = list1->head;
   list2->tail = element;

   list1->head = element->next;
   if (list1->head == NULL) {
      list1->tail = NULL;
   }

   element->next = NULL;
}

/*
 ***********************************************************************
 * vmk_SListSplitNHead --                                         */ /**
 *
 * \brief Split a list into two list starting a given element.
 *
 * \note The second list must be empty.
 *
 * \param[in]  list1       Target list, becomes left part of the list
 * \param[in]  list2       Right part of the list
 * \param[in]  n           Index of the element where to start splitting
 *
 ***********************************************************************
 */
static inline void
vmk_SListSplitNHead(vmk_SList *list1,
                    vmk_SList *list2,
                    vmk_uint64 n)
{
   vmk_SList_Links *cur;

   VMK_SLIST_FORN(list1, cur, n);
   if (cur == NULL) {
      vmk_SListAppend(list2, list1);
   } else {
      vmk_SListSplitHead(list1, list2, cur);
   }
}

/*
 ***********************************************************************
 * vmk_SListReplace --                                            */ /**
 *
 * \brief Replace the given entry with a new entry. Runs O(1).
 *
 * \param[in] list     List destination
 * \param[in] targetEntry Entry to replace
 * \param[in] newEntry    New entry
 * \param[in] prevEntry   Predecessor of the entry to replace
 *
 ***********************************************************************
 */
static inline void
vmk_SListReplace(vmk_SList *list, 
                 vmk_SList_Links *targetEntry,
                 vmk_SList_Links *newEntry, 
                 vmk_SList_Links *prevEntry)
{
   VMK_ASSERT(list);
   VMK_ASSERT(targetEntry);

   if (!prevEntry) {
      VMK_ASSERT(list->head == targetEntry);
      list->head = newEntry;
   } else {
      VMK_ASSERT(prevEntry->next == targetEntry);
      prevEntry->next = newEntry;
   }

   if (list->tail == targetEntry) {
      list->tail = newEntry;
   }

   newEntry->next = targetEntry->next;
   targetEntry->next = NULL;
}

#endif /* _VMKAPI_SLIST_H_ */
/** @} */
/** @} */
