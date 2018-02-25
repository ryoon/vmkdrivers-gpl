/* **********************************************************
 * Copyright (C) 2012,2013 VMware, Inc.  All Rights Reserved. -- VMware Confidential
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * RDMA client interfaces                                         */ /**
 *
 * \addtogroup RDMA
 * @{
 *
 ***********************************************************************
 */

#ifndef _VMKAPI_RDMA_CLIENT_H_
#define _VMKAPI_RDMA_CLIENT_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */


/**
 * \brief Invalid RDMA connection manager instance.
 */
#define VMK_RDMA_CM_ID_INVALID ((vmk_RDMACMID)NULL)

/**
 * \brief RDMA CM Notify Type.
 */
typedef enum vmk_RDMACMNotifyType {
   /**
    * \brief Notification that the bind properties have changed.
    *
    * Upon receipt of this notification the callback should call
    * vmk_RDMACMQueryBind() to retrieve updated properties associated with the
    * binding and may need to call vmk_RDMAQueryGid() to retrieve a new Gid
    * value.  Additional actions by this callback may be necessary to ensure
    * existing communication utilizing this binding continue without error.
    */
   VMK_RDMA_CM_NOTIFY_BIND_CHANGE,
   /**
    * \brief Notification that the binding has been removed.
    *
    * Upon receipt of this notification the callback should clean up any
    * resources associated with this binding and call vmk_RDMACMDestroyID().
    * Further operations using this connection will not succeed and a new
    * gid index may only be obtained by calling vmk_RDMACMCreateID() and
    * obtaining a new binding.
    */
   VMK_RDMA_CM_NOTIFY_BIND_REMOVE,
} vmk_RDMACMNotifyType;


/*
 ***********************************************************************
 * vmk_RDMACMNotifyCB --                                          */ /**
 *
 * \brief Handles notification for a connection manager.
 *
 * \note  This callback may block.
 *
 * \param[in]  cbData     Private data provided to
 *                        vmk_RDMACMCreateID().
 * \param[in]  type       Type of notification.
 * \param[in]  id         Connection manager id.
 *
 ***********************************************************************
 */
typedef void (*vmk_RDMACMNotifyCB)(vmk_AddrCookie cbData,
                                   vmk_RDMACMNotifyType type,
                                   vmk_RDMACMID id);


/*
 ***********************************************************************
 * vmk_RDMAClientRegister --                                      */ /**
 *
 * \brief Registers an RDMA client.
 *
 * \note The addDevice operation specified in the provided properties
 *       may be called before this function has returned.
 *
 * \note This function may block.
 *
 * \param[in]  props      Properties defining this client.
 * \param[out] client     Returned opaque client handle.
 *
 * \retval VMK_OK         Client successfully registered.
 * \retval VMK_BAD_PARAM  Invalid argument provided.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMAClientRegister(vmk_RDMAClientProps *props,
                                        vmk_RDMAClient *client);


/*
 ***********************************************************************
 * vmk_RDMAClientUnregister --                                    */ /**
 *
 * \brief Unregisters an RDMA client.
 *
 * \note The removeDevice operation specified in the provided properties
 *       may be called before this function has returned.
 *
 * \note This function may block.
 *
 * \param[in] client      Client handle to unregister.
 *
 ***********************************************************************
 */
void vmk_RDMAClientUnregister(vmk_RDMAClient client);


/*
 ***********************************************************************
 * vmk_RDMAClientAllocSize --                                     */ /**
 *
 * \brief Specifies memory required from client registration heap.
 *
 * This will provide the number of bytes required from the heap due
 * to a client registration.
 *
 * \note This function will not block.
 *
 ***********************************************************************
 */
vmk_ByteCount vmk_RDMAClientAllocSize(void);


/*
 ***********************************************************************
 * vmk_RDMAQueryDevice --                                         */ /**
 *
 * \brief Queries the device attributes.
 *
 * \note This function may block.
 *
 * \param[in]  device     Device whose attributes are queried.
 * \param[out] attr       Attribute structure to be filled in.
 *
 * \retval VMK_OK         Attributes successfully retrieved.
 * \retval VMK_BAD_PARAM  Invalid device or attributes.
 *
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMAQueryDevice(vmk_RDMADevice device,
                                     vmk_RDMADeviceAttr *attr);


/*
 ***********************************************************************
 * vmk_RDMAQueryPort --                                           */ /**
 *
 * \brief Queries RDMA device port attributes
 *
 * Queries the RDMA device's port and returns its port attributes.
 *
 * \note  This function may block.
 *
 * \param[in]     device     Device to query port attributes for.
 * \param[out]    portAttr   Port attributes for the device.
 *
 * \retval VMK_OK            Port attributes queried successfully.
 * \retval VMK_BAD_PARAM     Invalid arguments.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMAQueryPort(vmk_RDMADevice device,
                                   vmk_RDMAPortAttr *portAttr);


/*
 ***********************************************************************
 * vmk_RDMAQueryGid --                                            */ /**
 *
 * \brief Queries RDMA port GID
 *
 * Queries the RDMA device port and returns its GID.
 *
 * \deprecated This function is deprecated and will be removed in a later
 *             release. Querying the GID is not supported unless the index
 *             was retrieved using vmk_RDMACMQueryBind. Please use
 *             vmk_RDMACMQueryBind to obtain the GID if the client
 *             registered a vmk_RDMACMID using vmk_RDMACMCreateID.
 *
 * \note  This function may block.
 *
 * \param[in]     device     Device to query port GID for.
 * \param[in]     index      Index in the GID cache.
 * \param[out]    gid        Returned GID.
 *
 * \retval VMK_OK            Port GID queried successfully.
 * \retval VMK_BAD_PARAM     Invalid arguments.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMAQueryGid(vmk_RDMADevice device,
                                  vmk_uint32 index,
                                  vmk_RDMAGid *gid);


/*
 ***********************************************************************
 * vmk_RDMAQueryPkey --                                            */ /**
 *
 * \brief Queries RDMA port pkey
 *
 * Queries the RDMA device port and returns its pkey.
 *
 * \note  This function may block,
 *
 * \param[in]     device     Device to query port pkey for.
 * \param[in]     index      Index in the pkey cache.
 * \param[out]    pkey       Returned pkey.
 *
 * \retval VMK_OK            Port pkey queried successfully.
 * \retval VMK_BAD_PARAM     Invalid arguments.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMAQueryPkey(vmk_RDMADevice device,
                                   vmk_uint16 index,
                                   vmk_uint16 *pkey);
/*
 ***********************************************************************
 * vmk_RDMAEventHandlerRegister --                                */ /**
 *
 * \brief Registers an event handler.
 *
 * \note  This function may block.
 *
 * \param[in]  device  Device for which event handler is registered.
 * \param[in]  props   Properties defining this event handler.
 * \param[out] handler Returned Opaque Event Handler handle.
 *
 * \retval VMK_OK         Event Handler successfully registered.
 * \retval VMK_BAD_PARAM  Invalid device or props.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMAEventHandlerRegister(vmk_RDMADevice device,
                                              vmk_RDMAEventHandlerProps *props,
                                              vmk_RDMAEventHandler *handler);


/*
 ***********************************************************************
 * vmk_RDMAEventHandlerUnregister --                              */ /**
 *
 * \brief Unregisters an event handler.
 *
 * \note  This function may block.
 *
 * \param[in] handler  Event Handler handle to be unregistered.
 *
 * \retval VMK_OK         Event Handler successfully unregistered.
 * \retval VMK_BAD_PARAM  Invalid handler.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMAEventHandlerUnregister(vmk_RDMAEventHandler handler);


/*
 ***********************************************************************
 * vmk_RDMAEventHandlerAllocSize --                               */ /**
 *
 * \brief Specifies memory required from event handler registration heap.
 *
 * This will provide the number of bytes required from the heap due
 * to an event handler registration.
 *
 * \note This function will not block.
 *
 * \retval vmk_ByteCount  Number of bytes required.
 *
 ***********************************************************************
 */
vmk_ByteCount vmk_RDMAEventHandlerAllocSize(void);


/*
 ***********************************************************************
 * vmk_RDMACreateAddrHandle --                                    */ /**
 *
 * \brief Creates an address handle.
 *
 * \note  This function may block.
 *
 * \param[in]  props   Properties defining this address handle.
 * \param[out] ah      Returned Opaque address handle handle.
 *
 * \retval VMK_OK         Address handle successfully created.
 * \retval VMK_BAD_PARAM  Invalid properties.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMACreateAddrHandle(vmk_RDMAAddrHandleProps *props,
                                          vmk_RDMAAddrHandle *ah);

/*
 ***********************************************************************
 * vmk_RDMAQueryAddrHandle --                                     */ /**
 *
 * \brief Queries an address handle's attributes.
 *
 * \note  This function may block.
 *
 * \param[in]  ah      Address handle to query.
 * \param[out] attr    Attributes for this address handle.
 *
 * \retval VMK_OK         Address handle successfully queried.
 * \retval VMK_BAD_PARAM  Invalid device or address handle.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMAQueryAddrHandle(vmk_RDMAAddrHandle ah,
                                         vmk_RDMAAddrHandleAttr *attr);


/*
 ***********************************************************************
 * vmk_RDMADestroyAddrHandle --                                   */ /**
 *
 * \brief Destroys an address handle.
 *
 * \note  This function may block.
 *
 * \param[in]  ah      Address handle to destroy.
 *
 * \retval VMK_OK         Address handle successfully destroyed.
 * \retval VMK_BAD_PARAM  Invalid device or address handle.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMADestroyAddrHandle(vmk_RDMAAddrHandle ah);


/*
 ***********************************************************************
 * vmk_RDMAAddrHandleAllocSize --                                 */ /**
 *
 * \brief Specifies memory required from address handle creation heap.
 *
 * This will provide the number of bytes required from the heap due
 * to an address handle creation.
 *
 * \note This function will not block.
 *
 * \retval vmk_ByteCount  Number of bytes required.
 *
 ***********************************************************************
 */
vmk_ByteCount vmk_RDMAAddrHandleAllocSize(void);


/*
 ***********************************************************************
 * vmk_RDMAAllocProtDomain --                                     */ /**
 *
 * \brief Allocates a protection domain.
 *
 * \note  This function may block.
 *
 * \param[in]  props   Properties for protection domain allocation.
 * \param[out] pd      Returned opaque protection domain handle.
 *
 * \retval VMK_OK         Protection domain successfully created.
 * \retval VMK_BAD_PARAM  Invalid device or heap.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMAAllocProtDomain(vmk_RDMAProtDomainProps *props,
                                         vmk_RDMAProtDomain *pd);


/*
 ***********************************************************************
 * vmk_RDMAFreeProtDomain --                                      */ /**
 *
 * \brief Frees a protection domain.
 *
 * \note  This function may block.
 *
 * \param[in]  pd      Protection domain to free.
 *
 * \retval VMK_OK         Protection domain successfully freed.
 * \retval VMK_BAD_PARAM  Invalid device or protection domain.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMAFreeProtDomain(vmk_RDMAProtDomain pd);


/*
 ***********************************************************************
 * vmk_RDMAProtDomainAllocSize --                                 */ /**
 *
 * \brief Specifies memory required from protection domain heap.
 *
 * This will provide the number of bytes required from the heap due
 * to allocation of a protection domain.
 *
 * \note This function will not block.
 *
 * \retval vmk_ByteCount  Number of bytes required.
 *
 ***********************************************************************
 */
vmk_ByteCount vmk_RDMAProtDomainAllocSize(void);


/*
 ***********************************************************************
 * vmk_RDMACreateQueuePair --                                    */ /**
 *
 * \brief Creates a queue pair.
 *
 * \note  This function may block.
 *
 * \param[in]  qpProps    Properties for queue pair creation.
 * \param[out] qp         Returned opaque queue pair handle.
 *
 * \retval VMK_OK         Queue pair successfully created.
 * \retval VMK_BAD_PARAM  Invalid properties or queue pair.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMACreateQueuePair(vmk_RDMAQueuePairCreateProps *qpProps,
                                         vmk_RDMAQueuePair *qp);


/*
 ***********************************************************************
 * vmk_RDMAModifyQueuePair --                                     */ /**
 *
 * \brief Modifies a queue pair.
 *
 * \note  This function may block.
 *
 * \param[in]  qp         Queue pair to be modified.
 * \param[in]  attr       Attributes for queue pair to be modified.
 * \param[in]  qpAttrMask Queue pair attribute mask.
 *
 * \retval VMK_OK         Queue pair successfully modified.
 * \retval VMK_BAD_PARAM  Invalid properties or queue pair.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMAModifyQueuePair(vmk_RDMAQueuePair qp,
                                         vmk_RDMAQueuePairAttr *attr,
                                         vmk_RDMAQueuePairAttrMask qpAttrMask);


/*
 ***********************************************************************
 * vmk_RDMAQueryQueuePair --                                      */ /**
 *
 * \brief Queries a queue pair.
 *
 * \note  This function may block.
 *
 * \param[in]  qp         Queue pair to be modified.
 * \param[out] attr       Attributes for queue pair.
 * \param[in]  qpAttrMask Queue pair attribute mask.
 * \param[out] initAttr   Initialization attributes for queue pair.
 *
 * \retval VMK_OK         Queue pair attributes successfully queried.
 * \retval VMK_BAD_PARAM  Invalid arguments or queue pair.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMAQueryQueuePair(vmk_RDMAQueuePair qp,
                                        vmk_RDMAQueuePairAttr *attr,
                                        vmk_RDMAQueuePairAttrMask qpAttrMask,
                                        vmk_RDMAQueuePairInitAttr *initAttr);


/*
 ***********************************************************************
 * vmk_RDMADestroyQueuePair --                                   */ /**
 *
 * \brief Destroys the provided queue pair.
 *
 * \note  This function may block.
 *
 * \param[in] qp          Queue pair to destroy.
 *
 * \retval VMK_OK         Queue pair successfully destroyed.
 * \retval VMK_BAD_PARAM  Invalid queue pair.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMADestroyQueuePair(vmk_RDMAQueuePair qp);


/*
 ***********************************************************************
 * vmk_RDMAQueuePairAllocSize --                                 */ /**
 *
 * \brief Specifies memory required from queue pair heap.
 *
 * This will provide the number of bytes required from the heap due
 * to creation of a queue pair.
 *
 * \note This function will not block.
 *
 * \retval vmk_ByteCount  Number of bytes required.
 *
 ***********************************************************************
 */
vmk_ByteCount vmk_RDMAQueuePairAllocSize(void);


/*
 ***********************************************************************
 * vmk_RDMACreateComplQueue --                                    */ /**
 *
 * \brief Creates a completion queue.
 *
 * \note  This function may block.
 *
 * \param[in]  props      Properties for completion queue creation.
 * \param[out] cq         Returned opaque completion queue handle.
 *
 * \retval VMK_OK         Completion queue successfully created.
 * \retval VMK_BAD_PARAM  Invalid properties or completion queue.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMACreateComplQueue(vmk_RDMAComplQueueCreateProps *props,
                                          vmk_RDMAComplQueue *cq);


/*
 ***********************************************************************
 * vmk_RDMADestroyComplQueue --                                   */ /**
 *
 * \brief Destroys the provided completion queue.
 *
 * \note  This function may block.
 *
 * \param[in] cq          Completion queue to destroy.
 *
 * \retval VMK_OK         Completion queue successfully destroyed.
 * \retval VMK_BAD_PARAM  Invalid completion queue.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMADestroyComplQueue(vmk_RDMAComplQueue cq);


/*
 ***********************************************************************
 * vmk_RDMAPollComplQueue --                                      */ /**
 *
 * \brief Polls the specified completion queue.
 *
 * \note  This function may block.
 *
 * \param[in]     cq       Completion queue to poll.
 * \param[in,out] entries  The number of entries wc has room to hold
 *                         and when VMK_OK is returned, the number
 *                         of entries placed in to wc.
 * \param[in]     wc       Work completion array containing room for
 *                         at least the specified number of entries.
 *
 * \retval VMK_OK         Completion queue successfully polled.
 * \retval VMK_BAD_PARAM  Invalid arguments.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMAPollComplQueue(vmk_RDMAComplQueue cq,
                                        vmk_uint32 *entries,
                                        vmk_RDMAWorkCompl *wc);


/*
 ***********************************************************************
 * vmk_RDMAReqNotifyComplQueue --                                 */ /**
 *
 * \brief Requests notification from the specified completion queue.
 *
 * \note  This function may block.
 *
 * \param[in]  cq            Completion queue handle.
 * \param[in]  flags         Completion queue flags.
 * \param[out] missedEvents  Optional argument expected only if flags
 *                           contains
 *                           VMK_RDMA_CQ_FLAGS_REPORT_MISSED_EVENTS.
 *                           If that flag is provided, missedEvents will
 *                           be set to VMK_TRUE if it is possible that
 *                           an event was missed due to a race between
 *                           notification request and completion queue
 *                           entry.  In this case the caller should poll
 *                           the completion queue again to ensure it is
 *                           empty.  If this argument is set to
 *                           VMK_FALSE, no events were missed.
 *
 * \retval VMK_OK            Request issued successfully.
 * \retval VMK_BAD_PARAM     Invalid arguments.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMAReqNotifyComplQueue(vmk_RDMAComplQueue cq,
                                             vmk_RDMAComplQueueFlags flags,
                                             vmk_Bool *missedEvents);


/*
 ***********************************************************************
 * vmk_RDMAComplQueueAllocSize --                                 */ /**
 *
 * \brief Specifies memory required from completion queue heap.
 *
 * This will provide the number of bytes required from the heap due
 * to creation of a completion queue.
 *
 * \note This function will not block.
 *
 * \retval vmk_ByteCount  Number of bytes required.
 *
 ***********************************************************************
 */
vmk_ByteCount vmk_RDMAComplQueueAllocSize(void);


/*
 ***********************************************************************
 * vmk_RDMAPostSend --                                            */ /**
 *
 * \brief Posts a work request on the specified queue pair's send queue.
 *
 * \note This function may block.
 *
 * \param[in]  device     Device on which to post send.
 * \param[in]  qp         Queue pair used to send work request.
 * \param[in]  sendWr     Work request to be posted on send queue.
 * \param[out] badSendWr  Erroneous Work Request.  If set will point
 *                        to a work request in list rooted at sendWr.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMAPostSend(vmk_RDMADevice device,
                                  vmk_RDMAQueuePair qp,
                                  vmk_RDMASendWorkRequest *sendWr,
                                  vmk_RDMASendWorkRequest **badSendWr);


/*
 ***********************************************************************
 * vmk_RDMAPostRecv --                                            */ /**
 *
 * \brief Posts a work request on the queue pair's receive queue.
 *
 * \note This function may block.
 *
 * \param[in]  device     Device on which to post receive.
 * \param[in]  qp         Queue pair used to receive work request.
 * \param[in]  recvWr     Work request to be posted on receive queue.
 * \param[out] badRecvWr  Erroneous Work Request.  If set will point
 *                        to a work request in list rooted at recvWr.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMAPostRecv(vmk_RDMADevice device,
                                  vmk_RDMAQueuePair qp,
                                  vmk_RDMARecvWorkRequest *recvWr,
                                  vmk_RDMARecvWorkRequest **badRecvWr);


/*
 ***********************************************************************
 * vmk_RDMAGetDmaMemRegion --                                     */ /**
 *
 * \brief Creates mapped Memory Region.
 *
 * \note  This function may block.
 *
 * \param[in]  props      Memory region properties.
 * \param[out] mr         Created memory region.
 *
 * \retval VMK_OK         Memory region successfully retrieved.
 * \retval VMK_BAD_PARAM  Invalid protection domain or flags.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMAGetDmaMemRegion(vmk_RDMAMemRegionProps *props,
                                         vmk_RDMAMemRegion *mr);


/*
 ***********************************************************************
 * vmk_RDMAQueryMemRegion --                                     */ /**
 *
 * \brief Query a memory region's attributes.
 *
 * \note  This function may block.
 *
 * \param[in]  mr         Pointer to memory region.
 * \param[out] attr       Queried memory region's attributes.
 *
 * \retval VMK_OK         Memory region's attributes successfully
 *                        retrieved.
 * \retval VMK_BAD_PARAM  Invalid memory region.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMAQueryMemRegion(vmk_RDMAMemRegion mr,
                                        vmk_RDMAMemRegionAttr *attr);


/*
 ***********************************************************************
 * vmk_RDMADeregMemRegion --                                      */ /**
 *
 * \brief Deregisters memory region.
 *
 * \note  This function may block.
 *
 * \param[in]  mr         Pointer to memory region to be deregistered.
 *
 * \retval VMK_OK         On success.
 * \retval VMK_BAD_PARAM  Invalid memory region.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMADeregMemRegion(vmk_RDMAMemRegion mr);


/*
 ***********************************************************************
 * vmk_RDMADMAMapSg --                                            */ /**
 *
 * \brief Map machine memory in an SG array to IO addresses.
 *
 * This call will attempt to map a machine-address SG array and create
 * a new IO-address SG array element.
 *
 * \note The input SG array must not be freed or modified while it
 *       is mapped or the results are undefined.
 *
 * \note If an SG array is simultaneously mapped with multiple DMA
 *       directions, the contents of the memory the SG array represents
 *       are undefined.
 *
 * \note This function will not block.
 *
 * \param[in]  device      RDMA device for DMA operation.
 * \param[in]  direction   Direction of the DMA transfer for the
 *                         mapping.
 * \param[in]  coherent    Whether this mapping should be coherent.
 * \param[in]  sgOps       Scatter-gather ops handle used to allocate
 *                         the new SG array for the mapping.
 * \param[in]  in          SG array containing machine addresses to
 *                         map for the DMA engine.
 * \param[out] out         New SG array containing mapped IO addresses.
 *                         Note that this may be the same SG array as
 *                         the array passed in depending on choices
 *                         made by the kernel's mapping code.
 * \param[out] err         If this call fails with
 *                         VMK_DMA_MAPPING_FAILED, additional
 *                         information about the failure may be found
 *                         here. This may be set to NULL if the
 *                         information is not desired.
 *
 * \retval VMK_BAD_PARAM            The specified RDMA device is
 *                                  invalid.
 * \retval VMK_DMA_MAPPING_FAILED   The mapping failed because the
 *                                  DMA constraints could not be met.
 *                                  Additional information about the
 *                                  failure can be found in the "err"
 *                                  argument.
 * \retval VMK_NO_MEMORY            There is currently insufficient
 *                                  memory available to construct the
 *                                  mapping.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMADMAMapSg(vmk_RDMADevice device,
                                  vmk_DMADirection direction,
                                  vmk_Bool coherent,
                                  vmk_SgOpsHandle sgOps,
                                  vmk_SgArray *in,
                                  vmk_SgArray **out,
                                  vmk_DMAMapErrorInfo *err);

/*
 ***********************************************************************
 * vmk_RDMADMAFlushSg --                                          */ /**
 *
 * \brief Synchronize a DMA mapping specified in a SG array.
 *
 * This call is used to synchronize data if the CPU needs to read or
 * write after an DMA mapping is active on a region of machine memory
 * but before the DMA mapping is unmapped.
 *
 * If the specified memory is DMA-mapped this call must be invoked
 * with VMK_DMA_DIRECTION_FROM_MEMORY after CPU writes are complete but
 * before any new DMA read transactions occur on the memory.
 *
 * If the specified memory is DMA-mapped this call must be invoked
 * with VMK_DMA_DIRECTION_TO_MEMORY before CPU reads but after
 * any write DMA transactions complete on the memory.
 *
 * DMA map and unmap calls will implicitly perform a flush of the entire
 * SG array.
 *
 * The code may flush bytes rounded up to the nearest page or other
 * HW-imposed increment.
 *
 * \note The sg array supplied to this function must be an array output
 *       from vmk_RDMADMAMapSG or the results of this call are
 *       undefined.
 *
 * \note This function will not block.
 *
 * \param[in] device      RDMA device for DMA operation.
 * \param[in] direction   Direction of the DMA transfer for the
 *                        mapping.
 * \param[in] coherent    Whether this mapping is coherent.
 * \param[in] sg          Scatter-gather array containing the
 *                        IO-address ranges to flush.
 * \param[in] offset      Offset into the buffer the SG array
 *                        represents.
 * \param[in] len         Number of bytes to flush or
 *                        VMK_DMA_FLUSH_SG_ALL to flush the entire SG
 *                        array starting from the offset.
 *
 *
 * \retval VMK_BAD_PARAM         Unknown duration or direction, or
 *                               unsupported direction.
 * \retval VMK_INVALID_ADDRESS   Memory in the specified scatter-gather
 *                               array is not mapped.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMADMAFlushSg(vmk_RDMADevice device,
                                    vmk_DMADirection direction,
                                    vmk_Bool coherent,
                                    vmk_SgArray *sg,
                                    vmk_ByteCount offset,
                                    vmk_ByteCount len);

/*
 ***********************************************************************
 * vmk_RDMADMAUnmapSg --                                          */ /**
 *
 * \brief Unmaps previously mapped IO addresses from an SG array.
 *
 * \note The direction must match the direction at the time of mapping
 *       or the results of this call are undefined.
 *
 * \note The sg array supplied to this function must be one mapped with
 *       vmk_RDMADMAMapSG or the results of this call are undefined.
 *
 * \note This function will not block.
 *
 * \param[in] device      RDMA device for DMA operation.
 * \param[in] direction   Direction of the DMA transfer for the
 *                        mapping.
 * \param[in] coherent    Whether this mapping is coherent.
 * \param[in] sg          Scatter-gather array containing the
 *                        IO address ranges to unmap.
 * \param[in] sgOps       Scatter-gather ops handle used to free
 *                        the SG array if necessary.
 *
 * \retval VMK_BAD_PARAM         Unknown direction, or unsupported
 *                               direction.
 * \retval VMK_INVALID_ADDRESS   One ore more pages in the specified
 *                               machine address range are not mapped.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMADMAUnmapSg(vmk_RDMADevice device,
                                    vmk_DMADirection direction,
                                    vmk_Bool coherent,
                                    vmk_SgOpsHandle sgOps,
                                    vmk_SgArray *sg);

/*
 ***********************************************************************
 * vmk_RDMADMAMapElem --                                          */ /**
 *
 * \brief Map machine memory from single SG element to IO addresses.
 *
 * This call will attempt to map a single machine-address range and
 * create a new IO-address address range that maps to it.
 *
 * \note The input range must not be freed or modified while it
 *       is mapped or the results are undefined.
 *
 * \note If the range is simultaneously mapped with multiple DMA
 *       directions, the contents of the memory the SG array represents
 *       are undefined.
 *
 * \note This function will not block.
 *
 * \param[in]  device      RDMA device for DMA operation.
 * \param[in]  direction   Direction of the DMA transfer for the
 *                         mapping.
 * \param[in]  coherent    Whether this mapping should be coherent.
 * \param[in]  in          A single SG array element containing a single
 *                         machine addresse range to map for the
 *                         DMA engine.
 * \param[in]  lastElem    Indicates if this is the last element in
 *                         the transfer.
 * \param[out] out         A single SG array element to hold the mapped
 *                         IO address for the range.
 * \param[out] err         If this call fails with
 *                         VMK_DMA_MAPPING_FAILED, additional
 *                         information about the failure may be found
 *                         here. This may be set to NULL if the
 *                         information is not desired.
 *
 * \retval VMK_BAD_PARAM            The specified RDMA device is
 *                                  invalid.
 * \retval VMK_DMA_MAPPING_FAILED   The mapping failed because the
 *                                  DMA constraints could not be met.
 *                                  Additional information about the
 *                                  failure can be found in the "err"
 *                                  argument.
 * \retval VMK_NO_MEMORY            There is currently insufficient
 *                                  memory available to construct the
 *                                  mapping.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMADMAMapElem(vmk_RDMADevice device,
                                    vmk_DMADirection direction,
                                    vmk_Bool coherent,
                                    vmk_SgElem *in,
                                    vmk_Bool   lastElem,
                                    vmk_SgElem *out,
                                    vmk_DMAMapErrorInfo *err);

/*
 ***********************************************************************
 * vmk_RDMADMAFlushElem --                                        */ /**
 *
 * \brief Synchronize a DMA mapping for a single IO address range.
 *
 * This call is used to synchronize data if the CPU needs to read or
 * write after an DMA mapping is active on a region of machine memory
 * but before the DMA mapping is unmapped.
 *
 * If the specified memory is DMA-mapped this call must be invoked
 * with VMK_DMA_DIRECTION_FROM_MEMORY after CPU writes are complete but
 * before any new DMA read transactions occur on the memory.
 *
 * If the specified memory is DMA-mapped this call must be invoked
 * with VMK_DMA_DIRECTION_TO_MEMORY before CPU reads but after
 * any write DMA transactions complete on the memory.
 *
 * DMA map and unmap calls will implicitly perform a flush of the
 * element.
 *
 * The code may flush bytes rounded up to the nearest page or other
 * HW-imposed increment.
 *
 * \note The IO element supplied to this function must be an element
 *       output from vmk_RDMADMAMapElem or the results of this call
 *       are undefined.
 *
 *       Do not use this to flush a single element in an SG array
 *       that was mapped by vmk_RDMADMAMapSg.
 *
 * \note The original element supplied to this function must be
 *       the one supplied to vmk_RDMADMAMapElem when the IO element
 *       was created or the results of this call are undefined.
 *
 * \note This function will not block.
 *
 * \param[in] device      RDMA device for DMA operation.
 * \param[in] direction   Direction of the DMA transfer for the
 *                        mapping.
 * \param[in] coherent    Whether this mapping is coherent.
 * \param[in] IOElem      Scatter-gather element contained the
 *                        IO-address range to flush.
 *
 * \retval VMK_BAD_PARAM         Unknown duration or direction, or
 *                               unsupported direction.
 * \retval VMK_INVALID_ADDRESS   Memory in the specified element
 *                               is not mapped.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMADMAFlushElem(vmk_RDMADevice device,
                                      vmk_DMADirection direction,
                                      vmk_Bool coherent,
                                      vmk_SgElem *IOElem);

/*
 ***********************************************************************
 * vmk_RDMADMAUnmapElem --                                        */ /**
 *
 * \brief Unmaps previously mapped IO address range.
 *
 * \note The direction must match the direction at the time of mapping
 *       or the results of this call are undefined.
 *
 * \note The element supplied to this function must be one mapped with
 *       vmk_RDMADMAMapElem or the results of this call are undefined.
 *
 * \note This function will not block.
 *
 * \param[in] device      RDMA device for DMA operation.
 * \param[in] direction   Direction of the DMA transfer for the
 *                        mapping.
 * \param[in] coherent    Whether this mapping is coherent.
 * \param[in] IOElem      Scatter-gather element contained the
 *                        IO-address range to unmap.
 *
 * \retval VMK_BAD_PARAM         Unknown direction, or unsupported
 *                               direction.
 * \retval VMK_INVALID_ADDRESS   One ore more pages in the specified
 *                               machine address range are not mapped.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMADMAUnmapElem(vmk_RDMADevice device,
                                      vmk_DMADirection direction,
                                      vmk_Bool coherent,
                                      vmk_SgElem *IOElem);

/*
 ***********************************************************************
 * vmk_RDMAGetLinkLayer --                                        */ /**
 *
 * \brief Gets the link layer for the specified device.
 *
 * \note  This function may block.
 *
 * \param[in]  device     Device to get link layer for.
 * \param[out] link       Link layer of device.
 *
 * \retval VMK_OK         On success.
 * \retval VMK_BAD_PARAM  Invalid device.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMAGetLinkLayer(vmk_RDMADevice device,
                                      vmk_RDMALinkLayer *link);

/*
 ***********************************************************************
 * vmk_RDMAGetStats --                                            */ /**
 *
 * \brief Gets the statistics for specified device.
 *
 * \note  This function may block.
 *
 * \param[in]  device     Device to get statistics for.
 * \param[out] stats      Statistics of device.
 *
 * \retval VMK_OK         On success.
 * \retval VMK_BAD_PARAM  Invalid device.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMAGetStats(vmk_RDMADevice device,
                                  vmk_RDMAStats *stats);


/*
 ***********************************************************************
 * vmk_RDMADeviceGetAlias --                                      */ /**
 *
 * \brief Gets the alias for the specified device.
 *
 * \note  This function may block.
 *
 * \param[in]  device     Device to get alias of.
 * \param[in]  alias      Name structure to fill in with alias.
 *
 * \retval VMK_OK         On success.
 * \retval VMK_BAD_PARAM  Invalid device.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMADeviceGetAlias(vmk_RDMADevice device,
                                        vmk_Name *alias);

/*
 ***********************************************************************
 * vmk_RDMACMCreateIDAllocSize --                                 */ /**
 *
 * \brief Specifies memory required for connection manager creation.
 *
 * Provides the number of bytes allocated and the alignment of that
 * allocation for each call to vmk_RDMACMCreateID().  The heap id
 * provided to that function will be used for allocations.
 *
 * \allocsizenote
 *
 * \note This function will not block.
 *
 * \param[out] alignment  Required pointer to where alignment value
 *                        is written.
 *
 * \retval vmk_ByteCount  Number of bytes allocated from heap.
 *
 ***********************************************************************
 */
vmk_ByteCount vmk_RDMACMCreateIDAllocSize(vmk_ByteCount *alignment);


/*
 ***********************************************************************
 * vmk_RDMACMCreateID --                                          */ /**
 *
 * \brief Create a connection manager instance.
 *
 * Creates a connection manager identifier for use with other connection
 * management functions.  The caller must invoke vmk_RDMACMDestroyID()
 * when done with the connection manager instance.
 *
 * \note  Currently there are interfaces to bind to a local device but
 *        no address resolution or connection management facilities.
 *
 * \note  This function may block.
 *
 * \param[in]  client     Client creating this instance.
 * \param[in]  heapID     Heap to perform connection manager allocations
 *                        from.
 * \param[in]  cb         Callback that receives notifications of
 *                        changes affecting this connection manager
 *                        instance.
 * \param[in]  cbData     Private data to pass to callback.
 * \param[out] id         Connection manager id.
 *
 * \retval VMK_OK         On success.
 * \retval VMK_BAD_PARAM  Invalid parameter.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMACMCreateID(vmk_RDMAClient client,
                                    vmk_HeapID heapID,
                                    vmk_RDMACMNotifyCB cb,
                                    vmk_AddrCookie cbData,
                                    vmk_RDMACMID *id);

/*
 ***********************************************************************
 * vmk_RDMACMBind --                                              */ /**
 *
 * \brief Establishes a binding with a specific IP address.
 *
 * Establishes a binding on the specified IP address and network
 * stack instance name.  The vmk_RDMADevice associated with this address
 * and its gid index from the binding can be retrieved by calling
 * vmk_RDMACMQueryBind() once this function has returned success.
 *
 * \note  The binding established by this function will be removed
 *        when the connection manager instance is destroyed.
 *
 * \note  This function may block.
 *
 * \param[in]  id         Connection manager id.
 * \param[in]  srcAddr    Socket address to bind to.
 * \param[in]  instance   Name of network stack instance to use.  This
 *                        is optional and if NULL the default network
 *                        stack instance will be used.
 *
 * \retval VMK_OK             On success.
 * \retval VMK_EXISTS         There is already a binding for this
 *                            connection manager instance.
 * \retval VMK_NOT_SUPPORTED  Device does not support operations
 *                            necessary to perform binding.
 * \retval VMK_BAD_PARAM      Invalid parameter.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMACMBind(vmk_RDMACMID id,
                                vmk_SocketAddress *srcAddr,
                                vmk_Name *instance);

/*
 ***********************************************************************
 * vmk_RDMACMQueryBind --                                         */ /**
 *
 * \brief Queries bind information for an existing binding.
 *
 * Queries the bind properties, gid index, and device for the binding
 * established using vmk_RDMACMBind().
 *
 * The gid index is useful for obtaining the gid value for the binding
 * by a subsequent call to vmk_RDMAQueryGid().
 *
 * \note  This function may block.
 *
 * \param[in]  id         Connection manager id.
 * \param[out] device     Optional device filled in on success.
 * \param[out] gidIndex   Optional gid index filled in on success.
 * \param[out] props      Optional bind properties structure to be
 *                        filled in on success.
 *
 * \retval VMK_OK         On success.
 * \retval VMK_NOT_FOUND  No binding exists for this connection
 *                        manager instance.
 * \retval VMK_BAD_PARAM  Invalid parameter.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMACMQueryBind(vmk_RDMACMID id,
                                     vmk_RDMADevice *device,
                                     vmk_uint32 *gidIndex,
                                     vmk_RDMACMBindProps *props);

/*
 ***********************************************************************
 * vmk_RDMACMGetMTU --                                            */ /**
 *
 * \brief Returns the RDMA MTU for a given connection manager id.
 *
 * Queries the RDMA MTU that clients should pass into
 * \ref vmk_RDMAModifyQueuePair for a given CM id. The function
 * accounts for the overhead of all the necessary RDMA headers.
 *
 * \note  This function may block.
 *
 * \param[in]  id             Connection manager id.
 * \param[out] mtu            RDMA mtu associated with this CM id.
 *
 * \retval     VMK_OK         On success.
 * \retval     VMK_NOT_FOUND  No binding exists for this connection
 *                            manager instance.
 * \retval     VMK_BAD_PARAM  Invalid parameter.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_RDMACMGetMTU(vmk_RDMACMID id,
                 vmk_RDMAMtu *mtu);

/*
 ***********************************************************************
 * vmk_RDMACMDestroyID --                                         */ /**
 *
 * \brief Destroys the connection manager instance.
 *
 * Destroys the provided connection manager instance, including tearing
 * down any established binding.  This signifies to the RDMA stack that
 * the client is done with all resources associated with the connection
 * manager instance.
 *
 * \note  This function may block.
 *
 * \param[in]  id         Connection manager id.
 *
 * \retval VMK_OK         On success.
 * \retval VMK_BAD_PARAM  Invalid parameter.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RDMACMDestroyID(vmk_RDMACMID id);


#endif /* _VMKAPI_RDMA_CLIENT_H_ */
/** @} */
