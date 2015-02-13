/*
 * include/asm-x86_64/crashdump.h
 *
 * Copyright (C) Hitachi, Ltd. 2004
 * Written by Satoshi Oshima (oshima@sdl.hitachi.co.jp)
 *
 * Derived from include/asm-i386/diskdump.h
 * Copyright (c) 2004 FUJITSU LIMITED
 * Copyright (c) 2003 Red Hat, Inc. All rights reserved.
 *
 */

#ifndef _ASM_X86_64_CRASHDUMP_H
#define _ASM_X86_64_CRASHDUMP_H

#ifdef __KERNEL__

#include <linux/elf.h>

extern int page_is_ram(unsigned long);
extern unsigned long next_ram_page(unsigned long);

#define platform_fix_regs() \
{                                                                      \
       unsigned long rsp;                                              \
       unsigned short ss;                                              \
       rsp = (unsigned long) ((char *)regs + sizeof (struct pt_regs)); \
       ss = __KERNEL_DS;                                               \
       if (regs->cs & 3) {                                             \
               rsp = regs->rsp;                                        \
               ss = regs->ss & 0xffff;                                 \
       }                                                               \
       myregs = *regs;                                                 \
       myregs.rsp = rsp;                                               \
       myregs.ss = (myregs.ss & (~0xffff)) | ss;                       \
}

#define platform_timestamp(x) rdtscll(x)

#define platform_freeze_cpu()					\
{								\
	for (;;) local_irq_disable();				\
}

static inline void platform_init_stack(void **stackptr)
{
	struct page *page;

	if ((page = alloc_page(GFP_KERNEL)))
		*stackptr = (void *)page_address(page);

	if (*stackptr)
		memset(*stackptr, 0, PAGE_SIZE);
	else
		printk(KERN_WARNING
		    "crashdump: unable to allocate separate stack\n");
}

#define platform_cleanup_stack(stackptr)               \
do {                                                   \
	if (stackptr)                                  \
		free_page((unsigned long)stackptr);    \
} while (0)

typedef asmlinkage void (*crashdump_func_t)(struct pt_regs *, void *);

static inline void platform_start_crashdump(void *stackptr,
					    crashdump_func_t dumpfunc,
					    struct pt_regs *regs)
{								
	static unsigned long old_rsp;
	unsigned long new_rsp;

	if (stackptr) {
		asm volatile("movq %%rsp,%0" : "=r" (old_rsp));
		new_rsp = (unsigned long)stackptr + PAGE_SIZE;
		asm volatile("movq %0,%%rsp" :: "r" (new_rsp));
		dumpfunc(regs, NULL);
		asm volatile("movq %0,%%rsp" :: "r" (old_rsp));
	} else
		dumpfunc(regs, NULL);
}

#endif /* __KERNEL__ */

#endif /* _ASM_X86_64_CRASHDUMP_H */
