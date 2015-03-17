/***************************************************************************
 * Copyright 2004 - 2012 VMware, Inc.  All rights reserved.
 ***************************************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * MPP Types                                                      */ /**
 * \addtogroup MPP
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_MPP_TYPES_H_
#define _VMKAPI_MPP_TYPES_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "scsi/vmkapi_scsi_types.h"

/**
 * \brief Private data container for SCSI path
 */

typedef struct vmk_ScsiPath {
   /** \brief claiming plugin's priv data */
   void *pluginPrivateData;
} vmk_ScsiPath;

/**
 * \brief SCSI path attributes
 */
typedef enum vmk_ScsiPathBoolAttribute {
   /**
    * \brief Unknown/invalid attribute.
    */
   VMK_SCSI_PATH_BOOL_ATTR_UNKNOWN     = 0x0,
   /**
    * \brief Path leads to a VVol Protocol Endpoint (PE).
    *
    * Only paths with the PE attribute set can be legitimately claimed
    * for a PE device. A PE device in this context refers to a SCSI device
    * with which one or more PE paths have been associated.
    *
    * A plugin should never mix PE and non-PE paths for any SCSI device.
    */
   VMK_SCSI_PATH_BOOL_ATTR_PE          = 0x1,
   VMK_SCSI_PATH_BOOL_ATTR_MAX         = 0x2 /* Add all boolean attributes before this */
} vmk_ScsiPathBoolAttribute;

/*
 * \brief SCSI plugin type.
 */
typedef enum {
   VMK_SCSI_PLUGIN_TYPE_INVALID,
   VMK_SCSI_PLUGIN_TYPE_MULTIPATHING,
   VMK_SCSI_PLUGIN_TYPE_FILTER,       /* Requires incompatible APIs */
   VMK_SCSI_PLUGIN_TYPE_VAAI,
#define VMK_SCSI_MAX_PLUGIN_TYPE (VMK_SCSI_PLUGIN_TYPE_VAAI)
} vmk_ScsiPluginType;

/*
 * \brief SCSI plugin priority as expressed by rank
 */
typedef enum {
   VMK_SCSI_PLUGIN_PRIORITY_UNKNOWN       = 0,
#define VMK_SCSI_PLUGIN_PRIORITY_SIMPLE (VMK_SCSI_PLUGIN_PRIORITY_HIGHEST)
   VMK_SCSI_PLUGIN_PRIORITY_HIGHEST       = 1,
   VMK_SCSI_PLUGIN_PRIORITY_VIRUSSCAN     = 0x20000000,
   VMK_SCSI_PLUGIN_PRIORITY_VAAI          = 0x40000000,
   VMK_SCSI_PLUGIN_PRIORITY_DEDUPLICATION = 0x60000000,
   VMK_SCSI_PLUGIN_PRIORITY_COMPRESSION   = 0x80000000,
   VMK_SCSI_PLUGIN_PRIORITY_ENCRYPTION    = 0xa0000000,
   VMK_SCSI_PLUGIN_PRIORITY_REPLICATION   = 0xc0000000,
   VMK_SCSI_PLUGIN_PRIORITY_LOWEST        = 0xffffffff
} vmk_ScsiPluginPriority;

/** \cond nodoc */
typedef struct vmk_ScsiDevice vmk_ScsiDevice;
/** \endcond */

typedef enum vmk_ScsiPluginStatelogFlag {
   VMK_SCSI_PLUGIN_STATELOG_GLOBALSTATE = 0x00000001,
   VMK_SCSI_PLUGIN_STATE_LOG_CRASHDUMP  = 0x00000002
} vmk_ScsiPluginStatelogFlag;

/**
 * \brief SCSI device attributes
 */
typedef enum vmk_ScsiDeviceBoolAttribute {
   /**
    * \brief Unknown/invalid attribute.
    */
   VMK_SCSI_DEVICE_BOOL_ATTR_UNKNOWN = 0x0,
   /**
    * \brief Device is a pseudo/management device.
    */
   VMK_SCSI_DEVICE_BOOL_ATTR_PSEUDO  = 0x1,
   /**
    * \brief Device is an SSD device.
    *
    * Plugin can use the getBoolAttr entrypoint to indicate devices
    * that match the characteristics of a Solid State Device (SSD).
    * Typical SSD devices are flash-based and have fast random
    * access due to zero-seek latency.
    *
    * Any device identified as an SSD device by this attribute will be
    * marked as an SSD device by the PSA layer. This information might be
    * used by users of this device to leverage the performance
    * characteristics of SSD devices.
    *
    */
   VMK_SCSI_DEVICE_BOOL_ATTR_SSD     = 0x2,
   /**
    * \brief Device is a local (non-shared) device.
    *
    * Plugin can use the getBoolAttr entrypoint along with this attribute
    * to indicate devices that are local (non-shared) to the host.
    *
    * Any device identified as local device by getBoolAttr entrypoint will be
    * marked as a local device by the PSA layer. This information might be
    * used by users of this device to do host specific performance
    * optimizations (like swap). Also, clustered file systems (like VMFS)
    * can potentially enable locking/performance optimizations on local devices.
    */
   VMK_SCSI_DEVICE_BOOL_ATTR_LOCAL   = 0x3,
   VMK_SCSI_DEVICE_BOOL_ATTR_MAX     = 0x4 /* Add all boolean attributes before this */
} vmk_ScsiDeviceBoolAttribute;

#define VMK_SCSI_UID_MAX_ID_LEN       256

/**
 * \brief data for ioctl on SCSI plugin
 */

/**
 * \brief Ioctl data argument encapsulating every optional argument
 *        that can be passed to vmk_ScsiPlugin->pluginIoctl()
 */
typedef vmk_uint32 vmk_ScsiPluginIoctl;

typedef union vmk_ScsiPluginIoctlData {
   /** \brief plugin type specific union */
   union {
      /** \brief reserved */
      vmk_uint8 reserved[128];
   } u;
} vmk_ScsiPluginIoctlData;

/**
 ***********************************************************************
 *                                                                */ /**
 * \struct vmk_ScsiMPPluginSpecific
 * \brief vmk_ScsiPlugin specific data & operations for plugin type
 *        VMK_SCSI_PLUGIN_TYPE_MULTIPATHING
 *
 ***********************************************************************
 */
struct vmk_ScsiPlugin;

typedef struct vmk_ScsiMPPluginSpecific {
   /**
    * \brief Tell the plugin we're about to start offering it paths
    *
    *   A series of pathClaim calls is always preceded by a
    *   pathClaimBegin call.
    */
   VMK_ReturnStatus (*pathClaimBegin)(struct vmk_ScsiPlugin *plugin);
   /**
    * \brief Offer a path to the plugin for claiming
    *
    *   Give the plugin a chance to claim a path. The plugin is the
    *   owner of the path for the duration of the call; to retain
    *   ownership of the path beyond that call, the plugin must return
    *   VMK_OK and set claimed to VMK_TRUE.
    *
    *   If an error occurs that prevents the plugin from claiming the
    *   path (e.g. out of memory, IO error, ...) it should return a non
    *   VMK_OK status to let SCSI know that a problem occured and
    *   that it should skip that path.
    *   The plugin can issue IOs on the path, change its state, sleep,
    *   ...
    *   If it decides not to claim the path, the plugin must drain all
    *   the IOs it has issued to the path before returning.
    *
    *   A series of pathClaim calls is always preceded by a
    *   pathClaimBegin call and followed by a pathClaimEnd call.
    */
   VMK_ReturnStatus (*pathClaim)(vmk_ScsiPath *path,
		                 vmk_Bool *claimed);
   /**
    * \brief Ask the plugin to unclaim a path
    *
    *   Tell the plugin to unclaim the specified path.
    *   The plugin can perform blocking tasks before returning. The plugin must
    *   drain all outstanding IOs on the path that came to it via the
    *   \em pathIssueCmd entry point before returning.
    *   If the plugin successfully unclaims the path, then this path
    *   is moved to the "DEAD" state.
    */
   VMK_ReturnStatus (*pathUnclaim)(vmk_ScsiPath *path);
   /**
    * \brief Tell the plugin we're done offering it paths
    *
    *   A series of pathClaim calls is always terminated by a
    *   pathClaimEnd call.  The PSA framework serializes this call and will
    *   not call into this entrypoint from more than a single world
    *   simultaneously. Paths can be in a dead state when they are first 
    *   claimed, so a probe OR other path state update is needed during this
    *   call. 
    */
   VMK_ReturnStatus (*pathClaimEnd)(struct vmk_ScsiPlugin *plugin);
   /**
    * \brief Probe the path; update its current state
    *
    *   Issue a probe IO and update the current state of the path.
    *   The function blocks until the state of the path has been updated
    */
   VMK_ReturnStatus (*pathProbe)(vmk_ScsiPath *path);
   /**
    * \brief Set a path's state (on or off only)
    *
    *   Turn the path on or off per administrator action, or else mark the
    *   underlying adapter driver is informing the plugin that a path is dead.
    *   An MP Plugin should notify the PSA framework if a path state has changed
    *   via vmk_ScsiSetPathState() after it has performed any necessary internal
    *   bookkeeping.
    */
   VMK_ReturnStatus (*pathSetState)(vmk_ScsiPath *path,
                                    vmk_ScsiPathState state);
   /** 
    * \brief Get device name associated with a path 
    *
    *   On successful completion of this routine the MP plugin is expected to 
    *   return the device UID in the deviceName. The device UID string must
    *   be NULL terminated.
    *   If the path is not claimed by the plugin then an error status of
    *   of VMK_FAILURE has to be returned by the plugin.
    *   If the device corresponding to the path has not been registered with
    *   the PSA framework then an error status of VMK_NOT_FOUND has to be 
    *   returned by the plugin.
    *   PSA will return these errors to the callers of this routine.
    */ 
   VMK_ReturnStatus (*pathGetDeviceName)(vmk_ScsiPath *path,
                                      char deviceName[VMK_SCSI_UID_MAX_ID_LEN]); 
   /**
    * \brief Request MPP to issue an IO on a path
    * 
    * All I/O requests to a path claimed by MPP through the sync and the 
    * async APIs barring vmk_ScsiIssueAsyncPathCommandDirect() are
    * sent to the MPP layer through this entry point. The entry point 
    * may issue the I/O on the path using the 
    * vmk_ScsiIssueAsyncPathCommandDirect API.
    *
    * If the I/O cannot be issued then a non-VMK_OK status is returned.
    * If a completion frame has been pushed then the plugin will complete
    * the command with the appropriate error status.
    *
    * \note If this is omitted the caller will issue a command 
    * on the path bypassing the MPP. As a result MPP will never see
    * completion for these commands. Also, MPP will lose the benefit of 
    * seeing the intermediate completion status of command sent via the SYNC
    * interfaces.
    */
   VMK_ReturnStatus (*pathIssueCmd) (vmk_ScsiPath *path, vmk_ScsiCommand *vmkCmd);
} vmk_ScsiMPPluginSpecific;

/*
 * \brief VAAI-specific command completion status
 */
typedef enum {
   VMK_SCSI_VAAI_CMD_STATUS_OK,
   VMK_SCSI_VAAI_CMD_STATUS_QUEUE_FULL,
} vmk_ScsiVAAIPCmdStatus;

typedef struct vmk_ScsiVAAIPluginSpecific {
   /**
    * \brief Offer a device to the plugin for claiming
    *
    *   Give the plugin a chance to claim a device. The plugin is the
    *   owner of the device for the duration of the call; To retain
    *   ownership of the device beyond that call, the plugin must return
    *   VMK_OK.
    *
    *   If it decides not to claim the device, the plugin must drain all
    *   the IOs it has issued to the device and return a failure code.
    */
   VMK_ReturnStatus (*claim)(vmk_ScsiDevice *device);
   /**
    * \brief Tell the plugin to unclaim a device
    *
    *   Tell the plugin to try and unclaim the specified device.
    *   The plugin can perform blocking tasks before returning.
    */
   void (*unclaim)(vmk_ScsiDevice *device);
   /**
    * \brief Initialize vendor cloneblocks command
    *
    *        Initialize vmkCmd with a vendor specific cdb necessary for
    *        initiating a clone of lbc blocks from srcDev beginning
    *        at srcLba to dstDev beginning at dstLba.
    *        This function just needs to initialize vmkCmd, the caller
    *        is responsible for issuing the command to the device.
    *        The command populated by this call will be issued to
    *        either prefIssueDev, or the value of preferCloneSrc will
    *        be honored.
    *
    * \param[in] srcDev The source device to clone from
    * \param[in] dstDev The destination device to clone to
    * \param[in] srcLba The source lba to start cloning from
    * \param[in] dstLba The destination lba to start cloning from
    * \param[in] lbc The number of blocks to clone
    * \param[out] prefIssueDev The preferred device to issue vmkCmd to
    * \param[out] vmkCmd The command to populate
    *
    * \note This is a non-blocking call.
    * \note Function may populate vmkCmd->done with a completion handler
    *       to get notification of i/o completion.  The caller guarantees
    *       that vmkCmd->done will be NULL prior to this call.
    * \note The framework at its discretion may or may not honor prefIssueDev.
    *       In the event it does not, it will honor preferCloneSrc below
    *       instead.
    */
   VMK_ReturnStatus (*cloneBlocks)(vmk_ScsiDevice *srcDev,
                                   vmk_ScsiDevice *dstDev,
                                   vmk_uint64 srcLba,
                                   vmk_uint64 dstLba,
                                   vmk_uint32 lbc,
                                   vmk_ScsiDevice **prefIssueDev,
                                   vmk_ScsiCommand *vmkCmd);
   /**
    * \brief Initialize zeroblocks command
    *
    *        Initialize vmkCmd with a vendor specific cdb necessary for
    *        initiating a zero of lbc blocks to device beginning
    *        at lba.
    *        This function just needs to initialize vmkCmd, the caller
    *        is responsible for issuing the command to the device.
    *
    * \param[in] device The device to zero blocks from
    * \param[in] lba The lba to start zeroing from
    * \param[in] lbc The number of blocks to zero
    * \param[out] vmkCmd The command to populate
    *
    * \note This is a non-blocking call.
    * \note Function may populate vmkCmd->done with a completion handler
    *       to get notification of i/o completion.  The caller guarantees
    *       that vmkCmd->done will be NULL prior to this call.
    */
   VMK_ReturnStatus (*zeroBlocks)(vmk_ScsiDevice *device,
                                  vmk_uint64 lba,
                                  vmk_uint32 lbc,
                                  vmk_ScsiCommand *vmkCmd);
   /**
    * \brief Initialize vendor ats command
    *
    *        Initialize vmkCmd with a vendor specific cdb necessary for
    *        initiating an atomic-test-and-set of testData and setData
    *        at lba on device dev.
    *        This function just needs to initialize vmkCmd, the caller
    *        is responsible for issuing the command to the device.
    *
    * \param[in] device The device to whice ats will be issued
    * \param[in] lba The lba to ats
    * \param[in] testData The data to test against
    * \param[in] setData The data to conditionally write
    * \param[out] vmkCmd The command to populate
    *
    * \note This is a non-blocking call.
    * \note Function may populate vmkCmd->done with a completion handler
    *       to get notification of i/o completion.  The caller guarantees
    *       that vmkCmd->done will be NULL prior to this call.
    */
   VMK_ReturnStatus (*ats)(vmk_ScsiDevice *dev,
                           vmk_uint64 lba,
                           const void *testData,
                           const void *setData,
                           vmk_ScsiCommand *vmkCmd);
   /**
    * \brief Get VAAIP command status
    *
    *        Determine the VAAIP error status of completed command
    *        vmkCmd.   This function should determine based on the
    *        scsi error status and sense data whether the error
    *        indicates a VAAI specific error code.  If not the function
    *        should return VMK_SCSI_VAAI_CMD_STATUS_OK.
    *
    * \param[in] vmkCmd The command to populate
    */
   vmk_ScsiVAAIPCmdStatus (*getVAAIPCmdStatus)(const vmk_ScsiCommand *vmkCmd);

   /**
    * \brief cloneLbaAlignment
    *
    * The VAAI plugin should set this to 0 if there are no restrictions on
    * a clone operation's start lba alignment.  The VAAI plugin should set
    * this to a power of 2 if there are restrictions on a clone operation's
    * start lba alignment.  Value is expressed in number of blocks.
    */
   vmk_uint64 cloneLbaAlignment;

   /**
    * \brief cloneLbcAlignment
    *
    * The VAAI plugin should set this to 0 if there are no restrictions on
    * a clone operation's lbc alignment.  The VAAI plugin should set
    * this to a power of 2 if there are restrictions on a clone operation's
    * lbc alignment.  Value is expressed in number of blocks.
    */
   vmk_uint64 cloneLbcAlignment;

   /**
    * \brief atsLbaAlignment
    *
    * The VAAI plugin should set this to 0 if there are no restrictions on
    * an ats operation's start lba alignment.  The VAAI plugin should set
    * this to a power of 2 if there are restrictions on a ats operation's
    * start lba alignment.  Value is expressed in number of blocks.
    */
   vmk_uint64 atsLbaAlignment;

   /**
    * \brief atsLbcAlignment
    *
    * The VAAI plugin should set this to 0 if there are no restrictions on
    * a ats operation's lbc alignment.  The VAAI plugin should set
    * this to a power of 2 if there are restrictions on a ats operation's
    * lbc alignment.  Value is expressed in number of blocks.
    */
   vmk_uint64 atsLbcAlignment;

   /**
    * \brief deleteLbaAlignment
    *
    * The VAAI plugin should set this to 0 if there are no restrictions on
    * a delete operation's start lba alignment.  The VAAI plugin should set
    * this to a power of 2 if there are restrictions on a delete operation's
    * start lba alignment.  Value is expressed in number of blocks.
    */
   vmk_uint64 deleteLbaAlignment;

   /**
    * \brief deleteLbcAlignment
    *
    * The VAAI plugin should set this to 0 if there are no restrictions on
    * a delete operation's lbc alignment.  The VAAI plugin should set
    * this to a power of 2 if there are restrictions on a delete operation's
    * lbc alignment.  Value is expressed in number of blocks.
    */
   vmk_uint64 deleteLbcAlignment;

   /**
    * \brief zeroLbaAlignment
    *
    * The VAAI plugin should set this to 0 if there are no restrictions on
    * a zero operation's start lba alignment.  The VAAI plugin should set
    * this to a power of 2 if there are restrictions on a zero operation's
    * start lba alignment.  Value is expressed in number of blocks.
    */
   vmk_uint64 zeroLbaAlignment;

   /**
    * \brief zeroLbcAlignment
    *
    * The VAAI plugin should set this to 0 if there are no restrictions on
    * a zero operation's lbc alignment.  The VAAI plugin should set
    * this to a power of 2 if there are restrictions on a zero operation's
    * lbc alignment.  Value is expressed in number of blocks.
    */
   vmk_uint64 zeroLbcAlignment;

   /**
    * \brief preferCloneSrc
    *
    * VAAI plugin should set this to TRUE at initialization time to indicate to
    * the framework that it prefers cloneBlocks() operation to be sent to
    * the source device rather than the destination device.  The framework
    * at its discretion may or may not honor this value.  In the event it
    * does not, it will honor the prefIssueDev out param in the cloneBlocks
    * function call.
    */
   vmk_Bool   preferCloneSrc;
} vmk_ScsiVAAIPluginSpecific;

#define VMK_SCSI_PLUGIN_NAME_MAX_LEN    40

/**
 * \brief SCSI plugin
 */
typedef struct vmk_ScsiPlugin {
   /** \brief Revision of the SCSI API implemented by the plugin */
   vmk_revnum  scsiRevision;
   /**
    * \brief Revision of the plugin.
    *
    * This field is used for support and debugging purposes only.
    */
   vmk_revnum  pluginRevision;
   /**
    * \brief Revision of the product implemented by this plugin.
    *
    * This field is used for support and debugging purposes only.
    */
   vmk_revnum  productRevision;
   /** \brief moduleID of the VMkernel module hosting the plugin. */
   vmk_ModuleID moduleID;
   /** \brief lock domain of the VMkernel module hosting the plugin. */
   vmk_LockDomainID lockDomain;
   /**
    * \brief Human readable name of the plugin.
    *
    * This field is used for logging and debugging only.
    */
   char        name[VMK_SCSI_PLUGIN_NAME_MAX_LEN];
   /** 
    * \brief plugin type specific union
    */
   union {
      /** \brief multipathing plugin specific data & entrypoints */
      /* type == VMK_SCSI_PLUGIN_TYPE_MULTIPATHING */
      vmk_ScsiMPPluginSpecific mp;
      /* type == VMK_SCSI_PLUGIN_TYPE_VAAI */
      vmk_ScsiVAAIPluginSpecific vaai;
      /** \brief reserved */
      unsigned char            reserved[512];
   } u;

   /**
    * \brief Issue a management ioctl on the specified plugin
    *
    * - Not currently used
    */
   VMK_ReturnStatus (*pluginIoctl)(struct vmk_ScsiPlugin *plugin,
                                   vmk_ScsiPluginIoctl cmd,
                                   vmk_ScsiPluginIoctlData *data);
   /**
    * \brief Prompt a plugin to log its internal state
    *
    * Log a plugin's internal state
    */
   VMK_ReturnStatus (*logState)(struct vmk_ScsiPlugin *plugin,
                                const vmk_uint8 *logParam,
                                vmk_ScsiPluginStatelogFlag logFlags);
   /** \brief reserved */
   vmk_VA reserved1[4];
   /** \brief reserved */
   vmk_uint32   reserved2[3];
   /** \brief Type of the plugin
    *
    * Currently only MULTIPATHING, FILTER, and VAAI are supported.
    * This field is used to determine what capabilities the plugin
    * is allowed to implement.
    */
   vmk_ScsiPluginType type;
} vmk_ScsiPlugin;

/*
 * SCSI Device Identifiers
 */
#define VMK_SCSI_UID_FLAG_PRIMARY          0x00000001
/** \brief This represents the legacy UID (starting with vml.xxx..) */
#define VMK_SCSI_UID_FLAG_LEGACY           0x00000002
/** \brief The uid is globally unique. */
#define VMK_SCSI_UID_FLAG_UNIQUE           0x00000004
/** \brief The uid does not change across reboots. */
#define VMK_SCSI_UID_FLAG_PERSISTENT       0x00000008

#define VMK_SCSI_UID_FLAG_DEVICE_MASK  \
   (VMK_SCSI_UID_FLAG_PRIMARY      |   \
    VMK_SCSI_UID_FLAG_LEGACY       |   \
    VMK_SCSI_UID_FLAG_UNIQUE       |   \
    VMK_SCSI_UID_FLAG_PERSISTENT)

/**
 * \brief SCSI identifier
 */
typedef struct vmk_ScsiUid {
   /** \brief null terminated printable identifier */
   char 	        id[VMK_SCSI_UID_MAX_ID_LEN];
   /** \brief id attributes, see uid_flags above */
   vmk_uint32 	        idFlags;
   /** \brief reserved */
   vmk_uint8            reserved[4];
} vmk_ScsiUid;

#define VMK_SCSI_UID_FLAG_PATH_MASK    \
   (VMK_SCSI_UID_FLAG_UNIQUE       |   \
    VMK_SCSI_UID_FLAG_PERSISTENT)

/**
 * \brief SCSI path identifier.
 *
 * Path identifier constructed from transport specific data
 */
typedef struct vmk_ScsiPathUid {
   /** \brief flags indicating uniqueness and persistence */
   vmk_uint64 		idFlags;
   /** \brief adapter id */
   char 		adapterId[VMK_SCSI_UID_MAX_ID_LEN];
   /** \brief target id */
   char 		targetId[VMK_SCSI_UID_MAX_ID_LEN];
   /** \brief device id (matches vmk_ScsiDevice's primary vmk_ScsiUid) */
   char 		deviceId[VMK_SCSI_UID_MAX_ID_LEN];
} vmk_ScsiPathUid;

typedef enum vmk_ScsiDeviceEvent {
   /** \brief Device has been turned off administratively. */
   VMK_SCSI_DEVICE_EVENT_OFF = 0x0,
   /** \brief Device has been turned on administratively. */
   VMK_SCSI_DEVICE_EVENT_ON = 0x1
}vmk_ScsiDeviceEvent;


/**
 * \brief Operations provided by a device managed by 
 *        VMK_SCSI_PLUGIN_TYPE_MULTIPATHING 
 */
typedef struct vmk_ScsiMPPluginDeviceOps { 
   /** \brief Get a list of path names associated with a device */
   VMK_ReturnStatus (*getPathNames)(vmk_ScsiDevice *device,
                                    vmk_HeapID *heapID,
                                    vmk_uint32 *numPathNames,
                                    char ***pathNames);
   /*
    ***********************************************************************
    * notifyDeviceEvent --                                           */ /**
    *
    * \brief Notify the MPP about a device being set off/on administratively.
    *
    *  This entry point can be called post device registration at any time.
    *  This entry point can also be called in the vmk_ScsiRegisterDevice() 
    *  context if the device had been marked "offline" persistently.
    *
    *  The startCommand, open, close and taskMgmt entry points should not be 
    *  called by PSA after this notification.  However other MPP device and 
    *  path entry points can be  called. 
    *
    * \note  This is a blocking interface with the exception stated below.
    * As the entry point can be called in the registration context, to avoid 
    * a deadlock, the notifyDeviceEvent entry point MUST not block for 
    * registration completion. 
    *
    * \note  This entry point is optional. 
    *        If its not supported, then the value should be initialized to NULL.  
    *        If supported then this call must complete successfully with a 
    *        return of VMK_OK.
    *
    * \param[in]  device   Device being targeted.
    * \param[in]  event    Event that's occurring on the device.
    *
    * \retval VMK_OK       Success
    *
    ***********************************************************************
    */
   VMK_ReturnStatus (*notifyDeviceEvent)(vmk_ScsiDevice *device,
                                         vmk_ScsiDeviceEvent event);

} vmk_ScsiMPPluginDeviceOps; 

/**
 * \brief Operations provided by a device to the storage stack
 */
typedef struct vmk_ScsiDeviceOps {
   /**
    * \brief Notify the plugin that a command is available on the specified
    *        device.
    *
    * The plugin should fetch the command at the earliest opportunity
    * using vmk_ScsiGetNextDeviceCommand().
    *
    * SCSI invokes startCommand() only when queueing an IO in an empty
    * queue. SCSI does not invoke startCommand() again until
    * vmk_ScsiGetNextDeviceCommand() has returned NULL once. 
    *  
    * \note IO CPU accounting: 
    * MP plugin has to implement CPU accounting for processing IOs 
    * on behalf of each world via vmk_ServiceTimeChargeBeginWorld / 
    * vmk_ServiceTimeChargeEndWorld API in the issuing and task
    * management paths. CPU accounting for the completion path is 
    * done by the PSA path layer. Path probing and other auxillary 
    * tasks are not executed on behalf of any particular world and, 
    * therefore, do not require CPU accounting. 
    *
    * \note This entry point is mandatory.
    *
    */
   VMK_ReturnStatus (*startCommand)(vmk_ScsiDevice *device);
   /**
    * \brief Issue a task management request on the specified device.
    * 
    * The plugin should:
    *
    * - invoke vmk_ScsiQueryTaskMgmtAction() for all the device's IOs
    *   currently queued inside the plugin or until
    *   vmk_ScsiGetNextDeviceCommand() returns
    *   VMK_SCSI_TASKMGMT_ACTION_BREAK or
    *   VMK_SCSI_TASKMGMT_ACTION_ABORT_AND_BREAK
    * - complete all IOs returning VMK_SCSI_TASKMGMT_ACTION_ABORT or
    *   VMK_SCSI_TASKMGMT_ACTION_ABORT_AND_BREAK with the status
    *   specified in the task management request
    *
    *   If vmk_ScsiQueryTaskMgmtAction() did not return with
    *   VMK_SCSI_TASKMGMT_ACTION_BREAK or
    *   VMK_SCSI_TASKMGMT_ACTION_ABORT_AND_BREAK:
    *
    * - forward the request to all paths with at least one IO in
    *   progress for that device, and
    * - one (live) path to the device
    *
    * \note This entry point is mandatory.
    *
    */
   VMK_ReturnStatus (*taskMgmt)(vmk_ScsiDevice *device,
                                vmk_ScsiTaskMgmt *taskMgmt);
   /**
    * \brief Open a device 
    * The plugin should fail the open if the device state is set 
    * to VMK_SCSI_DEVICE_STATE_OFF
    *
    * \note This entry point is mandatory.
    *
    */
   VMK_ReturnStatus (*open)(vmk_ScsiDevice *device);
   /**
    * \brief Close a device
    *
    * \note This entry point is mandatory.
    *
    */
   VMK_ReturnStatus (*close)(vmk_ScsiDevice *device);
   /**
    * \brief Probe a device
    *
    * \note This entry point is mandatory.
    */
   VMK_ReturnStatus (*probe)(vmk_ScsiDevice *device);
   /**
    * \brief Get the inquiry data from the specified device
    * 
    * \note This entry point is mandatory.
    *
    */
   VMK_ReturnStatus (*getInquiry)(vmk_ScsiDevice *device,
                                  vmk_ScsiInqType inqPage,
                                  vmk_uint8 *inquiryData,
                                  vmk_ByteCountSmall inquirySize);
   /**
    * \brief Issue a sync cmd to write out a core dump
    * 
    * \note This entry point is mandatory.
    *
    */
   VMK_ReturnStatus (*issueDumpCmd)(vmk_ScsiDevice *device,
                                    vmk_ScsiCommand *scsiCmd);
   /** \brief plugin type specific union */
   union {
      /** \brief multipathing plugin specific device ops */
      vmk_ScsiMPPluginDeviceOps mpDeviceOps;
      /** \brief reserved */
      unsigned char            reserved[128];
   } u;
   /*
    ***********************************************************************
    * getBoolAttr --                                                 */ /**
    *
    * \brief Determine the device's attribute.
    *
    * Plugin can use this entrypoint to indicate devices that match the
    * characteristics of the attribute.
    *
    * If this is omitted, vmk_ScsiDefaultDeviceGetBoolAttr() will be invoked
    * to auto-detect local/SSD devices.
    *
    * \see vmk_ScsiDefaultDeviceGetBoolAttr()
    *
    * \note This entry point is mandatory.
    *
    * \param[in]  device   Device to be probed.
    * \param[in]  attr     vmk_ScsiDeviceBoolAttribute to probe for.
    * \param[out] boolAttr Flag indicating if device supports the attribute.
    *
    * \return VMK_ReturnStatus
    * \retval VMK_OK         If the device attribute is set.
    * \retval VMK_BAD_PARAM  Invalid input device or attribute.
    *
    ***********************************************************************
    */
   VMK_ReturnStatus (*getBoolAttr)(vmk_ScsiDevice *device,
                                   vmk_ScsiDeviceBoolAttribute attr,
                                   vmk_Bool *boolAttr);
   /** \note reserved for future extension. */
   void (*reserved[2])(void);
} vmk_ScsiDeviceOps;


/**
 * \brief SCSI device protection information (PI) data.
 *
 * SCSI device protection information set by the multipathing plugin.
 */
typedef struct vmk_ScsiDevicePIData {
   /** \brief  Protecting enabled flag from READ_CAPACITY(16) response. */
   vmk_Bool                enabled;
   /** \brief  T10 protection type from READ_CAPACITY(16) response. */
   vmk_ScsiTargetProtTypes protType;
   /** \brief  Protection guard type from underlying adapters. */
   vmk_ScsiGuardTypes      guardType;
   /** \brief  Supported protection types mask from underlying adapters. */
   vmk_ScsiProtTypes       protMask;
} vmk_ScsiDevicePIData;


/** 
 * \brief SCSI device structure
 */
struct vmk_ScsiDevice {
   /** \brief producing plugin's priv data */
   void                 *pluginPrivateData;
   /** \brief device ops the PSA framework should use for this device */
   vmk_ScsiDeviceOps    *ops;
   /** \brief producing plugin's module id */
   vmk_ModuleID         moduleID;
   /** \brief reserved */
   vmk_uint8            reserved[4];
};

#endif /* _VMKAPI_MPP_TYPES_H_ */
/** @} */
