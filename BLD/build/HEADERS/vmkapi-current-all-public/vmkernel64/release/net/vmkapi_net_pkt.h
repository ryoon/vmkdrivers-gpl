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
 * \defgroup Pkt Packet Management
 *@{ 
 *
 * \par VMKernel packet representation:
 *
 * vmk_PktHandle represents a network packet in the vmkernel. The 
 * following are its important components:
 *
 * - <b>Fragments:</b>
 *\n
 *     The set of non-contiguous buffers that store the frame data. A
 *     packet can have at most VMK_PKT_FRAGS_MAX_LENGTH fragments. 
 *     Fragments can span multiple pages and be larger than 
 *     VMK_PAGE_SIZE, but any individual fragment has to be physically 
 *     contiguous in the machine address space.\n
 *\n
 * - <b>Mapped Area:</b>
 *\n
 *     In order to access packet data, the buffer containing it needs to
 *     be mapped in virtual memory. In vmkernel, only a leading portion
 *     of the first fragment of the packet is guaranteed to be mapped in
 *     virtual memory. The mapped area of a frame (returned via
 *     vmk_PktFrameMappedPointerGet() with length of
 *     vmk_PktFrameMappedLenGet()) starts from the beginning of the 
 *     first fragment.\n
 *\n
 *     The mapped area may also be larger than the frame 
 *     length, which is often the case for Rx processing since pNIC's 
 *     will allocate fixed size buffers in advance without knowing the 
 *     size of the frame that will be received. Thus, when processing 
 *     data use MIN(FrameLen, FrameMappedLen) to determine which part 
 *     of the frame can be accessed using the frame mapped pointer:\n
 *\n
 *     \code
 *     int i;
 *     unsigned int len;
 *     vmk_uint8 *frameVA;
 *
 *     len = MIN(vmk_PktFrameMappedLenGet(pkt), vmk_PktFrameLenGet(pkt));
 *     frameVA = (vmk_uint8 *) vmk_PktFrameMappedPointerGet(pkt);
 *
 *     printk("frame bytes that are mapped = 0x%x\n", len);
 *     for (i = 0; i < len; i++) {
 *        printk("0x%x ", frameVA[i]);
 *     }
 *     printk("\n");
 *     \endcode
 *\n
 *\n
 *     For TSO the mapped area must comtain the complete inet headers.  
 *     vmk_PktInetFrameLayoutGetInetHeaderLength()
 *     may be valuable in computing the complete length to determine
 *     when a partial copy may be necessary.\n
 *\n
 *     Care ought to be taken when packet data ouside the mapped area
 *     is accessed. as guests are free to modify memory regions 
 *     associated with the packet.  For example, if an ip header is outside
 *     the mapped area, a single copy of the ip header into
 *     local memory is a good practive to attempt to acquire a single
 *     consistent image, although the guest may still modify underlying
 *     storage.
 *\n
 * - <b>Headroom:</b>
 *\n
 *     The padding space available in front of the frame contents. 
 *     Packets allocated with vmk_PktAlloc() may not have any headroom 
 *     space. Headroom space can be adjusted using 
 *     vmk_PktPushHeadroom(), vmk_PktPullHeadroom() or vmk_PktAdjust(). 
 *     These API's redefine the bytes in the beginning of the frame
 *     to become the headroom. If it is desired to keep all the frame
 *     data and insert headroom in front of them then API's such as
 *     vmk_PktPartialCopyWithHeadroom() or vmk_PktDupWithHeadroom()
 *     can be used.
 *\n
 * - <b>Metadata:</b>
 *\n
 *     Apart from the buffers containing the frame data, 
 *     vmk_PktHandle also contains metadata fields and flags that 
 *     describe attributes of the packet. Some of these fields are:\n
 *\n
 *    - <b>VLAN ID:</b>\n
 *      802.1q VLAN identifier tag if one is associated with the packet.
 *      Valid values are 0 - 4094 inclusive. Please note that although
 *      the 802.1q specifications allow 4095 as a valid VLAN ID, this
 *      value is reserved under vmkernel and must not be used to tag
 *      packets. In vmkernel outer tags of packets with VLAN-tagged 
 *      ethernet headers are stripped from the frame and stored inside 
 *      the vmk_PktHandle VLAN ID metadata.\n
 *\n
 *      The VLAN ID of a packet can be accessed using vmk_PktVlanIDGet() 
 *      and changed via vmk_PktVlanIDSet() and vmk_PktVlanIDClear(). 
 *      Note that setting a VLAN ID for a packet causes the packet to 
 *      be marked for possible VLAN tag insertion into the frame's ethernet
 *      header before the packet is delivered (note that in some
 *      configurations such as vswitch tagging mode, packets going into a
 *      VM will not be subject to VLAN tag insertion even if a VLAN ID was
 *      set). Whether a packet is marked for VLAN tag insertion can be
 *      checked via vmk_PktMustVlanTag().\n
 *\n
 *    - <b>Large TCP packet flag</b>\n
 *      The large TCP packet flag identifies a packet as a TSO (TCP 
 *      Segmentation Offload) or LRO (Large Receive Offload) packet. 
 *      This flag is only allowed for TCP packets, and indicates 
 *      that the frame is potentially larger than the default MTU size,
 *      and thus TCP segmentation must be applied before transmitting 
 *      the packet out of an uplink or receiving into a VM without LPD 
 *      (Large Packet Delivery) support. In the TSO case this flag 
 *      implies that the TCP checksum needs to be computed.\n
 *\n
 *      The state of the large tcp packet flag can be retrieved with 
 *      vmk_PktIsLargeTcpPacket() and its value can be modified with 
 *      vmk_PktSetLargeTcpPacket(), which also requires the MSS 
 *      (Maximum segment size) for the packet to be specified. The 
 *      maximum segment size value is for use by the TCP segmentation 
 *      implementation (software or hardware in case of pNIC) to 
 *      determine the maximum TCP payload size for individual packets.
 *      The MSS value can be retrieved with vmk_PktGetLargeTcpPacketMss()\n
 *\n
 *    - <b>Checksum required flag:</b>\n
 *      This flag indicates to vmkernel that the underlying packet is 
 *      using layer 4 checksum offload and requires the checksum to be 
 *      computed and inserted before final delivery of the frame. 
 *      TCP/UDP over IPv4 and IPv6 are supported in software, and if 
 *      supported by the pNIC are offloaded to hardware for outgoing 
 *      packets. The value of this flag can be accessed with 
 *      vmk_PktIsMustCsum() and modified with vmk_PktSetMustCsum()\n
 *\n
 *    - <b>Checksum verified flag:</b>\n
 *      This flag indicates to vmkernel that the layer 4 checksum of the
 *      underlying packet (TCP or UDP) has already been verified, and 
 *      doesn't need to be verified again before final delivery of the 
 *      frame. The value of this flag can be modified with 
 *      vmk_PktClearCsumVfd() and vmk_PktSetCsumVfd()\n
 *
 * \par vmk_PktHandle Serialization:
 *
 * VMkernel does NOT provide any serialization guarantees with respect
 * to accessing a vmk_PktHandle. The API user must ensure:
 *\n
 * - Only one thread is operating on a given vmk_PktHandle at any time.
 * - A vmk_PktHandle is a member of AT MOST one vmk_PktList.
 * 
 ***********************************************************************
 */

#ifndef _VMKAPI_NET_PKT_H_
#define _VMKAPI_NET_PKT_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "net/vmkapi_net_types.h"

/** Maximum number of fragments a packet can contain */
#define VMK_PKT_FRAGS_MAX_LENGTH 24

/** Packet handle representation */
typedef struct PktHandle vmk_PktHandle;

/**
 * \ingroup Pkt
 * \struct vmk_PktFrag
 * \brief Structure representing a buffer fragment that is part of a packet.
 *
 * This structure is used for returning fragment information via vmk_PktFragGet().
 */
typedef struct {
   /** Machine address of the buffer fragment. */
   vmk_MA      addr;

   /** Length of the buffer fragment. */
   vmk_ByteCountSmall  length;
} vmk_PktFrag;

/** Handle used to deregister a dynamic vmk_PktHandle attribute from the system. */
typedef struct vmk_PktAttrRegHandle vmk_PktAttrRegHandle;

/** Handle used to get/set a dynamic vmk_PktHandle attribute. */
typedef struct vmk_PktAttrPktHandle vmk_PktAttrPktHandle;

/** Dynamic vmk_PktHandle attribute value type */
typedef vmk_uint64 vmk_PktAttrValue;

/**
 * \ingroup Pkt
 * \brief Packet allocation flags for use with vmk_PktAllocWithFlags()
 */
typedef enum vmk_PktAllocFlags {
   /** Buffer fragment is allocated from high memory (>= 4GB) in machine
    * address space. If high memory is not available in the system the
    * buffer will be allocated from low memory instead.
    */
   VMK_PKT_ALLOC_FROM_LOW_MEM    = 0x00000001,

   /** Buffer fragment is allocated from low memory (< 4GB) in machine
    * address space. This flag is mutually exclusive with
    * VMK_PKT_ALLOC_FROM_HIGH_MEM. In case neither of these two flags
    * are specified the buffer allocation defaults to low memory.
    */
   VMK_PKT_ALLOC_FROM_HIGH_MEM   = 0x00000002,
} vmk_PktAllocFlags;

/**
 * \ingroup Pkt
 * \struct vmk_PktAttrProperties
 * \brief Structure representing the properties of a dynamic vmk_PktHandle
 * attribute to be registered.
 *
 * This structure is used to provide information about an attribute to 
 * be registered via vmk_PktAttrRegister().
 */

#define VMK_PKT_ATTR_NAME_LENGTH_MAX    255

typedef struct vmk_PktAttrProperties {
   /** Module ID of the module registering the attribute.
       May be used to delay module unload. Must not be zero or invalid. */
   vmk_ModuleID moduleID;
   /** Unique NULL-terminated ANSI string name/description of attribute. */
   vmk_uint8 *name;

} vmk_PktAttrProperties;

/*
 ***********************************************************************
 * vmk_PktAlloc --                                                */ /**
 *
 * \ingroup Pkt
 * \brief Allocate a vmk_PktHandle containing a single buffer fragment.
 *
 * This function allocates a vmk_PktHandle with its metadata fields
 * initialized to these default values:
 *
 * <b>VLAN:</b> No VLAN (tagging not required)\n
 * <b>Priority:</b> 0\n
 * <b>Csum offload:</b> Not required\n
 * <b>Csum verified:</b> No\n
 * <b>TSO required:</b> No\n
 *
 * A single buffer (guaranteed to be contiguous in machine address space) 
 * at least as large as the requested length is allocated from low memory
 * and appended to the vmk_PktHandle as the first (index 0) fragment.
 *
 * The returned vmk_PktHandle has a modifiable buffer descriptor (ie.
 * vmk_PktIsBufDescWritable() is VMK_TRUE) until a copy/clone operation
 * such as vmk_PktPartialCopy() or vmk_PktClone() is performed.
 *
 * \param[in]  len         Minimum size of the buffer allocated for this 
 *                         packet.
 * \param[out] pkt         Pointer to the allocated vmk_PktHandle.
 *
 * \retval     VMK_OK           Allocation succeeded.
 * \retval     VMK_NO_MEMORY    Not enough memory to satisfy the 
 *                              allocation request, or an unspecified 
 *                              error has occured.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktAlloc(vmk_ByteCountSmall len,
                              vmk_PktHandle **pkt);

/*
 ***********************************************************************
 * vmk_PktAllocWithFlags --                                       */ /**
 *
 * \ingroup Pkt
 * \brief Allocate a vmk_PktHandle containing a single buffer fragment
 *        with the specified allocation flags.
 *
 * Please refer to vmk_PktAlloc() documentation for properties of the 
 * allocated packet. These properties can be customized by use of
 * allocation flags, refer to vmk_PktAllocFlags for documentation of
 * these flags.
 *
 * \param[in]  len         Minimum size of the buffer allocated for this 
 *                         packet.
 * \param[in]  allocFlags  Packet allocation flags.
 * \param[out] pkt         Pointer to the allocated vmk_PktHandle.
 *
 * \retval     VMK_OK           Allocation succeeded.
 * \retval     VMK_NO_MEMORY    Not enough memory to satisfy the 
 *                              allocation request, or an unspecified 
 *                              error has occured.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktAllocWithFlags(vmk_ByteCountSmall len,
                                       vmk_PktAllocFlags allocFlags,
                                       vmk_PktHandle **pkt);

/*
 ***********************************************************************
 * vmk_PktRelease --                                              */ /**
 *
 * \ingroup Pkt
 * \brief Release all resources of a given vmk_PktHandle.
 *
 * This function cleans up all resources that are referred to by the 
 * given vmk_PktHandle, which includes memory for the vmk_PktHandle
 * as well as the original buffer fragment allocated by vmk_PktAlloc().
 *
 * \pre   The packet MUST have a single buffer fragment that is
 *        identical to the fragment allocated by vmk_PktAlloc(), or a
 *        completion handler must have been registered by the entity that
 *        added the extra buffers. Note that version 2.x of the packet
 *        management API does NOT provide a compatible function to
 *        register such a handler.
 *
 * \pre   This function should only be called from a non-IRQ context.
 *        For freeing packets from an interrupt context use
 *        vmk_PktReleaseIRQ().
 *
 * \param[in]  pkt    Packet to be released.
 *
 ***********************************************************************
 */

void vmk_PktRelease(vmk_PktHandle *pkt);

/*
 ***********************************************************************
 * vmk_PktIsBufDescWritable --                                    */ /**
 *
 * \ingroup Pkt
 * \brief Returns whether the packet has a private buffer descriptor. 
 *
 * A packet can only modify its buffer descriptor (modify the contents 
 * of the buffers including the frame mapped region, append fragments,
 * modify frameLen etc.) if this function returns VMK_TRUE. Operations
 * such as vmk_PktPartialCopy(), vmk_PktClone() etc. can change the
 * buffer descriptor writability. Please check individual function
 * documentation for API functions that require this function to return
 * VMK_TRUE as a pre-requisite, and for functions that can change the
 * buffer writability status of a vmk_PktHandle.
 *
 * \param[in]  pkt        Target packet.
 *
 * \retval     VMK_TRUE   Buffer descriptor is writable.
 * \retval     VMK_FALSE  Buffer descriptor is not writable.
 *
 ***********************************************************************
 */

vmk_Bool vmk_PktIsBufDescWritable(const vmk_PktHandle *pkt);

/*
 ***********************************************************************
 * vmk_PktFrameLenGet --                                          */ /**
 *
 * \ingroup Pkt
 * \brief Retrieve the frame length of the given packet.
 *
 * The frame length of the packet indicates the number of bytes which 
 * constitute the actual frame data that will be delivered.
 *
 * \param[in]  pkt      Target packet.
 *
 * \return              Frame length.
 *
 ***********************************************************************
 */

vmk_ByteCountSmall vmk_PktFrameLenGet(vmk_PktHandle *pkt);

/*
 ***********************************************************************
 * vmk_PktFrameLenSet --                                          */ /**
 *
 * \ingroup Pkt
 * \brief Set the frame length of the given packet.
 *
 * The frame length of the packet indicates the number of bytes which 
 * constitute the actual frame data.
 *
 * \param[in]  pkt    Target packet
 * \param[in]  len    Size of the frame described by the packet. Has to
 *                    be less than or equal to the amount of available
 *                    buffer space in the vmk_PktHandle.
 *
 * \pre vmk_PktIsBufDescWritable() must be TRUE for pkt.
 *
 * \retval     VMK_OK        If the frame length is set.
 * \retval     VMK_BAD_PARAM Supplied frame length larger than available
 *                           buffer space.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktFrameLenSet(vmk_PktHandle *pkt,
                                    vmk_ByteCountSmall len);

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
 * \param[in]  pkt      Target packet.
 *
 * \return              Number of fragments.
 *
 ***********************************************************************
 */

vmk_uint16 vmk_PktFragsNb(vmk_PktHandle *pkt);

/*
 ***********************************************************************
 * vmk_PktIsFlatBuffer --                                         */ /**
 *
 * \ingroup Pkt
 * \brief Returns whether the packet has less than or equal to one buffer
 *        fragment.
 *
 * \note  A packet may have zero fragments if it was allocated through
 *        vmk_PktAlloc() with a length of 0.
 *
 * \param[in]  pkt        Target packet.
 *
 * \retval     VMK_TRUE   Packet has <= 1 buffer.
 * \retval     VMK_FALSE  Packet consists of >1 buffers.
 *
 ***********************************************************************
 */

vmk_Bool vmk_PktIsFlatBuffer(vmk_PktHandle *pkt);

/*
 ***********************************************************************
 * vmk_PktFrameMappedLenGet --                                    */ /**
 *
 * \ingroup Pkt
 * \brief Retrieve the length of the frame mapped area of the given
 *        packet.
 *
 * \see   Refer to the "Mapped Area" section of the \ref Pkt detailed
 *        desription.
 *
 * \param[in]  pkt            Target packet.
 *
 * \return                    Length of the frame mapped area.
 *
 ***********************************************************************
 */

vmk_ByteCountSmall vmk_PktFrameMappedLenGet(const vmk_PktHandle *pkt);

/*
 ***********************************************************************
 * vmk_PktIsFullyMapped --                                        */ /**
 *
 * \ingroup Pkt
 * \brief Returns whether the entire packet buffer is mapped.
 *
 * A packet is fully mapped if it consists of a single fragment and all 
 * of that fragment is mapped. If this function returns VMK_TRUE, that
 * implies that vmk_PktFragsNb() is 1, vmk_PktIsFlatBuffer() is VMK_TRUE
 * and vmk_PktFrameMappedLenGet() is equal to the length of the first
 * buffer fragment.
 *
 * \param[in]  pkt           Target packet.

 * \retval     VMK_TRUE      Packet buffer is fully mapped.
 * \retval     VMK_FALSE     Packet buffer(s) not fully mapped.
 *
 ***********************************************************************
 */

vmk_Bool vmk_PktIsFullyMapped(vmk_PktHandle *pkt);

/*
 ***********************************************************************
 * vmk_PktFrameMappedPointerGet --                                */ /**
 *
 * \ingroup Pkt
 * \brief Retrieve the pointer to the frame mapped area of the given 
 *        packet.
 *
 * \see   Refer to the "Mapped Area" section of the \ref Pkt detailed
 *        desription.
 *
 * \param[in]  pkt         Target packet.
 *
 * \return                 A pointer to the frame mapped area.
 *
 ***********************************************************************
 */

vmk_VA vmk_PktFrameMappedPointerGet(const vmk_PktHandle *pkt);
                                  
/*
 ***********************************************************************
 * vmk_PktPriorityGet --                                            */ /**
 *
 * \ingroup Pkt
 * \brief Returns the 802.1p priority associated with the given packet.
 *
 * \note  The priority that is returned is part of the vmk_PktHandle
 *        metadata, it doesn't come from the ethernet header of the
 *        underlying frame.
 *
 * \param[in]  pkt        Target packet.
 *
 * \return                Packet priority.
 *
 ***********************************************************************
 */

vmk_VlanPriority vmk_PktPriorityGet(vmk_PktHandle *pkt);

/*
 ***********************************************************************
 * vmk_PktPrioritySet --                                            */ /**
 *
 * \ingroup Pkt
 * \brief Modify 802.1p priority of the given packet.
 *
 * This function sets the priority metadata for the given packet and if 
 * supplied a non-zero priority marks the packet as "must insert VLAN 
 * tag" so that tag insertion is performed prior to final delivery of the 
 * frame. A zero priority value will clear this flag, and is identical 
 * to calling vmk_PktPriorityClear().
 *
 * \note  The priority that is modified is part of the vmk_PktHandle
 *        metadata, this function doesn't change the ethernet header of 
 *        the underlying frame.
 *
 * \note If priority parameter is outside the range of [0-7] then this
 *       function will cause an ASSERT failure on debug builds, and
 *       set (priority % 8) instead on release builds.
 *
 * \param[in] pkt        Target packet.
 * \param[in] priority   New priority between [0-7].
 *
 * \retval    VMK_OK        Priority set successfully.
 * \retval    VMK_BAD_PARAM Invalid priority value or NULL pkt parameter.
 * \retval    Otherwise     Failed to set priority.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktPrioritySet(vmk_PktHandle *pkt,
                                    vmk_VlanPriority priority);


/*
 ***********************************************************************
 * vmk_PktPriorityClear --                                            */ /**
 *
 * \ingroup Pkt
 * \brief Clear 802.1p priority of the given packet.
 *
 * This function sets the priority metadata for the given packet to 0 and 
 * unsets the "must insert VLAN tag" flag.
 *
 * \note  The priority that is cleared is part of the vmk_PktHandle
 *        metadata, this function doesn't change the ethernet header of 
 *        the underlying frame.
 *
 * \param[in] pkt        Target packet.
 *
 ***********************************************************************
 */

void vmk_PktPriorityClear(vmk_PktHandle *pkt);

/*
 ***********************************************************************
 * vmk_PktVlanIDGet --                                            */ /**
 *
 * \ingroup Pkt
 * \brief Returns the VLAN ID associated with the given packet.
 *
 * \note  The VLAN ID that is returned is part of the vmk_PktHandle
 *        metadata, it doesn't come from the ethernet header of the
 *        underlying frame.
 *
 * \param[in] pkt     Target packet.
 *
 * \return            VLAN ID associated with the packet.
 *
 ***********************************************************************
 */

vmk_VlanID vmk_PktVlanIDGet(vmk_PktHandle *pkt);

/*
 ***********************************************************************
 * vmk_PktVlanIDSet --                                            */ /**
 *
 * \ingroup Pkt
 * \brief Modify the VLAN ID of the given packet.
 *
 * This function sets the VLAN ID metadata for the given packet and if 
 * supplied a non-zero VLAN ID marks the packet as "must insert VLAN 
 * tag" so that tag insertion is performed prior to final delivery of the 
 * frame. A zero VLAN ID value will clear this flag, and is identical to 
 * calling vmk_PktVlanIDClear().
 *
 * \note  The VLAN ID that is modified is part of the vmk_PktHandle
 *        metadata, this function doesn't change the ethernet header of 
 *        the underlying frame.
 *
 * \note  If vid parameter is outside the range of [0-4094] then this
 *        function will cause an ASSERT failure on debug builds, and
 *        set (vid % 4095) instead on release builds.
 *
 * \param[in] pkt    Target packet.
 * \param[in] vid    New VLAN ID between [0-4094].
 *
 * \retval    VMK_OK         VLAN ID set successfully.
 * \retval    VMK_BAD_VLANID Invalid VLAN ID (>4094).
 * \retval    Otherwise      Failed to set VLAN ID.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktVlanIDSet(vmk_PktHandle *pkt,
                                  vmk_VlanID vid);

/*
 ***********************************************************************
 * vmk_PktVlanIDClear --                                            */ /**
 *
 * \ingroup Pkt
 * \brief Clear the VLAN ID of the given packet.
 *
 * This function sets the VLAN ID metadata for the given packet to 0 and 
 * unsets the "must insert VLAN tag" flag.
 *
 * \note  The VLAN ID that is cleared is part of the vmk_PktHandle
 *        metadata, this function doesn't change the ethernet header of 
 *        the underlying frame.
 *
 * \param[in] pkt    Target packet.
 *
 ***********************************************************************
 */

void vmk_PktVlanIDClear(vmk_PktHandle *pkt);

/*
 ***********************************************************************
 * vmk_PktResPoolTagGet --                                        */ /**
 *
 * \ingroup Pkt
 * \brief Retrieve the resource pool tag of the given packet.
 *
 * \param[in]  pkt      Target packet 
 * \param[out] poolTag  Packet resource pool tag
 *
 * \retval     VMK_OK         Packet resource pool tag retrieved
 * \retval     VMK_BAD_PARAM  Input parameter not valid
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PktResPoolTagGet(vmk_PktHandle *pkt,
                                      vmk_PortsetResPoolTag *poolTag);

/*
 ***********************************************************************
 * vmk_PktResPoolTagSet --                                        */ /**
 *
 * \ingroup Pkt
 * \brief Tag a packet with the given resource pool tag.
 *
 * \param[in]  pkt      Target packet
 * \param[in]  poolTag  Resource pool tag allocated via vDS API.
 *
 * \retval     VMK_OK         Packet properly tagged
 * \retval     VMK_BAD_PARAM  Input parameter not valid
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PktResPoolTagSet(vmk_PktHandle *pkt,
                                      vmk_PortsetResPoolTag poolTag);

/*
 ***********************************************************************
 * vmk_PktSrcPortIDGet --                                         */ /**
 *
 * \ingroup Pkt
 * \brief Retrieve source port ID of a specified packet.
 *
 * \param[in]  pkt      Target packet
 *
 * \return              Packet's source port ID on success,
 *                      0 on error or if no source port ID was set on
 *                      the packet.
 *
 ***********************************************************************
 */

vmk_SwitchPortID vmk_PktSrcPortIDGet(vmk_PktHandle *pkt);

/*
 ***********************************************************************
 * vmk_PktSrcPortIDSet --                                         */ /**
 *
 * \ingroup Pkt
 * \brief Modify source port ID of the specified packet.
 *
 * \pre   vmk_PktIsBufDescWritable() must be TRUE for pkt.
 *
 * \param[in] pkt        Target packet
 * \param[in] portID     New source port ID
 *
 * \retval    VMK_OK        Source port ID set successfully.
 * \retval    VMK_BAD_PARAM Invalid pkt pointer.
 * \retval    Otherwise     Failed to set source port ID.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktSrcPortIDSet(vmk_PktHandle *pkt,
                                     vmk_SwitchPortID portID);

/*
 ***********************************************************************
 * vmk_PktGetCsumVfd --                                           */ /**
 *
 * \ingroup Pkt
 * \brief Returns whether the given packet is marked as "TCP/UDP checksum
 *        verified".
 *
 * \param[in]  pkt        Target packet.
 *
 * \retval     VMK_TRUE   Checksum already verified.
 * \retval     VMK_FALSE  Checksum not verified.
 *
 ***********************************************************************
 */

vmk_Bool vmk_PktGetCsumVfd(const vmk_PktHandle *pkt);

/*
 ***********************************************************************
 * vmk_PktSetCsumVfd --                                           */ /**
 *
 * \ingroup Pkt
 * \brief Mark the given packet as "TCP/UDP checksum verified".
 *
 * If the packet's TCP/UDP checksum has already been verified, this
 * function can be called before queueing the packet for Rx processing
 * to optimize further checksum computations.
 *
 * \pre   vmk_PktIsBufDescWritable() must be TRUE for pkt.
 *
 * \param[in] pkt    Target packet.
 *
 * \retval    VMK_OK        Flag set successfully.
 * \retval    VMK_READ_ONLY vmk_PktIsBufDescWritable() was FALSE for pkt.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktSetCsumVfd(vmk_PktHandle *pkt);

/*
 ***********************************************************************
 * vmk_PktClearCsumVfd --                                         */ /**
 *
 * \ingroup Pkt
 * \brief Clear the "TCP/UDP checksum verified" flag from the given 
 *        packet.
 *
 * \pre   vmk_PktIsBufDescWritable() must be TRUE for pkt.
 *
 * \param[in]  pkt        Target packet.
 *
 * \retval     VMK_OK        Flag cleared successfully.
 * \retval     VMK_READ_ONLY vmk_PktIsBufDescWritable() was FALSE for pkt.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktClearCsumVfd(vmk_PktHandle *pkt);

/*
 ***********************************************************************
 * vmk_PktIsMustCsum --                                           */ /**
 *
 * \ingroup Pkt
 * \brief Returns whether the given packet requires TCP/UDP checksum to 
 *        be computed and inserted.
 *
 * \param[in]  pkt        Target packet.
 *
 * \retval     VMK_TRUE   Checksumming needed.
 * \retval     VMK_FALSE  Checksumming not needed.
 *
 ***********************************************************************
 */

vmk_Bool vmk_PktIsMustCsum(const vmk_PktHandle *pkt);

/*
 ***********************************************************************
 * vmk_PktClearMustCsum --                                        */ /**
 *
 * \ingroup Pkt
 * \brief Clear the "TCP/UDP checksumming needed" flag from the given
 *        packet.
 *
 * \pre   vmk_PktIsBufDescWritable() must be TRUE for pkt.
 *
 * \param[in]  pkt        Target packet.
 *
 * \retval     VMK_OK        Flag cleared successfully.
 * \retval     VMK_READ_ONLY vmk_PktIsBufDescWritable() was FALSE for pkt.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktClearMustCsum(const vmk_PktHandle *pkt);

/*
 ***********************************************************************
 * vmk_PktSetMustCsum --                                          */ /**
 *
 * \ingroup Pkt
 * \brief Mark the given packet as "TCP/UDP checksumming needed".
 *
 * \pre   The frame contained by pkt must be a IPv4 or IPv6 TCP/UDP 
 *        frame.
 *
 * \pre   vmk_PktIsBufDescWritable() must be TRUE for pkt.
 *
 * \param[in]  pkt        Target packet.
 *
 * \retval     VMK_OK        Flag set successfully.
 * \retval     VMK_READ_ONLY vmk_PktIsBufDescWritable() was FALSE for pkt.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktSetMustCsum(const vmk_PktHandle *pkt);

/*
 ***********************************************************************
 * vmk_PktMustVlanTag --                                          */ /**
 *
 * \ingroup Pkt
 * \brief Returns whether the given packet requires VLAN tag insertion
 *        before frame delivery.
 *
 *        VLAN/Priority values are stored in the vmk_PktHandle while
 *        the packet is in transit within the vmkernel. The presence
 *        of this flag indicates that the endpoint processing is
 *        responsible for insertion of the VLAN tag into the frame
 *        prior to final delivery of the packet.
 *
 * \see   vmk_PktVlanIDSet(), vmk_PktPrioritySet()
 *
 * \param[in]  pkt        Target packet.
 *
 * \retval     VMK_TRUE   Vlan tag needs to be inserted.
 * \retval     VMK_FALSE  Vlan tag insertion is not necessary.
 *
 ***********************************************************************
 */

vmk_Bool vmk_PktMustVlanTag(vmk_PktHandle *pkt);

/*
 ***********************************************************************
 * vmk_PktIsLargeTcpPacket --                                     */ /**
 *
 * \ingroup Pkt
 * \brief Returns whether the given packet requires TCP segmentation
 *        offload.
 *
 *        The presence of this flag indicates that the endpoint processing
 *        is responsible for performing TCP segmentation (for example
 *        using hardware offload capability).
 *
 * \see   vmk_PktSetLargeTcpPacket()
 *
 * \param[in]  pkt        Target packet.
 *
 * \retval     VMK_TRUE   TCP Segmentation Offload is needed.
 * \retval     VMK_FALSE  TCP Segmentation Offload is not needed.
 *
 ***********************************************************************
 */

vmk_Bool vmk_PktIsLargeTcpPacket(const vmk_PktHandle *pkt);

/*
 ***********************************************************************
 * vmk_PktSetLargeTcpPacket --                                    */ /**
 *
 * \ingroup Pkt
 * \brief Mark the given packet as requiring TCP segmentation offload.
 *
 * This function marks a packet as requiring TCP segmentation offload 
 * and sets the maximum segment size to be used by the segmentation 
 * implementation. TCP packets that are larger than the underlying MTU 
 * value due to certain optimizations need to be marked with this flag 
 * so that vmkernel can take appropriate action before delivery of the 
 * frame to an endpoint that doesn't support receiving such packets 
 * (software segmentation) or if the packet is going out of a physical
 * NIC that supports TSO to use hardware offload.
 *
 * \pre   vmk_PktIsBufDescWritable() must be TRUE on this packet.
 *
 * \param[in]  pkt    Target packet.
 * \param[in]  mss    Maximum segment size of the TCP connection for
 *                    use by the software/hardware segmentation
 *                    implementation.
 *
 * \retval     VMK_OK        Flag set successfully.
 * \retval     VMK_READ_ONLY vmk_PktIsBufDescWritable() was FALSE for pkt.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktSetLargeTcpPacket(vmk_PktHandle *pkt,
                                          vmk_uint32 mss);

/*
 ***********************************************************************
 * vmk_PktGetLargeTcpPacketMss --                                 */ /**
 *
 * \ingroup Pkt
 * \brief Returns the MSS value set with vmk_PktSetLargeTcpPacket().
 *
 * \param[in]  pkt    Target packet.
 *
 * \return            Maximum segment size of the TCP connection.
 *
 ***********************************************************************
 */

vmk_uint32 vmk_PktGetLargeTcpPacketMss(vmk_PktHandle *pkt);

/*
 ***********************************************************************
 * vmk_PktClearLargeTcpPacket --                                  */ /**
 *
 * \ingroup Pkt
 * \brief Clear the "TCP segmentation offload required" flag from the
 *        given packet.
 *
 * \note  The MSS value associated with the flag will also be cleared.
 *
 * \pre   vmk_PktIsBufDescWritable() must be TRUE on this packet.
 *
 * \param[in]  pkt        Target packet.
 *
 * \retval     VMK_OK        Flag cleared successfully.
 * \retval     VMK_READ_ONLY vmk_PktIsBufDescWritable() was FALSE for pkt.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktClearLargeTcpPacket(const vmk_PktHandle *pkt);

/*
 ***********************************************************************
 * vmk_PktCopyMetaData --                                         */ /**
 *
 * \ingroup Pkt
 * \brief Copy packet metadata from the given source packet into the
 *        given destination packet.
 *
 * This function copies the vmk_PktHandle metadata from srcPkt into 
 * dstPkt. The following fields are copied:
 *
 * - VLAN ID
 * - Priority
 * - Must VLAN tag flag
 * - Resource pool tag
 * - All dynamic attributes set via vmk_PktAttrSet() 
 *
 * \pre   dstPkt should not have any outstanding references created
 *        via functions such as vmk_PktPartialCopy(), vmk_PktClone()
 *        etc.
 *
 * \param[in] srcPkt Source packet with the metadata to copy.
 * \param[in] dstPkt Packet to copy the metadata into.
 *
 * \retval      VMK_OK          sucessfully moved meta data
 * \retval      otherwise       various errors
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktCopyMetaData(vmk_PktHandle *srcPkt,
                                     vmk_PktHandle *dstPkt);

/*
 ***********************************************************************
 * vmk_PktAttrRegister --                                         */ /**
 *
 * \ingroup Pkt
 * \brief Register a new dynamic vmk_PktHandle attribute.
 *
 * This function can be used by modules that require the ability to
 * store custom metadata information inside vmk_PktHandle. Dynamic
 * attributes do not occupy space in the vmk_PktHandle until they are
 * set, therefore registering a dynamic attribute type does not directly
 * increase memory footprint for all vmk_PktHandle's in the system.
 *
 * \note The only attribute type supported by version 2.x of this API
 *       is of type vmk_PktAttrValue, which can be used to store any
 *       type that requires 64 bits or less storage.
 *
 * \param[in]   properties  Configuration information that describes
 *                          the new attribute and how it's processed.
 * \param[out]  regHandle   Handle to pass to vmk_PktAttrDeregister().
 * \param[out]  attrHandle  Handle to pass to vmk_PktAttrGet() / 
 *                          vmk_PktAttrSet().
 *
 * \retval     VMK_OK            If registration was successful.
 * \retval     VMK_NO_RESOURCES  No space available for attribute.
 * \retval     VMK_BAD_PARAM     One or more properties is invalid.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PktAttrRegister(vmk_PktAttrProperties *properties,
                                     vmk_PktAttrRegHandle **regHandle,
                                     vmk_PktAttrPktHandle **attrHandle);

/*
 ***********************************************************************
 * vmk_PktAttrDeregister --                                       */ /**
 *
 * \ingroup Pkt
 * \brief Unregister a previously registered dynamic attribute from the
 *        system.
 *
 * This function is used to undo the changes made by a successful call
 * to vmk_PktAttrRegister(). The caller is responsible for ensuring that
 * no vmk_PktHandle in the system is actively using the attribute being
 * unregistered before calling into this function.
 *
 * \param[in]   handle      Handle obtained from vmk_PktAttrRegister().
 *
 * \retval     VMK_OK         Attribute successfully deregistered.
 * \retval     VMK_NOT_FOUND  Attribute could not be found to deregister
 *                            or already deregistered.
 * \retval     VMK_BAD_PARAM  Invalid handle was specified.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PktAttrDeregister(vmk_PktAttrRegHandle *handle);

/*
 ***********************************************************************
 * vmk_PktAttrGet --                                              */ /**
 *
 * \ingroup Pkt
 * \brief Obtain the value of a dynamic attribute from the vmk_PktHandle.
 *
 * This function is used to obtain the value of a dynamic attribute that
 * was previously set in a PktHandle.
 *
 * \param[in]   pkt         Target packet.
 * \param[in]   attrHandle  Handle that represents attribute to query.
 * \param[out]  value       Variable that receives the value of the
 *                          attribute.
 *
 * \retval     VMK_OK         Attribute get operation was successful.
 * \retval     VMK_NOT_FOUND  Attribute value is not set in PktHandle.
 * \retval     VMK_BAD_PARAM  One or more handles are invalid.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PktAttrGet(const vmk_PktHandle *pkt,
                                vmk_PktAttrPktHandle *attrHandle,
                                vmk_PktAttrValue *value);

/*
 ***********************************************************************
 * vmk_PktAttrSet --                                              */ /**
 *
 * \ingroup Pkt
 * \brief Set the value of a dynamic attribute in the vmk_PktHandle.
 *
 * This function is used to set the value of a dynamic attribute in the
 * vmk_PktHandle.
 *
 * \param[in]   pkt         Target packet.
 * \param[in]   attrHandle  Handle that represents an attribute.
 * \param[out]  value       Value of attribute to set in frame.
 *
 * \retval     VMK_OK            Attribute set operation was successful.
 * \retval     VMK_NO_RESOURCES  No space available to store attribute.
 * \retval     VMK_BAD_PARAM     One or more handles are invalid.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_PktAttrSet(vmk_PktHandle *pkt,
                                vmk_PktAttrPktHandle *attrHandle,
                                vmk_PktAttrValue value);

/*
 ***********************************************************************
 * vmk_PktPartialCopy --                                          */ /**
 *
 * \ingroup Pkt
 * \brief Create a partial copy of the given packet with a private 
 *        writable region.
 *
 * Creates a partial copy of the source packet. The private area of the
 * resulting packet starts at its frame mapped pointer (see
 * vmk_PktFrameMappedPointerGet()) and will have a 
 * vmk_PktFrameMappedLenGet() value at least as much as the requested 
 * numBytes parameter, and at most the frame length of the original 
 * packet. Frame contents of the resulting packet until
 * vmk_PktFrameMappedLenGet() are allowed to be modified, whereas frame
 * contents beyond that should NOT be modified since they will be
 * referring to buffers shared with the source packet.
 *
 * This function can be used by modules which intend to modify a packet. 
 * In general only the frame mapped area of a packet is allowed to be 
 * modified, and only when vmk_PktIsBufDescWritable() is VMK_TRUE for 
 * that packet. If the data beyond the frame mapped length needs to be 
 * modified or vmk_PktIsBufDescWritable() is VMK_FALSE, a partial copy of 
 * the packet should be made with this function in order to modify the
 * frame contents. The numBytes parameter describes the number of bytes 
 * modifiable in the frame mapped area of the resulting packet.
 *
 * All metadata present on the source packet will be copied to the
 * destination packet. See vmk_PktCopyMetaData() for a list of the
 * metadata fields that are copied.
 *
 * \note  It is possible for the vmk_PktFrameMappedLenGet() of the
 *        resulting packet to be larger than numBytes.
 *
 * \note  The headroom of the resulting copy packet is unspecified,
 *        the copied frame may or may not have the same headroom length
 *        as the original frame.  See vmk_PktPartialCopyWithHeadroom()
 *        when the copy has headroom length requirements.
 *
 * \note  If numBytes is larger than vmk_PktFrameLenGet() for pkt the
 *        partial copy will be taken as if numBytes was equal to
 *        vmk_PktFrameLenGet(). By using vmk_PktFrameLenGet() as
 *        numBytes a complete private copy of the original packet can
 *        be created (see vmk_PktCopy() for an alias of this behavior).
 *
 * \note  The numBytes parameter can be supplied as zero, in which case
 *        the resulting packet will be a clone of the original packet,
 *        ie. it is not guaranteed to have a private mapped region
 *        (see vmk_PktClone()).
 *
 * \note  Partial copies can be made using packets which are the result
 *        of previous vmk_PktPartialCopy() calls.
 *
 * \param[in]   pkt        Target packet.
 * \param[in]   numBytes   Number of bytes in the private region of
 *                         the resulting packet.
 * \param[out]  copyPkt    Resulting packet with a private mapped
 *                         region.
 *
 * \retval     VMK_OK      If the partial copy was successful.
 * \retval     VMK_FAILURE Otherwise.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktPartialCopy(vmk_PktHandle *pkt,
                                    vmk_ByteCountSmall numBytes,
                                    vmk_PktHandle **copyPkt);

/*
 ***********************************************************************
 * vmk_PktPartialCopyWithHeadroom --                              */ /**
 *
 * \ingroup Pkt
 *
 * \brief Create a partial copy of the given packet with a private area
 *        and at leat the requested headroom in front of the frame.
 *
 * This function is identical to vmk_PktPartialCopy() except that the 
 * resulting frame will also contain at least the requested length of
 * headroom. The resulting packet may contain more headroom than
 * requested.
 *
 * \see   vmk_PktPartialCopy(), vmk_PktPushHeadroom()
 *
 * \param[in]   pkt         Target packet.
 * \param[in]   numBytes    Number of bytes in the private region of
 *                          the resulting packet.
 * \param[in]   headroomLen Number of bytes in the headroom of the
 *                          resulting packet.
 * \param[out]  copyPkt     Resulting packet with a private mapped
 *                          region and requested amount of headroom.
 *
 * \retval     VMK_OK       If the partial copy was successful.
 * \retval     VMK_FAILURE  Otherwise.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktPartialCopyWithHeadroom(vmk_PktHandle *pkt,
                                                vmk_ByteCountSmall numBytes,
                                                vmk_uint16 headroomLen,
                                                vmk_PktHandle **copyPkt);

/*
 ***********************************************************************
 * vmk_PktCopy --                                                 */ /**
 *
 * \ingroup Pkt
 * \brief Create a copy of the frame contents of the given packet.
 *
 * This function is identical to calling vmk_PktPartialCopy() with 
 * vmk_PktFrameLenGet() as the numBytes argument.
 *
 * \see   vmk_PktPartialCopy()
 *
 * \note  Every packet pushed into a module is considered read-only by 
 *        default. This function is a way to change this policy. In 
 *        copyPkt, the frame contents are guarenteed to be mapped and 
 *        physically contiguous.
 *
 * \note  pkt must have vmk_PktFrameLenGet() greater than 0.
 *
 * \param[in]   pkt        Source packet.
 * \param[out]  copyPkt    Resulting packet with a private mapped region.
 *
 * \retval     VMK_OK      If the partial copy was successful.
 * \retval     VMK_FAILURE Otherwise.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktCopy(vmk_PktHandle *pkt,
                             vmk_PktHandle **copyPkt);

/*
 ***********************************************************************
 * vmk_PktCopyWithHeadroom --                                     */ /**
 *
 * \ingroup Pkt
 * \brief Create a copy of the frame contents of the given packet and
 *        at least the requested headroom in front of the frame.
 *
 * This function is identical to calling vmk_PktPartialCopyWithHeadroom() 
 * with vmk_PktFrameLenGet() as the numBytes argument.
 *
 * The resuting packet will have at least the number of bytes of headroom
 * requested, and may also have additional headroom.
 *
 * \see   vmk_PktPartialCopyWithHeadroom()
 *
 * \note  Every packet pushed into a module is considered read-only by 
 *        default. This function is a way to change this policy. In 
 *        copyPkt, the frame contents are guarenteed to be mapped and 
 *        physically contiguous. 
 * 
 * \param[in]   pkt         Source packet.
 * \param[in]   headroomLen Number of bytes in the headroom of the
 *                          resulting packet.
 * \param[out]  copyPkt     Resulting packet with a private mapped
 *                          region and requested amount of headroom.
 *
 * \retval     VMK_OK       If the partial copy was successful.
 * \retval     VMK_FAILURE  Otherwise.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktCopyWithHeadroom(vmk_PktHandle *pkt,
                                         vmk_uint16 headroomLen,
                                         vmk_PktHandle **copyPkt);

/*
 ***********************************************************************
 * vmk_PktClone --                                                */ /**
 *
 * \ingroup Pkt
 * \brief Create a clone of the given packet.
 *
 * This function is identical to calling vmk_PktPartialCopy() with 0 as 
 * the numBytes argument. The resulting packet has no private region 
 * (hence is read-only) and refers to the all the buffers in the source 
 * packet.
 *
 * \see   vmk_PktPartialCopy()
 *
 * \note  The headroom length of the resulting clone packet is unspecified,
 *        it may or may not be the same as original.
 *
 * \param[in]   pkt         Source packet.
 * \param[out]  clonePkt    Resulting packet that is a read-only copy
 *                          of the source packet.
 *
 * \retval     VMK_OK       If the partial copy was successful.
 * \retval     VMK_FAILURE  Otherwise.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktClone(vmk_PktHandle *pkt,
                              vmk_PktHandle **clonePkt);

/*
 ***********************************************************************
 * vmk_PktGetHeadroomLen --                                       */ /**
 *
 * \ingroup Pkt
 * \brief Return the length of headroom in front of the frame contents
 *        of the given packet.
 *
 * \param[in]   pkt         Target packet.
 *
 * \retval      >=0         Length of the headroom.
 *
 ***********************************************************************
 */

vmk_uint16 vmk_PktGetHeadroomLen(vmk_PktHandle *pkt);

/*
 ***********************************************************************
 * vmk_PktPushHeadroom --                                         */ /**
 *
 * \ingroup Pkt
 * \brief Increase the size of the headroom in front of the frame 
 *        contents of the given packet.
 *
 * "Push" the start of frame pointer forward without changing the frame 
 * contents. After this operation the frame length is reduced by 
 * headroomLen and the frame mapped pointer is pushed forward by 
 * headroomLen.
 *
 * \param[in]   pkt         Target packet.
 * \param[in]   headroomLen Amount of bytes to add to the headroom.
 *
 * \retval      VMK_OK      The headroom has been enlarged properly.
 * \retval      VMK_FAILURE The requested headroomLen to push is greater
 *                          than either the frame length (vmk_PktFrameLenGet())
 *                          or the frame mapped length 
 *                          (vmk_PktFrameMappedLenGet())
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktPushHeadroom(vmk_PktHandle *pkt,
                                     vmk_uint16 headroomLen);

/*
 ***********************************************************************
 * vmk_PktPullHeadroom --                                         */ /**
 *
 * \ingroup Pkt
 * \brief Decrease the size of the headroom in front of the frame 
 *        contents of the given packet.
 *
 * \note As a result of decreasing the headroom space, the length of 
 *       the frame mapped area and the set of fragments will be modified.
 *       The frame length will also be updated as a result.
 *
 * \param[in]   pkt         Target packet.
 * \param[in]   headroomLen Amount of bytes to reduce from the headroom.
 *
 * \retval      VMK_OK      The headroom has been reduced properly.
 * \retval      VMK_FAILURE The requested decrease in headroomLen is
 *                          larger than the current available headroomLen
 *                          (vmk_PktGetHeadroomLen()).
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktPullHeadroom(vmk_PktHandle *pkt,
                                     vmk_uint16 headroomLen);

/*
 ***********************************************************************
 * vmk_PktDup --                                                  */ /**
 *
 * \ingroup Pkt
 * \brief Make a full, independent copy of the given vmk_PktHandle
 *
 * The following values are identical with the source packet in the 
 * resulting packet:
 *
 * - Frame contents (fragment layout will likely differ)
 * - Frame length
 * - Attributes copied via vmk_PktCopyMetaData(), ie. VLAN ID, Priority, 
 *   vmk_PktMustVlanTag() value, and all dynamic attributes set via
 *   vmk_PktAttrSet()
 * - vmk_PktIsMustCsum() value.
 * - Checksum verified flag.
 * - vmk_PktIsLargeTcpPacket() and vmk_PktGetLargeTcpPacketMss() values.
 *
 * \note Headroom amount is NOT retained in the resulting packet (see
 *       vmk_PktDupWithHeadroom() if that is required).
 *
 * \note It is legal to call this function with a source packet that is
 *       the result of a previous vmk_PktPartialCopy() family function call.
 *       The resulting packet will be fully modifiable
 *       (vmk_PktIsBufDescWritable() will be VMK_TRUE).
 *
 * \param[in]   pkt             Target packet.
 * \param[out]  dupPkt          Resulting duplicate packet.
 *
 * \retval      VMK_OK          If the duplication was successful.
 * \retval      VMK_BAD_PARAM   If either pkt or dupPkt is NULL.
 * \retval      VMK_NO_MEMORY   Not enough memory to satisfy request.
 * \retval      otherwise       Various other errors.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktDup(vmk_PktHandle *pkt,
                            vmk_PktHandle **dupPkt);

/*
 ***********************************************************************
 * vmk_PktDupAndCsum --                                           */ /**
 *
 * \ingroup Pkt
 * \brief Make a full, independent copy of the given vmk_PktHandle and
 *        perform checksumming on the frame.
 *
 * This function is identical to vmk_PktDup() except that the TCP/UDP 
 * checksum for the frame contents is computed and inserted into the 
 * frame.
 *
 * \see   vmk_PktDup()
 *
 * \pre   The frame contained by pkt must be a IPv4 or IPv6 TCP/UDP 
 *        frame.
 *
 * \pre   For checksum computation to be performed vmk_PktIsMustCsum()
 *        must be VMK_TRUE. Otherwise the function will have the same
 *        effect as if vmk_PktDup() was called.
 *
 * \param[in]   pkt              Target packet.
 * \param[out]  dupPkt           Resulting duplicate packet.
 *
 * \retval      VMK_OK           If the duplication was successful.
 * \retval      VMK_BAD_PARAM    If either pkt or dupPkt is NULL.
 * \retval      VMK_NO_MEMORY    Not enough memory to satisfy request.
 * \retval      otherwise        Various other errors.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktDupAndCsum(vmk_PktHandle *pkt,
                                   vmk_PktHandle **dupPkt);

/*
 ***********************************************************************
 * vmk_PktDupWithHeadroom --                                      */ /**
 *
 * \ingroup Pkt
 * \brief Make a full, independent copy of the given vmk_PktHandle and
 *        reserve at least headroomLen bytes in front of the frame.
 *
 * This function is identical to vmk_PktDup() except that the resulting 
 * packet will also have a headroom of the specified size added in front 
 * of the frame contents.
 *
 * The resulting packet will have at least as many bytes of headroom
 * requested, and may have more than the requested headroom length.
 *
 * \see   vmk_PktDup()
 *
 * \param[in]   pkt              Target packet.
 * \param[in]   headroomLen      Number of headroom bytes before the actual 
 *                               frame.
 * \param[out]  dupPkt           Resulting duplicate packet.
 *
 * \retval      VMK_OK           If the duplication was successful.
 * \retval      VMK_BAD_PARAM    If either pkt or dupPkt is NULL.
 * \retval      VMK_NO_MEMORY    Not enough memory to satisfy request.
 * \retval      otherwise        Various other errors.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktDupWithHeadroom(vmk_PktHandle *pkt,
                                        vmk_uint16 headroomLen,
                                        vmk_PktHandle **dupPkt);

/*
 ***********************************************************************
 * vmk_PktDupLenWithHeadroomAndTailroom --                        */ /**
 *
 * \ingroup Pkt
 * \brief Make a partial, independent copy of the given vmk_PktHandle
 *        with at least headroomLen bytes and at least tailroomLen bytes. 
 *
 * This function is similar to vmk_PktDupWithHeadroom(), except that it
 * copies only "dupLen" bytes of the frame data, and it reserves
 * tailroomLen bytes of extra space at the end of the copied data.
 *
 * The resulting packet will have at least as many bytes of headroom
 * requested, and may have more than the requested length.
 *
 * \param[in]   pkt              Original packet.
 * \param[in]   dupLen           Amount of bytes to duplicate.
 * \param[in]   headroomLen      Amount of headroom before the frame.
 * \param[in]   tailroomLen      Amount of tailroom to reserve beyond copied data.
 * \param[out]  dupPkt           Pointer to the resulting packet.
 *
 * \retval      VMK_OK           If the duplication was successful.
 * \retval      VMK_BAD_PARAM    If pkt or dupPkt is NULL, or dupLen is zero.
 * \retval      VMK_NO_MEMORY    Not enough memory to satisfy request.
 * \retval      otherwise        Various other errors.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktDupLenWithHeadroomAndTailroom(vmk_PktHandle *pkt,
                                                      vmk_ByteCountSmall dupLen,
                                                      vmk_uint16 headroomLen,
                                                      vmk_uint16 tailroomLen,
                                                      vmk_PktHandle **dupPkt);

/*
 ***********************************************************************
 * vmk_PktDupAndCsumWithHeadroom --                               */ /**
 *
 * \ingroup Pkt
 * \brief Makes a full, independent copy of the given vmk_PktHandle,
 *        performs checksumming and adds headroom in front of the
 *        frame.
 *        
 * This function is identical to vmk_PktDupAndCsum() except that the 
 * resulting packet will also have a headroom length of at least as many
 * bytes requested.  The resulting packet may have more than the
 * requested headroom.
 *
 * \see   vmk_PktDupAndCsum()
 *
 * \pre   The frame contained by pkt must be a IPv4 or IPv6 TCP/UDP 
 *        frame.
 *
 * \pre   For checksum computation to be performed vmk_PktIsMustCsum()
 *        must be VMK_TRUE. Otherwise the function will have the same
 *        effect as if vmk_PktDupWithHeadroom() was called.
 *
 * \param[in]   pkt              Target packet.
 * \param[in]   headroomLen      Number of headroom bytes before the actual 
 *                               frame.
 * \param[out]  dupPkt           Resulting duplicate packet.
 *
 * \retval      VMK_OK           If the duplication was successful.
 * \retval      VMK_BAD_PARAM    If either pkt or dupPkt is NULL.
 * \retval      VMK_NO_MEMORY    Unable to allocate memory for dup.
 * \retval      otherwise        Various other errors.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktDupAndCsumWithHeadroom(vmk_PktHandle *pkt,
                                               vmk_uint16 headroomLen,
                                               vmk_PktHandle **dupPkt);

/*
 ***********************************************************************
 * vmk_PktPullHdrIntoMappedSpace --                               */ /**
 *
 * \ingroup Pkt
 * \brief Pulls data from following fragments into the frame mapped region.
 *
 * This function copies hdrCopyLen bytes starting from the end of the 
 * frame mapped region (vmk_PktFrameMappedLenGet()) into the frame mapped 
 * region with an offset of targetOffset.
 *
 * This function is typically used when a new packet is being constructued. 
 * Space is reserved in the frame mapped region of the packet to contain 
 * a prefix of the packet. Fragments are then added to the packet. When
 * sufficient fragments have been added, the prefix portion of the packet
 * is copied into the frame mapped region, and the packet's fragment array
 * is adjusted to "excise" the copied portion. This function also takes care
 * of the case when the resulting packet is smaller than the original
 * reserved space, in which case more of the packet, including the tail-end
 * of the frame mapped region, is excised from the packet.
 *
 * \note  frame mapped length should be >= targetOffset + hdrCopyLen.
 *
 * \param[in]   pkt          Target packet.
 * \param[in]   targetOffset Offset in the frame mapped region to copy 
 *                           data into.
 * \param[out]  hdrCopyLen   Length of data to copy starting from frame
 *                           mapped length as the offset.
 *
 * \retval      VMK_OK      If successful.
 * \retval      VMK_FAILURE Otherwise.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktPullHdrIntoMappedSpace(vmk_PktHandle *pkt,
                                               vmk_ByteCountSmall targetOffset,
                                               vmk_ByteCountSmall hdrCopyLen);

/*
 ***********************************************************************
 * vmk_PktCopyBytesOut --                                         */ /**
 *
 * \ingroup Pkt
 * \brief Copies bytes from a packet into a contiguous local buffer.
 *
 * \param[in]   buffer       Buffer to copy pkt data to.
 * \param[in]   length       Amount of pkt data to copy.
 * \param[in]   offset       Pkt offset from which to begin copy.
 * \param[in]   pkt          Source packet.
 *
 * \retval      VMK_OK              If successful.
 * \retval      VMK_BAD_PARAM       If buffer or pkt pointer is NULL.
 * \retval      VMK_READ_ERROR      If length + offset is beyond the pkt's mem.
 * \retval      VMK_INVALID_ADDRESS If pkt frag MA is invalid.
 * \retval      VMK_FAILURE         Otherwise.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktCopyBytesOut(vmk_uint8 *buffer,
                                     vmk_ByteCount length,
                                     vmk_uint32 offset,
                                     vmk_PktHandle *pkt);
#endif
/** @} */
/** @} */
