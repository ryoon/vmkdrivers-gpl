/* **********************************************************
 * Copyright 2013 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Shaper                                                         */ /**
 * \addtogroup Network
 *@{
 * \addtogroup Shaper Shaper Management APIs
 *@{
 *
 * \par Shaper:
 *
 * This API allows creating an instance of shaper independent of port.
 *
 *
 ***********************************************************************
 */

#ifndef _VMKAPI_NET_SHAPER_INCOMPAT_H_
#define _VMKAPI_NET_SHAPER_INCOMPAT_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "net/vmkapi_net_types.h"
#include "net/vmkapi_net_pktlist.h"

/**
 * \brief Opaque Handle for Shaper
 */

typedef struct vmk_ShaperHandleInt *vmk_ShaperHandle;

/**
 * \brief Structure containing shaper parameters
 */

typedef struct vmk_ShaperParams {
  /** Average Bandwidth (in Bytes Per Second) to sustain */
   vmk_uint64     avgBPS;

  /** Peak Bandwidth (in Bytes Per Second) to sustain */
   vmk_uint64     peakBPS;

  /** Maximum burst size (in Bytes) allowed */
   vmk_uint64     burstSize;
} vmk_ShaperParams;

/*
 ******************************************************************************
 * vmk_ShaperClientResumeFn --                                           */ /**
 *
 * \brief  Handler used by shaper resume shaper client's tolled IOs datapath
 *
 * \param[in]  data               Shaper client data.
 * \param[in]  pktList            List of packets.
 *
 * \retval     VMK_OK             If resume function succeeded.
 * \retval     VMK_FAILURE        Otherwise.
 ******************************************************************************
 */
typedef VMK_ReturnStatus (*vmk_ShaperClientResumeFn)(vmk_AddrCookie data,
                                                     vmk_PktList pktList);

/*
 ******************************************************************************
 * vmk_ShaperClientStatsFn --                                            */ /**
 *
 * \brief  Handler used by shaper module to update client statistics
 *
 * \param[in]  data               Shaper client data.
 * \param[in]  ioQueued           Number of packets queued.
 * \param[in]  ioInjected         Number of packets injected.
 * \param[in]  ioDropped          Number of packets dropped.
 *
 * \retval     None
 ******************************************************************************
 */
typedef void (*vmk_ShaperClientStatsFn)(vmk_AddrCookie data,
                                        vmk_uint32 ioQueued,
                                        vmk_uint32 ioInjected,
                                        vmk_uint32 ioDropped);

/*
 ******************************************************************************
 * vmk_ShaperClientCleanupFn --                                          */ /**
 *
 * \brief  Handler used by shaper module to cleanup client instance
 *
 * \param[in]  data               Shaper client data.
 *
 * \retval     None
 ******************************************************************************
 */
typedef void (*vmk_ShaperClientCleanupFn)(vmk_AddrCookie data);

/*
 ******************************************************************************
 * vmk_ShaperClientDropFn --                                             */ /**
 *
 * \brief  Handler used by shaper module to drop a list of packets in the
 *         client
 *
 * \param[in]  data               Shaper client data.
 * \param[in]  pktList            List of packets.
 *
 * \retval     None
 ******************************************************************************
 */
typedef void (*vmk_ShaperClientDropFn)(vmk_AddrCookie data,
                                       vmk_PktList pktList);

/**
 * \brief Structure containing shaper client operations
 */

typedef struct vmk_ShaperClientOps {

   /** Shaper Client resume function */
   vmk_ShaperClientResumeFn  resumeFn;

   /** Shaper Client cleanup function */
   vmk_ShaperClientCleanupFn cleanupFn;

   /** Shaper Client drop function */
   vmk_ShaperClientDropFn    dropFn;

   /** Shaper Client stats function */
   vmk_ShaperClientStatsFn   statsFn;
} vmk_ShaperClientOps;

/**
 * \brief Structure defining shaper client
 */

typedef struct vmk_ShaperClient {

   /** Shaper Client opaque data structure */
   vmk_AddrCookie data;

   /** Shaper Client operations */
   vmk_ShaperClientOps  ops;
} vmk_ShaperClient;

/**
 * \brief Define different shaper flags
 */
typedef enum vmk_ShaperFlags {

   /** \brief Flag indicating the shaper to copy the packet when put on queue */
   VMK_SHAPER_FLAG_COPY_ON_QUEUE = 1,
} vmk_ShaperFlags;

/*
 ******************************************************************************
 * vmk_ShaperCreate --                                                   */ /**
 *
 * \brief  Creates a shaper instance
 *
 * \param[in]  id                         Shaper identifier.
 * \param[in]  params                     Shaper parameters.
 * \param[in]  flags                      Shaper flags.
 * \param[in]  client                     Shaper client.
 * \param[out] handle                     Shaper handle.
 *
 * \retval     VMK_OK			  On success.
 * \retval     VMK_FAILURE		  Otherwise.
 ******************************************************************************
 */
VMK_ReturnStatus vmk_ShaperCreate(char             *id,
                                  vmk_ShaperParams *params,
                                  vmk_ShaperFlags  flags,
                                  vmk_ShaperClient *client,
                                  vmk_ShaperHandle *handle);

/*
 ******************************************************************************
 * vmk_ShaperRelease --                                                */ /**
 *
 * \brief  Destroys the shaper instance.
 *
 * \param[in]  handle                     Shaper handle.
 *
 * \retval     VMK_OK			  On success.
 * \retval     VMK_FAILURE		  Otherwise.
 ******************************************************************************
 */
VMK_ReturnStatus vmk_ShaperRelease(vmk_ShaperHandle handle);

/*
 ******************************************************************************
 * vmk_ShaperGetConfig --                                                */ /**
 *
 * \brief  Get the current configuration of a given shaper instance
 *
 * \param[in]  handle                     Shaper handle.
 * \param[in]  params                     Shaper parameters.
 *
 * \retval     VMK_OK			  On success.
 * \retval     VMK_FAILURE		  Otherwise.
 ******************************************************************************
 */
VMK_ReturnStatus vmk_ShaperGetConfig(vmk_ShaperHandle handle,
                                     vmk_ShaperParams *params);

/*
 ******************************************************************************
 * vmk_ShaperSetConfig --                                                */ /**
 *
 * \brief  Set up new configuration for a shaper instance
 *
 * \param[in]  handle                     Shaper handle.
 * \param[in]  params                     Shaper parameters.
 *
 * \retval     VMK_OK			  On success.
 * \retval     VMK_FAILURE		  Otherwise.
 ******************************************************************************
 */
VMK_ReturnStatus vmk_ShaperSetConfig(vmk_ShaperHandle handle,
                                     vmk_ShaperParams *params);

/*
 ******************************************************************************
 * vmk_ShaperFilter --                                                */ /**
 *
 * \brief  Filter a packetlist with the shaper instance
 *
 * \note   The packet list is passed to the shaper and if the calling instance
 *         immediately eligible to send some packets that packets are returned
 *         through the pktList. If no packets are eligible the pktList will be
 *         NULL.
 *
 * \param[in]      handle                 Shaper handle.
 * \param[in,out]  pktList                Packet list.
 *
 * \retval     VMK_OK			  On success.
 * \retval     VMK_FAILURE		  Otherwise.
 ******************************************************************************
 */
VMK_ReturnStatus vmk_ShaperFilter(vmk_ShaperHandle handle,
                                  vmk_PktList      pktList);

#endif /* _VMKAPI_NET_SHAPER_INCOMPAT_H_ */
/** @} */
/** @} */
