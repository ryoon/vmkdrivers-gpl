/***************************************************************************
 * Copyright 2014 VMware, Inc.  All rights reserved.
 ***************************************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Pkt
 *
 * \addtogroup Network
 * @{
 * \addtogroup Pkt Packet Management
 * @{
 * \defgroup Deprecated Pkt Packet Management Utilities
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_NET_PKT_DEPRECATED_H_
#define _VMKAPI_NET_PKT_DEPRECATED_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/**
 * Maximum number of fragments a packet can contain
 *
 * \deprecated This definition will be removed in a future release. Use
 *             vmk_PktSgArrayGet() to query the number of fragments in a
 *             given vmk_PktHandle.
 */
#define VMK_PKT_FRAGS_MAX_LENGTH 24

/**
 * \ingroup Pkt
 * \struct vmk_PktFrag
 * \brief Structure representing a buffer fragment that is part of a packet.
 *
 * \deprecated This definition will be removed in a future release in favor
 *             of vmk_SgElem.
 *
 * This structure is used for returning fragment information via vmk_PktFragGet().
 */
typedef struct {
   /** Machine address of the buffer fragment. */
   vmk_MA      addr;

   /** Length of the buffer fragment. */
   vmk_ByteCountSmall  length;
} vmk_PktFrag;

/*
 ***********************************************************************
 * vmk_PktFragGet --                                              */ /**
 *
 * \ingroup Pkt
 * \brief Retrieve information for a particular buffer fragment of a
 *        packet.
 *
 * The fragment information is returned in a vmk_PktFrag struct and 
 * contains the machine address of the buffer and its length. The API
 * caller has to manage the allocation and release of the vmk_PktFrag
 * struct.
 *
 * This function can only be called for existing valid fragment indices
 * in the packet, ie. from index 0 to (vmk_PktFragsNb() - 1) 
 *
 * \note  A vmk_PktHandle can have at most #VMK_PKT_FRAGS_MAX_LENGTH
 *        fragments.
 *
 * \deprecated This function will be removed in a future release. See
 *             vmk_PktSgElemGet() as a replacement.
 *
 * \param[in]  pkt           Target packet.
 * \param[out] frag          Structure for returning fragment information
 * \param[in]  entry         Index of the queried fragment. The first
 *                           fragment of a vmk_PktHandle is at index
 *                           0.
 *
 * \retval     VMK_OK        Fragment data returned sucessfully.
 * \retval     VMK_NOT_FOUND No fragment exists with the given index.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktFragGet(vmk_PktHandle *pkt,
                                vmk_PktFrag *frag,
                                vmk_uint16 entry);

/*
 ***********************************************************************
 * vmk_PktFragsNb --                                              */ /**
 *
 * \ingroup Pkt
 * \brief Return the number of fragments attached to the given 
 *        vmk_PktHandle.
 *
 * \deprecated This function will be removed in a future release. See
 *             vmk_PktSgArrayGet() as a replacement.
 *
 * \param[in]  pkt      Target packet.
 *
 * \return              Number of fragments.
 *
 ***********************************************************************
 */

vmk_uint16 vmk_PktFragsNb(vmk_PktHandle *pkt);

#endif /* _VMKAPI_NET_PKT_DEPRECATED_H_ */
/** @} */
/** @} */
/** @} */
