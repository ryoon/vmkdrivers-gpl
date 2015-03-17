/* **********************************************************
 * Copyright 1998, 2007-2010,2012 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * linux_char.c --
 *
 *      Linux character device emulation. Provides un/register_chrdev()
 *      functions and an ioctl dispatcher.
 */

#include <linux/module.h>
#include <linux/poll.h>
#include <linux/smp_lock.h>

#include "vmkapi.h"
#include "linux_stubs.h"
#include "linux/miscdevice.h"
#include "vmklinux_dist.h"
#include <vmkplexer_chardevs.h>

#define VMKLNX_LOG_HANDLE LinChar
#include "vmklinux_log.h"

typedef struct LinuxCharDev {
   int major;
   int minor;
   vmk_ModuleID modID;
   const struct file_operations *ops;
} LinuxCharDev;

typedef struct LinuxCharFileInfo {
   struct inode inode;
   struct file file;
   struct dentry dentry;
} LinuxCharFileInfo;

static VMK_ReturnStatus LinuxCharOpen(vmk_CharDevFdAttr *attr);
static VMK_ReturnStatus LinuxCharClose(vmk_CharDevFdAttr *attr);
static VMK_ReturnStatus LinuxCharIoctl(vmk_CharDevFdAttr *attr, 
                                       unsigned int cmd,
                                       vmk_uintptr_t userData,
                                       vmk_IoctlCallerSize callerSize, 
                                       vmk_int32 *result);
static VMK_ReturnStatus LinuxCharPoll(vmk_CharDevFdAttr *attr, 
                                      void *pollCtx,
                                      unsigned *pollMask);
static VMK_ReturnStatus LinuxCharRead(vmk_CharDevFdAttr *attr,
                                      char *buffer,
                                      vmk_ByteCount nbytes, 
                                      vmk_loff_t *ppos, 
                                      vmk_ByteCountSigned *nread);
static VMK_ReturnStatus LinuxCharWrite(vmk_CharDevFdAttr *attr, 
                                       char *buffer,
                                       vmk_ByteCount nbytes,
                                       vmk_loff_t *ppos, 
                                       vmk_ByteCountSigned *nwritten);

/*
 * vmkplxr_chardev_register requires that all members of
 * the file operations be present.
 */
static vmk_CharDevOps linuxCharDevOps = {
   LinuxCharOpen,
   LinuxCharClose,
   LinuxCharIoctl,
   LinuxCharPoll,
   LinuxCharRead,
   LinuxCharWrite,
};

// Per Alan Cox, 0x3 maps to no permission.  See PR 369282.
#define VMKLNX_CHAR_OFLAGS_TO_FMODE(__flags)			\
   (((__flags) & O_ACCMODE) + 1) & (FMODE_READ | FMODE_WRITE)

/*
 *-----------------------------------------------------------------------------
 *
 * LinuxCharOpen --
 *
 *      Open a vmkernel driver char device. 
 *      
 * Results:
 *      Driver return value wrapped in VMK_ReturnStatus. 
 *
 * Side effects:
 *      Calls driver open().
 *
 *-----------------------------------------------------------------------------
 */

VMK_ReturnStatus
LinuxCharOpen(vmk_CharDevFdAttr *attr) 
{
   vmkplxr_ChardevHandles *handles;
   LinuxCharDev *cdev;
   LinuxCharFileInfo *fileInfo;
   struct inode *inode;
   struct file  *file;
   struct dentry *dentry;
   int ret;
   VMK_ReturnStatus status;
   vmk_HeapID heap;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);

   handles = (vmkplxr_ChardevHandles *) attr->clientDeviceData.ptr;
   VMK_ASSERT(handles != NULL);

   cdev = (LinuxCharDev *) handles->vmklinuxInfo.ptr;

   //Check vmkapi chardev open and Linux open flags compatibility on compile
   VMK_ASSERT_ON_COMPILE(VMK_CHARDEV_OFLAG_RDONLY == O_RDONLY);
   VMK_ASSERT_ON_COMPILE(VMK_CHARDEV_OFLAG_WRONLY  == O_WRONLY);
   VMK_ASSERT_ON_COMPILE(VMK_CHARDEV_OFLAG_RDWR == O_RDWR);
   VMK_ASSERT_ON_COMPILE(VMK_CHARDEV_OFLAG_RDWR_MASK == O_ACCMODE);
   VMK_ASSERT_ON_COMPILE(VMK_CHARDEV_OFLAG_EXCLUSIVE  == O_EXCL);
   VMK_ASSERT_ON_COMPILE(VMK_CHARDEV_OFLAG_APPEND  == O_APPEND);
   VMK_ASSERT_ON_COMPILE(VMK_CHARDEV_OFLAG_NONBLOCK  == O_NONBLOCK);
   VMK_ASSERT_ON_COMPILE(VMK_CHARDEV_OFLAG_DSYNC == O_DSYNC);
   VMK_ASSERT_ON_COMPILE(VMK_CHARDEV_OFLAG_SYNC == O_SYNC);
   VMK_ASSERT_ON_COMPILE(VMK_CHARDEV_OFLAG_DIRECT  == O_DIRECT);

   VMK_ASSERT(cdev != NULL);
   VMK_ASSERT(cdev->ops != NULL);
   VMK_ASSERT(cdev->modID != VMK_INVALID_MODULE_ID);

   VMKLNX_DEBUG(2, "M=%d m=%d flags=0x%x", 
                   cdev->major, cdev->minor, attr->openFlags);

   heap = vmk_ModuleGetHeapID(cdev->modID);

   VMK_ASSERT(heap != VMK_INVALID_HEAP_ID);

   fileInfo = vmk_HeapAlloc(heap, sizeof(*fileInfo));
   VMK_ASSERT(fileInfo != NULL);
   if (fileInfo == NULL) {
      attr->clientInstanceData.ptr = NULL;
      return VMK_NO_MEMORY;
   }

   memset(fileInfo, 0, sizeof(*fileInfo));

   file = &fileInfo->file;
   inode = &fileInfo->inode;
   dentry = &fileInfo->dentry;

   dentry->d_inode = inode;
   file->f_dentry = dentry;

   inode->i_rdev = MKDEV((cdev->major & 0xff), (cdev->minor & 0xff));
   file->f_flags = attr->openFlags;
   file->f_mode = VMKLNX_CHAR_OFLAGS_TO_FMODE(attr->openFlags);

   attr->clientInstanceData.ptr = fileInfo;

   if (cdev->ops->open == NULL) {
      return VMK_OK;
   }

   VMKAPI_MODULE_CALL(cdev->modID, ret, cdev->ops->open, inode, file);

   status = vmklnx_errno_to_vmk_return_status(ret);
   if (status != VMK_OK) {
      attr->clientInstanceData.ptr = NULL;
      vmk_HeapFree(heap, fileInfo);
   }
   return status;
}

/*
 *-----------------------------------------------------------------------------
 *
 * LinuxCharClose --
 *
 *      Close a vmkernel driver char device. 
 *      
 * Results:
 *      Driver return value wrapped in VMK_ReturnStatus. 
 *
 * Side effects:
 *      Calls driver close(). 
 *
 *-----------------------------------------------------------------------------
 */

VMK_ReturnStatus
LinuxCharClose(vmk_CharDevFdAttr *attr)
{
   vmkplxr_ChardevHandles *handles;
   LinuxCharDev *cdev;
   struct inode *inode;
   struct file *file;
   int ret;
   VMK_ReturnStatus status;
   LinuxCharFileInfo *fileInfo;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);

   handles = (vmkplxr_ChardevHandles *) attr->clientDeviceData.ptr;
   VMK_ASSERT(handles != NULL);

   cdev = (LinuxCharDev *) handles->vmklinuxInfo.ptr;

   VMK_ASSERT(cdev != NULL);
   VMK_ASSERT(cdev->ops != NULL);
   VMK_ASSERT(cdev->modID != VMK_INVALID_MODULE_ID);
   VMK_ASSERT(vmk_ModuleGetHeapID(cdev->modID) != VMK_INVALID_HEAP_ID);

   VMKLNX_DEBUG(2, "M=%d m=%d flags=0x%x", 
                   cdev->major, cdev->minor, attr->openFlags);

   fileInfo = attr->clientInstanceData.ptr;
   VMK_ASSERT(fileInfo != NULL);
   if (fileInfo == NULL) {
      return VMK_NO_MEMORY;
   }

   if (cdev->ops->release == NULL) {
      vmk_HeapFree(vmk_ModuleGetHeapID(cdev->modID), fileInfo);
      return VMK_OK;
   }

   inode = &fileInfo->inode;
   file = &fileInfo->file;

   VMK_ASSERT(cdev->major == MAJOR(inode->i_rdev));
   VMK_ASSERT(cdev->minor == MINOR(inode->i_rdev));

   file->f_flags = attr->openFlags;
   file->f_mode = VMKLNX_CHAR_OFLAGS_TO_FMODE(attr->openFlags);

   VMKAPI_MODULE_CALL(cdev->modID, ret, cdev->ops->release, inode, file);

   status =  vmklnx_errno_to_vmk_return_status(ret);
   if (status == VMK_OK) {
      vmk_HeapFree(vmk_ModuleGetHeapID(cdev->modID), fileInfo);
   }

   return status;
}

/*
 *-----------------------------------------------------------------------------
 *
 * LinuxCharIoctl --
 *
 *      Ioctl dispatcher. 
 *
 * Results:
 *      VMK_OK - called driver ioctl successfully. (Driver error in "result.") 
 *      VMK_NOT_FOUND - no matching char device.
 *      VMK_NOT_SUPPORTED - no ioctl handler for this device.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

VMK_ReturnStatus
LinuxCharIoctl(vmk_CharDevFdAttr *attr, unsigned int cmd, 
               vmk_uintptr_t userData, vmk_IoctlCallerSize callerSize, 
               vmk_int32 *result)
{
   vmkplxr_ChardevHandles *handles;
   LinuxCharDev *cdev;
   LinuxCharFileInfo *fileInfo;
   struct file *file;
   struct inode *inode;
   int done;
   VMK_ReturnStatus status;
  
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);

   handles = (vmkplxr_ChardevHandles *) attr->clientDeviceData.ptr;
   VMK_ASSERT(handles != NULL);

   cdev = (LinuxCharDev *) handles->vmklinuxInfo.ptr;

   VMK_ASSERT(cdev != NULL);
   VMK_ASSERT(cdev->ops != NULL);
   VMK_ASSERT(cdev->modID != VMK_INVALID_MODULE_ID);

   VMKLNX_DEBUG(2, "M=%d m=%d flags=0x%x cmd=0x%x uargs=0x%lx",
                   cdev->major, cdev->minor, attr->openFlags, 
                   cmd, (unsigned long)userData);

   if (cdev->ops->compat_ioctl == NULL && 
       cdev->ops->unlocked_ioctl == NULL &&
       cdev->ops->ioctl == NULL) {
      return VMK_NOT_IMPLEMENTED;
   }

   /*
    * if we have a 64-bit caller, one of unlocked_ioctl() or ioctl()
    * should be defined.
    */

   if (cdev->ops->unlocked_ioctl == NULL &&
       cdev->ops->ioctl == NULL &&
       callerSize == VMK_IOCTL_CALLER_64) {
      return VMK_NOT_IMPLEMENTED;
   }

   fileInfo = attr->clientInstanceData.ptr;
   VMK_ASSERT(fileInfo != NULL);
   if (fileInfo == NULL) {
      return VMK_NO_MEMORY;
   }

   file = &fileInfo->file;
   inode = &fileInfo->inode;

   VMK_ASSERT(cdev->major == MAJOR(inode->i_rdev));
   VMK_ASSERT(cdev->minor == MINOR(inode->i_rdev));

   file->f_flags = attr->openFlags;
   file->f_mode = VMKLNX_CHAR_OFLAGS_TO_FMODE(attr->openFlags);

   done = 0;
   if (callerSize == VMK_IOCTL_CALLER_32 && cdev->ops->compat_ioctl) {
      VMKAPI_MODULE_CALL(cdev->modID, *result, cdev->ops->compat_ioctl,
                 file, cmd, userData);
      if (*result != -ENOIOCTLCMD) {
         done = 1;
      }
   }
   if (!done && cdev->ops->unlocked_ioctl) {
      VMKAPI_MODULE_CALL(cdev->modID, *result, cdev->ops->unlocked_ioctl,
                         file, cmd, userData);
      if (*result != -ENOIOCTLCMD) {
         done = 1;
      }
   }
   if (!done && cdev->ops->ioctl) {
      lock_kernel();
      VMKAPI_MODULE_CALL(cdev->modID, *result, cdev->ops->ioctl,
                         inode, file, cmd, userData);
      unlock_kernel();
   }

   status = VMK_OK;

   return status;
}

/*
 *----------------------------------------------------------------------------
 *
 *  LinuxCharPoll --
 *
 *    Invoke the device's poll handler if it has been declared.
 *
 *  Results:
 *    VMK_ReturnStatus
 *
 *  Side effects:
 *    None
 *
 *----------------------------------------------------------------------------
 */

VMK_ReturnStatus
LinuxCharPoll(vmk_CharDevFdAttr *attr, void *pollCtx, unsigned *pollMask)
{
   vmkplxr_ChardevHandles *handles;
   LinuxCharDev *cdev;
   LinuxCharFileInfo *fileInfo;
   struct file *file;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);

   handles = (vmkplxr_ChardevHandles *) attr->clientDeviceData.ptr;
   VMK_ASSERT(handles != NULL);

   cdev = (LinuxCharDev *) handles->vmklinuxInfo.ptr;

   VMK_ASSERT(cdev != NULL);
   VMK_ASSERT(cdev->ops != NULL);
   VMK_ASSERT(cdev->modID != VMK_INVALID_MODULE_ID);

   VMKLNX_DEBUG(2, "M=%d m=%d flags=0x%x", 
                   cdev->major, cdev->minor, attr->openFlags);

   if (cdev->ops->poll == NULL) {
      /*
       * From O'Reilly's "Linux Device Drivers", Chapter 3:
       * "If a driver leaves its poll method NULL, the device is assumed to
       * be both readable and writable without blocking."
       */
      *pollMask = (POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM);
      return VMK_OK;
   }

   fileInfo = attr->clientInstanceData.ptr;
   VMK_ASSERT(fileInfo != NULL);
   if (fileInfo == NULL) {
      return VMK_NO_MEMORY;
   }

   file = &fileInfo->file;

   VMK_ASSERT(cdev->major == MAJOR(fileInfo->inode.i_rdev));
   VMK_ASSERT(cdev->minor == MINOR(fileInfo->inode.i_rdev));

   file->f_flags = attr->openFlags; 
   file->f_mode = VMKLNX_CHAR_OFLAGS_TO_FMODE(attr->openFlags);

   VMKAPI_MODULE_CALL(cdev->modID, *pollMask, cdev->ops->poll, file, pollCtx);

   return VMK_OK;
}

/*
 *-----------------------------------------------------------------------------
 *
 * LinuxCharRead --
 *
 *      Char device read dispatcher.  Reads "len" bytes into the buffer.
 *      XXX The buffer is assumed to be large enough
 *
 * Results:
 *      VMK_OK - called driver read successfully. (Driver error in TBD) 
 *      VMK_NOT_FOUND - no matching char device.
 *      VMK_NOT_SUPPORTED - no read handler for this device.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

VMK_ReturnStatus
LinuxCharRead(vmk_CharDevFdAttr *attr, char *buffer, vmk_ByteCount nbytes, 
              vmk_loff_t *ppos, vmk_ByteCountSigned *nread)
{
   vmkplxr_ChardevHandles *handles;
   LinuxCharDev *cdev;
   LinuxCharFileInfo *fileInfo;
   struct file *file;
   VMK_ReturnStatus status = VMK_OK;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);

   handles = (vmkplxr_ChardevHandles *) attr->clientDeviceData.ptr;
   VMK_ASSERT(handles != NULL);

   cdev = (LinuxCharDev *) handles->vmklinuxInfo.ptr;

   VMK_ASSERT(cdev != NULL);
   VMK_ASSERT(cdev->ops != NULL);
   VMK_ASSERT(cdev->modID != VMK_INVALID_MODULE_ID);

   VMKLNX_DEBUG(1, "M=%d m=%d flags=0x%x read %lu bytes at offset 0x%llx",
                   cdev->major, cdev->minor, attr->openFlags, nbytes, *ppos);

   if (cdev->ops->read == NULL) {
      return VMK_NOT_IMPLEMENTED;
   }

   fileInfo = attr->clientInstanceData.ptr;
   VMK_ASSERT(fileInfo != NULL);
   if (fileInfo == NULL) {
      return VMK_NO_MEMORY;
   }

   file = &fileInfo->file;

   VMK_ASSERT(cdev->major == MAJOR(fileInfo->inode.i_rdev));
   VMK_ASSERT(cdev->minor == MINOR(fileInfo->inode.i_rdev));

   file->f_flags = attr->openFlags; 
   file->f_mode = VMKLNX_CHAR_OFLAGS_TO_FMODE(attr->openFlags);

   VMKAPI_MODULE_CALL(cdev->modID, *nread, cdev->ops->read, file,
                           buffer, nbytes, ppos);

   if (*nread < 0) {
      if (!(attr->openFlags & VMK_CHARDEV_OFLAG_NONBLOCK) 
          && *nread != -EAGAIN) {
         static uint32_t throttle = 0;
         VMKLNX_THROTTLED_WARN(throttle, "M=%d m=%d flags=%#x read %lu "
                               "bytes at offset %#llx failed (%ld)",
                               cdev->major, cdev->minor, attr->openFlags, 
                               nbytes, *ppos, *nread);
      } else {
         VMKLNX_DEBUG(2, "M=%d m=%d flags=%#x non-blocking read %lu bytes at offset %#llx failed (%ld)",
              cdev->major, cdev->minor, attr->openFlags, nbytes, *ppos, *nread);
      }
      status =  vmklnx_errno_to_vmk_return_status(*nread);
   } else {
      VMK_ASSERT(*nread <= nbytes);
   }

   return status;
}

/*
 *-----------------------------------------------------------------------------
 *
 * LinuxCharWrite --
 *
 *      Char device read dispatcher.  Write "len" bytes from the buffer.
 *      XXX The buffer is assumed to be large enough
 *
 * Results:
 *      VMK_OK - called driver read successfully. (Driver error in TBD) 
 *      VMK_NOT_FOUND - no matching char device.
 *      VMK_NOT_SUPPORTED - no read handler for this device.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

VMK_ReturnStatus
LinuxCharWrite(vmk_CharDevFdAttr *attr, char *buffer, vmk_ByteCount nbytes, 
               vmk_loff_t *ppos, vmk_ByteCountSigned *nwritten)
{
   vmkplxr_ChardevHandles *handles;
   LinuxCharDev *cdev;
   struct file *file;
   LinuxCharFileInfo *fileInfo;
   VMK_ReturnStatus status = VMK_OK;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);

   handles = (vmkplxr_ChardevHandles *) attr->clientDeviceData.ptr;
   VMK_ASSERT(handles != NULL);

   cdev = (LinuxCharDev *) handles->vmklinuxInfo.ptr;

   VMK_ASSERT(cdev != NULL);
   VMK_ASSERT(cdev->ops != NULL);
   VMK_ASSERT(cdev->modID != VMK_INVALID_MODULE_ID);

   VMKLNX_DEBUG(1, "M=%d m=%d flags=0x%x write %lu bytes at offset 0x%llx",
                   cdev->major, cdev->minor, attr->openFlags, nbytes, *ppos);

   if (cdev->ops->write == NULL) {
      return VMK_NOT_IMPLEMENTED;
   }

   fileInfo = attr->clientInstanceData.ptr;
   VMK_ASSERT(fileInfo != NULL);
   if (fileInfo == NULL) {
      return VMK_NO_MEMORY;
   }

   file = &fileInfo->file;

   VMK_ASSERT(cdev->major == MAJOR(fileInfo->inode.i_rdev));
   VMK_ASSERT(cdev->minor == MINOR(fileInfo->inode.i_rdev));

   file->f_flags = attr->openFlags; 
   file->f_mode = VMKLNX_CHAR_OFLAGS_TO_FMODE(attr->openFlags);

   VMKAPI_MODULE_CALL(cdev->modID, *nwritten, cdev->ops->write, file,
                           buffer, nbytes, ppos);

   VMK_ASSERT(*nwritten <= (vmk_ByteCountSigned)nbytes);
 
   if (*nwritten < 0) {
      if (!(attr->openFlags & VMK_CHARDEV_OFLAG_NONBLOCK) 
          && *nwritten != -EAGAIN) {
         static uint32_t throttle = 0;
         VMKLNX_THROTTLED_WARN(throttle,
                               "M=%d m=%d flags=%#x write %lu "
                               "bytes at offset %#llx failed (%ld)",
                               cdev->major, cdev->minor, 
                               attr->openFlags, nbytes, *ppos, 
                               *nwritten);
      } else {
         VMKLNX_DEBUG(2, "M=%d m=%d flags=%#x non-blocking write %lu bytes at offset %#llx failed (%ld)",
              cdev->major, cdev->minor, attr->openFlags, nbytes, *ppos, *nwritten);
      }
      status =  vmklnx_errno_to_vmk_return_status(*nwritten);
   } else {
      VMK_ASSERT(*nwritten <= nbytes);
   }

   return status;
}

/*-----------------------------------------------------------------------------
 *
 * LinuxCharFreeDevData --
 *
 *      Free metadata associated with a character device that has been 
 *      unregistered.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
static VMK_ReturnStatus
LinuxCharFreeDevData(vmk_AddrCookie vmkplxrHandles)
{
   vmkplxr_ChardevHandles *handles;
   LinuxCharDev *dev;
   vmk_HeapID heap;

   handles = vmkplxrHandles.ptr;
   VMK_ASSERT(handles != NULL);
   dev = handles->vmklinuxInfo.ptr;
   VMK_ASSERT(dev != NULL);
   heap = vmk_ModuleGetHeapID(dev->modID);

   vmk_HeapFree(heap, dev);

   return VMK_OK;
}


/*
 *-----------------------------------------------------------------------------
 *
 * register_chrdev --
 *
 *      Register a char device with the VMkernel
 *
 * Results:
 *      0 on success for static major request, new major number if 
 *      dynamic major request, errno on failure.
 *      
 * Side effects:
 *      Call into vmkernel to register chrdev. 
 *
 *-----------------------------------------------------------------------------
 */
/**                                          
 *  register_chrdev - Register a character device     
 *  @major: major number request or 0 to let VMkernel pick a major number.
 *  @name: name of this range of devices
 *  @fops: file operations associated with devices
 *
 *  If @major == 0, this function will dynamically allocate a major and return its number.
 * 
 *  If @major > 0 this function will attempt to reserve a device with the given major number
 *  and will return 0 on success.
 *
 *  This function registers a range of 256 minor numbers, the first being 0.
 *
 */                                          
/* _VMKLNX_CODECHECK_: register_chrdev */
int 
register_chrdev(unsigned int major,           // IN: driver requested major
                const char *name,             // IN: 
                const struct file_operations *fops) // IN: file ops
{
   int rc;
   int assignedMajor = major;
   int minor = 0;

   VMK_ReturnStatus status;
   vmk_ModuleID module = vmk_ModuleStackTop();
   vmk_HeapID heap = vmk_ModuleGetHeapID(module);
   LinuxCharDev *dev;
   vmk_AddrCookie vmklinuxInfo;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);

   VMK_ASSERT_ON_COMPILE(MISC_MAJOR == VMKPLXR_MISC_MAJOR);
   VMK_ASSERT_ON_COMPILE(MISC_DYNAMIC_MINOR == VMKPLXR_DYNAMIC_MINOR);
   VMK_ASSERT_ON_COMPILE(VMKPLXR_DYNAMIC_MAJOR == 0);

   VMKLNX_DEBUG(2, "M=%d driver=%s open=%p, close=%p, ioctl=%p, "
                   "poll=%p, read=%p, write=%p ioctl_compat=%p,", 
                   major, name, fops->open, fops->release, fops->ioctl, 
                   fops->poll, fops->read, fops->write, fops->compat_ioctl);

   VMK_ASSERT(heap != VMK_INVALID_HEAP_ID);

   dev = vmk_HeapAlloc(heap, sizeof(*dev));
   if (dev == NULL) {
      return -ENOMEM;
   }

   dev->ops = fops;
   dev->modID = module;
   vmklinuxInfo.ptr = dev;

   /*
    * We only register the vmklinux-specific information.  
    * The vmkplexer will register its own handle as well in a 
    * vmkplxr_ChardevHandles struct.  This 
    * vmkplxr_ChardevHandles struct then gets passed along to 
    * us in every vmkapi callback, including cleanup, from which
    * we can extract this vmklinuxInfo value.
    */
   status = vmkplxr_RegisterChardev(&assignedMajor, &minor, name,
                                    &linuxCharDevOps,
                                    vmklinuxInfo,
                                    LinuxCharFreeDevData,
                                    module);

   if (status != VMK_OK) {
      vmk_HeapFree(heap, dev);
      return -EBUSY;
   }

   dev->major = assignedMajor;
   dev->minor = minor;

   rc = ((major == 0) ? assignedMajor : 0);
   return rc;
}
EXPORT_SYMBOL(register_chrdev);

/*
 *-----------------------------------------------------------------------------
 *
 * unregister_chrdev --
 *
 *      Unregister a char device with the VMkernel
 *
 * Results:
 *      0 on success, errno on failure.
 *      
 * Side effects:
 *      Call into vmkernel to unregister chrdev. 
 *
 *-----------------------------------------------------------------------------
 */
/**                                          
 *  unregister_chrdev - Unregister a char device
 *  @major: major device number of device to be unregistered
 *  @name: name of device
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: unregister_chrdev */
int 
unregister_chrdev(unsigned int major, 
                  const char *name)
{
   int rc = 0;
   VMK_ReturnStatus status;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMKLNX_DEBUG(2, "M=%d driver=%s", major, name);

   /* 
    * Final cleanup will happen in vmkplexer once all references
    * to the device have been closed.  During that cleanup,
    * LinuxCharFreeDevData will be invoked.
    */
   status = vmkplxr_UnregisterChardev(major, 0, name);

   if (status != VMK_OK) {
      rc = -EINVAL;
      VMKLNX_WARN("Failed unregistering character device M=%d driver=%s", 
                  major, name);
      /*
       * XXX - We can't assert here because it is customary in some
       * Linux drivers to unconditionally unregister their character
       * devices at module-unload time, regardless of whether or not 
       * they might've previously unregistered the character device at 
       * runtime.  USB does this.
       */
   }

   return rc;
}
EXPORT_SYMBOL(unregister_chrdev);

/**                                          
 *  misc_register - Register a misc device with the VMkernel 
 *  @misc: device to register
 *
 *  misc_register attempts to register a character device with the VMkernel,
 *  using the device's supplied file operations and requested
 *  minor.
 *                                           
 *  ESX Deviation Notes:
 *  ESX does not support the fasync handler.
 *
 *  RETURN VALUE:
 *  A negative error code on an error, 0 on success
 *                
 */                                          
/* _VMKLNX_CODECHECK_: misc_register */
int 
misc_register(struct miscdevice *misc)
{
   vmk_int32 major = VMKPLXR_MISC_MAJOR;
   vmk_uint32 minor = misc->minor;
   VMK_ReturnStatus status;
   vmk_ModuleID module = vmk_ModuleStackTop();
   vmk_HeapID heap = vmk_ModuleGetHeapID(module);
   LinuxCharDev *dev;
   vmk_AddrCookie vmklinuxInfo;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMKLNX_DEBUG(2, "driver=%s open=%p, close=%p, poll=%p, read=%p, "
                   "write=%p, ioctl=%p, compat_ioctl=%p", 
                   misc->name, misc->fops->open, misc->fops->release, 
                   misc->fops->poll, misc->fops->read, misc->fops->write,
                   misc->fops->ioctl, misc->fops->compat_ioctl);

   VMK_ASSERT(heap != VMK_INVALID_HEAP_ID);

   dev = vmk_HeapAlloc(heap, sizeof(*dev));
   if (dev == NULL) {
      return -ENOMEM;
   }

   dev->ops = misc->fops;
   dev->modID = module;
   vmklinuxInfo.ptr = dev;

   /*
    * We only register the vmklinux-specific information.  
    * The vmkplxr_ChardevHandles struct that is passed as the
    * devicePrivateInformation of the vmkapi file descriptor to 
    * the file ops will contain this.
    */
   status = vmkplxr_RegisterChardev(&major, &minor, misc->name,
                                     &linuxCharDevOps, vmklinuxInfo, 
                                     LinuxCharFreeDevData,
                                     module);

   if (status != VMK_OK) {
      vmk_HeapFree(heap, dev);
      return -EBUSY;
   }

   dev->major = major;
   dev->minor = minor;
   misc->minor = minor;

   return 0;
}
EXPORT_SYMBOL(misc_register);


/*
 *-----------------------------------------------------------------------------
 *
 * misc_deregister --
 *
 *      Unregister a misc device with the VMkernel
 *
 * Results:
 *      
 *      
 * Side effects:
 *      
 *
 *-----------------------------------------------------------------------------
 */
/**                                          
 *  misc_deregister - Unregister a misc device from the VMkernel
 *  @misc: Device to unregister
 *                                           
 *  Attempts to deregister the given misc device from the VMkernel.
 *  This will not succeed if the device is in use or is not present.
 *
 *  RETURN VALUE:
 *  0 on success, a negative error code on failure                                           
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: misc_deregister */
int 
misc_deregister(struct miscdevice *misc)
{
   int rc = 0;
   VMK_ReturnStatus status;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMKLNX_DEBUG(2, "m=%d driver=%s", misc->minor, misc->name);

   /* 
    * Final cleanup will happen in vmkplexer once all references
    * to the device have been closed.  During that cleanup,
    * LinuxCharFreeDevData will be invoked.
    */
   status = vmkplxr_UnregisterChardev(VMKPLXR_MISC_MAJOR, 
                                      misc->minor, misc->name);

   if (status != VMK_OK) {
      rc = -EINVAL;
      VMKLNX_WARN("M=%d, m=%d, driver=%s, Failed unregistering misc device",
                  MISC_MAJOR, misc->minor, misc->name);
      /*
       * XXX - We can't assert here because it is customary in some
       * Linux drivers to unconditionally unregister their character
       * devices at module-unload time, regardless of whether or not 
       * they might've previously unregistered the character device at 
       * runtime.  USB does this.
       */
   }

   return rc;
}
EXPORT_SYMBOL(misc_deregister);


/*
 *-----------------------------------------------------------------------------
 *
 * LinuxChar_Init --
 *
 *      Initialize the Linux emulation character driver subsystem
 *
 * Results:
 *      None
 *      
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */
void
LinuxChar_Init(void)
{
   VMKLNX_CREATE_LOG();
}

/*
 *-----------------------------------------------------------------------------
 *
 * LinuxChar_Cleanup --
 *
 *      Shutdown the Linux emulation character driver subsystem
 *
 * Results:
 *      None
 *      
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */
void
LinuxChar_Cleanup(void)
{
   VMKLNX_DESTROY_LOG();
}

/**                                          
 *  no_llseek - Stub llseek implementation for drivers that do not support llseek.
 *  @file: the file being seeked
 *  @offset: new offset position
 *  @origin: 0 for absolute, 1 for relative to current file position.
 *                                         
 *  This is a null llseek implementation that always returns -ESPIPE,
 *  which is the POSIX-defined error associated with attempting a seek 
 *  operation on a pipe or FIFO.  The file pointer remains unchanged.
 *                                           
 *  Return Value:
 *  -ESPIPE
 *
 */                                          
/* _VMKLNX_CODECHECK_: no_llseek */
loff_t
no_llseek(struct file *file, loff_t offset, int origin)
{
    VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
    return -ESPIPE;
}
EXPORT_SYMBOL(no_llseek);


