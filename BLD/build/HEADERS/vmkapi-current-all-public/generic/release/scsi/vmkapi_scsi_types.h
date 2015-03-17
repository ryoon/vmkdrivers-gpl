/***************************************************************************
 * Copyright 2004 - 2012 VMware, Inc.  All rights reserved.
 ***************************************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * SCSI-specific types                                            */ /**
 * \addtogroup SCSI
 * @{
 *
 * \defgroup SCSItypes SCSI Types
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_SCSI_TYPES_H_
#define _VMKAPI_SCSI_TYPES_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "core/vmkapi_dma.h"
#include "scsi/vmkapi_scsi_ext.h"
#include "scsi/vmkapi_scsi_mgmt_types.h"

#define VMK_SECTOR_SIZE 512

/**
 * \brief SCSI second-level lun id.
 */
typedef vmk_uint64 vmk_ScsiSecondLevelId;

/**
 * \brief Invalid SCSI second-level lun id.
 */
#define VMK_SCSI_INVALID_SECONDLEVEL_ID   ((vmk_ScsiSecondLevelId)-1)

/**
 * \brief configured SCSI system limits
 */
typedef struct vmk_ScsiSystemLimits {
   vmk_uint32     maxDevices;
   vmk_uint32     maxPaths;
   vmk_VA   pad[3];
} vmk_ScsiSystemLimits;

/**
 * \brief Fixed format sense data structure maintained in each SCSI device
 * SPC 3 r23, Section 4.5.3 table 26
 *
 * NB: The "valid" bit in the data structure does NOT tell whether the sense is
 * actually valid and thus the name is really badly chosen (even though it is
 * the official name from the SCSI II specification).  The SCSI II spec.
 * states "A valid bit of zero indicates that the information field is not as
 * defined in this International Standard".  we have seen that many tape drives
 * are capable of returning sense without this bit set
 */
typedef struct vmk_ScsiSenseData {
   /** \brief error type and format of sense data (see SPC 3 r23, Sec. 4.5.1) */
   vmk_uint8 error         :7,
/** \brief sense data is for "current command" */
#define VMK_SCSI_SENSE_ERROR_CURCMD  0x70
/** \brief sense data is for an earlier command */
#define VMK_SCSI_SENSE_ERROR_PREVCMD 0x71
   /** \brief set to one indicates the 'info' field is valid */
             valid         :1;
   /** \brief obsolete */
   vmk_uint8 segment;
   /** \brief generic information describing the error or exception condition */
   vmk_uint8 key           :4,
                           :1,
   /** \brief see above spc section */
             ili           :1,
   /** \brief see above spc section */
             eom           :1,
   /** \brief see above spc section */
             filmrk        :1;
   /** \brief see above spc section */
   vmk_uint8 info[4];
   /** \brief see above spc section */
   vmk_uint8 optLen;
   /** \brief see above spc section */
   vmk_uint8 cmdInfo[4];
   /** \brief further information about to the condition reported in 'key' */
   vmk_uint8 asc;
   /** \brief detailed information about to the additional sense code in 'asc' */
   vmk_uint8 ascq;
   /** \brief Field Replacable Unit code (see SPC 3 r23, 4.5.2.5) */
   vmk_uint8 fru;
   /** \brief Sense-key specific fields (see SPC 3 r23, 4.5.2.4) */
   vmk_uint8 bitpos        :3,
             bpv           :1,
                           :2,
             cd            :1,
   /** \brief Set to indicate sense-key specific fields are valid */
             sksv          :1;
   /** \brief field indicates which byte of the CDB or param data was in error */
   vmk_uint16 epos;

   /**
    * \brief may contain vendor specific data further defining exception cond.
    *
    * Some vendors want to return additional data which
    * requires a sense buffer of up to 64 bytes.
    *
    * See SPC 3 r23, Section 4.5
    */
   vmk_uint8 additional[46];
} VMK_ATTRIBUTE_PACKED vmk_ScsiSenseData;

/*
 * Default SCSI fixed format sense data structure without any additional data.
 */

typedef struct vmk_ScsiSenseDataSimple {
   union {
      struct {
         /** \brief error type and format of sense data (see SPC 3 r23,
          *         Sec. 4.5.1)
          */
         vmk_uint8 error         :7,
         /** \brief set to one indicates the 'info' field is valid */
                   valid         :1;
         /** \brief obsolete */
         vmk_uint8 segment;
         /** \brief generic information describing the error or exception
          *         condition
          */
         vmk_uint8 key           :4,
                                 :1,
         /** \brief see above spc section */
                   ili           :1,
         /** \brief see above spc section */
                   eom           :1,
         /** \brief see above spc section */
                   filmrk        :1;
         /** \brief see above spc section */
         vmk_uint8 info[4];
         /** \brief see above spc section */
         vmk_uint8 optLen;
         /** \brief see above spc section */
         vmk_uint8 cmdInfo[4];
         /** \brief further information about to the condition reported in
          *         'key'
          */
         vmk_uint8 asc;
         /** \brief detailed information about to the additional sense code
          *         in 'asc'
          */
         vmk_uint8 ascq;
         /** \brief Field Replacable Unit code (see SPC 3 r23, 4.5.2.5) */
         vmk_uint8 fru;
         /** \brief Sense-key specific fields (see SPC 3 r23, 4.5.2.4) */
         vmk_uint8 bitpos        :3,
                   bpv           :1,
                                 :2,
                   cd            :1,
         /** \brief Set to indicate sense-key specific fields are valid */
                   sksv          :1;
         /** \brief field indicates which byte of the CDB or param data was in
          *         error
          */
         vmk_uint16 epos;
      } VMK_ATTRIBUTE_PACKED fixed; /* fixed format sense data */
      struct {
         vmk_uint8 error         :7,
                                 :1;
         vmk_uint8 key           :4,
                                 :4;
         vmk_uint8 asc;
         vmk_uint8 ascq;
         vmk_uint8 reserved[3];
         vmk_uint8 optLen;
         vmk_uint8 additional[10];
      } VMK_ATTRIBUTE_PACKED descriptor; /* descriptor format sense data */
      struct {
         vmk_uint8 senseData[18];
      } VMK_ATTRIBUTE_PACKED byte; /* byte-stream sense data */
   } format;
} vmk_ScsiSenseDataSimple;

/**
 * \brief SCSI Device event handler callback entry
 *
 * This function is called when a registered device
 * event occurs. This function can consume the event
 * or ignore it.
 * 
 * \note  This function must not block.
 *
 * \param[in]  clientData  Client specific data.
 * \param[in]  parm        Parameter of the callback handler.
 * \param[in]  eventType   Type of the event which occured.
 *
 */
typedef void (*vmk_ScsiEventHandlerCbk) (vmk_AddrCookie clientData,
                                         vmk_AddrCookie parm,
                                         vmk_uint32 eventType);

/*
 * Commands
 */

/**
 * \brief SCSI command ID
 */

typedef struct vmk_ScsiCommandId {
   /** \brief unique token of originator of cmd */
   void                        *initiator;
   /** \brief unique serial of cmd */
   vmk_uint64                serialNumber;
} vmk_ScsiCommandId;

struct vmk_ScsiCommand;

typedef void (*vmk_ScsiCommandDoneCbk)(struct vmk_ScsiCommand *cmd);

/**
 * \brief Flag definitions for vmk_ScsiCommand.flags field.
 *
 */
typedef enum {
   /**
    * \brief Command requests Use of Head-of-Q tag.
    *
    * This instructs the lower level drivers to issue this command
    * with Head-of-Queue tag if full task management model is
    * supported by the target else it can be ignored. This is a
    * best-effort flag and its effectiveness is determined by
    * how it is implemented by the underlying driver/devices.
    *
    * \note This is a SCSI target flag and is not used to prioritize
    * I/Os in the internal queues, use
    * VMK_SCSI_COMMAND_FLAGS_USE_PRIORITY_QUEUE for the latter.
    */
   VMK_SCSI_COMMAND_FLAGS_ISSUE_WITH_HEAD_OF_Q_TAG = 0x00020000,
   /**
    * \brief Command requests Use of ORDERED tag.
    *
    * This instructs the lower level drivers to issue this command
    * with ORDERED tag if tagged queuing is being used. If tagged
    * queuing is not in effect for the target, it can be ignored.
    * This is a best-effort flag and its effectiveness is determined
    * by how it is implemented by the underlying driver/devices.
    */
   VMK_SCSI_COMMAND_FLAGS_ISSUE_WITH_ORDERED_TAG  = 0x00010000,
   /**
    * \brief Command is reservation sensitive.
    *
    * If a command has the VMK_SCSI_COMMAND_FLAGS_RESERVATION_SENSITIVE
    * flag set, the plugin should _not_ retry the IO upon failure.
    */
   VMK_SCSI_COMMAND_FLAGS_RESERVATION_SENSITIVE   = 0x00008000,
   /**
    * \brief Command was issued to probe if a device supports it.
    *
    * This should be treated as hint for plugins to not log if these
    * IOs fail with a "not supported" SCSI error status.
    */
   VMK_SCSI_COMMAND_FLAGS_PROBE_FOR_SUPPORT       = 0x00002000,
   /**
    * \brief IO is treated as high priority on internal queues.
    *
    * \note This is not used by drivers/devices to prioritize the
    * I/O at the target. For this use
    * VMK_SCSI_COMMAND_FLAGS_ISSUE_WITH_HEAD_OF_Q_TAG.
    */
   VMK_SCSI_COMMAND_FLAGS_USE_PRIORITY_QUEUE      = 0x00001000,
   /**
    * \brief Fail IO with "no-connect" if device is all-paths-dead.
    *
    * Instructs an MP Plugin to fail the I/O with a
    * VMK_SCSI_HOST_NO_CONNECT host status if it finds that a
    * vmk_ScsiDevice is in an all-paths-dead condition.
    *
    * Normally an MP Plugin should keep retrying the I/O until a path
    * becomes available or it receives a task management abort or
    * reset request.
    */
   VMK_SCSI_COMMAND_FLAGS_NO_CONNECT_IF_APD       = 0x00000800,
   /**
    * \brief Command is issued by a VM that is a guest cluster node.
    *
    * A Cluster may use SCSI reservation as a mechanism to determine
    * node membership. As a result, a VM cluster is particularly
    * sensitive to RESERVATION CONFLICT command status.
    *
    * The MP Plugin must ensure that the VM never sees incorrect
    * reservation conflicts - e.g. reservation conflicts that arise
    * during failover as paths are switched. Further, it must ensure
    * that I/Os are never retried on (correct) reservation
    * conflicts. So, the guest should see all genuine reservation
    * conflicts from the device (e.g. ones caused by the other nodes
    * in the VM cluster), but not any other ones.
    */
   VMK_SCSI_COMMAND_FLAGS_VM_CLUSTER              = 0x00000400,
   /**
    * \brief Indicator of a swap IO that is used to swap memory to disk.
    *
    * The swap IOs help relieve memory pressure on the system.
    * So system should identify them and prioritize them if possible.
    */
   VMK_SCSI_COMMAND_FLAGS_SWAP_IO                 = 0x00000200,
} /** \cond nodoc */ VMK_ATTRIBUTE_PACKED VMK_ATTRIBUTE_ALIGN(1) /* \endcond*/
vmk_ScsiCommandFlags;


/**
 * \brief T10 SCSI target protection types
 *
 * These protection types are also known as DIF (Data Integrity Field)
 * device protection types.
 *
 * SBC 3 r35, Section 4.22.
 */
typedef enum vmk_ScsiTargetProtTypes {
   /** \brief See above sbc section */
   VMK_SCSI_TARGET_PROT_TYPE_0 = 0,
   /** \brief See above sbc section */
   VMK_SCSI_TARGET_PROT_TYPE_1,
   /** \brief See above sbc section */
   VMK_SCSI_TARGET_PROT_TYPE_2,
   /** \brief See above sbc section */
   VMK_SCSI_TARGET_PROT_TYPE_3,
   /** \brief Count of protection types */
   VMK_SCSI_TARGET_PROT_TYPE_MAX,
} vmk_ScsiTargetProtTypes;


/**
 * \brief SCSI command protection operation types
 *
 *  The protection operation types are a power of 2 in order to support
 *  bi-directional commands that can have data flow in both directions
 *  in the same SCSI command.
 */
typedef enum vmk_ScsiCommandProtOps {
   /**
    * \brief Normal protection for a SCSI command.
    *
    * No extra protection is given for a SCSI command than what is normally
    * provided by the transport.
    */
   VMK_SCSI_COMMAND_PROT_NORMAL = (1 << 0),
   /**
    * \brief Protection enabled from HBA to OS for READ SCSI commands.
    *
    * For adapters that do not support target device protection type, this
    * protection type indicates protection is enabled from HBA to OS only.
    * The HBA generates protection data for READ SCSI commands.
    */
   VMK_SCSI_COMMAND_PROT_READ_INSERT = (1 << 1),
   /**
    * \brief Protection enabled from HBA to OS for WRITE SCSI commands.
    *
    * For adapters that do not support target protection type, this type
    * indicates protection data is enabled from HBA to OS for WRITE commands.
    * The HBA strips off the protection data before sending it to the target.
    */
   VMK_SCSI_COMMAND_PROT_WRITE_STRIP = (1 << 2),
   /**
    * \brief Protection enabled from HBA to target for READ SCSI commands.
    *
    * For adapters that do not support DIX, this type indicates protection data
    * is enabled from HBA to target for READ commands. The HBA strips off
    * protection data in the SCSI command and hands it to the storage stack.
    */
   VMK_SCSI_COMMAND_PROT_READ_STRIP = (1 << 3),
   /**
    * \brief Protection enabled from HBA to target for WRITE SCSI commands.
    *
    * For adapters that do not support DIX, this type indicates protection data
    * is enabled from HBA to target only for WRITE commands. The HBA adds
    * protection data and sends it to the target.
    */
   VMK_SCSI_COMMAND_PROT_WRITE_INSERT = (1 << 4),
   /**
    * \brief No change in protection for READ SCSI commands.
    *
    * This type indicates full support of protection data from OS to the target.
    */
   VMK_SCSI_COMMAND_PROT_READ_PASS = (1 << 5),
   /**
    * \brief No change in protection for WRITE SCSI commands.
    *
    * This type indicates full support of protection data from OS to the target.
    */
   VMK_SCSI_COMMAND_PROT_WRITE_PASS = (1 << 6),
   /** \brief Count of SCSI command protection operations */
   VMK_SCSI_COMMAND_PROT_MAX = (1 << 7),
} vmk_ScsiCommandProtOps;


/**
 * \brief Mask of all valid SCSI command protection operation types.
 */
#define VMK_SCSI_COMMAND_PROT_ALL_VALID (VMK_SCSI_COMMAND_PROT_MAX - 1)

#define VMK_SCSI_MAX_CDB_LEN        16
#define VMK_SCSI_MAX_SENSE_DATA_LEN 64

/**
 * \brief SCSI command structure
 */
typedef struct vmk_ScsiCommand {
   /** \brief Command completion callback for async IO. */
   vmk_ScsiCommandDoneCbk       done;
   /** \brief Command completion callback argument for async IO. */
   void                         *doneData;
   /**
    * \brief Scatter/gather array representing the machine-address ranges
    *        associated with the IO buffer.
    */
   vmk_SgArray                  *sgArray;
   /** \brief Hint indicating the direction of data transfer */
   vmk_ScsiCommandDirection     dataDirection;
   /** \brief Command flags, see above */
   vmk_uint32                   flags;
   /** \brief Hint indicating whether a cmd is a read IO */
   vmk_Bool                     isReadCdb;
   /** \brief Hint indicating whether a cmd is a write IO */
   vmk_Bool                     isWriteCdb;
   /** \brief Reserved */
   vmk_uint8                    reserved1[2];
   /**
    * \brief Minimum data transfer length.
    *
    * Must be <= vmk_SgGetDataLen(sgArray).
    */
   vmk_ByteCountSmall           requiredDataLen;
   /** \brief WorldId of the world on behalf of whom cmd is issued. */
   vmk_uint32                   worldId;
   /**
    * \brief Number of blocks to be transferred.
    *
    * Only valid for when isReadCdb or isWriteCdb are set.
    */
   vmk_uint32                   lbc;
   /**
    * \brief address of first block to be xfer'd
    *
    * Only valid for isReadCdb or isWriteCdb.
    */
   vmk_uint64                   lba;
   /**
    * \brief Number of milliseconds since system boot before
    *        the command times out.
    *
    *   0 indicates the cmd will not be timed out.
    *
    *   A nonzero value indicates the system time beyond which the storage
    *   framework will issue an abort task management request and fail
    *   the IO with a timeout status.
    */
   vmk_uint64                   absTimeoutMs;
   /** \brief Unique tag for this command. */
   vmk_ScsiCommandId            cmdId;
   /** \brief SCSI command descriptor block. */
   vmk_uint8                    cdb[VMK_SCSI_MAX_CDB_LEN];
   /**
    * \brief Number of valid bytes in cdb.
    *
    *  Must be <= VMK_SCSI_MAX_CDB_LEN
    */
   vmk_ByteCountSmall           cdbLen;
   /** \brief Command completion status. */
   vmk_ScsiCmdStatus            status;
   /** \brief Number of bytes transferred to or from the data buffer. */
   vmk_ByteCountSmall           bytesXferred;

   /**
    * \deprecated Do not use in Native Drivers
    * \brief Worldlet ID that submitted the command, if any.
    */
   vmk_WorldletID               worldletId;
   /**
    * \brief Scatter/gather array representing the IO-address ranges
    *        associated with the IO buffer.
    *
    * \note Sometimes this will be the same as sgArray but other
    *       times it will be different. Do not rely on the contents
    *       of sgIOArray and sgArray being identical.
    */
   vmk_SgArray                  *sgIOArray;
   /**
    * \brief SCSI sense data.
    *
    * \deprecated This structure member is being deprecated.
    * Consumers of this field should rely on the APIs
    * vmk_ScsiCmd{Get,Set,Clear}SenseData() and
    * vmk_ScsiGetSupportedCmdSenseDataSize().
    *
    * Only valid if vmk_ScsiCmdStatusIsCheck(cmd->status).
    * Do not add any fields below this member.
    */
   vmk_ScsiSenseData            senseData;
   /** \brief Reserved. */
   vmk_VA                 reserved2[2];
} vmk_ScsiCommand;

/**
 * \brief SCSI task management action
 */
typedef enum {
   VMK_SCSI_TASKMGMT_ACTION_IGNORE          = 0,
   VMK_SCSI_TASKMGMT_ACTION_ABORT           = 1,
} vmk_ScsiTaskMgmtAction;

/**
 * \brief Used to abort all commands, regardless of initiator and s/n
 */
#define VMK_SCSI_TASKMGMT_ANY_INITIATOR (void*)0xA11

/**
 * \brief SCSI task management
 */
typedef struct vmk_ScsiTaskMgmt {
   /** \brief magic */
   vmk_uint32               magic;
   /** \brief task mgmt type */
   vmk_ScsiTaskMgmtType     type;
   /** \brief command status */
   vmk_ScsiCmdStatus        status;
   /** \brief cmdId of the cmd to abort.  cmdId.serialNumber is only
    * applicable for VMK_SCSI_TASKMGMT_ABORT and ignored for all
    * other vmk_ScsiTaskMgmtType values.  cmdId.initiator is only
    * applicable for VMK_SCSI_TASKMGMT_ABORT and
    * VMK_SCSI_TASKMGMT_VIRT_RESET, ignored for all other
    * vmk_ScsiTaskMgmtType values
    */
   vmk_ScsiCommandId        cmdId;
   /** \brief worldId of the cmd(s) to abort.  only applicable for
    * VMK_SCSI_TASKMGMT_ABORT and VMK_SCSI_TASKMGMT_VIRT_RESET, ignored
    * for all other vmk_ScsiTaskMgmtType values
    */
   vmk_uint32               worldId;
} vmk_ScsiTaskMgmt;

/*
 * SCSI Adapter
 */

/**
 * \brief Don't care value for 'hostMaxSectors'.
 */
#define VMK_SCSI_HOST_MAX_SECTORS_IGNORE 0xFFFF

/**
 * \brief actions the 'discover' function can execute
 */
typedef enum vmk_ScanAction {
   /** Create a path to the named LUN */
   VMK_SCSI_SCAN_CREATE_PATH,
   /** Keep a previously created path to the named LUN */
   VMK_SCSI_SCAN_CONFIGURE_PATH,
   /** Destroy the path to the named LUN */
   VMK_SCSI_SCAN_DESTROY_PATH,
} vmk_ScanAction;

/**
 * \brief bitmasks for adapter events
 *
 * Events are bit fields because users can
 * wait for multiple events
 *
 */
typedef enum vmk_ScsiAdapterEvents {
   VMK_SCSI_ADAPTER_EVENT_FC_LOOP_UP                = 0x00000001,
   VMK_SCSI_ADAPTER_EVENT_FC_LOOP_DOWN              = 0x00000002,
   VMK_SCSI_ADAPTER_EVENT_FC_RSCN                   = 0x00000004,
   VMK_SCSI_ADAPTER_EVENT_FC_NEW_TARGET             = 0x00000008,
   VMK_SCSI_ADAPTER_EVENT_FC_REMOVED_TARGET         = 0x00000010,
   VMK_SCSI_ADAPTER_EVENT_FC_NEW_VPORT              = 0x00000020,
   VMK_SCSI_ADAPTER_EVENT_FC_REMOVED_VPORT          = 0x00000040,
   VMK_SCSI_ADAPTER_EVENT_HOT_PLUG_ADD              = 0x00000080,
   VMK_SCSI_ADAPTER_EVENT_HOT_PLUG_REMOVE           = 0x00000100,
   VMK_SCSI_ADAPTER_EVENT_FC_TARGET_SCAN_DONE       = 0x00000200,
} vmk_ScsiAdapterEvents;

   /** \brief Event handler callback entry */
typedef void (*vmk_EventHandlerCbk) (void *clientData, vmk_uint32 eventType);

/**
 * \brief bitmasks for flags field of SCSI adapter
 *
 * Through this flags, give vmkernel information how handle the adapter.
 *
 */
typedef enum vmk_ScsiAdapterFlags {
   /** \deprecated Legacy from Console-OS based shared block adapters */
   VMK_SCSI_ADAPTER_FLAG_BLOCK                   = 0x00000001,
   /**
    * \deprecated Do not use in Native Drivers
    * This adapter is an NPIV VPORT */
   VMK_SCSI_ADAPTER_FLAG_NPIV_VPORT              = 0x00000002,
   /** Do not scan when registering this adapter */
   VMK_SCSI_ADAPTER_FLAG_REGISTER_WITHOUT_SCAN   = 0x00000004,
   /** Do not scan periodically */
   VMK_SCSI_ADAPTER_FLAG_NO_PERIODIC_SCAN        = 0x00000008,
   /** Ok to probe periodically, but no adapter rescan */
   VMK_SCSI_ADAPTER_FLAG_NO_ADAPTER_RESCAN       = 0x00000010,
   /**
    * \deprecated Do not use in Native Drivers
    * This adapter is a Legacy NPIV VPORT */
   VMK_SCSI_ADAPTER_FLAG_NPIV_LEGACY_VPORT       = 0x00000020,
} vmk_ScsiAdapterFlags;


/**
 * \brief bitmask for supported guard types
 *
 * Used to set default guard type for SCSI Adapter and is used by the OS to
 * determine guard type to be used for SCSI commands.
 */
typedef enum vmk_ScsiGuardTypes {
   /** \brief Supports T10 CRC checksumming */
   VMK_SCSI_GUARD_CRC = (1 << 0),
   /** \brief Supports IP checksumming */
   VMK_SCSI_GUARD_IP  = (1 << 1),
   /** \brief Max value of guard type */
   VMK_SCSI_GUARD_MAX = (1 << 2),
} vmk_ScsiGuardTypes;

/**
 * \brief Mask of all valid SCSI DIX Guard Types.
 */
#define VMK_SCSI_GUARD_TYPE_ALL_VALID  (VMK_SCSI_GUARD_MAX - 1)

/**
 * \brief bitmasks for SCSI data protection types
 *
 * Used to set supported protection types for SCSI adapter.
 */
typedef enum vmk_ScsiProtTypes {
   /** \brief Type 1 Protection */
   VMK_SCSI_TYPE1_PROT = (1 << 0),
   /** \brief Type 2 Protection */
   VMK_SCSI_TYPE2_PROT = (1 << 1),
   /** \brief Type 3 Protection */
   VMK_SCSI_TYPE3_PROT = (1 << 2),
   /** \brief DIX with Type 0 Protection */
   VMK_SCSI_DIX_TYPE0_PROT = (1 << 3),
   /** \brief DIX with Type 1 Protection */
   VMK_SCSI_DIX_TYPE1_PROT = (1 << 4),
   /** \brief DIX with Type 2 Protection */
   VMK_SCSI_DIX_TYPE2_PROT = (1 << 5),
   /** \brief DIX with Type 3 Protection */
   VMK_SCSI_DIX_TYPE3_PROT = (1 << 6),
   /** \brief Max value of protection types  */
   VMK_SCSI_PROT_TYPE_MAX = (1 << 7),
} vmk_ScsiProtTypes;


/**
 * \brief Mask of all valid SCSI Data Protection Types.
 */
#define VMK_SCSI_PROT_TYPE_ALL_VALID \
               (VMK_SCSI_PROT_TYPE_MAX - 1)


/**
 * \brief Adapter capabilities mask
 *
 * Capabilities of a SCSI adapter. The driver can set capability with a call
 * to vmk_ScsiAdapterSetCapabilities().
 */
typedef vmk_uint32 vmk_ScsiAdapterCapabilitiesMask;

/**
 * \brief bitmasks for SCSI adapter capabilities
 *
 * Used to set capabilities of SCSI adapter.
 */
typedef enum vmk_ScsiAdapterCapabilities {
   /** \brief Supports second-level addressing */
   VMK_SCSI_ADAPTER_CAP_SECONDLEVEL_ADDRESSING = (1 << 0),
   /** \brief Supports data integrity */
   VMK_SCSI_ADAPTER_CAP_DATA_INTEGRITY         = (1 << 1),
   /** \brief Driver provides own completion worlds */
   VMK_SCSI_ADAPTER_CAP_DRIVER_COMPL_WORLDS    = (1 << 2),
   /** \brief Max value of adapter capabilities */
   VMK_SCSI_ADAPTER_CAP_MAX = (1 << 3),
} vmk_ScsiAdapterCapabilities;


/**
 * \brief Mask of all valid SCSI Adapter capabilities.
 */
#define VMK_SCSI_ADAPTER_CAP_ALL_VALID (VMK_SCSI_ADAPTER_CAP_MAX - 1)

/*
 ***********************************************************************
 * vmk_ScsiAdapterCommand --                                      */ /**
 *
 * \brief Queue a SCSI command on the adapter and LUN specified.
 *
 * Perform the command passed in 'cmd' on the adapter passed in
 * 'clientData' on the LUN passed in 'deviceData'.
 *
 * \note Required function pointer
 *
 * \param[in]  clientData  Handle to the adapter to perform the
 *                         comand on.
 * \param[in]  cmd         Pointer to the command to execute.
 * \param[in]  deviceData  Pointer to deviceData returned by 'discover'
 *                         Create path.
 *
 * \retval VMK_WOULD_BLOCK cmd not queued because of Queue Depth limit
 *                         or device was quiesced.
 * \retval VMK_NO_MEMORY   Out of memory.
 * \retval VMK_OK          Success - cmd queued or will be completed
 *                         with error.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_ScsiAdapterCommand)(void *clientData,
                                                   struct vmk_ScsiCommand *cmd,
                                                   void *deviceData);

/*
 ***********************************************************************
 * vmk_ScsiAdapterTaskMgmt --                                     */ /**
 *
 * \brief Perform a SCSI task management command on the adapter and
 *        LUN specified.
 *
 * Perform the task management command passed in 'taskMgmt' on the
 * adapter passed in 'clientData' (if applicable) and on the LUN
 * passed in 'deviceData'.
 *
 * \note Required function pointer
 *
 * \param[in]  clientData  Handle to the adapter to perform the task
 *                         management action on.
 * \param[in]  taskMgmt    Pointer to the task management command to
 *                         be executed.
 * \param[in]  deviceData  Pointer to deviceData returned by 'discover'
 *                         Create.
 *
 * \retval VMK_FAILURE   Task management command failed.
 * \retval VMK_NO_MEMORY Out of memory.
 * \retval VMK_OK        Success.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_ScsiAdapterTaskMgmt)(
                          void *clientData,
                          struct vmk_ScsiTaskMgmt *taskMgmt,
                          void *deviceData);

/*
 ***********************************************************************
 * vmk_ScsiAdapterDumpCommand --                                  */ /**
 *
 * \brief Queue a SCSI command during a core dump on the adapter
 *        and LUN specified.
 *
 * Perform the command passed in 'cmd' on the adapter passed in
 * 'clientData' on the LUN passed in 'deviceData' during a core dump.
 *
 * \note Required function pointer
 *
 * \note Please see the description of system dumps in the VMkernel
 *       fundamentals guide.
 *
 * \param[in]  clientData  Handle to the adapter to perform the discover
 *                         action on.
 * \param[in]  cmd         Pointer to the command to execut.
 * \param[in]  deviceData  Pointer to deviceData returned by 'discover'
 *                         Create.
 *
 * \note No one else can issue commands to this device anymore though
 *       the ordinary 'command' entry point.
 *
 * \retval VMK_WOULD_BLOCK cmd not queued because of Queue Depth limit
 *                         or device was quiesced.
 * \retval VMK_BUSY      Command cannot be executed, error handler active.
 * \retval VMK_FAILURE   Command cannot be executed.
 * \retval VMK_NO_MEMORY Out of memory.
 * \retval VMK_OK        Success - cmd queued or will be completed
 *                       with error.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_ScsiAdapterDumpCommand)(
                          void *clientData,
                          vmk_ScsiCommand *cmd,
                          void *deviceData);

/*
 ***********************************************************************
 * vmk_ScsiAdapterDumpQueue --                                    */ /**
 *
 * \brief Log the current adapter queue.
 *
 * \note Required function pointer
 *
 * \param[in]  clientData  Handle to the adapter.
 *
 ***********************************************************************
 */
typedef void (*vmk_ScsiAdapterDumpQueue)(void *clientData);

/*
 ***********************************************************************
 * vmk_ScsiAdapterDumpPollHandler --                              */ /**
 *
 * \brief Run the adapter's poll handler, called on the dump device
 *        during a system dump.
 *
 * \note Required function pointer
 *
 * \note Please see the description of system dumps in the VMkernel
 *       fundamentals guide.
 *
 * \param[in]  clientData  Handle to the adapter.
 *
 * \param[in]  dumpPollHandlerData Argument passed to dumpPollHandler
 *
 ***********************************************************************
 */
typedef void (*vmk_ScsiAdapterDumpPollHandler)(void *clientData);


/*
 ***********************************************************************
 * vmk_ScsiAdapterDiscover --                                     */ /**
 *
 * \brief Perform the discovery action specified
 *
 * The driver should perform the action specified in 'action' for the
 * LUN described by 'channel', 'targetId' and 'lunId'.
 * The pointer passed in 'deviceData' points to a void * to place or
 * use the driver private data structure pointer that is used as a
 * handle to the LUN.
 *
 * \note Required function pointer
 *
 * \note This function is allowed to block in the driver.
 *
 * \param[in]  clientData     Handle to the adapter.
 * \param[in]  action         One of the vmk_ScanAction enums declared above
 *                            can be create, configure or destroy.
 * \param[in]  channel        The channel to perform the action on.
 * \param[in]  targetId       The target Id to perform the action on.
 * \param[in]  lunId          The LUN Id to perform the action on.
 * \param[in,out]  deviceData Pointer to where the deviceData pointer
 *                            is placed for create and destroy
 *
 * \note *deviceData is the private data structure the driver returns
 *       to represent the channel/targetId/lunId created. This pointer is
 *       used by the VMkernel as a handle for performance reasons in
 *       various other function pointer calls to represent the path
 *       to the LUN to be acted on.
 *
 * \note Return values for Create (VMK_SCSI_SCAN_CREATE_PATH)
 * \retval VMK_NO_CONNECT  The targetId does not exist or a target error
 *                         occured.
 * \retval VMK_NO_MEMORY   Out of memory (not enough to create path).
 * \retval VMK_EXISTS      Path with specified channel/targetId/lunId
 *                         already exists - *deviceData returned for
 *                         existing path
 * \retval VMK_OK          Success - *deviceData returned for newly
 *                         created path
 *
 * \note Return values for Configure (VMK_SCSI_SCAN_CONFIGURE_PATH)
 * \retval VMK_FAILURE     Path cannot be configured
 * \retval VMK_NOT_FOUND   Path cannot be found
 * \retval VMK_OK          Success - *deviceData not changed
 *
 * \note Return values for Destroy (VMK_SCSI_SCAN_DESTROY_PATH)
 * \retval VMK_FAILURE     Path cannot be destroyed
 * \retval VMK_OK          Success - *deviceData will be written with NULL
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_ScsiAdapterDiscover)(
                          void *clientData,
                          vmk_ScanAction action,
                          int channel,
                          int targetId,
                          int lunId,
                          void **deviceData);

/*
 ***********************************************************************
 * vmk_ScsiAdapterModifyDeviceQueueDepth --                       */ /**
 *
 * \brief Modify path queue depth on LUN specified if possible.
 *
 * \param[in]  clientData  Handle to the adapter.
 * \param[in]  qDepth      New queue depth to set.
 * \param[in]  deviceData  LUN pointer returned by 'discover' Create.
 *
 * \note Optional function pointer
 *
 * \retval Returns the new queue depth in effect, which could be
 *         lower than the depth actually requested if the driver
 *         cannot honor the requested (higher) queue depth.
 * \retval Returns 0 if not a valid path.
 *
 ***********************************************************************
 */
typedef int (*vmk_ScsiAdapterModifyDeviceQueueDepth)(
             void *clientData,
             int qDepth,
             void *deviceData);


/*
 ***********************************************************************
 * vmk_ScsiAdapterQueryDeviceQueueDepth --                        */ /**
 *
 * \brief Return the current path queue depth on LUN specified.
 *
 * \note Required function pointer
 *
 * \param[in]  clientData  Handle to the adapter.
 * \param[in]  deviceData  LUN pointer returned by 'discover' Create.
 *
 * \retval Returns the current queue depth for the path.
 * \retval Returns 0 if not a valid path.
 *
 ***********************************************************************
 */
typedef int (*vmk_ScsiAdapterQueryDeviceQueueDepth)(
             void *clientData,
             void *deviceData);


/*
 ***********************************************************************
 * vmk_ScsiAdapterCheckTarget --                                  */ /**
 *
 * \brief Check if the specified target exists on the adapter and
 *        channel specified.
 *
 * \note Required function pointer for SAN drivers, optional
 *       for all others.
 *
 * \note If checkTarget returns VMK_FAILURE, we will not call
 *       discover to create paths.
 *
 * \param[in]  clientData  Handle to the adapter.
 * \param[in]  channel     The channel to check for target on.
 * \param[in]  targetId    The target Id to check.
 *
 * \retval VMK_FAILURE   Target does not exist
 * \retval VMK_OK        Success - target exists
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_ScsiAdapterCheckTarget)(
                          void *clientData,
                          int channel,
                          int targetId);

/*
 ***********************************************************************
 * vmk_ScsiAdapterNotifyIOAllowed --                              */ /**
 *
 * \brief Callback to notify when IO is allowed to adapter
 *
 * \note Optional function pointer
 *
 * \note The Native Driver must assume IO is not possible until a
 *       "VMK_TRUE" is notified through this API.
 *
 * \note This function is not allowed to block in the driver.
 *
 * \note Passes VMK_TRUE or VMK_FALSE on these conditions:
 *       VMK_TRUE -  When logical device registration is complete and
 *                   when unquiescing the logical device.
 *       VMK_FALSE - When logical device quiesce is complete and when
 *                   deviceForget is complete.
 *
 * \param[in] logicalDevice  Handle to the logical device.
 * \param[in] ioAllowed      VMK_TRUE if IO is allowed, VMK_FALSE if IO
 *                           not allowed.
 *
 ***********************************************************************
 */
typedef void (*vmk_ScsiAdapterNotifyIOAllowed)(
              vmk_Device logicalDevice,
              vmk_Bool ioAllowed);


/**
 * \brief main data structure for SCSI adapter
 *
 * The discover() entrypoint passes back a deviceData.
 * This deviceData must be able to completely identify
 * the channel:target:lun of the device as it's the only
 * thing that will be passed back once it's set by the
 * discover() entrypoint.
 */
typedef struct vmk_ScsiAdapter {
   /**
    * \deprecated Do not use in Native Drivers
    * \brief DMA constraints for this adapter.
    * \note See 'engine' field below for more information.
    */
   vmk_DMAConstraints constraints;
   /** \brief Device for this adapter. */
   vmk_Device device;
   /** \brief max # of blocks per i/o */
   int                  hostMaxSectors;
   /** \brief Pointer to the total queue depth for the adapter */
   vmk_uint32           *qDepthPtr;

   /** \brief Issue a SCSI command to the specified device */
   vmk_ScsiAdapterCommand command;

   /** \brief Issue a SCSI task management */
   vmk_ScsiAdapterTaskMgmt taskMgmt;

   /** \brief Issue a SCSI command during a core dump */
   vmk_ScsiAdapterDumpCommand dumpCommand;

   /**
    * \deprecated Do not use in Native Drivers
    * \brief Perform a close on the adapter specified.
    */
   void (*close)(void *clientData);

   /**
    * \deprecated Do not use in Native Drivers
    * \brief proc handler for the given SCSI host adapter.
    */
   VMK_ReturnStatus (*procInfo)(
      void* clientData,
      char* buf,
      vmk_ByteCountSmall offset,
      vmk_ByteCountSmall count,
      vmk_ByteCountSmall* nbytes,
      int isWrite);

   /** \brief Log the current adapter queue */
   vmk_ScsiAdapterDumpQueue dumpQueue;

   /** \brief Run the adapter's poll handler, called on the dump device
    *         during a system dump. */
   vmk_ScsiAdapterDumpPollHandler dumpPollHandler;

   /** \brief Passed as the argument to dumpPollHandler() */
   void *dumpPollHandlerData;

   /**
    * \deprecated Do not use in Native Drivers
    * \brief driver specific ioctl
    */
   VMK_ReturnStatus (*ioctl)(
      void *clientData,
      void *deviceData,
      vmk_uint32 fileFlags,
      vmk_uint32 cmd,
      vmk_VA userArgsPtr,
      vmk_IoctlCallerSize callerSize,
      vmk_int32 *drvErr);

   /** \brief discover & destroy a device */
   vmk_ScsiAdapterDiscover discover;

   /**
    * \deprecated Do not use in Native Drivers
    * \brief send NPIV specific commands to a device
    */
   VMK_ReturnStatus (*vportop)(
      void *clientData,
      vmk_uint32 cmd,
      void *arg);

   /**
    * \deprecated Do not use in Native Drivers
    * \brief scan a single LUN on a vport
    */
   VMK_ReturnStatus (*vportDiscover)(
      void *clientData,
      vmk_ScanAction action,
      int ctlr,
      int devNum,
      int lun,
      struct vmk_ScsiAdapter **vmkAdapter,
      void **deviceData);

   /** \brief try changing the path queue depth */
   vmk_ScsiAdapterModifyDeviceQueueDepth modifyDeviceQueueDepth;

   /** \brief query the path queue depth */
   vmk_ScsiAdapterQueryDeviceQueueDepth queryDeviceQueueDepth;

   /** \brief checks if a target exists */
   vmk_ScsiAdapterCheckTarget checkTarget;

   /** \brief SCSI target id of adapter, or -1 if n/a */
   int                  targetId;
   /** \brief adapter flags, see vmk_ScsiAdapterFlags above */
   vmk_uint32           flags;
   int                  reserved;
   /** \brief id of module running this adapter */
   vmk_ModuleID         moduleID;
   /** \brief adapter creator's private convenience scratch data ptr
     *
     * \note clientData is a pointer to a private structure provided
     *       by the driver for use as 'handle' to represent an
     *       adapter for many function calls.
     */
   void                 *clientData;
   /** \brief # of channels */
   int                  channels;
   /** \brief # of targets per channel */
   int                  maxTargets;
   /** \brief max # of logical units per target */
   int                  maxLUNs;
   /** \brief Unique adapter number. Set by vmk_ScsiAllocateAdapter. */
   vmk_uint16           hostNum;
   /** \brief adapter support physical address extensions? */
   vmk_Bool             paeCapable;
   /** \brief max len of SCSI cmds adapter can accept */
   vmk_uint8            maxCmdLen;
   /** \brief adapter name */
   vmk_Name             name;
   /** \brief driver name */
   vmk_Name             driverName;
   /**
    * \deprecated Do not use in Native Drivers
    * \brief /proc directory */
   vmk_Name             procName;
   /** \brief transport specific mgmtAdapter hooks */
   vmk_SCSITransportMgmt mgmtAdapter;
   /** \brief notify when IO is allowed/disallowed */
   vmk_ScsiAdapterNotifyIOAllowed notifyIOAllowed;
   /*
    * \brief DMA Engine created by driver
    *
    * \note Native Drivers must create and pass a DMA Engine for use by the
    *       PSA stack for SCSI IO. The vmk_DMAConstraints are embedded
    *       in the 'engine' at creation time.
    */
   vmk_DMAEngine engine;
} vmk_ScsiAdapter;

/*
 * Physical Path
 */

#define VMK_SCSI_PATH_ANY_ADAPTER        "*"
#define VMK_SCSI_PATH_ANY_CHANNEL        ~0
#define VMK_SCSI_PATH_ANY_TARGET         ~0
#define VMK_SCSI_PATH_ANY_LUN            ~0

typedef enum {
   VMK_SCSI_PLUGIN_STATE_ENABLED,
   VMK_SCSI_PLUGIN_STATE_DISABLING,
   VMK_SCSI_PLUGIN_STATE_DISABLED,
   VMK_SCSI_PLUGIN_STATE_CLAIM_PATHS,
} vmk_ScsiPluginState;

/**
 * \deprecated Do not use in Native Drivers
 * \brief Opaque handle for Completion Objects, provided by lower layer.
 */
typedef vmk_AddrCookie vmk_ScsiCompletionHandle;

/**
 * \deprecated Do not use in Native Drivers
 * \brief Completion Object information passed by the vmkLinux module.
 */
typedef struct vmk_ScsiCompObjectInfo {
   /** \brief Completion Object Handle assigned by the lower layer. */
   vmk_ScsiCompletionHandle completionHandle;
   /** \brief Worldlet - if any - associated with the completion object. */
   vmk_Worldlet  completionWorldlet;
   vmk_uint8     infoPad[8];
} vmk_ScsiCompObjectInfo;

/**
 * \brief Format of INQUIRY request block.
 * SPC 3 r23, Section 6.4.1 table 80
 */
typedef struct vmk_ScsiInquiryCmd {
   /** \brief see above spc section */
   vmk_uint8 opcode;
   /** \brief see above spc section */
   vmk_uint8 evpd  :1,
   /** \brief see above spc section */
         cmddt :1,
   /** \brief see above spc section */
         resv12:3,
   /** \brief see above spc section */
         lun   :3;
   /** \brief see above spc section */
   vmk_uint8 pagecode;
   /** \brief see above spc section */
   vmk_uint16 length;
   /** \brief see above spc section */
   vmk_uint8 ctrl;
} VMK_ATTRIBUTE_PACKED vmk_ScsiInquiryCmd;

/**
 * \brief Format of INQUIRY response block.
 * SPC 3 r23, Section 6.4.2 table 81
 */
typedef struct vmk_ScsiInquiryResponse {
   /** \brief see above spc section */
   vmk_uint8 devclass    :5,
   /** \brief see above spc section */
         pqual             :3;
#define VMK_SCSI_PQUAL_CONNECTED     0  // device described is connected to the LUN
#define VMK_SCSI_PQUAL_NOTCONNECTED  1  // target supports such a device, but none is connected
#define VMK_SCSI_PQUAL_NODEVICE      3  // target does not support a physical device for this LUN
   /** \brief see above spc section */
   vmk_uint8    :7,
   /** \brief see above spc section */
         rmb:1;
   /** \brief see above spc section */
   vmk_uint8 ansi             :3,
#define VMK_SCSI_ANSI_SCSI1      0x0   // device supports SCSI-1
#define VMK_SCSI_ANSI_CCS        0x1   // device supports the CCS
#define VMK_SCSI_ANSI_SCSI2      0x2   // device supports SCSI-2
#define VMK_SCSI_ANSI_SCSI3_SPC  0x3   // device supports SCSI-3 version SPC
#define VMK_SCSI_ANSI_SCSI3_SPC2 0x4   // device supports SCSI-3 version SPC-2
#define VMK_SCSI_ANSI_SCSI3_SPC3 0x5   // device supports SCSI-3 version SPC-3
#define VMK_SCSI_ANSI_SCSI3_SPC4 0x6   // device supports SCSI-3 version SPC-4
   /** \brief see above spc section */
         ecma             :3,
   /** \brief see above spc section */
         iso             :2;
   /** \brief see above spc section */
   vmk_uint8 dataformat  :4,
   /** \brief see above spc section */
                     :1,
   /** \brief see above spc section */
         naca             :1,
   /** \brief see above spc section */
         tio             :1,
   /** \brief see above spc section */
         aen             :1;
   /** \brief see above spc section */
   vmk_uint8 optlen;
   /** \brief see above spc section */
   vmk_uint8         protect   :1,
   /** \brief see above spc section */
         rsrv              :2,
   /** \brief see above spc section */
          tpcp              :1,
   /** \brief see above spc section */
         tpgs        :2,
   /** \brief see above spc section */
         acc             :1,
   /** \brief see above spc section */
         sccs             :1;
#define VMK_SCSI_TPGS_NONE                       0x0
#define VMK_SCSI_TPGS_IMPLICIT_ONLY              0x1
#define VMK_SCSI_TPGS_IMPLICIT                   VMK_SCSI_TPGS_IMPLICIT_ONLY
#define VMK_SCSI_TPGS_EXPLICIT_ONLY              0x2
#define VMK_SCSI_TPGS_EXPLICIT                   VMK_SCSI_TPGS_EXPLICIT_ONLY
#define VMK_SCSI_TPGS_BOTH_IMPLICIT_AND_EXPLICIT 0x3
#define VMK_SCSI_TPGS_BOTH                       VMK_SCSI_TPGS_BOTH_IMPLICIT_AND_EXPLICIT
   /** \brief see above spc section */
   vmk_uint8 adr16             :1,
   /** \brief see above spc section */
         adr32             :1,
   /** \brief see above spc section */
         arq             :1,
   /** \brief see above spc section */
         mchngr             :1,
   /** \brief see above spc section */
         dualp             :1,
   /** \brief see above spc section */
         port             :1,
   /** \brief see above spc section */
                     :2;
   /** \brief see above spc section */
   vmk_uint8 sftr             :1,
   /** \brief see above spc section */
         que             :1,
   /** \brief see above spc section */
         trndis             :1,
   /** \brief see above spc section */
         link             :1,
   /** \brief see above spc section */
         sync             :1,
   /** \brief see above spc section */
         w16             :1,
   /** \brief see above spc section */
         w32             :1,
   /** \brief see above spc section */
         rel             :1;
   /** \brief see above spc section */
   vmk_uint8 manufacturer[8];
   /** \brief see above spc section */
   vmk_uint8 product[16];
   /** \brief see above spc section */
   vmk_uint8 revision[4];
   /** \brief see above spc section */
   vmk_uint8 vendor1[20];
   /** \brief see above spc section */
   vmk_uint8 reserved[40];
} vmk_ScsiInquiryResponse;

#define VMK_SCSI_STANDARD_INQUIRY_MIN_LENGTH 36

/**
 * \brief Format of INQUIRY EVPD responses
 * SPC 3 r23, Section 7.6
 */
typedef struct vmk_ScsiInquiryVPDResponse {
   /** \brief see above spc section */
   vmk_uint8 devclass    :5,
   /** \brief see above spc section */
             pqual       :3;
   /** \brief see above spc section */
   vmk_uint8 pageCode;
   /** \brief see above spc section */
   vmk_uint8 reserved;
   /** \brief see above spc section */
   vmk_uint8 payloadLen;
   /** \brief see above spc section */
   vmk_uint8 payload[0];
} VMK_ATTRIBUTE_PACKED VMK_ATTRIBUTE_ALIGN(1) vmk_ScsiInquiryVPDResponse;

/**
 * \brief Format of INQUIRY EVPD Page 83 response
 *  SPC 3 r23, Section 7.6.3.1 table 294
 */
typedef struct vmk_ScsiInquiryVPD83Response {
   /** \brief see above spc section */
   vmk_uint8 devclass    :5,
   /** \brief see above spc section */
             pqual       :3;
   /** \brief see above spc section */
   vmk_uint8 pageCode;
   /** \brief see above spc section */
   vmk_uint16 payloadLen;
   /** \brief see above spc section */
   vmk_uint8 payload[0];
} VMK_ATTRIBUTE_PACKED VMK_ATTRIBUTE_ALIGN(1) vmk_ScsiInquiryVPD83Response;

/**
 * \brief Format of INQUIRY EVPD Page 83 response id descriptor
 *  SPC 3 r23, Section 7.6.3.1 table 295
 */
typedef struct vmk_ScsiInquiryVPD83IdDesc {
   /** \brief see above spc section */
   vmk_uint8 codeSet     :4,
   /** \brief see above spc section */
             protocolId  :4;
#define VMK_SCSI_EVPD83_ID_VENDOR        (0x0)
#define VMK_SCSI_EVPD83_ID_T10           (0x1)
#define VMK_SCSI_EVPD83_ID_EUI           (0x2)
#define VMK_SCSI_EVPD83_ID_NAA           (0x3)
#define VMK_SCSI_EVPD83_ID_RTP           (0x4)
#define VMK_SCSI_EVPD83_ID_TPG           (0x5)
#define VMK_SCSI_EVPD83_ID_LUG           (0x6)
#define VMK_SCSI_EVPD83_ID_MD5           (0x7)
#define VMK_SCSI_EVPD83_ID_SCSINAME      (0x8)
   /** \brief see above spc section */
   vmk_uint8 idType      :4,
#define VMK_SCSI_EVPD83_ASSOCIATION_LU   (0x0)
#define VMK_SCSI_EVPD83_ASSOCIATION_PORT (0x1)
#define VMK_SCSI_EVPD83_ASSOCIATION_DEV  (0x2)
   /** \brief see above spc section */
             association :2,
   /** \brief see above spc section */
             reserved1   :1,
   /** \brief see above spc section */
             piv         :1;
   /** \brief see above spc section */
   vmk_uint8 reserved2;
   /** \brief see above spc section */
   vmk_uint8 idLen;
   /** \brief see above spc section */
   vmk_uint8 id[0];
} VMK_ATTRIBUTE_PACKED VMK_ATTRIBUTE_ALIGN(1) vmk_ScsiInquiryVPD83IdDesc;

#define VMK_SCSI_EVPD83_NAA_EXTENDED     (0x2)
#define VMK_SCSI_EVPD83_NAA_REGISTERED   (0x5)
#define VMK_SCSI_EVPD83_NAA_REG_EXT      (0x6)

/**
 * \brief Format of INQUIRY EVPD Page 86 response
 * SPC 4 r33, Section 7.8.8 page 650
 */
typedef struct vmk_ScsiInquiryVPD86Response {
   /** \brief see above spc section */
   vmk_uint8  devClass      :5,
   /** \brief see above spc section */
              pQual         :3;
   /** \brief see above spc section */
   vmk_uint8  pageCode;
   /** \brief see above spc section */
   vmk_uint16 pageLength;
   /** \brief see above spc section */
   vmk_uint8  refChk        :1,
   /** \brief see above spc section */
              appChk        :1,
   /** \brief see above spc section */
              grdChk        :1,
   /** \brief see above spc section */
              spt           :3,
   /** \brief see above spc section */
              actUcode      :2;
   /** \brief see above spc section */
   vmk_uint8  simSup        :1,
   /** \brief see above spc section */
              ordSup        :1,
   /** \brief see above spc section */
              headSup       :1,
   /** \brief see above spc section */
              priorSup      :1,
   /** \brief see above spc section */
              groupSup      :1,
   /** \brief see above spc section */
              uaskSup       :1,
   /** \brief see above spc section */
              res1          :2;
   /** \brief see above spc section */
   vmk_uint8  vSup          :1,
   /** \brief see above spc section */
              nvSup         :1,
   /** \brief see above spc section */
              crdSup        :1,
   /** \brief see above spc section */
              wuSup         :1,
   /** \brief see above spc section */
              res2          :4;
   /** \brief see above spc section */
   vmk_uint8  luiClr        :1,
   /** \brief see above spc section */
              res3          :3,
   /** \brief see above spc section */
              piiSup        :1,
   /** \brief see above spc section */
              res4          :3;
   /** \brief see above spc section */
   vmk_uint8  cbcs          :1,
   /** \brief see above spc section */
              res5          :3,
   /** \brief see above spc section */
              rSup          :1,
   /** \brief see above spc section */
              res6          :3;
   /** \brief see above spc section */
   vmk_uint8  multiUcode    :4,
   /** \brief see above spc section */
              res7          :4;
   /** \brief see above spc section */
   vmk_uint16 extSelfTestMins;
   /** \brief see above spc section */
   vmk_uint8  depLU         :1,
   /** \brief see above spc section */
              supLU         :1,
   /** \brief see above spc section */
              res8          :3,
   /** \brief see above spc section */
              vsaSup        :1,
   /** \brief see above spc section */
              hraSup        :1,
   /** \brief see above spc section */
              poaSup        :1;
   /** \brief see above spc section */
   vmk_uint8  maxSenseDataLen;
   /*
    * Reserved bytes - 14 - 63
    */
   vmk_uint8  res9[50];
} VMK_ATTRIBUTE_PACKED VMK_ATTRIBUTE_ALIGN(1) vmk_ScsiInquiryVPD86Response;

/**
 * \brief NAA IEEE Registered Extended identification field foramt
 * SPC 3 r23:
 * Section 7.6.3.6.4 table 309.
 */
typedef struct vmk_ScsiNAARegExtendedIdField {
   vmk_uint8 companyIdMSB :4,
             NAA          :4;
   vmk_uint16 companyIdMiddleBytes;
   vmk_uint8 vendorSpecIdMSB :4,
             companyIdLSB    :4;
   vmk_uint32 vendorSpecIdLSB;
   vmk_uint64 vendorSpecIdExt;
} __attribute__((packed,aligned(1))) vmk_ScsiNAARegExtendedIdField;

/**
 * \brief Format of the 6 byte version of MODE SELECT
 * SPC 3 r23, Section 6.9.1 table 97
 */
typedef struct vmk_ScsiModeSenseCmd {
   /** \brief see above spc section */
   vmk_uint8    opcode;
   /** \brief see above spc section */
   vmk_uint8          :3,
   /** \brief see above spc section */
            dbd          :1,
                  :1,
   /** \brief see above spc section */
            lun          :3;
   /** \brief see above spc section */
   vmk_uint8    page          :6,
#define VMK_SCSI_MS_PAGE_VENDOR   0x00     // vendor-specific (ALL)
#define VMK_SCSI_MS_PAGE_RWERROR  0x01     // read/write error (DISK/TAPE/CDROM/OPTICAL)
#define VMK_SCSI_MS_PAGE_CONNECT  0x02     // disconnect/connect (ALL)
#define VMK_SCSI_MS_PAGE_FORMAT   0x03     // format (DISK)
#define VMK_SCSI_MS_PAGE_PARALLEL 0x03     // parallel interface (PRINTER)
#define VMK_SCSI_MS_PAGE_UNITS    0x03     // measurement units (SCANNER)
#define VMK_SCSI_MS_PAGE_GEOMETRY 0x04     // rigid disk geometry (DISK)
#define VMK_SCSI_MS_PAGE_SERIAL   0x04     // serial interface (PRINTER)
#define VMK_SCSI_MS_PAGE_FLEXIBLE 0x05     // flexible disk geometry (DISK)
#define VMK_SCSI_MS_PAGE_PRINTER  0x05     // printer operations (PRINTER)
#define VMK_SCSI_MS_PAGE_OPTICAL  0x06     // optical memory (OPTICAL)
#define VMK_SCSI_MS_PAGE_VERIFY   0x07     // verification error (DISK/CDROM/OPTICAL)
#define VMK_SCSI_MS_PAGE_CACHE    0x08     // cache (DISK/CDROM/OPTICAL)
#define VMK_SCSI_MS_PAGE_PERIPH   0x09     // peripheral device (ALL)
#define VMK_SCSI_MS_PAGE_CONTROL  0x0a     // control mode (ALL)
#define VMK_SCSI_MS_PAGE_MEDIUM   0x0b     // medium type (DISK/CDROM/OPTICAL)
#define VMK_SCSI_MS_PAGE_NOTCH    0x0c     // notch partitions (DISK)
#define VMK_SCSI_MS_PAGE_CDROM    0x0d     // CD-ROM (CDROM)
#define VMK_SCSI_MS_PAGE_CDAUDIO  0x0e     // CD-ROM audio (CDROM)
#define VMK_SCSI_MS_PAGE_COMPRESS 0x0f     // data compression (TAPE)
#define VMK_SCSI_MS_PAGE_CONFIG   0x10     // device configuration (TAPE)
#define VMK_SCSI_MS_PAGE_EXCEPT   0x1c     // informal exception (ALL:SCSI-3)
#define VMK_SCSI_MS_PAGE_CDCAPS   0x2a     // CD-ROM capabilities and mechanical status (CDROM)
// more defined...
#define VMK_SCSI_MS_PAGE_ALL      0x3f     // all available pages (ALL)
   /** \brief see above spc section */
            pcf          :2;
#define VMK_SCSI_MS_PCF_CURRENT   0x00     // current values
#define VMK_SCSI_MS_PCF_VOLATILE  0x01     // changeable values
#define VMK_SCSI_MS_PCF_DEFAULT   0x02     // default values
#define VMK_SCSI_MS_PCF_SAVED     0x03     // saved values
   /** \brief see above spc section */
   vmk_uint8    subpage;
   /** \brief see above spc section */
   vmk_uint8    length;
   /** \brief see above spc section */
   vmk_uint8    ctrl;
} vmk_ScsiModeSenseCmd;

/**
 * \brief Format of READ_CAPACITY(10) response block.
 * SBC 2 r16, section 5.10.1 table 34
 */
typedef struct {
   /** \brief see above sbc section */
   vmk_uint32 lbn;
   /** \brief see above sbc section */
   vmk_uint32 blocksize;
} vmk_ScsiReadCapacityResponse;

/*
 * Format of REPORT TARGET PORT GROUPS request and response blocks.
 */


/**
 * \brief target port group command for SCSI report
 * SPC 3 r23, section 6.25 table 162
 */
typedef struct vmk_ScsiReportTargetPortGroupsCmd {
   /** \brief see above spc section */
   vmk_uint8    opcode;
   /** \brief see above spc section */
   vmk_uint8    svc:5,
#define VMK_SCSI_RTPGC_SVC    0xA
   /** \brief see above spc section */
                res1:3;
   /** \brief see above spc section */
   vmk_uint8    reserved1[4];
   /** \brief see above spc section */
   vmk_uint32   length;
   /** \brief see above spc section */
   vmk_uint8    reserved2;
   /** \brief see above spc section */
   vmk_uint8    ctrl;
} VMK_ATTRIBUTE_PACKED vmk_ScsiReportTargetPortGroupsCmd;

/**
 * \brief target port group response for SCSI report
 * SPC 3 r23, section 6.25 table 163
 */
typedef struct vmk_ScsiReportTargetPortGroupsResponse {
   /** \brief see above spc section */
   vmk_uint32   length;
} VMK_ATTRIBUTE_PACKED vmk_ScsiReportTargetPortGroupsResponse;

/**
 * \brief target port group descriptor for SCSI report
 * SPC 3 r23, section 6.25 table 164
 */
typedef struct  vmk_ScsiReportTargetPortGroupDescriptor {
   /** \brief see above spc section */
   vmk_uint8   aas:4,
   /** \brief see above spc section */
               res1:3,
   /** \brief see above spc section */
               pref:1;
#define VMK_SCSI_TPGD_AAS_AO      0x00 //!< active/optimized
#define VMK_SCSI_TPGD_AAS_ANO     0x01 //!< active/nonoptomized
#define VMK_SCSI_TPGD_AAS_STBY    0x02 //!< standby
#define VMK_SCSI_TPGD_AAS_UNAVAIL 0x03 //!< unavailable
#define VMK_SCSI_TPGD_AAS_TRANS   0x0F //!< transitioning
   /** \brief see above spc section */
   vmk_uint8   aosup:1,
   /** \brief see above spc section */
               ansup:1,
   /** \brief see above spc section */
               ssup:1,
   /** \brief see above spc section */
               usup:1,
   /** \brief see above spc section */
               res2:3,
   /** \brief see above spc section */
               tsup:1;
   /** \brief see above spc section */
   vmk_uint16 targetPortGroup;
   /** \brief see above spc section */
   vmk_uint8  reserved1;
#define VMK_SCSI_TPGD_STAT_NONE 0x00 //!< no status available
#define VMK_SCSI_TPGD_STAT_EXP  0x01 //!< state altered by SET command
#define VMK_SCSI_TPGD_STAT_IMP  0x02 //!< state altered by implicit behavior
   /** \brief see above spc section */
   vmk_uint8  status;
   /** \brief see above spc section */
   vmk_uint8  vendorSpec;
   /** \brief see above spc section */
   vmk_uint8  targetPortCount;
} VMK_ATTRIBUTE_PACKED vmk_ScsiReportTargetPortGroupDescriptor;

/**
 * \brief SCSI target port descriptor format
 * SPC 3 r23, section 6.25 table 167
 */
typedef struct vmk_ScsiReportTargetPortDescriptor {
   /** \brief see above spc section */
   vmk_uint16  obsolete;
   /** \brief see above spc section */
   vmk_uint16  relativeTargetPortId;
} VMK_ATTRIBUTE_PACKED vmk_ScsiReportTargetPortDescriptor;

/**
 * \brief set target port groups command
 * SPC 3 r23, Section 6.31 table 178
 */
typedef struct vmk_ScsiSetTargetPortGroupsCmd{
   /** \brief see above spc section */
   vmk_uint8    opcode;
   /** \brief see above spc section */
   vmk_uint8    svc:5,
#define VMK_SCSI_STPGC_SVC    0xA
   /** \brief see above spc section */
      res1:3;
   /** \brief see above spc section */
   vmk_uint8    reserved1[4];
   /** \brief see above spc section */
   vmk_uint32   length;
   /** \brief see above spc section */
   vmk_uint8    reserved2;
   /** \brief see above spc section */
   vmk_uint8    ctrl;
} VMK_ATTRIBUTE_PACKED vmk_ScsiSetTargetPortGroupsCmd;

#define SCSI_STPG_PARAM_LIST_OFFSET 4

/**
 * \brief set target port group descriptor
 * SPC 3 r23, Section 6.31 table 180
 */
typedef struct vmk_ScsiSetTargetPortGroupDescriptor{
   /** \brief see above spc section */
   vmk_uint8  aas:4,
   /** \brief see above spc section */
      res1:4;
   /** \brief see above spc section */
   vmk_uint8  reserved1;
   /** \brief see above spc section */
   vmk_uint16 targetPortGroup;
} VMK_ATTRIBUTE_PACKED vmk_ScsiSetTargetPortGroupDescriptor;


/**
 * \brief parameter header of module sense for SCSI
 * SPC 3 r23, Section 7.4.3 table 239
 */
typedef struct vmk_Scsi4ByteModeSenseParameterHeader {
   /** \brief see above spc section */
   vmk_uint8   modeDataLength;
   /** \brief see above spc section */
   vmk_uint8   mediumType;
   /** \brief see above spc section */
   vmk_uint8   reserved1:4,
   /** \brief see above spc section */
               dpofua:1,
   /** \brief see above spc section */
               reserved2:2,
   /** \brief see above spc section */
               wp:1;
   /** \brief see above spc section */
   vmk_uint8   blockDescriptorLength;
} VMK_ATTRIBUTE_PACKED vmk_Scsi4ByteModeSenseParameterHeader;

/**
 * \brief block descriptor for SCSI mode sense
 * SPC 3 r23, Section 7.4.4.1 table 241
 */
typedef struct vmk_ScsiModeSenseBlockDescriptor {
   /** \brief see above spc section */
   vmk_uint8   densityCode;
   /** \brief see above spc section */
   vmk_uint8   numberOfBlocks[3];
   /** \brief see above spc section */
   vmk_uint8   reserved1;
   /** \brief see above spc section */
   vmk_uint8   blockLength[3];
} VMK_ATTRIBUTE_PACKED vmk_ScsiModeSenseBlockDescriptor;


/**
 * \brief Format of READ/WRITE (6) request.
 * SBC 2 r16, Section 5.5 table 26 & 5.24 table 61
 */
typedef struct vmk_ScsiReadWrite6Cmd {
   /** \brief see above sbc section */
   vmk_uint32 opcode:8,
   /** \brief see above sbc section */
              lun:3,
   /** \brief see above sbc section */
              lbn:21;
   /** \brief see above sbc section */
   vmk_uint8  length;
   /** \brief see above sbc section */
   vmk_uint8  control;
} VMK_ATTRIBUTE_PACKED vmk_ScsiReadWrite6Cmd;

/**
 * \brief Format of READ/WRITE (10) request.
 * SBC 2 r16, Section 5.6 table 28 & 5.25 table 62
 */
typedef struct vmk_ScsiReadWrite10Cmd {
   /** \brief see above sbc section */
   vmk_uint8 opcode;
   /** \brief see above sbc section */
   vmk_uint8 rel   :1,
                   :2,
   /** \brief see above sbc section */
             flua  :1,
   /** \brief see above sbc section */
             dpo   :1,
   /** \brief see above sbc section */
             lun   :3;
   /** \brief see above sbc section */
   vmk_uint32 lbn;
   /** \brief see above sbc section */
   vmk_uint8 reserved;
   /** \brief see above sbc section */
   vmk_uint16 length;
   /** \brief see above sbc section */
   vmk_uint8 control;
} VMK_ATTRIBUTE_PACKED vmk_ScsiReadWrite10Cmd;

/**
 * \brief Format of READ/WRITE (12) request.
 * SBC 2 r16, Section 5.7 table 31 & 5.26 table 65
 */
typedef struct vmk_ScsiReadWrite12Cmd {
   /** \brief see above sbc section */
   vmk_uint8 opcode;
   /** \brief see above sbc section */
   vmk_uint8 rel   :1,
                   :2,
   /** \brief see above sbc section */
             flua  :1,
   /** \brief see above sbc section */
             dpo   :1,
   /** \brief see above sbc section */
             lun   :3;
   /** \brief see above sbc section */
   vmk_uint32 lbn;
   /** \brief see above sbc section */
   vmk_uint32 length;
   /** \brief see above sbc section */
   vmk_uint8 reserved;
   /** \brief see above sbc section */
   vmk_uint8 control;
} VMK_ATTRIBUTE_PACKED vmk_ScsiReadWrite12Cmd;

/**
 * \brief Format of READ/WRITE (16) request.
 * SBC 2 r16, Section 5.8 table 32 & 5.27 table 66
 */
typedef struct vmk_ScsiReadWrite16Cmd {
   /** \brief see above sbc section */
   vmk_uint8 opcode;
   /** \brief see above sbc section */
   vmk_uint8 rel   :1,
                   :2,
   /** \brief see above sbc section */
             flua  :1,
   /** \brief see above sbc section */
             dpo   :1,
                   :3;
   /** \brief see above sbc section */
   vmk_uint64 lbn;
   /** \brief see above sbc section */
   vmk_uint32 length;
   /** \brief see above sbc section */
   vmk_uint8 reserved;
   /** \brief see above sbc section */
   vmk_uint8 control;
} VMK_ATTRIBUTE_PACKED vmk_ScsiReadWrite16Cmd;

/**
 * \brief Scsi page types for inquiry data
 */
typedef enum {
   VMK_SCSI_INQ_TYPE_STD    = 1,
   VMK_SCSI_INQ_TYPE_EVPD80 = 2,
   VMK_SCSI_INQ_TYPE_EVPD83 = 3,
   VMK_SCSI_INQ_TYPE_EVPD86 = 4,
} vmk_ScsiInqType;

/**
 * \brief Persistent Reserve Out Command
 * SPC 3 r23, Section 6.12.1 (Table 112)
 */
typedef struct vmk_ScsiPersistentReserveOutCmd {
   /** \brief see above spc section */
   vmk_uint8  opcode;
   /** \brief see above spc section */
   vmk_uint8  serviceAction :5,
   /** \brief see above spc section */
              reserved      :3;
   /** \brief see above spc section */
   vmk_uint8  type          :4,
   /** \brief see above spc section */
              scope         :4;
   /** \brief see above spc section */
   vmk_uint8  reserved1[2];
   /** \brief see above spc section */
   vmk_uint32 parameterListLength;
   /** \brief see above spc section */
   vmk_uint8  control;
} VMK_ATTRIBUTE_PACKED vmk_ScsiPersistentReserveOutCmd;

/**
 * \brief Persistent Reserve Out parameter List.
 * SPC 3 r23, Section 6.12.3 (Table 114)
 */
typedef struct vmk_ScsiPersistentReserveOutPList {
   /** \brief see above spc section */
   vmk_uint64 reservationKey;
   /** \brief see above spc section */
   vmk_uint64 serviceActionResKey;
   /** \brief see above spc section */
   vmk_uint8  obsolete1[4];
   /** \brief see above spc section */
   vmk_uint8  aptpl          :1,
   /** \brief see above spc section */
              reserved1      :1,
   /** \brief see above spc section */
              all_tg_pt      :1,
   /** \brief see above spc section */
              spec_i_pt      :1,
   /** \brief see above spc section */
              reserved2      :4;
   /** \brief see above spc section */
   vmk_uint8  reserved3;
   /** \brief see above spc section */
   vmk_uint8  obsolete2[2];
} VMK_ATTRIBUTE_PACKED vmk_ScsiPersistentReserveOutPList;

/**
 * \brief Persistent Reserve In Command
 * SPC 3 r23, Section 6.11.1 (Table 101)
 */
typedef struct vmk_ScsiPersistentReserveInCmd {
   /** \brief see above spc section */
   vmk_uint8  opcode;
   /** \brief see above spc section */
   vmk_uint8  serviceAction :5,
   /** \brief see above spc section */
              reserved      :3;
   /** \brief see above spc section */
   vmk_uint8  reserved1[5];
   /** \brief see above spc section */
   vmk_uint16 allocationLength;
   /** \brief see above spc section */
   vmk_uint8  control;
} VMK_ATTRIBUTE_PACKED vmk_ScsiPersistentReserveInCmd;

/**
 * \brief Persistent read reservation response
 * SPC 3 r23, Section 6.11.3.4 (Table 105)
 */
typedef struct vmk_ScsiReadReservationResp {
   /** \brief see above spc section */
   vmk_uint32 prGeneration;
   /** \brief see above spc section */
   vmk_uint32 additionalLength;
   /** \brief see above spc section */
   vmk_uint64 reservationKey;
   /** \brief see above spc section */
   vmk_uint8  obsolete[4];
   /** \brief see above spc section */
   vmk_uint8  reserved;
   /** \brief see above spc section */
   vmk_uint8  type      :4,
   /** \brief see above spc section */
              scope     :4;
   /** \brief see above spc section */
   vmk_uint8 obsolete1[2];
} VMK_ATTRIBUTE_PACKED vmk_ScsiReadReservationResp;

/**
 * \brief Extended copy (XCOPY) command
 * SPC 3 r23, Section 6.3.1 table 50
 */
typedef struct vmk_ScsiExtendedCopyCmd {
   /** \brief see above spc section */
   vmk_uint8 opcode;
   /** \brief see above spc section */
   vmk_uint8 reserved1[9];
   /** \brief see above spc section */
   vmk_uint32 parameterListLength;
   /** \brief see above spc section */
   vmk_uint8 reserved2;
   /** \brief see above spc section */
   vmk_uint8 control;
} __attribute__ ((packed)) vmk_ScsiExtendedCopyCmd;

/**
 * \brief Extended copy parameter list header
 * SPC 3 r23, Section 6.3.1 table 51
 */
typedef struct vmk_ScsiExtendedCopyParamListHeader {
   /** \brief see above spc section */
   vmk_uint8	listID;
   /** \brief see above spc section */
   vmk_uint8	priority:3,
   /** \brief see above spc section */
                reserved1:1,
   /** \brief see above spc section */
                NRCR:1,
   /** \brief see above spc section */
                STR:1,
   /** \brief see above spc section */
                reserved2:2;
   /** \brief see above spc section */
   vmk_uint16	targetDescriptorListLength;
   /** \brief see above spc section */
   vmk_uint8	reserved3[4];
   /** \brief see above spc section */
   vmk_uint32	segmentDescriptorListLength;
   /** \brief see above spc section */
   vmk_uint32	inlineDataLength;
} __attribute__ ((packed,aligned(1))) vmk_ScsiExtendedCopyParamListHeader;

/**
 * \brief Identification descriptor target
 *        descriptor for the extended copy command
 * SPC 3 r23, Section 6.3.6.2 table 56
 */
typedef struct vmk_ScsiIdDescriptorTargetDescriptor {
   /** \brief see above spc section */
   vmk_uint8	descTypeCode;
   /** \brief see above spc section */
   vmk_uint8	peripheralDeviceType:5,
   /** \brief see above spc section */
                nul:1,
   /** \brief see above spc section */
                luIdType:2;
   /** \brief see above spc section */
   vmk_uint16	relativeInitiatorPortId;
   /** \brief see above spc section */
   vmk_uint8 codeSet     :4,
   /** \brief see above spc section */
             reserved1   :4;
   /** \brief see above spc section */
   vmk_uint8 idType      :4,
   /** \brief see above spc section */
             association :2,
   /** \brief see above spc section */
             reserved2   :2;
   /** \brief see above spc section */
   vmk_uint8 reserved3;
   /** \brief see above spc section */
   vmk_uint8 idLen;
   /** \brief see above spc section */
   vmk_uint8    varLenId[20];
   /** \brief see above spc section */
   vmk_uint8	devTypeSpecParams[4];
} __attribute__ ((packed,aligned(1))) vmk_ScsiIdDescriptorTargetDescriptor;


/**
 * \brief Device type specific target descriptor parameters for block
 *        device types, for the extended copy command
 * SPC 3 r23, Section 6.3.6.4 table 58
 */
typedef struct vmk_ScsiBlockDeviceSpecTargetDescParams {
   /** \brief see above spc section */
   vmk_uint8	reserved1:2;
   /** \brief see above spc section */
   vmk_uint8	PAD:1;
   /** \brief see above spc section */
   vmk_uint8	reserved2:5;
   /** \brief see above spc section */
   vmk_uint8	blockSize[3];
} __attribute__ ((packed,aligned(1))) vmk_ScsiBlockDeviceSpecTargetDescParams;


/**
 * \brief Block device to block device segment descriptor
 *        for the extended copy command
 * SPC 3 r23, Section 6.3.7.5 table 66
 */
typedef struct vmk_ScsiBlockToBlockSegmentDescriptor {
   /** \brief see above spc section */
   vmk_uint8	descriptorTypeCode;
   /** \brief see above spc section */
   vmk_uint8	CAT:1,
   /** \brief see above spc section */
                DC:1,
   /** \brief see above spc section */
                reserved1:6;
   /** \brief see above spc section */
   vmk_uint16	descriptorLength;
   /** \brief see above spc section */
   vmk_uint16	srcTargetDescriptorIndex;
   /** \brief see above spc section */
   vmk_uint16	dstTargetDescriptorIndex;
   /** \brief see above spc section */
   vmk_uint8	reserved2[2];
   /** \brief see above spc section */
   vmk_uint16	numBlocks;
   /** \brief see above spc section */
   vmk_uint64	srcLBA;
   /** \brief see above spc section */
   vmk_uint64	dstLBA;
} __attribute__ ((packed,aligned(1))) vmk_ScsiBlockToBlockSegmentDescriptor;

/**
 * \brief Format of REQUEST SENSE block.
 * SPC 3 r23, Section 6.27 table 170
 */
typedef struct vmk_ScsiRequestSenseCommand {
   /** \brief Operation Code (03h) */
   vmk_uint8 opcode;
   /** \brief DESC */
   vmk_uint8 desc:1,
   /** \brief Reserved */
             resv1:4,
   /** \brief Reserved in SPC3R23, and ignored */
             lun:3;
   /** \brief Reserved */
   vmk_uint8  resv2;
   /** \brief Reserved */
   vmk_uint8 resv3;
   /** \brief Allocation Length */
   vmk_uint8  len;
   /** \brief Control */
   vmk_uint8  control;
} VMK_ATTRIBUTE_PACKED vmk_ScsiRequestSenseCommand;

/**
 * \brief Format of MODE SELECT   block.
 * SPC 3 r23, Section 6.7 table 94
 */
typedef struct vmk_ScsiModeSelectCommand {
   /** \brief Operation Code (15h) */
   vmk_uint8 opcode;
   /** \brief Save Pages */
   vmk_uint8 sp:1,
   /** \brief Reserved */
             resv11:3,
   /** \brief Page Format */
             pf:1,
   /** \brief Actually, reserved */
             lun:3;
   /** \brief Reserved */
   vmk_uint8 resv2;
   /** \brief Reserved */
   vmk_uint8 resv3;
   /** \brief Allocation Length */
   vmk_uint8 len;
   /** \brief Control */
   vmk_uint8 control;
} VMK_ATTRIBUTE_PACKED vmk_ScsiModeSelectCommand;

/**
 * \brief Format of REPORT LUNS   block.
 * SPC 3 r23, Section 6.21 table 147
 */
typedef struct vmk_ScsiReportLunsCommand {
   /** \brief Operation Code (A0h) */
   vmk_uint8 opcode;
   /** \brief Reserved */
   vmk_uint8 resv1;
   /** \brief SELECT REPORT */
   vmk_uint8 selectReport;
   /** \brief Reserved */
   vmk_uint8 resv2;
   /** \brief Reserved */
   vmk_uint8 resv3;
   /** \brief Reserved */
   vmk_uint8 resv4;
   /** \brief Allocation Length */
   vmk_uint32 len;
   /** \brief Reserved */
   vmk_uint8 resv5;
   /** \brief Control */
   vmk_uint8 control;
} VMK_ATTRIBUTE_PACKED vmk_ScsiReportLunsCommand;

/**
 * \brief Format of READ CAPACITY 10 byte  block.
 * SBC 3 r18, Section 5.12.1 table 45
 */
typedef struct vmk_ScsiReadCap10Command {
   /** \brief Operation Code (25h) */
   vmk_uint8 opcode;
   /** \brief Obsolete */
   vmk_uint8 obs:1,
             resv1:7;
   /** \brief Logical Block Address */
   vmk_uint32 lba;
   vmk_uint8 resv2;
   vmk_uint8 resv3;
   /** \brief PMI */
   vmk_uint8 pmi:1,
             resv4:7;
   /** \brief Control */
   vmk_uint8 control;
} VMK_ATTRIBUTE_PACKED vmk_ScsiReadCap10Command;

/**
 * \brief Format of READ CAPACITY 16 byte  block.
 * SBC 3 r18, Section 5.13.1 table 47
 */
typedef struct vmk_ScsiReadCap16Command {
   /** \brief Operation Code (9eh) */
   vmk_uint8 opcode;
   /** \brief Service Action (10h) */
   vmk_uint8 sa:5,
             resv1:3;
   /** \brief Logical Block Address */
   vmk_uint64 lba;
   /** \brief Allocation Length */
   vmk_uint32 len;
   vmk_uint8 pmi:1,
             resv2:7;
   /** \brief Control */
   vmk_uint8 control;
} VMK_ATTRIBUTE_PACKED vmk_ScsiReadCap16Command;

/**
 * \brief Format of READ BUFFER   block.
 * SPC 3 r23, Section 6.15 table 126
 */
typedef struct vmk_ScsiReadBufferCommand {
   /** \brief Operation Code (3Ch) */
   vmk_uint8 opcode;
   /** \brief Mode */
   vmk_uint8 mode:5,
   /** \brief Reserved */
             resv1:3;
   /** \brief BUFFER ID */
   vmk_uint8 bufferId;
   /** \brief BUFFER OFFSET - MSB */
   vmk_uint8 msbbo;
   /** \brief BUFFER OFFSET - MID */
   vmk_uint8 midbo;
   /** \brief BUFFER OFFSET - LSB */
   vmk_uint8 lsbbo;
   /** \brief Allocation Length MSB */
   vmk_uint8 msblen;
   /** \brief Allocation Length MID */
   vmk_uint8 midlen;
   /** \brief Allocation Length LSB */
   vmk_uint8 lsblen;
   /** \brief Control */
   vmk_uint8 control;
} VMK_ATTRIBUTE_PACKED vmk_ScsiReadBufferCommand;

/**
 * \brief WRITE LONG (16) Command
 * SBC 2 r16, Secion 5.34 table 73
 */
typedef struct vmk_ScsiWriteLong16 {
   /** \brief Operation Code (9fh) */
   vmk_uint8 opcode;
   /** \brief service action (11h) */
   vmk_uint8  serviceAction :5,
   /** \brief reserved */
              reserved1     :3;
   /** \brief see above sbc section */
   vmk_uint64 lba;
   /** \brief see above sbc section */
   vmk_uint8  reserved2[2];
   /** \brief see above sbc section */
   vmk_uint16 transferLength;
   /** \brief see above sbc section */
   vmk_uint8 corrct     : 1,
             reserved3  : 7;
   /** \brief see above sbc section */
   vmk_uint8  control;
} VMK_ATTRIBUTE_PACKED vmk_ScsiWriteLong16;

#endif  /* _VMKAPI_SCSI_TYPES_H_ */
/** @} */
/** @} */
