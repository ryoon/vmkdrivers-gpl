/*
 * Portions Copyright 1998, 2007 - 2010 VMware, Inc.
 */

#ifndef _VMKLINUX_BLOCK_H
#define _VMKLINUX_BLOCK_H

/* similar to linux bio_set */
struct vmklnx_bio_set {
   kmem_cache_t *bio_slab;
   unsigned int front_pad;
   mempool_t *bio_pool;
};

/*
 * Block device functions.
 */

#ifdef BLOCK_DRIVER

struct block_device_operations;

extern void vmklnx_block_init_start(void);
extern void vmklnx_block_init_done(int);
extern int vmklnx_register_blkdev(vmk_uint32 major, const char *name,
                                  int domain, int bus, int devfn, void *data);
extern void vmklnx_block_register_sglimit(vmk_uint32 major, int sgSize);
extern void vmklnx_block_register_disk_maxXfer(vmk_uint32 major,
                                               int targetNum,
                                               vmk_uint64 maxXfer);
extern void vmklnx_block_register_ssd(vmk_uint32 major,
                                      int targetNum);
extern void vmklnx_block_register_poll_handler(vmk_uint32 major,
                                               unsigned int irq,
                                               irq_handler_t handler,
                                               void *dev_id);

/**
 *  vmklnx_blk_rq_map_sg - map a request to scatterlist
 *  @rq: pointer to request struct
 *  @sg: pointer to scatterlist struct pointer
 *
 *  Sets up scatterlist @sg to point to already mapped sg
 *  list embbeded in request @rq->bio and returns number
 *  of sg entries present.
 *
 *  ESX Deviation Notes:
 *  Modified version of blk_rq_map_sg function - no need to pre-allocate
 *  sg list prior to calling this function, instead pass pointer to hold
 *  scatterlist struct pointer.
 *  Only vmkernel SG lists are supported.
 *
 *  RETURN VALUE:
 *  Number of sg entries, **sg pointer holds pointer to sg list
 *
 */
/* _VMKLNX_CODECHECK_: vmklnx_blk_rq_map_sg */

/*
 * we use vmk sg list which is already mapped.
 * And, in our scheme: only one bio for each request
 * so just return bio->bi_max_vecs
 */
static inline int vmklnx_blk_rq_map_sg(struct request *rq,
                                       struct scatterlist **sg)
{
   VMK_ASSERT(rq);
   VMK_ASSERT(rq->bio);

   *sg = rq->bio->vmksg;
   return rq->bio->bi_max_vecs;
}

#endif // BLOCK_DRIVER

#endif /* _VMKLINUX_BLOCK_H */
