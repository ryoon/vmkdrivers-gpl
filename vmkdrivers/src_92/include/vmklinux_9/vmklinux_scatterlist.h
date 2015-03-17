/*
 * Portions Copyright 2008 - 2010 VMware, Inc.
 */
#ifndef _VMKLINUX_SCATTERLIST_H
#define _VMKLINUX_SCATTERLIST_H

#include <linux/mm.h>

typedef vmk_SgElem vmk_sgelem;
typedef vmk_SgArray vmk_sgarray;

struct scatterlist {
   vmk_sgelem *vmksgel; /* Always points to the first vmk sgel */
   vmk_sgelem *cursgel; /* Points to the currently used vmk sgel */

   vmk_sgelem *vmkIOsgel; /* Always points to the first IO vmk sgel */
   vmk_sgelem *curIOsgel; /* Points to the currently used IO sgel */

   int premapped; /* Is the IO sg array pre-mapped by vmkernel? */

   /* These are used for non-premapped arrays during mappings */
   vmk_sgarray *vmksga;     /* The machine sg array */
   vmk_sgarray *vmkIOsga;   /* The IO sg array */      
};

/*
 * These functions and macros are useful when building local scatterlist
 * and should be called before calling sg_init_one().
 *
 * If there is a premapped SG array with IO addresses, then set IOsga with
 * VMK_INIT_VMK_SG_WITH_ARRAYS otherwise, use VMKLNX_INIT_VMK_SG and the
 * array must be mapped with dma_map_sg or pci_map_sg to be passed to the
 * a device for DMA.
 */

/**
 * VMKLNX_INIT_VMK_SG - Initialize a scatterlist with a vmk_sgelem list
 * @sg: scatterlist to initialize
 * @sgel: vmk_sgelem list
 *
 * RETURN VALUE:
 * None
 */
/* _VMKLNX_CODECHECK_: VMKLNX_INIT_VMK_SG */
static inline void
VMKLNX_INIT_VMK_SG(struct scatterlist *sg, vmk_sgelem *sgel)
{
   sg->vmksgel = sg->cursgel = sgel;
   sg->vmkIOsgel = sg->curIOsgel = NULL;
   sg->premapped = 0;
   sg->vmksga = NULL;
   sg->vmkIOsga = NULL;
}

static inline void
VMKLNX_INIT_VMK_SG_WITH_ARRAYS(struct scatterlist *sg, vmk_sgarray *sga,
                               vmk_sgarray *IOsga)
{
   sg->vmksgel = sg->cursgel = &(sga->elem[0]);
   sg->vmkIOsgel = sg->curIOsgel = &(IOsga->elem[0]);
   sg->premapped = 1;
   sg->vmksga = sga;
   sg->vmkIOsga = IOsga;
}

#define VMKLNX_DECLARE_VMK_SG(name)				\
	vmk_sgelem name = {}				        \

/**
 *  vmklnx_sg_offset - Return offset in page for the given scatterlist entry
 *  @sg: The scatterlist entry
 *
 *  Retuns the offset in page for the given scatterlist element
 *
 *  Keep in mind this is the machine address offset and not the IO address
 *  offset. There are circumstances where they may be different.
 *
 *  RETURN VALUE:
 *  offset in page
 *
 */
static inline unsigned int
vmklnx_sg_offset(struct scatterlist *sg)
{
   return offset_in_page(sg->cursgel->addr);
}

#endif /* _VMKLINUX_SCATTERLIST_H */
