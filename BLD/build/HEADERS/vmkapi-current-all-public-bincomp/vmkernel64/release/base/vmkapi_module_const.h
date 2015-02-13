/* **********************************************************
 * Copyright 1998 - 2009 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Module                                                         */ /**
 * \addtogroup Module
 *
 * Module-related constants.
 *
 * This header is shared with user-mode.
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_MODULE_CONST_H_
#define _VMKAPI_MODULE_CONST_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/**
 * \brief The maximum length of a module name including the terminating nul.
 */
#define VMK_MODULE_NAME_MAX 32


/**
 * \brief The separator used to split a namespace stamp into a namespace tag
 *        version tag.
 */
/** \cond nodoc */
#define _VMK_NS_VER_SEPARATOR #
/** \endcond nodoc */
#define VMK_NS_VER_SEPARATOR_STRING VMK_STRINGIFY(_VMK_NS_VER_SEPARATOR)
#define VMK_NS_VER_SEPARATOR VMK_NS_VER_SEPARATOR_STRING[0]


#endif /* _VMKAPI_MODULE_CONST_H_ */
/** @} */
