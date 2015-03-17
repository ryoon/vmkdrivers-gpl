/*
 * Portions Copyright 2008 - 2011 VMware, Inc.
 */
#ifndef _X8664_SCATTERLIST_H
#define _X8664_SCATTERLIST_H

#if defined(__VMKLNX__)
#include "vmkapi.h"
#include "vmklinux_scatterlist.h"
#else /* !defined(__VMKLNX__) */
struct scatterlist {
    struct page		*page;
    unsigned int	offset;
    unsigned int	length;
    dma_addr_t		dma_address;
    unsigned int        dma_length;
};
#endif /* defined(__VMKLNX__) */

#define ISA_DMA_THRESHOLD (0x00ffffff)

/* These macros should be used after a pci_map_sg call has been done
 * to get bus addresses of each of the SG entries and their lengths.
 * You should only work with the number of sg entries pci_map_sg
 * returns.
 */
#if defined(__VMKLNX__)

/**
 *  sg_next - Point to next sg element in the list
 *  @sg: Pointer to the scatter gather list
 *
 *  This function updates the internal pointers in the scatter gather
 *  list that is passed in to point to the next element in the list.
 *
 *  RETURN VALUE:
 *  Same pointer that was passed in
 *
 *  SEE ALSO:
 *  sg_reset
 *
 *  ESX Deviation Notes:
 *  ESX returns back the same pointer that was passed into the function
 *  everytime. Also, ESX does not return NULL when the end of the list 
 *  is hit.
 *
 */
/* _VMKLNX_CODECHECK_: sg_next */
static inline struct scatterlist * 
sg_next(struct scatterlist *sg)
{
	sg->cursgel++;
        sg->curIOsgel++;
	return sg;
}

/**
 *  nth_sg - get address of the n'th element in sg array 
 *  @sg: scatterlist
 *  @n: the index of sg element to look for
 *
 *  Get the address of specified sg element from the scatterlist 
 *  provided as input
 *
 *  RETURN VALUE:
 *  Pointer to the specified sg entry in the list
 */
/* _VMKLNX_CODECHECK_: nth_sg */
static inline struct scatterlist *
nth_sg(struct scatterlist *sg, unsigned int n)
{
	sg->cursgel += n;
        sg->curIOsgel += n;

	return sg;
}

/**
 *  sg_reset - Point back to the beginning of the list
 *  @sg: Pointer to a scatter gather list
 *
 *  This function resets some pointers inside the scatter gather list to
 *  point back to the beginning of the list.
 *  This function has to be called after a scatter gather list traversal
 *  in order to ensure proper results in subsequent traversals of that list.
 *  For example, after using the macro for_each_sg.
 *
 *  ESX Deviation Notes:
 *  This function is unique to ESXi. Not found in Linux.
 *
 */
/* _VMKLNX_CODECHECK_: sg_reset */
static inline void
sg_reset(struct scatterlist *sg)
{
	sg->cursgel = sg->vmksgel;
        sg->curIOsgel = sg->vmkIOsgel;
}

/**
 *  sg_dma_address - Return the dma_address element of the scatterlist
 *  @sg: the scatterlist
 *
 *  Return the dma_address element of the scatterlist.
 *
 *  SYNOPSIS:
 *   # define sg_dma_address(sg)
 *
 *  RETURN VALUE:
 *  dma_address of the scatterlist
 *
 */
 /* _VMKLNX_CODECHECK_: sg_dma_address */
static inline dma_addr_t
sg_dma_address(struct scatterlist *sg)
{
   VMK_ASSERT(sg->vmkIOsgel != NULL);
   VMK_ASSERT(sg->curIOsgel != NULL);
   return ((sg)->curIOsgel->ioAddr);
}

/**
 *  sg_dma_len - Return the dma_len element of the scatterlist
 *  @sg: the scatterlist
 *
 *  Return the dma_len element of the scatterlist.
 *
 *  SYNOPSIS:
 *   # define sg_dma_len(sg)
 *
 *  RETURN VALUE:
 *  dma_length of the scatterlist
 *
 */
 /* _VMKLNX_CODECHECK_: sg_dma_len */
static inline unsigned int
sg_dma_len(struct scatterlist *sg)
{
   if (sg->vmkIOsgel != NULL) {
      return ((sg)->curIOsgel->length);
   } else {
      return ((sg)->cursgel->length);
   }
}	

#else /* !defined(__VMKLNX__) */
#define sg_dma_address(sg)     ((sg)->dma_address)
#define sg_dma_len(sg)         ((sg)->dma_length)
#endif /* defined(__VMKLNX__) */

#endif 
