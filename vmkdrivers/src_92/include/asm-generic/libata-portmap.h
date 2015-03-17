/*
 * Portions Copyright 2008 VMware, Inc.
 */
#ifndef __ASM_GENERIC_LIBATA_PORTMAP_H
#define __ASM_GENERIC_LIBATA_PORTMAP_H

#if defined(__VMKLNX__)
#define ATA_PRIMARY_IRQ(dev)	vmklnx_convert_isa_irq(14)
#else
#define ATA_PRIMARY_IRQ(dev)	14
#endif /* defined(__VMKLNX__) */

#if defined(__VMKLNX__)
#define ATA_SECONDARY_IRQ(dev)	vmklnx_convert_isa_irq(15)
#else
#define ATA_SECONDARY_IRQ(dev)	15
#endif /* defined(__VMKLNX__) */

#endif
