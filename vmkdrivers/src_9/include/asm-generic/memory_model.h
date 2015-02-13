/*
 * Portions Copyright 2008 VMware, Inc.
 */
#ifndef __ASM_MEMORY_MODEL_H
#define __ASM_MEMORY_MODEL_H

#ifdef __KERNEL__
#ifndef __ASSEMBLY__

#if defined(__VMKLNX__)

/**
 *  __pfn_to_page - page number to page handle
 *  @pfn: page number
 *
 *  ESX Deviation Notes:
 *  The resulting page handle cannot be derefenced. The returned value
 *  doesn't correspond to an address of page structure but to the actual page
 *  number. This page handle needs to be handled through the page api only.
 *
 */
/* _VMKLNX_CODECHECK_: __pfn_to_page */
#define __pfn_to_page(pfn)	((struct page *)(pfn))

/**
 *  __page_to_pfn - page handle to page number
 *  @page: page handle
 *
 *  ESX Deviation Notes:
 *  None.
 *
 */
/* _VMKLNX_CODECHECK_: __page_to_pfn */
#define __page_to_pfn(page)	((unsigned long)(page))

#define page_to_pfn __page_to_pfn
#define pfn_to_page __pfn_to_page

#else /* !defined(__VMKLNX__) */

#if defined(CONFIG_FLATMEM)

#ifndef ARCH_PFN_OFFSET
#define ARCH_PFN_OFFSET		(0UL)
#endif

#elif defined(CONFIG_DISCONTIGMEM)

#ifndef arch_pfn_to_nid
#define arch_pfn_to_nid(pfn)	pfn_to_nid(pfn)
#endif

#ifndef arch_local_page_offset
#define arch_local_page_offset(pfn, nid)	\
	((pfn) - NODE_DATA(nid)->node_start_pfn)
#endif

#endif /* CONFIG_DISCONTIGMEM */

/*
 * supports 3 memory models.
 */
#if defined(CONFIG_FLATMEM)

#define __pfn_to_page(pfn)	(mem_map + ((pfn) - ARCH_PFN_OFFSET))
#define __page_to_pfn(page)	((unsigned long)((page) - mem_map) + \
				 ARCH_PFN_OFFSET)
#elif defined(CONFIG_DISCONTIGMEM)

#define __pfn_to_page(pfn)			\
({	unsigned long __pfn = (pfn);		\
	unsigned long __nid = arch_pfn_to_nid(pfn);  \
	NODE_DATA(__nid)->node_mem_map + arch_local_page_offset(__pfn, __nid);\
})

#define __page_to_pfn(pg)						\
({	struct page *__pg = (pg);					\
	struct pglist_data *__pgdat = NODE_DATA(page_to_nid(__pg));	\
	(unsigned long)(__pg - __pgdat->node_mem_map) +			\
	 __pgdat->node_start_pfn;					\
})

#elif defined(CONFIG_SPARSEMEM)
/*
 * Note: section's mem_map is encorded to reflect its start_pfn.
 * section[i].section_mem_map == mem_map's address - start_pfn;
 */
#define __page_to_pfn(pg)					\
({	struct page *__pg = (pg);				\
	int __sec = page_to_section(__pg);			\
	__pg - __section_mem_map_addr(__nr_to_section(__sec));	\
})

#define __pfn_to_page(pfn)				\
({	unsigned long __pfn = (pfn);			\
	struct mem_section *__sec = __pfn_to_section(__pfn);	\
	__section_mem_map_addr(__sec) + __pfn;		\
})
#endif /* CONFIG_FLATMEM/DISCONTIGMEM/SPARSEMEM */

#ifdef CONFIG_OUT_OF_LINE_PFN_TO_PAGE
struct page;
/* this is useful when inlined pfn_to_page is too big */
extern struct page *pfn_to_page(unsigned long pfn);
extern unsigned long page_to_pfn(struct page *page);
#else
#define page_to_pfn __page_to_pfn
#define pfn_to_page __pfn_to_page
#endif /* CONFIG_OUT_OF_LINE_PFN_TO_PAGE */
#endif /* defined(__VMKLNX__) */


#endif /* __ASSEMBLY__ */
#endif /* __KERNEL__ */

#endif
