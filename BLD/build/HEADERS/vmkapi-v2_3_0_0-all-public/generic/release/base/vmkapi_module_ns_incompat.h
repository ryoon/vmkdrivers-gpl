/* **********************************************************
 * Copyright 2012 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Module                                                         */ /**
 * \defgroup NamespaceIncompat Incompatible symbol name-spaces
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_MODULE_NS_INCOMPAT_H_
#define _VMKAPI_MODULE_NS_INCOMPAT_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/*
 ***********************************************************************
 * VMK_NAMESPACE_VMKAPI_INCOMPAT
 *
 * \ingroup Namespace
 * \brief Incompatible VMKAPI name-space
 *
 * This string should be used to access the Incompatible VMKAPI
 * name-space
 *
 ***********************************************************************
 */
#define VMK_NAMESPACE_VMKAPI_INCOMPAT "com.vmware.vmkapi.incompat"

/*
 ***********************************************************************
 * VMK_NAMESPACE_INCOMPAT_CURRENT_VERSION --                      */ /**
 *
 * \ingroup Namespace
 * \brief The current incompatible VMKAPI name-space version
 *
 * Short-cut for exporting to the current incompatible VMKAPI name-space
 *
 ***********************************************************************
 */
#define VMK_NAMESPACE_INCOMPAT_CURRENT_VERSION "v2_3_0_0"

#endif /* _VMKAPI_MODULE_NS_INCOMPAT_H_ */
/** @} */
