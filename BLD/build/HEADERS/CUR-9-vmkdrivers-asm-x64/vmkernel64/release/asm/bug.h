#ifndef __ASM_X8664_BUG_H
#define __ASM_X8664_BUG_H 1

#if defined(__VMKLNX__)

/*
 * For vmklinux, the trick below doesn't work.  It works by raising a
 * ud2 exception, and then the exception handler knows the rip is
 * pointing to a struct bug_frame.  However, pushq can only have a
 * 32-bit immediate, so we can't push the 64-address of __FILE__ into
 * it (this works on linux, because linux is loaded at -2Gb, and when
 * 'signed int filename' is cast to long it is sign-extended, the top
 * bits filled to 0xffff... and the address is correct.  Besides this,
 * the vmkernel ud2 exception handler doesn't know anything about
 * this.  So the long and the short of it is - we don't do any of this
 * arch specific BUG() stuff, and just fall back to the generic
 * panic()
 */

#include <asm-generic/bug.h>

#else /* !defined(__VMKLNX__) */

#include <linux/stringify.h>

/*
 * Tell the user there is some problem.  The exception handler decodes 
 * this frame.
 */
struct bug_frame {
	unsigned char ud2[2];
	unsigned char push;
	signed int filename;
	unsigned char ret;
	unsigned short line;
} __attribute__((packed));

#ifdef CONFIG_BUG
#define HAVE_ARCH_BUG
/* We turn the bug frame into valid instructions to not confuse
   the disassembler. Thanks to Jan Beulich & Suresh Siddha
   for nice instruction selection.
   The magic numbers generate mov $64bitimm,%eax ; ret $offset. */

#define BUG() 								\
	asm volatile(							\
	"ud2 ; pushq $%c1 ; ret $%c0" :: 				\
		     "i"(__LINE__), "i" (__FILE__))
void out_of_line_bug(void);
#else
static inline void out_of_line_bug(void) { }
#endif

#include <asm-generic/bug.h>
#endif

#endif /* defined(__VMKLNX __) */
