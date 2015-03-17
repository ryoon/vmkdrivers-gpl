/*
 **********************************************************
 * Copyright 2010 VMware, Inc.  All rights reserved.
 **********************************************************
 */

/*
 * vmkplexer_scsi.c --
 *
 *      Management of global resources for SCSI layer
 */

#include "vmkapi.h"
#include <vmkplexer_module.h>
#include <vmkplexer_init.h>
#include <vmkplexer_scsi.h>
#include <vmkplexer_mempool.h>

#define VMKPLXR_LOG_HANDLE VmkplxrScsi
#include <vmkplexer_log.h>

/*
 * Thin-vmklinux scsi cmd slabs are backed by this mempool.
 * Min and Max values are based on the number of outstanding IOs,
 * currently ESX supports 64k commands.
 * Min size reserved = 64k * align(slab item size)
 *                     64k * 512 = 32M
 * Here slab item is scsi_cmd struct and it's aligned size is 512 bytes.
 *
 * Max size of 50M is choosen to provide some room for any overhead with
 * internal allocatiion housekeeping. Also, curently slabs backed by
 * mempool can't give back extra memory to mempool.
 */
#define VMKPLXR_SCSICMD_MEMPOOL		"vmkplxrScsiCmdMempool"
#define VMKPLXR_SCSICMD_POOL_MIN	(32*1024*1024)
#define VMKPLXR_SCSICMD_POOL_MAX	(50*1024*1024)
vmk_MemPool scsiCmdMemPool;

#define SIZE_IN_PAGES(size)	(((size) + (VMK_PAGE_SIZE - 1)) / VMK_PAGE_SIZE)
#define VMK_SCSI_SLAB_CONTROL_OFFSET(size, alignment)					\
	((((size) + vmk_SlabControlSize()) > VMK_UTIL_ROUNDUP((size), (alignment))) ?	\
	(VMK_UTIL_ROUNDUP((size), (alignment)) - vmk_SlabControlSize()) : (size))

/*
 *-----------------------------------------------------------------------------
 *
 *  VmkplxrScsiCmdMempoolCreate --
 *
 *      Create scsiCmdMemPool - a backing mempool for the scsi cmd slabs.
 *
 *  Results:
 *      VMK_OK on success, panics otherwise
 *
 *  Side effects:
 *      scsiCmdMemPool may have been created
 *
 *-----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
VmkplxrScsiCmdMempoolCreate(void)
{
   vmk_MemPoolProps props;
   VMK_ReturnStatus status = VMK_OK;

   /*
    * set reservation, limit - size in pages
    */
   props.module = vmk_ModuleStackTop();

   status = vmkplxr_GetMemPool(&props.parentMemPool);
   VMK_ASSERT(status == VMK_OK);

   props.memPoolType = VMK_MEM_POOL_LEAF;
   props.resourceProps.reservation = SIZE_IN_PAGES(VMKPLXR_SCSICMD_POOL_MIN);
   props.resourceProps.limit = SIZE_IN_PAGES(VMKPLXR_SCSICMD_POOL_MAX);
   status = vmk_NameInitialize(&props.name, VMKPLXR_SCSICMD_MEMPOOL);
   VMK_ASSERT(status == VMK_OK);

   status = vmk_MemPoolCreate(&props, &scsiCmdMemPool);

   if (status != VMK_OK) {
      vmk_Panic("VmkplxrScsiCmdMempoolCreate: vmk_MemPoolCreate failed for %s: %s",
                 VMKPLXR_SCSICMD_MEMPOOL, vmk_StatusToString(status));
   }

   return status;
}

/*
 *-----------------------------------------------------------------------------
 *
 *  VmkplxrScsiCmdMempoolDestroy --
 *
 *      Destroy scsiCmdMemPool.
 *
 *  Results:
 *      None.
 *
 *  Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
static void
VmkplxrScsiCmdMempoolDestroy(void)
{
   VMK_ReturnStatus status;

   status = vmk_MemPoolDestroy(scsiCmdMemPool);
   VMK_ASSERT(status == VMK_OK);
}

/*
 *-----------------------------------------------------------------------------
 *
 *  vmkplxr_ScsiCmdSlabCreate --
 *
 *      Create scsi cmd slab for the thin vmklinux client.
 *
 *      This is wrapper for vmk_SlabCreate function.
 *      Thin-vmklinux clients should provide slab object size and it's
 *      moduleID and this function will setup rest of the scsi cmd slab
 *      properties based on storage layer's scsiCmdSlab.
 *
 *  Results:
 *      If successful, the vmk_SlabID of the cache so created, or
 *      VMK_SLABID_INVALID otherwise.
 *
 *  Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
vmk_SlabID
vmkplxr_ScsiCmdSlabCreate(vmkplxr_ScsiCmdSlabPool *slabpool)
{
   vmk_SlabCreateProps props;
   VMK_ReturnStatus status;
   vmk_SlabID slab = VMK_INVALID_SLAB_ID;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);

   if (slabpool->moduleID == VMK_INVALID_MODULE_ID) {
      VMKPLXR_WIRPID("Invalid module ID");
      goto error;
   }

   /*
    * slab name, owner
    */
   vmk_NameInitialize(&(props.name), slabpool->name);
   props.module = slabpool->moduleID;

   /*
    * slabs are backed up by mempool
    */
   props.type = VMK_SLAB_TYPE_MEMPOOL;
   props.typeSpecific.memPool.memPool = scsiCmdMemPool;
   props.typeSpecific.memPool.physContiguity = VMK_MEM_PHYS_ANY_CONTIGUITY;
   props.typeSpecific.memPool.physRange = VMK_PHYS_ADDR_ANY;

   /*
    * slab size - storage scsiCmdSlab values used here
    * minObj is kept same for all thin vmklinux clients and this value is choosen
    * not based on number HBAs each vmklinux support but how much IO activity we
    * may see, even if vmklinux has just one HBA to it - user can issue more IOs
    * to it and that vmklinux should be ready to handle the IO workload.
    * And with drivers of large queue depth and with high IO load, that the
    * number of allocated slab objects can be close to or higher than the minObj
    * value, even with single HBA driver is bound to vmklinux.
    */   
   props.maxObj = vmk_ScsiCommandMaxCommands();
   props.minObj = props.maxObj / 20;

   /*
    * slab object properties
    */
   props.objSize = slabpool->objSize;
   props.alignment = VMK_L1_CACHELINE_SIZE;
   props.ctrlOffset = VMK_SCSI_SLAB_CONTROL_OFFSET(props.objSize, props.alignment);

   /*
    * slab - nullify unused ones
    */
   props.constructor = NULL;
   props.destructor = NULL;
   props.constructorArg.ptr = NULL;

   /*
    * create slab
    */
   status = vmk_SlabCreate(&props, &slab);

   if (status != VMK_OK) {
      vmk_Panic("vmkplxr_ScsiCmdSlabCreate: vmk_SlabCreate failed for %s: %s",
                 slabpool->name, vmk_StatusToString(status));
   }

error:
   return slab;
}
VMK_MODULE_EXPORT_SYMBOL(vmkplxr_ScsiCmdSlabCreate);

/*
 *-----------------------------------------------------------------------------
 *
 *  vmkplxr_ScsiInit/Cleanup --
 *
 *      Init and cleanup routines for SCSI layer.
 *
 *  Results:
 *      None.
 *
 *  Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
VMK_ReturnStatus
vmkplxr_ScsiInit(void)
{
   VMK_ReturnStatus status;

   VMKPLXR_CREATE_LOG();

   status = VmkplxrScsiCmdMempoolCreate();
   
   return status;
}

void
vmkplxr_ScsiCleanup(void)
{
   VmkplxrScsiCmdMempoolDestroy();

   VMKPLXR_DESTROY_LOG();
}

