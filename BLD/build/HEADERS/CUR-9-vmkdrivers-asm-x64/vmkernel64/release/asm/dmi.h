#ifndef _ASM_DMI_H
#define _ASM_DMI_H 1

#include <asm/io.h>

#if !defined(__VMKLNX__)
extern void *dmi_ioremap(unsigned long addr, unsigned long size);
extern void dmi_iounmap(void *addr, unsigned long size);
#endif

#define DMI_MAX_DATA 2048

extern int dmi_alloc_index;
extern char dmi_alloc_data[DMI_MAX_DATA];

/* This is so early that there is no good way to allocate dynamic memory. 
   Allocate data in an BSS array. */
static inline void *dmi_alloc(unsigned len)
{
	int idx = dmi_alloc_index;
	if ((dmi_alloc_index += len) > DMI_MAX_DATA)
		return NULL;
	return dmi_alloc_data + idx;
}

#if defined(__VMKLNX__)
#define dmi_ioremap(addr, size) ioremap(addr, size)
#define dmi_iounmap(addr, size) iounmap(addr)
#else
#define dmi_ioremap early_ioremap
#define dmi_iounmap early_iounmap
#endif /* #if defined(__VMKLNX__) */

#endif
