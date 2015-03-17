/* **********************************************************
 * Copyright 2006 - 2009 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * UplinkQueue                                                    */ /**
 * \addtogroup Network
 *@{
 * \defgroup UplinkQueue Uplink Queue
 *@{
 *
 ***********************************************************************
 */

#ifndef _VMKAPI_NET_QUEUE_H_
#define _VMKAPI_NET_QUEUE_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "net/vmkapi_net_types.h"
#include "net/vmkapi_net_poll.h"
#include "net/vmkapi_net_pkt.h"


/** Default queue ID */
#define VMK_UPLINK_QUEUE_DEFAULT_QUEUEID     ((vmk_UplinkQueueID)0)

/** Invalid queue ID */
#define VMK_UPLINK_QUEUE_INVALID_QUEUEID     ((vmk_UplinkQueueID)-1)

/** Invalid filter ID */
#define VMK_UPLINK_QUEUE_INVALID_FILTERID    ((vmk_UplinkQueueFilterID)-1)

/** Invalid uplink queue priority */
#define VMK_UPLINK_QUEUE_PRIORITY_INVALID    VMK_VLAN_PRIORITY_INVALID

/** Definition of RSS maximum number of queues */
#define VMK_UPLINK_QUEUE_MAX_RSS_QUEUES    16

/** Definition of RSS indirection table maximum size */
#define VMK_UPLINK_QUEUE_MAX_RSS_IND_TABLE_SIZE 128

/** Definition of RSS hash key maximum size */
#define VMK_UPLINK_QUEUE_MAX_RSS_KEY_SIZE  40

struct vmk_UplinkStats;


/**
 * \brief UplinkQueue ID
 */
typedef struct vmk_UplinkQueueIDInt *vmk_UplinkQueueID;


/**
 * \brief UplinkQueue filter ID
 */
typedef vmk_uint32 vmk_UplinkQueueFilterID;


/** UplinkQueue priority */
typedef vmk_VlanPriority vmk_UplinkQueuePriority;


/**
 * \brief UplinkQueue filter type
 */
typedef enum vmk_UplinkQueueFilterType {

   /** Invalid filter type */
   VMK_UPLINK_QUEUE_FILTER_TYPE_INVALID = 0,

   /** Rx/Tx filter type */
   VMK_UPLINK_QUEUE_FILTER_TYPE_TXRX    = 1,
} vmk_UplinkQueueFilterType;


/**
 * \brief UplinkQueue filter class
 */
typedef enum vmk_UplinkQueueFilterClass {
   /** Invalid filter */
   VMK_UPLINK_QUEUE_FILTER_CLASS_NONE      = 0x0,

   /** MAC only filter */
   VMK_UPLINK_QUEUE_FILTER_CLASS_MAC_ONLY  = (1 << 0),

   /** VLAN only filter */
   VMK_UPLINK_QUEUE_FILTER_CLASS_VLAN_ONLY = (1 << 1),

   /** VLAN + MAC address filter */
   VMK_UPLINK_QUEUE_FILTER_CLASS_VLANMAC   = (1 << 2),

   /** VXLAN filter */
   VMK_UPLINK_QUEUE_FILTER_CLASS_VXLAN     = (1 << 3),
} vmk_UplinkQueueFilterClass;


/**
 * UplinkQueue queue type
 */
typedef enum vmk_UplinkQueueType {
   /** Invalid queue type */
   VMK_UPLINK_QUEUE_TYPE_INVALID     = 0x0,

   /** RX queue type */
   VMK_UPLINK_QUEUE_TYPE_RX          = 0x1,

   /** TX queue type */
   VMK_UPLINK_QUEUE_TYPE_TX          = 0x2,
} vmk_UplinkQueueType;


/**
 * UplinkQueue queue flags
 */
typedef enum vmk_UplinkQueueFlags {
   /** queue is unused */
   VMK_UPLINK_QUEUE_FLAG_UNUSED   = 0x0,

   /** queue is allocated and in use */
   VMK_UPLINK_QUEUE_FLAG_IN_USE   = (1 << 0),

   /** queue is the default queue */
   VMK_UPLINK_QUEUE_FLAG_DEFAULT  = (1 << 1),
} vmk_UplinkQueueFlags;


/**
 * UplinkQueue queue state
 */
typedef enum vmk_UplinkQueueState {
   /** Queue stopped by the driver */
   VMK_UPLINK_QUEUE_STATE_STOPPED  = 0x0,

   /** Queue started administratively by the networking stack */
   VMK_UPLINK_QUEUE_STATE_STARTED  = 0x1,
} vmk_UplinkQueueState;

/**
 * LRO type
 *
 * \note LRO types are for informational purpose only. They don't participate
 *       in queues partitioning and load balancing. When VMKernel allocates
 *       a RX queue with LRO feature, it passes VMK_UPLINK_QUEUE_FEAT_LRO
 *       down to driver. And driver should never expect VMKernel will pass
 *       LRO types down.
 */
typedef enum vmk_UplinkQueueLROType {
   /** Large receive offload on IPv4 */
   VMK_UPLINK_QUEUE_LRO_TYPE_IPV4   = (1 << 0),

   /** Large receive offload on IPv6 */
   VMK_UPLINK_QUEUE_LRO_TYPE_IPV6   = (1 << 1),
} vmk_UplinkQueueLROType;

/**
 * LRO constraints
 */
typedef struct vmk_UplinkQueueLROConstraints {
   /** Combination of hardware supported LRO types */
   vmk_UplinkQueueLROType  supportedTypes;
} vmk_UplinkQueueLROConstraints;

/**
 * UplinkQueue queue feature
 */
typedef enum vmk_UplinkQueueFeature {
   /** None */
   VMK_UPLINK_QUEUE_FEAT_NONE     = 0x00,

   /** Supports setting queue large receive offload(LRO) feature */
   VMK_UPLINK_QUEUE_FEAT_LRO      = (1 << 0),

   /** Paired queue feature */
   VMK_UPLINK_QUEUE_FEAT_PAIR     = (1 << 1),

   /** Supports setting queue receive segment scaling (RSS) feature */
   VMK_UPLINK_QUEUE_FEAT_RSS      = (1 << 2),

   /** Supports setting queue priority */
   VMK_UPLINK_QUEUE_FEAT_SET_PRIO = (1 << 3),

   /** Supports setting queue level intr coalescing parameters */
   VMK_UPLINK_QUEUE_FEAT_COALESCE = (1 << 4),

   /**
    * Supports modification of RSS indirection table at run time
    * \note Driver must call vmk_UplinkQueueRegisterFeatureOps, and
    *       provide vmk_UplinkQueueRSSDynOps before claiming it supports
    *       this feature
    */
   VMK_UPLINK_QUEUE_FEAT_RSS_DYN  = (1 << 5),

   /** Latency Sensitive queue feature */
   VMK_UPLINK_QUEUE_FEAT_LATENCY     = (1 << 6),

   /** Dynamic queue feature */
   VMK_UPLINK_QUEUE_FEAT_DYNAMIC     = (1 << 7),

   /** Pre-emptible queue feature */
   VMK_UPLINK_QUEUE_FEAT_PREEMPTIBLE     = (1 << 8),
} vmk_UplinkQueueFeature;


/**
 * \brief MAC address only filter info
 */
typedef struct vmk_UplinkQueueMACFilterInfo {
   /** MAC address */
   vmk_EthAddress     mac;
} vmk_UplinkQueueMACFilterInfo;


/**
 * \brief VLAN ID only filter info
 */
typedef struct vmk_UplinkQueueVLANFilterInfo {
   /** VLAN ID */
   vmk_VlanID     vlanID;
} vmk_UplinkQueueVLANFilterInfo;


/**
 * \brief MAC address + VLAN ID filter info
 */
typedef struct vmk_UplinkQueueVLANMACFilterInfo {
   /** MAC address */
   vmk_EthAddress     mac;

   /** VLAN ID */
   vmk_VlanID         vlanID;
} vmk_UplinkQueueVLANMACFilterInfo;


/**
 * \brief VXLAN filter info
 */
typedef struct vmk_UplinkQueueVXLANFilterInfo {
   /** Inner MAC address */
   vmk_EthAddress     innerMAC;

   /** Outer  MAC address */
   vmk_EthAddress     outerMAC;

   /** VXLAN ID */
   vmk_uint32         vxlanID;
} vmk_UplinkQueueVXLANFilterInfo;


/**
 * \brief Filter definition
 */
typedef struct vmk_UplinkQueueFilter {

   /** Filter class */
   vmk_UplinkQueueFilterClass class;

   /**
    * pointer to filter when using filterInfo
    * class                 filter
    * MAC_ONLY              vmk_UplinkQueueMACFilterInfo
    * VLAN_ONLY             vmk_UplinkQueueVLANFilterInfo
    * VLAN+MAC              vmk_UplinkQueueVLANMACFilterInfo
    * VXLAN                 vmk_UplinkQueueVXLANFilterInfo
    */
   union {
       vmk_AddrCookie filterInfo;
       vmk_UplinkQueueMACFilterInfo *macFilterInfo;
       vmk_UplinkQueueVLANFilterInfo *vlanFilterInfo;
       vmk_UplinkQueueVLANMACFilterInfo *vlanMacFilterInfo;
       vmk_UplinkQueueVXLANFilterInfo *vxlanFilterInfo;
   };
} vmk_UplinkQueueFilter;


/**
 * \brief Filter properties
 */
typedef enum vmk_UplinkQueueFilterProperties {
   /** None */
   VMK_UPLINK_QUEUE_FILTER_PROP_NONE = 0x0,

   /** Management filter */
   VMK_UPLINK_QUEUE_FILTER_PROP_MGMT = 0x1,

   /** Opportunistically packed with other filters */
   VMK_UPLINK_QUEUE_FILTER_PROP_PACK_OPPO = 0x2,

   /** Opportunistically seek exclusive netqueue */
   VMK_UPLINK_QUEUE_FILTER_EXCL_PACK_OPPO = 0x4,
} vmk_UplinkQueueFilterProperties;


/**
 * \brief UplinkQueue queue attribute type
 */
typedef enum vmk_UplinkQueueAttrType {
   /** Priority attribute */
   VMK_UPLINK_QUEUE_ATTR_PRIOR = 0x0,

   /** Features attribute */
   VMK_UPLINK_QUEUE_ATTR_FEAT  = 0x1,
} vmk_UplinkQueueAttrType;


/**
 * \brief UplinkQueue queue attribute
 */
typedef struct vmk_UplinkQueueAttr {
   /** Uplink queue attribute type */
   vmk_UplinkQueueAttrType type;

   union {

      /** VMK_UPLINK_QUEUE_ATTR_PRIOR argument */
      vmk_VlanPriority priority;

      /** VMK_UPLINK_QUEUE_ATTR_FEAT argument */
      vmk_UplinkQueueFeature features;

      /** Generic attribute argument */
      vmk_AddrCookie attr;
   } args;
} vmk_UplinkQueueAttr;


/**
 * \brief Structure describing interrupt coalescing parameters
 *        of an uplink
 */
typedef struct vmk_UplinkCoalesceParams {
   /**
    * \brief number of microseconds to wait for Rx, before
    *        interrupting
    */
   vmk_uint32             rxUsecs;

   /**
    * \brief maximum number of (Rx) frames to wait for, before
    *        interrupting
    */
   vmk_uint32             rxMaxFrames;

   /**
    * \brief number of microseconds to wait for completed Tx, before
    *        interrupting
    */
   vmk_uint32             txUsecs;

   /**
    * \brief maximum number of completed (Tx) frames to wait for,
    *        before interrupting
    */
   vmk_uint32             txMaxFrames;

   /**
    * \brief Use adaptive RX coalescing.
    *
    * Adaptive RX coalescing is an algorithm implemented by some
    * drivers to improve latency under low packet rates and improve
    * throughput under high packet rates.  This field can be silently
    * ignored by driver if it's not implemented.
    */
   vmk_Bool               useAdaptiveRx;

   /**
    * \brief Use adaptive TX coalescing.
    *
    * Adaptive TX coalescing is an algorithm implemented by some
    * drivers to improve latency under low packet rates and improve
    * throughput under high packet rates.  This field can be silently
    * ignored by driver if it's not implemented.
    */
   vmk_Bool               useAdaptiveTx;

   /**
    * \brief Rate sampling interval.
    *
    * How often driver performs adaptive coalescing packet rate
    * sampling, measured in seconds.  Must not be zero.
    */
   vmk_uint32             rateSampleInterval;

} vmk_UplinkCoalesceParams;


/** \brief Structure describing realloc params of queue. */
typedef struct vmk_UplinkQueueReallocParams {
   /** \brief Queue type. */
   vmk_UplinkQueueType     qType;

   /**
    * \brief Number of attributes.
    *
    * Attributes associated with the function vmk_UplinkQueueReallocWithAttrCB.
    * Number of attributes should not be greater than VMK_UPLINK_QUEUE_ATTR_NUM.
    */
   vmk_uint16              numAttr;

   /** \brief Queue attributes. */
   vmk_UplinkQueueAttr     *attr;

   /** \brief ID of already created queue. */
   vmk_UplinkQueueID       *qID;

   /** \brief Count of filters to be removed. */
   vmk_uint16              numOldFilters;

   /** \brief Pointer to array of already applied filter IDs. */
   vmk_UplinkQueueFilterID *oldFid;

   /** \brief New queue filter to be applied. */
   vmk_UplinkQueueFilter   *newFilter;

   /** \brief New Filter ID. */
   vmk_UplinkQueueFilterID *newFid;

   /** \brief Potential paired tx queue hardware index. */
   vmk_uint32             *pairHWQID;

   /** \brief Net poll on top of the allocated queue if any. */
   vmk_NetPoll            *netpoll;

} vmk_UplinkQueueReallocParams;


/**
 * \brief RSS hash key
 *
 * This structure is used to program the RSS hash key.
 */
typedef struct vmk_UplinkQueueRSSHashKey {
   /** Key size in bytes */
   vmk_uint16  keySize;

   /** Key contents */
   vmk_uint8   key[0];

} vmk_UplinkQueueRSSHashKey;


/**
 * \brief RSS indirection table
 *
 * This structure is used to program or update the RSS indirection table.
 */
typedef struct vmk_UplinkQueueRSSIndTable {
   /** Size of the table in bytes */
   vmk_uint16  tableSize;

   /** Contents */
   vmk_uint8   table[0];

} vmk_UplinkQueueRSSIndTable;


/**
 * \brief RSS parameters
 */
typedef struct vmk_UplinkQueueRSSParams {
   /** Number of RSS pools supported */
   vmk_uint16     numRSSPools;

   /** Number of RSS queues per pool */
   vmk_uint16     numRSSQueuesPerPool;

   /** Length of the RSS hash key in bytes */
   vmk_uint16     rssHashKeySize;

   /** Size of the RSS indirection table */
   vmk_uint16     rssIndTableSize;

} vmk_UplinkQueueRSSParams;


/*
 ***********************************************************************
 * vmk_UplinkCoalesceParamsGetCB --                               */ /**
 *
 * \brief  Handler used by vmkernel to get coalescing parameters of
 *         device associated with an uplink.
 *
 * \param[in]  driverData      Points to the internal module structure
 *                             for the device associated to the uplink.
 *                             Before calling vmk_DeviceRegister, device
 *                             driver needs to assign this pointer to
 *                             member driverData in structure
 *                             vmk_UplinkRegData.
 * \param[out] coalesceParams  Structure used to store all the requested
 *                             information.
 *
 * \retval     VMK_OK          If the driver information is properly
 *                             stored
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkCoalesceParamsGetCB)(vmk_AddrCookie driverData,
                                                          vmk_UplinkCoalesceParams *params);


/*
 ***********************************************************************
 * vmk_UplinkCoalesceParamsSetCB --                               */ /**
 *
 * \brief  Handler used by vmkernel to set coalescing parameters of
 *         device associated with an uplink.
 *
 * \param[in] driverData       Points to the internal module structure
 *                             for the device associated to the uplink.
 *                             Before calling vmk_DeviceRegister, device
 *                             driver needs to assign this pointer to
 *                             member driverData in structure
 *                             vmk_UplinkRegData.
 * \param[in] coalesceParams   Structure used to store all the requested
 *                             information.
 *
 * \retval    VMK_OK           If the driver information is properly set
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkCoalesceParamsSetCB)(vmk_AddrCookie driverData,
                                                          vmk_UplinkCoalesceParams *params);


/**
 * Uplink coalesce paramters
 */
typedef struct vmk_UplinkCoalesceParamsOps {
   /** callback to get coalesce paramters */
   vmk_UplinkCoalesceParamsGetCB    getParams;

   /** callback to set coalesce paramters */
   vmk_UplinkCoalesceParamsSetCB    setParams;
} vmk_UplinkCoalesceParamsOps;


/*
 ***********************************************************************
 * vmk_UplinkQueueGetNumQueuesSupported                           */ /**
 *
 * \brief Get maximum number of TX and RX queues device driver should
 *        expose to vmkernel
 *
 * \note Device driver must call this function before uplink logical
 *       device registration. The total TX/RX queue number exposed by
 *       driver should not exceed the numbers returned.
 *
 * \param[in]  numDevTxQueues  Number of TX queues device can support
 * \param[in]  numDevRxQueues  Number of RX queues device can support
 * \param[out] maxTxQueues     Maximum number of TX queues driver should
 *                             expose to vmkernel. It's set to the lower
 *                             one of numDevTxQueues and vmkernel
 *                             physical CPU cores
 * \param[out] maxRxQueues     Maximum number of RX queues driver should
 *                             expose to vmkernel. It's set to the lower
 *                             one of numDevRxQueues and vmkernel
 *                             physical CPU cores
 *
 * \retval     VMK_OK          Always
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkQueueGetNumQueuesSupported(vmk_uint32 numDevTxQueues,
                                                      vmk_uint32 numDevRxQueues,
                                                      vmk_uint32 *maxTxQueues,
                                                      vmk_uint32 *maxRxQueues);


/*
 ***********************************************************************
 * vmk_UplinkQueueSetQueueIDUserVal --                            */ /**
 *
 * \brief Set user's private value for queue ID.
 *
 * \param[in]  qid       Queue ID.
 * \param[in]  userval   User value.
 *
 * \retval     VMK_OK    Always.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkQueueSetQueueIDUserVal(vmk_UplinkQueueID *qid,
                                                  vmk_uint32 userval);


/*
 ***********************************************************************
 * vmk_UplinkQueueIDUserVal --                                    */ /**
 *
 * \brief Get user part of queue ID
 *
 * \param[in] qid           Queue ID.
 *
 * \retval    vmk_uint32    User value.
 *
 ***********************************************************************
 */

vmk_uint32 vmk_UplinkQueueIDUserVal(vmk_UplinkQueueID qid);


/*
 ***********************************************************************
 * vmk_UplinkQueueSetQueueIDQueueDataIndex --                     */ /**
 *
 * \brief Set queue data index for queue ID.
 *
 * \param[in]  qid       Queue ID.
 * \param[in]  index     Index into queue data array in shared data
 *
 * \retval     VMK_OK    Always.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkQueueSetQueueIDQueueDataIndex(vmk_UplinkQueueID *qid,
                                                         vmk_uint32 index);


/*
 ***********************************************************************
 * vmk_UplinkQueueIDQueueDataIndex --                             */ /**
 *
 * \brief Get queue data array index part of queue ID
 *
 * \param[in] qid           Queue ID.
 *
 * \retval    vmk_uint32    Queue data array index
 *
 ***********************************************************************
 */

vmk_uint32 vmk_UplinkQueueIDQueueDataIndex(vmk_UplinkQueueID qid);


/*
 ***********************************************************************
 * vmk_UplinkQueueMkTxQueueID --                                  */ /**
 *
 * \brief Set shared queue data array index and user value in a TX queue
 *        ID.
 *
 * \param[out] qid       Pointer to queue ID allocated by caller.
 * \param[in]  index     The index into queue data array in shared data
 * \param[in]  val       The embedded value of the queue ID.
 *
 * \retval     VMK_OK    Always.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkQueueMkTxQueueID(vmk_UplinkQueueID *qid,
                                            vmk_uint32 index,
                                            vmk_uint32 val);


/*
 ***********************************************************************
 * vmk_UplinkQueueMkRxQueueID --                                  */ /**
 *
 * \brief Set shared queue data array index and user value in a RX queue
 *        ID.
 *
 * \param[out] qid       Pointer to queue ID allocated by caller.
 * \param[in]  index     The index into queue data array in shared data
 * \param[in]  val       The embedded value of the queue ID.
 *
 * \retval     VMK_OK    Always.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkQueueMkRxQueueID(vmk_UplinkQueueID *qid,
                                            vmk_uint32 index,
                                            vmk_uint32 val);


/*
 ***********************************************************************
 * vmk_UplinkQueueIDVal --                                        */ /**
 *
 * \brief Retrieve the embedded value of a queue ID.
 *
 * \param[in] qid       The aimed queue ID.
 *
 * \return              The embedded value.
 *
 ***********************************************************************
 */

vmk_uint32 vmk_UplinkQueueIDVal(vmk_UplinkQueueID qid);


/*
 ***********************************************************************
 * vmk_UplinkQueueIDType --                                       */ /**
 *
 * \brief Retrieve the type of a queue ID.
 *
 * \param[in] qid       The aimed queue ID.
 *
 * \return              The type.
 *
 ***********************************************************************
 */

vmk_UplinkQueueType vmk_UplinkQueueIDType(vmk_UplinkQueueID qid);


/*
 ***********************************************************************
 * vmk_UplinkQueueMkFilterID --                                   */ /**
 *
 * \brief Create a filter ID.
 *
 * \param[in]  val       The embedded value of the filter ID.
 * \param[out] fid       The created filter.
 *
 * \retval     VMK_OK    Always.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkQueueMkFilterID(vmk_UplinkQueueFilterID *fid,
                                           vmk_uint16 val);


/*
 ***********************************************************************
 * vmk_UplinkQueueFilterIDVal --                                  */ /**
 *
 * \brief Retrieve the embedded value of a filter ID.
 *
 * \param[in] fid       The aimed filter ID.
 *
 * \return              The embedded value.
 *
 ***********************************************************************
 */

vmk_uint16 vmk_UplinkQueueFilterIDVal(vmk_UplinkQueueFilterID fid);


/*
 ***********************************************************************
 * vmk_PktQueueIDGet --                                           */ /**
 *
 * \brief Retrieve netqueue queue ID of a specified packet.
 *
 * \param[in]  pkt        Packet of interest
 *
 * \return                UplinkQueue queue ID.
 *
 ***********************************************************************
 */

vmk_UplinkQueueID vmk_PktQueueIDGet(vmk_PktHandle *pkt);


/*
 ***********************************************************************
 * vmk_PktQueueIDSet --                                           */ /**
 *
 * \brief Set netqueue queue ID of a specified packet.
 *
 * \param[in]  pkt        Packet of interest
 * \param[in]  qid        Packet queue ID
 *
 * \retval     VMK_OK
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktQueueIDSet(vmk_PktHandle    *pkt,
                                   vmk_UplinkQueueID qid);


/*
 ***********************************************************************
 * vmk_UplinkQueueAllocCB --                                      */ /**
 *
 * \brief Handler used by vmkernel to call into device driver to
 *        allocate a TX/RX queue.
 *
 * \note  The TX/RX queue is allocated and maintained by device driver.
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[in]   qType        queue type
 * \param[out]  qID          ID of newly created queue, driver must call
 *                           vmk_UplinkQueueMkTxQueueID or
 *                           vmk_UplinkQueueMkRxQueueID to set its value.
 * \param[out]  netpoll      Net poll on top of the allocated queue if
 *                            any
 *
 * \retval      VMK_OK       on success
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkQueueAllocCB)(vmk_AddrCookie       driverData,
                                                   vmk_UplinkQueueType  qType,
                                                   vmk_UplinkQueueID   *qID,
                                                   vmk_NetPoll         *netpoll);


/*
 ***********************************************************************
 * vmk_UplinkQueueAllocWithAttrCB --                              */ /**
 *
 * \brief Handler used by vmkernel to call into device driver to
 *        allocate a TX/RX queue with extra attributes.
 *
 * \note  The TX/RX queue is allocated and maintained by device driver.
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[in]   qType        queue type
 * \param[in]   numAttr      Number of attributes, It cannot be greater
                             than VMK_UPLINK_QUEUE_ATTR_NUM
 * \param[in]   attr         Queue attributes
 * \param[out]  qID          ID of newly created queue, driver must call
 *                           vmk_UplinkQueueMkTxQueueID or
 *                           vmk_UplinkQueueMkRxQueueID to set its value.
 * \param[out]  netpoll      Net poll on top of the allocated queue if
 *                           any
 *
 * \retval      VMK_OK       on success
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkQueueAllocWithAttrCB)(vmk_AddrCookie        driverData,
                                                           vmk_UplinkQueueType   qType,
                                                           vmk_uint16            numAttr,
                                                           vmk_UplinkQueueAttr  *attr,
                                                           vmk_UplinkQueueID    *qID,
                                                           vmk_NetPoll          *netpoll);


/*
 ***********************************************************************
 * vmk_UplinkQueueReallocWithAttrCB --                            */ /**
 *
 * \brief Handler for reallocating RX queue.
 *
 * Handler used by vmkernel to call into device driver to
 * reallocate a RX queue with extra attributes.
 *
 * \note  The RX queue is allocated and maintained by device driver.
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[in]   params       Realloc queue params.
 *
 * \retval      VMK_OK       On success.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkQueueReallocWithAttrCB)(
   vmk_AddrCookie driverData,
   vmk_UplinkQueueReallocParams *params);


/*
 ***********************************************************************
 * vmk_UplinkQueueFreeCB --                                       */ /**
 *
 * \brief Handler used by vmkernel to free a queue
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[in]   qID          queue ID
 *
 * \retval      VMK_OK       on success
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkQueueFreeCB)(vmk_AddrCookie     driverData,
                                                  vmk_UplinkQueueID  qID);


/*
 ***********************************************************************
 * vmk_UplinkQueueQuiesceCB --                                    */ /**
 *
 * \brief Handler used by vmkernel to quiesce a queue
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[in]   qID          queue ID
 *
 * \retval      VMK_OK       on success
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkQueueQuiesceCB)(vmk_AddrCookie     driverData,
                                                     vmk_UplinkQueueID  qID);


/*
 ***********************************************************************
 * vmk_UplinkQueueStartCB --                                      */ /**
 *
 * \brief Handler used by vmkernel to start a queue
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[in]   qID          queue ID
 *
 * \retval      VMK_OK       on success
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkQueueStartCB)(vmk_AddrCookie     driverData,
                                                   vmk_UplinkQueueID  qID);


/*
 ***********************************************************************
 * vmk_UplinkQueueFilterApplyCB --                                */ /**
 *
 * \brief Handler used by vmkernel to apply a queue filter
 *
 * \note pairHWQID is hardware's internal TX queue identifier, it must
 *       be same as the embedded value passed to
 *       vmk_UplinkQueueMkTxQueueID when a TX queue is allocated.
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[in]   qID          queue ID
 * \param[in]   qFilter      queue Filter
 * \param[out]  fid          Filter ID
 * \param[out]  pairHWQID    Potential paired tx queue hardware index
 *
 * \retval      VMK_OK       on success
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkQueueFilterApplyCB)(vmk_AddrCookie           driverData,
                                                         vmk_UplinkQueueID        qID,
                                                         vmk_UplinkQueueFilter   *qFilter,
                                                         vmk_UplinkQueueFilterID *fid,
                                                         vmk_uint32              *pairHWQID);


/*
 ***********************************************************************
 * vmk_UplinkQueueFilterRemoveCB --                               */ /**
 *
 * \brief Handler used by vmkernel to remove a queue filter
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[in]   qID          queue ID
 * \param[in]   fid          Filter ID
 *
 * \retval      VMK_OK       on success
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkQueueFilterRemoveCB)(vmk_AddrCookie           driverData,
                                                          vmk_UplinkQueueID        qID,
                                                          vmk_UplinkQueueFilterID  fid);


/*
 ***********************************************************************
 * vmk_UplinkQueueStatsGetCB --                                   */ /**
 *
 * \brief Handler used by vmkernel to get queue stats
 *
 * \note This callback is designed for future use, and not invoked by
 *       current uplink layer implementation.
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[in]   qID          queue ID
 * \param[out]  stats        stats of queue
 *
 * \retval      VMK_OK       on success
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkQueueStatsGetCB)(vmk_AddrCookie          driverData,
                                                      vmk_UplinkQueueID       qID,
                                                      struct vmk_UplinkStats *stats);


/*
 ***********************************************************************
 * vmk_UplinkQueueFeatureToggleCB --                              */ /**
 *
 * \brief Handler used by vmkernel to toggle a queue feature
 *
 * \note This callback is designed for future use, and not invoked by
 *       current uplink layer implementation.
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[in]   qID          queue ID
 * \param[in]   qFeature     queue Feature
 * \param[in]   setUnset     set or unset the feature
 *
 * \retval      VMK_OK       on success
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkQueueFeatureToggleCB)(vmk_AddrCookie          driverData,
                                                           vmk_UplinkQueueID       qID,
                                                           vmk_UplinkQueueFeature  qFeature,
                                                           vmk_Bool                setUnset);


/*
 ***********************************************************************
 * vmk_UplinkQueueTxPrioritySetCB --                              */ /**
 *
 * \brief Handler used by vmkernel to set queue priority
 *
 * \note This callback is designed for future use, and not invoked by
 *       current uplink layer implementation.
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[in]   qID          queue ID
 * \param[in]   priority     queue priority
 *
 * \retval      VMK_OK       on success
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkQueueTxPrioritySetCB)(vmk_AddrCookie            driverData,
                                                           vmk_UplinkQueueID         qID,
                                                           vmk_UplinkQueuePriority   priority);


/*
 ***********************************************************************
 * vmk_UplinkQueueCoalesceParamsSetCB --                          */ /**
 *
 * \brief Handler used by vmkernel to set queue coalesce parameters
 *
 * \note This callback is designed for future use, and not invoked by
 *       current uplink layer implementation.
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[in]   qID          queue ID
 * \param[in]   params       coalesce params
 *
 * \retval      VMK_OK       on success
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkQueueCoalesceParamsSetCB)(vmk_AddrCookie            driverData,
                                                               vmk_UplinkQueueID         qID,
                                                               vmk_UplinkCoalesceParams *params);


/*
 ***********************************************************************
 * vmk_UplinkQueueRSSParamsGetCB --                               */ /**
 *
 * \brief Handler used by vmkernel to get device RSS parameters
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[out]  params       RSS parameters returned
 *
 * \retval      VMK_OK       on success
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkQueueRSSParamsGetCB)(vmk_AddrCookie            driverData,
                                                          vmk_UplinkQueueRSSParams *params);


/*
 ***********************************************************************
 * vmk_UplinkQueueRSSStateInitCB --                               */ /**
 *
 * \brief Handler used by vmkernel to init RSS state on device
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[in]   rssHashKey   Initial value of RSS hash key
 * \param[in]   rssIndTable  Initial value of RSS indirection table
 *
 * \retval      VMK_OK       on success
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkQueueRSSStateInitCB)(vmk_AddrCookie                driverData,
                                                          vmk_UplinkQueueRSSHashKey    *rssHashKey,
                                                          vmk_UplinkQueueRSSIndTable   *rssIndTable);


/*
 ***********************************************************************
 * vmk_UplinkQueueRSSIndTableUpdateCB --                          */ /**
 *
 * \brief Handler used by vmkernel to update RSS indirection table on
 *        device
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[in]   rssIndTable  RSS indirection table to update
 *
 * \retval      VMK_OK       on success
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkQueueRSSIndTableUpdateCB)(vmk_AddrCookie                driverData,
                                                               vmk_UplinkQueueRSSIndTable   *rssIndTable);


/*
 ***********************************************************************
 * vmk_UplinkQueueRSSIndTableGetCB --                             */ /**
 *
 * \brief Handler used by vmkernel to get current RSS indirection table
 *        on device
 *
 * \note This callback is designed for future use, and not invoked by
 *       current uplink layer implementation.
 *
 * \param[in]   driverData   Points to the internal module structure for
 *                           the device associated to the uplink. Before
 *                           calling vmk_DeviceRegister, device driver
 *                           needs to assign this pointer to member
 *                           driverData in structure vmk_UplinkRegData.
 * \param[out]  rssIndTable  Current RSS hash indirection table on device
 *
 * \retval      VMK_OK       on success
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_UplinkQueueRSSIndTableGetCB)(vmk_AddrCookie                driverData,
                                                            vmk_UplinkQueueRSSIndTable   *rssIndTable);


/** UplinkQueue operations */
typedef struct vmk_UplinkQueueOps {
   /** callback to allocate netqueue queue */
   vmk_UplinkQueueAllocCB              queueAlloc;

   /** callback to allocate queue with attributes */
   vmk_UplinkQueueAllocWithAttrCB      queueAllocWithAttr;

   /** callback to Reallocate queue with attributes */
   vmk_UplinkQueueReallocWithAttrCB    queueReallocWithAttr;

   /** callback to free queue */
   vmk_UplinkQueueFreeCB               queueFree;

   /** callback to quiesce queue */
   vmk_UplinkQueueQuiesceCB            queueQuiesce;

   /** callback to start queue */
   vmk_UplinkQueueStartCB              queueStart;

   /** callback to apply queue filter */
   vmk_UplinkQueueFilterApplyCB        queueApplyFilter;

   /** callback to remove queue filter */
   vmk_UplinkQueueFilterRemoveCB       queueRemoveFilter;

   /**callback to get queue stats */
   vmk_UplinkQueueStatsGetCB           queueGetStats;

   /** callback to toggle queue feature */
   vmk_UplinkQueueFeatureToggleCB      queueToggleFeature;

   /** callback to set queue priority */
   vmk_UplinkQueueTxPrioritySetCB      queueSetPriority;

   /** callback to set coalesce parameters */
   vmk_UplinkQueueCoalesceParamsSetCB  queueSetCoalesceParams;
} vmk_UplinkQueueOps;


/** UplinkQueue dynamic RSS operations */
typedef struct vmk_UplinkQueueRSSDynOps{
   /** callback to get RSS parameters */
   vmk_UplinkQueueRSSParamsGetCB       queueGetRSSParams;

   /** callback to init RSS state */
   vmk_UplinkQueueRSSStateInitCB       queueInitRSSState;

   /** callback to update RSS indirection table */
   vmk_UplinkQueueRSSIndTableUpdateCB  queueUpdateRSSIndTable;

   /** callback to get RSS indirection table */
   vmk_UplinkQueueRSSIndTableGetCB     queueGetRSSIndTable;
} vmk_UplinkQueueRSSDynOps;


/**
 * Shared data of a single queue
 */
typedef struct vmk_UplinkSharedQueueData {
   /** queue flags */
   volatile vmk_UplinkQueueFlags flags;

   /** queue type */
   vmk_UplinkQueueType           type;

   /** queue ID */
   vmk_UplinkQueueID             qid;

   /** queue state */
   volatile vmk_UplinkQueueState state;

   /** queue supported features */
   vmk_UplinkQueueFeature        supportedFeatures;

   /** queue active features bit vector */
   vmk_UplinkQueueFeature        activeFeatures;

   /** maximum filters supported */
   vmk_uint32                    maxFilters;

   /** number of active filters */
   vmk_uint32                    activeFilters;

   /** Associated vmk_NetPoll context */
   vmk_NetPoll                   poll;

   /** Associated DMA engine for allocation constraints */
   vmk_DMAEngine                 dmaEngine;

   /** Tx priority assigned to queue */
   vmk_UplinkQueuePriority       priority;

   /** Queue level interrupt coalescing parameters */
   vmk_UplinkCoalesceParams      coalesceParams;
} vmk_UplinkSharedQueueData;


/**
 * Shared information of all queues on a driver
 */
typedef struct vmk_UplinkSharedQueueInfo {
   /** supported queue types */
   vmk_UplinkQueueType         supportedQueueTypes;

   /** supported queue filter classes */
   vmk_UplinkQueueFilterClass  supportedRxQueueFilterClasses;

   /** default Rx queue ID */
   vmk_UplinkQueueID           defaultRxQueueID;

   /** default Tx queue ID */
   vmk_UplinkQueueID           defaultTxQueueID;

   /** maximum Rx queues */
   vmk_uint32                  maxRxQueues;

   /** maximum Tx queues */
   vmk_uint32                  maxTxQueues;

   /** number of active Rx queues */
   vmk_uint32                  activeRxQueues;

   /** number of active Tx queues */
   vmk_uint32                  activeTxQueues;

   /** active queue bit vector */
   vmk_BitVector              *activeQueues;

   /** maximum/total filters supported across all the queues of device */
   vmk_uint32                  maxTotalDeviceFilters;

   /**
    * shared queue data. Drivers that do not support multiple queues
    * still need to populate one queue
    */
   vmk_UplinkSharedQueueData  *queueData;
} vmk_UplinkSharedQueueInfo;


/*
 ***********************************************************************
 * vmk_UplinkQueueInvalidate --                                   */ /**
 *
 * \brief Invalidate a queue in an uplink
 *
 * The driver can ask the kernel to invalidate a queue. What this means
 * is that all the filters on that queue are assumed to be removed, all
 * the queue features are reset and the queue is free'd.
 *
 * \param[in]   uplink            Uplink handle
 * \param[in]   qID               queue id to be invalidated
 *
 * \retval      VMK_OK            if the reset call succeeds
 * \retval      VMK_FAILURE       if the uplink doesn't support reset
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkQueueInvalidate(vmk_Uplink uplink,
                                           vmk_UplinkQueueID qID);


/*
 ***********************************************************************
 * vmk_UplinkQueueStop --                                         */ /**
 *
 * \brief Notify stack of uplink queue stop
 *
 * \param[in]  uplink        Uplink aimed
 * \param[in]  qid           Queue ID
 *
 * \retval     VMK_OK
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkQueueStop(vmk_Uplink uplink,
                                     vmk_UplinkQueueID qid);


/*
 ***********************************************************************
 * vmk_UplinkQueueStart --                                        */ /**
 *
 * \brief Notify stack of uplink queue (re)start
 *
 * \param[in]  uplink        Uplink aimed
 * \param[in]  qid           Queue ID
 *
 * \retval     VMK_OK
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkQueueStart(vmk_Uplink uplink,
                                      vmk_UplinkQueueID qid);


/*
 ***********************************************************************
 * vmk_UplinkQueueRegisterFeatureOps --                           */ /**
 *
 * \brief Register callback operations for a specific queue feature
 *
 * Some feature requires a set of callback operations. Driver needs to
 * register these callbacks before claiming it supports this feature.
 *
 * \param[in]   uplink            Uplink handle
 * \param[in]   feature           The uplink queue feature to support
 * \param[in]   ops               Pointer to the operations associated
 *                                to this feature
 *
 * \retval      VMK_OK            if the reset call succeeds
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_UplinkQueueRegisterFeatureOps(vmk_Uplink uplink,
                                                   vmk_UplinkQueueFeature feature,
                                                   void *ops);

#endif /* _VMKAPI_NET_QUEUE_H_ */
/** @} */
/** @} */
