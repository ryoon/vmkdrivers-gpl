/* **********************************************************
 * Copyright 1998 - 2009 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Module (incompatible)                                          */ /**
 * \addtogroup Module
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_MODULE_INCOMPAT_H_
#define _VMKAPI_MODULE_INCOMPAT_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "vmkapi_module_ns_incompat.h"

/*
 ***********************************************************************
 * VMK_MODULE_EXPORT_SYMBOL_DIRECT --                             */ /**
 *
 * \ingroup Module
 * \brief Mark a symbol as exported only for direct calls
 *
 * Mark the given symbol as exported, and hence available for other
 * modules to find/call.  This differs from VMK_MODULE_EXPORT_SYMBOL
 * in that the exported symbol will never be called through a
 * trampoline; all calls will be "direct".  Note that exported
 * symbols are not called through trampolines by default, so this is
 * only necessary if you know callers may use trampolines and you want
 * to prevent it.  See below for more information.
 *
 * \param[in] __symname    The symbol to export.
 *
 ***********************************************************************
 */
#define VMK_MODULE_EXPORT_SYMBOL_DIRECT(__symname)      \
   __VMK_MODULE_EXPORT_SYMBOL_DIRECT(__symname)


/*
 ***********************************************************************
 * vmk_ModuleLoad --                                              */ /**
 *
 * \ingroup Module
 * \brief Load a vmkernel module.
 *
 * \note This is a blocking call.
 *
 * \pre The caller should not hold any spinlock.
 *
 * \param[in]  module      Module name to load.
 * \param[out] moduleID    Module ID if module loaded successfully
 *
 * \retval VMK_OK          Module loaded without error or is already
 *                         loaded.
 * \retval VMK_FAILURE     Module could not be loaded.
 * \retval VMK_TIMEOUT     Module could not be loaded.
 * \retval VMK_NO_MEMORY   Module could not be loaded out of memory.
 * 
 ***********************************************************************
 */

VMK_ReturnStatus vmk_ModuleLoad(const char *module, vmk_ModuleID *moduleID);


#endif /* _VMKAPI_MODULE_INCOMPAT_H_ */
/** @} */
