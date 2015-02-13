/*
 * Portions Copyright 2010 VMware, Inc.
 */

#ifndef _LINUX_IRQ_H_
#define _LINUX_IRQ_H_

#include <linux/interrupt.h>
#include <linux/device.h>

void LinuxIRQ_Init(void);
void LinuxIRQ_Cleanup(void);
struct device * LinuxIRQ_GetDumpIRQDev(void);

extern void devm_irq_release(struct device *dev, void *res);
extern int devm_irq_match(struct device *dev, void *res, void *data);

extern void *irqdevres_register(struct device *dev, unsigned int irq,
                                irq_handler_t handler, void *dev_id);
extern void irqdevres_unregister(struct device *dev, unsigned int irq,
                                 void *dev_id);
extern void *irqdevres_find(struct device *dev, dr_match_t match,
                            unsigned int irq, void *dev_id);

extern void *irqdevres_get_handler(void *data);
extern void Linux_IRQHandler(void *clientData, uint32_t vector);

#endif /* _LINUX_IRQ_H_ */

