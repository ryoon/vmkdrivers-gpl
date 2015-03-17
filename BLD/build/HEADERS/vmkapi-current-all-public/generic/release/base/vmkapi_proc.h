/* **********************************************************
 * Copyright 1998 - 2009 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Proc                                                           */ /**
 * \defgroup Proc ProcFS Emulation Interfaces
 *
 * Interfaces relating to the procfs emulation
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_PROC_H_
#define _VMKAPI_PROC_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/**
 * \brief Top-level parent nodess
 */
typedef enum vmk_ProcEntryParent {
   VMK_PROC_PRIVATE = -1,
   VMK_PROC_ROOT = 0,
   VMK_PROC_ROOT_DRIVER,
   VMK_PROC_ROOT_NET,
   VMK_PROC_ROOT_SCSI,
   VMK_PROC_ROOT_BUS,
   VMK_PROC_MAX_PREDEF,
} vmk_ProcEntryParent;

/** Max length of proc name including terminating nul. */
#define VMK_PROC_MAX_NAME	64

/** Max length that can be read during a proc read */
#define VMK_PROC_READ_MAX       (128 * 1024)

typedef struct vmk_ProcEntryInt vmk_ProcEntryInt, *vmk_ProcEntry;

typedef int (*vmk_ProcRead)(vmk_ProcEntry vpe, char *buffer, int *len);
typedef int (*vmk_ProcWrite)(vmk_ProcEntry vpe, char *buffer, int *len);

/*
 ***********************************************************************
 * vmk_ProcEntryCreate --                                         */ /**
 *
 * \ingroup Proc
 * \brief Allocate and initialize proc entry structure
 *
 * \note Requires that the module heap be initialized.
 *
 * \param[out] vpe Pointer to a new vmk proc entry is returned.
 * \param[in] moduleID Module ID who manages the created proc entry.
 * \param[in] name Name of the proc entry, 0 terminated string.
 * \param[in] isDir Specify if this proc entry is directory or not.
 *
 * \retval VMK_OK vmk proc entry was successfully initialized
 * \retval VMK_NO_MEMORY vmk proc entry could not be allocated
 * \retval VMK_NO_MODULE_HEAP The module has no heap to allocate from
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_ProcEntryCreate(
   vmk_ProcEntry *vpe,
   vmk_ModuleID moduleID,
   const char *name,
   vmk_Bool isDir);

/*
 ***********************************************************************
 * vmk_ProcEntryDestroy --                                        */ /**
 *
 * \ingroup Proc
 * \brief Frees the proc entry structure
 *
 * \param[in] vpe Pointer to the vmk proc entry to be freed.
 *
 ***********************************************************************
 */
void vmk_ProcEntryDestroy(
   vmk_ProcEntry vpe);

/*
 ***********************************************************************
 * vmk_ProcEntrySetup --                                          */ /**
 *
 * \ingroup Proc
 * \brief Sets up the proc entry structure
 *
 * \param[in] vpe Pointer to the vmk proc entry to setup.
 * \param[in] parent Pointer to the parent vmk proc entry.
 * \param[in] read Read handler for this proc entry.
 * \param[in] write Write handler for this proc entry.
 * \param[in] canBlock Specify if user need to block.
 * \param[in] private Private data for this proc entry.
 *
 ***********************************************************************
 */
void vmk_ProcEntrySetup(
   vmk_ProcEntry vpe,
   vmk_ProcEntry parent,
   vmk_ProcRead read,
   vmk_ProcWrite write,
   vmk_Bool canBlock,
   void *private);

/*
 ***********************************************************************
 * vmk_ProcEntryGetPrivate --                                     */ /**
 *
 * \ingroup Proc
 * \brief Returns private data of a proc entry
 *
 * \param[in] vpe Pointer to the vmk proc entry.
 *
 * \retval Pointer to the private data
 *
 ***********************************************************************
 */
void* vmk_ProcEntryGetPrivate(
   vmk_ProcEntry vpe);

/*
 ***********************************************************************
 * vmk_ProcEntryGetGUID --                                        */ /**
 *
 * \ingroup Proc
 * \brief Returns UUID of a proc entry
 *
 * \param[in] vpe Pointer to the vmk proc entry.
 *
 * \retval UUID of the proc entry.
 *
 ***********************************************************************
 */
vmk_uint32 vmk_ProcEntryGetGUID(
   vmk_ProcEntry vpe);

/*
 ***********************************************************************
 * vmk_ProcRegister --                                            */ /**
 *
 * \ingroup Proc
 * \brief Invoke VMKernel function to register a proc entry
 *
 * \param[in] vpe Pointer to the vmk proc entry to be registered.
 *
 ***********************************************************************
 */
void vmk_ProcRegister(
   vmk_ProcEntry vpe);

/*
 ***********************************************************************
 * vmk_ProcUnRegister --                                              */ /**
 *
 * \ingroup Proc
 * \brief Invoke VMKernel function to remove a previously registered proc
 *        entry from the /proc file system.
 *
 * \param[in] vpe Pointer to the vmk proc entry to be registered.
 *
 * \retval VMK_OK Unregister successful
 * \retval VMK_NOT_FOUND vmk proc entry not found
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_ProcUnRegister(
   vmk_ProcEntry vpe);

/*
 ***********************************************************************
 * vmk_ProcSetupPredefinedNode --                                 */ /**
 *
 * \ingroup Proc
 * \brief Invoke VMKernel function to hook up pre-defined proc node
 *
 * \param[in] idx Index of the pre-defined proc node
 * \param[in] vpe Pointer to the pre-defined proc node to be setup.
 * \param[in] root Specify if vmk proc entry is the root node
 *
 ***********************************************************************
 */
void vmk_ProcSetupPredefinedNode(
   vmk_ProcEntryParent idx,
   vmk_ProcEntry vpe,
   vmk_Bool root);

/*
 ***********************************************************************
 * vmk_ProcRemovePredefinedNode --                                 */ /**
 *
 * \ingroup Proc
 * \brief Invoke VMKernel function to remove pre-defined proc node
 *
 * \param[in] idx Index of the pre-defined proc node
 * \param[in] vpe Pointer to the pre-defined proc node to be removed.
 * \param[in] root Specify if vmk proc entry is the root node
 *
 ***********************************************************************
 */
void vmk_ProcRemovePredefinedNode(
   vmk_ProcEntryParent idx,
   vmk_ProcEntry vpe,
   vmk_Bool root);

#endif /* _VMKAPI_PROC_H_ */
/** @} */
