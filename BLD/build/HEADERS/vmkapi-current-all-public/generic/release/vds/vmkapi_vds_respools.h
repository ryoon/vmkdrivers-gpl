/***********************************************************
 * Copyright 2008 - 2009 VMware, Inc.  All rights reserved.
 ***********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Portset                                                      */ /**
 * \addtogroup VDS
 * @{
 * \defgroup VDSResPools VDS Resource Pools
 *
 * This group defines structures used for VDS resource pools. Resource
 * pool APIs that use these data structure are included in VDS portset
 * APIs.
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_VDS_RESPOOLS_H_
#define _VMKAPI_VDS_RESPOOLS_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "net/vmkapi_net_types.h"

/**
 * \brief Identifier for a resource pool.
 */
typedef void *vmk_PortsetResPoolID;

/**
 * \brief Shares associated to a resource pool.
 *
 * The number of shares given to a resource pool is an abstract value
 * that only makes sense while taking into account the total number
 * of shares distributed among all the existing resource pools. The
 * scheduler uses these shares to provide flexible bandwidth allocation
 * for each resource pool. Each resource pool will get a piece of the
 * underlying networking capacity based on the weight of its relative shares
 * over the overall number of distributed shares. For instance, if resource
 * pool A has 2 shares and resource pool B has 4 shares then effectively
 * resource pool B should have twice as much bandwidth as resource pool A.
 * If now resource pool A and resource pool B have 50 shares each then
 * they will get the same share of the bandwidth.
 */
typedef vmk_uint32 vmk_PortsetResPoolShares;

/**
 * \brief Limit associated to a resource pool.
 *
 * This limit, expressed in bits per second, is the maximum networking
 * bandwidth a given resource pool will be able to use on average. The
 * value VMK_PORTSET_RESPOOL_LIMIT_INFINITE can be used if the resource
 * pool networking usage should not be capped.
 */
typedef vmk_int32 vmk_PortsetResPoolLimit;

/**
 * \brief Queue depth associated to a resource pool.
 *
 * This queue depth, expressed in bytes, represents the maximum
 * capacity reserved for a given resource pool for packets queuing
 * purposes. Reaching this limit will effectively mean packet drops.
 */
typedef vmk_int32 vmk_PortsetResPoolQueueDepth;

/**
 * \brief Reservation associated to a resource pool.
 *
 * This Reservation, expressed in Mbps, represents the minimum
 * capacity reserved for a given resource pool for packets queuing
 * purposes.
 */
typedef vmk_int32 vmk_PortsetResPoolReservation;

/**
 * \brief 802.1p Priority tag associated to a resource pool.
 *
 * The associated 802.1p tag does not have any semantics in term of
 * scheduling. However it will be used to tag all packets outgoing
 * of a given resource pool so that QoS policy are not lost
 * after the first hop and may be still enforced in the network.
 */
typedef vmk_uint8 vmk_PortsetResPoolPriorityTag;

/**
 * \brief Infinite Limit for a resource pool configuration.
 */
#define VMK_PORTSET_RESPOOL_LIMIT_INFINITE -1

/**
 * \brief Special queue depth.
 *
 * This value indicates the system default queue depth.
 */
#define VMK_PORTSET_RESPOOL_DEFAULT_QUEUE_DEPTH -1

/**
 * \brief Special priority tag.
 *
 * This value indicates the system default priority tag.
 */
#define VMK_PORTSET_RESPOOL_DEFAULT_PRIORITY_TAG 0

/**
 * \brief Maximum user shares.
 *
 * This value indicates the maximum number of shares a user allocates to a resource pool.
 */
#define VMK_PORTSET_RESPOOL_MAX_SHARES 100

/**
 * \brief Structure used to represent a resource pool configuration.
 */
typedef struct vmk_PortsetResPoolCfg {
   /** Shares */
   vmk_PortsetResPoolShares      shares;
   /** Limit */
   vmk_PortsetResPoolLimit       limit;
   /** Reservation */
   vmk_PortsetResPoolReservation reservation;
   /** 802.1p Priotiy */
   vmk_PortsetResPoolPriorityTag pTag;
   /** Buffer depth */
   vmk_PortsetResPoolQueueDepth  queueDepth;
} vmk_PortsetResPoolCfg;


/**
 * \brief Structure used to represent resource pool stats.
 */
typedef struct vmk_PortsetResPoolStats {
   /** Number of packets input to resource pool */
   vmk_uint64                    pktsIn;
   /** Number of bytes input to resource pool */
   vmk_uint64                    bytesIn;
   /** Number of packets output from resource pool */
   vmk_uint64                    pktsOut;
   /** Number of bytes output from resource pool */
   vmk_uint64                    bytesOut;
   /** Number of packets dropped at resource pool */
   vmk_uint64                    pktsDropped;
   /** Number of bytes dropped at resource pool */
   vmk_uint64                    bytesDropped;
} vmk_PortsetResPoolStats;


/**
 * \brief Pre-defined resource pools
 */
#define VMK_PORTSET_VMOTION_RESPOOL_ID "netsched.pools.persist.vmotion"
#define VMK_PORTSET_ISCSI_RESPOOL_ID   "netsched.pools.persist.iscsi"
#define VMK_PORTSET_NFS_RESPOOL_ID     "netsched.pools.persist.nfs"
#define VMK_PORTSET_FT_RESPOOL_ID      "netsched.pools.persist.ft"
#define VMK_PORTSET_VM_RESPOOL_ID      "netsched.pools.persist.vm"
#define VMK_PORTSET_MGMT_RESPOOL_ID    "netsched.pools.persist.mgmt"
#define VMK_PORTSET_FCOE_RESPOOL_ID    "netsched.pools.persist.fcoe"
#define VMK_PORTSET_HBR_RESPOOL_ID     "netsched.pools.persist.hbr"
#define VMK_PORTSET_VSAN_RESPOOL_ID    "netsched.pools.persist.vsan"
#define VMK_PORTSET_VDP_RESPOOL_ID     "netsched.pools.persist.vdp"

#endif
/** @} */
/** @} */
