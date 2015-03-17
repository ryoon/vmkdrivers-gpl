/* **********************************************************
 * Copyright 2012 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Character Devices                                              */ /**
 * \addtogroup CharDev 
 *
 * Interfaces that allow registration of generic vmkernel character
 * device nodes.
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_CHAR_LEGACY_H_
#define _VMKAPI_CHAR_LEGACY_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/**
 * \brief Opaque handle to a character device.
 */
typedef struct vmkCharDevInt* vmk_CharDev;

/**
 * \brief A default initialization value for a vmk_CharDev.
 */
#define VMK_INVALID_CHARDEV (NULL)

/**
 ***********************************************************************
 * vmk_CharDevCleanupFn --                                        */ /**
 *
 * \brief Prototype for a character device driver's cleanup callback.
 *
 * \param[in]  private  Optional private data to be used by the callback
 *
 * \retval VMK_OK The cleanup function executed correctly.
 *                This is not an indicator of the success or failure of
 *                the operations in the function, but merely that they
 *                ran.  Any other return value is not allowed.
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_CharDevCleanupFn)(vmk_AddrCookie private);

/*
 ***********************************************************************
 * vmk_CharDevRegister --                                         */ /**
 *
 * \brief Register the specified character device, to be invoked from
 *        user-space.
 *
 * \nativedriversdisallowed
 *
 * \param[in]  module         Module that owns the character device.
 * \param[in]  name           The name of the device - this must be unique.
 * \param[in]  fileOps        Table of the driver file operations.
 *                            Neither open nor close can be supplied 
 *                            without the other.
 *                            If read or write operations are supplied, 
 *                            then open and close must also be supplied.
 * \param[in]  cleanup        Function automatically invoked to clean up
 *                            after all file ops have ceased and the 
 *                            device has been unregistered.  May be NULL.
 * \param[in]  devicePrivate  Data given to the driver for each file 
 *                            op and cleaned up after unregistration.
 * \param[out] assignedHandle Handle to the registered character device.
 *
 * \retval VMK_EXISTS         A device with that name is already registered
 * \retval VMK_FAILURE        Unable to allocate internal slot for the device
 * \retval VMK_NO_MEMORY      Unable to allocate memory for device metadata
 * \retval VMK_BAD_PARAM      Module ID was invalid, name was invalid,
 *                            or one or more specified driver ops are NULL
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_CharDevRegister(
   vmk_ModuleID module,
   const char *name,
   const vmk_CharDevOps *fileOps,
   vmk_CharDevCleanupFn cleanup,
   vmk_AddrCookie devicePrivate,
   vmk_CharDev *assignedHandle);

/*
 ***********************************************************************
 * vmk_CharDevUnregister --                                       */ /**
 *
 * \brief Unregister a character device.
 *
 * The character device will be unregistered automatically by
 * the kernel only after all open files to the device have been
 * closed.  If no files are open when vmk_CharDevUnregister is
 * called, the device may be unregistered immediately and have the
 * cleanup function registered with it invoked.  If the device has 
 * files open, vmk_CharDevUnregister internally defers the device for 
 * later automatic removal and returns to the caller immediately.  When 
 * the last file is closed, the device will then be destroyed and the 
 * cleanup function invoked.
 * 
 * \nativedriversdisallowed
 * \note No new open files to the device can be created after calling
 *       vmk_CharDevUnregister.
 * \note The vmkernel will prevent a module from being unloaded while
 *       it has open files associated with a character device, even
 *       if that device has been requested to be unregistered.
 *
 * \param[in] deviceHandle Handle of device assigned during registration.
 *
 * \retval VMK_NOT_FOUND The device does not exist.
 * \retval VMK_OK The device was either unregistered or internally
 *                deferred for unregistration once all associated files
 *                close.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_CharDevUnregister(vmk_CharDev deviceHandle);

#endif /* _VMKAPI_CHAR_LEGACY_H_ */
/** @} */
