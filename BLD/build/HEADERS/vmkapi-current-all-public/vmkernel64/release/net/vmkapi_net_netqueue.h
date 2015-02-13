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
 * \defgroup Netqueue Netqueue
 *@{ 
 *
 ***********************************************************************
 */

#ifndef _VMKAPI_NET_NETQUEUE_H_
#define _VMKAPI_NET_NETQUEUE_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "net/vmkapi_net_types.h"
#include "net/vmkapi_net_poll.h"
#include "net/vmkapi_net_pkt.h"

/** Major version of netqueue api */
#define VMK_NETQUEUE_OPS_MAJOR_VER (2)

/** Minor version of netqueue api */
#define VMK_NETQUEUE_OPS_MINOR_VER (0)

/** 
 * \brief Netqueue queue ID
 */
typedef vmk_uint64 vmk_NetqueueQueueID;

/** Well known Netqueue queue ID values */
enum {
   /** Default queue ID */
   VMK_NETQUEUE_DEFAULT_QUEUEID =  (vmk_NetqueueQueueID)0,

   /** Invalid queue ID */
   VMK_NETQUEUE_INVALID_QUEUEID = (vmk_NetqueueQueueID)~0,
};

/** 
 * \brief Netqueue filter ID
 */
typedef vmk_uint32 vmk_NetqueueFilterID;

/** Well known Netqueue filter ID values */
enum {
   /** Invalid filter ID */
   VMK_NETQUEUE_INVALID_FILTERID = (vmk_NetqueueFilterID)~0,
};

/** Netqueue priority */
typedef vmk_VlanPriority vmk_NetqueuePriority;

/** Netqueue features supported */
typedef enum {
   
   /** No features supported */
   VMK_NETQUEUE_FEATURE_NONE     = 0x0,

   /** Rx queues features supported */
   VMK_NETQUEUE_FEATURE_RXQUEUES = 0x1,

   /** Tx queues features supported */
   VMK_NETQUEUE_FEATURE_TXQUEUES = 0x2,
} vmk_NetqueueFeatures;

/**
 * \ingroup Netqueue
 * \brief Netqueue operations
 */
typedef enum {

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
    * \brief Get queue vector 
    * Get the queue vector associated to a queue.
    */     
   VMK_NETQUEUE_OP_GET_QUEUE_VECTOR         = 0x7,

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
    * \brief Set queue vector 
    * Set the queue vector associated to a queue.
    */
   VMK_NETQUEUE_OP_SET_QUEUE_VECTOR         = 0xe,

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
} vmk_NetqueueOp;

/**
 * \ingroup Netqueue
 * \brief Filter class
 */
typedef enum vmk_NetqueueFilterClass {

   /** Invalid filter */
   VMK_NETQUEUE_FILTER_NONE            = 0x0,

   /** Mac address filter */
   VMK_NETQUEUE_FILTER_MACADDR         = 0x1,

   /** Vlan tag filter */
   VMK_NETQUEUE_FILTER_VLAN            = 0x2,

   /** Vlan tag + mac addr filter */
   VMK_NETQUEUE_FILTER_VLANMACADDR     = 0x4,
} vmk_NetqueueFilterClass;

/**
 * \ingroup Netqueue
 * \brief Queue type
 */
typedef enum {

   /** Invalid queue type */
   VMK_NETQUEUE_QUEUE_TYPE_INVALID   = 0,

   /** Rx queue type */
   VMK_NETQUEUE_QUEUE_TYPE_RX        = 1,

   /** Tx queue type */
   VMK_NETQUEUE_QUEUE_TYPE_TX        = 2,

   /** Default Rx queue type */
   VMK_NETQUEUE_QUEUE_TYPE_DEFAULTRXQUEUE = 3,

   /** Default Tx queue type */
   VMK_NETQUEUE_QUEUE_TYPE_DEFAULTTXQUEUE = 4,
} vmk_NetqueueQueueType;

/**
 * \ingroup Netqueue
 * \brief Filter type
 */
typedef enum vmk_NetqueueFilterType {

   /** Invalid filter type */
   VMK_NETQUEUE_FILTER_TYPE_INVALID = 0,

   /** Rx/Tx filter type */
   VMK_NETQUEUE_FILTER_TYPE_TXRX    = 1,
} vmk_NetqueueFilterType;

/**
 * \ingroup Netqueue
 * \brief Filter definition
 */
typedef struct vmk_NetqueueFilter {

   /** Filter class */
   vmk_NetqueueFilterClass class;
   union {

      /** Filter class mac only */
      vmk_uint8 macaddr[6];

      /** Filter class vlan tag only */
      vmk_uint16 vlan_id;

      /** Filter class vlan tag + mac */
      struct {
         vmk_uint8 macaddr[6];
         vmk_uint16 vlan_id;
      } vlanmac;
   } u;
} vmk_NetqueueFilter;

/**
 * \ingroup Netqueue
 * \brief Filter properties
 */
typedef enum vmk_NetqueueFilterProperties {

   /** None */
   VMK_NETQUEUE_FILTER_PROP_NONE = 0x0,

   /** Management filter */
   VMK_NETQUEUE_FILTER_PROP_MGMT = 0x1,
} vmk_NetqueueFilterProperties;

/**
 * \ingroup Netqueue
 * \brief Features supported on queues
 */
typedef enum vmk_NetqueueQueueFeatures {

   /** None */
   VMK_NETQUEUE_QUEUE_FEAT_NONE    = 0x0,

   /** LRO feature */
   VMK_NETQUEUE_QUEUE_FEAT_LRO     = 0x1,

   /** Paired queue feature */
   VMK_NETQUEUE_QUEUE_FEAT_PAIR    = 0x2,
} vmk_NetqueueQueueFeatures;

/**
 * \ingroup Netqueue
 * \brief Netqueue queue attribute
 */
typedef struct vmk_NetqueueQueueAttr {
   enum {

      /** Priority attribute */
      VMK_NETQUEUE_QUEUE_ATTR_PRIOR,

      /** Features attribute */
      VMK_NETQUEUE_QUEUE_ATTR_FEAT,

      /** Number of attributes */
      VMK_NETQUEUE_QUEUE_ATTR_NUM,
   } type;
   
   union {

      /** VMK_NETQUEUE_QUEUE_ATTR_PRIOR argument */
      vmk_VlanPriority priority;

      /** VMK_NETQUEUE_QUEUE_ATTR_FEAT argument */
      vmk_NetqueueQueueFeatures features;

      /** Generic attribute argument */
      void *p;
   } args;
} vmk_NetqueueQueueAttr;

/** 
 * \ingroup Netqueue
 * \brief Arguments to VMK_NETQUEUE_OP_GET_VERSION
 */
typedef struct vmk_NetqueueOpGetVersionArgs {

   /** Minor version [out] */
   vmk_uint16 minor;

   /** Major version [out] */
   vmk_uint16 major;
} vmk_NetqueueOpGetVersionArgs;

/** 
 * \ingroup Netqueue
 * \brief Arguments to VMK_NETQUEUE_OP_GET_FEATURES
 */
typedef struct vmk_NetqueueOpGetFeaturesArgs {

   /** Supported features [out] */
   vmk_NetqueueFeatures features;
} vmk_NetqueueOpGetFeaturesArgs;

/** 
 * \ingroup Netqueue
 * \brief Arguments to VMK_NETQUEUE_OP_QUEUE_COUNT
 */
typedef struct vmk_NetqueueOpGetQueueCountArgs {

   /** Queue type aimed [in] */
   vmk_NetqueueQueueType qtype; 

   /** Number of queue of this type [out] */
   vmk_uint16 count;
} vmk_NetqueueOpGetQueueCountArgs;

/** 
 * \ingroup Netqueue
 * \brief Arguments to VMK_NETQUEUE_OP_FILTER_COUNT
 */
typedef struct vmk_NetqueueOpGetFilterCountArgs {

   /** Queue type aimed [in] */
   vmk_NetqueueQueueType qtype;

   /** Number of filters per queue of this type [out] */
   vmk_uint16 count;
} vmk_NetqueueOpGetFilterCountArgs;

/** 
 * \ingroup Netqueue
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
 * \ingroup Netqueue
 * \brief Arguments to VMK_NETQUEUE_OP_FREE_QUEUE
 */
typedef struct vmk_NetqueueOpFreeQueueArgs {

   /** Queue to release [in] */
   vmk_NetqueueQueueID qid;
} vmk_NetqueueOpFreeQueueArgs;

/** 
 * \ingroup Netqueue
 * \brief Arguments to VMK_NETQUEUE_OP_GET_QUEUE_VECTOR
 */
typedef struct vmk_NetqueueOpGetQueueVectorArgs {

   /** Queue aimed [in] */
   vmk_NetqueueQueueID qid;

   /** Interrupt vector associated to the queue [out] */
   vmk_uint16 vector;
} vmk_NetqueueOpGetQueueVectorArgs;

/** 
 * \ingroup Netqueue
 * \brief Arguments to VMK_NETQUEUE_OP_GET_DEFAULT_QUEUE
 */
typedef struct vmk_NetqueueOpGetDefaultQueueArgs {

   /** Default queue type VMK_NETQUEUE_QUEUE_TYPE_[RX|TX] [in] */
   vmk_NetqueueQueueType qtype;

   /** Queue ID of the default queue [out] */
   vmk_NetqueueQueueID qid;

   /** Net poll on top of the default queue if any [out] */
   vmk_NetPoll net_poll;
} vmk_NetqueueOpGetDefaultQueueArgs;

/** 
 * \ingroup Netqueue
 * \brief Arguments to VMK_NETQUEUE_OP_APPLY_RX_FILTER
 */
typedef struct vmk_NetqueueOpApplyRxFilterArgs {

   /** Queue aimed [in] */
   vmk_NetqueueQueueID qid;

   /** Filter to be applied [in] */
   vmk_NetqueueFilter filter;

   /** Filter ID [out] */
   vmk_NetqueueFilterID fid;

   /** Potential Paired tx queue hardware index [out] */
   vmk_uint16 pairhwqid;
} vmk_NetqueueOpApplyRxFilterArgs;

/** 
 * \ingroup Netqueue
 * \brief Arguments to VMK_NETQUEUE_OP_REMOVE_RX_FILTER
 */
typedef struct vmk_NetqueueOpRemoveRxFilterArgs {

   /** Queue aimed [in] */
   vmk_NetqueueQueueID qid;

   /** Filter to be removed [in] */
   vmk_NetqueueFilterID fid;
} vmk_NetqueueOpRemoveRxFilterArgs;

/** 
 * \ingroup Netqueue
 * \brief Arguments to VMK_NETQUEUE_OP_GET_QUEUE_STATS
 */
typedef struct vmk_NetqueueOpGetQueueStatsArgs {

   /** Queue aimed [in] */
   vmk_NetqueueQueueID qid;
} vmk_NetqueueOpGetQueueStatsArgs;

/** 
 * \ingroup Netqueue
 * \brief Arguments to VMK_NETQUEUE_OP_SET_TX_PRIORITY
 */
typedef struct vmk_NetqueueOpSetTxPriorityArgs {

   /** Queue aimed [in] */
   vmk_NetqueueQueueID qid;

   /** Priority to set on the queue [in] */
   vmk_VlanPriority priority;
} vmk_NetqueueOpSetTxPriorityArgs;

/** 
 * \ingroup Netqueue
 * \brief Arguments to VMK_NETQUEUE_OP_GETSET_QUEUE_STATE
 */
typedef struct vmk_NetqueueOpGetSetQueueStateArgs {

   /** Netqueue old state (TRUE: Netqueue was in valid state, FALSE: otherwise) [out] */
   vmk_Bool oldState;

   /** Netqueue new state (TRUE: Netqueue set to valid state, FALSE: otherwise) [in] */
   vmk_Bool newState;    
} vmk_NetqueueOpGetSetQueueStateArgs;

/** 
 * \ingroup Netqueue
 * \brief Arguments to VMK_NETQUEUE_OP_SET_QUEUE_VECTOR
 */
typedef struct vmk_NetqueueOpSetQueueVectorArgs {

   /** Queue aimed [in] */
   vmk_NetqueueQueueID qid;

   /** Vector to be set [in] */
   vmk_uint16 vector;
} vmk_NetqueueOpSetQueueVectorArgs;

/** 
 * \ingroup Netqueue
 * \brief Arguments to VMK_NETQUEUE_OP_ALLOC_QUEUE_WITH_ATTR
 */
typedef struct vmk_NetqueueOpAllocQueueWithAttrArgs {

   /** Queue type aimed [in] */
   vmk_NetqueueQueueType qtype;

   /** Number of attributes (Cannot be greater than VMK_NETQUEUE_QUEUE_ATTR_NUM) [in] */
   vmk_uint16 nattr;
   
   /** Queue attributes [in] */
   vmk_NetqueueQueueAttr *attr;

   /** Queue ID of the allocated queue [out] */
   vmk_NetqueueQueueID qid;

   /** Net poll on top of the allocated queue if any [out] */
   vmk_NetPoll net_poll;
} vmk_NetqueueOpAllocQueueWithAttrArgs;

/** 
 * \ingroup Netqueue
 * \brief Arguments to VMK_NETQUEUE_OP_ENABLE_QUEUE_FEAT
 */
typedef struct vmk_NetqueueOpEnableQueueFeatArgs {

   /** Queue aimed [in] */
   vmk_NetqueueQueueID qid;

   /** Features to be enabled [in] */
   vmk_NetqueueQueueFeatures features;
} vmk_NetqueueOpEnableQueueFeatArgs;

/** 
 * \ingroup Netqueue
 * \brief Arguments to VMK_NETQUEUE_OP_DISABLE_QUEUE_FEAT
 */
typedef struct vmk_NetqueueOpDisableQueueFeatArgs {

   /** Queue aimed [in] */
   vmk_NetqueueQueueID qid;

   /** Features to be disabled [in] */
   vmk_NetqueueQueueFeatures features;
} vmk_NetqueueOpDisableQueueFeatArgs;

/** 
 * \ingroup Netqueue
 * \brief Arguments to VMK_NETQUEUE_OP_GET_QUEUE_SUPPORTED_FEAT
 */
typedef struct vmk_NetqueueOpGetQueueSupFeatArgs {

   /** Queue type aimed [in] */
   vmk_NetqueueQueueType qtype;

   /** Features supported [out] */
   vmk_NetqueueQueueFeatures features;
} vmk_NetqueueOpGetQueueSupFeatArgs;

/** 
 * \ingroup Netqueue
 * \brief Arguments to VMK_NETQUEUE_OP_GET_FILTER_CLASS
 */
typedef struct vmk_NetqueueOpGetQueueSupFilterArgs {

   /** Queue type aimed [in] */
   vmk_NetqueueQueueType qtype;

   /** Filter class required [out] */
   vmk_NetqueueFilterClass class;
} vmk_NetqueueOpGetQueueSupFilterArgs;

/*
 ***********************************************************************
 * vmk_NetqueueSetQueueIDUserVal --                                  */ /**
 *
 * \ingroup Netqueue
 * \brief Set user's private value for queue ID.
 *
 * \param[in]  qid       Queue ID.
 * \param[in]  userval   User value.
 *
 * \retval     VMK_OK    Always.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_NetqueueSetQueueIDUserVal(vmk_NetqueueQueueID *qid,
                                               vmk_uint32 userval);

/*
 ***********************************************************************
 * vmk_NetqueueQueueIDUserVal --                                  */ /**
 *
 * \ingroup Netqueue
 * \brief Get user part of queue ID
 *
 * \param[in] qid           Queue ID.
 *
 * \retval    vmk_uint32    User value.
 *
 ***********************************************************************
 */

vmk_uint32  vmk_NetqueueQueueIDUserVal(vmk_NetqueueQueueID qid);

/*
 ***********************************************************************
 * vmk_NetqueueMkTxQueueID --                                     */ /**
 *
 * \ingroup Netqueue
 * \brief Create a Tx queue ID.
 *
 * \param[in]  val       The embedded value of the queue ID.
 * \param[out] qid       The created queue ID.
 *
 * \retval     VMK_OK    Always.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_NetqueueMkTxQueueID(vmk_NetqueueQueueID *qid,
                                         vmk_uint32 val);

/*
 ***********************************************************************
 * vmk_NetqueueMkRxQueueID --                                     */ /**
 *
 * \ingroup Netqueue
 * \brief Create a Rx queue ID.
 *
 * \param[in]  val       The embedded value of the queue ID.
 * \param[out] qid       The created queue ID.
 *
 * \retval     VMK_OK    Always.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_NetqueueMkRxQueueID(vmk_NetqueueQueueID *qid,
                                         vmk_uint32 val);

/*
 ***********************************************************************
 * vmk_NetqueueQueueIDVal --                                      */ /**
 *
 * \ingroup Netqueue
 * \brief Retrieve the embedded value of a queue ID.
 *
 * \param[in] qid       The aimed queue ID.
 *
 * \return              The embedded value.
 *
 ***********************************************************************
 */

vmk_uint32 vmk_NetqueueQueueIDVal(vmk_NetqueueQueueID qid);

/*
 ***********************************************************************
 * vmk_NetqueueQueueIDType --                                     */ /**
 *
 * \ingroup Netqueue
 * \brief Retrieve the type of a queue ID.
 *
 * \param[in] qid       The aimed queue ID.
 *
 * \return              The type.
 *
 ***********************************************************************
 */

vmk_NetqueueQueueType vmk_NetqueueQueueIDType(vmk_NetqueueQueueID qid);

/*
 ***********************************************************************
 * vmk_NetqueueMkFilterID --                                      */ /**
 *
 * \ingroup Netqueue
 * \brief Create a filter ID.
 *
 * \param[in]  val       The embedded value of the filter ID.
 * \param[out] fid       The created filter.
 *
 * \retval     VMK_OK    Always.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_NetqueueMkFilterID(vmk_NetqueueFilterID *fid,
                                        vmk_uint16 val);

/*
 ***********************************************************************
 * vmk_NetqueueFilterIDVal --                                     */ /**
 *
 * \ingroup Netqueue
 * \brief Retrieve the embedded value of a filter ID.
 *
 * \param[in] fid       The aimed filter ID.
 *
 * \return              The embedded value.
 *
 ***********************************************************************
 */

vmk_uint16 vmk_NetqueueFilterIDVal(vmk_NetqueueFilterID fid);

/*
 ***********************************************************************
 * vmk_PktQueueIDGet --                                           */ /**
 *
 * \ingroup Netqueue
 * \brief Retrieve netqueue queue ID of a specified packet.
 *
 * \param[in]  pkt        Packet of interest
 *
 * \return                Netqueue queue ID.
 *
 ***********************************************************************
 */
extern
vmk_NetqueueQueueID vmk_PktQueueIDGet(vmk_PktHandle *pkt);

/*
 ***********************************************************************
 * vmk_PktQueueIDSet --                                           */ /**
 *
 * \ingroup Netqueue
 * \brief Set netqueue queue ID of a specified packet.
 *
 * \param[in]  pkt        Packet of interest
 * \param[in]  qid        Packet queue ID
 *
 * \retval     VMK_OK
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_PktQueueIDSet(vmk_PktHandle *pkt, vmk_NetqueueQueueID qid);


#endif /* _VMKAPI_NET_NETQUEUE_H_ */
/** @} */
/** @} */
