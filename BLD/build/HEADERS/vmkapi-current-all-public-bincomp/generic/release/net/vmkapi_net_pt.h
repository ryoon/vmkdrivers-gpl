/* **********************************************************
 * Copyright 2009 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Passthru                                                        */ /**
 * \addtogroup Network
 *@{
 * \defgroup Passthru Networking Passthrough
 *
 * Networking passthrough includes Uniform Passthrough (UPT) and
 * Network Plugin Architecture (NPA).
 *@{
 *
 ***********************************************************************
 */

#ifndef _VMKAPI_NET_PT_H_
#define _VMKAPI_NET_PT_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "net/vmkapi_net_types.h"

/** Definition of maximum number passthrough regions for a VF */
#define VMK_VF_MAX_PT_REGIONS 6

/** Definition of RSS indirection table maximum size */
#define VMK_NETVF_RSS_MAX_IND_TABLE_SIZE 128

/** Definition of RSS hash key maximum size */
#define VMK_NETVF_RSS_MAX_KEY_SIZE 40

/** Maximum length of a plugin name */
#define VMK_NPA_MAX_PLUGIN_NAME_LEN 256

/** Maximum size of the plugin data (passed from PF driver to plugin)
    in 32-bit words */
#define VMK_NPA_MAX_PLUGIN_DATA_SIZE 32

/** Number of possible multicast MAC filters per VF (NPA) */
#define VMK_NPA_MAX_MULTICAST_FILTERS 32

/** Bit to set in cfgChanged when configuring MAC address of VF */
#define VMK_CFG_MAC_CHANGED		(1 << 0)

/** Bit to set in cfgChanged when configuring Default VLAN (VST) for VF */
#define VMK_CFG_DEFAULT_VLAN_CHANGED	(1 << 1)

/** Bit to set in cfgChanged when adding Guest VLAN (VGT) tags for VF */
#define VMK_CFG_GUEST_VLAN_ADD		(1 << 2)

/** Bit to set in cfgChanged when removing Guest VLAN (VGT) tags for VF */
#define VMK_CFG_GUEST_VLAN_REMOVE	(1 << 3)

/** Bit to set in cfgChanged when changing receive mode of VF */
#define VMK_CFG_RXMODE_CHANGED		(1 << 4)

/** Bit to set in cfgChanged when changing MTU of VF */
#define VMK_CFG_MTU_CHANGED		(1 << 5)

/** Bit to set in cfgChanged when disabling default VLAN for VF */
#define VMK_CFG_DISABLE_DEFAULT_VLAN	(1 << 6)



/**
 * \ingroup Passthru
 * \brief VF identifier
 *
 * This identifier is unique among VFs owned by a PF
 */
typedef vmk_uint32 vmk_VFID;

/**
 * \ingroup Passthru
 * \brief VF Descriptor
 *
 * A VF descriptor uniquely identifies a VF on a host.
 */

typedef struct vmk_VFDesc {
   /** The PCI segment number of the PF */
   vmk_uint16  pfSegment;
   /** The PCI bus number of the PF */
   vmk_uint8   pfBus;
   /** The PCI device number of the PF */
   vmk_uint8   pfDev;
   /** The PCI function number of the PF */
   vmk_uint8   pfFunc;
   /** The identifier of the VF */
   vmk_uint32  vfID;
   /** Switch PortID where hiddenUplink is connected */
   vmk_SwitchPortID portID;
} vmk_VFDesc;

/**
 * \ingroup Passthru
 * \brief Passthru operations
 *
 * Each value corresponds to an operation that needs to be performed
 * by the driver.
 *
 * The driver is called through a callback of the following form:
 *
 * VMK_ReturnStatus (*vmk_UplinkPTOpFunc)(void *clientData, 
 *                                        vmk_NetPTOP op, 
 *                                        void *args);
 *
 * `clientData' is the device (struct net_device *) in the case of a
 * network driver.
 * `args' is a pointer to an operation-specific argument
 * structure. Please refer to the documentation of these structures
 * (vmk_NetPTOP...Args) to see which one match to each operation.
 */
typedef enum vmk_NetPTOP {
   /** Is passthrough supported */
   VMK_NETPTOP_IS_SUPPORTED           = 0x00000001,
  
   /** Allocate a VF */
   VMK_NETPTOP_VF_ACQUIRE             = 0x00000002,
  
   /** Initialize a VF */
   VMK_NETPTOP_VF_INIT                = 0x00000003,
  
   /** Activate a VF */
   VMK_NETPTOP_VF_ACTIVATE            = 0x00000004,
  
   /** Quiesce a VF */
   VMK_NETPTOP_VF_QUIESCE             = 0x00000005,
  
   /** Save state of a VF */
   VMK_NETPTOP_VF_SAVE_STATE          = 0x00000006,
  
   /** Release a VF */
   VMK_NETPTOP_VF_RELEASE             = 0x00000007,
  
   /** Get VF information */
   VMK_NETPTOP_VF_GET_INFO            = 0x00000008,
  
   /** Set RSS indirection table for a VF */
   VMK_NETPTOP_VF_SET_RSS_IND_TABLE   = 0x00000009,
  
   /** Get traffic statistics of a queue */
   VMK_NETPTOP_VF_GET_QUEUE_STATS     = 0x0000000a,
  
   /** Get status of a queue */
   VMK_NETPTOP_VF_GET_QUEUE_STATUS    = 0x0000000b,
  
   /** Set the MAC address of a VF */
   VMK_NETPTOP_VF_SET_MAC             = 0x0000000c,
  
   /** Set multicast MAC addresses filtered for a VF port */
   VMK_NETPTOP_VF_SET_MULTICAST       = 0x0000000d,
  
   /** Control RX mode of a VF port */
   VMK_NETPTOP_VF_SET_RX_MODE         = 0x0000000e,
  
   /** Add an authorized VLAN range to a VF port */
   VMK_NETPTOP_VF_ADD_VLAN_RANGE      = 0x0000000f,
  
   /** Remove an authorized VLAN range from a VF port */
   VMK_NETPTOP_VF_DEL_VLAN_RANGE      = 0x00000010,
  
   /** Setup VLAN insertion/stripping for a VF port */
   VMK_NETPTOP_VF_SET_DEFAULT_VLAN    = 0x00000011,
 
   /** Enable MAC-address anti-spoofing on a VF port */
   VMK_NETPTOP_VF_SET_ANTISPOOF       = 0x00000012,
  
   /** Set interrupt moderation for a VF */
   VMK_NETPTOP_VF_SET_IML             = 0x00000013,

   /** Add a MAC address filter to the PF */
   VMK_NETPTOP_PF_ADD_MAC_FILTER      = 0x00000014,
  
   /** Delete a MAC address filter from the PF */
   VMK_NETPTOP_PF_DEL_MAC_FILTER      = 0x00000015,
  
   /** Enable mirroring all traffic to the PF */
   VMK_NETPTOP_PF_MIRROR_ALL          = 0x00000016,

   /** Set MTU for a VF port */
   VMK_NETPTOP_VF_SET_MTU             = 0x00000017

} vmk_NetPTOP;

/**
 * \ingroup Passthru
 * \brief Enumeration of VF features.
 */
typedef enum vmk_NetVFFeatures {
   /** Supports RX checksum offload */
   VMK_NETVF_F_RXCSUM = 0x0001,
  
   /** Supports Receive Side Scaling */
   VMK_NETVF_F_RSS    = 0x0002,
  
   /** Supports RX VLAN acceleration */
   VMK_NETVF_F_RXVLAN = 0x0004,
  
   /** Supports Large Receive Offload */
   VMK_NETVF_F_LRO    = 0x0008
  
} vmk_NetVFFeatures;
  
/**
 * \ingroup Passthru
 * \brief Type for VF requested settings.
 *
 * This structure contains the main characteristics of a VF in the
 * case of VF allocation and initialization.
 *
 * The `version' field can be used to distinguish between the
 * different API versions on VF allocation (VMK_NETPTOP_VF_ACQUIRE).
 */
typedef struct vmk_NetVFRequirements {
   /** Version of the UPT/NPA code */
   vmk_uint32 version;
  
   /** Features requested */
   vmk_NetVFFeatures features;
  
   /** Features optionally requested */
   vmk_NetVFFeatures optFeatures;
  
   /** MTU requested */
   vmk_uint16 mtu;

   /** Number of TX queues requested */
   vmk_uint8 numTxQueues;

   /** Number of RX queues requested */
   vmk_uint8 numRxQueues;

   /** Number of requested interrupts */
   vmk_uint8 numIntrs;

   /** Reserved for future use */
   vmk_uint32 _rsvd[4];

} vmk_NetVFRequirements;

/**
 * \ingroup Passthru
 * \brief Enumeration of Interrupt Moderation Level (IML).
 */
typedef enum vmk_NetVFIML {
   /** No interrupt moderation */
   VMK_NETVF_IML_NONE = 0,
   
   /** Most interrupts generated */
   VMK_NETVF_IML_LOWEST = 1,

   /** Least interrupts generated */
   VMK_NETVF_IML_HIGHEST = 7,

   /** Adaptive moderation */
   VMK_NETVF_IML_ADAPTIVE = 8,

   /** Custom moderation */
   VMK_NETVF_IML_CUSTOM = 9,
} vmk_NetVFIML;

/**
 * \ingroup Passthru
 * \brief Type for ring descriptor (UPT only).
 *
 * This structure is used in UPT to save or restore the parameters of
 * a ring queue.
 */
typedef struct vmk_UPTVFRingDesc {
   /** Physical address of the ring */
   vmk_uint64 basePA;

   /** Number of descriptors */
   vmk_uint32 size;

   /** Index of the desriptor the producer will write to next */
   vmk_uint32 prodIdx;

   /** Index of the desriptor the consumer will read next */
   vmk_uint32 consIdx;

} vmk_UPTVFRingDesc;

/**
 * \ingroup Passthru
 * \brief Type for RX queue settings (UPT).
 *
 * This structure is used in UPT to initialize or restore the
 * parameters of a RX queue for a VF.
 */
typedef struct vmk_UPTVFRXQueueParams {
   /** First RX command ring */
   vmk_UPTVFRingDesc rxRing;

   /** Second RX command ring */
   vmk_UPTVFRingDesc rxRing2;

   /** RX completion ring */
   vmk_UPTVFRingDesc compRing;

   /** Value of GEN bit for the completion ring */
   vmk_uint32 compGen;

   /** Index of the MSI/MSI-X vector for this queue */
   vmk_uint8 intrIdx;

   /** Whether or not the queue is stopped */
   vmk_Bool stopped;

   /** Reserved for future use */
   vmk_uint32 _rsvd[4];

} vmk_UPTVFRXQueueParams;

/**
 * \ingroup Passthru
 * \brief Type for TX queue settings (UPT).
 *
 * This structure is used in UPT to initialize or restore the parameters of
 * a TX queue for a VF.
 */
typedef struct vmk_UPTVFTXQueueParams {
   /** TX command ring */
   vmk_UPTVFRingDesc txRing;

   /** TX data ring */
   vmk_UPTVFRingDesc dataRing;

   /** TX completion ring */
   vmk_UPTVFRingDesc compRing;

   /** Value of GEN bit for the completion ring */
   vmk_uint32 compGen;

   /** Index of the MSI/MSI-X vector for this queue */
   vmk_uint8 intrIdx;

   /** Whether or not the queue is stopped */
   vmk_Bool stopped;

   /** Reserved for future use */
   vmk_uint32 _rsvd[4];

} vmk_UPTVFTXQueueParams;

/**
 * \ingroup Passthru
 * \brief Type for queue settings (NPA).
 *
 * This structure is used in NPA to initialize a queue.
 *
 */
typedef struct vmk_NPAVFQueueParams {
   /** Physical address of the ring */
   vmk_uint64 basePA;

   /** Size of the ring */
   vmk_uint32 size;

   /** Length in bytes of the ring */
   vmk_ByteCountSmall length;

   /** Reserved for future use */
   vmk_uint32 _rsvd[4];

} vmk_NPAVFQueueParams;

/**
 * \ingroup Passthru
 * \brief Type for interrupt moderation settings.
 */

typedef struct vmk_VFIntrModParams {
   /** Interrupt moderation level for the vector. */
   vmk_NetVFIML modLevel;

   /** The interrupt rate, valid only if modLevel is VMK_NETVF_IML_CUSTOM */
   vmk_uint32 intrRate;

} vmk_VFIntrModParams;

/**
 * \ingroup Passthru
 * \brief Type for interrupt vector settings.
 *
 * This structure is used in UPT to initialize of restore an interrupt
 * vector's parameters for a VF.
 */
typedef struct vmk_UPTVFIntrVectorParams {
   /** Value of IMR for the vector */
   vmk_uint32 imr;

   /** Is there an interrupt pending on this vector? */
   vmk_Bool pending;

   /** Interrupt moderation parameter for this vector. */
   vmk_VFIntrModParams intrMod;

} vmk_UPTVFIntrVectorParams;

/**
 * \ingroup Passthru
 * \brief Type for interrupts settings (UPT).
 *
 * This structure is used in UPT to initialize or restore interrupts
 * parameters of a VF for each vector.
 */
typedef struct vmk_UPTVFIntrParams {
   /** Interrupt scheme to use */
   vmk_PCIInterruptType intrType;

   /** Whether or not auto mask is used */
   vmk_Bool autoMask;

   /** (array) Parameters specific to each vector */
   vmk_UPTVFIntrVectorParams *vectors;

   /** Reserved for future use */
   vmk_uint32 _rsvd[4];

} vmk_UPTVFIntrParams;

/**
 * \ingroup Passthru
 * \brief Type for interrupts settings (NPA).
 *
 * This structure is used in NPA to initialize an interrupts vector's
 * parameters.
 */
typedef struct vmk_NPAVFIntrParams {
   vmk_VFIntrModParams  intrMod;
} vmk_NPAVFIntrParams;

/**
 * \ingroup Passthru
 * \brief Enumeration of RSS hash types.
 */
typedef enum vmk_NetVFRSSHashType {
   /** Disable RSS */
   VMK_NETVF_HASH_TYPE_NONE     = 0x00,

   /** RSS hash based on IP */
   VMK_NETVF_HASH_TYPE_IPV4     = 0x01,

   /** RSS hash based on TCP (with IP) */
   VMK_NETVF_HASH_TYPE_TCP_IPV4 = 0x02,

   /** RSS hash based on IPv6 */
   VMK_NETVF_HASH_TYPE_IPV6     = 0x04,

   /** RSS hash based on TCP (with IPv6) */
   VMK_NETVF_HASH_TYPE_TCP_IPV6 = 0x08

} vmk_NetVFRSSHashType;

/**
 * \ingroup Passthru
 * \brief Enumeration of RSS hash functions.
 */
typedef enum vmk_NetVFRSSHashFunc {
   /** Disable RSS */
   VMK_NETVF_HASH_FUNC_NONE     = 0x00,
   /** Use Toeplitz hash function */
   VMK_NETVF_HASH_FUNC_TOEPLITZ = 0x01

} vmk_NetVFRSSHashFunc;

/**
 * \ingroup Passthru
 * \brief Type for RSS hash key.
 *
 * This structure is used to program the RSS hash key.
 */
typedef struct vmk_NetVFRSSHashKey {
   /** Size of the key */
   vmk_uint16 keySize;

   /** Contents */
   vmk_uint8 key[VMK_NETVF_RSS_MAX_KEY_SIZE];

} vmk_NetVFRSSHashKey;

/**
 * \ingroup Passthru
 * \brief Type for RSS indirection table.
 *
 * This structure is used to program or update the RSS indirection
 * table.
 */
typedef struct vmk_NetVFRSSIndTable {
   /** Size of the table */
   vmk_uint16 indTableSize;

   /** Contents */
   vmk_uint8 indTable[VMK_NETVF_RSS_MAX_IND_TABLE_SIZE];

} vmk_NetVFRSSIndTable;

/**
 * \ingroup Passthru
 * \brief Type for RSS configuration.
 *
 * This structure contains all the necessary settings to program a
 * VF's RSS engine.
 */
typedef struct vmk_NetVFRSSParams {
   /** Hash type to be used */
   vmk_NetVFRSSHashType hashType;

   /** Hash function to be used */
   vmk_NetVFRSSHashFunc hashFunc;

   /** Hash key */
   vmk_NetVFRSSHashKey key;

   /** Indirection table */
   vmk_NetVFRSSIndTable indTable;

} vmk_NetVFRSSParams;

/**
 * \ingroup Passthru
 * \brief Type for VF settings.
 *
 * This structure is used in both UPT and NPA to initialize or restore
 * (in the case of UPT) the settings of a VF, including RX, TX queues,
 * interrupt vectors, RSS and more.
 *
 * Since a driver can only be UPT or NPA but not both at a time, there
 * is no field to distinguish whether to use u.upt or u.npa in the
 * union. The driver must always be aware of whether it is doing UPT
 * or NPA.
 */
typedef struct vmk_NetVFParameters {
   /** Number of TX queues requested */
   vmk_uint8 numTxQueues;

   /** Number of RX queues requested */
   vmk_uint8 numRxQueues;

   /** Number of requested vectors */
   vmk_uint8 numIntrs;

   /** Parameters for RSS */
   vmk_NetVFRSSParams   rss;

   union {
      /** UPT specific parameters */
      struct {
         /** (array of numRxQueues) Parameters for each RX queue */
         vmk_UPTVFRXQueueParams *rxQueues;

         /** (array of numTxQueues) Parameters for each TX queue */
         vmk_UPTVFTXQueueParams *txQueues;

         /** Parameters for interrupts */
         vmk_UPTVFIntrParams intr;

         /** Number of OOB pages allocated */
         vmk_uint8 numOOBPages;

         /** The starting guest physical address of the OOB pages. OOB pages are mapped contiguously */
         vmk_uint64 OOBStartPA;

         /** The VMkernel virtual address used to access the OOB pages */
         void *OOBMapped;

      } upt;

      /** NPA specific parameters */
      struct {
         /** (array of numRxQueues) Parameters for each RX queue */
         vmk_NPAVFQueueParams *rxQueues;

         /** (array of numTxQueues) Parameters for each TX queue */
         vmk_NPAVFQueueParams *txQueues;

         /** (array of numIntrs) Parameters for interrupts */
         vmk_NPAVFIntrParams *intr;

         /** Memory region shared with the plugin */
         void *sharedRegion;

         /** Length of the shared region in bytes */
         vmk_ByteCountSmall sharedRegionLength;

      } npa;

   } u;

   /** Features requested */
   vmk_NetVFFeatures features;
  
   /** Features optionally requested */
   vmk_NetVFFeatures optFeatures;
  
   /** MTU requested */
   vmk_uint16 mtu;

   /** Reserved for future use */
   vmk_uint8 _rsvd[6];

} vmk_NetVFParameters;

/**
 * \ingroup Passthru
 * \brief Type for VF passthrough MMIO region.
 */
typedef struct vmk_VFPTRegion {
   /** Region address */
   vmk_uint64 MA;

   /** Region size in pages */
   vmk_uint32 numPages;
} vmk_VFPTRegion;

/**
 * \ingroup Passthru
 * \brief Type for VF info.
 *
 * This structure is used to retrieve VF's information, including
 * passthrough regions, device address on the bus (SBDF), plugin
 * information etc...
 */
typedef struct vmk_NetVFInfo {
   /** Address and size of passthrough MMIO regions */
   vmk_VFPTRegion ptRegions[VMK_VF_MAX_PT_REGIONS];

   /** Number of regions used */
   vmk_uint8 numPtRegions;

   /** Segment,Bus,Device,Function of the VF */
   vmk_uint32 sbdf;

   union {
      /** UPT specific information */
      struct {
         /** The device revision */
         vmk_uint32 devRevision;

         /** Reserved, must set to 0 */
         vmk_uint32 reserved;

         /** The size of the out-of-band DMA buffer needed, in pages */
         vmk_uint8 numOOBPages;
      } upt;

      /** NPA specific information */
      struct {
         /** Plugin name */
         char pluginName[VMK_NPA_MAX_PLUGIN_NAME_LEN];

         /** Some opaque data to be passed to the plugin */
         vmk_uint32 pluginData[VMK_NPA_MAX_PLUGIN_DATA_SIZE];
      } npa;

   } u;

   /** Reserved for future use */
   vmk_uint32 _rsvd[4];

} vmk_NetVFInfo;

/**
 * \ingroup Passthru
 * \brief Enumeration of queue types.
 */
typedef enum vmk_NetVFQueueType {
   /** Queue is RX */
   VMK_NETVF_QUEUE_TYPE_RX,

   /** Queue is TX */
   VMK_NETVF_QUEUE_TYPE_TX

} vmk_NetVFQueueType;

/**
 * \ingroup Passthru
 * \brief Type for RX statistics for a queue.
 *
 * This structure is used to save, restore or retrieve statistics of
 * a VF's RX queue.
 */
typedef struct vmk_NetVFRXQueueStats {
   /** Unicast packets received */
   vmk_uint64 unicastPkts;

   /** Unicast bytes received */
   vmk_uint64 unicastBytes;

   /** Multicast packets received */
   vmk_uint64 multicastPkts;

   /** Multicast bytes received */
   vmk_uint64 multicastBytes;

   /** Broadcast packets received */
   vmk_uint64 broadcastPkts;

   /** Broadcast bytes received */
   vmk_uint64 broadcastBytes;

   /** Packets dropped due to buffer shortage */
   vmk_uint64 outOfBufferDrops;

   /** Packets dropped due to other errors */
   vmk_uint64 errorDrops;

   /** Number of packets aggregated by LRO */
   vmk_uint64 LROPkts;

   /** Bytes received from LRO packets */
   vmk_uint64 LROBytes;

   /** Reserved for future use */
   vmk_uint32 _rsvd[4];

} vmk_NetVFRXQueueStats;

/**
 * \ingroup Passthru
 * \brief Type for storing RX statistics for a queue.
 *
 * This structure is used to save, restore or retrieve statistics of
 * a VF's TX queue.
 */
typedef struct vmk_NetVFTXQueueStats {
   /** Unicast packets transmitted */
   vmk_uint64 unicastPkts;

   /** Unicast bytes transmitted */
   vmk_uint64 unicastBytes;

   /** Multicast packets transmitted */
   vmk_uint64 multicastPkts;

   /** Multicast bytes transmitted */
   vmk_uint64 multicastBytes;

   /** Broadcast packets transmitted */
   vmk_uint64 broadcastPkts;

   /** Broadcast bytes transmitted */
   vmk_uint64 broadcastBytes;

   /** Packets failed to transmit */
   vmk_uint64 errors;

   /** Packets discarded */
   vmk_uint64 discards;

   /** Number of TSO packets transmitted */
   vmk_uint64 TSOPkts;

   /** Number of bytes transmitted in TSO packets */
   vmk_uint64 TSOBytes;

   /** Reserved for future use */
   vmk_uint32 _rsvd[4];

} vmk_NetVFTXQueueStats;

/**
 * \ingroup Passthru
 * \brief Type for storing the status of a queue.
 *
 * This structure is query the status of a TX or RX queue.
 */
typedef struct vmk_NetVFQueueStatus {
   /** Is the queue stopped */
   vmk_Bool stopped;

   /** Vendor-specific error code */
   vmk_uint32 error;

   /** Reserved for future use */
   vmk_uint32 _rsvd[4];

} vmk_NetVFQueueStatus;

/**
 * \ingroup Passthru
 * \brief Type for the state saved for a TX queue (UPT).
 *
 * This structure is used by UPT to save the state of a TX queue.
 */
typedef struct vmk_UPTTXQueueSaveState {
   /** The index of the tx command descriptor the device will read next */
   vmk_uint32  txProd;

   /** The index of the tx command descriptor the driver will write to next */
   vmk_uint32  txCons;

   /** The index of the tx completion descriptor the device will write
       to next */
   vmk_uint32  tcProd;

   /** The value of the GEN bit the device will write to the next tx
       completion descriptor */
   vmk_uint32  tcGen;

   /** The stats maintained by a tx queue */
   vmk_NetVFTXQueueStats stats;

   /** Is the queue stopped */
   vmk_Bool stopped;

   /** Vendor-specific error code for stoppage */
   vmk_uint32 error;

   /** Reserved for future use */
   vmk_uint32 _rsvd[4];

} vmk_UPTTXQueueSaveState;

/**
 * \ingroup Passthru
 * \brief Type for the state saved for a RX queue (UPT).
 *
 * This structure is used by UPT to save the state of a TX queue.
 */
typedef struct vmk_UPTRXQueueSaveState {
   /** The index of the next rx command descriptor the driver will write to */
   vmk_uint32 rxProd;

   /** The index of the next rx command descriptor the device will
       receive pkts into */
   vmk_uint32 rxCons;

   /** The index of the next rx command descriptor in the 2nd ring the
       driver will write to */
   vmk_uint32 rxProd2;

   /** The index of the next rx command descriptor in the 2nd ring the
       device will receive pkts into */
   vmk_uint32 rxCons2;

   /** The index of the next rx completion descriptor the device will
       write to */
   vmk_uint32 rcProd;

   /** The value of the GEN bit the device will write to the next rx
       completion descriptor  */
   vmk_uint32 rcGen;

   /** The stats maintained by a rx queue */
   vmk_NetVFRXQueueStats stats;

   /** Is the queue stopped */
   vmk_Bool stopped;

   /** Vendor-specific error code for stoppage */
   vmk_uint32 error;

   /** Reserved for future use */
   vmk_uint32 _rsvd[4];

} vmk_UPTRXQueueSaveState;

/**
 * \ingroup Passthru
 * \brief Type for the state saved for the interrupt unit (UPT).
 *
 * This structure is used by UPT to save the state of a VF's interrupt
 * vector.
 */
typedef struct vmk_UPTIntrSaveState {
   /** The value of IMR for the i-th intr */
   vmk_uint32 imr;

   /** Reserved for future use */
   vmk_uint32 _rsvd[2];

} vmk_UPTIntrSaveState; 

/**
 * \ingroup Passthru
 * \brief Type for storing VF state (UPT).
 *
 * This structure is used by UPT to store the entire state of a VF,
 * including RX/TX queues state and interrupt state.
 */
typedef struct vmk_UPTVFSaveState {
   /** The number of tx queues */
   vmk_uint8        numTxQueues;

   /** The number of rx queues */
   vmk_uint8        numRxQueues;

   /** The number of intrs configured */
   vmk_uint32       numIntrs;

   /** The state of the tx queues */
   vmk_UPTTXQueueSaveState *tqState;

   /** The state of the rx queues */
   vmk_UPTRXQueueSaveState *rqState;

   /** The state of the intr unit */
   vmk_UPTIntrSaveState *intrState;

   /** Reserved for future use */
   vmk_uint32 _rsvd[4];

} vmk_UPTVFSaveState;

/**
 * \ingroup Passthru
 * \brief Arguments for the VF Acquire operation.
 *
 * This structure is passed as argument of the NETPTOP_VF_ACQUIRE
 * operation. It contains the required features on the VF being
 * allocated.
 */
typedef struct vmk_NetPTOPVFAcquireArgs {
   /** Requested options for the VF */
   vmk_NetVFRequirements props;

   /** (output) Identifier of the allocated VF */
   vmk_VFID vf;

} vmk_NetPTOPVFAcquireArgs;

/**
 * \ingroup Passthru
 * \brief Arguments for the VF Init operation.
 *
 * This structure is passed as argument of the NETPTOP_VF_INIT
 * operation. It contains the settings used to initialize the VF.
 */
typedef struct vmk_NetPTOPVFInitArgs {
   /** VF id */
   vmk_VFID vf;

   /** Parameters for the VF */
   vmk_NetVFParameters params;

} vmk_NetPTOPVFInitArgs;

/**
 * \ingroup Passthru
 * \brief Arguments for the VF Activate, Quiesce and Release operations.
 *
 * This structure is passed as an argument of the NETPTOP_VF_ACTIVATE,
 * NETPTOP_VF_QUIESCE and NETPTOP_VF_RELEASE operations.
 */
typedef struct vmk_NetPTOPVFSimpleArgs {
   /** VF id */
   vmk_VFID vf;

} vmk_NetPTOPVFSimpleArgs;

/**
 * \ingroup Passthru
 * \brief Arguments for VF Save State operation.
 *
 * This structure is passed as argument of UPT's NETPTOP_VF_SAVE_STATE
 * operation. The driver must fill the `state' structure with the
 * current state of the VF.
 */
typedef struct vmk_NetPTOPVFSaveStateArgs {
   /** VF id */
   vmk_VFID vf;

   /** (output) VF state */
   vmk_UPTVFSaveState state;

} vmk_NetPTOPVFSaveStateArgs;

/**
 * \ingroup Passthru
 * \brief Arguments for the VF Get Info operation.
 *
 * This structure is passed as argument of NETPTOP_VF_GET_INFO
 * operation and have to be filled (`info' structure) by the driver.
 */
typedef struct vmk_NetPTOPVFGetInfoArgs {
   /** VF id */
   vmk_VFID vf;

   /** (output) Information of the VF */
   vmk_NetVFInfo info;

} vmk_NetPTOPVFGetInfoArgs;

/**
 * \ingroup Passthru
 * \brief Arguments for the VF Set Indirection Table operation.
 *
 * This structure is passed as argument of the
 * NETPTOP_VF_SET_RSS_IND_TABLE operation and contains the updated RSS
 * indirection table for the VF.
 */
typedef struct vmk_NetPTOPVFSetRSSIndTableArgs {
   /** VF id */
   vmk_VFID vf;

   /** RSS Indirection table for the VF */
   vmk_NetVFRSSIndTable table;

} vmk_NetPTOPVFSetRSSIndTableArgs;

/**
 * \ingroup Passthru
 * \brief Arguments for the VF Get Queue Stats operation.
 *
 * This structure is passed as argument of the
 * NETPTOP_VF_GET_QUEUE_STATS operation. It has to be allocated by the
 * caller and `vf', `numTxQueues' and `numRxQueues' must be set
 * properly. The driver then put the statistics of each queue in the
 * `tqStats' and `rqStats' arrays.
 */
typedef struct vmk_NetPTOPVFGetQueueStatsArgs {
   /** VF id */
   vmk_VFID vf;

   /** Number of TX queues requested */
   vmk_uint8 numTxQueues;

   /** Number of RX queues requested */
   vmk_uint8 numRxQueues;

   /** (output) Statistics from the TX queues. Buffer is allocated and zeroed by the caller */
   vmk_NetVFTXQueueStats *tqStats;

   /** (output) Statistics from the RX queues. Buffer is allocated and zeroed by the caller */
   vmk_NetVFRXQueueStats *rqStats;
} vmk_NetPTOPVFGetQueueStatsArgs;

/**
 * \ingroup Passthru
 * \brief Arguments for the VF Get Queue Status operation.
 *
 * This structure is passed as argument of the
 * NETPTOP_VF_GET_QUEUE_STATUS to query the status of a VF's queue.
 * It has to be allocated by the caller and `vf', `numTxQueues' and
 * `numRxQueues' must be set properly. The driver then put the status
 * of each queue in the `tqStatus' and `rqStatus' arrays.
 */
typedef struct vmk_NetPTOPVFGetQueueStatusArgs {
   /** VF id */
   vmk_VFID vf;

   /** Number of TX queues requested */
   vmk_uint8 numTxQueues;

   /** Number of RX queues requested */
   vmk_uint8 numRxQueues;

   /** (output) Status for the TX queues. Buffer is allocated and zeroed by the caller */
   vmk_NetVFQueueStatus *tqStatus;

   /** (output) Status for the RX queues. Buffer is allocated and zeroed by the caller */
   vmk_NetVFQueueStatus *rqStatus;

} vmk_NetPTOPVFGetQueueStatusArgs;

/**
 * \ingroup Passthru
 * \brief Arguments for the VF Set Mac operation.
 *
 * This structure is passed to the NETPTOP_VF_SET_MAC operation and
 * configures or updates the MAC address of a VF.
 */
typedef struct vmk_NetPTOPVFSetMacArgs {
   /** VF id */
   vmk_VFID vf;

   /** MAC address */
   vmk_EthAddress mac;

} vmk_NetPTOPVFSetMacArgs;

/**
 * \ingroup Passthru
 * \brief Arguments for the VF Set MTU.
 *
 * This structure is passed to the NETPTOP_VF_SET_MTU operation and
 * configures or updates the MTU of a VF port.
 */
typedef struct vmk_NetPTOPVFSetMtuArgs {
   /** VF id */
   vmk_VFID vf;

   /** MTU for the VF */
   vmk_uint32 mtu;

} vmk_NetPTOPVFSetMtuArgs;

/**
 * \ingroup Passthru
 * \brief Arguments for the VF Set Multicast operation.
 *
 * This structure is passed to the NETPTOP_VF_SET_MULTICAST operation
 * and sets the list of multicast addresses to receive packets on.
 */
typedef struct vmk_NetPTOPVFSetMulticastArgs {
   /** VF id */
   vmk_VFID vf;

   /** Number of MAC filters contained in the `mac' array */
   vmk_ByteCount nmac;

   /** MAC addresses */
   vmk_EthAddress macList[VMK_NPA_MAX_MULTICAST_FILTERS];

   /** Reserved for future use */
   vmk_uint32 _rsvd[4];

} vmk_NetPTOPVFSetMulticastArgs;

/**
 * \ingroup Passthru
 * \brief Arguments for the VF Set RX Mode operation.
 *
 * This structure is passed as argument of the NETPTOP_VF_SET_RX_MODE
 * operation and sets or update the RX filtering options of a VF.
 */
typedef struct vmk_NetPTOPVFSetRxModeArgs {
   /** VF id */
   vmk_VFID vf;

   /** Enable unicast receive filter */
   vmk_Bool unicast;

   /** Enable multicast receive filter(s) */
   vmk_Bool multicast;

   /** Enable accepting broadcast */
   vmk_Bool broadcast;

   /** Enable multicast promiscuous mode */
   vmk_Bool allmulti;

   /** Enable promiscuous mode */
   vmk_Bool promiscuous;

} vmk_NetPTOPVFSetRxModeArgs;

/**
 * \ingroup Passthru
 * \brief Arguments for the Add Vlan Range and Del Vlan Range operations.
 *
 * This structure is used both for the NETPTOP_VF_ADD_VLAN_RANGE and
 * NETPTOP_VF_DEL_VLAN_RANGE to enable or disable forwarding of a
 * given range of VLANs to a VF.
 */
typedef struct vmk_NetPTOPVFVlanRangeArgs {
   /** VF id */
   vmk_VFID vf;

   /** starting vlan ID */
   vmk_VlanID first;

   /** ending vlan ID (included) */
   vmk_VlanID last;

} vmk_NetPTOPVFVlanRangeArgs;

/**
 * \ingroup Passthru
 * \brief Arguments for the VF Set Default Vlan operation.
 *
  This structure is passed as argument of the
  NETPTOP_VF_SET_DEFAULT_VLAN operation to enable or disable 802.1
  VLAN tag/priority insertion and stripping.
 */
typedef struct vmk_NetPTOPVFSetDefaultVlanArgs {
   /** VF id */
   vmk_VFID vf;

   /** Enable/disable */
   vmk_Bool enable;

   /** VLAN id to insert */
   vmk_VlanID vid;

   /** Priority value */
   vmk_VlanPriority prio;

} vmk_NetPTOPVFSetDefaultVlanArgs;

/**
 * \ingroup Passthru
 * \brief Arguments for the VF Set Antispoof operation.
 *
 * This structure is passed as argument of the
 * NETPTOP_VF_SET_ANTISPOOF operation to enable or disable source MAC
 * address enforcement in the 802.3 header.
 */
typedef struct vmk_NetPTOPVFSetAntispoofArgs {
   /** VF id */
   vmk_VFID vf;

   /** Enable MAC anti-spoofing */
   vmk_Bool enable;

} vmk_NetPTOPVFSetAntispoofArgs;


/**
 * \ingroup Passthru
 * \brief Arguments for VMK_NETPTOP_VF_SET_IML operation
 *
 * This simple structure is used to set the interrupt moderation for a VF.
 */

typedef struct vmk_NETPTOPVFSetIMLArgs {
   /** Number of vectors */
   vmk_uint8  numIntrs;

   /** (array of numIntrs) Interrupt moderation parameters for the vector. */
   vmk_VFIntrModParams *intrMod;

} vmk_NETPTOPVFSetIMLArgs;


/**
 * \ingroup Passthru
 * \brief Arguments for the PF Add Mac Filter and Del Mac Filter operations.
 *
 * This simple structure is used for both NETPTOP_PF_ADD_MAC_FILTER
 * and NETPTOP_PF_DEL_MAC_FILTER operations to insert or remove a MAC
 * address filter on the default queue.
 */
typedef struct vmk_NetPTOPPFMacFilterArgs {
   /** MAC address */
   vmk_EthAddress mac;

} vmk_NetPTOPPFMacFilterArgs;

/**
 * \ingroup Passthru
 * \brief Arguments for the PF Mirror All operation.
 *
 * This simple structure is passed to the NETPTOP_PF_MIRROR_ALL
 * operation and enables or disables the mirroring of all traffic to
 * the default queue.
 */
typedef struct vmk_NetPTOPPFMirrorAllArgs {
   /** Enable mirroring */
   vmk_Bool enable;

} vmk_NetPTOPPFMirrorAllArgs;


/**
 * \ingroup Passthru
 * \brief Enumeration of receive modes of a VF.
 */
typedef enum vmk_VFRXMode {
   /** Pass unicast (directed) frames */
   VMK_VF_RXMODE_UNICAST   =  (1 << 0),

   /** Pass some multicast frames */
   VMK_VF_RXMODE_MULTICAST =  (1 << 1),

   /** pass *all* multicast frames */
   VMK_VF_RXMODE_ALLMULTI  =  (1 << 2),

   /** Pass broadcast frames */
   VMK_VF_RXMODE_BROADCAST =  (1 << 3),

   /** Pass all frames (ie no filter) */
   VMK_VF_RXMODE_PROMISC   =  (1 << 4),

   /** Use the LADRF for multicast filtering */
   VMK_VF_RXMODE_USE_LADRF =  (1 << 5),

   /** pass not-matched unicast frames */
   VMK_VF_RXMODE_SINK      =  (1 << 6)

} vmk_VFRXMode;


/**
 * \ingroup Passthru
 * \brief VF Configuration information.
 *
 * This structure contains information about VF configuration. PF driver
 * passes this structure as an argument of vmk_NetPTConfigureVF()
 */
typedef struct vmk_NetVFCfgInfo {

   /** MAC Address for VF */
   vmk_uint8	   macAddr[6];

   union vmk_vlanInfo {

     /** Bitmap for Guest VLAN tags (VGT) */
      vmk_uint8    guestVlans[512];

     /** VLAN switch tagging (VST) */
      vmk_uint16   defaultVlan;
   } vlan;

   /** Config change being requested */
   vmk_uint32      cfgChanged;

   /** Receive mode (Unicast/Mcast/Bcast) */
   vmk_VFRXMode    rxMode;

   /** MTU for VF */
   vmk_uint16      mtu;
   vmk_uint8       reserved[7];
} vmk_NetVFCfgInfo;


/*
 ***********************************************************************
 * vmk_NetPTConfigureVF --                                        */ /**
 *
 * \brief Request to configure properties of a passthrough VF
 *
 * PF Driver calls this to request configuration of a passthrough VF. This
 * will mostly be called as a result of the guest requesting changes in
 * passthrough NIC configuration. VMKernel validates the VF configuration
 * request against the DVS or Portgroup properties. It then pushes
 * the configuration to VFs as applicable. VMKernel uses passthrough ops
 * (vmk_NetPTOP) published by PF driver to apply VF configuration.
 *
 * \see vmk_NetVFCfgInfo.
 * \see vmk_NetPTOP
 *
 * \note This in turn calls Passthrough OPs in PF driver
 *
 * \param[in]     cfgInfo  Requested VF Configuration.
 * \param[in]     vfSbdf   PCI Address of VF to be configured.
 *
 * \retval VMK_OK          The command was issued successfully and
 *                         the command's status is valid.
 * \retval VMK_FAILURE     The command failed.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_NetPTConfigureVF(vmk_NetVFCfgInfo *cfgInfo,
                                      vmk_PCIDeviceAddr vfSbdf);

#endif /* _VMKAPI_NET_PT_H_ */
/** @} */
/** @} */
