/* **********************************************************
 * Copyright 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Module                                                         */ /**
 * \addtogroup Module
 *
 * Module-related type definitions.
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_MODULE_TYPES_H_
#define _VMKAPI_MODULE_TYPES_H_

/**
 * \brief Opaque handle for a vmkernel module.
 *
 * \note A handle should never be printed directly. Instead, use
 *       vmk_ModuleGetDebugID to get a printable value.
 */
typedef int vmk_ModuleID;

/**
 * \brief Module ID of the currently executing module
 *
 * Module code can always use this as a reference to the ID of the module
 */
extern const vmk_ModuleID vmk_ModuleCurrentID;

#endif /* _VMKAPI_MODULE_H_ */
/** @} */
