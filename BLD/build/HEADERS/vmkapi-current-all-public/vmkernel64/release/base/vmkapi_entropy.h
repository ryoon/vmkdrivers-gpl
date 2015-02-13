/* **********************************************************
 * Copyright 1998 - 2009 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Entropy                                                        */ /**
 * \addtogroup Core
 * @{
 * \defgroup Entropy Entropy
 *
 * These interfaces to deal with entropy in vmkernel.
 *
 * Two types of interfaces are provided:
 * \li To provide various types of entropy directly to the VMkernel
 *     and device drivers (as opposed to via reads to /dev/{,u}random).
 * \li To allow VMkernel and device drivers to submit hardware entropy.
 *
 * @{
 ***********************************************************************
 */


#ifndef _VMKAPI_ENTROPY_H_
#define _VMKAPI_ENTROPY_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

typedef enum {
   VMK_ENTROPY_HARDWARE_INTERRUPT = 0,
   VMK_ENTROPY_HARDWARE_RNG = 1,
   VMK_ENTROPY_KEYBOARD = 2,
   VMK_ENTROPY_MOUSE = 3,
   VMK_ENTROPY_OTHER_HID = 4,
   VMK_ENTROPY_STORAGE = 5,
   VMK_ENTROPY_SOURCE_LIMIT = VMK_ENTROPY_STORAGE
} vmk_EntropySource;

typedef enum {
   VMK_ENTROPY_HARDWARE = 0,
   VMK_ENTROPY_HARDWARE_NON_BLOCKING = 1,
   VMK_ENTROPY_SOFTWARE = 2,
   VMK_ENTROPY_SOFTWARE_ONLY = 3,
   VMK_ENTROPY_TYPE_LIMIT = VMK_ENTROPY_SOFTWARE_ONLY
} vmk_EntropyType;

/*
 ***********************************************************************
 *
 * Entropy Modules export two types of interfaces:
 *
 *
 *
 ***********************************************************************
 */

/*
 ***********************************************************************
 * vmk_AddEntropyFunction --                                      */ /**
 *
 * \ingroup Entropy
 * \brief Submit hardware entropy to the entropy driver.
 *
 * \note  Callbacks of this type may not block.
 *
 * vmk_AddEntropyFunctions transfer hardware entropy to an entropy module
 * and are guaranteed to be non-blocking and have minimal overhead.
 * There is no return status because for security reasons any failure is
 * cloaked from all callers who lack privilege to know entropy status

 * \param[in] typeSpecificInfo  Source specific info (example: interrupt vector)
 *
 * \retval none; caller has no need to know success/failure and cannot be told.
 *
 ***********************************************************************
 */
typedef void (*vmk_AddEntropyFunction)(int sourceSpecificInfo);

/*
 ***********************************************************************
 * vmk_GetEntropyFunction --                                      */ /**
 *
 * \ingroup Entropy
 * \brief Retrieve entropy from the entropy driver.
 *
 * \note  Callbacks of this type may not block.
 *
 * vmk_GetEntropyFunctions return various types of entropy:
 *    VMK_ENTROPY_HARDWARE always returns VMK_OK but may block
 *    VMK_ENTROPY_HARDWARE_NON_BLOCKING may return VMK_FAILURE but wont block
 *    VMK_ENTROPY_SOFTWARE always returns VMK_OK and wont block
 *    VMK_ENTROPY_SOFTWARE_ONLY always returns VMK_OK and wont block
 *
 *    Note: SOFTWARE entropy may include (and thus consume) HARDWARE
 *          entropy but SOFTWARE_ONLY entropy is guaranteed not to.
 *
 * \param[in,out]  entropy          Buffer to store retrieved entropy
 * \param[in]     bytesRequested    Amount of entropy requested
 * \param[out]    bytesReturned     Amount of entropy actually returned
 *
 * \retval VMK_OK All requested bytes of entropy returned
 * \retval VMK_FAILURE Less than all requested bytes of entropy returned
 *
 ***********************************************************************
 */
typedef VMK_ReturnStatus (*vmk_GetEntropyFunction)(void *entropy,
						   int   bytesRequested,
						   int  *bytesReturned);

/*
 ***********************************************************************
 * vmk_RegisterAddEntropyFunction --                              */ /**
 *
 * \ingroup Entropy
 * \brief Register a function to be used to submit hardware entropy to the
 *        entropy driver.
 *
 * \note  This function will not block.
 *
 * \param[in] moduleID Module ID of the entropy module
 * \param[in] function Function to call when various hardware events occur
 *                 (e.g., hardware interrupts)
 * \param[in] source   Type of hardware entropy
 *
 * \retval VMK_OK Add entropy function was sucessfully registered.
 * \retval VMK_FAILURE Add entropy function was not registered.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RegisterAddEntropyFunction(
    vmk_ModuleID           moduleID,
    vmk_AddEntropyFunction function,
    vmk_EntropySource      source);

/*
 ***********************************************************************
 * vmk_RegisterGetEntropyFunction --                              */ /**
 *
 * \ingroup Entropy
 * \brief Register an entropy function to be used to obtain hardware
 *        or software entropy from an external entropy pool.
 *
 * \note  This function will not block.
 *
 * \param[in] moduleID Module ID of the entropy module
 * \param[in] function Function that returns entropy
 * \param[in] type     Type of entropy (e.g., hardware)
 *
 * \retval VMK_OK Entropy function was sucessfully registered.
 * \retval VMK_FAILURE Entropy function was not registered.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_RegisterGetEntropyFunction(
    vmk_ModuleID           moduleID,
    vmk_GetEntropyFunction function,
    vmk_EntropyType        type);

/*
 ***********************************************************************
 * vmk_GetRegisteredGetEntropyFunction --                           */ /**
 *
 * \ingroup Entropy
 * \brief Get a registered get entropy function to be used to obtain hardware
 *        or software entropy from an external entropy pool.  Note that this
 *        function must be called with VMKMODCALL
 *
 * \note  This function will not block.
 *
 * \param[out] *moduleID Module ID of the entropy module providing the function
 * \param[out] *function Function that returns entropy
 * \param[in]   type     Type of entropy desired (e.g., hardware)
 *
 * \retval VMK_OK Entropy function was sucessfully found and returned.
 * \retval VMK_FAILURE Entropy function was not found.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_GetRegisteredEntropyFunction(
    vmk_ModuleID           *moduleID,
    vmk_GetEntropyFunction *function,
    vmk_EntropyType        type);

/*
 ***********************************************************************
 * vmk_UnregisterAddEntropyFunction --                            */ /**
 *
 * \ingroup Entropy
 * \brief Unregister a submit entropy function (removes for all sources
 *        of entropy)
 *
 * \note  This function will not block.
 *
 * \param[in] moduleID Module ID of the entropy module
 * \param[in] function Function to unregister
 *
 * \retval VMK_OK Add entropy function was sucessfully unregistered.
 * \retval VMK_FAILURE Add entropy function was not unregistered.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_UnregisterAddEntropyFunction(
    vmk_ModuleID           moduleID,
    vmk_AddEntropyFunction function);

/*
 ***********************************************************************
 * vmk_UnregisterGetEntropyFunction --                               */ /**
 *
 * \ingroup Entropy
 * \brief Unregister an entropy function (removes for all types of entropy)
 *
 * \note  This function will not block.
 *
 * \param[in] moduleID Module ID of the entropy module
 * \param[in] function Function that returns entropy
 *
 * \retval VMK_OK Entropy function was sucessfully unregistered.
 * \retval VMK_FAILURE Entropy function was not unregistered.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_UnregisterGetEntropyFunction(
    vmk_ModuleID           moduleID,
    vmk_GetEntropyFunction function);

#endif /* _VMKAPI_ENTROPY_H_ */
/** @} */
/** @} */
