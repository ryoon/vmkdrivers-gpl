/* **********************************************************
 * Copyright (C) 2012-2014 VMware, Inc.  All Rights Reserved. -- VMware Confidential
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * RDMA Types                                                     */ /**
 *
 * \addtogroup Core
 * @{
 * \defgroup RDMA Support for RDMA devices and clients
 *
 * The VMKernel provides support for device drivers to register RDMA
 * logical devices to make their RDMA functionality available for use by
 * RDMA clients.
 *
 * @{
 *
 ***********************************************************************
 */

#ifndef _VMKAPI_RDMA_TYPES_H_
#define _VMKAPI_RDMA_TYPES_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/* Forward declaration. */
struct vmk_RDMAEvent;

/**
 * \brief Address handl flag for global routing header.
 */
#define VMK_RDMA_AH_FLAGS_GRH  1


/**
 * \brief RDMA logical device handle.
 *
 * Opaque handle used to identify an RDMA logical device.
 */
typedef struct vmk_RDMADeviceInt *vmk_RDMADevice;


/**
 * \brief RDMA client handle.
 *
 * Opaque handle used to register and unregister an RDMA client.
 */
typedef struct vmk_RDMAClientInt *vmk_RDMAClient;


/**
 * \brief RDMA Link Layer
 */
typedef enum vmk_RDMALinkLayer {
   VMK_RDMA_LINK_UNSPECIFIED,
   VMK_RDMA_LINK_INFINIBAND,
   VMK_RDMA_LINK_ETHERNET,
} vmk_RDMALinkLayer;


/**
 * \brief RDMA Node Type
 */
typedef enum vmk_RDMANodeType {
   VMK_RDMA_NODE_TYPE_CA = 1,
   VMK_RDMA_NODE_TYPE_SWITCH,
   VMK_RDMA_NODE_TYPE_ROUTER,
} vmk_RDMANodeType;


/**
 * \brief RDMA speed
 */
typedef enum vmk_RDMADeviceSpeed {
   /** Single data rate */
   VMK_RDMA_SPEED_SDR    = 1 << 0,
   /** Double data rate */
   VMK_RDMA_SPEED_DDR    = 1 << 1,
   /** Quad data rate */
   VMK_RDMA_SPEED_QDR    = 1 << 2,
   /** Fourteen data rate */
   VMK_RDMA_SPEED_FDR10  = 1 << 3,
   /** Fourteen data rate */
   VMK_RDMA_SPEED_FDR    = 1 << 4,
   /** Enhanced data rate */
   VMK_RDMA_SPEED_EDR    = 1 << 5,
} vmk_RDMADeviceSpeed;


/**
 * \brief RDMA Device Width 
 */
typedef enum vmk_RDMADeviceWidth {
   VMK_RDMA_WIDTH_1X  = 1 << 0,
   VMK_RDMA_WIDTH_4X  = 1 << 1, 
   VMK_RDMA_WIDTH_8X  = 1 << 2, 
   VMK_RDMA_WIDTH_12X = 1 << 3, 
} vmk_RDMADeviceWidth;


/**
 * \brief RDMA Atomic Capabilities.
 */
typedef enum vmk_RDMAAtomicCap {
   VMK_RDMA_ATOMIC_CAP_NONE,
   VMK_RDMA_ATOMIC_CAP_HCA,
   VMK_RDMA_ATOMIC_CAP_GLOB,
} vmk_RDMAAtomicCap;


/**
 * \brief RDMA completion queue request flags.
 */
typedef enum vmk_RDMAComplQueueFlags {
   VMK_RDMA_CQ_FLAGS_SOLICITED            = 1 << 0,
   VMK_RDMA_CQ_FLAGS_NEXT_COMP            = 1 << 1,
   VMK_RDMA_CQ_FLAGS_SOLICITED_MASK       = VMK_RDMA_CQ_FLAGS_SOLICITED |
                                            VMK_RDMA_CQ_FLAGS_NEXT_COMP,
   VMK_RDMA_CQ_FLAGS_REPORT_MISSED_EVENTS = 1 << 2,
}  vmk_RDMAComplQueueFlags;

/**
 * \brief RDMA memory region access flags.
 */
typedef enum vmk_RDMAMemRegionFlags {
   VMK_RDMA_ACCESS_LOCAL_WRITE   = 1 << 0,
   VMK_RDMA_ACCESS_REMOTE_WRITE  = 1 << 1,
   VMK_RDMA_ACCESS_REMOTE_READ   = 1 << 2,
   VMK_RDMA_ACCESS_REMOTE_ATOMIC = 1 << 3,
   VMK_RDMA_ACCESS_MW_BIND       = 1 << 4,
} vmk_RDMAMemRegionFlags;

/**
 * \brief RDMA device capability flags.
 */
typedef enum vmk_RDMADeviceCapFlags {
   VMK_RDMA_DEVICE_RESIZE_MAX_WR      = 1 << 0,
   VMK_RDMA_DEVICE_BAD_PKEY_CNTR      = 1 << 1,
   VMK_RDMA_DEVICE_BAD_QKEY_CNTR      = 1 << 2,
   VMK_RDMA_DEVICE_RAW_MULTI          = 1 << 3,
   VMK_RDMA_DEVICE_AUTO_PATH_MIG      = 1 << 4,
   VMK_RDMA_DEVICE_CHANGE_PHY_PORT    = 1 << 5,
   VMK_RDMA_DEVICE_UD_AV_PORT_ENFORCE = 1 << 6,
   VMK_RDMA_DEVICE_CURR_QP_STATE_MOD  = 1 << 7,
   VMK_RDMA_DEVICE_SHUTDOWN_PORT      = 1 << 8,
   VMK_RDMA_DEVICE_INIT_TYPE          = 1 << 9,
   VMK_RDMA_DEVICE_PORT_ACTIVE_EVENT  = 1 << 10,
   VMK_RDMA_DEVICE_SYS_IMAGE_GUID     = 1 << 11,
   VMK_RDMA_DEVICE_RC_RNR_NAK_GEN     = 1 << 12,
   VMK_RDMA_DEVICE_SRQ_RESIZE	      = 1 << 13,
   VMK_RDMA_DEVICE_N_NOTIFY_CQ	      = 1 << 14,
   VMK_RDMA_DEVICE_LOCAL_DMA_LKEY     = 1 << 15,
   VMK_RDMA_DEVICE_RESERVED           = 1 << 16,
   VMK_RDMA_DEVICE_MEM_WINDOW         = 1 << 17,
   VMK_RDMA_DEVICE_BLOCK_MULTICAST_LOOPBACK = 1 << 22,
} vmk_RDMADeviceCapFlags;

/**
 * \brief RDMA device attributes.
 */
typedef struct vmk_RDMADeviceAttr {
   vmk_uint64 firmwareVersion;
   vmk_uint64 sysImageGuid;
   vmk_uint64 maxMrSize;
   vmk_uint64 pageSizeCap;
   vmk_uint32 vendorId;
   vmk_uint32 vendorPartId;
   vmk_uint32 hardwareVersion;
   vmk_int32 maxQp;
   vmk_int32 maxQpWr;
   vmk_RDMADeviceCapFlags deviceCapFlags;
   vmk_int32 maxSge;
   vmk_int32 maxSgeRd;
   vmk_int32 maxCq;
   vmk_int32 maxCqe;
   vmk_int32 maxMr;
   vmk_int32 maxPd;
   vmk_int32 maxQpRdAtom;
   vmk_int32 maxEeRdAtom;
   vmk_int32 maxResRdAtom;
   vmk_int32 maxQpInitRdAtom;
   vmk_int32 maxEEInitRdAtom;
   vmk_RDMAAtomicCap atomicCap;
   vmk_RDMAAtomicCap maskedAtomicCap;
   vmk_int32 maxEe;
   vmk_int32 maxRdd;
   vmk_int32 maxMw;
   vmk_int32 maxRawIpv6Qp;
   vmk_int32 maxRawEthyQp;
   vmk_int32 maxMcastGrp;
   vmk_int32 maxMcastQpAttach;
   vmk_int32 maxTotalMcastQpAttach;
   vmk_int32 maxAh;
   vmk_int32 maxFmr;
   vmk_int32 maxMapPerFmr;
   vmk_int32 maxSrq;
   vmk_int32 maxSrqWr;
   vmk_int32 maxSrqSge;
   vmk_uint32 maxFastRegPageListLen;
   vmk_uint16 maxPkeys;
   vmk_uint8 localCaAckDelay;
   vmk_int32 numCompVectors;
   vmk_uint64 nodeGuid;
   vmk_uint32 localDmaLkey;
   vmk_uint8 nodeType;
} vmk_RDMADeviceAttr;


/**
 * \brief RDMA GID
 */
typedef union vmk_RDMAGid {
   vmk_uint8 raw[16];
   struct {
      vmk_uint64 subnetPrefix;
      vmk_uint64 interfaceId;
   } global;
} vmk_RDMAGid;

/**
 * \brief RDMA Completion Queue handle.
 */
typedef struct vmk_RDMAComplQueueInt *vmk_RDMAComplQueue;

/** \brief RDMA Completion Queue completion callback. */
typedef void (*vmk_RDMAComplQueueCompletionCb)(vmk_RDMAComplQueue cq, vmk_AddrCookie cbArg);

/** \brief RDMA Completion queue event callback. */
typedef void (*vmk_RDMAComplQueueEventCb)(struct vmk_RDMAEvent *event, vmk_AddrCookie cbArg);

/**
 * \brief RDMA Completion Queue create properties.
 */
typedef struct vmk_RDMAComplQueueCreateProps {
   vmk_ModuleID moduleID;
   vmk_HeapID heapID;
   vmk_RDMADevice device;
   /** \brief Entries for this completion queue. */
   vmk_uint32 entries;
   /** \brief Vector for this completion queue. */
   vmk_uint32 vector;
   /** \brief Completion callback for completion queue. */
   vmk_RDMAComplQueueCompletionCb completionCb;
   /** \brief Event callback for completion queue. */
   vmk_RDMAComplQueueEventCb eventCb;
   /** \brief Argument passed to both callbacks. */
   vmk_AddrCookie cbArg;
} vmk_RDMAComplQueueCreateProps;

/**
 * \brief RDMA Protection Domain handle.
 */
typedef struct vmk_RDMAProtDomainInt *vmk_RDMAProtDomain;

/**
 * \brief RDMA Protection domain properties.
 *
 * Properties provided for allocation of a protection domain.
 */
typedef struct vmk_RDMAProtDomainProps {
   vmk_ModuleID moduleID;
   vmk_HeapID heapID;
   vmk_RDMADevice device;
} vmk_RDMAProtDomainProps;

/**
 * \brief RDMA Address handle handle.
 */
typedef struct vmk_RDMAAddrHandleInt *vmk_RDMAAddrHandle;

/**
 * \brief RDMA global route.
 */
typedef struct vmk_RDMAGlobalRoute {
   vmk_RDMAGid dgid;
   vmk_uint32 flowLabel;
   vmk_uint8 sgidIndex;
   vmk_uint8 hopLimit;
   vmk_uint8 trafficClass;
} vmk_RDMAGlobalRoute;

/**
 * \brief RDMA Address handle attributes.
 */
typedef struct vmk_RDMAAddrHandleAttr {
   vmk_RDMAGlobalRoute globalRoute;
   vmk_uint16 dlid;
   vmk_uint8 sl;
   vmk_uint8 srcPathBits;
   vmk_uint8 staticRate;
   vmk_uint8 ahFlags;
   vmk_EthAddress dstMac;
} vmk_RDMAAddrHandleAttr;

/**
 * \brief RDMA Address handle properties.
 *
 * Properties provided for creation of an address handle.
 */
typedef struct vmk_RDMAAddrHandleProps {
   vmk_ModuleID moduleID;
   vmk_HeapID heapID;
   vmk_RDMADevice device;
   vmk_RDMAProtDomain pd;
   vmk_RDMAAddrHandleAttr attr;
} vmk_RDMAAddrHandleProps;

/**
 * \brief RDMA Queue Pair type.
 */
typedef enum vmk_RDMAQueuePairType {
   VMK_RDMA_QP_TYPE_GSI = 1,
   VMK_RDMA_QP_TYPE_RC,
   VMK_RDMA_QP_TYPE_UC,
   VMK_RDMA_QP_TYPE_UD,
   VMK_RDMA_QP_TYPE_MAX = 9,
} vmk_RDMAQueuePairType;

/**
 * \brief RDMA Queue Pair capabilities.
 */
typedef struct vmk_RDMAQueuePairCap {
   vmk_uint32 maxSendWr;
   vmk_uint32 maxRecvWr;
   vmk_uint32 maxSendSge;
   vmk_uint32 maxRecvSge;
   vmk_uint32 maxInlineData;
} vmk_RDMAQueuePairCap;

/**
 * \brief RDMA Signal type.
 */
typedef enum vmk_RDMASigType {
   VMK_RDMA_SIGNAL_ALL_WR,
   VMK_RDMA_SIGNAL_REQ_WR,
} vmk_RDMASigType;

/**
 * \brief RDMA Queue Pair Create Flags.
 */
typedef enum vmk_RDMAQpCreateFlags {
   VMK_RDMA_QP_CREATE_FLAGS_NONE = 0,
   VMK_RDMA_QP_CREATE_UD_LSO = 1 << 0,
   VMK_RDMA_QP_CREATE_FLAGS_BLOCK_MULTICAST_LOOPBACK  = 1 << 1,
} vmk_RDMAQpCreateFlags;

/**
 * \brief RDMA Queue Pair state.
 */
typedef enum vmk_RDMAQueuePairState {
   VMK_RDMA_QP_STATE_RESET,
   VMK_RDMA_QP_STATE_INIT,
   VMK_RDMA_QP_STATE_RTR,
   VMK_RDMA_QP_STATE_RTS,
   VMK_RDMA_QP_STATE_SQD,
   VMK_RDMA_QP_STATE_SQE,
   VMK_RDMA_QP_STATE_ERR,
} vmk_RDMAQueuePairState;

/**
 * \brief RDMA MTU.
 */
typedef enum vmk_RDMAMtu {
   VMK_RDMA_MTU_256  = 1,
   VMK_RDMA_MTU_512  = 2,
   VMK_RDMA_MTU_1024 = 3,
   VMK_RDMA_MTU_2048 = 4,
   VMK_RDMA_MTU_4096 = 5,
} vmk_RDMAMtu;

/**
 * \brief RDMA Migration state.
 */
typedef enum vmk_RDMAMigState {
   VMK_RDMA_MIG_MIGRATED,
   VMK_RDMA_MIG_REARM,
   VMK_RDMA_MIG_ARMED,
} vmk_RDMAMigState;

/**
 * \brief RDMA Queue Pair attributes.
 */
typedef struct vmk_RDMAQueuePairAttr {
   vmk_uint32 qpNum;
   vmk_RDMAQueuePairState qpState;
   vmk_RDMAQueuePairState qpCurrState;
   vmk_RDMAMtu pathMtu;
   vmk_RDMAMigState pathMigState;
   vmk_uint32 qkey;
   vmk_uint32 rqPsn;
   vmk_uint32 sqPsn;
   vmk_uint32 destQpNum;
   vmk_uint32 qpAccessFlags;
   vmk_RDMAQueuePairCap cap;
   vmk_RDMAAddrHandleAttr ahAttr;
   vmk_RDMAAddrHandleAttr altAhAttr;
   vmk_uint16 pkeyIndex;
   vmk_uint16 altPkeyIndex;
   vmk_uint8 enSqdAsyncNotify;
   vmk_uint8 sqDraining;
   vmk_uint8 maxRdAtomic;
   vmk_uint8 maxDestRdAtomic;
   vmk_uint8 minRnrTimer;
   vmk_uint8 timeout;
   vmk_uint8 retryCount;
   vmk_uint8 rnrRetry;
   vmk_uint8 altTimeout;
} vmk_RDMAQueuePairAttr;

/**
 * \brief RDMA Queue Pair Init attributes.
 */
typedef struct vmk_RDMAQueuePairInitAttr {
  /**
   * \brief Opaque handle for send completion queue.
   *
   * \note This needs to be specified by the client
   * during queue pair creation.
   */
   vmk_RDMAComplQueue sendCq;
  /**
   * \brief Opaque handle for receive completion queue.
   *
   * \note This needs to be specified by the client
   * during queue pair creation.
   */
   vmk_RDMAComplQueue recvCq;
   /**
    * \brief Driver's private data associated with sendCq.
    *
    * \note The RDMA client need not provide this value when filling in
    *       the attributes structure.  This will be filled in by the RDMA stack
    *       internally for the driver's use.
    */
   vmk_AddrCookie sendCqData;
   /**
    * \brief Driver's private data associated with recvCq.
    *
    * \note The RDMA client need not provide this value when filling in
    *       the attributes structure.  This will be filled in by the RDMA stack
    *       internally for the driver's use.
    */
   vmk_AddrCookie recvCqData;
   vmk_RDMAQueuePairCap cap;
   vmk_RDMASigType sqSigType;
   vmk_RDMAQueuePairType qpType;
   vmk_RDMAQpCreateFlags createFlags;
} vmk_RDMAQueuePairInitAttr;

/*
 *************************************************************************
 * vmk_RDMAQueuePairEventCb --                                      */ /**
 *
 * \brief RDMA Queue pair event callback invoked by the driver.
 *
 * \note  This callback may not block.
 *
 * \param[in] event              Event to be dispatched.
 * \param[in] eventHandlerData   Data defining this event callback.
 *
 * \retval VMK_OK         Queue pair event callback successful.
 * \retval VMK_BAD_PARAM  Invalid event data or event.
 *
 *************************************************************************
 */
typedef void (*vmk_RDMAQueuePairEventCb)(struct vmk_RDMAEvent *event, vmk_AddrCookie cbArg);

/**
 * \brief RDMA Queue Pair properties.
 *
 * Properties provided for creation of a queue pair.
 */
typedef struct vmk_RDMAQueuePairCreateProps {
   vmk_ModuleID moduleID;
   vmk_HeapID heapID;
   vmk_RDMADevice device;
   vmk_RDMAProtDomain pd;
   vmk_RDMAQueuePairInitAttr initAttr;
   /** \brief Event callback for queue pair. */
   vmk_RDMAQueuePairEventCb eventCb;
   /** \brief Argument passed to callback. */
   vmk_AddrCookie cbArg;
} vmk_RDMAQueuePairCreateProps;


/**
 * \brief RDMA Queue Pair handle.
 */
typedef struct vmk_RDMAQueuePairInt *vmk_RDMAQueuePair;

/**
 * \brief RDMA Scatter/gather entry.
 */
typedef struct vmk_RDMASge {
   vmk_uint64 addr;
   vmk_uint32 length;
   vmk_uint32 lkey;
} vmk_RDMASge;

/**
 * \brief RDMA Work Request opcode.
 */
typedef enum vmk_RDMAWorkRequestOpcode {
   VMK_RDMA_WR_RDMA_WRITE,
   VMK_RDMA_WR_RDMA_WRITE_WITH_IMM,
   VMK_RDMA_WR_SEND,
   VMK_RDMA_WR_SEND_WITH_IMM,
   VMK_RDMA_WR_RDMA_READ,
   VMK_RDMA_WR_ATOMIC_CMP_AND_SWP,
   VMK_RDMA_WR_ATOMIC_FETCH_AND_ADD,
   VMK_RDMA_WR_LSO,
   VMK_RDMA_WR_SEND_WITH_INV,
   VMK_RDMA_WR_RDMA_READ_WITH_INV,
   VMK_RDMA_WR_LOCAL_INV,
   VMK_RDMA_WR_MASKED_ATOMIC_CMP_AND_SWP = 12,
   VMK_RDMA_WR_MASKED_ATOMIC_FETCH_AND_ADD = 13,
} vmk_RDMAWorkRequestOpcode;

/**
 * \brief RDMA Work Request send flags.
 */
typedef enum vmk_RDMAWorkRequestSendFlags {
   VMK_RDMA_WR_SENDFLAG_FENCE     = 1,
   VMK_RDMA_WR_SENDFLAG_SIGNALED  = 1 << 1,
   VMK_RDMA_WR_SENDFLAG_SOLICITED = 1 << 2,
   VMK_RDMA_WR_SENDFLAG_INLINE	  = 1 << 3,
   VMK_RDMA_WR_SENDFLAG_IP_CSUM	  = 1 << 4
} vmk_RDMAWorkRequestSendFlags;

/**
 * \brief RDMA Send Work Request.
 */
typedef struct vmk_RDMASendWorkRequest {
   struct vmk_RDMASendWorkRequest *next;
   vmk_uint64 workRequestID;
   vmk_RDMASge *sgList;
   vmk_uint32 numSge;
   vmk_RDMAWorkRequestOpcode opcode;
   vmk_RDMAWorkRequestSendFlags sendFlags;
   union {
      vmk_uint32 immediateData;
      vmk_uint32 invalidateRkey;
   } ex;
   union {
      struct {
         vmk_uint64 remoteAddr;
         vmk_uint32 rkey;
      } rdma;
      struct {
         vmk_uint64 remoteAddr;
         vmk_uint64 compareAdd;
         vmk_uint64 swap;
         vmk_uint64 compareAddMask;
         vmk_uint64 swapMask;
         vmk_uint32 rkey;
      } atomic;
      struct {
         vmk_RDMAAddrHandle ah;
         /** \brief Driver's private data associated with ah.
          *
          * \note The RDMA client need not provide this value when filling in
          *       the work request. This will be filled in by the RDMA stack
          *       internally for the driver's use.
          */
         vmk_AddrCookie ahData;
         void *header;
         vmk_uint32 headerLen;
         vmk_uint32 mss;
         vmk_uint32 remoteQpn;
         vmk_uint32 remoteQkey;
         vmk_uint16 pkeyIndex;
      } ud;
   } wr;
} vmk_RDMASendWorkRequest;

/**
 * \brief RDMA Receive Work Request.
 */
typedef struct vmk_RDMARecvWorkRequest {
   struct vmk_RDMARecvWorkRequest *next;
   vmk_uint64 workRequestID;
   vmk_RDMASge *sgList;
   vmk_uint32 numSge;
} vmk_RDMARecvWorkRequest;

/*
 * \brief RDMA memory region properties.
 */
typedef struct vmk_RDMAMemRegionProps {
   vmk_ModuleID moduleID;
   vmk_HeapID heapID;
   vmk_RDMADevice device;
   vmk_RDMAMemRegionFlags flags;
   vmk_RDMAProtDomain pd;
   /* \brief driver allocated memory region structure */
   vmk_AddrCookie mrData;
} vmk_RDMAMemRegionProps;

/*
 * \brief RDMA memory region attributes.
 */
typedef struct vmk_RDMAMemRegionAttr {
   vmk_RDMAProtDomain pd;
   vmk_RDMAMemRegionFlags accessFlags;
   vmk_uint32 lkey;
   vmk_uint32 rkey;
} vmk_RDMAMemRegionAttr;

/**
 * \brief RDMA Memory Region.
 */
typedef struct vmk_RDMAMemRegionInt *vmk_RDMAMemRegion;

/**
 * \brief RDMA work completion status.
 */
typedef enum vmk_RDMAWorkComplStatus {
   VMK_RDMA_WC_STATUS_SUCCESS,
   VMK_RDMA_WC_STATUS_LOC_LEN_ERR,
   VMK_RDMA_WC_STATUS_LOC_QP_OP_ERR,
   VMK_RDMA_WC_STATUS_LOC_EEC_OP_ERR,
   VMK_RDMA_WC_STATUS_LOC_PROT_ERR,
   VMK_RDMA_WC_STATUS_WR_FLUSH_ERR,
   VMK_RDMA_WC_STATUS_MW_BIND_ERR,
   VMK_RDMA_WC_STATUS_BAD_RESP_ERR,
   VMK_RDMA_WC_STATUS_LOC_ACCESS_ERR,
   VMK_RDMA_WC_STATUS_REM_INV_REQ_ERR,
   VMK_RDMA_WC_STATUS_REM_ACCESS_ERR,
   VMK_RDMA_WC_STATUS_REM_OP_ERR,
   VMK_RDMA_WC_STATUS_RETRY_EXC_ERR,
   VMK_RDMA_WC_STATUS_RNR_RETRY_EXC_ERR,
   VMK_RDMA_WC_STATUS_LOC_RDD_VIOL_ERR,
   VMK_RDMA_WC_STATUS_REM_INV_RD_REQ_ERR,
   VMK_RDMA_WC_STATUS_REM_ABORT_ERR,
   VMK_RDMA_WC_STATUS_INV_EECN_ERR,
   VMK_RDMA_WC_STATUS_INV_EEC_STATE_ERR,
   VMK_RDMA_WC_STATUS_FATAL_ERR,
   VMK_RDMA_WC_STATUS_RESP_TIMEOUT_ERR,
   VMK_RDMA_WC_STATUS_GENERAL_ERR
} vmk_RDMAWorkComplStatus;

/**
 * \brief RDMA work completion opcode.
 */
typedef enum vmk_RDMAWorkComplOpcode {
   VMK_RDMA_WC_OPCODE_SEND,
   VMK_RDMA_WC_OPCODE_RDMA_WRITE,
   VMK_RDMA_WC_OPCODE_RDMA_READ,
   VMK_RDMA_WC_OPCODE_COMP_SWAP,
   VMK_RDMA_WC_OPCODE_FETCH_ADD,
   VMK_RDMA_WC_OPCODE_BIND_MW,
   VMK_RDMA_WC_OPCODE_LSO,
   VMK_RDMA_WC_OPCODE_LOCAL_INV,
   VMK_RDMA_WC_OPCODE_FAST_REG_MR,
   VMK_RDMA_WC_OPCODE_MASKED_COMP_SWAP,
   VMK_RDMA_WC_OPCODE_MASKED_FETCH_ADD,
   /*
    * Set value of VMK_RDMA_WC_OPCODE_RECV so consumers can test if a completion is a
    * receive by testing (opcode & VMK_RDMA_WC_OPCODE_RECV).
    */
   VMK_RDMA_WC_OPCODE_RECV			= 1 << 7,
   VMK_RDMA_WC_OPCODE_RECV_RDMA_WITH_IMM
} vmk_RDMAWorkComplOpcode;

/**
 * \brief RDMA work completion flags.
 */
typedef enum vmk_RDMAWorkComplFlags {
   VMK_RDMA_WC_FLAGS_GRH              = 1,
   VMK_RDMA_WC_FLAGS_WITH_IMM         = 1 << 1,
   VMK_RDMA_WC_FLAGS_WITH_INVALIDATE  = 1 << 2,
   VMK_RDMA_WC_FLAGS_IP_CSUM_OK	      = 1 << 3,
   VMK_RDMA_WC_FLAGS_WITH_SMAC        = 1 << 4,
   VMK_RDMA_WC_FLAGS_WITH_VLAN        = 1 << 5,
} vmk_RDMAWorkComplFlags;

/**
 * \brief RDMA work completion.
 */
typedef struct vmk_RDMAWorkCompl {
   vmk_uint64 workRequestID;
   vmk_RDMAWorkComplStatus status;
   vmk_RDMAWorkComplOpcode opcode;
   vmk_uint32 vendorErr;
   vmk_uint32 byteLen;
   vmk_RDMAQueuePair qp;
   union {
      vmk_uint32 immediateData;
      vmk_uint32 invalidateRkey;
   } ex;
   vmk_uint32 srcQp;
   vmk_RDMAWorkComplFlags flags;
   vmk_uint16 pkeyIndex;
   vmk_uint16 slid;
   vmk_uint8 sl;
   vmk_uint8 dlidPathBits;
   vmk_uint32 csumOk;
   vmk_EthAddress srcMac;
   vmk_uint16 vlanID;
   vmk_EthAddress dstMac;
} vmk_RDMAWorkCompl;

/**
 * \brief RDMA Port states.
 */
typedef enum vmk_RDMAPortState {
   VMK_RDMA_PORT_NOP,
   VMK_RDMA_PORT_DOWN,
   VMK_RDMA_PORT_INIT,
   VMK_RDMA_PORT_ARMED,
   VMK_RDMA_PORT_ACTIVE,
   VMK_RDMA_PORT_ACTIVE_DEFER,
} vmk_RDMAPortState;

/**
 * \brief RDMA Port attributes type.
 */
typedef struct vmk_RDMAPortAttr {
   vmk_RDMAPortState state;
   vmk_RDMAMtu maxMTU;
   vmk_RDMAMtu activeMTU;
   vmk_uint32 gidTableLen;
   vmk_uint32 portCapFlags;
   vmk_uint32 maxMsgSize;
   vmk_uint32 badPkeyCntr;
   vmk_uint32 qkeyViolCntr;
   vmk_uint16 pkeyTableLen;
   vmk_uint16 lid;
   vmk_uint16 smLid;
   vmk_uint8 lmc;
   vmk_uint8 maxVlNum;
   vmk_uint8 smSl;
   vmk_uint8 subnetTimeout;
   vmk_uint8 initTypeReply;
   vmk_uint8 activeWidth;
   vmk_uint8 activeSpeed;
   vmk_uint8 physicalState;
} vmk_RDMAPortAttr;

/**
 * \brief RDMA Device statistics.
 */
typedef struct vmk_RDMAStats {
   /** The number of packets received by the device. */
   vmk_uint64 rxPackets;

   /** The number of packets sent by the device. */
   vmk_uint64 txPackets;

   /** The number of bytes received by the device. */
   vmk_uint64 rxBytes;

   /** The number of bytes sent by the device. */
   vmk_uint64 txBytes;

   /** The number of packets received with errors. */
   vmk_uint64 rxErrors;

   /** The number of packets sent with errors. */
   vmk_uint64 txErrors;

   /** The number of packets received with length errors. */
   vmk_uint64 rxLengthErrors;

   /** The number of unicast packets received by the device. */
   vmk_uint64 rxUcastPkts;

   /** The number of multicast and broadcast packets received by the device. */
   vmk_uint64 rxMcastPkts;

   /** The number of unicast bytes received by the device. */
   vmk_uint64 rxUcastBytes;

   /** The number of  multicast and broadcast bytes received by the device. */
   vmk_uint64 rxMcastBytes;

   /** The number of unicast packets sent by the device. */
   vmk_uint64 txUcastPkts;

   /** The number of multicast and broadcast packets sent by the device. */
   vmk_uint64 txMcastPkts;

   /** The number of unicast bytes sent by the device. */
   vmk_uint64 txUcastBytes;

   /** The number of multicast and broadcast bytes sent by the device. */
   vmk_uint64 txMcastBytes;

   /** The number of allocated QueuePairs. */
   vmk_uint64 qpCnt;

   /** The number of QueuePairs in VMK_RDMA_QP_STATE_RESET state. */
   vmk_uint64 qpResetStateCnt;

   /** The number of QueuePairs in VMK_RDMA_QP_STATE_INIT state. */
   vmk_uint64 qpInitStateCnt;

   /** The number of QueuePairs in VMK_RDMA_QP_STATE_RTR state. */
   vmk_uint64 qpRtrStateCnt;

   /** The number of QueuePairs in VMK_RDMA_QP_STATE_RTS state. */
   vmk_uint64 qpRtsStateCnt;

   /** The number of QueuePairs in VMK_RDMA_QP_STATE_SQD state. */
   vmk_uint64 qpSqdStateCnt;

   /** The number of QueuePairs in VMK_RDMA_QP_STATE_SQE state. */
   vmk_uint64 qpSqeStateCnt;

   /** The number of QueuePairs in VMK_RDMA_QP_STATE_ERR state. */
   vmk_uint64 qpErrStateCnt;

   /** The number of events received on allocated QueuePairs. */
   vmk_uint64 qpEventCnt;

   /** The number of allocated CompletionQueues. */
   vmk_uint64 cqCnt;

   /** The number of events received on allocated completion queues. */
   vmk_uint64 cqEventCnt;

   /** The number of allocated shared receive queues(SRQs). */
   vmk_uint64 srqCnt;

   /** The number of events received on allocated shared receive queues. */
   vmk_uint64 srqEventCnt;

   /** The number of allocated Protection Domains. */
   vmk_uint64 pdCnt;

   /** The number of allocated Memory Regions.  */
   vmk_uint64 mrCnt;

   /** The number of allocated Address Handles. */
   vmk_uint64 ahCnt;

   /** The number of allocated Memory Windows. */
   vmk_uint64 mwCnt;
} vmk_RDMAStats;

/**
 * \brief RDMA Queue Pair Attributes Mask.
 */
typedef enum vmk_RDMAQueuePairAttrMask {
   VMK_RDMA_QP_STATE                = 1,
   VMK_RDMA_QP_CUR_STATE            = 1 << 1,
   VMK_RDMA_QP_EN_SQD_ASYNC_NOTIFY  = 1 << 2,
   VMK_RDMA_QP_ACCESS_FLAGS         = 1 << 3,
   VMK_RDMA_QP_PKEY_INDEX           = 1 << 4,
   VMK_RDMA_QP_PORT                 = 1 << 5,
   VMK_RDMA_QP_QKEY                 = 1 << 6,
   VMK_RDMA_QP_AV                   = 1 << 7,
   VMK_RDMA_QP_PATH_MTU             = 1 << 8,
   VMK_RDMA_QP_TIMEOUT              = 1 << 9,
   VMK_RDMA_QP_RETRY_CNT            = 1 << 10,
   VMK_RDMA_QP_RNR_RETRY            = 1 << 11,
   VMK_RDMA_QP_RQ_PSN               = 1 << 12,
   VMK_RDMA_QP_MAX_QP_RD_ATOMIC     = 1 << 13,
   VMK_RDMA_QP_ALT_PATH             = 1 << 14,
   VMK_RDMA_QP_MIN_RNR_TIMER        = 1 << 15,
   VMK_RDMA_QP_SQ_PSN               = 1 << 16,
   VMK_RDMA_QP_MAX_DEST_RD_ATOMIC   = 1 << 17,
   VMK_RDMA_QP_PATH_MIG_STATE       = 1 << 18,
   VMK_RDMA_QP_CAP                  = 1 << 19,
   VMK_RDMA_QP_DEST_QPN             = 1 << 20,
   VMK_RDMA_QP_SRC_QPN              = 1 << 21,
} vmk_RDMAQueuePairAttrMask;

/**
 * \brief RDMA connection manager bind properties.
 */
typedef struct vmk_RDMACMBindProps {
   vmk_SocketAddress socketAddr;
   vmk_EthAddress mac;
   vmk_uint32 mtu;
   vmk_uint16 vlanID;
   vmk_RDMAGid gid;
} vmk_RDMACMBindProps;

/**
 * \brief RDMA connection manager identifier.
 */
typedef struct vmk_RDMACMIDInt *vmk_RDMACMID;

/**
 * \brief RDMA Event Handler handle.
 */
typedef struct vmk_RDMAEventHandlerInt *vmk_RDMAEventHandler;

/*
 ***********************************************************************
 * vmk_RDMAOpAssociate --                                         */ /**
 *
 * \brief Associates RDMA device with a device.
 *
 * Handler used by vmkernel to notify driver that RDMA device is
 * associated with device.
 *
 * \note  This callback may block
 * \note  Driver must declare all of its capabilities in this callback.
 *
 * \param[in] driverData  Points to the driver internal structure
 *                        associated with the device to be operated on.
 *                        Before calling vmk_DeviceRegister(), the
 *                        driver must assign this pointer to the
 *                        driverData member of vmk_RDMADeviceRegData.
 * \param[in] rdmaDevice  The RDMA device associated with the device.
 *
 * \retval VMK_OK         Capabilities associated successfully.
 * \retval VMK_FAILURE    Capabilities association failed.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_RDMAOpAssociate)(vmk_AddrCookie driverData,
                                                vmk_RDMADevice rdmaDevice);

/*
 ***********************************************************************
 * vmk_RDMAOpDisassociate --                                      */ /**
 *
 * \brief Disassociates RDMA device from device.
 *
 * Handler used by vmkernel to notify driver that RDMA device is
 * disassociated from device.
 *
 * \note  This callback may block
 * \note  Driver doesn't need to unregister its capabilities in this
 *        callback.
 *
 * \param[in]  driverData Points to the driver internal structure
 *                        associated with the device to be operated on.
 *                        Before calling vmk_DeviceRegister(), the
 *                        driver must assign this pointer to the
 *                        driverData member of vmk_RDMADeviceRegData.
 *
 * \retval VMK_OK         Capabilities disassociated successfully.
 * \retval VMK_FAILURE    Capabilities disassociate failed.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_RDMAOpDisassociate)(vmk_AddrCookie driverData);


/*
 ***********************************************************************
 * vmk_RDMAOpGetStats --                                          */ /**
 *
 * \brief Obtains statistics for specified RDMA device.
 *
 * \note  This callback may block
 *
 * \param[in]  driverData Points to the driver internal structure
 *                        associated with the device to be operated on.
 *                        Before calling vmk_DeviceRegister(), the
 *                        driver must assign this pointer to the
 *                        driverData member of vmk_RDMADeviceRegData.
 * \param[out] stats      Returned statistics.
 *
 * \retval VMK_OK         On success.
 * \retval VMK_BAD_PARAM  Invalid memory region.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_RDMAOpGetStats)(vmk_AddrCookie driverData,
                                               vmk_RDMAStats *stats);


/*
 ***********************************************************************
 * vmk_RDMAOpCreateBind --                                        */ /**
 *
 * \brief Create a binding between a network device a this RDMA device.
 *
 * \note  This callback may block
 *
 * \note  The driver should ensure that this is a unique binding
 *        in cases where VMK_OK is returned.  In other cases VMK_EXISTS
 *        should be returned.
 *
 * \param[in]  driverData Points to the driver internal structure
 *                        associated with the device to be operated on.
 *                        Before calling vmk_DeviceRegister(), the
 *                        driver must assign this pointer to the
 *                        driverData member of vmk_RDMADeviceRegData.
 * \param[in]  props      Bind properties.
 * \param[out] index      Gid index assigned to this binding.  This
 *                        value should be the same index provided
 *                        to vmk_RDMAOpQueryGid() to obtain the
 *                        associated Gid.
 *
 * \retval VMK_OK         On success.
 * \retval VMK_EXISTS     A binding with these properties already
 *                        exists.
 * \retval VMK_BAD_PARAM  Invalid argument.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_RDMAOpCreateBind)(vmk_AddrCookie driverData,
                                                 vmk_RDMACMBindProps *props,
                                                 vmk_uint32 *index);

/*
 ***********************************************************************
 * vmk_RDMAOpModifyBind --                                        */ /**
 *
 * \brief Modifies an existing binding with the provided properties.
 *
 * Modification of a binding is necessary when any of the network
 * properties specified in vmk_RDMACMBindProps change for an interface
 * that has already been bound by invoking vmk_RDMAOpCreateBind().
 *
 * The Gid index may not be changed due to this call but the Gid or
 * other properties of the device may be changed.  If other device
 * properties change then the driver is responsible for dispatching
 * a vmk_RDMAEvent to notify clients that have registered to receive
 * events for the device.
 *
 * \note  This callback may block
 *
 * \param[in]  driverData Points to the driver internal structure
 *                        associated with the device to be operated on.
 *                        Before calling vmk_DeviceRegister(), the
 *                        driver must assign this pointer to the
 *                        driverData member of vmk_RDMADeviceRegData.
 * \param[in]  index      Gid index of the existing binding.
 * \param[in]  props      Bind properties.
 *
 * \retval VMK_OK         On success.
 * \retval VMK_NOT_FOUND  This index does contain an existing
 *                        binding.
 * \retval VMK_BAD_PARAM  Invalid properties.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_RDMAOpModifyBind)(vmk_AddrCookie driverData,
                                                 vmk_uint32 index,
                                                 vmk_RDMACMBindProps *props);

/*
 ***********************************************************************
 * vmk_RDMADestroyBind --                                         */ /**
 *
 * \brief Destroys a binding associated with this index.
 *
 * \note  This callback may block
 *
 * \param[in]  driverData Points to the driver internal structure
 *                        associated with the device to be operated on.
 *                        Before calling vmk_DeviceRegister(), the
 *                        driver must assign this pointer to the
 *                        driverData member of vmk_RDMADeviceRegData.
 * \param[in]  index      Index to unbind.
 *
 * \retval VMK_OK         On success.
 * \retval VMK_BAD_PARAM  Invalid argument.
 * \retval VMK_NOT_FOUND  Unbind request for binding not found.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_RDMAOpDestroyBind)(vmk_AddrCookie driverData,
                                                  vmk_uint32 index);

/*
 * Driver verb operations.
 */

/*
 ***********************************************************************
 * vmk_RDMAOpQueryDevice --                                       */ /**
 *
 * \brief Queries the device attributes.
 *
 * \note  This callback may block
 *
 * \param[in]  driverData Points to the driver internal structure
 *                        associated with the device to be operated on.
 *                        Before calling vmk_DeviceRegister(), the
 *                        driver must assign this pointer to the
 *                        driverData member of vmk_RDMADeviceRegData.
 * \param[out] attr       Attribute structure to be filled in.
 *
 * \retval VMK_OK         Attributes successfully retrieved.
 * \retval VMK_BAD_PARAM  Invalid device or attributes.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_RDMAOpQueryDevice)(vmk_AddrCookie driverData,
                                                  vmk_RDMADeviceAttr *attr);

/*
 ***********************************************************************
 * vmk_RDMAOpQueryPort --                                         */ /**
 *
 * \brief Queries RDMA port attributes
 *
 * Queries the RDMA device's port and returns its port attributes.
 *
 * \note  This callback may block
 *
 * \note  This callback may be invoked before the associate callback
 *        has been invoked.
 *
 * \param[in]  driverData Points to the driver internal structure
 *                        associated with the device to be operated on.
 *                        Before calling vmk_DeviceRegister(), the
 *                        driver must assign this pointer to the
 *                        driverData member of vmk_RDMADeviceRegData.
 * \param[out] portAttr   Port attributes for the device.
 *
 * \retval VMK_OK            Port attributes queried successfully.
 * \retval VMK_BAD_PARAM     Invalid arguments.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_RDMAOpQueryPort)(vmk_AddrCookie driverData,
                                                vmk_RDMAPortAttr *portAttr);

/*
 ***********************************************************************
 * vmk_RDMAOpGetLinkLayer --                                      */ /**
 *
 * \brief Gets the link layer of the device.
 *
 * \note  This callback may block
 *
 * \param[in]  driverData Points to the driver internal structure
 *                        associated with the device to be operated on.
 *                        Before calling vmk_DeviceRegister(), the
 *                        driver must assign this pointer to the
 *                        driverData member of vmk_RDMADeviceRegData.
 * \param[out] link       Link layer type.
 *
 * \retval VMK_OK         Link layer successfully retrieved.
 * \retval VMK_BAD_PARAM  Invalid device.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_RDMAOpGetLinkLayer)(vmk_AddrCookie driverData,
                                                   vmk_RDMALinkLayer *link);

/*
 ***********************************************************************
 * vmk_RDMAOpQueryGid --                                          */ /**
 *
 * \brief Queries gid for the device and index.
 *
 * \note  This callback may block
 *
 * \param[in]  driverData Points to the driver internal structure
 *                        associated with the device to be operated on.
 *                        Before calling vmk_DeviceRegister(), the
 *                        driver must assign this pointer to the
 *                        driverData member of vmk_RDMADeviceRegData.
 * \param[in]  index      Index querying.
 * \param[out] gid        Gid structure to be filled in.
 *
 * \retval VMK_OK         Attributes successfully retrieved.
 * \retval VMK_BAD_PARAM  Invalid device or index.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_RDMAOpQueryGid)(vmk_AddrCookie driverData,
                                               vmk_uint32 index,
                                               vmk_RDMAGid *gid);

/*
 ***********************************************************************
 * vmk_RDMAOpQueryPkey --                                         */ /**
 *
 * \brief Queries Pkey table entry
 *
 * Queries the Pkey table entry of the device.
 *
 * \note  This callback may block
 *
 * \param[in]     driverData Points to the driver internal structure
 *                           associated with the device to be operated on.
 *                           Before calling vmk_DeviceRegister(), the
 *                           driver must assign this pointer to the
 *                           driverData member of vmk_RDMADeviceRegData.
 * \param[in]     index      Key table index to query.
 * \param[out]    pkey       Returned pkey table entry of device.
 *
 * \retval VMK_OK            Pkey table entry queried successfully.
 * \retval VMK_BAD_PARAM     Invalid arguments.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_RDMAOpQueryPkey)(vmk_AddrCookie driverData,
                                                vmk_uint16 index,
                                                vmk_uint16 *pkey);

/*
 ***********************************************************************
 * vmk_RDMAOpAllocProtDomain --                                   */ /**
 *
 * \brief Allocates Protection Domain.
 *
 * \note  This callback may block.
 *
 * \param[in]  driverData Points to the driver internal structure
 *                        associated with the device to be operated on.
 *                        Before calling vmk_DeviceRegister(), the
 *                        driver must assign this pointer to the
 *                        driverData member of vmk_RDMADeviceRegData.
 * \param[in]  pd         Handle to protection domain allocated by
 *                        RDMA stack.
 * \param[out] pdData     Private driver data that will be passed
 *                        to vmk_RDMAOpFreeProtDomain or other
 *                        operations on this vmk_RDMAProtDomain.
 *
 * \retval VMK_OK          Protection domain allocated.
 * \retval VMK_BAD_PARAM   Invalid device or protection domain.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_RDMAOpAllocProtDomain)(vmk_AddrCookie driverData,
                                                      vmk_RDMAProtDomain pd,
                                                      vmk_AddrCookie *pdData);

/*
 ***********************************************************************
 * vmk_RDMAOpFreeProtDomain --                                    */ /**
 *
 * \brief Free the allocated Protection Domain.
 *
 * \note  This callback may block.
 *
 * \param[in]  driverData Points to the driver internal structure
 *                        associated with the device to be operated on.
 *                        Before calling vmk_DeviceRegister(), the
 *                        driver must assign this pointer to the
 *                        driverData member of vmk_RDMADeviceRegData.
 * \param[in] pdData      Data returned in pdData of
 *                        vmk_RDMAOpAllocProtDomain for the protection
 *                        domain to be freed.
 *
 * \retval VMK_OK        Protection domain freed.
 * \retval VMK_BAD_PARAM Invalid device or protection domain.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_RDMAOpFreeProtDomain)(vmk_AddrCookie driverData,
                                                     vmk_AddrCookie pdData);


/*
 ***********************************************************************
 * vmk_RDMAOpCreateAddrHandle --                                  */ /**
 *
 * \brief Creates an address handle.
 *
 * \note  This callback may block.
 *
 * \param[in]  driverData Points to the driver internal structure
 *                        associated with the device to be operated on.
 *                        Before calling vmk_DeviceRegister(), the
 *                        driver must assign this pointer to the
 *                        driverData member of vmk_RDMADeviceRegData.
 * \param[in]  attr       Address handle attributes.
 * \param[in]  pdData     Data returned in pdData of
 *                        vmk_RDMAOpAllocProtDomain for the protection
 *                        domain associated with this address handle.
 * \param[in]  ah         Handle to address handle created by RDMA
 *                        stack.
 * \param[out] ahData     Private driver data that will be passed
 *                        to vmk_RDMAOpQueryAddrHandle and
 *                        vmk_RDMAOpDestroyAddrHandle for any
 *                        subsequent operations on this
 *                        vmk_RDMAAddrHandle.
 *
 * \retval VMK_OK         Address handle created successfully.
 * \retval VMK_BAD_PARAM  Invalid device, protection domain, or
 *                        properties.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_RDMAOpCreateAddrHandle)(vmk_AddrCookie driverData,
                                                       vmk_RDMAAddrHandleAttr *attr,
                                                       vmk_AddrCookie pdData,
                                                       vmk_RDMAAddrHandle ah,
                                                       vmk_AddrCookie *ahData);

/*
 ***********************************************************************
 * vmk_RDMAOpQueryAddrHandle --                                   */ /**
 *
 * \brief Query the address handle.
 *
 * \note  This callback may block.
 *
 * \param[in]  driverData Points to the driver internal structure
 *                        associated with the device to be operated on.
 *                        Before calling vmk_DeviceRegister(), the
 *                        driver must assign this pointer to the
 *                        driverData member of vmk_RDMADeviceRegData.
 * \param[in]  ahData     Driver's private address handle data.
 * \param[out] ahAttr     Address handle attributes.
 *
 * \retval VMK_OK         Address handle properties queried successfully.
 * \retval VMK_BAD_PARAM  Invalid device or address handle.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_RDMAOpQueryAddrHandle)(vmk_AddrCookie driverData,
                                                      vmk_AddrCookie ahData,
                                                      vmk_RDMAAddrHandleAttr *ahAttr);

/*
 ***********************************************************************
 * vmk_RDMAOpDestroyAddrHandle --                                 */ /**
 *
 * \brief Destroy an address handle.
 *
 * \note  This callback may block.
 *
 * \param[in] driverData  Points to the driver internal structure
 *                        associated with the device to be operated on.
 *                        Before calling vmk_DeviceRegister(), the
 *                        driver must assign this pointer to the
 *                        driverData member of vmk_RDMADeviceRegData.
 * \param[in] ah          Address handle to destroy.
 * \param[in] ahData      Driver's private address handle data.
 *
 * \retval VMK_OK         Protection domain freed.
 * \retval VMK_BAD_PARAM  Invalid device or address handle.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_RDMAOpDestroyAddrHandle)(vmk_AddrCookie driverData,
                                                        vmk_AddrCookie ahData);

/*
 ***********************************************************************
 * vmk_RDMAOpCreateQueuePair --                                  */ /**
 *
 * \brief Creates a queue pair.
 *
 * \note  This callback may block
 *
 * \param[in]  driverData  Points to the driver internal structure
 *                         associated with the device to be operated on.
 *                         Before calling vmk_DeviceRegister(), the
 *                         driver must assign this pointer to the
 *                         driverData member of vmk_RDMADeviceRegData.
 * \param[in]  initAttr    Queue pair init attributes.
 * \param[in]  pdData      Data returned in pdData of
 *                         vmk_RDMAOpAllocProtDomain for the protection
 *                         domain associated with this queue pair.
 * \param[in]  qp          Handle to queue pair allocated by
 *                         RDMA stack.
 * \param[out] qpData      Private driver data that will be passed
 *                         to vmk_RDMAOpModifyQueuePair or other
 *                         operations on this vmk_RDMAQueuePair.
 *
 * \retval VMK_OK          Queue pair created successfully.
 * \retval VMK_BAD_PARAM   Invalid device or attributes.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_RDMAOpCreateQueuePair)(vmk_AddrCookie driverData,
			                              vmk_RDMAQueuePairInitAttr *initAttr,
		                                      vmk_AddrCookie pdData,
			                              vmk_RDMAQueuePair qp,
                                                      vmk_AddrCookie *qpData);

/*
 ***********************************************************************
 * vmk_RDMAOpModifyQueuePair --                                  */ /**
 *
 * \brief Modifies a queue pair.
 *
 * \note  This callback may block
 *
 * \param[in]  driverData  Points to the driver internal structure
 *                         associated with the device to be operated on.
 *                         Before calling vmk_DeviceRegister(), the
 *                         driver must assign this pointer to the
 *                         driverData member of vmk_RDMADeviceRegData.
 * \param[in]  qpData      Driver's private queue pair data.
 * \param[in]  attr        Queue Pair attributes.
 * \param[in]  qpAttrMask  Queue Pair attribute mask.
 *
 * \retval VMK_OK          Queue pair modified successfully.
 * \retval VMK_BAD_PARAM   Invalid device or attributes.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_RDMAOpModifyQueuePair)(vmk_AddrCookie driverData,
                                                      vmk_AddrCookie qpData,
                                                      vmk_RDMAQueuePairAttr *attr,
                                                      vmk_RDMAQueuePairAttrMask qpAttrMask);

/*
 ***********************************************************************
 * vmk_RDMAOpQueryQueuePair --                                    */ /**
 *
 * \brief Queries a queue pair.
 *
 * \note  This callback may block
 *
 * \param[in]  driverData  Points to the driver internal structure
 *                         associated with the device to be operated on.
 *                         Before calling vmk_DeviceRegister(), the
 *                         driver must assign this pointer to the
 *                         driverData member of vmk_RDMADeviceRegData.
 * \param[in]  qpData      Driver's private queue pair data.
 * \param[out] attr        Queue Pair attributes.
 * \param[in]  qpAttrMask  Queue Pair attribute mask.
 * \param[out] initAttr    Queue Pair init attributes.
 *
 * \retval VMK_OK          Queue pair queried successfully.
 * \retval VMK_BAD_PARAM   Invalid device or attributes.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_RDMAOpQueryQueuePair)(vmk_AddrCookie driverData,
                                                     vmk_AddrCookie qpData,
                                                     vmk_RDMAQueuePairAttr *attr,
                                                     vmk_RDMAQueuePairAttrMask qpAttrMask,
                                                     vmk_RDMAQueuePairInitAttr *initAttr);

/*
 ***********************************************************************
 * vmk_RDMAOpDestroyQueuePair --                                  */ /**
 *
 * \brief Destroys a queue pair.
 *
 * \note  This callback may block
 *
 * \param[in]  driverData  Points to the driver internal structure
 *                         associated with the device to be operated on.
 *                         Before calling vmk_DeviceRegister(), the
 *                         driver must assign this pointer to the
 *                         driverData member of vmk_RDMADeviceRegData.
 * \param[in]  qpData      Driver's private queue pair data.
 *
 * \retval VMK_OK          Queue pair successfully destroyed.
 * \retval VMK_BAD_PARAM   Invalid device or queue pair.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_RDMAOpDestroyQueuePair)(vmk_AddrCookie driverData,
                                                       vmk_AddrCookie qpData);

/*
 ***********************************************************************
 * vmk_RDMAOpPostSend --                                  */ /**
 *
 * \brief Posts a work request on the specified queue pair's send queue.
 *
 * \note  This callback may block
 *
 * \param[in]  driverData  Points to the driver internal structure
 *                         associated with the device to be operated on.
 *                         Before calling vmk_DeviceRegister(), the
 *                         driver must assign this pointer to the
 *                         driverData member of vmk_RDMADeviceRegData.
 * \param[in]  qpData      Driver's private queue pair data.
 * \param[in]  sendWr      Work Request to be posted on send queue.
 * \param[out] badSendWr   Erroneous Work Request.  Must point to a
 *                         work request in list rooted at sendWr.
 *
 * \retval VMK_OK          Work Request successfully posted on send queue.
 * \retval VMK_BAD_PARAM   Invalid device or attributes.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_RDMAOpPostSend)(vmk_AddrCookie driverData,
                                               vmk_AddrCookie qpData,
					       vmk_RDMASendWorkRequest *sendWr,
					       vmk_RDMASendWorkRequest **badSendWr);

/*
 ***********************************************************************
 * vmk_RDMAOpPostRecv --                                  */ /**
 *
 * \brief Posts a work request on the specified queue pair's receive queue.
 *
 * \note  This callback may block
 *
 * \param[in]  driverData  Points to the driver internal structure
 *                         associated with the device to be operated on.
 *                         Before calling vmk_DeviceRegister(), the
 *                         driver must assign this pointer to the
 *                         driverData member of vmk_RDMADeviceRegData.
 * \param[in]  qpData      Driver's private queue pair data.
 * \param[in]  recvWr      Work Request to be posted on receive queue.
 * \param[out] badRecvWr   Erroneous Work Request.
 *
 * \retval VMK_OK          Work Request successfully posted on receive queue.
 * \retval VMK_BAD_PARAM   Invalid device or attributes.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_RDMAOpPostRecv)(vmk_AddrCookie driverData,
                                               vmk_AddrCookie qpData,
                                               vmk_RDMARecvWorkRequest *recvWr,
                                               vmk_RDMARecvWorkRequest **badRecvWr);

/*
 ***********************************************************************
 * vmk_RDMAOpCreateComplQueue --                                  */ /**
 *
 * \brief Creates a completion queue.
 *
 * \note  This callback may block
 *
 * \param[in]  driverData Points to the driver internal structure
 *                        associated with the device to be operated on.
 *                        Before calling vmk_DeviceRegister(), the
 *                        driver must assign this pointer to the
 *                        driverData member of vmk_RDMADeviceRegData.
 * \param[in]  entries    Entries in the queue.
 * \param[in]  vector     Vector number for this queue.
 * \param[in]  cq         Handle to completion queue created by
 *                        RDMA stack.
 * \param[out] cqData     Private driver data that will be passed
 *                        to vmk_RDMAOpDestroyComplQueue or other
 *                        operations on this completion queue.
 *
 * \retval VMK_OK         Completion queue created succesfully.
 * \retval VMK_BAD_PARAM  Invalid device or attributes.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_RDMAOpCreateComplQueue)(vmk_AddrCookie driverData,
                                                       vmk_uint32 entries,
                                                       vmk_uint32 vector,
                                                       vmk_RDMAComplQueue cq,
                                                       vmk_AddrCookie *cqData);

/*
 ***********************************************************************
 * vmk_RDMAOpDestroyComplQueue --                                 */ /**
 *
 * \brief Destroys a completion queue.
 *
 * \note  This callback may block
 *
 * \param[in]  driverData Points to the driver internal structure
 *                        associated with the device to be operated on.
 *                        Before calling vmk_DeviceRegister(), the
 *                        driver must assign this pointer to the
 *                        driverData member of vmk_RDMADeviceRegData.
 * \param[in]  cqData     Data returned in cqData of
 *                        vmk_RDMAOpCreateComplQueue for the completion
 *                        queue to be destroyed.
 *
 * \retval VMK_OK         Completion queue destroyed succesfully.
 * \retval VMK_BAD_PARAM  Invalid device or completion queue.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_RDMAOpDestroyComplQueue)(vmk_AddrCookie driverData,
                                                        vmk_AddrCookie cqData);

/*
 ***********************************************************************
 * vmk_RDMAOpPollComplQueue --                                    */ /**
 *
 * \brief Polls a completion queue.
 *
 * Polls a completion queue and returns the specified number of entries
 * in provided work completions array.
 *
 * \note  This callback may block
 *
 * \param[in]  driverData    Points to the driver internal structure
 *                           associated with the device to be operated
 *                           on.  Before calling vmk_DeviceRegister(),
 *                           the driver must assign this pointer to the
 *                           driverData member of
 *                           vmk_RDMADeviceRegData.
 * \param[in]  cqData        Data returned in cqData of
 *                           vmk_RDMAOpCreateComplQueue for the
 *                           completion queue to be destroyed.
 * \param[in,out] entries    The number of entries wc has room to hold
 *                           and when VMK_OK is returned, the number of
 *                           entries placed in to wc.
 * \param[in]     wc         Work completion array containing room for
 *                           at least the specified number of entries.
 *
 * \retval VMK_OK         Completion queue polled successfully.
 * \retval VMK_BAD_PARAM  Invalid arguments.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_RDMAOpPollComplQueue)(vmk_AddrCookie driverData,
                                                     vmk_AddrCookie cqData,
                                                     vmk_uint32 *entries,
                                                     vmk_RDMAWorkCompl *wc);

/*
 ***********************************************************************
 * vmk_RDMAOpReqNotifyComplQueue --                               */ /**
 *
 * \brief Requests notification from a completion queue.
 *
 * \note  This callback may block
 *
 * \param[in]  driverData     Points to the driver internal structure
 *                            associated with the device to be operated
 *                            on.  Before calling vmk_DeviceRegister(),
 *                            the driver must assign this pointer to the
 *                            driverData member of
 *                            vmk_RDMADeviceRegData.
 * \param[in]  cqData         Data returned in cqData of
 *                            vmk_RDMAOpCreateComplQueue for the
 *                            completion queue to be destroyed.
 * \param[in]   flags         Flags for notification request.
 * \param[out]  missedEvents  Optional out argument expected only if
 *                            flags contains
 *                            VMK_RDMA_CQ_FLAGS_REPORT_MISSED_EVENTS.
 *                            If that flag is provided, missedEvents
 *                            will be set to VMK_TRUE if it is
 *                            possible that an event was missed due
 *                            to race between notification request
 *                            and completion queue entry.  In this
 *                            case the caller should poll the
 *                            completion queue again to ensure it is
 *                            empty.  If this argument is set to
 *                            VMK_FALSE, no events were missed.
 *
 *
 * \retval VMK_OK         Request issued successfully.
 * \retval VMK_BAD_PARAM  Invalid arguments.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_RDMAOpReqNotifyComplQueue)(vmk_AddrCookie driverData,
                                                          vmk_AddrCookie cqData,
                                                          vmk_RDMAComplQueueFlags flags,
                                                          vmk_Bool *missedEvents);

/*
 ***********************************************************************
 * vmk_RDMAOpGetDmaMemRegion --                                  */ /**
 *
 * \brief Creates mapped Memory Region.
 *
 * \note  This callback may block
 *
 * \param[in]  driverData Points to the driver internal structure
 *                        associated with the device to be operated on.
 *                        Before calling vmk_DeviceRegister(), the
 *                        driver must assign this pointer to the
 *                        driverData member of vmk_RDMADeviceRegData.
 * \param[in]  pdData     Data returned in pdData of
 *                        vmk_RDMAOpAllocProtDomain for the protection
 *                        domain associated with this address handle.
 * \param[in]  flags      Memory region access flags.
 * \param[in]  mr         Handle to memory region created by
 *                        RDMA stack.
 * \param[out] mrData     Private driver data that will be passed to
 *                        vmk_RDMAOpDeregMemRegion or to other operations
 *                        on this memory region.
 *
 * \retval VMK_OK         Memory region successfully retrieved.
 * \retval VMK_BAD_PARAM  Invalid protection domain or flags.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_RDMAOpGetDmaMemRegion)(vmk_AddrCookie driverData,
                                                      vmk_AddrCookie pdData,
                                                      vmk_RDMAMemRegionFlags flags,
                                                      vmk_RDMAMemRegion mr,
                                                      vmk_AddrCookie *mrData);

/*
 ***********************************************************************
 * vmk_RDMAOpQueryMemRegion --                                   */ /**
 *
 * \brief Queries specified memory region.
 *
 * \note  This callback may block
 *
 * \param[in]  driverData Points to the driver internal structure
 *                        associated with the device to be operated on.
 *                        Before calling vmk_DeviceRegister(), the
 *                        driver must assign this pointer to the
 *                        driverData member of vmk_RDMADeviceRegData.
 * \param[in]  mrData     Data returned in mrData of
 *                        vmk_RDMAOpGetDmaMemRegion for the memory region
 *                        to be deregistered.
 * \param[out] lkey       Lkey number returned by driver.
 * \param[out] rkey       Rkey number returned by driver.
 *
 * \retval VMK_OK         Memory region successfully retrieved.
 * \retval VMK_BAD_PARAM  Invalid protection domain or flags.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_RDMAOpQueryMemRegion)(vmk_AddrCookie driverData,
                                                     vmk_AddrCookie mrData,
                                                     vmk_uint32 *lkey,
                                                     vmk_uint32 *rkey);

/*
 ***********************************************************************
 * vmk_RDMAOpDeregMemRegion --                                  */ /**
 *
 * \brief Deregisters memory region.
 *
 * \note  This callback may block
 *
 * \param[in]  driverData Points to the driver internal structure
 *                        associated with the device to be operated on.
 *                        Before calling vmk_DeviceRegister(), the
 *                        driver must assign this pointer to the
 *                        driverData member of vmk_RDMADeviceRegData.
 * \param[in]  mrData     Data returned in mrData of
 *                        vmk_RDMAOpGetDmaMemRegion for the memory region
 *                        to be deregistered.
 *
 * \retval VMK_OK         On success.
 * \retval VMK_BAD_PARAM  Invalid memory region.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_RDMAOpDeregMemRegion)(vmk_AddrCookie driverData,
                                                     vmk_AddrCookie mrData);


/**
 * \brief RDMA device operations.
 */
typedef struct vmk_RDMADeviceOps {
   vmk_RDMAOpAssociate associate;
   vmk_RDMAOpDisassociate disassociate;
   vmk_RDMAOpGetStats getStats;
   vmk_RDMAOpCreateBind createBind;
   vmk_RDMAOpModifyBind modifyBind;
   vmk_RDMAOpDestroyBind destroyBind;
   vmk_RDMAOpQueryDevice queryDevice;
   vmk_RDMAOpQueryPort queryPort;
   vmk_RDMAOpGetLinkLayer getLinkLayer;
   vmk_RDMAOpQueryGid queryGid;
   vmk_RDMAOpQueryPkey queryPkey;
   vmk_RDMAOpAllocProtDomain allocPd;
   vmk_RDMAOpFreeProtDomain freePd;
   vmk_RDMAOpCreateAddrHandle createAh;
   vmk_RDMAOpQueryAddrHandle queryAh;
   vmk_RDMAOpDestroyAddrHandle destroyAh;
   vmk_RDMAOpCreateQueuePair createQp;
   vmk_RDMAOpModifyQueuePair modifyQp;
   vmk_RDMAOpQueryQueuePair queryQp;
   vmk_RDMAOpDestroyQueuePair destroyQp;
   vmk_RDMAOpPostSend postSend;
   vmk_RDMAOpPostRecv postRecv;
   vmk_RDMAOpCreateComplQueue createCq;
   vmk_RDMAOpDestroyComplQueue destroyCq;
   vmk_RDMAOpPollComplQueue pollCq;
   vmk_RDMAOpReqNotifyComplQueue reqNotifyCq;
   vmk_RDMAOpGetDmaMemRegion getDmaMemRegion;
   vmk_RDMAOpQueryMemRegion queryMemRegion;
   vmk_RDMAOpDeregMemRegion deregMemRegion;
} vmk_RDMADeviceOps;


/**
 * \brief Maximum RDMA node description size.
 */
#define VMK_RDMA_NODE_DESCRIPTION_MAX 64


/**
 * \brief RDMA logical device registration data.
 *
 * \note Before calling vmk_DeviceRegister, device driver needs to allocate
 *       and populate this structure. Then assign its pointer to member
 *       registrationData of structure vmk_DeviceProps, a parameter passed
 *       to vmk_DeviceRegister.
 */
typedef struct vmk_RDMADeviceRegData {
   /** Indicates vmkapi revision number driver was compiled with.  */
   vmk_revnum apiRevision;
   /** Module ID of device driver. */
   vmk_ModuleID moduleID;
   /** Type of RDMA node. */
   vmk_RDMANodeType nodeType;
   /** RDMA device operations. */
   vmk_RDMADeviceOps ops;
   /** DMA Engine used for mappings for underlying device. */
   vmk_DMAEngine dmaEngine;
   /** DMA Engine used for coherent mappings for underlying device. */
   vmk_DMAEngine dmaEngineCoherent;
   /**
    * Private driver context data defined by driver. It will
    * be passed to all callbacks into the driver.
    */
   vmk_AddrCookie driverData;
   /** Description of the underlying device. */
   char description[VMK_RDMA_NODE_DESCRIPTION_MAX];
} vmk_RDMADeviceRegData;


/*
 ***********************************************************************
 * vmk_RDMAClientOpAddDevice --                                   */ /**
 *
 * \brief Adds an RDMA device to a registered client.
 *
 * \note  This callback may not block
 *
 * \param[in] device   Device handle that has been added to system.
 *
 ***********************************************************************
 */
typedef void (*vmk_RDMAClientOpAddDevice)(vmk_AddrCookie cookie,
                                          vmk_RDMADevice device);


/*
 ***********************************************************************
 * vmk_RDMAClientOpRemoveDevice --                                */ /**
 *
 * \brief Removes an RDMA device from a registered client.
 *
 * \note  This callback may not block
 *
 * \param[in] device   Device handle that has been added to system.
 *
 ***********************************************************************
 */
typedef void (*vmk_RDMAClientOpRemoveDevice)(vmk_AddrCookie cookie,
                                             vmk_RDMADevice device);


/**
 * \brief RDMA client operations.
 */
typedef struct vmk_RDMAClientOps {
   /** \brief Called for each device added to system. */
   vmk_RDMAClientOpAddDevice addDevice;
   /** \brief Called for each device removed from system. */
   vmk_RDMAClientOpRemoveDevice removeDevice;
} vmk_RDMAClientOps;


/**
 * \brief RDMA client registration properties.
 */
typedef struct vmk_RDMAClientProps {
   /** \brief Name of this RDMA client. */
   vmk_Name name;
   /** \brief Module identifier for this client. */
   vmk_ModuleID moduleID;
   /**
    * \brief Heap to perform client allocations from.
    *
    * The amount of memory required from this heap for client
    * registration can be obtained via vmk_RDMAClientAllocSize().
    */
   vmk_HeapID heapID;
   /** \brief Operations table for clients. */
   vmk_RDMAClientOps ops;
   /** \brief Opaque data passed to client operations. */
   vmk_AddrCookie clientData;
} vmk_RDMAClientProps;


/**
 * \brief RDMA Event Type.
 */
typedef enum vmk_RDMAEventType {
   VMK_RDMA_EVENT_CQ_ERR,
   VMK_RDMA_EVENT_QP_FATAL,
   VMK_RDMA_EVENT_QP_REQ_ERR,
   VMK_RDMA_EVENT_QP_ACCESS_ERR,
   VMK_RDMA_EVENT_COMM_EST,
   VMK_RDMA_EVENT_SQ_DRAINED,
   VMK_RDMA_EVENT_PATH_MIG,
   VMK_RDMA_EVENT_PATH_MIG_ERR,
   VMK_RDMA_EVENT_DEVICE_FATAL,
   VMK_RDMA_EVENT_PORT_ACTIVE,
   VMK_RDMA_EVENT_PORT_ERR,
   VMK_RDMA_EVENT_LID_CHANGE,
   VMK_RDMA_EVENT_PKEY_CHANGE,
   VMK_RDMA_EVENT_SM_CHANGE,
   VMK_RDMA_EVENT_SRQ_ERR,
   VMK_RDMA_EVENT_SRQ_LIMIT_REACHED,
   VMK_RDMA_EVENT_QP_LAST_WQE_REACHED,
   VMK_RDMA_EVENT_CLIENT_REREGISTER,
} vmk_RDMAEventType;

/**
 * \brief RDMA Event.
 */
typedef struct vmk_RDMAEvent {
   vmk_RDMADevice device;
   union {
      vmk_RDMAComplQueue cq;
      vmk_RDMAQueuePair qp;
   } element;
   vmk_RDMAEventType event;
} vmk_RDMAEvent;

/*
 ***********************************************************************
 * vmk_RDMAEventHandlerCB --                                      */ /**
 *
 * \brief Callback to be invoked when an RDMA device dispatches an event.
 *
 * \note  This callback may not block.
 *
 * \param[in] eventHandlerData   Data defining this event handler.
 * \param[in] event              Event that was dispatched.
 *
 * \retval VMK_OK         Event Handler callback successful.
 * \retval VMK_BAD_PARAM  Invalid eventHandlerData or event.
 *
 ***********************************************************************
 */
typedef void (*vmk_RDMAEventHandlerCB)(vmk_AddrCookie eventHandlerData,
                                       vmk_RDMAEvent *event);

/**
 * \brief RDMA Event Handler Registration Properties.
 */
typedef struct vmk_RDMAEventHandlerProps {
   /** \brief Module identifier for this event handler. */
   vmk_ModuleID moduleID;
   /**
    * \brief Heap to perform event handler allocations from.
    *
    * The amount of memory required from this heap for event handler
    * registration can be obtained via vmk_RDMAEventHandlerAllocSize().
    */
   vmk_HeapID heapID;
   /** \brief Operations table for clients. */
   vmk_RDMAEventHandlerCB cb;
   /** \brief Opaque data passed to client operations. */
   vmk_AddrCookie eventHandlerData;
} vmk_RDMAEventHandlerProps;

/**
 * \brief RDMA UD header packet field lengths.
 */
typedef enum vmk_RDMAUDHeaderPacketLengths {
   VMK_RDMA_LRH_BYTES  = 8,
   VMK_RDMA_ETH_BYTES  = 14,
   VMK_RDMA_VLAN_BYTES = 4,
   VMK_RDMA_GRH_BYTES  = 40,
   VMK_RDMA_BTH_BYTES  = 12,
   VMK_RDMA_DETH_BYTES = 8
} vmk_RDMAUDHeaderPacketLengths;

#define VMK_RDMA_OPCODE(transport, op) \
   VMK_RDMA_OPCODE_ ## transport ## _ ## op = \
      VMK_RDMA_OPCODE_ ## transport + VMK_RDMA_OPCODE_ ## op

/**
 * \brief RDMA transport opcodes.
 */
typedef enum vmk_RDMAOpcodes {
   /* transport types -- just used to define real constants */
   VMK_RDMA_OPCODE_RC                                = 0x00,
   VMK_RDMA_OPCODE_UC                                = 0x20,
   VMK_RDMA_OPCODE_RD                                = 0x40,
   VMK_RDMA_OPCODE_UD                                = 0x60,

   /* operations -- just used to define real constants */
   VMK_RDMA_OPCODE_SEND_FIRST                        = 0x00,
   VMK_RDMA_OPCODE_SEND_MIDDLE                       = 0x01,
   VMK_RDMA_OPCODE_SEND_LAST                         = 0x02,
   VMK_RDMA_OPCODE_SEND_LAST_WITH_IMMEDIATE          = 0x03,
   VMK_RDMA_OPCODE_SEND_ONLY                         = 0x04,
   VMK_RDMA_OPCODE_SEND_ONLY_WITH_IMMEDIATE          = 0x05,
   VMK_RDMA_OPCODE_RDMA_WRITE_FIRST                  = 0x06,
   VMK_RDMA_OPCODE_RDMA_WRITE_MIDDLE                 = 0x07,
   VMK_RDMA_OPCODE_RDMA_WRITE_LAST                   = 0x08,
   VMK_RDMA_OPCODE_RDMA_WRITE_LAST_WITH_IMMEDIATE    = 0x09,
   VMK_RDMA_OPCODE_RDMA_WRITE_ONLY                   = 0x0a,
   VMK_RDMA_OPCODE_RDMA_WRITE_ONLY_WITH_IMMEDIATE    = 0x0b,
   VMK_RDMA_OPCODE_RDMA_READ_REQUEST                 = 0x0c,
   VMK_RDMA_OPCODE_RDMA_READ_RESPONSE_FIRST          = 0x0d,
   VMK_RDMA_OPCODE_RDMA_READ_RESPONSE_MIDDLE         = 0x0e,
   VMK_RDMA_OPCODE_RDMA_READ_RESPONSE_LAST           = 0x0f,
   VMK_RDMA_OPCODE_RDMA_READ_RESPONSE_ONLY           = 0x10,
   VMK_RDMA_OPCODE_ACKNOWLEDGE                       = 0x11,
   VMK_RDMA_OPCODE_ATOMIC_ACKNOWLEDGE                = 0x12,
   VMK_RDMA_OPCODE_COMPARE_SWAP                      = 0x13,
   VMK_RDMA_OPCODE_FETCH_ADD                         = 0x14,

   /* real constants follow -- see comment about above VMK_RDMA_OPCODE()
      macro for more details */

   /* RC */
   VMK_RDMA_OPCODE(RC, SEND_FIRST),
   VMK_RDMA_OPCODE(RC, SEND_MIDDLE),
   VMK_RDMA_OPCODE(RC, SEND_LAST),
   VMK_RDMA_OPCODE(RC, SEND_LAST_WITH_IMMEDIATE),
   VMK_RDMA_OPCODE(RC, SEND_ONLY),
   VMK_RDMA_OPCODE(RC, SEND_ONLY_WITH_IMMEDIATE),
   VMK_RDMA_OPCODE(RC, RDMA_WRITE_FIRST),
   VMK_RDMA_OPCODE(RC, RDMA_WRITE_MIDDLE),
   VMK_RDMA_OPCODE(RC, RDMA_WRITE_LAST),
   VMK_RDMA_OPCODE(RC, RDMA_WRITE_LAST_WITH_IMMEDIATE),
   VMK_RDMA_OPCODE(RC, RDMA_WRITE_ONLY),
   VMK_RDMA_OPCODE(RC, RDMA_WRITE_ONLY_WITH_IMMEDIATE),
   VMK_RDMA_OPCODE(RC, RDMA_READ_REQUEST),
   VMK_RDMA_OPCODE(RC, RDMA_READ_RESPONSE_FIRST),
   VMK_RDMA_OPCODE(RC, RDMA_READ_RESPONSE_MIDDLE),
   VMK_RDMA_OPCODE(RC, RDMA_READ_RESPONSE_LAST),
   VMK_RDMA_OPCODE(RC, RDMA_READ_RESPONSE_ONLY),
   VMK_RDMA_OPCODE(RC, ACKNOWLEDGE),
   VMK_RDMA_OPCODE(RC, ATOMIC_ACKNOWLEDGE),
   VMK_RDMA_OPCODE(RC, COMPARE_SWAP),
   VMK_RDMA_OPCODE(RC, FETCH_ADD),

   /* UC */
   VMK_RDMA_OPCODE(UC, SEND_FIRST),
   VMK_RDMA_OPCODE(UC, SEND_MIDDLE),
   VMK_RDMA_OPCODE(UC, SEND_LAST),
   VMK_RDMA_OPCODE(UC, SEND_LAST_WITH_IMMEDIATE),
   VMK_RDMA_OPCODE(UC, SEND_ONLY),
   VMK_RDMA_OPCODE(UC, SEND_ONLY_WITH_IMMEDIATE),
   VMK_RDMA_OPCODE(UC, RDMA_WRITE_FIRST),
   VMK_RDMA_OPCODE(UC, RDMA_WRITE_MIDDLE),
   VMK_RDMA_OPCODE(UC, RDMA_WRITE_LAST),
   VMK_RDMA_OPCODE(UC, RDMA_WRITE_LAST_WITH_IMMEDIATE),
   VMK_RDMA_OPCODE(UC, RDMA_WRITE_ONLY),
   VMK_RDMA_OPCODE(UC, RDMA_WRITE_ONLY_WITH_IMMEDIATE),

   /* RD */
   VMK_RDMA_OPCODE(RD, SEND_FIRST),
   VMK_RDMA_OPCODE(RD, SEND_MIDDLE),
   VMK_RDMA_OPCODE(RD, SEND_LAST),
   VMK_RDMA_OPCODE(RD, SEND_LAST_WITH_IMMEDIATE),
   VMK_RDMA_OPCODE(RD, SEND_ONLY),
   VMK_RDMA_OPCODE(RD, SEND_ONLY_WITH_IMMEDIATE),
   VMK_RDMA_OPCODE(RD, RDMA_WRITE_FIRST),
   VMK_RDMA_OPCODE(RD, RDMA_WRITE_MIDDLE),
   VMK_RDMA_OPCODE(RD, RDMA_WRITE_LAST),
   VMK_RDMA_OPCODE(RD, RDMA_WRITE_LAST_WITH_IMMEDIATE),
   VMK_RDMA_OPCODE(RD, RDMA_WRITE_ONLY),
   VMK_RDMA_OPCODE(RD, RDMA_WRITE_ONLY_WITH_IMMEDIATE),
   VMK_RDMA_OPCODE(RD, RDMA_READ_REQUEST),
   VMK_RDMA_OPCODE(RD, RDMA_READ_RESPONSE_FIRST),
   VMK_RDMA_OPCODE(RD, RDMA_READ_RESPONSE_MIDDLE),
   VMK_RDMA_OPCODE(RD, RDMA_READ_RESPONSE_LAST),
   VMK_RDMA_OPCODE(RD, RDMA_READ_RESPONSE_ONLY),
   VMK_RDMA_OPCODE(RD, ACKNOWLEDGE),
   VMK_RDMA_OPCODE(RD, ATOMIC_ACKNOWLEDGE),
   VMK_RDMA_OPCODE(RD, COMPARE_SWAP),
   VMK_RDMA_OPCODE(RD, FETCH_ADD),

   /* UD */
   VMK_RDMA_OPCODE(UD, SEND_ONLY),
   VMK_RDMA_OPCODE(UD, SEND_ONLY_WITH_IMMEDIATE),
} vmk_RDMAOpcodes;

/**
 * \brief RDMA Link Next Header (LNH) flags.
 */
typedef enum vmk_RDMALNHFlags {
   VMK_RDMA_LNH_RAW        = 0,
   VMK_RDMA_LNH_IP         = 1,
   VMK_RDMA_LNH_IBA_LOCAL  = 2,
   VMK_RDMA_LNH_IBA_GLOBAL = 3,
} vmk_RDMALNHFlags;

/**
 * \brief RDMA Unpacked Local Routing Header (LRH) type.
 */
typedef struct vmk_RDMAUnpackedLRH {
   vmk_uint8   virtualLane;
   vmk_uint8   linkVersion;
   vmk_uint8   serviceLevel;
   vmk_uint8   linkNextHeader;
   vmk_uint16  destinationLid;
   vmk_uint16  packetLength;
   vmk_uint16  sourceLid;
} vmk_RDMAUnpackedLRH;

/**
 * \brief RDMA Unpacked Global Routing Header (GRH) type.
 */
typedef struct vmk_RDMAUnpackedGRH {
   vmk_uint8   ipVersion;
   vmk_uint8   trafficClass;
   vmk_uint32  flowLabel;
   vmk_uint16  payloadLength;
   vmk_uint8   nextHeader;
   vmk_uint8   hopLimit;
   vmk_RDMAGid sourceGID;
   vmk_RDMAGid destinationGID;
} vmk_RDMAUnpackedGRH;

/**
 * \brief RDMA Unpacked BTH type.
 */
typedef struct vmk_RDMAUnpackedBTH {
   vmk_uint8   opcode;
   vmk_uint8   solicitedEvent;
   vmk_uint8   migReq;
   vmk_uint8   padCount;
   vmk_uint8   transportHeaderVersion;
   vmk_uint16  pkey;
   vmk_uint32  destinationQpn;
   vmk_uint8   ackReq;
   vmk_uint32  psn;
} vmk_RDMAUnpackedBTH;

/**
 * \brief RDMA Unpacked DETH type.
 */
typedef struct vmk_RDMAUnpackedDETH {
   vmk_uint32  qkey;
   vmk_uint32  sourceQpn;
} vmk_RDMAUnpackedDETH;

/**
 * \brief RDMA Unpacked VLAN type.
 */
typedef struct vmk_RDMAUnpackedVLAN {
   vmk_uint16  tag;
   vmk_uint16  type;
} vmk_RDMAUnpackedVLAN;

/**
 * \brief RDMA Unpacked ETH type.
 */
typedef struct vmk_RDMAUnpackedETH {
   vmk_uint8   dmacHigh[4];
   vmk_uint8   dmacLow[2];
   vmk_uint8   smacHigh[2];
   vmk_uint8   smacLow[4];
   vmk_uint16  type;
} vmk_RDMAUnpackedETH;

/**
 * \brief RDMA UD Header Type.
 */
typedef struct vmk_RDMAUDHeader {
   vmk_uint8            lrhPresent;
   vmk_RDMAUnpackedLRH  lrh;
   vmk_uint8            ethPresent;
   vmk_RDMAUnpackedETH  eth;
   vmk_uint8            vlanPresent;
   vmk_RDMAUnpackedVLAN vlan;
   vmk_uint8            grhPresent;
   vmk_RDMAUnpackedGRH  grh;
   vmk_RDMAUnpackedBTH  bth;
   vmk_RDMAUnpackedDETH deth;
   vmk_uint8            immediatePresent;
   vmk_uint32           immediateData;
} vmk_RDMAUDHeader;

/*
 ***********************************************************************
 * vmk_RDMAOpGetPrivStats --                                      */ /**
 *
 * \brief Obtain private statistics for specified RDMA device.
 *
 * \note  This callback may block
 *
 * \param[in]  driverData Points to the driver internal structure
 *                        associated with the device to be operated on.
 *                        Before calling vmk_DeviceRegister(), the
 *                        driver must assign this pointer to the
 *                        driverData member of vmk_RDMADeviceRegData.
 * \param[out] statBuf    Returned private statistics.
 * \param[in]  length     Length of private statistics in bytes.
 *
 * \retval VMK_OK         On success.
 * \retval VMK_BAD_PARAM  Invalid parameters.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus
(*vmk_RDMAOpGetPrivStats)(vmk_AddrCookie driverData,
                          char *statBuf,
                          vmk_ByteCount length);

/*
 ***********************************************************************
 * vmk_RDMAOpGetPrivStatsLength --                                */ /**
 *
 * \brief Obtain private statistics length for specified RDMA device.
 *
 * \note  This callback may block.
 *
 * \param[in]  driverData Points to the driver internal structure
 *                        associated with the device to be operated on.
 *                        Before calling vmk_DeviceRegister(), the
 *                        driver must assign this pointer to the
 *                        driverData member of vmk_RDMADeviceRegData.
 * \param[out] length     Length of private statistics in bytes.
 *
 * \retval VMK_OK         On success.
 * \retval VMK_BAD_PARAM  Invalid parameters.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_RDMAOpGetPrivStatsLength)(vmk_AddrCookie driverData,
                                                         vmk_ByteCount *length);

/**
 * \brief RDMA private statistics operations.
 */
typedef struct vmk_RDMAPrivStatsOps {
   /** Handler used by vmkernel to get driver's private stats length. */
   vmk_RDMAOpGetPrivStatsLength getPrivStatsLength;

   /** Handler used by vmkernel to get driver's private stats. */
   vmk_RDMAOpGetPrivStats getPrivStats;
} vmk_RDMAPrivStatsOps;


/*
 ***********************************************************************
 * vmk_RDMAOpGetPairedUplink --                                   */ /**
 *
 * \brief Obtain paired uplink for specified RDMA device.
 *
 * \note  This callback may block.
 *
 * \param[in]  driverData Points to the driver internal structure
 *                        associated with the device to be operated on.
 *                        Before calling vmk_DeviceRegister(), the
 *                        driver must assign this pointer to the
 *                        driverData member of vmk_RDMADeviceRegData.
 * \param[out] uplink     Handle of the uplink device.
 *
 * \retval VMK_OK         On success.
 * \retval VMK_BAD_PARAM  Invalid parameters.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_RDMAOpGetPairedUplink)(vmk_AddrCookie driverData,
                                                      vmk_Uplink *uplink);

/**
 * \brief RDMA RoCE operations.
 */
typedef struct vmk_RDMARoceOps {
   /** Handler used by vmkernel to get Uplink paired with RDMA device. */
   vmk_RDMAOpGetPairedUplink getPairedUplink;
} vmk_RDMARoceOps;

/**
 * \brief RDMA Capabilities.
 */
typedef enum vmk_RDMACap {
   /** Driver supports accessing private statistics. */
   VMK_RDMA_CAP_PRIV_STATS        = 1 << 0,
   /** Driver supports RoCE. */
   VMK_RDMA_CAP_ROCE              = 1 << 1,
} vmk_RDMACap;

#endif /* _VMKAPI_RDMA_TYPES_H_ */
/** @} */
/** @} */
