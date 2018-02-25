/* ****************************************************************
 * Portions Copyright 1998, 2010, 2012-2013, 2015-2016 VMware, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * ****************************************************************/

/******************************************************************
 *
 *  linux_block.c
 *
 *      Emulation for Linux block device drivers.
 *
 * From linux-2.6.18-8/block/ll_rw_blk.c:
 *
 * Copyright (C) 1991, 1992 Linus Torvalds
 * Copyright (C) 1994,      Karl Keyte: Added support for disk statistics
 * Elevator latency, (C) 2000  Andrea Arcangeli <andrea@suse.de> SuSE
 * Queue request tables / lock, selectable elevator, Jens Axboe <axboe@suse.de>
 * kernel-doc documentation started by NeilBrown <neilb@cse.unsw.edu.au> -  July2000
 * bio rewrite, highmem i/o, etc, Jens Axboe <axboe@suse.de> - may 2001
 *
 * From linux-2.6.18-8/fs/bio.c:
 *
 * Copyright (C) 2001 Jens Axboe <axboe@suse.de>
 *
 ******************************************************************/

#include <linux/list.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/major.h>
#include <linux/fs.h>
#include <linux/timer.h>
#include <linux/proc_fs.h>
#include <linux/hdreg.h>
#include <linux/genhd.h>
#include <linux/blkpg.h>
#include <linux/spinlock.h>
#include <linux/completion.h>
#include <linux/random.h>
#include <linux/bio.h>
#include <linux/stat.h>
#include <linux/log2.h>
#include <asm/uaccess.h>
#include <asm/byteorder.h>
#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_dbg.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>

#include "vmkapi.h"

#include "linux_stubs.h"
#include "linux_scsi.h"
#include "linux_pci.h"
#include "linux_scsi.h"
#include "linux_stubs.h"
#include "linux_irq.h"
#include "linux_time.h"

#include <linux/blkdev.h>

#define VMKLNX_LOG_HANDLE LinBlock
#include "vmklinux_log.h"


/* Global definitions */
#define BLK_MAX_SECTORS 256
#define SECTOR_SIZE     512
#define MAX_SEGMENTS   128
#define MAX_CTLR_CMDS 128
#define MAX_TARG_CMDS 32
#define BLK_MAX_SECTORS 256
#define MAX_BLOCK_DISKS 256
#define MAX_NR(dev) (1 << (MINORBITS - (dev)->minors))

#define BLOCK_GET_ID(blkDev)  (blkDev->adapter->moduleID)
/* Default vendor string, must be at least SCSI_VENDOR_OFFSET characters */
#define BLOCK_DEFAULT_VENDOR_STR        "VMware   "
/* Length of model name in SCSI inquiry */
/* Default model string, must be at least SCSI_MODEL_OFFSET characters */
#define BLOCK_DEFAULT_MODEL_STR         "Block device    "
/* Default revision string, must be at least SCSI_REVISION_OFFSET characters */
#define BLOCK_DEFAULT_REVISION_STR      "1.0  "
/* Max number of commands that can be handled in bottom half at one time */
#define BLOCK_MAX_BH_COMMANDS          32

uint32_t maxCtlrCmds = MAX_CTLR_CMDS;
static vmk_Semaphore blkDrvSem;
int *max_sectors[MAX_BLKDEV];
static vmk_BHID linuxBlockBHNum;
static vmk_SpinlockIRQ blockBHListLock;
static void LinuxBlockBH(void *clientData);
struct list_head  linuxBHCompletionList;


/* For queue allocation */
static kmem_cache_t *requestq_cachep;
/* For the allocated request tables */
static kmem_cache_t *request_cachep;
static kmem_cache_t *bdev_cachep;
static LIST_HEAD(all_bdevs);

/*
 * holds bio slab and pool info
 */
struct vmklnx_bio_set *vmklnx_bio_set;

/*
 * The fake EVPD page for VMware emulated block devices
 *
 * See SCSIInqVMwarePageC0Response in scsi_ext.h.
 */
#define LINUX_BLOCK_EVPD_PAGE_CODE    0xc0
#define LINUX_BLOCK_INFO_MAGIC        0x7774663f
typedef struct LinuxBlockPageC0Response {
   vmk_uint8  devClass  :5,
              pQual     :3;
   vmk_uint8  pageCode;  /* Always VMKLNX_BLOCK_EVPD_PAGE_CODE */
   vmk_uint8  reserved1;
   vmk_uint8  pageLength;
   vmk_uint32 magic;
   vmk_uint32 channelId;
   vmk_uint32 targetId;
   vmk_uint32 lunId;
   char   adapter[VMK_MISC_NAME_MAX];
} __attribute__ ((packed)) LinuxBlockPageC0Response;

/*
 * The fake EVPD page for VMware emulated block devices
 * Required to support auto-detect of SSD devices.
 * 
 * Format of INQUIRY EVPD Page B1 response
 * SBC3-r18, Section 6.5.3 table 138 Page 178
 */
#define LINUX_BLOCK_EVPD_PAGE_B1_CODE                  0xB1
#define LINUX_BLOCK_ROTATION_RATE_NOT_REPORTED         0x0
#define LINUX_BLOCK_ROTATION_RATE_NON_ROTATING_MEDIUM  0x1
typedef struct LinuxBlockPageB1Response {
   vmk_uint8  devClass  :5,
              pQual     :3;
   vmk_uint8  pageCode;  /* Always VMKLNX_BLOCK_EVPD_PAGE_CODE */
   vmk_uint8  reserved1;
   vmk_uint8  pageLength;
   vmk_uint16 mediumRotationRate;
   vmk_uint8  reserved2;
   vmk_uint8  formFactor:4,
              reserved3:4;
   vmk_uint8  reserved4[56];
} __attribute__ ((packed)) LinuxBlockPageB1Response;

/*
 * Controlling structure to kblockd
 */
static struct workqueue_struct *kblockd_workqueue;
/*
 * bdev-inode mapping
 */
typedef struct bdev_inode {
   struct block_device bdev;
   struct inode vfs_inode;
} bdev_inode;

/*
 * Disk information
 */
typedef struct LinuxBlockDisk {
   vmk_Bool exists;
   vmk_Bool cdrom;
   vmk_Bool capacityValid;
   vmk_Bool isSSD; // Device is an SSD device.
   uint32_t capacity; // Cached capacity in sectors
   uint32_t targetId;
   struct   gendisk* gd;
} LinuxBlockDisk;

/*
 * Vmklinux specific block adapter info.
 */
typedef struct LinuxBlockAdapter {
   char devName[VMK_DEVICE_NAME_MAX_LENGTH];
   vmk_ScsiAdapter                *adapter;
   struct block_device_operations *ops;
   const char                     *name;
   void                           *data;
   uint32_t                       maxDisks;
   LinuxBlockDisk                 *disks;
   int                            major;
   int                            minor_shift;
} LinuxBlockAdapter;

/*
 * Global array of block adapters.
 */
static LinuxBlockAdapter *blockDevices[MAX_BLKDEV];

/*
 * Holds request completion list .
 */
typedef struct LinuxBlockBuffer {
   struct list_head     requests;
   struct bio           *lbio;
   vmk_ScsiCommand      *cmd;
   vmk_Bool             lastOne; /* TODO: will be removed? */
   struct request       *creq;
} LinuxBlockBuffer;

typedef struct LinuxCapacityRequest {
   struct list_head  links;
   volatile vmk_Bool exit;
   uint32_t          targetID;
   vmk_ScsiCommand   *cmd;
} LinuxCapacityRequest;

static void vmklnx_block_register_disk_maxXfer(vmk_uint32 major,
                                               int targetNum,
                                               vmk_uint64 maxXfer);
static inline void vmklnx_bio_fs_destructor(struct bio *bio);
static void vmklnx_blk_init_request_from_bio(struct request *rq, struct bio *bio);
static void vmklnx_blk_rq_init(request_queue_t *q, struct request *rq);
static inline void vmklnx_blk_free_request(request_queue_t *q, struct request *rq);
static struct vmklnx_bio_set *vmklnx_bioset_create(unsigned int pool_size,
                                                   unsigned int front_pad);
static void vmklnx_bioset_free(struct vmklnx_bio_set *bs);
static inline LinuxBlockBuffer *vmklnx_blk_get_lbb_from_bio(struct bio *bio);
static inline struct bio *bio_alloc(gfp_t gfp_mask, int nr_iovecs);
static inline struct request *vmklnx_blk_alloc_request(request_queue_t *q,
                                                       int rw, gfp_t gfp_mask);

static void LinuxBlockScheduleCompletion(vmk_ScsiCommand *cmd,
                                         vmk_ScsiHostStatus host,
                                         vmk_ScsiDeviceStatus device);

/*
 * add-request adds a request to the linked list.
 * queue lock is held and interrupts disabled, as we muck with the
 * request queue list.
 */
static inline void
add_request(request_queue_t * q, struct request * req)
{
   list_add(&req->queuelist, &q->queue_head);
}

/*
 * Driver calls this to release a request struct. We don't use freelists in the
 * the request_queue structure; request structs are alloc'd and freed in
 * LinuxBlockIssueCmd and LinuxBlockIODone respectively. So there's not much to
 * do here.
 */
void
blkdev_release_request(struct request *req)
{
   req->rq_status = RQ_INACTIVE;
   /*
    * By now lbb + bio belong to this request should be cleaned up,
    * now is the time to free request struct itself
    */
   vmklnx_blk_free_request(req->q, req);
}

/* The only useful code from this routine up(req->sem) has been moved up to
 * end_that_request_first because req may be have freed by LinuxBlockIODone
 */
/**
 *  end_that_request_last - releases the given request and issues its completion
 *  @req: request to end
 *  @uptodate: ignored
 *
 *  Releases the given request and issues its waiting completion if
 *  req->waiting is non-NULL
 *
 *  ESX Deviation Notes:
 *  This function is a no-op if req->waiting is not set
 *
 *  RETURN VALUE:
 *  This function does not return a value
 */
/* _VMKLNX_CODECHECK_: end_that_request_last */
void
end_that_request_last(struct request *req, int uptodate)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   /* TODO IDE driver need this as it has callers waiting */
   if (req->waiting) {
      VMKLNX_DEBUG(4, "callers waiting");
      complete(req->waiting);
   }

   /* now release request */
   blkdev_release_request(req);
}
EXPORT_SYMBOL(end_that_request_last);

/* from block/ll_rw_blk.c */
void
blk_queue_dma_alignment(request_queue_t *q, int mask)
{
   q->dma_alignment = mask;
}

/**
 *  blk_queue_max_sectors - set max sectors for a request for this queue
 *  @q: the request queue for the device
 *  @max_sectors: max sectors in the usual 512 byte units
 *
 *  Enables a low level driver to set an upper limit on the size of received requests
 *
 *  RETURN VALUE:
 *  None
 *
 */
/* _VMKLNX_CODECHECK_: blk_queue_max_sectors */
void
blk_queue_max_sectors(request_queue_t *q, unsigned int max_sectors)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if ((max_sectors << 9) < PAGE_CACHE_SIZE) {
      max_sectors = 1 << (PAGE_CACHE_SHIFT - 9);
      VMKLNX_DEBUG(4, "set to minimum %d", max_sectors);
   }

   if (BLK_DEF_MAX_SECTORS > max_sectors)
      q->max_hw_sectors = q->max_sectors = max_sectors;
   else {
      q->max_sectors = BLK_DEF_MAX_SECTORS;
      q->max_hw_sectors = max_sectors;
   }
}
EXPORT_SYMBOL(blk_queue_max_sectors);

/**
 *  blk_cleanup_queue - cleanup a queue
 *  @q: pointer to the struct request_queue_t
 *
 *  cleanup a block request queue
 *
 *  RETURN VALUE:
 *  None
 *
 */
/* _VMKLNX_CODECHECK_: blk_cleanup_queue */
void
blk_cleanup_queue(request_queue_t * q)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   blk_put_queue(q);
}
EXPORT_SYMBOL(blk_cleanup_queue);

/*
 *----------------------------------------------------------------------
 *
 * blk_queue_bounce_limit --
 *
 *      Dummy function to support Linux 2.4 drivers. This function is
 *      called to tell the upper layer that the driver will not do I/O
 *      to/from buffers above the range given by dma_addr (which is a
 *      DMA mask).
 *
 *      In the Linux kernel, this function resides in
 *          drivers/block/ll_rw_blk.c
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
/**
 *  blk_queue_bounce_limit - set bounce buffer limit for queue
 *  @q: the request queue for the device
 *  @dma_addr: dma bus address limit
 *
 *  Sets bounce buffer limit for given queue. Called by a low level driver
 *  to tell the upper layer that it can't do I/O to/from buffers above the
 *  range given by dma_addr and so upper layer can allocate lower memory
 *  pages for bounce buffers below @dma_addr
 *
 *  RETURN VALUE:
 *  None
 *
 */
/* _VMKLNX_CODECHECK_: blk_queue_bounce_limit */
void
blk_queue_bounce_limit(request_queue_t *q, u64 dma_addr)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   q->bounce_pfn = dma_addr >> PAGE_SHIFT;
}
EXPORT_SYMBOL(blk_queue_bounce_limit);

/*
 *----------------------------------------------------------------------
 *
 * resetup_one_dev --
 *
 *      Emulate the resetup_one_dev Linux call.  This function is called
 * for each disk drive on an adapter.
 *
 * Results:
 * None.
 *
 * Side effects:
 * Disk structures are allocated and initialized.
 *
 *----------------------------------------------------------------------
 */
void
resetup_one_dev(struct gendisk *dev, int drive)
{
   vmk_Bool cdrom = VMK_FALSE;

   VMKLNX_DEBUG(0, "Initializing Drive %d", drive);

   if (blockDevices[dev->major] == NULL) {
      VMKLNX_WARN("No device registered for major %d", dev->major);
   } else if (drive > MAX_NR(dev)) {
      VMKLNX_WARN("Drive #%d > %d", drive, MAX_NR(dev));
   } else {
     if (blockDevices[dev->major]->disks == NULL) {
        if (MAX_NR(dev) <= 0 || MAX_NR(dev) > MAX_BLOCK_DISKS) {
           VMKLNX_WARN("Bad value for max_nr = %d", MAX_NR(dev));
           return;
        }

         blockDevices[dev->major]->disks =
            (LinuxBlockDisk *)kzalloc(sizeof(LinuxBlockDisk) * MAX_NR(dev), GFP_KERNEL);
         if (blockDevices[dev->major]->disks == NULL) {
            VMKLNX_WARN("Memory allocation failed");
            return;
         }
         blockDevices[dev->major]->maxDisks = MAX_NR(dev);
      }
      if (blockDevices[dev->major]->disks[drive].exists) {
         VMKLNX_WARN("(%d:%d) already exists", dev->major, drive);
         return;
      }
      blockDevices[dev->major]->disks[drive].exists = VMK_TRUE;
      blockDevices[dev->major]->disks[drive].targetId = drive;
      blockDevices[dev->major]->disks[drive].cdrom = cdrom;
      blockDevices[dev->major]->disks[drive].capacityValid = VMK_FALSE;
      blockDevices[dev->major]->disks[drive].capacity = 0;
      blockDevices[dev->major]->disks[drive].gd = dev;
   }
}

/**
 *  bdget - get a block_device structure 
 *  
 *  @dev: device number of a block device 
 *
 *  Retrieve the block_device structure of
 *  the device that is identified by the device number @dev.
 *  If the device does not have a block_device structure yet,
 *  the function will create one for the device.
 *
 *  RETURN VALUE:
 *  block_device on success; NULL on failure.
 *
 */
/* _VMKLNX_CODECHECK_: bdget */

struct block_device *
bdget(dev_t dev)
{
   struct list_head *entry;
   struct block_device *bdev = NULL;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   list_for_each(entry, &all_bdevs) {
      bdev = list_entry(entry, struct block_device, bd_list);
      if (bdev && (bdev->bd_dev == dev)) {
         VMKLNX_DEBUG(4, "bdev found");
         break;
      }
   }
   if (!bdev || bdev->bd_dev != dev) {
      /*
       * Create bdev
       */
      bdev = kmem_cache_alloc(bdev_cachep, GFP_ATOMIC);
      if (bdev) {
         VMKLNX_DEBUG(4, "bdev created");
         bdev->bd_dev = dev;
         bdev->bd_disk = NULL;
         bdev->bd_contains = NULL;
         bdev->bd_part_count = 0;
         bdev->bd_invalidated = 0;
         list_add(&bdev->bd_list, &all_bdevs);
      } else {
         VMKLNX_WARN("kmem_cache_alloc failed");
      }
   }
   return bdev;
}
EXPORT_SYMBOL(bdget);

/*
 *----------------------------------------------------------------------
 * add_disk --
 *
 *      Add a generic harddisk to the gendisk_head list. This happens
 *      during driver initialization. struct gendisk gives us some useful
 *      adapter information; only one generic disk is added per adapter.
 *
 *      In the Linux kernel, this function resides in
 *          drivers/block/genhd.c
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Adds a block device on the COS for this adapter. We have to do this
 *      here instead of in register_blkdev() because "minor_shift" is not known
 *      there.
 *
 *----------------------------------------------------------------------
 */

/**                                          
 *  add_disk - add a generic harddisk and update adapter information.       
 *  @gp: the pointer to gendisk structure
 *
 *  Once the gendisk structure is ready,  a call to add_disk will add the disk 
 *  pointed to by @gp to the adapter, initializes the parameters of the disk 
 *  and update the necessary fields for the managing the device. Only one disk 
 *  is added per adapter.                                           
 *
 *  ESX Deviation Notes:                     
 *  Disk reference counts are not updated by add_disk.
 *
 *  RETURN VALUE:
 *  NONE
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: add_disk */
void
add_disk(struct gendisk *gp)
{
   int majorNumber, blockInstance = 0;
   LinuxBlockAdapter *bd = blockDevices[gp->major];
   int   disk;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if (bd->ops == NULL) {
      bd->ops = gp->fops;
   }
   bd->minor_shift = ffs(gp->minors) - 1;
   disk = (gp->first_minor) >> bd->minor_shift;

   /*
    * Update required fields for managing the device
    */
   strncpy(bd->adapter->mgmtAdapter.t.block->devName, gp->disk_name,
           VMKDRIVER_DEV_NAME_LENGTH);

   for (majorNumber = 0; majorNumber <= gp->major; majorNumber++) {
      LinuxBlockAdapter *devp = blockDevices[majorNumber];
      if (devp && devp->adapter) {
         if ((devp->adapter->mgmtAdapter.transport ==
              VMK_STORAGE_ADAPTER_BLOCK) &&
             (!strcmp(devp->adapter->mgmtAdapter.t.block->devName, gp->disk_name))) {
            ++blockInstance;
         }
      }
   }
   bd->adapter->mgmtAdapter.t.block->controllerInstance = blockInstance - 1;
   resetup_one_dev(gp, disk);
}
EXPORT_SYMBOL(add_disk);

/**
 *  del_gendisk - non-operational function       
 *  @gp: Ignored	 
 *                                           
 *  A non-operational function provided to help reduce kernel ifdefs.
 *  It is not supported in this release of ESX.
 *                                           
 *  RETURN VALUE:
 *  This function does not return a value
 *
 *  ESX Deviation Notes:                     
 *  A non-operational function provided to help reduce kernel ifdefs.
 *  It is not supported in this release of ESX.
 *
 */
/* _VMKLNX_CODECHECK_: del_gendisk */
void
del_gendisk(struct gendisk *gp)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
}
EXPORT_SYMBOL(del_gendisk);

void
BlockLinux_Init(void)
{
   VMK_ReturnStatus status;
   vmk_BHProps bhProps;

   VMKLNX_CREATE_LOG();

   status = vmk_SPCreateIRQ_LEGACY(&blockBHListLock, vmklinuxModID, \
                            "blockBHListLock", NULL, VMK_SP_RANK_IRQ_LEAF_LEGACY);
   VMK_ASSERT(status == VMK_OK);

   bhProps.moduleID = vmklinuxModID;
   status = vmk_NameInitialize(&bhProps.name, "linuxBlock");
   VMK_ASSERT(status == VMK_OK);
   bhProps.callback = LinuxBlockBH;
   bhProps.priv = NULL;
   status = vmk_BHRegister(&bhProps, &linuxBlockBHNum);
   VMK_ASSERT_BUG(status == VMK_OK);

   INIT_LIST_HEAD(&linuxBHCompletionList);

   status = vmk_SemaCreate(&blkDrvSem, vmk_ModuleCurrentID, "blkDrvSem", 1);
   if (status != VMK_OK) {
      VMKLNX_WARN("Init: vmk_SemaCreate failed: %s", vmk_StatusToString(status));
      vmk_SPDestroyIRQ(&blockBHListLock);
      return;
   }

   kblockd_workqueue = create_workqueue("kblockd");
   if (!kblockd_workqueue) {
      vmk_Panic("Failed to create kblockd\n");
   }
   request_cachep = kmem_cache_create("blkdev requests", sizeof(struct request), \
                                      0, SLAB_PANIC, NULL, NULL);
   requestq_cachep = kmem_cache_create("blkdev queue", sizeof(request_queue_t), \
                                       0, SLAB_PANIC, NULL, NULL);
   bdev_cachep = kmem_cache_create("bdev_cache", sizeof(struct bdev_inode), \
                                   0, (SLAB_HWCACHE_ALIGN|SLAB_RECLAIM_ACCOUNT|SLAB_MEM_SPREAD|SLAB_PANIC), \
                                   NULL, NULL);

   if (!request_cachep || !requestq_cachep || !bdev_cachep) {
      vmk_Panic("Failed to create vmlinux block layer slabs\n");
   }

   vmklnx_bio_set = vmklnx_bioset_create(BLKDEV_MIN_RQ, sizeof(LinuxBlockBuffer));
   if (!vmklnx_bio_set) {
      vmk_Panic("Failed to create vmklinux block layer bio slab\n");
   }
}


void
BlockLinux_Cleanup(void)
{
   vmklnx_bioset_free(vmklnx_bio_set);
   kmem_cache_destroy(bdev_cachep);
   kmem_cache_destroy(requestq_cachep);
   kmem_cache_destroy(request_cachep);

   vmk_SemaDestroy(&blkDrvSem);
   vmk_SPDestroyIRQ(&blockBHListLock);
   VMKLNX_DESTROY_LOG();
}

/*
 *----------------------------------------------------------------------
 *
 * LinuxBlockDiscover --
 *
 *      Handle device scanning
 *      channel, lun: expected to be 0
 *      targetNum: is equivalent to a disk number on a
 *        particular block device
 *      deviceData: meant to be cached by upper layers and provided
 *        in subsequent calls to the vmklinux scsi api.
 *
 * Results:
 *      VMK_OK Suceess
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
static VMK_ReturnStatus
LinuxBlockDiscover(void *clientData, vmk_ScanAction action,
                   int channel, int targetNum, int lun,
                   void **deviceData)
{
   LinuxBlockAdapter *dev = (LinuxBlockAdapter *)clientData;


   VMK_ASSERT(dev);
   VMK_ASSERT(channel == 0);
   VMK_ASSERT(lun == 0);

   switch(action) {
   case VMK_SCSI_SCAN_CREATE_PATH:
      VMKLNX_DEBUG(3, "VMK_SCSI_SCAN_CREATE_PATH");
   case VMK_SCSI_SCAN_CONFIGURE_PATH:
      VMKLNX_DEBUG(3, "VMK_SCSI_SCAN_CONFIGURE_PATH");
      /* Return block disk; block devices created during driver init */
      *deviceData = (void *)&dev->disks[targetNum];
      
      if (dev->disks[targetNum].gd &&
          dev->disks[targetNum].gd->maxXfer) {
         /* Propagate the previously set max. transfer size for this path */
         vmklnx_block_register_disk_maxXfer(dev->major, targetNum,
                                            dev->disks[targetNum].gd->maxXfer);
      }

      return VMK_OK;
   case VMK_SCSI_SCAN_DESTROY_PATH:
      VMKLNX_DEBUG(3, "VMK_SCSI_SCAN_DESTROY_PATH");
      *deviceData = NULL;
      return VMK_OK;
   default:
      VMK_ASSERT(0);
      return VMK_NOT_IMPLEMENTED;
   }
}

/*
 * Set the sense buffer with info indicating an illegal SCSI opcode.
 * You must also return a device status of SDSTAT_CHECK for the SCSI
 * command in order for the sense buffer to be examined.
 */
static void
LinuxBlockInvalidOpcode(vmk_ScsiSenseDataSimple *sense, int isCommand)
{
   memset(sense, 0, sizeof(*sense));
   sense->format.fixed.valid = VMK_TRUE;
   sense->format.fixed.error = VMK_SCSI_SENSE_ERROR_CURCMD;
   sense->format.fixed.key = VMK_SCSI_SENSE_KEY_ILLEGAL_REQUEST;
   sense->format.fixed.optLen = 10;         // 10 bytes gets the SKSV info
   sense->format.fixed.asc = 0x20;         // invalid opcode
   sense->format.fixed.ascq = 0;
   sense->format.fixed.sksv = VMK_TRUE;     // sense key specific data is valid
   sense->format.fixed.cd = isCommand;
   sense->format.fixed.epos = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * LinuxBlockCompleteCapacity --
 *
 *      Complete a READ_CAPACITY command sucessfully
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
static void
LinuxBlockCompleteCapacity(vmk_ScsiCommand *cmd, uint32_t sectors)
{
   vmk_ScsiHostStatus hostStatus;
   vmk_ScsiReadCapacityResponse rcp;
   uint32_t len;

   hostStatus = VMK_SCSI_HOST_OK;
   len = min(sizeof(rcp), (size_t)vmk_SgGetDataLen(cmd->sgArray));

   rcp.blocksize = cpu_to_be32(512);
   rcp.lbn = cpu_to_be32(sectors > 0 ? sectors - 1 : 0);

   if (vmk_SgCopyTo(cmd->sgArray, &rcp, len) == VMK_OK) {
      cmd->bytesXferred = len;
   } else {
      VMK_ASSERT(0);
      hostStatus = VMK_SCSI_HOST_ERROR;
   }

   LinuxBlockScheduleCompletion(cmd, hostStatus, VMK_SCSI_DEVICE_GOOD);
}

/*
 *----------------------------------------------------------------------
 *
 * LinuxBlockGetCapacity --
 *
 *      Get capacity of device in sectors. Uses BLKGETSIZE ioctl.
 *
 * Results:
 *      Size of device in sectors.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static VMK_ReturnStatus
LinuxBlockGetCapacity(LinuxBlockAdapter *dev, // IN:
                      LinuxBlockDisk *disk,   // IN:
                      int   noWait,           // IN:
                      uint32_t *capacity)     // OUT:
{
   if (disk->targetId >= dev->maxDisks || !disk->exists) {
      return VMK_BAD_PARAM;
   }

   *capacity = disk->gd->capacity;
   disk->capacity = disk->gd->capacity;
   disk->capacityValid = VMK_TRUE;
   return VMK_OK;
}

/*
 * Set the sense buffer with info indicating an illegal SCSI request.
 * You must also return a device status of SDSTAT_CHECK for the SCSI
 * command in order for the sense buffer to be examined.
 */
static void
LinuxBlockIllegalRequest(vmk_ScsiSenseDataSimple *sense, int isCommand,
                         uint16_t byteOffset)
{
   memset(sense, 0, sizeof(*sense));
   sense->format.fixed.valid = VMK_TRUE;
   sense->format.fixed.error = VMK_SCSI_SENSE_ERROR_CURCMD;
   sense->format.fixed.key = VMK_SCSI_SENSE_KEY_ILLEGAL_REQUEST;
   sense->format.fixed.optLen = 10;         // 10 bytes gets the SKSV info
   sense->format.fixed.asc = 0x24;         // error in command block
   sense->format.fixed.ascq = 0;
   sense->format.fixed.sksv = VMK_TRUE;     // sense key specific data is valid
   sense->format.fixed.cd = isCommand;
   sense->format.fixed.epos = vmk_CPUToBE16(byteOffset);
}

static void
LinuxBlockCompleteCommand(vmk_ScsiCommand *cmd)
{
   /*
    * Force an error on underrun
    */
   if (cmd->bytesXferred < cmd->requiredDataLen &&
       cmd->status.host == VMK_SCSI_HOST_OK &&
       cmd->status.device == VMK_SCSI_DEVICE_GOOD) {
      cmd->status.host = VMK_SCSI_HOST_ERROR;
      cmd->status.device = VMK_SCSI_DEVICE_GOOD;
   }

   if (unlikely(!((cmd->status.host == VMK_SCSI_HOST_OK) &&
                  (cmd->status.device == VMK_SCSI_DEVICE_GOOD)))) {
      static uint32_t logThrottleCounter = 0;
      VMKLNX_THROTTLED_INFO(logThrottleCounter, "Command 0x%x (%p) failed H:0x%x D:0x%x",
                   cmd->cdb[0], cmd, cmd->status.host, cmd->status.device);
   }

   cmd->done(cmd);
}

/*
 * A command cannot be completed during the path I/O initiation code.
 * The cmd->done() routine may issue another I/O causing a recursion
 * and potentially exceeding the stack. See PR 319980.
 * Instead, this routine schedules a completion handler to be called
 * via a timer and the completion is outside of the initiation code.
 */
static void
LinuxBlockScheduleCompletion(vmk_ScsiCommand *cmd,
                          vmk_ScsiHostStatus host,
                          vmk_ScsiDeviceStatus device)
{
   vmk_Timer timer;

   /*
    * fill the cmd status
    */
   cmd->status.host = host;
   cmd->status.device = device;

   VMKLNX_DEBUG(2, "Scheduling cmd 0x%x (%p) completion via timer",
                    cmd->cdb[0], cmd);

   if (vmk_TimerSchedule(vmklnxTimerQueue,
			 (vmk_TimerCallback) LinuxBlockCompleteCommand,
			 (void *)cmd,
			 0,
			 VMK_TIMER_DEFAULT_TOLERANCE,
			 VMK_TIMER_ATTR_NONE,
			 VMK_LOCKDOMAIN_INVALID,
			 VMK_SPINLOCK_UNRANKED,
			 &timer) != VMK_OK) {
      vmk_Panic("Failed to schedule timer");
   }
}

/*
 *----------------------------------------------------------------------
 *
 * LinuxBlockIODone --
 *
 *      Call back function for bio completion - bio->bi_end_io()
 *      This is called by softirq_done_fn() callback during BH processing
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *      Locks - "blockBHListLock" held.
 *
 *----------------------------------------------------------------------
 */

static int
LinuxBlockIODone(struct bio *bio, int bytes_done, int errors)
{
   LinuxBlockBuffer *llb = (LinuxBlockBuffer *) bio->bi_private;
   struct request *req;
   vmk_ScsiCommand *cmd;

   VMK_ASSERT(llb->lbio == bio);

   req = llb->creq;
   cmd = llb->cmd;
   VMK_ASSERT(req);
   VMK_ASSERT(cmd);
   req->errors = 0;

   if (likely(llb->lastOne)) {
      /*
       * Record errors
       */
      if (errors) {
         req->errors++;
         VMKLNX_DEBUG(0, "SCSI_HOST_TIMEOUT");
         cmd->status.host = VMK_SCSI_HOST_TIMEOUT;
         cmd->status.device = VMK_SCSI_DEVICE_GOOD;
      } else {
         VMKLNX_DEBUG(3, "SCSI_HOST_OK");
         cmd->status.host = VMK_SCSI_HOST_OK;
         cmd->status.device = VMK_SCSI_DEVICE_GOOD;
         if (req->nr_sectors > 0) {
            cmd->bytesXferred = req->nr_sectors * SECTOR_SIZE;
         } else if (req->hard_nr_sectors > 0){
            cmd->bytesXferred = req->hard_nr_sectors * SECTOR_SIZE;
         } else {
            cmd->bytesXferred = (req->sector - req->hard_sector) * SECTOR_SIZE;
         }
      }

      LinuxBlockCompleteCommand(cmd);

      /*
       * Cleanup resources (lbb + bio) belong to this request
       */
      bio->bi_private = NULL;
      vmklnx_bio_fs_destructor(bio); /* this free's llb + bio */
   } else {
      /* we shouldn't come here */
      VMK_ASSERT(0);
   }

   return 1;
}

/*
 *-----------------------------------------------------------------------------
 *
 *  LinuxBlockBH --
 *
 *      Bottom-half to process completed requests in the bh queue.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
void
LinuxBlockBH(void *clientData)
{
   LinuxBlockBuffer *llb;
   uint64_t prevIRQL;
   int major;
   int count = 0;

   while (1) {
      /*
       * Inspired from vmklinux-scsi layer's BH behavior. The BH processes at
       * most BLOCK_MAX_BH_COMMANDS commands in one invocation.
       */
      if (count == BLOCK_MAX_BH_COMMANDS) {
         break;
      }
      ++count;

      /* 
       * Get completion from list 
       */

      prevIRQL = vmk_SPLockIRQ(&blockBHListLock);
      if (!list_empty(&linuxBHCompletionList)) {
         struct request *req;

         llb = list_entry(linuxBHCompletionList.next, LinuxBlockBuffer,
                               requests);

         list_del(&llb->requests);

         vmk_SPUnlockIRQ(&blockBHListLock, prevIRQL);

         req = llb->creq;
         VMK_ASSERT(req && req->q && req->q->softirq_done_fn);

         /*
          * softirq_done_fn() ends request and triggers cleaup of resources
          * that belong to the request -
          * softirq_done_fn() --> bio_endio() --> bio->bi_end_io() which
          * does everything needed to put this request to rest.
          */
         if (req && req->q && req->q->softirq_done_fn) {
            major = req->rq_disk->major;
            LinuxBlockAdapter *bd = blockDevices[major];
            VMKAPI_MODULE_CALL_VOID(BLOCK_GET_ID(bd), req->q->softirq_done_fn, req);
         }

         /* at this point no one should be using request or it's resources */
      } else {
         vmk_SPUnlockIRQ(&blockBHListLock, prevIRQL);
         break;
      }
   } /* while 1 */
}

inline int
bio_phys_segments(request_queue_t *q, struct bio *bio)
{
   return bio->bi_phys_segments;
}

inline int
bio_hw_segments(request_queue_t *q, struct bio *bio)
{
   return bio->bi_hw_segments;
}


/*
 *----------------------------------------------------------------------
 *
 * LinuxBlockIssueCmd --
 *
 *      Issue a block IO command to the driver.  This includes
 *      performing any physical to machine memory mapping that is
 *      needed.
 *
 *      Now, the really useful comment: vmk_scsi has already converted
 *      partition offsets into absolute block offsets (from sector 0). Block
 *      drivers are supposed to do this math based on creq->rq_dev, but we
 *      always set rq_dev corresponding to partition 0.
 *
 * Results:
 *      VMK_OK if the command could be successfully issued.
 *      VMK_NO_MEMORY if memory couldn't be allocated.
 *
 * Side effects:
 *      A new command is added to the  blk_dev[dev->major].current_request
 *      list.  The ref count is incremented on the async IO token
 *      referred to by rid->token.
 *
 *----------------------------------------------------------------------
 */

static VMK_ReturnStatus
LinuxBlockIssueCmd(LinuxBlockAdapter *dev,
                   vmk_ScsiCommand *cmd,
                   LinuxBlockDisk *disk,
                   uint32_t sectorNumber,
                   uint32_t numSectors,
                   int isRead)
{
   int i;
   struct request *creq;
   struct bio *bio;
   struct block_device *bdev;
   request_queue_t *queue;
   vmk_SgArray *sgArray = cmd->sgArray;
   vmk_SgArray *sgIOArray = cmd->sgIOArray;
   struct scatterlist *sg;
   int sectorsSeen = 0;
   VMK_ReturnStatus status = VMK_OK;
   LinuxBlockBuffer *b;
   int minor;

   VMKLNX_DEBUG(4, "read=%d sector=%d numSectors=%d",
                isRead, sectorNumber, numSectors);

   /* 
    * Handle dummy requests 
    */
   if (numSectors == 0) {
      VMKLNX_DEBUG(4, "Dummy %s request", isRead ? "READ":"WRITE");
      LinuxBlockScheduleCompletion(cmd, VMK_SCSI_HOST_OK, VMK_SCSI_DEVICE_GOOD);
      return VMK_OK;
   }

   /*
    * pre-checks to process this command
    */
   minor = disk->targetId << dev->minor_shift;
   bdev = bdget(MKDEV(dev->major, minor));
   if (bdev == NULL) {
      VMKLNX_WARN("No block device at: major %d - minor %d", dev->major, minor);
      return VMK_NOT_FOUND;
   }

   /* Create bdev-to-gendisk mapping */
   if (!bdev->bd_disk) {
      bdev->bd_disk = disk->gd;
   }

   /* now is the time to get our queue */
   queue = bdev_get_queue(bdev);
   if (queue == NULL) {
      VMKLNX_WARN("Trying to access nonexistent block-device");
      return VMK_NOT_FOUND;
   }

   /*
    * alloc lbb + bio
    * Note: bio slab object is (lbb + bio)
    */
   bio = bio_alloc(GFP_NOWAIT, sgArray->numElems);
   if (bio == NULL) {
      VMKLNX_WARN("No Memory for bio!");
      return VMK_NO_MEMORY;
   }

   b = vmklnx_blk_get_lbb_from_bio(bio);
   
   bio->bi_bdev = bdev;
   bio->bi_end_io = (bio_end_io_t *)LinuxBlockIODone;
   bio->bi_private = b;
   bio->bi_rw = (isRead) ? BIO_RW : BIO_RW+1;
   bio->bi_sector = sectorNumber;

   sectorsSeen = 0;

   /*
    * compute bio size
    */
   for (i = 0; i < sgArray->numElems; i++) {
      uint32_t len = sgArray->elem[i].length;

      VMKLNX_DEBUG(6, "sg[%d]=(%"VMK_FMT64"x,%x)", i, sgArray->elem[i].addr, len);
      VMK_ASSERT(len % SECTOR_SIZE == 0);
      VMK_ASSERT(vmk_ScsiAdapterIsPAECapable(dev->adapter) || 
                 (vmk_MAGetConstraint(sgArray->elem[i].addr) == 
                  VMK_PHYS_ADDR_BELOW_4GB));

      bio->bi_size += len;

      sectorsSeen += len / SECTOR_SIZE;
      VMK_ASSERT(sectorsSeen <= numSectors);
   }

   /*
    * use vmk sg list directly and avoid building bvec list
    */
   sg = bio->vmksg;
   VMKLNX_INIT_VMK_SG_WITH_ARRAYS(sg, sgArray, sgIOArray);

   /*
    * get request from queue's request slab pool
    */
   creq = vmklnx_blk_alloc_request(queue, bio_data_dir(bio), GFP_NOWAIT);
   if (creq == NULL) {
      status = VMK_NO_MEMORY;
      goto error;
   }

   vmklnx_blk_init_request_from_bio(creq, bio);
   creq->nr_sectors = numSectors;
   
   b->creq = creq;
   b->cmd = cmd;

   spin_lock_irq(queue->queue_lock);

   bio_get(bio); // don't let driver free
   
   blk_plug_device(queue);
   add_request(queue, creq);

   VMKLNX_DEBUG(5, "Appended request %p to major %d", creq, dev->major);

   spin_unlock_irq(queue->queue_lock);

   if (1 || bio_sync(bio)) {
      VMKLNX_DEBUG(2, "Unplug the Queue here");
      generic_unplug_device(queue, dev);
   }

   return VMK_OK;

error:
   vmklnx_bio_fs_destructor(bio);

   return status;
}

/*
 *----------------------------------------------------------------------
 *
 * LinuxBlockEVPDPageB1Command --
 *
 *      Handle a VMW vendor specific EVPD request on this block device.
 *      Return medium rotation rate info required to auto-detect SSD
 *      devices.
 *
 *      Refer to SBC3-r18, Section 6.5.3 table 138 Page 178.
 *
 * Results:
 *      device status
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
static uint32_t
LinuxBlockEVPDPageB1Command(LinuxBlockAdapter *dev, vmk_ScsiCommand *cmd,
                            LinuxBlockDisk *disk)
{
   LinuxBlockPageB1Response response;

   if (disk->cdrom) {
      return VMK_SCSI_DEVICE_CHECK_CONDITION;
   }

   vmk_Memset(&response, 0, sizeof response);
   response.devClass = VMK_SCSI_CLASS_DISK;
   response.pQual = VMK_SCSI_PQUAL_CONNECTED;
   response.pageCode = LINUX_BLOCK_EVPD_PAGE_B1_CODE;
   response.pageLength = sizeof(response) - 4;

   if (disk->isSSD) {
      response.mediumRotationRate =
         vmk_CPUToBE16(LINUX_BLOCK_ROTATION_RATE_NON_ROTATING_MEDIUM);
   }

   if (vmk_SgCopyTo(cmd->sgArray, &response, sizeof(response)) == VMK_OK) {
      cmd->bytesXferred = sizeof(response);
   } else {
      return VMK_SCSI_DEVICE_BUSY;
   }

   return VMK_SCSI_DEVICE_GOOD;
}

/*
 *----------------------------------------------------------------------
 *
 * LinuxBlockEVPDPageC0Command --
 *
 *      Handle a VMW vendor specific EVPD request on this block device.
 *      This is a gross hack but required due to the complexity of
 *      massaging block device names in the COS for storage management
 *      agent consumption.  See also
 *      scsi_device.c:SCSIGetBlockDeviceInfo() and PR#188850 for
 *      further explanation.  This can be removed if VMware drops
 *      support for COS-based ESX or drops support for unmodified 3rd
 *      party Linux storage management agents running in the COS.
 *
 * Results:
 *      device status
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
static uint32_t
LinuxBlockEVPDPageC0Command(LinuxBlockAdapter *dev, vmk_ScsiCommand *cmd,
                            LinuxBlockDisk *disk)
{
   LinuxBlockPageC0Response response;

   response.devClass = disk->cdrom ? VMK_SCSI_CLASS_CDROM :
      VMK_SCSI_CLASS_DISK;
   response.pQual = VMK_SCSI_PQUAL_CONNECTED;
   response.pageCode = LINUX_BLOCK_EVPD_PAGE_CODE;
   response.reserved1 = 0;
   response.pageLength = sizeof(response) - 4;
   response.magic = LINUX_BLOCK_INFO_MAGIC;
   response.channelId = 0;
   response.targetId = disk->targetId;
   response.lunId = 0;
   VMK_ASSERT_ON_COMPILE(sizeof(response.adapter) ==
                         sizeof(dev->adapter->name));
   memcpy(response.adapter, vmk_NameToString(&dev->adapter->name),
          sizeof(dev->adapter->name));
   if (vmk_SgCopyTo(cmd->sgArray, &response, sizeof(response)) == VMK_OK) {
      cmd->bytesXferred = sizeof(response);
   } else {
      return VMK_SCSI_DEVICE_BUSY;
   }
   return VMK_SCSI_DEVICE_GOOD;
}

/*
 *----------------------------------------------------------------------
 *
 * LinuxBlockCommand --
 *
 *      Handle a request to issue a SCSI command on this block device.
 *      This SCSI command is translated to a block device command.
 *
 * Results:
 *      VMK_OK
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static VMK_ReturnStatus
LinuxBlockCommand(void *clientData,
                  vmk_ScsiCommand *cmd,
                  void *deviceData)
{
   VMK_ReturnStatus status;
   LinuxBlockAdapter *dev = (LinuxBlockAdapter *)clientData;
   LinuxBlockDisk *disk = (LinuxBlockDisk *)deviceData;
   vmk_ScsiSenseDataSimple sense;
   vmk_ByteCount size;

   VMK_ASSERT((cmd->sgIOArray != NULL) ||
              (cmd->dataDirection == VMK_SCSI_COMMAND_DIRECTION_NONE) ||
              (cmd->requiredDataLen == 0));

   if (disk->targetId >= dev->maxDisks || !disk->exists) {
      LinuxBlockScheduleCompletion(cmd, VMK_SCSI_HOST_NO_CONNECT,
                                VMK_SCSI_DEVICE_GOOD);
      return VMK_OK;
   }

   size = vmk_ScsiGetSupportedCmdSenseDataSize();
   switch (cmd->cdb[0]) {
   case VMK_SCSI_CMD_READ16:
   case VMK_SCSI_CMD_WRITE16:
   case VMK_SCSI_CMD_READ10:
   case VMK_SCSI_CMD_WRITE10:
   case VMK_SCSI_CMD_READ6:
   case VMK_SCSI_CMD_WRITE6: {
      vmk_uint64 blockOffset;
      vmk_uint32 numBlocks, diskEndSector;
      int isRead;

      vmk_ScsiGetLbaLbc(cmd->cdb, cmd->cdbLen, VMK_SCSI_CLASS_DISK,
                        &blockOffset, &numBlocks);

      VMK_ASSERT(disk->cdrom | disk->capacityValid);
      diskEndSector = disk->capacity;

      /*
       * Size check isn't performed for CDROM as we don't know if a media
       * change has occured at this point. So, we just let the driver
       * figure it out.
       */
      if (unlikely((blockOffset + numBlocks > diskEndSector) &&
                   (!disk->cdrom))) {
         VMKLNX_WARN("%s past end of device on major %d - %u + %u > %u",
                     cmd->isReadCdb ? "READ" : "WRITE",
                     dev->major, (uint32_t) blockOffset, numBlocks, diskEndSector);
         LinuxBlockIllegalRequest(&sense, 1, 1);
         /* Use the additional sense length field for sense data length */
         size = min(size, (size_t)(sense.format.fixed.optLen + 8));
         vmk_ScsiCmdSetSenseData((vmk_ScsiSenseData *)&sense, cmd,
                                 min(size, sizeof(sense)));
         LinuxBlockScheduleCompletion(cmd, VMK_SCSI_HOST_OK,
                                   VMK_SCSI_DEVICE_CHECK_CONDITION);
         status = VMK_OK;
      } else {
         isRead = cmd->isReadCdb ? 1 : 0;
         status = LinuxBlockIssueCmd(dev, cmd, disk, blockOffset,
                                     numBlocks, isRead);
      }
      break;
   }

   case VMK_SCSI_CMD_READ_CAPACITY: {
      uint32_t sectors;

      /*
       * Try to get the capacity without blocking 
       */
      status = LinuxBlockGetCapacity(dev, disk, 1, &sectors);
      if (status == VMK_OK) {
         VMKLNX_DEBUG(3, "Servicing READ_CAPACITY request on %s "
                      "without blocking",
                      dev->devName);
         LinuxBlockCompleteCapacity(cmd, sectors);
      } else {
         VMKLNX_DEBUG(0, "READ_CAPACITY on %s failed: %s", dev->devName,
                      vmk_StatusToString(status));
         LinuxBlockScheduleCompletion(cmd, VMK_SCSI_HOST_ERROR,
                                   VMK_SCSI_DEVICE_GOOD);
         status = VMK_OK;
      }

      break;
   }

   case VMK_SCSI_CMD_INQUIRY: {
      vmk_ScsiInquiryCmd* inqCmd = (vmk_ScsiInquiryCmd*) cmd->cdb;
      vmk_ScsiHostStatus hostStatus = VMK_SCSI_HOST_OK;
      vmk_ScsiDeviceStatus deviceStatus = VMK_SCSI_DEVICE_GOOD;

      if (inqCmd->evpd || inqCmd->cmddt) {
         VMKLNX_DEBUG(2, "INQUIRY request with %s set",
                      inqCmd->evpd ? "EVPD" : "CmdDt");
         if (inqCmd->evpd && inqCmd->pagecode ==
             LINUX_BLOCK_EVPD_PAGE_CODE) {
            deviceStatus = LinuxBlockEVPDPageC0Command(dev, cmd, disk);
         } if (inqCmd->evpd && !disk->cdrom &&
               inqCmd->pagecode == LINUX_BLOCK_EVPD_PAGE_B1_CODE) {
            deviceStatus = LinuxBlockEVPDPageB1Command(dev, cmd, disk);
         } else {
            LinuxBlockIllegalRequest(&sense, 1, 1);
            /* Use the additional sense length field for sense data length */
            size = min(size, (size_t)(sense.format.fixed.optLen + 8));
            vmk_ScsiCmdSetSenseData((vmk_ScsiSenseData *)&sense, cmd,
                                    min(size, sizeof(sense)));
            deviceStatus = VMK_SCSI_DEVICE_CHECK_CONDITION;
         }
      } else {
         vmk_ScsiInquiryResponse inqResponse;

         memset(&inqResponse, 0, sizeof(inqResponse));
         inqResponse.pqual = VMK_SCSI_PQUAL_CONNECTED;
         if (unlikely(disk->cdrom)) {
            inqResponse.devclass = VMK_SCSI_CLASS_IDE_CDROM;
         } else {
            inqResponse.devclass = VMK_SCSI_CLASS_DISK;
         }
         inqResponse.ansi = VMK_SCSI_ANSI_SCSI2;

         /*
          * account two reserved bytes 
          */
         inqResponse.optlen += 2;

         inqResponse.rmb = VMK_FALSE;
         inqResponse.rel = VMK_FALSE;  // rel. addr. w/ linked cmds
         inqResponse.w32 = VMK_TRUE;   // 32-bit wide SCSI
         inqResponse.w16 = VMK_TRUE;   // 16-bit wide SCSI
         inqResponse.sync = VMK_TRUE;  // synchronous transfers
         inqResponse.link = VMK_FALSE; // linked commands (XXX not yet)
         inqResponse.que = VMK_TRUE;   // tagged commands
         inqResponse.sftr = VMK_TRUE;  // soft reset on RESET condition
         inqResponse.optlen += 2;

         VMK_ASSERT_ON_COMPILE(sizeof(inqResponse.manufacturer) <
                               sizeof(BLOCK_DEFAULT_VENDOR_STR));
         memcpy(inqResponse.manufacturer, BLOCK_DEFAULT_VENDOR_STR,
                sizeof(inqResponse.manufacturer));
         inqResponse.optlen += sizeof (inqResponse.manufacturer);

         VMK_ASSERT_ON_COMPILE(sizeof(inqResponse.product) <
                               sizeof(BLOCK_DEFAULT_MODEL_STR));
         memcpy(inqResponse.product, BLOCK_DEFAULT_MODEL_STR,
                sizeof (inqResponse.product));
         inqResponse.optlen += sizeof(inqResponse.product);

         VMK_ASSERT_ON_COMPILE(sizeof(inqResponse.revision) <
                               sizeof(BLOCK_DEFAULT_REVISION_STR));
         memcpy(inqResponse.revision, BLOCK_DEFAULT_REVISION_STR,
                sizeof(inqResponse.revision));
         inqResponse.optlen += sizeof(inqResponse.revision);

         if (vmk_SgCopyTo(cmd->sgArray, &inqResponse, sizeof(inqResponse)
                         ) == VMK_OK) {
            // The total lenth is the header size (4 bytes) + add. length
            cmd->bytesXferred = 4 + inqResponse.optlen;
         } else {
            hostStatus = VMK_SCSI_HOST_ERROR;
         }
      }
      LinuxBlockScheduleCompletion(cmd, hostStatus, deviceStatus);
      status = VMK_OK;
      break;
   }

   case VMK_SCSI_CMD_REQUEST_SENSE: {
      size_t len = min(sizeof(sense),
                       (size_t)vmk_SgGetDataLen(cmd->sgArray));
      vmk_ScsiHostStatus hostStatus = VMK_SCSI_HOST_OK;

      memset(&sense, 0, sizeof(sense));
      sense.format.fixed.valid = 1;
      sense.format.fixed.error = VMK_SCSI_SENSE_ERROR_CURCMD;
      sense.format.fixed.key = VMK_SCSI_SENSE_KEY_NONE;
      sense.format.fixed.optLen = 10;

      len = min(len, (size_t)(sense.format.fixed.optLen + 8));
      if (vmk_SgCopyTo(cmd->sgArray, &sense, len) == VMK_OK) {
         cmd->bytesXferred = len;
      } else {
         hostStatus = VMK_SCSI_HOST_ERROR;
      }

      LinuxBlockScheduleCompletion(cmd, hostStatus, VMK_SCSI_DEVICE_GOOD);
      status = VMK_OK;
      break;
   }
   case VMK_SCSI_CMD_RESERVE_UNIT:
   case VMK_SCSI_CMD_RELEASE_UNIT:
   case VMK_SCSI_CMD_FORMAT_UNIT:
   case VMK_SCSI_CMD_VERIFY:
   case VMK_SCSI_CMD_SYNC_CACHE:
   case VMK_SCSI_CMD_TEST_UNIT_READY:
   case VMK_SCSI_CMD_START_UNIT:
      LinuxBlockScheduleCompletion(cmd, VMK_SCSI_HOST_OK, VMK_SCSI_DEVICE_GOOD);
      status = VMK_OK;
      break;
   case VMK_SCSI_CMD_MODE_SENSE:
      LinuxBlockIllegalRequest(&sense, 1, 2);
      /* Use the additional sense length field for sense data length */
      size = min(size, (size_t)(sense.format.fixed.optLen + 8));
      vmk_ScsiCmdSetSenseData((vmk_ScsiSenseData *)&sense, cmd,
                              min(size, sizeof(sense)));
      LinuxBlockScheduleCompletion(cmd, VMK_SCSI_HOST_OK,
                                VMK_SCSI_DEVICE_CHECK_CONDITION);
      status = VMK_OK;
      break;
   case VMK_SCSI_CMD_READ_BUFFER:
   case VMK_SCSI_CMD_WRITE_BUFFER:
   case VMK_SCSI_CMD_MEDIUM_REMOVAL:
      /*
       * do not log invalid opcode message for these commands
       */
      LinuxBlockInvalidOpcode(&sense, 1);
      /* Use the additional sense length field for sense data length */
      size = min(size, (size_t)(sense.format.fixed.optLen + 8));
      vmk_ScsiCmdSetSenseData((vmk_ScsiSenseData *)&sense, cmd,
                              min(size, sizeof(sense)));
      LinuxBlockScheduleCompletion(cmd, VMK_SCSI_HOST_OK,
                                VMK_SCSI_DEVICE_CHECK_CONDITION);
      status = VMK_OK;
      break;
   default:
      VMKLNX_DEBUG(2, "Invalid Opcode (0x%x) ", cmd->cdb[0]);
      LinuxBlockInvalidOpcode(&sense, 1);
      /* Use the additional sense length field for sense data length */
      size = min(size, (size_t)(sense.format.fixed.optLen + 8));
      vmk_ScsiCmdSetSenseData((vmk_ScsiSenseData *)&sense, cmd,
                              min(size, sizeof(sense)));
      LinuxBlockScheduleCompletion(cmd, VMK_SCSI_HOST_OK,
                                VMK_SCSI_DEVICE_CHECK_CONDITION);
      status = VMK_OK;
      break;
   }

   return status;
}

/*
 *----------------------------------------------------------------------
 *
 * LinuxBlockTaskMgmt --
 *
 *      Handle the task management requests issued on this block device.
 *
 * Results:
 *      VMK_OK
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
static VMK_ReturnStatus
LinuxBlockTaskMgmt(void *clientData,
                   vmk_ScsiTaskMgmt *vmkTaskMgmt,
                   void *deviceData)
{
#ifdef VMX86_LOG
   LinuxBlockAdapter *dev =  (LinuxBlockAdapter *)clientData;
   LinuxBlockDisk   *disk = (LinuxBlockDisk *)deviceData;

   VMK_ASSERT(disk->targetId < dev->maxDisks && disk->exists);
#endif

   switch (vmkTaskMgmt->type) {
   case VMK_SCSI_TASKMGMT_ABORT:
   case VMK_SCSI_TASKMGMT_LUN_RESET:
   case VMK_SCSI_TASKMGMT_DEVICE_RESET:
   case VMK_SCSI_TASKMGMT_BUS_RESET:
   case VMK_SCSI_TASKMGMT_VIRT_RESET:
      VMKLNX_DEBUG(0, "Ignoring %s initiator:%p S/N:%#"VMK_FMT64"x on "
                   "%s:C%u:T%u:L%u",
                   vmk_ScsiGetTaskMgmtTypeName(vmkTaskMgmt->type),
                   vmkTaskMgmt->cmdId.initiator,
                   vmkTaskMgmt->cmdId.serialNumber,
                   vmk_NameToString(&dev->adapter->name),
                   0,
                   disk->targetId,
                   0);
      break;
   default:
      VMKLNX_NOT_IMPLEMENTED();
   }

   return VMK_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * LinuxBlockDumpIODone --
 *
 *      Call back function for when dumping core and block IO is complete.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
static int
LinuxBlockDumpIODone(struct bio *bio, int bytesdone, int error)
{
   LinuxBlockBuffer *llb = (LinuxBlockBuffer *) bio->bi_private;

   llb->creq->errors = 0;

   if(error) {
      llb->creq->errors++;
      VMKLNX_DEBUG(0, "Errors! !OK");
   }

   if (llb->creq->errors) {
      llb->cmd->status.host = VMK_SCSI_HOST_ERROR;
      llb->cmd->status.device = VMK_SCSI_DEVICE_GOOD;
   } else {
      llb->cmd->status.host = VMK_SCSI_HOST_OK;
      llb->cmd->status.device = VMK_SCSI_DEVICE_GOOD;
   }

   LinuxBlockCompleteCommand(llb->cmd);

   return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * LinuxBlockDumpCommand --
 *
 *      Handle a request from SCSI_Dump to issue a SCSI write
 *      command on this block device.
 *      This SCSI command is translated to a block device command.
 *
 * Results:
 *      VMK_OK
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static VMK_ReturnStatus
LinuxBlockDumpCommand(void *clientData,
                      vmk_ScsiCommand *cmd,
                      void *deviceData)
{
   static struct request dumpcreq;
   static LinuxBlockBuffer dumpBlockBuffer;

   LinuxBlockAdapter *dev = (LinuxBlockAdapter *)clientData;
   LinuxBlockDisk *disk = (LinuxBlockDisk *)deviceData;

   vmk_ScsiAdapter *adapter;
   struct request *creq;
   request_queue_t *queue;
   vmk_MA addr;
   LinuxBlockBuffer *b;
   static struct bio biostruct;
   struct bio *bio;
   struct scatterlist *sg;
   struct block_device *bdev;
   int minor;
   uint32_t nr_sectors = 0;

   if (disk->targetId >= dev->maxDisks || !disk->exists) {
      return VMK_NOT_FOUND;
   }

   adapter = dev->adapter;

   /*
    * SCSI_OnPanic() can issue RELEASE_UNIT, so handle that here
    */
   if (cmd->cdb[0] == VMK_SCSI_CMD_RELEASE_UNIT) {
      LinuxBlockScheduleCompletion(cmd, VMK_SCSI_HOST_OK, VMK_SCSI_DEVICE_GOOD);
      return VMK_OK;
   }

   /*
    * SCSI_Dump may generate WRITE10 or WRITE16 commands
    */
   VMK_ASSERT(cmd->cdb[0] == VMK_SCSI_CMD_WRITE10 || cmd->cdb[0] == VMK_SCSI_CMD_WRITE16);
   VMK_ASSERT(cmd->sgArray->numElems == 1);
   VMK_ASSERT(cmd->sgArray->elem[0].length%SECTOR_SIZE == 0);

   b = &dumpBlockBuffer;
   memset(b, 0, sizeof(*b));

   addr = cmd->sgArray->elem[0].addr;

   minor = disk->targetId << dev->minor_shift;
   bdev = bdget(MKDEV(dev->major, minor));
   if (!bdev) {
      VMKLNX_WARN("No block device found at: major: %d, minor: %d", dev->major, minor);
      return VMK_NOT_FOUND;
   }

   bio = &biostruct;
   bio_init(bio);
   bio->bi_max_vecs = cmd->sgArray->numElems;
   sg = bio->vmksg;
   VMKLNX_INIT_VMK_SG_WITH_ARRAYS(sg, cmd->sgArray, cmd->sgIOArray);

   bio->bi_bdev = bdev;
   bio->bi_end_io = (bio_end_io_t *)LinuxBlockDumpIODone;
   bio->bi_private = b;
   bio->bi_rw = BIO_RW+1;
   bio->bi_phys_segments++;
   bio->bi_hw_segments++;
   bio->bi_size += cmd->sgArray->elem[0].length;

   if (cmd->cdb[0] == VMK_SCSI_CMD_WRITE10) {
      bio->bi_sector = be32_to_cpu(((vmk_ScsiReadWrite10Cmd *)cmd->cdb)->lbn);
      nr_sectors = be16_to_cpu(((vmk_ScsiReadWrite10Cmd *)cmd->cdb)->length);
   } else if (cmd->cdb[0] == VMK_SCSI_CMD_WRITE16) {
      bio->bi_sector = be64_to_cpu(((vmk_ScsiReadWrite16Cmd *)cmd->cdb)->lbn);
      nr_sectors = be32_to_cpu(((vmk_ScsiReadWrite16Cmd *)cmd->cdb)->length);
   }

   creq = &dumpcreq;

   b->creq = creq;
   b->lbio = bio;
   b->lastOne = VMK_TRUE;
   b->cmd = cmd;

   queue = bdev_get_queue(bio->bi_bdev);
   if (!queue) {
      VMKLNX_WARN("Trying to access a non-existent block device");
      return VMK_NOT_FOUND;
   }

   /*
    * init/prepare request struct
    */
   vmklnx_blk_rq_init(queue, creq);
   vmklnx_blk_init_request_from_bio(creq, bio);
   creq->nr_sectors = nr_sectors;

   spin_lock_irq(queue->queue_lock);

   minor = disk->targetId << dev->minor_shift;

   bio_get(bio);
   blk_plug_device(queue);
   add_request(queue, creq);

   spin_unlock_irq(queue->queue_lock);

   VMKLNX_DEBUG(5, "Appended request %p to major %d", creq, dev->major);
   if ( 1 || bio_sync(bio) ) {
      generic_unplug_device(queue, dev);
   }

   return VMK_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * LinuxBlockIoctl --
 *
 *      Ioctl command
 *
 * Results:
 *      VMK_INVALID_ADAPTER - couldn't find adapter.
 *      VMK_OK - driver ioctl succeeded.
 *      VMK_FAILURE - driver ioctl failed. In this and above, drvErr contains
 *      the driver ioctl retval.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static VMK_ReturnStatus
LinuxBlockIoctl(void *clientData,
                void *deviceData,
                uint32_t fileFlags,
                uint32_t cmd,
                vmk_VA userArgsPtr,
                vmk_IoctlCallerSize callerSize,
                int32_t *drvErr)
{
   struct inode *inode;
   struct file file;
   LinuxBlockAdapter *dev = (LinuxBlockAdapter *) clientData;
   LinuxBlockDisk *disk = (LinuxBlockDisk *) deviceData;
   int minor;
   int done;
   struct block_device* bdev;

   VMKLNX_DEBUG(2, "tid=%d, cmd=0x%x, uargs=%p flags=0x%x",
                disk->targetId, cmd,
                (void *)userArgsPtr, fileFlags);
   if (dev == NULL) {
      return VMK_INVALID_ADAPTER;
   }
   if (dev->ops == NULL || dev->ops->ioctl == NULL) {
      return VMK_NOT_SUPPORTED;
   }
   if ((inode = kzalloc(sizeof *inode, GFP_KERNEL)) == NULL) {
      return VMK_NO_MEMORY;
   }

   minor = disk->targetId << dev->minor_shift;
   inode->i_rdev = (dev->major << MINORBITS) | minor;
   inode->i_mode = S_IFBLK;

   bdev = bdget(inode->i_rdev);
   if (bdev == NULL) {
      kfree(inode);
      return VMK_NO_MEMORY;
   }

   bdev->bd_inode = inode;
   inode->i_bdev = bdev;
   /*
    * Create bdev-to-gendisk mapping 
    */
   if (!bdev->bd_disk) {
      bdev->bd_disk = disk->gd;
   }

   memset(&file, '\0', sizeof file);
   file.f_flags = fileFlags;

   vmk_SemaLock(&blkDrvSem);
   done = 0;
   if (callerSize == VMK_IOCTL_CALLER_32 && dev->ops->compat_ioctl) {
      VMKAPI_MODULE_CALL(BLOCK_GET_ID(dev), *drvErr, dev->ops->compat_ioctl,
                         &file, cmd, (unsigned long) userArgsPtr);
      if (*drvErr == -ENOIOCTLCMD) {
         done = 1;
      }
   }
   if (!done) {
      VMKAPI_MODULE_CALL(BLOCK_GET_ID(dev), *drvErr, dev->ops->ioctl,
                         inode, &file, cmd, (unsigned long) userArgsPtr);
   }
   vmk_SemaUnlock(&blkDrvSem);

   VMKLNX_DEBUG(2, "ioctl ret=%d", *drvErr);
   kfree(inode);
   return VMK_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * LinuxBlockAdapterGetQueueDepth
 *
 *     This is called to get queue depth on a device. Block interfaces
 * dont advertise any such limits, so return the adapter queue depth here
 *
 * Result:
 *     This returns the queue depth
 *
 * Side effects:
 *     None
 *----------------------------------------------------------------------
 */
static int
LinuxBlockAdapterGetQueueDepth(void *clientData,
                               void *deviceData)
{
   return MAX_TARG_CMDS;
}

/*
 *----------------------------------------------------------------------
 *
 * LinuxBlockAdapterModifyQueueDepth
 *
 *     This is called to get queue depth on a device. Block interfaces
 * dont advertise any such limits, so return the adapter queue depth here
 *
 * Result:
 *     This returns the queue depth
 *
 * Side effects:
 *     None
 *----------------------------------------------------------------------
 */
static int
LinuxBlockAdapterModifyQueueDepth(void *clientData,
                                  int qDepth,
                                  void *deviceData)
{
   return MAX_TARG_CMDS;
}

/*
 *----------------------------------------------------------------------
 *
 * LinuxBlockClose --
 *
 *      Close this block device.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
static void
LinuxBlockClose(void *clientData)
{
}

/*
 *----------------------------------------------------------------------
 *
 * LinuxBlockDumpQueue --
 *
 *      Stub for Linux block command queue dumping.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
static void
LinuxBlockDumpQueue(void *clientData)
{
}

/**
 *  vmklnx_block_register_ssd - mark the block device as SSD
 *  @major: major number of the device
 *  @targetNum: target number of the disk
 *
 *  Marks the disk device as SSD.
 *
 *  RETURN VALUE:
 *  None.
 *
 */
/* _VMKLNX_CODECHECK_: vmklnx_block_register_ssd */
void
vmklnx_block_register_ssd(vmk_uint32 major,
                          int targetNum)
{
   LinuxBlockAdapter *ba = blockDevices[major];
   LinuxBlockDisk *bd;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if (major >= MAX_BLKDEV) {
      VMKLNX_WARN("Major number %d is too high", major);
      return;
   } else if (ba == NULL) {
      VMKLNX_WARN("Major number %d not defined", major);
      return;
   }

   if (targetNum >= ba->maxDisks) {
      VMKLNX_WARN("Device %s - Target %d is too high", ba->devName, targetNum);
      return;
   }

   bd = &ba->disks[targetNum];
   if (!bd->exists) {
      VMKLNX_WARN("Device %s Target %d does not exist", ba->devName, targetNum);
      return;
   }

   if (bd->cdrom) {
      VMKLNX_WARN("Device %s Target %d is not a disk device",
                  ba->devName, targetNum);
      return;
   }

   bd->isSSD = VMK_TRUE;
   VMKLNX_DEBUG(0, "Device %s Target %d registered as SSD device",
                ba->devName, targetNum);
   return;
}
EXPORT_SYMBOL(vmklnx_block_register_ssd);

/**
 *  vmklnx_block_register_sglimit - set the sglimit to the block device
 *  @major: major number of the device
 *  @sgSize: number of scatter/gather entries
 *
 *  Limits the upper bound value of sfSize to MAX_SEGMENTS,
 *  and stores the sgSize value in the adapter structure of the device.
 *
 *  RETURN VALUE:
 *  None.
 *
 */
/* _VMKLNX_CODECHECK_: vmklnx_block_register_sglimit */
void
vmklnx_block_register_sglimit(vmk_uint32 major, int sgSize)
{
   vmk_ScsiAdapter *adapter = NULL;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if (major >= MAX_BLKDEV) {
      VMKLNX_WARN("Major number %d is too high", major);
      return;
   } else if (blockDevices[major] == NULL) {
      VMKLNX_WARN("Major number %d not defined", major);
      return;
   }
   adapter = blockDevices[major]->adapter;

   if (sgSize < MAX_SEGMENTS) {
      adapter->constraints.sgMaxEntries = sgSize;
   }
   else {
      adapter->constraints.sgMaxEntries = MAX_SEGMENTS;
   }
   VMKLNX_DEBUG(3, "sgSize set to %d for major %d", sgSize, major);
}
EXPORT_SYMBOL(vmklnx_block_register_sglimit);

/**
 *  vmklnx_block_register_disk_maxXfer - set a block device's maxXfer
 *  @major: major number of the device
 *  @targetNum: target ID
 *  @maxXfer: maxXfer value of the device to set
 *
 *  Update maxXfer value of the underlying kernel SCSI path.
 *
 */
/* _VMKLNX_CODECHECK_: vmklnx_block_register_disk_maxXfer*/
static void
vmklnx_block_register_disk_maxXfer(vmk_uint32 major,
                                   int targetNum,
                                   vmk_uint64 maxXfer)
{
   vmk_ScsiAdapter *adapter = NULL;

   if (major >= MAX_BLKDEV) {
      VMKLNX_WARN("Major number %d is too high", major);
      return;
   } else if (blockDevices[major] == NULL) {
      VMKLNX_WARN("Major number %d not defined", major);
      return;
   }
   adapter = blockDevices[major]->adapter;

   if (maxXfer != 0) {
      VMKLNX_DEBUG(3, "Setting maxXfer for device %d / target %d to %#"VMK_FMT64"x",
                   major, targetNum, maxXfer);
      (void)vmk_ScsiSetPathXferLimit(adapter, 0, targetNum, 0, maxXfer);
   }
}

/**
 *  vmklnx_block_register_poll_handler - register poll handler with storage layer
 *  @major: major number of the device
 *  @irq: irq to register
 *  @handler: irq handler to register
 *  @dev_id: callback data for handler
 *
 *  Registers poll handler with storage layer, which is used during coredump
 *
 *  RETURN VALUE
 *  None
 */
/* _VMKLNX_CODECHECK_: vmklnx_block_register_poll_handler */
void vmklnx_block_register_poll_handler(vmk_uint32 major,
                                        unsigned int irq,
                                        irq_handler_t handler,
                                        void *dev_id)
{
   VMK_ReturnStatus result;
   vmk_ScsiAdapter *vmkAdapter;
   void *idr;
   struct device *dev = LinuxIRQ_GetDumpIRQDev();

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if (major >= MAX_BLKDEV) {
      VMKLNX_WARN("Major number %d is too high", major);
      return;
   } else if (blockDevices[major] == NULL) {
      VMKLNX_WARN("Major number %d not defined", major);
      return;
   }

   vmkAdapter = blockDevices[major]->adapter;

   if (!irq || irq >= NR_IRQS) {
      VMKLNX_WARN("Coredump Poll handler not registered for %s - invalid irq 0x%x",
                   vmk_ScsiGetAdapterName(vmkAdapter), irq);
      return;
   }

   idr = irqdevres_register(dev, irq, handler, dev_id);
   if (!idr) {
      VMKLNX_WARN("Coredump Poll handler not registered for %s - irqdevres_register failed",
                   vmk_ScsiGetAdapterName(vmkAdapter));
      VMK_ASSERT(0);
      return;
   }

   result = vmk_ScsiRegisterIRQ(vmkAdapter, LinuxIRQ_VectorToCookie(irq), Linux_IRQHandler, idr);
   VMK_ASSERT(result == VMK_OK);
   VMKLNX_DEBUG(0, "Coredump Poll handler registered for adpater %s with irq %d",
                    vmk_ScsiGetAdapterName(vmkAdapter), irq);
}
EXPORT_SYMBOL(vmklnx_block_register_poll_handler);

/**
 *  vmklnx_register_blkdev - register with blockDevice table
 *  @major: major number of the device
 *  @name: name of the adapter to be created
 *  @domain: domain this device is on
 *  @bus: bus this device is on
 *  @devfn: encoded device and function index
 *  @data: private data of the adapter
 *
 *  Registers the block device with blockDevice table. A valid @major has a
 *  value ranging from 1 - (MAX_BLKDEV - 1) and an @major of 0 is used for
 *  requesting a dynamic major.
 *
 *  ESX Deviation Notes:
 *  A slightly modified version of the Linux register_blkdev call.
 *  This interface takes additional arguments that we pass to
 *  the ESX SCSI device registration code.
 *
 *  vmklnx_register_blkdev(major, name, bus, devfn, data)
 *
 *  RETURN VALUE:
 *  0, if a valid major number is passed and available
 *  major, if a dynamic major is requested and available
 *  errno, otherwise
 *
 */
/* _VMKLNX_CODECHECK_: vmklnx_register_blkdev */
int
vmklnx_register_blkdev(vmk_uint32 major,
                       const char *name,
                       int domain,
                       int bus,
                       int devfn,
                       void *data)
{
   vmk_Name adapterName;
   int retry = 1;
   vmk_ScsiAdapter *adapter;
   VMK_ReturnStatus status;
   vmk_BlockAdapter *pBlockAdapter;
   vmk_ModuleID moduleID = vmk_ModuleStackTop();
   struct pci_dev *pdev;
   vmk_PCIDevice vmkDev;
   int retval = 0;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMKLNX_DEBUG(2, "Registering device %s", name);

   /*
    * if major is zero, we auto generate a major number for the device, if not
    * we see if the major is a valid major and if yes, we see if the major is
    * already in use.
    *
    * TODO: make this function and others in the register/unregister paths thread-safe.
    */

   if (0 == major) {
      vmk_uint32 assignedMajor;

      for(assignedMajor=MAX_BLKDEV - 1; assignedMajor > 0; assignedMajor--) {
         if (blockDevices[assignedMajor] == NULL) {
            break;
         }
      }
      if (assignedMajor == 0) {
         VMKLNX_WARN("Unable to allocate a dynamic major number for the device %s", name);
         return -EBUSY;
      } 
      major = assignedMajor;
      VMKLNX_DEBUG(0, "Assigning major number: %u to the block device: %s", major, name);
      retval = assignedMajor;
   } else if (major >= MAX_BLKDEV) {
      VMKLNX_WARN("Major number %d is too high", major);
      return -EINVAL;
   } else if (blockDevices[major] != NULL) {
      VMKLNX_WARN("Already have adapter at major number %d", major);
      return -EBUSY;
   }

   blockDevices[major] = (LinuxBlockAdapter *)kzalloc(sizeof(LinuxBlockAdapter), GFP_KERNEL);
   if (blockDevices[major] == NULL) {
      VMKLNX_WARN("No memory for block device struct");
      return -ENOMEM;
   }

   pBlockAdapter = (vmk_BlockAdapter *)kzalloc(sizeof(vmk_BlockAdapter), GFP_KERNEL);
   if (!pBlockAdapter) {
      VMKLNX_WARN("No memory for block adapter struct");
      goto unrollBlockDevice;
   }

   pdev = LinuxPCI_FindDevByBusSlot(domain, bus, devfn);
   SCSILinuxNameAdapter(&adapterName, pdev);
   VMKLNX_DEBUG(0, "Device at " PCI_DEVICE_BUS_ADDRESS " assigned name %s",
                pci_domain_nr(pdev->bus), bus, PCI_SLOT(devfn), PCI_FUNC(devfn),
                vmk_NameToString(&adapterName));

   vmkDev = NULL;
   status = vmk_PCIGetPCIDevice(domain, bus, PCI_SLOT(devfn),
                                PCI_FUNC(devfn), &vmkDev);
   VMK_ASSERT((status == VMK_OK && vmkDev != NULL) ||
              (status != VMK_OK && vmkDev == NULL));

retry:
   adapter = vmk_ScsiAllocateAdapter();
   if (adapter == NULL) {
      char pciAlternateName[VMK_MISC_NAME_MAX];

      /*
       *  PCI domain:bus:slot:fn based naming is inadequate for IDE
       *  devices as a single "function" could have multiple
       *  ports and they would all have the same PCI device name.
       */
      if (vmkDev != NULL) {
         status = vmk_PCIGetAlternateName(vmkDev,
                                          pciAlternateName,
                                          VMK_MISC_NAME_MAX);
         if (status == VMK_OK) {
            vmk_NameInitialize(&adapterName, pciAlternateName);
         }
      }

      if (retry && status == VMK_OK) {
         retry = 0;
         goto retry;
      }

      VMKLNX_WARN("Could not create " PCI_DEVICE_BUS_ADDRESS " with name %s",
                  pci_domain_nr(pdev->bus), bus, PCI_SLOT(devfn), PCI_FUNC(devfn),
                  vmk_NameToString(&adapterName));
      goto unrollBlockAdapter;
   }

   if (vmkDev != NULL) {
      status = vmk_PCIGetGenDevice(vmkDev, &adapter->device);
      VMK_ASSERT(status == VMK_OK);
   }
   adapter->discover = LinuxBlockDiscover;
   adapter->command = LinuxBlockCommand;
   adapter->taskMgmt = LinuxBlockTaskMgmt;
   adapter->dumpCommand = LinuxBlockDumpCommand;
   adapter->close = LinuxBlockClose;
   adapter->dumpQueue = LinuxBlockDumpQueue;
   adapter->dumpPollHandlerData = LINUX_BHHANDLER_NO_IRQS;
   adapter->dumpPollHandler = LinuxBlockBH;
   adapter->ioctl = LinuxBlockIoctl;
   adapter->modifyDeviceQueueDepth = LinuxBlockAdapterModifyQueueDepth;
   adapter->queryDeviceQueueDepth = LinuxBlockAdapterGetQueueDepth;
   adapter->paeCapable =  LinuxPCI_DeviceIsPAECapable(
                                LinuxPCI_FindDevByBusSlot(domain, bus, devfn));

   /*
    * The following fields are in block_init_done(), after the
    * driver has had a chance to change the default value:
    *
    * adapter->channels
    * adapter->maxTargets
    * adapter->maxLUNs
    * adapter->qDepthPtr
    * adapter->targetId
    */

   VMK_ASSERT_ON_COMPILE(SECTOR_SIZE == VMK_SECTOR_SIZE);
   adapter->hostMaxSectors = BLK_MAX_SECTORS;

   adapter->constraints.addressMask = adapter->paeCapable ?
                                         VMK_ADDRESS_MASK_64BIT :
                                         VMK_ADDRESS_MASK_32BIT;
   adapter->constraints.maxTransfer = (vmk_ByteCount)-1;
   adapter->constraints.sgMaxEntries = MAX_SEGMENTS;
   adapter->constraints.sgElemSizeMult = SECTOR_SIZE;
   adapter->constraints.sgElemAlignment = 0;
   adapter->constraints.sgElemStraddle = DMA_32BIT_MASK + 1;

   VMK_ASSERT(strlen(vmk_NameToString(&adapterName)) < sizeof(adapter->name));
   vmk_NameFormat(&adapter->name, "%s", vmk_NameToString(&adapterName));
   adapter->moduleID = moduleID;
   adapter->clientData = blockDevices[major];
   status = VMK_OK;
   
   max_sectors[major] = NULL;

   VMK_ASSERT(strlen(name) < VMK_DEVICE_NAME_MAX_LENGTH);
   strlcpy(blockDevices[major]->devName, name, VMK_DEVICE_NAME_MAX_LENGTH);
   blockDevices[major]->adapter = adapter;
   blockDevices[major]->data = data;
   blockDevices[major]->major = major;
   blockDevices[major]->minor_shift = -1;

   adapter->mgmtAdapter.t.block = pBlockAdapter;
   adapter->mgmtAdapter.transport = VMK_STORAGE_ADAPTER_BLOCK;

   VMKLNX_DEBUG(2, "Device %s registered", name);

   return retval;

unrollBlockAdapter:
   kfree(pBlockAdapter);

unrollBlockDevice:
   kfree(blockDevices[major]);
   blockDevices[major] = NULL;

   return -ENOMEM;
}
EXPORT_SYMBOL(vmklnx_register_blkdev);

/**
 *  unregister_blkdev - unregister a block device
 *  @major: major device number of device to be unregistered
 *  @name: name of device to be unregistered
 *
 *  ESX Deviation Notes:
 *  ESX version of this function also removes all paths to this
 *  device.
 *  Return value:
 *  0 upon on success, 1 on failure.
 */
/* _VMKLNX_CODECHECK_: unregister_blkdev */
int
unregister_blkdev(unsigned int major, const char *name)
{
   LinuxBlockAdapter *bd = blockDevices[major];
   LinuxCapacityRequest req;
   VMK_ReturnStatus status;
   int i;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMKLNX_DEBUG(0, "Unregistering device %s.", name);

   if (bd == NULL) {
      VMKLNX_WARN("No such device: major=%d name=%s", major, name);
      return 1;
   }

   VMKLNX_DEBUG(0, "Destroying scsi device %s.", name);

   for (i = 0; i < bd->maxDisks; i++) {
      if (bd->disks[i].exists) {
         int r;
         VMK_ASSERT(bd->minor_shift != -1);
         /* modUnload = TRUE because unregister_blkdev happens only on
          * unload; block devices don't support rescan.
          */
         r = vmk_ScsiRemovePath(bd->adapter, 0,
                                bd->disks[i].targetId, 0);
         if (r == VMK_FALSE) {
            VMKLNX_WARN("Cannot remove target %d lun %d",
                          bd->disks[i].targetId, 0);
            return 1;
         }
      }
   }

   if (bd->disks) {
      kfree(bd->disks);
   }

   status = vmk_ScsiUnregisterAdapter(bd->adapter);
   VMKLNX_ASSERT_NOT_IMPLEMENTED(status == VMK_OK);

   req.exit=VMK_TRUE;

   /* Free up the block mgmt Structure */
   bd->adapter->mgmtAdapter.transport = VMK_STORAGE_ADAPTER_TRANSPORT_UNKNOWN;
   kfree(bd->adapter->mgmtAdapter.t.block);

   vmk_ScsiFreeAdapter(bd->adapter);
   kfree(bd);
   blockDevices[major] = NULL;

   VMKLNX_DEBUG(2, "Device %s unregistered.", name);

   return 0;
}
EXPORT_SYMBOL(unregister_blkdev);

/**
 *  blk_queue_max_hw_segments - set max hw segments for a request for this queue
 *  @q: the request queue for the device
 *  @max_segments: max number of segments
 *
 *  Enables a low level driver to set an upper limit on the number of hw data
 *  segments in a request. This would be the largest number of address/length
 *  pairs the host adapter can actually give as once to the device.
 *
 *  RETURN VALUE:
 *  None
 *
 */
/* _VMKLNX_CODECHECK_: blk_queue_max_hw_segments */
void
blk_queue_max_hw_segments(request_queue_t *q, unsigned short max_segments)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if (!max_segments) {
      max_segments = 1;
      VMKLNX_DEBUG(4, "set to minimum %d", max_segments);
   }
   q->max_hw_segments = max_segments;
}
EXPORT_SYMBOL(blk_queue_max_hw_segments);

/**
 *  blk_queue_softirq_done - set softirq done callback for the queue
 *  @q: the reuest queue for the device
 *  @fn: softirq done function
 *
 *  Sets softirq done callback function of the @q with @fn.
 *
 *  RETURN VALUE:
 *  None
 *
 */
/* _VMKLNX_CODECHECK_: blk_queue_softirq_done */
void
blk_queue_softirq_done(request_queue_t *q, softirq_done_fn *fn)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   q->softirq_done_fn = fn;
}
EXPORT_SYMBOL(blk_queue_softirq_done);

/**
 *  blk_queue_hardsect_size - set hardware sector size for the queue
 *  @q:  the request queue for the device
 *  @size:  the hardware sector size, in bytes
 *
 *  This should typically be set to the lowest possible sector size
 *  that the hardware can operate on (possible without reverting to
 *  even internal read-modify-write operations). Usually the default
 *  of 512 covers most hardware.
 *
 *  RETURN VALUE:
 *  None
 *
 */
/* _VMKLNX_CODECHECK_: blk_queue_hardsect_size */
void
blk_queue_hardsect_size(request_queue_t *q, unsigned short size)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   q->hardsect_size = size;
}
EXPORT_SYMBOL(blk_queue_hardsect_size);

/**
 *  blk_queue_max_phys_segments - set max phys segments for a request for this queue
 *  @q:  the request queue for the device
 *  @max_segments:  max number of segments
 *
 *  Enables a low level driver to set an upper limit on the number of physical
 *  data segments in a request.  This would be the largest sized scatter list
 *  the driver could handle.
 *
 *  RETURN VALUE:
 *  None
 *
 */
/* _VMKLNX_CODECHECK_: blk_queue_max_phys_segments */
void
blk_queue_max_phys_segments(request_queue_t *q, unsigned short max_segments)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if (!max_segments) {
      max_segments = 1;
      VMKLNX_DEBUG(4, "set to minimum %d", max_segments);
   }

   q->max_phys_segments = max_segments;
}
EXPORT_SYMBOL(blk_queue_max_phys_segments);

/**
 *  put_disk - non-operational function
 *  @disk: Ignored
 *
 *  Description:
 *
 *  A non-operational function provided to help reduce kernel ifdefs. It is not
 *  supported in this release of ESX.
 *
 *  ESX Deviation Notes:
 *  A non-operational function provided to help reduce kernel ifdefs. It is not
 *  supported in this release of ESX.
 *
 */
/* _VMKLNX_CODECHECK_: put_disk */
void
put_disk(struct gendisk *disk)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
}
EXPORT_SYMBOL(put_disk);

/**
 *  vmklnx_block_init_start -  non-operation function
 *
 *  Description:
 *
 *  A non-operational function provided to help reduce kernel ifdefs. It is not
 *  supported in this release of ESX.
 *
 *  ESX Deviation Notes:
 *  A non-operational function provided to help reduce kernel ifdefs. It is not
 *  supported in this release of ESX.
 */
/* _VMKLNX_CODECHECK_: vmklnx_block_init_start */
void
vmklnx_block_init_start(void)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
}
EXPORT_SYMBOL(vmklnx_block_init_start);

/**
 *  vmklnx_block_init_done - complete block driver initialization
 *  @major: major number of the device
 *
 *  Stores the "max_sectors" value, if the driver set it during
 *  intialization. Also registers the adapter with the VMkernel.
 *
 *  RETURN VALUE:
 *  None.
 */
/* _VMKLNX_CODECHECK_: vmklnx_block_init_done */
void
vmklnx_block_init_done(int major)
{
   int minor, status;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   LinuxBlockAdapter *devp = blockDevices[major];
   if (devp && devp->adapter) {
      /*
       * Finish initializing the adapter
       */
      devp->adapter->channels = 1;
      devp->adapter->maxTargets = devp->maxDisks;
      devp->adapter->maxLUNs = 1;
      devp->adapter->maxCmdLen = 16;
      devp->adapter->qDepthPtr = &maxCtlrCmds;
      devp->adapter->targetId = devp->maxDisks + 1;

      if (max_sectors[major]) {
         for (minor = 0; minor < 256; minor++) {
            if (max_sectors[major][minor] > 0) {
               vmklnx_block_register_disk_maxXfer(
                   major,
                   minor,
                   ((vmk_uint64)max_sectors[major][minor]) * VMK_SECTOR_SIZE);
            }
         }
      }

      devp->adapter->flags |= VMK_SCSI_ADAPTER_FLAG_NO_PERIODIC_SCAN;

      status = vmk_ScsiRegisterAdapter(devp->adapter);
      if (status != VMK_OK) {
         VMKLNX_WARN("vmk_ScsiRegisterAdapter Failed: %s", vmk_StatusToString(status));
         if (devp->data != NULL) {
            vmk_ScsiFreeAdapter(devp->adapter);
         }
      }
   }
}
EXPORT_SYMBOL(vmklnx_block_init_done);
/**                                          
 *  alloc_disk - allocate and initialize a gendisk structure      
 *  @minors: the number of partitions plus 1    
 *  Allocate and initialize a gendisk structure and the associated partition
 *  tables 
 *
 *  RETURN VALUE:
 *  The pointer to the newly allocated gendisk structure.                       
 *                                           
 */
/* _VMKLNX_CODECHECK_: alloc_disk */
struct gendisk *
alloc_disk(int minors)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   return alloc_disk_node(minors, -1);
}
EXPORT_SYMBOL(alloc_disk);

struct gendisk *
alloc_disk_node(int minors, int node_id)
{
   struct gendisk *disk;

   disk = kmalloc_node(sizeof(struct gendisk), GFP_KERNEL, node_id);
   if (disk) {
      memset(disk, 0, sizeof(struct gendisk));
      if (minors > 1) {
         int size = (minors - 1) * sizeof(struct hd_struct *);
         disk->part = kmalloc_node(size, GFP_KERNEL, node_id);
         if (!disk->part) {
            kfree(disk);
            return NULL;
         }
         memset(disk->part, 0, size);
      }
      disk->minors = minors;
   }
   return disk;
}

/**
 *  blk_queue_max_segment_size - set max segment size for blk_rq_map_sg
 *  @q:  the request queue for the device
 *  @max_size:  max size of segment in bytes
 *
 *  Enables a low level driver to set an upper limit on the size of a
 *  coalesced segment
 *
 *  RETURN VALUE:
 *  None
 *
 */
/* _VMKLNX_CODECHECK_: blk_queue_max_segment_size */
void
blk_queue_max_segment_size(request_queue_t *q, unsigned int max_size)
{
   if(max_size < PAGE_CACHE_SIZE) {
      max_size = PAGE_CACHE_SIZE;
      VMKLNX_DEBUG(4, "set to minimum %d", max_size);
   }

   q->max_segment_size = max_size;
}


/*
 * remove the plug and let it rip..
 */
void
__generic_unplug_device(request_queue_t *q, void *data)
{
   LinuxBlockAdapter *dev = (LinuxBlockAdapter*) data;
   if (unlikely(blk_queue_stopped(q))) {
      return;
   }

   if (!blk_remove_plug(q)) {
      return;
   }

   VMKLNX_DEBUG(2, "Calling request function %p for major %d",
                q->request_fn, dev->major);

   VMKAPI_MODULE_CALL_VOID(BLOCK_GET_ID(dev), q->request_fn, q);
}

/*
 * generic_unplug_device - fire a request queue
 * @q:    The &request_queue_t in question
 *
 * Description:
 *   Linux uses plugging to build bigger requests queues before letting
 *   the device have at them. If a queue is plugged, the I/O scheduler
 *   is still adding and merging requests on the queue. Once the queue
 *   gets unplugged, the request_fn defined for the queue is invoked and
 *   transfers started.
 */
void
generic_unplug_device(request_queue_t *q, void *data)
{
   spin_lock_irq(q->queue_lock);
   __generic_unplug_device(q, data);
   spin_unlock_irq(q->queue_lock);
}

/**
 * blk_queue_segment_boundary - set boundary rules for segment merging
 * @q:  the request queue for the device
 * @mask:  the memory boundary mask
 **/
void
blk_queue_segment_boundary(request_queue_t *q, unsigned long mask)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if (mask < PAGE_CACHE_SIZE - 1) {
      mask = PAGE_CACHE_SIZE - 1;
      VMKLNX_DEBUG(4, "set to minimum %lx", mask);
   }
   q->seg_boundary_mask = mask;
}
EXPORT_SYMBOL(blk_queue_segment_boundary);

request_queue_t *
blk_alloc_queue_node(gfp_t gfp_mask, int node_id)
{
   request_queue_t *q;

   q = kmem_cache_alloc_node(requestq_cachep, gfp_mask, node_id);
   if (!q)
      return NULL;

   memset(q, 0, sizeof(*q));

   return q;
}

/**
 *  blk_init_queue - prepare a request queue for use with a block device
 *  @rfn: The function to be called to process requests that have been placed on the queue
 *  @lock: Request queue spin lock
 *
 *  Prepares a request queue for use with a block device. The function @rfn will be called
 *  when there are requests on the queue that need to be proceesed.
 *  @rfn is not required, or even expected, to remove all requests off the queue, but only
 *  as many as it can handle at a time. If it does leave requests on the queue, it is
 *  responsible for arranging that the requests get dealt with eventually.
 *  The queue spin lock must be held while manipulating the requests on the request queue;
 *  this lock will be taken also from interrupt context, so irq disabling is needed for it.
 *
 *  Note, blk_init_queue must be paired with a blk_cleanup_queue call when the block device
 *  is deactivated (such as at module unload).
 *
 *  RETURN VALUE:
 *  Pointer to the initialized request queue, or NULL if it didn't succeed.
 *
 *  SEE ALSO:
 *  blk_cleanup_queue
 *
 */
/* _VMKLNX_CODECHECK_: blk_init_queue */
request_queue_t *
blk_init_queue(request_fn_proc *rfn, spinlock_t *lock)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   return blk_init_queue_node(rfn, lock, -1);
}
EXPORT_SYMBOL(blk_init_queue);

void
blk_put_queue(request_queue_t *q)
{
   /*
    * we are not using kobj stuff here to release
    * resources automatically, so do it here manually
    */
   struct request_list *rl = &q->rq;

   if (rl->rq_pool) {
      mempool_destroy(rl->rq_pool);
   }

   kmem_cache_free(requestq_cachep, q);
}

static int
blk_init_free_list(request_queue_t *q)
{
   struct request_list *rl = &q->rq;

   rl->count[READ] = rl->count[WRITE] = 0;
   rl->starved[READ] = rl->starved[WRITE] = 0;
   rl->elvpriv = 0;
   init_waitqueue_head(&rl->wait[READ]);
   init_waitqueue_head(&rl->wait[WRITE]);

   rl->rq_pool = mempool_create_node(BLKDEV_MIN_RQ, mempool_alloc_slab,
                                     mempool_free_slab, request_cachep, q->node);

   if (!rl->rq_pool) {
      return -ENOMEM;
   }

   return 0;
}

request_queue_t *
blk_init_queue_node(request_fn_proc *rfn, spinlock_t *lock, int node_id)
{
   request_queue_t *q = blk_alloc_queue_node(GFP_KERNEL, node_id);

   if (!q) {
      return NULL;
   }

   INIT_LIST_HEAD(&q->queue_head);
   q->node = node_id;
   if (blk_init_free_list(q)) {
      kmem_cache_free(requestq_cachep, q);
      return NULL;
   }

   /*
    * if caller didn't supply a lock, they get per-queue locking with
    * our embedded lock
    */
   if (!lock) {
      spin_lock_init(&q->__queue_lock);
      lock = &q->__queue_lock;
   }

   q->request_fn           = rfn;
   q->prep_rq_fn           = NULL;
   q->unplug_fn            = NULL;
   q->queue_flags          = (1 << QUEUE_FLAG_CLUSTER);
   q->queue_lock           = lock;
   q->make_request_fn      = NULL;

   blk_queue_segment_boundary(q, 0xffffffff);
   blk_queue_max_segment_size(q, MAX_SEGMENT_SIZE);
   blk_queue_max_hw_segments(q, MAX_HW_SEGMENTS);
   blk_queue_max_phys_segments(q, MAX_PHYS_SEGMENTS);

   q->sg_reserved_size = INT_MAX;

   VMKLNX_DEBUG(2, "request function %p", q->request_fn);
   return q;
}

/**
 *  elv_next_request - returns the next request struct in the given queue
 *  @q: request queue from which to obtain a request struct
 *
 *  Returns the next request struct from the given request queue, or
 *  NULL if there are no request structs available
 *
 *  RETURN VALUES:
 *  The next request struct (if available) from the given request queue,
 *  otherwise NULL if there are no request structs available
 */
/* _VMKLNX_CODECHECK_: elv_next_request */
struct request *
elv_next_request(request_queue_t *q)
{
   struct request *rq;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if (list_empty(&q->queue_head)) {
      VMKLNX_DEBUG(3, "empty list: %p", q);
      return NULL;
   }

   rq = list_entry_rq(q->queue_head.next);
   return rq;
}
EXPORT_SYMBOL(elv_next_request);

void
elv_dequeue_request(request_queue_t *q, struct request *rq)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMK_ASSERT(!list_empty(&rq->queuelist));

   list_del_init(&rq->queuelist);

   if(blk_account_rq(rq)) {
      q->in_flight++;
   }
}
EXPORT_SYMBOL(elv_dequeue_request);

/**
 *  blk_start_queue - restart a previously stopped queue
 *  @q: The &request_queue_t in question
 *
 *  blk_start_queue() will clear the stop flag on the queue, and call
 *  the request_fn for the queue if it was in a stopped state when
 *  entered. Queue lock must be held. 
 *
 *  RETURN VALUE:
 *  None
 *
 *  SEE ALSO:
 *  blk_stop_queue
 *
 */
/* _VMKLNX_CODECHECK_: blk_start_queue */
void
blk_start_queue(request_queue_t *q)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   WARN_ON(!irqs_disabled());

   clear_bit(QUEUE_FLAG_STOPPED, &q->queue_flags);
   blk_plug_device(q);
}
EXPORT_SYMBOL(blk_start_queue);

#define blk_add_trace_generic(q, rq, rw, what)  do { } while (0)

/*
 * "plug" the device if there are no outstanding requests: this will
 * force the transfer to start only after we have put all the requests
 * on the list.
 *
 * This is called with interrupts off and no requests on the queue and
 * with the queue lock held.
 */
void
blk_plug_device(request_queue_t *q)
{
   WARN_ON(!irqs_disabled());

   /*
    * don't plug a stopped queue, it must be paired with blk_start_queue()
    * which will restart the queueing
    */
   if (blk_queue_stopped(q)) {
      return;
   }

   if (!test_and_set_bit(QUEUE_FLAG_PLUGGED, &q->queue_flags)) {
      blk_add_trace_generic(q, NULL, 0, BLK_TA_PLUG);
   }
}

/*
 * remove the queue from the plugged list, if present. called with
 * queue lock held and interrupts disabled.
 */
int
blk_remove_plug(request_queue_t *q)
{
   WARN_ON(!irqs_disabled());

   if (!test_and_clear_bit(QUEUE_FLAG_PLUGGED, &q->queue_flags)) {
      return 0;
   }
   return 1;
}

/**
 *  blk_stop_queue - stop a queue
 *  @q: The &request_queue_t in question
 *
 *  The Linux block layer assumes that a block driver will consume all
 *  entries on the request queue when the request_fn strategy is called.
 *  Often this will not happen, because of hardware limitations (queue
 *  depth settings). If a device driver gets a 'queue full' response,
 *  or if it simply chooses not to queue more I/O at one point, it can
 *  call this function to prevent the request_fn from being called until
 *  the driver has signalled it's ready to go again. This happens by calling
 *  blk_start_queue() to restart queue operations. Queue lock must be held.
 *
 *  RETURN VALUE:
 *  None
 *
 *  SEE ALSO:
 *  blk_start_queue
 *
 */
/* _VMKLNX_CODECHECK_: blk_stop_queue */
void
blk_stop_queue(request_queue_t *q)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   blk_remove_plug(q);
   set_bit(QUEUE_FLAG_STOPPED, &q->queue_flags);
}
EXPORT_SYMBOL(blk_stop_queue);

/**
 *  register_blkdev -  non-operational function
 *  @major: ignored
 *  @name: ignored
 *
 *  A non-operational function provided to help reduce
 *  kernel ifdefs. It is not supported in this release of ESX.
 *
 *  ESX Deviation Notes:
 *  A non-operational function provided to help reduce
 *  kernel ifdefs. It is not supported in this release of ESX.
 *
 *  RETURN VALUE:
 *  Always return 0.
 *
 */
/* _VMKLNX_CODECHECK_: register_blkdev */
int
register_blkdev(unsigned int major, const char *name)
{
   /* TODO: hook this to vmk_register_blkdev */
   return 0;
}

/**
 *  blk_complete_request - complete a block request
 *  @req: pointer of request structure
 *
 *  Complete a block request.
 */
/* _VMKLNX_CODECHECK_: blk_complete_request */
void
blk_complete_request(struct request *req)
{
   uint64_t prevIRQL;
   __attribute__((unused)) unsigned long flags;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   BUG_ON(!req->q->softirq_done_fn);

   /*
    * Schedule the BH here
    */
   LinuxBlockBuffer *llb = (LinuxBlockBuffer *) req->bio->bi_private;

   prevIRQL = vmk_SPLockIRQ(&blockBHListLock);
   list_add_tail(&llb->requests, &linuxBHCompletionList);
   vmk_SPUnlockIRQ(&blockBHListLock, prevIRQL);
   vmk_BHSchedulePCPU(linuxBlockBHNum, vmk_GetPCPUNum());
}
EXPORT_SYMBOL(blk_complete_request);

static void vmklnx_blk_init_request_from_bio(struct request *rq, struct bio *bio)
{
   VMK_ASSERT(rq);
   VMK_ASSERT(rq->q);
   VMK_ASSERT(bio);

   rq->bio = rq->biotail = bio;
   rq->flags |= bio_data_dir(bio);
   rq->sector = rq->hard_sector = bio->bi_sector;
   rq->current_nr_sectors = rq->hard_cur_sectors = bio_cur_sectors(bio);
   rq->nr_phys_segments = bio_phys_segments(rq->q, bio);
   rq->nr_hw_segments = bio_hw_segments(rq->q, bio);
   rq->rq_disk = bio->bi_bdev->bd_disk;
}

static void vmklnx_blk_rq_init(struct request_queue *q, struct request *rq)
{
   VMK_ASSERT(q);
   VMK_ASSERT(rq);

   rq->q = q;
   rq->queuelist = q->queue_head;
   rq->rq_status = RQ_ACTIVE;
   rq->errors = 0;
   rq->waiting = NULL;
   rq->start_time = jiffies;
}

static inline void
vmklnx_blk_free_request(request_queue_t *q, struct request *rq)
{
   /*
    * make sure we attempt to free only the ones we alloced
    * note: dump requests are not alloced from slab pool
    */
   if (!(rq->flags & REQ_ALLOCED_FROM_SLABPOOL)) {
      return;
   }
   //VMK_ASSERT(rq->rq_status == RQ_INACTIVE);   
   mempool_free(rq, q->rq.rq_pool);
}

static inline struct request *
vmklnx_blk_alloc_request(request_queue_t *q, int rw, gfp_t gfp_mask)
{
   struct request *rq = mempool_alloc(q->rq.rq_pool, gfp_mask);

   if (!rq) {
      return NULL;
   }

   BUG_ON(rw != READ && rw != WRITE);

   vmklnx_blk_rq_init(q, rq);
   rq->flags = rw | REQ_ALLOCED_FROM_SLABPOOL;

   return rq;
}

int
kblockd_schedule_work(struct work_struct *work)
{
   return queue_work(kblockd_workqueue, work);
}

void
kblockd_flush(void)
{
   flush_workqueue(kblockd_workqueue);
}


/**
 *  bio_endio - end I/O on a bio
 *  @bio: bio
 *  @bytes_done: number of bytes completed
 *  @error: error, if any
 *
 *  bio_endio() will end I/O on @bytes_done number of bytes. This may be
 *  just a partial part of the bio, or it may be the whole bio. bio_endio()
 *  is the preferred way to end I/O on a bio, it takes care of decrementing
 *  bi_size and clearing BIO_UPTODATE on error. @error is 0 on success, and
 *  and one of the established -Exxxx (-EIO, for instance) error values in
 *  case something went wrong. None should call bi_end_io() directly on
 *  a bio unless they own it and thus know that it has an end_io function.
 *
 *  RETURN VALUE:
 *  None
 *
 */
/* _VMKLNX_CODECHECK_: bio_endio */
void
bio_endio(struct bio *bio, unsigned int bytes_done, int error)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if (error) {
      clear_bit(BIO_UPTODATE, &bio->bi_flags);
   }

   if (unlikely(bytes_done > bio->bi_size)) {
      VMKLNX_DEBUG(4, "want %u bytes done, only %u left",
             bytes_done, bio->bi_size);
      bytes_done = bio->bi_size;
   }

   bio->bi_size -= bytes_done;
   bio->bi_sector += (bytes_done >> 9);

   if (bio->bi_end_io)
   {
      VMKLNX_DEBUG(2, "callling bi_end_io, error: %d", error);
      bio->bi_end_io(bio, bytes_done, error);
   }
}
EXPORT_SYMBOL(bio_endio);

static inline LinuxBlockBuffer *
vmklnx_blk_get_lbb_from_bio(struct bio *bio)
{
   LinuxBlockBuffer *lbb;
   struct vmklnx_bio_set *bs = vmklnx_bio_set;
   void *p = bio;

   /* make sure this bio is alloc'd from slab pool */
   if ((BIO_POOL_IDX(bio) & 1) == 0) {
      VMKLNX_WARN("bio %p with pool id %lu not alloc'd from slab pool", bio, BIO_POOL_IDX(bio));
      VMK_ASSERT(0);
      return NULL;
   }

   /* lbb resides at the front of bio */
   VMK_ASSERT(bs->front_pad);
   lbb = p - bs->front_pad;
   
   /* initialize common ones */
   lbb->lbio = bio;
   lbb->lastOne = VMK_TRUE; // for now, always the last one

   return lbb;
}

void
bio_init(struct bio *bio)
{
   bio->bi_next = NULL;
   bio->bi_bdev = NULL;
   bio->bi_flags = 1 << BIO_UPTODATE;
   bio->bi_rw = 0;
   bio->bi_phys_segments = 0;
   bio->bi_hw_segments = 0;
   bio->bi_hw_front_size = 0;
   bio->bi_hw_back_size = 0;
   bio->bi_size = 0;
   bio->bi_max_vecs = 0;
   bio->bi_end_io = NULL;
   atomic_set(&bio->bi_cnt, 1);
   bio->bi_private = NULL;
}

static struct bio *
vmklnx_bio_alloc(gfp_t gfp_mask, int nr_iovecs, struct vmklnx_bio_set *bs)
{
   struct bio *bio;
   void *p;

   p = mempool_alloc(bs->bio_pool, gfp_mask);
   if (unlikely(!p)) {
      return NULL;
   }

   bio = p + bs->front_pad;
   bio_init(bio);

   /* we have only one bio pool, and so set bit 1 to indicate pool alloc */
   bio->bi_flags |= 1UL << BIO_POOL_OFFSET;

   if (likely(nr_iovecs)) {
      /* TODO: reddys revisit */
      bio->bi_max_vecs = nr_iovecs;
      bio->bi_phys_segments = bio->bi_hw_segments = nr_iovecs;
   }
   return bio;
}

static void 
vmklnx_bio_free(struct bio *bio, struct vmklnx_bio_set *bs)
{
   void *p;

   /* make sure this bio is alloc'd from slab pool */
   if ((BIO_POOL_IDX(bio) & 1) == 0) {
      VMKLNX_WARN("bio %p with pool id %lu not alloc'd from slab pool", bio, BIO_POOL_IDX(bio));
      VMK_ASSERT(0);
      return;
   }

   p = bio;
   p -= bs->front_pad;

   mempool_free(p, bs->bio_pool);
}

/*
 * default destructor for a bio allocated with vmk_bio_alloc()
 */
static inline void
vmklnx_bio_fs_destructor(struct bio *bio)
{
   vmklnx_bio_free(bio, vmklnx_bio_set);
}

static inline struct bio *
bio_alloc(gfp_t gfp_mask, int nr_iovecs)
{
   return vmklnx_bio_alloc(gfp_mask, nr_iovecs, vmklnx_bio_set);
}

static void vmklnx_bioset_free(struct vmklnx_bio_set *bs)
{
   if (bs->bio_pool) {
      mempool_destroy(bs->bio_pool);
      bs->bio_pool = NULL;
   }

   if (bs->bio_slab) {
      kmem_cache_destroy(bs->bio_slab);
      bs->bio_slab = NULL;
   }
   kfree(bs);
}

static struct vmklnx_bio_set *
vmklnx_bioset_create(unsigned int pool_size, unsigned int front_pad)
{
   struct vmklnx_bio_set *bs;
   unsigned int sz;

   bs = kmalloc(sizeof(*bs), GFP_KERNEL);
   if (!bs) {
      return NULL;
   }

   /*
    * created slab object will be cache aligned, make this value
    * word aligned.
    */   
   bs->front_pad = ALIGN(front_pad, sizeof(void *));
   sz = bs->front_pad + sizeof(struct bio);
   bs->bio_slab = kmem_cache_create("vmklnx bio slab", sz, 0, 
                                     SLAB_HWCACHE_ALIGN, NULL, NULL);
   if (!bs->bio_slab) {
      goto fail;
   }

   bs->bio_pool = mempool_create_slab_pool(pool_size, (struct kmem_cache *)bs->bio_slab);
   if (bs->bio_pool) {
      return bs; 
   }

fail:
   vmklnx_bioset_free(bs);
   return NULL;
}

/* from blktrace.c
 * no block trace, just a dummy that does nothing
 */

void
__blk_add_trace(struct blk_trace *bt, sector_t sector, int bytes,
                     int rw, u32 what, int error, int pdu_len, void *pdu_data)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
}
EXPORT_SYMBOL(__blk_add_trace);
