/***************************************************************************
 * Copyright 2004 - 2009 VMware, Inc.  All rights reserved.
 ***************************************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * PSP                                                            */ /**
 * \addtogroup Storage
 * @{
 * \defgroup PSP Path Selection Plugin API
 * @{
 ***********************************************************************
 */

#ifndef _VMK_NMP_PSP_H_
#define _VMK_NMP_PSP_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "nmp/vmkapi_nmp.h"

/** \cond never */
#define VMK_NMP_PSP_REVISION_MAJOR        1
#define VMK_NMP_PSP_REVISION_MINOR        0
#define VMK_NMP_PSP_REVISION_UPDATE       0
#define VMK_NMP_PSP_REVISION_PATCH_LEVEL  0

#define VMK_NMP_PSP_REVISION VMK_REVISION_NUMBER(VMK_NMP_PSP)
/** \endcond never */

typedef enum vmk_NmpPspStatelogFlag {
   VMK_NMP_PSP_STATELOG_GLOBALSTATE = 0x00000001
} vmk_NmpPspStatelogFlag;

/*
 * Max length of a psp name such as 'psp_rr'. The psp name can have a
 * maximum of VMK_NMP_PSP_NAME_MAX_LEN non-NULL characters and must be
 * terminated by a NULL character.
 */
#define VMK_NMP_PSP_NAME_MAX_LEN     39

/** Standard set of PSP operations */
typedef struct vmk_NmpPspOps {
   /*
    ***********************************************************************
    * alloc --                                                       */ /**
    *
    * \ingroup PSP
    *
    * \brief Allocate and setup PSP device private data for the device.
    *
    * This function is invoked when a PSP is first associated with a
    * device. If this function returns an error, NMP will fall back to
    * the default PSP.
    *
    * \note This entry point is allowed to block.
    *
    * \param[in] nmpDevice Device to associate with the PSP.
    *
    * \return Any error other than those listed here indicate that the
    *         PSP cannot support the device.
    * \retval  VMK_NO_MEMORY Allocation of private data failed. 
    *
    ***********************************************************************
    */
   VMK_ReturnStatus (*alloc)(vmk_NmpDevice *nmpDevice);
   /*
    ***********************************************************************
    * free --                                                        */ /**
    *
    * \ingroup PSP
    *
    * \brief Free device private data associated with the device.
    *
    * This function is invoked to disassociate a PSP from a given device.
    * This entry point will be called when NMP is removing a device from
    * this plugin, to free any private data previously allocated by the
    * PSP to manage the logical device. This is called while
    * disassociating a PSP, if an earlier call to PSP's \em alloc entry
    * point returned VMK_OK.
    *
    * \note This entry point is allowed to block.
    *
    * \param[in] nmpDevice Device to be freed.
    *
    * \return  This call should succeed with VMK_OK status.
    *
    ***********************************************************************
    */
   VMK_ReturnStatus (*free)(vmk_NmpDevice *nmpDevice);
   /*
    ***********************************************************************
    * claim --                                                       */ /**
    *
    * \ingroup PSP
    *
    * \brief Add a path to a given device.
    *
    * This function is invoked when a PSP has been associated with a
    * device and is invoked for all known paths to the given device.
    * This entry point is called after the \em alloc entry point has
    * succeeded.
    *
    * \note This entry point is allowed to block.
    *
    * \param[in] nmpDevice Device being targeted.
    * \param[in] vmkPath   A path to the device in question.
    *
    * \retval  VMK_BUSY       The number of paths already claimed by the 
    *                         device has reached the max number allowed.
    * \retval  VMK_NO_MEMORY  Path private data allocation fails.
    * \retval  VMK_FAILURE    On any other error.
    *
    ***********************************************************************
    */
   VMK_ReturnStatus (*claim)(vmk_NmpDevice *nmpDevice,
                             vmk_ScsiPath *vmkPath);
   /*
    ***********************************************************************
    * unclaim --                                                     */ /**
    *
    * \ingroup PSP
    *
    * \brief Try to unclaim a path on the device. 
    *
    * This function is invoked when a path is no longer associated with
    * the PSP or device in question, such as when the device is being
    * freed. This is called if a prior call to \em claim entry point
    * succeeded.
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
    * info --                                                        */ /**
    *
    * \ingroup PSP
    *
    * \brief Get a description of the plugin.
    *
    * This plugin entry point will be called when NMP tries to get general
    * information about the plugin. This would be the vendor, author,
    * version etc. This information should be returned in the
    * NULL-terminated text string \em buffer. The string cannot exceed
    * \em bufferLen.
    *
    * \note This function is not allowed to block.
    *
    * \param[out] buffer      A character string containing the plugin
    *                         info.
    * \param[in]  bufferLen   The maximum length of the output  buffer
    *                         parameter including NULL-termination.
    *
    * \return  This call should succeed with VMK_OK status.
    *
    ***********************************************************************
    */
   VMK_ReturnStatus (*info)(vmk_uint8 *buffer,
                            vmk_ByteCountSmall bufferLen);
   /*
    ***********************************************************************
    * setDeviceConfig --                                             */ /**
    *
    * \ingroup PSP
    *
    * \brief Set the device configuration.
    *
    * This entry point will be called when NMP tries to set per-device
    * configuration; if the plugin has some persistent device
    * configuration that can be set by the user, this is the entry point
    * to use.  The plugin is free to interpret the bufferLen contents of
    * buffer in any manner.
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
    * \retval  VMK_FAILURE  The config string was not as expected.
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
    * \ingroup PSP
    *
    * \brief Generate a string describing the state of a given device.
    *
    * This entry point will be called to generate a string of at most the
    * specified length (including a NULL terminating character) to
    * describe the state of the specified device.
    *
    * \note This entry point is optional.
    * \note This entry point is allowed to block.
    *
    * \param[in] nmpDevice          Device being targeted.
    * \param[out] configDataBuffer  A character string representing the
    *                               device configuration information.
    * \param[in]  bufferLen         The maximum length of the
    *                               configuration string (including a
    *                               terminating NULL).
    *
    * \retval  VMK_BUF_TOO_SMALL    Buffer is too small to contain full
    *                               configuration info.
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
    * \ingroup PSP
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
    * \param[in] bufferLen          The length of the buffer parameter in
    *                               bytes.
    *
    * \retval  VMK_FAILURE The config string was not as expected. Error
    *                      message is logged. 
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
    * \ingroup NMPManagePSP
    *
    * \brief Get the path configuration.
    *
    * This entry point will be called to generate a string of at most
    * \em bufferLen (including a NULL terminating character) to describe
    * the state of the specified path.
    *
    * \note This entry point is optional.
    * \note This entry point is allowed to block.
    *
    * \param[in]  nmpDevice         The device for the specified path.
    * \param[in]  vmkPath           The path whose configuration is to be
    *                               returned.
    * \param[out] configDataBuffer  A character string containing the
    *                               configuration info.
    * \param[in]  bufferLen         The maximum length of the
    *                               configuration string (including a
    *                               terminating NULL).
    *
    * \retval  VMK_BUF_TOO_SMALL    The buffer is too small to hold the
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
    * getWorkingSet --                                               */ /**
    *
    * \ingroup PSP
    *
    * \brief Get a list of paths the PS instance's selectPath
    *        entrypoint is likely to pick from.
    *
    * In case this entry is not set by the PS plugin then it is set to an
    * internal function that returns all active paths.
    *
    * \note This entry point is optional. 
    * \note This entry point is allowed to block.
    *
    * \param[in] nmpDevice       Device whose working set is being
    *                            queried.
    * \param[out] heapID         HeapID from which pathNames is allocated.
    * \param[out] numPathNames   Number of path names returned in
    *                            pathNames.
    * \param[out] pathNames      Pointer to an array of path names. It
    *                            is allocated memory from plugin's heap.
    *
    * \retval  VMK_NO_MEMORY     Memory allocation failed.
    *
    * \sideeffect Memory is allocated from heapID.
    *
    ***********************************************************************
    */
   VMK_ReturnStatus (*getWorkingSet)(vmk_NmpDevice *nmpDevice,
                                     vmk_HeapID *heapID,
                                     vmk_uint32 *numPathNames,
                                     char ***pathNames);
   /** \brief update device's path selection */
   /*
    ***********************************************************************
    * updatePathSelection --                                         */ /**
    *
    * \ingroup PSP
    *
    * \brief Resync with working physical paths to a particular device.
    *
    * Update path states in PSP's internal data structure. It may
    * also set a new current path for I/O. 
    *
    * \note This entry point is allowed to block.
    * 
    * \param[in]  nmpDevice   Device whose path states should be updated.
    *
    * \return  This call should succeed with VMK_OK status.
    *
    ***********************************************************************
    */
   VMK_ReturnStatus (*updatePathSelection)(vmk_NmpDevice *nmpDevice);
   /*
    ***********************************************************************
    * selectPath --                                                  */ /**
    *
    * \ingroup PSP
    * \brief Select a path to use for the command. 
    *
    * This call can return VMK_OK in which case the vmkPath argument has
    * to be updated with a valid path to which the IO will be issued.
    * Alternatively it can return VMK_NO_WORKING_PATHS to indicate that
    * there are no live paths to select from which will cause NMP to try
    * to activate some passive paths (if any) and try again later.
    * Lastly, the PSP can return VMK_SUSPEND_IO to request that the cmd
    * is temporarily queued in NMP until the PSP either releases it again
    * (by calling vmk_NmpPspResumeIO) or until it is aborted.
    *
    * \see vmk_NmpPspResumeIO
    *
    * \note This entry point is not allowed to block.
    *
    * \param[in]  nmpDevice   Device command is being issued on.
    * \param[in]  vmkCmd      The command being issued.
    * \param[out] vmkPath     The path to issue the command on (if any)
    *
    * \retval VMK_SUSPEND_IO       Suspend the IO till it is resumed.
    * \retval VMK_NO_WORKING_PATHS Indicates All Paths Down (APD)
    *                              condition.
    *
    ***********************************************************************
    */
   VMK_ReturnStatus  (*selectPath)(vmk_NmpDevice *nmpDevice,
                                   vmk_ScsiCommand *vmkCmd,
                                   vmk_ScsiPath **vmkPath);
   /*
    ***********************************************************************
    * selectPathToActivate --                                        */ /**
    *
    * \ingroup PSP
    *
    * \brief Select a path to activate for the device during failover.
    *
    * If NULL is returned, the SATP will determine the paths to activate.
    * If this function returns a non-null path then on subsequent calls
    * to \em selectPath this returned path will be selected.
    *
    * \note This entry point is allowed to block.
    *
    * \param[in]  nmpDevice   Device for which a path should be activated.
    *
    * \return The active scsi path to be used. 
    * \retval NULL   Let the SATP determine the new path to activate.
    *
    ***********************************************************************
    */
   vmk_ScsiPath  *(*selectPathToActivate)(vmk_NmpDevice *nmpDevice);
   /*
    ***********************************************************************
    * logState --                                                    */ /**
    *
    * \ingroup PSP
    *
    * \brief Dump a PSP's internal state.
    *
    * This plugin entry point will be called when NMP needs to log the
    * PSP's internal state. If a device is specified, only the state
    * associated with that device should be dumped. If \em nmpDevice is NULL,
    * the state of all devices should be dumped.\em logParam, if non-NULL,
    * is a string specifying the specific information to be logged.
    * \em logFlags further specifies what information should be dumped;
    * VMK_NMP_PSP_STATELOG_GLOBALSTATE is set to request dumping of the
    * PSP's global state instead of any device state.
    * 
    * \note This entry point is optional.
    * \note This entry point is allowed to block.
    *
    * \param[in] nmpDevice Device being targeted, or NULL.
    * \param[in] logParam  The information to be dumped, or NULL.
    * \param[in] logFlags  Any flags specifying the information to be
    *                      dumped; VMK_NMP_PSP_STATELOG_GLOBALSTATE
    *                      overrides any device specification.
    *
    * \return  This call should succeed with VMK_OK status. 
    *
    ***********************************************************************
    */
   VMK_ReturnStatus (*logState)(vmk_NmpDevice *nmpDevice,
                                const vmk_uint8 *logParam,
                                vmk_NmpPspStatelogFlag logFlags);
   /*
    ***********************************************************************
    * notifyDeviceEvent --                                           */ /**
    *
    * \ingroup PSP
    *
    * \brief Notify the PSP about a device being set off/on administratively.
    *
    *  NMP-PSP entry points can be called during/after the notification. 
    *
    * \note This entry point is optional. 
    *       If its not supported, then the value should be initialized to 
    *       NULL. If supported, this call must complete  successfully with a 
    *       return of VMK_OK.
    *
    * \note This call can block.
    *
    * \param[in]  device   Device being targeted.
    * \param[in]  event    Event thats occurring on this device.
    *
    * \retval VMK_OK       Success
    *
    ***********************************************************************
    */
   VMK_ReturnStatus (*notifyDeviceEvent)(vmk_NmpDevice *nmpDevice, 
                                         vmk_ScsiDeviceEvent event);

   /** \note reserved for future extension. */
   void (*reserved[3])(void);
} vmk_NmpPspOps;

/**
 * \brief PSP structure
 */
typedef struct vmk_NmpPspPlugin {
   /** \brief psp revision */
   vmk_revnum    pspRevision;
   /** \brief psp module id */
   vmk_ModuleID  moduleID;
   /** \brief psp heap id */
   vmk_HeapID    heapID;
   /** \brief psp's lock domain */
   vmk_LockDomainID lockDomain;
   /** \brief psp's operation entry points */
   vmk_NmpPspOps ops;
   /** \brief name of the psp plugin */
   char          name[VMK_NMP_PSP_NAME_MAX_LEN + 1];
} vmk_NmpPspPlugin;

/*
 ***********************************************************************
 * vmk_NmpPspAllocatePlugin --                                    */ /**
 *
 * \ingroup PSP
 *
 * \brief Allocate a PS Plugin.
 *
 * Allocate memory for \em vmk_NmpPspPlugin structure and initialize its 
 * contents.
 *
 * \note This is a non-blocking call.
 *
 * \return The newly allocated \em vmk_NmpPspPlugin structure.
 * \retval  NULL  Allocation failed.
 *
 ***********************************************************************
 */
vmk_NmpPspPlugin *vmk_NmpPspAllocatePlugin(void);

/*
 ***********************************************************************
 * vmk_NmpPspFreePlugin --                                        */ /**
 *
 * \ingroup PSP
 *
 * \brief Free a PS Plugin.
 *
 * Free \em pp allocated by a successful call to
 * vmk_NmpPspAllocatePlugin() earlier.
 *
 * \note This is a non-blocking call.
 *
 * \param[in] pp  PS Plugin to be freed.
 *
 ***********************************************************************
 */
void vmk_NmpPspFreePlugin(vmk_NmpPspPlugin *pp);

/*
 ***********************************************************************
 * vmk_NmpPspRegisterPlugin --                                    */ /**
 *
 * \ingroup PSP
 *
 * \brief Register a PS Plugin.
 *
 * Register the given PS Plugin with the ESX Native Multipathing Module
 * to allow logical devices to use this path selection functionality.
 * \em pp should have been allocated by vmk_NmpPspAllocatePlugin() prior
 * to this call.
 *
 * \note This is a non-blocking call.
 *
 * \param[in] pp  PS Plugin to register.
 *
 * \retval  VMK_FAILURE           PS plugin has not been allocated using
 *                                vmk_NmpPspAllocatePlugin() or
 *                                PS plugin has missing required entry
 *                                points.
 * \retval  VMK_NOT_FOUND         NMP module not found.
 * \retval  VMK_MODULE_NOT_LOADED pp->moduleID not loaded.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_NmpPspRegisterPlugin(vmk_NmpPspPlugin *pp);

/*
 ***********************************************************************
 * vmk_NmpPspUnregisterPlugin --                                  */ /**
 *
 * \ingroup PSP
 *
 * \brief Unregister a PS Plugin.
 *
 * After this call logical devices registered with ESX Native
 * Multipathing Module will not be able to use this path selection
 * functionality. \em pp should have been registered by calling
 * vmk_NmpPspRegisterPlugin() prior to this call. 
 *
 * \note This is a non-blocking call.
 *
 * \param[in] pp     PS Plugin to be unregsitered.
 *
 * \retval  VMK_FAILURE PS plugin could not be unregistered.
 *
 * \sideeffect May wait for a fixed amount of time for the refCount 
 *             of the PS plugin to drop to zero.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_NmpPspUnregisterPlugin(vmk_NmpPspPlugin *pp);

/*
 ***********************************************************************
 * vmk_NmpPspGetDevicePrivateData --                              */ /**
 *
 * \ingroup PSP
 *
 * \brief Return the pointer to the private data allocated by the
 *        plugin.
 * 
 * \note This is a non-blocking call.
 *
 * \note Spin locks can be held while calling into this function
 *
 * \param[in] nmpDev    NMP Device to query.
 *
 * \return The device private data structure.
 *
 ***********************************************************************
 */
void *vmk_NmpPspGetDevicePrivateData(vmk_NmpDevice *nmpDev);

/*
 ***********************************************************************
 * nmp_PspSetDevicePrivateData --                                */ /**
 *
 * \ingroup PSP
 *
 * \brief Store the pointer to the private data allocated by the
 *        plugin.
 *
 * \note This is a non-blocking call.
 *
 * \note Spin locks can be held while calling into this function
 *
 * \param[in] nmpDev          NMP Device to set private data pointer on.
 * \param[in] privateData     Private data pointer.
 *
 ***********************************************************************
 */
void
vmk_NmpPspSetDevicePrivateData(vmk_NmpDevice *nmpDev,
                               void *privateData);

/*
 ***********************************************************************
 * vmk_NmpPspGetPathPrivateData --                                */ /**
 *
 * \ingroup PSP
 *
 * \brief Return the pointer to the private data allocated by the
 *        plugin.
 * 
 * \note This is a non-blocking call.
 *
 * \note Spin locks can be held while calling into this function
 *
 * \param[in] scsiPath     Path to query.
 *
 * \return The path private data structure.
 *
 ***********************************************************************
 */

void *vmk_NmpPspGetPathPrivateData(vmk_ScsiPath *scsiPath);

/*
 ***********************************************************************
 * vmk_NmpPspSetPathPrivateData --                                   */ /**
 *
 * \ingroup PSP
 *
 * \brief Store the pointer to the private data allocated by the
 *        plugin.
 *
 * \note This is a non-blocking call.
 *
 * \note Spin locks can be held while calling into this function
 *
 * \param[in] scsiPath        Path to set private data pointer on.
 * \param[in] privateData     Private data pointer.
 *
 ***********************************************************************
 */

void
vmk_NmpPspSetPathPrivateData(vmk_ScsiPath *scsiPath,
                             void *privateData);

/*
 ***********************************************************************
 * vmk_NmpPspRequestPathStateUpdate--                             */ /**
 *
 * \ingroup PSP
 *
 * \brief Force the plugin to update path state for a device.
 *
 * \note This is a non-blocking call.
 *
 * \param[in] nmpDev          NMP Device on which to schedule a path
 *                            state update.
 *
 ***********************************************************************
 */
void vmk_NmpPspRequestPathStateUpdate(vmk_NmpDevice *nmpDev);

/*
 ***********************************************************************
 * vmk_NmpPspResumeIO--                                           */ /**
 *
 * \ingroup PSP
 *
 * \brief Resume an IO that was previously suspended by the PSP.
 *
 * This is used to resume an IO that was earlier requested to be
 * (temporarily) suspended from the PSPs 'selectPath' entry point.
 * If the IO has been aborted in the mean time (after it was suspended
 * but before it was resumed) the function will return VMK_ABORTED.
 *
 * \note This is a non-blocking call.
 *
 * \param[in] nmpDev        Device to resume command for.
 * \param[in] initiator     The initiator who issued the command.
 * \param[in] serialNumber  The serial number of the command to resume.
 *
 * \retval VMK_OK         Command was resumed and will be issued again.
 * \retval VMK_NOT_FOUND  Command never suspended or already resumed.
 * \retval VMK_ABORTED    Command was aborted from the suspend queue.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_NmpPspResumeIO(vmk_NmpDevice *nmpDev,
                   void *initiator,
                   vmk_uint64 serialNumber);

#endif /* _VMK_NMP_PSP_H_ */
/** @} */
/** @} */
