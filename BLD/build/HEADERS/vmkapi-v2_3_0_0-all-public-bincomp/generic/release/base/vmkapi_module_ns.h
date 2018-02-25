/* **********************************************************
 * Copyright 2009 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Module                                                         */ /**
 * \defgroup Namespace Symbol name-spaces
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_MODULE_NS_H_
#define _VMKAPI_MODULE_NS_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/*
 ***********************************************************************
 * VMK_NAMESPACE_VMKAPI
 *
 * \ingroup Namespace
 * \brief Default VMKAPI name-space
 *
 * This string should be used to access the VMKAPI name-space
 *
 ***********************************************************************
 */
#define VMK_NAMESPACE_VMKAPI "com.vmware.vmkapi"

/*
 ***********************************************************************
 * VMK_NAMESPACE_CURRENT_VERSION      --                          */ /**
 *
 * \ingroup Namespace
 * \brief The current VMKAPI name-space version
 *
 * Short-cut for exporting to the current VMKAPI name-space
 *
 ***********************************************************************
 */
#define VMK_NAMESPACE_CURRENT_VERSION "v2_3_0_0"

#endif /* _VMKAPI_MODULE_NS_H_ */
/** @} */
