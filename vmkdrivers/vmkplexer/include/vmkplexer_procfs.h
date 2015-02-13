
/* **********************************************************
 * Copyright 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * vmkplexer_procfs.h
 *
 *      Definitions for procfs nodes management.
 */

#include "vmkapi.h"

#ifndef VMKPLEXER_PROCFS_H
#define VMKPLEXER_PROCFS_H

#define VMKPLXR_PROCFS_MAX_NAME    VMK_PROC_MAX_NAME                       

typedef struct VmkplxrProcfsEntry vmkplxr_ProcfsEntry;
typedef int (*vmkplxr_ProcfsReadFunc)(vmkplxr_ProcfsEntry *, char *, int *);
typedef int (*vmkplxr_ProcfsWriteFunc)(vmkplxr_ProcfsEntry *, char *, int *);

extern void *
vmkplxr_ProcfsGetPrivateData(vmkplxr_ProcfsEntry *entry);

extern void
vmkplxr_ProcfsSetPrivateData(vmkplxr_ProcfsEntry *entry, void *privateData);

extern vmkplxr_ProcfsEntry *
vmkplxr_ProcfsAllocEntry(vmk_ModuleID moduleID);

extern void
vmkplxr_ProcfsFreeEntry(vmkplxr_ProcfsEntry *);

void
vmkplxr_ProcfsInitEntry(vmkplxr_ProcfsEntry *entry, 
                        const char *name, 
                        vmk_Bool isDir,
                        vmkplxr_ProcfsReadFunc read,
                        vmkplxr_ProcfsWriteFunc write);

VMK_ReturnStatus
vmkplxr_ProcfsAttachEntry(vmkplxr_ProcfsEntry *searchLevelEntry,
                          const char          *path,
                          vmkplxr_ProcfsEntry *newEntry);

extern VMK_ReturnStatus
vmkplxr_ProcfsDetachEntry(vmk_ModuleID ownerID, vmkplxr_ProcfsEntry *searchLevelEntry,
                          const char *path, vmkplxr_ProcfsEntry **entryDetached);

extern vmkplxr_ProcfsEntry *
vmkplxr_ProcfsFindEntry(vmkplxr_ProcfsEntry *searchLevelEntry, const char *entryPath);

#endif /* VMKPLEXER_PROCFS_H */
