/* ****************************************************************
 * Portions Copyright 2010, 2012 VMware, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * ****************************************************************/

/*
 * linux_pci.h --
 *
 *      Linux PCI compatibility.
 */

#ifndef _LINUX_PCI_H_
#define _LINUX_PCI_H_

#include "vmkapi.h"

#define PCI_DEVICE_BUS_ADDRESS      "%04x:%02x:%02x.%01d" /* dom,bus,slot,func*/
#define PCI_DEVICE_VENDOR_SIGNATURE "%04x:%04x %04x:%04x" /* ven,dev,subV,subD*/

#define PCI_DEVICE_PRINT_ADDRESS(d) \
   pci_domain_nr((d)->bus), (d)->bus->number, PCI_SLOT((d)->devfn), PCI_FUNC((d)->devfn)

#define PCI_MAKE_SBDF(s, b, d, f) ((((s) & 0xffff) << 16) |             \
                                   (((b) & 0xff) << 8) |                \
                                   (((d) & 0x1f) << 3) |                \
                                   ((f) & 0x07))

typedef struct LinuxPCIDev {
   struct pci_dev linuxDev;
   vmk_PCIDevice  vmkDev;
   vmk_Device     gDev;
   vmk_uint32     *intrVectors;
   vmk_IntrCookie *intrArray;
   vmk_uint32     numIntrVectors;
   vmk_ModuleID   moduleID;
   vmk_uint64     flags;
   vmk_uint64     magic;
} LinuxPCIDevExt;

#define VMKLNX_PCIDEV_MAGIC     0x2c20af9a500abb51

int LinuxPCIConfigSpaceRead(struct pci_dev *dev, int size, int where, void *buf);
int LinuxPCIConfigSpaceWrite(struct pci_dev *dev, int size, int where, vmk_uint32 data);

extern void pci_fill_rom_bar(struct pci_dev *dev, int rom);
extern unsigned int pci_calc_resource_flags(unsigned int flags);

extern void LinuxPCI_Init(void);
extern void LinuxPCI_Shutdown(void);
extern void LinuxPCI_Cleanup(void);
extern void LinuxDMA_Init(void);
extern void LinuxDMA_Cleanup(void);
extern vmk_Bool LinuxPCI_DeviceIsPAECapable(struct pci_dev *dev);
extern void LinuxPCI_DeviceClaimed(LinuxPCIDevExt *pciDevExt,
                                   vmk_ModuleID moduleID);
extern void LinuxPCI_DeviceUnclaimed(LinuxPCIDevExt *pciDevExt);
int LinuxPCI_FindCapability(uint16_t domain, uint16_t bus, uint16_t devfn, uint8_t cap);

struct pci_dev *LinuxPCI_FindDevByPortBase(resource_size_t base, unsigned long io_port);
struct pci_dev *LinuxPCI_FindDevByBusSlot(uint16_t domain, uint16_t bus, uint16_t devfn);
vmk_Bool LinuxPCI_IsValidPCIBusDev(struct pci_dev *pdev);

#endif
