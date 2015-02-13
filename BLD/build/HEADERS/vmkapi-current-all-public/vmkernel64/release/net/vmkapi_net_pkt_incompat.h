/* **********************************************************
 * Copyright 2006 - 2009 VMware, Inc.  All rights reserved.
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

/*
 ***********************************************************************
 * vmk_PktAllocForUplink --                                       */ /**
 *
 * \ingroup Pkt
 * \brief Allocate a vmk_PktHandle containing a single buffer fragment
 *        for the specified uplink.
 *
 * Please refer to vmk_PktAlloc() documentation for properties of the 
 * allocated packet.
 *
 * The packet is allocated so it may be easily DMA mapped for the
 * uplink.
 *
 * \param[in]  len         Minimum size of the buffer allocated for
 *                         the new packet.
 * \param[in]  uplink      The uplink the packet will be allocated for.
 * \param[out] pkt         Pointer to the allocated vmk_PktHandle.
 *
 * \retval     VMK_OK           Allocation succeeded.
 * \retval     VMK_BAD_PARAM    Invalid uplink parameter.
 * \retval     VMK_NO_MEMORY    Not enough memory to satisfy the 
 *                              allocation request or an unspecified 
 *                              error has occured.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktAllocForUplink(vmk_ByteCountSmall len,
                                       vmk_Uplink uplink,
                                       vmk_PktHandle **pkt);

/*
 ***********************************************************************
 * vmk_PktReleaseIRQ --                                          */ /**
 *
 * \ingroup Pkt
 * \brief Release all resources of a given vmk_PktHandle from an 
 *        interrupt context.
 *
 * This function should be called instead of vmk_PktRelease() when 
 * releasing packets from an interrupt context. Please refer to 
 * vmk_PktRelease() documentation for more information on releasing 
 * packets.
 *
 * \param[in] pkt    Packet to be released.
 *
 ***********************************************************************
 */

void vmk_PktReleaseIRQ(vmk_PktHandle *pkt);

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
 * The ioData and auxData parameters are both pointers available for the 
 * implentation to set and will be preserved with the vmk_PktHandle. 
 * They can be retrieved at the completion handler to access completion 
 * context information specific to this packet.
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
 * \param[in] auxData             Additional pointer for completion context.
 * \param[in] allowSlowCompletion Packet can be held indefinitely before 
 *                                completion.
 *
 ***********************************************************************
 */

void vmk_PktSetCompletionData(vmk_PktHandle *pkt,
                              vmk_PktCompletionData ioData,
                              vmk_PktCompletionData auxData,
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
 * \param[out] auxData Additional pointer for completion context.
 *
 ***********************************************************************
 */

void vmk_PktGetCompletionData(vmk_PktHandle *pkt,
                              vmk_PktCompletionData *ioData,
                              vmk_PktCompletionData *auxData);

/*
 ***********************************************************************
 * vmk_PktClearCompletionData -                                   */ /**
 *
 * \ingroup Pkt
 * \brief Clear the embedded completion data for the given packet.
 *
 * Clears the completion context pointers set via 
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
 * vmk_PktAdjust --                                               */ /**
 *
 * \ingroup Pkt
 * \brief Adjust the packet buffers by moving frame mapped region and/or 
 *        modifying the fragment array.
 *
 * If a non-zero pushLen argument is supplied this function will move
 * the start of the frame mapped region (vmk_PktFrameMappedPointerGet())
 * forward by pushLen and decrease the frame mapped length
 * (vmk_PktFrameMappedLenGet()) by pushLen. The frame length
 * (vmk_PktFrameLenGet()) is left unmodified.
 *
 * This function will also modify the fragment array and remove all
 * fragments from it that contain all bytes beyond pushLen + adjustLen.
 * The number of fragments of the packet (vmk_PktFragsNb()) may be
 * changed, as well as the length of the last fragment. In case
 * pushLen + adjustLen falls within the first fragment the frame mapped
 * length (vmk_PktFrameMappedLenGet()) and frame mapped pointer
 * (vmk_PktFrameMappedPointerGet()) values can also change.
 *
 * This operation is not reversible, buffer regions left out via pushLen 
 * and adjustLen are no longer considered part of the packet's buffers.
 *
 * \note  A non-zero adjustLen parameter MUST be specified, whereas
 *        the pushLen parameter can be zero.
 *
 * \note  pushLen has to be less than or equal to the size of the first 
 *        fragment.
 *
 * \note  pushLen + adjustLen must be less than or equal to the total 
 *        length of all fragments in the packet.
 *
 * \note  The caller has to make sure that buffers to be removed by
 *        this function via adjustLen have properly been released.
 *
 * \pre   vmk_PktIsBufDescWritable() must be TRUE for pkt.
 *
 * \param[in] pkt       Target packet.
 * \param[in] pushLen   Number of bytes to push the frame mapped region
 *                      forward by.
 * \param[in] adjustLen Number of bytes to keep from the packet fragments 
 *                      after pushLen bytes. Remaining buffers are 
 *                      simply discarded.  When zero, VMK_LIMIT_EXCEEDED 
 *                      is returned.
 *
 * \retval     VMK_OK             The packet adjustement suceeded.
 * \retval     VMK_LIMIT_EXCEEDED The packet adjustment failed.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktAdjust(vmk_PktHandle *pkt,
                               vmk_ByteCountSmall pushLen,
                               vmk_ByteCountSmall adjustLen);

/*
 ***********************************************************************
 * vmk_PktAppend --                                               */ /**
 *
 * \ingroup Pkt
 * \brief Append fragments starting at the specified byte offset from a 
 *        source packet to the destination packet.
 *
 * The frame length, number of fragments and fragment information of the 
 * destination packet can be modified. The source packet is left 
 * unmodified.
 *
 * \note  After this operation the destination packet will contain
 *        references to fragments in the source packet, hence must
 *        be released BEFORE the source packet can safely be released.
 *
 * \note  This function invalidates all previous calls to 
 *        vmk_PktFragGet().
 *
 * \note  srcOffset + appendLen must be less than or equal to the frame
 *        length of the source packet.
 *
 * \pre   vmk_PktIsBufDescWritable() must be TRUE for dstPkt.
 *
 * \param[in] dstPkt      Packet to append fragments into.
 * \param[in] srcPkt      Packet to append fragments from.
 * \param[in] srcOffset   Data offset from the beginning of source packet.
 * \param[in] appendLen   Number of bytes to be appended.
 *
 * \retval    VMK_OK             The fragments have sucessfully been appended.
 * \retval    VMK_LIMIT_EXCEEDED The destination packet doesn't have
 *                               enough empty fragment array entries for
 *                               the append operation.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktAppend(vmk_PktHandle *dstPkt,
                               vmk_PktHandle *srcPkt,
                               vmk_ByteCountSmall srcOffset,
                               vmk_ByteCountSmall appendLen);

/*
 ***********************************************************************
 * vmk_PktAppendFrag --                                           */ /**
 *
 * \ingroup Pkt
 * \brief Append the given buffer as a fragment to the given packet.
 *
 * The given buffer is simply appended at the end of the fragment array
 * of the target packet. The frame length, number of fragments and
 * fragment information of the packet are modified.
 *
 * \note  The given buffer should NOT be released/reused until the packet
 *        is released, since the packet will contain a reference to the
 *        buffer.
 *
 * \note  Note that the vmk_PktRelease() can NOT properly release fragments
 *        appended through this API.
 *
 * \note  This function invalidates all previous calls to 
 *        vmk_PktFragGet().
 *
 * \pre   vmk_PktIsBufDescWritable() must be TRUE for pkt.
 *
 * \param[in] pkt         Target packet.
 * \param[in] fragMA      Fragment machine address - A virtual address
 *                        must first be converted to a machine address 
 *                        with vmk_VA2MA().
 * \param[in] fragLen     Fragment length.
 *
 * \retval    VMK_OK             If the fragment has been appended.
 * \retval    VMK_LIMIT_EXCEEDED The destination packet doesn't have
 *                               enough empty fragment array entries
 *                               for the append operation.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktAppendFrag(vmk_PktHandle *pkt,
                                   vmk_MA fragMA,
                                   vmk_ByteCountSmall fragLen);
 
/*
 ***********************************************************************
 * vmk_PktCheckInternalConsistency --                             */ /**
 *
 * \ingroup Pkt
 * \brief Check for inconsistencies in the internal representation
 *        of the given packet.
 *
 * This function is intended for debugging purposes and should not be 
 * called from the data path. It is intended for code that directly 
 * manipulates vmk_PktHandle fields instead of using vmk_Pkt* API's. 
 * Note that this is not recommended.
 *
 * \param[in]  pkt        Target packet.
 *
 * \retval     VMK_TRUE   Packet is consistent.
 * \retval     VMK_FALSE  Packet is not consistent.
 *
 ***********************************************************************
 */

vmk_Bool vmk_PktCheckInternalConsistency(vmk_PktHandle *pkt);

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
 * vmk_PktSlabAllocPage --                                        */ /**
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
 * \param[in]   allocFlags   Packet allocation flags.
 * \param[out]  firstPage    First page in the resulting allocation block.
 *
 * \retval      VMK_OK              Allocation successful.
 * \retval      VMK_BAD_PARAM       If firstPage is NULL.
 * \retval      VMK_NO_MEMORY       Not enough memory in the page pool
 *                                  to satisfy the request.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PktSlabAllocPage(vmk_uint32 numPages,
                                      vmk_MemPhysAddrConstraint mem,
                                      vmk_PktAllocFlags allocFlags,
                                      vmk_MPN *firstPage);

/*
 ***********************************************************************
 * vmk_PktSlabFreePage --                                         */ /**
 *
 * \ingroup PktIncompat
 * \brief Free one or more pages allocated from the packet page pool 
 * back to it.
 *
 * \note The pages must have been originally allocated through 
 * vmk_PktSlabAllocPage() and not any other page allocation mechanism.
 *
 * \note Pages must be free'd in the same block size that they have been
 * allocated with. For example, a call to vmk_PktSlabAllocPage() for 6
 * pages should only be followed by a vmk_PktSlabFreePage() for all the
 * 6 pages, and not individual calls for each page, or two calls for 3
 * pages each.
 *
 * \param[in]   page         First page in the block that is being freed.
 * \param[in]   numPages     Number of pages to free.
 *
 ***********************************************************************
 */
void vmk_PktSlabFreePage(vmk_MPN page, 
                         vmk_uint32 numPages);

/*
 ***********************************************************************
 * vmk_PktSetPageFrags --                                         */ /**
 *
 * \ingroup PktIncompat
 * \brief Mark a packet as having fragments allocated through
 *        vmk_PktSlabAllocPage().
 *
 * \note This API was born deprecated and will be removed.
 *
 * \param[in]   pkt          Packet to mark as having page fragments.
 *
 ***********************************************************************
 */
void             vmk_PktSetPageFrags(vmk_PktHandle *pkt);

/** @} */
/** @} */

/*
 ***********************************************************************
 * Inet Frame Layouts                                             */ /**
 * 
 * \addtogroup PktIncompat
 *@{
 * \defgroup PktIncompatInetFrameLayout Packet Inet Frame Layout Management (incompatible)
 *@{
 *
 * Family of API's to manage inet frame layout, component offsets and
 * lengths.
 *
 * The inet frame layout attribute, once attached, is used when inet
 * component offset and lenths are needed.  These include TSO segmentation
 * CSUM offload, code which needs to identify the ip source or destination
 * address, along with various other validations for vmk_PktHandle TX
 * via pNics.
 *
 * Once an inet frame layout is attached to a vmk_PktHandle, any clones
 * partial copies, segemented TSO frames, vmk_PktDup() or related frames
 * will inherit a copy of the same layout.
 *
 * An inet frame layout can be associated with a vmk_PktHandle, when the
 * layout of the frame can be recognized.  Such frames are composed
 * of a well known ethernet headers with ethertypes
 * for ipv4 or ipv6, then immediatly followed by the ipv4 or ipv6 header,
 * and possibly immediatly followed by the the next layer header.  For
 * an ip fragmented frame, the next protocol header may not be available,
 * when the fragment isn't the first frame.
 *
 * It is possible for the inet frame layout attachment to
 * fail due to a malformed ip header, which includes, mismatched ether type
 * with respect to ip version, incorrect ip version, short ip header length size,
 * insufficient frame lengt of the inet header payload, and other
 * similar frame layout inconsistencies.  This is not an exhaustive list
 * of all malformed inet frames.
 *
 * It is also possible for the inet frame layout attachment to fail
 * due to memory pressure required to manage a new attribute. 
 * A missing inet frame attribute does not then imply that the
 * vmk_PktHandle is not an ipv4 nor an ipv6 frame.
 *
 * Particular offloads may not be compatible with some inet frame layouts,
 * for example neither TSO nor CSUM offloads can be performed on fragmented
 * ip frames.  Even so, an inet frame layout can still be attached and
 * provide component offset and lengts for other uses.
 *
 * Once the inet frame layout is attached to manage the component offsets
 * and lengths, the inet frame layout MUST be updated when bytes are inserted
 * or removed from the vmk_PktHandle.  
 *
 *- <b>Consistency of the computation of the inet frame layout.</b>
 *\n
 *\n
 * Consistency of the inet frame headers can only be guaranteed when
 * the complete inet headers are contained within the frame mapped
 * area, since the guest is free to modify other bytes described by
 * the vmk_PktHandle.  Attempts are made to acquire components of
 * the inet headers via copyout of related components, but the guest
 * may be able to change portions of the unmapped headers while
 * the parsing is in progress.\n
 *\n
 * For ipv4 frames, the inet frame layout parsing routines require
 * the minimum sized ip header to be within the frame mapped area
 * Ipv6 headers, however, may be large, with each next header described in
 * length and type by the previous.  A malicious guest can modify previously
 * reviewed portions of the header while parsing is focused on a previous
 * portion of the ipv6 header.  One approach to prevent this issue
 * is to completely map the entire packet.  Another approach is to
 * compute the inet frame layout,  take a partial copy of the frame
 * with at the complete inet frame header length, then request the
 * recomputation of the inet headers of the partial copy.\n
 *\n
 *
 *- <b>Example Use</b>
 *\n
 *\n
 * Consider inserting bytes ahead of the the ethernet header. 
 * If the vmk_PktHandle is already writable, and has sufficient
 * bytes in the headroom, the vmk_PktInetLayoutIncrIpHdrOffset() can
 * be called to both attach and describe the layout change.  Otherwise
 * a private copy of the original frame needs to be made.\n
 *\n
 * For the private copy case, 
 * the original frame inet frame layout needs to be unmodified.
 * First, vmk_PktInetLayoutGetInetHeaderLength()
 * is called on the original frame to get the inet header's length. 
 * For this example, assume it returns a non-zero value. A private copy
 * is created using vmk_PktPartialCopyWithHeadroom(), using the length
 * of the inet headers, and a headroom value of the expected inserted
 * length.  Next vmk_PktInetLayoutIncrIpHdrOffset() is called on this
 * partial copy to describe the new ip header's offset.  vmk_PktPullHeadroom()
 * is called to pull the headroom into the mapped area, and the new prefix
 * is then copied to the front of the frame.\n
 *
 *      
 ***********************************************************************
 */
/*
 ***********************************************************************
 * vmk_PktInetFrameLayoutIsAvailable --                           */ /**
 *
 * \ingroup PktIncompatInetFrameLayout
 * \brief Predicate to determine whether an inet frame layout is
 * associated with the pktHandle.
 *
 *
 * \param[in]   pkt     handle to test for an associated inet frame layout.
 *
 * \retval      VMK_TRUE         The pkt handle does have an inet frame
 *                               layout associated.
 *
 * \retval      VMK_FALSE        The pkt handles does not have an inet frame
 *                               layout associated.
 *                              
 ***********************************************************************
 */
vmk_Bool  vmk_PktInetFrameLayoutIsAvailable(vmk_PktHandle *pkt);

/*
 ***********************************************************************
 * vmk_PktInetFrameLayoutIncrIpHdrOffset --                       */ /**
 *
 * \ingroup PktIncompatInetFrameLayout
 * \brief Modify the ip header offset within an inet frame layout.
 *
 * Adjust the location of an ip header withing an associated
 * inet frame layout.  An inet frame layout is attempted to
 * be associated with the frame if one is not already attached.
 * The procedure does not modify the contents of the frame, nor does
 * it guarantee that the frame mapped area covers the complete inet headers.
 *
 * When this call succeeds, frame modification can be make while
 * preserving the csum and TSO offloads. When the call fails, then the
 * caller needs to perform some remediation. 
 * Remeidation may include using software
 * offlaods on the original frame for CSUM or TSO,
 * or some other strategy to deal with the inability to
 * preserve any requested offloads (for example fragment, or imbed
 * the payload in some other transport) to honor the requested
 * offload before modifying any frame contents.
 *
 * Several of the non-VMK_OK return values indicate success,
 * indicate the API was  able to successfully update the layout,
 * but also indicate conditions likely of interest to the 
 * caller; VMK_BUF_TOO_SMALL and VMK_LIMIT_EXCEEDED.
 *
 * VMK_BUF_TOO_SMALL successfully updates the frame layout,
 * but alerts the caller that the frame layout's
 * ip offset is less than the minimim ethernet header length.
 * This value is only returned when 
 * the mtu parameter is non-zero.
 *
 * The VMK_LIMIT_EXCEEDED is intended to le the caller know that
 * the frame needs to be promoted to TSO to allow the offload to
 * be preserved,  The modification of the frame to enable TSO is
 * not performed by the API, but the return code is provided to
 * allow the caller to either update or drop the vmk_PktHandle.
 * This value is only returned when the 'incr' value is greater
 * and zero, and mtu parameter is also non-zero.
 *
 * Several return values will only occur when the value of the mtu
 * parameter is greater than zero.  These include VMK_LIMIT_EXCEEDED,
 * VMK_BUF_TOO_SMALL, VMK_RESULT_TOO_LARGE
 *
 * The procedure can be called with a zero incr, and it will
 * then attempt to associate an inet layout with the pkt. 
 *
 * \param[in]   pkt     handle who's ip header location will be modified
 * \param[in]   incr    Positive or negative change in ip header location
 * \param[in]   mtu     MTU (max transmission unit) for this frame.
 *
 * \retval      VMK_OK  The location of the ip header has been updated
 * \retval      VMK_NOT_SUPPORTED The frame contents are not identified
 *                      as an inet frame, the existing layout is invalid.
 * \retval      VMK_LIMIT_EXCEEDED The location of the ip header has
 *                      been updated, but the MTU has been exceeded. 
 *                      This error return code is only returned for
 *                      TCP frames, since these frames can be promoted
 *                      from csum offload to TSO offload.
 *                      If the pkt has csum offload enabled
 *                      (vmk_PktIsMustCsum() is VMK_TRUE), then although
 *                      the ip header has been updated, the frame cannot
 *                      be transmitted.  This return value will only
 *                      be returned for positive change values.
 * \retval      VMK_BUF_TOO_SMALL The location of the ip header has been
 *                      updated, but the resulting location of the ip
 *                      header is now closer to the frame's beginning than
 *                      the minimum ethernet header.  The frame cannot
 *                      be transmitted since the new ip header location
 *                      interferes with the ethernet header.  This return
 *                      code will only be returned for negative chagne values.
 * \retval      VMK_NOT_FOUND unable to associate layout information with
 *                      the pkt parameter.
 * \retval      VMK_EOVERFLOW the resulting change cannot be represented
 *                      by the field used to manage ip hdr offset,
 *                      'change' has not been applied to the inet layout.
 * \retval      VMK_RESULT_TOO_LARGE The location of the ip header has
 *                      not been updated, since the size with the new
 *                      length is greater than the MTU, and the protocol
 *                      isn't TCP.
 * \retval      VMK_FAILURE Unable to save the updated ip hdr offset
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PktInetFrameLayoutIncrIpHdrOffset(vmk_PktHandle *pkt,
                                                       vmk_int32 incr,
                                                       vmk_uint32 mtu);

/*
 ***********************************************************************
 * vmk_PktInetFrameLayoutSetMalformed --                           */ /**
 *
 * \ingroup PktIncompatInetFrameLayout
 * \brief Mark an inet frame layout associated as malformed.
 *
 * The inet frame layout assocaited with the pktHandle is marked
 * as malformed.  This API is helpful when a private copy of a 
 * frame with inet frame layout will be modified resulting in
 * frame which is not a recognizable ipv4 or ipv6 frame.
 *
 * \param[in]   pkt     handle who's ip header location will be modified
 *
 * \param[in]   createInetLayout if VMK_TRUE an inet layout is associated with
 *                      pkt when none exists, if FALSE, only an existing
 *                      net layout is accessed.
 * \retval      VMK_OK  The pktHandle's inet frame layout has been
 *                      marked as malformed.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PktInetFrameLayoutSetMalformed(vmk_PktHandle *pkt,
                                                    vmk_Bool createInetLayout);

/*
 ***********************************************************************
 * vmk_PktInetFrameLayoutGetIpHdrOffset --                        */ /**
 *                      
 * \ingroup PktIncompatInetFrameLayout
 * \brief Return the offset of the ip header associated with the pkt.
 *
 * If the pkt has an ip header offset associated with it, this will
 * return that value.
 *
 * \param[in]   pkt     handle who's ip hdr offset is to be returned.
 * \param[in]   createInetLayout if VMK_TRUE an inet layout is associated with
 *                      pkt when none exists, if FALSE, only an existing
 *                      inet layout is accessed.
 *
 * \return      Zero, if not set, otherwise returns the offset of the
 *              ip header from the front of the frame.  Does not guarantee
 *              that these bytes will be available in the frame mapped area.
 *
 ***********************************************************************
 */

vmk_uint32 vmk_PktInetFrameLayoutGetIpHdrOffset(vmk_PktHandle *pkt,
                                                vmk_Bool createInetLayout);


/*
 ***********************************************************************
 * vmk_PktInetFrameLayoutGetIpHdrLength --                        */ /**
 *                      
 * \ingroup PktIncompatInetFrameLayout
 * \brief Return the length of the ip header associated with the pkt.
 *
 * If the pkt has an ip header length associated with it, this will
 * return that value.
 *
 * \param[in]   pkt     handle who's ip hdr length is to be returned.
 * \param[in]   createInetLayout if VMK_TRUE an inet layout is associated with
 *                      pkt when none exists, if FALSE, only an existing
 *                      inet layout is accessed.
 *
 * \return      Zero, if not set, otherwise returns the length of the
 *              ip header.  Does not guarantee that these bytes will
 *              be available in the frame mapped area.
 *
 ***********************************************************************
 */

vmk_uint32 vmk_PktInetFrameLayoutGetIpHdrLength(vmk_PktHandle *pkt,
                                                vmk_Bool createInetLayout);


/*
 ***********************************************************************
 * vmk_PktInetFrameLayoutGetInetHeaderLength --                   */ /**
 *                      
 * \ingroup PktIncompatInetFrameLayout
 * \brief Return the length of the sum of all the inet headers.
 *
 * If the pkt has a layout information  associated, this returns the
 * length of the sum of all the inet headers
 *
 * \param[in]   pkt     handle who's inet header length is to be returned.
 * \param[in]   createInetLayout if VMK_TRUE an inet layout is associated with
 *                      pkt when none exists, if FALSE, only an existing
 *                      inet layout is accessed.
 *
 * \return      Zero, if not set, otherwise returns the sum of all the
 *              inet headers from the beginning of the frame.  Any 
 *              modifications of the ip header length via 
 *              vmk_PktInetLayoutIncrIpHdrOffset() are included in this
 *              sum.
 *
 ***********************************************************************
 */

vmk_uint32 vmk_PktInetFrameLayoutGetInetHeaderLength(vmk_PktHandle *pkt,
                                                     vmk_Bool createInetLayout);


/*
 ***********************************************************************
 * vmk_PktInetFrameLayoutGetL4HdrLength --                        */ /**
 *
 * \ingroup PktIncompatInetFrameLayout
 * \brief Return the L4 header length for the indicated handle.
 *
 * If the pkt has layout information associated, this returns the
 * length of the L4 (TCP, UDP, ICMP, IGMP, etc) header.
 *
 * For a fragmented IP packet, a length of zero is returned.
 *
 * \param[in]   pkt     handle who's L4 header length is to be returned.
 * \param[in]   createInetLayout if VMK_TRUE an inet layout is associated with
 *                      pkt when none exists, if FALSE, only an existing
 *                      inet layout is accessed.
 *
 * \return      Zero, if not set, otherwise returns the length of L4
 *              header in bytes.
 *
 ***********************************************************************
 */

vmk_uint32 vmk_PktInetFrameLayoutGetL4HdrLength(vmk_PktHandle *pkt,
                                                vmk_Bool createInetLayout);


/*
 ***********************************************************************
 * vmk_PktInetFrameLayoutGetInetProtocol --                       */ /**
 *
 * \ingroup PktIncompatInetFrameLayout
 * \brief Return the inet protocol of the indicated handle.
 *
 * \param[in]   pkt     handle who's inet header length is to be returned.
 * \param[in]   createInetLayout if VMK_TRUE an inet layout is associated with
 *                      pkt when none exists, if FALSE, only an existing
 *                      inet layout is accessed.
 *
 * \returns     Zero, if not set, otherwise returns the L4 protocol 
 *              type (IPPROTO_TCP, IPPROTO_UCP, etc)
 *
 ***********************************************************************
 */

vmk_uint8 vmk_PktInetFrameLayoutGetInetProtocol(vmk_PktHandle *pkt,
                                                vmk_Bool createInetLayout);


/*
 ***********************************************************************
 * vmk_PktInetFrameLayoutGetIsIPV4 --                                  */ /**
 *
 * \ingroup PktIncompatInetFrameLayout
 * \brief Return VMK_TRUE if the handle's layout is an ipv4 frame.
 *
 * \param[in]   pkt     handle who's inet header length is to be returned.
 * \param[in]   createInetLayout if VMK_TRUE an inet layout is associated with
 *                      pkt when none exists, if FALSE, only an existing
 *                      inet layout is accessed.
 *
 * \return      VMK_TRUE if the pkt layout is known, and the inet frame
 *              is an IPV4 frame. VMK_FALSE otherwise.
 *
 ***********************************************************************
 */

vmk_Bool vmk_PktInetFrameLayoutGetIsIPV4(vmk_PktHandle *pkt,
                                         vmk_Bool createInetLayout);


/*
 ***********************************************************************
 * vmk_PktInetFrameLayoutGetIsIPV6 --                             */ /**
 *
 * \ingroup PktIncompatInetFrameLayout
 * \brief Return VMK_TRUE if the handle's layout is an ipv6 frame.
 *
 * \param[in]   pkt     handle who's inet header length is to be returned.
 * \param[in]   createInetLayout if VMK_TRUE an inet layout is associated with
 *                      pkt when none exists, if FALSE, only an existing
 *                      inet layout is accessed.
 *
 * \return      VMK_TRUE if the pkt layout is known, and the inet frame
 *              is an IPV6 frame, VMK_FALSE otherwise
 *
 ***********************************************************************
 */

vmk_Bool vmk_PktInetFrameLayoutGetIsIPV6(vmk_PktHandle *pkt,
                                         vmk_Bool createInetLayout);


/*
 ***********************************************************************
 * vmk_PktInetFrameLayoutGetComponents --                         */ /**
 *
 * \ingroup PktIncompatInetFrameLayout
 * \brief Provide various inet frame layout components, along with
 *      a status return.
 *
 * \param[in]   pkt             handle inspected for inet frame layout.
 * \param[out]  ipHdrOffset     Reference to vmk_uint32 where the ip header
 *                              offset will be stored when VMK_OK is returned.
 *                              If NULL is passed, this parameter is not referenced.
 * \param[out]  ipHdrLength     Reference to vmk_uint32 where the ip header
 *                              length will be stored when VMK_OK is returned.
 *                              If NULL is passed, this parameter is not referenced.
 * \param[out]  ipVersion       Reference to vmk_uint8 where inet frame
 *                              layout details descriving either ipv4 or
 *                              ipv6 will be stored.  '4' indicates ipv4,
 *                              and '6' is stored for ipv6.
 *                              If the inet frame layout is neither an ipv4
 *                              nor ipv6 frame, VMK_NOT_SUPPORTED will be returned
 *                              as a error condition from the procedure.
 *                              If NULL is passed, this parameter is not referenced.
 * \param[out]  l4Protocol      Reference to vmk_uint8 where the l4 layer
 *                              inet protocol is stored.  If not available,
 *                              and requested, VMK_NOT_SUPPORTED will be 
 *                              returned.  If NULL is pased, this parameter
 *                              is not referenced.
 * \retval      VMK_OK  The inet frame layout is available, and requested
 *                               fields have been populated
 * \retval      VMK_FAILURE No inet frame layout is associated with the
 *                               pktHandle.
 * \retval      VMK_NOT_SUPPORTED The inet frame layout cound not be determined
 *                              due to invalid frame contents.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktInetFrameLayoutGetComponents(vmk_PktHandle *pkt,
                                                     vmk_uint32 *ipHdrOffset,
                                                     vmk_uint32 *ipHdrLength,
                                                     vmk_uint8 *ipVersion,
                                                     vmk_uint8 *l4Protocol);

#endif
/** @} */
/** @} */
