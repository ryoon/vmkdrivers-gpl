/* ****************************************************************
 * Portions Copyright 2010, 2012 VMware, Inc.
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
#include "linux_time.h"
#include "vmklinux_dist.h"

#define VMKLNX_LOG_HANDLE LinIRQ
#include "vmklinux_log.h"

// From linux/asm/irq.h
#ifndef NR_IRQS
#define NR_IRQS                 224
#endif
static unsigned int disableCount[NR_IRQS];

/*
 * Linux IRQ <-> vmk_IntrCookie mapping.
 */
typedef struct LinuxIRQ {
   vmk_IntrCookie intrCookie;

   void *dev;  /* device handle */
} LinuxIRQ;

/*
 * Linux IRQ <-> vmk_IntrCookie mapping array.
 * irqMappingLock guards this.
 * Note: NR_IRQS needs to be adjusted as per Interrupt scaling work.
 */
static LinuxIRQ irqs[NR_IRQS];

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

uint32_t
LinuxIRQ_AllocIRQ(LinuxPCIDevExt *pciDevExt, vmk_IntrCookie intrCookie)
{
   uint64_t prevIRQL;
   uint32_t i;

   /*
    * irq 0 is treated as invalid.
    * irq [1-15] reserved for ISA irqs, and we do not touch
    *            those irqs - see vmklnx_convert_isa_irq below.
    * irq [16-NR_IRQs] assigned for PCI devices.
    */
   prevIRQL = vmk_SPLockIRQ(&irqMappingLock);
   for (i = 16; i < NR_IRQS; i++ ) {
      if (irqs[i].intrCookie == VMK_INVALID_INTRCOOKIE) {
         irqs[i].intrCookie = intrCookie;
         irqs[i].dev = pciDevExt;
         break;
      }
   }
   vmk_SPUnlockIRQ(&irqMappingLock, prevIRQL);

   return i;
}

vmk_IntrCookie
LinuxIRQ_FreeIRQ(LinuxPCIDevExt *pciDevExt, uint32_t irq)
{
   vmk_IntrCookie intrCookie;
   uint64_t prevIRQL;

   VMK_ASSERT(irq && irq < NR_IRQS);

   prevIRQL = vmk_SPLockIRQ(&irqMappingLock);
   VMK_ASSERT(irqs[irq].dev == pciDevExt);
   VMK_ASSERT(irqs[irq].intrCookie != VMK_INVALID_INTRCOOKIE);
   intrCookie = irqs[irq].intrCookie;
   irqs[irq].intrCookie = VMK_INVALID_INTRCOOKIE;
   irqs[irq].dev = NULL;
   vmk_SPUnlockIRQ(&irqMappingLock, prevIRQL);

   return intrCookie;
}

vmk_IntrCookie
LinuxIRQ_VectorToCookie(uint32_t irq)
{
   VMK_ASSERT(irq && irq < NR_IRQS);
   return irqs[irq].intrCookie;
}

/**
 * vmklnx_convert_isa_irq -- Convert ISA IRQs to vmkernel vectors
 * @irq: ISA IRQ to convert
 *
 * RETURN VALUE:
 * vmkernel vector on success. 0 on failure.
 */
/* _VMKLNX_CODECHECK_: vmklnx_convert_isa_irq */
unsigned int
vmklnx_convert_isa_irq(unsigned int irq)
{
   vmk_IntrCookie intrCookie;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if (irq >= 16) {
      VMKLNX_WARN("non ISA irq %d", irq);
      VMKLNX_ASSERT_NOT_IMPLEMENTED(0);
   }
   if (irqs[irq].intrCookie != VMK_INVALID_INTRCOOKIE) {
      /* ISA irq already mapped to IntrCookie */
      return irq;
   }

   /* 11/7/2011: PR 715507. Removed checks which only allowed only 
   ISA IRQ 14 and 15 to be converted to vectors
   */

   /*
    * irq == vector in our scheme. For PCI devices, it's fine because we
    * directly store the vector in the irq field of the pci device structure.
    * For ISA devices, there is no equivalent, so the driver has to explicitly
    * call this function to get the vector we want it to use as irq.
    */
   if (vmk_ISAMapIRQToIntrCookie(irq, &intrCookie) != VMK_OK) {
      VMKLNX_WARN("no interrupt assigned for ISA irq %d", irq);
      return 0;
   }

   irqs[irq].intrCookie = intrCookie;
   VMKLNX_DEBUG(0, "Converted ISA IRQ %d to interrupt %d\n", irq, irq);
   return irq;
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
   VMK_ReturnStatus status;
   vmk_IntrProps props;
   vmk_IntrCookie intrCookie;
   LinuxPCIDevExt *pciDevExt;
   uint64_t prevIRQL;
   vmk_ModuleID moduleID;
   void *idr;
   struct device *dev = &vmklnxIRQDev;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
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

   prevIRQL = vmk_SPLockIRQ(&irqMappingLock);
   pciDevExt = (LinuxPCIDevExt *)(irqs[irq].dev);
   intrCookie = irqs[irq].intrCookie;
   vmk_SPUnlockIRQ(&irqMappingLock, prevIRQL);

   VMK_ASSERT(intrCookie != VMK_INVALID_INTRCOOKIE);

   status = vmk_NameInitialize(&props.deviceName, device);
   if (pciDevExt == NULL) {
      // exception for ISA type
      props.device = NULL;
   } else {
      props.device = pciDevExt->gDev;
   }
   props.handler = Linux_IRQHandler;
   props.acknowledgeInterrupt = NULL;
   props.handlerData = (void *) idr;
   props.attrs = (VMK_INTR_ATTRS_ENTROPY_SOURCE | VMK_INTR_ATTRS_INCOMPAT_ACK);

   status = vmk_IntrRegister(vmklinuxModID,
                             intrCookie,
                             &props);

   if (status != VMK_OK) {
      VMKLNX_WARN("Couldn't register irq 0x%x", irq);
      goto err;
   }

   if (vmk_IntrEnable(intrCookie) != VMK_OK) {
      VMKLNX_WARN("Couldn't enable irq 0x%x", irq);
      status = vmk_IntrUnregister(vmklinuxModID,
                                  intrCookie,
                                  idr);
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
   VMK_ReturnStatus status;
   vmk_IntrCookie intrCookie;
   uint64_t prevIRQL;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMKLNX_DEBUG(1, "called for irq: 0x%x, dev_id: %p", irq, dev_id);

   moduleID = vmk_ModuleStackTop();
   VMK_ASSERT(moduleID != VMK_INVALID_MODULE_ID);

   idr = irqdevres_find(dev, devm_irq_match, irq, dev_id);
   if (!idr) {
      VMK_ASSERT(0);
      return;
   }

   prevIRQL = vmk_SPLockIRQ(&irqMappingLock);
   intrCookie = irqs[irq].intrCookie;
   vmk_SPUnlockIRQ(&irqMappingLock, prevIRQL);

   if (vmk_IntrDisable(intrCookie) != VMK_OK) {
      VMKLNX_WARN("Couldn't disable irq 0x%x", irq);
   }

   status = vmk_IntrUnregister(vmklinuxModID,
                               intrCookie,
                               idr);
   VMK_ASSERT(status == VMK_OK);
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
   uint64_t prevIRQL;
   vmk_IntrCookie intrCookie = VMK_INVALID_INTRCOOKIE;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMKLNX_DEBUG(1, "called for: %d", irq);

   prevIRQL = vmk_SPLockIRQ(&irqMappingLock);

   switch (disableCount[irq]) {
   case 1:
      intrCookie = irqs[irq].intrCookie;
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

   if (intrCookie != VMK_INVALID_INTRCOOKIE &&
       vmk_IntrEnable(intrCookie) != VMK_OK) {
      VMKLNX_WARN("Cannot enable irq %d", irq);
   }
}
EXPORT_SYMBOL(enable_irq);

void
disable_irq_nosync(unsigned int irq)
{
   uint64_t prevIRQL;
   vmk_IntrCookie intrCookie = VMK_INVALID_INTRCOOKIE;

   VMKLNX_DEBUG(1, "called for: %d", irq);

   prevIRQL = vmk_SPLockIRQ(&irqMappingLock);

   if (disableCount[irq]++ == 0) {
      intrCookie = irqs[irq].intrCookie;
   }

   vmk_SPUnlockIRQ(&irqMappingLock, prevIRQL);

   if (intrCookie != VMK_INVALID_INTRCOOKIE &&
       vmk_IntrDisable(intrCookie) != VMK_OK) {
      VMKLNX_WARN("Cannot disable irq %d", irq);
   }
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
   uint64_t prevIRQL;
   vmk_IntrCookie intrCookie;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMKLNX_DEBUG(1, "called for: %d", irq);
   if (!irq || irq >= NR_IRQS) {
      return;
   }
   disable_irq_nosync(irq);

   prevIRQL = vmk_SPLockIRQ(&irqMappingLock);
   intrCookie = irqs[irq].intrCookie;
   vmk_SPUnlockIRQ(&irqMappingLock, prevIRQL);

   if (intrCookie != VMK_INVALID_INTRCOOKIE &&
       vmk_IntrSync(intrCookie) != VMK_OK) {
      VMKLNX_WARN("Cannot sync irq %d", irq);
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
   uint64_t prevIRQL;
   vmk_IntrCookie intrCookie;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   /*
    * Linux seems to bail out silently if the irq is zero or too high.
    */
   if (!irq || irq >= NR_IRQS) {
      return;
   }

   prevIRQL = vmk_SPLockIRQ(&irqMappingLock);
   intrCookie = irqs[irq].intrCookie;
   vmk_SPUnlockIRQ(&irqMappingLock, prevIRQL);

   if (intrCookie != VMK_INVALID_INTRCOOKIE &&
       vmk_IntrSync(intrCookie) != VMK_OK) {
      VMKLNX_WARN("Cannot sync irq %d", irq);
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

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if (vmk_TimerQueueIsCurrentWorld(vmklnxTimerQueue)) {
      return 1;
   }

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

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
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
   int i;
   VMKLNX_CREATE_LOG();

   for (i = 0; i < NR_IRQS; i++ ) {
      irqs[i].intrCookie = VMK_INVALID_INTRCOOKIE;
      irqs[i].dev = NULL;
   }
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
