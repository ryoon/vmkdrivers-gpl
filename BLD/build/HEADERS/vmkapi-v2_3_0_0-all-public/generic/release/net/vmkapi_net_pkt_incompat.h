/* **********************************************************
 * Copyright 2006 - 2012 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Pkt                                                            */ /**
 * \addtogroup Network
 *@{
 * \defgroup PktIncompat Packet Management (incompatible)
 *@{ 
 ***********************************************************************
 */

#ifndef _VMKAPI_NET_PKT_INCOMPAT_H_
#define _VMKAPI_NET_PKT_INCOMPAT_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "net/vmkapi_net_types.h"
#include "net/vmkapi_net_pkt.h"
#include "net/vmkapi_net_uplink.h"

/** Maximum size of the low-memory packet heap */
#define VMK_PKT_HEAP_MAX_SIZE    224

/** Maximum size of the high-memory packet heap */
#define VMK_HIGH_PKT_HEAP_MAX_SIZE   432

/** Pointer to context data for use by the packet completion handler */
typedef void *vmk_PktCompletionData;

/** 
 * \ingroup Pkt
 * \brief Event types for the packet tracing infrastructure
 */
typedef enum vmk_PktTraceEventType {
   /** Pkt received by driver */
   VMK_PKT_TRACE_PHY_DEV_RX        = 0x2,

   /** Pkt transmission started */
   VMK_PKT_TRACE_PHY_TX_START      = 0x3,

   /** Pkt transmission completed */
   VMK_PKT_TRACE_PHY_TX_DONE       = 0x4,
} vmk_PktTraceEventType;


/*
 ***********************************************************************
 * vmk_PktReleaseAfterComplete --                                 */ /**
 *
 * \ingroup Pkt
 * \brief Release a master vmk_PktHandle that has gone through 
 *        completion.
 *
 * This function is the fast path version of vmk_PktRelease() for calling 
 * at the end of a completion handler. The given packet MUST be a master, 
 * and the reference count to its resources MUST be 1 (no other 
 * references). The packet must either have a single fragment (allocated 
 * through vmk_PktAlloc()) or no buffer fragments.
 *
 * \pre   Packet must be a master with no other references to its 
 *        resources.
 *
 * \param[in] pkt    Packet to be released.
 * \retval           None
 *
 ***********************************************************************
 */

void vmk_PktReleaseAfterComplete(vmk_PktHandle *pkt);

/*
 ***********************************************************************
 * vmk_PktIsDescWritable --                                       */ /**
 *
 * \ingroup Pkt
 * \brief Returns whether the packet is a master with no other 
 *        references.
 *
 * The packet descriptor is considered "writable" if the given packet is 
 * a master (not generated through copy/clone API's) with no outstanding 
 * references left (all copies/clones are released). This function needs 
 * to be TRUE for modification of the packet descriptor via 
 * vmk_PktSetCompletionData(), vmk_PktClearCompletionData() or 
 * vmk_PktAllowSlowCompletion() to be allowed.
 *
 * \param[in]  pkt        Target packet.
 *
 * \retval     VMK_TRUE   Descriptor is writable.
 * \retval     VMK_FALSE  Descriptor is not writable.
 *
 ***********************************************************************
 */

vmk_Bool vmk_PktIsDescWritable(vmk_PktHandle *pkt);

/*
 ***********************************************************************
 * vmk_PktNeedCompletion -                                        */ /**
 *
 * \ingroup Pkt
 * \brief Returns whether the packet needs completion before being 
 *        released.
 *
 * This function will return VMK_TRUE if completion data has been set 
 * for the packet via vmk_PktSetCompletionData(). It indicates to 
 * vmkernel that the source port's notify chain needs to be executed (to 
 * run the registered completion handler) prior to releasing 
 * vmk_PktHandle resources.
 *
 * \param[in]  pkt                 Target packet.
 *
 * \retval     VMK_TRUE            Packet needs completion.
 * \retval     VMK_FALSE           Packet doesn't need completion.
 *
 ***********************************************************************
 */

vmk_Bool vmk_PktNeedCompletion(vmk_PktHandle *pkt);

/*
 ***********************************************************************
 * vmk_PktSetCompletionData --                                    */ /**
 *
 * \ingroup Pkt
 * \brief Set packet completion data for the given packet.
 *
 * Packet completion is a mechanism for releasing packets that have 
 * custom buffer arrangements or hold on to additional resources (such 
 * as descriptor entries in a vNIC ring) and as such a customized 
 * completion handler needs to be called prior to release of the 
 * vmk_PktHandle.
 *
 * See vmk_UplinkRegisterCompletionFn() for registering a completion 
 * handler. vmk_UplinkRegisterCompletionFn() is private API and should 
 * only be used in the driver layer. vmk_PktSetCompletionData(), 
 * vmk_PktGetCompletionData(), vmk_PktNeedCompletion() and 
 * vmk_PktAllowSlowCompletion() shouldn't be used unless a completion 
 * handler is registered.
 *
 * The ioData parameter is a pointer available for the implementation to
 * set and will be preserved with the vmk_PktHandle. It can be retrieved
 * at the completion handler to access completion context information
 * specific to this packet.
 *
 * See vmk_PktAllowSlowCompletion() for documentation of the slow 
 * completion flag.
 *
 * \note  As a side effect of calling this function the packet will be
 *        marked as needing completion (see vmk_PktNeedCompletion())
 *
 * \pre   vmk_PktIsDescWritable() must be TRUE for pkt.
 *
 * \param[in] pkt                 Target packet.
 * \param[in] ioData              Pointer to the completion context for 
 *                                the packet.
 * \param[in] allowSlowCompletion Packet can be held indefinitely before 
 *                                completion.
 *
 ***********************************************************************
 */

void vmk_PktSetCompletionData(vmk_PktHandle *pkt,
                              vmk_PktCompletionData ioData,
                              vmk_Bool allowSlowCompletion);

/*
 ***********************************************************************
 * vmk_PktGetCompletionData --                                    */ /**
 *
 * \ingroup Pkt
 * \brief Get embedded completion data for the given packet.
 *
 * \see   vmk_PktSetCompletionData()
 *
 * \param[in]  pkt     Target packet.
 * \param[out] ioData  Pointer to the completion context for the packet.
 *
 ***********************************************************************
 */

void vmk_PktGetCompletionData(vmk_PktHandle *pkt,
                              vmk_PktCompletionData *ioData);

/*
 ***********************************************************************
 * vmk_PktClearCompletionData -                                   */ /**
 *
 * \ingroup Pkt
 * \brief Clear the embedded completion data for the given packet.
 *
 * Clears the completion context pointer set via 
 * vmk_PktSetCompletionData() and marks the packet as not needing 
 * completion (see vmk_PktNeedCompletion()).
 *
 * \pre   vmk_PktIsDescWritable() must be TRUE for pkt.
 *
 * \param[in]  pkt    Target packet.
 *
 ***********************************************************************
 */

void vmk_PktClearCompletionData(vmk_PktHandle *pkt);

/*
 ***********************************************************************
 * vmk_PktAllowSlowCompletion -                                   */ /**
 *
 * \ingroup Pkt
 * \brief Return whether the packet can be held indefinitely without 
 *        completion or not.
 *
 * This flag is set via vmk_PktSetCompletionData().
 *
 * Packet receivers that can incur long delays are sensitive to the 
 * setting of this flag, and examine its value to determine what action 
 * to take. If the packet does NOT have allowSlowCompletion enabled, then 
 * such a receiver will need to make a deep copy of the frame, allowing 
 * the original frame to be completed earlier.
 *
 * An example packet receiver with long delays is the vmkernel TCP/IP 
 * stack. The long delays may occur due to fragment reassembly, packet 
 * reordering, or the zero-copy receive optimization.
 *
 * Another example is guest vNic's such as vlance that make use of a 
 * packet's fragments during their receive processing. The packet cannot 
 * be completed until the vNic indicates that it is done with the packet.
 *
 * \param[in]  pkt                 Target packet.
 *
 * \retval     VMK_TRUE            Packet can be held indefinitely 
 *                                 without completion.
 * \retval     VMK_FALSE           Packet can't be held indefinitely 
 *                                 without completion.
 *
 ***********************************************************************
 */

vmk_Bool vmk_PktAllowSlowCompletion(vmk_PktHandle *pkt);

/*
 ***********************************************************************
 * vmk_PktSetIsCorrupted --                                       */ /**
 *
 * \ingroup Pkt
 * \brief Mark a packet as having been corrupted.
 *
 * This function is intended for bookkeeping of stress option induced 
 * corruption in the frame contents of the packet. It shouldn't be used 
 * except for debugging purposes.
 *
 * \param[in]  pkt        Target packet.
 *
 ***********************************************************************
 */

void vmk_PktSetIsCorrupted(vmk_PktHandle *pkt);

/*
 ***********************************************************************
 * vmk_PktTraceRecordEvent --                                     */ /**
 *
 * \ingroup Pkt
 * \brief Record timestamp for the given pkthandle and event
 *
 * This function is intended for tracking the flow of a pktHandle across
 * various layers of the virtual networking stack.
 *
 * \note  Even on VMK_OK, this event might be missing from user-visible
 *        trace, e.g., if the event log buffer is not being drained
 *        fast enough. Log buffer processing tools MUST be designed to 
 *        work around missing event traces.
 *        
 * \note  As a side effect of calling this function the packet may now
 *        contain a pktTrace attribute.
 *
 * \param[in]  pkt        Packet of interest 
 * \param[in]  traceSrcID Identifier for the uplink device generating
 *                        this trace event.
 * \param[in]  eventType  Event type.
 *
 * \retval     VMK_OK         Event recorded successfully.
 * \retval     VMK_FAILURE    Event not recorded.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktTraceRecordEvent(vmk_PktHandle *pkt,
                                         vmk_uint64 traceSrcID,
                                         vmk_PktTraceEventType eventType);

/*
 ***********************************************************************
 * vmk_PktSlabAllocPages --                                       */ /**
 *
 * \ingroup PktIncompat
 * \brief Allocate one or more pages from the packet page pool.
 *
 * \note If more than one page is allocated through this API they are
 * guaranteed to be physically contiguous.
 *
 * \param[in]   numPages     Number of pages to allocate.
 * \param[in]   mem          If set to VMK_PHYS_ADDR_BELOW_4GB, then
 *                           the packet memory will be guaranteed to
 *                           be allocated with a machine address under
 *                           4 gigabytes. All other constraints will
 *                           be allocated from any memory.
 * \param[out]  firstPage    First page in the resulting allocation block.
 *
 * \retval      VMK_OK              Allocation successful.
 * \retval      VMK_BAD_PARAM       If firstPage is NULL.
 * \retval      VMK_NO_MEMORY       Not enough memory in the page pool
 *                                  to satisfy the request.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PktSlabAllocPages(vmk_uint32 numPages,
                                       vmk_MemPhysAddrConstraint mem,
                                       vmk_MPN *firstPage);

/*
 ***********************************************************************
 * vmk_PktSlabFreePages --                                        */ /**
 *
 * \ingroup PktIncompat
 * \brief Free one or more pages allocated from the packet page pool 
 * back to it.
 *
 * \note The pages must have been originally allocated through 
 * vmk_PktSlabAllocPages() and not any other page allocation mechanism.
 *
 * \note Pages must be free'd in the same block size that they have been
 * allocated with. For example, a call to vmk_PktSlabAllocPages() for 6
 * pages should only be followed by a vmk_PktSlabFreePage() for all the
 * 6 pages, and not individual calls for each page, or two calls for 3
 * pages each.
 *
 * \param[in]   page         First page in the block that is being freed.
 * \param[in]   numPages     Number of pages to free.
 *
 ***********************************************************************
 */
void vmk_PktSlabFreePages(vmk_MPN page, 
                         vmk_uint32 numPages);


/*
 ***********************************************************************
 * vmk_PktHeaderCacheMarkTainted --                               */ /**
 *
 * \ingroup Pkt
 * \brief Marks the packet header cache as tainted.
 *
 * This is used as a workaround for the fact that 3rd party code might
 * be modifying the header cache without updating it. Calling this API
 * results in all header cache operations to trigger invalidation of the
 * header cache and reparsing headers as a result.
 *
 * \deprecated
 *
 * \param[in]  pkt      Target packet.
 *
 ***********************************************************************
 */

void vmk_PktHeaderCacheMarkTainted(vmk_PktHandle *pkt);



#endif
/** @} */
/** @} */
