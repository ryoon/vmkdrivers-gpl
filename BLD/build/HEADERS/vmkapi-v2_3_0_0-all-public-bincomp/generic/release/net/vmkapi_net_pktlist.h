/* **********************************************************
 * Copyright 2006 - 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * PktList                                                        */ /**
 * \addtogroup Network
 *@{
 * \defgroup PktList Packet List Management
 *@{
 *
 * \par Packet Lists:
 *
 * Packet list are an important entity in vmkernel as any set of packets
 * are represented through this data structure.
 * Every module will need to deal with it as vmkernel expects it to
 * be able to.
 *
 * For example if a module is intended to manage device driver and want
 * vmkernel to use it in order to communicate with the external world,
 * it will receive packet lists for Tx process.
 *
 ***********************************************************************
 */

#ifndef _VMKAPI_NET_PKTLIST_H_
#define _VMKAPI_NET_PKTLIST_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "net/vmkapi_net_pkt.h"

/**
 * \ingroup PktList
 * \brief Packet list representation.
 */
typedef struct PktList *vmk_PktList;

/**
 * \ingroup PktList
 * \brief Behavior on vmk_PktListClone failure.
 */ 

typedef enum {
   VMK_PKTLIST_RELEASE_ON_FAIL, 
   VMK_PKTLIST_KEEP_ON_FAIL, 
} vmk_PktListCloneFailAction;

/**
 * \ingroup PktList
 * \brief Size of the vmk_PktList structure.
 * \note This value can be different between beta and release builds.
 */
extern const vmk_ByteCountSmall vmk_PktListSizeInBytes;

/**
 * \ingroup PktList
 * \brief Macro for defining a packet list on the stack.
 */
#define VMK_PKTLIST_STACK_DEF(listName)                            \
   char vmkPktList_##listName[vmk_PktListSizeInBytes];             \
   vmk_PktList listName = (vmk_PktList) &vmkPktList_##listName[0];

/**
 * \ingroup PktList
 * \brief Macro for defining a packet list on the stack and initializing it.
 */
#define VMK_PKTLIST_STACK_DEF_INIT(listName)                               \
           char vmkPktList_##listName[vmk_PktListSizeInBytes];             \
           vmk_PktList listName = (vmk_PktList) &vmkPktList_##listName[0]; \
           vmk_PktListInit(listName);

/**
 * \ingroup PktList
 * \brief Packet list iterator
 *
 * Iterators are used to browse packet lists and perform some operations such
 * as adding or removing packets.
 *
 * \note An iterator might be invalidated after a call to any PktList function
 * not using them.
 *
 * \note Only one iterator can be used on a PktList at any time. Furthermore,
 * iterators don't provide any serialization guarantee, it is up to the caller
 * to ensure that a vmk_PktList / vmk_PktListIter is being accessed by a single
 * thread at a time.
 */
typedef struct PktListIter *vmk_PktListIter;

/**
 * \ingroup PktList
 * \brief Macro for defining a packet iterator on the stack
 */
#define VMK_PKTLIST_ITER_STACK_DEF(iterName)                                  \
   char vmkPktListIter_##iterName[vmk_PktListIterSizeOf()];                   \
   vmk_PktListIter iterName = (vmk_PktListIter) &vmkPktListIter_##iterName[0];


/*
 ***********************************************************************
 * vmk_PktListInit --                                             */ /**
 *
 * \ingroup PktList
 * \brief Initialize a packet list.
 *
 * \param[in]  pktList Target packet list
 *
 ***********************************************************************
 */

void vmk_PktListInit(vmk_PktList pktList);

/*
 ***********************************************************************
 * vmk_PktListAlloc --                                            */ /**
 *
 * \ingroup PktList
 * \brief Allocate a new packet list from the heap.
 *
 * \note The new list is not initialized.
 *
 * \param[out]  list               Pointer to hold the allocated list.
 *
 * \retval      VMK_NO_MEMORY      The list could not be allocated.
 * \retval      VMK_OK             The list was successfully allocated.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktListAlloc(vmk_PktList *list);


/*
 ***********************************************************************
 * vmk_PktListFree --                                             */ /**
 *
 * \ingroup PktList
 * \brief Free a packet list.
 *
 * \note The list should have previously been allocated with
 * vmk_PktListAlloc.
 *
 * \param[in]   list               Packet list to free.
 *
 * \retval      VMK_OK             The list was successfully freed.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktListFree(vmk_PktList list);

/*
 ***********************************************************************
 * vmk_PktListGetCount --                                         */ /**
 *
 * \ingroup PktList
 * \brief Retrieve the number of packets in a packet list.
 *
 * \param[in]   list               Target packet list.
 *
 * \return                         Number of packets in the list.
 *
 ***********************************************************************
 */

vmk_uint32 vmk_PktListGetCount(vmk_PktList list);

/*
 ***********************************************************************
 * vmk_PktListIsEmpty --                                          */ /**
 *
 * \ingroup PktList
 * \brief Check whether a given packet list is empty.
 *
 * \param[in]   list               Target packet list.
 *
 * \retval      VMK_TRUE           List empty.
 * \retval      VMK_FALSE          List not empty.
 *
 ***********************************************************************
 */

static inline vmk_Bool vmk_PktListIsEmpty(vmk_PktList list)
{
   return vmk_PktListGetCount(list) == 0 ? VMK_TRUE : VMK_FALSE;
}

/*
 ***********************************************************************
 * vmk_PktListAppendPkt --                                        */ /**
 *
 * \ingroup PktList
 * \brief Add a packet at the end (tail) of a packet list.
 *
 * \param[in]   list               Target packet list.
 * \param[in]   pkt                Packet to be added.
 *
 ***********************************************************************
 */

void vmk_PktListAppendPkt(vmk_PktList list,
                          vmk_PktHandle *pkt);

/*
 ***********************************************************************
 * vmk_PktListPrependPkt --                                        */ /**
 *
 * \ingroup PktList
 * \brief Add a packet at the front (head) of a packet list.
 *
 * \param[in]  list                Target packet list.
 * \param[in]  pkt                 Packet to be added.
 *
 ***********************************************************************
 */

void vmk_PktListPrependPkt(vmk_PktList list,
                           vmk_PktHandle *pkt);

/*
 ***********************************************************************
 * vmk_PktListGetFirstPkt --                                      */ /**
 *
 * \ingroup PktList
 * \brief Return the first packet (head) of a packet list.
 *
 * \param[in]  list                Target packet list.
 *
 * \retval     NULL                The list is empty.
 * \return                         The first packet of the list.
 *
 ***********************************************************************
 */

vmk_PktHandle    *vmk_PktListGetFirstPkt(vmk_PktList list);

/*
 ***********************************************************************
 * vmk_PktListPopFirstPkt --                                      */ /**
 *
 * \ingroup PktList
 * \brief Return the first packet (head) of a packet list and remove it
 * from the list.
 *
 * \param[in]  list                Target list.
 *
 * \retval     NULL                The list is empty.
 * \return                         The former head of the list.
 *
 ***********************************************************************
 */

vmk_PktHandle    *vmk_PktListPopFirstPkt(vmk_PktList list);

/*
 ***********************************************************************
 * vmk_PktListGetLastPkt --                                       */ /**
 *
 * \ingroup PktList
 * \brief Return the last packet (tail) of a packet list.
 *
 * \param[in]  list                Target list.
 *
 * \retval     NULL                The list is empty.
 * \return                         The last packet of the list.
 *
 ***********************************************************************
 */

vmk_PktHandle    *vmk_PktListGetLastPkt(vmk_PktList list);

/*
 ***********************************************************************
 * vmk_PktListPopLastPkt --                                       */ /**
 *
 * \ingroup PktList
 * \brief Return the last packet (tail) of a packet list and remove it
 * from the list.
 *
 * \param[in]  list                Target list.
 *
 * \retval     NULL                The list is empty.
 * \return                         The former tail of the list.
 *
 ***********************************************************************
 */

vmk_PktHandle    *vmk_PktListPopLastPkt(vmk_PktList list);

/*
 ***********************************************************************
 * vmk_PktListIsConsistent --                                     */ /**
 *
 * \ingroup PktList
 * \brief Check whether the packet list is consistent.
 *
 * \param[in]  list                Target list.
 *
 * \retval     vmk_Bool            TRUE if the list is consistent,
 *                                 FALSE otherwise.
 *
 ***********************************************************************
 */

vmk_Bool         vmk_PktListIsConsistent(vmk_PktList list);

/*
 ***********************************************************************
 * vmk_PktListIterSizeOf --                                       */ /**
 *
 * \ingroup PktList
 * \brief Return the size of a packet list iterator.
 *
 * \return                         The size of a packet iterator.
 *
 ***********************************************************************
 */

vmk_ByteCount    vmk_PktListIterSizeOf(void);

/*
 ***********************************************************************
 * vmk_PktListIterAlloc --                                        */ /**
 *
 * \ingroup PktList
 * \brief Allocate a new packet list iterator from the heap.
 *
 * \param[out] iter                Pointer to hold the allocated
 *                                 iterator.
 *
 * \retval     VMK_NO_MEMORY       The iterator could not be allocated.
 * \retval     VMK_OK              The iterator was successfully
 *                                 allocated.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktListIterAlloc(vmk_PktListIter *iter);

/*
 ***********************************************************************
 * vmk_PktListIterFree --                                         */ /**
 *
 * \ingroup PktList
 * \brief Free a packet list iterator.
 *
 * \note The list should have previously been allocated with
 * vmk_PktListAlloc.
 *
 * \param[in]  iter                Iterator to free.
 *
 ***********************************************************************
 */

void             vmk_PktListIterFree(vmk_PktListIter iter);

/*
 ***********************************************************************
 * vmk_PktListIterStart --                                        */ /**
 *
 * \ingroup PktList
 * \brief Set an iterator at the beginning of a packet list.
 *
 * \param[in]  iter                Packet iterator.
 * \param[in]  list                Target packet list.
 *
 ***********************************************************************
 */

void vmk_PktListIterStart(vmk_PktListIter iter,
                          vmk_PktList list);


/*
 ***********************************************************************
 * vmk_PktListIterMove --                                         */ /**
 *
 * \ingroup PktList
 * \brief Move a packet list iterator to the next packet in the list.
 *
 * \param[in]  iter                Packet iterator.
 *
 * \retval     VMK_LIMIT_EXCEEDED  The iterator does not point to any
 *                                 packet or is at the end of the list.
 * \retval     VMK_OK              Iterator successfully moved.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktListIterMove(vmk_PktListIter iter);


/*
 ***********************************************************************
 * vmk_PktListIterIsAtEnd --                                      */ /**
 *
 * \ingroup PktList
 * \brief Check whether a packet list iterator has reached the end of
 * a packet list.
 *
 * \param[in]  iter                Packet iterator.
 *
 * \retval     vmk_Bool            TRUE if it is at the end of the list,
 *                                 FALSE otherwise
 *
 ***********************************************************************
 */

vmk_Bool         vmk_PktListIterIsAtEnd(vmk_PktListIter iter);


/*
 ***********************************************************************
 * vmk_PktListIterGetPkt --                                       */ /**
 *
 * \ingroup PktList
 * \brief Retrieve the packet pointed by a packet list iterator.
 *
 * \param[in]  iter          Packet iterator.
 *
 * \retval     NULL          The iterator is not pointing to any packet.
 * \return                   The retrieved packet.
 *
 ***********************************************************************
 */

vmk_PktHandle   *vmk_PktListIterGetPkt(vmk_PktListIter iter);


/*
 ***********************************************************************
 * vmk_PktListIterRemovePkt --                                    */ /**
 *
 * \ingroup PktList
 * \brief Retrieve the packet pointed by an iterator and remove it from
 * the packet list.
 *
 * \note The iterator is moved to the next packet.
 *
 * \param[in]  iter          Packet iterator.
 * \param[out] pkt           Address of pointer to update with a pointer
 *                           to the removed pkt. Can be NULL, in which
 *                           case it will be ignored.
 *
 * \retval     VMK_LIMIT_EXCEEDED The iterator does not point to any
 *                                packet or is at the end of the list.
 * \retval     VMK_OK             If successful.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktListIterRemovePkt(vmk_PktListIter iter,
                                          vmk_PktHandle **pkt);

/*
 ***********************************************************************
 * vmk_PktListIterReplace --                                      */ /**
 *
 * \ingroup PktList
 * \brief Replace the packet pointed by an iterator and release it.
 *
 * \note The iterator is moved to the new packet.
 *
 * \param[in]  iter                Packet iterator.
 * \param[in]  pkt                 Packet to be inserted.
 *
 * \retval     VMK_LIMIT_EXCEEDED  The iterator does not point to any
 *                                 packet or is at the end of the list.
 * \retval     VMK_OK              The packet was successfully replaced.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktListIterReplace(vmk_PktListIter iter,
                                        vmk_PktHandle *pkt);

/*
 ***********************************************************************
 * vmk_PktListIterInsertPktAfter --                               */ /**
 *
 * \ingroup PktList
 * \brief Insert a packet after the iterator.
 *
 * \note If the packet list was empty, the iterator is moved to the new
 * packet.
 *
 * \param[in]  iter                Packet iterator.
 * \param[in]  pkt                 Packet to be inserted.
 *
 * \retval     VMK_LIMIT_EXCEEDED  The iterator is at the end of the
 *                                 list.
 * \retval     VMK_OK              The packet was successfully inserted.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktListIterInsertPktAfter(vmk_PktListIter iter,
                                               vmk_PktHandle *pkt);

/*
 ***********************************************************************
 * vmk_PktListIterInsertPktBefore --                              */ /**
 *
 * \ingroup PktList
 * \brief Insert a packet before the iterator.
 *
 * \note If the list was empty, the iterator is moved at the new end.
 *
 * \param[in]  iter                Packet iterator.
 * \param[in]  pkt                 Packet to be inserted.
 *
 * \retval     VMK_OK              Always succeeds.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktListIterInsertPktBefore(vmk_PktListIter iter,
                                                vmk_PktHandle *pkt);


/*
 ***********************************************************************
 * vmk_PktListIterInsertListAfter --                              */ /**
 *
 * \ingroup PktList
 * \brief Insert a packet list after the iterator.

 * \note If the original packet list was empty, the iterator is moved to
 * the first element of the inserted list.
 *
 * \note The inserted list is reinitialized.
 *
 * \param[in]  iter                Packet iterator.
 * \param[in]  list                Packet list to be inserted.
 *
 * \retval     VMK_LIMIT_EXCEEDED  The iterator is at the end of the
 *                                 list.
 * \retval     VMK_OK              The packet was successfully inserted.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktListIterInsertListAfter(vmk_PktListIter iter,
                                                vmk_PktList list);


/*
 ***********************************************************************
 * vmk_PktListIterInsertListBefore --                             */ /**
 *
 * \ingroup PktList
 * \brief Insert a packet list before the iterator.

 * \note If the packet list was empty, the iterator is now at the end of
 * the new packet list.

 * \note The inserted list is reinitialized.
 *
 * \param[in]  iter                Packet iterator.
 * \param[in]  list                Packet list to be inserted.
 *
 * \retval     VMK_OK              Always succeeds.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktListIterInsertListBefore(vmk_PktListIter iter,
                                                 vmk_PktList list);


/*
 ***********************************************************************
 * vmk_PktListIterSplitListAfter --                               */ /**
 *
 * \ingroup PktList
 * \brief Split the current list after iterator.
 *
 * All the packets after the iterator will be moved to a new list.
 *
 * \note The iterator is set at the end of the original list.
 *
 * \note The split list will be clobbered.
 *
 * \param[in]  iter                Packet iterator.
 * \param[out] splitList           Pointer to hold the new list.
 *
 * \retval     VMK_LIMIT_EXCEEDED  The iterator does not point to any
 *                                 packet or is at the end of the list.
 * \retval     VMK_OK              List successfully split.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktListIterSplitListAfter(vmk_PktListIter iter,
                                               vmk_PktList *splitList);


/*
 ***********************************************************************
 * vmk_PktListIterSplitListBefore --                              */ /**
 *
 * \ingroup PktList
 * \brief Split the current list before iterator.
 *
 * All the packets before the iterator will be moved to a new list.
 *
 * \note The iterator is set at the beginning of the original list.
 *
 * \note The split list will be clobbered.
 *
 * \param[in]  iter                Packet iterator.
 * \param[out] splitList           Pointer to hold the new list.
 *
 * \retval     VMK_OK              Always succeeds.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktListIterSplitListBefore(vmk_PktListIter iter,
                                                vmk_PktList *splitList);


/*
 ***********************************************************************
 * vmk_PktListClone --                                           */ /**
 *
 * \ingroup PktList
 * \brief Create a list containing clones of all the packets from another
 *        list.
 *
 * \note In case of any failure before all the clone operations are
 *       completed, failAction will be consulted. If
 *       VMK_PKTLIST_RELEASE_ON_FAIL is specified, then all packets that
 *       were cloned up to that point will be released and an empty
 *       dstList will be returned. If VMK_PKTLIST_KEEP_ON_FAIL was
 *       specified, then dstList will contain all the clones that were
 *       successfully created until the failure.
 *
 * \pre  dstList must be an empty list.
 *
 * \param[in]  dstList           Destination list for cloned packets.
 * \param[in]  srcList           Source list to clone packets from.
 * \param[in]  failAction        Action to take on cloning failure.
 *
 * \retval     VMK_OK            dstList contains clones of all packets
 *                               from srcList
 *             VMK_BAD_PARAM     dstList was not an empty list
 *             VMK_NO_RESOURCES  Could not allocate memory to clone all
 *                               packets
 *             VMK_FAILURE       Miscellaneous clone failure
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktListClone(vmk_PktList dstList,
                                  vmk_PktList srcList,
                                  vmk_PktListCloneFailAction failAction);


/*
 ***********************************************************************
 * vmk_PktListAppend --                                           */ /**
 *
 * \ingroup PktList
 * \brief Append a packet list behind another list.
 *
 * \note srcList is appended to the end of dstList.
 *
 * \note srcList is emptied and reinitialized.
 *
 * \param[in]  dstList     Target list to append to
 * \param[in]  srcList     List to append from
 *
 ***********************************************************************
 */

void vmk_PktListAppend(vmk_PktList dstList,
                       vmk_PktList srcList);


/*
 ***********************************************************************
 * vmk_PktListPrepend --                                          */ /**
 *
 * \ingroup PktList
 * \brief Prepend a packet list in front of another list.
 *
 * \note srcList is prepended to the beginning of dstList.
 *
 * \note srcList is emptied and reinitialized.
 *
 * \param[in]  dstList     Target list to prepend to
 * \param[in]  srcList     List to prepend from
 *
 ***********************************************************************
 */

void vmk_PktListPrepend(vmk_PktList dstList,
                        vmk_PktList srcList);

/*
 ***********************************************************************
 * vmk_PktListReleaseIRQ --                                       */ /**
 *
 * \ingroup PktList
 * \brief Release all resources of a given vmk_PktList from an
 *        interrupt context.
 *
 * \param[in]  pktList    Packet List to be released.
 *
 * \retval     None
 *
 ***********************************************************************
 */

void vmk_PktListReleaseIRQ(vmk_PktList pktList);


/*
 ***********************************************************************
 * vmk_PktListReleaseAllPkts --                                   */ /**
 *
 * \ingroup PktList
 * \brief Release all the packets and empty the list. This function should
 *        not be used from a panic context.Use vmk_PktReleasePanic instead.
 *
 * \param[in]  list     Target list
 *
 ***********************************************************************
 */

void vmk_PktListReleaseAllPkts(vmk_PktList list);


/*
 ***********************************************************************
 * vmk_PktTcpSegmentation --                                      */ /**
 *
 * \ingroup PktList
 * \brief Perform Software TSO.
 *
 * The TSO friendly TCP/IP stack will make sure the next IP frame won't
 * have a conflicting IP id #. ipId are incremented starting with the
 * ident of the original (non-segmented) packet.
 *
 * \note The list must be empty.
 *
 * \note It is the responsibility of the caller to ensure the entire
 *       ip/tcp header is contained within frameVA/frameMappedLen.
 *
 * \param[in]   pkt         Packet to segment
 * \param[out]  list        List of segment packets
 *
 * \retval      VMK_OK      If the segmentation was successful
 * \retval      VMK_FAILURE Otherwise
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktTcpSegmentation(vmk_PktHandle *pkt,
                                        vmk_PktList list);

/*
 ***********************************************************************
 * vmk_PktListSplitBySrcPortID --                                 */ /**
 *
 * \brief Split a packet list into two sub list by source port ID.
 *
 * Go through the packet list till two consecutive pkts have different
 * source portIDs and split the list at that point.
 *
 * \note Caller must initialize outList.
 *
 * \param[in]  pktList  The list to split and the remaining packets.
 * \param[out] outList  The packet list with consecutive source portIDs.
 * \param[out] portID   The corresponding port ID.
 *
 ***********************************************************************
 */

void vmk_PktListSplitBySrcPortID(vmk_PktList pktList,
                                 vmk_PktList outList,
                                 vmk_SwitchPortID *portID);

#endif /* _VMKAPI_NET_PKTLIST_H_ */
/** @} */
/** @} */
