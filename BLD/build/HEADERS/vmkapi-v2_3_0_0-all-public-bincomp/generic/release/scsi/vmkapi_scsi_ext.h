/* **********************************************************
 * Copyright 2007 - 2009 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */


/*
 ***********************************************************************
 * SCSI Externally Exported Interfaces                            */ /**
 * \addtogroup SCSI
 * @{
 *
 * \defgroup SCSIext SCSI Interfaces Exported to User Mode
 *  
 * Vmkernel-specific SCSI constants & types which are shared with
 * user-mode code.
 * @{
 *
 ***********************************************************************
 */

#ifndef _VMKAPI_SCSI_EXT_H_
#define _VMKAPI_SCSI_EXT_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/**
 * \brief Length of the device class description,
 *        including the trailing NUL character.
 */
#define VMK_SCSI_CLASS_MAX_LEN	18

/** \cond nodoc */
#define VMK_SCSI_DEVICE_CLASSES \
   VMK_SCSI_DEVICE_CLASS_NUM(VMK_SCSI_CLASS_DISK, 0, "Direct-Access    ") \
   VMK_SCSI_DEVICE_CLASS(VMK_SCSI_CLASS_TAPE,        "Sequential-Access") \
   VMK_SCSI_DEVICE_CLASS(VMK_SCSI_CLASS_PRINTER,     "Printer          ") \
   VMK_SCSI_DEVICE_CLASS(VMK_SCSI_CLASS_CPU,         "Processor        ") \
   VMK_SCSI_DEVICE_CLASS(VMK_SCSI_CLASS_WORM,        "WORM             ") \
   VMK_SCSI_DEVICE_CLASS(VMK_SCSI_CLASS_CDROM,       "CD-ROM           ") \
   VMK_SCSI_DEVICE_CLASS(VMK_SCSI_CLASS_SCANNER,     "Scanner          ") \
   VMK_SCSI_DEVICE_CLASS(VMK_SCSI_CLASS_OPTICAL,     "Optical Device   ") \
   VMK_SCSI_DEVICE_CLASS(VMK_SCSI_CLASS_MEDIA,       "Medium Changer   ") \
   VMK_SCSI_DEVICE_CLASS(VMK_SCSI_CLASS_COM,         "Communications   ") \
   VMK_SCSI_DEVICE_CLASS(VMK_SCSI_CLASS_IDE_CDROM,   "Class 0xA/IDE CDROM") \
   VMK_SCSI_DEVICE_CLASS(VMK_SCSI_CLASS_IDE_OTHER,   "Class 0xB/IDE OTHER") \
   VMK_SCSI_DEVICE_CLASS(VMK_SCSI_CLASS_RAID,        "RAID Ctlr        ") \
   VMK_SCSI_DEVICE_CLASS(VMK_SCSI_CLASS_ENCLOSURE,   "Enclosure Svc Dev") \
   VMK_SCSI_DEVICE_CLASS(VMK_SCSI_CLASS_SIMPLE_DISK, "Simple disk      ") \
   VMK_SCSI_DEVICE_CLASS(VMK_SCSI_CLASS_RESV1,       "Reserved 0xF     ") \
   VMK_SCSI_DEVICE_CLASS(VMK_SCSI_CLASS_RESV2,       "Reserved 0x10    ") \
   VMK_SCSI_DEVICE_CLASS(VMK_SCSI_CLASS_RESV3,       "Reserved 0x11    ") \
   VMK_SCSI_DEVICE_CLASS(VMK_SCSI_CLASS_RESV4,       "Reserved 0x12    ") \
   VMK_SCSI_DEVICE_CLASS(VMK_SCSI_CLASS_RESV5,       "Reserved 0x13    ") \
   VMK_SCSI_DEVICE_CLASS(VMK_SCSI_CLASS_RESV6,       "Reserved 0x14    ") \
   VMK_SCSI_DEVICE_CLASS(VMK_SCSI_CLASS_RESV7,       "Reserved 0x15    ") \
   VMK_SCSI_DEVICE_CLASS(VMK_SCSI_CLASS_RESV8,       "Reserved 0x16    ") \
   VMK_SCSI_DEVICE_CLASS(VMK_SCSI_CLASS_RESV9,       "Reserved 0x17    ") \
   VMK_SCSI_DEVICE_CLASS(VMK_SCSI_CLASS_RESV10,      "Reserved 0x18    ") \
   VMK_SCSI_DEVICE_CLASS(VMK_SCSI_CLASS_RESV11,      "Reserved 0x19    ") \
   VMK_SCSI_DEVICE_CLASS(VMK_SCSI_CLASS_RESV12,      "Reserved 0x1A    ") \
   VMK_SCSI_DEVICE_CLASS(VMK_SCSI_CLASS_RESV13,      "Reserved 0x1B    ") \
   VMK_SCSI_DEVICE_CLASS(VMK_SCSI_CLASS_RESV14,      "Reserved 0x1C    ") \
   VMK_SCSI_DEVICE_CLASS(VMK_SCSI_CLASS_RESV15,      "Reserved 0x1D    ") \
   VMK_SCSI_DEVICE_CLASS(VMK_SCSI_CLASS_RESV16,      "Reserved 0x1E    ") \
   VMK_SCSI_DEVICE_CLASS(VMK_SCSI_CLASS_UNKNOWN,     "No device type   ") \

#define VMK_SCSI_DEVICE_CLASS(name, description) \
   /** \brief description */ name,
#define VMK_SCSI_DEVICE_CLASS_NUM(name, value, description) \
   /** \brief description */ name = value,
/** \endcond */

/**
 * \brief SCSI device classes.
 *
 * The device classes are mostly according to the SCSI spec, but a
 * few unsupported types have been "overloaded" for IDE devices:
 *  - Class 0xA: VMK_SCSI_CLASS_IDE_CDROM : IDE CDROM device.
 *  - Class 0xB: VMK_SCSI_CLASS_IDE_OTHER : Other IDE device.
 */
typedef enum {
   VMK_SCSI_DEVICE_CLASSES
   VMK_SCSI_DEVICE_CLASS_LAST
} vmk_ScsiDeviceClass;

/** \cond nodoc */
#undef VMK_SCSI_DEVICE_CLASS
#undef VMK_SCSI_DEVICE_CLASS_NUM
/** \endcond */

/** \cond nodoc */
/**
 * \note  This is an internal definition, only to be used inside this header file.
 */
#define _VMK_SCSI_STRINGIFY(var...) #var

#define VMK_SCSI_DEVICE_STATES \
   VMK_SCSI_DEVICE_STATE_NUM(VMK_SCSI_DEVICE_STATE_ON, 0, "on", \
                             "The device is operational.",\
                             _VMK_SCSI_STRINGIFY(This is the normal operating mode\
                                           of a device and what it will enter\
                                           into during registration.))\
   VMK_SCSI_DEVICE_STATE(VMK_SCSI_DEVICE_STATE_OFF, "off", \
                         "The device has been disabled by user intervention.",) \
   VMK_SCSI_DEVICE_STATE(VMK_SCSI_DEVICE_STATE_APD, "APD", \
                         "There are no paths to the device.",)\
   VMK_SCSI_DEVICE_STATE(VMK_SCSI_DEVICE_STATE_QUIESCED, "quiesced", \
                         "The device is not processing I/Os.", \
                          _VMK_SCSI_STRINGIFY(This option can only be used with\
                                              VMK_SCSI_DEVICE_UNREGISTER to \
                                              quiesce I/O and other activity \
                                              at device unregistration time.))\
   VMK_SCSI_DEVICE_STATE(VMK_SCSI_DEVICE_STATE_PERM_LOSS, "permanent device loss", \
                         "The device is permanently unavailable.", \
                         _VMK_SCSI_STRINGIFY(This is used to indicate a permanent\
                                       loss of the device due to user unmapping\
                                       the LUN, unrecoverable h/w error, uid change.\
                                       This MUST NOT be used to indicate transient\
                                       failures. The following state info values\
                                       are applicable to the\
                                       VMK_DEVICE_STATE_PERM_DEV_LOSS state :\
                                       VMK_DEVICE_INFO_UNRECHW_ERROR,\
                                       VMK_DEVICE_INFO_UID_CHANGE,\
                                       VMK_DEVICE_INFO_UNMAP,\
                                       VMK_DEVICE_INFO_OTHER))\

#define VMK_SCSI_DEVICE_STATE(name,description,longDesc,detailDesc) \
      /** \brief longDesc
       \details detailDesc */ name,
#define VMK_SCSI_DEVICE_STATE_NUM(name,value,description,longDesc,detailDesc) \
      /** \brief longDesc
       \details detailDesc */ name = value,
/** \endcond */

/**
 * \brief SCSI device states.
 */
typedef enum {
   VMK_SCSI_DEVICE_STATES
   VMK_SCSI_DEVICE_STATE_LAST
} vmk_ScsiDeviceState;

/** \cond nodoc */
#undef _VMK_SCSI_STRINGIFY
#undef VMK_SCSI_DEVICE_STATE
#undef VMK_SCSI_DEVICE_STATE_NUM
/** \endcond */

/** \cond nodoc */
#define VMK_SCSI_DEVICE_STATE_INFO_ALL \
   VMK_SCSI_DEVICE_STATE_INFO_NUM(VMK_SCSI_DEVICE_INFO_NONE, 0, "", \
                          "No additional device info.") \
   VMK_SCSI_DEVICE_STATE_INFO(VMK_SCSI_DEVICE_UNMAP, "unmap", \
                          "The device has been unmapped.") \
   VMK_SCSI_DEVICE_STATE_INFO(VMK_SCSI_DEVICE_INFO_HW_ERR, "hw error", \
                         "The device hit an unrecoverable hw_error.") \
   VMK_SCSI_DEVICE_STATE_INFO(VMK_SCSI_DEVICE_UUID_CHANGE, "uuid change", \
                         "The device uuid has changed.") \
   VMK_SCSI_DEVICE_STATE_INFO(VMK_SCSI_DEVICE_OTHER, "vendor specific", \
                         "The device is permanently lost.") \
   VMK_SCSI_DEVICE_STATE_INFO(VMK_SCSI_DEVICE_UNREGISTER, "unregister", \
                         "The device is being unregistered.") \

#define VMK_SCSI_DEVICE_STATE_INFO(name,description,longDesc) \
      /** \brief longDesc */ name,
#define VMK_SCSI_DEVICE_STATE_INFO_NUM(name,value,description,longDesc) \
      /** \brief longDesc */ name = value,
/** \endcond */

/**
 * \brief More info about the scsi device states.
 */
typedef enum {
   VMK_SCSI_DEVICE_STATE_INFO_ALL
   VMK_SCSI_DEVICE_STATE_INFO_LAST
} vmk_ScsiDeviceStateInfo;

/** \cond nodoc */
#undef VMK_SCSI_DEVICE_STATE_INFO
#undef VMK_SCSI_DEVICE_STATE_INFO_NUM
/** \endcond */

/*
 * Paths
 */

/**
 * \brief Maximum SCSI path name length.
 *
 * Path names are of the form "<adapter name>:C%u:T%u:L%u";
 * Their length is limited by SCSI_DISK_ID_LEN (44) because, absent
 * a better ID, the pathname may be used as ID on-disk.  This leaves
 * 11 bytes beyond VMK_MISC_NAME_MAX (32), which is enough
 * for "...:Cn:Tnn:Lnn" or "...:Cn:Tn:Ln    nn".
 */
#define VMK_SCSI_PATH_NAME_MAX_LEN 44

/** \cond nodoc */
#define VMK_SCSI_PATHSTATES \
   VMK_SCSI_PATH_STATE_NUM(VMK_SCSI_PATH_STATE_ON,   0,    "on")             \
   VMK_SCSI_PATH_STATE(VMK_SCSI_PATH_STATE_OFF,            "off")            \
   VMK_SCSI_PATH_STATE(VMK_SCSI_PATH_STATE_DEAD,           "dead")           \
   VMK_SCSI_PATH_STATE(VMK_SCSI_PATH_STATE_STANDBY,        "standby")        \
   VMK_SCSI_PATH_STATE(VMK_SCSI_PATH_STATE_DEVICE_CHANGED, "device_changed") \

#define VMK_SCSI_PATH_STATE(name,description) \
   /** \brief description */ name,
#define VMK_SCSI_PATH_STATE_NUM(name,value,description) \
   /** \brief description */ name = value,

/** \endcond */

/**
 * \brief State of a SCSI path.
 */
typedef enum {
   VMK_SCSI_PATHSTATES
   VMK_SCSI_PATH_STATE_LAST
} vmk_ScsiPathState;

/** \cond nodoc */
#undef VMK_SCSI_PATH_STATE
#undef VMK_SCSI_PATH_STATE_NUM
/** \endcond */

/*
 * Commands
 */

#define VMK_SCSI_MAX_CDB_LEN        16
#define VMK_SCSI_MAX_SENSE_DATA_LEN 64 

typedef enum {
   VMK_SCSI_COMMAND_DIRECTION_UNKNOWN,
   VMK_SCSI_COMMAND_DIRECTION_WRITE,
   VMK_SCSI_COMMAND_DIRECTION_READ,
   VMK_SCSI_COMMAND_DIRECTION_NONE,
} vmk_ScsiCommandDirection;

/**
 * \brief Adapter specific status for a SCSI command.
 * \note The vmk_ScsiHostStatus is a status value from the driver/hba. Most errors here
 * mean that the I/O was not issued to the target.
 */
typedef enum {
   /** \brief No error. */
   VMK_SCSI_HOST_OK          = 0x00,
   /** \brief The HBA could not reach the target.
    * \note ESX will try to switch to an alternate path upon receiving this error and
    * retry the I/O there.
    */
   VMK_SCSI_HOST_NO_CONNECT  = 0x01,
   /** \brief The SCSI BUS was busy. 
    * \note This error is most relevant for parallel SCSI devices because SCSI uses a
    * bus arbitration mechanism for multiple initiators. Newer transport protocols
    * are packetized and use switches, so this error does not occur here. However,
    * some drivers will return this in other unrelated error cases - if a connection
    * is temporarily lost for instance. ESX will retry on this error.
    */
   VMK_SCSI_HOST_BUS_BUSY    = 0x02,
   /** \brief Command timed out or was aborted. Not retried by ESX */
   VMK_SCSI_HOST_TIMEOUT     = 0x03,
   /** \brief The I/O was successfully aborted. Not retried by ESX */
   VMK_SCSI_HOST_ABORT       = 0x05,
   /** \brief A parity error was detected.
    * \note This error is most relevant to parallel SCSI devices  where the BUS uses 
    * a simple parity bit to check that transfers are not corrupted (it can detect 
    * only 1, 3,  5 and 7 bit errors). No retries will be done for this error by ESX.
    */
   VMK_SCSI_HOST_PARITY      = 0x06,
   /** \brief Generic error.
    * \note This is an error that the driver can return for
    * events not covered by other errors. For instance, drivers will return this
    * error in the event of a data overrun/underrun. There will be a limited amount
    * of retries on this error and otherwise the I/O is failed up to applications.
    */
   VMK_SCSI_HOST_ERROR       = 0x07,
   /** \brief Device was reset.
    * \note  This error  Indicates that the I/O was cleared from the HBA due to 
    * a BUS/target/LUN reset. Not retried by ESX.
    */
   VMK_SCSI_HOST_RESET       = 0x08,
   /** \brief Legacy error.
    * \note This error is not expected to be returned.  It was meant as
    * a way for drivers to return an I/O that has failed due to temporary conditions
    * in the driver and should be retried. Drivers should use VMK_SCSI_HOST_RETRY
    * instead.
    */
   VMK_SCSI_HOST_SOFT_ERROR  = 0x0b,
   /** \brief Request a retry of the I/O.
    * \note The driver requests that the I/O be requeued or immediately retried.
    */
   VMK_SCSI_HOST_RETRY       = 0x0c,
   /** \brief T10 PI GUARD tag check failed.
    * \note This error is used to indicate the storage stack that the T10 PI GUARD tag
    * check failed for the I/O. Drivers can use this code to indicate failed T10
    * PI Guard tag checks.
    */
   VMK_SCSI_HOST_PI_GUARD_ERROR = 0x0d,
   /** \brief T10 PI REF tag check failed.
    * \note This error is used to indicate the storage stack that the T10 PI REF tag
    * check failed for the I/O. Drivers can use this code to indicate failed T10
    * PI Ref tag checks.
    */
   VMK_SCSI_HOST_PI_REF_ERROR    = 0x0e,
   /* \brief T10 PI error.
    * \note This error is used to indicate the storage stack of any generic T10 PI
    * error for the I/O. Drivers can use this code to indicate failed generic T10
    * PI errors.
    */
   VMK_SCSI_HOST_PI_GENERIC_ERROR  = 0x0f,
   VMK_SCSI_HOST_MAX_ERRORS, /* Add all error codes before this. */
} vmk_ScsiHostStatus;

/**
 * \brief Device specific status for a SCSI command.
 * \note The vmk_ScsiDeviceStatus is the status reported by the target/LUN itself.  The
 * values are defined  in the SCSI specification. 
 */
typedef enum {
   VMK_SCSI_DEVICE_GOOD                       = 0x00,
   VMK_SCSI_DEVICE_CHECK_CONDITION            = 0x02,
   VMK_SCSI_DEVICE_CONDITION_MET              = 0x04,
   VMK_SCSI_DEVICE_BUSY                       = 0x08,
   VMK_SCSI_DEVICE_INTERMEDIATE               = 0x10,
   VMK_SCSI_DEVICE_INTERMEDIATE_CONDITION_MET = 0x14,
   VMK_SCSI_DEVICE_RESERVATION_CONFLICT       = 0x18,
   VMK_SCSI_DEVICE_COMMAND_TERMINATED         = 0x22,
   VMK_SCSI_DEVICE_QUEUE_FULL                 = 0x28,
   VMK_SCSI_DEVICE_ACA_ACTIVE                 = 0x30,
   VMK_SCSI_DEVICE_TASK_ABORTED               = 0x40,
} vmk_ScsiDeviceStatus;

/**
 * \brief Plugin specific status for a SCSI command.
 * \note The vmk_ScsiPluginStatus is a status value returned from the MP plugin that was 
 * processing the I/O cmd.  If an error is returned it means that the command could 
 * not be issued or needs to be retried etc. 
 */
typedef enum vmk_ScsiPluginStatus {
   /** \brief No error. */
   VMK_SCSI_PLUGIN_GOOD,
   /** \brief An unspecified error occurred. 
    * \note The I/O cmd should be retried. 
    */
   VMK_SCSI_PLUGIN_TRANSIENT,
   /** \brief The device is a deactivated snapshot. 
    * \note The I/O cmd failed because the device is a deactivated snapshot and so
    * the LUN is read-only. 
    */
   VMK_SCSI_PLUGIN_SNAPSHOT,
   /** \brief SCSI-2 reservation was lost. */
   VMK_SCSI_PLUGIN_RESERVATION_LOST,
   /** \brief The plugin wants to requeue the IO back
    * \note The IO will be retried. 
    */
   VMK_SCSI_PLUGIN_REQUEUE,
   /** 
    * \brief The test and set data in the ATS request returned false for
    * equality.
    */
   VMK_SCSI_PLUGIN_ATS_MISCOMPARE,
   /** 
    * \brief Allocating more thin provision space.
    * \note Device server is in the process of allocating more
    *       space in the backing pool for a thin provisioned LUN.
    */
   VMK_SCSI_PLUGIN_THINPROV_BUSY_GROWING,
   /** \brief Thin provisioning soft-limit exceeded. */
   VMK_SCSI_PLUGIN_THINPROV_ATQUOTA,
   /** \brief Backing pool for thin provisioned LUN is out of space. */
   VMK_SCSI_PLUGIN_THINPROV_NOSPACE,
} vmk_ScsiPluginStatus;

/**
 * \brief Status a for SCSI command.
 * \note The completion status for an I/O is a three-level hierarchy of
 * vmk_scsiPluginStatus, vmk_ScsiHostStatus, and vmk_ScsiDeviceStatus.
 *  - vmk_scsiPluginStatus is the highest level
 *  - vmk_scsiHostAtatus is the next level
 *  - vmk_scsiDeviceStatus is the lowest level
 *
 * An error reported at one level should not be considered valid if there is an
 * error reported by a higher level of the hierarchy. For instance, if
 * vmk_ScsiPluginStatus does not indicate an error and an error is indicated in
 * vmk_ScsiHostStatus, then the value of vmk_ScsiDeviceStatus is ignored.
 */
typedef struct vmk_ScsiCmdStatus {
   /** \brief Device specific command status.  
    * \note This is the lowest level error. 
    */
   vmk_ScsiDeviceStatus	device;
   /** \brief Adapter specific command status. */
   vmk_ScsiHostStatus	host;
   /** \brief Plugin specific command status. 
    *  \note This is the highest level error.
    */
   vmk_ScsiPluginStatus plugin;
} vmk_ScsiCmdStatus;

/* \cond nodoc */
#define VMK_SCSI_TASK_MGMT_TYPES(def)                                        \
   def(VMK_SCSI_TASKMGMT_ABORT, "abort", "Abort single command.") \
   def(VMK_SCSI_TASKMGMT_VIRT_RESET, "virt reset", \
       "Abort all commands sharing a unique originator ID" ) \
   def(VMK_SCSI_TASKMGMT_LUN_RESET, "lun reset", "Reset a LUN.") \
   def(VMK_SCSI_TASKMGMT_DEVICE_RESET, "target reset", "Reset a target.") \
   def(VMK_SCSI_TASKMGMT_BUS_RESET, "bus reset", "Reset the bus.") \

#define VMK_SCSI_DEF_TASK_MGMT_NAME(name, desc, longDesc) name,
#define VMK_SCSI_DEF_TASK_MGMT_DESC(name, desc, longDesc) desc,
#define VMK_SCSI_DEF_TASK_MGMT_NAME_WITH_COMMENT(name, desc, longDesc) \
   /** \brief longDesc */ name,
/* \endcond */

/**
 * \brief Task management types.
 */
typedef enum {
   VMK_SCSI_TASK_MGMT_TYPES(VMK_SCSI_DEF_TASK_MGMT_NAME_WITH_COMMENT)
   VMK_SCSI_TASKMGMT_LAST
} vmk_ScsiTaskMgmtType;

typedef struct vmk_ScsiIOCmdCountFields {
   vmk_uint32  rdCmds;
   vmk_uint32  wrCmds;
   vmk_uint32  otherCmds;
   vmk_uint32  totalCmds;
} vmk_ScsiIOCmdCountFields;

/**
 * \brief I/O Stats for Path and Adapter
 */
typedef struct vmk_ScsiIOCmdCounts {
   vmk_ScsiIOCmdCountFields  active;
   vmk_ScsiIOCmdCountFields  queued;
} vmk_ScsiIOCmdCounts;

#endif  /* _VMKAPI_SCSI_EXT_H_ */
/** @} */
/** @} */
