/* **********************************************************
 * Copyright (C) 2012,2013 VMware, Inc.  All Rights Reserved. -- VMware Confidential
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * RDMA device interfaces                                         */ /**
 *
 * \addtogroup RDMA
 * @{
 *
 ***********************************************************************
 */

#ifndef _VMKAPI_RDMA_DEVICE_H_
#define _VMKAPI_RDMA_DEVICE_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */


/**
 * \brief Identifier for logical RDMA devices.
 */
#define VMK_RDMA_DEVICE_IDENTIFIER "com.vmware.rdma"

/*
 ***********************************************************************
 * vmk_RDMAModifyQPArgsValid --                                   */ /**
 *
 * \brief Checks if arguments to vmk_RDMAOpModifyQueuePair are valid.
 *
 * \param[in]  curState     Current state of QP.
 * \param[in]  nextState    Next state of QP.
 * \param[in]  type         Type of QP.
 * \param[in]  mask         QP attributes mask.
 *
 * \retval VMK_OK           On success.
 * \retval VMK_BAD_PARAM    Invalid memory region.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_RDMAModifyQPArgsValid(vmk_RDMAQueuePairState curState,
                          vmk_RDMAQueuePairState nextState,
                          vmk_RDMAQueuePairType  type,
                          vmk_RDMAQueuePairAttrMask mask);

/*
 ***********************************************************************
 * vmk_RDMAEventDispatch --                                       */ /**
 *
 * \brief Dispatches an event from RDMA device.
 *
 * This function is invoked by the device driver to dispatch any
 * asynchronous device event up to the RDMA stack. The RDMA stack
 * then notifies the listeners for events on this device by invoking
 * the registered event handlers.
 *
 * \note  This function may block.
 *
 * \param[in]  event      RDMA event to be dispatched.
 *
 * \retval VMK_OK         Event successfully dispatched.
 * \retval VMK_BAD_PARAM  Invalid event or error.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMAEventDispatch(vmk_RDMAEvent *event);

/*
 ***********************************************************************
 * vmk_RDMACacheFindExactPkey --                                  */ /**
 *
 * \brief Finds the index of specified pkey in the device cache.
 *
 * \note This function may block.
 *
 * \param[in]  device     Device whose cache is queried.
 * \param[in]  pkey       Pkey to find.
 * \param[out] index      Index of found pkey.
 *
 * \retval VMK_OK         Pkey successfully found.
 * \retval VMK_NOT_FOUND  Pkey not found.
 * \retval VMK_BAD_PARAM  Invalid device or attributes.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMACacheFindExactPkey(vmk_RDMADevice device,
                                            vmk_uint16 pkey,
                                            vmk_uint32 *index);

/*
 ***********************************************************************
 * vmk_RDMACacheGetCachedGid --                                   */ /**
 *
 * \brief Retrieves a cached gid.
 *
 *
 * \deprecated This function is deprecated and will be removed in a later
 *             release. Querying the GID is not supported unless the index
 *             was retrieved using vmk_RDMACMQueryBind. Please use
 *             vmk_RDMACMQueryBind to obtain the GID if the client
 *             registered a vmk_RDMACMID using vmk_RDMACMCreateID.
 *
 * \note This function may block.
 *
 * \param[in]  device     Device whose cache is queried.
 * \param[in]  index      Index to find.
 * \param[out] gid        Gid of cached entry.
 *
 * \retval VMK_OK         Gid successfully found.
 * \retval VMK_NOT_FOUND  Gid not found.
 * \retval VMK_BAD_PARAM  Invalid device or attributes.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMACacheGetCachedGid(vmk_RDMADevice device,
                                           vmk_uint32 index,
                                           vmk_RDMAGid *gid);

/*
 ***********************************************************************
 * vmk_RDMACacheGetCachedPkey --                                  */ /**
 *
 * \brief Retrieves a cached pkey.
 *
 * \note This function may block.
 *
 * \param[in]  device     Device whose cache is queried.
 * \param[in]  index      Index to find.
 * \param[out] pkey       Pkey of cached entry.
 *
 * \retval VMK_OK         Pkey successfully found.
 * \retval VMK_NOT_FOUND  Pkey not found.
 * \retval VMK_BAD_PARAM  Invalid device or attributes.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMACacheGetCachedPkey(vmk_RDMADevice device,
                                            vmk_uint32 index,
                                            vmk_uint16 *pkey);

/*
 ***********************************************************************
 * vmk_RDMAComplQueueComplete --                                  */ /**
 *
 * \brief Invokes the specified completion queue's completion handler.
 *
 * \note This function may block.
 *
 * \param[in]  cq         Completion queue handle.
 *
 * \retval VMK_OK         Completion handler successfully invoked.
 * \retval VMK_BAD_PARAM  Invalid completion queue specified.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMAComplQueueComplete(vmk_RDMAComplQueue cq);

/*
 ***********************************************************************
 * vmk_RDMAComplQueueEventDispatch --                             */ /**
 *
 * \brief Invokes the specified completion queue's event handler.
 *
 * \note This function may block.
 *
 * \param[in]  cq         Completion queue handle.
 * \param[in]  event      Event to dispatch.
 *
 * \retval VMK_OK         Event handler successfully invoked.
 * \retval VMK_BAD_PARAM  Invalid completion queue or event specified.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMAComplQueueEventDispatch(vmk_RDMAComplQueue cq,
                                                 vmk_RDMAEvent *event);

/*
 ***********************************************************************
 * vmk_RDMAQueuePairEventDispatch --                             */ /**
 *
 * \brief Invokes the specified queue pair's event handler.
 *
 * \note This function may block.
 *
 * \param[in]  qp         Queue pair handle.
 * \param[in]  event      Event to dispatch.
 *
 * \retval VMK_OK         Event handler successfully invoked.
 * \retval VMK_BAD_PARAM  Invalid queue pair or event specified.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMAQueuePairEventDispatch(vmk_RDMAQueuePair qp,
                                                vmk_RDMAEvent *event);

/*
 ***********************************************************************
 * vmk_RDMAUDHeaderInit --                                        */ /**
 *
 * \brief Initializes a RDMA UD header structure.
 *
 * \param[in]  payloadBytes      Byte count in payload.
 * \param[in]  lrhPresent        Is LRH present.
 * \param[in]  ethPresent        Is ETH present.
 * \param[in]  vlanPresent       Is VLAN present.
 * \param[in]  grhPresent        Is GRH  present.
 * \param[in]  immediatePresent  Is immediate present.
 * \param[out] header            RDMA UD header.
 *
 ***********************************************************************
 */
void vmk_RDMAUDHeaderInit(vmk_uint8 payloadBytes,
                          vmk_uint32 lrhPresent,
                          vmk_uint32 ethPresent,
                          vmk_uint32 vlanPresent,
                          vmk_uint32 grhPresent,
                          vmk_uint32 immediatePresent,
                          vmk_RDMAUDHeader *header);
/*
 ***********************************************************************
 * vmk_RDMAUDHeaderPack --                                        */ /**
 *
 * \brief Pack UD header struct into wire format.
 *
 * \param[in]  header   UD header to be packed.
 * \param[out] buf      Buffer into which header is packed.
 * \param[in]  bufSize  Buffer length.
 *
 * \retval len Total length of buffer.
 *
 ***********************************************************************
 */
vmk_uint32 vmk_RDMAUDHeaderPack(vmk_RDMAUDHeader *header,
                                void *buf, vmk_uint32 bufSize);

/*
 ***********************************************************************
 * vmk_RDMAUDHeaderUnpack --                                      */ /**
 *
 * \brief Unpack the UD header
 *
 * \param[in]   buf           Buffer from which header is unpacked.
 * \param[in]   bufSize       Buffer length.
 * \param[out]  header        Unpacked UD header.
 *
 * \retval      VMK_OK        Header unpacked successfully.
 * \retval      VMK_BAD_PARAM Unpacking the header failed.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMAUDHeaderUnpack(void *buf,
                                        vmk_uint32 bufSize,
                                        vmk_RDMAUDHeader *header);

/*
 ***********************************************************************
 * vmk_RDMACapRegister --                                         */ /**
 *
 * \brief Register a capability with the RDMA stack.
 *
 * \note  This callback may block
 *
 * \param[in] device      Device to register a capability for. 
 * \param[in] capIndex    Capability value in vmk_RDMACap. 
 * \param[in] capOps      Operation function table of this functionality.
 *                        It will be NULL if function table is not
 *                        required for this capability.
 *
 * \retval VMK_OK         On success.
 * \retval VMK_BAD_PARAM  Invalid parameters.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMACapRegister(vmk_RDMADevice device,
                                     vmk_RDMACap capIndex,
                                     vmk_AddrCookie capOps);

/*
 ***********************************************************************
 * vmk_RDMADevicePrivStatsLengthGet --                            */ /**
 *
 * \brief Returns the length of private statistics in bytes.
 *
 * \note  This callback may block
 *
 * \param[in]  device     Device whose private statistics length to
 *                        retrieve.
 * \param[out] length     Returns the lenth of private statistics
 *                        in bytes.
 *
 * \retval VMK_OK         On success.
 * \retval VMK_BAD_PARAM  Invalid parameters.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_RDMADevicePrivStatsLengthGet(vmk_RDMADevice device,
                                 vmk_ByteCount *length);

/*
 ***********************************************************************
 * vmk_RDMADevicePrivStatsGet --                                  */ /**
 *
 * \brief Returns the private statistics.
 *
 * \note  This callback may block
 *
 * \param[in]  device     Device whose statistics to retrieve. 
 * \param[out] statBuffer Returned private tatistics in the buffer.
 * \param[in]  length     Length of statBuffer.
 *
 * \retval VMK_OK         On success.
 * \retval VMK_BAD_PARAM  Invalid parameters.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_RDMADevicePrivStatsGet(vmk_RDMADevice device,
                           char *statBuffer,
                           vmk_ByteCount length);

/*
 ***********************************************************************
 * vmk_RDMAGenerateGid --                                         */ /**
 *
 * \brief Generates the gid for the provided properties.
 *
 * This function will generate the gid for the provided properties
 * using the appropriate gid generation scheme as per the system
 * configuration.  Gid generation can be done using the MAC-based
 * scheme or the IP-based scheme.  Depending on the OFED version of
 * the systems this ESX host is communicating with, one should be
 * chosen for the entire system.  This function will generate the gid
 * in line with the system's configuration.
 *
 * \note  This function will not register this gid with the device's
 *        table, it is only a helper routine to generate the gid
 *        values as necessary.
 *
 * \note  This callback will not block.
 *
 * \param[in]  props      Bind properties for the network interface.
 * \param[out] gid        Gid value associated with these properties.
 *
 * \retval VMK_OK         On success.
 * \retval VMK_BAD_PARAM  Invalid parameters.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_RDMAGenerateGid(vmk_RDMACMBindProps *props, // IN
                    vmk_RDMAGid *gid);          // OUT

#endif /* _VMKAPI_RDMA_DEVICE_H_ */
/** @} */
