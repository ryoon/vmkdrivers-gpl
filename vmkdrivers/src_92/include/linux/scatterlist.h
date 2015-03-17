/*
 * Portions Copyright 2008 - 2010 VMware, Inc.
 */
#ifndef _LINUX_SCATTERLIST_H
#define _LINUX_SCATTERLIST_H

/******************************************************************
 * scatterlist.h
 *
 * From linux-2.6.27.10/lib/scatterlist.c:
 *
 * Copyright (C) 2007 Jens Axboe <jens.axboe@oracle.com>
 *
 ******************************************************************/

#include <asm/scatterlist.h>
#include <linux/mm.h>
#include <linux/string.h>

#if !defined(__VMKLNX__)
static inline void sg_set_buf(struct scatterlist *sg, void *buf,
			      unsigned int buflen)
{
	sg->page = virt_to_page(buf);
	sg->offset = offset_in_page(buf);
	sg->length = buflen;
}

static inline void sg_init_one(struct scatterlist *sg, void *buf,
			       unsigned int buflen)
{
	memset(sg, 0, sizeof(*sg));
	sg_set_buf(sg, buf, buflen);
}

static inline void sg_set_page(struct scatterlist *sg, struct page *page,
                               unsigned int len, unsigned int offset)
{
   sg->page   = page;
   sg->offset = offset;
   sg->length = len;
}
#endif /* !defined(__VMKLNX__) */

/**
 *  for_each_sg - Loop over each sg element
 *  @sglist: SG list
 *  @sg: Current sg element
 *  @nr: Number of elements in the list
 *  @__i: Iterator index
 *
 *  Loop over each sg element in the list.
 *
 *  ESX Deviation Notes:
 *  After iterating over @sglist, do reset using sg_reset() to start from
 *  the first sg element in the list. Otherwise, @sglist will be pointing
 *  to the last sg element the next time @sglist is used.
 *
 *  RETURN VALUE:
 *  None
 *
 */
/* _VMKLNX_CODECHECK_: for_each_sg */
#define for_each_sg(sglist, sg, nr, __i)	\
	for (__i = 0, sg = (sglist); __i < (nr); __i++, sg = sg_next(sg))

#if defined(__VMKLNX__)
#include <asm/io.h>
/**
 *  sg_set_page - Set a scatterlist entry to a given page
 *  @sg: The scatterlist entry to be set
 *  @page: The page to be set in the scatterlist entry
 *  @len:  The length of the data for the scatterlist entry
 *  @offset:  The offset into the given page where the data begins
 *
 *  sg_set_page takes the start page structure intended to be used in a
 *  scatterlist entry and marks the scatterlist entry to use that page, starting
 *  at the given offset in the page and having an overall length of 'len'.
 *
 *  ESX Deviation Notes:
 *  Linux overloads the low order bits of the page pointer with sg information.
 *  ESX does not.  This interface is provided for compatibility with Linux.
 *
 */
/* _VMKLNX_CODECHECK_: sg_set_page */
static inline void sg_set_page(struct scatterlist *sg, struct page *page,
                               unsigned int len, unsigned int offset)
{
	BUG_ON(sg->cursgel == NULL);
        BUG_ON(sg->vmksgel == NULL);

	sg->cursgel->addr = page_to_phys(page) + offset;
	sg->cursgel->length = len;
}

/**
 *  sg_page - Return page for the given scatterlist entry
 *  @sg: The scatterlist entry
 *
 *  Retuns the page pointer for the given scatterlist element
 *
 *  ESX Deviation Notes:
 *  The resulting pointer to the page descriptor should not be referenced nor
 *  used in any form of pointer arithmetic to obtain the page descriptor to
 *  any adjacent page. The pointer should be treated as an opaque handle and
 *  should only be used as argument to other functions.
 *
 *  RETURN VALUE:
 *  pointer to struct page
 *
 */
/* _VMKLNX_CODECHECK_: sg_page */
static inline struct page *sg_page(struct scatterlist *sg)
{
	return phys_to_page(sg->cursgel->addr);
}

static inline void sg_set_buf(struct scatterlist *sg, void *buf,
                              unsigned int buflen)
{
#if defined(VMX86_DEBUG)
	if (!vmklnx_is_physcontig(buf, buflen)) {
		vmk_Panic("sg_set_buf: discontiguous buffer %p, len %d\n", buf, buflen);
	}
#endif /* defined(VMX86_DEBUG) */

	sg_set_page(sg, virt_to_page(buf), buflen, offset_in_page(buf));
}

/**
 *  sg_init_one - Initialize a single entry sg list
 *  @sg: SG entry
 *  @buf: virtual address for IO
 *  @buflen: IO length
 *
 *  Initializes a single entry sg list
 *
 *  RETURN VALUE:
 *  NONE
 *
 */
/* _VMKLNX_CODECHECK_: sg_init_one */
static inline void sg_init_one(struct scatterlist *sg, void *buf,
                               unsigned int buflen)
{
        sg_set_buf(sg, buf, buflen);
}

/**
 *  sg_virt - Return virtual address of an sg entry
 *  @sg: SG entry
 *
 *  Gives virtual address of an sg entry.
 *
 *  RETURN VALUE:
 *  Returns the virtual address of an sg entry
 *  
 */
/* _VMKLNX_CODECHECK_: sg_virt */
static inline void *sg_virt(struct scatterlist *sg)
{
	return __va(sg->cursgel->addr);
}

/**
 *  sg_memset - Fill SG list elements with constant byte
 *  @sgl: SG list
 *  @nents: number of SG entries
 *  @c: byte to set
 *  @count: number of bytes to set
 *
 *  Fills the first @count bytes of an SG list @sgl elements with
 *  the constant byte @c.
 *
 *  RETURN VALUE:
 *  None
 *
 */
static inline void sg_memset(struct scatterlist *sgl, unsigned int nents,
			     int c, size_t count)
{
	struct scatterlist *sg = sgl;
	unsigned int len;
	void *addr;

	/* do reset to start from first sg element - for SG_VMK type */
	sg_reset(sg);

	for(; nents && count > 0; nents--, count -= len) {
		addr = sg_virt(sg);
		len = min_t(size_t, sg_dma_len(sg), count);
		memset(addr, c, len);
		sg = sg_next(sg);
	}

	/* do reset again - again for SG_VMK type */
	sg_reset(sg);
}

/**
 *  sg_copy_buffer - Copy data between a linear buffer and an SG list
 *  @sgl: SG list
 *  @nents: number of SG entries
 *  @buf: buffer for copying
 *  @buflen: number of bytes to copy
 *  @to_buffer: transfer direction
 *
 *  Copies data between a linear buffer @buf and an SG list @sgl. 
 *  @to_buffer is used to decide source and destination for copying.
 *
 *  RETURN VALUE:
 *  Returns the number of copied bytes
 *
 */
/* _VMKLNX_CODECHECK_: sg_copy_buffer */
static inline size_t sg_copy_buffer(struct scatterlist *sgl, unsigned int nents,
				    void *buf, size_t buflen, int to_buffer)
{
	struct scatterlist *sg = sgl;
	unsigned int len, offset;
	void *addr;

	/* do reset to start from first sg element - for SG_VMK type */
	sg_reset(sg);

	for(offset = 0; nents && (offset < buflen); nents--, offset += len) {
		addr = sg_virt(sg);
		len = min_t(size_t, sg_dma_len(sg), buflen - offset);

		if (to_buffer) {
			memcpy(buf + offset, addr, len);
		} else {
			memcpy(addr, buf + offset, len);
		}
		sg = sg_next(sg);
	}

	/* do reset again - again for SG_VMK type */
	sg_reset(sg);
	return offset;
}

/**
 *  sg_copy_from_buffer - Copy data from a linear buffer to an SG list
 *  @sgl: SG list
 *  @nents: number of SG entries
 *  @buf: buffer to copy from
 *  @buflen: number of bytes to copy
 *
 *  Copies data from a linear buffer @buf to an SG list @sgl.
 *
 *  RETURN VALUE:
 *  Returns the number of copied bytes.
 *
 */
/* _VMKLNX_CODECHECK_: sg_copy_from_buffer */
static inline size_t sg_copy_from_buffer(struct scatterlist *sgl, unsigned int nents,
					 void *buf, size_t buflen)
{
	return sg_copy_buffer(sgl, nents, buf, buflen, 0);
}

/**
 *  sg_copy_to_buffer - Copy data from an SG list to a linear buffer
 *  @sgl: SG list
 *  @nents: number of SG entries
 *  @buf: buffer to copy from
 *  @buflen: number of bytes to copy
 *
 *  Copies data from an SG list @sgl to a linear buffer @buf.
 *
 *  ESX Deviation Notes:
 *  Handles both SG_LINUX and SG_VMK types of SG list.
 *  For SG_LINUX type, caller should make sure that SG list dma mapping
 *  is done if needed.
 *
 *  RETURN VALUE:
 *  Returns the number of copied bytes.
 *
 */
/* _VMKLNX_CODECHECK_: sg_copy_to_buffer */
static inline size_t sg_copy_to_buffer(struct scatterlist *sgl, unsigned int nents,
				       void *buf, size_t buflen)
{
	return sg_copy_buffer(sgl, nents, buf, buflen, 1);
}
#endif /* defined(__VMKLNX__) */

#endif /* _LINUX_SCATTERLIST_H */
