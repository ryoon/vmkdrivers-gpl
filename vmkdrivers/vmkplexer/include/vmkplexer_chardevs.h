/* **********************************************************
 * Copyright 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * vmkplexer_chardevs.h
 *
 *      Definitions for registration and unregistration
 *      services regarding vmklinux character devices as
 *      managed by the vmkplexer
 */

#include "vmkapi.h"

#ifndef VMKPLEXER_CHARDEVS_H
#define VMKPLEXER_CHARDEVS_H

#define VMKPLXR_NUM_MAJORS 255
#define VMKPLXR_MINORS_PER_MAJOR 255
#define VMKPLXR_MISC_MAJOR 10
#define VMKPLXR_DYNAMIC_MAJOR 0
#define VMKPLXR_DYNAMIC_MINOR (VMKPLXR_MINORS_PER_MAJOR)

typedef struct vmkplxr_ChardevHandles {
   vmk_AddrCookie vmkplxrInfo;  /* Opaque handle used by the vmkplexer */
   vmk_AddrCookie vmklinuxInfo; /* Opaque handle used by vmklinux */
} vmkplxr_ChardevHandles;

typedef VMK_ReturnStatus (*vmkplxr_ChardevDestructor)(vmk_AddrCookie vmkPrivate);

VMK_ReturnStatus
vmkplxr_RegisterChardev(int *major,
                        int *minor,
                        const char *name,
                        const vmk_CharDevOps *fops,
                        vmk_AddrCookie driverPrivate,
                        vmkplxr_ChardevDestructor dtor,
                        vmk_ModuleID modID);

VMK_ReturnStatus
vmkplxr_UnregisterChardev(int major,
                          int minor,
                          const char *name);

#endif /* VMKPLEXER_CHARDEVS_H */

