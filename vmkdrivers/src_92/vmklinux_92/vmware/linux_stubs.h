/* ****************************************************************
 * Portions Copyright 1998, 2009, 2010 VMware, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * ****************************************************************/

/*
 * linux_stubs.h --
 *
 *      Linux device driver compatibility.
 */

#ifndef _LINUX_STUBS_H_
#define _LINUX_STUBS_H_

#include "vmkapi.h"
#include "vmklinux_dist.h"
#include "genhd.h"
#include "input.h"
#include <vmkplexer_entropy.h>

#define VMKLINUX_DRIVERLOCK_RANK (VMK_SP_RANK_IRQ_LOWEST)
#define VMKLINUX_ABORTLOCK_RANK  (VMKLINUX_DRIVERLOCK_RANK+1)

#define VMKLINUX_NAME             VMKLNX_MODIFY_NAME(vmklinux)

#define LINUX_BHHANDLER_NO_IRQS	(void *)0xdeadbeef

extern vmk_Bool is_vmvisor;

extern vmk_MPN max_pfn;

extern vmk_ModuleID vmklinuxModID;
extern vmk_SpinlockIRQ kthread_wait_lock;
extern vmk_HeapID vmklnxLowHeap;
extern vmk_HeapID vmklnxEmergencyHeap;
  
#define MODULE_GET_FLAG 0
#define MODULE_PUT_FLAG 1

typedef struct module_ll {
   vmk_ListLinks moduleList;
   vmk_ModuleID moduleID;
   struct module *mod;
   int refCnt;
} module_ll;

extern vmk_LogComponent  vmklinuxLog;
extern vmk_Semaphore pci_bus_sem;
extern vmk_SpinlockIRQ irqMappingLock;

struct softirq_action;

extern void SCSILinux_Init(void);
extern void SCSILinux_Cleanup(void);
extern void LinuxCNA_Init(void);
extern void LinuxCNA_Cleanup(void);
extern void LinuxUSB_Init(void);
extern void LinuxUSB_Cleanup(void);
extern void BlockLinux_Init(void);
extern void BlockLinux_Cleanup(void);
extern void LinuxChar_Init(void);
extern void LinuxChar_Cleanup(void);
extern void LinuxProc_Init(void);
extern void LinuxProc_Cleanup(void);
extern struct proc_dir_entry* LinuxProc_AllocPDE(const char* name);
extern void LinuxProc_FreePDE(struct proc_dir_entry* pde);

extern void Linux_BHInternal(void (*routine)(void *), void *data,
                             vmk_ModuleID modID);
extern void Linux_BHHandler(void *clientData);
extern void Linux_BH(void (*routine)(void *), void *data); 
extern void Linux_PollBH(void *data);
extern void Linux_OpenSoftirq(int nr, void (*action)(struct softirq_action *), void *data);
extern void driver_init(void);
extern int input_init(void);
extern void input_exit(void);
extern int hid_init(void);
extern void hid_exit(void);
extern int lnx_kbd_init(void);
extern int mousedev_init(void);
extern void mousedev_exit(void);
vmk_ModuleID vmklnx_get_driver_module_id(const struct device_driver *drv);

unsigned int vmklnx_get_low_port2numbering(void);

#endif
