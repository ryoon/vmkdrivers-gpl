/**************************************************************************
 * Copyright 2004 - 2009 VMware, Inc.  All rights reserved.
 ***************************************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * NMP                                                            */ /**
 * \addtogroup Storage
 * @{
 * \defgroup SATP Storage Array Type Plugin
 * @{
 ***********************************************************************
 */

#ifndef _VMK_NMP_SATP_H_
#define _VMK_NMP_SATP_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "scsi/vmkapi_scsi.h"
#include "nmp/vmkapi_nmp.h"

/** \cond never */
#define VMK_NMP_SATP_REVISION_MAJOR       1
#define VMK_NMP_SATP_REVISION_MINOR       0
#define VMK_NMP_SATP_REVISION_UPDATE      0
#define VMK_NMP_SATP_REVISION_PATCH_LEVEL 0

#define VMK_NMP_SATP_REVISION         VMK_REVISION_NUMBER(VMK_NMP_SATP)
/** \endcond never */

/** \brief Max length of a SATP name, including terminating nul. */
#define VMK_NMP_SATP_NAME_MAX_LEN         39

/** \brief Max length of a SATP configuration data. */
#define VMK_NMP_SATP_CONFIG_MAX_LEN       100

/** \brief Max length of a SATP descriptor string. */
#define VMK_NMP_SATP_DESCRIPTION_MAX_LEN  256

typedef enum vmk_NmpSatpStatelogFlag {
   VMK_NMP_SATP_STATELOG_GLOBALSTATE = 0x00000001
} vmk_NmpSatpStatelogFlag;

/** \brief NMP device attributes. */
typedef enum vmk_NmpSatpBoolAttribute {
   /**
    * \brief Unknown/invalid attribute.
    */
   VMK_NMP_SATP_BOOL_ATTR_UNKNOWN      = 0x0,
   /**
    * \brief Device is a pseudo/management device.
    *
    * This attribute will be used when NMP needs to determine if the
    * given device is a management LUN/pseudo device/LUNZ etc. If a
    * a LUN is marked as a management LUN it will not be usable by
    * the ESX system.
    */
   VMK_NMP_SATP_BOOL_ATTR_MANAGEMENT   = 0x1,
   /**
    * \brief Device is an SSD device.
    *
    * This attribute will be used  when NMP needs to determine
    * if the given device is an SSD Device.
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
    */
   VMK_NMP_SATP_BOOL_ATTR_SSD   = 0x2,
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
   VMK_NMP_SATP_BOOL_ATTR_LOCAL = 0x3,
   VMK_NMP_SATP_BOOL_ATTR_MAX   = 0x4  /* Add all the boolean attributes
                                          before this */
} vmk_NmpSatpBoolAttribute;

/** Standard set of SATP operations */
typedef struct vmk_NmpSatpOps {
   /*
    ***********************************************************************
    * uid --                                                         */ /**
    *
    * \ingroup SATP
    *
    * \brief Get the uid associated with the device on the given path.
    *
    * This entry point returns the uid for the device to which
    * \em vmkPath belongs. The SATP cannot assume that a path has been
    * claimed before this entry point is called. This means that the
    * \em uid call cannot rely on the path having the perPathPrivateData
    * pointer set. If the SATP cannot return a UID then this function
    * should return an error and NMP will use the physical path name as
    * the UID. The SATP may invoke the vmk_ScsiGetPathInquiry() API or the 
    * vmk_ScsiGetCachedPathStandardUID() API.
    *
    * \note This is an optional entry point.
    * \note This entry point is allowed to block.
    *
    * \param[in] vmkPath The path for the device in question
    * \param[out] uid    A pointer to the vmk_ScsiUid to be filled in.
    *
    * \retval VMK_FAILURE        An unspecified error occured.
    * \retval VMK_BAD_PARAM      An invalid parameter was supplied to the
    *                            routine like a NULL uid argument.
    * \retval VMK_NO_MEMORY      Some local temporary allocation failed.
    * \retval VMK_NOT_SUPPORTED  Path Inquiry failed.
    *
    ***********************************************************************
   */
   VMK_ReturnStatus (*uid)(vmk_ScsiPath *vmkPath,
                           vmk_ScsiUid *uid);
   /*
    ***********************************************************************
    * alloc --                                                       */ /**
    *
    * \ingroup SATP
    *
    * \brief Allocate the per-device private data for a given device.
    *
    * This function is invoked when a SATP is first associated with a
    * device. If this function returns an error then NMP may choose another
    * SATP for the device, or fall back to the default SATP.
    *
    * \note This entry point is allowed to block.
    *
    * \param[in] nmpDevice Device to associate with the SATP.
    *
    * \retval VMK_BUSY        Device private data already allocated i.e
    *                         device has already been added to this plugin.
    * \retval VMK_NO_MEMORY   Allocation failed.
    *
    ***********************************************************************
    */
   VMK_ReturnStatus (*alloc)(vmk_NmpDevice *nmpDevice);
   /*
    ***********************************************************************
    * free --                                                        */ /**
    *
    * \ingroup SATP
    *
    * \brief Free the SATP's private data for a given device.
    *
    * This function is invoked to disassociate a SATP from a given device.
    * This entry point will be called by NMP to free any private data
    * previously allocated by the SATP to manage the logical device. The
    * SATP \em alloc entry point should have returned VMK_OK. 
    *
    * \note This entry point is allowed to block.
    *
    * \param[in] nmpDevice Device to be freed.
    *
    * \retval VMK_FAILURE  Given device's private data was found
    *                      to be NULL.
    *
    ***********************************************************************
    */
   VMK_ReturnStatus (*free)(vmk_NmpDevice *nmpDevice);
   /*
    ***********************************************************************
    * claim --                                                       */ /**
    *
    * \ingroup SATP
    *
    * \brief Add a path to a given device.
    *
    * This function is invoked when a SATP has been associated with a
    * device and is invoked for all known paths to the given device. This
    * entry point is called after the \em alloc entry point has been
    * invoked. Per path private data may be maintained by the SATP that is
    * allocated while claiming the path.
    *
    * \note This entry point is allowed to block.
    *
    * \param[in] nmpDevice Device being targeted.
    * \param[in] vmkPath A path to the device in question.
    *
    * \retval VMK_NO_MEMORY      Allocation of the path private data
    *                            failed.
    * \retval VMK_FAILURE        Initialization of some field failed. A
    *                            warning is logged. 
    * \retval VMK_NOT_SUPPORTED  vmk_ScsiGetPathInquiry returned an error.
    *                            Error message logged.
    *
    ***********************************************************************
    */
   VMK_ReturnStatus (*claim)(vmk_NmpDevice *nmpDevice,
                             vmk_ScsiPath *vmkPath);
   /*
    ***********************************************************************
    * unclaim --                                                     */ /**
    *
    * \ingroup SATP
    *
    * \brief Remove a path from a given device.
    *
    * This function is invoked when a path is no longer associated with
    * the SATP or device in question, such as when the device is being
    * freed. The per path private data allocated by the \em claim entry
    * point is freed.
    *
    * \note This entry point is allowed to block.
    *
    * \param[in] nmpDevice Device being targeted.
    * \param[in] vmkPath   The path to be detached from the device in
    *                      question.
    *
    ***********************************************************************
    */
   void (*unclaim)(vmk_NmpDevice *nmpDevice,
                   vmk_ScsiPath *vmkPath);
   /*
    ***********************************************************************
    * updatePathStates --                                            */ /**
    *
    * \ingroup SATP
    *
    * \brief Update the current state of all paths for a given device.
    *
    * This function is invoked to update the status of all paths to a
    * device; it is invoked periodically as well as during possible path
    * failover. This routine should probe each of the paths to the device,
    * determine the current state of the path and move it to the
    * correct path group. 
    *
    * \note This call may block if the SATP issues an inquiry command.
    *
    * \param[in] nmpDevice       Paths of this device are updated. 
    * \param[out] statesUpdated  A flag to be set to indicate whether or
    *                            not any paths changed state.
    * 
    * \retval VMK_BUSY     Path update already in progress.
    *
    ***********************************************************************
    */
   VMK_ReturnStatus (*updatePathStates)(vmk_NmpDevice *nmpDevice,
                                        vmk_Bool *statesUpdated);
   /*
    ***********************************************************************
    * activatePaths --                                               */ /**
    *
    * \ingroup SATP
    *
    * \brief Activate at least one path to a given device.
    *
    * This plugin entry point will be called when NMP has determined that
    * some paths to the device are no longer working.  The SATP should
    * take whatever actions necessary to get some paths to work. This
    * may include sending commands along \em vmkpath.
    * If the \em vmkPath parameter is set, the plugin should try to make
    * that path work. If no path is specified, or the specified path cannot
    * be activated then the SATP should activate another path of it's own
    * choice.
    * 
    * \note The state of the paths for the device are not updated here.
    * \note This call may block.
    *
    * \param[in] nmpDevice       Device being targeted.
    * \param[in] vmkPath         The path to try activating first.
    * \param[out] pathsActivated Set to VMK_TRUE if and only if
    *                            activation was attempted and was
    *                            successful.
    *
    * \retval  VMK_NO_CONNECT               Path is in DEAD state.
    * \retval  VMK_NOT_SUPPORTED            Path is unmapped from the
    *                                       array.
    * \retval  VMK_NOT_READY                Path is in STANDBY state.
    * \retval  VMK_TIMEOUT                  Command timed out.
    * \retval  VMK_STORAGE_RETRY_OPERATION  Transient error.
    * \retval  VMK_BUSY                     Path activation already in
    *                                       progress.
    * \retval  VMK_FAILURE                  Tried to activate an OFF path
    *                                       or failed to activate any
    *                                       path. Error message logged.
    * 
    ***********************************************************************
    */
   VMK_ReturnStatus (*activatePaths)(vmk_NmpDevice *nmpDevice,
                                     vmk_ScsiPath *vmkPath,
                                     vmk_Bool *pathsActivated);
   /*
    ***********************************************************************
    * notify --                                                      */ /**
    *
    * \ingroup SATP
    *
    * \brief Invoked when an I/O command other than READ/WRITE is
    *        about to be issued.
    *
    * This plugin entry point will be called by NMP before the given I/O
    * is issued to the device.
    * The \em cmd->cdb[0] field can be examined to determine which SCSI
    * command is being issued.  By setting the "done" and "doneData"
    * fields in the \em vmk_ScsiCommand structure, the plugin can be
    * notified when the command completes. In case of failure it is
    * expected that the SATP will call the \em cmd completion routine. A
    * command must not be completed in the I/O initiation code path but
    * should be scheduled for completion from a different context.
    *
    * \note This is an optional entry point.
    * \note This entry point should not block as it is invoked in the
    *       command issue path.
    *
    * \param[in] nmpDevice Device being targeted.
    * \param[in] cmd       The command about to be issued.
    *
    * \retval VMK_OK      The I/O issue should proceed.
    * \retval VMK_FAILURE Indicates the SATP has posted a completion
    *                     and I/O should be aborted.
    *
    ***********************************************************************
    */
   VMK_ReturnStatus (*notify)(vmk_NmpDevice *nmpDevice,
                              vmk_ScsiCommand *cmd);
   /*
    ***********************************************************************
    * notifyRW --                                                    */ /**
    *
    * \ingroup SATP
    *
    * \brief Invoked when READ/WRITE I/O command is about to be issued.
    *
    * This plugin entry point will be called by NMP before the given I/O
    * is issued to the device. The \em cmd->cdb[0] field can be examined to
    * determine the SCSI command being issued.  By setting the "done" and
    * "doneData" fields in the \em vmk_ScsiCommand structure, the plugin can
    * be notified when the command completes. In case of failure it is
    * expected that the SATP will call the \em cmd completion routine. A
    * command must not be completed in the I/O initiation code path but
    * should be scheduled for completion from a different context.
    *
    * \note This is an optional entry point.
    * \note This should not block as invoked in the command issue path.
    *
    * \param[in] nmpDevice Device being targeted.
    * \param[in] cmd       The READ/WRITE command about to be issued.
    *
    * \retval VMK_OK      The I/O issue should proceed.
    * \retval VMK_FAILURE Indicates the SATP has posted a completion
    *                     and of the I/O should be aborted.
    *
    ***********************************************************************
    */
   VMK_ReturnStatus (*notifyRW)(vmk_NmpDevice *nmpDevice,
                                vmk_ScsiCommand *cmd);
   /*
    ***********************************************************************
    * getBoolAttr --                                                 */ /**
    *
    * \ingroup SATP
    *
    * \brief Determine the device's attribute.
    *
    * Plugin can use this entrypoint to indicate devices that match the
    * characteristics of the attribute.
    *
    * If this is omitted, NMP will invoke vmk_ScsiDefaultNmpSatpGetBoolAttr()
    * to auto-detect local/SSD devices.
    *
    * \see vmk_ScsiDefaultNmpSatpGetBoolAttr()
    *
    * \note This entry point is allowed to block.
    *
    * \param[in]  nmpDevice  Device being targeted.
    * \param[in]  attr       vmk_NmpSatpBoolAttribute to probe for.
    * \param[out] boolAttr   Flag indicating if device supports the attribute.
    *
    * \return VMK_ReturnStatus
    * \retval VMK_OK         If the device attribute is set.
    * \retval VMK_BAD_PARAM  Invalid input device or attribute.
    *
    ***********************************************************************
    */
   VMK_ReturnStatus (*getBoolAttr)(vmk_NmpDevice *nmpDevice,
                                   vmk_NmpSatpBoolAttribute attr,
                                   vmk_Bool *boolAttr);
   /*
    ***********************************************************************
    * pathFailure --                                                 */ /**
    *
    * \ingroup SATP
    *
    * \brief Determine if the given SCSI command failure should be retried.
    *
    * This plugin entry point will be called when NMP needs to determine if
    * the given I/O failure was caused by a path failure. If the command
    * failure was due to a path failure then the command will be retried,
    * possibly on a different path, after NMP has called the
    * \em activatePaths entry point.
    *
    * \note This entry point is optional.
    * \note This entry point should not block as it may be called from the
    *       command issue path.
    *
    *
    * \param[in] nmpDevice    The device the command was sent to.
    * \param[in] vmkPath      The specific path this I/O was issued on.
    * \param[in] cmd          The command that failed.
    * \param[in] status       The status of the failed command.
    * \param[in] senseBuffer  The senseBuffer data, if any, of the failed
    *                         command.
    *
    * \retval  VMK_OK        Command did not fail due to path failure.
    * \retval  VMK_NOT_READY Command failed due to a path failure.
    *                        This will cause the I/O to be retried.
    * \retval  VMK_FAILURE   How the command failed could not be
    *                        determined i.e  there is no valid sense
    *                        data. This will not cause the I/O to be
    *                        retried.
    *
    ***********************************************************************
    */
   VMK_ReturnStatus (*pathFailure)(vmk_NmpDevice *nmpDevice,
                                   vmk_ScsiPath *vmkPath,
                                   vmk_ScsiCommand *cmd,
                                   vmk_ScsiCmdStatus status,
                                   vmk_ScsiSenseData *senseBuffer);
   /*
    ***********************************************************************
    * info --                                                        */ /**
    *
    * \ingroup SATP
    *
    * \brief Get a description of the plugin.
    *
    * This plugin entry point will be called when NMP tries to get general
    * information about the plugin. This would be the vendor, author,
    * version etc. This information should be returned in the
    * (NULL-terminated) text string \em buffer. This string cannot exceed
    * \em bufferLen.
    *
    * \note This function is not allowed to block.
    *
    * \param[out] buffer      A character string containing the plugin
    *                         info.
    * \param[in]  bufferLen   The maximum length of the output  buffer
    *                         parameter including NULL-termination.
    *
    * \return This call should succeed with VMK_OK status.
    *
    ***********************************************************************
    */
   VMK_ReturnStatus (*info)(vmk_uint8 *buffer,
                            vmk_ByteCountSmall bufferLen);
   /*
    ***********************************************************************
    * setDeviceConfig --                                             */ /**
    *
    * \ingroup SATP
    *
    * \brief Set the device configuration.
    *
    * This entry point will be called when NMP tries to set per-device
    * configuration; if the plugin has some persistent device configuration
    * that can be set by the user, this is the entry point to use. The
    * plugin is free to interpret the bufferLen contents of buffer in any
    * manner.
    *
    * \note This entry point is optional.
    * \note This entry point is allowed to block.
    *
    * \param[in] nmpDevice          Device being configured.
    * \param[in] configDataBuffer   A character string containing the
    *                               device configuration info.
    * \param[in] bufferLen          The length of the buffer parameter in
    *                               bytes.
    *
    * \retval VMK_FAILURE  The config string was not as expected.
    *
    ***********************************************************************
    */
   VMK_ReturnStatus (*setDeviceConfig)(vmk_NmpDevice *nmpDevice,
                                       vmk_uint8 *configDataBuffer,
                                       vmk_ByteCountSmall bufferLen);
   /*
    ***********************************************************************
    * getDeviceConfig --                                             */ /**
    *
    * \ingroup SATP
    *
    * \brief Generate a string describing the state of a given device.
    *
    * This entry point will be called to generate a string of at most the
    * specified length (including a NULL terminating character) that
    * describes the state of the specified device.
    *
    * \note This entry point is optional. 
    * \note This entry point is allowed to block.
    *
    * \param[in] nmpDevice          Device being targeted.
    * \param[out] configDataBuffer  A character string representing the
    *                               device configuration information.
    * \param[in]  bufferLen         The maximum length of the configuration
    *                               string (including a terminating NULL).
    *
    * \retval VMK_BUF_TOO_SMALL     The buffer is too small to hold the
    *                               path configuration string.
    *
    ***********************************************************************
    */
   VMK_ReturnStatus (*getDeviceConfig)(vmk_NmpDevice *nmpDevice,
                                       vmk_uint8 *configDataBuffer,
                                       vmk_ByteCountSmall bufferLen);
   /*
    ***********************************************************************
    * setPathConfig --                                               */ /**
    *
    * \ingroup SATP
    *
    * \brief Set the path configuration.
    *
    * This entry point will be called when NMP tries to set per-path
    * configuration; if the plugin has some path persistent path
    * configuration that can be set by the user, this is the entry point
    * to use.  The plugin is free to interpret the bufferLen contents of
    * buffer in any manner.
    *
    * \note This entry point is optional.
    * \note This entry point is allowed to block.
    *
    * \param[in] nmpDevice          Device whose path is being configured.
    * \param[in] vmkPath            The path to be configured.
    * \param[in] configDataBuffer   A character string containing the
    *                               path configuration info.
    * \param[in] bufferLen          The length of the buffer parameter
    *                               in bytes.
    *
    * \retval  VMK_FAILURE The config string was not as expected.
    *
    ***********************************************************************
    */
   VMK_ReturnStatus (*setPathConfig)(vmk_NmpDevice *nmpDevice,
                                     vmk_ScsiPath *vmkPath,
                                     vmk_uint8 *configDataBuffer,
                                     vmk_ByteCountSmall bufferLen);
   /*
    ***********************************************************************
    * getPathConfig --                                               */ /**
    *
    * \ingroup SATP
    *
    * \brief Get the path configuration.
    *
    * This entry point will be called to generate a string of at most the
    * specified length (including a NULL terminating character) to
    * describe the state of the specified path.
    *
    * \note This entry point is optional.
    * \note This entry point is allowed to block.
    *
    * \param[in]  nmpDevice         The device for the specified path.
    * \param[in]  vmkPath           The path whose configuration is to
    *                               be returned.
    * \param[out] configDataBuffer  A character string containing the
    *                               configuration info.
    * \param[in]  bufferLen         The maximum length of the
    *                               configuration string (including a
    *                               terminating NULL).
    *
    * \retval VMK_BUF_TOO_SMALL     The buffer is too small to hold the
    *                               path configuration string.
    *
    ***********************************************************************
    */
   VMK_ReturnStatus (*getPathConfig)(vmk_NmpDevice *nmpDevice,
                                     vmk_ScsiPath *vmkPath,
                                     vmk_uint8 *configDataBuffer,
                                     vmk_ByteCountSmall bufferLen);
   /*
    ***********************************************************************
    * logState --                                                    */ /**
    *
    * \ingroup SATP
    *
    * \brief Dump a SATP's internal state.
    *
    * This entry point will be called when NMP needs to log the SATP's
    * internal state.  If a device is specified, only the state associated
    * with that device should be dumped.  If \em nmpDevice is NULL, the state
    * of all devices should be dumped.  The \em logParam argument, if non-NULL,
    * is a string specifying the specific information to be logged.
    * \em logFlags further specifies what information should be dumped;
    * VMK_NMP_SATP_STATELOG_GLOBALSTATE is set to request dumping of the
    * SATP's global state instead of any device state.
    *
    * \note This is an optional entry point.
    * \note This entry point is allowed to block.
    *
    * \param[in] nmpDevice Device being targeted, or NULL.
    * \param[in] logParam  The information to be dumped, or NULL.
    * \param[in] logFlags  Any flags specifying the information to be
    *                      dumped; VMK_NMP_SATP_STATELOG_GLOBALSTATE
    *                      overrides any device specification.
    *
    * \retval VMK_FAILURE  Failed to log.
    *
    ***********************************************************************
    */
   VMK_ReturnStatus (*logState)(vmk_NmpDevice *nmpDevice,
                                const vmk_uint8 *logParam,
                                vmk_NmpSatpStatelogFlag logFlags);
   /*
    ***********************************************************************
    * checkPath --                                                   */ /**
    *
    * \ingroup SATP
    *
    * \brief Determine the state of a given path.
    *
    * This entry point can be called in the context of NMP helper routines
    * that can be invoked by the SATPs to determine the state of a given
    * path.  If this entry point is NULL then NMP sets it to
    * vmk_NmpSatpIssueTUR().
    *
    * \note This is an optional entry point.
    * \note This is a blocking call.
    *
    * \param[in] nmpDevice       Device being targeted.
    * \param[in] vmkPath         The path whose state is to be determined.
    * \param[in] TURTimeoutMs    The timeout (in mSec.) for the TUR
    *                            command.
    *
    * \return The status of the path encoded in VMK_ReturnStatus. Any error
    *         not enumerated below indicates a failure to issue the
    *         command.
    * \retval  VMK_OK                       Path is in ON state.
    * \retval  VMK_NO_CONNECT               Path is in DEAD state.
    * \retval  VMK_PERM_DEV_LOSS            Path has been removed from the
    *                                       array, its UID has changed or
    *                                       its hit a permanent unrecoverable
    *                                       h/w failure.
    * \retval  VMK_NOT_READY                Path is in STANDBY state.
    * \retval  VMK_RESERVATION_CONFLICT     SCSI reservation conflict.
    * \retval  VMK_STORAGE_RETRY_OPERATION  Transient conditions.
    * \retval  VMK_FAILURE                  Sense data extraction failed or
    *                                       a device check condition status
    *                                       of other than "not ready" was
    *                                       returned.
    *
    ***********************************************************************
    */
   VMK_ReturnStatus (*checkPath)(vmk_NmpDevice *nmpDevice, 
                                 vmk_ScsiPath *vmkPath,
                                 vmk_uint32 TURTimeoutMs);
   /*
    ***********************************************************************
    * notReady --                                                    */ /**
    *
    * \ingroup SATP
    *
    * \brief Determine if the given key, ASC, and ASCQ indicates a
    *        not ready path.
    *
    * This entry point can be used to determine if the given
    * key/asc/ascq indicates a not ready path. If this entry point is not
    * supplied then the vmk_NmpSatpNotReady() routine will be used to
    * determined the meaning of the key/asc/ascq.
    *
    * \note This entry point is optional.
    * \note This entry point is allowed to block.   
    *
    * \param[in] sense_key      The SCSI check condition key.
    * \param[in] asc parameter  The SCSI check condition ASC value.
    * \param[in] ascq parameter The SCSI check condition ASCQ value.
    *
    * \retval  VMK_TRUE  Sense data indicates not ready.
    * \retval  VMK_FALSE Sense data does not indicate a not ready path or 
    *                    indicates that the whole device is not ready.
    *
    ***********************************************************************
    */
   vmk_Bool (*notReady)(vmk_uint8 sense_key,
                        vmk_uint8 asc,
                        vmk_uint8 ascq);
   /*
    ***********************************************************************
    * startPath --                                                   */ /**
    *
    * \ingroup SATP
    *
    * \brief Activate a path for use.
    *
    * This entry point is invoked when NMP needs to activate a path. If
    * this entry point is not supplied, then
    * vmk_NmpSatpIssueStartUnitCommand() routine will be used to activate
    * the path.
    *
    * \note This call may block.
    *
    * \param[in] nmpDevice          Device being targeted.
    * \param[in] vmkPath            The path to be activated.
    * \param[in] startUnitTimeoutMs The timeout (in mSec.) for the start
    *                               operation.
    *
    * \return No errors are expected but if encountered they indicate that
    *         the command failed. An appropriate error message is logged.
    * \retval  VMK_OK   The specified path was successfully activated.
    *
    ***********************************************************************
    */
   VMK_ReturnStatus (*startPath)(vmk_NmpDevice *nmpDevice, 
                                 vmk_ScsiPath *vmkPath,
                                 vmk_uint32 startUnitTimeoutMs);
   /*
    ***********************************************************************
    * notifyDeviceEvent --                                           */ /**
    *
    * \ingroup SATP
    *
    * \brief Notify the MPP about a device being set off/on administratively.
    *  
    * The notify, notifyRW entry points should not be called by NMP after 
    * this notification. However all other entry points can be called. 
    *
    * \note  This entry point is optional. If its not supported, then it 
    *        must be initialized to NULL.  If  supported, this call must 
    *        complete successfully with a return of VMK_OK.
    *
    * \note This call may block.
    *
    * \param[in]  device   Device being targeted.
    * \param[in]  event    Event thats occurring on the device.
    *
    * \retval VMK_OK        Only value supported so far.
    *
    ***********************************************************************
    */
   VMK_ReturnStatus (*notifyDeviceEvent)(vmk_NmpDevice *nmpDevice, 
                                         vmk_ScsiDeviceEvent event);

} vmk_NmpSatpOps;

/**
 * \brief SATP structure
 */
typedef struct vmk_NmpSatpPlugin {
   /** \brief satp revision */
   vmk_revnum   satpRevision;
   /** \brief satp module id */
   vmk_ModuleID moduleID;
   /** \brief satp heap id */
   vmk_HeapID   heapID;
   /** \brief satp's lock domain */
   vmk_LockDomainID lockDomain;
   /** \brief satp's operation entry points */
   vmk_NmpSatpOps ops;
   /** \brief satp name */
   char          name[VMK_NMP_SATP_NAME_MAX_LEN + 1];
} vmk_NmpSatpPlugin;

/*
 ***********************************************************************
 * vmk_NmpSatpDefaultGetBoolAttr                                  */ /**
 *
 * \ingroup NMP_SATP_API
 *
 * \brief Default attribute satp entrypoint.
 *
 * \param[in]  nmpDevice  Device to target.
 * \param[in]  attr       Device attribute to probe for.
 * \param[out] boolAttr   Flag indicating if device supports the attribute.
 *                        Valid only if the return status is VMK_OK.
 *
 * \note This function may block
 *
 * \return VMK_ReturnStatus
 * \retval VMK_OK         Device attribute status obtained successfully.
 * \retval VMK_BAD_PARAM  Invalid input device or attribute.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_NmpSatpDefaultGetBoolAttr(vmk_NmpDevice *nmpDevice,
                              vmk_NmpSatpBoolAttribute attr,
                              vmk_Bool *boolAttr);

/*
 ***********************************************************************
 * vmk_NmpSatpAllocatePlugin --                                   */ /**
 *
 * \ingroup SATP
 *
 * \brief Allocate a SAT Plugin structure.
 *
 * Allocate memory for a \em vmk_NmpSatpPlugin structure and initialize
 * the fields. This function is invoked by SATPs when they are loaded.
 *
 * \note This is a non-blocking call.
 *
 * \return The allocated SAT plugin struture.
 * \retval  NULL  Memory could not be allocated.
 *
 ***********************************************************************
 */
vmk_NmpSatpPlugin *vmk_NmpSatpAllocatePlugin(void);

/*
 ***********************************************************************
 * vmk_NmpSatpFreePlugin --                                       */ /**
 *
 * \ingroup SATP
 *
 * \brief Free a SAT Plugin structure.
 *
 * This is called when unloading a SATP module; if plugin was allocated
 * by vmk_NmpSatpAllocatePlugin() earlier, successfully.
 *
 * \note This is a non-blocking call.
 *
 * \param[in] plugin    SATP plugin to be freed.
 *
 ***********************************************************************
 */
void vmk_NmpSatpFreePlugin(vmk_NmpSatpPlugin *plugin);

/*
 ************************************************************************
 * vmk_NmpSatpRegisterPlugin --                                    */ /**
 *
 * \ingroup SATP
 *
 * \brief Register a SATP.
 * 
 * Make the given SAT Plugin known to the ESX Native Multipathing
 * Plugin so that it can be used to manage the access to the physical
 * paths to storage media. This is called after plugin has been allocated
 * by vmk_NmpSatpAllocatePlugin() successfully.
 *
 * \note This is a non-blocking call.
 *
 * \param[in] plugin    SATP to be registered.
 *
 * \retval  VMK_FAILURE   Plugin structure not allocated by 
 *                        vmk_NmpSatpAllocatePlugin() or some of the
 *                        required entry points are missing or the
 *                        plugin's revision major number does not match
 *                        the revision number
 *                        (VMK_NMP_SATP_REVISION_MAJOR) expected by NMP.
 * \retval  VMK_BAD_PARAM Bad plugin values like invalid plugin name. 
 *
 ************************************************************************
 */
VMK_ReturnStatus vmk_NmpSatpRegisterPlugin(vmk_NmpSatpPlugin *plugin);

/*
 ***********************************************************************
 * vmk_NmpSatpUnregisterPlugin --                                 */ /**
 *
 * \ingroup SATP
 *
 * \brief Unregister a SAT Plugin.
 *
 * Invoked while unloading the module and should be followed by a call
 * to vmk_NmpSatpFreePlugin(). \em plugin should have been registered by
 * vmk_NmpSatpRegisterPlugin() successfully.
 *
 * \note This is a non-blocking call.
 *
 * \param[in] plugin     SAT Plugin to be unregistered.
 *
 * \retval  VMK_FAILURE  SAT plugin has not been registered using
 *                       vmk_NmpSatpRegisterPlugin().
 * 
 * \sideeffect May wait for a certain amount of time for the plugin
 *             refCount to drop to zero.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_NmpSatpUnregisterPlugin(vmk_NmpSatpPlugin *plugin);

/*
 ***********************************************************************
 * vmk_NmpSatpGetSupportedIDOptions --                            */ /**
 *
 * \ingroup SATP
 *
 * \brief Return the "options" configuration.
 *
 * Return a text string containing the "options" configuration 
 * information for the SAT Plugin, for a given device.
 *
 * \note This is a non-blocking call.
 *
 * \note Spin locks can be held while calling into this function
 *
 * \param[in]  nmpDevice   Device to get supported id options.
 * \param[out] options     Options on a NMP device.
 * \param[in]  size        Size of the options buffer in bytes.
 *
 ***********************************************************************
 */
void vmk_NmpSatpGetSupportedIDOptions(
   vmk_NmpDevice *nmpDevice,
   vmk_uint8  *options,
   vmk_ByteCountSmall size);

/*
 ***********************************************************************
 * vmk_NmpSatpGetDevicePrivateData --                             */ /**
 *
 * \ingroup SATP
 *
 * \brief Return the pointer to the device private data allocated by
 *        the plugin.
 *
 * \note This is a non-blocking call.
 *
 * \note Spin locks can be held while calling into this function
 *
 * \param[in] nmpDevice    Device from which to get plugin's private
 *                         data.
 *
 * \return  The device private data for nmpDevice.
 *
 ***********************************************************************
 */
void *vmk_NmpSatpGetDevicePrivateData(vmk_NmpDevice *nmpDevice);

/*
 ***********************************************************************
 * vmk_NmpSatpSetDevicePrivateData --                             */ /**
 *
 * \ingroup SATP
 *
 * \brief Store the pointer to the device private data allocated by
 *        the plugin.
 *
 * \note This is a non-blocking call.
 *
 * \note Spin locks can be held while calling into this function
 *
 * \param[in] nmpDevice      Device to set plugin private data.
 * \param[in] privateData    Data to set.
 *
 ***********************************************************************
 */
void vmk_NmpSatpSetDevicePrivateData(
   vmk_NmpDevice *nmpDevice, 
   void *privateData);

/*
 ***********************************************************************
 * vmk_NmpSatpGetPathPrivateData --                               */ /**
 *
 * \ingroup SATP
 *
 * \brief Return the pointer to the path private data allocated by 
 *        the plugin.
 *
 * \note This is a non-blocking call.
 *
 * \note Spin locks can be held while calling into this function
 *
 * \param[in] scsiPath     Path to get plugin private data.
 *
 * \return  The path private data for scsiPath.
 *
 ***********************************************************************
 */
void *vmk_NmpSatpGetPathPrivateData(vmk_ScsiPath *scsiPath);

/*
 ***********************************************************************
 * vmk_NmpSatpSetPathPrivateData --                               */ /**
 *
 * \ingroup SATP
 *
 * \brief Store the pointer to the path private data allocated by the 
 *        plugin.
 *
 * \note This is a non-blocking call.
 *
 * \note Spin locks can be held while calling into this function
 *
 * \param[in] scsiPath     Path to set plugin private data.
 * \param[in] privateData  Data to set.
 *
 ************************************************************************
 */
void vmk_NmpSatpSetPathPrivateData(
   vmk_ScsiPath *scsiPath,
   void *privateData);

/*
 ***********************************************************************
 * vmk_NmpSatpIssueTUR --                                         */ /**
 *
 * \ingroup SATP
 *
 * \brief Determines what state a path is in by issuing a potentially
 *        vendor-specific probe to the array.
 *
 * This entry will default to a function that issues a TUR and
 * determines the path state from the result of the TUR.
 *
 * \note This is a blocking call.
 *
 * \param[in] nmpDev       NMP Device.
 * \param[in] scsiPath     Path to issue TUR.
 * \param[in] timeoutMs    Timeout in milliseconds, 0 for default.
 *
 * \retval  VMK_OK                      Path is in ON state.
 * \retval  VMK_NO_CONNECT              Path is in DEAD state.
 * \retval  VMK_PERM_DEV_LOSS           Path UID has changed.
 * \retval  VMK_NOT_READY               Path is in STANDBY state.
 * \retval  VMK_RESERVATION_CONFLICT    SCSI reservation conflict.
 * \retval  VMK_STORAGE_RETRY_OPERATION Transient conditions.
 * \retval  VMK_FAILURE                 Sense data extraction failed or
 *                                      check condition device status of
 *                                      other than "not ready"
 *                                      condition.
 *
 ************************************************************************
 */
VMK_ReturnStatus vmk_NmpSatpIssueTUR(
   vmk_NmpDevice *nmpDev,
   vmk_ScsiPath *scsiPath,
   vmk_uint32 timeoutMs);


/*
 ***********************************************************************
 * vmk_NmpSatpIssueStartUnitCommand --                            */ /**
 *
 * \ingroup SATP
 *
 * \brief Helper routine to send a start stop unit down a path.
 *
 * This function may be used by SATPs when trying to activate a path.
 * 
 * \note This is a blocking call.
 *
 * \param[in] nmpDev    NMP device.
 * \param[in] vmkPath   Physical path.
 * \param[in] timeoutMs Start Stop Unit timeout in Ms.
 *
 * \retval  VMK_NO_CONNECT       Path is in DEAD state.
 * \retval  VMK_NO_MEMORY        Some internal memory allocation failed.
 * \retval  VMK_TIMEOUT          Command timed out.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_NmpSatpIssueStartUnitCommand (vmk_NmpDevice *nmpDev, 
                                vmk_ScsiPath *vmkPath,
                                vmk_uint32 timeoutMs);
/*
 ***********************************************************************
 * vmk_NmpSatpUnrecHWError  --                                   */ /**
 *
 * \ingroup NMP_SATP_API
 * \brief  Determine if sense data indicates an unrecoverable h/w
 *         error.
 *
 * \param[in] sk   SCSI check condition key
 * \param[in] asc  SCSI check condition ASC value
 * \param[in] ascq SCSI check condition ASCQ value
 *
 * \note This is a non-blocking call.
 *
 * \note Spin locks can be held while calling into this function
 *
 * \retval VMK_TRUE  sense data indicates an unrecoverable h/w error
 * \retval VMK_FALSE sense data does not indicate an unrecoverable
 *                   h/w error.
 ***********************************************************************
 */
vmk_Bool
vmk_NmpSatpUnrecHWError (vmk_uint8 sk,
   vmk_uint8 asc,
   vmk_uint8 ascq);

/*
 ***********************************************************************
 * vmk_NmpSatpLUNotSupported --                                   */ /**
 *
 * \ingroup NMP_SATP_API
 *
 * \brief  Determine if sense data indicates an unmapped LUN condition.
 *
 * This routine is used to determine if the status indicated by the
 * given key/asc/ascq values indicates an unmapped LUN condition.
 * This may be called by the SATPs to determine the status of a
 * completion of a command or from the \em pathFailure entry point.
 *
 * \note This is a non-blocking call.
 *
 * \note Spin locks can be held while calling into this function
 *
 * \param[in] sk     SCSI check condition key.
 * \param[in] asc    SCSI check condition ASC value.
 * \param[in] ascq   SCSI check condition ASCQ value.
 *
 * \retval VMK_TRUE  Sense data indicates an unmapped LUN condition.
 * \retval VMK_FALSE Sense data does not indicate an unmapped LUN
 *                   condition.
 *
 ***********************************************************************
 */
vmk_Bool
vmk_NmpSatpLUNotSupported(
   vmk_uint8 sk,
   vmk_uint8 asc,
   vmk_uint8 ascq);

/*
 ***********************************************************************
 * vmk_NmpSatpNotReady --                                         */ /**
 *
 * \ingroup NMP_SATP_API
 *
 * \brief  Determine if sense data indicates a not ready path condition.
 *
 * This routine is used to determine the status indicated by the
 * given key/asc/ascq values.
 * This may be called by the SATPs to determine the status of a
 * completion of a command or from the \em pathFailure entry point.
 *
 * \note Device not ready conditions are ignored.
 *
 * \note This is a non-blocking call.
 *
 * \note Spin locks can be held while calling into this function
 *
 * \param[in] sk     SCSI check condition key.
 * \param[in] asc    SCSI check condition ASC value.
 * \param[in] ascq   SCSI check condition ASCQ value.
 *
 * \retval VMK_TRUE  Sense data indicates not ready.
 * \retval VMK_FALSE Sense data does not indicate a not ready path or 
 *                   indicates that the whole device is not ready.
 *
 ***********************************************************************
 */
vmk_Bool vmk_NmpSatpNotReady(
   vmk_uint8 sk,
   vmk_uint8 asc,
   vmk_uint8 ascq);

/*
 ***********************************************************************
 * vmk_NmpSatpAANotReady --                                       */ /**
 *
 * \ingroup NMP_SATP_API
 *
 * \brief  Determine if sense data indicates a not ready path condition
 *         for an Active-Active array.
 * 
 * This routine is used to determine the status indicated by the given
 * key/asc/ascq values.
 * This may be called by the SATPs to determine the status of
 * completion of a command or from the \em pathFailure entry point.
 *
 * \note This is a non-blocking call.
 *
 * \note Spin locks can be held while calling into this function
 *
 * \param[in] sk     SCSI check condition key.
 * \param[in] asc    SCSI check condition ASC value.
 * \param[in] ascq   SCSI check condition ASCQ value.
 *
 * \retval VMK_TRUE  Sense data indicates not ready.
 * \retval VMK_FALSE Sense data does not indicate a not ready path or 
 *                   indicates that the whole device is not ready.
 *
 ***********************************************************************
 */
vmk_Bool vmk_NmpSatpAANotReady(
   vmk_uint8 sk,
   vmk_uint8 asc,
   vmk_uint8 ascq);

/*
 ***********************************************************************
 * vmk_NmpVerifyPathUID                                           */ /**
 *
 * \ingroup  NMP_SATP_API
 *
 * \brief If necessary verify path UID.
 *
 * SATPs may invoke this API when path states are updated. Verify paths
 * UID is required whenever path state transitions from DEAD or OFF
 * state since a different device could show up.
 *
 * \note This call may block while issuing I/O to physical device.
 *
 * \param[in] scsiPath  Path to verify UID.
 * \param[in] newState  New state that the path is about to
 *                      transition to.
 *
 * \return Original state if UID did not change, or no check was done.
 *
 * \retval  VMK_NMP_PATH_GROUP_STATE_PERM_LOSS Path UID has changed.
 * \retval  VMK_NMP_PATH_GROUP_STATE_UNAVAILABLE No connection to target 
 *
 ***********************************************************************
 */
vmk_NmpPathGroupState   vmk_NmpVerifyPathUID(
   vmk_ScsiPath *scsiPath, 
   vmk_NmpPathGroupState newState);


/*
 ***********************************************************************
 * vmk_NmpSatpAPUpdatePathStatesHelper--                          */ /**
 *
 * \ingroup NMP_SATP_API
 *
 * \brief   Helper routine to update the SATP states for active/passive
 *          devices.
 *
 * SATPs for active/passive devices have the option of using this
 * interface to update Path states. The algorithm is as follows:
 * - For each path to the device
 * - If the path is not VMK_SCSI_PATH_STATE_OFF
 *     - If the SATP has a \em checkPath routine invoke it.
 *     - Else issue a TUR to check the path state.
 *     - Move the path to the appropriate path group.
 *     .
 * .
 *
 * \note This is a blocking call.
 *
 * \param[in]  nmpDevice      NMP device.
 * \param[in]  TURtimeoutMs   Timeout for TUR in milliseconds.
 * \param[out] statesUpdated  Indicates if paths got updated.
 *
 * \retval     VMK_FAILURE    Routine was not called in the
 *                            updatePathStates context.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_NmpSatpAPUpdatePathStatesHelper (
   vmk_NmpDevice *nmpDevice,
   vmk_uint32     TURtimeoutMs,
   vmk_Bool      *statesUpdated);

/*
 ***********************************************************************
 * vmk_NmpSatpAAUpdatePathStatesHelper--                          */ /**
 *
 * \ingroup NMP_SATP_API
 *
 * \brief   Helper routine to update the SATP states for active/active
 *          devices.
 *
 * SATPs for active/active devices have the option of using this
 * interface to update Path states. The algorithm is as follows:
 * - For each path to the device
 * - If the path is not VMK_SCSI_PATH_STATE_OFF
 *     - If the SATP has a \em checkPath routine invoke it.
 *     - Else issue a TUR to check the path state.
 *     - Move the path to the appropriate path group.
 *     .
 * .
 *
 * \note This is a blocking call.
 *
 * \param[in]  nmpDevice      NMP device.
 * \param[in]  TURtimeoutMs   TUR timeout in milliseconds.
 * \param[out] statesUpdated  Indicates if any paths got updated.
 *
 * \retval    VMK_FAILURE     Routine was not called in the
 *                            updatePathStates context.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_NmpSatpAAUpdatePathStatesHelper (
   vmk_NmpDevice *nmpDevice,
   vmk_uint32 TURtimeoutMs, 
   vmk_Bool *statesUpdated);


/*
 ***********************************************************************
 * vmk_NmpSatpAPActivatePathsHelper --                            */ /**
 *
 * \ingroup NMP_SATP_API
 *
 * \brief   Helper routine to update the SATP states for active/passive
 *          devices.
 *
 * SATPs for active/passive devices have the option of using this
 * routine to activate the paths. The algorithm is as follows:
 * - If a path is not specified for activation, then this routine will
 *   get an active path from the active group. If an active path is not
 *   available, this routine will get a standby path. This path will
 *   then be activated.
 * - To activate a path:
 *   - Check if the path is working by invoking the SATP \em checkPath
 *     entry point.If \em checkPath is not supplied a TUR is issued to
 *     see if the path is working.
 *   - If the path is not ready, then activate this path. If the SATP
 *     has provided an \em startPath entry point, this is invoked. If
 *     there is no entry point, then a start-stop unit is issued.
 *   - Verify path activation by calling the \em checkPath or TUR. 
 *
 * \note This is a blocking call.
 * 
 * \param[in]  nmpDevice               NMP device.
 * \param[in]  vmkPath                 Physical path.
 * \param[in]  TURtimeoutMs            TUR timeout in milliseocnds.
 * \param[in]  starStopUnitTimeoutMs   Start Stop Unit timeout in
 *                                     milliseconds.
 * \param[out] pathsActivated          Were paths activated?
 *
 * \retval  VMK_OK                       Path was already in ACTIVE
 *                                       state or successfully
 *                                       activated.
 * \retval  VMK_FAILURE                  Path is in OFF state or path is
 *                                       in non-ready state.
 * \retval  VMK_NO_CONNECT               Path is in DEAD state.
 * \retval  VMK_NOT_SUPPORTED            Path has been removed from the
 *                                       array or its UID has changed.
 * \retval  VMK_NOT_READY                Path is in STANDBY state and 
 *                                       could not be put in ACTIVE
 *                                       state.
 * \retval  VMK_RESERVATION_CONFLICT     SCSI reservation conflict.
 * \retval  VMK_STORAGE_RETRY_OPERATION  Transient error encountered.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_NmpSatpAPActivatePathsHelper ( 
   vmk_NmpDevice *nmpDevice,
   vmk_ScsiPath  *vmkPath, 
   vmk_uint32 TURtimeoutMs,
   vmk_uint32 starStopUnitTimeoutMs, 
   vmk_Bool *pathsActivated);

/*
 ************************************************************************
 *  vmk_NmpSatpSetProbeFailuresOptionFlag --                       */ /**
 *
 *  \ingroup NMP_SATP_API
 *  \brief Sets the option to handle retry failures for probe commands.
 *
 *  Sets the SATP's preference on handling continuous RETRY failures
 *  for probe sync commands. If set then the path will be marked dead
 *  after a timeout else the path state will not be updated. If a SATP
 *  uses NMP's default probing mechanism via
 *  vmk_NmpSatpAPUpdatePathStatesHelper or
 *  vmk_NmpSatpAAUpdatePathStatesHelper then this API must be called
 *  from the SATP when the config option 'enable_action_OnRetryErrors'
 *  or 'disable_action_OnRetryErrors' is set.
 *
 *  \param[in] nmpDevice   NMP device
 *  \param[in] flag        VMK_TRUE indicates option is ON
 *                         VMK_FALSE indicates option is OFF
 *
 ************************************************************************
 */
void
vmk_NmpSatpSetProbeFailuresOptionFlag(vmk_NmpDevice *nmpDevice, vmk_Bool flag);

#endif /* _VMK_NMP_SATP_H_ */
/** @} */
/** @} */
