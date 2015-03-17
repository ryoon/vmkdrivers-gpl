/* 
 **********************************************************
 * Copyright 2010 VMware, Inc.  All rights reserved.
 **********************************************************
 */

/*
 * vmkplexer_entropy.c --
 *
 *      Multiplexing of the vmkernel's entropy API among entropy sources
 *      and entropy consumers.
 */

#include "vmkapi.h"

#include <vmkplexer_module.h>
#include <vmkplexer_entropy.h>

VmkplxrRandomDriver randomDriver;

static VMK_ReturnStatus TeardownEntropyRegistration(void);
static VMK_ReturnStatus vmkplxr_GetSwEntropy(void *buf, 
                                             int nbytes, 
                                             int *bytesReturned);

static VMK_ReturnStatus RegisterVmkplxrSWRandom(void);
static VMK_ReturnStatus UnregisterVmkplxrSWRandom(void);
/*
 * We need to keep track of whether or not we're using a 3rd-party
 * driver to handle software-only entropy-read requests, or whether
 * no such driver exists and vmkplexer is handling it instead.
 */
static int VmkplxrSWRandomInUse = 0;

VMK_ReturnStatus
vmkplxr_EntropyInit(void)
{
   VMK_ReturnStatus status;

   vmk_Memset(&randomDriver, 0x00, sizeof(randomDriver));
   randomDriver.modID = VMK_INVALID_MODULE_ID;
   status = RegisterVmkplxrSWRandom();
   VMK_ASSERT(status == VMK_OK);

   return VMK_OK;
}

/*
 * Note that registration and unregistration must work and provide
 * no synchronization -- only one random driver is supported
 */
VMK_ReturnStatus
vmkplxr_RegisterRandomDriver(const VmkplxrRandomDriver *driver)
{
   VMK_ReturnStatus status;

   if (randomDriver.modID != VMK_INVALID_MODULE_ID) {
      return VMK_BUSY;
   }

   if (driver->addInterruptEntropy == NULL ||
       driver->addKeyboardEntropy == NULL ||
       driver->addMouseEntropy == NULL ||
       driver->addStorageEntropy == NULL ||
       driver->getHwRandomBytes == NULL ||
       driver->getHwRandomBytesNonblocking == NULL ||
       driver->getSwRandomBytes == NULL) {
      return VMK_BAD_PARAM;
   }

   randomDriver.modID = driver->modID;

   status = vmk_RegisterAddEntropyFunction(driver->modID,
                                           driver->addInterruptEntropy,
                                           VMK_ENTROPY_HARDWARE_INTERRUPT);
   if (status == VMK_OK) {
      randomDriver.addInterruptEntropy = driver->addInterruptEntropy;
   }

   status = vmk_RegisterAddEntropyFunction(driver->modID,
                                           driver->addKeyboardEntropy,
                                           VMK_ENTROPY_KEYBOARD);
   if (status == VMK_OK) {
      randomDriver.addKeyboardEntropy = driver->addKeyboardEntropy;
   }
   
   status = vmk_RegisterAddEntropyFunction(driver->modID,
                                           driver->addMouseEntropy,
                                           VMK_ENTROPY_MOUSE);
   if (status == VMK_OK) {
      randomDriver.addMouseEntropy = driver->addMouseEntropy;
   }

   status = vmk_RegisterAddEntropyFunction(driver->modID,
                                           driver->addStorageEntropy,
                                           VMK_ENTROPY_STORAGE);
   if (status == VMK_OK) {
      randomDriver.addStorageEntropy = driver->addStorageEntropy;
   }

   if (driver->addHwrngEntropy) {
      status = vmk_RegisterAddEntropyFunction(driver->modID,
                                              driver->addHwrngEntropy,
                                              VMK_ENTROPY_HARDWARE_RNG);
      if (status == VMK_OK) {
         randomDriver.addHwrngEntropy = driver->addHwrngEntropy;
      }
   }

   status = vmk_RegisterGetEntropyFunction(driver->modID,
                                           driver->getHwRandomBytes,
                                           VMK_ENTROPY_HARDWARE);
   if (status == VMK_OK) {
      randomDriver.getHwRandomBytes = driver->getHwRandomBytes;
   }

   status = vmk_RegisterGetEntropyFunction(driver->modID,
                                           driver->getHwRandomBytesNonblocking,
                                           VMK_ENTROPY_HARDWARE_NON_BLOCKING);
   if (status == VMK_OK) {
      randomDriver.getHwRandomBytesNonblocking = driver->getHwRandomBytesNonblocking;
   }

   status = vmk_RegisterGetEntropyFunction(driver->modID,
                                           driver->getSwRandomBytes,
                                           VMK_ENTROPY_SOFTWARE);
   if (status == VMK_OK) {
      randomDriver.getSwRandomBytes = driver->getSwRandomBytes;
   }

   if (driver->getSwOnlyRandomBytes) {
      UnregisterVmkplxrSWRandom();
      status = vmk_RegisterGetEntropyFunction(driver->modID,
                                              driver->getSwOnlyRandomBytes,
                                              VMK_ENTROPY_SOFTWARE_ONLY);
      if (status == VMK_OK) {
         randomDriver.getSwOnlyRandomBytes = driver->getSwOnlyRandomBytes;
      }
      else {
         RegisterVmkplxrSWRandom();
      }
   }

   if (driver->getHwrngRandomBytes) {
      status = vmk_RegisterGetEntropyFunction(driver->modID,
                                              driver->getHwrngRandomBytes,
                                              VMK_ENTROPY_HARDWARE_RNG);
      if (status == VMK_OK) {
         randomDriver.getHwrngRandomBytes = driver->getHwrngRandomBytes;
      }
   }

   return VMK_OK;
}
VMK_MODULE_EXPORT_SYMBOL(vmkplxr_RegisterRandomDriver);

VMK_ReturnStatus
TeardownEntropyRegistration(void)
{
   VMK_ReturnStatus status = VMK_OK;

   if (randomDriver.modID == VMK_INVALID_MODULE_ID) {
      return VMK_BAD_PARAM;
   }
   if (randomDriver.addInterruptEntropy) {
      if (vmk_UnregisterAddEntropyFunction(randomDriver.modID, 
                                           randomDriver.addInterruptEntropy) != VMK_OK) {
         status = VMK_BUSY;
      }
   }
   if (randomDriver.addHwrngEntropy) {
      if (vmk_UnregisterAddEntropyFunction(randomDriver.modID, 
                                           randomDriver.addHwrngEntropy) != VMK_OK) {
         status = VMK_BUSY;
      }
   }
   if (randomDriver.addKeyboardEntropy) {
      if (vmk_UnregisterAddEntropyFunction(randomDriver.modID, 
                                           randomDriver.addKeyboardEntropy) != VMK_OK) {
         status = VMK_BUSY;
      }
   }

   if (randomDriver.addMouseEntropy) {
      if (vmk_UnregisterAddEntropyFunction(randomDriver.modID,
                                           randomDriver.addMouseEntropy) != VMK_OK) {
         status = VMK_BUSY;
      }
   }
   if (randomDriver.addStorageEntropy) {
      if (vmk_UnregisterAddEntropyFunction(randomDriver.modID, 
                                           randomDriver.addStorageEntropy) != VMK_OK) {
         status = VMK_BUSY;
      }
   }

   if (randomDriver.getHwRandomBytes) {
       if (vmk_UnregisterGetEntropyFunction(randomDriver.modID, 
                                            randomDriver.getHwRandomBytes) != VMK_OK) {
         status = VMK_BUSY;
      }
   }
   if (randomDriver.getHwRandomBytesNonblocking) {
      if (vmk_UnregisterGetEntropyFunction(randomDriver.modID, 
                                           randomDriver.getHwRandomBytesNonblocking) != VMK_OK) {
         status = VMK_BUSY;
      }
   }
   if (randomDriver.getSwRandomBytes) {
      if (vmk_UnregisterGetEntropyFunction(randomDriver.modID, 
                                           randomDriver.getSwRandomBytes) != VMK_OK) {
         status = VMK_BUSY;
      }
   }

   if (randomDriver.getHwrngRandomBytes) {
      if (vmk_UnregisterGetEntropyFunction(randomDriver.modID,
                                           randomDriver.getHwrngRandomBytes) != VMK_OK) {
         status = VMK_BUSY;
      }
   }

   if (randomDriver.getSwOnlyRandomBytes) {
      if (vmk_UnregisterGetEntropyFunction(randomDriver.modID, 
                                           randomDriver.getSwOnlyRandomBytes) != VMK_OK) {
         status = VMK_BUSY;
      }
      RegisterVmkplxrSWRandom();
   }
   randomDriver.modID = VMK_INVALID_MODULE_ID;

   return status;
}

VMK_ReturnStatus
vmkplxr_UnregisterRandomDriver(const VmkplxrRandomDriver *driver)
{
   VMK_ReturnStatus status;
   if (driver->modID != randomDriver.modID) {
      return VMK_BAD_PARAM;
   }

   status = TeardownEntropyRegistration();
   VMK_ASSERT(status == VMK_OK);

   vmk_Memset(&randomDriver, 0x00, sizeof(randomDriver));
   randomDriver.modID = VMK_INVALID_MODULE_ID;
    
   return status;
}
VMK_MODULE_EXPORT_SYMBOL(vmkplxr_UnregisterRandomDriver);

void
vmkplxr_AddKeyboardEntropy(int code)
{
   if (randomDriver.addKeyboardEntropy) {
      VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
      VMKAPI_MODULE_CALL_VOID(randomDriver.modID,
                              randomDriver.addKeyboardEntropy, 
                              code);
   }
}
VMK_MODULE_EXPORT_SYMBOL(vmkplxr_AddKeyboardEntropy);

void
vmkplxr_AddMouseEntropy(int code)
{
   if (randomDriver.addMouseEntropy) {
      VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
      VMKAPI_MODULE_CALL_VOID(randomDriver.modID,
                              randomDriver.addMouseEntropy,
                              code);
   }
}
VMK_MODULE_EXPORT_SYMBOL(vmkplxr_AddMouseEntropy);

void
vmkplxr_AddStorageEntropy(int diskNum)
{
   if (randomDriver.addStorageEntropy) {
      VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
      VMKAPI_MODULE_CALL_VOID(randomDriver.modID,
                              randomDriver.addStorageEntropy,
                              diskNum);
   }
}
VMK_MODULE_EXPORT_SYMBOL(vmkplxr_AddStorageEntropy);

/*
 * This is a multiplexed function to be used by vmklinux,
 * not to be registered with vmkernel, to get random bytes.
 */
VMK_ReturnStatus
vmkplxr_GetRandomBytes(void *buf, int nbytes, int *bytesReturned)
{
   VMK_ReturnStatus status;
   vmkplxr_GetEntropyFunction getDriverEntropy;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);

   getDriverEntropy = randomDriver.getHwrngRandomBytes;

   if (getDriverEntropy == NULL) {
      getDriverEntropy = randomDriver.getSwRandomBytes;
   }

   if(getDriverEntropy) {
      VMKAPI_MODULE_CALL(randomDriver.modID, status,
                         getDriverEntropy,
                         buf, nbytes, bytesReturned);
   } 
   else {
      status = vmkplxr_GetSwEntropy(buf, nbytes, bytesReturned);
   }

   return status;
}
VMK_MODULE_EXPORT_SYMBOL(vmkplxr_GetRandomBytes);


VMK_ReturnStatus
vmkplxr_GetSwEntropy(void *buf, int nbytes, int *bytesReturned)
{
   static vmk_uint32 seed;
   vmk_uint32 rand=0;

   if (seed == 0) {
      seed = vmk_GetRandSeed();
   }

   while (nbytes >= sizeof(vmk_uint32)) {
      rand = vmk_Rand(seed);
      seed = rand;
      *(vmk_uint32 *)buf = rand;
      nbytes -= sizeof(vmk_uint32);
      buf += sizeof(vmk_uint32);
   }

   if (nbytes > 0) {
      VMK_ASSERT(nbytes <= 3);
      rand = vmk_Rand(seed);
      seed = rand;
      vmk_Memcpy (buf, (unsigned char*)&rand, nbytes);
   }

   return VMK_OK;
}

VMK_ReturnStatus RegisterVmkplxrSWRandom(void)
{
   VMK_ReturnStatus status;
   
   VMK_ASSERT(VmkplxrSWRandomInUse == 0);
   status = vmk_RegisterGetEntropyFunction(vmkplxr_module_id,
                                           vmkplxr_GetSwEntropy,
                                           VMK_ENTROPY_SOFTWARE_ONLY);
   VMK_ASSERT(status == VMK_OK);
   VmkplxrSWRandomInUse = 1;
   return status;
}

VMK_ReturnStatus UnregisterVmkplxrSWRandom(void)
{
   VMK_ReturnStatus status;

   VMK_ASSERT(VmkplxrSWRandomInUse == 1);
   status = vmk_UnregisterGetEntropyFunction(vmkplxr_module_id,
                                             vmkplxr_GetSwEntropy);
   VMK_ASSERT(status == VMK_OK);
   VmkplxrSWRandomInUse = 0;

   return status;
}
