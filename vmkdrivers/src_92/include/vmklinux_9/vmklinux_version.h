/* **********************************************************
 * Copyright 1998, 2010, 2011, 2013 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * vmklinux_version.h --
 *
 *      Version of the vmkernel / vmklinux interface
 */

#ifndef _VMKLINUX26_VERSION_H_
#define _VMKLINUX26_VERSION_H_

#include "buildNumber.h"

/*
 * Macro to convert a symbol to a string
 */
#define __VMKLNX_STRINGIFY(sym)            #sym
#define VMKLNX_STRINGIFY(sym)              __VMKLNX_STRINGIFY(sym)

/*
 * Version string fragments.
 */
#define UBAR            _
#define DOT             .

/*
 * "com.vmware.driverAPI" is used both as the name of the API and as the
 * name of the various module namespaces.
 *
 * This string is a well known name that also appears in scons scripts,
 * packaging, etc.
 */
#define VMKLNX_API      com.vmware.driverAPI

/*
 * Macros to glue tokens together.
 *
 * We use multiple layers of macros to force complete pre-processor expansion.
 */
#define __VMKLNX_MAKE_TOKEN3S(prefix, sep, suffix)                      \
   prefix ## sep ## suffix
#define __VMKLNX_MAKE_TOKEN3(prefix, sep, suffix)                        \
   __VMKLNX_MAKE_TOKEN3S(prefix, sep, suffix)
#define __VMKLNX_MAKE_TOKEN5S(sep, major, minor, update, patch)         \
   major ## sep ## minor ## sep ## update ## sep ## patch
#define __VMKLNX_VERSION_TOKEN5(sep, major, minor, update, patch)       \
   __VMKLNX_MAKE_TOKEN5S(sep, major, minor, update, patch)

/*
 * Macro to combine major, minor, update, and patch
 * version numbers into an integer.
 */
#define VMKLNX_MAKE_VERSION(major,minor,update,patch) \
   (((((((major) << 8) | (minor)) << 8) | (update)) << 8) | (patch))

/*
 * The MAJOR and MINOR values are exported to the scons script
 * bora/scons/modules/vmkDrivers.sc. If you change the name
 * of the two macros below, you need to change vmkDrivers.sc
 * as well.
 */
#define VMKLNX_DDI_VERSION_MAJOR           9
#define VMKLNX_DDI_VERSION_MINOR           2
#define VMKLNX_DDI_VERSION_UPDATE          3
#define VMKLNX_DDI_VERSION_PATCH           0

#define __VMKLNX_VERSION_TOKEN_D                                \
        __VMKLNX_VERSION_TOKEN5(DOT,                            \
                                VMKLNX_DDI_VERSION_MAJOR,       \
                                VMKLNX_DDI_VERSION_MINOR,       \
                                VMKLNX_DDI_VERSION_UPDATE,      \
                                VMKLNX_DDI_VERSION_PATCH)

#define __VMKLNX_VERSION_TOKEN_U                                \
        __VMKLNX_VERSION_TOKEN5(UBAR,                           \
                                VMKLNX_DDI_VERSION_MAJOR,       \
                                VMKLNX_DDI_VERSION_MINOR,       \
                                VMKLNX_DDI_VERSION_UPDATE,      \
                                VMKLNX_DDI_VERSION_PATCH)


/*
 * Name space support:
 * Both vmklinux and the shim export a namespace called "com.vmware.driverAPI".
 * The difference is in the version number.  The version for vmklinux is
 * given by just the major number, whereas the shims use all four numbers
 * separated by dots.
 */
#define VMKLINUX_NAMESPACE              VMKLNX_STRINGIFY(VMKLNX_API)
#define VMKLNX_NS_CURRENT_VERSION       VMKLNX_STRINGIFY(__VMKLNX_VERSION_TOKEN_D)

#define VMKLNX_DDI_VERSION                                      \
   VMKLNX_MAKE_VERSION(VMKLNX_DDI_VERSION_MAJOR,                \
                       VMKLNX_DDI_VERSION_MINOR,                \
                       VMKLNX_DDI_VERSION_UPDATE,               \
                       VMKLNX_DDI_VERSION_PATCH)

/*
 * Driver namespace definitions are found in the scons defineVmkDriver()
 * build rule.  We always use the two digit variant for drivers
 * (e.g. '9.2" for the version).  This is done because drivers have no
 * shims for update or patch releases.
 *
 * The implication for drivers: Any driver exporting an interface to
 * other drivers must maintain strict backwards binary compatibilty
 * in an update or patch release.
 */
#define __VMKDRV_VERSION_TOKEN_D                                \
        __VMKLNX_MAKE_TOKEN3(VMKLNX_DDI_VERSION_MAJOR,          \
                             DOT,                               \
                             VMKLNX_DDI_VERSION_MINOR)
#define VMKDRV_NS_CURRENT_VERSION       VMKLNX_STRINGIFY(__VMKDRV_VERSION_TOKEN_D)
#define VMKLNX_DDI_VERSION_STR          VMKDRV_NS_CURRENT_VERSION

#define VMKDRV_NAMESPACE_PROVIDES(ns) \
   VMK_NAMESPACE_PROVIDES(VMKLNX_STRINGIFY(ns), VMKDRV_NS_CURRENT_VERSION);
#define VMKDRV_NAMESPACE_REQUIRED(ns) \
   VMK_NAMESPACE_REQUIRED(VMKLNX_STRINGIFY(ns), VMKDRV_NS_CURRENT_VERSION);

/*
 * This macro is used to created different names in the various thin vmklinux
 * modules (e.g. log channels and mempool names).  For vmklinux_9, we do
 * nothing.  Other version of vmklinux need to modify the name to avoid
 * conflict.
 */
#define VMKLNX_MODIFY_NAME(name)                             \
        VMKLNX_STRINGIFY(name)

#endif // _VMKLINUX_VERSION_DIST_H_
