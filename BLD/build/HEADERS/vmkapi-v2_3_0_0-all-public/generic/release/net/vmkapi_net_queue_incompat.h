/* **********************************************************
 * Copyright 2006 - 2009 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Netqueue                                                       */ /**
 * \addtogroup Network
 *@{
 * \defgroup Netqueue Incompatible NetQueue
 *@{
 *
 ***********************************************************************
 */

#ifndef _VMKAPI_NET_QUEUE_INCOMPAT_H_
#define _VMKAPI_NET_QUEUE_INCOMPAT_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "net/vmkapi_net_types.h"
#include "net/vmkapi_net_poll.h"
#include "net/vmkapi_net_pkt.h"


/** Default queue ID */
#define VMK_NETQUEUE_DEFAULT_QUEUEID   VMK_UPLINK_QUEUE_DEFAULT_QUEUEID

/** Invalid queue ID */
#define VMK_NETQUEUE_INVALID_QUEUEID   VMK_UPLINK_QUEUE_INVALID_QUEUEID

/** Invalid filter ID */
#define VMK_NETQUEUE_INVALID_FILTERID  VMK_UPLINK_QUEUE_INVALID_FILTERID

/** Major version of netqueue api */
#define VMK_NETQUEUE_OPS_MAJOR_VER     (2)

/** Minor version of netqueue api */
#define VMK_NETQUEUE_OPS_MINOR_VER     (0)

/** Definition of RSS maximum number of queues */
#define VMK_NETQUEUE_MAX_RSS_QUEUES    VMK_UPLINK_QUEUE_MAX_RSS_QUEUES

/** Definition of RSS indirection table maximum size */
#define VMK_NETQUEUE_MAX_RSS_IND_TABLE_SIZE  VMK_UPLINK_QUEUE_MAX_RSS_IND_TABLE_SIZE

/** Definition of RSS hash key maximum size */
#define VMK_NETQUEUE_MAX_RSS_KEY_SIZE  VMK_UPLINK_QUEUE_MAX_RSS_KEY_SIZE


/**
 * \brief Netqueue queue ID
 */
typedef vmk_UplinkQueueID vmk_NetqueueQueueID;


/**
 * \brief Netqueue filter ID
 */
typedef vmk_UplinkQueueFilterID vmk_NetqueueFilterID;


/** Netqueue priority */
typedef vmk_UplinkQueuePriority vmk_NetqueuePriority;


/** Netqueue features supported */
typedef enum vmk_NetqueueFeatures {

   /** No features supported */
   VMK_NETQUEUE_FEATURE_NONE     = 0x0,

   /** Rx queues features supported */
   VMK_NETQUEUE_FEATURE_RXQUEUES = 0x1,

   /** Tx queues features supported */
   VMK_NETQUEUE_FEATURE_TXQUEUES = 0x2,
} vmk_NetqueueFeatures;


/**
 * \brief Netqueue filter type
 */
typedef enum vmk_NetqueueFilterType {

   /** Invalid filter type */
   VMK_NETQUEUE_FILTER_TYPE_INVALID  = VMK_UPLINK_QUEUE_FILTER_TYPE_INVALID,

   /** Invalid filter type */
   VMK_NETQUEUE_FILTER_TYPE_TXRX     = VMK_UPLINK_QUEUE_FILTER_TYPE_TXRX,
} vmk_NetqueueFilterType;


/**
 * \brief Netqueue filter class
 */
typedef enum vmk_NetqueueFilterClass {
   /** Invalid filter */
   VMK_NETQUEUE_FILTER_CLASS_NONE       = VMK_UPLINK_QUEUE_FILTER_CLASS_NONE,

   /** MAC only filter */
   VMK_NETQUEUE_FILTER_CLASS_MAC_ONLY   = VMK_UPLINK_QUEUE_FILTER_CLASS_MAC_ONLY,

   /** VLAN only filter */
   VMK_NETQUEUE_FILTER_CLASS_VLAN_ONLY  = VMK_UPLINK_QUEUE_FILTER_CLASS_VLAN_ONLY,

   /** VLAN + MAC address filter */
   VMK_NETQUEUE_FILTER_CLASS_VLANMAC    = VMK_UPLINK_QUEUE_FILTER_CLASS_VLANMAC,

   /** VXLAN filter */
   VMK_NETQUEUE_FILTER_CLASS_VXLAN      = VMK_UPLINK_QUEUE_FILTER_CLASS_VXLAN,
} vmk_NetqueueFilterClass;


/**
 * Netqueue queue type
 */
typedef enum vmk_NetqueueQueueType {
   /** Invalid queue type */
   VMK_NETQUEUE_QUEUE_TYPE_INVALID   = VMK_UPLINK_QUEUE_TYPE_INVALID,

   /** RX queue type */
   VMK_NETQUEUE_QUEUE_TYPE_RX        = VMK_UPLINK_QUEUE_TYPE_RX,

   /** TX queue type */
   VMK_NETQUEUE_QUEUE_TYPE_TX        = VMK_UPLINK_QUEUE_TYPE_TX,
} vmk_NetqueueQueueType;


/**
 * Netqueue queue flags
 */
typedef enum vmk_NetqueueQueueFlags {
   /** queue is unused */
   VMK_NETQUEUE_QUEUE_FLAG_UNUSED    = VMK_UPLINK_QUEUE_FLAG_UNUSED,

   /** queue is allocated and in use */
   VMK_NETQUEUE_QUEUE_FLAG_IN_USE    = VMK_UPLINK_QUEUE_FLAG_IN_USE,

   /** queue is the default queue */
   VMK_NETQUEUE_QUEUE_FLAG_DEFAULT   = VMK_UPLINK_QUEUE_FLAG_DEFAULT,
} vmk_NetqueueQueueFlags;


/**
 * Netqueue queue state
 */
typedef enum  vmk_NetqueueQueueState {
   /** Queue stopped by the driver */
   VMK_NETQUEUE_STATE_STOPPED  = VMK_UPLINK_QUEUE_STATE_STOPPED,

   /** Queue started administratively by the networking stack */
   VMK_NETQUEUE_STATE_STARTED  = VMK_UPLINK_QUEUE_STATE_STARTED,
} vmk_NetqueueQueueState;


/**
 * Netqueue queue feature
 */
typedef enum vmk_NetqueueQueueFeature {
   /** None */
   VMK_NETQUEUE_QUEUE_FEAT_NONE     = VMK_UPLINK_QUEUE_FEAT_NONE,

   /** Supports setting queue priority */
   VMK_NETQUEUE_QUEUE_FEAT_SET_PRIO = VMK_UPLINK_QUEUE_FEAT_SET_PRIO,

   /** Supports setting queue level intr coalescing parameters */
   VMK_NETQUEUE_QUEUE_FEAT_COALESCE = VMK_UPLINK_QUEUE_FEAT_COALESCE,

   /** Supports setting queue large receive offload(LRO) feature */
   VMK_NETQUEUE_QUEUE_FEAT_LRO      = VMK_UPLINK_QUEUE_FEAT_LRO,

   /** Supports setting queue receive segment scaling (RSS) feature */
   VMK_NETQUEUE_QUEUE_FEAT_RSS      = VMK_UPLINK_QUEUE_FEAT_RSS,

   /** Paired queue feature */
   VMK_NETQUEUE_QUEUE_FEAT_PAIR     = VMK_UPLINK_QUEUE_FEAT_PAIR,

   /** Supports modification of RSS indirection table at run time*/
   VMK_NETQUEUE_QUEUE_FEAT_RSS_DYN  = VMK_UPLINK_QUEUE_FEAT_RSS_DYN,

   /** Latency Sensitive queue feature */
   VMK_NETQUEUE_QUEUE_FEAT_LATENCY     = VMK_UPLINK_QUEUE_FEAT_LATENCY,

   /** Dynamic queue feature */
   VMK_NETQUEUE_QUEUE_FEAT_DYNAMIC     = VMK_UPLINK_QUEUE_FEAT_DYNAMIC,

   /** Pre-emptible queue feature */
   VMK_NETQUEUE_QUEUE_FEAT_PREEMPTIBLE     = VMK_UPLINK_QUEUE_FEAT_PREEMPTIBLE,
} vmk_NetqueueQueueFeature;


/**
 * \brief Filter definition
 */
typedef vmk_UplinkQueueFilter vmk_NetqueueFilter;


/**
 * \brief MAC address only filter info
 */
typedef vmk_UplinkQueueMACFilterInfo vmk_NetqueueMACFilterInfo;


/**
 * \brief VLAN ID only filter info
 */
typedef vmk_UplinkQueueVLANFilterInfo vmk_NetqueueVLANFilterInfo;


/**
 * \brief MAC address + VLAN ID filter info
 */
typedef vmk_UplinkQueueVLANMACFilterInfo vmk_NetqueueVLANMACFilterInfo;


/**
 * \brief VXLAN filter info
 */
typedef vmk_UplinkQueueVXLANFilterInfo vmk_NetqueueVXLANFilterInfo;


/**
 * \brief Filter properties
 */
typedef enum vmk_NetqueueFilterProperties {
   /** None */
   VMK_NETQUEUE_FILTER_PROP_NONE  = VMK_UPLINK_QUEUE_FILTER_PROP_NONE,

   /** Management filter */
   VMK_NETQUEUE_FILTER_PROP_MGMT  = VMK_UPLINK_QUEUE_FILTER_PROP_MGMT,

   /** Opportunistically packed with other filters */
   VMK_NETQUEUE_FILTER_PROP_PACK_OPPO  = VMK_UPLINK_QUEUE_FILTER_PROP_PACK_OPPO,

   /** Opportunistically seek exclusive netqueue */
   VMK_NETQUEUE_FILTER_PROP_EXCL_OPPO  = VMK_UPLINK_QUEUE_FILTER_EXCL_PACK_OPPO,
} vmk_NetqueueFilterProperties;


/**
 * \brief Netqueue queue attribute type
 */
typedef enum vmk_NetqueueQueueAttrType {

   /** Priority attribute */
   VMK_NETQUEUE_QUEUE_ATTR_PRIOR  = VMK_UPLINK_QUEUE_ATTR_PRIOR,

   /** Features attribute */
   VMK_NETQUEUE_QUEUE_ATTR_FEAT   = VMK_UPLINK_QUEUE_ATTR_FEAT,

   /** Number of attributes */
   VMK_NETQUEUE_QUEUE_ATTR_NUM    = 2,
} vmk_NetqueueQueueAttrType;

/**
 * \brief Netqueue queue attribute
 */
typedef vmk_UplinkQueueAttr vmk_NetqueueQueueAttr;


/*
 ***********************************************************************
 * vmk_NetqueueSetQueueIDUserVal --                               */ /**
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

static inline VMK_ReturnStatus
vmk_NetqueueSetQueueIDUserVal(vmk_NetqueueQueueID *qid,
                              vmk_uint32 userval)
{
   return vmk_UplinkQueueSetQueueIDUserVal(qid, userval);
}


/*
 ***********************************************************************
 * vmk_NetqueueQueueIDUserVal --                                  */ /**
 *
 * \brief Get user part of queue ID
 *
 * \param[in] qid           Queue ID.
 *
 * \retval    vmk_uint32    User value.
 *
 ***********************************************************************
 */

static inline vmk_uint32
vmk_NetqueueQueueIDUserVal(vmk_NetqueueQueueID qid)
{
   return vmk_UplinkQueueIDUserVal(qid);
}


/*
 ***********************************************************************
 * vmk_NetqueueSetQueueIDQueueDataIndex --                        */ /**
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

static inline VMK_ReturnStatus
vmk_NetqueueSetQueueIDQueueDataIndex(vmk_NetqueueQueueID *qid,
                                     vmk_uint32 index)
{
   return vmk_UplinkQueueSetQueueIDQueueDataIndex(qid, index);
}


/*
 ***********************************************************************
 * vmk_NetqueueIDQueueDataIndex --                                */ /**
 *
 * \brief Get queue data array index part of queue ID
 *
 * \param[in] qid           Queue ID.
 *
 * \retval    vmk_uint32    Queue data array index
 *
 ***********************************************************************
 */

static inline vmk_uint32
vmk_NetqueueIDQueueDataIndex(vmk_NetqueueQueueID qid)
{
   return vmk_UplinkQueueIDQueueDataIndex(qid);
}


/*
 ***********************************************************************
 * vmk_NetqueueMkTxQueueID --                                     */ /**
 *
 * \brief Create a Tx queue ID.
 *
 * \param[out] qid       The created queue ID.
 * \param[in]  index     The index into queue data array in shared data
 * \param[in]  val       The embedded value of the queue ID.
 *
 * \retval     VMK_OK    Always.
 *
 ***********************************************************************
 */

static inline VMK_ReturnStatus
vmk_NetqueueMkTxQueueID(vmk_NetqueueQueueID *qid,
                        vmk_uint32 index,
                        vmk_uint32 val)
{
   return vmk_UplinkQueueMkTxQueueID(qid, index, val);
}


/*
 ***********************************************************************
 * vmk_NetqueueMkRxQueueID --                                     */ /**
 *
 * \brief Create a Rx queue ID.
 *
 * \param[out] qid       The created queue ID.
 * \param[in]  index     The index into queue data array in shared data
 * \param[in]  val       The embedded value of the queue ID.
 *
 * \retval     VMK_OK    Always.
 *
 ***********************************************************************
 */

static inline VMK_ReturnStatus
vmk_NetqueueMkRxQueueID(vmk_NetqueueQueueID *qid,
                        vmk_uint32 index,
                        vmk_uint32 val)
{
   return vmk_UplinkQueueMkRxQueueID(qid, index, val);
}


/*
 ***********************************************************************
 * vmk_NetqueueQueueIDVal --                                      */ /**
 *
 * \brief Retrieve the embedded value of a queue ID.
 *
 * \param[in] qid       The aimed queue ID.
 *
 * \return              The embedded value.
 *
 ***********************************************************************
 */

static inline vmk_uint32
vmk_NetqueueQueueIDVal(vmk_NetqueueQueueID qid)
{
   return vmk_UplinkQueueIDVal(qid);
}


/*
 ***********************************************************************
 * vmk_NetqueueQueueIDType --                                     */ /**
 *
 * \brief Retrieve the type of a queue ID.
 *
 * \param[in] qid       The aimed queue ID.
 *
 * \return              The type.
 *
 ***********************************************************************
 */

static inline vmk_NetqueueQueueType
vmk_NetqueueQueueIDType(vmk_NetqueueQueueID qid)
{
   return vmk_UplinkQueueIDType(qid);
}


/*
 ***********************************************************************
 * vmk_NetqueueMkFilterID --                                      */ /**
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

static inline VMK_ReturnStatus
vmk_NetqueueMkFilterID(vmk_NetqueueFilterID *fid,
                       vmk_uint16 val)
{
   return vmk_UplinkQueueMkFilterID(fid, val);
}


/*
 ***********************************************************************
 * vmk_NetqueueFilterIDVal --                                     */ /**
 *
 * \brief Retrieve the embedded value of a filter ID.
 *
 * \param[in] fid       The aimed filter ID.
 *
 * \return              The embedded value.
 *
 ***********************************************************************
 */

static inline vmk_uint16
vmk_NetqueueFilterIDVal(vmk_NetqueueFilterID fid)
{
   return vmk_UplinkQueueFilterIDVal(fid);
}


/**
 * \brief Netqueue operations
 */
typedef enum vmk_NetqueueOp {

   /**
    * \brief None
    */
   VMK_NETQUEUE_OP_NONE                     = 0x0,

   /**
    * \brief Get version
    * Get the device's netqueue version.
    */
   VMK_NETQUEUE_OP_GET_VERSION              = 0x1,

   /**
    * \brief Get features
    * Get the device's netqueue features.
    */
   VMK_NETQUEUE_OP_GET_FEATURES             = 0x2,

   /**
    * \brief Get queue count
    * Get the number of Rx/Tx queues supported on a device.
    */
   VMK_NETQUEUE_OP_QUEUE_COUNT              = 0x3,

   /**
    * \brief Get filter count
    * Get the number of filters supported per queue.
    */
   VMK_NETQUEUE_OP_FILTER_COUNT             = 0x4,

   /**
    * \brief Allocate a queue
    * Allocate a queue of a specific type.
    */
   VMK_NETQUEUE_OP_ALLOC_QUEUE              = 0x5,

   /**
    * \brief Release a queue
    * Release an allocated queue.
    */
   VMK_NETQUEUE_OP_FREE_QUEUE               = 0x6,

   /**
    * \brief Get queue interrupt cookie
    * Get the interrupt cookie associated to a queue.
    */
   VMK_NETQUEUE_OP_GET_QUEUE_INTERRUPT      = 0x7,

   /**
    * \brief Get default queue
    * Get the Rx/Tx default queue.
    */
   VMK_NETQUEUE_OP_GET_DEFAULT_QUEUE        = 0x8,

   /**
    * \brief Apply rx filter
    * Apply a filter to a Rx queue.
    */
   VMK_NETQUEUE_OP_APPLY_RX_FILTER          = 0x9,

   /**
    * \brief Remove rx filter
    * Remove a filter from a Rx queue.
    */
   VMK_NETQUEUE_OP_REMOVE_RX_FILTER         = 0xa,

   /**
    * \brief Get queue stats
    * Get stats on a particular queue.
    */
   VMK_NETQUEUE_OP_GET_QUEUE_STATS          = 0xb,

   /**
    * \brief Set Tx priority
    * Set a priority on a particular Tx queue.
    */
   VMK_NETQUEUE_OP_SET_TX_PRIORITY          = 0xc,

   /**
    * \brief Get/Set queue state
    * Get and Set the state of netqueue on a particular device.
    * Changing the mtu on a device puts netqueue in an invalid state
    * in which the Rx filters are removed. Such state needs to
    * be notified in order to restore the configuration before invalidation.
    */
   VMK_NETQUEUE_OP_GETSET_QUEUE_STATE       = 0xd,

   /**
    * \brief Set queue interrupt cookie
    * Set the queue interrupt cookie associated to a queue.
    */
   VMK_NETQUEUE_OP_SET_QUEUE_INTERRUPT      = 0xe,

   /**
    * \brief Allocate a queue with attributes
    * Allocate a queue of a specific type with attributes.
    */
   VMK_NETQUEUE_OP_ALLOC_QUEUE_WITH_ATTR    = 0xf,

   /**
    * \brief Enable queue's feature
    * Enable a feature on a given queue.
    */
   VMK_NETQUEUE_OP_ENABLE_QUEUE_FEAT        = 0x10,

   /**
    * \brief Disable queue's feature
    * Disable a feature on a given queue.
    */
   VMK_NETQUEUE_OP_DISABLE_QUEUE_FEAT       = 0x11,

   /**
    * \brief Get queue's features
    * Get the supported queues' features of a device.
    */
   VMK_NETQUEUE_OP_GET_QUEUE_SUPPORTED_FEAT = 0x12,

   /**
    * \brief Get queue filter required class
    * Get the supported queue's filter class
    */
   VMK_NETQUEUE_OP_GET_QUEUE_SUPPORTED_FILTER_CLASS = 0x13,

   /**
    * \brief Reallocate a queue 
    * Free already allocated queue and reallocate of specific type.
    */
   VMK_NETQUEUE_OP_REALLOC_QUEUE_WITH_ATTR            = 0x14,

   /**
    * \brief RSS config operation
    * Perform an RSS specific operation
    */
   VMK_NETQUEUE_OP_CONFIG_RSS               = 0x15,

   /**
    * \brief Get filters count of device
    * Get the number of filters supported per device.
    */
   VMK_NETQUEUE_OP_FILTER_COUNT_OF_DEVICE   = 0x16,
} vmk_NetqueueOp;

/**
 * \ingroup Netqueue
 * \brief RSS Op types
 */
typedef enum vmk_NetqueueRSSOpType {
   /**
    * \brief Get RSS params
    * Get the RSS parameters from the device
    */
   VMK_NETQUEUE_RSS_OP_GET_PARAMS            = 0x1,

   /**
    * \brief Init RSS
    * Initialize RSS state (key + table) on the device
    */
   VMK_NETQUEUE_RSS_OP_INIT_STATE            = 0x2,

   /**
    * \brief Update RSS redirection table
    */
   VMK_NETQUEUE_RSS_OP_UPDATE_IND_TABLE      = 0x3,

   /**
    * \brief Get RSS redirection table
    */
   VMK_NETQUEUE_RSS_OP_GET_IND_TABLE         = 0x4,
} vmk_NetqueueRSSOpType;


/**
 * \brief Arguments to VMK_NETQUEUE_OP_GET_VERSION
 */
typedef struct vmk_NetqueueOpGetVersionArgs {

   /** Minor version [out] */
   vmk_uint16 minor;

   /** Major version [out] */
   vmk_uint16 major;
} vmk_NetqueueOpGetVersionArgs;


/**
 * \brief Arguments to VMK_NETQUEUE_OP_GET_FEATURES
 */
typedef struct vmk_NetqueueOpGetFeaturesArgs {

   /** Supported features [out] */
   vmk_NetqueueFeatures features;
} vmk_NetqueueOpGetFeaturesArgs;


/**
 * \brief Arguments to VMK_NETQUEUE_OP_QUEUE_COUNT
 */
typedef struct vmk_NetqueueOpGetQueueCountArgs {

   /** Queue type aimed [in] */
   vmk_NetqueueQueueType qtype;

   /** Number of queue of this type [out] */
   vmk_uint16 count;
} vmk_NetqueueOpGetQueueCountArgs;


/**
 * \brief Arguments to VMK_NETQUEUE_OP_FILTER_COUNT
 */
typedef struct vmk_NetqueueOpGetFilterCountArgs {

   /** Queue type aimed [in] */
   vmk_NetqueueQueueType qtype;

   /** Number of filters per queue of this type [out] */
   vmk_uint16 count;
} vmk_NetqueueOpGetFilterCountArgs;

/**
 * \brief Arguments to VMK_NETQUEUE_OP_FILTER_COUNT_OF_DEVICE
 */
typedef struct vmk_NetqueueOpGetFilterCountOfDeviceArgs {

   /** Queue type aimed [in] */
   vmk_NetqueueQueueType qtype;

   /** Number of total filters supported per device [out] */
   vmk_uint16 filtersOfDeviceCount;

   /** Number of max filters per queue [out] */
   vmk_uint16 filtersPerQueueCount;
} vmk_NetqueueOpGetFilterCountOfDeviceArgs;


/**
 * \brief Arguments to VMK_NETQUEUE_OP_ALLOC_QUEUE
 */
typedef struct vmk_NetqueueOpAllocQueueArgs {

   /** Queue type aimed [in] */
   vmk_NetqueueQueueType qtype;

   /** Queue ID of the allocated queue [out] */
   vmk_NetqueueQueueID qid;

   /** Net poll on top of the allocated queue if any [out] */
   vmk_NetPoll net_poll;
} vmk_NetqueueOpAllocQueueArgs;


/**
 * \brief Arguments to VMK_NETQUEUE_OP_FREE_QUEUE
 */
typedef struct vmk_NetqueueOpFreeQueueArgs {

   /** Queue to release [in] */
   vmk_NetqueueQueueID qid;
} vmk_NetqueueOpFreeQueueArgs;


/**
 * \brief Arguments to VMK_NETQUEUE_OP_GET_QUEUE_INTERRUPT
 */
typedef struct vmk_NetqueueOpGetQueueInterruptArgs {

   /** Queue aimed [in] */
   vmk_NetqueueQueueID qid;

   /** Interrupt cookie associated to the queue [out] */
   vmk_IntrCookie intrCookie;
} vmk_NetqueueOpGetQueueInterruptArgs;


/**
 * \brief Arguments to VMK_NETQUEUE_OP_GET_DEFAULT_QUEUE
 */
typedef struct vmk_NetqueueOpGetDefaultQueueArgs {

   /** Default queue type VMK_NETQUEUE_TYPE_[RX|TX] [in] */
   vmk_NetqueueQueueType qtype;

   /** Queue ID of the default queue [out] */
   vmk_NetqueueQueueID qid;

   /** Net poll on top of the default queue if any [out] */
   vmk_NetPoll net_poll;
} vmk_NetqueueOpGetDefaultQueueArgs;


/**
 * \brief Arguments to VMK_NETQUEUE_OP_APPLY_RX_FILTER
 */
typedef struct vmk_NetqueueOpApplyRxFilterArgs {

   /** Queue aimed [in] */
   vmk_NetqueueQueueID qid;

   /** Filter to be applied [in] */
   vmk_NetqueueFilter filter;

   /** Filter ID [out] */
   vmk_NetqueueFilterID fid;

   /**
    * Potential Paired tx queue hardware index [out]
    * pairHWQID is hardware's internal TX queue identifier, it must
    * be same as the embedded value passed to vmk_UplinkQueueMkTxQueueID
    * when a TX queue is allocated.
    */
   vmk_uint32 pairHWQID;
} vmk_NetqueueOpApplyRxFilterArgs;


/**
 * \brief Arguments to VMK_NETQUEUE_OP_REMOVE_RX_FILTER
 */
typedef struct vmk_NetqueueOpRemoveRxFilterArgs {

   /** Queue aimed [in] */
   vmk_NetqueueQueueID qid;

   /** Filter to be removed [in] */
   vmk_NetqueueFilterID fid;
} vmk_NetqueueOpRemoveRxFilterArgs;


/**
 * \brief Arguments to VMK_NETQUEUE_OP_GET_QUEUE_STATS
 */
typedef struct vmk_NetqueueOpGetQueueStatsArgs {

   /** Queue aimed [in] */
   vmk_NetqueueQueueID qid;
} vmk_NetqueueOpGetQueueStatsArgs;


/**
 * \brief Arguments to VMK_NETQUEUE_OP_SET_TX_PRIORITY
 */
typedef struct vmk_NetqueueOpSetTxPriorityArgs {

   /** Queue aimed [in] */
   vmk_NetqueueQueueID qid;

   /** Priority to set on the queue [in] */
   vmk_VlanPriority priority;
} vmk_NetqueueOpSetTxPriorityArgs;


/**
 * \brief Arguments to VMK_NETQUEUE_OP_GETSET_QUEUE_STATE
 */
typedef struct vmk_NetqueueOpGetSetQueueStateArgs {

   /** Netqueue old state (TRUE: Netqueue was in valid state, FALSE: otherwise) [out] */
   vmk_Bool oldState;

   /** Netqueue new state (TRUE: Netqueue set to valid state, FALSE: otherwise) [in] */
   vmk_Bool newState;
} vmk_NetqueueOpGetSetQueueStateArgs;


/**
 * \brief Arguments to VMK_NETQUEUE_OP_SET_QUEUE_INTERRUPT
 */
typedef struct vmk_NetqueueOpSetQueueInterruptArgs {

   /** Queue aimed [in] */
   vmk_NetqueueQueueID qid;

   /** Interrupt cookie to be set [in] */
   vmk_IntrCookie intrCookie;
} vmk_NetqueueOpSetQueueInterruptArgs;


/**
 * \brief Arguments to VMK_NETQUEUE_OP_ALLOC_QUEUE_WITH_ATTR
 */
typedef struct vmk_NetqueueOpAllocQueueWithAttrArgs {

   /** Queue type aimed [in] */
   vmk_NetqueueQueueType qtype;

   /** Number of attributes (Cannot be greater than VMK_NETQUEUE_ATTR_NUM) [in] */
   vmk_uint16 nattr;

   /** Queue attributes [in] */
   vmk_NetqueueQueueAttr *attr;

   /** Queue ID of the allocated queue [out] */
   vmk_NetqueueQueueID qid;

   /** Net poll on top of the allocated queue if any [out] */
   vmk_NetPoll net_poll;
} vmk_NetqueueOpAllocQueueWithAttrArgs;


/**
 * \brief Arguments to VMK_NETQUEUE_OP_ENABLE_QUEUE_FEAT
 */
typedef struct vmk_NetqueueOpEnableQueueFeatArgs {

   /** Queue aimed [in] */
   vmk_NetqueueQueueID qid;

   /** Features to be enabled [in] */
   vmk_NetqueueQueueFeature features;
} vmk_NetqueueOpEnableQueueFeatArgs;


/**
 * \brief Arguments to VMK_NETQUEUE_OP_DISABLE_QUEUE_FEAT
 */
typedef struct vmk_NetqueueOpDisableQueueFeatArgs {

   /** Queue aimed [in] */
   vmk_NetqueueQueueID qid;

   /** Features to be disabled [in] */
   vmk_NetqueueQueueFeature features;
} vmk_NetqueueOpDisableQueueFeatArgs;


/**
 * \brief Arguments to VMK_NETQUEUE_OP_GET_QUEUE_SUPPORTED_FEAT
 */
typedef struct vmk_NetqueueOpGetQueueSupFeatArgs {

   /** Queue type aimed [in] */
   vmk_NetqueueQueueType qtype;

   /** Features supported [out] */
   vmk_NetqueueQueueFeature features;
} vmk_NetqueueOpGetQueueSupFeatArgs;


/**
 * \brief Arguments to VMK_NETQUEUE_OP_GET_FILTER_CLASS
 */
typedef struct vmk_NetqueueOpGetQueueSupFilterArgs {

   /** Queue type aimed [in] */
   vmk_NetqueueQueueType qtype;

   /** Filter type required [out] */
   vmk_NetqueueFilterClass class;
} vmk_NetqueueOpGetQueueSupFilterArgs;

/**
 * \ingroup Netqueue
 * \brief Type for RSS hash key.
 *
 * This structure is used to program the RSS hash key.
 */
typedef vmk_UplinkQueueRSSHashKey vmk_NetqueueRSSHashKey;

/**
 * \ingroup Netqueue
 * \brief Type for RSS indirection table.
 *
 * This structure is used to program or update the RSS indirection
 * table.
 */
typedef vmk_UplinkQueueRSSIndTable vmk_NetqueueRSSIndTable;

/**
 * \ingroup Netqueue
 * \brief Arguments to VMK_NETQUEUE_RSS_OP_GET_PARAMS
 *
 */
typedef vmk_UplinkQueueRSSParams vmk_NetqueueRSSOpGetParamsArgs;

/**
 * \ingroup Netqueue
 * \brief Arguments for VMK_NETQUEUE_RSS_OP_INIT_STATE
 *
 */
typedef struct vmk_NetqueueRSSOpInitStateArgs {
   /** Hash key */
   vmk_NetqueueRSSHashKey *rssKey;

   /** Indirection table */
   vmk_NetqueueRSSIndTable *rssIndTable;
} vmk_NetqueueRSSOpInitStateArgs;


/**
 * \ingroup Netqueue
 * \brief Arguments for VMK_NETQUEUE_OP_CONFIG_RSS
 */
typedef struct vmk_NetqueueOpConfigRSSArgs {
   /** RSS Op type */
   vmk_NetqueueRSSOpType      opType;

   /** RSS Op specific args */
   void                      *opArgs;
} vmk_NetqueueOpConfigRSSArgs;

/** 
 * \ingroup Netqueue
 * \brief Arguments to VMK_NETQUEUE_OP_REALLOC_QUEUE_WITH_ATTR
 */
typedef struct vmk_NetqueueOpReAllocQueueWithAttrArgs {

   /** args for alloc queue [in] */
   struct vmk_NetqueueOpAllocQueueWithAttrArgs  *allocArgs;

   /** Number of filters to be removed from the queue [in] */
   vmk_uint16 rmFilterCount;
   
   /** args for remove filter [in] */
   vmk_NetqueueOpRemoveRxFilterArgs    *rmFilterArgs;

   /** args for apply filter [in] */
   vmk_NetqueueOpApplyRxFilterArgs     *applyRxFilterArgs;
} vmk_NetqueueOpReAllocQueueWithAttrArgs;


/*
 ***********************************************************************
 * vmk_UplinkQueueFlagsGet --                                     */ /**
 *
 * \ingroup UplinkQueue
 * \brief  Get uplink queue flags
 *
 * \param[in]   uplink                 Uplink handle
 * \param[in]   qID                    Capability
 *
 * \retval      vmk_UplinkQueueFlags   refer to definition of enum
 *                                     vmk_UplinkQueueFlags
 *
 ***********************************************************************
 */
vmk_UplinkQueueFlags vmk_UplinkQueueFlagsGet(vmk_Uplink uplink,
                                             vmk_UplinkQueueID qID);

/*
 ***********************************************************************
 * vmk_UplinkQueueSupportedFeaturesGet --                         */ /**
 *
 * \ingroup UplinkQueue
 * \brief  Get uplink queue supported features
 *
 * \param[in]   uplink                    Uplink handle
 * \param[in]   qID                       queue ID
 *
 * \retval      vmk_UplinkQueueFeature    refer to the definition of
 *                                        enum vmk_UplinkQueueFeature
 *
 ***********************************************************************
 */
vmk_UplinkQueueFeature vmk_UplinkQueueSupportedFeaturesGet(vmk_Uplink uplink,
                                                           vmk_UplinkQueueID qID);


/*
 ***********************************************************************
 * vmk_UplinkQueueActiveFeaturesGet --                            */ /**
 *
 * \ingroup UplinkQueue
 * \brief  Get uplink queue active features
 *
 * \param[in]   uplink                    Uplink handle
 * \param[in]   qID                       queue ID
 *
 * \retval      vmk_UplinkQueueFeature    refer to the definition of
 *                                        enum vmk_UplinkQueueFeature
 *
 ***********************************************************************
 */
vmk_UplinkQueueFeature vmk_UplinkQueueActiveFeaturesGet(vmk_Uplink uplink,
                                                        vmk_UplinkQueueID qID);


/*
 ***********************************************************************
 * vmk_UplinkQueueMaxFiltersGet --                                */ /**
 *
 * \ingroup UplinkQueue
 * \brief  Get uplink queue max filters
 *
 * \param[in]   uplink        Uplink handle
 * \param[in]   qID           queue ID
 *
 * \retval      vmk_uint32    Integer indicating the maximum filters
 *
 ***********************************************************************
 */
vmk_uint32 vmk_UplinkQueueMaxFiltersGet(vmk_Uplink uplink,
                                        vmk_UplinkQueueID qID);


/*
 ***********************************************************************
 * vmk_UplinkQueueActiveFiltersGet --                             */ /**
 *
 * \ingroup UplinkQueue
 * \brief  Get uplink queue active filters
 *
 * \param[in]   uplink        Uplink handle
 * \param[in]   qID           queue ID
 *
 * \retval      vmk_uint32    Integer indicating the number of active
 *                            filters
 *
 ***********************************************************************
 */
vmk_uint32 vmk_UplinkQueueActiveFiltersGet(vmk_Uplink uplink,
                                           vmk_UplinkQueueID qID);


/*
 ***********************************************************************
 * vmk_UplinkQueueNetPollGet --                                   */ /**
 *
 * \ingroup UplinkQueue
 * \brief  Get uplink queue net poll
 *
 * \param[in]   uplink        Uplink handle
 * \param[in]   qID           queue ID
 *
 * \retval      NULL          if no net poll routine is registered
 * \retval      Otherwise     pointer to net poll routine
 *
 ***********************************************************************
 */
vmk_NetPoll vmk_UplinkQueueNetPollGet(vmk_Uplink uplink,
                                      vmk_UplinkQueueID qID);


/*
 ***********************************************************************
 * vmk_UplinkQueueVLANPrioGet --                                  */ /**
 *
 * \ingroup UplinkQueue
 * \brief  Get uplink queue VLAN priority
 *
 * \param[in]   uplink                    Uplink handle
 * \param[in]   qID                       queue ID
 *
 * \retval      vmk_UplinkQueuePriority   refer to the definition of
 *                                        enum vmk_UplinkQueuePriority
 *
 ***********************************************************************
 */
vmk_UplinkQueuePriority vmk_UplinkQueueVLANPrioGet(vmk_Uplink uplink,
                                                   vmk_UplinkQueueID qID);


/*
 ***********************************************************************
 * vmk_UplinkQueueSupportedTypesGet --                            */ /**
 *
 * \ingroup UplinkQueue
 * \brief  Get uplink queue supported types
 *
 * \param[in]   uplink                 Uplink handle
 *
 * \retval      vmk_UplinkQueueType    refer to the definition of enum
 *                                     vmk_UplinkQueueType
 *
 ***********************************************************************
 */
vmk_UplinkQueueType vmk_UplinkQueueSupportedTypesGet(vmk_Uplink uplink);


/*
 ***********************************************************************
 * vmk_UplinkQueueDefaultRxQIDGet --                              */ /**
 *
 * \ingroup UplinkQueue
 * \brief  Get uplink queue default Rx queue ID
 *
 * \param[in]   uplink              Uplink handle
 *
 * \retval      vmk_UplinkQueueID   refer to the definition of enum
 *                                  vmk_UplinkQueueID
 *
 ***********************************************************************
 */
vmk_UplinkQueueID vmk_UplinkQueueDefaultRxQIDGet(vmk_Uplink uplink);


/*
 ***********************************************************************
 * vmk_UplinkQueueDefaultTxQIDGet --                              */ /**
 *
 * \ingroup UplinkQueue
 * \brief  Get uplink queue default TX queue ID
 *
 * \param[in]   uplink              Uplink handle
 *
 * \retval      vmk_UplinkQueueID   refer to the definition of enum
 *                                  vmk_UplinkQueueID
 *
 ***********************************************************************
 */
vmk_UplinkQueueID vmk_UplinkQueueDefaultTxQIDGet(vmk_Uplink uplink);


/*
 ***********************************************************************
 * vmk_UplinkQueueMaxRxQueuesGet --                               */ /**
 *
 * \ingroup UplinkQueue
 * \brief  Get uplink queue max Rx queues
 *
 * \param[in]   uplink        Uplink handle
 *
 * \retval      vmk_uint32    Integer indicating the maximum RX queues
 *
 ***********************************************************************
 */
vmk_uint32 vmk_UplinkQueueMaxRxQueuesGet(vmk_Uplink uplink);


/*
 ***********************************************************************
 * vmk_UplinkQueueMaxTxQueuesGet --                               */ /**
 *
 * \ingroup UplinkQueue
 * \brief  Get uplink queue max tx queues
 *
 * \param[in]   uplink        Uplink handle
 *
 * \retval      vmk_uint32    Integer indicating the maximum Tx queues
 *
 ***********************************************************************
 */
vmk_uint32 vmk_UplinkQueueMaxTxQueuesGet(vmk_Uplink uplink);


/*
 ***********************************************************************
 * vmk_UplinkQueueActiveQueuesGet --                              */ /**
 *
 * \ingroup UplinkQueue
 * \brief  Get uplink queue active queues
 *
 * \param[in]   uplink           Uplink handle
 * \param[in]   heap             ID of heap the returned bit vector will
 *                               be allocated from.
 *
 * \retval      vmk_BitVector    Bit vector keeping indices of active
 *                               queues, caller must call
 *                               vmk_BitVectorFree to free its memory.
 *              NULL             Failed to get.
 *
 ***********************************************************************
 */
vmk_BitVector *vmk_UplinkQueueActiveQueuesGet(vmk_Uplink uplink,
                                              vmk_HeapID heap);



#endif /* _VMKAPI_NET_QUEUE_INCOMPAT_H_ */
/** @} */
/** @} */
