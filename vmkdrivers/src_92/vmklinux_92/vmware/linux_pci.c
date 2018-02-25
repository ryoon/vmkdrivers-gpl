/* ****************************************************************
 * Portions Copyright 2003, 2007-2014 VMware, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * ****************************************************************/

#include <linux/pci.h>
#include <linux/proc_fs.h>
#include <linux/list.h>
#include <linux/log2.h>

#include "vmkapi.h"
#include "vmklinux_dist.h"
#include "linux_pci.h"
#include "linux_stubs.h"
#include "../linux/pci/pci.h"
#include "linux_hashtab.h"
#include "linux_irq.h"

#define VMKLNX_LOG_HANDLE LinPCI
#include "vmklinux_log.h"

struct bus_type pci_bus_type = {
   .name = "pci",
#if defined(__VMKLNX__)
   .owner =        THIS_MODULE,
#endif
};
EXPORT_SYMBOL(pci_bus_type);

extern void pci_announce_device_to_drivers(struct pci_dev *dev);

static struct vmklnx_hashtab linuxPCIBuses;     /* Table of busses */
static struct list_head      linuxPCIBusList;   /* List of busses */
static int linuxPCIBusCount;                    /* number of busses */

/*
 * System specific information attached to "struct pci_bus".
 */
struct PCI_sysdata {
   uint16_t     domain;
};

struct PCI_busdata {
   struct vmklnx_hashtab_item   hash_links;     /* for hash indexing */
   struct list_head             links;          /* for linear indexing */
   struct pci_bus               linuxPCIBus;    /* bus data */
   struct PCI_sysdata           sysdata;        /* system specific data */
};

#define PCI_HASH_ORDER          5               /* hash table size 32 */
#define PCI_HASH_KEY(dom, bus)  (((dom) << 8)|(bus))

// forward references
static struct pci_bus * LinuxPCIGetBus(uint16_t domain, uint8_t bus);
static void LinuxPCIFreeBusses(void);
static void LinuxPCIDeviceInserted(vmk_Device dev, vmk_PCIDevice vmkDev);
static void LinuxPCIDeviceRemoved(vmk_PCIDevice vmkDev);

/*
 * vmklinux native pci driver ops
 */
static VMK_ReturnStatus
LinuxPCIAttach(vmk_Device dev)
{
   VMK_ReturnStatus status = VMK_OK;
   vmk_AddrCookie drvData;
   LinuxPCIDevExt *pciDevExt = NULL;
   vmk_PCIDevice pciDev;

   status = vmk_DeviceGetRegistrationData(dev, (vmk_AddrCookie *)&pciDev);
   if (status != VMK_OK || pciDev == NULL) {
      VMK_ASSERT(0);
      return VMK_NOT_FOUND;
   }

   LinuxPCIDeviceInserted(dev, pciDev);

   // now set the driver data
   drvData.ptr = (void *) pciDevExt;
   vmk_DeviceSetAttachedDriverData(dev, drvData);

   return status;
}

static VMK_ReturnStatus
LinuxPCIDetach(vmk_Device dev)
{
   VMK_ReturnStatus status = VMK_OK;
   vmk_AddrCookie drvData;
   LinuxPCIDevExt *pciDevExt;
   vmk_PCIDevice pciDev;

   status = vmk_DeviceGetRegistrationData(dev, (vmk_AddrCookie *)&pciDev);
   if (status != VMK_OK || pciDev == NULL) {
      VMK_ASSERT(0);
      return VMK_NOT_FOUND;
   }

   status = vmk_DeviceGetAttachedDriverData(dev, &drvData);
   VMK_ASSERT(status == VMK_OK);

   /*
    * TODO: use this to avoid lookup of pci dev in
    * LinuxPCIDeviceRemoved
    */
   pciDevExt = (LinuxPCIDevExt *) drvData.ptr;

   LinuxPCIDeviceRemoved(pciDev);

   return status;
}

static VMK_ReturnStatus
LinuxPCIScan(vmk_Device dev)
{
   /* dummy callback */

   return VMK_OK;
}

static VMK_ReturnStatus
LinuxPCIQuiesce(vmk_Device dev)
{
   /* dummy callback */

   return VMK_OK;
}

static VMK_ReturnStatus
LinuxPCIStart(vmk_Device dev)
{
   /* dummy callback */

   return VMK_OK;
}

static void 
LinuxPCIForget(vmk_Device dev)
{
   /* dummy callback */
}

static vmk_Driver LinuxPCIDriver;

static vmk_DriverOps LinuxPCIDriverOps =
{
   .attachDevice    = LinuxPCIAttach,
   .scanDevice      = LinuxPCIScan,
   .detachDevice    = LinuxPCIDetach,
   .quiesceDevice   = LinuxPCIQuiesce,
   .startDevice     = LinuxPCIStart,
   .forgetDevice    = LinuxPCIForget,
};

static inline void
LinuxPCIMSIIntrVectorSet(LinuxPCIDevExt *pciDevExt, vmk_uint32 vector)
{
   pciDevExt->linuxDev.irq = vector;
   pciDevExt->linuxDev.msi_enabled = 1;
}

static inline void
LinuxPCILegacyIntrVectorSet(LinuxPCIDevExt *pciDevExt)
{
   vmk_IntrCookie intrCookie;
   uint32_t irq;
   VMK_ReturnStatus status;

   VMK_ASSERT(pciDevExt->vmkDev);

   status = vmk_PCIAllocIntrCookie(vmklinuxModID,
                                   pciDevExt->vmkDev,
                                   VMK_PCI_INTERRUPT_TYPE_LEGACY,
                                   1, 1, NULL,
                                   &intrCookie,
                                   NULL);

   if (status != VMK_OK) {
      VMKLNX_WARN("Could not allocate legacy PCI interrupt for device %s",
                  pciDevExt->linuxDev.dev.bus_id);
      /*
       * Ensure that the irq field is set to zero to indicate not to
       * attempt to free the interrupt cookie.
       */
      pciDevExt->linuxDev.irq = 0;
      return;
   }

   /*
    * Generate an irq for the given intrCookie.
    */
   irq = LinuxIRQ_AllocIRQ(pciDevExt, intrCookie);
   VMK_ASSERT(irq && irq < NR_IRQS);
   pciDevExt->linuxDev.irq = irq;
}

static inline void
LinuxPCIIntrVectorFree(LinuxPCIDevExt *pciDevExt)
{
   VMK_ReturnStatus status;

   /*
    * Check whether the device even has a currently valid,
    * allocated interrupt.  We attempt to allocate a legacy interrupt
    * on device-insert, but not all devices support legacy interrupts.
    */
   if (pciDevExt->linuxDev.irq == 0) {
      VMK_ASSERT(pciDevExt->linuxDev.msi_enabled == 0);
      return;
   }

   LinuxIRQ_FreeIRQ(pciDevExt, pciDevExt->linuxDev.irq);

   status = vmk_PCIFreeIntrCookie(vmklinuxModID, pciDevExt->vmkDev);
   VMK_ASSERT(status == VMK_OK);

   pciDevExt->linuxDev.msi_enabled = 0;
}

static struct pci_dev *
LinuxPCIFindDeviceByHandle(vmk_PCIDevice vmkDev)
{
   // search the pci_devices list
   struct pci_dev *linuxDev;

   down_read(&pci_bus_sem);
   linuxDev = NULL;
   list_for_each_entry(linuxDev, &pci_devices, global_list) {
      if (container_of(linuxDev, LinuxPCIDevExt, linuxDev)->vmkDev == vmkDev) {
         up_read(&pci_bus_sem);
         return linuxDev;
      }
   }
   up_read(&pci_bus_sem);

   return NULL;
}

inline static vmk_Bool
LinuxPCIDeviceIsClaimed(LinuxPCIDevExt *pciDevExt)
{
   return pciDevExt->moduleID != VMK_INVALID_MODULE_ID ? VMK_TRUE : VMK_FALSE;
}

void
LinuxPCI_DeviceUnclaimed(LinuxPCIDevExt *pciDevExt)
{  
   VMK_ASSERT(pciDevExt->linuxDev.driver == NULL);
   VMK_ASSERT(pciDevExt->linuxDev.dev.driver == NULL);

   pciDevExt->moduleID = VMK_INVALID_MODULE_ID;

   VMKLNX_INFO("Device %s unclaimed.", pciDevExt->linuxDev.dev.bus_id);
}

/*
 * Free the memory associated with the LinuxPCIDevExt struct of the device
 */
static void
LinuxPCIDeviceFree(struct device *dev)
{
   struct pci_dev *linuxDev;
   LinuxPCIDevExt *pciDevExt;
   vmk_DMAEngine engine;
   VMK_ReturnStatus status;

   linuxDev = to_pci_dev(dev);
   pciDevExt = container_of(linuxDev, LinuxPCIDevExt, linuxDev);

   VMKLNX_INFO("Device " PCI_DEVICE_BUS_ADDRESS " freed.", 
      ((struct PCI_sysdata *)(linuxDev->bus->sysdata))->domain,
      linuxDev->bus->number,
      PCI_SLOT(linuxDev->devfn),
      PCI_FUNC(linuxDev->devfn));

   engine = (vmk_DMAEngine)dev->dma_engine_primary;
   if (engine != VMK_DMA_ENGINE_INVALID) {
      status = vmk_DMAEngineDestroy(engine);
      if (status != VMK_OK) {
         VMK_ASSERT(status == VMK_OK);
         /* XXX Nothing to do but leak the engine and log a notice */
         VMKLNX_INFO("Couldn't destroy DMA engine on device '%s': %s",
                     dev_name(dev), vmk_StatusToString(status));
      }
   }
   
   engine = (vmk_DMAEngine)dev->dma_engine_secondary;
   if (engine != VMK_DMA_ENGINE_INVALID) {
      status = vmk_DMAEngineDestroy(engine);
      if (status != VMK_OK) {
         VMK_ASSERT(status == VMK_OK);
         /* XXX Nothing to do but leak the engine and log a notice */
         VMKLNX_INFO("Couldn't destroy DMA engine on device '%s': %s",
                     dev_name(dev), vmk_StatusToString(status));
      }
   }

   pciDevExt->magic = 0;
   vmk_HeapFree(VMK_MODULE_HEAP_ID, pciDevExt);
}

/*
 * This function will be called whenever a device is newly visible for
 * vmklinux. It is modeled after pci_insert_device, the function which would
 * be called in a linux system.
 *
 * Notes:
 * - vmklinux now receives info only for those devices it has attached.
 * - no bridges
 */
static void 
LinuxPCIDeviceInserted(vmk_Device gDev,
                       vmk_PCIDevice vmkDev)
{
   LinuxPCIDevExt *pciDevExt;
   struct pci_dev *linuxDev;
   struct pci_bus *linuxBus;
   vmk_PCIDeviceID devId;
   vmk_PCIDeviceAddr sbdf;
   vmk_PCIResource res[VMK_PCI_NUM_BARS];
   VMK_ReturnStatus status;
   int i;
 
   status = vmk_PCIQueryDeviceAddr(vmkDev, &sbdf);
   VMK_ASSERT(status == VMK_OK);

   status = vmk_PCIQueryDeviceID(vmkDev, &devId);
   VMK_ASSERT(status == VMK_OK);

   linuxDev = LinuxPCIFindDeviceByHandle(vmkDev);

   VMK_ASSERT(linuxDev == NULL);
   if (unlikely(linuxDev != NULL)) {
      VMKLNX_WARN("Already received device %s.", linuxDev->dev.bus_id);

      pciDevExt = container_of(linuxDev, LinuxPCIDevExt, linuxDev);
      VMK_ASSERT(pciDevExt->vmkDev == vmkDev);
      goto existing_device;
   }

   pciDevExt = vmk_HeapAlloc(VMK_MODULE_HEAP_ID, sizeof(LinuxPCIDevExt));
   if (unlikely(pciDevExt == NULL)) {
      VMKLNX_ALERT("Out of memory");
      return;
   }
   linuxBus = LinuxPCIGetBus(sbdf.seg, sbdf.bus);
   if (unlikely(linuxBus == NULL)) {
      VMKLNX_ALERT("failed to find bus");
      vmk_HeapFree(VMK_MODULE_HEAP_ID, pciDevExt);
      return;
   }

   memset(pciDevExt, 0, sizeof(LinuxPCIDevExt));

   pciDevExt->magic = VMKLNX_PCIDEV_MAGIC;

   linuxDev = &pciDevExt->linuxDev;
   linuxDev->dev.dev_type = PCI_DEVICE_TYPE;
   linuxDev->bus = linuxBus;
   linuxDev->devfn = PCI_DEVFN(sbdf.dev, sbdf.fn);
   linuxDev->vendor = devId.vendorID;
   linuxDev->device = devId.deviceID;
   linuxDev->class = (devId.classCode << 8) 
                      | (devId.progIFRevID >> 8);
   linuxDev->revision = devId.progIFRevID;
   /*
    * Linux expects only the header type(not the multi-functionness)
    */
   linuxDev->hdr_type = PCI_HEADER_TYPE_NORMAL; 
   linuxDev->subsystem_vendor = devId.subVendorID;
   linuxDev->subsystem_device = devId.subDeviceID;
   /*
    * vmkernel uses vector for irq
    */
   linuxDev->dev.dma_mask = &linuxDev->dma_mask;
   linuxDev->dma_mask = DMA_32BIT_MASK;
   device_initialize(&linuxDev->dev);
   linuxDev->dev.release = LinuxPCIDeviceFree;

   snprintf(linuxDev->dev.bus_id, sizeof(linuxDev->dev.bus_id),
            PCI_DEVICE_BUS_ADDRESS, sbdf.seg, sbdf.bus, sbdf.dev, sbdf.fn);

   pciDevExt->vmkDev = vmkDev;
   pciDevExt->gDev = gDev;

   /*
    * The device is not claimed yet/may not claimed at all within vmklinux. 
    */ 
   pciDevExt->moduleID = VMK_INVALID_MODULE_ID;

   status = vmk_PCIQueryIOResources(vmkDev, VMK_PCI_NUM_BARS, &res[0]);
   VMK_ASSERT(status == VMK_OK);

   for (i = 0; i < VMK_PCI_NUM_BARS; i++) {
      if (res[i].size == 0) {
         continue;
      }
      linuxDev->resource[i].start = res[i].start;
      linuxDev->resource[i].end = res[i].start + res[i].size - 1;
      linuxDev->resource[i].flags = res[i].flags |
         pci_calc_resource_flags(res[i].flags);
   }
   pci_fill_rom_bar(linuxDev, PCI_ROM_ADDRESS);


   down_write(&pci_bus_sem);
   list_add_tail(&linuxDev->global_list, &pci_devices);
   if (is_vmvisor) {
      pci_proc_attach_device(linuxDev);
   }
   up_write(&pci_bus_sem);

   LinuxPCILegacyIntrVectorSet(pciDevExt);

   VMKLNX_DEBUG(2, "Received device %s using vector %d.",
                linuxDev->dev.bus_id, (unsigned int)linuxDev->irq);

existing_device:
   pci_announce_device_to_drivers(linuxDev);
}

/*
 * This function will be called whenever a device is newly invisible for
 * vmklinux. It is modelled after pci_remove_device, the function which would
 * be called in a linux system.
 */
static void 
LinuxPCIDeviceRemoved(vmk_PCIDevice vmkDev)
{
   LinuxPCIDevExt *pciDevExt;
   struct pci_dev *linuxDev;
   char vmkDevName[VMK_PCI_DEVICE_NAME_LENGTH];

   linuxDev = LinuxPCIFindDeviceByHandle(vmkDev);

   if (unlikely(linuxDev == NULL)) {
      uint8_t bus = 0, slot = 0, func = 0;
      uint16_t domain = 0;
      VMK_ReturnStatus status;

      status = vmk_PCIGetSegmentBusDevFunc(vmkDev, &domain, &bus, &slot, &func);
      if (status == VMK_OK) {
         VMKLNX_INFO("Device remove failed. Device " PCI_DEVICE_BUS_ADDRESS " not a vmklinux device.", 
                     domain, bus, slot, func); 
      } else {
         VMKLNX_WARN("Device remove failed. Unknown device (%s).", vmk_StatusToString(status));
      }
      return;
   }

   if (unlikely(
         vmk_PCIGetDeviceName(vmkDev, vmkDevName, sizeof(vmkDevName)-1) != VMK_OK)) {
      vmkDevName[0] = 0;
   }
   VMKLNX_DEBUG(2, "Remove %s %s", linuxDev->dev.bus_id, vmkDevName);

   pciDevExt = container_of(linuxDev, LinuxPCIDevExt, linuxDev);
   VMK_ASSERT(pciDevExt->vmkDev == vmkDev);

   if (unlikely(LinuxPCIDeviceIsClaimed(pciDevExt) == VMK_FALSE)) {
      VMKLNX_INFO("Device %s %s is not claimed by vmklinux drivers",
                  linuxDev->dev.bus_id, vmkDevName);
      goto quit;
   }

   if (unlikely(linuxDev->driver == NULL)) {
      VMKLNX_WARN("no driver (or not hotplug compatible)");
      goto quit;
   }

   if (unlikely(linuxDev->driver->remove == NULL)) {
      VMKLNX_INFO("no remove function");
      goto quit;
   }

   vmk_PCIDoPreRemove(pciDevExt->moduleID, vmkDev);
   VMKAPI_MODULE_CALL_VOID(pciDevExt->moduleID, 
                      linuxDev->driver->remove, 
                      linuxDev);

   /* Only call for devres mananged device, different from Linux */
   if (pci_is_managed(linuxDev)) {
      /* VMKAPI_MODULE_CALL_VOID wrapper is required for inter-module
       * call. devres_release_all() will call driver's release function
       * for every managed resource, this should be done in driver's
       * context in ESX (different from Linux)
       */
      VMKAPI_MODULE_CALL_VOID(pciDevExt->moduleID, devres_release_all, &linuxDev->dev);
   }

   linuxDev->driver = NULL;
   linuxDev->dev.driver = NULL;
   LinuxPCI_DeviceUnclaimed(pciDevExt);

quit:
   /* free the legacy interrupt setup during LinuxPCIDeviceInserted() */
   LinuxPCIIntrVectorFree(pciDevExt);

   /* If device is physically removed, free up the structures */
   down_write(&pci_bus_sem);
   list_del(&linuxDev->global_list);
   up_write(&pci_bus_sem);

   VMKLNX_INFO("Removed device %s.", linuxDev->dev.bus_id);

   if (is_vmvisor) {
      pci_proc_detach_device(linuxDev);
   }
   pci_dev_put(linuxDev);
}

int proc_initialized = 0;
struct proc_dir_entry *proc_bus_pci_dir;
struct proc_dir_entry *proc_bus_pci_devices;
extern int proc_bus_pci_devices_read(char *page, char **start, off_t off, int count, int *eof, void *data);

void
LinuxPCI_Init(void)
{
   VMK_ReturnStatus status;
   vmk_DriverProps drvProps;
   static vmk_Bool initialized = VMK_FALSE;

   if (unlikely(initialized)) {
      return;
   }
   initialized = VMK_TRUE;
   proc_initialized = 1;

   VMKLNX_CREATE_LOG();

   /*
    * Each device has a pointer to the pci_bus structure of the bus it
    * resides on, but it seems to only use the number field of that
    * structure. We dynamically allocate these pci_bus structures in
    * a hash table because domain support implies that we don't know
    * how many busses can exist at system initialization time.
    */
   if (unlikely(vmklnx_hashtab_create(&linuxPCIBuses, PCI_HASH_ORDER) != 0)) {
      VMKLNX_ALERT("failed to create PCI bus hash table.");
      return;
   }

   INIT_LIST_HEAD(&linuxPCIBusList);

   if (is_vmvisor) {
      proc_bus_pci_dir = proc_mkdir("pci", proc_bus);
      proc_bus_pci_devices = create_proc_entry("devices", 0, proc_bus_pci_dir);
      VMK_ASSERT(proc_bus_pci_devices != NULL);
      proc_bus_pci_devices->read_proc = proc_bus_pci_devices_read;
   }

   drvProps.moduleID = vmklinuxModID;
   status = vmk_NameInitialize(&drvProps.name, VMKLINUX_NAME);
   VMK_ASSERT(status == VMK_OK);
   drvProps.ops = &LinuxPCIDriverOps;
   //drvProps.privateData = NULL;

   status = vmk_DriverRegister(&drvProps, &LinuxPCIDriver);
   VMK_ASSERT(status == VMK_OK);

   return;
}

void
LinuxPCI_Cleanup(void)
{
   vmk_DriverUnregister(LinuxPCIDriver);

   VMKLNX_DESTROY_LOG();
}

vmk_Bool
LinuxPCI_DeviceIsPAECapable(struct pci_dev *dev)
{
   if ((dev->dma_mask & 0xfffffffffULL) == 0xfffffffffULL) {
      VMKLNX_INFO("PAE capable device at %s", dev->dev.bus_id);
      return VMK_TRUE;
   } else {
      /* the following assert is not necessary.  It is simply here to warn
       * us if we ever run into a device that can dma to more than 4GB, but
       * less than 64GB */
      VMK_ASSERT((dev->dma_mask & 0x7ffffffffULL) 
              == (dev->dma_mask & 0xffffffffULL));
      return VMK_FALSE;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * LinuxPCI_DeviceClaimed --
 *
 *      Old-style (i.e. no pci_driver) drivers automatically claim devices
 *      through this during device registration with the I/O subsystems.  
 *
 * Results:
 *      None. 
 *
 * Side effects:
 *      Current module is associated with device. 
 *
 *-----------------------------------------------------------------------------
 */

void
LinuxPCI_DeviceClaimed(LinuxPCIDevExt *pciDevExt, vmk_ModuleID moduleID)
{
   vmk_PCIDevice vmkDev;

   pciDevExt->moduleID = moduleID;
   vmkDev = pciDevExt->vmkDev;
   vmk_PCIDoPostInsert(moduleID, vmkDev);

   VMKLNX_DEBUG(2, "Device %s claimed.", pciDevExt->linuxDev.dev.bus_id);
}

/*
 *----------------------------------------------------------------------------
 *
 *  LinuxPCI_FindDevByBusSlot --
 *
 *    Get struct pci_dev from (domain, bus, slot, fn)
 *
 *  Results:
 *    Pointer to struct pci_dev. NULL if no device matches the key.
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

struct pci_dev *
LinuxPCI_FindDevByBusSlot(uint16_t domain, uint16_t bus, uint16_t devfn)
{
   struct pci_dev *linuxDev;

   linuxDev = NULL;
   for_each_pci_dev(linuxDev) {
      if (((struct PCI_sysdata *)(linuxDev->bus->sysdata))->domain == domain &&
          linuxDev->bus->number == bus && linuxDev->devfn == devfn) {
         return linuxDev;
      }
   }

   return NULL;
}



/*
 *----------------------------------------------------------------------------
 *
 *  LinuxPCI_FindCapability --
 *
 *    Return the index to the specified capability in the config space of
 *    the given device.
 *
 *  Results:
 *    Index to the capability. 0, if the device doesn't have the specified
 *    capability.
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

int
LinuxPCI_FindCapability(uint16_t domain, uint16_t bus, uint16_t devfn, uint8_t cap)
{
   vmk_PCIDevice vmkDev;
   vmk_uint16 capIdx;

   if (unlikely(vmk_PCIGetPCIDevice(domain,
                                    bus, 
                                    PCI_SLOT(devfn), 
                                    PCI_FUNC(devfn), 
                                    &vmkDev) != VMK_OK)) {
      return 0;
   }

   if (unlikely(vmk_PCIFindCapability(vmkDev, cap, &capIdx) != VMK_OK)) {
      return 0;
   }

   return capIdx;
}

/*
 *----------------------------------------------------------------------------
 *
 *  LinuxPCI_EnableMSI --
 *
 *    Allocate vectors and enable MSI on the specified device.
 *
 *  Results:
 *    VMK_ReturnStatus indicating the outcome.
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

VMK_ReturnStatus
LinuxPCI_EnableMSI(struct pci_dev* dev)
{
   vmk_uint32 vector;
   LinuxPCIDevExt *pciDevExt;
   vmk_IntrCookie intrCookie;
   VMK_ReturnStatus status;

   pciDevExt = container_of(dev, LinuxPCIDevExt, linuxDev);
   VMK_ASSERT(pciDevExt->vmkDev);

   /*
    * Remove the previous legacy interrupt, if it exists,
    * before requesting MSI interrupt.
    */
   LinuxPCIIntrVectorFree(pciDevExt);

   status = vmk_PCIAllocIntrCookie(vmklinuxModID,
                                   pciDevExt->vmkDev,
                                   VMK_PCI_INTERRUPT_TYPE_MSI,
                                   1, 1, NULL,
                                   &intrCookie,
                                   NULL);

   if (unlikely(status != VMK_OK)) {
      VMKLNX_WARN("%s: Interrupt allocation failed with %s", 
                  pci_name(dev), vmk_StatusToString(status));
      /*
       * Restore the legacy default PCI interrupt vector.
       */
      LinuxPCILegacyIntrVectorSet(pciDevExt);
      return status;
   }

   /*
    * Generate an irq for the given intrCookie.
    */
   vector = LinuxIRQ_AllocIRQ(pciDevExt, intrCookie);
   VMK_ASSERT(vector && vector < NR_IRQS);

   /*
    * trickle MSI vector down to driver land
    */
   LinuxPCIMSIIntrVectorSet(pciDevExt, vector);

   return VMK_OK;
}

void
LinuxPCI_DisableMSI(struct pci_dev* dev)
{
   LinuxPCIDevExt *pciDevExt;

   if (!dev || !dev->msi_enabled)
      return;

   pciDevExt = container_of(dev, LinuxPCIDevExt, linuxDev);

   /*
    * Remove the MSI interrupt.
    */
   LinuxPCIIntrVectorFree(pciDevExt);
   
   /*
    * Restore the legacy default PCI interrupt vector
    */
   LinuxPCILegacyIntrVectorSet(pciDevExt);
}

/*
 *----------------------------------------------------------------------------
 *
 *  LinuxPCI_EnableMSIX --
 *
 *    Allocate vectors and enable MSI-X on the specified device. The new
 *    vectors will be added to the previous set of vectors allocated to the
 *    device.
 *
 *  Results:
 *    VMK_ReturnStatus indicating the outcome. Newly allocated vectors are
 *    returned in 'entries'.
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

VMK_ReturnStatus
LinuxPCI_EnableMSIX(struct pci_dev* dev, struct msix_entry *entries,
                    int nvecs, vmk_Bool bestEffortAllocation,
                    int *nvecs_alloced)
{
   int i;
   LinuxPCIDevExt *pciDevExt;
   vmk_PCIDevice vmkDev;
   vmk_uint32 *vectors;
   vmk_uint16 *indexes;
   vmk_IntrCookie *intrArray;
   VMK_ReturnStatus status;
   int prev_vecs;
   int minNumVecs;
   vmk_uint32 irq;

   pciDevExt = container_of(dev, LinuxPCIDevExt, linuxDev);
   vmkDev = pciDevExt->vmkDev;

   prev_vecs = pciDevExt->numIntrVectors;
   vectors = vmk_HeapAlloc(VMK_MODULE_HEAP_ID,
                           (nvecs + prev_vecs) * sizeof(*vectors));
   if (unlikely(vectors == NULL)) {
      return VMK_NO_RESOURCES;
   }

   intrArray = vmk_HeapAlloc(VMK_MODULE_HEAP_ID,
                             (nvecs + prev_vecs) * sizeof(*intrArray));
   if (unlikely(intrArray == NULL)) {
      vmk_HeapFree(VMK_MODULE_HEAP_ID, vectors);
      return VMK_NO_RESOURCES;
   }

   indexes = vmk_HeapAlloc(VMK_MODULE_HEAP_ID, nvecs * sizeof(*indexes));
   if (unlikely(indexes == NULL)) {
      vmk_HeapFree(VMK_MODULE_HEAP_ID, intrArray);
      vmk_HeapFree(VMK_MODULE_HEAP_ID, vectors);
      return VMK_NO_RESOURCES;
   }

   for (i = 0; i < nvecs; i++) {
      indexes[i] = entries[i].entry;
   }

   minNumVecs = (bestEffortAllocation == VMK_FALSE) ? nvecs : 1;

   /*
    * Remove the previous interrupt (legacy or msi) before adding msix.
    */
   LinuxPCIIntrVectorFree(pciDevExt);

   status = vmk_PCIAllocIntrCookie(vmklinuxModID,
                                   pciDevExt->vmkDev,
                                   VMK_PCI_INTERRUPT_TYPE_MSIX,
                                   nvecs, minNumVecs, indexes,
                                   &intrArray[prev_vecs],
                                   nvecs_alloced);

   vmk_HeapFree(VMK_MODULE_HEAP_ID, indexes);

   if (unlikely(status != VMK_OK)) {
      VMKLNX_WARN("%s: Interrupt allocation failed with %s",
                  pci_name(dev), vmk_StatusToString(status));
      vmk_HeapFree(VMK_MODULE_HEAP_ID, intrArray);
      vmk_HeapFree(VMK_MODULE_HEAP_ID, vectors);

      /*
       * Restore the legacy default PCI interrupt vector.
       */
      LinuxPCILegacyIntrVectorSet(pciDevExt);
      return status;
   }

   for (i = 0; i < *nvecs_alloced; i++) {
      irq = LinuxIRQ_AllocIRQ(pciDevExt, intrArray[i + prev_vecs]);
      if (likely(irq && irq < NR_IRQS)) {
         vectors[i + prev_vecs] = irq;
      } else {
         *nvecs_alloced = i;

         /*
          * Free previously allocated IRQs.
          */
         for (i--; i >= 0; i--) {
            LinuxIRQ_FreeIRQ(pciDevExt, vectors[prev_vecs + i]);
         }

         status = vmk_PCIFreeIntrCookie(vmklinuxModID, pciDevExt->vmkDev);
         if (status == VMK_OK) {
            /*
             * Restore the legacy default PCI interrupt vector.
             */
            LinuxPCILegacyIntrVectorSet(pciDevExt);
         }

         vmk_HeapFree(VMK_MODULE_HEAP_ID, intrArray);
         vmk_HeapFree(VMK_MODULE_HEAP_ID, vectors);
         return VMK_NO_RESOURCES;
      }
   }

   /* Good to go, fill previously allocated vectors, update entries[],
    * and free pciDevExt->intrVectors, pciDevExt->intrArray
    * if necessary (prev_vecs > 0) */
   for (i = 0; i < *nvecs_alloced; i++) {
      entries[i].vector = vectors[i + prev_vecs];
   }

   if (prev_vecs > 0) {
      for (i = 0; i  < prev_vecs; i++) {
         vectors[i] = pciDevExt->intrVectors[i];
         intrArray[i] = pciDevExt->intrArray[i];
      }
      vmk_HeapFree(VMK_MODULE_HEAP_ID, pciDevExt->intrVectors);
      vmk_HeapFree(VMK_MODULE_HEAP_ID, pciDevExt->intrArray);
   }

   pciDevExt->intrArray = intrArray;
   pciDevExt->intrVectors = vectors;
   pciDevExt->numIntrVectors += *nvecs_alloced;
   dev->msix_enabled = 1;

   return VMK_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 *  LinuxPCI_DisableMSIX --
 *
 *    Disable MSI-X on the specified device. Allocated interrupts
 *    are released.
 *
 *  Results:
 *    None.
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

void
LinuxPCI_DisableMSIX(struct pci_dev* dev)
{
   LinuxPCIDevExt *pciDevExt;
   VMK_ReturnStatus status;
   int i;

   if (!dev || !dev->msix_enabled)
      return;

   pciDevExt = container_of(dev, LinuxPCIDevExt, linuxDev);

   for (i = 0; i < pciDevExt->numIntrVectors; i++) {
      LinuxIRQ_FreeIRQ(pciDevExt, pciDevExt->intrVectors[i]);
   }

   status = vmk_PCIFreeIntrCookie(vmklinuxModID,
                                  pciDevExt->vmkDev);
   VMK_ASSERT(status == VMK_OK);

   vmk_HeapFree(VMK_MODULE_HEAP_ID, pciDevExt->intrVectors);
   vmk_HeapFree(VMK_MODULE_HEAP_ID, pciDevExt->intrArray);
   pciDevExt->intrVectors = NULL;
   pciDevExt->intrArray = NULL;
   pciDevExt->numIntrVectors = 0;
   dev->msix_enabled = 0;

   /*
    * Restore the legacy default PCI interrupt vector
    */
   LinuxPCILegacyIntrVectorSet(pciDevExt);
}


/*
 *----------------------------------------------------------------------------
 *
 *  LinuxPCI_FindDevByPortBase --
 *
 *    Get struct pci_dev from io_port or base address
 *
 *  Results:
 *    Pointer to struct pci_dev. NULL if no device matches the keys.
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

struct pci_dev *
LinuxPCI_FindDevByPortBase(resource_size_t base, unsigned long io_port)
{
   struct pci_dev *linuxDev;
   int i;

   linuxDev = NULL;
   for_each_pci_dev(linuxDev) {
      for (i = 0; i < DEVICE_COUNT_RESOURCE; i++) {
         resource_size_t start = pci_resource_start(linuxDev, i);
         unsigned long flags = pci_resource_flags(linuxDev, i);
         if ((start != 0) &&
             (((flags == PCI_BASE_ADDRESS_SPACE_MEMORY) && (start == base)) ||
              ((flags == PCI_BASE_ADDRESS_SPACE_IO) && (start == io_port)))) {
            return linuxDev;
         }
      }
   }

   return NULL;
}



/*
 *----------------------------------------------------------------------------
 *
 * LinuxPCI_Shutdown --
 *
 *      Call shutdown methods of PCI network device drivers
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Shuts down devices for poweroff.  Some net devices need this to
 *      arm them for wake-on-LAN. RAID controllers need this to flush
 *      their on-board cache.
 *
 *----------------------------------------------------------------------------
 */

void
LinuxPCI_Shutdown(void)
{
   struct pci_dev *linuxDev;
   vmk_ModuleID moduleID;

   linuxDev = NULL;
   for_each_pci_dev(linuxDev) {
      LinuxPCIDevExt *pciDevExt = container_of(linuxDev, LinuxPCIDevExt, linuxDev);

      /*
       * Skip the device if it is not already claimed yet.
       */
      if (pciDevExt->moduleID == VMK_INVALID_MODULE_ID)
         continue;

      VMK_ASSERT(linuxDev->driver != NULL);

      VMKLNX_DEBUG(1, "%04x.%02x.%02x:%x %s: netdev %p, shutdown %p",
                   pci_domain_nr(linuxDev->bus),
                   linuxDev->bus->number,
                   PCI_SLOT(linuxDev->devfn),
                   PCI_FUNC(linuxDev->devfn),
                   linuxDev->driver ? linuxDev->driver->name : "<no name>",
                   linuxDev->netdev,
                   linuxDev->driver ? linuxDev->driver->shutdown : NULL);

      if (linuxDev->driver != NULL && linuxDev->driver->shutdown != NULL) {
         char *driver_name = linuxDev->driver->name;

         moduleID = linuxDev->driver->driver.owner->moduleID;
         VMK_ASSERT(pciDevExt->moduleID == moduleID);

         VMKLNX_INFO("Calling %s (ModuleID=%d) driver shutdown",
                     driver_name, moduleID);

         VMKAPI_MODULE_CALL_VOID(moduleID, linuxDev->driver->shutdown, 
                                 linuxDev);
      }
   }

   down_read(&pci_bus_sem);
   LinuxPCIFreeBusses();

   up_read(&pci_bus_sem);
}

/**                                          
 *  pci_dev_get - get a reference to a PCI device
 *  @dev: The PCI device of interest
 *                                           
 *  Get a reference to the device @dev. 
 * 
 *  RETURN VALUE:
 *  Pointer to pci device structure
 */                                          
/* _VMKLNX_CODECHECK_: pci_dev_get */
struct pci_dev *pci_dev_get(struct pci_dev *dev)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if (dev) {
      get_device(&dev->dev);
   }
   return dev;
}
EXPORT_SYMBOL(pci_dev_get);

/**                                          
 *  pci_dev_put - release a reference to a PCI device
 *  @dev: The PCI device of interest
 *                                           
 *  Release a reference to the device @dev. If there is
 *  no more reference to the device, @dev will be
 *  deleted from the system.
 * 
 *  RETURN VALUE:
 *  This function does not return a value
 */                                          
/* _VMKLNX_CODECHECK_: pci_dev_put */
void 
pci_dev_put(struct pci_dev *dev)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if (dev) {
      put_device(&dev->dev);
   }
}
EXPORT_SYMBOL(pci_dev_put);



/*
 *----------------------------------------------------------------------------
 *
 *  LinuxPCI_IsValidPCIBusDev --
 *
 *    Check whether the device is valid PCI bus device 
 *
 *  Results:
 *    True if it is valid PCI bus device
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

vmk_Bool
LinuxPCI_IsValidPCIBusDev(struct pci_dev *pdev)
{
   if (pdev && pdev->bus) {
      if ((PCI_SLOT(pdev->devfn) >= 0 
           && PCI_SLOT(pdev->devfn) < VMK_PCI_NUM_SLOTS) 
           && (PCI_FUNC(pdev->devfn) >= 0 
           && PCI_FUNC(pdev->devfn) <= VMK_PCI_NUM_FUNCS)) {
         return VMK_TRUE;
      }
   }
   return VMK_FALSE;
}

/*
 *----------------------------------------------------------------------------
 *
 *  LinuxPCIGetBus --
 *
 *    Find the bus structure associated with (domain, bus).
 *
 *  Results:
 *    Pointer to the appropriate pci_bus struct, or NULL if allocation fails.
 *
 *  Side effects:
 *    Allocates a PCI bus structure if we don't already have one for
 *    the specified (domain, bus).
 *
 *----------------------------------------------------------------------------
 */

static struct pci_bus *
LinuxPCIGetBus(uint16_t domain, uint8_t bus)
{
   struct vmklnx_hashtab_item *item;
   struct PCI_busdata *busdata;
   int key = PCI_HASH_KEY(domain, bus);

   down_write(&pci_bus_sem);
   if (vmklnx_hashtab_find_item(&linuxPCIBuses, key, &item) < 0) {
      // not found, create it
      busdata = (struct PCI_busdata *) kzalloc(sizeof(struct PCI_busdata), GFP_KERNEL);
      if (unlikely(busdata == NULL)) {
         up_write(&pci_bus_sem);
         return NULL;
      }
      item = &busdata->hash_links;
      item->key = key;
      if (unlikely(vmklnx_hashtab_insert_item(&linuxPCIBuses, item) < 0)) {
         vmk_Panic("LinuxPCIGetBus: adding duplicate item.");
      }
      list_add(&busdata->links, &linuxPCIBusList);
      ++linuxPCIBusCount;
      INIT_LIST_HEAD(&busdata->linuxPCIBus.devices);
      busdata->linuxPCIBus.sysdata = &busdata->sysdata;
      busdata->sysdata.domain = domain;
      busdata->linuxPCIBus.number = bus;
   } else {
      busdata = container_of(item, struct PCI_busdata, hash_links);
   }
   up_write(&pci_bus_sem);
   return &busdata->linuxPCIBus;
}

/*
 *----------------------------------------------------------------------------
 *
 *  LinuxPCIFreeBusses --
 *
 *    Free all allocated bus structures plus the hash table.
 *
 *  Side effects:
 *    Free all PCI bus associated memory.
 *
 *----------------------------------------------------------------------------
 */

static void
LinuxPCIFreeBusses(void)
{
   struct vmklnx_hashtab_item *item;
   struct PCI_busdata *busdata;
   struct list_head *del;
   int key;

   while (!list_empty(&linuxPCIBusList)) {
      VMK_ASSERT(linuxPCIBusCount > 0);
      del = linuxPCIBusList.next;
      list_del(del);
      busdata = container_of(del, struct PCI_busdata, links);
      key = PCI_HASH_KEY(busdata->sysdata.domain, busdata->linuxPCIBus.number);
      if (unlikely(vmklnx_hashtab_remove_key(&linuxPCIBuses, key, &item) < 0)) {
         vmk_Panic("LinuxPCIFreeBusses: hash removal failed.");
      }
      VMK_ASSERT(item == &busdata->hash_links);
      --linuxPCIBusCount;
   }
   VMK_ASSERT(linuxPCIBusCount == 0);
   vmklnx_hashtab_remove(&linuxPCIBuses);
}

/*
 *----------------------------------------------------------------------------
 *
 *  LinuxPCIConfigSpaceRead/Write --
 *
 *    Read/Write PCI config space
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

int 
LinuxPCIConfigSpaceRead(struct pci_dev *dev, int size, int where, void *buf)
{
   VMK_ReturnStatus status = VMK_FAILURE;
   LinuxPCIDevExt *pciDevExt = container_of(dev, LinuxPCIDevExt, linuxDev);
   vmk_PCIDevice vmkDev = pciDevExt->vmkDev;
   vmk_ModuleID moduleID = vmklinuxModID;
   vmk_uint32 value;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   switch (size) {
      case  8:
         status = vmk_PCIReadConfig(moduleID, vmkDev, VMK_PCI_CONFIG_ACCESS_8, 
                                    (vmk_uint16) where, &value);
         *(vmk_uint8 *)buf = (vmk_uint8)value;
         break;
      case 16:
         status = vmk_PCIReadConfig(moduleID, vmkDev, VMK_PCI_CONFIG_ACCESS_16,
                                    (vmk_uint16) where, &value);
         *(vmk_uint16 *)buf = (vmk_uint16)value;
         break;
      case 32:
         status = vmk_PCIReadConfig(moduleID, vmkDev, VMK_PCI_CONFIG_ACCESS_32,
                                    (vmk_uint16) where, &value);
         *(vmk_uint32 *)buf = value;
         break;
   }

   return status == VMK_OK ? 0 : -EINVAL;
}
EXPORT_SYMBOL(LinuxPCIConfigSpaceRead);

int 
LinuxPCIConfigSpaceWrite(struct pci_dev *dev, int size, int where, vmk_uint32 data)
{
   VMK_ReturnStatus status = VMK_FAILURE;
   LinuxPCIDevExt *pciDevExt = container_of(dev, LinuxPCIDevExt, linuxDev);
   vmk_PCIDevice vmkDev = pciDevExt->vmkDev;
   vmk_ModuleID moduleID = vmklinuxModID;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   switch (size) {
      case  8:
         status = vmk_PCIWriteConfig(moduleID, vmkDev, VMK_PCI_CONFIG_ACCESS_8,
                                     (vmk_uint16) where, data);
         break;
      case 16:
         status = vmk_PCIWriteConfig(moduleID, vmkDev, VMK_PCI_CONFIG_ACCESS_16,
                                     (vmk_uint16) where, data);
         break;
      case 32:
         status = vmk_PCIWriteConfig(moduleID, vmkDev, VMK_PCI_CONFIG_ACCESS_32,
                                     (vmk_uint16) where, data);
         break;
   }

   return status == VMK_OK ? 0 : -EINVAL;
}
EXPORT_SYMBOL(LinuxPCIConfigSpaceWrite);


/**
 *  pci_domain_nr - get PCI domain for the specified bus
 *  @bus: Pointer to the pci_dev structure
 *
 *  This function returns the PCI domain (also called the PCI segment
 *  in ACPI documents) associated with the specified bus.
 *
 *  ESX Deviation Notes:
 *  This function is a mere stubs that returns 0 for drivers that do
 *  not support domains.  Drivers that do support domains must define
 *  preprocessor symbol _USE_CONFIG_PCI_DOMAINS in their build rules.
 *  ESX does not use symbol CONFIG_PCI_DOMAINS (from autoconf.h) to
 *  configure domain support.
 *
 *  RETURN VALUE:
 *  PCI domain associated with the specified bus.
 *
 */
/* _VMKLNX_CODECHECK_: pci_domain_nr */
int
pci_domain_nr(struct pci_bus *bus)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   return ((struct PCI_sysdata *)(bus->sysdata))->domain;
}
EXPORT_SYMBOL(pci_domain_nr);

/**
 *  vmklnx_enable_vfs - enables SR-IOV on the specified device
 *  @pf: Pointer to the PF's pci_dev structure
 *  @num_vfs: Number of Virtual Functions requested
 *  @cb: Callback function to be called for each VF after activation
 *  @data: Opaque data to be passed to the callback
 *
 *  This function enables SR-IOV on a specified device and activates a
 *  given number of Virtual Functions.
 *
 *  For convenience, a user-specified callback can be invoked for each
 *  VF to indicate it's identifier and PCI address & information.
 *
 *  RETURN VALUE:
 *  The number of virtual functions successfully allocated (can be 0 if none).
 *
 */
/* _VMKLNX_CODECHECK_: vmklnx_enable_vfs */
int
vmklnx_enable_vfs(struct pci_dev *pf, int num_vfs, vmklnx_vf_callback cb,
                  void *data)
{
   LinuxPCIDevExt *pciDevExt = container_of(pf, LinuxPCIDevExt, linuxDev);
   vmk_uint16 numvfs = num_vfs;
   int i;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMKLNX_INFO("enabling %d VFs on PCI device " PCI_DEVICE_BUS_ADDRESS,
               num_vfs, PCI_DEVICE_PRINT_ADDRESS(pf));

   if (vmk_PCIEnableVFs(pciDevExt->vmkDev, &numvfs) != VMK_OK) {
      VMKLNX_WARN("unable to enable SR-IOV on PCI device " 
                  PCI_DEVICE_BUS_ADDRESS, PCI_DEVICE_PRINT_ADDRESS(pf));
      return 0;
   }

   for (i = 0; cb != NULL && i < numvfs; i++) {
      struct pci_dev *vf = vmklnx_get_vf(pf, i, NULL);
      cb(pf, vf, i, data);
   }

   VMKLNX_INFO("%d VFs enabled on PCI device " PCI_DEVICE_BUS_ADDRESS,
               numvfs, PCI_DEVICE_PRINT_ADDRESS(pf));

   return numvfs;
}
EXPORT_SYMBOL(vmklnx_enable_vfs);

/**
 *  vmklnx_disable_vfs - disables SR-IOV on the specified device
 *  @pf: Pointer to the PF's pci_dev structure
 *  @num_vfs: Number of Virtual Functions activated
 *  @cb: Callback function to be called for each VF before deactivation
 *  @data: Opaque data to be passed to the callback
 *
 *  This function disables SR-IOV on a specified device. All VFs are
 *  disabled regardless of num_vfs.
 *
 *  For convenience, a user-specified callback can be invoked for each
 *  VF before it is deactivated.
 *
 *  RETURN VALUE:
 *  None.
 *
 */
/* _VMKLNX_CODECHECK_: vmklnx_disable_vfs */
void
vmklnx_disable_vfs(struct pci_dev *pf, int num_vfs, vmklnx_vf_callback cb,
                   void *data)
{
   LinuxPCIDevExt *pciDevExt = container_of(pf, LinuxPCIDevExt, linuxDev);
   int i;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   VMKLNX_INFO("disabling VFs on PCI device " PCI_DEVICE_BUS_ADDRESS,
               PCI_DEVICE_PRINT_ADDRESS(pf));

   for (i = 0; cb != NULL && i < num_vfs; i++) {
      struct pci_dev *vf = vmklnx_get_vf(pf, i, NULL);
      cb(pf, vf, i, data);
   }

   if (vmk_PCIDisableVFs(pciDevExt->vmkDev) != VMK_OK) {
      VMKLNX_WARN("unable to disable SR-IOV on PCI device " 
                  PCI_DEVICE_BUS_ADDRESS, PCI_DEVICE_PRINT_ADDRESS(pf));
   }

}
EXPORT_SYMBOL(vmklnx_disable_vfs);

/**
 *  vmklnx_get_vf - retrieves a Virtual Function
 *  @pf: Pointer to the PF's pci_dev structure
 *  @vf_idx: index of the VF
 *  @sbdf: an optional integer to receives the SBDF address of the VF
 *
 *  This function retrieves the pci_dev associated to one Virtual Function.
 *
 *  This function is likely to be called in the context of a PF's
 *  vf_get_info handler and thus can return the integer SBDF address
 *  of the VF (needed in vf_get_info).
 *
 *  XXX As of now, pci_dev objects are not created for VFs. So this function
 *  is going to return NULL always. It is a placeholder for when VFs will
 *  have pci_dev objects.
 *
 *
 *  RETURN VALUE:
 *  The pci_dev of the corresponding VF.
 *
 */
/* _VMKLNX_CODECHECK_: vmklnx_get_vf */
struct pci_dev *vmklnx_get_vf(struct pci_dev *pf, int vf_idx, vmk_uint32 *sbdf)
{
   LinuxPCIDevExt *pciDevExt = container_of(pf, LinuxPCIDevExt, linuxDev);
   vmk_PCIDevice vfDev;
   u16 seg;
   u8 bus, slot, func;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if (vmk_PCIGetVFPCIDevice(pciDevExt->vmkDev, vf_idx, &vfDev) != VMK_OK) {
      VMKLNX_WARN("unable to get VF %d on PCI device " PCI_DEVICE_BUS_ADDRESS,
                  vf_idx, PCI_DEVICE_PRINT_ADDRESS(pf));
      return NULL;
   }


   if (vmk_PCIGetSegmentBusDevFunc(vfDev, &seg, &bus, &slot, &func) != VMK_OK) {
      VMKLNX_WARN("unable to get VF %d SBDF on PCI device "
                  PCI_DEVICE_BUS_ADDRESS,
                  vf_idx, PCI_DEVICE_PRINT_ADDRESS(pf));
      return NULL;
   }

   if (sbdf != NULL) {
      *sbdf = PCI_MAKE_SBDF(seg, bus, slot, func);
   }

   return pci_find_slot(seg, bus, PCI_DEVFN(slot, func));
}
EXPORT_SYMBOL(vmklnx_get_vf);


/**
 *  vmklnx_configure_net_vf - Configure networking VF from PF driver
 *
 *  This function routes the per VF configuration from PF driver to VMKernel.
 *  VMKernel will in turn verify the configuration with portgroup settings
 *  and push the configuration to VF through passthru ops
 *
 *  @pf_dev: The PCI device for physical function (pnic).
 *  @cfg_data: VF configuration data.
 *  @vf_idx: index of the VF to be configured.
 *
 *  RETURN VALUE:
 *  VMK_OK on success, failure code on error.
 */
/* _VMKLNX_CODECHECK_: vmklnx_configure_net_vf */
int
vmklnx_configure_net_vf(struct pci_dev *pf_dev, void *cfg_data, int vf_idx)
{
   LinuxPCIDevExt *pciDevExt = container_of(pf_dev, LinuxPCIDevExt, linuxDev);
   vmk_PCIDevice vfDev;
   vmk_PCIDeviceAddr vf_sbdf;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   if (vmk_PCIGetVFPCIDevice(pciDevExt->vmkDev, vf_idx, &vfDev) != VMK_OK) {
      VMKLNX_WARN("unable to get VF %d on PCI device " PCI_DEVICE_BUS_ADDRESS,
                  vf_idx, PCI_DEVICE_PRINT_ADDRESS(pf_dev));
      return VMK_NOT_FOUND;
   }

   if(vmk_PCIQueryDeviceAddr(vfDev, &vf_sbdf) != VMK_OK) {
      VMKLNX_WARN("unable to get PCI address for the VFa %d on PCI device "
                  PCI_DEVICE_BUS_ADDRESS, vf_idx, PCI_DEVICE_PRINT_ADDRESS(pf_dev));
      return VMK_NOT_FOUND;
   } else {
      VMKLNX_DEBUG(1, "PCI address of VF %d is " PCI_DEVICE_BUS_ADDRESS, vf_idx,
                   vf_sbdf.seg, vf_sbdf.bus, vf_sbdf.dev, vf_sbdf.fn);
   }
   return (vmk_NetPTConfigureVF((vmk_NetVFCfgInfo *)cfg_data, vf_sbdf));
}
EXPORT_SYMBOL(vmklnx_configure_net_vf);


/*
 *  setup_dma_engine_from_mask - set up a dma engine in a pci device
 *
 *  This function sets up the a dma engine for given mask.  This function
 *  is used to set both the primary and secondary dma engines, depending
 *  upon the arguments.
 *
 *  codma - The vmklnx_codma structure for this module.
 *  dev - the pci device structure
 *  flags - the flags to create the engine with.
 *  mask - new dma mask to be set.
 *  destMask - Bidirectional parameter (pointer to the dma mask).
 *             On entry, *destMask contains the existing dma mask,
 *             possibly VMK_DMA_ENGINE_INVALID if not yet set.
 *             On successful exit, *destMask will be set to mask.
 *  engine - Bidirectional parameter (pointer to the vmk_DMAEngine).
 *           On entry, *engine is the existing engine, possibly
 *           VMK_DMA_ENGINE_INVALID if not yet set.  This engine
 *           may be destroyed if the mask is actually changing.
 *           On successful exit, *engine will be the new engine.
 *
 *  RETURN VALUE:
 *  0 if successfull or error otherwise.
 */
static int
setup_dma_engine_from_mask(struct vmklnx_codma *codma,
                           struct pci_dev *dev,
                           vmk_uint32 flags,
                           u64 mask,
                           u64 *destMask,
                           vmk_DMAEngine *engine)
{
   int err = 0;
   VMK_ReturnStatus status;
   vmk_DMAEngine newEngine, oldEngine;
   vmk_DMAConstraints constraints;
   vmk_DMAEngineProps props;
   vmk_DMABouncePoolProps bouncer;
   vmk_PCIDevice vmkPciDev;
   vmk_HeapID heapID;
   vmk_MemPhysAddrConstraint memType;
   LinuxPCIDevExt *pciDevExt;
   vmk_ModuleID owner;
   vmk_Bool priIdMapped, secIdMapped;

   if (!vmklnx_pci_dma_supported(codma, dev, mask)) {
      err = -EIO;
      goto out;
   }

   /*
    * Detect a pseudo PCI device OR a case where the mask is not changing.
    *
    * In either case, skip DMA engine creation.
    */
   pciDevExt = container_of(dev, LinuxPCIDevExt, linuxDev);
   if ((dev->bus == NULL) || (pciDevExt->moduleID == 0) || (pciDevExt->magic == 0) ||
       ((vmk_DMAEngine) *engine != VMK_DMA_ENGINE_INVALID && mask == *destMask)) {
      /* Setup a heap, if necessary */
      if (mask == VMK_ADDRESS_MASK_64BIT) {
         memType = VMK_PHYS_ADDR_ANY;
      } else {
         if (is_power_of_2(mask+1)) {
            if (mask >= VMK_ADDRESS_MASK_39BIT) {
               memType = VMK_PHYS_ADDR_BELOW_512GB;
            } else if (mask >= VMK_ADDRESS_MASK_32BIT) {
               memType = VMK_PHYS_ADDR_BELOW_4GB;
            } else if (mask >= VMK_ADDRESS_MASK_31BIT) {
               memType = VMK_PHYS_ADDR_BELOW_2GB;
            } else {
               VMKLNX_WARN("Unsupported DMA mask 0x%lx on device '%s'",
                           (unsigned long)mask, dev_name(&dev->dev));
               err = -EIO;
               goto out;
            }
         } else {
            VMKLNX_WARN("Unsupported DMA mask 0x%lx on device '%s'",
                           (unsigned long)mask, dev_name(&dev->dev));
            err = -EIO;
            goto out;
         }
      }
      
      *destMask = mask;
      goto setupHeap;
   } else {
      /*
       * Use of a pci pseudo-device under vmklinux_9 is illegal.
       * If you see this PSOD in your driver during certification,
       * then you must remove the pseudo device.
       *
       * alloc_pci_dev() has been removed from vmklinux. This is
       * a documented deviation from Linux.
       */
      BUG_ON(pciDevExt->magic != VMKLNX_PCIDEV_MAGIC);
      *destMask = mask;
   }

   /*
    * vmklinux itself "owns" the PCI devices and is responsible for
    * cleaning them up, so it should own the allocations for the
    * DMA engines. Otherwise we can get into a situation where the
    * DMA engines are allocated off the driver module's heap and but
    * because it doesn't do the final "put" on the PCI device, we
    * PSOD on module unload because the DMA engines are still hanging
    * around in the driver's module heap.
    */
   owner = THIS_MODULE->moduleID;

   /*
    * Setup a basic DMA engine for this PCI device
    */
   bouncer.module = owner;
   bouncer.type = VMK_DMA_BOUNCE_POOL_TYPE_NONE;

   memset(&constraints, 0, sizeof(constraints));
   constraints.addressMask = mask;

   status = vmk_NameFormat(&props.name, "vmklnxpci-%u:%u:%u.%u",
                           pci_domain_nr(dev->bus), dev->bus->number,
                           PCI_SLOT(dev->devfn), PCI_FUNC(dev->devfn));
   VMK_ASSERT(status == VMK_OK);

   props.module = owner;

   status = vmk_PCIGetPCIDevice(pci_domain_nr(dev->bus), dev->bus->number,
                                PCI_SLOT(dev->devfn), PCI_FUNC(dev->devfn),
                                &vmkPciDev);
   if (status != VMK_OK) {
      VMKLNX_INFO("Couldn't find vmk_PCI device for device '%s': %s",
                  dev_name(&dev->dev), vmk_StatusToString(status));
      VMK_ASSERT(status == VMK_OK);
      err = -EIO;
      goto out;
   }
   
   status = vmk_PCIGetGenDevice(vmkPciDev, &props.device);
   if (status != VMK_OK) {
      VMKLNX_INFO("Couldn't find generic vmk device for device '%s': %s",
                  dev_name(&dev->dev), vmk_StatusToString(status));
      VMK_ASSERT(status == VMK_OK);
      err = -EIO;
      goto out;
   }

   props.constraints = &constraints;
   props.bounce = &bouncer;
   props.flags = flags;

   status = vmk_DMAEngineCreate(&props, &newEngine);
   if (status != VMK_OK) {
      VMKLNX_INFO("Couldn't create DMA engine with mask 0x%lx on device '%s':"
                 "%s", (unsigned long)mask, dev_name(&dev->dev),
                 vmk_StatusToString(status));
      VMK_ASSERT(status == VMK_OK);
      err = -EIO;
      goto out;
   }

   oldEngine = (vmk_DMAEngine)*engine;
   if (oldEngine != VMK_DMA_ENGINE_INVALID) {
      /* Destroy the old engine */
      status = vmk_DMAEngineDestroy(oldEngine);
      if (status != VMK_OK) {
         VMK_ASSERT(status == VMK_OK);
         /* XXX Nothing to do but leak the engine and log a notice */
         VMKLNX_INFO("Couldn't destroy DMA engine on device '%s': %s",
                     dev_name(&dev->dev), vmk_StatusToString(status));
      }
   }

   *engine = newEngine;

   /*
    * Setup a special heap, if necessary.
    */
   status = vmk_DMAGetAllocAddrConstraint(newEngine, &memType); 
   if (status != VMK_OK) {
      VMKLNX_WARN("Couldn't get alloc constraint on device '%s'",
                  dev_name(&dev->dev));
      VMK_ASSERT(status == VMK_OK);
      err = -EIO;
   }

setupHeap:
   down(codma->mutex);

   if (memType == VMK_PHYS_ADDR_BELOW_2GB) {
      /* Driver needs a special low memory heap */
      if (codma->mask == DMA_32BIT_MASK) {
         vmk_HeapCreateProps heapProps;

         heapProps.type = VMK_HEAP_TYPE_CUSTOM;
         status = vmk_NameInitialize(&heapProps.name, codma->heapName);
         VMK_ASSERT(status == VMK_OK);
         heapProps.module = vmk_ModuleStackTop();
         heapProps.initial = codma->heapSize;
         heapProps.max = codma->heapSize;
         heapProps.creationTimeoutMS = VMK_TIMEOUT_NONBLOCKING;
         heapProps.typeSpecific.custom.physContiguity =
            VMK_MEM_PHYS_CONTIGUOUS;
         heapProps.typeSpecific.custom.physRange = memType;

         status = vmk_HeapCreate(&heapProps, &heapID);
         if (status != VMK_OK) {
            VMKLNX_WARN("No suitable memory available for heap %s",
                        codma->heapName);
            err = -EIO;
            goto outWithUp;
         }
         codma->heapID = heapID;
         codma->mask = mask;
      } else if (codma->mask > mask) {
         VMKLNX_WARN("Conflicting dma requirements for heap %s",
                     codma->heapName);
         err = -EIO;
         goto outWithUp;
      }
   }

   dev->dev.dma_mem = (struct dma_coherent_mem *) codma->heapID;

outWithUp:
   up(codma->mutex);

out:

   /*
    * We may have just reassigned an engine.  Re-evaluate if both
    * are identity and mark the device accordingly.
    *
    * As of now, it is not possible for one to be identity and the
    * other to not be identity.  But it is possible for one to be
    * valid and the other not to be, and that must be handled.
    */
   if (dev->dev.dma_engine_primary != VMK_DMA_ENGINE_INVALID) {
      status = vmk_DMAEngineIsIdentityMapped(dev->dev.dma_engine_primary, &priIdMapped);
      VMK_ASSERT(status == VMK_OK);

      if (dev->dev.dma_engine_secondary != VMK_DMA_ENGINE_INVALID) {
         /* Both engines are valid. */
         status = vmk_DMAEngineIsIdentityMapped(dev->dev.dma_engine_secondary, &secIdMapped);
         VMK_ASSERT(status == VMK_OK);
      } else {
         /* Only primary engine is valid. */
         secIdMapped = priIdMapped;
      }
   } else if (dev->dev.dma_engine_secondary != VMK_DMA_ENGINE_INVALID) {
      /* Only secondary engine is valid. */
      status = vmk_DMAEngineIsIdentityMapped(dev->dev.dma_engine_secondary, &secIdMapped);
      VMK_ASSERT(status == VMK_OK);
      priIdMapped = secIdMapped;
   } else {
      /* Neither engine is valid. */
      priIdMapped = secIdMapped = VMK_TRUE;
   }

   BUG_ON(priIdMapped != secIdMapped);
   if (priIdMapped && secIdMapped) {
      dev->dev.dma_identity_mapped = 1;
   }
   else {
      dev->dev.dma_identity_mapped = 0;
   }

   return err;
}

int
vmklnx_pci_set_dma_mask(struct vmklnx_codma *codma, struct pci_dev *dev,
                        u64 mask)
{
   int ret;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   ret = setup_dma_engine_from_mask(codma, dev, VMK_DMA_ENGINE_FLAGS_NONE,
                                    mask, &dev->dma_mask,
                                    (vmk_DMAEngine *)&dev->
                                    dev.dma_engine_primary);
   if (ret) {
      return ret;
   }

   /*
    * Create the secondary engine for coherent mappings.  Apparently some
    * drivers may attempt coherent mappings without first calling
    * pci_set_consistent_dma_mask().  If they do call that function after this
    * one, setup_dma_engine_from_mask() is smart enough to not recreate the
    * engine unless the mask changes.
    */
   if (dev->dev.dma_engine_secondary == (void *)VMK_DMA_ENGINE_INVALID) {
      /* coherent_dma_mask is set to a default value during initialization. */
      ret = setup_dma_engine_from_mask(codma, dev, VMK_DMA_ENGINE_FLAGS_COHERENT,
                                       dev->dev.coherent_dma_mask,
                                       &(dev->dev.coherent_dma_mask),
                                       (vmk_DMAEngine *)&dev->
                                       dev.dma_engine_secondary);
   }

   return ret;
}
EXPORT_SYMBOL(vmklnx_pci_set_dma_mask);

int
vmklnx_pci_set_consistent_dma_mask(struct vmklnx_codma *codma,
                                   struct pci_dev *dev, u64 mask)
{
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   return setup_dma_engine_from_mask(codma, dev, VMK_DMA_ENGINE_FLAGS_COHERENT,
                                     mask, &(dev->dev.coherent_dma_mask),
                                     (vmk_DMAEngine *)&dev->
                                     dev.dma_engine_secondary);
}
EXPORT_SYMBOL(vmklnx_pci_set_consistent_dma_mask);
