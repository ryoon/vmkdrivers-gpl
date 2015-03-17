/* ****************************************************************
 * Portions Copyright 2010, 2012 VMware, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * ****************************************************************/

/*
 * linux_irq.h --
 *
 *      Linux IRQ compatibility.
 */

#ifndef _LINUX_IRQ_H_
#define _LINUX_IRQ_H_

#include <linux/interrupt.h>
#include <linux/device.h>
#include "linux_pci.h"

void LinuxIRQ_Init(void);
void LinuxIRQ_Cleanup(void);
struct device * LinuxIRQ_GetDumpIRQDev(void);
uint32_t LinuxIRQ_AllocIRQ(LinuxPCIDevExt *pciDevExt, vmk_IntrCookie intrCookie);
vmk_IntrCookie LinuxIRQ_FreeIRQ(LinuxPCIDevExt *pciDevExt, uint32_t irq);
vmk_IntrCookie LinuxIRQ_VectorToCookie(uint32_t irq);

extern void devm_irq_release(struct device *dev, void *res);
extern int devm_irq_match(struct device *dev, void *res, void *data);

extern void *irqdevres_register(struct device *dev, unsigned int irq,
                                irq_handler_t handler, void *dev_id);
extern void irqdevres_unregister(struct device *dev, unsigned int irq,
                                 void *dev_id);
extern void *irqdevres_find(struct device *dev, dr_match_t match,
                            unsigned int irq, void *dev_id);

extern void *irqdevres_get_handler(void *data);
extern void Linux_IRQHandler(void *clientData, vmk_IntrCookie intrCookie);

#endif /* _LINUX_IRQ_H_ */

