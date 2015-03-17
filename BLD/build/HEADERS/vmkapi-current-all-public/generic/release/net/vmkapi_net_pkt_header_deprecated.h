/***************************************************************************
 * Copyright 2014 VMware, Inc.  All rights reserved.
 ***************************************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * PktHeader                                                      */ /**
 *
 * \addtogroup Network
 * @{
 * \addtogroup PktHeader Packet Header Utilities
 * @{
 * \defgroup Deprecated PktHeader Packet Header Utilities
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_NET_PKT_HEADER_DEPRECATED_H_
#define _VMKAPI_NET_PKT_HEADER_DEPRECATED_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/*
 ***********************************************************************
 * vmk_PktHeaderArrayLenGet --                                    */ /**
 *
 * \ingroup PktHeader
 * \brief Return the number of entries in the given packet's header
 *        entries array
 *
 * \deprecated This function is deprecated and will be removed in a
 *             future release. Manipulating the header cache array
 *             directly is no longer supported. Callers should instead
 *             use vmk_PktHeaderEntryInsert() or invalidate the cache
 *             with vmk_PktHeaderInvalidateAll().
 *
 * \param[in]  pkt       Packet to return array length.
 *
 * \return               Length of the packet's header entry array
 *
 ***********************************************************************
 */
vmk_uint16          vmk_PktHeaderArrayLenGet(vmk_PktHandle *pkt);

/*
 ***********************************************************************
 * vmk_PktHeaderArrayGet --                                       */ /**
 *
 * \ingroup PktHeader
 * \brief Return the given packet's header entries array for direct
 *        modification.
 *
 * \deprecated This function is deprecated and will be removed in a
 *             future release. Manipulating the header cache array
 *             directly is no longer supported. Callers should instead
 *             use vmk_PktHeaderEntryInsert() or invalidate the cache
 *             with vmk_PktHeaderInvalidateAll().
 *
 * This function is intended for use by NIC drivers or other packet
 * producing code paths that have pre-existing knowledge of the header
 * layout for the packet, and thus would like to fill the information
 * in for the benefit of the rest of the stack.
 *
 * \see vmk_PktHeaderArrayLenGet() for obtaining the length of this
 *      array.
 *
 * \param[in]  pkt       Packet to return the header entry array.
 *
 * \return               Packet's header entry array
 *
 ***********************************************************************
 */
vmk_PktHeaderEntry *vmk_PktHeaderArrayGet(vmk_PktHandle *pkt);

/*
 ***********************************************************************
 * vmk_PktHeaderArrayAlloc --                                     */ /**
 *
 * \ingroup PktHeader
 * \brief Allocate a new header entry array to associate with a packet.
 *
 * \deprecated This function is deprecated and will be removed in a
 *             future release. Manipulating the header cache array
 *             directly is no longer supported. Callers should instead
 *             use vmk_PktHeaderEntryInsert() or invalidate the cache
 *             with vmk_PktHeaderInvalidateAll().
 *
 * This function can be used to grow a packet's header entry array if
 * the current array length is insufficient to populate all the header
 * information.
 *
 * \see vmk_PktHeaderArraySet() to associate this array with a packet.
 *
 * \param[in]  numEntries Desired number of header entries in the
 *                        allocated array.
 *
 * \return                Allocated array, NULL if allocation fails.
 *
 ***********************************************************************
 */
vmk_PktHeaderEntry *vmk_PktHeaderArrayAlloc(vmk_uint16 numEntries);

/*
 ***********************************************************************
 * vmk_PktHeaderArraySet --                                       */ /**
 *
 * \ingroup PktHeader
 * \brief Associate the given header entry array with the given packet.
 *
 * \deprecated This function is deprecated and will be removed in a
 *             future release. Manipulating the header cache array
 *             directly is no longer supported. Callers should instead
 *             use vmk_PktHeaderEntryInsert() or invalidate the cache
 *             with vmk_PktHeaderInvalidateAll().
 *
 * This function is intended for use by NIC drivers or other packet
 * producing code paths that have pre-existing knowledge of the header
 * layout for the packet, and thus would like to fill the information
 * in for the benefit of the rest of the stack.
 *
 * \note The array needs to have been allocated with
 *       vmk_PktHeaderArrayAlloc()
 *
 * \note This function will free the existing array if necessary.
 *
 * \param[in] pkt         Packet to replace its header entry array.
 * \param[in] hdrEntry    Header entry array to set.
 * \param[in] numEntries  Size of the hdrEntry array.
 * \param[in] usedEntries Number of entries in hdrEntry that are
 *                        actually used, ie. have valid header data
 *                        populated.
 *
 * \retval    VMK_OK        Operation successful.
 * \retval    VMK_BAD_PARAM Invalid parameter(s) specified.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PktHeaderArraySet(vmk_PktHandle *pkt,
                                       vmk_PktHeaderEntry *hdrEntry,
                                       vmk_uint16 numEntries,
                                       vmk_uint16 usedEntries);

#endif /* _VMKAPI_NET_PKT_HEADER_DEPRECATED_H_ */
/** @} */
/** @} */
/** @} */
