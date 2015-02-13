/*
 * Portions Copyright 2008 VMware, Inc.
 */
#ifndef _X86_64_PAGE_H
#define _X86_64_PAGE_H

#ifdef __KERNEL__
#include <asm/const.h>

/* PAGE_SHIFT determines the page size */
#if defined(__VMKLNX__)
#include "vmkapi.h"
#define PAGE_SHIFT	(VMK_PAGE_SHIFT)
#else /* !defined(__VMKLNX__) */
#define PAGE_SHIFT	12
#endif /* defined(__VMKLNX__) */
#define PAGE_SIZE	(_AC(1,UL) << PAGE_SHIFT)
#define PAGE_MASK	(~(PAGE_SIZE-1))
#define PHYSICAL_PAGE_MASK	(~(PAGE_SIZE-1) & __PHYSICAL_MASK)

#define THREAD_ORDER 1 
#define THREAD_SIZE  (PAGE_SIZE << THREAD_ORDER)
#define CURRENT_MASK (~(THREAD_SIZE-1))

#define EXCEPTION_STACK_ORDER 0
#define EXCEPTION_STKSZ (PAGE_SIZE << EXCEPTION_STACK_ORDER)

#define DEBUG_STACK_ORDER (EXCEPTION_STACK_ORDER + 1)
#define DEBUG_STKSZ (PAGE_SIZE << DEBUG_STACK_ORDER)

#define IRQSTACK_ORDER 2
#define IRQSTACKSIZE (PAGE_SIZE << IRQSTACK_ORDER)

#define STACKFAULT_STACK 1
#define DOUBLEFAULT_STACK 2
#define NMI_STACK 3
#define DEBUG_STACK 4
#define MCE_STACK 5
#define N_EXCEPTION_STACKS 5  /* hw limit: 7 */

#define LARGE_PAGE_MASK (~(LARGE_PAGE_SIZE-1))
#define LARGE_PAGE_SIZE (_AC(1,UL) << PMD_SHIFT)

#define HPAGE_SHIFT PMD_SHIFT
#define HPAGE_SIZE	(_AC(1,UL) << HPAGE_SHIFT)
#define HPAGE_MASK	(~(HPAGE_SIZE - 1))
#define HUGETLB_PAGE_ORDER	(HPAGE_SHIFT - PAGE_SHIFT)

#ifndef __ASSEMBLY__

extern unsigned long end_pfn;

void clear_page(void *);
void copy_page(void *, void *);

#define clear_user_page(page, vaddr, pg)	clear_page(page)
#define copy_user_page(to, from, vaddr, pg)	copy_page(to, from)

#define alloc_zeroed_user_highpage(vma, vaddr) alloc_page_vma(GFP_HIGHUSER | __GFP_ZERO, vma, vaddr)
#define __HAVE_ARCH_ALLOC_ZEROED_USER_HIGHPAGE
/*
 * These are used to make use of C type-checking..
 */
typedef struct { unsigned long pte; } pte_t;
typedef struct { unsigned long pmd; } pmd_t;
typedef struct { unsigned long pud; } pud_t;
typedef struct { unsigned long pgd; } pgd_t;
#define PTE_MASK	PHYSICAL_PAGE_MASK

typedef struct { unsigned long pgprot; } pgprot_t;

extern unsigned long phys_base;

#define pte_val(x)	((x).pte)
#define pmd_val(x)	((x).pmd)
#define pud_val(x)	((x).pud)
#define pgd_val(x)	((x).pgd)
#define pgprot_val(x)	((x).pgprot)

#define __pte(x) ((pte_t) { (x) } )
#define __pmd(x) ((pmd_t) { (x) } )
#define __pud(x) ((pud_t) { (x) } )
#define __pgd(x) ((pgd_t) { (x) } )
#define __pgprot(x)	((pgprot_t) { (x) } )

#endif /* !__ASSEMBLY__ */

#define __START_KERNEL_map	_AC(0xffffffff80000000,UL)
#define __PAGE_OFFSET           _AC(0xffff810000000000,UL)

#define PAGE_ALIGN(addr)	(((addr)+PAGE_SIZE-1)&PAGE_MASK)

/* See Documentation/x86_64/mm.txt for a description of the memory map. */
#define __PHYSICAL_MASK_SHIFT	46
#define __PHYSICAL_MASK		((_AC(1,UL) << __PHYSICAL_MASK_SHIFT) - 1)
#define __VIRTUAL_MASK_SHIFT	48
#define __VIRTUAL_MASK		((_AC(1,UL) << __VIRTUAL_MASK_SHIFT) - 1)

#define KERNEL_TEXT_SIZE  (_AC(40,UL)*1024*1024)
#define KERNEL_TEXT_START _AC(0xffffffff80000000,UL)

#ifndef __ASSEMBLY__

#include <asm/bug.h>

#endif /* __ASSEMBLY__ */

#define PAGE_OFFSET		__PAGE_OFFSET

/* Note: __pa(&symbol_visible_to_c) should be always replaced with __pa_symbol.
   Otherwise you risk miscompilation. */
#if defined(__VMKLNX__)
/**                                          
 *  __pa - virtual address to machine address
 *  @vaddr: virtual address to convert
 *                                           
 *  ESX Deviation Notes:                     
 *  None.
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: __pa */
#define __pa(vaddr)                                                           \
({                                                                            \
        vmk_MA maddr;                                                         \
        vmk_VA2MA((vmk_VA)(vaddr), 1, &maddr);                                \
        (unsigned long)maddr;                                                 \
})

/**                                          
 *  __va - machine address to virtual address
 *  @maddr: machine address to convert
 *                                           
 *  ESX Deviation Notes:                     
 *  None.
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: __va */
#define __va(maddr)                                                   \
({                                                                    \
        vmk_VA vaddr;                                                 \
        vmk_MA2VA((vmk_MA)(maddr), 1, &vaddr);                        \
        (void *)vaddr;                                                \
})

/**                                          
 *  page_to_virt - page handle to virtual address
 *  @page: page handle
 *                                           
 *  ESX Deviation Notes:                     
 *  None.
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: page_to_virt */
#define page_to_virt(page)	(__va(page_to_pfn(page) << PAGE_SHIFT))

/**                                          
 *  virt_to_page - virtual address to page handle
 *  @vaddr: virtual address
 *                                           
 *  ESX Deviation Notes:                     
 *  The resulting page handle cannot be derefenced. The returned value
 *  doesn't correspond to an address of page structure but to the actual page
 *  number. This page handle needs to be handled through the page api only.
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: virt_to_page */
#define virt_to_page(vaddr)	(pfn_to_page(__pa(vaddr) >> PAGE_SHIFT))
#else /* !defined(__VMKLNX__) */
#define __pa(x)			((unsigned long)(x) - PAGE_OFFSET)
/* __pa_symbol should be used for C visible symbols.
   This seems to be the official gcc blessed way to do such arithmetic. */ 
#define __pa_symbol(x)		\
	({unsigned long v;  \
	  asm("" : "=r" (v) : "0" (x)); \
	  ((v - __START_KERNEL_map) + phys_base); })
#define __va(x)			((void *)((unsigned long)(x)+PAGE_OFFSET))
#ifdef CONFIG_FLATMEM
#define pfn_valid(pfn)		((pfn) < end_pfn)
#endif

#define virt_to_page(kaddr)	pfn_to_page(__pa(kaddr) >> PAGE_SHIFT)
#define virt_addr_valid(kaddr)	pfn_valid(__pa(kaddr) >> PAGE_SHIFT)
#define pfn_to_kaddr(pfn)      __va((pfn) << PAGE_SHIFT)

#define VM_DATA_DEFAULT_FLAGS \
	(((current->personality & READ_IMPLIES_EXEC) ? VM_EXEC : 0 ) | \
	 VM_READ | VM_WRITE | VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC)

#define __HAVE_ARCH_GATE_AREA 1	
#endif /* defined(__VMKLNX__) */

#include <asm-generic/memory_model.h>
#include <asm-generic/page.h>

#if !defined(__VMKLNX__)
#ifndef __ASSEMBLY__
extern int devmem_is_allowed(unsigned long pagenr);
#endif
#endif /* !defined(__VMKLNX__) */

#endif /* __KERNEL__ */

#endif /* _X86_64_PAGE_H */
