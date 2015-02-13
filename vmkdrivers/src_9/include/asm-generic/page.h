#ifndef _ASM_GENERIC_PAGE_H
#define _ASM_GENERIC_PAGE_H

#ifdef __KERNEL__
#ifndef __ASSEMBLY__

#include <linux/compiler.h>

/* Pure 2^n version of get_order */
/**                                          
 *  get_order - calculate the order for a buffer of a given size.
 *  @size: buffer size in bytes
 *                                           
 *  Calculate the base-2 logarithm of the number of pages for a buffer 
 *  of a given size, rounded up to the next largest integer.  This 
 *  logarithmic value is referred to as the order for the buffer and
 *  is typically used in page-allocation APIs.
 *
 *  SEE ALSO:
 *  alloc_pages, free_pages
 *                                           
 *  RETURN VALUE:
 *  Log-base-2 order value
 */                                          
/* _VMKLNX_CODECHECK_: get_order */
static __inline__ __attribute_const__ int get_order(unsigned long size)
{
	int order;

	size = (size - 1) >> (PAGE_SHIFT - 1);
	order = -1;
	do {
		size >>= 1;
		order++;
	} while (size);
	return order;
}

#endif	/* __ASSEMBLY__ */
#endif	/* __KERNEL__ */

#endif	/* _ASM_GENERIC_PAGE_H */
