/* **********************************************************
 * Copyright 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * vmkplexer_entropy.h
 *
 *      Definitions for entropy production and consumption
 *      services among vmklinux modules and vmklinux implementations.
 */

#include "vmkapi.h"

#ifndef VMKPLEXER_ENTROPY_H
#define VMKPLEXER_ENTROPY_H

typedef void (*vmkplxr_AddEntropyFunction)(int);
typedef VMK_ReturnStatus (*vmkplxr_GetEntropyFunction)(void *entropy,
                                                        int bytesRequested,
                                                        int *bytesReturned);

typedef struct VmkplxrRandomDriver {
   vmk_ModuleID modID;
   vmkplxr_AddEntropyFunction addInterruptEntropy;
   vmkplxr_AddEntropyFunction addHwrngEntropy;
   vmkplxr_AddEntropyFunction addKeyboardEntropy;
   vmkplxr_AddEntropyFunction addMouseEntropy;
   vmkplxr_AddEntropyFunction addStorageEntropy;

   vmkplxr_GetEntropyFunction getHwRandomBytes;
   vmkplxr_GetEntropyFunction getHwrngRandomBytes;
   vmkplxr_GetEntropyFunction getHwRandomBytesNonblocking;
   vmkplxr_GetEntropyFunction getSwRandomBytes;
   vmkplxr_GetEntropyFunction getSwOnlyRandomBytes;
} VmkplxrRandomDriver;

VMK_ReturnStatus 
vmkplxr_RegisterRandomDriver(const VmkplxrRandomDriver *driver);

VMK_ReturnStatus
vmkplxr_UnregisterRandomDriver(const VmkplxrRandomDriver *driver);

void vmkplxr_AddKeyboardEntropy(int);
void vmkplxr_AddMouseEntropy(int);
void vmkplxr_AddStorageEntropy(int);

VMK_ReturnStatus
vmkplxr_GetRandomBytes(void *buf, int nbytes, int *bytesReturned);

#endif /* VMKPLEXER_ENTROPY_H */

