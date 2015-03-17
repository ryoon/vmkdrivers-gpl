/*
 * include/asm-x86_64/diskdump.h
 *
 * Copyright (C) Hitachi, Ltd. 2004
 * Written by Satoshi Oshima (oshima@sdl.hitachi.co.jp)
 *
 * Derived from include/asm-i386/diskdump.h
 * Copyright (c) 2004 FUJITSU LIMITED
 * Copyright (c) 2003 Red Hat, Inc. All rights reserved.
 *
 */

#ifndef _ASM_X86_64_DISKDUMP_H
#define _ASM_X86_64_DISKDUMP_H

#ifdef __KERNEL__

#include <linux/elf.h>
#include <asm/crashdump.h>

const static int platform_supports_diskdump = 1;

struct disk_dump_sub_header {
	elf_gregset_t		elf_regs;
};

#define size_of_sub_header()	((sizeof(struct disk_dump_sub_header) + PAGE_SIZE - 1) / DUMP_BLOCK_SIZE)

#define write_sub_header() 						\
({									\
 	int ret;							\
									\
	ELF_CORE_COPY_REGS(dump_sub_header.elf_regs, (&myregs));	\
	clear_page(scratch);						\
	memcpy(scratch, &dump_sub_header, sizeof(dump_sub_header));	\
 									\
	if ((ret = write_blocks(dump_part, 2, scratch, 1)) >= 0)	\
		ret = 1; /* size of sub header in page */;		\
	ret;								\
})

#endif /* __KERNEL__ */

#endif /* _ASM_X86_64_DISKDUMP_H */
