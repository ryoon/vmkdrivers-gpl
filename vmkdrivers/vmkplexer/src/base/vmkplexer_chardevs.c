/* 
 **********************************************************
 * Copyright 2010, 2015 VMware, Inc.  All rights reserved.
 **********************************************************
 */

/*
 * vmkplexer_chardevs.c --
 *
 *      Management of the shared major/minor address space of vmklinux character
 *      devices including registration and unregistration services.
 */

#include "vmkapi.h"

#include <vmkplexer_module.h>
#include <vmkplexer_chardevs.h>
#include <vmkplexer_log.h>

/*
 * A note about synchronization --
 * vmkapi provides significant serialization with regard to 
 * registration, unregistration, and reference-counting of in-use
 * devices.  vmkapi ensures that during invocation of the
 * unregistration cleanup here in the vmkplexer no new registrations
 * can be going on.  We provide separate synchronization here for
 * allocating in the major/minor namespace, but we don't need
 * to synchronize about the file-ops so long as we ensure that
 * a file-op is immediately valid to use after calling 
 * vmk_CharDevRegister.  So, we prepare all metadata such that
 * the file ops are valid *first* before calling vmk_CharDevRegister.
 * vmkapi calls us only after all file ops are known to be quiesced,
 * when it is safe for us to then tear down the device and our
 * vmkplexer state.
 */

/*
 * chardevRegSem protects major/minor namespace allocation
 * structures.
 */
static vmk_Semaphore chardevRegSem;

typedef struct VmkplxrCharDev {
   vmk_ModuleID modID;
   int major;
   int minor;
   vmk_CharDev vmkHandle;
   vmkplxr_ChardevDestructor dtor;
   vmk_Bool unregPending;
} VmkplxrCharDev;


/* 
 * There are VMK_ASSERT_ON_COMPILES within the code to make sure
 * these values are correct with respect to the number of majors & 
 * minors.
 */
#define MAJOR_BITSETS 8
#define BITSETS_PER_MINOR 8
#define CHARDEVS_PER_BITSET 32
#define VMKPLXR_MAJOR_ARRAYSZ 256

/*
 * Allocation:  This is broken in to units of 32 majors each.
 */
vmk_uint32 majorsFreeBitset[MAJOR_BITSETS];
typedef struct VmkplxrMinorInfo {
   vmk_uint32 minorsFreeBitset[BITSETS_PER_MINOR];
   VmkplxrCharDev *chardevMinors[VMKPLXR_MINORS_PER_MAJOR];
} VmkplxrMinorInfo;

VmkplxrMinorInfo *minorInfoPerMajor[VMKPLXR_MAJOR_ARRAYSZ];

static VMK_ReturnStatus VmkplxrCharUnregCleanup(vmk_AddrCookie cookie);

static int
VmkplxrMajorHasAllFreeMinors(int major) {
   vmk_uint32 dynamicMinorBit, dynamicMinorBitNum, dynamicMinorIndex;
   vmk_uint32 allFree;
   int i;
   VmkplxrMinorInfo *minorInfo = minorInfoPerMajor[major];

   if (minorInfo == NULL) {
      /* This hasn't been allocated.  Therefore they are all free. */
      return 1;
   }

   dynamicMinorBitNum = VMKPLXR_DYNAMIC_MINOR % CHARDEVS_PER_BITSET;
   dynamicMinorIndex = VMKPLXR_DYNAMIC_MINOR / CHARDEVS_PER_BITSET;
   dynamicMinorBit = 1 << (dynamicMinorBitNum);
   for (i = 0; i < BITSETS_PER_MINOR; i++) {
      if (i != dynamicMinorIndex) {
         allFree = (vmk_uint32) (-1);
      }
      else {
         allFree = ~dynamicMinorBit;
      }
      if (minorInfo->minorsFreeBitset[i] != allFree) {
         /* There's one slot in use. */
         return 0;
      }
   }
   return 1;
}

static VMK_ReturnStatus
VmkplxrAllocDevNamespace(int *major, int *minor)
{
   int i;
   int majorAllocated, minorAllocated;
   VmkplxrMinorInfo *minorInfo;
   vmk_uint32 allocatedBit, allocatedBitNum;

   /*
    * If the following aren't true, the allocation scheme and 
    * metadata layout system breaks.
    */
   VMK_ASSERT_ON_COMPILE(VMKPLXR_MINORS_PER_MAJOR == 255);
   VMK_ASSERT_ON_COMPILE(VMKPLXR_NUM_MAJORS == 255);
   VMK_ASSERT_ON_COMPILE(VMKPLXR_DYNAMIC_MAJOR == 0);
   VMK_ASSERT_ON_COMPILE(VMKPLXR_DYNAMIC_MINOR == 255);

   vmk_SemaAssertIsLocked(&chardevRegSem);

   VMKPLXR_DEBUG(2, "requesting allocation of M=%d/m=%d", *major, *minor);

   if (*major == VMKPLXR_DYNAMIC_MAJOR && *minor != 0) {
      return VMK_BAD_PARAM;
   }

   if (*minor == VMKPLXR_DYNAMIC_MINOR &&
       *major == VMKPLXR_DYNAMIC_MAJOR) {
      return VMK_BAD_PARAM;
   }


   if (*major == VMKPLXR_DYNAMIC_MAJOR) {
      majorAllocated = VMKPLXR_DYNAMIC_MAJOR;
      /* Find a free major slot, searching from the largest first */
      for (i = (MAJOR_BITSETS - 1); i >= 0; i--) {
         if (majorsFreeBitset[i] != 0) {
            majorAllocated = (__builtin_ffs(majorsFreeBitset[i]));
            /* 
             * __builtin_ffs will give a 1-based index from 1 to 32.
             * The number returned is the index of the next available
             * free slot in this grouping of 32.  However, we must 
             * allocate from the largest of the namespace, so the actual
             * major offset corresponds to 32 minus this index.
             */
            VMK_ASSERT(majorAllocated > 0 && majorAllocated < 33);
            majorAllocated = CHARDEVS_PER_BITSET - majorAllocated;
            majorAllocated += (i*CHARDEVS_PER_BITSET);
            VMK_ASSERT(minorInfoPerMajor[majorAllocated] == NULL);
            break;
         }
      }
      if (majorAllocated == VMKPLXR_DYNAMIC_MAJOR) {
         /*
          * Allocation failed.  All majors are currently in use.
          */
         VMKPLXR_WARN("All majors in use");
         return VMK_NO_MEMORY;
      }
   }
   else {
      majorAllocated = *major;
   }

   minorInfo = minorInfoPerMajor[majorAllocated];

   if (minorInfo == NULL) {
      minorInfo = vmk_HeapAlloc(vmkplxr_heap_id, sizeof(VmkplxrMinorInfo));
      if(minorInfo == NULL) {
         VMKPLXR_DEBUG(2, "Can't allocate minorInfo for a new major");
         return VMK_NO_MEMORY;
      }
      for (i = 0; i < BITSETS_PER_MINOR; i++) {
         minorInfo->minorsFreeBitset[i] = (vmk_uint32) -1;
      }
      /* reserve the 'dynamic minor' index */
      i = VMKPLXR_DYNAMIC_MINOR / CHARDEVS_PER_BITSET;
      allocatedBitNum = VMKPLXR_DYNAMIC_MINOR % CHARDEVS_PER_BITSET;
      allocatedBit = 1 << (allocatedBitNum);
      minorInfo->minorsFreeBitset[i] &= (~allocatedBit);
      minorInfoPerMajor[majorAllocated] = minorInfo;
   }

   if (*minor == VMKPLXR_DYNAMIC_MINOR) {
      /* Find a free minor slot, searching from the *smallest* first */
      minorAllocated = VMKPLXR_DYNAMIC_MINOR;
      for (i = 0; i < BITSETS_PER_MINOR; i++) {
         if (minorInfo->minorsFreeBitset[i] != 0) {
            minorAllocated = (__builtin_ffs(minorInfo->minorsFreeBitset[i]) - 1);
            /*
             * Here, __builtin_ffs is returning the 1-based index of the 
             * first free minor.  Allocating from the smallest, this is 
             * correct, except we want something 0-based.
             */
            minorAllocated += (i * CHARDEVS_PER_BITSET);
            break;
         }
      }
      if (minorAllocated == VMKPLXR_DYNAMIC_MINOR) {
         /* All minors for this major are consumed. */
         VMKPLXR_WARN("Can't find a free minor within major=%d", majorAllocated);
         return VMK_NO_MEMORY;
      }
   }
   else {
      minorAllocated = *minor;
   }

   /* Mark the major and minor as allocated in their respective bitsets */
   i = minorAllocated / CHARDEVS_PER_BITSET;
   allocatedBitNum = minorAllocated % CHARDEVS_PER_BITSET;
   allocatedBit = 1 << (allocatedBitNum);
   if (!(minorInfo->minorsFreeBitset[i] & allocatedBit)) {
      /* 
       * The minor isn't free.  This can happen when a static
       * minor is requested.
       */
      VMKPLXR_DEBUG(2, "Can't allocate minor=%d (major %d) -- in use", minorAllocated, majorAllocated);
      return VMK_BUSY;
   }
   minorInfo->minorsFreeBitset[i] &= (~allocatedBit);

   i = majorAllocated / CHARDEVS_PER_BITSET;
   allocatedBitNum = majorAllocated % CHARDEVS_PER_BITSET;
   allocatedBitNum = CHARDEVS_PER_BITSET - allocatedBitNum;
   allocatedBit = 1 << (allocatedBitNum - 1);
   /* 
    * it's valid for the major to already be in use - we may just be
    * clearing it as allocated again, regardless.
    */
   majorsFreeBitset[i] &= (~allocatedBit);   

   VMKPLXR_DEBUG(2, "Allocating major=%u, minor=%u",
                majorAllocated, minorAllocated);

   *major = majorAllocated;
   *minor = minorAllocated;
   
   return VMK_OK;
}

static VMK_ReturnStatus
VmkplxrFreeDevNamespace(int major, int minor)
{
   VmkplxrMinorInfo *minorInfo;
   vmk_uint32 freeBit;
   int bitsetIndex, bitNum;

   vmk_SemaAssertIsLocked(&chardevRegSem);

   if (major == VMKPLXR_DYNAMIC_MAJOR || minor == VMKPLXR_DYNAMIC_MINOR) {
      VMKPLXR_WARN("Freeing invalid namespace element:  M=%d,m=%d", 
                   major, minor);
      return VMK_BAD_PARAM;
   }
 
   VMKPLXR_DEBUG(2, "M=%d/m=%d", major, minor);

   /* Mark the minor free */
   minorInfo = minorInfoPerMajor[major];
   VMK_ASSERT(minorInfo != NULL);

   bitNum = minor % CHARDEVS_PER_BITSET;
   bitsetIndex = minor / CHARDEVS_PER_BITSET;

   /* We're storing the bits for minors in least-to-greatest order */
   freeBit = 1 << bitNum;
   VMK_ASSERT((minorInfo->minorsFreeBitset[bitsetIndex] & (freeBit)) == 0);
   minorInfo->minorsFreeBitset[bitsetIndex] |= freeBit;

   /* If that was the last minor, also mark the major free */
   if (major != VMKPLXR_MISC_MAJOR && 
       VmkplxrMajorHasAllFreeMinors(major)) {

      /* mark the major free */
      bitNum = major % CHARDEVS_PER_BITSET;
      bitsetIndex = major / CHARDEVS_PER_BITSET;

      /* We're storing the bits for majors in greatest-to-least order */
      bitNum = CHARDEVS_PER_BITSET - bitNum;
      freeBit = 1 << (bitNum - 1);
      VMK_ASSERT((majorsFreeBitset[bitsetIndex] & (freeBit)) == 0);
      majorsFreeBitset[bitsetIndex] |= freeBit;
      vmk_HeapFree(vmkplxr_heap_id, minorInfo);
      minorInfoPerMajor[major] = NULL;
   } 

   return VMK_OK;
}

/*
 *-----------------------------------------------------------------------------
 *
 * vmkplxr_RegisterChardev --
 *
 *      Register a vmklinux character device with the vmkplexer and vmkernel.
 *      If *major == VMKPLXR_DYNAMIC_MAJOR => caller requests dynamic major
 *      If *minor == VMKPLXR_DYNAMIC_MINOR => caller requests dynamic minor
 *      dtor is optional, invoked to destroy the driverPrivate data
 *         when the device is ultimately destroyed after unregistration
 *      
 * Results:
 *      Status of registration.
 *
 * Side effects:
 *      Will cause a character device and a cleanup callback function to be
 *      registered with vmkernel.
 *
 *-----------------------------------------------------------------------------
 */
/**
 *  vmkplxr_RegisterChardev - Register a char device
 *  @major: major device number being requested, returned as assigned
 *  @minor: minor device number being requested, returned as assigned
 *  @name: name of device
 *  @fops: file-operations to be directly invoked by vmkapi
 *  @vmklinuxInfo: vmklinux-specific information to be registered (see note).
 *  @dtor: destructor for private data to be used when device is unregistered
 *  @modID: module ID of driver
 *
 *  If *major == VMKPLXR_DYNAMIC_MAJOR => caller requests dynamic major
 *  If *minor == VMKPLXR_DYNAMIC_MINOR => caller requests dynamic minor
 *  dtor is optional, invoked to destroy the vmklinuxInfo
 *     when the device is ultimately destroyed after unregistration
 *  NOTE: vmklinuxInfo is available to the file-ops handlers through 
 *        the vmkplxr_ChardevHandles struct that's passed to those 
 *        handlers.  The vmkplxr_ChardevHandles struct is passed as the
 *        clientDeviceData member of the vmk_CharDevFdAttr for each 
 *        file-ops handler.
 *  NOTE: Dynamic minors are NOT currently supported with any *major value
 *        other than VMKPLXR_MISC_MAJOR.
 *  NOTE: All of the file-operations functions must be supplied (non-NULL)
 *
 *  RETURN VALUE:
 *  Status of registration
 */
/* _VMKPLXR_CODECHECK_: vmkplxr_RegisterChardev */
VMK_ReturnStatus
vmkplxr_RegisterChardev(int *major,   // IN/OUT -- requested/assigned major
                        int *minor,   // IN/OUT -- requested/assigned minor
                        const char *name,  // IN
                        const vmk_CharDevOps *fops, // IN
                        vmk_AddrCookie vmklinuxInfo, // IN
                        vmkplxr_ChardevDestructor dtor, // IN (optional)
                        vmk_ModuleID modID)
{
   VMK_ReturnStatus status;
   VmkplxrCharDev *newDev;
   VmkplxrMinorInfo *minorInfo;
   int reqMajor, reqMinor;
   vmk_HeapID heap;
   vmkplxr_ChardevHandles *vmkDevicePrivate;
   vmk_AddrCookie vmkCookie;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   heap = vmk_ModuleGetHeapID(modID);
   VMK_ASSERT(heap != VMK_INVALID_HEAP_ID);

   newDev = vmk_HeapAlloc(heap, sizeof(*newDev));
   if (newDev == NULL) {
      status = VMK_NO_MEMORY;
      goto out_cdev_fail;
   }

   vmkDevicePrivate = vmk_HeapAlloc(heap, sizeof(*vmkDevicePrivate));
   if (vmkDevicePrivate == NULL) {
      status = VMK_NO_MEMORY;
      goto out_devprivate_fail;
   }

   reqMajor = *major;
   reqMinor = *minor;

   vmk_SemaLock(&chardevRegSem);

   VMKPLXR_DEBUG(2, "Allocating dev namespace for device %s %d/%d", 
                    name, reqMajor, reqMinor);

   if (VmkplxrAllocDevNamespace(&reqMajor, &reqMinor) != VMK_OK) {
      status = VMK_NO_MEMORY;
      vmk_SemaUnlock (&chardevRegSem);
      VMKPLXR_DEBUG(2, "Namespace alloc failed:  %d/%d", 
                       reqMajor, reqMinor);
      goto out_namespace_fail;
   }

   minorInfo = minorInfoPerMajor[reqMajor];
   newDev->dtor = dtor;
   newDev->modID = modID;
   newDev->major = reqMajor;
   newDev->minor = reqMinor;
   newDev->unregPending = VMK_FALSE;

   minorInfo->chardevMinors[reqMinor] = newDev;

   vmkDevicePrivate->vmkplxrInfo.ptr = newDev;
   vmkDevicePrivate->vmklinuxInfo = vmklinuxInfo;

   vmkCookie.ptr = vmkDevicePrivate;

   vmk_SemaUnlock(&chardevRegSem);

   status = vmk_CharDevRegister(modID, name, fops, 
                                VmkplxrCharUnregCleanup,
                                vmkCookie, &newDev->vmkHandle);

   if (status != VMK_OK) {
      goto out_vmkapi_fail;
   }

   *major = reqMajor;
   *minor = reqMinor;

   return status;

out_vmkapi_fail:
   vmk_SemaLock(&chardevRegSem);
   minorInfo->chardevMinors[reqMinor] = NULL;
   VmkplxrFreeDevNamespace(reqMajor, reqMinor);
   vmk_SemaUnlock(&chardevRegSem);
out_namespace_fail:
   vmk_HeapFree(heap, vmkDevicePrivate);
out_devprivate_fail:
   vmk_HeapFree(heap, newDev);
out_cdev_fail:

   return status;
}
VMK_MODULE_EXPORT_SYMBOL(vmkplxr_RegisterChardev);

/*
 *-----------------------------------------------------------------------------
 *
 * vmkplxr_UnregisterChardev --
 *
 *      Unregister a vmklinux character device from the vmkernel, marking it
 *      for destruction pending the completion of in-flight file operations on
 *      the device.
 *      
 *      NOTE: The major, minor, and name of the device MUST match that given
 *            during registration, or unregistration will not succeed and the
 *            device will remain active.
 *      
 * Results:
 *      Status of unregistration.
 *
 * Side effects:
 *      Will eventually cause VmkplxrCharUnregCleanup to be invoked by 
 *      vmkernel, which will in turn invoke the module's registered destructor
 *      for its driverPrivate data.
 *
 *-----------------------------------------------------------------------------
 */
/**
 *  vmkplxr_UnregisterChardev - unregister a char device
 *  @major: major device number being unregistered.
 *  @minor: minor device number being unregistered.
 *  @name: name of device
 *
 *  The major, minor, and name of the device MUST match that given
 *  during registration, or unregistration will not succeed and the
 *  device will remain active.  If request is successful, device
 *  is marked unregistered and will be destroyed once all outstanding
 *  operations have completed.
 *
 *  RETURN VALUE:
 *  Status of unregistration
 *
 */
/* _VMKPLXR_CODECHECK_: vmkplxr_RegisterChardev */
VMK_ReturnStatus
vmkplxr_UnregisterChardev(int major,
                          int minor,
                          const char *name)
{
   VmkplxrMinorInfo *minorInfo;
   VmkplxrCharDev *cdev;
   vmk_uint32 allocatedBit, allocatedBitNum;
   vmk_CharDev handle;
   int i;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);

   /*
    * Sanity check with allocation bitmap
    */
   
   VMK_ASSERT(major != VMKPLXR_DYNAMIC_MAJOR);
   VMK_ASSERT(minor != VMKPLXR_DYNAMIC_MINOR);

   VMKPLXR_DEBUG(2, "freeing dev %s M=%d/m=%d", name, major, minor);

   minorInfo = minorInfoPerMajor[major];

   if (minorInfo == NULL) {
      return VMK_BAD_PARAM;
   }

   i = major / CHARDEVS_PER_BITSET;
   allocatedBitNum = major % CHARDEVS_PER_BITSET;
   allocatedBitNum = (CHARDEVS_PER_BITSET - allocatedBitNum);
   allocatedBit = 1 << (allocatedBitNum - 1);

   /* Lock to get device ptr */
   vmk_SemaLock(&chardevRegSem);

   if (allocatedBit & (majorsFreeBitset[i])) {
      /* The major is marked free. */
      vmk_SemaUnlock(&chardevRegSem);
      return VMK_BAD_PARAM;
   }
   
   i = minor / CHARDEVS_PER_BITSET;
   allocatedBitNum = minor % CHARDEVS_PER_BITSET;
   allocatedBit = 1 << allocatedBitNum;
   
   if (allocatedBit & minorInfo->minorsFreeBitset[i]) {
      /* The minor is marked free */
      vmk_SemaUnlock(&chardevRegSem);
      return VMK_BAD_PARAM;
   }

   cdev = minorInfo->chardevMinors[minor];

   if (cdev->unregPending == VMK_TRUE) {
      /*
       * A caller has already requested that we unregister
       * the device.  This is not an error on vmklinux, where
       * it is assumed that reference counting makes multiple
       * freeing of the same major/minor safe.
       */
      vmk_SemaUnlock(&chardevRegSem);
      return VMK_OK;
   }

   cdev->unregPending = VMK_TRUE;

   VMK_ASSERT(cdev != NULL);
   handle = cdev->vmkHandle;

   vmk_SemaUnlock(&chardevRegSem);
   return vmk_CharDevUnregister(handle);
   /* The cleanup handler will de-allocate structures */
}
VMK_MODULE_EXPORT_SYMBOL(vmkplxr_UnregisterChardev);

/*
 *-----------------------------------------------------------------------------
 *
 * vmkplxr_ChardevsInit --
 *
 *      Initialize the vmkplexer character devices subsystem.
 *      
 * Results:
 *      Status of initialization.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
VMK_ReturnStatus
vmkplxr_ChardevsInit(void)
{
   int i;
   VMK_ReturnStatus status;
   vmk_uint32 bitNum, bitSetIndex, bit;
   VmkplxrMinorInfo *miscMinorInfo;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);

   status = vmk_SemaCreate(&chardevRegSem,
                           vmkplxr_module_id,
                           "vmkplexer-major-minor-alloc",
                           1 /* unlocked */);

   if (status != VMK_OK) {
      return status;
   }

   for (i = 0; i < (VMKPLXR_MAJOR_ARRAYSZ); i++) {
      minorInfoPerMajor[i] = NULL;
   }

   /* Mark all majors as free */
   for (i = 0; i < MAJOR_BITSETS; i++) {
      majorsFreeBitset[i] = (vmk_uint32) (-1);
   }

   /* mark the misc major as allocated */
   bitNum = VMKPLXR_MISC_MAJOR % CHARDEVS_PER_BITSET;
   bitSetIndex = VMKPLXR_MISC_MAJOR / CHARDEVS_PER_BITSET;
   /* Majors are stored in reverse order */
   bitNum = (CHARDEVS_PER_BITSET - bitNum);
   bit = 1 << (bitNum - 1);
   majorsFreeBitset[bitSetIndex] &= ~bit;
  

   /* mark the dynamic-major major number as allocated */
   bitNum = VMKPLXR_DYNAMIC_MAJOR % CHARDEVS_PER_BITSET;
   bitSetIndex = VMKPLXR_DYNAMIC_MAJOR / CHARDEVS_PER_BITSET;
   bitNum = (CHARDEVS_PER_BITSET - bitNum);
   bit = 1 << (bitNum - 1);
   majorsFreeBitset[bitSetIndex] &= ~bit;

   /* Allocate minor info for the misc major */
   miscMinorInfo = vmk_HeapAlloc(vmkplxr_heap_id, sizeof(*miscMinorInfo));
   if (miscMinorInfo == NULL) {
      return VMK_NO_MEMORY;
   }

   minorInfoPerMajor[VMKPLXR_MISC_MAJOR] = miscMinorInfo;

   for (i = 0; i < BITSETS_PER_MINOR; i++) {
      miscMinorInfo->minorsFreeBitset[i] = (vmk_uint32) -1;
   }
   
   /* reserve the 'dynamic minor' index for the misc major */

   bitNum = VMKPLXR_DYNAMIC_MINOR % CHARDEVS_PER_BITSET;
   bitSetIndex = VMKPLXR_DYNAMIC_MINOR / CHARDEVS_PER_BITSET;
   bit = 1 << (bitNum);
   miscMinorInfo->minorsFreeBitset[bitSetIndex] &= ~bit;

   return VMK_OK;
}
VMK_MODULE_EXPORT_SYMBOL(VmkplxrChardevsInit);


/*
 *-----------------------------------------------------------------------------
 *
 * VmkplxrCharUnregCleanup --
 *
 *      Cleanup function registered with vmkapi to be invoked by vmkapi when
 *      the corresponding character device is being finally destroyed.
 *
 * Results:
 *      Status regarding cleanup -- has no effect on upper layers.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
VMK_ReturnStatus
VmkplxrCharUnregCleanup(vmk_AddrCookie cookie)
{
   VmkplxrCharDev *cdev;
   int major, minor;
   vmk_HeapID heap;
   vmk_AddrCookie vmklinuxPrivate;
   vmkplxr_ChardevDestructor dtor;
   vmkplxr_ChardevHandles *chardevHandles;
   VmkplxrMinorInfo *minorInfo;
   VMK_ReturnStatus status;   
   
   chardevHandles = cookie.ptr;
   VMK_ASSERT(chardevHandles != NULL);
   
   cdev = chardevHandles->vmkplxrInfo.ptr;

   major = cdev->major;
   minor = cdev->minor;

   VMK_ASSERT(cdev->unregPending == VMK_TRUE);

   heap = vmk_ModuleGetHeapID(cdev->modID);
   VMK_ASSERT(heap != VMK_INVALID_HEAP_ID);
   vmklinuxPrivate.ptr = chardevHandles;
   dtor = cdev->dtor;

   if (dtor == NULL) {
      VMKPLXR_DEBUG(2, "Skipping invocation of a destructor - driver %d/%d "
                       "does not have one registered", major, minor);
   }
   else {
      dtor(vmklinuxPrivate);
   }

   vmk_SemaLock(&chardevRegSem);

   minorInfo = minorInfoPerMajor[major];
   minorInfo->chardevMinors[minor] = NULL;
   status = VmkplxrFreeDevNamespace(major, minor);
   VMK_ASSERT(status == VMK_OK);

   vmk_SemaUnlock(&chardevRegSem);

   vmk_HeapFree(heap, cdev);
   vmk_HeapFree(heap, chardevHandles);
   return VMK_OK;
}

