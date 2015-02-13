/* **********************************************************
 * Copyright 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * vmkplexer_scsi.h
 */

#ifndef VMKPLEXER_SCSI_H
#define VMKPLEXER_SCSI_H

#include "vmkapi.h"

/*
 * SCSI-specific structure definitions and prototypes exported to
 * thin-vmklinux modules go here.  Other subsystems can put their
 * definitions in other files under this directory.
 */

typedef struct vmkplxr_ScsiCmdSlabPool {
   vmk_SlabID slab;
   vmk_uint32 users;
   const char *name;
   vmk_ByteCountSmall objSize;
   vmk_ModuleID moduleID;
} vmkplxr_ScsiCmdSlabPool;

vmk_SlabID vmkplxr_ScsiCmdSlabCreate(vmkplxr_ScsiCmdSlabPool *pool);

#endif /* VMKPLEXER_SCSI_H */

