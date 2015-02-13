/* ****************************************************************
 * Portions Copyright 2010 VMware, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * ****************************************************************/

/******************************************************************
 *
 * linux_irq.c
 *
 * From linux-2.6.18.8/kernel/irq/manage.c:
 *
 * Copyright (C) 1992, 1998-2006 Linus Torvalds, Ingo Molnar
 * Copyright (C) 2005-2006 Thomas Gleixner
 *
 ******************************************************************/

#include <linux/pci.h>
#include <linux/interrupt.h>

#include "vmkapi.h"
#include "linux_pci.h"
#include "linux_stubs.h"
#include "linux_irq.h"
#include "vmklinux_dist.h"

#define VMKLNX_LOG_HANDLE LinIRQ
#include "vmklinux_log.h"

// From linux/asm/irq.h
#ifndef NR_IRQS
#define NR_IRQS                 224
#endif
static unsigned int disableCount[NR_IRQS];

/*
 * dummy device structs - used to hold devres info for all
 * registered irqs:
 * vmklnxIRQDev - irqs registered via request_irq
 * dumpIRQDev   - irqs registered for coredump
 * 
 * No one should use these except by irq registration code.
 */
struct device vmklnxIRQDev = {};
struct device dumpIRQDev = {};

struct device * LinuxIRQ_GetDumpIRQDev(void)
{
   return &dumpIRQDev;
}

/*
 *-----------------------------------------------------------------------------
 *
 *  vmklnx_convert_isa_irq --
 *
 *    Helper function to convert isa irqs to vmkernel vectors
 *
 * Results:
 *    returns vmkernel vector on success. 0 on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
/* _VMKLNX_CODECHECK_: vmklnx_convert_isa_irq */
unsigned int
vmklnx_convert_isa_irq(unsigned int irq)
{
   vmk_uint32 vector;

   if (irq >= 16) {
      VMKLNX_WARN("non ISA irq %d", irq);
      VMKLNX_ASSERT_NOT_IMPLEMENTED(0);
   }

   if (unlikely((irq != 14) && (irq != 15))) {
      /*
       * Not supported for now
       */
      VMKLNX_WARN("unsupported ISA irq %d", irq);
      VMKLNX_ASSERT_NOT_IMPLEMENTED(0);
   }

   /*
    * irq == vector in our scheme. For PCI devices, it's fine because we
    * directly store the vector in the irq field of the pci device structure.
    * For ISA devices, there is no equivalent, so the driver has to explicitly
    * call this function to get the vector we want it to use as irq.
    */
   if (vmk_ISAMapIRQToVector(irq, &vector) != VMK_OK) {
      VMKLNX_WARN("no vector for ISA irq %d", irq);
      return 0;
   }

   VMKLNX_DEBUG(0, "Converted ISA IRQ %d to vector %d\n", irq, vector);
   return vector;
}
EXPORT_SYMBOL(vmklnx_convert_isa_irq);

/**
 *  request_irq - allocate interrupt line
 *  @irq: interrupt line to allocate
 *  @handler: function to be called when irq occurs
 *  @flags: interrupt type flags
 *  @device: An ascii name for the claiming device
 *  @dev_id: A cookie passed back to the handler funtion
 *
 *  This call allocates interrupt resources and enables the interrupt line and
 *  IRQ handling. From the point this call is made the specified handler
 *  function may be invoked. Since the specified handler function must clear any
 *  interrupt the device raises, the caller of this service must first initialize
 *  the hardware.
 *
 *  Dev_id must be globally unique. Normally the address of the device data
 *  structure is used as the dev_id "cookie". Since the handler receives this
 *  value, the cookie, it can make sense to use the data structure address.
 *
 *  If the requested interrupt is shared, then a non-NULL dev_id is required,
 *  as it will be used when freeing the interrupt.
 *
 *  Flags:
 *
 *  IRQF_SHARED        Interrupt is shared
 *  IRQF_DISABLED      Disable interrupts while processing
 *  IRQF_SAMPLE_RANDOM The interrupt can be used for entropy
 *
 *  ESX Deviation Notes:
 *  All interrupt handlers are used as a source for entropy, regardless of
 *  IRQF_SAMPLE_RANDOM being set in the flags or not.
 *
 */
/* _VMKLNX_CODECHECK_: request_irq */
int
request_irq(unsigned int irq,
                   irq_handler_t handler,
                   unsigned long flags,
                   const char *device,
                   void *dev_id)
{
   vmk_VectorAddHandlerOptions addOptions;
   vmk_ModuleID moduleID;
   void *idr;
   struct device *dev = &vmklnxIRQDev;

   VMKLNX_DEBUG(1, "called for irq: 0x%x name: %s", irq, device);

   if (!irq || irq >= NR_IRQS) {
      VMKLNX_WARN("Invalid irq 0x%x", irq);
      return -1;
   }

   moduleID = vmk_ModuleStackTop();
   VMK_ASSERT(moduleID != VMK_INVALID_MODULE_ID);

   /* register irq with devres */
   idr = irqdevres_register(dev, irq, handler, dev_id);
   if (!idr) {
      VMK_ASSERT(0);
      return -1;
   }

   if (flags & IRQF_SAMPLE_RANDOM) {
      VMKLNX_DEBUG(2, "Enabling entropy sampling for device %s with moduleID %"VMK_FMT64"x",
                   device != NULL ? device : "unknown",
                   vmk_ModuleGetDebugID(moduleID));
   } else {
      VMKLNX_DEBUG(1, "Overriding driver, enabling entropy sampling for "
                   "device %s with moduleID %"VMK_FMT64"x",
                   device != NULL ? device : "unknown",
                   vmk_ModuleGetDebugID(moduleID));
   }

   addOptions.sharedVector = (flags & SA_SHIRQ) != 0;
   addOptions.entropySource = VMK_TRUE; /* entropy source for random device*/

   if (vmk_AddInterruptHandler(irq,
                               device,
                               Linux_IRQHandler,
                               idr,
                               &addOptions
                               ) != VMK_OK) {
      VMKLNX_WARN("Couldn't register vector 0x%x", irq);
      goto err;
   }

   if (vmk_VectorEnable(irq) != VMK_OK) {
      VMKLNX_WARN("Couldn't enable vector 0x%x", irq);
      vmk_RemoveInterruptHandler(irq, idr);
      goto err;
   }

   VMKLNX_DEBUG(0, "Registered handler for irq: 0x%x name: %s", irq, device);
   return 0;

err:
   irqdevres_unregister(dev, irq, dev_id);
   return -1;
}
EXPORT_SYMBOL(request_irq);

/**
 *  free_irq - free an interrupt allocated with request_irq
 *  @irq: Interrupt line to free
 *  @dev_id: Data structure uniquely identifying device making the request.
 *
 *  Removes previously installed interrupt handler for given interrupt line and device.
 *
 *  RETURN VALUE:
 *  None
 *
 */
/* _VMKLNX_CODECHECK_: free_irq */
void
free_irq(unsigned int irq, void *dev_id)
{
   struct device *dev = &vmklnxIRQDev;
   vmk_ModuleID moduleID;
   void *idr;

   VMKLNX_DEBUG(1, "called for irq: 0x%x, dev_id: %p", irq, dev_id);

   moduleID = vmk_ModuleStackTop();
   VMK_ASSERT(moduleID != VMK_INVALID_MODULE_ID);

   idr = irqdevres_find(dev, devm_irq_match, irq, dev_id);
   if (!idr) {
      VMK_ASSERT(0);
      return;
   }

   vmk_RemoveInterruptHandler(irq, idr);
   irqdevres_unregister(dev, irq, dev_id);
   VMKLNX_DEBUG(0, "Un-registered handler for irq: 0x%x dev_id: %p", irq, dev_id);
}
EXPORT_SYMBOL(free_irq);

/**
 *  enable_irq - enable interrupt handling on an IRQ
 *  @irq: interrupt to enable
 *
 *  Enables interrupts for the given IRQ
 *
 *  RETURN VALUE:
 *  This function does not return a value
 */
/* _VMKLNX_CODECHECK_: enable_irq */
void
enable_irq(unsigned int irq)
{
   uint32_t vector = irq;
   uint64_t prevIRQL;

   VMKLNX_DEBUG(1, "called for: %d", irq);

   prevIRQL = vmk_SPLockIRQ(&irqMappingLock);

   switch (disableCount[irq]) {
   case 1:
      if (vmk_VectorEnable(vector) != VMK_OK) {
         VMKLNX_WARN("Cannot enable vector %d", vector);
      }
      // falls through
   default:
      disableCount[irq]--;
      break;
   case 0:
      VMKLNX_WARN("enable_irq(%u) unbalanced from %p", irq,
                      __builtin_return_address(0));
      break;
   }

   vmk_SPUnlockIRQ(&irqMappingLock, prevIRQL);
}
EXPORT_SYMBOL(enable_irq);

void
disable_irq_nosync(unsigned int irq)
{
   uint32_t vector = irq;
   uint64_t prevIRQL;

   VMKLNX_DEBUG(1, "called for: %d", irq);

   prevIRQL = vmk_SPLockIRQ(&irqMappingLock);

   if (disableCount[irq]++ == 0) {
      if (vmk_VectorDisable(vector) != VMK_OK) {
         VMKLNX_WARN("Cannot disable vector %d", irq);
      }
   }

   vmk_SPUnlockIRQ(&irqMappingLock, prevIRQL);
}

/**
 *  disable_irq - disable an irq and synchronously wait for completion
 *  @irq: The interrupt request line to be disabled.
 *
 *  Disable the selected interrupt line.  Then wait for any in progress
 *  interrupts on this irq (possibly executing on other CPUs) to complete.
 *
 *  Enables and Disables nest, in the sense that n disables are needed to
 *  counteract n enables.
 *
 *  If you use this function while holding any resource the IRQ handler
 *  may need, you will deadlock.
 *
 *  ESX Deviation Notes:
 *  This function should not fail in ordinary circumstances.  But if failure
 *  does occur, the errors are reported to the vmkernel log.
 *
 * RETURN VALUE:
 * None.
 */
/* _VMKLNX_CODECHECK_: disable_irq */
void
disable_irq(unsigned int irq)
{
   uint32_t vector = irq;

   VMKLNX_DEBUG(1, "called for: %d", irq);
   disable_irq_nosync(irq);
   if (vmk_VectorSync(vector) != VMK_OK) {
      VMKLNX_WARN("Cannot sync vector %d", vector);
   }
}
EXPORT_SYMBOL(disable_irq);

/*
 * drivers want to only use the good version of synchronize_irq()
 * which takes a vector if the kernel is < KERNEL_VERSION(2,5,28)
 * but ours isn't, so we manually fix up the couple of places
 * where we need to do this.
 */
/**
 *  synchronize_irq - wait for pending IRQ handlers (on other CPUs)
 *  @irq: irq number to be synchronized
 *
 *  This function waits for any pending IRQ handlers for this interrupt to complete before returning.
 *  If you use this function while holding a resource the IRQ handler may need you will deadlock.
 *
 */
/* _VMKLNX_CODECHECK_: synchronize_irq */
void
synchronize_irq(unsigned int irq)
{
   uint32_t vector = irq;

   /*
    * Linux seems to bail out silently if the vector is zero or too high.
    */
   if (vector && vector < NR_IRQS) {
      if (vmk_VectorSync(vector) != VMK_OK) {
         VMKLNX_WARN("Cannot sync vector %d", vector);
      }
   }
}
EXPORT_SYMBOL(synchronize_irq);

/**                                          
 *  in_interrupt - Checks if the caller is in hardirq or softirq interrupt context.
 *                                           
 *  Checks if the caller is in hardirq or softirq interrupt context.
 *                                           
 *                                           
 *  RETURN VALUE:
 *  non-zero if it is in interrupt context and zero otherwise.
 *
 */                                          
/* _VMKLNX_CODECHECK_: in_interrupt */
int
in_interrupt(void)
{
   vmk_ContextType type;

   type = vmk_ContextGetCurrentType();
   return (vmk_ContextTypeCanBlock(type) == VMK_FALSE);
}
EXPORT_SYMBOL(in_interrupt);

/**                                          
 *  in_irq - Checks if the caller is in hardware interrupt context.
 *                                           
 *  Checks if the caller is in hardware interrupt context.
 *                                           
 *  RETURN VALUE:
 *  non-zero if it is in interrupt context and zero otherwise.
 */                                          
/* _VMKLNX_CODECHECK_: in_irq */
int
in_irq(void)
{
   vmk_ContextType type;

   type = vmk_ContextGetCurrentType();
   return ((type == VMK_CONTEXT_TYPE_INTERRUPT) ||
           (type == VMK_CONTEXT_TYPE_NMI));
}
EXPORT_SYMBOL(in_irq);

/*
 *-----------------------------------------------------------------------------
 *
 * LinuxIRQ_Init --
 *
 *      Initialize the Linux IRQ emulation subsystem
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
LinuxIRQ_Init(void)
{
   VMKLNX_CREATE_LOG();

   device_initialize(&vmklnxIRQDev);
   device_initialize(&dumpIRQDev);
}

/*
 *-----------------------------------------------------------------------------
 *
 * LinuxIRQ_Cleanup --
 *
 *      Shutdown the Linux IRQ emulation subsystem
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
LinuxIRQ_Cleanup(void)
{
   VMKLNX_DESTROY_LOG();
}
